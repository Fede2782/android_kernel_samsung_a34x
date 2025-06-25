// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include "cmdq-util.h"
#include "mtk-smi-dbg.h"
#include "tinysys-scmi.h"
#include "vcp_status.h"
#include "clk-mtk.h"
#include "mtk-pd-chk.h"
#include "mtk_iommu.h"
#include "mtk-iommu-util.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
#include <soc/mediatek/smi.h>
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <linux/soc/mediatek/devapc_public.h>
#endif

#include "mtk-mminfra-debug.h"
#include "mtk-hw-semaphore.h"

#define MMINFRA_MAX_CLK_NUM	(4)
#define MAX_SMI_COMM_NUM	(3)

#define MMINFRA_GALS_NR		(10)

#define MMPC_RSC_USER_NR	(2)

struct mminfra_dbg {
	void __iomem *ctrl_base;
	void __iomem *mminfra_base;
	void __iomem *mminfra_ao_base;
	void __iomem *mminfra_mtcmos_base;
	void __iomem *gce_base;
	void __iomem *vlp_base;
	void __iomem *spm_base;
	void __iomem *vcp_gipc_in_set;
	void __iomem *mm_cfg_base[MM_PWR_NR];
	void __iomem *mminfra_devapc_vio_sta;
	struct mtk_mminfra_pwr_ctrl *mmu_mminfra_pwr_ctrl;
	ssize_t ctrl_size;
	struct device *comm_dev[MAX_SMI_COMM_NUM];
	struct notifier_block nb;
	u32 gals_sel[MMINFRA_GALS_NR];
	u32 dbg_sel_out_nr[MM_PWR_NR];
	u32 mmpc_rsc_user[MMPC_RSC_USER_NR];
	u32 mm_voter_base;
	u32 mm_mtcmos_base;
	u32 mm_mtcmos_mask;
	u32 mm_devapc_vio_sta_base;
	u32 vcp_gipc_in_set_base;
	u32 mmpc_src_ctrl_base;
	u32 mm_proc_mtcmos_base;
	bool irq_safe;
};

static struct notifier_block mtk_pd_notifier;
static struct scmi_tinysys_info_st *tinfo;
static int feature_id;
static struct clk *mminfra_clk[MMINFRA_MAX_CLK_NUM];
static atomic_t clk_ref_cnt = ATOMIC_INIT(0);
static atomic_t vcp_ref_cnt = ATOMIC_INIT(0);
static struct device *dev;
static struct mminfra_dbg *dbg;
#if IS_ENABLED(CONFIG_MTK_MMINFRA_DEBUG)
static struct task_struct *mminfra_power_mon_thread;
#endif
static u32 mminfra_bkrs;
static u32 bkrs_reg_pa;
static u32 mm_pwr_ver;
static bool mminfra_ao_base;
static bool vcp_gipc;
static bool no_sleep_pd_cb;
static bool skip_apsrc;
static bool has_infra_hfrp;
static bool is_mminfra_shutdown;
static bool mm_no_cg_ctrl;
static bool mm_no_scmi;
static bool mmpc_src_ctrl;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static bool mm_dapc_pwr_on;
spinlock_t mm_dapc_lock;
#endif

#define MMINFRA_BASE		0x1e800000
#define MMINFRA_AO_BASE		0x1e8ff000
#define GCE_BASE		0x1e980000
#define VCP_GIPC_IN_SET		0x1ec24098

#define MMINFRA_CG_CON0		0x100
#define MMINFRA_DBG_SEL		0x300
#define MMINFRA_MODULE_DBG	0xf4
#define	GCE_GCTL_VALUE		0x48
#define GCED_CG_BIT			BIT(0)
#define GCEM_CG_BIT			BIT(1)
#define SMI_CG_BIT			BIT(2)

#define MMINFRA_CG_CON1		0x110
#define GCE26M_CG_BIT		BIT(17)

#define GCED				0
#define GCEM				1

#define MM_INFRA_AO_PDN_SRAM_CON	(0xf14)
#define MM_INFRA_SLEEP_SRAM_PDN		BIT(0)
#define MM_INFRA_SLEEP_SRAM_PDN_ACK	BIT(8)

#define GIPC4_SETCLR_BIT_0	(1U << 16)
#define GIPC4_SETCLR_BIT_1	(1U << 17)
#define GIPC4_SETCLR_BIT_2	(1U << 18)
#define GIPC4_SETCLR_BIT_3	(1U << 19)

#define	MM_SYS_RESUME		GIPC4_SETCLR_BIT_0
#define	MM_SYS_SUSPEND		GIPC4_SETCLR_BIT_1
#define	MM_INFRA_OFF		GIPC4_SETCLR_BIT_2
#define	MM_INFRA_LOG		GIPC4_SETCLR_BIT_3

#define MM_MON_CNT		(1000)

#define CLK_MMINFRA_PWR_VOTE_BIT_MMINFRA	(27)

u32 mm_pwr_cnt[MM_PWR_NR];
u32 voter_cnt[32] = {0};

static int is_mminfra_ready(struct mminfra_hw_voter *hw_voter)
{
	if (!hw_voter)
		return 0;

	return (readl(hw_voter->vlp_base + hw_voter->done_bits_ofs) & MMINFRA_DONE) &&
		(readl(hw_voter->vlp_base + hw_voter->done_bits_ofs) & VCP_READY);
}

static int mminfra_hwv_power_ctrl(struct mminfra_hw_voter *hw_voter, bool is_on)
{
	uint32_t vote_ofs, vote_mask, vote_ack;
	uint32_t val = 0, cnt;

	vote_mask = BIT(hw_voter->en_shift);
	vote_ofs = is_on ? hw_voter->set_ofs : hw_voter->clr_ofs;
	vote_ack = is_on ? vote_mask : 0x0;

	/* vote on off */
	cnt = 0;
	do {
		writel(vote_mask, hw_voter->vlp_base + vote_ofs);
		udelay(MTK_POLL_HWV_VOTE_US);
		val = readl(hw_voter->vlp_base + hw_voter->en_ofs);
		if ((val & vote_mask) == vote_ack)
			break;
		if (cnt > MTK_POLL_HWV_VOTE_CNT) {
			pr_notice("%s vote mminfra timeout, is_on:%d, 0x%x=0x%x\n",
				__func__, is_on, hw_voter->vlp_base_pa + hw_voter->en_ofs, val);
			return -1;
		}
		cnt++;
	} while (1);

	/* confirm done bits */
	cnt = 0;
	while (is_on) {
		if (is_mminfra_ready(hw_voter))
			break;
		if (cnt > MTK_POLL_DONE_TIMEOUT) {
			pr_notice("%s polling mminfra done timeout, 0x%x=0x%x\n",
				__func__, hw_voter->vlp_base_pa + hw_voter->done_bits_ofs, val);
			return -1;
		}
		cnt++;
		udelay(MTK_POLL_DONE_DELAY_US);
	}

	return 0;
}

static int mtk_mminfra_get_if_in_use(void)
{
	int ret, is_on = 0;
	unsigned long flags;
	struct mtk_mminfra_pwr_ctrl *pwr_ctrl = dbg->mmu_mminfra_pwr_ctrl;

	if (!pwr_ctrl || !pwr_ctrl->hw_voter.get_if_in_use_ena)
		return is_on;

	spin_lock_irqsave(&pwr_ctrl->lock, flags);
	if (atomic_read(&pwr_ctrl->ref_cnt) > 0) {
		atomic_inc(&pwr_ctrl->ref_cnt);
		is_on = 1;
		goto out;
	}

	ret = mtk_hw_semaphore_ctrl(MASTER_SMMU, true);
	if (ret < 0)
		goto err;
	/* check if mminfra is in use */
	if (is_mminfra_ready(&pwr_ctrl->hw_voter)) {
		ret = mminfra_hwv_power_ctrl(&pwr_ctrl->hw_voter, true);
		if (ret < 0) {
			pr_notice("%s vote for mminfra fail, ret=%d\n", __func__, ret);
			goto err;
		}
		atomic_inc(&pwr_ctrl->ref_cnt);
		is_on = 1;
	} else
		is_on = 0;

	ret = mtk_hw_semaphore_ctrl(MASTER_SMMU, false);
	if (ret < 0)
		goto err;
out:
	spin_unlock_irqrestore(&pwr_ctrl->lock, flags);
	return is_on;
err:
	spin_unlock_irqrestore(&pwr_ctrl->lock, flags);
	return ret;
}

