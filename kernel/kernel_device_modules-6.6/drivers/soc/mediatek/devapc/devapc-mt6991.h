/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6991_H__
#define __DEVAPC_MT6991_H__

#include "devapc-mtk-multi-ao.h"

/******************************************************************************
 * VARIABLE DEFINITION
 ******************************************************************************/
/* dbg status default setting */
#define PLAT_DBG_UT_DEFAULT		false
#define PLAT_DBG_KE_DEFAULT		true
#define PLAT_DBG_AEE_DEFAULT		true
#define PLAT_DBG_WARN_DEFAULT		true
#define PLAT_DBG_DAPC_DEFAULT		false

/******************************************************************************
 * STRUCTURE DEFINITION
 ******************************************************************************/
enum DEVAPC_SLAVE_TYPE {
	SLAVE_TYPE_APINFRA_IO = 0,
	SLAVE_TYPE_APINFRA_IO_CTRL,
	SLAVE_TYPE_APINFRA_IO_INTF,
	SLAVE_TYPE_APINFRA_BIG4,
	SLAVE_TYPE_APINFRA_DRAMC,
	SLAVE_TYPE_APINFRA_EMI,
	SLAVE_TYPE_APINFRA_SSR,
	SLAVE_TYPE_APINFRA_MEM,
	SLAVE_TYPE_APINFRA_MEM_CTRL,
	SLAVE_TYPE_APINFRA_MEM_INTF,
	SLAVE_TYPE_APINFRA_INT,
	SLAVE_TYPE_APINFRA_MMU,
	SLAVE_TYPE_APINFRA_SLB,
	SLAVE_TYPE_PERI_PAR,
	SLAVE_TYPE_VLP,
	SLAVE_TYPE_ADSP,
	SLAVE_TYPE_MMINFRA,
	SLAVE_TYPE_MMUP,
	SLAVE_TYPE_GPU,
	SLAVE_TYPE_GPU1,
	SLAVE_TYPE_NUM,
};

enum DEVAPC_VIO_MASK_STA_NUM {
	VIO_MASK_STA_NUM_APINFRA_IO = 12,
	VIO_MASK_STA_NUM_APINFRA_IO_CTRL = 1,
	VIO_MASK_STA_NUM_APINFRA_IO_INTF = 1,
	VIO_MASK_STA_NUM_APINFRA_BIG4 = 7,
	VIO_MASK_STA_NUM_APINFRA_DRAMC = 4,
	VIO_MASK_STA_NUM_APINFRA_EMI = 10,
	VIO_MASK_STA_NUM_APINFRA_SSR = 3,
	VIO_MASK_STA_NUM_APINFRA_MEM = 1,
	VIO_MASK_STA_NUM_APINFRA_MEM_CTRL = 2,
	VIO_MASK_STA_NUM_APINFRA_MEM_INTF = 1,
	VIO_MASK_STA_NUM_APINFRA_INT = 2,
	VIO_MASK_STA_NUM_APINFRA_MMU = 2,
	VIO_MASK_STA_NUM_APINFRA_SLB = 2,
	VIO_MASK_STA_NUM_PERI_PAR = 9,
	VIO_MASK_STA_NUM_VLP = 5,
	VIO_MASK_STA_NUM_ADSP = 4,
	VIO_MASK_STA_NUM_MMINFRA = 23,
	VIO_MASK_STA_NUM_MMUP = 5,
	VIO_MASK_STA_NUM_GPU = 1,
	VIO_MASK_STA_NUM_GPU1 = 1,
};

enum DEVAPC_PD_OFFSET {
	PD_VIO_MASK_OFFSET = 0x0,
	PD_VIO_STA_OFFSET = 0x400,
	PD_VIO_DBG0_OFFSET = 0x900,
	PD_VIO_DBG1_OFFSET = 0x904,
	PD_VIO_DBG2_OFFSET = 0x908,
	PD_APC_CON_OFFSET = 0xF00,
	PD_SHIFT_STA_OFFSET = 0xF20,
	PD_SHIFT_SEL_OFFSET = 0xF30,
	PD_SHIFT_CON_OFFSET = 0xF10,
	PD_VIO_DBG3_OFFSET = 0x90C,
};

#define SRAMROM_SLAVE_TYPE	SLAVE_TYPE_APINFRA_IO	/* APINFRA_IO */
#define MM2ND_SLAVE_TYPE	SLAVE_TYPE_NUM		/* No MM2ND */

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 354,
	CONN_VIO_INDEX = 0,
	MDP_VIO_INDEX = 0,
	DISP2_VIO_INDEX = 0,
	MMSYS_VIO_INDEX = 0,
};

enum INFRACFG_MM2ND_VIO_NUM {
	INFRACFG_MM_VIO_STA_NUM = 4,
	INFRACFG_MDP_VIO_STA_NUM = 10,
	INFRACFG_DISP2_VIO_STA_NUM = 4,
};

enum INFRACFG_MM2ND_OFFSET {
	INFRACFG_MM_SEC_VIO0_OFFSET = 0xB30,
	INFRACFG_MDP_SEC_VIO0_OFFSET = 0xB40,
	INFRACFG_DISP2_SEC_VIO0_OFFSET = 0xB70,
};

enum BUSID_LENGTH {
	APINFRAAXI_MI_BIT_LENGTH = 18,
	ADSPAXI_MI_BIT_LENGTH = 8,
	MMINFRAAXI_MI_BIT_LENGTH = 19,
};

struct INFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[APINFRAAXI_MI_BIT_LENGTH];
};

struct ADSPAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[ADSPAXI_MI_BIT_LENGTH];
};

struct MMINFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[MMINFRAAXI_MI_BIT_LENGTH];
};

enum DEVAPC_IRQ_TYPE {
	IRQ_TYPE_APINFRA_IO = 0,
	IRQ_TYPE_APINFRA_IO_CTRL,
	IRQ_TYPE_APINFRA_IO_INTF,
	IRQ_TYPE_APINFRA_BIG4,
	IRQ_TYPE_APINFRA_DRAMC,
	IRQ_TYPE_APINFRA_EMI,
	IRQ_TYPE_APINFRA_SSR,
	IRQ_TYPE_APINFRA_MEM,
	IRQ_TYPE_APINFRA_MEM_CTRL,
	IRQ_TYPE_APINFRA_MEM_INTF,
	IRQ_TYPE_APINFRA_INT,
	IRQ_TYPE_APINFRA_MMU,
	IRQ_TYPE_APINFRA_SLB,
	IRQ_TYPE_PERI,
	IRQ_TYPE_VLP,
	IRQ_TYPE_ADSP,
	IRQ_TYPE_MMINFRA,
	IRQ_TYPE_MMUP,
	IRQ_TYPE_GPU,
	IRQ_TYPE_NUM,
};

enum ADSP_MI_SELECT {
	ADSP_MI12 = 0,
	ADSP_MI17
};

/******************************************************************************
 * PLATFORM DEFINATION
 ******************************************************************************/

/* For Infra VIO_DBG */
#define INFRA_VIO_DBG_MSTID			0xFFFFFFFF
#define INFRA_VIO_DBG_MSTID_START_BIT		0
#define INFRA_VIO_DBG_DMNID			0x0000003F
#define INFRA_VIO_DBG_DMNID_START_BIT		0
#define INFRA_VIO_DBG_W_VIO			0x00000040
#define INFRA_VIO_DBG_W_VIO_START_BIT		6
#define INFRA_VIO_DBG_R_VIO			0x00000080
#define INFRA_VIO_DBG_R_VIO_START_BIT		7
#define INFRA_VIO_ADDR_HIGH			0xFFFFFFFF
#define INFRA_VIO_ADDR_HIGH_START_BIT		0

/* For SRAMROM VIO */
#define SRAMROM_SEC_VIO_ID_MASK			0x00FFFF00
#define SRAMROM_SEC_VIO_ID_SHIFT		8
#define SRAMROM_SEC_VIO_DOMAIN_MASK		0x0F000000
#define SRAMROM_SEC_VIO_DOMAIN_SHIFT		24
#define SRAMROM_SEC_VIO_RW_MASK			0x80000000
#define SRAMROM_SEC_VIO_RW_SHIFT		31

/* For MM 2nd VIO */
#define INFRACFG_MM2ND_VIO_DOMAIN_MASK		0x00000030
#define INFRACFG_MM2ND_VIO_DOMAIN_SHIFT		4
#define INFRACFG_MM2ND_VIO_ID_MASK		0x00FFFF00
#define INFRACFG_MM2ND_VIO_ID_SHIFT		8
#define INFRACFG_MM2ND_VIO_RW_MASK		0x01000000
#define INFRACFG_MM2ND_VIO_RW_SHIFT		24

#define SRAM_START_ADDR				(0x100000)
#define SRAM_END_ADDR				(0x1FFFFF)

#define L3CACHE_0_START				(0x01000000)
#define L3CACHE_0_END				(0x02FFFFFF)
#define L3CACHE_1_START				(0x0A000000)
#define L3CACHE_1_END				(0x0CFFFFFF)

/* For APInfra Bus Tracer1 Parser*/
#define APINFRA_DEBUG_START			(0x0D000000)
#define APINFRA_DEBUG_END			(0x0EFFFFFF)
#define APINFRA_HFRP_START			(0x31800000)
#define APINFRA_HFRP_END			(0x31BFFFFF)
#define APINFRA_MFG_START			(0x48000000)
#define APINFRA_MFG_END				(0x4BFFFFFF)
#define APINFRA_PERI_0_START			(0x16000000)
#define APINFRA_PERI_0_END			(0x169FFFFF)
#define APINFRA_PERI_1_START			(0x50000000)
#define APINFRA_PERI_1_END			(0x79FFFFFF)
#define APINFRA_SSR_START			(0x18000000)
#define APINFRA_SSR_END				(0x180F3FFF)

/* For APInfra MEM Bus Parser*/
#define APINFRA_MMU_0_START			(0x14060000)
#define APINFRA_MMU_0_END			(0x14063FFF)
#define APINFRA_MMU_1_START			(0x14070000)
#define APINFRA_MMU_1_END			(0x14071FFF)
#define APINFRA_MMU_2_START			(0x14800000)
#define APINFRA_MMU_2_END			(0x14800FFF)
#define APINFRA_MMU_3_START			(0x14804000)
#define APINFRA_MMU_3_END			(0x14810FFF)
#define APINFRA_SLB_0_START			(0x14080000)
#define APINFRA_SLB_0_END			(0x14083FFF)
#define APINFRA_SLB_1_START			(0x14091000)
#define APINFRA_SLB_1_END			(0x14090FFF)
#define APINFRA_SLB_2_START			(0x140E0000)
#define APINFRA_SLB_2_END			(0x140E0FFF)
#define APINFRA_SLB_3_START			(0x140F0000)
#define APINFRA_SLB_3_END			(0x140F9FFF)

/* For VLP Bus Parser */
#define VLP_SCP_START_ADDR			(0x1C600000)
#define VLP_SCP_END_ADDR			(0x1CFFFFFF)
#define VLP_INFRA_START				(0x00000000)
#define VLP_INFRA_END				(0x1BFFFFFF)
#define VLP_INFRA_1_START			(0x1D000000)
#define VLP_INFRA_1_END				(0xFFFFFFFFF)

/* For ADSP Bus Parser */
#define ADSP_INFRA_START			(0x00000000)
#define ADSP_INFRA_END				(0x1DFFFFFF)
#define ADSP_INFRA_1_START			(0x1E280000)
#define ADSP_INFRA_1_END			(0x3FFFFFFF)
#define ADSP_INFRA_2_START			(0x4D000000)
#define ADSP_INFRA_2_END			(0x4DFFFFFF)
#define ADSP_OTHER_START			(0x1E000000)
#define ADSP_OTHER_END				(0x1E01F9FF)

/* For MMINFRA Bus Parser */
#define IMG_START_ADDR				(0x34000000)
#define IMG_END_ADDR				(0x34805FFF)
#define CAM_START_ADDR				(0x3A000000)
#define CAM_END_ADDR				(0x3CFFFFFF)
#define CODEC_START_ADDR			(0x36000000)
#define CODEC_END_ADDR				(0x38FFFFFF)
#define DISP_START_ADDR				(0x32000000)
#define DISP_END_ADDR				(0x327FFFFF)
#define OVL_START_ADDR				(0x32800000)
#define OVL_END_ADDR				(0x32FFFFFF)
#define MML_START_ADDR				(0x3E000000)
#define MML_END_ADDR				(0x3EFFFFFF)

/* For GPU Bus Parser */
#define GPU1_PD_START				(0xC00000)
#define GPU1_PD_END				(0xC63FFF)

/* For Debugsys */
#define DEBUGSYS_VIO_BIT			(0x2)
#define DEBUGSYS_VIO_BUS_ID_MASK		(0x003FFFF0)

