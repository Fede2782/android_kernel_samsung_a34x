/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#if CFG_SUPPORT_NAN
#ifndef INCLUDE_FROM_NAN
#define INCLUDE_FROM_NAN
#include "precomp.h"
#undef INCLUDE_FROM_NAN
#endif
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
#include "wpa_supp/FourWayHandShake.h"
#include "wpa_supp/src/ap/wpa_auth_glue.h"
#include "precomp.h"
#include "nan_pairing.h"
#include "nan_sec.h"
struct wpa_ptk g_ptk[NAN_MAX_NDP_SESSIONS];
struct wpa_sm_ctx g_rNanWpaPairingSmCtx;
uint64_t g_publisherNonce;
uint8_t g_last_matched_report_npba_dialogTok;
uint8_t g_last_bootstrapReq_npba_dialogTok;
static const char * const
		apucDebugParingMgmtState[NAN_PAIRING_MGMT_STATE_NUM] = {
	"NAN_PAIRING_INIT",
	"NAN_PAIRING_BOOTSTRAPPING",
	"NAN_PAIRING_BOOTSTRAPPING_DONE",
	"NAN_PAIRING_SETUP",
	"NAN_PAIRING_PAIRED",
	"NAN_PAIRING_PAIRED_VERIFICATION",
	"NAN_PAIRING_INVALID",
	"NAN_PAIRING_CANCEL",
};
uint32_t nanPairingPeerNikReceived(struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf, struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, ERROR, "[pairing-Ex] enter %s()\n", __func__);
	prPairingFsm->fgPeerNIK_received = TRUE;

	if (mtk_cfg80211_vendor_event_nan_pairing_confirm(prAdapter,
		pcuEvtBuf) != 0) {
		DBGLOG(NAN, ERROR,
			"[pairing-Ex] failed - %s():%d\n", __func__, __LINE__);
		return WLAN_STATUS_FAILURE;
	}
	return WLAN_STATUS_SUCCESS;
}
uint32_t nanPasnRxAuthFrame(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct NanPairingPASNMsg pasnframe;
	uint32_t nan_pairing_request_type = NAN_PAIRING_SETUP_REQ_T;
	uint8_t enable_pairing_cache = 1;
	struct NanIdentityResolutionAttribute nira = {0};

	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;
	if (prAuthFrame == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] %s prAuthFrame=%p\n",
			__func__, prAuthFrame);
		rRetStatus = WLAN_STATUS_FAILURE;
		goto out;
	} else {
		uint32_t ret = WLAN_STATUS_SUCCESS;
		/* only M1/M2 include NAN IE */
		if (prAuthFrame->u2AuthTransSeqNo == PASN_M1_AUTH_SEQ ||
			prAuthFrame->u2AuthTransSeqNo == PASN_M2_AUTH_SEQ) {
			ret = nanPairingProcessAuthFrame(prAdapter,
				prSwRfb,
				&nan_pairing_request_type,
				&enable_pairing_cache,
				nira.nonce,
				nira.tag);
		}
		if (ret != WLAN_STATUS_SUCCESS) {
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			DBGLOG(NAN, ERROR,
			"[pairing-pasn]%s:request_type =%u\n",
			__func__, nan_pairing_request_type);
		}
	}
	DBGLOG(NAN, ERROR, PAIRING_DBGM1,
		prAuthFrame->u2AuthTransSeqNo, __func__);
	if (prAuthFrame->u2AuthTransSeqNo == PASN_M1_AUTH_SEQ) {
		prPairingFsm =
			pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
		if (prPairingFsm == NULL &&
			nan_pairing_request_type == NAN_PAIRING_SETUP_REQ_T) {
			DBGLOG(NAN, ERROR, PAIRING_DBGM2,
				__func__, __LINE__);
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			kalIndicateNanPairingRequest(prAdapter,
				prAdapter->prGlueInfo, prSwRfb, ucBssIdx);
		}
	} else if (prAuthFrame->u2AuthTransSeqNo == PASN_M2_AUTH_SEQ) {
		prPairingFsm =
			pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
		if (prPairingFsm == NULL ||
			prPairingFsm->ucPairingType != NAN_PAIRING_REQUESTOR) {
			DBGLOG(NAN, ERROR, PAIRING_DBGM3, __func__);
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
			pasnframe.framesize = prSwRfb->u2PacketLen;
			if (sizeof(pasnframe.PASN_FRAME) <
			    pasnframe.framesize) {
				DBGLOG(NAN, ERROR, PAIRING_DBGM4,
					pasnframe.framesize);
				rRetStatus = WLAN_STATUS_RESOURCES;
				goto out;
			} else {
				memcpy(pasnframe.PASN_FRAME, prSwRfb->pvHeader,
					pasnframe.framesize);
			}
			/* TODO : from Standard, M2 is not indicated to Host.
			 * (M3 tx with Confirm is indicated instead)
			 * directly report it to wpa_supplicant.
			 */
			nanPasnRequestM2_Rx(prAdapter, &pasnframe);
		}
	} else if (prAuthFrame->u2AuthTransSeqNo == PASN_M3_AUTH_SEQ) {
		prPairingFsm =
			pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
		if (prPairingFsm == NULL ||
			prPairingFsm->ucPairingType != NAN_PAIRING_RESPONDER) {
			DBGLOG(NAN, ERROR, PAIRING_DBGM5,
				__func__, __LINE__);
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
			pasnframe.framesize = prSwRfb->u2PacketLen;
			if (sizeof(pasnframe.PASN_FRAME) <
				pasnframe.framesize) {
				DBGLOG(NAN, ERROR, PAIRING_DBGM6,
				pasnframe.framesize);
				rRetStatus = WLAN_STATUS_RESOURCES;
				goto out;
			} else {
				memcpy(pasnframe.PASN_FRAME, prSwRfb->pvHeader,
					pasnframe.framesize);
			}
			/* TODO : from Standard, M3 rx with Confirm is
			* indicated to Host
			* directly report it to wpa_supplicant.
			*/
			nanPasnRequestM3_Rx(prAdapter, &pasnframe);
		}
	} else {
		DBGLOG(NAN, ERROR, PAIRING_DBGM7, __func__, __LINE__);
		rRetStatus = WLAN_STATUS_FAILURE;
		goto out;
	}
	DBGLOG(NAN, ERROR, "[pairing-pasn] trace %s():%d (rRetStatus=%u)\n",
		__func__, __LINE__, rRetStatus);
out:
	return rRetStatus;
}
uint32_t nanPasnRequestM1_Tx(struct ADAPTER *prAdapter,
	struct NAN_PASN_START_PARAM *prPasnStartParam)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	uint8_t *pucLocalNMI = NULL;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint8_t *pucPeerAddr = prPasnStartParam->peer_mac_addr;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPasnStartParam->nan_pairing_request_type ==
		NAN_PAIRING_VERIFICATION_REQ_T) {
	DBGLOG(NAN, INFO,
	"[%s][pairing-verification] debug peer NMI="MACSTR_A"\n",
	__func__, pucPeerAddr[0], pucPeerAddr[1], pucPeerAddr[2],
		pucPeerAddr[3], pucPeerAddr[4], pucPeerAddr[5]);
	}

	prPairingFsm =
		pairingFsmSearch(prAdapter, prPasnStartParam->peer_mac_addr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR,
		"[pairing-pasn](initiator) %s() failed to find PairingFsm\n",
		__func__);
		rRetStatus = WLAN_STATUS_FAILURE;
		return rRetStatus;
	}
	prPairingFsm->ucPairingType = NAN_PAIRING_REQUESTOR;

	pucLocalNMI = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	memcpy(prPasnStartParam->own_mac_addr, pucLocalNMI, MAC_ADDR_LEN);
	rRetStatus = nanDevGetClusterId(prAdapter, prPasnStartParam->bssid);
	DBGLOG(NAN, INFO,
	"[%s] Enter line:%d rRetStatus=%d prPairingFsm->ePairingState=%d prPasnStartParam->nan_pairing_request_type=%d\n",
	__func__, __LINE__, rRetStatus, prPairingFsm->ePairingState,
	prPasnStartParam->nan_pairing_request_type);
	if (rRetStatus == WLAN_STATUS_SUCCESS &&
		(prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE)
		&& prPasnStartParam->nan_pairing_request_type ==
		NAN_PAIRING_SETUP_REQ_T){
	DBGLOG(NAN, INFO, "[%s] Enter line %d\n", __func__, __LINE__);
		DBGLOG(NAN, INFO,
		"[pairing-pasn](initiator)[%s] Trigger pairing setup\n",
		__func__);
		pairingFsmSteps(prAdapter, prPairingFsm, NAN_PAIRING_SETUP);
		prPasnStartParam->nan_ie_len = prPairingFsm->nan_ie_len;
		memcpy(prPasnStartParam->nan_ie, prPairingFsm->nan_ie,
			prPairingFsm->nan_ie_len);

		kalRequestNanPasnStart(prAdapter,
			(void *)prPasnStartParam, ucBssIdx);
	} else if (rRetStatus == WLAN_STATUS_SUCCESS &&
		(prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE)
		&& prPasnStartParam->nan_pairing_request_type ==
		NAN_PAIRING_VERIFICATION_REQ_T) {
	DBGLOG(NAN, INFO, "[%s] Enter line %d\n", __func__, __LINE__);
		DBGLOG(NAN, INFO,
		"[pairing-verification](initiator)[%s] Trigger verification\n",
		__func__);
		pairingFsmSteps(prAdapter, prPairingFsm,
		NAN_PAIRING_PAIRED_VERIFICATION);
		prPasnStartParam->nan_ie_len = prPairingFsm->nan_ie_len;
		memcpy(prPasnStartParam->nan_ie, prPairingFsm->nan_ie,
			prPairingFsm->nan_ie_len);


		nanPairingCalCustomPMKID(
			*(uint64_t *)prPairingFsm->aucNonce,
			prPairingFsm->u8Tag,
			(uint8_t *)prPasnStartParam->pmkid);

		kalRequestNanPasnStart(prAdapter,
			(void *)prPasnStartParam, ucBssIdx);
	} else {
		DBGLOG(NAN, INFO, "[%s] Enter line %d\n", __func__, __LINE__);
		DBGLOG(NAN, ERROR, PAIRING_DBGM8);
	}

	return rRetStatus;
}


