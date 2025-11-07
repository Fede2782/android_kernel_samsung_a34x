/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include "gl_kal.h"
#include "gl_ics.h"
#include "gl_fw_log.h"

#if (CFG_SUPPORT_FW_LOG_SPLIT_AND_REDIRECT == 1)
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define FW_LOG_PARSE_DBG	0
#define FW_LOG_TXP_C_TARGET "TXP-C"
#define FW_LOG_TXP_C_TARGET_LEN 5  /* strlen("TXP-C") */
#define FW_IDX_LOG_MARKER 0xA8  /* Log start marker */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
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
void fw_log_parse_buffer(uint8_t *buffer, uint32_t length)
{
	uint32_t u4IdxCnt = 0;
	uint32_t u4CtrlTimeSyncCnt = 0;
	uint32_t u4CtrlLostCnt = 0;
	uint32_t u4TextLogCnt = 0;
	uint32_t u4UnknownVerCnt = 0;
	uint32_t u4UnknownCtrlCnt = 0;
	uint32_t u4SanityFailCnt = 0;
	ssize_t ret;

#if (FW_LOG_PARSE_DBG == 1)
	DBGLOG(INIT, VOC, "starting\n");
#endif

	if (buffer == NULL || length == 0)
		return;

	for (uint32_t i = 0; i < length; i++) {
		if (buffer[i] == FW_IDX_LOG_MARKER) {
			uint8_t ucVerType = (buffer[i + 1]) & 0x07;
			enum FwLogType eLogType = FW_LOG_TYPE_UNKNOWN;
			uint32_t u4LogLength = 0;

#if (FW_LOG_PARSE_DBG == 1)
			DBGLOG(INIT, VOC,
				"[A8] ver_type=%d, buff[%u]=%x buff[%u]=%x buff[%u]=%x buff[%u]=%x\n",
				ucVerType,
				i, buffer[i],
				i + 1, buffer[i + 1],
				i + 2, buffer[i + 2],
				i + 3, buffer[i + 3]);
#endif
			if (ucVerType == FW_LOG_VER_TYPE_INDEX_2_W_TS_32B
				|| ucVerType ==
				FW_LOG_VER_TYPE_INDEX_2_W_TS_64B) {
				uint8_t ucNumOfArgs = (buffer[i + 1] >> 3)
					& 0x1F;
#if (FW_LOG_PARSE_DBG == 1)
				uint8_t ucModuleId = (buffer[i + 2]);
				uint8_t ucSeqNo = (buffer[i + 3] >> 4) & 0x0F;
				uint32_t ucIndexId = 0;
#endif
				uint8_t ucLevelId = (buffer[i + 3]) & 0x0F;

#if (FW_LOG_PARSE_DBG == 1)
				if (ucVerType ==
					FW_LOG_VER_TYPE_INDEX_2_W_TS_64B)
					ucIndexId = (buffer[i + 11] << 24) |
						(buffer[i + 12] << 16) |
						(buffer[i + 13] << 8)  |
						(buffer[i + 14]);
				else
					ucIndexId = (buffer[i + 7] << 24) |
						(buffer[i + 8] << 16) |
						(buffer[i + 9] << 8)  |
						(buffer[i + 10]);
#endif

				eLogType = FW_LOG_TYPE_INDEX;
				u4LogLength = (ucVerType ==
					FW_LOG_VER_TYPE_INDEX_2_W_TS_32B) ?
					12 + (ucNumOfArgs * 4)
					: 16 + (ucNumOfArgs * 4);

				if (ucLevelId ==
					FW_LOG_DBG_CLASS_CUSTOM_STATE) {
					ret = kalIcsWrite(&buffer[i],
						u4LogLength);
					if (ret != u4LogLength)
						DBGLOG_LIMITED(INIT, VOC,
							"[A8] Write FW CS Log into ring buffer fail [len=%u|wlen=%zd]\n",
							u4LogLength, ret);
				}
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[idx] num_of_args=%d module_id=%d level_id=%d seq_no=%d index_id=%u log_length=%u\n",
					ucNumOfArgs,
					ucModuleId,
					ucLevelId,
					ucSeqNo,
					ucIndexId,
					u4LogLength);
#endif
			} else if (ucVerType == FW_LOG_VER_TYPE_CTRL) {
				uint8_t ucCtrlType = (buffer[i + 1] >> 3)
					& 0x1F;

				if (ucCtrlType == 1 || ucCtrlType == 3) {
					eLogType = FW_LOG_TYPE_CTRL_TIMESYNC;
					if (ucCtrlType == 1)
						u4LogLength = 16;
					else if (ucCtrlType == 3)
						u4LogLength = 20;
					ret = kalIcsWrite(&buffer[i],
						u4LogLength);
					if (ret != u4LogLength)
						DBGLOG_LIMITED(INIT, VOC,
							"[A8] Write FW TimeSync Log into ring buffer fail [len=%u|wlen=%zd]\n",
							u4LogLength, ret);
#if (FW_LOG_PARSE_DBG == 1)
					DBGLOG(INIT, VOC,
						"[ctrl] ctrl_type=%u(timesync) log_length=%u\n",
						ucCtrlType, u4LogLength);
#endif
				} else if (ucCtrlType == 2) {
					eLogType = FW_LOG_TYPE_CTRL_LOST;
					u4LogLength = 8;
#if (FW_LOG_PARSE_DBG == 1)
					DBGLOG(INIT, VOC,
						"[ctrl] ctrl_type=%u(loglost) log_length=%u\n",
						ucCtrlType, u4LogLength);
#endif
				} else {
					u4UnknownCtrlCnt++;
#if (FW_LOG_PARSE_DBG == 1)
					DBGLOG(INIT, VOC,
						"[ctrl] unknown ctrl_type=%u\n",
						ucCtrlType);
#endif
					continue;
				}
			} else if (ucVerType == 2) {
				uint8_t ucCtrlType = (buffer[i + 1] >> 3)
					& 0x1F;
#if (FW_LOG_PARSE_DBG == 1)
				uint32_t u4PayloadSizeWoPadding = buffer[i + 2];
#endif
				uint32_t u4PayloadSizeWiPadding = buffer[i + 3];

				eLogType = FW_LOG_TYPE_UNIFY_TEXT;
				u4LogLength = (ucCtrlType == 1) ?
					12 + u4PayloadSizeWiPadding
					: 8 + u4PayloadSizeWiPadding;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[text] ctrl_type=%u payload_size=[%u/%u] log_length=%u\n",
					ucCtrlType, u4PayloadSizeWoPadding,
					u4PayloadSizeWiPadding, u4LogLength);
#endif
			} else {
				u4UnknownVerCnt++;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[A8] unknown ver_type=%d, continue\n",
					ucVerType);
#endif
				continue;
			}

