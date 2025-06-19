// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    gpufreq_mt6991.c
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
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/printk.h>

#include <gpufreq_v2.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6991.h>
#include <gpufreq_reg_mt6991.h>
/* GPUEB */
#include <ghpm_wrapper.h>

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static void __gpufreq_dump_bus_tracker_status(char *log_buf, int *log_len, int log_size);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* whitebox function */
static void __gpufreq_wb_mfg1_slave_stress(void);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_fmeter_fstack0(void);
static unsigned int __gpufreq_get_fmeter_fstack1(void);
static unsigned int __gpufreq_get_pll_fgpu(void);
static unsigned int __gpufreq_get_pll_fstack0(void);
static unsigned int __gpufreq_get_pll_fstack1(void);
/* init function */
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);

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

static void __iomem *g_cksys_base;
static void __iomem *g_emi_infra_ao_mem_base;
static void __iomem *g_emi_infra_cfg_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_mali_base;
static void __iomem *g_mfg_hbvc_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_pll_sc0_base;
static void __iomem *g_mfg_pll_sc1_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_smmu_base;
static void __iomem *g_mfg_vcore_ao_cfg_base;
static void __iomem *g_mfg_vcore_devapc_base;
static void __iomem *g_mfg_vcore_bus_trk_base;
static void __iomem *g_mfg_eb_bus_trk_base;
static void __iomem *g_mfg_vgpu_bus_trk_base;
static void __iomem *g_nemi_mi32_smi;
static void __iomem *g_nemi_mi33_smi;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_semi_mi32_smi;
static void __iomem *g_semi_mi33_smi;
static void __iomem *g_sth_emi_ao_debug_ctrl;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_sleep;
static unsigned int g_eco_version;
static unsigned int g_gpueb_support;
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_load;
static unsigned int g_gpufreq_ready;
static unsigned int g_wb_mfg1_slave_stress;
static int g_slv_error_count;
static int g_slv_timeout_count;
static struct gpufreq_bus_tracker_info g_bus_slv_error[GPUFREQ_MAX_BUSTRK_NUM];
static struct gpufreq_bus_tracker_info g_bus_slv_timeout[GPUFREQ_MAX_BUSTRK_NUM];
static struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.bus_tracker_vio_handler = __gpufreq_bus_tracker_vio_handler,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.set_shared_status = __gpufreq_set_shared_status,
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return -1;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return 0;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return 0;
}

/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return -1;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return -1;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return -1;
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
	GPUFREQ_UNREFERENCED(power);
	return -1;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);
	return -1;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = GPU_DYN_REF_POWER;
	unsigned int ref_freq = GPU_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = GPU_DYN_REF_POWER_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000 * 100000);

	return (unsigned int)p_dynamic;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = 0;
	unsigned int ref_freq = STACK_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = STACK_DYN_REF_POWER_VOLT;

	if (g_eco_version == MT6991_B0)
		p_dynamic = STACK_DYN_REF_POWER_B0;
	else
		p_dynamic = STACK_DYN_REF_POWER_A0;

	p_dynamic = p_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000 * 100000);

	return (unsigned int)p_dynamic;
}

