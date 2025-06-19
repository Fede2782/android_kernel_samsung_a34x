/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_log.c
 */

/*! \file   "nan_ext_log.c"
 *  \brief  This file defines the log patterns that indicate the procedure
 *          of NDP setup.
 *
 *    Detail description.
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
#include "gl_os.h"
#include "gl_kal.h"
#include "gl_rst.h"
#if (CFG_EXT_VERSION == 1)
#include "gl_sys.h"
#endif
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"
#include <linux/rtc.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/net.h>

#include "nan_ext.h"
#include "nan_ext_ccm.h"
#include "nan_ext_pa.h"
#include "nan_ext_mdc.h"
#include "nan_ext_asc.h"
#include "nan_ext_amc.h"
#include "nan_ext_ascc.h"
#include "nan_ext_fr.h"
#include "nan_ext_adsdc.h"
#include "nan_ext_eht.h"
#include "nan_ext_log.h"

#if CFG_SUPPORT_NAN

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

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if (CFG_SUPPORT_NAN_LOG == 1)
uint32_t
nanSchedDbgDumpCommittedSlotAndChannel(struct ADAPTER *prAdapter,
				       const char *pucEvent) {
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Idx = 0;
	uint32_t u4Idx1 = 0;
	uint8_t ucTimeLineIdx = 0;
	struct _NAN_TIMELINE_MGMT_T *prNanTimelineMgmt = NULL;
	struct _NAN_CHANNEL_TIMELINE_T *prChnlTimeline = NULL;
	uint8_t aucBinStr[4][9] = {0};

	struct _NAN_SCHEDULER_T *prScheduler = NULL;
	struct _NAN_ATTR_DEVICE_CAPABILITY_T *prAttrDevCap = NULL;

	/* NAN_CHK_PNT log message */
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDL_SCHEDULE event=%s\n",
		   pucEvent);

	for (ucTimeLineIdx = 0;
	     ucTimeLineIdx < NAN_TIMELINE_MGMT_SIZE; ucTimeLineIdx++) {
		prNanTimelineMgmt =
			nanGetTimelineMgmt(prAdapter, ucTimeLineIdx);

		for (u4Idx = 0;
			u4Idx < NAN_TIMELINE_MGMT_CHNL_LIST_NUM; u4Idx++) {
			prChnlTimeline = &prNanTimelineMgmt->arChnlList[u4Idx];
			if (prChnlTimeline->fgValid == FALSE)
				continue;

			for (u4Idx1 = 0; u4Idx1 < 32; u4Idx1++) {
				if (prChnlTimeline->au4AvailMap[0] &
				    BIT(u4Idx1))
					aucBinStr[3 - (u4Idx1 / 8)]
						 [u4Idx1 % 8] = '1';
				else
					aucBinStr[3 - (u4Idx1 / 8)]
						 [u4Idx1 % 8] = '0';
			}
			for (u4Idx1 = 0; u4Idx1 < 4; u4Idx1++)
				aucBinStr[u4Idx1][8] = '\0';

			prScheduler = nanGetScheduler(prAdapter);
			prAttrDevCap = &prScheduler->rAttrDevCap;

			/* NAN_CHK_PNT log message */
			NAN_DBGLOG(NAN, INFO,
				   "[NAN_CHK_PNT] NDL_SCHEDULE pri_chnl=%u op_cls=%u chnl_bw=%u phy_mode=%u committed_bitmap=%s_%s_%s_%s\n",
				   prChnlTimeline->rChnlInfo.u4PrimaryChnl,
				   prChnlTimeline->rChnlInfo.u4OperatingClass,
				   nanRegGetBw(prChnlTimeline->rChnlInfo
					.u4OperatingClass),
				   prAttrDevCap->ucOperationMode,
				   aucBinStr[3], aucBinStr[2],
				   aucBinStr[1], aucBinStr[0]);
		}
	}

	return rRetStatus;
}

