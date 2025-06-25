// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <soc/mediatek/smpu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>

char *smpu_clear_md_violation(void)
{
	char *ret = "dummy";
	pr_info("%s: dummy for legacy!\n", __func__);
	return ret;
}
EXPORT_SYMBOL(smpu_clear_md_violation);

int mtk_smpu_isr_hook_register(smpu_isr_hook hook)
{
	pr_info("%s: dummy for legacy!\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mtk_smpu_isr_hook_register);

int mtk_smpu_md_handling_register(smpu_md_handler md_handling_func)
{
	pr_info("%s: dummy for legacy!\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mtk_smpu_md_handling_register);

static __init int smpu_init(void)
{
	pr_info("%s:smpu dummy was loaded.\n", __func__);
	return 0;
}

module_init(smpu_init);

MODULE_DESCRIPTION("MediaTek SMPU Dummy Driver");
MODULE_LICENSE("GPL");

