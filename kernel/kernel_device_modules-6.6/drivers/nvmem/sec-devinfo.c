// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Stanley Song <stanley.song@mediatek.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct factory_devinfo_tag {
	unsigned int data_size;
	unsigned int data[0];
};

/* Do not modify this strcuture. */
struct sec_device_info_prop {
	unsigned int version;
	unsigned int hrid[4];
	unsigned int vmin;
	unsigned int iddq;
	unsigned int class_dou;
	unsigned int class_gb_sc;
	unsigned int class_gb_mc;
	unsigned int class_aztec;
};

struct sec_device_prop {
	struct sec_device_info_prop info;
	struct platform_device *pdev;
};

static ssize_t get_hrid_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_hrid_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%08X%08X%08X%08X\n", prop->info.hrid[0], prop->info.hrid[1], prop->info.hrid[2], prop->info.hrid[3]);
}
static DEVICE_ATTR_RO(get_hrid);

#define make_bit_value(value, bsh, mark, pos) (((value)>>(bsh)&(mark))<<(pos))

static ssize_t get_voltage_min_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_voltage_min_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%u\n", prop->info.vmin);
}
static DEVICE_ATTR_RO(get_voltage_min);

static ssize_t get_iddq_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_iddq_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%d\n", prop->info.iddq);
}

static DEVICE_ATTR_RO(get_iddq);

const static char *class_index[] = {
	"unknown", "A", "B", "C", "D", "E", "F", "unknown"
};

static ssize_t get_class_dou_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_class_dou_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%s\n", class_index[prop->info.class_dou]);
}

static DEVICE_ATTR_RO(get_class_dou);

static ssize_t get_class_gb_sc_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_class_gb_sc_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%s\n", class_index[prop->info.class_gb_sc]);
}

static DEVICE_ATTR_RO(get_class_gb_sc);

static ssize_t get_class_gb_mc_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_class_gb_mc_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%s\n", class_index[prop->info.class_gb_mc]);
}

static DEVICE_ATTR_RO(get_class_gb_mc);

static ssize_t get_class_aztec_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_device_prop *prop;
	struct platform_device *pdev = to_platform_device(dev);

	prop = platform_get_drvdata(pdev);
	if (!prop) {
		pr_warn("pdev get_class_aztec_show is not found!!\n");
		return 0;
	}

	return sprintf(buf, "%s\n", class_index[prop->info.class_aztec]);
}

static DEVICE_ATTR_RO(get_class_aztec);

static struct attribute *sec_devinfo_attrs[] = {
	&dev_attr_get_hrid.attr,
	&dev_attr_get_voltage_min.attr,
	&dev_attr_get_iddq.attr,
	&dev_attr_get_class_dou.attr,
	&dev_attr_get_class_gb_sc.attr,
	&dev_attr_get_class_gb_mc.attr,
	&dev_attr_get_class_aztec.attr,
	NULL,
};

static struct attribute_group sec_devinfo_attr_group = {
	.attrs = sec_devinfo_attrs,
};

static int sec_devinfo_probe(struct platform_device *pdev)
{
	struct device_node *chosen_node;
	struct factory_devinfo_tag *tags;
	struct sec_device_prop *prop = NULL;
	int ret = 0;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node) {
		chosen_node = of_find_node_by_path("/chosen@0");
		if (!chosen_node) {
			pr_warn("chosen node is not found!!\n");
			return -ENXIO;
		}
	}

	tags = (struct factory_devinfo_tag *) of_get_property(chosen_node,
						"atag,factory_devinfo",	NULL);
	if (tags) {
		prop = devm_kzalloc(&pdev->dev, sizeof(struct sec_device_prop), GFP_KERNEL);
		if (!prop)
			return -ENOMEM;

		prop->pdev = pdev;
		platform_set_drvdata(pdev, prop);

		if (tags->data_size == sizeof(struct sec_device_info_prop)) {
			memcpy(&prop->info, &tags->data[0], sizeof(struct sec_device_info_prop));
			pr_info("factory devinfo data_size: %d, version :%d\n", tags->data_size, prop->info.version);
		} else {
			pr_info("factory devinfo data_size:%d, size: %lu\n", tags->data_size, sizeof(struct sec_device_info_prop));
			return -EINVAL;
		}
	} else {
		pr_warn("atag,devinfo is not found\n");
		return -ENXIO;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &sec_devinfo_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create sysfs group\n");
		return -ENXIO;
	}

	return ret;
}

static const struct of_device_id sec_devinfo_of_match[] = {
	{ .compatible = "sec,sec-devinfo", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_devinfo_of_match);

static struct platform_driver sec_devinfo_driver = {
	.probe = sec_devinfo_probe,
	.driver = {
		.name = "sec-devinfo",
		.of_match_table = sec_devinfo_of_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(sec_devinfo_driver);

MODULE_AUTHOR("Stanley Song <stanley.song@mediatek.com>");
MODULE_DESCRIPTION("Mediatek dou/gb_sc/gb_mc/aztec class information driver");
MODULE_LICENSE("GPL v2");
