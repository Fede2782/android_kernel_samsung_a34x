// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"

#define REG_ADDR(base, x) ((void __iomem *)((unsigned long)base + (x)))

struct match {
	char *compatible;
	struct fh_pll_domain **domain_list;
};

static int init_v1(struct fh_pll_domain *d,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base)
{
	struct fh_pll_data *data;
	struct fh_pll_offset *offset;
	struct fh_pll_regs *regs;
	char *name;

	name = d->name;
	data = d->data;
	offset = d->offset;
	regs = d->regs;

	if (regs->reg_hp_en) {
		FHDBG("domain<%s> inited\n", name);
		return 0;
	}

	FHDBG("init domain<%s>\n", name);
	while (data->dds_mask != 0) {
		int regs_offset;

		/* fhctl common part */
		regs->reg_hp_en = REG_ADDR(fhctl_base,
				offset->offset_hp_en);
		regs->reg_clk_con = REG_ADDR(fhctl_base,
				offset->offset_clk_con);
		regs->reg_rst_con = REG_ADDR(fhctl_base,
				offset->offset_rst_con);
		regs->reg_slope0 = REG_ADDR(fhctl_base,
				offset->offset_slope0);
		regs->reg_slope1 = REG_ADDR(fhctl_base,
				offset->offset_slope1);
		/* If SET/CLR register defined */
		if (offset->offset_hp_en_set != 0) {
			regs->reg_hp_en_set = REG_ADDR(fhctl_base,
					offset->offset_hp_en_set);
			regs->reg_hp_en_clr = REG_ADDR(fhctl_base,
					offset->offset_hp_en_clr);
			regs->reg_rst_con_set = REG_ADDR(fhctl_base,
					offset->offset_rst_con_set);
			regs->reg_rst_con_clr = REG_ADDR(fhctl_base,
					offset->offset_rst_con_clr);
			regs->reg_clk_con_set = REG_ADDR(fhctl_base,
					offset->offset_clk_con_set);
			regs->reg_clk_con_clr = REG_ADDR(fhctl_base,
					offset->offset_clk_con_clr);
		} else {
			regs->reg_hp_en_set = NULL;
			regs->reg_hp_en_clr = NULL;
			regs->reg_rst_con_set = NULL;
			regs->reg_rst_con_clr = NULL;
			regs->reg_clk_con_set = NULL;
			regs->reg_clk_con_clr = NULL;
		}

		/* fhctl pll part */
		regs_offset = offset->offset_fhctl + offset->offset_cfg;
		regs->reg_cfg = REG_ADDR(fhctl_base, regs_offset);
		regs->reg_updnlmt = REG_ADDR(regs->reg_cfg,
				offset->offset_updnlmt);
		regs->reg_dds = REG_ADDR(regs->reg_cfg,
				offset->offset_dds);
		regs->reg_dvfs = REG_ADDR(regs->reg_cfg,
				offset->offset_dvfs);
		regs->reg_mon = REG_ADDR(regs->reg_cfg,
				offset->offset_mon);

		/* apmixed part */
		regs->reg_con_pcw = REG_ADDR(apmixed_base,
				offset->offset_con_pcw);
		regs->reg_con_postdiv = REG_ADDR(apmixed_base,
				offset->offset_con_postdiv);

		FHDBG("pll<%s>, dds_mask<%d>, data<%lx> offset<%lx> regs<%lx> %s\n",
				data->name, data->dds_mask, (unsigned long)data, (unsigned long)offset,
				(unsigned long)regs, regs->reg_hp_en_set?"Support SET/CLR":"");
		data++;
		offset++;
		regs++;
	}

	return 0;
}

/* 6761 begin */
#define SIZE_6761_TOP (sizeof(mt6761_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6761_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6761_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x4,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6761_top_data[] = {
	DATA_6761_TOP("armpll"),
	DATA_6761_TOP("mainpll"),
	DATA_6761_TOP("msdcpll"),
	DATA_6761_TOP("mfgpll"),
	DATA_6761_TOP("mempll"),
	DATA_6761_TOP("mpll"),
	DATA_6761_TOP("mmpll"),
	{}
};
static struct fh_pll_offset mt6761_top_offset[SIZE_6761_TOP] = {
	OFFSET_6761_TOP(0x003C, 0x0310),	// FHCTL0_CFG, ARMPLL
	OFFSET_6761_TOP(0x0050, 0x022C),    // FHCTL1_CFG, MAINPLL
	OFFSET_6761_TOP(0x0064, 0x0354),    // FHCTL2_CFG, MSDCPLL
	OFFSET_6761_TOP(0x0078, 0x021C),    // FHCTL3_CFG, MFGPLL
	OFFSET_6761_TOP(0x008C, 0xdeb1),    // FHCTL4_CFG, MEMPLL
	OFFSET_6761_TOP(0x00A0, 0x0344),    // FHCTL5_CFG, MPLL
	OFFSET_6761_TOP(0x00B4, 0x0334),    // FHCTL6_CFG, MMPLL
	{}
};
static struct fh_pll_regs mt6761_top_regs[SIZE_6761_TOP];
static struct fh_pll_domain mt6761_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6761_top_data,
	.offset = (struct fh_pll_offset *)&mt6761_top_offset,
	.regs = (struct fh_pll_regs *)&mt6761_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6761_domain[] = {
	&mt6761_top,
	NULL
};
static struct match mt6761_match = {
	.compatible = "mediatek,mt6761-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6761_domain,
};
/* 6761 end */

/* 6765 begin */
#define SIZE_6765_TOP (sizeof(mt6765_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6765_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6765_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x4,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6765_top_data[] = {
	DATA_6765_TOP("armpll"),
	DATA_6765_TOP("mainpll"),
	DATA_6765_TOP("msdcpll"),
	DATA_6765_TOP("mfgpll"),
	DATA_6765_TOP("mempll"),
	DATA_6765_TOP("mpll"),
	DATA_6765_TOP("mmpll"),
	DATA_6765_TOP("armpll_l"),
	DATA_6765_TOP("ccipll"),
	{}
};
static struct fh_pll_offset mt6765_top_offset[SIZE_6765_TOP] = {
	OFFSET_6765_TOP(0x003C, 0x0210),	// FHCTL0_CFG, ARMPLL
	OFFSET_6765_TOP(0x0050, 0x0240),    // FHCTL1_CFG, MAINPLL
	OFFSET_6765_TOP(0x0064, 0x0280),    // FHCTL2_CFG, MSDCPLL
	OFFSET_6765_TOP(0x0078, 0x0250),    // FHCTL3_CFG, MFGPLL
	OFFSET_6765_TOP(0x008C, 0xffff),    // FHCTL4_CFG, MEMPLL
	OFFSET_6765_TOP(0x00A0, 0x02A4),    // FHCTL5_CFG, MPLL
	OFFSET_6765_TOP(0x00B4, 0x0260),    // FHCTL6_CFG, MMPLL
	OFFSET_6765_TOP(0x00C8, 0x0220),    // FHCTL7_CFG, ARMPLL_L
	OFFSET_6765_TOP(0x00DC, 0x0230),    // FHCTL8_CFG, CCIPLL
	{}
};
static struct fh_pll_regs mt6765_top_regs[SIZE_6765_TOP];
static struct fh_pll_domain mt6765_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6765_top_data,
	.offset = (struct fh_pll_offset *)&mt6765_top_offset,
	.regs = (struct fh_pll_regs *)&mt6765_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6765_domain[] = {
	&mt6765_top,
	NULL
};
static struct match mt6765_match = {
	.compatible = "mediatek,mt6765-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6765_domain,
};
/* 6765 end */

