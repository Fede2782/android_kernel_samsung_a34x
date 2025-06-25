/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext.c
 */

/*! \file   "nan_ext.c"
 *  \brief  This file defines the interface which can interact with users
 *          in /sys fs.
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

#if CFG_SUPPORT_NAN_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define NAN_INFO_BUF_SIZE (1024)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

#if 0
#if WLAN_INCLUDE_SYS
extern struct kobject *wifi_kobj;
#else
static struct kobject *wifi_kobj;
#endif

static char acNanInfo[NAN_INFO_BUF_SIZE] = {0};

#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

uint8_t g_ucNanIsOn;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
const char *oui_str(uint8_t i)
{
	static const char * const oui_string[] = {
		[NAN_CCM_CMD_OUI   - NAN_EXT_CMD_BASE] = "CCM command",
		[NAN_CCM_EVT_OUI   - NAN_EXT_CMD_BASE] = "CCM event",

		[NAN_PA_CMD_OUI    - NAN_EXT_CMD_BASE] = "PA command",
		[NAN_MDC_CMD_OUI   - NAN_EXT_CMD_BASE] = "MDC command",
		[NAN_ASC_CMD_OUI   - NAN_EXT_CMD_BASE] = "ASC command",
		[NAN_AMC_CMD_OUI   - NAN_EXT_CMD_BASE] = "AMC command",
		[NAN_ASCC_CMD_OUI  - NAN_EXT_CMD_BASE] = "AScC command",
		[NAN_FR_EVT_OUI    - NAN_EXT_CMD_BASE] = "Fast Recovery event",
		[NAN_EHT_CMD_OUI   - NAN_EXT_CMD_BASE] = "EHT command",
		[NAN_ADSDC_CMD_OUI - NAN_EXT_CMD_BASE] = "ADSDC command",
		[NAN_ADSDC_TSF_OUI - NAN_EXT_CMD_BASE] = "ADSDC TSF",
	};
	uint8_t ucIndex = i - NAN_EXT_CMD_BASE;

	if  (ucIndex < ARRAY_SIZE(oui_string))
		return oui_string[ucIndex];

	return "Invalid OUI";
}

static uint32_t (* const nanExtCmdHandler[])
	(struct ADAPTER *, const uint8_t *, size_t) = {
	[NAN_CCM_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessCCM,   /* 0x34 */
	[NAN_CCM_EVT_OUI   - NAN_EXT_CMD_BASE] = NULL,		  /* 0x35 */

	[NAN_PA_CMD_OUI    - NAN_EXT_CMD_BASE] = nanProcessPA,    /* 0x37 */
	[NAN_MDC_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessMDC,   /* 0x38 */
	[NAN_ASC_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessASC,   /* 0x39 */
	[NAN_AMC_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessAMC,   /* 0x3A */
	[NAN_ASCC_CMD_OUI  - NAN_EXT_CMD_BASE] = nanProcessASCC,  /* 0x3B */
	[NAN_FR_EVT_OUI    - NAN_EXT_CMD_BASE] = nanProcessFR,    /* 0x3C */
	[NAN_EHT_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessEHT,	  /* 0x3D */

	[NAN_ADSDC_CMD_OUI - NAN_EXT_CMD_BASE] = nanProcessADSDC, /* 0x40 */
};

uint32_t nanExtParseCmd(struct ADAPTER *prAdapter,
			uint8_t *buf, uint16_t *u2Size)
{
	uint8_t ucNanOui;
	uint8_t ucCmdByte[NAN_MAX_EXT_DATA_SIZE] = {}; /* working buffer */
	uint32_t rStatus;

	DBGLOG(NAN, DEBUG, "Cmd Hex:\n");
	DBGLOG_HEX(NAN, DEBUG, buf, *u2Size);

	/* Both source and destination are in byte array format */
	kalMemCopy(ucCmdByte, buf, kal_min_t(uint16_t, *u2Size,
					     NAN_MAX_EXT_DATA_SIZE));

	ucNanOui = ucCmdByte[0];

	if (ucNanOui < NAN_EXT_CMD_BASE || ucNanOui > NAN_EXT_CMD_END ||
	    nanExtCmdHandler[ucNanOui - NAN_EXT_CMD_BASE] == NULL) {
		DBGLOG(NAN, WARN, "Invalid OUI: %u(0x%X)\n",
		       ucNanOui, ucNanOui);
		return WLAN_STATUS_FAILURE;
	}

	rStatus = nanExtCmdHandler[ucNanOui - NAN_EXT_CMD_BASE](prAdapter,
					      ucCmdByte, sizeof(ucCmdByte));

	*u2Size = kal_min_t(uint16_t,
			    EXT_MSG_SIZE(ucCmdByte), NAN_MAX_EXT_DATA_SIZE);
	kalMemCopy(buf, ucCmdByte, *u2Size);
	DBGLOG(NAN, DEBUG, "Response Hex:\n");
	DBGLOG_HEX(NAN, DEBUG, buf, *u2Size);

	return rStatus;
}

void nanExtEnableReq(struct ADAPTER *prAdapter)
{
	g_ucNanIsOn = TRUE;

	nanEhtNanOnHandler(prAdapter);
	nanAdsdcNanOnHandler(prAdapter);
	nanAsccNanOnHandler(prAdapter);
	nanExtSetAsc(prAdapter);
}

void nanExtDisableReq(struct ADAPTER *prAdapter)
{
	nanEhtNanOffHandler(prAdapter);
	nanAdsdcNanOffHandler(prAdapter);
	nanAsccNanOffHandler(prAdapter);
	nanExtTerminateApNan(prAdapter, NAN_ASC_EVENT_ASCC_END_LEGACY);
	g_ucNanIsOn = FALSE;
}

void nanExtBackToNormal(struct ADAPTER *prAdapter)
{
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter is NULL\n");
		return;
        }

	nanAdsdcBackToNormal(prAdapter);
}

uint32_t nanExtSendIndication(struct ADAPTER *prAdapter,
			      void *buf, uint16_t u2Size)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int r;

	r = mtk_cfg80211_vendor_nan_ext_indication(prAdapter, buf, u2Size);
	if (r != WLAN_STATUS_SUCCESS)
		rStatus = WLAN_STATUS_FAILURE;

	return rStatus;
}

u32 wlanoidNANExtCmd(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanExtCmdMsg *pExtCmd = (struct NanExtCmdMsg *)pvSetBuffer;

	DBGLOG(NAN, DEBUG, "NAN Ext Cmd:\n");
	DBGLOG_HEX(NAN, DEBUG, pExtCmd->data, pExtCmd->fwHeader.msgLen);

	/**
	 * 1. Pass to NAN EXT CMD handler in binary array
	 * 2. The handler returns binary array in pExtCmd->data
	 * Both data buffer and data buffer size arguments are bidirectional
	 */
	return nanExtParseCmd(prAdapter, pExtCmd->data,
			      &pExtCmd->fwHeader.msgLen);
}

u32 wlanoidNANExtCmdRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanExtResponseMsg nanExtRsp = {0};
	struct NanExtResponseMsg *pNanExtRsp =
		(struct NanExtResponseMsg *)pvSetBuffer;
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

	nanExtRsp.fwHeader = pNanExtRsp->fwHeader;

	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanExtResponseMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN_EXT, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	/* TODO: append response string */
	kalMemCopy(nanExtRsp.data, pNanExtRsp->data, nanExtRsp.fwHeader.msgLen);
	DBGLOG(NAN, TRACE, "Resp data:");
	DBGLOG_HEX(NAN, TRACE, nanExtRsp.data, nanExtRsp.fwHeader.msgLen);

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanExtResponseMsg),
			     &nanExtRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	return WLAN_STATUS_SUCCESS;
}

