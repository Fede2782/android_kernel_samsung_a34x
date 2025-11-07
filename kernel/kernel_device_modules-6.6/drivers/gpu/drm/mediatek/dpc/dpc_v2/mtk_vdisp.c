// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>

#include "mtk_vdisp.h"
#include "mtk_disp_vidle.h"
#include "mtk-smi-dbg.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

int debug_recover;
module_param(debug_recover, int, 0644);

#define SPM_MML0_PWR_CON 0xE90
#define SPM_MML1_PWR_CON 0xE94
#define SPM_DIS0_PWR_CON 0xE98
#define SPM_DIS1_PWR_CON 0xE9C
#define SPM_OVL0_PWR_CON 0xEA0
#define SPM_OVL1_PWR_CON 0xEA4
#define SPM_RTFF_SAVE_FLAG BIT(27)

#define SPM_ISO_CON_STA 0xF64
#define SPM_ISO_CON_SET 0xF68
#define SPM_ISO_CON_CLR 0xF6C
#define SPM_VDISP_EXT_BUCK_ISO       BIT(0)
#define SPM_AOC_VDISP_SRAM_ISO_DIN   BIT(1)
#define SPM_AOC_VDISP_SRAM_LATCH_ENB BIT(2)

#define VLP_DISP_SW_VOTE_CON 0x410	/* for mminfra pwr on */
#define VLP_DISP_SW_VOTE_SET 0x414
#define VLP_DISP_SW_VOTE_CLR 0x418
#define VLP_MMINFRA_DONE_OFS 0x91c
#define VOTE_RETRY_CNT 2500
#define VOTE_DELAY_US 2
#define POLL_DELAY_US 10
#define TIMEOUT_300MS 300000
#define TIMEOUT_30MS 30000

#define HW_CCF_AP_VOTER_BIT			(0)
#define HW_CCF_XPU0_BACKUP1_SET		(0x230)
#define HW_CCF_XPU0_BACKUP1_CLR		(0x234)
#define HW_CCF_BACKUP1_ENABLE       (0x1430)
#define HW_CCF_BACKUP1_STATUS       (0x1434)
#define HW_CCF_BACKUP1_DONE			(0x143C)
#define HW_CCF_BACKUP1_SET_STATUS	(0x1484)
#define HW_CCF_BACKUP1_CLR_STATUS	(0x1488)

struct mtk_vdisp {
	void __iomem *spm_base;
	void __iomem *vlp_base;
	void __iomem *mmpc_bus_prot_gp1;
	void __iomem *vdisp_ao_merge_irq;
	void __iomem *vdisp_ao_cg_con;
	void __iomem *hwccf_base;
	void __iomem *pwr_ack_wait_con;
	struct notifier_block rgu_nb;
	struct notifier_block pd_nb;
	enum disp_pd_id pd_id;
	int clk_num;
	struct clk **clks;
	int pm_ret;
	u32 pwr_ack_wait_time;
};
const struct mtk_vdisp_data default_vdisp_driver_data = {
	.avs = &default_vdisp_avs_driver_data,
};
static struct device *g_dev[DISP_PD_NUM];
static struct device *g_parent_dev;
static void __iomem *g_vlp_base;
static void __iomem *g_disp_voter;
static atomic_t g_mtcmos_cnt = ATOMIC_INIT(0);
static DEFINE_MUTEX(g_mtcmos_cnt_lock);
static struct dpc_funcs disp_dpc_driver;
struct wakeup_source *g_vdisp_wake_lock;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
atomic_t g_vdisp_wakelock_cnt;
#endif

static bool vcp_warmboot_support;

static void __iomem *g_smi_disp_dram_sub_comm[4];

static void check_subcomm_status(void)
{
	int i = 0, offset = 0;
	bool trigger_aee = false;

	for (i = 0; i < 4; i++) {
		if (readl(g_smi_disp_dram_sub_comm[i] + 0x40) != 0x1) {
			for (offset = 0; offset <= 0x1c; offset += 0x4) {
				VDISPDBG("subcomm(%d) offset(%d)=%#x",
					i, offset, readl(g_smi_disp_dram_sub_comm[i] + offset));
			}
			trigger_aee = true;
		}
	}

	if (trigger_aee)
		VDISPAEE("subcomm busy");
}

