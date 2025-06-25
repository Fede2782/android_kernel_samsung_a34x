/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GPUEB_COMMON_H__
#define __GPUEB_COMMON_H__

/**************************************************
 * Definition
 **************************************************/
#define SRAM_GPR_SIZE_4B                    (0x4) /* 4 Bytes */

#define MFG0_PWR_ACK_2ND_BIT                (BIT(31))
#define MFG0_PWR_ACK_1ST_BIT                (BIT(30))
#define MFG0_PWR_ACK_BITS                   (MFG0_PWR_ACK_1ST_BIT | MFG0_PWR_ACK_2ND_BIT)

/**************************************************
 * Enumeration
 **************************************************/

enum gpueb_smc_op {
	GPUEB_SMC_OP_TRIGGER_WDT    = 0,
	GPUACP_SMC_OP_CPUPM_PWR     = 1,
	GPUEB_SMC_OP_NUMBER         = 2,
	GPUHRE_SMP_OP_READ_BACKUP   = 3,
	GPUHRE_SMP_OP_WRITE_RANDOM  = 4,
	GPUHRE_SMP_OP_CHECK_RESTORE = 5,
	GHPM_SWWA_SMP_OP_MFG0_ON    = 6,
	GHPM_SWWA_SMP_OP_MFG0_OFF   = 7
};

enum gpueb_sram_gpr_id {
	GPUEB_SRAM_GPR0  = 0,
	GPUEB_SRAM_GPR1  = 1,
	GPUEB_SRAM_GPR2  = 2,
	GPUEB_SRAM_GPR3  = 3,
	GPUEB_SRAM_GPR4  = 4,
	GPUEB_SRAM_GPR5  = 5,
	GPUEB_SRAM_GPR6  = 6,
	GPUEB_SRAM_GPR7  = 7,
	GPUEB_SRAM_GPR8  = 8,
	GPUEB_SRAM_GPR9  = 9,
	GPUEB_SRAM_GPR10 = 10,
	GPUEB_SRAM_GPR11 = 11,
	GPUEB_SRAM_GPR12 = 12,
	GPUEB_SRAM_GPR13 = 13,
	GPUEB_SRAM_GPR14 = 14,
	GPUEB_SRAM_GPR15 = 15,
	GPUEB_SRAM_GPR16 = 16,
	GPUEB_SRAM_GPR17 = 17,
	GPUEB_SRAM_GPR18 = 18,
	GPUEB_SRAM_GPR19 = 19,
	GPUEB_SRAM_GPR20 = 20,
	GPUEB_SRAM_GPR21 = 21,
	GPUEB_SRAM_GPR22 = 22,
	GPUEB_SRAM_GPR23 = 23,
	GPUEB_SRAM_GPR24 = 24,
	GPUEB_SRAM_GPR25 = 25,
	GPUEB_SRAM_GPR26 = 26,
	GPUEB_SRAM_GPR27 = 27,
	GPUEB_SRAM_GPR28 = 28,
	GPUEB_SRAM_GPR29 = 29,
	GPUEB_SRAM_GPR30 = 30,
	GPUEB_SRAM_GPR31 = 31
};

enum mfg0_pwr_sta {
	MFG0_PWR_OFF,
	MFG0_PWR_ON
};

/**************************************************
 * Function
 **************************************************/
void __iomem *gpueb_get_gpr_base(void);
void __iomem *gpueb_get_gpr_addr(enum gpueb_sram_gpr_id gpr_id);
void __iomem *gpueb_get_cfgreg_base(void);
int get_mfg0_pwr_con(void);
int mfg0_pwr_sta(void);
int is_gpueb_wfi(void);

#endif /* __GPUEB_COMMON_H__ */