uint32_t
nanSchedDbgDumpPeerCommittedSlotAndChannel(struct ADAPTER *prAdapter,
					   uint8_t *pucNmiAddr,
					   const char *pucEvent) {
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Idx = 0;
	uint32_t u4Idx1 = 0;
	uint32_t u4Idx2 = 0;
	struct _NAN_AVAILABILITY_DB_T *prNanAvailAttr = NULL;
	struct _NAN_AVAILABILITY_TIMELINE_T *prNanAvailEntry = NULL;
	struct _NAN_PEER_SCH_DESC_T *prPeerSchDesc = NULL;
	struct _NAN_DEVICE_CAPABILITY_T *prNanDevCapa = NULL;
	uint16_t u2EntryControl = 0;
	uint8_t aucBinStr[4][9] = {0};
	uint8_t ucHasPrinted = 0;

	prPeerSchDesc = nanSchedSearchPeerSchDescByNmi(prAdapter, pucNmiAddr);
	if (prPeerSchDesc == NULL)
		return WLAN_STATUS_FAILURE;

	for (u4Idx = 0; u4Idx < NAN_NUM_AVAIL_DB; u4Idx++) {
		prNanAvailAttr = &prPeerSchDesc->arAvailAttr[u4Idx];
		if (prNanAvailAttr->ucMapId == NAN_INVALID_MAP_ID)
			continue;

		for (u4Idx1 = 0; u4Idx1 < NAN_NUM_AVAIL_DB; u4Idx1++) {
			prNanDevCapa = &prPeerSchDesc->arDevCapability[u4Idx1];
			if (prNanDevCapa->fgValid &&
			    prNanDevCapa->ucMapId == prNanAvailAttr->ucMapId)
				break;
		}
		if (u4Idx1 == NAN_NUM_AVAIL_DB) {
			prNanDevCapa = &prPeerSchDesc->arDevCapability[u4Idx1];
			if (!prNanDevCapa->fgValid ||
			    prNanDevCapa->ucMapId != NAN_INVALID_MAP_ID)
				return WLAN_STATUS_FAILURE;
		}

		for (u4Idx1 = 0; u4Idx1 < NAN_NUM_AVAIL_TIMELINE; u4Idx1++) {
			prNanAvailEntry =
				&prNanAvailAttr->arAvailEntryList[u4Idx1];
			if (prNanAvailEntry->fgActive == FALSE)
				continue;

			if (prNanAvailEntry->arBandChnlCtrl[0].u4Type ==
			    NAN_BAND_CH_ENTRY_LIST_TYPE_BAND)
				continue;

			u2EntryControl = prNanAvailEntry->rEntryCtrl.u2RawData;
			if (!NAN_AVAIL_ENTRY_CTRL_COMMITTED(u2EntryControl))
				continue;

			for (u4Idx2 = 0; u4Idx2 < 32; u4Idx2++) {
				if (prNanAvailEntry->au4AvailMap[0] &
				    BIT(u4Idx2))
					aucBinStr[3 - (u4Idx2 / 8)]
						 [u4Idx2 % 8] = '1';
				else
					aucBinStr[3 - (u4Idx2 / 8)]
						 [u4Idx2 % 8] = '0';
			}
			for (u4Idx2 = 0; u4Idx2 < 4; u4Idx2++)
				aucBinStr[u4Idx2][8] = '\0';

			if (ucHasPrinted == 0) {
				/* NAN_CHK_PNT log message */
				NAN_DBGLOG(NAN, INFO,
					   "[NAN_CHK_PNT] NDL_PEER_SCHEDULE event=%s peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
					   pucEvent,
					   prPeerSchDesc->aucNmiAddr[0],
					   prPeerSchDesc->aucNmiAddr[1],
					   prPeerSchDesc->aucNmiAddr[2],
					   prPeerSchDesc->aucNmiAddr[3],
					   prPeerSchDesc->aucNmiAddr[4],
					   prPeerSchDesc->aucNmiAddr[5]);
				ucHasPrinted = 1;
			}

			/* NAN_CHK_PNT log message */
			NAN_DBGLOG(NAN, INFO,
				   "[NAN_CHK_PNT] NDL_PEER_SCHEDULE pri_chnl=%u op_cls=%u chnl_bw=%u phy_mode=%u committed_bitmap=%s_%s_%s_%s\n",
				   prNanAvailEntry->arBandChnlCtrl[0]
					.u4PrimaryChnl,
				   prNanAvailEntry->arBandChnlCtrl[0]
					.u4OperatingClass,
				   nanRegGetBw(prNanAvailEntry->
					arBandChnlCtrl[0].u4OperatingClass),
				   prNanDevCapa->ucOperationMode,
				   aucBinStr[3], aucBinStr[2],
				   aucBinStr[1], aucBinStr[0]);
		}
	}

	return rRetStatus;
}

