// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include "sharebuf.h"
#include "handle.h"
#include "policy.h"
#include "hvcfunc.h"
#include "mkp_err.h"
#include "mapping_attr.h"
#include "lib/malloc.h"

typedef u64 tag_t;

/* Global list for sharebuf */
static hyp_spinlock_t sharebuf_glist_lock __cacheline_aligned;
static struct sharebuf_object *sharebuf_glist;

/* Hash table for sharebuf */
#define HASH_TABLE_SIZE		(256)
#define HASH_TABLE_ENTRY(x)	(x % HASH_TABLE_SIZE)
static sharebuf_hash_bucket_t sharebuf_htab[HASH_TABLE_SIZE];

void init_sharebuf_manipulation(void)
{
	int i;
	sharebuf_hash_bucket_t *bucket;
	hyp_spinlock_t *lock;

	/* Initialize sharebuf_glist_lock explicitly  */
	hyp_spin_lock_init(&sharebuf_glist_lock);

	/* Initialize bucket explicitly */
	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		bucket = &sharebuf_htab[i];
		bucket->hList = NULL;
		lock = &bucket->hLock;
		hyp_spin_lock_init(lock);
	}
}

static inline int check_args(u64 *args, u32 nr_args)
{
	u32 i;
	int ret = 0;

	for (i = 0; i < nr_args; i++) {
		if (args[i]) {
			ret = 1;
			break;
		}
	}

	return ret;
}

static inline void store_n_args(u64 *content_ptr, u64 *args, u32 nr_args)
{
	u32 i;

	for (i = 0; i < nr_args; i++)
		*(content_ptr + i) = args[i];
}

/* Place sharebuf obj in LRU order */
static inline void __add_to_sharebuf_htab(struct sharebuf_ref_object *sharebuf_ref_obj)
{
	u32 bucket = HASH_TABLE_ENTRY(sharebuf_ref_obj->tag);
	hyp_spinlock_t *lock = &sharebuf_htab[bucket].hLock;

	hyp_spin_lock(lock);
	sharebuf_ref_obj->hlist = sharebuf_htab[bucket].hList;
	sharebuf_htab[bucket].hList = sharebuf_ref_obj;
	hyp_spin_unlock(lock);
}

static void setup_sharebuf_ref_obj(struct sharebuf_ref_object *sharebuf_ref_obj,
					u32 handle, u64 tag_value, u32 sharebuf_index)
{
	// setup the sharebuf_ref_object
	sharebuf_ref_obj->handle = handle;
	sharebuf_ref_obj->tag = tag_value;
	sharebuf_ref_obj->sharebuf_index = sharebuf_index;

	// insert the sharebuf_ref_object into sharebuf_htab
	__add_to_sharebuf_htab(sharebuf_ref_obj);
}

static void insert_sharebuf_list(struct sharebuf_object *sharebuf_obj)
{
	hyp_spin_lock(&sharebuf_glist_lock);
	sharebuf_obj->next = sharebuf_glist;
	sharebuf_glist = sharebuf_obj;
	hyp_spin_unlock(&sharebuf_glist_lock);
}

static struct sharebuf_object *pick_sharebuf_object(u32 handle)
{
	struct sharebuf_object *sb_obj_ptr = sharebuf_glist;
	u32 real_handle = 0;

	real_handle = scramble_handle(handle);
	hyp_spin_lock(&sharebuf_glist_lock);
	while (sb_obj_ptr) {
		if (sb_obj_ptr->handle_obj == NULL) {
			/* abnormal case */
			/* dump message */
			// trace_hyp_printk("[MKP] %s failed, handle_obj is NULL", __func__);
			sb_obj_ptr = NULL;
			break;
		}

		if (sb_obj_ptr->handle_obj->handle == real_handle)
			break;

		sb_obj_ptr = sb_obj_ptr->next;
	}
	hyp_spin_unlock(&sharebuf_glist_lock);

	return sb_obj_ptr;
}

static struct sharebuf_object *create_sharebuf_object(void)
{
	return malloc(sizeof(struct sharebuf_object));
}

static struct sharebuf_ref_object *create_sharebuf_ref_object(void)
{
	return malloc(sizeof(struct sharebuf_ref_object));
}

static void destroy_sharebuf_ref_object(struct sharebuf_ref_object *obj)
{
	/* Sanity check */
	if (obj == NULL)
		return;

	free(obj);
}

