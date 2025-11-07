/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _GL_WEXT_PRIV_EXT_H
#define _GL_WEXT_PRIV_EXT_H

#if CFG_ENABLE_WIFI_DIRECT
#if CFG_STAINFO_FEATURE
struct PARAM_GET_STA_TRX_INFO_RESULT {
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucWlanIndex;
	uint32_t u4RxRetryPkts;
	uint32_t u4RxBMCPkts;
	uint16_t u2CapInfo;
	uint8_t aucOui[3];
	uint32_t u4Freq;
	uint8_t ucBandwidth;
	int32_t i4Rssi;
	uint32_t u4DataRate;
	uint32_t u4Mode;
	uint8_t ucAntenna;
	uint32_t u4MuMimo;
	uint16_t u2DeauthReason;
	uint8_t ucSupportedBands;
	int32_t i4AvgRssi;
	uint32_t u4TxPktsPerRate[MAX_STA_INFO_MCS_NUM];
	uint32_t u4TxPkts;
	uint32_t u4TxFails;
	uint32_t u4TxPktsRetried;
	uint32_t u4RxPktsPerRate[MAX_STA_INFO_MCS_NUM];
	uint32_t u4RxPkts;
	uint32_t u4RxFails;
	uint32_t u4RxPktsRetried;
};

struct PARAM_GET_STA_TRX_INFO {
	uint8_t ucBssidx;
	uint8_t ucBssPhyTypeSet;
	uint8_t aucTargetMac[MAC_ADDR_LEN];
	uint8_t ucQueryCount;
	struct PARAM_GET_STA_TRX_INFO_RESULT rInfo;
	uint8_t *prRespBuf;
	uint32_t u4RespBufSz;
};
#endif /* CFG_STAINFO_FEATURE */
#endif /* CFG_ENABLE_WIFI_DIRECT */

#endif /* _GL_WEXT_PRIV_EXT_H */