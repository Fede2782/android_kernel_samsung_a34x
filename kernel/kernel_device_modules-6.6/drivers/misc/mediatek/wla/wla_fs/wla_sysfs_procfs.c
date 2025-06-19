// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "wla_sysfs.h"


#define WLA_PROC_ROOT_NAME	WLA_SYSFS_ENTRY_NAME
#define WLA_PROC_ROOT_MODE	0644

#if WLA_SYSFS_HAS_ENTRY
static struct wla_sysfs_handle wla_proc_root;
#endif

static int wla_sys_proc_offset;
void *wla_sysfs_procfs_seq_start(struct seq_file *sf, loff_t *ppos)
{
	/* return null and seq_next will stop */
	void *bRet = NULL;

	if (*ppos == 0) {
		wla_sys_proc_offset = 0;
		bRet = (void *)(&wla_sys_proc_offset);
	}
	return bRet;
}

void *wla_sysfs_procfs_seq_next(struct seq_file *sf
			, void *v, loff_t *ppos)
{
	++*ppos;
	return NULL;
}

static int wla_sysfs_procfs_seq_show(struct seq_file *sf, void *v)
{
	size_t out_sz = 0, buf_sz = 0;
	char *buf;

	buf_sz = seq_get_buf(sf, &buf);

	/* acquire buffer size is larger enough */
	if (buf_sz < WLA_SYSFS_BUF_READSZ)
		seq_commit(sf, -1);
	else {
		struct wla_sysfs_op *pOp;

		pOp = sf->private ?: sf->private;
		memset(buf, 0, buf_sz);
		if (pOp && pOp->fs_read)
			out_sz = pOp->fs_read(buf, buf_sz - 1, pOp->priv);
		buf[buf_sz - 1] = '\0';
		seq_commit(sf, out_sz);
		pr_info("[%s:%d] buf_sz=%lu\n", __func__, __LINE__, buf_sz);
	}

	return 0;
}

void wla_sysfs_procfs_seq_stop(struct seq_file *sf, void *v)
{
}

static const struct seq_operations wla_sysfs_procfs_seq_ops = {
	.start = wla_sysfs_procfs_seq_start,
	.next = wla_sysfs_procfs_seq_next,
	.stop = wla_sysfs_procfs_seq_stop,
	.show = wla_sysfs_procfs_seq_show,
};

static int wla_sysfs_procfs_open(
	struct inode *inode, struct file *filp)
{
	int error = -EACCES;

	error = seq_open(filp, &wla_sysfs_procfs_seq_ops);

	if (error == 0) {
		((struct seq_file *)filp->private_data)->private
			= pde_data(inode);
	}
	return 0;
}
static int wla_sysfs_procfs_close(
	struct inode *inode, struct file *filp)
{
	return seq_release(inode, filp);
}

static ssize_t wla_sysfs_procfs_read(struct file *filp,
				char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	return seq_read(filp, userbuf, count, f_pos);
}

