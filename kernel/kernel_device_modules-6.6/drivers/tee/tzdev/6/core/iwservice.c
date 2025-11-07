/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <linux/errno.h>
#include <linux/types.h>

#include "tzdev_internal.h"
#include "core/iw_mem.h"
#include "core/iwio.h"
#include "core/log.h"
#include "core/smc_channel.h"

struct iw_service_channel {
	uint32_t cpu_mask;
	uint32_t user_cpu_mask;
	uint32_t cpu_freq[NR_CPUS];
} __attribute__((__packed__));

static struct tzdev_iw_mem *iw_channel;

static inline struct iw_service_channel *iw_channel_get_iwd_buffer(void)
{
	return tzdev_iw_mem_get_map_address(iw_channel);
}

unsigned long tz_iwservice_get_cpu_mask(void)
{
	if (!iw_channel || IS_ERR(iw_channel))
		return 0;

	return iw_channel_get_iwd_buffer()->cpu_mask;
}

unsigned long tz_iwservice_get_user_cpu_mask(void)
{
	if (!iw_channel || IS_ERR(iw_channel))
		return 0;

	return iw_channel_get_iwd_buffer()->user_cpu_mask;
}

void tz_iwservice_set_cpu_freq(unsigned int cpu, unsigned int cpu_freq)
{
	if (!iw_channel || IS_ERR(iw_channel))
		return;

	iw_channel_get_iwd_buffer()->cpu_freq[cpu] = cpu_freq;
}

int tz_iwservice_init(void)
{
	iw_channel = tzdev_iwio_channel_connect(TZ_IWIO_CONNECT_SERVICE, PAGE_SIZE, NULL, NULL);
	if (IS_ERR(iw_channel)) {
		log_error(tzdev_iwservice, "Failed to map iw memory (error %ld)", PTR_ERR(iw_channel));
		return PTR_ERR(iw_channel);
	}

	log_info(tzdev_iwservice, "IW service initialization done.\n");

	return 0;
}