#if (FW_LOG_PARSE_DBG == 1)
			DBGLOG(INIT, VOC,
				"[A8] FW Log Type: %d, Length: %u bytes\n",
				eLogType, u4LogLength);
#endif
			if (((i + u4LogLength) < length)
				&& buffer[i + u4LogLength] !=
				FW_IDX_LOG_MARKER) {
				u4SanityFailCnt++;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[A8] Sanity fail buffer[%u]=%x buffer[%u]=%x buffer[%u]=%x buffer[%u]=%x\n",
					i + u4LogLength,
					buffer[i + u4LogLength],
					i + u4LogLength + 1,
					buffer[i + u4LogLength + 1],
					i + u4LogLength + 2,
					buffer[i + u4LogLength + 2],
					i + u4LogLength + 3,
					buffer[i + u4LogLength + 3]);
				DBGLOG_MEM8(INIT, VOC, &buffer[i + u4LogLength],
					length - (i + u4LogLength));
#endif
			}

			switch (eLogType) {
			case FW_LOG_TYPE_INDEX:
				u4IdxCnt++;
				break;
			case FW_LOG_TYPE_CTRL_TIMESYNC:
				u4CtrlTimeSyncCnt++;
				break;
			case FW_LOG_TYPE_CTRL_LOST:
				u4CtrlLostCnt++;
				break;
			case FW_LOG_TYPE_UNIFY_TEXT:
				u4TextLogCnt++;
				break;
			default:
				break;
			}

			if (u4LogLength > 0)
				i += (u4LogLength - 1);
			else
				DBGLOG(INIT, WARN, "Invalid log length: %u\n",
					u4LogLength);
		}
	}

