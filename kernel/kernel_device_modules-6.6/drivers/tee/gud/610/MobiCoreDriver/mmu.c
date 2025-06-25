// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2024 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/pgtable.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/pagemap.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/scatterlist.h>
#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
// We need to import module name
#if KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE
#include <linux/module.h>
MODULE_IMPORT_NS(DMA_BUF);
#endif
#endif
#ifdef CONFIG_ION
#ifdef CONFIG_ION_SYSTEM_HEAP /* Android kernel only */
#if KERNEL_VERSION(5, 4, 0) < LINUX_VERSION_CODE
#include <linux/ion.h>
#elif KERNEL_VERSION(4, 11, 12) < LINUX_VERSION_CODE
#include "../../drivers/staging/android/ion/ion.h"
#elif KERNEL_VERSION(3, 14, 0) < LINUX_VERSION_CODE
#include "../../drivers/staging/android/ion/ion_priv.h"
#endif /* KERNEL_VERSION */
#else /* CONFIG_ION_SYSTEM_HEAP */
#if KERNEL_VERSION(5, 4, 0) < LINUX_VERSION_CODE
#include "../../drivers/staging/android/ion/ion.h"
#elif KERNEL_VERSION(3, 14, 0) < LINUX_VERSION_CODE
/* very old Android kernel without ION_SYSTEM_HEAP falls here */
#include "../../drivers/staging/android/ion/ion_priv.h"
#endif
#endif /* CONFIG_ION_SYSTEM_HEAP */
#endif /* CONFIG_ION */

#include "platform.h"

#ifdef MC_XEN_FEBE
/* To get the MFN */
#include <linux/pfn.h>
#include <xen/page.h>
#endif

#include "public/mc_user.h"

#include "mci/mcimcp.h"

#include "main.h"
#include "mcp.h"	/* mcp_buffer_map */
#include "protocol.h"	/* protocol_fe_uses_pages_and_vas */
#include "mmu.h"
#include "mmu_internal.h"
#include "fastcall.h"

#define RATIO_PAGE_SIZE		(PAGE_SIZE / KINIBI_PAGE_SIZE)

#define PHYS_48BIT_MASK		(BIT(48) - 1)

/* Common */
#define MMU_BUFFERABLE		BIT(2)		/* AttrIndx[0] */
#define MMU_CACHEABLE		BIT(3)		/* AttrIndx[1] */
#define MMU_EXT_NG		BIT(11)		/* ARMv6 and higher */

/* LPAE */
#define MMU_TYPE_PAGE		(3 << 0)
#define MMU_NS			BIT(5)
#define MMU_AP_RW_ALL		BIT(6) /* AP[2:1], RW, at any privilege level */
#define	MMU_AP2_RO		BIT(7)
#define MMU_EXT_SHARED_64	(3 << 8)	/* SH[1:0], inner shareable */
#define MMU_EXT_AF		BIT(10)		/* Access Flag */
#define MMU_EXT_XN		(((u64)1) << 54) /* XN */

/* ION */
/* Trustonic Specific flag to detect ION mem */
#define MMU_ION_BUF		BIT(24)

#ifdef MC_SHADOW_BUFFER
static struct mutex		mmu_lock;

static void mmu_workqueue_handle(struct work_struct *work);

/*Creating work queue*/
DECLARE_WORK(mmu_workqueue, mmu_workqueue_handle);
#endif /*MC_SHADOW_BUFFER*/
/*
 * Specific case for kernel 4.4.168 that does not have the same
 * get_user_pages() implementation
 */
#if KERNEL_VERSION(4, 4, 167) < LINUX_VERSION_CODE && \
	KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	if (write)
		gup_flags |= FOLL_WRITE;

	return get_user_pages(NULL, mm, start, nr_pages, gup_flags, pages,
					NULL);
}
#elif KERNEL_VERSION(4, 6, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	return get_user_pages(NULL, mm, start, nr_pages, write, 0, pages, NULL);
}
#elif KERNEL_VERSION(4, 9, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	if (write)
		gup_flags |= FOLL_WRITE;
	/* gup_flags |= FOLL_CMA; */

	return get_user_pages_remote(NULL, mm, start, nr_pages, gup_flags,
				    0, pages, NULL);
}
#elif KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	if (write)
		gup_flags |= FOLL_WRITE;

	return get_user_pages_remote(NULL, mm, start, nr_pages, gup_flags,
				    pages, NULL);
}
#elif KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	if (write)
		gup_flags |= FOLL_WRITE;

	return get_user_pages_remote(NULL, mm, start, nr_pages, gup_flags,
				    pages, NULL, NULL);
}
#elif KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	gup_flags |= FOLL_LONGTERM;
	if (write)
		gup_flags |= FOLL_WRITE;

	return get_user_pages(start, nr_pages, gup_flags, pages, NULL);
}
#elif KERNEL_VERSION(6, 5, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	gup_flags |= FOLL_LONGTERM;
	if (write)
		gup_flags |= FOLL_WRITE;

	return pin_user_pages(start, nr_pages, gup_flags, pages, NULL);
}
#else
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	gup_flags |= FOLL_LONGTERM;
	if (write)
		gup_flags |= FOLL_WRITE;

	return pin_user_pages(start, nr_pages, gup_flags, pages);
}
#endif

