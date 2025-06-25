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

void mtk_vdisp_avs_vcp_notifier_v1(unsigned long vcp_event, void *data);
int mtk_vdisp_avs_dbg_opt_v1(const char *opt);

/* This enum is used to define the IPI function IDs for vdisp */
enum mtk_vdisp_avs_ipi_func_id {
	FUNC_IPI_AGING_UPDATE,
	FUNC_IPI_AVS_EN,
	FUNC_IPI_AVS_DBG_MODE,
	FUNC_IPI_AGING_ACK,
	FUNC_IPI_AVS_STEP,
};
#endif
