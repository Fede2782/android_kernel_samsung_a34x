// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/interconnect.h>
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt6877.h"
#include "mtk_cm_mgr_common.h"
#include <mtk_cpufreq_api.h>

/* #define CREATE_TRACE_POINTS */
/* #include "mtk_cm_mgr_events_mt6877.h" */
#define trace_CM_MGR__perf_hint(idx, en, opp, base, hint, force_hint)

#include <linux/pm_qos.h>

static int pm_qos_update_request_status;
static int cm_mgr_dram_opp = -1;
u32 *cm_mgr_perfs;
static struct cm_mgr_hook local_hk;

static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];

#ifdef USE_CM_USER_MODE
unsigned int cm_user_mode;
unsigned int cm_user_active;
#endif

int cm_mgr_dram_opp_base = -1;
#ifdef USE_STEP_PERF_OPP
int cm_mgr_dram_perf_opp = 2;
int cm_mgr_dram_step_opp = 2;
#endif
static int cm_mgr_init_done;
static int cm_mgr_idx = -1;
spinlock_t cm_mgr_lock;

u32 cm_mgr_get_perfs_mt6877(int num)
{
	if (num < 0 || num >= cm_mgr_get_num_perf())
		return 0;
	return cm_mgr_perfs[num];
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perfs_mt6877);

static int cm_mgr_check_dram_type(void)
{
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	int ddr_type = mtk_dramc_get_ddr_type();
	int ddr_hz = mtk_dramc_get_steps_freq(0);

	if (ddr_type == TYPE_LPDDR5)
		cm_mgr_idx = CM_MGR_LP5;
	else
		cm_mgr_idx = CM_MGR_LP4;

	pr_info("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC !!! set cm_mgr_idx to 0x%x\n",
			__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC */

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
	if (cm_mgr_idx >= 0)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_TYPE, cm_mgr_idx);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	return cm_mgr_idx;
};

static int cm_mgr_get_idx(void)
{
	if (cm_mgr_idx < 0)
		return cm_mgr_check_dram_type();
	else
		return cm_mgr_idx;
};

struct timer_list cm_mgr_perf_timeout_timer;
static struct delayed_work cm_mgr_timeout_work;
#define CM_MGR_PERF_TIMEOUT_MS	msecs_to_jiffies(100)

static void cm_mgr_timeout_process(struct work_struct *work)
{
	icc_set_bw(cm_mgr_get_bw_path(), 0, 0);
}

static void cm_mgr_perf_timeout_timer_fn(struct timer_list *timer)
{
	if (pm_qos_update_request_status) {
		cm_mgr_dram_opp = -1;
		cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
		schedule_delayed_work(&cm_mgr_timeout_work, 1);

		pm_qos_update_request_status = 0;
		debounce_times_perf_down_local_set(-1);
		debounce_times_perf_down_force_local_set(-1);

		trace_CM_MGR__perf_hint(2, 0,
				cm_mgr_dram_opp, cm_mgr_get_dram_opp_base(),
				debounce_times_perf_down_local_get(),
				debounce_times_perf_down_force_local_get());
	}
}

#define PERF_TIME 100

static ktime_t perf_now;

void cm_mgr_perf_platform_set_status_mt6877(int enable)
{
	unsigned long expires;
	int down_local;

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}

	/* set dsu mode */
	mt_cpufreq_update_cci_mode(enable, 2);

	if (enable) {
		if (!cm_mgr_get_perf_enable())
			return;

		debounce_times_perf_down_local_set(0);

		perf_now = ktime_get();

		if (cm_mgr_get_dram_opp_base() == -1) {
		#if IS_ENABLED(CONFIG_MTK_DVFSRC)
			cm_mgr_dram_opp_base = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);
		#else
			cm_mgr_dram_opp_base = 0;
		#endif /* CONFIG_MTK_DVFSRC */

			if (cm_mgr_dram_perf_opp < 0)
				cm_mgr_dram_opp = 0;
			else if (cm_mgr_dram_opp_base > cm_mgr_dram_perf_opp) {
				if (cm_mgr_dram_step_opp < 0)
					cm_mgr_dram_step_opp = 0;
				cm_mgr_dram_opp = cm_mgr_dram_step_opp;

			} else
				cm_mgr_dram_opp = 0;
			cm_mgr_dram_opp = 0;
			cm_mgr_set_dram_opp_base(cm_mgr_get_num_perf());
			icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			}
		}

		pm_qos_update_request_status = enable;
	} else {
		down_local = debounce_times_perf_down_local_get();
		if (down_local < 0)
			return;

		++down_local;
		debounce_times_perf_down_local_set(down_local);
		//To be confirmed:
		if (down_local > debounce_times_perf_down_get()) {
			if (cm_mgr_get_dram_opp_base() < 0) {
				icc_set_bw(cm_mgr_get_bw_path(), 0, 0);
				pm_qos_update_request_status = enable;
				debounce_times_perf_down_local_set(-1);
				goto trace;
			}
			if (ktime_ms_delta(ktime_get(), perf_now) < PERF_TIME)
				goto trace;
			cm_mgr_set_dram_opp_base(-1);
		}

		if ((cm_mgr_dram_opp < cm_mgr_get_dram_opp_base()) &&
				(debounce_times_perf_down_get() > 0)) {
			cm_mgr_dram_opp = cm_mgr_get_dram_opp_base() *
				debounce_times_perf_down_local_get() /
				debounce_times_perf_down_get();

			if ((cm_mgr_dram_perf_opp >= 0) &&
				(cm_mgr_dram_opp < cm_mgr_dram_step_opp))
				cm_mgr_dram_opp = cm_mgr_dram_step_opp;

			icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			cm_mgr_dram_opp = -1;
			cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
			icc_set_bw(cm_mgr_get_bw_path(), 0, 0);

			pm_qos_update_request_status = enable;
			debounce_times_perf_down_local_set(-1);
		}
	}

