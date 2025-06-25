// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "mtk-pcie.h"
#include "mtk_pcie_lib.h"

#define LINK_RETRAIN_TIMEOUT	HZ

static const char *const linkspeed[MTK_SPEED_MAX] = {
	"NULL", "2.5GT/s", "5.0GT/s", "8.0GT/s", "16GT/s",
};

struct mtk_aspm_state {
	int aspm_us;
	int aspm_ds;
};

unsigned int mtk_get_value(char *str)
{
	unsigned int index, value = 0;
	char *format;

	if (str[0] == '0' && str[1] == 'x') {
		index = 2;
		format = "%x";
	} else {
		index = 0;
		format = "%d";
	}

	if (sscanf(&str[index], format, &value) == 1)
		return value;

	return 0;
}
EXPORT_SYMBOL(mtk_get_value);

static int mtk_pcie_retrain(struct pci_dev *dev)
{
	struct pci_dev *parent;
	unsigned long start_jiffies;
	int ppos = 0, i = 0;
	u16 reg16 = 0;

	parent = dev->bus->self;
	ppos = parent->pcie_cap;

	/* Retrain link */
	pci_read_config_word(parent, ppos + PCI_EXP_LNKCTL, &reg16);
	reg16 |= PCI_EXP_LNKCTL_RL;
	pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL, reg16);
	/* Wait for link training end. Break out after waiting for timeout */
	start_jiffies = jiffies;
	usleep_range(10000, 20000);
	for (;;) {
		pci_read_config_word(parent, ppos + PCI_EXP_LNKSTA, &reg16);
		if (!(reg16 & PCI_EXP_LNKSTA_LT))
			return 0;

		if (time_after(jiffies, start_jiffies + LINK_RETRAIN_TIMEOUT)) {
			pr_info("timeout\n");
			return -ETIMEDOUT;
		}

		if (!(i % 100))
			pr_info("[%s]: try %d time\n", __func__, i);

		if (i > 1000) {
			pr_info("retrain fail!\n");
			return -ETIMEDOUT;
		}

		i++;

		usleep_range(1000, 2000);
	}
	return 0;
}

static int mtk_pcie_speed(struct pci_dev *dev, int speed)
{
	struct pci_dev *parent;
	u16 linksta = 0, plinksta = 0, plinkctl2 = 0;
	int pos, ppos;
	int ret;

	pos = dev->pcie_cap;
	parent = dev->bus->self;
	ppos = parent->pcie_cap;

	pci_read_config_word(dev, pos + PCI_EXP_LNKSTA, &linksta);
	pci_read_config_word(parent, ppos + PCI_EXP_LNKSTA, &plinksta);

	pci_read_config_word(parent, ppos + PCI_EXP_LNKCTL2, &plinkctl2);
	if ((plinkctl2 & PCI_SPEED_MASK) != speed) {
		plinkctl2 &= ~PCI_SPEED_MASK;
		plinkctl2 |= speed;
		pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL2, plinkctl2);
	}

	if (((linksta & PCI_EXP_LNKSTA_CLS) == speed) &&
				 ((plinksta & PCI_EXP_LNKSTA_CLS) == speed))
		return 0;

	ret = mtk_pcie_retrain(dev);
	if (ret) {
		pr_info("retrain fails\n");
		return ret;
	}

	pci_read_config_word(parent, ppos + PCI_EXP_LNKSTA, &plinksta);

	if ((plinksta & PCI_EXP_LNKSTA_CLS) != speed) {
		pr_info("can't train to target speed!! Link Status(parent) : 0x%x\n",
				   plinksta);
		return -EINVAL;
	}

	return 0;
}

static int mtk_pcie_change_speed(struct pci_dev *dev, int speed)
{
	int pos = 0, ori_speed = 0, ret = 0;
	u16 linksta = 0;

	if (speed >= MTK_SPEED_MAX || speed < 0) {
		pr_info("change speed %d exceeds maximum %d\n", speed, MTK_SPEED_MAX);
		return -EINVAL;
	}

	pos = dev->pcie_cap;
	pci_read_config_word(dev, pos + PCI_EXP_LNKSTA, &linksta);
	ori_speed = linksta & PCI_EXP_LNKSTA_CLS;

	ret = mtk_pcie_speed(dev, speed); /* train speed */
	if (ret) {
		pr_info("change speed to %s failed\n", linkspeed[speed]);
		return -EINVAL;
	}

	pr_info("[CLS] ori -> cur: %s->%s.\n",
		 ori_speed < MTK_SPEED_MAX ? linkspeed[ori_speed] : "unknown",
		 speed < MTK_SPEED_MAX ? linkspeed[speed] : "unknown");

	return ret;
}

