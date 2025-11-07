/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include "pmic_op.h"
#include "include/ufs_mtk_dbg.h"

/* For pr_notice, pr_info */
#include <linux/printk.h>
/* For GFP_KERNEL */
#include <linux/gfp.h>
/* For kstrtouint */
#include <linux/kstrtox.h>

/*
 * enum BOOTMODE {
 * NORMAL_BOOT = 0,
 * META_BOOT = 1,
 * RECOVERY_BOOT = 2,
 * SW_REBOOT = 3,
 * FACTORY_BOOT = 4,
 * ADVMETA_BOOT = 5,
 * ATE_FACTORY_BOOT = 6,
 * ALARM_BOOT = 7,
 * UNKNOWN_BOOT
 * };
 */
#define CAPID_UTIL_BOOTMODE_META 0 /* Set this to the required META boot mode value */
static int capid_util_bootmode = -1;

static int pmic_op_init_result = -1;

/*
 * Get boot mode from device tree.
 * Returns bootmode on success, -1 on failure.
 */
static int capid_util_get_bootmode(void)
{
	struct device_node *dnode = NULL;
	struct tag_bootmode {
		unsigned int size;
		unsigned int tag;
		unsigned int bootmode;
	};
	struct tag_bootmode *tag = NULL;

	dnode = of_find_node_by_path("/chosen");
	if (!dnode)
		dnode = of_find_node_by_path("/chosen@0");

	if (!dnode) {
		pr_notice("capid_util: failed to get /chosen node\n");
		return -1;
	}

	tag = (struct tag_bootmode *)of_get_property(dnode, "atag,boot", NULL);
	if (!tag) {
		pr_notice("capid_util: failed to get atag,boot property\n");
		of_node_put(dnode);
		return -1;
	}
	of_node_put(dnode);
	pr_info("capid_util: bootmode: 0x%x\n", tag->bootmode);
	return tag->bootmode;
}

/*
 * This function safely parses two unsigned integers from the input buffer,
 * using strsep and kstrtouint, to replace the use of sscanf in kernel code.
 * Returns 0 on success, <0 on failure.
 */
