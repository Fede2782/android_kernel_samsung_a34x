// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include "malloc.h"
#include <asm/page-def.h>
#include "stdlib.h"
#include "spinlock.h"

// TODO: supposed that MKP_HEAP_START is depends on mkp reserved memory location
#define MKP_HEAP_START          0x80000000
#define CFG_MKP_CHUNK_SIZE	0x20000
#define MALLOC_ALIGN		64

#ifdef memset
#undef memset
#endif

struct mem_control_block {
	int is_occupied;
	int block_size;
};

static struct pkvm_module_ops *module_ops;
static int is_malloc_initialized;
static u64 heap_phys_start;
static u64 heap_phys_end;
static void *heap_hyp_start;
static void *heap_hyp_end;
static u64 heap_size;

static DEFINE_HYP_SPINLOCK(heap_lock);

static int sbrk(size_t nr_bytes)
{
	u64 nr_pages, start_pfn;
	int ret;
	void *hyp_ptr = 0;

	if (!nr_bytes)
		return -1;

	// TODO: end boundary check

	// donate pages
	nr_pages = nr_bytes >> PAGE_SHIFT;
	start_pfn = heap_phys_end >> PAGE_SHIFT;
	ret = module_ops->host_donate_hyp(start_pfn, nr_pages, 0);

	if (ret)
		return -1;

	heap_phys_start = heap_phys_end;
	heap_phys_end = heap_phys_start + nr_bytes;
	heap_size += nr_bytes;
	hyp_ptr = (void *)(module_ops->hyp_va((u64)heap_phys_start));
	module_ops->memset(hyp_ptr, 0, nr_bytes);
	heap_hyp_start = hyp_ptr;
	heap_hyp_end = (void *)((u64)heap_hyp_start + nr_bytes);

	return ret;
}

int malloc_init(struct pkvm_module_ops *ops, u64 heap_start, u64 heap_size)
{
	int ret = 0;
	// initialize global variable
	module_ops = ops;
	heap_phys_start = heap_start;
	heap_phys_end = heap_start;
	ret = sbrk((size_t)CFG_MKP_CHUNK_SIZE);
	if (ret) {
		ret = -1;
		goto failed;
	}

	is_malloc_initialized = 1;

failed:
	return ret;
}

void *malloc(size_t nr_bytes)
{
	char *curr_ptr, *target_ptr = 0;
	struct mem_control_block *curr_memcb_ptr;
	u64 block_size = 0;
	int ret = 0;

	if (!is_malloc_initialized)
		return (void *)NULL;

	if (!nr_bytes)
		return (void *)NULL;

	// Maybe it is not necessary to round up...
	block_size = ROUNDUP(nr_bytes + sizeof(struct mem_control_block), MALLOC_ALIGN);
	curr_ptr = heap_hyp_start;

	hyp_spin_lock(&heap_lock);
	while(curr_ptr != heap_hyp_end) {
		curr_memcb_ptr = (struct mem_control_block *)curr_ptr;

		// represents this block is available
		if (!curr_memcb_ptr->is_occupied) {
			if (!curr_memcb_ptr->block_size ||
					(curr_memcb_ptr->block_size &&
					curr_memcb_ptr->block_size >= block_size)) {
				curr_memcb_ptr->is_occupied = 1;
				curr_memcb_ptr->block_size = block_size;
				target_ptr = curr_ptr;
				break;
			}
		}

		curr_ptr = curr_ptr + curr_memcb_ptr->block_size;
	}
	hyp_spin_unlock(&heap_lock);

	if (!target_ptr) {
		ret = sbrk((size_t)CFG_MKP_CHUNK_SIZE);
		if (ret) {
			target_ptr = (char *)NULL;
			goto failed;
		}

		hyp_spin_lock(&heap_lock);
		curr_memcb_ptr = (struct mem_control_block *)heap_hyp_start;
		curr_memcb_ptr->is_occupied = 1;
		curr_memcb_ptr->block_size = block_size;
		target_ptr = heap_hyp_start;
		hyp_spin_unlock(&heap_lock);
	}

	target_ptr = target_ptr + sizeof(struct mem_control_block);

failed:
	return (void *)target_ptr;
}

void free(void *hyp_ptr)
{
	struct mem_control_block *memcb_ptr;

	hyp_spin_lock(&heap_lock);
	memcb_ptr = hyp_ptr - sizeof(struct mem_control_block);
	memcb_ptr->is_occupied = 0;
	memcb_ptr->block_size = 0;
	hyp_spin_unlock(&heap_lock);
}
