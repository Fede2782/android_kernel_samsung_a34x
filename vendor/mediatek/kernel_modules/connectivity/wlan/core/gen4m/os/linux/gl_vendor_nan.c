/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*
 * gl_vendor_nan.c
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_cfg80211.h"
#include "gl_os.h"
#include "debug.h"
#include "gl_vendor.h"
#include "gl_wext.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include <linux/can/netlink.h>
#include <net/cfg80211.h>
#include <net/netlink.h>

#if CFG_SUPPORT_NAN_R4_PAIRING
#include "nan_pairing.h"
#include <linux/delay.h>
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

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
#if CFG_SUPPORT_NAN_R4_PAIRING
/* NAN pairing uses many test commands to compose one request for sigma */
struct NanPairingPubReq g_pairingPubReq;
struct NanBootstrap g_bootstrapMethod;
struct NanPairingSetupCmd g_pairingSetupCmd;
struct NanNikExchange g_nikExchange;
struct NanBootstrapPassword g_bootstrapPassword;

bool sigma_nik_exchange_run;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
uint8_t g_enableNAN = TRUE;
uint8_t g_disableNAN = FALSE;
uint8_t g_deEvent;
uint8_t g_aucNanServiceName[NAN_MAX_SERVICE_NAME_LEN];
uint8_t g_aucNanServiceId[6];

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

void hexdump_nan(void *buf, u16 len)
{
	int i = 0;
	char *bytes = (char *) buf;
	uint64_t addr;
	int remain;
	char remain_buf[16+2+3*4+2+3*3+1];
	int result;

	if (len) {
		pr_info("HEXDUMP: buf=%p, len=%d, len=%d * 8 + %d,",
		buf, len, len / 8, len % 8);
		pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		addr = (uint64_t) buf;
		for (i = 0; ((i + 7) < len); i += 8) {
			pr_info("| %02x %02x %02x %02x %02x %02x %02x %02x\n",
			(int) bytes[i],   (int) bytes[i+1],
			(int) bytes[i+2], (int) bytes[i+3],
			(int) bytes[i+4], (int) bytes[i+5],
			(int) bytes[i+6], (int) bytes[i+7]);
		}
		remain = len - i;
		if (remain > 0) {
			memset(remain_buf, 0, sizeof(remain_buf));
			result = snprintf(remain_buf, sizeof(remain_buf),
				"| %02x %02x %02x %02x %02x %02x %02x\n",
				(int) bytes[i],
				remain > 1 ? (int) bytes[i+1] : 0,
				remain > 2 ? (int) bytes[i+2] : 0,
				remain > 3 ? (int) bytes[i+3] : 0,
				remain > 4 ? (int) bytes[i+4] : 0,
				remain > 5 ? (int) bytes[i+5] : 0,
				remain > 6 ? (int) bytes[i+6] : 0);
				remain_buf[1 + 3 * remain] = 0;
			if (result >= 0)
				pr_info("%s", remain_buf);
		}
		pr_info("--------------------------------------------------\n");
	} else {
		return;
	}
}

void
nanAbortOngoingScan(struct ADAPTER *prAdapter)
{
	struct SCAN_INFO *prScanInfo;

	if (!prAdapter)
		return;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	if (!prScanInfo || (prScanInfo->eCurrentState != SCAN_STATE_SCANNING))
		return;

	if (IS_BSS_INDEX_AIS(prAdapter,
		prScanInfo->rScanParam.ucBssIndex))
		aisFsmStateAbort_SCAN(prAdapter,
			prScanInfo->rScanParam.ucBssIndex);
	else if (prScanInfo->rScanParam.ucBssIndex ==
			prAdapter->ucP2PDevBssIdx)
		p2pDevFsmRunEventScanAbort(prAdapter,
			prAdapter->ucP2PDevBssIdx);
}

uint32_t nanOidAbortOngoingScan(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_FAILURE;
	}

	nanAbortOngoingScan(prAdapter);

	DBGLOG(NAN, TRACE, "After\n");

	return WLAN_STATUS_SUCCESS;
}

void
nanNdpAbortScan(struct ADAPTER *prAdapter)
{
	uint32_t rStatus = 0;
	uint32_t u4SetInfoLen = 0;

	if (!prAdapter->rWifiVar.fgNanOnAbortScan)
		return;

	rStatus = kalIoctl(
		prAdapter->prGlueInfo,
		nanOidAbortOngoingScan, NULL, 0,
		&u4SetInfoLen);
}

uint32_t nanOidDissolveReq(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;
	struct _NAN_NDP_INSTANCE_T *prNDP;
	uint8_t i, j;
	u_int8_t found = FALSE;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(NAN, TRACE, "Before\n");

	prDataPathInfo = &(prAdapter->rDataPathInfo);

	for (i = 0; i < NAN_MAX_SUPPORT_NDL_NUM; i++) {
		if (!prDataPathInfo->arNDL[i].fgNDLValid)
			continue;

		for (j = 0; j < NAN_MAX_SUPPORT_NDP_NUM; j++) {
			prNDP = &prDataPathInfo->arNDL[i].arNDP[j];

			if (prNDP->eCurrentNDPProtocolState !=
				NDP_NORMAL_TR)
				continue;
			prNDP->eLastNDPProtocolState =
				NDP_NORMAL_TR;
			prNDP->eCurrentNDPProtocolState =
				NDP_TX_DP_TERMINATION;
			nanNdpUpdateTypeStatus(prAdapter, prNDP);
			nanNdpSendDataPathTermination(prAdapter,
				prNDP);

			found = TRUE;
		}

		if (i >= prAdapter->rWifiVar.ucNanMaxNdpDissolve)
			break;
	}

	/* Make the frame send to FW ASAP. */
#if !CFG_SUPPORT_MULTITHREAD
	wlanAcquirePowerControl(prAdapter);
#endif
	wlanProcessCommandQueue(prAdapter,
		&prAdapter->prGlueInfo->rCmdQueue);
#if !CFG_SUPPORT_MULTITHREAD
	wlanReleasePowerControl(prAdapter);
#endif

	if (!found) {
		DBGLOG(NAN, TRACE,
			"Dissolve: Complete NAN\n");
		complete(&prAdapter->prGlueInfo->rNanDissolveComp);
	}

	DBGLOG(NAN, TRACE, "After\n");

	return WLAN_STATUS_SUCCESS;
}

void
nanNdpDissolve(struct ADAPTER *prAdapter,
	uint32_t u4Timeout)
{
	uint32_t waitRet = 0;
	uint32_t rStatus = 0;
	uint32_t u4SetInfoLen = 0;

	reinit_completion(&prAdapter->prGlueInfo->rNanDissolveComp);

	if (prAdapter->rWifiVar.fgNanDissolveAbortScan)
		nanAbortOngoingScan(prAdapter);

	rStatus = kalIoctl(
		prAdapter->prGlueInfo,
		nanOidDissolveReq, NULL, 0,
		&u4SetInfoLen);

	waitRet = wait_for_completion_timeout(
		&prAdapter->prGlueInfo->rNanDissolveComp,
		MSEC_TO_JIFFIES(
		u4Timeout));
	if (!waitRet)
		DBGLOG(NAN, WARN, "Disconnect timeout.\n");
	else
		DBGLOG(NAN, INFO, "Disconnect complete.\n");
}

/* Helper function to Write and Read TLV called in indication as well as
 * request
 */
u16
nanWriteTlv(struct _NanTlv *pInTlv, u8 *pOutTlv)
{
	u16 writeLen = 0;
	u16 i;

	if (!pInTlv) {
		DBGLOG(NAN, ERROR, "NULL pInTlv\n");
		return writeLen;
	}

	if (!pOutTlv) {
		DBGLOG(NAN, ERROR, "NULL pOutTlv\n");
		return writeLen;
	}

	*pOutTlv++ = pInTlv->type & 0xFF;
	*pOutTlv++ = (pInTlv->type & 0xFF00) >> 8;
	writeLen += 2;

	DBGLOG(NAN, LOUD, "Write TLV type %u, writeLen %u\n", pInTlv->type,
	       writeLen);

	*pOutTlv++ = pInTlv->length & 0xFF;
	*pOutTlv++ = (pInTlv->length & 0xFF00) >> 8;
	writeLen += 2;

	DBGLOG(NAN, LOUD, "Write TLV length %u, writeLen %u\n", pInTlv->length,
	       writeLen);

	for (i = 0; i < pInTlv->length; ++i)
		*pOutTlv++ = pInTlv->value[i];

	writeLen += pInTlv->length;
	DBGLOG(NAN, LOUD, "Write TLV value, writeLen %u\n", writeLen);
	return writeLen;
}

u16
nan_read_tlv(u8 *pInTlv, struct _NanTlv *pOutTlv)
{
	u16 readLen = 0;

	if (!pInTlv)
		return readLen;

	if (!pOutTlv)
		return readLen;

	pOutTlv->type = *pInTlv++;
	pOutTlv->type |= *pInTlv++ << 8;
	readLen += 2;

	pOutTlv->length = *pInTlv++;
	pOutTlv->length |= *pInTlv++ << 8;
	readLen += 2;

	if (pOutTlv->length) {
		pOutTlv->value = pInTlv;
		readLen += pOutTlv->length;
	} else {
		pOutTlv->value = NULL;
	}
	return readLen;
}

u8 *
nanAddTlv(u16 type, u16 length, u8 *value, u8 *pOutTlv)
{
	struct _NanTlv nanTlv;
	u16 len;

	nanTlv.type = type;
	nanTlv.length = length;
	nanTlv.value = (u8 *)value;

	len = nanWriteTlv(&nanTlv, pOutTlv);
	return (pOutTlv + len);
}

u16
nanMapPublishReqParams(u16 *pIndata, struct NanPublishRequest *pOutparams)
{
	u16 readLen = 0;
	u32 *pPublishParams = NULL;

	DBGLOG(NAN, INFO, "Into nanMapPublishReqParams\n");

	/* Get value of ttl(time to live) */
	pOutparams->ttl = *pIndata;

	/* Get value of ttl(time to live) */
	pIndata++;
	readLen += 2;

	/* Get value of period */
	pOutparams->period = *pIndata;

	/* Assign default value */
	if (pOutparams->period == 0)
		pOutparams->period = 1;

	pIndata++;
	readLen += 2;

	pPublishParams = (u32 *)pIndata;
	dumpMemory32(pPublishParams, 4);
	pOutparams->recv_indication_cfg =
		(u8)(GET_PUB_REPLY_IND_FLAG(*pPublishParams) |
		     GET_PUB_FOLLOWUP_RX_IND_DISABLE_FLAG(*pPublishParams) |
		     GET_PUB_MATCH_EXPIRED_IND_DISABLE_FLAG(*pPublishParams) |
		     GET_PUB_TERMINATED_IND_DISABLE_FLAG(*pPublishParams));
	pOutparams->publish_type = GET_PUB_PUBLISH_TYPE(*pPublishParams);
	pOutparams->tx_type = GET_PUB_TX_TYPE(*pPublishParams);
	pOutparams->rssi_threshold_flag =
		(u8)GET_PUB_RSSI_THRESHOLD_FLAG(*pPublishParams);
	pOutparams->publish_match_indicator =
		GET_PUB_MATCH_ALG(*pPublishParams);
	pOutparams->publish_count = (u8)GET_PUB_COUNT(*pPublishParams);
	pOutparams->connmap = (u8)GET_PUB_CONNMAP(*pPublishParams);
	readLen += 4;

	DBGLOG(NAN, VOC,
	       "[Publish Req] ttl: %u, period: %u, recv_indication_cfg: %x, publish_type: %u,tx_type: %u, rssi_threshold_flag: %u, publish_match_indicator: %u, publish_count:%u, connmap:%u, readLen:%u\n",
	       pOutparams->ttl, pOutparams->period,
	       pOutparams->recv_indication_cfg, pOutparams->publish_type,
	       pOutparams->tx_type, pOutparams->rssi_threshold_flag,
	       pOutparams->publish_match_indicator, pOutparams->publish_count,
	       pOutparams->connmap, readLen);

	return readLen;
}

u16
nanMapSubscribeReqParams(u16 *pIndata, struct NanSubscribeRequest *pOutparams)
{
	u16 readLen = 0;
	u32 *pSubscribeParams = NULL;

	DBGLOG(NAN, TRACE, "IN %s\n", __func__);

	pOutparams->ttl = *pIndata;
	pIndata++;
	readLen += 2;

	pOutparams->period = *pIndata;
	pIndata++;
	readLen += 2;

	pSubscribeParams = (u32 *)pIndata;

	pOutparams->subscribe_type = GET_SUB_SUBSCRIBE_TYPE(*pSubscribeParams);
	pOutparams->serviceResponseFilter = GET_SUB_SRF_ATTR(*pSubscribeParams);
	pOutparams->serviceResponseInclude =
		GET_SUB_SRF_INCLUDE(*pSubscribeParams);
	pOutparams->useServiceResponseFilter =
		GET_SUB_SRF_SEND(*pSubscribeParams);
	pOutparams->ssiRequiredForMatchIndication =
		GET_SUB_SSI_REQUIRED(*pSubscribeParams);
	pOutparams->subscribe_match_indicator =
		GET_SUB_MATCH_ALG(*pSubscribeParams);
	pOutparams->subscribe_count = (u8)GET_SUB_COUNT(*pSubscribeParams);
	pOutparams->rssi_threshold_flag =
		(u8)GET_SUB_RSSI_THRESHOLD_FLAG(*pSubscribeParams);
	pOutparams->recv_indication_cfg =
		(u8)GET_SUB_FOLLOWUP_RX_IND_DISABLE_FLAG(*pSubscribeParams) |
		GET_SUB_MATCH_EXPIRED_IND_DISABLE_FLAG(*pSubscribeParams) |
		GET_SUB_TERMINATED_IND_DISABLE_FLAG(*pSubscribeParams);

	DBGLOG(NAN, VOC,
	       "[Subscribe Req] ttl: %u, period: %u, subscribe_type: %u, ssiRequiredForMatchIndication: %u, subscribe_match_indicator: %x, rssi_threshold_flag: %u\n",
	       pOutparams->ttl, pOutparams->period,
	       pOutparams->subscribe_type,
	       pOutparams->ssiRequiredForMatchIndication,
	       pOutparams->subscribe_match_indicator,
	       pOutparams->rssi_threshold_flag);
	pOutparams->connmap = (u8)GET_SUB_CONNMAP(*pSubscribeParams);
	readLen += 4;
	DBGLOG(NAN, LOUD, "Subscribe readLen : %d\n", readLen);
	return readLen;
}

u16
nanMapFollowupReqParams(u32 *pIndata,
			struct NanTransmitFollowupRequest *pOutparams)
{
	u16 readLen = 0;
	u32 *pXmitFollowupParams = NULL;

	pOutparams->requestor_instance_id = *pIndata;
	pIndata++;
	readLen += 4;

	pXmitFollowupParams = pIndata;

	pOutparams->priority = GET_FLWUP_PRIORITY(*pXmitFollowupParams);
	pOutparams->dw_or_faw = GET_FLWUP_WINDOW(*pXmitFollowupParams);
	pOutparams->recv_indication_cfg =
		GET_FLWUP_TX_RSP_DISABLE_FLAG(*pXmitFollowupParams);
	readLen += 4;

	DBGLOG(NAN, INFO,
	       "[%s]priority: %u, dw_or_faw: %u, recv_indication_cfg: %u\n",
	       __func__, pOutparams->priority, pOutparams->dw_or_faw,
	       pOutparams->recv_indication_cfg);

	return readLen;
}

#if CFG_SUPPORT_NAN_R4_PAIRING
u16
nanMapPairingRequestParams(u8 *pIndata,
			struct _NanPairingRequestParams *pOutparams)
{
	u16 readLen = sizeof(struct _NanPairingRequestParams);

	memcpy(pOutparams, pIndata, sizeof(struct _NanPairingRequestParams));
	return readLen;
}

u16
nanMapPairingResponseParams(u8 *pIndata,
			struct _NanPairingResponseParams *pOutparams)
{
	u16 readLen;

	readLen = sizeof(struct _NanPairingResponseParams);
	memcpy(pOutparams, pIndata, sizeof(struct _NanPairingResponseParams));
	return readLen;
}

#if TEST_FIXED_NIK
u8 toto_dummy_nik[NAN_IDENTITY_KEY_LEN] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
#endif /* TEST_FIXED_NIK */

uint32_t
nanCommandPairingRequest_Pairing(struct ADAPTER *prAdapter,
	struct _NanPairingRequestParams *pairingRequest)
{
	u8 passphrase[NAN_SECURITY_MAX_PASSPHRASE_LEN];
	u16 password_length = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(NAN, ERROR, "[pairing-pairing] enter %s()\n", __func__);
	memset(passphrase, 0, NAN_SECURITY_MAX_PASSPHRASE_LEN);
	password_length = pairingRequest->key_len;
	if (password_length > NAN_SECURITY_MAX_PASSPHRASE_LEN)
		password_length = NAN_SECURITY_MAX_PASSPHRASE_LEN;
	memcpy(passphrase, pairingRequest->key_data, password_length);
	DBGLOG(NAN, ERROR, "pairing param requestor_instance_id:%d\n",
			pairingRequest->requestor_instance_id);
	DBGLOG(NAN, ERROR, "pairing param MAC address:" MACSTR "\n",
			MAC2STR(pairingRequest->peer_disc_mac_addr));
	DBGLOG(NAN, ERROR, "pairing param pairing_request_type:%d\n",
			pairingRequest->nan_pairing_request_type);
	DBGLOG(NAN, ERROR, "pairing param is_opportunistic:%d\n",
			pairingRequest->is_opportunistic);
	DBGLOG(NAN, ERROR, "pairing param akm:%u\n",
			pairingRequest->akm);
	DBGLOG(NAN, ERROR, "pairing param key_type=%u\n",
			pairingRequest->key_type);
	DBGLOG(NAN, ERROR, "pairing param key_len=%u\n",
			pairingRequest->key_len);
	DBGLOG(NAN, ERROR, "pairing param cipher_type:%u\n",
			pairingRequest->cipher_type);

	if (password_length > 0) {
		u8 password[NAN_SECURITY_MAX_PASSPHRASE_LEN+1] = {0};

		memcpy(password, passphrase, NAN_SECURITY_MAX_PASSPHRASE_LEN);
		DBGLOG(NAN, ERROR,
			"[pairing-intf][initiator]pairing request passwd=%s\n",
			password);
	}

	DBGLOG(NAN, INFO, "Enter Beacon SDF Request.\n");

	/* save Local Nik(and akm) to peer PairingFsm.*/
	prPairingFsm = pairingFsmSearch(prAdapter,
			pairingRequest->peer_disc_mac_addr);
	if (prPairingFsm) {
		DBGLOG(NAN, ERROR,
				"[pairing-intf][initiator] install Nik\n");
		hexdump_nan((void *)pairingRequest->nan_identity_key,
				sizeof(pairingRequest->nan_identity_key));
#if TEST_FIXED_NIK
		nanPairingInstallLocalNik(prAdapter, prPairingFsm,
				toto_dummy_nik);
#else /* TEST_FIXED_NIK */
		nanPairingInstallLocalNik(prAdapter, prPairingFsm,
				pairingRequest->nan_identity_key);
#endif /* TEST_FIXED_NIK */
		prPairingFsm->akm = pairingRequest->akm;
	} else {
		DBGLOG(NAN, ERROR,
				"[pairing-intf][initiator] install Nik Fail\n");
		rRetStatus = WLAN_STATUS_FAILURE;
		return rRetStatus;
	}

	hexdump_nan((void *)pairingRequest,
		sizeof(struct _NanPairingRequestParams));
	{
		/* 1. send pasn request to supplicant */
		struct NAN_PASN_START_PARAM PasnStartParam;

		memcpy(PasnStartParam.peer_mac_addr,
				pairingRequest->peer_disc_mac_addr,
				MAC_ADDR_LEN);
		PasnStartParam.nan_pairing_request_type =
			pairingRequest->nan_pairing_request_type;
		PasnStartParam.akm = pairingRequest->akm;
		PasnStartParam.cipher_type = pairingRequest->cipher_type;
		PasnStartParam.key_type = pairingRequest->key_type;
		PasnStartParam.key_len = pairingRequest->key_len;
		/* Copy password */
		memcpy((void *)PasnStartParam.key_data,
				pairingRequest->key_data,
				pairingRequest->key_len);

		rRetStatus =
			nanPasnRequestM1_Tx(prAdapter, &PasnStartParam);
	}
	return rRetStatus;
}