/* platform data begin */
/* 6768 begin */
#define SIZE_6768_TOP (sizeof(mt6768_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6768_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),	\
		.postdiv_offset = 24,				\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6768_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,			\
		.offset_hp_en = 0x4,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6768_top_data[] = {
	DATA_6768_TOP("armpll"),
	DATA_6768_TOP("mainpll"),
	DATA_6768_TOP("msdcpll"),
	DATA_6768_TOP("mfgpll"),
	DATA_6768_TOP("mempll"),
	DATA_6768_TOP("mpll"),
	DATA_6768_TOP("mmpll"),
	DATA_6768_TOP("armpll_l"),
	DATA_6768_TOP("ccipll"),
	{}
};
static struct fh_pll_offset mt6768_top_offset[SIZE_6768_TOP] = {
	OFFSET_6768_TOP(0x003C, 0x020C),	// FHCTL0_CFG, ARMPLL
	OFFSET_6768_TOP(0x0050, 0x025C),    // FHCTL1_CFG, MAINPLL
	OFFSET_6768_TOP(0x0064, 0x0340),    // FHCTL2_CFG, MSDCPLL
	OFFSET_6768_TOP(0x0078, 0x024C),    // FHCTL3_CFG, MFGPLL
	OFFSET_6768_TOP(0x008C, 0xffff),    // FHCTL4_CFG, MEMPLL
	OFFSET_6768_TOP(0x00A0, 0x0330),    // FHCTL5_CFG, MPLL
	OFFSET_6768_TOP(0x00B4, 0x0320),    // FHCTL6_CFG, MMPLL
	OFFSET_6768_TOP(0x00C8, 0x021C),    // FHCTL7_CFG, ARMPLL_L
	OFFSET_6768_TOP(0x00DC, 0x022C),    // FHCTL8_CFG, CCIPLL
	{}
};
static struct fh_pll_regs mt6768_top_regs[SIZE_6768_TOP];
static struct fh_pll_domain mt6768_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6768_top_data,
	.offset = (struct fh_pll_offset *)&mt6768_top_offset,
	.regs = (struct fh_pll_regs *)&mt6768_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6768_domain[] = {
	&mt6768_top,
	NULL
};
static struct match mt6768_match = {
	.compatible = "mediatek,mt6768-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6768_domain,
};
/* 6768 end */
/* 6781 begin */
#define SIZE_6781_TOP (sizeof(mt6781_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6781_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6781_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6781_top_data[] = {
	DATA_6781_TOP("armpll_ll"),
	DATA_6781_TOP("armpll_bl"),
	DATA_6781_TOP("apupll"),
	DATA_6781_TOP("ccipll"),
	DATA_6781_TOP("gpupll"),
	DATA_6781_TOP("mpll"),
	DATA_6781_TOP("mempll"),
	DATA_6781_TOP("mainpll"),
	DATA_6781_TOP("msdcpll"),
	DATA_6781_TOP("mmpll"),
	DATA_6781_TOP("adsppll"),
	DATA_6781_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6781_top_offset[SIZE_6781_TOP] = {
	OFFSET_6781_TOP(0x0038, 0x0204),/*ARMPLL_LL */
	OFFSET_6781_TOP(0x004C, 0x0214),/*ARMPLL_BL */
	OFFSET_6781_TOP(0x0060, 0x02F0), /*APUPLL */
	OFFSET_6781_TOP(0x0074, 0x02A4), /*CCIPLL */
	OFFSET_6781_TOP(0x0088, 0x0254), /*GPUPLL */
	OFFSET_6781_TOP(0x009C, 0x0294), /*MPLL */
	OFFSET_6781_TOP(0x00B0, 0xffff), /*MEMPLL */
	OFFSET_6781_TOP(0x00C4, 0x0234), /*MAINPLL */
	OFFSET_6781_TOP(0x00D8, 0x0264), /*MSDCPLL */
	OFFSET_6781_TOP(0x00EC, 0x0284), /*MMPLL */
	OFFSET_6781_TOP(0x0100, 0x02B4), /*ADSPLL */
	OFFSET_6781_TOP(0x0114, 0x0274), /*TVDPLL */
	{}
};
static struct fh_pll_regs mt6781_top_regs[SIZE_6781_TOP];
static struct fh_pll_domain mt6781_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6781_top_data,
	.offset = (struct fh_pll_offset *)&mt6781_top_offset,
	.regs = (struct fh_pll_regs *)&mt6781_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6781_domain[] = {
	&mt6781_top,
	NULL
};
static struct match mt6781_match = {
	.compatible = "mediatek,mt6781-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6781_domain,
};
/* 6781 end */
/* 6853 begin */
#define SIZE_6853_TOP (sizeof(mt6853_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6853_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6853_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6853_top_data[] = {
	DATA_6853_TOP("armpll_ll"),
	DATA_6853_TOP("armpll_bl0"),
	DATA_6853_TOP("armpll_bl1"),
	DATA_6853_TOP("armpll_bl2"),
	DATA_6853_TOP("npupll"),
	DATA_6853_TOP("ccipll"),
	DATA_6853_TOP("mfgpll"),
	DATA_6853_TOP("mempll"),
	DATA_6853_TOP("mpll"),
	DATA_6853_TOP("mmpll"),
	DATA_6853_TOP("mainpll"),
	DATA_6853_TOP("msdcpll"),
	DATA_6853_TOP("adsppll"),
	DATA_6853_TOP("apupll"),
	DATA_6853_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6853_top_offset[SIZE_6853_TOP] = {
	OFFSET_6853_TOP(0x003C, 0x020C),
	OFFSET_6853_TOP(0x0050, 0x021C),
	OFFSET_6853_TOP(0x0064, 0x022C),
	OFFSET_6853_TOP(0x0078, 0x023C),
	OFFSET_6853_TOP(0x008C, 0x03B8),
	OFFSET_6853_TOP(0x00A0, 0x025C),
	OFFSET_6853_TOP(0x00B4, 0x026C),
	OFFSET_6853_TOP(0x00C8, 0xffff),
	OFFSET_6853_TOP(0x00DC, 0x0394),
	OFFSET_6853_TOP(0x00F0, 0x0364),
	OFFSET_6853_TOP(0x0104, 0x0344),
	OFFSET_6853_TOP(0x0118, 0x0354),
	OFFSET_6853_TOP(0x012c, 0x0374),
	OFFSET_6853_TOP(0x0140, 0xffff),
	OFFSET_6853_TOP(0x0154, 0x0384),
	{}
};
static struct fh_pll_regs mt6853_top_regs[SIZE_6853_TOP];
static struct fh_pll_domain mt6853_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6853_top_data,
	.offset = (struct fh_pll_offset *)&mt6853_top_offset,
	.regs = (struct fh_pll_regs *)&mt6853_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6853_domain[] = {
	&mt6853_top,
	NULL
};
static struct match mt6853_match = {
	.compatible = "mediatek,mt6853-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6853_domain,
};
/* 6853 end */
/* platform data begin */
/* 6877 begin */
#define SIZE_6877_TOP (sizeof(mt6877_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6877_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6877_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6877_top_data[] = {
	DATA_6877_TOP("armpll_ll"),
	DATA_6877_TOP("armpll_bl0"),
	DATA_6877_TOP("armpll_b"),
	DATA_6877_TOP("ccipll"),
	DATA_6877_TOP("mempll"),
	DATA_6877_TOP("emipll"),
	DATA_6877_TOP("mpll"),
	DATA_6877_TOP("mmpll"),
	DATA_6877_TOP("mainpll"),
	DATA_6877_TOP("msdcpll"),
	DATA_6877_TOP("adsppll"),
	DATA_6877_TOP("imgpll"),
	DATA_6877_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6877_top_offset[SIZE_6877_TOP] = {
	OFFSET_6877_TOP(0x003C, 0x020C),
	OFFSET_6877_TOP(0x0050, 0x021C),
	OFFSET_6877_TOP(0x0064, 0x022C),
	OFFSET_6877_TOP(0x0078, 0x023C),
	OFFSET_6877_TOP(0x008C, 0xffff),
	OFFSET_6877_TOP(0x00A0, 0x03B4),
	OFFSET_6877_TOP(0x00B4, 0x0394),
	OFFSET_6877_TOP(0x00C8, 0x03A4),
	OFFSET_6877_TOP(0x00DC, 0x0354),
	OFFSET_6877_TOP(0x00F0, 0x0364),
	OFFSET_6877_TOP(0x0104, 0x0384),
	OFFSET_6877_TOP(0x0118, 0x0374),
	OFFSET_6877_TOP(0x012c, 0x024c),
	{}
};

