/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _NAN_FUNC_H
#define _NAN_FUNC_H

#if !CFG_SUPPORT_NAN_EXT
static inline
void nanSetFlashCommunication(struct ADAPTER *prAdapter, u_int8_t fgEnable)
{
	DBGLOG(NAN, TRACE, "Set Flash Communication %u\n", fgEnable);
	/* TODO:
	 * 1. Set 2.4GHz/5GHz bitmap respectively
	 * 2. Call reconfigure function to update customized timeline
	 * 3. Send command to FW to set the bitmap, reuse Instant communication?
	 */
}

static inline
void nanExtEnableReq(struct ADAPTER *prAdapter)
{
	nanSetFlashCommunication(prAdapter, TRUE);
}

static inline
void nanExtDisableReq(struct ADAPTER *prAdapter)
{
}

static inline
void nanExtClearCustomNdpFaw(uint8_t ucIndex)
{
}

static inline
void nanPeerReportEhtEvent(struct ADAPTER *prAdapter,
					 uint8_t enable)
{
}

static inline
void nanEnableEhtMode(struct ADAPTER *prAdapter, uint8_t mode)
{
}

static inline
void nanEnableEht(struct ADAPTER *prAdapter, uint8_t enable)
{
}

static inline
void nanExtBackToNormal(struct ADAPTER *prAdapter)
{
}

static inline
void nanExtResetNdlConfig(struct _NAN_NDL_INSTANCE_T *prNDL)
{
}

static inline
struct _NAN_NDL_INSTANCE_T *nanExtGetReusedNdl(struct ADAPTER *prAdapter)
{
	return NULL;
}

static inline
u32 wlanoidNANExtCmd(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	return WLAN_STATUS_NOT_SUPPORTED;
}

static inline
u32 wlanoidNANExtCmdRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	return WLAN_STATUS_NOT_SUPPORTED;
}

static inline
void nanExtComposeBeaconTrack(struct ADAPTER *prAdapter,
				struct _NAN_EVENT_REPORT_BEACON *prFwEvt)
{
}

static inline
void nanExtComposeClusterEvent(struct ADAPTER *prAdapter,
			       struct NAN_DE_EVENT *prDeEvt)
{
}

static inline
uint32_t nanSchedGetVendorEhtAttr(struct ADAPTER *prAdapter,
				  uint8_t **ppucVendorAttr,
				  uint32_t *pu4VendorAttrLength)
{
	return 0;
}

static inline
uint32_t nanSchedGetVendorAttr(struct ADAPTER *prAdapter,
			       uint8_t **ppucVendorAttr,
			       uint32_t *pu4VendorAttrLength)
{
	return 0;
}

static inline
uint32_t nanGetFcSlots(struct ADAPTER *prAdapter)
{
	uint32_t u4Bitmap = 0;

	DBGLOG(NAN, TEMP,
	       "FC slots: %02x-%02x-%02x-%02x\n",
	       ((uint8_t *)&u4Bitmap)[0], ((uint8_t *)&u4Bitmap)[1],
	       ((uint8_t *)&u4Bitmap)[2], ((uint8_t *)&u4Bitmap)[3]);

	return u4Bitmap;
}

static inline
uint32_t nanGetTimelineFcSlots(struct ADAPTER *prAdapter, size_t szTimelineIdx,
			       size_t szSlotIdx)
{
	uint32_t u4Bitmap = 0;

	NAN_DW_DBGLOG(NAN, INFO, TRUE, szSlotIdx,
		      "Timeline %u FC slots: %02x-%02x-%02x-%02x\n",
		      szTimelineIdx,
		      ((uint8_t *)&u4Bitmap)[0], ((uint8_t *)&u4Bitmap)[1],
		      ((uint8_t *)&u4Bitmap)[2], ((uint8_t *)&u4Bitmap)[3]);

	return u4Bitmap;
}

/*
 * @szSlotIdx: slot index [0..512) representing the range in 0~8192 TU
 */
static inline
u_int8_t nanIsChnlSwitchSlot(struct ADAPTER *prAdapter,
			     unsigned char fgPrintLog,
			     size_t szTimelineIdx,
			     size_t szSlotIdx)
{
	return FALSE;
}

#endif

#endif /* _NAN_FUNC_H */