static int mtk_mminfra_put(void)
{
	struct mtk_mminfra_pwr_ctrl *pwr_ctrl = dbg->mmu_mminfra_pwr_ctrl;
	unsigned long flags;

	spin_lock_irqsave(&pwr_ctrl->lock, flags);
	if (atomic_dec_return(&pwr_ctrl->ref_cnt) > 0)
		goto out;

	mminfra_hwv_power_ctrl(&pwr_ctrl->hw_voter, false);
out:
	spin_unlock_irqrestore(&pwr_ctrl->lock, flags);
	return 0;
}

static bool mminfra_check_scmi_status(void)
{
	if (!mm_no_scmi) {
		if (tinfo)
			return true;

		tinfo = get_scmi_tinysys_info();

		if (IS_ERR_OR_NULL(tinfo)) {
			pr_notice("%s: tinfo is wrong!!\n", __func__);
			tinfo = NULL;
			return false;
		}

		if (IS_ERR_OR_NULL(tinfo->ph)) {
			pr_notice("%s: tinfo->ph is wrong!!\n", __func__);
			tinfo = NULL;
			return false;
		}

		of_property_read_u32(tinfo->sdev->dev.of_node, "scmi-mminfra", &feature_id);
		pr_notice("%s: get scmi_smi succeed id=%d!!\n", __func__, feature_id);
		return true;
	}
	pr_notice("%s: not support mminfra scmi\n", __func__);
	return false;
}

static void do_mminfra_bkrs(bool is_restore)
{
	int err;
	u64 start_ts, start_osts, end_ts, end_osts;

	if (mminfra_check_scmi_status()) {
		start_ts = sched_clock();
		start_osts = __arch_counter_get_cntvct();

		err = scmi_tinysys_common_set(tinfo->ph, feature_id,
				2, (is_restore)?0:1, 0, 0, 0);

		if (err) {
			end_ts = sched_clock();
			end_osts = __arch_counter_get_cntvct();
			pr_notice("%s: call scmi(%d) err=%d osts:%llu ts:%llu\n",
				__func__, is_restore, err, start_osts, start_ts);
			if (err == -ETIMEDOUT) {
				pr_notice("%s: call scmi(%d) timeout osts:%llu ts:%llu\n",
					__func__, is_restore, end_osts, end_ts);
				mdelay(3);
			}
		}
	}
}

static void mminfra_clk_set(bool is_enable)
{
	int err = 0;
	int i, j;

	if (is_enable) {
		for (i = 0; i < MMINFRA_MAX_CLK_NUM; i++) {
			if (mminfra_clk[i])
				err = clk_prepare_enable(mminfra_clk[i]);
			else
				break;

			if (err) {
				pr_notice("mminfra clk(%d) enable fail:%d\n", i, err);
				for (j = i - 1; j >= 0; j--)
					clk_disable_unprepare(mminfra_clk[j]);
				return;
			}
		}
	} else {
		for (i = MMINFRA_MAX_CLK_NUM - 1; i >= 0; i--) {
			if (mminfra_clk[i])
				clk_disable_unprepare(mminfra_clk[i]);
			else
				pr_notice("mminfra clk(%d) null\n", i);
		}
	}
}

static bool is_mminfra_power_on(void)
{
	if (mm_pwr_ver <= mm_pwr_v2) {
		if (dbg->spm_base && dbg->mm_mtcmos_mask && dbg->vlp_base) {
			if ((readl(dbg->spm_base+0xea8) & dbg->mm_mtcmos_mask)
				!= dbg->mm_mtcmos_mask) {
				pr_notice("mminfra mtcmos = 0x%x, done bits=0x%x\n",
					readl(dbg->spm_base+0xea8), readl(dbg->vlp_base+0x91c));
			}
		}

		return (atomic_read(&clk_ref_cnt) > 0);
	} else if (mm_pwr_ver == mm_pwr_v3) {
		return (atomic_read(&clk_ref_cnt) > 0);
	} else
		return true;
}

static bool is_gce_cg_on(u32 hw_id)
{
	u32 con0_val;

	if (!mm_no_cg_ctrl) {
		if (mminfra_ao_base)
			con0_val = readl_relaxed(dbg->mminfra_ao_base + MMINFRA_CG_CON0);
		else
			con0_val = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON0);

		if (con0_val & (hw_id == GCED ? GCED_CG_BIT : GCEM_CG_BIT))
			return false;
	}
	return true;
}

static void mminfra_cg_check(bool on)
{
	u32 con0_val;
	u32 con0_val_gce;
	u32 con1_val_gce;

	if (!mm_no_cg_ctrl) {
		con0_val = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON0);

		if (mminfra_ao_base) {
			con0_val_gce = readl_relaxed(dbg->mminfra_ao_base + MMINFRA_CG_CON0);
			con1_val_gce = readl_relaxed(dbg->mminfra_ao_base + MMINFRA_CG_CON1);
		} else {
			con0_val_gce = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON0);
			con1_val_gce = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON1);
		}

		if (on) {
			/* SMI CG still off */
			if ((con0_val & (SMI_CG_BIT)) || (con0_val_gce & GCEM_CG_BIT) ||
				(con0_val_gce & GCED_CG_BIT) || (con1_val_gce & GCE26M_CG_BIT)) {
				pr_notice("%s cg still off, CG_CON0:0x%x CG_CON0:0x%x CG_CON1:0x%x\n",
							__func__, con0_val, con0_val_gce, con1_val_gce);
				if (con0_val & (SMI_CG_BIT))
					mtk_smi_dbg_cg_status();
				if ((con0_val_gce & GCEM_CG_BIT) || (con0_val_gce & GCED_CG_BIT)
					|| (con1_val_gce & GCE26M_CG_BIT))
					cmdq_dump_usage();
			}
		} else {
			/* SMI CG still on */
			if (!(con0_val & (SMI_CG_BIT)) || !(con0_val_gce & GCEM_CG_BIT)
				|| !(con0_val_gce & GCED_CG_BIT) || !(con1_val_gce & GCE26M_CG_BIT)) {
				pr_notice("%s Scg still on, CG_CON0:0x%x CG_CON0:0x%x CG_CON1:0x%x\n",
							__func__, con0_val, con0_val_gce, con1_val_gce);
				if (!(con0_val & (SMI_CG_BIT)))
					mtk_smi_dbg_cg_status();
				if (!(con0_val_gce & GCEM_CG_BIT) || !(con0_val_gce & GCED_CG_BIT)
					|| !(con1_val_gce & GCE26M_CG_BIT))
					cmdq_dump_usage();
			}
		}
	}
}

static void mtk_mminfra_gce_sram_en(void)
{
	if (!dbg->spm_base) {
		pr_notice("%s: not support\n", __func__);
		return;
	}

	writel(readl(dbg->spm_base + MM_INFRA_AO_PDN_SRAM_CON) & (~MM_INFRA_SLEEP_SRAM_PDN),
			dbg->spm_base + MM_INFRA_AO_PDN_SRAM_CON);
	while (readl(dbg->spm_base + MM_INFRA_AO_PDN_SRAM_CON) & MM_INFRA_SLEEP_SRAM_PDN_ACK)
		;
}