static void mtk_pcie_config_aspm_dev(struct pci_dev *dev, int val)
{
	int pos = dev->pcie_cap;
	u16 reg16 = 0;

	dev_info(&dev->dev, "(config aspm) value : 0x%x\n", val);

	pci_read_config_word(dev, pos + PCI_EXP_LNKCTL, &reg16);
	reg16 &= ~0x3;
	reg16 |= val;
	pci_write_config_word(dev, pos + PCI_EXP_LNKCTL, reg16);
}

static int mtk_pcie_aspm(struct pci_dev *dev, struct mtk_aspm_state aspm)
{
	struct pci_dev *child = dev, *parent;

	if (!child) {
		pr_info("can't find target device\n");
		return -ENODEV;
	}

	parent = child->bus->self;
	if ((aspm.aspm_us == 0) && (aspm.aspm_ds == 0)) {
		mtk_pcie_config_aspm_dev(child, 0);
		mtk_pcie_config_aspm_dev(parent, 0);
	} else {
		mtk_pcie_config_aspm_dev(parent, aspm.aspm_ds);
		mtk_pcie_config_aspm_dev(child, aspm.aspm_us);
	}

	return 0;
}

static int mtk_pcie_lm_command(struct pci_dev *dev, int type, int payload, int rcv_num, int pos, int lane)
{
	int cmd = 0;
	u16 reg = 0;

	cmd = (rcv_num) | (type << PCIE_LM_TYPE_OFFSET) | (payload << PCIE_LM_PAYLOAD_OFFSET);
	pci_write_config_word(dev, pos + (PCI_EXT_LM_LANECTL + 0x4 * lane), cmd);
	/* Wait at most 10ms for status rg reflect */
	mdelay(10);
	pci_read_config_word(dev, pos + (PCI_EXT_LM_LANESTA + 0x4 * lane), &reg);

	if (((reg & PCIE_LM_TYPE_MASK) == (type << PCIE_LM_TYPE_OFFSET)) &&
	    ((reg & PCIE_LM_RECEIVE_NUM_MASK) == rcv_num))
		return reg;

	pr_info("LM command fail, status = 0x%x, type = 0x%x, rcv_num = 0x%x, payload = 0x%x, lane = %d.\n",
		reg, type, rcv_num, payload, lane);
	return -1;
}

static int mtk_pcie_lm_cmd_complete(struct pci_dev *dev, int type, int payload, int rcv_num, int pos, int lane)
{
	int lm_status = 0;

	lm_status = mtk_pcie_lm_command(dev, type, payload, rcv_num, pos, lane);
	mtk_pcie_lm_command(dev, PCIE_LM_NO_COMMAND_TYPE, PCIE_LM_NO_COMMAND_PAYLOAD, 0, pos, lane);

	return lm_status;
}

static int mtk_pcie_lm_reset(struct pci_dev *dev, int rcv_num, int lm_pos, int lane)
{
	int lm_status = 0;

	/* Clear error log */
	lm_status = mtk_pcie_lm_cmd_complete(dev, PCIE_LM_CLEAR_ERROR_LOG_TYPE,
					     PCIE_LM_CLEAR_ERROR_LOG_PAYLOAD, rcv_num, lm_pos, lane);
	if (lm_status < 0) {
		pr_info("[%s:%d], Clear error log fail\n", __func__, __LINE__);
		return lm_status;
	}

	/* Go to Normal setting */
	lm_status = mtk_pcie_lm_cmd_complete(dev, PCIE_LM_NORMAL_SETTING_TYPE,
					     PCIE_LM_NORMAL_SETTING_PAYLOAD, rcv_num, lm_pos, lane);
	if (lm_status < 0) {
		pr_info("[%s:%d], Go to Normal setting fail\n", __func__, __LINE__);
		return lm_status;
	}

	return 0;
}

