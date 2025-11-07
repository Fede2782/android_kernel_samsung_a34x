/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "rlm_tx_pwr_config_data_emi_pri_ch.h"
 *    \brief
 */

#ifndef _RLM_TXPWR_CONFIG_DATA_EMI_PRI_CH_H
#define _RLM_TXPWR_CONFIG_DATA_EMI_PRI_CH_H

#if (CFG_SUPPORT_PWR_LMT_EMI == 1)
#if (COUNTRY_CHANNEL_TXPOWER_LIMIT_CHANNEL_DEFINE == 1)

#include "rlm_txpwr_limit_emi.h"
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/****************************************************************************
 *                            Configuration (2G/5G)
 ***************************************************************************/
struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_LEGACY
	g_rRlmPowerLimitConfigurationLegacy[] = {
		/*Default*/
	{	{0, 0}
		, 165, {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63}
	}
};

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_HE
	g_rRlmPowerLimitConfigurationHE[] = {
	/*Default*/
	{	{0, 0}
		, 36,
			{64, 64, 64, /* RU26 L,H,U */
			64, 64, 64,  /* RU52 L,H,U*/
			64, 64, 64,  /* RU106 L,H,U*/
			64, 64, 64,  /* RU242 L,H,U*/
			64, 64, 64,  /* RU484 L,H,U*/
			64, 64, 64,  /* RU996 L,H,U*/
			64, 64, 64}  /* RU1992 L,H,U*/
	}
};

#if (CFG_SUPPORT_PWR_LIMIT_EHT == 1)
/* For EHT 2.4G & 5G setting */
struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_EHT
	g_rRlmPowerLimitConfigurationEHT[] = {
	/*Default*/
	{	{0, 0}
		, 1,
			{64, 64, 64, /* EHT26 L,H,U */
			64, 64, 64,  /* EHT52 L,H,U*/
			64, 64, 64,  /* EHT106 L,H,U*/
			64, 64, 64,  /* EHT242 L,H,U*/
			64, 64, 64,  /* EHT484 L,H,U*/
			64, 64, 64,  /* EHT996 L,H,U*/
			64, 64, 64,  /* EHT996X2 L,H,U*/
			64, 64, 64,  /* EHT996X4 L,H,U*/
			64, 64, 64,  /* EHT26_52 L,H,U*/
			64, 64, 64,  /* EHT26_106 L,H,U*/
			64, 64, 64,  /* EHT484_242 L,H,U*/
			64, 64, 64,  /* EHT996_484 L,H,U*/
			64, 64, 64,  /* EHT996_484_242 L,H,U*/
			64, 64, 64,  /* EHT996X2_484 L,H,U*/
			64, 64, 64,  /* EHT996X3 L,H,U*/
			64, 64, 64}  /* EHT996X3_484 L,H,U*/
	}
};
#endif /* CFG_SUPPORT_PWR_LIMIT_EHT */

/****************************************************************************
 *                            Configuration (6G)
 ***************************************************************************/

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_LEGACY
	g_rRlmPowerLimitConfigurationLegacy_6G[] = {
	/*Default*/
	{	{0, 0}
		, 36, {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64}
	}
};

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_HE
	g_rRlmPowerLimitConfigurationHE_6G[] = {
	/*Default*/
	{	{0, 0}
		, 36,
			{64, 64, 64, /* RU26 L,H,U */
			64, 64, 64,  /* RU52 L,H,U*/
			64, 64, 64,  /* RU106 L,H,U*/
			64, 64, 64,  /* RU242 L,H,U*/
			64, 64, 64,  /* RU484 L,H,U*/
			64, 64, 64,  /* RU996 L,H,U*/
			64, 64, 64}  /* RU1992 L,H,U*/
	}
};

#if (CFG_SUPPORT_PWR_LIMIT_EHT == 1)
struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_EHT
	g_rRlmPowerLimitConfigurationEHT_6G[] = {
	/*Default*/
	{	{0, 0}
		, 1,
			{64, 64, 64, /* EHT26 L,H,U */
			64, 64, 64,  /* EHT52 L,H,U*/
			64, 64, 64,  /* EHT106 L,H,U*/
			64, 64, 64,  /* EHT242 L,H,U*/
			64, 64, 64,  /* EHT484 L,H,U*/
			64, 64, 64,  /* EHT996 L,H,U*/
			64, 64, 64,  /* EHT996X2 L,H,U*/
			64, 64, 64,  /* EHT996X4 L,H,U*/
			64, 64, 64,  /* EHT26_52 L,H,U*/
			64, 64, 64,  /* EHT26_106 L,H,U*/
			64, 64, 64,  /* EHT484_242 L,H,U*/
			64, 64, 64,  /* EHT996_484 L,H,U*/
			64, 64, 64,  /* EHT996_484_242 L,H,U*/
			64, 64, 64,  /* EHT996X2_484 L,H,U*/
			64, 64, 64,  /* EHT996X3 L,H,U*/
			64, 64, 64}  /* EHT996X3_484 L,H,U*/
	}
};
#endif /* CFG_SUPPORT_PWR_LIMIT_EHT */

