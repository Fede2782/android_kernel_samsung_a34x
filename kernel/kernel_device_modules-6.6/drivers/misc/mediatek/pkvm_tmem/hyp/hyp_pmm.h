/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __HYP_PMM_H__
#define __HYP_PMM_H__

#include <linux/kernel.h>

#define ONE_PAGE_OFFSET 12
#define ONE_PAGE_SIZE (1 << ONE_PAGE_OFFSET)

int hyp_pmm_secure_pglist(uint64_t pglist_msg, uint32_t attr,
				uint32_t count, const struct pkvm_module_ops *tmem_ops);
int hyp_pmm_unsecure_pglist(uint64_t pglist_msg, uint32_t attr,
				uint32_t count, const struct pkvm_module_ops *tmem_ops);
#endif