static int mtk_mminfra_pd_callback(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	int count;
	void __iomem *test_base;
	static u32 bk_val;
	u32 val;

	if (flags == GENPD_NOTIFY_ON) {
		count = atomic_inc_return(&clk_ref_cnt);
		if (!no_sleep_pd_cb) {
			mminfra_clk_set(true);
			mminfra_cg_check(true);
			if (mminfra_bkrs) {
				/* avoid suspend/resume fail when mminfra debug
				 * is also initialized in sspm
				 */
				cmdq_util_mminfra_cmd(0);
				cmdq_util_mminfra_cmd(3); //mminfra rfifo init
				do_mminfra_bkrs(true);
			}
			cmdq_util_mmuen_devapc_disable();
			test_base = ioremap(bkrs_reg_pa, 4);
			val = readl_relaxed(test_base);
			if (val != bk_val) {
				pr_notice("%s: HRE restore failed %#x=%x\n",
					__func__, bkrs_reg_pa, val);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
				aee_kernel_warning("mminfra",
					"HRE restore failed %#x=%x, bk_val=%x\n",
					bkrs_reg_pa, val, bk_val);
#endif
				BUG_ON(1);
			}
			iounmap(test_base);
			if (!skip_apsrc) {
				writel(0x20002, dbg->gce_base + GCE_GCTL_VALUE);
				pr_notice("%s: enable gce apsrc: %#x=%#x\n",
					__func__, GCE_BASE + GCE_GCTL_VALUE,
					readl(dbg->gce_base + GCE_GCTL_VALUE));
			}
			mtk_mminfra_gce_sram_en();
		}
		if (mm_pwr_ver <= mm_pwr_v2) {
			if (dbg->spm_base && dbg->mm_mtcmos_mask && dbg->vlp_base) {
				if ((readl(dbg->spm_base+0xea8) & dbg->mm_mtcmos_mask)
					!= dbg->mm_mtcmos_mask) {
					pr_notice("mminfra mtcmos = 0x%x, done bits=0x%x\n",
						readl(dbg->spm_base+0xea8),
						readl(dbg->vlp_base+0x91c));
				}
			}
		}
		if (!no_sleep_pd_cb)
			pr_notice("%s: enable clk ref_cnt=%d\n", __func__, count);
	} else if (flags == GENPD_NOTIFY_PRE_OFF) {
		if (!no_sleep_pd_cb) {
			if (!skip_apsrc) {
				writel(0, dbg->gce_base + GCE_GCTL_VALUE);
				pr_notice("%s: disable gce apsrc: %#x=%#x\n",
					__func__, GCE_BASE + GCE_GCTL_VALUE,
					readl(dbg->gce_base + GCE_GCTL_VALUE));
			}
			test_base = ioremap(bkrs_reg_pa, 4);
			bk_val = readl_relaxed(test_base);
			if (mminfra_bkrs)
				do_mminfra_bkrs(false);
			iounmap(test_base);
			count = atomic_read(&clk_ref_cnt);
			if (count != 1) {
				pr_notice("%s: wrong clk ref_cnt=%d in PRE_OFF\n",
					__func__, count);
				return NOTIFY_OK;
			}
			mminfra_clk_set(false);
			mminfra_cg_check(false);
		}
		count = atomic_dec_return(&clk_ref_cnt);
		if (!no_sleep_pd_cb)
			pr_notice("%s: disable clk ref_cnt=%d\n", __func__, count);
	}

	return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_MTK_MMINFRA_DEBUG)

static int mminfra_voter_mon(void *data)
{
	void __iomem *voter_addr;
	u32 val;
	u32 bit;
	u32 mask;
	u32 cnt = 0;

	if (!dbg || !dbg->mm_mtcmos_base || !dbg->mm_mtcmos_mask) {
		pr_notice("%s skip\n", __func__);
		return 0;
	}

	if (!has_infra_hfrp) {
		pr_notice("%s set mm pwr on\n", __func__);
		pm_runtime_get(dbg->comm_dev[0]);
	}
	voter_addr = ioremap(dbg->mm_voter_base, 0x4);
	while (!kthread_should_stop()) {
		val = readl(voter_addr);
		for(bit = 0; bit < 32; bit++) {
			mask = (1<<bit);
			if ((val & mask) == mask)
				voter_cnt[bit]++;
		}
		cnt++;
		if (cnt == MM_MON_CNT) {
			pr_notice("%s ++++++++++, MM_MON_CNT = %u\n", __func__, MM_MON_CNT);
			for(bit = 0; bit < 32; bit+=4)
				pr_notice("bit=%u~%u,[%u][%u][%u][%u]\n", bit, bit+3,
					voter_cnt[bit],voter_cnt[bit+1],
					voter_cnt[bit+2],voter_cnt[bit+3]);
			pr_notice("%s ----------\n", __func__);
			for(bit = 0; bit < 32; bit++)
				voter_cnt[bit] = 0;
			cnt = 0;
		}
		mdelay(1);
	}
	return 0 ;
}

static int mminfra_power_mon(void *data)
{
	void __iomem *mtcmos_addr;
	void __iomem *mmpc_src_addr;
	void __iomem *mmpc_hw_req_addr[MMPC_NR];
	u32 val;
	u32 cnt = 0;

	if (mm_pwr_ver <= mm_pwr_v2) {
		if (!dbg || !dbg->mm_mtcmos_base || !dbg->mm_mtcmos_mask) {
			pr_notice("%s skip\n", __func__);
			return 0;
		}

		mtcmos_addr = ioremap(dbg->mm_mtcmos_base, 0x4);
		while (!kthread_should_stop()) {
			val = readl(mtcmos_addr);
			if ((val & dbg->mm_mtcmos_mask) == dbg->mm_mtcmos_mask)
				mm_pwr_cnt[MM_0]++;//pwr on;
			cnt++;
			if (cnt == MM_MON_CNT) {
				pr_notice("%s mminfra power_on ratio[%u/%u]\n",
					__func__, mm_pwr_cnt[MM_0], MM_MON_CNT);
				mm_pwr_cnt[MM_0] = 0;
				cnt = 0;
			}
			mdelay(1);
		}
	} else if (mm_pwr_ver == mm_pwr_v3) {
		if (!dbg || !dbg->mm_mtcmos_base || !dbg->mm_mtcmos_mask ||
			!dbg->mmpc_src_ctrl_base) {
			pr_notice("%s skip\n", __func__);
			return 0;
		}

		mmpc_src_addr = ioremap(dbg->mmpc_src_ctrl_base, 0x1000);
		mmpc_hw_req_addr[MM_DDRSRC] = mmpc_src_addr + 0x1C;
		mmpc_hw_req_addr[MM_EMI] = mmpc_src_addr + 0x3C;
		mmpc_hw_req_addr[MM_BUSPLL] = mmpc_src_addr + 0x5C;
		mmpc_hw_req_addr[MM_INFRA] = mmpc_src_addr + 0x7C;
		mmpc_hw_req_addr[MM_CK26M] = mmpc_src_addr + 0x9C;
		mmpc_hw_req_addr[MM_PMIC] = mmpc_src_addr + 0xBC;
		mmpc_hw_req_addr[MM_VCORE] = mmpc_src_addr + 0xDC;
		while (!kthread_should_stop()) {
			val = readl(dbg->mminfra_mtcmos_base);
			if ((val & dbg->mm_mtcmos_mask) == dbg->mm_mtcmos_mask)
				mm_pwr_cnt[MM_0]++;//pwr on;

			val = readl(dbg->mminfra_mtcmos_base + 0x4);
			if ((val & dbg->mm_mtcmos_mask) == dbg->mm_mtcmos_mask)
				mm_pwr_cnt[MM_1]++;//pwr on;

			val = readl(dbg->mminfra_mtcmos_base + 0x8);
			if ((val & dbg->mm_mtcmos_mask) == dbg->mm_mtcmos_mask)
				mm_pwr_cnt[MM_AO]++;//pwr on;

			cnt++;
			if (cnt == MM_MON_CNT) {
				pr_notice("%s mminfra power_on ratio: MM_0[%u/%u], MM_1[%u/%u], MM_AO[%u/%u]\n",
					__func__,
					mm_pwr_cnt[MM_0], MM_MON_CNT,
					mm_pwr_cnt[MM_1], MM_MON_CNT,
					mm_pwr_cnt[MM_AO], MM_MON_CNT);
				mm_pwr_cnt[MM_0] = 0;
				mm_pwr_cnt[MM_1] = 0;
				mm_pwr_cnt[MM_AO] = 0;
				cnt = 0;
				pr_notice("%s MM_DDRSRC[0x%x] MM_EMI[0x%x] MM_BUSPLL[0x%x].\n",
					__func__,
					readl(mmpc_hw_req_addr[MM_DDRSRC]),
					readl(mmpc_hw_req_addr[MM_EMI]),
					readl(mmpc_hw_req_addr[MM_BUSPLL]));
				pr_notice("%s MM_INFRA[0x%x] MM_CK26M[0x%x] MM_PMIC[0x%x].\n",
					__func__,
					readl(mmpc_hw_req_addr[MM_INFRA]),
					readl(mmpc_hw_req_addr[MM_CK26M]),
					readl(mmpc_hw_req_addr[MM_PMIC]));
				pr_notice("%s MM_VCORE[0x%x].\n",
					__func__,
					readl(mmpc_hw_req_addr[MM_VCORE]));
			}
			mdelay(1);
		}
		iounmap(mmpc_src_addr);
	} else
		pr_notice("%s not supported\n", __func__);

	return 0 ;
}
#endif