#if (FW_LOG_PARSE_DBG == 1)
	DBGLOG(INIT, VOC,
		"[idx_cnt:%u][timesync_cnt:%u][lost_cnt:%u][text_cnt:%u][unknown_v_cnt:%u][unknown_c_cnt:%u][sanity_fail_cnt:%u]\n",
		u4IdxCnt,
		u4CtrlTimeSyncCnt,
		u4CtrlLostCnt,
		u4TextLogCnt,
		u4UnknownVerCnt,
		u4UnknownCtrlCnt,
		u4SanityFailCnt);
	DBGLOG(INIT, VOC, "end\n");
#endif
}

static inline uint8_t checkUnifyStrIsSpecLog(uint8_t *buffer,
	uint32_t u4PayloadSizeWiPadding, uint32_t ucStartStrPos)
{
	if (u4PayloadSizeWiPadding < FW_LOG_TXP_C_TARGET_LEN)
		return FALSE;

	uint32_t maxSearchPos = u4PayloadSizeWiPadding -
		FW_LOG_TXP_C_TARGET_LEN;
	uint8_t *payloadStart = buffer + ucStartStrPos;

	/* Check each possible starting position */
	for (uint32_t j = 0; j <= maxSearchPos; j++) {
		/* First-byte quick check avoids unnecessary full comparisons */
		if (payloadStart[j] == 'T') {
			/* Full match check only if first character matches */
			if (kalMemCmp(&payloadStart[j], FW_LOG_TXP_C_TARGET,
				FW_LOG_TXP_C_TARGET_LEN) == 0) {
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"Found target string 'TXP-C' at position %d\n",
					j);
#endif
				return TRUE;
			}
		}
	}

	return FALSE;
}

static inline void write_spec_log(struct ADAPTER *ad,
	uint8_t *puSrcBuf, int32_t u4LogLength,
	uint8_t *puDstLogBuf, int32_t *puDstLogSize,
	enum FwSpecLogDest eFwSpecLogDst)
{
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;
	ssize_t ret = 0;

	switch (eFwSpecLogDst) {
	case FW_SPEC_LOG_DEST_FW_LOG_RING:
	case FW_SPEC_LOG_DEST_ICS_LOG_RING:
		/* Collect spec log buffer */
		if (puDstLogBuf && puDstLogSize) {
			kalMemCopy(puDstLogBuf + *puDstLogSize,
				puSrcBuf, u4LogLength);
			*puDstLogSize += u4LogLength;
		} else
			DBGLOG_LIMITED(INIT, ERROR,
				"Write spec log to dst[%d] fail because logBuffer is null\n",
				prWifiVar->ucFwSpecLogDst);
		break;
	case FW_SPEC_LOG_DEST_FW_LOG_RING_INLINE:
		/* Directly write to fw log ring */
		ret = fw_log_notify_rcv(ENUM_FW_LOG_CTRL_TYPE_WIFI,
			puSrcBuf, u4LogLength);
		if (ret != u4LogLength)
			DBGLOG_LIMITED(INIT, WARN,
				"Write spec log to fw_log_ring buffer [%zd != %u]\n",
				ret, u4LogLength);
		break;
	case FW_SPEC_LOG_DEST_ICS_LOG_RING_INLINE:
		/* Directly write to ics log ring */
		ret = kalIcsWrite(puSrcBuf, u4LogLength);
		if (ret != u4LogLength)
			DBGLOG_LIMITED(INIT, WARN,
				"[A8] Write spec log into ics_log_ring buffer fail [len=%u|wlen=%zd]\n",
				u4LogLength, ret);
		break;
	default:
		DBGLOG(INIT, WARN, "An error eFwSpecLogDst setting [%d]\n",
			eFwSpecLogDst);
		break;
	}
}

