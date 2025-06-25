// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_core.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/random.h>

#include <gpufreq_v2_legacy.h>
#include <gpufreq_debug_legacy.h>
#include <gpuppm_legacy.h>
#include <gpufreq_common_legacy.h>
#include <gpufreq_mt6877.h>
#include <gpufreq_early_init.h>
#include <mtk_gpu_utility.h>

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
#include <mtk_bp_thl.h>
#endif
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_STATIC_POWER_LEGACY)
#include <leakage_table_v2/mtk_static_power.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING)
#include <clk-mtk.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6877)
#include <clk-fmeter.h>
#include <clk-mt6877.h>
#include <clk-mt6877-fmeter.h>
#include <dt-bindings/clock/mt6877-clk.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static unsigned int __gpufreq_custom_init_enable(void);
static unsigned int __gpufreq_dvfs_enable(void);
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state);
static void __gpufreq_set_timestamp(void);
static void __gpufreq_check_bus_idle(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
static void __gpufreq_measure_power(void);
static void __gpufreq_set_springboard(void);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static void __gpufreq_apply_aging(unsigned int apply_aging);
static void __gpufreq_apply_adjust(struct gpufreq_adj_info *adj_table, int adj_num);
/* dvfs function */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu(bool mode, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(bool mode, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int vgpu);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq);
/* power control function */
static void __gpufreq_external_cg_control(void);
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
/* bringup function*/
static unsigned int __gpufreq_bringup(void);
/* init function */
static int __gpufreq_init_shader_present(void);
static void __gpufreq_segment_adjustment(struct platform_device *pdev);
static void __gpufreq_custom_adjustment(void);
static void __gpufreq_avs_adjustment(void);
static void __gpufreq_aging_adjustment(void);
static int __gpufreq_init_opp_idx(void);
static int __gpufreq_init_opp_table(struct platform_device *pdev);
static int __gpufreq_init_segment_id(struct platform_device *pdev);
static int __gpufreq_init_mtcmos(struct platform_device *pdev);
static int __gpufreq_init_clk(struct platform_device *pdev);
static int __gpufreq_init_pmic(struct platform_device *pdev);
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);
static int __gpufreq_mfgpll_probe(struct platform_device *pdev);
/*low power*/
static void __gpufreq_kick_pbm(int enable);
static unsigned int __gpufreq_get_ptpod_opp_idx(unsigned int idx);
static void __gpufreq_update_gpu_working_table(void);

//thermal
static void __mt_update_gpufreqs_power_table(void);
/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __gpufreq_pdrv_probe,
	.remove = __gpufreq_pdrv_remove,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static const struct of_device_id g_mfgpll_of_match[] = {
	{ .compatible = "mediatek,mt6877-gpu_pll_ctrl" },
	{ /* sentinel */ }
};

static struct platform_driver g_gpufreq_mfgpll_pdrv = {
	.probe = __gpufreq_mfgpll_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq_mfgpll",
		.owner = THIS_MODULE,
		.of_match_table = g_mfgpll_of_match,
	},
};



static unsigned int g_vgpu_sfchg_rrate;
static unsigned int g_vgpu_sfchg_frate;
static unsigned int g_vsram_sfchg_rrate;
static unsigned int g_vsram_sfchg_frate;

static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;

#if MT_GPUFREQ_SHADER_PWR_CTL_WA
static spinlock_t mt_gpufreq_clksrc_parking_lock;
#endif

static void __iomem *g_gpu_pll_ctrl;
static void __iomem *g_topckgen_base;
static void __iomem *g_MFG_base;
static void __iomem *g_infracfg_base;
static void __iomem *g_infracfg_ao;
static void __iomem *g_dbgtop;
static void __iomem *g_sleep;
static void __iomem *g_toprgu;
static void __iomem *g_apmixed_base;
static unsigned int g_aging_load;
static unsigned int g_mcl50_load;
static unsigned int g_gpueb_support;
static unsigned int g_aging_enable;
static unsigned int g_aging_margin;
static unsigned int g_shader_present;
static unsigned int g_stress_test_enable;
static struct gpufreq_status g_gpu;
static int g_aging_table_id = -1;

//thermal
static struct mt_gpufreq_power_table_info *g_power_table;
static bool g_thermal_protect_limited_ignore_state;
static int (*mt_gpufreq_wrap_fp)(void);

static enum gpufreq_dvfs_state g_dvfs_state;
static DEFINE_MUTEX(gpufreq_lock);
static DEFINE_MUTEX(ptpod_lock);

struct gpufreq_opp_info *g_default_gpu;

static const struct mtk_pll_div_table mfgpll_div_table[] = {
	{ .div = 0, .freq = MFGPLL_PLL_FMAX },
	{ .div = 1, .freq = 1900000000 },
	{ .div = 2, .freq = 950000000 },
	{ .div = 3, .freq = 1 },
	{ .div = 4, .freq = 1 },
	{ /* sentinel */ }
};

static const struct mtk_pll_data g_mfg_plls[] = {
	PLL_PWR(CLK_MFG_AO_MFGPLL1, "mfg_ao_mfgpll1",
		MFGPLL1_CON0_OFS/*base*/,
		MFGPLL1_CON0_OFS, BIT(0)/*en*/,
		MFGPLL1_CON3_OFS/*pwr*/, CLK_SET_ROUND_RATE/*flags*/,
		MFGPLL1_CON1_OFS, 24/*pd*/,
		MFGPLL1_CON1_OFS, 0, 22/*pcw*/, mfgpll_div_table),
	PLL_PWR(CLK_MFG_AO_MFGPLL4, "mfg_ao_mfgpll4",
		MFGPLL4_CON0_OFS/*base*/,
		MFGPLL4_CON0_OFS, BIT(0)/*en*/,
		MFGPLL4_CON3_OFS/*pwr*/, CLK_SET_ROUND_RATE/*flags*/,
		MFGPLL4_CON1_OFS, 24/*pd*/,
		MFGPLL4_CON1_OFS, 0, 22/*pcw*/, mfgpll_div_table),
};

static struct gpufreq_platform_fp platform_ap_fp = {
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.get_power_state = __gpufreq_get_power_state,
	.get_dvfs_state = __gpufreq_get_dvfs_state,
	.get_shader_present = __gpufreq_get_shader_present,
	.get_cur_fgpu = __gpufreq_get_cur_fgpu,
	.get_cur_vgpu = __gpufreq_get_cur_vgpu,
	.get_cur_vsram_gpu = __gpufreq_get_cur_vsram_gpu,
	.get_cur_pgpu = __gpufreq_get_cur_pgpu,
	.get_max_pgpu = __gpufreq_get_max_pgpu,
	.get_min_pgpu = __gpufreq_get_min_pgpu,
	.get_cur_idx_gpu = __gpufreq_get_cur_idx_gpu,
	.get_opp_num_gpu = __gpufreq_get_opp_num_gpu,
	.get_signed_opp_num_gpu = __gpufreq_get_signed_opp_num_gpu,
	.get_working_table_gpu = __gpufreq_get_working_table_gpu,
	.get_signed_table_gpu = __gpufreq_get_signed_table_gpu,
	.get_debug_opp_info_gpu = __gpufreq_get_debug_opp_info_gpu,
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.power_control = __gpufreq_power_control,
	.generic_commit_gpu = __gpufreq_generic_commit_gpu,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_stress_test = __gpufreq_set_stress_test,
	.get_core_num = __gpufreq_get_core_num,
	.update_power_table = __mt_update_gpufreqs_power_table,
	.get_idx_by_pgpu = __gpufreq_get_idx_by_pgpu,
};

static struct gpufreq_platform_fp platform_eb_fp = {
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get BRINGUP status */
unsigned int __gpufreq_bringup(void)
{
	return GPUFREQ_BRINGUP;
}

/* API: get POWER_CTRL status */
unsigned int __gpufreq_power_ctrl_enable(void)
{
	return GPUFREQ_POWER_CTRL_ENABLE;
}

/* API: get power state (on/off) */
unsigned int __gpufreq_get_power_state(void)
{
	if (g_gpu.power_count > 0)
		return GPU_PWR_ON;
	else
		return GPU_PWR_OFF;
}

/* API: get DVFS state (free/disable/keep) */
unsigned int __gpufreq_get_dvfs_state(void)
{
	return g_dvfs_state;
}

/* API: get GPU shader stack */
unsigned int __gpufreq_get_shader_present(void)
{
	return g_shader_present;
}

/* API: get current Freq of GPU */
unsigned int __gpufreq_get_cur_fgpu(void)
{
	return g_gpu.cur_freq;
}

/* API: get current Freq of STACK */
unsigned int __gpufreq_get_cur_fstack(void)
{
	return 0;
}

/* API: get current Volt of GPU */
unsigned int __gpufreq_get_cur_vgpu(void)
{
	return g_gpu.buck_count ? g_gpu.cur_volt : 0;
}

/* API: get current Volt of STACK */
unsigned int __gpufreq_get_cur_vstack(void)
{
	return 0;
}

/* API: get current Vsram of GPU */
unsigned int __gpufreq_get_cur_vsram_gpu(void)
{
	return g_gpu.cur_vsram;
}

/* API: get current Vsram of STACK */
unsigned int __gpufreq_get_cur_vsram_stack(void)
{
	return 0;
}

/* API: get current Power of GPU */
unsigned int __gpufreq_get_cur_pgpu(void)
{
	return g_gpu.working_table[g_gpu.cur_oppidx].power;
}

/* API: get current Power of STACK */
unsigned int __gpufreq_get_cur_pstack(void)
{
	return 0;
}

/* API: get max Power of GPU */
unsigned int __gpufreq_get_max_pgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].power;
}

/* API: get max Power of STACK */
unsigned int __gpufreq_get_max_pstack(void)
{
	return 0;
}

/* API: get min Power of GPU */
unsigned int __gpufreq_get_min_pgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].power;
}

/* API: get min Power of STACK */
unsigned int __gpufreq_get_min_pstack(void)
{
	return 0;
}

/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return g_gpu.cur_oppidx;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return g_gpu.opp_num;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return 0;
}


/* API: get number of signed OPP of GPU */
int __gpufreq_get_signed_opp_num_gpu(void)
{
	return g_gpu.signed_opp_num;
}

/* API: get number of signed OPP of STACK */
int __gpufreq_get_signed_opp_num_stack(void)
{
	return 0;
}

