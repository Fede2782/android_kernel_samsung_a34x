// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "aputop_log.h"
#include "apu_hw_sema.h"
#include "mt6991_apupwr.h"
#include "mt6991_apupwr_prot.h"
#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)

static uint32_t mbox_data;

/* Below reg_name has to 1-1 mapping DTS's name */
static const char *reg_name[APUPW_MAX_REGS] = {
	"sys_vlp", "sys_spm", "apu_rcx", "apu_rcx_dla", "apu_vcore",
	"apu_md32_mbox", "apu_rpc", "apu_pcu", "apu_ao_ctl", "apu_pll",
	"apu_acc", "apu_are", "apu_rpctop_mdla", "apu_acx0", "apu_acx0_rpc_lite"
	, "apu_acx1", "apu_acx1_rpc_lite", "apu_acx2", "apu_acx2_rpc_lite"
};

static struct apu_power apupw = {
	.env = MP,
	.rcx = CE_FW,
};

#if APUPW_DUMP_FROM_APMCU
static void aputop_dump_reg(enum apupw_reg idx, uint32_t offset, uint32_t size)
{
	char buf[32];
	int ret = 0;

	/* prepare pa address */
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08lx: ",
			(ulong)(apupw.phy_addr[idx]) + offset);
	/* dump content with pa as prefix */
	if (ret)
		print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			       apupw.regs[idx] + offset, size, true);
}
#endif

static int init_reg_base(struct platform_device *pdev)
{
	struct resource *res;
	int idx = 0;

	pr_info("%s %d APUPW_MAX_REGS = %d\n",
			__func__, __LINE__, APUPW_MAX_REGS);
	for (idx = 0; idx < APUPW_MAX_REGS; idx++) {
		res = platform_get_resource_byname(
				pdev, IORESOURCE_MEM, reg_name[idx]);
		if (res == NULL) {
			pr_info("%s: get resource \"%s\" fail\n",
					__func__, reg_name[idx]);
			return -ENODEV;
		}
		pr_info("%s: get resource \"%s\" pass\n",
				__func__, reg_name[idx]);
		apupw.regs[idx] = ioremap(res->start,
				res->end - res->start + 1);
		if (IS_ERR_OR_NULL(apupw.regs[idx])) {
			pr_info("%s: %s remap base fail\n",
					__func__, reg_name[idx]);
			return -ENOMEM;
		}
		pr_info("%s: %s remap base 0x%llx to 0x%p\n",
				__func__, reg_name[idx],
				res->start, apupw.regs[idx]);
		apupw.phy_addr[idx] = res->start;
	}

	return 0;
}

static uint32_t apusys_pwr_smc_call(struct device *dev, uint32_t smc_id,
		uint32_t a2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
			a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%lu)\n",
				__func__,
				smc_id, res.a0);

	return res.a0;
}

/*
 * APU_PCU_SEMA_CTRL
 * [15:00]      SEMA_KEY_SET    Each bit corresponds to different user.
 * [31:16]      SEMA_KEY_CLR    Each bit corresponds to different user.
 *
 * ctl:
 *      0x1 - acquire hw semaphore
 *      0x0 - release hw semaphore
 */
