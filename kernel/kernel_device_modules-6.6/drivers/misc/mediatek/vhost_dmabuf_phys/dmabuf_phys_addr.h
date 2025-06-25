/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _VHOST_DMABUF_DMABUF__
#define _VHOST_DMABUF_DMABUF__

#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>

#define VHOST_DMABUF_MSG(string, args...)                                      \
	pr_info("[VHOST-DMABUF] " string, ##args)
#define VHOST_DMABUF_DBG(string, args...)                                      \
	pr_debug("[VHOST-DMABUF] " string, ##args)


#define MAX_MEM_ENTRIES 16

struct virtio_dmabuf_mem_entry {
	uint64_t addr;
	uint32_t length;
	uint32_t padding;
};

struct export_dmabuf {
	__u32 nr_entries;
	struct virtio_dmabuf_mem_entry *entries;
	int fd;
};

#endif
