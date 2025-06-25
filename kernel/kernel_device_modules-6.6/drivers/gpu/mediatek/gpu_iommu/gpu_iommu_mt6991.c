// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/iommu.h>
#include <linux/of_address.h>

static int gpu_iommu_probe(struct platform_device *pdev)
{
	pr_info("[gpu_iommu] %s\n", __func__);
	return 0;
}

static int gpu_iommu_remove(struct platform_device *pdev)
{
	pr_info("[gpu_iommu] %s\n", __func__);
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "mediatek,gpu-iommu" },
	{ /* end of table */ }
};

static struct platform_driver gpu_iommu_driver = {
	.probe = gpu_iommu_probe,
	.remove = gpu_iommu_remove,
	.driver = {
		.name = "gpu_iommu_mt6991",
		.of_match_table = match_table,
	},
};
module_platform_driver(gpu_iommu_driver);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek GPU IOMMU Driver");
MODULE_LICENSE("GPL");
