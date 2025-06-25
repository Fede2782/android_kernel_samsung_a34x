// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "p2p_rlm.c"
 *    \brief
 *
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
#include "rlm.h"

#if CFG_ENABLE_WIFI_DIRECT
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
static enum ENUM_CHNL_EXT rlmGetSco(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo);
/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */
#if (CFG_SUPPORT_WIFI_6G == 1)
void rlmUpdate6GOpInfo(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	struct _6G_OPER_INFOR_T *pr6gOperInfo;
	uint8_t ucMaxBandwidth;
	uint8_t ucSeg0 = 0, ucSeg1 = 0;

	if (IS_BSS_APGO(prBssInfo) == FALSE || prBssInfo->eBand != BAND_6G)
		return;

	pr6gOperInfo = &prBssInfo->r6gOperInfor;

	HE_SET_6G_OP_INFOR_PRESENT(prBssInfo->ucHeOpParams);

	ucMaxBandwidth = rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo);
#if (CFG_SUPPORT_SAP_PUNCTURE == 1)
	if (prBssInfo->fgIsEhtDscbPresent) {
		/* get seg0 & seg1 with original bandwidth for puncturing */
		ucSeg0 = nicGetS1(prBssInfo->eBand,
				  prBssInfo->ucPrimaryChannel,
				  prBssInfo->eBssSCO,
				  ucMaxBandwidth);
		ucSeg1 = nicGetS2(prBssInfo->eBand,
				  prBssInfo->ucPrimaryChannel,
				  ucMaxBandwidth);

		rlmPunctUpdateLegacyBw(prBssInfo->eBand,
				       prBssInfo->u2EhtDisSubChanBitmap,
				       prBssInfo->ucPrimaryChannel,
				       &ucMaxBandwidth,
				       &ucSeg0,
				       &ucSeg1,
				       NULL);
	}
#endif /* CFG_SUPPORT_SAP_PUNCTURE */

	ucMaxBandwidth = kal_min_t(uint8_t,
				   ucMaxBandwidth,
				   MAX_BW_160MHZ);
	/* re-sync seg0 & seg1 channel in case puncture takes effect. */
	ucSeg0 = nicGetS1(prBssInfo->eBand,
			  prBssInfo->ucPrimaryChannel,
			  prBssInfo->eBssSCO,
			  ucMaxBandwidth);
	ucSeg1 = nicGetS2(prBssInfo->eBand,
			  prBssInfo->ucPrimaryChannel,
			  ucMaxBandwidth);

	pr6gOperInfo->rControl.bits.ChannelWidth =
		heRlmMaxBwToHeBw(ucMaxBandwidth);
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
	pr6gOperInfo->rControl.bits.RegulatoryInfo =
		HE_REG_INFO_VERY_LOW_POWER;
#endif
	pr6gOperInfo->ucPrimaryChannel =
		prBssInfo->ucPrimaryChannel;
	pr6gOperInfo->ucChannelCenterFreqSeg0 = ucSeg0;
	pr6gOperInfo->ucChannelCenterFreqSeg1 = ucSeg1;
	pr6gOperInfo->ucMinimumRate = 6;

	DBGLOG(RLM, TRACE,
		"Set 6G operating info: BW[%d] REG[%d] CH[%d] Seg0[%d] Seg1[%d]\n",
		pr6gOperInfo->rControl.bits.ChannelWidth,
		pr6gOperInfo->rControl.bits.RegulatoryInfo,
		pr6gOperInfo->ucPrimaryChannel,
		pr6gOperInfo->ucChannelCenterFreqSeg0,
		pr6gOperInfo->ucChannelCenterFreqSeg1);
}
#endif

void rlmBssUpdateChannelParams(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	uint8_t i;
	uint8_t ucMaxBw = 0;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	/* Operation band, channel shall be ready before invoking this function.
	 * Bandwidth may be ready if other network is connected
	 */
	prBssInfo->fg40mBwAllowed = FALSE;
	prBssInfo->fgAssoc40mBwAllowed = FALSE;
	prBssInfo->eBssSCO = CHNL_EXT_SCN;
	prBssInfo->ucHtOpInfo1 = 0;

	/* Check if AP can set its bw to 40MHz
	 * But if any of BSS is setup in 40MHz,
	 * the second BSS would prefer to use 20MHz
	 * in order to remain in SCC case
	 */
	if (cnmBss40mBwPermitted(prAdapter, prBssInfo->ucBssIndex)) {

		if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
			prBssInfo->eBssSCO =
				rlmGetScoForAP(prAdapter, prBssInfo);
		else
			prBssInfo->eBssSCO =
				rlmGetSco(prAdapter, prBssInfo);

		if (prBssInfo->eBssSCO != CHNL_EXT_SCN) {
			prBssInfo->fg40mBwAllowed = TRUE;
			prBssInfo->fgAssoc40mBwAllowed = TRUE;

			prBssInfo->ucHtOpInfo1 = (uint8_t)
				(((uint32_t) prBssInfo->eBssSCO)
				| HT_OP_INFO1_STA_CHNL_WIDTH);
		}
	}

	/* Filled the VHT BW/S1/S2 and MCS rate set */
	if (prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_VHT) {
		for (i = 0; i < 8; i++)
			prBssInfo->u2VhtBasicMcsSet |= BITS(2 * i, (2 * i + 1));
#if CFG_SUPPORT_TRX_LIMITED_CONFIG
		if (p2pFuncGetForceTrxConfig(prAdapter) ==
				P2P_FORCE_TRX_CONFIG_MCS7)
			prBssInfo->u2VhtBasicMcsSet &=
				(VHT_CAP_INFO_MCS_MAP_MCS7
				<< VHT_CAP_INFO_MCS_1SS_OFFSET);
		else
#endif
			prBssInfo->u2VhtBasicMcsSet &=
				(VHT_CAP_INFO_MCS_MAP_MCS9
				<< VHT_CAP_INFO_MCS_1SS_OFFSET);

		ucMaxBw = cnmOpModeGetMaxBw(prAdapter,
			prBssInfo);

		rlmFillVhtOpInfoByBssOpBw(prBssInfo, ucMaxBw);

		/* If the S1 is invalid, force to change bandwidth */
		if (prBssInfo->ucVhtChannelFrequencyS1 == 0) {
			/* Give GO/AP another chance to use BW80
			 * if failed to get S1 for BW160.
			 */
			if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT &&
				ucMaxBw == MAX_BW_160MHZ) {
				rlmFillVhtOpInfoByBssOpBw(prBssInfo,
					MAX_BW_80MHZ);
			}

			/* fallback to BW20/40 */
			if (prBssInfo->ucVhtChannelFrequencyS1 == 0) {
				prBssInfo->ucVhtChannelWidth =
					VHT_OP_CHANNEL_WIDTH_20_40;
			}
		}
	} else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_HE) {
		ucMaxBw = cnmOpModeGetMaxBw(prAdapter,
			prBssInfo);

		rlmFillVhtOpInfoByBssOpBw(prBssInfo, ucMaxBw);
	}

