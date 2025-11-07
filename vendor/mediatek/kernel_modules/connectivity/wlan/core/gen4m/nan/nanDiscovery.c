/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "precomp.h"
#include "nanDiscovery.h"
#include "wpa_supp/src/crypto/sha256.h"
#include "wpa_supp/src/crypto/sha256_i.h"
#include "wpa_supp/src/utils/common.h"

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1) /*NAN PAIRING*/
#include "wpa_supp/FourWayHandShake.h"
#include "wpa_supp/src/ap/wpa_auth_glue.h"
#include "nan/nan_sec.h"
#include "nan_pairing.h"
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

uint8_t g_ucInstanceID;
struct _NAN_DISC_ENGINE_T g_rNanDiscEngine;

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1) /*NAN PAIRING*/
struct _NAN_INSTANCE_T g_arNanInstance[NAN_SERVICE_INSTANCE_NUM];

static struct _APPEND_DISC_ATTR_ENTRY_T txDiscAttributeTable[] = {
	/*   ATTR-ID       fp for calc-var-len     fp for attr appending */
	{ NAN_ATTR_ID_SERVICE_DESCRIPTOR, nanDiscSdaAttrLength,
	  nanDiscSdaAttrAppend },
	{ NAN_ATTR_ID_SDEA, nanDiscSdeaAttrLength,
	  nanDiscSdeaAttrAppend },
	{ NAN_ATTR_ID_SHARED_KEY_DESCRIPTOR, nanDiscSharedKeyAttrLength,
	  nanDiscSharedKeyAttrAppend },
};
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _CMD_NAN_CANCEL_REQUEST {
	uint16_t publish_or_subscribe;
	uint16_t publish_subscribe_id;
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

int8_t atoi(uint8_t ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 87;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 55;
	else if (ch >= '0' && ch <= '9')
		return ch - 48;

	return 0;
}

void
nanConvertMatchFilter(uint8_t *pucFilterDst, uint8_t *pucFilterSrc,
		      uint8_t ucFilterSrcLen, uint16_t *pucFilterDstLen) {
	uint32_t u4Idx;
	uint8_t ucLen;
	uint16_t ucFilterLen;

	ucFilterLen = 0;
	for (u4Idx = 0; u4Idx < ucFilterSrcLen;) {
		/* skip comma */
		if (*pucFilterSrc == ',') {
			u4Idx++;
			pucFilterSrc++;
		}
		DBGLOG(INIT, INFO, "nan: filter[%d] = %p\n", u4Idx,
		       pucFilterSrc);
		ucLen = atoi(*pucFilterSrc);
		*pucFilterDst = ucLen;
		pucFilterDst++;
		u4Idx++;
		pucFilterSrc++;
		ucFilterLen++;
		/* skip comma */
		if (*pucFilterSrc == ',') {
			DBGLOG(INIT, INFO, "nan: skip comma%d\n", u4Idx);
			u4Idx++;
			pucFilterSrc++;
		}
		kalMemCopy(pucFilterDst, pucFilterSrc, ucLen);
		pucFilterDst += ucLen;
		pucFilterSrc += ucLen;
		u4Idx += ucLen;
		ucFilterLen += ucLen;
	}

	*pucFilterDstLen = ucFilterLen;
}

void
nanConvertUccMatchFilter(uint8_t *pucFilterDst, uint8_t *pucFilterSrc,
			 uint8_t ucFilterSrcLen, uint16_t *pucFilterDstLen) {
	char *const delim = ",";
	uint8_t *pucfilter;
	uint32_t u4FilterLen = 0;
	uint32_t u4TotalLen = 0;

	if (ucFilterSrcLen == 0 ||
		(ucFilterSrcLen > NAN_MAX_MATCH_FILTER_LEN)) {
		*pucFilterDstLen = 0;
		return;
	}

	DBGLOG(INIT, INFO, "ucFilterSrcLen %d\n", ucFilterSrcLen);
	if ((ucFilterSrcLen == 1) && (*pucFilterSrc == *delim)) {
		*pucFilterDstLen = 1;
		*pucFilterDst = 0;
		return;
	}

	/* skip last */
	if ((*(pucFilterSrc + ucFilterSrcLen - 1) == *delim) &&
	    (ucFilterSrcLen >= 2)) {
		DBGLOG(INIT, INFO, " erase last ','\n");
		*(pucFilterSrc + ucFilterSrcLen - 1) = 0;
	}
	while ((pucfilter = kalStrSep((char **)&pucFilterSrc, delim)) != NULL) {
		if (*pucfilter == '*') {
			DBGLOG(INIT, INFO, "met *, wildcard filter\n");
			*pucFilterDst = 0;
			u4TotalLen += 1;
			pucFilterDst += 1;
		} else {
			DBGLOG(INIT, INFO, "%s\n", pucfilter);
			u4FilterLen = kalStrLen(pucfilter);
			u4TotalLen += (u4FilterLen + 1);
			*(pucFilterDst) = u4FilterLen;
			kalMemCopy((pucFilterDst + 1), pucfilter, u4FilterLen);
			DBGLOG(INIT, INFO, "u4FilterLen: %d\n", u4FilterLen);
			if (au2DebugModule[DBG_NAN_IDX] & DBG_CLASS_INFO)
				dumpMemory8((uint8_t *)pucFilterDst,
					u4FilterLen + 1);
			pucFilterDst += (u4FilterLen + 1);
		}
	}
	*pucFilterDstLen = u4TotalLen;
}

uint32_t
nanCancelPublishRequest(
	struct ADAPTER *prAdapter,
	struct NanPublishCancelRequest *msg)
{
	uint8_t i;
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _CMD_NAN_CANCEL_REQUEST *prNanUniCmdCancelReq = NULL;
	struct _NAN_PUBLISH_SPECIFIC_INFO_T *prPubSpecificInfo = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _CMD_NAN_CANCEL_REQUEST);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_CANCEL_PUBLISH,
					 sizeof(struct _CMD_NAN_CANCEL_REQUEST),
					 u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prNanUniCmdCancelReq =
		(struct _CMD_NAN_CANCEL_REQUEST *)prTlvElement->aucbody;
	kalMemZero(prNanUniCmdCancelReq,
		sizeof(struct _CMD_NAN_CANCEL_REQUEST));
	prNanUniCmdCancelReq->publish_or_subscribe = 1;
	prNanUniCmdCancelReq->publish_subscribe_id = msg->publish_id;

	/* If FW will send terminate,
	 * decrease ucNanPubNum at terminate event handler
	 */
	for (i = 0; i < NAN_MAX_PUBLISH_NUM; i++) {
		prPubSpecificInfo =
			&prAdapter->rPublishInfo.rPubSpecificInfo[i];
		if (prPubSpecificInfo->ucPublishId == msg->publish_id) {
			prPubSpecificInfo->ucUsed = FALSE;
			if (!prPubSpecificInfo->ucReportTerminate)
				prAdapter->rPublishInfo.ucNanPubNum--;
		}
	}

	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);
	cnmMemFree(prAdapter, prCmdBuffer);

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	pairingFsmCancelRequest(prAdapter, msg->publish_id, TRUE);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanUpdatePublishRequest(struct ADAPTER *prAdapter,
			struct NanPublishRequest *msg) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanFWPublishRequest *prPublishReq = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanFWPublishRequest);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_UPDATE_PUBLISH,
					 sizeof(struct NanFWPublishRequest),
					 u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prPublishReq = (struct NanFWPublishRequest *)prTlvElement->aucbody;
	kalMemZero(prPublishReq, sizeof(struct NanFWPublishRequest));
	prPublishReq->publish_id = msg->publish_id;

	prPublishReq->service_specific_info_len =
		msg->service_specific_info_len;
	if (prPublishReq->service_specific_info_len >
	    NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN)
		prPublishReq->service_specific_info_len =
			NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN;
	kalMemCopy(prPublishReq->service_specific_info,
		   msg->service_specific_info,
		   prPublishReq->service_specific_info_len);

	DBGLOG(INIT, INFO, "nan: sdea_service_specific_info_len = %d\n",
	       prPublishReq->sdea_service_specific_info_len);
	prPublishReq->sdea_service_specific_info_len =
		msg->sdea_service_specific_info_len;
	if (prPublishReq->sdea_service_specific_info_len >
	    NAN_MAX_SDEA_LEN)
		prPublishReq->sdea_service_specific_info_len =
			NAN_MAX_SDEA_LEN;
	kalMemCopy(prPublishReq->sdea_service_specific_info,
		   msg->sdea_service_specific_info,
		   prPublishReq->sdea_service_specific_info_len);

	/* send command to fw */
	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);
	cnmMemFree(prAdapter, prCmdBuffer);
	return WLAN_STATUS_SUCCESS;
}

void
nanSetPublishPmkid(struct ADAPTER *prAdapter, struct NanPublishRequest *msg) {

	int i = 0;
	char *I_mac = "ff:ff:ff:ff:ff:ff";
	u8 pmkid[32];
	u8 IMac[6];
	struct _NAN_SPECIFIC_BSS_INFO_T *prNanSpecificBssInfo;
	struct BSS_INFO *prBssInfo;

	/* Get BSS info */
	prNanSpecificBssInfo = nanGetSpecificBssInfo(
		prAdapter, NAN_BSS_INDEX_BAND0);
	if (prNanSpecificBssInfo == NULL) {
		DBGLOG(NAN, ERROR, "prNanSpecificBssInfo is null\n");
		return;
	}
	prBssInfo = GET_BSS_INFO_BY_INDEX(
		prAdapter, prNanSpecificBssInfo->ucBssIndex);
	if (prBssInfo == NULL) {
		DBGLOG(NAN, ERROR, "prBssInfo is null\n");
		return;
	}

	hwaddr_aton(I_mac, IMac);
	caculate_pmkid(msg->key_info.body.pmk_info.pmk,
		IMac, prBssInfo->aucOwnMacAddr,
		msg->service_name, pmkid);
	DBGLOG(NAN, LOUD, "[publish] SCID=>");
	for (i = 0 ; i < 15; i++)
		DBGLOG(NAN, LOUD, "%X:", pmkid[i]);

	DBGLOG(NAN, LOUD, "%X\n", pmkid[i]);
	msg->scid_len = 16;
	kalMemCopy(msg->scid, pmkid, 16);
}

