/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __SMMU_H
#define __SMMU_H
#include "hyp_spinlock.h"

typedef unsigned int uint;
extern const struct pkvm_module_ops *pkvm_smmu_ops;
#define REG32(addr) ((volatile uint32_t *)(uintptr_t)(addr))

/*******************************************************************************
 * Common macro definitions
 ******************************************************************************/
#define ALL_1s(n) ((1ULL << (n)) - 1)
#define MAKE_MASK(MSB, LSB) ((ALL_1s(MSB - LSB + 1)) << (LSB))
#define EXTRACT(data, shift, mask) (((data) >> (shift)) & (mask))
#define COMPOSE(value, shift, mask) (((value) & (mask)) << (shift))
/*******************************************************************************
 * SMMU register definitions
 ******************************************************************************/
#define IDR1 0x4
#define CR0 0x20
#define CR0ACK 0x24
#define GERROR 0x60
#define GERRORN 0x64
#define GERROR_IRQ_CFG0 0x68
#define STRTAB_BASE 0x080
#define STRTAB_BASE_CFG 0x088
#define CMDQ_BASE 0x090
#define CMDQ_PROD 0x098
#define CMDQ_CONS 0x09c
#define EVENTQ_BASE 0xa0
#define EVENTQ_IRQ_CFG0 0xb0
#define PRIQ_BASE 0xc0
#define PRIQ_IRQ_CFG0 0xd0
#define GATOS_SID 0x108
#define GATOS_ADDR 0x110
#define GATOS_PAR 0x118
/* register other */
#define CMDQ_EN (1U << 3)
#define EVTQ_EN (1U << 2)
#define PRIQ_EN (1U << 1)
#define SMMU_EN (1U << 0)
#define SFM_ERR_MASK (1 << 8)
#define CMDQ_ERR_MASK (1 << 0)
#define CMDQ_ERRORCODE_SHIFT (24)
#define CMDQ_ERRORCODE_MASK (0x7F)
#define CERROR_NONE 0
#define CERROR_ILL 1
#define CERROR_ABT 2
#define CERROR_ATC_INV_SYNC 3
/*******************************************************************************
 * Stream table
 ******************************************************************************/
#define STE_BITS (512UL)
#define SID_CNT (256)
#define STE_ENTRY_256 (8)
#define LEAF_STE (1)
#define GLOBAL_STE_SIZE ((SID_CNT * STE_BITS) >> 3)
#define MTK_SMMU_STRTAB_BASE_CFG_FMT_MASK MAKE_MASK(17, 16)
#define STE_2lvl(strtab_cfg_base)                                              \
	((strtab_cfg_base) & MTK_SMMU_STRTAB_BASE_CFG_FMT_MASK)
#define STE_SID_FIELD(reg_val) ((reg_val & MAKE_MASK(63, 32)) >> 32)
#define STE_SIZE (64)
#define STE_SIZE_DW (STE_SIZE / 8)
#define STE_ENTRY_NUM(stream_n_bits) (1U << stream_n_bits)
#define MTK_SMMU_STE_SIZE_MASK MAKE_MASK(5, 0)
#define MTK_SMMU_STE_SIZE(strtab_base_cfg_value)                               \
	((1 << (strtab_base_cfg_value & MTK_SMMU_STE_SIZE_MASK)) * STE_SIZE)
#define MTK_SMMU_STE_ADDR_MASK MAKE_MASK(51, 6)
#define MTK_SMMU_STE_RA_MASK (1ULL << 62)
#define STRTAB_STE_0_V (1UL << 0)
#define STRTAB_STE_0_S2_CFG (1UL << 2)
#define STRTAB_STE_0_PASS_CFG (1UL << 3)
#define STRTAB_STE_2_S2_SETTING (~(0x0UL))
#define STRTAB_STE_3_S2_SETTING (~(0x0UL))
/*******************************************************************************
 * Queue
 ******************************************************************************/
#define SMMU_CMDQ_BASE_ADDR_MASK MAKE_MASK(51, 5)
#define CMD_SIZE (16)
#define CMD_SIZE_DW (CMD_SIZE / 8)
#define MTK_SMMU_CMDQ_SIZE_MASK MAKE_MASK(4, 0)
#define MTK_SMMU_CMDQ_SIZE(cmdq_base_value)                                    \
	((1 << (cmdq_base_value & MTK_SMMU_CMDQ_SIZE_MASK)) * (16))
#define MTK_SMMU_CMDQ_RA_MASK (1ULL << 62)
/* host's cmdq can hold 2^15 cmds */
#define MTK_SMMU_HOST_CMDQ_ENTRY (15U)
#define MTK_SMMU_GUEST_CMDQ_ENTRY (14U)
#define WRAP_MASK (1U)
#define CMDQ_INDEX(prod_index, mask) (prod_index & mask)
/* cmdq cmd */
#define CMDQ_OP_RESERVED 0x0
#define OP_CFGI_STE 0x03
#define OP_CFGI_ALL 0x04
#define CMDQ_OP_TLBI_NH_ASID 0x11
#define CMDQ_OP_TLBI_NH_VA 0x12
#define CMDQ_OP_TLBI_S12_VMALL 0x28
#define OP_CMD_SYNC 0x46
/* cmd other */
#define LEGAL_CMD (0)
#define ILLEGAL_CMD (1)
#define NS_STREAM (0)
#define OP_SHIFT (0)
#define OP_MASK (0xFF)
#define SSEC_SHIFT (10)
#define SSEC_MASK (1)
#define CMD_SID_SHIFT 32
#define CMD_SID_MASK (0xFFFFFFFF)
#define CSIGNAL_NONE (0)
#define CSIGNAL_SHIFT 12
#define CSIGNAL_MASK (0x3)
#define CMD_VMID_SHIFT (32)
#define CMD_VMID_FIELD MAKE_MASK(47, 32)
#define PROTECTED_VMID (1UL)
/*******************************************************************************
 * SMMU structure definitions
 ******************************************************************************/
