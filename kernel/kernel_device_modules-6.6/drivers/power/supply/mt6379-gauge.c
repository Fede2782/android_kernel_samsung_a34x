// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author:
 *   ChiYuan Huang <cy_huang@richtek.com>
 *   ChiaEn Wu <chiaen_wu@richtek.com>
 */

#include <linux/bitfield.h>
#include <linux/cdev.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <net/sock.h>

#include "mtk_battery.h"
#include "mtk_gauge.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

/* Commom REG */
#define MT6379_REG_CORE_CTRL0			0x001
#define MT6379_REG_TM_PAS_CODE1			0x007
#define MT6379_REG_FGADC_CLK_CTRL		0x010
#define MT6379_REG_SPMI_TXDRV2			0x02B
#define MT6379_REG_GM30_EVT			0x068
#define MT6379_REG_AUXADC_DIG_3_ELR0		0x343 /* MT6379_REG_AUXADC_EFUSE_GAIN_ERR */
#define MT6379_REG_LDO_EVENT			0x85D /* 6379 have only one LDO in BAT1 HK1 */
#define MT6379_REG_LDO_STAT			0x85E /* 6379 have only one LDO in BAT1 HK1 */

/* MASK */
#define MT6379_MASK_CELL_COUNT			BIT(7)
#define VBAT_MON_EN_MASK			BIT(5)
#define MT6379_MASK_RCS_INT_DONE		BIT(0)
#define MT6379_MASK_BM1_EVT			BIT(3)
#define MT6379_MASK_BM2_EVT			BIT(1)
#define MT6379_MASK_GM_LDO_EVT			BIT(1) /* 6379 have only one LDO in BAT1 HK1 */
#define MT6379_MASK_FORCE_BATON_EN		BIT(7)

#define NUM_IRQ_REG				3

#define RG_INT_EN_FG_ZCV_MASK			BIT(4)
#define FG_GAINERR_SEL_MASK			GENMASK(1, 0)
#define FG_ON_MASK				BIT(0)
#define FG_ZCV_DET_EN_MASK			BIT(2)
#define FG_ZCV_DET_EN_SHIFT			2
#define FG_LATCHDATA_ST_MASK			BIT(7)
#define FG_N_CHARGE_RST_MASK			BIT(3)
#define FG_CHARGE_RST_MASK			BIT(2)
#define FG_TIME_RST_MASK			BIT(1)
#define FG_SW_CLEAR_MASK			BIT(3)
#define FG_SW_READ_PRE_MASK			BIT(0)
#define FG_RSTB_STATUS_MASK			BIT(0)
#define FG_RSTB_STATUS_SHIFT			0

#define FG_N_CHARGE_TH_MASK			GENMASK(31, 0)
#define FG_IAVG_15_00_MASK			GENMASK(15, 0)
#define FG_IAVG_VLD_MASK			BIT(0)
#define FG_IAVG_27_16_MASK			GENMASK(11, 0)

#define FGADC_NTER_MASK				GENMASK(29, 0)
#define FGADC_ZCV_CON0_RSV			BIT(7)
#define FG_ZCV_DET_IV_MASK			GENMASK(3, 0)
#define FG_ZCV_DET_IV_SHIFT			0
#define FG_ZCV_CAR_TH_MASK			GENMASK(30, 0)

#define RESET_MASK				BIT(0)
#define HK_STRUP_AUXADC_START_SEL_MASK		BIT(2)
#define HK_STRUP_AUXADC_START_SEL_SHIFT		2
#define AD_BATON_UNDET_MASK			BIT(1)

#define AUXADC_ADC_RDY_PWRON_PCHR_MASK		BIT(15)
#define AUXADC_ADC_OUT_PWRON_PCHR_MASK		GENMASK(14, 0)
#define AUXADC_ADC_RDY_WAKEUP_PCHR_MASK		BIT(15)
#define AUXADC_ADC_OUT_WAKEUP_PCHR_MASK		GENMASK(14, 0)
#define AUXADC_ADC_OUT_FGADC_PCHR_MASK		GENMASK(14, 0)
#define AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_MASK	BIT(15)
#define AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK	GENMASK(14, 0)
#define AUXADC_ADC_OUT_NAG_MASK			GENMASK(14, 0)
#define AUXADC_ADC_RDY_PWRON_CLR_MASK		BIT(3)
#define AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK	BIT(2)
#define AUXADC_ADC_RDY_WAKEUP_CLR_MASK		BIT(0)
#define AUXADC_LBAT2_EN_MASK			BIT(0)
#define AUXADC_LBAT2_DEBT_MIN_SEL_MASK		GENMASK(5, 4)
#define AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT		4
#define AUXADC_LBAT2_DEBT_MAX_SEL_MASK		GENMASK(3, 2)
#define AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT		2
#define AUXADC_LBAT2_DET_PRD_SEL_MASK		GENMASK(1, 0)
#define AUXADC_LBAT2_DET_PRD_SEL_SHIFT		0
#define AUXADC_LBAT2_DET_MAX_MASK		BIT(1)
#define AUXADC_LBAT2_DET_MAX_SHIFT		1
#define AUXADC_LBAT2_IRQ_EN_MAX_MASK		BIT(0)
#define AUXADC_LBAT2_IRQ_EN_MAX_SHIFT		0
#define AUXADC_LBAT2_VOLT_MAX_MASK		GENMASK(11, 0)
#define AUXADC_LBAT2_DET_MIN_MASK		BIT(1)
#define AUXADC_LBAT2_DET_MIN_SHIFT		1
#define AUXADC_LBAT2_IRQ_EN_MIN_MASK		BIT(0)
#define AUXADC_LBAT2_IRQ_EN_MIN_SHIFT		0
#define AUXADC_LBAT2_VOLT_MIN_MASK		GENMASK(11, 0)
#define AUXADC_BAT_TEMP_EN_MASK			BIT(0)
#define AUXADC_BAT_TEMP_FROZE_EN_MASK		BIT(0)
#define AUXADC_BAT_TEMP_DEBT_MIN_SEL_MASK	GENMASK(5, 4)
#define AUXADC_BAT_TEMP_DEBT_MIN_SEL_SHIFT	4
#define AUXADC_BAT_TEMP_DEBT_MAX_SEL_MASK	GENMASK(3, 2)
#define AUXADC_BAT_TEMP_DEBT_MAX_SEL_SHIFT	2
#define AUXADC_BAT_TEMP_DET_PRD_SEL_MASK	GENMASK(1, 0)
#define AUXADC_BAT_TEMP_DET_PRD_SEL_SHIFT	0
#define AUXADC_BAT_TEMP_DET_MAX_MASK		BIT(1)
#define AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK		BIT(0)
#define AUXADC_BAT_TEMP_VOLT_MAX_MASK		GENMASK(11, 0)
#define AUXADC_BAT_TEMP_DET_MIN_MASK		BIT(1)
#define AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK		BIT(0)
#define AUXADC_BAT_TEMP_VOLT_MIN_MASK		GENMASK(11, 0)
#define AUXADC_NAG_IRQ_EN_MASK			BIT(5)
#define AUXADC_NAG_PRD_MASK			GENMASK(4, 3)
#define AUXADC_NAG_PRD_SHIFT			3
#define AUXADC_NAG_EN_MASK			BIT(0)
#define AUXADC_NAG_ZCV_MASK			GENMASK(14, 0)
#define AUXADC_NAG_C_DLTV_TH_MASK		GENMASK(26, 0)
#define AUXADC_NAG_CNT_MASK			GENMASK(25, 0)
#define AUXADC_NAG_C_DLTV_MASK			GENMASK(26, 0)

#define HTOL_THRESHOLD_MAX			20
#define HTOL_THRESHOLD_MIN			5
#define HTOL_CALI_MAX				267

/* MT6379 915.530 uA */
#define MT6379_UNIT_FGCURRENT			915527

/* MT6379 CHARGE_LSB 0.128 uA */
#define MT6379_UNIT_CHARGE			128

/* AUXADC */
#define R_VAL_TEMP_2				15
#define R_VAL_TEMP_3				40

#define UNIT_TIME				50

#define MT6379_UNIT_FG_IAVG			457764	/* 457.764 uA */
#define MT6379_UNIT_FGCAR_ZCV			128	/* 0.128 uAh */

#define VOLTAGE_FULL_RANGES			1840
#define ADC_PRECISE				32768	/* 15 bits */

#define CAR_TO_REG_SHIFT			5
/*coulomb interrupt lsb might be different with coulomb lsb */
#define CAR_TO_REG_FACTOR			0x2E14
/* 1000 * 1000 / CHARGE_LSB */
#define UNIT_FGCAR				174080

/* Latch timeout, and avoid return same error code as SPMI timeout */
#define MT6379_LATCH_TIMEOUT			5526789

enum mt6379_fg_rg_list {
	/* BM (BAT1 : 0x7xx, BAT2: 0xAxx) */
	MT6379_REG_BM_TOP_INT_CON0 = 0,			/* 0x24 */
	MT6379_REG_BM_TOP_INT_CON0_SET,			/* 0x25 */
	MT6379_REG_BM_TOP_INT_CON0_CLR,			/* 0x26 */
	MT6379_REG_BM_TOP_INT_MASK_CON0,		/* 0x2D */
	MT6379_REG_BM_TOP_INT_MASK_CON0_SET,		/* 0x2E */
	MT6379_REG_BM_TOP_INT_MASK_CON0_CLR,		/* 0x2F */
	MT6379_REG_BM_TOP_INT_STATUS0,			/* 0x36 */
	MT6379_REG_FGADC_CUR_CON4,			/* 0x44	: CIC3 output */
	MT6379_REG_FGADC_ANA_ELR4,			/* 0x63 */
	MT6379_REG_FGADC_GAINERR_CAL_L,			/* 0x5B:   5 mOhm efuse gain err */
	MT6379_REG_FGADC_GAINERR_CAL_2MOHM_L,		/* 0x5D:   2 mOhm efuse gain err */
	MT6379_REG_FGADC_GAINERR_CAL_1MOHM_L,		/* 0x5F:   1 mOhm efuse gain err */
	MT6379_REG_FGADC_GAINERR_CAL_0P5MOHM_L,		/* 0x61: 0.5 mOhm efuse gain err */
	MT6379_REG_FGADC_CON0,				/* 0x6D */
	MT6379_REG_FGADC_CON1,				/* 0x6E */
	MT6379_REG_FGADC_CON2,				/* 0x6F */
	MT6379_REG_FGADC_CON3,				/* 0x70 */
	MT6379_REG_FGADC_RST_CON0,			/* 0x77 */
	MT6379_REG_FGADC_CAR_CON0,			/* 0x78 */
	MT6379_REG_FGADC_CAR_CON1,			/* 0x7A */
	MT6379_REG_FGADC_CARTH_CON0,			/* 0x7C */
	MT6379_REG_FGADC_CARTH_CON1,			/* 0x7E */
	MT6379_REG_FGADC_CARTH_CON2,			/* 0x80 */
	MT6379_REG_FGADC_CARTH_CON3,			/* 0x82 */
	MT6379_REG_FGADC_NCAR_CON0,			/* 0x84 */
	MT6379_REG_FGADC_NCAR_CON2,			/* 0x88 */
	MT6379_REG_FGADC_IAVG_CON0,			/* 0x8C */
	MT6379_REG_FGADC_IAVG_CON1,			/* 0x8E */
	MT6379_REG_FGADC_IAVG_CON2,			/* 0x8F */
	MT6379_REG_FGADC_IAVG_CON3,			/* 0x91 */
	MT6379_REG_FGADC_IAVG_CON5,			/* 0x95 */
	MT6379_REG_FGADC_NTER_CON0,			/* 0x99 */
	MT6379_REG_FGADC_ZCV_CON0,			/* 0xAE */
	MT6379_REG_FGADC_ZCV_CON2,			/* 0xB0 */
	MT6379_REG_FGADC_ZCVTH_CON0,			/* 0xB6 */
	MT6379_REG_FGADC_R_CON0	,			/* 0xE5 */
	MT6379_REG_FGADC_CUR_CON0,			/* 0xE7	: CIC1 output */
	MT6379_REG_FGADC_CUR_CON3,			/* 0xED	: CIC2 output */
	MT6379_REG_FGADC_GAIN_CON0,			/* 0xF6	: sw gain err sel */
	MT6379_REG_SYSTEM_INFO_CON0,			/* 0xF9 */
	MT6379_REG_SYSTEM_INFO_CON1,			/* 0xFB */

	/* HK1 (BAT1 : 0x8xx, BAT2: 0xBxx) */
	MT6379_REG_HK_TOP_RST_CON0,			/* 0x0F */
	MT6379_REG_HK_TOP_STRUP_CON1,			/* 0x25 */
	MT6379_REG_HK_TOP_WKEY,				/* 0x28 */
	MT6379_REG_BATON_ANA_CON0,			/* 0x87 */
	MT6379_REG_BATON_ANA_MON0,			/* 0x88 */

	/* HK2 (BAT1 : 0x9xx, BAT2: 0xCxx) */
	MT6379_REG_AUXADC_ADC_OUT_PWRON_PCHR,		/* 0x0C */
	MT6379_REG_AUXADC_ADC_OUT_WAKEUP_PCHR,		/* 0x0E */
	MT6379_REG_AUXADC_ADC_OUT_FGADC_PCHR,		/* 0x12 */
	MT6379_REG_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR,	/* 0x14 */
	MT6379_REG_AUXADC_ADC_OUT_NAG,			/* 0x18 */

	MT6379_REG_AUXADC_CON42,			/* 0x67 */
	MT6379_REG_AUXADC_EFUSE_GAIN_TRIM,		/* 0x6E */
	MT6379_REG_AUXADC_EFUSE_OFFSET_TRIM,		/* 0x70 */
	MT6379_REG_AUXADC_LBAT2_0,			/* 0xB9 */
	MT6379_REG_AUXADC_LBAT2_1,			/* 0xBA */
	MT6379_REG_AUXADC_LBAT2_2,			/* 0xBB */
	MT6379_REG_AUXADC_LBAT2_3,			/* 0xBC */
	MT6379_REG_AUXADC_LBAT2_5,			/* 0xBE */
	MT6379_REG_AUXADC_LBAT2_6,			/* 0xBF */
	MT6379_REG_AUXADC_BAT_TEMP_0,			/* 0xC5 */
	MT6379_REG_AUXADC_BAT_TEMP_1,			/* 0xC6 */
	MT6379_REG_AUXADC_BAT_TEMP_2,			/* 0xC7 */
	MT6379_REG_AUXADC_BAT_TEMP_3,			/* 0xC8 */
	MT6379_REG_AUXADC_BAT_TEMP_4,			/* 0xC9 */
	MT6379_REG_AUXADC_BAT_TEMP_6,			/* 0xCB */
	MT6379_REG_AUXADC_BAT_TEMP_7,			/* 0xCC */
	MT6379_REG_AUXADC_NAG_0,			/* 0xD2 */
	MT6379_REG_AUXADC_NAG_1,			/* 0xD3 */
	MT6379_REG_AUXADC_NAG_2,			/* 0xD4 */
	MT6379_REG_AUXADC_NAG_3,			/* 0xD5 */
	MT6379_REG_AUXADC_NAG_4,			/* 0xD6 */
	MT6379_REG_AUXADC_NAG_5,			/* 0xD7 */
	MT6379_REG_AUXADC_NAG_6,			/* 0xD8 */
	MT6379_REG_AUXADC_NAG_7,			/* 0xD9 */
	MT6379_REG_AUXADC_NAG_8,			/* 0xDA */
	MT6379_REG_AUXADC_NAG_9,			/* 0xDB */
	MT6379_REG_AUXADC_NAG_10,			/* 0xDC */
	MT6379_REG_AUXADC_NAG_11,			/* 0xDD */
	MT6379_REG_AUXADC_NAG_12,			/* 0xDE */
	MT6379_REG_AUXADC_NAG_13,			/* 0xDF */
	MT6379_REG_AUXADC_NAG_14,			/* 0xE0 */
	MT6379_REG_AUXADC_NAG_15,			/* 0xE1 */
	MT6379_REG_AUXADC_NAG_16,			/* 0xE2 */
	MT6379_REG_AUXADC_NAG_17,			/* 0xE3 */
	MT6379_REG_AUXADC_NAG_18,			/* 0xE4 */

	/* END OF REG LIST */
	MT6379_FG_RG_MAX				/* Sentinel */
};

static const unsigned int rg[][MT6379_FG_RG_MAX] = {
	{
		/* BAT1 BM : 0x7xx */
		[MT6379_REG_BM_TOP_INT_CON0]			= 0x724,
		[MT6379_REG_BM_TOP_INT_CON0_SET]		= 0x725,
		[MT6379_REG_BM_TOP_INT_CON0_CLR]		= 0x726,
		[MT6379_REG_BM_TOP_INT_MASK_CON0]		= 0x72D,
		[MT6379_REG_BM_TOP_INT_MASK_CON0_SET]		= 0x72E,
		[MT6379_REG_BM_TOP_INT_MASK_CON0_CLR]		= 0x72F,
		[MT6379_REG_BM_TOP_INT_STATUS0]			= 0x736,
		[MT6379_REG_FGADC_CUR_CON4]			= 0x744, /* BAT1 CIC3 output */
		[MT6379_REG_FGADC_ANA_ELR4]			= 0x763,
		[MT6379_REG_FGADC_CON0]				= 0x76D,
		[MT6379_REG_FGADC_CON1]				= 0x76E,
		[MT6379_REG_FGADC_CON2]				= 0x76F,
		[MT6379_REG_FGADC_CON3]				= 0x770,
		[MT6379_REG_FGADC_RST_CON0]			= 0x777,
		[MT6379_REG_FGADC_CAR_CON0]			= 0x778,
		[MT6379_REG_FGADC_CAR_CON1]			= 0x77A,
		[MT6379_REG_FGADC_CARTH_CON0]			= 0x77C,
		[MT6379_REG_FGADC_CARTH_CON1]			= 0x77E,
		[MT6379_REG_FGADC_CARTH_CON2]			= 0x780,
		[MT6379_REG_FGADC_CARTH_CON3]			= 0x782,
		[MT6379_REG_FGADC_NCAR_CON0]			= 0x784,
		[MT6379_REG_FGADC_NCAR_CON2]			= 0x788,
		[MT6379_REG_FGADC_IAVG_CON0]			= 0x78C,
		[MT6379_REG_FGADC_IAVG_CON1]			= 0x78E,
		[MT6379_REG_FGADC_IAVG_CON2]			= 0x78F,
		[MT6379_REG_FGADC_IAVG_CON3]			= 0x791,
		[MT6379_REG_FGADC_IAVG_CON5]			= 0x795,
		[MT6379_REG_FGADC_NTER_CON0]			= 0x799,
		[MT6379_REG_FGADC_ZCV_CON0]			= 0x7AE,
		[MT6379_REG_FGADC_ZCV_CON2]			= 0x7B0,
		[MT6379_REG_FGADC_ZCVTH_CON0]			= 0x7B6,
		[MT6379_REG_FGADC_R_CON0]			= 0x7E5,
		[MT6379_REG_FGADC_CUR_CON0]			= 0x7E7, /* BAT1 CIC1 output */
		[MT6379_REG_FGADC_CUR_CON3]			= 0x7ED, /* BAT1 CIC2 output */
		[MT6379_REG_SYSTEM_INFO_CON0]			= 0x7F9,
		[MT6379_REG_SYSTEM_INFO_CON1]			= 0x7FB,

		/* BAT1 HK1 : 0x8xx */
		[MT6379_REG_HK_TOP_RST_CON0]			= 0x80F,
		[MT6379_REG_HK_TOP_STRUP_CON1]			= 0x825,
		[MT6379_REG_HK_TOP_WKEY]			= 0x828,
		[MT6379_REG_BATON_ANA_CON0]			= 0x887,
		[MT6379_REG_BATON_ANA_MON0]			= 0x888,

		/* BAT1 HK2 : 0x9xx */
		[MT6379_REG_AUXADC_ADC_OUT_PWRON_PCHR]		= 0x90C,
		[MT6379_REG_AUXADC_ADC_OUT_WAKEUP_PCHR]		= 0x90E,
		[MT6379_REG_AUXADC_ADC_OUT_FGADC_PCHR]		= 0x912,
		[MT6379_REG_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR]	= 0x914,
		[MT6379_REG_AUXADC_ADC_OUT_NAG]			= 0x918,
		[MT6379_REG_AUXADC_CON42]			= 0x967,
		[MT6379_REG_AUXADC_EFUSE_GAIN_TRIM]		= 0x96E,
		[MT6379_REG_AUXADC_EFUSE_OFFSET_TRIM]		= 0x970,
		[MT6379_REG_AUXADC_LBAT2_0]			= 0x9B9,
		[MT6379_REG_AUXADC_LBAT2_1]			= 0x9BA,
		[MT6379_REG_AUXADC_LBAT2_2]			= 0x9BB,
		[MT6379_REG_AUXADC_LBAT2_3]			= 0x9BC,
		[MT6379_REG_AUXADC_LBAT2_5]			= 0x9BE,
		[MT6379_REG_AUXADC_LBAT2_6]			= 0x9BF,
		[MT6379_REG_AUXADC_BAT_TEMP_0]			= 0x9C5,
		[MT6379_REG_AUXADC_BAT_TEMP_1]			= 0x9C6,
		[MT6379_REG_AUXADC_BAT_TEMP_2]			= 0x9C7,
		[MT6379_REG_AUXADC_BAT_TEMP_3]			= 0x9C8,
		[MT6379_REG_AUXADC_BAT_TEMP_4]			= 0x9C9,
		[MT6379_REG_AUXADC_BAT_TEMP_6]			= 0x9CB,
		[MT6379_REG_AUXADC_BAT_TEMP_7]			= 0x9CC,
		[MT6379_REG_AUXADC_NAG_0]			= 0x9D2,
		[MT6379_REG_AUXADC_NAG_1]			= 0x9D3,
		[MT6379_REG_AUXADC_NAG_2]			= 0x9D4,
		[MT6379_REG_AUXADC_NAG_3]			= 0x9D5,
		[MT6379_REG_AUXADC_NAG_4]			= 0x9D6,
		[MT6379_REG_AUXADC_NAG_5]			= 0x9D7,
		[MT6379_REG_AUXADC_NAG_6]			= 0x9D8,
		[MT6379_REG_AUXADC_NAG_7]			= 0x9D9,
		[MT6379_REG_AUXADC_NAG_8]			= 0x9DA,
		[MT6379_REG_AUXADC_NAG_9]			= 0x9DB,
		[MT6379_REG_AUXADC_NAG_10]			= 0x9DC,
		[MT6379_REG_AUXADC_NAG_11]			= 0x9DD,
		[MT6379_REG_AUXADC_NAG_12]			= 0x9DE,
		[MT6379_REG_AUXADC_NAG_13]			= 0x9DF,
		[MT6379_REG_AUXADC_NAG_14]			= 0x9E0,
		[MT6379_REG_AUXADC_NAG_15]			= 0x9E1,
		[MT6379_REG_AUXADC_NAG_16]			= 0x9E2,
		[MT6379_REG_AUXADC_NAG_17]			= 0x9E3,
		[MT6379_REG_AUXADC_NAG_18]			= 0x9E4,
	},
	{
		/* BAT2 BM : 0xAxx */
		[MT6379_REG_BM_TOP_INT_CON0]			= 0xA24,
		[MT6379_REG_BM_TOP_INT_CON0_SET]		= 0xA25,
		[MT6379_REG_BM_TOP_INT_CON0_CLR]		= 0xA26,
		[MT6379_REG_BM_TOP_INT_MASK_CON0]		= 0xA2D,
		[MT6379_REG_BM_TOP_INT_MASK_CON0_SET]		= 0xA2E,
		[MT6379_REG_BM_TOP_INT_MASK_CON0_CLR]		= 0xA2F,
		[MT6379_REG_BM_TOP_INT_STATUS0]			= 0xA36,
		[MT6379_REG_FGADC_CUR_CON4]			= 0xA44, /* BAT2 CIC3 output */
		[MT6379_REG_FGADC_ANA_ELR4]			= 0xA63,
		[MT6379_REG_FGADC_CON0]				= 0xA6D,
		[MT6379_REG_FGADC_CON1]				= 0xA6E,
		[MT6379_REG_FGADC_CON2]				= 0xA6F,
		[MT6379_REG_FGADC_CON3]				= 0xA70,
		[MT6379_REG_FGADC_RST_CON0]			= 0xA77,
		[MT6379_REG_FGADC_CAR_CON0]			= 0xA78,
		[MT6379_REG_FGADC_CAR_CON1]			= 0xA7A,
		[MT6379_REG_FGADC_CARTH_CON0]			= 0xA7C,
		[MT6379_REG_FGADC_CARTH_CON1]			= 0xA7E,
		[MT6379_REG_FGADC_CARTH_CON2]			= 0xA80,
		[MT6379_REG_FGADC_CARTH_CON3]			= 0xA82,
		[MT6379_REG_FGADC_NCAR_CON0]			= 0xA84,
		[MT6379_REG_FGADC_NCAR_CON2]			= 0xA88,
		[MT6379_REG_FGADC_IAVG_CON0]			= 0xA8C,
		[MT6379_REG_FGADC_IAVG_CON1]			= 0xA8E,
		[MT6379_REG_FGADC_IAVG_CON2]			= 0xA8F,
		[MT6379_REG_FGADC_IAVG_CON3]			= 0xA91,
		[MT6379_REG_FGADC_IAVG_CON5]			= 0xA95,
		[MT6379_REG_FGADC_NTER_CON0]			= 0xA99,
		[MT6379_REG_FGADC_ZCV_CON0]			= 0xAAE,
		[MT6379_REG_FGADC_ZCV_CON2]			= 0xAB0,
		[MT6379_REG_FGADC_ZCVTH_CON0]			= 0xAB6,
		[MT6379_REG_FGADC_R_CON0]			= 0xAE5,
		[MT6379_REG_FGADC_CUR_CON0]			= 0xAE7, /* BAT2 CIC1 output */
		[MT6379_REG_FGADC_CUR_CON3]			= 0xAED, /* BAT2 CIC2 output */
		[MT6379_REG_SYSTEM_INFO_CON0]			= 0xAF9,
		[MT6379_REG_SYSTEM_INFO_CON1]			= 0xAFB,

		/* BAT2 HK1 : 0xBxx */
		[MT6379_REG_HK_TOP_RST_CON0]			= 0xB0F,
		[MT6379_REG_HK_TOP_STRUP_CON1]			= 0xB25,
		[MT6379_REG_HK_TOP_WKEY]			= 0xB28,
		[MT6379_REG_BATON_ANA_CON0]			= 0xB87,
		[MT6379_REG_BATON_ANA_MON0]			= 0xB88,

		/* BAT2 HK2 : 0xCxx */
		[MT6379_REG_AUXADC_ADC_OUT_PWRON_PCHR]		= 0xC0C,
		[MT6379_REG_AUXADC_ADC_OUT_WAKEUP_PCHR]		= 0xC0E,
		[MT6379_REG_AUXADC_ADC_OUT_FGADC_PCHR]		= 0xC12,
		[MT6379_REG_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR]	= 0xC14,
		[MT6379_REG_AUXADC_ADC_OUT_NAG]			= 0xC18,
		[MT6379_REG_AUXADC_CON42]			= 0xC67,
		[MT6379_REG_AUXADC_EFUSE_GAIN_TRIM]		= 0xC6E,
		[MT6379_REG_AUXADC_EFUSE_OFFSET_TRIM]		= 0xC70,
		[MT6379_REG_AUXADC_LBAT2_0]			= 0xCB9,
		[MT6379_REG_AUXADC_LBAT2_1]			= 0xCBA,
		[MT6379_REG_AUXADC_LBAT2_2]			= 0xCBB,
		[MT6379_REG_AUXADC_LBAT2_3]			= 0xCBC,
		[MT6379_REG_AUXADC_LBAT2_5]			= 0xCBE,
		[MT6379_REG_AUXADC_LBAT2_6]			= 0xCBF,
		[MT6379_REG_AUXADC_BAT_TEMP_0]			= 0xCC5,
		[MT6379_REG_AUXADC_BAT_TEMP_1]			= 0xCC6,
		[MT6379_REG_AUXADC_BAT_TEMP_2]			= 0xCC7,
		[MT6379_REG_AUXADC_BAT_TEMP_3]			= 0xCC8,
		[MT6379_REG_AUXADC_BAT_TEMP_4]			= 0xCC9,
		[MT6379_REG_AUXADC_BAT_TEMP_6]			= 0xCCB,
		[MT6379_REG_AUXADC_BAT_TEMP_7]			= 0xCCC,
		[MT6379_REG_AUXADC_NAG_0]			= 0xCD2,
		[MT6379_REG_AUXADC_NAG_1]			= 0xCD3,
		[MT6379_REG_AUXADC_NAG_2]			= 0xCD4,
		[MT6379_REG_AUXADC_NAG_3]			= 0xCD5,
		[MT6379_REG_AUXADC_NAG_4]			= 0xCD6,
		[MT6379_REG_AUXADC_NAG_5]			= 0xCD7,
		[MT6379_REG_AUXADC_NAG_6]			= 0xCD8,
		[MT6379_REG_AUXADC_NAG_7]			= 0xCD9,
		[MT6379_REG_AUXADC_NAG_8]			= 0xCDA,
		[MT6379_REG_AUXADC_NAG_9]			= 0xCDB,
		[MT6379_REG_AUXADC_NAG_10]			= 0xCDC,
		[MT6379_REG_AUXADC_NAG_11]			= 0xCDD,
		[MT6379_REG_AUXADC_NAG_12]			= 0xCDE,
		[MT6379_REG_AUXADC_NAG_13]			= 0xCDF,
		[MT6379_REG_AUXADC_NAG_14]			= 0xCE0,
		[MT6379_REG_AUXADC_NAG_15]			= 0xCE1,
		[MT6379_REG_AUXADC_NAG_16]			= 0xCE2,
		[MT6379_REG_AUXADC_NAG_17]			= 0xCE3,
		[MT6379_REG_AUXADC_NAG_18]			= 0xCE4,
	},
};

