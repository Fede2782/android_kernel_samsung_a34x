/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _REVISER_REG_H_
#define _REVISER_REG_H_
#include <linux/types.h>

#define VLM_BASE                 (0x1D800000)
#define TCM_BASE                 (0x1D000000)
#define TCM_SIZE                 (0x000000)
#define VLM_SIZE                 (0x400000)
#define VLM_BANK_SIZE            (0x40000)
#define REMAP_DRAM_SIZE          (0x4000000)
#define REMAP_DRAM_BASE          (0x8000000)
#define VLM_DRAM_BANK_MAX        (16)
#define VLM_TCM_BANK_MAX         (0)
#define VLM_CTXT_DRAM_OFFSET     (0x200000)

#define REVISER_BASE             (0x19021000)
#define REVISER_INT_BASE         (0x19001000)

#define REVISER_FAIL             (0xFFFFFFFF)
#define REVISER_DEFAULT          (0xFFFFFFFF)

#define REVISER_INT_EN           (0x80)
#define APUSYS_EXCEPT_INT        (0x34)
#define REVISER_INT_EN_MASK      (0xFE000)

#define VP6_CORE0_BASE_0         (0x0100)
#define VP6_CORE0_BASE_1         (0x0108)

#define VLM_DEFAULT_MVA(VLM_remap_table_base) (VLM_remap_table_base + 0x00)

#define VLM_CTXT_MDLA_MAX         (1)
#define VLM_CTXT_MDLA_0(ctx_base) (ctx_base + 0x08)
#define VLM_CTXT_MDLA_1(ctx_base) (ctx_base + 0x0C)

#define VLM_CTXT_VPU_MAX          (2)
#define VLM_CTXT_VPU_0(ctx_base)  (ctx_base + 0x14)
#define VLM_CTXT_VPU_1(ctx_base)  (ctx_base + 0x1C)
#define VLM_CTXT_VPU_2(ctx_base)  (ctx_base + 0x24)

#define VLM_CTXT_EDMA_MAX         (1)
#define VLM_CTXT_EDMA_0(ctx_base) (ctx_base + 0x40) //Ch5
#define VLM_CTXT_EDMA_1(ctx_base) (ctx_base + 0x60) //Ch5

#define VLM_CTXT_UP_MAX          (0)

#define AXI_EXCEPTION_MD32       (0x0400)
#define AXI_EXCEPTION_MDLA_0     (0x0408)
#define AXI_EXCEPTION_MDLA_1     (0x040C)
#define AXI_EXCEPTION_VPU_0      (0x0410)
#define AXI_EXCEPTION_VPU_1      (0x0414)
#define AXI_EXCEPTION_VPU_2      (0x0418)
#define AXI_EXCEPTION_EDMA_0     (0x041C)
#define AXI_EXCEPTION_EDMA_1     (0x0420)

#define VLM_REMAP_VALID          (0x80000000)
#define VLM_REMAP_VALID_OFFSET   (31)
#define VLM_REMAP_CTX_ID         (0x03E00000)
#define VLM_REMAP_CTX_ID_OFFSET  (21)

#define VLM_CTXT_BDY_SELECT      (0x00000003)
#define VLM_CTXT_BDY_SELECT_MAX  (3)

#define VLM_CTXT_CTX_ID          (0x03E00000)
#define VLM_CTXT_CTX_ID_OFFSET   (21)
#define VLM_CTXT_CTX_ID_MAX      (32)
#define VLM_CTXT_CTX_ID_COUNT (VLM_CTXT_MDLA_MAX + \
							VLM_CTXT_VPU_MAX + \
							VLM_CTXT_EDMA_MAX + \
							VLM_CTXT_UP_MAX)

#define VLM_REMAP_TABLE_MAX  (0xD)


uint32_t  reviser_get_remap_offset(uint32_t index);
uint32_t  reviser_get_contex_offset_MDLA(uint32_t index);
uint32_t  reviser_get_contex_offset_VPU(uint32_t index);
uint32_t  reviser_get_contex_offset_EDMA(uint32_t index);
uint32_t  reviser_get_default_offset(void);
uint32_t  reviser_get_int_offset(void);
void reviser_reg_init_v10(uint32_t vlm_remap_table_base, uint32_t remap_table_dst_max,
	uint32_t MDLA_ctxt_0, uint32_t MDLA_ctxt_1, uint32_t VPU_ctxt_0, uint32_t VPU_ctxt_1,
	uint32_t VPU_ctxt_2, uint32_t EDMA_ctxt_0, uint32_t EDMA_ctxt_1);

#endif
