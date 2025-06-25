// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

static int sgmii_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain)
		return -EPROBE_DEFER;

	pm_runtime_enable(&pdev->dev);

	/* always enabled in lifetime */
	pm_runtime_get_sync(&pdev->dev);

	return 0;
}

static int sgmii_remove(struct platform_device *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

int sgmii_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (!pdev) {
		dev_info(device, "%s pdev == NULL\n", __func__);
		return -1;
	}

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	dev_info(device, "sgmii suspend_noirq done\n");

	return 0;
}

int sgmii_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (!pdev) {
		dev_err(device, "%s pdev == NULL\n", __func__);
		return -1;
	}

	pm_runtime_enable(&pdev->dev);

	/* always enabled in lifetime */
	pm_runtime_get_sync(&pdev->dev);

	dev_info(device, "sgmii resume_noirq done\n");

	return 0;
}

static const struct dev_pm_ops sgmii_pm_ops = {
	.suspend_noirq = sgmii_suspend,
	.resume_noirq = sgmii_resume,
};

static const struct of_device_id sgmii_id_table[] = {
	{ .compatible = "mediatek,sgmii-up",},
	{ },
};
MODULE_DEVICE_TABLE(of, sgmii_id_table);

static struct platform_driver sgmii_up = {
	.probe		= sgmii_probe,
	.remove		= sgmii_remove,
	.driver		= {
		.name	= "sgmii-up",
		.pm = &sgmii_pm_ops,
		.owner	= THIS_MODULE,
		.of_match_table = sgmii_id_table,
	},
};
module_platform_driver(sgmii_up);

MODULE_AUTHOR("Jianguo Zhang <jianguo.zhang@mediatek.com>");
MODULE_DESCRIPTION("SGMII power control");
MODULE_LICENSE("GPL");