#if APU_HW_SEMA_CTRL
static int apu_hw_sema_ctl(struct device *dev, uint32_t sema_offset,
		uint8_t usr_bit, uint8_t ctl, int32_t timeout)
{
	static uint16_t timeout_cnt_max;
	uint16_t timeout_cnt = 0;
	uint8_t ctl_bit = 0;
	int smc_op;

	if (ctl == 0x1) {
		// acquire is set
		ctl_bit = usr_bit;
		smc_op = SMC_HW_SEMA_PWR_CTL_LOCK;
	} else if (ctl == 0x0) {
		// release is clear
		ctl_bit = usr_bit + 16;
		smc_op = SMC_HW_SEMA_PWR_CTL_UNLOCK;
	} else {
		return -1;
	}

	if (log_lvl)
		pr_info("%s ++ usr_bit:%d ctl:%d (0x%p = 0x%08x)\n",
				__func__, usr_bit, ctl,
				apupw.regs[apu_pcu] + sema_offset,
				apu_readl(apupw.regs[apu_pcu] + sema_offset));

	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			smc_op);
	udelay(1);

	while ((apu_readl(apupw.regs[apu_pcu] + sema_offset) & BIT(ctl_bit))
			>> ctl_bit != ctl) {
		if (timeout >= 0 && timeout_cnt++ >= timeout) {
			pr_info(
			"%s timeout usr_bit:%d ctl:%d rnd:%d (0x%p = 0x%08x)\n",
				__func__, usr_bit, ctl, timeout_cnt,
				apupw.regs[apu_pcu] + sema_offset,
				apu_readl(apupw.regs[apu_pcu] + sema_offset));
			return -1;
		}

		if (ctl == 0x1) {
			apusys_pwr_smc_call(dev,
					MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
					smc_op);
		}
		udelay(1);
	}

	if (timeout_cnt > timeout_cnt_max)
		timeout_cnt_max = timeout_cnt;

	if (log_lvl)
		pr_info("%s -- usr_bit:%d ctl:%d (0x%p = 0x%08x) mx:%d\n",
				__func__, usr_bit, ctl,
				apupw.regs[apu_pcu] + sema_offset,
				apu_readl(apupw.regs[apu_pcu] + sema_offset),
				timeout_cnt_max);

	return 0;
}
#endif

/*
 * sub_func id :
 *	0 - DRV_STAT_SYNC_REG
 *	1 - MBRAIN_DATA_SYNC_0_REG
 *	2 - MBRAIN_DATA_SYNC_1_REG
 */
static void plat_get_up_drv_data(struct aputop_func_param *aputop)
{
	int sub_func = 0;

	sub_func = aputop->param1;

	if (sub_func == 0) {
		mbox_data = apu_readl(
			apupw.regs[apu_md32_mbox] + DRV_STAT_SYNC_REG);
	} else if (sub_func == 1) {
		mbox_data = apu_readl(
			apupw.regs[apu_md32_mbox] + MBRAIN_DATA_SYNC_0_REG);
	} else if (sub_func == 2) {
		mbox_data = apu_readl(
			apupw.regs[apu_md32_mbox] + MBRAIN_DATA_SYNC_1_REG);
	} else if (sub_func == 3) {
		mbox_data = apu_readl(
			apupw.regs[apu_are] + MBRAIN_RCX_CNT);
	} else if (sub_func == 4) {
		mbox_data = apu_readl(
			apupw.regs[apu_are] + MBRAIN_DVFS_CNT);
	} else {
		pr_info("%s#%d invalid sub_func : %d\n",
				__func__, __LINE__, sub_func);
	}
}

/*
 * APU_PCU_SEMA_READ
 * [15:00]      SEMA_KEY_SET    Each bit corresponds to different user.
 */
static uint32_t plat_apu_boot_host(void)
{
	uint32_t host = 0;
	uint32_t sema_offset = APU_HW_SEMA_PWR_CTL;

	host = apu_readl(apupw.regs[apu_pcu] + sema_offset) & 0x0000FFFF;
	if (host == BIT(SYS_APMCU))
		return SYS_APMCU;
	else if  (host == BIT(SYS_APU))
		return SYS_APU;
	else if  (host == BIT(SYS_SCP_LP))
		return SYS_SCP_LP;
	else if  (host == BIT(SYS_SCP_NP))
		return SYS_SCP_NP;
	else
		return SYS_MAX;
}

static void aputop_dump_pwr_reg(struct device *dev)
{
	// dump reg in ATF log
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_ALL);
	// dump reg in AEE db
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_REGDUMP, 0);
}

