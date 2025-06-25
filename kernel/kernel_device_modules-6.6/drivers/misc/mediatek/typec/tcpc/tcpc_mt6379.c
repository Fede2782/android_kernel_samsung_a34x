// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/iio/consumer.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci_core.h"
#include "inc/std_tcpci_v10.h"

#define MT6379_INFO_EN		1
#define MT6379_DBGINFO_EN	0
#define MT6379_WD1_EN		1
#define MT6379_WD2_EN		1
#define VBUS_TO_CC_DEBOUNCE	100
#define MT6379_FOD_SRC_EN	0

#define MT6379_INFO(fmt, ...) \
	do { \
		if (MT6379_INFO_EN) \
			pd_dbg_info("%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define MT6379_DBGINFO(fmt, ...) \
	do { \
		if (MT6379_DBGINFO_EN) \
			pd_dbg_info("%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define MT6379_VID	0x29CF
#define MT6379_PID	0x6379

#define MT6379_IRQ_WAKE_TIME	(500) /* ms */

/* PMU for cc open */
#define MT6379_REG_CORECTRL2	0x106
#define MT6379_MSK_CCOPEN_SEL	BIT(4)
#define MT6379_REG_PD_EVT	0x16A
#define MT6379_REG_PD_STAT	0x18A

/* Vendor Register Define */
#define MT6379_REG_PHYCTRL1	(0x80)
#define MT6379_REG_PHYCTRL2	(0x81)
#define MT6379_REG_PHYCTRL3	(0x82)
#define MT6379_REG_PHYCTRL7	(0x86)
#define MT6379_REG_PHYCTRL8	(0x89)
#define MT6379_REG_VCONCTRL1	(0x8A)
#define MT6379_REG_VCONCTRL2	(0x8B)
#define MT6379_REG_VCONCTRL3	(0x8C)
#define MT6379_REG_SYSCTRL1	(0x8F)
#define MT6379_REG_SYSCTRL2	(0x90)
#define MT6379_REG_MTMASK1	(0x91)
#define MT6379_REG_MTMASK2	(0x92)
#define MT6379_REG_MTMASK3	(0x93)
#define MT6379_REG_MTMASK4	(0x94)
#define MT6379_REG_MTMASK5	(0x95)
#define MT6379_REG_MTMASK6	(0x96)
#define MT6379_REG_MTMASK7	(0x97)
#define MT6379_REG_MTINT1	(0x98)
#define MT6379_REG_MTINT2	(0x99)
#define MT6379_REG_MTINT3	(0x9A)
#define MT6379_REG_MTINT4	(0x9B)
#define MT6379_REG_MTINT5	(0x9C)
#define MT6379_REG_MTINT6	(0x9D)
#define MT6379_REG_MTINT7	(0x9E)
#define MT6379_REG_MTST1	(0x9F)
#define MT6379_REG_MTST2	(0xA0)
#define MT6379_REG_MTST3	(0xA1)
#define MT6379_REG_MTST4	(0xA2)
#define MT6379_REG_MTST5	(0xA3)
#define MT6379_REG_MTST6	(0xA4)
#define MT6379_REG_MTST7	(0xA5)
#define MT6379_REG_PHYCTRL9	(0xAC)
#define MT6379_REG_SYSCTRL3	(0xB0)
#define MT6379_REG_TCPCCTRL1	(0xB1)
#define MT6379_REG_TCPCCTRL2	(0xB2)
#define MT6379_REG_TCPCCTRL3	(0xB3)
#define MT6379_REG_LPWRCTRL3	(0xBB)
#define MT6379_REG_LPWRCTRL5	(0xBD)
#define MT6379_REG_WATCHDOGCTRL	(0xBE)
#define MT6379_REG_I2CTORSTCTRL	(0xBF)
#define MT6379_REG_VCONN_LATCH	(0xC0)
#define MT6379_REG_HILOCTRL9	(0xC8)
#define MT6379_REG_HILOCTRL10	(0xC9)
#define MT6379_REG_SHIELDCTRL1	(0xCA)
#define MT6379_REG_FRS_CTRL2	(0xCC)
#define MT6379_REG_TYPECOTPCTRL	(0xCD)
#define MT6379_REG_FODCTRL	(0xCF)
#define MT6379_REG_WD12MODECTRL	(0xD0)
#define MT6379_REG_WD1PATHEN	(0xD1)
#define MT6379_REG_WD1MISCCTRL	(0xD2)
#define MT6379_REG_WD2PATHEN	(0xD3)
#define MT6379_REG_WD2MISCCTRL	(0xD4)
#define MT6379_REG_WD1PULLST	(0xD5)
#define MT6379_REG_WD1DISCHGST	(0xD6)
#define MT6379_REG_WD2PULLST	(0xD7)
#define MT6379_REG_WD2DISCHGST	(0xD8)
#define MT6379_REG_WD0MODECTRL	(0xD9)
#define MT6379_REG_WD0SET	(0xDA)
#define MT6379_REG_WDSET	(0xDB)
#define MT6379_REG_WDSET1	(0xDC)

/* RT2 */
#define MT6379_REG_WDSET2	(0x520)
#define MT6379_REG_WDSET3	(0x521)
#define MT6379_REG_WD1MISCSET	(0x522)
#define MT6379_REG_WD1VOLCMP	(0x523)
#define MT6379_REG_WD2MISCSET	(0x528)
#define MT6379_REG_WD2VOLCMP	(0x529)

/* MT6379 */
#define MT6379_REG_IRQ_IND	0x10B
#define MT6379_MASK_PD_IND_EVT	BIT(2)
#define MT6379_REG_SPMI_TXDRV2	0x12B
#define MT6379_MASK_RCS_INT_DONE BIT(0)

/* Mask & Shift */
/* MT6379_REG_PHYCTRL8: 0x89 */
#define MT6379_MSK_PRLRSTB	BIT(1)
/* MT6379_REG_VCONCTRL1: 0x8A*/
#define MT6379_MSK_VCON_RCPCPEN	BIT(0)
#define MT6379_MSK_VCON_RCP_EN	BIT(1)
#define MT6379_MSK_VCON_DET_EN	BIT(7)
#define MT6379_MSK_VCON_CTRL1_PROT	\
	(MT6379_MSK_VCON_DET_EN | MT6379_MSK_VCON_RCP_EN |	\
	 MT6379_MSK_VCON_RCPCPEN)
/* MT6379_REG_VCONCTRL2: 0x8B */
#define MT6379_MSK_VCON_RVPCPEN	BIT(1)
#define MT6379_MSK_VCON_RVPEN	BIT(3)
#define MT6379_MSK_VCON_OVCCEN	BIT(7)
#define MT6379_MSK_VCON_PROTEN	\
	(MT6379_MSK_VCON_RVPEN | MT6379_MSK_VCON_OVCCEN | \
	 MT6379_MSK_VCON_RVPCPEN)
/* MT6379_REG_VCONCTRL2: 0x8C */
#define MT6379_MSK_VCON_OVPEN	BIT(4)
/* MT6379_REG_SYSCTRL1: 0x8F */
#define MT6379_MSK_AUTOIDLE_TOUT	GENMASK(2, 0)
#define MT6379_MSK_AUTOIDLE_EN	BIT(3)
#define MT6379_MSK_SHIPPING_OFF	BIT(5)
#define MT6379_MSK_INTN_SELECT	BIT(7)

#define MT6379_VAL_AUTOIDLE_6P4_MS	0
/* MT6379_REG_SYSCTRL2: 0x90 */
#define MT6379_MSK_BMCIOOSC_EN	BIT(0)
#define MT6379_MSK_VBUSDET_EN	BIT(1)
#define MT6379_MSK_LPWR_EN	BIT(3)
/* MT6379_REG_MTINT1: 0x98 */
#define MT6379_MSK_WAKEUP	BIT(0)
#define MT6379_MSK_VBUS80	BIT(1)
#define MT6379_MSK_TYPECOTP	BIT(2)
#define MT6379_MSK_VBUSVALID	BIT(5)
/* MT6379_REG_MTINT2: 0x99 */
#define MT6379_MSK_VCON_OVCC1	BIT(0)
#define MT6379_MSK_VCON_OVCC2	BIT(1)
#define MT6379_MSK_VCON_RVP	BIT(2)
#define MT6379_MSK_VCON_UVP	BIT(4)
#define MT6379_MSK_VCON_SHTGND	BIT(5)
#define MT6379_MSK_VCON_FAULT \
	(MT6379_MSK_VCON_OVCC1 | MT6379_MSK_VCON_OVCC2 | MT6379_MSK_VCON_RVP | \
	 MT6379_MSK_VCON_UVP | MT6379_MSK_VCON_SHTGND)
/* MT6379_REG_MTINT3: 0x9A */
#define MT6379_MSK_CTD		BIT(4)
#define MT6379_SFT_VBUS_TO_CC1	(6)
#define MT6379_MSK_VBUS_TO_CC1	BIT(6)
#define MT6379_MSK_VBUS_TO_CC2	BIT(7)
#define MT6379_MSK_VBUS_TO_CC \
	(MT6379_MSK_VBUS_TO_CC1 | MT6379_MSK_VBUS_TO_CC2)
/* MT6379_REG_MTINT4: 0x9B */
#define MT6379_MSK_FOD_DONE	BIT(0)
#define MT6379_MSK_FOD_OV	BIT(1)
#define MT6379_MSK_FOD_LR	BIT(5)
#define MT6379_MSK_FOD_HR	BIT(6)
#define MT6379_MSK_FOD_DISCHGF	BIT(7)
#define MT6379_MSK_FOD_ALL \
	(MT6379_MSK_FOD_DONE | MT6379_MSK_FOD_OV | MT6379_MSK_FOD_LR | \
	 MT6379_MSK_FOD_HR | MT6379_MSK_FOD_DISCHGF)
/* MT6379_REG_MTINT5: 0x9C */
#define MT6379_SFT_HIDET_CC1	(4)
#define MT6379_MSK_HIDET_CC1	BIT(4)
#define MT6379_MSK_HIDET_CC2	BIT(5)
#define MT6379_MSK_HIDET_CC	(MT6379_MSK_HIDET_CC1 | MT6379_MSK_HIDET_CC2)
/* MT6379_REG_MTINT7: 0x9E */
#define MT6379_MSK_WD12_STFALL	BIT(0)
#define MT6379_MSK_WD12_STRISE	BIT(1)
#define MT6379_MSK_WD12_DONE	BIT(2)
#define MT6379_MSK_WD0_STFALL	BIT(3)
#define MT6379_MSK_WD0_STRISE	BIT(4)
/* MT6379_REG_MTST3: 0xA1 */
#define MT6379_MSK_CABLE_TYPEC	BIT(4)
#define MT6379_MSK_CABLE_TYPEA	BIT(5)
/* MT6379_REG_WATCHDOGCTRL: 0xBE */
#define MT6379_MSK_VBUSDISCHG_OPT	BIT(6)
/* MT6379_REG_I2CTORSTCTRL: 0xBF */
#define MT6379_MSK_I2CTORST_SEL		GENMASK(3, 0)
#define MT6379_MSK_VCONN_UVP_CPEN	BIT(5)
#define MT6379_MSK_VCONN_OCP_CPEN	BIT(4)
#define MT6379_MSK_I2CTORST_EN		BIT(7)
#define MT6379_MSK_VCONN_UVP_OCP_CPEN \
	(MT6379_MSK_VCONN_UVP_CPEN | MT6379_MSK_VCONN_OCP_CPEN)
/* MT6379_REG_VCONN_LATCH: 0xC0 */
#define MT6379_MSK_RCP_LATCH_EN BIT(3)
#define MT6379_MSK_OVP_LATCH_EN BIT(2)
#define MT6379_MSK_RVP_LATCH_EN BIT(1)
#define MT6379_MSK_UVP_LATCH_EN BIT(0)
#define MT6379_MSK_VCONN_LATCH_EN \
	(MT6379_MSK_RCP_LATCH_EN | MT6379_MSK_OVP_LATCH_EN | \
	 MT6379_MSK_RVP_LATCH_EN | MT6379_MSK_UVP_LATCH_EN)
/* MT6379_REG_HILOCTRL10: 0xC9 */
#define MT6379_MSK_HIDET_CC1_CMPEN	BIT(1)
#define MT6379_MSK_HIDET_CC2_CMPEN	BIT(4)
#define MT6379_MSK_HIDET_CC_CMPEN \
	(MT6379_MSK_HIDET_CC1_CMPEN | MT6379_MSK_HIDET_CC2_CMPEN)
/* MT6379_REG_SHIELDCTRL1: 0xCA */
#define MT6379_MSK_CTD_EN	BIT(1)
#define MT6379_MSK_RDDET_AUTO	BIT(3)
#define MT6379_MSK_OPEN40MS_EN	BIT(4)
#define MT6379_MSK_RPDET_MANUAL	BIT(6)
#define MT6379_MSK_RPDET_AUTO	BIT(7)
/* MT6379_REG_FRS_CTRL2: 0xCC */
#define MT6379_MSK_CMPEN_VBUS_TO_CC1	BIT(0)
#define MT6379_MSK_CMPEN_VBUS_TO_CC2	BIT(1)
#define MT6379_MSK_CMPEN_VBUS_TO_CC \
	(MT6379_MSK_CMPEN_VBUS_TO_CC1 | MT6379_MSK_CMPEN_VBUS_TO_CC2)
/* MT6379_REG_TYPECOTPCTRL: 0xCD */
#define MT6379_MSK_TYPECOTP_HWEN	BIT(0)
#define MT6379_MSK_TYPECOTP_FWEN	BIT(2)
/* MT6379_REG_FODCTRL: 0xCF */
#define MT6379_MSK_FOD_SRC_EN	BIT(3)
#define MT6379_MSK_FOD_SNK_EN	BIT(6)
#define MT6379_MSK_FOD_FW_EN	BIT(7)
/* MT6379_REG_WD12MODECTRL: 0xD0 */
#define MT6379_MSK_WD12MODE_EN	BIT(4)
#define MT6379_MSK_WD12PROT	BIT(6)
/* MT6379_REG_WD1PATHEN: 0xD1 */
#define MT6379_MSK_WDSBU1_EN	BIT(0)
#define MT6379_MSK_WDSBU2_EN	BIT(1)
#define MT6379_MSK_WDCC1_EN	BIT(2)
#define MT6379_MSK_WDCC2_EN	BIT(3)
#define MT6379_MSK_WDDP_EN	BIT(4)
#define MT6379_MSK_WDDM_EN	BIT(5)
/* MT6379_REG_WD1MISCCTRL: 0xD2 */
#define MT6379_MSK_WDFWMODE_EN	BIT(0)
#define MT6379_MSK_WDDISCHG_EN	BIT(1)
#define MT6379_MSK_WDRPULL_EN	BIT(2)
#define MT6379_MSK_WDIPULL_EN	BIT(3)
/* MT6379_REG_WD0MODECTRL: 0xD9 */
#define MT6379_MSK_WD0MODE_EN	BIT(4)
/* MT6379_REG_WD0SET: 0xDA */
#define MT6379_MSK_WD0PULL_STS	BIT(7)
/* MT6379_REG_WDSET: 0xDB */
#define MT6379_MSK_WDLDO_SEL	GENMASK(7, 6)
#define MT6379_SFT_WDLDO_SEL	(6)
/* RT2 MT6379_REG_WDSET3: 0x521 */
#define MT6379_MASK_WD_TDET	GENMASK(2, 0)
#define MT6379_SHFT_WD_TDET	(0)
/* RT2 MT6379_REG_WD1MISCSET: 0x522 */
#define MT6379_MSK_WDIPULL_SEL	GENMASK(6, 4)
#define MT6379_SFT_WDIPULL_SEL	(4)
#define MT6379_MSK_WDRPULL_SEL	GENMASK(3, 1)
#define MT6379_SFT_WDRPULL_SEL	(1)
/* RT2 MT6379_REG_WD1VOLCMP: 0x523 */
#define MT6379_MSK_WD12_VOLCOML	GENMASK(3, 0)
#define MT6379_SFT_WD12_VOLCOML	(0)

#define MT6379_MSK_WD0_TSLEEP	GENMASK(5, 4)
#define MT6379_SFT_WD0_TSLEEP	(4)
#define MT6379_MSK_WD0_TDET	GENMASK(2, 0)
#define MT6379_SFT_WD0_TDET	(0)

struct mt6379_tcpc_data {
	struct device *dev;
	struct regmap *rmap;
	struct tcpc_desc *desc;
	struct tcpc_device *tcpc;
	struct iio_channel *adc_iio;
	int irq;
	u8 wd0_tsleep;
	u8 wd0_tdet;

	atomic_t wd_protect_rty;
	bool wd_polling;
	struct tcpc_device *tcpc_port1;
	struct delayed_work wd_polling_dwork;
	struct delayed_work wd12_strise_irq_dwork;
	struct delayed_work fod_polling_dwork;

	struct delayed_work vbus_to_cc_dwork;
};

enum mt6379_vend_int {
	MT6379_VEND_INT1 = 0,
	MT6379_VEND_INT2,
	MT6379_VEND_INT3,
	MT6379_VEND_INT4,
	MT6379_VEND_INT5,
	MT6379_VEND_INT6,
	MT6379_VEND_INT7,
	MT6379_VEND_INT_NUM,
};

enum mt6379_wd_ldo {
	MT6379_WD_LDO_0_6V,
	MT6379_WD_LDO_1_8V,
	MT6379_WD_LDO_2_5V,
	MT6379_WD_LDO_3_0V,
};

enum mt6379_wd_status {
	MT6379_WD_PULL,
	MT6379_WD_DISCHG,
	MT6379_WD_STATUS_NUM,
};

enum mt6379_wd_chan {
	MT6379_WD_CHAN_WD1 = 0,
	MT6379_WD_CHAN_WD2,
	MT6379_WD_CHAN_NUM,
};

enum mt6379_wd_ipull {
	MT6379_WD_IPULL_2UA,
	MT6379_WD_IPULL_6UA,
	MT6379_WD_IPULL_10UA,
	MT6379_WD_IPULL_20UA,
	MT6379_WD_IPULL_40UA,
	MT6379_WD_IPULL_80UA,
	MT6379_WD_IPULL_160UA,
	MT6379_WD_IPULL_240UA,
};

enum mt6379_wd_rpull {
	MT6379_WD_RPULL_500K,
	MT6379_WD_RPULL_200K,
	MT6379_WD_RPULL_75K,
	MT6379_WD_RPULL_40K,
	MT6379_WD_RPULL_20K,
	MT6379_WD_RPULL_10K,
	MT6379_WD_RPULL_5K,
	MT6379_WD_RPULL_1K,
};

enum mt6379_wd_volcmpl {
	MT6379_WD_VOLCMPL_200MV,
	MT6379_WD_VOLCMPL_240MV,
	MT6379_WD_VOLCMPL_400MV,
	MT6379_WD_VOLCMPL_440MV,
	MT6379_WD_VOLCMPL_600MV,
	MT6379_WD_VOLCMPL_700MV,
	MT6379_WD_VOLCMPL_1000MV,
	MT6379_WD_VOLCMPL_1100MV,
	MT6379_WD_VOLCMPL_1200MV,
	MT6379_WD_VOLCMPL_1300MV,
	MT6379_WD_VOLCMPL_1440MV,
	MT6379_WD_VOLCMPL_1540MV,
	MT6379_WD_VOLCMPL_2000MV,
	MT6379_WD_VOLCMPL_2100MV,
	MT6379_WD_VOLCMPL_2200MV,
	MT6379_WD_VOLCMPL_2300MV,
};

enum mt6379_wd_tdet {
	MT6379_WD_TDET_400US,
	MT6379_WD_TDET_1MS,
	MT6379_WD_TDET_2MS,
	MT6379_WD_TDET_4MS,
	MT6379_WD_TDET_10MS,
	MT6379_WD_TDET_40MS,
	MT6379_WD_TDET_100MS,
	MT6379_WD_TDET_400MS,
};

/* REG RT2 0x20 ~ 0x2D */
static const u8 mt6379_rt2_wd_init_setting[] = {
	0x50, 0x34, 0x44, 0xCA, 0x00, 0x00, 0x00, 0x00,
	0x44, 0xCA, 0x00, 0x00, 0x00, 0x00,
};

static const bool mt6379_wd_chan_en[MT6379_WD_CHAN_NUM] = {
	MT6379_WD1_EN,
	MT6379_WD2_EN,
};

static const u8 mt6379_wd_path_reg[MT6379_WD_CHAN_NUM] = {
	MT6379_REG_WD1PATHEN,
	MT6379_REG_WD2PATHEN,
};

static const u8 mt6379_wd_polling_path[MT6379_WD_CHAN_NUM] = {
	MT6379_MSK_WDSBU1_EN,
	MT6379_MSK_WDSBU2_EN,
};

static const u8 mt6379_wd_protection_path[MT6379_WD_CHAN_NUM] = {
	MT6379_MSK_WDSBU1_EN | MT6379_MSK_WDSBU2_EN |
#if !CONFIG_WD_DURING_PLUGGED_IN
	MT6379_MSK_WDDP_EN | MT6379_MSK_WDDM_EN |
#endif	/* CONFIG_WD_DURING_PLUGGED_IN */
	MT6379_MSK_WDCC1_EN | MT6379_MSK_WDCC2_EN,
	MT6379_MSK_WDSBU1_EN | MT6379_MSK_WDSBU2_EN |
#if !CONFIG_WD_DURING_PLUGGED_IN
	MT6379_MSK_WDDP_EN | MT6379_MSK_WDDM_EN |
#endif	/* CONFIG_WD_DURING_PLUGGED_IN */
	MT6379_MSK_WDCC1_EN | MT6379_MSK_WDCC2_EN,
};

static const u8 mt6379_wd_miscctrl_reg[MT6379_WD_CHAN_NUM] = {
	MT6379_REG_WD1MISCCTRL,
	MT6379_REG_WD2MISCCTRL,
};

static const u8 mt6379_wd_status_reg[MT6379_WD_CHAN_NUM] = {
	MT6379_REG_WD1PULLST,
	MT6379_REG_WD2PULLST,
};

static const u16 mt6379_wd_rpull_reg[MT6379_WD_CHAN_NUM] = {
	MT6379_REG_WD1MISCSET,
	MT6379_REG_WD2MISCSET
};

static const u16 mt6379_wd_volcmp_reg[MT6379_WD_CHAN_NUM] = {
	MT6379_REG_WD1VOLCMP,
	MT6379_REG_WD2VOLCMP,
};


struct tcpc_desc def_tcpc_desc = {
	.role_def = TYPEC_ROLE_DRP,
	.rp_lvl = TYPEC_RP_DFT,
	.vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS,
	.name = "type_c_port0",
	.en_wd = false,
	.en_ctd = false,
	.en_fod = false,
	.en_typec_otp = false,
	.en_floatgnd = false,
	.wd_sbu_calib_init = CONFIG_WD_SBU_CALIB_INIT,
	.wd_sbu_pl_bound = CONFIG_WD_SBU_PL_BOUND,
	.wd_sbu_pl_lbound_c2c = CONFIG_WD_SBU_PL_LBOUND_C2C,
	.wd_sbu_pl_ubound_c2c = CONFIG_WD_SBU_PL_UBOUND_C2C,
	.wd_sbu_ph_auddev = CONFIG_WD_SBU_PH_AUDDEV,
	.wd_sbu_ph_lbound = CONFIG_WD_SBU_PH_LBOUND,
	.wd_sbu_ph_lbound1_c2c = CONFIG_WD_SBU_PH_LBOUND1_C2C,
	.wd_sbu_ph_ubound1_c2c = CONFIG_WD_SBU_PH_UBOUND1_C2C,
	.wd_sbu_ph_ubound2_c2c = CONFIG_WD_SBU_PH_UBOUND2_C2C,
	.wd_sbu_aud_ubound = CONFIG_WD_SBU_AUD_UBOUND,
};

#define CHIP_REG_BASE_MASK	0xFF00
#define CHIP_REG_OFFSET_MASK	0x00FF

#define CHIP_REG_BASE_PD	0x0000
#define MT6379_REG_PDBASEADDR	0x0400
#define CHIP_REG_BASE_PD2	0x0500
#define MT6379_REG_PD2BASEADDR	0x0500
#define CHIP_REG_BASE_PMU	0x0100
#define MT6379_REG_PMUBASEADDR	0x0000

static inline u32 rg_xlate(struct mt6379_tcpc_data *ddata, u32 reg)
{
	if ((reg & CHIP_REG_BASE_MASK) == CHIP_REG_BASE_PD)
		return (reg & CHIP_REG_OFFSET_MASK) | MT6379_REG_PDBASEADDR;
	if ((reg & CHIP_REG_BASE_MASK) == CHIP_REG_BASE_PD2)
		return (reg & CHIP_REG_OFFSET_MASK) | MT6379_REG_PD2BASEADDR;
	if ((reg & CHIP_REG_BASE_MASK) == CHIP_REG_BASE_PMU)
		return (reg & CHIP_REG_OFFSET_MASK) | MT6379_REG_PMUBASEADDR;

	dev_err(ddata->dev, "%s: failed to translate reg0x%04x\n", __func__, reg);
	return reg;
}

static int mt6379_write_helper(struct mt6379_tcpc_data *ddata, u32 reg,
			       const void *data, size_t count)
{
	struct tcpc_device *tcpc = ddata->tcpc;
	int ret = 0;

	atomic_inc(&tcpc->suspend_pending);
	wait_event(tcpc->resume_wait_que,
		   !ddata->dev->parent->power.is_suspended);
	ret = regmap_bulk_write(ddata->rmap, rg_xlate(ddata, reg), data, count);
	atomic_dec_if_positive(&tcpc->suspend_pending);
	return ret;
}

static int mt6379_read_helper(struct mt6379_tcpc_data *ddata, u32 reg,
			      void *data, size_t count)
{
	struct tcpc_device *tcpc = ddata->tcpc;
	int ret = 0;

	atomic_inc(&tcpc->suspend_pending);
	wait_event(tcpc->resume_wait_que,
		   !ddata->dev->parent->power.is_suspended);
	ret = regmap_bulk_read(ddata->rmap, rg_xlate(ddata, reg), data, count);
	atomic_dec_if_positive(&tcpc->suspend_pending);
	return ret;
}

static inline int mt6379_write8(struct mt6379_tcpc_data *ddata, u32 reg,
				u8 data)
{
	return mt6379_write_helper(ddata, reg, &data, 1);
}

static inline int mt6379_read8(struct mt6379_tcpc_data *ddata, u32 reg,
			       u8 *data)
{
	return mt6379_read_helper(ddata, reg, data, 1);
}

static inline int mt6379_write16(struct mt6379_tcpc_data *ddata, u32 reg,
				 u16 data)
{
	data = cpu_to_le16(data);
	return mt6379_write_helper(ddata, reg, &data, 2);
}

static inline int mt6379_read16(struct mt6379_tcpc_data *ddata, u32 reg,
				u16 *data)
{
	int ret;

	ret = mt6379_read_helper(ddata, reg, data, 2);
	if (ret < 0)
		return ret;
	*data = le16_to_cpu(*data);
	return 0;
}

static inline int mt6379_bulk_write(struct mt6379_tcpc_data *ddata, u32 reg,
				    const void *data, size_t count)
{
	return mt6379_write_helper(ddata, reg, data, count);
}


static inline int mt6379_bulk_read(struct mt6379_tcpc_data *ddata, u32 reg,
				   void *data, size_t count)
{
	return mt6379_read_helper(ddata, reg, data, count);
}

static inline int mt6379_update_bits(struct mt6379_tcpc_data *ddata, u32 reg,
				     u8 mask, u8 data)
{
	u8 orig_data = 0;
	int ret = 0;

	ret = mt6379_read_helper(ddata, reg, &orig_data, 1);
	if (ret < 0)
		return ret;

	orig_data &= ~mask;
	orig_data |= data & mask;

	return mt6379_write_helper(ddata, reg, &orig_data, 1);
}

static inline int mt6379_set_bits(struct mt6379_tcpc_data *ddata, u32 reg,
				  u8 mask)
{
	return mt6379_update_bits(ddata, reg, mask, mask);
}

static inline int mt6379_clr_bits(struct mt6379_tcpc_data *ddata, u32 reg,
				  u8 mask)
{
	return mt6379_update_bits(ddata, reg, mask, 0);
}

static int mt6379_sw_reset(struct mt6379_tcpc_data *ddata)
{
	int ret;

	ret = mt6379_write8(ddata, MT6379_REG_SYSCTRL3, 0x01);
	if (ret < 0)
		return ret;
	usleep_range(1000, 2000);
	return 0;
}

static inline int mt6379_init_vend_mask(struct mt6379_tcpc_data *ddata)
{
	u8 mask[MT6379_VEND_INT_NUM] = {0};
	struct tcpc_device *tcpc = ddata->tcpc;

	mask[MT6379_VEND_INT1] |= MT6379_MSK_WAKEUP |
				  MT6379_MSK_VBUS80 |
				  MT6379_MSK_VBUSVALID;

	if (tcpc->tcpc_flags & TCPC_FLAGS_TYPEC_OTP)
		mask[MT6379_VEND_INT1] |= MT6379_MSK_TYPECOTP;

	mask[MT6379_VEND_INT2] |= MT6379_MSK_VCON_FAULT;

	if (tcpc->tcpc_flags & TCPC_FLAGS_CABLE_TYPE_DETECTION)
		mask[MT6379_VEND_INT3] |= MT6379_MSK_CTD;

	if (tcpc->tcpc_flags & TCPC_FLAGS_VBUS_SHORT_CC)
		mask[MT6379_VEND_INT3] |= MT6379_MSK_VBUS_TO_CC;

	if (tcpc->tcpc_flags & TCPC_FLAGS_FOREIGN_OBJECT_DETECTION)
		mask[MT6379_VEND_INT4] |= MT6379_MSK_FOD_DONE |
					  MT6379_MSK_FOD_OV |
					  MT6379_MSK_FOD_DISCHGF;

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION)
		mask[MT6379_VEND_INT7] |= MT6379_MSK_WD12_STRISE |
					  MT6379_MSK_WD12_DONE;

	if (tcpc->tcpc_flags & TCPC_FLAGS_FLOATING_GROUND)
		mask[MT6379_VEND_INT7] |= MT6379_MSK_WD0_STFALL |
					  MT6379_MSK_WD0_STRISE;

	return mt6379_bulk_write(ddata, MT6379_REG_MTMASK1, mask,
				 MT6379_VEND_INT_NUM);
}

static inline int mt6379_init_alert_mask(struct mt6379_tcpc_data *ddata)
{
	u16 mask = TCPC_V10_REG_ALERT_VENDOR_DEFINED |
		   TCPC_V10_REG_ALERT_VBUS_SINK_DISCONNECT |
		   TCPC_V10_REG_ALERT_FAULT | TCPC_V10_REG_ALERT_CC_STATUS;
	u8 masks[4] = {0x00, 0x00, 0x00,
		       TCPC_V10_REG_FAULT_STATUS_VCONN_OC};

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS |
		TCPC_V10_REG_ALERT_TX_DISCARDED |
		TCPC_V10_REG_ALERT_TX_FAILED |
		TCPC_V10_REG_ALERT_RX_HARD_RST |
		TCPC_V10_REG_ALERT_RX_STATUS |
		TCPC_V10_REG_ALERT_RX_OVERFLOW;
#endif /* CONFIG_USB_POWER_DELIVERY */
	*(u16 *)masks = cpu_to_le16(mask);
	return mt6379_bulk_write(ddata, TCPC_V10_REG_ALERT_MASK,
				masks, sizeof(masks));
}

static int mt6379_enable_vsafe0v_detect(struct mt6379_tcpc_data *ddata, bool en)
{
	MT6379_DBGINFO("en = %d\n",en);
	return (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_MTMASK1, MT6379_MSK_VBUS80);
}

static int mt6379_enable_rpdet_auto(struct mt6379_tcpc_data *ddata, bool en)
{
	return (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_SHIELDCTRL1, MT6379_MSK_RPDET_AUTO);
}

static int mt6379_is_vconn_fault(struct mt6379_tcpc_data *ddata, bool *fault)
{
	int ret;
	u8 status;

	ret = mt6379_read8(ddata, MT6379_REG_MTST2, &status);
	if (ret < 0)
		return ret;
	*fault = (status & MT6379_MSK_VCON_FAULT) ? true : false;
	return 0;
}

static int mt6379_vend_alert_status_clear(struct mt6379_tcpc_data *ddata,
					  const u8 *mask)
{
	return mt6379_bulk_write(ddata, MT6379_REG_MTINT1, mask,
				 MT6379_VEND_INT_NUM);
}


static int mt6379_enable_typec_otp_fwen(struct tcpc_device *tcpc, bool en)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	MT6379_DBGINFO("en=%d\n", en);
	return (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_TYPECOTPCTRL, MT6379_MSK_TYPECOTP_FWEN);
}