trace:
	trace_CM_MGR__perf_hint(0, enable,
			cm_mgr_dram_opp, cm_mgr_get_dram_opp_base(),
			debounce_times_perf_down_local_get(),
			debounce_times_perf_down_force_local_get());
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_status_mt6877);

void cm_mgr_perf_platform_set_force_status(int enable)
{
	unsigned long expires;
	int down_force_local;

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}

	/* set dsu mode */
	mt_cpufreq_update_cci_mode(enable, 2);

	if (enable) {
		if (!cm_mgr_get_perf_enable())
			return;

		if (!cm_mgr_get_perf_force_enable())
			return;

		debounce_times_perf_down_force_local_set(0);

		if (cm_mgr_get_dram_opp_base() == -1) {
			cm_mgr_dram_opp = 0;
			icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			}
		}

		pm_qos_update_request_status = enable;
	} else {
		down_force_local = debounce_times_perf_down_force_local_get();
		if (down_force_local < 0)
			return;

		if (pm_qos_update_request_status == 0)
			return;

		++down_force_local;
		debounce_times_perf_down_force_local_set(down_force_local);
		if ((!cm_mgr_get_perf_force_enable()) ||
				(down_force_local >=
				 debounce_times_perf_force_down_get())) {

			if ((cm_mgr_dram_opp < cm_mgr_get_dram_opp_base()) &&
					(debounce_times_perf_down_get() > 0)) {
				cm_mgr_dram_opp = cm_mgr_get_dram_opp_base() *
					down_force_local /
					debounce_times_perf_force_down_get();
				icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			} else {
				cm_mgr_dram_opp = -1;
				cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
				icc_set_bw(cm_mgr_get_bw_path(), 0, 0);

				pm_qos_update_request_status = enable;
				debounce_times_perf_down_force_local_set(-1);
			}
		}
	}

	trace_CM_MGR__perf_hint(1, enable,
			cm_mgr_dram_opp, cm_mgr_get_dram_opp_base(),
			debounce_times_perf_down_local_get(),
			debounce_times_perf_down_force_local_get());
}

void cm_mgr_perf_set_status_mt6877(int enable)
{
	if (cm_mgr_get_disable_fb() == 1 && cm_mgr_get_blank_status() == 1)
		enable = 0;

	cm_mgr_perf_platform_set_force_status(enable);

	if (cm_mgr_get_perf_force_enable())
		return;

	cm_mgr_perf_platform_set_status_mt6877(enable);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_status_mt6877);

