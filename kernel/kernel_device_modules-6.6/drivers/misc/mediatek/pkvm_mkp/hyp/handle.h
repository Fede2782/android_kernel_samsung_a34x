/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __HANDLE_H
#define __HANDLE_H

#include <asm/kvm_pkvm_module.h>
#include "lib/spinlock.h"
#include "policy.h"

#define cpu_reg(regs, r)	(regs)->regs[r]
#define DECLARE_REG(type, name, regs, reg)\
			type name = (type)cpu_reg(regs, (reg))

enum attr_type {
	ATTR_RW = 0x0001,
	ATTR_RO = 0x0002,
	ATTR_WO = 0x0004,
	ATTR__X = 0x0010,
	ATTR_NX = 0x0020,
	ATTR_SB_SET = 0x0040,
	ATTR_UM = 0x8000,
};

#define ATTR_DESTROY_FORBIDDEN	(ATTR_UM)

/* Helpers for applied attributes */
static inline bool to_apply_ro(u32 curr_attr)
{
	return !(curr_attr & ATTR_RO);
}

static inline u32 ro_is_applied(u32 curr_attr)
{
	return ((curr_attr & ~(ATTR_RW | ATTR_WO)) | ATTR_RO);
}

static inline bool to_apply_wo(u32 curr_attr)
{
	return !(curr_attr & ATTR_WO);
}

static inline u32 wo_is_applied(u32 curr_attr)
{
	return ((curr_attr & ~(ATTR_RW | ATTR_RO)) | ATTR_WO);
}

static inline bool to_apply_rw(u32 curr_attr)
{
	return !(curr_attr & ATTR_RW);
}

static inline u32 rw_is_applied(u32 curr_attr)
{
	return ((curr_attr & ~(ATTR_RO | ATTR_WO)) | ATTR_RW);
}

static inline bool to_apply__x(u32 curr_attr)
{
	return !(curr_attr & ATTR__X);
}

static inline u32 _x_is_applied(u32 curr_attr)
{
	return ((curr_attr & ~ATTR_NX) | ATTR__X);
}

static inline bool to_apply_nx(u32 curr_attr)
{
	return !(curr_attr & ATTR_NX);
}

static inline u32 nx_is_applied(u32 curr_attr)
{
	return ((curr_attr & ~ATTR__X) | ATTR_NX);
}

static inline u32 to_apply_unmap(u32 curr_attr)
{
	return !(curr_attr & ATTR_UM);
}

static inline u32 unmap_is_applied(u32 curr_attr)
{
	return (curr_attr | ATTR_UM);	/* For security, don't clear other attributes */
}

static inline u32 to_apply_sb_set(u32 curr_attr)
{
	return (!(curr_attr & ATTR_SB_SET));
}

static inline u32 sb_set_is_applied(u32 curr_attr)
{
	return (curr_attr | ATTR_SB_SET);
}

void init_handle_manipulation(u64 start_ipa, u64 dram_size, u64 smccc_trng_available);

typedef struct hash_bucket {
	struct handle_object *hList;
	hyp_spinlock_t hLock;
} hash_bucket_t;

struct handle_object {
	u32 handle;
	u32 policy;
	u32 attrset;

	struct map_status {
		u32 cbi:16;
		u32 cbn:11;
		u32 ro:1;
		u32 rw:1;
		u32 nx:1;
		u32 _x:1;
		u32 um:1;
	} maps;

	u64 start;
	u64 size;
	struct list_head list;
	struct handle_object *hList;
};

union compact_blk_2MB {
	u64 val;
	struct {
		u64 ro_cnt:12;
		u64 rw_cnt:12;
		u64 nx_cnt:12;
		u64 _x_cnt:12;
		u64 um_cnt:16;
	};
};

#define MAPPING_4K		(0x1000UL)
#define MAPPING_1M		(0x100000UL)
#define MAPPING_2M		(0x200000UL)
#define MAPPING_2M_MASK		~(MAPPING_2M - 1)
extern hyp_spinlock_t cb_lock;
extern union compact_blk_2MB *cb_section;
extern u64 cbs_cnt;

// extern bool support_s2_mapping_compaction;
// TODO: extern bool mkp_support_s2_mapping(void);

/* Handle manipulation functions */
extern void put_back_handle(struct handle_object *obj);
extern struct handle_object *take_out_handle(u32 handle);
extern int validate_handle(struct handle_object *obj, enum mkp_policy_id policy, u32 check_char, bool destroy);
extern u32 scramble_handle(u32 handle);

/* Auxiliary for destroy_handle (implement in mapping.c) */
extern int reset_to_s2_mapping_attrs(struct handle_object *obj);

/* Helper function to find out corresponding policy from ipa */
extern u32 query_policy_from_ipa(u64 ipa);

// For pkvm ops
extern const struct pkvm_module_ops *module_ops;
#endif
