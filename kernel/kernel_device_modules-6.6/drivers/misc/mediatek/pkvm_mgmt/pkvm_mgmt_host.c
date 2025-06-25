// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/kvm_pkvm_module.h>

#include "../include/pkvm_mgmt/pkvm_mgmt.h"

#undef pr_fmt
#define pr_fmt(fmt) "pKVM handler: " fmt

int __kvm_nvhe_mtk_smc_handler_hyp_init(const struct pkvm_module_ops *ops);

static int __init mtk_smc_handler_nvhe_init(void)
{
	int ret;
	unsigned long token;

	if (!is_protected_kvm_enabled())
		return 0;

	ret = pkvm_load_el2_module(__kvm_nvhe_mtk_smc_handler_hyp_init, &token);
	if (ret)
		return ret;
	pr_info("pkvm smc handler okay\n");
	return 0;
}
module_init(mtk_smc_handler_nvhe_init);
MODULE_LICENSE("GPL");
