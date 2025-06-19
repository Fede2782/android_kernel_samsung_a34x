// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <asm/alternative-macros.h>
#include <asm/barrier.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/io.h>
#include <linux/arm-smccc.h>

#include "hyp_tmem_mpu.h"
#include "hyp_pmm.h"

#define cpu_reg(ctxt, r)    ((ctxt)->regs[r])

const struct pkvm_module_ops *tmem_ops;

int hyp_tmem_init(const struct pkvm_module_ops *ops)
{
	ops->puts("pkvm_tmem: hyp_tmem_init\n");

	tmem_ops = ops;

	return 0;
}

void hyp_region_protect(struct user_pt_regs *regs)
{
	tmem_ops->puts("pkvm_tmem: hyp_region_protect\n");
	cpu_reg(regs, 1) = enable_region_protection(regs->regs[1],
						regs->regs[2], regs->regs[3], tmem_ops);
	cpu_reg(regs, 0) = SMCCC_RET_SUCCESS;
}

void hyp_region_unprotect(struct user_pt_regs *regs)
{
	tmem_ops->puts("pkvm_tmem: hyp_region_unprotect\n");
	cpu_reg(regs, 1) = disable_region_protection(regs->regs[1], tmem_ops);
	cpu_reg(regs, 0) = SMCCC_RET_SUCCESS;
}

void hyp_page_protect(struct user_pt_regs *regs)
{
	tmem_ops->puts("pkvm_tmem: hyp_page_protect\n");
	cpu_reg(regs, 1) = hyp_pmm_secure_pglist(regs->regs[1],
						regs->regs[2], regs->regs[3], tmem_ops);
	cpu_reg(regs, 0) = SMCCC_RET_SUCCESS;
}

void hyp_page_unprotect(struct user_pt_regs *regs)
{
	tmem_ops->puts("pkvm_tmem: hyp_page_unprotect\n");
	cpu_reg(regs, 1) = hyp_pmm_unsecure_pglist(regs->regs[1],
						regs->regs[2], regs->regs[3], tmem_ops);
	cpu_reg(regs, 0) = SMCCC_RET_SUCCESS;
}