uint32_t
nanCommandPairingRequest_Verify(struct ADAPTER *prAdapter,
	struct _NanPairingRequestParams *pairingRequest,
	uint16_t publish_subscribe_id)
{
	u16 npk_length = NAN_NPK_LEN;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(NAN, ERROR, "[pairing-verification] enter %s()\n", __func__);

	/*hard code for Sigma*/
	if (nanGetFeatureIsSigma(prAdapter)) {
		pairingRequest->cipher_type =
			NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
	}

	npk_length = pairingRequest->key_len;
	if (npk_length > NAN_NPK_LEN)
		npk_length = NAN_NPK_LEN;
	DBGLOG(NAN, ERROR, "pairing param requestor_instance_id:%d\n",
			pairingRequest->requestor_instance_id);
	DBGLOG(NAN, ERROR, "pairing param MAC address:" MACSTR "\n",
			MAC2STR(pairingRequest->peer_disc_mac_addr));
	DBGLOG(NAN, ERROR, "pairing param pairing_request_type:%d\n",
			pairingRequest->nan_pairing_request_type);
	DBGLOG(NAN, ERROR, "pairing param is_opportunistic:%d\n",
			pairingRequest->is_opportunistic);
	DBGLOG(NAN, ERROR, "pairing param akm:%u\n",
			pairingRequest->akm);
	DBGLOG(NAN, ERROR, "pairing param key_type=%u\n",
			pairingRequest->key_type);
	DBGLOG(NAN, ERROR, "pairing param key_len=%u\n",
			pairingRequest->key_len);
	DBGLOG(NAN, ERROR, "pairing param cipher_type:%u\n",
			pairingRequest->cipher_type);

	if (npk_length > 0) {
		DBGLOG(NAN, ERROR,
		"[pairing-intf][initiator]verification request NPK dump\n");
		hexdump_nan((void *)pairingRequest->key_data, npk_length);
	}


	/* save Local Nik(and akm) to peer PairingFsm.*/
	/* check if there is existing prPairingFsm created by other service
	 * or create PairingFsm here.
	 */
	prPairingFsm = pairingFsmSearch(prAdapter,
			pairingRequest->peer_disc_mac_addr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO,
		"[%s] prep verification request as Subscriber\n", __func__);
		DBGLOG(NAN, ERROR,
		"[%s] Pairing FSM NULL and alloc new one\n", __func__);
		prPairingFsm = pairingFsmAlloc(prAdapter);
		if (prPairingFsm != NULL) {
			/* setup PairingFsm for verification */
			prPairingFsm->FsmMode =
				NAN_PAIRING_FSM_MODE_VERIFICATION;

			/* 2. NIK , NONCE, NIRA calculation  */
			memcpy((void *)prPairingFsm->aucNik,
				pairingRequest->nan_identity_key,
				NAN_IDENTITY_KEY_LEN);
			pairingDeriveNirNonce(prPairingFsm);
#if TEST_FIXED_NIK
			pairingDeriveNirTag(prAdapter, toto_dummy_nik,
				NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				prPairingFsm->aucNonce,
				&prPairingFsm->u8Tag);
#else /* TEST_FIXED_NIK */
			pairingDeriveNirTag(prAdapter, prPairingFsm->aucNik,
				NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				prPairingFsm->aucNonce,
				&prPairingFsm->u8Tag);
#endif /* TEST_FIXED_NIK */

				/* FSM setup at subscriber*/
			pairingFsmSetup(prAdapter,
			prPairingFsm, NAN_BOOTSTRAPPING_HANDSHAKE_SKIP,
			TRUE, pairingRequest->requestor_instance_id,
			publish_subscribe_id, FALSE, 0);

			/* create sta record for verification */
			rRetStatus =
			nanPairingVerification_FsmFF(prAdapter, prPairingFsm,
				pairingRequest->peer_disc_mac_addr);

			if (rRetStatus == WLAN_STATUS_SUCCESS && prPairingFsm) {
				DBGLOG(NAN, ERROR,
				"[pairing-intf][initiator] install Nik\n");
				hexdump_nan(
				(void *)pairingRequest->nan_identity_key,
				sizeof(pairingRequest->nan_identity_key));
#if TEST_FIXED_NIK
				nanPairingInstallLocalNik(prAdapter,
				prPairingFsm,
				toto_dummy_nik);
#else /* TEST_FIXED_NIK */
				nanPairingInstallLocalNik(prAdapter,
				prPairingFsm,
				pairingRequest->nan_identity_key);
#endif /* TEST_FIXED_NIK */
				prPairingFsm->akm = pairingRequest->akm;
			} else {
				if (rRetStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(NAN, ERROR,
				"[%s] Pairing verification FSM Setup Failed!\n",
				__func__);
				pairingFsmFree(prAdapter, prPairingFsm);
				return WLAN_STATUS_FAILURE;
				}
			}

		} else {
			DBGLOG(NAN, ERROR,
			"[%s] Pairing FSM Allocation Failed!\n",
			__func__);
			return WLAN_STATUS_FAILURE;
		}
	} else {
		DBGLOG(NAN, INFO,
		"[%s] prep verification request as Subscriber\n", __func__);
		DBGLOG(NAN, ERROR,
		"[%s] Pairing FSM %p and reuse it\n", __func__, prPairingFsm);
	}

	hexdump_nan((void *)pairingRequest,
		sizeof(struct _NanPairingRequestParams));
	{
		/* 1. send pasn request to supplicant */
		struct NAN_PASN_START_PARAM PasnStartParam;

		memcpy(PasnStartParam.peer_mac_addr,
				pairingRequest->peer_disc_mac_addr,
				MAC_ADDR_LEN);
		PasnStartParam.nan_pairing_request_type =
			pairingRequest->nan_pairing_request_type;
		PasnStartParam.akm = pairingRequest->akm;
		PasnStartParam.cipher_type = pairingRequest->cipher_type;
		PasnStartParam.key_type = pairingRequest->key_type;
		PasnStartParam.key_len = pairingRequest->key_len;

		/* Copy NPK */
		memcpy((void *)PasnStartParam.key_data,
				pairingRequest->key_data,
				pairingRequest->key_len);

		rRetStatus =
			nanPasnRequestM1_Tx(prAdapter, &PasnStartParam);
	}
	return rRetStatus;
}