#define SIZE_6877_GPU (sizeof(mt6877_gpu_data)\
	/sizeof(struct fh_pll_data))

static struct fh_pll_data mt6877_gpu_data[] = {
	DATA_6877_TOP("mfg_ao_mfgpll1"),
	DATA_6877_TOP("mfg_ao_mfgpll4"),
	{}
};
static struct fh_pll_offset mt6877_gpu_offset[] = {
	OFFSET_6877_TOP(0x003C, 0x000C),  // PLL4H_FHCTL0_CFG, PLL4H_PLL1_CON1
	OFFSET_6877_TOP(0x0078, 0x003C),  // PLL4HPLL_FHCTL3_CFG, PLL4H_PLL4_CON1
	{}
};

#define SIZE_6877_APU (sizeof(mt6877_apu_data)\
	/sizeof(struct fh_pll_data))

static struct fh_pll_data mt6877_apu_data[] = {
	DATA_6877_TOP("apupll"),
	DATA_6877_TOP("npupll"),
	DATA_6877_TOP("apupll1"),
	DATA_6877_TOP("apupll2"),
	{}
};
static struct fh_pll_offset mt6877_apu_offset[] = {
	OFFSET_6877_TOP(0x003C, 0x000C),  // PLL4HPLL_FHCTL0_CFG, PLL4H_PLL1_CON1
	OFFSET_6877_TOP(0x0050, 0x001C),  // PLL4HPLL_FHCTL1_CFG, PLL4H_PLL2_CON1
	OFFSET_6877_TOP(0x0064, 0x002C),  // PLL4HPLL_FHCTL2_CFG, PLL4H_PLL3_CON1
	OFFSET_6877_TOP(0x0078, 0x003C),  // PLL4HPLL_FHCTL3_CFG, PLL4H_PLL4_CON1
	{}
};
static struct fh_pll_regs mt6877_top_regs[SIZE_6877_TOP];
static struct fh_pll_regs mt6877_gpu_regs[SIZE_6877_GPU];
static struct fh_pll_regs mt6877_apu_regs[SIZE_6877_APU];
static struct fh_pll_domain mt6877_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6877_top_data,
	.offset = (struct fh_pll_offset *)&mt6877_top_offset,
	.regs = (struct fh_pll_regs *)&mt6877_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain mt6877_gpu = {
	.name = "gpu",
	.data = (struct fh_pll_data *)&mt6877_gpu_data,
	.offset = (struct fh_pll_offset *)&mt6877_gpu_offset,
	.regs = (struct fh_pll_regs *)&mt6877_gpu_regs,
	.init = &init_v1,
};
static struct fh_pll_domain mt6877_apu = {
	.name = "apu",
	.data = (struct fh_pll_data *)&mt6877_apu_data,
	.offset = (struct fh_pll_offset *)&mt6877_apu_offset,
	.regs = (struct fh_pll_regs *)&mt6877_apu_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6877_domain[] = {
	&mt6877_top,
	&mt6877_gpu,
	&mt6877_apu,
	NULL
};
static struct match mt6877_match = {
	.compatible = "mediatek,mt6877-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6877_domain,
};
/* 6877 end */

/* platform data begin */
/* 6885 begin */
#define SIZE_6885_TOP (sizeof(mt6885_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6885_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6885_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6885_top_data[] = {
	DATA_6885_TOP("armpll_ll"),
	DATA_6885_TOP("armpll_bl0"),
	DATA_6885_TOP("armpll_bl1"),
	DATA_6885_TOP("armpll_bl2"),
	DATA_6885_TOP("armpll_bl3"),
	DATA_6885_TOP("ccipll"),
	DATA_6885_TOP("mfgpll"),
	DATA_6885_TOP("mempll"),
	DATA_6885_TOP("mpll"),
	DATA_6885_TOP("mmpll"),
	DATA_6885_TOP("mainpll"),
	DATA_6885_TOP("msdcpll"),
	DATA_6885_TOP("adsppll"),
	DATA_6885_TOP("apupll"),
	DATA_6885_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6885_top_offset[SIZE_6885_TOP] = {
	OFFSET_6885_TOP(0x003C, 0x020C),
	OFFSET_6885_TOP(0x0050, 0x021C),
	OFFSET_6885_TOP(0x0064, 0x022C),
	OFFSET_6885_TOP(0x0078, 0x023C),
	OFFSET_6885_TOP(0x008C, 0x024C),
	OFFSET_6885_TOP(0x00A0, 0x025C),
	OFFSET_6885_TOP(0x00B4, 0x026C),
	OFFSET_6885_TOP(0x00C8, 0xffff),
	OFFSET_6885_TOP(0x00DC, 0x0394),
	OFFSET_6885_TOP(0x00F0, 0x0364),
	OFFSET_6885_TOP(0x0104, 0x0344),
	OFFSET_6885_TOP(0x0118, 0x0354),
	OFFSET_6885_TOP(0x012c, 0x0374),
	OFFSET_6885_TOP(0x0140, 0x03A4),
	OFFSET_6885_TOP(0x0154, 0x0384),
	{}
};
static struct fh_pll_regs mt6885_top_regs[SIZE_6885_TOP];
static struct fh_pll_domain mt6885_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6885_top_data,
	.offset = (struct fh_pll_offset *)&mt6885_top_offset,
	.regs = (struct fh_pll_regs *)&mt6885_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6885_domain[] = {
	&mt6885_top,
	NULL
};
static struct match mt6885_match = {
	.compatible = "mediatek,mt6885-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6885_domain,
};
/* 6885 end */
/* 6897 begin */
#define SIZE_6897_TOP (sizeof(mt6897_top_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_GPU0 (sizeof(mt6897_gpu0_data)\
	/sizeof(struct fh_pll_data))
