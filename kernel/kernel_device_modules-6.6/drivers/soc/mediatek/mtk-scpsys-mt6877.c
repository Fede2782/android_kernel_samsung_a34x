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

#include <dt-bindings/power/mt6877-power.h>

/* Define MTCMOS Bus Protect Mask */
#define CONN_PROT_STEP1_0_MASK			(BIT(13) | BIT(18))
#define CONN_PROT_STEP2_0_MASK			(BIT(14) | BIT(19))
#define ISP0_PROT_STEP1_0_MASK			(BIT(14))
#define ISP0_PROT_STEP2_0_MASK			(BIT(15))
#define IPE_PROT_STEP1_0_MASK			(BIT(16))
#define IPE_PROT_STEP2_0_MASK			(BIT(17))
#define VDE0_PROT_STEP1_0_MASK			(BIT(24))
#define VDE0_PROT_STEP2_0_MASK			(BIT(25))
#define VEN_PROT_STEP1_0_MASK			(BIT(26))
#define VEN_PROT_STEP2_0_MASK			(BIT(27))
#define DIS0_PROT_STEP1_0_MASK			(BIT(0) | BIT(2) | BIT(10) | BIT(12) \
						| BIT(14) | BIT(16) | BIT(24) | BIT(26))
#define DIS0_PROT_STEP2_0_MASK			(BIT(6))
#define DIS0_PROT_STEP2_1_MASK			(BIT(1) | BIT(3) | BIT(15) | BIT(17) \
						| BIT(25) | BIT(27))
#define AUDIO_PROT_STEP1_0_MASK			(BIT(4))
#define CAM_PROT_STEP1_0_MASK			(BIT(0))
#define CAM_PROT_STEP1_1_MASK			(BIT(0) | BIT(2))
#define CAM_PROT_STEP2_0_MASK			(BIT(22))
#define CAM_PROT_STEP2_1_MASK			(BIT(1) | BIT(3))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infracfg",
};

/*
 * MT6877 power domain support
 */
static const struct scp_domain_data scp_domain_data_mt6877[] = {
	[MT6877_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x0E04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				CONN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				CONN_PROT_STEP2_0_MASK),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6877_POWER_DOMAIN_ISP0] = {
		.name = "isp0",
		.sta_mask = BIT(9),
		.ctl_offs = 0x0E24,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"img"},
		.subsys_clk_prefix = "isp0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				ISP0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				ISP0_PROT_STEP2_0_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_ISP1] = {
		.name = "isp1",
		.sta_mask = BIT(10),
		.ctl_offs = 0x0E28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"img"},
		.subsys_clk_prefix = "isp1",
	},

	[MT6877_POWER_DOMAIN_IPE] = {
		.name = "ipe",
		.sta_mask = BIT(11),
		.ctl_offs = 0x0E2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				IPE_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				IPE_PROT_STEP2_0_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(12),
		.ctl_offs = 0x0E30,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vdec"},
		.subsys_clk_prefix = "vdec",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				VDE0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				VDE0_PROT_STEP2_0_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = BIT(14),
		.ctl_offs = 0x0E38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"venc"},
		.subsys_clk_prefix = "venc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				VEN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				VEN_PROT_STEP2_0_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = BIT(18),
		.ctl_offs = 0x0E48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.basic_lp_clk_name = {"mdp"},
		.subsys_clk_prefix = "disp",
		.subsys_lp_clk_prefix = "mdp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				DIS0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				DIS0_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				DIS0_PROT_STEP2_1_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = BIT(21),
		.ctl_offs = 0x0E54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"aud"},
		.subsys_clk_prefix = "aud",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				AUDIO_PROT_STEP1_0_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(23),
		.ctl_offs = 0x0E5C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				CAM_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				CAM_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				CAM_PROT_STEP2_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				CAM_PROT_STEP2_0_MASK),
		},
	},

	[MT6877_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam_rawa",
		.sta_mask = BIT(24),
		.ctl_offs = 0x0E60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawa",
	},

	[MT6877_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam_rawb",
		.sta_mask = BIT(25),
		.ctl_offs = 0x0E64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawb",
	},

	[MT6877_POWER_DOMAIN_CSI] = {
		.name = "csi",
		.sta_mask = BIT(30),
		.ctl_offs = 0x0E78,
	},

	[MT6877_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6877[] = {
	{MT6877_POWER_DOMAIN_DISP, MT6877_POWER_DOMAIN_ISP0},
	{MT6877_POWER_DOMAIN_DISP, MT6877_POWER_DOMAIN_ISP1},
	{MT6877_POWER_DOMAIN_DISP, MT6877_POWER_DOMAIN_IPE},
	{MT6877_POWER_DOMAIN_DISP, MT6877_POWER_DOMAIN_VDEC},
	{MT6877_POWER_DOMAIN_DISP, MT6877_POWER_DOMAIN_VENC},
	{MT6877_POWER_DOMAIN_DISP, MT6877_POWER_DOMAIN_CAM},
	{MT6877_POWER_DOMAIN_CAM, MT6877_POWER_DOMAIN_CAM_RAWA},
	{MT6877_POWER_DOMAIN_CAM, MT6877_POWER_DOMAIN_CAM_RAWB},
	{MT6877_POWER_DOMAIN_CAM, MT6877_POWER_DOMAIN_CSI},
};

static const struct scp_soc_data mt6877_data = {
	.domains = scp_domain_data_mt6877,
	.num_domains = MT6877_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6877,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6877),
	.regs = {
		.pwr_sta_offs = 0x0EF0,
		.pwr_sta2nd_offs = 0x0EF4
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6877-scpsys",
		.data = &mt6877_data,
	}, {
		/* sentinel */
	}
};

static int mt6877_scpsys_probe(struct platform_device *pdev)
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
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6877_scpsys_drv = {
	.probe = mt6877_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6877",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};
#if IS_BUILTIN(CONFIG_DEVICE_MODULES_MTK_SCPSYS)
static int __init mt6877_scpsys_drv_init(void)
{
	return platform_driver_register(&mt6877_scpsys_drv);
}
subsys_initcall(mt6877_scpsys_drv_init);
#else
module_platform_driver(mt6877_scpsys_drv);
#endif
MODULE_LICENSE("GPL");
