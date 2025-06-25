// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "arm_div.h"

#if IS_ENABLED(CONFIG_ARM)
unsigned int __aeabi_uldivmod(unsigned int num, unsigned int denom)
{
	return __aeabi_uidivmod(num, denom);
}
EXPORT_SYMBOL_GPL(__aeabi_uldivmod);

int __aeabi_ldivmod(int num, int denom)
{
	return __aeabi_idivmod(num, denom);
}
EXPORT_SYMBOL_GPL(__aeabi_ldivmod);
#endif

