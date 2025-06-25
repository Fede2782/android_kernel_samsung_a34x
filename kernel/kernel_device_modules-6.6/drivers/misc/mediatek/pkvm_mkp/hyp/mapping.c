// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include "mapping_attr.h"
#include "handle.h"
#include "mkp_err.h"
#include "emi_mpu.h"

/* The heart of lookup mapping entry */
static void __mkp_lookup_mapping_entry(u64 ipa, u64 *entry_level, u64 *permission)
{
	// int ret;
	u64 descriptor = 0;

	/* Clear entry_size and permission */
	*entry_level = 0;
	*permission = 0x0;

	/* Query corresponding descriptor */
	// ret = module_ops->host_stage2_get_leaf(ipa, &descriptor, (u32 *)entry_level);
	module_ops->host_stage2_get_leaf(ipa, &descriptor, (u32 *)entry_level);

	//if (ret)
	//	trace_hyp_printk("[MKP] __mkp_lookup_mapping_entry: failed to get pte");

	/*
	 * For pkvm, there is a lazy map mechanism only happened to the pte with RWX.
	 * In this case, descriptor will be zero.
	 */
	if (descriptor == 0) {
		*permission |= MT_RW;
		*permission |= MT_MEM;
		*permission |= MT_X;
		return;
	}

	/* Query access permission */
	if (descriptor & S2AP_R)
		*permission |= MT_RD;
	if (descriptor & S2AP_W)
		*permission |= MT_WR;
	if (descriptor & S2XN)
		*permission |= MT_UXN;
	else
		*permission |= MT_X;

	/* Query memory type */
	if (descriptor & S2MT_NORMAL)
		*permission |= MT_MEM;
	else
		*permission |= MT_DEV;
}

/* If the permission is 0 after query, this function will MT_MEM if the handle object was unmapped previously */
static inline void lookup_entry_for_mkp_set_mapping(struct handle_object *hobj, u64 *el, u64 *perm)
{
	__mkp_lookup_mapping_entry(hobj->start, el, perm);

	/* Need S2MT_NORMAL */
	if (*perm == MT_DEV && !to_apply_unmap(hobj->attrset))
		*perm = MT_MEM;
}

/* TODO: Need comment to explain */
int reset_to_default_state(u64 perm, u64 start_pfn, u64 nr_pages)
{
	u64 pfn = start_pfn;
	int ret = 0;

	/* Check if it is default state */
	if ((perm & S2_MEM_DEFAULT_ATTR) != S2_MEM_DEFAULT_ATTR) {
		ret = module_ops->host_stage2_mod_prot(pfn, S2_MEM_DEFAULT_ATTR, nr_pages, false);

		//if (ret_final)
		//	trace_hyp_printk("[MKP] reset_to_default_state: failed to reset property, ret: %d", ret_final);
	}

	return ret;
}

/* for HVC_FUNC_SET_MAPPING_RO */
int mkp_set_mapping_ro(u32 policy, u32 handle)
{
	struct handle_object *handle_obj;
	int ret = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, /* Nothing to check */0, false);
	if (ret != 0)
		goto out;

	/* Has RO been applied */
	if (to_apply_ro(handle_obj->attrset)) {
		u64 el, perm, pfn;
		int nr_pages, ret = 0;
		bool no_hw_access = policy_has_no_hw_access(policy);

		/* Look up the current permission */
		lookup_entry_for_mkp_set_mapping(handle_obj, &el, &perm);

		/* Check whether MT_WR exists */
		if ((perm & MT_WR) == MT_WR || no_hw_access) {
			/* IPA == PA */
			nr_pages = handle_obj->size >> PAGE_SHIFT;
			pfn = handle_obj->start >> PAGE_SHIFT;

			/* Check if it is default state */
			ret = reset_to_default_state(perm, pfn, nr_pages);
			if (ret) {
				//trace_hyp_printk("[MKP] mkp_set_mapping_ro, failed to reset property, ret: %d",
				//	ret_final);
				goto out;
			}

			ret = module_ops->host_stage2_mod_prot(pfn, perm & ~MT_WR, nr_pages, false);
		}

		/* Update attrset if applied successfully */
		if (!ret) {
			handle_obj->attrset = ro_is_applied(handle_obj->attrset);

			/* MPU Kernel Protection region setup */
			// TODO: Need to resolve an issue of smc call to tfa
			/*
			if (policy == MKP_POLICY_KERNEL_CODE) {
				if (emi_kp_set_protection(handle_obj->start, handle_obj->start + handle_obj->size,
						KP_RGN_KERNEL_CODE, MKP_KERNEL))
					//trace_hyp_printk("mkp_set_mapping_ro:%d : emi_kp_set_protection failed:\
					//	region id %d",	__LINE__, KP_RGN_KERNEL_CODE);
			} else if (policy == MKP_POLICY_KERNEL_RODATA) {
				if (emi_kp_set_protection(handle_obj->start, handle_obj->start + handle_obj->size,
						KP_RGN_KERNEL_RODATA, MKP_KERNEL))
					//trace_hyp_printk("mkp_set_mapping_ro:%d : emi_kp_set_protection failed:\
					//	region id %d\n", __LINE__, KP_RGN_KERNEL_RODATA);
			}
			*/
		}
	}