enum {
	CHAN_BAT_VOLT = 0,
	CHAN_BAT_TEMP,
	CHAN_PTIM_BAT_VOLT,
	CHAN_PTIM_R,
	CHAN_VREF,
	CHAN_ADC_VBATMON,
	CHAN_MAX
};

enum fg_cic_idx {
	FG_CIC1 = 0,
	FG_CIC2,
	FG_CIC3,
	FG_CIC_MAX
};

/************ bat_cali *******************/
#define BAT_CALI_DEVNAME "MT_pmic_adc_cali"
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
#define Get_META_BAT_CAR_TUNE_VALUE _IOW('k', 12, int)
#define Set_META_BAT_CAR_TUNE_VALUE _IOW('k', 13, int)
#define Set_BAT_DISABLE_NAFG _IOW('k', 14, int)
#define Set_CARTUNE_TO_KERNEL _IOW('k', 15, int)

struct mt6379_fg_info_data {
	int zcv_intr_en_cnt;
	int nafg_en_cnt;
	int en_l_vbat_cnt;
	long long bat_vol_get_t1;

	struct class *bat_cali_class;
	struct device *cdev_dev;
	int bat_cali_major;
	dev_t bat_cali_devno;
	struct cdev bat_cali_cdev;
};

struct gauge_chip_desc {
	const unsigned int bat_idx;
	const unsigned int mask_gm30_evt;
	char *psy_desc_name;
	char *mtk_gauge_name;
	char *gauge_path_name;
	char *gauge_name;
	const char *cdev_gauge_name;
};

struct mt6379_priv {
	struct mtk_gauge gauge;
	struct device *dev;
	struct regmap *regmap;
	struct iio_channel *adcs[CHAN_MAX];
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	struct mutex baton_lock;
	struct mt6379_fg_info_data fg_info;
	const struct gauge_chip_desc *desc;
	bool using_2p;
	int irq;
	u8 unmask_buf[NUM_IRQ_REG];
	u16 gain_err;
	u16 efuse_gain_err;
	int offset_trim;
	int default_r_fg;
	int dts_r_fg;
	int unit_fgcurrent;
	int unit_charge;
	int unit_fg_iavg;
	int unit_fgcar_zcv;
	int latch_timeout_cnt;
	int latch_spmi_timeout_cnt;
};

static int mt6379_enable_tm(struct mt6379_priv *priv, bool en)
{
	static u8 tm_pascode[] = { 0x69, 0x96, 0x63, 0x79 };

	if (en)
		return regmap_bulk_write(priv->regmap, MT6379_REG_TM_PAS_CODE1,
					 tm_pascode, ARRAY_SIZE(tm_pascode));

	return regmap_write(priv->regmap, MT6379_REG_TM_PAS_CODE1, 0);
}

static void gauge_irq_lock(struct irq_data *data)
{
	struct mt6379_priv *priv = irq_data_get_irq_chip_data(data);

	mutex_lock(&priv->irq_lock);
}

static void gauge_irq_sync_unlock(struct irq_data *data)
{
	struct mt6379_priv *priv = irq_data_get_irq_chip_data(data);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int bits = BIT(data->hwirq % 8), ret = 0;
	unsigned int reg, idx = data->hwirq / 8;

	if (priv->unmask_buf[idx] & bits)
		reg = rg[bat_idx][MT6379_REG_BM_TOP_INT_CON0_SET] + idx * 3;
	else
		reg = rg[bat_idx][MT6379_REG_BM_TOP_INT_CON0_CLR] + idx * 3;

	ret = regmap_write(priv->regmap, reg, bits);
	if (ret)
		dev_info(priv->dev, "Failed to set/clr %s irq con %d\n",
			 priv->desc->gauge_name, (int)data->hwirq);

	mutex_unlock(&priv->irq_lock);
}

static void gauge_irq_disable(struct irq_data *data)
{
	struct mt6379_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] &= ~BIT(data->hwirq % 8);
}

static void gauge_irq_enable(struct irq_data *data)
{
	struct mt6379_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] |= BIT(data->hwirq % 8);
}

static int gauge_irq_map(struct irq_domain *h, unsigned int virq, irq_hw_number_t hw)
{
	struct mt6379_priv *priv = h->host_data;

	irq_set_chip_data(virq, priv);
	irq_set_chip(virq, &priv->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, priv->irq);
	irq_set_noprobe(virq);
	return 0;
}

static const struct irq_domain_ops gauge_domain_ops = {
	.map = gauge_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

#define DUMP_REG_BUF_SIZE	1024
static int mt6379_gauge_dump_registers(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	char buf[DUMP_REG_BUF_SIZE] = "\0";
	size_t buf_size = sizeof(buf);
	int i, ret, offset = 0;
	u32 val;

	struct {
		u16 reg;
		const char *name;
	} regs[] = {
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_0], .name = "NAG_0" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_1], .name = "NAG_1" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_2], .name = "NAG_2" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_3], .name = "NAG_3" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_4], .name = "NAG_4" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_5], .name = "NAG_5" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_6], .name = "NAG_6" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_7], .name = "NAG_7" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_8], .name = "NAG_8" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_9], .name = "NAG_9" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_10], .name = "NAG_10" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_11], .name = "NAG_11" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_12], .name = "NAG_12" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_13], .name = "NAG_13" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_14], .name = "NAG_14" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_15], .name = "NAG_15" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_16], .name = "NAG_16" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_17], .name = "NAG_17" },
		{ .reg = rg[bat_idx][MT6379_REG_AUXADC_NAG_18], .name = "NAG_18" },
	};

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_read(priv->regmap, regs[i].reg, &val);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to read %s RG:%s(0x%X)\n",
				 __func__, priv->desc->gauge_name, regs[i].name, regs[i].reg);
			return ret;
		}

		if (i % 4 == 0)
			offset += scnprintf(buf + offset, buf_size - offset, "%s%s, %s: ",
					    i != 0 ? "\n" : "", __func__, priv->desc->gauge_name);

		offset += scnprintf(buf + offset, buf_size - offset,
				    "%s(0x%X) = 0x%02X, ", regs[i].name, regs[i].reg, val);
	}

	dev_info(priv->dev, "%s\n%s, %s latch_timeout_cnt:%d\n",
		 buf, __func__, priv->desc->gauge_name, priv->latch_timeout_cnt);

	return 0;
}

static irqreturn_t gauge_irq_thread(int irq, void *data)
{
	static const u8 mask[NUM_IRQ_REG] = { 0x9F, 0x1B, 0x0D };
	static const u8 no_status[NUM_IRQ_REG];
	u8 status_buf[NUM_IRQ_REG], status;
	struct mt6379_priv *priv = data;
	const unsigned int bat_idx = priv->desc->bat_idx;
	bool handled = false;
	unsigned int reg_val;
	int i, j, ret;

	ret = regmap_read(priv->regmap, MT6379_REG_GM30_EVT, &reg_val);
	if (ret) {
		dev_notice(priv->dev, "%s, Failed to read GM30_EVT\n", __func__);
		return ret;
	}

	/* If no gauge evt, return IRQ_HANDLED */
	if ((reg_val & priv->desc->mask_gm30_evt) == 0)
		return IRQ_HANDLED;

	ret = regmap_raw_read(priv->regmap, rg[bat_idx][MT6379_REG_BM_TOP_INT_STATUS0],
			      status_buf, sizeof(status_buf));
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s INT status\n",
			 __func__, priv->desc->gauge_name);
		return IRQ_HANDLED;
	}

	if (!memcmp(status_buf, no_status, NUM_IRQ_REG))
		return IRQ_HANDLED;

	/* mask irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   rg[bat_idx][MT6379_REG_BM_TOP_INT_MASK_CON0_SET] + i * 3,
				   mask[i]);
		if (ret)
			dev_info(priv->dev, "%s, Failed to mask %s irq[%d]\n",
				 __func__, priv->desc->gauge_name, i);
	}

	for (i = 0; i < NUM_IRQ_REG; i++) {
		status = status_buf[i] & priv->unmask_buf[i];
		if (!status)
			continue;

		for (j = 0; j < 8; j++) {
			if (!(status & BIT(j)))
				continue;

			dev_info(priv->dev, "%s, %s handle gauge irq(reg:0x%X, bit:%d)\n",
				 __func__, priv->desc->gauge_name,
				 rg[bat_idx][MT6379_REG_BM_TOP_INT_STATUS0] + i, j);
			handle_nested_irq(irq_find_mapping(priv->domain, i * 8 + j));
			handled = true;
		}
	}

	/* after process, unmask irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   rg[bat_idx][MT6379_REG_BM_TOP_INT_MASK_CON0_CLR] + i * 3,
				   mask[i]);
		if (ret)
			dev_info(priv->dev, "%s, Failed to unmask %s irq[%d]\n",
				 __func__, priv->desc->gauge_name, i);
	}

	ret = regmap_raw_write(priv->regmap, rg[bat_idx][MT6379_REG_BM_TOP_INT_STATUS0],
			       status_buf, sizeof(status_buf));
	if (ret)
		dev_info(priv->dev, "%s, Failed to clear %s INT status\n", __func__, priv->desc->gauge_name);

	/* MT6379 do retrigger */
	if (handled) {
		ret = regmap_write(priv->regmap, MT6379_REG_SPMI_TXDRV2, MT6379_MASK_RCS_INT_DONE);
		if (ret)
			dev_notice(priv->dev, "%s, Failed to do rcs IRQ retrigger\n", __func__);
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int gauge_add_irq_chip(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	int i, ret;

	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   rg[bat_idx][MT6379_REG_BM_TOP_INT_CON0_CLR] + i * 3, 0xFF);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to disable %s irq con [%d]\n",
				 __func__, priv->desc->gauge_name, i);
			return ret;
		}

		ret = regmap_write(priv->regmap,
				   rg[bat_idx][MT6379_REG_BM_TOP_INT_MASK_CON0] + i * 3, 0);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to init %s irq mask [%d]\n",
				 __func__, priv->desc->gauge_name, i);
			return ret;
		}
	}

	priv->irq_chip.name = dev_name(priv->dev);
	priv->irq_chip.irq_bus_lock = gauge_irq_lock;
	priv->irq_chip.irq_bus_sync_unlock = gauge_irq_sync_unlock;
	priv->irq_chip.irq_disable = gauge_irq_disable;
	priv->irq_chip.irq_enable = gauge_irq_enable;

	priv->domain = irq_domain_add_linear(priv->dev->of_node, NUM_IRQ_REG * 8,
					     &gauge_domain_ops, priv);
	if (!priv->domain) {
		dev_info(priv->dev, "%s, Failed to create %s IRQ domain\n",
			 __func__, priv->desc->gauge_name);
		return -ENOMEM;
	}

	ret = request_threaded_irq(priv->irq, NULL, gauge_irq_thread, IRQF_SHARED | IRQF_ONESHOT,
				   dev_name(priv->dev), priv);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to request %s IRQ %d for %s: %d\n",
			 __func__, priv->desc->gauge_name, priv->irq, dev_name(priv->dev), ret);
		goto err_irq;
	}

	enable_irq_wake(priv->irq);
	return 0;

err_irq:
	irq_domain_remove(priv->domain);
	return ret;
}

static void gauge_del_irq_chip(struct mt6379_priv *priv)
{
	unsigned int virq;
	int hwirq;

	free_irq(priv->irq, priv);

	for (hwirq = 0; hwirq < NUM_IRQ_REG * 8; hwirq++) {
		virq = irq_find_mapping(priv->domain, hwirq);
		if (virq)
			irq_dispose_mapping(virq);
	}

	irq_domain_remove(priv->domain);
}

static int gauge_get_all_auxadc_channels(struct mt6379_priv *priv)
{
	const char *adc_names[CHAN_MAX] = { "bat_volt", "bat_temp", "ptim_bat_volt",
					    "ptim_r", "vref", "adc_vbatmon" };
	struct mtk_gauge *gauge = &priv->gauge;
	int i;

	for (i = 0; i < CHAN_MAX; i++) {
		priv->adcs[i] = devm_iio_channel_get(priv->dev, adc_names[i]);
		if (IS_ERR(priv->adcs[i]))
			return PTR_ERR(priv->adcs[i]);
	}

	/* Filled adc channels into mtk_gauge */
	gauge->chan_bat_temp = priv->adcs[CHAN_BAT_TEMP];
	gauge->chan_bat_voltage = priv->adcs[CHAN_BAT_VOLT];
	gauge->chan_ptim_bat_voltage = priv->adcs[CHAN_PTIM_BAT_VOLT];
	gauge->chan_ptim_r = priv->adcs[CHAN_PTIM_R];
	gauge->chan_bif = priv->adcs[CHAN_VREF];

	return 0;
}

static int gauge_get_all_interrupts(struct mt6379_priv *priv)
{
	struct platform_device *pdev = to_platform_device(priv->dev);
	struct mtk_gauge *gauge = &priv->gauge;
	const char *irq_names[GAUGE_IRQ_MAX] = { "COULOMB_H", "COULOMB_L", "VBAT2_H",
						 "VBAT2_L", "NAFG", "BAT_IN", "BAT_OUT", "ZCV",
						 "FG_N_CHARGE_L", "FG_IAVG_H", "FG_IAVG_L",
						 "BAT_TMP_H", "BAT_TMP_L" };
	int i, irq_no;

	for (i = 0; i < GAUGE_IRQ_MAX; i++) {
		irq_no = platform_get_irq_byname(pdev, irq_names[i]);
		if (irq_no < 0)
			return irq_no;

		irq_set_status_flags(irq_no, IRQ_NOAUTOEN);
		gauge->irq_no[i] = irq_no;
	}

	return 0;
}

static signed int reg_to_mv_value(struct mtk_gauge *gauge, signed int _reg)
{
	long long _reg64 = _reg;
	int ret;

#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * VOLTAGE_FULL_RANGES * R_VAL_TEMP_3) / ADC_PRECISE;
#else
	_reg64 = div_s64(_reg64 * VOLTAGE_FULL_RANGES * R_VAL_TEMP_3, ADC_PRECISE);
#endif
	ret = _reg64;

	bm_debug(gauge->gm, "[%s] %lld => %d\n", __func__, _reg64, ret);
	return ret;
}

static signed int mv_to_reg_value(struct mtk_gauge *gauge, signed int _mv)
{
	int ret;
	long long _reg64 = _mv;
#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * ADC_PRECISE) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3);
#else
	_reg64 = div_s64((_reg64 * ADC_PRECISE), (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3));
#endif
	ret = _reg64;

	if (ret <= 0) {
		bm_err(gauge->gm, "[fg_bat_nafg][%s] mv=%d,%lld => %d,\n", __func__, _mv, _reg64, ret);
		return ret;
	}

	bm_debug(gauge->gm, "[%s] mv=%d,%lld => %d,\n", __func__, _mv, _reg64, ret);
	return ret;
}

static int mv_to_reg_12_temp_value(struct mtk_gauge *gauge, signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_2);

	bm_debug(gauge->gm, "[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static int pre_gauge_update(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int i, ret = 0, max_retry_cnt = 3;
	struct device *dev = priv->dev;
	u32 rdata = 0;

	if (gauge->gm->disableGM30)
		return ret;

	ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON3], &rdata);
	if (ret) {
		dev_info(dev, "%s, Failed to read %s latch and release RG(%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	if (rdata & (FG_SW_READ_PRE_MASK | FG_SW_CLEAR_MASK)) {
		dev_info(dev, "%s, %s not release yet! Try to release again! BM[0x70] = 0x%02X\n",
			 __func__, priv->desc->gauge_name, rdata);
		return -EINVAL;
	}

	ret = regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON3],
				 FG_SW_READ_PRE_MASK | FG_SW_CLEAR_MASK, FG_SW_READ_PRE_MASK);
	if (ret) {
		dev_info(dev, "%s, Failed to set %s pre read(%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	udelay(100);

	for (i = 0; i < max_retry_cnt; i++) {
		ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2], &rdata);
		if (ret) {
			dev_info(dev, "%s, Failed to read %s latch state(%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		if (rdata & FG_LATCHDATA_ST_MASK)
			break;

		udelay(50);
	}

	if (i == max_retry_cnt) {
		dev_info(dev, "%s, %s timeout! last BM[0x6F]=0x%x\n",
			 __func__, priv->desc->gauge_name, rdata);

		ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON3], &rdata);
		dev_notice(dev, "%s, %s BM[0x70]=0x%x, ret:%d\n",
			   __func__, priv->desc->gauge_name, rdata, ret);

		ret = regmap_read(gauge->regmap, MT6379_REG_LDO_EVENT, &rdata);
		dev_notice(dev, "%s, HK1[0x85D]=0x%x, ret:%d\n", __func__, rdata, ret);

		ret = regmap_read(gauge->regmap, MT6379_REG_LDO_STAT, &rdata);
		dev_notice(dev, "%s, HK1[0x85E]=0x%x, ret:%d\n", __func__, rdata, ret);

		ret = MT6379_LATCH_TIMEOUT;
	}

	return ret;
}

void disable_all_irq(struct mtk_battery *gm)
{
	disable_gauge_irq(gm->gauge, COULOMB_H_IRQ);
	disable_gauge_irq(gm->gauge, COULOMB_L_IRQ);
	disable_gauge_irq(gm->gauge, VBAT_H_IRQ);
	disable_gauge_irq(gm->gauge, VBAT_L_IRQ);
	disable_gauge_irq(gm->gauge, NAFG_IRQ);
	disable_gauge_irq(gm->gauge, BAT_PLUGOUT_IRQ);
	disable_gauge_irq(gm->gauge, ZCV_IRQ);
	disable_gauge_irq(gm->gauge, FG_N_CHARGE_L_IRQ);
	disable_gauge_irq(gm->gauge, FG_IAVG_H_IRQ);
	disable_gauge_irq(gm->gauge, FG_IAVG_L_IRQ);
	disable_gauge_irq(gm->gauge, BAT_TMP_H_IRQ);
	disable_gauge_irq(gm->gauge, BAT_TMP_L_IRQ);
}

