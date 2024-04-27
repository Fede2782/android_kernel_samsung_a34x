/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
/*! \file   "rlm_channel.h"
 *    \brief
 */


#ifndef _RLM_CHANNEL_H
#define _RLM_CHANNEL_H

/*******************************************************************************
 *       C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *  E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *   C O N S T A N T S
 *******************************************************************************
 */


/*******************************************************************************
 *  D A T A   T Y P E S
 *******************************************************************************
 */

/* The following country or domain shall be set from host driver.
 * And host driver should pass specified DOMAIN_INFO_ENTRY to MT6620 as
 * the channel list of being a STA to do scanning/searching AP or being an
 * AP to choose an adequate channel if auto-channel is set.
 */

/* Define mapping tables between country code and its channel set
 */
static const uint16_t g_u2CountryGroup0[] = {
	COUNTRY_CODE_AD, COUNTRY_CODE_AF, COUNTRY_CODE_AL, COUNTRY_CODE_AO,
	COUNTRY_CODE_AT, COUNTRY_CODE_AZ, COUNTRY_CODE_BA, COUNTRY_CODE_BE,
	COUNTRY_CODE_BF, COUNTRY_CODE_BG, COUNTRY_CODE_BI, COUNTRY_CODE_BJ,
	COUNTRY_CODE_BT, COUNTRY_CODE_BW, COUNTRY_CODE_CD, COUNTRY_CODE_CF,
	COUNTRY_CODE_CG, COUNTRY_CODE_CH, COUNTRY_CODE_CI, COUNTRY_CODE_CM,
	COUNTRY_CODE_CV, COUNTRY_CODE_CY, COUNTRY_CODE_CZ, COUNTRY_CODE_DE,
	COUNTRY_CODE_DJ, COUNTRY_CODE_DK, COUNTRY_CODE_EE, COUNTRY_CODE_ES,
	COUNTRY_CODE_FI, COUNTRY_CODE_FO, COUNTRY_CODE_FR, COUNTRY_CODE_GA,
	COUNTRY_CODE_GE, COUNTRY_CODE_GF, COUNTRY_CODE_GG, COUNTRY_CODE_GL,
	COUNTRY_CODE_GM, COUNTRY_CODE_GN, COUNTRY_CODE_GP, COUNTRY_CODE_GQ,
	COUNTRY_CODE_GR, COUNTRY_CODE_GW, COUNTRY_CODE_HR, COUNTRY_CODE_HU,
	COUNTRY_CODE_IE, COUNTRY_CODE_IM, COUNTRY_CODE_IQ, COUNTRY_CODE_IS,
	COUNTRY_CODE_IT, COUNTRY_CODE_JE, COUNTRY_CODE_KE, COUNTRY_CODE_KM,
	COUNTRY_CODE_KW, COUNTRY_CODE_LB, COUNTRY_CODE_LI, COUNTRY_CODE_LS,
	COUNTRY_CODE_LT, COUNTRY_CODE_LU, COUNTRY_CODE_LV, COUNTRY_CODE_LY,
	COUNTRY_CODE_MC, COUNTRY_CODE_MD, COUNTRY_CODE_ME, COUNTRY_CODE_MK,
	COUNTRY_CODE_ML, COUNTRY_CODE_MQ, COUNTRY_CODE_MR, COUNTRY_CODE_MT,
	COUNTRY_CODE_MU, COUNTRY_CODE_MZ, COUNTRY_CODE_NC, COUNTRY_CODE_NE,
	COUNTRY_CODE_NL, COUNTRY_CODE_NO, COUNTRY_CODE_NR, COUNTRY_CODE_PF,
	COUNTRY_CODE_PL, COUNTRY_CODE_PM, COUNTRY_CODE_PT, COUNTRY_CODE_RE,
	COUNTRY_CODE_RO, COUNTRY_CODE_RS, COUNTRY_CODE_SE, COUNTRY_CODE_SI,
	COUNTRY_CODE_SK, COUNTRY_CODE_SM, COUNTRY_CODE_SO, COUNTRY_CODE_ST,
	COUNTRY_CODE_SZ, COUNTRY_CODE_TD, COUNTRY_CODE_TF, COUNTRY_CODE_TG,
	COUNTRY_CODE_TJ, COUNTRY_CODE_TM, COUNTRY_CODE_TR, COUNTRY_CODE_TV,
	COUNTRY_CODE_TZ, COUNTRY_CODE_VA, COUNTRY_CODE_WF, COUNTRY_CODE_XK,
	COUNTRY_CODE_YT, COUNTRY_CODE_ZM
};
static const uint16_t g_u2CountryGroup1[] = {
	COUNTRY_CODE_AE, COUNTRY_CODE_AG, COUNTRY_CODE_AI, COUNTRY_CODE_AM,
	COUNTRY_CODE_AN, COUNTRY_CODE_AQ, COUNTRY_CODE_AW, COUNTRY_CODE_AX,
	COUNTRY_CODE_BB, COUNTRY_CODE_BM, COUNTRY_CODE_BN, COUNTRY_CODE_BO,
	COUNTRY_CODE_BR, COUNTRY_CODE_BS, COUNTRY_CODE_BV, COUNTRY_CODE_BZ,
	COUNTRY_CODE_CK, COUNTRY_CODE_CO, COUNTRY_CODE_CR, COUNTRY_CODE_DO,
	COUNTRY_CODE_EC, COUNTRY_CODE_FJ, COUNTRY_CODE_FK, COUNTRY_CODE_FM,
	COUNTRY_CODE_GB, COUNTRY_CODE_GD, COUNTRY_CODE_GI, COUNTRY_CODE_GS,
	COUNTRY_CODE_GY, COUNTRY_CODE_HK, COUNTRY_CODE_HN, COUNTRY_CODE_HT,
	COUNTRY_CODE_IL, COUNTRY_CODE_IN, COUNTRY_CODE_IO, COUNTRY_CODE_IR,
	COUNTRY_CODE_KG, COUNTRY_CODE_KH, COUNTRY_CODE_KI, COUNTRY_CODE_KN,
	COUNTRY_CODE_KP, COUNTRY_CODE_KY, COUNTRY_CODE_KZ, COUNTRY_CODE_LA,
	COUNTRY_CODE_LC, COUNTRY_CODE_LK, COUNTRY_CODE_LR, COUNTRY_CODE_MH,
	COUNTRY_CODE_MN, COUNTRY_CODE_MO, COUNTRY_CODE_MS, COUNTRY_CODE_MW,
	COUNTRY_CODE_NA, COUNTRY_CODE_NI, COUNTRY_CODE_NU, COUNTRY_CODE_NZ,
	COUNTRY_CODE_PA, COUNTRY_CODE_PE, COUNTRY_CODE_PG, COUNTRY_CODE_PH,
	COUNTRY_CODE_PN, COUNTRY_CODE_PS, COUNTRY_CODE_PW, COUNTRY_CODE_PY,
	COUNTRY_CODE_QA, COUNTRY_CODE_RW, COUNTRY_CODE_SB, COUNTRY_CODE_SC,
	COUNTRY_CODE_SD, COUNTRY_CODE_SG, COUNTRY_CODE_SH, COUNTRY_CODE_SJ,
	COUNTRY_CODE_SN, COUNTRY_CODE_SS, COUNTRY_CODE_SV, COUNTRY_CODE_SX,
	COUNTRY_CODE_SY, COUNTRY_CODE_TC, COUNTRY_CODE_TH, COUNTRY_CODE_TK,
	COUNTRY_CODE_TL, COUNTRY_CODE_TO, COUNTRY_CODE_TT, COUNTRY_CODE_TW,
	COUNTRY_CODE_VC, COUNTRY_CODE_VG, COUNTRY_CODE_VN, COUNTRY_CODE_VU,
	COUNTRY_CODE_WS, COUNTRY_CODE_YE, COUNTRY_CODE_ZA,
};
static const uint16_t g_u2CountryGroup2[] = {
	COUNTRY_CODE_BY, COUNTRY_CODE_ET, COUNTRY_CODE_EU, COUNTRY_CODE_MF,
	COUNTRY_CODE_MG, COUNTRY_CODE_MM, COUNTRY_CODE_OM, COUNTRY_CODE_SL,
	COUNTRY_CODE_SR, COUNTRY_CODE_ZW
};
static const uint16_t g_u2CountryGroup3[] = {
	COUNTRY_CODE_CU, COUNTRY_CODE_DM, COUNTRY_CODE_GT
};

