/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6781_H__
#define __GPUFREQ_MT6781_H__

/**************************************************
 * GPUFREQ Local Config
 **************************************************/
#define GPUFREQ_BRINGUP                 (0)
/*
 * 0 -> power on once then never off and disable DDK power on/off callback
 */
#define GPUFREQ_POWER_CTRL_ENABLE       (1)
/*
 * (DVFS_ENABLE, CUST_INIT)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPPIDX
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPPIDX (do DVFS only onces)
 * (0, 0) -> DVFS disable
 */
#define GPUFREQ_DVFS_ENABLE             (1)
#define GPUFREQ_CUST_INIT_ENABLE        (0)
#define GPUFREQ_CUST_INIT_OPPIDX        (8) //As K510 has (g_opp_table_segment_1[8].gpufreq_khz)
#define MT_GPUFREQ_DFD_ENABLE        (0)

#define PBM_RAEDY                       (0)
#define SHADER_CORE_NUM                 (3)
/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)            /* MHz */
#define MFGPLL_FH_PLL                   FH_PLL6//(6)
#define MFGPLL_CON1                     (g_apmixed_base + 0x026C)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
 //todo disable until CONFIG_COMMON_CLK_MTK_FREQ_HOPPING ready
#define GPUFREQ_FHCTL_ENABLE            (0)
#define MFG_PLL_NAME                    "mfgpll1"
//TODO:MFG_PLL_NAME should be FH_PLL4 same as K419 ?
//Currently not working as GPUFREQ_FHCTL_ENABLE is 0

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
#define MT_GPUFREQ_STATIC_PWR_READY2USE         1


/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG2_SHADER_STACK0         (T0C0)
#define MFG3_SHADER_STACK2         (T2C0)
#define MFG5_SHADER_STACK4         (T4C0)
#define MT_GPU_SHADER_PRESENT_3    (T0C0 | T2C0 | T4C0)

/**************************************************
 * Reference Power Setting MT6781
 **************************************************/
#define GPU_ACT_REF_POWER			(1133)		/* mW  */
#define GPU_ACT_REF_FREQ			(950000)	/* KHz */
#define GPU_ACT_REF_VOLT			(80000)		/* mV x 100 */
#define PTPOD_DISABLE_VOLT              (75000)

#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6781
 **************************************************/
#define VGPU_MAX_VOLT		(119375)         /* mV x 100 */
#define VGPU_MIN_VOLT       (30000)         /* mV x 100 */
#define VSRAM_MAX_VOLT      (129375)        /* mV x 100 */
#define VSRAM_MIN_VOLT      (50000)         /* mV x 100 */
#define PMIC_STEP           (625)           /* mV x 100 */

/*
 * (-100)mv <= (VSRAM - VGPU) <= (300)mV
 */
#define MAX_BUCK_DIFF                   (30000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (-10000)        /* mV x 100 */

/* On opp table, low vgpu will use the same vsram.
 * And hgih vgpu will have the same diff with vsram.
 *
 * if (vgpu <= FIXED_VSRAM_VOLT_THSRESHOLD) {
 *     vsram = FIXED_VSRAM_VOLT;
 * } else {
 *     vsram = vgpu + FIXED_VSRAM_VOLT_DIFF;
 * }
 */
#define VSRAM_FIXED_THRESHOLD           (75000)
#define VSRAM_FIXED_VOLT                (75000)
#define VSRAM_FIXED_DIFF                (0)
#define VOLT_NORMALIZATION(volt) \
	((volt % PMIC_STEP) ? (volt - (volt % PMIC_STEP) + PMIC_STEP) : volt)


/**************************************************
 * SRAMRC Setting
 **************************************************/
#define GPUFREQ_SAFE_VLOGIC             (60000)

/**************************************************
 * Power Throttling Setting
 **************************************************/
// MT6781_PORTING_TODO check below power settings @{
/**************************************************
 * Battery Over Current Protect
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (1)
#define GPUFREQ_BATT_OC_FREQ            (485000)
/**************************************************
 * Battery Percentage Protect
 **************************************************/
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_BATT_PERCENT_IDX        (1)
/**************************************************
 * Low Battery Volume Protect
 **************************************************/
#define GPUFREQ_LOW_BATT_ENABLE         (1)
#define GPUFREQ_LOW_BATT_FREQ           (485000)

/**************************************************
 * AGING Setting
 **************************************************/
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)
/**************************************************
 * Dump infra status
 **************************************************/
#define FM_HF_FMFG_CK 3
#define FM_AD_MFGPLL_CK 26

/**************************************************
 * Enumeration MT6781
 **************************************************/
