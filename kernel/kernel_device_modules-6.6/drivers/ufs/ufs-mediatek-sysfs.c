// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <linux/async.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "ufshcd-priv.h"
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"
#include "ufs-mediatek-mimic.h"
#include "ufs-mediatek-priv.h"


#define MPHY_RX_LANE0_INDEX 4
#define MPHY_RX_LANE1_INDEX 5
#define EYEMON_GET 1
#define EYEMON_SET 0
#define HOST_EYE   1
#define DEVICE_EYE 0

#define PA_SCRAMBLING		0x1585

#define EYEMON_PRINTF(out, len, fmt, args...) \
	(len += sprintf(out+len, fmt, ##args))

static DEFINE_SEMAPHORE(eyemon_sem, 1);

struct eyemon_cmd {
	int get_set; /* 1: dme_(peer_)get, 0: dme_(peer_)set */
	u32 attr;
	u32 value;
};

enum eyemon_attr {
	RX_EYEMON_CAPABILITY			= 0xF1,
	RX_EYEMON_TIMING_MAX_STEPS_CAPABILITY	= 0xF2,
	RX_EYEMON_TIMING_MAX_OFFSET_CAPABILITY	= 0xF3,
	RX_EYEMON_VOLTAGE_MAX_STEPS_CAPABILITY	= 0xF4,
	RX_EYEMON_VOLTAGE_MAX_OFFSET_CAPABILITY	= 0xF5,
	RX_EYEMON_ENABLE			= 0xF6,
	RX_EYEMON_TIMING_STEPS			= 0xF7,
	RX_EYEMON_VOLTAGE_STEPS			= 0xF8,
	RX_EYEMON_TARGET_TEST_COUNT		= 0xF9,
	RX_EYEMON_TESTED_COUNT			= 0xFA,
	RX_EYEMON_ERROR_COUNT			= 0xFB,
	RX_EYEMON_START				= 0xFC,
};

enum eyemon_cmd_type {
	EYEMON_ENABLE,
	TIMING_SET,
	VOLTAGE_SET,
	EYEMON_START,
	ERR_CNT_GET,
	EYEMON_DISABLE,
	TARGET_TEST_COUNT,
	TESTED_COUNT,
	EYEMON_CAPABILITY,
	MAX_TIMING_STEPS_CAPABILITY,
	MAX_TIMING_OFFSET_CAPABILITY,
	MAX_VOLTAGE_STEPS_CAPABILITY,
	MAX_VOLTAGE_OFFSET_CAPABILITY,
	EYEMON_EN_CHECK,
};

struct eyemon_cmd eyemon_cmd_para[]= {
	{EYEMON_SET, RX_EYEMON_ENABLE, 1},
	{EYEMON_SET, RX_EYEMON_TIMING_STEPS, 0},
	{EYEMON_SET, RX_EYEMON_VOLTAGE_STEPS, 0},
	{EYEMON_GET, RX_EYEMON_START, 0},
	{EYEMON_GET, RX_EYEMON_ERROR_COUNT, 0},
	{EYEMON_SET, RX_EYEMON_ENABLE, 0},
	{EYEMON_SET, RX_EYEMON_TARGET_TEST_COUNT, 0},
	{EYEMON_GET, RX_EYEMON_TESTED_COUNT, 0},
	{EYEMON_GET, RX_EYEMON_CAPABILITY, 0},
	{EYEMON_GET, RX_EYEMON_TIMING_MAX_STEPS_CAPABILITY, 0},
	{EYEMON_GET, RX_EYEMON_TIMING_MAX_OFFSET_CAPABILITY, 0},
	{EYEMON_GET, RX_EYEMON_VOLTAGE_MAX_STEPS_CAPABILITY, 0},
	{EYEMON_GET, RX_EYEMON_VOLTAGE_MAX_OFFSET_CAPABILITY, 0},
	{EYEMON_GET, RX_EYEMON_ENABLE, 0},
};

int eyemon_cmd_parameter_setting(struct eyemon_cmd *cmd,
	int cmd_type, int write_val)
{
	int err = 0;

	if ((cmd_type < EYEMON_ENABLE) || (cmd_type > EYEMON_EN_CHECK))
		return -1;

	cmd->get_set = eyemon_cmd_para[cmd_type].get_set;
	cmd->attr = eyemon_cmd_para[cmd_type].attr;
	cmd->value = eyemon_cmd_para[cmd_type].value | write_val;

	return err;
}

int eyemon_dev_command(struct ufs_hba *hba, struct eyemon_cmd *cmd,
	int write_val, int lane_index, int cmd_type)
{
	int err = 0;

	err = eyemon_cmd_parameter_setting(cmd, cmd_type, write_val);

	if (cmd->get_set == EYEMON_SET) {
		err = ufshcd_dme_peer_set(hba,
			UIC_ARG_MIB_SEL(cmd->attr, lane_index), cmd->value);
	}  else if (cmd->get_set == EYEMON_GET) {
		err = ufshcd_dme_peer_get(hba,
			UIC_ARG_MIB_SEL(cmd->attr, lane_index), &cmd->value);
	}

	return err;
}

int eyemon_host_command(struct ufs_hba *hba, struct eyemon_cmd *cmd,
	int write_val, int lane_index, int cmd_type)
{
	int err = 0;

	err = eyemon_cmd_parameter_setting(cmd, cmd_type, write_val);

	if (cmd->get_set == EYEMON_SET) {
		err = ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(cmd->attr, lane_index), cmd->value);
	}  else if (cmd->get_set == EYEMON_GET) {
		err = ufshcd_dme_get(hba,
			UIC_ARG_MIB_SEL(cmd->attr, lane_index), &cmd->value);
	}

	return err;
}

int eyemon_command(struct ufs_hba *hba, struct eyemon_cmd *cmd, int host_device,
	int write_val, int lane_index, int cmd_type)
{
	int err = 0;

	if (host_device == DEVICE_EYE)
		err = eyemon_dev_command(hba, cmd, write_val,
			lane_index, cmd_type);
	else
		err = eyemon_host_command(hba, cmd, write_val,
			lane_index, cmd_type);

	return err;
}

/*
 * Get the eye data index by timing & voltage, and prevent index out of bounds
 */
int eyemon_get_eye_index(struct ufs_hba *hba, int timing, int voltage,
	int eye_data_cnt,int timing_step, int voltage_step, int timing_start,
	int timing_end, int voltage_start, int voltage_end, int *index)
{
	int err = 0;
	int total_timing_offset;

	if (timing < min(timing_start, timing_end) ||
		timing > max(timing_start, timing_end) ||
		voltage < min(voltage_start, voltage_end) ||
		voltage > max(voltage_start, voltage_end)) {
		err = 1;
		dev_err(hba->dev, "Eye data index out of bound\n");
		return err;
	}

	total_timing_offset = (max(timing_start, timing_end) -
		min(timing_start, timing_end)) / timing_step + 1;

	*index = (max(voltage_start, voltage_end) - voltage) /
		voltage_step * total_timing_offset + timing / timing_step;

	if (*index >= eye_data_cnt || *index < 0) {
		err = 1;
		dev_err(hba->dev, "Eye data index out of bound\n");
		return err;
	}

	return err;
}

int eyemon_scan(struct ufs_hba *hba, int lane, int host_device,
	int target_test_count, int timing_step, int voltage_step,
	char *out,  ssize_t *size)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct eyemon_cmd cmd;
	int err = 0;
	int i = 0;
	int lane_index = MPHY_RX_LANE0_INDEX;
	int timing, voltage;
	int max_timing_offset, max_voltage_offset;
	int max_timing_step, max_voltage_step;
	int ui_per_step;
	int mv_per_step;
	int timing_offset, voltage_offset;
	int timing_start, timing_end, voltage_start, voltage_end;
	u32 err_cnt;
	u32 *eye_data = NULL;
	char eye_row[1000] = "";
	char eye_tmp[4] = "";
	char row_label[16] = "";
	int eye_count, eye_data_cnt = 0;
	int val = 0;
	int tmp_w = 0;
	int max_w, max_w_idx;
	int tmp_h = 0;
	int max_h, max_h_idx;
	int tmp_mv, tmp_ui;
	bool flag_res = true;

	/* check eye monitor capability */
	err = eyemon_command(hba, &cmd, host_device, 0, lane_index, EYEMON_CAPABILITY);
	if (err)
		return -1;

	if ((cmd.value != 0x1) || ((host_device == HOST_EYE) &&
		(host->ip_ver <= IP_VER_MT6899))) {
		EYEMON_PRINTF(out, *size,
			"[UFS] %s does not support eye monitor!\n",
			(host_device == HOST_EYE) ? "Host" : "Device");
		return -1;
	}

	/* get max timing and voltage steps */
	err = eyemon_command(hba, &cmd, host_device, 0, lane_index,
		MAX_TIMING_STEPS_CAPABILITY);
	if (err)
		return -1;
	max_timing_step = cmd.value;

	err = eyemon_command(hba, &cmd, host_device, 0, lane_index,
		MAX_VOLTAGE_STEPS_CAPABILITY);
	if (err)
		return -1;
	max_voltage_step = cmd.value;

	eye_count = ((max_timing_step * 2 / timing_step) + 1) *
			((max_voltage_step * 2 / voltage_step) + 1);

	eye_data = kcalloc(eye_count, sizeof(u32), GFP_KERNEL);
	if (eye_data == NULL)
		return -1;

	/* get max timing and voltage */
	err = eyemon_command(hba, &cmd, host_device, 0, lane_index,
		MAX_TIMING_OFFSET_CAPABILITY);
	if (err)
		goto exit_free_eye_data;
	max_timing_offset = cmd.value;

	err = eyemon_command(hba, &cmd, host_device, 0, lane_index,
		MAX_VOLTAGE_OFFSET_CAPABILITY);
	if (err)
		goto exit_free_eye_data;
	max_voltage_offset = cmd.value;

	/* Host step need fine tune for gear 5 */
	if (host_device == HOST_EYE) {
		if (host->max_gear == UFS_HS_G5)
			max_timing_step = 32;
	}

	ui_per_step = max_timing_offset * 100 / max_timing_step;
	mv_per_step = max_voltage_offset * 10 * 100 / max_voltage_step;

	/* Mediatek host EOM mv_per_step always = 6mV */
	if ((host->ip_ver >= IP_VER_MT6991_A0) && (host_device == HOST_EYE))
		mv_per_step = 600;

	EYEMON_PRINTF(out, *size, "[UFS] max timing steps: %d\n",
		max_timing_step);
	EYEMON_PRINTF(out, *size, "[UFS] max voltage steps: %d\n",
		max_voltage_step);
	EYEMON_PRINTF(out, *size, "[UFS] max timing offset: 0.%02d UI\n",
		max_timing_offset);
	EYEMON_PRINTF(out, *size, "[UFS] max voltage offset: %d mV\n",
		max_voltage_offset * 10);
	EYEMON_PRINTF(out, *size, "[UFS] timing per step: 0.%04d UI\n",
		ui_per_step);
	EYEMON_PRINTF(out, *size, "[UFS] voltage per step: %d.%02d mV\n",
		mv_per_step / 100, mv_per_step % 100);


	/* eye monitor body */
	lane_index = MPHY_RX_LANE0_INDEX + lane;
	voltage_start = max_voltage_step * 2;
	voltage_end = 0;
	timing_start = 0;
	timing_end = max_timing_step * 2;

	EYEMON_PRINTF(out, *size, "[UFS] voltage_start: %d, voltage_end: %d, voltage_step: %d\n",
		voltage_start, voltage_end, voltage_step);

	EYEMON_PRINTF(out, *size, "[UFS] timing_start: %d, timing_end: %d, timing_step: %d\n",
		timing_start, timing_end, timing_step);

	/* Change to lower power mode to trigger configuration update */
	ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_G1);

	/*
	 * sweep offset from left to right (timing = [0:2 * max_timing_step]),
	 * top to down (voltage = [2 * max_voltage_step:0])
	 * ex: max_voltage_step = max_timing_step = 64
	 *
	 *                             - - > (timing from 0 to 128)
	 *                             0 4 ......124 128
	 *                             - - - - - - - - -
	 *                        128 |x x x x x x x x x|
	 *                 |      124 |x x x x x x x x x|
	 *                 |      ... |x x x o o o x x x|
	 *                 |      ... |x o o o o o o o x|
	 *                 v      ... |x x x o o o x x x|
	 * (voltage from 128 to 0)  4 |x x x x x x x x x|
	 *                          0 |x x x x x x x x x|
	 *                             - - - - - - - - -
	 */

	/* sweep offset from top left to bottom right of the eye */
	for (voltage = voltage_start; voltage >= voltage_end; voltage -= voltage_step) {
		for (timing = timing_start; timing <= timing_end; timing += timing_step) {
			err_cnt = 0;

			/* set timing step */
			timing_offset = timing - max_timing_step;
			if (timing - max_timing_step < 0)
				timing_offset = (max_timing_step - timing) | (1 << 6);

			err = eyemon_command(hba, &cmd, host_device, timing_offset, lane_index, TIMING_SET);
			if (err)
				goto exit_free_eye_data;

			/* set voltage step */
			voltage_offset = voltage - max_voltage_step;
			if (voltage - max_voltage_step < 0)
				voltage_offset = (max_voltage_step - voltage) | (1 << 6);

			err = eyemon_command(hba, &cmd, host_device, voltage_offset, lane_index, VOLTAGE_SET);
			if (err)
				goto exit_free_eye_data;

			/* set target test count */
			err = eyemon_command(hba, &cmd, host_device, target_test_count, lane_index, TARGET_TEST_COUNT);
			if (err)
				goto exit_free_eye_data;

			/* eyemon enable */
			err = eyemon_command(hba, &cmd, host_device, 0, lane_index, EYEMON_ENABLE);
			if (err)
				goto exit_free_eye_data;

			/* power mode change to highest mode */
			ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_UP);

			/* read flag to assure sending FLR on traffic after PMC to HS fast mode */
			err = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,
					QUERY_FLAG_IDN_FDEVICEINIT, 0, &flag_res);
			if (err)
				goto exit_free_eye_data;

			/* start to calculate error count until EYEMON_START bit = 0 */
			while (true) {
				err = eyemon_command(hba, &cmd, host_device, 0, lane_index, EYEMON_START);
				if (err)
					goto exit_free_eye_data;

				if (!cmd.value)
					break;
			}

			/* get err cnt */
			err = eyemon_command(hba, &cmd, host_device, 0, lane_index, ERR_CNT_GET);
			if (err)
				goto exit_free_eye_data;

			err_cnt += cmd.value;

			/*
			 * get tested count
			 * err = eyemon_command(hba, &cmd, host_device, 0, lane_index, TESTED_COUNT);
			 * if (err)
			 *	goto exit_free_eye_data;
			 */

			/* eyemon disable */
			err = eyemon_command(hba, &cmd, host_device, 0, lane_index, EYEMON_DISABLE);
			if (err)
				goto exit_free_eye_data;

			/* change to lower power mode to trigger configuration update */
			ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_DOWN);

			/* calculate average error count and tested count */
			eye_data[eye_data_cnt] = err_cnt;
			eye_data_cnt++;
			/*
			 * dev_info(hba->dev, "[UFS] timing:%d, voltage:%d, avg_err_cnt:%d\n",
			 *	 timing_offset, voltage_offset, err_cnt);
			 */
		}
	}

	/* eyemon disable */
	err = eyemon_command(hba, &cmd, host_device, 0, lane_index, EYEMON_DISABLE);
	if (err)
		goto exit_free_eye_data;

	ufs_mtk_dynamic_clock_scaling(hba, CLK_SCALE_FREE_RUN);

	/* find max width & height */
	tmp_w = 0;
	max_w = 0;

	for (voltage = voltage_start; voltage >= voltage_end; voltage -= voltage_step) {
		for (timing = timing_start; timing <= timing_end; timing += timing_step) {

			err = eyemon_get_eye_index(hba, timing, voltage, eye_data_cnt, timing_step,
				voltage_step, timing_start, timing_end, voltage_start, voltage_end, &i);
			if (err)
				goto exit_free_eye_data;

			val = eye_data[i];
			if (val == 0) {
				tmp_w++;
				if (tmp_w > max_w) {
					max_w = tmp_w;
					max_w_idx = voltage;
				}
			} else {
				tmp_w = 0;
			}
		}
		tmp_w = 0;
	}

	tmp_ui = max_w * timing_step * ui_per_step;
	EYEMON_PRINTF(out, *size, "[UFS] max width:0.%02d UI (voltage index = %d, max_w = %d)\n",
		tmp_ui, max_w_idx - max_voltage_step, max_w);

	tmp_h = 0;
	max_h = 0;

	for (timing = timing_start; timing <= timing_end; timing += timing_step) {
		for (voltage = voltage_start; voltage >= voltage_end; voltage -= voltage_step) {

			err = eyemon_get_eye_index(hba, timing, voltage, eye_data_cnt, timing_step,
				voltage_step, timing_start, timing_end, voltage_start, voltage_end, &i);
			if (err)
				goto exit_free_eye_data;

			val = eye_data[i];
			if (val == 0) {
				tmp_h++;
				if (tmp_h > max_h) {
					max_h = tmp_h;
					max_h_idx = timing;
				}
			} else {
				tmp_h = 0;
			}
		}
		tmp_h = 0;
	}

	tmp_mv = max_h * voltage_step * mv_per_step;
	EYEMON_PRINTF(out, *size, "[UFS] eye height:%d.%02d mV (timing index = %d, max_h = %d)\n",
		tmp_mv / 100, tmp_mv % 100, max_h_idx - max_timing_step, max_h);


	/* print eye monitor result on terminal */
	EYEMON_PRINTF(out, *size, "[UFS] Error count lane: %d\n", lane);

	for (voltage = voltage_start; voltage >= voltage_end; voltage -= voltage_step) {
		memset(eye_row, 0, sizeof(eye_row));
		if (snprintf(row_label, sizeof(row_label), "[%+03d]: ", voltage - max_voltage_step) < 0)
			goto exit_free_eye_data;

		strcat(eye_row, row_label);
		for (timing = timing_start; timing <= timing_end; timing += timing_step) {

			err = eyemon_get_eye_index(hba, timing, voltage, eye_data_cnt, timing_step,
				voltage_step, timing_start, timing_end, voltage_start, voltage_end, &i);
			if (err)
				goto exit_free_eye_data;

			if (snprintf(eye_tmp, sizeof(eye_tmp), "%02x ", eye_data[i]) < 0)
				goto exit_free_eye_data;

			strcat(eye_row, eye_tmp);
		}
		strcat(eye_row, "\n");

		EYEMON_PRINTF(out, *size, "%s", eye_row);
		memset(eye_row, 0, sizeof(eye_row));

	}
	EYEMON_PRINTF(out, *size, "\n");