static inline void write_dbg_log(struct ADAPTER *ad,
	uint8_t *puSrcBuf, uint32_t u4LogLength,
	uint8_t *puDstLogBuf, uint32_t *puDstLogSize,
	enum FwDbgLogDest eFwDbgLogDst)
{
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;
	ssize_t ret = 0;

	switch (eFwDbgLogDst) {
	case FW_DBG_LOG_DEST_ICS_LOG_RING_INLINE:
		/* Directly write to ics log ring */
		ret = kalIcsWrite(puSrcBuf, u4LogLength);
		if (ret != u4LogLength)
			DBGLOG_LIMITED(INIT, WARN,
				"[A8] Write DBG Log into ring buffer fail [len=%u|wlen=%zd]\n",
				u4LogLength, ret);
		break;
	case FW_DBG_LOG_DEST_FW_LOG_RING_INLINE:
		/* Directly write to fw log ring */
		ret = fw_log_notify_rcv(ENUM_FW_LOG_CTRL_TYPE_WIFI,
			puSrcBuf, u4LogLength);
		if (ret != u4LogLength)
			DBGLOG_LIMITED(INIT, WARN,
				"Write spec log to fw_log_ring buffer [%zd != %u]\n",
				ret, u4LogLength);
		break;
	case FW_DBG_LOG_DEST_ICS_LOG_RING:
	case FW_DBG_LOG_DEST_FW_LOG_RING:
	case FW_DBG_LOG_DEST_GET_RING_DATA:
		/* Collect debug log buffer */
		if (puDstLogBuf && puDstLogSize) {
			kalMemCopy(puDstLogBuf + *puDstLogSize,
				puSrcBuf, u4LogLength);
			*puDstLogSize += u4LogLength;
		} else
			DBGLOG_LIMITED(INIT, ERROR,
				"Write dbg log to dst[%d] fail because logBuffer is null\n",
				prWifiVar->ucFwDbgLogDst);
		break;
	case FW_DBG_LOG_DEST_GET_RING_DATA_INLINE:
		/* Get ring data inline: directly write to get_ring_data ring */
		logger_write_wififw(puSrcBuf, u4LogLength);
		break;
	default:
		DBGLOG(INIT, WARN, "An error eFwDbgLogDst setting [%d]\n",
			eFwDbgLogDst);
		break;
	}
}

uint32_t fw_log_redir_handler(struct ADAPTER *ad,
	uint8_t *buffer, uint32_t length,
	uint8_t *puDstSpecLogBuf, uint32_t *puDstSpecLogSize,
	uint8_t *puDstDbgLogBuf, uint32_t *puDstDbgLogSize,
	enum FwSpecLogDest eFwSpecLogDst,
	enum FwDbgLogDest eFwDbgLogDst)
{
	uint32_t u4IdxCnt = 0;
	uint32_t u4CtrlTimeSyncCnt = 0;
	uint32_t u4CtrlLostCnt = 0;
	uint32_t u4TextLogCnt = 0;
	uint32_t u4UnknownVerCnt = 0;
	uint32_t u4UnknownCtrlCnt = 0;
	uint32_t u4SanityFailCnt = 0;
	uint32_t u4ProcessedSize = 0;
	uint32_t u4SkippedBytes = 0;
#if (FW_LOG_PARSE_DBG == 1)
	uint32_t u4TotalBuffSize = 0;
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	DBGLOG(INIT, VOC, "starting [spec log dst:%d, dbg log dst:%d]\n",
	       prWifiVar->ucFwSpecLogDst, prWifiVar->ucFwDbgLogDst);
#endif