enum smmu_id { SMMU_MM = 0, SMMU_APU, SMMU_SOC, SMMU_GPU, SMMU_ID_NUM };
struct smmuv3_stream_table_config {
	void *base;
};
struct smmuv3_queue {
	void *q_base;
	void *cons_reg_base;
	void *prod_reg_base;
	hyp_spinlock_t cmdq_issue_lock;
};
struct smmuv3_features {
	unsigned int cmdq_entries_log2;
	unsigned int stream_n_bits;
};
struct smmuv3_driver {
	void *base_addr;
	struct smmuv3_features prop;
	struct smmuv3_queue cmd_queue;
	struct smmuv3_stream_table_config strtab_cfg;
};
typedef struct smmu_device {
	/* subsys info */
	unsigned long reg_base_pa_addr;
	void *reg_base_va_addr;
	unsigned int smmu_id;
	unsigned int reg_size;
	unsigned int dma_coherent;
	/* driver structure info */
	struct smmuv3_driver *smmuv3;
	/* command queue info */
	unsigned int guest_cmdq_entries_log2;
	unsigned long guest_cmdq_regval;
	unsigned int guest_prod_reg;
	unsigned int guest_cons_reg;
	unsigned long guest_cmdq_pa;
	unsigned long long *guest_cmdq_va;
	hyp_spinlock_t cmdq_batch_lock;
	/* ste info */
	unsigned long guest_strtab_base_reg;
	unsigned long guest_strtab_base_pa;
	unsigned long long *guest_strtab_base_va;
	/* regs info */
	unsigned long guest_cr0_regval;
	unsigned long long guest_cr0ack_regval;
} smmu_device_t;
/*******************************************************************************
 * EL1 operations
 ******************************************************************************/
enum hyp_smmu_hvc_cmd {
	HYP_SMMU_STE_DUMP,
	HYP_SMMU_TF_DUMP,
	HYP_SMMU_REG_DUMP
};
/*
 * These structure is for smmu_mgmt.c usage
 * because some structure belong to hfanium just like spinlock.
 * it will cause some redefine problem if hfanium structure was included
 */
#define INVALID_SID_BIT (1UL << 63)
#define INVALID_VMID_BIT (1UL << 62)
#define INVALID_IPA_BIT (1UL << 61)
#define INVALID_STE_ROW_BIT (1UL << 60)
#define INVALID_ACTION_ID_BIT (1UL << 59)
#define INVALID_SMMU_TYPE_BIT (1UL << 58)
/*******************************************************************************
 * EL3 operations
 ******************************************************************************/
enum hyp_smmu_smc_cmd {
	HYP_SMMU_PM_GET,
	HYP_SMMU_PM_PUT,
	HYP_SMMU_GLOBAL_STE_BASE_INFO,
	HYP_SMMU_S2_TTBR_INFO,
	HYP_SMMU_GLOBAL_STE_DUMP,
	HYP_SMMU_S2_PGTABLE_DUMP
};
#define SMMU_POWER_ON 0
#define MTK_SIP_HYP_SMMU_CONTROL 0xC2000831
#define MTK_SMC_GZ_PREFIX 0x8000
/*******************************************************************************
 * HYP PMM operations
 ******************************************************************************/
#define PMM_MSG_ORDER_SHIFT (24UL)
#define GET_PMM_ENTRY_PFN(entry) (entry & ((1UL << PMM_MSG_ORDER_SHIFT) - 1))
#define GET_PMM_ENTRY_ORDER(entry) ((entry >> PMM_MSG_ORDER_SHIFT) & 0Xf)
#define ONE_PAGE_OFFSET 12
#define ONE_PAGE_SIZE (1 << ONE_PAGE_OFFSET)
/* record for MPU addr/size */
struct mpu_record {
	uint64_t addr;
	uint64_t size;
};
/* It is the same with enum MTEE_MCHUNKS_ID at tmem_ffa.c */
enum MPU_REQ_ORIGIN_ZONE_ID {
	MPU_REQ_ORIGIN_EL2_ZONE_PROT = 0,
	MPU_REQ_ORIGIN_EL2_ZONE_SVP = 8,
	MPU_REQ_ORIGIN_EL2_ZONE_WFD = 9,
	MPU_REQ_ORIGIN_EL2_ZONE_TUI = 12,

	MPU_REQ_ORIGIN_EL2_ZONE_MAX = 13,
};
/*******************************************************************************
 * Other
 ******************************************************************************/
#define STE_SHARE 1
#define GET_CMDQ 0
#define GET_GLOBAL_STE 1
#define GET_POOL 2

void flush_dcache_range(void *ptr, unsigned int size);
void write_ste(unsigned long long *st_entry, const unsigned long long *data);
int smmuv3_issue_cmd(smmu_device_t *dev, unsigned long long *cmd);
void construct_cmd_sync(unsigned long long *cmd);
void construct_cmd_tlbi_s12_vmall(unsigned long long *cmd, unsigned int vmid);
int smmuv3_rd_meets_wr_idx(smmu_device_t *dev);
unsigned int has_dma_coherent_in_devices(void);

#endif