static void aputop_dump_rpc_data(void)
{
	char buf[32];
	int ret = 0;

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_rpc]));
	if (!ret)
		print_hex_dump(KERN_INFO, buf, DUMP_PREFIX_OFFSET, 16, 4,
			       apupw.regs[apu_rpc], 0x20, true);
}

static void aputop_dump_pcu_data(struct device *dev)
{
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_PCU);
}

#if APUPW_DUMP_FROM_APMCU
static void aputop_dump_pll_data(void)
{
	// need to 1-1 in order mapping with array in __apu_pll_init func
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE};
	uint32_t pll_offset_arr[] = {
				PLL1CPLL_FHCTL_HP_EN, PLL1CPLL_FHCTL_RST_CON,
				PLL1CPLL_FHCTL_CLK_CON, PLL1CPLL_FHCTL0_CFG,
				PLL1C_PLL1_CON1, PLL1CPLL_FHCTL0_DDS};
	int base_arr_size = ARRAY_SIZE(pll_base_arr);
	int offset_arr_size = ARRAY_SIZE(pll_offset_arr);
	int pll_idx;
	int ofs_idx;
	uint32_t phy_addr = 0x0;
	char buf[256];
	int ret = 0;

	for (pll_idx = 0 ; pll_idx < base_arr_size ; pll_idx++) {
		memset(buf, 0, sizeof(buf));
		for (ofs_idx = 0 ; ofs_idx < offset_arr_size ; ofs_idx++) {
			phy_addr = apupw.phy_addr[apu_pll] +
				pll_base_arr[pll_idx] +
				pll_offset_arr[ofs_idx];
			ret = snprintf(buf + strlen(buf),
					sizeof(buf) - strlen(buf),
					" 0x%08x",
					apu_readl(apupw.regs[apu_pll] +
						pll_base_arr[pll_idx] +
						pll_offset_arr[ofs_idx]));
			if (ret <= 0)
				break;
		}

		if (ret <= 0)
			break;
		pr_info("%s pll_base:0x%08x = %s\n", __func__,
				apupw.phy_addr[apu_pll] + pll_base_arr[pll_idx],
				buf);
	}
}
#endif

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	dev_info(dev, "%s before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* Change used RV SMC call to wake up RPC */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL, 1);

	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);

	if (ret) {
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);
		/* show powerack info */
		dev_info(dev, "%s RCX APU_RPC_PWR_ACK 0x%x = 0x%x\n",
					 __func__,
					 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_PWR_ACK),
					 readl(apupw.regs[apu_rpc] + APU_RPC_PWR_ACK));
		goto out;
	}

	dev_info(dev, "%s after wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			     __func__,
			     (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			     readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_STATUS_1),
			val, (val & (0x1 << 13)), 50, 10000);

	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		/* show powerack info */
		dev_info(dev, "%s RCX APU_RPC_PWR_ACK 0x%x = 0x%x\n",
					 __func__,
					 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_PWR_ACK),
					 readl(apupw.regs[apu_rpc] + APU_RPC_PWR_ACK));
		goto out;
	}

	/* clear vcore/rcx cgs */
	apu_writel(0xFFFFFFFF, apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_rcx] + APU_RCX_CG_CLR);

out:
	return ret;
}

static int mt6991_apu_top_on(struct device *dev)
{
	int ret = 0;

	if (apupw.env < MP)
		return 0;

	if (log_lvl)
		pr_info("%s +\n", __func__);
	// acquire hw sema
#if APU_HW_SEMA_CTRL
	apu_hw_sema_ctl(dev, APU_HW_SEMA_PWR_CTL, SYS_APMCU, 1, -1);
#endif

	ret = __apu_wake_rpc_rcx(dev);

	if (ret) {
		pr_info("%s fail to wakeup RPC, ret %d\n", __func__, ret);
		aputop_dump_pwr_reg(dev);
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_pll_data();
#endif
		if (ret == -EIO)
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_RPC_CFG_ERR");
		else
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_WAKEUP_FAIL");
		return -1;
	}

	dev_info(dev, "%s RCX before Spare RG 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rcx] + 0x110),
			readl(apupw.regs[apu_rcx] + 0x110));

	apu_writel(0x12345678, apupw.regs[apu_rcx] + 0x110);

	dev_info(dev, "%s RCX after Spare RG 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rcx] + 0x110),
			readl(apupw.regs[apu_rcx] + 0x110));

	if (log_lvl)
		pr_info("%s -\n", __func__);

	return 0;
}

