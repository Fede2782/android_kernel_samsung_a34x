// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/irqflags.h>
#include <linux/timekeeping.h>

#include "mtk_ccu_common_mssv.h"

#define MTK_CCU_TAG "[ccu_rproc]"
#define LOG_ERR(format, args...) \
	pr_err(MTK_CCU_TAG "[%s] " format, __func__, ##args)

static inline unsigned int mtk_ccu_mstojiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

void mtk_ccu_memclr(void *dst, int len)
{
	int i = 0;
	uint32_t *dstPtr = (uint32_t *)dst;

	for (i = 0; i < len/4; i++)
		writel(0, dstPtr + i);
}

void mtk_ccu_memcpy(void *dst, const void *src, uint32_t len)
{
	int i, copy_len;
	uint32_t data = 0;
	uint32_t align_data = 0;

#if (CCU_MORE_DEBUG)
	int ecount = 0;

	LOG_ERR("dst@0x%llx src@0x%llx len=%d", (uint64_t) dst, (uint64_t) src, len);
#endif

	for (i = 0; i < len/4; ++i)
		writel(*((uint32_t *)src+i), (uint32_t *)dst+i);

	if ((len % 4) != 0) {
		copy_len = len & ~(0x3);
		for (i = 0; i < 4; ++i) {
			if (i < (len%4)) {
				data = *((char *)src + copy_len + i);
				align_data += data << (8 * i);
			}
		}
		writel(align_data, (uint32_t *)dst + len/4);
	}

#if (CCU_MORE_DEBUG)
	for (i = 0; i < (len >> 2); ++i)
		if (readl((uint32_t *)dst+i) != *((uint32_t *)src+i)) {
			LOG_ERR("mismatch @ %d-th uint32_t", i);
			if (++ecount > 16)
				break;
		}
#endif
}

struct mtk_ccu_mem_info *mtk_ccu_get_meminfo(struct mtk_ccu *ccu,
	enum mtk_ccu_buffer_type type)
{
	if (type >= MTK_CCU_BUF_MAX)
		return NULL;
	else
		return &ccu->buffer_handle[type].meminfo;
}

int rproc_bootx(struct rproc *rproc, unsigned int uid)
{
	struct mtk_ccu *ccu;
	int ret;

	if (!rproc) {
		LOG_ERR("rproc is NULL");
		return -EINVAL;
	}

	ccu = (struct mtk_ccu *)rproc->priv;
	if (!ccu) {
		LOG_ERR("ccu is NULL");
		return -EINVAL;
	}

	if (uid < RPROC_UID_MAX)
		atomic_inc(&ccu->bootcnt[uid][0]);

	ret = rproc_boot(rproc);

	if ((ret) && (uid < RPROC_UID_MAX))
		atomic_inc(&ccu->bootcnt[uid][1]);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_bootx);

int rproc_shutdownx(struct rproc *rproc, unsigned int uid)
{
	struct mtk_ccu *ccu;
	int ret;

	if (!rproc) {
		LOG_ERR("rproc is NULL");
		return -EINVAL;
	}

	ccu = (struct mtk_ccu *)rproc->priv;
	if (!ccu) {
		LOG_ERR("ccu is NULL");
		return -EINVAL;
	}

	if (uid < RPROC_UID_MAX)
		atomic_dec(&ccu->bootcnt[uid][0]);

	ret = rproc_shutdown(rproc);
	if ((ret) && (uid < RPROC_UID_MAX))
		atomic_inc(&ccu->bootcnt[uid][2]);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_shutdownx);

MODULE_DESCRIPTION("MTK CCU Rproc Driver");
MODULE_LICENSE("GPL");
