// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "mbraink_v6989.h"

static ssize_t mbraink_platform_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "show mbraink v6989 information...\n");
}

static ssize_t mbraink_platform_info_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	unsigned int command;
	unsigned long long value;
	int retSize = 0;

	retSize = sscanf(buf, "%d %llu", &command, &value);
	if (retSize == -1)
		return 0;

	pr_info("%s: Get Command (%d), Value (%llu) size(%d)\n",
			__func__,
			command,
			value,
			retSize);

	if (command == 1)
		mbraink_v6989_gpu_setQ2QTimeoutInNS(value);
	if (command == 2)
		mbraink_v6989_gpu_setPerfIdxTimeoutInNS(value);
	if (command == 3)
		mbraink_v6989_gpu_setPerfIdxLimit(value);
	if (command == 4)
		mbraink_v6989_gpu_dumpPerfIdxList();

	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_info);


static int mbraink_v6989_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *mbraink_v6989_device = &pdev->dev;

	ret = device_create_file(mbraink_v6989_device, &dev_attr_mbraink_platform_info);
	pr_info("[MBK_v6989] %s: device create file mbraink info ret = %d\n", __func__, ret);

	ret = mbraink_v6989_memory_init();
	if (ret)
		pr_notice("[MBK_v6989] mbraink v6989 memory init failed.\n");

	ret = mbraink_v6989_audio_init();
	if (ret)
		pr_notice("[MBK_v6989] mbraink v6989 audio init failed.\n");

	ret = mbraink_v6989_battery_init(mbraink_v6989_device);
	if (ret)
		pr_notice("[MBK_v6989] mbraink v6989 battery init failed.\n");

	ret = mbraink_v6989_power_init();
	if (ret)
		pr_notice("[MBK_v6989] mbraink v6989 power init failed.\n");

	ret = mbraink_v6989_gpu_init();
	if (ret)
		pr_notice("[MBK_v6989] mbraink v6989 gpu init failed.\n");
	return ret;
}


static int mbraink_v6989_remove(struct platform_device *pdev)
{
	struct device *mbraink_v6989_device = &pdev->dev;

	device_remove_file(mbraink_v6989_device, &dev_attr_mbraink_platform_info);
	mbraink_v6989_memory_deinit();
	mbraink_v6989_audio_deinit();
	mbraink_v6989_battery_deinit();
	mbraink_v6989_power_deinit();
	mbraink_v6989_gpu_deinit();

	return 0;
}

static const struct of_device_id mtk_mbraink_v6989_of_ids[] = {
	{ .compatible = "mediatek,mbraink-v6989" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mbraink_v6989_of_ids);

static struct platform_driver mtk_mbraink_v6989_driver = {
	.probe = mbraink_v6989_probe,
	.remove = mbraink_v6989_remove,
	.driver = {
		.name = "mtk_mbraink_v6989",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = mtk_mbraink_v6989_of_ids,
		.pm = NULL,
	},
};
module_platform_driver(mtk_mbraink_v6989_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK v6989 Linux Device Driver");
MODULE_VERSION("1.0");
