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

#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_TRAW	(BIT(10))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_DIP	(BIT(11))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_MAIN	(BIT(12))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_VCORE	(BIT(0))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_EIS	(BIT(13))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_TNR	(BIT(14))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_LITE	(BIT(15))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE0	(BIT(16))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE1	(BIT(17))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE_VCORE0	(BIT(1))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN0	(BIT(2))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN1	(BIT(18))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN2	(BIT(19))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MRAW	(BIT(20))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWA	(BIT(21))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWB	(BIT(22))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWC	(BIT(23))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSA	(BIT(24))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSB	(BIT(25))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSC	(BIT(26))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MAIN	(BIT(27))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_VCORE	(BIT(3))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_CCU	(BIT(28))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_DISP_VCORE	(BIT(4))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS0	(BIT(30))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS1	(BIT(31))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL0	(BIT(0))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL1	(BIT(1))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_EDPTX	(BIT(2))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_DPTX	(BIT(3))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_MML0	(BIT(4))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_MML1	(BIT(5))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA0	(BIT(5))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA1	(BIT(6))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA_AO	(BIT(7))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_BS_RX	(BIT(8))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_LS_RX	(BIT(9))

enum regmap_type {
	INVALID_TYPE = 0,
	MMPC_TYPE = 1,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[MMPC_TYPE] = "mmpc",
};

/*
 * MT6991 power domain support
 */

static const struct scp_domain_data scp_domain_mt6991_mmpc_data[] = {
	[MT6991_POWER_DOMAIN_ISP_TRAW] = {
		.name = "isp-traw",
		.ctl_offs = 0x000,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_TRAW),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_DIP] = {
		.name = "isp-dip",
		.ctl_offs = 0x004,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_DIP),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.ctl_offs = 0x008,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.ctl_offs = 0x00C,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_EIS] = {
		.name = "isp-wpe-eis",
		.ctl_offs = 0x010,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_EIS),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_TNR] = {
		.name = "isp-wpe-tnr",
		.ctl_offs = 0x014,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_TNR),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_LITE] = {
		.name = "isp-wpe-lite",
		.ctl_offs = 0x018,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_LITE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0x01C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.ctl_offs = 0x020,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE_VCORE0] = {
		.name = "vde-vcore0",
		.ctl_offs = 0x024,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE_VCORE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0x028,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.ctl_offs = 0x02C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN2] = {
		.name = "ven2",
		.ctl_offs = 0x030,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam-mraw",
		.ctl_offs = 0x034,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MRAW),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam-rawa",
		.ctl_offs = 0x038,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam-rawb",
		.ctl_offs = 0x03C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWC] = {
		.name = "cam-rawc",
		.ctl_offs = 0x040,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWC),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSA] = {
		.name = "cam-rmsa",
		.ctl_offs = 0x044,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSB] = {
		.name = "cam-rmsb",
		.ctl_offs = 0x048,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSC] = {
		.name = "cam-rmsc",
		.ctl_offs = 0x04C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSC),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0x050,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.ctl_offs = 0x054,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.ctl_offs = 0x058,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_CCU),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_VCORE] = {
		.name = "disp-vcore",
		.ctl_offs = 0x060,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_DISP_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DIS0_DORMANT] = {
		.name = "dis0-dormant",
		.ctl_offs = 0x064,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DIS1_DORMANT] = {
		.name = "dis1-dormant",
		.ctl_offs = 0x068,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_OVL0_DORMANT] = {
		.name = "ovl0-dormant",
		.ctl_offs = 0x06C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_OVL1_DORMANT] = {
		.name = "ovl1-dormant",
		.ctl_offs = 0x070,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_EDPTX_DORMANT] = {
		.name = "disp-edptx-dormant",
		.ctl_offs = 0x074,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_EDPTX),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_DPTX_DORMANT] = {
		.name = "disp-dptx-dormant",
		.ctl_offs = 0x078,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_DPTX),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MML0_SHUTDOWN] = {
		.name = "mml0-shutdown",
		.ctl_offs = 0x07C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_MML0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MML1_SHUTDOWN] = {
		.name = "mml1-shutdown",
		.ctl_offs = 0x080,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_MML1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA0] = {
		.name = "mm-infra0",
		.ctl_offs = 0x084,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA1] = {
		.name = "mm-infra1",
		.ctl_offs = 0x088,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA_AO] = {
		.name = "mm-infra-ao",
		.ctl_offs = 0x08C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CSI_BS_RX] = {
		.name = "csi-bs-rx",
		.ctl_offs = 0x090,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_BS_RX),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CSI_LS_RX] = {
		.name = "csi-ls-rx",
		.ctl_offs = 0x094,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_LS_RX),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY0] = {
		.name = "dsi-phy0",
		.ctl_offs = 0x0F0,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY1] = {
		.name = "dsi-phy1",
		.ctl_offs = 0x0F4,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY2] = {
		.name = "dsi-phy2",
		.ctl_offs = 0x0F8,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
};

