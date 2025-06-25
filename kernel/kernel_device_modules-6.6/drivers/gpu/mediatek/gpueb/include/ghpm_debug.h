/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GHPM_DEBUG_H__
#define __GHPM_DEBUG_H__

#include <linux/platform_device.h>

#if defined(CONFIG_PROC_FS)
#define PROC_FOPS_RW(name)            \
	static int name ## _proc_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _proc_show,   \
				pde_data(inode));     \
	}                                 \
	static const struct proc_ops name ## _proc_fops = \
	{                                 \
		.proc_open = name ## _proc_open,   \
		.proc_read = seq_read,             \
		.proc_lseek = seq_lseek,           \
		.proc_release = single_release,    \
		.proc_write = name ## _proc_write, \
	}

#define PROC_FOPS_RO(name)            \
	static int name ## _proc_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _proc_show,   \
				pde_data(inode));     \
	}                                 \
	static const struct proc_ops name ## _proc_fops = \
	{                                 \
		.proc_open = name ## _proc_open,   \
		.proc_read = seq_read,             \
		.proc_lseek = seq_lseek,           \
		.proc_release = single_release,    \
	}

#define PROC_ENTRY(name)              \
	{                                 \
		__stringify(name),            \
		&name ## _proc_fops           \
	}
#endif /* CONFIG_PROC_FS */

enum mfg0_a0_state {
	MFG0_AO_DISABLE,
	MFG0_AO_ENABLE
};

extern int g_ghpm_profile_enable;

void ghpm_debug_init(struct platform_device *pdev);

#endif /* __GHPM_DEBUG_H__ */