/* API: get poiner of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}


/* API: get poiner of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}


/* API: get debug info of GPU for Proc show */
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gpu(void)
{
	struct gpufreq_debug_opp_info opp_info = {};

	mutex_lock(&gpufreq_lock);
	opp_info.cur_oppidx = g_gpu.cur_oppidx;
	opp_info.cur_freq = g_gpu.cur_freq;
	opp_info.cur_volt = g_gpu.cur_volt;
	opp_info.cur_vsram = g_gpu.cur_vsram;
	opp_info.power_count = g_gpu.power_count;
	opp_info.cg_count = g_gpu.cg_count;
	opp_info.mtcmos_count = g_gpu.mtcmos_count;
	opp_info.buck_count = g_gpu.buck_count;
	opp_info.segment_id = g_gpu.segment_id;
	opp_info.opp_num = g_gpu.opp_num;
	opp_info.signed_opp_num = g_gpu.signed_opp_num;
	opp_info.dvfs_state = g_dvfs_state;
	opp_info.shader_present = g_shader_present;
	opp_info.aging_enable = g_aging_enable;
	if (__gpufreq_get_power_state()) {
		opp_info.fmeter_freq = __gpufreq_get_fmeter_fgpu();
		opp_info.con1_freq = __gpufreq_get_real_fgpu();
		opp_info.regulator_volt = __gpufreq_get_real_vgpu();
		opp_info.regulator_vsram = __gpufreq_get_real_vsram();
	} else {
		opp_info.fmeter_freq = 0;
		opp_info.con1_freq = 0;
		opp_info.regulator_volt = 0;
		opp_info.regulator_vsram = 0;
	}
	mutex_unlock(&gpufreq_lock);

	return opp_info;
}

/* API: get opp idx in original opp tables. */
/* This is usually for ptpod use. */
unsigned int __gpufreq_get_ptpod_opp_idx(unsigned int idx)
{
	if (idx < PTPOD_OPP_GPU_NUM && idx >= 0)
		return g_ptpod_opp_idx_table[idx];
	else
		return idx;

}

/*
 * set AUTO_MODE or PWM_MODE to PMIC(VGPU)
 * REGULATOR_MODE_FAST: PWM Mode
 * REGULATOR_MODE_NORMAL: Auto Mode
 */
static void __mt_gpufreq_vgpu_set_mode(unsigned int mode)
{
	int ret;

	ret = regulator_set_mode(g_pmic->reg_vgpu, mode);
	if (ret == 0) {
		GPUFREQ_LOGD("set AUTO_MODE(%d) or PWM_MODE(%d) to PMIC(VGPU), mode = %d\n",
				REGULATOR_MODE_NORMAL, REGULATOR_MODE_FAST, mode);
	} else {
		GPUFREQ_LOGE("failed to configure mode, ret = %d, mode = %d\n", ret, mode);
	}
}

static void __gpufreq_volt_switch_by_ptpod(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int vsram_volt_new, vsram_volt_old;
	int ret = GPUFREQ_SUCCESS;
	bool g_fixed_freq_volt_state = g_dvfs_state & DVFS_FIX_FREQ_VOLT;
	volt_new = VOLT_NORMALIZATION(volt_new);

	GPUFREQ_LOGD("volt_new = %d, volt_old = %d\n", volt_new, volt_old);

	mutex_lock(&gpufreq_lock);
	if(__gpufreq_get_power_state() && !g_fixed_freq_volt_state){

		vsram_volt_new = __gpufreq_get_vsram_by_vgpu(volt_new);
		vsram_volt_old = __gpufreq_get_vsram_by_vgpu(volt_old);

			/* voltage scaling */
		ret = __gpufreq_volt_scale_gpu(
			volt_old, volt_new, vsram_volt_old, vsram_volt_new);

		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				volt_old, volt_new, vsram_volt_old, vsram_volt_new);
		} else {
			g_gpu.cur_volt = volt_new;
			g_gpu.cur_vsram = vsram_volt_new;
		}
	}
	mutex_unlock(&gpufreq_lock);
}

/*
 * interpolation none PTPOP.
 *
 * step = (large - small) / range
 * vnew = large - step * j
 */
void mt_gpufreq_update_volt_interpolation(void)
{
	int i, j, largeOppIndex, smallOppIndex, range, freq, vnew;
	int large_vgpu, small_vgpu, large_freq, small_freq, slope;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	for (i = 1; i < PTPOD_OPP_GPU_NUM; i++) {
		largeOppIndex = __gpufreq_get_ptpod_opp_idx(i - 1);
		smallOppIndex = __gpufreq_get_ptpod_opp_idx(i);
		range = smallOppIndex - largeOppIndex;

		large_vgpu = signed_table[largeOppIndex].volt;
		large_freq = signed_table[largeOppIndex].freq / 1000;

		small_vgpu = signed_table[smallOppIndex].volt;
		small_freq = signed_table[smallOppIndex].freq / 1000;

		slope = (large_vgpu - small_vgpu) / (large_freq - small_freq);

		if (slope < 0) {
			dump_stack();
			/* todo: gpu_assert */
		}

		for (j = 1; j < range; j++) {
			freq = signed_table[largeOppIndex + j].freq
				/ 1000;
			vnew = small_vgpu + slope * (freq - small_freq);
			vnew = VOLT_NORMALIZATION(vnew);
			/* todo: gpu_assert */

			signed_table[largeOppIndex + j].volt = vnew;
			signed_table[largeOppIndex + j].vsram =
				__gpufreq_get_vsram_by_vgpu(vnew);
		}
	}
}

/*
 * API : update OPP and switch back to default voltage setting
 */
void mt_gpufreq_restore_default_volt(void)
{
	int i;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	mutex_lock(&ptpod_lock);

	GPUFREQ_LOGD("restore OPP table to default voltage\n");

	for (i = 0; i < g_gpu.signed_opp_num; i++) {
		signed_table[i].volt = g_default_gpu[i].volt;
		signed_table[i].vsram = g_default_gpu[i].vsram;

		GPUFREQ_LOGD("signed_table[%d].volt = %d, vsram = %d\n",
				i,
				signed_table[i].volt,
				signed_table[i].vsram);
	}

	__gpufreq_update_gpu_working_table();

	if (g_aging_enable)
		__gpufreq_apply_aging(true);

	__gpufreq_set_springboard();

	__gpufreq_volt_switch_by_ptpod(g_gpu.cur_volt,
		g_gpu.working_table[g_gpu.cur_oppidx].volt);

	mutex_unlock(&ptpod_lock);
}
EXPORT_SYMBOL(mt_gpufreq_restore_default_volt);

/*
 * API : get current voltage
 */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return (__gpufreq_get_power_state() == GPU_PWR_ON) ? g_gpu.cur_volt : 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* API : get frequency via OPP table index */
unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	return __gpufreq_get_fgpu_by_idx(idx);
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);

/* API: get opp idx in original opp tables */
/* This is usually for ptpod use */
unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx)
{

	unsigned int ptpod_opp_idx_num;

	ptpod_opp_idx_num = ARRAY_SIZE(g_ptpod_opp_idx_table);

	if (idx < ptpod_opp_idx_num && idx >= 0)
		return g_ptpod_opp_idx_table[idx];
	else
		return idx;

}
EXPORT_SYMBOL(mt_gpufreq_get_ori_opp_idx);

/* API : get voltage via OPP table real index */
unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx)
{
	if (idx < g_gpu.signed_opp_num)
		return g_gpu.signed_table[idx].volt;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_volt_by_real_idx);

/* API : get frequency via OPP table real index */
unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx)
{
	if (idx < g_gpu.signed_opp_num)
		return g_gpu.signed_table[idx].freq;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_real_idx);

/*
 * API : update OPP and set voltage
 * because PTPOD modified voltage table by PMIC wrapper
 */
unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size)
{
	int i;
	int target_idx;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	//__mt_gpufreq_update_table_by_asensor();

	mutex_lock(&ptpod_lock);

	GPUFREQ_LOGD("%s : update OPP table to given voltage\n",__func__);

	for (i = 0; i < array_size; i++) {
		target_idx = __gpufreq_get_ptpod_opp_idx(i);
		signed_table[target_idx].volt = pmic_volt[i];
		signed_table[target_idx].vsram =
		__gpufreq_get_vsram_by_vgpu(pmic_volt[i]);
	}

	// update none PTP
	mt_gpufreq_update_volt_interpolation();

	if (g_aging_enable)
		__gpufreq_apply_aging(true);
	else
		__gpufreq_set_springboard();

	__gpufreq_update_gpu_working_table();

	/* update volt if powered */
	__gpufreq_volt_switch_by_ptpod(
		g_gpu.cur_volt,
		signed_table[g_gpu.cur_oppidx].volt);

	mutex_unlock(&ptpod_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_update_volt);

/*
 * API : set gpu wrap function pointer
 */

void mt_gpufreq_set_gpu_wrap_fp(int (*gpu_wrap_fp)(void))
{
	mt_gpufreq_wrap_fp = gpu_wrap_fp;
}
EXPORT_SYMBOL(mt_gpufreq_set_gpu_wrap_fp);

/*
 * API : get current segment max opp index
 */
unsigned int mt_gpufreq_get_seg_max_opp_index(void)
{
	return g_gpu.segment_upbound;
}
EXPORT_SYMBOL(mt_gpufreq_get_seg_max_opp_index);

/* API : get OPP table index number */
/* need to sub g_segment_max_opp_idx to map to real idx */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return  g_gpu.segment_lowbound - g_gpu.segment_upbound + 1;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

/* power calculation for power table */
static void __mt_gpufreq_calculate_power(unsigned int idx, unsigned int freq,
			unsigned int volt, unsigned int temp)
{
	unsigned int p_total = 0;
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_volt = 0;
	int p_leakage = 0;

	p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

#ifdef CONFIG_MTK_STATIC_POWER_LEGACY
	p_leakage = mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif /* ifdef CONFIG_MTK_STATIC_POWER_LEGACY */

	p_total = p_dynamic + p_leakage;

	GPUFREQ_LOGD("idx = %d, p_dynamic = %d, p_leakage = %d, p_total = %d, temp = %d\n",
			idx, p_dynamic, p_leakage, p_total, temp);

	g_power_table[idx].gpufreq_power = p_total;

}

/*
 * OPP power table initialization
 */
static void _gpufreq_setup_opp_power_table(int num)
{
	int i = 0;
	int temp = 0;

	g_power_table = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);

	if (g_power_table == NULL)
		return;

#ifdef CONFIG_THERMAL
	if (mt_gpufreq_wrap_fp)
		temp = mt_gpufreq_wrap_fp() / 1000;
	else
		temp = 40;
#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

	GPUFREQ_LOGD("temp = %d\n", temp);

	if ((temp < -20) || (temp > 125)) {
		GPUFREQ_LOGD("temp < -20 or temp > 125!\n");
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_gpu.signed_table[i].freq;
		g_power_table[i].gpufreq_volt = g_gpu.signed_table[i].volt;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt, temp);

		GPUFREQ_LOGD("[%d], freq_khz = %u, volt = %u, power = %u\n",
				i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
	}
}


