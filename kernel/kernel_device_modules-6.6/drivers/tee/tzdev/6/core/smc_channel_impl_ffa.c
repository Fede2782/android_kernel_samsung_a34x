/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <linux/mm.h>

#include "core/smc_channel_impl.h"
#include <ffa.h>

#include "tzdev_internal.h"

struct tzdev_smc_channel_metadata {
	uint32_t write_offset;
	uint32_t pfns_count;
	uint32_t num_handles;
	uint32_t reserved;
	ffa_handle_t handle[];
} __packed;

static int init(struct tzdev_smc_channel_metadata *metadata, struct page **pages,
		size_t pages_count, void *impl_data)
{
	int ret;

	ret = tzdev_ffa_mem_share(pages_count, pages, &metadata->handle[0]);
	if (ret)
		return ret;

	metadata->pfns_count = pages_count;
	metadata->num_handles = 1;

	memcpy(impl_data, &metadata->handle, sizeof(ffa_handle_t));

	return 0;
}

static int init_swd(struct page *meta_pages)
{
	ffa_handle_t handle;
	struct page *pages[NR_CPUS];
	unsigned int i;
	int ret;

	for (i = 0; i < NR_SW_CPU_IDS; i++)
		pages[i] = meta_pages + i;

	ret = tzdev_ffa_mem_share(NR_SW_CPU_IDS, pages, &handle);
	if (ret)
		return -1;

	ret = tzdev_smc_channels_init(FFA_HANDLE_TO_UINT(handle), NR_SW_CPU_IDS, PAGE_SIZE);
	if (ret)
		tzdev_ffa_mem_reclaim(handle);

	return ret;
}

static int deinit(void *impl_data)
{
	return tzdev_ffa_mem_reclaim(*(ffa_handle_t *)impl_data);
}

static void acquire(struct tzdev_smc_channel_metadata *metadata)
{
	metadata->write_offset = 0;
	metadata->pfns_count = 0;
}

static int reserve(struct tzdev_smc_channel_metadata *metadata, struct page **pages,
		size_t old_pages_count, size_t new_pages_count, void *impl_data)
{
	ffa_handle_t handle;
	int ret;

	(void)impl_data;

	ret = tzdev_ffa_mem_share(new_pages_count - old_pages_count,
			pages + old_pages_count, &handle);
	if (ret)
		return ret;

	memcpy(&metadata->handle[metadata->num_handles++], &handle, sizeof(handle));

	metadata->pfns_count += new_pages_count - old_pages_count;

	return 0;
}

static int release(struct tzdev_smc_channel_metadata *metadata, void *impl_data)
{
	int i;

	(void)impl_data;

	for (i = 1; i < metadata->num_handles; i++)
		tzdev_ffa_mem_reclaim(metadata->handle[i]);

	metadata->pfns_count = 0;
	metadata->num_handles = 1;

	return 0;
}

static void set_write_offset(struct tzdev_smc_channel_metadata *metadata, uint32_t value)
{
	metadata->write_offset = value;
}

static uint32_t get_write_offset(struct tzdev_smc_channel_metadata *metadata)
{
	return metadata->write_offset;
}

static struct tzdev_smc_channel_impl tzdev_smc_channel_impl = {
	.data_size = sizeof(ffa_handle_t),
	.init = init,
	.init_swd = init_swd,
	.deinit = deinit,
	.acquire = acquire,
	.reserve = reserve,
	.release = release,
	.set_write_offset = set_write_offset,
	.get_write_offset = get_write_offset,
};

struct tzdev_smc_channel_impl *tzdev_get_smc_channel_impl(void)
{
	return &tzdev_smc_channel_impl;
}
