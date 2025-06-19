/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 * MTK heap api can be used by other modules
 */

/**
 * This file is used to export api for mtk dmabufheap users
 * Please don't add dmabufheap private info
 */
#ifndef _MTK_DMABUFHEAP_H
#define _MTK_DMABUFHEAP_H
#include <linux/dma-buf.h>

extern atomic64_t dma_heap_normal_total;

typedef void (*dmaheap_slc_callback)(struct dma_buf *dmabuf);

int mtk_dmaheap_register_slc_callback(dmaheap_slc_callback cb);
int mtk_dmaheap_unregister_slc_callback(void);

int dma_buf_set_gid(struct dma_buf *dmabuf, int gid);
int dma_buf_get_gid(struct dma_buf *dmabuf);

/* return 0 means error */
u64 dmabuf_to_secure_handle(const struct dma_buf *dmabuf);

int is_system_heap_dmabuf(const struct dma_buf *dmabuf);
int is_mtk_mm_heap_dmabuf(const struct dma_buf *dmabuf);
int is_uncached_dmabuf(const struct dma_buf *dmabuf);
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) && !IS_ENABLED(CONFIG_GRT_HYPERVISOR)
int is_mtk_sec_heap_dmabuf(const struct dma_buf *dmabuf);
#endif
#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
static inline int is_mtk_sec_heap_dmabuf(const struct dma_buf *dmabuf)
{
	return 0;
}
#endif
int is_support_secure_handle(const struct dma_buf *dmabuf);

long mtk_dma_buf_set_name(struct dma_buf *dmabuf, const char *buf);

#if (!(IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)))
/*
 * dmabuf_to_sec_id() - Get iommu_sec_id corresponding to dma-buf
 * @dmabuf: the dma-buf
 * @sec_hdl: for get secure handle
 * returns >0 means valid iomm_sec_id, -1 means error
 */
int dmabuf_to_sec_id(const struct dma_buf *dmabuf, u32 *sec_hdl);

/*
 * dmabuf_to_tmem_type() - Get trusted memory type corresponding to dma-buf
 * @dmabuf: the dma-buf
 * @sec_hdl: for get secure handle
 * returns >0 means valid the trusted memory type, -1 means error
 */
int dmabuf_to_tmem_type(const struct dma_buf *dmabuf, u32 *sec_hdl);
#endif

void dma_heap_pool_prefill(unsigned long size, const char *heap_name);
/*
 * in 32bit project compile the arithmetic division, the "/" will
 * cause the __aeabi_uldivmod error.
 *
 * use DO_DMA_BUFFER_COMMON_DIV and DO_DMA_BUFFER_COMMON_MOD to
 * intead "/".
 *
 */
#define DO_DMA_BUFFER_COMMON_DIV(x, base)({ \
	unsigned long result = 0; \
	if (sizeof(x) < sizeof(uint64_t)) \
		result = (x / base); \
	else { \
		uint64_t _source = (x); \
		do_div(_source, base); \
		result = _source; \
	} \
	result; \
})

#define DO_DMA_BUFFER_COMMON_MOD(x, base)({ \
	unsigned long result = 0; \
	if (sizeof(x) < sizeof(uint64_t)) \
		result = (x % base); \
	else { \
		uint64_t _source = (x); \
		result = do_div(_source, base); \
	} \
	result; \
})


#endif /* _MTK_DMABUFHEAP_DEBUG_H */