#if APMCU_REQ_RPC_SLEEP
// backup solution : send request for RPC sleep from APMCU
static int __apu_sleep_rpc_rcx(struct device *dev)
{
	// REG_WAKEUP_CLR
	pr_info("%s step1. set REG_WAKEUP_CLR\n", __func__);
	apu_writel(0x1 << 12, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(10);
	// mask RPC IRQ and bypass WFI
	pr_info("%s step2. mask RPC IRQ and bypass WFI\n", __func__);
	apu_setl(1 << 7, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	udelay(10);
	pr_info("%s step3. raise up sleep request.\n", __func__);
	apu_writel(1, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(100);
	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	return 0;
}
#endif

static int mt6991_apu_top_off(struct device *dev)
{
	int ret = 0, val = 0;
	int rpc_timeout_val = 500000; // 500 ms

	if (apupw.env < MP)
		return 0;

	if (log_lvl)
		pr_info("%s +\n", __func__);
#if APMCU_REQ_RPC_SLEEP
	__apu_sleep_rpc_rcx(dev);
#endif
	// blocking until sleep success or timeout, delay 50 us per round
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL) == 0x0, 50, rpc_timeout_val);

	if (ret) {
		pr_info("%s polling PWR RDY timeout\n", __func__);
	} else {
		ret = readl_relaxed_poll_timeout_atomic(
				(apupw.regs[apu_rpc] + APU_RPC_STATUS),
				val, (val & 0x1UL) == 0x1, 50, 10000);
		if (ret)
			pr_info("%s polling PWR STATUS timeout\n", __func__);
	}

	if (ret) {
		pr_info(
		"%s timeout to wait RPC sleep (val:%d), ret %d\n", __func__, rpc_timeout_val, ret);
		aputop_dump_pwr_reg(dev);
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_pll_data();
#endif
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_SLEEP_TIMEOUT");
		return -1;
	}
	// release hw sema
#if APU_HW_SEMA_CTRL
	apu_hw_sema_ctl(dev, APU_HW_SEMA_PWR_CTL, SYS_APMCU, 0, -1);
#endif
	if (log_lvl)
		pr_info("%s -\n", __func__);

	return 0;
}

static void release_smmu_hw_sema(void)
{
	void *mbox0 = ioremap(0x4c200000, 0x100);

	pr_info("%s mbox0 status: 0x%08x\n", __func__, apu_readl(mbox0 + 0xa8));
	apu_writel(0x2, mbox0 + 0xA0); // sema1 bit 1 for release
	pr_info("%s mbox0 status: 0x%08x\n", __func__, apu_readl(mbox0 + 0xa8));
	iounmap(mbox0);
}

static int mt6991_apu_top_pb(struct platform_device *pdev)
{
	int ret = 0, val = 0;

	pr_info("%s +%d\n", __func__, apupw.env);
	init_reg_base(pdev);
	if (apupw.env < MP)
		ret = mt6991_all_on(pdev, &apupw);

	mt6991_apu_top_on(&pdev->dev);

	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (!ret) {
		/* release hw sema before smmu driver init */
		pr_info("%s release hw sema before smmu driver init\n", __func__);
		release_smmu_hw_sema();
	}

	mt6991_init_remote_data_sync(apupw.regs[apu_md32_mbox]);

	return ret;
}

static int mt6991_apu_top_rm(struct platform_device *pdev)
{
	int idx;

	pr_info("%s +\n", __func__);
	if (apupw.env < MP)
		mt6991_all_off(pdev);
	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);
	pr_info("%s -\n", __func__);

	return 0;
}

