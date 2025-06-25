// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include "malloc.h"
#include <asm/page-def.h>
#include "hyp_spinlock.h"
#include "smmu_mgmt.h"

#define MALLOC_ALIGN 64

struct mem_control_block {
	int is_occupied;
	int block_size;
};

static struct pkvm_module_ops *module_ops;
static int is_malloc_initialized;
static void *heap_hyp_start;
static void *heap_hyp_end;
static DEFINE_HYP_SPINLOCK(heap_lock);

static int sbrk(size_t nr_bytes)
{
	int ret = 0;

	if (!nr_bytes) {
		module_ops->puts("sbrk: nr_bytes is zero");
		return -1;
	}

	heap_hyp_end = (void *)((u64)heap_hyp_start + nr_bytes);
	return ret;
}

int malloc_init(struct pkvm_module_ops *ops)
{
	int ret = 0;

	module_ops = ops;
	heap_hyp_start = smmu_mpool_alloc(PAGE_SIZE);

	if (!heap_hyp_start) {
		module_ops->puts("malloc_init : mpool alloc failed");
		ret = -1;
		goto failed;
	} else
		ret = sbrk((size_t)PAGE_SIZE);

	if (ret) {
		module_ops->puts("malloc_init failed");
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

	if (!is_malloc_initialized) {
		module_ops->puts("malloc uninitialized");
		return (void *)NULL;
	}

	if (!nr_bytes) {
		module_ops->puts("malloc: nr_bytes is zero");
		return (void *)NULL;
	}

	block_size =
		((nr_bytes + sizeof(struct mem_control_block)) / MALLOC_ALIGN);

	if (((nr_bytes + sizeof(struct mem_control_block)) % MALLOC_ALIGN))
		block_size++;
	block_size *= MALLOC_ALIGN;
	curr_ptr = heap_hyp_start;
	hyp_spin_lock(&heap_lock);
	while (curr_ptr != heap_hyp_end) {
		curr_memcb_ptr = (struct mem_control_block *)curr_ptr;

		/* represents this block is available */
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

	if (curr_ptr == heap_hyp_end) {
		module_ops->puts("malloc memory run out");
		return NULL;
	}

	target_ptr = target_ptr + sizeof(struct mem_control_block);
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