/* API: get core_mask table */
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void)
{
	return g_core_mask_table;
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{
	if (!g_gpufreq_ready)
		return;

	__gpufreq_dump_bus_tracker_status(log_buf, log_len, log_size);

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INFRA STATUS] ==");

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x",
		"[MFG]",
		"EDCM_CON", DRV_Reg32(MFG_EARLY_DCM_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[BUS_PROT]",
		"EMI_SLPPROT_EN_SET", DRV_Reg32(EMI_IFR_AO_SLPPROT_EN_SET),
		"MFG_RPC_SLV_SLP_PROT_RDY", DRV_Reg32(MFG_RPC_SLV_SLP_PROT_RDY_STA),
		"MFG_RPC_SLV_WAY_EN_SET", DRV_Reg32(MFG_RPC_SLV_WAY_EN_SET),
		"MFG_RPC_SLV_WAY_EN_CLR", DRV_Reg32(MFG_RPC_SLV_WAY_EN_CLR),
		"MFG_RPC_SLV_CTRL_UPDATE", DRV_Reg32(MFG_RPC_SLV_CTRL_UPDATE));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"NTH_MFG_EMI1_GALS_SLV", DRV_Reg32(EMI_IFR_NTH_MFG_EMI1_GALS_SLV_DBG),
		"NTH_MFG_EMI0_GALS_SLV", DRV_Reg32(EMI_IFR_NTH_MFG_EMI0_GALS_SLV_DBG),
		"STH_MFG_EMI1_GALS_SLV", DRV_Reg32(EMI_IFR_STH_MFG_EMI1_GALS_SLV_DBG),
		"STH_MFG_EMI0_GALS_SLV", DRV_Reg32(EMI_IFR_STH_MFG_EMI0_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"APU_M1_NOC_GALS_SLV", DRV_Reg32(EMI_IFR_APU_M1_NOC_GALS_SLV_DBG),
		"APU_M0_NOC_GALS_SLV", DRV_Reg32(EMI_IFR_APU_M0_NOC_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"NTH_SOC_EMI_M6_GALS_SLV", DRV_Reg32(EMI_IFR_M6_NTH_GALS_SLV_DBG),
		"NTH_SOC_EMI_M7_GALS_SLV", DRV_Reg32(EMI_IFR_M7_NTH_GALS_SLV_DBG),
		"STH_SOC_EMI_M6_GALS_SLV", DRV_Reg32(EMI_IFR_M6_STH_GALS_SLV_DBG),
		"STH_SOC_EMI_M7_GALS_SLV", DRV_Reg32(EMI_IFR_M7_STH_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_M6M7]",
		"NTH_M6M7_IDLE_1", DRV_Reg32(NTH_AO_M6M7_IDLE_BIT_EN_1),
		"NTH_M6M7_IDLE_0", DRV_Reg32(NTH_AO_M6M7_IDLE_BIT_EN_0),
		"STH_M6M7_IDLE_1", DRV_Reg32(STH_AO_M6M7_IDLE_BIT_EN_1),
		"STH_M6M7_IDLE_0", DRV_Reg32(STH_AO_M6M7_IDLE_BIT_EN_0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_PROT]",
		"NTH_SLEEP_PROT", DRV_Reg32(NTH_AO_SLEEP_PROT_START),
		"NTH_GLITCH_PROT", DRV_Reg32(NTH_AO_GLITCH_PROT_START),
		"STH_SLEEP_PROT", DRV_Reg32(STH_AO_SLEEP_PROT_START),
		"STH_GLITCH_PROT", DRV_Reg32(STH_AO_GLITCH_PROT_START));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_M6M7_PROT]",
		"M6M7_IDLE_1", DRV_Reg32(EMI_IFR_AO_M6M7_IDLE_BIT_EN_1),
		"M6M7_IDLE_0", DRV_Reg32(EMI_IFR_AO_M6M7_IDLE_BIT_EN_0),
		"SLEEP_PROT", DRV_Reg32(EMI_IFR_AO_SLEEP_PROT_START),
		"GLITCH_PROT", DRV_Reg32(EMI_IFR_AO_GLITCH_PROT_START));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI32_SMI_S0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S0),
		"NEMI_MI32_SMI_S1", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S1),
		"NEMI_MI32_SMI_S2", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S2),
		"NEMI_MI32_SMI_M0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_M0),
		"NEMI_MI32_SMI_MISC", DRV_Reg32(NEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI33_SMI_S0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S0),
		"NEMI_MI33_SMI_S1", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S1),
		"NEMI_MI33_SMI_S2", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S2),
		"NEMI_MI33_SMI_M0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_M0),
		"NEMI_MI33_SMI_MISC", DRV_Reg32(NEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"SEMI_MI32_SMI_S0", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S0),
		"SEMI_MI32_SMI_S1", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S1),
		"SEMI_MI32_SMI_S2", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S2),
		"SEMI_MI32_SMI_M0", DRV_Reg32(SEMI_MI32_SMI_DEBUG_M0),
		"SEMI_MI32_SMI_MISC", DRV_Reg32(SEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"SEMI_MI33_SMI_S0", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S0),
		"SEMI_MI33_SMI_S1", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S1),
		"SEMI_MI33_SMI_S2", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S2),
		"SEMI_MI33_SMI_M0", DRV_Reg32(SEMI_MI33_SMI_DEBUG_M0),
		"SEMI_MI33_SMI_MISC", DRV_Reg32(SEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_DBG]",
		"INFRA_BUS0_DBG_CTRL", DRV_Reg32(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		"NTH_EMI_BUS_DBG_CTRL", DRV_Reg32(NTH_EMI_AO_BUS_U_DEBUG_CTRL0),
		"STH_EMI_BUS_DBG_CTRL", DRV_Reg32(STH_EMI_AO_BUS_U_DEBUG_CTRL0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x",
		"[MFG_ACP]",
		"DVM_GALS_MST", DRV_Reg32(EMI_IFR_MFG_ACP_DVM_GALS_MST_DBG),
		"GALS_SLV", DRV_Reg32(EMI_IFR_MFG_ACP_GALS_MST_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08lx",
		"[MISC]",
		"SPM_SRC_REQ", DRV_Reg32(SPM_SRC_REQ),
		"SPM_SOC_BUCK_ISO", DRV_Reg32(SPM_SOC_BUCK_ISO_CON),
		"PWR_STATUS", MFG_0_23_37_PWR_STATUS);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[HBVC_TOP]",
		"GRP0_BACKEND0", DRV_Reg32(MFG_HBVC_GRP0_DBG_BACKEND0),
		"GRP0_BACKEND1", DRV_Reg32(MFG_HBVC_GRP0_DBG_BACKEND1),
		"FLL0_FRONTEND0", DRV_Reg32(MFG_HBVC_FLL0_DBG_FRONTEND0),
		"FLL0_FRONTEND1", DRV_Reg32(MFG_HBVC_FLL0_DBG_FRONTEND1));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[HBVC_STACK]",
		"GRP1_BACKEND0", DRV_Reg32(MFG_HBVC_GRP1_DBG_BACKEND0),
		"GRP1_BACKEND1", DRV_Reg32(MFG_HBVC_GRP1_DBG_BACKEND1),
		"FLL1_FRONTEND0", DRV_Reg32(MFG_HBVC_FLL1_DBG_FRONTEND0),
		"FLL1_FRONTEND1", DRV_Reg32(MFG_HBVC_FLL1_DBG_FRONTEND1));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG0", DRV_Reg32(MFG_RPC_MFG0_PWR_CON),
		"MFG1", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MFG37", DRV_Reg32(MFG_RPC_MFG37_PWR_CON),
		"MFG2", DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		"MFG3", DRV_Reg32(MFG_RPC_MFG3_PWR_CON),
		"MFG4", DRV_Reg32(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG5", DRV_Reg32(MFG_RPC_MFG5_PWR_CON),
		"MFG7", DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		"MFG8", DRV_Reg32(MFG_RPC_MFG8_PWR_CON),
		"MFG23", DRV_Reg32(MFG_RPC_MFG23_PWR_CON),
		"MFG9", DRV_Reg32(MFG_RPC_MFG9_PWR_CON),
		"MFG10", DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG11", DRV_Reg32(MFG_RPC_MFG11_PWR_CON),
		"MFG12", DRV_Reg32(MFG_RPC_MFG12_PWR_CON),
		"MFG13", DRV_Reg32(MFG_RPC_MFG13_PWR_CON),
		"MFG14", DRV_Reg32(MFG_RPC_MFG14_PWR_CON),
		"MFG15", DRV_Reg32(MFG_RPC_MFG15_PWR_CON),
		"MFG16", DRV_Reg32(MFG_RPC_MFG16_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG17", DRV_Reg32(MFG_RPC_MFG17_PWR_CON),
		"MFG18", DRV_Reg32(MFG_RPC_MFG18_PWR_CON),
		"MFG19", DRV_Reg32(MFG_RPC_MFG19_PWR_CON),
		"MFG20", DRV_Reg32(MFG_RPC_MFG20_PWR_CON));
}

