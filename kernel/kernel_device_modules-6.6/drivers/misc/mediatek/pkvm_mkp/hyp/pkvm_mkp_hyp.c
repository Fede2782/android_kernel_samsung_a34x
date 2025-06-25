// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/alternative-macros.h>
#include <asm/barrier.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/io.h>
#include <linux/arm-smccc.h>
#include "lib/malloc.h"
#include "hvcfunc.h"
#include "policy.h"
#include "sharebuf.h"
#include "mkp_handler.h"

#define STATIC_ASSERT(x)
#define assert(x)
#define SRC_CLK_FRQ 124800000
#define UART_PORT 0

typedef unsigned int uint32_t;
typedef struct hvc_retval {
	long x[4];
} hvc_retval_t;

// TODO: suppoosed to declared in handle.h
const struct pkvm_module_ops *module_ops;
hvc_retval_t mkp_hvc_handler(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5, u64 x6);

/* Allow setup of essentials or not */
static int stop_setup_essentials;

void handle__mkp_hyp_hvc(struct user_pt_regs *regs)
{
	hvc_retval_t ret = {{0}};

	/* Handle the call */
	DECLARE_REG(u64, x0, regs, 1);
	DECLARE_REG(u64, x1, regs, 2);
	DECLARE_REG(u64, x2, regs, 3);
	DECLARE_REG(u64, x3, regs, 4);
	DECLARE_REG(u64, x4, regs, 5);
	DECLARE_REG(u64, x5, regs, 6);
	DECLARE_REG(u64, x6, regs, 7);

	cpu_reg(regs, 0) = SMCCC_RET_SUCCESS;
	ret = mkp_hvc_handler(x0, x1, x2, x3, x4, x5, x6);

	cpu_reg(regs, 1) = ret.x[0];
	cpu_reg(regs, 2) = ret.x[1];
	cpu_reg(regs, 3) = ret.x[2];
	cpu_reg(regs, 4) = ret.x[3];
}

int mkp_hyp_prepare1(u64 start_ipa, u64 dram_size, u64 heap_start, u64 heap_size, u64 smccc_trng_available)
{
	int ret = 0;

	/*
	 * Try to update essentials is forbidden after serving this request
	 * no  matter what the result is success or fail.
	 */
	stop_setup_essentials = 1;
	ret = malloc_init((struct pkvm_module_ops *)module_ops, heap_start, heap_size);
	if (ret)
		return ret;

	init_handle_manipulation(start_ipa, dram_size, smccc_trng_available);

	return 0;
}

int mkp_hyp_prepare2(u64 fixaddr_top, u64 fixaddr_real_start)
{
	FIX_END = fixaddr_top;
	FIX_START = fixaddr_real_start;

	return 0;
}

hvc_retval_t mkp_hvc_handler(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5, u64 x6)
{
	hvc_retval_t ret = {{0}};
	sharebuf_update_t update_ret;
	u32 func, policy;
	int retval;

	func = FUNCTION_ID((u32)x0);
	policy = POLICY_ID((u32)x0);

	switch(func) {
	case HVC_FUNC_MKP_HYP_PREPARE1:
		ret.x[0] = mkp_hyp_prepare1(x1, x2, x3, x4, x5);
		break;
	case HVC_FUNC_MKP_HYP_PREPARE2:
		ret.x[0] = mkp_hyp_prepare2(x1, x2);
		break;
	case HVC_FUNC_NEW_POLICY:
		if (policy != 0) {
			ret.x[0] = 0;
			module_ops->puts("[MKP] policy should be 0 !");
		} else {
			retval = request_new_policy(x1);
			if (retval < 0) {
				ret.x[0] = 0;
				module_ops->puts("[MKP] failed to request new policy for policy_char");
			} else {
				ret.x[0] = retval;
			}
		}
		break;
	case HVC_FUNC_CREATE_HANDLE:
		if (policy_is_valid(policy))
			ret.x[0] = create_handle(x1, x2, policy);
		else
			ret.x[0] = 0;

		if (ret.x[0] == 0)
			module_ops->puts("[MKP] failed to create handle for policy");
		break;
	case HVC_FUNC_DESTROY_HANDLE:
		ret.x[0] = destroy_handle((u32)x1, policy);
		break;
	case HVC_FUNC_SET_MAPPING_RO:
		ret.x[0] = mkp_set_mapping_ro(policy, (u32)x1);
		break;
	case HVC_FUNC_SET_MAPPING_RW:
		ret.x[0] = mkp_set_mapping_rw(policy, (u32)x1);
		break;
	case HVC_FUNC_SET_MAPPING_NX:
		ret.x[0] = mkp_set_mapping_nx(policy, (u32)x1);
		break;
	case HVC_FUNC_SET_MAPPING_X:
		ret.x[0] = mkp_set_mapping_x(policy, (u32)x1);
		break;
	case HVC_FUNC_CLEAR_MAPPING:
		ret.x[0] = mkp_clear_mapping(policy, (u32)x1);
		break;
	case HVC_FUNC_LOOKUP_MAPPING_ENTRY:
		ret.x[0] = mkp_lookup_mapping_entry(policy, (u32)x1, (u64 *)&ret.x[1], (u64 *)&ret.x[2]);
		break;
	case HVC_FUNC_CONFIGURE_SHAREBUF:
		ret.x[0] = mkp_configure_sharebuf(policy, (u32)x1, (u32)x2, x3, x4);
		break;
	case HVC_FUNC_UPDATE_SHAREBUF_1_ARGU:
		update_ret = mkp_update_sharebuf_1_argu(policy, (u32)x1, x2, x3);
		ret.x[0] = update_ret.value;
		ret.x[1] = update_ret.index;
		break;
	case HVC_FUNC_UPDATE_SHAREBUF_2_ARGU:
		update_ret = mkp_update_sharebuf_2_argu(policy, (u32)x1, x2, x3, x4);
		ret.x[0] = update_ret.value;
		ret.x[1] = update_ret.index;
		break;
	case HVC_FUNC_UPDATE_SHAREBUF_3_ARGU:
		update_ret = mkp_update_sharebuf_3_argu(policy, (u32)x1, x2, x3, x4, x5);
		ret.x[0] = update_ret.value;
		ret.x[1] = update_ret.index;
		break;
	case HVC_FUNC_UPDATE_SHAREBUF_4_ARGU:
		update_ret = mkp_update_sharebuf_4_argu(policy, (u32)x1, x2, x3, x4, x5, x6);
		ret.x[0] = update_ret.value;
		ret.x[1] = update_ret.index;
		break;
	}

	/* For security, requesting new policy & update actions are no longer allowed after handle creation */
	if (func == HVC_FUNC_CREATE_HANDLE) {
		/* stop_setup_essentials is expected to be 1 here */
		if (stop_setup_essentials != 1) {
			BUG_ON(1);
			return ret;
		}

		/* It's ok to stop policy setup now */
		stop_setup_policy();
	}

	return ret;
}

int mkp_hyp_init(const struct pkvm_module_ops *ops)
{
	ops->register_host_perm_fault_handler(mkp_perm_fault_handler);
	ops->register_illegal_abt_notifier(mkp_illegal_abt_notifier);
	module_ops = (const struct pkvm_module_ops *)ops;
	initialize_policy_table();
	init_sharebuf_manipulation();
	ops->puts("pkvm_mkp el2 init done");

	return 0;
}

