/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_VIRTIO_CMDQ_PROTO_H__
#define __MTK_VIRTIO_CMDQ_PROTO_H__

#include <linux/types.h>
#include <linux/uio.h>

#define RESPONSE_DATA_SIZE 8

#define MAX_CMDQ_IOV_SIZE (35)

enum {
	CMDQ_OPS_FLUSH,
	CMDQ_OPS_WAIT_COMPLETE,
	CMDQ_OPS_DESTROY,
	CMDQ_OPS_CHAN_STOP,
	CMDQ_OPS_MBOX_ENABLE,
	CMDQ_OPS_MBOX_DISABLE,
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

struct cmdq_mbox_disable_request {
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
		struct cmdq_mbox_disable_request mbox_disable_req;
	};
};

struct cmdq_response {
	int32_t ret;
};

#endif