/* update OPP power table */
static void __mt_update_gpufreqs_power_table(void)
{
	int i;
	int temp = 0;
	unsigned int freq = 0;
	unsigned int volt = 0;

#ifdef CONFIG_THERMAL
	if (mt_gpufreq_wrap_fp)
		temp = mt_gpufreq_wrap_fp() / 1000;
	else
		temp = 40;

#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

	GPUFREQ_LOGD("temp = %d\n", temp);

	mutex_lock(&gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < g_gpu.signed_opp_num; i++) {
			freq = g_power_table[i].gpufreq_khz;
			volt = g_power_table[i].gpufreq_volt;

			__mt_gpufreq_calculate_power(i, freq, volt, temp);

			GPUFREQ_LOGD("[%d] freq_khz = %d, volt = %d, power = %d\n",
				i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
		}
	} else {
		GPUFREQ_LOGE("temp < -20 or temp > 125, NOT update power table!\n");
	}

	mutex_unlock(&gpufreq_lock);

}

struct mt_gpufreq_power_table_info *mt_gpufreq_get_power_table(void)
{
	return g_power_table;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_table);

unsigned int mt_gpufreq_get_power_table_num(void)
{
	return g_gpu.signed_opp_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_table_num);

#if MT_GPUFREQ_SHADER_PWR_CTL_WA
void _gpufreq_clock_parking_lock(unsigned long *pFlags)
{
	spin_lock_irqsave(&mt_gpufreq_clksrc_parking_lock, *pFlags);
}
EXPORT_SYMBOL(_gpufreq_clock_parking_lock);

void _gpufreq_clock_parking_unlock(unsigned long *pFlags)
{
	spin_unlock_irqrestore(&mt_gpufreq_clksrc_parking_lock, *pFlags);
}
EXPORT_SYMBOL(_gpufreq_clock_parking_unlock);

unsigned int _gpufreq_clock_parking(int clksrc)
{
	/*
	 * This function will be called under the Interrupt-Handler,
	 * so can't implement any mutex-lock behaviors
	 * (that will result the sleep/schedule operations).
	 */

	if (clksrc == CLOCK_MAIN) {
		/* Switch to <&topckgen_clk CLK_TOP_MFGPLL1> */
		/* 0. set reg bit [18] */
		writel(0x00040000, g_topckgen_base + 0x124);

		/* Switch to <&clk26m>: 26MHz */
		/* 1. clr reg bit [19] */
		writel(0x00080000, g_topckgen_base + 0x128);

	} else if (clksrc == CLOCK_SUB) {
		/* Switch to <&topckgen_clk CLK_TOP_MFGPLL4> */
		/* 0. set reg bit [19] */
		writel(0x00080000, g_topckgen_base + 0x124);

		/* Switch to <&topckgen_clk CLK_TOP_MFG_INTERNAL1> */
		/* 1. clr reg bit [18] */
		writel(0x00040000, g_topckgen_base + 0x128);

	} else if (clksrc == CLOCK_SUB2) {
		/* Switch to <&clk26m>: 26MHz */
		/* 0. clr reg bit [19] */
		writel(0x00080000, g_topckgen_base + 0x128);

		/* Switch to <&topckgen_clk CLK_TOP_MFG_INTERNAL1> */
		/* 1. clr reg bit [18] */
		writel(0x00040000, g_topckgen_base + 0x128);
	}

	GPUFREQ_LOGD("%s, clksrc: %d ... [MUX] = 0x%08X\n",
		__func__, clksrc, readl(g_topckgen_base + 0x120));

	return 0;
}
EXPORT_SYMBOL(_gpufreq_clock_parking);
#endif

/* API: get Freq of GPU via OPP index */
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].freq;
	else
		return 0;
}


/* API: get Power of GPU via OPP index */
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].power;
	else
		return 0;
}


/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given freq */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].freq >= freq)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);

	return 0;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given volt */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].volt >= volt)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return -1;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given power */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].power >= power)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);

	return -1;
}

/* API: get Volt of SRAM via volt of GPU */
unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int vgpu)
{
	unsigned int vsram;

	if (vgpu > VSRAM_FIXED_THRESHOLD)
		vsram = vgpu + VSRAM_FIXED_DIFF;
	else
		vsram = VSRAM_FIXED_VOLT;

	return vsram;
}

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt, int temper)
{
	GPUFREQ_UNREFERENCED(volt);
	GPUFREQ_UNREFERENCED(temper);

	return GPU_LEAKAGE_POWER;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = GPU_ACT_REF_POWER;
	unsigned int ref_freq = GPU_ACT_REF_FREQ;
	unsigned int ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) *
		((volt * 100) / ref_volt) /
		(100 * 100 * 100);

	return p_dynamic;
}

/*
 * kick Power Budget Manager(PBM) when OPP changed
 */
static void __gpufreq_kick_pbm(int enable)
{
#if PBM_RAEDY
	unsigned int power;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int found = 0;
	int tmp_idx = -1;
	int i;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;

	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;

	if (enable) {
		for (i = 0; i < g_gpu.opp_num; i++) {
			if (working_table[i].freq == cur_freq) {
				/* record idx since current voltage may not in DVFS table */
				tmp_idx = i;

				if (working_table[i].volt == cur_volt) {
					power = working_table[i].power;
					found = 1;
					kicker_pbm_by_gpu(true, power, cur_volt / 100);
					GPUFREQ_LOGD("request GPU power = %d,
						cur_volt = %d uV,
						cur_freq = %d KHz\n",
						power, cur_volt * 10, cur_freq);
					return;
				}
			}
		}

		if (!found) {
			GPUFREQ_LOGD("tmp_idx = %d\n", tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_gpu.opp_num) {
				/* use freq to found corresponding power budget */
				power = working_table[tmp_idx].power;
				kicker_pbm_by_gpu(true, power, cur_volt / 100);
				GPUFREQ_LOGD("request GPU power = %d, cur_volt = %d uV,
					cur_freq = %d KHz\n", power, cur_volt * 10, cur_freq);
			} else {
				GPUFREQ_LOGD("Cannot found request power in power table,
					cur_freq = %d KHz, cur_volt = %d uV\n",
					cur_freq, cur_volt * 10);
			}
		}
	} else {
		kicker_pbm_by_gpu(false, 0, cur_volt / 100);
	}
#endif
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	int ret = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("switch power: %s (Power: %d, Buck: %d, MTCMOS: %d, CG: %d)",
			power ? "On" : "Off",
			g_gpu.power_count, g_gpu.buck_count,
			g_gpu.mtcmos_count, g_gpu.cg_count);

	if (power == GPU_PWR_ON)
		g_gpu.power_count++;
	else
		g_gpu.power_count--;

	__gpufreq_footprint_power_count(g_gpu.power_count);

	if (power == GPU_PWR_ON && g_gpu.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* control Buck */
		ret = __gpufreq_buck_control(GPU_PWR_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x02);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(GPU_PWR_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x03);

		/* control clock */
		ret = __gpufreq_clock_control(GPU_PWR_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x04);

		/* config ocl timestamp */
		__gpufreq_set_timestamp();
		__gpufreq_footprint_power_step(0x05);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_kick_pbm(1);
	} else if (power == GPU_PWR_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(0x06);
		/* check all transaction complete before power off */
		__gpufreq_check_bus_idle();
		__gpufreq_footprint_power_step(0x07);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x08);

		/* control clock */
		ret = __gpufreq_clock_control(GPU_PWR_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x09);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(GPU_PWR_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x0A);

		/* control Buck */
		ret = __gpufreq_buck_control(GPU_PWR_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}

		__gpufreq_footprint_power_step(0x0B);

		__gpufreq_kick_pbm(0);
	}

	/* return power count if successfully control power */
	ret = g_gpu.power_count;

done_unlock:
	GPUFREQ_LOGD("PWR_STATUS DONE");

	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: commit DVFS to GPU by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	struct gpufreq_opp_info *opp_table = g_gpu.working_table;
	struct gpufreq_sb_info *sb_table = g_gpu.sb_table;
	int opp_num = g_gpu.opp_num;
	int cur_oppidx = 0;
	unsigned int cur_freq = 0, target_freq = 0;
	unsigned int cur_volt = 0, target_volt = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;
	int sb_idx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d",
		target_oppidx, key);

	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx < 0 || target_oppidx >= opp_num) {
		GPUFREQ_LOGE("invalid target GPU OPP index: %d (OPP_NUM: %d)",
			target_oppidx, opp_num);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable dvfs state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* randomly replace target index */
	if (g_stress_test_enable) {
		get_random_bytes(&target_oppidx, sizeof(target_oppidx));
		target_oppidx = target_oppidx < 0 ?
			(target_oppidx*-1) % opp_num : target_oppidx % opp_num;
	}

	cur_oppidx = g_gpu.cur_oppidx;
	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_freq = opp_table[target_oppidx].freq;
	target_volt = opp_table[target_oppidx].volt;
	target_vsram = opp_table[target_oppidx].vsram;

	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d)",
		cur_oppidx, target_oppidx);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

	if (target_freq == cur_freq) {
		/* voltage scaling */
		ret = __gpufreq_volt_scale_gpu(
			cur_volt, target_volt, cur_vsram, target_vsram);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				cur_volt, target_volt, cur_vsram, target_vsram);
			goto done_unlock;
		}
	} else if (target_freq > cur_freq) {
		/* voltage scaling */
		while (target_volt != cur_volt) {
			sb_idx = target_oppidx > sb_table[cur_oppidx].up ?
				target_oppidx : sb_table[cur_oppidx].up;

			ret = __gpufreq_volt_scale_gpu(
				cur_volt, opp_table[sb_idx].volt,
				cur_vsram, opp_table[sb_idx].vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, opp_table[sb_idx].volt,
					cur_vsram, opp_table[sb_idx].vsram);
				goto done_unlock;
			}

			cur_oppidx = sb_idx;
			cur_volt = opp_table[sb_idx].volt;
			cur_vsram = opp_table[sb_idx].vsram;
		}
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
	} else {
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
		/* voltage scaling */
		while (target_volt != cur_volt) {
			sb_idx = target_oppidx < sb_table[cur_oppidx].down ?
				target_oppidx : sb_table[cur_oppidx].down;

			ret = __gpufreq_volt_scale_gpu(
				cur_volt, opp_table[sb_idx].volt,
				cur_vsram, opp_table[sb_idx].vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, opp_table[sb_idx].volt,
					cur_vsram, opp_table[sb_idx].vsram);
				goto done_unlock;
			}

			cur_oppidx = sb_idx;
			cur_volt = opp_table[sb_idx].volt;
			cur_vsram = opp_table[sb_idx].vsram;
		}
	}

	g_gpu.cur_oppidx = target_oppidx;

	__gpufreq_footprint_oppidx(target_oppidx);

	__gpufreq_kick_pbm(1);