uint32_t
nanCommandPairingRespond_Pairing(struct ADAPTER *prAdapter,
	struct _NanPairingResponseParams *pairingResponse,
	const void **prdata, int remainingLen)
{
	u8 passphrase[NAN_SECURITY_MAX_PASSPHRASE_LEN];
	u16 password_length = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame = NULL;
	const void *data = *prdata;
	struct NanPairingPASNMsg pasnframe;
	struct _NanTlv outputTlv;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	u16 readLen = 0;

	DBGLOG(NAN, ERROR, "[pairing-pairing] enter %s()\n", __func__);


	memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
	memset(passphrase, 0, NAN_SECURITY_MAX_PASSPHRASE_LEN);
	password_length = pairingResponse->key_len;
	if (password_length > NAN_SECURITY_MAX_PASSPHRASE_LEN)
		password_length = NAN_SECURITY_MAX_PASSPHRASE_LEN;
	memcpy(passphrase, pairingResponse->key_data, password_length);
	DBGLOG(NAN, ERROR,
	"pairing param pairing_instance_id:%d\n",
	pairingResponse->pairing_instance_id);
	DBGLOG(NAN, ERROR,
	"pairing param nan_pairing_request_type:%d\n",
	pairingResponse->nan_pairing_request_type);
	DBGLOG(NAN, ERROR,
	"pairing param rsp_code:%d\n",
	pairingResponse->rsp_code);
	DBGLOG(NAN, ERROR,
	"pairing param is_opportunistic:%d\n",
	pairingResponse->is_opportunistic);
	DBGLOG(NAN, ERROR,
	"pairing param akm:%u\n",
	pairingResponse->akm);
	DBGLOG(NAN, ERROR,
	"pairing param key_type=%u\n",
	pairingResponse->key_type);
	DBGLOG(NAN, ERROR,
	"pairing param key_len=%u\n",
	pairingResponse->key_len);
	DBGLOG(NAN, ERROR,
	"pairing param cipher_type:%d\n",
	pairingResponse->cipher_type);
	if (password_length > 0) {
		u8 password[NAN_SECURITY_MAX_PASSPHRASE_LEN+1] = {0};

		memcpy(password, passphrase, NAN_SECURITY_MAX_PASSPHRASE_LEN);
		DBGLOG(NAN, ERROR,
			"[pairing-intf][responder]pairing response passwd=%s\n",
			password);
	}
	hexdump_nan((void *)&pairingResponse,
		sizeof(struct _NanPairingResponseParams));
	while ((remainingLen >= 4) &&
	(0 != (readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
		switch (outputTlv.type) {
		case NAN_TLV_TYPE_NAN40_PAIRING_RAWFRAME:
			memcpy((void *)&pasnframe, outputTlv.value,
				sizeof(struct NanPairingPASNMsg));
			break;
		default:
			break;
		}
		remainingLen -= readLen;
		data += readLen;
		memset(&outputTlv, 0, sizeof(outputTlv));
	}

	/* save Local Nik(and akm) to peer PairingFsm */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)
		pasnframe.PASN_FRAME;
	prPairingFsm = pairingFsmSearch(prAdapter,
		prAuthFrame->aucSrcAddr);
	if (prPairingFsm) {
		DBGLOG(NAN, ERROR,
		"[pairing-intf][responder] install Nik\n");
		hexdump_nan((void *)
			pairingResponse->nan_identity_key,
			sizeof(pairingResponse->
			nan_identity_key));
		nanPairingInstallLocalNik(prAdapter,
			prPairingFsm, pairingResponse->
			nan_identity_key);
		prPairingFsm->akm = pairingResponse->akm;
	} else {
		DBGLOG(NAN, ERROR,
		"[pairing-intf][responder]install NIK Fail\n");
		rRetStatus = WLAN_STATUS_FAILURE;
		return rRetStatus;
	}

	if ((enum NanPairingResponseCode)pairingResponse->rsp_code ==
		NAN_PAIRING_REQUEST_ACCEPT && pasnframe.framesize > 0) {
		/* 2. report pasn M1 to supplicant */

		DBGLOG(NAN, ERROR,
		"[pairing-pasn] report pasn M1 to supplicant\n");
		memcpy(&pasnframe.localhostconfig.hostM1param,
			pairingResponse,
			sizeof(struct _NanPairingResponseParams));

		rRetStatus = nanPasnRequestM1_Rx(prAdapter, &pasnframe);
	} else {
		DBGLOG(NAN, ERROR,
		"[pairing-pasn] rsp_code(%u)/framesize(%u) error\n",
		pairingResponse->rsp_code,
		pasnframe.framesize);
	}
	return rRetStatus;
}
uint32_t
nanCommandPairingRespond_Verify(struct ADAPTER *prAdapter,
	struct _NanPairingResponseParams *pairingResponse,
	const void **prdata, int remainingLen,
	uint16_t publish_subscribe_id)
{
	u16 npk_length = NAN_NPK_LEN;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame = NULL;
	const void *data = *prdata;
	struct NanPairingPASNMsg pasnframe;
	struct _NanTlv outputTlv;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	u16 readLen = 0;

	DBGLOG(NAN, ERROR, "[pairing-verification] enter\n");

	/*hard code for Sigma*/
	if (nanGetFeatureIsSigma(prAdapter)) {
		pairingResponse->cipher_type =
			NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
	}

	memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
	npk_length = pairingResponse->key_len;
	if (npk_length > NAN_NPK_LEN)
		npk_length = NAN_NPK_LEN;
	DBGLOG(NAN, ERROR, "pairing param pairing_instance_id:%d\n",
		pairingResponse->pairing_instance_id);
	DBGLOG(NAN, ERROR, "pairing param nan_pairing_request_type:%d\n",
		pairingResponse->nan_pairing_request_type);
	DBGLOG(NAN, ERROR, "pairing param rsp_code:%d\n",
		pairingResponse->rsp_code);
	DBGLOG(NAN, ERROR, "pairing param is_opportunistic:%d\n",
		pairingResponse->is_opportunistic);
	DBGLOG(NAN, ERROR, "pairing param akm:%u\n",
		pairingResponse->akm);
	DBGLOG(NAN, ERROR, "pairing param key_type=%u\n",
		pairingResponse->key_type);
	DBGLOG(NAN, ERROR, "pairing param key_len=%u\n",
		pairingResponse->key_len);
	DBGLOG(NAN, ERROR, "pairing param cipher_type:%d\n",
		pairingResponse->cipher_type);
	hexdump_nan((void *)&pairingResponse,
		sizeof(struct _NanPairingResponseParams));

	while ((remainingLen >= 4) &&
	(0 != (readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
		switch (outputTlv.type) {
		case NAN_TLV_TYPE_NAN40_PAIRING_RAWFRAME:
			memcpy((void *)&pasnframe, outputTlv.value,
				sizeof(struct NanPairingPASNMsg));
			break;
		default:
			break;
		}
		remainingLen -= readLen;
		data += readLen;
		memset(&outputTlv, 0, sizeof(outputTlv));
	}

	if (npk_length > 0) {
		DBGLOG(NAN, ERROR,
		"[pairing-intf][responder]verification response NPK dump\n");
		hexdump_nan((void *)pairingResponse->key_data, npk_length);
	}

	/* save Local Nik(and akm) to peer PairingFsm */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)
		pasnframe.PASN_FRAME;
	prPairingFsm = pairingFsmSearch(prAdapter,
		prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO,
		"[%s] preparing verification response as Publisher\n",
		__func__);
		DBGLOG(NAN, ERROR,
		"[%s] Pairing FSM NULL and alloc new one\n", __func__);
		prPairingFsm = pairingFsmAlloc(prAdapter);
		if (prPairingFsm != NULL) {
			uint64_t nonce = nanPairingLoadPublisherNonce();
			/* setup PairingFsm for verification */
			prPairingFsm->FsmMode =
				NAN_PAIRING_FSM_MODE_VERIFICATION;

			/* 2. NIK , NONCE, NIRA calcuration  */
			memcpy((void *)prPairingFsm->aucNik,
				pairingResponse->nan_identity_key,
				NAN_IDENTITY_KEY_LEN);
			memcpy(prPairingFsm->aucNonce, &nonce,
				NAN_NIR_NONCE_LEN);
			pairingDeriveNirTag(prAdapter,
				prPairingFsm->aucNik, NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				prPairingFsm->aucNonce,
				&prPairingFsm->u8Tag);

			pairingFsmSetup(prAdapter,
			prPairingFsm, NAN_BOOTSTRAPPING_HANDSHAKE_SKIP,
			TRUE, publish_subscribe_id,
			0 /*requestor_instance_id*/, TRUE, 0);

			/* create sta record for verification */
			rRetStatus =
			nanPairingVerification_FsmFF(prAdapter, prPairingFsm,
				prAuthFrame->aucSrcAddr);
			if (rRetStatus == WLAN_STATUS_SUCCESS) {
				if (prPairingFsm) {
				DBGLOG(NAN, ERROR,
				"[pairing-intf][responder] install Nik\n");
				hexdump_nan(
				(void *)pairingResponse->nan_identity_key,
				sizeof(pairingResponse->nan_identity_key));
				nanPairingInstallLocalNik(prAdapter,
				prPairingFsm,
				pairingResponse->nan_identity_key);
				prPairingFsm->akm = pairingResponse->akm;
				}
			} else {
				DBGLOG(NAN, ERROR,
				"[%s] Pairing verification FSM Setup Failed!\n",
				__func__);
				pairingFsmFree(prAdapter, prPairingFsm);
				return WLAN_STATUS_FAILURE;
			}

		} else {
			DBGLOG(NAN, ERROR,
			"[%s] Pairing FSM Allocation Failed!\n",
			__func__);
			return WLAN_STATUS_FAILURE;
		}
	} else {
		DBGLOG(NAN, INFO,
		"[%s] preparing verification response as Publisher\n",
		__func__);
		DBGLOG(NAN, ERROR,
		"[%s] Pairing FSM %p and reuse it\n", __func__, prPairingFsm);
	}

	if ((enum NanPairingResponseCode)pairingResponse->rsp_code ==
		NAN_PAIRING_REQUEST_ACCEPT && pasnframe.framesize > 0) {
		/* 2. report pasn M1 to supplicant */

		DBGLOG(NAN, ERROR,
		"[pairing-verification] report pasn M1 to supplicant\n");
		memcpy(&pasnframe.localhostconfig.hostM1param,
			pairingResponse,
			sizeof(struct _NanPairingResponseParams));

		rRetStatus = nanPasnRequestM1_Rx(prAdapter, &pasnframe);
	} else {
		DBGLOG(NAN, ERROR,
		"[pairing-verification] rsp_code(%u)/framesize(%u) error\n",
		pairingResponse->rsp_code,
		pasnframe.framesize);
	}
	return rRetStatus;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
void
nanMapSdeaCtrlParams(u32 *pIndata,
		     struct NanSdeaCtrlParams *prNanSdeaCtrlParms)
{
	prNanSdeaCtrlParms->config_nan_data_path =
		GET_SDEA_DATA_PATH_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->ndp_type = GET_SDEA_DATA_PATH_TYPE(*pIndata);
	prNanSdeaCtrlParms->security_cfg = GET_SDEA_SECURITY_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->ranging_state = GET_SDEA_RANGING_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->range_report = GET_SDEA_RANGE_REPORT(*pIndata);
	prNanSdeaCtrlParms->fgFSDRequire = GET_SDEA_FSD_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->fgGAS = GET_SDEA_FSD_WITH_GAS(*pIndata);
	prNanSdeaCtrlParms->fgQoS = GET_SDEA_QOS_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->fgRangeLimit =
		GET_SDEA_RANGE_LIMIT_PRESENT(*pIndata);

	DBGLOG(NAN, INFO,
	       "config_nan_data_path: %u, ndp_type: %u, security_cfg: %u\n",
	       prNanSdeaCtrlParms->config_nan_data_path,
	       prNanSdeaCtrlParms->ndp_type, prNanSdeaCtrlParms->security_cfg);
	DBGLOG(NAN, INFO,
	       "ranging_state: %u, range_report: %u, fgFSDRequire: %u, fgGAS: %u, fgQoS: %u, fgRangeLimit: %u\n",
	       prNanSdeaCtrlParms->ranging_state,
	       prNanSdeaCtrlParms->range_report,
	       prNanSdeaCtrlParms->fgFSDRequire,
	       prNanSdeaCtrlParms->fgGAS, prNanSdeaCtrlParms->fgQoS,
	       prNanSdeaCtrlParms->fgRangeLimit);
}

void
nanMapRangingConfigParams(u32 *pIndata, struct NanRangingCfg *prNanRangingCfg)
{
	struct NanFWRangeConfigParams *prNanFWRangeCfgParams;

	prNanFWRangeCfgParams = (struct NanFWRangeConfigParams *)pIndata;

	prNanRangingCfg->ranging_resolution =
		prNanFWRangeCfgParams->range_resolution;
	prNanRangingCfg->ranging_interval_msec =
		prNanFWRangeCfgParams->range_interval;
	prNanRangingCfg->config_ranging_indications =
		prNanFWRangeCfgParams->ranging_indication_event;

	if (prNanRangingCfg->config_ranging_indications &
	    NAN_RANGING_INDICATE_INGRESS_MET_MASK)
		prNanRangingCfg->distance_ingress_cm =
			prNanFWRangeCfgParams->geo_fence_threshold
				.inner_threshold /
			10;
	if (prNanRangingCfg->config_ranging_indications &
	    NAN_RANGING_INDICATE_EGRESS_MET_MASK)
		prNanRangingCfg->distance_egress_cm =
			prNanFWRangeCfgParams->geo_fence_threshold
				.outer_threshold /
			10;

	DBGLOG(NAN, INFO,
	       "[%s]ranging_resolution: %u, ranging_interval_msec: %u, config_ranging_indications: %u\n",
	       __func__, prNanRangingCfg->ranging_resolution,
	       prNanRangingCfg->ranging_interval_msec,
	       prNanRangingCfg->config_ranging_indications);
	DBGLOG(NAN, INFO, "[%s]distance_egress_cm: %u\n", __func__,
	       prNanRangingCfg->distance_egress_cm);
}

void
nanMapNan20RangingReqParams(u32 *pIndata,
			    struct NanRangeResponseCfg *prNanRangeRspCfgParms)
{
	struct NanFWRangeReqMsg *pNanFWRangeReqMsg;

	pNanFWRangeReqMsg = (struct NanFWRangeReqMsg *)pIndata;

	prNanRangeRspCfgParms->requestor_instance_id =
		pNanFWRangeReqMsg->range_id;
	memcpy(&prNanRangeRspCfgParms->peer_addr,
	       &pNanFWRangeReqMsg->range_mac_addr, NAN_MAC_ADDR_LEN);
	if (pNanFWRangeReqMsg->ranging_accept == 1)
		prNanRangeRspCfgParms->ranging_response_code =
			NAN_RANGE_REQUEST_ACCEPT;
	else if (pNanFWRangeReqMsg->ranging_reject == 1)
		prNanRangeRspCfgParms->ranging_response_code =
			NAN_RANGE_REQUEST_REJECT;
	else
		prNanRangeRspCfgParms->ranging_response_code =
			NAN_RANGE_REQUEST_CANCEL;

	DBGLOG(NAN, INFO,
	       "[%s]requestor_instance_id: %u, ranging_response_code:%u\n",
	       __func__, prNanRangeRspCfgParms->requestor_instance_id,
	       prNanRangeRspCfgParms->ranging_response_code);
	DBGFWLOG(NAN, INFO, "[%s] addr=>%02x:%02x:%02x:%02x:%02x:%02x\n",
		 __func__, prNanRangeRspCfgParms->peer_addr[0],
		 prNanRangeRspCfgParms->peer_addr[1],
		 prNanRangeRspCfgParms->peer_addr[2],
		 prNanRangeRspCfgParms->peer_addr[3],
		 prNanRangeRspCfgParms->peer_addr[4],
		 prNanRangeRspCfgParms->peer_addr[5]);
}

#if CFG_SUPPORT_NAN_R4_PAIRING
u16
nanMapPublishPairingReqParams(u32 *pIndata,
			struct NanPublishRequest *prNanPublishReqParams)
{
	struct NanPairingCapabilityMsg *pPairingCap;

	pPairingCap = (struct NanPairingCapabilityMsg *)pIndata;
	prNanPublishReqParams->pairing_enable =
	    pPairingCap->enable_pairing_setup;
	prNanPublishReqParams->key_caching_enable =
	    pPairingCap->enable_pairing_cache;
	prNanPublishReqParams->bootstrap_type =
	    NAN_BOOTSTRAPPING_TYPE_ADVERTISE;
	prNanPublishReqParams->bootstrap_method =
	    pPairingCap->supported_bootstrapping_methods;
	prNanPublishReqParams->nira_enable =
	    pPairingCap->enable_pairing_verification;
	if (prNanPublishReqParams->nira_enable) {
		memcpy(prNanPublishReqParams->nik,
			pPairingCap->nan_identity_key,
			NAN_IDENTITY_KEY_LEN);
	}
	return 0;
}

u16
nanMapSubscribePairingReqParams(u32 *pIndata,
			struct NanSubscribeRequest *pOutparams)
{
	struct NanPairingCapabilityMsg *pPairingCap;

	pPairingCap = (struct NanPairingCapabilityMsg *)pIndata;
	pOutparams->pairing_enable =
	    pPairingCap->enable_pairing_setup;
	pOutparams->key_caching_enable =
	    pPairingCap->enable_pairing_cache;
	pOutparams->bootstrap_type =
	    NAN_BOOTSTRAPPING_TYPE_ADVERTISE;
	pOutparams->bootstrap_method =
	    pPairingCap->supported_bootstrapping_methods;
	return 0;
}
u16
nanMapBootstrapPasswordReqParams(u32 *pIndata,
			struct NanBootstrapPassword *pOutparams)
{

	u16 readLen = 0;
#if 0
	u32 *pXmitFollowupParams = NULL;

	pOutparams->requestor_instance_id = *pIndata;
	pIndata++;
	readLen += 4;

	pXmitFollowupParams = pIndata;

	pOutparams->priority = GET_FLWUP_PRIORITY(*pXmitFollowupParams);
	pOutparams->dw_or_faw = GET_FLWUP_WINDOW(*pXmitFollowupParams);
	pOutparams->recv_indication_cfg =
		GET_FLWUP_TX_RSP_DISABLE_FLAG(*pXmitFollowupParams);
	readLen += 4;

	DBGLOG(NAN, INFO,
	       "[%s]priority: %u, dw_or_faw: %u, recv_indication_cfg: %u\n",
	       __func__, pOutparams->priority, pOutparams->dw_or_faw,
	       pOutparams->recv_indication_cfg);
#endif
	return readLen;
}

void
nanClearPairingGlobalSettings(IN struct ADAPTER *prAdapter)
{
	DBGLOG(NAN, INFO, "[%s]\n]", __func__);
#if 0
	kalMemZero(&g_pairingPubReq, sizeof(g_pairingPubReq));
	kalMemZero(&g_bootstrapMethod, sizeof(g_bootstrapMethod));
	kalMemZero(&g_pairingSetupCmd, sizeof(g_pairingSetupCmd));
	kalMemZero(&g_nikExchange, sizeof(g_nikExchange));
#endif
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

u32
wlanoidGetNANCapabilitiesRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			     uint32_t u4SetBufferLen,
			     uint32_t *pu4SetInfoLen)
{
	struct NanCapabilitiesRspMsg nanCapabilitiesRsp;
	struct NanCapabilitiesRspMsg *pNanCapabilitiesRsp =
		(struct NanCapabilitiesRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	nanCapabilitiesRsp.fwHeader.msgVersion = 1;
	nanCapabilitiesRsp.fwHeader.msgId = NAN_MSG_ID_CAPABILITIES_RSP;
	nanCapabilitiesRsp.fwHeader.msgLen =
		sizeof(struct NanCapabilitiesRspMsg);
	nanCapabilitiesRsp.fwHeader.transactionId =
		pNanCapabilitiesRsp->fwHeader.transactionId;
	nanCapabilitiesRsp.status = 0;
	nanCapabilitiesRsp.max_concurrent_nan_clusters = 1;
	nanCapabilitiesRsp.max_service_name_len = NAN_MAX_SERVICE_NAME_LEN;
	nanCapabilitiesRsp.max_match_filter_len = NAN_FW_MAX_MATCH_FILTER_LEN;
	nanCapabilitiesRsp.max_service_specific_info_len =
		NAN_MAX_SERVICE_SPECIFIC_INFO_LEN;
	nanCapabilitiesRsp.max_sdea_service_specific_info_len =
		NAN_FW_MAX_TX_FOLLOW_UP_SDEA_LEN;
	nanCapabilitiesRsp.max_scid_len = NAN_MAX_SCID_BUF_LEN;
	nanCapabilitiesRsp.max_total_match_filter_len =
		(NAN_FW_MAX_MATCH_FILTER_LEN * 2);
	nanCapabilitiesRsp.cipher_suites_supported =
		NAN_CIPHER_SUITE_SHARED_KEY_128_MASK;
#if CFG_SUPPORT_NAN_R4_PAIRING
	/* for SKEA KDE encryption */
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_2WDH_128_MASK;
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_2WDH_256_MASK;
	/* for PASN PDU encryption */
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
#if 0   /* disable NCS-PK-PASN-256 */
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_256_MASK;
#endif
#endif
	nanCapabilitiesRsp.max_ndi_interfaces = 1;
	nanCapabilitiesRsp.max_publishes = NAN_MAX_PUBLISH_NUM;
	nanCapabilitiesRsp.max_subscribes = NAN_MAX_SUBSCRIBE_NUM;
	nanCapabilitiesRsp.max_ndp_sessions =
		prAdapter->rWifiVar.ucNanMaxNdpSession;
	nanCapabilitiesRsp.max_app_info_len = NAN_DP_MAX_APP_INFO_LEN;
	nanCapabilitiesRsp.max_queued_transmit_followup_msgs =
		NAN_MAX_QUEUE_FOLLOW_UP;
	nanCapabilitiesRsp.max_subscribe_address =
		NAN_MAX_SUBSCRIBE_MAX_ADDRESS;
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	nanCapabilitiesRsp.is_pairing_supported =
		prAdapter->rWifiVar.ucNanEnablePairing;
	DBGLOG(NAN, INFO,
	"[pairing-capability]nanCapabilitiesRsp.is_pairing_supported=%d\n",
	nanCapabilitiesRsp.is_pairing_supported);
#endif
	nanCapabilitiesRsp.is_instant_mode_supported = FALSE;
	nanCapabilitiesRsp.is_set_cluster_id_supported = FALSE;
	nanCapabilitiesRsp.is_suspension_supported = FALSE;
#if (CFG_SUPPORT_802_11AX == 1)
	nanCapabilitiesRsp.is_he_supported = TRUE;
#else
	nanCapabilitiesRsp.is_he_supported = FALSE;
#endif
#if (CFG_SUPPORT_WIFI_6G == 1)
	nanCapabilitiesRsp.is_6g_supported =
		!prAdapter->rWifiVar.ucDisallowBand6G;
#else
	nanCapabilitiesRsp.is_6g_supported = FALSE;
#endif

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
		sizeof(struct NanCapabilitiesRspMsg) +
		NLMSG_HDRLEN, WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanCapabilitiesRspMsg),
			     &nanCapabilitiesRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANEnableRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		    uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanEnableRspMsg nanEnableRsp;
	struct NanEnableRspMsg *pNanEnableRsp =
		(struct NanEnableRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	nanSchedUpdateP2pAisMcc(prAdapter);
	nanExtEnableReq(prAdapter);

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	nanEnableRsp.fwHeader.msgVersion = 1;
	nanEnableRsp.fwHeader.msgId = NAN_MSG_ID_ENABLE_RSP;
	nanEnableRsp.fwHeader.msgLen = sizeof(struct NanEnableRspMsg);
	nanEnableRsp.fwHeader.transactionId =
		pNanEnableRsp->fwHeader.transactionId;
	nanEnableRsp.status = 0;
	nanEnableRsp.value = 0;

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanEnableRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanEnableRspMsg),
			     &nanEnableRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	g_disableNAN = TRUE;

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANDisableRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanDisableRspMsg nanDisableRsp;
	struct NanDisableRspMsg *pNanDisableRsp =
		(struct NanDisableRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	nanExtDisableReq(prAdapter);

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	nanDisableRsp.fwHeader.msgVersion = 1;
	nanDisableRsp.fwHeader.msgId = NAN_MSG_ID_DISABLE_RSP;
	nanDisableRsp.fwHeader.msgLen = sizeof(struct NanDisableRspMsg);
	nanDisableRsp.fwHeader.transactionId =
		pNanDisableRsp->fwHeader.transactionId;
	nanDisableRsp.status = 0;

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanDisableRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanDisableRspMsg),
			     &nanDisableRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->rWifiVar.fgNanConcurrency)
		p2pFuncSwitchSapChannel(prAdapter);
#endif

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANConfigRsp(struct ADAPTER *prAdapter,
			      void *pvSetBuffer, uint32_t u4SetBufferLen,
			      uint32_t *pu4SetInfoLen)
{
	struct NanConfigRspMsg nanConfigRsp;
	struct NanConfigRspMsg *pNanConfigRsp =
		(struct NanConfigRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	nanConfigRsp.fwHeader.msgVersion = 1;
	nanConfigRsp.fwHeader.msgId = NAN_MSG_ID_CONFIGURATION_RSP;
	nanConfigRsp.fwHeader.msgLen = sizeof(struct NanConfigRspMsg);
	nanConfigRsp.fwHeader.transactionId =
		pNanConfigRsp->fwHeader.transactionId;
	nanConfigRsp.status = 0;
	nanConfigRsp.value = 0;

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanConfigRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanConfigRspMsg),
			     &nanConfigRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNanPublishRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanPublishServiceRspMsg nanPublishRsp;
	struct NanPublishServiceRspMsg *pNanPublishRsp =
		(struct NanPublishServiceRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	kalMemZero(&nanPublishRsp, sizeof(struct NanPublishServiceRspMsg));
	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	/* Prepare publish response header*/
	nanPublishRsp.fwHeader.msgVersion = 1;
	nanPublishRsp.fwHeader.msgId = NAN_MSG_ID_PUBLISH_SERVICE_RSP;
	nanPublishRsp.fwHeader.msgLen = sizeof(struct NanPublishServiceRspMsg);
	nanPublishRsp.fwHeader.handle = pNanPublishRsp->fwHeader.handle;
	nanPublishRsp.fwHeader.transactionId =
		pNanPublishRsp->fwHeader.transactionId;
	nanPublishRsp.value = 0;

	if (nanPublishRsp.fwHeader.handle != 0)
		nanPublishRsp.status = NAN_I_STATUS_SUCCESS;
	else
		nanPublishRsp.status = NAN_I_STATUS_INVALID_HANDLE;

	DBGLOG(NAN, INFO, "publish ID:%u, msgId:%u, msgLen:%u, tranID:%u\n",
	       nanPublishRsp.fwHeader.handle, nanPublishRsp.fwHeader.msgId,
	       nanPublishRsp.fwHeader.msgLen,
	       nanPublishRsp.fwHeader.transactionId);

	/*  Fill values of nanPublishRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanPublishServiceRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanPublishServiceRspMsg),
			     &nanPublishRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	/* Free the memory due to no use anymore */
	kfree(pvSetBuffer);

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANCancelPublishRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			   uint32_t u4SetBufferLen,
			   uint32_t *pu4SetInfoLen)
{
	struct NanPublishServiceCancelRspMsg nanPublishCancelRsp;
	struct NanPublishServiceCancelRspMsg *pNanPublishCancelRsp =
		(struct NanPublishServiceCancelRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	kalMemZero(&nanPublishCancelRsp,
		   sizeof(struct NanPublishServiceCancelRspMsg));
	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	DBGLOG(NAN, INFO, "%s\n", __func__);

	nanPublishCancelRsp.fwHeader.msgVersion = 1;
	nanPublishCancelRsp.fwHeader.msgId =
		NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_RSP;
	nanPublishCancelRsp.fwHeader.msgLen =
		sizeof(struct NanPublishServiceCancelRspMsg);
	nanPublishCancelRsp.fwHeader.handle =
		pNanPublishCancelRsp->fwHeader.handle;
	nanPublishCancelRsp.fwHeader.transactionId =
		pNanPublishCancelRsp->fwHeader.transactionId;
	nanPublishCancelRsp.value = 0;
	nanPublishCancelRsp.status = pNanPublishCancelRsp->status;

	DBGLOG(NAN, INFO, "[%s] nanPublishCancelRsp.fwHeader.handle = %d\n",
	       __func__, nanPublishCancelRsp.fwHeader.handle);
	DBGLOG(NAN, INFO,
	       "[%s] nanPublishCancelRsp.fwHeader.transactionId = %d\n",
	       __func__, nanPublishCancelRsp.fwHeader.transactionId);

	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanPublishServiceCancelRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanPublishServiceCancelRspMsg),
			     &nanPublishCancelRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	kfree(pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNanSubscribeRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		       uint32_t u4SetBufferLen,
		       uint32_t *pu4SetInfoLen)
{
	struct NanSubscribeServiceRspMsg nanSubscribeRsp;
	struct NanSubscribeServiceRspMsg *pNanSubscribeRsp =
		(struct NanSubscribeServiceRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	kalMemZero(&nanSubscribeRsp, sizeof(struct NanSubscribeServiceRspMsg));
	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	DBGLOG(NAN, INFO, "%s\n", __func__);

	nanSubscribeRsp.fwHeader.msgVersion = 1;
	nanSubscribeRsp.fwHeader.msgId = NAN_MSG_ID_SUBSCRIBE_SERVICE_RSP;
	nanSubscribeRsp.fwHeader.msgLen =
		sizeof(struct NanSubscribeServiceRspMsg);
	nanSubscribeRsp.fwHeader.handle = pNanSubscribeRsp->fwHeader.handle;
	nanSubscribeRsp.fwHeader.transactionId =
		pNanSubscribeRsp->fwHeader.transactionId;
	nanSubscribeRsp.value = 0;
	if (nanSubscribeRsp.fwHeader.handle != 0)
		nanSubscribeRsp.status = NAN_I_STATUS_SUCCESS;
	else
		nanSubscribeRsp.status = NAN_I_STATUS_INVALID_HANDLE;

	/*  Fill values of nanSubscribeRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanSubscribeServiceRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanSubscribeServiceRspMsg),
			     &nanSubscribeRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	DBGLOG(NAN, VOC, "handle:%u,transactionId:%u\n",
	       nanSubscribeRsp.fwHeader.handle,
	       nanSubscribeRsp.fwHeader.transactionId);

	kfree(pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANCancelSubscribeRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			     uint32_t u4SetBufferLen,
			     uint32_t *pu4SetInfoLen)
{
	struct NanSubscribeServiceCancelRspMsg nanSubscribeCancelRsp;
	struct NanSubscribeServiceCancelRspMsg *pNanSubscribeCancelRsp =
		(struct NanSubscribeServiceCancelRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	kalMemZero(&nanSubscribeCancelRsp,
		   sizeof(struct NanSubscribeServiceCancelRspMsg));
	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	DBGLOG(NAN, INFO, "%s\n", __func__);

	nanSubscribeCancelRsp.fwHeader.msgVersion = 1;
	nanSubscribeCancelRsp.fwHeader.msgId =
		NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_RSP;
	nanSubscribeCancelRsp.fwHeader.msgLen =
		sizeof(struct NanSubscribeServiceCancelRspMsg);
	nanSubscribeCancelRsp.fwHeader.handle =
		pNanSubscribeCancelRsp->fwHeader.handle;
	nanSubscribeCancelRsp.fwHeader.transactionId =
		pNanSubscribeCancelRsp->fwHeader.transactionId;
	nanSubscribeCancelRsp.value = 0;
	nanSubscribeCancelRsp.status = pNanSubscribeCancelRsp->status;

	/*  Fill values of NanSubscribeServiceCancelRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanSubscribeServiceCancelRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanSubscribeServiceCancelRspMsg),
			     &nanSubscribeCancelRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	DBGLOG(NAN, ERROR, "handle:%u, transactionId:%u\n",
	       nanSubscribeCancelRsp.fwHeader.handle,
	       nanSubscribeCancelRsp.fwHeader.transactionId);

	kfree(pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANFollowupRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanTransmitFollowupRspMsg nanXmitFollowupRsp;
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanPairingBootStrapMsg bootstrap_msg = {0};
	uint8_t *tlvs = NULL;
	struct NanTransmitFollowupRspMsg_p
	*pNanXmitFollowupRsp_p =
	(struct NanTransmitFollowupRspMsg_p *)pvSetBuffer;

	struct NanTransmitFollowupRspMsg *pNanXmitFollowupRsp =
		pNanXmitFollowupRsp_p->pfollowRsp;
#else
	struct NanTransmitFollowupRspMsg *pNanXmitFollowupRsp =
		(struct NanTransmitFollowupRspMsg *)pvSetBuffer;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	size_t message_len = 0;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;
	kalMemZero(&nanXmitFollowupRsp,
		   sizeof(struct NanTransmitFollowupRspMsg));

	DBGLOG(NAN, INFO, "%s\n", __func__);

	/* Prepare Transmit Follow up response */
	nanXmitFollowupRsp.fwHeader.msgVersion = 1;
	nanXmitFollowupRsp.fwHeader.msgId = NAN_MSG_ID_TRANSMIT_FOLLOWUP_RSP;
	nanXmitFollowupRsp.fwHeader.msgLen =
		sizeof(struct NanTransmitFollowupRspMsg);
	nanXmitFollowupRsp.fwHeader.handle =
		pNanXmitFollowupRsp->fwHeader.handle;
	nanXmitFollowupRsp.fwHeader.transactionId =
		pNanXmitFollowupRsp->fwHeader.transactionId;
	nanXmitFollowupRsp.status = pNanXmitFollowupRsp->status;
	nanXmitFollowupRsp.value = 0;

	message_len = sizeof(struct NanTransmitFollowupRspMsg) + NLMSG_HDRLEN;
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingBootStrapMsg));
		bootstrap_msg.bootstrap_type =
		pNanXmitFollowupRsp_p->bootstrap_type;
		bootstrap_msg.bootstrap_status =
		pNanXmitFollowupRsp_p->bootstrap_status;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	/*  Fill values of NanSubscribeServiceCancelRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, message_len,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanTransmitFollowupRspMsg *prNanTransmitFollowupRsp;

	prNanTransmitFollowupRsp = kmalloc(message_len, GFP_KERNEL);

	if (!prNanTransmitFollowupRsp) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_RESOURCES;
	}

	kalMemZero(prNanTransmitFollowupRsp, message_len);
	memcpy(prNanTransmitFollowupRsp, &nanXmitFollowupRsp,
		sizeof(struct NanTransmitFollowupRspMsg));

	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		tlvs = prNanTransmitFollowupRsp->ptlv;
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING,
			sizeof(struct NanPairingBootStrapMsg),
			(u8 *)&bootstrap_msg, tlvs);
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     message_len, prNanTransmitFollowupRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanTransmitFollowupRsp);
		return WLAN_STATUS_INVALID_DATA;
	}
#else
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanTransmitFollowupRspMsg),
			     &nanXmitFollowupRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	cfg80211_vendor_event(skb, GFP_KERNEL);

#if CFG_SUPPORT_NAN_R4_PAIRING
	kfree(pNanXmitFollowupRsp_p->pfollowRsp);
	kfree(prNanTransmitFollowupRsp);
#else
	kfree(pvSetBuffer);
#endif
	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_NAN_R4_PAIRING
u32
wlanoidNANPairingRequestRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct _NanPairingRequestRspMsg nanPairingRequestRsp;
	struct _NanPairingRequestRspMsg *pNanPairingRequestRsp =
		(struct _NanPairingRequestRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;
	kalMemZero(&nanPairingRequestRsp,
		   sizeof(struct _NanPairingRequestRspMsg));

	DBGLOG(NAN, ERROR, "%s\n", __func__);

	/* Prepare Pairing request response */
	nanPairingRequestRsp.fwHeader.msgVersion = 1;
	nanPairingRequestRsp.fwHeader.msgId = NAN_MSG_ID_PAIRING_REQUEST_RSP;
	nanPairingRequestRsp.fwHeader.msgLen =
		sizeof(struct _NanPairingRequestRspMsg);
	nanPairingRequestRsp.fwHeader.handle =
		pNanPairingRequestRsp->fwHeader.handle;
	nanPairingRequestRsp.fwHeader.transactionId =
		pNanPairingRequestRsp->fwHeader.transactionId;
	nanPairingRequestRsp.status = pNanPairingRequestRsp->status;
	nanPairingRequestRsp.value = 0;

	/*  Fill values of _NanPairingRequestRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct _NanPairingRequestRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct _NanPairingRequestRspMsg),
			     &nanPairingRequestRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	hexdump_nan(&nanPairingRequestRsp,
		sizeof(struct _NanPairingRequestRspMsg));
	cfg80211_vendor_event(skb, GFP_KERNEL);

	/* kfree(pvSetBuffer); */ /* This makes exception */
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANPairingResponseRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct _NanPairingResponseRspMsg nanPairingResponseRsp;
	struct _NanPairingResponseRspMsg *pNanPairingResponseRsp =
		(struct _NanPairingResponseRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;
	kalMemZero(&nanPairingResponseRsp,
		   sizeof(struct _NanPairingResponseRspMsg));

	DBGLOG(NAN, INFO, "%s\n", __func__);

	/* Prepare Transmit Follow up response */
	nanPairingResponseRsp.fwHeader.msgVersion = 1;
	nanPairingResponseRsp.fwHeader.msgId = NAN_MSG_ID_PAIRING_RESPONSE_RSP;
	nanPairingResponseRsp.fwHeader.msgLen =
		sizeof(struct _NanPairingResponseRspMsg);
	nanPairingResponseRsp.fwHeader.handle =
		pNanPairingResponseRsp->fwHeader.handle;
	nanPairingResponseRsp.fwHeader.transactionId =
		pNanPairingResponseRsp->fwHeader.transactionId;
	nanPairingResponseRsp.status = pNanPairingResponseRsp->status;
	nanPairingResponseRsp.value = 0;

	/*  Fill values of NanSubscribeServiceCancelRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct _NanPairingResponseRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct _NanPairingResponseRspMsg),
			     &nanPairingResponseRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	hexdump_nan(&nanPairingResponseRsp,
		sizeof(struct _NanPairingResponseRspMsg));
	cfg80211_vendor_event(skb, GFP_KERNEL);

	/* kfree(pvSetBuffer); */ /* This makes exception */
	return WLAN_STATUS_SUCCESS;
}
#endif

#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
static struct genl_info *info;

void nan_wiphy_unlock(struct wiphy *wiphy)
{
	struct cfg80211_registered_device *rdev = NULL;

	if (!wiphy) {
		log_dbg(NAN, ERROR, "wiphy is null\n");
		return;
	}
	rdev = container_of(wiphy,
		struct cfg80211_registered_device, wiphy);

	info = rdev->cur_cmd_info;

	wiphy_unlock(wiphy);
}

void nan_wiphy_lock(struct wiphy *wiphy)
{
	struct cfg80211_registered_device *rdev = NULL;

	if (!wiphy) {
		log_dbg(NAN, ERROR, "wiphy is null\n");
		return;
	}
	rdev = container_of(wiphy,
		struct cfg80211_registered_device, wiphy);

	wiphy_lock(wiphy);

	if (rdev->cur_cmd_info != info) {
		u32 seq = 0;

		log_dbg(NAN, ERROR,
			"own_p:%p, curr_p:%p\n",
			info,
			rdev->cur_cmd_info);

		if (rdev->cur_cmd_info)
			seq = rdev->cur_cmd_info->snd_seq;

		log_dbg(NAN, ERROR,
			"own:%u, curr:%u\n",
			info->snd_seq,
			seq);

		rdev->cur_cmd_info = info;
	}
}
#endif

struct NanDataPathInitiatorNDPE g_ndpReqNDPE;
int mtk_cfg80211_vendor_nan(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct sk_buff *skb = NULL;
	struct ADAPTER *prAdapter;

	struct _NanMsgHeader nanMsgHdr;
	struct _NanTlv outputTlv;
	u16 readLen = 0;
	u32 u4BufLen;
	u32 i4Status = -EINVAL;
	u32 u4DelayIdx;
	int ret = 0;
	int remainingLen;
	u32 waitRet = 0;

	if (data_len < sizeof(struct _NanMsgHeader)) {
		DBGLOG(NAN, ERROR, "data_len error!\n");
		return -EINVAL;
	}
	remainingLen = (data_len - (sizeof(struct _NanMsgHeader)));

	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EINVAL;
	}
	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev error!\n");
		return -EINVAL;
	}

	if (data == NULL || data_len <= 0) {
		DBGLOG(NAN, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}
	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "prGlueInfo error!\n");
		return -EINVAL;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return -EINVAL;
	}

	prAdapter->fgIsNANfromHAL = TRUE;
	DBGLOG(NAN, LOUD, "NAN fgIsNANfromHAL set %u\n",
		prAdapter->fgIsNANfromHAL);

	if (au2DebugModule[DBG_NAN_IDX] & DBG_CLASS_INFO)
		dumpMemory8((uint8_t *)data, data_len);
	DBGLOG(NAN, TRACE, "DATA len from user %d, lock(%d)\n",
		data_len,
		rtnl_is_locked());

	memcpy(&nanMsgHdr, (struct _NanMsgHeader *)data,
		sizeof(struct _NanMsgHeader));
	data += sizeof(struct _NanMsgHeader);

	if (au2DebugModule[DBG_NAN_IDX] & DBG_CLASS_INFO)
		dumpMemory8((uint8_t *)data, remainingLen);
	DBGLOG(NAN, VOC, "nanMsgHdr.length %u, nanMsgHdr.msgId %d\n",
		nanMsgHdr.msgLen, nanMsgHdr.msgId);

	switch (nanMsgHdr.msgId) {
	case NAN_MSG_ID_ENABLE_REQ: {
		struct NanEnableRequest nanEnableReq;
		struct NanEnableRspMsg nanEnableRsp;
#if KERNEL_VERSION(5, 12, 0) > CFG80211_VERSION_CODE
		uint8_t fgRollbackRtnlLock = FALSE;
#endif

		nanNdpAbortScan(prAdapter);

		kalMemZero(&nanEnableReq, sizeof(struct NanEnableRequest));
		kalMemZero(&nanEnableRsp, sizeof(struct NanEnableRspMsg));

		memcpy(&nanEnableRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanEnableRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb, sizeof(struct NanEnableRspMsg),
					   &nanEnableRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		ret = cfg80211_vendor_cmd_reply(skb);

		if (prAdapter->fgIsNANRegistered) {
			DBGLOG(NAN, WARN, "NAN is already enabled\n");
			goto skip_enable;
		}

#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
		kal_reinit_completion(
			&prAdapter->prGlueInfo->rNanHaltComp);
#if (CFG_SUPPORT_MLO_STA_NAN_FALLBACK == 1)
		kal_reinit_completion(
			&prAdapter->prGlueInfo->rNanAisComp);
#endif
#else
		prAdapter->prGlueInfo->rNanHaltComp.done = 0;
#if (CFG_SUPPORT_MLO_STA_NAN_FALLBACK == 1)
		prAdapter->prGlueInfo->rNanAisComp.done = 0;
#endif
#endif

		for (u4DelayIdx = 0; u4DelayIdx < 5; u4DelayIdx++) {
			if (g_enableNAN == TRUE) {
				g_enableNAN = FALSE;
				break;
			}
			msleep(1000);
		}
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_unlock(wiphy);
#else
		/* to avoid re-enter rtnl lock during
		 * register_netdev/unregister_netdev NAN/P2P
		 * we take away lock first and return later
		 */
		if (rtnl_is_locked()) {
			fgRollbackRtnlLock = TRUE;
			rtnl_unlock();
		}
#endif

#if (CFG_SUPPORT_MLO_STA_NAN_FALLBACK == 1)
		if (aisGetLinkNum(
			aisGetDefaultAisInfo(prAdapter)) > 1 &&
			nanIsSapOrP2pActive(prAdapter)) {
			prAdapter->fgIsNANStartWaiting = TRUE;
			aisBssBeaconTimeout_impl(prAdapter,
			BEACON_TIMEOUT_REASON_NUM,
			FALSE,
			aisGetDefaultLinkBssIndex(prAdapter));
			waitRet = wait_for_completion_timeout(
				&prAdapter->prGlueInfo->rNanAisComp,
				MSEC_TO_JIFFIES(2*1000));
			prAdapter->fgIsNANStartWaiting = FALSE;
		}
#endif

		DBGLOG(NAN, TRACE,
			"[DBG] NAN enable enter set_nan_handler, lock(%d)\n",
			rtnl_is_locked());
		set_nan_handler(wdev->netdev, 1, FALSE);
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_lock(wiphy);
#else
		if (fgRollbackRtnlLock)
			rtnl_lock();
#endif

		g_deEvent = 0;

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_CONFIG_DISCOVERY_INDICATIONS:
					nanEnableReq.discovery_indication_cfg =
						*outputTlv.value;
				break;
			case NAN_TLV_TYPE_CLUSTER_ID_LOW:
				if (outputTlv.length >
					sizeof(nanEnableReq.cluster_low)) {
					DBGLOG(NAN, ERROR,
					"type%d outputTlv.length is invalid!\n",
					outputTlv.type);
					return -EFAULT;
				}
				memcpy(&nanEnableReq.cluster_low,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_CLUSTER_ID_HIGH:
				if (outputTlv.length >
					sizeof(nanEnableReq.cluster_high)) {
					DBGLOG(NAN, ERROR,
					"type%d outputTlv.length is invalid!\n",
					outputTlv.type);
					return -EFAULT;
				}
				memcpy(&nanEnableReq.cluster_high,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_MASTER_PREFERENCE:
				if (outputTlv.length >
					sizeof(nanEnableReq.master_pref)) {
					DBGLOG(NAN, ERROR,
					"type%d outputTlv.length is invalid!\n",
					outputTlv.type);
					return -EFAULT;
				}
				memcpy(&nanEnableReq.master_pref,
				       outputTlv.value, outputTlv.length);
				break;
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		nanEnableReq.master_pref = prAdapter->rWifiVar.ucMasterPref;
		nanEnableReq.config_random_factor_force = 0;
		nanEnableReq.random_factor_force_val = 0;
		nanEnableReq.config_hop_count_force = 0;
		nanEnableReq.hop_count_force_val = 0;
		nanEnableReq.config_5g_channel =
			prAdapter->rWifiVar.ucConfig5gChannel;
		if (rlmDomainIsLegalChannel(prAdapter,
					    BAND_5G,
					    NAN_5G_LOW_DISC_CHANNEL))
			nanEnableReq.channel_5g_val |= BIT(0);
		if (rlmDomainIsLegalChannel(prAdapter,
					    BAND_5G,
					    NAN_5G_HIGH_DISC_CHANNEL))
			nanEnableReq.channel_5g_val |= BIT(1);

		/* Wait DBDC enable here, then send Nan neable request */
		waitRet = wait_for_completion_timeout(
			&prAdapter->prGlueInfo->rNanHaltComp,
			MSEC_TO_JIFFIES(4*1000));

		if (waitRet == 0) {
			DBGLOG(NAN, ERROR,
				"wait event timeout!\n");
			return FALSE;
		}

		nanEnableRsp.status = nanDevEnableRequest(prAdapter,
							  &nanEnableReq);

		for (u4DelayIdx = 0; u4DelayIdx < 50; u4DelayIdx++) {
			if (g_deEvent == NAN_BSS_INDEX_NUM) {
				g_deEvent = 0;
				break;
			}
			msleep(100);
		}

skip_enable:
		i4Status = kalIoctl(prGlueInfo, wlanoidNANEnableRsp,
				    (void *)&nanEnableRsp,
				    sizeof(struct NanEnableRequest), &u4BufLen);

		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			return -EFAULT;
		}
		break;
	}
	case NAN_MSG_ID_DISABLE_REQ: {
		struct NanDisableRspMsg nanDisableRsp;
#if KERNEL_VERSION(5, 12, 0) > CFG80211_VERSION_CODE
		uint8_t fgRollbackRtnlLock = FALSE;
#endif

		kalMemZero(&nanDisableRsp, sizeof(struct NanDisableRspMsg));

		memcpy(&nanDisableRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanDisableRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb, sizeof(struct NanDisableRspMsg),
					   &nanDisableRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		ret = cfg80211_vendor_cmd_reply(skb);

		if (!prAdapter->fgIsNANRegistered) {
			DBGLOG(NAN, WARN, "NAN is already disabled\n");
			goto skip;
		}

		for (u4DelayIdx = 0; u4DelayIdx < 5; u4DelayIdx++) {
			/* Do not block to disable if not enable */
			if (g_disableNAN == TRUE || g_enableNAN == TRUE) {
				g_disableNAN = FALSE;
				break;
			}
			msleep(1000);
		}

		if (!wlanIsDriverReady(prGlueInfo,
			WLAN_DRV_READY_CHECK_WLAN_ON |
			WLAN_DRV_READY_CHECK_HIF_SUSPEND)) {
			DBGLOG(NAN, WARN, "driver is not ready\n");
			return -EFAULT;
		}

		if (prAdapter->rWifiVar.ucNanMaxNdpDissolve)
			nanNdpDissolve(prAdapter,
				prAdapter->rWifiVar.u4NanDissolveTimeout);

		nanDisableRsp.status =
			nanDevDisableRequest(prGlueInfo->prAdapter);

#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_unlock(wiphy);
#else
		/* to avoid re-enter rtnl lock during
		 * register_netdev/unregister_netdev NAN/P2P
		 * we take away lock first and return later
		 */
		if (rtnl_is_locked()) {
			fgRollbackRtnlLock = TRUE;
			rtnl_unlock();
		}
#endif
		DBGLOG(NAN, TRACE,
			"[DBG] NAN disable, enter set_nan_handler, lock(%d)\n",
			rtnl_is_locked());
		set_nan_handler(wdev->netdev, 0, FALSE);
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_lock(wiphy);
#else
		if (fgRollbackRtnlLock)
			rtnl_lock();
#endif

skip:

		i4Status = kalIoctl(prGlueInfo, wlanoidNANDisableRsp,
				    (void *)&nanDisableRsp,
				    sizeof(struct NanDisableRspMsg), &u4BufLen);

		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			return -EFAULT;
		}

		break;
	}
	case NAN_MSG_ID_CONFIGURATION_REQ: {
		struct NanConfigRequest nanConfigReq;
		struct NanConfigRspMsg nanConfigRsp;

		kalMemZero(&nanConfigReq, sizeof(struct NanConfigRequest));
		kalMemZero(&nanConfigRsp, sizeof(struct NanConfigRspMsg));

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_MASTER_PREFERENCE:
				if (outputTlv.length >
					sizeof(nanConfigReq.master_pref)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					return -EFAULT;
				}
				memcpy(&nanConfigReq.master_pref,
				       outputTlv.value, outputTlv.length);
				nanDevSetMasterPreference(
					prGlueInfo->prAdapter,
					nanConfigReq.master_pref);
				break;
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		nanConfigRsp.status = 0;

		memcpy(&nanConfigRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanConfigRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb, sizeof(struct NanConfigRspMsg),
					   &nanConfigRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidNANConfigRsp,
			(void *)&nanConfigRsp, sizeof(struct NanConfigRspMsg),
			&u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);
		break;
	}
	case NAN_MSG_ID_CAPABILITIES_REQ: {
		struct NanCapabilitiesRspMsg nanCapabilitiesRsp;

		memcpy(&nanCapabilitiesRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanCapabilitiesRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
					   sizeof(struct NanCapabilitiesRspMsg),
					   &nanCapabilitiesRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidGetNANCapabilitiesRsp,
				    (void *)&nanCapabilitiesRsp,
				    sizeof(struct NanCapabilitiesRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
		DBGLOG(NAN, INFO, "i4Status = %u\n", i4Status);
		ret = cfg80211_vendor_cmd_reply(skb);

		break;
	}
	case NAN_MSG_ID_PUBLISH_SERVICE_REQ: {
		struct NanPublishRequest *pNanPublishReq = NULL;
		struct NanPublishServiceRspMsg *pNanPublishRsp = NULL;
		uint16_t publish_id = 0;
		uint8_t ucCipherType = 0;

		DBGLOG(NAN, VOC, "IN case NAN_MSG_ID_PUBLISH_SERVICE_REQ\n");

		pNanPublishReq =
			kmalloc(sizeof(struct NanPublishRequest), GFP_ATOMIC);

		if (!pNanPublishReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanPublishRsp = kmalloc(sizeof(struct NanPublishServiceRspMsg),
					 GFP_ATOMIC);

		if (!pNanPublishRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanPublishReq);
			return -ENOMEM;
		}

		kalMemZero(pNanPublishReq, sizeof(struct NanPublishRequest));
		kalMemZero(pNanPublishRsp,
			   sizeof(struct NanPublishServiceRspMsg));

		/* Mapping publish req related parameters */
		readLen = nanMapPublishReqParams((u16 *)data, pNanPublishReq);
		remainingLen -= readLen;
		data += readLen;

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_SERVICE_NAME:
				if (outputTlv.length >
					NAN_MAX_SERVICE_NAME_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memset(g_aucNanServiceName, 0,
					NAN_MAX_SERVICE_NAME_LEN);
				memcpy(pNanPublishReq->service_name,
				       outputTlv.value, outputTlv.length);
				memcpy(g_aucNanServiceName,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->service_name_len =
					outputTlv.length;
				DBGLOG(NAN, INFO,
					"type:SERVICE_NAME:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, INFO,
				"type:SERVICE_SPECIFIC_INFO:%u Len:%u\n",
				outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_RX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->rx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->rx_match_filter_len =
					outputTlv.length;
				if (au2DebugModule[DBG_NAN_IDX]
					& DBG_CLASS_INFO) {
					DBGLOG(NAN, INFO,
					"type:RX_MATCH_FILTER:%u Len:%u\n",
					outputTlv.type,
					outputTlv.length);

					dumpMemory8(
						(uint8_t *)
						pNanPublishReq
						->rx_match_filter,
						pNanPublishReq
						->rx_match_filter_len);
				}
				break;
			case NAN_TLV_TYPE_TX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->tx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->tx_match_filter_len =
					outputTlv.length;
				if (au2DebugModule[DBG_NAN_IDX]
					& DBG_CLASS_INFO) {
					DBGLOG(NAN, INFO,
					"type:TX_MATCH_FILTER:%u Len:%u\n",
					outputTlv.type,
					outputTlv.length);

					dumpMemory8(
						(uint8_t *)
						pNanPublishReq->tx_match_filter,
						pNanPublishReq
						->tx_match_filter_len);
				}
				break;
			case NAN_TLV_TYPE_NAN_SERVICE_ACCEPT_POLICY:
				pNanPublishReq->service_responder_policy =
					*(outputTlv.value);
				DBGLOG(NAN, INFO,
				"type:SERVICE_ACCEPT_POLICY:%u Len:%u\n",
				outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN_CSID:
				pNanPublishReq->cipher_type =
					*(outputTlv.value);
				break;
			case NAN_TLV_TYPE_NAN_PMK:
				if (outputTlv.length >
					NAN_PMK_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->key_info.body.pmk_info
					       .pmk,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->key_info.body.pmk_info.pmk_len =
					outputTlv.length;
				break;
			case NAN_TLV_TYPE_NAN_PASSPHRASE:
				if (outputTlv.length >
					NAN_SECURITY_MAX_PASSPHRASE_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->key_info.body
					       .passphrase_info.passphrase,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->key_info.body.passphrase_info
					.passphrase_len = outputTlv.length;
				break;
			case NAN_TLV_TYPE_SDEA_CTRL_PARAMS:
				nanMapSdeaCtrlParams(
					(u32 *)outputTlv.value,
					&pNanPublishReq->sdea_params);
				DBGLOG(NAN, INFO,
					"type:_SDEA_CTRL_PARAMS:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN_RANGING_CFG:
				nanMapRangingConfigParams(
					(u32 *)outputTlv.value,
					&pNanPublishReq->ranging_cfg);
				break;
			case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
				pNanPublishReq->sdea_service_specific_info[0] =
					*(outputTlv.value);
				pNanPublishReq->sdea_service_specific_info_len =
					outputTlv.length;
				break;
			case NAN_TLV_TYPE_NAN20_RANGING_REQUEST:
				nanMapNan20RangingReqParams(
					(u32 *)outputTlv.value,
					&pNanPublishReq->range_response_cfg);
				break;
#if CFG_SUPPORT_NAN_R4_PAIRING /*NAN_PAIRING*/
			case NAN_TLV_TYPE_NAN40_PAIRING_CAPABILITY:
				nanMapPublishPairingReqParams(
					(u32 *)outputTlv.value,
					pNanPublishReq);
				break;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

#if CFG_SUPPORT_NAN_R4_PAIRING /*NAN_PAIRING*/
		if (prAdapter->fgIsNANfromHAL == TRUE) {
			DBGLOG(NAN, INFO,
			"[pairing-disc][%s][pairing_enable=%u]\n", __func__,
			pNanPublishReq->pairing_enable);
		} else {
			pNanPublishReq->pairing_enable =
				g_pairingPubReq.ucPairingSetupEnabled;
			if (prAdapter->rWifiVar.ucNanEnablePairing == 0)
				pNanPublishReq->pairing_enable = 0;

			if (pNanPublishReq->pairing_enable) {
				pNanPublishReq->sdea_params.security_cfg =
					NAN_DP_CONFIG_SECURITY;
				pNanPublishReq->key_caching_enable =
					g_pairingPubReq.ucNPKNIKCache;
				pNanPublishReq->bootstrap_method =
					g_pairingPubReq.ucBootstrapMethod;
				pNanPublishReq->nira_enable =
					g_pairingPubReq.ucNiraEnabled;
				kalMemCpyS(pNanPublishReq->cipher_suite_list,
					sizeof(g_pairingPubReq.
					aucCipherSuiteList),
					g_pairingPubReq.aucCipherSuiteList, 2);
			}
			nanClearPairingGlobalSettings(prAdapter);
		}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

		/* Publish response message */
		memcpy(&pNanPublishRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanPublishServiceRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR,
				"Allocate skb failed\n");
			kfree(pNanPublishRsp);
			kfree(pNanPublishReq);
			return -ENOMEM;
		}

		if (unlikely(nla_put_nohdr(
			skb,
			sizeof(struct NanPublishServiceRspMsg),
			pNanPublishRsp) < 0)) {
			kfree_skb(skb);
			kfree(pNanPublishRsp);
			kfree(pNanPublishReq);
			DBGLOG(NAN, ERROR, "Fail send reply\n");
			return -EFAULT;
		}
		/* WIFI HAL will set nanMsgHdr.handle to 0xFFFF
		 * if publish id is 0. (means new publish) Otherwise set
		 * to previous publish id.
		 */
		if (nanMsgHdr.handle != 0xFFFF)
			pNanPublishReq->publish_id = nanMsgHdr.handle;

		/* return publish ID */
		publish_id = (uint16_t)nanPublishRequest(prGlueInfo->prAdapter,
							pNanPublishReq);
		/* NAN_CHK_PNT log message */
		if (nanMsgHdr.handle == 0xFFFF) {
			DBGLOG(NAN, VOC,
		       "[NAN_CHK_PNT] NAN_NEW_PUBLISH publish_id/handle=%u\n",
		       publish_id);
		}

		pNanPublishRsp->fwHeader.handle = publish_id;
		DBGLOG(NAN, VOC,
			"pNanPublishRsp->fwHeader.handle %u, publish_id : %u\n",
			pNanPublishRsp->fwHeader.handle, publish_id);

		if (pNanPublishReq->sdea_params.security_cfg &&
			publish_id != 0) {
#if CFG_SUPPORT_NAN_R4_PAIRING
			if (pNanPublishReq->pairing_enable) {

				DBGLOG(NAN, INFO,
				"nanCmdAddCsid for cipher_suite_list\n");
				nanCmdAddCsid(
					prGlueInfo->prAdapter,
					publish_id,
					2,
					pNanPublishReq->cipher_suite_list);
			} else
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
			{
				/* Fixme: supply a cipher suite list */
				ucCipherType = pNanPublishReq->cipher_type;
				nanCmdAddCsid(
						prGlueInfo->prAdapter,
						publish_id,
						1,
						&ucCipherType);
				nanSetPublishPmkid(
						prGlueInfo->prAdapter,
						pNanPublishReq);
				if (pNanPublishReq->scid_len) {
					if (pNanPublishReq->scid_len
							> NAN_SCID_DEFAULT_LEN)
						pNanPublishReq->scid_len
							= NAN_SCID_DEFAULT_LEN;
					nanCmdManageScid(
							prGlueInfo->prAdapter,
							TRUE,
							publish_id,
							pNanPublishReq->scid);
				}
			}
		}

#if CFG_SUPPORT_NAN_EXT
		nanExtTerminateApNan(prAdapter, NAN_ASC_EVENT_ASCC_END_LEGACY);
#endif

		i4Status = kalIoctl(prGlueInfo, wlanoidNanPublishRsp,
				    (void *)pNanPublishRsp,
				    sizeof(struct NanPublishServiceRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			kfree(pNanPublishReq);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanPublishReq);
		break;
	}
	case NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_REQ: {
		uint32_t rStatus;
		struct NanPublishCancelRequest *pNanPublishCancelReq = NULL;
		struct NanPublishServiceCancelRspMsg *pNanPublishCancelRsp =
			NULL;

		pNanPublishCancelReq = kmalloc(
			sizeof(struct NanPublishCancelRequest), GFP_ATOMIC);

		if (!pNanPublishCancelReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanPublishCancelRsp =
			kmalloc(sizeof(struct NanPublishServiceCancelRspMsg),
				GFP_ATOMIC);

		if (!pNanPublishCancelRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanPublishCancelReq);
			return -ENOMEM;
		}

		DBGLOG(NAN, INFO, "Enter CANCEL Publish Request\n");
		pNanPublishCancelReq->publish_id = nanMsgHdr.handle;

		DBGLOG(NAN, INFO, "PID %d\n", pNanPublishCancelReq->publish_id);
		rStatus = nanCancelPublishRequest(prGlueInfo->prAdapter,
						  pNanPublishCancelReq);

		/* Prepare for command reply */
		memcpy(&pNanPublishCancelRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanPublishServiceCancelRspMsg));
		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanPublishCancelReq);
			kfree(pNanPublishCancelRsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb, sizeof(struct
						 NanPublishServiceCancelRspMsg),
				     pNanPublishCancelRsp) < 0)) {
			kfree_skb(skb);
			kfree(pNanPublishCancelReq);
			kfree(pNanPublishCancelRsp);
			return -EFAULT;
		}

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, INFO, "CANCEL Publish Error %x\n", rStatus);
			pNanPublishCancelRsp->status = NAN_I_STATUS_DE_FAILURE;
		} else {
			DBGLOG(NAN, INFO, "CANCEL Publish Success %x\n",
			       rStatus);
			pNanPublishCancelRsp->status = NAN_I_STATUS_SUCCESS;
		}

		i4Status =
			kalIoctl(prGlueInfo, wlanoidNANCancelPublishRsp,
				 (void *)pNanPublishCancelRsp,
				 sizeof(struct NanPublishServiceCancelRspMsg),
				 &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			kfree(pNanPublishCancelReq);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanPublishCancelReq);
		break;
	}
	case NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ: {
		struct NanSubscribeRequest *pNanSubscribeReq = NULL;
		struct NanSubscribeServiceRspMsg *pNanSubscribeRsp = NULL;
		bool fgRangingCFG = FALSE;
		bool fgRangingREQ = FALSE;
		uint16_t Subscribe_id = 0;
		int i = 0;

		DBGLOG(NAN, INFO, "In NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ\n");

		pNanSubscribeReq =
			kmalloc(sizeof(struct NanSubscribeRequest), GFP_ATOMIC);

		if (!pNanSubscribeReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		pNanSubscribeRsp = kmalloc(
			sizeof(struct NanSubscribeServiceRspMsg), GFP_ATOMIC);

		if (!pNanSubscribeRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanSubscribeReq);
			return -ENOMEM;
		}
		kalMemZero(pNanSubscribeReq,
			   sizeof(struct NanSubscribeRequest));
		kalMemZero(pNanSubscribeRsp,
			   sizeof(struct NanSubscribeServiceRspMsg));

		/* WIFI HAL will set nanMsgHdr.handle to 0xFFFF
		 * if subscribe_id is 0. (means new subscribe)
		 */
		if (nanMsgHdr.handle != 0xFFFF)
			pNanSubscribeReq->subscribe_id = nanMsgHdr.handle;

		/* Mapping subscribe req related parameters */
		readLen =
			nanMapSubscribeReqParams((u16 *)data, pNanSubscribeReq);
		remainingLen -= readLen;
		data += readLen;
		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_SERVICE_NAME:
				if (outputTlv.length >
					NAN_MAX_SERVICE_NAME_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memset(g_aucNanServiceName, 0,
					NAN_MAX_SERVICE_NAME_LEN);
				memcpy(pNanSubscribeReq->service_name,
				       outputTlv.value, outputTlv.length);
				memcpy(g_aucNanServiceName,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->service_name_len =
					outputTlv.length;
				DBGLOG(NAN, INFO,
				"SERVICE_NAME type:%u len:%u SRV_name:%s\n",
				outputTlv.type,
				outputTlv.length,
				pNanSubscribeReq->service_name);
				break;
			case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, INFO,
					"SERVICE_SPECIFIC_INFO type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);
				DBGLOG_HEX(NAN, INFO,
					pNanSubscribeReq->service_specific_info,
					outputTlv.length);
				break;
			case NAN_TLV_TYPE_RX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->rx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->rx_match_filter_len =
					outputTlv.length;
				if (au2DebugModule[DBG_NAN_IDX]
					& DBG_CLASS_INFO) {
					DBGLOG(NAN, INFO,
					"RX_MATCH_FILTER type:%u len:%u rx_match_filter:%s\n",
					outputTlv.type,
					outputTlv.length,
					pNanSubscribeReq
					->rx_match_filter);
					dumpMemory8((uint8_t *)pNanSubscribeReq
							    ->rx_match_filter,
						    outputTlv.length);
				}
				break;
			case NAN_TLV_TYPE_TX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->tx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->tx_match_filter_len =
					outputTlv.length;
				DBGLOG(NAN, INFO,
					"TX_MATCH_FILTERtype:%u len:%u\n",
					outputTlv.type, outputTlv.length);
				DBGLOG_HEX(NAN, INFO,
					   pNanSubscribeReq->tx_match_filter,
					   outputTlv.length);
				break;
			case NAN_TLV_TYPE_MAC_ADDRESS:
				if (outputTlv.length >
					sizeof(uint8_t)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				if (i < NAN_MAX_SUBSCRIBE_MAX_ADDRESS) {
					/* Get column neumbers */
					memcpy(pNanSubscribeReq->intf_addr[i],
					     outputTlv.value, outputTlv.length);
					i++;
				}
				break;
			case NAN_TLV_TYPE_NAN_CSID:
				pNanSubscribeReq->cipher_type =
					*(outputTlv.value);
				DBGLOG(NAN, INFO, "NAN_CSID type:%u len:%u\n",
				       outputTlv.type, outputTlv.length);
				break;
			case NAN_TLV_TYPE_NAN_PMK:
				if (outputTlv.length >
					NAN_PMK_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->key_info.body.pmk_info
					       .pmk,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->key_info.body.pmk_info
					.pmk_len = outputTlv.length;
				break;
			case NAN_TLV_TYPE_NAN_PASSPHRASE:
				if (outputTlv.length >
					NAN_SECURITY_MAX_PASSPHRASE_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->key_info.body
					       .passphrase_info.passphrase,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->key_info.body.passphrase_info
					.passphrase_len = outputTlv.length;
				DBGLOG(NAN, INFO,
					"NAN_PASSPHRASE type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);
				break;
			case NAN_TLV_TYPE_SDEA_CTRL_PARAMS:
				nanMapSdeaCtrlParams(
					(u32 *)outputTlv.value,
					&pNanSubscribeReq->sdea_params);
				DBGLOG(NAN, INFO,
					"SDEA_CTRL_PARAMS type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN_RANGING_CFG:
				fgRangingCFG = TRUE;
				DBGLOG(NAN, INFO, "fgRangingCFG %d\n",
					fgRangingCFG);
				nanMapRangingConfigParams(
					(u32 *)outputTlv.value,
					&pNanSubscribeReq->ranging_cfg);
				break;
			case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SDEA_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq
					->sdea_service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq
					->sdea_service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, INFO,
				"SDEA_SERVICE_SPECIFIC_INFO type:%u len:%u\n",
				outputTlv.type,
				outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN20_RANGING_REQUEST:
				fgRangingREQ = TRUE;
				DBGLOG(NAN, INFO, "fgRangingREQ %d\n",
					fgRangingREQ);
				nanMapNan20RangingReqParams(
					(u32 *)outputTlv.value,
					&pNanSubscribeReq->range_response_cfg);
				break;
#if CFG_SUPPORT_NAN_R4_PAIRING /* NAN_PAIRING */
			case NAN_TLV_TYPE_NAN40_PAIRING_CAPABILITY:
				nanMapSubscribePairingReqParams(
				(u32 *)outputTlv.value, pNanSubscribeReq);
				break;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

#if CFG_SUPPORT_NAN_R4_PAIRING /* NAN_PAIRING */
		if (prAdapter->fgIsNANfromHAL == TRUE) {
			DBGLOG(NAN, INFO,
			"[pairing-disc][%s][pairing_enable=%u]\n",
			__func__, pNanSubscribeReq->pairing_enable);
		} else {
			pNanSubscribeReq->pairing_enable =
				g_pairingPubReq.ucPairingSetupEnabled;
			pNanSubscribeReq->bootstrap_method =
				g_bootstrapMethod.ucBootstrapMethod;
			nanClearPairingGlobalSettings(prAdapter);
		}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

		/* Prepare command reply of Subscriabe response */
		memcpy(&pNanSubscribeRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanSubscribeServiceRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanSubscribeReq);
			kfree(pNanSubscribeRsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb,
				     sizeof(struct NanSubscribeServiceRspMsg),
				     pNanSubscribeRsp) < 0)) {
			kfree(pNanSubscribeReq);
			kfree(pNanSubscribeRsp);
			kfree_skb(skb);
			return -EFAULT;
		}
		/* Ranging */
		if (fgRangingCFG && fgRangingREQ) {

			struct NanRangeRequest *rgreq = NULL;
			uint16_t rgId = 0;
			uint32_t rStatus;

			rgreq = kmalloc(sizeof(struct NanRangeRequest),
				GFP_ATOMIC);

			if (!rgreq) {
				DBGLOG(NAN, ERROR, "Allocate failed\n");
				kfree(pNanSubscribeReq);
				kfree(pNanSubscribeRsp);
				kfree_skb(skb);
				return -ENOMEM;
			}

			kalMemZero(rgreq, sizeof(struct NanRangeRequest));

			memcpy(&rgreq->peer_addr,
				&pNanSubscribeReq->range_response_cfg.peer_addr,
				NAN_MAC_ADDR_LEN);
			memcpy(&rgreq->ranging_cfg,
				&pNanSubscribeReq->ranging_cfg,
				sizeof(struct NanRangingCfg));
			rgreq->range_id =
			pNanSubscribeReq->range_response_cfg
				.requestor_instance_id;
			DBGLOG(NAN, INFO, MACSTR
				" id %d reso %d intev %d indicat %d ING CM %d ENG CM %d\n",
				MAC2STR(rgreq->peer_addr),
				rgreq->range_id,
				rgreq->ranging_cfg.ranging_resolution,
				rgreq->ranging_cfg.ranging_interval_msec,
				rgreq->ranging_cfg.config_ranging_indications,
				rgreq->ranging_cfg.distance_ingress_cm,
				rgreq->ranging_cfg.distance_egress_cm);
			rStatus =
			nanRangingRequest(prGlueInfo->prAdapter, &rgId, rgreq);

#if CFG_SUPPORT_NAN_EXT
			nanExtTerminateApNan(prAdapter,
				NAN_ASC_EVENT_ASCC_END_LEGACY);
#endif

			pNanSubscribeRsp->fwHeader.handle = rgId;
			i4Status = kalIoctl(prGlueInfo, wlanoidNanSubscribeRsp,
				       (void *)pNanSubscribeRsp,
				       sizeof(struct NanSubscribeServiceRspMsg),
				       &u4BufLen);
			if (i4Status != WLAN_STATUS_SUCCESS) {
				DBGLOG(NAN, ERROR, "kalIoctl failed\n");
				kfree(pNanSubscribeReq);
				kfree(rgreq);
				kfree_skb(skb);
				return -EFAULT;
			}
			kfree(rgreq);
			kfree(pNanSubscribeReq);
			ret = cfg80211_vendor_cmd_reply(skb);
			break;

		}

		prAdapter->fgIsNANfromHAL = TRUE;

		/* return subscribe ID */
		Subscribe_id = (uint16_t)nanSubscribeRequest(
			prGlueInfo->prAdapter, pNanSubscribeReq);
		/* NAN_CHK_PNT log message */
		if (nanMsgHdr.handle == 0xFFFF) {
			DBGLOG(NAN, VOC,
			       "[NAN_CHK_PNT] NAN_NEW_SUBSCRIBE subscribe_id/handle=%u\n",
			       Subscribe_id);
		}

		pNanSubscribeRsp->fwHeader.handle = Subscribe_id;

		DBGLOG(NAN, VOC,
		       "Subscribe_id:%u, pNanSubscribeRsp->fwHeader.handle:%u\n",
		       Subscribe_id, pNanSubscribeRsp->fwHeader.handle);
		i4Status = kalIoctl(prGlueInfo, wlanoidNanSubscribeRsp,
				    (void *)pNanSubscribeRsp,
				    sizeof(struct NanSubscribeServiceRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree(pNanSubscribeReq);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanSubscribeReq);
		break;
	}
	case NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_REQ: {
		uint32_t rStatus;
		struct NanSubscribeCancelRequest *pNanSubscribeCancelReq = NULL;
		struct NanSubscribeServiceCancelRspMsg *pNanSubscribeCancelRsp =
			NULL;

		pNanSubscribeCancelReq = kmalloc(
			sizeof(struct NanSubscribeCancelRequest), GFP_ATOMIC);
		if (!pNanSubscribeCancelReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanSubscribeCancelRsp =
			kmalloc(sizeof(struct NanSubscribeServiceCancelRspMsg),
				GFP_ATOMIC);
		if (!pNanSubscribeCancelRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanSubscribeCancelReq);
			return -ENOMEM;
		}
		kalMemZero(pNanSubscribeCancelReq,
			   sizeof(struct NanSubscribeCancelRequest));
		kalMemZero(pNanSubscribeCancelRsp,
			   sizeof(struct NanSubscribeServiceCancelRspMsg));

		DBGLOG(NAN, INFO, "Enter CANCEL Subscribe Request\n");
		pNanSubscribeCancelReq->subscribe_id = nanMsgHdr.handle;

		DBGLOG(NAN, INFO, "PID %d\n",
		       pNanSubscribeCancelReq->subscribe_id);
		rStatus = nanCancelSubscribeRequest(prGlueInfo->prAdapter,
						    pNanSubscribeCancelReq);

		/* Prepare Cancel Subscribe command reply message */
		memcpy(&pNanSubscribeCancelRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanSubscribeServiceCancelRspMsg));
		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanSubscribeCancelReq);
			kfree(pNanSubscribeCancelRsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb,
				     sizeof(struct
					    NanSubscribeServiceCancelRspMsg),
				     pNanSubscribeCancelRsp) < 0)) {
			kfree(pNanSubscribeCancelReq);
			kfree(pNanSubscribeCancelRsp);
			kfree_skb(skb);
			return -EFAULT;
		}

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "CANCEL Subscribe Error %X\n",
			       rStatus);
			pNanSubscribeCancelRsp->status =
				NAN_I_STATUS_DE_FAILURE;
		} else {
			DBGLOG(NAN, INFO, "CANCEL Subscribe Success %X\n",
			       rStatus);
			pNanSubscribeCancelRsp->status = NAN_I_STATUS_SUCCESS;
		}

		i4Status =
			kalIoctl(prGlueInfo, wlanoidNANCancelSubscribeRsp,
				 (void *)pNanSubscribeCancelRsp,
				 sizeof(struct NanSubscribeServiceCancelRspMsg),
				 &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree(pNanSubscribeCancelReq);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanSubscribeCancelReq);
		break;
	}

	case NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ: {
		uint32_t rStatus;
		struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;
		struct NanTransmitFollowupRspMsg *pNanXmitFollowupRsp = NULL;
#if CFG_SUPPORT_NAN_R4_PAIRING
		struct NanPairingBootStrapMsg bootstrap_msg = {0};
		struct NanTransmitFollowupRspMsg_p NanXmitFollowupRsp_p = {0};
#endif

		pNanXmitFollowupReq = kmalloc(
			sizeof(struct NanTransmitFollowupRequest), GFP_ATOMIC);

		if (!pNanXmitFollowupReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanXmitFollowupRsp = kmalloc(
			sizeof(struct NanTransmitFollowupRspMsg), GFP_ATOMIC);
		if (!pNanXmitFollowupRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanXmitFollowupReq);
			return -ENOMEM;
		}
		kalMemZero(pNanXmitFollowupReq,
			   sizeof(struct NanTransmitFollowupRequest));
		kalMemZero(pNanXmitFollowupRsp,
			   sizeof(struct NanTransmitFollowupRspMsg));

		DBGLOG(NAN, VOC, "Enter Transmit follow up Request\n");

		/* Mapping publish req related parameters */
		readLen = nanMapFollowupReqParams((u32 *)data,
						  pNanXmitFollowupReq);
		remainingLen -= readLen;
		data += readLen;
		pNanXmitFollowupReq->publish_subscribe_id = nanMsgHdr.handle;

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_MAC_ADDRESS:
				if (outputTlv.length >
					NAN_MAC_ADDR_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(pNanXmitFollowupReq->addr,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(pNanXmitFollowupReq
					       ->service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanXmitFollowupReq->service_specific_info_len =
					outputTlv.length;
				break;
			case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_FW_MAX_TX_FOLLOW_UP_SDEA_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(pNanXmitFollowupReq
					       ->sdea_service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanXmitFollowupReq
					->sdea_service_specific_info_len =
					outputTlv.length;
				break;
#if CFG_SUPPORT_NAN_R4_PAIRING
			case NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING:
				if (outputTlv.length >
					sizeof(struct NanPairingBootStrapMsg)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(&bootstrap_msg, outputTlv.value,
					sizeof(struct NanPairingBootStrapMsg));
				DBGLOG(NAN, ERROR,
				PREL1"FollowupReq: [boot_type=%d]\n",
				bootstrap_msg.bootstrap_type);
				DBGLOG(NAN, ERROR,
				PREL1"FollowupReq: [boot_method=0x%02x]\n",
				bootstrap_msg.bootstrap_method);
				DBGLOG(NAN, ERROR,
				PREL1"FollowupReq: [boot_status=0x%02x]\n",
				bootstrap_msg.bootstrap_status);
				DBGLOG(NAN, ERROR,
				PREL1"FollowupReq: [u2ComebackAfter=0x%02x]\n",
				bootstrap_msg.u2ComebackAfter);

				pNanXmitFollowupReq->bootstrap_type =
					bootstrap_msg.bootstrap_type;
				pNanXmitFollowupReq->bootstrap_method =
					bootstrap_msg.bootstrap_method;
				pNanXmitFollowupReq->bootstrap_status =
					bootstrap_msg.bootstrap_status;
				if (bootstrap_msg.u2ComebackAfter > 0) {
					pNanXmitFollowupReq->comeback_after
					= bootstrap_msg.u2ComebackAfter;
					pNanXmitFollowupReq->comeback_enable
					= TRUE;
				}
				break;
#endif
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}



#if CFG_SUPPORT_NAN_R4_PAIRING
		/* FIXME: Refactor follow up of pairing command to
		 * bootstramping req/resp command, WiFi HAL should be
		 updated for pairing.
		 */
		DBGLOG(NAN, INFO,
		   "[%s]: pNanXmitFollowupReq->addr=>"MACSTR_A"\n",
		   __func__,
		   pNanXmitFollowupReq->addr[0], pNanXmitFollowupReq->addr[1],
		   pNanXmitFollowupReq->addr[2], pNanXmitFollowupReq->addr[3],
		   pNanXmitFollowupReq->addr[4], pNanXmitFollowupReq->addr[5]);

		if (g_nikExchange.ucNanIdKey)
			pNanXmitFollowupReq->nan_id_key = TRUE;

		if (g_pairingSetupCmd.ucPairingSetup ||
			g_pairingSetupCmd.ucPairingVerification) {
			pNanXmitFollowupReq->pairing_verification =
				g_pairingSetupCmd.ucPairingVerification;
			pNanXmitFollowupReq->bootstrap_method =
				g_pairingSetupCmd.ucBootstrapMethod;
			pNanXmitFollowupReq->pairing_type =
				g_pairingSetupCmd.ucPairingType;
			nanCmdPairingSetupReq(prAdapter, pNanXmitFollowupReq);
			nanClearPairingGlobalSettings(prAdapter);
			kfree(pNanXmitFollowupReq);
			return WLAN_STATUS_SUCCESS;
		}
#endif
#if CFG_SUPPORT_NAN_R4_PAIRING
		if (prAdapter->fgIsNANfromHAL == TRUE) {
			DBGLOG(NAN, INFO,
			PREL1"[%s] Method=%u\n]",
			__func__,
			pNanXmitFollowupReq->bootstrap_method);
			DBGLOG(NAN, INFO,
			PREL1"[%s] Type=%u\n]",
			__func__, pNanXmitFollowupReq->bootstrap_type);
			DBGLOG(NAN, INFO,
			PREL1"[%s] Status=%u\n]",
			__func__,
			pNanXmitFollowupReq->bootstrap_status);
			DBGLOG(NAN, INFO,
			PREL1"[%s] comeback_after=%u\n]",
			__func__,
			pNanXmitFollowupReq->comeback_after);
		} else {
			if (g_bootstrapMethod.ucBootstrapMethod) {
				pNanXmitFollowupReq->bootstrap_method =
				g_bootstrapMethod.ucBootstrapMethod;
				pNanXmitFollowupReq->bootstrap_type =
				g_bootstrapMethod.ucBootstrapType;
				pNanXmitFollowupReq->bootstrap_status =
				g_bootstrapMethod.ucBootstrapStatus;
				pNanXmitFollowupReq->comeback_enable =
				g_bootstrapMethod.ucComebackEnabled;
				pNanXmitFollowupReq->comeback_after =
				g_bootstrapMethod.u2ComebackAfter;
			}
			nanClearPairingGlobalSettings(prAdapter);
		}
#endif

		/* Follow up Command reply message */
		memcpy(&pNanXmitFollowupRsp->fwHeader, &nanMsgHdr,
				sizeof(struct _NanMsgHeader));

		pNanXmitFollowupReq->transaction_id =
			pNanXmitFollowupRsp->fwHeader.transactionId;

		skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			sizeof(struct NanTransmitFollowupRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanXmitFollowupReq);
			kfree(pNanXmitFollowupRsp);
			return -ENOMEM;
		}

		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct NanTransmitFollowupRspMsg),
			pNanXmitFollowupRsp) < 0)) {
			kfree(pNanXmitFollowupReq);
			kfree(pNanXmitFollowupRsp);
			kfree_skb(skb);
			DBGLOG(NAN, ERROR, "Fail send reply\n");
			return -EFAULT;
		}

		rStatus = nanTransmitRequest(prGlueInfo->prAdapter,
				pNanXmitFollowupReq);
		if (rStatus != WLAN_STATUS_SUCCESS)
			pNanXmitFollowupRsp->status =
				NAN_I_STATUS_DE_FAILURE;
		else
			pNanXmitFollowupRsp->status =
				NAN_I_STATUS_SUCCESS;

#if CFG_SUPPORT_NAN_EXT
		nanExtTerminateApNan(prAdapter, NAN_ASC_EVENT_ASCC_END_LEGACY);
#endif

#if CFG_SUPPORT_NAN_R4_PAIRING
		NanXmitFollowupRsp_p.pfollowRsp = pNanXmitFollowupRsp;
		NanXmitFollowupRsp_p.bootstrap_type =
			pNanXmitFollowupReq->bootstrap_type;
		NanXmitFollowupRsp_p.bootstrap_status =
			pNanXmitFollowupReq->bootstrap_status;
		i4Status = kalIoctl(prGlueInfo, wlanoidNANFollowupRsp,
				    (void *)&NanXmitFollowupRsp_p,
				    sizeof(struct NanTransmitFollowupRspMsg_p),
				    &u4BufLen);
#else
		i4Status = kalIoctl(prGlueInfo, wlanoidNANFollowupRsp,
				    (void *)pNanXmitFollowupRsp,
				    sizeof(struct NanTransmitFollowupRspMsg),
				    &u4BufLen);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree(pNanXmitFollowupReq);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanXmitFollowupReq);
		break;
	} /* NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ */

	case NAN_MSG_ID_BEACON_SDF_REQ: {
		u16 vsa_length = 0;
		u32 *pXmitVSAparms = NULL;
		struct NanTransmitVendorSpecificAttribute *pNanXmitVSAttrReq =
			NULL;
		struct NanBeaconSdfPayloadRspMsg *pNanBcnSdfVSARsp = NULL;

		DBGLOG(NAN, INFO, "Enter Beacon SDF Request.\n");

		pNanXmitVSAttrReq = kmalloc(
			sizeof(struct NanTransmitVendorSpecificAttribute),
			GFP_ATOMIC);

		if (!pNanXmitVSAttrReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		pNanBcnSdfVSARsp = kmalloc(
			sizeof(struct NanBeaconSdfPayloadRspMsg), GFP_ATOMIC);

		if (!pNanBcnSdfVSARsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanXmitVSAttrReq);
			return -ENOMEM;
		}

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_TRANSMIT:
				pXmitVSAparms = (u32 *)outputTlv.value;
				pNanXmitVSAttrReq->payload_transmit_flag =
					(u8)(*pXmitVSAparms & BIT(0));
				pNanXmitVSAttrReq->tx_in_discovery_beacon =
					(u8)(*pXmitVSAparms & BIT(1));
				pNanXmitVSAttrReq->tx_in_sync_beacon =
					(u8)(*pXmitVSAparms & BIT(2));
				pNanXmitVSAttrReq->tx_in_service_discovery =
					(u8)(*pXmitVSAparms & BIT(3));
				pNanXmitVSAttrReq->vendor_oui =
					*pXmitVSAparms & BITS(8, 31);
				outputTlv.value += 4;

				vsa_length = outputTlv.length - sizeof(u32);
				if (vsa_length >
					NAN_MAX_VSA_DATA_LEN) {
					DBGLOG(NAN, ERROR,
						"outputTlv.length is invalid!\n");
					kfree(pNanXmitVSAttrReq);
					kfree(pNanBcnSdfVSARsp);
					return -EFAULT;
				}
				memcpy(pNanXmitVSAttrReq->vsa, outputTlv.value,
				       vsa_length);
				pNanXmitVSAttrReq->vsa_len = vsa_length;
				break;
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		/* To be implement
		 * Beacon SDF VSA request.................................
		 * rStatus = ;
		 */

		/* Prepare Beacon Sdf Payload Response */
		memcpy(&pNanBcnSdfVSARsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		pNanBcnSdfVSARsp->fwHeader.msgId = NAN_MSG_ID_BEACON_SDF_RSP;
		pNanBcnSdfVSARsp->fwHeader.msgLen =
			sizeof(struct NanBeaconSdfPayloadRspMsg);

		pNanBcnSdfVSARsp->status = NAN_I_STATUS_SUCCESS;

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanBeaconSdfPayloadRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanXmitVSAttrReq);
			kfree(pNanBcnSdfVSARsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb,
				     sizeof(struct NanBeaconSdfPayloadRspMsg),
				     pNanBcnSdfVSARsp) < 0)) {
			kfree(pNanXmitVSAttrReq);
			kfree(pNanBcnSdfVSARsp);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanXmitVSAttrReq);
		kfree(pNanBcnSdfVSARsp);

		break;
	}
#if CFG_SUPPORT_NAN_R4_PAIRING
	case NAN_MSG_ID_PAIRING_REQUEST: {
		struct _NanPairingRequestParams pairingRequest;
		struct _NanPairingRequestRspMsg nanPairingRequestRsp;
		uint32_t rRetStatus = WLAN_STATUS_SUCCESS;


		DBGLOG(NAN, ERROR, "sizeof(NanPairingRequestParams)=%zu\n",
			sizeof(struct _NanPairingRequestParams));
		DBGLOG(NAN, ERROR, "sizeof(_NanMsgHeader)=%zu\n",
			sizeof(struct _NanMsgHeader));
		DBGLOG(NAN, ERROR, "data_len=%d\n", data_len);
		kalMemZero(&pairingRequest,
			sizeof(struct _NanPairingRequestParams));
		readLen = nanMapPairingRequestParams((u8 *)data,
				&pairingRequest);

		switch (pairingRequest.nan_pairing_request_type) {
		case NAN_PAIRING_SETUP_REQ_T: {
			rRetStatus = nanCommandPairingRequest_Pairing(prAdapter,
					&pairingRequest);
			break;
		}
		case NAN_PAIRING_VERIFICATION_REQ_T: {
			uint16_t publish_subscribe_id = nanMsgHdr.handle;

			rRetStatus = nanCommandPairingRequest_Verify(prAdapter,
					&pairingRequest, publish_subscribe_id);
			break;
		}
		default: {
			break;
		}
		}
		if (rRetStatus == WLAN_STATUS_SUCCESS)
			nanPairingRequestRsp.status = NAN_I_STATUS_SUCCESS;
		else
			nanPairingRequestRsp.status = NAN_I_STATUS_DE_FAILURE;

		memcpy(&nanPairingRequestRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct _NanPairingRequestRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct _NanPairingRequestRspMsg),
			&nanPairingRequestRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidNANPairingRequestRsp,
			(void *)&nanPairingRequestRsp,
			sizeof(struct _NanPairingRequestRspMsg),
			&u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
		DBGLOG(NAN, ERROR, "i4Status = %u\n", i4Status);

		ret = cfg80211_vendor_cmd_reply(skb);
		break;
	}
	case NAN_MSG_ID_PAIRING_RESPONSE: {
		struct _NanPairingResponseParams pairingResponse;
		struct _NanPairingResponseRspMsg nanPairingResponseRsp;
		uint32_t rRetStatus = WLAN_STATUS_SUCCESS;

		DBGLOG(NAN, INFO, "data_len=%d\n", data_len);
		kalMemZero(&pairingResponse,
			sizeof(struct _NanPairingResponseParams));
		readLen = nanMapPairingResponseParams((u8 *)data,
				&pairingResponse);
		data += readLen;
		remainingLen -= readLen;

		switch (pairingResponse.nan_pairing_request_type) {
		case NAN_PAIRING_SETUP_REQ_T: {
			rRetStatus = nanCommandPairingRespond_Pairing(prAdapter,
					&pairingResponse, &data, remainingLen);
			break;
		}
		case NAN_PAIRING_VERIFICATION_REQ_T: {
			uint64_t publish_subscribe_id = nanMsgHdr.handle;

			rRetStatus = nanCommandPairingRespond_Verify(prAdapter,
					&pairingResponse, &data, remainingLen,
					publish_subscribe_id);
			break;
		}
		default: {
			break;
		}
		}
		if (rRetStatus == WLAN_STATUS_SUCCESS)
			nanPairingResponseRsp.status = NAN_I_STATUS_SUCCESS;
		else
			nanPairingResponseRsp.status = NAN_I_STATUS_DE_FAILURE;

		memcpy(&nanPairingResponseRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct _NanPairingRequestRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct _NanPairingResponseRspMsg),
			&nanPairingResponseRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidNANPairingResponseRsp,
				    (void *)&nanPairingResponseRsp,
				    sizeof(struct _NanPairingResponseRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			return -EFAULT;
		}
		DBGLOG(NAN, INFO, "i4Status = %u\n", i4Status);
		ret = cfg80211_vendor_cmd_reply(skb);

		break;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	case NAN_MSG_ID_TESTMODE_REQ:
	{
		struct NanDebugParams *pNanDebug = NULL;

		pNanDebug = kmalloc(sizeof(struct NanDebugParams), GFP_ATOMIC);
		if (!pNanDebug) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		kalMemZero(pNanDebug, sizeof(struct NanDebugParams));
		DBGLOG(NAN, INFO, "NAN_MSG_ID_TESTMODE_REQ\n");

		while ((remainingLen >= 4) &&
			(0 != (readLen = nan_read_tlv((u8 *)data,
			&outputTlv)))) {
			DBGLOG(NAN, INFO, "outputTlv.type= %d\n",
				outputTlv.type);
			if (outputTlv.type ==
				NAN_TLV_TYPE_TESTMODE_GENERIC_CMD) {
				if (outputTlv.length >
					sizeof(
					struct NanDebugParams
					)) {
					DBGLOG(NAN, ERROR,
						"outputTlv.length is invalid!\n");
					kfree(pNanDebug);
					return -EFAULT;
				}
				memcpy(pNanDebug, outputTlv.value,
					outputTlv.length);
				switch (pNanDebug->cmd) {
				case NAN_TEST_MODE_CMD_DISABLE_NDPE:
					g_ndpReqNDPE.fgEnNDPE = TRUE;
					g_ndpReqNDPE.ucNDPEAttrPresent =
						pNanDebug->
						debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN_TEST_MODE_CMD_DISABLE_NDPE: fgEnNDPE = %d\n",
						g_ndpReqNDPE.fgEnNDPE);
					break;
#if CFG_SUPPORT_NAN_R4_PAIRING
				case NAN_TEST_MODE_CMD_ENABLE_PAIRING:
					g_pairingPubReq.ucPairingSetupEnabled =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Pairing Enabled = %u\n",
					g_pairingPubReq.ucPairingSetupEnabled);
					break;
				case NAN_TEST_MODE_CMD_CIPHERSUITEI_ID:
					g_bootstrapMethod.ucCipherSuiteId =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Cipher suite id = %u\n",
					g_bootstrapMethod.ucCipherSuiteId);
					break;
				case NAN_TEST_MODE_CMD_CIPHERSUITEI_ID_LIST:
					g_pairingPubReq.aucCipherSuiteList[0] =
						pNanDebug->debug_cmd_data[0];
					g_pairingPubReq.aucCipherSuiteList[1] =
						pNanDebug->debug_cmd_data[1];
					DBGLOG(NAN, INFO,
					"NAN Cipher Suite Id List = %u, %u\n",
					g_pairingPubReq.aucCipherSuiteList[0],
					g_pairingPubReq.aucCipherSuiteList[1]);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_METHOD:
					g_pairingPubReq.ucBootstrapMethod =
						pNanDebug->debug_cmd_data[0];
					g_bootstrapMethod.ucBootstrapMethod =
						pNanDebug->debug_cmd_data[0];
					g_pairingSetupCmd.ucBootstrapMethod =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Method = %u\n",
					g_bootstrapMethod.ucBootstrapMethod);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_TYPE:
					g_bootstrapMethod.ucBootstrapType =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Type = %u\n",
					g_bootstrapMethod.ucBootstrapType);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_STATUS:
					g_bootstrapMethod.ucBootstrapStatus =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Status = %u\n",
					g_bootstrapMethod.ucBootstrapStatus);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_COMEBACK:
					g_bootstrapMethod.ucComebackEnabled =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Comeback = %u\n",
					g_bootstrapMethod.ucComebackEnabled);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_COMEBACK_AFTER:
					g_bootstrapMethod.u2ComebackAfter =
					pNanDebug->debug_cmd_data[0] +
					pNanDebug->debug_cmd_data[1] * 256;
					DBGLOG(NAN, INFO,
					"NAN Bootstrap ComebackAfter = %u\n",
					g_bootstrapMethod.u2ComebackAfter);
					DBGLOG(NAN, INFO,
					"[NAN] check comeback input = %u, %u\n",
					pNanDebug->debug_cmd_data[0],
					pNanDebug->debug_cmd_data[1]);
					break;
				case NAN_TEST_MODE_CMD_NAN_ID_KEY:
					g_nikExchange.ucNanIdKey =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN NIK Id Key = %u\n",
						g_nikExchange.ucNanIdKey);
					break;
				case NAN_TEST_MODE_CMD_PAIRING_SETUP:
					g_pairingSetupCmd.ucPairingSetup =
						TRUE;
					g_pairingSetupCmd.ucPairingType =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Pairing Setup Type = %u\n",
					g_pairingSetupCmd.ucPairingType);
					break;
				case NAN_TEST_MODE_CMD_PAIRING_VERIFICATION:
					g_pairingSetupCmd.
						ucPairingVerification = TRUE;
					g_pairingSetupCmd.ucPairingType =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Pairing Verification Type = %u\n",
					g_pairingSetupCmd.ucPairingType);
					break;
				case NAN_TEST_MODE_CMD_NIRA_PRESENCE:
					g_pairingPubReq.ucNiraEnabled =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN NIRA Presense = %u\n",
						g_pairingPubReq.ucNiraEnabled);
					break;
				case NAN_TEST_MODE_CMD_NPK_NIK_CACHE:
					g_pairingPubReq.ucNPKNIKCache =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN NPK/NIK Cache = %u\n",
						g_pairingPubReq.ucNPKNIKCache);
					break;
				case NAN_TEST_MODE_CMD_FOLLOWUP_TYPE:
					/* from vendor command, req:0, rsp:1 */
					/* for NPBA,0:advertise, 1:req, 2:rsp */
					g_bootstrapMethod.ucBootstrapType =
					pNanDebug->debug_cmd_data[0] + 1;
					DBGLOG(NAN, INFO,
					"NAN Bootstrap type = %u\n",
					g_bootstrapMethod.ucBootstrapType);
					break;
				case NAN_TEST_MODE_CMD_PASSWORD_PINCODE:
				case NAN_TEST_MODE_CMD_PASSWORD_PASSPHRASE:
					kalMemCpyS(g_bootstrapPassword.password,
						NAN_PAIRING_MAX_PASSWORD_SIZE,
						pNanDebug->debug_cmd_data,
						NAN_PAIRING_MAX_PASSWORD_SIZE);
					g_bootstrapPassword.password_len =
						NAN_PAIRING_MAX_PASSWORD_SIZE;
					DBGLOG(NAN, INFO,
						"NAN Bootstrap Password = %s\n",
						pNanDebug->debug_cmd_data);
					nanCmdBootstrapPwdSetup(prAdapter,
						&g_bootstrapPassword);
					break;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
				default:
					break;
				}
			} else {
				DBGLOG(NAN, ERROR,
					"Testmode invalid TLV type\n");
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		kfree(pNanDebug);
		return 0;
	}
	default:
		return -EOPNOTSUPP;
	}
	return ret;
}