static int mt6379_set_vbus_short_cc(struct tcpc_device *tcpc,
				    bool cc1, bool cc2)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 val = 0;

	MT6379_DBGINFO("cc1:%d, cc2:%d\n", cc1, cc2);
	mt6379_write8(ddata, MT6379_REG_MTINT3, MT6379_MSK_VBUS_TO_CC);

	val = (cc1 ? MT6379_MSK_CMPEN_VBUS_TO_CC1 : 0) |
		(cc2 ? MT6379_MSK_CMPEN_VBUS_TO_CC2 : 0);

	return mt6379_update_bits(ddata, MT6379_REG_FRS_CTRL2,
				 MT6379_MSK_CMPEN_VBUS_TO_CC, val);
}

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
static int mt6379_set_force_discharge(struct tcpc_device *tcpc, bool en, int mv)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, TCPC_V10_REG_POWER_CTRL, TCPC_V10_REG_FORCE_DISC_EN);
}
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

static int __mt6379_get_cc_hi(struct mt6379_tcpc_data *ddata)
{
	int ret;
	u8 data;

	ret = mt6379_read8(ddata, MT6379_REG_MTST5, &data);
	if (ret < 0)
		return ret;
	return ((data ^ MT6379_MSK_HIDET_CC) & MT6379_MSK_HIDET_CC)
		>> MT6379_SFT_HIDET_CC1;
}

