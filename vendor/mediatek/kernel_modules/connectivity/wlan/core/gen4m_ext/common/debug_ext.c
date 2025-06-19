/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*! \file debug_ext.c
 *   \brief This file contains the WLAN OID processing routines of Windows
 *          driver for MediaTek Inc. 802.11 Wireless LAN Adapters.
 */


/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */
#include "precomp.h"

#if (CFG_SUPPORT_UV == 1)
/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */
static u_int8_t gIsUvLogTestMode = FALSE;
static u_int8_t gIsUvChecked = FALSE;

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */
u_int8_t wlanCheckIfUvTestMode(void)
{
	uint8_t *pucConfigBuf = NULL;
	uint32_t u4ConfigReadLen = 0;
	void *pvDev = NULL;

	if (!gIsUvChecked) {
		kalGetDev(&pvDev);
		if (pvDev == NULL)
			DBGLOG(INIT, DEBUG, "kalGetDev failed\n");

		/* push uv_enable_test_mode.cfg to enable uv test mode */
		if (kalRequestFirmware("uv_enable_test_mode.cfg",
			&pucConfigBuf, &u4ConfigReadLen, TRUE,
			pvDev) == 0) {
			LOG_FUNC("Enable UV Test Mode\n");
			gIsUvLogTestMode = TRUE;
		}
		gIsUvChecked = TRUE;
		if (pucConfigBuf)
			kalMemFree(pucConfigBuf, VIR_MEM_TYPE, u4ConfigReadLen);
	}

	return gIsUvLogTestMode;
}

void wlanSetDbgMaskForUvTestMode(uint32_t *pu4NewDbgMask) {
	/* if UV Test Mode is enabled, then DbgMask is
	 * set to 0x02f(UV Level)
	 */
	if (wlanCheckIfUvTestMode())
		*pu4NewDbgMask = DBG_LOG_LEVEL_UV;
	DBGLOG(INIT, DEBUG, "UvTestMode=%d, NewDbgMask=0x%x\n",
		wlanCheckIfUvTestMode(),
		*pu4NewDbgMask);
}

void wlanUpdateLogLevelByUvTestMode(struct ADAPTER *prAdapter,
				    uint32_t *pu4LogLevel, u_int8_t fgEarlySet)
{
	if (wlanCheckIfUvTestMode()) {
		DBGLOG(INIT, DEBUG,
		       "Forcibly update the log level %d->%d @UV Test Mode\n",
		       *pu4LogLevel, ENUM_WIFI_LOG_LEVEL_UV);
		*pu4LogLevel = ENUM_WIFI_LOG_LEVEL_UV;
	}

	/* When the FW log level is set to 3 (UV),
	 * the driver log level is also set to 3.
	 */
	if (*pu4LogLevel == ENUM_WIFI_LOG_LEVEL_UV) {
		wlanDbgSetLogLevel(prAdapter,
				   ENUM_WIFI_LOG_LEVEL_VERSION_V1,
				   ENUM_WIFI_LOG_MODULE_DRIVER,
				   *pu4LogLevel, fgEarlySet);
	}
}

void wlanGetDbgMaskForUvTestMode(uint32_t *pu4DbgMask) {
    if (wlanCheckIfUvTestMode())
		*pu4DbgMask = DBG_LOG_LEVEL_UV;
	else
		*pu4DbgMask = DBG_LOG_LEVEL_DEFAULT;
}

#endif