static int check_sub_devices(enum disp_pd_id pd_id)
{
	struct device *d;
	struct pm_domain_data *pdd;
	struct generic_pm_domain *gpd = pd_to_genpd(g_dev[pd_id]->pm_domain);
	int total_usage_count = 0;

	mutex_lock(&g_mtcmos_cnt_lock);
	list_for_each_entry(pdd, &gpd->dev_list, list_node) {
		d = pdd->dev;
		if (!d)
			continue;

		total_usage_count += atomic_read(&d->power.usage_count);
		if (d->power.runtime_status != RPM_SUSPENDED) {
			VDISPDBG("\t+ (%-80s %3d %3d %3d %3d %3d status:%3d)",
				dev_name(d),
				atomic_read(&d->power.usage_count),
				d->power.is_noirq_suspended,
				d->power.syscore,
				d->power.direct_complete,
				d->power.must_resume,
				d->power.runtime_status);
		}
	}
	mutex_unlock(&g_mtcmos_cnt_lock);

	return total_usage_count;
}

static void set_devices_syscore(void)
{
	struct device *d;
	struct pm_domain_data *pdd;
	struct generic_pm_domain *gpd;
	int pd_id;

	for (pd_id = 0; pd_id < DISP_PD_NUM; pd_id++) {
		gpd = pd_to_genpd(g_dev[pd_id]->pm_domain);
		list_for_each_entry(pdd, &gpd->dev_list, list_node) {
			d = pdd->dev;
			if (!d)
				continue;

			dev_pm_syscore_device(d, true);
		}
	}
}

static s32 mtk_vdisp_get_power_cnt(void)
{
	return atomic_read_acquire(&g_mtcmos_cnt);
}

static s32 mtk_vdisp_poll_power_cnt(s32 val)
{
	s32 ret, tmp;

	ret = readx_poll_timeout(mtk_vdisp_get_power_cnt, , tmp, tmp == val, 100, TIMEOUT_30MS);
	if (ret < 0) {
		u32 idx, mtcmos_cnt;
		u64 begin = sched_clock();
		static u32 fail_cnt, recover_cnt;
		u32 fail_mtcmos = tmp;

		/* get power from lower bit to higher bit, so disp_vcore is the first one */
		mtcmos_cnt = tmp;
		while (mtcmos_cnt) {
			idx = __builtin_ffs(mtcmos_cnt) - 1;
			mtcmos_cnt &= ~BIT(idx);
			check_sub_devices(idx);
			pm_runtime_get_sync(g_dev[idx]);
		}

		/* put power from higher bit to lower bit */
		mtcmos_cnt = tmp;
		while (mtcmos_cnt) {
			idx = 31 - __builtin_clz(mtcmos_cnt);
			mtcmos_cnt &= ~BIT(idx);
			pm_runtime_put_sync(g_dev[idx]);
		}

		ret = readx_poll_timeout(mtk_vdisp_get_power_cnt, , tmp, tmp == val, 100, TIMEOUT_30MS);
		if (ret == 0) {
			recover_cnt++;
			if (unlikely(debug_recover))
				VDISPAEE("mtcmos_cnt(%#x) recovery success, cost(%llu)", tmp,
					 sched_clock() - begin);
			else
				VDISPDBG("mtcmos_cnt(%#x) recovery success, cost(%llu)", tmp,
					 sched_clock() - begin);
		}

		fail_cnt++;
		VDISPDBG("mtcmos(%#x) fail_recover(%u,%u)", fail_mtcmos, fail_cnt, recover_cnt);
	}

	return ret;
}

static int regulator_event_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct mtk_vdisp *priv = container_of(nb, struct mtk_vdisp, rgu_nb);
	u32 val = 0;
	void __iomem *addr = 0;

	if (event == REGULATOR_EVENT_ENABLE) {
		addr = priv->spm_base + SPM_ISO_CON_CLR;
		writel_relaxed(SPM_VDISP_EXT_BUCK_ISO, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_ISO_DIN, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_LATCH_ENB, addr);

		// addr = priv->spm_base + SPM_ISO_CON_STA;
		// pr_info("REGULATOR_EVENT_ENABLE (%#llx) ", (u64)readl(addr));
	} else if (event == REGULATOR_EVENT_PRE_DISABLE) {
		addr = priv->spm_base + SPM_MML0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_MML1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_DIS0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_DIS1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_OVL0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_OVL1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_ISO_CON_SET;
		writel_relaxed(SPM_AOC_VDISP_SRAM_LATCH_ENB, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_ISO_DIN, addr);
		writel_relaxed(SPM_VDISP_EXT_BUCK_ISO, addr);

		// addr = priv->spm_base + SPM_ISO_CON_STA;
		// pr_info("REGULATOR_EVENT_PRE_DISABLE (%#llx) ", (u64)readl(addr));
	}

	return 0;
}