uint32_t nanAddVendorPayload(
	struct ADAPTER *prAdapter,
	struct NanVendorPayload *payload)
{
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanVendorPayload *pNanVendorPayload = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanVendorPayload);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_VENDOR_PAYLOAD,
					 sizeof(struct NanVendorPayload),
					 u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	pNanVendorPayload = (struct NanVendorPayload *)prTlvElement->aucbody;
	kalMemCopy(pNanVendorPayload, payload, sizeof(struct NanVendorPayload));

	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);

	cnmMemFree(prAdapter, prCmdBuffer);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanPublishRequest(struct ADAPTER *prAdapter, struct NanPublishRequest *msg) {
	uint8_t i, auc_tk[32];
	uint16_t u2PublishId = 0;
	uint32_t u4CmdBufferLen, rStatus, u4Idx;
	void *prCmdBuffer;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanFWPublishRequest *prPublishReq = NULL;
	char aucServiceName[NAN_MAX_SERVICE_NAME_LEN  + 1];
	struct nan_rdf_sha256_state r_SHA_256_state;
	struct _NAN_PUBLISH_SPECIFIC_INFO_T *prPubSpecificInfo = NULL;
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	u_int8_t fgIsNanPairingEn = prAdapter->rWifiVar.ucNanEnablePairing;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING*/

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanFWPublishRequest);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_PUBLISH,
					 sizeof(struct NanFWPublishRequest),
					 u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prPublishReq = (struct NanFWPublishRequest *)prTlvElement->aucbody;
	kalMemZero(prPublishReq, sizeof(struct NanFWPublishRequest));

	if (prAdapter->rPublishInfo.ucNanPubNum < NAN_MAX_PUBLISH_NUM) {
		if (msg->publish_id == 0) {
			prPublishReq->publish_id = ++g_ucInstanceID;
			prAdapter->rPublishInfo.ucNanPubNum++;
		} else {
			prPublishReq->publish_id = msg->publish_id;
			for (i = 0; i < NAN_MAX_PUBLISH_NUM; i++) {
				prPubSpecificInfo =
					&prAdapter->rPublishInfo
					.rPubSpecificInfo[i];
				if (prPubSpecificInfo->ucPublishId ==
					msg->publish_id &&
					!prPubSpecificInfo->ucUsed) {
					DBGLOG(NAN, INFO,
						"PID%d might be timeout, update FAIL!\n",
						prPublishReq->publish_id);
					return 0;
				}
			}
		}
	} else {
		DBGLOG(NAN, INFO, "Exceed max number, allocate fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return 0;
	}
	u2PublishId = prPublishReq->publish_id;
	if (g_ucInstanceID == 255)
		g_ucInstanceID = 0;

	/* Find available Publish Info */
	for (i = 0; i < NAN_MAX_PUBLISH_NUM; i++) {
		prPubSpecificInfo =
			&prAdapter->rPublishInfo.rPubSpecificInfo[i];
		if (!prPubSpecificInfo->ucUsed) {
			prPubSpecificInfo->ucUsed = TRUE;
			prPubSpecificInfo->ucPublishId =
				prPublishReq->publish_id;
			break;
		}
	}

	prPublishReq->tx_type = msg->tx_type;
	prPublishReq->publish_type = msg->publish_type;
	prPublishReq->cipher_type = msg->cipher_type;
	prPublishReq->ttl = msg->ttl;
	prPublishReq->rssi_threshold_flag = msg->rssi_threshold_flag;
	prPublishReq->recv_indication_cfg = msg->recv_indication_cfg;


	/* Bit0 of recv_indication_cfg indicate report terminate event or not */
	if (prPublishReq->recv_indication_cfg & BIT(0))
		prPubSpecificInfo->ucReportTerminate = FALSE;
	else
		prPubSpecificInfo->ucReportTerminate = TRUE;

	kalMemZero(aucServiceName, sizeof(aucServiceName));
	kalMemCopy(aucServiceName,
			msg->service_name,
			NAN_MAX_SERVICE_NAME_LEN);
	for (u4Idx = 0; u4Idx < kalStrLen(aucServiceName); u4Idx++) {
		if ((aucServiceName[u4Idx] >= 'A') &&
		    (aucServiceName[u4Idx] <= 'Z'))
			aucServiceName[u4Idx] = aucServiceName[u4Idx] + 32;
	}
	nan_rdf_sha256_init(&r_SHA_256_state);
	sha256_process(&r_SHA_256_state, aucServiceName,
		       kalStrLen(aucServiceName));
	kalMemZero(auc_tk, sizeof(auc_tk));
	sha256_done(&r_SHA_256_state, auc_tk);
	kalMemCopy(prPublishReq->service_name_hash, auc_tk,
		   NAN_SERVICE_HASH_LENGTH);
	kalMemCopy(g_aucNanServiceId,
			prPublishReq->service_name_hash,
			6);
	nanUtilDump(prAdapter, "service hash", auc_tk, NAN_SERVICE_HASH_LENGTH);

	prPublishReq->service_specific_info_len =
		msg->service_specific_info_len;
	if (prPublishReq->service_specific_info_len >
	    NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN)
		prPublishReq->service_specific_info_len =
			NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN;
	kalMemCopy(prPublishReq->service_specific_info,
		   msg->service_specific_info,
		   prPublishReq->service_specific_info_len);

	prPublishReq->sdea_service_specific_info_len =
		msg->sdea_service_specific_info_len;
	if (prPublishReq->sdea_service_specific_info_len >
	    NAN_MAX_SDEA_LEN)
		prPublishReq->sdea_service_specific_info_len =
			NAN_MAX_SDEA_LEN;
	kalMemCopy(prPublishReq->sdea_service_specific_info,
		   msg->sdea_service_specific_info,
		   prPublishReq->sdea_service_specific_info_len);

	DBGLOG(NAN, INFO, "nan: service_len=(%d, %d)\n",
		   msg->service_specific_info_len,
	       msg->sdea_service_specific_info_len);

	prPublishReq->sdea_params.config_nan_data_path =
		msg->sdea_params.config_nan_data_path;
	prPublishReq->sdea_params.ndp_type = msg->sdea_params.ndp_type;
	prPublishReq->sdea_params.security_cfg = msg->sdea_params.security_cfg;
	prPublishReq->period = msg->period;
	prPublishReq->sdea_params.ranging_state =
		msg->sdea_params.ranging_state;

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	if (fgIsNanPairingEn) {
		prPublishReq->pairing_enable = msg->pairing_enable;
		if (prPublishReq->pairing_enable) {
			prPublishReq->key_caching_enable =
				msg->key_caching_enable;
			prPublishReq->bootstrap_type =
				msg->bootstrap_type;
			prPublishReq->bootstrap_method =
				msg->bootstrap_method;
			prPublishReq->nira_enable = msg->nira_enable;

			if (prPublishReq->nira_enable) {
			random_get_bytes(
				(unsigned char *)&prPublishReq->nonce,
				NAN_NIR_NONCE_LEN);
			nanPairingSavePublisherNonce(prPublishReq->nonce);
			pairingDeriveNirTag(prAdapter, msg->nik, NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				(u8 *)&prPublishReq->nonce, &prPublishReq->tag);
				DBGLOG(NAN, INFO,
				"[pairing-disc]derived NIR(%llu) from NIK(%llu)\n"
				, prPublishReq->tag,
				*(unsigned long long *)msg->nik);
			}
		}
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	if (prAdapter->fgIsNANfromHAL == FALSE) {
		nanConvertUccMatchFilter(prPublishReq->tx_match_filter,
					 msg->tx_match_filter,
					 msg->tx_match_filter_len,
					 &prPublishReq->tx_match_filter_len);
		nanUtilDump(prAdapter, "tx match filter",
			    prPublishReq->tx_match_filter,
			    prPublishReq->tx_match_filter_len);
		nanConvertUccMatchFilter(prPublishReq->rx_match_filter,
					 msg->rx_match_filter,
					 msg->rx_match_filter_len,
					 &prPublishReq->rx_match_filter_len);

		nanUtilDump(prAdapter, "rx match filter",
			    prPublishReq->rx_match_filter,
			    prPublishReq->rx_match_filter_len);
	} else {
		/* fgIsNANfromHAL, then no need to convert match filter */
		prPublishReq->tx_match_filter_len = msg->tx_match_filter_len;
		kalMemCopy(prPublishReq->tx_match_filter, msg->tx_match_filter,
			   prPublishReq->tx_match_filter_len);
		nanUtilDump(prAdapter, "tx match filter",
			    prPublishReq->tx_match_filter,
			    prPublishReq->tx_match_filter_len);

		prPublishReq->rx_match_filter_len = msg->rx_match_filter_len;
		kalMemCopy(prPublishReq->rx_match_filter, msg->rx_match_filter,
			   prPublishReq->rx_match_filter_len);
		nanUtilDump(prAdapter, "rx match filter",
			    prPublishReq->rx_match_filter,
			    prPublishReq->rx_match_filter_len);
	}

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	if (prPublishReq->pairing_enable) {
		nanDiscSetupInstance(prPublishReq->publish_id,
			NAN_SERVICE_TYPE_PUBLISH,
			prPublishReq->service_name_hash);
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);

	cnmMemFree(prAdapter, prCmdBuffer);

	return u2PublishId;
}

uint32_t
nanTransmitRequest(struct ADAPTER *prAdapter,
		   struct NanTransmitFollowupRequest *msg) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanFWTransmitFollowupRequest *prTransmitReq = NULL;
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint8_t ucBssIndex = 0;
	enum ENUM_BAND eBand = 0;
	uint8_t ucIsPub = 0;
	u_int8_t fgIsNanPairingEn = prAdapter->rWifiVar.ucNanEnablePairing;

#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanFWTransmitFollowupRequest);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_TRANSMIT,
				sizeof(struct NanFWTransmitFollowupRequest),
				u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prTransmitReq =
		(struct NanFWTransmitFollowupRequest *)prTlvElement->aucbody;
	kalMemZero(prTransmitReq, sizeof(struct NanFWTransmitFollowupRequest));

	prTransmitReq->publish_subscribe_id = msg->publish_subscribe_id;
	prTransmitReq->requestor_instance_id = msg->requestor_instance_id;
	prTransmitReq->transaction_id = msg->transaction_id;
	kalMemCopy(prTransmitReq->addr, msg->addr, MAC_ADDR_LEN);
	prTransmitReq->service_specific_info_len =
		msg->service_specific_info_len;
	if (prTransmitReq->service_specific_info_len >
	    NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN)
		prTransmitReq->service_specific_info_len =
			NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN;
	kalMemCopy(prTransmitReq->service_specific_info,
		   msg->service_specific_info,
		   prTransmitReq->service_specific_info_len);

	prTransmitReq->sdea_service_specific_info_len =
		msg->sdea_service_specific_info_len;
	if (prTransmitReq->sdea_service_specific_info_len >
	    NAN_FW_MAX_FOLLOW_UP_SDEA_LEN)
		prTransmitReq->sdea_service_specific_info_len =
			NAN_FW_MAX_FOLLOW_UP_SDEA_LEN;
	kalMemCopy(prTransmitReq->sdea_service_specific_info,
		msg->sdea_service_specific_info,
		prTransmitReq->sdea_service_specific_info_len);

	DBGLOG(NAN, VOC,
	       "publish_subscribe_id:%d,requestor_instance_id:%d,len(%d, %d)\n",
	       prTransmitReq->publish_subscribe_id,
	       prTransmitReq->requestor_instance_id,
	       msg->service_specific_info_len,
	       msg->sdea_service_specific_info_len);
	DBGLOG(NAN, VOC,
	       "TransmitReq->addr=>%02x:%02x:%02x:%02x:%02x:%02x\n",
	       prTransmitReq->addr[0], prTransmitReq->addr[1],
	       prTransmitReq->addr[2], prTransmitReq->addr[3],
	       prTransmitReq->addr[4], prTransmitReq->addr[5]);
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	if (fgIsNanPairingEn &&
		(msg->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST ||
		msg->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_RESPONSE)
		) {
		DBGLOG(NAN, ERROR,
		       PREL1"debug (msg->boot_type(%d) )\n",
		       msg->bootstrap_type);
		prPairingFsm = pairingFsmSearch(prAdapter, msg->addr);
		if (prPairingFsm == NULL) {
			/* PairingFsm alloc when start followup as Subscriber */
			DBGLOG(NAN, INFO,
			       "send follow-up as Subscriber\n");
			DBGLOG(NAN, ERROR,
			       "Pairing FSM NULL and alloc new one\n");

			prPairingFsm = pairingFsmAlloc(prAdapter);
			if (prPairingFsm != NULL) {
				prPairingFsm->FsmMode =
					NAN_PAIRING_FSM_MODE_PAIRING;
				cnmTimerInitTimer(prAdapter,
				&prPairingFsm->arKeyExchangeTimer,
				(PFN_MGMT_TIMEOUT_FUNC)nanSDFRetryTimeout,
				(uintptr_t)prPairingFsm);
				ucIsPub =
				nanDiscIsInstancePub(msg->publish_subscribe_id);
				DBGLOG(NAN, INFO, "IsPub=%u\n", ucIsPub);
				if (ucIsPub) {
					pairingFsmSetup(prAdapter,
					prPairingFsm, msg->bootstrap_method,
					TRUE, msg->publish_subscribe_id,
					msg->requestor_instance_id, TRUE, 0);
				} else {
					pairingFsmSetup(prAdapter,
					prPairingFsm, msg->bootstrap_method,
					TRUE, msg->requestor_instance_id,
					msg->publish_subscribe_id, FALSE, 0);
				}
			} else {
				DBGLOG(NAN, ERROR,
				"Pairing FSM NULL from service name\n");
				return WLAN_STATUS_FAILURE;
			}
		} else {
			/* PairingFsm already create when Publish */
			if (prPairingFsm->ucIsPub) {
				DBGLOG(NAN, INFO, "set SubId=%u\n",
				       msg->requestor_instance_id);
				prPairingFsm->ucSubscribeID =
					msg->requestor_instance_id;
			}
		}
		eBand = nanSchedGetSchRecBandByMac(prAdapter, msg->addr);
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, eBand);
		if (prPairingFsm->prStaRec == NULL) {
			DBGLOG(NAN, INFO,
			"go alloc pairing starec\n");
			prPairingFsm->prStaRec =
				cnmStaRecAlloc(prAdapter, STA_TYPE_NAN,
						ucBssIndex, msg->addr);
			if (prPairingFsm->prStaRec)
				atomic_set(
				&prPairingFsm->prStaRec->NanRefCount,
				1);
		}

		DBGLOG(NAN, INFO, "Band=%u, BssIdx=%u\n", eBand, ucBssIndex);
		DBGLOG(NAN, INFO,
		PREL1"type=%u,m=%u,st=%u,comeback_en=%u,comeback=%u(TU)\n",
		msg->bootstrap_type, msg->bootstrap_method,
		msg->bootstrap_status, msg->comeback_enable,
		msg->comeback_after);

		if (fgIsNanPairingEn &&
		(msg->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST ||
		 msg->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_RESPONSE)) {
			DBGLOG(NAN, INFO, PREL1"Bootstrap:%u, state=%u\n",
					msg->bootstrap_type,
					prPairingFsm->ePairingState);
			if (prPairingFsm->ePairingState == NAN_PAIRING_INIT) {
				pairingFsmSetBootStrappingMethod(prPairingFsm,
						msg->bootstrap_method);
				pairingFsmSteps(prAdapter, prPairingFsm,
						NAN_PAIRING_BOOTSTRAPPING);
				if (msg->bootstrap_type ==
					NAN_BOOTSTRAPPING_TYPE_REQUEST) {
					prPairingFsm->ucDialogToken =
					g_last_matched_report_npba_dialogTok+1;
				} else if (msg->bootstrap_type ==
					NAN_BOOTSTRAPPING_TYPE_RESPONSE) {
					prPairingFsm->ucDialogToken =
					g_last_bootstrapReq_npba_dialogTok;
				}
			}

			prTransmitReq->bootstrap_enable = TRUE;
			prTransmitReq->bootstrap_type = msg->bootstrap_type;
			prTransmitReq->bootstrap_method = msg->bootstrap_method;
			prTransmitReq->bootstrap_status = msg->bootstrap_status;
			prTransmitReq->bootstrap_dialogToken =
				prPairingFsm->ucDialogToken;

			if (msg->comeback_enable) {
				if (msg->bootstrap_type ==
					NAN_BOOTSTRAPPING_TYPE_RESPONSE) {
					prTransmitReq->comeback_after =
						msg->comeback_after;
				} else if (msg->bootstrap_type ==
					NAN_BOOTSTRAPPING_TYPE_REQUEST){
					prTransmitReq->comeback_after = 0;
				}
			}

			if ((prPairingFsm->ePairingState ==
						NAN_PAIRING_BOOTSTRAPPING) &&
					(prTransmitReq->bootstrap_type ==
					 NAN_BOOTSTRAPPING_TYPE_RESPONSE) &&
					(prTransmitReq->bootstrap_status ==
					 NAN_BOOTSTRAPPING_STATUS_ACCEPTED ||
					 prTransmitReq->bootstrap_status ==
					 NAN_BOOTSTRAPPING_STATUS_REJECTED)) {
				/* Tx Bootstrap response and status accepted */
				DBGLOG(NAN, ERROR, "go bootstrap done\n");
				pairingFsmSteps(prAdapter, prPairingFsm,
						NAN_PAIRING_BOOTSTRAPPING_DONE);
			}
		}

		DBGLOG(NAN, ERROR, PAIRING_DBGM60,
		       __func__, prTransmitReq->bootstrap_type);
		DBGLOG(NAN, ERROR, PAIRING_DBGM61,
		       __func__, prTransmitReq->publish_subscribe_id,
		       prTransmitReq->requestor_instance_id);
		DBGLOG(NAN, ERROR, PAIRING_DBGM62,
		       __func__, prTransmitReq->addr[0], prTransmitReq->addr[1],
		       prTransmitReq->addr[2], prTransmitReq->addr[3],
		       prTransmitReq->addr[4], prTransmitReq->addr[5]);
		/* send command to fw */
		wlanSendSetQueryCmd(
			prAdapter,		/* prAdapter */
			CMD_ID_NAN_EXT_CMD,	/* ucCID */
			TRUE,		/* fgSetQuery */
			FALSE,		/* fgNeedResp */
			FALSE,		/* fgIsOid */
			NULL,		/* pfCmdDoneHandler */
			NULL,		/* pfCmdTimeoutHandler */
			u4CmdBufferLen,	/* u4SetQueryInfoLen */
			prCmdBuffer,	/* pucInfoBuffer */
			NULL,		/* pvSetQueryBuffer */
			0 /* u4SetQueryBufferLen */);
	} else
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	{
	/* send command to fw */
	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);
	}
	cnmMemFree(prAdapter, prCmdBuffer);
	return WLAN_STATUS_SUCCESS;
}


