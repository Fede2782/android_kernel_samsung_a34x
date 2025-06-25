/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __HVC_FUNC_H
#define __HVC_FUNC_H
#include <asm/kvm_pkvm_module.h>
#include "policy.h"
#include "sharebuf.h"

enum mkp_hvc_func_num {
	/* 0 ~ 15 : No use */
	HVC_FUNC_GUARD_1 = 15,

	/* 16 ~ 31 : Policy ops */
	HVC_FUNC_NEW_POLICY = 16,
	HVC_FUNC_POLICY_ACTION = 17,
	HVC_FUNC_NEW_SPECIFIED_POLICY = 18,
	HVC_FUNC_GUARD_2 = 31,

	/* 32 ~ 47 : Handle ops */
	HVC_FUNC_CREATE_HANDLE = 32,
	HVC_FUNC_DESTROY_HANDLE = 33,
	HVC_FUNC_GUARD_3 = 47,

	/* 48 ~ 63 : Mapping ops */
	HVC_FUNC_SET_MAPPING_RO = 48,
	HVC_FUNC_SET_MAPPING_RW = 49,
	HVC_FUNC_SET_MAPPING_NX = 50,
	HVC_FUNC_SET_MAPPING_X = 51,
	HVC_FUNC_CLEAR_MAPPING = 52,
	HVC_FUNC_LOOKUP_MAPPING_ENTRY = 53,
	HVC_FUNC_GUARD_4 = 63,

	/* 64 ~ 79 : Sharebuf ops */
	HVC_FUNC_CONFIGURE_SHAREBUF = 64,
	HVC_FUNC_UPDATE_SHAREBUF_1_ARGU = 65,
	HVC_FUNC_UPDATE_SHAREBUF_2_ARGU = 66,
	HVC_FUNC_UPDATE_SHAREBUF_3_ARGU = 67,
	HVC_FUNC_UPDATE_SHAREBUF_4_ARGU = 68,
	HVC_FUNC_UPDATE_SHAREBUF_5_ARGU = 69,
	HVC_FUNC_UPDATE_SHAREBUF = 70,
	HVC_FUNC_GUARD_5 = 79,

	/* 80 ~ 95 : Helper ops */
	HVC_FUNC_MKP_HYP_PREPARE1 = 80,
	HVC_FUNC_MKP_HYP_PREPARE2 = 81,
	HVC_FUNC_GUARD_6 = 95,

	/* 96 ~ 127 : Essential for MKP service */
	HVC_FUNC_ESS_0 = 96,
	HVC_FUNC_ESS_1 = 97,
	HVC_FUNC_GUARD_7 = 127,

	/* 128 ~ 255 : No use */
	HVC_FUNC_GUARD_8 = 255,
};

/* policy ops for hvc function */
extern int request_new_policy(u64 policy_char);

/* handle ops for hvc function */
extern u32 create_handle(u64 ipa, u64 size, enum mkp_policy_id policy);
extern int destroy_handle(u32 handle, enum mkp_policy_id policy);

/* mapping ops for hvc function */
extern int mkp_set_mapping_ro(u32 policy, u32 handle);
extern int mkp_set_mapping_rw(u32 policy, u32 handle);
extern int mkp_set_mapping_nx(u32 policy, u32 handle);
extern int mkp_set_mapping_x(u32 policy, u32 handle);
extern int mkp_clear_mapping(u32 policy, u32 handle);
extern int mkp_lookup_mapping_entry(u32 policy, u32 handle, u64 *entry_size, u64 *permission);

/* sharebuf ops for hvc function */
extern int mkp_configure_sharebuf(u32 policy, u32 handle, u32 type, u64 num, u64 size);
extern sharebuf_update_t mkp_update_sharebuf_1_argu(u32 policy, u32 handle, u64 index, u64 u64_a1);
extern sharebuf_update_t mkp_update_sharebuf_2_argu(u32 policy, u32 handle, u64 index, u64 u64_a1, u64 u64_a2);
extern sharebuf_update_t mkp_update_sharebuf_3_argu(u32 policy, u32 handle, u64 index, u64 u64_a1, u64 u64_a2,
	u64 u64_a3);
extern sharebuf_update_t mkp_update_sharebuf_4_argu(u32 policy, u32 handle, u64 index, u64 u64_a1, u64 u64_a2,
	u64 u64_a3, u64 u64_a4);

/* mkp_update_sharebuf_5_argu is not available for pkvm */
#endif