exit_free_eye_data:
	kfree(eye_data);

	return err;
}

void eyemon_scan_show(struct ufs_hba *hba, char *out, ssize_t *size,
	int host_device, int lane)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret;
	u32 max_t_step, max_v_step;
	int t, v;

	ufshcd_rpm_get_sync(hba);
	ufsm_scsi_block_requests(hba);
	ufshcd_hold(hba);
	down(&eyemon_sem);

	if (ufsm_wait_for_doorbell_clr(hba, 1000 * 1000)) { /* 1 sec */
		dev_err(hba->dev, "%s: ufshcd_wait_for_doorbell_clr timeout!\n",
				__func__);
		goto out;
	}

	if (host_device == DEVICE_EYE) {
		ret = ufshcd_dme_peer_get(hba,
			UIC_ARG_MIB_SEL(RX_EYEMON_TIMING_MAX_STEPS_CAPABILITY,
			MPHY_RX_LANE0_INDEX), &max_t_step);
		if (ret)
			EYEMON_PRINTF(out, *size, "Failed get max_t_step! ret=%d!\n",
				ret);

		ret = ufshcd_dme_peer_get(hba,
			UIC_ARG_MIB_SEL(RX_EYEMON_VOLTAGE_MAX_STEPS_CAPABILITY,
			MPHY_RX_LANE0_INDEX), &max_v_step);
		if (ret)
			EYEMON_PRINTF(out, *size, "Failed get max_v_step! ret=%d!\n",
				ret);
	} else {
		ret = ufshcd_dme_get(hba,
			UIC_ARG_MIB_SEL(RX_EYEMON_TIMING_MAX_STEPS_CAPABILITY,
			MPHY_RX_LANE0_INDEX), &max_t_step);
		if (ret)
			EYEMON_PRINTF(out, *size, "Failed get max_t_step! ret=%d!\n",
				ret);

		ret = ufshcd_dme_get(hba,
			UIC_ARG_MIB_SEL(RX_EYEMON_VOLTAGE_MAX_STEPS_CAPABILITY,
			MPHY_RX_LANE0_INDEX), &max_v_step);
		if (ret)
			EYEMON_PRINTF(out, *size, "Failed get max_v_step! ret=%d!\n",
				ret);

		/* Host step need fine tune for gear 5 */
		if (host->max_gear == UFS_HS_G5)
			max_t_step = 32;
	}

	if (max_t_step >= 63)
		t = 4;
	else if (max_t_step >= 31)
		t = 2;
	else
		t = 1;

	if (max_v_step >= 63)
		v = 4;
	else if (max_v_step >= 31)
		v = 2;
	else
		v = 1;

	/*
	 * Disable auto-hibern8 for eye monitor scan.
	 * Prevent traffic+FLR enter auto-hibern8 if test count increase.
	 */
	ret = ufs_mtk_auto_hibern8_disable(hba);
	if (ret) {
		dev_err(hba->dev, "%s: disable AH8 failed (%d)",
			__func__, ret);
		goto out;
	}

	/* Enable scramble, else eye diagram may bigger than reality */
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 1);
	if (ret) {
		dev_err(hba->dev, "%s: Enable scramble failed (%d)",
			__func__, ret);
		goto out_ah8;
	}

	ret = eyemon_scan(hba, lane, host_device, 0x3F, t, v, out, size);
	if (ret)
		EYEMON_PRINTF(out, *size, "Failed eye monitor scan! ret=%d!\n",
			ret);


	/* Disable scramble */
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 0);
	if (ret) {
		dev_err(hba->dev, "%s: Disable scramble failed (%d)",
			__func__, ret);
		goto out;
	}

	/* Change power mode to trigger scramble update */
	ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_G1);
	ufs_mtk_dynamic_clock_scaling(hba, CLK_SCALE_FREE_RUN);

