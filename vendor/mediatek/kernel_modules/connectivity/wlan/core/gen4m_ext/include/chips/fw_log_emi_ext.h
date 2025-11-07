/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _FW_LOG_EMI_EXT_H_
#define _FW_LOG_EMI_EXT_H_

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define FW_LOG_DBG_CLASS_CUSTOM_STATE	5

enum FwLogType {
	FW_LOG_TYPE_INDEX = 0,
	FW_LOG_TYPE_CTRL_TIMESYNC,
	FW_LOG_TYPE_CTRL_LOST,
	FW_LOG_TYPE_UNIFY_TEXT,
	FW_LOG_TYPE_UNKNOWN,
	FW_LOG_TYPE_NUM
};

enum FwLogVerType {
	FW_LOG_VER_TYPE_INDEX_2_W_TS_32B = 0,
	FW_LOG_VER_TYPE_CTRL,
	FW_LOG_VER_TYPE_UNIFY_TEXT,
	FW_LOG_VER_TYPE_INDEX_2_W_TS_64B,
	FW_LOG_VER_TYPE_NUM
};

enum FwCtrlLogCtrlType {
	FW_CTRL_TYPE_TSC_W_TS_32B = 1,
	FW_CTRL_TYPE_LOG_LOST,
	FW_CTRL_TYPE_TSC_W_TS_64B,
	FW_CTRL_TYPE_NUM
};

enum FwUnifyLogCtrlType {
	FW_UNIFY_LOG_CTRL_TYPE_TS_32B = 0,
	FW_UNIFY_LOG_CTRL_TYPE_TS_64B,
	FW_UNIFY_LOG_CTRL_TYPE_NUM
};

enum FwSpecLogDest {
	FW_SPEC_LOG_DEST_FW_LOG_RING = 0,
	FW_SPEC_LOG_DEST_FW_LOG_RING_INLINE,
	FW_SPEC_LOG_DEST_ICS_LOG_RING,
	FW_SPEC_LOG_DEST_ICS_LOG_RING_INLINE,
	FW_SPEC_LOG_DEST_NUM
};

enum FwDbgLogDest {
	FW_DBG_LOG_DEST_FW_LOG_RING = 0,
	FW_DBG_LOG_DEST_FW_LOG_RING_INLINE,
	FW_DBG_LOG_DEST_ICS_LOG_RING,
	FW_DBG_LOG_DEST_ICS_LOG_RING_INLINE,
	FW_DBG_LOG_DEST_GET_RING_DATA,
	FW_DBG_LOG_DEST_GET_RING_DATA_INLINE,
	FW_DBG_LOG_DEST_NUM
};

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
void fw_log_parse_buffer(uint8_t *buffer, uint32_t length);
uint32_t fw_log_redir_handler(struct ADAPTER *ad,
	uint8_t *buffer, uint32_t length,
	uint8_t *puDstSpecLogBuf, uint32_t *puDstSpecLogSize,
	uint8_t *puDstDbgLogBuf, uint32_t *puDstDbgLogSize,
	enum FwSpecLogDest eFwSpecLogDst,
	enum FwDbgLogDest eFwDbgLogDst);

#endif /* _FW_LOG_EMI_EXT_H */

