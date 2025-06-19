// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "wla_sysfs.h"

DEFINE_MUTEX(wla_sysfs_locker);
static LIST_HEAD(wla_sysfs_parent);

int __weak wla_sysfs_entry_create_plat(const char *name,
		int mode, struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	return 0;
}
int __weak wla_sysfs_entry_node_add_plat(const char *name,
		int mode, const struct wla_sysfs_op *op,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	return 0;
}

int __weak wla_sysfs_entry_node_remove_plat(
		struct wla_sysfs_handle *node)
{
	return 0;
}

int __weak wla_sysfs_entry_group_create_plat(const char *name,
		int mode, struct wla_sysfs_group *_group,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	return 0;
}

static
int __wla_sysfs_handle_add(struct wla_sysfs_handle *parent,
				      struct wla_sysfs_handle *node)
{
	if (!node)
		return -EINVAL;

	INIT_LIST_HEAD(&node->dr);
	INIT_LIST_HEAD(&node->np);

	if (parent)
		list_add(&node->np, &parent->dr);
	else
		list_add(&node->np, &wla_sysfs_parent);
	return 0;
}

int wla_sysfs_entry_func_create(const char *name,
		int mode, struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	int bRet;
	struct wla_sysfs_handle *p = NULL;

	mutex_lock(&wla_sysfs_locker);

	do {
		if (!handle) {
			p = kzalloc(sizeof(*p), GFP_KERNEL);
			if (!p) {
				bRet = -ENOMEM;
				break;
			}
			p->flag |= WLA_SYSFS_FREEZABLE;
		} else
			p = handle;

		bRet = wla_sysfs_entry_create_plat(name,
					mode, parent, p);

		if (!bRet) {
			p->flag |= WLA_SYSFS_TYPE_ENTRY;
			p->name = name;
			__wla_sysfs_handle_add(parent, p);
		} else
			kfree(p);
	} while (0);
	mutex_unlock(&wla_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(wla_sysfs_entry_func_create);

int wla_sysfs_entry_func_node_add(const char *name,
		int mode, const struct wla_sysfs_op *op,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *node)
{
	int bRet;
	struct wla_sysfs_handle *p = NULL;

	mutex_lock(&wla_sysfs_locker);

	do {
		if (!node) {
			p = kzalloc(sizeof(*p), GFP_KERNEL);
			if (!p) {
				bRet = -ENOMEM;
				break;
			}
			p->flag |= WLA_SYSFS_FREEZABLE;
		} else
			p = node;

		bRet = wla_sysfs_entry_node_add_plat(name,
					mode, op, parent, p);
		if (!bRet) {
			p->flag &= ~WLA_SYSFS_TYPE_ENTRY;
			p->name = name;
			__wla_sysfs_handle_add(parent, p);
		} else
			kfree(p);
	} while (0);

	mutex_unlock(&wla_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(wla_sysfs_entry_func_node_add);

static
int __wla_sysfs_handle_remove(struct wla_sysfs_handle *node)
{
	int ret = 0;

	if (!node)
		return -EINVAL;

	ret = wla_sysfs_entry_node_remove_plat(node);

	if (!ret) {
		list_del(&node->np);
		INIT_LIST_HEAD(&node->np);
		if (node->flag & WLA_SYSFS_FREEZABLE)
			kfree(node);
	}
	return ret;
}

static
int __wla_sysfs_entry_rm(struct wla_sysfs_handle *node)
{
	int bret = 0;
	struct wla_sysfs_handle *n;
	struct wla_sysfs_handle *cur;

	cur = list_first_entry(&node->dr, struct wla_sysfs_handle, np);
	do {
		if (list_is_last(&cur->np, &node->dr))
			n = NULL;
		else
			n = list_next_entry(cur, np);

		if (cur->flag & WLA_SYSFS_TYPE_ENTRY)
			__wla_sysfs_entry_rm(cur);
		__wla_sysfs_handle_remove(cur);
		cur = n;
	} while (cur);

	INIT_LIST_HEAD(&node->dr);
	return bret;
}

int wla_sysfs_entry_func_node_remove(
		struct wla_sysfs_handle *node)
{
	int bRet = 0;

	if (!node)
		return -EINVAL;

	mutex_lock(&wla_sysfs_locker);
	if (node->flag & WLA_SYSFS_TYPE_ENTRY)
		__wla_sysfs_entry_rm(node);
	__wla_sysfs_handle_remove(node);
	mutex_unlock(&wla_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(wla_sysfs_entry_func_node_remove);

int wla_sysfs_entry_func_group_create(const char *name,
		int mode, struct wla_sysfs_group *_group,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	int bRet;

	mutex_lock(&wla_sysfs_locker);
	bRet = wla_sysfs_entry_group_create_plat(name
			, mode, _group, parent, handle);
	mutex_unlock(&wla_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(wla_sysfs_entry_func_group_create);
