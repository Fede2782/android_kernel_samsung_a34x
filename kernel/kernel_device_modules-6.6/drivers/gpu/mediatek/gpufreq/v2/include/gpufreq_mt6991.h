/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6991_H__
#define __GPUFREQ_MT6991_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
#define GPUFREQ_KDEBUG_VERSION              (0x20241029)

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ                   (1900000)         /* KHz */
#define POSDIV_2_MIN_FREQ                   (750000)          /* KHz */
#define POSDIV_4_MAX_FREQ                   (950000)          /* KHz */
#define POSDIV_4_MIN_FREQ                   (375000)          /* KHz */
#define POSDIV_8_MAX_FREQ                   (475000)          /* KHz */
#define POSDIV_8_MIN_FREQ                   (187500)          /* KHz */
#define POSDIV_16_MAX_FREQ                  (237500)          /* KHz */
#define POSDIV_16_MIN_FREQ                  (125000)          /* KHz */
#define POSDIV_SHIFT                        (24)              /* bit */
#define DDS_SHIFT                           (14)              /* bit */
#define MFGPLL_FIN                          (26)              /* MHz */
#define MFG_REF_SEL_BIT                     (BIT(0))
#define MFG_TOP_SEL_BIT                     (BIT(0))
#define MFG_SC0_SEL_BIT                     (BIT(1))
#define MFG_SC1_SEL_BIT                     (BIT(2))
#define FREQ_ROUNDUP_TO_10(freq)            ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define MFG_0_1_PWR_STATUS \
	(DRV_Reg32(MFG_RPC_PWR_CON_STATUS) & GENMASK(1, 0))
#define MFG_0_23_37_PWR_STATUS \
	((DRV_Reg32(MFG_RPC_PWR_CON_STATUS) & GENMASK(23, 0)) & ~(BIT(6) | GENMASK(22, 21)) | \
	(((DRV_Reg32(MFG_RPC_MFG37_PWR_CON) & BIT(30)) >> 30) << 24))
#define MFG_0_31_PWR_STATUS \
	DRV_Reg32(MFG_RPC_PWR_CON_STATUS)
#define MFG_32_37_PWR_STATUS \
	(DRV_Reg32(MFG_RPC_PWR_CON_STATUS_1) | \
	(((DRV_Reg32(MFG_RPC_MFG37_PWR_CON) & BIT(30)) >> 30) << 5))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0 | T0C1)   /* MFG9,  MFG12 */
#define MFG4_SHADER_STACK1                  (T1C0 | T1C1)   /* MFG10, MFG13 */
#define MFG5_SHADER_STACK2                  (T2C0 | T2C1)   /* MFG11, MFG14 */
#define MFG7_SHADER_STACK5                  (T5C0 | T5C1)   /* MFG15, MFG18 */
#define MFG8_SHADER_STACK6                  (T6C0 | T6C1)   /* MFG16, MFG19 */
#define MFG23_SHADER_STACK7                 (T7C0 | T7C1)   /* MFG17, MFG20 */
#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | T1C0)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | T2C0)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2)
#define GPU_SHADER_PRESENT_7 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 T5C0)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_9 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5 | T6C0)
#define GPU_SHADER_PRESENT_10 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)
#define GPU_SHADER_PRESENT_11 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6 | T7C0)
#define GPU_SHADER_PRESENT_12 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6 | MFG23_SHADER_STACK7)
#define SHADER_CORE_NUM                 (12)
struct gpufreq_core_mask_info g_core_mask_table[SHADER_CORE_NUM] = {
	{12, GPU_SHADER_PRESENT_12},
	{11, GPU_SHADER_PRESENT_11},
	{10, GPU_SHADER_PRESENT_10},
	{9, GPU_SHADER_PRESENT_9},
	{8, GPU_SHADER_PRESENT_8},
	{7, GPU_SHADER_PRESENT_7},
	{6, GPU_SHADER_PRESENT_6},
	{5, GPU_SHADER_PRESENT_5},
	{4, GPU_SHADER_PRESENT_4},
	{3, GPU_SHADER_PRESENT_3},
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Dynamic Power Setting
 **************************************************/
#define GPU_DYN_REF_POWER                   (1102)          /* mW  */
#define GPU_DYN_REF_POWER_FREQ              (1360000)       /* KHz */
#define GPU_DYN_REF_POWER_VOLT              (90000)         /* mV x 100 */
#define STACK_DYN_REF_POWER_A0              (30514)         /* mW  */
#define STACK_DYN_REF_POWER_B0              (28652)         /* mW  */
#define STACK_DYN_REF_POWER_FREQ            (1800000)       /* KHz */
#define STACK_DYN_REF_POWER_VOLT            (100000)        /* mV x 100 */

/**************************************************
 * Bus Tracker Setting
 **************************************************/
#define BUS_TRACKER_OP(_info, _count, _type, _timestamp, _log, _id, _addr) \
	{ \
		_info.type = _type; \
		_info.timestamp = _timestamp; \
		_info.log = _log; \
		_info.id = _id; \
		_info.addr = _addr; \
		_count++; \
	}

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_eco_version {
	MT6991_A0 = 0,
	MT6991_B0,
};

#endif /* __GPUFREQ_MT6991_H__ */
