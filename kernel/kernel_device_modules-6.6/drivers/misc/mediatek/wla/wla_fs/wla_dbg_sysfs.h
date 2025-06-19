/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __WLA_DBG_SYSFS__
#define __WLA_DBG_SYSFS__
#include "wla_sysfs.h"
#include "wla_kernfs.h"

#define WLA_DBG_SYS_FS_MODE      0644

#undef wla_dbg_log
#define wla_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

struct WLA_DBG_NODE {
	const char *name;
	int type;
	struct wla_sysfs_handle handle;
	struct wla_sysfs_op op;
};

/*Get the mtk idle system fs root entry handle*/
int wla_dbg_sysfs_entry_root_get(struct wla_sysfs_handle **handle);

/*Creat the entry for mtk idle systme fs*/
int wla_dbg_sysfs_entry_group_add(const char *name,
		int mode, struct wla_sysfs_group *_group,
		struct wla_sysfs_handle *handle);

/*Add the child file node to mtk idle system*/
int wla_dbg_sysfs_entry_node_add(const char *name, int mode,
			const struct wla_sysfs_op *op,
			struct wla_sysfs_handle *node);

int wla_dbg_sysfs_entry_node_remove(
		struct wla_sysfs_handle *handle);

int wla_dbg_sysfs_root_entry_create(void);

int wla_dbg_sysfs_remove(void);

int wla_dbg_sysfs_sub_entry_add(const char *name, int mode,
					struct wla_sysfs_handle *parent,
					struct wla_sysfs_handle *handle);

int wla_dbg_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct wla_sysfs_op *op
		, struct wla_sysfs_handle *parent
		, struct wla_sysfs_handle *handle);
#endif