int mkp_configure_sharebuf(u32 policy, u32 handle, u32 type, u64 num, u64 size)
{
	struct handle_object *handle_obj;
	struct sharebuf_object *sharebuf_obj;
	u64 sb_size = 0;
	u32 i;
	int ret = 0, err_line = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		// trace_hyp_printk("[MKP] mkp_configure_sharebuf failed, handle_obj is NULL");
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, /* Nothing to check */0, false);
	if (ret != 0) {
		err_line = __LINE__;
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* sanity check (TBD) */
	if (!handle_obj->start) {
		err_line = __LINE__;
		ret = ERR_INVALID_IPA;
		goto out;
	}

	if (type) {	// ONLY accept 0 currently
		err_line = __LINE__;
		ret = ERR_INVALID_PARAM;
		goto out;
	}

	/* sanoty check on num */
	if (num == 0 || num > MAX_SHAREBUF_ENTRIES) {
		err_line = __LINE__;
		ret = ERR_INVALID_PARAM;
		goto out;
	}

	/* sanity check on size */
	if (size == 0 || size > MAX_SHAREBUF_ENTRY_SIZE) {
		err_line = __LINE__;
		ret = ERR_INVALID_PARAM;
		goto out;
	}

	// identify the type of shared buffer and calculate the size of sharebuf
	if (check_sb_entry_ordered(policy)) {
		sb_size = num * round_up(size, sizeof(u64));
	} else if (check_sb_entry_disordered(policy)) {
		sb_size = num * (round_up(size, sizeof(u64)) + sizeof(tag_t));
	} else {
		err_line = __LINE__;
		ret = ERR_INVALID_CHAR;
		goto out;
	}

	if (sb_size > handle_obj->size) {
		err_line = __LINE__;
		ret = ERR_NO_AVAIL_SPACE;
		goto out;
	}

	if (to_apply_sb_set(handle_obj->attrset)) {

		// setup handle object
		handle_obj->attrset = sb_set_is_applied(handle_obj->attrset);

		if (check_sb_entry_disordered(policy)) {
			char *sharebuf_ptr = 0;
			tag_t *entry_ptr = 0, *fixmap_ptr = 0;

			// initialize sharebuf entries
			sharebuf_ptr = (char *)handle_obj->start;
			for (i = 0; i < num - 1; i++) {
				entry_ptr = (tag_t *)(sharebuf_ptr + i * (sizeof(tag_t) +
						round_up(size, sizeof(uint64_t))));
				fixmap_ptr = (tag_t *)(module_ops->fixmap_map((u64)entry_ptr));
				*fixmap_ptr = i + 1;
				module_ops->fixmap_unmap();
			}

			entry_ptr = (tag_t *)(sharebuf_ptr + i * (sizeof(tag_t) + round_up(size, sizeof(uint64_t))));
			fixmap_ptr = (tag_t *)(module_ops->fixmap_map((u64)entry_ptr));
			*fixmap_ptr = -1;
			module_ops->fixmap_unmap();
		}

		// setup sharebuf object
		sharebuf_obj = create_sharebuf_object();
		if (sharebuf_obj) {
			sharebuf_obj->nr_entries = num;
			sharebuf_obj->content_size = size;
			sharebuf_obj->freelist = 0;
			sharebuf_obj->handle_obj = handle_obj;
			hyp_spin_lock_init(&sharebuf_obj->handle_obj_lock);
			insert_sharebuf_list(sharebuf_obj);
		} else {
			err_line = __LINE__;
			ret = ERR_NO_AVAIL_SPACE;
		}
	} else {
		err_line = __LINE__;
		ret = ERR_INVALID_SHAREBUF;
	}
out:
	if (err_line) {
		put_back_handle(handle_obj);
		// trace_hyp_printk("[MKP] mkp_configure_sharebuf, err_line: %d", __LINE__);
	}

	return ret;
}

/* Try to lookup and take out the sharebuf ref object */
static struct sharebuf_ref_object *__lookup_sharebuf_ref_object(u32 handle, u64 index)
{
	u32 bucket = HASH_TABLE_ENTRY(index);
	struct sharebuf_ref_object **pprev = &sharebuf_htab[bucket].hList;
	hyp_spinlock_t *lock = &sharebuf_htab[bucket].hLock;
	struct sharebuf_ref_object *obj = NULL;

