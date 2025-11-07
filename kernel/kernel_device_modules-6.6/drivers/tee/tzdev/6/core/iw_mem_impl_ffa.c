/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "tzdev_internal.h"
#include "core/ffa_shmem.h"
#include "core/iw_mem_impl.h"
#include "core/log.h"
#include <ffa.h>

static int init(struct page **pages, size_t pages_count, void *impl_data, int optimize)
{
	(void)optimize;
	ffa_handle_t *handle = impl_data;
	int ret;

	FFA_INIT_HANDLE(*handle);

	ret = tzdev_ffa_mem_share(pages_count, pages, handle);
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to share %ld pages\n", pages_count);
		return ret;
	}

	return ret;
}

static int deinit(void *impl_data)
{
	ffa_handle_t *handle = impl_data;

	tzdev_ffa_mem_reclaim(*handle);

	return 0;
}

static int pack(struct tzdev_smc_channel *channel, struct page **pages, size_t pages_count,
		void *impl_data)
{
	ffa_handle_t *handle = impl_data;
	uint32_t num_pages = (uint32_t)pages_count;
	int ret;

	ret = tzdev_smc_channel_reserve(channel, sizeof(uint32_t) + sizeof(ffa_handle_t));
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to reserve %zu bytes (error %d)\n",
				 sizeof(uint32_t) + sizeof(ffa_handle_t), ret);
		goto reclaim;
	}

	ret = tzdev_smc_channel_write(channel, &num_pages, sizeof(num_pages));
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to write num pages (error %d)\n", ret);
		goto reclaim;
	}

	ret = tzdev_smc_channel_write(channel, handle, sizeof(*handle));
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to write handle (error %d)\n", ret);
		goto reclaim;
	}

	return 0;

reclaim:
	tzdev_ffa_mem_reclaim(*handle);
	return ret;
}

static const struct tzdev_iw_mem_impl impl = {
	.data_size = sizeof(ffa_handle_t),
	.init = init,
	.pack = pack,
	.deinit = deinit,
};

const struct tzdev_iw_mem_impl *tzdev_get_iw_mem_impl(void)
{
	return &impl;
}