static const struct mtk_device_info mt6991_devices_apinfra_io[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DEBUG_TRACKER_APB_S", true},
	{0, 1, 1, "DEBUG_TRACKER_APB_S-1", true},
	{0, 2, 2, "DEBUG_TRACKER_APB1_S", true},
	{0, 3, 3, "DEBUG_TRACKER_APB1_S-1", true},
	{0, 4, 4, "DEBUG_TRACKER_APB2_S", true},
	{0, 5, 5, "DEBUG_TRACKER_APB2_S-1", true},
	{0, 6, 6, "INFRA2PERI_S", true},
	{0, 7, 7, "INFRA2PERI_S-1", true},
	{0, 8, 8, "INFRA2PERI_S-2", true},
	{0, 9, 9, "INFRA2PERI_S-3", true},

	/* 10 */
	{0, 10, 10, "INFRA2PERI_S-4", true},
	{0, 11, 11, "INFRA2MM_S", true},
	{0, 12, 12, "INFRA2MM_S-1", true},
	{0, 13, 13, "INFRA2MM_S-2", true},
	{0, 14, 14, "INFRA2MM_S-3", true},
	{0, 15, 15, "INFRA2MM_S-4", true},
	{0, 16, 16, "INFRA2MM_S-5", true},
	{0, 17, 17, "INFRA2MM_S-6", true},
	{0, 18, 18, "INFRA2MM_S-7", true},
	{0, 19, 19, "INFRA2MM_S-8", true},

	/* 20 */
	{0, 20, 20, "L3C_S", true},
	{0, 21, 21, "L3C_S-1", true},
	{0, 22, 22, "L3C_S-2", true},
	{0, 23, 23, "L3C_S-3", true},
	{0, 24, 24, "L3C_S-4", true},
	{0, 25, 25, "L3C_S-5", true},
	{0, 26, 26, "L3C_S-6", true},
	{0, 27, 27, "L3C_S-7", true},
	{0, 28, 28, "L3C_S-8", true},
	{0, 29, 29, "L3C_S-9", true},

	/* 30 */
	{0, 30, 30, "L3C_S-10", true},
	{0, 31, 31, "L3C_S-11", true},
	{0, 32, 32, "L3C_S-12", true},
	{0, 33, 33, "L3C_S-13", true},
	{0, 34, 34, "L3C_S-14", true},
	{0, 35, 35, "L3C_S-15", true},
	{0, 36, 36, "L3C_S-16", true},
	{0, 37, 37, "L3C_S-17", true},
	{0, 38, 38, "L3C_S-18", true},
	{0, 39, 39, "L3C_S-19", true},

	/* 40 */
	{0, 40, 40, "L3C_S-20", true},
	{0, 41, 41, "L3C_S-21", true},
	{0, 42, 42, "L3C_S-22", true},
	{0, 43, 43, "L3C_S-23", true},
	{0, 44, 44, "L3C_S-24", true},
	{0, 45, 45, "L3C_S-25", true},
	{0, 46, 46, "L3C_S-26", true},
	{0, 47, 47, "L3C_S-27", true},
	{0, 48, 48, "L3C_S-28", true},
	{0, 49, 49, "L3C_S-29", true},

	/* 50 */
	{0, 50, 50, "L3C_S-30", true},
	{0, 51, 51, "L3C_S-31", true},
	{0, 52, 52, "L3C_S-32", true},
	{0, 53, 53, "L3C_S-33", true},
	{0, 54, 54, "L3C_S-34", true},
	{0, 55, 55, "L3C_S-35", true},
	{0, 56, 56, "L3C_S-36", true},
	{0, 57, 57, "L3C_S-37", true},
	{0, 58, 58, "L3C_S-38", true},
	{0, 59, 59, "L3C_S-39", true},

	/* 60 */
	{0, 60, 60, "L3C_S-40", true},
	{0, 61, 61, "L3C_S-41", true},
	{0, 62, 62, "L3C_S-42", true},
	{0, 63, 63, "L3C_S-43", true},
	{0, 64, 64, "L3C_S-44", true},
	{0, 65, 65, "L3C_S-45", true},
	{0, 66, 66, "L3C_S-46", true},
	{0, 67, 67, "L3C_S-47", true},
	{0, 68, 68, "L3C_S-48", true},
	{0, 69, 69, "L3C_S-49", true},

	/* 70 */
	{0, 70, 70, "L3C_S-50", true},
	{0, 71, 71, "L3C_S-51", true},
	{0, 72, 72, "L3C_S-52", true},
	{0, 73, 73, "APU_S_S", true},
	{0, 74, 74, "APU_S_S-1", true},
	{0, 75, 75, "MD_AP_S", true},
	{0, 76, 76, "MD_AP_S-1", true},
	{0, 77, 77, "MD_AP_S-2", true},
	{0, 78, 78, "MD_AP_S-3", true},
	{0, 79, 79, "MD_AP_S-4", true},

	/* 80 */
	{0, 80, 80, "MD_AP_S-5", true},
	{0, 81, 81, "MD_AP_S-6", true},
	{0, 82, 82, "MD_AP_S-7", true},
	{0, 83, 83, "MD_AP_S-8", true},
	{0, 84, 84, "MD_AP_S-9", true},
	{0, 85, 85, "MD_AP_S-10", true},
	{0, 86, 86, "MD_AP_S-11", true},
	{0, 87, 87, "MD_AP_S-12", true},
	{0, 88, 88, "MD_AP_S-13", true},
	{0, 89, 89, "MD_AP_S-14", true},

	/* 90 */
	{0, 90, 90, "MD_AP_S-15", true},
	{0, 91, 91, "MD_AP_S-16", true},
	{0, 92, 92, "MD_AP_S-17", true},
	{0, 93, 93, "MD_AP_S-18", true},
	{0, 94, 94, "MD_AP_S-19", true},
	{0, 95, 95, "MD_AP_S-20", true},
	{0, 96, 96, "MD_AP_S-21", true},
	{0, 97, 97, "MD_AP_S-22", true},
	{0, 98, 98, "MD_AP_S-23", true},
	{0, 99, 99, "MD_AP_S-24", true},

	/* 100 */
	{0, 100, 100, "MD_AP_S-25", true},
	{0, 101, 101, "MD_AP_S-26", true},
	{0, 102, 102, "MD_AP_S-27", true},
	{0, 103, 103, "MD_AP_S-28", true},
	{0, 104, 104, "MD_AP_S-29", true},
	{0, 105, 105, "MD_AP_S-30", true},
	{0, 106, 106, "MD_AP_S-31", true},
	{0, 107, 107, "MD_AP_S-32", true},
	{0, 108, 108, "MD_AP_S-33", true},
	{0, 109, 109, "MD_AP_S-34", true},

	/* 110 */
	{0, 110, 110, "MD_AP_S-35", true},
	{0, 111, 111, "MD_AP_S-36", true},
	{0, 112, 112, "MD_AP_S-37", true},
	{0, 113, 113, "MD_AP_S-38", true},
	{0, 114, 114, "MD_AP_S-39", true},
	{0, 115, 115, "MD_AP_S-40", true},
	{0, 116, 116, "MD_AP_S-41", true},
	{0, 117, 117, "MD_AP_S-42", true},
	{0, 118, 118, "MD_AP_S-43", true},
	{0, 119, 119, "MD_AP_S-44", true},

	/* 120 */
	{0, 120, 120, "MD_AP_S-45", true},
	{0, 121, 121, "MD_AP_S-46", true},
	{0, 122, 122, "CONN_S", true},
	{0, 123, 124, "TOPCKGEN_APB_S", true},
	{0, 124, 138, "INFRACFG_PDN_APB_S", true},
	{0, 125, 139, "SEC_INFRACFG_PDN_APB_S", true},
	{0, 126, 140, "BCRM_APINFRA_IO_PDN_APB_S", true},
	{0, 127, 141, "DEVAPC_APINFRA_IO_PDN_APB_S", true},
	{0, 128, 142, "IRQ2AXI_APB_S", true},
	{0, 129, 143, "SRAMROM_APB_S", true},

	/* 130 */
	{0, 130, 146, "HWCCF_APB_S0", true},
	{0, 131, 147, "HWCCF_APB_S1", true},
	{0, 132, 148, "HWCCF_APB_S2", true},
	{0, 133, 149, "HWCCF_APB_S3", true},
	{0, 134, 150, "HWCCF_APB_S4", true},
	{0, 135, 151, "HWCCF_APB_S5", true},
	{0, 136, 152, "HWCCF_APB_S6", true},
	{0, 137, 153, "HWCCF_APB_S7", true},
	{0, 138, 154, "HWCCF_APB_S8", true},
	{0, 139, 155, "HWCCF_APB_S9", true},

	/* 140 */
	{0, 140, 156, "HWCCF_APB_S10", true},
	{0, 141, 157, "HWCCF_APB_S11", true},
	{0, 142, 158, "HWCCF_APB_S12", true},
	{0, 143, 159, "HWCCF_APB_S13", true},
	{0, 144, 160, "HWCCF_APB_S14", true},
	{0, 145, 161, "HWCCF_APB_S15", true},
	{0, 146, 162, "HWCCF_APB_S16", true},
	{0, 147, 163, "HWCCF_APB_S17", true},
	{0, 148, 164, "HWCCF_APB_S18", true},
	{0, 149, 165, "HWCCF_APB_S19", true},

	/* 150 */
	{0, 150, 166, "APINFRA_IO_BUS_HRE_APB_S", true},
	{0, 151, 167, "IPI_APB_S0", true},
	{0, 152, 168, "IPI_APB_S1", true},
	{0, 153, 169, "IPI_APB_S2", true},
	{0, 154, 170, "IPI_APB_S3", true},
	{0, 155, 171, "IPI_APB_S4", true},
	{0, 156, 172, "IPI_APB_S5", true},
	{0, 157, 173, "IPI_APB_S6", true},
	{0, 158, 174, "IPI_APB_S7", true},
	{0, 159, 175, "PTP_THERM_CTRL_APB_S", true},

	/* 160 */
	{0, 160, 176, "DBGSYS_PDN_APB_PM", true},
	{0, 161, 177, "FADSP_MON_APB_S", true},
	{0, 162, 178, "FVLP_MON1_APB_S", true},
	{0, 163, 179, "FIRQ_MON_APB_S", true},
	{0, 164, 180, "FPERI_MON_APB_S", true},
	{0, 165, 181, "FMM_MON_APB_S", true},
	{0, 166, 182, "FL3C_MON_APB_S", true},
	{0, 167, 183, "FMFG_MON_APB_S", true},
	{0, 168, 184, "FGPU_MON_APB_S", true},
	{0, 169, 185, "FAPU_MON_APB_S", true},

	/* 170 */
	{0, 170, 186, "FINT_MON_APB_S", true},
	{0, 171, 187, "FMD_MON0_APB_S", true},
	{0, 172, 188, "FMD_MON1_APB_S", true},
	{0, 173, 189, "FSSR_MON_APB_S", true},
	{0, 174, 190, "FSLV_MON_APB_S", true},
	{0, 175, 191, "FDBG_MON_APB_S", true},
	{0, 176, 192, "FHFRP_MON_APB_S", true},
	{0, 177, 193, "FPERI_IO_MON_APB_S", true},
	{0, 178, 194, "ADSPSYS_S", true},
	{0, 179, 195, "VLPSYS_S", true},

	/* 180 */
	{0, 180, 205, "MFG_S_S", true},
	{0, 181, 206, "INFRA2HFRP_S", true},
	{0, 182, 209, "APMIXEDSYS_APB_S", true},
	{0, 183, 210, "APMIXEDSYS_APB_S-1", true},
	{0, 184, 211, "BCRM_APINFRA_IO_AO_APB_S", true},
	{0, 185, 212, "DEVAPC_APINFRA_IO_AO_APB_S", true},
	{0, 186, 213, "DEBUG_CTRL_APINFRA_IO_AO_APB_S", true},
	{0, 187, 214, "APINFRA_SECURITY_AO_APB_S", true},
	{0, 188, 215, "SEC_APINFRA_SECURITY_AO_APB_S", true},
	{0, 189, 216, "IO_EFUSE_AO_APB_S", true},

	/* 190 */
	{0, 190, 217, "GPIO_APB_S", true},
	{0, 191, 218, "GPIO_SECURE_APB_S", true},
	{0, 192, 219, "PMSR_APB_S", true},
	{0, 193, 220, "RESERVED_APB_S", true},
	{0, 194, 222, "DRM_DEBUG_TOP_APB_S", true},
	{0, 195, 223, "TOP_MISC_APB_S", true},
	{0, 196, 224, "SYS_CIRQ_APB_S", true},
	{0, 197, 225, "MBIST_AO_APB_S", true},
	{0, 198, 226, "INFRACFG_AO_APB_S", true},
	{0, 199, 227, "SEC_INFRACFG_AO_APB_S", true},

	/* 200 */
	{0, 200, 228, "DBGSYS_AO_APB_PM", true},
	{0, 201, 229, "IO_PBUS_APB_S", true},

	{-1, -1, 230, "NA", true},
	{-1, -1, 231, "NA", true},
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},

	/* 210 */
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},
	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},
	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},

	/* 220 */
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},
	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},
	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},

	/* 230 */
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},
	{-1, -1, 260, "OOB_way_en", true},
	{-1, -1, 261, "OOB_way_en", true},
	{-1, -1, 262, "OOB_way_en", true},
	{-1, -1, 263, "OOB_way_en", true},
	{-1, -1, 264, "OOB_way_en", true},
	{-1, -1, 265, "OOB_way_en", true},
	{-1, -1, 266, "OOB_way_en", true},
	{-1, -1, 267, "OOB_way_en", true},

	/* 240 */
	{-1, -1, 268, "OOB_way_en", true},
	{-1, -1, 269, "OOB_way_en", true},
	{-1, -1, 270, "OOB_way_en", true},
	{-1, -1, 271, "OOB_way_en", true},
	{-1, -1, 272, "OOB_way_en", true},
	{-1, -1, 273, "OOB_way_en", true},
	{-1, -1, 274, "OOB_way_en", true},
	{-1, -1, 275, "OOB_way_en", true},
	{-1, -1, 276, "OOB_way_en", true},
	{-1, -1, 277, "OOB_way_en", true},

	/* 250 */
	{-1, -1, 278, "OOB_way_en", true},
	{-1, -1, 279, "OOB_way_en", true},
	{-1, -1, 280, "OOB_way_en", true},
	{-1, -1, 281, "OOB_way_en", true},
	{-1, -1, 282, "OOB_way_en", true},
	{-1, -1, 283, "OOB_way_en", true},
	{-1, -1, 284, "OOB_way_en", true},
	{-1, -1, 285, "OOB_way_en", true},
	{-1, -1, 286, "OOB_way_en", true},
	{-1, -1, 287, "OOB_way_en", true},

	/* 260 */
	{-1, -1, 288, "OOB_way_en", true},
	{-1, -1, 289, "OOB_way_en", true},
	{-1, -1, 290, "OOB_way_en", true},
	{-1, -1, 291, "OOB_way_en", true},
	{-1, -1, 292, "OOB_way_en", true},
	{-1, -1, 293, "OOB_way_en", true},
	{-1, -1, 294, "OOB_way_en", true},
	{-1, -1, 295, "OOB_way_en", true},
	{-1, -1, 296, "OOB_way_en", true},
	{-1, -1, 297, "OOB_way_en", true},

	/* 270 */
	{-1, -1, 298, "OOB_way_en", true},
	{-1, -1, 299, "OOB_way_en", true},
	{-1, -1, 300, "OOB_way_en", true},
	{-1, -1, 301, "OOB_way_en", true},
	{-1, -1, 302, "OOB_way_en", true},
	{-1, -1, 303, "OOB_way_en", true},
	{-1, -1, 304, "OOB_way_en", true},
	{-1, -1, 305, "OOB_way_en", true},
	{-1, -1, 306, "OOB_way_en", true},
	{-1, -1, 307, "OOB_way_en", true},

	/* 280 */
	{-1, -1, 308, "OOB_way_en", true},
	{-1, -1, 309, "OOB_way_en", true},
	{-1, -1, 310, "OOB_way_en", true},
	{-1, -1, 311, "OOB_way_en", true},
	{-1, -1, 312, "OOB_way_en", true},
	{-1, -1, 313, "OOB_way_en", true},
	{-1, -1, 314, "OOB_way_en", true},
	{-1, -1, 315, "OOB_way_en", true},
	{-1, -1, 316, "OOB_way_en", true},
	{-1, -1, 317, "OOB_way_en", true},

	/* 290 */
	{-1, -1, 318, "OOB_way_en", true},
	{-1, -1, 319, "OOB_way_en", true},
	{-1, -1, 320, "OOB_way_en", true},
	{-1, -1, 321, "OOB_way_en", true},
	{-1, -1, 322, "OOB_way_en", true},
	{-1, -1, 323, "OOB_way_en", true},
	{-1, -1, 324, "OOB_way_en", true},
	{-1, -1, 325, "OOB_way_en", true},
	{-1, -1, 326, "OOB_way_en", true},
	{-1, -1, 327, "OOB_way_en", true},

	/* 300 */
	{-1, -1, 328, "OOB_way_en", true},
	{-1, -1, 329, "OOB_way_en", true},
	{-1, -1, 330, "OOB_way_en", true},
	{-1, -1, 331, "OOB_way_en", true},
	{-1, -1, 332, "OOB_way_en", true},
	{-1, -1, 333, "OOB_way_en", true},
	{-1, -1, 334, "OOB_way_en", true},
	{-1, -1, 335, "OOB_way_en", true},
	{-1, -1, 336, "OOB_way_en", true},
	{-1, -1, 337, "OOB_way_en", true},

	/* 310 */
	{-1, -1, 338, "OOB_way_en", true},
	{-1, -1, 339, "OOB_way_en", true},
	{-1, -1, 340, "OOB_way_en", true},
	{-1, -1, 341, "OOB_way_en", true},
	{-1, -1, 342, "OOB_way_en", true},
	{-1, -1, 343, "OOB_way_en", true},
	{-1, -1, 344, "OOB_way_en", true},
	{-1, -1, 345, "OOB_way_en", true},
	{-1, -1, 346, "OOB_way_en", true},
	{-1, -1, 347, "OOB_way_en", true},

	/* 320 */
	{-1, -1, 348, "Decode_error", true},
	{-1, -1, 349, "Decode_error", true},
	{-1, -1, 350, "Decode_error", true},
	{-1, -1, 351, "Decode_error", false},
	{-1, -1, 352, "Decode_error", true},
	{-1, -1, 353, "Decode_error", true},

	{-1, -1, 354, "SRAMROM", true},
	{-1, -1, 355, "DEVAPC_APINFRA_IO_AO", false},
	{-1, -1, 356, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_io_ctrl[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "IFRBUS_PDN_APB_S", true},
	{0, 1, 1, "BCRM_APINFRA_IO_CTRL_PDN_APB_S", true},
	{0, 2, 2, "DEVAPC_APINFRA_IO_CTRL_PDN_APB_S", true},
	{0, 3, 3, "APINFRA_IO_PDN_CLK_MON_S", true},
	{0, 4, 4, "FMCU_FAKE_ENG_APB_S", true},
	{0, 5, 5, "IFRBUS_AO_APB_S", true},
	{0, 6, 6, "BCRM_APINFRA_IO_CTRL_AO_APB_S", true},
	{0, 7, 7, "DEVAPC_APINFRA_IO_CTRL_AO_APB_S", true},
	{0, 8, 8, "DEBUG_CTRL_APINFRA_IO_CTRL_AO_APB_S", true},
	{0, 9, 9, "APINFRA_IO_AO_CLK_MON_S", true},

	/* 10 */
	{-1, -1, 10, "OOB_way_en", true},
	{-1, -1, 11, "OOB_way_en", true},
	{-1, -1, 12, "OOB_way_en", true},
	{-1, -1, 13, "OOB_way_en", true},
	{-1, -1, 14, "OOB_way_en", true},
	{-1, -1, 15, "OOB_way_en", true},
	{-1, -1, 16, "OOB_way_en", true},
	{-1, -1, 17, "OOB_way_en", true},
	{-1, -1, 18, "OOB_way_en", true},
	{-1, -1, 19, "OOB_way_en", true},

	/* 20 */
	{-1, -1, 20, "OOB_way_en", true},
	{-1, -1, 21, "OOB_way_en", true},

	{-1, -1, 22, "Decode_error", true},
	{-1, -1, 23, "Decode_error", true},

	{-1, -1, 24, "DEVAPC_APINFRA_IO_CTRL_AO", false},
	{-1, -1, 25, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_io_intf[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BCRM_APINFRA_IO_INTF_PDN_APB_S", true},
	{0, 1, 1, "DEVAPC_APINFRA_IO_INTF_PDN_APB_S", true},
	{0, 2, 2, "IO_NOC_CTRL_APB_S", true},
	{0, 3, 3, "BCRM_APINFRA_IO_NOC_WRAP_APB_S", true},
	{0, 4, 4, "BCRM_APINFRA_IO_INTF_AO_APB_S", true},
	{0, 5, 5, "DEVAPC_APINFRA_IO_INTF_AO_APB_S", true},
	{0, 6, 6, "DEBUG_CTRL_APINFRA_IO_INTF_AO_APB_S", true},
	{0, 7, 7, "BCRM_APINFRA_IO_NOC_APB_S", true},

	{-1, -1, 8, "OOB_way_en", true},
	{-1, -1, 9, "OOB_way_en", true},

	/* 10 */
	{-1, -1, 10, "OOB_way_en", true},
	{-1, -1, 11, "OOB_way_en", true},
	{-1, -1, 12, "OOB_way_en", true},
	{-1, -1, 13, "OOB_way_en", true},
	{-1, -1, 14, "OOB_way_en", true},
	{-1, -1, 15, "OOB_way_en", true},
	{-1, -1, 16, "OOB_way_en", true},
	{-1, -1, 17, "OOB_way_en", true},

	{-1, -1, 18, "Decode_error", true},
	{-1, -1, 19, "Decode_error", true},

	/* 20 */
	{-1, -1, 24, "DEVAPC_APINFRA_IO_INTF_AO", false},
	{-1, -1, 25, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_big4[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BND_EAST_APB0_S", true},
	{0, 1, 1, "BND_EAST_APB1_S", true},
	{0, 2, 2, "BND_EAST_APB2_S", true},
	{0, 3, 3, "BND_EAST_APB3_S", true},
	{0, 4, 4, "BND_EAST_APB4_S", true},
	{0, 5, 5, "BND_EAST_APB5_S", true},
	{0, 6, 6, "BND_EAST_APB6_S", true},
	{0, 7, 7, "BND_EAST_APB7_S", true},
	{0, 8, 8, "BND_EAST_APB8_S", true},
	{0, 9, 9, "BND_EAST_APB9_S", true},

	/* 10 */
	{0, 10, 10, "BND_EAST_APB10_S", true},
	{0, 11, 11, "BND_EAST_APB11_S", true},
	{0, 12, 12, "BND_EAST_APB12_S", true},
	{0, 13, 13, "BND_EAST_APB13_S", true},
	{0, 14, 14, "BND_EAST_APB14_S", true},
	{0, 15, 15, "BND_EAST_APB15_S", true},
	{0, 16, 16, "BND_EAST_APB16_S", true},
	{0, 17, 17, "BND_EAST_APB17_S", true},
	{0, 18, 18, "BND_EAST_APB18_S", true},
	{0, 19, 19, "BND_EAST_APB19_S", true},

	/* 20 */
	{0, 20, 20, "BND_WEST_APB0_S", true},
	{0, 21, 21, "BND_WEST_APB1_S", true},
	{0, 22, 22, "BND_WEST_APB2_S", true},
	{0, 23, 23, "BND_WEST_APB3_S", true},
	{0, 24, 24, "BND_WEST_APB4_S", true},
	{0, 25, 25, "BND_WEST_APB5_S", true},
	{0, 26, 26, "BND_WEST_APB6_S", true},
	{0, 27, 27, "BND_WEST_APB7_S", true},
	{0, 28, 28, "BND_WEST_APB8_S", true},
	{0, 29, 29, "BND_WEST_APB9_S", true},

	/* 30 */
	{0, 30, 30, "BND_WEST_APB10_S", true},
	{0, 31, 31, "BND_WEST_APB11_S", true},
	{0, 32, 32, "BND_WEST_APB12_S", true},
	{0, 33, 33, "BND_WEST_APB13_S", true},
	{0, 34, 34, "BND_WEST_APB14_S", true},
	{0, 35, 35, "BND_WEST_APB15_S", true},
	{0, 36, 36, "BND_WEST_APB16_S", true},
	{0, 37, 37, "BND_WEST_APB17_S", true},
	{0, 38, 38, "BND_WEST_APB18_S", true},
	{0, 39, 39, "BND_WEST_APB19_S", true},

	/* 40 */
	{0, 40, 40, "BND_WEST_APB20_S", true},
	{0, 41, 41, "BND_WEST_APB21_S", true},
	{0, 42, 42, "BND_WEST_APB22_S", true},
	{0, 43, 43, "BND_WEST_APB23_S", true},
	{0, 44, 44, "BND_NORTH_APB0_S", true},
	{0, 45, 45, "BND_NORTH_APB1_S", true},
	{0, 46, 46, "BND_NORTH_APB2_S", true},
	{0, 47, 47, "BND_NORTH_APB3_S", true},
	{0, 48, 48, "BND_NORTH_APB4_S", true},
	{0, 49, 49, "BND_NORTH_APB5_S", true},

	/* 50 */
	{0, 50, 50, "BND_NORTH_APB6_S", true},
	{0, 51, 51, "BND_NORTH_APB7_S", true},
	{0, 52, 52, "BND_NORTH_APB8_S", true},
	{0, 53, 53, "BND_NORTH_APB9_S", true},
	{0, 54, 54, "BND_NORTH_APB10_S", true},
	{0, 55, 55, "BND_NORTH_APB11_S", true},
	{0, 56, 56, "BND_NORTH_APB12_S", true},
	{0, 57, 57, "BND_NORTH_APB13_S", true},
	{0, 58, 58, "BND_NORTH_APB14_S", true},
	{0, 59, 59, "BND_NORTH_APB15_S", true},

	/* 60 */
	{0, 60, 60, "BND_NORTH_APB16_S", true},
	{0, 61, 61, "BND_NORTH_APB17_S", true},
	{0, 62, 62, "BND_NORTH_APB18_S", true},
	{0, 63, 63, "BND_NORTH_APB19_S", true},
	{0, 64, 64, "BND_SOUTH_APB0_S", true},
	{0, 65, 65, "BND_SOUTH_APB1_S", true},
	{0, 66, 66, "BND_SOUTH_APB2_S", true},
	{0, 67, 67, "BND_SOUTH_APB3_S", true},
	{0, 68, 68, "BND_SOUTH_APB4_S", true},
	{0, 69, 69, "BND_SOUTH_APB5_S", true},

	/* 70 */
	{0, 70, 70, "BND_SOUTH_APB6_S", true},
	{0, 71, 71, "BND_SOUTH_APB7_S", true},
	{0, 72, 72, "BND_SOUTH_APB8_S", true},
	{0, 73, 73, "BND_SOUTH_APB9_S", true},
	{0, 74, 74, "BND_SOUTH_APB10_S", true},
	{0, 75, 75, "BND_SOUTH_APB11_S", true},
	{0, 76, 76, "BND_SOUTH_APB12_S", true},
	{0, 77, 77, "BND_SOUTH_APB13_S", true},
	{0, 78, 78, "BND_SOUTH_APB14_S", true},
	{0, 79, 79, "BND_SOUTH_APB15_S", true},

	/* 80 */
	{0, 80, 80, "BND_SOUTH_APB16_S", true},
	{0, 81, 81, "BND_SOUTH_APB17_S", true},
	{0, 82, 82, "BND_SOUTH_APB18_S", true},
	{0, 83, 83, "BND_SOUTH_APB19_S", true},
	{0, 84, 84, "BCRM_APINFRA_BIG4_PDN_APB_S", true},
	{0, 85, 85, "DEVAPC_APINFRA_BIG4_PDN_APB_S", true},
	{0, 86, 86, "BCRM_APINFRA_BIG4_AO_APB_S", true},
	{0, 87, 87, "DEVAPC_APINFRA_BIG4_AO_APB_S", true},
	{0, 88, 88, "DEBUG_CTRL_APINFRA_BIG4_AO_APB_S", true},

	{-1, -1, 89, "OOB_way_en", true},

	/* 90 */
	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "OOB_way_en", true},
	{-1, -1, 95, "OOB_way_en", true},
	{-1, -1, 96, "OOB_way_en", true},
	{-1, -1, 97, "OOB_way_en", true},
	{-1, -1, 98, "OOB_way_en", true},
	{-1, -1, 99, "OOB_way_en", true},

	/* 100 */
	{-1, -1, 100, "OOB_way_en", true},
	{-1, -1, 101, "OOB_way_en", true},
	{-1, -1, 102, "OOB_way_en", true},
	{-1, -1, 103, "OOB_way_en", true},
	{-1, -1, 104, "OOB_way_en", true},
	{-1, -1, 105, "OOB_way_en", true},
	{-1, -1, 106, "OOB_way_en", true},
	{-1, -1, 107, "OOB_way_en", true},
	{-1, -1, 108, "OOB_way_en", true},
	{-1, -1, 109, "OOB_way_en", true},

	/* 110 */
	{-1, -1, 110, "OOB_way_en", true},
	{-1, -1, 111, "OOB_way_en", true},
	{-1, -1, 112, "OOB_way_en", true},
	{-1, -1, 113, "OOB_way_en", true},
	{-1, -1, 114, "OOB_way_en", true},
	{-1, -1, 115, "OOB_way_en", true},
	{-1, -1, 116, "OOB_way_en", true},
	{-1, -1, 117, "OOB_way_en", true},
	{-1, -1, 118, "OOB_way_en", true},
	{-1, -1, 119, "OOB_way_en", true},

	/* 120 */
	{-1, -1, 120, "OOB_way_en", true},
	{-1, -1, 121, "OOB_way_en", true},
	{-1, -1, 122, "OOB_way_en", true},
	{-1, -1, 123, "OOB_way_en", true},
	{-1, -1, 124, "OOB_way_en", true},
	{-1, -1, 125, "OOB_way_en", true},
	{-1, -1, 126, "OOB_way_en", true},
	{-1, -1, 127, "OOB_way_en", true},
	{-1, -1, 128, "OOB_way_en", true},
	{-1, -1, 129, "OOB_way_en", true},

	/* 130 */
	{-1, -1, 130, "OOB_way_en", true},
	{-1, -1, 131, "OOB_way_en", true},
	{-1, -1, 132, "OOB_way_en", true},
	{-1, -1, 133, "OOB_way_en", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},

	/* 140 */
	{-1, -1, 140, "OOB_way_en", true},
	{-1, -1, 141, "OOB_way_en", true},
	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},

	/* 150 */
	{-1, -1, 150, "OOB_way_en", true},
	{-1, -1, 151, "OOB_way_en", true},
	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},

	/* 160 */
	{-1, -1, 160, "OOB_way_en", true},
	{-1, -1, 161, "OOB_way_en", true},
	{-1, -1, 162, "OOB_way_en", true},
	{-1, -1, 163, "OOB_way_en", true},
	{-1, -1, 164, "OOB_way_en", true},
	{-1, -1, 165, "OOB_way_en", true},
	{-1, -1, 166, "OOB_way_en", true},
	{-1, -1, 167, "OOB_way_en", true},
	{-1, -1, 168, "OOB_way_en", true},
	{-1, -1, 169, "OOB_way_en", true},

	/* 170 */
	{-1, -1, 170, "OOB_way_en", true},
	{-1, -1, 171, "OOB_way_en", true},
	{-1, -1, 172, "OOB_way_en", true},
	{-1, -1, 173, "OOB_way_en", true},
	{-1, -1, 174, "OOB_way_en", true},
	{-1, -1, 175, "OOB_way_en", true},
	{-1, -1, 176, "OOB_way_en", true},
	{-1, -1, 177, "OOB_way_en", true},
	{-1, -1, 178, "OOB_way_en", true},
	{-1, -1, 179, "OOB_way_en", true},

	/* 180 */
	{-1, -1, 180, "OOB_way_en", true},
	{-1, -1, 181, "OOB_way_en", true},
	{-1, -1, 182, "OOB_way_en", true},
	{-1, -1, 183, "OOB_way_en", true},

	{-1, -1, 184, "Decode_error", true},
	{-1, -1, 185, "Decode_error", true},
	{-1, -1, 186, "Decode_error", true},
	{-1, -1, 187, "Decode_error", true},
	{-1, -1, 188, "Decode_error", false},
	{-1, -1, 189, "Decode_error", true},

	/* 190 */
	{-1, -1, 190, "BND_EAST_APB_19", false},
	{-1, -1, 191, "BND_SOUTH_APB_19", false},
	{-1, -1, 192, "BND_WEST_APB_19", false},
	{-1, -1, 193, "BND_NORTH_APB_19", false},
	{-1, -1, 194, "DEVAPC_APINFRA_BIG4_AO", false},
	{-1, -1, 195, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_dramc[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DRAMC_MD32_S0_APB_S", true},
	{0, 1, 1, "DRAMC_MD32_S0_APB_S-1", true},
	{0, 2, 2, "DRAMC_MD32_S1_APB_S", true},
	{0, 3, 3, "DRAMC_MD32_S1_APB_S-1", true},
	{0, 4, 4, "DRAMC_MD32_S2_APB_S", true},
	{0, 5, 5, "DRAMC_MD32_S2_APB_S-1", true},
	{0, 6, 6, "DRAMC_MD32_S3_APB_S", true},
	{0, 7, 7, "DRAMC_MD32_S3_APB_S-1", true},
	{0, 8, 9, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 9, 10, "DRAMC_CH0_TOP1_APB_S", true},

	/* 10 */
	{0, 10, 11, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 11, 12, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 12, 13, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 13, 14, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 14, 15, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 15, 16, "DRAMC_CH0_TOP0_APB_SEC_S", true},
	{0, 16, 17, "DRAMC_CH0_TOP1_APB_SEC_S", true},
	{0, 17, 18, "DRAMC_CH0_TOP2_APB_SEC_S", true},
	{0, 18, 19, "DRAMC_CH0_TOP3_APB_SEC_S", true},
	{0, 19, 21, "DRAMC_CH1_TOP0_APB_S", true},

	/* 20 */
	{0, 20, 22, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 21, 23, "DRAMC_CH1_TOP2_APB_S", true},
	{0, 22, 24, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 23, 25, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 24, 26, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 25, 27, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 26, 28, "DRAMC_CH1_TOP0_APB_SEC_S", true},
	{0, 27, 29, "DRAMC_CH1_TOP1_APB_SEC_S", true},
	{0, 28, 30, "DRAMC_CH1_TOP2_APB_SEC_S", true},
	{0, 29, 31, "DRAMC_CH1_TOP3_APB_SEC_S", true},

	/* 30 */
	{0, 30, 33, "DRAMC_CH2_TOP0_APB_S", true},
	{0, 31, 34, "DRAMC_CH2_TOP1_APB_S", true},
	{0, 32, 35, "DRAMC_CH2_TOP2_APB_S", true},
	{0, 33, 36, "DRAMC_CH2_TOP3_APB_S", true},
	{0, 34, 37, "DRAMC_CH2_TOP4_APB_S", true},
	{0, 35, 38, "DRAMC_CH2_TOP5_APB_S", true},
	{0, 36, 39, "DRAMC_CH2_TOP6_APB_S", true},
	{0, 37, 40, "DRAMC_CH2_TOP0_APB_SEC_S", true},
	{0, 38, 41, "DRAMC_CH2_TOP1_APB_SEC_S", true},
	{0, 39, 42, "DRAMC_CH2_TOP2_APB_SEC_S", true},

	/* 40 */
	{0, 40, 43, "DRAMC_CH2_TOP3_APB_SEC_S", true},
	{0, 41, 45, "DRAMC_CH3_TOP0_APB_S", true},
	{0, 42, 46, "DRAMC_CH3_TOP1_APB_S", true},
	{0, 43, 47, "DRAMC_CH3_TOP2_APB_S", true},
	{0, 44, 48, "DRAMC_CH3_TOP3_APB_S", true},
	{0, 45, 49, "DRAMC_CH3_TOP4_APB_S", true},
	{0, 46, 50, "DRAMC_CH3_TOP5_APB_S", true},
	{0, 47, 51, "DRAMC_CH3_TOP6_APB_S", true},
	{0, 48, 52, "DRAMC_CH3_TOP0_APB_SEC_S", true},
	{0, 49, 53, "DRAMC_CH3_TOP1_APB_SEC_S", true},

	/* 50 */
	{0, 50, 54, "DRAMC_CH3_TOP2_APB_SEC_S", true},
	{0, 51, 55, "DRAMC_CH3_TOP3_APB_SEC_S", true},
	{0, 52, 56, "BCRM_APINFRA_DRAMC_PDN_APB_S", true},
	{0, 53, 57, "DEVAPC_APINFRA_DRAMC_PDN_APB_S", true},
	{0, 54, 58, "BCRM_APINFRA_DRAMC_AO_APB_S", true},
	{0, 55, 59, "DEVAPC_APINFRA_DRAMC_AO_APB_S", true},
	{0, 56, 60, "DEBUG_CTRL_APINFRA_DRAMC_AO_APB_S", true},

	{-1, -1, 61, "OOB_way_en", true},
	{-1, -1, 62, "OOB_way_en", true},
	{-1, -1, 63, "OOB_way_en", true},

	/* 60 */
	{-1, -1, 64, "OOB_way_en", true},
	{-1, -1, 65, "OOB_way_en", true},
	{-1, -1, 66, "OOB_way_en", true},
	{-1, -1, 67, "OOB_way_en", true},
	{-1, -1, 68, "OOB_way_en", true},
	{-1, -1, 69, "OOB_way_en", true},
	{-1, -1, 70, "OOB_way_en", true},
	{-1, -1, 71, "OOB_way_en", true},
	{-1, -1, 72, "OOB_way_en", true},
	{-1, -1, 73, "OOB_way_en", true},

	/* 70 */
	{-1, -1, 74, "OOB_way_en", true},
	{-1, -1, 75, "OOB_way_en", true},
	{-1, -1, 76, "OOB_way_en", true},
	{-1, -1, 77, "OOB_way_en", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},
	{-1, -1, 80, "OOB_way_en", true},
	{-1, -1, 81, "OOB_way_en", true},
	{-1, -1, 82, "OOB_way_en", true},
	{-1, -1, 83, "OOB_way_en", true},

	/* 80 */
	{-1, -1, 84, "OOB_way_en", true},
	{-1, -1, 85, "OOB_way_en", true},
	{-1, -1, 86, "OOB_way_en", true},
	{-1, -1, 87, "OOB_way_en", true},
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},
	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},

	/* 90 */
	{-1, -1, 94, "OOB_way_en", true},
	{-1, -1, 95, "OOB_way_en", true},
	{-1, -1, 96, "OOB_way_en", true},
	{-1, -1, 97, "OOB_way_en", true},
	{-1, -1, 98, "OOB_way_en", true},
	{-1, -1, 99, "OOB_way_en", true},
	{-1, -1, 100, "OOB_way_en", true},
	{-1, -1, 101, "OOB_way_en", true},
	{-1, -1, 102, "OOB_way_en", true},
	{-1, -1, 103, "OOB_way_en", true},

	/* 100 */
	{-1, -1, 104, "OOB_way_en", true},
	{-1, -1, 105, "OOB_way_en", true},
	{-1, -1, 106, "OOB_way_en", true},
	{-1, -1, 107, "OOB_way_en", true},
	{-1, -1, 108, "OOB_way_en", true},
	{-1, -1, 109, "OOB_way_en", true},
	{-1, -1, 110, "OOB_way_en", true},
	{-1, -1, 111, "OOB_way_en", true},
	{-1, -1, 112, "OOB_way_en", true},
	{-1, -1, 113, "OOB_way_en", true},

	/* 110 */
	{-1, -1, 114, "OOB_way_en", true},
	{-1, -1, 115, "OOB_way_en", true},
	{-1, -1, 116, "OOB_way_en", true},
	{-1, -1, 117, "OOB_way_en", true},
	{-1, -1, 118, "OOB_way_en", true},
	{-1, -1, 119, "OOB_way_en", true},

	{-1, -1, 120, "Decode_error", true},
	{-1, -1, 121, "Decode_error", true},
	{-1, -1, 122, "Decode_error", true},
	{-1, -1, 123, "Decode_error", false},

	/* 120 */
	{-1, -1, 124, "Decode_error", true},
	{-1, -1, 125, "Decode_error", true},

	{-1, -1, 126, "DEVAPC_APINFRA_DRAMC_AO", false},
	{-1, -1, 127, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_emi[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "NEMI_PDN_SEC_SNOC", true},
	{0, 1, 1, "NEMI_PDN_SEC_WLA_MEMSYS", true},
	{0, 2, 2, "NEMI_PDN_SEC_FAKE_ENG1", true},
	{0, 3, 3, "NEMI_PDN_SEC_FAKE_ENG0", true},
	{0, 4, 4, "NEMI_PDN_SEC_VDNR_BCRM", true},
	{0, 5, 5, "NEMI_PDN_SEC_EMICFG", true},
	{0, 6, 6, "NEMI_PDN_SEC_EMI_CXATBFUNNEL", true},
	{0, 7, 7, "NEMI_PDN_SEC_EMI_BUS_TRACE_CON", true},
	{0, 8, 8, "NEMI_PDN_SEC_EMI_MPU_REG_M1", true},
	{0, 9, 9, "NEMI_PDN_SEC_EMI_REG_M1", true},

	/* 10 */
	{0, 10, 10, "NEMI_PDN_SEC_VDNR_SSC_2", true},
	{0, 11, 11, "NEMI_PDN_SEC_DVM_MI", true},
	{0, 12, 12, "NEMI_PDN_SEC_VDNR_SSC_01", true},
	{0, 13, 13, "NEMI_PDN_SEC_SMPU_REG", true},
	{0, 14, 14, "NEMI_PDN_SEC_SLC_REG_M1", true},
	{0, 15, 15, "NEMI_PDN_SEC_MCU_EMI_SMPU", true},
	{0, 16, 16, "NEMI_PDN_SEC_MCU_EMI_OBF", true},
	{0, 17, 17, "NEMI_PDN_SEC_MCU_EMI_OBF2", true},
	{0, 18, 18, "NEMI_PDN_SEC_SLC_REG", true},
	{0, 19, 19, "NEMI_PDN_SEC_EMI_MPU_REG", true},

	/* 20 */
	{0, 20, 20, "NEMI_PDN_SEC_EMI_REG", true},
	{0, 21, 21, "NEMI_PDN_SEC_VDNR_RSI", true},
	{0, 22, 22, "NEMI_PDN_NON_SEC_SNOC", true},
	{0, 23, 23, "NEMI_PDN_NON_SEC_WLA_MEMSYS", true},
	{0, 24, 24, "NEMI_PDN_NON_SEC_FAKE_ENG1", true},
	{0, 25, 25, "NEMI_PDN_NON_SEC_FAKE_ENG0", true},
	{0, 26, 26, "NEMI_PDN_NON_SEC_VDNR_BCRM", true},
	{0, 27, 27, "NEMI_PDN_NON_SEC_EMICFG", true},
	{0, 28, 28, "NEMI_PDN_NON_SEC_EMI_CXATBFUNNEL", true},
	{0, 29, 29, "NEMI_PDN_NON_SEC_EMI_BUS_TRACE_CON", true},

	/* 30 */
	{0, 30, 30, "NEMI_PDN_NON_SEC_EMI_MPU_REG_M1", true},
	{0, 31, 31, "NEMI_PDN_NON_SEC_EMI_REG_M1", true},
	{0, 32, 32, "NEMI_PDN_NON_SEC_VDNR_SSC_2", true},
	{0, 33, 33, "NEMI_PDN_NON_SEC_DVM_MI", true},
	{0, 34, 34, "NEMI_PDN_NON_SEC_VDNR_SSC_01", true},
	{0, 35, 35, "NEMI_PDN_NON_SEC_SMPU_REG", true},
	{0, 36, 36, "NEMI_PDN_NON_SEC_SLC_REG_M1", true},
	{0, 37, 37, "NEMI_PDN_NON_SEC_MCU_EMI_SMPU", true},
	{0, 38, 38, "NEMI_PDN_NON_SEC_MCU_EMI_OBF", true},
	{0, 39, 39, "NEMI_PDN_NON_SEC_MCU_EMI_OBF2", true},

	/* 40 */
	{0, 40, 40, "NEMI_PDN_NON_SEC_SLC_REG", true},
	{0, 41, 41, "NEMI_PDN_NON_SEC_EMI_MPU_REG", true},
	{0, 42, 42, "NEMI_PDN_NON_SEC_EMI_REG", true},
	{0, 43, 43, "NEMI_PDN_NON_SEC_VDNR_RSI", true},
	{0, 44, 44, "SEMI_PDN_SEC_SNOC", true},
	{0, 45, 45, "SEMI_PDN_SEC_WLA_MEMSYS", true},
	{0, 46, 46, "SEMI_PDN_SEC_FAKE_ENG1", true},
	{0, 47, 47, "SEMI_PDN_SEC_FAKE_ENG0", true},
	{0, 48, 48, "SEMI_PDN_SEC_VDNR_BCRM", true},
	{0, 49, 49, "SEMI_PDN_SEC_EMICFG", true},

	/* 50 */
	{0, 50, 50, "SEMI_PDN_SEC_EMI_CXATBFUNNEL", true},
	{0, 51, 51, "SEMI_PDN_SEC_EMI_BUS_TRACE_CON", true},
	{0, 52, 52, "SEMI_PDN_SEC_EMI_MPU_REG_M1", true},
	{0, 53, 53, "SEMI_PDN_SEC_EMI_REG_M1", true},
	{0, 54, 54, "SEMI_PDN_SEC_VDNR_SSC_2", true},
	{0, 55, 55, "SEMI_PDN_SEC_DVM_MI", true},
	{0, 56, 56, "SEMI_PDN_SEC_VDNR_SSC_01", true},
	{0, 57, 57, "SEMI_PDN_SEC_SMPU_REG", true},
	{0, 58, 58, "SEMI_PDN_SEC_SLC_REG_M1", true},
	{0, 59, 59, "SEMI_PDN_SEC_MCU_EMI_SMPU", true},

	/* 60 */
	{0, 60, 60, "SEMI_PDN_SEC_MCU_EMI_OBF", true},
	{0, 61, 61, "SEMI_PDN_SEC_MCU_EMI_OBF2", true},
	{0, 62, 62, "SEMI_PDN_SEC_SLC_REG", true},
	{0, 63, 63, "SEMI_PDN_SEC_EMI_MPU_REG", true},
	{0, 64, 64, "SEMI_PDN_SEC_EMI_REG", true},
	{0, 65, 65, "SEMI_PDN_SEC_VDNR_RSI", true},
	{0, 66, 66, "SEMI_PDN_NON_SEC_SNOC", true},
	{0, 67, 67, "SEMI_PDN_NON_SEC_WLA_MEMSYS", true},
	{0, 68, 68, "SEMI_PDN_NON_SEC_FAKE_ENG1", true},
	{0, 69, 69, "SEMI_PDN_NON_SEC_FAKE_ENG0", true},

	/* 70 */
	{0, 70, 70, "SEMI_PDN_NON_SEC_VDNR_BCRM", true},
	{0, 71, 71, "SEMI_PDN_NON_SEC_EMICFG", true},
	{0, 72, 72, "SEMI_PDN_NON_SEC_EMI_CXATBFUNNEL", true},
	{0, 73, 73, "SEMI_PDN_NON_SEC_EMI_BUS_TRACE_CON", true},
	{0, 74, 74, "SEMI_PDN_NON_SEC_EMI_MPU_REG_M1", true},
	{0, 75, 75, "SEMI_PDN_NON_SEC_EMI_REG_M1", true},
	{0, 76, 76, "SEMI_PDN_NON_SEC_VDNR_SSC_2", true},
	{0, 77, 77, "SEMI_PDN_NON_SEC_DVM_MI", true},
	{0, 78, 78, "SEMI_PDN_NON_SEC_VDNR_SSC_01", true},
	{0, 79, 79, "SEMI_PDN_NON_SEC_SMPU_REG", true},

	/* 80 */
	{0, 80, 80, "SEMI_PDN_NON_SEC_SLC_REG_M1", true},
	{0, 81, 81, "SEMI_PDN_NON_SEC_MCU_EMI_SMPU", true},
	{0, 82, 82, "SEMI_PDN_NON_SEC_MCU_EMI_OBF", true},
	{0, 83, 83, "SEMI_PDN_NON_SEC_MCU_EMI_OBF2", true},
	{0, 84, 84, "SEMI_PDN_NON_SEC_SLC_REG", true},
	{0, 85, 85, "SEMI_PDN_NON_SEC_EMI_MPU_REG", true},
	{0, 86, 86, "SEMI_PDN_NON_SEC_EMI_REG", true},
	{0, 87, 87, "SEMI_PDN_NON_SEC_VDNR_RSI", true},
	{0, 88, 88, "BCRM_APINFRA_EMI_PDN_APB_S", true},
	{0, 89, 89, "DEVAPC_APINFRA_EMI_PDN_APB_S", true},

	/* 90 */
	{0, 90, 90, "EMI_IFR_APB0_S", true},
	{0, 91, 91, "EMI_IFR_APB1_S", true},
	{0, 92, 92, "EMI_IFR_APB2_S", true},
	{0, 93, 93, "EMI_IFR_APB3_S", true},
	{0, 94, 94, "EMI_IFR_APB4_S", true},
	{0, 95, 95, "EMI_IFR_APB5_S", true},
	{0, 96, 96, "EMI_IFR_APB6_S", true},
	{0, 97, 97, "EMI_IFR_APB7_S", true},
	{0, 98, 98, "EMI_IFR_APB8_S", true},
	{0, 99, 99, "EMI_IFR_APB9_S", true},

	/* 100 */
	{0, 100, 100, "EMI_IFR_APB10_S", true},
	{0, 101, 101, "EMI_IFR_APB11_S", true},
	{0, 102, 102, "EMI_IFR_APB12_S", true},
	{0, 103, 103, "EMI_IFR_APB13_S", true},
	{0, 104, 104, "EMI_IFR_SEC_APB0_S", true},
	{0, 105, 105, "EMI_IFR_SEC_APB1_S", true},
	{0, 106, 106, "EMI_IFR_SEC_APB2_S", true},
	{0, 107, 107, "EMI_IFR_SEC_APB3_S", true},
	{0, 108, 108, "EMI_IFR_SEC_APB4_S", true},
	{0, 109, 109, "EMI_IFR_SEC_APB5_S", true},

	/* 110 */
	{0, 110, 110, "EMI_IFR_SEC_APB6_S", true},
	{0, 111, 111, "EMI_IFR_SEC_APB7_S", true},
	{0, 112, 112, "EMI_IFR_SEC_APB8_S", true},
	{0, 113, 113, "EMI_IFR_SEC_APB9_S", true},
	{0, 114, 114, "EMI_IFR_SEC_APB10_S", true},
	{0, 115, 115, "EMI_IFR_SEC_APB11_S", true},
	{0, 116, 116, "EMI_IFR_SEC_APB12_S", true},
	{0, 117, 117, "EMI_IFR_SEC_APB13_S", true},
	{0, 118, 118, "NEMI_AO_SEC_SLC_REG_2ND", true},
	{0, 119, 119, "NEMI_AO_SEC_ACP_PCTRL", true},

	/* 120 */
	{0, 120, 120, "NEMI_AO_SEC_EMI_REG_2ND", true},
	{0, 121, 121, "NEMI_AO_SEC_MBIST", true},
	{0, 122, 122, "NEMI_AO_SEC_EMICFG_AO", true},
	{0, 123, 123, "NEMI_AO_SEC_VDNR_BCRM_AO", true},
	{0, 124, 124, "NEMI_AO_SEC_VDNR_DBG_AO", true},
	{0, 125, 125, "NEMI_AO_NON_SEC_SLC_REG_2ND", true},
	{0, 126, 126, "NEMI_AO_NON_SEC_ACP_PCTRL", true},
	{0, 127, 127, "NEMI_AO_NON_SEC_EMI_REG_2ND", true},
	{0, 128, 128, "NEMI_AO_NON_SEC_MBIST", true},
	{0, 129, 129, "NEMI_AO_NON_SEC_EMICFG_AO", true},

	/* 130 */
	{0, 130, 130, "NEMI_AO_NON_SEC_VDNR_BCRM_AO", true},
	{0, 131, 131, "NEMI_AO_NON_SEC_VDNR_DBG_AO", true},
	{0, 132, 132, "SEMI_AO_SEC_SLC_REG_2ND", true},
	{0, 133, 133, "SEMI_AO_SEC_ACP_PCTRL", true},
	{0, 134, 134, "SEMI_AO_SEC_EMI_REG_2ND", true},
	{0, 135, 135, "SEMI_AO_SEC_MBIST", true},
	{0, 136, 136, "SEMI_AO_SEC_EMICFG_AO", true},
	{0, 137, 137, "SEMI_AO_SEC_VDNR_BCRM_AO", true},
	{0, 138, 138, "SEMI_AO_SEC_VDNR_DBG_AO", true},
	{0, 139, 139, "SEMI_AO_NON_SEC_SLC_REG_2ND", true},

	/* 140 */
	{0, 140, 140, "SEMI_AO_NON_SEC_ACP_PCTRL", true},
	{0, 141, 141, "SEMI_AO_NON_SEC_EMI_REG_2ND", true},
	{0, 142, 142, "SEMI_AO_NON_SEC_MBIST", true},
	{0, 143, 143, "SEMI_AO_NON_SEC_EMICFG_AO", true},
	{0, 144, 144, "SEMI_AO_NON_SEC_VDNR_BCRM_AO", true},
	{0, 145, 145, "SEMI_AO_NON_SEC_VDNR_DBG_AO", true},
	{0, 146, 146, "BCRM_APINFRA_EMI_AO_APB_S", true},
	{0, 147, 147, "DEVAPC_APINFRA_EMI_AO_APB_S", true},
	{0, 148, 148, "DEBUG_CTRL_APINFRA_EMI_AO_APB_S", true},

	{-1, -1, 149, "OOB_way_en", true},

	/* 150 */
	{-1, -1, 150, "OOB_way_en", true},
	{-1, -1, 151, "OOB_way_en", true},
	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},

	/* 160 */
	{-1, -1, 160, "OOB_way_en", true},
	{-1, -1, 161, "OOB_way_en", true},
	{-1, -1, 162, "OOB_way_en", true},
	{-1, -1, 163, "OOB_way_en", true},
	{-1, -1, 164, "OOB_way_en", true},
	{-1, -1, 165, "OOB_way_en", true},
	{-1, -1, 166, "OOB_way_en", true},
	{-1, -1, 167, "OOB_way_en", true},
	{-1, -1, 168, "OOB_way_en", true},
	{-1, -1, 169, "OOB_way_en", true},

	/* 170 */
	{-1, -1, 170, "OOB_way_en", true},
	{-1, -1, 171, "OOB_way_en", true},
	{-1, -1, 172, "OOB_way_en", true},
	{-1, -1, 173, "OOB_way_en", true},
	{-1, -1, 174, "OOB_way_en", true},
	{-1, -1, 175, "OOB_way_en", true},
	{-1, -1, 176, "OOB_way_en", true},
	{-1, -1, 177, "OOB_way_en", true},
	{-1, -1, 178, "OOB_way_en", true},
	{-1, -1, 179, "OOB_way_en", true},

	/* 180 */
	{-1, -1, 180, "OOB_way_en", true},
	{-1, -1, 181, "OOB_way_en", true},
	{-1, -1, 182, "OOB_way_en", true},
	{-1, -1, 183, "OOB_way_en", true},
	{-1, -1, 184, "OOB_way_en", true},
	{-1, -1, 185, "OOB_way_en", true},
	{-1, -1, 186, "OOB_way_en", true},
	{-1, -1, 187, "OOB_way_en", true},
	{-1, -1, 188, "OOB_way_en", true},
	{-1, -1, 189, "OOB_way_en", true},

	/* 190 */
	{-1, -1, 190, "OOB_way_en", true},
	{-1, -1, 191, "OOB_way_en", true},
	{-1, -1, 192, "OOB_way_en", true},
	{-1, -1, 193, "OOB_way_en", true},
	{-1, -1, 194, "OOB_way_en", true},
	{-1, -1, 195, "OOB_way_en", true},
	{-1, -1, 196, "OOB_way_en", true},
	{-1, -1, 197, "OOB_way_en", true},
	{-1, -1, 198, "OOB_way_en", true},
	{-1, -1, 199, "OOB_way_en", true},

	/* 200 */
	{-1, -1, 200, "OOB_way_en", true},
	{-1, -1, 201, "OOB_way_en", true},
	{-1, -1, 202, "OOB_way_en", true},
	{-1, -1, 203, "OOB_way_en", true},
	{-1, -1, 204, "OOB_way_en", true},
	{-1, -1, 205, "OOB_way_en", true},
	{-1, -1, 206, "OOB_way_en", true},
	{-1, -1, 207, "OOB_way_en", true},
	{-1, -1, 208, "OOB_way_en", true},
	{-1, -1, 209, "OOB_way_en", true},

	/* 210 */
	{-1, -1, 210, "OOB_way_en", true},
	{-1, -1, 211, "OOB_way_en", true},
	{-1, -1, 212, "OOB_way_en", true},
	{-1, -1, 213, "OOB_way_en", true},
	{-1, -1, 214, "OOB_way_en", true},
	{-1, -1, 215, "OOB_way_en", true},
	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},

	/* 220 */
	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},
	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},

	/* 230 */
	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "OOB_way_en", true},
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},

	/* 240 */
	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},
	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},

	/* 250 */
	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},
	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},

	/* 260 */
	{-1, -1, 260, "OOB_way_en", true},
	{-1, -1, 261, "OOB_way_en", true},
	{-1, -1, 262, "OOB_way_en", true},
	{-1, -1, 263, "OOB_way_en", true},
	{-1, -1, 264, "OOB_way_en", true},
	{-1, -1, 265, "OOB_way_en", true},
	{-1, -1, 266, "OOB_way_en", true},
	{-1, -1, 267, "OOB_way_en", true},
	{-1, -1, 268, "OOB_way_en", true},
	{-1, -1, 269, "OOB_way_en", true},

	/* 270 */
	{-1, -1, 270, "OOB_way_en", true},
	{-1, -1, 271, "OOB_way_en", true},
	{-1, -1, 272, "OOB_way_en", true},
	{-1, -1, 273, "OOB_way_en", true},
	{-1, -1, 274, "OOB_way_en", true},
	{-1, -1, 275, "OOB_way_en", true},
	{-1, -1, 276, "OOB_way_en", true},
	{-1, -1, 277, "OOB_way_en", true},
	{-1, -1, 278, "OOB_way_en", true},
	{-1, -1, 279, "OOB_way_en", true},

	/* 280 */
	{-1, -1, 280, "OOB_way_en", true},
	{-1, -1, 281, "OOB_way_en", true},
	{-1, -1, 282, "OOB_way_en", true},
	{-1, -1, 283, "OOB_way_en", true},
	{-1, -1, 284, "OOB_way_en", true},
	{-1, -1, 285, "OOB_way_en", true},
	{-1, -1, 286, "OOB_way_en", true},
	{-1, -1, 287, "OOB_way_en", true},
	{-1, -1, 288, "OOB_way_en", true},
	{-1, -1, 289, "OOB_way_en", true},

	/* 290 */
	{-1, -1, 290, "OOB_way_en", true},
	{-1, -1, 291, "OOB_way_en", true},
	{-1, -1, 292, "OOB_way_en", true},
	{-1, -1, 293, "OOB_way_en", true},
	{-1, -1, 294, "OOB_way_en", true},
	{-1, -1, 295, "OOB_way_en", true},
	{-1, -1, 296, "OOB_way_en", true},
	{-1, -1, 297, "OOB_way_en", true},
	{-1, -1, 298, "OOB_way_en", true},
	{-1, -1, 299, "OOB_way_en", true},

	/* 300 */
	{-1, -1, 300, "OOB_way_en", true},
	{-1, -1, 301, "OOB_way_en", true},
	{-1, -1, 302, "OOB_way_en", true},

	{-1, -1, 303, "Decode_error", true},
	{-1, -1, 304, "Decode_error", true},
	{-1, -1, 305, "Decode_error", true},
	{-1, -1, 306, "Decode_error", true},
	{-1, -1, 307, "Decode_error", true},

	{-1, -1, 308, "NEMI_PDN_NON_SEC_EMI_MPU_REG", false},
	{-1, -1, 309, "NEMI_PDN_NON_SEC_EMI_REG", false},
	{-1, -1, 310, "SEMI_PDN_NON_SEC_EMI_MPU_REG", false},
	{-1, -1, 311, "SEMI_PDN_NON_SEC_EMI_REG", false},
	{-1, -1, 312, "DEVAPC_APINFRA_EMI_AO", false},
	{-1, -1, 313, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_ssr[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BCRM_APINFRA_SSR_PDN_APB_S", true},
	{0, 1, 1, "DEVAPC_APINFRA_SSR_PDN_APB_S", true},
	{0, 2, 2, "SSR_APB0_S", true},
	{0, 3, 3, "SSR_APB1_S", true},
	{0, 4, 4, "SSR_APB2_S", true},
	{0, 5, 5, "SSR_APB3_S", true},
	{0, 6, 6, "SSR_APB4_S", true},
	{0, 7, 7, "SSR_APB5_S", true},
	{0, 8, 8, "SSR_APB6_S", true},
	{0, 9, 9, "SSR_APB7_S", true},

	/* 10 */
	{0, 10, 10, "SSR_APB8_S", true},
	{0, 11, 11, "SSR_APB9_S", true},
	{0, 12, 12, "SSR_APB10_S", true},
	{0, 13, 13, "SSR_APB11_S", true},
	{0, 14, 14, "SSR_APB12_S", true},
	{0, 15, 15, "SSR_APB13_S", true},
	{0, 16, 16, "SSR_APB14_S", true},
	{0, 17, 17, "SSR_APB15_S", true},
	{0, 18, 18, "SSR_APB16_S", true},
	{0, 19, 19, "SSR_APB17_S", true},

	/* 20 */
	{0, 20, 20, "SSR_AP_APB0_S", true},
	{0, 21, 21, "SSR_AP_APB1_S", true},
	{0, 22, 22, "SSR_AP_APB2_S", true},
	{0, 23, 23, "SSR_AP_APB3_S", true},
	{0, 24, 24, "SSR_AP_APB4_S", true},
	{0, 25, 25, "SSR_AP_APB5_S", true},
	{0, 26, 26, "SSR_AP_APB6_S", true},
	{0, 27, 27, "SSR_AP_APB7_S", true},
	{0, 28, 28, "SSR_AP_APB8_S", true},
	{0, 29, 29, "SSR_AP_APB9_S", true},

	/* 30 */
	{0, 30, 30, "SSR_AP_APB10_S", true},
	{0, 31, 31, "SSR_AP_APB11_S", true},
	{0, 32, 32, "SSR_AP_APB12_S", true},
	{0, 33, 33, "SSR_AP_APB13_S", true},
	{0, 34, 34, "SSR_AP_APB14_S", true},
	{0, 35, 35, "SSR_AP_APB15_S", true},
	{0, 36, 36, "BCRM_APINFRA_SSR_AO_APB_S", true},
	{0, 37, 37, "DEVAPC_APINFRA_SSR_AO_APB_S", true},
	{0, 38, 38, "DEBUG_CTRL_APINFRA_SSR_AO_APB_S", true},

	{-1, -1, 39, "OOB_way_en", true},

	/* 40 */
	{-1, -1, 40, "OOB_way_en", true},
	{-1, -1, 41, "OOB_way_en", true},
	{-1, -1, 42, "OOB_way_en", true},
	{-1, -1, 43, "OOB_way_en", true},
	{-1, -1, 44, "OOB_way_en", true},
	{-1, -1, 45, "OOB_way_en", true},
	{-1, -1, 46, "OOB_way_en", true},
	{-1, -1, 47, "OOB_way_en", true},
	{-1, -1, 48, "OOB_way_en", true},
	{-1, -1, 49, "OOB_way_en", true},

	/* 50 */
	{-1, -1, 50, "OOB_way_en", true},
	{-1, -1, 51, "OOB_way_en", true},
	{-1, -1, 52, "OOB_way_en", true},
	{-1, -1, 53, "OOB_way_en", true},
	{-1, -1, 54, "OOB_way_en", true},
	{-1, -1, 55, "OOB_way_en", true},
	{-1, -1, 56, "OOB_way_en", true},
	{-1, -1, 57, "OOB_way_en", true},
	{-1, -1, 58, "OOB_way_en", true},
	{-1, -1, 59, "OOB_way_en", true},

	/* 60 */
	{-1, -1, 60, "OOB_way_en", true},
	{-1, -1, 61, "OOB_way_en", true},
	{-1, -1, 62, "OOB_way_en", true},
	{-1, -1, 63, "OOB_way_en", true},
	{-1, -1, 64, "OOB_way_en", true},
	{-1, -1, 65, "OOB_way_en", true},
	{-1, -1, 66, "OOB_way_en", true},
	{-1, -1, 67, "OOB_way_en", true},
	{-1, -1, 68, "OOB_way_en", true},
	{-1, -1, 69, "OOB_way_en", true},

	/* 70 */
	{-1, -1, 70, "OOB_way_en", true},
	{-1, -1, 71, "OOB_way_en", true},
	{-1, -1, 72, "OOB_way_en", true},
	{-1, -1, 73, "OOB_way_en", true},
	{-1, -1, 74, "OOB_way_en", true},
	{-1, -1, 75, "OOB_way_en", true},
	{-1, -1, 76, "OOB_way_en", true},
	{-1, -1, 77, "OOB_way_en", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},

	/* 80 */
	{-1, -1, 80, "Decode_error", true},
	{-1, -1, 81, "Decode_error", true},

	{-1, -1, 82, "DEVAPC_APINFRA_SSR_AO", false},
	{-1, -1, 83, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_mem[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DPMAIF_PDN_APB_S", true},
	{0, 1, 1, "DPMAIF_PDN_APB_S-1", true},
	{0, 2, 2, "DPMAIF_PDN_APB_S-2", true},
	{0, 3, 3, "DPMAIF_PDN_APB_S-3", true},
	{0, 4, 4, "BCRM_APINFRA_MEM_PDN_APB_S", true},
	{0, 5, 5, "DEVAPC_APINFRA_MEM_PDN_APB_S", true},
	{0, 6, 7, "BCRM_APINFRA_MEM_AO_APB_S", true},
	{0, 7, 8, "DEVAPC_APINFRA_MEM_AO_APB_S", true},
	{0, 8, 9, "DEBUG_CTRL_APINFRA_MEM_AO_APB_S", true},
	{0, 9, 10, "DPMAIF_AO_APB_S", true},

	/* 10 */
	{0, 10, 11, "MEM_PBUS_APB_S", true},
	{0, 11, 12, "MEM_EFUSE_AO_APB_S", true},

	{-1, -1, 13, "OOB_way_en", true},
	{-1, -1, 14, "OOB_way_en", true},
	{-1, -1, 15, "OOB_way_en", true},
	{-1, -1, 16, "OOB_way_en", true},
	{-1, -1, 17, "OOB_way_en", true},
	{-1, -1, 18, "OOB_way_en", true},
	{-1, -1, 19, "OOB_way_en", true},

	/* 20 */
	{-1, -1, 20, "OOB_way_en", true},
	{-1, -1, 21, "OOB_way_en", true},
	{-1, -1, 22, "OOB_way_en", true},
	{-1, -1, 23, "OOB_way_en", true},

	{-1, -1, 24, "Decode_error", true},
	{-1, -1, 25, "Decode_error", true},

	{-1, -1, 26, "DEVAPC_APINFRA_MEM_AO", false},
	{-1, -1, 27, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_mem_ctrl[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BCRM_APINFRA_MEM_CTRL_PDN_APB_S", true},
	{0, 1, 1, "DEVAPC_APINFRA_MEM_CTRL_PDN_APB_S", true},
	{0, 2, 2, "IFRBUS_MEM_PDN_APB_S", true},
	{0, 3, 3, "APINFRA_MEM_BUS_HRE_APB_S", true},
	{0, 4, 4, "APINFRA_MEM_PDN_CLK_MON_APB_S", true},
	{0, 5, 5, "EM4_FAKE_ENG_APB_S", true},
	{0, 6, 6, "EM6_FAKE_ENG_APB_S", true},
	{0, 7, 7, "EM7_FAKE_ENG_APB_S", true},
	{0, 8, 8, "ECONN_MON_APB_S", true},
	{0, 9, 9, "EVLPSYS_MON_APB_S", true},

	/* 10 */
	{0, 10, 10, "EADSP1_MON_APB_S", true},
	{0, 11, 11, "EPERI0_MON_APB_S", true},
	{0, 12, 12, "EPERI1_MON_APB_S", true},
	{0, 13, 13, "ESSR_MON_APB_S", true},
	{0, 14, 14, "EADSP0_MON_APB_S", true},
	{0, 15, 15, "BCRM_APINFRA_MEM_CTRL_AO_APB_S", true},
	{0, 16, 16, "DEVAPC_APINFRA_MEM_CTRL_AO_APB_S", true},
	{0, 17, 17, "DEBUG_CTRL_APINFRA_MEM_CTRL_AO_APB_S", true},
	{0, 18, 18, "IFRBUS_MEM_AO_APB_S", true},
	{0, 19, 19, "APINFRA_MEM_SECURITY_AO_APB_S", true},

	/* 20 */
	{0, 20, 20, "MBIST_MEM_AO_APB_S", true},
	{0, 21, 21, "APINFRA_MEM_AO_CLK_MON_APB_S", true},
	{0, 22, 22, "SEC_APINFRA_MEM_SECURITY_AO_APB_S", true},

	{-1, -1, 23, "OOB_way_en", true},
	{-1, -1, 24, "OOB_way_en", true},
	{-1, -1, 25, "OOB_way_en", true},
	{-1, -1, 26, "OOB_way_en", true},
	{-1, -1, 27, "OOB_way_en", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

	/* 30 */
	{-1, -1, 31, "OOB_way_en", true},
	{-1, -1, 32, "OOB_way_en", true},
	{-1, -1, 33, "OOB_way_en", true},
	{-1, -1, 34, "OOB_way_en", true},
	{-1, -1, 35, "OOB_way_en", true},
	{-1, -1, 36, "OOB_way_en", true},
	{-1, -1, 37, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},
	{-1, -1, 39, "OOB_way_en", true},

	/* 40 */
	{-1, -1, 41, "OOB_way_en", true},
	{-1, -1, 42, "OOB_way_en", true},
	{-1, -1, 43, "OOB_way_en", true},
	{-1, -1, 44, "OOB_way_en", true},
	{-1, -1, 45, "OOB_way_en", true},
	{-1, -1, 46, "OOB_way_en", true},
	{-1, -1, 47, "OOB_way_en", true},

	{-1, -1, 48, "Decode_error", true},
	{-1, -1, 49, "Decode_error", true},

	{-1, -1, 50, "DEVAPC_APINFRA_MEM_CTRL_AO", false},
	{-1, -1, 51, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_mem_intf[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BCRM_APINFRA_MEM_INTF_PDN_APB_S", true},
	{0, 1, 1, "DEVAPC_APINFRA_MEM_INTF_PDN_APB_S", true},
	{0, 2, 2, "MEM_NOC_CTRL_APB_S", true},
	{0, 3, 3, "BCRM_APINFRA_MEM_INTF_AO_APB_S", true},
	{0, 4, 4, "DEVAPC_APINFRA_MEM_INTF_AO_APB_S", true},
	{0, 5, 5, "DEBUG_CTRL_APINFRA_MEM_INTF_AO_APB_S", true},
	{0, 6, 6, "BCRM_APINFRA_MEM_NOC_APB_S", true},

	{-1, -1, 7, "OOB_way_en", true},
	{-1, -1, 8, "OOB_way_en", true},
	{-1, -1, 9, "OOB_way_en", true},

	/* 10 */
	{-1, -1, 10, "OOB_way_en", true},
	{-1, -1, 11, "OOB_way_en", true},
	{-1, -1, 12, "OOB_way_en", true},
	{-1, -1, 13, "OOB_way_en", true},
	{-1, -1, 14, "OOB_way_en", true},
	{-1, -1, 15, "OOB_way_en", true},

	{-1, -1, 16, "Decode_error", true},
	{-1, -1, 17, "Decode_error", true},

	{-1, -1, 18, "DEVAPC_APINFRA_MEM_INTF_AO", false},
	{-1, -1, 19, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_int[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "CQ_DMA_APB_S", true},
	{0, 1, 1, "CQ_DMA_APB_S-1", true},
	{0, 2, 2, "CQ_DMA_APB_S-2", true},
	{0, 3, 3, "CQ_DMA_APB_S-3", true},
	{0, 4, 4, "CQ_DMA_APB_S-4", true},
	{0, 5, 5, "CQ_DMA_APB_S-5", true},
	{0, 6, 6, "CQ_DMA_APB_S-6", true},
	{0, 7, 7, "BCRM_APINFRA_INT_PDN_APB_S", true},
	{0, 8, 8, "DEVAPC_APINFRA_INT_PDN_APB_S", true},
	{0, 9, 9, "CCIF0_AP_APB_S", true},

	/* 10 */
	{0, 10, 10, "CCIF0_MD_APB_S", true},
	{0, 11, 11, "CCIF1_AP_APB_S", true},
	{0, 12, 12, "CCIF1_MD_APB_S", true},
	{0, 13, 14, "RESERVED_DVFS_PROC_APB_S", true},
	{0, 14, 15, "CCIF2_AP_APB_S", true},
	{0, 15, 16, "CCIF2_MD_APB_S", true},
	{0, 16, 17, "CCIF3_AP_APB_S", true},
	{0, 17, 18, "CCIF3_MD_APB_S", true},
	{0, 18, 19, "CCIF4_AP_APB_S", true},
	{0, 19, 20, "CCIF4_MD_APB_S", true},

	/* 20 */
	{0, 20, 21, "CCIF5_AP_APB_S", true},
	{0, 21, 22, "CCIF5_MD_APB_S", true},
	{0, 22, 23, "BCRM_APINFRA_INT_AO_APB_S", true},
	{0, 23, 24, "DEVAPC_APINFRA_INT_AO_APB_S", true},
	{0, 24, 25, "DEBUG_CTRL_APINFRA_INT_AO_APB_S", true},

	{-1, -1, 26, "OOB_way_en", true},
	{-1, -1, 27, "OOB_way_en", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

	/* 30 */
	{-1, -1, 30, "OOB_way_en", true},
	{-1, -1, 31, "OOB_way_en", true},
	{-1, -1, 32, "OOB_way_en", true},
	{-1, -1, 33, "OOB_way_en", true},
	{-1, -1, 34, "OOB_way_en", true},
	{-1, -1, 35, "OOB_way_en", true},
	{-1, -1, 36, "OOB_way_en", true},
	{-1, -1, 37, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},
	{-1, -1, 39, "OOB_way_en", true},

	/* 40 */
	{-1, -1, 40, "OOB_way_en", true},
	{-1, -1, 41, "OOB_way_en", true},
	{-1, -1, 42, "OOB_way_en", true},
	{-1, -1, 43, "OOB_way_en", true},
	{-1, -1, 44, "OOB_way_en", true},
	{-1, -1, 45, "OOB_way_en", true},
	{-1, -1, 46, "OOB_way_en", true},

	{-1, -1, 47, "Decode_error", true},
	{-1, -1, 48, "Decode_error", true},

	{-1, -1, 49, "CQ_DMA", false},

	/* 50 */
	{-1, -1, 50, "DEVAPC_APINFRA_INT_AO", false},
	{-1, -1, 51, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_mmu[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "SMMU_APB_S", true},
	{0, 1, 1, "SMMU_APB_S-1", true},
	{0, 2, 2, "SMMU_APB_S-2", true},
	{0, 3, 3, "BCRM_APINFRA_MMU_PDN_APB_S", true},
	{0, 4, 4, "DEVAPC_APINFRA_MMU_PDN_APB_S", true},
	{0, 5, 5, "ERSI0_APB_S", true},
	{0, 6, 6, "ERSI1_APB_S", true},
	{0, 7, 7, "ERSI2_APB_S", true},
	{0, 8, 9, "ENM4_MON_APB_S", true},
	{0, 9, 10, "ESM4_MON_APB_S", true},

	/* 10 */
	{0, 10, 11, "ENM6_MON_APB_S", true},
	{0, 11, 12, "ESM6_MON_APB_S", true},
	{0, 12, 13, "ENM7_MON_APB_S", true},
	{0, 13, 14, "ESM7_MON_APB_S", true},
	{0, 14, 15, "ESMI_SUB_COMM_S", true},
	{0, 15, 16, "M4_SMMU_SID_APB_S", true},
	{0, 16, 17, "BCRM_APINFRA_MMU_AO_APB_S", true},
	{0, 17, 18, "DEVAPC_APINFRA_MMU_AO_APB_S", true},
	{0, 18, 19, "DEBUG_CTRL_APINFRA_MMU_AO_APB_S", true},

	/* 20 */
	{-1, -1, 20, "OOB_way_en", true},
	{-1, -1, 21, "OOB_way_en", true},
	{-1, -1, 22, "OOB_way_en", true},
	{-1, -1, 23, "OOB_way_en", true},
	{-1, -1, 24, "OOB_way_en", true},
	{-1, -1, 25, "OOB_way_en", true},
	{-1, -1, 26, "OOB_way_en", true},
	{-1, -1, 27, "OOB_way_en", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

	/* 30 */
	{-1, -1, 30, "OOB_way_en", true},
	{-1, -1, 31, "OOB_way_en", true},
	{-1, -1, 32, "OOB_way_en", true},
	{-1, -1, 33, "OOB_way_en", true},
	{-1, -1, 34, "OOB_way_en", true},
	{-1, -1, 35, "OOB_way_en", true},
	{-1, -1, 36, "OOB_way_en", true},
	{-1, -1, 37, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},

	{-1, -1, 39, "Decode_error", true},

	/* 40 */
	{-1, -1, 40, "Decode_error", true},
	{-1, -1, 50, "DEVAPC_APINFRA_MMU_AO", false},
	{-1, -1, 51, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_apinfra_slb[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BCRM_APINFRA_SLB_PDN_APB_S", true},
	{0, 1, 1, "DEVAPC_APINFRA_SLB_PDN_APB_S", true},
	{0, 2, 2, "ERSI3_APB_S", true},
	{0, 3, 3, "ERSI4_APB_S", true},
	{0, 4, 4, "ESLB_CFG_MON_APB_S", true},
	{0, 5, 5, "ESLB0_MON_APB_S", true},
	{0, 6, 6, "ESLB1_MON_APB_S", true},
	{0, 7, 7, "ENSLB_MON_APB_S", true},
	{0, 8, 8, "ESSLB_MON_APB_S", true},
	{0, 9, 9, "EFAKE_ENG0_APB_S", true},

	/* 10 */
	{0, 10, 10, "EFAKE_ENG1_APB_S", true},
	{0, 11, 11, "BCRM_APINFRA_SLB_AO_APB_S", true},
	{0, 12, 12, "DEVAPC_APINFRA_SLB_AO_APB_S", true},
	{0, 13, 13, "DEBUG_CTRL_APINFRA_SLB_AO_APB_S", true},

	{-1, -1, 14, "OOB_way_en", true},
	{-1, -1, 15, "OOB_way_en", true},
	{-1, -1, 16, "OOB_way_en", true},
	{-1, -1, 17, "OOB_way_en", true},
	{-1, -1, 18, "OOB_way_en", true},
	{-1, -1, 19, "OOB_way_en", true},

	/* 20 */
	{-1, -1, 20, "OOB_way_en", true},
	{-1, -1, 21, "OOB_way_en", true},
	{-1, -1, 22, "OOB_way_en", true},
	{-1, -1, 23, "OOB_way_en", true},
	{-1, -1, 24, "OOB_way_en", true},
	{-1, -1, 25, "OOB_way_en", true},
	{-1, -1, 26, "OOB_way_en", true},
	{-1, -1, 27, "OOB_way_en", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

	/* 30 */
	{-1, -1, 30, "Decode_error", true},
	{-1, -1, 31, "Decode_error", true},

	{-1, -1, 32, "DEVAPC_APINFRA_SLB_AO", false},
	{-1, -1, 33, "reserve", false},
};

static const struct mtk_device_info mt6991_devices_peri_par_a0[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "UARTHUB_APB_S", true},
	{0, 1, 1, "UARTHUB_APB_S-1", true},
	{0, 2, 2, "UARTHUB_APB_S-2", true},
	{0, 3, 3, "UARTHUB_APB_S-3", true},
	{0, 4, 4, "UARTHUB_APB_S-4", true},
	{0, 5, 5, "SPI0_APB_S", true},
	{0, 6, 6, "SPI0_APB_S-1", true},
	{0, 7, 7, "SPI1_APB_S", true},
	{0, 8, 8, "SPI1_APB_S-1", true},
	{0, 9, 9, "SPI2_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPI2_APB_S-1", true},
	{0, 11, 11, "SPI3_APB_S", true},
	{0, 12, 12, "SPI3_APB_S-1", true},
	{0, 13, 13, "SPI4_APB_S", true},
	{0, 14, 14, "SPI4_APB_S-1", true},
	{0, 15, 15, "SPI5_APB_S", true},
	{0, 16, 16, "SPI5_APB_S-1", true},
	{0, 17, 17, "SPI6_APB_S", true},
	{0, 18, 18, "SPI6_APB_S-1", true},
	{0, 19, 19, "SPI7_APB_S", true},

	/* 20 */
	{0, 20, 20, "SPI7_APB_S-1", true},
	{0, 21, 21, "I2C_APB_S", true},
	{0, 22, 22, "I2C_APB_S-1", true},
	{0, 23, 23, "I2C_APB_S-2", true},
	{0, 24, 24, "I2C_APB_S-3", true},
	{0, 25, 25, "I2C_APB_S-4", true},
	{0, 26, 26, "I2C_APB_S-5", true},
	{0, 27, 27, "I2C_APB_S-6", true},
	{0, 28, 28, "I2C_APB_S-7", true},
	{0, 29, 29, "I2C_APB_S-8", true},

	/* 30 */
	{0, 30, 30, "I2C_APB_S-9", true},
	{0, 31, 31, "I2C_APB_S-10", true},
	{0, 32, 32, "I2C_APB_S-11", true},
	{0, 33, 33, "I2C_APB_S-12", true},
	{0, 34, 34, "I2C_APB_S-13", true},
	{0, 35, 35, "I2C_APB_S-14", true},
	{0, 36, 36, "I2C_APB_S-15", true},
	{0, 37, 37, "I2C_APB_S-16", true},
	{0, 38, 38, "APDMA_APB_S", true},
	{0, 39, 39, "APDMA_APB_S-1", true},

	/* 40 */
	{0, 40, 40, "APDMA_APB_S-2", true},
	{0, 41, 41, "APDMA_APB_S-3", true},
	{0, 42, 42, "APDMA_APB_S-4", true},
	{0, 43, 43, "APDMA_APB_S-5", true},
	{0, 44, 44, "APDMA_APB_S-6", true},
	{0, 45, 45, "APDMA_APB_S-7", true},
	{0, 46, 46, "APDMA_APB_S-8", true},
	{0, 47, 47, "APDMA_APB_S-9", true},
	{0, 48, 48, "APDMA_APB_S-10", true},
	{0, 49, 49, "APDMA_APB_S-11", true},

	/* 50 */
	{0, 50, 50, "APDMA_APB_S-12", true},
	{0, 51, 51, "APDMA_APB_S-13", true},
	{0, 52, 52, "APDMA_APB_S-14", true},
	{0, 53, 53, "APDMA_APB_S-15", true},
	{0, 54, 54, "APDMA_APB_S-16", true},
	{0, 55, 55, "APDMA_APB_S-17", true},
	{0, 56, 56, "APDMA_APB_S-18", true},
	{0, 57, 57, "APDMA_APB_S-19", true},
	{0, 58, 58, "APDMA_APB_S-20", true},
	{0, 59, 59, "APDMA_APB_S-21", true},

	/* 60 */
	{0, 60, 60, "APDMA_APB_S-22", true},
	{0, 61, 61, "APDMA_APB_S-23", true},
	{0, 62, 62, "APDMA_APB_S-24", true},
	{0, 63, 63, "APDMA_APB_S-25", true},
	{0, 64, 64, "APDMA_APB_S-26", true},
	{0, 65, 65, "APDMA_APB_S-27", true},
	{0, 66, 66, "APDMA_APB_S-28", true},
	{0, 67, 67, "APDMA_APB_S-29", true},
	{0, 68, 68, "APDMA_APB_S-30", true},
	{0, 69, 69, "APDMA_APB_S-31", true},

	/* 70 */
	{0, 70, 70, "APDMA_APB_S-32", true},
	{0, 71, 71, "APDMA_APB_S-33", true},
	{0, 72, 72, "UART0_APB_S", true},
	{0, 73, 73, "UART1_APB_S", true},
	{0, 74, 74, "UART2_APB_S", true},
	{0, 75, 75, "UART3_APB_S", true},
	{0, 76, 76, "UART4_APB_S", true},
	{0, 77, 77, "UART5_APB_S", true},
	{0, 78, 79, "PWM_PERI_APB_S", true},
	{0, 79, 80, "DISP_PWM0_APB_S", true},

	/* 80 */
	{0, 80, 81, "DISP_PWM1_APB_S", true},
	{0, 81, 82, "DISP_PWM2_APB_S", true},
	{0, 82, 83, "DISP_PWM3_APB_S", true},
	{0, 83, 84, "DISP_PWM4_APB_S", true},
	{0, 84, 93, "NOR_APB_S", true},
	{0, 85, 96, "ETHERNET0_APB_S", true},
	{0, 86, 97, "ETHERNET1_APB_S", true},
	{0, 87, 98, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 88, 99, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 89, 100, "USB_TO_PERI_RX_SNOC_CTRL", true},

	/* 90 */
	{0, 90, 101, "PERI_TO_PCIE2_TX_SNOC_CTRL", true},
	{0, 91, 102, "PCIE2_TO_PERI_RX_SNOC_CTRL", true},
	{0, 92, 103, "PERI_TO_PCIE_TX_SNOC_CTRL", true},
	{0, 93, 104, "PCIE_TO_PERI_RX_SNOC_CTRL", true},
	{0, 94, 105, "UFS_TO_PERI_RX_SNOC_CTRL", true},
	{0, 95, 106, "PERI_FAKE_ENG_MI20_APB_S", true},
	{0, 96, 107, "PERI_FAKE_ENG_MI21_APB_S", true},
	{0, 97, 108, "PERI_FAKE_ENG_MI22_APB_S", true},
	{0, 98, 109, "PCIE0_S", true},
	{0, 99, 110, "PCIE0_S-1", true},

	/* 100 */
	{0, 100, 111, "PCIE0_S-2", true},
	{0, 101, 112, "PCIE0_S-3", true},
	{0, 102, 113, "PCIE0_S-4", true},
	{0, 103, 114, "PCIE0_S-5", true},
	{0, 104, 115, "PCIE0_S-6", true},
	{0, 105, 116, "PCIE0_S-7", true},
	{0, 106, 117, "PCIE0_S-8", true},
	{0, 107, 118, "PCIE0_S-9", true},
	{0, 108, 119, "PCIE0_S-10", true},
	{0, 109, 120, "PCIE0_S-11", true},

	/* 110 */
	{0, 110, 121, "PCIE0_S-12", true},
	{0, 111, 122, "PCIE0_S-13", true},
	{0, 112, 123, "PCIE0_S-14", true},
	{0, 113, 124, "PCIE0_S-15", true},
	{0, 114, 125, "PCIE0_S-16", true},
	{0, 115, 126, "PCIE0_S-17", true},
	{0, 116, 127, "PCIE0_S-18", true},
	{0, 117, 128, "PCIE0_S-19", true},
	{0, 118, 129, "PCIE0_S-20", true},
	{0, 119, 130, "PCIE0_S-21", true},

	/* 120 */
	{0, 120, 131, "PCIE0_S-22", true},
	{0, 121, 132, "PCIE0_S-23", true},
	{0, 122, 133, "PCIE0_S-24", true},
	{0, 123, 134, "PCIE0_S-25", true},
	{0, 124, 135, "PCIE0_S-26", true},
	{0, 125, 136, "PCIE1_S", true},
	{0, 126, 137, "PCIE1_S-1", true},
	{0, 127, 138, "PCIE1_S-2", true},
	{0, 128, 139, "PCIE1_S-3", true},
	{0, 129, 140, "PCIE1_S-4", true},

	/* 130 */
	{0, 130, 141, "PCIE1_S-5", true},
	{0, 131, 142, "PCIE1_S-6", true},
	{0, 132, 143, "PCIE1_S-7", true},
	{0, 133, 144, "PCIE1_S-8", true},
	{0, 134, 145, "PCIE1_S-9", true},
	{0, 135, 146, "PCIE1_S-10", true},
	{0, 136, 147, "PCIE1_S-11", true},
	{0, 137, 148, "PCIE1_S-12", true},
	{0, 138, 149, "PCIE1_S-13", true},
	{0, 139, 150, "PCIE1_S-14", true},

	/* 140 */
	{0, 140, 151, "PCIE1_S-15", true},
	{0, 141, 152, "PCIE1_S-16", true},
	{0, 142, 153, "PCIE1_S-17", true},
	{0, 143, 154, "PCIE1_S-18", true},
	{0, 144, 155, "PCIE1_S-19", true},
	{0, 145, 156, "PCIE1_S-20", true},
	{0, 146, 157, "PCIE1_S-21", true},
	{0, 147, 158, "PCIE1_S-22", true},
	{0, 148, 159, "USB0_S", true},
	{0, 149, 160, "USB0_S-1", true},

	/* 150 */
	{0, 150, 161, "USB0_S-2", true},
	{0, 151, 162, "USB0_S-3", true},
	{0, 152, 163, "USB0_S-4", true},
	{0, 153, 164, "USB0_S-5", true},
	{0, 154, 165, "USB0_S-6", true},
	{0, 155, 166, "USB0_S-7", true},
	{0, 156, 167, "USB0_S-8", true},
	{0, 157, 168, "USB0_S-9", true},
	{0, 158, 169, "USB0_S-10", true},
	{0, 159, 170, "USB0_S-11", true},

	/* 160 */
	{0, 160, 171, "USB0_S-12", true},
	{0, 161, 172, "USB0_S-13", true},
	{0, 162, 173, "USB0_S-14", true},
	{0, 163, 174, "USB0_S-15", true},
	{0, 164, 175, "USB0_S-16", true},
	{0, 165, 176, "USB0_S-17", true},
	{0, 166, 177, "USB0_S-18", true},
	{0, 167, 178, "USB0_S-19", true},
	{0, 168, 179, "USB0_S-20", true},
	{0, 169, 180, "USB0_S-21", true},

	/* 170 */
	{0, 170, 181, "USB0_S-22", true},
	{0, 171, 182, "USB0_S-23", true},
	{0, 172, 183, "USB0_S-24", true},
	{0, 173, 185, "NOR_AXI_S", true},
	{0, 174, 188, "MSDC1_S-1", true},
	{0, 175, 189, "MSDC1_S-2", true},
	{0, 176, 190, "MSDC2_S-1", true},
	{0, 177, 191, "MSDC2_S-2", true},
	{0, 178, 192, "PERI_MBIST_AO_APB_S", true},
	{0, 179, 193, "BCRM_PERI_PAR_AO_APB_S", true},

	/* 180 */
	{0, 180, 194, "PERICFG_SEC_AO_APB_S", true},
	{0, 181, 195, "PERICFG_AO_DUAL_APB_S", true},
	{0, 182, 196, "PERICFG_AO_APB_S", true},
	{0, 183, 197, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{0, 184, 198, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 185, 199, "PERI_AO_PBUS_S", true},
	{0, 186, 200, "PERI_PCIE0_SMPU_APB_S", true},
	{0, 187, 201, "PERI_PCIE1_SMPU_APB_S", true},
	{0, 188, 202, "PERI_SMI_SUB_COMMON0_APB_S", true},
	{0, 189, 203, "PERI_SMI_SUB_COMMON1_APB_S", true},

	/* 190 */
	{-1, -1, 204, "OOB_way_en", true},
	{-1, -1, 205, "OOB_way_en", true},
	{-1, -1, 206, "OOB_way_en", true},
	{-1, -1, 207, "OOB_way_en", true},
	{-1, -1, 208, "OOB_way_en", true},
	{-1, -1, 209, "OOB_way_en", true},
	{-1, -1, 210, "OOB_way_en", true},
	{-1, -1, 211, "OOB_way_en", true},
	{-1, -1, 212, "OOB_way_en", true},
	{-1, -1, 213, "OOB_way_en", true},

	/* 200 */
	{-1, -1, 214, "OOB_way_en", true},
	{-1, -1, 215, "OOB_way_en", true},
	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},
	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},
	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},

	/* 210 */
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},
	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "OOB_way_en", true},
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},

	/* 220 */
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},
	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},
	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},

	/* 230 */
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},
	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},
	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},

	/* 240 */
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},
	{-1, -1, 260, "OOB_way_en", true},

	{-1, -1, 261, "Decode_error", true},
	{-1, -1, 262, "Decode_error", true},
	{-1, -1, 263, "Decode_error", true},

	/* 250 */
	{-1, -1, 264, "IIC_P2P_REMAP", false},
	{-1, -1, 265, "APDMA", false},
	{-1, -1, 266, "PCIE0_SMPU", false},
	{-1, -1, 267, "PCIE1_SMPU", false},
	{-1, -1, 268, "DEVICE_APC_PERI _AO", false},
	{-1, -1, 269, "DEVICE_APC_PERI_PDN", false},
};

static const struct mtk_device_info mt6991_devices_peri_par_b0[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "UARTHUB_APB_S", true},
	{0, 1, 1, "UARTHUB_APB_S-1", true},
	{0, 2, 2, "UARTHUB_APB_S-2", true},
	{0, 3, 3, "UARTHUB_APB_S-3", true},
	{0, 4, 4, "UARTHUB_APB_S-4", true},
	{0, 5, 5, "SPI0_APB_S", true},
	{0, 6, 6, "SPI0_APB_S-1", true},
	{0, 7, 7, "SPI1_APB_S", true},
	{0, 8, 8, "SPI1_APB_S-1", true},
	{0, 9, 9, "SPI2_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPI2_APB_S-1", true},
	{0, 11, 11, "SPI3_APB_S", true},
	{0, 12, 12, "SPI3_APB_S-1", true},
	{0, 13, 13, "SPI4_APB_S", true},
	{0, 14, 14, "SPI4_APB_S-1", true},
	{0, 15, 15, "SPI5_APB_S", true},
	{0, 16, 16, "SPI5_APB_S-1", true},
	{0, 17, 17, "SPI6_APB_S", true},
	{0, 18, 18, "SPI6_APB_S-1", true},
	{0, 19, 19, "SPI7_APB_S", true},

	/* 20 */
	{0, 20, 20, "SPI7_APB_S-1", true},
	{0, 21, 21, "I2C_APB_S", true},
	{0, 22, 22, "I2C_APB_S-1", true},
	{0, 23, 23, "I2C_APB_S-2", true},
	{0, 24, 24, "I2C_APB_S-3", true},
	{0, 25, 25, "I2C_APB_S-4", true},
	{0, 26, 26, "I2C_APB_S-5", true},
	{0, 27, 27, "I2C_APB_S-6", true},
	{0, 28, 28, "I2C_APB_S-7", true},
	{0, 29, 29, "I2C_APB_S-8", true},

	/* 30 */
	{0, 30, 30, "I2C_APB_S-9", true},
	{0, 31, 31, "I2C_APB_S-10", true},
	{0, 32, 32, "I2C_APB_S-11", true},
	{0, 33, 33, "I2C_APB_S-12", true},
	{0, 34, 34, "I2C_APB_S-13", true},
	{0, 35, 35, "I2C_APB_S-14", true},
	{0, 36, 36, "I2C_APB_S-15", true},
	{0, 37, 37, "I2C_APB_S-16", true},
	{0, 38, 38, "APDMA_APB_S", true},
	{0, 39, 39, "APDMA_APB_S-1", true},

	/* 40 */
	{0, 40, 40, "APDMA_APB_S-2", true},
	{0, 41, 41, "APDMA_APB_S-3", true},
	{0, 42, 42, "APDMA_APB_S-4", true},
	{0, 43, 43, "APDMA_APB_S-5", true},
	{0, 44, 44, "APDMA_APB_S-6", true},
	{0, 45, 45, "APDMA_APB_S-7", true},
	{0, 46, 46, "APDMA_APB_S-8", true},
	{0, 47, 47, "APDMA_APB_S-9", true},
	{0, 48, 48, "APDMA_APB_S-10", true},
	{0, 49, 49, "APDMA_APB_S-11", true},

	/* 50 */
	{0, 50, 50, "APDMA_APB_S-12", true},
	{0, 51, 51, "APDMA_APB_S-13", true},
	{0, 52, 52, "APDMA_APB_S-14", true},
	{0, 53, 53, "APDMA_APB_S-15", true},
	{0, 54, 54, "APDMA_APB_S-16", true},
	{0, 55, 55, "APDMA_APB_S-17", true},
	{0, 56, 56, "APDMA_APB_S-18", true},
	{0, 57, 57, "APDMA_APB_S-19", true},
	{0, 58, 58, "APDMA_APB_S-20", true},
	{0, 59, 59, "APDMA_APB_S-21", true},

	/* 60 */
	{0, 60, 60, "APDMA_APB_S-22", true},
	{0, 61, 61, "APDMA_APB_S-23", true},
	{0, 62, 62, "APDMA_APB_S-24", true},
	{0, 63, 63, "APDMA_APB_S-25", true},
	{0, 64, 64, "APDMA_APB_S-26", true},
	{0, 65, 65, "APDMA_APB_S-27", true},
	{0, 66, 66, "APDMA_APB_S-28", true},
	{0, 67, 67, "APDMA_APB_S-29", true},
	{0, 68, 68, "APDMA_APB_S-30", true},
	{0, 69, 69, "APDMA_APB_S-31", true},

	/* 70 */
	{0, 70, 70, "APDMA_APB_S-32", true},
	{0, 71, 71, "APDMA_APB_S-33", true},
	{0, 72, 72, "APDMA_APB_S-34", true},
	{0, 73, 73, "APDMA_APB_S-35", true},
	{0, 74, 74, "APDMA_APB_S-36", true},
	{0, 75, 75, "APDMA_APB_S-37", true},
	{0, 76, 76, "UART0_APB_S", true},
	{0, 77, 77, "UART1_APB_S", true},
	{0, 78, 78, "UART2_APB_S", true},
	{0, 79, 79, "UART3_APB_S", true},

	/* 80 */
	{0, 80, 80, "UART4_APB_S", true},
	{0, 81, 81, "UART5_APB_S", true},
	{0, 82, 83, "PWM_PERI_APB_S", true},
	{0, 83, 84, "DISP_PWM0_APB_S", true},
	{0, 84, 85, "DISP_PWM1_APB_S", true},
	{0, 85, 86, "DISP_PWM2_APB_S", true},
	{0, 86, 87, "DISP_PWM3_APB_S", true},
	{0, 87, 88, "DISP_PWM4_APB_S", true},
	{0, 88, 97, "NOR_APB_S", true},
	{0, 89, 100, "ETHERNET0_APB_S", true},

	/* 90 */
	{0, 90, 101, "ETHERNET1_APB_S", true},
	{0, 91, 102, "ETHERNET0_MCGR_APB_S", true},
	{0, 92, 103, "ETHERNET1_MCGR_APB_S", true},
	{0, 93, 104, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 94, 105, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 95, 106, "USB_TO_PERI_RX_SNOC_CTRL", true},
	{0, 96, 107, "PERI_TO_PCIE2_TX_SNOC_CTRL", true},
	{0, 97, 108, "PCIE2_TO_PERI_RX_SNOC_CTRL", true},
	{0, 98, 109, "PERI_TO_PCIE_TX_SNOC_CTRL", true},
	{0, 99, 110, "PCIE_TO_PERI_RX_SNOC_CTRL", true},

	/* 100 */
	{0, 100, 111, "UFS_TO_PERI_RX_SNOC_CTRL", true},
	{0, 101, 112, "PERI_FAKE_ENG_MI20_APB_S", true},
	{0, 102, 113, "PERI_FAKE_ENG_MI21_APB_S", true},
	{0, 103, 114, "PERI_FAKE_ENG_MI22_APB_S", true},
	{0, 104, 115, "PERI_PCIE0_SMPU_APB_S", true},
	{0, 105, 116, "PERI_PCIE1_SMPU_APB_S", true},
	{0, 106, 117, "PERI_SMI_SUB_COMMON0_APB_S", true},
	{0, 107, 118, "PERI_SMI_SUB_COMMON1_APB_S", true},
	{0, 108, 119, "PCIE0_S", true},
	{0, 109, 120, "PCIE0_S-1", true},

	/* 110 */
	{0, 110, 121, "PCIE0_S-2", true},
	{0, 111, 122, "PCIE0_S-3", true},
	{0, 112, 123, "PCIE0_S-4", true},
	{0, 113, 124, "PCIE0_S-5", true},
	{0, 114, 125, "PCIE0_S-6", true},
	{0, 115, 126, "PCIE0_S-7", true},
	{0, 116, 127, "PCIE0_S-8", true},
	{0, 117, 128, "PCIE0_S-9", true},
	{0, 118, 129, "PCIE0_S-10", true},
	{0, 119, 130, "PCIE0_S-11", true},

	/* 120 */
	{0, 120, 131, "PCIE0_S-12", true},
	{0, 121, 132, "PCIE0_S-13", true},
	{0, 122, 133, "PCIE0_S-14", true},
	{0, 123, 134, "PCIE0_S-15", true},
	{0, 124, 135, "PCIE0_S-16", true},
	{0, 125, 136, "PCIE0_S-17", true},
	{0, 126, 137, "PCIE0_S-18", true},
	{0, 127, 138, "PCIE0_S-19", true},
	{0, 128, 139, "PCIE0_S-20", true},
	{0, 129, 140, "PCIE0_S-21", true},

	/* 130 */
	{0, 130, 141, "PCIE0_S-22", true},
	{0, 131, 142, "PCIE0_S-23", true},
	{0, 132, 143, "PCIE0_S-24", true},
	{0, 133, 144, "PCIE0_S-25", true},
	{0, 134, 145, "PCIE0_S-26", true},
	{0, 135, 146, "PCIE1_S", true},
	{0, 136, 147, "PCIE1_S-1", true},
	{0, 137, 148, "PCIE1_S-2", true},
	{0, 138, 149, "PCIE1_S-3", true},
	{0, 139, 150, "PCIE1_S-4", true},

	/* 140 */
	{0, 140, 151, "PCIE1_S-5", true},
	{0, 141, 152, "PCIE1_S-6", true},
	{0, 142, 153, "PCIE1_S-7", true},
	{0, 143, 154, "PCIE1_S-8", true},
	{0, 144, 155, "PCIE1_S-9", true},
	{0, 145, 156, "PCIE1_S-10", true},
	{0, 146, 157, "PCIE1_S-11", true},
	{0, 147, 158, "PCIE1_S-12", true},
	{0, 148, 159, "PCIE1_S-13", true},
	{0, 149, 160, "PCIE1_S-14", true},

	/* 150 */
	{0, 150, 161, "PCIE1_S-15", true},
	{0, 151, 162, "PCIE1_S-16", true},
	{0, 152, 163, "PCIE1_S-17", true},
	{0, 153, 164, "PCIE1_S-18", true},
	{0, 154, 165, "PCIE1_S-19", true},
	{0, 155, 166, "PCIE1_S-20", true},
	{0, 156, 167, "PCIE1_S-21", true},
	{0, 157, 168, "PCIE1_S-22", true},
	{0, 158, 169, "USB0_S", true},
	{0, 159, 170, "USB0_S-1", true},

	/* 160 */
	{0, 160, 171, "USB0_S-2", true},
	{0, 161, 172, "USB0_S-3", true},
	{0, 162, 173, "USB0_S-4", true},
	{0, 163, 174, "USB0_S-5", true},
	{0, 164, 175, "USB0_S-6", true},
	{0, 165, 176, "USB0_S-7", true},
	{0, 166, 177, "USB0_S-8", true},
	{0, 167, 178, "USB0_S-9", true},
	{0, 168, 179, "USB0_S-10", true},
	{0, 169, 180, "USB0_S-11", true},

	/* 170 */
	{0, 170, 181, "USB0_S-12", true},
	{0, 171, 182, "USB0_S-13", true},
	{0, 172, 183, "USB0_S-14", true},
	{0, 173, 184, "USB0_S-15", true},
	{0, 174, 185, "USB0_S-16", true},
	{0, 175, 186, "USB0_S-17", true},
	{0, 176, 187, "USB0_S-18", true},
	{0, 177, 188, "USB0_S-19", true},
	{0, 178, 189, "USB0_S-20", true},
	{0, 179, 190, "USB0_S-21", true},

	/* 180 */
	{0, 180, 191, "USB0_S-22", true},
	{0, 181, 192, "USB0_S-23", true},
	{0, 182, 193, "USB0_S-24", true},
	{0, 183, 195, "NOR_AXI_S", true},
	{0, 184, 198, "MSDC1_S-1", true},
	{0, 185, 199, "MSDC1_S-2", true},
	{0, 186, 200, "MSDC2_S-1", true},
	{0, 187, 201, "MSDC2_S-2", true},
	{0, 188, 202, "PERI_MBIST_AO_APB_S", true},
	{0, 189, 203, "BCRM_PERI_PAR_AO_APB_S", true},

	/* 190 */
	{0, 190, 204, "PERICFG_SEC_AO_APB_S", true},
	{0, 191, 205, "PERICFG_AO_DUAL_APB_S", true},
	{0, 192, 206, "PERICFG_AO_APB_S", true},
	{0, 193, 207, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{0, 194, 208, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 195, 209, "PERI_AO_PBUS_S", true},

	{-1, -1, 210, "OOB_way_en", true},
	{-1, -1, 211, "OOB_way_en", true},
	{-1, -1, 212, "OOB_way_en", true},
	{-1, -1, 213, "OOB_way_en", true},

	/* 200 */
	{-1, -1, 214, "OOB_way_en", true},
	{-1, -1, 215, "OOB_way_en", true},
	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},
	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},
	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},

	/* 210 */
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},
	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "OOB_way_en", true},
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},

	/* 220 */
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},
	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},
	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},

	/* 230 */
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},
	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},
	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},

	/* 240 */
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},
	{-1, -1, 260, "OOB_way_en", true},
	{-1, -1, 261, "OOB_way_en", true},
	{-1, -1, 262, "OOB_way_en", true},
	{-1, -1, 263, "OOB_way_en", true},

	/* 250 */
	{-1, -1, 264, "OOB_way_en", true},
	{-1, -1, 265, "OOB_way_en", true},
	{-1, -1, 266, "OOB_way_en", true},
	{-1, -1, 267, "OOB_way_en", true},
	{-1, -1, 268, "OOB_way_en", true},

	{-1, -1, 269, "Decode_error", true},
	{-1, -1, 270, "Decode_error", true},
	{-1, -1, 271, "Decode_error", true},

	{-1, -1, 272, "IIC_P2P_REMAP", false},
	{-1, -1, 273, "APDMA", false},

	/* 260 */
	{-1, -1, 274, "PCIE0_SMPU", false},
	{-1, -1, 275, "PCIE1_SMPU", false},
	{-1, -1, 276, "DEVICE_APC_PERI _AO", false},
	{-1, -1, 277, "DEVICE_APC_PERI_PDN", false},
};