uint32_t nanPasnRequestM1_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *prPasnframe)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint32_t framesize = prPasnframe->framesize;

	DBGLOG(NAN, ERROR, "[pairing-pasn] enter %s\n", __func__);
	if (framesize == 0)
		return WLAN_STATUS_INVALID_PACKET;
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prPasnframe->PASN_FRAME;

	prPairingFsm = pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] prPairingFsm is NULL ! %s\n",
		__func__);
		return WLAN_STATUS_FAILURE;
	}

	prPairingFsm->ucPairingType = NAN_PAIRING_RESPONDER;
	if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING &&
		prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE) {
		DBGLOG(NAN, INFO, PAIRING_DBGM9, __func__);
		pairingFsmSteps(prAdapter, prPairingFsm, NAN_PAIRING_SETUP);

		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
				prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}

		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);
	} else if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION &&
		prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE) {
		DBGLOG(NAN, INFO, PAIRING_DBGM9, __func__);
		pairingFsmSteps(prAdapter, prPairingFsm,
		NAN_PAIRING_PAIRED_VERIFICATION);

		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
				prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}
		nanPairingCalCustomPMKID(*(uint64_t *)prPairingFsm->aucNonce,
			prPairingFsm->u8Tag,
			(uint8_t *)prPasnframe->localhostconfig.
			hostM1param.pmkid);

		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);
	}
	DBGLOG(NAN, ERROR, "[pairing-pasn] leave %s():%d\n",
		__func__, __LINE__);

	return rRetStatus;
}

uint32_t nanPasnRequestM2_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *prPasnframe) {
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint32_t framesize = prPasnframe->framesize;

	DBGLOG(NAN, ERROR, "[pairing-pasn] enter %s\n", __func__);
	if (framesize == 0)
		return WLAN_STATUS_INVALID_PACKET;
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prPasnframe->PASN_FRAME;

	prPairingFsm = pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR,
		"[pairing-pasn] prPairingFsm is NULL ! %s\n",
		__func__);
		return WLAN_STATUS_FAILURE;
	}

/*	prPairingFsm->ucPairingType = NAN_PAIRING_REQUESTOR;
*	--> it should already iniaiated to NAN_PAIRING_REQUESTOR
*	before sending M1.
*/
	DBGLOG(NAN, ERROR,
	"[pairing-pasn] trace %s():%d ucPairingType=%u ePairingState=%u\n",
	__func__, __LINE__, prPairingFsm->ucPairingType,
	prPairingFsm->ePairingState);
	if (prPairingFsm->ePairingState == NAN_PAIRING_SETUP ||
		(prPairingFsm->ePairingState ==
		NAN_PAIRING_PAIRED_VERIFICATION)) {
		DBGLOG(NAN, INFO, PAIRING_DBGM10, __func__);
		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
				prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}
		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);
	}
	DBGLOG(NAN, ERROR, "[pairing-pasn] leave %s():%d\n",
		__func__, __LINE__);

	return rRetStatus;
}

uint32_t nanPasnRequestM3_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *prPasnframe) {
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint32_t framesize = prPasnframe->framesize;

	DBGLOG(NAN, ERROR, "[pairing-pasn] enter %s\n", __func__);
	if (framesize == 0)
		return WLAN_STATUS_INVALID_PACKET;
	prAuthFrame = (struct WLAN_AUTH_FRAME *) prPasnframe->PASN_FRAME;

	prPairingFsm = pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] prPairingFsm is NULL ! %s\n",
			__func__);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(NAN, ERROR, PAIRING_DBGM11, __func__, __LINE__,
		prPairingFsm->ucPairingType, prPairingFsm->ePairingState);
	if (prPairingFsm->ePairingState == NAN_PAIRING_SETUP ||
		(prPairingFsm->ePairingState ==
		NAN_PAIRING_PAIRED_VERIFICATION)) {
		DBGLOG(NAN, INFO, PAIRING_DBGM12, __func__);
		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
			prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}
		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);

		/* for responder , key install happens
		 * at reception of PASN-M3 confirm
		 */
		pairingFsmSteps(prAdapter, prPairingFsm, NAN_PAIRING_PAIRED);

	}
	DBGLOG(NAN, ERROR, "[pairing-pasn] leave %s() ret=%u\n",
		__func__, rRetStatus);

	return rRetStatus;
}

uint32_t nanPasnSetKey(struct ADAPTER *prAdapter,
	struct nan_pairing_keys_t *key_data) {
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return NAN_PAIRING_INVALID;
	}
	prPairingFsm = pairingFsmSearch(prAdapter, key_data->peer_mac_addr);
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "[%s] prPairingFsm error\n", __func__);
		return NAN_PAIRING_INVALID;
	}
	if (prPairingFsm->ePairingState != NAN_PAIRING_SETUP &&
	prPairingFsm->ePairingState != NAN_PAIRING_PAIRED_VERIFICATION) {
		DBGLOG(NAN, ERROR, "[%s] ePairingState error\n", __func__);
		return NAN_PAIRING_INVALID;
	}

	switch (key_data->selected_pairwise_cipher) {
	case WPA_CIPHER_CCMP:
		prPairingFsm->u4SelCipherType =
			NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
		break;
	case WPA_CIPHER_GCMP_256:
		prPairingFsm->u4SelCipherType =
			NAN_CIPHER_SUITE_ID_NCS_PK_PASN_256;
		break;
	default:
		DBGLOG(NAN, ERROR,
		"[%s] selected_pairwise_cipher error\n", __func__);
		return NAN_PAIRING_INVALID;
	}
	kalMemCopy(prPairingFsm->prPtk->tk, key_data->nm_tk,
		NAN_NM_TK_MAX_LEN);
	prPairingFsm->prPtk->tk_len = key_data->nm_tk_len;
#if 1   /* NM-TK */
	DBGLOG(NAN, INFO, "prPairingFsm->prPtk->tk_len=%zu\n",
	prPairingFsm->prPtk->tk_len);
	nanUtilDump(prAdapter, "prPairingFsm->prPtk->tk",
		prPairingFsm->prPtk->tk, prPairingFsm->prPtk->tk_len);
#endif

	kalMemCopy(prPairingFsm->prPtk->kek, key_data->nm_kek,
		NAN_NM_KEK_MAX_LEN);
	prPairingFsm->prPtk->kek_len = key_data->nm_kek_len;
#if 1   /* NM-KEK */
	DBGLOG(NAN, INFO, "prPairingFsm->prPtk->kek_len=%zu\n",
	prPairingFsm->prPtk->kek_len);
	nanUtilDump(prAdapter, "prPairingFsm->prPtk->kek",
		prPairingFsm->prPtk->kek, prPairingFsm->prPtk->kek_len);
#endif

	kalMemCopy(prPairingFsm->prPtk->kck, key_data->nm_kck,
		NAN_NM_KCK_MAX_LEN);
	prPairingFsm->prPtk->kck_len = key_data->nm_kck_len;
#if 1   /* NM-KCK */
	DBGLOG(NAN, INFO, "prPairingFsm->prPtk->kck_len=%zu\n",
	prPairingFsm->prPtk->kck_len);
	nanUtilDump(prAdapter, "prPairingFsm->prPtk->kck",
		prPairingFsm->prPtk->kck, prPairingFsm->prPtk->kck_len);
#endif
	kalMemCopy(prPairingFsm->prPtk->kdk, key_data->nm_kdk,
		NAN_NM_KDK_MAX_LEN);
	prPairingFsm->prPtk->kdk_len = key_data->nm_kdk_len;
#if 1   /* NM-KDK */
	DBGLOG(NAN, INFO, "prPairingFsm->prPtk->kdk_len=%zu\n",
	prPairingFsm->prPtk->kdk_len);
	nanUtilDump(prAdapter, "prPairingFsm->prPtk->kdk",
		prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);
#endif
	kalMemCopy(prPairingFsm->npk, key_data->npk,
		key_data->npk_len);
	prPairingFsm->npk_len = key_data->npk_len;
#if 1   /* NPK */
	DBGLOG(NAN, INFO, "prPairingFsm->npk_len=%zu\n",
	prPairingFsm->npk_len);
	nanUtilDump(prAdapter, "prPairingFsm->npk",
		prPairingFsm->npk, prPairingFsm->npk_len);
#endif


	DBGLOG(NAN, ERROR, "nanPasnSetKey() done.\n");

	return WLAN_STATUS_SUCCESS;
}

struct PAIRING_FSM_INFO *
pairingFsmAlloc(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "prAdapter NULL\n");
		return NULL;
	}
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm == NULL) {
			DBGLOG(NAN, ERROR, "Alloc failed\n");
			return NULL;
		}
		if (!prPairingFsm->fgIsInUse) {
			DBGLOG(NAN, INFO, "Alloc PairingFSM\n");
			prPairingFsm->ucIndex = szIdx;
			pairingFsmInit(prAdapter, prPairingFsm);
			return prPairingFsm;
		}
	}
	DBGLOG(NAN, ERROR, "[%s] No available PairingFSM\n", __func__);
	return NULL;
}
void
pairingFsmInit(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	prPairingFsm->fgIsInUse = TRUE;
	prPairingFsm->ePairingState = NAN_PAIRING_INIT;
	prPairingFsm->prPtk = NULL;
	pairingDeriveNirNonce(prPairingFsm);
	pairingDeriveNik(prAdapter, prPairingFsm);
	nanUtilDump(prAdapter, "Paring nik",
		prPairingFsm->aucNik, NAN_NIK_LEN);
	nanUtilDump(prAdapter, "Paring nmi",
		prAdapter->rDataPathInfo.aucLocalNMIAddr, MAC_ADDR_LEN);
	nanUtilDump(prAdapter, "Paring nonce",
		prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
	pairingDeriveNirTag(prAdapter, prPairingFsm->aucNik, NAN_NIK_LEN,
		prAdapter->rDataPathInfo.aucLocalNMIAddr,
		prPairingFsm->aucNonce,
		&prPairingFsm->u8Tag);
	prPairingFsm->fgPeerNIK_received = FALSE;
	prPairingFsm->fgLocalNIK_sent = FALSE;
	prPairingFsm->fgM3Acked = FALSE;
	init_waitqueue_head(&prPairingFsm->confirm_ack_wq);
	prPairingFsm->ucTxKeyRetryCounter = 0;
}
struct PAIRING_FSM_INFO *
pairingFsmSearch(struct ADAPTER *prAdapter, uint8_t *pucPeerAddr)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return NULL;
	}
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm->fgIsInUse &&
			(prPairingFsm->prStaRec != NULL)) {
			if (EQUAL_MAC_ADDR(prPairingFsm->prStaRec->aucMacAddr,
				pucPeerAddr)) {
				DBGLOG(NAN, INFO,
				"[%s],idx=%zu,addr="MACSTR_A"\n",
				__func__, szIdx,
				pucPeerAddr[0], pucPeerAddr[1], pucPeerAddr[2],
				pucPeerAddr[3], pucPeerAddr[4], pucPeerAddr[5]);
				return prPairingFsm;
			}
		}
	}
	DBGLOG(NAN, ERROR, "[%s] return NULL\n", __func__);
	return NULL;
}