#ifdef MC_SHADOW_BUFFER
struct tee_mmu_zombie {
	/* mmu descriptor */
	struct tee_mmu *mmu;
	/* The list entry to zombie mmu */
	struct list_head	list;
};

static struct list_head mmu_zombies = LIST_HEAD_INIT(mmu_zombies);

static void *mmu_allocate_shadow_buffer(size_t order)
{
	void *shadow_buffer = NULL;
	struct page *pages;

	pages = alloc_pages(GFP_KERNEL, order);
	if (!pages) {
		mc_dev_devel("allocate pages failed with order %d",
			     (int)order);
		return ERR_PTR(-ENOMEM);
	}

	shadow_buffer = page_address(pages);
	memset(shadow_buffer, 0, PAGE_SIZE * BIT(order));

	return shadow_buffer;
}

void tee_mmu_free_shadow(struct tee_mmu *mmu)
{
	size_t order = mmu->shadow.order;
	void *shadow_buffer = mmu->shadow.buffer;

	free_pages((unsigned long)shadow_buffer - mmu->offset, order);
	mmu->shadow.buffer = NULL;
}
#endif /* MC_SHADOW_BUFFER */

/* base must be a power of 2 or else f_modulo will WARN_ON */
static inline u32 f_modulo(u32 val, u32 base)
{
	WARN_ON(base & (base - 1));
	return val & (base - 1);
}

static inline long gup_local_repeat(struct mm_struct *mm, uintptr_t start,
				    unsigned long nr_pages, int write,
				    struct page **pages)
{
	int retries = 10;
	long ret = 0;

	while (retries--) {
		ret = gup_local(mm, start, nr_pages, write, pages);

		if (-EBUSY != ret && -ENOMEM != ret)
			break;
	}
	if (-EBUSY == ret || -ENOMEM == ret)
		mc_dev_err((int)ret, "gup_local_repeat failed");

	return ret;
}

static int tee_mmu_free(struct tee_mmu *mmu)
{
	unsigned long i, nr_pages_left = mmu->nr_pages;
	u64 *pte64_bak = 0;

#ifdef CONFIG_DMA_SHARED_BUFFER
	if (mmu->dma_buf) {
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
		dma_buf_unmap_attachment_unlocked(mmu->attach, mmu->sgt,
						  DMA_BIDIRECTIONAL);
#else
		dma_buf_unmap_attachment(mmu->attach, mmu->sgt,
					 DMA_BIDIRECTIONAL);
#endif
		dma_buf_detach(mmu->dma_buf, mmu->attach);
		dma_buf_put(mmu->dma_buf);
	}
#endif

	/* Release all locked user space pages */
	for (i = 0; i < mmu->nr_pmd_entries; i++) {
		union mmu_table *pte_table = &mmu->pte_tables[i];
		unsigned long nr_pages = nr_pages_left;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		nr_pages_left -= nr_pages;

		if (!pte_table->page)
			break;

		if (mmu->user && mmu->use_pages_and_vas) {
			struct page **page = pte_table->pages;
			int j;

			for (j = 0; j < nr_pages; j++, page++)
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
				put_page(*page);
#else
				unpin_user_page(*page);
#endif

			mmu->pages_locked -= nr_pages;
		} else if (mmu->user) {
			u64 *pte64 = pte_table->entries;
			pte_t pte;
			int j;

			for (j = 0; j < nr_pages; j++) {
#if (KERNEL_VERSION(4, 7, 0) > LINUX_VERSION_CODE) || defined(CONFIG_ARM)
				{
					pte = *pte64++;
					/* Unused entries are 0 */
					if (!pte)
						break;
				}
#else
				{
					unsigned long v_pte64 = 0;

					v_pte64 = *pte64;
					/*
					 * Check if the current page is shared.
					 * Otherwise, take the backup (*pte64
					 * == 0 means not shared)
					 */
					if (v_pte64 == 0 && pte64_bak != 0)
						v_pte64 = *pte64_bak;

					/* Real 16Kb to unpin */
					pte.pte =
						ALIGN_DOWN(v_pte64, PAGE_SIZE);
					/* Note: RATIO_PAGE_SIZE can be greater
					 * than 1. In this case, one kernel
					 * page contains multiple kinibi pages.
					 * Jump to avoid unpin the same page
					 * multiple times.
					 */
					pte64 = pte64 + RATIO_PAGE_SIZE;
					pte64_bak =
						pte64 - mmu->first_page_pos;

					/* Unused entries are 0 */
					if (!pte.pte)
						break;
				}
#endif

				/* pte_page() cannot return NULL */
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
				put_page(pte_page(pte));
#else
				unpin_user_page(pte_page(pte));
#endif
			}
			mmu->pages_locked -= nr_pages;
		}

		mc_dev_devel("free PTE #%lu", i);
		free_page(pte_table->page);
		mmu->pages_created--;
	}

	if (mmu->pmd_table.page) {
		mc_dev_devel("free PMD");
		free_page(mmu->pmd_table.page);
		mmu->pages_created--;
	}

	if (mmu->pages_created || mmu->pages_locked) {
		mc_dev_err(-EUCLEAN,
			   "leak detected: still in use %d, still locked %d",
			   mmu->pages_created, mmu->pages_locked);
		/* Do not free the mmu, so the error can be spotted */
		return -EUCLEAN;
	}

	if (mmu->deleter)
		mmu->deleter->delete(mmu->deleter->object);

#ifdef MC_SHADOW_BUFFER
	if (mmu->shadow.buffer)
		tee_mmu_free_shadow(mmu);
#endif /* MC_SHADOW_BUFFER */

	kfree(mmu);

	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_mmus);

	return 0;
}

