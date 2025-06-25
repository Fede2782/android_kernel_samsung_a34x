// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>

#include "mkp_hvc.h"
#include "debug.h"

DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

int mkp_set_mapping_ro_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_RO);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_num: %d, hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		hvc_number, mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a1 ? -1 : 0;
}

int mkp_set_mapping_rw_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_RW);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a1 ? -1 : 0;
}

int mkp_set_mapping_nx_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_NX);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_num: %d, hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		hvc_number, mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a1 ? -1 : 0;
}

int mkp_set_mapping_x_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_X);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a1 ? -1 : 0;
}

int mkp_clear_mapping_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_CLEAR_MAPPING);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a1 ? -1 : 0;
}

int mkp_lookup_mapping_entry_hvc_call(uint32_t policy, uint32_t handle,
	unsigned long *entry_size, unsigned long *permission)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_LOOKUP_MAPPING_ENTRY);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	if (res.a1)
		return -1;

	*entry_size = res.a2;
	*permission = res.a3;
	return 0;

}
int mkp_req_new_policy_hvc_call(unsigned long policy_char)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	uint32_t policy = 0;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_NEW_POLICY);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, policy_char);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	policy = (uint32_t)res.a1;

	if (res.a1 == 0)
		return -1;

	return (int)policy;
}

int mkp_change_policy_action_hvc_call(uint32_t policy, unsigned long policy_char_action)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	int ret = -1;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_POLICY_ACTION);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, policy_char_action);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	ret = (int)res.a1;

	/* Success: 0, Fail: other */
	return ret;
}

int mkp_req_new_specified_policy_hvc_call(unsigned long policy_char, uint32_t specified_policy)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	uint32_t policy = 0;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_NEW_SPECIFIED_POLICY);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, policy_char, specified_policy);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, specified_policy, res.a0, res.a1, res.a2, res.a3);
	policy = (uint32_t)res.a1;

	if (res.a1 == 0)
		return -1;

	/* Expect "policy == specified_policy" */
	if (policy != specified_policy)
		return -1;

	return (int)policy;
}

uint32_t mkp_create_handle_hvc_call(uint32_t policy,
	unsigned long ipa, unsigned long size)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	uint32_t handle = 0;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_CREATE_HANDLE);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, ipa, size);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	handle = (uint32_t)(res.a1);

	// if fail return 0
	// success return >0

	return handle;
}
int mkp_destroy_handle_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_DESTROY_HANDLE);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a1 ? -1 : 0;
}

int mkp_configure_sharebuf_hvc_call(uint32_t policy, uint32_t handle, uint32_t type,
	unsigned long nr_entries, unsigned long size)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_CONFIGURE_SHAREBUF);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle, type, nr_entries, size);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a1 ? -1 : 0;
}

int mkp_update_sharebuf_1_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_1_ARGU);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle, index, a1);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a1 ? -1 : (int)res.a2;
}

int mkp_update_sharebuf_2_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_2_ARGU);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle, index, a1, a2);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a1 ? -1 : (int)res.a2;
}

int mkp_update_sharebuf_3_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_3_ARGU);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle, index, a1, a2, a3);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a1 ? -1 : (int)res.a2;
}

int mkp_update_sharebuf_4_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_4_ARGU);
	res = mkp_el2_mod_call(hvc_number, mkp_hvc_fast_call_id, handle, index, a1, a2, a3, a4);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a1 ? -1 : (int)res.a2;
}

int mkp_update_sharebuf_5_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5)
{
	MKP_DEBUG("%s:%d Not support for pkvm\n", __func__, __LINE__);
	return -1;
}

int mkp_update_sharebuf_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long ipa)
{
	MKP_DEBUG("%s:%d Not support for pkvm\n", __func__, __LINE__);
	return -1;

}

