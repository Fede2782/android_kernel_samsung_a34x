/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Mediatek Inc.
 */

/*
 * #define INFRA_LASTBUS_TIMEOUT_MASK      (1 << 0)
 * #define PERI_LASTBUS_TIMEOUT_MASK       (1 << 1)
 * #define DRAM_MD32_WDT_EVENT_CH_A_MASK   (1 << 2)
 * #define DRAM_MD32_WDT_EVENT_CH_B_MASK   (1 << 3)
 * #define DRAM_MD32_WDT_EVENT_CH_C_MASK   (1 << 4)
 * #define DRAM_MD32_WDT_EVENT_CH_D_MASK   (1 << 5)
 * #define INFRA_SMMU_IRQ_MASK             (1 << 6)
 * #define INFRA_SMMU_NS_IRQ_MASK          (1 << 7)
 * #define AP_TRACKER_TIMEOUT_MASK         (1 << 8)
 * #define INFRA_TRACKER_TIMEOUT_MASK      (1 << 9)
 * #define VLP_TRACKER_TIMEOUT_MASK        (1 << 10)
 * #define MCU_TO_SOC_DFD_EVENT_MASK       (1 << 11)
 * #define APU_SMMU_IRQ_MASK               (1 << 12)
 * #define MFG_TO_SOC_DFD_EVENT_MASK       (1 << 13)
 * #define MMINFRA_SMMU_IRQ_MASK           (1 << 14)
 * #define MFG_TO_EMI_SLV_PARITY_MASK      (1 << 15)
 * #define MCU2SUB_EMI_M1_PARITY_MASK      (1 << 16)
 * #define MCU2SUB_EMI_M0_PARITY_MASK      (1 << 17)
 * #define MCU2EMI_M1_PARITY_MASK          (1 << 18)
 * #define MCU2EMI_M0_PARITY_MASK          (1 << 19)
 * #define MCU2INFRA_REG_PARITY_MASK       (1 << 20)
 * #define INFRA_L3_CACHE2MCU_PARITY_MASK  (1 << 21)
 * #define EMI_PARITY_CEN__MASK            (1 << 22)
 * #define EMI_PARITY_SUB_CEN_MASK         (1 << 23)
 * #define EMI_PARITY_CHAN1_MASK           (1 << 24)
 * #define EMI_PARITY_CHAN2_MASK           (1 << 25)
 * #define EMI_PARITY_CHAN3_MASK           (1 << 26)
 * #define EMI_PARITY_CHAN4_MASK           (1 << 27)
 * #define DRAMC_ERROR_FLAG_CH_A_MASK      (1 << 28)
 * #define DRAMC_ERROR_FLAG_CH_B_MASK      (1 << 29)
 * #define DRAMC_ERROR_FLAG_CH_C_MASK      (1 << 30)
 * #define DRAMC_ERROR_FLAG_CH_D_MASK      (1 << 31)
 */
#ifndef _DBG_ERROR_FLAG_H_
#define _DBG_ERROR_FLAG_H_

#define NAME_LEN 64

enum DBG_ERR_FLAG_OP {
	DBG_ERR_FLAG_CLR = 0,
	DBG_ERR_FLAG_UNMASK = 1,
	NR_DBG_ERR_FLAG_OP
};

struct DBG_ERROR_FLAG_DESC {
	char mask_name[NAME_LEN];
	bool support;
	u32 mask;
};

enum DBG_ERROR_FLAG_ENUM {
	VLP_TRACE_HALT_IRQ,
	INFRA_LASTBUS_TIMEOUT,
	PERI_LASTBUS_TIMEOUT,
	DRAM_MD32_WDT_EVENT_CH_A,
	DRAM_MD32_WDT_EVENT_CH_B,
	DRAM_MD32_WDT_EVENT_CH_C,
	DRAM_MD32_WDT_EVENT_CH_D,
	INFRA_SMMU_IRQ,
	INFRA_SMMU_NS_IRQ,
	AP_TRACKER_TIMEOUT,
	INFRA_TRACKER_TIMEOUT,
	SLOW_INFRA_TRACKER_TIMEOUT,
	FAST_INFRA_TRACKER_TIMEOUT,
	VLP_TRACKER_TIMEOUT,
	MCU_TO_SOC_DFD_EVENT,
	APU_SMMU_IRQ,
	MFG_TO_SOC_DFD_EVENT,
	MMINFRA_SMMU_IRQ,
	MFG_TO_EMI_SLV_PARITY,
	MCU2SUB_EMI_M1_PARITY,
	MCU2SUB_EMI_M0_PARITY,
	MCU2EMI_M1_PARITY,
	MCU2EMI_M0_PARITY,
	MCU2INFRA_REG_PARITY,
	INFRA_L3_CACHE2MCU_PARITY,
	GPUEB_PARITY_FAIL,
	EMI_PARITY_CEN,
	EMI_PARITY_SUB_CEN,
	EMI_PARITY_CHAN1,
	EMI_PARITY_CHAN2,
	EMI_PARITY_CHAN3,
	EMI_PARITY_CHAN4,
	DRAMC_ERROR_FLAG_CH_A,
	DRAMC_ERROR_FLAG_CH_B,
	DRAMC_ERROR_FLAG_CH_C,
	DRAMC_ERROR_FLAG_CH_D,
	SOC_DBG_ERR_FLAG_WDT_IRQ,
	MMU_TO_SOC_DFD_TRIGGER_EVENT,
	MFG2SOC_DFD_SMMU_TRIGGER_EVENT,
	DBG_ERROR_FLAG_TOTAL,
};

extern struct DBG_ERROR_FLAG_DESC dbg_error_flag_desc[];
extern void dbg_error_flag_register_notify(struct notifier_block *nb);
extern unsigned int get_dbg_error_flag_mask(unsigned int err_flag_enum);
#endif