static int capid_util_parse_cmd_arg(const char *buf, unsigned int *cmd, unsigned int *arg)
{
	char *input, *token, *cur;
	unsigned int tmp_cmd = 0, tmp_arg = 0;
	int ret = -EINVAL;

	input = kstrdup(buf, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	cur = strim(input);

	token = strsep(&cur, " \t");
	if (token && !kstrtouint(token, 0, &tmp_cmd)) {
		token = strsep(&cur, " \t");
		if (token && !kstrtouint(token, 0, &tmp_arg)) {
			*cmd = tmp_cmd;
			*arg = tmp_arg;
			ret = 0;
		}
	}
	kfree(input);
	return ret;
}

static ssize_t capid_util_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	int ret = 0;
	unsigned int cmd = 0, arg = 0;
	int parse_ret;

	if (capid_util_bootmode != CAPID_UTIL_BOOTMODE_META) {
		pr_info("capid_util: not META bootmode skip actual operation\n");
		pr_info("capid_util: bootmode: 0x%x, META_BOOT: 0x%x\n",
			capid_util_bootmode, CAPID_UTIL_BOOTMODE_META);
		return count;
	}

	parse_ret = capid_util_parse_cmd_arg(buf, &cmd, &arg);
	if (parse_ret != 0) {
		pr_info("capid_util: user write: %.*s", (int)count, buf);
		return count;
	}

	/*
	 * Supported command format(example):
	 *   echo "0x1000 0x0" > /sys/kernel/gps_meta_capid_util
	 *   echo "0x2000 0xA0" > /sys/kernel/gps_meta_capid_util
	 */
	pr_info("capid_util: user input cmd: 0x%x, arg: 0x%x, bootmode: 0x%x, META_BOOT: 0x%x\n",
		cmd, arg, capid_util_bootmode, CAPID_UTIL_BOOTMODE_META);

	switch (cmd) {
	case 0x1000: /* SET_CAPID_PRE_1 */
	{
		if (pmic_op_init_result != 0) {
			pr_notice("%s: skip SET_CAPID_PRE_1: pmic_op_of_init failed, result=%d\n",
				  __func__, pmic_op_init_result);
			break;
		}
#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
		int hold_ret = 0, release_ret = 0;

		hold_ret = ufs_mtk_cali_hold();
		if (hold_ret != 0)
			pr_notice("capid_util: ufs_mtk_cali_hold failed, ret=%d\n", hold_ret);
		ret = pmic_op_set_capid_pre1(arg);
		if (hold_ret == 0)
			release_ret = ufs_mtk_cali_release();
		else
			release_ret = -1;
		pr_notice("capid_util: pmic_op_set_capid_pre1(%u) ret=%d, ufs_mtk_cali_hold=%d, ufs_mtk_cali_release=%d\n",
			  arg, ret, hold_ret, release_ret);
#else
		ret = pmic_op_set_capid_pre1(arg);
		pr_notice("capid_util: pmic_op_set_capid_pre1(%u) ret=%d, ufs_dbg api not support\n",
			  arg, ret);
#endif
		break;
	}
	case 0x2000: /* SET_CAPID_PRE_2 */
	{
		if (pmic_op_init_result != 0) {
			pr_notice("%s: skip SET_CAPID_PRE_2: pmic_op_of_init failed, result=%d\n",
				  __func__, pmic_op_init_result);
			break;
		}
#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
		int hold_ret = 0, release_ret = 0;

		hold_ret = ufs_mtk_cali_hold();
		if (hold_ret != 0)
			pr_notice("capid_util: ufs_mtk_cali_hold failed, ret=%d\n", hold_ret);
		ret = pmic_op_set_capid_pre2(arg);
		if (hold_ret == 0)
			release_ret = ufs_mtk_cali_release();
		else
			release_ret = -1;
		pr_notice("capid_util: pmic_op_set_capid_pre2(%u) ret=%d, ufs_mtk_cali_hold=%d, ufs_mtk_cali_release=%d\n",
			  arg, ret, hold_ret, release_ret);
#else
		ret = pmic_op_set_capid_pre2(arg);
		pr_notice("capid_util: pmic_op_set_capid_pre2(%u) ret=%d, ufs_dbg api not support\n",
			  arg, ret);
#endif
		break;
	}
	default:
		pr_notice("capid_util: unknown cmd: 0x%x\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (ret)
		pr_notice("capid_util: cmd:0x%x, error ret:%d\n", cmd, ret);

	return count;
}

static struct kobj_attribute capid_util_attr =
	__ATTR(gps_meta_capid_util, 0664, NULL, capid_util_store);

static int __init capid_util_init(void)
{
	int ret;

	capid_util_bootmode = capid_util_get_bootmode();
	pr_info("capid_util: detected bootmode: 0x%x\n", capid_util_bootmode);

	if (capid_util_bootmode == CAPID_UTIL_BOOTMODE_META) {
		pmic_op_init_result = pmic_op_of_init();
		if (pmic_op_init_result)
			pr_notice("capid_util: pmic_op_of_init failed: %d\n", pmic_op_init_result);
	} else {
		pr_info("capid_util: not META bootmode (current: 0x%x, required: 0x%x), skip pmic_op_of_init\n",
			capid_util_bootmode, CAPID_UTIL_BOOTMODE_META);
		pmic_op_init_result = -1;
	}

	ret = sysfs_create_file(kernel_kobj, &capid_util_attr.attr);
	if (ret) {
		pr_notice("capid_util: failed to create sysfs file\n");
		return ret;
	}

	pr_info("capid_util: module loaded\n");
	return 0;
}

static void __exit capid_util_exit(void)
{
	sysfs_remove_file(kernel_kobj, &capid_util_attr.attr);
	pr_info("[capid_util] module unloaded\n");
}

module_init(capid_util_init);
module_exit(capid_util_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("GPS META CapID Util");