static const struct mtk_device_info mt6991_devices_vlp[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "INFRA_S0_S", true},
	{0, 1, 1, "INFRA_S0_S-1", true},
	{0, 2, 2, "INFRA_S0_S-2", true},
	{0, 3, 3, "SSPM_S", true},
	{0, 4, 4, "SSPM_S-1", true},
	{0, 5, 5, "SSPM_S-2", true},
	{0, 6, 6, "SSPM_S-3", true},
	{0, 7, 7, "SSPM_S-4", true},
	{0, 8, 8, "SSPM_S-5", true},
	{0, 9, 9, "SSPM_S-6", true},

	/* 10 */
	{0, 10, 10, "SSPM_S-7", true},
	{0, 11, 11, "SSPM_S-8", true},
	{0, 12, 12, "SSPM_S-9", true},
	{0, 13, 13, "SSPM_S-10", true},
	{0, 14, 14, "SSPM_S-11", true},
	{0, 15, 15, "SSPM_S-12", true},
	{0, 16, 16, "SSPM_S-13", true},
	{0, 17, 17, "SSPM_S-14", true},
	{0, 18, 18, "SSPM_S-15", true},
	{0, 19, 19, "SSPM_S-16", true},

	/* 20 */
	{0, 20, 20, "SSPM_S-17", true},
	{0, 21, 21, "SSPM_S-18", true},
	{0, 22, 22, "SCP_S", true},
	{0, 23, 23, "SCP_S-1", true},
	{0, 24, 24, "SCP_S-2", true},
	{0, 25, 25, "SCP_S-3", true},
	{0, 26, 26, "SCP_S-4", true},
	{0, 27, 27, "SCP_S-5", true},
	{0, 28, 28, "SCP_S-6", true},
	{0, 29, 29, "DVFSRC_APB_S", true},

	/* 30 */
	{0, 30, 30, "TIA_APB_S", true},
	{0, 31, 31, "IPS_APB_S", true},
	{0, 32, 32, "DBG_tracker", true},
	{0, 33, 33, "AVS_APB_S", true},
	{0, 34, 34, "BCRM_VLP_PDN_APB_S", true},
	{0, 35, 35, "MNOC_TX_APB_S", true},
	{0, 36, 36, "MNOC_RX_APB_S", true},
	{0, 37, 37, "TIA_SECURE_APB_S", true},
	{0, 38, 38, "SPM_APB_S", true},
	{0, 39, 39, "SPM_APB_S-1", true},

	/* 40 */
	{0, 40, 40, "SPM_APB_S-2", true},
	{0, 41, 41, "SPM_APB_S-3", true},
	{0, 42, 42, "SPM_APB_S-4", true},
	{0, 43, 43, "SPM_APB_S-5", true},
	{0, 44, 44, "SPM_APB_S-6", true},
	{0, 45, 45, "SPM_APB_S-7", true},
	{0, 46, 46, "SPM_APB_S-8", true},
	{0, 47, 47, "SPM_APB_S-9", true},
	{0, 48, 48, "SPM_APB_S-10", true},
	{0, 49, 49, "SPM_APB_S-11", true},

	/* 50 */
	{0, 50, 50, "SPM_APB_S-12", true},
	{0, 51, 51, "SPM_APB_S-13", true},
	{0, 52, 52, "SPM_APB_S-14", true},
	{0, 53, 53, "SYS_TIMER_APB_S", true},
	{0, 54, 54, "SYS_TIMER_APB_S-1", true},
	{0, 55, 55, "SYS_TIMER_APB_S-2", true},
	{0, 56, 56, "SYS_TIMER_APB_S-3", true},
	{0, 57, 57, "SYS_TIMER_APB_S-4", true},
	{0, 58, 58, "SYS_TIMER_APB_S-5", true},
	{0, 59, 59, "SYS_TIMER_APB_S-6", true},

	/* 60 */
	{0, 60, 60, "SYS_TIMER_APB_S-7", true},
	{0, 61, 61, "SYS_TIMER_APB_S-8", true},
	{0, 62, 62, "SYS_TIMER_APB_S-9", true},
	{0, 63, 63, "SYS_TIMER_APB_S-10", true},
	{0, 64, 64, "SYS_TIMER_APB_S-11", true},
	{0, 65, 65, "VLPCFG_AO_APB_S", true},
	{0, 66, 67, "SPM_LA_APB_S", true},
	{0, 67, 68, "SRCLKEN_RC_APB_S", true},
	{0, 68, 69, "TOPRGU_APB_S", true},
	{0, 69, 70, "APXGPT_APB_S", true},

	/* 70 */
	{0, 70, 71, "KP_APB_S", true},
	{0, 71, 72, "MBIST_APB_S", true},
	{0, 72, 73, "VLP_CKSYS_APB_S", true},
	{0, 73, 74, "PMIF1_APB_S", true},
	{0, 74, 75, "PMIF2_APB_S", true},
	{0, 75, 76, "DEVICE_APC_VLP_AO_APB_S", true},
	{0, 76, 77, "DEVICE_APC_VLP_APB_S", true},
	{0, 77, 78, "BCRM_VLP_AO_APB_S", true},
	{0, 78, 79, "DEBUG_CTRL_VLP_AO_APB_S", true},
	{0, 79, 80, "VLPCFG_APB_S", true},

	/* 80 */
	{0, 80, 81, "SPMI_M_MST_APB_S", true},
	{0, 81, 82, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 82, 83, "MD_BUCK_CTRL_SEQUENCER_APB_S", true},
	{0, 83, 84, "AP_CIRQ_EINT_APB_S", true},
	{0, 84, 85, "DPSW_CTRL_APB_S", true},
	{0, 85, 86, "DPSW_CENTRAL_CTRL_APB_S", true},
	{0, 86, 87, "DPMSR_APB_S", true},
	{0, 87, 88, "SUBSYS_PM_GP_APB_S", true},
	{0, 88, 89, "DEBUG_ERR_FLAG_APB_S", true},
	{0, 89, 90, "UARTHUB_WAKEUP_APB_S", true},

	/* 90 */
	{0, 90, 91, "VLP_PBUS_APB_S", true},
	{0, 91, 92, "VCORE_PBUS_APB_S", true},
	{0, 92, 94, "PWM_VLP_APB_S", true},
	{0, 93, 95, "TOPRGU_SECURE_APB_S", true},
	{0, 94, 96, "DPMSR_SECURE_APB_S", true},
	{0, 95, 97, "PMIF2_SECURE_APB_S", true},
	{0, 96, 98, "PMIF1_SECURE_APB_S", true},
	{0, 97, 99, "SPMI_M_MST_SECURE_APB_S", true},
	{0, 98, 100, "AP_CIRQ_EINT_SECURE_APB_S", true},

	{-1, -1, 101, "OOB_way_en", true},

	/* 100 */
	{-1, -1, 102, "OOB_way_en", true},
	{-1, -1, 103, "OOB_way_en", true},
	{-1, -1, 104, "OOB_way_en", true},
	{-1, -1, 105, "OOB_way_en", true},
	{-1, -1, 106, "OOB_way_en", true},
	{-1, -1, 107, "OOB_way_en", true},
	{-1, -1, 108, "OOB_way_en", true},
	{-1, -1, 109, "OOB_way_en", true},
	{-1, -1, 110, "OOB_way_en", true},
	{-1, -1, 111, "OOB_way_en", true},

	/* 120 */
	{-1, -1, 112, "OOB_way_en", true},
	{-1, -1, 113, "OOB_way_en", true},
	{-1, -1, 114, "OOB_way_en", true},
	{-1, -1, 115, "OOB_way_en", true},
	{-1, -1, 116, "OOB_way_en", true},
	{-1, -1, 117, "OOB_way_en", true},
	{-1, -1, 118, "OOB_way_en", true},
	{-1, -1, 119, "OOB_way_en", true},
	{-1, -1, 120, "OOB_way_en", true},
	{-1, -1, 121, "OOB_way_en", true},

	/* 130 */
	{-1, -1, 122, "OOB_way_en", true},
	{-1, -1, 123, "OOB_way_en", true},
	{-1, -1, 124, "OOB_way_en", true},
	{-1, -1, 125, "OOB_way_en", true},
	{-1, -1, 126, "OOB_way_en", true},
	{-1, -1, 127, "OOB_way_en", true},
	{-1, -1, 128, "OOB_way_en", true},
	{-1, -1, 129, "OOB_way_en", true},
	{-1, -1, 130, "OOB_way_en", true},
	{-1, -1, 131, "OOB_way_en", true},

	/* 140 */
	{-1, -1, 132, "OOB_way_en", true},
	{-1, -1, 133, "OOB_way_en", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},
	{-1, -1, 140, "OOB_way_en", true},
	{-1, -1, 141, "OOB_way_en", true},

	/* 150 */
	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},
	{-1, -1, 150, "OOB_way_en", true},

	{-1, -1, 151, "Decode_error", true},

	/* 160 */
	{-1, -1, 152, "Decode_error", true},

	{-1, -1, 153, "PMIF1", false},
	{-1, -1, 154, "PMIF2", false},
	{-1, -1, 155, "DEVICE_APC_VLP_AO", false},
	{-1, -1, 156, "DEVICE_APC_VLP_PDN", false},
};

