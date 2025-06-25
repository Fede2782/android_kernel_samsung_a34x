// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: xiufeng Li <xiufeng.li@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include <dt-bindings/clock/mt6991-ivi-clk.h>

#define CLK_CFG_UPDATE2				0x000c
#define CLK_CFG_19				0x0140
#define CLK_CFG_19_SET				0x0144
#define CLK_CFG_19_CLR				0x0148
#define CLK_CFG_20				0x0150
#define CLK_CFG_20_SET				0x0154
#define CLK_CFG_20_CLR				0x0158
#define CLK_CFG_21				0x0160
#define CLK_CFG_21_SET				0x0164
#define CLK_CFG_21_CLR				0x0168
#define CKSTA_REG2				0x01D0
#define CLK_FENC_STATUS_MON_2			0x0278

#define CKSYS2_CLK_CFG_UPDATE			0x0004
#define CKSYS2_CLK_CFG_3			0x0040
#define CKSYS2_CLK_CFG_3_SET			0x0044
#define CKSYS2_CLK_CFG_3_CLR			0x0048
#define CKSYS2_CLK_CFG_4			0x0050
#define CKSYS2_CLK_CFG_4_SET			0x0054
#define CKSYS2_CLK_CFG_4_CLR			0x0058
#define CKSYS2_CKSTA_REG			0x00F8
#define CKSYS2_CLK_FENC_STATUS_MON_0		0x0174
/* TOPCK MUX SHIFT */
#define TOP_MUX_SGMII0_REF_325M_SHIFT		17
#define TOP_MUX_SGMII0_REG_SHIFT			18
#define TOP_MUX_SGMII1_REF_325M_SHIFT		19
#define TOP_MUX_SGMII1_REG_SHIFT			20
#define TOP_MUX_GMAC_312P5M_SHIFT			21
#define TOP_MUX_GMAC_125M_SHIFT				22
#define TOP_MUX_GMAC_RMII_SHIFT				23
#define TOP_MUX_GMAC_62P4M_SHIFT			24
/* TOPCK2 MUX SHIFT */
#define TOP_MUX_DVO_SHIFT			15
#define TOP_MUX_DVO_FAVT_SHIFT			16
/* APMIXED PLL REG */
#define NET1PLL_CON0				0x2C8
#define NET1PLL_CON1				0x2CC
#define NET1PLL_CON2				0x2D0
#define NET1PLL_CON3				0x2D4
#define SGMIIPLL_CON0				0x2DC
#define SGMIIPLL_CON1				0x2E0
#define SGMIIPLL_CON2				0x2E4
#define SGMIIPLL_CON3				0x2E8

#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

static DEFINE_SPINLOCK(mt6991_ivi_clk_lock);

static const struct mtk_fixed_factor ivi_divs[] = {
	FACTOR(CLK_CK_NET1PLL_D4, "ck_net1pll_d4",
			"net1pll", 1, 4),
	FACTOR(CLK_CK_NET1PLL_D5, "ck_net1pll_d5",
			"net1pll", 1, 5),
	FACTOR(CLK_CK_NET1PLL_D5_D5, "ck_net1pll_d5_d5",
			"net1pll", 1, 25),
	FACTOR(CLK_CK_SGMIIPLL, "ck_sgmiipll_ck",
			"sgmiipll", 1, 1),
	FACTOR(CLK_CK_UNIVPLL_D5_D8, "ck_univpll_d5_d8",
			"univpll", 1, 40),
	FACTOR(CLK_CK_APLL1_D3, "ck_apll1_d3",
			"vlp_apll1", 1, 3),
	FACTOR(CLK_CK_APLL2_D3, "ck_apll2_d3",
			"vlp_apll2", 1, 3),
	FACTOR(CLK_CK_GMAC_312P5M_SEL_V0, "ck_gmac_312p5m_sel_v0",
			"ck_gmac_312p5m_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_312P5M_SEL_V1, "ck_gmac_312p5m_sel_v1",
			"ck_gmac_312p5m_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_125M_SEL_V0, "ck_gmac_125m_sel_v0",
			"ck_gmac_125m_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_125M_SEL_V1, "ck_gmac_125m_sel_v1",
			"ck_gmac_125m_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_RMII_SEL_V0, "ck_gmac_rmii_sel_v0",
			"ck_gmac_rmii_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_RMII_SEL_V1, "ck_gmac_rmii_sel_v1",
			"ck_gmac_rmii_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_62P4M_PTP_SEL_V0, "ck_gmac_62p4m_ptp_sel_v0",
			"ck_gmac_62p4m_ptp_sel", 1, 1),
	FACTOR(CLK_CK_GMAC_62P4M_PTP_SEL_V1, "ck_gmac_62p4m_ptp_sel_v1",
			"ck_gmac_62p4m_ptp_sel", 1, 1),
};

static const char * const ck_sgmii_ref_325m_sel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_sgmiipll_ck",
};