static int tee_mmu_delete(struct tee_mmu *mmu)
{
#ifdef MC_SHADOW_BUFFER
	int ret = 0;
	struct tee_mmu_zombie *mmu_zombie = NULL;

	ret = fc_reclaim_buffer(mmu);
	if (ret != 0) {
		mmu_zombie = kzalloc(sizeof(*mmu_zombie), GFP_KERNEL);
		if (!mmu_zombie)
			return -ENOMEM;

		INIT_LIST_HEAD(&mmu_zombie->list);
		mmu_zombie->mmu = mmu;

		mutex_lock(&mmu_lock);
		list_add_tail(&mmu_zombie->list, &mmu_zombies);
		mutex_unlock(&mmu_lock);

		mc_dev_devel("Add %p in zombie", mmu);

		return -EACCES;
	}
#else
	fc_reclaim_buffer(mmu);
#endif /* MC_SHADOW_BUFFER */
	tee_mmu_free(mmu);

	return 0;
}

static struct tee_mmu *tee_mmu_create_common(const struct mcp_buffer_map *b_map)
{
	struct tee_mmu *mmu;
	int ret = -ENOMEM;
	unsigned long nr_pages =
		PAGE_ALIGN(b_map->offset + b_map->length) / PAGE_SIZE;

	/* Allow Registered Shared mem with valid pointer and zero size. */
	if (!nr_pages)
		nr_pages = 1;

	if (nr_pages > (PMD_ENTRIES_MAX * PTE_ENTRIES_MAX)) {
		ret = -EINVAL;
		mc_dev_err(ret, "data mapping exceeds %d pages: %lu",
			   PMD_ENTRIES_MAX * PTE_ENTRIES_MAX, nr_pages);
		return ERR_PTR(ret);
	}

	/* Allocate the struct */
	mmu = tee_mmu_create_and_init();
	if (IS_ERR(mmu))
		return mmu;

	/* The Xen front-end does not use PTEs */
	if (protocol_fe_uses_pages_and_vas())
		mmu->use_pages_and_vas = true;

	/* Buffer info */
	mmu->offset = b_map->offset;
	mmu->length = b_map->length;
	mmu->flags = b_map->flags;

	/* Pages info */
	mmu->nr_pages = nr_pages;
	mmu->nr_pmd_entries = (mmu->nr_pages + PTE_ENTRIES_MAX - 1) /
			    PTE_ENTRIES_MAX;
	mc_dev_devel("mmu->nr_pages %lu num_ptes_pages %zu",
		     mmu->nr_pages, mmu->nr_pmd_entries);

	/* Allocate a page for the L1 table, always used for DomU */
	mmu->pmd_table.page = get_zeroed_page(GFP_KERNEL);
	if (!mmu->pmd_table.page)
		goto end;

	mmu->pages_created++;

	return mmu;

end:
	tee_mmu_free(mmu);
	return ERR_PTR(ret);
}