static const struct scp_domain_data scp_domain_mt6991_mmpc_hwv_data[] = {
	[MT6991_POWER_DOMAIN_ISP_TRAW] = {
		.name = "isp-traw",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_DIP] = {
		.name = "isp-dip",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 1,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 2,
		.chk_data = {
			.hwv_debug_mux_ofs_opt = 0x174,
			.hwv_debug_mux_shift_opt = 0x3000000,
		},
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap | MTK_SCPD_HWV_CHK_MUX_OPT,
	},
	[MT6991_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 3,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_EIS] = {
		.name = "isp-wpe-eis",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 4,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_TNR] = {
		.name = "isp-wpe-tnr",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 5,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_LITE] = {
		.name = "isp-wpe-lite",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 6,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 7,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 8,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE_VCORE0] = {
		.name = "vde-vcore0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 9,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 10,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 11,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN2] = {
		.name = "ven2",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 12,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam-mraw",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 13,
		.chk_data = {
			.hwv_debug_mux_ofs_opt = 0x174,
			.hwv_debug_mux_shift_opt = 0x200000,
		},
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap | MTK_SCPD_HWV_CHK_MUX_OPT,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam-rawa",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 14,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam-rawb",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 15,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWC] = {
		.name = "cam-rawc",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 16,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSA] = {
		.name = "cam-rmsa",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 17,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSB] = {
		.name = "cam-rmsb",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 18,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSC] = {
		.name = "cam-rmsc",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 19,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 20,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 21,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.ctl_offs = 0x058,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ccusys"},
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_CCU),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_VCORE] = {
		.name = "disp-vcore",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 24,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_DIS0_DORMANT] = {
		.name = "dis0-dormant",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 25,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_DIS1_DORMANT] = {
		.name = "dis1-dormant",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 26,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_OVL0_DORMANT] = {
		.name = "ovl0-dormant",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 27,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_OVL1_DORMANT] = {
		.name = "ovl1-dormant",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 28,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_EDPTX_DORMANT] = {
		.name = "disp-edptx-dormant",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 29,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_DPTX_DORMANT] = {
		.name = "disp-dptx-dormant",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 30,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_MML0_SHUTDOWN] = {
		.name = "mml0-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0218,
		.hwv_clr_ofs = 0x021C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 31,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_MML1_SHUTDOWN] = {
		.name = "mml1-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0220,
		.hwv_clr_ofs = 0x0224,
		.hwv_done_ofs = 0x142C,
		.hwv_en_ofs = 0x1420,
		.hwv_set_sta_ofs = 0x1474,
		.hwv_clr_sta_ofs = 0x1478,
		.hwv_shift = 0,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA0] = {
		.name = "mm-infra0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0210,
		.hwv_clr_ofs = 0x0214,
		.hwv_done_ofs = 0x140C,
		.hwv_en_ofs = 0x1400,
		.hwv_set_sta_ofs = 0x1464,
		.hwv_clr_sta_ofs = 0x1468,
		.hwv_shift = 1,
		.hwv_debug_history_ofs = 0x3F9C,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_IRQ_SAVE | MTK_SCPD_BYPASS_INIT_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA1] = {
		.name = "mm-infra1",
		.hwv_comp = "mminfra-hwv-regmap",
		.hwv_ofs = 0x0100,
		.hwv_set_ofs = 0x0104,
		.hwv_clr_ofs = 0x0108,
		.hwv_done_ofs = 0x0144,
		.hwv_shift = 2,
		.sta_mask = 1,
		.caps = MTK_SCPD_MMINFRA_HWV_OPS | MTK_SCPD_IRQ_SAVE | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA_AO] = {
		.name = "mm-infra-ao",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0210,
		.hwv_clr_ofs = 0x0214,
		.hwv_done_ofs = 0x140C,
		.hwv_en_ofs = 0x1400,
		.hwv_set_sta_ofs = 0x1464,
		.hwv_clr_sta_ofs = 0x1468,
		.hwv_shift = 0,
		.hwv_debug_history_ofs = 0x3F9C,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_IRQ_SAVE | default_cap,
	},
	[MT6991_POWER_DOMAIN_CSI_BS_RX] = {
		.name = "csi-bs-rx",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0220,
		.hwv_clr_ofs = 0x0224,
		.hwv_done_ofs = 0x142C,
		.hwv_en_ofs = 0x1420,
		.hwv_set_sta_ofs = 0x1474,
		.hwv_clr_sta_ofs = 0x1478,
		.hwv_shift = 5,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_CSI_LS_RX] = {
		.name = "csi-ls-rx",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0220,
		.hwv_clr_ofs = 0x0224,
		.hwv_done_ofs = 0x142C,
		.hwv_en_ofs = 0x1420,
		.hwv_set_sta_ofs = 0x1474,
		.hwv_clr_sta_ofs = 0x1478,
		.hwv_shift = 6,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY0] = {
		.name = "dsi-phy0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0220,
		.hwv_clr_ofs = 0x0224,
		.hwv_done_ofs = 0x142C,
		.hwv_en_ofs = 0x1420,
		.hwv_set_sta_ofs = 0x1474,
		.hwv_clr_sta_ofs = 0x1478,
		.hwv_shift = 7,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY1] = {
		.name = "dsi-phy1",
		.ctl_offs = 0x0F4,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY2] = {
		.name = "dsi-phy2",
		.ctl_offs = 0x0F8,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt6991_mmpc[] = {
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_TRAW},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_DIP},
	{MT6991_POWER_DOMAIN_ISP_VCORE, MT6991_POWER_DOMAIN_ISP_MAIN},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_ISP_VCORE},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_WPE_EIS},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_WPE_TNR},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_WPE_LITE},
	{MT6991_POWER_DOMAIN_VDE_VCORE0, MT6991_POWER_DOMAIN_VDE0},
	{MT6991_POWER_DOMAIN_VDE_VCORE0, MT6991_POWER_DOMAIN_VDE1},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_VDE_VCORE0},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_VEN0},
	{MT6991_POWER_DOMAIN_VEN0, MT6991_POWER_DOMAIN_VEN1},
	{MT6991_POWER_DOMAIN_VEN1, MT6991_POWER_DOMAIN_VEN2},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_MRAW},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_RAWA},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_RAWB},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_RAWC},
	{MT6991_POWER_DOMAIN_CAM_RAWA, MT6991_POWER_DOMAIN_CAM_RMSA},
	{MT6991_POWER_DOMAIN_CAM_RAWB, MT6991_POWER_DOMAIN_CAM_RMSB},
	{MT6991_POWER_DOMAIN_CAM_RAWC, MT6991_POWER_DOMAIN_CAM_RMSC},
	{MT6991_POWER_DOMAIN_CAM_VCORE, MT6991_POWER_DOMAIN_CAM_MAIN},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_CAM_VCORE},
	{MT6991_POWER_DOMAIN_CAM_VCORE, MT6991_POWER_DOMAIN_CAM_CCU},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DIS0_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DIS1_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_OVL0_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_OVL1_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DISP_EDPTX_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DISP_DPTX_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_MML0_SHUTDOWN},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_MML1_SHUTDOWN},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_CSI_BS_RX},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_CSI_LS_RX},
};

