// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
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

static const struct mtk_gate_regs mm10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm10_hwv_regs = {
	.set_ofs = 0x0010,
	.clr_ofs = 0x0014,
	.sta_ofs = 0x2C08,
};

static const struct mtk_gate_regs mm11_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm11_hwv_regs = {
	.set_ofs = 0x0018,
	.clr_ofs = 0x001C,
	.sta_ofs = 0x2C0C,
};

#define GATE_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM10_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm10_cg_regs,			\
		.hwv_regs = &mm10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM11_V(_id, _name, _parent) {\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm11_cg_regs,			\
		.hwv_regs = &mm11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate mm1_clks[] = {
	/* MM10 */
	GATE_HWV_MM10(CLK_MM1_DISPSYS1_CONFIG, "mm1_dispsys1_config",
		"ck2_disp_ck"/* parent */, 0),
	GATE_MM10_V(CLK_MM1_DISPSYS1_CONFIG_DISP, "mm1_dispsys1_config_disp",
		"mm1_dispsys1_config"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISPSYS1_S_CONFIG, "mm1_dispsys1_s_config",
		"ck2_disp_ck"/* parent */, 1),
	GATE_MM10_V(CLK_MM1_DISPSYS1_S_CONFIG_DISP, "mm1_dispsys1_s_config_disp",
		"mm1_dispsys1_s_config"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_MUTEX0, "mm1_disp_mutex0",
		"ck2_disp_ck"/* parent */, 2),
	GATE_MM10_V(CLK_MM1_DISP_MUTEX0_DISP, "mm1_disp_mutex0_disp",
		"mm1_disp_mutex0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC20, "mm1_disp_dli_async20",
		"ck2_disp_ck"/* parent */, 3),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC20_DISP, "mm1_disp_dli_async20_disp",
		"mm1_disp_dli_async20"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC21, "mm1_disp_dli_async21",
		"ck2_disp_ck"/* parent */, 4),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC21_DISP, "mm1_disp_dli_async21_disp",
		"mm1_disp_dli_async21"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC22, "mm1_disp_dli_async22",
		"ck2_disp_ck"/* parent */, 5),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC22_DISP, "mm1_disp_dli_async22_disp",
		"mm1_disp_dli_async22"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC23, "mm1_disp_dli_async23",
		"ck2_disp_ck"/* parent */, 6),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC23_DISP, "mm1_disp_dli_async23_disp",
		"mm1_disp_dli_async23"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC24, "mm1_disp_dli_async24",
		"ck2_disp_ck"/* parent */, 7),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC24_DISP, "mm1_disp_dli_async24_disp",
		"mm1_disp_dli_async24"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC25, "mm1_disp_dli_async25",
		"ck2_disp_ck"/* parent */, 8),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC25_DISP, "mm1_disp_dli_async25_disp",
		"mm1_disp_dli_async25"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC26, "mm1_disp_dli_async26",
		"ck2_disp_ck"/* parent */, 9),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC26_DISP, "mm1_disp_dli_async26_disp",
		"mm1_disp_dli_async26"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC27, "mm1_disp_dli_async27",
		"ck2_disp_ck"/* parent */, 10),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC27_DISP, "mm1_disp_dli_async27_disp",
		"mm1_disp_dli_async27"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DLI_ASYNC28, "mm1_disp_dli_async28",
		"ck2_disp_ck"/* parent */, 11),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC28_DISP, "mm1_disp_dli_async28_disp",
		"mm1_disp_dli_async28"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_RELAY0, "mm1_disp_relay0",
		"ck2_disp_ck"/* parent */, 12),
	GATE_MM10_V(CLK_MM1_DISP_RELAY0_DISP, "mm1_disp_relay0_disp",
		"mm1_disp_relay0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_RELAY1, "mm1_disp_relay1",
		"ck2_disp_ck"/* parent */, 13),
	GATE_MM10_V(CLK_MM1_DISP_RELAY1_DISP, "mm1_disp_relay1_disp",
		"mm1_disp_relay1"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_RELAY2, "mm1_disp_relay2",
		"ck2_disp_ck"/* parent */, 14),
	GATE_MM10_V(CLK_MM1_DISP_RELAY2_DISP, "mm1_disp_relay2_disp",
		"mm1_disp_relay2"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_RELAY3, "mm1_disp_relay3",
		"ck2_disp_ck"/* parent */, 15),
	GATE_MM10_V(CLK_MM1_DISP_RELAY3_DISP, "mm1_disp_relay3_disp",
		"mm1_disp_relay3"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DP_INTF0, "mm1_DP_CLK",
		"ck2_disp_ck"/* parent */, 16),
	GATE_MM10_V(CLK_MM1_DISP_DP_INTF0_DISP, "mm1_dp_clk_disp",
		"mm1_DP_CLK"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DP_INTF1, "mm1_disp_dp_intf1",
		"ck2_disp_ck"/* parent */, 17),
	GATE_MM10_V(CLK_MM1_DISP_DP_INTF1_DISP, "mm1_disp_dp_intf1_disp",
		"mm1_disp_dp_intf1"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSC_WRAP0, "mm1_disp_dsc_wrap0",
		"ck2_disp_ck"/* parent */, 18),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP0_DISP, "mm1_disp_dsc_wrap0_disp",
		"mm1_disp_dsc_wrap0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSC_WRAP1, "mm1_disp_dsc_wrap1",
		"ck2_disp_ck"/* parent */, 19),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP1_DISP, "mm1_disp_dsc_wrap1_disp",
		"mm1_disp_dsc_wrap1"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSC_WRAP2, "mm1_disp_dsc_wrap2",
		"ck2_disp_ck"/* parent */, 20),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP2_DISP, "mm1_disp_dsc_wrap2_disp",
		"mm1_disp_dsc_wrap2"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSC_WRAP3, "mm1_disp_dsc_wrap3",
		"ck2_disp_ck"/* parent */, 21),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP3_DISP, "mm1_disp_dsc_wrap3_disp",
		"mm1_disp_dsc_wrap3"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSI0, "mm1_CLK0",
		"ck2_disp_ck"/* parent */, 22),
	GATE_MM10_V(CLK_MM1_DISP_DSI0_DISP, "mm1_clk0_disp",
		"mm1_CLK0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSI1, "mm1_CLK1",
		"ck2_disp_ck"/* parent */, 23),
	GATE_MM10_V(CLK_MM1_DISP_DSI1_DISP, "mm1_clk1_disp",
		"mm1_CLK1"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DSI2, "mm1_CLK2",
		"ck2_disp_ck"/* parent */, 24),
	GATE_MM10_V(CLK_MM1_DISP_DSI2_DISP, "mm1_clk2_disp",
		"mm1_CLK2"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_DVO0, "mm1_disp_dvo0",
		"ck2_disp_ck"/* parent */, 25),
	GATE_MM10_V(CLK_MM1_DISP_DVO0_DISP, "mm1_disp_dvo0_disp",
		"mm1_disp_dvo0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_GDMA0, "mm1_disp_gdma0",
		"ck2_disp_ck"/* parent */, 26),
	GATE_MM10_V(CLK_MM1_DISP_GDMA0_DISP, "mm1_disp_gdma0_disp",
		"mm1_disp_gdma0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_MERGE0, "mm1_disp_merge0",
		"ck2_disp_ck"/* parent */, 27),
	GATE_MM10_V(CLK_MM1_DISP_MERGE0_DISP, "mm1_disp_merge0_disp",
		"mm1_disp_merge0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_MERGE1, "mm1_disp_merge1",
		"ck2_disp_ck"/* parent */, 28),
	GATE_MM10_V(CLK_MM1_DISP_MERGE1_DISP, "mm1_disp_merge1_disp",
		"mm1_disp_merge1"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_MERGE2, "mm1_disp_merge2",
		"ck2_disp_ck"/* parent */, 29),
	GATE_MM10_V(CLK_MM1_DISP_MERGE2_DISP, "mm1_disp_merge2_disp",
		"mm1_disp_merge2"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_ODDMR0, "mm1_disp_oddmr0",
		"ck2_disp_ck"/* parent */, 30),
	GATE_MM10_V(CLK_MM1_DISP_ODDMR0_PQ, "mm1_disp_oddmr0_pq",
		"mm1_disp_oddmr0"/* parent */),
	GATE_HWV_MM10(CLK_MM1_DISP_POSTALIGN0, "mm1_disp_postalign0",
		"ck2_disp_ck"/* parent */, 31),
	GATE_MM10_V(CLK_MM1_DISP_POSTALIGN0_PQ, "mm1_disp_postalign0_pq",
		"mm1_disp_postalign0"/* parent */),
	/* MM11 */
	GATE_HWV_MM11(CLK_MM1_DISP_DITHER2, "mm1_disp_dither2",
		"ck2_disp_ck"/* parent */, 0),
	GATE_MM11_V(CLK_MM1_DISP_DITHER2_PQ, "mm1_disp_dither2_pq",
		"mm1_disp_dither2"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_R2Y0, "mm1_disp_r2y0",
		"ck2_disp_ck"/* parent */, 1),
	GATE_MM11_V(CLK_MM1_DISP_R2Y0_DISP, "mm1_disp_r2y0_disp",
		"mm1_disp_r2y0"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_SPLITTER0, "mm1_disp_splitter0",
		"ck2_disp_ck"/* parent */, 2),
	GATE_MM11_V(CLK_MM1_DISP_SPLITTER0_DISP, "mm1_disp_splitter0_disp",
		"mm1_disp_splitter0"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_SPLITTER1, "mm1_disp_splitter1",
		"ck2_disp_ck"/* parent */, 3),
	GATE_MM11_V(CLK_MM1_DISP_SPLITTER1_DISP, "mm1_disp_splitter1_disp",
		"mm1_disp_splitter1"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_SPLITTER2, "mm1_disp_splitter2",
		"ck2_disp_ck"/* parent */, 4),
	GATE_MM11_V(CLK_MM1_DISP_SPLITTER2_DISP, "mm1_disp_splitter2_disp",
		"mm1_disp_splitter2"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_SPLITTER3, "mm1_disp_splitter3",
		"ck2_disp_ck"/* parent */, 5),
	GATE_MM11_V(CLK_MM1_DISP_SPLITTER3_DISP, "mm1_disp_splitter3_disp",
		"mm1_disp_splitter3"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_VDCM0, "mm1_disp_vdcm0",
		"ck2_disp_ck"/* parent */, 6),
	GATE_MM11_V(CLK_MM1_DISP_VDCM0_DISP, "mm1_disp_vdcm0_disp",
		"mm1_disp_vdcm0"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_WDMA1, "mm1_disp_wdma1",
		"ck2_disp_ck"/* parent */, 7),
	GATE_MM11_V(CLK_MM1_DISP_WDMA1_DISP, "mm1_disp_wdma1_disp",
		"mm1_disp_wdma1"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_WDMA2, "mm1_disp_wdma2",
		"ck2_disp_ck"/* parent */, 8),
	GATE_MM11_V(CLK_MM1_DISP_WDMA2_DISP, "mm1_disp_wdma2_disp",
		"mm1_disp_wdma2"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_WDMA3, "mm1_disp_wdma3",
		"ck2_disp_ck"/* parent */, 9),
	GATE_MM11_V(CLK_MM1_DISP_WDMA3_DISP, "mm1_disp_wdma3_disp",
		"mm1_disp_wdma3"/* parent */),
	GATE_HWV_MM11(CLK_MM1_DISP_WDMA4, "mm1_disp_wdma4",
		"ck2_disp_ck"/* parent */, 10),
	GATE_MM11_V(CLK_MM1_DISP_WDMA4_DISP, "mm1_disp_wdma4_disp",
		"mm1_disp_wdma4"/* parent */),
	GATE_HWV_MM11(CLK_MM1_MDP_RDMA1, "mm1_mdp_rdma1",
		"ck2_disp_ck"/* parent */, 11),
	GATE_MM11_V(CLK_MM1_MDP_RDMA1_DISP, "mm1_mdp_rdma1_disp",
		"mm1_mdp_rdma1"/* parent */),
	GATE_HWV_MM11(CLK_MM1_SMI_LARB0, "mm1_smi_larb0",
		"ck2_disp_ck"/* parent */, 12),
	GATE_MM11_V(CLK_MM1_SMI_LARB0_SMI, "mm1_smi_larb0_smi",
		"mm1_smi_larb0"/* parent */),
	GATE_HWV_MM11(CLK_MM1_MOD1, "mm1_mod1",
			"ck_f26m_ck"/* parent */, 13),
	GATE_MM11_V(CLK_MM1_MOD1_DISP, "mm1_mod1_disp",
		"mm1_mod1"/* parent */),
	GATE_HWV_MM11(CLK_MM1_MOD2, "mm1_mod2",
		"ck_f26m_ck"/* parent */, 14),
	GATE_MM11_V(CLK_MM1_MOD2_DISP, "mm1_mod2_disp",
		"mm1_mod2"/* parent */),
	GATE_HWV_MM11(CLK_MM1_MOD3, "mm1_mod3",
		"ck_f26m_ck"/* parent */, 15),
	GATE_MM11_V(CLK_MM1_MOD3_DISP, "mm1_mod3_disp",
		"mm1_mod3"/* parent */),
	GATE_HWV_MM11(CLK_MM1_MOD4, "mm1_mod4",
		"ck2_dp0_ck"/* parent */, 16),
	GATE_MM11_V(CLK_MM1_MOD4_DISP, "mm1_mod4_disp",
		"mm1_mod4"/* parent */),
	GATE_HWV_MM11(CLK_MM1_MOD5, "mm1_mod5",
		"ck2_dp1_ck"/* parent */, 17),
	GATE_MM11_V(CLK_MM1_MOD5_DISP, "mm1_mod5_disp",
		"mm1_mod5"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG0, "mm1_cg0",
		"ck2_disp_ck"/* parent */, 20),
	GATE_MM11_V(CLK_MM1_CK_CG0_DISP, "mm1_cg0_disp",
		"mm1_cg0"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG1, "mm1_cg1",
		"ck2_disp_ck"/* parent */, 21),
	GATE_MM11_V(CLK_MM1_CK_CG1_DISP, "mm1_cg1_disp",
		"mm1_cg1"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG2, "mm1_cg2",
		"ck2_disp_ck"/* parent */, 22),
	GATE_MM11_V(CLK_MM1_CK_CG2_DISP, "mm1_cg2_disp",
		"mm1_cg2"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG3, "mm1_cg3",
		"ck2_disp_ck"/* parent */, 23),
	GATE_MM11_V(CLK_MM1_CK_CG3_DISP, "mm1_cg3_disp",
		"mm1_cg3"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG4, "mm1_cg4",
		"ck2_disp_ck"/* parent */, 24),
	GATE_MM11_V(CLK_MM1_CK_CG4_DISP, "mm1_cg4_disp",
		"mm1_cg4"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG5, "mm1_cg5",
		"ck2_disp_ck"/* parent */, 25),
	GATE_MM11_V(CLK_MM1_CK_CG5_DISP, "mm1_cg5_disp",
		"mm1_cg5"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG6, "mm1_cg6",
		"ck2_disp_ck"/* parent */, 26),
	GATE_MM11_V(CLK_MM1_CK_CG6_DISP, "mm1_cg6_disp",
		"mm1_cg6"/* parent */),
	GATE_HWV_MM11(CLK_MM1_CK_CG7, "mm1_cg7",
		"ck2_disp_ck"/* parent */, 27),
	GATE_MM11_V(CLK_MM1_CK_CG7_DISP, "mm1_cg7_disp",
		"mm1_cg7"/* parent */),
	GATE_HWV_MM11(CLK_MM1_F26M, "mm1_f26m_ck",
		"ck_f26m_ck"/* parent */, 28),
	GATE_MM11_V(CLK_MM1_F26M_DISP, "mm1_f26m_ck_disp",
		"mm1_f26m_ck"/* parent */),
};