out_ah8:
	/* Enable auto-hibern8 */
	ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);

out:
	up(&eyemon_sem);
	ufshcd_release(hba);
	ufsm_scsi_unblock_requests(hba);
	ufshcd_rpm_put(hba);
}

static ssize_t downdifferential_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", hba->vps->ondemand_data.downdifferential);
}

static ssize_t downdifferential_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int err = 0;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&hba->devfreq->lock);
	if (value > 100 || value > hba->vps->ondemand_data.upthreshold) {
		err = -EINVAL;
		goto out;
	}
	hba->vps->ondemand_data.downdifferential = value;

out:
	mutex_unlock(&hba->devfreq->lock);
	return err ? err : count;
}

static ssize_t upthreshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", hba->vps->ondemand_data.upthreshold);
}

static ssize_t upthreshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int err = 0;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&hba->devfreq->lock);
	if (value > 100 || value < hba->vps->ondemand_data.downdifferential) {
		err = -EINVAL;
		goto out;
	}
	hba->vps->ondemand_data.upthreshold = value;

out:
	mutex_unlock(&hba->devfreq->lock);
	return err ? err : count;
}

static ssize_t clkscale_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ssize_t size = 0;
	int value;

	value = atomic_read((&host->clkscale_control));

	size += sprintf(buf + size, "current: %d\n", value);
	size += sprintf(buf + size, "===== control manual =====\n");
	size += sprintf(buf + size, "0: free run\n");
	size += sprintf(buf + size, "1: scale down\n");
	size += sprintf(buf + size, "2: scale up\n");
	size += sprintf(buf + size, "3: scale down and frobid change\n");
	size += sprintf(buf + size, "4: scale up and frobid change\n");
	size += sprintf(buf + size, "5: allow change and free run\n");

	return size;
}