/****************************************************************************
 *                            Configuration (6G_VLP)
 ***************************************************************************/
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_LEGACY
	g_rRlmPowerLimitConfigurationLegacy_6G_VLP[] = {
	/*Default*/
	{	{0, 0}
		, 1, {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64}
	}
};

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_HE
	g_rRlmPowerLimitConfigurationHE_6G_VLP[] = {
	/*Default*/
	{	{0, 0}
		, 1,
			{64, 64, 64, /* RU26 L,H,U */
			64, 64, 64,  /* RU52 L,H,U*/
			64, 64, 64,  /* RU106 L,H,U*/
			64, 64, 64,  /* RU242 L,H,U*/
			64, 64, 64,  /* RU484 L,H,U*/
			64, 64, 64,  /* RU996 L,H,U*/
			64, 64, 64}  /* RU1992 L,H,U*/
	}
};

#if (CFG_SUPPORT_PWR_LIMIT_EHT == 1)
struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_EHT
	g_rRlmPowerLimitConfigurationEHT_6G_VLP[] = {
	/*Default*/
	{	{0, 0}
		, 1,
			{64, 64, 64, /* EHT26 L,H,U */
			64, 64, 64,  /* EHT52 L,H,U*/
			64, 64, 64,  /* EHT106 L,H,U*/
			64, 64, 64,  /* EHT242 L,H,U*/
			64, 64, 64,  /* EHT484 L,H,U*/
			64, 64, 64,  /* EHT996 L,H,U*/
			64, 64, 64,  /* EHT996X2 L,H,U*/
			64, 64, 64,  /* EHT996X4 L,H,U*/
			64, 64, 64,  /* EHT26_52 L,H,U*/
			64, 64, 64,  /* EHT26_106 L,H,U*/
			64, 64, 64,  /* EHT484_242 L,H,U*/
			64, 64, 64,  /* EHT996_484 L,H,U*/
			64, 64, 64,  /* EHT996_484_242 L,H,U*/
			64, 64, 64,  /* EHT996X2_484 L,H,U*/
			64, 64, 64,  /* EHT996X3 L,H,U*/
			64, 64, 64}  /* EHT996X3_484 L,H,U*/
	}
};
#endif /* CFG_SUPPORT_PWR_LIMIT_EHT */

/****************************************************************************
 *                            Configuration (6G_SP)
 ***************************************************************************/

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_LEGACY
	g_rRlmPowerLimitConfigurationLegacy_6G_SP[] = {
	/*Default*/
	{	{0, 0}
		, 1, {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64}
	}
};

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_HE
	g_rRlmPowerLimitConfigurationHE_6G_SP[] = {
	/*Default*/
	{	{0, 0}
		, 1,
			{64, 64, 64, /* RU26 L,H,U */
			64, 64, 64,  /* RU52 L,H,U*/
			64, 64, 64,  /* RU106 L,H,U*/
			64, 64, 64,  /* RU242 L,H,U*/
			64, 64, 64,  /* RU484 L,H,U*/
			64, 64, 64,  /* RU996 L,H,U*/
			64, 64, 64}  /* RU1992 L,H,U*/
	}
};

#if (CFG_SUPPORT_PWR_LIMIT_EHT == 1)
struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_EHT
	g_rRlmPowerLimitConfigurationEHT_6G_SP[] = {
	/*Default*/
	{	{0, 0}
		, 1,
			{64, 64, 64, /* EHT26 L,H,U */
			64, 64, 64,  /* EHT52 L,H,U*/
			64, 64, 64,  /* EHT106 L,H,U*/
			64, 64, 64,  /* EHT242 L,H,U*/
			64, 64, 64,  /* EHT484 L,H,U*/
			64, 64, 64,  /* EHT996 L,H,U*/
			64, 64, 64,  /* EHT996X2 L,H,U*/
			64, 64, 64,  /* EHT996X4 L,H,U*/
			64, 64, 64,  /* EHT26_52 L,H,U*/
			64, 64, 64,  /* EHT26_106 L,H,U*/
			64, 64, 64,  /* EHT484_242 L,H,U*/
			64, 64, 64,  /* EHT996_484 L,H,U*/
			64, 64, 64,  /* EHT996_484_242 L,H,U*/
			64, 64, 64,  /* EHT996X2_484 L,H,U*/
			64, 64, 64,  /* EHT996X3 L,H,U*/
			64, 64, 64}  /* EHT996X3_484 L,H,U*/
	}
};
#endif /* CFG_SUPPORT_PWR_LIMIT_EHT */
#endif /*(CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)*/
#endif /* COUNTRY_CHANNEL_TXPOWER_LIMIT_CHANNEL_DEFINE == 1 */
#endif /*CFG_SUPPORT_PWR_LMT_EMI == 1*/
#endif /*_RLM_TXPWR_CONFIG_DATA_EMI_PRI_CH_H*/