void cm_mgr_perf_set_force_status_mt6877(int enable)
{
	if (enable != cm_mgr_get_perf_force_enable()) {
		cm_mgr_set_perf_force_enable(enable);
		if (!enable)
			cm_mgr_perf_platform_set_force_status(enable);
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_force_status_mt6877);

void check_cm_mgr_status_mt6877(unsigned int cluster, unsigned int freq,
		unsigned int idx)
{
	unsigned long spinlock_save_flag;

	if (!cm_mgr_init_done)
		return;

	spin_lock_irqsave(&cm_mgr_lock, spinlock_save_flag);

	prev_freq_idx[cluster] = idx;
	prev_freq[cluster] = freq;

	spin_unlock_irqrestore(&cm_mgr_lock, spinlock_save_flag);
	cm_mgr_update_dram_by_cpu_opp
		(prev_freq_idx[CM_MGR_CPU_CLUSTER - 1]);
}
EXPORT_SYMBOL_GPL(check_cm_mgr_status_mt6877);

#ifdef USE_CM_USER_MODE
void cm_mgr_user_mode_set(unsigned int mode)
{
	/* reset to default if active before*/
	if (cm_user_active && !mode)
		cm_mgr_user_mode_cmd(1, NULL, 0, 0);

	if (((0x1 << cm_mgr_idx) & mode) != 0)
		cm_user_active = 1;
	else
		cm_user_active = 0;

	cm_user_mode = mode;

	pr_info("#@# %s(%d) cm_user_mode: %d active:%d (mode=%d idx=%d)\n",
		__func__, __LINE__, cm_user_mode, cm_user_active, mode, cm_mgr_idx);
}

void cm_mgr_user_mode_cmd(int reset, char *cmd, unsigned int val_1,
							unsigned int val_2)
{
	int i;

	if (reset) {
		if (cm_mgr_idx == CM_MGR_LP4) {
			for (i = 0; i < CM_MGR_EMI_OPP; i++) {
				cpu_power_ratio_up[i] = cpu_power_ratio_up0[i];
				cpu_power_ratio_down[i] = cpu_power_ratio_down0[i];
				debounce_times_down_adb[i] = debounce_times_down_adb0[i];
			}
		} else if (cm_mgr_idx == CM_MGR_LP5) {
			for (i = 0; i < CM_MGR_EMI_OPP; i++) {
				cpu_power_ratio_up[i] = cpu_power_ratio_up1[i];
				cpu_power_ratio_down[i] = cpu_power_ratio_down1[i];
				debounce_times_down_adb[i] = debounce_times_down_adb1[i];
			}
		} else {
			pr_info("#@# %s(%d) cm_mgr_idx: %d unknown\n",
					__func__, __LINE__, cm_mgr_idx);
		}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		for (i = 0; i < CM_MGR_EMI_OPP; i++) {
			cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_UP,
				i << 16 | cpu_power_ratio_up[i]);
			cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_DOWN,
				i << 16 | cpu_power_ratio_down[i]);
			cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_DOWN,
				i << 16 | debounce_times_down_adb[i]);
		}
#endif
	} else {
		if (!strcmp(cmd, "cpu_power_ratio_up")) {
			if (val_1 < CM_MGR_EMI_OPP)
				cpu_power_ratio_up[val_1] = val_2;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
			cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_UP,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		} else if (!strcmp(cmd, "cpu_power_ratio_down")) {
			if (val_1 < CM_MGR_EMI_OPP)
				cpu_power_ratio_down[val_1] = val_2;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
			cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_DOWN,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		} else if (!strcmp(cmd, "debounce_times_down_adb")) {
			if (val_1 < CM_MGR_EMI_OPP)
				debounce_times_down_adb[val_1] = val_2;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
			cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_DOWN,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		} else {
			pr_info("cmd:%s not support for user mode\n", cmd);
		}
	}
}
#endif /* USE_CM_USER_MODE */

void cm_mgr_ddr_setting_init(void)
{
	int i;
	int idx = cm_mgr_get_idx();

	if (idx == CM_MGR_LP4) {
		for (i = 0; i < CM_MGR_EMI_OPP; i++) {
			cpu_power_ratio_up[i] = cpu_power_ratio_up0[i];
			cpu_power_ratio_down[i] = cpu_power_ratio_down0[i];
			debounce_times_up_adb[i] = debounce_times_up_adb0[i];
			debounce_times_down_adb[i] = debounce_times_down_adb0[i];
		}
#ifdef USE_BCPU_WEIGHT
		cpu_power_bcpu_weight_max = cpu_power_bcpu_weight_max0;
		cpu_power_bcpu_weight_min = cpu_power_bcpu_weight_min0;
#endif
#ifdef CM_BCPU_MIN_OPP_WEIGHT
		cm_mgr_bcpu_min_opp_weight = cm_mgr_bcpu_min_opp_weight0;
		cm_mgr_bcpu_low_opp_weight = cm_mgr_bcpu_low_opp_weight0;
		cm_mgr_bcpu_low_opp_bound = cm_mgr_bcpu_low_opp_bound0;
#endif /* CM_BCPU_MIN_OPP_WEIGHT */
	} else if (idx == CM_MGR_LP5) {
		for (i = 0; i < CM_MGR_EMI_OPP; i++) {
			cpu_power_ratio_up[i] = cpu_power_ratio_up1[i];
			cpu_power_ratio_down[i] = cpu_power_ratio_down1[i];
			debounce_times_up_adb[i] = debounce_times_up_adb1[i];
			debounce_times_down_adb[i] = debounce_times_down_adb1[i];
		}
#ifdef USE_BCPU_WEIGHT
		cpu_power_bcpu_weight_max = cpu_power_bcpu_weight_max1;
		cpu_power_bcpu_weight_min = cpu_power_bcpu_weight_min1;
#endif
#ifdef CM_BCPU_MIN_OPP_WEIGHT
		cm_mgr_bcpu_min_opp_weight = cm_mgr_bcpu_min_opp_weight1;
		cm_mgr_bcpu_low_opp_weight = cm_mgr_bcpu_low_opp_weight1;
		cm_mgr_bcpu_low_opp_bound = cm_mgr_bcpu_low_opp_bound1;
#endif /* CM_BCPU_MIN_OPP_WEIGHT */
	}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
	for (i = 0; i < CM_MGR_EMI_OPP; i++) {
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_UP,
			i << 16 | cpu_power_ratio_up[i]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_DOWN,
			i << 16 | cpu_power_ratio_down[i]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_UP,
			i << 16 | debounce_times_up_adb[i]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_DOWN,
			i << 16 | debounce_times_down_adb[i]);
	}
