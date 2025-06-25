/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_VDISP_AVS_H__
#define __MTK_VDISP_AVS_H__

#include <soc/mediatek/mmdvfs_v3.h>

#define VDISP_SHRMEM_BITWISE_IDX 5
#define VDISP_SHRMEM_BITWISE	(mmdvfs_get_mmup_sram_enable() \
								? SRAM_VDISP_VAL(VDISP_SHRMEM_BITWISE_IDX) \
								: MEM_VDISP_AVS_STEP(VDISP_SHRMEM_BITWISE_IDX))
#define VDISP_SHRMEM_BITWISE_VAL readl(VDISP_SHRMEM_BITWISE)
#define VDISP_AVS_ENABLE_BIT        0
#define VDISP_AVS_AGING_ENABLE_BIT  1
#define VDISP_AVS_AGING_ACK_BIT     2
#define VDISP_AVS_IPI_ACK_BIT       3

void mtk_vdisp_avs_vcp_notifier(unsigned long vcp_event, void *data);
void mtk_vdisp_avs_query_aging_val(struct device *dev);
int mtk_vdisp_avs_probe(struct platform_device *pdev);
int mtk_vdisp_avs_dbg_opt(const char *opt);

/* This enum is used to define the IPI function IDs for vdisp */
enum mtk_vdisp_avs_ipi_func_id {
	FUNC_IPI_AGING_UPDATE,
	FUNC_IPI_AVS_EN,
	FUNC_IPI_AVS_DBG_MODE,
	FUNC_IPI_AGING_ACK,
	FUNC_IPI_AVS_STEP,
};

struct mtk_vdisp_avs_data {
	/* Aging */
	// register
	u32 aging_reg_ro_en0;
	u32 aging_reg_ro_en1;
	u32 aging_reg_pwr_ctrl;
	u32 aging_ro_fresh;
	u32 aging_ro_aging;
	u32 aging_ro_sel_0;
	u32 aging_win_cyc;
	u32 aging_reg_test;
	u32 aging_reg_ro_en2;
	u32 aging_ro_sel_1;
	// value
	u32 aging_ro_sel_0_fresh_val;
	u32 aging_ro_sel_1_fresh_val;
	u32 aging_ro_sel_0_aging_val;
	u32 aging_ro_sel_1_aging_val;
	u32 aging_reg_ro_en0_fresh_val;
	u32 aging_reg_ro_en0_aging_val;
	u32 aging_reg_ro_en2_fresh_val;
	u32 aging_reg_ro_en2_aging_val;
};

static const struct mtk_vdisp_avs_data mt6989_vdisp_avs_driver_data = {
	.aging_reg_ro_en0   = 0x00,
	.aging_reg_ro_en1   = 0x04,
	.aging_reg_pwr_ctrl = 0x08,
	.aging_ro_fresh     = 0x10, //counter_grp0
	.aging_ro_aging     = 0x2C, //counter_grp7
	.aging_ro_sel_0     = 0x34, //aging_ro_sel
	.aging_ro_sel_0_fresh_val = 0x1B6DB6DB,
	.aging_ro_sel_0_aging_val = 0x00000000,
	.aging_win_cyc      = 0x38,
	.aging_reg_test     = 0x3C,
	.aging_reg_ro_en2   = 0x40,
	.aging_ro_sel_1     = 0x44, //aging_reg_dbg_rdata
	.aging_ro_sel_1_fresh_val = 0x00000000,
	.aging_ro_sel_1_aging_val = 0x00000001,
	.aging_reg_ro_en0_fresh_val = 0x00000004,
	.aging_reg_ro_en0_aging_val = 0x00000000,
	.aging_reg_ro_en2_fresh_val = 0x00000000,
	.aging_reg_ro_en2_aging_val = 0x00010000,
};

// compatible with mt6991
static const struct mtk_vdisp_avs_data default_vdisp_avs_driver_data = {
	.aging_reg_ro_en0   = 0x00,
	.aging_reg_ro_en1   = 0x04,
	.aging_reg_pwr_ctrl = 0x14,
	.aging_ro_fresh     = 0x24, //counter_grp2
	.aging_ro_aging     = 0x4C, //counter_grp12
	.aging_ro_sel_0     = 0x54,
	.aging_ro_sel_0_fresh_val = 0x000001C0,
	.aging_ro_sel_0_aging_val = 0x00000000,
	.aging_win_cyc      = 0x6C,
	.aging_reg_test     = 0x70,
	.aging_reg_ro_en2   = 0x08,
	.aging_ro_sel_1     = 0x58,
	.aging_ro_sel_1_fresh_val = 0x00000000,
	.aging_ro_sel_1_aging_val = 0x00000200,
	.aging_reg_ro_en0_fresh_val = 0x00100000,
	.aging_reg_ro_en0_aging_val = 0x00000000,
	.aging_reg_ro_en2_fresh_val = 0x00000000,
	.aging_reg_ro_en2_aging_val = 0x00000800,
};
#endif