static ssize_t clkscale_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	const char *opcode = buf;
	u32 value, value_orig;

	if (!strncmp(buf, "powerhal_set: ", 14))
		opcode = buf + 14;

	if (kstrtou32(opcode, 0, &value) || value > 5)
		return -EINVAL;

	/* Only UFS 4.0 device need  */
	if (hba->dev_info.wspecversion < 0x0400)
		return count;

	value_orig = atomic_read((&host->clkscale_control));

	switch (value) {
	case 0: /* free run */
		/*
		 * free run is not allowed in user-only mode
		 * since related-devfreq is not initialized
		 */
		if (host->caps & UFS_MTK_CAP_CLK_SCALE_ONLY_BY_USER)
			goto out;

		ufs_mtk_dynamic_clock_scaling(hba, CLK_SCALE_FREE_RUN);
		break;

	case 1: /* scale down */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_DOWN);
		break;

	case 2: /* scale up */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_UP);
		break;

	case 3: /* scale down and not allow change anymore */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_DOWN);
		host->clk_scale_forbid = true;
		break;

	case 4: /* scale up and not allow change anymore */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_UP);
		host->clk_scale_forbid = true;
		break;

	case 5: /* free run and allow change */
		host->clk_scale_forbid = false;
		if (!(host->caps & UFS_MTK_CAP_CLK_SCALE_ONLY_BY_USER)) {
			ufs_mtk_dynamic_clock_scaling(hba, CLK_SCALE_FREE_RUN);
			value = 0;
		} else {
			if (value_orig == 3)
				value = 1;
			else if (value_orig == 4)
				value = 2;
		}
		break;

	default:
		goto out;
	}

	atomic_set(&host->clkscale_control, value);