static void post_gauge_update(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int i, ret, max_retry_cnt = 3;
	u32 regval;

	ret = regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON3],
				 FG_SW_CLEAR_MASK | FG_SW_READ_PRE_MASK, FG_SW_CLEAR_MASK);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to set %s release bit(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return;
	}

	udelay(100);

	for (i = 0; i < max_retry_cnt; i++) {
		ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2], &regval);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to read %s release state(ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			break;
		}

		if (!(regval & FG_LATCHDATA_ST_MASK))
			break;

		udelay(50);
	}

	if (i == max_retry_cnt)
		dev_info(priv->dev, "%s, %s read release state timeout!\n",
			 __func__, priv->desc->gauge_name);

	ret = regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON3],
				 FG_SW_CLEAR_MASK, 0);
	if (ret)
		dev_info(priv->dev, "%s, Failed to clear %s release bit(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
}

static int mv_to_reg_12_value(struct mtk_gauge *gauge, signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3);

	bm_debug(gauge->gm, "[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static int reg_to_current(struct mtk_gauge *gauge, unsigned int regval)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	unsigned short uvalue16 = 0;
	long long temp_value = 0;
	bool is_charging = true;
	int dvalue, retval;

	uvalue16 = (unsigned short) regval;

	dvalue = (unsigned int) uvalue16;
	if (dvalue == 0) {
		temp_value = (long long) dvalue;
		is_charging = false;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		temp_value = (long long) (dvalue - 65535);
		temp_value = temp_value - (temp_value * 2);
		is_charging = false;
	} else {
		temp_value = (long long) dvalue;
	}

	temp_value = temp_value * priv->unit_fgcurrent;
#if defined(__LP64__) || defined(_LP64)
	do_div(temp_value, 100000);
#else
	temp_value = div_s64(temp_value, 100000);
#endif
	retval = (unsigned int) temp_value;

	bm_trace(gauge->gm, "[%s]regval:0x%x,uvalue16:0x%x,dvalue:0x%x,temp_value:0x%x,retval:0x%x,is_charging:%d\n",
		 __func__, regval, uvalue16, dvalue, (int)temp_value,
		 retval, is_charging);

	if (is_charging == false)
		return -retval;

	return retval;
}

static u8 get_rtc_spare0_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u8 *buf, data;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		bm_err(gauge->gm, "%s, Failed to get rtc cell\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		bm_err(gauge->gm, "%s, Failed to read rtc cell\n", __func__);
		return 0;
	}
	bm_debug(gauge->gm, "%s, val=0x%x, %d\n", __func__, *buf, *buf);

	data = *buf;
	kfree(buf);

	return data;
}

static void set_rtc_spare0_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		bm_err(gauge->gm, "%s, Failed to get rtc cell\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);
	if (ret != length)
		bm_err(gauge->gm, "%s, Failed to write rtc cell\n", __func__);
}

static u8 get_rtc_spare_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u8 *buf, data;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		bm_err(gauge->gm, "%s, Failed to get rtc cell\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		bm_err(gauge->gm, "%s, Failed to read rtc cell\n", __func__);
		return 0;
	}

	bm_debug(gauge->gm, "%s, val=%d\n", __func__, *buf);
	data = *buf;
	kfree(buf);

	return data;
}

static void set_rtc_spare_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		bm_err(gauge->gm, "%s, Failed to get rtc cell\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);

	if (ret != length)
		bm_err(gauge->gm, "%s, Failed to write rtc cell\n", __func__);

	bm_debug(gauge->gm, "%s, val=%d\n", __func__, val);
}

static void fgauge_read_RTC_boot_status(struct mtk_gauge *gauge)
{
	unsigned int hw_id = 0x6379;
	u8 spare0_reg = 0;
	unsigned int spare0_reg_b13 = 0;
	u8 spare3_reg = 0;
	int spare3_reg_valid = 0;

	spare0_reg = get_rtc_spare0_fg_value(gauge);
	spare3_reg = get_rtc_spare_fg_value(gauge);
	gauge->hw_status.gspare0_reg = spare0_reg;
	gauge->hw_status.gspare3_reg = spare3_reg;
	spare3_reg_valid = (spare3_reg & 0x80) >> 7;

	if (spare3_reg_valid == 0)
		gauge->hw_status.rtc_invalid = 1;
	else
		gauge->hw_status.rtc_invalid = 0;

	if (gauge->hw_status.rtc_invalid == 0) {
		spare0_reg_b13 = (spare0_reg & 0x20) >> 5;
		gauge->hw_status.is_bat_plugout = !spare0_reg_b13;
		gauge->hw_status.bat_plug_out_time = spare0_reg & 0x1f;
	} else {
		gauge->hw_status.is_bat_plugout = 1;
		gauge->hw_status.bat_plug_out_time = 31;
	}

	bm_err(gauge->gm, "[%s]rtc_invalid %d plugout %d plugout_time %d spare3 0x%x spare0 0x%x hw_id 0x%x\n",
	       __func__, gauge->hw_status.rtc_invalid, gauge->hw_status.is_bat_plugout,
	       gauge->hw_status.bat_plug_out_time, spare3_reg, spare0_reg, hw_id);
}

static int fgauge_set_info(struct mtk_gauge *gauge, enum gauge_property ginfo, unsigned int value)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 regval;
	int ret;

	bm_debug(gauge->gm, "%s, %s set info:%d v:%d\n", __func__, priv->desc->gauge_name, ginfo, value);

	if (ginfo >= GAUGE_PROP_CON1_UISOC)
		ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON1],
				      &regval, sizeof(regval));
	else
		ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON0],
				      &regval, sizeof(regval));
	if (ret)
		return ret;

	switch (ginfo) {
	case GAUGE_PROP_2SEC_REBOOT:
		regval = value ? (regval | BIT(0)) : (regval & ~BIT(0));
		break;
	case GAUGE_PROP_PL_CHARGING_STATUS:
		regval = value ? (regval | BIT(1)) : (regval & ~BIT(1));
		break;
	case GAUGE_PROP_MONITER_PLCHG_STATUS:
		regval = value ? (regval | BIT(2)) : (regval & ~BIT(2));
		break;
	case GAUGE_PROP_BAT_PLUG_STATUS:
		regval = value ? (regval | BIT(3)) : (regval & ~BIT(3));
		break;
	case GAUGE_PROP_IS_NVRAM_FAIL_MODE:
		regval = value ? (regval | BIT(4)) : (regval & ~BIT(4));
		break;
	case GAUGE_PROP_MONITOR_SOFF_VALIDTIME:
		regval = value ? (regval | BIT(5)) : (regval & ~BIT(5));
		break;
	case GAUGE_PROP_CON0_SOC:
		regval &= ~GENMASK(15, 9);
		regval |= ((value / 100) << 9);
		break;
	case GAUGE_PROP_CON1_UISOC:
		regval &= ~GENMASK(6, 0);
		regval |= value;
		break;
	case GAUGE_PROP_CON1_VAILD:
		regval = value ? (regval | BIT(7)) : (regval & ~BIT(7));
		break;
	default:
		return -EINVAL;
	}

	if (ginfo >= GAUGE_PROP_CON1_UISOC)
		return regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON1],
					&regval, sizeof(regval));
	else
		return regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON0],
					&regval, sizeof(regval));
}

static int fgauge_get_info(struct mtk_gauge *gauge, enum gauge_property ginfo, int *value)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 regval = 0;
	int ret;

	if (ginfo >= GAUGE_PROP_CON1_UISOC)
		ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON1],
				      &regval, sizeof(regval));
	else
		ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON0],
				      &regval, sizeof(regval));
	if (ret)
		return ret;

	switch (ginfo) {
	case GAUGE_PROP_2SEC_REBOOT:
		*value = regval & BIT(0);
		break;
	case GAUGE_PROP_PL_CHARGING_STATUS:
		*value = (regval & BIT(1)) >> 1;
		break;
	case GAUGE_PROP_MONITER_PLCHG_STATUS:
		*value = (regval & BIT(2)) >> 2;
		break;
	case GAUGE_PROP_BAT_PLUG_STATUS:
		*value = (regval & BIT(3)) >> 3;
		break;
	case GAUGE_PROP_IS_NVRAM_FAIL_MODE:
		*value = (regval & BIT(4)) >> 4;
		break;
	case GAUGE_PROP_MONITOR_SOFF_VALIDTIME:
		*value = (regval & BIT(5)) >> 5;
		break;
	case GAUGE_PROP_CON0_SOC:
		*value = (regval & GENMASK(15, 9)) >> 9;
		break;
	case GAUGE_PROP_CON1_UISOC:
		*value = (regval & GENMASK(6, 0));
		break;
	case GAUGE_PROP_CON1_VAILD:
		*value = (regval & BIT(7)) >> 7;
		break;
	default:
		return -EINVAL;
	}

	bm_debug(gauge->gm, "%s, %s get info:%d v:%d\n",
		 __func__, priv->desc->gauge_name, ginfo, *value);
	return 0;
}

static unsigned int instant_current_for_car_tune(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 reg_value = 0;
	int ret = 0;

	pre_gauge_update(gauge);

	ret = regmap_raw_read(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_CUR_CON0],
			      &reg_value, sizeof(reg_value));
	if (ret)
		dev_info(priv->dev, "%s, Failed to read %s FGADC_CUR_CON0 (ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);

	post_gauge_update(gauge);

	bm_err(gauge->gm, "%s, %s reg_value=0x%04x\n", __func__, priv->desc->gauge_name, reg_value);

	return ret < 0 ? ret : reg_value;
}

static int calculate_car_tune(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	long long sum_all = 0, Temp_Value1 = 0, current_from_ADC = 0;
	unsigned long long temp_sum = 0, Temp_Value2 = 0;
	int i, cali_car_tune = 0, avg_cnt = 0;
	unsigned int uvalue32 = 0;
	signed int dvalue = 0;

	if (!gauge->gm) {
		dev_info(priv->dev, "%s, gm has not initialized\n", __func__);
		return 0;
	}

	bm_err(gauge->gm, "%s, meta_current=%d,\n", __func__, gauge->hw_status.meta_current);
	if (gauge->hw_status.meta_current != 0) {
		for (i = 0; i < CALI_CAR_TUNE_AVG_NUM; i++) {
			uvalue32 = instant_current_for_car_tune(gauge);
			if (uvalue32 != 0) {
				if (uvalue32 <= 0x8000) {
					Temp_Value1 = (long long)uvalue32;
					bm_err(gauge->gm, "[111]uvalue32 %d Temp_Value1 %lld\n",
					       uvalue32, Temp_Value1);
				} else if (uvalue32 > 0x8000) {
					Temp_Value1 = (long long) (65535 - uvalue32);
					bm_err(gauge->gm, "[222]uvalue32 %d Temp_Value1 %lld\n",
					       uvalue32, Temp_Value1);
				}

				sum_all += Temp_Value1;
				avg_cnt++;
				/*****************/
				bm_err(gauge->gm, "[333]uvalue32 %d Temp_Value1 %lld sum_all %lld\n",
				       uvalue32, Temp_Value1, sum_all);
				/*****************/
			}

			mdelay(30);
		}
		/*calculate the real world data    */
		/*current_from_ADC = sum_all / avg_cnt;*/
		temp_sum = sum_all;
		bm_err(gauge->gm, "[444]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
		       sum_all, temp_sum, avg_cnt, current_from_ADC);

		if (avg_cnt != 0)
			do_div(temp_sum, avg_cnt);

		current_from_ADC = temp_sum;

		bm_err(gauge->gm, "[555]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
		       sum_all, temp_sum, avg_cnt, current_from_ADC);

		Temp_Value2 = current_from_ADC * priv->unit_fgcurrent;

		bm_err(gauge->gm, "[555]Temp_Value2 %lld current_from_ADC %lld priv->unit_fgcurrent %d\n",
		       Temp_Value2, current_from_ADC, priv->unit_fgcurrent);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*do_div(Temp_Value2, 1000000);*/
		do_div(Temp_Value2, 10000);

		bm_err(gauge->gm, "[666]Temp_Value2 %lld current_from_ADC %lld priv->unit_fgcurrent %d\n",
		       Temp_Value2, current_from_ADC, priv->unit_fgcurrent);

		dvalue = (unsigned int) Temp_Value2;

		/* Auto adjust value */
		if (gauge->hw_status.r_fg_value != priv->default_r_fg)
			dvalue = (dvalue * priv->default_r_fg) / gauge->hw_status.r_fg_value;

		bm_err(gauge->gm, "[666]dvalue %d fg_cust_data.r_fg_value %d\n",
		       dvalue, gauge->hw_status.r_fg_value);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*cali_car_tune = meta_input_cali_current * 1000 / dvalue;*/

		if (dvalue != 0) {
			cali_car_tune = gauge->hw_status.meta_current * 1000 * 100 / dvalue;

			bm_err(gauge->gm, "[777]dvalue %d fg_cust_data.r_fg_value %d cali_car_tune %d\n",
			       dvalue, gauge->hw_status.r_fg_value, cali_car_tune);
			gauge->hw_status.tmp_car_tune = cali_car_tune;

			bm_err(gauge->gm,
			       "[fgauge_meta_cali_car_tune_value][%d] meta:%d, adc:%lld, UNI_FGCUR:%d, r_fg_value:%d\n",
			       cali_car_tune, gauge->hw_status.meta_current, current_from_ADC,
			       priv->unit_fgcurrent, gauge->hw_status.r_fg_value);
		}

		return 0;
	}

	return 0;
}

static int info_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int ret = 0;

	if (attr->prop == GAUGE_PROP_CAR_TUNE_VALUE && (val > 500 && val < 1500)) {
		/* send external_current for calculate_car_tune */
		gauge->hw_status.meta_current = val;
		calculate_car_tune(gauge);
	} else if (attr->prop == GAUGE_PROP_R_FG_VALUE && val != 0)
		gauge->hw_status.r_fg_value = val;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_TIME)
		gauge->hw_status.vbat2_det_time = val;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_COUNTER)
		gauge->hw_status.vbat2_det_counter = val;
	else
		ret = fgauge_set_info(gauge, attr->prop, val);

	return ret;
}

static int info_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret = 0;

	if (attr->prop == GAUGE_PROP_CAR_TUNE_VALUE)
		*val = gauge->hw_status.tmp_car_tune;
	else if (attr->prop == GAUGE_PROP_R_FG_VALUE)
		*val = gauge->hw_status.r_fg_value;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_TIME)
		*val = gauge->hw_status.vbat2_det_time;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_COUNTER)
		*val = gauge->hw_status.vbat2_det_counter;
	else
		ret = fgauge_get_info(gauge, attr->prop, val);

	return ret;
}

static void mt6379_2p_battery_baton_force_en(struct mt6379_priv *priv, bool en)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int reg_val;
	int ret = 0;

	if (!priv->using_2p)
		return;

	mutex_lock(&priv->baton_lock);
	ret = regmap_update_bits(priv->regmap, rg[bat_idx][MT6379_REG_BATON_ANA_CON0],
				 MT6379_MASK_FORCE_BATON_EN, en ? MT6379_MASK_FORCE_BATON_EN : 0);
	if (ret) {
		dev_info(priv->dev, "Failed to %s %s force on baton detection\n",
			 en ? "enable" : "disable", priv->desc->gauge_name);
		return;
	}

	/*
	 * Must wait at least 1.5ms to update the status of battery existence,
	 * in order to avoid interference from external factors, set "5ms" here.
	 */
	if (en)
		mdelay(5);

	ret = regmap_read(priv->regmap, rg[bat_idx][MT6379_REG_BATON_ANA_CON0], &reg_val);
	if (ret) {
		dev_info(priv->dev, "Failed to read %s HK1 0x87\n", priv->desc->gauge_name);
		return;
	}

	mutex_unlock(&priv->baton_lock);

	dev_info(priv->dev, "%s %s force_on_baton, HK1 0x87:0x%X\n",
		 en ? "enable" : "disable", priv->desc->gauge_name, reg_val);
}

static int mt6379_get_battery_state(struct mt6379_priv *priv, int *exist)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int regval = 0;
	int ret = 0;

	/* For 2P battery */
	mt6379_2p_battery_baton_force_en(priv, true);

	ret = regmap_read(priv->regmap, rg[bat_idx][MT6379_REG_BATON_ANA_MON0], &regval);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s BATON_ANA_MON0 (ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	/* For 2P battery */
	mt6379_2p_battery_baton_force_en(priv, false);

	regval = regval & AD_BATON_UNDET_MASK;

	*exist = !regval ? 1 : 0;

	if (regval) {
		ret = regmap_update_bits(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
					 AUXADC_ADC_RDY_PWRON_CLR_MASK,
					 AUXADC_ADC_RDY_PWRON_CLR_MASK);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to clear %s PWRON RDY(ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		mdelay(1);
		ret = regmap_update_bits(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
					 AUXADC_ADC_RDY_PWRON_CLR_MASK, 0);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to recovey %s PWRON RDY(ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}
	}

	bm_debug(priv->gauge.gm, "%s, %s %s\n",
		 __func__, priv->desc->gauge_name, *exist ? "exist!" : "not exist!");

	return 0;
}

static inline unsigned int mt6379_get_efuse_gain_err_rg(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;

	switch(priv->dts_r_fg) {
	case 50:
		return rg[bat_idx][MT6379_REG_FGADC_GAINERR_CAL_L];
	case 20:
		return rg[bat_idx][MT6379_REG_FGADC_GAINERR_CAL_2MOHM_L];
	case 10:
		return rg[bat_idx][MT6379_REG_FGADC_GAINERR_CAL_1MOHM_L];
	case 5:
		return rg[bat_idx][MT6379_REG_FGADC_GAINERR_CAL_0P5MOHM_L];
	default:
		return rg[bat_idx][MT6379_REG_FGADC_GAINERR_CAL_1MOHM_L];
	}
}

static int mt6379_reload_fg_efuse_gain_err(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int sw_r_fg_sel, hw_r_fg_sel;
	u16 sw_sel_gain_err, efuse_gain_err;
	struct device *dev = priv->dev;
	int last_ret = 0, ret = 0, bat_exist = 0;

	/* Check if battery exist or not */
	ret = mt6379_get_battery_state(priv, &bat_exist);
	if (ret)
		return ret;

	/* Enable TM */
	ret = mt6379_enable_tm(priv, true);
	if (ret) {
		dev_info(dev, "%s, Failed to enable test mode\n", __func__);
		return ret;
	}

	ret = regmap_raw_read(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_GAIN_CON0],
			      &sw_sel_gain_err, sizeof(sw_sel_gain_err));
	if (ret) {
		dev_info(dev, "%s, Failed to read %s SW gain err\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	ret = regmap_raw_read(priv->regmap, mt6379_get_efuse_gain_err_rg(priv),
			      &efuse_gain_err, sizeof(efuse_gain_err));
	if (ret) {
		dev_info(dev, "%s, Failed to read %s EFUSE gain err\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	/* if EFUSE gain_err == 0x0000, maybe not connect baatery */
	dev_info(dev, "%s, %s r_fg(unit:0.1mOhm):(default:%d, dts:%d), SW FG gain err:0x%04X, FG EFUSE gain err:0x%04X\n",
		 __func__, priv->desc->gauge_name, priv->default_r_fg, priv->dts_r_fg,
		 sw_sel_gain_err, efuse_gain_err);

	if (priv->dts_r_fg != priv->default_r_fg)
		priv->default_r_fg = priv->dts_r_fg;

	/*
	 * R_FG (unit: 0.1mOhm):
	 * 50 -->   5 mOhm --> Level 0
	 * 20 -->   2 mOhm --> Level 1
	 * 10 -->   1 mOhm --> Level 2
	 *  5 --> 0.5 mOhm --> Level 3
	 */
	sw_r_fg_sel = priv->dts_r_fg == 50 ? 0 : priv->dts_r_fg == 20 ? 1 :
		      priv->dts_r_fg == 10 ? 2 : priv->dts_r_fg == 5 ? 3 : 2;

	ret = regmap_read(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_ANA_ELR4], &hw_r_fg_sel);
	if (ret) {
		dev_info(dev, "%s, Failed to read %s R_FG selector\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	dev_info(dev, "%s, %s r_fg_sel:(rg:%d, dts:%d)\n",
		 __func__, priv->desc->gauge_name, sw_r_fg_sel, hw_r_fg_sel);

	/* !! Check Point !! */
	if (sw_sel_gain_err == efuse_gain_err && sw_r_fg_sel == hw_r_fg_sel) {
		dev_info(dev, "%s, %s no need to config FG gain err\n",
			 __func__, priv->desc->gauge_name);
		last_ret = 0;
		goto out_reload_gain_err;
	}

	/* Copy EFUSE gain err value to FG_GAIN_ERR */
	ret = regmap_raw_write(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_GAIN_CON0],
			       &efuse_gain_err, sizeof(efuse_gain_err));
	if (ret) {
		dev_info(dev, "%s, Failed to write %s FG EFUSE gain err to FGADC_GAIN_CON0\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	/* Set DTS r_fg to register */
	ret = regmap_write(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_ANA_ELR4], sw_r_fg_sel);
	if (ret) {
		dev_info(dev, "%s, Failed to set %s R_FG selector to %d(dts_r_fg:%d)\n",
			 __func__, priv->desc->gauge_name, sw_r_fg_sel, priv->dts_r_fg);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	dev_info(dev, "%s, %s FG ON = 0\n", __func__, priv->desc->gauge_name);
	ret = regmap_write_bits(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_CON1], FG_ON_MASK, 0);
	if (ret) {
		dev_info(dev, "%s, Failed to disable %s FG_ON\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	ret = regmap_write_bits(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2],
				FG_CHARGE_RST_MASK, FG_CHARGE_RST_MASK);
	if (ret) {
		dev_info(dev, "%s, Failed to set %s RESET_CAR bit\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	ret = regmap_write_bits(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2],
				FG_CHARGE_RST_MASK, 0);
	if (ret) {
		dev_info(dev, "%s, Failed to clear %s RESET_CAR bit\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

	udelay(200);

	dev_info(dev, "%s, %s FG ON = 1\n", __func__, priv->desc->gauge_name);
	ret = regmap_write_bits(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_CON1],
				FG_ON_MASK, FG_ON_MASK);
	if (ret) {
		dev_info(dev, "%s, Failed to disable %s FG_ON\n",
			 __func__, priv->desc->gauge_name);
		last_ret = ret;
		goto out_reload_gain_err;
	}

out_reload_gain_err:

	/* Disable TM */
	ret = mt6379_enable_tm(priv, false);
	if (ret) {
		dev_info(dev, "%s, Failed to disable test mode\n", __func__);
		return ret;
	}

	mdelay(33);

	dev_info(dev, "%s, %sreload %s FG gain err from EFUSE%s! (ret:%d)\n",
		 __func__, last_ret != 0 ? "Failed to " : "", priv->desc->gauge_name,
		 last_ret == 0 ? " successfully" : "", last_ret);
	return last_ret;
}

static int instant_current(struct mtk_gauge *gauge, int *val, enum fg_cic_idx cic_idx)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int dvalue = 0, r_fg_value = 0, car_tune_value = 0, vbat_p = 0;
	int ibat_p = 0, bat_exist = 0, ret = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int dist_reg = 0, rdata = 0, rdata2 = 0;
	struct device *dev = priv->dev;
	bool latch_timeout = false;
	u16 reg_value = 0;

	/* Check if battery exist or not */
	ret = mt6379_get_battery_state(priv, &bat_exist);
	if (ret || (ret == 0 && !bat_exist)) {
		*val = 0;
		dev_info(priv->dev, "%s, Failed to get %s battery status(%s, ret:%d)\n",
			 __func__, priv->desc->gauge_name,
			 ret == 0 ? "battery not exist!!" : "I/O Error", ret);
		return -EINVAL;
	}

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;

	bm_debug(priv->gauge.gm, "%s, read %s CIC%d\n", __func__, priv->desc->gauge_name, cic_idx + 1);

	switch (cic_idx) {
	case FG_CIC1:
		dist_reg = rg[bat_idx][MT6379_REG_FGADC_CUR_CON0];
		break;
	case FG_CIC2:
		dist_reg = rg[bat_idx][MT6379_REG_FGADC_CUR_CON3];
		break;
	case FG_CIC3:
		dist_reg = rg[bat_idx][MT6379_REG_FGADC_CUR_CON4];
		break;
	default:
		return -EINVAL;
	}

	ret = pre_gauge_update(gauge);
	if (ret == MT6379_LATCH_TIMEOUT)
		latch_timeout = true;
	else if (ret == -ETIMEDOUT) {
		aee_kernel_warning("SPMI", "\nCRDISPATCH_KEY:SPMI\nspmi timeout when %s pre_gauge_update",
				   priv->desc->gauge_name);
		dev_info(priv->dev, "%s, %s Latch SPMI timeout, retry_cnt:%d\n",
			 __func__, priv->desc->gauge_name, ++priv->latch_spmi_timeout_cnt);
		post_gauge_update(gauge);
		pre_gauge_update(gauge);
	}

	ret = regmap_raw_read(gauge->regmap, dist_reg, &reg_value, sizeof(reg_value));
	if (ret) {
		dev_info(dev, "%s, Failed to read %s CIC%d current (ret:%d)\n",
			 __func__, priv->desc->gauge_name, cic_idx + 1, ret);
		return ret;
	}

	post_gauge_update(gauge);

	dvalue = reg_to_current(gauge, reg_value);

	/* Auto adjust value */
	if (r_fg_value != priv->default_r_fg && r_fg_value != 0)
		dvalue = (dvalue * priv->default_r_fg) / r_fg_value;

	dvalue = ((dvalue * car_tune_value) / 1000);

	bm_debug(gauge->gm, "%s, %s, r_fg=%d, car_tune:%d, CIC%d current:%d\n",
		 __func__, priv->desc->gauge_name, r_fg_value, car_tune_value, cic_idx, dvalue);

	*val = dvalue;

	if (latch_timeout) {
		priv->latch_timeout_cnt++;
		dev_info(dev, "%s, Failed to read %s CIC%d with external 32k, latch_timeout_cnt:%d\n",
			 __func__, priv->desc->gauge_name, cic_idx + 1, priv->latch_timeout_cnt);

		ret = iio_read_channel_attribute(gauge->chan_ptim_bat_voltage,
						 &vbat_p, &ibat_p, IIO_CHAN_INFO_RAW);
		dev_notice(dev, "%s, %s ptim vbat=%d, ibat=%d, ret=%d (ret should be 1)\n",
			   __func__, priv->desc->gauge_name, vbat_p, ibat_p, ret);

		ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2], &rdata);
		if (ret)
			dev_info(dev, "%s, Failed to read %s FGADC_CON2\n",
				 __func__, priv->desc->gauge_name);

		ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON3], &rdata2);
		if (ret)
			dev_info(dev, "%s, Failed to read %s FGADC_CON3\n",
				 __func__, priv->desc->gauge_name);

		dev_notice(dev, "%s, %s BM[0x6F,70]=0x%x,0x%x\n",
			   __func__, priv->desc->gauge_name, rdata, rdata2);

		ret = regmap_update_bits(gauge->regmap, MT6379_REG_FGADC_CLK_CTRL, 0x10, 0x10);
		if (ret)
			dev_info(dev, "%s, Failed to switch %s to internal 32k\n",
				 __func__, priv->desc->gauge_name);

		pre_gauge_update(gauge);

		ret = regmap_raw_read(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_CUR_CON0],
				      &reg_value, sizeof(reg_value));
		if (ret)
			dev_info(dev, "%s, Failed to read %s CIC1\n",
				 __func__, priv->desc->gauge_name);

		post_gauge_update(gauge);
		ret = regmap_update_bits(priv->regmap, MT6379_REG_FGADC_CLK_CTRL, 0x10, 0x00);
		if (ret)
			dev_info(dev, "%s, Failed to switch %s to external 32k\n",
				 __func__, priv->desc->gauge_name);

		dvalue = reg_to_current(gauge, reg_value);
		dev_notice(dev, "%s, %s internal 32k CIC%d = %d, ret:%d\n",
			   __func__, priv->desc->gauge_name, cic_idx + 1, dvalue, ret);
	}

	return ret;
}