static int mt6991_apu_top_suspend(struct device *dev)
{
	return 0;
}

static int mt6991_apu_top_resume(struct device *dev)
{
	return 0;
}

static int mt6991_apu_top_func_return_val(int func_id, char *buf)
{
	ulong reg = 0;
	int i = 0, length = 0;
	int dump_size = MBRAIN_DUMP_SIZE;

	if (func_id == APUTOP_FUNC_GET_UP_DATA) {
		return snprintf(buf, 64,
			"func_id:%d, aputop_func_return_val:0x%08x\n",
			func_id, mbox_data);
	} else if (func_id == APUTOP_FUNC_GET_MBRAIN_DATA) {
		reg = (ulong)apupw.regs[apu_are] + MBRAIN_RCX_CNT;
		if (apu_readl((void __iomem *)reg) > 0) {
			reg = (ulong)apupw.regs[apu_are] + MBRAIN_RCX_DUMPMNOCPLL_REG;
			for (i = 0; i < dump_size; i++, reg += 4) {
				length += snprintf(buf + length,
				PAGE_SIZE - length,
				"%08x ",
				apu_readl((void __iomem *)reg));
				pr_info("%s phys 0x%08lx, val:0x%08x\n",
				__func__, reg, apu_readl((void __iomem *)reg));
			}
			reg = (ulong)apupw.regs[apu_are] + MBRAIN_RCX_DUMPUPPLL_REG;
			for (i = 0; i < dump_size; i++, reg += 4) {
				length += snprintf(buf + length,
				PAGE_SIZE - length,
				"%08x ",
				apu_readl((void __iomem *)reg));
				pr_info("%s phys 0x%08lx, val:0x%08x\n",
				__func__, reg, apu_readl((void __iomem *)reg));
			}
		}
		reg = (ulong)apupw.regs[apu_are] + MBRAIN_DVFS_CNT;
		if (apu_readl((void __iomem *)reg) > 0) {
			reg = (ulong)apupw.regs[apu_are] + MBRAIN_DVFS_DUMPMNOCPLL_REG;
			for (i = 0; i < dump_size; i++, reg += 4) {
				length += snprintf(buf + length,
				PAGE_SIZE - length,
				"%08x ",
				apu_readl((void __iomem *)reg));
				pr_info("%s phys 0x%08lx, val:0x%08x\n",
				__func__, reg, apu_readl((void __iomem *)reg));
			}
			reg = (ulong)apupw.regs[apu_are] + MBRAIN_DVFS_DUMPUPPLL_REG;
			for (i = 0; i < dump_size; i++, reg += 4) {
				length += snprintf(buf + length,
				PAGE_SIZE - length,
				"%08x ",
				apu_readl((void __iomem *)reg));
				pr_info("%s phys 0x%08lx, val:0x%08x\n",
				__func__, reg, apu_readl((void __iomem *)reg));
			}
		}
		buf[length] = '\0';
	} else {
		pr_info("%s func_id %d, NOT supported\n", __func__, func_id);
	}

	return length;
}

static int mt6991_apu_top_func(struct platform_device *pdev,
		enum aputop_func_id func_id, struct aputop_func_param *aputop)
{
	pr_info("%s func_id : %d\n", __func__, aputop->func_id);