out:

	return count;
}

static DEVICE_ATTR_RW(downdifferential);
static DEVICE_ATTR_RW(upthreshold);
static DEVICE_ATTR_RW(clkscale_control);

static struct attribute *ufs_mtk_sysfs_clkscale_attrs[] = {
	&dev_attr_downdifferential.attr,
	&dev_attr_upthreshold.attr,
	&dev_attr_clkscale_control.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_clkscale_group = {
	.name = "clkscale",
	.attrs = ufs_mtk_sysfs_clkscale_attrs,
};

static void init_clk_scaling_sysfs(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host->caps & UFS_MTK_CAP_CLK_SCALE_ONLY_BY_USER)
		atomic_set(&host->clkscale_control, 2);
	else
		atomic_set(&host->clkscale_control, 0);
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_clkscale_group))
		dev_info(hba->dev, "Failed to create sysfs for clkscale_control\n");
}

static void remove_clk_scaling_sysfs(struct ufs_hba *hba)
{
	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_clkscale_group);
}

static ssize_t device_eye_lane0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ssize_t size = 0;

	eyemon_scan_show(hba, buf, &size, DEVICE_EYE, 0);

	return size;
}

static ssize_t device_eye_lane1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ssize_t size = 0;

	eyemon_scan_show(hba, buf, &size, DEVICE_EYE, 1);

	return size;
}