static int read_hw_ocv_6379_plug_in(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	signed int adc_rdy = 0, adc_result_reg = 0, adc_result = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int sel = 0;
	u16 regval = 0;
	int ret;

	ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR],
			      &regval, sizeof(regval));
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s plugin PCHR\n",
			 __func__, priv->desc->gauge_name);
		return ret;
	}

	adc_rdy = regval & AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_MASK;
	adc_result_reg = regval & AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK;

	ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_HK_TOP_STRUP_CON1], &sel);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s HK_TOP_STRUP_CON1\n",
			 __func__, priv->desc->gauge_name);
		return ret;
	}

	sel = (sel & HK_STRUP_AUXADC_START_SEL_MASK) >> HK_STRUP_AUXADC_START_SEL_SHIFT;
	adc_result = reg_to_mv_value(gauge, adc_result_reg);
	bm_err(gauge->gm, "[oam] %s (%s pchr): adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
	       __func__, priv->desc->gauge_name, adc_result_reg, adc_result, sel, adc_rdy);

	if (adc_rdy) {
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
				   AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK,
				   AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK);
		mdelay(1);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
				   AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK, 0);
	}

	return adc_result;
}

static int read_hw_ocv_6379_power_on(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	signed int adc_result_rdy = 0, adc_result_reg = 0, adc_result = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	int offset_trim = priv->offset_trim, ret;
	unsigned int sel = 0, data = 0;
	bool is_ship_rst;
	u16 regval = 0;

	ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_ADC_OUT_PWRON_PCHR],
			      &regval, sizeof(regval));
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s PWRON_PCHR\n",
			 __func__, priv->desc->gauge_name);
		return ret;
	}

	adc_result_rdy = regval & AUXADC_ADC_RDY_PWRON_PCHR_MASK;
	adc_result_reg = regval & AUXADC_ADC_OUT_PWRON_PCHR_MASK;

	regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_HK_TOP_STRUP_CON1], &sel);
	sel = (sel & HK_STRUP_AUXADC_START_SEL_MASK) >> HK_STRUP_AUXADC_START_SEL_SHIFT;

	regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_ZCV_CON0], &data);
	is_ship_rst = data & FGADC_ZCV_CON0_RSV ? true : false;
	if (is_ship_rst) {
		bm_err(gauge->gm, "%s: %s before cali, is_ship_rst:%d, offset_trim:0x%x, gain_err:0x%x, adc_result_reg:0x%x\n",
			__func__, priv->desc->gauge_name, is_ship_rst, offset_trim,
			priv->gain_err, adc_result_reg);
		adc_result_reg = adc_result_reg * (ADC_PRECISE + priv->gain_err) / ADC_PRECISE +
				 offset_trim;
		bm_err(gauge->gm, "%s: %s after cali, adc_result_reg:0x%x\n",
		       __func__, priv->desc->gauge_name, adc_result_reg);
	}

	adc_result = reg_to_mv_value(gauge, adc_result_reg);
	bm_err(gauge->gm, "[oam] %s (%s pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, priv->desc->gauge_name, adc_result_reg, adc_result, sel, adc_result_rdy);

	if (adc_result_rdy) {
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
				   AUXADC_ADC_RDY_PWRON_CLR_MASK,AUXADC_ADC_RDY_PWRON_CLR_MASK);
		mdelay(1);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
				   AUXADC_ADC_RDY_PWRON_CLR_MASK, 0);
	}

	return adc_result;
}

static int read_hw_ocv_6379_before_chgin(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	signed int adc_result_rdy = 0, adc_result_reg = 0, adc_result = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int sel = 0;
	u16 regval = 0;
	int ret;

	ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_ADC_OUT_WAKEUP_PCHR],
			      &regval, sizeof(regval));
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s WAKEUP_PCHR\n",
			 __func__, priv->desc->gauge_name);
		return ret;
	}

	adc_result_rdy = regval & AUXADC_ADC_RDY_WAKEUP_PCHR_MASK;
	adc_result_reg = regval & AUXADC_ADC_OUT_WAKEUP_PCHR_MASK;

	regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_HK_TOP_STRUP_CON1], &sel);
	sel = (sel & HK_STRUP_AUXADC_START_SEL_MASK) >> HK_STRUP_AUXADC_START_SEL_SHIFT;

	adc_result = reg_to_mv_value(gauge, adc_result_reg);
	bm_err(gauge->gm, "[oam] %s (%s pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, priv->desc->gauge_name, adc_result_reg, adc_result, sel, adc_result_rdy);

	if (adc_result_rdy) {
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
				   AUXADC_ADC_RDY_WAKEUP_CLR_MASK, AUXADC_ADC_RDY_WAKEUP_CLR_MASK);
		mdelay(1);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_CON42],
				   AUXADC_ADC_RDY_WAKEUP_CLR_MASK, 0);
	}

	return adc_result;
}

static int read_hw_ocv_6379_power_on_rdy(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int pon_rdy = 0, ret;
	u16 regval = 0;

	ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_ADC_OUT_PWRON_PCHR],
			      &regval, sizeof(regval));
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s PWRON_PCHR\n",
			 __func__, priv->desc->gauge_name);
		return ret;
	}

	pon_rdy = (regval & AUXADC_ADC_RDY_PWRON_PCHR_MASK) ? 1 : 0;

	bm_err(gauge->gm, "[%s] %s pwron_PCHR_rdy %d\n", __func__, priv->desc->gauge_name, pon_rdy);

	return pon_rdy;
}

static void switch_nafg_period(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 5)
		*value = 0;
	else if (_prd >= 5 && _prd < 10)
		*value = 1;
	else if (_prd >= 10 && _prd < 20)
		*value = 2;
	else if (_prd >= 20)
		*value = 3;
}

static void fgauge_set_nafg_interrupt_internal(struct mtk_gauge *gauge, int _prd, int _zcv_mv,
					       int _thr_mv)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u32 NAG_C_DLTV_Threashold;
	int period = 0;
	u16 regval = 0;

	gauge->zcv_reg = mv_to_reg_value(gauge, _zcv_mv);
	gauge->thr_reg = mv_to_reg_value(gauge, _thr_mv);

	if (gauge->thr_reg >= 32768) {
		bm_err(gauge->gm, "[%s] %s nag_c_dltv_thr mv=%d ,thr_reg=%d, limit thr_reg to 32767\n",
			__func__, priv->desc->gauge_name, _thr_mv, gauge->thr_reg);
		gauge->thr_reg = 32767;
	}

	regval = gauge->zcv_reg & AUXADC_NAG_ZCV_MASK;
	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_1],
			 &regval, sizeof(regval));

	NAG_C_DLTV_Threashold = gauge->thr_reg & AUXADC_NAG_C_DLTV_TH_MASK;

	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_3],
			 &NAG_C_DLTV_Threashold, sizeof(NAG_C_DLTV_Threashold));

	switch_nafg_period(_prd, &period);

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_0],
			   AUXADC_NAG_PRD_MASK, period << AUXADC_NAG_PRD_SHIFT);

	bm_debug(gauge->gm, "[fg_bat_nafg][%s] %s time[%d] zcv[%d:%d] thr[%d:%d] 26_0[0x%x]\n",
		 __func__, priv->desc->gauge_name, _prd, _zcv_mv, gauge->zcv_reg,
		 _thr_mv, gauge->thr_reg, NAG_C_DLTV_Threashold);
}

static int get_nafg_vbat(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int nag_vbat_mv, i = 0;
	unsigned int vbat_val;
	u16 nag_vbat_reg = 0;

	do {
		regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_ADC_OUT_NAG],
				&nag_vbat_reg, sizeof(nag_vbat_reg));

		if (nag_vbat_reg & BIT(15))
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & AUXADC_ADC_OUT_NAG_MASK;
	nag_vbat_mv = reg_to_mv_value(gauge, vbat_val);

	return nag_vbat_mv;
}

static void fgauge_set_zcv_intr_internal(struct mtk_gauge *gauge, int fg_zcv_det_time,
					 int fg_zcv_car_th)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	long long fg_zcv_car_th_reg = fg_zcv_car_th;
	u32 fg_zcv_car_th_regval;

	fg_zcv_car_th_reg = (fg_zcv_car_th_reg * 100 * 1000);

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_zcv_car_th_reg, priv->unit_fgcar_zcv);
#else
	fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg, priv->unit_fgcar_zcv);
#endif

	if (gauge->hw_status.r_fg_value != priv->default_r_fg)
#if defined(__LP64__) || defined(_LP64)
		fg_zcv_car_th_reg = (fg_zcv_car_th_reg *
				gauge->hw_status.r_fg_value) /
				priv->default_r_fg;
#else
		fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg *
				gauge->hw_status.r_fg_value,
				priv->default_r_fg);
#endif

#if defined(__LP64__) || defined(_LP64)
	fg_zcv_car_th_reg = ((fg_zcv_car_th_reg * 1000) /
			gauge->gm->fg_cust_data.car_tune_value);
#else
	fg_zcv_car_th_reg = div_s64((fg_zcv_car_th_reg * 1000),
			gauge->gm->fg_cust_data.car_tune_value);
#endif

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_ZCV_CON0],
			   FG_ZCV_DET_IV_MASK, fg_zcv_det_time << FG_ZCV_DET_IV_SHIFT);

	fg_zcv_car_th_regval = fg_zcv_car_th_reg & FG_ZCV_CAR_TH_MASK;
	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_ZCVTH_CON0],
			 &fg_zcv_car_th_regval, sizeof(fg_zcv_car_th_regval));

	bm_debug(gauge->gm, "[FG_ZCV_INT][%s] %s det_time %d mv %d reg %lld 30_00 0x%x\n",
		 __func__, priv->desc->gauge_name, fg_zcv_det_time, fg_zcv_car_th, fg_zcv_car_th_reg,
		 fg_zcv_car_th_regval);
}

static void read_fg_hw_info_ncar(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	unsigned int uvalue32_NCAR = 0, uvalue32_NCAR_MSB = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	signed int dvalue_NCAR = 0;
	long long Temp_Value = 0;
	u32 temp_NCAR = 0;

	pre_gauge_update(gauge);
	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_NCAR_CON0],
			&temp_NCAR, sizeof(temp_NCAR));
	post_gauge_update(gauge);

	uvalue32_NCAR = temp_NCAR & ~BIT(31);
	uvalue32_NCAR_MSB = temp_NCAR & BIT(31);

	/*calculate the real world data    */
	dvalue_NCAR = (signed int)uvalue32_NCAR;
	if (uvalue32_NCAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_NCAR_MSB) {
		/* dis-charging */
		Temp_Value = (long long)(dvalue_NCAR - 0x7fffffff);
		/* keep negative value */
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		/*charging */
		Temp_Value = (long long)dvalue_NCAR;
	}


	/* 0.1 mAh */
#if defined(__LP64__) || defined(_LP64)
	Temp_Value = Temp_Value * priv->unit_charge / 1000;
#else
	Temp_Value = div_s64(Temp_Value * priv->unit_charge, 1000);
#endif

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);
#else
	Temp_Value = div_s64(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	Temp_Value = div_s64(Temp_Value, 10);
#endif

	if (uvalue32_NCAR_MSB)
		dvalue_NCAR = (signed int)(Temp_Value - (Temp_Value * 2));
	else
		dvalue_NCAR = (signed int)Temp_Value;

	/*Auto adjust value*/
	if (gauge->hw_status.r_fg_value != priv->default_r_fg)
		dvalue_NCAR = (dvalue_NCAR * priv->default_r_fg) / gauge->hw_status.r_fg_value;

	gauge->fg_hw_info.ncar = ((dvalue_NCAR * gauge->gm->fg_cust_data.car_tune_value) / 1000);
}

static int fgauge_get_time(struct mtk_gauge *gauge, unsigned int *ptime)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int ret_time;
	u32 time_regval = 0;
	long long time = 0;

	pre_gauge_update(gauge);

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_NTER_CON0],
			&time_regval, sizeof(time_regval));

	post_gauge_update(gauge);

	time = time_regval & FGADC_NTER_MASK;

#if defined(__LP64__) || defined(_LP64)
	time = time * UNIT_TIME / 100;
#else
	time = div_s64(time * UNIT_TIME, 100);
#endif
	ret_time = time;

	bm_debug(gauge->gm, "[%s] %s regval:0x%x rtime:0x%llx 0x%x!\r\n",
		 __func__, priv->desc->gauge_name, time_regval, time, ret_time);


	*ptime = ret_time;

	return 0;
}

static int nafg_check_corner(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int nag_vbat = 0, setto_cdltv_thr_mv = 0, get_c_dltv_mv = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	int nag_zcv = gauge->nafg_zcv_mv;
	signed int nag_c_dltv_reg_value;
	u32 nag_c_dltv_value = 0;
	bool bcheckbit10;

	setto_cdltv_thr_mv = gauge->nafg_c_dltv_mv;

	/*AUXADC_NAG_7*/
	regmap_raw_read(gauge->regmap,rg[bat_idx][MT6379_REG_AUXADC_NAG_13],
			&nag_c_dltv_value, sizeof(nag_c_dltv_value));
	nag_c_dltv_value &= AUXADC_NAG_C_DLTV_MASK;

	bcheckbit10 = nag_c_dltv_value & BIT(26);
	nag_c_dltv_reg_value = nag_c_dltv_value;

	if (bcheckbit10)
		nag_c_dltv_reg_value |= 0xF8000000;

	get_c_dltv_mv = reg_to_mv_value(gauge, nag_c_dltv_reg_value);
	nag_vbat = get_nafg_vbat(gauge);

	bm_debug(gauge->gm, "%s, %s nag_vbat:%d nag_zcv:%d get_c_dltv_mv:%d setto_cdltv_thr_mv:%d, RG[0x%x]\n",
		 __func__, priv->desc->gauge_name, nag_vbat, nag_zcv, get_c_dltv_mv,
		 setto_cdltv_thr_mv, nag_c_dltv_value);

	return 0;
}

static int coulomb_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	unsigned int uvalue32_car = 0, uvalue32_car_msb = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	int r_fg_value, car_tune_value, ret = 0;
	signed int dvalue_CAR = 0;
	long long temp_value = 0;
	u32 temp_car = 0;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	pre_gauge_update(gauge);

	ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CAR_CON0],
			      &temp_car, sizeof(temp_car));
	if (ret) {
		dev_info(priv->dev, "%s, Failed to read %s FGADC CAR (ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	post_gauge_update(gauge);

	uvalue32_car = temp_car & 0x7fffffff;
	uvalue32_car_msb = (temp_car & BIT(31)) >> 31;

	/* calculate the real world data */
	dvalue_CAR = (signed int) uvalue32_car;

	if (uvalue32_car == 0) {
		temp_value = 0;
	} else if (uvalue32_car_msb) {
		/* dis-charging */
		temp_value = (long long) (dvalue_CAR - 0x7fffffff);
		/* keep negative value */
		temp_value = temp_value - (temp_value * 2);
	} else {
		/*charging */
		temp_value = (long long) dvalue_CAR;
	}

#if defined(__LP64__) || defined(_LP64)
	temp_value = temp_value * priv->unit_charge / 1000;
#else
	temp_value = div_s64(temp_value * priv->unit_charge, 1000);
#endif


#if defined(__LP64__) || defined(_LP64)
	do_div(temp_value, 10);
	temp_value = temp_value + 5;
	do_div(temp_value, 10);
#else
	temp_value = div_s64(temp_value, 10);
	temp_value = temp_value + 5;
	temp_value = div_s64(temp_value, 10);
#endif


	if (uvalue32_car_msb)
		dvalue_CAR = (signed int) (temp_value - (temp_value * 2));
		/* keep negative value */
	else
		dvalue_CAR = (signed int) temp_value;

	bm_trace(gauge->gm, "[%s] %s l:0x%x h:0x%x val:%d msb:%d car:%d\n",
		 __func__, priv->desc->gauge_name, temp_car & 0xFFFF, (temp_car & 0xFFFF0000) >> 16,
		 uvalue32_car, uvalue32_car_msb, dvalue_CAR);

	/* Auto adjust value */
	if (r_fg_value != priv->default_r_fg && r_fg_value != 0) {
		bm_trace(gauge->gm, "[%s] %s Auto adjust value due to the Rfg is %d\n Ori CAR=%d",
			 __func__, priv->desc->gauge_name, r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * priv->default_r_fg) / r_fg_value;

		bm_trace(gauge->gm, "[%s] %s new CAR=%d\n", __func__, priv->desc->gauge_name, dvalue_CAR);
	}

	dvalue_CAR = (dvalue_CAR * car_tune_value) / 1000;

	bm_trace(gauge->gm, "[%s] %s CAR=%d r_fg_value=%d car_tune_value=%d\n",
		__func__, priv->desc->gauge_name, dvalue_CAR, r_fg_value, car_tune_value);

	*val = dvalue_CAR;

	return ret;
}

static int average_current_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			       int *data)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int sign_bit = 0, iavg_vld = 0, ret = 0, r_fg_value, car_tune_value;
	long long fg_iavg_reg = 0, fg_iavg_reg_tmp = 0, fg_iavg_ma = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 fg_iavg_reg_27_16 = 0, fg_iavg_reg_15_00 = 0;
	struct device *dev = priv->dev;
	bool is_bat_charging = false;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;

	pre_gauge_update(gauge);

	ret = regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_IAVG_CON1], &iavg_vld);
	if (ret) {
		dev_info(dev, "%s, Failed to read %s FGADC_IAVG_CON1(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	post_gauge_update(gauge);

	iavg_vld = iavg_vld & FG_IAVG_VLD_MASK;

	if (iavg_vld) {
		ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_IAVG_CON2],
				      &fg_iavg_reg_27_16, sizeof(fg_iavg_reg_27_16));
		if (ret) {
			dev_info(dev, "%s, Failed to read %s FGADC_IAVG_CON2(iavg27_16)(ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		fg_iavg_reg_27_16 &= FG_IAVG_27_16_MASK;

		ret = regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_IAVG_CON0],
				      &fg_iavg_reg_15_00, sizeof(fg_iavg_reg_15_00));
		if (ret) {
			dev_info(dev, "%s, Failed to read %s FGADC_IAVG_CON0(iavg15_00)(ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		fg_iavg_reg_15_00 &= FG_IAVG_15_00_MASK;

		fg_iavg_reg = fg_iavg_reg_27_16;
		fg_iavg_reg = ((long long)fg_iavg_reg << 16) + fg_iavg_reg_15_00;

		sign_bit = (fg_iavg_reg_27_16 & BIT(11)) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			/*fg_iavg_reg = fg_iavg_reg_tmp - 0xfffffff - 1;*/
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		is_bat_charging = sign_bit ? false : true;

		fg_iavg_ma = fg_iavg_reg * priv->unit_fg_iavg * car_tune_value;

		bm_debug(gauge->gm, "[fg_get_current_iavg] %s fg_iavg_ma:%lld, fg_iavg_reg:%lld, fg_iavg_reg_tmp:%lld\n",
			 priv->desc->gauge_name, fg_iavg_ma, fg_iavg_reg, fg_iavg_reg_tmp);

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
#endif


		if (r_fg_value != priv->default_r_fg && r_fg_value != 0) {
#if defined(__LP64__) || defined(_LP64)
			fg_iavg_ma = (fg_iavg_ma * priv->default_r_fg / r_fg_value);
#else
			fg_iavg_ma = div_s64(fg_iavg_ma * priv->default_r_fg, r_fg_value);
#endif
		}

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 100);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 100);
#endif

		bm_debug(gauge->gm, "[fg_get_current_iavg] %s fg_iavg_ma %lld\n",
			 priv->desc->gauge_name, fg_iavg_ma);

		if (sign_bit)
			fg_iavg_ma = 0 - fg_iavg_ma;

		bm_debug(gauge->gm, "[fg_get_current_iavg] %s fg_iavg_ma:%lld, fg_iavg_reg:%lld, r_fg_value:%d, 27_16:0x%x, 15_00:0x%x\n",
			 priv->desc->gauge_name, fg_iavg_ma, fg_iavg_reg,
			 r_fg_value, fg_iavg_reg_27_16, fg_iavg_reg_15_00);

		gauge->fg_hw_info.current_avg = fg_iavg_ma;
		gauge->fg_hw_info.current_avg_sign = sign_bit;
		bm_debug(gauge->gm, "[fg_get_current_iavg] %s PMIC_FG_IAVG_VLD == 1\n", priv->desc->gauge_name);
	} else {
		ret = instant_current(gauge, &gauge->fg_hw_info.current_1, FG_CIC1);
		if (ret) {
			dev_info(dev, "%s, Failed to get %s CIC1(ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		gauge->fg_hw_info.current_avg = gauge->fg_hw_info.current_1;

		if (gauge->fg_hw_info.current_1 < 0)
			gauge->fg_hw_info.current_avg_sign = 1;

		bm_debug(gauge->gm, "[fg_get_current_iavg] %s PMIC_FG_IAVG_VLD != 1, avg %d, current_1 %d\n",
			 priv->desc->gauge_name, gauge->fg_hw_info.current_avg, gauge->fg_hw_info.current_1);
	}

	*data = gauge->fg_hw_info.current_avg;

	gauge->fg_hw_info.current_avg_valid = iavg_vld;
	bm_debug(gauge->gm,
		 "[fg_get_current_iavg] %s current_avg:%d valid:%d is_bat_charging:%d\n",
		 priv->desc->gauge_name, *data, iavg_vld, is_bat_charging);

	return 0;
}

static int bat_temp_froze_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				 int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	regmap_update_bits(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_1],
			   AUXADC_BAT_TEMP_FROZE_EN_MASK,
			   val ? AUXADC_BAT_TEMP_FROZE_EN_MASK : 0);
	return 0;
}

