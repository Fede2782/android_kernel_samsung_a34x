// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "mvpu_plat.h"

#ifndef MVPU_V20_PLAT_DATA
#ifndef MVPU_V25_PLAT_DATA
struct mvpu_platdata mvpu_mt6879_platdata;
struct mvpu_platdata mvpu_mt6886_platdata;
struct mvpu_platdata mvpu_mt6895_platdata;
struct mvpu_platdata mvpu_mt6897_platdata;
struct mvpu_platdata mvpu_mt6899_platdata;
struct mvpu_platdata mvpu_mt6983_platdata;
struct mvpu_platdata mvpu_mt6985_platdata;
struct mvpu_platdata mvpu_mt6989_platdata;
struct mvpu_platdata mvpu_mt6991_platdata;
struct mvpu_platdata mvpu_mt8139_platdata;
#endif // MVPU_V25_PLAT_DATA
#endif // MVPU_V20_PLAT_DATA

struct mvpu_platdata *g_mvpu_platdata;

int mvpu_drv_loglv;

static const struct of_device_id mvpu_of_match[] = {
	{
	.compatible = "mediatek, mt6897-mvpu",
	.data = &mvpu_mt6897_platdata
	},
	{
	.compatible = "mediatek, mt6886-mvpu",
	.data = &mvpu_mt6886_platdata
	},
	{
	.compatible = "mediatek, mt6895-mvpu",
	.data = &mvpu_mt6895_platdata
	},
	{
	.compatible = "mediatek, mt6879-mvpu",
	.data = &mvpu_mt6879_platdata
	},
	{
	.compatible = "mediatek, mt6899-mvpu",
	.data = &mvpu_mt6899_platdata
	},
	{
	.compatible = "mediatek, mt6983-mvpu",
	.data = &mvpu_mt6983_platdata
	},
	{
	.compatible = "mediatek, mt6985-mvpu",
	.data = &mvpu_mt6985_platdata
	},
	{
	.compatible = "mediatek, mt6989-mvpu",
	.data = &mvpu_mt6989_platdata
	},
	{
	.compatible = "mediatek, mt6991-mvpu",
	.data = &mvpu_mt6991_platdata
	},
	{
	.compatible = "mediatek, mt8139-mvpu",
	.data = &mvpu_mt8139_platdata
	},
	{
	/* end of list */
	},
};

MODULE_DEVICE_TABLE(of, mvpu_of_match);

const struct of_device_id *mvpu_plat_get_device(void)
{
	return mvpu_of_match;
}

int mvpu_platdata_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mvpu_platdata *platdata;
	unsigned int dts_ver;

	if (g_mvpu_platdata != NULL) {
		dev_info(dev, "%s: already init, by pass\n", __func__);
		return 0;
	}

	platdata = (struct mvpu_platdata *)of_device_get_match_data(dev);

	if (!platdata) {
		dev_info(dev, "%s: get of_device_get_match_data fail\n", __func__);
		return -EINVAL;
	}

	dev_info(dev, "%s: platdata_sw_ver = %d\n", __func__, platdata->sw_ver);
	dev_info(dev, "%s: sw_preemption_level = %d\n",
			__func__, platdata->sw_preemption_level);

	if (of_property_read_u32(dev->of_node, "version", &dts_ver) == 0)
		dev_info(dev, "%s: dts_ver = %d\n", __func__, dts_ver);

	if (!of_property_read_u32(dev->of_node, "core_num", &platdata->core_num)) {
	} else if (!of_property_read_u32(dev->of_node, "core-num", &platdata->core_num)) {
	} else {
		dev_info(dev, "%s: get core num fail\n", __func__);
		return -EINVAL;
	}
	dev_info(dev, "%s: core-num = %d\n", __func__, platdata->core_num);

	if (platdata->core_num > MAX_CORE_NUM) {
		dev_info(dev, "%s: invalid core number: %d\n", __func__, platdata->core_num);
		return -EINVAL;
	}

	if (of_property_read_u64(dev->of_node, "mask", &platdata->dma_mask)) {
		dev_info(dev, "%s: get mask fail\n", __func__);
		return -EINVAL;
	}
	dev_info(dev, "%s: mask = 0x%llx\n", __func__, platdata->dma_mask);

	platdata->pdev = pdev;
	g_mvpu_platdata = platdata;

	return 0;
}



