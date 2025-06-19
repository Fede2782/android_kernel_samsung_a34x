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

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPC_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_I2C11, "impc_i2c11",
		"ck_i2c_p_ck"/* parent */, 0),
	GATE_IMPC_V(CLK_IMPC_I2C11_I2C, "impc_i2c11_i2c",
		"impc_i2c11"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C12, "impc_i2c12",
		"ck_i2c_p_ck"/* parent */, 1),
	GATE_IMPC_V(CLK_IMPC_I2C12_I2C, "impc_i2c12_i2c",
		"impc_i2c12"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C13, "impc_i2c13",
		"ck_i2c_p_ck"/* parent */, 2),
	GATE_IMPC_V(CLK_IMPC_I2C13_I2C, "impc_i2c13_i2c",
		"impc_i2c13"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C14, "impc_i2c14",
		"ck_i2c_p_ck"/* parent */, 3),
	GATE_IMPC_V(CLK_IMPC_I2C14_I2C, "impc_i2c14_i2c",
		"impc_i2c14"/* parent */),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impe_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPE_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate impe_clks[] = {
	GATE_IMPE(CLK_IMPE_I2C5, "impe_i2c5",
		"ck_i2c_east_ck"/* parent */, 0),
	GATE_IMPE_V(CLK_IMPE_I2C5_I2C, "impe_i2c5_i2c",
		"impe_i2c5"/* parent */),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

static const struct mtk_gate_regs impn_hwv_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0004,
	.sta_ofs = 0x2C00,
};

#define GATE_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPN_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.hwv_comp = "hw-voter-regmap",		\
		.regs = &impn_cg_regs,			\
		.hwv_regs = &impn_hwv_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_hwv,		\
		.dma_ops = &mtk_clk_gate_ops_setclr,	\
		.flags = CLK_USE_HW_VOTER,		\
	}

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(CLK_IMPN_I2C1, "impn_i2c1",
		"ck_i2c_north_ck"/* parent */, 0),
	GATE_IMPN_V(CLK_IMPN_I2C1_I2C, "impn_i2c1_i2c",
		"impn_i2c1"/* parent */),
	GATE_IMPN(CLK_IMPN_I2C2, "impn_i2c2",
		"ck_i2c_north_ck"/* parent */, 1),
	GATE_IMPN_V(CLK_IMPN_I2C2_I2C, "impn_i2c2_i2c",
		"impn_i2c2"/* parent */),
	GATE_IMPN(CLK_IMPN_I2C4, "impn_i2c4",
		"ck_i2c_north_ck"/* parent */, 2),
	GATE_IMPN_V(CLK_IMPN_I2C4_I2C, "impn_i2c4_i2c",
		"impn_i2c4"/* parent */),
	GATE_HWV_IMPN(CLK_IMPN_I2C7, "impn_i2c7",
			"ck_i2c_north_ck"/* parent */, 3),
	GATE_IMPN_V(CLK_IMPN_I2C7_I2C, "impn_i2c7_i2c",
		"impn_i2c7"/* parent */),
	GATE_IMPN(CLK_IMPN_I2C8, "impn_i2c8",
		"ck_i2c_north_ck"/* parent */, 4),
	GATE_IMPN_V(CLK_IMPN_I2C8_I2C, "impn_i2c8_i2c",
		"impn_i2c8"/* parent */),
	GATE_IMPN(CLK_IMPN_I2C9, "impn_i2c9",
		"ck_i2c_north_ck"/* parent */, 5),
	GATE_IMPN_V(CLK_IMPN_I2C9_I2C, "impn_i2c9_i2c",
		"impn_i2c9"/* parent */),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
};