int mminfra_scmi_test(const char *val, const struct kernel_param *kp)
{
	int ret, arg0;
	unsigned int test_case;
	void __iomem *test_base = ioremap(0x1e800280, 4);

	ret = sscanf(val, "%u %d", &test_case, &arg0);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}
	if (mminfra_check_scmi_status()) {
		if (test_case == 2 && arg0 == 0) {
			writel(0x123, test_base);
			pr_notice("%s: before BKRS read 0x1e800280 = 0x%x\n",
				__func__, readl_relaxed(test_base));
		}
		pr_notice("%s: feature_id=%d test_case=%d arg0=%d\n",
			__func__, feature_id, test_case, arg0);
		ret = scmi_tinysys_common_set(tinfo->ph, feature_id, test_case, arg0, 0, 0, 0);
		pr_notice("%s: scmi return %d\n", __func__, ret);
		if (test_case == 2 && arg0 == 1)
			pr_notice("%s after BKRS read 0x1e800280 = 0x%x\n",
				__func__, readl_relaxed(test_base));
	}

	iounmap(test_base);

	return 0;
}

static struct kernel_param_ops scmi_test_ops = {
	.set = mminfra_scmi_test,
};
module_param_cb(scmi_test, &scmi_test_ops, NULL, 0644);
MODULE_PARM_DESC(scmi_test, "scmi test");

static int mminfra_ut(const char *val, const struct kernel_param *kp)
{
#if IS_ENABLED(CONFIG_MTK_MMINFRA_DEBUG)
	int ret, arg0;
	unsigned int test_case, value;
	void __iomem *test_base;
	struct mtk_mminfra_pwr_ctrl *pwr_ctrl = dbg->mmu_mminfra_pwr_ctrl;
	uint32_t voter_val = 0;

	ret = sscanf(val, "%u %i", &test_case, &arg0);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}
	pr_notice("%s: input: %s\n", __func__, val);
	switch (test_case) {
	case 0:
		ret = pm_runtime_get_sync(dev);
		test_base = ioremap(arg0, 4);
		value = readl_relaxed(test_base);
		do_mminfra_bkrs(false); // backup
		writel(0x123, test_base);
		do_mminfra_bkrs(true); // restore
		if (value == readl_relaxed(test_base))
			pr_notice("%s: test_case(%d) pass\n",
				__func__, test_case);
		else
			pr_notice("%s: test_case(%d) fail value=%d\n",
				__func__, test_case, value);
		iounmap(test_base);
		pm_runtime_put_sync(dev);
		break;
	case 1:
		ret = pm_runtime_get_sync(dev);
		test_base = ioremap(bkrs_reg_pa, 4);
		value = readl_relaxed(test_base);
		do_mminfra_bkrs(false); // backup
		writel(0x123, test_base);
		do_mminfra_bkrs(true); // restore
		if (value == readl_relaxed(test_base))
			pr_notice("%s: test_case(%d) pass\n",
				__func__, test_case);
		else
			pr_notice("%s: test_case(%d) fail value=%d\n",
				__func__, test_case, value);
		pr_notice("%s: HRE restore result %#x=%x value=%x\n",
			__func__, bkrs_reg_pa, readl_relaxed(test_base), value);
		iounmap(test_base);
		pm_runtime_put_sync(dev);
		break;
	case 2:
		pr_notice("%s: test_case(%d) enable mminfra_voter_mon\n", __func__, test_case);
		kthread_run(mminfra_voter_mon, NULL, "mminfra_voter_mon");
		break;
	case 3:
		pr_notice("%s: test_case(%d) arg0:%d mminfra_power_mon\n", __func__, test_case, arg0);
		if (!arg0) {
			if (!mminfra_power_mon_thread)
				mminfra_power_mon_thread = kthread_run(mminfra_power_mon, NULL,
								"mminfra_power_mon");
		} else {
			if (mminfra_power_mon_thread) {
				kthread_stop(mminfra_power_mon_thread);
				mminfra_power_mon_thread = NULL;
			}
		}
		break;
	case 4:
		ret = mtk_mminfra_get_if_in_use();
		voter_val = readl(pwr_ctrl->hw_voter.vlp_base + pwr_ctrl->hw_voter.en_ofs);
		pr_notice("%s mtk_mminfra_get_if_in_use ret:%d voter:%x\n",
			__func__, ret, voter_val);
		break;
	case 5:
		ret = mtk_mminfra_put();
		voter_val = readl(pwr_ctrl->hw_voter.vlp_base + pwr_ctrl->hw_voter.en_ofs);
		pr_notice("%s mtk_mminfra_put ret:%d voter:%x\n",
			__func__, ret, voter_val);
		break;
	default:
		pr_notice("%s: wrong test_case(%d)\n", __func__, test_case);
		break;
	}
#endif
	return 0;
}

static struct kernel_param_ops mminfra_ut_ops = {
	.set = mminfra_ut,
};
module_param_cb(mminfra_ut, &mminfra_ut_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_ut, "mminfra ut");


int mminfra_log(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int test_case;

	ret = kstrtouint(val, 0, &test_case);
	if (ret) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}

	pr_notice("%s: input: %s\n", __func__, val);
	switch (test_case) {
	case 0:
		if (mm_pwr_ver <= mm_pwr_v2) {
			if (vcp_gipc) {
				pm_runtime_get_sync(dev);
				writel(MM_INFRA_LOG, dbg->vcp_gipc_in_set);
				pr_notice("VCP_GIPC_IN_SET = 0x%x\n", readl(dbg->vcp_gipc_in_set));
				pm_runtime_put_sync(dev);
			}
		}
		break;
	default:
		pr_notice("%s: wrong test_case(%d)\n", __func__, test_case);
		break;
	}

	return 0;
}

static const struct kernel_param_ops mminfra_log_ops = {
	.set = mminfra_log,
};
module_param_cb(mminfra_log, &mminfra_log_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_log, "mminfra log");