static void mt6379_vbus_to_cc_dwork_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6379_tcpc_data *ddata = container_of(dwork,
						      struct mt6379_tcpc_data,
						      vbus_to_cc_dwork);
	int ret = 0;
	u8 data = 0;

	pm_system_wakeup();

	ret = mt6379_read8(ddata, MT6379_REG_MTST3, &data);
	if (ret < 0) {
		MT6379_DBGINFO("Read MT6379_REG_MTST3 fail.\n");
		return;
	}
	data = (data & MT6379_MSK_VBUS_TO_CC) >> MT6379_SFT_VBUS_TO_CC1;
	tcpci_notify_vbus_short_cc_status(ddata->tcpc, data);
}

static int mt6379_hidet_cc_evt_process(struct mt6379_tcpc_data *ddata)
{
	int ret;

	ret = __mt6379_get_cc_hi(ddata);
	if (ret < 0)
		return ret;
	return tcpci_notify_cc_hi(ddata->tcpc, ret);
}

static int mt6379_get_fod_status(struct mt6379_tcpc_data *ddata,
				 enum tcpc_fod_status *fod)
{
	int ret;
	u8 data;

	ret = mt6379_read8(ddata, MT6379_REG_MTST4, &data);
	if (ret < 0)
		return ret;
	data &= MT6379_MSK_FOD_ALL;

	/* LR possesses the highest priority */
	if (data & MT6379_MSK_FOD_LR)
		*fod = TCPC_FOD_LR;
	else if (data & MT6379_MSK_FOD_HR)
		*fod = TCPC_FOD_HR;
	else if (data & MT6379_MSK_FOD_DISCHGF)
		*fod = TCPC_FOD_DISCHG_FAIL;
	else if (data & MT6379_MSK_FOD_OV)
		*fod = TCPC_FOD_OV;
	else
		*fod = TCPC_FOD_NONE;
	return 0;
}

static void mt6379_fod_polling_dwork_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6379_tcpc_data *ddata = container_of(dwork,
						      struct mt6379_tcpc_data,
						      fod_polling_dwork);

	pm_system_wakeup();

	MT6379_DBGINFO("Set FOD_FW_EN\n");
	tcpci_lock_typec(ddata->tcpc);
	mt6379_set_bits(ddata, MT6379_REG_FODCTRL, MT6379_MSK_FOD_FW_EN);
	tcpci_unlock_typec(ddata->tcpc);
}

static int mt6379_fod_evt_process(struct mt6379_tcpc_data *ddata)
{
	int ret = 0;
	enum tcpc_fod_status fod = TCPC_FOD_NONE;
	struct tcpc_device *tcpc = ddata->tcpc;

	ret = mt6379_get_fod_status(ddata, &fod);
	if (ret < 0)
		return ret;
	ret = mt6379_clr_bits(ddata, MT6379_REG_FODCTRL, MT6379_MSK_FOD_FW_EN);
	if (ret < 0)
		return ret;
	if (fod == TCPC_FOD_LR)
		mod_delayed_work(system_wq, &ddata->fod_polling_dwork,
				 msecs_to_jiffies(5000));
	return tcpc_typec_handle_fod(tcpc, fod);
}

#if CONFIG_CABLE_TYPE_DETECTION
static int mt6379_get_cable_type(struct mt6379_tcpc_data *ddata,
				 enum tcpc_cable_type *type)
{
	int ret;
	u8 data;

	ret = mt6379_read8(ddata, MT6379_REG_MTST3, &data);
	if (ret < 0)
		return ret;
	if (data & MT6379_MSK_CABLE_TYPEC)
		*type = TCPC_CABLE_TYPE_C2C;
	else if (data & MT6379_MSK_CABLE_TYPEA)
		*type = TCPC_CABLE_TYPE_A2C;
	else
		*type = TCPC_CABLE_TYPE_NONE;
	return 0;
}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

static int mt6379_set_wd_ldo(struct mt6379_tcpc_data *ddata,
			     enum mt6379_wd_ldo ldo)
{
	return mt6379_update_bits(ddata, MT6379_REG_WDSET, MT6379_MSK_WDLDO_SEL,
				  ldo << MT6379_SFT_WDLDO_SEL);
}

static int mt6379_init_wd(struct mt6379_tcpc_data *ddata)
{
	/*
	 * WD_LDO = 1.8V
	 * WD_CHPHYS_EN = 0
	 * WD_SWITCH_CNT = 100
	 * WD_POLL_SWITCH = 0
	 * WD12_TDET_ALWAYS = 0, depend on WD_TDET[2:0]
	 * WD0_TDET_ALWAYS = 0, depend on WD_TDET[2:0]
	 */
	mt6379_write8(ddata, MT6379_REG_WDSET, 0x50);

	/* WD_EXIT_CNT = 4times */
	mt6379_set_bits(ddata, MT6379_REG_WDSET1, 0x02);

	/* WD1_RPULL_EN = 1, WD1_DISCHG_EN = 1 */
	mt6379_write8(ddata, MT6379_REG_WD1MISCCTRL, 0x06);

	/* WD2_RPULL_EN = 1, WD2_DISCHG_EN = 1 */
	mt6379_write8(ddata, MT6379_REG_WD2MISCCTRL, 0x06);

	/* WD0_RPULL_EN = 1, WD0_DISCHG_EN = 1 */
	mt6379_write8(ddata, MT6379_REG_WD0SET, 0x06);

	mt6379_set_wd_ldo(ddata, MT6379_WD_LDO_1_8V);

	mt6379_bulk_write(ddata, MT6379_REG_WDSET2, mt6379_rt2_wd_init_setting,
			  ARRAY_SIZE(mt6379_rt2_wd_init_setting));
	return 0;
}