static bool mmu_get_dma_buffer(struct tee_mmu *mmu, int va)
{
#ifdef CONFIG_DMA_SHARED_BUFFER
	struct dma_buf *buf;

	buf = dma_buf_get(va);
	if (IS_ERR(buf))
		return false;

	mmu->dma_buf = buf;
	mmu->attach = dma_buf_attach(mmu->dma_buf, g_ctx.mcd);
	if (IS_ERR(mmu->attach))
		goto err_attach;

#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	mmu->sgt = dma_buf_map_attachment_unlocked(mmu->attach,
						   DMA_BIDIRECTIONAL);
#else
	mmu->sgt = dma_buf_map_attachment(mmu->attach, DMA_BIDIRECTIONAL);
#endif
	if (IS_ERR(mmu->sgt))
		goto err_map;

	return true;

err_map:
	dma_buf_detach(mmu->dma_buf, mmu->attach);

err_attach:
	dma_buf_put(mmu->dma_buf);
#endif
	return false;
}

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
static struct tee_mmu *mmu_create_instance(struct mm_struct *mm,
					   const struct mc_ioctl_buffer *buf)
{
	struct tee_mmu	*mmu;
	const void	*data = (const void *)(uintptr_t)buf->va;
	const void	*reader = (const void *)((uintptr_t)data & PAGE_MASK);
	struct page	**pages;	/* Same as below, conveniently typed */
	unsigned long	chunk;
	struct mcp_buffer_map b_map = {
		.offset = (u32)(buf->va & ~PAGE_MASK),
		.length = buf->len,
		.flags = buf->flags,
	};
	bool		writeable = buf->flags & MC_IO_MAP_OUTPUT;
	int		ret = 0;
	struct page	**all_pages = NULL; /* Store pages pointers */

#ifndef CONFIG_DMA_SHARED_BUFFER
	if (buf->flags & MMU_ION_BUF) {
		mc_dev_err(-EINVAL, "ION buffers not supported by kernel");
		return ERR_PTR(-EINVAL);
	}
#endif

	/* Check input arguments */
	if (!(buf->flags & MMU_ION_BUF) && !buf->va)
		return ERR_PTR(-EINVAL);

	if (buf->flags & MMU_ION_BUF)
		/* buf->va is not a valid address. ION buffers are aligned */
		b_map.offset = 0;

	/* Allocate the struct */
	mmu = tee_mmu_create_common(&b_map);
	if (IS_ERR(mmu))
		return mmu;

	if (buf->flags & MMU_ION_BUF) {
		mc_dev_devel("Buffer is ION");
		/* Buffer is ION -
		 * va is the client's dma_buf fd, which should be converted
		 * to a struct sg_table * directly.
		 */
		if (!mmu_get_dma_buffer(mmu, buf->va)) {
			mc_dev_err(ret, "mmu_get_dma_buffer failed");
			ret = -EINVAL;
			goto end;
		}
	}
	/* Get an array to store page pointers */
	all_pages = kmalloc_array(mmu->nr_pages, sizeof(struct page *),
				  GFP_KERNEL);
	if (!all_pages) {
		ret = -ENOMEM;
		goto end;
	}

	pages = (struct page **)all_pages;
	for (chunk = 0; chunk < mmu->nr_pmd_entries; chunk++) {
		unsigned long nr_pages;
		int i;

		/* Size to map for this chunk */
		if (chunk == (mmu->nr_pmd_entries - 1))
			nr_pages = ((mmu->nr_pages - 1) % PTE_ENTRIES_MAX) + 1;
		else
			nr_pages = PTE_ENTRIES_MAX;

		/* Allocate a page to hold ptes that describe buffer pages */
		mmu->pte_tables[chunk].page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->pte_tables[chunk].page) {
			ret = -ENOMEM;
			goto end;
		}
		mmu->pages_created++;

		/* Add page address to pmd table if needed */
		if (mmu->use_pages_and_vas)
			mmu->pmd_table.vas[chunk] =
				mmu->pte_tables[chunk].page;

		/* Get pages */
		if (mmu->dma_buf) {
			/* Buffer is ION */
			struct sg_mapping_iter miter;
			struct page **page_ptr;
			unsigned int cnt = 0;
			unsigned int global_cnt = 0;

			page_ptr = pages;
			sg_miter_start(&miter, mmu->sgt->sgl,
				       mmu->sgt->nents,
				       SG_MITER_FROM_SG);

			while (sg_miter_next(&miter)) {
				if (((global_cnt) >=
				    (PTE_ENTRIES_MAX * chunk)) &&
				    cnt < nr_pages) {
					page_ptr[cnt] = miter.page;
					cnt++;
				}
				global_cnt++;
			}
			sg_miter_stop(&miter);
		} else if (mm) {
			long gup_ret;

			/* Buffer was allocated in user space */
#if KERNEL_VERSION(5, 7, 19) < LINUX_VERSION_CODE
			down_read(&mm->mmap_lock);
#else
			down_read(&mm->mmap_sem);
#endif
			/*
			 * Always try to map read/write from a Linux PoV, so
			 * Linux creates (page faults) the underlying pages if
			 * missing.
			 */
			gup_ret = gup_local_repeat(mm, (uintptr_t)reader,
						   nr_pages, 1, pages);
			if ((gup_ret == -EFAULT) && !writeable) {
				/*
				 * If mapping read/write fails, and the buffer
				 * is to be shared as input only, try to map
				 * again read-only.
				 */
				gup_ret = gup_local_repeat(mm,
							   (uintptr_t)reader,
							   nr_pages, 0, pages);
			}
#if KERNEL_VERSION(5, 7, 19) < LINUX_VERSION_CODE
			up_read(&mm->mmap_lock);
#else
			up_read(&mm->mmap_sem);
#endif
			if (gup_ret < 0) {
				ret = gup_ret;
				mc_dev_err(ret, "failed to get user pages @%p",
					   reader);
				goto end;
			}

			/* check if we could lock all pages. */
			if (gup_ret != nr_pages) {
				mmu->pages_locked += gup_ret;
				mc_dev_err((int)gup_ret,
					   "failed to get user pages");
				ret = -EINVAL;
				goto end;
			}

			reader += nr_pages * PAGE_SIZE;
			mmu->user = true;
			mmu->pages_locked += nr_pages;
		} else if (is_vmalloc_addr(data)) {
			/* Buffer vmalloc'ed in kernel space */
			for (i = 0; i < nr_pages; i++) {
				struct page *page = vmalloc_to_page(reader);

				if (!page) {
					ret = -EINVAL;
					mc_dev_err(ret,
						   "failed to map address");
					goto end;
				}

				pages[i] = page;
				reader += PAGE_SIZE;
			}
		} else {
			/* Buffer kmalloc'ed in kernel space */
			struct page *page = virt_to_page(reader);

			reader += nr_pages * PAGE_SIZE;
			for (i = 0; i < nr_pages; i++)
				pages[i] = page++;
		}

		/* Create Table of physical addresses*/
		if (mmu->use_pages_and_vas) {
			memcpy(mmu->pte_tables[chunk].pages, pages,
			       nr_pages * sizeof(*pages));
		}
		pages += nr_pages;
	}
	/*
	 * Create mmu pte table for Kinibi. A readme is given in
	 * mmu_readme.txt
	 */
	if (!mmu->use_pages_and_vas) {
		u32 ofs_kinibi, first_page_pos, length;
		u32 total_pages_kinibi, total_pages_linux;
		u64 phys_addr;
		void *adr;
		int c_pages, pg_linux, pg_kinibi, idx_mod, idx;
		int j, k, modulo, sum;

		ofs_kinibi = (u32)(buf->va & ~KINIBI_PAGE_MASK);
		first_page_pos  = mmu->offset / KINIBI_PAGE_SIZE;
		total_pages_linux = mmu->nr_pages;
		length = mmu->length;

		chunk = 0;
		k = 0;
		pg_linux = 0;
		pg_kinibi = 0;
		total_pages_kinibi = KINIBI_PAGE_ALIGN(ofs_kinibi + length);
		total_pages_kinibi = total_pages_kinibi  / KINIBI_PAGE_SIZE;
		/* Ensure 1 minimum page */
		if (total_pages_kinibi == 0)
			total_pages_kinibi = 1;

		sum = RATIO_PAGE_SIZE + first_page_pos;
		while (pg_kinibi < total_pages_kinibi) {
			c_pages = pg_linux;
			phys_addr = page_to_phys(all_pages[c_pages]);
			for (j = first_page_pos; j < sum; j++) {
				/*
				 * If first_page_pos is greater than 0,
				 * borrow first_page_pos pages from the
				 * next kernel page, i.e. first_page_pos = 2,
				 * borrow 2 pages
				 */
				if (j == RATIO_PAGE_SIZE) {
					c_pages++;
					phys_addr =
					page_to_phys(all_pages[c_pages]);
				}
				chunk = pg_kinibi / PTE_ENTRIES_MAX;
				idx_mod = f_modulo(pg_kinibi, PTE_ENTRIES_MAX);
				modulo = f_modulo(chunk, RATIO_PAGE_SIZE);
				idx = idx_mod + modulo * PTE_ENTRIES_MAX;

				/* Each pte_tables[i] contains RATIO_PAGE_SIZE
				 * chunks. For instance, RATIO_PAGE_SIZE = 4
				 * the size of pte table is extended to
				 * (4 * PTE_ENTRIES_MAX)
				 */
				k = chunk / RATIO_PAGE_SIZE;
				modulo = f_modulo(j, RATIO_PAGE_SIZE);
				mmu->pte_tables[k].entries[idx] =
					phys_addr + modulo * KINIBI_PAGE_SIZE;

				if (idx_mod == 0) {
					adr = &mmu->pte_tables[k].entries[idx];
					mmu->pmd_table.entries[chunk] =
							virt_to_phys(adr);
				}
				pg_kinibi++;
				if (pg_kinibi == total_pages_kinibi)
					break;
			}
			pg_linux++;
		}

		c_pages++;
		if (c_pages != total_pages_linux ||
		    pg_kinibi != total_pages_kinibi) {
			ret = -EINVAL;
			mc_dev_err(ret, "page number mismatch");
		}
		#ifndef MC_FFA_FASTCALL
		/*
		 * Update page offset value with kinibi-based one in
		 * case of legacy mode
		 */
		mmu->offset = ofs_kinibi;
		#endif
		mmu->first_page_pos = first_page_pos;
	}