int mminfra_dbg_ut(const char *val, const struct kernel_param *kp)
{
#if IS_ENABLED(CONFIG_MTK_MMINFRA_DEBUG)
	int ret, i;
	unsigned int test_case, arg0, arg1, value;
	void __iomem *mm_proc_mtcmos;
	void __iomem *mmpc_src_addr;
	void __iomem *addr;
	void __iomem *test_base;
	u32 *mmpc_rsc_user_num = dbg->mmpc_rsc_user;

	ret = sscanf(val, "%u %u %u", &test_case, &arg0, &arg1);
	if (ret != 3) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}

	pr_notice("%s: input: %s\n", __func__, val);
	switch (test_case) {
	case 0:
		if (mmpc_src_ctrl) {
			if (mm_pwr_ver == mm_pwr_v3) {
				mm_proc_mtcmos = ioremap(dbg->mm_proc_mtcmos_base, 0x4);
				if ((readl(mm_proc_mtcmos) & 0xC0000000) != 0xC0000000) {
					pr_notice("mmproc pwr off: 0x%x\n", readl(mm_proc_mtcmos));
				} else {
					mmpc_src_addr = ioremap(dbg->mmpc_src_ctrl_base, 0x1000);
					for (i = 0x4; i <= 0xc4; i += 0x20) {
						pr_notice("mmpc sw mode[0x%x]: 0x%x\n",
							i, readl(mmpc_src_addr+i));
					}
					for (i = 0x18; i <= 0xd8; i += 0x20) {
						pr_notice("mmpc fsm output[0x%x]: 0x%x\n",
							i, readl(mmpc_src_addr+i));
					}
					for (i = 0x1c; i <= 0xdc; i += 0x20) {
						pr_notice("mmpc hw req[0x%x]: 0x%x\n",
							i, readl(mmpc_src_addr+i));
					}
					iounmap(mmpc_src_addr);
				}
				iounmap(mm_proc_mtcmos);
			}
		}
		break;
	case 1:
	case 2:
		if (mmpc_src_ctrl) {
			if (mm_pwr_ver == mm_pwr_v3) {
				if (arg0 >= mmpc_rsc_user_num[0] || arg1 >= mmpc_rsc_user_num[1]) {
					pr_notice("%s: wrong test index(%d)(%d)(%d)\n",
						__func__, test_case, arg0, arg1);
					return -EINVAL;
				}

				mm_proc_mtcmos = ioremap(dbg->mm_proc_mtcmos_base, 0x4);
				if ((readl(mm_proc_mtcmos) & 0xC0000000) != 0xC0000000) {
					pr_notice("mmproc pwr off: 0x%x\n", readl(mm_proc_mtcmos));
				} else {
					mmpc_src_addr = ioremap(dbg->mmpc_src_ctrl_base, 0x1000);
					addr = mmpc_src_addr + (unsigned int)(arg0 << 5) + 4;
					value = (unsigned int)(1 << arg1);
					if (test_case == 1)
						writel(readl(addr) | value, addr);
					else
						writel(readl(addr) & ~value, addr);
					pr_notice("mmpc sw mode[0x%x]: 0x%x\n",
						((arg0 << 5) + 4), readl(addr));
					iounmap(mmpc_src_addr);
				}
				iounmap(mm_proc_mtcmos);
			}
		}
		break;
	case 3:
		pr_notice("%s: write test index(%d)(%d)(%d)\n", __func__, test_case, arg0, arg1);
		test_base = ioremap(arg0, 4);
		writel(arg1, test_base);
		value = readl_relaxed(test_base);
		pr_notice("%s: read %x=0x%x\n", __func__, arg0, value);
		iounmap(test_base);
		break;
	case 4:
		pr_notice("%s: read test index(%d)(%d)(%d)\n", __func__, test_case, arg0, arg1);
		test_base = ioremap(arg0, 4);
		value = readl_relaxed(test_base);
		pr_notice("%s: read %x=0x%x\n", __func__, arg0, value);
		iounmap(test_base);
		break;
	default:
		pr_notice("%s: wrong test_case(%d)\n", __func__, test_case);
		break;
	}
#endif
	return 0;
}

static const struct kernel_param_ops mminfra_dbg_ut_ops = {
	.set = mminfra_dbg_ut,
};
module_param_cb(mminfra_dbg_ut, &mminfra_dbg_ut_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_dbg_ut, "mminfra dbg ut");

