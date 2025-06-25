// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "lib/spinlock.h"
#include "lib/malloc.h"
#include "handle.h"
#include "policy.h"
#include "mkp_err.h"

#ifdef memset
#undef memset
#endif

#define SMP_MAX_CPUS	8

/* The predicates checked here are taken from lib/list_debug.c. */
/*
bool __list_add_valid(struct list_head *new, struct list_head *prev,
		      struct list_head *next)
{
	if (NVHE_CHECK_DATA_CORRUPTION(next->prev != prev) ||
	    NVHE_CHECK_DATA_CORRUPTION(prev->next != next) ||
	    NVHE_CHECK_DATA_CORRUPTION(new == prev || new == next))
		return false;

	return true;
}
*/
/*
bool __list_del_entry_valid(struct list_head *entry)
{
	struct list_head *prev, *next;

	prev = entry->prev;
	next = entry->next;

	if (NVHE_CHECK_DATA_CORRUPTION(next == LIST_POISON1) ||
	    NVHE_CHECK_DATA_CORRUPTION(prev == LIST_POISON2) ||
	    NVHE_CHECK_DATA_CORRUPTION(prev->next != entry) ||
	    NVHE_CHECK_DATA_CORRUPTION(next->prev != entry))
		return false;

	return true;
}
*/
/* Global list for handles */
static hyp_spinlock_t handle_glist_lock __cacheline_aligned;
static struct list_head handle_glist;

/* Maintainable IPA range */
u64 HANDLE_IPA_START = 0x40000000;
u64 HANDLE_IPA_END;

/* Pool for handle */
static hyp_spinlock_t handle_lock __cacheline_aligned;
static u32 handle_start;
static u32 handle_exhausted;
static u32 handle_key;

/* For mapping compaction */
hyp_spinlock_t cb_lock __cacheline_aligned;
union compact_blk_2MB *cb_section;
u64 cbs_cnt;

/* Synchronize exception handling */
#ifdef MERGE_S2EL1_IN_LEGACY
rwlock_t trap_lock __cacheline_aligned;
int trap_entry[SMP_MAX_CPUS];
#endif
// bool support_s2_mapping_compaction = false;

/* For granting tickets */
static int grant_ticket[SMP_MAX_CPUS];
// static bool start_granting = false;
static u64 ticket_key;
static bool forbid_root_handle;

/* Hash table for handles */
#define HASH_TABLE_SIZE		(16)
#define HASH_BUCKET_ENTRY(x)	(x % HASH_TABLE_SIZE)
static hash_bucket_t handle_htab[HASH_TABLE_SIZE];

static unsigned char scramble_byte(unsigned char pk, unsigned char ci, unsigned char i)
{
	unsigned char xi = 0, yi = 0, zi = 0;

	xi = ci ^ pk;
	yi = (unsigned char)(254 - (254 & i)) | (unsigned char)(127 ^ i);
	zi = xi ^ yi;

	return zi;
}

u32 scramble_handle(u32 handle)
{
	static const u32 byte_mask = 0x000000ff;
	unsigned char i, k;
	unsigned char ci, pk, zi;
	u32 c_mask = 0, p_mask = 0, res = 0;

	for (i = 0, k = i + 1; i < sizeof(u32); i++, k++) {
		k = k % sizeof(u32);
		c_mask = byte_mask << (i * 8);
		p_mask = byte_mask << (k * 8);
		pk = (handle_key & p_mask) >> (k * 8);
		ci = (handle & c_mask) >> (i * 8);
		zi = scramble_byte(pk, ci, i);
		res = res | (zi << (i * 8));
	}

	return res;
}

/* Try to get policy info from IPA */
u32 query_policy_from_ipa(u64 ipa)
{
	struct handle_object *tmp;
	struct list_head *itr;
	u64 tend;
	u32 policy = MKP_POLICY_NR;

	hyp_spin_lock(&handle_glist_lock);

	list_for_each(itr, &handle_glist) {
		tmp = list_entry(itr, struct handle_object, list);
		if (ipa < tmp->start)
			continue;

		/* Is ipa in the range */
		tend = tmp->start + tmp->size;
		if (ipa < tend) {
			policy = tmp->policy;
			break;
		}
	}

	hyp_spin_unlock(&handle_glist_lock);

	return policy;
}

