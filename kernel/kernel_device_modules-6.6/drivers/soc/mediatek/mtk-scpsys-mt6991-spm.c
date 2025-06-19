// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
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

#include <dt-bindings/power/mt6991-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6991_TOP_AXI_PROT_EN_SLEEP0_MD	(BIT(29))
#define MT6991_TOP_AXI_PROT_EN_SLEEP1_MD	(BIT(0))
#define MT6991_SPM_PROT_EN_BUS_CONN	(BIT(1))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_DP_PHY_P0	(BIT(6))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_P0	(BIT(7))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_P1	(BIT(8))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_P23	(BIT(9))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_PHY_P2	(BIT(10))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_MAC0	(BIT(13))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_MAC1	(BIT(14))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_MAC2	(BIT(15))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_PHY0	(BIT(16))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_PHY1	(BIT(17))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_PHY2	(BIT(18))
#define MT6991_SPM_PROT_EN_BUS_AUDIO	(BIT(19))
#define MT6991_SPM_PROT_EN_BUS_ADSP_TOP	(BIT(21))
#define MT6991_SPM_PROT_EN_BUS_ADSP_INFRA	(BIT(22))
#define MT6991_SPM_PROT_EN_BUS_ADSP_AO	(BIT(23))
#define MT6991_SPM_PROT_EN_BUS_MM_PROC	(BIT(24))
#define MT6991_SPM_PROT_EN_BUS_SSRSYS	(BIT(10))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	SPM_TYPE = 2,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "apifrbus-ao-io-reg-bus",
	[SPM_TYPE] = "spm",
};

static struct genpd_onecell_data *pd_data;

/*
 * MT6991 power domain support
 */

static const struct scp_domain_data scp_domain_mt6991_spm_data[] = {
	[MT6991_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xEFC,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x4, 0x8, 0x0, 0xC,
				MT6991_TOP_AXI_PROT_EN_SLEEP0_MD),
			BUS_PROT(IFR_TYPE, 0x24, 0x28, 0x20, 0x2C,
				MT6991_TOP_AXI_PROT_EN_SLEEP1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS |
			MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_REMOVE_MD_RSTB,
	},
	[MT6991_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6991_POWER_DOMAIN_SSUSB_DP_PHY_P0] = {
		.name = "ssusb-dp-phy-p0",
		.ctl_offs = 0xE18,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_DP_PHY_P0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P0] = {
		.name = "ssusb-p0",
		.ctl_offs = 0xE1C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P1] = {
		.name = "ssusb-p1",
		.ctl_offs = 0xE20,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P23] = {
		.name = "ssusb-p23",
		.ctl_offs = 0xE24,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P23),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_PHY_P2] = {
		.name = "ssusb-phy-p2",
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_PHY_P2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC0] = {
		.name = "pextp-mac0",
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC1] = {
		.name = "pextp-mac1",
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC2] = {
		.name = "pextp-mac2",
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp-phy0",
		.ctl_offs = 0xE40,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp-phy1",
		.ctl_offs = 0xE44,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY2] = {
		.name = "pextp-phy2",
		.ctl_offs = 0xE48,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE54,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp-infra",
		.ctl_offs = 0xE58,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE5C,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm-proc-dormant",
		.ctl_offs = 0xE60,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_MM_PROC),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IRQ_SAVE | MTK_SCPD_SRAM_SLP |
			MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.ctl_offs = 0xE84,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xE8, 0xEC, 0xE4, 0x20C,
				MT6991_SPM_PROT_EN_BUS_SSRSYS),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
};