static int mtk_pcie_margin_step(struct mtk_pcie_info *pcie_smt, struct pci_dev *dev, int lane,
				int rcv_num, int lm_pos, int lm_max_step, int margin_type)
{
	int lm_status = 0, lm_step = 0, lm_error = 0, lm_exc_sta = 0, ret = 0;
	int type = 0, payload = 0;

	switch (margin_type) {
	case MTK_TIME_MARGIN_LEFT:
		type = PCIE_LM_STEP_MARGIN_TIME_TYPE;
		payload = PCIE_LM_STEP_MARGIN_TIME_LEFT_PAYLOAD;
		pr_info("Time margin left!\n");
		break;
	case MTK_TIME_MARGIN_RIGHT:
		type = PCIE_LM_STEP_MARGIN_TIME_TYPE;
		payload = PCIE_LM_STEP_MARGIN_TIME_RIGHT_PAYLOAD;
		pr_info("Time margin right!\n");
		break;
	case MTK_VOLTAGE_MARGIN_UP:
		type = PCIE_LM_STEP_MARGIN_VOL_TYPE;
		payload = PCIE_LM_STEP_MARGIN_VOL_UP_PAYLOAD;
		pr_info("Voltage margin up!\n");
		break;
	case MTK_VOLTAGE_MARGIN_DOWN:
		type = PCIE_LM_STEP_MARGIN_VOL_TYPE;
		payload = PCIE_LM_STEP_MARGIN_VOL_DOWN_PAYLOAD;
		pr_info("Voltage margin down!\n");
		break;
	default:
		pr_info("Error margin type!\n");
		return -1;
	};

	for (lm_step = 0; lm_step <= lm_max_step; lm_step++) {
		lm_status = mtk_pcie_lm_cmd_complete(dev, type, payload + lm_step, rcv_num, lm_pos, lane);
		if (lm_status < 0) {
			pr_info("[%s:%d], Margin fail\n", __func__, __LINE__);
			ret = mtk_pcie_lm_reset(dev, rcv_num, lm_pos, lane);
			if (ret < 0)
				pr_info("[%s:%d], Reset fail\n", __func__, __LINE__);

			return lm_status;
		}

		lm_exc_sta = (lm_status & PCIE_LM_EXECUTION_STA_MASK) >> PCIE_LM_EXECUTION_STA_OFFSET;
		pr_info("step %d: Status 0x%x, lane: %d\n", lm_step, lm_exc_sta, lane);

		if(lm_exc_sta == 0x1) {
			mdelay(200);
			lm_status = mtk_pcie_lm_cmd_complete(dev, type, payload + lm_step, rcv_num, lm_pos, lane);
			if (lm_status < 0) {
				pr_info("[%s:%d], Margin fail\n", __func__, __LINE__);
				ret = mtk_pcie_lm_reset(dev, rcv_num, lm_pos, lane);
				if (ret < 0)
					pr_info("[%s:%d], Reset fail\n", __func__, __LINE__);

				return lm_status;
			}
			lm_exc_sta = (lm_status & PCIE_LM_EXECUTION_STA_MASK) >> PCIE_LM_EXECUTION_STA_OFFSET;
			pr_info("After wait 200ms, Status 0x%x\n", lm_exc_sta);
		}

		if (lm_exc_sta != 0x2 || lm_step == lm_max_step) {
			lm_error = (lm_status & PCIE_LM_ERROR_CNT_MASK) >> PCIE_LM_PAYLOAD_OFFSET;
			pr_info("breaks at step %d: Status 0x%x, ErrCount: 0x%x, lane: %d\n",
				lm_step, lm_exc_sta, lm_error, lane);
			pcie_smt->eye[margin_type] = lm_step;
			break;
		}
	}

	/* Reset LM setting */
	ret = mtk_pcie_lm_reset(dev, rcv_num, lm_pos, lane);
	if (ret < 0)
		pr_info("[%s:%d], Reset fail\n", __func__, __LINE__);

	return ret;
}