static const struct mtk_clk_desc mm1_mcd = {
	.clks = mm1_clks,
	.num_clks = CLK_MM1_NR_CLK,
};

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm0_hwv_regs = {
	.set_ofs = 0x0020,
	.clr_ofs = 0x0024,
	.sta_ofs = 0x2C10,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm1_hwv_regs = {
	.set_ofs = 0x0028,
	.clr_ofs = 0x002C,
	.sta_ofs = 0x2C14,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM0_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm0_cg_regs,			\
		.hwv_regs = &mm0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm1_cg_regs,			\
		.hwv_regs = &mm1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_HWV_MM0(CLK_MM_CONFIG, "mm_config",
			"ck2_disp_ck"/* parent */, 0),
	GATE_MM0_V(CLK_MM_CONFIG_DISP, "mm_config_disp",
		"mm_config"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"ck2_disp_ck"/* parent */, 1),
	GATE_MM0_V(CLK_MM_DISP_MUTEX0_DISP, "mm_disp_mutex0_disp",
		"mm_disp_mutex0"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"ck2_disp_ck"/* parent */, 2),
	GATE_MM0_V(CLK_MM_DISP_AAL0_PQ, "mm_disp_aal0_pq",
		"mm_disp_aal0"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_AAL1, "mm_disp_aal1",
			"ck2_disp_ck"/* parent */, 3),
	GATE_MM0_V(CLK_MM_DISP_AAL1_PQ, "mm_disp_aal1_pq",
		"mm_disp_aal1"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
		"ck2_disp_ck"/* parent */, 4),
	GATE_MM0_V(CLK_MM_DISP_C3D0_PQ, "mm_disp_c3d0_pq",
		"mm_disp_c3d0"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D1, "mm_disp_c3d1",
		"ck2_disp_ck"/* parent */, 5),
	GATE_MM0_V(CLK_MM_DISP_C3D1_PQ, "mm_disp_c3d1_pq",
		"mm_disp_c3d1"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D2, "mm_disp_c3d2",
		"ck2_disp_ck"/* parent */, 6),
	GATE_MM0_V(CLK_MM_DISP_C3D2_PQ, "mm_disp_c3d2_pq",
		"mm_disp_c3d2"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D3, "mm_disp_c3d3",
		"ck2_disp_ck"/* parent */, 7),
	GATE_MM0_V(CLK_MM_DISP_C3D3_PQ, "mm_disp_c3d3_pq",
		"mm_disp_c3d3"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
		"ck2_disp_ck"/* parent */, 8),
	GATE_MM0_V(CLK_MM_DISP_CCORR0_PQ, "mm_disp_ccorr0_pq",
		"mm_disp_ccorr0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
		"ck2_disp_ck"/* parent */, 9),
	GATE_MM0_V(CLK_MM_DISP_CCORR1_PQ, "mm_disp_ccorr1_pq",
		"mm_disp_ccorr1"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
		"ck2_disp_ck"/* parent */, 10),
	GATE_MM0_V(CLK_MM_DISP_CCORR2_PQ, "mm_disp_ccorr2_pq",
		"mm_disp_ccorr2"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
		"ck2_disp_ck"/* parent */, 11),
	GATE_MM0_V(CLK_MM_DISP_CCORR3_PQ, "mm_disp_ccorr3_pq",
		"mm_disp_ccorr3"/* parent */),
	GATE_MM0(CLK_MM_DISP_CHIST0, "mm_disp_chist0",
		"ck2_disp_ck"/* parent */, 12),
	GATE_MM0_V(CLK_MM_DISP_CHIST0_PQ, "mm_disp_chist0_pq",
		"mm_disp_chist0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CHIST1, "mm_disp_chist1",
		"ck2_disp_ck"/* parent */, 13),
	GATE_MM0_V(CLK_MM_DISP_CHIST1_PQ, "mm_disp_chist1_pq",
		"mm_disp_chist1"/* parent */),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
		"ck2_disp_ck"/* parent */, 14),
	GATE_MM0_V(CLK_MM_DISP_COLOR0_PQ, "mm_disp_color0_pq",
		"mm_disp_color0"/* parent */),
	GATE_MM0(CLK_MM_DISP_COLOR1, "mm_disp_color1",
		"ck2_disp_ck"/* parent */, 15),
	GATE_MM0_V(CLK_MM_DISP_COLOR1_PQ, "mm_disp_color1_pq",
		"mm_disp_color1"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
		"ck2_disp_ck"/* parent */, 16),
	GATE_MM0_V(CLK_MM_DISP_DITHER0_PQ, "mm_disp_dither0_pq",
		"mm_disp_dither0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
		"ck2_disp_ck"/* parent */, 17),
	GATE_MM0_V(CLK_MM_DISP_DITHER1_PQ, "mm_disp_dither1_pq",
		"mm_disp_dither1"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC0, "mm_disp_dli_async0",
			"ck2_disp_ck"/* parent */, 18),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC0_DISP, "mm_disp_dli_async0_disp",
		"mm_disp_dli_async0"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC1, "mm_disp_dli_async1",
			"ck2_disp_ck"/* parent */, 19),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC1_DISP, "mm_disp_dli_async1_disp",
		"mm_disp_dli_async1"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC2, "mm_disp_dli_async2",
			"ck2_disp_ck"/* parent */, 20),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC2_DISP, "mm_disp_dli_async2_disp",
		"mm_disp_dli_async2"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC3, "mm_disp_dli_async3",
			"ck2_disp_ck"/* parent */, 21),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC3_DISP, "mm_disp_dli_async3_disp",
		"mm_disp_dli_async3"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC4, "mm_disp_dli_async4",
			"ck2_disp_ck"/* parent */, 22),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC4_DISP, "mm_disp_dli_async4_disp",
		"mm_disp_dli_async4"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC5, "mm_disp_dli_async5",
			"ck2_disp_ck"/* parent */, 23),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC5_DISP, "mm_disp_dli_async5_disp",
		"mm_disp_dli_async5"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC6, "mm_disp_dli_async6",
			"ck2_disp_ck"/* parent */, 24),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC6_DISP, "mm_disp_dli_async6_disp",
		"mm_disp_dli_async6"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC7, "mm_disp_dli_async7",
			"ck2_disp_ck"/* parent */, 25),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC7_DISP, "mm_disp_dli_async7_disp",
		"mm_disp_dli_async7"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC8, "mm_disp_dli_async8",
			"ck2_disp_ck"/* parent */, 26),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC8_DISP, "mm_disp_dli_async8_disp",
		"mm_disp_dli_async8"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC9, "mm_disp_dli_async9",
			"ck2_disp_ck"/* parent */, 27),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC9_DISP, "mm_disp_dli_async9_disp",
		"mm_disp_dli_async9"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC10, "mm_disp_dli_async10",
			"ck2_disp_ck"/* parent */, 28),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC10_DISP, "mm_disp_dli_async10_disp",
		"mm_disp_dli_async10"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC11, "mm_disp_dli_async11",
			"ck2_disp_ck"/* parent */, 29),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC11_DISP, "mm_disp_dli_async11_disp",
		"mm_disp_dli_async11"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC12, "mm_disp_dli_async12",
			"ck2_disp_ck"/* parent */, 30),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC12_DISP, "mm_disp_dli_async12_disp",
		"mm_disp_dli_async12"/* parent */),
	GATE_HWV_MM0(CLK_MM_DISP_DLI_ASYNC13, "mm_disp_dli_async13",
			"ck2_disp_ck"/* parent */, 31),
	GATE_MM0_V(CLK_MM_DISP_DLI_ASYNC13_DISP, "mm_disp_dli_async13_disp",
			"mm_disp_dli_async13"/* parent */),
	/* MM1 */
	GATE_HWV_MM1(CLK_MM_DISP_DLI_ASYNC14, "mm_disp_dli_async14",
			"ck2_disp_ck"/* parent */, 0),
	GATE_MM1_V(CLK_MM_DISP_DLI_ASYNC14_DISP, "mm_disp_dli_async14_disp",
		"mm_disp_dli_async14"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLI_ASYNC15, "mm_disp_dli_async15",
			"ck2_disp_ck"/* parent */, 1),
	GATE_MM1_V(CLK_MM_DISP_DLI_ASYNC15_DISP, "mm_disp_dli_async15_disp",
		"mm_disp_dli_async15"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC0, "mm_disp_dlo_async0",
			"ck2_disp_ck"/* parent */, 2),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC0_DISP, "mm_disp_dlo_async0_disp",
		"mm_disp_dlo_async0"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC1, "mm_disp_dlo_async1",
			"ck2_disp_ck"/* parent */, 3),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC1_DISP, "mm_disp_dlo_async1_disp",
		"mm_disp_dlo_async1"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC2, "mm_disp_dlo_async2",
			"ck2_disp_ck"/* parent */, 4),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC2_DISP, "mm_disp_dlo_async2_disp",
		"mm_disp_dlo_async2"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC3, "mm_disp_dlo_async3",
			"ck2_disp_ck"/* parent */, 5),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC3_DISP, "mm_disp_dlo_async3_disp",
		"mm_disp_dlo_async3"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC4, "mm_disp_dlo_async4",
			"ck2_disp_ck"/* parent */, 6),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC4_DISP, "mm_disp_dlo_async4_disp",
		"mm_disp_dlo_async4"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC5, "mm_disp_dlo_async5",
			"ck2_disp_ck"/* parent */, 7),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC5_DISP, "mm_disp_dlo_async5_disp",
		"mm_disp_dlo_async5"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC6, "mm_disp_dlo_async6",
			"ck2_disp_ck"/* parent */, 8),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC6_DISP, "mm_disp_dlo_async6_disp",
		"mm_disp_dlo_async6"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC7, "mm_disp_dlo_async7",
			"ck2_disp_ck"/* parent */, 9),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC7_DISP, "mm_disp_dlo_async7_disp",
		"mm_disp_dlo_async7"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_DLO_ASYNC8, "mm_disp_dlo_async8",
			"ck2_disp_ck"/* parent */, 10),
	GATE_MM1_V(CLK_MM_DISP_DLO_ASYNC8_DISP, "mm_disp_dlo_async8_disp",
		"mm_disp_dlo_async8"/* parent */),
	GATE_MM1(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
		"ck2_disp_ck"/* parent */, 11),
	GATE_MM1_V(CLK_MM_DISP_GAMMA0_PQ, "mm_disp_gamma0_pq",
		"mm_disp_gamma0"/* parent */),
	GATE_MM1(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
		"ck2_disp_ck"/* parent */, 12),
	GATE_MM1_V(CLK_MM_DISP_GAMMA1_PQ, "mm_disp_gamma1_pq",
		"mm_disp_gamma1"/* parent */),
	GATE_MM1(CLK_MM_MDP_AAL0, "mm_mdp_aal0",
		"ck2_disp_ck"/* parent */, 13),
	GATE_MM1_V(CLK_MM_MDP_AAL0_PQ, "mm_mdp_aal0_pq",
		"mm_mdp_aal0"/* parent */),
	GATE_MM1(CLK_MM_MDP_AAL1, "mm_mdp_aal1",
		"ck2_disp_ck"/* parent */, 14),
	GATE_MM1_V(CLK_MM_MDP_AAL1_PQ, "mm_mdp_aal1_pq",
		"mm_mdp_aal1"/* parent */),
	GATE_HWV_MM1(CLK_MM_MDP_RDMA0, "mm_mdp_rdma0",
			"ck2_disp_ck"/* parent */, 15),
	GATE_MM1_V(CLK_MM_MDP_RDMA0_DISP, "mm_mdp_rdma0_disp",
		"mm_mdp_rdma0"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"ck2_disp_ck"/* parent */, 16),
	GATE_MM1_V(CLK_MM_DISP_POSTMASK0_DISP, "mm_disp_postmask0_disp",
		"mm_disp_postmask0"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_POSTMASK1, "mm_disp_postmask1",
			"ck2_disp_ck"/* parent */, 17),
	GATE_MM1_V(CLK_MM_DISP_POSTMASK1_DISP, "mm_disp_postmask1_disp",
		"mm_disp_postmask1"/* parent */),
	GATE_HWV_MM1(CLK_MM_MDP_RSZ0, "mm_mdp_rsz0",
			"ck2_disp_ck"/* parent */, 18),
	GATE_MM1_V(CLK_MM_MDP_RSZ0_DISP, "mm_mdp_rsz0_disp",
		"mm_mdp_rsz0"/* parent */),
	GATE_HWV_MM1(CLK_MM_MDP_RSZ1, "mm_mdp_rsz1",
			"ck2_disp_ck"/* parent */, 19),
	GATE_MM1_V(CLK_MM_MDP_RSZ1_DISP, "mm_mdp_rsz1_disp",
		"mm_mdp_rsz1"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"ck2_disp_ck"/* parent */, 20),
	GATE_MM1_V(CLK_MM_DISP_SPR0_DISP, "mm_disp_spr0_disp",
		"mm_disp_spr0"/* parent */),
	GATE_MM1(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
		"ck2_disp_ck"/* parent */, 21),
	GATE_MM1_V(CLK_MM_DISP_TDSHP0_PQ, "mm_disp_tdshp0_pq",
		"mm_disp_tdshp0"/* parent */),
	GATE_MM1(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
		"ck2_disp_ck"/* parent */, 22),
	GATE_MM1_V(CLK_MM_DISP_TDSHP1_PQ, "mm_disp_tdshp1_pq",
		"mm_disp_tdshp1"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_WDMA0, "mm_disp_wdma0",
			"ck2_disp_ck"/* parent */, 23),
	GATE_MM1_V(CLK_MM_DISP_WDMA0_DISP, "mm_disp_wdma0_disp",
		"mm_disp_wdma0"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_Y2R0, "mm_disp_y2r0",
			"ck2_disp_ck"/* parent */, 24),
	GATE_MM1_V(CLK_MM_DISP_Y2R0_DISP, "mm_disp_y2r0_disp",
		"mm_disp_y2r0"/* parent */),
	GATE_HWV_MM1(CLK_MM_SMI_SUB_COMM0, "mm_ssc",
			"ck2_disp_ck"/* parent */, 25),
	GATE_MM1_V(CLK_MM_SMI_SUB_COMM0_SMI, "mm_ssc_smi",
		"mm_ssc"/* parent */),
	GATE_HWV_MM1(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"ck2_disp_ck"/* parent */, 26),
	GATE_MM1_V(CLK_MM_DISP_FAKE_ENG0_DISP, "mm_disp_fake_eng0_disp",
		"mm_disp_fake_eng0"/* parent */),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs mm_v_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm_v_hwv_regs = {
	.set_ofs = 0x0030,
	.clr_ofs = 0x0034,
	.sta_ofs = 0x2C18,
};