/* Indication part */
int
mtk_cfg80211_vendor_event_nan_event_indication(struct ADAPTER *prAdapter,
					       uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NanEventIndMsg *prNanEventInd;
	struct NAN_DE_EVENT *prDeEvt;
	uint16_t u2EventType;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;

	prDeEvt = (struct NAN_DE_EVENT *) pcuEvtBuf;

	if (prDeEvt == NULL) {
		DBGLOG(NAN, ERROR, "pcuEvtBuf is null\n");
		return -EFAULT;
	}

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	/*Final length includes all TLVs*/
	message_len = sizeof(struct _NanMsgHeader) +
		SIZEOF_TLV_HDR + MAC_ADDR_LEN;

	prNanEventInd = kalMemAlloc(message_len, VIR_MEM_TYPE);
	if (!prNanEventInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	prNanEventInd->fwHeader.msgVersion = 1;
	prNanEventInd->fwHeader.msgId = NAN_MSG_ID_DE_EVENT_IND;
	prNanEventInd->fwHeader.msgLen = message_len;
	prNanEventInd->fwHeader.handle = 0;
	prNanEventInd->fwHeader.transactionId = 0;

	tlvs = prNanEventInd->ptlv;


	if (prDeEvt->ucEventType != NAN_EVENT_ID_DISC_MAC_ADDR) {
		DBGLOG(NAN, INFO, "ClusterId=%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->ucClusterId[0], prDeEvt->ucClusterId[1],
		       prDeEvt->ucClusterId[2], prDeEvt->ucClusterId[3],
		       prDeEvt->ucClusterId[4], prDeEvt->ucClusterId[5]);
		/* NAN_CHK_PNT log message */
		if (prDeEvt->ucEventType == NAN_EVENT_ID_STARTED_CLUSTER) {
			DBGLOG(NAN, VOC,
				"[NAN_CHK_PNT] NAN_START_CLUSTER new_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x (NMI)\n",
			       prDeEvt->ucOwnNmi[0], prDeEvt->ucOwnNmi[1],
			       prDeEvt->ucOwnNmi[2], prDeEvt->ucOwnNmi[3],
			       prDeEvt->ucOwnNmi[4], prDeEvt->ucOwnNmi[5]);
			DBGLOG(NAN, VOC,
			       "[NAN_CHK_PNT] NAN_START_CLUSTER cluster_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
			       prDeEvt->ucClusterId[0],
			       prDeEvt->ucClusterId[1],
			       prDeEvt->ucClusterId[2],
			       prDeEvt->ucClusterId[3],
			       prDeEvt->ucClusterId[4],
			       prDeEvt->ucClusterId[5]);
		} else if (prDeEvt->ucEventType ==
			   NAN_EVENT_ID_JOINED_CLUSTER) {
			DBGLOG(NAN, VOC,
			       "[NAN_CHK_PNT] NAN_JOIN_CLUSTER cluster_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
			       prDeEvt->ucClusterId[0],
			       prDeEvt->ucClusterId[1],
			       prDeEvt->ucClusterId[2],
			       prDeEvt->ucClusterId[3],
			       prDeEvt->ucClusterId[4],
			       prDeEvt->ucClusterId[5]);
		}
		DBGLOG(NAN, INFO,
		       "AnchorMasterRank=%02x%02x%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->aucAnchorMasterRank[0],
		       prDeEvt->aucAnchorMasterRank[1],
		       prDeEvt->aucAnchorMasterRank[2],
		       prDeEvt->aucAnchorMasterRank[3],
		       prDeEvt->aucAnchorMasterRank[4],
		       prDeEvt->aucAnchorMasterRank[5],
		       prDeEvt->aucAnchorMasterRank[6],
		       prDeEvt->aucAnchorMasterRank[7]);
		DBGLOG(NAN, INFO, "MyNMI=%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->ucOwnNmi[0], prDeEvt->ucOwnNmi[1],
		       prDeEvt->ucOwnNmi[2], prDeEvt->ucOwnNmi[3],
		       prDeEvt->ucOwnNmi[4], prDeEvt->ucOwnNmi[5]);
		DBGLOG(NAN, INFO, "MasterNMI=%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->ucMasterNmi[0], prDeEvt->ucMasterNmi[1],
		       prDeEvt->ucMasterNmi[2], prDeEvt->ucMasterNmi[3],
		       prDeEvt->ucMasterNmi[4], prDeEvt->ucMasterNmi[5]);
	}

	if (prDeEvt->ucEventType == NAN_EVENT_ID_DISC_MAC_ADDR)
		u2EventType = NAN_TLV_TYPE_EVENT_SELF_STATION_MAC_ADDRESS;
	else if (prDeEvt->ucEventType == NAN_EVENT_ID_STARTED_CLUSTER)
		u2EventType = NAN_TLV_TYPE_EVENT_STARTED_CLUSTER;
	else if (prDeEvt->ucEventType == NAN_EVENT_ID_JOINED_CLUSTER)
		u2EventType = NAN_TLV_TYPE_EVENT_JOINED_CLUSTER;
	else {
		kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);
		return WLAN_STATUS_SUCCESS;
	}

	nanExtComposeClusterEvent(prAdapter, prDeEvt);

	/* Add TLV datas */
	tlvs = nanAddTlv(u2EventType, MAC_ADDR_LEN, prDeEvt->ucClusterId, tlvs);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanEventInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);

	return WLAN_STATUS_SUCCESS;
}