static const struct mtk_device_info mt6991_devices_adsp[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "EMI_S", true},
	{0, 1, 1, "INFRA_S", true},
	{0, 2, 2, "INFRA_S-1", true},
	{0, 3, 3, "INFRA_S-2", true},
	{0, 4, 4, "DSP1_S", true},
	{0, 5, 5, "DSP1_S-1", true},
	{0, 6, 6, "DSP2_S", true},
	{0, 7, 7, "DSP2_S-1", true},
	{0, 8, 8, "F_SRAM_S", true},
	{0, 9, 9, "SCP_S", true},

	/* 10 */
	{0, 10, 10, "AUDIO_S", true},
	{0, 11, 14, "TinyXNNE_S", true},
	{0, 12, 15, "DBG_TRACKER_EMI_APB_S", true},
	{0, 13, 16, "DBG_TRACKER_EMI_APB_S-1", true},
	{0, 14, 17, "DBG_TRACKER_INFRA_APB_S", true},
	{0, 15, 18, "DBG_TRACKER_INFRA_APB_S-1", true},
	{0, 16, 19, "PBUS_S", true},
	{0, 17, 20, "EMI_APB_S", true},
	{0, 18, 21, "INFRA_APB_S", true},
	{0, 19, 22, "INFRA_RX_APB_S", true},

	/* 20 */
	{0, 20, 23, "PMSR_APB_S", true},
	{0, 21, 24, "DSPCFG_0_S", true},
	{0, 22, 25, "DSPCKCTL_S", true},
	{0, 23, 26, "DMA_0_CFG_S", true},
	{0, 24, 27, "DMA_1_CFG_S", true},
	{0, 25, 28, "DMA_AXI_CFG_S", true},
	{0, 26, 29, "DSPCFG_SEC_S", true},
	{0, 27, 30, "DSPMBOX_0_S", true},
	{0, 28, 31, "DSPMBOX_1_S", true},
	{0, 29, 32, "DSPMBOX_2_S", true},

	/* 30 */
	{0, 30, 33, "DSPMBOX_3_S", true},
	{0, 31, 34, "DSPMBOX_4_S", true},
	{0, 32, 35, "DSP_TIMER_0_S", true},
	{0, 33, 36, "DSP_TIMER_1_S", true},
	{0, 34, 37, "DSP_UART_0_S", true},
	{0, 35, 38, "DSP_UART_1_S", true},
	{0, 36, 39, "ADSP_BUSCFG_S", true},
	{0, 37, 40, "ADSP_TMBIST_S", true},
	{0, 38, 41, "ADSP_RSV_S", true},
	{0, 39, 42, "HRE_S", true},

	/* 40 */
	{0, 40, 43, "SYSCFG_AO_S", true},
	{0, 41, 44, "BUSMON_DRAM_S", true},
	{0, 42, 45, "BUSMON_INFRA_S", true},
	{0, 43, 48, "BUS_DEBUG_S", true},
	{0, 44, 49, "DAPC_S", true},
	{0, 45, 50, "K_BCRM_S", true},
	{0, 46, 51, "BCRM_S", true},
	{0, 47, 52, "DAPC_AO_S", true},
	{0, 48, 53, "SPI_S", true},

	{-1, -1, 54, "OOB_way_en", true},

	/* 50 */
	{-1, -1, 55, "OOB_way_en", true},
	{-1, -1, 56, "OOB_way_en", true},
	{-1, -1, 57, "OOB_way_en", true},
	{-1, -1, 58, "OOB_way_en", true},
	{-1, -1, 59, "OOB_way_en", true},
	{-1, -1, 60, "OOB_way_en", true},
	{-1, -1, 61, "OOB_way_en", true},
	{-1, -1, 62, "OOB_way_en", true},
	{-1, -1, 63, "OOB_way_en", true},
	{-1, -1, 64, "OOB_way_en", true},

	/* 60 */
	{-1, -1, 65, "OOB_way_en", true},
	{-1, -1, 66, "OOB_way_en", true},
	{-1, -1, 67, "OOB_way_en", true},
	{-1, -1, 68, "OOB_way_en", true},
	{-1, -1, 69, "OOB_way_en", true},
	{-1, -1, 70, "OOB_way_en", true},
	{-1, -1, 71, "OOB_way_en", true},
	{-1, -1, 72, "OOB_way_en", true},
	{-1, -1, 73, "OOB_way_en", true},
	{-1, -1, 74, "OOB_way_en", true},

	/* 70 */
	{-1, -1, 75, "OOB_way_en", true},
	{-1, -1, 76, "OOB_way_en", true},
	{-1, -1, 77, "OOB_way_en", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},
	{-1, -1, 80, "OOB_way_en", true},
	{-1, -1, 81, "OOB_way_en", true},
	{-1, -1, 82, "OOB_way_en", true},
	{-1, -1, 83, "OOB_way_en", true},
	{-1, -1, 84, "OOB_way_en", true},

	/* 80 */
	{-1, -1, 85, "OOB_way_en", true},
	{-1, -1, 86, "OOB_way_en", true},
	{-1, -1, 87, "OOB_way_en", true},
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},
	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "OOB_way_en", true},

	/* 90 */
	{-1, -1, 95, "OOB_way_en", true},
	{-1, -1, 96, "OOB_way_en", true},
	{-1, -1, 97, "OOB_way_en", true},
	{-1, -1, 98, "OOB_way_en", true},
	{-1, -1, 99, "OOB_way_en", true},
	{-1, -1, 100, "OOB_way_en", true},
	{-1, -1, 101, "OOB_way_en", true},
	{-1, -1, 102, "OOB_way_en", true},
	{-1, -1, 103, "OOB_way_en", true},

	{-1, -1, 104, "Decode_error", true},

	/* 100 */
	{-1, -1, 105, "Decode_error", true},
	{-1, -1, 106, "Decode_error", true},
	{-1, -1, 107, "Decode_error", true},
	{-1, -1, 108, "Decode_error", true},
	{-1, -1, 109, "Decode_error", true},
	{-1, -1, 110, "Decode_error", true},
	{-1, -1, 111, "Decode_error", true},

	{-1, -1, 112, "DEVICE_APC_AO_AUD_BUS_AO_PDN", false},
	{-1, -1, 113, "DEVICE_APC_AUD_BUS_AO_PDN", false},
};

