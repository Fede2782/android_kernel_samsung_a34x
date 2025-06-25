/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __PKVM_TMEM_HOST_H__
#define __PKVM_TMEM_HOST_H__

int kvm_nvhe_sym(hyp_tmem_init)(const struct pkvm_module_ops *ops);
void kvm_nvhe_sym(hyp_region_protect)(struct user_pt_regs *regs);
void kvm_nvhe_sym(hyp_region_unprotect)(struct user_pt_regs *regs);
void kvm_nvhe_sym(hyp_page_protect)(struct user_pt_regs *regs);
void kvm_nvhe_sym(hyp_page_unprotect)(struct user_pt_regs *regs);

#endif