int mtk_cfg80211_vendor_event_nan_disable_indication(
		struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NanDisableIndMsg *prNanDisableInd;
	struct NAN_DISABLE_EVENT *prDisableEvt;
	size_t message_len = 0;

	prDisableEvt = (struct NAN_DISABLE_EVENT *) pcuEvtBuf;

	if (prDisableEvt == NULL) {
		DBGLOG(NAN, ERROR, "pcuEvtBuf is null\n");
		return -EFAULT;
	}

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, AIS_DEFAULT_INDEX))
		->ieee80211_ptr;

	/*Final length includes all TLVs*/
	message_len = sizeof(struct _NanMsgHeader) +
			sizeof(u16) +
			sizeof(u16);

	prNanDisableInd = kalMemAlloc(message_len, VIR_MEM_TYPE);
	if (!prNanDisableInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	prNanDisableInd->fwHeader.msgVersion = 1;
	prNanDisableInd->fwHeader.msgId = NAN_MSG_ID_DISABLE_IND;
	prNanDisableInd->fwHeader.msgLen = message_len;
	prNanDisableInd->fwHeader.handle = 0;
	prNanDisableInd->fwHeader.transactionId = 0;

	prNanDisableInd->reason = 0;

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kalMemFree(prNanDisableInd, VIR_MEM_TYPE, message_len);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
		message_len, prNanDisableInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kalMemFree(prNanDisableInd, VIR_MEM_TYPE, message_len);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kalMemFree(prNanDisableInd, VIR_MEM_TYPE, message_len);

	g_enableNAN = TRUE;

	return WLAN_STATUS_SUCCESS;
}