static ssize_t host_eye_lane0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ssize_t size = 0;

	eyemon_scan_show(hba, buf, &size, HOST_EYE, 0);

	return size;
}

static ssize_t host_eye_lane1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ssize_t size = 0;

	eyemon_scan_show(hba, buf, &size, HOST_EYE, 1);

	return size;
}

static DEVICE_ATTR_RO(device_eye_lane0);
static DEVICE_ATTR_RO(device_eye_lane1);
static DEVICE_ATTR_RO(host_eye_lane0);
static DEVICE_ATTR_RO(host_eye_lane1);

static struct attribute *ufs_mtk_sysfs_eyemon_attrs[] = {
	&dev_attr_device_eye_lane0.attr,
	&dev_attr_device_eye_lane1.attr,
	&dev_attr_host_eye_lane0.attr,
	&dev_attr_host_eye_lane1.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_eyemon_group = {
	.name = "eyemon",
	.attrs = ufs_mtk_sysfs_eyemon_attrs,
};

static void init_eyemon_sysfs(struct ufs_hba *hba)
{
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_eyemon_group))
		dev_info(hba->dev, "Failed to create sysfs for eye monitor\n");
}

static void remove_eyemon_sysfs(struct ufs_hba *hba)
{
	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_eyemon_group);
}