#if (CFG_SUPPORT_802_11AX == 1)
	/* Filled the HE Operation IE */
	if (prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_HE) {
		memset(prBssInfo->ucHeOpParams, 0, HE_OP_BYTE_NUM);

		prBssInfo->ucHeOpParams[0]
			|= HE_OP_PARAM0_TXOP_DUR_RTS_THRESHOLD_MASK;
		prBssInfo->ucHeOpParams[1]
			|= HE_OP_PARAM1_TXOP_DUR_RTS_THRESHOLD_MASK;

		prBssInfo->u2HeBasicMcsSet |= (HE_CAP_INFO_MCS_MAP_MCS7 << 0);
		for (i = 1; i < 8; i++)
			prBssInfo->u2HeBasicMcsSet |=
				(HE_CAP_INFO_MCS_NOT_SUPPORTED << 2 * i);

		if (IS_FEATURE_ENABLED(
			prAdapter->rWifiVar.fgBssMaxIdle)) {
			prBssInfo->u2MaxIdlePeriod =
				prAdapter->rWifiVar.u2BssMaxIdlePeriod;
			prBssInfo->ucIdleOptions =
				secIsProtectedBss(prAdapter, prBssInfo);
		}

#if (CFG_SUPPORT_WIFI_6G == 1)
		ucMaxBw = cnmOpModeGetMaxBw(prAdapter, prBssInfo);
		rlmFillVhtOpInfoByBssOpBw(prBssInfo, ucMaxBw);
		rlmUpdate6GOpInfo(prAdapter, prBssInfo);
#endif
	} else {
		memset(prBssInfo->ucHeOpParams, 0, HE_OP_BYTE_NUM);
		prBssInfo->ucBssColorInfo = 0;
		prBssInfo->u2HeBasicMcsSet = 0;
		prBssInfo->u2MaxIdlePeriod = 0;
		prBssInfo->ucIdleOptions = 0;
	}
#endif

#if (CFG_SUPPORT_802_11BE == 1)
	if (prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_EHT) {
		EHT_RESET_OP(prBssInfo->ucEhtOpParams);
		/* TODO */
	} else {
		EHT_RESET_OP(prBssInfo->ucEhtOpParams);
	}