static int vcp_mminfra_on(void)
{
	int count, ret;

	count = atomic_inc_return(&vcp_ref_cnt);
	ret = pm_runtime_get_sync(dev);
	pr_notice("%s: ref_cnt=%d, ret=%d\n", __func__, count, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
static int vcp_mminfra_off(void)
{
	int count, ret;

	count = atomic_dec_return(&vcp_ref_cnt);
	ret = pm_runtime_put_sync(dev);
	pr_notice("%s: ref_cnt=%d, ret=%d\n", __func__, count, ret);

	return ret;
}
#endif

static void mminfra_gals_dump_v1(void)
{
	u32 i;
	u32 *mux_setting = dbg->gals_sel;

	for (i = 0; i < MMINFRA_GALS_NR; i++) {
		if (mux_setting[i] == 0)
			continue;
		writel(mux_setting[i], dbg->mminfra_base + MMINFRA_DBG_SEL);
		pr_notice("%s: %#x=%#x, %#x=%#x\n", __func__,
			MMINFRA_BASE + MMINFRA_DBG_SEL,
			readl(dbg->mminfra_base + MMINFRA_DBG_SEL),
			MMINFRA_BASE + MMINFRA_MODULE_DBG,
			readl(dbg->mminfra_base + MMINFRA_MODULE_DBG));
	}
}

static void mminfra_gals_dump_v3(void)
{
	s32 len, ret, i, j;
	u32 dbg_sel, val;
	char buf[LINK_MAX + 1] = {0};

	for (i = 0; i < MM_PWR_NR; i++) {
		if (!dbg->mm_cfg_base[i] || !dbg->dbg_sel_out_nr[i])
			continue;
		dev_info(dev, "mm_pwr:%d gals dump: (%#x,%#x)\n", i,
						MMINFRA_DBG_SEL, MMINFRA_MODULE_DBG);
		memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
		len = 0;
		for (j = 0; j < dbg->dbg_sel_out_nr[i]; j++) {
			writel(j, dbg->mm_cfg_base[i] + MMINFRA_DBG_SEL);
			dbg_sel = readl(dbg->mm_cfg_base[i] + MMINFRA_DBG_SEL);
			val = readl(dbg->mm_cfg_base[i] + MMINFRA_MODULE_DBG);
			ret = snprintf(buf + len, LINK_MAX - len, " (%#x,%#x),", dbg_sel, val);
			if (ret < 0 || ret >= LINK_MAX - len) {
				ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
				if (ret < 0)
					pr_notice("%s: ret:%d buf size:%d\n",
						__func__, ret, LINK_MAX - len);
				dev_info(dev, "%s\n", buf);

				len = 0;
				memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
				ret = snprintf(buf + len, LINK_MAX - len, " (%#x,%#x),",
										dbg_sel, val);
				if (ret < 0)
					pr_notice("%s: ret:%d buf size:%d\n",
						__func__, ret, LINK_MAX - len);
			}
			len += ret;
		}
		ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
		if (ret < 0)
			pr_notice("%s: ret:%d buf size:%d\n", __func__, ret, LINK_MAX - len);
		dev_info(dev, "%s\n", buf);
	}
	/* TODO: snoc gals dump */
}

static void mminfra_gals_dump(void)
{
	switch (mm_pwr_ver) {
	case mm_pwr_v1:
	case mm_pwr_v2:
		mminfra_gals_dump_v1();
		break;
	case mm_pwr_v3:
		mminfra_gals_dump_v3();
		break;
	default:
		break;
	}
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
static void vcp_debug_dump(void)
{
	pr_info("cg_con0 = 0x%x\n", readl(dbg->mminfra_base + MMINFRA_CG_CON0));
	mminfra_gals_dump();
}
#endif

int mtk_mminfra_dbg_hang_detect(const char *user, bool skip_pm_runtime)
{
	s32 offset, len = 0, ret, i;
	u32 val;
	char buf[LINK_MAX + 1] = {0};

	pr_info("%s: check caller:%s, skip_pm_runtime:%d\n", __func__, user, skip_pm_runtime);
	for (i = 0; i < MAX_SMI_COMM_NUM; i++) {
		if (!dev || !dbg || !dbg->comm_dev[i])
			return -ENODEV;

		if (skip_pm_runtime)
			ret = 1;
		else
			ret = pm_runtime_get_if_in_use(dbg->comm_dev[i]);
		if (ret <= 0) {
			dev_info(dev, " MMinfra may off, comm_nr(%d), %d\n", i, ret);
			continue;
		}

		for (offset = 0; offset <= dbg->ctrl_size; offset += 4) {
			val = readl_relaxed(dbg->ctrl_base + offset);
			ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
				offset, val);
			if (ret < 0 || ret >= LINK_MAX - len) {
				ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
				if (ret < 0 || ret >= LINK_MAX - len)
					pr_notice("%s: ret:%d buf size:%d\n",
						__func__, ret, LINK_MAX - len);
				dev_info(dev, "%s\n", buf);

				len = 0;
				memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
				ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
					offset, val);
				if (ret < 0 || ret >= LINK_MAX - len)
					pr_notice("%s: ret:%d buf size:%d\n",
						__func__, ret, LINK_MAX - len);
			}
			len += ret;
		}
		ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
		if (ret < 0 || ret >= LINK_MAX - len)
			pr_notice("%s: ret:%d buf size:%d\n", __func__, ret, LINK_MAX - len);
		dev_info(dev, "%s\n", buf);

		mminfra_gals_dump();
		if (!skip_apsrc)
			pr_notice("%s: gce apsrc: %#x=%#x\n", __func__, GCE_BASE + GCE_GCTL_VALUE,
					readl(dbg->gce_base + GCE_GCTL_VALUE));

		if (!skip_pm_runtime)
			pm_runtime_put(dbg->comm_dev[i]);

		return 0;
	}
	return 0;
}

static int mminfra_smi_dbg_cb(struct notifier_block *nb,
		unsigned long value, void *data)
{
	if (strncmp((char *) data, "FORCE_DUMP", strlen("FORCE_DUMP")) == 0)
		mtk_mminfra_dbg_hang_detect("smi_dbg", true);
	else
		mtk_mminfra_dbg_hang_detect("smi_dbg", false);

	return 0;
}

static int mminfra_get_for_smi_dbg(void *v)
{
	int ret;
	struct smi_user_pwr_ctrl *smi_dbg = (struct smi_user_pwr_ctrl *)v;

	pr_notice("%s: caller:%s\n", __func__, smi_dbg->caller);
	if (strncmp(smi_dbg->caller, "VCP", strlen("VCP")) == 0)
		ret = 0;
	else {
		pm_runtime_get_sync(dev);
		mminfra_gals_dump();
		ret = 1;
	}

	return ret;
}

static int mminfra_put_for_smi_dbg(void *v)
{
	pm_runtime_put_sync(dev);

	return 0;
}

static struct smi_user_pwr_ctrl mminfra_pwr_ctrl = {
	.name = "mminfra",
	.data = &mminfra_pwr_ctrl,
	.smi_user_id = MTK_SMI_MMINFRA,
	.smi_user_get_if_in_use = mminfra_get_for_smi_dbg,
	.smi_user_put = mminfra_put_for_smi_dbg,
};

void mtk_mminfra_off_gipc(void)
{
	if (vcp_gipc) {
		if (dbg->vcp_gipc_in_set) {
			writel(MM_INFRA_OFF, dbg->vcp_gipc_in_set);
			pr_notice("VCP_GIPC_IN_SET = 0x%x\n", readl(dbg->vcp_gipc_in_set));
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mminfra_off_gipc);

static bool aee_dump;
static irqreturn_t mminfra_irq_handler(int irq, void *data)
{
	int ret;
	//char buf[LINK_MAX + 1] = {0};

	pr_notice("handle mminfra irq!\n");
	if (!dev || !dbg || !dbg->comm_dev[0])
		return IRQ_NONE;

	if (dbg->irq_safe) {
		ret = pm_runtime_get_sync(dev);
		if (ret)
			pr_notice("%s pm_runtime_get_sync ret[%d].\n", __func__, ret);
	}

	cmdq_util_mminfra_cmd(1);

	if (!aee_dump) {
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning("mminfra", "MMInfra bus timeout\n");
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
		if (dbg->irq_safe)
			mtk_smi_dbg_dump_for_mminfra();
		else
			mtk_smi_dbg_hang_detect("mminfra irq");
#endif
		aee_dump = true;
	}

	cmdq_util_mminfra_cmd(0);

	if (dbg->irq_safe)
		pm_runtime_put_sync(dev);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static void mminfra_devapc_power_off_cb(void)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mm_dapc_lock, flags);
	if (!mm_dapc_pwr_on) {
		spin_unlock_irqrestore(&mm_dapc_lock, flags);
		return;
	}
	if (mm_pwr_ver == mm_pwr_v3) {
		if (dbg->irq_safe) {
			pr_info("%s mm0 mtcmos = 0x%x, mm1 mtcmos = 0x%x, mm_dapc_pwr_on = %d, is_mminfra_shutdown = %d\n",
				__func__,
				readl(dbg->mminfra_mtcmos_base),
				readl(dbg->mminfra_mtcmos_base + 0x4),
				mm_dapc_pwr_on,
				is_mminfra_shutdown);

			if (!is_mminfra_shutdown) {
				pr_info("%s set mminfra pwr off\n", __func__);
				ret = pm_runtime_put_sync(dev);
				if (ret)
					pr_notice("%s: ret=%d\n", __func__, ret);
				mm_dapc_pwr_on = false;
				spin_unlock_irqrestore(&mm_dapc_lock, flags);
				return;
			}
		}
	}

	spin_unlock_irqrestore(&mm_dapc_lock, flags);
}

static bool mminfra_devapc_power_cb(void)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&mm_dapc_lock, flags);
	if (mm_pwr_ver <= mm_pwr_v2) {
		if (dbg->irq_safe) {
			pdchk_debug_dump();
			pr_info("mminfra mtcmos = 0x%x\n", readl(dbg->spm_base+0xea8));
			pr_info("cg_con0 = 0x%x, ao_cg_con0 = 0x%x, ao_cg_con1 = 0x%x\n",
				readl(dbg->mminfra_base + MMINFRA_CG_CON0),
				readl(dbg->mminfra_ao_base + MMINFRA_CG_CON0),
				readl(dbg->mminfra_ao_base + MMINFRA_CG_CON1));
			pr_info("%s is_mminfra_shutdown = %d\n", __func__, is_mminfra_shutdown);
			if (!is_mminfra_shutdown) {
				pr_info("%s set mminfra pwr on\n", __func__);
				vcp_mminfra_on();
			}
			spin_unlock_irqrestore(&mm_dapc_lock, flags);
			return true;
		}
	} else if (mm_pwr_ver == mm_pwr_v3) {
		if (dbg->irq_safe) {
			pr_info("%s mm0 mtcmos = 0x%x, mm1 mtcmos = 0x%x, mm_dapc_pwr_on = %d, is_mminfra_shutdown = %d\n",
				__func__,
				readl(dbg->mminfra_mtcmos_base),
				readl(dbg->mminfra_mtcmos_base + 0x4),
				mm_dapc_pwr_on,
				is_mminfra_shutdown);
			if ((readl(dbg->mminfra_mtcmos_base) & dbg->mm_mtcmos_mask)
				== dbg->mm_mtcmos_mask) {
				spin_unlock_irqrestore(&mm_dapc_lock, flags);
				return true;
			}

			if (!is_mminfra_shutdown && !mm_dapc_pwr_on) {
				pr_info("%s set mminfra pwr on\n", __func__);
				ret = pm_runtime_get_sync(dev);
				if (ret)
					pr_notice("%s: ret=%d\n", __func__, ret);
				mm_dapc_pwr_on = true;
				spin_unlock_irqrestore(&mm_dapc_lock, flags);
				return true;
			}
		}
	}
	spin_unlock_irqrestore(&mm_dapc_lock, flags);
	return is_mminfra_power_on();
}

static struct devapc_power_callbacks devapc_power_handle = {
	.type = DEVAPC_TYPE_MMINFRA,
	.query_power = mminfra_devapc_power_cb,
	.power_off = mminfra_devapc_power_off_cb,
};

#define MMINFRA_DEVAPC_VIO_STA_BIT_19		BIT(19)

static bool mminfra_devapc_excep_cb(int slave_type)
{
	int i;
	bool ret = false;
	unsigned int vio_sta_22;

	if (mm_pwr_ver == mm_pwr_v3) {
		vio_sta_22 = readl(dbg->mminfra_devapc_vio_sta + 0x458);
		if ((vio_sta_22 & MMINFRA_DEVAPC_VIO_STA_BIT_19) == MMINFRA_DEVAPC_VIO_STA_BIT_19)
			return true;

		for (i = 0; ;i++) {
			if(!(readl(dbg->mminfra_devapc_vio_sta + 0x400 + i*4) &
				(~readl(dbg->mminfra_devapc_vio_sta + i*4))))
				ret = true;
			else {
				pr_info("%s (%d) vio_mask = 0x%x, vio_sta = 0x%x\n", __func__,
					i, readl(dbg->mminfra_devapc_vio_sta + i*4),
					readl(dbg->mminfra_devapc_vio_sta + 0x400 + i*4));
				return false;
			}

			if ((dbg->mminfra_devapc_vio_sta + 0x400 + i*4)
				== dbg->mminfra_devapc_vio_sta + 0x458)
				break;
		}
	}

	return ret;
}