/*
 * (NOTE)
 * There will be some frequent accesses on some handles.
 * Implement the chaining of hash table entries as LRU will
 * make this situation work better.
 */

/* Place handle obj in LRU order */
static inline void __add_to_handle_htab(struct handle_object *obj)
{
	u32 bucket = HASH_BUCKET_ENTRY(obj->handle);
	hyp_spinlock_t *lock = &handle_htab[bucket].hLock;

	hyp_spin_lock(lock);
	obj->hList = handle_htab[bucket].hList;
	handle_htab[bucket].hList = obj;
	hyp_spin_unlock(lock);
}

static void destroy_handle_object(struct handle_object *obj)
{
	/* Sanity check */
	if (obj == NULL)
		return;

	free(obj);
}

/*
 * Place handle obj.
 *
 * Expect being called by create/destroy_handle directly.
 */
static void add_to_handle_htab(struct handle_object *obj)
{
	__add_to_handle_htab(obj);
}

/* Remove handle obj */
static inline struct handle_object *__del_from_handle_htab(u32 handle)
{
	u32 bucket = HASH_BUCKET_ENTRY(handle);
	struct  handle_object **pprev = &handle_htab[bucket].hList;
	hyp_spinlock_t *lock = &handle_htab[bucket].hLock;
	struct handle_object *obj = NULL;

	hyp_spin_lock(lock);
	while (*pprev != NULL) {
		if ((*pprev)->handle == handle) {
			obj = *pprev;
			*pprev = obj->hList;
			obj->hList = NULL;
			break;
		}
		pprev = &(*pprev)->hList;
	}
	hyp_spin_unlock(lock);

	return obj;
}

/*
 * Remove handle obj.
 *
 * Expect being called by destroy_handle directly.
 */
static struct handle_object *del_from_handle_htab(u32 handle)
{
	u32 real_handle;

	real_handle = scramble_handle(handle);
	return __del_from_handle_htab(real_handle);
}

/*
 * Put back handle obj.
 *
 * Expect being called by operations except for create/destroy_handle
 */
void put_back_handle(struct handle_object *obj)
{
	/* Sanity check */
	if (obj == NULL)
		return;

	__add_to_handle_htab(obj);
}

/*
 * Take out handle obj
 *
 * Expect being called by operations except for destroy_handle.
 */
struct handle_object *take_out_handle(u32 handle)
{
	u32 real_handle;

	real_handle = scramble_handle(handle);

	return __del_from_handle_htab(real_handle);
}

/* Validation on policy, characteristics */
int validate_handle(struct handle_object *obj, enum mkp_policy_id policy, u32 check_char, bool destroy)
{
	/* Validate policy */
	if (obj->policy != policy) {
		BUG_ON(1);
		return ERR_INVALID_POLICY;
	}

	/* Validate characteristic */
	if (get_policy_characteristic(policy) & check_char) {
		do_policy_action(policy, __func__, __LINE__);
		return ERR_INVALID_CHAR;
	}

	/* Validate attr if this is a validation for destroy */
	if (destroy && (obj->attrset & ATTR_DESTROY_FORBIDDEN))
		return ERR_INVALID_ATTR;

	return 0;
}

