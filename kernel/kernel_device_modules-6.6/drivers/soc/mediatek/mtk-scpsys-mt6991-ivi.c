// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Xiufeng Li <xiufeng.li@mediatek.com>
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

#include <dt-bindings/power/mt6991-ivi-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6991_SPM_PROT_EN_BUS_PERI_EHER	(BIT(5))
#define MT6991_SPM_PROT_EN_BUS_HSGMII0		(BIT(14))
#define MT6991_SPM_PROT_EN_BUS_HSGMII1		(BIT(15))

enum regmap_type {
	INVALID_TYPE = 0,
	SPM_TYPE = 1,
	MMPC_TYPE = 2,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[SPM_TYPE] = "spm",
	[MMPC_TYPE] = "mmpc",
};

/*
 * MT6991 ivi power domain support
 */

static const struct scp_domain_data scp_domain_mt6991_ivi_spm_data[] = {
	[MT6991_POWER_DOMAIN_PERI_ETHER] = {
		.name = "peri-ether",
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"gmac125"},
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PERI_EHER),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_HSGMII0] = {
		.name = "hsgmii0",
		.ctl_offs = 0xE94,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xE8, 0xEC, 0xE4, 0x20C,
				MT6991_SPM_PROT_EN_BUS_HSGMII0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_HSGMII1] = {
		.name = "hsgmii1",
		.ctl_offs = 0xE98,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xE8, 0xEC, 0xE4, 0x20C,
				MT6991_SPM_PROT_EN_BUS_HSGMII1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
};

static const struct scp_soc_data mt6991_ivi_spm_data = {
	.domains = scp_domain_mt6991_ivi_spm_data,
	.num_domains = MT6991_IVI_SPM_POWER_DOMAIN_NR,
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6991-ivi-scpsys",
		.data = &mt6991_ivi_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt6991_ivi_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);
	if (!soc)
		return -EINVAL;

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	ret = mtk_register_power_domains(pdev, scp, soc->num_domains);
	if (ret)
		return ret;

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_info(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6991_ivi_scpsys_drv = {
	.probe = mt6991_ivi_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6991-ivi",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6991_ivi_scpsys_drv);
MODULE_LICENSE("GPL");