done_unlock:
	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: commit DVFS to STACK by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx_gpu);
	GPUFREQ_UNREFERENCED(target_oppidx_stack);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/* API: fix OPP of GPU via given OPP index */
int __gpufreq_fix_target_oppidx_gpu(int oppidx)
{
	int opp_num = g_gpu.opp_num;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
		goto done;
	}

	if (oppidx == -1) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_OPP);

		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_FIX_OPP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)", oppidx, ret);
			__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
		}
	} else {
		GPUFREQ_LOGE("invalid fixed OPP index: %d", oppidx);
		ret = GPUFREQ_EINVAL;
	}

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	return ret;
}

/* API: fix Freq and Volt of GPU via given Freq and Volt */
int __gpufreq_fix_custom_freq_volt_gpu(unsigned int freq, unsigned int volt)
{
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
		goto done;
	}

	if (freq == 0 && volt == 0) {
		mutex_lock(&gpufreq_lock);
		g_dvfs_state &= ~DVFS_FIX_FREQ_VOLT;
		mutex_unlock(&gpufreq_lock);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > POSDIV_2_MAX_FREQ || freq < POSDIV_8_MIN_FREQ) {
		GPUFREQ_LOGE("invalid fixed freq: %d\n", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > VGPU_MAX_VOLT || volt < VGPU_MIN_VOLT) {
		GPUFREQ_LOGE("invalid fixed volt: %d\n", volt);
		ret = GPUFREQ_EINVAL;
	} else {
		mutex_lock(&gpufreq_lock);
		g_dvfs_state |= DVFS_FIX_FREQ_VOLT;
		mutex_unlock(&gpufreq_lock);

		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_FIX_FREQ_VOLT);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to custom commit GPU freq: %d, volt: %d (%d)",
				freq, volt, ret);
			mutex_lock(&gpufreq_lock);
			g_dvfs_state &= ~DVFS_FIX_FREQ_VOLT;
			mutex_unlock(&gpufreq_lock);
		}
	}

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	return ret;
}


void __gpufreq_set_timestamp(void)
{
	/* Write 1 into 0x13000130 bit 0 to enable timestamp register (TIMESTAMP).*/
	/* TIMESTAMP will be used by clGetEventProfilingInfo.*/
	writel(0x00000003, g_MFG_base + 0x130);
}

void __gpufreq_check_bus_idle(void)
{
	u32 val;

	/* MFG_QCHANNEL_CON (0x130000b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_MFG_base + 0xb4);
	GPUFREQ_LOGD("0x130000b4 val = 0x%x\n", readl(g_MFG_base + 0xb4));

	/* set register MFG_DEBUG_SEL (0x13000170) bit [7:0] = 0x03 */
	writel(0x00000003, g_MFG_base + 0x170);
	GPUFREQ_LOGD("0x13000170 val = 0x%x\n", readl(g_MFG_base + 0x170));

	/* polling register MFG_DEBUG_TOP (0x13000178) bit 2 = 0x1 */
	/* => 1 for GPU (BUS) idle, 0 for GPU (BUS) non-idle */
	/* do not care about 0x13000174 */
	do {
		val = readl(g_MFG_base + 0x178);
		GPUFREQ_LOGD("0x13000178 val = 0x%x\n", val);
	} while ((val & 0x4) != 0x4);
}

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6877)
	u32 val = 0;

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	//todo mark for build pass
//	GPUFREQ_LOGI("mfgpll=%d, GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
//		mt_get_abist_freq(AD_MFGPLL_CK), g_gpu.cur_oppidx, g_gpu.cur_freq,
//		g_gpu.cur_volt, g_gpu.cur_vsram);
#endif /* IS_ENABLED(CONFIG_COMMON_CLK_MT6877) */
}

/* API: get working OPP index of GPU limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
#if (GPUFREQ_BATT_OC_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING))
	if (batt_oc_level == BATTERY_OC_LEVEL_1)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_OC_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_oc_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_OC_ENABLE && CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */
}

/* API: get working OPP index of GPU limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
#if (GPUFREQ_BATT_PERCENT_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING))
	if (batt_percent_level == BATTERY_PERCENT_LEVEL_1)
		return GPUFREQ_BATT_PERCENT_IDX - g_gpu.segment_upbound;
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_percent_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_PERCENT_ENABLE && CONFIG_MTK_BATTERY_PERCENT_THROTTLING */
}

/* API: get working OPP index of GPU limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
#if (GPUFREQ_LOW_BATT_ENABLE && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING))
	if (low_batt_level == LOW_BATTERY_LEVEL_2)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_LOW_BATT_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(low_batt_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_LOW_BATT_ENABLE && CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */
}

/* API: enable/disable random OPP index substitution to do stress test */
void __gpufreq_set_stress_test(unsigned int mode)
{
	g_stress_test_enable = mode;
}

/* API: apply/restore Vaging to working table of GPU */
int __gpufreq_set_aging_mode(unsigned int mode)
{
	/* prevent from repeatedly applying aging */
	if (g_aging_enable ^ mode) {
		__gpufreq_apply_aging(mode);
		g_aging_enable = mode;

		/* set power info to working table */
		__gpufreq_measure_power();

		return GPUFREQ_SUCCESS;
	} else {
		return GPUFREQ_EINVAL;
	}
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static unsigned int __gpufreq_custom_init_enable(void)
{
	return GPUFREQ_CUST_INIT_ENABLE;
}

static unsigned int __gpufreq_dvfs_enable(void)
{
	return GPUFREQ_DVFS_ENABLE;
}

/* API: set/reset DVFS state with lock */
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state)
{
	mutex_lock(&gpufreq_lock);
	if (set)
		g_dvfs_state |= state;
	else
		g_dvfs_state &= ~state;
	mutex_unlock(&gpufreq_lock);
}

/*
 * API: commit DVFS to GPU by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	unsigned int cur_freq = 0, cur_volt = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;
	unsigned int sb_volt = 0, sb_vsram = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_freq=%d, target_volt=%d, key=%d",
		target_freq, target_volt, key);

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGI("unavailable dvfs state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_vsram = __gpufreq_get_vsram_by_vgpu(target_volt);

	GPUFREQ_LOGD("begin to custom commit freq: (%d->%d), volt: (%d->%d)",
		cur_freq, target_freq, cur_volt, target_volt);

	if (target_freq == cur_freq) {
		/* voltage scaling */
		ret = __gpufreq_volt_scale_gpu(
			cur_volt, target_volt, cur_vsram, target_vsram);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				cur_volt, target_volt, cur_vsram, target_vsram);
			goto done_unlock;
		}
	} else if (target_freq > cur_freq) {
		/* voltage scaling */
		while (target_volt != cur_volt) {
			if ((target_vsram - cur_volt) > MAX_BUCK_DIFF) {
				sb_volt = cur_volt + MAX_BUCK_DIFF;
				sb_vsram = __gpufreq_get_vsram_by_vgpu(sb_volt);
			} else {
				sb_volt = target_volt;
				sb_vsram = target_vsram;
			}

			ret = __gpufreq_volt_scale_gpu(
				cur_volt, sb_volt, cur_vsram, sb_vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, sb_volt, cur_vsram, sb_vsram);
				goto done_unlock;
			}

			cur_volt = sb_volt;
			cur_vsram = sb_vsram;
		}
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
	} else {
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
		/* voltage scaling */
		while (target_volt != cur_volt) {
			if ((cur_vsram - target_volt) > MAX_BUCK_DIFF) {
				sb_volt = cur_volt - MAX_BUCK_DIFF;
				sb_vsram = __gpufreq_get_vsram_by_vgpu(sb_volt);
			} else {
				sb_volt = target_volt;
				sb_vsram = target_vsram;
			}

			ret = __gpufreq_volt_scale_gpu(
				cur_volt, sb_volt, cur_vsram, sb_vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, sb_volt, cur_vsram, sb_vsram);
				goto done_unlock;
			}

			cur_volt = sb_volt;
			cur_vsram = sb_vsram;
		}
	}

	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(target_freq);

	__gpufreq_footprint_oppidx(g_gpu.cur_oppidx);

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (unlikely(ret)) {
		__gpufreq_abort("fail to enable clk_mux(TOP_MUX_MFG) (%d)", ret);
		goto done;
	}

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to switch GPU to CLOCK_MAIN (%d)", ret);
		goto done;
		}
	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to switch GPU to CLOCK_SUB (%d)", ret);
			goto done;
		}
	} else {
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);
		goto done;
	}
	clk_disable_unprepare(g_clk->clk_mux);

done:
	GPUFREQ_TRACE_END();
	return ret;
}