void
pairingFsmSteps(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			enum NAN_PAIRING_STATE eNextState)
{
	uint8_t ucBssIndex = 0;
	uint8_t *pucLocalNMI = NULL;
	uint8_t *pucPeerNMI = NULL;
	uint8_t pmk[NAN_NPK_LEN] = {0};
	u8 pmkid[16] = {0};
	u8 tag[8] = {0};
	u8 nonce[8] = {0};
	size_t szPairingFsmIdx = 0;
	size_t szIdx = 0;
	enum NAN_PAIRING_STATE eState = 0;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return;
	}
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] Pairing mgmt STATE_%d: [%s] -> [%s]\n",
	__func__, prPairingFsm->ucIndex,
	apucDebugParingMgmtState[(uint8_t)prPairingFsm->ePairingState],
	apucDebugParingMgmtState[(uint8_t)eNextState]);

	if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING)
		eState = pairingFsmNextState(prPairingFsm);
	else if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION)
		eState = pairingFsmNextStateForVerification(prPairingFsm);

	if (eNextState != eState) {
		DBGLOG(NAN, ERROR,
		"[%s] Steps Error, Go to NAN_PAIRING_INIT\n",
		__func__);
		/* GO TO INIT for error handling */
		eNextState = NAN_PAIRING_INIT;
	}
	pucPeerNMI = prPairingFsm->prStaRec->aucMacAddr;
	DBGLOG(NAN, INFO, "[%s] Peer=%x:%x:%x:%x:%x:%x\n", __func__,
		pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
		pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);
	switch (eNextState) {
	/* error handling and go init */
	case NAN_PAIRING_INIT:
		/* TODO: reset pairing setting */
		pairingFsmFree(prAdapter, prPairingFsm);
		break;
	/* 1. recevie followup command with bootstrapping info */
	/* 2. receive followup with bootstrapping request */
	case NAN_PAIRING_BOOTSTRAPPING:
	/* TODO: set Followup to carry NPBA and set Bootstrapping_request */
#if 0
	if (prPairingFsm->eBootStrapMethod == NAN_BOOTSTRAPPING_HANDSHAKE_SKIP)
		prPairingFsm->fgIsBootStrapNeedHandShake = FALSE;
	else
		prPairingFsm->fgIsBootStrapNeedHandShake = TRUE;
#endif
		break;
	/* 1. receive bootstrapping password */
	/* 2. receive bootstrapping response */
	case NAN_PAIRING_BOOTSTRAPPING_DONE:
		/* TODO: idle to wait pairing_start commmand */
		/* TODO: derive NIK and NIR-TAG */

		/* station record creation for PMF on 2.4G */
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, BAND_2G4);
		pucLocalNMI = prAdapter->rDataPathInfo.aucLocalNMIAddr;
		/* 2G station record could be created at bootstrapping
		 * while 5G station record needs to be created here
		 */
		if (prPairingFsm->prStaRec == NULL) {
		prPairingFsm->prStaRec =
			cnmStaRecAlloc(prAdapter,
			STA_TYPE_NAN, ucBssIndex, pucPeerNMI);
		}
		if (prPairingFsm->prStaRec)
			atomic_set(
			&prPairingFsm->prStaRec->NanRefCount,
			1);

		/* station record creation for PMF on 5G */
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, BAND_5G);
		prPairingFsm->prStaRec5G =
			cnmStaRecAlloc(prAdapter,
			STA_TYPE_NAN, ucBssIndex, pucPeerNMI);
		if (prPairingFsm->prStaRec5G)
			atomic_set(
			&prPairingFsm->prStaRec5G->NanRefCount,
			1);
		DBGLOG(NAN, INFO, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		pucLocalNMI[0], pucLocalNMI[1], pucLocalNMI[2],
		pucLocalNMI[3], pucLocalNMI[4], pucLocalNMI[5]);
		DBGLOG(NAN, INFO, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
		pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);
		cnmStaRecChangeState(prAdapter,
			prPairingFsm->prStaRec, STA_STATE_1);
		cnmStaRecChangeState(prAdapter,
			prPairingFsm->prStaRec5G, STA_STATE_1);
		szPairingFsmIdx = 0;
		for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
			if (prPairingFsm ==
				&prAdapter->rWifiVar.arPairingFsmInfo[szIdx]) {
				DBGLOG(NAN, ERROR,
				PREL3"found szPairingFsmIdx=%zu\n",
				szPairingFsmIdx);
				szPairingFsmIdx = szIdx;
				break;
			}
		}
		prPairingFsm->prPtk = &g_ptk[szPairingFsmIdx];
		break;
	/* 1. receive pairing_start command to trigger NAN pairing */
	/* 2. receive PASN_Auth1 */
	case NAN_PAIRING_SETUP:
		/* TODO: trigger to start PASN TRX */
		/* Pairing Initiator: start TX PARN Auth1 */
		/* Pairing Responder: wait for PASN Auth1 */
		prPairingFsm->u4SelCipherType =
			NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
		pairingComposeNanIeForSetup(prAdapter, prPairingFsm);
		break;
	/* 1. get PASN results */
	case NAN_PAIRING_PAIRED:
		/* TODO: key cache */
		DBGLOG(NAN, INFO, "[%s] PAIRED\n", __func__);
		prAdapter->rWifiVar.fgNoPmf = FALSE;
		pairingDeriveNdPmk(prAdapter, prPairingFsm);

		kalMemCpyS(tag, NAN_NIR_TAG_LEN,
			&prPairingFsm->u8Tag, NAN_NIR_TAG_LEN);
		nanUtilDump(prAdapter, "Paring tag",
			tag, NAN_NIR_TAG_LEN);
		kalMemCpyS(nonce, NAN_NIR_NONCE_LEN,
			prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
		nanUtilDump(prAdapter, "Paring nonce",
			nonce, NAN_NIR_NONCE_LEN);
		kalMemCpyS(pmkid, NAN_NIR_NONCE_LEN,
			nonce, NAN_NIR_NONCE_LEN);
		kalMemCpyS(pmkid + NAN_NIR_NONCE_LEN,
			NAN_NIR_TAG_LEN, tag, NAN_NIR_TAG_LEN);
		nanUtilDump(prAdapter, "Paring pmkid",
			pmkid, NAN_NPKID_LEN);

		DBGLOG(NAN, INFO,
		PREL6"[%s] local NIK dump s>>>\n", __func__);
		nanUtilDump(prAdapter,
		PREL6" local NIK:", prPairingFsm->aucNik, NAN_NIK_LEN);
		DBGLOG(NAN, INFO, PREL6"[%s] local NIK dump <<<e\n",
		__func__);

		DBGLOG(NAN, INFO,
		PREL3"[%s] Install NM-TK for peer["MACSTR_A"]\n", __func__,
		pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
		pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);
		nanPairingInstallTk(prPairingFsm->prPtk,
			prPairingFsm->u4SelCipherType, prPairingFsm->prStaRec);
		nanPairingInstallTk(prPairingFsm->prPtk,
			prPairingFsm->u4SelCipherType,
			prPairingFsm->prStaRec5G);

#if 0 /* Dump key for RDUT */
		nanUtilDump(prAdapter, "Pairing-TK",
			prPairingFsm->prPtk->tk, prPairingFsm->prPtk->tk_len);
		nanUtilDump(prAdapter, "Pairing-KCK",
			prPairingFsm->prPtk->kck, prPairingFsm->prPtk->kck_len);
		nanUtilDump(prAdapter, "Pairing-KDK",
			prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);
		nanUtilDump(prAdapter, "Pairing-KEK",
			prPairingFsm->prPtk->kek, prPairingFsm->prPtk->kek_len);
#endif
		if (prPairingFsm->u2BootstrapMethod !=
		    NAN_BOOTSTRAPPING_OPPORTUNISTIC) {
			prPairingFsm->rNpksaCache.isPmkCached = TRUE;
			kalMemCpyS(prPairingFsm->rNpksaCache.pmkid,
			NAN_NPKID_LEN, pmkid, NAN_NPKID_LEN);
			kalMemCpyS(prPairingFsm->rNpksaCache.pmk,
			NAN_NPK_LEN, pmk, NAN_NPK_LEN);
			prPairingFsm->rNpksaCache.pmk_len = NAN_NPK_LEN;
#if 0
			nanUtilDump(prAdapter, "pairing-PMK",
				prPairingFsm->key_info.body.pmk_info.pmk,
				prPairingFsm->key_info.body.pmk_info.pmk_len);
			nanUtilDump(prAdapter, "cache Pairing-PMK",
				prPairingFsm->rNpksaCache.pmk,
				prPairingFsm->rNpksaCache.pmk_len);
			nanUtilDump(prAdapter, "cache Pairing-PMKID",
				prPairingFsm->rNpksaCache.pmkid, NAN_NPKID_LEN);
#endif
		}
		pairingPrintKeyInfo(prAdapter, prPairingFsm);
		break;
	/* 1. receive verification command */
	/* 2. receive PASN_Auth1 */
	case NAN_PAIRING_PAIRED_VERIFICATION:
		/* TODO: trigger to start PASN TRX */
		/* Pairing Initiator: start TX PASN Auth1 */
		/* Pairing Responder: wait for PASN Auth1 */
		pairingComposeNanIeForVerification(prAdapter, prPairingFsm);
		break;
	default:
		break;
	}
	prPairingFsm->ePairingState = eNextState;
}
uint8_t
pairingFsmCurrState(struct ADAPTER *prAdapter, uint8_t *pucPeerAddr)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return NAN_PAIRING_INVALID;
	}
	prPairingFsm = pairingFsmSearch(prAdapter, pucPeerAddr);
	if (prPairingFsm)
		return prPairingFsm->ePairingState;
	else
		return NAN_PAIRING_INVALID;
}
uint8_t
pairingFsmNextState(IN struct PAIRING_FSM_INFO *prPairingFsm)
{
	enum NAN_PAIRING_STATE eNextState = NAN_PAIRING_INIT;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return NAN_PAIRING_INVALID;
	}
	switch (prPairingFsm->ePairingState) {
	case NAN_PAIRING_INIT:
		eNextState = NAN_PAIRING_BOOTSTRAPPING;
		break;
	case NAN_PAIRING_BOOTSTRAPPING:
		eNextState = NAN_PAIRING_BOOTSTRAPPING_DONE;
		break;
	case NAN_PAIRING_BOOTSTRAPPING_DONE:
		eNextState = NAN_PAIRING_SETUP;
		break;
	case NAN_PAIRING_SETUP:
		eNextState = NAN_PAIRING_PAIRED;
		break;
#if 0
	case NAN_PAIRING_PAIRED:
		eNextState = NAN_PAIRING_PAIRED_VERIFICATION;
		break;
	case NAN_PAIRING_PAIRED_VERIFICATION:
		eNextState = NAN_PAIRING_PAIRED;
		break;
#endif
	default:
		eNextState = NAN_PAIRING_INIT;
		break;
	}
	return eNextState;
}