/* Indication part */
int
mtk_cfg80211_vendor_event_nan_replied_indication(struct ADAPTER *prAdapter,
						 uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NAN_REPLIED_EVENT *prRepliedEvt = NULL;
	struct NanPublishRepliedIndMsg *prNanPubRepliedInd;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	prRepliedEvt = (struct NAN_REPLIED_EVENT *)pcuEvtBuf;

	/* Final length includes all TLVs */
	message_len = sizeof(struct _NanMsgHeader) +
		      sizeof(struct _NanPublishRepliedIndParams) +
		      ((SIZEOF_TLV_HDR) + MAC_ADDR_LEN) +
		      ((SIZEOF_TLV_HDR) + sizeof(prRepliedEvt->ucRssi_value));

	prNanPubRepliedInd = kmalloc(message_len, GFP_KERNEL);
	if (prNanPubRepliedInd == NULL)
		return -ENOMEM;

	kalMemZero(prNanPubRepliedInd, message_len);

	DBGLOG(NAN, INFO, "[%s] message_len : %lu\n", __func__, message_len);
	prNanPubRepliedInd->fwHeader.msgVersion = 1;
	prNanPubRepliedInd->fwHeader.msgId = NAN_MSG_ID_PUBLISH_REPLIED_IND;
	prNanPubRepliedInd->fwHeader.msgLen = message_len;
	prNanPubRepliedInd->fwHeader.handle = prRepliedEvt->u2Pubid;
	prNanPubRepliedInd->fwHeader.transactionId = 0;

	prNanPubRepliedInd->publishRepliedIndParams.matchHandle =
		prRepliedEvt->u2Subid;

	tlvs = prNanPubRepliedInd->ptlv;
	/* Add TLV datas */
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 &prRepliedEvt->auAddr[0], tlvs);

	tlvs = nanAddTlv(NAN_TLV_TYPE_RECEIVED_RSSI_VALUE,
			 sizeof(prRepliedEvt->ucRssi_value),
			 &prRepliedEvt->ucRssi_value, tlvs);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPubRepliedInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPubRepliedInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree(prNanPubRepliedInd);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);

	kfree(prNanPubRepliedInd);
	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_match_indication(struct ADAPTER *prAdapter,
					       uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NAN_DISCOVERY_EVENT *prDiscEvt;
	struct NanMatchIndMsg *prNanMatchInd;
	struct NanSdeaCtrlParams peer_sdea_params;
	struct NanFWSdeaCtrlParams nanPeerSdeaCtrlarms;
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanPairingCapabilityMsg PairingCapMsg;
	struct NanPairingNiraMsg  PairingNiraMsg;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	size_t message_len = 0;
	uint8_t *tlvs = NULL;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	kalMemZero(&nanPeerSdeaCtrlarms, sizeof(struct NanFWSdeaCtrlParams));
	kalMemZero(&peer_sdea_params, sizeof(struct NanSdeaCtrlParams));

	prDiscEvt = (struct NAN_DISCOVERY_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct _NanMsgHeader) +
		      sizeof(struct _NanMatchIndParams) +
		      (SIZEOF_TLV_HDR + MAC_ADDR_LEN) +
		      (SIZEOF_TLV_HDR + prDiscEvt->u2Service_info_len) +
		      (SIZEOF_TLV_HDR + prDiscEvt->ucSdf_match_filter_len) +
		      (SIZEOF_TLV_HDR + sizeof(struct NanFWSdeaCtrlParams));

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingCapabilityMsg));
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingNiraMsg));
		memset(&PairingCapMsg, 0,
			sizeof(struct NanPairingCapabilityMsg));
		memset(&PairingNiraMsg, 0,
			sizeof(struct NanPairingNiraMsg));
		g_last_matched_report_npba_dialogTok =
			prDiscEvt->ucPairingNPBA_DialogToken;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	prNanMatchInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanMatchInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	kalMemZero(prNanMatchInd, message_len);

	prNanMatchInd->fwHeader.msgVersion = 1;
	prNanMatchInd->fwHeader.msgId = NAN_MSG_ID_MATCH_IND;
	prNanMatchInd->fwHeader.msgLen = message_len;
	prNanMatchInd->fwHeader.handle = prDiscEvt->u2SubscribeID;
	prNanMatchInd->fwHeader.transactionId = 0;

	prNanMatchInd->matchIndParams.matchHandle = prDiscEvt->u2PublishID;
	prNanMatchInd->matchIndParams.matchOccuredFlag =
		0; /* means match in SDF */
	prNanMatchInd->matchIndParams.outOfResourceFlag =
		0; /* doesn't outof resource. */
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		PairingCapMsg.enable_pairing_setup
			= prDiscEvt->ucPairingEnable;
		PairingCapMsg.enable_pairing_cache
			= prDiscEvt->ucPairingCacheEnabled;
		PairingCapMsg.enable_pairing_verification
			= prDiscEvt->ucPairingVerificationEnabled;
		PairingCapMsg.supported_bootstrapping_methods
			= prDiscEvt->u2PairingBootstrapMethod;

		kalMemCpyS(&PairingNiraMsg.NiraTag,
		NAN_PAIRING_NIRA_CIPHERVERSION1_TAG_SIZE,
		&prDiscEvt->u8NiraTag, sizeof(uint8_t)*8);
		kalMemCpyS(&PairingNiraMsg.NiraNonce,
		NAN_PAIRING_NIRA_CIPHERVERSION1_NONCE_SIZE,
		&prDiscEvt->u8NiraNonce, sizeof(uint8_t)*8);
#if 0 /*pairing verification debug*/
	DBGLOG(NAN, ERROR, PREL0" nira Tag Dump!!\n");
	hexdump_nan((void *)&PairingNiraMsg.NiraTag,
	NAN_PAIRING_NIRA_CIPHERVERSION1_TAG_SIZE);
	DBGLOG(NAN, ERROR, PREL0" nira nonce Dump!!\n");
	hexdump_nan((void *)&PairingNiraMsg.NiraNonce,
	NAN_PAIRING_NIRA_CIPHERVERSION1_NONCE_SIZE);