	hyp_spin_lock(lock);
	while(*pprev != NULL) {
		if ((*pprev)->tag == index && (*pprev)->handle == handle) {
			obj = *pprev;
			*pprev = obj->hlist;
			obj->hlist = NULL;
			break;
		}
		pprev = &(*pprev)->hlist;
	}
	hyp_spin_unlock(lock);

	return obj;
}

static sharebuf_update_t __update_disordered_sharebuf_n_args
(struct sharebuf_object *sharebuf_obj, u64 index, u64 *args, u64 nr_args)
{
	struct handle_object *handle_obj = sharebuf_obj->handle_obj;
	u32 handle = handle_obj->handle;
	struct sharebuf_ref_object *sharebuf_ref_obj = NULL;
	char *sharebuf_ptr = (char *)handle_obj->start;
	u64 *content_ptr = 0;
	char update_flag = 0;	// 0: No update, 1: Updated
	u64 store_flag = 0;	// 0: give back the entry, 1: store content
	tag_t *entry_ptr = 0;
	u64 *fixmap_content_ptr, *fixmap_entry_ptr;
	tag_t tag;
	u32 i;
	sharebuf_update_t ret = {0, -1};

	sharebuf_ref_obj = __lookup_sharebuf_ref_object(handle, index);

	if (sharebuf_ref_obj) {
		i = sharebuf_ref_obj->sharebuf_index;
		entry_ptr = (tag_t *)(sharebuf_ptr + i * (sizeof(tag_t) + nr_args * sizeof(u64)));
		content_ptr = (u64 *)((char *)entry_ptr + sizeof(tag_t));
		store_flag = check_args(args, (u32)nr_args);	// check if args are zero
		update_flag = 1;
		ret.index = 1;

		if (store_flag) {
			// write values into the sharebuf entry
			fixmap_content_ptr = (u64 *)(module_ops->fixmap_map((u64)content_ptr));
			store_n_args(fixmap_content_ptr, args, (u32)nr_args);
			module_ops->fixmap_unmap();

			// No further access, just putback
			__add_to_sharebuf_htab(sharebuf_ref_obj);
		} else {
			// remove the sharebuf_ref_object
			destroy_sharebuf_ref_object(sharebuf_ref_obj);

			// update freelist
			fixmap_entry_ptr = (u64 *)(module_ops->fixmap_map((u64)entry_ptr));
			*fixmap_entry_ptr = (tag_t)sharebuf_obj->freelist;
			sharebuf_obj->freelist = i;
			module_ops->fixmap_unmap();
		}
	}

	if (!update_flag) {
		if (sharebuf_obj->freelist == -1) {
			ret.value = ERR_NO_AVAIL_SPACE;
			//trace_hyp_printk("[MKP] __update_disordered_sharebuf_n_args:\
			//			No available sharebuf free entry");
		} else {
			// take ith sharebuf entry
			i = (u32)sharebuf_obj->freelist;
			entry_ptr = (tag_t *)(sharebuf_ptr + i * (sizeof(tag_t) + nr_args * sizeof(u64)));
			content_ptr = (u64 *)((char *)entry_ptr + sizeof(tag_t));
			fixmap_entry_ptr = (u64 *)(module_ops->fixmap_map((u64)entry_ptr));
			fixmap_content_ptr = (u64 *)(module_ops->fixmap_map((u64)content_ptr));
			tag = *fixmap_entry_ptr;
			store_flag = check_args(args, (u32)nr_args);	// check if args are zero
			ret.index = i;

			if (store_flag) {
				sharebuf_ref_obj = create_sharebuf_ref_object();

				if (sharebuf_ref_obj) {
					store_n_args(fixmap_content_ptr, args, (u32)nr_args);
					*fixmap_entry_ptr = index;
					setup_sharebuf_ref_obj(sharebuf_ref_obj, handle, *fixmap_entry_ptr, i);
					sharebuf_obj->freelist = tag;	// update freelist
				} else {
					ret.value = ERR_NO_AVAIL_SPACE;
					//trace_hyp_printk("[MKP] __update_disordered_sharebuf_n_args:\
					//			No enough space to create a sharebuf_ref_obj");
				}
			}

			module_ops->fixmap_unmap();
		}
	}

	return ret;
}