end:
	if (ret) {
		if (all_pages) {
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
			release_pages(all_pages, mmu->pages_locked, 0);
#else
			release_pages(all_pages, mmu->pages_locked);
#endif
			kfree(all_pages);
		}

		mmu->pages_locked = 0;
		/* set tee_mmu_free not to read pte */
		mmu->user = 0;
		tee_mmu_free(mmu);
		return ERR_PTR(ret);
	}

	mmu->all_pages = all_pages;

	mc_dev_devel(
		"created mmu %p: %s va %llx len %u off %u flg %x pmd %lx",
		mmu, mmu->user ? "user" : "kernel", buf->va, mmu->length,
		mmu->offset, mmu->flags, mmu->pmd_table.page);
	return mmu;
}

static inline int tee_mmu_register_buffer(struct tee_mmu	*mmu,
					  const struct mc_ioctl_buffer *buf)
{
	int ret = -ENOMEM;
	struct page	**all_pages = NULL;

	all_pages = mmu->all_pages;
	if (all_pages) {
		ret = fc_register_buffer(all_pages, mmu, buf->tag);
		kfree(all_pages);
		mmu->all_pages = NULL;
		if (ret) {
#ifndef MTK_ADAPTED
			mc_dev_err(ret, "sharing buffer failed, free mmu %p", mmu);
#endif
			tee_mmu_free(mmu);
		}
	}
	return ret;
}

