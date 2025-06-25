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

static const struct mtk_gate_regs ifr_mem_cg_regs = {
	.set_ofs = 0xD04,
	.clr_ofs = 0xD08,
	.sta_ofs = 0xD00,
};

#define GATE_IFR_MEM(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr_mem_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR_MEM_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ifr_mem_clks[] = {
	GATE_IFR_MEM(CLK_IFR_MEM_DPMAIF_MAIN, "ifr_mem_dpmaif_main",
		"ck_dpmaif_main_ck"/* parent */, 2),
	GATE_IFR_MEM_V(CLK_IFR_MEM_DPMAIF_MAIN_CCCI, "ifr_mem_dpmaif_main_ccci",
		"ifr_mem_dpmaif_main"/* parent */),
	GATE_IFR_MEM(CLK_IFR_MEM_DPMAIF_26M, "ifr_mem_dpmaif_26m",
		"vlp_infra_26m_ck"/* parent */, 3),
	GATE_IFR_MEM_V(CLK_IFR_MEM_DPMAIF_26M_CCCI, "ifr_mem_dpmaif_26m_ccci",
		"ifr_mem_dpmaif_26m"/* parent */),
};

static const struct mtk_clk_desc ifr_mem_mcd = {
	.clks = ifr_mem_clks,
	.num_clks = CLK_IFR_MEM_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6991_bus[] = {
	{
		.compatible = "mediatek,mt6991-apifrbus_ao_mem_reg",
		.data = &ifr_mem_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6991_bus_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6991_bus_drv = {
	.probe = clk_mt6991_bus_grp_probe,
	.driver = {
		.name = "clk-mt6991-bus",
		.of_match_table = of_match_clk_mt6991_bus,
	},
};

module_platform_driver(clk_mt6991_bus_drv);
MODULE_LICENSE("GPL");