static int mt6379_set_wd_volcmpl(struct mt6379_tcpc_data *ddata,
				 enum mt6379_wd_chan chan,
				 enum mt6379_wd_volcmpl vcmpl)
{
	return mt6379_update_bits(ddata,
				  mt6379_wd_volcmp_reg[chan],
				  MT6379_MSK_WD12_VOLCOML,
				  vcmpl << MT6379_SFT_WD12_VOLCOML);
}

static int mt6379_set_wd_rpull(struct mt6379_tcpc_data *ddata,
			       enum mt6379_wd_chan chan,
			       enum mt6379_wd_rpull rpull)
{
	return mt6379_update_bits(ddata, mt6379_wd_rpull_reg[chan],
				  MT6379_MSK_WDRPULL_SEL,
				  rpull << MT6379_SFT_WDRPULL_SEL);
}

static int mt6379_set_wd_path(struct mt6379_tcpc_data *ddata,
			      enum mt6379_wd_chan chan, u8 path)
{
	return mt6379_write8(ddata, mt6379_wd_path_reg[chan], path);
}

static int mt6379_get_wd_path(struct mt6379_tcpc_data *ddata,
			      enum mt6379_wd_chan chan, u8 *path)
{
	return mt6379_read8(ddata, mt6379_wd_path_reg[chan], path);
}

static int mt6379_set_wd_polling_path(struct mt6379_tcpc_data *ddata,
				      enum mt6379_wd_chan chan)
{
	return mt6379_set_wd_path(ddata, chan, mt6379_wd_polling_path[chan]);
}

static int mt6379_set_wd_protection_path(struct mt6379_tcpc_data *ddata,
					 enum mt6379_wd_chan chan)
{
	return mt6379_set_wd_path(ddata, chan, mt6379_wd_protection_path[chan]);
}

static int mt6379_set_wd_polling_parameter(struct mt6379_tcpc_data *ddata,
					   enum mt6379_wd_chan chan)
{
	int ret;

	ret = mt6379_set_wd_rpull(ddata, chan, MT6379_WD_RPULL_75K);
	if (ret < 0)
		return ret;
	ret = mt6379_set_wd_volcmpl(ddata, chan, MT6379_WD_VOLCMPL_1440MV);
	if (ret < 0)
		return ret;
	ret = mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
			    MT6379_MSK_WDRPULL_EN | MT6379_MSK_WDDISCHG_EN);
	if (ret < 0)
		return ret;
	return mt6379_set_wd_polling_path(ddata, chan);
}

static int mt6379_set_wd_protection_parameter(struct mt6379_tcpc_data *ddata,
					      enum mt6379_wd_chan chan)
{
	int ret;

	ret = mt6379_set_wd_rpull(ddata, chan, MT6379_WD_RPULL_75K);
	if (ret < 0)
		return ret;
	ret = mt6379_set_wd_volcmpl(ddata, chan, MT6379_WD_VOLCMPL_1440MV);
	if (ret < 0)
		return ret;
	ret = mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
			    MT6379_MSK_WDRPULL_EN | MT6379_MSK_WDDISCHG_EN);
	if (ret < 0)
		return ret;
	return mt6379_set_wd_protection_path(ddata, chan);
}

static int mt6379_check_wd_status(struct mt6379_tcpc_data *ddata,
				  enum mt6379_wd_chan chan, bool *error)
{
	int i, ret;
	u8 path;
	u8 data[MT6379_WD_STATUS_NUM];

	ret = mt6379_bulk_read(ddata, mt6379_wd_status_reg[chan], data,
			       MT6379_WD_STATUS_NUM);
	if (ret < 0)
		return ret;
	ret = mt6379_get_wd_path(ddata, chan, &path);
	if (ret < 0)
		return ret;
	*error = false;
	for (i = 0; i < MT6379_WD_STATUS_NUM; i++) {
		if (path & data[i])
			*error = true;
		MT6379_DBGINFO("chan(path,stat)=%d(0x%02X,0x%02X)\n", chan,
			       path, data[i]);
	}
	return 0;
}

static int mt6379_enable_wd_dischg(struct mt6379_tcpc_data *ddata,
				   enum mt6379_wd_chan chan, bool en)
{
	int ret;

	if (en) {
		ret = mt6379_set_wd_polling_path(ddata, chan);
		if (ret < 0)
			return ret;
		ret = mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
				    MT6379_MSK_WDDISCHG_EN |
				    MT6379_MSK_WDFWMODE_EN);
	} else {
		ret = mt6379_set_wd_path(ddata, chan, 0);
		if (ret < 0)
			return ret;
		ret = mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
				    MT6379_MSK_WDRPULL_EN |
				    MT6379_MSK_WDDISCHG_EN);
	}
	return ret;
}

static int mt6379_enable_wd_pullup(struct mt6379_tcpc_data *ddata,
				   enum mt6379_wd_chan chan,
				   enum mt6379_wd_rpull rpull, bool en)
{
	int ret;

	if (en) {
		ret = mt6379_set_wd_polling_path(ddata, chan);
		if (ret < 0)
			return ret;
		ret = mt6379_set_wd_rpull(ddata, chan, rpull);
		if (ret < 0)
			return ret;
		ret = mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
				    MT6379_MSK_WDRPULL_EN |
				    MT6379_MSK_WDFWMODE_EN);
	} else {
		ret = mt6379_set_wd_path(ddata, chan, 0);
		if (ret < 0)
			return ret;
		ret = mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
				    MT6379_MSK_WDRPULL_EN |
				    MT6379_MSK_WDDISCHG_EN);
	}
	return ret;
}

static int mt6379_get_wd_adc(struct mt6379_tcpc_data *ddata,
			     enum mt6379_wd_chan chan, int *val)
{
	int ret;

	ret = iio_read_channel_processed(&ddata->adc_iio[chan], val);
	if (ret < 0)
		return ret;
	*val /= 1000;
	return 0;
}

static bool mt6379_is_wd_audio_device(struct mt6379_tcpc_data *ddata,
				      enum mt6379_wd_chan chan, int wd_adc)
{
	struct tcpc_desc *desc = ddata->desc;
	int ret;

	if (wd_adc >= desc->wd_sbu_ph_auddev)
		return false;

	/* Pull high with 1K resistor */
	ret = mt6379_enable_wd_pullup(ddata, chan, MT6379_WD_RPULL_1K, true);
	if (ret < 0) {
		MT6379_DBGINFO("chan%d pull up 1k fail(%d)\n", chan, ret);
		goto not_auddev;
	}

	ret = mt6379_get_wd_adc(ddata, chan, &wd_adc);
	if (ret < 0) {
		MT6379_DBGINFO("get chan%d adc fail(%d)\n", chan, ret);
		goto not_auddev;
	}

	if (wd_adc >= desc->wd_sbu_aud_ubound)
		goto not_auddev;
	return true;

not_auddev:
	mt6379_enable_wd_pullup(ddata, chan, MT6379_WD_RPULL_500K, true);
	return false;
}

static int mt6379_is_water_detected(struct mt6379_tcpc_data *ddata,
				    enum mt6379_wd_chan chan, bool *wd)
{
	int ret, wd_adc, i;
	struct tcpc_desc *desc = ddata->desc;
	u32 lb = desc->wd_sbu_ph_lbound;
	u32 ub = desc->wd_sbu_calib_init * 110 / 100;
	enum tcpc_cable_type cable_type;
	u8 ctd_evt;
	struct tcpc_device *tcpc = ddata->tcpc;

	if (tcpc->tcpc_flags & TCPC_FLAGS_WD_DUAL_PORT &&
	    chan == MT6379_WD_CHAN_WD2)
		tcpc = ddata->tcpc_port1;

	/* Check WD1/2 pulled low */
	for (i = 0; i < CONFIG_WD_SBU_PL_RETRY; i++) {
		ret = mt6379_enable_wd_dischg(ddata, chan, true);
		if (ret < 0) {
			MT6379_DBGINFO("en chan%d dischg fail(%d)\n", chan,
				       ret);
			goto out;
		}
		ret = mt6379_get_wd_adc(ddata, chan, &wd_adc);
		if (ret < 0) {
			MT6379_DBGINFO("get chan%d adc fail(%d)\n", chan, ret);
			goto out;
		}
		MT6379_DBGINFO("chan%d pull low %dmV\n", chan, wd_adc);
		ret = mt6379_enable_wd_dischg(ddata, chan, false);
		if (ret < 0) {
			MT6379_DBGINFO("disable chan%d dischg fail(%d)\n", chan,
				       ret);
			goto out;
		}
		if (wd_adc <= desc->wd_sbu_pl_bound ||
			(wd_adc >= desc->wd_sbu_pl_lbound_c2c &&
			wd_adc <= desc->wd_sbu_pl_ubound_c2c))
			break;
	}
	if (i == CONFIG_WD_SBU_PL_RETRY) {
		*wd = true;
		goto out;
	}

	ret = mt6379_enable_wd_pullup(ddata, chan, MT6379_WD_RPULL_500K, true);
	if (ret < 0) {
		MT6379_DBGINFO("chan%d pull up 500k fail(%d)\n", chan, ret);
		goto out;
	}

	for (i = 0; i < CONFIG_WD_SBU_PH_RETRY; i++) {
		ret = mt6379_get_wd_adc(ddata, chan, &wd_adc);
		if (ret < 0) {
			MT6379_DBGINFO("get chan%d adc fail(%d)\n", chan, ret);
			goto out;
		}
		MT6379_DBGINFO("chan%d pull high %dmV(lb %d, ub %d)\n", chan,
			       wd_adc, lb, ub);
		if ((wd_adc >= lb && wd_adc <= ub)
#if CONFIG_WD_DURING_PLUGGED_IN
		    || (wd_adc > CONFIG_WD_SBU_PH_TITAN_LBOUND &&
			wd_adc < CONFIG_WD_SBU_PH_TITAN_UBOUND)
#endif	/* CONFIG_WD_DURING_PLUGGED_IN */
		   ) {
			*wd = false;
			goto out;
		}
		msleep(20);
	}

#if CONFIG_CABLE_TYPE_DETECTION
	if (tcpc->tcpc_flags & TCPC_FLAGS_CABLE_TYPE_DETECTION) {
		cable_type = tcpc->typec_cable_type;
		if (cable_type == TCPC_CABLE_TYPE_NONE) {
			ret = mt6379_read8(ddata, MT6379_REG_MTINT3, &ctd_evt);
			if (ret >= 0 && (ctd_evt & MT6379_MSK_CTD))
				ret = mt6379_get_cable_type(ddata, &cable_type);
		}
		if (cable_type == TCPC_CABLE_TYPE_C2C) {
			if (((wd_adc >= desc->wd_sbu_ph_lbound1_c2c) &&
			    (wd_adc <= desc->wd_sbu_ph_ubound1_c2c)) ||
			    (wd_adc > desc->wd_sbu_ph_ubound2_c2c)) {
				MT6379_DBGINFO("ignore water for C2C\n");
				*wd = false;
				goto out;
			}
		}
	}
#endif /*CONFIG_CABLE_TYPE_DETECTION*/

	if (mt6379_is_wd_audio_device(ddata, chan, wd_adc)) {
		MT6379_DBGINFO("suspect audio device but not water\n");
		*wd = false;
		goto out;
	}
	*wd = true;
out:
	MT6379_DBGINFO("water %s\n", *wd ? "detected" : "not detected");
	mt6379_write8(ddata, mt6379_wd_miscctrl_reg[chan],
		      MT6379_MSK_WDRPULL_EN | MT6379_MSK_WDDISCHG_EN);
	return ret;
}

static int mt6379_enable_wd_polling(struct mt6379_tcpc_data *ddata, bool en)
{
	int ret, i;

	if (en) {
		ret = mt6379_update_bits(ddata,
					 MT6379_REG_WDSET3,
					 MT6379_MASK_WD_TDET,
					 MT6379_WD_TDET_10MS << MT6379_SHFT_WD_TDET);
		if (ret < 0)
			return ret;
		for (i = 0; i < MT6379_WD_CHAN_NUM; i++) {
			if (!mt6379_wd_chan_en[i])
				continue;
			ret = mt6379_set_wd_polling_parameter(ddata, i);
			if (ret < 0)
				return ret;
		}
	}
	ret = mt6379_write8(ddata, MT6379_REG_WD12MODECTRL,
			    en ? MT6379_MSK_WD12MODE_EN : 0);
	if (ret >= 0)
		ddata->wd_polling = en;
	return ret;
}