#if CFG_SUPPORT_NAN_R4_PAIRING
uint32_t
nanTransmitRequest_host(struct ADAPTER *prAdapter,
		   struct NanTransmitFollowupRequest *msg) {
	struct NanFWTransmitFollowupRequest *prTransmitReq = NULL;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");

	prPairingFsm = pairingFsmSearch(prAdapter, msg->addr);

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, PREL2"prPairingFsm is NULL !");
		return WLAN_STATUS_FAILURE;
	}

	prTransmitReq = (struct NanFWTransmitFollowupRequest *)
			cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
			sizeof(struct NanFWTransmitFollowupRequest));

	if (!prTransmitReq) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}
	kalMemZero(prTransmitReq, sizeof(struct NanFWTransmitFollowupRequest));

	prTransmitReq->publish_subscribe_id = msg->publish_subscribe_id;
	prTransmitReq->requestor_instance_id = msg->requestor_instance_id;
	prTransmitReq->transaction_id = msg->transaction_id;
	kalMemCopy(prTransmitReq->addr, msg->addr, MAC_ADDR_LEN);
	prTransmitReq->service_specific_info_len =
		msg->service_specific_info_len;
	if (prTransmitReq->service_specific_info_len >
	    NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN)
		prTransmitReq->service_specific_info_len =
			NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN;
	kalMemCopy(prTransmitReq->service_specific_info,
		   msg->service_specific_info,
		   prTransmitReq->service_specific_info_len);

	prTransmitReq->sdea_service_specific_info_len =
		msg->sdea_service_specific_info_len;
	if (prTransmitReq->sdea_service_specific_info_len >
	    NAN_FW_MAX_FOLLOW_UP_SDEA_LEN)
		prTransmitReq->sdea_service_specific_info_len =
			NAN_FW_MAX_FOLLOW_UP_SDEA_LEN;
	kalMemCopy(prTransmitReq->sdea_service_specific_info,
		msg->sdea_service_specific_info,
		prTransmitReq->sdea_service_specific_info_len);

	DBGLOG(NAN, VOC,
	       "publish_subscribe_id:%d,requestor_instance_id:%d,len(%d, %d)\n",
	       prTransmitReq->publish_subscribe_id,
	prTransmitReq->requestor_instance_id,
	msg->service_specific_info_len,
	msg->sdea_service_specific_info_len);
	DBGLOG(NAN, VOC,
	       "TransmitReq->addr=>%02x:%02x:%02x:%02x:%02x:%02x\n",
	       prTransmitReq->addr[0], prTransmitReq->addr[1],
	       prTransmitReq->addr[2], prTransmitReq->addr[3],
	       prTransmitReq->addr[4], prTransmitReq->addr[5]);

	if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			((prPairingFsm->fgPeerNIK_received == FALSE) ||
			 (prPairingFsm->fgLocalNIK_sent == FALSE))) {
		/* Paired and go NK exchange */
		DBGLOG(NAN, ERROR,
		       PREL6"send host SDF Follow-up (role:%d)\n",
		       prPairingFsm->ucPairingType);

		if (prPairingFsm->ucPairingType ==
				NAN_PAIRING_REQUESTOR) {
			prTransmitReq->publish_subscribe_id =
				prPairingFsm->ucSubscribeID;
			prTransmitReq->requestor_instance_id =
				prPairingFsm->ucPublishID;
		} else if (prPairingFsm->ucPairingType ==
				NAN_PAIRING_RESPONDER) {
			prTransmitReq->publish_subscribe_id =
				prPairingFsm->ucPublishID;
			prTransmitReq->requestor_instance_id =
				prPairingFsm->ucSubscribeID;
		}
		nanDiscSendFollowup(prAdapter, prTransmitReq);
		cnmTimerStopTimer(prAdapter, &prPairingFsm->arKeyExchangeTimer);
		cnmTimerStartTimer(prAdapter,
			&prPairingFsm->arKeyExchangeTimer,
			SDF_TX_RETRY_COUNT_LIMIT_TIMEOUT
			);
	}

	cnmMemFree(prAdapter, prTransmitReq);
	return WLAN_STATUS_SUCCESS;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