#define GATE_MM_V(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm_v_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM_V_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_MM_V(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm_v_cg_regs,			\
		.hwv_regs = &mm_v_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_MM_V_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm_v_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate mm_v_clks[] = {
	GATE_HWV_MM_V(CLK_MM_V_DISP_VDISP_AO_CONFIG, "mm_v_disp_vdisp_ao_config",
		"ck2_disp_ck"/* parent */, 0),
	GATE_MM_V_V(CLK_MM_V_DISP_VDISP_AO_CONFIG_DISP, "mm_v_disp_vdisp_ao_config_disp",
		"mm_v_disp_vdisp_ao_config"/* parent */),
	GATE_HWV_MM_V(CLK_MM_V_DISP_DPC, "mm_v_disp_dpc",
		"ck2_disp_ck"/* parent */, 1),
	GATE_MM_V_V(CLK_MM_V_DISP_DPC_DISP, "mm_v_disp_dpc_disp",
		"mm_v_disp_dpc"/* parent */),
	GATE_MM_V_DUMMY(CLK_MM_V_SMI_SUB_SOMM0, "mm_v_smi_sub_somm0",
		"ck2_disp_ck"/* parent */, 2),
	GATE_MM_V_V(CLK_MM_V_SMI_SUB_SOMM0_SMI, "mm_v_smi_sub_somm0_smi",
		"mm_v_smi_sub_somm0"/* parent */),
};