static int mtk_pcie_lane_margin_voltage(struct mtk_pcie_info *pcie_smt, struct pci_dev *dev,
					int lane, int rcv_num, int lm_pos)
{
	int lm_status = 0, lm_volsup = 0, lm_max_step = 0;
	int ret = 0;

	/* Report LM cap. */
	lm_status = mtk_pcie_lm_cmd_complete(dev, PCIE_LM_REPORT_CAP_TYPE,
					     PCIE_LM_REPORT_CAP_PAYLOAD, rcv_num, lm_pos, lane);
	if (lm_status < 0) {
		pr_info("[%s:%d], Report cap fail\n", __func__, __LINE__);
		return lm_status;
	}

	lm_volsup = (lm_status & PCIE_LM_VOL_SUP_MASK) >> PCIE_LM_PAYLOAD_OFFSET;
	if (!lm_volsup){
		pr_info("Can't Support Voltage Margin\n");
		return 0;
	}

	/* Report Max Voltage Steps */
	lm_status = mtk_pcie_lm_cmd_complete(dev, PCIE_LM_VOL_STEP_TYPE,
					     PCIE_LM_VOL_STEP_PAYLOAD, rcv_num, lm_pos, lane);
	if (lm_status < 0) {
		pr_info("[%s:%d], Report MAX Voltage Steps fail\n", __func__, __LINE__);
		return lm_status;
	}

	lm_max_step = (lm_status & PCIE_LM_NUM_VOL_STEP_MASK) >> PCIE_LM_PAYLOAD_OFFSET;
	if (lm_max_step == 0)
		lm_max_step = 50;

	pr_info("Voltage Margin max step is %d\n", lm_max_step);

	ret = mtk_pcie_margin_step(pcie_smt, dev, lane, rcv_num, lm_pos,
				   lm_max_step, MTK_VOLTAGE_MARGIN_UP);
	if(ret)
		return ret;

	return mtk_pcie_margin_step(pcie_smt, dev, lane, rcv_num, lm_pos,
				    lm_max_step, MTK_VOLTAGE_MARGIN_DOWN);
}

static int mtk_pcie_lane_margin_time(struct mtk_pcie_info *pcie_smt, struct pci_dev *dev,
				     int lane, int rcv_num, int lm_pos)
{
	int lm_status = 0, lm_max_step = 0, ret = 0;

	/* Report Max Timing Steps */
	lm_status = mtk_pcie_lm_cmd_complete(dev, PCIE_LM_TIME_STEP_TYPE,
					     PCIE_LM_TIME_STEP_PAYLOAD, rcv_num, lm_pos, lane);
	if (lm_status < 0) {
		pr_info("[%s:%d], Report MAX Timing Steps fail\n", __func__, __LINE__);
		return lm_status;
	}

	lm_max_step = (lm_status & PCIE_LM_NUM_TIME_STEP_MASK) >> PCIE_LM_PAYLOAD_OFFSET;
	if (lm_max_step == 0)
		lm_max_step = 50;

	pr_info("Timing Margin max step is %d\n", lm_max_step);

	ret = mtk_pcie_margin_step(pcie_smt, dev, lane, rcv_num, lm_pos, lm_max_step, MTK_TIME_MARGIN_LEFT);
	if(ret)
		return ret;

	return ret = mtk_pcie_margin_step(pcie_smt, dev, lane, rcv_num, lm_pos, lm_max_step, MTK_TIME_MARGIN_RIGHT);
}