static void mtk_vdisp_vlp_disp_vote(u32 user, bool set)
{
	u32 addr = set ? VLP_DISP_SW_VOTE_SET : VLP_DISP_SW_VOTE_CLR;
	u32 ack = set ? BIT(user) : 0;
	u16 i = 0;

	if (unlikely(!g_vlp_base)) {
		VDISPERR("uninitialized g_vlp_base");
		return;
	}

	writel_relaxed(BIT(user), g_vlp_base + addr);
	do {
		writel_relaxed(BIT(user), g_vlp_base + addr);
		if ((readl(g_vlp_base + VLP_DISP_SW_VOTE_CON) & BIT(user)) == ack)
			break;

		if (i > VOTE_RETRY_CNT) {
			VDISPERR("vlp vote bit(%u) timeout", user);
			return;
		}

		udelay(VOTE_DELAY_US);
		i++;
	} while (1);
}

static void vdisp_hwccf_ctrl(struct mtk_vdisp *priv, bool enable)
{
	u32 hwccf_done = HW_CCF_BACKUP1_DONE;
	u32 ctrl_reg = (enable) ? HW_CCF_XPU0_BACKUP1_SET : HW_CCF_XPU0_BACKUP1_CLR;
	u32 hwccf_ctrl_status = (enable) ? HW_CCF_BACKUP1_SET_STATUS : HW_CCF_BACKUP1_CLR_STATUS;

	if (IS_ERR_OR_NULL(priv->hwccf_base))
		return;

	while((readl_relaxed(priv->hwccf_base + hwccf_done) & BIT(HW_CCF_AP_VOTER_BIT)) != BIT(HW_CCF_AP_VOTER_BIT))
		udelay(VOTE_DELAY_US);

	// set/clr
	writel_relaxed(BIT(HW_CCF_AP_VOTER_BIT), (priv->hwccf_base + ctrl_reg));

	// //polling
	// while(readl_relaxed(ctrl_reg) != 0){
	//	VDISPDBG("HWCCF_CTL_STSTUS= 0x%x", readl_relaxed(ctrl_reg));
	// }

	// wait for hwccf done
	while((readl_relaxed(priv->hwccf_base + hwccf_done) & BIT(HW_CCF_AP_VOTER_BIT)) != BIT(HW_CCF_AP_VOTER_BIT))
		udelay(VOTE_DELAY_US);

	// wait for ctrl status been cleared
	while((readl_relaxed(priv->hwccf_base + hwccf_ctrl_status) & BIT(HW_CCF_AP_VOTER_BIT)) != 0)
		udelay(VOTE_DELAY_US);
}

static void mminfra_hwv_pwr_ctrl(struct mtk_vdisp *priv, bool on)
{
	u32 value = 0, mask;
	int ret = 0;

	if (IS_ERR_OR_NULL(priv->vlp_base))
		return;

	/* [0] MMINFRA_DONE_STA
	 * [1] VCP_READY_STA
	 * [2] MMINFRA_DURING_OFF_STA
	 *
	 * Power on flow
	 *   polling 91c & 0x2 = 0x2 (wait 300ms timeout)
	 *   start vote (keep write til vote rg == voting value)
	 *   polling 91c == 0x3 (wait 300ms timeout)
	 *
	 * Power off flow
	 *   polling 91c & 0x2 = 0x2 (wait 300ms timeout)
	 *   start unvote (keep write til vote rg == unvoting value)
	 */

	if (vcp_warmboot_support)
		mask = 0xB;
	else
		mask = 0x3;

	ret = readl_poll_timeout_atomic(priv->vlp_base + VLP_MMINFRA_DONE_OFS, value,
					(value & 0x2) == 0x2, POLL_DELAY_US, TIMEOUT_300MS);
	if (ret < 0) {
		VDISPERR("failed to wait voter free");
		return;
	}

	mtk_vdisp_vlp_disp_vote(priv->pd_id, on);

	if (on) {
		ret = readl_poll_timeout_atomic(priv->vlp_base + VLP_MMINFRA_DONE_OFS, value,
						value == mask, POLL_DELAY_US, TIMEOUT_300MS);
		if (ret < 0)
			VDISPERR("failed to power on mminfra");
	}
}