out:
	/* After operation, we need to put back handle */
	put_back_handle(handle_obj);

	return ret;
}

/* for HVC_FUNC_SET_MAPPING_RW */
int mkp_set_mapping_rw(u32 policy, u32 handle)
{
	struct handle_object *handle_obj;
	int ret = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, to_apply_ro(handle_obj->attrset) ? 0 : NO_UPGRADE_TO_WRITE, false);
	if (ret != 0)
		goto out;

	/* Has RW been applied */
	if (to_apply_rw(handle_obj->attrset)) {
		u64 el, perm, pfn;
		int nr_pages, ret = 0;
		bool no_hw_access = policy_has_no_hw_access(policy);

		/* Look up the current permission */
		lookup_entry_for_mkp_set_mapping(handle_obj, &el, &perm);

		/* Check whether MT_RW exists */
		if ((perm & MT_RW) != MT_RW || no_hw_access) {
			/* IPA == PA */
			nr_pages = handle_obj->size >> PAGE_SHIFT;
			pfn = handle_obj->start >> PAGE_SHIFT;

			/* Check if it is default state */
			ret = reset_to_default_state(perm, pfn, nr_pages);
			if (ret) {
				//trace_hyp_printk("[MKP] mkp_set_mapping_rw,  failed to reset property, ret: %d",
				//	ret_final);
				goto out;
			}

			ret = module_ops->host_stage2_mod_prot(pfn, perm | MT_RW, nr_pages, false);
		}

		/* Update attrset if applied successfully */
		if (!ret)
			handle_obj->attrset = rw_is_applied(handle_obj->attrset);
	}

out:
	/* After operation, we need to put back handle */
	put_back_handle(handle_obj);

	return ret;

}

/* for HVC_FUNC_SET_MAPPING_NX */
int mkp_set_mapping_nx(u32 policy, u32 handle)
{
	struct handle_object *handle_obj;
	int ret = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, /* Nothing to check */0, false);
	if (ret != 0)
		goto out;

	/* Has NX been applied */
	if (to_apply_nx(handle_obj->attrset)) {
		u64 el, perm, pfn;
		int nr_pages, ret = 0;
		bool no_hw_access = policy_has_no_hw_access(policy);

		/* Look up the current permission */
		lookup_entry_for_mkp_set_mapping(handle_obj, &el, &perm);

		/* Check whether MT_AXN exists */
		if ((perm & MT_AXN) == 0 || no_hw_access) {
			/* IPA == PA */
			nr_pages = handle_obj->size >> PAGE_SHIFT;
			pfn = handle_obj->start >> PAGE_SHIFT;

			/* Check if it is default state */
			ret = reset_to_default_state(perm, pfn, nr_pages);
			if (ret) {
				//trace_hyp_printk("MKP: mkp_set_mapping_nx,  failed to reset property, ret: %d",
				//	ret_final);
				goto out;
			}

			ret = module_ops->host_stage2_mod_prot(pfn, (perm & ~MT_X) | KVM_PGTABLE_PROT_UXN, nr_pages,
				false);
		}

		/* Update attrset if applied successfully */
		if (!ret)
			handle_obj->attrset = nx_is_applied(handle_obj->attrset);
	}

out:
	/* After operation, we need to put back handle */
	put_back_handle(handle_obj);

	return ret;

}

/* for HVC_FUNC_SET_MAPPING_X */
int mkp_set_mapping_x(u32 policy, u32 handle)
{
	struct handle_object *handle_obj;
	int ret = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, to_apply_nx(handle_obj->attrset) ? 0 : NO_UPGRADE_TO_EXEC, false);
	if (ret != 0)
		goto out;

	/* Has X been applied */
	if (to_apply__x(handle_obj->attrset)) {
		u64 el, perm, pfn;
		int nr_pages, ret = 0;
		bool no_hw_access = policy_has_no_hw_access(policy);

		/* Look up the current permission */
		lookup_entry_for_mkp_set_mapping(handle_obj, &el, &perm);

		/* Check whether MT_AXN exists */
		if ((perm & MT_AXN) != 0 || no_hw_access) {
			/* IPA == PA */
			nr_pages = handle_obj->size >> PAGE_SHIFT;
			pfn = handle_obj->start >> PAGE_SHIFT;

			/* Check if it is default state */
			ret = reset_to_default_state(perm, pfn, nr_pages);
			if (ret) {
				//trace_hyp_printk("[MKP] mkp_set_mapping_x,  failed to reset property, ret: %d",
				//	ret_final);
				goto out;
			}

			ret = module_ops->host_stage2_mod_prot(pfn, (perm & ~MT_AXN) | MT_X, nr_pages, false);
		}

		/* Update attrset if applied successfully */
		if (!ret)
			handle_obj->attrset = _x_is_applied(handle_obj->attrset);
	}