static const uint16_t g_u2CountryGroup4[] = {
	COUNTRY_CODE_AR, COUNTRY_CODE_AU, COUNTRY_CODE_CC, COUNTRY_CODE_CX,
	COUNTRY_CODE_HM, COUNTRY_CODE_MX, COUNTRY_CODE_NF
};
static const uint16_t g_u2CountryGroup5[] = {
	COUNTRY_CODE_BH, COUNTRY_CODE_CN, COUNTRY_CODE_MV, COUNTRY_CODE_UY,
	COUNTRY_CODE_VE
};
static const uint16_t g_u2CountryGroup6[] = {
	COUNTRY_CODE_BD, COUNTRY_CODE_JM, COUNTRY_CODE_PK
};
static const uint16_t g_u2CountryGroup7[] = {
	COUNTRY_CODE_NG
};
static const uint16_t g_u2CountryGroup8[] = {
	COUNTRY_CODE_CA
};
static const uint16_t g_u2CountryGroup9[] = {
	COUNTRY_CODE_ER, COUNTRY_CODE_RKS,

};
static const uint16_t g_u2CountryGroup10[] = {
	COUNTRY_CODE_DZ
};
static const uint16_t g_u2CountryGroup11[] = {
	COUNTRY_CODE_EG, COUNTRY_CODE_EH, COUNTRY_CODE_MA, COUNTRY_CODE_UZ
};
static const uint16_t g_u2CountryGroup12[] = {
	COUNTRY_CODE_JO
};
static const uint16_t g_u2CountryGroup13[] = {
	COUNTRY_CODE_JP
};
static const uint16_t g_u2CountryGroup14[] = {
	COUNTRY_CODE_MY
};
static const uint16_t g_u2CountryGroup15[] = {
	COUNTRY_CODE_RU
};
static const uint16_t g_u2CountryGroup16[] = {
	COUNTRY_CODE_GH, COUNTRY_CODE_SA, COUNTRY_CODE_UG
};
static const uint16_t g_u2CountryGroup17[] = {
	COUNTRY_CODE_TN
};
static const uint16_t g_u2CountryGroup18[] = {
	COUNTRY_CODE_UA
};
static const uint16_t g_u2CountryGroup19[] = {
	COUNTRY_CODE_CL, COUNTRY_CODE_KR
};
static const uint16_t g_u2CountryGroup20[] = {
	COUNTRY_CODE_AS, COUNTRY_CODE_GU, COUNTRY_CODE_MP, COUNTRY_CODE_PR,
	COUNTRY_CODE_UM, COUNTRY_CODE_US, COUNTRY_CODE_VI
};
static const uint16_t g_u2CountryGroup21[] = {
	COUNTRY_CODE_ID, COUNTRY_CODE_NP
};