/*
 * API: calculate pcw for setting CON1
 * Fin is 26 MHz
 * VCO Frequency = Fin * N_INFO
 * MFGPLL output Frequency = VCO Frequency / POSDIV
 * N_INFO = MFGPLL output Frequency * POSDIV / FIN
 * N_INFO[21:14] = FLOOR(N_INFO, 8)
 */
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv)
{
	/*
	 * MFGPLL VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 * MFGPLL range: 125MHz - 3.8GHz,
	 * | VCO MAX | VCO MIN | POSDIV | PLL OUT MAX | PLL OUT MIN |
	 * |  3800   |  1500   |    1   |   3800MHz   |   1500MHz   |
	 * |  3800   |  1500   |    2   |   1900MHz   |    750MHz   |
	 * |  3800   |  1500   |    4   |    950MHz   |    375MHz   |
	 * |  3800   |  1500   |    8   |    475MHz   |  187.5MHz   |
	 * |  3800   |  2000   |   16   |  237.5MHz   |    125MHz   |
	 */
	unsigned int pcw = 0;

	/* only use posdiv 2 or 4 */
	if ((freq >= POSDIV_4_MIN_FREQ) &&
		(freq <= POSDIV_2_MAX_FREQ)) {
		pcw = (((freq / TO_MHZ_HEAD * (1 << posdiv))
			<< DDS_SHIFT) /
			MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		GPUFREQ_LOGE("out of range Freq: %d (EINVAL)", freq);
	}

	return pcw;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	mfgpll = readl(MFGPLL1_CON1);

	posdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq)
{
	int i;

	for (i = 0; i < SIGNED_OPP_GPU_NUM; i++) {
		if (g_gpu.signed_table[i].freq <= freq)
			return g_gpu.signed_table[i].posdiv;
	}
	return (freq > POSDIV_4_MAX_FREQ) ? POSDIV_POWER_2 : POSDIV_POWER_4;
}

/* API: scale Freq of GPU via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv __maybe_unused = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	unsigned int parking = false;
	int ret = GPUFREQ_SUCCESS;
	int hopping = -1;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);

#if MT_GPUFREQ_SHADER_PWR_CTL_WA
	unsigned long flags;
#endif

	cur_posdiv = __gpufreq_get_real_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid PCW: 0x%x", pcw);
		goto done;
	}
	/*
	 * MFGPLL1_CON1[31:31] = MFGPLL_SDM_PCW_CHG
	 * MFGPLL1_CON1[26:24] = MFGPLL_POSDIV
	 * MFGPLL1_CON1[21:0]  = MFGPLL_SDM_PCW (dds)
	 */
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;

#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (target_posdiv != cur_posdiv)
		parking = true;
	else
		parking = false;

#else
	/* force parking if FHCTL isn't ready */
	parking = true;
#endif

	if (parking) {
#ifndef GLITCH_FREE
#if MT_GPUFREQ_SHADER_PWR_CTL_WA
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (ret)
			GPUFREQ_LOGI("enable clk_mux(TOP_MUX_MFG) failed:%d\n", ret);

		ret = clk_prepare_enable(g_clk->clk_pll4);
		if (ret)
			GPUFREQ_LOGI("enable clk_pll4 failed:%d\n", ret);

		_gpufreq_clock_parking_lock(&flags);
		_gpufreq_clock_parking(CLOCK_SUB);
#else
		__gpufreq_switch_clksrc(CLOCK_SUB);
#endif

		/*
		 * MFGPLL1_CON1[31:31] = MFGPLL_SDM_PCW_CHG
		 * MFGPLL1_CON1[26:24] = MFGPLL_POSDIV
		 * MFGPLL1_CON1[21:0]  = MFGPLL_SDM_PCW (dds)
		 */
		writel(pll,MFGPLL1_CON1);

		/* PLL spec */
		udelay(20);
#if MT_GPUFREQ_SHADER_PWR_CTL_WA
		_gpufreq_clock_parking(CLOCK_MAIN);
		_gpufreq_clock_parking_unlock(&flags);

		clk_disable_unprepare(g_clk->clk_pll4);
		clk_disable_unprepare(g_clk->clk_mux);
#else
		__gpufreq_switch_clksrc(CLOCK_MAIN);
#endif
#else
		/*
		 * MFGPLL1_CON0[0] = RG_MFGPLL_EN
		 * MFGPLL1_CON0[4] = RG_MFGPLL_GLITCH_FREE_EN
		 */
		if (freq_new > freq_old) {
			/* 1. change PCW (by hopping) */
			hopping = mt_dfs_general_pll(FH_GPU_PLL0, pcw);
			if (hopping != 0)
				GPUFREQ_LOGI("@%s: hopping failing: %d\n",
						__func__, hopping);
			/* 2. change POSDIV (by MFGPLL1_CON1) */
			pll = (readl(MFGPLL1_CON1) & 0xF8FFFFFF) |
				(posdiv_power << POSDIV_SHIFT);
			writel(pll,MFGPLL1_CON1);
			/* 3. wait 20us for MFGPLL Stable */
			udelay(20);
		} else {
			/* 1. change POSDIV (by MFGPLL1_CON1) */
			pll = (readl(MFGPLL1_CON1) & 0xF8FFFFFF) |
				(posdiv_power << POSDIV_SHIFT);
			writel(pll,MFGPLL1_CON1);
			/* 2. wait 20us for MFGPLL Stable */
			udelay(20);
			/* 3. change PCW (by hopping) */
			hopping = mt_dfs_general_pll(FH_GPU_PLL0, pcw);
			if (hopping != 0)
				GPUFREQ_LOGI("@%s: hopping failing: %d\n",
						__func__, hopping);
		}
#endif
	} else {
#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
		GPUFREQ_LOGD("CONFIG_COMMON_CLK_MTK_FREQ_HOPPING clk_set_rate");
		ret = clk_prepare_enable(g_clk->clk_fhctl);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_main_parent (%d)", ret);
			goto done;
		}
		ret = clk_set_rate(g_clk->clk_fhctl, freq_new*1000);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to clk_set_rate (%d)", ret);
			goto done;
		}
		clk_disable_unprepare(g_clk->clk_fhctl);
#endif
	}

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort("inconsistent scaled Fgpu, cur_freq: %d, target_freq: %d",
			g_gpu.cur_freq, freq_new);

	GPUFREQ_LOGD("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, pll);

	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;

	/* notify gpu freq change to DDK */
	mtk_notify_gpu_freq_change(0, freq_new);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_settle_time_vgpu(bool mode, int deltaV)
{
	unsigned int settleTime;
	/* [MT6359P][VGPU]
	 * DVS Rising : delta(mV) / 10(mV/us) + 4(us) + 5(us)
	 * DVS Falling: delta(mV) /  5(mV/us) + 4(us) + 5(us)
	 */

	if (mode) {
		/* rising 10 mV/us */
		settleTime = deltaV / (10 * 100) + 9;
	} else {
		/* falling 10 mV/us */
		settleTime = deltaV / (5 * 100) + 9;
	}
	return settleTime; /* us */
}

static unsigned int __gpufreq_settle_time_vsram(bool mode, int deltaV)
{
	unsigned int settleTime;
	/* [MT6359P][VSRAM_GPU]
	 * DVS Rising : delta(mV) / 12.5(mV/us) + 3(us) + 5(us)
	 * DVS Falling: delta(mV) /    5(mV/us) + 3(us) + 5(us)
	 */

	if (mode) {
		/* rising 10 mV/us */
		settleTime = deltaV / (125 * 10) + 8;
	} else {
		/* falling 5 mV/us */
		settleTime = deltaV / (5 * 100) + 8;
	}
	return settleTime; /* us */
}

/* API: scale vgpu and vsram via PMIC */
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	unsigned int t_settle_vgpu = 0;
	unsigned int t_settle_vsram = 0;
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("vgpu_old=%d, vgpu_new=%d, vsram_old=%d, vsram_new=%d",
		vgpu_old, vgpu_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		vgpu_old, vgpu_new, vsram_old, vsram_new);

	/* volt scaling up */
	if (vgpu_new > vgpu_old) {
		/* scale-up volt */
		t_settle_vgpu =
			__gpufreq_settle_time_vgpu(
				true, (vgpu_new - vgpu_old));
		t_settle_vsram =
			__gpufreq_settle_time_vsram(
				true, (vsram_new - vsram_old));

		ret = regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_new * 10,
				VSRAM_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set VSRAM_G (%d)", ret);
			goto done;
		}

		ret = regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set VGPU (%d)", ret);
			goto done;
		}
	} else if (vgpu_new < vgpu_old) {
		/* scale-down volt */
		t_settle_vgpu =
			__gpufreq_settle_time_vgpu(
				false, (vgpu_old - vgpu_new));
		t_settle_vsram =
			__gpufreq_settle_time_vsram(
				false, (vsram_old - vsram_new));

		ret = regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set VGPU (%d)", ret);
			goto done;
		}

		ret = regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_new * 10,
				VSRAM_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set VSRAM_GPU (%d)", ret);
			goto done;
		}
	} else {
		/* keep volt */
		ret = GPUFREQ_SUCCESS;
		goto done;
	}

	t_settle = (t_settle_vgpu > t_settle_vsram) ?
		t_settle_vgpu : t_settle_vsram;
	udelay(t_settle);

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	if (unlikely(g_gpu.cur_volt != vgpu_new))
		__gpufreq_abort("inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d, vgpu_old: %d",
		g_gpu.cur_volt, vgpu_new, vgpu_old);

	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	if (unlikely(g_gpu.cur_vsram != vsram_new))
		__gpufreq_abort("inconsistent scaled Vsram, cur_vsram: %d, target_vsram: %d, vsram_old: %d",
		g_gpu.cur_vsram, vsram_new, vsram_old);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

	GPUFREQ_LOGD("Vgpu: %d, Vsram: %d, udelay: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram, t_settle);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{

GPUFREQ_LOGI("[TOP] FMETER: %d, CON1: %d",
		__gpufreq_get_fmeter_fgpu(), __gpufreq_get_real_fgpu());

}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6877)
//todo mark for build pass
//	return mt_get_abist_freq(AD_MFGPLL_CK);
//#else
	return 0;
#endif
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fgpu(void)
{
	unsigned int mfgpll = 0;
	unsigned int posdiv_power = 0;
	unsigned int freq = 0;
	unsigned int pcw = 0;

	mfgpll = readl(MFGPLL1_CON1);

	GPUFREQ_LOGD("MFGPLL_CON1 = 0x%x", mfgpll);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >> DDS_SHIFT) /
		(1 << posdiv_power) * TO_MHZ_HEAD;

	return freq;
}

