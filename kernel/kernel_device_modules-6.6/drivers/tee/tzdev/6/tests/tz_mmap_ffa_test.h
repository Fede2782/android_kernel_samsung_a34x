/*
 * Copyright (C) 2023 Samsung Electronics, Inc.
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

#ifndef __TZ_MMAP_FFA_TEST_H__
#define __TZ_MMAP_FFA_TEST_H__

#include <linux/types.h>

#define TZ_MMAP_FFA_IOC_MAGIC		'f'

#define TZ_MMAP_GET_BUFFER_HANDLE	_IOR(TZ_MMAP_FFA_IOC_MAGIC, 0, uint64_t)
#define TZ_MMAP_CLOSE_BUFFER		_IOW(TZ_MMAP_FFA_IOC_MAGIC, 0, uint64_t)

#endif /*!__TZ_MMAP_FFA_TEST_H__*/