static const struct scp_domain_data scp_domain_mt6991_spm_hwv_data[] = {
	[MT6991_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xEFC,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x4, 0x8, 0x0, 0xC,
				MT6991_TOP_AXI_PROT_EN_SLEEP0_MD),
			BUS_PROT(IFR_TYPE, 0x24, 0x28, 0x20, 0x2C,
				MT6991_TOP_AXI_PROT_EN_SLEEP1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS |
			MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_REMOVE_MD_RSTB,
	},
	[MT6991_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6991_POWER_DOMAIN_SSUSB_DP_PHY_P0] = {
		.name = "ssusb-dp-phy-p0",
		.ctl_offs = 0xE18,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_DP_PHY_P0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P0] = {
		.name = "ssusb-p0",
		.ctl_offs = 0xE1C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P1] = {
		.name = "ssusb-p1",
		.ctl_offs = 0xE20,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P23] = {
		.name = "ssusb-p23",
		.ctl_offs = 0xE24,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P23),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_PHY_P2] = {
		.name = "ssusb-phy-p2",
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_PHY_P2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC0] = {
		.name = "pextp-mac0",
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC1] = {
		.name = "pextp-mac1",
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC2] = {
		.name = "pextp-mac2",
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp-phy0",
		.ctl_offs = 0xE40,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp-phy1",
		.ctl_offs = 0xE44,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY2] = {
		.name = "pextp-phy2",
		.ctl_offs = 0xE48,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE54,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp-infra",
		.ctl_offs = 0xE58,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE5C,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm-proc-dormant",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_IRQ_SAVE | default_cap | MTK_SCPD_INFRA_REQ_OPT,
	},
	[MT6991_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 1,
		.caps = MTK_SCPD_HWV_OPS | default_cap | MTK_SCPD_INFRA_REQ_OPT,
	},
};

static const struct scp_domain_data scp_domain_mt6991_pbus_used_data[] = {
	[MT6991_PBUS_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0x004,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON |
			MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_SSUSB_DP_PHY_P0] = {
		.name = "ssusb-dp-phy-p0",
		.ctl_offs = 0x018,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap |
			MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_SSUSB_P0] = {
		.name = "ssusb-p0",
		.ctl_offs = 0x01C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap |
			MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_SSUSB_P1] = {
		.name = "ssusb-p1",
		.ctl_offs = 0x020,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap |
			MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_SSUSB_P23] = {
		.name = "ssusb-p23",
		.ctl_offs = 0x024,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap |
			MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_SSUSB_PHY_P2] = {
		.name = "ssusb-phy-p2",
		.ctl_offs = 0x028,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap |
			MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_PEXTP_MAC0] = {
		.name = "pextp-mac0",
		.ctl_offs = 0x034,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_PEXTP_MAC1] = {
		.name = "pextp-mac1",
		.ctl_offs = 0x038,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_PEXTP_MAC2] = {
		.name = "pextp-mac2",
		.ctl_offs = 0x03C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp-phy0",
		.ctl_offs = 0x040,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp-phy1",
		.ctl_offs = 0x044,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_PEXTP_PHY2] = {
		.name = "pextp-phy2",
		.ctl_offs = 0x048,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0x04C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0x054,
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp-infra",
		.ctl_offs = 0x058,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap | MTK_SCPD_PBUS_OPS,
	},
	[MT6991_PBUS_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0x05C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap | MTK_SCPD_PBUS_OPS,
	},
};