/* API: get real current Vgpu from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;

	return volt;
}

/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram_gpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;

	return volt;
}

static void __gpufreq_external_cg_control(void)
{
	u32 val;

	/* [K] MFG_ASYNC_CON 0x13FB_F020 [22] MEM0_MST_CG_ENABLE = 0x1 */
	/* [B] MFG_ASYNC_CON 0x13FB_F020 [23] MEM0_SLV_CG_ENABLE = 0x1 */
	/* [K] MFG_ASYNC_CON 0x13FB_F020 [24] MEM1_MST_CG_ENABLE = 0x1 */
	/* [B] MFG_ASYNC_CON 0x13FB_F020 [25] MEM1_SLV_CG_ENABLE = 0x1 */
	val = readl(g_MFG_base + 0x20);
	val |= (1UL << 22);
	val |= (1UL << 23);
	val |= (1UL << 24);
	val |= (1UL << 25);
	writel(val, g_MFG_base + 0x20);

	/* [A] MFG_GLOBAL_CON 0x13FB_F0B0 [8] MFG_SOC_IN_AXI_FREE_RUN = 0x0 */
	/* [C] MFG_GLOBAL_CON 0x13FB_F0B0 [20] RG_FAXI_CK_SOC_IN_FREE_RUN = 0x0 */
	/* [I] MFG_GLOBAL_CON 0x13FB_F0B0 [21] DVFS_HINT_CG_EN = 0x0 */
	/* [L] MFG_GLOBAL_CON 0x13FB_F0B0 [10] GPU_CLK_FREE_RUN = 0x0 */
	val = readl(g_MFG_base + 0xB0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 20);
	val &= ~(1UL << 21);
	val &= ~(1UL << 10);
	writel(val, g_MFG_base + 0xB0);

	/* [G] MFG_DCM_CON 0x13FB_F010 [15] BG3D_DCM_ENABLE = 0x1 */
	/* [G] MFG_DCM_CON 0x13FB_F010 [6:0] BG3D_DCM_DBC_CNT = 0x0111111 */
	val = readl(g_MFG_base + 0x10);
	val |= (1UL << 15);
	val &= ~(1UL << 6);
	val |= (1UL << 5);
	val |= (1UL << 4);
	val |= (1UL << 3);
	val |= (1UL << 2);
	val |= (1UL << 1);
	val |= (1UL << 0);
	writel(val, g_MFG_base + 0x10);
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		ret = clk_prepare_enable(g_clk->subsys_bg3d);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable subsys_mfg_cg (%d)", ret);
			goto done;
		}
		__gpufreq_external_cg_control();

		g_gpu.cg_count++;
	} else {
		clk_disable_unprepare(g_clk->subsys_bg3d);
		g_gpu.cg_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;
	unsigned int shader_present = 0;

	GPUFREQ_TRACE_START("power=%d", power);
	shader_present = __gpufreq_init_shader_present();

	if (power == GPU_PWR_ON) {
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x70 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		ret = clk_prepare_enable(g_clk->mtcmos_mfg0);
		if (ret) {
			__gpufreq_abort("fail to enable mtcmos_mfg0 (%d)", ret);
			goto done;
		}
		ret = clk_prepare_enable(g_clk->mtcmos_mfg1);
		if (ret) {
			__gpufreq_abort("fail to enable mtcmos_mfg1 (%d)", ret);
			goto done;
		}
		if (shader_present & MFG2_SHADER_STACK0) {
			ret = clk_prepare_enable(g_clk->mtcmos_mfg2);
			if (ret) {
				__gpufreq_abort("fail to enable mtcmos_mfg2 (%d)", ret);
				goto done;
			}
		}
		if (shader_present & MFG3_SHADER_STACK2) {
			ret = clk_prepare_enable(g_clk->mtcmos_mfg3);
			if (ret) {
				__gpufreq_abort("fail to enable mtcmos_mfg3 (%d)", ret);
				goto done;
			}

		}
		if (shader_present & MFG4_SHADER_STACK4) {
			ret = clk_prepare_enable(g_clk->mtcmos_mfg4);
			if (ret) {
				__gpufreq_abort("fail to enable mtcmos_mfg4 (%d)", ret);
				goto done;
			}
		}
		if (shader_present & MFG5_SHADER_STACK6) {
			ret = clk_prepare_enable(g_clk->mtcmos_mfg5);
			if (ret) {
				__gpufreq_abort("fail to enable mtcmos_mfg5 (%d)", ret);
				goto done;
			}
		}
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x80 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		GPUFREQ_LOGD("enable MTCMOS done\n");

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
	//TODO:GKI check mtcmos power
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
		g_gpu.mtcmos_count++;
	} else {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x90 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		if (shader_present & MFG5_SHADER_STACK6)
			clk_disable_unprepare(g_clk->mtcmos_mfg5);

		if (shader_present & MFG4_SHADER_STACK4)
			clk_disable_unprepare(g_clk->mtcmos_mfg4);

		if (shader_present & MFG3_SHADER_STACK2)
			clk_disable_unprepare(g_clk->mtcmos_mfg3);

		if (shader_present & MFG2_SHADER_STACK0)
			clk_disable_unprepare(g_clk->mtcmos_mfg2);

		clk_disable_unprepare(g_clk->mtcmos_mfg1);
		clk_disable_unprepare(g_clk->mtcmos_mfg0);
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0xA0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		GPUFREQ_LOGD("disable MTCMOS done\n");
		g_gpu.mtcmos_count--;

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
	//TODO:GKI check mtcmos power
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
	}
done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on */
	if (power == GPU_PWR_ON) {
		ret = regulator_enable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VGPU (%d)", ret);
			goto done;
		}
		ret = regulator_enable(g_pmic->reg_vsram_gpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VSRAM_GPU (%d)",
			ret);
			goto done;
		}
		g_gpu.buck_count++;
	/* power off */
	} else {
		ret = regulator_disable(g_pmic->reg_vsram_gpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VSRAM_GPU (%d)",
			ret);
			goto done;
		}
		ret = regulator_disable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: find the longest and valid opp idx can be reached
 * use springboard opp index to avoid buck variation,
 * as the diff between Vgpu and Vsram must be in valid range
 * that is, MIN_BUCK_DIFF <= Vsram - Vgpu <= MAX_BUCK_DIFF
 * and Vsram always >= Vgpu, so we only focus on "MAX_BUCK_DIFF"
 */
static void __gpufreq_set_springboard(void)
{
	struct gpufreq_opp_info *opp_table = g_gpu.working_table;
	int src_idx = 0, dst_idx = 0;
	unsigned int src_vgpu = 0, src_vsram = 0;
	unsigned int dst_vgpu = 0, dst_vsram = 0;

	/* build volt scale-up springboad table */
	/* when volt scale-up: Vsram -> Vgpu */
	for (src_idx = 0; src_idx < g_gpu.opp_num; src_idx++) {
		src_vgpu = opp_table[src_idx].volt;
		/* search from the beginning of opp table */
		for (dst_idx = 0; dst_idx < g_gpu.opp_num; dst_idx++) {
			dst_vsram = opp_table[dst_idx].vsram;
			/* the smallest valid opp idx can be reached */
			if (dst_vsram - src_vgpu <= MAX_BUCK_DIFF) {
				g_gpu.sb_table[src_idx].up = dst_idx;
				break;
			}
		}
		GPUFREQ_LOGD("springboard_up[%02d]: %d",
			src_idx, g_gpu.sb_table[src_idx].up);
	}

	/* build volt scale-down springboad table */
	/* when volt scale-down: Vgpu -> Vsram */
	for (src_idx = 0; src_idx < g_gpu.opp_num; src_idx++) {
		src_vsram = opp_table[src_idx].vsram;
		/* search from the end of opp table */
		for (dst_idx = g_gpu.min_oppidx; dst_idx >= 0 ; dst_idx--) {
			dst_vgpu = opp_table[dst_idx].volt;
			/* the largest valid opp idx can be reached */
			if (src_vsram - dst_vgpu <= MAX_BUCK_DIFF) {
				g_gpu.sb_table[src_idx].down = dst_idx;
				break;
			}
		}
		GPUFREQ_LOGD("springboard_down[%02d]: %d",
			src_idx, g_gpu.sb_table[src_idx].down);
	}
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	unsigned int cur_freq = 0;
	int oppidx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	cur_freq = __gpufreq_get_real_fgpu();
	GPUFREQ_LOGI("preloader init freq: %d", cur_freq);

	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable()) {
		oppidx = GPUFREQ_CUST_INIT_OPPIDX;
		GPUFREQ_LOGI("custom init GPU OPP index: %d, Freq: %d",
			oppidx, working_table[oppidx].freq);
	/* decide first OPP idx by SRAMRC setting */
	} else {
		/* Restrict freq to legal opp idx */
		if (cur_freq >= working_table[0].freq) {
			oppidx = 0;
		} else if (cur_freq <= working_table[g_gpu.min_oppidx].freq) {
			oppidx = g_gpu.min_oppidx;
		/* Mapping freq to the first smaller opp idx */
		} else {
			for (oppidx = 1; oppidx < g_gpu.opp_num; oppidx++) {
				if (cur_freq >= working_table[oppidx].freq)
					break;
			}
		}
	}

	g_gpu.cur_oppidx = oppidx;
	g_gpu.cur_freq = cur_freq;
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable()) {
			ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_DISABLE);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
					oppidx, ret);
				goto done;
			}
		}
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);

		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_FREE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
				oppidx, ret);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: calculate power of every OPP in working table */
