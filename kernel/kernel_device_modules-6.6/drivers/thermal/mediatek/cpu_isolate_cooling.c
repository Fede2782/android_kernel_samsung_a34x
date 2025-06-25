// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <core_ctl.h>
#include "cpu_isolate_cooling.h"

#define MAX_CPUS 8

struct cpu_isolate_cooling_device {
	struct thermal_cooling_device *cdev;
	unsigned int cpu_id;
	char cooler_name[20];
	unsigned int max_state;
	unsigned int cur_state;
};


static struct cpu_isolate_cooling_device cpu_isolate_devs[MAX_CPUS];

static int cpu_isolate_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct cpu_isolate_cooling_device *dev = cdev->devdata;
	*state = dev->max_state;
	return 0;
}

static int cpu_isolate_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct cpu_isolate_cooling_device *dev = cdev->devdata;
	*state = dev->cur_state;
	return 0;
}

static int cpu_isolate_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct cpu_isolate_cooling_device *dev = cdev->devdata;
	int ret;

	if (state > dev->max_state)
		return -EINVAL;

	dev->cur_state = state;

	if (state == 1)
		ret = core_ctl_force_pause_cpu(dev->cpu_id, true);
	else
		ret = core_ctl_force_pause_cpu(dev->cpu_id, false);

	if (ret != 0)
		pr_info("%s:%d set cpu%d iso %lu error %d\n", __func__, __LINE__, dev->cpu_id, state, ret);

	return 0;
}

static const struct thermal_cooling_device_ops cpu_isolate_cooling_ops = {
	.get_max_state = cpu_isolate_get_max_state,
	.get_cur_state = cpu_isolate_get_cur_state,
	.set_cur_state = cpu_isolate_set_cur_state,
};

static int cpu_isolate_probe(struct platform_device *pdev)
{
	int i, ret;
	struct device_node *np = pdev->dev.of_node;
	const char *name;
	int num_cpu = MAX_CPUS;

	ret = of_property_read_string(np, "cooler-names", &name);
	if (ret) {
		pr_info("%s:%d get cooler name error, use cpu-isolate\n", __func__, __LINE__);
		name = "cpu-isolate";
	}
	if (nr_cpu_ids < MAX_CPUS)
		num_cpu = nr_cpu_ids;

	for (i = 0; i < num_cpu; i++) {
		cpu_isolate_devs[i].cpu_id = i;
		cpu_isolate_devs[i].max_state = 1;
		cpu_isolate_devs[i].cur_state = 0;

		ret = snprintf(cpu_isolate_devs[i].cooler_name, THERMAL_NAME_LENGTH, "%s-%d", name, i);
		if (ret)
			pr_notice("%s:%d snprintf error\n", __func__, __LINE__);

		cpu_isolate_devs[i].cdev = thermal_of_cooling_device_register(np,
			cpu_isolate_devs[i].cooler_name, &cpu_isolate_devs[i],  &cpu_isolate_cooling_ops);

		if (IS_ERR(cpu_isolate_devs[i].cdev)) {
			pr_info("Failed to register cooling device for CPU %d ~~~\n", i);
			return PTR_ERR(cpu_isolate_devs[i].cdev);
		}
	}

	return 0;
}

static int cpu_isolate_remove(struct platform_device *pdev)
{
	int i;
	int num_cpu = MAX_CPUS;

	if (nr_cpu_ids < MAX_CPUS)
		num_cpu = nr_cpu_ids;

	for (i = 0; i < num_cpu; i++)
		thermal_cooling_device_unregister(cpu_isolate_devs[i].cdev);


	return 0;
}

static const struct of_device_id cpu_isolate_of_match[] = {
	{ .compatible = "mediatek,cpu-isolate", },
	{},
};
MODULE_DEVICE_TABLE(of, cpu_isolate_of_match);

static struct platform_driver cpu_isolate_driver = {
	.driver = {
		.name = "mtk-cpu-isolate-cooling",
		.of_match_table = cpu_isolate_of_match,
	},
	.probe = cpu_isolate_probe,
	.remove = cpu_isolate_remove,
};

module_platform_driver(cpu_isolate_driver);

MODULE_AUTHOR("Samuel Hsieh <samuel.hsieh@mediatek.com>");
MODULE_DESCRIPTION("Mediatek cpu isolate cooling driver");
MODULE_LICENSE("GPL");