	if (buffer == NULL || length == 0)
		return 0;

	for (uint32_t i = 0; i < length; i++) {
#if (FW_LOG_PARSE_DBG == 1)
		DBGLOG(INIT, VOC,
			"[loop] i=%u[%x] length=%u specLogSize=%u dbgLogSize=%u\n",
			i, buffer[i], length,
			*puDstSpecLogSize, *puDstDbgLogSize);
#endif
		if (buffer[i] == FW_IDX_LOG_MARKER) {
			uint8_t ucVerType = (buffer[i + 1]) & 0x07;
			enum FwLogType eLogType = FW_LOG_TYPE_UNKNOWN;
			uint32_t u4LogLength = 0;

#if (FW_LOG_PARSE_DBG == 1)
			DBGLOG(INIT, VOC,
				"[A8] ver_type=%d, buff[%u]=%x buff[%u]=%x buff[%u]=%x buff[%u]=%x\n",
				ucVerType,
				i, buffer[i],
				i + 1, buffer[i + 1],
				i + 2, buffer[i + 2],
				i + 3, buffer[i + 3]);
#endif
			if (ucVerType == FW_LOG_VER_TYPE_INDEX_2_W_TS_32B
				|| ucVerType ==
				FW_LOG_VER_TYPE_INDEX_2_W_TS_64B) {
				uint8_t ucNumOfArgs = (buffer[i + 1] >> 3)
					& 0x1F;
#if (FW_LOG_PARSE_DBG == 1)
				uint8_t ucModuleId = (buffer[i + 2]);
				uint8_t ucSeqNo = (buffer[i + 3] >> 4) & 0x0F;
				uint32_t ucIndexId = 0;
#endif
				uint8_t ucLevelId = (buffer[i + 3]) & 0x0F;

#if (FW_LOG_PARSE_DBG == 1)
				if (ucVerType ==
					FW_LOG_VER_TYPE_INDEX_2_W_TS_64B)
					ucIndexId = (buffer[i + 11] << 24) |
						(buffer[i + 12] << 16) |
						(buffer[i + 13] << 8)  |
						(buffer[i + 14]);
				else
					ucIndexId = (buffer[i + 7] << 24) |
						(buffer[i + 8] << 16) |
						(buffer[i + 9] << 8)  |
						(buffer[i + 10]);
#endif

				eLogType = FW_LOG_TYPE_INDEX;
				u4LogLength = (ucVerType == 0) ?
					12 + (ucNumOfArgs * 4)
					: 16 + (ucNumOfArgs * 4);

				if (ucLevelId ==
					FW_LOG_DBG_CLASS_CUSTOM_STATE)
					write_spec_log(ad, buffer + i,
						u4LogLength,
						puDstSpecLogBuf,
						puDstSpecLogSize,
						eFwSpecLogDst);
				else
					write_dbg_log(ad, buffer + i,
						u4LogLength,
						puDstDbgLogBuf,
						puDstDbgLogSize,
						eFwDbgLogDst);
				u4ProcessedSize += u4LogLength;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[idx] num_of_args=%d module_id=%d level_id=%d seq_no=%d index_id=%u log_length=%u\n",
					ucNumOfArgs,
					ucModuleId,
					ucLevelId,
					ucSeqNo,
					ucIndexId,
					u4LogLength);
#endif
			} else if (ucVerType == FW_LOG_VER_TYPE_CTRL) {
				uint8_t ucCtrlType = (buffer[i + 1] >> 3)
					& 0x1F;

				if (ucCtrlType ==
					FW_CTRL_TYPE_TSC_W_TS_32B
					|| ucCtrlType ==
					FW_CTRL_TYPE_TSC_W_TS_64B) {
					eLogType = FW_LOG_TYPE_CTRL_TIMESYNC;
					u4LogLength = (ucCtrlType ==
						FW_CTRL_TYPE_TSC_W_TS_32B)
						? 16 : 20;
					write_spec_log(ad, buffer + i,
						u4LogLength,
						puDstSpecLogBuf,
						puDstSpecLogSize,
						eFwSpecLogDst);
					write_dbg_log(ad, buffer + i,
						u4LogLength,
						puDstDbgLogBuf,
						puDstDbgLogSize,
						eFwDbgLogDst);
					u4ProcessedSize += u4LogLength;
#if (FW_LOG_PARSE_DBG == 1)
					DBGLOG(INIT, VOC,
						"[ctrl] ctrl_type=%u(timesync) log_length=%u\n",
						ucCtrlType, u4LogLength);
#endif
				} else if (ucCtrlType ==
					FW_CTRL_TYPE_LOG_LOST) {
					eLogType = FW_LOG_TYPE_CTRL_LOST;
					u4LogLength = 8;

					write_dbg_log(ad, buffer + i,
						u4LogLength,
						puDstDbgLogBuf,
						puDstDbgLogSize,
						eFwDbgLogDst);
					u4ProcessedSize += u4LogLength;
#if (FW_LOG_PARSE_DBG == 1)
					DBGLOG(INIT, VOC,
						"[ctrl] ctrl_type=%u(loglost) log_length=%u\n",
						ucCtrlType, u4LogLength);
#endif
				} else {
					u4UnknownCtrlCnt++;
					u4SkippedBytes++;
#if (FW_LOG_PARSE_DBG == 1)
					DBGLOG(INIT, VOC,
						"[ctrl] unknown ctrl_type=%u\n",
						ucCtrlType);
#endif
					continue;
				}
			} else if (ucVerType == FW_LOG_VER_TYPE_UNIFY_TEXT) {
				uint8_t ucCtrlType = (buffer[i + 1] >> 3)
					& 0x1F;
#if (FW_LOG_PARSE_DBG == 1)
				uint32_t u4PayloadSizeWoPadding = buffer[i + 2];
#endif
				uint32_t u4PayloadSizeWiPadding = buffer[i + 3];
				uint8_t ucStartStrPos = (ucCtrlType ==
					FW_UNIFY_LOG_CTRL_TYPE_TS_64B) ? 12 : 8;
				uint8_t ucIsSpecLog = FALSE;

				eLogType = FW_LOG_TYPE_UNIFY_TEXT;
				u4LogLength = (ucCtrlType ==
					FW_UNIFY_LOG_CTRL_TYPE_TS_64B) ?
					12 + u4PayloadSizeWiPadding
					: 8 + u4PayloadSizeWiPadding;

#if (FW_LOG_PARSE_DBG == 1)
				for (uint32_t j = 0; j < u4PayloadSizeWiPadding;
					j++) {
					DBGLOG(INIT, VOC,
						"[text] text[%d]=%c,%d\n",
						j,
						buffer[i + ucStartStrPos + j],
						buffer[i + ucStartStrPos + j]);
				}
#endif
				/* Since the UnifyText cannot determine from
				 * the Header whether the log level is
				 * CUSTOM_STATE
				 */
				ucIsSpecLog = checkUnifyStrIsSpecLog(buffer,
					u4PayloadSizeWiPadding,
					i + ucStartStrPos);
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"Is Found TXP-C = %d\n", ucIsSpecLog);
#endif
				if (ucIsSpecLog == FALSE)
					write_dbg_log(ad, buffer + i,
						u4LogLength,
						puDstDbgLogBuf,
						puDstDbgLogSize,
						eFwDbgLogDst);
				else
					write_spec_log(ad, buffer + i,
						u4LogLength,
						puDstSpecLogBuf,
						puDstSpecLogSize,
						eFwSpecLogDst);
				u4ProcessedSize += u4LogLength;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[text] ctrl_type=%u payload_size=[%u/%u] log_length=%u\n",
					ucCtrlType, u4PayloadSizeWoPadding,
					u4PayloadSizeWiPadding, u4LogLength);
#endif
			} else {
				u4UnknownVerCnt++;
				u4SkippedBytes++;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[A8] unknown ver_type=%d, continue\n",
					ucVerType);