uint32_t
nanCancelSubscribeRequest(struct ADAPTER *prAdapter,
			  struct NanSubscribeCancelRequest *msg) {
	uint8_t i;
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _CMD_NAN_CANCEL_REQUEST *prNanUniCmdCancelReq = NULL;
	struct _NAN_SUBSCRIBE_SPECIFIC_INFO_T *prSubSpecificInfo = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _CMD_NAN_CANCEL_REQUEST);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_CANCEL_SUBSCRIBE,
					 sizeof(struct _CMD_NAN_CANCEL_REQUEST),
					 u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}
	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prNanUniCmdCancelReq =
		(struct _CMD_NAN_CANCEL_REQUEST *)prTlvElement->aucbody;
	kalMemZero(prNanUniCmdCancelReq,
		   sizeof(struct _CMD_NAN_CANCEL_REQUEST));
	prNanUniCmdCancelReq->publish_or_subscribe = 0;
	prNanUniCmdCancelReq->publish_subscribe_id =
		msg->subscribe_id;

	/* Decrease ucNanPubNum if FW will not send terminate,
	 * or ucNanPubNum will decrese in terminate event handler.
	 */
	for (i = 0; i < NAN_MAX_SUBSCRIBE_NUM; i++) {
		prSubSpecificInfo =
			&prAdapter->rSubscribeInfo.rSubSpecificInfo[i];
		if (prSubSpecificInfo->ucSubscribeId == msg->subscribe_id) {
			prSubSpecificInfo->ucUsed = FALSE;
			if (!prSubSpecificInfo->ucReportTerminate)
				prAdapter->rSubscribeInfo.ucNanSubNum--;
		}
	}

	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);
	cnmMemFree(prAdapter, prCmdBuffer);
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	pairingFsmCancelRequest(prAdapter, msg->subscribe_id, FALSE);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanSubscribeRequest(struct ADAPTER *prAdapter,
		    struct NanSubscribeRequest *msg) {
	uint8_t i, auc_tk[32];
	uint16_t u2SubscribeId = 0;
	uint32_t u4CmdBufferLen, rStatus, u4Idx;
	void *prCmdBuffer;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanFWSubscribeRequest *prSubscribeReq = NULL;
	struct _NAN_SUBSCRIBE_SPECIFIC_INFO_T *prSubSpecificInfo = NULL;
	char aucServiceName[NAN_MAX_SERVICE_NAME_LEN  + 1];
	struct nan_rdf_sha256_state r_SHA_256_state;

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	u_int8_t fgIsNanPairingEn = prAdapter->rWifiVar.ucNanEnablePairing;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING*/

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanFWSubscribeRequest);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_SUBSCRIBE,
					 sizeof(struct NanFWSubscribeRequest),
					 u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}
	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return WLAN_STATUS_FAILURE;
	}

	prSubscribeReq = (struct NanFWSubscribeRequest *) prTlvElement->aucbody;

	if (prAdapter->rSubscribeInfo.ucNanSubNum < NAN_MAX_SUBSCRIBE_NUM) {
		if (msg->subscribe_id == 0) {
			prSubscribeReq->subscribe_id = ++g_ucInstanceID;
			prAdapter->rSubscribeInfo.ucNanSubNum++;
		} else {
			prSubscribeReq->subscribe_id = msg->subscribe_id;
			for (i = 0; i < NAN_MAX_SUBSCRIBE_NUM; i++) {
				prSubSpecificInfo =
					&prAdapter->rSubscribeInfo
					.rSubSpecificInfo[i];
				if (prSubSpecificInfo->ucSubscribeId ==
					msg->subscribe_id &&
					!prSubSpecificInfo->ucUsed) {
					DBGLOG(NAN, INFO,
						"SID%d might be timeout, update FAIL!\n",
						prSubscribeReq->subscribe_id);
					return 0;
				}
			}
		}
	} else {
		cnmMemFree(prAdapter, prCmdBuffer);
		return 0;
	}
	u2SubscribeId = prSubscribeReq->subscribe_id;
	if (g_ucInstanceID == 255)
		g_ucInstanceID = 0;

	/* Find available Subscribe Info */
	for (i = 0; i < NAN_MAX_PUBLISH_NUM; i++) {
		prSubSpecificInfo =
			&prAdapter->rSubscribeInfo.rSubSpecificInfo[i];
		if (!prSubSpecificInfo->ucUsed) {
			prSubSpecificInfo->ucUsed = TRUE;
			prSubSpecificInfo->ucSubscribeId =
				prSubscribeReq->subscribe_id;
			break;
		}
	}

	prSubscribeReq->subscribe_type = msg->subscribe_type;
	prSubscribeReq->ttl = msg->ttl;
	prSubscribeReq->rssi_threshold_flag = msg->rssi_threshold_flag;
	prSubscribeReq->recv_indication_cfg = msg->recv_indication_cfg;
	/* Bit0 of recv_indication_cfg indicate report terminate event or not */
	if (prSubscribeReq->recv_indication_cfg & BIT(0))
		prSubSpecificInfo->ucReportTerminate = FALSE;
	else
		prSubSpecificInfo->ucReportTerminate = TRUE;

	prSubscribeReq->period = msg->period;

	kalMemZero(aucServiceName, sizeof(aucServiceName));
	kalMemCopy(aucServiceName,
			msg->service_name,
			NAN_MAX_SERVICE_NAME_LEN);
	for (u4Idx = 0; u4Idx < kalStrLen(aucServiceName); u4Idx++)
		aucServiceName[u4Idx] = tolower(aucServiceName[u4Idx]);

	nan_rdf_sha256_init(&r_SHA_256_state);
	sha256_process(&r_SHA_256_state, aucServiceName,
		       kalStrLen(aucServiceName));
	kalMemZero(auc_tk, sizeof(auc_tk));
	sha256_done(&r_SHA_256_state, auc_tk);
	kalMemCopy(prSubscribeReq->service_name_hash, auc_tk,
		   NAN_SERVICE_HASH_LENGTH);
	kalMemCopy(g_aucNanServiceId,
			prSubscribeReq->service_name_hash,
			6);
	prSubscribeReq->service_specific_info_len =
		msg->service_specific_info_len;
	if (prSubscribeReq->service_specific_info_len >
	    NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN)
		prSubscribeReq->service_specific_info_len =
			NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN;
	kalMemCopy(prSubscribeReq->service_specific_info,
		   msg->service_specific_info,
		   prSubscribeReq->service_specific_info_len);

	prSubscribeReq->sdea_service_specific_info_len =
		msg->sdea_service_specific_info_len;
	if (prSubscribeReq->sdea_service_specific_info_len >
	    NAN_MAX_SDEA_LEN)
		prSubscribeReq->sdea_service_specific_info_len =
			NAN_MAX_SDEA_LEN;

	DBGLOG(INIT, INFO,
		"nan: sdea_service_specific_info_len = %d\n",
		prSubscribeReq->sdea_service_specific_info_len);
	kalMemCopy(prSubscribeReq->sdea_service_specific_info,
		   msg->sdea_service_specific_info,
		   prSubscribeReq->sdea_service_specific_info_len);

	if (prAdapter->fgIsNANfromHAL == FALSE) {
		nanConvertUccMatchFilter(prSubscribeReq->tx_match_filter,
					 msg->tx_match_filter,
					 msg->tx_match_filter_len,
					 &prSubscribeReq->tx_match_filter_len);
		nanUtilDump(prAdapter, "tx match filter",
			    prSubscribeReq->tx_match_filter,
			    prSubscribeReq->tx_match_filter_len);

		nanConvertUccMatchFilter(prSubscribeReq->rx_match_filter,
					 msg->rx_match_filter,
					 msg->rx_match_filter_len,
					 &prSubscribeReq->rx_match_filter_len);
		nanUtilDump(prAdapter, "rx match filter",
			    prSubscribeReq->rx_match_filter,
			    prSubscribeReq->rx_match_filter_len);
	} else {
		/* fgIsNANfromHAL, then no need to convert match filter */
		prSubscribeReq->tx_match_filter_len = msg->tx_match_filter_len;
		if (prSubscribeReq->tx_match_filter_len >
			NAN_FW_MAX_MATCH_FILTER_LEN)
			prSubscribeReq->tx_match_filter_len =
			NAN_FW_MAX_MATCH_FILTER_LEN;
		kalMemCopy(prSubscribeReq->tx_match_filter,
			   msg->tx_match_filter,
			   prSubscribeReq->tx_match_filter_len);
		nanUtilDump(prAdapter, "tx match filter",
			    prSubscribeReq->tx_match_filter,
			    prSubscribeReq->tx_match_filter_len);

		prSubscribeReq->rx_match_filter_len = msg->rx_match_filter_len;
		if (prSubscribeReq->rx_match_filter_len >
			NAN_FW_MAX_MATCH_FILTER_LEN)
			prSubscribeReq->rx_match_filter_len =
			NAN_FW_MAX_MATCH_FILTER_LEN;
		kalMemCopy(prSubscribeReq->rx_match_filter,
			   msg->rx_match_filter,
			   prSubscribeReq->rx_match_filter_len);
		nanUtilDump(prAdapter, "rx match filter",
			    prSubscribeReq->rx_match_filter,
			    prSubscribeReq->rx_match_filter_len);
	}

	prSubscribeReq->serviceResponseFilter = msg->serviceResponseFilter;
	prSubscribeReq->serviceResponseInclude = msg->serviceResponseInclude;
	prSubscribeReq->useServiceResponseFilter =
		msg->useServiceResponseFilter;
	if (msg->num_intf_addr_present > NAN_MAX_SUBSCRIBE_MAX_ADDRESS)
		msg->num_intf_addr_present = NAN_MAX_SUBSCRIBE_MAX_ADDRESS;
	prSubscribeReq->num_intf_addr_present = msg->num_intf_addr_present;

	if (prSubscribeReq->num_intf_addr_present >
		NAN_MAX_SUBSCRIBE_MAX_ADDRESS)
		prSubscribeReq->num_intf_addr_present =
		NAN_MAX_SUBSCRIBE_MAX_ADDRESS;
	kalMemCopy(prSubscribeReq->intf_addr, msg->intf_addr,
		   prSubscribeReq->num_intf_addr_present * MAC_ADDR_LEN);
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	if (fgIsNanPairingEn) {
		prSubscribeReq->pairing_enable = msg->pairing_enable;
		prSubscribeReq->key_caching_enable = msg->key_caching_enable;
		prSubscribeReq->bootstrap_method =  msg->bootstrap_method;

		if (prSubscribeReq->pairing_enable) {
			nanDiscSetupInstance(prSubscribeReq->subscribe_id,
					NAN_SERVICE_TYPE_SUBSCRIBE,
					prSubscribeReq->service_name_hash);
		}
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	/* send command to fw */
	wlanSendSetQueryCmd(prAdapter,		/* prAdapter */
			    CMD_ID_NAN_EXT_CMD,	/* ucCID */
			    TRUE,		/* fgSetQuery */
			    FALSE,		/* fgNeedResp */
			    FALSE,		/* fgIsOid */
			    NULL,		/* pfCmdDoneHandler */
			    NULL,		/* pfCmdTimeoutHandler */
			    u4CmdBufferLen,	/* u4SetQueryInfoLen */
			    prCmdBuffer,	/* pucInfoBuffer */
			    NULL,		/* pvSetQueryBuffer */
			    0 /* u4SetQueryBufferLen */);
	cnmMemFree(prAdapter, prCmdBuffer);

	return u2SubscribeId;
}

