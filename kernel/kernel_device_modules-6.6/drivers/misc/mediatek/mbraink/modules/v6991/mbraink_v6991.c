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

#include "mbraink_v6991.h"

static ssize_t mbraink_platform_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "show mbraink v6991 information...\n");
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
		mbraink_v6991_gpu_setQ2QTimeoutInNS(value);
	if (command == 2)
		mbraink_v6991_gpu_setPerfIdxTimeoutInNS(value);
	if (command == 3)
		mbraink_v6991_gpu_setPerfIdxLimit(value);
	if (command == 4)
		mbraink_v6991_gpu_dumpPerfIdxList();
	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_info);

static int mbraink_v6991_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *mbraink_v6991_device = &pdev->dev;

	ret = device_create_file(mbraink_v6991_device, &dev_attr_mbraink_platform_info);
	pr_info("[MBK_v6991] %s: device create file mbraink info ret = %d\n", __func__, ret);

	ret = mbraink_v6991_memory_init(mbraink_v6991_device);
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 memory init failed.\n");

	ret = mbraink_v6991_audio_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 audio init failed.\n");

	ret = mbraink_v6991_battery_init(mbraink_v6991_device);
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 battery init failed.\n");

	ret = mbraink_v6991_power_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 power init failed.\n");

	ret = mbraink_v6991_gpu_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 gpu init failed.\n");

	ret = mbraink_v6991_gps_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 gps init failed.\n");

	ret = mbraink_v6991_wifi_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 wifi init failed.\n");

	ret = mbraink_v6991_camera_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 camera init failed.\n");

	return ret;
}

static int mbraink_v6991_remove(struct platform_device *pdev)
{
	struct device *mbraink_v6991_device = &pdev->dev;

	device_remove_file(mbraink_v6991_device, &dev_attr_mbraink_platform_info);

	mbraink_v6991_memory_deinit(mbraink_v6991_device);
	mbraink_v6991_audio_deinit();
	mbraink_v6991_battery_deinit();
	mbraink_v6991_power_deinit();
	mbraink_v6991_gpu_deinit();
	mbraink_v6991_gps_deinit();
	mbraink_v6991_wifi_deinit();
	mbraink_v6991_camera_deinit();

	return 0;
}

static const struct of_device_id mtk_mbraink_v6991_of_ids[] = {
	{ .compatible = "mediatek,mbraink-v6991" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mbraink_v6991_of_ids);

static struct platform_driver mtk_mbraink_v6991_driver = {
	.probe = mbraink_v6991_probe,
	.remove = mbraink_v6991_remove,
	.driver = {
		.name = "mtk_mbraink_v6991",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = mtk_mbraink_v6991_of_ids,
		.pm = NULL,
	},
};
module_platform_driver(mtk_mbraink_v6991_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK v6991 Linux Device Driver");
MODULE_VERSION("1.0");