void nicNanSlotStatisticsEvt(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct _NAN_SCHED_EVENT_SLOT_STATISTICS_T *prSlotStatsEvt;

	prSlotStatsEvt =
		(struct _NAN_SCHED_EVENT_SLOT_STATISTICS_T *)pcuEvtBuf;

	/* NAN_CHK_PNT log message */
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_TX_STATS tx_bytes=%u per_slot_tx_bytes=%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u\n",
		   prSlotStatsEvt->u4TotalSlotTxByte,
		   prSlotStatsEvt->u4SlotTxByte[0],
		   prSlotStatsEvt->u4SlotTxByte[1],
		   prSlotStatsEvt->u4SlotTxByte[2],
		   prSlotStatsEvt->u4SlotTxByte[3],
		   prSlotStatsEvt->u4SlotTxByte[4],
		   prSlotStatsEvt->u4SlotTxByte[5],
		   prSlotStatsEvt->u4SlotTxByte[6],
		   prSlotStatsEvt->u4SlotTxByte[7],
		   prSlotStatsEvt->u4SlotTxByte[8],
		   prSlotStatsEvt->u4SlotTxByte[9],
		   prSlotStatsEvt->u4SlotTxByte[10],
		   prSlotStatsEvt->u4SlotTxByte[11],
		   prSlotStatsEvt->u4SlotTxByte[12],
		   prSlotStatsEvt->u4SlotTxByte[13],
		   prSlotStatsEvt->u4SlotTxByte[14],
		   prSlotStatsEvt->u4SlotTxByte[15],
		   prSlotStatsEvt->u4SlotTxByte[16],
		   prSlotStatsEvt->u4SlotTxByte[17],
		   prSlotStatsEvt->u4SlotTxByte[18],
		   prSlotStatsEvt->u4SlotTxByte[19],
		   prSlotStatsEvt->u4SlotTxByte[20],
		   prSlotStatsEvt->u4SlotTxByte[21],
		   prSlotStatsEvt->u4SlotTxByte[22],
		   prSlotStatsEvt->u4SlotTxByte[23],
		   prSlotStatsEvt->u4SlotTxByte[24],
		   prSlotStatsEvt->u4SlotTxByte[25],
		   prSlotStatsEvt->u4SlotTxByte[26],
		   prSlotStatsEvt->u4SlotTxByte[27],
		   prSlotStatsEvt->u4SlotTxByte[28],
		   prSlotStatsEvt->u4SlotTxByte[29],
		   prSlotStatsEvt->u4SlotTxByte[30],
		   prSlotStatsEvt->u4SlotTxByte[31]);
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_RX_STATS rx_bytes=%u per_slot_rx_bytes=%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u_%u\n",
		   prSlotStatsEvt->u4TotalSlotRxByte,
		   prSlotStatsEvt->u4SlotRxByte[0],
		   prSlotStatsEvt->u4SlotRxByte[1],
		   prSlotStatsEvt->u4SlotRxByte[2],
		   prSlotStatsEvt->u4SlotRxByte[3],
		   prSlotStatsEvt->u4SlotRxByte[4],
		   prSlotStatsEvt->u4SlotRxByte[5],
		   prSlotStatsEvt->u4SlotRxByte[6],
		   prSlotStatsEvt->u4SlotRxByte[7],
		   prSlotStatsEvt->u4SlotRxByte[8],
		   prSlotStatsEvt->u4SlotRxByte[9],
		   prSlotStatsEvt->u4SlotRxByte[10],
		   prSlotStatsEvt->u4SlotRxByte[11],
		   prSlotStatsEvt->u4SlotRxByte[12],
		   prSlotStatsEvt->u4SlotRxByte[13],
		   prSlotStatsEvt->u4SlotRxByte[14],
		   prSlotStatsEvt->u4SlotRxByte[15],
		   prSlotStatsEvt->u4SlotRxByte[16],
		   prSlotStatsEvt->u4SlotRxByte[17],
		   prSlotStatsEvt->u4SlotRxByte[18],
		   prSlotStatsEvt->u4SlotRxByte[19],
		   prSlotStatsEvt->u4SlotRxByte[20],
		   prSlotStatsEvt->u4SlotRxByte[21],
		   prSlotStatsEvt->u4SlotRxByte[22],
		   prSlotStatsEvt->u4SlotRxByte[23],
		   prSlotStatsEvt->u4SlotRxByte[24],
		   prSlotStatsEvt->u4SlotRxByte[25],
		   prSlotStatsEvt->u4SlotRxByte[26],
		   prSlotStatsEvt->u4SlotRxByte[27],
		   prSlotStatsEvt->u4SlotRxByte[28],
		   prSlotStatsEvt->u4SlotRxByte[29],
		   prSlotStatsEvt->u4SlotRxByte[30],
		   prSlotStatsEvt->u4SlotRxByte[31]);

}

