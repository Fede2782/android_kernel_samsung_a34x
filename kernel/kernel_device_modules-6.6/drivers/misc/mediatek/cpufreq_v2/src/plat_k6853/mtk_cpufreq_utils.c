// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>

#include "mtk_cpufreq_platform.h"

int efuse_get_value_by_offset(unsigned int offset, size_t size, void *buf)
{
	int ret;
	struct nvmem_device *nvmem_dev;
	struct device_node *node;

	node = of_find_node_by_name(NULL, "eem-fsm");
	if (!node) {
		tag_pr_info("%s fail to get device node\n", __func__);
		return -1;
	}
	nvmem_dev = of_nvmem_device_get(node, "mtk_efuse");
	if (!nvmem_dev) {
		of_node_put(node);
		tag_pr_info("%s fail to get nvmem device\n", __func__);
		return -1;
	}
	ret = nvmem_device_read(nvmem_dev, offset, size, buf);
	nvmem_device_put(nvmem_dev);
	of_node_put(node);

	return ret;
}
EXPORT_SYMBOL(efuse_get_value_by_offset);

unsigned int _mt_cpufreq_get_cpu_level(void)
{
	int val, cpulv, cpulv0, cpulv1, cpulv2, seg, ret __maybe_unused = 0;
#if IS_ENABLED(CONFIG_REGULATOR_MT6362)
	unsigned int lv = CPU_LEVEL_3;
#else
	unsigned int lv = CPU_LEVEL_4;
#endif

	ret |= efuse_get_value_by_offset(0x001C, sizeof(__u32), &val); /* segment code index 7*/
	ret |= efuse_get_value_by_offset(0x00D0, sizeof(__u32), &cpulv0); /* cpu level code index 52*/
	ret |= efuse_get_value_by_offset(0x00F8, sizeof(__u32), &cpulv); /* cpu level code index 62*/

//	if(ret < 0) {
//		tag_pr_info("%s error reading efuse\n", __func__);
//		goto exit;
//	}

	val &= 0xFF;
	cpulv1 = (cpulv0 & 0xFF); /* cpu level code [7:0]*/
	cpulv2 = (cpulv & 0x300); /* cpu level code [9:8]*/
	seg = val & 0x3; /* segment cod[1:0] */

	if (!val) {
#if IS_ENABLED(CONFIG_REGULATOR_MT6362)
		lv = CPU_LEVEL_1;
#else
		lv = CPU_LEVEL_2;
#endif
	} else {
		if (seg) {
#if IS_ENABLED(CONFIG_REGULATOR_MT6362)
			lv = CPU_LEVEL_1;
#else
			lv = CPU_LEVEL_2;
#endif
		}
	}

#if defined(K6853TV1)
	if (!val) {
		if (cpulv1 > 1 || cpulv2) {
#if IS_ENABLED(CONFIG_REGULATOR_MT6362)
			lv = CPU_LEVEL_3;
#else
			lv = CPU_LEVEL_4;
#endif
		}
	}
	WARN_ON(GEN_DB_ON(lv < CPU_LEVEL_3,
		"cpufreq segment wrong, efuse_val = 0x%x 0x%x",
		val, cpulv));
#endif

#if defined(QEA)
	if (!val) {
		if (cpulv1 > 1 || cpulv2) {
#if IS_ENABLED(CONFIG_REGULATOR_MT6362)
			lv = CPU_LEVEL_5;
#else
			lv = CPU_LEVEL_6;
#endif
		}
	}
	WARN_ON(GEN_DB_ON(lv < CPU_LEVEL_5,
			"cpufreq segment wrong for qea, efuse_val = 0x%x 0x%x",
		val, cpulv));
#endif

#if defined(TURBO)
	lv = CPU_LEVEL_0;
	tag_pr_info("turbo project over\n");
#endif
//exit:
	tag_pr_info("%d, Settle time(%d, %d) efuse_val = 0x%x 0x%x 0x%x 0x%x\n",
		lv, UP_SRATE, DOWN_SRATE, val, cpulv, cpulv1, cpulv2);
	return lv;
}
EXPORT_SYMBOL(_mt_cpufreq_get_cpu_level);

static int __init mt_cpufreq_utils_init(void)
{
	pr_info("%s probe success", __func__);
	return 0;
}

static void __exit mt_cpufreq_utils_exit(void)
{
	pr_info("%s exit success", __func__);
}

#if IS_BUILTIN(CONFIG_MEDIATEK_CPU_DVFS)
late_initcall(mt_cpufreq_utils_init);
#else
module_init(mt_cpufreq_utils_init);
#endif
module_exit(mt_cpufreq_utils_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek CPU DVFS Driver v0.3.1");
MODULE_AUTHOR("Mediatek Inc");