static const struct mtk_device_info mt6991_devices_mminfra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "GCE_M_APB_S", true},
	{0, 1, 1, "GCE_M_APB_S-1", true},
	{0, 2, 2, "GCE_M_APB_S-2", true},
	{0, 3, 3, "GCE_M_APB_S-3", true},
	{0, 4, 4, "GCE_M_APB_S-4", true},
	{0, 5, 5, "GCE_M_APB_S-5", true},
	{0, 6, 6, "GCE_M_APB_S-6", true},
	{0, 7, 7, "GCE_M_APB_S-7", true},
	{0, 8, 8, "GCE_M_APB_S-8", true},
	{0, 9, 9, "GCE_M_APB_S-9", true},

	/* 10 */
	{0, 10, 10, "GCE_M_APB_S-10", true},
	{0, 11, 11, "GCE_M_APB_S-11", true},
	{0, 12, 12, "GCE_M_APB_S-12", true},
	{0, 13, 13, "GCE_M_APB_S-13", true},
	{0, 14, 14, "GCE_M_APB_S-14", true},
	{0, 15, 15, "GCE_M_APB_S-15", true},
	{0, 16, 16, "GCE_D_APB_S", true},
	{0, 17, 17, "GCE_D_APB_S-1", true},
	{0, 18, 18, "GCE_D_APB_S-2", true},
	{0, 19, 19, "GCE_D_APB_S-3", true},

	/* 20 */
	{0, 20, 20, "GCE_D_APB_S-4", true},
	{0, 21, 21, "GCE_D_APB_S-5", true},
	{0, 22, 22, "GCE_D_APB_S-6", true},
	{0, 23, 23, "GCE_D_APB_S-7", true},
	{0, 24, 24, "GCE_D_APB_S-8", true},
	{0, 25, 25, "GCE_D_APB_S-9", true},
	{0, 26, 26, "GCE_D_APB_S-10", true},
	{0, 27, 27, "GCE_D_APB_S-11", true},
	{0, 28, 28, "GCE_D_APB_S-12", true},
	{0, 29, 29, "GCE_D_APB_S-13", true},

	/* 30 */
	{0, 30, 30, "GCE_D_APB_S-14", true},
	{0, 31, 31, "GCE_D_APB_S-15", true},
	{0, 32, 32, "MMINFRA_APB_S", true},
	{0, 33, 33, "MMINFRA_APB_S-1", true},
	{0, 34, 34, "MMINFRA_APB_S-2", true},
	{0, 35, 35, "MMINFRA_APB_S-3", true},
	{0, 36, 36, "MMINFRA_APB_S-4", true},
	{0, 37, 37, "MMINFRA_APB_S-5", true},
	{0, 38, 38, "MMINFRA_APB_S-6", true},
	{0, 39, 39, "MMINFRA_APB_S-7", true},

	/* 40 */
	{0, 40, 40, "MMINFRA_APB_S-8", true},
	{0, 41, 41, "MMINFRA_APB_S-9", true},
	{0, 42, 42, "MMINFRA_APB_S-10", true},
	{0, 43, 43, "MMINFRA_APB_S-11", true},
	{0, 44, 44, "MMINFRA_APB_S-12", true},
	{0, 45, 45, "MMINFRA_APB_S-13", true},
	{0, 46, 46, "MMINFRA_APB_S-14", true},
	{0, 47, 47, "MMINFRA_APB_S-15", true},
	{0, 48, 48, "MMINFRA_APB_S-16", true},
	{0, 49, 49, "MMINFRA_APB_S-17", true},

	/* 50 */
	{0, 50, 50, "MMINFRA_APB_S-23", true},
	{0, 51, 51, "MMINFRA_AO_APB_S", true},
	{0, 52, 52, "MMINFRA_AO_APB_S-1", true},
	{0, 53, 53, "MMINFRA1_APB_S", true},
	{0, 54, 54, "MMINFRA1_APB_S-1", true},
	{0, 55, 55, "MMINFRA1_APB_S-2", true},
	{0, 56, 56, "MMINFRA1_APB_S-3", true},
	{0, 57, 57, "MMINFRA1_APB_S-4", true},
	{0, 58, 58, "MMINFRA1_APB_S-5", true},
	{0, 59, 59, "MMINFRA1_APB_S-6", true},

	/* 60 */
	{0, 60, 60, "MMINFRA1_APB_S-7", true},
	{0, 61, 61, "MMINFRA1_APB_S-8", true},
	{0, 62, 62, "MMINFRA1_APB_S-9", true},
	{0, 63, 63, "MMINFRA1_APB_S-10", true},
	{0, 64, 64, "MMINFRA1_APB_S-11", true},
	{0, 65, 65, "MMINFRA1_APB_S-12", true},
	{0, 66, 66, "MMINFRA1_APB_S-13", true},
	{0, 67, 67, "MMINFRA1_APB_S-14", true},
	{0, 68, 68, "MMINFRA1_APB_S-15", true},
	{0, 69, 69, "MMINFRA1_APB_S-16", true},

	/* 70 */
	{0, 70, 70, "MMINFRA1_APB_S-17", true},
	{0, 71, 71, "MMINFRA1_APB_S-18", true},
	{0, 72, 72, "MMINFRA1_APB_S-19", true},
	{0, 73, 73, "MMINFRA1_APB_S-20", true},
	{0, 74, 74, "MMINFRA1_APB_S-21", true},
	{0, 75, 75, "MMINFRA1_APB_S-22", true},
	{0, 76, 76, "MMINFRA1_APB_S-23", true},
	{0, 77, 77, "MMINFRA1_APB_S-24", true},
	{0, 78, 78, "MMINFRA1_APB_S-25", true},
	{0, 79, 79, "MMINFRA1_APB_S-26", true},

	/* 80 */
	{0, 80, 80, "MMINFRA1_APB_S-27", true},
	{0, 81, 81, "MMINFRA1_APB_S-28", true},
	{0, 82, 82, "MMINFRA1_APB_S-29", true},
	{0, 83, 83, "MMINFRA1_APB_S-30", true},
	{0, 84, 84, "MMINFRA1_APB_S-31", true},
	{0, 85, 85, "MMINFRA1_APB_S-32", true},
	{0, 86, 86, "MMINFRA1_APB_S-33", true},
	{0, 87, 87, "MMINFRA1_APB_S-34", true},
	{0, 88, 88, "MMINFRA1_APB_S-35", true},
	{0, 89, 89, "MMINFRA1_APB_S-36", true},

	/* 90 */
	{0, 90, 90, "MMINFRA1_APB_S-37", true},
	{0, 91, 91, "MMINFRA_SMMU_APB_S", true},
	{0, 92, 92, "MMINFRA_SMMU_APB_S-1", true},
	{0, 93, 93, "MMINFRA_SMMU_APB_S-2", true},
	{0, 94, 94, "DAPC_PDN_S", true},
	{0, 95, 95, "BCRM_PDN_S", true},
	{0, 96, 96, "VENC1_APB_S", true},
	{0, 97, 97, "VENC1_APB_S-1", true},
	{0, 98, 98, "VENC1_APB_S-2", true},
	{0, 99, 99, "VENC1_APB_S-3", true},

	/* 100 */
	{0, 100, 100, "VENC1_APB_S-4", true},
	{0, 101, 101, "VENC1_APB_S-5", true},
	{0, 102, 102, "VENC1_APB_S-6", true},
	{0, 103, 103, "VENC1_APB_S-7", true},
	{0, 104, 104, "VENC1_APB_S-8", true},
	{0, 105, 105, "VENC1_APB_S-9", true},
	{0, 106, 106, "VENC1_APB_S-10", true},
	{0, 107, 107, "VENC1_APB_S-11", true},
	{0, 108, 108, "VENC1_APB_S-12", true},
	{0, 109, 109, "VENC1_APB_S-13", true},

	/* 110 */
	{0, 110, 110, "VENC1_APB_S-14", true},
	{0, 111, 111, "VENC1_APB_S-15", true},
	{0, 112, 112, "VENC1_APB_S-16", true},
	{0, 113, 113, "VENC1_APB_S-17", true},
	{0, 114, 114, "VENC1_APB_S-18", true},
	{0, 115, 115, "VENC1_APB_S-19", false},
	{0, 116, 116, "VENC1_APB_S-20", true},
	{0, 117, 117, "VENC1_APB_S-21", true},
	{0, 118, 118, "VENC1_APB_S-22", true},
	{0, 119, 119, "VENC1_APB_S-23", true},

	/* 120 */
	{0, 120, 120, "VENC1_APB_S-24", true},
	{0, 121, 121, "VENC1_APB_S-25", true},
	{0, 122, 122, "VENC1_APB_S-26", true},
	{0, 123, 123, "VENC1_APB_S-27", true},
	{0, 124, 124, "VENC1_APB_S-28", true},
	{0, 125, 125, "VENC1_APB_S-29", true},
	{0, 126, 126, "VENC1_APB_S-30", true},
	{0, 127, 127, "VENC1_APB_S-31", true},
	{0, 128, 128, "VENC1_APB_S-32", true},
	{0, 129, 129, "VENC1_APB_S-33", true},

	/* 130 */
	{0, 130, 130, "VENC1_APB_S-34", true},
	{0, 131, 131, "VENC1_APB_S-35", true},
	{0, 132, 132, "VENC1_APB_S-36", true},
	{0, 133, 133, "VENC1_APB_S-37", true},
	{0, 134, 134, "VENC1_APB_S-38", true},
	{0, 135, 135, "VENC1_APB_S-39", true},
	{0, 136, 136, "VENC1_APB_S-40", true},
	{0, 137, 137, "VENC1_APB_S-41", true},
	{0, 138, 138, "VENC1_APB_S-42", true},
	{0, 139, 139, "VENC1_APB_S-43", true},

	/* 140 */
	{0, 140, 140, "VENC1_APB_S-44", true},
	{0, 141, 141, "VENC1_APB_S-45", true},
	{0, 142, 142, "VENC1_APB_S-46", true},
	{0, 143, 143, "VENC1_APB_S-47", true},
	{0, 144, 144, "VENC1_APB_S-48", true},
	{0, 145, 145, "VENC1_APB_S-49", true},
	{0, 146, 146, "VENC1_APB_S-50", true},
	{0, 147, 147, "VENC1_APB_S-51", true},
	{0, 148, 148, "VENC1_APB_S-52", true},
	{0, 149, 149, "VENC1_APB_S-53", true},

	/* 150 */
	{0, 150, 150, "VENC1_APB_S-54", true},
	{0, 151, 151, "VENC1_APB_S-55", true},
	{0, 152, 152, "VENC1_APB_S-56", true},
	{0, 153, 153, "VENC1_APB_S-57", true},
	{0, 154, 154, "VDEC_APB_S", true},
	{0, 155, 155, "VDEC_APB_S-1", true},
	{0, 156, 156, "VDEC_APB_S-2", true},
	{0, 157, 157, "VDEC_APB_S-3", true},
	{0, 158, 158, "VDEC_APB_S-4", true},
	{0, 159, 159, "VDEC_APB_S-5", true},

	/* 160 */
	{0, 160, 160, "VDEC_APB_S-6", true},
	{0, 161, 161, "VDEC_APB_S-7", true},
	{0, 162, 162, "VDEC_APB_S-8", true},
	{0, 163, 163, "VDEC_APB_S-9", true},
	{0, 164, 164, "VDEC_APB_S-10", true},
	{0, 165, 165, "VDEC_APB_S-11", true},
	{0, 166, 166, "VDEC_APB_S-12", true},
	{0, 167, 167, "VDEC_APB_S-13", true},
	{0, 168, 168, "VDEC_APB_S-14", true},
	{0, 169, 169, "VDEC_APB_S-15", true},

	/* 170 */
	{0, 170, 170, "VDEC_APB_S-16", true},
	{0, 171, 171, "VDEC_APB_S-17", true},
	{0, 172, 172, "VDEC_APB_S-18", true},
	{0, 173, 173, "VDEC_APB_S-19", false},
	{0, 174, 174, "VDEC_APB_S-20", true},
	{0, 175, 175, "VDEC_APB_S-21", true},
	{0, 176, 176, "CCU_APB_S", true},
	{0, 177, 177, "CCU_APB_S-1", true},
	{0, 178, 178, "CCU_APB_S-2", true},
	{0, 179, 179, "CCU_APB_S-3", true},

	/* 180 */
	{0, 180, 180, "CCU_APB_S-4", true},
	{0, 181, 181, "CCU_APB_S-5", true},
	{0, 182, 182, "CCU_APB_S-6", true},
	{0, 183, 183, "CCU_APB_S-7", true},
	{0, 184, 184, "CCU_APB_S-8", true},
	{0, 185, 185, "CCU_APB_S-9", true},
	{0, 186, 186, "CCU_APB_S-10", true},
	{0, 187, 187, "CCU_APB_S-11", true},
	{0, 188, 188, "CCU_APB_S-12", true},
	{0, 189, 189, "CCU_APB_S-13", true},

	/* 190 */
	{0, 190, 190, "CCU_APB_S-14", true},
	{0, 191, 191, "CCU_APB_S-15", true},
	{0, 192, 192, "CCU_APB_S-16", true},
	{0, 193, 193, "CCU_APB_S-17", true},
	{0, 194, 194, "CCU_APB_S-18", true},
	{0, 195, 195, "CCU_APB_S-19", true},
	{0, 196, 196, "CCU_APB_S-20", true},
	{0, 197, 197, "CCU_APB_S-21", true},
	{0, 198, 198, "CCU_APB_S-22", true},
	{0, 199, 199, "CCU_APB_S-23", true},

	/* 200 */
	{0, 200, 200, "CCU_APB_S-24", true},
	{0, 201, 201, "CCU_APB_S-25", true},
	{0, 202, 202, "CCU_APB_S-26", true},
	{0, 203, 203, "CCU_APB_S-27", true},
	{0, 204, 204, "CCU_APB_S-28", true},
	{0, 205, 205, "CCU_APB_S-29", true},
	{0, 206, 206, "CCU_APB_S-30", true},
	{0, 207, 207, "CCU_APB_S-31", true},
	{0, 208, 208, "CCU_APB_S-32", true},
	{0, 209, 209, "CCU_APB_S-33", true},

	/* 210 */
	{0, 210, 210, "CCU_APB_S-34", true},
	{0, 211, 211, "CCU_APB_S-35", true},
	{0, 212, 212, "CCU_APB_S-36", true},
	{0, 213, 213, "CCU_APB_S-37", true},
	{0, 214, 214, "CCU_APB_S-38", true},
	{0, 215, 215, "CCU_APB_S-39", true},
	{0, 216, 216, "CCU_APB_S-40", true},
	{0, 217, 217, "CCU_APB_S-41", true},
	{0, 218, 218, "CCU_APB_S-42", true},
	{0, 219, 219, "CCU_APB_S-43", true},

	/* 220 */
	{0, 220, 220, "CCU_APB_S-44", true},
	{0, 221, 221, "CCU_APB_S-45", true},
	{0, 222, 222, "CCU_APB_S-46", true},
	{0, 223, 223, "CCU_APB_S-47", true},
	{0, 224, 224, "IMG_APB_S", true},
	{0, 225, 225, "IMG_APB_S-1", true},
	{0, 226, 226, "IMG_APB_S-2", true},
	{0, 227, 227, "IMG_APB_S-3", true},
	{0, 228, 228, "IMG_APB_S-4", true},
	{0, 229, 229, "IMG_APB_S-5", true},

	/* 230 */
	{0, 230, 230, "IMG_APB_S-6", true},
	{0, 231, 231, "IMG_APB_S-7", true},
	{0, 232, 232, "IMG_APB_S-8", true},
	{0, 233, 233, "IMG_APB_S-9", true},
	{0, 234, 234, "IMG_APB_S-10", true},
	{0, 235, 235, "IMG_APB_S-11", true},
	{0, 236, 236, "IMG_APB_S-12", true},
	{0, 237, 237, "IMG_APB_S-13", true},
	{0, 238, 238, "IMG_APB_S-14", true},
	{0, 239, 239, "IMG_APB_S-15", true},

	/* 240 */
	{0, 240, 240, "IMG_APB_S-16", true},
	{0, 241, 241, "IMG_APB_S-17", true},
	{0, 242, 242, "IMG_APB_S-18", true},
	{0, 243, 243, "IMG_APB_S-19", true},
	{0, 244, 244, "IMG_APB_S-20", true},
	{0, 245, 245, "IMG_APB_S-21", true},
	{0, 246, 246, "IMG_APB_S-22", true},
	{0, 247, 247, "IMG_APB_S-23", true},
	{0, 248, 248, "IMG_APB_S-24", true},
	{0, 249, 249, "IMG_APB_S-25", true},

	/* 250 */
	{0, 250, 250, "IMG_APB_S-26", true},
	{0, 251, 251, "IMG_APB_S-27", true},
	{0, 252, 252, "IMG_APB_S-28", true},
	{0, 253, 253, "IMG_APB_S-29", true},
	{0, 254, 254, "IMG_APB_S-30", true},
	{0, 255, 255, "IMG_APB_S-31", true},
	{1, 0, 256, "IMG_APB_S-32", true},
	{1, 1, 257, "IMG_APB_S-33", true},
	{1, 2, 258, "IMG_APB_S-34", true},
	{1, 3, 259, "IMG_APB_S-35", true},

	/* 260 */
	{1, 4, 260, "IMG_APB_S-36", true},
	{1, 5, 261, "IMG_APB_S-37", true},
	{1, 6, 262, "IMG_APB_S-38", true},
	{1, 7, 263, "IMG_APB_S-39", true},
	{1, 8, 264, "IMG_APB_S-40", true},
	{1, 9, 265, "IMG_APB_S-41", true},
	{1, 10, 266, "IMG_APB_S-42", true},
	{1, 11, 267, "IMG_APB_S-43", true},
	{1, 12, 268, "IMG_APB_S-44", true},
	{1, 13, 269, "IMG_APB_S-45", true},

	/* 270 */
	{1, 14, 270, "IMG_APB_S-46", true},
	{1, 15, 271, "IMG_APB_S-47", true},
	{1, 16, 272, "IMG_APB_S-48", true},
	{1, 17, 273, "IMG_APB_S-49", true},
	{1, 18, 274, "IMG_APB_S-50", true},
	{1, 19, 275, "IMG_APB_S-51", true},
	{1, 20, 276, "IMG_APB_S-52", true},
	{1, 21, 277, "IMG_APB_S-53", true},
	{1, 22, 278, "IMG_APB_S-54", true},
	{1, 23, 279, "IMG_APB_S-55", true},

	/* 280 */
	{1, 24, 280, "IMG_APB_S-56", true},
	{1, 25, 281, "IMG_APB_S-57", true},
	{1, 26, 282, "IMG_APB_S-58", true},
	{1, 27, 283, "IMG_APB_S-59", true},
	{1, 28, 284, "IMG_APB_S-60", true},
	{1, 29, 285, "IMG_APB_S-61", true},
	{1, 30, 286, "IMG_APB_S-62", true},
	{1, 31, 287, "IMG_APB_S-63", true},
	{1, 32, 288, "IMG_APB_S-64", true},
	{1, 33, 289, "IMG_APB_S-65", true},

	/* 290 */
	{1, 34, 290, "IMG_APB_S-66", true},
	{1, 35, 291, "IMG_APB_S-67", true},
	{1, 36, 292, "IMG_APB_S-68", true},
	{1, 37, 293, "IMG_APB_S-69", true},
	{1, 38, 294, "IMG_APB_S-70", true},
	{1, 39, 295, "IMG_APB_S-71", true},
	{1, 40, 296, "IMG_APB_S-72", true},
	{1, 41, 297, "IMG_APB_S-73", true},
	{1, 42, 298, "IMG_APB_S-74", true},
	{1, 43, 299, "IMG_APB_S-75", true},

	/* 300 */
	{1, 44, 300, "IMG_APB_S-76", true},
	{1, 45, 301, "IMG_APB_S-77", true},
	{1, 46, 302, "IMG_APB_S-78", true},
	{1, 47, 303, "IMG_APB_S-79", true},
	{1, 48, 304, "CAM_APB_S", true},
	{1, 49, 305, "CAM_APB_S-1", true},
	{1, 50, 306, "CAM_APB_S-2", true},
	{1, 51, 307, "CAM_APB_S-3", true},
	{1, 52, 308, "CAM_APB_S-4", true},
	{1, 53, 309, "CAM_APB_S-5", true},

	/* 310 */
	{1, 54, 310, "CAM_APB_S-6", true},
	{1, 55, 311, "CAM_APB_S-7", true},
	{1, 56, 312, "CAM_APB_S-8", true},
	{1, 57, 313, "CAM_APB_S-9", true},
	{1, 58, 314, "CAM_APB_S-10", true},
	{1, 59, 315, "CAM_APB_S-11", true},
	{1, 60, 316, "CAM_APB_S-12", true},
	{1, 61, 317, "CAM_APB_S-13", true},
	{1, 62, 318, "CAM_APB_S-14", true},
	{1, 63, 319, "CAM_APB_S-15", true},

	/* 320 */
	{1, 64, 320, "CAM_APB_S-16", true},
	{1, 65, 321, "CAM_APB_S-17", true},
	{1, 66, 322, "CAM_APB_S-18", true},
	{1, 67, 323, "CAM_APB_S-19", true},
	{1, 68, 324, "CAM_APB_S-20", true},
	{1, 69, 325, "CAM_APB_S-21", true},
	{1, 70, 326, "CAM_APB_S-22", true},
	{1, 71, 327, "CAM_APB_S-23", true},
	{1, 72, 328, "CAM_APB_S-24", true},
	{1, 73, 329, "CAM_APB_S-25", true},

	/* 330 */
	{1, 74, 330, "CAM_APB_S-26", true},
	{1, 75, 331, "CAM_APB_S-27", true},
	{1, 76, 332, "CAM_APB_S-28", true},
	{1, 77, 333, "CAM_APB_S-29", true},
	{1, 78, 334, "CAM_APB_S-30", true},
	{1, 79, 335, "CAM_APB_S-31", true},
	{1, 80, 336, "CAM_APB_S-32", true},
	{1, 81, 337, "CAM_APB_S-33", true},
	{1, 82, 338, "CAM_APB_S-34", true},
	{1, 83, 339, "CAM_APB_S-35", true},

	/* 340 */
	{1, 84, 340, "CAM_APB_S-36", true},
	{1, 85, 341, "CAM_APB_S-37", true},
	{1, 86, 342, "CAM_APB_S-38", true},
	{1, 87, 343, "CAM_APB_S-39", true},
	{1, 88, 344, "CAM_APB_S-40", true},
	{1, 89, 345, "CAM_APB_S-41", true},
	{1, 90, 346, "CAM_APB_S-42", true},
	{1, 91, 347, "CAM_APB_S-43", true},
	{1, 92, 348, "CAM_APB_S-44", true},
	{1, 93, 349, "CAM_APB_S-45", true},

	/* 350 */
	{1, 94, 350, "CAM_APB_S-46", true},
	{1, 95, 351, "CAM_APB_S-47", true},
	{1, 96, 352, "CAM_APB_S-48", true},
	{1, 97, 353, "CAM_APB_S-49", true},
	{1, 98, 354, "CAM_APB_S-50", true},
	{1, 99, 355, "CAM_APB_S-51", true},
	{1, 100, 356, "CAM_APB_S-52", true},
	{1, 101, 357, "CAM_APB_S-53", true},
	{1, 102, 358, "CAM_APB_S-54", true},
	{1, 103, 359, "CAM_APB_S-55", true},

	/* 360 */
	{1, 104, 360, "CAM_APB_S-56", true},
	{1, 105, 361, "CAM_APB_S-57", true},
	{1, 106, 362, "CAM_APB_S-58", true},
	{1, 107, 363, "CAM_APB_S-59", true},
	{1, 108, 364, "CAM_APB_S-60", true},
	{1, 109, 365, "CAM_APB_S-61", true},
	{1, 110, 366, "CAM_APB_S-62", true},
	{1, 111, 367, "CAM_APB_S-63", true},
	{1, 112, 368, "CAM_APB_S-64", true},
	{1, 113, 369, "CAM_APB_S-65", true},

	/* 370 */
	{1, 114, 370, "CAM_APB_S-66", true},
	{1, 115, 371, "CAM_APB_S-67", true},
	{1, 116, 372, "CAM_APB_S-68", true},
	{1, 117, 373, "CAM_APB_S-69", true},
	{1, 118, 374, "CAM_APB_S-70", true},
	{1, 119, 375, "CAM_APB_S-71", true},
	{1, 120, 376, "CAM_APB_S-72", true},
	{1, 121, 377, "CAM_APB_S-73", true},
	{1, 122, 378, "CAM_APB_S-74", true},
	{1, 123, 379, "CAM_APB_S-75", true},

	/* 380 */
	{1, 124, 380, "CAM_APB_S-76", true},
	{1, 125, 381, "CAM_APB_S-77", true},
	{1, 126, 382, "CAM_APB_S-78", true},
	{1, 127, 383, "CAM_APB_S-79", true},
	{1, 128, 384, "CAM_APB_S-80", true},
	{1, 129, 385, "CAM_APB_S-81", true},
	{1, 130, 386, "CAM_APB_S-82", true},
	{1, 131, 387, "CAM_APB_S-83", true},
	{1, 132, 388, "CAM_APB_S-84", true},
	{1, 133, 389, "CAM_APB_S-85", true},

	/* 390 */
	{1, 134, 390, "CAM_APB_S-86", true},
	{1, 135, 391, "CAM_APB_S-87", true},
	{1, 136, 392, "CAM_APB_S-88", true},
	{1, 137, 393, "CAM_APB_S-89", true},
	{1, 138, 394, "CAM_APB_S-90", true},
	{1, 139, 395, "CAM_APB_S-91", true},
	{1, 140, 396, "CAM_APB_S-92", true},
	{1, 141, 397, "CAM_APB_S-93", true},
	{1, 142, 398, "CAM_APB_S-94", true},
	{1, 143, 399, "CAM_APB_S-95", true},

	/* 400 */
	{1, 144, 400, "CAM_APB_S-96", true},
	{1, 145, 401, "CAM_APB_S-97", true},
	{1, 146, 402, "CAM_APB_S-98", true},
	{1, 147, 403, "CAM_APB_S-99", true},
	{1, 148, 404, "CAM_APB_S-100", true},
	{1, 149, 405, "CAM_APB_S-101", true},
	{1, 150, 406, "CAM_APB_S-102", true},
	{1, 151, 407, "CAM_APB_S-103", true},
	{1, 152, 408, "CAM_APB_S-104", true},
	{1, 153, 409, "CAM_APB_S-105", true},

	/* 410 */
	{1, 154, 410, "CAM_APB_S-106", true},
	{1, 155, 411, "CAM_APB_S-107", true},
	{1, 156, 412, "CAM_APB_S-108", true},
	{1, 157, 413, "CAM_APB_S-109", true},
	{1, 158, 414, "CAM_APB_S-110", true},
	{1, 159, 415, "CAM_APB_S-111", true},
	{1, 160, 416, "CAM_APB_S-112", true},
	{1, 161, 417, "CAM_APB_S-113", true},
	{1, 162, 418, "CAM_APB_S-114", true},
	{1, 163, 419, "CAM_APB_S-115", true},

	/* 420 */
	{1, 164, 420, "CAM_APB_S-116", true},
	{1, 165, 421, "CAM_APB_S-117", true},
	{1, 166, 422, "CAM_APB_S-118", true},
	{1, 167, 423, "CAM_APB_S-119", true},
	{1, 168, 424, "CAM_APB_S-120", true},
	{1, 169, 425, "CAM_APB_S-121", true},
	{1, 170, 426, "CAM_APB_S-122", true},
	{1, 171, 427, "CAM_APB_S-123", true},
	{1, 172, 428, "CAM_APB_S-124", true},
	{1, 173, 429, "CAM_APB_S-125", true},

	/* 430 */
	{1, 174, 430, "CAM_APB_S-126", true},
	{1, 175, 431, "CAM_APB_S-127", true},
	{1, 176, 432, "DISP_APB_S", true},
	{1, 177, 433, "DISP_APB_S-1", true},
	{1, 178, 434, "DISP_APB_S-2", true},
	{1, 179, 435, "DISP_APB_S-3", true},
	{1, 180, 436, "DISP_APB_S-4", true},
	{1, 181, 437, "DISP_APB_S-5", true},
	{1, 182, 438, "DISP_APB_S-6", true},
	{1, 183, 439, "DISP_APB_S-7", true},

	/* 440 */
	{1, 184, 440, "DISP_APB_S-8", true},
	{1, 185, 441, "DISP_APB_S-9", true},
	{1, 186, 442, "DISP_APB_S-10", true},
	{1, 187, 443, "DISP_APB_S-11", true},
	{1, 188, 444, "DISP_APB_S-12", true},
	{1, 189, 445, "DISP_APB_S-13", true},
	{1, 190, 446, "DISP_APB_S-14", true},
	{1, 191, 447, "DISP_APB_S-15", true},
	{1, 192, 448, "DISP_APB_S-16", true},
	{1, 193, 449, "DISP_APB_S-17", true},

	/* 450 */
	{1, 194, 450, "DISP_APB_S-18", true},
	{1, 195, 451, "DISP_APB_S-19", true},
	{1, 196, 452, "DISP_APB_S-20", true},
	{1, 197, 453, "DISP_APB_S-21", true},
	{1, 198, 454, "DISP_APB_S-22", true},
	{1, 199, 455, "DISP_APB_S-23", true},
	{1, 200, 456, "DISP_APB_S-24", true},
	{1, 201, 457, "DISP_APB_S-25", true},
	{1, 202, 458, "DISP_APB_S-26", true},
	{1, 203, 459, "DISP_APB_S-27", true},

	/* 460 */
	{1, 204, 460, "DISP_APB_S-28", true},
	{1, 205, 461, "DISP_APB_S-29", true},
	{1, 206, 462, "DISP_APB_S-30", true},
	{1, 207, 463, "DISP_APB_S-31", true},
	{1, 208, 464, "DISP_APB_S-32", true},
	{1, 209, 465, "DISP_APB_S-33", true},
	{1, 210, 466, "DISP_APB_S-34", true},
	{1, 211, 467, "DISP_APB_S-35", true},
	{1, 212, 468, "DISP_APB_S-36", true},
	{1, 213, 469, "DISP_APB_S-37", true},

	/* 470 */
	{1, 214, 470, "DISP_APB_S-38", true},
	{1, 215, 471, "DISP_APB_S-39", true},
	{1, 216, 472, "DISP_APB_S-40", true},
	{1, 217, 473, "DISP_APB_S-41", true},
	{1, 218, 474, "DISP_APB_S-42", true},
	{1, 219, 475, "DISP_APB_S-43", true},
	{1, 220, 476, "DISP_APB_S-44", true},
	{1, 221, 477, "DISP_APB_S-45", true},
	{1, 222, 478, "DISP_APB_S-46", true},
	{1, 223, 479, "DISP_APB_S-47", true},

	/* 480 */
	{1, 224, 480, "DISP_APB_S-48", true},
	{1, 225, 481, "DISP_APB_S-49", true},
	{1, 226, 482, "DISP_APB_S-50", true},
	{1, 227, 483, "DISP_APB_S-51", true},
	{1, 228, 484, "DISP_APB_S-52", true},
	{1, 229, 485, "DISP_APB_S-53", true},
	{1, 230, 486, "DISP_APB_S-54", true},
	{1, 231, 487, "DISP_APB_S-55", true},
	{1, 232, 488, "DISP_APB_S-56", true},
	{1, 233, 489, "DISP_APB_S-57", true},

	/* 490 */
	{1, 234, 490, "DISP_APB_S-58", true},
	{1, 235, 491, "DISP_APB_S-59", true},
	{1, 236, 492, "DISP_APB_S-60", true},
	{1, 237, 493, "DISP_APB_S-61", true},
	{1, 238, 494, "DISP_APB_S-62", true},
	{1, 239, 495, "DISP_APB_S-63", true},
	{1, 240, 496, "DISP_APB_S-64", true},
	{1, 241, 497, "DISP_APB_S-65", true},
	{1, 242, 498, "DISP_APB_S-66", true},
	{1, 243, 499, "DISP_APB_S-67", true},

	/* 500 */
	{1, 244, 500, "DISP_APB_S-68", true},
	{1, 245, 501, "DISP_APB_S-69", true},
	{1, 246, 502, "DISP_APB_S-70", true},
	{1, 247, 503, "DISP_APB_S-71", true},
	{1, 248, 504, "DISP_APB_S-72", true},
	{1, 249, 505, "DISP_APB_S-73", true},
	{1, 250, 506, "DISP_APB_S-74", true},
	{1, 251, 507, "DISP_APB_S-75", true},
	{1, 252, 508, "DISP_APB_S-76", true},
	{1, 253, 509, "DISP_APB_S-77", true},

	/* 510 */
	{1, 254, 510, "DISP_APB_S-78", true},
	{1, 255, 511, "DISP_APB_S-79", true},
	{2, 0, 512, "DISP_APB_S-80", true},
	{2, 1, 513, "DISP_APB_S-81", true},
	{2, 2, 514, "DISP_APB_S-82", true},
	{2, 3, 515, "DISP_APB_S-83", true},
	{2, 4, 516, "DISP_APB_S-84", true},
	{2, 5, 517, "DISP_APB_S-85", true},
	{2, 6, 518, "DISP_APB_S-86", true},
	{2, 7, 519, "DISP_APB_S-87", true},

	/* 520 */
	{2, 8, 520, "DISP_APB_S-88", true},
	{2, 9, 521, "DISP_APB_S-89", true},
	{2, 10, 522, "DISP_APB_S-90", true},
	{2, 11, 523, "DISP_APB_S-91", true},
	{2, 12, 524, "DISP_APB_S-92", true},
	{2, 13, 525, "DISP_APB_S-93", true},
	{2, 14, 526, "DISP_APB_S-94", true},
	{2, 15, 527, "DISP_APB_S-95", true},
	{2, 16, 528, "DISP_APB_S-96", true},
	{2, 17, 529, "DISP_APB_S-97", true},

	/* 530 */
	{2, 18, 530, "DISP_APB_S-98", true},
	{2, 19, 531, "DISP_APB_S-99", true},
	{2, 20, 532, "DISP_APB_S-100", true},
	{2, 21, 533, "DISP_APB_S-101", true},
	{2, 22, 534, "DISP_APB_S-102", true},
	{2, 23, 535, "DISP_APB_S-103", true},
	{2, 24, 536, "DISP_APB_S-104", true},
	{2, 25, 537, "DISP_APB_S-105", true},
	{2, 26, 538, "DISP_APB_S-106", true},
	{2, 27, 539, "DISP_APB_S-107", true},

	/* 540 */
	{2, 28, 540, "DISP_APB_S-108", true},
	{2, 29, 541, "DISP_APB_S-109", true},
	{2, 30, 542, "DISP_APB_S-110", true},
	{2, 31, 543, "DISP_APB_S-111", true},
	{2, 32, 544, "DISP_APB_S-112", true},
	{2, 33, 545, "DISP_APB_S-113", true},
	{2, 34, 546, "DISP_APB_S-114", true},
	{2, 35, 547, "DISP_APB_S-115", true},
	{2, 36, 548, "DISP_APB_S-116", true},
	{2, 37, 549, "DISP_APB_S-117", true},

	/* 550 */
	{2, 38, 550, "DISP_APB_S-118", true},
	{2, 39, 551, "DISP_APB_S-119", true},
	{2, 40, 552, "DISP_APB_S-120", true},
	{2, 41, 553, "DISP_APB_S-121", true},
	{2, 42, 554, "DISP_APB_S-122", true},
	{2, 43, 555, "DISP_APB_S-123", true},
	{2, 44, 556, "DISP_APB_S-124", true},
	{2, 45, 557, "DISP_APB_S-125", true},
	{2, 46, 558, "DISP_APB_S-126", true},
	{2, 47, 559, "DISP_APB_S-127", true},

	/* 560 */
	{2, 48, 560, "DISP_APB_S-128", true},
	{2, 49, 561, "DISP_APB_S-129", true},
	{2, 50, 562, "DISP_APB_S-130", true},
	{2, 51, 563, "DISP_APB_S-131", true},
	{2, 52, 564, "DISP_APB_S-132", true},
	{2, 53, 565, "DISP_APB_S-133", true},
	{2, 54, 566, "DISP_APB_S-134", true},
	{2, 55, 567, "DISP_APB_S-135", true},
	{2, 56, 568, "DISP_APB_S-136", true},
	{2, 57, 569, "DISP_APB_S-137", true},

	/* 570 */
	{2, 58, 570, "DISP_APB_S-138", true},
	{2, 59, 571, "DISP_APB_S-139", true},
	{2, 60, 572, "DISP_APB_S-140", true},
	{2, 61, 573, "DISP_APB_S-141", true},
	{2, 62, 574, "DISP_APB_S-142", true},
	{2, 63, 575, "DISP_APB_S-143", true},
	{2, 64, 576, "DISP_APB_S-144", true},
	{2, 65, 577, "DISP_APB_S-145", true},
	{2, 66, 578, "DISP_APB_S-146", true},
	{2, 67, 579, "DISP_APB_S-147", true},

	/* 580 */
	{2, 68, 580, "DISP_APB_S-148", true},
	{2, 69, 581, "DISP_APB_S-149", true},
	{2, 70, 582, "DISP_APB_S-150", true},
	{2, 71, 583, "DISP_APB_S-151", true},
	{2, 72, 584, "DISP_APB_S-152", true},
	{2, 73, 585, "DISP_APB_S-153", true},
	{2, 74, 586, "DISP_APB_S-154", true},
	{2, 75, 587, "DISP_APB_S-155", true},
	{2, 76, 588, "DISP_APB_S-156", true},
	{2, 77, 589, "DISP_APB_S-157", true},

	/* 590 */
	{2, 78, 590, "DISP_APB_S-158", true},
	{2, 79, 591, "DISP_APB_S-159", true},
	{2, 80, 592, "DISP_APB_S-160", true},
	{2, 81, 593, "DISP_APB_S-161", true},
	{2, 82, 594, "DISP_APB_S-162", true},
	{2, 83, 595, "DISP_APB_S-163", true},
	{2, 84, 596, "DISP_APB_S-164", true},
	{2, 85, 597, "DISP_APB_S-165", true},
	{2, 86, 598, "DISP_APB_S-166", true},
	{2, 87, 599, "DISP_APB_S-167", true},

	/* 600 */
	{2, 88, 600, "DISP_APB_S-168", true},
	{2, 89, 601, "DISP_APB_S-169", true},
	{2, 90, 602, "DISP_APB_S-170", true},
	{2, 91, 603, "DISP_APB_S-171", true},
	{2, 92, 604, "DISP_APB_S-172", true},
	{2, 93, 605, "DISP_APB_S-173", true},
	{2, 94, 606, "DISP_APB_S-174", true},
	{2, 95, 607, "DISP_APB_S-175", true},
	{2, 96, 608, "DISP_APB_S-176", true},
	{2, 97, 609, "DISP_APB_S-177", true},

	/* 610 */
	{2, 98, 610, "DISP_APB_S-178", true},
	{2, 99, 611, "DISP_APB_S-179", true},
	{2, 100, 612, "DISP_APB_S-180", true},
	{2, 101, 613, "DISP_APB_S-181", true},
	{2, 102, 614, "DISP_APB_S-182", true},
	{2, 103, 615, "DISP_APB_S-183", true},
	{2, 104, 616, "DISP_APB_S-184", true},
	{2, 105, 617, "DISP_APB_S-185", true},
	{2, 106, 618, "DISP_APB_S-186", true},
	{2, 107, 619, "DISP_APB_S-187", true},

	/* 620 */
	{2, 108, 620, "DISP_APB_S-188", true},
	{2, 109, 621, "DISP_APB_S-189", true},
	{2, 110, 622, "DISP_APB_S-190", true},
	{2, 111, 623, "DISP_APB_S-191", true},
	{2, 112, 624, "DISP_APB_S-192", true},
	{2, 113, 625, "DISP_APB_S-193", true},
	{2, 114, 626, "DISP_APB_S-194", true},
	{2, 115, 627, "DISP_APB_S-195", true},
	{2, 116, 628, "DISP_APB_S-196", true},
	{2, 117, 629, "DISP_APB_S-197", true},

	/* 630 */
	{2, 118, 630, "DISP_APB_S-198", true},
	{2, 119, 631, "DISP_APB_S-199", true},
	{2, 120, 632, "DISP_APB_S-200", true},
	{2, 121, 633, "DISP_APB_S-201", true},
	{2, 122, 634, "DISP_APB_S-202", true},
	{2, 123, 635, "DISP_APB_S-203", true},
	{2, 124, 636, "DISP_APB_S-204", true},
	{2, 125, 637, "DISP_APB_S-205", true},
	{2, 126, 638, "DISP_APB_S-206", true},
	{2, 127, 639, "DISP_APB_S-207", true},

	/* 640 */
	{2, 128, 640, "DISP_APB_S-208", true},
	{2, 129, 641, "DISP_APB_S-209", true},
	{2, 130, 642, "DISP_APB_S-210", true},
	{2, 131, 643, "DISP_APB_S-211", true},
	{2, 132, 644, "DISP_APB_S-212", true},
	{2, 133, 645, "DISP_APB_S-213", true},
	{2, 134, 646, "DISP_APB_S-214", true},
	{2, 135, 647, "DISP_APB_S-215", true},
	{2, 136, 648, "DISP_APB_S-216", true},
	{2, 137, 649, "DISP_APB_S-217", true},

	/* 650 */
	{2, 138, 650, "DISP_APB_S-218", true},
	{2, 139, 651, "DISP_APB_S-219", true},
	{2, 140, 652, "DISP_APB_S-220", true},
	{2, 141, 653, "DISP_APB_S-221", true},
	{2, 142, 654, "DISP_APB_S-222", true},
	{2, 143, 655, "DISP_APB_S-223", true},
	{2, 144, 656, "DISP_APB_S-224", true},
	{2, 145, 657, "DISP_APB_S-225", true},
	{2, 146, 658, "DISP_APB_S-226", true},
	{2, 147, 659, "DISP_APB_S-227", true},

	/* 660 */
	{2, 148, 660, "DISP_APB_S-228", true},
	{2, 149, 661, "DISP_APB_S-229", true},
	{2, 150, 662, "DISP_APB_S-230", true},
	{2, 151, 663, "DISP_APB_S-231", true},
	{2, 152, 664, "DISP_APB_S-232", true},
	{2, 153, 665, "DISP_APB_S-233", true},
	{2, 154, 666, "DISP_APB_S-234", true},
	{2, 155, 667, "DISP_APB_S-235", true},
	{2, 156, 668, "DISP_APB_S-236", true},
	{2, 157, 669, "DISP_APB_S-237", true},

	/* 670 */
	{2, 158, 670, "DISP_APB_S-238", true},
	{2, 159, 671, "DISP_APB_S-239", true},
	{2, 160, 672, "DISP_APB_S-240", true},
	{2, 161, 673, "DISP_APB_S-241", true},
	{2, 162, 674, "DISP_APB_S-242", true},
	{2, 163, 675, "DISP_APB_S-243", true},
	{2, 164, 676, "DISP_APB_S-244", true},
	{2, 165, 677, "DISP_APB_S-245", true},
	{2, 166, 678, "DISP_APB_S-246", true},
	{2, 167, 679, "DISP_APB_S-247", true},

	/* 680 */
	{2, 168, 680, "DISP_APB_S-248", true},
	{2, 169, 681, "DISP_APB_S-249", true},
	{2, 170, 682, "DISP_APB_S-250", true},
	{2, 171, 683, "DISP_APB_S-251", true},
	{2, 172, 684, "DISP_APB_S-252", true},
	{2, 173, 685, "DISP_APB_S-253", true},
	{2, 174, 686, "DISP_APB_S-254", true},
	{2, 175, 687, "DISP_APB_S-255", true},
	{2, 176, 689, "HRE_APB_S", true},
	{2, 177, 690, "HRE2_APB_S", true},

	/* 690 */
	{2, 178, 692, "HFRP_APB_S", true},
	{2, 179, 696, "DAPC_AO_S", true},
	{2, 180, 697, "BCRM_AO_S", true},
	{2, 181, 698, "DEBUG_CTL_AO_S", true},
	{-1, -1, 699, "OOB_way_en", true},
	{-1, -1, 700, "OOB_way_en", true},
	{-1, -1, 701, "OOB_way_en", true},
	{-1, -1, 702, "OOB_way_en", true},
	{-1, -1, 703, "OOB_way_en", true},
	{-1, -1, 704, "OOB_way_en", true},

	/* 700 */
	{-1, -1, 705, "OOB_way_en", true},
	{-1, -1, 706, "OOB_way_en", true},
	{-1, -1, 707, "OOB_way_en", true},
	{-1, -1, 708, "OOB_way_en", true},
	{-1, -1, 709, "OOB_way_en", true},
	{-1, -1, 710, "OOB_way_en", true},
	{-1, -1, 711, "OOB_way_en", true},
	{-1, -1, 712, "OOB_way_en", true},
	{-1, -1, 713, "OOB_way_en", true},
	{-1, -1, 714, "OOB_way_en", true},

	{-1, -1, 715, "Decode_error", true},
	{-1, -1, 716, "Decode_error", true},
	{-1, -1, 717, "Decode_error", true},
	{-1, -1, 718, "Decode_error", true},
	{-1, -1, 719, "GCE_D", false},
	{-1, -1, 720, "GCE_E", false},
	{-1, -1, 721, "DEVICE_APC_MM_PDN", false},
	{-1, -1, 722, "DEVICE_APC_MM_AO", false},
};