uint8_t
pairingFsmNextStateForVerification(IN struct PAIRING_FSM_INFO *prPairingFsm)
{
	enum NAN_PAIRING_STATE eNextState = NAN_PAIRING_INIT;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return NAN_PAIRING_INVALID;
	}
	switch (prPairingFsm->ePairingState) {
	case NAN_PAIRING_INIT:
		eNextState = NAN_PAIRING_BOOTSTRAPPING_DONE;
		break;
	case NAN_PAIRING_BOOTSTRAPPING_DONE:
		eNextState = NAN_PAIRING_PAIRED_VERIFICATION;
		break;
	case NAN_PAIRING_PAIRED_VERIFICATION:
		eNextState = NAN_PAIRING_PAIRED;
		break;
	default:
		eNextState = NAN_PAIRING_INIT;
		break;
	}
	return eNextState;
}
void
pairingFsmSetBootStrappingMethod(IN struct PAIRING_FSM_INFO *prPairingFsm,
	uint16_t u2BootstrapMethod)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	prPairingFsm->u2BootstrapMethod = u2BootstrapMethod;
	if (prPairingFsm->u2BootstrapMethod)
		prPairingFsm->fgIsBootStrapNeedHandShake = TRUE;
	else
		prPairingFsm->fgIsBootStrapNeedHandShake = FALSE;
}
void
pairingFsmSetup(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			uint16_t u2BootstrapMethod,
			uint8_t ucCacheEnable, uint8_t ucPubID, uint8_t ucSubID,
			uint8_t ucIsPub, uint16_t u2Ttl)
{
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return;
	}
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[%s] NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] IsPub=%d, PubId=%d, SubId=%d\n",
		__func__, ucIsPub, ucPubID, ucSubID);
	prPairingFsm->fgCachingEnable = ucCacheEnable;
	prPairingFsm->ucPublishID = ucPubID;
	prPairingFsm->ucSubscribeID = ucSubID;
	prPairingFsm->ucIsPub = ucIsPub;
	pairingFsmSetBootStrappingMethod(prPairingFsm, u2BootstrapMethod);
	prPairingFsm->u2Ttl = u2Ttl;
	if (prPairingFsm->u2Ttl > 0) {
		cnmTimerStopTimer(prAdapter,
			&prPairingFsm->rTtlTimer);
		/*ToDo:Init Timer to check get
		 * Auth Txdone avoid sta_rec not clear
		 */
		cnmTimerInitTimer(prAdapter,
			&prPairingFsm->rTtlTimer,
			(PFN_MGMT_TIMEOUT_FUNC)
			pairingServiceLifetimeout,
			(unsigned long) prPairingFsm);
		cnmTimerStartTimer(prAdapter,
			&prPairingFsm->rTtlTimer,
			prPairingFsm->u2Ttl);
	}
}
void
pairingFsmFree(IN struct ADAPTER *prAdapter,
			IN struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] Adapter NULL\n", __func__);
		return;
	}
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[%s] NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] Enter, IDX_%d\n",
		__func__, prPairingFsm->ucIndex);
	if (prPairingFsm->fgIsInUse) {
		prPairingFsm->fgIsInUse = FALSE;
		cnmStaRecFree(prAdapter, prPairingFsm->prStaRec);
		cnmStaRecFree(prAdapter, prPairingFsm->prStaRec5G);
		prPairingFsm->prStaRec = NULL;
		prPairingFsm->prStaRec5G = NULL;
		prPairingFsm->ucPublishID = 0;
		prPairingFsm->ucSubscribeID = 0;
		prPairingFsm->ePairingState = NAN_PAIRING_INIT;
		prPairingFsm->fgCachingEnable = FALSE;
		prPairingFsm->prPtk = NULL;
		prPairingFsm->wpa_passphrase = NULL;
		cnmTimerStopTimer(prAdapter, &prPairingFsm->rTtlTimer);
		cnmTimerStopTimer(prAdapter, &prPairingFsm->rNikTimer);
	}
}
void
pairingFsmUninit(IN struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm->fgIsInUse)
			pairingFsmFree(prAdapter, prPairingFsm);
	}
}
void
pairingFsmCancelRequest(IN struct ADAPTER *prAdapter,
			uint8_t ucInstanceId, uint8_t ucIsPub)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return;
	}
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm->fgIsInUse) {
			if (ucIsPub) {
				if (prPairingFsm->ucIsPub &&
					prPairingFsm->ucPublishID ==
					ucInstanceId) {
					DBGLOG(NAN, INFO,
					"[%s] PairingFSM %zu cancel, Pub=1, InsId=%d\n",
					__func__,
					szIdx,
					ucInstanceId);
					pairingFsmFree(prAdapter, prPairingFsm);
				}
			} else {
				if (!prPairingFsm->ucIsPub &&
					prPairingFsm->ucSubscribeID ==
					ucInstanceId) {
					DBGLOG(NAN, INFO,
					"[%s] PairingFSM %zu cancel, Sub=1, InsId=%d\n",
					__func__,
					szIdx,
					ucInstanceId);
					pairingFsmFree(prAdapter, prPairingFsm);
				}
			}
		}
	}
}
uint32_t
nanCmdPairingSetupReq(IN struct ADAPTER *prAdapter,
			struct NanTransmitFollowupRequest *msg)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}
	prPairingFsm = pairingFsmSearch(prAdapter, msg->addr);
	nanUtilDump(prAdapter, "NAN_PAIRING_ADDR", msg->addr, 6);
	if (prPairingFsm) {
		DBGLOG(NAN, INFO,
		"[%s] pairing_state = %u, verification=%u, pairing_type=%u\n",
		__func__,
		prPairingFsm->ePairingState,
		msg->pairing_verification,
		msg->pairing_type);
		/*very important*/
		prPairingFsm->ucPairingType = msg->pairing_type;
		if (prPairingFsm->ePairingState ==
		    NAN_PAIRING_BOOTSTRAPPING_DONE) {
			/* Trigger pairing setup */
			DBGLOG(NAN, INFO,
			"[%s] Trigger pairing setup\n", __func__);
			pairingFsmSteps(prAdapter,
			prPairingFsm, NAN_PAIRING_SETUP);
			pairingNotifyPasn(prAdapter, prPairingFsm);
		} else if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			msg->pairing_verification == TRUE &&
			(msg->pairing_type == NAN_PAIRING_RESPONDER ||
			prPairingFsm->fgIsPaired)) {
			/* Trigger pairing verification */
			DBGLOG(NAN, INFO,
			"[%s] Trigger pairing verification\n", __func__);
			pairingFsmSteps(prAdapter,
			prPairingFsm, NAN_PAIRING_PAIRED_VERIFICATION);
			pairingNotifyPasn(prAdapter, prPairingFsm);
		} else {
			DBGLOG(NAN, ERROR, "[%s] INVALID state\n", __func__);
			pairingFsmSteps(prAdapter,
			prPairingFsm, NAN_PAIRING_INVALID);
		}
	}
	return WLAN_STATUS_SUCCESS;
}
uint32_t
nanCmdBootstrapPwdSetup(IN struct ADAPTER *prAdapter,
			struct NanBootstrapPassword *msg)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
#if 0
	prPairingFsm = pairingFsmSearchByServiceName(prAdapter,
		msg->service_name, msg->service_name_len);
#else
	/* FIXME: Should find PairingFSM by service name */
	prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[0];
#endif
	if (prPairingFsm) {
		if (prPairingFsm->ePairingState ==
		    NAN_PAIRING_BOOTSTRAPPING_DONE) {
			/* TODO:
			* forward password and bootstrap method to
			* PASN for SAE
			*/
			prPairingFsm->wpa_passphrase = (char *)msg->password;
			nanUtilDump(prAdapter,
			"NAN BOOTSTRAP PASSEWORD",
			msg->password, msg->password_len);
			DBGLOG(NAN, INFO, "[%s] wpa_passphrase=%s\n",
			__func__, prPairingFsm->wpa_passphrase);
		}
	}
	return WLAN_STATUS_SUCCESS;
}

void nanPairingInstallLocalNik(IN struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	u8 *nan_identity_key) {
	memcpy(prPairingFsm->aucNik, nan_identity_key,
		NAN_IDENTITY_KEY_LEN);
}
void
nanPairingInstallTk(struct wpa_ptk *prPtk,
		uint32_t u4SelCipherType,
		struct STA_RECORD *prStaRec)
{
	uint8_t *pu1Tk = NULL;
	size_t szTkLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Cipher = 0;
	enum wpa_alg alg = WPA_ALG_NONE;