#endif

	/*ERROR HANDLE*/
	if ((prBssInfo->ucVhtChannelWidth >= VHT_OP_CHANNEL_WIDTH_80) &&
	    (prBssInfo->ucVhtChannelFrequencyS1 == 0)) {
		DBGLOG(RLM, INFO,
			"Wrong AP S1 parameter setting, back to BW20!!!\n");

		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_20_40;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Init AP Bss
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rlmBssInitForAP(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	if (prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT)
		return;

	/* Check if AP can set its bw to 40MHz
	 * But if any of BSS is setup in 40MHz,
	 * the second BSS would prefer to use 20MHz
	 * in order to remain in SCC case
	 */
	rlmBssUpdateChannelParams(prAdapter, prBssInfo);

	if (cnmBss40mBwPermitted(prAdapter, prBssInfo->ucBssIndex)) {
		if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
			rlmUpdateBwByChListForAP(prAdapter, prBssInfo);
	}

	/* We may limit AP/GO Nss by RfBand in some case, ex CoAnt.
	 * Recalculte Nss when channel is selected.
	 */
	cnmOpModeGetTRxNss(prAdapter,
		prBssInfo->ucBssIndex,
		&prBssInfo->ucOpRxNss,
		&prBssInfo->ucOpTxNss);

	DBGLOG(RLM, INFO,
		"WLAN AP SCO=%d BW=%d S1=%d S2=%d CH=%d Band=%d TxN=%d RxN=%d\n",
		prBssInfo->eBssSCO,
		prBssInfo->ucVhtChannelWidth,
		prBssInfo->ucVhtChannelFrequencyS1,
		prBssInfo->ucVhtChannelFrequencyS2,
		prBssInfo->ucPrimaryChannel,
		prBssInfo->eBand,
		prBssInfo->ucOpTxNss,
		prBssInfo->ucOpRxNss);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief For probe response (GO, IBSS) and association response
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rlmRspGenerateObssScanIE(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct IE_OBSS_SCAN_PARAM *prObssScanIe;
	struct STA_RECORD *prStaRec =
		(struct STA_RECORD *) NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prMsduInfo->ucStaRecIndex);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsduInfo->ucBssIndex);

	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	/* !RLM_NET_IS_BOW(prBssInfo) &&   FIXME. */
	if (RLM_NET_IS_11N(prBssInfo) &&
	    prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)) &&
	    prBssInfo->eBand == BAND_2G4 &&
	    prBssInfo->eBssSCO != CHNL_EXT_SCN) {

		prObssScanIe = (struct IE_OBSS_SCAN_PARAM *)
		    (((uint8_t *) prMsduInfo->prPacket)
				+ prMsduInfo->u2FrameLength);

		/* Add 20/40 BSS coexistence IE */
		prObssScanIe->ucId = ELEM_ID_OBSS_SCAN_PARAMS;
		prObssScanIe->ucLength =
			sizeof(struct IE_OBSS_SCAN_PARAM) - ELEM_HDR_LEN;

		prObssScanIe->u2ScanPassiveDwell =
			dot11OBSSScanPassiveDwell;
		prObssScanIe->u2ScanActiveDwell =
			dot11OBSSScanActiveDwell;
		prObssScanIe->u2TriggerScanInterval =
			dot11BSSWidthTriggerScanInterval;
		prObssScanIe->u2ScanPassiveTotalPerChnl =
			dot11OBSSScanPassiveTotalPerChannel;
		prObssScanIe->u2ScanActiveTotalPerChnl =
			dot11OBSSScanActiveTotalPerChannel;
		prObssScanIe->u2WidthTransDelayFactor =
			dot11BSSWidthChannelTransitionDelayFactor;
		prObssScanIe->u2ScanActivityThres =
			dot11OBSSScanActivityThreshold;

		ASSERT(
			IE_SIZE(prObssScanIe)
			<= (ELEM_HDR_LEN + ELEM_MAX_LEN_OBSS_SCAN));

		prMsduInfo->u2FrameLength += IE_SIZE(prObssScanIe);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief P2P GO.
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
u_int8_t rlmUpdateBwByChListForAP(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	uint8_t ucLevel;
	u_int8_t fgBwChange;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	fgBwChange = FALSE;

	if (prBssInfo->eBssSCO == CHNL_EXT_SCN)
		return fgBwChange;

	ucLevel = rlmObssChnlLevel(prBssInfo,
		prBssInfo->eBand,
		prBssInfo->ucPrimaryChannel,
		prBssInfo->eBssSCO);

	if ((ucLevel == CHNL_LEVEL0) &&
		!prAdapter->rWifiVar.fgSapGoSkipObss) {
		/* Forced to 20MHz,
		 * so extended channel is SCN and STA width is zero
		 */
		prBssInfo->fgObssActionForcedTo20M = TRUE;

		if (prBssInfo->ucHtOpInfo1 != (uint8_t) CHNL_EXT_SCN) {
			prBssInfo->ucHtOpInfo1 = (uint8_t) CHNL_EXT_SCN;
			fgBwChange = TRUE;
			DBGLOG(RLM, INFO,
				"BW40: Set fgObssActionForcedTo20M\n");
		}

		cnmTimerStartTimer(prAdapter,
			&prBssInfo->rObssScanTimer,
			OBSS_20_40M_TIMEOUT * MSEC_PER_SEC);
	}

	/* Clear up all channel lists */
	prBssInfo->auc2G_20mReqChnlList[0] = 0;
	prBssInfo->auc2G_NonHtChnlList[0] = 0;
	prBssInfo->auc2G_PriChnlList[0] = 0;
	prBssInfo->auc2G_SecChnlList[0] = 0;
	prBssInfo->auc5G_20mReqChnlList[0] = 0;
	prBssInfo->auc5G_NonHtChnlList[0] = 0;
	prBssInfo->auc5G_PriChnlList[0] = 0;
	prBssInfo->auc5G_SecChnlList[0] = 0;

	return fgBwChange;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rlmHandleObssStatusEventPkt(struct ADAPTER *prAdapter,
		struct EVENT_AP_OBSS_STATUS *prObssStatus)
{
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prObssStatus);
	ASSERT(prObssStatus->ucBssIndex
		< prAdapter->ucSwBssIdNum);

	prBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter, prObssStatus->ucBssIndex);

	if (!prBssInfo || prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT)
		return;

	DBGLOG(RLM, TRACE,
		"erp_prot=%u ht_prot=%u gf=%u rifs=%u force_20m=%u\n",
		prObssStatus->ucObssErpProtectMode,
		prObssStatus->ucObssHtProtectMode,
		prObssStatus->ucObssGfOperationMode,
		prObssStatus->ucObssRifsOperationMode,
		prObssStatus->ucObssBeaconForcedTo20M);

	prBssInfo->fgObssErpProtectMode =
		(u_int8_t) prObssStatus->ucObssErpProtectMode;
	prBssInfo->eObssHtProtectMode =
		(enum ENUM_HT_PROTECT_MODE) prObssStatus->ucObssHtProtectMode;
	prBssInfo->eObssGfOperationMode =
		(enum ENUM_GF_MODE) prObssStatus->ucObssGfOperationMode;
	prBssInfo->fgObssRifsOperationMode =
		(u_int8_t) prObssStatus->ucObssRifsOperationMode;
	prBssInfo->fgObssBeaconForcedTo20M =
		(u_int8_t) prObssStatus->ucObssBeaconForcedTo20M;

	if (prAdapter->rWifiVar.fgSapGoSkipObss)
		prBssInfo->fgObssBeaconForcedTo20M = FALSE;

	/* Check if Beacon content need to be updated */
	rlmUpdateParamsForAP(prAdapter, prBssInfo, TRUE);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief It is only for AP mode in NETWORK_TYPE_P2P_INDEX.
 *
 * \param[in]
 *
 * \return if beacon was updated
 */
/*----------------------------------------------------------------------------*/
u_int8_t rlmUpdateParamsForAP(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo,
		u_int8_t fgUpdateBeacon)
{
	struct LINK *prStaList;
	struct STA_RECORD *prStaRec;
	u_int8_t fgErpProtectMode, fgSta40mIntolerant;
	u_int8_t fgUseShortPreamble, fgUseShortSlotTime;
	enum ENUM_HT_PROTECT_MODE eHtProtectMode;
	enum ENUM_GF_MODE eGfOperationMode;
	uint8_t ucHtOpInfo1;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	if (!IS_BSS_ACTIVE(prBssInfo)
		|| prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT)
		return FALSE;

	fgErpProtectMode = FALSE;
	eHtProtectMode = HT_PROTECT_MODE_NONE;
	eGfOperationMode = GF_MODE_NORMAL;
	fgSta40mIntolerant = FALSE;
	fgUseShortPreamble = prBssInfo->fgIsShortPreambleAllowed;
	fgUseShortSlotTime = TRUE;
	ucHtOpInfo1 = (uint8_t) CHNL_EXT_SCN;

	prStaList = &prBssInfo->rStaRecOfClientList;

	LINK_FOR_EACH_ENTRY(prStaRec,
		prStaList, rLinkEntry, struct STA_RECORD) {
		if (!prStaRec) {
			DBGLOG(P2P, WARN,
				"NULL STA_REC ptr in BSS client list\n");
			bssDumpClientList(prAdapter, prBssInfo);
			break;
		}

		if (prStaRec->fgIsInUse
			&& prStaRec->ucStaState == STA_STATE_3
			&& prStaRec->ucBssIndex == prBssInfo->ucBssIndex) {
			if (!(prStaRec->ucPhyTypeSet
				& (PHY_TYPE_SET_802_11GN
				| PHY_TYPE_SET_802_11A))) {
				/* B-only mode, so mode 1 (ERP protection) */
				fgErpProtectMode = TRUE;
			}

			if (!(prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)) {
				/* BG-only or A-only */
				eHtProtectMode = HT_PROTECT_MODE_NON_HT;
			} else if (prBssInfo->fg40mBwAllowed &&
				!(prStaRec->u2HtCapInfo
				& HT_CAP_INFO_SUP_CHNL_WIDTH)) {
				/* 20MHz-only */
				if (eHtProtectMode == HT_PROTECT_MODE_NONE)
					eHtProtectMode = HT_PROTECT_MODE_20M;
			}

			if (!(prStaRec->u2HtCapInfo & HT_CAP_INFO_HT_GF))
				eGfOperationMode = GF_MODE_PROTECT;

			if (!(prStaRec->u2CapInfo & CAP_INFO_SHORT_PREAMBLE))
				fgUseShortPreamble = FALSE;
#if 1
			/* ap mode throughput enhancement
			 * only 2.4G with B mode client connecion
			 * use long slot time
			 */
			if ((!(prStaRec->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME))
					&& fgErpProtectMode
					&& prBssInfo->eBand == BAND_2G4)
				fgUseShortSlotTime = FALSE;
#else
			if (!(prStaRec->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME))
				fgUseShortSlotTime = FALSE;
#endif
			if (prStaRec->u2HtCapInfo & HT_CAP_INFO_40M_INTOLERANT)
				fgSta40mIntolerant = TRUE;
		}
	}			/* end of LINK_FOR_EACH_ENTRY */

	/* Check if HT operation IE
	 * about 20/40M bandwidth shall be updated
	 */
	if (prBssInfo->eBssSCO != CHNL_EXT_SCN) {
		if (/*!LINK_IS_EMPTY(prStaList) && */ !fgSta40mIntolerant &&
		    !prBssInfo->fgObssActionForcedTo20M
		    && !prBssInfo->fgObssBeaconForcedTo20M) {

			ucHtOpInfo1 = (uint8_t)
				(((uint32_t) prBssInfo->eBssSCO)
					| HT_OP_INFO1_STA_CHNL_WIDTH);
		}
	}

	/* Check if any new parameter may be updated */
	if (prBssInfo->fgErpProtectMode != fgErpProtectMode ||
	    prBssInfo->eHtProtectMode != eHtProtectMode ||
	    prBssInfo->eGfOperationMode != eGfOperationMode ||
	    prBssInfo->ucHtOpInfo1 != ucHtOpInfo1 ||
	    prBssInfo->fgUseShortPreamble != fgUseShortPreamble ||
	    prBssInfo->fgUseShortSlotTime != fgUseShortSlotTime) {

		prBssInfo->fgErpProtectMode = fgErpProtectMode;
		prBssInfo->eHtProtectMode = eHtProtectMode;
		prBssInfo->eGfOperationMode = eGfOperationMode;
		prBssInfo->ucHtOpInfo1 = ucHtOpInfo1;
		prBssInfo->fgUseShortPreamble = fgUseShortPreamble;
		prBssInfo->fgUseShortSlotTime = fgUseShortSlotTime;

		if (fgUseShortSlotTime)
			prBssInfo->u2CapInfo |= CAP_INFO_SHORT_SLOT_TIME;
		else
			prBssInfo->u2CapInfo &= ~CAP_INFO_SHORT_SLOT_TIME;

		rlmSyncOperationParams(prAdapter, prBssInfo);
		fgUpdateBeacon = TRUE;
	}

	/* Update Beacon content if related IE content is changed */
	if (fgUpdateBeacon)
		bssUpdateBeaconContent(prAdapter, prBssInfo->ucBssIndex);

	return fgUpdateBeacon;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
enum ENUM_CHNL_EXT rlmDecideScoForAP(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	struct DOMAIN_SUBBAND_INFO *prSubband;
	struct DOMAIN_INFO_ENTRY *prDomainInfo;
	uint8_t ucSecondChannel, i, j;
	enum ENUM_CHNL_EXT eSCO;
	enum ENUM_CHNL_EXT eTempSCO;
	/*chip capability*/
	uint8_t ucMaxBandwidth = MAX_BW_80_80_MHZ;

	eSCO = CHNL_EXT_SCN;
	eTempSCO = CHNL_EXT_SCN;

	if (prBssInfo->eBand == BAND_2G4) {
		if (prBssInfo->ucPrimaryChannel != 14)
			eSCO = (prBssInfo->ucPrimaryChannel > 7)
				? CHNL_EXT_SCB : CHNL_EXT_SCA;
	} else {
		if (regd_is_single_sku_en()) {
			if (rlmDomainIsLegalChannel(prAdapter,
					prBssInfo->eBand,
					prBssInfo->ucPrimaryChannel))
				eSCO = rlmSelectSecondaryChannelType(prAdapter,
					prBssInfo->eBand,
					prBssInfo->ucPrimaryChannel);
		} else {
		prDomainInfo = rlmDomainGetDomainInfo(prAdapter);
		ASSERT(prDomainInfo);

		for (i = 0; i < MAX_SUBBAND_NUM; i++) {
			prSubband = &prDomainInfo->rSubBand[i];
			if (prSubband->ucBand == prBssInfo->eBand) {
				for (j = 0; j < prSubband->ucNumChannels; j++) {
					if ((prSubband->ucFirstChannelNum
						+ j * prSubband->ucChannelSpan)
					    == prBssInfo->ucPrimaryChannel) {
						eSCO = (j & 1)
							? CHNL_EXT_SCB
							: CHNL_EXT_SCA;
						break;
					}
				}

				if (j < prSubband->ucNumChannels)
					break;	/* Found */
			}
		}
	}
	}

	/* Check if it is boundary channel
	 * and 40MHz BW is permitted
	 */
	if (eSCO != CHNL_EXT_SCN) {
		ucSecondChannel = (eSCO == CHNL_EXT_SCA)
			? (prBssInfo->ucPrimaryChannel + CHNL_SPAN_20)
			: (prBssInfo->ucPrimaryChannel - CHNL_SPAN_20);

		if (!rlmDomainIsLegalChannel(prAdapter,
			prBssInfo->eBand,
			ucSecondChannel))
			eSCO = CHNL_EXT_SCN;
	}

	/* Overwrite SCO settings by wifi cfg */
	if (IS_BSS_P2P(prBssInfo)) {
		/* AP mode */
		if (IS_BSS_AP(prAdapter, prBssInfo)) {
			if (prAdapter->rWifiVar.ucApSco == CHNL_EXT_SCA
				|| prAdapter->rWifiVar.ucApSco == CHNL_EXT_SCB)
				eTempSCO =
					(enum ENUM_CHNL_EXT)
						prAdapter->rWifiVar.ucApSco;
#if ((CFG_SUPPORT_TWT == 1) && (CFG_SUPPORT_TWT_HOTSPOT == 1))
				prBssInfo->twt_flow_id_bitmap = 0;
				prBssInfo->aeTWTRespState = 0;
				LINK_INITIALIZE(&prBssInfo->twt_sch_link);

				for (i = 0; i < TWT_MAX_FLOW_NUM; i++)
					prBssInfo->arTWTSta[i].agrt_tbl_idx = i;

				DBGLOG(RLM, INFO,
					"WLAN AP BSS_INFO[%d] TWT flow id bitmap=%d\n",
					prBssInfo->ucBssIndex,
					prBssInfo->twt_flow_id_bitmap);
#endif
		}
		/* P2P mode */
		else {
			if (prAdapter->rWifiVar.ucP2pGoSco == CHNL_EXT_SCA ||
			    prAdapter->rWifiVar.ucP2pGoSco == CHNL_EXT_SCB) {
				eTempSCO =
					(enum ENUM_CHNL_EXT)
						prAdapter->rWifiVar.ucP2pGoSco;
			}
		}

		/* Check again if it is boundary channel
		 * and 40MHz BW is permitted
		 */
		if (eTempSCO != CHNL_EXT_SCN) {
			ucSecondChannel = (eTempSCO == CHNL_EXT_SCA)
				? (prBssInfo->ucPrimaryChannel + 4)
				: (prBssInfo->ucPrimaryChannel - 4);
			if (rlmDomainIsLegalChannel(prAdapter,
				prBssInfo->eBand,
				ucSecondChannel))
				eSCO = eTempSCO;
		}
	}

	/* Overwrite SCO settings by wifi cfg bandwidth setting */
	if (IS_BSS_P2P(prBssInfo)) {
		/* AP mode */
		if (IS_BSS_AP(prAdapter, prBssInfo)) {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth =
					prAdapter->rWifiVar.ucAp2gBandwidth;
			else if (prBssInfo->eBand == BAND_5G)
				ucMaxBandwidth =
					prAdapter->rWifiVar.ucAp5gBandwidth;
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prBssInfo->eBand == BAND_6G)
				ucMaxBandwidth =
					prAdapter->rWifiVar.ucAp6gBandwidth;
#endif
		}
		/* P2P mode */
		else {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth =
					prAdapter->rWifiVar.ucP2p2gBandwidth;
			else if (prBssInfo->eBand == BAND_5G)
				ucMaxBandwidth =
					prAdapter->rWifiVar.ucP2p5gBandwidth;
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prBssInfo->eBand == BAND_6G)
				ucMaxBandwidth =
					prAdapter->rWifiVar.ucP2p6gBandwidth;
#endif
		}

		if (ucMaxBandwidth < MAX_BW_40MHZ)
			eSCO = CHNL_EXT_SCN;
	}

	return eSCO;
}