/*
#define SIZE_6897_GPU1 (sizeof(mt6897_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_GPU2 (sizeof(mt6897_gpu2_data)\
	/sizeof(struct fh_pll_data))
*/
#define SIZE_6897_GPU3 (sizeof(mt6897_gpu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU0 (sizeof(mt6897_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU1 (sizeof(mt6897_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU2 (sizeof(mt6897_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU3 (sizeof(mt6897_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU4 (sizeof(mt6897_mcu4_data)\
	/sizeof(struct fh_pll_data))


#define DATA_6897_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),	\
		.postdiv_offset = 24,				\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6897_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_hp_en_set = 0x168,			\
		.offset_hp_en_clr = 0x16c,			\
		.offset_clk_con = 0x8,				\
		.offset_clk_con_set = 0x170,		\
		.offset_clk_con_clr = 0x174,		\
		.offset_rst_con = 0xc,				\
		.offset_rst_con_set = 0x178,		\
		.offset_rst_con_clr = 0x17c,		\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6897_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6897_top_data[] = {
/*
	DATA_6897_CONVERT("mainpll2"),
	DATA_6897_CONVERT("mmpll2"),
*/
	DATA_6897_CONVERT("nouse-mempll"),
	DATA_6897_CONVERT("nouse-emipll"),
	DATA_6897_CONVERT("mpll"),
	DATA_6897_CONVERT("mmpll"),
	DATA_6897_CONVERT("mainpll"),
	DATA_6897_CONVERT("msdcpll"),
	DATA_6897_CONVERT("adsppll"),
	DATA_6897_CONVERT("imgpll"),
	DATA_6897_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_offset mt6897_top_offset[SIZE_6897_TOP] = {
/*
	REG_6897_CONVERT(0x003C, 0x0284),  //	DATA_6897_CONVERT("mainpll2"),
	REG_6897_CONVERT(0x0050, 0x02A4),  //	DATA_6897_CONVERT("mmpll2"),
*/
	REG_6897_CONVERT(0x0064, 0xffff),  //	DATA_6897_CONVERT("mempll"),
	REG_6897_CONVERT(0x0078, 0x03B4),  //	DATA_6897_CONVERT("emipll"),
	REG_6897_CONVERT(0x008C, 0x0394),  //	DATA_6897_CONVERT("mpll"),
	REG_6897_CONVERT(0x00A0, 0x03A4),  //	DATA_6897_CONVERT("mmpll"),
	REG_6897_CONVERT(0x00B4, 0x0354),  //	DATA_6897_CONVERT("mainpll"),
	REG_6897_CONVERT(0x00C8, 0x0364),  //	DATA_6897_CONVERT("msdcpll "),
	REG_6897_CONVERT(0x00DC, 0x0384),  //	DATA_6897_CONVERT("adsppll"),
	REG_6897_CONVERT(0x00F0, 0x0374),  //	DATA_6897_CONVERT("imgpll"),
	REG_6897_CONVERT(0x0104, 0x024c),  //	DATA_6897_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_regs mt6897_top_regs[SIZE_6897_TOP];
static struct fh_pll_domain mt6897_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6897_top_data,
	.offset = (struct fh_pll_offset *)&mt6897_top_offset,
	.regs = (struct fh_pll_regs *)&mt6897_top_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu0
static struct fh_pll_data mt6897_gpu0_data[] = {
	DATA_6897_CONVERT("mfgpll"),
	{}
};
static struct fh_pll_offset mt6897_gpu0_offset[SIZE_6897_GPU0] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu0_regs[SIZE_6897_GPU0];
static struct fh_pll_domain mt6897_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6897_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu0_regs,
	.init = &init_v1,
};

/*
///////////////////////////////////gpu1
static struct fh_pll_data mt6897_gpu1_data[] = {
	DATA_6897_CONVERT("mfgnr"),
	{}
};
static struct fh_pll_offset mt6897_gpu1_offset[SIZE_6897_GPU1] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu1_regs[SIZE_6897_GPU1];
static struct fh_pll_domain mt6897_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6897_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6897_gpu2_data[] = {
	DATA_6897_CONVERT("gpuebpll"),
	{}
};
static struct fh_pll_offset mt6897_gpu2_offset[SIZE_6897_GPU2] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu2_regs[SIZE_6897_GPU2];
static struct fh_pll_domain mt6897_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6897_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu2_regs,
	.init = &init_v1,
};
*/

///////////////////////////////////gpu3
static struct fh_pll_data mt6897_gpu3_data[] = {
	DATA_6897_CONVERT("mfgscpll"),
	{}
};
static struct fh_pll_offset mt6897_gpu3_offset[SIZE_6897_GPU3] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu3_regs[SIZE_6897_GPU3];
static struct fh_pll_domain mt6897_gpu3 = {
	.name = "gpu3",
	.data = (struct fh_pll_data *)&mt6897_gpu3_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu3_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu3_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu0
static struct fh_pll_data mt6897_mcu0_data[] = {
	DATA_6897_CONVERT("buspll"),
	{}
};
static struct fh_pll_offset mt6897_mcu0_offset[SIZE_6897_MCU0] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu0_regs[SIZE_6897_MCU0];
static struct fh_pll_domain mt6897_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6897_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu0_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu1
static struct fh_pll_data mt6897_mcu1_data[] = {
	DATA_6897_CONVERT("cpu0pll"),
	{}
};
static struct fh_pll_offset mt6897_mcu1_offset[SIZE_6897_MCU1] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu1_regs[SIZE_6897_MCU1];
static struct fh_pll_domain mt6897_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6897_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6897_mcu2_data[] = {
	DATA_6897_CONVERT("cpu1pll"),
	{}
};
static struct fh_pll_offset mt6897_mcu2_offset[SIZE_6897_MCU2] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu2_regs[SIZE_6897_MCU2];
static struct fh_pll_domain mt6897_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6897_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6897_mcu3_data[] = {
	DATA_6897_CONVERT("cpu2pll"),
	{}
};
static struct fh_pll_offset mt6897_mcu3_offset[SIZE_6897_MCU3] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu3_regs[SIZE_6897_MCU3];
static struct fh_pll_domain mt6897_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6897_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu3_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu4
static struct fh_pll_data mt6897_mcu4_data[] = {
	DATA_6897_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6897_mcu4_offset[SIZE_6897_MCU4] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu4_regs[SIZE_6897_MCU4];
static struct fh_pll_domain mt6897_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6897_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6897_domain[] = {
	&mt6897_top,
	&mt6897_gpu0,
/*
	&mt6897_gpu1,
	&mt6897_gpu2,
*/
	&mt6897_gpu3,
	&mt6897_mcu0,
	&mt6897_mcu1,
	&mt6897_mcu2,
	&mt6897_mcu3,
	&mt6897_mcu4,
	NULL
};
static struct match mt6897_match = {
	.compatible = "mediatek,mt6897-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6897_domain,
};
/* 6897 end */


/* 6985 begin */
#define SIZE_6985_TOP0 (sizeof(mt6985_top0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_TOP1 (sizeof(mt6985_top1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU0 (sizeof(mt6985_gpu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU1 (sizeof(mt6985_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU2 (sizeof(mt6985_gpu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU3 (sizeof(mt6985_gpu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU0 (sizeof(mt6985_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU1 (sizeof(mt6985_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU2 (sizeof(mt6985_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU3 (sizeof(mt6985_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU4 (sizeof(mt6985_mcu4_data)\
	/sizeof(struct fh_pll_data))


#define DATA_6985_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),		    \
		.postdiv_offset = 24,				        \
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6985_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw, 	\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6985_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw, 	\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6985_top0_data[] = {
	DATA_6985_CONVERT("mainpll2"),
	DATA_6985_CONVERT("mmpll2"),
	DATA_6985_CONVERT("mempll"),
	DATA_6985_CONVERT("emipll"),
	DATA_6985_CONVERT("mpll"),
	DATA_6985_CONVERT("mmpll"),
	DATA_6985_CONVERT("mainpll"),
	DATA_6985_CONVERT("msdcpll"),
	DATA_6985_CONVERT("adsppll"),
	DATA_6985_CONVERT("imgpll"),
	DATA_6985_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_offset mt6985_top0_offset[SIZE_6985_TOP0] = {
	REG_6985_CONVERT(0x003C, 0x0284),  //	DATA_6985_CONVERT("mainpll2"),
	REG_6985_CONVERT(0x0050, 0x02A4),  //	DATA_6985_CONVERT("mmpll2"),
	REG_6985_CONVERT(0x0064, 0xffff),  //	DATA_6985_CONVERT("mempll"),
	REG_6985_CONVERT(0x0078, 0x03B4),  //	DATA_6985_CONVERT("emipll"),
	REG_6985_CONVERT(0x008C, 0x0394),  //	DATA_6985_CONVERT("mpll"),
	REG_6985_CONVERT(0x00A0, 0x03A4),  //	DATA_6985_CONVERT("mmpll"),
	REG_6985_CONVERT(0x00B4, 0x0354),  //	DATA_6985_CONVERT("mainpll"),
	REG_6985_CONVERT(0x00C8, 0x0364),  //	DATA_6985_CONVERT("msdcpll "),
	REG_6985_CONVERT(0x00DC, 0x0384),  //	DATA_6985_CONVERT("adsppll"),
	REG_6985_CONVERT(0x00F0, 0x0374),  //	DATA_6985_CONVERT("imgpll"),
	REG_6985_CONVERT(0x0104, 0x024c),  //	DATA_6985_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_regs mt6985_top0_regs[SIZE_6985_TOP0];
static struct fh_pll_domain mt6985_top0 = {
	.name = "top0",
	.data = (struct fh_pll_data *)&mt6985_top0_data,
	.offset = (struct fh_pll_offset *)&mt6985_top0_offset,
	.regs = (struct fh_pll_regs *)&mt6985_top0_regs,
	.init = &init_v1,
};

///////////////////////////////////top1
static struct fh_pll_data mt6985_top1_data[] = {
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("imgpll"),
	{}
};
static struct fh_pll_offset mt6985_top1_offset[SIZE_6985_TOP1] = {
	REG_6985_CONVERT(0x003C, 0x0284),  //	DATA_6985_CONVERT("mainpll2"),
	REG_6985_CONVERT(0x0050, 0x02A4),  //	DATA_6985_CONVERT("mmpll2"),
	REG_6985_CONVERT(0x0064, 0xffff),  //	DATA_6985_CONVERT("mempll"),
	REG_6985_CONVERT(0x0078, 0x03B4),  //	DATA_6985_CONVERT("emipll"),
	REG_6985_CONVERT(0x008C, 0x0394),  //	DATA_6985_CONVERT("mpll"),
	REG_6985_CONVERT(0x00A0, 0x03A4),  //	DATA_6985_CONVERT("mmpll"),
	REG_6985_CONVERT(0x00B4, 0x0354),  //	DATA_6985_CONVERT("mainpll"),
	REG_6985_CONVERT(0x00C8, 0x0364),  //	DATA_6985_CONVERT("msdcpll "),
	REG_6985_CONVERT(0x00DC, 0x0384),  //	DATA_6985_CONVERT("adsppll"),
	REG_6985_CONVERT(0x00F0, 0x0374),  //	DATA_6985_CONVERT("imgpll"),
	{}
};
static struct fh_pll_regs mt6985_top1_regs[SIZE_6985_TOP1];
static struct fh_pll_domain mt6985_top1 = {
	.name = "top1",
	.data = (struct fh_pll_data *)&mt6985_top1_data,
	.offset = (struct fh_pll_offset *)&mt6985_top1_offset,
	.regs = (struct fh_pll_regs *)&mt6985_top1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu0
static struct fh_pll_data mt6985_gpu0_data[] = {
	DATA_6985_CONVERT("mfgpll"),
	{}
};
static struct fh_pll_offset mt6985_gpu0_offset[SIZE_6985_GPU0] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu0_regs[SIZE_6985_GPU0];
static struct fh_pll_domain mt6985_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6985_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu0_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu1

static struct fh_pll_data mt6985_gpu1_data[] = {
	DATA_6985_CONVERT("mfgnr"),
	{}
};
static struct fh_pll_offset mt6985_gpu1_offset[SIZE_6985_GPU1] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu1_regs[SIZE_6985_GPU1];
static struct fh_pll_domain mt6985_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6985_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6985_gpu2_data[] = {
	DATA_6985_CONVERT("gpuebpll"),
	{}
};
static struct fh_pll_offset mt6985_gpu2_offset[SIZE_6985_GPU2] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu2_regs[SIZE_6985_GPU2];
static struct fh_pll_domain mt6985_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6985_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu2_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu3
static struct fh_pll_data mt6985_gpu3_data[] = {
	DATA_6985_CONVERT("mfgscpll"),
	{}
};
static struct fh_pll_offset mt6985_gpu3_offset[SIZE_6985_GPU3] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu3_regs[SIZE_6985_GPU3];
static struct fh_pll_domain mt6985_gpu3 = {
	.name = "gpu3",
	.data = (struct fh_pll_data *)&mt6985_gpu3_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu3_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu0
static struct fh_pll_data mt6985_mcu0_data[] = {
	DATA_6985_CONVERT("buspll"),
	{}
};
static struct fh_pll_offset mt6985_mcu0_offset[SIZE_6985_MCU0] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu0_regs[SIZE_6985_MCU0];
static struct fh_pll_domain mt6985_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6985_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu0_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu1

static struct fh_pll_data mt6985_mcu1_data[] = {
	DATA_6985_CONVERT("cpu0pll"),
	{}
};
static struct fh_pll_offset mt6985_mcu1_offset[SIZE_6985_MCU1] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu1_regs[SIZE_6985_MCU1];
static struct fh_pll_domain mt6985_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6985_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6985_mcu2_data[] = {
	DATA_6985_CONVERT("cpu1pll"),
	{}
};
static struct fh_pll_offset mt6985_mcu2_offset[SIZE_6985_MCU2] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu2_regs[SIZE_6985_MCU2];
static struct fh_pll_domain mt6985_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6985_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6985_mcu3_data[] = {
	DATA_6985_CONVERT("cpu2pll"),
	{}
};
static struct fh_pll_offset mt6985_mcu3_offset[SIZE_6985_MCU3] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu3_regs[SIZE_6985_MCU3];
static struct fh_pll_domain mt6985_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6985_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu4
static struct fh_pll_data mt6985_mcu4_data[] = {
	DATA_6985_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6985_mcu4_offset[SIZE_6985_MCU4] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu4_regs[SIZE_6985_MCU4];
static struct fh_pll_domain mt6985_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6985_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6985_domain[] = {
	&mt6985_top0,
	&mt6985_top1,
	&mt6985_gpu0,
	&mt6985_gpu1,
	&mt6985_gpu2,
	&mt6985_gpu3,
	&mt6985_mcu0,
	&mt6985_mcu1,
	&mt6985_mcu2,
	&mt6985_mcu3,
	&mt6985_mcu4,
	NULL
};
static struct match mt6985_match = {
	.compatible = "mediatek,mt6985-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6985_domain,
};
/* 6985 end */

/* 6989 begin */
#define SIZE_6989_TOP0 (sizeof(mt6989_top0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_TOP1 (sizeof(mt6989_top1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_GPU0 (sizeof(mt6989_gpu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_GPU1 (sizeof(mt6989_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_GPU2 (sizeof(mt6989_gpu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU0 (sizeof(mt6989_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU1 (sizeof(mt6989_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU2 (sizeof(mt6989_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU3 (sizeof(mt6989_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU4 (sizeof(mt6989_mcu4_data)\
	/sizeof(struct fh_pll_data))


#define DATA_6989_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),		    \
		.postdiv_offset = 24,				        \
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6989_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,                \
		.offset_hp_en_set = 0x168,          \
		.offset_hp_en_clr = 0x16c,			\
		.offset_clk_con = 0x8,				\
		.offset_clk_con_set = 0x170,        \
		.offset_clk_con_clr = 0x174,        \
		.offset_rst_con = 0xc,				\
		.offset_rst_con_set = 0x178,        \
		.offset_rst_con_clr = 0x17c,        \
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6989_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}


///////////////////////////////////top0
static struct fh_pll_data mt6989_top0_data[] = {
	DATA_6989_CONVERT("mainpll2"),
	DATA_6989_CONVERT("mmpll2"),
	DATA_6989_CONVERT("noHW-mempll"),
	DATA_6989_CONVERT("emipll2"),
	DATA_6989_CONVERT("emipll"),
	DATA_6989_CONVERT("noHW-mpll"),
	DATA_6989_CONVERT("mmpll"),
	DATA_6989_CONVERT("mainpll"),
	DATA_6989_CONVERT("msdcpll"),
	DATA_6989_CONVERT("adsppll"),
	DATA_6989_CONVERT("nouse-imgpll"),
	DATA_6989_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_offset mt6989_top0_offset[SIZE_6989_TOP0] = {
	REG_6989_CONVERT(0x003C, 0x0284),  //	DATA_6989_CONVERT("mainpll2"),
	REG_6989_CONVERT(0x0050, 0x02A4),  //	DATA_6989_CONVERT("mmpll2"),
	REG_6989_CONVERT(0x0064, 0xffff),  //	DATA_6989_CONVERT("mempll"),
	REG_6989_CONVERT(0x0078, 0x03C4),  //	DATA_6989_CONVERT("emipll2"),

	REG_6989_CONVERT(0x008C, 0x03B4),  //   DATA_6989_CONVERT("emipll"),
	REG_6989_CONVERT(0x00A0, 0xffff),  //	can not find mpll con1@ apmix CODA
	REG_6989_CONVERT(0x00B4, 0x03A4),  //	DATA_6989_CONVERT("mmpll"),
	REG_6989_CONVERT(0x00C8, 0x0354),  //	DATA_6989_CONVERT("mainpll"),
	REG_6989_CONVERT(0x00DC, 0x0364),  //	DATA_6989_CONVERT("msdcpll "),
	REG_6989_CONVERT(0x00F0, 0x0384),  //	DATA_6989_CONVERT("adsppll"),
	REG_6989_CONVERT(0x0104, 0x0374),  //	DATA_6989_CONVERT("imgpll"),
	REG_6989_CONVERT(0x0118, 0x024c),  //	DATA_6989_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_regs mt6989_top0_regs[SIZE_6989_TOP0];
static struct fh_pll_domain mt6989_top0 = {
	.name = "top0",
	.data = (struct fh_pll_data *)&mt6989_top0_data,
	.offset = (struct fh_pll_offset *)&mt6989_top0_offset,
	.regs = (struct fh_pll_regs *)&mt6989_top0_regs,
	.init = &init_v1,
};
///////////////////////////////////top1
static struct fh_pll_data mt6989_top1_data[] = {
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("imgpll"),
	{}
};
static struct fh_pll_offset mt6989_top1_offset[SIZE_6989_TOP1] = {
	REG_6989_CONVERT(0x003C, 0x0284),  //	DATA_6989_CONVERT("mainpll2"),
	REG_6989_CONVERT(0x0050, 0x02A4),  //	DATA_6989_CONVERT("mmpll2"),
	REG_6989_CONVERT(0x0064, 0xffff),  //	DATA_6989_CONVERT("mempll"),
	REG_6989_CONVERT(0x0078, 0x03C4),  //	DATA_6989_CONVERT("emipll2"),

	REG_6989_CONVERT(0x008C, 0x03B4),  //   DATA_6989_CONVERT("emipll"),
	REG_6989_CONVERT(0x00A0, 0xffff),  //	can not find mpll con1@ apmix CODA
	REG_6989_CONVERT(0x00B4, 0x03A4),  //	DATA_6989_CONVERT("mmpll"),
	REG_6989_CONVERT(0x00C8, 0x0354),  //	DATA_6989_CONVERT("mainpll"),
	REG_6989_CONVERT(0x00DC, 0x0364),  //	DATA_6989_CONVERT("msdcpll "),
	REG_6989_CONVERT(0x00F0, 0x0384),  //	DATA_6989_CONVERT("adsppll"),
	REG_6989_CONVERT(0x0104, 0x0374),  //	DATA_6989_CONVERT("imgpll"),
	{}
};
static struct fh_pll_regs mt6989_top1_regs[SIZE_6989_TOP1];
static struct fh_pll_domain mt6989_top1 = {
	.name = "top1",
	.data = (struct fh_pll_data *)&mt6989_top1_data,
	.offset = (struct fh_pll_offset *)&mt6989_top1_offset,
	.regs = (struct fh_pll_regs *)&mt6989_top1_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu0
static struct fh_pll_data mt6989_gpu0_data[] = {
	DATA_6989_CONVERT("mfg-ao-mfgpll"),
	{}
};
static struct fh_pll_offset mt6989_gpu0_offset[SIZE_6989_GPU0] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_gpu0_regs[SIZE_6989_GPU0];
static struct fh_pll_domain mt6989_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6989_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6989_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6989_gpu0_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu1

static struct fh_pll_data mt6989_gpu1_data[] = {
	DATA_6989_CONVERT("mfgsc0-ao-mfgpll-sc0"),
	{}
};
static struct fh_pll_offset mt6989_gpu1_offset[SIZE_6989_GPU1] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_gpu1_regs[SIZE_6989_GPU1];
static struct fh_pll_domain mt6989_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6989_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6989_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6989_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6989_gpu2_data[] = {
	DATA_6989_CONVERT("mfgsc1-ao-mfgpll-sc1"),
	{}
};
static struct fh_pll_offset mt6989_gpu2_offset[SIZE_6989_GPU2] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_gpu2_regs[SIZE_6989_GPU2];
static struct fh_pll_domain mt6989_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6989_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6989_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6989_gpu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu0
static struct fh_pll_data mt6989_mcu0_data[] = {
	DATA_6989_CONVERT("buspll"),
	{}
};
static struct fh_pll_offset mt6989_mcu0_offset[SIZE_6989_MCU0] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu0_regs[SIZE_6989_MCU0];
static struct fh_pll_domain mt6989_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6989_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu0_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu1

static struct fh_pll_data mt6989_mcu1_data[] = {
	DATA_6989_CONVERT("cpu0pll"),
	{}
};
static struct fh_pll_offset mt6989_mcu1_offset[SIZE_6989_MCU1] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu1_regs[SIZE_6989_MCU1];
static struct fh_pll_domain mt6989_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6989_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6989_mcu2_data[] = {
	DATA_6989_CONVERT("cpu1pll"),
	{}
};
static struct fh_pll_offset mt6989_mcu2_offset[SIZE_6989_MCU2] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu2_regs[SIZE_6989_MCU2];
static struct fh_pll_domain mt6989_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6989_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6989_mcu3_data[] = {
	DATA_6989_CONVERT("cpu2pll"),
	{}
};
static struct fh_pll_offset mt6989_mcu3_offset[SIZE_6989_MCU3] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu3_regs[SIZE_6989_MCU3];
static struct fh_pll_domain mt6989_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6989_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu4
static struct fh_pll_data mt6989_mcu4_data[] = {
	DATA_6989_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6989_mcu4_offset[SIZE_6989_MCU4] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu4_regs[SIZE_6989_MCU4];
static struct fh_pll_domain mt6989_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6989_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6989_domain[] = {
	&mt6989_top0,
	&mt6989_top1,
	&mt6989_gpu0,
	&mt6989_gpu1,
	&mt6989_gpu2,
	&mt6989_mcu0,
	&mt6989_mcu1,
	&mt6989_mcu2,
	&mt6989_mcu3,
	&mt6989_mcu4,
	NULL
};
static struct match mt6989_match = {
	.compatible = "mediatek,mt6989-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6989_domain,
};
/* 6989 end */

/* 6991 begin */
#define SIZE_6991_TOP0 (sizeof(mt6991_top0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_TOP1 (sizeof(mt6991_top1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_TOP2 (sizeof(mt6991_top2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_GPU0 (sizeof(mt6991_gpu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_GPU1 (sizeof(mt6991_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_GPU2 (sizeof(mt6991_gpu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_MCU0 (sizeof(mt6991_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_MCU1 (sizeof(mt6991_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_MCU2 (sizeof(mt6991_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_MCU3 (sizeof(mt6991_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6991_MCU4 (sizeof(mt6991_mcu4_data)\
	/sizeof(struct fh_pll_data))

/* top0 PLLGP1 slope1 is for emipll, top1 PLLGP2 no slope2 */
#define DATA_6991_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),		    \
		.postdiv_offset = 24,				        \
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6991_CONVERT_T0GP1(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,                \
		.offset_hp_en_set = 0x4,          \
		.offset_hp_en_clr = 0x8,			\
		.offset_clk_con = 0x10,				\
		.offset_clk_con_set = 0x14,        \
		.offset_clk_con_clr = 0x18,        \
		.offset_rst_con = 0x1c,				\
		.offset_rst_con_set = 0x20,        \
		.offset_rst_con_clr = 0x24,        \
		.offset_slope0 = 0x28,				\
		.offset_slope1 = 0x2c,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
#define REG_6991_CONVERT_T1GP2(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,                \
		.offset_hp_en_set = 0x4,          \
		.offset_hp_en_clr = 0x8,			\
		.offset_clk_con = 0x10,				\
		.offset_clk_con_set = 0x14,        \
		.offset_clk_con_clr = 0x18,        \
		.offset_rst_con = 0x1c,				\
		.offset_rst_con_set = 0x20,        \
		.offset_rst_con_clr = 0x24,        \
		.offset_slope0 = 0x28,				\
		.offset_slope1 = 0x28,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6991_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}


///////////////////////////////////top0
static struct fh_pll_data mt6991_top0_data[] = {
	DATA_6991_CONVERT("mainpll"),
	DATA_6991_CONVERT("msdcpll"),
	DATA_6991_CONVERT("adsppll"),
	DATA_6991_CONVERT("emipll"),
	DATA_6991_CONVERT("emipll2"),
	DATA_6991_CONVERT("net1pll"),
	DATA_6991_CONVERT("sgmiipll"),
	{}
};
static struct fh_pll_offset mt6991_top0_offset[SIZE_6991_TOP0] = {
	REG_6991_CONVERT_T0GP1(0x0054, 0x0254),  //	DATA_6991_CONVERT("mainpll"),
	REG_6991_CONVERT_T0GP1(0x0068, 0x027C),  //	DATA_6991_CONVERT("msdcpll"),
	REG_6991_CONVERT_T0GP1(0x007C, 0x0290),  //	DATA_6991_CONVERT("adsppll"),
	REG_6991_CONVERT_T0GP1(0x0090, 0x02A4),  //	DATA_6991_CONVERT("emipll"),
	REG_6991_CONVERT_T0GP1(0x00A4, 0x02B8),  //	DATA_6991_CONVERT("emipll2"),
	REG_6991_CONVERT_T0GP1(0x00B8, 0x02CC),  //	DATA_6991_CONVERT("net1pll"),
	REG_6991_CONVERT_T0GP1(0x00CC, 0x02E0),  //   DATA_6991_CONVERT("sgmiipll"),
	{}
};
static struct fh_pll_regs mt6991_top0_regs[SIZE_6991_TOP0];
static struct fh_pll_domain mt6991_top0 = {
	.name = "top0",
	.data = (struct fh_pll_data *)&mt6991_top0_data,
	.offset = (struct fh_pll_offset *)&mt6991_top0_offset,
	.regs = (struct fh_pll_regs *)&mt6991_top0_regs,
	.init = &init_v1,
};
///////////////////////////////////top1
static struct fh_pll_data mt6991_top1_data[] = {
	DATA_6991_CONVERT("mainpll2"),
	DATA_6991_CONVERT("mmpll2"),
	DATA_6991_CONVERT("nouse-imgpll"),
	DATA_6991_CONVERT("tvdpll1"),
	DATA_6991_CONVERT("tvdpll2"),
	DATA_6991_CONVERT("tvdpll3"),
	{}
};
static struct fh_pll_offset mt6991_top1_offset[SIZE_6991_TOP1] = {
	REG_6991_CONVERT_T1GP2(0x0050, 0x0254),  //	DATA_6991_CONVERT("mainpll2"),
	REG_6991_CONVERT_T1GP2(0x0064, 0x027C),  //	DATA_6991_CONVERT("mmpll2"),
	REG_6991_CONVERT_T1GP2(0x0078, 0x0290),  //	DATA_6991_CONVERT("imgpll"),
	REG_6991_CONVERT_T1GP2(0x008C, 0x02A4),  //	DATA_6991_CONVERT("tvdpll1"),
	REG_6991_CONVERT_T1GP2(0x00A0, 0x02B8),  //	DATA_6991_CONVERT("tvdpll2"),
	REG_6991_CONVERT_T1GP2(0x00B4, 0x02CC),  //	DATA_6991_CONVERT("tvdpll3"),
	{}
};
static struct fh_pll_regs mt6991_top1_regs[SIZE_6991_TOP1];
static struct fh_pll_domain mt6991_top1 = {
	.name = "top1",
	.data = (struct fh_pll_data *)&mt6991_top1_data,
	.offset = (struct fh_pll_offset *)&mt6991_top1_offset,
	.regs = (struct fh_pll_regs *)&mt6991_top1_regs,
	.init = &init_v1,
};
///////////////////////////////////top2
static struct fh_pll_data mt6991_top2_data[] = {
	DATA_6991_CONVERT("nouse"),
	DATA_6991_CONVERT("nouse"),
	DATA_6991_CONVERT("imgpll"),
	DATA_6991_CONVERT("nouse"),
	DATA_6991_CONVERT("nouse"),
	DATA_6991_CONVERT("nouse"),
	{}
};
static struct fh_pll_offset mt6991_top2_offset[SIZE_6991_TOP2] = {
	REG_6991_CONVERT_T1GP2(0x0050, 0x0254),  //	DATA_6991_CONVERT("mainpll2"),
	REG_6991_CONVERT_T1GP2(0x0064, 0x027C),  //	DATA_6991_CONVERT("mmpll2"),
	REG_6991_CONVERT_T1GP2(0x0078, 0x0290),  //	DATA_6991_CONVERT("imgpll"),
	REG_6991_CONVERT_T1GP2(0x008C, 0x02A4),  //	DATA_6991_CONVERT("tvdpll1"),
	REG_6991_CONVERT_T1GP2(0x00A0, 0x02B8),  //	DATA_6991_CONVERT("tvdpll2"),
	REG_6991_CONVERT_T1GP2(0x00B4, 0x02CC),  //	DATA_6991_CONVERT("tvdpll3"),
	{}
};
static struct fh_pll_regs mt6991_top2_regs[SIZE_6991_TOP2];
static struct fh_pll_domain mt6991_top2 = {
	.name = "top2",
	.data = (struct fh_pll_data *)&mt6991_top2_data,
	.offset = (struct fh_pll_offset *)&mt6991_top2_offset,
	.regs = (struct fh_pll_regs *)&mt6991_top2_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu0
static struct fh_pll_data mt6991_gpu0_data[] = {
	DATA_6991_CONVERT("mfgpll"),
	{}
};
static struct fh_pll_offset mt6991_gpu0_offset[SIZE_6991_GPU0] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_gpu0_regs[SIZE_6991_GPU0];
static struct fh_pll_domain mt6991_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6991_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6991_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6991_gpu0_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu1

static struct fh_pll_data mt6991_gpu1_data[] = {
	DATA_6991_CONVERT("mfgpll-sc0"),
	{}
};
static struct fh_pll_offset mt6991_gpu1_offset[SIZE_6991_GPU1] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_gpu1_regs[SIZE_6991_GPU1];
static struct fh_pll_domain mt6991_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6991_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6991_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6991_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6991_gpu2_data[] = {
	DATA_6991_CONVERT("mfgpll-sc1"),
	{}
};
static struct fh_pll_offset mt6991_gpu2_offset[SIZE_6991_GPU2] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_gpu2_regs[SIZE_6991_GPU2];
static struct fh_pll_domain mt6991_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6991_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6991_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6991_gpu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu0
static struct fh_pll_data mt6991_mcu0_data[] = {
	DATA_6991_CONVERT("ccipll"),
	{}
};
static struct fh_pll_offset mt6991_mcu0_offset[SIZE_6991_MCU0] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_mcu0_regs[SIZE_6991_MCU0];
static struct fh_pll_domain mt6991_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6991_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6991_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6991_mcu0_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu1

static struct fh_pll_data mt6991_mcu1_data[] = {
	DATA_6991_CONVERT("armpll-ll"),
	{}
};
static struct fh_pll_offset mt6991_mcu1_offset[SIZE_6991_MCU1] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_mcu1_regs[SIZE_6991_MCU1];
static struct fh_pll_domain mt6991_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6991_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6991_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6991_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6991_mcu2_data[] = {
	DATA_6991_CONVERT("armpll-bl"),
	{}
};
static struct fh_pll_offset mt6991_mcu2_offset[SIZE_6991_MCU2] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_mcu2_regs[SIZE_6991_MCU2];
static struct fh_pll_domain mt6991_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6991_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6991_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6991_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6991_mcu3_data[] = {
	DATA_6991_CONVERT("armpll-b"),
	{}
};
static struct fh_pll_offset mt6991_mcu3_offset[SIZE_6991_MCU3] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_mcu3_regs[SIZE_6991_MCU3];
static struct fh_pll_domain mt6991_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6991_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6991_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6991_mcu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu4
static struct fh_pll_data mt6991_mcu4_data[] = {
	DATA_6991_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6991_mcu4_offset[SIZE_6991_MCU4] = {
	REG_6991_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6991_mcu4_regs[SIZE_6991_MCU4];
static struct fh_pll_domain mt6991_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6991_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6991_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6991_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6991_domain[] = {
	&mt6991_top0,
	&mt6991_top1,
	&mt6991_top2,
	&mt6991_gpu0,
	&mt6991_gpu1,
	&mt6991_gpu2,
	&mt6991_mcu0,
	&mt6991_mcu1,
	&mt6991_mcu2,
	&mt6991_mcu3,
	&mt6991_mcu4,
	NULL
};
static struct match mt6991_match = {
	.compatible = "mediatek,mt6991-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6991_domain,
};
/* 6991 end */

static const struct match *matches[] = {
	&mt6761_match,
	&mt6765_match,
	&mt6768_match,
	&mt6781_match,
	&mt6853_match,
	&mt6877_match,
	&mt6885_match,
	&mt6897_match,
	&mt6985_match,
	&mt6989_match,
	&mt6991_match,
	NULL
};

static struct fh_pll_domain **get_list(char *comp)
{
	struct match **match;
	static struct fh_pll_domain **list;

	match = (struct match **)matches;

	/* name used only if !list */
	if (!list) {
		while (*matches != NULL) {
			if (strcmp(comp,
						(*match)->compatible) == 0) {
				list = (*match)->domain_list;
				FHDBG("target<%s>\n", comp);
				break;
			}
			match++;
		}
	}
	return list;
}
void init_fh_domain(const char *domain,
		char *comp,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base)
{
	struct fh_pll_domain **list;

	list = get_list(comp);

	while (*list != NULL) {
		if (strcmp(domain,
					(*list)->name) == 0) {
			(*list)->init(*list,
					fhctl_base,
					apmixed_base);
			return;
		}
		list++;
	}
}
struct fh_pll_domain *get_fh_domain(const char *domain)
{
	struct fh_pll_domain **list;

	list = get_list(NULL);

	/* find instance */
	while (*list != NULL) {
		if (strcmp(domain,
					(*list)->name) == 0)
			return *list;
		list++;
	}
	return NULL;
}