static int mt6379_enable_wd_protection(struct mt6379_tcpc_data *ddata, bool en)
{
	int i, ret;

	MT6379_DBGINFO("en = %d\n", en);
	if (en) {
		ret = mt6379_update_bits(ddata,
					 MT6379_REG_WDSET3,
					 MT6379_MASK_WD_TDET,
					 MT6379_WD_TDET_1MS << MT6379_SHFT_WD_TDET);
		if (ret < 0)
			return ret;
		for (i = 0; i < MT6379_WD_CHAN_NUM; i++) {
			if (!mt6379_wd_chan_en[i])
				continue;
			mt6379_set_wd_protection_parameter(ddata, i);
		}
	}
	return mt6379_write8(ddata, MT6379_REG_WD12MODECTRL,
			     en ?
			     MT6379_MSK_WD12MODE_EN | MT6379_MSK_WD12PROT : 0);
}

static void mt6379_wd_polling_dwork_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6379_tcpc_data *ddata = container_of(dwork,
						      struct mt6379_tcpc_data,
						      wd_polling_dwork);
#if !CONFIG_WD_DURING_PLUGGED_IN
	struct tcpc_device *tcpcs[] = {ddata->tcpc, ddata->tcpc_port1};
	const uint32_t tcpc_flags = ddata->tcpc->tcpc_flags;
	size_t array_size = (tcpc_flags & TCPC_FLAGS_WD_DUAL_PORT) ?  2 : 1;
	int i = 0, ret = 0;
#endif	/* !CONFIG_WD_DURING_PLUGGED_IN */

	pm_system_wakeup();

#if !CONFIG_WD_DURING_PLUGGED_IN
	for (i = 0; i < array_size; i++) {
		tcpci_lock_typec(tcpcs[i]);
		ret = tcpci_is_plugged_in(tcpcs[i]);
		tcpci_unlock_typec(tcpcs[i]);
		if (ret) {
			if (i != 0)
				schedule_delayed_work(dwork,
						   msecs_to_jiffies(10000));
			return;
		}
	}
#endif /* !CONFIG_WD_DURING_PLUGGED_IN */

	tcpci_lock_typec(ddata->tcpc);
	mt6379_enable_wd_polling(ddata, true);
	tcpci_unlock_typec(ddata->tcpc);
}

static int mt6379_wd_polling_evt_process(struct mt6379_tcpc_data *ddata)
{
	int i, ret;
	bool polling = true, error = false;
	struct tcpc_device *tcpcs[] = {ddata->tcpc, ddata->tcpc_port1};
	const uint32_t tcpc_flags = ddata->tcpc->tcpc_flags;
	size_t array_size = (tcpc_flags & TCPC_FLAGS_WD_DUAL_PORT) ?  2 : 1;

	if (!ddata->wd_polling)
		return 0;
	mt6379_enable_wd_polling(ddata, false);
#if !CONFIG_WD_DURING_PLUGGED_IN
	tcpci_unlock_typec(ddata->tcpc);
	for (i = 0; i < array_size; i++) {
		tcpci_lock_typec(tcpcs[i]);
		ret = tcpci_is_plugged_in(tcpcs[i]);
		tcpci_unlock_typec(tcpcs[i]);
		if (ret) {
			if (i != 0)
				schedule_delayed_work(&ddata->wd_polling_dwork,
						   msecs_to_jiffies(10000));
			tcpci_lock_typec(ddata->tcpc);
			return 0;
		}
	}
	tcpci_lock_typec(ddata->tcpc);
#endif	/* CONFIG_WD_DURING_PLUGGED_IN */
	for (i = 0; i < MT6379_WD_CHAN_NUM; i++) {
		if (!mt6379_wd_chan_en[i])
			continue;
		ret = mt6379_check_wd_status(ddata, i, &error);
		if (ret < 0 || !error)
			continue;
		ret = mt6379_is_water_detected(ddata, i, &error);
		if (ret < 0 || !error)
			continue;
		polling = false;
		break;
	}
	if (polling)
		mt6379_enable_wd_polling(ddata, true);
	else
		tcpc_typec_handle_wd(tcpcs, array_size, true);
	return 0;
}

static int mt6379_wd_protection_evt_process(struct mt6379_tcpc_data *ddata)
{
	int i, ret;
	bool error[2] = {false, false}, protection = false;
	struct tcpc_device *tcpcs[] = {ddata->tcpc, ddata->tcpc_port1};
	const uint32_t tcpc_flags = ddata->tcpc->tcpc_flags;
	size_t array_size = (tcpc_flags & TCPC_FLAGS_WD_DUAL_PORT) ?  2 : 1;

	for (i = 0; i < MT6379_WD_CHAN_NUM; i++) {
		if (!mt6379_wd_chan_en[i])
			continue;
		ret = mt6379_check_wd_status(ddata, i, &error[0]);
		if (ret < 0)
			goto out;
		ret = mt6379_is_water_detected(ddata, i, &error[1]);
		if (ret < 0)
			goto out;
		MT6379_DBGINFO("err1:%d, err2:%d\n", error[0], error[1]);
		if (!error[0] && !error[1])
			continue;
out:
		protection = true;
		break;
	}
	MT6379_DBGINFO("retry cnt = %d\n", atomic_read(&ddata->wd_protect_rty));
	if (!protection && atomic_dec_and_test(&ddata->wd_protect_rty)) {
		tcpc_typec_handle_wd(tcpcs, array_size, false);
		atomic_set(&ddata->wd_protect_rty,
			   CONFIG_WD_PROTECT_RETRY_COUNT);
	} else
		mt6379_enable_wd_protection(ddata, true);
	return 0;
}

static int mt6379_is_floating_ground_enabled(struct mt6379_tcpc_data *ddata,
					     bool *en)
{
	int ret;
	u8 data;

	ret = mt6379_read8(ddata, MT6379_REG_WD0MODECTRL, &data);
	if (ret < 0)
		return ret;
	*en = (data & MT6379_MSK_WD0MODE_EN) ? true : false;
	return 0;
}

static int mt6379_enable_floating_ground(struct mt6379_tcpc_data *ddata,
					 bool en)
{
	int ret = 0;
	u8 value = 0;

	MT6379_DBGINFO("en:%d\n", en);
	if (en) {
		/* set wd0 detect time */
		value |= (ddata->wd0_tsleep << MT6379_SFT_WD0_TSLEEP);
		value |= (ddata->wd0_tdet << MT6379_SFT_WD0_TDET);
		ret = mt6379_write8(ddata, MT6379_REG_WDSET3, value);
		if (ret < 0)
			return ret;
		ret = mt6379_set_wd_ldo(ddata, MT6379_WD_LDO_0_6V);
		if (ret < 0)
			return ret;
	}
	ret = (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_WD0MODECTRL, MT6379_MSK_WD0MODE_EN);
	if (!en) {
		ret = mt6379_set_wd_ldo(ddata, MT6379_WD_LDO_1_8V);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int mt6379_floating_ground_evt_process(struct mt6379_tcpc_data *ddata)
{
	int ret;
	bool en;
	u8 data;

	ret = mt6379_is_floating_ground_enabled(ddata, &en);
	if (ret < 0 || !en)
		return ret;
	ret = mt6379_read8(ddata, MT6379_REG_WD0SET, &data);
	if (ret < 0)
		return ret;
	return tcpci_notify_wd0_state(ddata->tcpc,
				      !!(data & MT6379_MSK_WD0PULL_STS));
}

/*
 * ==================================================================
 * TCPC ops
 * ==================================================================
 */

static int mt6379_init_mask(struct tcpc_device *tcpc)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	mt6379_init_vend_mask(ddata);
	mt6379_init_alert_mask(ddata);

	return 0;
}

static int mt6379_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	if (sw_reset) {
		ret = mt6379_sw_reset(ddata);
		if (ret < 0)
			return ret;
	}

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	mt6379_write8(ddata, TCPC_V10_REG_ROLE_CTRL,
		      TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	/* tTCPCFilter = 500us */
	mt6379_write8(ddata, MT6379_REG_TCPCCTRL1, 0x14);

	/*
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duty Ctrl : dcSRC / 1024
	 */
	mt6379_write8(ddata, MT6379_REG_TCPCCTRL2, 0);
	mt6379_write16(ddata, MT6379_REG_TCPCCTRL3, TCPC_NORMAL_RP_DUTY);

	/*
	 * Transition toggle count = 7
	 * OSC_FREQ_CFG = 0x01
	 * RXFilter out 100ns glich = 0x00
	 */
	mt6379_write8(ddata, MT6379_REG_PHYCTRL1, 0x74);

	/* PHY_CDR threshold = 0x3A */
	mt6379_write8(ddata, MT6379_REG_PHYCTRL2, 0x3A);

	/* Transition window time = 43.29us */
	mt6379_write8(ddata, MT6379_REG_PHYCTRL3, 0x82);

	/* BMC decoder idle time = 17.982us */
	mt6379_write8(ddata, MT6379_REG_PHYCTRL7, 0x36);

	/* Retry period = 26.208us */
	mt6379_write8(ddata, MT6379_REG_PHYCTRL9, 0x3C);

	/* Enable PD Vconn current limit mode, ocp sel 300mA, and analog OVP */
	mt6379_write8(ddata, MT6379_REG_VCONCTRL3, 0x51);

	/* VBUS_VALID debounce time: 375us */
	mt6379_write8(ddata, MT6379_REG_LPWRCTRL5, 0x2F);

	/* Set HILOCCFILTER 250us */
	mt6379_write8(ddata, MT6379_REG_HILOCTRL9, 0xAA);

	/* Enable CC open 40ms when PMIC SYSUV */
	mt6379_set_bits(ddata, MT6379_REG_SHIELDCTRL1, MT6379_MSK_OPEN40MS_EN);
	mt6379_set_bits(ddata, MT6379_REG_CORECTRL2, MT6379_MSK_CCOPEN_SEL);

	/*
	 * Enable Alert.CCStatus assertion
	 * when CCStatus.Looking4Connection changes
	 */
	mt6379_set_bits(ddata, TCPC_V10_REG_TCPC_CTRL,
			TCPC_V10_REG_TCPC_CTRL_EN_LOOK4CONNECTION_ALERT);

	mt6379_init_mask(tcpc);

	/* Enable auto dischg timer for IQ about 12mA consumption */
	mt6379_set_bits(ddata, MT6379_REG_WATCHDOGCTRL,
			MT6379_MSK_VBUSDISCHG_OPT);

	/* Disable bleed dischg for IQ about 2mA consumption */
	mt6379_clr_bits(ddata, TCPC_V10_REG_POWER_CTRL,
			TCPC_V10_REG_BLEED_DISC_EN);

	/* Set Low Power LDO to 2V */
	mt6379_write8(ddata, MT6379_REG_LPWRCTRL3, 0xF8);

	/* Sync IRQ to 3M path */
	mt6379_set_bits(ddata, MT6379_REG_SYSCTRL1, MT6379_MSK_INTN_SELECT);

	/* AUTOIDLE enable, TIMEOUT = 6.4ms */
	mt6379_update_bits(ddata, MT6379_REG_SYSCTRL1,
			   MT6379_MSK_AUTOIDLE_TOUT | MT6379_MSK_AUTOIDLE_EN,
			   MT6379_VAL_AUTOIDLE_6P4_MS | MT6379_MSK_AUTOIDLE_EN);
	/* SHIPPING off */
	mt6379_set_bits(ddata, MT6379_REG_SYSCTRL1, MT6379_MSK_SHIPPING_OFF);
	mdelay(1);

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) {
		if (tcpc->tcpc_flags & TCPC_FLAGS_WD_DUAL_PORT) {
			ddata->tcpc_port1 =
				tcpc_dev_get_by_name("type_c_port1");
			if (!ddata->tcpc_port1)
				tcpc->tcpc_flags &= ~TCPC_FLAGS_WD_DUAL_PORT;
		}
		mt6379_init_wd(ddata);
		schedule_delayed_work(&ddata->wd_polling_dwork,
				      msecs_to_jiffies(5000));
	}

	if (tcpc->tcpc_flags & TCPC_FLAGS_FLOATING_GROUND)
		mt6379_enable_floating_ground(ddata, true);

	return 0;
}

static int mt6379_alert_status_clear(struct tcpc_device *tcpc, u32 mask)
{
	u16 std_mask = mask & 0xffff;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return std_mask ?
	       mt6379_write16(ddata, TCPC_V10_REG_ALERT, std_mask) : 0;
}

static int mt6379_fault_status_clear(struct tcpc_device *tcpc, u8 status)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6379_write8(ddata, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6379_set_alert_mask(struct tcpc_device *tcpc, u32 mask)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	MT6379_DBGINFO("mask = 0x%04x\n", mask);
	return mt6379_write16(ddata, TCPC_V10_REG_ALERT_MASK, mask);
}

static int mt6379_get_alert_mask(struct tcpc_device *tcpc, u32 *mask)
{
	int ret = 0;
	u16 data = 0;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	ret = mt6379_read16(ddata, TCPC_V10_REG_ALERT_MASK, &data);
	if (ret < 0)
		return ret;
	*mask = data;
	return 0;
}

static int mt6379_get_alert_status_and_mask(struct tcpc_device *tcpc,
					    u32 *alert, u32 *mask)
{
	int ret;
	u8 buf[4] = {0};
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	ret = mt6379_bulk_read(ddata, TCPC_V10_REG_ALERT, buf, 4);
	if (ret < 0)
		return ret;
	*alert = le16_to_cpu(*(u16 *)&buf[0]);
	*mask = le16_to_cpu(*(u16 *)&buf[2]);
	return 0;
}