static const struct mtk_device_info mt6991_devices_mmup[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "p_main_BCRM", true},
	{0, 1, 1, "p_main_DEBUG", true},
	{0, 2, 2, "p_main_DEVAPCAO", true},
	{0, 3, 3, "p_main_DEVAPC", true},
	{0, 4, 4, "p_main_BCRM_DRAM", true},
	{0, 5, 5, "p_par_top", true},
	{0, 6, 6, "pslv_clk_ctrl", true},
	{0, 7, 7, "pslv_cfgreg", true},
	{0, 8, 8, "pslv_uart", true},
	{0, 9, 9, "pslv_uart1", true},

	/* 10 */
	{0, 10, 10, "pslv_cfg_core0", true},
	{0, 11, 11, "pslv_dma_core0", true},
	{0, 12, 12, "pslv_irq_core0", true},
	{0, 13, 13, "pslv_tmr_core0", true},
	{0, 14, 14, "pslv_dbg_core0", true},
	{0, 15, 15, "pslv_dbg_core0_l2", true},
	{0, 16, 16, "pslv_dbg_core0_l2_sram", true},
	{0, 17, 17, "pslv_cfg_core1", true},
	{0, 18, 18, "pslv_irq_core1", true},
	{0, 19, 19, "pslv_tmr_core1", true},

	/* 20 */
	{0, 20, 20, "pslv_dbg_core1", true},
	{0, 21, 21, "pCACHE0", true},
	{0, 22, 22, "pCACHE1", true},
	{0, 23, 23, "pslv_cfgreg_sec", true},
	{0, 24, 24, "pslv_rsv0", true},
	{0, 25, 25, "pslv_rsv0_1", true},
	{0, 26, 26, "pslv_rsv0_2", true},
	{0, 27, 27, "pslv_rsv0_3", true},
	{0, 28, 28, "pslv_rsv1", true},
	{0, 29, 29, "pslv_rsv2", true},

	/* 30 */
	{0, 30, 30, "pslv_rsv3", true},
	{0, 31, 31, "pslv_rv_l2_base", true},
	{0, 32, 32, "pslv_dvfsrc_vmm", true},
	{0, 33, 33, "pslv_dvfsrc_vdisp", true},
	{0, 34, 34, "pslv_dpsw_vdec", true},
	{0, 35, 35, "pslv_dpsw_vdisp", true},
	{0, 36, 36, "pslv_hfrp_pwr_ctrl", true},
	{0, 37, 37, "pslv_sub_pm", true},
	{0, 38, 38, "pslv_sub_pm_reg", true},
	{0, 39, 39, "pslv_hw_vote", true},

	/* 40 */
	{0, 40, 40, "HWCCF_S", true},
	{0, 41, 41, "HWCCF_S1", true},
	{0, 42, 42, "HWCCF_S2", true},
	{0, 43, 43, "HWCCF_S3", true},
	{0, 44, 44, "HWCCF_S4", true},
	{0, 45, 45, "HWCCF_S5", true},
	{0, 46, 46, "HWCCF_S6", true},
	{0, 47, 47, "HWCCF_S7", true},
	{0, 48, 48, "HWCCF_S8", true},
	{0, 49, 49, "HWCCF_S9", true},

	/* 50 */
	{0, 50, 50, "HWCCF_S10", true},
	{0, 51, 51, "HWCCF_S11", true},
	{0, 52, 52, "HWCCF_S12", true},
	{0, 53, 53, "HWCCF_S13", true},
	{0, 54, 54, "HWCCF_S14", true},
	{0, 55, 55, "HWCCF_S15", true},
	{0, 56, 56, "HWCCF_S16", true},
	{0, 57, 57, "HWCCF_S17", true},
	{0, 58, 58, "HWCCF_S18", true},
	{0, 59, 59, "HWCCF_S19", true},

	/* 60 */
	{0, 60, 60, "pslv_mm_mtcmos", true},
	{0, 61, 61, "pslv_mm_cfgreg", true},
	{0, 62, 62, "pslv_pbus", true},
	{0, 63, 63, "p_mbox0", true},
	{0, 64, 64, "p_mbox1", true},
	{0, 65, 65, "p_mbox2", true},
	{0, 66, 66, "p_mbox3", true},
	{0, 67, 67, "p_mbox4", true},
	{0, 68, 68, "pslv_cfg_ap", true},
	{0, 69, 69, "pbus_tracker", true},

	/* 70 */
	{0, 70, 74, "p_main_BCRM_PDN", true},

	{-1, -1, 75, "OOB_way_en", true},
	{-1, -1, 76, "OOB_way_en", true},
	{-1, -1, 77, "OOB_way_en", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},

	/* 80 */
	{-1, -1, 80, "OOB_way_en", true},
	{-1, -1, 81, "OOB_way_en", true},
	{-1, -1, 82, "OOB_way_en", true},
	{-1, -1, 83, "OOB_way_en", true},
	{-1, -1, 84, "OOB_way_en", true},
	{-1, -1, 85, "OOB_way_en", true},
	{-1, -1, 86, "OOB_way_en", true},
	{-1, -1, 87, "OOB_way_en", true},
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},

	/* 90 */
	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "OOB_way_en", true},
	{-1, -1, 95, "OOB_way_en", true},
	{-1, -1, 96, "OOB_way_en", true},
	{-1, -1, 97, "OOB_way_en", true},
	{-1, -1, 98, "OOB_way_en", true},
	{-1, -1, 99, "OOB_way_en", true},

	/* 100 */
	{-1, -1, 100, "OOB_way_en", true},
	{-1, -1, 101, "OOB_way_en", true},
	{-1, -1, 102, "OOB_way_en", true},
	{-1, -1, 103, "OOB_way_en", true},
	{-1, -1, 104, "OOB_way_en", true},
	{-1, -1, 105, "OOB_way_en", true},
	{-1, -1, 106, "OOB_way_en", true},
	{-1, -1, 107, "OOB_way_en", true},
	{-1, -1, 108, "OOB_way_en", true},
	{-1, -1, 109, "OOB_way_en", true},

	/* 110 */
	{-1, -1, 110, "OOB_way_en", true},
	{-1, -1, 111, "OOB_way_en", true},
	{-1, -1, 112, "OOB_way_en", true},
	{-1, -1, 113, "OOB_way_en", true},
	{-1, -1, 114, "OOB_way_en", true},
	{-1, -1, 115, "OOB_way_en", true},
	{-1, -1, 116, "OOB_way_en", true},
	{-1, -1, 117, "OOB_way_en", true},
	{-1, -1, 118, "OOB_way_en", true},
	{-1, -1, 119, "OOB_way_en", true},

	/* 120 */
	{-1, -1, 120, "OOB_way_en", true},
	{-1, -1, 121, "OOB_way_en", true},
	{-1, -1, 122, "OOB_way_en", true},
	{-1, -1, 123, "OOB_way_en", true},
	{-1, -1, 124, "OOB_way_en", true},
	{-1, -1, 125, "OOB_way_en", true},
	{-1, -1, 126, "OOB_way_en", true},
	{-1, -1, 127, "OOB_way_en", true},
	{-1, -1, 128, "OOB_way_en", true},
	{-1, -1, 129, "OOB_way_en", true},

	/* 130 */
	{-1, -1, 130, "OOB_way_en", true},
	{-1, -1, 131, "OOB_way_en", true},
	{-1, -1, 132, "OOB_way_en", true},
	{-1, -1, 133, "OOB_way_en", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},

	/* 140 */
	{-1, -1, 140, "OOB_way_en", true},
	{-1, -1, 141, "OOB_way_en", true},
	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},

	/* 150 */
	{-1, -1, 150, "OOB_way_en", true},
	{-1, -1, 151, "OOB_way_en", true},
	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},

	{-1, -1, 154, "Decode_error", true},
	{-1, -1, 155, "Decode_error", true},
	{-1, -1, 156, "Decode_error", true},
	{-1, -1, 157, "Decode_error", true},
	{-1, -1, 158, "Decode_error", true},
};

