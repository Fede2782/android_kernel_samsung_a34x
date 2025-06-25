/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MKP_HANDLER_H
#define __MKP_HANDLER_H

#include <asm/kvm_pkvm_module.h>
#include "handle.h"

#define MKP_EXCEPTION_NO_ERROR	0
#define MKP_EXCEPTION_NORMAL	1

extern u64 FIX_END;
extern u64 FIX_START;

u32 mkp_sync_handler(struct user_pt_regs *regs);
int mkp_perm_fault_handler(struct user_pt_regs *regs, u64 esr, u64 addr);
void mkp_illegal_abt_notifier(struct user_pt_regs *regs);

#endif
