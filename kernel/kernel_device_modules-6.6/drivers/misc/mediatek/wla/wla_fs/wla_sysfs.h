/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __WLA_SYSFS__
#define __WLA_SYSFS__
#include <linux/list.h>

#define WLA_SYSFS_HAS_ENTRY		(1)

#define WLA_SYSFS_ENTRY_NAME		"wla"
#define WLA_SYSFS_BUF_READSZ		12288
#define WLA_SYSFS_BUF_WRITESZ	512

typedef ssize_t (*f_wla_sysfs_show)(char *ToUserBuf
			, size_t sz, void *priv);
typedef ssize_t (*f_wla_sysfs_write)(char *FromUserBuf
			, size_t sz, void *priv);


#define WLA_SYSFS_TYPE_ENTRY		(1 << 0ul)
#define WLA_SYSFS_BUF_SZ		(1 << 1ul)
#define WLA_SYSFS_FREEZABLE		(1 << 2ul)

struct wla_sysfs_op {
	unsigned int buf_sz;
	f_wla_sysfs_show	fs_read;
	f_wla_sysfs_write	fs_write;
	void *priv;
};

struct wla_sysfs_handle {
	const char *name;
	unsigned int flag;
	void *_current;
	struct list_head dr;
	struct list_head np;
};

struct wla_sysfs_attr {
	char	*name;
	umode_t	mode;
	struct wla_sysfs_op sysfs_op;
};
struct wla_sysfs_group {
	struct wla_sysfs_attr **attrs;
	unsigned int attr_num;
};


#define __WLA_SYSFS_ATTR(_name, _mode, _read, _write, _priv) {\
	.name = __stringify(_name),\
	.mode = _mode,\
	.sysfs_op.fs_read = _read,\
	.sysfs_op.fs_write = _write,\
	.sysfs_op.priv = _priv,\
}

#define DEFINE_WLA_SYSFS_ATTR(_name, _mode, _read, _write, _priv)\
	struct wla_sysfs_attr wla_sysfs_attr_##_name =\
		__WLA_SYSFS_ATTR(_name, _mode, _read, _write, _priv)

#define WLA_SYSFS_ATTR_PTR(_name)	(&wla_sysfs_attr_##_name)



/* Macro for auto generate which used by internal */
#define AUTO_MACRO_WLA_SYSFS_ATTR_PTR(_mod, _name)\
	(&mtk_##_mod##_attrs_def.wla_sysfs_attr_##_name)

#define AUTO_MACRO_WLA_SYSFS_GROUP(_mod, _name, _mode, _read, _write, _priv)\
	AUTO_MACRO_WLA_SYSFS_ATTR_PTR(_mod, _name),

#define AUTO_MACRO_WLA_SYSFS_STRUCT_DECLARE(_mod, _name, _mode\
		, _read, _write, _priv)\
	struct wla_sysfs_attr wla_sysfs_attr_##_name

#define AUTO_MACRO_WLA_SYSFS_STRUCT_DEFINE(_mod, _name, _mode\
		, _read, _write, _priv)\
	.wla_sysfs_attr_##_name =\
		__WLA_SYSFS_ATTR(_name, _mode, _read, _write, _priv),

#define WLA_SYSFS_GROUP_PTR(_name)	(&wla_##_name##_group)

/* Macro for declare debug group sysfs */
#define DECLARE_WLA_SYSFS_GROUP(_name, _mode, _foreach) \
	struct _##_name##_attr_declare_section_ \
		_foreach(AUTO_MACRO_WLA_SYSFS_STRUCT_DECLARE, _name); \
	struct _##_name##_attr_declare_section_ mtk_##_name##_attrs_def =\
		_foreach(AUTO_MACRO_WLA_SYSFS_STRUCT_DEFINE, _name); \
	struct wla_sysfs_attr *wla_##_name##_group_attr[] =\
		_foreach(AUTO_MACRO_WLA_SYSFS_GROUP, _name);\
	struct wla_sysfs_group wla_##_name##_group = {\
		.attrs = wla_##_name##_group_attr,\
		.attr_num =\
			sizeof(struct _##_name##_attr_declare_section_)\
				/ sizeof(struct wla_sysfs_attr)\
	}

#define IS_WLA_SYS_HANDLE_VALID(x)\
	({ struct wla_sysfs_handle *Po = x;\
	if ((!Po) || (!Po->_current))\
		Po = NULL;\
	(Po); })

int wla_sysfs_entry_func_create(const char *name, int mode,
			struct wla_sysfs_handle *parent,
			struct wla_sysfs_handle *handle);

int wla_sysfs_entry_func_node_add(const char *name,
		int mode, const struct wla_sysfs_op *op,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *node);

int wla_sysfs_entry_func_node_remove(
		struct wla_sysfs_handle *node);

int wla_sysfs_entry_func_group_create(const char *name,
		int mode, struct wla_sysfs_group *_group,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle);
#endif