void nanLogNdiInfo(struct _NAN_NDP_INSTANCE_T *prNDP)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_NDI peer_ndi_addr=%02x:%02x:%02x:%02x:%02x:%02x own_ndi_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   prNDP->aucPeerNDIAddr[0],
		   prNDP->aucPeerNDIAddr[1],
		   prNDP->aucPeerNDIAddr[2],
		   prNDP->aucPeerNDIAddr[3],
		   prNDP->aucPeerNDIAddr[4],
		   prNDP->aucPeerNDIAddr[5],
		   prNDP->aucLocalNDIAddr[0],
		   prNDP->aucLocalNDIAddr[1],
		   prNDP->aucLocalNDIAddr[2],
		   prNDP->aucLocalNDIAddr[3],
		   prNDP->aucLocalNDIAddr[4],
		   prNDP->aucLocalNDIAddr[5]);
}

const char *nanReasonCodeString(uint8_t reason_code)
{
	static const char * const reason_code_str[] = {
		[NAN_REASON_CODE_RESERVED] = "Reserved",
		[NAN_REASON_CODE_UNSPECIFIED] = "Unspecified",
		[NAN_REASON_CODE_RESOURCE_LIMITATION] = "Resource_Limitation",
		[NAN_REASON_CODE_INVALID_PARAMS] = "Invalid_Params",
		[NAN_REASON_CODE_FTM_PARAMS_INCAPABLE] = "Ftm_Params_Incapable",
		[NAN_REASON_CODE_NO_MOVEMENT] = "No_Movement",
		[NAN_REASON_CODE_INVALID_AVAILABILITY] = "Invalid_Availability",
		[NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE] =
			"Immutable_Unacceptable",
		[NAN_REASON_CODE_SECURITY_POLICY] = "Security_Policy",
		[NAN_REASON_CODE_QOS_UNACCEPTABLE] = "Qos_Unacceptable",
		[NAN_REASON_CODE_NDP_REJECTED] = "Ndp_Rejected",
		[NAN_REASON_CODE_NDL_UNACCEPTABLE] = "Ndl_Unacceptable",
		[NAN_REASON_CODE_RANGING_SCHEDULE_UNACCEPTABLE] =
			"Ranging_Schedule_Unacceptable",
		[NAN_REASON_CODE_RANGING_BOOTSTRAPPING_REJECTED] =
			"Ranging_Bootstrapping_Rejected",
	};

	if (unlikely(reason_code >= ARRAY_SIZE(reason_code_str)))
		return "Undefined";
	else
		return reason_code_str[reason_code];
}

void nanLogFailRxReqStr(char *pucReasonStr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_REQ reason=%s\n",
		   pucReasonStr);
}

void nanLogFailRxReq(uint8_t ucReasonCode)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_REQ reason=%s\n",
		   nanReasonCodeString(ucReasonCode));
}

void nanLogFailRxRespStr(char *pucReasonStr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_RESP reason=%s\n",
		   pucReasonStr);
}

void nanLogFailRxResp(uint8_t ucReasonCode)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_RESP reason=%s\n",
		   nanReasonCodeString(ucReasonCode));
}

void nanLogFailRxConfmStr(char *pucReasonStr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_CONFIRM reason=%s\n",
		   pucReasonStr);
}

void nanLogFailRxConfm(uint8_t ucReasonCode)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_CONFIRM reason=%s\n",
		   nanReasonCodeString(ucReasonCode));
}

void nanLogFailRxKeyInstlStr(char *pucReasonStr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_KEY_INSTALL reason=%s\n",
		   pucReasonStr);
}

void nanLogFailRxKeyInstl(uint8_t ucReasonCode)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_FAIL_AT_RX_KEY_INSTALL reason=%s\n",
		   nanReasonCodeString(ucReasonCode));
}

void nanLogNdpFinlConfm(char *pucStatusStr,
			struct _NAN_NDL_INSTANCE_T *prNDL,
			struct _NAN_NDP_INSTANCE_T *prNDP)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_NDP_CONFIRM status=%s peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x peer_ndi_addr=%02x:%02x:%02x:%02x:%02x:%02x ndp_id=%u ndp_num=%u\n",
		   pucStatusStr,
		   prNDL->aucPeerMacAddr[0],
		   prNDL->aucPeerMacAddr[1],
		   prNDL->aucPeerMacAddr[2],
		   prNDL->aucPeerMacAddr[3],
		   prNDL->aucPeerMacAddr[4],
		   prNDL->aucPeerMacAddr[5],
		   prNDP->aucPeerNDIAddr[0],
		   prNDP->aucPeerNDIAddr[1],
		   prNDP->aucPeerNDIAddr[2],
		   prNDP->aucPeerNDIAddr[3],
		   prNDP->aucPeerNDIAddr[4],
		   prNDP->aucPeerNDIAddr[5],
		   prNDP->ucNDPID,
		   prNDL->ucNDPNum);
}