static void __iomem *SPM_SEMA_AP;
#define KEY_HOLE    BIT(1)
static void mtk_sent_aod_scp_sema(void __iomem *_SPM_SEMA_AP)
{
	SPM_SEMA_AP = _SPM_SEMA_AP;
	VDISPDBG("%s:0x%llx\n", __func__, (long long)SPM_SEMA_AP);
}

static void vdisp_set_aod_scp_semaphore(int lock)
{
	int i = 0;
	bool key = false;

	if (SPM_SEMA_AP == NULL)
		return;

	key = ((readl(SPM_SEMA_AP) & KEY_HOLE) == KEY_HOLE);
	if (key == lock) {
		VDISPDBG("%s, skip %s sema\n", __func__, lock ? "get" : "put");
		return;
	}

	if (lock) {
		do {
			/* 40ms timeout */
			if (unlikely(++i > 4000))
				goto fail;
			writel(KEY_HOLE, SPM_SEMA_AP);
			udelay(10);
		} while ((readl(SPM_SEMA_AP) & KEY_HOLE) != KEY_HOLE);
	} else {
		writel(KEY_HOLE, SPM_SEMA_AP);
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			udelay(10);
		} while (readl(SPM_SEMA_AP) & KEY_HOLE);
	}

	return;
fail:
	VDISPERR("%s: %s sema:0x%lx fail(0x%x), retry:%d\n",
		__func__, lock ? "get" : "put", (unsigned long)SPM_SEMA_AP,
		readl(SPM_SEMA_AP), i);
}