static int bat_tmp_lt_threshold_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int tmp_int_lt = mv_to_reg_12_temp_value(gauge, threshold);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 regval;

	regval = tmp_int_lt & AUXADC_BAT_TEMP_VOLT_MAX_MASK;
	/* max is low temp */
	regmap_raw_write(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_4],
			 &regval, sizeof(regval));

	bm_debug(gauge->gm, "[%s] %s mv:%d reg:%d\n", __func__, priv->desc->gauge_name, threshold, tmp_int_lt);
	return 0;
}

static int bat_tmp_ht_threshold_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int tmp_int_ht = mv_to_reg_12_temp_value(gauge, threshold);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 regval;

	regval = tmp_int_ht & AUXADC_BAT_TEMP_VOLT_MIN_MASK;
	/* min is high temp */
	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_7],
			 &regval, sizeof(regval));

	bm_debug(gauge->gm, "[%s] %s mv:%d reg:%d\n", __func__, priv->desc->gauge_name, threshold, tmp_int_ht);

	return 0;
}

static int en_bat_tmp_lt_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			     int en)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	if (en == 0) {
		disable_gauge_irq(gauge, BAT_TMP_L_IRQ);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_3],
				   AUXADC_BAT_TEMP_DET_MAX_MASK | AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK,
				   0);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_0],
				   AUXADC_BAT_TEMP_EN_MASK, 0);
	} else {
		/* unit: 0x10 = 2, means 5 second */
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_2],
				   AUXADC_BAT_TEMP_DET_PRD_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DET_PRD_SEL_SHIFT);

		/* debounce 0x10 = 2 , means 4 times*/
		/* 5s * 4 times = 20s to issue bat_temp  CIC1 output interrupt */
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_2],
				   AUXADC_BAT_TEMP_DEBT_MAX_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DEBT_MAX_SEL_SHIFT);

		enable_gauge_irq(gauge, BAT_TMP_L_IRQ);

		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_3],
				   AUXADC_BAT_TEMP_DET_MAX_MASK | AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK,
				   AUXADC_BAT_TEMP_DET_MAX_MASK | AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_0],
				   AUXADC_BAT_TEMP_EN_MASK, AUXADC_BAT_TEMP_EN_MASK);
	}

	bm_debug(gauge->gm, "[%s] %s en:%d\n", __func__, priv->desc->gauge_name, en);

	return 0;
}

static int en_bat_tmp_ht_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			     int en)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	if (en == 0) {
		disable_gauge_irq(gauge, BAT_TMP_H_IRQ);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_6],
				   AUXADC_BAT_TEMP_DET_MIN_MASK | AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK,
				   0);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_0],
				   AUXADC_BAT_TEMP_EN_MASK, 0);
	} else {
		/* unit: 0x10 = 2, means 5 second */
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_2],
				   AUXADC_BAT_TEMP_DET_PRD_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DET_PRD_SEL_SHIFT);

		/* debounce 0x10 = 2 , means 4 times*/
		/* 5s * 4 times = 20s to issue bat_temp interrupt */
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_2],
				   AUXADC_BAT_TEMP_DEBT_MIN_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DEBT_MIN_SEL_SHIFT);

		enable_gauge_irq(gauge, BAT_TMP_H_IRQ);

		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_6],
				   AUXADC_BAT_TEMP_DET_MIN_MASK | AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK,
				   AUXADC_BAT_TEMP_DET_MIN_MASK | AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK);
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_BAT_TEMP_0],
				   AUXADC_BAT_TEMP_EN_MASK, AUXADC_BAT_TEMP_EN_MASK);
	}

	bm_debug(gauge->gm, "[%s] %s en:%d\n", __func__, priv->desc->gauge_name, en);

	return 0;
}

static int event_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int event)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);

	switch(event) {
	case EVT_INT_NAFG_CHECK:
		return nafg_check_corner(gauge);
	case EVT_INT_BAT_PLUGIN:
		return mt6379_reload_fg_efuse_gain_err(priv);
	default:
		return 0;
	}
}

static signed int fg_set_iavg_intr(struct mtk_gauge *gauge_dev, void *data)
{
	struct mt6379_priv *priv = container_of(gauge_dev, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int iavg_gap = *(unsigned int *) (data);
	int iavg;
	long long iavg_ht, iavg_lt;
	long long fg_iavg_reg_ht, fg_iavg_reg_lt;
	int fg_iavg_lth_28_16, fg_iavg_lth_15_00;
	int fg_iavg_hth_28_16, fg_iavg_hth_15_00;
	u32 regval;
	int ret = 0;

	ret = average_current_get(gauge_dev, NULL, &iavg);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to get %s IAVG(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	iavg_ht = abs(iavg) + iavg_gap;
	iavg_lt = abs(iavg) - iavg_gap;

	if (iavg_lt <= 0)
		iavg_lt = 0;

	gauge_dev->hw_status.iavg_ht = iavg_ht;
	gauge_dev->hw_status.iavg_lt = iavg_lt;

/* reverse for IAVG */
/* fg_iavg_ma * 100 * fg_cust_data.r_fg_value / DEFAULT_RFG * 1000 * 1000 */
/* / fg_cust_data.car_tune_value / priv->unit_fg_iavg  = fg_iavg_reg  */

	fg_iavg_reg_ht = iavg_ht * 100;
	if ((gauge_dev->hw_status.r_fg_value != priv->default_r_fg) &&
	    priv->default_r_fg != 0) {
		fg_iavg_reg_ht = fg_iavg_reg_ht * gauge_dev->hw_status.r_fg_value;
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_ht, priv->default_r_fg);
#else
		fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, priv->default_r_fg);
#endif
	}

	fg_iavg_reg_ht = fg_iavg_reg_ht * 1000000;

	if (priv->unit_fg_iavg != 0 && gauge_dev->gm->fg_cust_data.car_tune_value != 0) {
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_ht, priv->unit_fg_iavg);
		do_div(fg_iavg_reg_ht, gauge_dev->gm->fg_cust_data.car_tune_value);
#else
		fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, priv->unit_fg_iavg);
		fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht,
					 gauge_dev->gm->fg_cust_data.car_tune_value);
#endif
	}


	fg_iavg_reg_lt = iavg_lt * 100;

	if ((gauge_dev->hw_status.r_fg_value != priv->default_r_fg) &&
	    priv->default_r_fg != 0) {
		fg_iavg_reg_lt = fg_iavg_reg_lt *
			gauge_dev->hw_status.r_fg_value;
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_lt, priv->default_r_fg);
#else
		fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, priv->default_r_fg);
#endif
	}

	fg_iavg_reg_lt = fg_iavg_reg_lt * 1000000;

	if (priv->unit_fg_iavg != 0 && gauge_dev->gm->fg_cust_data.car_tune_value != 0) {
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_lt, priv->unit_fg_iavg);
		do_div(fg_iavg_reg_lt, gauge_dev->gm->fg_cust_data.car_tune_value);
#else
		fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, priv->unit_fg_iavg);
		fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt,
					gauge_dev->gm->fg_cust_data.car_tune_value);
#endif
	}

	fg_iavg_lth_28_16 = (fg_iavg_reg_lt & 0x1fff0000) >> 16;
	fg_iavg_lth_15_00 = fg_iavg_reg_lt & 0xffff;
	fg_iavg_hth_28_16 = (fg_iavg_reg_ht & 0x1fff0000) >> 16;
	fg_iavg_hth_15_00 = fg_iavg_reg_ht & 0xffff;

	disable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
	disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);

	regval = fg_iavg_lth_28_16 << 16 | fg_iavg_lth_15_00;
	regmap_raw_write(gauge_dev->regmap, rg[bat_idx][MT6379_REG_FGADC_IAVG_CON3],
			 &regval, sizeof(regval));

	regval = fg_iavg_hth_28_16 << 16 | fg_iavg_hth_15_00;
	regmap_raw_write(gauge_dev->regmap, rg[bat_idx][MT6379_REG_FGADC_IAVG_CON5],
			 &regval, sizeof(regval));

	enable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
	if (iavg_lt > 0)
		enable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);
	else
		disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);

	bm_debug(gauge_dev->gm, "[FG_IAVG_INT][%s] %s iavg %d iavg_gap %d iavg_ht %lld iavg_lt %lld fg_iavg_reg_ht %lld fg_iavg_reg_lt %lld\n",
		 __func__, priv->desc->gauge_name, iavg, iavg_gap, iavg_ht, iavg_lt,
		 fg_iavg_reg_ht, fg_iavg_reg_lt);

	bm_debug(gauge_dev->gm, "[FG_IAVG_INT][%s] %slt_28_16 0x%x lt_15_00 0x%x ht_28_16 0x%x ht_15_00 0x%x\n",
		 __func__, priv->desc->gauge_name, fg_iavg_lth_28_16, fg_iavg_lth_15_00,
		 fg_iavg_hth_28_16, fg_iavg_hth_15_00);

	return 0;
}

static int hw_info_set(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr, int en)
{
	struct mt6379_priv *priv = container_of(gauge_dev, struct mt6379_priv, gauge);
	int ret = 0, is_iavg_valid, avg_current, iavg_th;
	struct gauge_hw_status *gauge_status;
	struct device *dev = priv->dev;
	unsigned int time;

	gauge_status = &gauge_dev->hw_status;

	/* Current_1 */
	ret = instant_current(gauge_dev, &gauge_dev->fg_hw_info.current_1, FG_CIC1);
	if (ret) {
		dev_info(dev, "%s, Failed to get %s CIC1(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	/* Current_2 */
	ret = instant_current(gauge_dev, &gauge_dev->fg_hw_info.current_2, FG_CIC2);
	if (ret) {
		dev_info(dev, "%s, Failed to get %s CIC2(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	/* curr_out = pmic_get_register_value(PMIC_FG_CURRENT_OUT); */
	/* fg_offset = pmic_get_register_value(PMIC_FG_OFFSET); */

	/* Iavg */
	ret = average_current_get(gauge_dev, NULL, &avg_current);
	if (ret) {
		dev_info(dev, "%s, Failed to get %s IAVG(ret:%d)\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	is_iavg_valid = gauge_dev->fg_hw_info.current_avg_valid;
	if ((is_iavg_valid == 1) && (gauge_status->iavg_intr_flag == 0)) {

		bm_debug(gauge_dev->gm, "[read_fg_hw_info] %s set first fg_set_iavg_intr:%d, %d\n",
			 priv->desc->gauge_name, is_iavg_valid, gauge_status->iavg_intr_flag);

		gauge_status->iavg_intr_flag = 1;
		iavg_th = get_iavg_gap(gauge_dev->gm);
		ret = fg_set_iavg_intr(gauge_dev, &iavg_th);
	} else if (is_iavg_valid == 0) {
		gauge_status->iavg_intr_flag = 0;
		disable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
		disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);
		bm_debug(gauge_dev->gm, "[read_fg_hw_info] %s double check first fg_set_iavg_intr:%d, %d\n",
			 priv->desc->gauge_name, is_iavg_valid, gauge_status->iavg_intr_flag);
	}

	bm_debug(gauge_dev->gm, "[read_fg_hw_info] %s third check first fg_set_iavg_intr:%d, %d\n",
		 priv->desc->gauge_name, is_iavg_valid, gauge_status->iavg_intr_flag);

	/* Ncar */
	read_fg_hw_info_ncar(gauge_dev);

	coulomb_get(gauge_dev, NULL, &gauge_dev->fg_hw_info.car);
	fgauge_get_time(gauge_dev, &time);
	gauge_dev->fg_hw_info.time = time;

	bm_debug(gauge_dev->gm, "[FGADC_intr_end][read_fg_hw_info] %s curr_1:%d curr_2:%d Iavg:%d sign:%d car:%d ncar:%d time:%d\n",
		 priv->desc->gauge_name, gauge_dev->fg_hw_info.current_1, gauge_dev->fg_hw_info.current_2,
		 gauge_dev->fg_hw_info.current_avg, gauge_dev->fg_hw_info.current_avg_sign,
		 gauge_dev->fg_hw_info.car, gauge_dev->fg_hw_info.ncar, gauge_dev->fg_hw_info.time);

	return 0;
}

static int bat_cycle_intr_threshold_set(struct mtk_gauge *gauge,
					struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	long long car = threshold;
	long long carReg;
	u32 regval;

	disable_gauge_irq(gauge, FG_N_CHARGE_L_IRQ);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / priv->unit_charge;
	/* 1000 * 100 */
#else
	car = div_s64(car * 100000, priv->unit_charge);
#endif

	if (gauge->hw_status.r_fg_value != priv->default_r_fg) {
		car = (car * gauge->hw_status.r_fg_value);
#if defined(__LP64__) || defined(_LP64)
		do_div(car, priv->default_r_fg);
#else
		car = div_s64(car, priv->default_r_fg);
#endif
	}

	car = car * 1000;
#if defined(__LP64__) || defined(_LP64)
	do_div(car, gauge->gm->fg_cust_data.car_tune_value);
#else
	car = div_s64(car, gauge->gm->fg_cust_data.car_tune_value);
#endif

	carReg = car;
	carReg = 0 - carReg;

	regval = carReg & FG_N_CHARGE_TH_MASK;
	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_NCAR_CON2],
			 &regval, sizeof(regval));


	bm_err(gauge->gm, "%s car:%d carR:%lld r:%lld\n", priv->desc->gauge_name, threshold, car, carReg);
	enable_gauge_irq(gauge, FG_N_CHARGE_L_IRQ);

	return 0;

}

static int ncar_reset_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2],
			   FG_N_CHARGE_RST_MASK, FG_N_CHARGE_RST_MASK);
	udelay(200);
	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2],
			   FG_N_CHARGE_RST_MASK, 0);

	return 0;
}

static int soff_reset_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int en)
{
	return 0;
}

static int zcv_intr_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int en)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int ret, *cnt = &(priv->fg_info.zcv_intr_en_cnt);

	bm_debug(gauge->gm, "%s, %s zcv_intr_en_cnt:%d en:%d\n",
		 __func__, priv->desc->gauge_name, *cnt, en);

	*cnt = en ? *cnt + 1 : *cnt - 1;

	if (en) {
		enable_gauge_irq(gauge, ZCV_IRQ);
		ret = regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON0],
					 FG_ZCV_DET_EN_MASK, FG_ZCV_DET_EN_MASK);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to enable zcv_det\n", __func__);
			return ret;
		}

		ret = regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_BM_TOP_INT_CON0],
					 RG_INT_EN_FG_ZCV_MASK, RG_INT_EN_FG_ZCV_MASK);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to enable fg_zcv irq\n", __func__);
			return ret;
		}
	} else {
		disable_gauge_irq(gauge, ZCV_IRQ);
		ret = regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_BM_TOP_INT_CON0],
					 RG_INT_EN_FG_ZCV_MASK, 0);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to disable fg_zcv irq\n", __func__);
			return ret;
		}
	}

	bm_debug(gauge->gm, "%s, [FG_ZCV_INT][fg_set_zcv_intr_en] %s en:%d\n",
		 __func__, priv->desc->gauge_name, en);
	return 0;
}

static int zcv_intr_threshold_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				  int zcv_avg_current)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int fg_zcv_det_time;
	int fg_zcv_car_th = 0;

	fg_zcv_det_time = gauge->gm->fg_cust_data.zcv_suspend_time;
	fg_zcv_car_th = (fg_zcv_det_time + 1) * 4 * zcv_avg_current / 60;

	bm_debug(gauge->gm, "[%s] %s current:%d, fg_zcv_det_time:%d, fg_zcv_car_th:%d\n",
		 __func__, priv->desc->gauge_name, zcv_avg_current, fg_zcv_det_time, fg_zcv_car_th);

	fgauge_set_zcv_intr_internal(gauge, fg_zcv_det_time, fg_zcv_car_th);

	return 0;
}

static int bat_plugout_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			      int val)
{
	if (!!val)
		enable_gauge_irq(gauge, BAT_PLUGOUT_IRQ);
	else
		disable_gauge_irq(gauge, BAT_PLUGOUT_IRQ);

	return 0;
}

static int gauge_initialized_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				 int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int fg_reset_status = 0;

	regmap_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_RST_CON0], &fg_reset_status);
	*val = fg_reset_status & FG_RSTB_STATUS_MASK;

	return 0;
}

static int gauge_initialized_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				 int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_RST_CON0],
			   FG_RSTB_STATUS_MASK, val ? FG_RSTB_STATUS_MASK : 0);

	return 0;
}

static int reset_fg_rtc_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			    int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	u8 spare0_reg, after_rst_spare0_reg, spare3_reg, after_rst_spare3_reg;
	int temp_value;

	if (!gauge->gm) {
		dev_info(priv->dev, "%s, gm has not initialized\n", __func__);
		return 0;
	}

	/* read spare0 */
	spare0_reg = get_rtc_spare0_fg_value(gauge);

	/* raise 15b to reset */
	temp_value = 0x80;
	set_rtc_spare0_fg_value(gauge, temp_value);
	mdelay(1);
	temp_value = 0x20;
	set_rtc_spare0_fg_value(gauge, temp_value);

	/* read spare0 again */
	after_rst_spare0_reg = get_rtc_spare0_fg_value(gauge);

	/* read spare3 */
	spare3_reg = get_rtc_spare_fg_value(gauge);

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(gauge, spare3_reg | 0x80);

	/* read spare3 again */
	after_rst_spare3_reg = get_rtc_spare_fg_value(gauge);

	bm_err(gauge->gm, "[fgauge_read_RTC_boot_status]%s, %s spare0 0x%x 0x%x, spare3 0x%x 0x%x\n",
	       __func__, priv->desc->gauge_name, spare0_reg, after_rst_spare0_reg, spare3_reg, after_rst_spare3_reg);

	if (((after_rst_spare3_reg != (spare3_reg | 0x80)) || (after_rst_spare0_reg != temp_value))
	    && gauge->gm->disableGM30 == 0) {
		after_rst_spare0_reg = get_rtc_spare0_fg_value(gauge);
		after_rst_spare3_reg = get_rtc_spare_fg_value(gauge);

		bm_err(gauge->gm, "[%s][retry] %s spare0 0x%x 0x%x, spare3 0x%x 0x%x\n",
		       __func__, priv->desc->gauge_name, spare0_reg, after_rst_spare0_reg,
		       spare3_reg, after_rst_spare3_reg);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		if ((after_rst_spare3_reg != (spare3_reg | 0x80)) ||
		    (after_rst_spare0_reg != temp_value))
			aee_kernel_warning("BATTERY_RTC",
					   "RTC: Failed to write RG_SPARE, contact the RTC owner!");
#endif
	}
	return 0;
}

static int nafg_vbat_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			 int *vbat)
{
	*vbat = get_nafg_vbat(gauge);
	return 0;
}

static int nafg_zcv_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int zcv)
{
	gauge->nafg_zcv_mv = zcv;	/* 0.1 mv*/
	return 0;
}

static int nafg_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int *cnt = &(priv->fg_info.nafg_en_cnt);


	bm_debug(gauge->gm, "%s, %s nafg_en_cnt:%d, en:%d\n", __func__, priv->desc->gauge_name, *cnt, val);

	*cnt = val ? *cnt + 1 : *cnt - 1;

	if (val) {
		enable_gauge_irq(gauge, NAFG_IRQ);
		bm_debug(gauge->gm, "[%s] %s enable:%d\n", __func__, priv->desc->gauge_name, val);
	} else {
		disable_gauge_irq(gauge, NAFG_IRQ);
		bm_debug(gauge->gm, "[%s] %s disable:%d\n", __func__, priv->desc->gauge_name, val);
	}

	gauge->hw_status.nafg_en = val;
	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_0],
			   AUXADC_NAG_IRQ_EN_MASK | AUXADC_NAG_EN_MASK,
			   val ? (AUXADC_NAG_IRQ_EN_MASK | AUXADC_NAG_EN_MASK) : 0);

	return 0;
}

static int nafg_c_dltv_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *nafg_c_dltv)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	signed int nag_c_dltv_value;
	signed int nag_c_dltv_value_h;
	signed int nag_c_dltv_reg_value;
	signed int nag_c_dltv_mv_value;
	bool bcheckbit10;
	u32 nag_c_dltv_regval = 0;

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_13],
			&nag_c_dltv_regval, sizeof(nag_c_dltv_regval));
	nag_c_dltv_regval &= AUXADC_NAG_C_DLTV_MASK;

	nag_c_dltv_value = nag_c_dltv_regval & 0xffff;
	nag_c_dltv_value_h = nag_c_dltv_regval >> 16;

	bcheckbit10 = nag_c_dltv_value_h & 0x0400;

	if (bcheckbit10 == 0)
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0xffff) +
				((nag_c_dltv_value_h & 0x07ff) << 16);
	else
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0xffff) +
			(((nag_c_dltv_value_h | 0xf800) & 0xffff) << 16);

	nag_c_dltv_mv_value = reg_to_mv_value(gauge, nag_c_dltv_reg_value);
	*nafg_c_dltv = nag_c_dltv_mv_value;

	bm_debug(gauge->gm, "[fg_bat_nafg][%s] %s mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)]\n",
		 __func__, priv->desc->gauge_name, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
		 bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value);

	return 0;
}

static int nafg_c_dltv_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int c_dltv_mv)
{
	gauge->nafg_c_dltv_mv = c_dltv_mv;	/* 0.1 mv*/
	fgauge_set_nafg_interrupt_internal(gauge, gauge->gm->fg_cust_data.nafg_time_setting,
					   gauge->nafg_zcv_mv, gauge->nafg_c_dltv_mv);

	return 0;
}

static int nafg_dltv_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			 int *nag_dltv)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 nag_dltv_reg_value = 0;
	signed int nag_dltv_mv_value;
	short reg_value;

	/*AUXADC_NAG_4*/
	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_11],
			&nag_dltv_reg_value, sizeof(nag_dltv_reg_value));

	reg_value = nag_dltv_reg_value & 0xffff;

	nag_dltv_mv_value = reg_to_mv_value(gauge, nag_dltv_reg_value);
	*nag_dltv = reg_to_mv_value(gauge, reg_value);

	bm_debug(gauge->gm, "[fg_bat_nafg][%s] %s mV:Reg [%d:%d] [%d:%d]\n",
		 __func__, priv->desc->gauge_name, nag_dltv_mv_value,
		 nag_dltv_reg_value, reg_to_mv_value(gauge, reg_value), reg_value);

	return 0;
}

static int nafg_cnt_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			int *nag_cnt)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u32 NAG_C_DLTV_CNT = 0;

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_7],
			&NAG_C_DLTV_CNT, sizeof(NAG_C_DLTV_CNT));
	*nag_cnt = NAG_C_DLTV_CNT & AUXADC_NAG_CNT_MASK;
	bm_debug(gauge->gm, "[fg_bat_nafg][%s] %s %d [25_0 %d]\n",
		 __func__, priv->desc->gauge_name, *nag_cnt, NAG_C_DLTV_CNT);

	mt6379_gauge_dump_registers(priv);

	return 0;
}