static const struct mtk_clk_desc mm_v_mcd = {
	.clks = mm_v_clks,
	.num_clks = CLK_MM_V_NR_CLK,
};


static const struct mtk_gate_regs ovl10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ovl10_hwv_regs = {
	.set_ofs = 0x0050,
	.clr_ofs = 0x0054,
	.sta_ofs = 0x2C28,
};

static const struct mtk_gate_regs ovl11_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs ovl11_hwv_regs = {
	.set_ofs = 0x0058,
	.clr_ofs = 0x005C,
	.sta_ofs = 0x2C2C,
};

#define GATE_OVL10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_OVL10_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_OVL10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl10_cg_regs,			\
		.hwv_regs = &ovl10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_OVL11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_OVL11_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_OVL11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl11_cg_regs,			\
		.hwv_regs = &ovl11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ovl1_clks[] = {
	/* OVL10 */
	GATE_HWV_OVL10(CLK_OVL1_OVLSYS_CONFIG, "ovl1_ovlsys_config",
			"ck2_disp_ck"/* parent */, 0),
	GATE_OVL10_V(CLK_OVL1_OVLSYS_CONFIG_DISP, "ovl1_ovlsys_config_disp",
		"ovl1_ovlsys_config"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_FAKE_ENG0, "ovl1_ovl_fake_eng0",
			"ck2_disp_ck"/* parent */, 1),
	GATE_OVL10_V(CLK_OVL1_OVL_FAKE_ENG0_DISP, "ovl1_ovl_fake_eng0_disp",
		"ovl1_ovl_fake_eng0"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_FAKE_ENG1, "ovl1_ovl_fake_eng1",
			"ck2_disp_ck"/* parent */, 2),
	GATE_OVL10_V(CLK_OVL1_OVL_FAKE_ENG1_DISP, "ovl1_ovl_fake_eng1_disp",
		"ovl1_ovl_fake_eng1"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_MUTEX0, "ovl1_ovl_mutex0",
			"ck2_disp_ck"/* parent */, 3),
	GATE_OVL10_V(CLK_OVL1_OVL_MUTEX0_DISP, "ovl1_ovl_mutex0_disp",
		"ovl1_ovl_mutex0"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA0, "ovl1_ovl_exdma0",
			"ck2_disp_ck"/* parent */, 4),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA0_DISP, "ovl1_ovl_exdma0_disp",
		"ovl1_ovl_exdma0"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA1, "ovl1_ovl_exdma1",
			"ck2_disp_ck"/* parent */, 5),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA1_DISP, "ovl1_ovl_exdma1_disp",
		"ovl1_ovl_exdma1"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA2, "ovl1_ovl_exdma2",
			"ck2_disp_ck"/* parent */, 6),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA2_DISP, "ovl1_ovl_exdma2_disp",
		"ovl1_ovl_exdma2"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA3, "ovl1_ovl_exdma3",
			"ck2_disp_ck"/* parent */, 7),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA3_DISP, "ovl1_ovl_exdma3_disp",
		"ovl1_ovl_exdma3"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA4, "ovl1_ovl_exdma4",
			"ck2_disp_ck"/* parent */, 8),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA4_DISP, "ovl1_ovl_exdma4_disp",
		"ovl1_ovl_exdma4"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA5, "ovl1_ovl_exdma5",
			"ck2_disp_ck"/* parent */, 9),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA5_DISP, "ovl1_ovl_exdma5_disp",
		"ovl1_ovl_exdma5"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA6, "ovl1_ovl_exdma6",
			"ck2_disp_ck"/* parent */, 10),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA6_DISP, "ovl1_ovl_exdma6_disp",
		"ovl1_ovl_exdma6"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA7, "ovl1_ovl_exdma7",
			"ck2_disp_ck"/* parent */, 11),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA7_DISP, "ovl1_ovl_exdma7_disp",
		"ovl1_ovl_exdma7"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA8, "ovl1_ovl_exdma8",
			"ck2_disp_ck"/* parent */, 12),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA8_DISP, "ovl1_ovl_exdma8_disp",
		"ovl1_ovl_exdma8"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_EXDMA9, "ovl1_ovl_exdma9",
			"ck2_disp_ck"/* parent */, 13),
	GATE_OVL10_V(CLK_OVL1_OVL_EXDMA9_DISP, "ovl1_ovl_exdma9_disp",
		"ovl1_ovl_exdma9"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER0, "ovl1_ovl_blender0",
			"ck2_disp_ck"/* parent */, 14),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER0_DISP, "ovl1_ovl_blender0_disp",
		"ovl1_ovl_blender0"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER1, "ovl1_ovl_blender1",
			"ck2_disp_ck"/* parent */, 15),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER1_DISP, "ovl1_ovl_blender1_disp",
		"ovl1_ovl_blender1"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER2, "ovl1_ovl_blender2",
			"ck2_disp_ck"/* parent */, 16),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER2_DISP, "ovl1_ovl_blender2_disp",
		"ovl1_ovl_blender2"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER3, "ovl1_ovl_blender3",
			"ck2_disp_ck"/* parent */, 17),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER3_DISP, "ovl1_ovl_blender3_disp",
		"ovl1_ovl_blender3"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER4, "ovl1_ovl_blender4",
			"ck2_disp_ck"/* parent */, 18),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER4_DISP, "ovl1_ovl_blender4_disp",
		"ovl1_ovl_blender4"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER5, "ovl1_ovl_blender5",
			"ck2_disp_ck"/* parent */, 19),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER5_DISP, "ovl1_ovl_blender5_disp",
		"ovl1_ovl_blender5"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER6, "ovl1_ovl_blender6",
			"ck2_disp_ck"/* parent */, 20),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER6_DISP, "ovl1_ovl_blender6_disp",
		"ovl1_ovl_blender6"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER7, "ovl1_ovl_blender7",
			"ck2_disp_ck"/* parent */, 21),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER7_DISP, "ovl1_ovl_blender7_disp",
		"ovl1_ovl_blender7"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER8, "ovl1_ovl_blender8",
			"ck2_disp_ck"/* parent */, 22),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER8_DISP, "ovl1_ovl_blender8_disp",
		"ovl1_ovl_blender8"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_BLENDER9, "ovl1_ovl_blender9",
			"ck2_disp_ck"/* parent */, 23),
	GATE_OVL10_V(CLK_OVL1_OVL_BLENDER9_DISP, "ovl1_ovl_blender9_disp",
		"ovl1_ovl_blender9"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_OUTPROC0, "ovl1_ovl_outproc0",
			"ck2_disp_ck"/* parent */, 24),
	GATE_OVL10_V(CLK_OVL1_OVL_OUTPROC0_DISP, "ovl1_ovl_outproc0_disp",
		"ovl1_ovl_outproc0"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_OUTPROC1, "ovl1_ovl_outproc1",
			"ck2_disp_ck"/* parent */, 25),
	GATE_OVL10_V(CLK_OVL1_OVL_OUTPROC1_DISP, "ovl1_ovl_outproc1_disp",
		"ovl1_ovl_outproc1"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_OUTPROC2, "ovl1_ovl_outproc2",
			"ck2_disp_ck"/* parent */, 26),
	GATE_OVL10_V(CLK_OVL1_OVL_OUTPROC2_DISP, "ovl1_ovl_outproc2_disp",
		"ovl1_ovl_outproc2"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_OUTPROC3, "ovl1_ovl_outproc3",
			"ck2_disp_ck"/* parent */, 27),
	GATE_OVL10_V(CLK_OVL1_OVL_OUTPROC3_DISP, "ovl1_ovl_outproc3_disp",
		"ovl1_ovl_outproc3"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_OUTPROC4, "ovl1_ovl_outproc4",
			"ck2_disp_ck"/* parent */, 28),
	GATE_OVL10_V(CLK_OVL1_OVL_OUTPROC4_DISP, "ovl1_ovl_outproc4_disp",
		"ovl1_ovl_outproc4"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_OUTPROC5, "ovl1_ovl_outproc5",
			"ck2_disp_ck"/* parent */, 29),
	GATE_OVL10_V(CLK_OVL1_OVL_OUTPROC5_DISP, "ovl1_ovl_outproc5_disp",
		"ovl1_ovl_outproc5"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_MDP_RSZ0, "ovl1_ovl_mdp_rsz0",
			"ck2_disp_ck"/* parent */, 30),
	GATE_OVL10_V(CLK_OVL1_OVL_MDP_RSZ0_DISP, "ovl1_ovl_mdp_rsz0_disp",
		"ovl1_ovl_mdp_rsz0"/* parent */),
	GATE_HWV_OVL10(CLK_OVL1_OVL_MDP_RSZ1, "ovl1_ovl_mdp_rsz1",
			"ck2_disp_ck"/* parent */, 31),
	GATE_OVL10_V(CLK_OVL1_OVL_MDP_RSZ1_DISP, "ovl1_ovl_mdp_rsz1_disp",
		"ovl1_ovl_mdp_rsz1"/* parent */),
	/* OVL11 */
	GATE_HWV_OVL11(CLK_OVL1_OVL_DISP_WDMA0, "ovl1_ovl_disp_wdma0",
			"ck2_disp_ck"/* parent */, 0),
	GATE_OVL11_V(CLK_OVL1_OVL_DISP_WDMA0_DISP, "ovl1_ovl_disp_wdma0_disp",
		"ovl1_ovl_disp_wdma0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVL_DISP_WDMA1, "ovl1_ovl_disp_wdma1",
			"ck2_disp_ck"/* parent */, 1),
	GATE_OVL11_V(CLK_OVL1_OVL_DISP_WDMA1_DISP, "ovl1_ovl_disp_wdma1_disp",
		"ovl1_ovl_disp_wdma1"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVL_UFBC_WDMA0, "ovl1_ovl_ufbc_wdma0",
			"ck2_disp_ck"/* parent */, 2),
	GATE_OVL11_V(CLK_OVL1_OVL_UFBC_WDMA0_DISP, "ovl1_ovl_ufbc_wdma0_disp",
		"ovl1_ovl_ufbc_wdma0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVL_MDP_RDMA0, "ovl1_ovl_mdp_rdma0",
			"ck2_disp_ck"/* parent */, 3),
	GATE_OVL11_V(CLK_OVL1_OVL_MDP_RDMA0_DISP, "ovl1_ovl_mdp_rdma0_disp",
		"ovl1_ovl_mdp_rdma0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVL_MDP_RDMA1, "ovl1_ovl_mdp_rdma1",
			"ck2_disp_ck"/* parent */, 4),
	GATE_OVL11_V(CLK_OVL1_OVL_MDP_RDMA1_DISP, "ovl1_ovl_mdp_rdma1_disp",
		"ovl1_ovl_mdp_rdma1"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVL_BWM0, "ovl1_ovl_bwm0",
			"ck2_disp_ck"/* parent */, 5),
	GATE_OVL11_V(CLK_OVL1_OVL_BWM0_DISP, "ovl1_ovl_bwm0_disp",
		"ovl1_ovl_bwm0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI0, "ovl1_dli0",
			"ck2_disp_ck"/* parent */, 6),
	GATE_OVL11_V(CLK_OVL1_DLI0_DISP, "ovl1_dli0_disp",
		"ovl1_dli0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI1, "ovl1_dli1",
			"ck2_disp_ck"/* parent */, 7),
	GATE_OVL11_V(CLK_OVL1_DLI1_DISP, "ovl1_dli1_disp",
		"ovl1_dli1"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI2, "ovl1_dli2",
			"ck2_disp_ck"/* parent */, 8),
	GATE_OVL11_V(CLK_OVL1_DLI2_DISP, "ovl1_dli2_disp",
		"ovl1_dli2"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI3, "ovl1_dli3",
			"ck2_disp_ck"/* parent */, 9),
	GATE_OVL11_V(CLK_OVL1_DLI3_DISP, "ovl1_dli3_disp",
		"ovl1_dli3"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI4, "ovl1_dli4",
			"ck2_disp_ck"/* parent */, 10),
	GATE_OVL11_V(CLK_OVL1_DLI4_DISP, "ovl1_dli4_disp",
		"ovl1_dli4"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI5, "ovl1_dli5",
			"ck2_disp_ck"/* parent */, 11),
	GATE_OVL11_V(CLK_OVL1_DLI5_DISP, "ovl1_dli5_disp",
		"ovl1_dli5"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI6, "ovl1_dli6",
			"ck2_disp_ck"/* parent */, 12),
	GATE_OVL11_V(CLK_OVL1_DLI6_DISP, "ovl1_dli6_disp",
		"ovl1_dli6"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI7, "ovl1_dli7",
			"ck2_disp_ck"/* parent */, 13),
	GATE_OVL11_V(CLK_OVL1_DLI7_DISP, "ovl1_dli7_disp",
		"ovl1_dli7"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLI8, "ovl1_dli8",
			"ck2_disp_ck"/* parent */, 14),
	GATE_OVL11_V(CLK_OVL1_DLI8_DISP, "ovl1_dli8_disp",
		"ovl1_dli8"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO0, "ovl1_dlo0",
			"ck2_disp_ck"/* parent */, 15),
	GATE_OVL11_V(CLK_OVL1_DLO0_DISP, "ovl1_dlo0_disp",
		"ovl1_dlo0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO1, "ovl1_dlo1",
			"ck2_disp_ck"/* parent */, 16),
	GATE_OVL11_V(CLK_OVL1_DLO1_DISP, "ovl1_dlo1_disp",
		"ovl1_dlo1"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO2, "ovl1_dlo2",
			"ck2_disp_ck"/* parent */, 17),
	GATE_OVL11_V(CLK_OVL1_DLO2_DISP, "ovl1_dlo2_disp",
		"ovl1_dlo2"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO3, "ovl1_dlo3",
			"ck2_disp_ck"/* parent */, 18),
	GATE_OVL11_V(CLK_OVL1_DLO3_DISP, "ovl1_dlo3_disp",
		"ovl1_dlo3"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO4, "ovl1_dlo4",
			"ck2_disp_ck"/* parent */, 19),
	GATE_OVL11_V(CLK_OVL1_DLO4_DISP, "ovl1_dlo4_disp",
		"ovl1_dlo4"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO5, "ovl1_dlo5",
			"ck2_disp_ck"/* parent */, 20),
	GATE_OVL11_V(CLK_OVL1_DLO5_DISP, "ovl1_dlo5_disp",
		"ovl1_dlo5"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO6, "ovl1_dlo6",
			"ck2_disp_ck"/* parent */, 21),
	GATE_OVL11_V(CLK_OVL1_DLO6_DISP, "ovl1_dlo6_disp",
		"ovl1_dlo6"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO7, "ovl1_dlo7",
			"ck2_disp_ck"/* parent */, 22),
	GATE_OVL11_V(CLK_OVL1_DLO7_DISP, "ovl1_dlo7_disp",
		"ovl1_dlo7"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO8, "ovl1_dlo8",
			"ck2_disp_ck"/* parent */, 23),
	GATE_OVL11_V(CLK_OVL1_DLO8_DISP, "ovl1_dlo8_disp",
		"ovl1_dlo8"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO9, "ovl1_dlo9",
			"ck2_disp_ck"/* parent */, 24),
	GATE_OVL11_V(CLK_OVL1_DLO9_DISP, "ovl1_dlo9_disp",
		"ovl1_dlo9"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO10, "ovl1_dlo10",
			"ck2_disp_ck"/* parent */, 25),
	GATE_OVL11_V(CLK_OVL1_DLO10_DISP, "ovl1_dlo10_disp",
		"ovl1_dlo10"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO11, "ovl1_dlo11",
			"ck2_disp_ck"/* parent */, 26),
	GATE_OVL11_V(CLK_OVL1_DLO11_DISP, "ovl1_dlo11_disp",
		"ovl1_dlo11"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_DLO12, "ovl1_dlo12",
			"ck2_disp_ck"/* parent */, 27),
	GATE_OVL11_V(CLK_OVL1_DLO12_DISP, "ovl1_dlo12_disp",
		"ovl1_dlo12"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVLSYS_RELAY0, "ovl1_ovlsys_relay0",
			"ck2_disp_ck"/* parent */, 28),
	GATE_OVL11_V(CLK_OVL1_OVLSYS_RELAY0_DISP, "ovl1_ovlsys_relay0_disp",
		"ovl1_ovlsys_relay0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_OVL_INLINEROT0, "ovl1_ovl_inlinerot0",
			"ck2_disp_ck"/* parent */, 29),
	GATE_OVL11_V(CLK_OVL1_OVL_INLINEROT0_DISP, "ovl1_ovl_inlinerot0_disp",
		"ovl1_ovl_inlinerot0"/* parent */),
	GATE_HWV_OVL11(CLK_OVL1_SMI, "ovl1_smi",
			"ck2_disp_ck"/* parent */, 30),
	GATE_OVL11_V(CLK_OVL1_SMI_SMI, "ovl1_smi_smi",
		"ovl1_smi"/* parent */),
};