static const struct mtk_gate_regs impw_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPW(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impw_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPW_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate impw_clks[] = {
	GATE_IMPW(CLK_IMPW_I2C0, "impw_i2c0",
		"ck_i2c_west_ck"/* parent */, 0),
	GATE_IMPW_V(CLK_IMPW_I2C0_I2C, "impw_i2c0_i2c",
		"impw_i2c0"/* parent */),
	GATE_IMPW(CLK_IMPW_I2C3, "impw_i2c3",
		"ck_i2c_west_ck"/* parent */, 1),
	GATE_IMPW_V(CLK_IMPW_I2C3_I2C, "impw_i2c3_i2c",
		"impw_i2c3"/* parent */),
	GATE_IMPW(CLK_IMPW_I2C6, "impw_i2c6",
		"ck_i2c_west_ck"/* parent */, 2),
	GATE_IMPW_V(CLK_IMPW_I2C6_I2C, "impw_i2c6_i2c",
		"impw_i2c6"/* parent */),
	GATE_IMPW(CLK_IMPW_I2C10, "impw_i2c10",
		"ck_i2c_west_ck"/* parent */, 3),
	GATE_IMPW_V(CLK_IMPW_I2C10_I2C, "impw_i2c10_i2c",
		"impw_i2c10"/* parent */),
};

static const struct mtk_clk_desc impw_mcd = {
	.clks = impw_clks,
	.num_clks = CLK_IMPW_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao1_hwv_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x000C,
	.sta_ofs = 0x2C04,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO0_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_HWV_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",				\
		.regs = &perao1_cg_regs,			\
		.hwv_regs = &perao1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAO_UART0_BCLK, "perao_uart0_bclk",
		"ck_uart_ck"/* parent */, 0),
	GATE_PERAO0_V(CLK_PERAO_UART0_BCLK_UART, "perao_uart0_bclk_uart",
		"perao_uart0_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_UART1_BCLK, "perao_uart1_bclk",
		"ck_uart_ck"/* parent */, 1),
	GATE_PERAO0_V(CLK_PERAO_UART1_BCLK_UART, "perao_uart1_bclk_uart",
		"perao_uart1_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_UART2_BCLK, "perao_uart2_bclk",
		"ck_uart_ck"/* parent */, 2),
	GATE_PERAO0_V(CLK_PERAO_UART2_BCLK_UART, "perao_uart2_bclk_uart",
		"perao_uart2_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_UART3_BCLK, "perao_uart3_bclk",
		"ck_uart_ck"/* parent */, 3),
	GATE_PERAO0_V(CLK_PERAO_UART3_BCLK_UART, "perao_uart3_bclk_uart",
		"perao_uart3_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_UART4_BCLK, "perao_uart4_bclk",
		"ck_uart_ck"/* parent */, 4),
	GATE_PERAO0_V(CLK_PERAO_UART4_BCLK_UART, "perao_uart4_bclk_uart",
		"perao_uart4_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_UART5_BCLK, "perao_uart5_bclk",
		"ck_uart_ck"/* parent */, 5),
	GATE_PERAO0_V(CLK_PERAO_UART5_BCLK_UART, "perao_uart5_bclk_uart",
		"perao_uart5_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_PWM_X16W_HCLK, "perao_pwm_x16w",
		"ck_p_axi_ck"/* parent */, 12),
	GATE_PERAO0_V(CLK_PERAO_PWM_X16W_HCLK_PWM, "perao_pwm_x16w_pwm",
		"perao_pwm_x16w"/* parent */),
	GATE_PERAO0(CLK_PERAO_PWM_X16W_BCLK, "perao_pwm_x16w_bclk",
		"ck_pwm_ck"/* parent */, 13),
	GATE_PERAO0_V(CLK_PERAO_PWM_X16W_BCLK_PWM, "perao_pwm_x16w_bclk_pwm",
		"perao_pwm_x16w_bclk"/* parent */),
	GATE_PERAO0(CLK_PERAO_PWM_PWM_BCLK0, "perao_pwm_pwm_bclk0",
		"ck_pwm_ck"/* parent */, 14),
	GATE_PERAO0_V(CLK_PERAO_PWM_PWM_BCLK0_PWM, "perao_pwm_pwm_bclk0_pwm",
		"perao_pwm_pwm_bclk0"/* parent */),
	GATE_PERAO0(CLK_PERAO_PWM_PWM_BCLK1, "perao_pwm_pwm_bclk1",
		"ck_pwm_ck"/* parent */, 15),
	GATE_PERAO0_V(CLK_PERAO_PWM_PWM_BCLK1_PWM, "perao_pwm_pwm_bclk1_pwm",
		"perao_pwm_pwm_bclk1"/* parent */),
	GATE_PERAO0(CLK_PERAO_PWM_PWM_BCLK2, "perao_pwm_pwm_bclk2",
		"ck_pwm_ck"/* parent */, 16),
	GATE_PERAO0_V(CLK_PERAO_PWM_PWM_BCLK2_PWM, "perao_pwm_pwm_bclk2_pwm",
		"perao_pwm_pwm_bclk2"/* parent */),
	GATE_PERAO0(CLK_PERAO_PWM_PWM_BCLK3, "perao_pwm_pwm_bclk3",
		"ck_pwm_ck"/* parent */, 17),
	GATE_PERAO0_V(CLK_PERAO_PWM_PWM_BCLK3_PWM, "perao_pwm_pwm_bclk3_pwm",
		"perao_pwm_pwm_bclk3"/* parent */),
	/* PERAO1 */
	GATE_HWV_PERAO1(CLK_PERAO_SPI0_BCLK, "perao_spi0_bclk",
			"ck_spi0_b_ck"/* parent */, 0),
	GATE_PERAO1_V(CLK_PERAO_SPI0_BCLK_SPI, "perao_spi0_bclk_spi",
		"perao_spi0_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI1_BCLK, "perao_spi1_bclk",
			"ck_spi1_b_ck"/* parent */, 2),
	GATE_PERAO1_V(CLK_PERAO_SPI1_BCLK_SPI, "perao_spi1_bclk_spi",
		"perao_spi1_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI2_BCLK, "perao_spi2_bclk",
			"ck_spi2_b_ck"/* parent */, 3),
	GATE_PERAO1_V(CLK_PERAO_SPI2_BCLK_SPI, "perao_spi2_bclk_spi",
		"perao_spi2_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI3_BCLK, "perao_spi3_bclk",
			"ck_spi3_b_ck"/* parent */, 4),
	GATE_PERAO1_V(CLK_PERAO_SPI3_BCLK_SPI, "perao_spi3_bclk_spi",
		"perao_spi3_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI4_BCLK, "perao_spi4_bclk",
			"ck_spi4_b_ck"/* parent */, 5),
	GATE_PERAO1_V(CLK_PERAO_SPI4_BCLK_SPI, "perao_spi4_bclk_spi",
		"perao_spi4_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI5_BCLK, "perao_spi5_bclk",
			"ck_spi5_b_ck"/* parent */, 6),
	GATE_PERAO1_V(CLK_PERAO_SPI5_BCLK_SPI, "perao_spi5_bclk_spi",
		"perao_spi5_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI6_BCLK, "perao_spi6_bclk",
			"ck_spi6_b_ck"/* parent */, 7),
	GATE_PERAO1_V(CLK_PERAO_SPI6_BCLK_SPI, "perao_spi6_bclk_spi",
		"perao_spi6_bclk"/* parent */),
	GATE_HWV_PERAO1(CLK_PERAO_SPI7_BCLK, "perao_spi7_bclk",
			"ck_spi7_b_ck"/* parent */, 8),
	GATE_PERAO1_V(CLK_PERAO_SPI7_BCLK_SPI, "perao_spi7_bclk_spi",
		"perao_spi7_bclk"/* parent */),
	GATE_PERAO1(CLK_PERAO_AP_DMA_X32W_BCLK, "perao_ap_dma_x32w_bclk",
		"ck_p_axi_ck"/* parent */, 26),
	GATE_PERAO1_V(CLK_PERAO_AP_DMA_X32W_BCLK_UART, "perao_ap_dma_x32w_bclk_uart",
		"perao_ap_dma_x32w_bclk"/* parent */),
	GATE_PERAO1_V(CLK_PERAO_AP_DMA_X32W_BCLK_I2C, "perao_ap_dma_x32w_bclk_i2c",
		"perao_ap_dma_x32w_bclk"/* parent */),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_MSDC1_MSDC_SRC, "perao_msdc1_msdc_src",
		"ck_msdc30_1_ck"/* parent */, 1),
	GATE_PERAO2_V(CLK_PERAO_MSDC1_MSDC_SRC_MSDC1, "perao_msdc1_msdc_src_msdc1",
		"perao_msdc1_msdc_src"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC1_HCLK, "perao_msdc1",
		"ck_msdc30_1_ck"/* parent */, 2),
	GATE_PERAO2_V(CLK_PERAO_MSDC1_HCLK_MSDC1, "perao_msdc1_msdc1",
		"perao_msdc1"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC1_AXI, "perao_msdc1_axi",
		"ck_p_axi_ck"/* parent */, 3),
	GATE_PERAO2_V(CLK_PERAO_MSDC1_AXI_MSDC1, "perao_msdc1_axi_msdc1",
		"perao_msdc1_axi"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC1_HCLK_WRAP, "perao_msdc1_h_wrap",
		"ck_p_axi_ck"/* parent */, 4),
	GATE_PERAO2_V(CLK_PERAO_MSDC1_HCLK_WRAP_MSDC1, "perao_msdc1_h_wrap_msdc1",
		"perao_msdc1_h_wrap"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC2_MSDC_SRC, "perao_msdc2_msdc_src",
		"ck_msdc30_2_ck"/* parent */, 10),
	GATE_PERAO2_V(CLK_PERAO_MSDC2_MSDC_SRC_MSDC2, "perao_msdc2_msdc_src_msdc2",
		"perao_msdc2_msdc_src"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC2_HCLK, "perao_msdc2",
		"ck_msdc30_2_ck"/* parent */, 11),
	GATE_PERAO2_V(CLK_PERAO_MSDC2_HCLK_MSDC2, "perao_msdc2_msdc2",
		"perao_msdc2"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC2_AXI, "perao_msdc2_axi",
		"ck_p_axi_ck"/* parent */, 12),
	GATE_PERAO2_V(CLK_PERAO_MSDC2_AXI_MSDC2, "perao_msdc2_axi_msdc2",
		"perao_msdc2_axi"/* parent */),
	GATE_PERAO2(CLK_PERAO_MSDC2_HCLK_WRAP, "perao_msdc2_h_wrap",
		"ck_p_axi_ck"/* parent */, 13),
	GATE_PERAO2_V(CLK_PERAO_MSDC2_HCLK_WRAP_MSDC2, "perao_msdc2_h_wrap_msdc2",
		"perao_msdc2_h_wrap"/* parent */),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs pext_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXT(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pext_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PEXT_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate pext_clks[] = {
	GATE_PEXT(CLK_PEXT_PEXTP_MAC_P0_TL, "pext_pm0_tl",
		"ck_tl_ck"/* parent */, 0),
	GATE_PEXT_V(CLK_PEXT_PEXTP_MAC_P0_TL_PCIE, "pext_pm0_tl_pcie",
		"pext_pm0_tl"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_MAC_P0_REF, "pext_pm0_ref",
		"ck_f26m_ck"/* parent */, 1),
	GATE_PEXT_V(CLK_PEXT_PEXTP_MAC_P0_REF_PCIE, "pext_pm0_ref_pcie",
		"pext_pm0_ref"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_PHY_P0_MCU_BUS, "pext_pp0_mcu_bus",
		"ck_f26m_ck"/* parent */, 6),
	GATE_PEXT_V(CLK_PEXT_PEXTP_PHY_P0_MCU_BUS_PCIE, "pext_pp0_mcu_bus_pcie",
		"pext_pp0_mcu_bus"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_PHY_P0_PEXTP_REF, "pext_pp0_pextp_ref",
		"ck_f26m_ck"/* parent */, 7),
	GATE_PEXT_V(CLK_PEXT_PEXTP_PHY_P0_PEXTP_REF_PCIE, "pext_pp0_pextp_ref_pcie",
		"pext_pp0_pextp_ref"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_MAC_P0_AXI_250, "pext_pm0_axi_250",
		"ck_pexpt0_mem_sub_ck"/* parent */, 12),
	GATE_PEXT_V(CLK_PEXT_PEXTP_MAC_P0_AXI_250_PCIE, "pext_pm0_axi_250_pcie",
		"pext_pm0_axi_250"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_MAC_P0_AHB_APB, "pext_pm0_ahb_apb",
		"ck_pextp0_axi_ck"/* parent */, 13),
	GATE_PEXT_V(CLK_PEXT_PEXTP_MAC_P0_AHB_APB_PCIE, "pext_pm0_ahb_apb_pcie",
		"pext_pm0_ahb_apb"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_MAC_P0_PL_P, "pext_pm0_pl_p",
		"ck_f26m_ck"/* parent */, 14),
	GATE_PEXT_V(CLK_PEXT_PEXTP_MAC_P0_PL_P_PCIE, "pext_pm0_pl_p_pcie",
		"pext_pm0_pl_p"/* parent */),
	GATE_PEXT(CLK_PEXT_PEXTP_VLP_AO_P0_LP, "pext_pextp_vlp_ao_p0_lp",
		"ck_f26m_ck"/* parent */, 19),
	GATE_PEXT_V(CLK_PEXT_PEXTP_VLP_AO_P0_LP_PCIE, "pext_pextp_vlp_ao_p0_lp_pcie",
		"pext_pextp_vlp_ao_p0_lp"/* parent */),
};

static const struct mtk_clk_desc pext_mcd = {
	.clks = pext_clks,
	.num_clks = CLK_PEXT_NR_CLK,
};

static const struct mtk_gate_regs pext1_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXT1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pext1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PEXT1_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate pext1_clks[] = {
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P1_TL, "pext1_pm1_tl",
		"ck_tl_p1_ck"/* parent */, 0),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P1_TL_PCIE, "pext1_pm1_tl_pcie",
		"pext1_pm1_tl"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P1_REF, "pext1_pm1_ref",
		"ck_f26m_ck"/* parent */, 1),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P1_REF_PCIE, "pext1_pm1_ref_pcie",
		"pext1_pm1_ref"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P2_TL, "pext1_pm2_tl",
		"ck_tl_p2_ck"/* parent */, 2),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P2_TL_PCIE, "pext1_pm2_tl_pcie",
		"pext1_pm2_tl"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P2_REF, "pext1_pm2_ref",
		"ck_f26m_ck"/* parent */, 3),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P2_REF_PCIE, "pext1_pm2_ref_pcie",
		"pext1_pm2_ref"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_PHY_P1_MCU_BUS, "pext1_pp1_mcu_bus",
		"ck_f26m_ck"/* parent */, 8),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_PHY_P1_MCU_BUS_PCIE, "pext1_pp1_mcu_bus_pcie",
		"pext1_pp1_mcu_bus"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_PHY_P1_PEXTP_REF, "pext1_pp1_pextp_ref",
		"ck_f26m_ck"/* parent */, 9),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_PHY_P1_PEXTP_REF_PCIE, "pext1_pp1_pextp_ref_pcie",
		"pext1_pp1_pextp_ref"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_PHY_P2_MCU_BUS, "pext1_pp2_mcu_bus",
		"ck_f26m_ck"/* parent */, 10),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_PHY_P2_MCU_BUS_PCIE, "pext1_pp2_mcu_bus_pcie",
		"pext1_pp2_mcu_bus"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_PHY_P2_PEXTP_REF, "pext1_pp2_pextp_ref",
		"ck_f26m_ck"/* parent */, 11),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_PHY_P2_PEXTP_REF_PCIE, "pext1_pp2_pextp_ref_pcie",
		"pext1_pp2_pextp_ref"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P1_AXI_250, "pext1_pm1_axi_250",
		"ck_pextp1_usb_axi_ck"/* parent */, 16),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P1_AXI_250_PCIE, "pext1_pm1_axi_250_pcie",
		"pext1_pm1_axi_250"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P1_AHB_APB, "pext1_pm1_ahb_apb",
		"ck_pextp1_usb_mem_sub_ck"/* parent */, 17),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P1_AHB_APB_PCIE, "pext1_pm1_ahb_apb_pcie",
		"pext1_pm1_ahb_apb"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P1_PL_P, "pext1_pm1_pl_p",
		"ck_f26m_ck"/* parent */, 18),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P1_PL_P_PCIE, "pext1_pm1_pl_p_pcie",
		"pext1_pm1_pl_p"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P2_AXI_250, "pext1_pm2_axi_250",
		"ck_pextp1_usb_axi_ck"/* parent */, 19),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P2_AXI_250_PCIE, "pext1_pm2_axi_250_pcie",
		"pext1_pm2_axi_250"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P2_AHB_APB, "pext1_pm2_ahb_apb",
		"ck_pextp1_usb_mem_sub_ck"/* parent */, 20),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P2_AHB_APB_PCIE, "pext1_pm2_ahb_apb_pcie",
		"pext1_pm2_ahb_apb"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_MAC_P2_PL_P, "pext1_pm2_pl_p",
		"ck_f26m_ck"/* parent */, 21),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_MAC_P2_PL_P_PCIE, "pext1_pm2_pl_p_pcie",
		"pext1_pm2_pl_p"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_VLP_AO_P1_LP, "pext1_pextp_vlp_ao_p1_lp",
		"ck_f26m_ck"/* parent */, 26),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_VLP_AO_P1_LP_PCIE, "pext1_pextp_vlp_ao_p1_lp_pcie",
		"pext1_pextp_vlp_ao_p1_lp"/* parent */),
	GATE_PEXT1(CLK_PEXT1_PEXTP_VLP_AO_P2_LP, "pext1_pextp_vlp_ao_p2_lp",
		"ck_f26m_ck"/* parent */, 27),
	GATE_PEXT1_V(CLK_PEXT1_PEXTP_VLP_AO_P2_LP_PCIE, "pext1_pextp_vlp_ao_p2_lp_pcie",
		"pext1_pextp_vlp_ao_p2_lp"/* parent */),
};

