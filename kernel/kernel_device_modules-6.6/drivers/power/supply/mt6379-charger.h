/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * mt6379-charger.h -- Mediatek MT6379 Charger Driver Header
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: SHIH CHIA CHANG <jeff_chang@richtek.com>
 */

#ifndef __LINUX_MT6379_CHARGER_H
#define __LINUX_MT6379_CHARGER_H

#include <linux/atomic.h>
#include <linux/iio/consumer.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>

#include "charger_class.h"
#include "mtk_charger.h"
#include "mtk_chg_type_det.h"

extern unsigned int dbg_log_level;
#define mt_dbg(dev, fmt, ...)						\
	do {								\
		switch (dbg_log_level) {				\
		case 0:							\
			break;						\
		case 1:							\
			dev_info_ratelimited(dev, fmt, ##__VA_ARGS__);	\
			break;						\
		case 2:							\
		default:						\
			dev_info(dev, fmt, ##__VA_ARGS__);		\
			break;						\
		}							\
	} while(0)

#define M_TO_U(val)			((val) * 1000)
#define U_TO_M(val)			((val) / 1000)

#define ADC_CONV_TIME_US		2200
#define ADC_VBAT_SCALE			1000
#define ADC_TO_VBAT_RAW(vbat)		((vbat) / ADC_VBAT_SCALE)
#define ADC_FROM_VBAT_RAW(raw)		((raw) * ADC_VBAT_SCALE)

#define MT6379_REG_DEV_INFO		(0x00)
#define MT6379_REG_CORE_CTRL0		(0x01)
#define MT6379_REG_RST1			(0x02)
#define MT6379_REG_RST_PAS_CODE1	(0x04)
#define MT6379_REG_CORE_CTRL2		(0x06)
#define MT6379_REG_TM_PAS_CODE1		(0x07)
#define MT6379_REG_VDDA_SUPPLY		(0x11)
#define MT6379_REG_BB_VOUT_SEL		(0x43)
#define MT6379_REG_CHG_STAT0		(0x70)
#define MT6379_REG_CHG_STAT1		(0x71)
#define MT6379_REG_CHG_STAT2		(0x72)
#define MT6379_REG_USBID_STAT		(0x89)

#define MT6379_REG_CHG_BATPRO_SLE	(0x100)
#define MT6379_REG_CHG_BATPRO		(0x101)
#define MT6379_REG_CHG_TOP1		(0x107)
#define MT6379_REG_CHG_TOP2		(0x108)
#define MT6379_REG_CHG_IBUS_AICR	(0x109)
#define MT6379_REG_CHG_WLIN_AICR	(0x10A)
#define MT6379_REG_CHG_VBUS_MIVR	(0x10B)
#define MT6379_REG_CHG_WLIN_MIVR	(0x10C)
#define MT6379_REG_CHG_VCHG		(0x10E)
#define MT6379_REG_CHG_ICHG		(0x10F)
#define MT6379_REG_CHG_TMR		(0x110)
#define MT6379_REG_CHG_EOC1		(0x111)
#define MT6379_REG_CHG_EOC2		(0x112)
#define MT6379_REG_CHG_VSYS		(0X113)
#define MT6379_REG_CHG_WDT		(0x114)
#define MT6379_REG_CHG_PUMPX		(0x115)
#define MT6379_REG_CHG_AICC		(0x116)
#define MT6379_REG_CHG_IPREC		(0x117)
#define MT6379_REG_CHG_AICC_RPT		(0x118)
#define MT6379_REG_CHG_OTG_LBP		(0x11B)
#define MT6379_REG_CHG_OTG_CV_MSB	(0x11C)
#define MT6379_REG_CHG_OTG_C		(0x11E)
#define MT6379_REG_CHG_COMP1		(0x11F)
#define MT6379_REG_CHG_COMP2		(0x120)
#define MT6379_REG_CHG_STAT		(0x121)
#define MT6379_REG_CHG_HD_DIG2		(0x126)
#define MT6379_REG_CHG_HD_TOP1		(0x129)
#define MT6379_REG_CHG_HD_BUBO5		(0x12E)
#define MT6379_REG_CHG_HD_PP7		(0x135)
#define MT6379_REG_CHG_HD_TRIM6		(0x140)
#define MT6379_REG_CHG_BYPASS_IQ	(0x149)
#define MT6379_REG_ADC_CONFG1		(0x14E)
#define MT6379_REG_BATEND_CODE		(0x158)
#define MT6379_REG_ADC_ZCV_RPT		(0x17F)
#define MT6379_REG_USBID_CTRL1		(0x190)
#define MT6379_REG_USBID_CTRL2		(0x191)

#define MT6379_REG_PD_SYS_CTRL3		(0x4B0)
#define MT6379_REG_TYPECOTP_CTRL	(0x4CD)

#define MT6379_REG_BC12_FUNC		(0x600)
#define MT6379_REG_BC12_STAT		(0x601)
#define MT6379_REG_DPDM_CTRL1		(0x603)
#define MT6379_REG_DPDM_CTRL2		(0x604)
#define MT6379_REG_DPDM_CTRL4		(0x606)

#define MT6379_REG_FGADC_SYS_INFO_CON0	(0x7F9)

#define MT6379_CHIP_REV_MSK		GENMASK(3, 0)
#define MT6379_CHG_RAMPUP_COMP_MSK	GENMASK(7, 6)
#define MT6379_CHG_IEOC_FLOW_RB_MSK	BIT(4)

enum mt6379_charger_reg_field {
	/* MT6379_REG_CORE_CTRL0 */
	F_MREN,
	/* MT6379_REG_CHG_BATPRO_SLE */
	F_BATPROTECT_SOURCE,
	/* MT6379_REG_CORE_CTRL2 */
	F_SHIP_RST_DIS, F_PD_MDEN,
	/* MT6379_REG_CHG_STAT0 */
	F_ST_PWR_RDY,
	/* MT6379_REG_CHG_STAT1 */
	F_ST_MIVR,
	/* MT6379_REG_CHG_STAT2 */
	F_ST_AICC_DONE,
	/* MT6379_REG_USBID_STAT */
	F_ST_USBID,
	/* MT6379_REG_CHG_BATPRO */
	F_BATINT, F_BATPROTECT_EN,
	/* MT6379_REG_CHG_TOP1 */
	F_PP_PG_FLAG, F_BATFET_DIS, F_BATFET_DISDLY, F_QON_RST_EN,
	/* MT6379_REG_CHG_TOP2 */
	F_UUG_FULLON, F_CHG_BYPASS,
	/* MT6379_REG_CHG_IBUS_AICR */
	F_IBUS_AICR,
	/* MT6379_REG_CHG_WLIN_AICR */
	F_ILIM_EN, F_WLIN_AICR,
	/* MT6379_REG_CHG_VBUS_MIVR */
	F_VBUS_MIVR,
	/* MT6379_REG_CHG_WLIN_MIVR */
	F_WLIN_MIVR,
	/* MT6379_REG_CHG_VCHG */
	F_VREC, F_CV,
	/* MT6379_REG_CHG_ICHG */
	F_CC,
	/* MT6379_REG_CHG_TMR */
	F_CHG_TMR_EN, F_CHG_TMR_2XT, F_CHG_TMR,
	/* MT6379_REG_CHG_EOC1 */
	F_IEOC,
	/* MT6379_REG_CHG_EOC2 */
	F_WLIN_FST, F_CHGIN_OV, F_EOC_TIME, F_TE, F_EOC_RST,
	/* MT6379_REG_CHG_VSYS */
	F_DISCHARGE_EN,  F_VSYSOV, F_VSYSMIN,
	/* MT6379_REG_CHG_WDT */
	F_HZ, F_BUCK_EN, F_CHG_EN, F_WDT_EN, F_WDT_RST, F_WDT_TIME,
	/* MT6379_REG_CHG_PUMPX */
	F_PE_EN, F_PE_SEL, F_PE10_INC, F_PE20_CODE,
	/* MT6379_REG_CHG_AICC */
	F_AICC_EN, F_AICC_ONESHOT,
	/* MT6379_REG_CHG_IPREC */
	F_IPREC,
	/* MT6379_REG_CHG_AICC_RPT */
	F_AICC_RPT,
	/* MT6379_REG_CHG_OTG_LBP */
	F_OTG_LBP,
	/* MT6379_REG_CHG_OTG_C */
	F_SEAMLESS_OTG, F_OTG_THERMAL_EN, F_OTG_OCP, F_OTG_WLS, F_OTG_CC,
	/* MT6379_REG_CHG_COMP1 */
	F_IRCMP_EN, F_IRCMP_R,
	/* MT6379_REG_CHG_COMP2 */
	F_IRCMP_V,
	/* MT6379_REG_CHG_STAT */
	F_IC_STAT,
	/* MT6379_REG_CHG_HD_TOP1 */
	F_FORCE_VBUS_SINK,
	/* MT6379_REG_ADC_CONFG1 */
	F_VBAT_MON_EN, F_VBAT_MON2_EN,
	/* MT6379_REG_USBID_CTRL1 */
	F_IS_TDET, F_ID_RUPSEL, F_USBID_EN,
	/* MT6379_REG_USBID_CTRL2 */
	F_USBID_FLOATING,
	/* MT6379_REG_BC12_FUNC */
	F_BC12_EN,
	/* MT6379_REG_BC12_STAT */
	F_PORT_STAT,
	/* MT6379_REG_DPDM_CTRL1 */
	F_MANUAL_MODE, F_DPDM_SW_VCP_EN, F_DP_DET_EN, F_DM_DET_EN,
	/* MT6379_REG_DPDM_CTRL2 */
	F_DP_LDO_EN, F_DP_LDO_VSEL,
	/* MT6379_REG_DPDM_CTRL4 */
	F_DP_PULL_REN, F_DP_PULL_RSEL,
	F_MAX
};

enum mt6379_adc_chan {
	ADC_CHAN_VBATMON = 0,
	ADC_CHAN_CHGVIN,
	ADC_CHAN_USBDP,
	ADC_CHAN_VSYS,
	ADC_CHAN_VBAT,
	ADC_CHAN_IBUS,
	ADC_CHAN_IBAT,
	ADC_CHAN_USBDM,
	ADC_CHAN_TEMPJC,
	ADC_CHAN_SBU2,
	ADC_CHAN_VBATMON2,
	ADC_CHAN_ZCV,
	ADC_CHAN_MAX
};

static const char *const mt6379_adc_chan_names[] = {
	[ADC_CHAN_VBATMON] = "vbatmon",
	[ADC_CHAN_CHGVIN] "chg-vin",
	[ADC_CHAN_USBDP] = "usb-dp",
	[ADC_CHAN_VSYS] = "vsys",
	[ADC_CHAN_VBAT] = "vbat",
	[ADC_CHAN_IBUS] = "ibus",
	[ADC_CHAN_IBAT] = "ibat",
	[ADC_CHAN_USBDM] = "usb-dm",
	[ADC_CHAN_TEMPJC] = "temp-jc",
	[ADC_CHAN_SBU2] = "sbu2",
	[ADC_CHAN_VBATMON2] = "vbatmon2",
	[ADC_CHAN_ZCV] = "zcv",
};

/* irq index */
enum {
	MT6379_IRQ_PWR_RDY,
	MT6379_IRQ_DETACH,
	MT6379_IRQ_RECHG,
	MT6379_IRQ_CHG_DONE,
	MT6379_IRQ_BK_CHG,
	MT6379_IRQ_IEOC,
	MT6379_IRQ_BUS_CHG_RDY,
	MT6379_IRQ_WLIN_CHG_RDY,
	MT6379_IRQ_VBUS_OV,
	MT6379_IRQ_CHG_BATOV,
	MT6379_IRQ_CHG_SYSOV,
	MT6379_IRQ_CHG_TOUT,
	MT6379_IRQ_CHG_BUSUV,
	MT6379_IRQ_CHG_THREG,
	MT6379_IRQ_CHG_AICR,
	MT6379_IRQ_CHG_MIVR,
	MT6379_IRQ_AICC_DONE,
	MT6379_IRQ_PE_DONE,
	MT6379_PP_PGB_EVT,
	MT6379_IRQ_WDT,
	MT6379_IRQ_OTG_FAULT,
	MT6379_IRQ_OTG_LBP,
	MT6379_IRQ_OTG_CC,
	MT6379_IRQ_BATPRO_DONE,
	MT6379_IRQ_OTG_CLEAR,
	MT6379_IRQ_DCD_DONE,
	MT6379_IRQ_BC12_HVDCP,
	MT6379_IRQ_BC12_DN,
	MT6379_ADC_VBAT_MON,
	MT6379_USBID_EVT,
	MT6379_IRQ_OTP0,
	MT6379_IRQ_OTP1,
	MT6379_IRQ_OTP2,
	MT6379_IRQ_MAX
};

enum mt6379_attach_trigger {
	ATTACH_TRIG_IGNORE,
	ATTACH_TRIG_PWR_RDY,
	ATTACH_TRIG_TYPEC,
};

/* For F_WDT_TIME Field */
enum {
	MT6379_WDT_TIME_8S,
	MT6379_WDT_TIME_40S,
	MT6379_WDT_TIME_80S,
	MT6379_WDT_TIME_160S,
};

/* For F_CHGIN_OV Field */
enum {
	MT6379_CHGIN_OV_4_7V,
	MT6379_CHGIN_OV_5_8V,
	MT6379_CHGIN_OV_6_5V,
	MT6379_CHGIN_OV_11V,
	MT6379_CHGIN_OV_14_5V,
	MT6379_CHGIN_OV_18V,
	MT6379_CHGIN_OV_22_5V,
};

enum mt6379_batpro_src {
	MT6379_BATPRO_SRC_VBAT_MON = 0,
	MT6379_BATPRO_SRC_VBAT_MON2
};

struct mt6379_charger_platform_data {
	u32 aicr;
	u32 mivr;
	u32 ichg;
	u32 ieoc;
	u32 cv;
	u32 wdt_time;
	u32 ircmp_v;
	u32 ircmp_r;
	u32 vrec;
	u32 chgin_ov;
	u32 chg_tmr;
	u32 nr_port;
	u32 bc12_sel;
	u32 boot_mode;
	u32 boot_type;
	u32 pmic_uvlo;
	enum mt6379_attach_trigger attach_trig;
	const char *chgdev_name;
	const char *ls_dev_name; /* low switch */
	bool chg_tmr_en;
	bool wdt_en;
	bool te_en;
	bool usb_killer_detect;
};

struct mt6379_charger_data {
	struct device *dev;
	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX];
	struct regulator_dev *rdev;
	struct regulator *otg_regu;
	struct workqueue_struct *wq;
	struct work_struct bc12_work;
	struct delayed_work switching_work;
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	struct iio_channel *iio_adcs;
	struct charger_device *chgdev;
	struct mutex attach_lock;
	struct mutex cv_lock;
	struct mutex tm_lock;
	struct mutex pe_lock;
	struct mutex ramp_lock;
	unsigned int irq_nums[MT6379_IRQ_MAX];
	bool batprotect_en;
	int tm_use_cnt;
	int vbat0_flag;
	atomic_t tchg;
	atomic_t eoc_cnt;
	atomic_t no_6pin_used;
	bool non_switching;
	u32 zcv;
	u32 cv;
	u8 bypass_mode_entered;

	struct ufcs_port *ufcs;
	struct notifier_block ufcs_noti;
	bool wait_for_ufcs_attach;

	int active_idx;
	enum power_supply_type *psy_type;
	enum power_supply_usb_type *psy_usb_type;
	atomic_t *attach;
	bool *bc12_dn;
};

enum mt6379_chip_rev {
	MT6379_CHIP_REV_E0 = 0,
	MT6379_CHIP_REV_E1,
	MT6379_CHIP_REV_E2,
	MT6379_CHIP_REV_E3,
	MT6379_CHIP_REV_E4,
	MT6379_CHIP_REV_MAX
};

extern int mt6379_charger_init_chgdev(struct mt6379_charger_data *cdata);
extern int mt6379_charger_field_set(struct mt6379_charger_data *cdata,
				    enum mt6379_charger_reg_field fd, unsigned int val);
extern int mt6379_charger_field_get(struct mt6379_charger_data *cdata,
				    enum mt6379_charger_reg_field fd, u32 *val);
extern int mt6379_charger_fsw_control(struct mt6379_charger_data *cdata);
extern int mt6379_charger_set_non_switching_setting(struct mt6379_charger_data *cdata);


static inline int mt6379_enable_tm(struct mt6379_charger_data *cdata, bool en)
{
	const u8 tm_pascode[] = { 0x69, 0x96, 0x63, 0x79 };
	int ret = 0;

	mutex_lock(&cdata->tm_lock);
	if (en) {
		if (cdata->tm_use_cnt == 0) {
			ret = regmap_bulk_write(cdata->rmap, MT6379_REG_TM_PAS_CODE1,
						tm_pascode, ARRAY_SIZE(tm_pascode));
			if (ret < 0)
				goto out;
		}
		cdata->tm_use_cnt++;
	} else {
		if (cdata->tm_use_cnt == 1) {
			ret = regmap_write(cdata->rmap, MT6379_REG_TM_PAS_CODE1, 0);
			if (ret < 0)
				goto out;
		}
		if (cdata->tm_use_cnt > 0)
			cdata->tm_use_cnt--;
	}
out:
	mutex_unlock(&cdata->tm_lock);
	return ret;
}

static inline enum mt6379_chip_rev mt6379_charger_get_chip_rev(struct mt6379_charger_data *cdata)
{
	enum mt6379_chip_rev rev = MT6379_CHIP_REV_E4;
	int ret;
	u32 val;

	ret = regmap_read(cdata->rmap, MT6379_REG_DEV_INFO, &val);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get dev_info, use default rev%d\n",
			 __func__, rev);
		return rev;
	}

	val = FIELD_GET(MT6379_CHIP_REV_MSK, val);
	rev = val < MT6379_CHIP_REV_MAX ? (enum mt6379_chip_rev)val : rev;

	return rev;
}

#endif /* __LINUX_MT6379_CHARGER_H */