#endif
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	tlvs = prNanMatchInd->ptlv;
	/* Add TLV datas */
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 &prDiscEvt->aucNanAddress[0], tlvs);
	DBGLOG(NAN, INFO, "[%s] :NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO %u\n",
	       __func__, NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO);

	tlvs = nanAddTlv(NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
			 prDiscEvt->u2Service_info_len,
			 &prDiscEvt->aucSerive_specificy_info[0], tlvs);

	tlvs = nanAddTlv(NAN_TLV_TYPE_SDF_MATCH_FILTER,
			 prDiscEvt->ucSdf_match_filter_len,
			 prDiscEvt->aucSdf_match_filter,
			 tlvs);
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_CAPABILITY,
			sizeof(struct NanPairingCapabilityMsg),
			(u8 *)&PairingCapMsg, tlvs);

		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_NIRA,
			sizeof(struct NanPairingNiraMsg),
			(u8 *)&PairingNiraMsg, tlvs);
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	nanPeerSdeaCtrlarms.data_path_required =
		(prDiscEvt->ucDataPathParm != 0) ? 1 : 0;
	nanPeerSdeaCtrlarms.security_required =
		(prDiscEvt->aucSecurityInfo[0] != 0) ? 1 : 0;
	nanPeerSdeaCtrlarms.ranging_required =
		(prDiscEvt->ucRange_measurement != 0) ? 1 : 0;

	DBGLOG(NAN, LOUD,
	       "[%s] data_path_required : %d, security_required:%d, ranging_required:%d\n",
	       __func__, nanPeerSdeaCtrlarms.data_path_required,
	       nanPeerSdeaCtrlarms.security_required,
	       nanPeerSdeaCtrlarms.ranging_required);

	/* NAN_CHK_PNT log message */
	DBGLOG(NAN, VOC,
	       "[NAN_CHK_PNT] NAN_NEW_MATCH_EVENT peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       prDiscEvt->aucNanAddress[0], prDiscEvt->aucNanAddress[1],
	       prDiscEvt->aucNanAddress[2], prDiscEvt->aucNanAddress[3],
	       prDiscEvt->aucNanAddress[4], prDiscEvt->aucNanAddress[5]);

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prDiscEvt->ucPairingEnable)
		pairingFsmPairedOrNot(prAdapter, prDiscEvt);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	tlvs = nanAddTlv(NAN_TLV_TYPE_SDEA_CTRL_PARAMS,
			 sizeof(struct NanFWSdeaCtrlParams),
			 (u8 *)&nanPeerSdeaCtrlarms, tlvs);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanMatchInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanMatchInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanMatchInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanMatchInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_publish_terminate(struct ADAPTER *prAdapter,
						uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NAN_PUBLISH_TERMINATE_EVENT *prPubTerEvt;
	struct NanPublishTerminatedIndMsg nanPubTerInd;
	struct _NAN_PUBLISH_SPECIFIC_INFO_T *prPubSpecificInfo = NULL;
	size_t message_len = 0;
	uint8_t i;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;
	kalMemZero(&nanPubTerInd, sizeof(struct NanPublishTerminatedIndMsg));
	prPubTerEvt = (struct NAN_PUBLISH_TERMINATE_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct NanPublishTerminatedIndMsg);

	nanPubTerInd.fwHeader.msgVersion = 1;
	nanPubTerInd.fwHeader.msgId = NAN_MSG_ID_PUBLISH_TERMINATED_IND;
	nanPubTerInd.fwHeader.msgLen = message_len;
	nanPubTerInd.fwHeader.handle = prPubTerEvt->u2Pubid;
	/* Indication doesn't have transactionId, don't care */
	nanPubTerInd.fwHeader.transactionId = 0;
	/* For all user should be success. */
	nanPubTerInd.reason = prPubTerEvt->ucReasonCode;
	prAdapter->rPublishInfo.ucNanPubNum--;

	DBGLOG(NAN, INFO, "Cancel Pub ID = %d, PubNum = %d\n",
	       nanPubTerInd.fwHeader.handle,
	       prAdapter->rPublishInfo.ucNanPubNum);

	for (i = 0; i < NAN_MAX_PUBLISH_NUM; i++) {
		prPubSpecificInfo =
			&prAdapter->rPublishInfo.rPubSpecificInfo[i];
		if (prPubSpecificInfo->ucPublishId == prPubTerEvt->u2Pubid) {
			prPubSpecificInfo->ucUsed = FALSE;
			if (prPubSpecificInfo->ucReportTerminate) {
				/* Fill skb and send to kernel by nl80211 */
				skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN,
					GFP_KERNEL);
				if (!skb) {
					DBGLOG(NAN, ERROR,
						"Allocate skb failed\n");
					return -ENOMEM;
				}
				if (unlikely(nla_put(skb,
					MTK_WLAN_VENDOR_ATTR_NAN,
					message_len,
					&nanPubTerInd) < 0)) {
					DBGLOG(NAN, ERROR,
						"nla_put_nohdr failed\n");
					kfree_skb(skb);
					return -EFAULT;
				}
				cfg80211_vendor_event(skb, GFP_KERNEL);
			}
		}
	}
	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_subscribe_terminate(struct ADAPTER *prAdapter,
						  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NAN_SUBSCRIBE_TERMINATE_EVENT *prSubTerEvt;
	struct NanSubscribeTerminatedIndMsg nanSubTerInd;
	struct _NAN_SUBSCRIBE_SPECIFIC_INFO_T *prSubSpecificInfo = NULL;
	size_t message_len = 0;
	uint8_t i;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;
	kalMemZero(&nanSubTerInd, sizeof(struct NanSubscribeTerminatedIndMsg));
	prSubTerEvt = (struct NAN_SUBSCRIBE_TERMINATE_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct NanSubscribeTerminatedIndMsg);

	nanSubTerInd.fwHeader.msgVersion = 1;
	nanSubTerInd.fwHeader.msgId = NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND;
	nanSubTerInd.fwHeader.msgLen = message_len;
	nanSubTerInd.fwHeader.handle = prSubTerEvt->u2Subid;
	/* Indication doesn't have transactionId, don't care */
	nanSubTerInd.fwHeader.transactionId = 0;
	/* For all user should be success. */
	nanSubTerInd.reason = prSubTerEvt->ucReasonCode;
	prAdapter->rSubscribeInfo.ucNanSubNum--;

	DBGLOG(NAN, INFO, "Cancel Sub ID = %d, SubNum = %d\n",
		nanSubTerInd.fwHeader.handle,
		prAdapter->rSubscribeInfo.ucNanSubNum);

	for (i = 0; i < NAN_MAX_SUBSCRIBE_NUM; i++) {
		prSubSpecificInfo =
			&prAdapter->rSubscribeInfo.rSubSpecificInfo[i];
		if (prSubSpecificInfo->ucSubscribeId == prSubTerEvt->u2Subid) {
			prSubSpecificInfo->ucUsed = FALSE;
			if (prSubSpecificInfo->ucReportTerminate) {
				/*  Fill skb and send to kernel by nl80211*/
				skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN,
					GFP_KERNEL);
				if (!skb) {
					DBGLOG(NAN, ERROR,
						"Allocate skb failed\n");
					return -ENOMEM;
				}
				if (unlikely(nla_put(skb,
					MTK_WLAN_VENDOR_ATTR_NAN,
					message_len,
					&nanSubTerInd) < 0)) {
					DBGLOG(NAN, ERROR,
						"nla_put_nohdr failed\n");
					kfree_skb(skb);
					return -EFAULT;
				}
				cfg80211_vendor_event(skb, GFP_KERNEL);
			}
		}
	}
	return WLAN_STATUS_SUCCESS;
}
#if CFG_SUPPORT_NAN_R4_PAIRING
int indicatePairingSuccess_resp(struct ADAPTER *prAdapter,
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt,
	struct PAIRING_FSM_INFO *prPairingFsm)
{
	struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;

	DBGLOG(NAN, INFO, PREL6"enter %s\n", __func__);
	pNanXmitFollowupReq =
		kmalloc(sizeof(struct NanTransmitFollowupRequest), GFP_ATOMIC);
	if (!pNanXmitFollowupReq) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return  -ENOMEM;
	}
	/*indicate Pairing success to WifiHal for responder*/
	nanPairingPeerNikReceived(prAdapter, (u8 *)prFollowupEvt, prPairingFsm);
	kalMemZero(pNanXmitFollowupReq,
		sizeof(struct NanTransmitFollowupRequest));
	pNanXmitFollowupReq->publish_subscribe_id = prPairingFsm->ucPublishID;
	pNanXmitFollowupReq->requestor_instance_id =
		prPairingFsm->ucSubscribeID;
	memcpy(pNanXmitFollowupReq->addr,
		prPairingFsm->prStaRec->aucMacAddr, NAN_MAC_ADDR_LEN);
	nanTransmitRequest_host(prAdapter, pNanXmitFollowupReq);
	kfree(pNanXmitFollowupReq);

	DBGLOG(NAN, INFO, PREL6"leave %s\n", __func__);
	return WLAN_STATUS_SUCCESS;
}
void indicatePairingSuccess_init(struct ADAPTER *prAdapter,
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt,
	struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, PREL6"enter %s\n", __func__);
	/* indicate Pairing success to WifiHal for initiator */
	nanPairingPeerNikReceived(prAdapter, (u8 *)prFollowupEvt, prPairingFsm);
	DBGLOG(NAN, INFO, PREL6"leave %s\n", __func__);
}

static uint32_t
sendBootstrappingResponseWithComeback(struct ADAPTER *prAdapter,
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt, uint16_t comeback) {

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;

	DBGLOG(NAN, VOC, "Enter %s\n", __func__);

	if (prFollowupEvt == NULL)
		return WLAN_STATUS_INVALID_DATA;

	pNanXmitFollowupReq = kmalloc(sizeof(struct NanTransmitFollowupRequest),
				GFP_ATOMIC);
	if (!pNanXmitFollowupReq) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(pNanXmitFollowupReq,
		sizeof(struct NanTransmitFollowupRequest));

	/* FW Follow up instance ID */
	pNanXmitFollowupReq->publish_subscribe_id =
		prFollowupEvt->publish_subscribe_id;
	pNanXmitFollowupReq->requestor_instance_id =
		prFollowupEvt->requestor_instance_id;

	/* PEER MAC */
	memcpy(pNanXmitFollowupReq->addr, prFollowupEvt->addr,
		NAN_MAC_ADDR_LEN);

	/* SDA */
	pNanXmitFollowupReq->service_specific_info_len =
		prFollowupEvt->service_specific_info_len;
	memcpy(pNanXmitFollowupReq->service_specific_info,
		prFollowupEvt->service_specific_info,
		prFollowupEvt->service_specific_info_len);

	/* SDEA */
	pNanXmitFollowupReq->sdea_service_specific_info_len =
		prFollowupEvt->sdea_service_specific_info_len;
	memcpy(pNanXmitFollowupReq->sdea_service_specific_info,
		prFollowupEvt->sdea_service_specific_info,
		prFollowupEvt->sdea_service_specific_info_len);

	/* NPBA */
	pNanXmitFollowupReq->bootstrap_type =
		NAN_BOOTSTRAPPING_TYPE_RESPONSE;
	pNanXmitFollowupReq->bootstrap_method =
		0x00;
	pNanXmitFollowupReq->bootstrap_status =
		NAN_BOOTSTRAPPING_STATUS_COMEBACK;
	pNanXmitFollowupReq->comeback_enable = TRUE;
	pNanXmitFollowupReq->comeback_after = comeback;


	rStatus = nanTransmitRequest(prAdapter,	pNanXmitFollowupReq);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(NAN, ERROR,
		"Failed (rStatus=%u)\n", rStatus);
	else
		DBGLOG(NAN, VOC, "Leave %s\n", __func__);

	kfree(pNanXmitFollowupReq);
	return rStatus;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
int
mtk_cfg80211_vendor_event_nan_followup_indication(struct ADAPTER *prAdapter,
						  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NanFollowupIndMsg *prNanFollowupInd = NULL;
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;

#if CFG_SUPPORT_NAN_R4_PAIRING
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct NanPairingBootStrapMsg bootstrap_msg = {0};
	u_int8_t fgIsNanPairingEn = prAdapter->rWifiVar.ucNanEnablePairing;
	u_int16_t comeback_time = prAdapter->rWifiVar.u2NanPairingComeback;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	prFollowupEvt = (struct NAN_FOLLOW_UP_EVENT *)pcuEvtBuf;

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (fgIsNanPairingEn &&
	prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST) {
		g_last_bootstrapReq_npba_dialogTok =
			prFollowupEvt->bootstrap_dialog;
	}

	if (fgIsNanPairingEn &&
	prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST
	&& (prFollowupEvt->bootstrap_status
	!= NAN_BOOTSTRAPPING_STATUS_COMEBACK)) {
		if (comeback_time > 0) {
			sendBootstrappingResponseWithComeback(prAdapter,
				prFollowupEvt, comeback_time);
		}
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	if (prFollowupEvt->service_specific_info_len <
		NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN &&
		prFollowupEvt->sdea_service_specific_info_len <
		NAN_FW_MAX_SDEA_SERVICE_SPECIFIC_INFO_LEN) {
		message_len =
		sizeof(struct _NanMsgHeader) +
		sizeof(struct _NanFollowupIndParams) +
		(SIZEOF_TLV_HDR + MAC_ADDR_LEN) +
		(SIZEOF_TLV_HDR + prFollowupEvt->service_specific_info_len) +
		(SIZEOF_TLV_HDR +
		prFollowupEvt->sdea_service_specific_info_len);
#if CFG_SUPPORT_NAN_R4_PAIRING
		if (fgIsNanPairingEn &&
			(prFollowupEvt->bootstrap_type ==
			NAN_BOOTSTRAPPING_TYPE_REQUEST
			|| prFollowupEvt->bootstrap_type ==
			NAN_BOOTSTRAPPING_TYPE_RESPONSE)) {
			message_len += (SIZEOF_TLV_HDR +
				sizeof(struct NanPairingBootStrapMsg));
		}
#endif
	}

	if (message_len > 0)
		prNanFollowupInd = kmalloc(message_len, GFP_KERNEL);

	if (!prNanFollowupInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanFollowupInd, message_len);
	if (!prNanFollowupInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	prNanFollowupInd->fwHeader.msgVersion = 1;
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (fgIsNanPairingEn && prFollowupEvt->bootstrap_type ==
		NAN_BOOTSTRAPPING_TYPE_REQUEST)
		prNanFollowupInd->fwHeader.msgId =
			NAN_MSG_ID_BOOTSTRAPPING_REQ_IND;
	else if (fgIsNanPairingEn && prFollowupEvt->bootstrap_type ==
		NAN_BOOTSTRAPPING_TYPE_RESPONSE)
		prNanFollowupInd->fwHeader.msgId =
			NAN_MSG_ID_BOOTSTRAPPING_RSP_IND;
	else
		prNanFollowupInd->fwHeader.msgId = NAN_MSG_ID_FOLLOWUP_IND;

	if (prNanFollowupInd->fwHeader.msgId ==
		NAN_MSG_ID_BOOTSTRAPPING_REQ_IND)
		DBGLOG(NAN, INFO, PAIRING_DBGM90, __func__);
#else
	prNanFollowupInd->fwHeader.msgId = NAN_MSG_ID_FOLLOWUP_IND;
#endif
	prNanFollowupInd->fwHeader.msgLen = message_len;
	prNanFollowupInd->fwHeader.handle = prFollowupEvt->publish_subscribe_id;

	/* Indication doesn't have transition ID */
	prNanFollowupInd->fwHeader.transactionId = 0;

	/* Mapping datas */
	prNanFollowupInd->followupIndParams.matchHandle =
		prFollowupEvt->requestor_instance_id;
	prNanFollowupInd->followupIndParams.window = prFollowupEvt->dw_or_faw;

	DBGLOG(NAN, INFO2,
	       "[%s] matchHandle: %d, window:%d, ServiceLen(%d,%d)\n",
	       __func__,
	       prNanFollowupInd->followupIndParams.matchHandle,
	       prNanFollowupInd->followupIndParams.window,
	       prFollowupEvt->service_specific_info_len,
	       prFollowupEvt->sdea_service_specific_info_len);

#if CFG_SUPPORT_NAN_R4_PAIRING
	DBGLOG(NAN, INFO,
	"[%s] bootstrap_type=%u, bootstrap_status=%u, u2ComebackAfter=%u, key_len=%u\n",
	__func__,
	prFollowupEvt->bootstrap_type,
	prFollowupEvt->bootstrap_status,
	prFollowupEvt->u2ComebackAfter,
	prFollowupEvt->key_length);

/* for debug */
#if 0
	prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);
	if (prPairingFsm != NULL) {
		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
			DBGLOG(NAN, ERROR,
			"Follow up coming after paired!!\n");
		} else {
			DBGLOG(NAN, ERROR,
			"Follow up coming before paired!! ePairingState=%u)\n",
			prPairingFsm->ePairingState);
		}
	}
#endif

	if (prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_RESPONSE
	&& prFollowupEvt->bootstrap_status ==
		NAN_BOOTSTRAPPING_STATUS_ACCEPTED
	&& prFollowupEvt->u2ComebackAfter == 0) {

		/* Rx Bootstrap response and status accepted */
		prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);

		if (prPairingFsm) {
			DBGLOG(NAN, INFO,
			"[%s], state=%u, go to BOOTSTRAP_DONE\n",
			__func__, prPairingFsm->ePairingState);
		} else {
			DBGLOG(NAN, INFO, "[%s], FSM NULL(addr:"MACSTR_A"\n",
			__func__,
			prFollowupEvt->addr[0],
			prFollowupEvt->addr[1],
			prFollowupEvt->addr[2],
			prFollowupEvt->addr[3],
			prFollowupEvt->addr[4],
			prFollowupEvt->addr[5]);
		}
		if (prPairingFsm) {
			if (prPairingFsm->ePairingState ==
				NAN_PAIRING_BOOTSTRAPPING) {
				pairingFsmSteps(prAdapter,
				prPairingFsm, NAN_PAIRING_BOOTSTRAPPING_DONE);
			}
		}
	} else if (prFollowupEvt->key_length > 0) {

		/* Get PairingFSM for SKDA */
		prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);
		if (prPairingFsm) {
			DBGLOG(NAN, INFO,
			"[%s], state=%u, go to DECRYPT_SKDA\n",
			__func__, prPairingFsm->ePairingState);
		}
		DBGLOG(NAN, INFO,
		"[%s], FSM %p, SKDA, addr="MACSTR_A"\n",
		__func__, prPairingFsm,
		prFollowupEvt->addr[0],
		prFollowupEvt->addr[1],
		prFollowupEvt->addr[2],
		prFollowupEvt->addr[3],
		prFollowupEvt->addr[4],
		prFollowupEvt->addr[5]);
		/* Print PairingFsm for debug */
		pairingDbgInfo(prAdapter);

		if (prPairingFsm &&
			prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			prFollowupEvt->key_length > 0) {
			/* Copy key data for decryption */
			DBGLOG(NAN, INFO,
			"[pairing-dbg]%s go decrypt SKDA\n", __func__);
			prPairingFsm->u2Skda2KeyLength =
				prFollowupEvt->key_length;
			kalMemCpyS(prPairingFsm->aucSkdaKeyData,
				NAN_KDE_ATTR_BUF_SIZE,
				prFollowupEvt->key_data,
				prFollowupEvt->key_length);
			pairingDecryptSKDA(prAdapter,
				prPairingFsm, prFollowupEvt->key_data,
				prFollowupEvt->key_length);
/* pairing debug */
#if 1
			DBGLOG(NAN, INFO,
			PREL6"[%s] peer NIK dump s>>>\n",
			__func__);
			nanUtilDump(prAdapter,
			PREL6" peer NIK:",
			prPairingFsm->aucPeerNik, NAN_NIK_LEN);
			DBGLOG(NAN, INFO,
			PREL6"[%s] peer NIK dump <<<e\n",
			__func__);
#endif
			/* NIK sending at responder */
			if (prPairingFsm->ucPairingType ==
			NAN_PAIRING_RESPONDER &&
			prPairingFsm->fgCachingEnable == TRUE &&
			prPairingFsm->fgPeerNIK_received == FALSE) {
				int ret = WLAN_STATUS_SUCCESS;

				ret = indicatePairingSuccess_resp(
					prAdapter, prFollowupEvt,
					prPairingFsm);
				if (ret != WLAN_STATUS_SUCCESS) {
					kfree(prNanFollowupInd);
					return ret;
				}
			} else if (prPairingFsm->ucPairingType ==
			 NAN_PAIRING_REQUESTOR &&
			prPairingFsm->fgCachingEnable == TRUE &&
			prPairingFsm->fgPeerNIK_received == FALSE){
				indicatePairingSuccess_init(
				prAdapter, prFollowupEvt, prPairingFsm);
			}

		}
		nanUtilDump(prAdapter, "NAN SKDA",
			prFollowupEvt->key_data, NAN_KDE_ATTR_BUF_SIZE);
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	tlvs = prNanFollowupInd->ptlv;
	/* Add TLV datas */

#if (!defined(CFG_SUPPORT_NAN_R4_PAIRING) || (CFG_SUPPORT_NAN_R4_PAIRING == 0))
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 prFollowupEvt->addr, tlvs);
#endif

	if (prFollowupEvt->service_specific_info_len > 0)
		tlvs = nanAddTlv(NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
			 prFollowupEvt->service_specific_info_len,
			 prFollowupEvt->service_specific_info, tlvs);
	if (prFollowupEvt->sdea_service_specific_info_len > 0)
		tlvs = nanAddTlv(NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO,
			 prFollowupEvt->sdea_service_specific_info_len,
			 prFollowupEvt->sdea_service_specific_info, tlvs);

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (fgIsNanPairingEn &&
	(prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST
	|| prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_RESPONSE)) {
		bootstrap_msg.bootstrap_type = prFollowupEvt->bootstrap_type;
		bootstrap_msg.bootstrap_status =
			prFollowupEvt->bootstrap_status;
		bootstrap_msg.bootstrap_method =
			prFollowupEvt->bootstrap_method;
		bootstrap_msg.u2ComebackAfter = prFollowupEvt->u2ComebackAfter;
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING,
				sizeof(struct NanPairingBootStrapMsg),
				(u8 *)&bootstrap_msg, tlvs);
	}
	/* need to add NAN_TLV_TYPE_MAC_ADDRESS after
	 * NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING
	 */
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 prFollowupEvt->addr, tlvs);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	DBGLOG(NAN, INFO,
		"pub/subid: %d, addr: %02x:%02x:%02x:%02x:%02x:%02x, specific_info[0]: %02x\n",
		prNanFollowupInd->fwHeader.handle,
		((uint8_t *)prFollowupEvt->addr)[0],
		((uint8_t *)prFollowupEvt->addr)[1],
		((uint8_t *)prFollowupEvt->addr)[2],
		((uint8_t *)prFollowupEvt->addr)[3],
		((uint8_t *)prFollowupEvt->addr)[4],
		((uint8_t *)prFollowupEvt->addr)[5],
		prFollowupEvt->service_specific_info[0]);

	/* NAN_CHK_PNT log message */
	DBGLOG(NAN, INFO2,
	       "[NAN_CHK_PNT] NAN_RX type=Follow_Up peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		prFollowupEvt->addr[0], prFollowupEvt->addr[1],
		prFollowupEvt->addr[2], prFollowupEvt->addr[3],
		prFollowupEvt->addr[4], prFollowupEvt->addr[5]);

	/* Ranging report
	 * To be implement. NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO
	 */

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanFollowupInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanFollowupInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanFollowupInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanFollowupInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_selfflwup_indication(
	struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NanSelfFollowupIndMsg *prNanSelfFollowupInd;
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt;
	size_t message_len = 0;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo,
				AIS_DEFAULT_INDEX))->ieee80211_ptr;

	prFollowupEvt = (struct NAN_FOLLOW_UP_EVENT *) pcuEvtBuf;

	message_len = sizeof(*prNanSelfFollowupInd);

	prNanSelfFollowupInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanSelfFollowupInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	kalMemZero(prNanSelfFollowupInd, message_len);

	prNanSelfFollowupInd->fwHeader.msgVersion = 1;
	prNanSelfFollowupInd->fwHeader.msgId =
			NAN_MSG_ID_SELF_TRANSMIT_FOLLOWUP_IND;
	prNanSelfFollowupInd->fwHeader.msgLen = message_len;
	prNanSelfFollowupInd->fwHeader.handle =
		prFollowupEvt->publish_subscribe_id;
	/* Indication doesn't have transition ID */
	prNanSelfFollowupInd->fwHeader.transactionId =
		prFollowupEvt->transaction_id;

	if (prFollowupEvt->tx_status == WLAN_STATUS_SUCCESS)
		prNanSelfFollowupInd->reason = NAN_I_STATUS_SUCCESS;
	else
		prNanSelfFollowupInd->reason = NAN_I_STATUS_DE_FAILURE;

	/* NAN_CHK_PNT log message */
	DBGLOG(NAN, VOC,
	       "[NAN_CHK_PNT] NAN_TX type=Follow_Up peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x tx_status=%u\n",
		prFollowupEvt->addr[0], prFollowupEvt->addr[1],
		prFollowupEvt->addr[2], prFollowupEvt->addr[3],
		prFollowupEvt->addr[4], prFollowupEvt->addr[5],
		prFollowupEvt->tx_status);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanSelfFollowupInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
		message_len, prNanSelfFollowupInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanSelfFollowupInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanSelfFollowupInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_match_expire(struct ADAPTER *prAdapter,
					       uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct NAN_MATCH_EXPIRE_EVENT *prMatchExpireEvt;
	struct NanMatchExpiredIndMsg *prNanMatchExpiredInd;
	size_t message_len = 0;

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	prMatchExpireEvt = (struct NAN_MATCH_EXPIRE_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct NanMatchExpiredIndMsg);

	prNanMatchExpiredInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanMatchExpiredInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	kalMemZero(prNanMatchExpiredInd, message_len);

	prNanMatchExpiredInd->fwHeader.msgVersion = 1;
	prNanMatchExpiredInd->fwHeader.msgId = NAN_MSG_ID_MATCH_EXPIRED_IND;
	prNanMatchExpiredInd->fwHeader.msgLen = message_len;
	prNanMatchExpiredInd->fwHeader.handle =
			prMatchExpireEvt->u2PublishSubscribeID;
	prNanMatchExpiredInd->fwHeader.transactionId = 0;

	prNanMatchExpiredInd->matchExpiredIndParams.matchHandle =
		prMatchExpireEvt->u4RequestorInstanceID;

	DBGLOG(NAN, INFO, "[%s] Handle:%d, matchHandle:%d\n", __func__,
		prNanMatchExpiredInd->fwHeader.handle,
		prNanMatchExpiredInd->matchExpiredIndParams.matchHandle);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
		message_len + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN,
		GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanMatchExpiredInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb,
		MTK_WLAN_VENDOR_ATTR_NAN,
		message_len,
		prNanMatchExpiredInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree(prNanMatchExpiredInd);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanMatchExpiredInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_report_beacon(
	struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf)
{
	struct _NAN_EVENT_REPORT_BEACON *prFwEvt;
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame
		= (struct WLAN_BEACON_FRAME *) NULL;

	prFwEvt = (struct _NAN_EVENT_REPORT_BEACON *) pcuEvtBuf;
	prWlanBeaconFrame = (struct WLAN_BEACON_FRAME *)
		prFwEvt->aucBeaconFrame;

	DBGLOG(NAN, INFO,
		"Cl:" MACSTR ",Src:" MACSTR ",rssi:%d,chnl:%d,TsfL:0x%x\n",
		MAC2STR(prWlanBeaconFrame->aucBSSID),
		MAC2STR(prWlanBeaconFrame->aucSrcAddr),
		prFwEvt->i4Rssi,
		prFwEvt->ucHwChnl,
		prFwEvt->au4LocalTsf[0]);

	nanExtComposeBeaconTrack(prAdapter, prFwEvt);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_schedule_config(
	struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf)
{
	g_deEvent++;

	nanUpdateAisBitmap(prAdapter, TRUE);

	return WLAN_STATUS_SUCCESS;
}


#if CFG_SUPPORT_NAN_R4_PAIRING
int
mtk_cfg80211_vendor_event_nan_pairing_indication(struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf, struct SW_RFB *prSwRfb)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct _NanPairingRequestIndMsg *prNanPairingRequestInd = NULL;
	struct _NanPairingRequestIndParams *pairingParam = NULL;
	size_t message_len = 0;
	struct NanPairingPASNMsg pasnframe = {0};
	uint8_t *tlvs = NULL;
	enum NanPairingRequestType nan_pairing_request_type =
		NAN_PAIRING_SETUP_REQ_T;

	DBGLOG(NAN, ERROR, "NAN_MSG_ID_PAIRING_INDICATION[0]!!\n");

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	pairingParam = (struct _NanPairingRequestIndParams *)pcuEvtBuf;

	pasnframe.framesize = prSwRfb->u2PacketLen;
	DBGLOG(NAN, ERROR, "[pairing-pasn] prSwRfb->u2PacketLen = %u\n",
		prSwRfb->u2PacketLen);
	if (sizeof(pasnframe.PASN_FRAME) >= pasnframe.framesize)
		memcpy(pasnframe.PASN_FRAME,
			prSwRfb->pvHeader, pasnframe.framesize);
	else {
		DBGLOG(NAN, ERROR,
		"FATAL !!! failed to cache full PASN-M1(size:%u)\n",
		pasnframe.framesize);
		return -ENOMEM;
	}
	message_len = sizeof(struct _NanPairingRequestIndMsg);
	message_len += (SIZEOF_TLV_HDR + sizeof(struct NanPairingPASNMsg));
	DBGLOG(NAN, ERROR, "message_len=%zu\n", message_len);

	prNanPairingRequestInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanPairingRequestInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanPairingRequestInd, message_len);
	if (!prNanPairingRequestInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	prNanPairingRequestInd->fwHeader.msgVersion = 1;
	prNanPairingRequestInd->fwHeader.msgId = NAN_MSG_ID_PAIRING_INDICATION;
	prNanPairingRequestInd->fwHeader.msgLen = message_len;

	/* Indication doesn't have transition ID */
	prNanPairingRequestInd->fwHeader.transactionId = 0;

	/* copy MAC, pairing_request_type, enable_pairing_cache,
	 * nira.nonce, nira.tag
	 */
	memcpy(prNanPairingRequestInd->pairingRequestIndParams.
		peer_disc_mac_addr, pairingParam->peer_disc_mac_addr,
		MAC_ADDR_LEN);
	nan_pairing_request_type = pairingParam->nan_pairing_request_type;
	prNanPairingRequestInd->pairingRequestIndParams.
		nan_pairing_request_type = nan_pairing_request_type;
	prNanPairingRequestInd->pairingRequestIndParams.
		enable_pairing_cache = pairingParam->enable_pairing_cache;
	if (nan_pairing_request_type == NAN_PAIRING_VERIFICATION_REQ_T) {
		memcpy(prNanPairingRequestInd->pairingRequestIndParams.
			nira.nonce, pairingParam->nira.nonce,
			NAN_IDENTITY_NONCE_LEN);
		memcpy(prNanPairingRequestInd->pairingRequestIndParams.nira.tag,
			pairingParam->nira.tag, NAN_IDENTITY_TAG_LEN);
	}

	tlvs = prNanPairingRequestInd->ptlv;
	tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_RAWFRAME,
			sizeof(struct NanPairingPASNMsg),
			(u8 *)&pasnframe,
			tlvs);

	DBGLOG(NAN, ERROR, "NAN_MSG_ID_PAIRING_INDICATION[1]!!\n");
	hexdump_nan((void *)prNanPairingRequestInd, message_len);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPairingRequestInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPairingRequestInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanPairingRequestInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanPairingRequestInd);

	DBGLOG(NAN, ERROR, "NAN_MSG_ID_PAIRING_INDICATION[2]!!\n");

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_pairing_confirm(struct ADAPTER *prAdapter,
						  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct _NanPairingConfirmIndMsg *prNanPairingConfirmInd = NULL;
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	u32 mask = NAN_CIPHER_SUITE_SHARED_KEY_NONE;

	DBGLOG(NAN, ERROR, "NAN_MSG_ID_PAIRING_CONFIRM[0]!!\n");

	wiphy = wlanGetWiphy();
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		       ->ieee80211_ptr;

	prFollowupEvt = (struct NAN_FOLLOW_UP_EVENT *)pcuEvtBuf;

	message_len =
		sizeof(struct _NanMsgHeader) +
		sizeof(struct _NanPairingConfirmIndParams) +
		(SIZEOF_TLV_HDR + MAC_ADDR_LEN);

	prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "[%s] prPairingFsm error\n", __func__);
		return -EINVAL;
	}

	if (prPairingFsm->ePairingState != NAN_PAIRING_PAIRED)
		return -EINVAL;

	prNanPairingConfirmInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanPairingConfirmInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanPairingConfirmInd, message_len);

	prNanPairingConfirmInd->fwHeader.msgVersion = 1;
	prNanPairingConfirmInd->fwHeader.msgId = NAN_MSG_ID_PAIRING_CONFIRM;
	prNanPairingConfirmInd->fwHeader.msgLen = message_len;
	prNanPairingConfirmInd->fwHeader.handle =
		prFollowupEvt->publish_subscribe_id;


	if (prPairingFsm->fgPeerNIK_received == TRUE) {

		DBGLOG(NAN, ERROR,
		"[pairing-pasn] composing _NanPairingConfirmIndMsg !! %s()\n",
		__func__);
		/* initial value. it will be filled by Hal */
		prNanPairingConfirmInd->pairingConfirmIndParams.
			pairing_instance_id = 0;
		prNanPairingConfirmInd->pairingConfirmIndParams.
			rsp_code = NAN_PAIRING_REQUEST_ACCEPT;
		prNanPairingConfirmInd->pairingConfirmIndParams.
			reason_code = NAN_STATUS_SUCCESS;
		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED)
			prNanPairingConfirmInd->pairingConfirmIndParams.
				nan_pairing_request_type =
					NAN_PAIRING_SETUP_REQ_T;
		else if (prPairingFsm->ePairingState ==
				NAN_PAIRING_PAIRED_VERIFICATION)
			prNanPairingConfirmInd->pairingConfirmIndParams.
				nan_pairing_request_type =
					NAN_PAIRING_VERIFICATION_REQ_T;


		prNanPairingConfirmInd->pairingConfirmIndParams.
			enable_pairing_cache = prPairingFsm->fgCachingEnable;
		memcpy(prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.peer_nan_identity_key,
			prPairingFsm->aucPeerNik, NAN_NIK_LEN);
		memcpy(prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.local_nan_identity_key,
			prPairingFsm->aucNik, NAN_NIK_LEN);
		memcpy(prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.npk.pmk,
			prPairingFsm->npk, prPairingFsm->npk_len);
		prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.npk.pmk_len =
			prPairingFsm->npk_len;
		prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.akm =
			prPairingFsm->akm;


		switch (prPairingFsm->u4SelCipherType) {
		case NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128:
			mask = NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
			break;
		case NAN_CIPHER_SUITE_ID_NCS_PK_PASN_256:
			mask = NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_256_MASK;
			break;
		default:
			mask = NAN_CIPHER_SUITE_SHARED_KEY_NONE;
			break;

		}
		prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.cipher_type = mask;

		/* Add TLV datas */
		tlvs = prNanPairingConfirmInd->ptlv;
		tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
				 prFollowupEvt->addr, tlvs);
	}





	/* Indication doesn't have transition ID */
	prNanPairingConfirmInd->fwHeader.transactionId = 0;

	DBGLOG(NAN, ERROR, "NAN_MSG_ID_PAIRING_CONFIRM[1]!!\n");
	hexdump_nan((void *)prNanPairingConfirmInd, message_len);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPairingConfirmInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPairingConfirmInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanPairingConfirmInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanPairingConfirmInd);

	DBGLOG(NAN, ERROR, "NAN_MSG_ID_PAIRING_CONFIRM[2]!!\n");

	return WLAN_STATUS_SUCCESS;
}