void
nanCmdAddCsid(struct ADAPTER *prAdapter, uint8_t ucPubID, uint8_t ucNumCsid,
	      uint8_t *pucCsidList) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_DISC_CMD_ADD_CSID_T *prCmdAddCsid = NULL;
	uint8_t ucIdx;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_DISC_CMD_ADD_CSID_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(NAN, ERROR, "Memory allocation fail\n");
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_ADD_CSID,
					sizeof(struct _NAN_DISC_CMD_ADD_CSID_T),
					u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(NAN, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prCmdAddCsid = (struct _NAN_DISC_CMD_ADD_CSID_T *)prTlvElement->aucbody;
	prCmdAddCsid->ucPubID = ucPubID;
	if (ucNumCsid > NAN_MAX_CIPHER_SUITE_NUM)
		ucNumCsid = NAN_MAX_CIPHER_SUITE_NUM;
	prCmdAddCsid->ucNum = ucNumCsid;
	for (ucIdx = 0; ucIdx < ucNumCsid; ucIdx++) {
		prCmdAddCsid->aucSupportedCSID[ucIdx] = *pucCsidList;
		pucCsidList++;
	}

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, NULL, NULL, u4CmdBufferLen,
				      prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
}

void
nanCmdManageScid(struct ADAPTER *prAdapter, unsigned char fgAddDelete,
		 uint8_t ucPubID, uint8_t *pucScid) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_DISC_CMD_MANAGE_SCID_T *prCmdManageScid = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_DISC_CMD_MANAGE_SCID_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);
	if (!prCmdBuffer) {
		DBGLOG(NAN, ERROR, "Memory allocation fail\n");
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicNanAddNewTlvElement(NAN_CMD_MANAGE_SCID,
				sizeof(struct _NAN_DISC_CMD_MANAGE_SCID_T),
				u4CmdBufferLen, prCmdBuffer);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvElement = nicNanGetTargetTlvElement(prTlvCommon->u2TotalElementNum,
						 prCmdBuffer);
	if (prTlvElement == NULL) {
		DBGLOG(NAN, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prCmdManageScid =
		(struct _NAN_DISC_CMD_MANAGE_SCID_T *)prTlvElement->aucbody;
	prCmdManageScid->fgAddDelete = fgAddDelete;
	prCmdManageScid->ucPubID = ucPubID;
	kalMemCopy(prCmdManageScid->aucSCID, pucScid, NAN_SCID_DEFAULT_LEN);

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, NULL, NULL, u4CmdBufferLen,
				      prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
}

void
nanDiscResetServiceSession(struct ADAPTER *prAdapter,
			   struct _NAN_SERVICE_SESSION_T *prServiceSession) {
	prServiceSession->ucNumCSID = 0;
	prServiceSession->ucNumSCID = 0;
}

struct _NAN_SERVICE_SESSION_T *
nanDiscAcquireServiceSession(struct ADAPTER *prAdapter,
			     uint8_t *pucPublishNmiAddr, uint8_t ucPubID) {
	struct LINK *rSeviceSessionList;
	struct LINK *rFreeServiceSessionList;
	struct _NAN_SERVICE_SESSION_T *prServiceSession;
	struct _NAN_SERVICE_SESSION_T *prServiceSessionNext;
	struct _NAN_SERVICE_SESSION_T *prAgingServiceSession;
	struct _NAN_DISC_ENGINE_T *prDiscEngine = &g_rNanDiscEngine;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return NULL;
	}
	rSeviceSessionList = &prDiscEngine->rSeviceSessionList;
	rFreeServiceSessionList = &prDiscEngine->rFreeServiceSessionList;
	prAgingServiceSession = NULL;

	LINK_FOR_EACH_ENTRY_SAFE(prServiceSession, prServiceSessionNext,
				 rSeviceSessionList, rLinkEntry,
				 struct _NAN_SERVICE_SESSION_T) {
		if (EQUAL_MAC_ADDR(prServiceSession->aucPublishNmiAddr,
				   pucPublishNmiAddr) &&
		    (prServiceSession->ucPublishID == ucPubID)) {
			LINK_REMOVE_KNOWN_ENTRY(rSeviceSessionList,
						prServiceSession);

			LINK_INSERT_TAIL(rSeviceSessionList,
					 &prServiceSession->rLinkEntry);
			return prServiceSession;
		} else if (prAgingServiceSession == NULL)
			prAgingServiceSession = prServiceSession;
	}

	LINK_REMOVE_HEAD(rFreeServiceSessionList, prServiceSession,
			 struct _NAN_SERVICE_SESSION_T *);
	if (prServiceSession) {
		kalMemZero(prServiceSession,
			   sizeof(struct _NAN_SERVICE_SESSION_T));
		kalMemCopy(prServiceSession->aucPublishNmiAddr,
			   pucPublishNmiAddr, MAC_ADDR_LEN);
		nanDiscResetServiceSession(prAdapter, prServiceSession);

		LINK_INSERT_TAIL(rSeviceSessionList,
				 &prServiceSession->rLinkEntry);
	} else if (prAgingServiceSession) {
		LINK_REMOVE_KNOWN_ENTRY(rSeviceSessionList,
					prAgingServiceSession);

		kalMemZero(prAgingServiceSession,
			   sizeof(struct _NAN_SERVICE_SESSION_T));
		kalMemCopy(prAgingServiceSession->aucPublishNmiAddr,
			   pucPublishNmiAddr, MAC_ADDR_LEN);
		nanDiscResetServiceSession(prAdapter, prAgingServiceSession);

		LINK_INSERT_TAIL(rSeviceSessionList,
				 &prAgingServiceSession->rLinkEntry);
		return prAgingServiceSession;
	}

	return prServiceSession;
}

void
nanDiscReleaseAllServiceSession(struct ADAPTER *prAdapter) {
	struct LINK *rSeviceSessionList;
	struct LINK *rFreeServiceSessionList;
	struct _NAN_SERVICE_SESSION_T *prServiceSession;
	struct _NAN_DISC_ENGINE_T *prDiscEngine = &g_rNanDiscEngine;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return;
	}
	rSeviceSessionList = &prDiscEngine->rSeviceSessionList;
	rFreeServiceSessionList = &prDiscEngine->rFreeServiceSessionList;

	while (!LINK_IS_EMPTY(rSeviceSessionList)) {
		LINK_REMOVE_HEAD(rSeviceSessionList, prServiceSession,
				 struct _NAN_SERVICE_SESSION_T *);
		if (prServiceSession) {
			kalMemZero(prServiceSession, sizeof(*prServiceSession));
			LINK_INSERT_TAIL(rFreeServiceSessionList,
					 &prServiceSession->rLinkEntry);
		} else {
			/* should not deliver to this function */
			DBGLOG(NAN, ERROR, "prServiceSession error!\n");
			return;
		}
	}
}

struct _NAN_SERVICE_SESSION_T *
nanDiscSearchServiceSession(struct ADAPTER *prAdapter,
			    uint8_t *pucPublishNmiAddr, uint8_t ucPubID) {
	struct LINK *rSeviceSessionList;
	struct _NAN_SERVICE_SESSION_T *prServiceSession;
	struct _NAN_DISC_ENGINE_T *prDiscEngine = &g_rNanDiscEngine;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return NULL;
	}

	rSeviceSessionList = &prDiscEngine->rSeviceSessionList;

	LINK_FOR_EACH_ENTRY(prServiceSession, rSeviceSessionList, rLinkEntry,
			    struct _NAN_SERVICE_SESSION_T) {
		if (EQUAL_MAC_ADDR(prServiceSession->aucPublishNmiAddr,
				   pucPublishNmiAddr) &&
		    (prServiceSession->ucPublishID == ucPubID)) {
			return prServiceSession;
		}
	}

	return NULL;
}