out:
	/* After operation, we need to put back handle */
	put_back_handle(handle_obj);

	return ret;

}

/* for HVC_FUNC_CLEAR_MAPPING */
int mkp_clear_mapping(u32 policy, u32 handle)
{
	struct handle_object *handle_obj;
	int ret = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, to_apply_nx(handle_obj->attrset) ? 0 : NO_UPGRADE_TO_EXEC, false);
	if (ret != 0)
		goto out;

	/* Has unmap been applied */
	if (to_apply_unmap(handle_obj->attrset)) {
		u64 pfn;
		int nr_pages;

		/* IPA == PA */
		nr_pages = handle_obj->size >> PAGE_SHIFT;
		pfn = handle_obj->start >> PAGE_SHIFT;
		ret = module_ops->host_donate_hyp(pfn, nr_pages, 0);

		/* Update attrset if applied successfully */
		if (!ret)
			handle_obj->attrset = unmap_is_applied(handle_obj->attrset);
	}

out:
	/* After operation, we need to put back handle */
	put_back_handle(handle_obj);

	return ret;

}

/* for HVC_FUNC_LOOKUP_MAPPING_ENTRY */
int mkp_lookup_mapping_entry(u32 policy, u32 handle, u64 *entry_size, u64 *permission)
{
#define PERM_R	(0b100)
#define PERM_W	(0b010)
#define PERM_X	(0b001)

	struct handle_object *handle_obj;
	int ret = 0;
	u64 perm, entry_level, perm_rwx = 0;

	/* Before operation, we need to take out handle */
	handle_obj = take_out_handle(handle);

	/* Is handle valid */
	if (handle_obj == NULL) {
		ret = ERR_INVALID_HANDLE;
		goto out;
	}

	/* Do validation */
	ret = validate_handle(handle_obj, policy, to_apply_nx(handle_obj->attrset) ? 0 : NO_UPGRADE_TO_EXEC, false);
	if (ret != 0)
		goto out;

	// TODO: MAPPING_CB_LOCK()

	/* Look up the current permission */
	__mkp_lookup_mapping_entry(handle_obj->start, &entry_level, &perm);
	*entry_size = BIT(ARM64_HW_PGTABLE_LEVEL_SHIFT(entry_level));

	// TODO: MAPPING_CB_UNLOCK()

	/* Transform to (R: 100b, W:010b, X: 001b) */
	if ((perm & MT_RD) == MT_RD)
		perm_rwx |= PERM_R;
	if ((perm & MT_WR) == MT_WR)
		perm_rwx |= PERM_W;
	if ((perm & MT_AXN) == 0)
		perm_rwx |= PERM_X;

	/* Return result */
	*permission = perm_rwx;

out:
	/* After operation, we need to put back handle */
	put_back_handle(handle_obj);
	return ret;

#undef	PERM_R
#undef	PERM_W
#undef	PERM_X
}

/* Recover to original S2 mapping attrs for MT_MEM */
int reset_to_s2_mapping_attrs(struct handle_object *obj)
{
	u64 el, perm;
	u64 pfn;
	int ret = 0;
	int nr_pages;

	/* Look up the current permission */
	__mkp_lookup_mapping_entry(obj->start, &el, &perm);

	/* Check whether the attr is the same as original S2 mapping ones */
	// TODO: not sure if it is required to do compaciton

	/* Check if it is unmapped */

	/* Recover to original S2 mapping attrs for MT_MEM */
	nr_pages = obj->size >> PAGE_SHIFT;
	pfn = obj->start >> PAGE_SHIFT;
	ret = module_ops->host_stage2_mod_prot(pfn, KVM_PGTABLE_PROT_RWX, nr_pages, false);
	/*
	for (i = 0; i < nr_pages; i++) {
		ret = module_ops->host_stage2_mod_prot(pfn, KVM_PGTABLE_PROT_RWX);
		pfn += 1;
		ret_final = ret_final | ret;
	}
	*/

	return ret;
}