	DBGLOG(NAN, INFO,
	"[%s] Enter, StaIdx:%d, BssIdx:%d, Cipher=%x, WlanIdx=%d\n",
	__func__, prStaRec->ucIndex, prStaRec->ucBssIndex,
	u4SelCipherType, prStaRec->ucWlanIndex);
	prStaRec->rPmfCfg.fgApplyPmf = TRUE;
	pu1Tk = prPtk->tk;
	szTkLen = prPtk->tk_len;
	u4Cipher = u4SelCipherType;
	dumpMemory8(pu1Tk, szTkLen);
	/* TODO_CJ: dynamic chiper, dynamic keyID */
	if (u4Cipher == NAN_CIPHER_SUITE_ID_NCS_PK_PASN_256)
		alg = WPA_ALG_GCMP_256;
	else
		alg = WPA_ALG_CCMP;
	rStatus = nan_sec_wpas_setkey_glue(FALSE, prStaRec->ucBssIndex, alg,
					   prStaRec->aucMacAddr, 0, pu1Tk,
					   szTkLen);
	prPtk->installed = 1;
}
void
pairingComposeNanIeForSetup(IN struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;
	struct _NAN_IE_HDR_T *prNanIeHdr = NULL;
	struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *prAttrDCEA = NULL;
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T *prAttrCSIA = NULL;
	struct _NAN_ATTR_NPBA_T *prAttrNPBA = NULL;
	struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCsid = NULL;
	struct _NAN_ATTR_EXT_CAPABILITIES_T *prExtCap;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "%s, NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] go to PASN\n", __func__);
	/* TODO: compose DCEA, CSIA, NPBA for Pairing Setup */
	/* NAN IE Hdr(len=6) */
	prPairingFsm->nan_ie_len = 0;
	kalMemZero(prPairingFsm->nan_ie, NAN_PASN_IE_LEN);
	prNanIeHdr = (struct _NAN_IE_HDR_T *)(prPairingFsm->nan_ie);
	prNanIeHdr->ucElementId = 0xDD;
	prNanIeHdr->ucTagLen = 25; /* 21(5+8+8) + 4*/
	kalMemCpyS(prNanIeHdr->aucOUI, VENDOR_OUI_LEN, aucOui, VENDOR_OUI_LEN);
	prNanIeHdr->ucOUItype = VENDOR_OUI_TYPE_NAN_SDF;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_IE_HDR_T);
	/* DCEA(len=5): Device Capability Extension Attr */
	prAttrDCEA = (struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrDCEA->ucAttrId = NAN_ATTR_ID_DEVICE_CAPABILITY_EXT;
	prAttrDCEA->u2Length = 2;
	prExtCap =
	(struct _NAN_ATTR_EXT_CAPABILITIES_T *)prAttrDCEA->aucExtCapabilities;
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_PARING_SETUP_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_NPK_NIK_CACHE_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);

	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T);
	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_EXT_CAPABILITIES_T);

	/* CSIA(len=8): Cipher Suite Info Attr */
	prAttrCSIA = (struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrCSIA->ucAttrId = NAN_ATTR_ID_CIPHER_SUITE_INFO;
	prAttrCSIA->u2Length = 5;
	prAttrCSIA->ucCapabilities = 0;
	prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
					(prAttrCSIA->aucCipherSuiteList);
	prCsid->ucPublishID = prPairingFsm->ucPublishID;
	prCsid->ucCipherSuiteID = 1;
	prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
		(prAttrCSIA->aucCipherSuiteList +
		sizeof(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T));
	prCsid->ucPublishID = prPairingFsm->ucPublishID;
	prCsid->ucCipherSuiteID = 7;
	prPairingFsm->nan_ie_len += 8; /* 1+2+1+ 2x2(id:1, id:7) */
	/* NPBA(len=8): Nan Pairing Bootstrapping Attr */
	prAttrNPBA = (struct _NAN_ATTR_NPBA_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrNPBA->ucAttribID = NAN_ATTR_ID_NAN_PAIRING_BOOTSTRAPPING;
	prAttrNPBA->u2Len = 5;
	prAttrNPBA->ucDialogTok = prPairingFsm->ucDialogToken;
	prAttrNPBA->ucTypeStatus = 0;
	prAttrNPBA->ucReasonCode = 0;
	prAttrNPBA->u2BootstapMethod = prPairingFsm->u2BootstrapMethod;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_ATTR_NPBA_T);

	DBGLOG(NAN, INFO, "[%s], NanIeLen=%zu\n",
	__func__, prPairingFsm->nan_ie_len);
	nanUtilDump(prAdapter, "NAN IE Setup",
		prPairingFsm->nan_ie, prPairingFsm->nan_ie_len);
}
void
pairingComposeNanIeForVerification(IN struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;
	struct _NAN_IE_HDR_T *prNanIeHdr = NULL;
	struct _NAN_ATTR_NIRA_T *prAttrNIRA = NULL;
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T *prAttrCSIA = NULL;
	struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCsid = NULL;
	struct _NAN_ATTR_EXT_CAPABILITIES_T *prExtCap;
	struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *prAttrDCEA = NULL;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[%s] NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] go to PASN\n", __func__);
	/* TODO: compose CSIA, NIRA for verification */
	/* NAN IE Hdr(len=6) */
	prPairingFsm->nan_ie_len = 0;
	kalMemZero(prPairingFsm->nan_ie, NAN_PASN_IE_LEN);
	prNanIeHdr = (struct _NAN_IE_HDR_T *)(prPairingFsm->nan_ie);
	prNanIeHdr->ucElementId = 0xDD;
	prNanIeHdr->ucTagLen = 37; /* 31(5+8+20) + 4*/
	kalMemCpyS(prNanIeHdr->aucOUI, VENDOR_OUI_LEN, aucOui, VENDOR_OUI_LEN);
	prNanIeHdr->ucOUItype = VENDOR_OUI_TYPE_NAN_SDF;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_IE_HDR_T);

	/* DCEA(len=5): Device Capability Extension Attr */
	prAttrDCEA = (struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrDCEA->ucAttrId = NAN_ATTR_ID_DEVICE_CAPABILITY_EXT;
	prAttrDCEA->u2Length = 2;
	prExtCap =
	(struct _NAN_ATTR_EXT_CAPABILITIES_T *)prAttrDCEA->aucExtCapabilities;
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_PARING_SETUP_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_NPK_NIK_CACHE_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);

	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T);
	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_EXT_CAPABILITIES_T);

    /* CSIA(len=8) */
	prAttrCSIA = (struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrCSIA->ucAttrId = NAN_ATTR_ID_CIPHER_SUITE_INFO;
	prAttrCSIA->u2Length = 5;
	prAttrCSIA->ucCapabilities = 0;
	prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
					(prAttrCSIA->aucCipherSuiteList);
	prCsid->ucPublishID = prPairingFsm->ucPublishID;
	prCsid->ucCipherSuiteID = 1;
	prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
		(prAttrCSIA->aucCipherSuiteList +
		sizeof(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T));

	prCsid->ucPublishID = prPairingFsm->ucPublishID;
	prCsid->ucCipherSuiteID = 7;
	prPairingFsm->nan_ie_len += 8; /* 5 + 3 */

	/* NIRA(len=20) */
	prAttrNIRA = (struct _NAN_ATTR_NIRA_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrNIRA->ucAttribID = NAN_ATTR_ID_NAN_IDENTITY_RESOLUTION;
	prAttrNIRA->u2Len = 17;
	prAttrNIRA->ucCipherVer = 0;
	kalMemCpyS(&prAttrNIRA->u8Nonce,
		NAN_NIR_NONCE_LEN,
		prPairingFsm->aucNonce,
		NAN_NIR_NONCE_LEN);
	prAttrNIRA->u8Tag = prPairingFsm->u8Tag;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_ATTR_NIRA_T);

	DBGLOG(NAN, INFO, "[%s] NanIeLen=%zu\n",
		__func__, prPairingFsm->nan_ie_len);
	nanUtilDump(prAdapter, "NAN IE Verification",
		prPairingFsm->nan_ie, prPairingFsm->nan_ie_len);
}
void
pairingNotifyPasn(IN struct ADAPTER *prAdapter,
		struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[%s] NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] go to PASN\n", __func__);
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief PASN Auth frame TX Wrapper Function
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint32_t
pairingTxPasnAuthFrame(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u2FrameLength,
	IN PFN_TX_DONE_HANDLER pfTxDoneHandler,
	IN struct STA_RECORD *prSelectStaRec)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
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
	if (!prAdapter->rWifiVar.fgNoPmf && (prSelectStaRec != NULL) &&
		(prSelectStaRec->rPmfCfg.fgApplyPmf == TRUE)) {
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
			TRUE);
		DBGLOG(NAN, INFO,
		"[%s] StaIdx:%d, MAC=>%02x:%02x:%02x:%02x:%02x:%02x\n",
		__func__,
		prSelectStaRec->ucIndex, prSelectStaRec->aucMacAddr[0],
		prSelectStaRec->aucMacAddr[1], prSelectStaRec->aucMacAddr[2],
		prSelectStaRec->aucMacAddr[3], prSelectStaRec->aucMacAddr[4],
		prSelectStaRec->aucMacAddr[5]);
	}
	/* 4 <6> Enqueue the frame to send this NAF frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
	return WLAN_STATUS_SUCCESS;
}
uint32_t
pairingRxPasnAuthFrame(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb)
{
	/* TODO: add interface for Rx PASN auth frame */
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	return WLAN_STATUS_SUCCESS;
}
uint8_t
pairingFsmPairedOrNot(IN struct ADAPTER *prAdapter,
			IN struct NAN_DISCOVERY_EVENT *prDiscEvt)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint8_t fgIsPaired = FALSE;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	DBGLOG(NAN, INFO, "[%s] tag=%llu, nonce=%llu\n",
						__func__,
						prDiscEvt->u8NiraTag,
						prDiscEvt->u8NiraNonce);
	prPairingFsm = pairingFsmSearch(prAdapter, prDiscEvt->aucNanAddress);
	if (prPairingFsm) {
		/* Check service paired or not */
		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
			if (prPairingFsm->u8PeerTag == prDiscEvt->u8NiraTag) {
				DBGLOG(NAN, INFO,
				"[%s]PairingFsm%d is paired,addr="MACSTR_A"\n",
				__func__,
				prPairingFsm->ucIndex,
				prDiscEvt->aucNanAddress[0],
				prDiscEvt->aucNanAddress[1],
				prDiscEvt->aucNanAddress[2],
				prDiscEvt->aucNanAddress[3],
				prDiscEvt->aucNanAddress[4],
				prDiscEvt->aucNanAddress[5]);
				fgIsPaired = TRUE;
			}
		}
		prPairingFsm->fgIsPaired = fgIsPaired;
	}
	return fgIsPaired;
}
/**
 * rsn_pmkid_suite_b - Calculate PMK identifier for Suite B AKM
 * @kck: Key confirmation key
 * @kck_len: Length of kck in bytes
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @pmkid: Buffer for PMKID
 * Returns: 0 on success, -1 on failure
 *
 * IEEE Std 802.11ac-2013 - 11.6.1.3 Pairwise key hierarchy
 * NIR-TAG = Truncate-64(HMAC-SHA-256(NIK, "NIR" || AA(NMI) || SPA(nonce)))
 */