static sharebuf_update_t __update_sharebuf_n_args(u32 policy, u32 handle, u64 index, u64 *args, u64 nr_args)
{
	struct sharebuf_object *sharebuf_obj = NULL;
	struct handle_object *handle_obj;
	hyp_spinlock_t *lock;
	u64 *sharebuf_ptr = 0, *fixmap_ptr;
	u64 i;
	int ret = 0;
	sharebuf_update_t update_ret = {0, -1};

	/* Before operation, we need to pick the sharebuf_object */
	sharebuf_obj = pick_sharebuf_object(handle);

	if (!sharebuf_obj) {
		// err_line = __LINE__;
		update_ret.value = ERR_INVALID_HANDLE;
		goto out;
	}

	handle_obj = sharebuf_obj->handle_obj;
	lock = &sharebuf_obj->handle_obj_lock;

	/* Do validation */
	ret = validate_handle(handle_obj, policy, /* Nothing to check */0, false);
	if (ret) {
		// err_line = __LINE__;
		update_ret.value = ERR_INVALID_HANDLE;
		goto out;
	}

	/* sanity check (TBD) */
	if (!handle_obj->start) {
		// err_line = __LINE__;
		update_ret.value = ERR_INVALID_IPA;
		goto out;
	}

	if (round_up(sharebuf_obj->content_size, sizeof(args[0])) != sizeof(args[0]) * nr_args) {
		// err_line = __LINE__;
		update_ret.value = ERR_INVALID_CONTENT;
		goto out;
	}

	if (to_apply_sb_set(handle_obj->attrset)) {
		// err_line = __LINE__;
		update_ret.value = ERR_INVALID_SHAREBUF;
		goto out;
	}

	hyp_spin_lock(lock);
	sharebuf_ptr = (u64 *)handle_obj->start;

	// identify the type of shared buffer by policy
	if (check_sb_entry_ordered(policy)) {

		if (index < sharebuf_obj->nr_entries) {
			sharebuf_ptr = sharebuf_ptr + nr_args * index;

			for (i = 0; i < nr_args; i++) {
				fixmap_ptr = (u64 *)(module_ops->fixmap_map((u64)(sharebuf_ptr + i)));
				*fixmap_ptr = args[i];
			}

			module_ops->fixmap_unmap();
			update_ret.index = index;
		} else {
			// err_line = __LINE__;
			update_ret.value = ERR_INVALID_PARAM;
		}
	} else if (check_sb_entry_disordered(policy)) {
		update_ret = __update_disordered_sharebuf_n_args(sharebuf_obj, index, args, nr_args);
		if (update_ret.value) {
			// err_line = __LINE__;
			update_ret.value = ERR_INVALID_PARAM;
		}
	}

	hyp_spin_unlock(lock);

out:
	// if (err_line)
	//	trace_hyp_printk("[MKP] __update_sharebuf_n_args: err_line: %d", __LINE__);

	return update_ret;
}

sharebuf_update_t mkp_update_sharebuf_1_argu(u32 policy, u32 handle, u64 index, u64 u64_a1)
{
	u64 args[1] = {u64_a1};
	sharebuf_update_t ret;

	ret = __update_sharebuf_n_args(policy, handle, index, args, 1);

	return ret;
}

sharebuf_update_t mkp_update_sharebuf_2_argu(u32 policy, u32 handle, u64 index, u64 u64_a1, u64 u64_a2)
{
	u64 args[2] = {u64_a1, u64_a2};
	sharebuf_update_t ret;

	ret = __update_sharebuf_n_args(policy, handle, index, args, 2);

	return ret;
}

sharebuf_update_t mkp_update_sharebuf_3_argu(u32 policy, u32 handle, u64 index, u64 u64_a1, u64 u64_a2, u64 u64_a3)
{
	u64 args[3] = {u64_a1, u64_a2, u64_a3};
	sharebuf_update_t ret;

	ret = __update_sharebuf_n_args(policy, handle, index, args, 3);

	return ret;
}

sharebuf_update_t mkp_update_sharebuf_4_argu
(u32 policy, u32 handle, u64 index, u64 u64_a1, u64 u64_a2, u64 u64_a3, u64 u64_a4)
{
	u64 args[4] = {u64_a1, u64_a2, u64_a3, u64_a4};
	sharebuf_update_t ret;

	ret = __update_sharebuf_n_args(policy, handle, index, args, 4);

	return ret;
}