uint32_t nanSchedGetVendorAttr(struct ADAPTER *prAdapter,
			       uint8_t **ppucVendorAttr,
			       uint32_t *pu4VendorAttrLength)
{
	__KAL_ATTRIB_PACKED_FRONT__
	struct _NAN_ATTR_VSIE_SPECIFIC_T {
		uint8_t aucValue[8];
	} __KAL_ATTRIB_PACKED__;
	uint8_t aucSsOui[] = {0x00, 0x00, 0xF0};
	struct _NAN_ATTR_VENDOR_SPECIFIC_T *prAttrVendor;
	struct _NAN_ATTR_VSIE_SPECIFIC_T *prVsie;
	uint8_t *pucPos;
	uint8_t *pucNanIEBuffer = nanGetNanIEBuffer();

	prAttrVendor =
		(struct _NAN_ATTR_VENDOR_SPECIFIC_T *)
		pucNanIEBuffer;
	kalMemZero(pucNanIEBuffer, NAN_IE_BUF_MAX_SIZE);

	prAttrVendor->ucAttrId = NAN_ATTR_ID_VENDOR_SPECIFIC;

	prAttrVendor->u2Length = 14;

	kalMemCopy(prAttrVendor->aucOui, aucSsOui, VENDOR_OUI_LEN);

	prAttrVendor->ucVendorSpecificOuiType = 0x33;

	prAttrVendor->u2SubAttrLength = 8;

	pucPos = prAttrVendor->aucVendorSpecificOuiData;

	prVsie = (struct _NAN_ATTR_VSIE_SPECIFIC_T *)pucPos;

	kalMemZero(prVsie, sizeof(struct _NAN_ATTR_VSIE_SPECIFIC_T));

#if (CFG_SUPPORT_802_11AX == 1)
	prVsie->aucValue[0] = 0x1;
#endif

	pucPos += sizeof(struct _NAN_ATTR_VSIE_SPECIFIC_T);

	if (ppucVendorAttr)
		*ppucVendorAttr = pucNanIEBuffer;
	if (pu4VendorAttrLength)
		*pu4VendorAttrLength = (pucPos - pucNanIEBuffer);

	DBGDUMP_HEX(NAN, DEBUG, "SS VSIE",
		    pucNanIEBuffer, pucPos - pucNanIEBuffer);

	return WLAN_STATUS_SUCCESS;
}

