/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MKP_DEMO_HOST_H
#define __MKP_DEMO_HOST_H

#include <asm/kvm_asm.h>

int kvm_nvhe_sym(mkp_hyp_init)(const struct pkvm_module_ops *ops);
void kvm_nvhe_sym(handle__mkp_hyp_hvc)(struct user_pt_regs *regs);

#endif
