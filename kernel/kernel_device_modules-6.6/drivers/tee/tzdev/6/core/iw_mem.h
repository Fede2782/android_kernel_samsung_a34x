/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
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

#pragma once

#include <linux/types.h>

struct tzdev_iw_mem;
struct tzdev_smc_channel;

struct tzdev_iw_mem *tzdev_iw_mem_create(size_t size);
struct tzdev_iw_mem *tzdev_iw_mem_create_exist(void *ptr, size_t size);
void tzdev_iw_mem_destroy(struct tzdev_iw_mem *mem);

void *tzdev_iw_mem_map(struct tzdev_iw_mem *mem);
void tzdev_iw_mem_unmap(struct tzdev_iw_mem *mem);

int tzdev_iw_mem_map_user(struct tzdev_iw_mem *mem, struct vm_area_struct *vma);

struct page **tzdev_iw_mem_get_pages(struct tzdev_iw_mem *mem);
void *tzdev_iw_mem_get_map_address(struct tzdev_iw_mem *mem);
size_t tzdev_iw_mem_get_size(struct tzdev_iw_mem *mem);

int tzdev_iw_mem_pack(struct tzdev_iw_mem *mem, struct tzdev_smc_channel *channel);
void tz_iw_mem_init(void);

typedef int (*tzdev_smc_channel_preconnect_cb)(struct tzdev_iw_mem *buf, unsigned long num_pages, void *ext_data);
struct tzdev_iw_mem *tzdev_iwio_channel_connect(int mode, size_t size, tzdev_smc_channel_preconnect_cb cb, void *data);