struct tee_mmu *tee_mmu_create(struct mm_struct *mm,
			       const struct mc_ioctl_buffer *buf)
{
	struct tee_mmu *mmu;
	struct mm_struct *temp_mm;
	int i;
	int ret = 0;
	struct mc_ioctl_buffer buf_in;
	void *shadow_buffer = NULL;
	u32 offset, length;
	size_t size, order;

	temp_mm = mm;
	memcpy(&buf_in, buf, sizeof(struct mc_ioctl_buffer));

	offset = offset_in_page(buf->va);
	length = buf->len;

	size = PAGE_ALIGN(offset + length);
	order = size ? get_order(size) : 0;

	i = 0;
	do {
		if (shadow_buffer)
			temp_mm = NULL;/*handle shadow buffer*/

		mmu = mmu_create_instance(temp_mm, &buf_in);

		/* if failed, no mmu nor all_pages */
		if (IS_ERR(mmu))
			return mmu;

		ret = tee_mmu_register_buffer(mmu, &buf_in);
		/*registered with success, break the loop*/
		if (!ret)
			break;
#ifdef MC_SHADOW_BUFFER
		/*memory register failed*/
		if (shadow_buffer) {
			free_pages((unsigned long)shadow_buffer, order);
			return ERR_PTR(ret);
		}

		/*(re)allocate a shadow buffer in kernel space*/
		shadow_buffer = mmu_allocate_shadow_buffer(order);
		if (IS_ERR(shadow_buffer))
			return ERR_PTR(-ENOMEM);

		buf_in.va = (__u64)shadow_buffer + offset;
		mc_dev_devel("shadow buffer %llx with order %lx",
			     (u64)shadow_buffer, order);
#else
		return ERR_PTR(ret);
#endif /* MC_SHADOW_BUFFER */
		i++;
	} while (i < 2);

	mmu->shadow.order = order;
	mmu->shadow.cva = (void *)buf->va;

#ifdef MC_SHADOW_BUFFER
	if (shadow_buffer) {/*set shadow buffer*/
		mmu->shadow.buffer = shadow_buffer + offset;
		tee_mmu_copy_to_shadow(mmu);
	}
#endif /* MC_SHADOW_BUFFER */

	mc_dev_devel("mmu %p shadow buffer %p client va %llx, order %d",
		     mmu, mmu->shadow.buffer, (u64)mmu->shadow.cva,
		     (int)mmu->shadow.order);
	return mmu;
}