static int zcv_current_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *zcv_current)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	long long Temp_Value = 0;
	signed int dvalue = 0;
	u16 uvalue16 = 0;

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_ZCV_CON2],
			&uvalue16, sizeof(uvalue16));

	dvalue = uvalue16;
	if (dvalue == 0) {
		Temp_Value = (long long) dvalue;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (long long) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		Temp_Value = (long long) dvalue;
	}

	Temp_Value = Temp_Value * priv->unit_fgcurrent;

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	/* Auto adjust value */
	if (gauge->gm->fg_cust_data.r_fg_value != priv->default_r_fg) {
		bm_debug(gauge->gm, "[fgauge_read_current] %s Auto adjust value due to the Rfg is %d\n Ori curr=%d",
			 priv->desc->gauge_name, gauge->gm->fg_cust_data.r_fg_value, dvalue);

		dvalue = (dvalue * priv->default_r_fg) / gauge->gm->fg_cust_data.r_fg_value;

		bm_debug(gauge->gm, "[fgauge_read_current] %s new current=%d\n", priv->desc->gauge_name, dvalue);
	}

	bm_debug(gauge->gm, "[fgauge_read_current] %s ori current=%d\n", priv->desc->gauge_name, dvalue);
	dvalue = ((dvalue * gauge->gm->fg_cust_data.car_tune_value) / 1000);
	bm_debug(gauge->gm, "[fgauge_read_current] %s final current=%d (ratio=%d)\n",
		 priv->desc->gauge_name, dvalue, gauge->gm->fg_cust_data.car_tune_value);
	*zcv_current = dvalue;

	return 0;
}

static int zcv_get(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr, int *zcv)
{
	struct mt6379_priv *priv = container_of(gauge_dev, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	u16 regval = 0;

	regmap_raw_read(gauge_dev->regmap, rg[bat_idx][MT6379_REG_AUXADC_ADC_OUT_FGADC_PCHR],
			&regval, sizeof(regval));
	adc_result_reg = regval & AUXADC_ADC_OUT_FGADC_PCHR_MASK;

	adc_result = reg_to_mv_value(gauge_dev, adc_result_reg);
	bm_err(gauge_dev->gm, "[oam] %s %s BATSNS ZCV (pchr):adc_result_reg=%d, adc_result=%d\n",
		 __func__, priv->desc->gauge_name, adc_result_reg, adc_result);

	*zcv = adc_result;
	return 0;
}

static int get_charger_zcv(struct mtk_gauge *gauge_dev)
{
	union power_supply_propval val = { .intval = 0 };
	struct power_supply *chg_psy;
	int ret = 0;

	chg_psy = power_supply_get_by_name("mtk-master-charger");

	if (chg_psy == NULL) {
		bm_err(gauge_dev->gm, "[%s] can get charger psy\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_VOLTAGE_BOOT, &val);

	bm_err(gauge_dev->gm, "[%s]_hw_ocv_chgin=%d, ret=%d\n", __func__, val.intval, ret);

	return val.intval;
}

static int boot_zcv_get(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr,
			int *val)
{
	struct mt6379_priv *priv = container_of(gauge_dev, struct mt6379_priv, gauge);
	int _hw_ocv, _sw_ocv;
	int _hw_ocv_src;
	int _prev_hw_ocv, _prev_hw_ocv_src;
	int _hw_ocv_rdy;
	int _flag_unreliable;
	int _hw_ocv_75_pon;
	int _hw_ocv_75_plugin;
	int _hw_ocv_75_before_chgin;
	int _hw_ocv_75_pon_rdy;
	int _hw_ocv_chgin;
	int _hw_ocv_chgin_rdy;
	int now_temp = -1;
	int now_thr;
	int tmp_hwocv_chgin = 0;
	bool fg_is_charger_exist;
	struct mtk_battery *gm;
	struct zcv_data *zcvinfo;
	struct gauge_hw_status *p;

	gm = gauge_dev->gm;
	p = &gauge_dev->hw_status;
	zcvinfo = &gauge_dev->zcv_info;
	_hw_ocv_75_pon_rdy = read_hw_ocv_6379_power_on_rdy(gauge_dev);
	_hw_ocv_75_pon = read_hw_ocv_6379_power_on(gauge_dev);
	_hw_ocv_75_plugin = read_hw_ocv_6379_plug_in(gauge_dev);
	_hw_ocv_75_before_chgin = read_hw_ocv_6379_before_chgin(gauge_dev);

	tmp_hwocv_chgin = get_charger_zcv(gauge_dev);
	if (tmp_hwocv_chgin != -ENODEV)
		_hw_ocv_chgin = tmp_hwocv_chgin / 100;
	else
		_hw_ocv_chgin = 0;

	if (gm == NULL)
		now_thr = 300;
	else {
		now_temp = gm->battery_temp;
		if (now_temp > gm->ext_hwocv_swocv_lt_temp)
			now_thr = gm->ext_hwocv_swocv;
		else
			now_thr = gm->ext_hwocv_swocv_lt;
	}

	if (_hw_ocv_chgin < 25000)
		_hw_ocv_chgin_rdy = 0;
	else
		_hw_ocv_chgin_rdy = 1;

	/* if preloader records charge in, need to using subpmic as hwocv */
	fgauge_get_info(gauge_dev, GAUGE_PROP_PL_CHARGING_STATUS, &zcvinfo->pl_charging_status);
	fgauge_set_info(gauge_dev, GAUGE_PROP_PL_CHARGING_STATUS, 0);
	fgauge_get_info(gauge_dev, GAUGE_PROP_MONITER_PLCHG_STATUS, &zcvinfo->moniter_plchg_bit);
	fgauge_set_info(gauge_dev, GAUGE_PROP_MONITER_PLCHG_STATUS, 0);

	if (zcvinfo->pl_charging_status == 1)
		fg_is_charger_exist = 1;
	else
		fg_is_charger_exist = 0;

	_hw_ocv = _hw_ocv_75_pon;
	_sw_ocv = gauge_dev->hw_status.sw_ocv;
	/* _sw_ocv = get_sw_ocv();*/
	_hw_ocv_src = FROM_PMIC_PON_ON;
	_prev_hw_ocv = _hw_ocv;
	_prev_hw_ocv_src = FROM_PMIC_PON_ON;
	_flag_unreliable = 0;

	if (fg_is_charger_exist) {
		_hw_ocv_rdy = _hw_ocv_75_pon_rdy;
		if (_hw_ocv_rdy == 1) {
			if (_hw_ocv_chgin_rdy == 1) {
				_hw_ocv = _hw_ocv_chgin;
				_hw_ocv_src = FROM_CHR_IN;
			} else {
				_hw_ocv = _hw_ocv_75_pon;
				_hw_ocv_src = FROM_PMIC_PON_ON;
			}

			if (abs(_hw_ocv - _sw_ocv) > now_thr) {
				_prev_hw_ocv = _hw_ocv;
				_prev_hw_ocv_src = _hw_ocv_src;
				_hw_ocv = _sw_ocv;
				_hw_ocv_src = FROM_SW_OCV;
				p->flag_hw_ocv_unreliable = true;
				_flag_unreliable = 1;
			}
		} else {
			/* fixme: swocv is workaround */
			/* plug charger poweron but charger not ready */
			/* should use swocv to workaround */
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
			if (_hw_ocv_chgin_rdy != 1) {
				if (abs(_hw_ocv - _sw_ocv) > now_thr) {
					_prev_hw_ocv = _hw_ocv;
					_prev_hw_ocv_src = _hw_ocv_src;
					_hw_ocv = _sw_ocv;
					_hw_ocv_src = FROM_SW_OCV;
					p->flag_hw_ocv_unreliable = true;
					_flag_unreliable = 1;
				}
			}
		}
	} else {
		if (_hw_ocv_75_pon_rdy == 0) {
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}
	}

	/* final chance to check hwocv */
	if (gm != NULL)
		if (_hw_ocv < 28000 && (gm->disableGM30 == 0)) {
			bm_err(gm, "[%s] %s ERROR, _hw_ocv=%d  src:%d, force use swocv\n",
			       __func__, priv->desc->gauge_name, _hw_ocv, _hw_ocv_src);
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}

	*val = _hw_ocv;

	zcvinfo->charger_zcv = _hw_ocv_chgin;
	zcvinfo->pmic_rdy = _hw_ocv_75_pon_rdy;
	zcvinfo->pmic_zcv = _hw_ocv_75_pon;
	zcvinfo->pmic_in_zcv = _hw_ocv_75_plugin;
	zcvinfo->swocv = _sw_ocv;
	zcvinfo->zcv_from = _hw_ocv_src;
	zcvinfo->zcv_tmp = now_temp;

	if (zcvinfo->zcv_1st_read == false) {
		zcvinfo->charger_zcv_1st = zcvinfo->charger_zcv;
		zcvinfo->pmic_rdy_1st = zcvinfo->pmic_rdy;
		zcvinfo->pmic_zcv_1st = zcvinfo->pmic_zcv;
		zcvinfo->pmic_in_zcv_1st = zcvinfo->pmic_in_zcv;
		zcvinfo->swocv_1st = zcvinfo->swocv;
		zcvinfo->zcv_from_1st = zcvinfo->zcv_from;
		zcvinfo->zcv_tmp_1st = zcvinfo->zcv_tmp;
		zcvinfo->zcv_1st_read = true;
	}

	gauge_dev->fg_hw_info.pmic_zcv = _hw_ocv_75_pon;
	gauge_dev->fg_hw_info.pmic_zcv_rdy = _hw_ocv_75_pon_rdy;
	gauge_dev->fg_hw_info.charger_zcv = _hw_ocv_chgin;
	gauge_dev->fg_hw_info.hw_zcv = _hw_ocv;

	bm_err(gm, "[%s] %s g_fg_is_charger_exist:%d _hw_ocv_chgin_rdy:%d pl:%d %d\n",
	       __func__, priv->desc->gauge_name, fg_is_charger_exist, _hw_ocv_chgin_rdy,
	       zcvinfo->pl_charging_status, zcvinfo->moniter_plchg_bit);
	bm_err(gm, "[%s] %s _hw_ocv:%d _sw_ocv:%d now_thr:%d\n",
	       __func__, priv->desc->gauge_name, _prev_hw_ocv, _sw_ocv, now_thr);
	bm_err(gm, "[%s] %s _hw_ocv:%d _hw_ocv_src:%d _prev_hw_ocv:%d  _prev_hw_ocv_src:%d _flag_unreliable:%d\n",
	       __func__, priv->desc->gauge_name, _hw_ocv, _hw_ocv_src, _prev_hw_ocv,
	       _prev_hw_ocv_src, _flag_unreliable);
	bm_err(gm, "[%s] %s _hw_ocv_75_pon_rdy:%d _hw_ocv_75_pon:%d _hw_ocv_75_plugin:%d _hw_ocv_chgin:%d _sw_ocv:%d now_temp:%d now_thr:%d\n",
	       __func__, priv->desc->gauge_name, _hw_ocv_75_pon_rdy, _hw_ocv_75_pon,
		_hw_ocv_75_plugin, _hw_ocv_chgin, _sw_ocv, now_temp, now_thr);
	bm_err(gm, "[%s] %s _hw_ocv_75_before_chgin %d\n",
	       __func__, priv->desc->gauge_name, _hw_ocv_75_before_chgin);

	return 0;
}

static int reset_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		     int threshold)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	bm_err(gauge->gm, "[fgauge_hw_reset] %s\n", priv->desc->gauge_name);
	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2],
			   FG_CHARGE_RST_MASK | FG_TIME_RST_MASK, FG_CHARGE_RST_MASK | FG_TIME_RST_MASK);
	mdelay(1);
	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CON2],
			   FG_CHARGE_RST_MASK | FG_TIME_RST_MASK, 0);

	return 0;
}

static int ptim_resist_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret;

	if (IS_ERR(gauge->chan_ptim_r)) {
		bm_err(gauge->gm, "%s, %s auxadc chan: imix_r(ptim_r) error\n",
		       __func__, priv->desc->gauge_name);
		return -EOPNOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_ptim_r, val);
	if (ret)
		bm_err(gauge->gm, "%s, Failed to read %s auxadc imix_r(ptim_r), ret=%d\n",
		       __func__, priv->desc->gauge_name, ret);

	bm_err(gauge->gm, "%s, %s imix_r(ptim_r) val:%d, ret=%d\n", __func__, priv->desc->gauge_name, *val, ret);
	return ret;
}

static int ptim_battery_voltage_get(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret;

	if (IS_ERR(gauge->chan_ptim_bat_voltage)) {
		bm_err(gauge->gm, "%s, %s auxadc chan: imp(ptim_bat_volt) error\n",
		       __func__, priv->desc->gauge_name);
		return -EOPNOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_ptim_bat_voltage, val);

	if (ret)
		bm_err(gauge->gm, "%s, Failed to read %s auxadc imp(ptim_bat_volt), ret=%d\n",
		       __func__, priv->desc->gauge_name, ret);

	bm_err(gauge->gm, "%s, %s imp(ptim_bat_volt) val:%d, ret=%d\n",
	       __func__, priv->desc->gauge_name, *val, ret);
	return ret;
}

static int rtc_ui_soc_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			  int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int rtc_ui_soc = 0;
	u8 rtc_value;

	rtc_value = get_rtc_spare_fg_value(gauge);
	rtc_ui_soc = (rtc_value & 0x7f);

	*val = rtc_ui_soc;

	if (rtc_ui_soc > 100 || rtc_ui_soc < 0)
		bm_err(gauge->gm, "%s, %s ERR! rtc=0x%x, ui_soc=%d\n",
		       __func__, priv->desc->gauge_name, rtc_value, rtc_ui_soc);
	else
		bm_debug(gauge->gm, "%s, %s rtc=0x%x, ui_soc=%d\n",
			 __func__, priv->desc->gauge_name, rtc_value, rtc_ui_soc);


	return 0;
}

static int rtc_ui_soc_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	u8 spare3_reg = get_rtc_spare_fg_value(gauge);
	int spare3_reg_valid = 0;
	int new_spare3_reg = 0;
	int latest_spare3_reg = 0;

	if (!gauge->gm) {
		dev_info(priv->dev, "%s, gm has not initialized\n", __func__);
		return 1;
	}

	spare3_reg_valid = (spare3_reg & 0x80);
	new_spare3_reg = spare3_reg_valid + val;

	set_rtc_spare_fg_value(gauge, new_spare3_reg);

	latest_spare3_reg = get_rtc_spare_fg_value(gauge);

	bm_debug(gauge->gm, "[%s] %s ui_soc=%d, spare3_reg=0x%x, %x, %x, valid:%d\n",
		 __func__, priv->desc->gauge_name, val, spare3_reg, new_spare3_reg,
		 latest_spare3_reg, spare3_reg_valid);

	if (latest_spare3_reg != new_spare3_reg) {
		latest_spare3_reg = get_rtc_spare_fg_value(gauge);
		bm_err(gauge->gm, "[%s][retry] %sui_soc=%d, spare3_reg=0x%x, %x, %x, valid:%d\n",
			__func__, priv->desc->gauge_name, val, spare3_reg, new_spare3_reg,
			latest_spare3_reg, spare3_reg_valid);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		if (latest_spare3_reg != new_spare3_reg && gauge->gm->disableGM30 == 0)
			aee_kernel_warning("BATTERY_RTC",
					   "RTC: Failed to write RG_SPARE, contact the RTC owner!");
#endif
	}

	return 1;
}

static void switch_vbat2_det_time(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 3)
		*value = 0;
	else if (_prd >= 3 && _prd < 5)
		*value = 1;
	else if (_prd >= 5 && _prd < 10)
		*value = 2;
	else if (_prd >= 10)
		*value = 3;
}

static void switch_vbat2_debt_counter(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 2)
		*value = 0;
	else if (_prd >= 2 && _prd < 4)
		*value = 1;
	else if (_prd >= 4 && _prd < 8)
		*value = 2;
	else if (_prd >= 8)
		*value = 3;
}

static int vbat_lt_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		       int threshold)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int vbat2_l_th_mv =  threshold;
	int vbat2_l_th_reg = mv_to_reg_12_value(gauge, vbat2_l_th_mv);
	int vbat2_det_counter = 0;
	int vbat2_det_time = 0;
	u16 regval;

	switch_vbat2_det_time(gauge->hw_status.vbat2_det_time, &vbat2_det_time);
	switch_vbat2_debt_counter(gauge->hw_status.vbat2_det_counter, &vbat2_det_counter);

	regval = vbat2_l_th_reg & AUXADC_LBAT2_VOLT_MIN_MASK;
	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_6],
			 &regval, sizeof(regval));

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_1],
			   AUXADC_LBAT2_DET_PRD_SEL_MASK,
			   vbat2_det_time << AUXADC_LBAT2_DET_PRD_SEL_SHIFT);

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_1],
			   AUXADC_LBAT2_DEBT_MIN_SEL_MASK,
			   vbat2_det_counter << AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT);

	bm_debug(gauge->gm, "[fg_set_vbat2_l_th] %s thr:%d [0x%x %d 0x%x %d 0x%x]\n",
		 priv->desc->gauge_name, threshold, vbat2_l_th_reg, gauge->hw_status.vbat2_det_time,
		 vbat2_det_time, gauge->hw_status.vbat2_det_counter, vbat2_det_counter);

	return 0;
}

static int vbat_ht_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		       int threshold)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int vbat2_h_th_mv =  threshold;
	int vbat2_h_th_reg = mv_to_reg_12_value(gauge, vbat2_h_th_mv);
	int vbat2_det_counter = 0;
	int vbat2_det_time = 0;
	u16 regval;

	switch_vbat2_det_time(gauge->hw_status.vbat2_det_time, &vbat2_det_time);
	switch_vbat2_debt_counter(gauge->hw_status.vbat2_det_counter, &vbat2_det_counter);

	regval = vbat2_h_th_reg & AUXADC_LBAT2_VOLT_MAX_MASK;
	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_3],
			 &regval, sizeof(regval));

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_1],
			   AUXADC_LBAT2_DET_PRD_SEL_MASK,
			   vbat2_det_time << AUXADC_LBAT2_DET_PRD_SEL_SHIFT);

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_1],
			   AUXADC_LBAT2_DEBT_MAX_SEL_MASK,
			   vbat2_det_counter << AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT);

	bm_debug(gauge->gm, "[fg_set_vbat2_h_th] %s thr:%d [0x%x %d 0x%x %d 0x%x]\n",
		 priv->desc->gauge_name, threshold, vbat2_h_th_reg, gauge->hw_status.vbat2_det_time,
		 vbat2_det_time, gauge->hw_status.vbat2_det_counter, vbat2_det_counter);

	return 0;
}

static void enable_lbat2_en(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	if (gauge->vbat_l_en == true || gauge->vbat_h_en == true)
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_0],
				   AUXADC_LBAT2_EN_MASK, AUXADC_LBAT2_EN_MASK);

	if (gauge->vbat_l_en == false && gauge->vbat_h_en == false)
		regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_0],
				   AUXADC_LBAT2_EN_MASK, 0);
}

static int en_l_vbat_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int *cnt = &(priv->fg_info.en_l_vbat_cnt);

	bm_debug(gauge->gm, "%s, %s en_l_vbat_cnt:%d en:%d\n", __func__, priv->desc->gauge_name, *cnt, val);
	*cnt = val ? *cnt + 1 : *cnt - 1;

	if (val)
		enable_gauge_irq(gauge, VBAT_L_IRQ);
	else
		disable_gauge_irq(gauge, VBAT_L_IRQ);

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_5],
			   AUXADC_LBAT2_IRQ_EN_MIN_MASK | AUXADC_LBAT2_DET_MIN_MASK,
			   val ? (AUXADC_LBAT2_IRQ_EN_MIN_MASK | AUXADC_LBAT2_DET_MIN_MASK) : 0);

	gauge->vbat_l_en = !!val;
	enable_lbat2_en(gauge);

	return 0;
}

static int en_h_vbat_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;

	bm_debug(gauge->gm, "%s, %s en:%d\n", __func__, priv->desc->gauge_name, val);
	if (val)
		enable_gauge_irq(gauge, VBAT_H_IRQ);
	else
		disable_gauge_irq(gauge, VBAT_H_IRQ);

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_LBAT2_2],
			   AUXADC_LBAT2_IRQ_EN_MAX_MASK | AUXADC_LBAT2_DET_MAX_MASK,
			   val ? (AUXADC_LBAT2_IRQ_EN_MAX_MASK | AUXADC_LBAT2_DET_MAX_MASK) : 0);

	gauge->vbat_h_en = !!val;
	enable_lbat2_en(gauge);

	return 0;
}

static int mt6379_enable_auxadc_hm(struct mt6379_priv *priv, bool en)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	static const u8 code[] = { 0x63, 0x63 };

	if (en)
		return regmap_bulk_write(priv->regmap, rg[bat_idx][MT6379_REG_HK_TOP_WKEY],
					 code, ARRAY_SIZE(code));

	return regmap_write(priv->regmap, rg[bat_idx][MT6379_REG_HK_TOP_WKEY], 0);
}

static int mt6379_get_vbat_mon_rpt(struct mt6379_priv *priv, int *vbat)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_phandle(priv->dev->of_node, "charger");
	if (psy) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		if (ret >= 0)
			*vbat = val.intval;

		power_supply_put(psy);
	} else {
		ret = iio_read_channel_processed(priv->adcs[CHAN_ADC_VBATMON], vbat);
		if (ret) {
			*vbat = 0;
			dev_info(priv->dev, "Failed to get chg_adc VBAT_MON data\n");
		}

		*vbat /= 1000;
	}

	return ret;
}

static int __maybe_unused battery_voltage_cali(struct mtk_gauge *gauge,
					       struct mtk_gauge_sysfs_field_info *attr,
					       int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int ret = 0;
	int chg_vbat, auxadc_vbat, vbat_diff, vbat_diff_sum = 0, vbat_diff_avg;
	int chg_vbat_min = INT_MAX, auxadc_vbat_min = INT_MAX;
	int chg_vbat_max = 0, auxadc_vbat_max = 0;
	int cnt = 0, max_cnt = 5;
	int value = 0;
	u16 gain_err = priv->gain_err, gain_err_diff;
	u16 data = 0;

	while (abs(cnt) < max_cnt) {
		ret = instant_current(gauge, &value, FG_CIC1);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to get %s cic1 (ret:%d)\n",
				 __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		bm_err(gauge->gm, "%s, %s cic1 = %d\n", __func__, priv->desc->gauge_name, value);
		if (abs(value) > 500) {
			bm_err(gauge->gm, "%s, %s cic1 out of range(%d)\n",
			       __func__, priv->desc->gauge_name, value);
			return -EINVAL;
		}

		ret = mt6379_get_vbat_mon_rpt(priv, &chg_vbat);
		if (ret < 0) {
			bm_err(gauge->gm, "%s, Failed to get %s vbat_mon\n",
			       __func__, priv->desc->gauge_name);
			return ret;
		}

		ret = iio_read_channel_processed(gauge->chan_bat_voltage,
						 &auxadc_vbat);
		if (ret < 0) {
			bm_err(gauge->gm, "%s, Failed to get %s auxadc_vbat(%d)\n",
			       __func__, priv->desc->gauge_name, ret);
			return ret;
		}

		dev_info(priv->dev, "%s, %s chg_vbat(vbat_mon):%d, auxadc_vbat:%d\n",
			 __func__, priv->desc->gauge_name, chg_vbat, auxadc_vbat);
		chg_vbat_min = min(chg_vbat_min, chg_vbat);
		chg_vbat_max = max(chg_vbat_max, chg_vbat);
		auxadc_vbat_min = min(auxadc_vbat_min, auxadc_vbat);
		auxadc_vbat_max = max(auxadc_vbat_max, auxadc_vbat);
		dev_info(priv->dev, "%s, %s chg_vbat_min:%d, chg_vbat_max:%d\n",
			 __func__, priv->desc->gauge_name, chg_vbat_min, chg_vbat_max);
		dev_info(priv->dev, "%s, %s auxadc_vbat_min:%d, auxadc_vbat_max:%d\n",
			 __func__, priv->desc->gauge_name, auxadc_vbat_min, auxadc_vbat_max);
		if (chg_vbat_max - chg_vbat_min > HTOL_THRESHOLD_MAX ||
		    auxadc_vbat_max - auxadc_vbat_min > HTOL_THRESHOLD_MAX) {
			bm_err(gauge->gm, "%s, %s vbat_diff min/max out of range\n",
			       __func__, priv->desc->gauge_name);
			return ret;
		}

		vbat_diff = chg_vbat - auxadc_vbat;
		vbat_diff_sum += vbat_diff;

		if (abs(vbat_diff) > HTOL_THRESHOLD_MAX || abs(vbat_diff) < HTOL_THRESHOLD_MIN) {
			bm_err(gauge->gm, "%s, %s vbat_diff is out of range(%d), no need to calibrate\n",
			       __func__, priv->desc->gauge_name, vbat_diff);
			return ret;
		}

		if (vbat_diff >= HTOL_THRESHOLD_MIN && cnt++ >= 0)
			continue;
		else if (vbat_diff <= -HTOL_THRESHOLD_MIN && cnt-- <= 0)
			continue;
		else
			return ret;
	}

	vbat_diff_avg = vbat_diff_sum / max_cnt;
	dev_info(priv->dev, "%s, %s vbat_diff_avg:%d, gain_err:0x%x, efuse_gain_err:0x%x\n",
		 __func__, priv->desc->gauge_name, vbat_diff_avg, gain_err, priv->efuse_gain_err);
	gain_err += vbat_diff_avg;
	gain_err_diff = abs((int)gain_err - (int)priv->efuse_gain_err);
	if (abs(gain_err_diff) > HTOL_CALI_MAX) {
		bm_err(gauge->gm, "%s, %s gain_err_diff out of theshold(%d), adjust HTOL_THRESHOLD_MAX\n",
			__func__, priv->desc->gauge_name, gain_err_diff);
		if (gain_err > priv->efuse_gain_err)
			gain_err = priv->efuse_gain_err + HTOL_CALI_MAX;
		else
			gain_err = priv->efuse_gain_err - HTOL_CALI_MAX;
		return ret;
	}

	ret = mt6379_enable_auxadc_hm(priv, true);
	if (ret < 0)
		return ret;
	ret = regmap_bulk_write(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_EFUSE_GAIN_TRIM],
				&gain_err, 2);
	if (ret < 0)
		goto out;
	priv->gain_err = gain_err;
