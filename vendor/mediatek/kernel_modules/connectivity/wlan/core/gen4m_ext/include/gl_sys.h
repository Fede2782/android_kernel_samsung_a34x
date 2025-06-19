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

void iniFileErrorCheck (struct ADAPTER *prAdapter, uint8_t **ppucIniBuf,
	uint32_t *pu4ReadSize);

/* temp solution for build pass */
int _kalSprintf(char *buf, const char *fmt, ...);
#define kalSprintf(buf, fmt, ...)          \
	_kalSprintf((char *)(buf), (const char *)(fmt), ##__VA_ARGS__)

#endif /* _GL_SYS_H */