struct tee_mmu *tee_mmu_wrap(struct tee_deleter *deleter, struct page **pages,
			     unsigned long nr_pages,
			     const struct mcp_buffer_map *b_map)
{
	int ret = -EINVAL;
#ifdef MC_XEN_FEBE
	struct tee_mmu *mmu;
	unsigned long chunk, nr_pages_left;

	/* Allocate the struct */
	mmu = tee_mmu_create_common(b_map);
	if (IS_ERR(mmu))
		return mmu;

	mmu->nr_pages = nr_pages;
	nr_pages_left = mmu->nr_pages;
	for (chunk = 0; chunk < mmu->nr_pmd_entries; chunk++) {
		unsigned long nr_pages_in_pte = nr_pages_left;
		u64 *pte;
		int i;

		if (nr_pages_in_pte > PTE_ENTRIES_MAX)
			nr_pages_in_pte = PTE_ENTRIES_MAX;

		nr_pages_left -= nr_pages_in_pte;

		/* Allocate a page to hold ptes that describe buffer pages */
		mmu->pte_tables[chunk].page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->pte_tables[chunk].page) {
			ret = -ENOMEM;
			goto err;
		}
		mmu->pages_created++;

		/* Add page address to pmd table if needed */
		mmu->pmd_table.entries[chunk] =
			virt_to_phys(mmu->pte_tables[chunk].addr);

		/* Convert to PTEs */
		pte = &mmu->pte_tables[chunk].entries[0];

		for (i = 0; i < nr_pages_in_pte; i++, pages++, pte++) {
			unsigned long phys;
			unsigned long pfn;

			phys = page_to_phys(*pages);
			phys &= PHYS_48BIT_MASK;
			pfn = PFN_DOWN(phys);
			*pte = __pfn_to_mfn(pfn) << PAGE_SHIFT;
		}
	}

	mmu->deleter = deleter;
	mmu->handle = mmu->pmd_table.page;
	mc_dev_devel("wrapped mmu %p: len %u off %u flg %x pmd table %lx",
		     mmu, mmu->length, mmu->offset, mmu->flags,
		     mmu->pmd_table.page);
	return mmu;

err:
	tee_mmu_free(mmu);
#endif
	return ERR_PTR(ret);
}

void tee_mmu_set_deleter(struct tee_mmu *mmu, struct tee_deleter *deleter)
{
	mmu->deleter = deleter;
}

static void tee_mmu_release(struct kref *kref)
{
	struct tee_mmu *mmu = container_of(kref, struct tee_mmu, kref);

	mc_dev_devel("free mmu %p: %s len %u off %u pmd table %lx",
		     mmu, mmu->user ? "user" : "kernel", mmu->length,
		     mmu->offset, mmu->pmd_table.page);
	tee_mmu_delete(mmu);
}

void tee_mmu_get(struct tee_mmu *mmu)
{
	kref_get(&mmu->kref);
}

void tee_mmu_put(struct tee_mmu *mmu)
{
	kref_put(&mmu->kref, tee_mmu_release);
}

u64 tee_mmu_get_handle(struct tee_mmu *mmu)
{
	return mmu->handle;
}