uint32_t
nanDiscInit(struct ADAPTER *prAdapter) {
	uint32_t u4Idx;
	struct _NAN_DISC_ENGINE_T *prDiscEngine = &g_rNanDiscEngine;

	if (prDiscEngine->fgInit == FALSE) {
		LINK_INITIALIZE(&prDiscEngine->rSeviceSessionList);
		LINK_INITIALIZE(&prDiscEngine->rFreeServiceSessionList);
		for (u4Idx = 0; u4Idx < NAN_NUM_SERVICE_SESSION; u4Idx++) {
			kalMemZero(&prDiscEngine->arServiceSessionList[u4Idx],
				   sizeof(struct _NAN_SERVICE_SESSION_T));
			LINK_INSERT_TAIL(
				&prDiscEngine->rFreeServiceSessionList,
				&prDiscEngine->arServiceSessionList[u4Idx]
					 .rLinkEntry);
		}
	}

	prDiscEngine->fgInit = TRUE;

	nanDiscReleaseAllServiceSession(prAdapter);
	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDiscUpdateSecContextInfoAttr(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf) {
	uint8_t *pucPublishNmiAddr;
	uint8_t *pucSecContextInfoAttr;
	struct _NAN_SCHED_EVENT_NAN_ATTR_T *prEventNanAttr;
	struct _NAN_ATTR_SECURITY_CONTEXT_INFO_T *prAttrSecContextInfo;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	struct _NAN_SERVICE_SESSION_T *prServiceSession;
	uint8_t *pucSecContextList;
	int32_t i4RemainLength;
	struct _NAN_SECURITY_CONTEXT_ID_T *prSecContext;

	prEventNanAttr = (struct _NAN_SCHED_EVENT_NAN_ATTR_T *)pcuEvtBuf;
	pucPublishNmiAddr = prEventNanAttr->aucNmiAddr;
	pucSecContextInfoAttr = prEventNanAttr->aucNanAttr;

	prAttrSecContextInfo = (struct _NAN_ATTR_SECURITY_CONTEXT_INFO_T *)
		pucSecContextInfoAttr;

	pucSecContextList = prAttrSecContextInfo->aucSecurityContextIDList;
	i4RemainLength = prAttrSecContextInfo->u2Length;

	while (i4RemainLength >
	       (sizeof(struct _NAN_SECURITY_CONTEXT_ID_T) - 1)) {
		prSecContext =
			(struct _NAN_SECURITY_CONTEXT_ID_T *)pucSecContextList;
		i4RemainLength -=
			(prSecContext->u2SecurityContextIDTypeLength +
			 sizeof(struct _NAN_SECURITY_CONTEXT_ID_T) - 1);

		if (prSecContext->ucSecurityContextIDType != 1)
			continue;

		if (prSecContext->u2SecurityContextIDTypeLength !=
		    NAN_SCID_DEFAULT_LEN)
			continue;

		prServiceSession = nanDiscAcquireServiceSession(
			prAdapter, pucPublishNmiAddr,
			prSecContext->ucPublishID);
		if (prServiceSession) {
			if (prServiceSession->ucNumSCID < NAN_MAX_SCID_NUM) {
				kalMemCopy(
					&prServiceSession->aaucSupportedSCID
						 [prServiceSession->ucNumSCID]
						 [0],
					prSecContext->aucSecurityContextID,
					NAN_SCID_DEFAULT_LEN);
				prServiceSession->ucNumSCID++;
			}
		}
	}

	return rRetStatus;
}

uint32_t nanDiscUpdateCipherSuiteInfoAttr(struct ADAPTER *prAdapter,
					  uint8_t *pcuEvtBuf)
{
	uint8_t *pucPublishNmiAddr;
	uint8_t *pucCipherSuiteInfoAttr;
	struct _NAN_SCHED_EVENT_NAN_ATTR_T *prEventNanAttr;
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T *prAttrCipherSuiteInfo;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	struct _NAN_SERVICE_SESSION_T *prServiceSession;
	struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCipherSuite;
	uint8_t ucNumCsid;
	uint8_t ucIdx;
	struct _NAN_ATTR_HDR_T *prAttrHdr;

	prEventNanAttr = (struct _NAN_SCHED_EVENT_NAN_ATTR_T *)pcuEvtBuf;
	pucPublishNmiAddr = prEventNanAttr->aucNmiAddr;
	pucCipherSuiteInfoAttr = prEventNanAttr->aucNanAttr;

	prAttrHdr = (struct _NAN_ATTR_HDR_T *)prEventNanAttr->aucNanAttr;
	/* nanUtilDump(prAdapter, "NAN Attribute",
	 *	     (PUINT_8)prAttrHdr, (prAttrHdr->u2Length + 3));
	 */

	prAttrCipherSuiteInfo =
		(struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)pucCipherSuiteInfoAttr;

	ucNumCsid = (prAttrCipherSuiteInfo->u2Length - 1) /
		    sizeof(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T);

	prCipherSuite = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
				prAttrCipherSuiteInfo->aucCipherSuiteList;
	for (ucIdx = 0; ucIdx < ucNumCsid; ucIdx++) {
		prServiceSession = nanDiscAcquireServiceSession(
			prAdapter, pucPublishNmiAddr,
			prCipherSuite->ucPublishID);
		if (prServiceSession) {
			if (prServiceSession->ucNumCSID <
			    NAN_MAX_CIPHER_SUITE_NUM) {
				prServiceSession->aucSupportedCipherSuite
					[prServiceSession->ucNumCSID] =
					prCipherSuite->ucCipherSuiteID;
				prServiceSession->ucNumCSID++;
			}
		}

		prCipherSuite++;
	}

	return rRetStatus;
}

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
/*----------------------------------------------------------------------------*/
/*!
 * \brief        Utility function to compose NAF header
 *
 * \param[in]
 *
 * \return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanDiscComposeNAFHeader(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		enum _NAN_ACTION_T eAction,
		uint8_t *pucLocalMacAddr, uint8_t *pucPeerMacAddr) {
	struct _NAN_SDF_FRAME_T *prNAF = NULL;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;

	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");

	prPairingFsm = pairingFsmSearch(prAdapter, pucPeerMacAddr);

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "prPairingFsm error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	prNAF = (struct _NAN_SDF_FRAME_T *)prMsduInfo->prPacket;

	/* MAC header */
	WLAN_SET_FIELD_16(&(prNAF->u2FrameCtrl), MAC_FRAME_ACTION);

	COPY_MAC_ADDR(prNAF->aucDestAddr, pucPeerMacAddr);
	COPY_MAC_ADDR(prNAF->aucSrcAddr, pucLocalMacAddr);
	COPY_MAC_ADDR(prNAF->aucClusterID,
			nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_MAIN)
			->aucClusterId);

	prNAF->u2SeqCtrl = 0;

	/* action frame body */
	if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED ||
		prPairingFsm->ePairingState ==
			NAN_PAIRING_PAIRED_VERIFICATION) {
		prNAF->ucCategory = CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION;
	} else {
		prNAF->ucCategory = CATEGORY_PUBLIC_ACTION;
	}
	prNAF->ucAction = ACTION_PUBLIC_VENDOR_SPECIFIC;
	kalMemCpyS(prNAF->aucOUI,
			VENDOR_OUI_LEN,
			aucOui,
			VENDOR_OUI_LEN);
	prNAF->ucOUItype = VENDOR_OUI_TYPE_NAN_SDF;

	/* Append attr beginning from here */
	prMsduInfo->u2FrameLength =
		OFFSET_OF(struct _NAN_SDF_FRAME_T, aucInfoContent);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            Send NAF - Follow-up
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanDiscSendFollowup(IN struct ADAPTER *prAdapter,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq) {
	size_t i = 0;
	size_t szEstimatedFrameLen = 0;
	struct MSDU_INFO *prMsduInfo = NULL;
	struct STA_RECORD *prStaRec = NULL;
	uint8_t *pucLocalAddr = NULL;
	uint8_t *pucPeerAddr = NULL;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");

	pucPeerAddr = prNanFollowupReq->addr;
	prPairingFsm = pairingFsmSearch(prAdapter, pucPeerAddr);
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "PairingFSM error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	szEstimatedFrameLen =
		OFFSET_OF(struct _NAN_SDF_FRAME_T, aucInfoContent);

	/* estimate total length of NAN attributes */
	for (i = 0; i < sizeof(txDiscAttributeTable) /
			sizeof(struct _APPEND_DISC_ATTR_ENTRY_T);
			i++) {
		if (txDiscAttributeTable[i].pfnCalculateVariableAttrLen) {
			szEstimatedFrameLen +=
				txDiscAttributeTable[i]
				.pfnCalculateVariableAttrLen(
						prAdapter, prNanFollowupReq);
			DBGLOG(NAN, INFO, "FrameLen=%zu, AttrLen=%zu\n",
					szEstimatedFrameLen,
					txDiscAttributeTable[i]
					.pfnCalculateVariableAttrLen(
						prAdapter, prNanFollowupReq));
		}
	}
	DBGLOG(NAN, ERROR, "szEstimateFrameLen=%zu\n",
		szEstimatedFrameLen);
	/* allocate MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, szEstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(NAN, WARN,
		       "NAN Discovery Engine: packet allocation failure\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero((uint8_t *)prMsduInfo->prPacket, szEstimatedFrameLen);
	pucLocalAddr = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	nanDiscComposeNAFHeader(prAdapter, prMsduInfo,
			NAN_ACTION_DATA_PATH_REQUEST,
			pucLocalAddr, pucPeerAddr);
	DBGLOG(NAN, INFO, "frame_len=%u\n", prMsduInfo->u2FrameLength);
/* modify to SDF attribute */
	/* fill NAN attributes */
	for (i = 0; i < sizeof(txDiscAttributeTable) /
			sizeof(struct _APPEND_ATTR_ENTRY_T);
			i++) {
		if (txDiscAttributeTable[i].pfnCalculateVariableAttrLen &&
			txDiscAttributeTable[i].pfnCalculateVariableAttrLen(
				prAdapter, prNanFollowupReq) != 0) {
			if (txDiscAttributeTable[i].pfnAppendAttr)
				txDiscAttributeTable[i].pfnAppendAttr(
					prAdapter, prMsduInfo,
					prNanFollowupReq);
			DBGLOG(NAN, INFO,
				"i=%zu, frame_len=%u\n",
				i, prMsduInfo->u2FrameLength);
		}
	}

	/* NAN PAIRING */
	prPairingFsm = pairingFsmSearch(prAdapter, pucPeerAddr);
	DBGLOG(NAN, INFO, "peerAddr=%x:%x:%x:%x:%x:%x\n",
	       pucPeerAddr[0], pucPeerAddr[1], pucPeerAddr[2],
	       pucPeerAddr[3], pucPeerAddr[4], pucPeerAddr[5]);
	if (prPairingFsm &&
		prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
		DBGLOG(NAN, INFO, "Fsm=%u, StaIdx=%u\n",
		       prPairingFsm->ucIndex,
		       prPairingFsm->prStaRec->ucIndex);
		prStaRec = prPairingFsm->prStaRec;
	} else {
		DBGLOG(NAN, INFO, "Null StaRec\n");
	}

	DBGLOG(NAN, INFO, "FrameLength=%u\n", prMsduInfo->u2FrameLength);
	nanUtilDump(prAdapter, "NAN follow-up",
		prMsduInfo->prPacket, prMsduInfo->u2FrameLength);

	return nanDiscSendSDF(
		prAdapter, prMsduInfo, prMsduInfo->u2FrameLength,
		nanSDFTxDone, prStaRec);

}


