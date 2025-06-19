/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __SHAREBUF_H
#define __SHAREBUF_H

#include <asm/kvm_pkvm_module.h>
#include "lib/spinlock.h"
#include "handle.h"

// TODO: add comment here
#define MAX_SHAREBUF_ENTRIES	(0x80000UL)
#define MAX_SHAREBUF_ENTRY_SIZE	(0x400000UL)

struct sharebuf_object {
	struct handle_object *handle_obj;
	u32 nr_entries;
	u32 content_size;
	int freelist;
	hyp_spinlock_t handle_obj_lock;
	struct sharebuf_object *next;
};

/* Sharebuf Reference Object Type */
struct sharebuf_ref_object {
	u64 tag;
	u32 handle;
	u32 sharebuf_index;
	struct sharebuf_ref_object *hlist;
};

/* Hash table for sharebuf */
typedef struct sharebuf_hash_bucket {
	struct sharebuf_ref_object *hList;
	hyp_spinlock_t hLock;
} sharebuf_hash_bucket_t;

/* Sharebuf update type */
typedef struct sharebuf_update {
	int value;
	u64 index;
} sharebuf_update_t;

void init_sharebuf_manipulation(void);

#endif