void tee_mmu_buffer(struct tee_mmu *mmu, struct mcp_buffer_map *map)
{
#ifdef MC_FFA_FASTCALL
	map->type = WSM_FFA;
	map->addr = mmu->handle;
#else
	map->type = WSM_L1;
#ifdef MC_FEBE
	if (mmu->use_pages_and_vas)
		map->addr = mmu->pmd_table.page;
	else
		map->addr = mmu->handle;
#else
	map->addr = mmu->handle;
#endif
#endif

	map->secure_va = 0;
	map->offset = mmu->offset;
	map->length = mmu->length;
	map->flags = mmu->flags;
#ifdef CONFIG_DMA_SHARED_BUFFER
	if (mmu->dma_buf) {
#if KERNEL_VERSION(3, 14, 0) < LINUX_VERSION_CODE
#ifdef CONFIG_ION
		/*
		 * TODO: this is lost on 5.11 kernel
		 * ION: We assume that if ION is available on the platform, it
		 * is the one used for sharing dma-buf so we can access priv to
		 * determine if the  buffer is cacheable to use the same
		 * attributes on TEE side
		 */
		if (!(((struct ion_buffer *)mmu->dma_buf->priv)->flags
			& ION_FLAG_CACHED)) {
#else
		if (mmu->attach->dev->coherent_dma_mask) {
#endif
			/* uncached buffers */
			map->type |= WSM_UNCACHED;
			mc_dev_devel("buffer Non cacheable");
		} else {
			mc_dev_devel("buffer cacheable");
		}
#endif
	}
#endif
	map->mmu = mmu;
}

int tee_mmu_debug_structs(struct kasnprintf_buf *buf, const struct tee_mmu *mmu)
{
	return kasnprintf(buf,
			  "\t\t\tmmu %pK: %s len %u off %u table %pK\n",
			  mmu, mmu->user ? "user" : "kernel", mmu->length,
			  mmu->offset, (void *)mmu->pmd_table.page);
}

struct tee_mmu *tee_mmu_create_and_init(void)
{
	struct tee_mmu *mmu;

	/* Allocate the mmu */
	mmu = kzalloc(sizeof(*mmu), GFP_KERNEL);
	if (!mmu)
		return ERR_PTR(-ENOMEM);
	/* Increment debug counter */
	atomic_inc(&g_ctx.c_mmus);
	kref_init(&mmu->kref);

	return mmu;
}

#ifdef MC_SHADOW_BUFFER
int tee_mmu_copy_to_shadow(struct tee_mmu *mmu)
{
	void *shadow_buffer, *client_va;
	size_t size;
	int ret = 0;

	/*need to check null pointer*/
	if (!mmu)
		return -EINVAL;

	/*if no shadow buffer, consider as succes*/
	if (!mmu->shadow.buffer)
		return ret;

	shadow_buffer = mmu->shadow.buffer;
	client_va = mmu->shadow.cva;
	size = (size_t)mmu->length;

	mc_dev_devel("mmu %p copy to shadow %llx from client %llx size %d",
		     mmu, (u64)shadow_buffer, (u64)client_va, (int)size);

	/*if client is from kernel*/
	if (is_vmalloc_addr(client_va))
		memcpy(shadow_buffer, client_va, size);
	else
		ret = (int)copy_from_user(shadow_buffer, client_va, size);

	if (ret != 0) {
		mc_dev_devel("%d bytes copy from user failed", ret);
		ret = -EFAULT;
	}

	mc_dev_devel("1st 2 bytes in shadow buffer %02x %02x",
		     *(u8 *)shadow_buffer,
		     *((u8 *)shadow_buffer + 1));

	return ret;
}

int tee_mmu_copy_from_shadow(struct tee_mmu *mmu)
{
	void *shadow_buffer, *client_va;
	size_t size;
	int ret = 0;

	/*need to check null pointer*/
	if (!mmu)
		return -EINVAL;

	/*if no shadow buffer, consider as succes*/
	if (!mmu->shadow.buffer)
		return ret;

	shadow_buffer = mmu->shadow.buffer;
	client_va = mmu->shadow.cva;
	size = (size_t)mmu->length;

	mc_dev_devel("mmu %p copy from shadow %llx to client %llx size %d",
		     mmu, (u64)shadow_buffer, (u64)client_va, (int)size);

	mc_dev_devel("1st 2 bytes in shadow buffer %02x %02x",
		     *(u8 *)shadow_buffer,
		     *((u8 *)shadow_buffer + 1));

	/*if client is from kernel*/
	if (is_vmalloc_addr(client_va))
		memcpy(client_va, shadow_buffer, size);
	else
		ret = (int)copy_to_user(client_va, shadow_buffer, size);

	if (ret != 0) {
		mc_dev_devel("%d bytes copy from shadow failed", ret);
		ret = -EFAULT;
	}

	return ret;
}

static void mmu_workqueue_handle(struct work_struct *work)
{
	struct tee_mmu_zombie *mmu_zombie = NULL;
	struct tee_mmu_zombie *nop = NULL;
	struct tee_mmu *mmu = NULL;
	int ret = 0;

	mutex_lock(&mmu_lock);
	list_for_each_entry_safe(mmu_zombie, nop, &mmu_zombies, list) {
		mmu = mmu_zombie->mmu;
		ret = fc_reclaim_buffer(mmu);
		if (ret == 0) {
			tee_mmu_free(mmu);
			list_del(&mmu_zombie->list);
			kfree(mmu_zombie);
		}
		mc_dev_devel("zombie delete mmu %p, handle %llx in %p return %d",
			     mmu, mmu->handle, mmu_zombie, ret);
	}
	mutex_unlock(&mmu_lock);
}

void tee_mmu_delete_zombies(void)
{
	schedule_work(&mmu_workqueue);
}

void mmu_init(void)
{
	mutex_init(&mmu_lock);
}
#endif /* MC_SHADOW_BUFFER  */