static const struct scp_soc_data mt6991_mmpc_data = {
	.domains = scp_domain_mt6991_mmpc_data,
	.num_domains = MT6991_MMPC_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_mmpc,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_mmpc),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

static const struct scp_soc_data mt6991_mmpc_hwv_data = {
	.domains = scp_domain_mt6991_mmpc_hwv_data,
	.num_domains = MT6991_MMPC_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_mmpc,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_mmpc),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_mmpc_match_tbl[] = {
	{
		.compatible = "mediatek,mt6991-hfrpsys",
		.data = &mt6991_mmpc_data,
	}, {
		.compatible = "mediatek,mt6991-hfrpsys-hwv",
		.data = &mt6991_mmpc_hwv_data,
	}, {
		/* sentinel */
	}
};

extern struct generic_pm_domain *mt6991_mmup_get_power_domain(void);

static int mt6991_scpsys_mmpc_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	struct generic_pm_domain *mmup_pd;
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
	/* add mmup as parent of mminfra&disp vcore */
	mmup_pd = mt6991_mmup_get_power_domain();
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_MM_INFRA_AO]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
			ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_MM_INFRA0]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n", ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_MM_INFRA1]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n", ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_DISP_VCORE]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n", ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_CSI_BS_RX]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
			ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_CSI_LS_RX]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
			ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_DSI_PHY0]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
			ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_DSI_PHY1]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
			ret);
		return ret;
	}
	ret = pm_genpd_add_subdomain(mmup_pd, pd_data->domains[MT6991_POWER_DOMAIN_DSI_PHY2]);
	if (ret && IS_ENABLED(CONFIG_PM)) {
		dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
			ret);
		return ret;
	}

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

static struct platform_driver mt6991_scpsys_mmpc_drv = {
	.probe = mt6991_scpsys_mmpc_probe,
	.driver = {
		.name = "mtk-scpsys-mt6991-mmpc",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_mmpc_match_tbl),
	},
};

module_platform_driver(mt6991_scpsys_mmpc_drv);
MODULE_LICENSE("GPL");
