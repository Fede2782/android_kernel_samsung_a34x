/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6877_H__
#define __GPUFREQ_MT6877_H__

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
#define GPUFREQ_CUST_INIT_OPPIDX        (0)

#define PBM_RAEDY                       (0)
/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ               (1900000)       /* KHz */
#define POSDIV_2_MIN_FREQ               (750000)        /* KHz */
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_16_MAX_FREQ              (237500)        /* KHz */
#define POSDIV_16_MIN_FREQ              (125000)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)            /* MHz */
#define MFGPLL_FH_PLL                   (6)
#define MFGPLL1_CON0_OFS                (0x08)
#define MFGPLL1_CON1_OFS                (0x0C)
#define MFGPLL1_CON2_OFS                (0x10)
#define MFGPLL1_CON3_OFS                (0x14)
#define MFGPLL4_CON0_OFS                (0x38)
#define MFGPLL4_CON1_OFS                (0x3C)
#define MFGPLL4_CON2_OFS                (0x40)
#define MFGPLL4_CON3_OFS                (0x44)
#define MFGPLL1_CON0                    (g_gpu_pll_ctrl + MFGPLL1_CON0_OFS)
#define MFGPLL1_CON1                    (g_gpu_pll_ctrl + MFGPLL1_CON1_OFS)
#define MFGPLL1_CON2                    (g_gpu_pll_ctrl + MFGPLL1_CON2_OFS)
#define MFGPLL1_CON3                    (g_gpu_pll_ctrl + MFGPLL1_CON3_OFS)
#define MFGPLL4_CON0                    (g_gpu_pll_ctrl + MFGPLL4_CON0_OFS)
#define MFGPLL4_CON1                    (g_gpu_pll_ctrl + MFGPLL4_CON1_OFS)
#define MFGPLL4_CON2                    (g_gpu_pll_ctrl + MFGPLL4_CON2_OFS)
#define MFGPLL4_CON3                    (g_gpu_pll_ctrl + MFGPLL4_CON3_OFS)
#define PLL4H_FQMTR_CON0_OFS            (0x200)
#define PLL4H_FQMTR_CON1_OFS            (0x204)
#define PWR_STATUS_OFS                  (0xEF8)
#define PWR_STATUS_2ND_OFS              (0xEFC)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (0)
#define MFG_PLL_NAME                    "mfgpll1"
#define MFGPLL_PLL_FMAX		(3800UL  * MHZ)

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
#define MT_GPUFREQ_STATIC_PWR_READY2USE         1


/**************************************************
 * Shader Core Setting
 **************************************************/
#define SHADER_CORE_NUM                 (2)

#define MFG2_SHADER_STACK0         (T0C0)
#define MFG3_SHADER_STACK2         (T2C0)
#define MFG4_SHADER_STACK4         (T4C0)
#define MFG5_SHADER_STACK6         (T6C0)
#define MT_GPU_SHADER_PRESENT_4    (T0C0 | T2C0 | T4C0 | T6C0)

/**************************************************
 * Reference Power Setting MT6877
 **************************************************/
#define GPU_ACT_REF_POWER			(1223)		/* mW  */
#define GPU_ACT_REF_FREQ			(950000)	/* KHz */
#define GPU_ACT_REF_VOLT			(78125)		/* mV x 100 */

#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6877
 **************************************************/
#define VGPU_MAX_VOLT		(119375)         /* mV x 100 */
#define VGPU_MIN_VOLT       (40000)         /* mV x 100 */
#define VSRAM_MAX_VOLT      (129375)        /* mV x 100 */
#define VSRAM_MIN_VOLT      (50000)         /* mV x 100 */
#define PMIC_STEP		(625)

/*
 * (0)mv <= (VSRAM - VGPU) <= (200)mV
 */
#define MAX_BUCK_DIFF                   (20000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (0)        /* mV x 100 */

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


/*
 * PMIC hardware range:
 * vgpu      0.4 ~ 1.19300 V (MT6368)
 * vsram     0.5 ~ 1.29300 V (MT6363)
 */
#define PMIC_STEP                       (625)           /* mV x 100 */


/**************************************************
 * SRAMRC Setting
 **************************************************/
#define GPUFREQ_SAFE_VLOGIC             (60000)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (1)
#define GPUFREQ_BATT_PERCENT_ENABLE     (1)
#define GPUFREQ_LOW_BATT_ENABLE         (1)
#define GPUFREQ_BATT_OC_FREQ            (474000)
#define GPUFREQ_BATT_PERCENT_IDX        (1)
#define GPUFREQ_LOW_BATT_FREQ           (474000)

/**************************************************
 * AGING Setting
 **************************************************/
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * Enumeration MT6877
 **************************************************/
enum gpufreq_segment {
	MT6877_SEGMENT = 1,
	MT6877T_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
	CLOCK_SUB2,
};

/**************************************************
 * Structure MT6877
 **************************************************/
