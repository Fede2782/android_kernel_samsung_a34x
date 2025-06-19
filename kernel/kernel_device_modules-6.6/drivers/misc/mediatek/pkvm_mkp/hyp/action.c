// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include "handle.h"
#include "policy.h"
#include "lib/spinlock.h"

static hyp_spinlock_t polvio_lock __cacheline_aligned;
static u32 policy_violations[MKP_POLICY_NR];
static int start_actions;

/* Do corresponding policy action */
void do_policy_action(enum mkp_policy_id policy, const char *func, int ln)
{
	u32 policy_char_action;
	bool ret = false;

	if (start_actions == 0)
		return;

	/* Identify actions */
	policy_char_action = get_policy_characteristic(policy) & ACTION_BITS;
	switch (policy_char_action) {
	case ACTION_PANIC:
		ret = true;
		break;
	case ACTION_WARNING:
		// TODO: exceed_warning_threshold
	case ACTION_NOTIFICATION:
	default:
		break;
	}

	/* OOPS or not */
	if (ret) {
		//trace_hyp_printk("[MKP] do_policy_action: (OOPS) violation on policy, \
		//	policy(%u) at", policy);
		module_ops->puts(func);
		module_ops->putx64(ln);
		BUG_ON(1);
	} else {
		//trace_hyp_printk("[MKP] do_policy_action: violation on policy(%u) at", policy);
		module_ops->puts(func);
		module_ops->putx64(ln);
	}
}

/* Initialize data structure for policy actions */
void initialize_policy_action(void)
{
	int i;

	/* Initialize the count to 1 */
	for (i = 0; i < MKP_POLICY_NR; i++)
		policy_violations[i] = 0;

	/* Initialize global lock explicitly */
	hyp_spin_lock_init(&polvio_lock);

	/* After this, do_action_xx takes effect */
	start_actions = 1;

	// TODO: UNIT TEST
}
