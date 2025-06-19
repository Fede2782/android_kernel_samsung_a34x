// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */


#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/sysfs.h>

#include "adsp_dbg.h"
#include "adsp_clk.h"
#include "adsp_platform_driver.h"

static const struct cmd_fn common_cmds[];
static char last_cmd[128] = "null";

static void dump_debug_cmds(struct seq_file *s, void *data)
{
	const struct cmd_fn *cf;

	seq_puts(s, "adsp ap support commands:\n");
	for (cf = common_cmds; cf->cmd; cf++)
		seq_printf(s, "%s\n", cf->cmd);
}

static void set_adsp_ao(struct seq_file *s, void *data)
{
	int val = *(int *)data;

	if (val == 1)
		adsp_enable_pd();
	else if (val == 0)
		adsp_disable_pd();
	else
		seq_printf(s, "unsupported input value (%d)\n", val);
}

static const struct cmd_fn common_cmds[] = {
	CMDFN("cmds", dump_debug_cmds),
	CMDFN("set_adsp_ao", set_adsp_ao),
	{}
};

static int adsp_ap_dbg_show(struct seq_file *s, void *v)
{
	const struct cmd_fn *cf;
	char cmd[sizeof(last_cmd)];
	char *temp, *token = NULL;
	char delim[] = " ,\t\n";
	int value = 0, ret = -1;

	strscpy(cmd, last_cmd, sizeof(cmd));
	temp = cmd;
	token = strsep(&temp, delim);

	for (cf = common_cmds; cf->cmd; cf++) {
		if (token && strcmp(cf->cmd, token) == 0) {
			if (temp) {
				token = strsep(&temp, delim);
				ret = kstrtoint(token, 10, &value);

				if (ret != 0)
					value = 0;
			}

			cf->fn(s, (void *)&value);
			break;
		}
	}

	return 0;
}

static ssize_t adsp_ap_debug_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	size_t buf_size;
	char *nl;

	buf_size = min(count, (sizeof(last_cmd) - 1UL));

	if (unlikely(buf_size == 0))
		return -EFAULT;

	if (copy_from_user(last_cmd, buffer, buf_size))
		return -EFAULT;

	last_cmd[buf_size - 1] = '\0';

	nl = strchr(last_cmd, '\n');
	if (nl != NULL)
		*nl = '\0';

	return (ssize_t)buf_size;
}

static int adsp_ap_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, adsp_ap_dbg_show, NULL);
}

const struct file_operations adsp_ap_debug_ops = {
	.open = adsp_ap_dbg_open,
	.read = seq_read,
	.write = adsp_ap_debug_write,
	.release = single_release,
};

void adsp_dbg_init(struct adspsys_priv *mt_adspsys)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_create_file("audiodsp_ap", S_IFREG | 0644, NULL, mt_adspsys, &adsp_ap_debug_ops);
#endif
}
EXPORT_SYMBOL(adsp_dbg_init);

