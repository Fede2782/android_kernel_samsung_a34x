// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6991-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs vde20_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde20_hwv_regs = {
	.set_ofs = 0x0088,
	.clr_ofs = 0x008C,
	.sta_ofs = 0x2C44,
};

static const struct mtk_gate_regs vde21_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde21_hwv_regs = {
	.set_ofs = 0x0080,
	.clr_ofs = 0x0084,
	.sta_ofs = 0x2C40,
};

static const struct mtk_gate_regs vde22_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs vde22_hwv_regs = {
	.set_ofs = 0x0078,
	.clr_ofs = 0x007C,
	.sta_ofs = 0x2C3C,
};

#define GATE_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE20_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde20_cg_regs,			\
		.hwv_regs = &vde20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE21_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde21_cg_regs,			\
		.hwv_regs = &vde21_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE22(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde22_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE22_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE22(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde22_cg_regs,			\
		.hwv_regs = &vde22_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_HWV_VDE20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_VDEC, "vde2_vdec_cken_vdec",
		"vde2_vdec_cken"/* parent */),
	GATE_HWV_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
		"ck2_vdec_ck"/* parent */, 4),
	GATE_VDE20_V(CLK_VDE2_VDEC_ACTIVE_VDEC, "vde2_vdec_active_vdec",
		"vde2_vdec_active"/* parent */),
	GATE_HWV_VDE20(CLK_VDE2_VDEC_CKEN_ENG, "vde2_vdec_cken_eng",
		"ck2_vdec_ck"/* parent */, 8),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_ENG_VDEC, "vde2_vdec_cken_eng_vdec",
		"vde2_vdec_cken_eng"/* parent */),
	/* VDE21 */
	GATE_HWV_VDE21(CLK_VDE2_LAT_CKEN, "vde2_lat_cken",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE21_V(CLK_VDE2_LAT_CKEN_VDEC, "vde2_lat_cken_vdec",
		"vde2_lat_cken"/* parent */),
	GATE_HWV_VDE21(CLK_VDE2_LAT_ACTIVE, "vde2_lat_active",
		"ck2_vdec_ck"/* parent */, 4),
	GATE_VDE21_V(CLK_VDE2_LAT_ACTIVE_VDEC, "vde2_lat_active_vdec",
		"vde2_lat_active"/* parent */),
	GATE_HWV_VDE21(CLK_VDE2_LAT_CKEN_ENG, "vde2_lat_cken_eng",
		"ck2_vdec_ck"/* parent */, 8),
	GATE_VDE21_V(CLK_VDE2_LAT_CKEN_ENG_VDEC, "vde2_lat_cken_eng_vdec",
		"vde2_lat_cken_eng"/* parent */),
	/* VDE22 */
	GATE_HWV_VDE22(CLK_VDE2_LARB1_CKEN, "vde2_larb1_cken",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE22_V(CLK_VDE2_LARB1_CKEN_VDEC, "vde2_larb1_cken_vdec",
		"vde2_larb1_cken"/* parent */),
	GATE_VDE22_V(CLK_VDE2_LARB1_CKEN_SMI, "vde2_larb1_cken_smi",
		"vde2_larb1_cken"/* parent */),
};

static const struct mtk_clk_desc vde2_mcd = {
	.clks = vde2_clks,
	.num_clks = CLK_VDE2_NR_CLK,
};

static const struct mtk_gate_regs vde10_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde10_hwv_regs = {
	.set_ofs = 0x00A0,
	.clr_ofs = 0x00A4,
	.sta_ofs = 0x2C50,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x1E0,
	.clr_ofs = 0x1E0,
	.sta_ofs = 0x1E0,
};

static const struct mtk_gate_regs vde11_hwv_regs = {
	.set_ofs = 0x00B0,
	.clr_ofs = 0x00B4,
	.sta_ofs = 0x2C58,
};

static const struct mtk_gate_regs vde12_cg_regs = {
	.set_ofs = 0x1EC,
	.clr_ofs = 0x1EC,
	.sta_ofs = 0x1EC,
};

static const struct mtk_gate_regs vde12_hwv_regs = {
	.set_ofs = 0x00A8,
	.clr_ofs = 0x00AC,
	.sta_ofs = 0x2C54,
};

static const struct mtk_gate_regs vde13_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde13_hwv_regs = {
	.set_ofs = 0x0098,
	.clr_ofs = 0x009C,
	.sta_ofs = 0x2C4C,
};

static const struct mtk_gate_regs vde14_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs vde14_hwv_regs = {
	.set_ofs = 0x0090,
	.clr_ofs = 0x0094,
	.sta_ofs = 0x2C48,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE10_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde10_cg_regs,			\
		.hwv_regs = &vde10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VDE11_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde11_cg_regs,			\
		.hwv_regs = &vde11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_no_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VDE12_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE12(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde12_cg_regs,			\
		.hwv_regs = &vde12_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_no_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE13(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde13_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE13_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE13(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde13_cg_regs,			\
		.hwv_regs = &vde13_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE14(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde14_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE14_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VDE14(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde14_cg_regs,			\
		.hwv_regs = &vde14_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_HWV_VDE10(CLK_VDE1_VDEC_CKEN, "vde1_vdec_cken",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE10_V(CLK_VDE1_VDEC_CKEN_VDEC, "vde1_vdec_cken_vdec",
		"vde1_vdec_cken"/* parent */),
	GATE_HWV_VDE10(CLK_VDE1_VDEC_ACTIVE, "vde1_vdec_active",
		"ck2_vdec_ck"/* parent */, 4),
	GATE_VDE10_V(CLK_VDE1_VDEC_ACTIVE_VDEC, "vde1_vdec_active_vdec",
		"vde1_vdec_active"/* parent */),
	GATE_HWV_VDE10(CLK_VDE1_VDEC_CKEN_ENG, "vde1_vdec_cken_eng",
		"ck2_vdec_ck"/* parent */, 8),
	GATE_VDE10_V(CLK_VDE1_VDEC_CKEN_ENG_VDEC, "vde1_vdec_cken_eng_vdec",
		"vde1_vdec_cken_eng"/* parent */),
	/* VDE11 */
	GATE_HWV_VDE11(CLK_VDE1_VDEC_SOC_IPS_EN, "vde1_vdec_soc_ips_en",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE11_V(CLK_VDE1_VDEC_SOC_IPS_EN_VDEC, "vde1_vdec_soc_ips_en_vdec",
		"vde1_vdec_soc_ips_en"/* parent */),
	/* VDE12 */
	GATE_HWV_VDE12(CLK_VDE1_VDEC_SOC_APTV_EN, "vde1_aptv_en",
		"ck2_avs_vdec_ck"/* parent */, 0),
	GATE_VDE12_V(CLK_VDE1_VDEC_SOC_APTV_EN_VDEC, "vde1_aptv_en_vdec",
		"vde1_aptv_en"/* parent */),
	GATE_HWV_VDE12(CLK_VDE1_VDEC_SOC_APTV_TOP_EN, "vde1_aptv_topen",
		"ck2_avs_vdec_ck"/* parent */, 1),
	GATE_VDE12_V(CLK_VDE1_VDEC_SOC_APTV_TOP_EN_VDEC, "vde1_aptv_topen_vdec",
		"vde1_aptv_topen"/* parent */),
	/* VDE13 */
	GATE_HWV_VDE13(CLK_VDE1_LAT_CKEN, "vde1_lat_cken",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE13_V(CLK_VDE1_LAT_CKEN_VDEC, "vde1_lat_cken_vdec",
		"vde1_lat_cken"/* parent */),
	GATE_HWV_VDE13(CLK_VDE1_LAT_ACTIVE, "vde1_lat_active",
		"ck2_vdec_ck"/* parent */, 4),
	GATE_VDE13_V(CLK_VDE1_LAT_ACTIVE_VDEC, "vde1_lat_active_vdec",
		"vde1_lat_active"/* parent */),
	GATE_HWV_VDE13(CLK_VDE1_LAT_CKEN_ENG, "vde1_lat_cken_eng",
		"ck2_vdec_ck"/* parent */, 8),
	GATE_VDE13_V(CLK_VDE1_LAT_CKEN_ENG_VDEC, "vde1_lat_cken_eng_vdec",
		"vde1_lat_cken_eng"/* parent */),
	/* VDE14 */
	GATE_HWV_VDE14(CLK_VDE1_LARB1_CKEN, "vde1_larb1_cken",
		"ck2_vdec_ck"/* parent */, 0),
	GATE_VDE14_V(CLK_VDE1_LARB1_CKEN_VDEC, "vde1_larb1_cken_vdec",
		"vde1_larb1_cken"/* parent */),
	GATE_VDE14_V(CLK_VDE1_LARB1_CKEN_SMI, "vde1_larb1_cken_smi",
		"vde1_larb1_cken"/* parent */),
};

static const struct mtk_clk_desc vde1_mcd = {
	.clks = vde1_clks,
	.num_clks = CLK_VDE1_NR_CLK,
};

static const struct mtk_gate_regs ven10_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven10_hwv_regs = {
	.set_ofs = 0x00B8,
	.clr_ofs = 0x00BC,
	.sta_ofs = 0x2C5C,
};

static const struct mtk_gate_regs ven11_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x14,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs ven11_hwv_regs = {
	.set_ofs = 0x00C0,
	.clr_ofs = 0x00C4,
	.sta_ofs = 0x2C60,
};

#define GATE_VEN10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN10_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VEN10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven10_cg_regs,			\
		.hwv_regs = &ven10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VEN11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_VEN11_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VEN11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven11_cg_regs,			\
		.hwv_regs = &ven11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven1_clks[] = {
	/* VEN10 */
	GATE_HWV_VEN10(CLK_VEN1_CKE0_LARB, "ven1_larb",
		"ck2_venc_ck"/* parent */, 0),
	GATE_VEN10_V(CLK_VEN1_CKE0_LARB_VENC, "ven1_larb_venc",
		"ven1_larb"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE0_LARB_JPGENC, "ven1_larb_jpgenc",
		"ven1_larb"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE0_LARB_JPGDEC, "ven1_larb_jpgdec",
		"ven1_larb"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE0_LARB_SMI, "ven1_larb_smi",
		"ven1_larb"/* parent */),
	GATE_HWV_VEN10(CLK_VEN1_CKE1_VENC, "ven1_venc",
		"ck2_venc_ck"/* parent */, 4),
	GATE_VEN10_V(CLK_VEN1_CKE1_VENC_VENC, "ven1_venc_venc",
		"ven1_venc"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE1_VENC_SMI, "ven1_venc_smi",
		"ven1_venc"/* parent */),
	GATE_VEN10(CLK_VEN1_CKE2_JPGENC, "ven1_jpgenc",
		"ck2_venc_ck"/* parent */, 8),
	GATE_VEN10_V(CLK_VEN1_CKE2_JPGENC_JPGENC, "ven1_jpgenc_jpgenc",
		"ven1_jpgenc"/* parent */),
	GATE_VEN10(CLK_VEN1_CKE3_JPGDEC, "ven1_jpgdec",
		"ck2_venc_ck"/* parent */, 12),
	GATE_VEN10_V(CLK_VEN1_CKE3_JPGDEC_JPGDEC, "ven1_jpgdec_jpgdec",
		"ven1_jpgdec"/* parent */),
	GATE_VEN10(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_jpgdec_c1",
		"ck2_venc_ck"/* parent */, 16),
	GATE_VEN10_V(CLK_VEN1_CKE4_JPGDEC_C1_JPGDEC, "ven1_jpgdec_c1_jpgdec",
		"ven1_jpgdec_c1"/* parent */),
	GATE_HWV_VEN10(CLK_VEN1_CKE5_GALS, "ven1_gals",
		"ck2_venc_ck"/* parent */, 28),
	GATE_VEN10_V(CLK_VEN1_CKE5_GALS_VENC, "ven1_gals_venc",
		"ven1_gals"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE5_GALS_JPGENC, "ven1_gals_jpgenc",
		"ven1_gals"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE5_GALS_JPGDEC, "ven1_gals_jpgdec",
		"ven1_gals"/* parent */),
	GATE_HWV_VEN10(CLK_VEN1_CKE29_VENC_ADAB_CTRL, "ven1_venc_adab_ctrl",
		"ck2_venc_ck"/* parent */, 29),
	GATE_VEN10_V(CLK_VEN1_CKE29_VENC_ADAB_CTRL_VENC, "ven1_venc_adab_ctrl_venc",
		"ven1_venc_adab_ctrl"/* parent */),
	GATE_HWV_VEN10(CLK_VEN1_CKE29_VENC_XPC_CTRL, "ven1_venc_xpc_ctrl",
		"ck2_venc_ck"/* parent */, 30),
	GATE_VEN10_V(CLK_VEN1_CKE29_VENC_XPC_CTRL_VENC, "ven1_venc_xpc_ctrl_venc",
		"ven1_venc_xpc_ctrl"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE29_VENC_XPC_CTRL_JPGENC, "ven1_venc_xpc_ctrl_jpgenc",
		"ven1_venc_xpc_ctrl"/* parent */),
	GATE_VEN10_V(CLK_VEN1_CKE29_VENC_XPC_CTRL_JPGDEC, "ven1_venc_xpc_ctrl_jpgdec",
		"ven1_venc_xpc_ctrl"/* parent */),
	GATE_HWV_VEN10(CLK_VEN1_CKE6_GALS_SRAM, "ven1_gals_sram",
		"ck2_venc_ck"/* parent */, 31),
	GATE_VEN10_V(CLK_VEN1_CKE6_GALS_SRAM_VENC, "ven1_gals_sram_venc",
		"ven1_gals_sram"/* parent */),
	/* VEN11 */
	GATE_HWV_VEN11(CLK_VEN1_RES_FLAT, "ven1_res_flat",
		"ck2_venc_ck"/* parent */, 0),
	GATE_VEN11_V(CLK_VEN1_RES_FLAT_VENC, "ven1_res_flat_venc",
		"ven1_res_flat"/* parent */),
	GATE_VEN11_V(CLK_VEN1_RES_FLAT_JPGENC, "ven1_res_flat_jpgenc",
		"ven1_res_flat"/* parent */),
	GATE_VEN11_V(CLK_VEN1_RES_FLAT_JPGDEC, "ven1_res_flat_jpgdec",
		"ven1_res_flat"/* parent */),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct mtk_gate_regs ven20_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven20_hwv_regs = {
	.set_ofs = 0x00C8,
	.clr_ofs = 0x00CC,
	.sta_ofs = 0x2C64,
};

static const struct mtk_gate_regs ven21_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x14,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs ven21_hwv_regs = {
	.set_ofs = 0x00D0,
	.clr_ofs = 0x00D4,
	.sta_ofs = 0x2C68,
};

#define GATE_VEN20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN20_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VEN20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven20_cg_regs,			\
		.hwv_regs = &ven20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VEN21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_VEN21_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VEN21(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven21_cg_regs,			\
		.hwv_regs = &ven21_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven2_clks[] = {
	/* VEN20 */
	GATE_HWV_VEN20(CLK_VEN2_CKE0_LARB, "ven2_larb",
		"ck2_venc_ck"/* parent */, 0),
	GATE_VEN20_V(CLK_VEN2_CKE0_LARB_VENC, "ven2_larb_venc",
		"ven2_larb"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE0_LARB_JPGENC, "ven2_larb_jpgenc",
		"ven2_larb"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE0_LARB_JPGDEC, "ven2_larb_jpgdec",
		"ven2_larb"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE0_LARB_SMI, "ven2_larb_smi",
		"ven2_larb"/* parent */),
	GATE_HWV_VEN20(CLK_VEN2_CKE1_VENC, "ven2_venc",
		"ck2_venc_ck"/* parent */, 4),
	GATE_VEN20_V(CLK_VEN2_CKE1_VENC_VENC, "ven2_venc_venc",
		"ven2_venc"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE1_VENC_SMI, "ven2_venc_smi",
		"ven2_venc"/* parent */),
	GATE_VEN20(CLK_VEN2_CKE2_JPGENC, "ven2_jpgenc",
		"ck2_venc_ck"/* parent */, 8),
	GATE_VEN20_V(CLK_VEN2_CKE2_JPGENC_JPGENC, "ven2_jpgenc_jpgenc",
		"ven2_jpgenc"/* parent */),
	GATE_VEN20(CLK_VEN2_CKE3_JPGDEC, "ven2_jpgdec",
		"ck2_venc_ck"/* parent */, 12),
	GATE_VEN20_V(CLK_VEN2_CKE3_JPGDEC_JPGDEC, "ven2_jpgdec_jpgdec",
		"ven2_jpgdec"/* parent */),
	GATE_HWV_VEN20(CLK_VEN2_CKE5_GALS, "ven2_gals",
		"ck2_venc_ck"/* parent */, 28),
	GATE_VEN20_V(CLK_VEN2_CKE5_GALS_VENC, "ven2_gals_venc",
		"ven2_gals"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE5_GALS_JPGENC, "ven2_gals_jpgenc",
		"ven2_gals"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE5_GALS_JPGDEC, "ven2_gals_jpgdec",
		"ven2_gals"/* parent */),
	GATE_HWV_VEN20(CLK_VEN2_CKE29_VENC_XPC_CTRL, "ven2_venc_xpc_ctrl",
		"ck2_venc_ck"/* parent */, 30),
	GATE_VEN20_V(CLK_VEN2_CKE29_VENC_XPC_CTRL_VENC, "ven2_venc_xpc_ctrl_venc",
		"ven2_venc_xpc_ctrl"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE29_VENC_XPC_CTRL_JPGENC, "ven2_venc_xpc_ctrl_jpgenc",
		"ven2_venc_xpc_ctrl"/* parent */),
	GATE_VEN20_V(CLK_VEN2_CKE29_VENC_XPC_CTRL_JPGDEC, "ven2_venc_xpc_ctrl_jpgdec",
		"ven2_venc_xpc_ctrl"/* parent */),
	GATE_HWV_VEN20(CLK_VEN2_CKE6_GALS_SRAM, "ven2_gals_sram",
		"ck2_venc_ck"/* parent */, 31),
	GATE_VEN20_V(CLK_VEN2_CKE6_GALS_SRAM_VENC, "ven2_gals_sram_venc",
		"ven2_gals_sram"/* parent */),
	/* VEN21 */
	GATE_HWV_VEN21(CLK_VEN2_RES_FLAT, "ven2_res_flat",
		"ck2_venc_ck"/* parent */, 0),
	GATE_VEN21_V(CLK_VEN2_RES_FLAT_VENC, "ven2_res_flat_venc",
		"ven2_res_flat"/* parent */),
	GATE_VEN21_V(CLK_VEN2_RES_FLAT_JPGENC, "ven2_res_flat_jpgenc",
		"ven2_res_flat"/* parent */),
	GATE_VEN21_V(CLK_VEN2_RES_FLAT_JPGDEC, "ven2_res_flat_jpgdec",
		"ven2_res_flat"/* parent */),
};

static const struct mtk_clk_desc ven2_mcd = {
	.clks = ven2_clks,
	.num_clks = CLK_VEN2_NR_CLK,
};

static const struct mtk_gate_regs ven_c20_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven_c20_hwv_regs = {
	.set_ofs = 0x00D8,
	.clr_ofs = 0x00DC,
	.sta_ofs = 0x2C6C,
};

static const struct mtk_gate_regs ven_c21_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x14,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs ven_c21_hwv_regs = {
	.set_ofs = 0x00E0,
	.clr_ofs = 0x00E4,
	.sta_ofs = 0x2C70,
};

#define GATE_VEN_C20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven_c20_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN_C20_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VEN_C20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven_c20_cg_regs,			\
		.hwv_regs = &ven_c20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VEN_C21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven_c21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_VEN_C21_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_VEN_C21(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven_c21_cg_regs,			\
		.hwv_regs = &ven_c21_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven_c2_clks[] = {
	/* VEN_C20 */
	GATE_HWV_VEN_C20(CLK_VEN_C2_CKE0_LARB, "ven_c2_larb",
		"ck2_venc_ck"/* parent */, 0),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE0_LARB_VENC, "ven_c2_larb_venc",
		"ven_c2_larb"/* parent */),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE0_LARB_SMI, "ven_c2_larb_smi",
		"ven_c2_larb"/* parent */),
	GATE_HWV_VEN_C20(CLK_VEN_C2_CKE1_VENC, "ven_c2_venc",
		"ck2_venc_ck"/* parent */, 4),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE1_VENC_VENC, "ven_c2_venc_venc",
		"ven_c2_venc"/* parent */),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE1_VENC_SMI, "ven_c2_venc_smi",
		"ven_c2_venc"/* parent */),
	GATE_HWV_VEN_C20(CLK_VEN_C2_CKE5_GALS, "ven_c2_gals",
		"ck2_venc_ck"/* parent */, 28),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE5_GALS_VENC, "ven_c2_gals_venc",
		"ven_c2_gals"/* parent */),
	GATE_HWV_VEN_C20(CLK_VEN_C2_CKE29_VENC_XPC_CTRL, "ven_c2_venc_xpc_ctrl",
		"ck2_venc_ck"/* parent */, 30),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE29_VENC_XPC_CTRL_VENC, "ven_c2_venc_xpc_ctrl_venc",
		"ven_c2_venc_xpc_ctrl"/* parent */),
	GATE_HWV_VEN_C20(CLK_VEN_C2_CKE6_GALS_SRAM, "ven_c2_gals_sram",
		"ck2_venc_ck"/* parent */, 31),
	GATE_VEN_C20_V(CLK_VEN_C2_CKE6_GALS_SRAM_VENC, "ven_c2_gals_sram_venc",
		"ven_c2_gals_sram"/* parent */),
	/* VEN_C21 */
	GATE_HWV_VEN_C21(CLK_VEN_C2_RES_FLAT, "ven_c2_res_flat",
		"ck2_venc_ck"/* parent */, 0),
	GATE_VEN_C21_V(CLK_VEN_C2_RES_FLAT_VENC, "ven_c2_res_flat_venc",
		"ven_c2_res_flat"/* parent */),
};

static const struct mtk_clk_desc ven_c2_mcd = {
	.clks = ven_c2_clks,
	.num_clks = CLK_VEN_C2_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6991_vcodec[] = {
	{
		.compatible = "mediatek,mt6991-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6991-vdecsys_soc",
		.data = &vde1_mcd,
	}, {
		.compatible = "mediatek,mt6991-vencsys",
		.data = &ven1_mcd,
	}, {
		.compatible = "mediatek,mt6991-vencsys_c1",
		.data = &ven2_mcd,
	}, {
		.compatible = "mediatek,mt6991-vencsys_c2",
		.data = &ven_c2_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6991_vcodec_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6991_vcodec_drv = {
	.probe = clk_mt6991_vcodec_grp_probe,
	.driver = {
		.name = "clk-mt6991-vcodec",
		.of_match_table = of_match_clk_mt6991_vcodec,
	},
};

module_platform_driver(clk_mt6991_vcodec_drv);
MODULE_LICENSE("GPL");