out:
	mt6379_enable_auxadc_hm(priv, false);
	ret = regmap_bulk_read(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_EFUSE_GAIN_TRIM],
			       &data, 2);
	dev_info(priv->dev, "%s, %s after cali, gain_err:0x%x\n", __func__, priv->desc->gauge_name, data);
	dev_info(priv->dev, "%s, %s done(%d)\n", __func__, priv->desc->gauge_name, ret);
	return ret;
}

static int mt6379_auxadc_init_vbat_calibration(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	int ret, offset_trim = 0;
	u16 data = 0;

	regmap_bulk_read(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_EFUSE_OFFSET_TRIM],
			 &offset_trim, 2);
	if (offset_trim >= 0x4000) {
		bm_err(priv->gauge.gm, "%s, %s before handle offset trim signed, offset_trim:0x%x\n",
		       __func__, priv->desc->gauge_name, offset_trim);
		offset_trim = -(0x8000 - offset_trim);
	}

	priv->offset_trim = offset_trim;

	ret = mt6379_enable_auxadc_hm(priv, true);
	if (ret < 0)
		return ret;

	ret = regmap_bulk_read(priv->regmap, rg[bat_idx][MT6379_REG_AUXADC_EFUSE_GAIN_TRIM],
			       &data, 2);
	if (ret) {
		mt6379_enable_auxadc_hm(priv, false);
		return ret;
	}

	priv->gain_err = data;
	mt6379_enable_auxadc_hm(priv, false);

	ret = mt6379_enable_tm(priv, true);
	if (ret)
		return ret;

	ret = regmap_bulk_read(priv->regmap, MT6379_REG_AUXADC_DIG_3_ELR0, &data, 2);
	if (ret < 0)
		bm_err(priv->gauge.gm, "%s, Failed to get auxadc efuse trim\n", __func__);

	priv->efuse_gain_err = data;
	dev_info(priv->dev, "%s, %s auxadc gain_err:0x%x, auxadc efuse_gain_err:0x%x\n",
		 __func__, priv->desc->gauge_name, priv->gain_err, priv->efuse_gain_err);

	return mt6379_enable_tm(priv, false);
}

static int regmap_type_get(struct mtk_gauge *gauge,
				struct mtk_gauge_sysfs_field_info *attr,
				int *val)
{
	*val = gauge->regmap_type;
	return 0;
}

static int bif_voltage_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret;

	if (IS_ERR(gauge->chan_bif)) {
		bm_err(gauge->gm, "%s, %s auxadc chan: vref(bif) error\n",
		       __func__, priv->desc->gauge_name);
		return -EOPNOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_bif, val);
	if (ret < 0)
		bm_err(gauge->gm, "%s, Failed to read %s auxadc vref(bif), ret=%d\n",
		       __func__, priv->desc->gauge_name, ret);

	bm_debug(gauge->gm, "%s, %s vref(bif) val:%d, ret=%d\n",
		 __func__, priv->desc->gauge_name, *val, ret);
	return ret;
}

static int battery_temperature_adc_get(struct mtk_gauge *gauge,
				       struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret;

	if (IS_ERR(gauge->chan_bat_temp)) {
		bm_err(gauge->gm, "%s, %s auxadc chan: baton(bat_temp) error\n",
		       __func__, priv->desc->gauge_name);
		return -EOPNOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_bat_temp, val);
	if (ret < 0)
		bm_err(gauge->gm, "%s, %s auxadc chan: baton(bat_temp) error\n",
		       __func__, priv->desc->gauge_name);

	bm_debug(gauge->gm, "%s, %s baton(bat_temp) val:%d, ret=%d\n",
		 __func__, priv->desc->gauge_name, *val, ret);
	return ret;
}

static int __maybe_unused auxadc_reset(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	int ret;

	ret = mt6379_enable_auxadc_hm(priv, true);
	if (ret < 0)
		return ret;

	ret = regmap_write(priv->regmap, rg[bat_idx][MT6379_REG_HK_TOP_RST_CON0], RESET_MASK);
	if (ret)
		goto out;

	ret = regmap_write(priv->regmap, rg[bat_idx][MT6379_REG_HK_TOP_RST_CON0], 0);
	if (ret)
		goto out;
out:
	mt6379_enable_auxadc_hm(priv, false);
	return ret;
}

#define BAT_VOL_GET_PRINT_PERIOD	3
static int bat_vol_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int i = 0, ret = 0, vbat_mon = 0;
	u32 data = 0;
	long long *t1 = &(priv->fg_info.bat_vol_get_t1);
	static int dump_reg[] = { 0x736, 0x737, 0x738, 0x81C, 0x81D, 0x838, 0x839,
				  0x83A, 0x85D, 0x85E, 0x908, 0x909, 0x90A, 0x90B,
				  0x910, 0x911, 0x916, 0x917, 0x91E, 0x91F, 0x922,
				  0x923, 0x95C, 0x96E, 0x96F, 0x970, 0x971 };
	int bank_offset = bat_idx * 0x300;

	if (IS_ERR(gauge->chan_bat_voltage)) {
		bm_debug(gauge->gm, "%s, %s auxadc chan: batsns(bat_volt) error\n",
		       __func__, priv->desc->gauge_name);
		return -EOPNOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_bat_voltage, val);
	if (ret < 0) {
		bm_err(gauge->gm, "%s, Failed to read %s auxadc batsns(bat_volt), ret=%d\n",
		       __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	if (*val < 1000) {
		if (*t1 == 0) {
			*t1 = local_clock();
		} else if ((local_clock() - *t1) / NSEC_PER_SEC > BAT_VOL_GET_PRINT_PERIOD) {
			*t1 = local_clock();
			ret = mt6379_get_vbat_mon_rpt(priv, &vbat_mon);
			bm_err(gauge->gm, "[%s] %s vbat_mon = %d(%d)\n",
			       __func__, priv->desc->gauge_name, vbat_mon, ret);
			for (i = 0; i < ARRAY_SIZE(dump_reg); i++) {
				ret = regmap_read(gauge->regmap,
						  dump_reg[i] + bank_offset,
						  &data);
				bm_err(gauge->gm, "[%s] %s addr:0x%4x, data:0x%x(%d)\n",
				       __func__, priv->desc->gauge_name, dump_reg[i] + bank_offset,
				       data, ret);
			}
		}
	}

	bm_debug(gauge->gm, "%s, %s batsns(bat_volt) val:%d, ret=%d\n",
		 __func__, priv->desc->gauge_name, *val, ret);
	return ret;
}

static int hw_version_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			  int *val)
{
	*val = GAUGE_HW_V2000;
	return 0;
}


static int battery_exist_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			     int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);

#if defined(CONFIG_FPGA_EARLY_PORTING)
	*val = 0;
	return 0;
#endif

	return mt6379_get_battery_state(priv, val);
}

static int coulomb_interrupt_lt_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 temp_car_15_0 = 0;
	u16 temp_car_31_16 = 0;
	unsigned int uvalue32_car_msb = 0;
	signed int lowbound = 0;
	u16 lowbound_31_16 = 0, lowbound_15_00 = 0;
	signed int value32_car;
	long long car = val;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	bm_debug(gauge->gm, "%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_gauge_irq(gauge, COULOMB_L_IRQ);
		return 0;
	}

	pre_gauge_update(gauge);

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CAR_CON0], &temp_car_15_0, sizeof(temp_car_15_0));
	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CAR_CON1], &temp_car_31_16, sizeof(temp_car_31_16));

	post_gauge_update(gauge);

	uvalue32_car_msb = (temp_car_31_16 & 0x8000) >> 15;
	value32_car = temp_car_31_16 << 16 | temp_car_15_0;

	bm_debug(gauge->gm, "[%s] %s FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		 __func__, priv->desc->gauge_name, value32_car, value32_car, uvalue32_car_msb,
		 temp_car_15_0, temp_car_31_16);

	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / priv->unit_charge;
	/* car * 1000 * 100 */
#else
	car = div_s64(car * 100000, priv->unit_charge);
#endif

	if (r_fg_value != priv->default_r_fg && priv->default_r_fg != 0)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) / priv->default_r_fg;
#else
		car = div_s64(car * r_fg_value, priv->default_r_fg);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	lowbound = value32_car;

	bm_debug(gauge->gm, "[%s] %s low=0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, priv->desc->gauge_name, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_00 = (lowbound & 0xffff);

	bm_debug(gauge->gm, "[%s] %s final low=0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, priv->desc->gauge_name, lowbound, lowbound, car, car);

	bm_debug(gauge->gm, "[%s] %s final low 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, priv->desc->gauge_name, lowbound, lowbound_31_16, lowbound_15_00, car);

	disable_gauge_irq(gauge, COULOMB_L_IRQ);

	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CARTH_CON0],
			 &lowbound_15_00, sizeof(lowbound_15_00));

	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CARTH_CON1],
			 &lowbound_31_16, sizeof(lowbound_31_16));

	mdelay(1);

	enable_gauge_irq(gauge, COULOMB_L_IRQ);

	bm_debug(gauge->gm, "[%s] %s low:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		 __func__, priv->desc->gauge_name, lowbound_15_00, lowbound_31_16, val, value32_car,
		 gauge->irq_no[COULOMB_L_IRQ]);

	return 0;
}

static int coulomb_interrupt_ht_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 temp_car_15_0 = 0;
	u16 temp_car_31_16 = 0;
	unsigned int uvalue32_car_msb = 0;
	signed int upperbound = 0;
	u16 upperbound_31_16 = 0, upperbound_15_00 = 0;
	signed int value32_car;
	long long car = val;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	bm_debug(gauge->gm, "%s, %s car=%d\n", __func__, priv->desc->gauge_name, val);
	if (car == 0) {
		disable_gauge_irq(gauge, COULOMB_H_IRQ);
		return 0;
	}

	pre_gauge_update(gauge);

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CAR_CON0],
			&temp_car_15_0, sizeof(temp_car_15_0));
	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CAR_CON1],
			&temp_car_31_16, sizeof(temp_car_31_16));

	post_gauge_update(gauge);

	uvalue32_car_msb = (temp_car_31_16 & 0x8000) >> 15;
	value32_car = temp_car_31_16 << 16 | temp_car_15_0;

	bm_debug(gauge->gm, "[%s] %s FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		 __func__, priv->desc->gauge_name, value32_car, value32_car, uvalue32_car_msb,
		 temp_car_15_0, temp_car_31_16);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / priv->unit_charge;
#else
	car = div_s64(car * 100000, priv->unit_charge);
#endif

	if (r_fg_value != priv->default_r_fg && priv->default_r_fg != 0)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) /
			priv->default_r_fg;
#else
		car = div_s64(car * r_fg_value,
			priv->default_r_fg);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	upperbound = value32_car;

	bm_debug(gauge->gm, "[%s] %s upper = 0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, priv->desc->gauge_name, upperbound, upperbound, car, car);

	upperbound = upperbound + car;

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_00 = (upperbound & 0xffff);

	bm_debug(gauge->gm, "[%s] %s final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, priv->desc->gauge_name, upperbound, upperbound, car, car);

	bm_debug(gauge->gm, "[%s] %s final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, priv->desc->gauge_name, upperbound, upperbound_31_16, upperbound_15_00, car);

	disable_gauge_irq(gauge, COULOMB_H_IRQ);

	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CARTH_CON2],
			 &upperbound_15_00, sizeof(upperbound_15_00));

	regmap_raw_write(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_CARTH_CON3],
			 &upperbound_31_16, sizeof(upperbound_31_16));

	mdelay(1);

	enable_gauge_irq(gauge, COULOMB_H_IRQ);

	bm_debug(gauge->gm, "[%s] %s high:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		 __func__, priv->desc->gauge_name, upperbound_15_00, upperbound_31_16, val, value32_car,
		 gauge->irq_no[COULOMB_H_IRQ]);

	return 0;
}

static int battery_current_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			       int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret = 0;

	ret = instant_current(gauge, val, FG_CIC1);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to get %s CIC1, ret = %d\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	return 0;
}

static int battery_cic2_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			    int *val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret = 0;

	ret = instant_current(gauge, val, FG_CIC2);
	if (ret) {
		dev_info(priv->dev, "%s, Failed to get %s CIC2, ret = %d\n",
			 __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	return 0;
}

static int initial_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int is_charger_exist = 0, bat_flag = 0, valid = 0, ui_soc = 0;
	const unsigned int bat_idx = priv->desc->bat_idx;
	u16 rev_val = 0;

	regmap_update_bits(gauge->regmap, rg[bat_idx][MT6379_REG_AUXADC_NAG_0],
			   AUXADC_NAG_PRD_MASK, 2 << AUXADC_NAG_PRD_SHIFT);

	fgauge_get_info(gauge, GAUGE_PROP_BAT_PLUG_STATUS, &bat_flag);
	fgauge_get_info(gauge, GAUGE_PROP_PL_CHARGING_STATUS, &is_charger_exist);

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON0],
			&rev_val, sizeof(rev_val));

	bm_err(gauge->gm, "%s bat_plug:%d chr:%d info0:0x%x\n",
	       priv->desc->gauge_name, bat_flag, is_charger_exist, rev_val);

	fgauge_get_info(gauge, GAUGE_PROP_CON1_VAILD, &valid);
	fgauge_get_info(gauge, GAUGE_PROP_CON1_UISOC, &ui_soc);

	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_SYSTEM_INFO_CON1],
			&rev_val, sizeof(rev_val));
	bm_err(gauge->gm, "%s valid:%d uisoc:%d info1:0x%x\n", priv->desc->gauge_name, valid, ui_soc, rev_val);

	gauge->hw_status.pl_charger_status = is_charger_exist;

	if (is_charger_exist == 1) {
		gauge->hw_status.is_bat_plugout = 1;
		fgauge_set_info(gauge, GAUGE_PROP_2SEC_REBOOT, 0);
	} else
		gauge->hw_status.is_bat_plugout = bat_flag ? 0 : 1;

	fgauge_set_info(gauge, GAUGE_PROP_BAT_PLUG_STATUS, 1);
	/*[12:8], 5 bits*/
	gauge->hw_status.bat_plug_out_time = 31;

	fgauge_read_RTC_boot_status(gauge);

	return 1;
}

static ssize_t gauge_sysfs_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *gauge_attr;
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	gauge_attr = container_of(attr, struct mtk_gauge_sysfs_field_info, attr);
	if (gauge_attr->set != NULL) {
		mutex_lock(&gauge->ops_lock);
		gauge_attr->set(gauge, gauge_attr, val);
		mutex_unlock(&gauge->ops_lock);
	}

	return count;
}

static ssize_t gauge_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *gauge_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	gauge_attr = container_of(attr, struct mtk_gauge_sysfs_field_info, attr);
	if (gauge_attr->get != NULL) {
		mutex_lock(&gauge->ops_lock);
		gauge_attr->get(gauge, gauge_attr, &val);
		mutex_unlock(&gauge->ops_lock);
	}

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