static const struct scp_domain_data scp_domain_mt6991_pbus_unused_data[] = {
	[MT6991_NONPBUS_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xEFC,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x4, 0x8, 0x0, 0xC,
				MT6991_TOP_AXI_PROT_EN_SLEEP0_MD),
			BUS_PROT(IFR_TYPE, 0x24, 0x28, 0x20, 0x2C,
				MT6991_TOP_AXI_PROT_EN_SLEEP1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS |
			MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_REMOVE_MD_RSTB,
	},
	[MT6991_NONPBUS_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm-proc-dormant",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_IRQ_SAVE | default_cap,
	},
	[MT6991_NONPBUS_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 1,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt6991_spm[] = {
	{MT6991_POWER_DOMAIN_SSUSB_P0, MT6991_POWER_DOMAIN_SSUSB_DP_PHY_P0},
	{MT6991_POWER_DOMAIN_SSUSB_P23, MT6991_POWER_DOMAIN_SSUSB_PHY_P2},
	{MT6991_POWER_DOMAIN_PEXTP_MAC0, MT6991_POWER_DOMAIN_PEXTP_PHY0},
	{MT6991_POWER_DOMAIN_PEXTP_MAC1, MT6991_POWER_DOMAIN_PEXTP_PHY1},
	{MT6991_POWER_DOMAIN_PEXTP_MAC2, MT6991_POWER_DOMAIN_PEXTP_PHY2},
	{MT6991_POWER_DOMAIN_ADSP_INFRA, MT6991_POWER_DOMAIN_AUDIO},
	{MT6991_POWER_DOMAIN_ADSP_INFRA, MT6991_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6991_POWER_DOMAIN_ADSP_AO, MT6991_POWER_DOMAIN_ADSP_INFRA},
};

static const struct scp_subdomain scp_subdomain_mt6991_pbus_used[] = {
	{MT6991_PBUS_POWER_DOMAIN_SSUSB_P0, MT6991_PBUS_POWER_DOMAIN_SSUSB_DP_PHY_P0},
	{MT6991_PBUS_POWER_DOMAIN_SSUSB_P23, MT6991_PBUS_POWER_DOMAIN_SSUSB_PHY_P2},
	{MT6991_PBUS_POWER_DOMAIN_PEXTP_MAC0, MT6991_PBUS_POWER_DOMAIN_PEXTP_PHY0},
	{MT6991_PBUS_POWER_DOMAIN_PEXTP_MAC1, MT6991_PBUS_POWER_DOMAIN_PEXTP_PHY1},
	{MT6991_PBUS_POWER_DOMAIN_PEXTP_MAC2, MT6991_PBUS_POWER_DOMAIN_PEXTP_PHY2},
	{MT6991_PBUS_POWER_DOMAIN_ADSP_INFRA, MT6991_PBUS_POWER_DOMAIN_AUDIO},
	{MT6991_PBUS_POWER_DOMAIN_ADSP_INFRA, MT6991_PBUS_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6991_PBUS_POWER_DOMAIN_ADSP_AO, MT6991_PBUS_POWER_DOMAIN_ADSP_INFRA},
};

static const struct scp_subdomain scp_subdomain_mt6991_pbus_unused[] = {
};

static const struct scp_soc_data mt6991_spm_data = {
	.domains = scp_domain_mt6991_spm_data,
	.num_domains = MT6991_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_spm),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

static const struct scp_soc_data mt6991_spm_hwv_data = {
	.domains = scp_domain_mt6991_spm_hwv_data,
	.num_domains = MT6991_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_spm),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

static const struct scp_soc_data mt6991_pbus_used_data = {
	.domains = scp_domain_mt6991_pbus_used_data,
	.num_domains = MT6991_PBUS_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_pbus_used,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_pbus_used),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

static const struct scp_soc_data mt6991_pbus_unused_data = {
	.domains = scp_domain_mt6991_pbus_unused_data,
	.num_domains = MT6991_NONPBUS_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_pbus_unused,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_pbus_unused),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_spm_match_tbl[] = {
	{
		.compatible = "mediatek,mt6991-scpsys",
		.data = &mt6991_spm_data,
	}, {
		.compatible = "mediatek,mt6991-scpsys-hwv",
		.data = &mt6991_spm_hwv_data,
	}, {
		.compatible = "mediatek,mt6991-scpsys-pbus-used",
		.data = &mt6991_pbus_used_data,
	}, {
		.compatible = "mediatek,mt6991-scpsys-pbus-unused",
		.data = &mt6991_pbus_unused_data,
	}, {
		/* sentinel */
	}
};

static int mt6991_scpsys_spm_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
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
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6991_scpsys_spm_drv = {
	.probe = mt6991_scpsys_spm_probe,
	.driver = {
		.name = "mtk-scpsys-mt6991-spm",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_spm_match_tbl),
	},
};

struct generic_pm_domain *mt6991_mmup_get_power_domain(void)
{

	if (pd_data->num_domains == MT6991_SPM_POWER_DOMAIN_NR)
		return pd_data->domains[MT6991_POWER_DOMAIN_MM_PROC_DORMANT];
	else if (pd_data->num_domains == MT6991_NONPBUS_POWER_DOMAIN_NR)
		return pd_data->domains[MT6991_NONPBUS_POWER_DOMAIN_MM_PROC_DORMANT];
	else {
		pr_err("%s: Unexpected pd_data->num_domains(%d)\n", __func__, pd_data->num_domains);
		BUG_ON(1);
	}

}
EXPORT_SYMBOL_GPL(mt6991_mmup_get_power_domain);

module_platform_driver(mt6991_scpsys_spm_drv);
MODULE_LICENSE("GPL");
