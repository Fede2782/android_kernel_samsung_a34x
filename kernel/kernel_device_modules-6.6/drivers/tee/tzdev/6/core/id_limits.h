/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd All Rights Reserved
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

#pragma once

#include <linux/types.h>

enum limit_type {
	LIMIT_TYPE_IW_SMEM = 0,
	LIMIT_TYPE_IW_SOCK,
	LIMIT_TYPE_NUM
};

typedef int (*tz_check_limit_cb_t)(unsigned int cntr);

struct task_struct *tzdev_get_client_task(void);
int tzdev_inc_id_cntr(enum limit_type, struct task_struct *, tz_check_limit_cb_t);
void tzdev_dec_id_cntr(enum limit_type, struct task_struct *);