void init_handle_manipulation(u64 start_ipa, u64 dram_size, u64 smccc_trng_available)
{
	int i;
	hash_bucket_t *bucket;
	hyp_spinlock_t *lock;
	u64 platform_dram_size = dram_size;	// TODO: read DRAM size from pkvm
	struct arm_smccc_res res;
	size_t cbs_size;

	/* handle glist */
	INIT_LIST_HEAD(&handle_glist);

	/* Initialize global lock explicitly */
	hyp_spin_lock_init(&handle_glist_lock);

	/* Initialize bucket lock explicitly */
	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		bucket = &handle_htab[i];
		lock = &bucket->hLock;
		hyp_spin_lock_init(lock);
	}

	/* Get maintainable IPA range */
	HANDLE_IPA_START = start_ipa;
	HANDLE_IPA_END = HANDLE_IPA_START + platform_dram_size;
	//trace_hyp_printk("[MKP] init_handle_manipulation:%d - (0x%llx..0x%llx)",
	//	__LINE__, HANDLE_IPA_START, HANDLE_IPA_END);

	/* Initialize handle pool */
	hyp_spin_lock_init(&handle_lock);

	if (smccc_trng_available) {
		arm_smccc_1_1_smc(ARM_SMCCC_TRNG_RND64, /* dummy argument */0, &res);
		handle_start = res.a1;
		arm_smccc_1_1_smc(ARM_SMCCC_TRNG_RND64, /* dummy argument */0, &res);
		handle_key = res.a1;
	} else {
		handle_start = 100;
		handle_key = 1;
	}
	handle_exhausted = handle_start - 1;

	/* Initialize structure for mapping compaction */
	hyp_spin_lock_init(&cb_lock);
	cbs_cnt = (platform_dram_size + MAPPING_2M - 1) / MAPPING_2M;
	cbs_size = cbs_cnt * sizeof(union compact_blk_2MB);

	cb_section = (union compact_blk_2MB *)malloc(cbs_size);
	if (cb_section)
		module_ops->memset((void *)cb_section, 0, cbs_size);
	else
		//trace_hyp_printk("[MKP] init_handle_manipulation:%d - \
		//	Failed to alloc memory for cb_section!", __LINE__);


	/* Initialize structures for synchronization of exception handling and granting tickets */
#ifdef MERGE_S2EL1_IN_LEGACY
	rw_lock_init(&trap_lock);
#endif
	for (i = 0; i < SMP_MAX_CPUS; i++) {
#ifdef MERGE_S2EL1_IN_LEGACY
		trap_retry[i] = 0;
#endif
		grant_ticket[i] = 0;
	}

	/* TODO: Generate key for ticket */
	ticket_key = 0xFFFFFFFFBBBBBBBB;

	/* TODO: Check whether we support s2 mapping compaction */
	// support_s2_mapping_compaction = mkp_support_s2_mapping_compaction();
}

static void del_from_handle_glist(struct handle_object *obj)
{
	hyp_spin_lock(&handle_glist_lock);
	list_del(&obj->list);
	hyp_spin_unlock(&handle_glist_lock);
}

static struct handle_object *create_handle_object(void)
{
	return malloc(sizeof(struct handle_object));
}

/*
 * Do validation on IPA ranges and add the obj to the
 * sorted global list if validation is pass.
 */
static int validate_add_to_handle_glist(struct handle_object *obj)
{
	struct handle_object *tmp;
	struct list_head *itr;
	u64 tend, end = obj->start + obj->size;
	int ret = 0;

	hyp_spin_lock(&handle_glist_lock);

	/* Add the obj to the list in ascending order */
	list_for_each(itr, &handle_glist) {

		tmp = list_entry(itr, struct handle_object, list);
		if (obj->start < tmp->start) {
			/* tail overlapped */
			if (end > tmp->start)
				ret = -1;
			break;
		}

		tend = tmp->start + tmp->size;
		/* head overlapped */
		if (obj->start < tend) {
			ret = -1;
			break;
		}
	}

	/* No overlap */
	if (ret == 0)
		list_add_tail(&obj->list, itr);

	hyp_spin_unlock(&handle_glist_lock);

	return ret;
}

