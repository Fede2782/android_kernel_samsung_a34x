// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/alternative-macros.h>
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>

#include "../../include/pkvm_mgmt/pkvm_mgmt.h"
#include "../../../../arch/arm64/kvm/hyp/include/nvhe/ffa.h"
#include "../../../../arch/arm64/kvm/hyp/include/nvhe/spinlock.h"
#include "pkvm_ctrl.h"

/*
 * Here saves all el2 smc call ids for blocking any invalid el1 smc
 * call to use el2 smc call ids
 */
static const uint64_t el2_smc_id_list[] = {
};

struct nlist {
	u64 key;
	int val;
};

#define HVC_TABLE_SIZE 64
static struct nlist hvc_table[HVC_TABLE_SIZE];
static u32 hvc_cur_index;
static hyp_spinlock_t handler_glist_lock;

void add_hvc(u64 smc_id, int hvc_id)
{
	hyp_spin_lock(&handler_glist_lock);
	hvc_table[hvc_cur_index].key = smc_id;
	hvc_table[hvc_cur_index].val = hvc_id;
	hvc_cur_index++;
	hyp_spin_unlock(&handler_glist_lock);
}

int lookup_hvc(u64 smc_id)
{
	int i = 0;
	int max_size = hvc_cur_index < HVC_TABLE_SIZE ? hvc_cur_index : HVC_TABLE_SIZE;

	for (i = 0; i < max_size; i++) {
		if (hvc_table[i].key == smc_id)
			return hvc_table[i].val;
	}
	return -1;
}

static bool is_ffa_call(u64 func_id)
{
	/* remove maximum bit from the function num field */
	func_id &= ~0x8000UL;

	return ARM_SMCCC_IS_FAST_CALL(func_id) &&
	       ARM_SMCCC_OWNER_NUM(func_id) == ARM_SMCCC_OWNER_STANDARD &&
	       ARM_SMCCC_FUNC_NUM(func_id) >= FFA_MIN_FUNC_NUM &&
	       ARM_SMCCC_FUNC_NUM(func_id) <= FFA_MAX_FUNC_NUM;
}

bool mtk_smc_handler(struct user_pt_regs *ctxt)
{
	u64 smc_id = ctxt->regs[0] & ~ARM_SMCCC_CALL_HINTS;
	u64 smc_key_id;
	int hvc_id;
	size_t i = 0;

	if (is_ffa_call(smc_id)) {
		ctxt->regs[0] &= ~0x8000UL;
		return false;
	}

	for (i = 0; i < ARRAY_SIZE(el2_smc_id_list); i++) {
		if (smc_id == el2_smc_id_list[i])
			return true;
	}

	hvc_id = lookup_hvc(smc_id);
	if (hvc_id != -1) {
		ctxt->regs[1] = hvc_id;
		return true;
	}

	switch (smc_id){
	case SMC_ID_MTK_PKVM_ADD_HVC:
		hvc_id = lookup_hvc(smc_id);
		if (hvc_id != -1) {
			ctxt->regs[0] = SMC_RET_MTK_PKVM_SMC_HANDLER_DUPLICATED_ID;
			ctxt->regs[1] = hvc_id;
			return true;
		}

		smc_key_id = ctxt->regs[1];
		hvc_id = ctxt->regs[2];
		add_hvc(smc_key_id, hvc_id);
		return true;
	}
	return false;
}

/* For pkvm print */
void pkvm_print_tfa_char(const char ch)
{
	arm_smccc_1_1_smc(MTK_SIP_HYP_PKVM_CONTROL, PKVM_HYP_PUTS, (unsigned long)ch, 0, 0);
}

int mtk_smc_handler_hyp_init(const struct pkvm_module_ops *ops)
{
	hyp_spin_lock_init(&handler_glist_lock);
	ops->register_serial_driver(pkvm_print_tfa_char);
	return ops->register_host_smc_handler(mtk_smc_handler);
}
