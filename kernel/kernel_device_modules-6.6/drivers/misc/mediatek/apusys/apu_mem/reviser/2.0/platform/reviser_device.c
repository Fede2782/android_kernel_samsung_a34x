// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include "reviser_cmn.h"
#include "reviser_plat.h"
#include "reviser_device.h"

static struct reviser_plat mt6893_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.mdla_max				= 2,
	.vpu_max				= 3,
	.edma_max				= 2,
	.up_max					= 1,
	.slb_wait_time				= 0,

	.vlm_remap_table_base   = 0x200,
	.vlm_CTX_base           = 0x300,
	.vlm_remap_ctx_src      = 0x001C0000,
	.vlm_remap_ctx_src_ofst = 18,
	.vlm_remap_ctx_dst      = 0x0003C000,
	.vlm_remap_ctx_dst_ofst = 14,
	.vlm_remap_tlb_src_max  = 0x7,
	.vlm_remap_tlb_dst_max  = 0xC,

	.mva_base = 0x4000000,
	.MDLA_ctxt_0 = 0x308,
	.MDLA_ctxt_1 = 0x30C,
	.VPU_ctxt_0  = 0x310,
	.VPU_ctxt_1  = 0x314,
	.VPU_ctxt_2  = 0x318,
	.EDMA_ctxt_0 = 0x31C,
	.EDMA_ctxt_1 = 0x320,
};

static struct reviser_plat mt6885_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.mdla_max				= 2,
	.vpu_max				= 3,
	.edma_max				= 2,
	.up_max					= 1,
	.slb_wait_time				= 0,

	.vlm_remap_table_base   = 0x200,
	.vlm_CTX_base           = 0x300,
	.vlm_remap_ctx_src      = 0x001C0000,
	.vlm_remap_ctx_src_ofst = 18,
	.vlm_remap_ctx_dst      = 0x0003C000,
	.vlm_remap_ctx_dst_ofst = 14,
	.vlm_remap_tlb_src_max  = 0x7,
	.vlm_remap_tlb_dst_max  = 0xC,

	.mva_base = 0x4000000,
	.MDLA_ctxt_0 = 0x308,
	.MDLA_ctxt_1 = 0x30C,
	.VPU_ctxt_0  = 0x310,
	.VPU_ctxt_1  = 0x314,
	.VPU_ctxt_2  = 0x318,
	.EDMA_ctxt_0 = 0x31C,
	.EDMA_ctxt_1 = 0x320,
};

static struct reviser_plat mt6873_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.mdla_max				= 1,
	.vpu_max				= 2,
	.edma_max				= 1,
	.up_max					= 1,
	.slb_wait_time				= 0,

	.vlm_remap_table_base   = 0x200,
	.vlm_CTX_base           = 0x300,
	.vlm_remap_ctx_src      = 0x001C0000,
	.vlm_remap_ctx_src_ofst = 18,
	.vlm_remap_ctx_dst      = 0x0003C000,
	.vlm_remap_ctx_dst_ofst = 14,
	.vlm_remap_tlb_src_max  = 0x7,
	.vlm_remap_tlb_dst_max  = 0xC,

	.mva_base = 0x4000000,
	.MDLA_ctxt_0 = 0x308,
	.MDLA_ctxt_1 = 0x30C,
	.VPU_ctxt_0  = 0x310,
	.VPU_ctxt_1  = 0x314,
	.VPU_ctxt_2  = 0x318,
	.EDMA_ctxt_0 = 0x31C,
	.EDMA_ctxt_1 = 0x320,
};

static struct reviser_plat mt6853_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.mdla_max				= 0,
	.vpu_max				= 2,
	.edma_max				= 0,
	.up_max					= 1,
	.slb_wait_time				= 0,

	.vlm_remap_table_base   = 0x200,
	.vlm_CTX_base           = 0x300,
	.vlm_remap_ctx_src      = 0x001C0000,
	.vlm_remap_ctx_src_ofst = 18,
	.vlm_remap_ctx_dst      = 0x0003C000,
	.vlm_remap_ctx_dst_ofst = 14,
	.vlm_remap_tlb_src_max  = 0x7,
	.vlm_remap_tlb_dst_max  = 0xC,

	.mva_base = 0x4000000,
	.MDLA_ctxt_0 = 0x308,
	.MDLA_ctxt_1 = 0x30C,
	.VPU_ctxt_0  = 0x310,
	.VPU_ctxt_1  = 0x314,
	.VPU_ctxt_2  = 0x318,
	.EDMA_ctxt_0 = 0x31C,
	.EDMA_ctxt_1 = 0x320,
};

static struct reviser_plat mt6877_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.mdla_max				= 1,
	.vpu_max				= 2,
	.edma_max				= 1,
	.up_max					= 1,
	.slb_wait_time				= 0,

	.vlm_remap_table_base   = 0x300,
	.vlm_CTX_base           = 0x100,
	.vlm_remap_ctx_src      = 0x001E0000,
	.vlm_remap_ctx_src_ofst = 17,
	.vlm_remap_ctx_dst      = 0x0001F000,
	.vlm_remap_ctx_dst_ofst = 12,
	.vlm_remap_tlb_src_max  = 0xF,
	.vlm_remap_tlb_dst_max  = 0x14,

	.mva_base = 0x8000000,
	.MDLA_ctxt_0 = 0x108,
	.MDLA_ctxt_1 = 0x10C,
	.VPU_ctxt_0  = 0x114,
	.VPU_ctxt_1  = 0x11C,
	.VPU_ctxt_2  = 0x124,
	.EDMA_ctxt_0 = 0x140,
	.EDMA_ctxt_1 = 0x160,
};

static struct reviser_plat mt6886_drv = {
	.init					= reviser_vrv_init,
	.uninit					= reviser_vrv_uninit,

	.bank_size				= 0x20000,
	.mdla_max				= 2,
	.vpu_max				= 3,
	.edma_max				= 2,
	.up_max					= 1,
	.slb_wait_time				= 85,
};

static struct reviser_plat rv_drv = {
	.init					= reviser_vrv_init,
	.uninit					= reviser_vrv_uninit,

	.bank_size				= 0x20000,
	.mdla_max				= 2,
	.vpu_max				= 3,
	.edma_max				= 2,
	.up_max					= 1,
	.slb_wait_time				= 0,
};

static const struct of_device_id reviser_of_match[] = {
	{ .compatible = "mediatek, mt6893-reviser",    .data = &mt6893_drv},
	{ .compatible = "mediatek, mt6885-reviser",    .data = &mt6885_drv},
	{ .compatible = "mediatek, mt6873-reviser",    .data = &mt6873_drv},
	{ .compatible = "mediatek, mt6853-reviser",    .data = &mt6853_drv},
	{ .compatible = "mediatek, mt6877-reviser",    .data = &mt6877_drv},
	{ .compatible = "mediatek, rv-reviser-mt6886", .data = &mt6886_drv},
	{ .compatible = "mediatek, rv-reviser",        .data = &rv_drv},
	{/* end of list */},
};

MODULE_DEVICE_TABLE(of, reviser_of_match);

const struct of_device_id *reviser_get_of_device_id(void)
{
	return reviser_of_match;
}

