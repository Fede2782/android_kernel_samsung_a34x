// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>

#include "reviser_cmn.h"
#include "reviser_reg_v1_0.h"

static uint32_t rvr_v10_vlm_remap_table_base;
static uint32_t rvr_v10_remap_table_dst_max;
static uint32_t rvr_v10_VLM_ctxt_MDLA0;
static uint32_t rvr_v10_VLM_ctxt_MDLA1;
static uint32_t rvr_v10_VLM_ctxt_VPU0;
static uint32_t rvr_v10_VLM_ctxt_VPU1;
static uint32_t rvr_v10_VLM_ctxt_VPU2;
static uint32_t rvr_v10_VLM_ctxt_EDMA0;
static uint32_t rvr_v10_VLM_ctxt_EDMA1;


uint32_t reviser_get_remap_offset(uint32_t index)
{
	uint32_t offset = 0;


	if (index <= rvr_v10_remap_table_dst_max)
		offset = rvr_v10_vlm_remap_table_base + (index + 1) * 4;
	else
		offset = REVISER_FAIL;
	return offset;
}

uint32_t reviser_get_contex_offset_MDLA(uint32_t index)
{
	uint32_t offset = 0;

	switch (index) {
	case 0:
		offset = rvr_v10_VLM_ctxt_MDLA0;
		break;
	case 1:
		offset = rvr_v10_VLM_ctxt_MDLA1;
		break;
	default:
		offset = REVISER_FAIL;
		break;
	}

	return offset;
}

uint32_t reviser_get_contex_offset_VPU(uint32_t index)
{
	uint32_t offset = 0;

	switch (index) {
	case 0:
		offset = rvr_v10_VLM_ctxt_VPU0;
		break;
	case 1:
		offset = rvr_v10_VLM_ctxt_VPU1;
		break;
	case 2:
		offset = rvr_v10_VLM_ctxt_VPU2;
		break;
	default:
		offset = REVISER_FAIL;
		break;
	}

	return offset;
}

uint32_t reviser_get_contex_offset_EDMA(uint32_t index)
{
	uint32_t offset = 0;

	switch (index) {
	case 0:
		offset = rvr_v10_VLM_ctxt_EDMA0;
		break;
	case 1:
		offset = rvr_v10_VLM_ctxt_EDMA1;
		break;
	default:
		offset = REVISER_FAIL;
		break;
	}

	return offset;
}

uint32_t reviser_get_default_offset(void)
{
	uint32_t offset = 0;

	offset = VLM_DEFAULT_MVA(rvr_v10_vlm_remap_table_base);

	return offset;
}

void reviser_reg_init_v10(uint32_t vlm_remap_table_base, uint32_t remap_table_dst_max,
	uint32_t MDLA_ctxt_0, uint32_t MDLA_ctxt_1, uint32_t VPU_ctxt_0, uint32_t VPU_ctxt_1,
	uint32_t VPU_ctxt_2, uint32_t EDMA_ctxt_0, uint32_t EDMA_ctxt_1)
{
	rvr_v10_vlm_remap_table_base = vlm_remap_table_base;
	rvr_v10_remap_table_dst_max = remap_table_dst_max;
	rvr_v10_VLM_ctxt_MDLA0 = MDLA_ctxt_0;
	rvr_v10_VLM_ctxt_MDLA1 = MDLA_ctxt_1;
	rvr_v10_VLM_ctxt_VPU0  = VPU_ctxt_0;
	rvr_v10_VLM_ctxt_VPU1  = VPU_ctxt_1;
	rvr_v10_VLM_ctxt_VPU2  = VPU_ctxt_2;
	rvr_v10_VLM_ctxt_EDMA0 = EDMA_ctxt_0;
	rvr_v10_VLM_ctxt_EDMA1 = EDMA_ctxt_1;
}