void __gpufreq_dump_power_tracker_status(void)
{
	int i = 0;
	unsigned int val = 0, cur_point = 0, read_point = 0;

	if (!g_gpufreq_ready)
		return;

	cur_point = (DRV_Reg32(MFG_POWER_TRACKER_SETTING) >> 10) & GENMASK(5, 0);
	GPUFREQ_LOGI("== [PDC POWER TRACKER STATUS: %u] ==", cur_point);
	for (i = 1; i <= 16; i++) {
		/* only dump last 16 record */
		read_point = (cur_point + ~i + 1) & GENMASK(5, 0);
		val = (DRV_Reg32(MFG_POWER_TRACKER_SETTING) & ~GENMASK(9, 4)) | (read_point << 4);
		DRV_WriteReg32(MFG_POWER_TRACKER_SETTING, val);
		udelay(1);

		GPUFREQ_LOGI("[%02u][%u] STA 1=0x%08x, 2=0x%08x, 3=0x%08x, 4=0x%08x, 5=0x%08x",
			read_point, DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS0),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS1),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS2),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS3),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS4),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS5));
	}
}

unsigned int __gpufreq_bus_tracker_vio_handler(void)
{
	unsigned int ret = false, vio_sta = 0, check_mask = 0;
	unsigned int vcore_bus_dbg_con = 0, vgpu_bus_dbg_con = 0, gpueb_bus_dbg_con = 0;
	unsigned long long timestamp = 0;
	int i = 0;

	if (!g_gpufreq_ready)
		return ret;

	vio_sta = DRV_Reg32(MFG_VCORE_DEVAPC_D0_VIO_STA_0);
	/* B0 violation bit is 28 */
	if (g_eco_version == MT6991_B0)
		check_mask = BIT(28);
	else
		check_mask = BIT(26);

	if (vio_sta & check_mask) {
		/* power on gpueb */
		ret = gpueb_ctrl(GHPM_ON, MFG1_OFF, SUSPEND_POWER_ON);
		if (ret) {
			__gpufreq_abort("gpueb power on fail, ret=%d", ret);
			return false;
		}

		/* check bus tracker violation status */
		vcore_bus_dbg_con = DRV_Reg32(MFG_VCORE_BUS_DBG_CON_0);
		vgpu_bus_dbg_con= DRV_Reg32(MFG_VGPU_BUS_DBG_CON_0);
		gpueb_bus_dbg_con = DRV_Reg32(MFG_GPUEB_BUS_DBG_CON_0);

		/* GPUEB bus tracker timeout */
		if (gpueb_bus_dbg_con & GENMASK(9, 8)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_H) << 32 |
				DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_L);
			GPUFREQ_LOGE("[GPUEB_BUS_TRK][%llu] %s=0x%08x, %s=0x%08x", timestamp,
				"GPUEB BUS_DBG_CON_0", gpueb_bus_dbg_con,
				"TIMEOUT_INFO", DRV_Reg32(MFG_GPUEB_BUS_TIMEOUT_INFO));

			/* GPUEB read timeout */
			if (gpueb_bus_dbg_con & BIT(8)) {
				for (i = 0; i < 32; i++) {
					if (!DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_LOG + (i * 4)))
						continue;
					BUS_TRACKER_OP(
						g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
						g_slv_timeout_count, BUS_GPUEB_AR, timestamp,
						DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_LOG + (i * 4)),
						DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_ID + (i * 4)),
						DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_L + (i * 4)));
				}
			}
			/* GPUEB write timeout */
			if (gpueb_bus_dbg_con & BIT(9)) {
				for (i = 0; i < 32; i++) {
					if (!DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_LOG + (i * 4)))
						continue;
					BUS_TRACKER_OP(
						g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
						g_slv_timeout_count, BUS_GPUEB_AW, timestamp,
						DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_LOG + (i * 4)),
						DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_ID + (i * 4)),
						DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_L + (i * 4)));
				}
			}

			/* update current status to shared memory */
			if (g_shared_status)
				ARRAY_ASSIGN(g_shared_status->bus_slv_timeout,
					g_bus_slv_timeout, GPUFREQ_MAX_BUSTRK_NUM);
		}

		/* VCORE/VGPU bus tracker timeout */
		if (vcore_bus_dbg_con & GENMASK(9, 8) || vgpu_bus_dbg_con & GENMASK(9, 8)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_H) << 32 |
				DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_L);
			GPUFREQ_LOGE("[VCORE_BUS_TRK][%llu] %s=0x%08x, %s=0x%08x", timestamp,
				"VCORE BUS_DBG_CON_0", vcore_bus_dbg_con,
				"TIMEOUT_INFO", DRV_Reg32(MFG_VCORE_BUS_TIMEOUT_INFO));
			timestamp = (unsigned long long)DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_H) << 32 |
				DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_L);
			GPUFREQ_LOGE("[VGPU_BUS_TRK][%llu] %s=0x%08x, %s=0x%08x", timestamp,
				"VGPU BUS_DBG_CON_0", vgpu_bus_dbg_con,
				"TIMEOUT_INFO", DRV_Reg32(MFG_VGPU_BUS_TIMEOUT_INFO));

			/* VCORE read timeout */
			if (vcore_bus_dbg_con & BIT(8)) {
				for (i = 0; i < 16; i++) {
					if (!DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_LOG + (i * 4)))
						continue;
					GPUFREQ_LOGE("[VCORE_AR_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_L + (i * 4)));
					BUS_TRACKER_OP(
						g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
						g_slv_timeout_count, BUS_VCORE_AR, timestamp,
						DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_LOG + (i * 4)),
						DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_ID + (i * 4)),
						DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_L + (i * 4)));
				}
			}
			/* VCORE write timeout */
			if (vcore_bus_dbg_con & BIT(9)) {
				for (i = 0; i < 16; i++) {
					if (!DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_LOG + (i * 4)))
						continue;
					GPUFREQ_LOGE("[VCORE_AW_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_L + (i * 4)));
					BUS_TRACKER_OP(
						g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
						g_slv_timeout_count, BUS_VCORE_AW, timestamp,
						DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_LOG + (i * 4)),
						DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_ID + (i * 4)),
						DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_L + (i * 4)));
				}
			}
			/* VGPU read timeout */
			if (vgpu_bus_dbg_con & BIT(8)) {
				for (i = 0; i < 32; i++) {
					if (!DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_LOG + (i * 4)))
						continue;
					GPUFREQ_LOGE("[VGPU_AR_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_L + (i * 4)));
					BUS_TRACKER_OP(
						g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
						g_slv_timeout_count, BUS_VGPU_AR, timestamp,
						DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_LOG + (i * 4)),
						DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_ID + (i * 4)),
						DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_L + (i * 4)));
				}
			}
			/* VGPU write timeout */
			if (vgpu_bus_dbg_con & BIT(9)) {
				for (i = 0; i < 32; i++) {
					if (!DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_LOG + (i * 4)))
						continue;
					GPUFREQ_LOGE("[VGPU_AW_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_L + (i * 4)));
					BUS_TRACKER_OP(
						g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
						g_slv_timeout_count, BUS_VGPU_AW, timestamp,
						DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_LOG + (i * 4)),
						DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_ID + (i * 4)),
						DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_L + (i * 4)));
				}
			}

			/* update current status to shared memory */
			if (g_shared_status)
				ARRAY_ASSIGN(g_shared_status->bus_slv_timeout,
					g_bus_slv_timeout, GPUFREQ_MAX_BUSTRK_NUM);

			__gpufreq_abort("VCORE/VGPU bus tracker violation");
		}

		/* VCORE/VGPU/GPUEB bus tracker error */
		/* VCORE read error */
		if (vcore_bus_dbg_con & BIT(12)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
				DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L);
			BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
				g_slv_error_count, BUS_VCORE_AR, timestamp,
				DRV_Reg32(MFG_VCORE_BUS_AR_SLVERR_LOG),
				DRV_Reg32(MFG_VCORE_BUS_AR_SLVERR_ID),
				DRV_Reg32(MFG_VCORE_BUS_AR_SLVERR_ADDR_L));
		}
		/* VCORE write error */
		if (vcore_bus_dbg_con & BIT(13)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
				DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L);
			BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
				g_slv_error_count, BUS_VCORE_AW, timestamp,
				DRV_Reg32(MFG_VCORE_BUS_AW_SLVERR_LOG),
				DRV_Reg32(MFG_VCORE_BUS_AW_SLVERR_ID),
				DRV_Reg32(MFG_VCORE_BUS_AW_SLVERR_ADDR_L));
		}
		/* VGPU read error */
		if (vgpu_bus_dbg_con & BIT(12)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
				DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_L);
			BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
				g_slv_error_count, BUS_VGPU_AR, timestamp,
				DRV_Reg32(MFG_VGPU_BUS_AR_SLVERR_LOG),
				DRV_Reg32(MFG_VGPU_BUS_AR_SLVERR_ID),
				DRV_Reg32(MFG_VGPU_BUS_AR_SLVERR_ADDR_L));
		}
		/* VGPU write error */
		if (vgpu_bus_dbg_con & BIT(13)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
				DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_L);
			BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
				g_slv_error_count, BUS_VGPU_AW, timestamp,
				DRV_Reg32(MFG_VGPU_BUS_AW_SLVERR_LOG),
				DRV_Reg32(MFG_VGPU_BUS_AW_SLVERR_ID),
				DRV_Reg32(MFG_VGPU_BUS_AW_SLVERR_ADDR_L));
		}
		/* GPUEB read error */
		if (gpueb_bus_dbg_con & BIT(12)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
				DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L);
			BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
				g_slv_error_count, BUS_GPUEB_AR, timestamp,
				DRV_Reg32(MFG_GPUEB_BUS_AR_SLVERR_LOG),
				DRV_Reg32(MFG_GPUEB_BUS_AR_SLVERR_ID),
				DRV_Reg32(MFG_GPUEB_BUS_AR_SLVERR_ADDR_L));
		}
		/* GPUEB write error */
		if (gpueb_bus_dbg_con & BIT(13)) {
			timestamp = (unsigned long long)DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
				DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L);
			BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
				g_slv_error_count, BUS_GPUEB_AW, timestamp,
				DRV_Reg32(MFG_GPUEB_BUS_AW_SLVERR_LOG),
				DRV_Reg32(MFG_GPUEB_BUS_AW_SLVERR_ID),
				DRV_Reg32(MFG_GPUEB_BUS_AW_SLVERR_ADDR_L));
		}

		/* update current status to shared memory */
		if (g_shared_status)
			ARRAY_ASSIGN(g_shared_status->bus_slv_error, g_bus_slv_error, GPUFREQ_MAX_BUSTRK_NUM);

		/* clear bus tracker IRQ */
		if (vcore_bus_dbg_con & GENMASK(13, 12)) {
			DRV_WriteReg32(MFG_VCORE_BUS_DBG_CON_0, BIT(7));
			DRV_WriteReg32(MFG_VCORE_BUS_DBG_CON_0, (BIT(7) | BIT(16)));
			DRV_WriteReg32(MFG_VCORE_BUS_DBG_CON_0, 0x0);
		}
		if (vgpu_bus_dbg_con & GENMASK(13, 12)) {
			DRV_WriteReg32(MFG_VGPU_BUS_DBG_CON_0, BIT(7));
			DRV_WriteReg32(MFG_VGPU_BUS_DBG_CON_0, (BIT(7) | BIT(16)));
			DRV_WriteReg32(MFG_VGPU_BUS_DBG_CON_0, 0x0);
		}
		if (gpueb_bus_dbg_con & (GENMASK(13, 12) | GENMASK(9, 8))) {
			DRV_WriteReg32(MFG_GPUEB_BUS_DBG_CON_0, BIT(7));
			DRV_WriteReg32(MFG_GPUEB_BUS_DBG_CON_0, (BIT(7) | BIT(16)));
			DRV_WriteReg32(MFG_GPUEB_BUS_DBG_CON_0, 0x0);
		}

		/* power off GPUEB */
		ret = gpueb_ctrl(GHPM_OFF, MFG1_OFF, SUSPEND_POWER_OFF);
		if (ret) {
			__gpufreq_abort("gpueb power off fail, ret=%d", ret);
			return false;
		}

		/* clear status and bypass DEVAPC violation if only caught bus tracker violation */
		if (vio_sta == check_mask) {
			DRV_WriteReg32(MFG_VCORE_DEVAPC_D0_VIO_STA_0, check_mask);
			DRV_WriteReg32(MFG_VCORE_DEVAPC_D0_VIO_MASK_0, 0x0);
			ret = true;
		}
	}

	/* true: bypass DEVAPC violation, false: keep handling DEVAPC */
	return ret;
}