struct devapc_excep_callbacks devapc_excep_handle = {
	.type = DEVAPC_TYPE_MMINFRA,
	.handle_excep = mminfra_devapc_excep_cb,
};
#endif

static struct mtk_iommu_mm_pm_ops mminfra_pm_ops = {
	.pm_get = mtk_mminfra_get_if_in_use,
	.pm_put = mtk_mminfra_put,
};

static int mminfra_debug_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct platform_device *comm_pdev;
	struct property *prop;
	struct resource *res;
	struct mtk_mminfra_pwr_ctrl *pwr_ctl;
	const char *name;
	struct clk *clk;
	u32 comm_id, vlp_base_pa, spm_base_pa, hwccf_base_pa, tmp;
	int ret = 0, i = 0, irq, comm_nr = 0, clk_nr = 0;

	dbg = kzalloc(sizeof(*dbg), GFP_KERNEL);
	if (!dbg)
		return -ENOMEM;

	pwr_ctl = kzalloc(sizeof(*pwr_ctl), GFP_KERNEL);
	if(!pwr_ctl)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for ctrl\n");
		return -EINVAL;
	}

	dbg->ctrl_size = resource_size(res);
	dbg->ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dbg->ctrl_base)) {
		dev_notice(&pdev->dev, "could not ioremap resource for ctrl\n");
		return PTR_ERR(dbg->ctrl_base);
	}

	for_each_compatible_node(node, NULL, "mediatek,smi-common") {
		if (!node || !of_property_read_bool(node, "smi-common"))
			continue;

		of_property_read_u32(node, "mediatek,common-id", &comm_id);
		comm_pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!comm_pdev)
			return -EINVAL;
		pr_notice("[mminfra] comm_id=%d, comm_nr=%d\n", comm_id, comm_nr);
		dbg->comm_dev[comm_nr++] = &comm_pdev->dev;
	}

	node = pdev->dev.of_node;
	of_property_read_u32(node, "mminfra-bkrs", &mminfra_bkrs);

	of_property_read_u32(node, "bkrs-reg", &bkrs_reg_pa);
	if (!bkrs_reg_pa)
		bkrs_reg_pa = MMINFRA_BASE + 0x280;

	if (!mminfra_bkrs) {
		vcp_gipc = of_property_read_bool(node, "vcp-gipc");
		no_sleep_pd_cb = of_property_read_bool(node, "no-sleep-pd-cb");
	}

	skip_apsrc = of_property_read_bool(node, "skip-apsrc");
	has_infra_hfrp = of_property_read_bool(node, "has-infra-hfrp");

	if (!of_property_read_u32(node, "vlp-base", &vlp_base_pa)) {
		pr_notice("[mminfra] vlp_base_pa=%#x\n", vlp_base_pa);
		dbg->vlp_base = ioremap(vlp_base_pa, 0x1000);
	} else
		dbg->vlp_base = NULL;

	pwr_ctl->hw_voter.vlp_base = dbg->vlp_base;
	pwr_ctl->hw_voter.vlp_base_pa = vlp_base_pa;

	if (!of_property_read_u32(node, "hwccf-base", &hwccf_base_pa)) {
		pr_notice("[mminfra] hwccf_pa=%#x\n", hwccf_base_pa);
		pwr_ctl->hw_voter.hw_ccf_base = ioremap(hwccf_base_pa, 0x1000);
	} else
		pwr_ctl->hw_voter.hw_ccf_base = NULL;

	if (!of_property_read_u32(node, "spm-base", &spm_base_pa)) {
		pr_notice("[mminfra] spm_base_pa=%#x\n", spm_base_pa);
		dbg->spm_base = ioremap(spm_base_pa, 0x1000);
	} else
		dbg->spm_base = NULL;

	of_property_read_u32(node, "mm-voter-base", &dbg->mm_voter_base);
	of_property_read_u32(node, "mm-mtcmos-base", &dbg->mm_mtcmos_base);
	of_property_read_u32(node, "mm-mtcmos-mask", &dbg->mm_mtcmos_mask);
	of_property_read_u32(node, "mm-devapc-vio-sta-base", &dbg->mm_devapc_vio_sta_base);
	if (of_property_read_bool(node, "get-if-in-use-ena")) {
		pwr_ctl->hw_voter.get_if_in_use_ena = true;
		of_property_read_u32(node, "hw-voter-set-ofs", &pwr_ctl->hw_voter.set_ofs);
		of_property_read_u32(node, "hw-voter-clr-ofs", &pwr_ctl->hw_voter.clr_ofs);
		of_property_read_u32(node, "hw-voter-en-ofs", &pwr_ctl->hw_voter.en_ofs);
		of_property_read_u32(node, "hw-voter-done-bit-ofs", &pwr_ctl->hw_voter.done_bits_ofs);
		of_property_read_u32(node, "hw-voter-en-shift-ofs", &pwr_ctl->hw_voter.en_shift);
		pr_notice("%s set_ofs=%x clr_ofs=%x en_ofs=%x done_bits_ofs=%x shift=%d\n",
			__func__, pwr_ctl->hw_voter.set_ofs, pwr_ctl->hw_voter.clr_ofs,
			pwr_ctl->hw_voter.en_ofs, pwr_ctl->hw_voter.done_bits_ofs,
			pwr_ctl->hw_voter.en_shift);
	} else
		pwr_ctl->hw_voter.get_if_in_use_ena = false;

	dbg->mmu_mminfra_pwr_ctrl = pwr_ctl;
	mm_no_scmi = of_property_read_bool(node, "mm-no-scmi");
	mm_no_cg_ctrl = of_property_read_bool(node, "mm-no-cg-ctrl");
	of_property_read_u32(node, "mm-pwr-ver", &mm_pwr_ver);

	if (mm_pwr_ver <= mm_pwr_v2) {
		dbg->nb.notifier_call = mminfra_smi_dbg_cb;
		mtk_smi_dbg_register_notifier(&dbg->nb);
	} else if (mm_pwr_ver == mm_pwr_v3)
		mtk_smi_dbg_register_pwr_ctrl_cb(&mminfra_pwr_ctrl);

	mminfra_check_scmi_status();

	dev = &pdev->dev;
	pm_runtime_enable(dev);

	if (mm_pwr_ver <= mm_pwr_v2) {
		for (i = 0; i < MMINFRA_GALS_NR; i++) {
			if (!of_property_read_u32_index(dev->of_node, "mminfra-gals-sel", i, &tmp))
				dbg->gals_sel[i] = tmp;
			else
				dbg->gals_sel[i] = 0;
			pr_notice("[mminfra] gals_sel[%d]=%d\n", i, dbg->gals_sel[i]);
		}
	} else if (mm_pwr_ver == mm_pwr_v3) {
		/* get mm0/mm1/mm_ao debug out sel */
		if (!of_property_read_u32_index(dev->of_node, "mm-dbg-out-sel-nr", MM_0, &tmp))
			dbg->dbg_sel_out_nr[MM_0] = tmp;
		if (!of_property_read_u32_index(dev->of_node, "mm-dbg-out-sel-nr", MM_1, &tmp))
			dbg->dbg_sel_out_nr[MM_1] = tmp;
		if (!of_property_read_u32_index(dev->of_node, "mm-dbg-out-sel-nr", MM_AO, &tmp))
			dbg->dbg_sel_out_nr[MM_AO] = tmp;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get irq (%d)\n", irq);
	} else {
		ret = devm_request_irq(&pdev->dev, irq, mminfra_irq_handler, IRQF_SHARED,
				"mtk_mminfra_debug", dbg);
		if (ret) {
			dev_notice(&pdev->dev,
				"failed to register ISR %d (%d)", irq, ret);
			return ret;
		}
		cmdq_util_mminfra_cmd(0);
		cmdq_util_mminfra_cmd(3); //mminfra rfifo init
	}

	mminfra_ao_base = of_property_read_bool(node, "mminfra-ao-base");
	if (mm_pwr_ver <= mm_pwr_v2) {
		if (mminfra_ao_base)
			dbg->mminfra_ao_base = ioremap(MMINFRA_AO_BASE, 0xf18);
		dbg->mminfra_base = ioremap(MMINFRA_BASE, 0x8f4);
		dbg->gce_base = ioremap(GCE_BASE, 0x1000);
		dbg->vcp_gipc_in_set = ioremap(VCP_GIPC_IN_SET, 0x100);
	} else if (mm_pwr_ver == mm_pwr_v3){
		of_property_read_u32(node, "mmup-gipc-in-set", &dbg->vcp_gipc_in_set_base);
		if (dbg->vcp_gipc_in_set_base)
			dbg->vcp_gipc_in_set = ioremap(dbg->vcp_gipc_in_set_base, 0x100);

		/* get mm0/mm1/mm_ao cfg base */
		if (!of_property_read_u32_index(dev->of_node, "mm-cfg-base", MM_0, &tmp))
			dbg->mm_cfg_base[MM_0] = ioremap(tmp, 0x1000);
		if (!of_property_read_u32_index(dev->of_node, "mm-cfg-base", MM_1, &tmp))
			dbg->mm_cfg_base[MM_1] = ioremap(tmp, 0x1000);
		if (!of_property_read_u32_index(dev->of_node, "mm-cfg-base", MM_AO, &tmp))
			dbg->mm_cfg_base[MM_AO] = ioremap(tmp, 0x1000);

		if (dbg->mm_mtcmos_base)
			dbg->mminfra_mtcmos_base = ioremap(dbg->mm_mtcmos_base, 0x10);

		if (dbg->mm_devapc_vio_sta_base)
			dbg->mminfra_devapc_vio_sta = ioremap(dbg->mm_devapc_vio_sta_base, 0x1000);
	}

	mmpc_src_ctrl = of_property_read_bool(node, "mmpc-src-ctrl");
	if (mmpc_src_ctrl) {
		if (mm_pwr_ver == mm_pwr_v3) {
			of_property_read_u32(node, "mmpc-src-ctrl-base",
				&dbg->mmpc_src_ctrl_base);
			of_property_read_u32(node, "mm-proc-mtcmos-base",
				&dbg->mm_proc_mtcmos_base);
			for (i = 0; i < MMPC_RSC_USER_NR; i++) {
				if (!of_property_read_u32_index(dev->of_node, "mmpc-rsc-user",
					i, &tmp))
					dbg->mmpc_rsc_user[i] = tmp;
				else
					dbg->mmpc_rsc_user[i] = 0;
				pr_notice("[mminfra] mmpc_rsc_user[%d]=%d\n",
					i, dbg->mmpc_rsc_user[i]);
			}
		}
	}

	cmdq_get_mminfra_cb(is_mminfra_power_on);
	cmdq_get_mminfra_gce_cg_cb(is_gce_cg_on);

	if (mm_pwr_ver <= mm_pwr_v2) {
		if (vcp_gipc) {
			pm_runtime_irq_safe(dev);
			dbg->irq_safe = true;
		#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
			vcp_register_mminfra_cb_ex(vcp_mminfra_on, vcp_mminfra_off, vcp_debug_dump);
		#endif
			is_mminfra_shutdown = false;
		}
	} else {
		pm_runtime_irq_safe(dev);
		dbg->irq_safe = true;
		is_mminfra_shutdown = false;
	}

	mtk_pd_notifier.notifier_call = mtk_mminfra_pd_callback;
	ret = dev_pm_genpd_add_notifier(dev, &mtk_pd_notifier);

	of_property_for_each_string(node, "clock-names", prop, name) {
		clk = devm_clk_get(dev, name);
		if (IS_ERR(clk)) {
			dev_notice(dev, "%s: clks of %s init failed\n",
				__func__, name);
			ret = PTR_ERR(clk);
			break;
		}

		if (clk_nr == MMINFRA_MAX_CLK_NUM) {
			dev_notice(dev, "%s: clk num is wrong\n", __func__);
			ret = -EINVAL;
			break;
		}
		mminfra_clk[clk_nr++] = clk;
	}
	if (of_property_read_bool(node, "init-clk-on")) {
		atomic_inc(&clk_ref_cnt);
		if (!vcp_gipc)
			mminfra_clk_set(true);
		pr_notice("%s: init-clk-on enable clk\n", __func__);
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	spin_lock_init(&mm_dapc_lock);
	register_devapc_power_callback(&devapc_power_handle);
	register_devapc_exception_callback(&devapc_excep_handle);
#endif
	spin_lock_init(&dbg->mmu_mminfra_pwr_ctrl->lock);

	if (pwr_ctl->hw_voter.get_if_in_use_ena)
		mtk_iommu_set_pm_ops(&mminfra_pm_ops);

	return ret;
}

