/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/string.h>

#include <ffa.h>

#include "tzdev_internal.h"
#include "tzdev_limits.h"
#include "core/cdev.h"
#include "core/log.h"
#include "debug/iw_boot_log.h"

#define TZ_BOOT_LOG_PREFIX		KERN_DEFAULT "SW_BOOT> "

static atomic_t tz_iw_boot_log_already_read = ATOMIC_INIT(0);

static void tz_iw_boot_log_print(char *buf, unsigned long nbytes)
{
	unsigned long printed;
	char *p;

	while (nbytes) {
		p = memchr(buf, '\n', nbytes);

		if (p) {
			*p = '\0';
			printed = p - buf + 1;
		} else {
			printed = nbytes;
		}

		printk(TZ_BOOT_LOG_PREFIX "%.*s\n", (int)printed, buf);

		nbytes -= printed;
		buf += printed;
	}
}

#ifdef CONFIG_TZDEV_FFA
static inline int share_mem(unsigned long *pfn, unsigned num)
{
	struct page *pages = pfn_to_page(*pfn);
	struct page **page;
	ffa_handle_t handle;
	unsigned int i;
	int ret;

	page = kmalloc_array(sizeof(struct page *), num, GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	for (i = 0; i < num; i++)
		page[i] = pages + i;

	ret = tzdev_ffa_mem_share(num, page, &handle);
	kfree(page);

	if (ret == 0)
		*pfn = FFA_HANDLE_TO_UINT(handle);

	return ret;
}

static inline void unshare_mem(unsigned long pfn)
{
	ffa_handle_t handle = FFA_ULONG_TO_HANDLE(pfn);

	tzdev_ffa_mem_reclaim(handle);
}
#else
static inline int share_mem(unsigned long *pfn, unsigned num)
{
	(void) pfn;
	(void) num;

	return 0;
}

static inline void unshare_mem(unsigned long pfn)
{
	(void) pfn;
}
#endif

void tz_iw_boot_log_read(void)
{
	struct page *pages;
	unsigned int order;
	unsigned long pfn;
	int nbytes;

	if (atomic_cmpxchg(&tz_iw_boot_log_already_read, 0, 1))
		return;

	order = order_base_2(CONFIG_TZ_BOOT_LOG_PG_CNT);

	pages = alloc_pages(GFP_KERNEL, order);
	if (!pages)
		return;

	pfn = page_to_pfn(pages);

	BUG_ON(pfn > U32_MAX);

	if (share_mem(&pfn, CONFIG_TZ_BOOT_LOG_PG_CNT))
		goto free_mem;

	nbytes = tzdev_smc_boot_log_read(page_to_phys(pages),
			CONFIG_TZ_BOOT_LOG_PG_CNT * PAGE_SIZE);
	if (nbytes > 0)
		tz_iw_boot_log_print(page_address(pages), nbytes);

free_mem:
	__free_pages(pages, order);
	unshare_mem(pfn);
}