#endif
				continue;
			}

#if (FW_LOG_PARSE_DBG == 1)
			DBGLOG(INIT, VOC,
				"[A8] FW Log Type: %d, Length: %u bytes\n",
				eLogType, u4LogLength);
#endif
			if (((i + u4LogLength) < length)
				&& buffer[i + u4LogLength] !=
				FW_IDX_LOG_MARKER) {
				u4SanityFailCnt++;
#if (FW_LOG_PARSE_DBG == 1)
				DBGLOG(INIT, VOC,
					"[A8] Sanity fail buffer[%u]=%x buffer[%u]=%x buffer[%u]=%x buffer[%u]=%x\n",
					i + u4LogLength,
					buffer[i + u4LogLength],
					i + u4LogLength + 1,
					buffer[i + u4LogLength + 1],
					i + u4LogLength + 2,
					buffer[i + u4LogLength + 2],
					i + u4LogLength + 3,
					buffer[i + u4LogLength + 3]);
				DBGLOG_MEM8(INIT, VOC, &buffer[i + u4LogLength],
					length - (i + u4LogLength));
#endif
			}

			switch (eLogType) {
			case FW_LOG_TYPE_INDEX:
				u4IdxCnt++;
				break;
			case FW_LOG_TYPE_CTRL_TIMESYNC:
				u4CtrlTimeSyncCnt++;
				break;
			case FW_LOG_TYPE_CTRL_LOST:
				u4CtrlLostCnt++;
				break;
			case FW_LOG_TYPE_UNIFY_TEXT:
				u4TextLogCnt++;
				break;
			default:
				break;
			}

			if (u4LogLength > 0)
				i += (u4LogLength - 1);
			else
				DBGLOG(INIT, WARN, "Invalid log length: %u\n",
					u4LogLength);
		} else
			u4SkippedBytes++;

	}