static void mminfra_debug_shutdown(struct platform_device *pdev)
{
	if (dbg->irq_safe) {
		is_mminfra_shutdown = true;
		pr_notice("[mminfra] shutdown = %d\n", is_mminfra_shutdown);
	}
}

static int mminfra_pm_prepare(struct device *dev)
{
	pr_notice("mminfra prepare\n");
	if (mm_pwr_ver <= mm_pwr_v2) {
		if (vcp_gipc) {
			mtk_clk_mminfra_hwv_power_ctrl_optional(true, CLK_MMINFRA_PWR_VOTE_BIT_MMINFRA);
			writel(MM_SYS_SUSPEND, dbg->vcp_gipc_in_set);
			pr_notice("VCP_GIPC_IN_SET = 0x%x\n", readl(dbg->vcp_gipc_in_set));
			mtk_clk_mminfra_hwv_power_ctrl_optional(false, CLK_MMINFRA_PWR_VOTE_BIT_MMINFRA);
		}
	}
	return 0;
}

static void mminfra_pm_complete(struct device *dev)
{
	pr_notice("mminfra complete\n");
	if (mm_pwr_ver <= mm_pwr_v2) {
		if (vcp_gipc) {
			mtk_clk_mminfra_hwv_power_ctrl_optional(true, CLK_MMINFRA_PWR_VOTE_BIT_MMINFRA);
			pr_notice("mminfra mtcmos = 0x%x, hfrp mtcmos = 0x%x, done bits=0x%x\n",
				readl(dbg->spm_base+0xea8), readl(dbg->spm_base+0xeac),
				readl(dbg->vlp_base+0x91c));
			pr_notice("mminfra dummy2 = 0x%x\n", readl(dbg->mminfra_base+0x408));
			writel(MM_SYS_RESUME, dbg->vcp_gipc_in_set);
			pr_notice("VCP_GIPC_IN_SET = 0x%x\n", readl(dbg->vcp_gipc_in_set));
			mtk_clk_mminfra_hwv_power_ctrl_optional(false,
				CLK_MMINFRA_PWR_VOTE_BIT_MMINFRA);
		}
	}
}

static int mminfra_pm_suspend(struct device *dev)
{
	mtk_smi_dbg_cg_status();
	return 0;
}

static int mminfra_pm_suspend_noirq(struct device *dev)
{
	pr_notice("mminfra suspend noirq\n");
	return 0;
}

static const struct dev_pm_ops mminfra_debug_pm_ops = {
	.prepare = mminfra_pm_prepare,
	.complete = mminfra_pm_complete,
	.suspend = mminfra_pm_suspend,
	.suspend_noirq = mminfra_pm_suspend_noirq,
};

static const struct of_device_id of_mminfra_debug_match_tbl[] = {
	{
		.compatible = "mediatek,mminfra-debug",
	},
	{}
};

static struct platform_driver mminfra_debug_drv = {
	.probe = mminfra_debug_probe,
	.shutdown = mminfra_debug_shutdown,
	.driver = {
		.name = "mtk-mminfra-debug",
		.of_match_table = of_mminfra_debug_match_tbl,
		.pm = &mminfra_debug_pm_ops,
	},
};

static int __init mtk_mminfra_debug_init(void)
{
	s32 status;

	status = platform_driver_register(&mminfra_debug_drv);
	if (status) {
		pr_notice("Failed to register MMInfra debug driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mminfra_debug_exit(void)
{
	platform_driver_unregister(&mminfra_debug_drv);
}

module_init(mtk_mminfra_debug_init);
module_exit(mtk_mminfra_debug_exit);
MODULE_DESCRIPTION("MTK MMInfra Debug driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL v2");