static const struct mtk_device_info mt6991_devices_gpu[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MFG_S_CG", true},
	{0, 1, 1, "MFG_S_BRCAST", true},
	{0, 2, 2, "MFG_S_G3D_CONFIG", true},
	{0, 3, 3, "MFG_S_G3D_SECURE_CONFIG", true},
	{0, 4, 4, "MFG_S_VGPU_DEVAPC_GRP4", true},
	{0, 5, 5, "MFG_S_VGPU_TOP_DBG_TRACKER-1", true},
	{0, 6, 6, "MFG_S_VGPU_TOP_DBG_TRACKER-2", true},
	{0, 7, 7, "MFG_S_IP", true},
	{0, 8, 8, "MFG_S_MALI_AXUSER", true},
	{0, 9, 9, "MFG_S_VGPU_DEVAPC_GRP0", true},

	/* 10 */
	{0, 10, 10, "MFG_S_VGPU_DEVAPC_GRP1", true},
	{0, 11, 11, "MFG_S_SMMU_CONFIG", true},
	{0, 12, 12, "MFG_S_BRISKET_TOP", true},
	{0, 13, 13, "MFG_S_SES_TOP0", true},
	{0, 14, 14, "MFG_S_DVFS_HINT", true},
	{0, 15, 15, "MFG_S_SMMU-1", true},
	{0, 16, 16, "MFG_S_SMMU-2", true},
	{0, 17, 17, "MFG_S_SMMU-3", true},
	{0, 18, 18, "MFG_S_VGPU_DEVAPC_GRP2", true},
	{0, 19, 19, "MFG_S_VGPU_DEVAPC_GRP3", true},
};