#ifdef CM_BCPU_MIN_OPP_WEIGHT
	cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_MIN_OPP_WEIGHT_SET,
			cm_mgr_bcpu_min_opp_weight);
	cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_LOW_OPP_WEIGHT_SET,
			cm_mgr_bcpu_low_opp_weight);
	cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_LOW_OPP_BOUND_SET,
			cm_mgr_bcpu_low_opp_bound);
#endif /* CM_BCPU_MIN_OPP_WEIGHT */
#endif
}

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
	int i;
#endif /* CONFIG_MTK_DVFSRC */
	struct icc_path *bw_path;

	spin_lock_init(&cm_mgr_lock);
	force_use_bcpu_weight();
	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO INIT(%d)\n", ret);
		return ret;
	}

	(void)cm_mgr_get_idx();

	/* required-opps */
	ret = of_count_phandle_with_args(node, "required-opps", NULL);
	cm_mgr_set_num_perf(ret);
	pr_info("#@# %s(%d) cm_mgr_num_perf %d\n",
			__func__, __LINE__, ret);

	bw_path = of_icc_get(&pdev->dev, "cm-perf-bw");
	if (IS_ERR(bw_path)) {
		dev_info(&pdev->dev, "get cm-perf_bw fail\n");
		cm_mgr_set_bw_path(NULL);
	} else {
		cm_mgr_set_bw_path(bw_path);
	}

	if (ret > 0) {
		cm_mgr_perfs = devm_kzalloc(&pdev->dev,
				ret * sizeof(u32),
				GFP_KERNEL);

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
		for (i = 0; i < ret; i++) {
			cm_mgr_perfs[i] =
				dvfsrc_get_required_opp_peak_bw(node, i);
		}
#endif /* CONFIG_MTK_DVFSRC */
		cm_mgr_set_num_array(ret - 2);
	} else
		cm_mgr_set_num_array(0);
	pr_info("#@# %s(%d) cm_mgr_num_array %d\n",
			__func__, __LINE__, cm_mgr_get_num_array());

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&cm_mgr_timeout_work, cm_mgr_timeout_process);
	timer_setup(&cm_mgr_perf_timeout_timer, cm_mgr_perf_timeout_timer_fn,
			0);

	local_hk.cm_mgr_get_perfs =
		cm_mgr_get_perfs_mt6877;
	local_hk.cm_mgr_perf_set_force_status =
		cm_mgr_perf_set_force_status_mt6877;
	local_hk.check_cm_mgr_status =
		check_cm_mgr_status_mt6877;
	local_hk.cm_mgr_perf_platform_set_status =
		cm_mgr_perf_platform_set_status_mt6877;
	local_hk.cm_mgr_perf_set_status =
		cm_mgr_perf_set_status_mt6877;

	cm_mgr_register_hook(&local_hk);

	cm_mgr_set_pdev(pdev);

	dev_pm_genpd_set_performance_state(&pdev->dev, 0);

	cm_mgr_ddr_setting_init();

	cm_mgr_init_done = 1;

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

	return 0;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_unregister_hook(&local_hk);
	cm_mgr_common_exit();
	icc_put(cm_mgr_get_bw_path());
	kfree(cm_mgr_perfs);

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt6877-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6877-cm_mgr", 0},
	{ },
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt6877-cm_mgr",
		.owner = THIS_MODULE,
		.of_match_table = platform_cm_mgr_of_match,
	},
	.id_table = platform_cm_mgr_id_table,
};

/*
 * driver initialization entry point
 */
static int __init platform_cm_mgr_init(void)
{
	return platform_driver_register(&mtk_platform_cm_mgr_driver);
}

static void __exit platform_cm_mgr_exit(void)
{
	platform_driver_unregister(&mtk_platform_cm_mgr_driver);
	pr_info("[CM_MGR] platform-cm_mgr Exit.\n");
}

subsys_initcall(platform_cm_mgr_init);
module_exit(platform_cm_mgr_exit);

MODULE_DESCRIPTION("Mediatek cm_mgr driver");
MODULE_AUTHOR("Morven-CF Yeh<morven-cf.yeh@mediatek.com>");
MODULE_LICENSE("GPL");