	switch (aputop->func_id) {
	case APUTOP_FUNC_PWR_OFF:
		break;
	case APUTOP_FUNC_PWR_ON:
		break;
	case APUTOP_FUNC_OPP_LIMIT_HAL:
		mt6991_aputop_opp_limit(aputop, OPP_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		mt6991_aputop_opp_limit(aputop, OPP_LIMIT_DEBUG);
		break;
	case APUTOP_FUNC_DUMP_REG:
		aputop_dump_pwr_reg(&pdev->dev);
		break;
	case APUTOP_FUNC_DRV_CFG:
		mt6991_drv_cfg_remote_sync(aputop);
		break;
	case APUTOP_FUNC_IPI_TEST:
		test_ipi_wakeup_apu();
		break;
	case APUTOP_FUNC_ARE_DUMP1:
	case APUTOP_FUNC_ARE_DUMP2:
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_reg(apu_are, 0x0, 0x4000);
		aputop_dump_reg(apu_vcore, 0x3000, 0x10);
#endif
		break;
	case APUTOP_FUNC_BOOT_HOST:
		return plat_apu_boot_host();
	case APUTOP_FUNC_GET_UP_DATA:
		plat_get_up_drv_data(aputop);
		break;
	default:
		pr_info("%s invalid func_id : %d\n", __func__, aputop->func_id);
		return -EINVAL;
	}

	return 0;
}

/* call by mt6991_pwr_func.c */
void mt6991_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump)
{
	uint32_t status1 = 0x0;
	uint32_t status2 = 0x0;
	uint32_t status3 = 0x0;

	if (id == D_ACX) { //[Fix me] This is RCX_DLA status
		status1 = apu_readl(apupw.regs[apu_rpctop_mdla]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_rcx_dla]
				+ APU_RCX_MDLA0_CG_CON);
		pr_info("%s D_ACX RPC_PWR_RDY:0x%08x APU_RCX_MDLA0_CG_CON:0x%08x\n",
			__func__, status1, status2);
	} else if (id == ACX0) {
		status1 = apu_readl(apupw.regs[apu_acx0_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx0]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s ACX0 RPC_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);
	} else if (id == ACX1) {
		status1 = apu_readl(apupw.regs[apu_acx1_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx1]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s ACX1 RPC_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);

	} else if (id == ACX2) {
		status1 = apu_readl(apupw.regs[apu_acx2_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx2]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s ACX2 RPC_PWR_RDY:0x%08x APU_NCX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);
	} else {
		status1 = apu_readl(apupw.regs[apu_rpc]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_vcore]
				+ APUSYS_VCORE_CG_CON);
		status3 = apu_readl(apupw.regs[apu_rcx]
				+ APU_RCX_CG_CON);
		pr_info("%s RCX RPC_PWR_RDY:0x%08x VCORE_CG_CON:0x%08x RCX_CG_CON:0x%08x\n",
			__func__, status1, status2, status3);
		/*
		 * print_hex_dump(KERN_ERR, "rpc: ", DUMP_PREFIX_OFFSET,
		 *              16, 4, apupw.regs[apu_rpc], 0x100, 1);
		 */
	}

	if (!IS_ERR_OR_NULL(dump)) {
		dump->rpc_reg_status = status1;
		dump->conn_reg_status = status2;
		if (id == RCX)
			dump->vcore_reg_status = status3;
	}

}

const struct apupwr_plat_data mt6991_plat_data = {
	.plat_name = "mt6991_apupwr",
	.plat_aputop_on = mt6991_apu_top_on,
	.plat_aputop_off = mt6991_apu_top_off,
	.plat_aputop_pb = mt6991_apu_top_pb,
	.plat_aputop_rm = mt6991_apu_top_rm,
	.plat_aputop_suspend = mt6991_apu_top_suspend,
	.plat_aputop_resume = mt6991_apu_top_resume,
	.plat_aputop_func = mt6991_apu_top_func,
	.plat_aputop_func_return_val = mt6991_apu_top_func_return_val,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.plat_aputop_dbg_open = mt6991_apu_top_dbg_open,
	.plat_aputop_dbg_write = mt6991_apu_top_dbg_write,
#endif
	.plat_rpmsg_callback = mt6991_apu_top_rpmsg_cb,
	.bypass_pwr_on = 0,
	.bypass_pwr_off = 0,
};
