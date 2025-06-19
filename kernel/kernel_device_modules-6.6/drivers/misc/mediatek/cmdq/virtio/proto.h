/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __VIRTIO_MTK_PROTO_H__
#define __VIRTIO_MTK_PROTO_H__

#include <linux/types.h>

#define RESPONSE_DATA_SIZE	8

#define MAX_CMDQ_IOV_SIZE (35)

enum {
	REQ_MBOX,
	REQ_IPI,
	REQ_CLK,
};

enum {
	MCUPM_MBOX,
	SSPM_MBOX,
};

enum {
	MBOX_OPS_READ,
	MBOX_OPS_WRITE,
	MBOX_OPS_GET_IRQSTATE,
};

enum {
	IPI_OPS_SEND,
	IPI_OPS_SEND_COMP,
};

enum {
	CLK_OPS_PREPARE,
	CLK_OPS_UNPREPARE,
	CLK_OPS_ENABLE,
	CLK_OPS_DISABLE,
	CLK_OPS_SET_RATE,
	CLK_OPS_SET_PARENT,
};

enum {
	CMDQ_OPS_FLUSH,
	CMDQ_OPS_WAIT_COMPLETE,
	CMDQ_OPS_DESTROY,
	CMDQ_OPS_CHAN_STOP,
	CMDQ_OPS_MBOX_ENABLE,
};

enum {
	VCORE_DVFS_TYPE_UNKNOWN,
	VCORE_DVFS_TYPE_ADD_REQUEST,
	VCORE_DVFS_TYPE_UPDATE_REQUEST,
	VCORE_DVFS_TYPE_REMOVE_REQUEST,
};

struct mbox_req {
	uint8_t mbox_type;
	uint8_t ops;
	uint8_t mbox_id;
	uint8_t slot;
	uint8_t pin_index;
};

struct ipi_req {
	uint8_t mbox_type;
	uint8_t ipi_id;
	uint8_t ops;
	uint8_t opt;
	uint32_t timeout_ms;
};

#define CLK_NAME_MAX_LEN 64

struct clk_req {
	char name[CLK_NAME_MAX_LEN];
	char parent_name[CLK_NAME_MAX_LEN];
	uint8_t ops;
	uint64_t arg1;
	uint64_t arg2;
};

struct mtk_request {
	uint8_t type;
	union {
		struct mbox_req mbox_req;
		struct ipi_req ipi_req;
		struct clk_req clk_req;
	};
};

struct mtk_response {
	int32_t ret;
	uint32_t payload_len;
	uint64_t payload[RESPONSE_DATA_SIZE];
};

struct cmdq_flush_request {
	uint32_t thread_id;
	uint64_t key;
	uint32_t avail_buf_size;
	uint32_t cmd_buf_size;
	uint32_t buf_size;
	uint32_t priority;
	uint32_t hwid;
	bool loop;
	uint iov_len;
	uint64_t cmd_buf_paddrs[MAX_CMDQ_IOV_SIZE];
	uint64_t cmd_buf_iovaddrs[MAX_CMDQ_IOV_SIZE];
	struct iovec iov[MAX_CMDQ_IOV_SIZE];
};

struct cmdq_wait_complete_request {
	uint64_t key;
	uint32_t hwid;
};

struct cmdq_destroy_request {
	uint64_t key;
	uint32_t hwid;
};

struct cmdq_chan_stop_request {
	uint32_t thread_id;
	uint32_t hwid;
};

struct cmdq_mbox_enable_request {
	uint32_t thread_id;
	uint32_t hwid;
};

struct cmdq_task_complete_event {
	uint64_t key;
	int32_t result;
};

struct cmdq_request {
	uint8_t type;
	union {
		struct cmdq_flush_request flush_req;
		struct cmdq_wait_complete_request wait_req;
		struct cmdq_destroy_request destroy_req;
		struct cmdq_chan_stop_request chan_stop_req;
		struct cmdq_mbox_enable_request mbox_enable_req;
	};
};

struct cmdq_response {
	int32_t ret;
};

struct cmdq_util_platform_fp;
void virtio_cmdq_util_set_fp(struct cmdq_util_platform_fp *cust_cmdq_platform);
#endif