static const char * const ck_sgmii0_sel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4",
};

static const char * const ck_gmac_312p5m_sel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_net1pll_d4",
};

static const char * const ck_gmac_125m_sel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_net1pll_d5",
};

static const char * const ck_gmac_rmii_sel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_net1pll_d5_d5",
};

static const char * const ck_gmac_62p4m_ptp_sel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_apll1_d3",
	"ck_univpll_d5_d8",
	"ck_apll2_d3",
};

static struct mtk_mux ivi_muxes[] = {
#if MT_CCF_MUX_DISABLE
	MUX_CLR_SET_UPD_CHK(CLK_CK_SGMII0_REF_325M_SEL/* dts */, "ck_sgmii0_ref_325m_sel",
		ck_sgmii_ref_325m_sel_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SGMII0_REF_325M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SGMII0_REG_SEL/* dts */, "ck_sgmii0_sel",
		ck_sgmii0_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SGMII0_REG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SGMII1_REF_325M_SEL/* dts */, "ck_sgmii1_ref_325m_sel",
		ck_sgmii_ref_325m_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SGMII1_REF_325M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 12/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SGMII1_REG_SEL/* dts */, "ck_sgmii1_sel",
		ck_sgmii0_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SGMII1_REG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_GMAC_312P5M_SEL/* dts */, "ck_gmac_312p5m_sel",
		ck_gmac_312p5m_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GMAC_312P5M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_GMAC_125M_SEL/* dts */, "ck_gmac_125m_sel",
		ck_gmac_125m_sel_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GMAC_125M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_GMAC_RMII_SEL/* dts */, "ck_gmac_rmii_sel",
		ck_gmac_rmii_sel_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GMAC_RMII_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_GMAC_62P4M_PTP_SEL/* dts */, "ck_gmac_62p4m_ptp_sel",
		ck_gmac_62p4m_ptp_sel_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GMAC_62P4M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 7/* cksta shift */),
#else
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_SGMII0_REF_325M_SEL/* dts */, "ck_sgmii0_ref_325m_sel",
		ck_sgmii_ref_325m_sel_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SGMII0_REF_325M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 14/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 16/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_SGMII0_REG_SEL/* dts */, "ck_sgmii0_sel",
		ck_sgmii0_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SGMII0_REG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 13/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 15/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_SGMII1_REF_325M_SEL/* dts */, "ck_sgmii1_ref_325m_sel",
		ck_sgmii_ref_325m_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SGMII1_REF_325M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 12/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 14/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_SGMII1_REG_SEL/* dts */, "ck_sgmii1_sel",
		ck_sgmii0_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SGMII1_REG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 11/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 13/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_GMAC_312P5M_SEL/* dts */, "ck_gmac_312p5m_sel",
		ck_gmac_312p5m_sel_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_GMAC_312P5M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 10/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 12/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_GMAC_125M_SEL/* dts */, "ck_gmac_125m_sel",
		ck_gmac_125m_sel_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_GMAC_125M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 9/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 11/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_GMAC_RMII_SEL/* dts */, "ck_gmac_rmii_sel",
		ck_gmac_rmii_sel_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_GMAC_RMII_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 8/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 10/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_GMAC_62P4M_PTP_SEL/* dts */, "ck_gmac_62p4m_ptp_sel",
		ck_gmac_62p4m_ptp_sel_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_GMAC_62P4M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 7/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 9/* fenc shift */),
#endif
};

static const struct mtk_fixed_factor ck2_divs[] = {
	FACTOR(CLK_CK2_TVDPLL3_D2, "ck2_tvdpll3_d2",
			"tvdpll3", 1, 2),
	FACTOR(CLK_CK2_TVDPLL3_D4, "ck2_tvdpll3_d4",
			"tvdpll3", 1, 4),
	FACTOR(CLK_CK2_TVDPLL3_D8, "ck2_tvdpll3_d8",
			"tvdpll3", 1, 8),
	FACTOR(CLK_CK2_TVDPLL3_D16, "ck2_tvdpll3_d16",
			"tvdpll3", 1, 16),
};

static const char * const ck2_dvo_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_tvdpll3_d16",
	"ck2_tvdpll3_d8",
	"ck2_tvdpll3_d4",
	"ck2_tvdpll3_d2"
};
static const char * const ck2_dvo_favt_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_tvdpll3_d16",
	"ck2_tvdpll3_d8",
	"ck2_tvdpll3_d4",
	"ck_apll1_ck",
	"ck_apll2_ck",
	"ck2_tvdpll3_d2"
};

static struct mtk_mux ck2_muxes[] = {
#if MT_CCF_MUX_DISABLE
	MUX_CLR_SET_UPD(CLK_CK2_DVO_SEL/* dts */, "ck2_dvo_sel",
		ck2_dvo_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVO_SHIFT/* upd shift */),
	/* CKSYS2_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_CK2_DVO_FAVT_SEL/* dts */, "ck2_dvo_favt_sel",
		ck2_dvo_favt_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVO_FAVT_SHIFT/* upd shift */),
