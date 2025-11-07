/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/*! \file   "gl_sys.h"
 *    \brief  Functions for sysfs driver and others
 *
 *    Functions for sysfs driver and others
 */

#ifndef _GL_SYS_H
#define _GL_SYS_H

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define RST_REPORT_DATA_MAX_LEN 512

/* HANG INFO struct for driver info collection */
struct PARAM_RESET_RECORD_INFO {
	uint8_t aucCallTrace[RST_REPORT_DATA_MAX_LEN];
	uint32_t u4CallTraceLen;
	uint8_t aucFwPc[RST_REPORT_DATA_MAX_LEN];
	uint32_t u4FwPcLen;
	uint8_t aucCustomizeData[RST_REPORT_DATA_MAX_LEN];
	uint32_t u4CustomizeDataLen;

	uint32_t u4SubDataFlag;
};

/* HANG INFO struct for report to upper layer */
struct PARAM_RESET_REPORT_INFO {
	uint8_t id;
	uint8_t len;
	uint8_t fwVer[30];
	uint8_t driverVer[30];
	uint8_t cidInfo[30];
	uint32_t hangType;
	uint8_t rawData[RST_REPORT_DATA_MAX_LEN];
};

#define RST_REPORT_NEED_CUSTOMIZE_DATA (1 << 0)
#define RST_REPORT_NEED_CALL_TRACE (1 << 1)
#define RST_REPORT_NEED_FW_PC_LOG (1 << 2)
#define RST_REPORT_DEFAULT_FLAG (RST_REPORT_NEED_CALL_TRACE | \
				 RST_REPORT_NEED_FW_PC_LOG)

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if CFG_EXT_FEATURE
u_int8_t glIsWiFi7CfgFile(void);
#endif

#ifndef CFG_HDM_WIFI_SUPPORT
#define CFG_HDM_WIFI_SUPPORT 0
#endif

#if CFG_HDM_WIFI_SUPPORT
void HdmWifi_SysfsInit(void);
void HdmWifi_SysfsUninit(void);
#endif

#endif /* _GL_SYS_H */