struct g_clk_info {
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *clk_pll4;
	struct clk *clk_fhctl;
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
	GPUOP(950000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /* 0 sign off */
	GPUOP(938000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /* 1 */
	GPUOP(926000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /* 2 */
	GPUOP(914000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /* 3 */
	GPUOP(902000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /* 4 */
	GPUOP(890000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /* 5 sign off */
	GPUOP(876000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /* 6 */
	GPUOP(862000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /* 7 */
	GPUOP(848000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /* 8 */
	GPUOP(835000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /* 9 */
	GPUOP(821000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /*10 */
	GPUOP(807000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /*11 */
	GPUOP(793000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /*12 */
	GPUOP(780000, 70000, 75000, POSDIV_POWER_4, 1875, 0), /*13 */
	GPUOP(766000, 69375, 75000, POSDIV_POWER_4, 1875, 0), /*14 */
	GPUOP(752000, 68750, 75000, POSDIV_POWER_4, 1250, 0), /*15 */
	GPUOP(738000, 68125, 75000, POSDIV_POWER_4, 1250, 0), /*16 */
	GPUOP(725000, 67500, 75000, POSDIV_POWER_4, 1250, 0), /*17 */
	GPUOP(711000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /*18 */
	GPUOP(697000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /*19 */
	GPUOP(683000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /*20 */
	GPUOP(670000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*21 sign off */
	GPUOP(652000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*22 */
	GPUOP(634000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /*23 */
	GPUOP(616000, 63125, 75000, POSDIV_POWER_4, 1250, 0), /*24 */
	GPUOP(598000, 62500, 75000, POSDIV_POWER_4, 1250, 0), /*25 */
	GPUOP(580000, 61875, 75000, POSDIV_POWER_4, 1250, 0), /*26 */
	GPUOP(563000, 61250, 75000, POSDIV_POWER_4,  625, 0), /*27 */
	GPUOP(545000, 60625, 75000, POSDIV_POWER_4,  625, 0), /*28 */
	GPUOP(527000, 60000, 75000, POSDIV_POWER_4,  625, 0), /*29 */
	GPUOP(509000, 59375, 75000, POSDIV_POWER_4,  625, 0), /*30 */
	GPUOP(491000, 58750, 75000, POSDIV_POWER_4,  625, 0), /*31 */
	GPUOP(474000, 58125, 75000, POSDIV_POWER_4,  625, 0), /*32 */
	GPUOP(456000, 57500, 75000, POSDIV_POWER_4,  625, 0), /*33 */
	GPUOP(438000, 56875, 75000, POSDIV_POWER_4,  625, 0), /*34 */
	GPUOP(420000, 56250, 75000, POSDIV_POWER_4,  625, 0), /*35 */
	GPUOP(402000, 55625, 75000, POSDIV_POWER_4,  625, 0), /*36 */
	GPUOP(385000, 55000, 75000, POSDIV_POWER_4,  625, 0), /*37 sign off */
};

struct gpufreq_opp_info g_opp_table_segment_2[] = {
	GPUOP(950000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /* 0 sign off */
	GPUOP(938000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /* 1 */
	GPUOP(926000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /* 2 */
	GPUOP(914000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /* 3 */
	GPUOP(902000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /* 4 */
	GPUOP(890000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /* 5 sign off */
	GPUOP(876000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /* 6 */
	GPUOP(862000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /* 7 */
	GPUOP(848000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /* 8 */
	GPUOP(835000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /* 9 */
	GPUOP(821000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /*10 */
	GPUOP(807000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /*11 */
	GPUOP(793000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /*12 */
	GPUOP(780000, 70000, 75000, POSDIV_POWER_4, 1875, 0), /*13 */
	GPUOP(766000, 69375, 75000, POSDIV_POWER_4, 1875, 0), /*14 */
	GPUOP(752000, 68750, 75000, POSDIV_POWER_4, 1250, 0), /*15 */
	GPUOP(738000, 68125, 75000, POSDIV_POWER_4, 1250, 0), /*16 */
	GPUOP(725000, 67500, 75000, POSDIV_POWER_4, 1250, 0), /*17 */
	GPUOP(711000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /*18 */
	GPUOP(697000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /*19 */
	GPUOP(683000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /*20 */
	GPUOP(670000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*21 sign off */
	GPUOP(652000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*22 */
	GPUOP(634000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*23 */
	GPUOP(616000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /*24 */
	GPUOP(598000, 63125, 75000, POSDIV_POWER_4, 1250, 0), /*25 */
	GPUOP(580000, 63125, 75000, POSDIV_POWER_4, 1250, 0), /*26 */
	GPUOP(563000, 62500, 75000, POSDIV_POWER_4,  625, 0), /*27 */
	GPUOP(545000, 61875, 75000, POSDIV_POWER_4,  625, 0), /*28 */
	GPUOP(527000, 61250, 75000, POSDIV_POWER_4,  625, 0), /*29 */
	GPUOP(509000, 61250, 75000, POSDIV_POWER_4,  625, 0), /*30 */
	GPUOP(491000, 60625, 75000, POSDIV_POWER_4,  625, 0), /*31 */
	GPUOP(474000, 60000, 75000, POSDIV_POWER_4,  625, 0), /*32 */
	GPUOP(456000, 59375, 75000, POSDIV_POWER_4,  625, 0), /*33 */
	GPUOP(438000, 59375, 75000, POSDIV_POWER_4,  625, 0), /*34 */
	GPUOP(420000, 58750, 75000, POSDIV_POWER_4,  625, 0), /*35 */
	GPUOP(402000, 58125, 75000, POSDIV_POWER_4,  625, 0), /*36 */
	GPUOP(385000, 57500, 75000, POSDIV_POWER_4,  625, 0), /*37 sign off */
};

struct gpufreq_opp_info g_opp_table_segment_3[] = {
	GPUOP(950000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /* 0 sign off */
	GPUOP(938000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /* 1 */
	GPUOP(926000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /* 2 */
	GPUOP(914000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /* 3 */
	GPUOP(902000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /* 4 */
	GPUOP(890000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /* 5 sign off */
	GPUOP(876000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /* 6 */
	GPUOP(862000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /* 7 */
	GPUOP(848000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /* 8 */
	GPUOP(835000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /* 9 */
	GPUOP(821000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /*10 */
	GPUOP(807000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /*11 */
	GPUOP(793000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /*12 */
	GPUOP(780000, 70000, 75000, POSDIV_POWER_4, 1875, 0), /*13 */
	GPUOP(766000, 69375, 75000, POSDIV_POWER_4, 1875, 0), /*14 */
	GPUOP(752000, 68750, 75000, POSDIV_POWER_4, 1250, 0), /*15 */
	GPUOP(738000, 68125, 75000, POSDIV_POWER_4, 1250, 0), /*16 */
	GPUOP(725000, 67500, 75000, POSDIV_POWER_4, 1250, 0), /*17 */
	GPUOP(711000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /*18 */
	GPUOP(697000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /*19 */
	GPUOP(683000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /*20 */
	GPUOP(670000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*21 sign off */
	GPUOP(652000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /*22 */
	GPUOP(634000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*23 */
	GPUOP(616000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /*24 */
	GPUOP(598000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /*25 */
	GPUOP(580000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /*26 */
	GPUOP(563000, 63125, 75000, POSDIV_POWER_4,  625, 0), /*27 */
	GPUOP(545000, 63125, 75000, POSDIV_POWER_4,  625, 0), /*28 */
	GPUOP(527000, 62500, 75000, POSDIV_POWER_4,  625, 0), /*29 */
	GPUOP(509000, 62500, 75000, POSDIV_POWER_4,  625, 0), /*30 */
	GPUOP(491000, 61875, 75000, POSDIV_POWER_4,  625, 0), /*31 */
	GPUOP(474000, 61875, 75000, POSDIV_POWER_4,  625, 0), /*32 */
	GPUOP(456000, 61250, 75000, POSDIV_POWER_4,  625, 0), /*33 */
	GPUOP(438000, 61250, 75000, POSDIV_POWER_4,  625, 0), /*34 */
	GPUOP(420000, 60625, 75000, POSDIV_POWER_4,  625, 0), /*35 */
	GPUOP(402000, 60625, 75000, POSDIV_POWER_4,  625, 0), /*36 */
	GPUOP(385000, 60000, 75000, POSDIV_POWER_4,  625, 0), /*37 sign off */
};

/**************************************************
 * Aging Adjustment
 **************************************************/
unsigned int g_aging_table[][SIGNED_OPP_GPU_NUM] = {
	/* Aging Table 0 */
	{1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875,
	 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1250,
	 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250,
	 1250, 1250, 1250,  625,  625,  625,  625,  625,
	  625,  625,  625,  625,  625,  625},
	/* Aging Table 1 */
	{1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250,
	 1250, 1250, 1250, 1250, 1250, 1250, 1250,  625,
	  625,  625,  625,  625,  625,  625,  625,  625,
	  625,  625,  625,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0},
	/* Aging Table 2 */
	{ 625,  625,  625,  625,  625,  625,  625,  625,
	  625,  625,  625,  625,  625,  625,  625,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0},
	/* Aging Table 3 */
	{   0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0},
};

/**************************************************
 * PTPOD Adjustment
 **************************************************/
#define PTPOD_OPP_GPU_NUM              ARRAY_SIZE(g_ptpod_opp_idx_table)
unsigned int g_ptpod_opp_idx_table[] = {
	0,  3,  5,  9,
	12, 15, 18, 21,
	23, 25, 27, 29,
	31, 33, 35, 37
};

#define MT_GPU_SHADER_PRESENT_4    (T0C0 | T2C0 | T4C0 | T6C0)

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

#endif /* __GPUFREQ_MT6877_H__ */