static int write_irq_affinity(unsigned int irq, const char *buf)
{
	cpumask_var_t new_mask;
	int ret;

	if (!zalloc_cpumask_var(&new_mask, GFP_KERNEL))
		return -ENOMEM;

	ret = cpumask_parse(buf, new_mask);
	if (ret)
		goto free;

	if (!cpumask_intersects(new_mask, cpu_online_mask)) {
		ret = -EINVAL;
		goto free;
	}

	ret = irq_set_affinity(irq, new_mask);

free:
	free_cpumask_var(new_mask);
	return ret;
}

static ssize_t smp_affinity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct irq_desc *desc = irq_to_desc(hba->irq);
	const struct cpumask *mask;

	mask = desc->irq_common_data.affinity;
#ifdef CONFIG_GENERIC_PENDING_IRQ
	if (irqd_is_setaffinity_pending(&desc->irq_data))
		mask = desc->pending_mask;
#endif

	return sprintf(buf, "%*pb\n", cpumask_pr_args(mask));
}

static ssize_t smp_affinity_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long mask;
	int ret = count;

	if (kstrtoul(buf, 16, &mask) || mask >= 256)
		return -EINVAL;

	ret = write_irq_affinity(hba->irq, buf);
	if (!ret)
		ret = count;

	dev_info(hba->dev, "set irq affinity %lx\n", mask);

	return count;
}

