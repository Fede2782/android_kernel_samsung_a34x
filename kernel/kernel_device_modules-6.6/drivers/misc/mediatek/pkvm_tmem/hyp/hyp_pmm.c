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

#include "hyp_pmm.h"

/*
 *  PMM_MSG_ENTRY format
 *  page number = PA >> PAGE_SHIFT
 *   _______________________________________
 *  |  reserved  | page order | page number |
 *  |____________|____________|_____________|
 *  31         28 27        24 23          0
 */
#define PMM_MSG_ORDER_SHIFT (24UL)
#define PMM_MSG_ENTRY(pa, page_order) \
	((pa >> PAGE_SHIFT) | (page_order << PMM_MSG_ORDER_SHIFT))
#define GET_PMM_ENTRY_PFN(entry) \
	(entry & ((1UL << PMM_MSG_ORDER_SHIFT) - 1))
#define GET_PMM_ENTRY_PA(entry) \
	((entry & ((1UL << PMM_MSG_ORDER_SHIFT) - 1)) << PAGE_SHIFT)
#define GET_PMM_ENTRY_ORDER(entry) ((entry >> PMM_MSG_ORDER_SHIFT) & 0Xf)
#define PMM_MSG_ENTRIES_PER_PAGE (PAGE_SIZE / sizeof(uint32_t))
#define LV2_PAGE_IDX(page) ((uint32_t)(page - lv2_page_array))
#define TOTAL_PAGES() (lv2_page_pool_size >> SHIFT_1KB)

int secure_pglist_common(uint64_t pglist_pfn,
							uint32_t attr,
							uint32_t count,
							bool enable,
							const struct pkvm_module_ops *tmem_ops)
{
	uint32_t entry, order, i;
	uint32_t *pmm_page;
	uint64_t rc;
	phys_addr_t pfn;
	void *pglist_pa = (void *)(pglist_pfn << ONE_PAGE_OFFSET);

//	tmem_ops->puts("pglist_pfn = ");
//	tmem_ops->putx64(pglist_pfn);
//	tmem_ops->puts("attr = ");
//	tmem_ops->putx64(attr);
//	tmem_ops->puts("count = ");
//	tmem_ops->putx64(count);

	rc = tmem_ops->host_share_hyp(pglist_pfn);
	if (rc != 0)
		tmem_ops->puts("pkvm_tmem parse fail\n");

	pmm_page = (uint32_t *)tmem_ops->hyp_va((phys_addr_t)pglist_pa);

	rc = tmem_ops->pin_shared_mem(((void *)tmem_ops->hyp_va((phys_addr_t)pglist_pa)),
			((void *)tmem_ops->hyp_va((phys_addr_t)pglist_pa)) + ONE_PAGE_SIZE);
	if (rc != 0)
		tmem_ops->puts("pkvm_tmem pin_shared_mem fail\n");

	for (i = 0; i < count; i++) {
		entry = pmm_page[i];
		order = GET_PMM_ENTRY_ORDER(entry);
		pfn = GET_PMM_ENTRY_PFN(entry);
//		tmem_ops->puts("order = ");
//		tmem_ops->putx64(order);
//		tmem_ops->puts("pfn = ");
//		tmem_ops->putx64(pfn);
		if (enable)
			rc = tmem_ops->host_donate_hyp(pfn, 1 << order, false);
		else
			rc = tmem_ops->hyp_donate_host(pfn, 1 << order);
		if (rc != 0) {
			tmem_ops->puts("page-based: failed to CPU EL1 Stage2 unmap/map\n");
			return -1;
		}
	}

	tmem_ops->unpin_shared_mem(((void *)tmem_ops->hyp_va((phys_addr_t)pglist_pa)),
			((void *)tmem_ops->hyp_va((phys_addr_t)pglist_pa) + ONE_PAGE_SIZE));
	tmem_ops->host_unshare_hyp(pglist_pfn);

	return 0;
}

int hyp_pmm_secure_pglist(uint64_t pglist_pfn,
							uint32_t attr,
							uint32_t count,
							const struct pkvm_module_ops *tmem_ops)
{
	tmem_ops->puts("pkvm_tmem: hyp_pmm_secure_pglist\n");
	secure_pglist_common(pglist_pfn, attr, count, true, tmem_ops);

	return 0;
}

int hyp_pmm_unsecure_pglist(uint64_t pglist_pfn,
							uint32_t attr,
							uint32_t count,
							const struct pkvm_module_ops *tmem_ops)
{
	tmem_ops->puts("pkvm_tmem: hyp_pmm_unsecure_pglist\n");
	secure_pglist_common(pglist_pfn, attr, count, false, tmem_ops);

	return 0;
}
