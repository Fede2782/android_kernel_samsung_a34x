// SPDX-License-Identifier: GPL-2.0-only
/*
 * CPU-agnostic ARM page table allocator.
 * Host-specific functions. The rest is in io-pgtable-arm-common.c.
 *
 * Copyright (C) 2014 ARM Limited
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt)	"arm-lpae io-pgtable: " fmt

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/io-pgtable-arm.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>

#include <asm/barrier.h>

#include "mtk-io-pgtable-arm.h"

bool selftest_running;

static dma_addr_t __arm_lpae_dma_addr(void *pages)
{
	return (dma_addr_t)virt_to_phys(pages);
}

void *__arm_lpae_alloc_pages(size_t size, gfp_t gfp, struct io_pgtable_cfg *cfg)
{
	struct device *dev = cfg->iommu_dev;
	int order = get_order(size);
	struct page *p;
	dma_addr_t dma;
	void *pages;

	if (gfp & __GFP_HIGHMEM) {
		gfp &= ~__GFP_HIGHMEM;
		WARN_ON_ONCE(1);
	}
	p = alloc_pages_node(dev_to_node(dev), gfp | __GFP_ZERO, order);
	if (!p)
		return NULL;

	pages = page_address(p);
	if (!cfg->coherent_walk) {
		dma = dma_map_single(dev, pages, size, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, dma))
			goto out_free;
		/*
		 * We depend on the IOMMU being able to work with any physical
		 * address directly, so if the DMA layer suggests otherwise by
		 * translating or truncating them, that bodes very badly...
		 */
		if (dma != virt_to_phys(pages))
			goto out_unmap;
	}

	return pages;

out_unmap:
	dev_err(dev, "Cannot accommodate DMA translation for IOMMU page tables\n");
	dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);
out_free:
	__free_pages(p, order);
	return NULL;
}

void __arm_lpae_free_pages(void *pages, size_t size, struct io_pgtable_cfg *cfg)
{
	if (!cfg->coherent_walk)
		dma_unmap_single(cfg->iommu_dev, __arm_lpae_dma_addr(pages),
				 size, DMA_TO_DEVICE);
	free_pages((unsigned long)pages, get_order(size));
}

void __arm_lpae_sync_pte(arm_lpae_iopte *ptep, int num_entries,
			 struct io_pgtable_cfg *cfg)
{
	dma_sync_single_for_device(cfg->iommu_dev, __arm_lpae_dma_addr(ptep),
				   sizeof(*ptep) * num_entries, DMA_TO_DEVICE);
}

static void arm_lpae_free_pgtable(struct io_pgtable *iop)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_to_data(iop);
	struct arm_lpae_io_pgtable_ext *data_ext = io_pgtable_to_data_ext(data);

	__arm_lpae_free_pgtable(data, data->start_level, data->pgd);
	kfree(data_ext);
}

static struct io_pgtable *
arm_64_lpae_alloc_pgtable_s1(struct io_pgtable_cfg *cfg, void *cookie)
{
	struct arm_lpae_io_pgtable_ext *data_ext;
	struct arm_lpae_io_pgtable *data;

	data_ext = kzalloc(sizeof(*data_ext), GFP_KERNEL);
	if (!data_ext)
		return NULL;

	data = &data_ext->data;

	if (arm_lpae_init_pgtable_s1(cfg, data))
		goto out_free_data;

	/* Looking good; allocate a pgd */
	data->pgd = __arm_lpae_alloc_pages(ARM_LPAE_PGD_SIZE(data),
					   GFP_KERNEL, cfg);
	if (!data->pgd)
		goto out_free_data;

	/* Ensure the empty pgd is visible before any actual TTBR write */
	wmb();

	/* TTBR */
	cfg->arm_lpae_s1_cfg.ttbr = virt_to_phys(data->pgd);

	/* Initialize the spinlock */
	spin_lock_init(&data_ext->split_lock);

	return &data->iop;

out_free_data:
	kfree(data_ext);
	return NULL;
}

static int arm_64_lpae_configure_s1(struct io_pgtable_cfg *cfg, size_t *pgd_size)
{
	int ret;
	struct arm_lpae_io_pgtable data = {};

	ret = arm_lpae_init_pgtable_s1(cfg, &data);
	if (ret)
		return ret;
	*pgd_size = sizeof(arm_lpae_iopte) << data.pgd_bits;
	return 0;
}

struct io_pgtable_init_fns mtk_io_pgtable_arm_64_lpae_s1_contig_fns = {
	.alloc		= arm_64_lpae_alloc_pgtable_s1,
	.free		= arm_lpae_free_pgtable,
	.configure	= arm_64_lpae_configure_s1,
};