static const struct mtk_clk_desc ovl1_mcd = {
	.clks = ovl1_clks,
	.num_clks = CLK_OVL1_NR_CLK,
};

static const struct mtk_gate_regs ovl0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ovl0_hwv_regs = {
	.set_ofs = 0x0060,
	.clr_ofs = 0x0064,
	.sta_ofs = 0x2C30,
};

static const struct mtk_gate_regs ovl1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs ovl1_hwv_regs = {
	.set_ofs = 0x0068,
	.clr_ofs = 0x006C,
	.sta_ofs = 0x2C34,
};

#define GATE_OVL0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_OVL0_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_OVL0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl0_cg_regs,			\
		.hwv_regs = &ovl0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_OVL1_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl1_cg_regs,			\
		.hwv_regs = &ovl1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ovl_clks[] = {
	/* OVL0 */
	GATE_HWV_OVL0(CLK_OVLSYS_CONFIG, "ovlsys_config",
			"ck2_disp_ck"/* parent */, 0),
	GATE_OVL0_V(CLK_OVLSYS_CONFIG_DISP, "ovlsys_config_disp",
		"ovlsys_config"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_FAKE_ENG0, "ovl_fake_eng0",
			"ck2_disp_ck"/* parent */, 1),
	GATE_OVL0_V(CLK_OVL_FAKE_ENG0_DISP, "ovl_fake_eng0_disp",
		"ovl_fake_eng0"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_FAKE_ENG1, "ovl_fake_eng1",
			"ck2_disp_ck"/* parent */, 2),
	GATE_OVL0_V(CLK_OVL_FAKE_ENG1_DISP, "ovl_fake_eng1_disp",
		"ovl_fake_eng1"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_MUTEX0, "ovl_mutex0",
			"ck2_disp_ck"/* parent */, 3),
	GATE_OVL0_V(CLK_OVL_MUTEX0_DISP, "ovl_mutex0_disp",
		"ovl_mutex0"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA0, "ovl_exdma0",
			"ck2_disp_ck"/* parent */, 4),
	GATE_OVL0_V(CLK_OVL_EXDMA0_DISP, "ovl_exdma0_disp",
		"ovl_exdma0"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA1, "ovl_exdma1",
			"ck2_disp_ck"/* parent */, 5),
	GATE_OVL0_V(CLK_OVL_EXDMA1_DISP, "ovl_exdma1_disp",
		"ovl_exdma1"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA2, "ovl_exdma2",
			"ck2_disp_ck"/* parent */, 6),
	GATE_OVL0_V(CLK_OVL_EXDMA2_DISP, "ovl_exdma2_disp",
		"ovl_exdma2"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA3, "ovl_exdma3",
			"ck2_disp_ck"/* parent */, 7),
	GATE_OVL0_V(CLK_OVL_EXDMA3_DISP, "ovl_exdma3_disp",
		"ovl_exdma3"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA4, "ovl_exdma4",
			"ck2_disp_ck"/* parent */, 8),
	GATE_OVL0_V(CLK_OVL_EXDMA4_DISP, "ovl_exdma4_disp",
		"ovl_exdma4"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA5, "ovl_exdma5",
			"ck2_disp_ck"/* parent */, 9),
	GATE_OVL0_V(CLK_OVL_EXDMA5_DISP, "ovl_exdma5_disp",
		"ovl_exdma5"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA6, "ovl_exdma6",
			"ck2_disp_ck"/* parent */, 10),
	GATE_OVL0_V(CLK_OVL_EXDMA6_DISP, "ovl_exdma6_disp",
		"ovl_exdma6"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA7, "ovl_exdma7",
			"ck2_disp_ck"/* parent */, 11),
	GATE_OVL0_V(CLK_OVL_EXDMA7_DISP, "ovl_exdma7_disp",
		"ovl_exdma7"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA8, "ovl_exdma8",
			"ck2_disp_ck"/* parent */, 12),
	GATE_OVL0_V(CLK_OVL_EXDMA8_DISP, "ovl_exdma8_disp",
		"ovl_exdma8"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_EXDMA9, "ovl_exdma9",
			"ck2_disp_ck"/* parent */, 13),
	GATE_OVL0_V(CLK_OVL_EXDMA9_DISP, "ovl_exdma9_disp",
		"ovl_exdma9"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER0, "ovl_blender0",
			"ck2_disp_ck"/* parent */, 14),
	GATE_OVL0_V(CLK_OVL_BLENDER0_DISP, "ovl_blender0_disp",
		"ovl_blender0"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER1, "ovl_blender1",
			"ck2_disp_ck"/* parent */, 15),
	GATE_OVL0_V(CLK_OVL_BLENDER1_DISP, "ovl_blender1_disp",
		"ovl_blender1"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER2, "ovl_blender2",
			"ck2_disp_ck"/* parent */, 16),
	GATE_OVL0_V(CLK_OVL_BLENDER2_DISP, "ovl_blender2_disp",
		"ovl_blender2"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER3, "ovl_blender3",
			"ck2_disp_ck"/* parent */, 17),
	GATE_OVL0_V(CLK_OVL_BLENDER3_DISP, "ovl_blender3_disp",
		"ovl_blender3"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER4, "ovl_blender4",
			"ck2_disp_ck"/* parent */, 18),
	GATE_OVL0_V(CLK_OVL_BLENDER4_DISP, "ovl_blender4_disp",
		"ovl_blender4"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER5, "ovl_blender5",
			"ck2_disp_ck"/* parent */, 19),
	GATE_OVL0_V(CLK_OVL_BLENDER5_DISP, "ovl_blender5_disp",
		"ovl_blender5"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER6, "ovl_blender6",
			"ck2_disp_ck"/* parent */, 20),
	GATE_OVL0_V(CLK_OVL_BLENDER6_DISP, "ovl_blender6_disp",
		"ovl_blender6"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER7, "ovl_blender7",
			"ck2_disp_ck"/* parent */, 21),
	GATE_OVL0_V(CLK_OVL_BLENDER7_DISP, "ovl_blender7_disp",
		"ovl_blender7"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER8, "ovl_blender8",
			"ck2_disp_ck"/* parent */, 22),
	GATE_OVL0_V(CLK_OVL_BLENDER8_DISP, "ovl_blender8_disp",
		"ovl_blender8"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_BLENDER9, "ovl_blender9",
			"ck2_disp_ck"/* parent */, 23),
	GATE_OVL0_V(CLK_OVL_BLENDER9_DISP, "ovl_blender9_disp",
		"ovl_blender9"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_OUTPROC0, "ovl_outproc0",
			"ck2_disp_ck"/* parent */, 24),
	GATE_OVL0_V(CLK_OVL_OUTPROC0_DISP, "ovl_outproc0_disp",
		"ovl_outproc0"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_OUTPROC1, "ovl_outproc1",
			"ck2_disp_ck"/* parent */, 25),
	GATE_OVL0_V(CLK_OVL_OUTPROC1_DISP, "ovl_outproc1_disp",
		"ovl_outproc1"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_OUTPROC2, "ovl_outproc2",
			"ck2_disp_ck"/* parent */, 26),
	GATE_OVL0_V(CLK_OVL_OUTPROC2_DISP, "ovl_outproc2_disp",
		"ovl_outproc2"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_OUTPROC3, "ovl_outproc3",
			"ck2_disp_ck"/* parent */, 27),
	GATE_OVL0_V(CLK_OVL_OUTPROC3_DISP, "ovl_outproc3_disp",
		"ovl_outproc3"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_OUTPROC4, "ovl_outproc4",
			"ck2_disp_ck"/* parent */, 28),
	GATE_OVL0_V(CLK_OVL_OUTPROC4_DISP, "ovl_outproc4_disp",
		"ovl_outproc4"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_OUTPROC5, "ovl_outproc5",
			"ck2_disp_ck"/* parent */, 29),
	GATE_OVL0_V(CLK_OVL_OUTPROC5_DISP, "ovl_outproc5_disp",
		"ovl_outproc5"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_MDP_RSZ0, "ovl_mdp_rsz0",
			"ck2_disp_ck"/* parent */, 30),
	GATE_OVL0_V(CLK_OVL_MDP_RSZ0_DISP, "ovl_mdp_rsz0_disp",
		"ovl_mdp_rsz0"/* parent */),
	GATE_HWV_OVL0(CLK_OVL_MDP_RSZ1, "ovl_mdp_rsz1",
			"ck2_disp_ck"/* parent */, 31),
	GATE_OVL0_V(CLK_OVL_MDP_RSZ1_DISP, "ovl_mdp_rsz1_disp",
		"ovl_mdp_rsz1"/* parent */),
	/* OVL1 */
	GATE_HWV_OVL1(CLK_OVL_DISP_WDMA0, "ovl_disp_wdma0",
			"ck2_disp_ck"/* parent */, 0),
	GATE_OVL1_V(CLK_OVL_DISP_WDMA0_DISP, "ovl_disp_wdma0_disp",
		"ovl_disp_wdma0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DISP_WDMA1, "ovl_disp_wdma1",
			"ck2_disp_ck"/* parent */, 1),
	GATE_OVL1_V(CLK_OVL_DISP_WDMA1_DISP, "ovl_disp_wdma1_disp",
		"ovl_disp_wdma1"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_UFBC_WDMA0, "ovl_ufbc_wdma0",
			"ck2_disp_ck"/* parent */, 2),
	GATE_OVL1_V(CLK_OVL_UFBC_WDMA0_DISP, "ovl_ufbc_wdma0_disp",
		"ovl_ufbc_wdma0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_MDP_RDMA0, "ovl_mdp_rdma0",
			"ck2_disp_ck"/* parent */, 3),
	GATE_OVL1_V(CLK_OVL_MDP_RDMA0_DISP, "ovl_mdp_rdma0_disp",
		"ovl_mdp_rdma0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_MDP_RDMA1, "ovl_mdp_rdma1",
			"ck2_disp_ck"/* parent */, 4),
	GATE_OVL1_V(CLK_OVL_MDP_RDMA1_DISP, "ovl_mdp_rdma1_disp",
		"ovl_mdp_rdma1"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_BWM0, "ovl_bwm0",
			"ck2_disp_ck"/* parent */, 5),
	GATE_OVL1_V(CLK_OVL_BWM0_DISP, "ovl_bwm0_disp",
		"ovl_bwm0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI0, "ovl_dli0",
			"ck2_disp_ck"/* parent */, 6),
	GATE_OVL1_V(CLK_OVL_DLI0_DISP, "ovl_dli0_disp",
		"ovl_dli0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI1, "ovl_dli1",
			"ck2_disp_ck"/* parent */, 7),
	GATE_OVL1_V(CLK_OVL_DLI1_DISP, "ovl_dli1_disp",
		"ovl_dli1"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI2, "ovl_dli2",
			"ck2_disp_ck"/* parent */, 8),
	GATE_OVL1_V(CLK_OVL_DLI2_DISP, "ovl_dli2_disp",
		"ovl_dli2"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI3, "ovl_dli3",
			"ck2_disp_ck"/* parent */, 9),
	GATE_OVL1_V(CLK_OVL_DLI3_DISP, "ovl_dli3_disp",
		"ovl_dli3"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI4, "ovl_dli4",
			"ck2_disp_ck"/* parent */, 10),
	GATE_OVL1_V(CLK_OVL_DLI4_DISP, "ovl_dli4_disp",
		"ovl_dli4"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI5, "ovl_dli5",
			"ck2_disp_ck"/* parent */, 11),
	GATE_OVL1_V(CLK_OVL_DLI5_DISP, "ovl_dli5_disp",
		"ovl_dli5"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI6, "ovl_dli6",
			"ck2_disp_ck"/* parent */, 12),
	GATE_OVL1_V(CLK_OVL_DLI6_DISP, "ovl_dli6_disp",
		"ovl_dli6"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI7, "ovl_dli7",
			"ck2_disp_ck"/* parent */, 13),
	GATE_OVL1_V(CLK_OVL_DLI7_DISP, "ovl_dli7_disp",
		"ovl_dli7"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLI8, "ovl_dli8",
			"ck2_disp_ck"/* parent */, 14),
	GATE_OVL1_V(CLK_OVL_DLI8_DISP, "ovl_dli8_disp",
		"ovl_dli8"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO0, "ovl_dlo0",
			"ck2_disp_ck"/* parent */, 15),
	GATE_OVL1_V(CLK_OVL_DLO0_DISP, "ovl_dlo0_disp",
		"ovl_dlo0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO1, "ovl_dlo1",
			"ck2_disp_ck"/* parent */, 16),
	GATE_OVL1_V(CLK_OVL_DLO1_DISP, "ovl_dlo1_disp",
		"ovl_dlo1"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO2, "ovl_dlo2",
			"ck2_disp_ck"/* parent */, 17),
	GATE_OVL1_V(CLK_OVL_DLO2_DISP, "ovl_dlo2_disp",
		"ovl_dlo2"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO3, "ovl_dlo3",
			"ck2_disp_ck"/* parent */, 18),
	GATE_OVL1_V(CLK_OVL_DLO3_DISP, "ovl_dlo3_disp",
		"ovl_dlo3"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO4, "ovl_dlo4",
			"ck2_disp_ck"/* parent */, 19),
	GATE_OVL1_V(CLK_OVL_DLO4_DISP, "ovl_dlo4_disp",
		"ovl_dlo4"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO5, "ovl_dlo5",
			"ck2_disp_ck"/* parent */, 20),
	GATE_OVL1_V(CLK_OVL_DLO5_DISP, "ovl_dlo5_disp",
		"ovl_dlo5"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO6, "ovl_dlo6",
			"ck2_disp_ck"/* parent */, 21),
	GATE_OVL1_V(CLK_OVL_DLO6_DISP, "ovl_dlo6_disp",
		"ovl_dlo6"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO7, "ovl_dlo7",
			"ck2_disp_ck"/* parent */, 22),
	GATE_OVL1_V(CLK_OVL_DLO7_DISP, "ovl_dlo7_disp",
		"ovl_dlo7"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO8, "ovl_dlo8",
			"ck2_disp_ck"/* parent */, 23),
	GATE_OVL1_V(CLK_OVL_DLO8_DISP, "ovl_dlo8_disp",
		"ovl_dlo8"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO9, "ovl_dlo9",
			"ck2_disp_ck"/* parent */, 24),
	GATE_OVL1_V(CLK_OVL_DLO9_DISP, "ovl_dlo9_disp",
		"ovl_dlo9"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO10, "ovl_dlo10",
			"ck2_disp_ck"/* parent */, 25),
	GATE_OVL1_V(CLK_OVL_DLO10_DISP, "ovl_dlo10_disp",
		"ovl_dlo10"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO11, "ovl_dlo11",
			"ck2_disp_ck"/* parent */, 26),
	GATE_OVL1_V(CLK_OVL_DLO11_DISP, "ovl_dlo11_disp",
		"ovl_dlo11"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_DLO12, "ovl_dlo12",
			"ck2_disp_ck"/* parent */, 27),
	GATE_OVL1_V(CLK_OVL_DLO12_DISP, "ovl_dlo12_disp",
		"ovl_dlo12"/* parent */),
	GATE_HWV_OVL1(CLK_OVLSYS_RELAY0, "ovlsys_relay0",
			"ck2_disp_ck"/* parent */, 28),
	GATE_OVL1_V(CLK_OVLSYS_RELAY0_DISP, "ovlsys_relay0_disp",
		"ovlsys_relay0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_INLINEROT0, "ovl_inlinerot0",
			"ck2_disp_ck"/* parent */, 29),
	GATE_OVL1_V(CLK_OVL_INLINEROT0_DISP, "ovl_inlinerot0_disp",
		"ovl_inlinerot0"/* parent */),
	GATE_HWV_OVL1(CLK_OVL_SMI, "ovl_smi",
		"ck2_disp_ck"/* parent */, 30),
	GATE_OVL1_V(CLK_OVL_SMI_SMI, "ovl_smi_smi",
		"ovl_smi"/* parent */),
};

static const struct mtk_clk_desc ovl_mcd = {
	.clks = ovl_clks,
	.num_clks = CLK_OVL_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6991_mmsys[] = {
	{
		.compatible = "mediatek,mt6991-mmsys1",
		.data = &mm1_mcd,
	}, {
		.compatible = "mediatek,mt6991-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6991-disp_vdisp_ao_config",
		.data = &mm_v_mcd,
	}, {
		.compatible = "mediatek,mt6991-ovlsys1_config",
		.data = &ovl1_mcd,
	}, {
		.compatible = "mediatek,mt6991-ovlsys_config",
		.data = &ovl_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6991_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6991_mmsys_drv = {
	.probe = clk_mt6991_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6991-mmsys",
		.of_match_table = of_match_clk_mt6991_mmsys,
	},
};

module_platform_driver(clk_mt6991_mmsys_drv);
MODULE_LICENSE("GPL");