void
pairingDeriveNirTag(struct ADAPTER *prAdapter, const u8 *nik,
	size_t nik_len, const u8 *aa,
	const u8 *spa, u64 *tag)
{
	/* aa = NMI, spa = nonce */
	char *title = "NIR";
	const u8 *addr[3] = {NULL, NULL, NULL};
	const size_t len[3] = { 3, ETH_ALEN, NAN_NIR_TAG_LEN };
	unsigned char hash[SHA256_MAC_LEN] = {0};

	addr[0] = (u8 *)title;
	addr[1] = aa;
	addr[2] = spa;
	if (hmac_sha256_vector(nik, nik_len, 3, addr, len, hash) < 0)
		DBGLOG(NAN, ERROR, "[%s] NIR-TAG derivation failed\n",
		__func__);
	/* TAG=64bit, len = 8byte */
	kalMemCpyS(tag, NAN_NIR_TAG_LEN, hash, NAN_NIR_TAG_LEN);
	nanUtilDump(prAdapter, "hash", hash, SHA256_MAC_LEN);
}
void
pairingDeriveNdPmk(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	u8 *myaddr = NULL;
	u8 *peer = NULL;
	u8 context[2 * MAC_ADDR_LEN] = {0}, *ptr = context;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	/*
	 * AEK = KDF-Hash-256(PMK, "AEK Derivation", Selected AKM Suite ||
	 *       min(localMAC, peerMAC) || max(localMAC, peerMAC))
	 */
	/* Selected AKM Suite: SAE */
	myaddr = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	peer = prPairingFsm->prStaRec->aucMacAddr;
	DBGLOG(NAN, INFO,
	"[%s] pairing_type=%d\n", __func__, prPairingFsm->ucPairingType);
	if (prPairingFsm->ucPairingType == NAN_PAIRING_REQUESTOR) {
		kalMemCpyS(ptr, MAC_ADDR_LEN, myaddr, MAC_ADDR_LEN);
		ptr += MAC_ADDR_LEN;
		kalMemCpyS(ptr, MAC_ADDR_LEN, peer, MAC_ADDR_LEN);
	} else { /* NAN_PAIRING_RESPONDER */
		kalMemCpyS(ptr, MAC_ADDR_LEN, peer, MAC_ADDR_LEN);
		ptr += MAC_ADDR_LEN;
		kalMemCpyS(ptr, MAC_ADDR_LEN, myaddr, MAC_ADDR_LEN);
	}
	if (prPairingFsm->prPtk == NULL) {
		DBGLOG(NAN, INFO, "[%s] null ptk and return\n", __func__);
		return;
	}
	if (prPairingFsm->prPtk->kdk_len != WPA_KDK_MAX_LEN) {
		DBGLOG(NAN, INFO, "[%s] NM-KDK should be 256 bits always\n",
		__func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] pmk size=%zu, context size=%zu, kdk_len=%zu\n",
		__func__,
		sizeof(prPairingFsm->key_info.body.pmk_info.pmk),
		sizeof(context),
		prPairingFsm->prPtk->kdk_len);
	DBGLOG(NAN, INFO, "[%s] check context and kdk\n", __func__);
	nanUtilDump(prAdapter, "context", context, sizeof(context));
	nanUtilDump(prAdapter, "NM-KDK",
		prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);
	sha256_prf(prPairingFsm->prPtk->kdk,
		prPairingFsm->prPtk->kdk_len,
		"NDP PMK Derivation",
		context, (MAC_ADDR_LEN + MAC_ADDR_LEN),
		prPairingFsm->key_info.body.pmk_info.pmk,
		sizeof(prPairingFsm->key_info.body.pmk_info.pmk));
	nanUtilDump(prAdapter, "ND-PMK:",
		prPairingFsm->key_info.body.pmk_info.pmk,
		sizeof(prPairingFsm->key_info.body.pmk_info.pmk));
}
void
pairingDeriveNik(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
#if 0
	random_get_bytes(prPairingFsm->aucGmk, WPA_GMK_LEN);
#endif
}
void
pairingDeriveNirNonce(struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	random_get_bytes(prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
}
void
pairingComposeNikKde(struct NIK_KDE_INFO *prNikKde,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	prNikKde->type = 0xdd;
	prNikKde->length = 21;
	prNikKde->oui[0] = 0x50;
	prNikKde->oui[1] = 0x6F;
	prNikKde->oui[2] = 0x9A;
	prNikKde->data_type = NAN_KDE_NIK;
	prNikKde->cipher_ver = 0;
	kalMemCpyS(prNikKde->nik,
		NAN_NIK_LEN, prPairingFsm->aucNik, NAN_NIK_LEN);
}
void
pairingComposeNikLifetimeKde(struct NIK_LIFETIME_KDE_INFO *prNikLifetimeKde,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	prNikLifetimeKde->type = 0xdd;
	prNikLifetimeKde->length = 10;
	prNikLifetimeKde->oui[0] = 0x50;
	prNikLifetimeKde->oui[1] = 0x6F;
	prNikLifetimeKde->oui[2] = 0x9A;
	prNikLifetimeKde->data_type = NAN_KDE_KEY_LIFETIME;
	prNikLifetimeKde->key_bitmap = 0x08;
	/* Lifetime uint: sec, set as 15 min */
	prNikLifetimeKde->lifetime = 60 * 15;
}
void
pairingDecryptSKDA(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			uint8_t *pucKey, uint16_t u2KeyLen)
{
	struct wpa_sm *sm = NULL;
	struct wpa_eapol_key *key = NULL;
	uint8_t *key_data = NULL;
	uint16_t ver = 0, key_info = 0;
	size_t key_data_len = 0;
	struct NIK_KDE_INFO *prNikKde = NULL;
	struct NIK_LIFETIME_KDE_INFO *prNikLifetimeKde = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[%s] FSM null\n", __func__);
		return;
	}
	if (pucKey == NULL) {
		DBGLOG(NAN, ERROR, "[%s] Key null\n", __func__);
		return;
	}
	prPairingFsm->u2Skda2KeyLength = u2KeyLen;
	kalMemCpyS(prPairingFsm->aucSkdaKeyData,
		NAN_KDE_ATTR_BUF_SIZE, pucKey, u2KeyLen);
	key = (struct wpa_eapol_key *)prPairingFsm->aucSkdaKeyData;
	key_data = (uint8_t *)(key + 1);
	key_info = WPA_GET_BE16(key->key_info);
	ver = key_info & WPA_KEY_INFO_TYPE_MASK;

	key_data_len = WPA_GET_BE16(key->key_data_length);
	if (key_data_len > NAN_KDE_ATTR_BUF_SIZE)
		return;

	nanUtilDump(prAdapter, "SKDA",
		(uint8_t *)prPairingFsm->aucSkdaKeyData, NAN_KDE_ATTR_BUF_SIZE);
	DBGLOG(NAN, INFO, "[%s] go set responder sm\n", __func__);
	sm = nanSecGetPairingResponderSm(prPairingFsm->ucIndex);
#if 1 /* sm init */
	sm->renew_snonce = 1;
	sm->ctx = &g_rNanWpaPairingSmCtx;
	sm->dot11RSNAConfigPMKLifetime = 43200;
	sm->dot11RSNAConfigPMKReauthThreshold = 70;
	sm->dot11RSNAConfigSATimeout = 60;
	kalMemCpyS(&sm->ptk,
		sizeof(struct wpa_ptk),
		prPairingFsm->prPtk,
		sizeof(struct wpa_ptk));
	sm->ptk_set = 1;
	DBGLOG(NAN, INFO, "[%s] kek_len=%zu, kck_len=%zu, tk_len=%zu\n",
	__func__, sm->ptk.kek_len, sm->ptk.kck_len, sm->ptk.tk_len);
	DBGLOG(NAN, INFO, "[%s] P_FSM kek_len=%zu, kck_len=%zu, tk_len=%zu\n",
	__func__, prPairingFsm->prPtk->kek_len, prPairingFsm->prPtk->kck_len,
	prPairingFsm->prPtk->tk_len);
	DBGLOG(NAN, INFO, "[%s] ver=0x%x, key_info=0x%x, key_data_len=%zu\n",
	__func__, ver, key_info, key_data_len);
#endif
	DBGLOG(NAN, INFO, "[%s] go wpa_supplicant_decrypt_key_data\n",
	__func__);
	ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	wpa_supplicant_decrypt_key_data(sm, key, ver, key_data, &key_data_len);
	DBGLOG(NAN, INFO, "[%s] get NIK KDE\n", __func__);
	prNikKde = (struct NIK_KDE_INFO *)key_data;
	prNikLifetimeKde = (struct NIK_LIFETIME_KDE_INFO *)
		(key_data + sizeof(struct NIK_KDE_INFO));
	kalMemCpyS(prPairingFsm->aucPeerNik,
		NAN_NIK_LEN, prNikKde->nik, NAN_NIK_LEN);
	nanUtilDump(prAdapter, "Key_data", key_data, key_data_len);
	nanUtilDump(prAdapter, "NIK_KDE", (uint8_t *)prNikKde,
		sizeof(struct NIK_KDE_INFO));
#if 0 /* temp disable timer*/
	cnmTimerStopTimer(prAdapter,
		&prPairingFsm->rNikTimer);
	/*ToDo:Init Timer to check get
	 * Auth Txdone avoid sta_rec not clear
	 */
	cnmTimerInitTimer(prAdapter,
		&prPairingFsm->rNikTimer,
		(PFN_MGMT_TIMEOUT_FUNC)
		pairingNikLifetimeout,
		(unsigned long) prPairingFsm);
	cnmTimerStartTimer(prAdapter,
		&prPairingFsm->rNikTimer,
		prNikLifetimeKde->lifetime);
#endif
}
void
pairingNikLifetimeout(IN struct ADAPTER *prAdapter,
			IN unsigned long plParamPtr)
{
	struct PAIRING_FSM_INFO *prPairingFsm =
		(struct PAIRING_FSM_INFO *) plParamPtr;
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] PairingFSM %d clear peer nik\n",
		__func__, prPairingFsm->ucIndex);
	kalMemZero(prPairingFsm->aucPeerNik, NAN_NIK_LEN);
}
void
pairingServiceLifetimeout(IN struct ADAPTER *prAdapter,
			IN unsigned long plParamPtr)
{
	struct PAIRING_FSM_INFO *prPairingFsm =
		(struct PAIRING_FSM_INFO *) plParamPtr;
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "[%s] FSM NULL and return\n", __func__);
		return;
	}
	DBGLOG(NAN, INFO, "[%s] PairingFSM %d clear peer nik\n",
		__func__, prPairingFsm->ucIndex);
	pairingFsmFree(prAdapter, prPairingFsm);
}
void
pairingDbgInfo(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct STA_RECORD *prStaRec = NULL;
	uint8_t tag[8] = {0}, peer_tag[8] = {0};

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm) {
			if (prPairingFsm->fgIsInUse) {
				prStaRec = prPairingFsm->prStaRec;
				DBGLOG(NAN, INFO,
				"--- PairingFSM %zu ---\n", szIdx);
				DBGLOG(NAN, INFO,
				"Bootstrapping method = 0x%x\n",
				prPairingFsm->u2BootstrapMethod);
				DBGLOG(NAN, INFO,
				"Pairing state = %u\n",
				prPairingFsm->ePairingState);
				DBGLOG(NAN, INFO,
				"Pub or not=%u\n",
				prPairingFsm->ucIsPub);
				DBGLOG(NAN, INFO,
				"PubId = %u, SubId = %u\n",
				prPairingFsm->ucPublishID,
				prPairingFsm->ucSubscribeID);
				DBGLOG(NAN, INFO,
				"NPK/NIK Caching enable = %u\n",
				prPairingFsm->fgCachingEnable);
				if (prStaRec) {
					DBGLOG(NAN, INFO,
					"Peer Addr = %x:%x:%x:%x:%x:%x\n",
					prStaRec->aucMacAddr[0],
					prStaRec->aucMacAddr[1],
					prStaRec->aucMacAddr[2],
					prStaRec->aucMacAddr[3],
					prStaRec->aucMacAddr[4],
					prStaRec->aucMacAddr[5]);
					DBGLOG(NAN, INFO,
					"WlanIdx = %u, StaRecIndex=%u\n",
					prStaRec->ucWlanIndex,
					prStaRec->ucIndex);
				} else {
					DBGLOG(NAN, INFO,
					"StaRec still NULL\n");
				}
				/* Local Info */
				nanUtilDump(prAdapter, "NAN_Nonce",
					prPairingFsm->aucNonce,
					NAN_NIR_NONCE_LEN);
				kalMemCpyS(tag, NAN_NIR_TAG_LEN,
					&prPairingFsm->u8Tag, NAN_NIR_TAG_LEN);
				nanUtilDump(prAdapter, "NAN_Tag",
					tag, NAN_NIR_TAG_LEN);
				nanUtilDump(prAdapter, "NAN_NIK",
					prPairingFsm->aucNik, NAN_NIK_LEN);
				/* Peer Info */
				nanUtilDump(prAdapter, "NAN_PEER_Nonce",
					prPairingFsm->aucPeerNonce,
					NAN_NIR_NONCE_LEN);
				kalMemCpyS(peer_tag, NAN_NIR_TAG_LEN,
					&prPairingFsm->u8PeerTag,
					NAN_NIR_TAG_LEN);
				nanUtilDump(prAdapter, "NAN_PEER_Tag",
					peer_tag, NAN_NIR_TAG_LEN);
				nanUtilDump(prAdapter, "NAN_PEER_NIK",
					prPairingFsm->aucPeerNik, NAN_NIK_LEN);
				/* TK */
				if (prPairingFsm->ePairingState ==
					NAN_PAIRING_PAIRED ||
					prPairingFsm->ePairingState ==
					NAN_PAIRING_PAIRED_VERIFICATION) {
					nanUtilDump(prAdapter, "NAN_TK",
					prPairingFsm->prPtk->tk,
					prPairingFsm->prPtk->tk_len);
				} else {
					DBGLOG(NAN, INFO, "NAN_TK=0\n");
				}
				nanUtilDump(prAdapter, "NAN_PMKID",
					prPairingFsm->rNpksaCache.pmkid,
					NAN_NPKID_LEN);
				nanUtilDump(prAdapter, "NAN_PMK",
					prPairingFsm->rNpksaCache.pmk,
					NAN_NPK_LEN);
				DBGLOG(NAN, INFO, "---------------------\n");
			}
		}
	}
	DBGLOG(NAN, INFO, "NAN_PAIRING_INIT = %d\n", NAN_PAIRING_INIT);
	DBGLOG(NAN, INFO, "NAN_PAIRING_BOOTSTRAPPING = %d\n",
		NAN_PAIRING_BOOTSTRAPPING);
	DBGLOG(NAN, INFO, "NAN_PAIRING_BOOTSTRAPPING_DONE = %d\n",
		NAN_PAIRING_BOOTSTRAPPING_DONE);
	DBGLOG(NAN, INFO, "NAN_PAIRING_SETUP = %d\n",
		NAN_PAIRING_SETUP);
	DBGLOG(NAN, INFO, "NAN_PAIRING_PAIRED = %d\n",
		NAN_PAIRING_PAIRED);
	DBGLOG(NAN, INFO, "NAN_PAIRING_PAIRED_VERIFICATION = %d\n",
		NAN_PAIRING_PAIRED_VERIFICATION);
}
void pairingDbgStepsLog(struct PAIRING_FSM_INFO *prPairingFsm,
	size_t szIdx)
{
	if (prPairingFsm->fgIsInUse) {
		DBGLOG(NAN, INFO, "--- PairingFSM %zu STATE ---\n", szIdx);
		DBGLOG(NAN, INFO, "Current state = %u\n",
		prPairingFsm->ePairingState);
		prPairingFsm->ePairingState = pairingFsmNextState(prPairingFsm);
		DBGLOG(NAN, INFO, "Next state = %u\n",
		prPairingFsm->ePairingState);
		DBGLOG(NAN, INFO, "PubId = %u, SubId = %u\n",
		prPairingFsm->ucPublishID, prPairingFsm->ucSubscribeID);
		DBGLOG(NAN, INFO, "---------------------\n");
		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
			if (prPairingFsm->prPtk) {
				prPairingFsm->prPtk->kek_len = WPA_KEK_MAX_LEN;
				prPairingFsm->prPtk->kck_len = WPA_KCK_MAX_LEN;
				prPairingFsm->prPtk->kdk_len = WPA_KDK_MAX_LEN;
				prPairingFsm->prPtk->tk_len = WPA_TK_MAX_LEN;
				random_get_bytes(prPairingFsm->prPtk->kek,
						WPA_KEK_MAX_LEN);
				random_get_bytes(prPairingFsm->prPtk->kdk,
						WPA_KDK_MAX_LEN);
				random_get_bytes(prPairingFsm->prPtk->kck,
						WPA_KCK_MAX_LEN);
				random_get_bytes(prPairingFsm->prPtk->tk,
						WPA_TK_MAX_LEN);
			}
		}
	}
}
void
pairingDbgSteps(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm)
			pairingDbgStepsLog(prPairingFsm, szIdx);
	}
}
void
pairingDbgTwoStepsImpl(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	size_t szIdx)
{
	if (prPairingFsm->fgIsInUse) {
		DBGLOG(NAN, INFO,
		"--- PairingFSM %zu GOOT WRONG STATE ---\n", szIdx);
		DBGLOG(NAN, INFO,
		"Current state = %u\n", prPairingFsm->ePairingState);
		prPairingFsm->ePairingState += 2;
		DBGLOG(NAN, INFO,
		"Next state = %u\n", prPairingFsm->ePairingState);
		DBGLOG(NAN, INFO,
		"PubId = %u, SubId = %u\n",
		prPairingFsm->ucPublishID, prPairingFsm->ucSubscribeID);
		pairingFsmSteps(prAdapter,
		prPairingFsm, prPairingFsm->ePairingState);
		DBGLOG(NAN, INFO, "---------------------\n");
	}
}
void
pairingDbgTwoSteps(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "[%s] Enter\n", __func__);
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm)
			pairingDbgTwoStepsImpl(prAdapter, prPairingFsm, szIdx);
	}
}
void
pairingPrintKeyInfo(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm)
{
	u8 pmkid[16] = {0};
	u8 tag[8] = {0};
	u8 nonce[8] = {0};

	kalMemCpyS(tag, NAN_NIR_TAG_LEN, &prPairingFsm->u8Tag, NAN_NIR_TAG_LEN);
	nanUtilDump(prAdapter, "tag", tag, NAN_NIR_TAG_LEN);
	kalMemCpyS(nonce, NAN_NIR_NONCE_LEN,
		prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
	nanUtilDump(prAdapter, "nonce", nonce, NAN_NIR_NONCE_LEN);
	kalMemCpyS(pmkid, NAN_NIR_NONCE_LEN, nonce, NAN_NIR_NONCE_LEN);
	kalMemCpyS(pmkid + NAN_NIR_NONCE_LEN,
		NAN_NIR_TAG_LEN, tag, NAN_NIR_TAG_LEN);
	nanUtilDump(prAdapter, "pmkid", pmkid, NAN_NPKID_LEN);
#if 1 /* Dump key for RDUT */
	nanUtilDump(prAdapter, PREL3"Pairing-TK",
		prPairingFsm->prPtk->tk, prPairingFsm->prPtk->tk_len);
	nanUtilDump(prAdapter, PREL3"Pairing-KCK",
		prPairingFsm->prPtk->kck, prPairingFsm->prPtk->kck_len);
	nanUtilDump(prAdapter, PREL3"Pairing-KDK",
		prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);
	nanUtilDump(prAdapter, PREL4"Pairing-KEK",
		prPairingFsm->prPtk->kek, prPairingFsm->prPtk->kek_len);
#endif
	if (prPairingFsm->u2BootstrapMethod !=
		NAN_BOOTSTRAPPING_OPPORTUNISTIC) {
		nanUtilDump(prAdapter, "pairing-PMK",
			prPairingFsm->key_info.body.pmk_info.pmk,
			prPairingFsm->key_info.body.pmk_info.pmk_len);
		nanUtilDump(prAdapter, "cache Pairing-PMK",
			prPairingFsm->rNpksaCache.pmk,
			prPairingFsm->rNpksaCache.pmk_len);
		nanUtilDump(prAdapter, "cache Pairing-PMKID",
			prPairingFsm->rNpksaCache.pmkid,
			NAN_NPKID_LEN);
	}
}
uint32_t
nanPairingProcessAuthFrame(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	u32 *pairing_request_type,
	u8 *enable_pairing_cache,
	u8 *nonce,
	u8 *nir_tag
	)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint8_t *pucAttrList = NULL;
	uint16_t u2AttrListLength;
	uint8_t *pucOffset, *pucEnd;
	struct _NAN_ATTR_HDR_T *prNanAttr;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(NAN, ERROR, "[pairing-pasn][%s] Enter\n", __func__);

	if (!prSwRfb) {
		DBGLOG(NAN, ERROR, "[%s] prSwRfb error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	*pairing_request_type = NAN_PAIRING_SETUP_REQ_T;
	*enable_pairing_cache = 0;

	prAuthFrame =
		(struct WLAN_AUTH_FRAME *)(prSwRfb->pvHeader);
	if (!prAuthFrame) {
		DBGLOG(NAN, ERROR, "[%s] prNaf error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	u2AttrListLength =
		prSwRfb->u2PacketLen -
		OFFSET_OF(struct WLAN_AUTH_FRAME, aucInfoElem);
	pucAttrList =
		(uint8_t *)(prAuthFrame->aucInfoElem);
	{
		/* move pucAttrList & u2_AttrListLength more
		 * until VendorSpecific IE with OUI Type 19
		 */
		const uint8_t *start = pucAttrList;
		size_t len = u2AttrListLength;
		const struct element *elem;

		pucOffset = NULL;
		pucEnd = NULL;
		for_each_element(elem, start, len) {
			uint8_t id = elem->id;
			const uint8_t *pos = elem->data;
			unsigned int oui = WPA_GET_BE24(pos);

			if ((id == WLAN_EID_VENDOR_SPECIFIC) &&
				(oui == OUI_WFA) &&
				(pos[3] == VENDOR_OUI_TYPE_NAN_SDF)) {
				pucOffset = (uint8_t *)&pos[4];
				pucEnd = (uint8_t *)(pos + elem->datalen);
			}
		}
	}
	if (pucOffset != NULL) {
		DBGLOG(NAN, ERROR, PAIRING_DBGM13,
		__func__,
		pucOffset[0], pucOffset[1], pucOffset[2], pucOffset[3]);
	} else {
		DBGLOG(NAN, ERROR,
		"[pairing-pasn] %s() parsing NAN IE failed\n",
		__func__);
		return WLAN_STATUS_FAILURE;
	}


	DBGLOG(NAN, ERROR, "[pairing-pasn]dump s---->\n");
	for (int i = 0; i < u2AttrListLength; i++)
		DBGLOG(NAN, ERROR, "[%02x]", pucAttrList[i]);
	DBGLOG(NAN, ERROR, "[pairing-pasn]dump <-----e\n");

	while (pucOffset < pucEnd && rStatus == WLAN_STATUS_SUCCESS) {
		if (pucEnd - pucOffset <
			OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody)) {
			/* insufficient length */
			DBGLOG(NAN, ERROR, "[pairing-pasn][%s] trace:%d\n",
				__func__, __LINE__);
			break;
		}
		/* buffer pucAttr for later type-casting purposes */
		prNanAttr = (struct _NAN_ATTR_HDR_T *)pucOffset;
		if (pucEnd - pucOffset <
			OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
				prNanAttr->u2Length) {
			/* insufficient length */
			DBGLOG(NAN, ERROR,
			"[pairing-pasn][%s] trace:%d prNanAttr->u2Length=%u\n",
			__func__, __LINE__, prNanAttr->u2Length);
			break;
		}
		DBGLOG(NAN, ERROR,
			   "prNanAttr->ucAttrId=%u, prNanAttr->u2Length=%u\n",
			   prNanAttr->ucAttrId, prNanAttr->u2Length);
		/* move to next Attr */
		pucOffset += (OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
				  prNanAttr->u2Length);
		DBGLOG(NAN, ERROR, "[pairing-pasn][%s] trace:%d ucAttrId=%u\n",
			__func__, __LINE__, prNanAttr->ucAttrId);

		/* Parsing attributes */
		switch (prNanAttr->ucAttrId) {
		case NAN_ATTR_ID_DEVICE_CAPABILITY_EXT:
		{
			/* DCEA */
			struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T
			*prAttrDevCapExt =
			(struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *) prNanAttr;
			*enable_pairing_cache =
			((*(u16 *)prAttrDevCapExt->aucExtCapabilities) &
				NAN_ATTR_DCEA_NPK_NIK_CACHING_ENABLE);
			DBGLOG(NAN, ERROR,
				"[pairing-pasn]:enable_pairing_cache=%d\n",
				*enable_pairing_cache);
			break;
		}
		case NAN_ATTR_ID_NAN_PAIRING_BOOTSTRAPPING:
		{
			/* NPBA */
			struct _NAN_ATTR_NPBA_T *prAttrNPBA =
				(struct _NAN_ATTR_NPBA_T *) prNanAttr;

			DBGLOG(NAN, ERROR,
			"ucDialogTok:%d,ucTypeStatus:%d,u2BootstapMethod:%d\n",
			prAttrNPBA->ucDialogTok,
			prAttrNPBA->ucTypeStatus,
			prAttrNPBA->u2BootstapMethod);


			break;
		}
		case NAN_ATTR_ID_CIPHER_SUITE_INFO:
		{
			/* CSIA */
			struct _NAN_ATTR_CIPHER_SUITE_INFO_T
			*prCipherSuiteAttr =
				(struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
				prNanAttr;
			struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCipherSuite =
				(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
				prCipherSuiteAttr->aucCipherSuiteList;

			DBGLOG(NAN, ERROR,
				"ucCipherSuiteID:%d, ucPublishID:%d\n",
				prCipherSuite->ucCipherSuiteID,
				prCipherSuite->ucPublishID);

			break;
		}
		case NAN_ATTR_ID_NAN_IDENTITY_RESOLUTION:
		{
			/* NIRA */
			struct _NAN_ATTR_NIRA_T *prNira =
				(struct _NAN_ATTR_NIRA_T *) prNanAttr;
			DBGLOG(NAN, ERROR,
			"[pairing-verification]%s():u8Nonce:%llu, u8Tag:%llu\n",
			__func__, prNira->u8Nonce, prNira->u8Tag);
			memcpy(nonce, &prNira->u8Nonce, NAN_IDENTITY_NONCE_LEN);
			memcpy(nir_tag, &prNira->u8Tag, NAN_IDENTITY_TAG_LEN);
			*pairing_request_type = NAN_PAIRING_VERIFICATION_REQ_T;
			DBGLOG(NAN, ERROR,
			"[pairing-verification]%s():pairing_request_type=%d\n",
			__func__,
			*pairing_request_type);
			break;
		}
		default:
			break;
		}
	}
	DBGLOG(NAN, ERROR, "[pairing-pasn][%s] leave\n", __func__);
	return WLAN_STATUS_SUCCESS;
}

int dump_pairing_fsm(struct ADAPTER *prAdapter, char *startoff, int lenlimit)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	int count = 0;

	count += sprintf(startoff, "[%s] Enter ---->\n", __func__);
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		count += sprintf(startoff + count,
			"[pairing-debug]<<<<szIdx = %lu>>>>\n", szIdx);
		if (prPairingFsm == NULL) {
			count += sprintf(startoff + count,
				"[pairing-debug]szIdx[%lu] NULL skip\n", szIdx);
			continue;
		}
		if (prPairingFsm->fgIsInUse) {
			count += sprintf(startoff + count,
				"[pairing-debug] szIdx[%lu] dump\n", szIdx);
			count += sprintf(startoff + count,
			"[pairing-debug] szIdx(%lu), ucIsPub(%d), ucPairingType(%d), ePairingState(%d), prStaRec=%p\n",
			szIdx, prPairingFsm->ucIsPub,
			prPairingFsm->ucPairingType,
			prPairingFsm->ePairingState,
			prPairingFsm->prStaRec);
		}
	}
	count += sprintf(startoff + count, "<--------[%s] Leave\n", __func__);

	return count;
}

uint32_t nanPairingVerification_FsmFF(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm, uint8_t *pucPeerNMI)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	DBGLOG(NAN, ERROR, "[pairing-verification] enter %s()\n", __func__);

	if (prPairingFsm == NULL || pucPeerNMI == NULL) {
		DBGLOG(NAN, ERROR,
		"[pairing-verification] %s() prPairingFsm NULL !!\n", __func__);
		return WLAN_STATUS_FAILURE;
	}
	/* 1. starec alloc for for 2G */
	if (prPairingFsm->prStaRec == NULL) {
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, BAND_2G4);
		DBGLOG(NAN, INFO,
		"[%s] go alloc pairing starec\n", __func__);
		prPairingFsm->prStaRec =
			cnmStaRecAlloc(prAdapter, STA_TYPE_NAN,
			ucBssIndex, pucPeerNMI);
		if (prPairingFsm->prStaRec) {
			atomic_set(
			&prPairingFsm->prStaRec->NanRefCount,
			1);
		} else {
			rStatus = WLAN_STATUS_FAILURE;
			goto error;
		}
	}
	DBGLOG(NAN, INFO, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
			pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
			pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);

	DBGLOG(NAN, INFO, "[pairing-verification]%s()step 1 done.\n", __func__);
	/* 2. for 5G, let FSM setup 5gStaRec */
	pairingFsmSteps(prAdapter, prPairingFsm,
		NAN_PAIRING_BOOTSTRAPPING_DONE);
	DBGLOG(NAN, INFO, "[pairing-verification]%s()step 2 done.\n", __func__);

error:
	return rStatus;
}

void nanPairingCalCustomPMKID(uint64_t nonce, uint64_t tag, uint8_t *PMKID)
{
	memcpy((void *)PMKID, &nonce, NAN_NIR_NONCE_LEN);
	memcpy((void *)PMKID+NAN_NIR_NONCE_LEN, &tag, sizeof(uint64_t));
}

void nanPairingSavePublisherNonce(uint64_t nonce)
{
	g_publisherNonce = nonce;
}
uint64_t nanPairingLoadPublisherNonce(void)
{
	return g_publisherNonce;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#endif /* CFG_SUPPORT_NAN */