#if (CFG_SUPPORT_NAN_11BE == 1)
uint32_t nanSchedGetVendorEhtAttr(struct ADAPTER *prAdapter,
				  uint8_t **ppucVendorAttr,
				  uint32_t *pu4VendorAttrLength)
{
#if CFG_EXT_FEATURE
	__KAL_ATTRIB_PACKED_FRONT__
	struct _NAN_ATTR_EHT_SPECIFIC_T {
		u_int8_t ucId;
		u_int8_t ucLength;
		u_int8_t ucExtId;
		u_int8_t ucEhtMacCap[2];
		u_int8_t ucEhtPhyCap[9];
		u_int8_t ucEhtMcs[9];
		u_int8_t ucValue[4];
	} __KAL_ATTRIB_PACKED__;
	uint8_t aucSsOui[] = {0x00, 0x00, 0xF0};
	uint8_t aucSsMacCap[] = {0x00, 0x00};
	uint8_t aucSsPhyCap[] = {0xE2, 0xFF, 0xFF, 0x01,
			0x00, 0x06, 0x00, 0x00, 0x02};
	uint8_t aucSsMcs[] = {0x22, 0x22, 0x22, 0x22,
			0x22, 0x22, 0x22, 0x22, 0x22};
	uint8_t aucSsMode2[] = {0x12, 0x00, 0x00, 0x00};
	uint8_t aucSsMode0[] = {0x10, 0x00, 0x00, 0x00};
	struct _NAN_ATTR_VENDOR_SPECIFIC_T *prAttrVendor;
	struct _NAN_ATTR_EHT_SPECIFIC_T *prEht;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNanSpecificBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t *pucPos;
	uint8_t *pucNanIEBuffer = nanGetNanIEBuffer();

#if (CFG_SUPPORT_NAN_DBDC == 1)
	prNanSpecificBssInfo =
		nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND1);
#else
	prNanSpecificBssInfo =
		nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND0);
