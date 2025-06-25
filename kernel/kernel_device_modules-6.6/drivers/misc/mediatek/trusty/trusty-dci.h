/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _TRUSTY_DCI_H_
#define _TRUSTY_DCI_H_

#include <linux/types.h>

#define LOOP_MAX 60

/* ISE TDrv Info */
#define ISE_TDRV_UUID	{{0x32, 0xb7, 0x3e, 0x20, 0xd7, 0x9e, 0x83, 0x6e,\
			  0x9c, 0x90, 0x3c, 0xc6, 0xbe, 0x96, 0x28, 0xc2}}

int trusty_dci_init(void);

/*
 * Command ID
 */
#define DCI_ISE_POWER_ON  101
#define DCI_ISE_POWER_OFF 102

/* DCI message data */
typedef uint32_t dciCommandId_t;
typedef uint32_t dciResponseId_t;
typedef uint32_t dciReturnCode_t;

/**
 * DCI command header.
 */
struct dciCommandHeader_t {
	dciCommandId_t commandId; /**< Command ID */
};

/**
 * DCI response header.
 */
struct dciResponseHeader_t {
	dciResponseId_t	responseId; /**< Response ID (Cmd_Id|RSP_ID_MASK) */
	dciReturnCode_t	returnCode; /**< Return code of command */
};

/**
 * command message.
 *
 * @param header Command/Response header
 * @param len Length of the data to process.
 */
#define CMD_LEN_MAGIC_NUM 0xF00DBABE
struct cmd_t {
	struct dciCommandHeader_t header;
	uint32_t                  len;
};

/**
 * Response structure
 */
struct rsp_t {
	struct dciResponseHeader_t header;
	uint32_t                   len;
};

struct dciMessage_t {
	union {
		struct cmd_t command;
		struct rsp_t response;
	};
};

#endif
