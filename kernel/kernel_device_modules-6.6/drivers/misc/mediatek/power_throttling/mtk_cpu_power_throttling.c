// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"
#include "mtk_cpu_power_throttling.h"

#if !IS_BUILTIN(CONFIG_MTK_CPU_POWER_THROTTLING)
#define CREATE_TRACE_POINTS
#endif
#include "mtk_low_battery_throttling_trace.h"

#define CPU_LIMIT_FREQ 900000
#define CPU_UNLIMIT_FREQ 2147483647
#define CLUSTER_NUM 3
#define CORE_NUM 8
#define NAME_LENGTH 32
#define CPU_DOWN(i)	\
	(get_cpu_device(i) == NULL ? false \
	: remove_cpu(i))
#define CPU_UP(i)	\
	(get_cpu_device(i) == NULL ? false \
	: add_cpu(i))
#define cpu_is_offline(cpu)	unlikely(!cpu_online(cpu))

struct cpu_pt_priv {
	char max_lv_name[NAME_LENGTH];
	char freq_limit_name[NAME_LENGTH];
	char core_onoff_name[NAME_LENGTH];
	u32 max_lv;
	u32 *freq_limit;
	u32 *core_onoff;
};

static struct cpu_pt_priv cpu_pt_info[POWER_THROTTLING_TYPE_MAX] = {
	[LBAT_POWER_THROTTLING] = {
		.max_lv_name = "lbat-max-level",
		.freq_limit_name = "lbat-limit-freq-lv",
		.core_onoff_name = "",
		.max_lv = LOW_BATTERY_LEVEL_NUM - 1,
	},
	[OC_POWER_THROTTLING] = {
		.max_lv_name = "oc-max-level",
		.freq_limit_name = "oc-limit-freq-lv",
		.core_onoff_name = "",
		.max_lv = BATTERY_OC_LEVEL_NUM - 1,
	},
	[SOC_POWER_THROTTLING] = {
		.max_lv_name = "soc-max-level",
		.freq_limit_name = "soc-limit-freq-lv",
		.core_onoff_name = "soc-core-onoff-lv",
		.max_lv = BATTERY_PERCENT_LEVEL_NUM - 1,
	}
};

static unsigned int cpu_off_by_soc[CORE_NUM];