#endif
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prNanSpecificBssInfo->ucBssIndex);

	if (!prBssInfo || !nanIsEhtSupport(prAdapter))
		return 0;

	prAttrVendor = (struct _NAN_ATTR_VENDOR_SPECIFIC_T *)pucNanIEBuffer;
	kalMemZero(pucNanIEBuffer, NAN_IE_BUF_MAX_SIZE);

	prAttrVendor->ucAttrId = NAN_ATTR_ID_VENDOR_SPECIFIC;

	kalMemCopy(prAttrVendor->aucOui, aucSsOui, VENDOR_OUI_LEN);

	prAttrVendor->ucVendorSpecificOuiType = 0x41;

	pucPos = prAttrVendor->aucVendorSpecificOuiData;

	prEht = (struct _NAN_ATTR_EHT_SPECIFIC_T *)pucPos;

	kalMemZero(prEht, sizeof(struct _NAN_ATTR_EHT_SPECIFIC_T));

	prEht->ucId = ELEM_ID_RESERVED;
	prEht->ucLength = 0x19;
	prEht->ucExtId = ELEM_EXT_ID_EHT_CAPS;
	kalMemCopy(prEht->ucEhtMacCap, aucSsMacCap, 2);
	kalMemCopy(prEht->ucEhtPhyCap, aucSsPhyCap, 9);
	kalMemCopy(prEht->ucEhtMcs, aucSsMcs, 9);
	if (nanIsEhtEnable(prAdapter))
		kalMemCopy(prEht->ucValue, aucSsMode2, 4);
	else
		kalMemCopy(prEht->ucValue, aucSsMode0, 4);

	prAttrVendor->u2SubAttrLength = prEht->ucLength + 2;

	prAttrVendor->u2Length = prAttrVendor->u2SubAttrLength + 6;

	pucPos += sizeof(struct _NAN_ATTR_EHT_SPECIFIC_T);

	if (ppucVendorAttr)
		*ppucVendorAttr = pucNanIEBuffer;
	if (pu4VendorAttrLength)
		*pu4VendorAttrLength = (pucPos - pucNanIEBuffer);

	DBGDUMP_HEX(NAN, DEBUG, "SS EHT VSIE",
		    pucNanIEBuffer, pucPos - pucNanIEBuffer);
#endif

	return WLAN_STATUS_SUCCESS;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Vendor
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineVendorAttrLength(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucVendorAttr = NULL;
	uint32_t u4VendorAttrLength = 0;

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	if (nanGetFeatureIsSigma(prAdapter))
		return 0;

	nanSchedGetVendorAttr(prAdapter, &pucVendorAttr,
				     &u4VendorAttrLength);

	return u4VendorAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Vendor
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineVendorAttrAppend(struct ADAPTER *prAdapter,
			      struct MSDU_INFO *prMsduInfo,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucVendorAttr = NULL;
	uint32_t u4VendorAttrLength = 0;

	if ((prNDL == NULL) && (prNDP == NULL))
		return;

	nanSchedGetVendorAttr(prAdapter, &pucVendorAttr,
				     &u4VendorAttrLength);

	if ((pucVendorAttr != NULL) && (u4VendorAttrLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucVendorAttr, u4VendorAttrLength);
		prMsduInfo->u2FrameLength += u4VendorAttrLength;
	}
}

#if (CFG_SUPPORT_NAN_11BE == 1)
/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Vendor EHT
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineVendorEhtAttrLength(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucVendorAttr = NULL;
	uint32_t u4VendorAttrLength = 0;

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	if (nanGetFeatureIsSigma(prAdapter))
		return 0;

	nanSchedGetVendorEhtAttr(prAdapter, &pucVendorAttr,
				     &u4VendorAttrLength);

	return u4VendorAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Vendor EHT
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineVendorEhtAttrAppend(struct ADAPTER *prAdapter,
			      struct MSDU_INFO *prMsduInfo,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucVendorAttr = NULL;
	uint32_t u4VendorAttrLength = 0;

	if ((prNDL == NULL) && (prNDP == NULL))
		return;

	nanSchedGetVendorEhtAttr(prAdapter, &pucVendorAttr,
				     &u4VendorAttrLength);

	if ((pucVendorAttr != NULL) && (u4VendorAttrLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucVendorAttr, u4VendorAttrLength);
		prMsduInfo->u2FrameLength += u4VendorAttrLength;
	}
}
#endif
#endif