static int mt6379_vbus_change_helper(struct mt6379_tcpc_data *ddata)
{
	int ret;
	u8 data;
	struct tcpc_device *tcpc = ddata->tcpc;

	ret = mt6379_read8(ddata, MT6379_REG_MTST1, &data);
	if (ret < 0)
		return ret;
	tcpc->vbus_present = !!(data & MT6379_MSK_VBUSVALID);
	/*
	 * Vsafe0v only triggers when vbus falls under 0.8V,
	 * also update parameter if vbus present triggers
	 */
	tcpc->vbus_safe0v = !!(data & MT6379_MSK_VBUS80);
	return 0;
}

static int mt6379_get_power_status(struct tcpc_device *tcpc)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6379_vbus_change_helper(ddata);
}

static int mt6379_get_fault_status(struct tcpc_device *tcpc, u8 *status)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6379_read8(ddata, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6379_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	bool act_as_sink = false;
	u8 buf[4], status = 0, role_ctrl = 0, cc_role = 0;
	int ret = 0;

	ret = mt6379_bulk_read(ddata, TCPC_V10_REG_ROLE_CTRL, buf, sizeof(buf));
	if (ret < 0)
		return ret;
	role_ctrl = buf[0];
	status = buf[3];

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		if (role_ctrl & TCPC_V10_REG_ROLE_CTRL_DRP) {
			*cc1 = TYPEC_CC_DRP_TOGGLING;
			*cc2 = TYPEC_CC_DRP_TOGGLING;
			return 0;
		}
			/* Toggle reg0x1A[6] DRP = 1 and = 0 */
			mt6379_write8(ddata, TCPC_V10_REG_ROLE_CTRL,
				      role_ctrl | TCPC_V10_REG_ROLE_CTRL_DRP);
			mt6379_write8(ddata, TCPC_V10_REG_ROLE_CTRL, role_ctrl);
			return -EAGAIN;
		}
	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	if (role_ctrl & TCPC_V10_REG_ROLE_CTRL_DRP)
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	else {
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
			cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
		act_as_sink = (cc_role != TYPEC_CC_RP);
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */
	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);
	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);
	return 0;
}

static int mt6379_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret = 0;
	u8 data = 0;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	MT6379_INFO("%d\n", pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(1, rp_lvl, TYPEC_CC_RD,
						      TYPEC_CC_RD);
		ret = mt6379_write8(ddata, TCPC_V10_REG_ROLE_CTRL, data);
		if (ret < 0)
			return ret;
		/*
		 * Before set LOOK_CONNECTION, at least 30us needed after
		 * setting TCPC_V10_REG_ROLE_CTRL
		 */
		udelay(32);
		ret = mt6379_write8(ddata, TCPC_V10_REG_COMMAND,
				    TCPM_CMD_LOOK_CONNECTION);
	} else {
		pull2 = pull1 = pull;

		if (pull == TYPEC_CC_RP &&
		    tcpc->typec_state == typec_attached_src) {
			if (tcpc->typec_polarity)
				pull1 = TYPEC_CC_RD;
			else
				pull2 = TYPEC_CC_RD;
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
		ret = mt6379_write8(ddata, TCPC_V10_REG_ROLE_CTRL, data);
	}
	return ret;
}

static int mt6379_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return (polarity ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT);
}

static int mt6379_set_vconn(struct tcpc_device *tcpc, int en)
{
	int ret;
	bool fault = false;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	/*
	 * Set Vconn OVP RVP
	 * Otherwise vconn present fail will be triggered
	 */
	if (en) {
		mt6379_set_bits(ddata, MT6379_REG_VCONCTRL3,
				MT6379_MSK_VCON_OVPEN);
		mt6379_set_bits(ddata, MT6379_REG_VCONCTRL2,
				MT6379_MSK_VCON_PROTEN);
		usleep_range(20, 50);
		ret = mt6379_is_vconn_fault(ddata, &fault);
		if (ret >= 0 && fault)
			return -EINVAL;
	}
	ret = (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, TCPC_V10_REG_POWER_CTRL, TCPC_V10_REG_POWER_CTRL_VCONN);
	if (!en) {
		mt6379_clr_bits(ddata, MT6379_REG_VCONCTRL2,
				MT6379_MSK_VCON_PROTEN);

		mt6379_clr_bits(ddata, MT6379_REG_VCONCTRL3,
				MT6379_MSK_VCON_OVPEN);
	}
	mdelay(1);
	ret = (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_I2CTORSTCTRL, MT6379_MSK_VCONN_UVP_OCP_CPEN);

	return ret;
}

static int mt6379_tcpc_deinit(struct tcpc_device *tcpc)
{
	int cc1 = TYPEC_CC_VOLT_OPEN, cc2 = TYPEC_CC_VOLT_OPEN;

	mt6379_get_cc(tcpc, &cc1, &cc2);
	if (cc1 != TYPEC_CC_DRP_TOGGLING &&
	    (cc1 != TYPEC_CC_VOLT_OPEN || cc2 != TYPEC_CC_VOLT_OPEN)) {
	mt6379_set_cc(tcpc, TYPEC_CC_OPEN);
		usleep_range(20000, 30000);
	}
	return 0;
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int mt6379_protocol_reset(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_POWER_DELIVERY */
static int mt6379_set_low_power_mode(struct tcpc_device *tcpc, bool en,
				     int pull)
{
	int ret = 0;
	u8 data;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) {
#if CONFIG_WD_DURING_PLUGGED_IN
		if (en)
			ret = mt6379_enable_wd_polling(ddata, en);
#else
		ret = mt6379_enable_wd_polling(ddata, en);
#endif	/* CONFIG_WD_DURING_PLUGGED_IN */
		if (ret < 0)
			return ret;
	}

	if (tcpc->tcpc_flags & TCPC_FLAGS_TYPEC_OTP) {
		ret = mt6379_enable_typec_otp_fwen(tcpc, !en &&
				tcpc_typec_is_act_as_sink_role(tcpc));
		if (ret < 0)
			return ret;
	}

	ret = mt6379_enable_vsafe0v_detect(ddata, !en);
	if (ret < 0)
		return ret;
	if (en) {
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
		/* [Workaround]
		 * rx_buffer can't be cleared,
		 * try to reset protocol before disabling BMC clock
		 */
		mt6379_protocol_reset(tcpc);
		mt6379_alert_status_clear(tcpc, TCPC_V10_REG_ALERT_RX_ALL_MASK);
		mt6379_alert_status_clear(tcpc, TCPC_V10_REG_ALERT_RX_ALL_MASK);
#endif	/* CONFIG_USB_POWER_DELIVERY */
		data = MT6379_MSK_LPWR_EN;
#if CONFIG_TYPEC_CAP_NORP_SRC
		data |= MT6379_MSK_VBUSDET_EN;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	} else {
		data = MT6379_MSK_VBUSDET_EN | MT6379_MSK_BMCIOOSC_EN;
	}
	ret = mt6379_write8(ddata, MT6379_REG_SYSCTRL2, data);
	/* Let CC pins re-toggle */
	if (en && ret >= 0 &&
	    (tcpc->typec_local_cc & TYPEC_CC_DRP)) {
		udelay(32);
		ret = mt6379_write8(ddata, TCPC_V10_REG_COMMAND,
				    TCPM_CMD_LOOK_CONNECTION);
	}
	return ret;
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int mt6379_set_msg_header(struct tcpc_device *tcpc, u8 power_role,
				 u8 data_role)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(data_role, power_role);

	return mt6379_write8(ddata, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int mt6379_protocol_reset(struct tcpc_device *tcpc)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret = 0;
	u8 phy_ctrl8 = 0;

	ret = mt6379_read8(ddata, MT6379_REG_PHYCTRL8, &phy_ctrl8);
	if (ret < 0)
		return ret;
	ret = mt6379_write8(ddata, MT6379_REG_PHYCTRL8,
			    phy_ctrl8 & ~MT6379_MSK_PRLRSTB);
	if (ret < 0)
		return ret;
	udelay(20);
	return mt6379_write8(ddata, MT6379_REG_PHYCTRL8,
			     phy_ctrl8 | MT6379_MSK_PRLRSTB);
}

static int mt6379_set_rx_enable(struct tcpc_device *tcpc, u8 en)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6379_write8(ddata, TCPC_V10_REG_RX_DETECT, en);
}

static int mt6379_get_message(struct tcpc_device *tcpc, u32 *payload,
			      u16 *msg_head,
			      enum tcpm_transmit_type *frame_type)
{
	int ret = 0;
	u8 cnt = 0, buf[32] = {0};
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	ret = mt6379_bulk_read(ddata, TCPC_V10_REG_RX_BYTE_CNT,
			       buf, 14);
	if (ret < 0)
		return ret;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(u16 *)&buf[2]);

	MT6379_DBGINFO("Count is %u\n", cnt);
	MT6379_DBGINFO("FrameType is %u\n", *frame_type);
	MT6379_DBGINFO("MessageType is %u\n", PD_HEADER_TYPE(*msg_head));

	if (cnt <= 3)
		return ret;

	cnt -= 3; /* FRAME_TYPE + HEADER */
	if (cnt > sizeof(buf) - 4)
		cnt = sizeof(buf) - 4;
	if (cnt > 10) {
		ret = mt6379_bulk_read(ddata, TCPC_V10_REG_RX_DATA + 10,
				       buf + 14, cnt - 10);
		if (ret < 0)
			return ret;
	}
	memcpy(payload, buf + 4, cnt);

	return ret;
}

/* message header (2byte) + data object (7*4) */
#define MT6379_TRANSMIT_MAX_SIZE	(sizeof(u16) + sizeof(u32) * 7)

static int mt6379_transmit(struct tcpc_device *tcpc,
			   enum tcpm_transmit_type type, u16 header,
			   const u32 *data)
{
	int ret, data_cnt, packet_cnt;
	u8 temp[MT6379_TRANSMIT_MAX_SIZE + 1];
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u64 t = 0;

	MT6379_DBGINFO("++\n");
	t = local_clock();
	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(u32) * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(u16);

		temp[0] = packet_cnt;
		memcpy(temp + 1, &header, 2);
		if (data_cnt > 0)
			memcpy(temp + 3, data, data_cnt);

		ret = mt6379_bulk_write(ddata, TCPC_V10_REG_TX_BYTE_CNT,
					temp, packet_cnt + 1);
		if (ret < 0)
			return ret;
	}

	ret = mt6379_write8(ddata, TCPC_V10_REG_TRANSMIT,
			     TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count,
			     type));
	t = local_clock() - t;
	MT6379_INFO("-- delta = %lluns\n", t);

	return ret;
}

static int mt6379_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
static int mt6379_retransmit(struct tcpc_device *tcpc)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6379_write8(ddata, TCPC_V10_REG_TRANSMIT,
			     TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count,
			     TCPC_TX_SOP));
}
#endif /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

static int mt6379_set_cc_hidet(struct tcpc_device *tcpc, bool en)
{
	int ret;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	if (en)
		mt6379_enable_rpdet_auto(ddata, false);
	ret = (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_HILOCTRL10, MT6379_MSK_HIDET_CC_CMPEN);
	if (ret < 0)
		return ret;
	ret = (en ? mt6379_set_bits : mt6379_clr_bits)
		(ddata, MT6379_REG_MTMASK5, MT6379_MSK_HIDET_CC);
	if (ret < 0)
		return ret;
	if (!en)
		mt6379_enable_rpdet_auto(ddata, true);
	return ret;
}

static int mt6379_get_cc_hi(struct tcpc_device *tcpc)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return __mt6379_get_cc_hi(ddata);
}

static int mt6379_set_water_protection(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	ret = mt6379_enable_wd_protection(ddata, en);
#if CONFIG_WD_DURING_PLUGGED_IN
	if (!en && ret >= 0)
		ret = mt6379_enable_wd_polling(ddata, true);
#endif	/* CONFIG_WD_DURING_PLUGGED_IN */
	return ret;
}

/*
 * ==================================================================
 * TCPC vendor irq handlers
 * ==================================================================
 */

static int mt6379_wakeup_irq_handler(struct mt6379_tcpc_data *ddata)
{
	return tcpci_alert_wakeup(ddata->tcpc);
}

static void mt6379_wd12_strise_irq_dwork_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6379_tcpc_data *ddata = container_of(dwork,
						      struct mt6379_tcpc_data,
						      wd12_strise_irq_dwork);

	pm_system_wakeup();

	MT6379_DBGINFO("++\n");
	tcpci_lock_typec(ddata->tcpc);
	mt6379_wd_polling_evt_process(ddata);
	/* unmask */
	mt6379_set_bits(ddata, MT6379_REG_MTMASK7, MT6379_MSK_WD12_STRISE);
	tcpci_unlock_typec(ddata->tcpc);
}

static int mt6379_wd12_strise_irq_handler(struct mt6379_tcpc_data *ddata)
{
	/* Pull or discharge status from 0 to 1 in normal polling mode */
	/* mask */
	mt6379_clr_bits(ddata, MT6379_REG_MTMASK7, MT6379_MSK_WD12_STRISE);
	schedule_delayed_work(&ddata->wd12_strise_irq_dwork,
			      msecs_to_jiffies(900));
	return 0;
}

static int mt6379_wd12_done_irq_handler(struct mt6379_tcpc_data *ddata)
{
	/* Oneshot or protect mode done */
	return mt6379_wd_protection_evt_process(ddata);
}

