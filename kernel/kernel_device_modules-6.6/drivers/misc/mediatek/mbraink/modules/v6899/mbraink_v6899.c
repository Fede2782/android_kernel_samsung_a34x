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

#include "mbraink_v6899.h"

static ssize_t mbraink_platform_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "show mbraink v6899 information...\n");
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
		mbraink_v6899_gpu_setQ2QTimeoutInNS(value);
	if (command == 2)
		mbraink_v6899_gpu_setPerfIdxTimeoutInNS(value);
	if (command == 3)
		mbraink_v6899_gpu_setPerfIdxLimit(value);
	if (command == 4)
		mbraink_v6899_gpu_dumpPerfIdxList();
	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_info);

static int mbraink_v6899_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *mbraink_v6899_device = &pdev->dev;

	ret = device_create_file(mbraink_v6899_device, &dev_attr_mbraink_platform_info);
	pr_info("[MBK_v6899] %s: device create file mbraink info ret = %d\n", __func__, ret);

	ret = mbraink_v6899_memory_init(mbraink_v6899_device);
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 memory init failed.\n");

	ret = mbraink_v6899_audio_init();
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 audio init failed.\n");

	ret = mbraink_v6899_battery_init(mbraink_v6899_device);
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 battery init failed.\n");

	ret = mbraink_v6899_power_init();
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 power init failed.\n");

	ret = mbraink_v6899_gpu_init();
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 gpu init failed.\n");

	ret = mbraink_v6899_gps_init();
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 gps init failed.\n");

	ret = mbraink_v6899_wifi_init();
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 wifi init failed.\n");

	ret = mbraink_v6899_camera_init();
	if (ret)
		pr_notice("[MBK_v6899] mbraink v6899 camera init failed.\n");

	return ret;
}

static int mbraink_v6899_remove(struct platform_device *pdev)
{
	struct device *mbraink_v6899_device = &pdev->dev;

	device_remove_file(mbraink_v6899_device, &dev_attr_mbraink_platform_info);

	mbraink_v6899_memory_deinit(mbraink_v6899_device);
	mbraink_v6899_audio_deinit();
	mbraink_v6899_battery_deinit();
	mbraink_v6899_power_deinit();
	mbraink_v6899_gpu_deinit();
	mbraink_v6899_gps_deinit();
	mbraink_v6899_wifi_deinit();
	mbraink_v6899_camera_deinit();

	return 0;
}

static const struct of_device_id mtk_mbraink_v6899_of_ids[] = {
	{ .compatible = "mediatek,mbraink-v6899" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mbraink_v6899_of_ids);

static struct platform_driver mtk_mbraink_v6899_driver = {
	.probe = mbraink_v6899_probe,
	.remove = mbraink_v6899_remove,
	.driver = {
		.name = "mtk_mbraink_v6899",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = mtk_mbraink_v6899_of_ids,
		.pm = NULL,
	},
};
module_platform_driver(mtk_mbraink_v6899_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK v6899 Linux Device Driver");
MODULE_VERSION("1.0");