static void __gpufreq_measure_power(void)
{
	unsigned int freq = 0, volt = 0;
	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0;
	int i = 0;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int opp_num = g_gpu.opp_num;

	mutex_lock(&gpufreq_lock);

	for (i = 0; i < opp_num; i++) {
		freq = working_table[i].freq;
		volt = working_table[i].volt;

		p_leakage = __gpufreq_get_lkg_pgpu(volt,30);
		p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_table[i].power = p_total;

		GPUFREQ_LOGD("GPU[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: fix Freq and Volt of GPU via given Freq and Volt */
int __gpufreq_fix_freq_volt_gpu(unsigned int freq, unsigned int volt)
{
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
		goto done;
	}

	if (freq == 0 && volt == 0) {
		ret = GPUFREQ_SUCCESS;
	} else if (freq > POSDIV_2_MAX_FREQ || freq < POSDIV_8_MIN_FREQ) {
		GPUFREQ_LOGE("invalid fixed freq: %d\n", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > VGPU_MAX_VOLT || volt < VGPU_MIN_VOLT) {
		GPUFREQ_LOGE("invalid fixed volt: %d\n", volt);
		ret = GPUFREQ_EINVAL;
	} else {
		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_DISABLE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to custom commit GPU freq: %d, volt: %d (%d)",
				freq, volt, ret);
		}
	}

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	return ret;
}

/* API: apply aging volt diff to working table */
static void __gpufreq_apply_aging(unsigned int apply_aging)
{
	int i = 0;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int opp_num = g_gpu.opp_num;

	mutex_lock(&gpufreq_lock);

	for (i = 0; i < opp_num; i++) {
		if (apply_aging)
			working_table[i].volt -= working_table[i].margin;
		else
			working_table[i].volt += working_table[i].margin;

		working_table[i].vsram = __gpufreq_get_vsram_by_vgpu(working_table[i].volt);

		GPUFREQ_LOGD("apply Vaging: %d, GPU[%02d] Volt: %d, Vsram: %d",
			apply_aging, i, working_table[i].volt, working_table[i].vsram);
	}

	__gpufreq_set_springboard();

	mutex_unlock(&gpufreq_lock);
}

/* API: apply given adjustment table to signed table */
static void __gpufreq_apply_adjust(struct gpufreq_adj_info *adj_table, int adj_num)
{
	int i = 0;
	int oppidx = 0;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	int opp_num = g_gpu.signed_opp_num;

	GPUFREQ_TRACE_START("adj_table=0x%x, adj_num=%d",
		adj_table, adj_num, target);

	if (!adj_table) {
		GPUFREQ_LOGE("null adjustment table (EINVAL)");
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	for (i = 0; i < adj_num; i++) {
		oppidx = adj_table[i].oppidx;
		if (oppidx >= 0 && oppidx < opp_num) {
			signed_table[oppidx].freq = adj_table[i].freq ?
				adj_table[i].freq : signed_table[oppidx].freq;
			signed_table[oppidx].volt = adj_table[i].volt ?
				adj_table[i].volt : signed_table[oppidx].volt;
			signed_table[oppidx].vsram = adj_table[i].vsram ?
				adj_table[i].vsram : signed_table[oppidx].vsram;
		} else {
			GPUFREQ_LOGE("invalid adj_table[%d].oppidx: %d", i, adj_table[i].oppidx);
		}

		GPUFREQ_LOGD("[%02d*] Freq: %d, Volt: %d, Vsram: %d",
			oppidx, signed_table[oppidx].freq,
			signed_table[oppidx].volt,
			signed_table[oppidx].vsram);
	}

	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();
}

static void __gpufreq_aging_adjustment(void)
{
	struct gpufreq_adj_info *aging_adj = NULL;
	int adj_num = 0;
	int i;
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

	adj_num = g_gpu.signed_opp_num;
	/* prepare aging adj */
	aging_adj = kcalloc(adj_num, sizeof(struct gpufreq_adj_info), GFP_KERNEL);
	if (!aging_adj) {
		GPUFREQ_LOGE("fail to alloc gpufreq_adj_info (ENOMEM)");
		return;
	}

	/* Aging margin is set if any OPP is adjusted by Aging */
	if (aging_table_idx == 0)
		g_aging_margin = true;

	/* apply aging to signed table */
	__gpufreq_apply_adjust(aging_adj, adj_num);

	kfree(aging_adj);
}

static int __gpufreq_get_segment_table(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int efuse_id = 0;
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;
#endif

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_ptpod28_cell");
	if (IS_ERR(efuse_cell)) {
		GPUFREQ_LOGE("cannot get efuse_ptpod28_cell\n");
		ret = PTR_ERR(efuse_cell);
		return ret;
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		GPUFREQ_LOGE("cannot get efuse_buf of efuse_ptpod28_cell\n");
		ret = PTR_ERR(efuse_buf);
		return ret;
	}

	efuse_id = (*efuse_buf >> 14 & 0x3);
	kfree(efuse_buf);

#else
	efuse_id = 0x0;
#endif /* CONFIG_MTK_DEVINFO */

	switch (efuse_id) {
	case 0x0: // EFUSE 0x11F105F0[15:14] = 2’b00
		g_default_gpu = g_opp_table_segment_1;
		break;
	case 0x1: // EFUSE 0x11F105F0[15:14] = 2’b01
		g_default_gpu = g_opp_table_segment_3;
		break;
	case 0x2: // EFUSE 0x11F105F0[15:14] = 2’b10
		g_default_gpu = g_opp_table_segment_2;
		break;
	default:
		GPUFREQ_LOGE("@%s: invalid efuse_id(0x%x)\n",
				__func__, efuse_id);
		g_default_gpu = g_opp_table_segment_1;
	}
	GPUFREQ_LOGE("@%s: efuse_id(0x%x)\n",__func__, efuse_id);
	return ret;
}

static void __gpufreq_custom_adjustment(void)
{

}

static void __gpufreq_segment_adjustment(struct platform_device *pdev)
{

}

static int __gpufreq_init_shader_present(void)
{
	g_shader_present = MT_GPU_SHADER_PRESENT_4;
	return g_shader_present;
}

static void __gpufreq_update_gpu_working_table(void)
{
	int i = 0, j = 0;

	mutex_lock(&gpufreq_lock);
	for (i = 0; i < g_gpu.opp_num; i++) {
		j = i + g_gpu.segment_upbound;
		g_gpu.working_table[i].freq = g_gpu.signed_table[j].freq;
		g_gpu.working_table[i].volt = g_gpu.signed_table[j].volt;
		g_gpu.working_table[i].vsram = g_gpu.signed_table[j].vsram;
		g_gpu.working_table[i].posdiv = g_gpu.signed_table[j].posdiv;
		g_gpu.working_table[i].margin = g_gpu.signed_table[j].margin;
		g_gpu.working_table[i].power = g_gpu.signed_table[j].power;

		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].margin);
	}
	mutex_unlock(&gpufreq_lock);

	if (g_aging_enable)
		__gpufreq_apply_aging(true);
	/* set power info to working table */
	__gpufreq_measure_power();

}

/*
 * 1. init working OPP range
 * 2. init working OPP table = default + adjustment
 * 3. init springboard table
 */
static int __gpufreq_init_opp_table(struct platform_device *pdev)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;
	int ret = GPUFREQ_SUCCESS;

	/* init working OPP range */
	segment_id = g_gpu.segment_id;

	/* setup segment max/min opp_idx */
	if (segment_id == MT6877_SEGMENT)
		g_gpu.segment_upbound = 4;
	else if (segment_id == MT6877T_SEGMENT)
		g_gpu.segment_upbound = 0;
	else
		g_gpu.segment_upbound = 4;

	g_gpu.segment_lowbound = SIGNED_OPP_GPU_NUM - 1;
	g_gpu.signed_opp_num = SIGNED_OPP_GPU_NUM;

	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;

	GPUFREQ_LOGD("number of signed GPU OPP: %d, upper and lower bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("number of working GPU OPP: %d, max and min OPP index: [%d, %d]",
		g_gpu.opp_num, g_gpu.max_oppidx, g_gpu.min_oppidx);

	__gpufreq_get_segment_table(pdev);
	g_gpu.signed_table = g_default_gpu;
	/* apply segment adjustment to GPU signed table */
	__gpufreq_segment_adjustment(pdev);

	/* after these, signed table is settled down */
	g_gpu.working_table = kcalloc(g_gpu.opp_num, sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_gpu.working_table) {
		GPUFREQ_LOGE("fail to alloc gpufreq_opp_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	for (i = 0; i < g_gpu.opp_num; i++) {
		j = i + g_gpu.segment_upbound;
		g_gpu.working_table[i].freq = g_gpu.signed_table[j].freq;
		g_gpu.working_table[i].volt = g_gpu.signed_table[j].volt;
		g_gpu.working_table[i].vsram = g_gpu.signed_table[j].vsram;
		g_gpu.working_table[i].posdiv = g_gpu.signed_table[j].posdiv;
		g_gpu.working_table[i].margin = g_gpu.signed_table[j].margin;
		g_gpu.working_table[i].power = g_gpu.signed_table[j].power;

		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].margin);
	}

	/* set power info to working table */
	__gpufreq_measure_power();

	/* init springboard table */
	g_gpu.sb_table = kcalloc(g_gpu.opp_num,
		sizeof(struct gpufreq_sb_info), GFP_KERNEL);
	if (!g_gpu.sb_table) {
		GPUFREQ_LOGE("fail to alloc springboard table (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	__gpufreq_set_springboard();
	_gpufreq_setup_opp_power_table(g_gpu.signed_opp_num);

done:
	return ret;
}

static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

static int __gpufreq_init_segment_id(struct platform_device *pdev)
{
	unsigned int efuse_id = 0;
	unsigned int segment_id = 0;
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;
#endif

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		GPUFREQ_LOGE("cannot get efuse_segment_cell\n");
		ret = PTR_ERR(efuse_cell);
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		GPUFREQ_LOGE("cannot get efuse_buf of efuse segment\n");
	    ret = PTR_ERR(efuse_buf);
	}

	efuse_id = (*efuse_buf & 0xFF);
	kfree(efuse_buf);

#else
	efuse_id = 0x0;
#endif /* CONFIG_MTK_DEVINFO */

	switch (efuse_id) {
	case 0x01:
		segment_id = MT6877_SEGMENT;    /* 5G-5 */
		break;
	case 0x02:
		segment_id = MT6877T_SEGMENT;    /* 5G-5+ */
		break;
	case 0x03:
		segment_id = MT6877T_SEGMENT;    /* 5G-5++ */
		break;
	case 0x04:
		segment_id = MT6877T_SEGMENT;    /* 5G-5+++ */
		break;
	default:
		segment_id = MT6877_SEGMENT;
	}

	GPUFREQ_LOGD("efuse_id = 0x%08X, segment_id = %d\n", efuse_id, segment_id);
	g_gpu.segment_id = segment_id;
	return ret;
}

static int __gpufreq_init_mtcmos(struct platform_device *pdev)
{
	return GPUFREQ_SUCCESS;
}

