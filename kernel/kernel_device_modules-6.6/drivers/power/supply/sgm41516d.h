/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

// #include "mtk_charger_intf.h"
#include <linux/power_supply.h>
#include "charger_class.h"

#ifndef _sgm41516d_SW_H_
#define _sgm41516d_SW_H_

#define sgm41516d_CON0      0x00
#define sgm41516d_CON1      0x01
#define sgm41516d_CON2      0x02
#define sgm41516d_CON3      0x03
#define sgm41516d_CON4      0x04
#define sgm41516d_CON5      0x05
#define sgm41516d_CON6      0x06
#define sgm41516d_CON7      0x07
#define sgm41516d_CON8      0x08
#define sgm41516d_CON9      0x09
#define sgm41516d_CON10      0x0A
#define	sgm41516d_CON11		0x0B
#define	sgm41516d_CON12		0x0C
#define	sgm41516d_CON13		0x0D
#define	sgm41516d_CON14		0x0E
#define	sgm41516d_CON15		0x0F
#define sgm41516d_REG_NUM 16


/**********************************************************
 *
 *   [MASK/SHIFT]
 *
 *********************************************************/
//CON0
#define CON0_EN_HIZ_MASK   0x01
#define CON0_EN_HIZ_SHIFT  7

#define	CON0_STAT_IMON_CTRL_MASK	0x03
#define	CON0_STAT_IMON_CTRL_SHIFT 5

#define CON0_IINLIM_MASK   0x1F
#define CON0_IINLIM_SHIFT  0

//CON1
#define CON1_PFM_MASK     0x01
#define CON1_PFM_SHIFT    7

#define CON1_WDT_RST_MASK     0x01
#define CON1_WDT_RST_SHIFT    6

#define CON1_OTG_CONFIG_MASK	0x01
#define CON1_OTG_CONFIG_SHIFT	5

#define CON1_CHG_CONFIG_MASK        0x01
#define CON1_CHG_CONFIG_SHIFT       4

#define CON1_SYS_MIN_MASK        0x07
#define CON1_SYS_MIN_SHIFT       1

#define	CON1_MIN_VBAT_SEL_MASK	0x01
#define	CON1_MIN_VBAT_SEL_SHIFT	0

//CON2
#define CON2_BOOST_LIM_MASK   0x01
#define CON2_BOOST_LIM_SHIFT  7

#define	CON2_Q1_FULLON_MASK		0x01
#define	CON2_Q1_FULLON_SHIFT	6

#define CON2_ICHG_MASK    0x3F
#define CON2_ICHG_SHIFT   0

//CON3
#define CON3_IPRECHG_MASK   0x0F
#define CON3_IPRECHG_SHIFT  4

#define CON3_ITERM_MASK           0x0F
#define CON3_ITERM_SHIFT          0

//CON4
#define CON4_VREG_MASK     0x1F
#define CON4_VREG_SHIFT    3

#define	CON4_TOPOFF_TIMER_MASK 0x03
#define	CON4_TOPOFF_TIMER_SHIFT 1

#define CON4_VRECHG_MASK    0x01
#define CON4_VRECHG_SHIFT   0

//CON5
#define CON5_EN_TERM_MASK      0x01
#define CON5_EN_TERM_SHIFT     7

#define CON5_WATCHDOG_MASK     0x03
#define CON5_WATCHDOG_SHIFT    4

#define CON5_EN_TIMER_MASK      0x01
#define CON5_EN_TIMER_SHIFT     3

#define CON5_CHG_TIMER_MASK           0x01
#define CON5_CHG_TIMER_SHIFT          2

#define CON5_TREG_MASK     0x01
#define CON5_TREG_SHIFT    1


//CON6
#define	CON6_OVP_MASK		0x03
#define	CON6_OVP_SHIFT		6

#define	CON6_BOOSTV_MASK	0x3
#define	CON6_BOOSTV_SHIFT	4

#define	CON6_VINDPM_MASK	0x0F
#define	CON6_VINDPM_SHIFT	0

//CON7
#define	CON7_FORCE_DPDM_MASK	0x01
#define	CON7_FORCE_DPDM_SHIFT	7

#define CON7_TMR2X_EN_MASK      0x01
#define CON7_TMR2X_EN_SHIFT     6

