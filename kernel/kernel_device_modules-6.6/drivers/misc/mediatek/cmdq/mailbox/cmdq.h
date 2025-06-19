/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_VIRTIO_MTK_CMDQ_H__
#define __MTK_VIRTIO_MTK_CMDQ_H__

#include "proto.h"
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

int init_cmdq_service(struct device *dev, struct resource *res);
void release_pending_cmdq_pkt(void);

struct vhost_virtqueue;
void handle_cmdq_request(struct vhost_virtqueue *vq, struct cmdq_request *req,
			 struct cmdq_response *resp);

void cmdq_request_done(struct vhost_virtqueue *vq, uint64_t key, s32 result);

void set_cmdq_client(void *client, uint32_t hwid);

typedef s32 (*platform_vhost_cmdq_pkt_flush_async)(struct cmdq_pkt *pkt, cmdq_async_flush_cb cb, void *data);
typedef int (*platform_vhost_cmdq_pkt_wait_complete)(struct cmdq_pkt *pkt);
typedef void (*platform_vhost_cmdq_pkt_destroy)(struct cmdq_pkt *pkt);
typedef void (*platform_vhost_cmdq_mbox_channel_stop)(struct mbox_chan *chan);
typedef void (*platform_vhost_cmdq_set_client)(void *client, uint32_t hwid);

struct vhost_cmdq_platform_fp {
	platform_vhost_cmdq_pkt_flush_async vhost_cmdq_pkt_flush_async;
	platform_vhost_cmdq_pkt_wait_complete vhost_cmdq_pkt_wait_complete;
	platform_vhost_cmdq_pkt_destroy vhost_cmdq_pkt_destroy;
	platform_vhost_cmdq_mbox_channel_stop vhost_cmdq_mbox_channel_stop;
	platform_vhost_cmdq_set_client vhost_cmdq_set_client;
};

#endif
