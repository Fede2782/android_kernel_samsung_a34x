// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include <asm/alternative.h>
#include <asm/lse.h>
#include <asm/rwonce.h>
#include "lib/spinlock.h"
#include "policy.h"
#include "mkp_err.h"
#include "handle.h"

/* lock for policy control */
static hyp_spinlock_t policy_lock __cacheline_aligned;

/* Policy Table */
static u32 mkp_policy[MKP_POLICY_NR] = {
	[MKP_POLICY_MKP]		= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | NO_UPGRADE_TO_EXEC | HANDLE_PERMANENT | ACTION_PANIC),
	[MKP_POLICY_DRV]		= (NO_MAP_TO_DEVICE | ACTION_PANIC),
	[MKP_POLICY_SELINUX_STATE]	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | ACTION_PANIC),
	[MKP_POLICY_SELINUX_AVC]	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | SB_ENTRY_DISORDERED | ACTION_WARNING),
	[MKP_POLICY_TASK_CRED]		= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | SB_ENTRY_ORDERED | ACTION_WARNING),
	[MKP_POLICY_KERNEL_CODE]	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | ACTION_WARNING),
	[MKP_POLICY_KERNEL_RODATA]	= (NO_MAP_TO_DEVICE | NO_UPGRADE_TO_WRITE | HANDLE_PERMANENT | ACTION_WARNING),
	[MKP_POLICY_KERNEL_PAGES]	= (0x0 | ACTION_NOTIFICATION),
	[MKP_POLICY_PGTABLE]		= (0x0 | ACTION_NOTIFICATION),
	[MKP_POLICY_S1_MMU_CTRL]	= (0x0 | ACTION_NOTIFICATION),
	[MKP_POLICY_FILTER_SMC_HVC]	= (0x0 | ACTION_NOTIFICATION),
};

/* Request of new policy starts */
static u32 next_policy_for_request = MKP_POLICY_VENDOR_START;

/* Tell MKP when to stop policy setip (request new, or change action) */
static int stop_setup_policy_done;

static bool invalid_policy_char(u64 policy_char)
{
	/* Invalid policy_char */
	if ((policy_char & (SB_ENTRY_DISORDERED | SB_ENTRY_ORDERED)) == (SB_ENTRY_DISORDERED | SB_ENTRY_ORDERED))
		return true;

	/* Invalid policy_char - exceed valid falg */
	if (policy_char >= ACTION_BITS)
		return true;

	/* Invalid policy action */
	if ((policy_char & ACTION_BITS) != 0x0 &&
	    (policy_char & ACTION_BITS) != ACTION_PANIC &&
	    (policy_char & ACTION_BITS) != ACTION_WARNING &&
	    (policy_char & ACTION_BITS) != ACTION_NOTIFICATION)
		return true;

	return false;
}

/* Get chearacteristic of corresponding policy */
static u32 __get_policy_characteristic(enum mkp_policy_id policy)
{
	/* Sanity check */
	if (policy < 0 || policy >= MKP_POLICY_NR)
		return CHAR_POLICY_INVALID;

	/* Is policy valid */
	if (mkp_policy[policy] & CHAR_POLICY_INVALID)
		return CHAR_POLICY_INVALID;

	return mkp_policy[policy];
}

u32 get_policy_characteristic(enum mkp_policy_id policy)
{
	u32 policy_char;

	hyp_spin_lock(&policy_lock);

	policy_char = __get_policy_characteristic(policy);

	hyp_spin_unlock(&policy_lock);

	return policy_char;
}

static bool __policy_is_valid(enum mkp_policy_id policy)
{
	return true;
}

bool policy_is_valid(enum mkp_policy_id policy)
{
	bool ret;

	ret = __policy_is_valid(policy);

	return ret;
}

/* Input policy is guaranteed to be valid after successful handle creation */
bool policy_has_no_hw_access(enum mkp_policy_id policy)
{
	if (get_policy_characteristic(policy) & NO_MAP_TO_DEVICE)
		return true;

	return false;
}

void initialize_policy_table(void)
{
	int i;

	/* Mark unused policies as CHAR_POLICY_INVALID */
	for (i = MKP_POLICY_DEFAULT_END; i < MKP_POLICY_NR; i++)
		mkp_policy[i] |= CHAR_POLICY_INVALID;

	/* Mark vendor available policies */
	for (i = MKP_POLICY_VENDOR_START; i < MKP_POLICY_NR; i++)
		mkp_policy[i] |= CHAR_POLICY_AVAILABLE;

	// TODO: sth for UNIT_TEST

	/* Initialize global lock explicitly */
	hyp_spin_lock_init(&policy_lock);

	// TODO: sth for UNIT_TEST
}

bool check_sb_entry_ordered(enum mkp_policy_id policy)
{
	u32 policy_char = get_policy_characteristic(policy);

	return (policy_char & SB_ENTRY_ORDERED);
}

bool check_sb_entry_disordered(enum mkp_policy_id policy)
{
	u32 policy_char = get_policy_characteristic(policy);

	return (policy_char & SB_ENTRY_DISORDERED);
}

/* Clear CHAR_POLICY_AVAILABLE */
static void clear_all_policy_available(void)
{
	int i;

	hyp_spin_lock(&policy_lock);

	for (i = MKP_POLICY_VENDOR_START; i < MKP_POLICY_NR; i++)
		mkp_policy[i] &= ~CHAR_POLICY_AVAILABLE;

	hyp_spin_unlock(&policy_lock);
}

/* for HVC_FUNC_NEW_POLICY */
int request_new_policy(u64 policy_char)
{
	int ret = 0;

	/* Validate policy_char */
	if (invalid_policy_char(policy_char))
		return ERR_INVALID_CHAR;

	hyp_spin_lock(&policy_lock);

	/* Try to find available one */
	while (next_policy_for_request < MKP_POLICY_NR) {

		if ((mkp_policy[next_policy_for_request] & CHAR_POLICY_AVAILABLE) == CHAR_POLICY_AVAILABLE)
			break;

		next_policy_for_request++;
	}

	/* No available policy for request */
	if (next_policy_for_request >= MKP_POLICY_NR) {
		ret = ERR_NO_AVAIL_POLICY;
		goto err;
	}

	ret = next_policy_for_request++;
	mkp_policy[ret] = policy_char;

err:
	hyp_spin_unlock(&policy_lock);
	return ret;
}

/* Stop policy setup (request new, or change action) */
void stop_setup_policy(void)
{
	if (stop_setup_policy_done > 0)
		return;

	clear_all_policy_available();
	stop_setup_policy_done = 1;
}