#define CON7_BATFET_Disable_MASK      0x01
#define CON7_BATFET_Disable_SHIFT     5

#define	CON7_BATFET_DLY_MASK		0x01
#define	CON7_BATFET_DLY_SHIFT		3

#define	CON7_BATFET_RST_EN_MASK		0x01
#define	CON7_BATFET_RST_EN_SHIFT	2

#define	CON7_VDPM_BAT_TRACK_MASK	0x03
#define	CON7_VDPM_BAT_TRACK_SHIFT	0

#define	CON7_BOOST_CUR_LIMIT_MASK	0x03
#define	CON7_BOOST_CUR_LIMIT_SHIFT	0


//CON8
#define CON8_VBUS_STAT_MASK      0x07
#define CON8_VBUS_STAT_SHIFT     5

#define CON8_CHRG_STAT_MASK           0x03
#define CON8_CHRG_STAT_SHIFT          3

#define CON8_PG_STAT_MASK           0x01
#define CON8_PG_STAT_SHIFT          2

#define CON8_THERM_STAT_MASK           0x01
#define CON8_THERM_STAT_SHIFT          1

#define CON8_VSYS_STAT_MASK           0x01
#define CON8_VSYS_STAT_SHIFT          0

//CON9
#define CON9_WATCHDOG_FAULT_MASK      0x01
#define CON9_WATCHDOG_FAULT_SHIFT     7

#define CON9_OTG_FAULT_MASK           0x01
#define CON9_OTG_FAULT_SHIFT          6

#define CON9_CHRG_FAULT_MASK           0x03
#define CON9_CHRG_FAULT_SHIFT          4

#define CON9_BAT_FAULT_MASK           0x01
#define CON9_BAT_FAULT_SHIFT          3

#define CON9_NTC_FAULT_MASK           0x07
#define CON9_NTC_FAULT_SHIFT          0

//CON10
#define	CON10_VBUS_GD_MASK				0x01
#define	CON10_VBUS_GD_SHIFT				7

#define	CON10_VINDPM_STAT_MASK			0x01
#define	CON10_VINDPM_STAT_SHIFT			6

#define	CON10_IINDPM_STAT_MASK			0x01
#define	CON10_IINDPM_STAT_SHIFT			5

#define	CON10_TOPOFF_ACTIVE_MASK		0x01
#define	CON10_TOPOFF_ACTIVE_SHIFT		3

#define	CON10_ACOV_STAT_MASK			0x01
#define	CON10_ACOV_STAT_SHIFT			2

#define	CON10_VINDPM_INT_MASK			0x01
#define	CON10_VINDPM_INT_SHIFT			1

#define	CON10_INT_MASK_MASK				0x03
#define	CON10_INT_MASK_SHIFT			0

//CON11
#define CON11_REG_RST_MASK     0x01
#define CON11_REG_RST_SHIFT    7


#define CON11_PN_MASK		0x0F
#define CON11_PN_SHIFT		3

#define CON11_Rev_MASK           0x03
#define CON11_Rev_SHIFT          0

//CON13
#define CV_OFFSET     3856000
#define SPECIAL_CV_VAL     4352000
#define SPECIAL_CV_BIT     0xF

#define CON13_REG_EN_PUMPX_MASK     0x01
#define CON13_REG_EN_PUMPX_SHIFT    7
#define CON13_REG_DP_VSET_MASK     0x3
#define CON13_REG_DP_VSET_SHIFT    3
#define CON13_REG_DM_VSET_MASK     0x3
#define CON13_REG_DM_VSET_SHIFT    1

//CON15

//MIVR VALUE
#define VINDPM_OS0_MIVR_MIN		3900
#define VINDPM_OS0_MIVR_MAX		5400
#define VINDPM_OS1_MIVR_MIN		5900
#define VINDPM_OS1_MIVR_MAX		7400
#define VINDPM_OS2_MIVR_MIN		7500
#define VINDPM_OS2_MIVR_MAX		9000
#define VINDPM_OS3_MIVR_MIN		10500
#define VINDPM_OS3_MIVR_MAX		12000
#define VINDPM_OS_MIVR_STEP	100

