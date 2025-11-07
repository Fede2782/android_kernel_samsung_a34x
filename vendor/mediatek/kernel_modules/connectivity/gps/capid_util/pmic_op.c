/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/sysfs.h>
#include "pmic_op.h"
#include "include/pmic_chip_data.h"

static const struct pmic_chip_data *g_pmic_chip_data;
static struct regmap *g_pmic_regmap;

/*
 * Initialize PMIC operation by obtaining regmap and chip-specific data
 * from device tree.
 */
int pmic_op_of_init(void)
{
	struct device_node *pmic_node;
	struct platform_device *pmic_dev;
	const char *compatible;

	pmic_node = of_find_compatible_node(NULL, NULL, "mediatek,mt6685-tb-clkbuf");
	if (!pmic_node) {
		pr_notice("%s: failed to find compatible node\n", __func__);
		return -ENODEV;
	}

	/* Get the 'compatible' property from the device node */
	compatible = of_get_property(pmic_node, "compatible", NULL);
	if (!compatible) {
		pr_notice("%s: failed to get compatible string\n", __func__);
		of_node_put(pmic_node);
		return -ENODEV;
	}

	g_pmic_chip_data = get_pmic_chip_data(compatible);
	if (!g_pmic_chip_data) {
		pr_notice("%s: failed to get chip data for compatible: %s\n", __func__, compatible);
		return -ENODEV;
	}

	pmic_dev = of_find_device_by_node(pmic_node);
	if (!pmic_dev) {
		pr_notice("%s: failed to find device by node\n", __func__);
		return -ENODEV;
	}
	g_pmic_regmap = dev_get_regmap(pmic_dev->dev.parent, NULL);
	if (!g_pmic_regmap) {
		pr_notice("%s: failed to get regmap\n", __func__);
		return -ENODEV;
	}
	return 0;
}

/*
 * Set capid pre1 value using chip-specific register and mask.
 */
int pmic_op_set_capid_pre1(int acc)
{
	int ret;
	int old_val = 0, read_back_val1 = 0, read_back_val2 = 0;
	int val = 0;

	pr_notice("%s: acc=%d is ignored, always assume 0\n", __func__, acc);

	if (!g_pmic_regmap || !g_pmic_chip_data) {
		if (!g_pmic_regmap)
			pr_notice("pmic_op: g_pmic_regmap is NULL\n");
		if (!g_pmic_chip_data)
			pr_notice("pmic_op: g_pmic_chip_data is NULL\n");
		return -ENODEV;
	}

	ret = regmap_read(g_pmic_regmap, g_pmic_chip_data->rg_xo_aac_fpm_swen_addr, &old_val);
	if (ret) {
		pr_notice("regmap_read failed, reg=0x%x, ret=%d\n",
			g_pmic_chip_data->rg_xo_aac_fpm_swen_addr, ret);
		return ret;
	}

	/* Clear bit 6 and write */
	val = old_val & ~(1 << 6);
	ret = regmap_update_bits(g_pmic_regmap, g_pmic_chip_data->rg_xo_aac_fpm_swen_addr,
		g_pmic_chip_data->rg_xo_aac_fpm_swen_mask << g_pmic_chip_data->rg_xo_aac_fpm_swen_shift, val);
	if (ret) {
		pr_notice("regmap_update_bits (clear) failed, reg=0x%x, val=0x%x, ret=%d\n",
			g_pmic_chip_data->rg_xo_aac_fpm_swen_addr, val, ret);
		return ret;
	}
	mdelay(1);

	/* Read value after first write */
	ret = regmap_read(g_pmic_regmap, g_pmic_chip_data->rg_xo_aac_fpm_swen_addr, &read_back_val1);
	if (ret)
		pr_notice("regmap_read1 failed, ret=%d\n", ret);

	/* Set bit 6 and write */
	val |= (1 << 6);
	ret = regmap_update_bits(g_pmic_regmap, g_pmic_chip_data->rg_xo_aac_fpm_swen_addr,
		g_pmic_chip_data->rg_xo_aac_fpm_swen_mask << g_pmic_chip_data->rg_xo_aac_fpm_swen_shift, val);
	if (ret) {
		pr_notice("regmap_update_bits (set) failed, reg=0x%x, val=0x%x, ret=%d\n",
			g_pmic_chip_data->rg_xo_aac_fpm_swen_addr, val, ret);
		return ret;
	}
	mdelay(5);

	/* Read value after second write */
	ret = regmap_read(g_pmic_regmap, g_pmic_chip_data->rg_xo_aac_fpm_swen_addr, &read_back_val2);
	if (ret)
		pr_notice("regmap_read2 failed, ret=%d\n", ret);

	pr_notice("set capid_pre1: old_val=0x%x, read_back1=0x%x, read_back2=0x%x\n",
		old_val, read_back_val1, read_back_val2);
	return 0;
}

/*
 * Set capid pre2 value using chip-specific register and mask.
 */
int pmic_op_set_capid_pre2(int capid)
{
	int ret;
	int old_capid = 0, new_capid = 0;

	if (!g_pmic_regmap || !g_pmic_chip_data) {
		if (!g_pmic_regmap)
			pr_notice("pmic_op: g_pmic_regmap is NULL\n");
		if (!g_pmic_chip_data)
			pr_notice("pmic_op: g_pmic_chip_data is NULL\n");
		return -ENODEV;
	}

	ret = regmap_read(g_pmic_regmap, g_pmic_chip_data->rg_xo_cdac_fpm_addr, &old_capid);
	if (ret) {
		pr_notice("regmap_read old_capid failed, reg=0x%x, ret=%d\n",
			  g_pmic_chip_data->rg_xo_cdac_fpm_addr, ret);
		return ret;
	}

	if (capid < 0 || capid > 255) {
		pr_notice("capid out of range: 0x%x, old_capid: 0x%x\n", capid, old_capid);
		return 0;
	}

	ret = regmap_update_bits(g_pmic_regmap, g_pmic_chip_data->rg_xo_cdac_fpm_addr,
		g_pmic_chip_data->rg_xo_cdac_fpm_mask << g_pmic_chip_data->rg_xo_cdac_fpm_shift, capid);
	if (ret) {
		pr_notice("regmap_update_bits failed, reg=0x%x, capid=0x%x, ret=%d\n",
			g_pmic_chip_data->rg_xo_cdac_fpm_addr, capid, ret);
		return ret;
	}

	ret = regmap_read(g_pmic_regmap, g_pmic_chip_data->rg_xo_cdac_fpm_addr, &new_capid);
	if (ret) {
		pr_notice("regmap_read new_capid failed, reg=0x%x, ret=%d\n",
			  g_pmic_chip_data->rg_xo_cdac_fpm_addr, ret);
		return ret;
	}

	pr_notice("set capid: 0x%x, old_capid: 0x%x, new_capid: 0x%x\n",
		   capid, old_capid, new_capid);
	return 0;
}