struct DOMAIN_INFO_ENTRY arSupportedRegDomains[] = {
	{
		(uint16_t *) g_u2CountryGroup0, sizeof(g_u2CountryGroup0) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
			,	/*CH_SET_UNII_MID_100_140 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/*CH_SET_UNII_UPPER_149_165 */
			{125, BAND_NULL, 0, 0, 0, FALSE}
				/* CH_SET_UNII_UPPER_NA */
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup1, sizeof(g_u2CountryGroup1) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/* CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/* CH_SET_UNII_WW_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/* CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup2, sizeof(g_u2CountryGroup2) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,			/* CH_SET_2G4_1_13 */

			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,			/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,			/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
			,			/* CH_SET_UNII_WW_100_140 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup3, sizeof(g_u2CountryGroup3) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 11, FALSE}
			,	/* CH_SET_2G4_1_11 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/* CH_SET_UNII_WW_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/* CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup4, sizeof(g_u2CountryGroup4) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/* CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 5, TRUE}
			,	/* CH_SET_UNII_WW_100_116 */
			{121, BAND_5G, CHNL_SPAN_20, 132, 4, TRUE}
			,	/* CH_SET_UNII_WW_132_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
				/* CH_SET_UNII_UPPER_149_165 */
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup5, sizeof(g_u2CountryGroup5) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/* CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/* CH_SET_UNII_MID_52_64 */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/* CH_SET_UNII_WW_NA */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/* CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup6, sizeof(g_u2CountryGroup6) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_LOW_NA */
			{118, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_MID_NA */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_WW_NA */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/*CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup7, sizeof(g_u2CountryGroup7) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_LOW_NA */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_WW_NA */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/*CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup8, sizeof(g_u2CountryGroup8) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 11, FALSE}
			,	/*CH_SET_2G4_1_11 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 5, TRUE}
			,	/*CH_SET_UNII_MID_100_116 */
			{121, BAND_5G, CHNL_SPAN_20, 132, 4, TRUE}
			,	/*CH_SET_UNII_MID_132_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/*CH_SET_UNII_UPPER_149_165 */
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup9, sizeof(g_u2CountryGroup9) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_LOW_NA */
			{118, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_MID_NA */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_WW_NA */
			{125, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_UPPER_NA */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup10, sizeof(g_u2CountryGroup10) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 9, TRUE}
			,	/*CH_SET_UNII_MID_100_132 */
			{125, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_UPPER_NA */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup11, sizeof(g_u2CountryGroup11) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_4 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_4 */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_WW_NA */
			{125, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_UPPER_NA */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup12, sizeof(g_u2CountryGroup12) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_MID_NA */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_WW_NA */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/*CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup13, sizeof(g_u2CountryGroup13) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{82, BAND_2G4, CHNL_SPAN_5, 14, 1, FALSE}
			,	/*CH_SET_2G4_14_1 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/*CH_SET_UNII_MID_100_144 */
			{125, BAND_NULL, 0, 0, 0, FALSE}
				/*CH_SET_UNII_UPPER_NA */
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup14, sizeof(g_u2CountryGroup14) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 8, TRUE}
			,	/*CH_SET_UNII_MID_100_128 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/*CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup15, sizeof(g_u2CountryGroup15) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 132, 4, TRUE}
			,	/*CH_SET_UNII_MID_132_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
				/*CH_SET_UNII_UPPER_149_165 */
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup16, sizeof(g_u2CountryGroup16) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/*CH_SET_UNII_MID_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 4, FALSE}
			,	/*CH_SET_UNII_UPPER_149_161 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup17, sizeof(g_u2CountryGroup17) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 5, TRUE}
			,	/*CH_SET_UNII_MID_100_116 */
			{125, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_UPPER_NA */
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup18, sizeof(g_u2CountryGroup18) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/*CH_SET_UNII_MID_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/* CH_SET_UNII_UPPER_149_165 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup19, sizeof(g_u2CountryGroup19) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/* CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/* CH_SET_UNII_WW_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
			,	/* CH_SET_UNII_UPPER_149_165 */
#if (CFG_SUPPORT_WIFI_6G == 1)
			{131, BAND_6G, CHNL_SPAN_20, 1, 59, FALSE}
			,	/* 6G_CH_1_233 */
#endif
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup20, sizeof(g_u2CountryGroup20) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 11, FALSE}
			,	/* CH_SET_2G4_1_11 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,	/* CH_SET_UNII_WW_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 8, FALSE}
			,	/* CH_SET_UNII_UPPER_149_177 */
#if (CFG_SUPPORT_WIFI_6G == 1)
			{131, BAND_6G, CHNL_SPAN_20, 1, 59, FALSE}
			,	/* 6G_CH_1_233 */
#endif
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		(uint16_t *) g_u2CountryGroup21, sizeof(g_u2CountryGroup21) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,	/*CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,	/*CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,	/*CH_SET_UNII_MID_52_64 */
			{121, BAND_NULL, 0, 0, 0, FALSE}
			,	/*CH_SET_UNII_WW_NA */
			{125, BAND_5G, CHNL_SPAN_20, 149, 4, FALSE}
			,	/* CH_SET_UNII_UPPER_149_161 */
			{0, BAND_NULL, 0, 0, 0, FALSE}
		}
	}
	,
	{
		/* Note: Default group if no matched country code */
		NULL, 0,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
			,			/* CH_SET_2G4_1_13 */
			{115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
			,			/* CH_SET_UNII_LOW_36_48 */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,			/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,			/* CH_SET_UNII_WW_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 8, FALSE}
			,			/* CH_SET_UNII_UPPER_149_177 */
#if (CFG_SUPPORT_WIFI_6G == 1)
			{131, BAND_6G, CHNL_SPAN_20, 1, 59, FALSE}
			,	/* 6G_CH_1_233 */
#endif
		}
	}
};

static const uint16_t g_u2CountryGroup0_Passive[] = {
	COUNTRY_CODE_TW
};

struct DOMAIN_INFO_ENTRY arSupportedRegDomains_Passive[] = {
	{
		(uint16_t *) g_u2CountryGroup0_Passive,
		sizeof(g_u2CountryGroup0_Passive) / 2,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 0, FALSE}
			,			/* CH_SET_2G4_1_14_NA */
			{82, BAND_2G4, CHNL_SPAN_5, 14, 0, FALSE}
			,

