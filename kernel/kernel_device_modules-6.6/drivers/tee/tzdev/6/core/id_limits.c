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

#include <asm/current.h>
#include <linux/hashtable.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>

#include "core/id_limits.h"

#define LIMITS_HASH_BITS	6

struct id_limit {
	unsigned int cntr;
};

struct client_limits {
	struct task_struct *task;
	unsigned long cntr;
	struct id_limit limits[LIMIT_TYPE_NUM];
	struct hlist_node node;
};

static DEFINE_HASHTABLE(tzdev_client_hash, LIMITS_HASH_BITS);
static DEFINE_MUTEX(tzdev_client_hash_lock);

static struct client_limits *tzdev_find_client_limits(struct task_struct *task)
{
    struct client_limits *entry;

    hash_for_each_possible(tzdev_client_hash, entry, node, (unsigned long)task) {
        if (entry->task == task) {
            return entry;
        }
    }

    return NULL;
}

int tzdev_inc_id_cntr(enum limit_type limit_type, struct task_struct *task, tz_check_limit_cb_t tz_check_limit_cb)
{
	int ret;
	struct client_limits *client_limits;

	mutex_lock(&tzdev_client_hash_lock);

	client_limits = tzdev_find_client_limits(task);
	if (!client_limits) {
		client_limits = kzalloc(sizeof(struct client_limits), GFP_KERNEL);
		if (!client_limits) {
			ret = -ENOMEM;
			goto exit;
		}

		client_limits->task = task;

		hash_add(tzdev_client_hash, &client_limits->node, (unsigned long)task);

		get_task_struct(client_limits->task);
	}

	ret = tz_check_limit_cb(client_limits->limits[limit_type].cntr + 1);
	if (ret)
		goto exit;
	client_limits->limits[limit_type].cntr++;
	client_limits->cntr++;

	ret = 0;

exit:
	mutex_unlock(&tzdev_client_hash_lock);
	return ret;
}

void tzdev_dec_id_cntr(enum limit_type limit_type, struct task_struct *task)
{
	struct client_limits *client_limits;

	mutex_lock(&tzdev_client_hash_lock);

	client_limits = tzdev_find_client_limits(task);
	BUG_ON(!client_limits);

	BUG_ON(!client_limits->limits[limit_type].cntr);
	BUG_ON(!client_limits->cntr);

	client_limits->limits[limit_type].cntr--;
	client_limits->cntr--;
	if (client_limits->cntr == 0) {
		hash_del(&client_limits->node);
		put_task_struct(client_limits->task);
		kfree(client_limits);
	}

	mutex_unlock(&tzdev_client_hash_lock);
}

struct task_struct *tzdev_get_client_task(void)
{
	if (current->mm) {
		if (current->tgid == current->pid)
			return current;
		else if (current->real_parent->tgid == current->real_parent->pid)
			return current->real_parent;
		else
			BUG();
	}

	return current->real_parent;
}