static const struct mtk_device_info mt6991_devices_gpu1[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MFG_S_RPC", true},
	{0, 1, 1, "MFG_S_DFD_V3P6", true},
	{0, 2, 2, "MFG_S_HRE0", true},
	{0, 3, 3, "MFG_S_VGPU_DEVAPC_AO_WRAPPER", true},
	{0, 4, 4, "MFG_S_VCORE_DEVAPC_AO_WRAPPER", true},
	{0, 5, 5, "MFG_S_VCORE_AO_DBG_TRACKER-1", true},
	{0, 6, 6, "MFG_S_VCORE_AO_DBG_TRACKER-2", true},
	{0, 7, 7, "MFG_S_PLL", true},
	{0, 8, 8, "MFG_S_DPMSR", true},
	{0, 9, 9, "MFG_S_HBVC", true},

	/* 10 */
	{0, 10, 10, "MFG_S_VCORE_AO_CONFIG", true},
	{0, 11, 11, "MFG_S_VGPU_DEVAPC_WRAPPER", true},
	{0, 12, 12, "MFG_S_VCORE_DEVAPC_WRAPPER", true},
	{0, 13, 13, "MFG_S_GPUEB_TCM_MPOOL", true},
	{0, 14, 14, "MFG_S_GPUEB_AUTO_DMA", true},
	{0, 15, 15, "MFG_S_GPUEB_CKCTRL", true},
	{0, 16, 16, "MFG_S_GPUEB_INTC", true},
	{0, 17, 17, "MFG_S_GPUEB_DMA", true},
	{0, 18, 18, "MFG_S_GPUEB_CFG", true},
	{0, 19, 19, "MFG_S_VCORE_DEVAPC_GRP0", true},

	/* 20 */
	{0, 20, 20, "MFG_S_GPUEB_OCD_BIU", true},
	{0, 21, 21, "MFG_S_GPUEB_MBOX", true},
	{0, 22, 22, "MFG_S_GPUEB_DEBUG_COMP", true},
	{0, 23, 23, "MFG_S_TSFDC_CTRL", true},
	{0, 24, 24, "MFG_S_ACP_SNOC", true},
	{0, 25, 25, "MFG_S_TCU_ACP_SNOC", true},
	{0, 26, 26, "MFG_S_GPUEB_DBG_TRACKER-1", true},
	{0, 27, 27, "MFG_S_GPUEB_DBG_TRACKER-2", true},
	{0, 28, 28, "MFG_S_DBG_TRACKER_IRQ", true},
};

enum DEVAPC_VIO_SLAVE_NUM {
	VIO_SLAVE_NUM_APINFRA_IO = ARRAY_SIZE(mt6991_devices_apinfra_io),
	VIO_SLAVE_NUM_APINFRA_IO_CTRL = ARRAY_SIZE(mt6991_devices_apinfra_io_ctrl),
	VIO_SLAVE_NUM_APINFRA_IO_INTF = ARRAY_SIZE(mt6991_devices_apinfra_io_intf),
	VIO_SLAVE_NUM_APINFRA_BIG4 = ARRAY_SIZE(mt6991_devices_apinfra_big4),
	VIO_SLAVE_NUM_APINFRA_DRAMC = ARRAY_SIZE(mt6991_devices_apinfra_dramc),
	VIO_SLAVE_NUM_APINFRA_EMI = ARRAY_SIZE(mt6991_devices_apinfra_emi),
	VIO_SLAVE_NUM_APINFRA_SSR = ARRAY_SIZE(mt6991_devices_apinfra_ssr),
	VIO_SLAVE_NUM_APINFRA_MEM = ARRAY_SIZE(mt6991_devices_apinfra_mem),
	VIO_SLAVE_NUM_APINFRA_MEM_CTRL = ARRAY_SIZE(mt6991_devices_apinfra_mem_ctrl),
	VIO_SLAVE_NUM_APINFRA_MEM_INTF = ARRAY_SIZE(mt6991_devices_apinfra_mem_intf),
	VIO_SLAVE_NUM_APINFRA_INT = ARRAY_SIZE(mt6991_devices_apinfra_int),
	VIO_SLAVE_NUM_APINFRA_MMU = ARRAY_SIZE(mt6991_devices_apinfra_mmu),
	VIO_SLAVE_NUM_APINFRA_SLB = ARRAY_SIZE(mt6991_devices_apinfra_slb),
	VIO_SLAVE_NUM_PERI_PAR = ARRAY_SIZE(mt6991_devices_peri_par_a0),
	VIO_SLAVE_NUM_VLP = ARRAY_SIZE(mt6991_devices_vlp),
	VIO_SLAVE_NUM_ADSP = ARRAY_SIZE(mt6991_devices_adsp),
	VIO_SLAVE_NUM_MMINFRA = ARRAY_SIZE(mt6991_devices_mminfra),
	VIO_SLAVE_NUM_MMUP = ARRAY_SIZE(mt6991_devices_mmup),
	VIO_SLAVE_NUM_GPU = ARRAY_SIZE(mt6991_devices_gpu),
	VIO_SLAVE_NUM_GPU1 = ARRAY_SIZE(mt6991_devices_gpu1),
};

#endif /* __DEVAPC_MT6991_H__ */