int mtk_cfg80211_vendor_nan_pasn_rsp(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *data,
	int data_len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct nlattr *tb
		[MTK_WLAN_VENDOR_ATTR_MAX + 1] = {};
	struct MSDU_INFO *prMgmtFrame = (struct MSDU_INFO *) NULL;
	struct MSG_MGMT_TX_REQUEST *prMsgTxReq = NULL;
	uint32_t frame_size;

	if (!wiphy || !wdev || !data || !data_len) {
		DBGLOG(NAN, ERROR, "%s():input data null.\n", __func__);
		rStatus = -EINVAL;
		goto exit;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "get glue structure fail.\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(NAN, WARN, "driver is not ready\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->prAdapter->rWifiVar.ucNanEnablePairing == 0) {
		DBGLOG(NAN, WARN, "Pairing is not enabled\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_MAX,
		data,
		data_len,
		nla_pasn_rsp_policy)) {
		DBGLOG(NAN, ERROR, "NLA_PARSE failed\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (!tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_RAW_FRAME]) {
		DBGLOG(NAN, ERROR, "Failed to get raw PASN frame\n");
		rStatus = -EINVAL;
		goto exit;
	}

	prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter,
			RAM_TYPE_MSG, sizeof(struct MSG_MGMT_TX_REQUEST));
	if (prMsgTxReq == NULL) {
		DBGLOG(REQ, ERROR, "allocate msg req. fail.\n");
		rStatus = -ENOMEM;
		goto exit;
	}

	prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_NAN_MGMT_TX;
	prMsgTxReq->ucBssIdx = NAN_DEFAULT_INDEX;
	frame_size = nla_len(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_RAW_FRAME]);
	prMgmtFrame = cnmMgtPktAlloc(prGlueInfo->prAdapter,
				(int32_t) (frame_size + sizeof(uint64_t)
				+ MAC_TX_RESERVED_FIELD));
	if (prMgmtFrame == NULL) {
		rStatus = -ENOMEM;
		goto exit;
	}
	prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
	prMsgTxReq->prMgmtMsduInfo->u2FrameLength = frame_size;
	kalMemCopy((uint8_t *)((unsigned long) prMgmtFrame->prPacket +
		MAC_TX_RESERVED_FIELD),
		nla_data(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_RAW_FRAME]),
		frame_size);

	mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgTxReq,
			MSG_SEND_METHOD_BUF);
	return rStatus;
exit:
	if (prMsgTxReq && prMsgTxReq->prMgmtMsduInfo)
		cnmMgtPktFree(prGlueInfo->prAdapter,
			prMsgTxReq->prMgmtMsduInfo);
	if (prMsgTxReq)
		cnmMemFree(prGlueInfo->prAdapter, prMsgTxReq);

	return rStatus;
}


int mtk_cfg80211_vendor_nan_pasn_setkey(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *data,
	int data_len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct nlattr *tb
		[MTK_WLAN_VENDOR_ATTR_MAX + 1] = {};
	struct nan_pairing_keys_t key_data = {0};
	struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;

	if (!wiphy || !wdev || !data || !data_len) {
		DBGLOG(NAN, ERROR, "%s():input data null.\n", __func__);
		rStatus = -EINVAL;
		goto exit;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "get glue structure fail.\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(NAN, WARN, "driver is not ready\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_MAX,
		data,
		data_len,
		nla_pasn_setkey_policy)) {
		DBGLOG(NAN, ERROR, "NLA_PARSE failed\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (!tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]) {
		DBGLOG(NAN, ERROR, "Failed to get nan_pairing_keys\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (nla_len(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]) !=
			sizeof(struct nan_pairing_keys_t)) {
		DBGLOG(NAN, ERROR,
		"[pairing-key1] NAN Pairing Key Data size mismatch !\n");
		rStatus = -EINVAL;
		goto exit;
	}
	kalMemCopy(&key_data,
		nla_data(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]),
		sizeof(struct nan_pairing_keys_t));

	DBGLOG(NAN, INFO,
		"[pairing-key1] enter mtk_cfg80211_vendor_nan_pasn_setkey()\n");

	if (nanPasnSetKey(prGlueInfo->prAdapter, &key_data) ==
		WLAN_STATUS_SUCCESS) {
		struct PAIRING_FSM_INFO *prPairingFsm = NULL;

		prPairingFsm = pairingFsmSearch(prGlueInfo->prAdapter,
			key_data.peer_mac_addr);
		if (!prPairingFsm) {
			DBGLOG(NAN, ERROR,
			"[%s] prPairingFsm error\n", __func__);
			rStatus = -EINVAL;
			goto exit;
		}

		/* for responder , only key save happens here and
		 * key install happens at reception of PASN-M3 confirm
		 * for initiator , both key save and key install happens here.
		 */
		if (prPairingFsm->ucPairingType == NAN_PAIRING_REQUESTOR) {
			pairingFsmSteps(prGlueInfo->prAdapter, prPairingFsm,
				NAN_PAIRING_PAIRED);
		}

		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			prPairingFsm->ucPairingType == NAN_PAIRING_REQUESTOR &&
			prPairingFsm->fgCachingEnable == TRUE &&
			(prPairingFsm->fgPeerNIK_received == FALSE &&
			prPairingFsm->fgLocalNIK_sent == FALSE)) {
			int remainingtime = 0;

			if (prPairingFsm->fgM3Acked == FALSE) {
				DBGLOG(NAN, INFO,
				PREL6" wait M3 acked by responder(max:3s)->\n");
				/* coverity[double_unlock]
				 * no wait queue unclock other than below.
				 */
				remainingtime =
				wait_event_timeout(prPairingFsm->confirm_ack_wq,
				prPairingFsm->fgM3Acked == TRUE, 3 * HZ);
				if (remainingtime > 0) {
					DBGLOG(NAN, INFO,
					PREL6" wait responder to install TK\n");
					msleep(512);
				} else if (remainingtime == 0) {
					DBGLOG(NAN, ERROR,
					PREL6" wait M3 ack timeout!!\n");
					rStatus =  -ETIMEDOUT;
					goto exit;
				}
			}

			if (nanGetFeatureIsSigma(prGlueInfo->prAdapter) &&
			prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING) {
				sigma_nik_exchange_run = false;
				do {
					DBGLOG(NAN, ERROR,
					PREL6"waiting UCC command\n");
					msleep(1000);
				} while (sigma_nik_exchange_run == false);
				sigma_nik_exchange_run = false;
			}
			DBGLOG(NAN, INFO,
			PREL6" trigger NIK exchange at initiator!!\n");
			pNanXmitFollowupReq =
			kmalloc(sizeof(struct NanTransmitFollowupRequest),
				GFP_ATOMIC);
			if (!pNanXmitFollowupReq) {
				DBGLOG(NAN, ERROR, "Allocate failed\n");
				rStatus =  -ENOMEM;
				goto exit;
			}
			kalMemZero(pNanXmitFollowupReq,
				sizeof(struct NanTransmitFollowupRequest));
			pNanXmitFollowupReq->publish_subscribe_id =
				prPairingFsm->ucSubscribeID;
			pNanXmitFollowupReq->requestor_instance_id =
				prPairingFsm->ucPublishID;
			memcpy(pNanXmitFollowupReq->addr,
				prPairingFsm->prStaRec->aucMacAddr,
				NAN_MAC_ADDR_LEN);
			nanTransmitRequest_host(prGlueInfo->prAdapter,
				pNanXmitFollowupReq);
			kfree(pNanXmitFollowupReq);
		}
	}

	return rStatus;
exit:
	return rStatus;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

#if CFG_SUPPORT_NAN_EXT
int mtk_cfg80211_vendor_nan_ext_indication(struct ADAPTER *prAdapter,
					   u8 *data, uint16_t u2Size)
{
	struct NanExtIndMsg nanExtInd = {0};
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	wiphy = wlanGetWiphy();
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EFAULT;
	}

	wdev = (wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX))
		->ieee80211_ptr;
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EFAULT;
	}

	nanExtInd.fwHeader.msgVersion = 1;
	nanExtInd.fwHeader.msgId = NAN_MSG_ID_EXT_IND;
	nanExtInd.fwHeader.msgLen = u2Size;
	nanExtInd.fwHeader.transactionId = 0;

	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanExtIndMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN_EXT, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	kalMemCopy(nanExtInd.data, data, u2Size);
	DBGLOG(NAN, INFO, "NAN Ext Ind:\n");
	DBGLOG_HEX(NAN, INFO, nanExtInd.data, u2Size)

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanExtIndMsg),
			     &nanExtInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_nan_ext(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct sk_buff *skb = NULL;
	struct ADAPTER *prAdapter;

	struct NanExtCmdMsg extCmd = {0};
	struct NanExtResponseMsg extRsp = {0};
	u32 u4BufLen;
	u32 i4Status = -EINVAL;

	/* sanity check */
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EINVAL;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "prGlueInfo error!\n");
		return -EINVAL;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return -EINVAL;
	}

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev error!\n");
		return -EINVAL;
	}

	if (data == NULL || data_len < sizeof(struct NanExtCmdMsg)) {
		DBGLOG(NAN, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}

	/* read ext cmd */
	kalMemCopy(&extCmd, data, sizeof(struct NanExtCmdMsg));
	extRsp.fwHeader = extCmd.fwHeader;

	/* execute ext cmd */
	i4Status = kalIoctl(prGlueInfo, wlanoidNANExtCmd, &extCmd,
				sizeof(struct NanExtCmdMsg), &u4BufLen);
	if (i4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, ERROR, "kalIoctl NAN Ext Cmd failed\n");
		return -EFAULT;
	}
	kalMemCopy(extRsp.data, extCmd.data, extCmd.fwHeader.msgLen);
	extRsp.fwHeader.msgLen = extCmd.fwHeader.msgLen;

	DBGLOG(NAN, TRACE, "Resp data:", extRsp.data);
	DBGLOG_HEX(NAN, TRACE, extRsp.data, extCmd.fwHeader.msgLen)
	/* reply to framework */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
				sizeof(struct NanExtResponseMsg));

	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb %zu bytes failed\n",
		       sizeof(struct NanExtResponseMsg));
		return -ENOMEM;
	}

	if (unlikely(
		nla_put_nohdr(skb, sizeof(struct NanExtResponseMsg),
			(void *)&extRsp) < 0)) {
		DBGLOG(NAN, ERROR, "Fail send reply\n");
		goto failure;
	}

	i4Status = kalIoctl(prGlueInfo, wlanoidNANExtCmdRsp, (void *)&extRsp,
				sizeof(struct NanExtResponseMsg), &u4BufLen);

	if (i4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, ERROR, "kalIoctl NAN Ext Cmd Rsp failed\n");
		goto failure;
	}

	return cfg80211_vendor_cmd_reply(skb);

failure:
	kfree_skb(skb);
	return -EFAULT;
}
#endif /* CFG_SUPPORT_NAN_EXT */