static int mt6379_fod_irq_handler(struct mt6379_tcpc_data *ddata)
{
	return mt6379_fod_evt_process(ddata);
}

static int mt6379_ctd_irq_handler(struct mt6379_tcpc_data *ddata)
{
	int ret = 0;
#if CONFIG_CABLE_TYPE_DETECTION
	enum tcpc_cable_type cable_type = TCPC_CABLE_TYPE_NONE;

	ret = mt6379_get_cable_type(ddata, &cable_type);
	if (ret < 0)
		return ret;
	ret = tcpc_typec_handle_ctd(ddata->tcpc, cable_type);
#endif
	return ret;
}

static int mt6379_typec_otp_irq_handler(struct mt6379_tcpc_data *ddata)
{
	int ret = 0;
	u8 data = 0;

	ret = mt6379_read8(ddata, MT6379_REG_MTST1, &data);
	if (ret < 0)
		return ret;
	return tcpc_typec_handle_otp(ddata->tcpc,
				     !!(data & MT6379_MSK_TYPECOTP));
}

static int mt6379_hidet_cc_irq_handler(struct mt6379_tcpc_data *ddata)
{
	return mt6379_hidet_cc_evt_process(ddata);
}

static int mt6379_wd0_irq_handler(struct mt6379_tcpc_data *ddata)
{
	return mt6379_floating_ground_evt_process(ddata);
}

static int mt6379_vbus_to_cc_irq_handler(struct mt6379_tcpc_data *ddata)
{
	mod_delayed_work(system_wq, &ddata->vbus_to_cc_dwork,
			 msecs_to_jiffies(VBUS_TO_CC_DEBOUNCE));
	return 0;
}

static int mt6379_vbus_irq_handler(struct mt6379_tcpc_data *ddata)
{
	return mt6379_vbus_change_helper(ddata);
}

struct irq_mapping_tbl {
	u8 num;
	s8 grp;
	int (*hdlr)(struct mt6379_tcpc_data *ddata);
};

#define MT6379_IRQ_MAPPING(_num, _grp, _name) \
	{ .num = _num, .grp = _grp, .hdlr = mt6379_##_name##_irq_handler }

static struct irq_mapping_tbl mt6379_vend_irq_mapping_tbl[] = {
	MT6379_IRQ_MAPPING(0, -1, wakeup),
	MT6379_IRQ_MAPPING(49, -1, wd12_strise),
	MT6379_IRQ_MAPPING(50, -1, wd12_done),
	MT6379_IRQ_MAPPING(24, 0, fod),		/* fod_done */
	MT6379_IRQ_MAPPING(25, 0, fod),		/* fod_ov */
	MT6379_IRQ_MAPPING(31, 0, fod),		/* fod_dischgf */
	MT6379_IRQ_MAPPING(20, -1, ctd),
	MT6379_IRQ_MAPPING(2, -1, typec_otp),
	MT6379_IRQ_MAPPING(36, 1, hidet_cc),	/* hidet_cc1 */
	MT6379_IRQ_MAPPING(37, 1, hidet_cc),	/* hidet_cc2 */
	MT6379_IRQ_MAPPING(51, 2, wd0),		/* wd0_stfall */
	MT6379_IRQ_MAPPING(52, 2, wd0),		/* wd0_strise */
	MT6379_IRQ_MAPPING(22, 3, vbus_to_cc),	/* vbus_to_cc1 */
	MT6379_IRQ_MAPPING(23, 3, vbus_to_cc),	/* vbus_to_cc2 */

	MT6379_IRQ_MAPPING(1, 4, vbus),		/* vsafe0V */
	MT6379_IRQ_MAPPING(5, 4, vbus),		/* vbus_valid */
};

static int mt6379_alert_vendor_defined_handler(struct tcpc_device *tcpc)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 irqnum = 0, irqbit = 0;
	u8 buf[MT6379_VEND_INT_NUM * 2];
	u8 *mask = &buf[0];
	u8 *alert = &buf[MT6379_VEND_INT_NUM];
	s8 grp = 0;
	unsigned long handled_bitmap = 0;
	int ret = 0, i = 0;

	ret = mt6379_bulk_read(ddata, MT6379_REG_MTMASK1, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	for (i = 0; i < MT6379_VEND_INT_NUM; i++) {
		if (!(alert[i] & mask[i]))
			continue;
		MT6379_INFO("vend_alert[%d]=alert,mask(0x%02X,0x%02X)\n",
			    i + 1, alert[i], mask[i]);
		alert[i] &= mask[i];
	}

	mt6379_vend_alert_status_clear(ddata, alert);

	for (i = 0; i < ARRAY_SIZE(mt6379_vend_irq_mapping_tbl); i++) {
		irqnum = mt6379_vend_irq_mapping_tbl[i].num / 8;
		if (irqnum >= MT6379_VEND_INT_NUM)
			continue;
		irqbit = mt6379_vend_irq_mapping_tbl[i].num % 8;
		if (alert[irqnum] & BIT(irqbit)) {
			grp = mt6379_vend_irq_mapping_tbl[i].grp;
			if (grp >= 0) {
				ret = test_and_set_bit(grp, &handled_bitmap);
				if (ret)
					continue;
			}
			mt6379_vend_irq_mapping_tbl[i].hdlr(ddata);
	}
	}
	return 0;
}

static int mt6379_set_auto_dischg_discnt(struct tcpc_device *tcpc, bool en)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 data = 0;
	int ret = 0;

	MT6379_DBGINFO("en = %d\n", en);
	if (en) {
		ret = mt6379_read8(ddata, TCPC_V10_REG_POWER_CTRL, &data);
		if (ret < 0)
			return ret;
		data &= ~TCPC_V10_REG_VBUS_MONITOR;
		ret = mt6379_write8(ddata, TCPC_V10_REG_POWER_CTRL, data);
		if (ret < 0)
		return ret;
		data |= TCPC_V10_REG_AUTO_DISCHG_DISCNT;
		return mt6379_write8(ddata, TCPC_V10_REG_POWER_CTRL, data);
	}
	return mt6379_update_bits(ddata, TCPC_V10_REG_POWER_CTRL,
				  TCPC_V10_REG_VBUS_MONITOR |
				  TCPC_V10_REG_AUTO_DISCHG_DISCNT,
				  TCPC_V10_REG_VBUS_MONITOR);
}

static int mt6379_get_vbus_voltage(struct tcpc_device *tcpc, u32 *vbus)
{
	struct mt6379_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u16 data = 0;
	int ret = 0;

	ret = mt6379_read16(ddata, TCPC_V10_REG_VBUS_VOLTAGE_L, &data);
	if (ret < 0)
		return ret;
	data = le16_to_cpu(data);
	*vbus = (data & 0x3FF) * 25;
	MT6379_DBGINFO("0x%04x, %dmV\n", data, *vbus);
	return 0;
}

static struct tcpc_ops mt6379_tcpc_ops = {
	.init = mt6379_tcpc_init,
	.init_alert_mask = mt6379_init_mask,
	.alert_status_clear = mt6379_alert_status_clear,
	.fault_status_clear = mt6379_fault_status_clear,
	.get_alert_mask = mt6379_get_alert_mask,
	.set_alert_mask = mt6379_set_alert_mask,
	.get_alert_status_and_mask = mt6379_get_alert_status_and_mask,
	.get_power_status = mt6379_get_power_status,
	.get_fault_status = mt6379_get_fault_status,
	.get_cc = mt6379_get_cc,
	.set_cc = mt6379_set_cc,
	.set_polarity = mt6379_set_polarity,
	.set_vconn = mt6379_set_vconn,
	.deinit = mt6379_tcpc_deinit,
	.alert_vendor_defined_handler = mt6379_alert_vendor_defined_handler,
	.set_auto_dischg_discnt = mt6379_set_auto_dischg_discnt,
	.get_vbus_voltage = mt6379_get_vbus_voltage,

	.set_low_power_mode = mt6379_set_low_power_mode,

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	.set_msg_header = mt6379_set_msg_header,
	.set_rx_enable = mt6379_set_rx_enable,
	.protocol_reset = mt6379_protocol_reset,
	.get_message = mt6379_get_message,
	.transmit = mt6379_transmit,
	.set_bist_test_mode = mt6379_set_bist_test_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = mt6379_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	.set_cc_hidet = mt6379_set_cc_hidet,
	.get_cc_hi = mt6379_get_cc_hi,

	.set_water_protection = mt6379_set_water_protection,

	.set_vbus_short_cc = mt6379_set_vbus_short_cc,

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
	.set_force_discharge = mt6379_set_force_discharge,
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */
};

static irqreturn_t mt6379_pd_evt_handler(int irq, void *data)
{
	struct mt6379_tcpc_data *ddata = data;
	bool handled = false;
	int ret = 0;

	MT6379_DBGINFO("++\n");
	pm_stay_awake(ddata->dev);

	tcpci_lock_typec(ddata->tcpc);
	do {
		ret = tcpci_alert(ddata->tcpc, false);
		if (ret == -ENODATA)
			break;
		else if (ret >= 0)
			handled = true;
	} while (1);
	tcpci_unlock_typec(ddata->tcpc);

	if (handled) {
		ret = mt6379_write8(ddata, MT6379_REG_SPMI_TXDRV2, MT6379_MASK_RCS_INT_DONE);
		if (ret)
			MT6379_DBGINFO("Failed to do IRQ retrigger\n");
	}

	pm_relax(ddata->dev);
	MT6379_DBGINFO("--\n");

	return IRQ_HANDLED;
}


static int mt6379_tcpc_init_irq(struct mt6379_tcpc_data *ddata)
{
	int ret;

	dev_info(ddata->dev, "IRQ: %s\n", __func__);
	/* Mask all alerts & clear them */
	mt6379_write16(ddata, TCPC_V10_REG_ALERT_MASK, 0);
	mt6379_write16(ddata, TCPC_V10_REG_ALERT, 0xffff);

	ret = platform_get_irq_byname(to_platform_device(ddata->dev), "pd_evt");
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get irq pd_evt\n");
		return ret;
	}
	ddata->irq = ret;
	device_init_wakeup(ddata->dev, true);
	ret = devm_request_threaded_irq(ddata->dev, ret, NULL,
					mt6379_pd_evt_handler, IRQF_ONESHOT,
					dev_name(ddata->dev), ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to request irq %d\n", ddata->irq);
		device_init_wakeup(ddata->dev, false);
		return ret;
	}

	enable_irq_wake(ddata->irq);
	return 0;
}