enum ENUM_CHNL_EXT rlmGetScoByChnInfo(struct ADAPTER *prAdapter,
		struct RF_CHANNEL_INFO *prChannelInfo)
{
	enum ENUM_CHNL_EXT eSCO = CHNL_EXT_SCN;
	int32_t i4DeltaBw;
	uint32_t u4AndOneSCO;

	if (prChannelInfo->ucChnlBw == MAX_BW_40MHZ) {
		/* If BW 40, compare S0 and primary channel freq */
		if (prChannelInfo->u4CenterFreq1
			> prChannelInfo->u2PriChnlFreq)
			eSCO = CHNL_EXT_SCA;
		else
			eSCO = CHNL_EXT_SCB;
	} else if (prChannelInfo->ucChnlBw > MAX_BW_40MHZ) {
		/* P: PriChnlFreq,
		 * A: CHNL_EXT_SCA,
		 * B: CHNL_EXT_SCB, -:BW SPAN 5M
		 */
		/* --|----|--CenterFreq1--|----|-- */
		/* --|----|--CenterFreq1--B----P-- */
		/* --|----|--CenterFreq1--P----A-- */
		i4DeltaBw = prChannelInfo->u2PriChnlFreq
			- prChannelInfo->u4CenterFreq1;
		u4AndOneSCO = CHNL_EXT_SCB;
		eSCO = CHNL_EXT_SCA;
		if (i4DeltaBw < 0) {
			/* --|----|--CenterFreq1--|----|-- */
			/* --P----A--CenterFreq1--|----|-- */
			/* --B----P--CenterFreq1--|----|-- */
			u4AndOneSCO = CHNL_EXT_SCA;
			eSCO = CHNL_EXT_SCB;
			i4DeltaBw = -i4DeltaBw;
		}
		i4DeltaBw = i4DeltaBw - (CHANNEL_SPAN_20 >> 1);
		if ((i4DeltaBw/CHANNEL_SPAN_20) & 1)
			eSCO = u4AndOneSCO;
	}