/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAF TX Wrapper Function
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanDiscSendSDF(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo, IN uint16_t u2FrameLength,
		IN PFN_TX_DONE_HANDLER pfTxDoneHandler,
		IN struct STA_RECORD *prSelectStaRec)
{
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
#endif

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(
			prAdapter, prMsduInfo,
			(prSelectStaRec != NULL)
			? (prSelectStaRec->ucBssIndex)
			: nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_MAIN)
			->ucBssIndex,
			(prSelectStaRec != NULL) ? (prSelectStaRec->ucIndex)
			: (STA_REC_INDEX_NOT_FOUND),
			OFFSET_OF(struct _NAN_ACTION_FRAME_T, ucCategory),
			u2FrameLength, pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	prMsduInfo->ucTxToNafQueFlag = TRUE;

	if (prSelectStaRec) {
		DBGLOG(NAN, INFO, "NoPmf=%u, ApplyPmf=%u\n",
			prAdapter->rWifiVar.fgNoPmf,
			prSelectStaRec->rPmfCfg.fgApplyPmf);
	} else {
		DBGLOG(NAN, INFO, "NoPmf=%u\n", prAdapter->rWifiVar.fgNoPmf);
	}

	if (!prAdapter->rWifiVar.fgNoPmf && (prSelectStaRec != NULL) &&
			(prSelectStaRec->rPmfCfg.fgApplyPmf == TRUE)) {
		struct _NAN_ACTION_FRAME_T *prNAF = NULL;

		prNAF = (struct _NAN_ACTION_FRAME_T *)prMsduInfo->prPacket;
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				TRUE);
		DBGLOG(NAN, INFO, "Tx PMF, OUItype:%u, OUISubtype:%u\n",
			prNAF->ucOUItype, prNAF->ucOUISubtype);
		DBGLOG(NAN, INFO,
			"StaIdx:%u, MAC=>%02x:%02x:%02x:%02x:%02x:%02x\n",
			prSelectStaRec->ucIndex, prSelectStaRec->aucMacAddr[0],
			prSelectStaRec->aucMacAddr[1],
			prSelectStaRec->aucMacAddr[2],
			prSelectStaRec->aucMacAddr[3],
			prSelectStaRec->aucMacAddr[4],
			prSelectStaRec->aucMacAddr[5]);
	}
	nicTxSetPktLifeTime(prAdapter, prMsduInfo, 0);
	nicTxSetPktRetryLimit(prMsduInfo, SDF_TX_RETRY_COUNT_LIMIT*10);


	DBGLOG(NAN, ERROR, "Is8011=%u\n", prMsduInfo->fgIs802_11);
	/* 4 <6> Enqueue the frame to send this SDF frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
size_t
nanDiscSdaAttrLength(struct ADAPTER *prAdapter,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq) {
	uint16_t u2AttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
#endif

	/* ATTR_SDA */
	u2AttrLength = sizeof(struct _NAN_ATTR_SDA_T);
	DBGLOG(NAN, INFO, "len=%u\n", u2AttrLength);
	return u2AttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDP ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDiscSdaAttrAppend(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq) {
	size_t szAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
#endif

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return;
	}

	szAttrLength = nanDiscSdaAttrLength(prAdapter, prNanFollowupReq);

	if (szAttrLength != 0) {
		nanDiscSdaAttrAppendImpl(prAdapter, prMsduInfo,
				prNanFollowupReq, 0, 0);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDP ATTR Implementation
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDiscSdaAttrAppendImpl(IN struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		IN struct NanFWTransmitFollowupRequest *prNanFollowupReq,
		IN uint8_t ucTypeStatus, IN uint8_t ucReasonCode) {
	struct _NAN_ATTR_SDA_T *prAttrSda = NULL;
	size_t szAttrLength = 0;
	size_t szIdx = 0;

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return;
	}

	prAttrSda = (struct _NAN_ATTR_SDA_T *)((uint8_t *)prMsduInfo->prPacket +
			prMsduInfo->u2FrameLength);
	DBGLOG(NAN, INFO, "MsduInfo->FrameLength=%u\n",
	       prMsduInfo->u2FrameLength);

	/* AttrId(1) + AttrLen(2) */
	szAttrLength = sizeof(struct _NAN_ATTR_SDA_T) - 3;

	if (szAttrLength != 0) {
		prAttrSda->ucAttribID = NAN_ATTR_ID_SERVICE_DESCRIPTOR;
		prAttrSda->u2Len = szAttrLength;
		prAttrSda->ucInstanceID =
			prNanFollowupReq->publish_subscribe_id;
		prAttrSda->ucRequesterID =
			prNanFollowupReq->requestor_instance_id;
		prAttrSda->ucServiceControl |=
			NAN_SDA_SERVICE_CONTROL_TYPE_FOLLOWUP;
		/* TODO: porting instance and set service hash */
		for (szIdx = 0; szIdx < NAN_SERVICE_INSTANCE_NUM; szIdx++) {
			if (g_arNanInstance[szIdx].ucInstanceID ==
				prNanFollowupReq->publish_subscribe_id) {
				DBGLOG(NAN, INFO, PAIRING_DBGM31,
				       __func__, szIdx,
				       g_arNanInstance[szIdx].ucInstanceID);
				kalMemCpyS(prAttrSda->aucServiceID,
					NAN_SERVICE_HASH_LENGTH,
					g_arNanInstance[szIdx].aucServiceHash,
					NAN_SERVICE_HASH_LENGTH);
				break;
			}
		}
		prMsduInfo->u2FrameLength += (sizeof(struct _NAN_ATTR_SDA_T));
		DBGLOG(NAN, INFO, "MsduInfo->FrameLength=%u <---\n",
			prMsduInfo->u2FrameLength);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
size_t
nanDiscSdeaAttrLength(struct ADAPTER *prAdapter,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq) {
	size_t szAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
#endif

	/* ATTR_SDEA - SDEA_Detail(1) */
	szAttrLength = sizeof(struct _NAN_ATTR_SDEA_T);
	DBGLOG(NAN, INFO, "len=%zu\n", szAttrLength);

	return szAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDP ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDiscSdeaAttrAppend(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq)
{
	size_t szAttrLength = 0;

	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return;
	}

	szAttrLength = nanDiscSdeaAttrLength(prAdapter, prNanFollowupReq);

	if (szAttrLength != 0) {
		nanDiscSdeaAttrAppendImpl(prAdapter, prMsduInfo,
				prNanFollowupReq, 0, 0);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDP ATTR Implementation
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDiscSdeaAttrAppendImpl(IN struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		IN struct NanFWTransmitFollowupRequest *prNanFollowupReq,
		IN uint8_t ucTypeStatus, IN uint8_t ucReasonCode) {
	struct _NAN_ATTR_SDEA_T *prAttrSdea = NULL;
	size_t szAttrLength = 0;

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return;
	}

	DBGLOG(NAN, INFO, "MsduInfo->FrameLength=%u\n",
		prMsduInfo->u2FrameLength);

	prAttrSdea =
	(struct _NAN_ATTR_SDEA_T *)((uint8_t *)prMsduInfo->prPacket +
		prMsduInfo->u2FrameLength);
	szAttrLength = 3; /* Instance ID(1) + Control(2) */

	if (szAttrLength != 0) {
		prAttrSdea->ucAttribID = NAN_ATTR_ID_SDEA;
		prAttrSdea->u2Len = szAttrLength;
		prAttrSdea->ucInstanceID =
			prNanFollowupReq->publish_subscribe_id;
		prAttrSdea->u2Control = 0;
		prMsduInfo->u2FrameLength += (sizeof(struct _NAN_ATTR_SDEA_T));
		DBGLOG(NAN, INFO, "MsduInfo->FrameLength=%u <---\n",
			prMsduInfo->u2FrameLength);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NAN Shared Key Descriptor
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
size_t
nanDiscSharedKeyAttrLength(struct ADAPTER *prAdapter,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq) {
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	size_t szLen = 0;
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
#endif

	prPairingFsm = pairingFsmSearch(prAdapter, prNanFollowupReq->addr);
	if (prPairingFsm == NULL)
		return 0;

	DBGLOG(NAN, INFO,
	       "wpa_eapol_key=%zu, nik_kde=%zu, nik_lifetime_kde=%zu\n",
	       sizeof(struct wpa_eapol_key),	sizeof(struct NIK_KDE_INFO),
	       sizeof(struct NIK_LIFETIME_KDE_INFO));
	szLen = sizeof(struct _NAN_SEC_KDE_ATTR_HDR) +
		sizeof(struct wpa_eapol_key) +
		sizeof(struct NIK_KDE_INFO) +
		sizeof(struct NIK_LIFETIME_KDE_INFO);
	DBGLOG(NAN, INFO, "u2Len=%zu\n", szLen);

	szLen = sizeof(struct _NAN_SEC_KDE_ATTR_HDR) +
		sizeof(struct wpa_eapol_key) + 48;
	DBGLOG(NAN, INFO, "_u2Len=%zu\n", szLen);
	return szLen;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NAN Shared Key Descriptor
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
struct NIK_KDE_INFO g_NikKde;
struct NIK_LIFETIME_KDE_INFO g_NikLifetimeKde;

void
nanDiscSharedKeyAttrAppend(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		struct NanFWTransmitFollowupRequest *prNanFollowupReq)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct NIK_KDE_INFO *prNikKde = &g_NikKde;
	struct NIK_LIFETIME_KDE_INFO *prNikLifetimeKde = &g_NikLifetimeKde;
	struct wpa_eapol_key *key = NULL;
	size_t mic_len = 0, keyhdrlen = 0, mic_data_len = 0; /* len */
	struct _NAN_SEC_KDE_ATTR_HDR *prNanSecKdeAttrHdr = NULL;
	size_t szTotalLen = 0;
	int pairwise = 0;
	u8 *key_data = NULL;
	u8 *buf = NULL, *pos = NULL, *mic_data = NULL;
	size_t key_data_len = 0, pad_len = 0;
	/* TODO: Check following input in nan_sec_wpa_send_eapol */
	int key_info = 0;
	struct wpa_state_machine *sm = &g_arNanWpaAuthSm[0];
	u8 *key_rsc = NULL;
	int encr = TRUE;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
#endif

	prPairingFsm = pairingFsmSearch(prAdapter, prNanFollowupReq->addr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "Pairing FSM Null\n");
		return;
	}

	pairwise = !!(key_info & WPA_KEY_INFO_KEY_TYPE);

	mic_len = 16; /* Check 16 or 24 */
	keyhdrlen = sizeof(*key);
	/* KDE len: NIK(24) + NIK_LIFETIME(11) */
	key_data_len =
	sizeof(struct NIK_KDE_INFO) +
	sizeof(struct NIK_LIFETIME_KDE_INFO);

	pad_len = key_data_len % 8;
	if (pad_len)
		pad_len = 8 - pad_len;
	key_data_len += pad_len + 8;

	szTotalLen =
	sizeof(struct _NAN_SEC_KDE_ATTR_HDR) + keyhdrlen + key_data_len;

	DBGLOG(NAN, INFO, "MsduInfo->FrameLength=%u\n",
		prMsduInfo->u2FrameLength);
	prNanSecKdeAttrHdr =
	(struct _NAN_SEC_KDE_ATTR_HDR *)((uint8_t *)prMsduInfo->prPacket +
			prMsduInfo->u2FrameLength);
	prNanSecKdeAttrHdr->u1AttrId = NAN_ATTR_ID_SHARED_KEY_DESCRIPTOR;
	prNanSecKdeAttrHdr->u2AttrLen =
		keyhdrlen + key_data_len + 1; /*1:publishId*/
	prNanSecKdeAttrHdr->u1PublishId =
		prNanFollowupReq->publish_subscribe_id; /* Check it */

	DBGLOG(NAN, INFO, "u4TotalLen:%zu\n", szTotalLen);
	DBGLOG(NAN, INFO, "keyhdrlen=%zu, key_data_len=%zu, pad_len=%zu\n",
		keyhdrlen, key_data_len, pad_len);
	DBGLOG(NAN, INFO, "sizeof(wpa_eapol_key)=%zu\n",
		sizeof(struct wpa_eapol_key));
	DBGLOG(NAN, INFO, "nik_kde=%zu, nik_lifetime_kde=%zu\n",
		sizeof(struct NIK_KDE_INFO),
		sizeof(struct NIK_LIFETIME_KDE_INFO));

	key = (struct wpa_eapol_key *)((uint8_t *)prNanSecKdeAttrHdr +
			sizeof(struct _NAN_SEC_KDE_ATTR_HDR));

	key_data = ((u8 *)key) + keyhdrlen;

	/* Setup Key Header */
	key->type = EAPOL_KEY_TYPE_RSN;
#if 0
	key_info |= WPA_KEY_INFO_TYPE_AKM_DEFINED;
	key_info |= WPA_KEY_INFO_ENCR_KEY_DATA;
#endif
	/* key_info align to brcm = 0x13c8 */
	key_info = 0x12c8;
	WPA_PUT_BE16(key->key_info, key_info);

	WPA_PUT_BE16(key->key_length, 0);

	inc_byte_array(sm->key_replay[0].counter, WPA_REPLAY_COUNTER_LEN);
	kalMemCpyS(key->replay_counter,
			WPA_REPLAY_COUNTER_LEN,
			sm->key_replay[0].counter,
			WPA_REPLAY_COUNTER_LEN);
	wpa_hexdump(MSG_DEBUG, "WPA: Replay Counter", key->replay_counter,
			WPA_REPLAY_COUNTER_LEN);
	sm->key_replay[0].valid = TRUE;

	/* TO CHECK: nonce value */
	kalMemZero(key->key_nonce, WPA_NONCE_LEN);

	if (key_rsc)
		kalMemCpyS(key->key_rsc,
				WPA_KEY_RSC_LEN,
				key_rsc,
				WPA_KEY_RSC_LEN);
#if 0
	if (kde && !encr) {
		kalMemCpyS(key_data,
				key_data_len,
				kde,
				key_data_len);
		WPA_PUT_BE16(key->key_data_length, key_data_len);
	} else
#endif
	if (encr && key_data_len) {
		buf = os_zalloc(key_data_len);
		if (buf == NULL) {
			/* os_free(hdr); */
			return;
		}
		pos = buf;
		pairingComposeNikKde(prNikKde, prPairingFsm);
		pairingComposeNikLifetimeKde(prNikLifetimeKde, prPairingFsm);
		kalMemCpyS(pos, sizeof(struct NIK_KDE_INFO),
				prNikKde, sizeof(struct NIK_KDE_INFO));
		pos += sizeof(struct NIK_KDE_INFO);
		kalMemCpyS(pos, sizeof(struct NIK_LIFETIME_KDE_INFO),
			prNikLifetimeKde,
			sizeof(struct NIK_LIFETIME_KDE_INFO));
		pos += sizeof(struct NIK_LIFETIME_KDE_INFO);

		DBGLOG(NAN, INFO, "compose kde done\n");
		nanUtilDump(prAdapter, "NAN Plaintext EAPOL-Key Key Data",
			buf, key_data_len);

		DBGLOG(NAN, INFO, "go aes_wrap\n");
		if (aes_wrap(prPairingFsm->prPtk->kek,
			prPairingFsm->prPtk->kek_len,
			(key_data_len - 8) / 8, buf, key_data)) {

			DBGLOG(NAN, INFO, "aes_wrap wrong\n");
			/* os_free(hdr); */
			os_free(buf);
			return;
		}
		DBGLOG(NAN, INFO, "aes_wrap done, set key_data_length=%zu\n",
		       key_data_len);
		nanUtilDump(prAdapter, "NAN Encrypted Key Data",
			key_data, key_data_len);
		nanUtilDump(prAdapter, "full key data",
			(uint8_t *)prNanSecKdeAttrHdr, szTotalLen);
		WPA_PUT_BE16(key->key_data_length,
				key_data_len);
		os_free(buf);
	}

	key_info |= WPA_KEY_INFO_MIC;
	if (key_info & WPA_KEY_INFO_MIC) {
		/* u8 *key_mic; */
		mic_data_len = keyhdrlen + key_data_len;
		mic_data = os_zalloc(mic_data_len);
		if (mic_data) {
			kalMemCpyS(mic_data, keyhdrlen,
				(u8 *)key, keyhdrlen);
			kalMemCpyS(mic_data + keyhdrlen, key_data_len,
				key_data, key_data_len);

			DBGLOG(NAN, INFO,
			       "mic_data_len=%zu, keyhdrlen=%zu, key_data_len=%zu\n",
			       mic_data_len, keyhdrlen, key_data_len);
			nanUtilDump(prAdapter,
				"MIC Input", mic_data, mic_data_len);

			wpa_eapol_key_mic_wpa(prPairingFsm->prPtk->kck,
				prPairingFsm->prPtk->kck_len,
				WPA_KEY_MGMT_PSK,
				WPA_KEY_INFO_TYPE_HMAC_SHA1_AES,
				mic_data, mic_data_len, key->key_mic);

			nanUtilDump(prAdapter, "SKDA MIC", key->key_mic, 16);

			WPA_PUT_BE16(key->key_info, key_info);
			os_free(mic_data);
		}
	}

	prMsduInfo->u2FrameLength += szTotalLen;
	DBGLOG(NAN, INFO, "MsduInfo->FrameLength=%u <---\n",
		prMsduInfo->u2FrameLength);
}

void
nanDiscSetupInstance(uint8_t ucInstanceId,
	uint8_t type, uint8_t *service_hash)
{
	size_t szIdx = 0;

	TRACE_FUNC(NAN, INFO, "[%s] Enter\n");
	for (szIdx = 0; szIdx < NAN_SERVICE_INSTANCE_NUM ; szIdx++) {
		if (g_arNanInstance[szIdx].ucInstanceID == 0) {
			g_arNanInstance[szIdx].ucInstanceID =
				ucInstanceId;
			g_arNanInstance[szIdx].eType = type;
			kalMemCpyS(g_arNanInstance[szIdx].
				aucServiceHash,
				NAN_SERVICE_HASH_LENGTH,
				service_hash,
				NAN_SERVICE_HASH_LENGTH);
		}
	}
}

uint8_t
nanDiscIsInstancePub(uint8_t ucInstanceId) {
	size_t szIdx = 0;

	for (szIdx = 0; szIdx < NAN_SERVICE_INSTANCE_NUM; szIdx++) {
		if (g_arNanInstance[szIdx].ucInstanceID == ucInstanceId &&
			g_arNanInstance[szIdx].eType ==
			NAN_SERVICE_TYPE_PUBLISH)
			return TRUE;
	}
	return FALSE;
}
uint32_t nanSDFTxDone(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	uint32_t u4Status = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct _NAN_SDF_FRAME_T *prSDF = NULL;

	DBGLOG(NAN, VOC, "Enter, Status:%x\n", rTxDoneStatus);

	if (!prAdapter || !prMsduInfo)
		goto exit;

	if (rTxDoneStatus == WLAN_STATUS_SUCCESS) {
		prSDF = (struct _NAN_SDF_FRAME_T *)prMsduInfo->prPacket;
		prPairingFsm = pairingFsmSearch(prAdapter, prSDF->aucDestAddr);
		if (!prPairingFsm)
			goto exit;
		prPairingFsm->ucTxKeyRetryCounter = 0;
		cnmTimerStopTimer(prAdapter, &prPairingFsm->arKeyExchangeTimer);
		prPairingFsm->fgLocalNIK_sent = TRUE;
		DBGLOG(NAN, ERROR, PREL6" succeed to send NIK");
	} else {
		prSDF = (struct _NAN_SDF_FRAME_T *)prMsduInfo->prPacket;
		prPairingFsm = pairingFsmSearch(prAdapter, prSDF->aucDestAddr);
		if (!prPairingFsm)
			goto exit;
		if (prPairingFsm->ucTxKeyRetryCounter++ <
				KEY_EXCHANGE_RETRY_LIMIT) {
			DBGLOG(NAN, ERROR,
			       PREL6"rTxDoneStatus(%d). re-sending NIK",
			       rTxDoneStatus);
			nanDiscSendSDF(prAdapter, prMsduInfo,
			prMsduInfo->u2FrameLength, nanSDFTxDone,
			prPairingFsm->prStaRec);
		} else {
			DBGLOG(NAN, ERROR,
			       PREL6"retry limit exceeded give up send NIK");
			cnmTimerStopTimer(prAdapter,
			&prPairingFsm->arKeyExchangeTimer);
		}
	}

exit:

	return u4Status;
}
void
nanSDFRetryTimeout(struct ADAPTER *prAdapter, uintptr_t ulParam) {
	struct PAIRING_FSM_INFO *prPairingFsm =
	(struct PAIRING_FSM_INFO *)ulParam;
	DBGLOG(NAN, ERROR, PREL6"Failed to send NIK(retry:%d)",
		prPairingFsm->ucTxKeyRetryCounter);
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