static ssize_t wla_sysfs_procfs_write(struct file *filp,
					const char __user *userbuf,
					size_t count, loff_t *f_pos)
{
	char BufFromUser[WLA_SYSFS_BUF_WRITESZ];
	ssize_t bSz = -EINVAL;

	struct wla_sysfs_op *pOp = (struct wla_sysfs_op *)
			((struct seq_file *)filp->private_data)->private;

	if (count >= WLA_SYSFS_BUF_WRITESZ) {
		pr_info("[name:wlam][P] - over WLA_SYSFS_BUF_WRITESZ error (%s:%d)\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	count = min(count, sizeof(BufFromUser));

	memset(&BufFromUser[0], 0, sizeof(BufFromUser));
	if (copy_from_user(&BufFromUser[0], userbuf, count))
		return -EFAULT;

	if (pOp && pOp->fs_write)
		bSz = pOp->fs_write(&BufFromUser[0], count, pOp->priv);
	return bSz;
}

static const struct proc_ops wlasysfs_proc_op = {
	.proc_open = wla_sysfs_procfs_open,
	.proc_read = wla_sysfs_procfs_read,
	.proc_write = wla_sysfs_procfs_write,
	.proc_lseek = seq_lseek,
	.proc_release = wla_sysfs_procfs_close,
};

#if WLA_SYSFS_HAS_ENTRY
int wla_proc_root_get(void)
{
	struct proc_dir_entry *p = NULL;
	int ret = 0;

	if (IS_WLA_SYS_HANDLE_VALID(&wla_proc_root))
		return ret;

	p = proc_mkdir(WLA_PROC_ROOT_NAME, NULL);

	if (p)
		wla_proc_root._current = (void *)p;
	else
		ret = -EPERM;
	return ret;
}

int wla_proc_root_put(void)
{
	if (!IS_WLA_SYS_HANDLE_VALID(&wla_proc_root)) {
		proc_remove((struct proc_dir_entry *)
			     wla_proc_root._current);
		wla_proc_root._current = NULL;
	}
	return 0;
}
#endif

int wla_sysfs_entry_create_plat(const char *name,
		int mode, struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	struct proc_dir_entry *pHandle = NULL;
	int bRet = 0;
	(void)mode;

	if (!handle)
		return -EINVAL;

#if WLA_SYSFS_HAS_ENTRY
	bRet = wla_proc_root_get();
#endif
	if (!bRet) {
		if (parent && parent->_current)
			pHandle = (struct proc_dir_entry *)parent->_current;
#if WLA_SYSFS_HAS_ENTRY
		else
			pHandle = (struct proc_dir_entry *)
				  wla_proc_root._current;
#endif

		if (pHandle)
			handle->_current =
				(void *)proc_mkdir(name, pHandle);
		else
			handle->_current =
				(void *)proc_mkdir(name, NULL);

		if (!handle->_current)
			bRet = -EPERM;
	}
	return bRet;
}

int wla_sysfs_entry_node_add_plat(const char *name,
		int mode, const struct wla_sysfs_op *op,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *node)
{
	struct proc_dir_entry *c = NULL;
	struct proc_dir_entry *pHandle = NULL;
	int bRet = 0;

	if (!parent || !parent->_current) {
#if WLA_SYSFS_HAS_ENTRY
		if (!wla_proc_root_get()) {
			pHandle = (struct proc_dir_entry *)
				  wla_proc_root._current;
		} else
			return -EINVAL;
#else
		return -EINVAL;
#endif
	} else
		pHandle =
			(struct proc_dir_entry *)parent->_current;

	c = proc_create_data(name, mode, pHandle,
			     (void *)&wlasysfs_proc_op,
			     (void *)op);

	if (node)
		node->_current = (void *)c;
	return bRet;
}

int wla_sysfs_entry_node_remove_plat(
		struct wla_sysfs_handle *node)
{
	int bRet = 0;

	if (!node)
		return -EINVAL;
	pr_info("FS remove %s\n", node->name);
	return bRet;
}

int wla_sysfs_entry_group_create_plat(const char *name,
		int mode, struct wla_sysfs_group *_group,
		struct wla_sysfs_handle *parent,
		struct wla_sysfs_handle *handle)
{
	int bRet = 0;
	int idx = 0;
	struct wla_sysfs_handle Grouper;
	struct wla_sysfs_handle *pGrouper = &Grouper;

	if (handle)
		pGrouper = handle;

	wla_sysfs_entry_create_plat(name, mode, parent, pGrouper);

	if (_group && IS_WLA_SYS_HANDLE_VALID(pGrouper)) {
		for (idx = 0;; ++idx) {
			if ((_group->attrs[idx] == NULL) ||
				(idx >= _group->attr_num))
				break;

			wla_sysfs_entry_node_add_plat(
				_group->attrs[idx]->name
				, _group->attrs[idx]->mode
				, &_group->attrs[idx]->sysfs_op
				, pGrouper
				, NULL);
		}
	}

	return bRet;
}