static int mtk_pcie_lane_margin(struct mtk_pcie_info *pcie_smt, struct pci_dev *dev, int mode)
{
	struct pci_dev *parent, *dut;
	struct mtk_aspm_state aspm;
	int pos = 0, ppos = 0;
	int ret = 0, width = 0, lane = 0, rcv_num = 0;
	int lm_pos = 0;
	u16 reg = 0;

	parent = dev->bus->self;
	pos = dev->pcie_cap;
	ppos = parent->pcie_cap;

	/*
	 * Setting before Lane margin:
	 * 1. Speed change to Gen4
	 * 2. Disable ASPM
	 * 3. Link ctrl Hardware Autonomous Width Disable = 1
	 * 4. Link ctrl 2 Hardware Autonomous Speed Disable = 1
	 */
	ret = mtk_pcie_change_speed(dev, MTK_SPEED_16_0GT);
	if (ret) {
		pr_info("Change speed to Gen4 fail.\n");
		return ret;
	}

	aspm.aspm_us = LP_NORMAL;
	aspm.aspm_ds = LP_NORMAL;
	ret = mtk_pcie_aspm(dev, aspm);
	if (ret) {
		pr_info("Disable aspm fail.\n");
		return ret;
	}

	pci_read_config_word(dev, pos + PCI_EXP_LNKCTL, &reg);
	pci_write_config_word(dev, pos + PCI_EXP_LNKCTL, reg & (~PCI_EXP_LNKCTL_HAWD));
	pci_read_config_word(parent, ppos + PCI_EXP_LNKCTL, &reg);
	pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL, reg & (~PCI_EXP_LNKCTL_HAWD));

	pci_read_config_word(dev, pos + PCI_EXP_LNKCTL2, &reg);
	pci_write_config_word(dev, pos + PCI_EXP_LNKCTL2, reg & (~PCI_EXP_LNKCTL2_HASD));
	pci_read_config_word(parent, ppos + PCI_EXP_LNKCTL2, &reg);
	pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL2, reg & (~PCI_EXP_LNKCTL2_HASD));

	if (mode == MTK_PCIE_DN) {
		dut = parent;
		rcv_num = PCIE_LM_DN_RCV_NUM;
	} else {
		dut = dev;
		rcv_num = PCIE_LM_UP_RCV_NUM;
	}
	pr_info("******* Test downstream port(%s) LM start *********\n", mode ? "EP" : "RC");

	lm_pos = pci_find_ext_capability(dut, PCI_EXT_CAP_ID_LM);
	if (!lm_pos) {
		pr_info("LM capability not found on device\n");
		return -ENODEV;
	}

	pci_read_config_word(dev, pos + PCI_EXP_LNKSTA, &reg);
	width = (reg & PCI_EXP_LNKSTA_NLW) >> 4;

	for (lane = 0; lane < width; lane++) {
		ret = mtk_pcie_lane_margin_voltage(pcie_smt, dut, lane, rcv_num, lm_pos);
		if (ret < 0) {
			pr_info("[%s:%d], Voltage margin fail\n", __func__, __LINE__);
			return ret;
		}

		ret = mtk_pcie_lane_margin_time(pcie_smt, dut, lane, rcv_num, lm_pos);
		if (ret < 0) {
			pr_info("[%s:%d], Time margin fail\n", __func__, __LINE__);
			return ret;
		}
	}

	pr_info("******* Test downstream port(%s) LM end *********\n", mode ? "EP" : "RC");

	return 0;
}

int mtk_pcie_lane_margin_entry(struct mtk_pcie_info *pcie_smt, int port, int mode)
{
	struct pci_dev *dev, *parent;
	unsigned int pos = 0, ppos = 0, bus = 1, devfn = 0;
	unsigned int speed_cap = 0, p_speed_cap = 0;
	int ret = 0;

	/**
	 * Given a PCI domain, bus, and slot/function number, the desired PCI
	 * device is located in the list of PCI devices. If the device is
	 * found, its reference count is increased and this function returns a
	 * pointer to its data structure. The caller must decrement the
	 * reference count by calling pci_dev_put(). If no device is found,
	 * %NULL is returned.
	 */
	dev = pci_get_domain_bus_and_slot(port, bus, devfn);
	if (!dev) {
		pr_info("can't find target device\n");
		return -1;
	}

	pos = dev->pcie_cap;
	parent = dev->bus->self;
	ppos = parent->pcie_cap;

	pci_read_config_dword(dev, pos + PCI_EXP_LNKCAP, &speed_cap);
	pci_read_config_dword(parent, ppos + PCI_EXP_LNKCAP, &p_speed_cap);
	speed_cap &= PCI_EXP_LNKCAP_SLS;
	p_speed_cap &= PCI_EXP_LNKCAP_SLS;

	if(speed_cap < MTK_SPEED_16_0GT && p_speed_cap < MTK_SPEED_16_0GT) {
		pr_info("[%s:%d], Don't sup Gen4, speed_cap=%#x, p_speed_cap=%#x\n",
			__func__, __LINE__, speed_cap, p_speed_cap);
		ret = -1;
		goto error;
	}

	ret = mtk_pcie_lane_margin(pcie_smt, dev, mode);
	if (ret)
		pr_info("[%s:%d], Lane Margin fail.\n", __func__, __LINE__);

error:
	pci_dev_put(dev);

	return ret;
}
EXPORT_SYMBOL(mtk_pcie_lane_margin_entry);

MODULE_LICENSE("GPL");
