// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "mtk_drm_dp_debug.h"
#include "mtk_drm_dp.h"
#include "mtk_drm_dp_api.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif

extern struct mtk_dp *g_mtk_dp;
static bool g_dptx_log;

void mtk_dp_debug_enable(bool enable)
{
	g_dptx_log = enable;
}

bool mtk_dp_debug_get(void)
{
	return g_dptx_log;
}

void mtk_dp_debug(const char *opt)
{
	DP_FUNC("[debug]: %s\n", opt);

	if (strncmp(opt, "fec:", 4) == 0) {
		if (strncmp(opt + 4, "enable", 6) == 0)
			mtk_dp_fec_enable(g_mtk_dp, 1);
		else if (strncmp(opt + 4, "disable", 7) == 0)
			mtk_dp_fec_enable(g_mtk_dp, 1);
		else
			DP_MSG("fec:enable/disable error msg\n");
	} else if (strncmp(opt, "debug_log:", 10) == 0) {
		if (strncmp(opt + 10, "on", 2) == 0)
			mtk_dp_debug_enable(true);
		else if (strncmp(opt + 10, "off", 3) == 0)
			mtk_dp_debug_enable(false);
	}
}

#ifdef MTK_DPINFO
#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *mtkdp_dbgfs;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
static struct proc_dir_entry *mtkdp_procfs;
#endif

struct mtk_dp_debug_info {
	char *name;
	u8 index;
};

enum mtk_dp_debug_index {
	DP_INFO_HDCP      = 0,
	DP_INFO_PHY       = 1,
	DP_INFO_MAX
};

static struct mtk_dp_debug_info dp_info[DP_INFO_MAX] = {
	{"HDCP", DP_INFO_HDCP},
	{"PHY", DP_INFO_PHY},
};

static u8 info_index = DP_INFO_HDCP;

static int mtk_dp_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t mtk_dp_debug_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	int ret = 0;
	char *buffer;

	buffer = kmalloc(PAGE_SIZE / 8, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	switch (info_index) {
	case DP_INFO_PHY:
		ret = mtk_dp_phy_get_info(buffer, PAGE_SIZE / 8);
		break;
	default:
		DP_ERR("Invalid inedx!");
	}

	if (ret > 0)
		ret = simple_read_from_buffer(ubuf, count, ppos, buffer, ret);

	kfree(buffer);
	return ret;
}

static void mtk_dp_process_dbg_opt(const char *opt)
{
	int i = 0;

	for (i = 0; i < DP_INFO_MAX; i++) {
		if (!strncmp(opt, dp_info[i].name, strlen(dp_info[i].name))) {
			info_index = dp_info[i].index;
			break;
		}
	}

	if (info_index == DP_INFO_MAX)
		info_index = DP_INFO_HDCP;
}

static ssize_t mtk_dp_debug_write(struct file *file, const char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	const int debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512];
	char *tok;
	char *cmd = cmd_buffer;

	ret = count;
	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count) != 0ULL)
		return -EFAULT;

	cmd_buffer[count] = '\0';

	DP_MSG("[mtkdp_dbg]%s!\n", cmd_buffer);
	while ((tok = strsep(&cmd, " ")) != NULL)
		mtk_dp_process_dbg_opt(tok);

	return ret;
}

static const struct file_operations dp_debug_fops = {
	.read = mtk_dp_debug_read,
	.write = mtk_dp_debug_write,
	.open = mtk_dp_debug_open,
};

static const struct proc_ops dp_debug_proc_fops = {
	.proc_read = mtk_dp_debug_read,
	.proc_write = mtk_dp_debug_write,
	.proc_open = mtk_dp_debug_open,
};

int mtk_dp_debugfs_init(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	mtkdp_dbgfs = debugfs_create_file("mtk_dpinfo", 0644,
					  NULL, NULL, &dp_debug_fops);
	if (IS_ERR_OR_NULL(mtkdp_dbgfs))
		return -ENOMEM;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	mtkdp_procfs = proc_create("mtk_dpinfo", 0644, NULL,
				   &dp_debug_proc_fops);
	if (IS_ERR_OR_NULL(mtkdp_procfs))
		return -ENOMEM;
#endif
	return 0;
}

void mtk_dp_debugfs_deinit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(mtkdp_dbgfs);
	mtkdp_dbgfs = NULL;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	if (mtkdp_procfs) {
		proc_remove(mtkdp_procfs);
		mtkdp_procfs = NULL;
	}
#endif
}
#endif

