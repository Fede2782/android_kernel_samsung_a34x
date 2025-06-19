// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "scpsys.h"
#include "mtk-scpsys.h"

#include <dt-bindings/power/mt6853-power.h>


#define MT6853_TOP_AXI_PROT_EN_MD	(BIT(7))
#define MT6853_TOP_AXI_PROT_EN_VDNR_MD	(BIT(2) | BIT(12) |  \
			BIT(20))
#define MT6853_TOP_AXI_PROT_EN_CONN	(BIT(13) | BIT(18))
#define MT6853_TOP_AXI_PROT_EN_CONN_2ND	(BIT(14))
#define MT6853_TOP_AXI_PROT_EN_1_CONN	(BIT(10))
#define MT6853_TOP_AXI_PROT_EN_1_MFG1	(BIT(21))
#define MT6853_TOP_AXI_PROT_EN_2_MFG1	(BIT(5) | BIT(6))
#define MT6853_TOP_AXI_PROT_EN_MFG1	(BIT(21) | BIT(22))
#define MT6853_TOP_AXI_PROT_EN_2_MFG1_2ND	(BIT(7))
#define MT6853_TOP_AXI_PROT_EN_MM_2_ISP	(BIT(8))
#define MT6853_TOP_AXI_PROT_EN_MM_2_ISP_2ND	(BIT(9))
#define MT6853_TOP_AXI_PROT_EN_MM_IPE	(BIT(16))
#define MT6853_TOP_AXI_PROT_EN_MM_IPE_2ND	(BIT(17))
#define MT6853_TOP_AXI_PROT_EN_MM_VDEC	(BIT(24))
#define MT6853_TOP_AXI_PROT_EN_MM_VDEC_2ND	(BIT(25))
#define MT6853_TOP_AXI_PROT_EN_MM_VENC	(BIT(26))
#define MT6853_TOP_AXI_PROT_EN_MM_VENC_2ND	(BIT(27))
#define MT6853_TOP_AXI_PROT_EN_MM_DISP	(BIT(0) | BIT(2) |  \
			BIT(10) | BIT(12) |  \
			BIT(16) | BIT(24) |  \
			BIT(26))
#define MT6853_TOP_AXI_PROT_EN_MM_2_DISP	(BIT(8))
#define MT6853_TOP_AXI_PROT_EN_DISP	(BIT(6) | BIT(23))
#define MT6853_TOP_AXI_PROT_EN_MM_DISP_2ND	(BIT(1) | BIT(3) |  \
			BIT(17) | BIT(25) |  \
			BIT(27))
