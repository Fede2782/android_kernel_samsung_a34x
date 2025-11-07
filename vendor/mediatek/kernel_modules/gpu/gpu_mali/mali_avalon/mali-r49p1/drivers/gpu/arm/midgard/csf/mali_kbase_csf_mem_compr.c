// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2024 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/*
 * Base kernel memory compression module for CSF GPUs
 */
#include <mali_kbase.h>
#include <mali_kbase_reg_track.h>
#include <mali_kbase_trace_gpu_mem.h>
#include <mmu/mali_kbase_mmu.h>

#include <linux/crypto.h>
#include <linux/zsmalloc.h>

#define CREATE_TRACE_POINTS
#include <csf/mali_kbase_csf_mem_compr.h>

static const char *const backends[] = {
#if IS_ENABLED(CONFIG_CRYPTO_LZO)
	"lzo",	 "lzo-rle",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_LZ4)
	"lz4",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_LZ4HC)
	"lz4hc",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_842)
	"842",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_ZSTD)
	"zstd",
#endif
};

#define MALI_MAX_DEFAULT_COMPRESSION_BACKEND_NAME_LEN ((size_t)20)

static char default_compression_backend[MALI_MAX_DEFAULT_COMPRESSION_BACKEND_NAME_LEN] = "lz4";
module_param_string(compression_backend, default_compression_backend,
		    sizeof(default_compression_backend), 0444);
MODULE_PARM_DESC(
	compression_backend,
	"Memory compression backend, if the respective kernel configurations is enabled: lzo, lzo-rle, lz4, lz4hc, 842, zstd. Default: lzo");

char *kbase_zs_backend_get(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(backends); i++) {
		if (!strcmp(default_compression_backend, backends[i]))
			break;
	}

	if (i >= ARRAY_SIZE(backends)) {
		dev_err(dev, "Invalid compression backend: %s", default_compression_backend);
		return NULL;
	}

	return default_compression_backend;
}

static void mem_account_inc(struct kbase_context *kctx, u32 pages_decompressed,
			    unsigned long compressed_mem_dec)
{
	atomic_add(pages_decompressed, &kctx->used_pages);
	atomic_add(pages_decompressed, &kctx->kbdev->memdev.used_pages);
	kbase_process_page_usage_inc(kctx, pages_decompressed - compressed_mem_dec);
	kbase_trace_gpu_mem_usage_inc(kctx->kbdev, kctx, pages_decompressed);
}

static void mem_account_dec(struct kbase_context *kctx, u32 pages_compressed,
			    unsigned long compressed_mem_inc)
{
	atomic_sub(pages_compressed, &kctx->used_pages);
	atomic_sub(pages_compressed, &kctx->kbdev->memdev.used_pages);
	kbase_process_page_usage_dec(kctx, pages_compressed - compressed_mem_inc);
	kbase_trace_gpu_mem_usage_dec(kctx->kbdev, kctx, pages_compressed);
}

static u32 free_partial_page(struct kbase_context *kctx, int group_id, struct page *p)
{
	struct page *head_page;
	struct kbase_sub_alloc *sa;
	u32 nr_pages_to_account = 0;

	head_page = (struct page *)p->lru.prev;
	sa = (struct kbase_sub_alloc *)head_page->lru.next;
	spin_lock(&kctx->mem_partials_lock);
	clear_bit(p - head_page, sa->sub_pages);
	if (bitmap_empty(sa->sub_pages, NUM_PAGES_IN_2MB_LARGE_PAGE)) {
		list_del(&sa->link);
		kbase_mem_pool_free_page(&kctx->mem_pools.large[group_id], head_page);
		kfree(sa);
		nr_pages_to_account = NUM_PAGES_IN_2MB_LARGE_PAGE;
	} else if (bitmap_weight(sa->sub_pages, NUM_PAGES_IN_2MB_LARGE_PAGE) ==
		   NUM_PAGES_IN_2MB_LARGE_PAGE - 1) {
		/* expose the partial again */
		list_add(&sa->link, &kctx->mem_partials);
	}
	spin_unlock(&kctx->mem_partials_lock);

	return nr_pages_to_account;
}

static struct page *alloc_page_for_decompression(struct kbase_context *kctx,
						 struct kbase_mem_pool *pool)
{
	struct page *p = NULL;

	do {
		int err;

		p = kbase_mem_pool_alloc(pool);
		if (p)
			break;

		err = kbase_mem_pool_grow(pool, 1, kctx->task);
		if (err)
			break;
	} while (1);

	return p;
}

static int compress(struct kbase_device *kbdev, const u8 *src, unsigned int slen, u8 *dst,
		    unsigned int *dlen)
{
	int ret;

	mutex_lock(&kbdev->csf.comp_mutex);
	ret = crypto_comp_compress(kbdev->csf.cc, src, slen, dst, dlen);
	mutex_unlock(&kbdev->csf.comp_mutex);

	return ret;
}

static int decompress(struct kbase_device *kbdev, const u8 *src, unsigned int slen, u8 *dst,
		      unsigned int *dlen)
{
	int ret;

	mutex_lock(&kbdev->csf.comp_mutex);
	ret = crypto_comp_decompress(kbdev->csf.cc, src, slen, dst, dlen);
	mutex_unlock(&kbdev->csf.comp_mutex);

	return ret;
}