#if (FW_LOG_PARSE_DBG == 1)
	u4TotalBuffSize = *puDstSpecLogSize + *puDstDbgLogSize;
	DBGLOG(INIT, VOC,
		"[size analysis] input=%u processed=%u[diff=%u] total=%u(spec=%u+dbg=%u) skipped=%u\n",
		length, u4ProcessedSize, abs(length - u4ProcessedSize),
		u4TotalBuffSize, *puDstSpecLogSize,
		*puDstDbgLogSize, u4SkippedBytes);

	if (abs(length - u4ProcessedSize) > 3) {
		DBGLOG(INIT, WARN,
			"[size mismatch] length=%u != processed=%u[diff=%u][skip=%u] (total=%d[spec=%u+dbg=%u])\n",
			length, u4ProcessedSize,
			abs(length - u4ProcessedSize), u4SkippedBytes,
			u4TotalBuffSize, *puDstSpecLogSize, *puDstDbgLogSize);
		DBGLOG(INIT, WARN, "Dumping buffer for analysis:");
		DBGLOG_MEM8(INIT, WARN, buffer, length);
	}

	DBGLOG(INIT, VOC,
		"[idx_cnt:%u][timesync_cnt:%u][lost_cnt:%u][text_cnt:%u][unknown_v_cnt:%u][unknown_c_cnt:%u][sanity_fail_cnt:%u][processed:%u]\n",
		u4IdxCnt,
		u4CtrlTimeSyncCnt,
		u4CtrlLostCnt,
		u4TextLogCnt,
		u4UnknownVerCnt,
		u4UnknownCtrlCnt,
		u4SanityFailCnt,
		u4ProcessedSize);
#endif

	return u4ProcessedSize;
}

#endif