u32 create_handle(u64 ipa, u64 size, enum mkp_policy_id policy)
{
	u64 range_end;
	struct handle_object *handle_obj = NULL;
	u32 ret_handle, scrambled_handle;

	/* Sanity check (TBD) */
	if (ipa == 0 || (ipa % PAGE_SIZE) != 0) {
		//trace_hyp_printk("[MKP] create_handle:%d invalid ipa(0x%llx)", __LINE__, ipa);
		return 0;
	}

	if (size == 0 || (size % PAGE_SIZE) != 0) {
		//trace_hyp_printk("[MKP] create_handle:%d invalid size(0x%llx) ", __LINE__, size);
		return 0;
	}

	/* TBD: the size should not exceed 1GB */

	/* IPA range should be maintainable */
	range_end = ipa + size;
	if (ipa > range_end || ipa < HANDLE_IPA_START || range_end > HANDLE_IPA_END) {
		//trace_hyp_printk("[MKP] create_handle:%d invalid range(0x%llx..0x%llx)", __LINE__, ipa, range_end);
		return 0;
	}

	/* The handle creation for MKP_POLICY_MKP is forbidden? */
	if (forbid_root_handle && policy == MKP_POLICY_MKP) {
		BUG_ON(1);
		return 0;
	}

	/* Create handle object */
	handle_obj = create_handle_object();
	if (handle_obj == NULL) {
		//trace_hyp_printk("[MKP] create_handle:%d failed to create handle_object", __LINE__);
		return 0;
	}

	/*
	 * ->policy may be referenced right after the insertion of this handle_obj.
	 * So let's move its initialization here.
	 */
	handle_obj->policy = policy;

	/*
	 * Check whether input IPA range overlaps others
	 * [start, end) = [ipa, ipa + size)
	 */
	handle_obj->start = ipa;
	handle_obj->size = size;
	if (validate_add_to_handle_glist(handle_obj) < 0) {
		//trace_hyp_printk("[MKP] create_handle:%d validation failed: ipa(0x%llx), size(0x%llx)",
		//	__LINE__, ipa, size);
		free(handle_obj);
		BUG_ON(1);
		return 0;
	}

retry:
	/* Generate handle */
	hyp_spin_lock(&handle_lock);

	/*
	 * If handle number is exhausted...
	 * The last one will be left as unused and unavailable...
	 * It's fine.
	 */
	if (handle_start == handle_exhausted) {
		hyp_spin_unlock(&handle_lock);
		del_from_handle_glist(handle_obj);
		free(handle_obj);
		//trace_hyp_printk("[MKP] create_handle:%d: no available handles", __LINE__);
		return 0;
	}

	/* Get current handle value and update for the next one */
	ret_handle = handle_start++;
	hyp_spin_unlock(&handle_lock);

	/* Scramble handle , TODO: */
	scrambled_handle = scramble_handle(ret_handle);

	/* We don't expect the handle returned is 0, TODO: */
	if (scrambled_handle == 0)
		goto retry;

	/* Setup handle object */
	handle_obj->handle = ret_handle;
	handle_obj->attrset = 0;

	/* Set maps */
	handle_obj->maps.cbi = (ipa - HANDLE_IPA_START) / MAPPING_2M;
	handle_obj->maps.cbn = (range_end - 1 - HANDLE_IPA_START) / MAPPING_2M - handle_obj->maps.cbi + 1;
	handle_obj->maps.ro = 0;
	handle_obj->maps.rw = 0;
	handle_obj->maps.nx = 0;
	handle_obj->maps._x = 0;
	handle_obj->maps.um = 0;

	/* Add to hash table */
	add_to_handle_htab(handle_obj);

	return scrambled_handle;
}

// TODO: add comment here
int destroy_handle(u32 handle, enum mkp_policy_id policy)
{
	struct handle_object *handle_obj = NULL;
	int ret = 0;
	// int err_line = 0;

	/* Take out the handle */
	handle_obj = del_from_handle_htab(handle);
	if (handle_obj == NULL)
		return ERR_INVALID_HANDLE;

	/* Do validation */
	ret = validate_handle(handle_obj, policy, HANDLE_PERMANENT, true);
	if (ret != 0) {
		// err_line = __LINE__;
		goto err;
	}

	/* Recover to original s2 mapping attrs for MT_MEM */
	if (handle_obj->attrset != 0) {
		ret = reset_to_s2_mapping_attrs(handle_obj);
		if (ret != 0) {
			// err_line = __LINE__;
			goto err;
		}
	}

	/* After validation is passed, handle can be removed safely */
	del_from_handle_glist(handle_obj);
	destroy_handle_object(handle_obj);

	return 0;

err:
	/* Put the handle back */
	add_to_handle_htab(handle_obj);
	//trace_hyp_printk("[MKP] destroy_handle: error occurs at (%d)", err_line);

	return ret;
}