enum gpufreq_segment {
	MT6853_SEGMENT = 1,
	MT6853T_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

/**************************************************
 * Structure MT6781
 **************************************************/
struct g_clk_info {
	struct clk *clk_mux;
	struct clk *clk_ref_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_bg3d;
	struct clk *mtcmos_mfg0;
	struct clk *mtcmos_mfg1;
	struct clk *mtcmos_mfg2;
	struct clk *mtcmos_mfg3;
	struct clk *mtcmos_mfg4;
	struct clk *mtcmos_mfg5;
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};
struct gpufreq_sb_info {
	int up;
	int down;
};
struct gpufreq_status {
	struct gpufreq_opp_info *signed_table;
	struct gpufreq_opp_info *working_table;
	struct gpufreq_sb_info *sb_table;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	unsigned int segment_id;
	int signed_opp_num;
	int segment_upbound;
	int segment_lowbound;
	int opp_num;
	int max_oppidx;
	int min_oppidx;
	int cur_oppidx;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int cur_vsram;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define SIGNED_OPP_GPU_NUM              ARRAY_SIZE(g_opp_table_segment_1)

struct gpufreq_opp_info g_opp_table_segment_1[] = {
	GPUOP(950000, 80000, 80000, POSDIV_POWER_4, 1875, 0), /* 0 sign off */
	GPUOP(941000, 79375, 79375, POSDIV_POWER_4, 1875, 0), /* 1 */
	GPUOP(932000, 78750, 78750, POSDIV_POWER_4, 1875, 0), /* 2 */
	GPUOP(923000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /* 3 */
	GPUOP(915000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /* 4 */
	GPUOP(906000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /* 5 */
	GPUOP(897000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /* 6 */
	GPUOP(888000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /* 7 */
	GPUOP(880000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /* 8 sign off */
	GPUOP(865000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /* 9 */
	GPUOP(850000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /*10 */
	GPUOP(835000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /*11 */
	GPUOP(820000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /*12 */
	GPUOP(805000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /*13 */
	GPUOP(790000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /*14 */
	GPUOP(775000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /*15 */
	GPUOP(760000, 70000, 75000, POSDIV_POWER_4, 1250, 0), /*16 */
	GPUOP(745000, 69375, 75000, POSDIV_POWER_4, 1250, 0), /*17 */
	GPUOP(730000, 68750, 75000, POSDIV_POWER_4, 1250, 0), /*18 */
	GPUOP(715000, 68125, 75000, POSDIV_POWER_4, 1250, 0), /*19 */
	GPUOP(700000, 67500, 75000, POSDIV_POWER_4, 1250, 0), /*20 */
	GPUOP(685000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /*21 */
	GPUOP(670000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /*22 */
	GPUOP(655000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /*23 */
	GPUOP(640000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*24 sign off */
	GPUOP(619000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*25 */
	GPUOP(598000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /*26 */
	GPUOP(577000, 63125, 75000, POSDIV_POWER_4, 1250, 0), /*27 */
	GPUOP(556000, 62500, 75000, POSDIV_POWER_4, 625, 0),  /*28 */
	GPUOP(535000, 61875, 75000, POSDIV_POWER_4, 625, 0),  /*29 */
	GPUOP(515000, 61250, 75000, POSDIV_POWER_4, 625, 0),  /*30 */
	GPUOP(494000, 60625, 75000, POSDIV_POWER_4, 625, 0),  /*31 */
	GPUOP(473000, 60000, 75000, POSDIV_POWER_4, 625, 0),  /*32 */
	GPUOP(452000, 59375, 75000, POSDIV_POWER_4, 625, 0),  /*33 */
	GPUOP(431000, 58750, 75000, POSDIV_POWER_4, 625, 0),  /*34 */
	GPUOP(410000, 58125, 75000, POSDIV_POWER_4, 625, 0),  /*35 */
	GPUOP(390000, 57500, 75000, POSDIV_POWER_4, 625, 0),  /*36 sign off */
};

struct gpufreq_opp_info g_opp_table_segment_2[] = {
	GPUOP(950000, 80000, 80000, POSDIV_POWER_4, 1875, 0), /* 0 sign off */
	GPUOP(941000, 79375, 79375, POSDIV_POWER_4, 1875, 0), /* 1 */
	GPUOP(932000, 78750, 78750, POSDIV_POWER_4, 1875, 0), /* 2 */
	GPUOP(923000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /* 3 */
	GPUOP(915000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /* 4 */
	GPUOP(906000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /* 5 */
	GPUOP(897000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /* 6 */
	GPUOP(888000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /* 7 */
	GPUOP(880000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /* 8 sign off */
	GPUOP(865000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /* 9 */
	GPUOP(850000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /*10 */
	GPUOP(835000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /*11 */
	GPUOP(820000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /*12 */
	GPUOP(805000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /*13 */
	GPUOP(790000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /*14 */
	GPUOP(775000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /*15 */
	GPUOP(760000, 70000, 75000, POSDIV_POWER_4, 1250, 0), /*16 */
	GPUOP(745000, 69375, 75000, POSDIV_POWER_4, 1250, 0), /*17 */
	GPUOP(730000, 68750, 75000, POSDIV_POWER_4, 1250, 0), /*18 */
	GPUOP(715000, 68125, 75000, POSDIV_POWER_4, 1250, 0), /*19 */
	GPUOP(700000, 67500, 75000, POSDIV_POWER_4, 1250, 0), /*20 */
	GPUOP(685000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /*21 */
	GPUOP(670000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /*22 */
	GPUOP(655000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /*23 */
	GPUOP(640000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*24 sign off */
	GPUOP(619000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*25 */
	GPUOP(598000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*26 */
	GPUOP(577000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /*27 */
	GPUOP(556000, 63750, 75000, POSDIV_POWER_4, 625, 0),  /*28 */
	GPUOP(535000, 63125, 75000, POSDIV_POWER_4, 625, 0),  /*29 */
	GPUOP(515000, 62500, 75000, POSDIV_POWER_4, 625, 0),  /*30 */
	GPUOP(494000, 62500, 75000, POSDIV_POWER_4, 625, 0),  /*31 */
	GPUOP(473000, 61875, 75000, POSDIV_POWER_4, 625, 0),  /*32 */
	GPUOP(452000, 61250, 75000, POSDIV_POWER_4, 625, 0),  /*33 */
	GPUOP(431000, 61250, 75000, POSDIV_POWER_4, 625, 0),  /*34 */
	GPUOP(410000, 60625, 75000, POSDIV_POWER_4, 625, 0),  /*35 */
	GPUOP(390000, 60000, 75000, POSDIV_POWER_4, 625, 0),  /*36 sign off */
};

struct gpufreq_opp_info g_opp_table_segment_3[] = {
	GPUOP(950000, 80000, 80000, POSDIV_POWER_4, 1875, 0), /* 0 sign off */
	GPUOP(941000, 79375, 79375, POSDIV_POWER_4, 1875, 0), /* 1 */
	GPUOP(932000, 78750, 78750, POSDIV_POWER_4, 1875, 0), /* 2 */
	GPUOP(923000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /* 3 */
	GPUOP(915000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /* 4 */
	GPUOP(906000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /* 5 */
	GPUOP(897000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /* 6 */
	GPUOP(888000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /* 7 */
	GPUOP(880000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /* 8 sign off */
	GPUOP(865000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /* 9 */
	GPUOP(850000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /*10 */
	GPUOP(835000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /*11 */
	GPUOP(820000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /*12 */
	GPUOP(805000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /*13 */
	GPUOP(790000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /*14 */
	GPUOP(775000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /*15 */
	GPUOP(760000, 70000, 75000, POSDIV_POWER_4, 1250, 0), /*16 */
	GPUOP(745000, 69375, 75000, POSDIV_POWER_4, 1250, 0), /*17 */
	GPUOP(730000, 68750, 75000, POSDIV_POWER_4, 1250, 0), /*18 */
	GPUOP(715000, 68125, 75000, POSDIV_POWER_4, 1250, 0), /*19 */
	GPUOP(700000, 67500, 75000, POSDIV_POWER_4, 1250, 0), /*20 */
	GPUOP(685000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /*21 */
	GPUOP(670000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /*22 */
	GPUOP(655000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /*23 */
	GPUOP(640000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*24 sign off */
	GPUOP(619000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*25 */
	GPUOP(598000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*26 */
	GPUOP(577000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*27 */
	GPUOP(556000, 64375, 75000, POSDIV_POWER_4, 625, 0),  /*28 */
	GPUOP(535000, 64375, 75000, POSDIV_POWER_4, 625, 0),  /*29 */
	GPUOP(515000, 63750, 75000, POSDIV_POWER_4, 625, 0),  /*30 */
	GPUOP(494000, 63750, 75000, POSDIV_POWER_4, 625, 0),  /*31 */
	GPUOP(473000, 63750, 75000, POSDIV_POWER_4, 625, 0),  /*32 */
	GPUOP(452000, 63125, 75000, POSDIV_POWER_4, 625, 0),  /*33 */
	GPUOP(431000, 63125, 75000, POSDIV_POWER_4, 625, 0),  /*34 */
	GPUOP(410000, 63125, 75000, POSDIV_POWER_4, 625, 0),  /*35 */
	GPUOP(390000, 62500, 75000, POSDIV_POWER_4, 625, 0),  /*36 sign off */
};
/**************************************************
 * PTPOD Adjustment
 **************************************************/
#define PTPOD_OPP_GPU_NUM              ARRAY_SIZE(g_ptpod_opp_idx_table)
unsigned int g_ptpod_opp_idx_table[] = {
	0, 4, 8, 11,
	14, 16, 18, 20,
	22, 24, 26, 28,
	30, 32, 34, 36
};

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

#endif /* __GPUFREQ_MT6781_H__ */