static int mt6379_register_tcpcdev(struct mt6379_tcpc_data *ddata)
{
	struct tcpc_desc *desc = ddata->desc;
	struct tcpc_device *tcpc = NULL;

	tcpc = tcpc_device_register(ddata->dev, desc, &mt6379_tcpc_ops, ddata);
	if (IS_ERR_OR_NULL(tcpc))
		return -EINVAL;
	ddata->tcpc = tcpc;

#if CONFIG_USB_PD_DISABLE_PE
	tcpc->disable_pe = device_property_read_bool(ddata->dev,
						     "tcpc,disable_pe");
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	/* Init tcpc_flags */
#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#if CONFIG_USB_PD_REV30
	tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;
#endif	/* CONFIG_USB_PD_REV30 */

	if (desc->en_wd)
		tcpc->tcpc_flags |= TCPC_FLAGS_WATER_DETECTION;
	if (desc->en_wd_dual_port)
		tcpc->tcpc_flags |= TCPC_FLAGS_WD_DUAL_PORT;
	if (desc->en_ctd)
		tcpc->tcpc_flags |= TCPC_FLAGS_CABLE_TYPE_DETECTION;
	if (desc->en_fod)
		tcpc->tcpc_flags |= TCPC_FLAGS_FOREIGN_OBJECT_DETECTION;
	if (desc->en_typec_otp)
		tcpc->tcpc_flags |= TCPC_FLAGS_TYPEC_OTP;
	if (desc->en_floatgnd)
		tcpc->tcpc_flags |= TCPC_FLAGS_FLOATING_GROUND;
	if (desc->en_vbus_short_cc)
		tcpc->tcpc_flags |= TCPC_FLAGS_VBUS_SHORT_CC;

	if (tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(ddata->dev, "%s PD REV30\n", __func__);
	else
		dev_info(ddata->dev, "%s PD REV20\n", __func__);

	return 0;
}

static int mt6379_parse_dt(struct mt6379_tcpc_data *ddata)
{
	struct tcpc_desc *desc = ddata->desc;
	struct device *dev = ddata->dev;
	u32 val;
	int i;
	const struct {
		const char *name;
		const char *legacy_name;
		bool *val_ptr;
	} tcpc_props_bool[] = {
		{ "tcpc,en-wd", "tcpc,en_wd", &desc->en_wd },
		{ "tcpc,en-wd-dual-port", "tcpc,en_wd_dual_port", &desc->en_wd_dual_port },
		{ "tcpc,en-ctd", "tcpc,en_ctd", &desc->en_ctd },
		{ "tcpc,en-fod", "tcpc,en_fod", &desc->en_fod },
		{ "tcpc,en-typec-otp", "tcpc,en_typec_otp", &desc->en_typec_otp },
		{ "tcpc,en-floatgnd", "tcpc,en_floatgnd", &desc->en_floatgnd },
		{ "tcpc,en-vbus-short-cc", "tcpc,en_vbus_short_cc", &desc->en_vbus_short_cc },
	};
	const struct {
		const char *name;
		const char *legacy_name;
		u32 *val_ptr;
	} tcpc_props_u32[] = {
		{ "wd,sbu-calib-init", "wd,sbu_calib_init", &desc->wd_sbu_calib_init },
		{ "wd,sbu-pl-bound", "wd,sbu_pl_bound", &desc->wd_sbu_pl_bound },
		{ "wd,sbu-pl-lbound-c2c", "wd,sbu_pl_lbound_c2c", &desc->wd_sbu_pl_lbound_c2c },
		{ "wd,sbu-pl-ubound-c2c", "wd,sbu_pl_ubound_c2c", &desc->wd_sbu_pl_ubound_c2c },
		{ "wd,sbu-ph-auddev", "wd,sbu_ph_auddev", &desc->wd_sbu_ph_auddev },
		{ "wd,sbu-ph-lbound", "wd,sbu_ph_lbound", &desc->wd_sbu_ph_lbound },
		{ "wd,sbu-ph-lbound1-c2c", "wd,sbu_ph_lbound1_c2c", &desc->wd_sbu_ph_lbound1_c2c },
		{ "wd,sbu-ph-ubound1-c2c", "wd,sbu_ph_ubound1_c2c", &desc->wd_sbu_ph_ubound1_c2c },
		{ "wd,sbu-ph-ubound2-c2c", "wd,sbu_ph_ubound2_c2c", &desc->wd_sbu_ph_ubound2_c2c },
		{ "wd,sbu-aud-ubound", "wd,sbu_aud_ubound", &desc->wd_sbu_aud_ubound },
	};

	memcpy(desc, &def_tcpc_desc, sizeof(*desc));

	if (device_property_read_string(dev, "tcpc,name", &desc->name))
		desc->name = "type_c_port0";

	if ((!device_property_read_u32(dev, "tcpc,role-def", &val) ||
	     !device_property_read_u32(dev, "tcpc,role_def", &val)) &&
	    val < TYPEC_ROLE_NR)
		desc->role_def = val;

	if (!device_property_read_u32(dev, "tcpc,rp-level", &val) ||
	    !device_property_read_u32(dev, "tcpc,rp_level", &val)) {
		switch (val) {
		case TYPEC_RP_DFT:
		case TYPEC_RP_1_5:
		case TYPEC_RP_3_0:
			desc->rp_lvl = val;
			break;
		default:
			desc->rp_lvl = TYPEC_RP_DFT;
			break;
		}
	}

	if ((!device_property_read_u32(dev, "tcpc,vconn-supply", &val) ||
	     !device_property_read_u32(dev, "tcpc,vconn_supply", &val)) &&
	    val < TCPC_VCONN_SUPPLY_NR)
		desc->vconn_supply = val;

	for (i = 0; i < ARRAY_SIZE(tcpc_props_bool); i++) {
		*tcpc_props_bool[i].val_ptr =
			device_property_read_bool(dev, tcpc_props_bool[i].name) ||
			device_property_read_bool(dev, tcpc_props_bool[i].legacy_name);
			dev_info(dev, "props[%s] (legacy: props[%s]) = %d\n",
				 tcpc_props_bool[i].name, tcpc_props_bool[i].legacy_name,
				 *tcpc_props_bool[i].val_ptr);
	}

	for (i = 0; i < ARRAY_SIZE(tcpc_props_u32); i++) {
		if (device_property_read_u32(dev, tcpc_props_u32[i].name,
					     tcpc_props_u32[i].val_ptr) &&
		    device_property_read_u32(dev, tcpc_props_u32[i].legacy_name,
					     tcpc_props_u32[i].val_ptr))
			dev_notice(dev, "failed to parse props[%s] (legacy: props[%s])\n",
				   tcpc_props_u32[i].name, tcpc_props_u32[i].legacy_name);
		else
			dev_info(dev, "props[%s] (legacy: props[%s]) = %d\n",
				 tcpc_props_u32[i].name, tcpc_props_u32[i].legacy_name,
				 *tcpc_props_u32[i].val_ptr);
	}

	if (desc->en_floatgnd) {
		if (device_property_read_u32(dev, "wd,wd0-tsleep", &val) &&
		    device_property_read_u32(dev, "wd,wd0_tsleep", &val)) {
			dev_notice(dev, "wd0_tsleep use default\n");
			ddata->wd0_tsleep = 1;
		} else {
			dev_notice(dev, "wd0_tsleep = %d\n", val);
			ddata->wd0_tsleep = val;
		}

		if (device_property_read_u32(dev, "wd,wd0-tdet", &val) &&
		    device_property_read_u32(dev, "wd,wd0_tdet", &val)) {
			dev_notice(dev, "wd0_tdet use default\n");
			ddata->wd0_tdet = 3;
		} else {
			dev_notice(dev, "wd0_tdet = %d\n", val);
			ddata->wd0_tdet = val;
		}
	}

	return 0;
}

static int mt6379_check_revision(struct mt6379_tcpc_data *ddata)
{
	int ret;
	u16 id;

	ret = mt6379_read16(ddata, TCPC_V10_REG_VID, &id);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to read vid(%d)\n", ret);
		return ret;
	}
	if (id != MT6379_VID) {
		dev_err(ddata->dev, "incorrect vid(0x%04X)\n", id);
		return -ENODEV;
	}

	ret = mt6379_read16(ddata, TCPC_V10_REG_PID, &id);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to read pid(%d)\n", ret);
		return ret;
	}
	if (id != MT6379_PID) {
		dev_err(ddata->dev, "incorrect pid(0x%04X)\n", id);
		return -ENODEV;
	}

	ret = mt6379_read16(ddata, TCPC_V10_REG_DID, &id);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to read did(%d)\n", ret);
		return ret;
	}
	dev_info(ddata->dev, "did = 0x%04X\n", id);
	return 0;
}

static int mt6379_tcpc_probe(struct platform_device *pdev)
{
	int ret;
	struct mt6379_tcpc_data *ddata;

	dev_info(&pdev->dev, "probe: %s\n", __func__);

	ddata = devm_kzalloc(&pdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;
	ddata->dev = &pdev->dev;
	platform_set_drvdata(pdev, ddata);

	ddata->rmap = dev_get_regmap(ddata->dev->parent, NULL);
	if (!ddata->rmap) {
		dev_err(ddata->dev, "failed to get regmap\n");
		return -ENODEV;
	}

	ddata->desc = devm_kzalloc(ddata->dev, sizeof(*ddata->desc),
				   GFP_KERNEL);
	if (!ddata->desc)
		return -ENOMEM;
	ret = mt6379_parse_dt(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to parse dt(%d)\n", ret);
		return ret;
	}

	atomic_set(&ddata->wd_protect_rty, CONFIG_WD_PROTECT_RETRY_COUNT);
	INIT_DELAYED_WORK(&ddata->wd_polling_dwork,
			  mt6379_wd_polling_dwork_handler);
	INIT_DELAYED_WORK(&ddata->wd12_strise_irq_dwork,
			  mt6379_wd12_strise_irq_dwork_handler);
	INIT_DELAYED_WORK(&ddata->fod_polling_dwork, mt6379_fod_polling_dwork_handler);
	INIT_DELAYED_WORK(&ddata->vbus_to_cc_dwork,
			  mt6379_vbus_to_cc_dwork_handler);

	ddata->adc_iio = devm_iio_channel_get_all(ddata->dev);
	if (IS_ERR(ddata->adc_iio)) {
		ret = PTR_ERR(ddata->adc_iio);
		dev_err(ddata->dev, "failed to get adc iio(%d)\n", ret);
		return ret;
	}

	ret = mt6379_register_tcpcdev(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to register tcpcdev(%d)\n", ret);
		return ret;
	}

	ret = mt6379_check_revision(ddata);
	if (ret < 0) {
		dev_notice(ddata->dev, "failed to check revision(%d)\n", ret);
		goto err;
	}

	ret = mt6379_sw_reset(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to reset sw(%d)\n", ret);
		goto err;
	}

	if (ddata->desc->en_fod) {
#if MT6379_FOD_SRC_EN
		/* Enable RD Connect auto detection */
		ret = mt6379_set_bits(ddata, MT6379_REG_SHIELDCTRL1,
				      MT6379_MSK_RDDET_AUTO);
		if (ret < 0) {
			dev_notice(ddata->dev, "failed to enable rddet auto\n");
			goto err;
		}
		/* Enable FOD trigger control by CC attached as SRC */
		ret = mt6379_set_bits(ddata, MT6379_REG_FODCTRL,
				      MT6379_MSK_FOD_SRC_EN);
		if (ret < 0) {
			dev_notice(ddata->dev, "failed to enable fod src en\n");
			goto err;
		}
#endif /* MT6379_FOD_SRC_EN */
	} else {
		/* disable fod */
		ret = mt6379_clr_bits(ddata, MT6379_REG_FODCTRL,
				      MT6379_MSK_FOD_SNK_EN);
		if (ret < 0) {
			dev_notice(ddata->dev,
				   "failed to disable fod snk en\n");
			goto err;
		}
	}

	/* disable otp */
	if (!ddata->desc->en_typec_otp) {
		ret = mt6379_clr_bits(ddata, MT6379_REG_TYPECOTPCTRL,
				      MT6379_MSK_TYPECOTP_HWEN);
		if (ret < 0) {
			dev_notice(ddata->dev, "failed to disable otp\n");
			goto err;
		}
	}

	ret = mt6379_tcpc_init_irq(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to init irq\n");
		goto err;
	}

	dev_info(ddata->dev, "%s successfully!\n", __func__);
	return 0;
err:
	tcpc_device_unregister(ddata->dev, ddata->tcpc);
	return ret;
}

static int mt6379_tcpc_remove(struct platform_device *pdev)
{
	struct mt6379_tcpc_data *ddata = platform_get_drvdata(pdev);

	disable_irq(ddata->irq);
	device_init_wakeup(ddata->dev, false);
	cancel_delayed_work_sync(&ddata->vbus_to_cc_dwork);
	cancel_delayed_work_sync(&ddata->fod_polling_dwork);
	if (ddata->tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) {
		cancel_delayed_work_sync(&ddata->wd_polling_dwork);
		cancel_delayed_work_sync(&ddata->wd12_strise_irq_dwork);
	}
	tcpc_device_unregister(ddata->dev, ddata->tcpc);

	return 0;
}

static void mt6379_tcpc_shutdown(struct platform_device *pdev)
{
	struct mt6379_tcpc_data *ddata = platform_get_drvdata(pdev);

	disable_irq(ddata->irq);
	cancel_delayed_work_sync(&ddata->vbus_to_cc_dwork);
	cancel_delayed_work_sync(&ddata->fod_polling_dwork);
	if (ddata->tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) {
#if !CONFIG_WD_DURING_PLUGGED_IN
		cancel_delayed_work_sync(&ddata->wd_polling_dwork);
#endif	/* !CONFIG_WD_DURING_PLUGGED_IN */
		cancel_delayed_work_sync(&ddata->wd12_strise_irq_dwork);
	}
	tcpm_shutdown(ddata->tcpc);
}

static int __maybe_unused mt6379_tcpc_suspend(struct device *dev)
{
	struct mt6379_tcpc_data *ddata = dev_get_drvdata(dev);

	return tcpm_suspend(ddata->tcpc);
}

static int __maybe_unused mt6379_tcpc_check_suspend_pending(struct device *dev)
{
	struct mt6379_tcpc_data *ddata = dev_get_drvdata(dev);

	return tcpm_check_suspend_pending(ddata->tcpc);
}

static int __maybe_unused mt6379_tcpc_resume(struct device *dev)
{
	struct mt6379_tcpc_data *ddata = dev_get_drvdata(dev);

	tcpm_resume(ddata->tcpc);

	return 0;
}

static const struct dev_pm_ops mt6379_tcpc_pm_ops = {
#if IS_ENABLED(CONFIG_PM_SLEEP)
	.prepare = mt6379_tcpc_check_suspend_pending,
#endif	/* CONFIG_PM_SLEEP */
	SET_SYSTEM_SLEEP_PM_OPS(mt6379_tcpc_suspend, mt6379_tcpc_resume)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mt6379_tcpc_check_suspend_pending, NULL)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mt6379_tcpc_check_suspend_pending, NULL)
};

static const struct of_device_id mt6379_tcpc_of_match[] = {
	{.compatible = "mediatek,mt6379-tcpc",},
	{}
};
MODULE_DEVICE_TABLE(of, mt6379_tcpc_of_match);

static struct platform_driver mt6379_tcpc_driver = {
	.probe = mt6379_tcpc_probe,
	.remove = mt6379_tcpc_remove,
	.shutdown = mt6379_tcpc_shutdown,
	.driver = {
		.name = "mt6379-tcpc",
		.owner = THIS_MODULE,
		.of_match_table = mt6379_tcpc_of_match,
		.pm = &mt6379_tcpc_pm_ops,
	},
};
module_platform_driver(mt6379_tcpc_driver);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("MT6379 USB Type-C Port Controller Interface Driver");
MODULE_VERSION("1.0.3");
#endif	/* CONFIG_TCPC_CLASS */
MODULE_LICENSE("GPL");