#define MT6853_TOP_AXI_PROT_EN_MM_2_DISP_2ND	(BIT(9))
#define MT6853_TOP_AXI_PROT_EN_2_AUDIO	(BIT(4))
#define MT6853_TOP_AXI_PROT_EN_2_ADSP_DORMANT	(BIT(3))
#define MT6853_TOP_AXI_PROT_EN_2_CAM	(BIT(0))
#define MT6853_TOP_AXI_PROT_EN_MM_CAM	(BIT(0) | BIT(2))
#define MT6853_TOP_AXI_PROT_EN_1_CAM	(BIT(22))
#define MT6853_TOP_AXI_PROT_EN_MM_CAM_2ND	(BIT(1) | BIT(3))
#define MT6853_TOP_AXI_PROT_EN_VDNR_CAM	(BIT(19))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE,
	SMI_TYPE,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infracfg",
	[SMI_TYPE] = "smi_comm",
};
/*
 * MT6853 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6853[] = {
	[MT6853_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x300,
		.caps = MTK_SCPD_MD_OPS,
		.extb_iso_offs = 0x398,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6853_TOP_AXI_PROT_EN_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x0B84, 0x0B88, 0x0B80, 0x0B90,
				MT6853_TOP_AXI_PROT_EN_VDNR_MD),
		},
	},
	[MT6853_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x304,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6853_TOP_AXI_PROT_EN_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6853_TOP_AXI_PROT_EN_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6853_TOP_AXI_PROT_EN_1_CONN),
		},
	},
	[MT6853_POWER_DOMAIN_MFG0] = {
		.name = "mfg0",
		.sta_mask = BIT(2),
		.ctl_offs = 0x308,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mfg"},
	},
	[MT6853_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = BIT(3),
		.ctl_offs = 0x30C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6853_TOP_AXI_PROT_EN_1_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6853_TOP_AXI_PROT_EN_2_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6853_TOP_AXI_PROT_EN_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6853_TOP_AXI_PROT_EN_2_MFG1_2ND),
		},
	},
	[MT6853_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = BIT(4),
		.ctl_offs = 0x310,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6853_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = BIT(5),
		.ctl_offs = 0x314,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6853_POWER_DOMAIN_MFG5] = {
		.name = "mfg5",
		.sta_mask = BIT(7),
		.ctl_offs = 0x31C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6853_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(12),
		.ctl_offs = 0x330,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp"},
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6853_TOP_AXI_PROT_EN_MM_2_ISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6853_TOP_AXI_PROT_EN_MM_2_ISP_2ND),
		},
	},
	[MT6853_POWER_DOMAIN_ISP2] = {
		.name = "isp2",
		.sta_mask = BIT(13),
		.ctl_offs = 0x334,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp2"},
		.subsys_clk_prefix = "isp2",
	},
	[MT6853_POWER_DOMAIN_IPE] = {
		.name = "ipe",
		.sta_mask = BIT(14),
		.ctl_offs = 0x338,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_IPE),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_IPE_2ND),
		},
	},
	[MT6853_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(15),
		.ctl_offs = 0x33C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vdec"},
		.subsys_clk_prefix = "vdec",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_VDEC),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_VDEC_2ND),
		},
	},
	[MT6853_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = BIT(17),
		.ctl_offs = 0x344,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"venc"},
		.subsys_clk_prefix = "venc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_VENC),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_VENC_2ND),
		},
	},
	[MT6853_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = BIT(20),
		.ctl_offs = 0x350,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp", "mdp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6853_TOP_AXI_PROT_EN_MM_2_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6853_TOP_AXI_PROT_EN_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_DISP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6853_TOP_AXI_PROT_EN_MM_2_DISP_2ND),
		},
	},
	[MT6853_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = BIT(21),
		.ctl_offs = 0x354,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.subsys_clk_prefix = "audio",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6853_TOP_AXI_PROT_EN_2_AUDIO),
		},
	},
	[MT6853_POWER_DOMAIN_ADSP_DORMANT] = {
		.name = "adsp_dormant",
		.sta_mask = BIT(22),
		.ctl_offs = 0x358,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"adsp"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6853_TOP_AXI_PROT_EN_2_ADSP_DORMANT),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6853_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(23),
		.ctl_offs = 0x35C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6853_TOP_AXI_PROT_EN_2_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6853_TOP_AXI_PROT_EN_1_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6853_TOP_AXI_PROT_EN_MM_CAM_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0B84, 0x0B88, 0x0B80, 0x0B90,
				MT6853_TOP_AXI_PROT_EN_VDNR_CAM),
		},
	},
	[MT6853_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam_rawa",
		.sta_mask = BIT(24),
		.ctl_offs = 0x360,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawa",
	},
	[MT6853_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam_rawb",
		.sta_mask = BIT(25),
		.ctl_offs = 0x0364,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawb",
	},
	[MT6853_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS,
	},
};

static const struct scp_subdomain scp_subdomain_mt6853[] = {
	{MT6853_POWER_DOMAIN_MFG0, MT6853_POWER_DOMAIN_MFG1},
	{MT6853_POWER_DOMAIN_MFG1, MT6853_POWER_DOMAIN_MFG2},
	{MT6853_POWER_DOMAIN_MFG1, MT6853_POWER_DOMAIN_MFG3},
	{MT6853_POWER_DOMAIN_MFG1, MT6853_POWER_DOMAIN_MFG5},
	{MT6853_POWER_DOMAIN_DISP, MT6853_POWER_DOMAIN_ISP},
	{MT6853_POWER_DOMAIN_DISP, MT6853_POWER_DOMAIN_ISP2},
	{MT6853_POWER_DOMAIN_DISP, MT6853_POWER_DOMAIN_IPE},
	{MT6853_POWER_DOMAIN_DISP, MT6853_POWER_DOMAIN_VDEC},
	{MT6853_POWER_DOMAIN_DISP, MT6853_POWER_DOMAIN_VENC},
	{MT6853_POWER_DOMAIN_DISP, MT6853_POWER_DOMAIN_CAM},
	{MT6853_POWER_DOMAIN_CAM, MT6853_POWER_DOMAIN_CAM_RAWA},
	{MT6853_POWER_DOMAIN_CAM, MT6853_POWER_DOMAIN_CAM_RAWB},
};
static const struct scp_soc_data mt6853_data = {
	.domains = scp_domain_data_mt6853,
	.num_domains = MT6853_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6853,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6853),
	.regs = {
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6853-scpsys",
		.data = &mt6853_data,
	}, {
		/* sentinel */
	}
};

static int mt6853_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	mtk_register_power_domains(pdev, scp, soc->num_domains);

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM))
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
	}

	return 0;
}

static struct platform_driver mt6853_scpsys_drv = {
	.probe = mt6853_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6853",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6853_scpsys_drv);
MODULE_LICENSE("GPL");