	return eSCO;
}

static enum ENUM_CHNL_EXT rlmGetSco(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	enum ENUM_CHNL_EXT eSCO = CHNL_EXT_SCN;
	int32_t i4DeltaBw;
	uint32_t u4AndOneSCO;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;

	prP2pRoleFsmInfo = p2pFuncGetRoleByBssIdx(prAdapter,
		prBssInfo->ucBssIndex);

	if (prP2pRoleFsmInfo) {

		prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
		eSCO = CHNL_EXT_SCN;

		if (cnmGetBssMaxBw(prAdapter,
			prBssInfo->ucBssIndex) == MAX_BW_40MHZ) {
			/* If BW 40, compare S0 and primary channel freq */
			if (prP2pConnReqInfo->u4CenterFreq1
				> prP2pConnReqInfo->u2PriChnlFreq)
				eSCO = CHNL_EXT_SCA;
			else
				eSCO = CHNL_EXT_SCB;
		} else if (cnmGetBssMaxBw(prAdapter,
			prBssInfo->ucBssIndex) > MAX_BW_40MHZ) {
			/* P: PriChnlFreq,
			 * A:CHNL_EXT_SCA,
			 * B: CHNL_EXT_SCB, -:BW SPAN 5M
			 */
			/* --|----|--CenterFreq1--|----|-- */
			/* --|----|--CenterFreq1--B----P-- */
			/* --|----|--CenterFreq1--P----A-- */
			i4DeltaBw = prP2pConnReqInfo->u2PriChnlFreq
				- prP2pConnReqInfo->u4CenterFreq1;
			u4AndOneSCO = CHNL_EXT_SCB;
			eSCO = CHNL_EXT_SCA;
			if (i4DeltaBw < 0) {
				/* --|----|--CenterFreq1--|----|-- */
				/* --P----A--CenterFreq1--|----|-- */
				/* --B----P--CenterFreq1--|----|-- */
				u4AndOneSCO = CHNL_EXT_SCA;
				eSCO = CHNL_EXT_SCB;
				i4DeltaBw = -i4DeltaBw;
			}
			i4DeltaBw = i4DeltaBw - (CHANNEL_SPAN_20 >> 1);
			if ((i4DeltaBw/CHANNEL_SPAN_20) & 1)
				eSCO = u4AndOneSCO;
		}
	}

	return eSCO;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief: Get AP secondary channel offset from cfg80211 or wifi.cfg
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T, prBssInfo Pointer of BSS_INFO_T,
 *
 * \return ENUM_CHNL_EXT_T AP secondary channel offset
 */
/*----------------------------------------------------------------------------*/
enum ENUM_CHNL_EXT rlmGetScoForAP(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	enum ENUM_BAND eBand;
	uint8_t ucChannel;
	enum ENUM_CHNL_EXT eSCO;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;

	prP2pRoleFsmInfo = p2pFuncGetRoleByBssIdx(prAdapter,
		prBssInfo->ucBssIndex);

	if (!prAdapter->rWifiVar.ucApChnlDefFromCfg
		&& prP2pRoleFsmInfo) {
		eSCO = rlmGetSco(prAdapter, prBssInfo);
	} else {
		/* In this case, the first BSS's SCO is 40MHz
		 * and known, so AP can apply 40MHz bandwidth,
		 * but the first BSS's SCO may be changed
		 * later if its Beacon lost timeout occurs
		 */
		if (!(cnmPreferredChannel(prAdapter,
			&eBand, &ucChannel, &eSCO)
			&& eSCO != CHNL_EXT_SCN
			&& ucChannel == prBssInfo->ucPrimaryChannel
			&& eBand == prBssInfo->eBand))
			eSCO = rlmDecideScoForAP(prAdapter, prBssInfo);
	}
	return eSCO;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief: Get AP channel number of Channel Center Frequency Segment 0
 *           from cfg80211 or wifi.cfg
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T, prBssInfo Pointer of BSS_INFO_T,
 *
 * \return UINT_8 AP channel number of Channel Center Frequency Segment 0
 */
/*----------------------------------------------------------------------------*/
uint8_t rlmGetVhtS1ForAP(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo)
{
	uint32_t ucFreq1Channel;

	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;

	prP2pRoleFsmInfo =
		p2pFuncGetRoleByBssIdx(prAdapter, prBssInfo->ucBssIndex);

	if (prBssInfo->ucVhtChannelWidth == VHT_OP_CHANNEL_WIDTH_20_40)
		return 0;

	if (!prAdapter->rWifiVar.ucApChnlDefFromCfg && prP2pRoleFsmInfo) {
		prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
		ucFreq1Channel =
			nicFreq2ChannelNum(
				prP2pConnReqInfo->u4CenterFreq1 * 1000);
	} else {
		ucFreq1Channel = nicGetS1(
			prBssInfo->eBand,
			prBssInfo->ucPrimaryChannel,
			prBssInfo->eBssSCO,
			rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo));
	}

	return ucFreq1Channel;
}

void rlmGetChnlInfoForCSA(struct ADAPTER *prAdapter,
	enum ENUM_BAND eBand,
	uint8_t ucCh,
	uint8_t ucBssIdx,
	struct RF_CHANNEL_INFO *prRfChnlInfo)
{
	struct BSS_INFO *prBssInfo = NULL;
	enum ENUM_BAND eBandOrig, eBandCsa;
	enum ENUM_CHNL_EXT eScoCsa;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	prRfChnlInfo->ucChannelNum = ucCh;
	eScoCsa = nicGetSco(prAdapter, eBand, ucCh);

	eBandCsa = eBand;
	prRfChnlInfo->eBand = eBandCsa;

	/* temp replace BSS eBand to get BW of CSA band */
	eBandOrig = prBssInfo->eBand;
	prBssInfo->eBand = eBandCsa;
	prRfChnlInfo->ucChnlBw = cnmGetBssMaxBw(prAdapter, ucBssIdx);
	prRfChnlInfo->eSco = eScoCsa;
#if (CFG_SUPPORT_802_11BE == 1)
	if ((!(prBssInfo->ucPhyTypeSet &
		PHY_TYPE_BIT_EHT)) &&
		(prRfChnlInfo->ucChnlBw >=
		MAX_BW_320_1MHZ))
		prRfChnlInfo->ucChnlBw = MAX_BW_160MHZ;
#endif
	prBssInfo->eBand = eBandOrig; /* Restore BSS eBand */

	prRfChnlInfo->u2PriChnlFreq =
		nicChannelNum2Freq(ucCh, eBandCsa) / 1000;
	prRfChnlInfo->u4CenterFreq1 = nicGetS1Freq(eBandCsa, ucCh, eScoCsa,
		prRfChnlInfo->ucChnlBw);
	prRfChnlInfo->u4CenterFreq2 = nicGetS2Freq(eBandCsa, ucCh,
		prRfChnlInfo->ucChnlBw);

	if ((eBand == BAND_5G) &&
		(ucCh >= 52 && ucCh <= 144))
		prRfChnlInfo->fgDFS = TRUE;
	else
		prRfChnlInfo->fgDFS = FALSE;
}

#if (CFG_SUPPORT_SAP_PUNCTURE == 1)
static void rlmPunctUpdateLegacyBw80(uint16_t u2Bitmap, uint8_t ucPriChannel,
				     uint8_t *pucSeg0)
{
	uint8_t ucFirstChan = *pucSeg0 - 6, ucSecChan;

	switch (u2Bitmap) {
	case 0x6:
		*pucSeg0 = 0;
		return;
	case 0x8:
	case 0x4:
	case 0x2:
	case 0x1:
	case 0xC:
	case 0x3:
		if (ucPriChannel < *pucSeg0)
			*pucSeg0 -= 4;
		else
			*pucSeg0 += 4;
		break;
	}

	if (ucPriChannel < *pucSeg0)
		ucSecChan = ucPriChannel + 4;
	else
		ucSecChan = ucPriChannel - 4;

	if (u2Bitmap & BIT((ucSecChan - ucFirstChan) / 4))
		*pucSeg0 = 0;
}

static void rlmPunctUpdateLegacyBw160(uint16_t u2Bitmap, uint8_t ucPriChannel,
				      uint8_t *pucBw, uint8_t *pucSeg0)
{
	if (ucPriChannel < *pucSeg0) {
		*pucSeg0 -= 8;
		if (u2Bitmap & 0x0F) {
			*pucBw = MAX_BW_40MHZ;
			rlmPunctUpdateLegacyBw80(u2Bitmap & 0xF, ucPriChannel,
						 pucSeg0);
		}
	} else {
		*pucSeg0 += 8;
		if (u2Bitmap & 0xF0) {
			*pucBw = MAX_BW_40MHZ;
			rlmPunctUpdateLegacyBw80((u2Bitmap & 0xF0) >> 4,
						 ucPriChannel, pucSeg0);
		}
	}
}

void rlmPunctUpdateLegacyBw(enum ENUM_BAND eBand, uint16_t u2Bitmap,
			    uint8_t ucPriChannel, uint8_t *pucBw,
			    uint8_t *pucSeg0, uint8_t *pucSeg1,
			    uint8_t *pucOpClass)
{
	uint8_t ucCenterCh, ucSecCh;

	if (*pucBw < MAX_BW_80MHZ || *pucBw > MAX_BW_80_80_MHZ)
		return;

	switch (*pucBw) {
	case MAX_BW_80MHZ:
	case MAX_BW_80_80_MHZ:
		ucCenterCh = *pucSeg0;
		ucSecCh = *pucSeg1;
		break;
	case MAX_BW_160MHZ:
		ucCenterCh = *pucSeg1;
		ucSecCh = 0;
		break;
	default:
		return;
	}

	if ((*pucBw == MAX_BW_80MHZ || *pucBw == MAX_BW_80_80_MHZ) &&
	    (u2Bitmap & 0xF)) {
		*pucBw = MAX_BW_40MHZ;
		rlmPunctUpdateLegacyBw80(u2Bitmap & 0xF, ucPriChannel,
					 &ucCenterCh);
	}

	if (*pucBw == MAX_BW_160MHZ && (u2Bitmap & 0xFF)) {
		*pucBw = MAX_BW_80MHZ;
		ucSecCh = 0;
		rlmPunctUpdateLegacyBw160(u2Bitmap & 0xFF, ucPriChannel,
					  pucBw, &ucCenterCh);
	}

	*pucSeg0 = nicGetS1(eBand, ucPriChannel, CHNL_EXT_RES, *pucBw);
	*pucSeg1 = nicGetS2(eBand, ucPriChannel, *pucBw);

	if (pucOpClass) {
		struct RF_CHANNEL_INFO rChannelInfo;

		rChannelInfo.eBand = eBand;
		rChannelInfo.ucChnlBw = *pucBw;
		/* Sco no matter for BW > 40 MHz */
		rChannelInfo.eSco = CHNL_EXT_RES;
		rChannelInfo.ucChannelNum = ucPriChannel;
		rChannelInfo.u2PriChnlFreq =
			nicChannelNum2Freq(ucPriChannel,
					   eBand) / 1000;
		rChannelInfo.u4CenterFreq1 =
			nicGetS1Freq(eBand, ucPriChannel,
				     rChannelInfo.eSco, *pucBw);
		rChannelInfo.u4CenterFreq2 =
			nicGetS2Freq(eBand, ucPriChannel, *pucBw);

		*pucOpClass = nicChannelInfo2OpClass(&rChannelInfo);
	}
}

u_int8_t rlmValidatePunctBitmap(struct ADAPTER *prAdapter,
				enum ENUM_BAND eBand,
				enum ENUM_MAX_BANDWIDTH_SETTING eBw,
				uint8_t ucPriCh, uint16_t u2PunctBitmap)
{
	uint8_t ucIdx, ucCount, ucStartCh, ucCenterCh, ucVhtBw;
	uint16_t u2Bitmap, u2PriChBit;
	const uint16_t *prValidBitmaps;

	if (!u2PunctBitmap)
		return TRUE;

	u2Bitmap = ~u2PunctBitmap;
	if (!u2Bitmap)
		return FALSE;

	ucVhtBw = rlmGetVhtOpBwByBssOpBw(eBw);
	ucCenterCh = nicGetS1(eBand, ucPriCh, CHNL_EXT_RES, ucVhtBw);

	switch (eBw) {
	case MAX_BW_80MHZ:
		if (u2PunctBitmap > 0xF || ucCenterCh < 7)
			return FALSE;
		u2Bitmap &= 0xF;
		prValidBitmaps = PUNCT_VALID_BITMAP_80;
		ucCount = ARRAY_SIZE(PUNCT_VALID_BITMAP_80);
		ucStartCh = ucCenterCh - 6;
		break;

	case MAX_BW_160MHZ:
		if (u2PunctBitmap > 0xFF || ucCenterCh < 15)
			return FALSE;
		u2Bitmap &= 0xFF;
		prValidBitmaps = PUNCT_VALID_BITMAP_160;
		ucCount = ARRAY_SIZE(PUNCT_VALID_BITMAP_160);
		ucStartCh = ucCenterCh - 14;
		break;

	case MAX_BW_320_1MHZ:
	case MAX_BW_320_2MHZ:
		if (ucCenterCh < 31)
			return FALSE;
		u2Bitmap &= 0xFFFF;
		prValidBitmaps = PUNCT_VALID_BITMAP_320;
		ucCount = ARRAY_SIZE(PUNCT_VALID_BITMAP_320);
		ucStartCh = ucCenterCh - 30;
		break;

	default:
		return FALSE;
	}

	u2PriChBit = (uint16_t)((ucPriCh - ucStartCh) / 4);

	/* Primary channel cannot be punctured */
	if (!(u2Bitmap & BIT(u2PriChBit)))
		return FALSE;

	for (ucIdx = 0; ucIdx < ucCount; ucIdx++) {
		if (prValidBitmaps[ucIdx] == u2Bitmap)
			return TRUE;
	}

	return FALSE;
}
#endif /* CFG_SUPPORT_SAP_PUNCTURE */

#endif /* CFG_ENABLE_WIFI_DIRECT */