static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;
	struct fm_pwr_sta mfg_fm_pwr_status = {
		.ofs = PWR_STATUS_OFS,
		.msk = 0x3F,
	};
	struct fm_subsys mfg_fm[] = {
		{
		.id = FM_MFGPLL1,
		.name = "fm_mfgpll1",
		.base = 0,
		.con0 = PLL4H_FQMTR_CON0_OFS,
		.con1 = PLL4H_FQMTR_CON1_OFS,
		.pwr_sta = mfg_fm_pwr_status,
		}
	};

	/* MFGPLL1 & MFGPLL4 are from GPU_PLL_CTRL */
	// 0x13fa0000
	g_gpu_pll_ctrl =
			__gpufreq_of_ioremap(
				"mediatek,mt6877-gpu_pll_ctrl", 0);
	if (!g_gpu_pll_ctrl) {
		GPUFREQ_LOGE("ioremap failed at GPU_PLL_CTRL\n");
		return -ENOENT;
	}

	mfg_fm[0].base = g_gpu_pll_ctrl;
	ret = mt_subsys_freq_register(&mfg_fm[0], ARRAY_SIZE(mfg_fm));
	if (ret) {
		GPUFREQ_LOGE("failed to register fmeter\n");
		return ret;
	}

	// 0x10000000
	g_topckgen_base =
			__gpufreq_of_ioremap("mediatek,topckgen", 0);
	if (!g_topckgen_base) {
		GPUFREQ_LOGE("ioremap failed at topckgen\n");
		return -ENOENT;
	}

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL) {
		GPUFREQ_LOGE("cannot allocate g_clk\n");
		return -ENOMEM;
	}

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		GPUFREQ_LOGE("cannot get clk_mux\n");
		return PTR_ERR(g_clk->clk_mux);
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		GPUFREQ_LOGE("cannot get clk_main_parent\n");
		return PTR_ERR(g_clk->clk_main_parent);
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		GPUFREQ_LOGE("cannot get clk_sub_parent\n");
		return PTR_ERR(g_clk->clk_sub_parent);
	}

	g_clk->clk_pll4 = devm_clk_get(&pdev->dev, "clk_pll4");
	if (IS_ERR(g_clk->clk_pll4)) {
		GPUFREQ_LOGE("@%s: cannot get clk_pll4\n", __func__);
		return PTR_ERR(g_clk->clk_pll4);
	}

	g_clk->subsys_bg3d = devm_clk_get(&pdev->dev, "subsys_bg3d");
	if (IS_ERR(g_clk->subsys_bg3d)) {
		GPUFREQ_LOGE("@%s: cannot get subsys_bg3d\n", __func__);
		return PTR_ERR(g_clk->subsys_bg3d);
	}

	g_clk->mtcmos_mfg0 = devm_clk_get(&pdev->dev, "mtcmos_mfg0");
	if (IS_ERR(g_clk->mtcmos_mfg0)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg0\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg0);
	}

	g_clk->mtcmos_mfg1 = devm_clk_get(&pdev->dev, "mtcmos_mfg1");
	if (IS_ERR(g_clk->mtcmos_mfg1)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg1\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg1);
	}

	g_clk->mtcmos_mfg2 = devm_clk_get(&pdev->dev, "mtcmos_mfg2");
	if (IS_ERR(g_clk->mtcmos_mfg2)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg2\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg2);
	}

	g_clk->mtcmos_mfg3 = devm_clk_get(&pdev->dev, "mtcmos_mfg3");
	if (IS_ERR(g_clk->mtcmos_mfg3)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg3\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg3);
	}

	g_clk->mtcmos_mfg4 = devm_clk_get(&pdev->dev, "mtcmos_mfg4");
	if (IS_ERR(g_clk->mtcmos_mfg4)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg4\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg4);
	}

	g_clk->mtcmos_mfg5 = devm_clk_get(&pdev->dev, "mtcmos_mfg5");
	if (IS_ERR(g_clk->mtcmos_mfg5)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg5\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg5);
	}

	#if MT_GPUFREQ_SHADER_PWR_CTL_WA
		spin_lock_init(&mt_gpufreq_clksrc_parking_lock);
	#endif

	g_clk->clk_fhctl = devm_clk_get(&pdev->dev, "clk_fhctl");
	if (IS_ERR(g_clk->clk_fhctl)) {
		GPUFREQ_LOGE("cannot get clk_fhctl\n");
		return PTR_ERR(g_clk->clk_fhctl);
	}

	// 0x1020E000
	g_infracfg_base = __gpufreq_of_ioremap("mediatek,infracfg", 0);
	if (!g_infracfg_base) {
		GPUFREQ_LOGE("@%s: ioremap failed at infracfg\n", __func__);
		return -ENOENT;
	}

	// 0x10006000
	g_sleep = __gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		GPUFREQ_LOGE("@%s: ioremap failed at sleep\n", __func__);
		return -ENOENT;
	}

	// 0x10001000
	g_infracfg_ao = __gpufreq_of_ioremap("mediatek,infracfg_ao", 0);
	if (!g_infracfg_ao) {
		GPUFREQ_LOGE("@%s: ioremap failed at infracfg_ao\n",
				__func__);
		return -ENOENT;
	}

	// 0x1000d000
	g_dbgtop = __gpufreq_of_ioremap("mediatek,dbgtop-drm", 0);
	if (!g_dbgtop) {
		GPUFREQ_LOGE("@%s: ioremap failed at dbgtop-drm\n", __func__);
		return -ENOENT;
	}

	// 0x10007000
	g_toprgu = __gpufreq_of_ioremap("mediatek,toprgu", 0);
	if (!g_toprgu) {
		GPUFREQ_LOGE("@%s: ioremap failed at toprgu\n", __func__);
		return -ENOENT;
	}

	//todo mark for build pass
	/*
	GPUFREQ_LOGI("clk_mux is at 0x%p, clk_main_parent is at 0x%p, \t"
			"clk_sub_parent is at 0x%p, subsys_mfg_cg is at 0x%p, \t"
			"mtcmos_mfg_async is at 0x%p, mtcmos_mfg is at 0x%p, \t"
			"mtcmos_mfg_core0 is at 0x%p, mtcmos_mfg_core1 is at 0x%p\n",
			g_clk->clk_mux, g_clk->clk_main_parent, g_clk->clk_sub_parent,
			g_clk->subsys_mfg_cg, g_clk->mtcmos_mfg_async, g_clk->mtcmos_mfg,
			g_clk->mtcmos_mfg_core0, g_clk->mtcmos_mfg_core1);
			*/

	return ret;
}

static void __gpufreq_init_acp(void)
{
	unsigned int val;

	/* disable acp */
	val = readl(g_infracfg_ao + 0x290) | (0x1 << 9);
	writel(val, g_infracfg_ao + 0x290);
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL) {
		GPUFREQ_LOGE("cannot allocate g_pmic\n");
		return -ENOMEM;
	}

	g_pmic->reg_vgpu = regulator_get(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		GPUFREQ_LOGE("cannot get VGPU\n");
		return PTR_ERR(g_pmic->reg_vgpu);
	}

	g_pmic->reg_vsram_gpu = regulator_get(&pdev->dev, "vsram-gpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		GPUFREQ_LOGE("cannot get VSRAM_GPU\n");
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}

	return ret;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(gpufreq_dev->of_node, "enable-aging", &g_aging_enable);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);


	/* 13000000 */
	g_MFG_base = __gpufreq_of_ioremap("mediatek,mfgcfg", 0);
	if (unlikely(!g_MFG_base)) {
		GPUFREQ_LOGE("fail to get resource mfgcfg");
		goto done;
	}

	g_apmixed_base = __gpufreq_of_ioremap("mediatek,apmixed", 0);
	if (unlikely(!g_apmixed_base)) {
		GPUFREQ_LOGE("fail to ioremap APMIXED");
		goto done;
	}

	/* 0x1000C000 */

	ret = GPUFREQ_SUCCESS;

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;
	struct device_node *node;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	node = of_find_matching_node(NULL, g_gpufreq_of_match);
	if (!node)
		GPUFREQ_LOGI("@%s: find GPU node failed\n", __func__);

	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		//__gpufreq_dump_bringup_status(pdev);
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}

	/* skip most of probe in EB mode */
	if (g_gpueb_support) {
		GPUFREQ_LOGI("gpufreq platform probe only init reg_base/dfd/pmic/fp in EB mode");
		goto register_fp;
	}

	/* init clock source */
	ret = __gpufreq_init_clk(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init clk (%d)", ret);
		goto done;
	}

	__gpufreq_init_acp();

	/* init mtcmos power domain */
	ret = __gpufreq_init_mtcmos(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init mtcmos (%d)", ret);
		goto done;
	}

	/* init segment id */
	ret = __gpufreq_init_segment_id(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init segment id (%d)", ret);
		goto done;
	}

	/* init shader present */
	__gpufreq_init_shader_present();

	/* power on to init first OPP index */
	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
		goto done;
	}

	/* init OPP table */
	ret = __gpufreq_init_opp_table(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP table (%d)", ret);
		goto done;
	}

	if (g_aging_enable)
		__gpufreq_apply_aging(true);

	/* init first OPP index by current freq and volt */
	ret = __gpufreq_init_opp_idx();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP index (%d)", ret);
		goto done;
	}

	/* power off after init first OPP index */
	if (__gpufreq_power_ctrl_enable())
		__gpufreq_power_control(GPU_PWR_OFF);
	else
		/* never power off if power control is disabled */
		GPUFREQ_LOGI("power control always on");

	notify_gpufreq_probe_done();

register_fp:
	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	if (g_gpueb_support)
		gpufreq_register_gpufreq_fp(&platform_eb_fp);
	else
		gpufreq_register_gpufreq_fp(&platform_ap_fp);

	/* init gpu ppm */
	ret = gpuppm_init(TARGET_GPU, g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	GPUFREQ_LOGI("gpufreq platform driver probe done");
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	kfree(g_gpu.working_table);
	kfree(g_gpu.sb_table);
	kfree(g_clk);
	kfree(g_pmic);
	kfree(g_power_table);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mfgpll_probe(struct platform_device *pdev)
{
	int ret = 0;

	GPUFREQ_LOGI("@%s: driver init is started\n", __func__);

	ret = clk_mt6877_pll_registration(GPU_PLL_CTRL, g_mfg_plls,
		pdev, ARRAY_SIZE(g_mfg_plls));
	if (ret) {
		GPUFREQ_LOGI("@%s: failed to register pll\n", __func__);
		return ret;
	}

	GPUFREQ_LOGI("@%s: driver init is finished\n", __func__);

	return ret;
}

/*
 * register the gpufreq mfgpll driver
 */
static int __init __gpufreq_mfgpll_init(void)
{
	int ret = 0;

	GPUFREQ_LOGI("@%s: start to initialize mfgpll driver\n",
			__func__);

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_mfgpll_pdrv);
	if (ret)
		GPUFREQ_LOGI("@%s: fail to register mfgpll driver\n",
			__func__);

	return ret;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	__gpufreq_mfgpll_init();

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_mfgpll_pdrv);
	platform_driver_unregister(&g_gpufreq_pdrv);
}

#if IS_BUILTIN(CONFIG_MTK_GPU_MT6877_SUPPORT)
rootfs_initcall(__gpufreq_init);
#else
module_init(__gpufreq_init);
#endif
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_mfgpll_of_match);
MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