static const struct mtk_clk_desc pext1_mcd = {
	.clks = pext1_clks,
	.num_clks = CLK_PEXT1_NR_CLK,
};

static const struct mtk_gate_regs scp_i3c_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

#define GATE_SCP_I3C(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_i3c_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_SCP_I3C_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate scp_i3c_clks[] = {
	GATE_SCP_I3C(CLK_SCP_I3C_I2C1, "scp_i3c_i2c1",
		"ck_f26m_ck"/* parent */, 1),
	GATE_SCP_I3C_V(CLK_SCP_I3C_I2C1_SCP_I2C, "scp_i3c_i2c1_scp_i2c",
		"scp_i3c_i2c1"/* parent */),
};

static const struct mtk_clk_desc scp_i3c_mcd = {
	.clks = scp_i3c_clks,
	.num_clks = CLK_SCP_I3C_NR_CLK,
};

static const struct mtk_gate_regs ufsao0_cg_regs = {
	.set_ofs = 0x108,
	.clr_ofs = 0x10C,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs ufsao1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSAO0_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_UFSAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSAO1_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ufsao_clks[] = {
	/* UFSAO0 */
	GATE_UFSAO0(CLK_UFSAO_UFSHCI_UFS, "ufsao_ufshci_ufs",
		"ck_ck"/* parent */, 0),
	GATE_UFSAO0_V(CLK_UFSAO_UFSHCI_UFS_UFS, "ufsao_ufshci_ufs_ufs",
		"ufsao_ufshci_ufs"/* parent */),
	GATE_UFSAO0(CLK_UFSAO_UFSHCI_AES, "ufsao_ufshci_aes",
		"ck_aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSAO0_V(CLK_UFSAO_UFSHCI_AES_UFS, "ufsao_ufshci_aes_ufs",
		"ufsao_ufshci_aes"/* parent */),
	/* UFSAO1 */
	GATE_UFSAO1(CLK_UFSAO_UNIPRO_TX_SYM, "ufsao_unipro_tx_sym",
		"ck_f26m_ck"/* parent */, 0),
	GATE_UFSAO1_V(CLK_UFSAO_UNIPRO_TX_SYM_UFS, "ufsao_unipro_tx_sym_ufs",
		"ufsao_unipro_tx_sym"/* parent */),
	GATE_UFSAO1(CLK_UFSAO_UNIPRO_RX_SYM0, "ufsao_unipro_rx_sym0",
		"ck_f26m_ck"/* parent */, 1),
	GATE_UFSAO1_V(CLK_UFSAO_UNIPRO_RX_SYM0_UFS, "ufsao_unipro_rx_sym0_ufs",
		"ufsao_unipro_rx_sym0"/* parent */),
	GATE_UFSAO1(CLK_UFSAO_UNIPRO_RX_SYM1, "ufsao_unipro_rx_sym1",
		"ck_f26m_ck"/* parent */, 2),
	GATE_UFSAO1_V(CLK_UFSAO_UNIPRO_RX_SYM1_UFS, "ufsao_unipro_rx_sym1_ufs",
		"ufsao_unipro_rx_sym1"/* parent */),
	GATE_UFSAO1(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
		"ck_ck"/* parent */, 3),
	GATE_UFSAO1_V(CLK_UFSAO_UNIPRO_SYS_UFS, "ufsao_unipro_sys_ufs",
		"ufsao_unipro_sys"/* parent */),
	GATE_UFSAO1(CLK_UFSAO_UNIPRO_SAP, "ufsao_unipro_sap",
		"ck_f26m_ck"/* parent */, 4),
	GATE_UFSAO1_V(CLK_UFSAO_UNIPRO_SAP_UFS, "ufsao_unipro_sap_ufs",
		"ufsao_unipro_sap"/* parent */),
	GATE_UFSAO1(CLK_UFSAO_PHY_SAP, "ufsao_phy_sap",
		"ck_f26m_ck"/* parent */, 8),
	GATE_UFSAO1_V(CLK_UFSAO_PHY_SAP_UFS, "ufsao_phy_sap_ufs",
		"ufsao_phy_sap"/* parent */),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6991_peri[] = {
	{
		.compatible = "mediatek,mt6991-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6991-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6991-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6991-imp_iic_wrap_w",
		.data = &impw_mcd,
	}, {
		.compatible = "mediatek,mt6991-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6991-pextp0cfg_ao",
		.data = &pext_mcd,
	}, {
		.compatible = "mediatek,mt6991-pextp1cfg_ao",
		.data = &pext1_mcd,
	}, {
		.compatible = "mediatek,mt6991-scp_i3c",
		.data = &scp_i3c_mcd,
	}, {
		.compatible = "mediatek,mt6991-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6991_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6991_peri_drv = {
	.probe = clk_mt6991_peri_grp_probe,
	.driver = {
		.name = "clk-mt6991-peri",
		.of_match_table = of_match_clk_mt6991_peri,
	},
};

module_platform_driver(clk_mt6991_peri_drv);
MODULE_LICENSE("GPL");