static LIST_HEAD(pt_policy_list);
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void cpu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level, void *data)
{
	struct cpu_pt_policy *pt_policy;
	int cpu;
	s32 freq_limit;

	if (level > cpu_pt_info[LBAT_POWER_THROTTLING].max_lv)
		return;
	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == LBAT_POWER_THROTTLING) {
			if (level != LOW_BATTERY_LEVEL_0)
				freq_limit = pt_policy->freq_limit[level-1];
			else
				freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
			freq_qos_update_request(&pt_policy->qos_req, freq_limit);
			cpu = pt_policy->cpu;
			trace_low_battery_throttling_cpu_freq(cpu, freq_limit);
		}
	}
}
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void cpu_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level, void *data)
{
	struct cpu_pt_policy *pt_policy;
	s32 freq_limit;
	if (level > cpu_pt_info[OC_POWER_THROTTLING].max_lv)
		return;
	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == OC_POWER_THROTTLING) {
			if (level != BATTERY_OC_LEVEL_0)
				freq_limit = pt_policy->freq_limit[level-1];
			else
				freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
			freq_qos_update_request(&pt_policy->qos_req, freq_limit);
		}
	}
}
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
static void cpu_pt_battery_percent_cb(enum BATTERY_PERCENT_LEVEL_TAG level)
{
	struct cpu_pt_policy *pt_policy;
	struct cpu_pt_priv *pt_info_p = &cpu_pt_info[SOC_POWER_THROTTLING];
	s32 freq_limit;
	int ret, idx = 0, i = 0;

	if (level > cpu_pt_info[SOC_POWER_THROTTLING].max_lv)
		return;

	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == SOC_POWER_THROTTLING) {
			if (level != BATTERY_PERCENT_LEVEL_0)
				freq_limit = pt_policy->freq_limit[level-1];
			else
				freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
			freq_qos_update_request(&pt_policy->qos_req, freq_limit);
		}
	}

	for_each_possible_cpu(i) {
		if (level == BATTERY_PERCENT_LEVEL_0) {
			if (cpu_is_offline(i) && cpu_off_by_soc[i]) {
				ret = CPU_UP(i);
				if (ret)
					pr_notice("failed to bring up cpu:%d\n", i);
				else {
					cpu_off_by_soc[i] = 0;
					pr_notice("PT bring up cpu:%d\n", i);
				}
			}
		} else {
			idx = (level - 1) * CORE_NUM + i;
			if (pt_info_p->core_onoff[idx] == 0 && !cpu_is_offline(i) && !cpu_off_by_soc[i]) {
				ret = CPU_DOWN(i);
				if (ret)
					pr_notice("failed to bring down cpu: %d\n", i);
				else {
					cpu_off_by_soc[i] = 1;
					pr_notice("PT bring down cpu:%d\n", i);
				}

			} else if (pt_info_p->core_onoff[idx] == 1 && cpu_is_offline(i) && cpu_off_by_soc[i]) {
				ret = CPU_UP(i);
				if (ret)
					pr_notice("failed to bring up cpu: %d\n", i);
				else {
					cpu_off_by_soc[i] = 0;
					pr_notice("PT bring up cpu:%d\n", i);
				}
			}
		}
	}
}
#endif
static void __used cpu_limit_default_setting(struct device *dev, enum cpu_pt_type type)
{
	struct device_node *np = dev->of_node;
	int i, max_lv, ret;
	struct cpu_pt_priv *pt_info_p;

	pt_info_p = &cpu_pt_info[type];
	if (type == LBAT_POWER_THROTTLING)
		max_lv = 2;
	else if (type == OC_POWER_THROTTLING)
		max_lv = 1;
	else
		max_lv = 0;
	pt_info_p->max_lv = max_lv;
	if (!pt_info_p->max_lv)
		return;
	pt_info_p->freq_limit = kzalloc(sizeof(u32) * pt_info_p->max_lv * CLUSTER_NUM, GFP_KERNEL);
	pt_info_p->core_onoff = kzalloc(sizeof(u32) * pt_info_p->max_lv * CORE_NUM, GFP_KERNEL);
	for (i = 0; i < CLUSTER_NUM; i++) {
		pt_info_p->freq_limit[i] = CPU_LIMIT_FREQ;
		pt_info_p->core_onoff[i] = 1;
	}

	if (type == LBAT_POWER_THROTTLING) {
		ret = of_property_read_u32_array(np, "lbat_cpu_limit",
			&pt_info_p->freq_limit[0], 3);
		if (ret < 0)
			pr_notice("%s: get lbat cpu limit fail %d\n", __func__, ret);
	} else if (type == OC_POWER_THROTTLING) {
		ret = of_property_read_u32_array(np, "oc_cpu_limit",
			&pt_info_p->freq_limit[0], 3);
		if (ret < 0)
			pr_notice("%s: get oc cpu limit fail %d\n", __func__, ret);
	}
	for (i = 1; i < pt_info_p->max_lv; i++) {
		memcpy(&pt_info_p->freq_limit[i * CLUSTER_NUM], &pt_info_p->freq_limit[0],
			sizeof(u32) * CLUSTER_NUM);
		memcpy(&pt_info_p->core_onoff[i * CORE_NUM], &pt_info_p->core_onoff[0],
			sizeof(u32) * CORE_NUM);
	}
}