static DEVICE_ATTR_RW(smp_affinity);

static struct attribute *ufs_mtk_sysfs_irq_attrs[] = {
	&dev_attr_smp_affinity.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_irq_group = {
	.name = "irq",
	.attrs = ufs_mtk_sysfs_irq_attrs,
};

static void init_irq_sysfs(struct ufs_hba *hba)
{
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_irq_group))
		dev_info(hba->dev, "Failed to create sysfs for irq\n");
}

static void remove_irq_sysfs(struct ufs_hba *hba)
{
	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_irq_group);
}

static ssize_t dbg_tp_unregister_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ssize_t size = 0;
	int value;

	value = atomic_read(&host->dbg_tp_unregister);
	size += sprintf(buf + size, "%d\n", value);

	return size;
}

static ssize_t dbg_tp_unregister_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	const char *opcode = buf;
	u32 value;

	if (kstrtou32(opcode, 0, &value) || value > 1)
		return -EINVAL;

	if (value == atomic_xchg(&host->dbg_tp_unregister, value))
		return count;

	if (value)
		ufs_mtk_dbg_tp_unregister();
	else
		ufs_mtk_dbg_tp_register();

	return count;
}

static ssize_t skip_blocktag_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ssize_t size = 0;
	int value;

	value = atomic_read((&host->skip_btag));
	size += sprintf(buf + size, "%d\n", value);

	return size;
}

static ssize_t skip_blocktag_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	const char *opcode = buf;
	u32 value;

	if (kstrtou32(opcode, 0, &value) || value > 1)
		return -EINVAL;

	atomic_set(&host->skip_btag, value);

	return count;
}

static DEVICE_ATTR_RW(skip_blocktag);
static DEVICE_ATTR_RW(dbg_tp_unregister);

static struct attribute *ufs_mtk_sysfs_attrs[] = {
	&dev_attr_skip_blocktag.attr,
	&dev_attr_dbg_tp_unregister.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_group = {
	.attrs = ufs_mtk_sysfs_attrs,
};

void ufs_mtk_init_sysfs(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	atomic_set(&host->skip_btag, 0);
	atomic_set(&host->dbg_tp_unregister, 0);
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_group))
		dev_info(hba->dev, "Failed to create sysfs for btag\n");

	if ((hba->caps & UFSHCD_CAP_CLK_SCALING) ||
	    (host->caps & UFS_MTK_CAP_CLK_SCALE_ONLY_BY_USER))
		init_clk_scaling_sysfs(hba);

	init_eyemon_sysfs(hba);

	init_irq_sysfs(hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_init_sysfs);

void ufs_mtk_remove_sysfs(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_group);

	if ((hba->caps & UFSHCD_CAP_CLK_SCALING) ||
	    (host->caps & UFS_MTK_CAP_CLK_SCALE_ONLY_BY_USER))
		remove_clk_scaling_sysfs(hba);

	remove_eyemon_sysfs(hba);

	remove_irq_sysfs(hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_remove_sysfs);