#define CON15_REG_VINDPM_OS_MASK     0x03
#define CON15_REG_VINDPM_OS_SHIFT    0

#define CON15_REG_FINE_TUNING_MASK     0x3
#define CON15_REG_FINE_TUNING_SHIFT    6

//CV fine tuning
#define SMG41516_CV_TUNING_POST_VAL 8000
#define SMG41516_CV_TUNING_POST 1

#define SMG41516_CV_TUNING_NEG_VAL 8000
#define SMG41516_CV_TUNING_NEG 2

#define bq25600d_VENDOR_ID		(0x01)
#define bq25601_VENDOR_ID		(0x02)
#define SGM41516_VENDOR_ID		(0x0C)
#define SGM41516D_VENDOR_ID		(0x0D)
#define SGM41516E_VENDOR_ID		(0x0E)

struct chg_type_record{
	int chg_type;
	int chg_type_count;
};

#define  RECORD_NUM		(11)
struct sgm41516d_info {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	struct i2c_client *client;
	struct mutex sgm41516d_i2c_access;
	struct mutex sgm41516d_access_lock;
	struct regulator_dev *otg_rdev;
	struct power_supply_desc psy_desc;
	struct power_supply *psy;
	struct work_struct bc12_work;
	atomic_t attach;
	struct mutex attach_lock;
	int psy_usb_type;
	const char *chg_dev_name;
	const char *eint_name;
	int sgm41516d_otg_enable_flag;
	int irq;
	int jeita_hight_temp_cv;
	int temp_cv;
	int vendor_id;
	bool bc12_done;
};

/**********************************************************
 *
 *   [Extern Function]
 *
 *********************************************************/
//CON0----------------------------------------------------
extern void sgm41516d_set_en_hiz(struct sgm41516d_info *info, bool val);
extern void sgm41516d_set_vindpm(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_iinlim(struct sgm41516d_info *info, unsigned int val);
//CON1----------------------------------------------------
extern void sgm41516d_set_reg_rst(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_pfm(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_wdt_rst(struct sgm41516d_info *info, bool val);
extern void sgm41516d_set_chg_config(struct sgm41516d_info *info, bool val);
extern void sgm41516d_set_otg_config(struct sgm41516d_info *info, bool val);
extern void sgm41516d_set_sys_min(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_boost_lim(struct sgm41516d_info *info, unsigned int val);
//CON2----------------------------------------------------
extern void sgm41516d_set_ichg(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_rdson(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_force_20pct(struct sgm41516d_info *info, unsigned int val);
//CON3----------------------------------------------------
extern void sgm41516d_set_iprechg(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_iterm(struct sgm41516d_info *info, unsigned int val);
//CON4----------------------------------------------------
extern void sgm41516d_set_vreg(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_batlowv(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_vrechg(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_topoff_timer(struct sgm41516d_info *info, unsigned int val);
//CON5----------------------------------------------------
extern void sgm41516d_set_en_term(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_term_stat(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_watchdog(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_en_timer(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_chg_timer(struct sgm41516d_info *info, unsigned int val);
//CON6----------------------------------------------------
extern void sgm41516d_set_treg(struct sgm41516d_info *info, unsigned int val);
//CON7----------------------------------------------------
extern void sgm41516d_set_tmr2x_en(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_batfet_disable(struct sgm41516d_info *info, unsigned int val);
extern void sgm41516d_set_int_mask(struct sgm41516d_info *info, unsigned int val);
//CON8----------------------------------------------------
extern unsigned int sgm41516d_get_system_status(struct sgm41516d_info *info);
extern unsigned int sgm41516d_get_vbus_stat(struct sgm41516d_info *info);
extern unsigned int sgm41516d_get_chrg_stat(struct sgm41516d_info *info);
extern unsigned int sgm41516d_get_vsys_stat(struct sgm41516d_info *info);
//---------------------------------------------------------

extern unsigned int sgm41516d_read_interface (struct sgm41516d_info *info,
unsigned char RegNum, unsigned char *val, unsigned char MASK, unsigned char SHIFT);
extern unsigned int sgm41516d_config_interface (struct sgm41516d_info *info,
unsigned char RegNum, unsigned char val, unsigned char MASK, unsigned char SHIFT);
#endif // _sgm41516d_SW_H_
