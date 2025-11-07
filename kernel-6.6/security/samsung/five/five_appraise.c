// SPDX-License-Identifier: GPL-2.0
/*
 * This code is based on IMA's code
 *
 * Copyright (C) 2016 Samsung Electronics, Inc.
 *
 * Egor Uleyskiy, <e.uleyskiy@samsung.com>
 * Viacheslav Vovchenko <v.vovchenko@samsung.com>
 * Yevgen Kopylov <y.kopylov@samsung.com>
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

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <crypto/hash_info.h>

#include <linux/task_integrity.h>
#include "five.h"
#include "five_audit.h"
#include "five_hooks.h"
#include "five_tee_api.h"
#include "five_porting.h"
#include "five_cache.h"
#include "five_dmverity.h"
#include "five_log.h"
#include "five_iint.h"

int five_read_xattr(struct dentry *dentry, char **xattr_value)
{
	ssize_t ret;

	ret = vfs_getxattr_alloc(dentry, XATTR_NAME_FIVE, xattr_value,
			0, GFP_NOFS);
	if (ret < 0)
		ret = 0;

	return ret;
}

static bool bad_fs(struct inode *inode)
{
	if (inode->i_sb->s_magic == EXT4_SUPER_MAGIC ||
	    inode->i_sb->s_magic == F2FS_SUPER_MAGIC ||
	    inode->i_sb->s_magic == OVERLAYFS_SUPER_MAGIC ||
	    inode->i_sb->s_magic == EROFS_SUPER_MAGIC_V1)
		return false;

	return true;
}

/*
 * five_is_fsverity_protected - checks if file is protected by FSVERITY
 *
 * Return true/false
 */
static bool five_is_fsverity_protected(const struct inode *inode)
{
	return IS_VERITY(inode);
}

/*
 * five_appraise_measurement - appraise file measurement
 *
 * Return 0 on success, error code otherwise
 */
int five_appraise_measurement(struct task_struct *task, int func,
			      struct five_iint_cache *iint,
			      struct file *file,
			      struct five_cert *cert)
{
	enum task_integrity_reset_cause cause = CAUSE_UNKNOWN;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	enum five_file_integrity status = FIVE_FILE_UNKNOWN;
	enum task_integrity_value prev_integrity;
	int rc = 0;

	FIVE_BUG_ON(!task || !iint || !file);

	prev_integrity = task_integrity_read(TASK_INTEGRITY(task));
	dentry = file->f_path.dentry;
	inode = d_backing_inode(dentry);

	if (bad_fs(inode)) {
		status = FIVE_FILE_FAIL;
		cause = CAUSE_BAD_FS;
		rc = -EOPNOTSUPP;
		goto out;
	}

	cause = CAUSE_NO_CERT;
	if (five_is_fsverity_protected(inode))
		status = FIVE_FILE_FSVERITY;
	else if (five_is_dmverity_protected(file))
		status = FIVE_FILE_DMVERITY;

out:
	if (status == FIVE_FILE_FAIL || status == FIVE_FILE_UNKNOWN) {
		task_integrity_set_reset_reason(TASK_INTEGRITY(task),
						cause, file);
		five_audit_verbose(task, file, five_get_string_fn(func),
				prev_integrity, prev_integrity,
				tint_reset_cause_to_string(cause), rc);
	}

	five_set_cache_status(iint, status);

	return rc;
}

static void five_reset_appraise_flags(struct dentry *dentry)
{
	struct inode *inode = d_backing_inode(dentry);
	struct five_iint_cache *iint;

	if (!S_ISREG(inode->i_mode))
		return;

	iint = five_iint_find(inode);
	if (iint)
		five_set_cache_status(iint, FIVE_FILE_UNKNOWN);
}

/**
 * five_inode_post_setattr - reflect file metadata changes
 * @dentry: pointer to the affected dentry
 *
 * Changes to a dentry's metadata might result in needing to appraise.
 *
 * This function is called from notify_change(), which expects the caller
 * to lock the inode's i_mutex.
 */
void five_inode_post_setattr(struct task_struct *task, struct dentry *dentry)
{
	five_reset_appraise_flags(dentry);
}

/*
 * five_protect_xattr - protect 'security.five'
 *
 * Ensure that not just anyone can modify or remove 'security.five'.
 */
static int five_protect_xattr(struct dentry *dentry, const char *xattr_name,
			     const void *xattr_value, size_t xattr_value_len)
{
	if (strcmp(xattr_name, XATTR_NAME_FIVE) == 0) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		return 1;
	}
	return 0;
}

int five_inode_setxattr(struct dentry *dentry, const char *xattr_name,
			const void *xattr_value, size_t xattr_value_len)
{
	int result = five_protect_xattr(dentry, xattr_name, xattr_value,
				   xattr_value_len);

	if (result == 1 && xattr_value_len == 0) {
		five_reset_appraise_flags(dentry);
		return 0;
	}

	if (result == 1) {
		bool digsig;
		struct five_cert_header *header;
		struct five_cert cert = { {0} };

		result = five_cert_fillout(&cert, xattr_value, xattr_value_len);
		if (result)
			return result;

		header = (struct five_cert_header *)cert.body.header->value;

		if (!xattr_value_len || !header ||
				(header->signature_type >= FIVE_XATTR_END))
			return -EINVAL;

		digsig = (header->signature_type == FIVE_XATTR_DIGSIG);
		if (!digsig)
			return -EPERM;

		five_reset_appraise_flags(dentry);
		result = 0;
	}

	return result;
}

int five_inode_removexattr(struct dentry *dentry, const char *xattr_name)
{
	int result;

	result = five_protect_xattr(dentry, xattr_name, NULL, 0);
	if (result == 1) {
		five_reset_appraise_flags(dentry);
		result = 0;
	}
	return result;
}

/* Called from do_fcntl */
int five_fcntl_sign(struct file *file, struct integrity_label __user *label)
{
	return -EOPNOTSUPP;
}

int five_fcntl_edit(struct file *file)
{
	return -EOPNOTSUPP;
}

int five_fcntl_close(struct file *file)
{
	return -EOPNOTSUPP;
}