/* API: get working OPP index of STACK limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
	GPUFREQ_UNREFERENCED(batt_oc_level);
	return -1;
}

/* API: get working OPP index of STACK limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
	GPUFREQ_UNREFERENCED(batt_percent_level);
	return -1;
}

/* API: get working OPP index of STACK limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
	GPUFREQ_UNREFERENCED(low_batt_level);
	return -1;
}

int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/* API: commit DVFS with only STACK OPP index */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	return __gpufreq_generic_commit_dual(target_oppidx, target_oppidx, key);
}

/* API: commit DVFS by given both GPU and STACK OPP index */
int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx_gpu);
	GPUFREQ_UNREFERENCED(target_oppidx_stack);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	GPUFREQ_UNREFERENCED(power);

	return 1;
}

/* API: init first time shared status */
void __gpufreq_set_shared_status(struct gpufreq_shared_status *shared_status)
{
	if (shared_status && !g_shared_status)
		g_shared_status = shared_status;

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->kdbg_version = GPUFREQ_KDEBUG_VERSION;

	GPUFREQ_LOGI("shared memory addr: 0x%llx", (unsigned long long)shared_status);
}

void __gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val)
{
	switch (target) {
	case CONFIG_WB_TEST_ONCE:
		/* execute every test case */
		__gpufreq_wb_mfg1_slave_stress();
		break;
	case CONFIG_WB_MFG1_SLAVE_STRESS:
		g_wb_mfg1_slave_stress = val;
		GPUFREQ_LOGI("set WB_MFG1_SLAVE_STRESS: %d", g_wb_mfg1_slave_stress);
		break;
	default:
		GPUFREQ_LOGE("invalid config target: %d", target);
		break;
	}
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static void __gpufreq_wb_mfg1_slave_stress(void)
{
	int i = 0;
	unsigned int val = 0;

	if (g_wb_mfg1_slave_stress) {
		for (i = 0; i < 100; i++) {
			val = DRV_Reg32(MALI_GPU_ID);
			val = DRV_Reg32(MFG_TOP_CG_CON);
		}
	}
}

static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (!gpufreq_dev) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		return;
	}

	/* 0x48000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}
	/* 0x48500000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		return;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		return;
	}
	/* 0x48600000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SMMU");
		return;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		return;
	}
	/* 0x4B800000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		return;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		return;
	}
	/* 0x4B810000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		return;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		return;
	}
	/* 0x4B810400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc0");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC0");
		return;
	}
	g_mfg_pll_sc0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc0_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC0: 0x%llx", res->start);
		return;
	}
	/* 0x4B810800 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc1");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC1");
		return;
	}
	g_mfg_pll_sc1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc1_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC1: 0x%llx", res->start);
		return;
	}
	/* 0x4B860000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_ao_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_AO_CFG");
		return;
	}
	g_mfg_vcore_ao_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_ao_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_AO_CFG: 0x%llx", res->start);
		return;
	}
	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cksys");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource CKSYS");
		return;
	}
	g_cksys_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_cksys_base) {
		GPUFREQ_LOGE("fail to ioremap CKSYS: 0x%llx", res->start);
		return;
	}
	/* 0x1C004000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		return;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		return;
	}

	GPUFREQ_LOGI("[SPM] %s=0x%08x",
		"SPM2GPUPM_CON", DRV_Reg32(SPM_SPM2GPUPM_CON));
	GPUFREQ_LOGI("[MFG] %s=0x%08lx, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG_0_23_37_PWR_STATUS", MFG_0_23_37_PWR_STATUS,
		"MFG0_PWR_CON", DRV_Reg32(MFG_RPC_MFG0_PWR_CON),
		"MFG1_PWR_CON", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"SMMU_CR0", DRV_Reg32(MFG_SMMU_CR0),
		"SMMU_GBPA", DRV_Reg32(MFG_SMMU_GBPA));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG_DREQ", DRV_Reg32(MFG_VCORE_AO_RPC_DREQ_CONFIG),
		"MFG_DEFAULT_DELSEL", DRV_Reg32(MFG_DEFAULT_DELSEL_00),
		"MFG_TOP_DELSEL", DRV_Reg32(MFG_SRAM_FUL_SEL_ULV_TOP),
		"MFG_STACK_DELSEL", DRV_Reg32(MFG_SRAM_FUL_SEL_ULV));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_TOP", DRV_Reg32(MFG_RPC_BRISKET_TOP_AO_CFG_0),
		"BRISKET_ST0", DRV_Reg32(MFG_BRISKET_ST0_AO_CFG_0),
		"BRISKET_ST1", DRV_Reg32(MFG_BRISKET_ST1_AO_CFG_0),
		"BRISKET_ST2", DRV_Reg32(MFG_BRISKET_ST2_AO_CFG_0));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_ST5", DRV_Reg32(MFG_BRISKET_ST5_AO_CFG_0),
		"BRISKET_ST6", DRV_Reg32(MFG_BRISKET_ST6_AO_CFG_0),
		"BRISKET_ST7", DRV_Reg32(MFG_BRISKET_ST7_AO_CFG_0));
	GPUFREQ_LOGI("[TOP] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_CON0),
		"CON1", __gpufreq_get_pll_fgpu(),
		"FMETER", __gpufreq_get_fmeter_fgpu(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_TOP_SEL_BIT,
		"REF_SEL", DRV_Reg32(CKSYS_CLK_CFG_6) & MFG_REF_SEL_BIT);
	GPUFREQ_LOGI("[SC0] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_SC0_CON0),
		"CON1", __gpufreq_get_pll_fstack0(),
		"FMETER", __gpufreq_get_fmeter_fstack0(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_SC0_SEL_BIT);
	GPUFREQ_LOGI("[SC1] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_SC1_CON0),
		"CON1", __gpufreq_get_pll_fstack1(),
		"FMETER", __gpufreq_get_fmeter_fstack1(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_SC1_SEL_BIT);
	GPUFREQ_LOGI("[GPU] %s=0x%08x",
		"MALI_GPU_ID", DRV_Reg32(MALI_GPU_ID));
}

static void __gpufreq_dump_bus_tracker_status(char *log_buf, int *log_len, int log_size)
{
	int i = 0, idx = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [BUS TRACKER SLAVE ERROR: %d] ==", g_slv_error_count);
	for (i = 1; i <= GPUFREQ_MAX_BUSTRK_NUM; i++) {
		if (g_slv_error_count >= i) {
			idx = (g_slv_error_count - i) % GPUFREQ_MAX_BUSTRK_NUM;
			GPUFREQ_LOGB(log_buf, log_len, log_size,
				"[%02d][%s][%llu] LOG=0x%08x, ID=0x%08x, ADDR=0x%08x",
				g_slv_error_count - i,
				GPUFREQ_BUS_TRACKER_TYPE_STRING(g_bus_slv_error[idx].type),
				g_bus_slv_error[idx].timestamp, g_bus_slv_error[idx].log,
				g_bus_slv_error[idx].id, g_bus_slv_error[idx].addr);
		}
	}

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [BUS TRACKER SLAVE TIMEOUT: %d] ==", g_slv_timeout_count);
	for (i = 1; i <= GPUFREQ_MAX_BUSTRK_NUM; i++) {
		if (g_slv_timeout_count >= i) {
			idx = (g_slv_timeout_count - i) % GPUFREQ_MAX_BUSTRK_NUM;
			GPUFREQ_LOGB(log_buf, log_len, log_size,
				"[%02d][%s][%llu] LOG=0x%08x, ID=0x%08x, ADDR=0x%08x",
				g_slv_timeout_count - i,
				GPUFREQ_BUS_TRACKER_TYPE_STRING(g_bus_slv_timeout[idx].type),
				g_bus_slv_timeout[idx].timestamp, g_bus_slv_timeout[idx].log,
				g_bus_slv_timeout[idx].id, g_bus_slv_timeout[idx].addr);
		}
	}
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	int i = 0;
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0, freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_CON0);
	DRV_WriteReg32(MFG_PLL_CON0, (val | BIT(12)));

	/* Enable RG_TST_CK_SEL */
	val = DRV_Reg32(MFG_PLL_CON5);
	DRV_WriteReg32(MFG_PLL_CON5, (val | BIT(4)));

	DRV_WriteReg32(MFG_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	/* MFG_PLL_FQMTR_CON0 0x13FA0040 [1:0] = 2'b10, select brisket_out_ck */
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (BIT(1) & ~BIT(0) | BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_TOP_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fstack0(void)
{
	int i = 0;
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0, freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_SC0_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_CON0, (val | BIT(12)));

	/* Enable RG_TST_CK_SEL */
	val = DRV_Reg32(MFG_PLL_SC0_CON5);
	DRV_WriteReg32(MFG_PLL_SC0_CON5, (val | BIT(4)));

	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_SC0_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fstack1(void)
{
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_SC1_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_CON0, (val | BIT(12)));

	/* Enable RG_TST_CK_SEL */
	val = DRV_Reg32(MFG_PLL_SC1_CON5);
	DRV_WriteReg32(MFG_PLL_SC1_CON5, (val | BIT(4)));

	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_SC0_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fgpu(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fstack0(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_SC0_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fstack1(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_SC1_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
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

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	if (!gpufreq_dev) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (!of_wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x48000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x48500000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x48600000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SMMU");
		goto done;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		goto done;
	}

	/* 0x48800000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vgpu_bus_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VGPU_BUS_TRACKER");
		goto done;
	}
	g_mfg_vgpu_bus_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vgpu_bus_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VGPU_BUS_TRACKER: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B190000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_gpueb_bus_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_GPUEB_BUS_TRACKER");
		goto done;
	}
	g_mfg_eb_bus_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_eb_bus_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_GPUEB_BUS_TRACKER: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B800000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B810000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B810400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc0");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC0");
		goto done;
	}
	g_mfg_pll_sc0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc0_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC0: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B810800 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc1");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC1");
		goto done;
	}
	g_mfg_pll_sc1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc1_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC1: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B840000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_hbvc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_HBVC");
		goto done;
	}
	g_mfg_hbvc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_hbvc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_HBVC: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B860000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_ao_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_AO_CFG");
		goto done;
	}
	g_mfg_vcore_ao_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_ao_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_AO_CFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B8B0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_devapc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_DEVAPC");
		goto done;
	}
	g_mfg_vcore_devapc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_devapc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_DEVAPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B900000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_bus_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_BUS_TRACKER");
		goto done;
	}
	g_mfg_vcore_bus_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_bus_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_BUS_TRACKER: 0x%llx", res->start);
		goto done;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cksys");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource CKSYS");
		goto done;
	}
	g_cksys_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_cksys_base) {
		GPUFREQ_LOGE("fail to ioremap CKSYS: 0x%llx", res->start);
		goto done;
	}

	/* 0x10404000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emicfg_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10416000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emi_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10504000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emicfg_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10516000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emi_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_sth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emi_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap STH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10621000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi32_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI32_SMI");
		goto done;
	}
	g_semi_mi32_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_semi_mi32_smi) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI32_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10622000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi32_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI32_SMI");
		goto done;
	}
	g_nemi_mi32_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi32_smi) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI32_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10623000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi33_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI33_SMI");
		goto done;
	}
	g_semi_mi33_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_semi_mi33_smi) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI33_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10624000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi33_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI33_SMI");
		goto done;
	}
	g_nemi_mi33_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi33_smi) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI33_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10644000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_infra_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10646000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_INFRA_AO_MEM");
		goto done;
	}
	g_emi_infra_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_INFRA_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10648000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_INFRA_CFG");
		goto done;
	}
	g_emi_infra_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_INFRA_CFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C004000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* set chip version */
	g_eco_version = (DRV_Reg32(MFG_VCORE_AO_MT6991_ID_CON) == 0x101) ? MT6991_B0 : MT6991_A0;

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

/* API: skip gpufreq driver probe if in bringup state */
static unsigned int __gpufreq_bringup(void)
{
	struct device_node *of_wrapper = NULL;
	unsigned int bringup_state = false;

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (!of_wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node, treat as bringup");
		return true;
	}

	/* check bringup state by dts */
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &bringup_state);

	return bringup_state;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		__gpufreq_dump_bringup_status(pdev);
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	if (!g_gpueb_support) {
		GPUFREQ_LOGE("gpufreq not support AP mode");
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	gpufreq_register_gpufreq_fp(&platform_eb_fp);

	/* init gpuppm */
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support);
	if (ret) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	return GPUFREQ_SUCCESS;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

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
	platform_driver_unregister(&g_gpufreq_pdrv);
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