static int mtk_vdisp_link_parent_power(bool to_link)
{
	static bool linked;
	int ret = 0;
	struct generic_pm_domain *gpm, *parent;

	if (IS_ERR_OR_NULL(g_dev[DISP_PD_DISP_VCORE]) ||
	    IS_ERR_OR_NULL(g_dev[DISP_PD_DISP_VCORE]->pm_domain) ||
	    IS_ERR_OR_NULL(g_parent_dev) ||
	    IS_ERR_OR_NULL(g_parent_dev->pm_domain)) {
		VDISPERR("device or parent device is NULL");
		return -EINVAL;
	}

	gpm = pd_to_genpd(g_dev[DISP_PD_DISP_VCORE]->pm_domain);
	parent = pd_to_genpd(g_parent_dev->pm_domain);

	if (!linked && to_link) {
		/* parent must be powered on before link */
		pm_runtime_get_sync(g_parent_dev);
		ret = pm_genpd_add_subdomain(parent, gpm);
		if (ret == 0)
			linked = true;
		else {
			VDISPDBG("failed to add subdomain ret(%d)", ret);
			if (!list_empty(&gpm->parent_links))
				VDISPDBG("parent_link is not empty");
			if (gpm->device_count)
				VDISPDBG("device_count(%d) not zero", gpm->device_count);
		}
		pm_runtime_put_sync(g_parent_dev);
	} else if (linked && !to_link) {
		ret = pm_genpd_remove_subdomain(parent, gpm);
		if (ret == 0)
			linked = false;
		else {
			VDISPDBG("failed to remove subdomain ret(%d)", ret);
			if (!list_empty(&gpm->parent_links))
				VDISPDBG("parent_link is not empty");
			if (gpm->device_count)
				VDISPDBG("device_count(%d) not zero", gpm->device_count);
		}
	}

	return ret;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
static void mtk_vdisp_wk_lock(u32 crtc_index, bool get, const char *func, int line)
{
	if (get) {
		__pm_stay_awake(g_vdisp_wake_lock);
		atomic_inc(&g_vdisp_wakelock_cnt);
	} else {
		__pm_relax(g_vdisp_wake_lock);
		atomic_dec(&g_vdisp_wakelock_cnt);
	}

	VDISPDBG("CRTC%d %s wakelock %s %d cnt(%u)",
		crtc_index, (get ? "hold" : "release"),
		func, line, atomic_read(&g_vdisp_wakelock_cnt));
}
#endif

static int genpd_event_notifier(struct notifier_block *nb,
			  unsigned long event, void *data)
{
	struct mtk_vdisp *priv = container_of(nb, struct mtk_vdisp, pd_nb);
	int i = 0, err = 0;

	switch (event) {
	case GENPD_NOTIFY_PRE_ON:
		mutex_lock(&g_mtcmos_cnt_lock);

		if (priv->pd_id == DISP_PD_DISP_VCORE) {
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
			__pm_stay_awake(g_vdisp_wake_lock);
#endif
			vdisp_set_aod_scp_semaphore(1); //protect AOD SCP flow
		}
		mminfra_hwv_pwr_ctrl(priv, true);

		if (atomic_read(&g_mtcmos_cnt) == 0)
			vdisp_hwccf_ctrl(priv, true);

		/* vote and power on mminfra */
		if (disp_dpc_driver.dpc_vidle_power_keep)
			priv->pm_ret = disp_dpc_driver.dpc_vidle_power_keep((enum mtk_vidle_voter_user)priv->pd_id);

		if (disp_dpc_driver.dpc_mtcmos_auto) {
			if (priv->pd_id == DISP_PD_DISP1)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_DIS1, DPC_MTCMOS_MANUAL);
			else if (priv->pd_id == DISP_PD_MML1)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_MML1, DPC_MTCMOS_MANUAL);
			else if (priv->pd_id == DISP_PD_MML0)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_MML0, DPC_MTCMOS_MANUAL);
		}

		/* modify power wait ack time */
		if (priv->pwr_ack_wait_con)
			writel(priv->pwr_ack_wait_time, priv->pwr_ack_wait_con);

		atomic_fetch_or(BIT(priv->pd_id), &g_mtcmos_cnt);
		mutex_unlock(&g_mtcmos_cnt_lock);
		break;
	case GENPD_NOTIFY_ON:
		for (i = 0; i < priv->clk_num; i++) {
			err = clk_prepare_enable(priv->clks[i]);
			if (err) {
				VDISPERR("failed to enable clk(%d): %d", i, err);
				break;
			}
		}

		/* clr vdisp_ao bus prot */
		if (priv->mmpc_bus_prot_gp1)
			writel(0x80, priv->mmpc_bus_prot_gp1 + 0x8);

		/* clr dpc cg */
		if (priv->vdisp_ao_cg_con)
			writel(BIT(16), priv->vdisp_ao_cg_con + 0x8);

		/* hold the platform resources ASAP, to avoid timing issue */
		if (disp_dpc_driver.dpc_group_enable)
			if (priv->pd_id == DISP_PD_DISP_VCORE)
				disp_dpc_driver.dpc_group_enable(DPC_SUBSYS_DISP, false);

		if (disp_dpc_driver.dpc_mtcmos_auto) {
			if (priv->pd_id == DISP_PD_MML1)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_MML1, DPC_MTCMOS_AUTO);
			else if (priv->pd_id == DISP_PD_MML0)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_MML0, DPC_MTCMOS_AUTO);
		}

		/* unvote and power off mminfra, release should be called only if keep successfully */
		if (disp_dpc_driver.dpc_vidle_power_release && priv->pm_ret == VOTER_PM_DONE)
			disp_dpc_driver.dpc_vidle_power_release((enum mtk_vidle_voter_user)priv->pd_id);

		mminfra_hwv_pwr_ctrl(priv, false);
		break;
	case GENPD_NOTIFY_PRE_OFF:
		mminfra_hwv_pwr_ctrl(priv, true);

		if (disp_dpc_driver.dpc_vidle_power_keep)
			priv->pm_ret = disp_dpc_driver.dpc_vidle_power_keep((enum mtk_vidle_voter_user)priv->pd_id);

		if (priv->pd_id == DISP_PD_DISP_VCORE)
			check_subcomm_status();

		if (disp_dpc_driver.dpc_mtcmos_auto) {
			if (priv->pd_id == DISP_PD_DISP1)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_DIS1, DPC_MTCMOS_MANUAL);
			else if (priv->pd_id == DISP_PD_MML1)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_MML1, DPC_MTCMOS_MANUAL);
			else if (priv->pd_id == DISP_PD_MML0)
				disp_dpc_driver.dpc_mtcmos_auto(DPC_SUBSYS_MML0, DPC_MTCMOS_MANUAL);
		}

		/* enable vdisp_ao merge irq, to fix burst irq when mtcmos on */
		if (priv->vdisp_ao_merge_irq)
			writel(0, priv->vdisp_ao_merge_irq);

		/* set vdisp_ao bus prot */
		if (priv->mmpc_bus_prot_gp1)
			writel(0x80, priv->mmpc_bus_prot_gp1 + 0x4);

		for (i = 0; i < priv->clk_num; i++)
			clk_disable_unprepare(priv->clks[i]);
		break;
	case GENPD_NOTIFY_OFF:
		mutex_lock(&g_mtcmos_cnt_lock);

		if (atomic_read(&g_mtcmos_cnt) == BIT(DISP_PD_DISP_VCORE))
			vdisp_hwccf_ctrl(priv, false);

		if (disp_dpc_driver.dpc_vidle_power_release && priv->pm_ret == VOTER_PM_DONE)
			disp_dpc_driver.dpc_vidle_power_release((enum mtk_vidle_voter_user)priv->pd_id);

		mminfra_hwv_pwr_ctrl(priv, false);
		if (priv->pd_id == DISP_PD_DISP_VCORE) {
			vdisp_set_aod_scp_semaphore(0); //protect AOD SCP flow
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
			__pm_relax(g_vdisp_wake_lock);
#endif
			if (!atomic_dec_and_test(&g_mtcmos_cnt)) {
				atomic_set_release(&g_mtcmos_cnt, 0);
				VDISPERR("mtcmos_cnt should be zero, reset to (%d)\n",
					 atomic_read_acquire(&g_mtcmos_cnt));
			}
		} else
			atomic_fetch_and(~BIT(priv->pd_id), &g_mtcmos_cnt);

		mutex_unlock(&g_mtcmos_cnt_lock);
		break;
	default:
		break;
	}

	return 0;
}