/* Must be in the same order as GAUGE_PROP_* */
static struct mtk_gauge_sysfs_field_info mt6379_sysfs_field_tbl[] = {
	GAUGE_SYSFS_FIELD_WO(initial_set, GAUGE_PROP_INITIAL),
	GAUGE_SYSFS_FIELD_RO(battery_current_get, GAUGE_PROP_BATTERY_CURRENT),
	GAUGE_SYSFS_FIELD_RO(coulomb_get, GAUGE_PROP_COULOMB),
	GAUGE_SYSFS_FIELD_WO(coulomb_interrupt_ht_set, GAUGE_PROP_COULOMB_HT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(coulomb_interrupt_lt_set, GAUGE_PROP_COULOMB_LT_INTERRUPT),
	GAUGE_SYSFS_FIELD_RO(battery_exist_get, GAUGE_PROP_BATTERY_EXIST),
	GAUGE_SYSFS_FIELD_RO(hw_version_get, GAUGE_PROP_HW_VERSION),
	GAUGE_SYSFS_FIELD_RO(bat_vol_get, GAUGE_PROP_BATTERY_VOLTAGE),
	GAUGE_SYSFS_FIELD_RO(battery_temperature_adc_get, GAUGE_PROP_BATTERY_TEMPERATURE_ADC),
	GAUGE_SYSFS_FIELD_RO(bif_voltage_get, GAUGE_PROP_BIF_VOLTAGE),
	GAUGE_SYSFS_FIELD_WO(en_h_vbat_set, GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(en_l_vbat_set, GAUGE_PROP_EN_LOW_VBAT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(vbat_ht_set, GAUGE_PROP_VBAT_HT_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(vbat_lt_set, GAUGE_PROP_VBAT_LT_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_RW(rtc_ui_soc, rtc_ui_soc_set, rtc_ui_soc_get, GAUGE_PROP_RTC_UI_SOC),
	GAUGE_SYSFS_FIELD_RO(ptim_battery_voltage_get, GAUGE_PROP_PTIM_BATTERY_VOLTAGE),
	GAUGE_SYSFS_FIELD_RO(ptim_resist_get, GAUGE_PROP_PTIM_RESIST),
	GAUGE_SYSFS_FIELD_WO(reset_set, GAUGE_PROP_RESET),
	GAUGE_SYSFS_FIELD_RO(boot_zcv_get, GAUGE_PROP_BOOT_ZCV),
	GAUGE_SYSFS_FIELD_RO(zcv_get, GAUGE_PROP_ZCV),
	GAUGE_SYSFS_FIELD_RO(zcv_current_get, GAUGE_PROP_ZCV_CURRENT),
	GAUGE_SYSFS_FIELD_RO(nafg_cnt_get, GAUGE_PROP_NAFG_CNT),
	GAUGE_SYSFS_FIELD_RO(nafg_dltv_get, GAUGE_PROP_NAFG_DLTV),
	GAUGE_SYSFS_FIELD_RW(nafg_c_dltv, nafg_c_dltv_set, nafg_c_dltv_get, GAUGE_PROP_NAFG_C_DLTV),
	GAUGE_SYSFS_FIELD_WO(nafg_en_set, GAUGE_PROP_NAFG_EN),
	GAUGE_SYSFS_FIELD_WO(nafg_zcv_set, GAUGE_PROP_NAFG_ZCV),
	GAUGE_SYSFS_FIELD_RO(nafg_vbat_get, GAUGE_PROP_NAFG_VBAT),
	GAUGE_SYSFS_FIELD_WO(reset_fg_rtc_set, GAUGE_PROP_RESET_FG_RTC),
	GAUGE_SYSFS_FIELD_RW(gauge_initialized, gauge_initialized_set, gauge_initialized_get,
			     GAUGE_PROP_GAUGE_INITIALIZED),
	GAUGE_SYSFS_FIELD_RO(average_current_get, GAUGE_PROP_AVERAGE_CURRENT),
	GAUGE_SYSFS_FIELD_WO(bat_plugout_en_set, GAUGE_PROP_BAT_PLUGOUT_EN),
	GAUGE_SYSFS_FIELD_WO(zcv_intr_threshold_set, GAUGE_PROP_ZCV_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(zcv_intr_en_set, GAUGE_PROP_ZCV_INTR_EN),
	GAUGE_SYSFS_FIELD_WO(soff_reset_set, GAUGE_PROP_SOFF_RESET),
	GAUGE_SYSFS_FIELD_WO(ncar_reset_set, GAUGE_PROP_NCAR_RESET),
	GAUGE_SYSFS_FIELD_WO(bat_cycle_intr_threshold_set, GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(hw_info_set, GAUGE_PROP_HW_INFO),
	GAUGE_SYSFS_FIELD_WO(event_set, GAUGE_PROP_EVENT),
	GAUGE_SYSFS_FIELD_WO(en_bat_tmp_ht_set, GAUGE_PROP_EN_BAT_TMP_HT),
	GAUGE_SYSFS_FIELD_WO(en_bat_tmp_lt_set, GAUGE_PROP_EN_BAT_TMP_LT),
	GAUGE_SYSFS_FIELD_WO(bat_tmp_ht_threshold_set, GAUGE_PROP_BAT_TMP_HT_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(bat_tmp_lt_threshold_set, GAUGE_PROP_BAT_TMP_LT_THRESHOLD),
	GAUGE_SYSFS_INFO_FIELD_RW(info_2sec_reboot, GAUGE_PROP_2SEC_REBOOT),
	GAUGE_SYSFS_INFO_FIELD_RW(info_pl_charging_status, GAUGE_PROP_PL_CHARGING_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(info_monitor_plchg_status, GAUGE_PROP_MONITER_PLCHG_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(info_bat_plug_status, GAUGE_PROP_BAT_PLUG_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(info_is_nvram_fail_mode, GAUGE_PROP_IS_NVRAM_FAIL_MODE),
	GAUGE_SYSFS_INFO_FIELD_RW(info_monitor_soff_validtime, GAUGE_PROP_MONITOR_SOFF_VALIDTIME),
	GAUGE_SYSFS_INFO_FIELD_RW(info_con0_soc, GAUGE_PROP_CON0_SOC),
	GAUGE_SYSFS_INFO_FIELD_RW(info_con1_uisoc, GAUGE_PROP_CON1_UISOC),
	GAUGE_SYSFS_INFO_FIELD_RW(info_con1_vaild, GAUGE_PROP_CON1_VAILD),
	GAUGE_SYSFS_INFO_FIELD_RW(info_shutdown_car, GAUGE_PROP_SHUTDOWN_CAR),
	GAUGE_SYSFS_INFO_FIELD_RW(car_tune_value, GAUGE_PROP_CAR_TUNE_VALUE),
	GAUGE_SYSFS_INFO_FIELD_RW(r_fg_value, GAUGE_PROP_R_FG_VALUE),
	GAUGE_SYSFS_INFO_FIELD_RW(vbat2_detect_time, GAUGE_PROP_VBAT2_DETECT_TIME),
	GAUGE_SYSFS_INFO_FIELD_RW(vbat2_detect_counter, GAUGE_PROP_VBAT2_DETECT_COUNTER),
	GAUGE_SYSFS_FIELD_WO(bat_temp_froze_en_set, GAUGE_PROP_BAT_TEMP_FROZE_EN),
	GAUGE_SYSFS_FIELD_RO(battery_voltage_cali, GAUGE_PROP_BAT_EOC),
	GAUGE_SYSFS_FIELD_RO(regmap_type_get, GAUGE_PROP_REGMAP_TYPE),
	GAUGE_SYSFS_FIELD_RO(battery_cic2_get, GAUGE_PROP_CIC2),
};

static struct attribute *mt6379_sysfs_attrs[GAUGE_PROP_MAX + 1];

static const struct attribute_group mt6379_sysfs_attr_group = {
	.attrs = mt6379_sysfs_attrs,
};

static void mt6379_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(mt6379_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		mt6379_sysfs_attrs[i] = &mt6379_sysfs_field_tbl[i].attr.attr;

	mt6379_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int mt6379_sysfs_create_group(struct mtk_gauge *gauge)
{
	mt6379_sysfs_init_attrs();

	return sysfs_create_group(&gauge->psy->dev.kobj, &mt6379_sysfs_attr_group);
}

signed int battery_meter_meta_tool_cali_car_tune(struct mtk_battery *gm, int meta_current)
{
	struct mtk_gauge *gauge = gm->gauge;
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int cali_car_tune = 0;

	if (meta_current == 0)
		return gm->fg_cust_data.car_tune_value * 10;

	gm->gauge->hw_status.meta_current = meta_current;
	bm_err(gm, "%s, %s meta_current=%d\n", __func__, priv->desc->gauge_name, meta_current);

	calculate_car_tune(gm->gauge);
	cali_car_tune = gm->gauge->hw_status.tmp_car_tune;

	bm_err(gm, "%s, %s cali_car_tune=%d\n", __func__, priv->desc->gauge_name, cali_car_tune);

	return cali_car_tune;		/* 1000 base */
}

#if IS_ENABLED(CONFIG_COMPAT)
static const char *get_cmd_name(unsigned int cmd)
{
	switch (cmd) {
	case Get_META_BAT_VOL:
		return "GET_META_BAT_VOL";
	case Get_META_BAT_SOC:
		return "GET_META_BAT_SOC";
	case Get_META_BAT_CAR_TUNE_VALUE:
		return "GET_META_BAT_CAR_TUNE_VALUE";
	case Set_META_BAT_CAR_TUNE_VALUE:
		return "SET_META_BAT_CAR_TUNE_VALUE";
	case Set_BAT_DISABLE_NAFG:
		return "SET_BAT_DISABLE_NAFG";
	case Set_CARTUNE_TO_KERNEL:
		return "SET_CAR_TUNE_TO_KERNEL";
	default:
		return "ERROR_UNKNOWN_CMD";
	}
}

static long compat_adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mt6379_priv *priv = file->private_data;
	struct mtk_battery *null_gm = NULL;
	int adc_out_datas[2] = { 1, 1 };

	bm_notice(null_gm, "%s, %s 32bit IOCTL, cmd=0x%08x (%s)\n",
		  __func__, priv->desc->gauge_name, cmd, get_cmd_name(cmd));
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		bm_err(null_gm, "%s, %s file has no f_op or no f_op->unlocked_ioctl.\n",
		       __func__, priv->desc->gauge_name);
		return -ENOTTY;
	}

	if (sizeof(arg) != sizeof(adc_out_datas))
		return -EFAULT;

	switch (cmd) {
	case Get_META_BAT_VOL:
	case Get_META_BAT_SOC:
	case Get_META_BAT_CAR_TUNE_VALUE:
	case Set_META_BAT_CAR_TUNE_VALUE:
	case Set_BAT_DISABLE_NAFG:
	case Set_CARTUNE_TO_KERNEL:
		bm_notice(null_gm, "%s, %s send to unlocked_ioctl cmd=0x%08x (%s)\n",
			  __func__, priv->desc->gauge_name, cmd, get_cmd_name(cmd));
		return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
	default:
		bm_err(null_gm, "%s, %s unknown IOCTL: 0x%08x (%s), %d\n",
		       __func__, priv->desc->gauge_name, cmd, get_cmd_name(cmd), adc_out_datas[0]);
		return 0;
	}
}
#endif

static long adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mt6379_priv *priv = file->private_data;
	struct mtk_battery *gm = priv->gauge.gm;
	int temp_car_tune, ret = 0, isdisNAFG = 0;
	int adc_out_data[2] = { 1, 1 };
	int adc_in_data[2] = { 1, 1 };
	int *user_data_addr;

	mutex_lock(&gm->gauge->fg_mutex);
	user_data_addr = (int *)arg;
	ret = copy_from_user(adc_in_data, user_data_addr, sizeof(adc_in_data));
	if (adc_in_data[1] < 0) {
		bm_err(gm, "%s, %s unknown data: %d\n",
		       __func__, priv->desc->gauge_name, adc_in_data[1]);
		mutex_unlock(&gm->gauge->fg_mutex);
		return -EFAULT;
	}

	switch (cmd) {
	case Get_META_BAT_VOL:
		adc_out_data[0] = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_VOLTAGE);

		if (copy_to_user(user_data_addr, adc_out_data, sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_notice(gm, "%s, **** unlocked_ioctl: %s Get_META_BAT_VOL Done!\n",
			  __func__, priv->desc->gauge_name);
		break;
	case Get_META_BAT_SOC:
		adc_out_data[0] = gm->ui_soc;

		if (copy_to_user(user_data_addr, adc_out_data, sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_notice(gm, "%s, **** unlocked_ioctl: %s Get_META_BAT_SOC Done!\n",
			  __func__, priv->desc->gauge_name);
		break;

	case Get_META_BAT_CAR_TUNE_VALUE:
		adc_out_data[0] = gm->fg_cust_data.car_tune_value;
		bm_err(gm, "%s, %s Get_BAT_CAR_TUNE_VALUE, res=%d\n",
		       __func__, priv->desc->gauge_name, adc_out_data[0]);

		if (copy_to_user(user_data_addr, adc_out_data, sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_notice(gm, "%s, **** unlocked_ioctl: %s Get_META_BAT_CAR_TUNE_VALUE Done!\n",
			  __func__, priv->desc->gauge_name);
		break;
	case Set_META_BAT_CAR_TUNE_VALUE:
		/* meta tool input: adc_in_data[1] (mA)*/
		/* Send cali_current to hal to calculate car_tune_value*/
		temp_car_tune =
			battery_meter_meta_tool_cali_car_tune(gm, adc_in_data[1]);

		/* return car_tune_value to meta tool in adc_out_data[0] */
		if (temp_car_tune >= 900 && temp_car_tune <= 1100)
			gm->fg_cust_data.car_tune_value = temp_car_tune;
		else
			bm_err(gm, "%s, %s car_tune_value invalid:%d\n",
			       __func__, priv->desc->gauge_name, temp_car_tune);

		adc_out_data[0] = temp_car_tune;

		if (copy_to_user(user_data_addr, adc_out_data, sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_err(gm, "%s, **** unlocked_ioctl: %s Set_BAT_CAR_TUNE_VALUE[%d], tmp_car_tune=%d result=%d, ret=%d\n",
		       __func__, priv->desc->gauge_name, adc_in_data[1], adc_out_data[0], temp_car_tune, ret);

		break;
	case Set_BAT_DISABLE_NAFG:
		isdisNAFG = adc_in_data[1];

		if (isdisNAFG == 1) {
			gm->cmd_disable_nafg = true;
			wakeup_fg_algo_cmd(gm, FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_DISABLE_NAFG, 1);
		} else if (isdisNAFG == 0) {
			gm->cmd_disable_nafg = false;
			wakeup_fg_algo_cmd(gm, FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_DISABLE_NAFG, 0);
		}

		bm_debug(gm, "%s, unlocked_ioctl: %s Set_BAT_DISABLE_NAFG,isdisNAFG=%d [%d]\n",
			 __func__, priv->desc->gauge_name, isdisNAFG, adc_in_data[1]);
		break;
	case Set_CARTUNE_TO_KERNEL:
		temp_car_tune = adc_in_data[1];
		if (temp_car_tune > 500 && temp_car_tune < 1500)
			gm->fg_cust_data.car_tune_value = temp_car_tune;

		bm_err(gm, "%s, **** unlocked_ioctl: %s Set_CARTUNE_TO_KERNEL[%d,%d], ret=%d\n",
		       __func__, priv->desc->gauge_name, adc_in_data[0], adc_in_data[1], ret);
		break;
	default:
		bm_err(gm, "%s, **** unlocked_ioctl: %s unknown IOCTL: 0x%08x\n",
		       __func__, priv->desc->gauge_name, cmd);
		mutex_unlock(&gm->gauge->fg_mutex);
		return -EINVAL;
	}

	mutex_unlock(&gm->gauge->fg_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	struct mt6379_fg_info_data *fg_info = container_of(inode->i_cdev,
							   struct mt6379_fg_info_data,
							   bat_cali_cdev);
	struct mt6379_priv *priv = container_of(fg_info, struct mt6379_priv, fg_info);

	file->private_data = priv;
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_adc_cali_ioctl,
#endif
	.open = adc_cali_open,
	.release = adc_cali_release,
};

static int adc_cali_cdev_init(struct mtk_battery *gm, struct platform_device *pdev)
{
	struct mtk_gauge *gauge = gm->gauge;
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret = 0;

	mutex_init(&gauge->fg_mutex);

	ret = alloc_chrdev_region(&(priv->fg_info.bat_cali_devno), 0, 1,
				  priv->desc->cdev_gauge_name);
	if (ret)
		bm_err(gauge->gm, "%s, Error: %s Can't Get Major number for adc_cali\n",
		       __func__, priv->desc->gauge_name);

	cdev_init(&(priv->fg_info.bat_cali_cdev), &adc_cali_fops);
	priv->fg_info.bat_cali_cdev.owner = THIS_MODULE;
	ret = cdev_add(&(priv->fg_info.bat_cali_cdev), priv->fg_info.bat_cali_devno, 1);
	if (ret)
		bm_err(gauge->gm, "%s, %s adc_cali Error: cdev_add\n",
		       __func__, priv->desc->gauge_name);

	priv->fg_info.bat_cali_major = MAJOR(priv->fg_info.bat_cali_devno);
	priv->fg_info.bat_cali_class = class_create(priv->desc->cdev_gauge_name);
	priv->fg_info.cdev_dev = device_create(priv->fg_info.bat_cali_class, NULL,
						priv->fg_info.bat_cali_devno, NULL,
						"%s", priv->desc->cdev_gauge_name);
	if (IS_ERR(priv->fg_info.cdev_dev)) {
		dev_info(priv->dev, "%s, Failed to create %s cdev_dev\n",
			 __func__, priv->desc->gauge_name);
		cdev_del(&priv->fg_info.bat_cali_cdev);
		return PTR_ERR(priv->fg_info.cdev_dev);
	}

	return 0;
}

static enum power_supply_property gauge_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	//POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
};

static int get_ptim_current(struct mtk_gauge *gauge)
{
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	const unsigned int bat_idx = priv->desc->bat_idx;
	int dvalue, r_fg_value, car_tune_value;
	u16 reg_value = 0;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	regmap_raw_read(gauge->regmap, rg[bat_idx][MT6379_REG_FGADC_R_CON0],
			&reg_value, sizeof(reg_value));

	dvalue = reg_to_current(gauge, reg_value);

	/* Auto adjust value */
	if (r_fg_value != priv->default_r_fg && r_fg_value != 0)
		dvalue = (dvalue * priv->default_r_fg) / r_fg_value;

	dvalue = ((dvalue * car_tune_value) / 1000);

	/* ptim current > 0 means discharge, different to bat_current */
	dvalue = dvalue * -1;
	bm_debug(gauge->gm, "[%s] %s ptim current:%d\n", __func__, priv->desc->gauge_name, dvalue);

	return dvalue;
}

static int psy_gauge_get_property(struct power_supply *psy, enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct mtk_gauge *gauge = power_supply_get_drvdata(psy);
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	int ret = 0, value = 0;
	struct mtk_battery *gm;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		/* store disableGM30 status to mtk-gauge psy for DLPT */
		if (!gauge || !gauge->gm)
			val->intval = 0;
		else
			val->intval = gauge->gm->disableGM30;

		return 0;
	case POWER_SUPPLY_PROP_ONLINE:
		if (!gauge || !gauge->gm)
			val->intval = 0;
		else
			val->intval = gauge->gm->disableGM30;

		return 0;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = get_ptim_current(gauge);
		return 0;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = gauge_get_property(gauge->gm, GAUGE_PROP_BATTERY_CURRENT, &value);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to get %s CIC1, ret = %d\n",
				 __func__, priv->desc->gauge_name, ret);
			value = gauge->gm->ibat;
		}
		val->intval = value * 100;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (!gauge || !gauge->gm || gauge->gm->disableGM30)
			val->intval = 4000 * 1000;
		else
			val->intval = gauge_get_int_property(gauge->gm,
							     GAUGE_PROP_BATTERY_VOLTAGE) * 1000;
		return 0;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
#ifdef POWER_MISC_OFF
		gm = gauge->gm;
		if (gm)
			val->intval = gm->sdc.shutdown_status.is_dlpt_shutdown;
#else	/* POWER_MISC_OFF */
		val->intval = 0;
#endif	/* POWER_MISC_OFF */
		return 0;
	case POWER_SUPPLY_PROP_TEMP:
		gm = gauge->gm;
		if (gm)
			val->intval = gm->battery_temp * 10;

		return 0;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		gm = gauge->gm;
		if (gm)
			val->intval = gm->daemon_data.qmxa_t_0ma;

		return 0;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		gm = gauge->gm;
		if (gm)
			val->intval = gm->daemon_data.quse;

		return 0;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		gm = gauge->gm;
		if (gm)
			val->intval = gm->daemon_data.soc;

		return 0;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		gm = gauge->gm;
		if (gm)
			val->intval = gm->daemon_data.uisoc;

		return 0;
	default:
		return -EINVAL;
	}
}

static int psy_gauge_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct mtk_gauge *gauge = power_supply_get_drvdata(psy);
	struct mt6379_priv *priv = container_of(gauge, struct mt6379_priv, gauge);
	struct mtk_battery *gm = gauge->gm;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_notice("%s: %d %d\n", __func__, psp, val->intval);
		return 0;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		if (gm && val->intval == 1)
			set_shutdown_cond(gm, DLPT_SHUTDOWN);

		return 0;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN:
		if (gm && val->intval != 0) {
			gm->imix = val->intval;
			if (gm->imix > 5500) {
				gm->imix = 5500;
				dev_info(priv->dev, "%s, %s imix reach limitation 5500, val:%d\n",
					 __func__, priv->desc->gauge_name, val->intval);
			}
		}

		return 0;
	default:
		return -EINVAL;
	}
}

static int mtk_gauge_proprietary_init(struct mt6379_priv *priv)
{
	struct mtk_gauge *gauge = &priv->gauge;

	/* Variable initialization */
	gauge->regmap = priv->regmap;
	gauge->regmap_type = REGMAP_TYPE_SPMI;
	gauge->pdev = to_platform_device(priv->dev);
	mutex_init(&gauge->ops_lock);
	gauge->hw_status.car_tune_value = 1000;
	gauge->attr = mt6379_sysfs_field_tbl;

	dev_notice(priv->dev, "%s, %s before battery_psy_init (1)\n", __func__, priv->desc->gauge_name);
	if (battery_psy_init(gauge->pdev))
		return -ENOMEM;

	dev_notice(priv->dev, "%s, %s before setting psy config (2)\n", __func__, priv->desc->gauge_name);
	gauge->psy_desc.name = priv->desc->psy_desc_name;
	gauge->name = priv->desc->mtk_gauge_name;
	gauge->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	gauge->psy_desc.properties = gauge_properties;
	gauge->psy_desc.num_properties = ARRAY_SIZE(gauge_properties);
	gauge->psy_desc.get_property = psy_gauge_get_property;
	gauge->psy_desc.set_property = psy_gauge_set_property;
	gauge->psy_cfg.drv_data = gauge;
	gauge->psy = power_supply_register(priv->dev, &gauge->psy_desc, &gauge->psy_cfg);
	if (IS_ERR(gauge->psy))
		return PTR_ERR(gauge->psy);

	dev_notice(priv->dev, "%s, %s before creating sysfs (3)\n", __func__, priv->desc->gauge_name);
	mt6379_sysfs_create_group(gauge);
	initial_set(gauge, 0, 0);
	battery_init(gauge->pdev);

	priv->fg_info.zcv_intr_en_cnt = 0;
	priv->fg_info.nafg_en_cnt = 0;
	priv->fg_info.en_l_vbat_cnt = 0;
	priv->fg_info.bat_vol_get_t1 = 0;
	priv->latch_timeout_cnt = 0;
	priv->latch_spmi_timeout_cnt = 0;

	adc_cali_cdev_init(gauge->gm, gauge->pdev);

	return mt6379_reload_fg_efuse_gain_err(priv);
}

static int mt6379_gauge_refactor_unit(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	const int r_fg_val[] = { 50, 20, 10, 5 };
	struct device *dev = priv->dev;
	u32 regval = 0;
	int ret;

	priv->unit_fgcurrent = MT6379_UNIT_FGCURRENT;
	priv->unit_charge = MT6379_UNIT_CHARGE;
	priv->unit_fg_iavg = MT6379_UNIT_FG_IAVG;
	priv->unit_fgcar_zcv = MT6379_UNIT_FGCAR_ZCV;

	/* Get default r_fg gain error selection, should be set in LK */
	ret = regmap_read(priv->regmap, rg[bat_idx][MT6379_REG_FGADC_ANA_ELR4],
			  &regval);
	if (ret)
		return ret;

	regval &= FG_GAINERR_SEL_MASK;
	priv->default_r_fg = r_fg_val[regval];

	ret = device_property_read_u32(dev, "r-fg-value", &priv->dts_r_fg);
	if (ret) {
		dev_notice(dev, "%s: Failed to parse %s dt (ret:%d)\n",
			   __func__, priv->desc->gauge_name, ret);
		return ret;
	}

	/* Avoid decimal point problems, so multiply by 10 (Like LK2) */
	priv->dts_r_fg *= 10;

	dev_info(dev, "%s, %s r_fg(unit:0.1mOhm):(lk2_set:%d, dts:%d), unit_fg_current:%d, unit_charge:%d, unit_fg_iavg:%d, unit_fgcar_zcv:%d\n",
		 __func__, priv->desc->gauge_name, priv->default_r_fg, priv->dts_r_fg,
		 priv->unit_fgcurrent, priv->unit_charge, priv->unit_fg_iavg, priv->unit_fgcar_zcv);
	return 0;
}

static const struct gauge_chip_desc mt6379_bat1_desc = {
	.bat_idx = 0,
	.psy_desc_name = "mt6379-gauge1",
	.mtk_gauge_name = "fgauge1",
	.gauge_path_name = "gauge",
	.gauge_name = "MT6379 GAUGE BAT1",
	.cdev_gauge_name = "MT_pmic_adc_cali",
	.mask_gm30_evt = MT6379_MASK_BM1_EVT,
};

static const struct gauge_chip_desc mt6379_bat2_desc = {
	.bat_idx = 1,
	.psy_desc_name = "mt6379-gauge2",
	.mtk_gauge_name = "fgauge2",
	.gauge_path_name = "gauge2",
	.gauge_name = "MT6379 GAUGE BAT2",
	.cdev_gauge_name = "MT_pmic_adc_cali2",
	.mask_gm30_evt = MT6379_MASK_BM2_EVT,
};

static int mt6379_check_bat_cell_count(struct mt6379_priv *priv)
{
	const unsigned int bat_idx = priv->desc->bat_idx;
	unsigned int val;
	int ret;

	ret = regmap_read(priv->regmap, MT6379_REG_CORE_CTRL0, &val);
	if (ret) {
		dev_info(priv->dev, "Failed to read CORE_CTRL0\n");
		return ret;
	}

	priv->using_2p = FIELD_GET(MT6379_MASK_CELL_COUNT, val);

	/* if BAT1, skip this checking */
	if (bat_idx > 0 && !priv->using_2p) {
		dev_info(priv->dev, "HW not support!\n");
		return -ENODEV;
	}

	return 0;
}

static int mt6379_gauge_probe(struct platform_device *pdev)
{
	struct mt6379_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->desc = device_get_match_data(&pdev->dev);

	mutex_init(&priv->irq_lock);
	mutex_init(&priv->baton_lock);
	platform_set_drvdata(pdev, priv);

	ret = sysfs_create_link(kernel_kobj, &pdev->dev.kobj, priv->desc->gauge_path_name);
	if (ret)
		dev_info(&pdev->dev, "Failed to link\n");

	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap) {
		dev_info(&pdev->dev, "Failed to get regmap\n");
		return -ENODEV;
	}

	ret = mt6379_check_bat_cell_count(priv);
	if (ret)
		return ret;

	ret = mt6379_gauge_refactor_unit(priv);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to refactor unit\n");
		return ret;
	}

	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0) {
		dev_info(&pdev->dev, "Failed to get gm30 irq\n");
		return priv->irq;
	}

	ret = gauge_add_irq_chip(priv);
	if (ret) {
		dev_info(&pdev->dev, "Failed to add irq chip\n");
		return ret;
	}

	ret = gauge_get_all_auxadc_channels(priv);
	if (ret) {
		dev_info(&pdev->dev, "Failed to get all auxadc\n");
		goto out_irq_chip;
	}

	ret = mt6379_auxadc_init_vbat_calibration(priv);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to init vbat calibration\n");
		goto out_irq_chip;
	}

	ret = gauge_get_all_interrupts(priv);
	if (ret) {
		dev_info(&pdev->dev, "Failed to get all interrupts\n");
		goto out_irq_chip;
	}

	ret = mtk_gauge_proprietary_init(priv);
	if (ret) {
		dev_info(&pdev->dev, "Failed to do mtk gauge init\n");
		goto out_irq_chip;
	}

	return 0;

out_irq_chip:
	gauge_del_irq_chip(priv);
	return ret;
}

static int mt6379_gauge_remove(struct platform_device *pdev)
{
	struct mt6379_priv *priv = platform_get_drvdata(pdev);

	gauge_del_irq_chip(priv);
	return 0;
}

static void mt6379_gauge_shutdown(struct platform_device *pdev)
{
	struct mt6379_priv *priv = platform_get_drvdata(pdev);
	struct mtk_battery *gm = priv->gauge.gm;

	if (gm->shutdown)
		gm->shutdown(gm);
}

static int __maybe_unused mt6379_gauge_suspend(struct device *dev)
{
	struct mt6379_priv *priv = dev_get_drvdata(dev);
	struct mtk_battery *gm = priv->gauge.gm;
	pm_message_t state = { .event = 0, };

	if (gm->suspend)
		gm->suspend(gm, state);

	return 0;
}

static int __maybe_unused mt6379_gauge_resume(struct device *dev)
{
	struct mt6379_priv *priv = dev_get_drvdata(dev);
	struct mtk_battery *gm = priv->gauge.gm;

	if (gm->resume)
		gm->resume(gm);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6379_gauge_pm_ops, mt6379_gauge_suspend, mt6379_gauge_resume);

static const struct of_device_id mt6379_gauge_of_match[] = {
	{ .compatible = "mediatek,mt6379-gauge-1", .data = (void *)&mt6379_bat1_desc, },
	{ .compatible = "mediatek,mt6379-gauge-2", .data = (void *)&mt6379_bat2_desc, },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6379_gauge_of_match);

static struct platform_driver mt6379_gauge_driver = {
	.probe = mt6379_gauge_probe,
	.remove = mt6379_gauge_remove,
	.shutdown = mt6379_gauge_shutdown,
	.driver = {
		.name = "mt6379-gauge",
		.pm = &mt6379_gauge_pm_ops,
		.of_match_table = mt6379_gauge_of_match,
	},
};
module_platform_driver(mt6379_gauge_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_AUTHOR("ChiaEn Wu <chiaen_wu@richtek.com>");
MODULE_DESCRIPTION("MediaTek MT6379 Fuel Gauge Driver");
MODULE_LICENSE("GPL");