			{115, BAND_5G, CHNL_SPAN_20, 36, 0, FALSE}
			,			/* CH_SET_UNII_LOW_NA */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,			/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
			,			/* CH_SET_UNII_WW_100_140 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 0, FALSE}
						/* CH_SET_UNII_UPPER_NA */
#if (CFG_SUPPORT_WIFI_6G == 1)
			,
			{131, BAND_6G, CHNL_SPAN_20, 1, 59, FALSE}
						/* 6G_CH_1_233 */
#endif
		}
	}
	,
	{
		/* Default passive scan channel table: ch52~64, ch100~144 */
		NULL,
		0,
		{
			{81, BAND_2G4, CHNL_SPAN_5, 1, 0, FALSE}
			,			/* CH_SET_2G4_1_14_NA */
			{82, BAND_2G4, CHNL_SPAN_5, 14, 0, FALSE}
			,

			{115, BAND_5G, CHNL_SPAN_20, 36, 0, FALSE}
			,			/* CH_SET_UNII_LOW_NA */
			{118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
			,			/* CH_SET_UNII_MID_52_64 */
			{121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
			,			/* CH_SET_UNII_WW_100_144 */
			{125, BAND_5G, CHNL_SPAN_20, 149, 0, FALSE}
						/* CH_SET_UNII_UPPER_NA */
#if (CFG_SUPPORT_WIFI_6G == 1)
			,
			{131, BAND_6G, CHNL_SPAN_20, 1, 59, FALSE}
						/* 6G_CH_1_233 */
#endif
		}
	}
};

/*******************************************************************************
 * P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *         P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *      M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 * F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *   F U N C T I O N S
 *******************************************************************************
 */

#endif /* _RLM_CHANNEL_H */