static void mtk_vdisp_genpd_put(void)
{
	int i = 0, j = 0;

	for (i = 0; i < DISP_PD_NUM; i++) {
		if (g_dev[i]) {
			VDISPDBG("pd(%d) ref(%u)", i,
				 atomic_read(&g_dev[i]->power.usage_count));
			pm_runtime_put_sync(g_dev[i]);
			j++;
		}
	}
	VDISPDBG("%d mtcmos has been put", j);

	/* set all devices as syscore to avoid expected power on when suspend resuming */
	set_devices_syscore();
}

static void mtk_vdisp_query_aging_val(void)
{
	mtk_vdisp_avs_query_aging_val(g_dev[DISP_PD_DISP_VCORE]);
}

static void mtk_vdisp_debug_mtcmos_ctrl(u32 pd_id, bool on)
{
	if (pd_id >= DISP_PD_NUM)
		return;

	if (on)
		pm_runtime_get_sync(g_dev[pd_id]);
	else
		pm_runtime_put_sync(g_dev[pd_id]);
}

static const struct mtk_vdisp_funcs funcs = {
	.genpd_put = mtk_vdisp_genpd_put,
	.vlp_disp_vote = mtk_vdisp_vlp_disp_vote,
	.poll_power_cnt = mtk_vdisp_poll_power_cnt,
	.sent_aod_scp_sema = mtk_sent_aod_scp_sema,
	.set_clk = mtk_vdisp_set_clk,
	.query_aging_val = mtk_vdisp_query_aging_val,
	.debug_mtcmos_ctrl = mtk_vdisp_debug_mtcmos_ctrl,
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	.wk_lock = mtk_vdisp_wk_lock,
#endif
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
static int mtk_smi_disp_get(void)
{
	mtk_vdisp_vlp_disp_vote(DISP_VIDLE_USER_SMI_DUMP, true);

	/* wait for disp mtcmos on */
	udelay(50);

	return 0;
}
static int mtk_smi_disp_put(void)
{
	mtk_vdisp_vlp_disp_vote(DISP_VIDLE_USER_SMI_DUMP, false);

	return 0;
}
static const struct smi_disp_ops smi_funcs = {
	.disp_get = mtk_smi_disp_get,
	.disp_put = mtk_smi_disp_put,
};
#endif

static int mtk_vdisp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *vcp_node;
	struct device_node *chosen_node;
	struct tag_bootmode *tag_boot;
	struct mtk_vdisp *priv;
	struct regulator *rgu;
	struct resource *res;
	const char *clkpropname = "vdisp-clock-names";
	const char *clkname;
	struct property *prop;
	struct clk *clk;
	int ret = 0;
	int support = 0;
	u32 pd_id = 0;
	int i = 0, clk_num = 0, genpd_num = 0, pair_size = 0;

	vcp_node = of_find_node_by_name(NULL, "vcp");
	if (vcp_node == NULL)
		pr_info("failed to find vcp_node @ %s\n", __func__);
	else {
		ret = of_property_read_u32(vcp_node, "warmboot-support", &support);
		of_node_put(vcp_node);

		if (ret || support == 0) {
			pr_info("%s vcp_warmboot_support is disabled: %d\n", __func__, ret);
			vcp_warmboot_support = false;
		} else
			vcp_warmboot_support = true;
	}

	VDISPDBG("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "SPM_BASE");
	if (res) {
		priv->spm_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->spm_base) {
			VDISPERR("fail to ioremap SPM_BASE: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "VLP_BASE");
	if (res) {
		priv->vlp_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->vlp_base) {
			VDISPERR("fail to ioremap VLP_BASE: 0x%pa", &res->start);
			return -EINVAL;
		}
		g_vlp_base = priv->vlp_base;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mmpc_bus_prot_gp1");
	if (res) {
		priv->mmpc_bus_prot_gp1 = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->mmpc_bus_prot_gp1) {
			VDISPERR("fail to ioremap mmpc_bus_prot_gp1: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vdisp_ao_inten");
	if (res) {
		priv->vdisp_ao_merge_irq = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->vdisp_ao_merge_irq) {
			VDISPERR("fail to ioremap vdisp_ao_merge_irq: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vdisp_ao_cg_con");
	if (res) {
		priv->vdisp_ao_cg_con = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->vdisp_ao_cg_con) {
			VDISPERR("fail to ioremap vdisp_ao_cg_con: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hwccf_base");
	if (res) {
		priv->hwccf_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->hwccf_base) {
			VDISPERR("fail to ioremap hwccf_base: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwr_ack_wait_con");
	if (res) {
		priv->pwr_ack_wait_con = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->pwr_ack_wait_con) {
			VDISPERR("fail to ioremap pwr_ack_wait_con: 0x%pa", &res->start);
			return -EINVAL;
		}

		ret = of_property_read_u32(dev->of_node, "pwr-ack-wait-time", &priv->pwr_ack_wait_time);
		if (ret) {
			VDISPERR("pwr-ack-wait-time property read fail(%d)", ret);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_sw_vote_set");
	if (res) {
		g_disp_voter = devm_ioremap(dev, res->start, resource_size(res));
		if (!g_disp_voter) {
			VDISPERR("fail to ioremap hwccf_base: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	if (of_find_property(dev->of_node, "dis1-shutdown-supply", NULL)) {
		rgu = devm_regulator_get(dev, "dis1-shutdown");
		if (!IS_ERR(rgu)) {
			priv->rgu_nb.notifier_call = regulator_event_notifier;
			ret = devm_regulator_register_notifier(rgu, &priv->rgu_nb);
			if (ret)
				VDISPERR("Failed to register notifier ret(%d)", ret);
		}
	}

	ret = of_property_read_u32(dev->of_node, "disp-pd-id", &pd_id);
	if (ret) {
		VDISPERR("disp-pd-id property read fail(%d)", ret);
		return -ENODEV;
	}

	clk_num = of_property_count_strings(dev->of_node, clkpropname);
	if (clk_num > 0) {
		priv->clk_num = clk_num;
		priv->clks = devm_kmalloc_array(dev, priv->clk_num, sizeof(*priv->clks), GFP_KERNEL);

		of_property_for_each_string(dev->of_node, clkpropname, prop, clkname) {
			clk = devm_clk_get(dev, clkname);
			if (IS_ERR(clk)) {
				VDISPERR("%s get %s clk failed\n", __func__, clkname);
				priv->clk_num = 0;
				break;
			}
			priv->clks[i] = clk;

			ret = clk_prepare_enable(priv->clks[i]);
			if (ret) {
				VDISPERR("failed to enable pd(%d) clk(%s): %d", pd_id, clkname, ret);
				return ret;
			}
			VDISPDBG("pd(%d) clk(%s) enable", pd_id, clkname);
			i++;
		}
	}

	if (mtk_vdisp_avs_probe(pdev))
		return -EINVAL;

	// Vote on when probe for sync status
	vdisp_hwccf_ctrl(priv, true);

	priv->pd_id = pd_id;
	priv->pd_nb.notifier_call = genpd_event_notifier;

	/* count power-domains pairs and attach to g_dev */
	genpd_num = of_count_phandle_with_args(dev->of_node, "power-domains", NULL);
	of_property_read_u32(dev->of_node, "disp-power-pair-size", &pair_size);
	if (pair_size > 0)
		genpd_num /= pair_size;
	else
		genpd_num = 0;
	g_dev[pd_id] = genpd_num <= 1 ? dev : dev_pm_domain_attach_by_id(dev, 0);

	if (pd_id == DISP_PD_DISP_VCORE) {
		g_vdisp_wake_lock = wakeup_source_create("vdisp_wakelock");
		wakeup_source_add(g_vdisp_wake_lock);

		g_smi_disp_dram_sub_comm[0] = ioremap(0x3e810400, 0x40);
		g_smi_disp_dram_sub_comm[1] = ioremap(0x3e820400, 0x40);
		g_smi_disp_dram_sub_comm[2] = ioremap(0x3e830400, 0x40);
		g_smi_disp_dram_sub_comm[3] = ioremap(0x3e840400, 0x40);

		if (genpd_num > 1) {
			VDISPDBG("pd(%d) power domains num > 1\n", pd_id);
			g_parent_dev = dev_pm_domain_attach_by_id(dev, 1);
			if (!pm_runtime_enabled(g_parent_dev))
				pm_runtime_enable(g_parent_dev);
		}

		/* if not normal mode, link parent power */
		chosen_node = of_find_node_by_path("/chosen");
		if (chosen_node) {
			tag_boot = (struct tag_bootmode *)of_get_property(chosen_node, "atag,boot", NULL);
			of_node_put(chosen_node);
			if (tag_boot) {
				if (tag_boot->bootmode != 0) {
					VDISPDBG("bootmode(%u), link parent power", tag_boot->bootmode);
					mtk_vdisp_link_parent_power(true);
				}
			}
		}
	}

	if (!pm_runtime_enabled(g_dev[pd_id]))
		pm_runtime_enable(g_dev[pd_id]);
	ret = dev_pm_genpd_add_notifier(g_dev[pd_id], &priv->pd_nb);
	if (ret)
		VDISPERR("dev_pm_genpd_add_notifier fail(%d)", ret);

	pm_runtime_get_sync(g_dev[pd_id]);
	VDISPDBG("get pd(%d)", pd_id);
	atomic_inc(&g_mtcmos_cnt);
	mtk_vdisp_register(&funcs, VDISP_VER2);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
	mtk_smi_set_disp_ops(&smi_funcs);
#endif

	return ret;
}

void mtk_vdisp_dpc_register(const struct dpc_funcs *funcs)
{
	disp_dpc_driver = *funcs;
}
EXPORT_SYMBOL(mtk_vdisp_dpc_register);

static int mtk_vdisp_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_vdisp_driver_v2_dt_match[] = {
	{.compatible = "mediatek,mt6991-vdisp-ctrl-v2",
	 .data = &default_vdisp_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vdisp_driver_v2_dt_match);

struct platform_driver mtk_vdisp_driver_v2 = {
	.probe = mtk_vdisp_probe,
	.remove = mtk_vdisp_remove,
	.driver = {
		.name = "mediatek-vdisp-ctrl-v2",
		.owner = THIS_MODULE,
		.of_match_table = mtk_vdisp_driver_v2_dt_match,
	},
};

static int __init mtk_vdisp_init(void)
{
	VDISPDBG("+");
	platform_driver_register(&mtk_vdisp_driver_v2);
	VDISPDBG("-");
	return 0;
}

static void __exit mtk_vdisp_exit(void)
{
	platform_driver_unregister(&mtk_vdisp_driver_v2);
}

late_initcall(mtk_vdisp_init);
module_exit(mtk_vdisp_exit);
MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK VDISP driver V2.0");
MODULE_SOFTDEP("post:mediatek-drm");
MODULE_LICENSE("GPL");