void nanLogTx(struct _NAN_ACTION_FRAME_T *prNAF)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_TX type=%s peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   nanActionFrameOuiString(prNAF->ucOUISubtype),
		   prNAF->aucDestAddr[0], prNAF->aucDestAddr[1],
		   prNAF->aucDestAddr[2], prNAF->aucDestAddr[3],
		   prNAF->aucDestAddr[4], prNAF->aucDestAddr[5]);
}

void nanLogTxDone(uint8_t ucNafOui, uint8_t ucTxDoneStatus)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_TX_DONE type=%s tx_status=%s\n",
		   nanActionFrameOuiString(ucNafOui),
		   nanTxDoneStatusString(ucTxDoneStatus));
}

void nanLogTxAndTxDoneFollowup(const char *pucTypeStr,
			       struct NAN_FOLLOW_UP_EVENT *prFollowupEvt)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_TX_AND_TX_DONE type=%s peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x tx_status=%s\n",
		   pucTypeStr,
		   prFollowupEvt->addr[0], prFollowupEvt->addr[1],
		   prFollowupEvt->addr[2], prFollowupEvt->addr[3],
		   prFollowupEvt->addr[4], prFollowupEvt->addr[5],
		   nanTxDoneStatusString(prFollowupEvt->tx_status));
}

void nanLogRx(uint8_t ucNafOui, uint8_t *pucSrcMacAddr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_RX type=%s peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   nanActionFrameOuiString(ucNafOui),
		   pucSrcMacAddr[0], pucSrcMacAddr[1],
		   pucSrcMacAddr[2], pucSrcMacAddr[3],
		   pucSrcMacAddr[4], pucSrcMacAddr[5]);
}

void nanLogPublish(uint16_t u2PublishId)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_NEW_PUBLISH publish_id/handle=%u\n",
		   u2PublishId);
}

void nanLogSubscribe(uint16_t u2SubscribeId)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_NEW_SUBSCRIBE subscribe_id/handle=%u\n",
		   u2SubscribeId);
}

void nanLogClusterMac(uint8_t *pucOwnMacAddr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_START_CLUSTER new_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x (NMI)\n",
		   pucOwnMacAddr[0], pucOwnMacAddr[1],
		   pucOwnMacAddr[2], pucOwnMacAddr[3],
		   pucOwnMacAddr[4], pucOwnMacAddr[5]);
}

void nanLogClusterId(uint8_t *pucClusterId)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_START_CLUSTER cluster_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   pucClusterId[0], pucClusterId[1],
		   pucClusterId[2], pucClusterId[3],
		   pucClusterId[4], pucClusterId[5]);
}

void nanLogJoinCluster(uint8_t *pucClusterId)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_JOIN_CLUSTER cluster_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   pucClusterId[0], pucClusterId[1],
		   pucClusterId[2], pucClusterId[3],
		   pucClusterId[4], pucClusterId[5]);
}

void nanLogMatch(uint8_t *pucPeerMacAddr)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NAN_NEW_MATCH_EVENT peer_mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   pucPeerMacAddr[0], pucPeerMacAddr[1],
		   pucPeerMacAddr[2], pucPeerMacAddr[3],
		   pucPeerMacAddr[4], pucPeerMacAddr[5]);
}

void nanLogEndReason(char *pucTerminationReason)
{
	NAN_DBGLOG(NAN, INFO,
		   "[NAN_CHK_PNT] NDP_TERMINATION reason=%s\n",
		   pucTerminationReason);
}

const char *nanTxDoneStatusString(uint8_t status_code)
{
	static const char * const status_code_str[] = {
		[TX_RESULT_SUCCESS] = "Success",
		[TX_RESULT_LIFE_TIMEOUT] = "Life_Timeout",
		[TX_RESULT_RTS_ERROR] = "Rts_Error",
		[TX_RESULT_MPDU_ERROR] = "Mpdu_Error",
		[TX_RESULT_AGING_TIMEOUT] = "Aging_Timeout",
		[TX_RESULT_FLUSHED] = "Flushed",
		[TX_RESULT_BIP_ERROR] = "Bip_Error",
		[TX_RESULT_UNSPECIFIED_ERROR] = "Unspecified_Error",
	};

	if (unlikely(status_code >= ARRAY_SIZE(status_code_str)))
		return "Undefined";
	else
		return status_code_str[status_code];
}

#endif /* CFG_SUPPORT_NAN_LOG */

#endif /* CFG_SUPPORT_NAN */

