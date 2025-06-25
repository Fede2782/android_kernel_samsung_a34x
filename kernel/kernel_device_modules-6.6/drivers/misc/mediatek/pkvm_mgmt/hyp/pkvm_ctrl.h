/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __ARM64_KVM_NVHE_PKVM_CTRL_H__
#define __ARM64_KVM_NVHE_PKVM_CTRL_H__
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

enum pkvm_control_ops {
	PKVM_HYP_PUTS = 0,
};

void pkvm_print_tfa_char(const char ch);

#endif