#else
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK2_DVO_SEL/* dts */, "ck2_dvo_sel",
		ck2_dvo_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DVO_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		16/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		16/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK2_DVO_FAVT_SEL/* dts */, "ck2_dvo_favt_sel",
		ck2_dvo_favt_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DVO_FAVT_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		15/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		15/* fenc shift */),

#endif
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
	GATE_HWV_MM11(CLK_MM1_MOD6, "mm1_mod6",
		"ck2_dvo_sel"/* parent */, 18),
};

#define MT6991_PLL_FMAX		(3800UL * MHZ)
#define MT6991_PLL_FMIN		(1500UL * MHZ)
#define MT6991_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define IVI_PLL_SETCLR(_id, _name, _pll_setclr, _en_setclr_bit,		\
			_rstb_setclr_bit, _flags, _pd_reg,		\
			_pd_shift, _tuner_reg, _tuner_en_reg,		\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.pll_setclr = &(_pll_setclr),				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6991_PLL_FMAX,				\
		.fmin = MT6991_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6991_INTEGER_BITS,			\
	}

#define IVI_PLL_FENC(_id, _name, _fenc_sta_ofs, _fenc_sta_bit,		\
			_flags, _pd_reg, _pd_shift,			\
			 _pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.fenc_sta_ofs = _fenc_sta_ofs,				\
		.fenc_sta_bit = _fenc_sta_bit,				\
		.flags = (_flags | PLL_CFLAGS | CLK_FENC_ENABLE),	\
		.fmax = MT6991_PLL_FMAX,				\
		.fmin = MT6991_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6991_INTEGER_BITS,			\
	}

static const struct mtk_pll_data apmixed_plls[] = {
	IVI_PLL_FENC(CLK_APMIXED_NET1PLL, "net1pll",
		0x003C/*fenc*/, 1, 0,
		NET1PLL_CON1, 24/*pd*/,
		NET1PLL_CON1, 0, 22/*pcw*/),
	IVI_PLL_FENC(CLK_APMIXED_SGMIIPLL, "sgmiipll",
		0x003C/*fenc*/, 0, 0,
		SGMIIPLL_CON1, 24/*pd*/,
		SGMIIPLL_CON1, 0, 22/*pcw*/),
};

static int clk_mt6991_ivi_pll_registration(const struct mtk_pll_data *plls,
		struct platform_device *pdev, int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		if (base == IOMEM_ERR_PTR(-EBUSY))
			base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		else {
			pr_info("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}


static int clk_mt6991_ivi_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6991_ivi_pll_registration(apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}


static int clk_mt6991_ivi_cksys_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_CK_IVI_NR_CLK);

	mtk_clk_register_muxes(ivi_muxes, ARRAY_SIZE(ivi_muxes),
					node, &mt6991_ivi_clk_lock, clk_data);
	mtk_clk_register_factors(ivi_divs, ARRAY_SIZE(ivi_divs), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r) {
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
		mtk_free_clk_data(clk_data);
	}

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6991_ivi_cksys2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_CK2_IVI_NR_CLK);

	mtk_clk_register_muxes(ck2_muxes, ARRAY_SIZE(ck2_muxes),
					node, &mt6991_ivi_clk_lock, clk_data);
	mtk_clk_register_factors(ck2_divs, ARRAY_SIZE(ck2_divs), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r) {
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
		mtk_free_clk_data(clk_data);
	}

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6991_ivi_mm1_cg_probe(struct platform_device *pdev)
{
	int r;
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;


	clk_data = mtk_alloc_clk_data(CLK_MM1_IVI_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	r = mtk_clk_register_gates(node, mm1_clks, CLK_MM1_IVI_NR_CLK, clk_data);
	if (r)
		return r;

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt6991_ivi[] = {
	{
		.compatible = "mediatek,mt6991-ivi-apmixedsys",
		.data = clk_mt6991_ivi_apmixed_probe,
	},	{
		.compatible = "mediatek,mt6991-ivi-cksys",
		.data = clk_mt6991_ivi_cksys_probe,
	},	{
		.compatible = "mediatek,mt6991-ivi-cksys2",
		.data = clk_mt6991_ivi_cksys2_probe,
	},	{
		.compatible = "mediatek,mt6991-ivi-mmsys1",
		.data = &clk_mt6991_ivi_mm1_cg_probe,
	},	{
		/* sentinel */
	}
};

static int clk_mt6991_ivi_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6991_ivi_drv = {
	.probe = clk_mt6991_ivi_probe,
	.driver = {
		.name = "clk-mt6991-ivi",
		.of_match_table = of_match_clk_mt6991_ivi,
	},
};

module_platform_driver(clk_mt6991_ivi_drv);
MODULE_LICENSE("GPL");
