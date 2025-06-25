/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _HDMI_CA_H_
#define _HDMI_CA_H_

#if IS_ENABLED(CONFIG_OPTEE)

#include <linux/types.h>

#define TZ_TA_HDMI_UUID "eaf800b0-da1b-11e2-a28f-0800200c9a66"

enum HDMI_TA_SERVICE_CMD_T {
	HDMI_TA_WRITE_REG = 0,
	HDMI_TA_DPI1_WRITE_REG,
	HDMI_TA_INSTALL_HDCP_KEY,
	HDMI_TA_LOAD_HDCP_KEY,
	HDMI_TA_GET_HDCP_AKSV,
	HDMI_TA_HDCP_ENC_EN,
	HDMI_TA_HDCP_RST,
	HDMI_TA_VID_UNMUTE,
	HDMI_TA_AUD_UNMUTE,
	HDMI_TA_PROTECT_HDMIREG,
	HDMI_TA_LOAD_ROM,
	HDMI_TA_HDCP_FAIL,
	HDMI_TA_TEST_HDCP_VERSION,
	HDMI_TA_SET_LOG_LEVEL,
	HDMI_TA_HDCP_OFF,
	HDMI_TA_READ_STATUS,
	HDMI_TA_LOAD_RAM,
	HDMI_TA_SECURE_REG_ENTRY,
	HDMI_TA_GET_EFUSE,
};

enum TA_RETURN_HDMI_HDCP_STATE {
	TA_RETURN_HDCP_STATE_ENC_EN = 0,
	TA_RETURN_HDCP_STATE_ENC_FAIL,
	TA_RETURN_HDCP_STATE_ENC_UNKNOWN,
};

bool fgCaHDMICreate(void);
bool fgCaHDMIClose(void);
void vCaHDMIWriteReg(unsigned int u4addr, unsigned int u4data);
bool fgCaHDMIInstallHdcpKey(unsigned char *pdata, unsigned int u4Len);
bool fgCaHDMIGetAKsv(unsigned char *pdata);
bool fgCaHDMILoadHDCPKey(void);
bool fgCaHDMILoadROM(void);
void vCaHDMIWriteHdcpCtrl(unsigned int u4addr, unsigned int u4data);
bool fgCaHDMITestHDCPVersion(void);
void vCaHDMIWriteHDCPRST(unsigned int u4addr, unsigned int u4data);
void fgCaHDMISetTzLogLevel(unsigned int loglevel);
bool fgCaHDMIGetTAStatus(unsigned char *pdata);
void fgCaHDMISetSecureRegEntry(unsigned int Tx_entry,
	unsigned int Rx_entry);

bool hdmi_ca_init(void);


#endif
#endif
