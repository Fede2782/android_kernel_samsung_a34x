// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "wla_dbg_sysfs.h"

#define WLA_DBG_SYS_FS_NAME	"wla_dbg"
#define WLA_DBG_SYS_FS_MODE	0644

static struct wla_sysfs_handle wla_dbg_fs_root;

int wla_dbg_sysfs_entry_node_add(const char *name,
		int mode, const struct wla_sysfs_op *op,
		struct wla_sysfs_handle *handle)
{
	if (!IS_WLA_SYS_HANDLE_VALID(&wla_dbg_fs_root))
		wla_dbg_sysfs_root_entry_create();

	return wla_sysfs_entry_func_node_add(name
			, mode, op, &wla_dbg_fs_root, handle);
}
EXPORT_SYMBOL(wla_dbg_sysfs_entry_node_add);

int wla_dbg_sysfs_entry_node_remove(
		struct wla_sysfs_handle *handle)
{
	return wla_sysfs_entry_func_node_remove(handle);
}
EXPORT_SYMBOL(wla_dbg_sysfs_entry_node_remove);

int wla_dbg_sysfs_root_entry_create(void)
{
	int bRet = 0;

	if (!IS_WLA_SYS_HANDLE_VALID(&wla_dbg_fs_root)) {
		bRet = wla_sysfs_entry_func_create(
			WLA_DBG_SYS_FS_NAME, WLA_DBG_SYS_FS_MODE
			, NULL, &wla_dbg_fs_root);
	}
	return bRet;
}
EXPORT_SYMBOL(wla_dbg_sysfs_root_entry_create);

int wla_dbg_sysfs_entry_root_get(struct wla_sysfs_handle **handle)
{
	if (!handle ||
		!IS_WLA_SYS_HANDLE_VALID(&wla_dbg_fs_root)
	)
		return -1;
	*handle = &wla_dbg_fs_root;
	return 0;
}
EXPORT_SYMBOL(wla_dbg_sysfs_entry_root_get);

int wla_dbg_sysfs_sub_entry_add(const char *name, int mode,
					struct wla_sysfs_handle *parent,
					struct wla_sysfs_handle *handle)
{
	int bRet = 0;
	struct wla_sysfs_handle *p;

	p = parent;

	if (!p) {
		if (!IS_WLA_SYS_HANDLE_VALID(&wla_dbg_fs_root)) {
			bRet = wla_dbg_sysfs_root_entry_create();
			if (bRet)
				return bRet;
		}
		p = &wla_dbg_fs_root;
	}

	bRet = wla_sysfs_entry_func_create(name, mode, p, handle);
	return bRet;
}
EXPORT_SYMBOL(wla_dbg_sysfs_sub_entry_add);

int wla_dbg_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct wla_sysfs_op *op
		, struct wla_sysfs_handle *parent
		, struct wla_sysfs_handle *handle)
{
	return wla_sysfs_entry_func_node_add(name
			, mode, op, parent, handle);
}
EXPORT_SYMBOL(wla_dbg_sysfs_sub_entry_node_add);

int wla_dbg_sysfs_remove(void)
{
	return wla_sysfs_entry_func_node_remove(&wla_dbg_fs_root);
}
EXPORT_SYMBOL(wla_dbg_sysfs_remove);