static u32 compress_large_page(struct kbase_context *kctx, struct kbase_va_region *reg, u32 index)
{
	struct kbase_device *kbdev = kctx->kbdev;
	struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
	struct page *page = as_page(phys_alloc->pages[index]);
	unsigned int compressed_size = SZ_2M - sizeof(u32);
	unsigned int remaining_size, compressed_size_in_pages, block_size;
	void *zs_mapping, *kaddr;
	unsigned long zs_handle;
	u8 *compressed_buf;
	unsigned int tag;
	phys_addr_t base_phys_addr;
	int ret, i, j;
	const unsigned int scratch_buf_size = SZ_2M;
	u8 *scratch_buf;
	struct page **scratch_pages_array;
	u32 out = 0;

	if (!IS_ENABLED(CONFIG_ZSMALLOC))
		return 0;

	scratch_buf = vmalloc(scratch_buf_size);
	if (!scratch_buf) {
		dev_err(kbdev->dev, "Could not allocate temporary compression buffer: %ld",
				PTR_ERR(scratch_buf));
		return 0;
	}

	scratch_pages_array =
		kmalloc_array(NUM_PAGES_IN_2MB_LARGE_PAGE, sizeof(struct page *), GFP_KERNEL);
	if (!scratch_pages_array) {
		dev_err(kbdev->dev, "Could not allocate temporary compression pages array: %ld",
				PTR_ERR(scratch_pages_array));
		kfree(scratch_buf);
		return 0;
	}

	dma_sync_single_for_cpu(kbdev->dev, kbase_dma_addr(page), SZ_2M, DMA_BIDIRECTIONAL);

	if (likely(!PageHighMem(page))) {
		/* Use the linear mapping of kernel */
		kaddr = lowmem_page_address(page);
	} else {
		for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++)
			scratch_pages_array[i] = page + i;

		kaddr = vmap(scratch_pages_array, NUM_PAGES_IN_2MB_LARGE_PAGE, VM_MAP, PAGE_KERNEL);
		if (!kaddr) {
			dev_warn(
				kbdev->dev,
				"vmap failed for compression of large page with VA %llx of ctx %d_%d",
				(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
			goto exit;
		}
	}

	ret = compress(kbdev, kaddr, SZ_2M, scratch_buf, &compressed_size);
	if (unlikely(PageHighMem(page)))
		vunmap(kaddr);
	if (ret) {
		dev_warn(
			kbdev->dev,
			"Compression failed with error %d for large page with VA %llx of ctx %d_%d",
			ret, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		goto exit;
	}
	if (compressed_size > (15 * SZ_2M / 16)) {
		dev_dbg(
			kbdev->dev,
			"Compressed size %u larger than expected for large page with VA %llx of ctx %d_%d",
			compressed_size, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid,
			kctx->id);
		goto exit;
	}

	compressed_size_in_pages = PFN_UP(compressed_size + sizeof(u32));
	remaining_size = compressed_size + sizeof(u32);
	compressed_buf = scratch_buf;
	for (i = 0; i < compressed_size_in_pages; i++) {
		tag = ZS_ALLOC_LARGE;
		block_size = MIN((u32)PAGE_SIZE, remaining_size);
		remaining_size -= block_size;
		zs_handle = zs_malloc(kctx->csf.zs_pool, block_size,
				      __GFP_NOWARN | __GFP_HIGHMEM | __GFP_MOVABLE);
		if (IS_ERR_VALUE(zs_handle)) {
			dev_warn(
				kbdev->dev,
				"Failed to alloc mem to store compressed data of %u bytes for large page VA %llx of ctx %d_%d",
				block_size, (reg->start_pfn + index + i) << PAGE_SHIFT, kctx->tgid,
				kctx->id);
			goto rollback;
		}
		if (unlikely(zs_handle & TAG_MASK)) {
			dev_warn(
				kbdev->dev,
				"Unsupported value for memory handle of compressed data of large page VA %llx of ctx %d_%d",
				(reg->start_pfn + index + i) << PAGE_SHIFT, kctx->tgid, kctx->id);
			zs_free(kctx->csf.zs_pool, zs_handle);
			goto rollback;
		}

		zs_mapping = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_WO);
		if (i == 0) {
			*(u32 *)zs_mapping = compressed_size;
			zs_mapping += sizeof(u32);
			block_size -= sizeof(u32);
			tag = ZS_ALLOC_LARGE_HEAD;
		}
		memcpy(zs_mapping, compressed_buf, block_size);
		compressed_buf += block_size;
		zs_unmap_object(kctx->csf.zs_pool, zs_handle);

		phys_alloc->pages[index + i] = as_tagged_tag((phys_addr_t)zs_handle, tag);
	}

	for (i = compressed_size_in_pages; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++)
		phys_alloc->pages[index + i] =
			as_tagged_tag(KBASE_INVALID_PHYSICAL_ADDRESS, ZS_ALLOC_LARGE);

	kbase_mem_pool_free_page(&kctx->mem_pools.large[phys_alloc->group_id], page);
	phys_alloc->compressed_nents += NUM_PAGES_IN_2MB_LARGE_PAGE;
	atomic_add(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->csf.compressed_pages_cnt);
	atomic_add(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->kbdev->memdev.compressed_pages_total);

	out = NUM_PAGES_IN_2MB_LARGE_PAGE;
	goto exit;
rollback:
	base_phys_addr = as_phys_addr_t(phys_alloc->pages[index + i]) & ~(SZ_2M - 1UL);
	for (j = 0; j < i; j++) {
		tag = j ? HUGE_PAGE : HUGE_HEAD;
		zs_handle = (unsigned long)as_phys_addr_t(phys_alloc->pages[index + j]);
		zs_free(kctx->csf.zs_pool, zs_handle);
		phys_alloc->pages[index + j] = as_tagged_tag(base_phys_addr + PAGE_SIZE * j, tag);
	}
exit:
	kfree(scratch_pages_array);
	vfree(scratch_buf);

	return out;
}

static u32 compress_small_page(struct kbase_context *kctx, struct kbase_va_region *reg, u32 index,
			       bool is_partial)
{
	struct kbase_device *kbdev = kctx->kbdev;
	struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
	struct page *page = as_page(phys_alloc->pages[index]);
	unsigned int compressed_size = 2 * PAGE_SIZE - sizeof(u16);
	void *zs_mapping, *src;
	unsigned long zs_handle;
	int ret;
	const unsigned int scratch_buf_size = 2 * PAGE_SIZE;
	u8 *scratch_buf = NULL;

	if (!IS_ENABLED(CONFIG_ZSMALLOC))
		/* No page was compressed */
		return 0;

	scratch_buf = kmalloc(scratch_buf_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(scratch_buf)) {
		dev_err(kbdev->dev, "Could not allocate temporary compression buffer: %ld",
				PTR_ERR(scratch_buf));
		/* No page was compressed */
		return 0;
	}

	dma_sync_single_for_cpu(kbdev->dev, kbase_dma_addr(page), PAGE_SIZE, DMA_BIDIRECTIONAL);

	src = kbase_kmap(page);
	ret = compress(kbdev, src, PAGE_SIZE, scratch_buf, &compressed_size);
	kbase_kunmap(page, src);
	if (ret) {
		dev_warn(kbdev->dev, "Compression failed with error %d for VA %llx of ctx %d_%d",
			 ret, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		/* No page was compressed */
		ret = 0;
		goto exit;
	}
	if (compressed_size > (15 * PAGE_SIZE / 16)) {
		dev_dbg(kbdev->dev,
			"Compressed size %u larger than expected for VA %llx of ctx %d_%d",
			compressed_size, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid,
			kctx->id);
		/* No page was compressed */
		ret = 0;
		goto exit;
	}

	zs_handle = zs_malloc(kctx->csf.zs_pool, compressed_size + sizeof(u16),
			      __GFP_NOWARN | __GFP_HIGHMEM | __GFP_MOVABLE);
	if (IS_ERR_VALUE(zs_handle)) {
		dev_warn(
			kbdev->dev,
			"Failed to alloc mem to store compressed data of %u bytes for VA %llx of ctx %d_%d",
			compressed_size, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid,
			kctx->id);
		/* No page was compressed */
		ret = 0;
		goto exit;
	}
	if (unlikely(zs_handle & TAG_MASK)) {
		dev_warn(
			kbdev->dev,
			"Unsupported value for memory handle of compressed data of VA %llx of ctx %d_%d",
			(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		zs_free(kctx->csf.zs_pool, zs_handle);
		/* No page was compressed */
		ret = 0;
		goto exit;
	}

	zs_mapping = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_WO);
	*(u16 *)zs_mapping = compressed_size;
	memcpy(zs_mapping + sizeof(u16), scratch_buf, compressed_size);
	zs_unmap_object(kctx->csf.zs_pool, zs_handle);

	phys_alloc->pages[index] = as_tagged_tag((phys_addr_t)zs_handle, ZS_ALLOC);

	/* Compress 1 page  */
	phys_alloc->compressed_nents += 1;
	atomic_add(1, &kctx->csf.compressed_pages_cnt);
	atomic_add(1, &kctx->kbdev->memdev.compressed_pages_total);
	if (!is_partial) {
		/* Free the 1 page, if not partial */
		kbase_mem_pool_free_page(&kctx->mem_pools.small[phys_alloc->group_id], page);
		ret = 1;
	} else {
		ret = free_partial_page(kctx, phys_alloc->group_id, page);
	}
exit:
	kfree(scratch_buf);
	return ret;
}

static bool compress_region(struct kbase_context *kctx, struct kbase_va_region *reg,
			    u32 *pages_freed)
{
	const unsigned int max_compressed_pages = kctx->csf.max_compressed_pages_cnt;
	struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
	struct tagged_addr *pages = phys_alloc->pages;
	bool stop_compression = false;
	u32 freed = 0;
	size_t i;
	int ret = 0;

	mutex_lock(&kctx->kbdev->csf.reg_lock);
	kbase_mem_shrink_cpu_mapping(kctx, reg, 0, phys_alloc->nents);

	/* unmap all pages from GPU */
        ret = kbase_mem_shrink_gpu_mapping(kctx, reg, 0, phys_alloc->nents);
        if (ret) {
		dev_err(kctx->kbdev->dev, "kctx %d_%d compress mem shrink failed", kctx->tgid, kctx->id);
		return ret;
	}
	mutex_unlock(&kctx->kbdev->csf.reg_lock);

	for (i = 0; i < phys_alloc->nents; i++) {
		if (is_compressed_small(pages[i]))
			continue;

		if (is_compressed_large_head(pages[i])) {
			i += (NUM_PAGES_IN_2MB_LARGE_PAGE - 1);
			continue;
		}

		if (atomic_read(&kctx->csf.compressed_pages_cnt) == max_compressed_pages) {
			stop_compression = true;
			break;
		}

		if (!is_huge(pages[i]) && !is_partial(pages[i]))
			freed += compress_small_page(kctx, reg, i, false);
		else if (is_huge_head(phys_alloc->pages[i])) {
			if ((atomic_read(&kctx->csf.compressed_pages_cnt) +
			     NUM_PAGES_IN_2MB_LARGE_PAGE) <= max_compressed_pages)
				freed += compress_large_page(kctx, reg, i);

			/* Move past the current large page */
			i += (NUM_PAGES_IN_2MB_LARGE_PAGE - 1);
		} else if (is_partial(pages[i]))
			freed += compress_small_page(kctx, reg, i, true);
	}

	*pages_freed += freed;
	return stop_compression;
}

static int decompress_large_page_using_small_pages(struct kbase_context *kctx, struct kbase_va_region *reg, u32 index)
{
        struct kbase_device *kbdev = kctx->kbdev;
        struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
        unsigned int compressed_size, compressed_size_in_pages, block_size, remaining_size;
        unsigned int dst_len = SZ_2M;
        void *compressed_buf, *dst;
        unsigned long zs_handle;
        void *zs_mapping;
        struct page *page;
        int ret = 0;
        int i;
        const unsigned int scratch_buf_size = SZ_2M;
        u8 *scratch_buf;
        struct page **scratch_pages_array;

	if (!IS_ENABLED(CONFIG_ZSMALLOC))
		return 0;

	if (WARN_ON_ONCE(!is_compressed_large_head(phys_alloc->pages[index])))
		return -EINVAL;

	scratch_buf = vmalloc(scratch_buf_size);
	if (!scratch_buf) {
		dev_err(kbdev->dev, "Could not allocate temporary decompression buffer: %ld",
			PTR_ERR(scratch_buf));
		return 0;
	}

	scratch_pages_array =
		kmalloc_array(NUM_PAGES_IN_2MB_LARGE_PAGE, sizeof(struct page *), GFP_KERNEL);
	if (!scratch_pages_array) {
		dev_err(kbdev->dev, "Could not allocate temporary decompression pages array: %ld",
			PTR_ERR(scratch_pages_array));
		kfree(scratch_buf);
		return 0;
	}

	for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++) {
		page = alloc_page_for_decompression(kctx, &kctx->mem_pools.small[phys_alloc->group_id]);
		if (!page) {
			dev_warn(
					kbdev->dev,
					"Failed to allocate page for decompression of small page with VA %llx of ctx %d_%d",
					(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
			/* free previous allocated pages */
		        for(;i > 0;i--)
				kbase_mem_pool_free_page(&kctx->mem_pools.small[phys_alloc->group_id],
					scratch_pages_array[i-1]);
			goto exit;
		}

		scratch_pages_array[i] = page;
	}

	dst = vmap(scratch_pages_array, NUM_PAGES_IN_2MB_LARGE_PAGE, VM_MAP, PAGE_KERNEL);
	if (!dst) {
		dev_warn(
				kbdev->dev,
				"vmap failed for decompression of large page with VA %llx of ctx %d_%d",
				(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++)
			kbase_mem_pool_free_page(&kctx->mem_pools.small[phys_alloc->group_id],
					scratch_pages_array[i]);
		ret = -ENOMEM;
		goto exit;
	}

	compressed_buf = scratch_buf;

	zs_handle = (unsigned long)as_phys_addr_t(phys_alloc->pages[index]);
	zs_mapping = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_RO);
	compressed_size = *(u32 *)zs_mapping;
	block_size = MIN((u32)PAGE_SIZE, compressed_size);
	block_size -= sizeof(u32);
	memcpy(compressed_buf, zs_mapping + sizeof(u32), block_size);
	zs_unmap_object(kctx->csf.zs_pool, zs_handle);

	compressed_buf += block_size;
	remaining_size = compressed_size - block_size;
	compressed_size_in_pages = PFN_UP(compressed_size + sizeof(u32));

	for (i = 1; i < compressed_size_in_pages; i++) {
		block_size = MIN((u32)PAGE_SIZE, remaining_size);

		zs_handle = (unsigned long)as_phys_addr_t(phys_alloc->pages[index + i]);
		zs_mapping = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_RO);
		memcpy(compressed_buf, zs_mapping, block_size);
		zs_unmap_object(kctx->csf.zs_pool, zs_handle);

		compressed_buf += block_size;
		remaining_size -= block_size;
	}

	ret = decompress(kbdev, scratch_buf, compressed_size, dst, &dst_len);
	if (ret) {
		dev_warn(
			kbdev->dev,
			"Decompression failed with error %d for large page with VA %llx of ctx %d_%d",
			ret, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++)
			kbase_mem_pool_free_page(&kctx->mem_pools.small[phys_alloc->group_id],
					scratch_pages_array[i]);
		goto exit;
	}

	for (i = 0; i < compressed_size_in_pages; i++) {
		zs_free(kctx->csf.zs_pool,
			(unsigned long)as_phys_addr_t(phys_alloc->pages[index + i]));
	}

	for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++) {
		phys_alloc->pages[index + i] =
			as_tagged(page_to_phys(scratch_pages_array[i]));
		dma_sync_single_for_device(kbdev->dev, kbase_dma_addr(scratch_pages_array[i]), PAGE_SIZE, DMA_BIDIRECTIONAL);
	}

	phys_alloc->compressed_nents -= NUM_PAGES_IN_2MB_LARGE_PAGE;
	atomic_sub(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->csf.compressed_pages_cnt);
	atomic_sub(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->kbdev->memdev.compressed_pages_total);

exit:
	kfree(scratch_pages_array);
	vfree(scratch_buf);

	return ret;
}

static int decompress_large_page(struct kbase_context *kctx, struct kbase_va_region *reg, u32 index)
{
	struct kbase_device *kbdev = kctx->kbdev;
	struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
	unsigned int compressed_size, compressed_size_in_pages, block_size, remaining_size;
	unsigned int dst_len = SZ_2M;
	void *compressed_buf, *dst;
	unsigned long zs_handle;
	void *zs_mapping;
	struct page *page;
	int ret = 0;
	int i;
	const unsigned int scratch_buf_size = SZ_2M;
	u8 *scratch_buf;
	struct page **scratch_pages_array;

	if (!IS_ENABLED(CONFIG_ZSMALLOC))
		return 0;

	if (WARN_ON_ONCE(!is_compressed_large_head(phys_alloc->pages[index])))
		return -EINVAL;

	scratch_buf = vmalloc(scratch_buf_size);
	if (!scratch_buf) {
		dev_err(kbdev->dev, "Could not allocate temporary decompression buffer: %ld",
			PTR_ERR(scratch_buf));
		return 0;
	}

	scratch_pages_array =
		kmalloc_array(NUM_PAGES_IN_2MB_LARGE_PAGE, sizeof(struct page *), GFP_KERNEL);
	if (!scratch_pages_array) {
		dev_err(kbdev->dev, "Could not allocate temporary decompression pages array: %ld",
			PTR_ERR(scratch_pages_array));
		kfree(scratch_buf);
		return 0;
	}

	page = alloc_page_for_decompression(kctx, &kctx->mem_pools.large[phys_alloc->group_id]);
	if (!page) {
		dev_info(
			kbdev->dev,
			"Failed to allocate page for decompression of large page with VA %llx of ctx %d_%d fallback to small pages",
			(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);

		/* free the allocated temp buffer */
		kfree(scratch_pages_array);
		vfree(scratch_buf);

		/* fallback to small pages */
		if (decompress_large_page_using_small_pages(kctx, reg, index)) {
			dev_warn(
					kbdev->dev,
					"Failed to allocate page for decompression of large page with VA %llx of ctx %d_%d",
					(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
			return -ENOMEM;
		} else {
			return 0;
		}
	}

	if (likely(!PageHighMem(page))) {
		/* Use the linear mapping of kernel */
		dst = lowmem_page_address(page);
	} else {
		for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++)
			scratch_pages_array[i] = page + i;

		dst = vmap(scratch_pages_array, NUM_PAGES_IN_2MB_LARGE_PAGE, VM_MAP, PAGE_KERNEL);
		if (!dst) {
			dev_warn(
				kbdev->dev,
				"vmap failed for decompression of large page with VA %llx of ctx %d_%d",
				(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
			kbase_mem_pool_free_page(&kctx->mem_pools.large[phys_alloc->group_id],
						 page);
			ret = -ENOMEM;
			goto exit;
		}
	}

	compressed_buf = scratch_buf;

	zs_handle = (unsigned long)as_phys_addr_t(phys_alloc->pages[index]);
	zs_mapping = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_RO);
	compressed_size = *(u32 *)zs_mapping;
	block_size = MIN((u32)PAGE_SIZE, compressed_size);
	block_size -= sizeof(u32);
	memcpy(compressed_buf, zs_mapping + sizeof(u32), block_size);
	zs_unmap_object(kctx->csf.zs_pool, zs_handle);

	compressed_buf += block_size;
	remaining_size = compressed_size - block_size;
	compressed_size_in_pages = PFN_UP(compressed_size + sizeof(u32));

	for (i = 1; i < compressed_size_in_pages; i++) {
		block_size = MIN((u32)PAGE_SIZE, remaining_size);

		zs_handle = (unsigned long)as_phys_addr_t(phys_alloc->pages[index + i]);
		zs_mapping = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_RO);
		memcpy(compressed_buf, zs_mapping, block_size);
		zs_unmap_object(kctx->csf.zs_pool, zs_handle);

		compressed_buf += block_size;
		remaining_size -= block_size;
	}

	ret = decompress(kbdev, scratch_buf, compressed_size, dst, &dst_len);
	if (unlikely(PageHighMem(page)))
		vunmap(dst);
	if (ret) {
		dev_warn(
			kbdev->dev,
			"Decompression failed with error %d for large page with VA %llx of ctx %d_%d",
			ret, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		kbase_mem_pool_free_page(&kctx->mem_pools.large[phys_alloc->group_id], page);
		goto exit;
	}

	for (i = 0; i < compressed_size_in_pages; i++) {
		zs_free(kctx->csf.zs_pool,
			(unsigned long)as_phys_addr_t(phys_alloc->pages[index + i]));
	}

	phys_alloc->pages[index] = as_tagged_tag(page_to_phys(page), HUGE_HEAD);
	for (i = 1; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++) {
		phys_alloc->pages[index + i] =
			as_tagged_tag(page_to_phys(page) + PAGE_SIZE * i, HUGE_PAGE);
	}

	dma_sync_single_for_device(kbdev->dev, kbase_dma_addr(page), SZ_2M, DMA_BIDIRECTIONAL);
	phys_alloc->compressed_nents -= NUM_PAGES_IN_2MB_LARGE_PAGE;
	atomic_sub(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->csf.compressed_pages_cnt);
	atomic_sub(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->kbdev->memdev.compressed_pages_total);

exit:
	kfree(scratch_pages_array);
	vfree(scratch_buf);

	return ret;
}

static int decompress_small_page(struct kbase_context *kctx, struct kbase_va_region *reg, u32 index)
{
	struct kbase_device *kbdev = kctx->kbdev;
	struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
	void *compressed_buf, *dst;
	struct page *page;
	unsigned long zs_handle;
	unsigned int compressed_size, dst_len = PAGE_SIZE;
	int ret;
	const unsigned int scratch_buf_size = 2 * PAGE_SIZE;
	u8 *scratch_buf = NULL;

	if (!IS_ENABLED(CONFIG_ZSMALLOC))
		return 0;

	if (WARN_ON_ONCE(!is_compressed(phys_alloc->pages[index])))
		return -EINVAL;

	page = alloc_page_for_decompression(kctx, &kctx->mem_pools.small[phys_alloc->group_id]);
	if (!page) {
		dev_warn(
			kbdev->dev,
			"Failed to allocate page for decompression of small page with VA %llx of ctx %d_%d",
			(reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		return -ENOMEM;
	}

	scratch_buf = kmalloc(scratch_buf_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(scratch_buf))
		dev_err(kbdev->dev, "Could not allocate temporary decompression buffer: %ld",
			PTR_ERR(scratch_buf));

	zs_handle = (unsigned long)as_phys_addr_t(phys_alloc->pages[index]);
	compressed_buf = zs_map_object(kctx->csf.zs_pool, zs_handle, ZS_MM_RO);
	compressed_size = *(u16 *)compressed_buf;
	compressed_buf += sizeof(u16);
	memcpy(scratch_buf, compressed_buf, compressed_size);
	zs_unmap_object(kctx->csf.zs_pool, zs_handle);

	dst = kbase_kmap(page);
	ret = decompress(kbdev, scratch_buf, compressed_size, dst, &dst_len);
	kbase_kunmap(page, dst);
	if (ret) {
		dev_warn(kbdev->dev, "Decompression failed with error %d for VA %llx of ctx %d_%d",
			 ret, (reg->start_pfn + index) << PAGE_SHIFT, kctx->tgid, kctx->id);
		kbase_mem_pool_free_page(&kctx->mem_pools.small[phys_alloc->group_id], page);
		kfree(scratch_buf);

		return ret;
	}

	zs_free(kctx->csf.zs_pool, zs_handle);

	phys_alloc->pages[index] = as_tagged(page_to_phys(page));

	dma_sync_single_for_device(kbdev->dev, kbase_dma_addr(page), PAGE_SIZE, DMA_BIDIRECTIONAL);
	phys_alloc->compressed_nents -= 1;
	atomic_sub(1, &kctx->csf.compressed_pages_cnt);
	atomic_sub(1, &kctx->kbdev->memdev.compressed_pages_total);
	kfree(scratch_buf);

	return 0;
}

int kbase_zs_decompress_region(struct kbase_context *kctx, struct kbase_va_region *reg,
			       bool skip_flush, bool skip_account)
{
	struct kbase_mem_phy_alloc *phys_alloc = reg->gpu_alloc;
	struct tagged_addr *pages = phys_alloc->pages;
	size_t new_compressed_nents, old_compressed_nents = phys_alloc->compressed_nents;
	unsigned long old_compressed_mem_size;
	ktime_t ts_start;
	size_t i;
	int ret;

	if (!IS_ENABLED(CONFIG_ZSMALLOC))
		return 0;

	lockdep_assert_held(&kctx->reg_lock);

	if (!skip_account) {
		ts_start = ktime_get_raw();
		old_compressed_mem_size = zs_get_total_pages(kctx->csf.zs_pool);
	}

	for (i = 0; i < phys_alloc->nents; i++) {
		if (is_compressed_small(pages[i])) {
			ret = decompress_small_page(kctx, reg, i);
			if (ret)
				break;
		} else if (is_compressed_large_head(pages[i])) {
			ret = decompress_large_page(kctx, reg, i);
			if (ret)
				break;
			i += (NUM_PAGES_IN_2MB_LARGE_PAGE - 1);
		}
	}

	ret = kbase_mmu_insert_on_decompress_region(kctx, reg, i, skip_flush);
	if (ret < 0) {
		dev_err(kctx->kbdev->dev, "kctx %d_%d decompression insert page table failed", kctx->tgid, kctx->id);
	}

	if (!skip_account) {
		new_compressed_nents = phys_alloc->compressed_nents;
		if (old_compressed_nents > new_compressed_nents) {
			unsigned long compressed_mem_dec =
				old_compressed_mem_size - zs_get_total_pages(kctx->csf.zs_pool);

			mem_account_inc(kctx, old_compressed_nents - new_compressed_nents,
					compressed_mem_dec);
			trace_graphics_mem_swapin(
				kctx->pid, old_compressed_nents - new_compressed_nents,
				ktime_to_us(ktime_sub(ktime_get_raw(), ts_start)), skip_account);
		}
	}

	return ret;
}

int kbase_zs_decompress(struct kbase_context *kctx)
{
	enum kbase_memory_zone idx;
	u32 old_compressed_pages_cnt, new_compressed_pages_cnt;
	unsigned long old_compressed_mem_size;
	ktime_t ts_start;
	int ret = 0;

	if (!IS_ENABLED(CONFIG_ZSMALLOC) || !kctx->csf.zs_pool)
		return 0;

	ts_start = ktime_get_raw();

	cancel_work_sync(&kctx->csf.compress_work);

	kbase_gpu_vm_lock_with_pmode_sync(kctx);
	old_compressed_pages_cnt = atomic_read(&kctx->csf.compressed_pages_cnt);
	old_compressed_mem_size = zs_get_total_pages(kctx->csf.zs_pool);

	for (idx = 0; idx < CONTEXT_ZONE_MAX; idx++) {
		struct kbase_reg_zone *zone = &kctx->reg_zone[idx];
		struct rb_root *rbtree = &zone->reg_rbtree;
		struct rb_node *p;

		for (p = rb_first(rbtree); p && atomic_read(&kctx->csf.compressed_pages_cnt);
		     p = rb_next(p)) {
			struct kbase_va_region *reg = rb_entry(p, struct kbase_va_region, rblink);

			if (!reg->gpu_alloc)
				continue;

			/* Skip if there are no pages to decompress */
			if (!reg->gpu_alloc->compressed_nents)
				continue;

			/* Skip decompressing a region that was made evictable (post compression) */
			if (reg->flags & KBASE_REG_DONT_NEED)
				continue;

			ret = kbase_zs_decompress_region(kctx, reg, false, true);
			if (ret)
				goto out;
		}
	}

out:
	new_compressed_pages_cnt = atomic_read(&kctx->csf.compressed_pages_cnt);
	WARN_ON_ONCE(new_compressed_pages_cnt > old_compressed_pages_cnt);
	if (new_compressed_pages_cnt < old_compressed_pages_cnt) {
		unsigned long compressed_mem_dec =
			old_compressed_mem_size - zs_get_total_pages(kctx->csf.zs_pool);

		mem_account_inc(kctx, old_compressed_pages_cnt - new_compressed_pages_cnt,
				compressed_mem_dec);
		trace_graphics_mem_swapin(kctx->tgid,
					  old_compressed_pages_cnt - new_compressed_pages_cnt,
					  ktime_to_us(ktime_sub(ktime_get_raw(), ts_start)), 1);
	}
	kbase_gpu_vm_unlock_with_pmode_sync(kctx);

	return ret;
}

int kbase_zs_compress(struct kbase_context *kctx)
{
	struct kbase_device *kbdev = kctx->kbdev;
	enum kbase_memory_zone idx;
	u32 old_compressed_pages_cnt, new_compressed_pages_cnt;
	unsigned long old_compressed_mem_size;
	u32 pages_freed = 0;
	ktime_t ts_start;
	int ret;

	if (!IS_ENABLED(CONFIG_ZSMALLOC) || !kctx->csf.zs_pool)
		return 0;

	ts_start = ktime_get_raw();

	ret = kbase_csf_scheduler_ctx_compression_begin(kbdev, kctx);
	if (ret)
		return ret;

	kbase_gpu_vm_lock_with_pmode_sync(kctx);
	old_compressed_pages_cnt = atomic_read(&kctx->csf.compressed_pages_cnt);
	old_compressed_mem_size = zs_get_total_pages(kctx->csf.zs_pool);

	for (idx = 0; idx < CONTEXT_ZONE_MAX; idx++) {
		struct kbase_reg_zone *zone = &kctx->reg_zone[idx];
		struct rb_root *rbtree = &zone->reg_rbtree;
		struct rb_node *p;

		for (p = rb_first(rbtree); p; p = rb_next(p)) {
			struct kbase_va_region *reg = rb_entry(p, struct kbase_va_region, rblink);

			if (!reg->gpu_alloc || !reg->gpu_alloc->nents)
				continue;

			/* Ignore region which has been compressed */
			if (reg->gpu_alloc->compressed_nents > 0)
				continue;

			/* Ignore imported and aliased region */
			if (reg->gpu_alloc->type != KBASE_MEM_TYPE_NATIVE)
				continue;

			/* Ignore region marked as evictable */
			if (reg->flags & KBASE_REG_DONT_NEED)
				continue;

			/* Ignore region having a kernel mapping */
			if (atomic_read(&reg->cpu_alloc->kernel_mappings) > 0)
				continue;

			/* Ignore region having multiple GPU mappings */
			if (atomic_read(&reg->gpu_alloc->gpu_mappings) > 1)
				continue;

			if (compress_region(kctx, reg, &pages_freed))
				goto out;
		}
	}

out:
	new_compressed_pages_cnt = atomic_read(&kctx->csf.compressed_pages_cnt);
	WARN_ON_ONCE(new_compressed_pages_cnt < old_compressed_pages_cnt);
	if (new_compressed_pages_cnt > old_compressed_pages_cnt) {
		trace_graphics_mem_swapout(kctx->tgid, kctx->csf.max_compressed_pages_cnt, ret,
					   new_compressed_pages_cnt - old_compressed_pages_cnt,
					   ktime_to_us(ktime_sub(ktime_get_raw(), ts_start)));
	}
	if (pages_freed) {
		unsigned long compressed_mem_inc =
			zs_get_total_pages(kctx->csf.zs_pool) - old_compressed_mem_size;

		mem_account_dec(kctx, pages_freed, compressed_mem_inc);
	}

	kbase_gpu_vm_unlock_with_pmode_sync(kctx);
	kbase_csf_scheduler_ctx_compression_end(kbdev, kctx);

	return 0;
}

static void kbase_zs_compress_worker(struct work_struct *data)
{
	struct kbase_context *kctx = container_of(data, struct kbase_context, csf.compress_work);

	kbase_zs_compress(kctx);
}

void kbase_zs_free_compressed_page(struct kbase_context *kctx, struct tagged_addr *pages)
{
	unsigned long zs_handle;
	unsigned int i;

	if (!IS_ENABLED(CONFIG_ZSMALLOC) || !kctx->csf.zs_pool)
		return;

	if (WARN_ON(!is_compressed(*pages)))
		return;

	if (is_compressed_small(*pages)) {
		zs_handle = (unsigned long)as_phys_addr_t(*pages);
		zs_free(kctx->csf.zs_pool, zs_handle);
		*pages = as_tagged(KBASE_INVALID_PHYSICAL_ADDRESS);
		atomic_sub(1, &kctx->csf.compressed_pages_cnt);
		atomic_sub(1, &kctx->kbdev->memdev.compressed_pages_total);
		return;
	}

	if (WARN_ON(!is_compressed_large_head(*pages)))
		return;

	for (i = 0; i < NUM_PAGES_IN_2MB_LARGE_PAGE; i++) {
		zs_handle = (unsigned long)as_phys_addr_t(pages[i]);
		if (zs_handle != KBASE_INVALID_PHYSICAL_ADDRESS)
			zs_free(kctx->csf.zs_pool, zs_handle);
		pages[i] = as_tagged(KBASE_INVALID_PHYSICAL_ADDRESS);
	}
	atomic_sub(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->csf.compressed_pages_cnt);
	atomic_sub(NUM_PAGES_IN_2MB_LARGE_PAGE, &kctx->kbdev->memdev.compressed_pages_total);
}

#define KBASE_CSF_MEM_COMPR_SYSFS_ATTR(_name, _mode)                \
	struct attribute kbase_csf_mem_compr_sysfs_attr_##_name = { \
		.name = __stringify(_name),                         \
		.mode = VERIFY_OCTAL_PERMISSIONS(_mode),            \
	}

static KBASE_CSF_MEM_COMPR_SYSFS_ATTR(proc_state, 0664);
static KBASE_CSF_MEM_COMPR_SYSFS_ATTR(max_reclaimable_mem_limit, 0664);
static KBASE_CSF_MEM_COMPR_SYSFS_ATTR(reclaimed_mem_count, 0444);
static KBASE_CSF_MEM_COMPR_SYSFS_ATTR(reclaimable_memory_count, 0444);

static void kbase_csf_mem_compr_sysfs_release(struct kobject *kobj)
{
}

static void per_process_parameter_calculate(struct kbase_process *kprcs, u32 *compressed_pages_cnt, u32 *used_pages)
{
	bool still_in_device_list;
	struct kbase_context *check_kctx, *kctx;
	struct kbase_device *kbdev = kprcs->kbdev;

	/* calculate the value of the process */
	mutex_lock(&kbdev->kctx_list_lock);
	list_for_each_entry(kctx, &kprcs->kctx_list, kprcs_link) {
		WARN_ON(kctx->tgid != kprcs->tgid);

		/* check whether the kctx has been removed from the device list */
                still_in_device_list = false;
                list_for_each_entry(check_kctx, &kbdev->kctx_list, kctx_list_link) {
                        if (check_kctx == kctx) {
                                still_in_device_list = true;
                                break;
                        }
                }

                /* kctx already deleted from device list can not process */
                if (!still_in_device_list)
                        continue;

		*compressed_pages_cnt += atomic_read(&kctx->csf.compressed_pages_cnt);
		*used_pages += atomic_read(&kctx->used_pages);
	}
	mutex_unlock(&kbdev->kctx_list_lock);
}

static ssize_t kbase_csf_mem_compr_sysfs_show(struct kobject *kobj, struct attribute *attr,
					      char *buf)
{
	struct kbase_process *kprcs = container_of(kobj, struct kbase_process, kobj);
	struct kbase_context *kctx;
	u32 compressed_pages_cnt_perprocess = 0;
	u32 used_pages_perprocess = 0;
	WARN_ON(!kprcs);

	kctx = (!list_empty(&kprcs->kctx_list)) ?
		list_first_entry(&kprcs->kctx_list, struct kbase_context, kprcs_link) :
		NULL;
	if (!kctx)
		return -ENODEV;

	if (attr == &kbase_csf_mem_compr_sysfs_attr_proc_state)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
				 (kctx->csf.foreground) ? "foreground" : "background");

	if (attr == &kbase_csf_mem_compr_sysfs_attr_max_reclaimable_mem_limit)
		return scnprintf(buf, PAGE_SIZE, "%i\n", kctx->csf.max_compressed_pages_cnt);

	if (attr == &kbase_csf_mem_compr_sysfs_attr_reclaimed_mem_count) {
		per_process_parameter_calculate(kprcs, &compressed_pages_cnt_perprocess, &used_pages_perprocess);
		return scnprintf(buf, PAGE_SIZE, "%u\n", compressed_pages_cnt_perprocess);
	}

	if (attr == &kbase_csf_mem_compr_sysfs_attr_reclaimable_memory_count) {
		per_process_parameter_calculate(kprcs, &compressed_pages_cnt_perprocess, &used_pages_perprocess);
		return scnprintf(buf, PAGE_SIZE, "%u\n", used_pages_perprocess);
	}

	dev_err(kctx->kbdev->dev, "Unexpected read from entry %d/%s", kctx->tgid, attr->name);

	return -EINVAL;
}

static ssize_t kbase_csf_mem_compr_sysfs_store(struct kobject *kobj, struct attribute *attr,
					       const char *buf, size_t count)
{
	struct kbase_process *kprcs = container_of(kobj, struct kbase_process, kobj);
	struct kbase_context *kctx;
	struct kbase_context *check_kctx;
	struct kbase_device *kbdev;
	bool still_in_device_list;
	int ret = 0;

	WARN_ON(!kprcs);
	kbdev = kprcs->kbdev;

	//dev_err(kbdev->dev, "[Begin]kbase_csf_mem_compr_sysfs_store: %d, called %d times", kprcs->tgid, kprcs->store_count++);

try_lock:
	if (mutex_trylock(&kbdev->kctx_list_lock)) {
		list_for_each_entry(kctx, &kprcs->kctx_list, kprcs_link) {
			WARN_ON(kctx->tgid != kprcs->tgid);

			/* check whether the kctx has been removed from the device list */
			still_in_device_list = false;
			list_for_each_entry(check_kctx, &kbdev->kctx_list, kctx_list_link) {
				if (check_kctx == kctx) {
					still_in_device_list = true;
					break;
				}
			}

			/* kctx already deleted from device list can not process */
			if (!still_in_device_list){
				continue;
			}

			if (attr == &kbase_csf_mem_compr_sysfs_attr_proc_state) {
				if (sysfs_streq(buf, "foreground")) {
					kctx->csf.foreground = true;

					ret = kbase_zs_decompress(kctx);
					if (ret)
						goto out;
				} else if (sysfs_streq(buf, "background")) {
					kctx->csf.foreground = false;

					queue_work(kctx->csf.wq, &kctx->csf.compress_work);
				} else {
					dev_err(kctx->kbdev->dev, "Couldn't process %d_%d/%s write operation",
							kctx->tgid, kctx->id, attr->name);
					ret = -EINVAL;
					goto out;
				}

				ret = (ssize_t)count;
				continue;
			}

			if (attr == &kbase_csf_mem_compr_sysfs_attr_max_reclaimable_mem_limit) {
				s32 val;
				ret = kstrtoint(buf, 0, &val);

				if (ret) {
					dev_err(kctx->kbdev->dev, "Couldn't process %d_%d/%s write operation",
							kctx->tgid, kctx->id, attr->name);
					ret = -EINVAL;
					goto out;
				}

				kctx->csf.max_compressed_pages_cnt = val;

				ret = (ssize_t)count;
				continue;
			}

			dev_err(kctx->kbdev->dev, "Unexpected write to entry %d_%d/%s", kctx->tgid, kctx->id, attr->name);

			ret = -EINVAL;
			goto out;
		}

	} else {
		/* Get the lock failed check whether kprcs already in
		 * term processing, if yes return directly
                 * or a deadlock will generated.
		 * if no try to lock again
                 */
		if (atomic_read(&kprcs->term_processing) > 0) {
			if (list_empty(&kprcs->kctx_list))
				return ret;
		} else {
			//dev_err(kbdev->dev, "[Retry]kbase_csf_mem_compr_sysfs_store: %d\n", kprcs->tgid);
			goto try_lock;
		}
	}
out:
	mutex_unlock(&kbdev->kctx_list_lock);
	return ret;
}

static const struct sysfs_ops kbase_csf_mem_compr_sysfs_ops = {
	.show = &kbase_csf_mem_compr_sysfs_show,
	.store = &kbase_csf_mem_compr_sysfs_store,
};

static struct attribute *kbase_csf_mem_compr_sysfs_attrs[] = {
	&kbase_csf_mem_compr_sysfs_attr_proc_state,
	&kbase_csf_mem_compr_sysfs_attr_max_reclaimable_mem_limit,
	&kbase_csf_mem_compr_sysfs_attr_reclaimed_mem_count,
	&kbase_csf_mem_compr_sysfs_attr_reclaimable_memory_count,
	NULL,
};

#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
ATTRIBUTE_GROUPS(kbase_csf_mem_compr_sysfs);
#endif

static struct kobj_type kbase_csf_mem_compr_sysfs_kobj_type = {
	.release = &kbase_csf_mem_compr_sysfs_release,
	.sysfs_ops = &kbase_csf_mem_compr_sysfs_ops,
#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
	.default_groups = kbase_csf_mem_compr_sysfs_groups,
#else
	.default_attrs = kbase_csf_mem_compr_sysfs_attrs,
#endif
};

int kbase_csf_mem_compr_init(struct kbase_context *kctx)
{
	char kctx_zpool_name[64];

	if (!IS_ENABLED(CONFIG_ZSMALLOC) || !IS_ENABLED(CONFIG_SYSFS))
		return 0;

	if (kbase_is_page_migration_enabled()) {
		dev_info_once(kctx->kbdev->dev,
			      "GPU memory compression not supported with Page migration enabled");
		return 0;
	}

	if (!kbase_zs_backend_get(kctx->kbdev->dev))
		return -EINVAL;

	scnprintf(kctx_zpool_name, ARRAY_SIZE(kctx_zpool_name), "kctx_%d_%d_zpool", kctx->tgid,
		  kctx->id);

	kctx->csf.zs_pool = zs_create_pool(kctx_zpool_name);
	if (!IS_ENABLED(CONFIG_ZSMALLOC) || !kctx->csf.zs_pool) {
		dev_err(kctx->kbdev->dev, "Failed to create zs pool");
		return -ENOMEM;
	}

	INIT_WORK(&kctx->csf.compress_work, kbase_zs_compress_worker);
	return 0;
}

void kbase_csf_mem_compr_term(struct kbase_context *kctx)
{
	if (!IS_ENABLED(CONFIG_ZSMALLOC) || !IS_ENABLED(CONFIG_SYSFS))
		return;

	WARN_ON(atomic_read(&kctx->csf.compressed_pages_cnt));

	if (kctx->csf.zs_pool) {
		zs_destroy_pool(kctx->csf.zs_pool);
		kctx->csf.zs_pool = NULL;
	}
}

void kbase_csf_mem_compr_kobj_init(struct kbase_context *kctx)
{
	int err = 0;

	/* create kobj per process */
        err = kobject_init_and_add(&kctx->kprcs->kobj, &kbase_csf_mem_compr_sysfs_kobj_type,
                        kctx->kbdev->mcompr_kobj, "%d", kctx->tgid);
        if (err) {
                kobject_put(&kctx->kprcs->kobj);

                dev_err(kctx->kbdev->dev, "Creation of ctx/%d sysfs sub-directory failed %d",
                                kctx->tgid, err);
        }
}

int kbase_csf_mem_compr_sysfs_init(struct kbase_context *kctx)
{
	if (!kctx->csf.zs_pool)
		return 0;

	/* Set unlimited number of pages for compression by default */
	kctx->csf.max_compressed_pages_cnt = MAX_RECLAIMABLE_MEM_LIMIT_UNLIMITED;

	return 0;
}

void kbase_csf_mem_compr_sysfs_term(struct kbase_context *kctx)
{
	if (!kctx->csf.zs_pool)
		return;

	cancel_work_sync(&kctx->csf.compress_work);
}