static int __used parse_cpu_limit_table(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i, j, k, num, ret;
	struct cpu_pt_priv *pt_info_p;

	char buf[NAME_LENGTH];

	for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
		pt_info_p = &cpu_pt_info[i];
		ret = of_property_read_u32(np, pt_info_p->max_lv_name, &num);
		if (ret < 0) {
			cpu_limit_default_setting(dev, i);
			continue;
		} else if (num <= 0 || num > pt_info_p->max_lv) {
			pt_info_p->max_lv = 0;
			continue;
		}

		pt_info_p->max_lv = num;
		pt_info_p->freq_limit = kzalloc(sizeof(u32) * pt_info_p->max_lv * CLUSTER_NUM, GFP_KERNEL);
		if (!pt_info_p->freq_limit)
			return -ENOMEM;

		pt_info_p->core_onoff = kzalloc(sizeof(u32) * pt_info_p->max_lv * CORE_NUM, GFP_KERNEL);
		if (!pt_info_p->core_onoff)
			return -ENOMEM;

		for (j = 0; j < pt_info_p->max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", pt_info_p->freq_limit_name, j+1);
			if (ret < 0)
				pr_notice("can't merge %s %d\n", pt_info_p->freq_limit_name, j+1);

			ret = of_property_read_u32_array(np, buf,
				&pt_info_p->freq_limit[j * CLUSTER_NUM], CLUSTER_NUM);
			if (ret < 0) {
				pr_notice("%s: get %s fail %d\n", __func__, buf, ret);
				for (k = 0; k < CLUSTER_NUM; k++)
					pt_info_p->freq_limit[j * CLUSTER_NUM + k] = CPU_UNLIMIT_FREQ;
			}

			if (i == SOC_POWER_THROTTLING) {
				memset(buf, 0, sizeof(buf));
				ret = snprintf(buf, sizeof(buf), "%s%d", pt_info_p->core_onoff_name, j+1);
				if (ret < 0)
					pr_notice("can't merge %s %d\n", pt_info_p->core_onoff_name, j+1);
				ret = of_property_read_u32_array(np, buf,
					&pt_info_p->core_onoff[j * CORE_NUM], CORE_NUM);
				if (ret < 0) {
					pr_notice("%s: get %s fail %d set core limit to 1\n", __func__, buf, ret);
					for (k = 0; k < CORE_NUM; k++)
						pt_info_p->core_onoff[j * CORE_NUM + k] = 1;
				}
			}
		}
	}

	return 0;
}
static int mtk_cpu_power_throttling_probe(struct platform_device *pdev)
{
	struct cpufreq_policy *policy;
	struct cpu_pt_policy *pt_policy;
	unsigned int i = 0, j = 0, k = 0;
	int cpu, ret, max_lv;
	s32 *freq_limit_t;

	ret = parse_cpu_limit_table(&pdev->dev);
	if (ret != 0)
		return ret;
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_info("cpu[%d]: failed to get cpufreq policy\n", cpu);
			continue;
		}
		if (policy->cpu == cpu) {
			for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
				pt_policy = kzalloc(sizeof(*pt_policy), GFP_KERNEL);
				if (!pt_policy)
					return -ENOMEM;
				max_lv = (cpu_pt_info[i].max_lv > 1) ? cpu_pt_info[i].max_lv : 1;
				freq_limit_t = kcalloc(max_lv, sizeof(u32), GFP_KERNEL);
				if (!freq_limit_t) {
					kfree(pt_policy);
					return -ENOMEM;
				}

				for (j = 0; j < cpu_pt_info[i].max_lv; j++)
					freq_limit_t[j] = cpu_pt_info[i].freq_limit[j * CLUSTER_NUM + k];
				pt_policy->pt_type = (enum cpu_pt_type)i;
				pt_policy->policy = policy;
				pt_policy->cpu = cpu;
				pt_policy->pt_max_lv = cpu_pt_info[i].max_lv;
				pt_policy->freq_limit = freq_limit_t;
				ret = freq_qos_add_request(&policy->constraints,
					&pt_policy->qos_req, FREQ_QOS_MAX,
					FREQ_QOS_MAX_DEFAULT_VALUE);
				if (ret < 0) {
					pr_notice("%s: Fail to add freq constraint (%d)\n",
						__func__, ret);
					kfree(pt_policy);
					kfree(freq_limit_t);
					return ret;
				}
				list_add_tail(&pt_policy->cpu_pt_list, &pt_policy_list);
			}
			k++;
		}
	}
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	if (cpu_pt_info[LBAT_POWER_THROTTLING].max_lv > 0)
		register_low_battery_notify(&cpu_pt_low_battery_cb, LOW_BATTERY_PRIO_CPU_B, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	if (cpu_pt_info[OC_POWER_THROTTLING].max_lv > 0)
		register_battery_oc_notify(&cpu_pt_over_current_cb, BATTERY_OC_PRIO_CPU_B, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	if (cpu_pt_info[SOC_POWER_THROTTLING].max_lv > 0)
		register_bp_thl_notify(&cpu_pt_battery_percent_cb, BATTERY_PERCENT_PRIO_CPU_B);
#endif

	return 0;
}
static int mtk_cpu_power_throttling_remove(struct platform_device *pdev)
{
	struct cpu_pt_policy *pt_policy, *pt_policy_t;
	list_for_each_entry_safe(pt_policy, pt_policy_t, &pt_policy_list, cpu_pt_list) {
		freq_qos_remove_request(&pt_policy->qos_req);
		cpufreq_cpu_put(pt_policy->policy);
		list_del(&pt_policy->cpu_pt_list);
		kfree(pt_policy);
	}
	return 0;
}
static const struct of_device_id cpu_power_throttling_of_match[] = {
	{ .compatible = "mediatek,cpu-power-throttling", },
	{},
};
MODULE_DEVICE_TABLE(of, cpu_power_throttling_of_match);
static struct platform_driver cpu_power_throttling_driver = {
	.probe = mtk_cpu_power_throttling_probe,
	.remove = mtk_cpu_power_throttling_remove,
	.driver = {
		.name = "mtk-cpu_power_throttling",
		.of_match_table = cpu_power_throttling_of_match,
	},
};
module_platform_driver(cpu_power_throttling_driver);
MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK cpu power throttling driver");
MODULE_LICENSE("GPL");
