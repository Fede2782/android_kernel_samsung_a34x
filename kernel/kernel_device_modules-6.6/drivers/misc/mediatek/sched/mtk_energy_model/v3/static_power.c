// SPDX-License-Identifier: GPL-2.0
/*
 * static_power.c - static power api
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/types.h>
#include "energy_model.h"

#define __LKG_PROCFS__ 0
#define __LKG_DEBUG__ 0
#define __DYN_CHECK__ 0

#define LUT_FREQ			GENMASK(11, 0)
#define LUT_NR_TOTAL_TYPE	GENMASK(7, 0)
#define LUT_NR_CPU_TYPE		GENMASK(11, 8)
#define LUT_NR_DSU_TYPE		GENMASK(15, 12)
#define LUT_NR_TOTAL_TYPE	GENMASK(7, 0)
#define LUT_DSU_WEIGHT		GENMASK(15, 0)
#define LUT_EMI_WEIGHT		GENMASK(31, 16)
#define LUT_ROW_SIZE		0x4

static void __iomem *usram_base, *csram_base, *wl_base, *curve_adj_base, *eemsn_log_base;

struct mtk_mapping mtk_mapping;
struct leakage_data info;
static struct em_base_info *mtk_em_base_info;
static bool wl_support, curve_adj_support;

bool is_wl_support(void)
{
	return wl_support;
}
EXPORT_SYMBOL_GPL(is_wl_support);

static int init_mtk_mapping(void)
{
	unsigned long offset = 0;
	int type = 0;
	u32 data;

	if (!is_wl_support())
		goto disable_wl;

	data = readl_relaxed(wl_base);
	mtk_mapping.total_type = FIELD_GET(LUT_NR_TOTAL_TYPE, data);
	mtk_mapping.nr_cpu_type = FIELD_GET(LUT_NR_CPU_TYPE, data);
	mtk_mapping.nr_dsu_type = FIELD_GET(LUT_NR_DSU_TYPE, data);
	pr_info("%s: %d, %d, %d\n",	__func__,
		mtk_mapping.total_type,
		mtk_mapping.nr_cpu_type,
		mtk_mapping.nr_dsu_type);

	offset += 0x4;
	if (mtk_mapping.total_type > MAX_NR_WL) {
		pr_info("Invalid total type value: %d\n", mtk_mapping.total_type);
		goto disable_wl;
	}

	if (mtk_mapping.total_type < mtk_mapping.nr_cpu_type ||
		mtk_mapping.total_type < mtk_mapping.nr_dsu_type) {
		pr_info("Invalid type value: total_type: %d, nr_cpu_type: %d, nr_dsu_type: %d\n",
				mtk_mapping.total_type,
				mtk_mapping.nr_cpu_type,
				mtk_mapping.nr_dsu_type);
		goto disable_wl;
	}

	mtk_mapping.cpu_to_dsu =
		kcalloc(mtk_mapping.total_type, sizeof(struct mtk_relation),
					GFP_KERNEL);
	if (!mtk_mapping.cpu_to_dsu) {
		kfree(mtk_mapping.cpu_to_dsu);
		return -ENOMEM;
	}

	for (type = 0; type < mtk_mapping.total_type; type++) {
		mtk_mapping.cpu_to_dsu[type].cpu_type = ioread8(wl_base + offset);
		offset += 0x1;
		mtk_mapping.cpu_to_dsu[type].dsu_type = ioread8(wl_base + offset);
		offset += 0x1;
		pr_info("cpu type: %d, dsu type: %d\n",
			mtk_mapping.cpu_to_dsu[type].cpu_type,
			mtk_mapping.cpu_to_dsu[type].dsu_type);
	}

	return 0;

disable_wl:
	mtk_mapping.cpu_to_dsu =
		kcalloc(DEFAULT_NR_TYPE, sizeof(struct mtk_relation),
					GFP_KERNEL);
	if (!mtk_mapping.cpu_to_dsu)
		return -ENOMEM;

	mtk_mapping.total_type = DEFAULT_NR_TYPE;
	mtk_mapping.nr_cpu_type = DEFAULT_NR_TYPE;
	mtk_mapping.nr_dsu_type = DEFAULT_NR_TYPE;
	mtk_mapping.cpu_to_dsu[type].cpu_type = DEFAULT_TYPE;
	mtk_mapping.cpu_to_dsu[type].dsu_type = DEFAULT_TYPE;

	return 0;
}

static int check_wl_support(void)
{
	struct device_node *np;
	struct platform_device *pdev;
	int ret = 0, support;
	struct resource *res;

	np = of_find_node_by_name(NULL, "wl-info");
	if (!np) {
		pr_info("failed to find node @ %s\n", __func__);
		goto wl_disable;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_info("failed to find pdev @ %s\n", __func__);
		of_node_put(np);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "wl-support", &support);
	if (ret || support == 0)
		goto wl_disable;

	res = platform_get_resource(pdev, IORESOURCE_MEM, WL_MEM_RES_IND);
	if (!res) {
		ret = -ENODEV;
		goto no_res;
	}

	wl_base = ioremap(res->start, resource_size(res));
	if (!wl_base) {
		pr_info("%s failed to map resource %pR\n", __func__, res);
		ret = -ENOMEM;
		goto release_region;
	}

	wl_support = true;
	pr_info("%s: wl supports\n", __func__);
	of_node_put(np);

	return 0;
release_region:
	release_mem_region(res->start, resource_size(res));
no_res:
	pr_info("%s can't get mem resource %d\n", __func__, WL_MEM_RES_IND);
wl_disable:
	pr_info("%s wl-support is disabled: %d\n", __func__, ret);
	wl_support = false;
	of_node_put(np);

	return ret;
}

int check_curve_adj_support(struct device_node *dvfs_node)
{
	struct device_node *curve_adj_node;

	curve_adj_node = of_parse_phandle(dvfs_node, "curve-adj-base", 0);
	if (curve_adj_node) {
		curve_adj_base = of_iomap(curve_adj_node, 0);
		if (!curve_adj_base) {
			curve_adj_support = false;
			return -ENODEV;
		}

		curve_adj_support = true;
		pr_info("%s: curve adjustment enable\n", __func__);
	} else
		curve_adj_support = false;

	return 0;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		int cpu_cluster_id = topology_cluster_id(cpu);

		if (cpu_cluster_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

struct em_base_info *mtk_get_em_base_info(void)
{
	return mtk_em_base_info;
}
EXPORT_SYMBOL(mtk_get_em_base_info);

unsigned int get_nr_cpus(void)
{
	unsigned int cpu_idx = 0, nr_cpus = 0;

	for_each_possible_cpu(cpu_idx)
		nr_cpus++;

	return nr_cpus;
}

unsigned int *get_cpu_cluster_id_array(void)
{
	unsigned int cpu_idx = 0;
	unsigned int *cpu_cluster_id;

	cpu_cluster_id = kcalloc(get_nr_cpus(), sizeof(unsigned int), GFP_KERNEL);

	for_each_possible_cpu(cpu_idx)
		cpu_cluster_id[cpu_idx] = topology_cluster_id(cpu_idx);

	return cpu_cluster_id;
}

cpumask_t **get_cpu_cluster_mask_array(void)
{
	unsigned int cpu_idx = 0;
	cpumask_t **cpu_cluster_mask;

	cpu_cluster_mask = kcalloc(get_nr_cpus(), sizeof(cpumask_t *), GFP_KERNEL);

	for_each_possible_cpu(cpu_idx)
		cpu_cluster_mask[cpu_idx] = topology_cluster_cpumask(cpu_idx);

	return cpu_cluster_mask;
}

static int mtk_static_power_probe(struct platform_device *pdev)
{
#if __LKG_DEBUG__
	unsigned int i, power;
#endif
	int ret = 0, err = 0;
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;
	struct resource *usram_res, *csram_res, *eem_res;

	pr_info("[Static Power v3.0.0] Start to parse DTS\n");
	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dvfs_node);
	if (pdev_temp == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	usram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (usram_res) {
		usram_base = ioremap(usram_res->start, resource_size(usram_res));
		pr_info("%s usram_base: %p\n", __func__, usram_base);
	} else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);
	if (csram_res) {
		csram_base = ioremap(csram_res->start, resource_size(csram_res));
		pr_info("%s csram_base: %p\n", __func__, csram_base);
	} else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	eem_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 2);
	if (eem_res) {
		eemsn_log_base = ioremap(eem_res->start, resource_size(eem_res));
		pr_info("%s eemsn_log_base: %p\n", __func__, eemsn_log_base);
	} else {
		ret = -ENODEV;
		pr_info("%s can't get EEM resource, ret: %d\n", __func__, ret);
		goto error;
	}

	ret = check_curve_adj_support(dvfs_node);
	if (ret)
		pr_info("%s: Failed to check curve adj support: %d\n", __func__, ret);

	ret = check_wl_support();
	if (ret)
		pr_info("%s: Failed to check wl support: %d\n", __func__, ret);

	ret = init_mtk_mapping();
	if (ret) {
		pr_info("%s: Failed to init mtk mapping: %d\n", __func__, ret);
		goto error;
	}

	pr_info("[Static Power v3.0.0] Parse DTS Done\n");

	mtk_em_base_info = kcalloc(EM_BASE_INFO_COUNT, sizeof(struct em_base_info), GFP_KERNEL);
	if (!mtk_em_base_info) {
		ret = -ENOMEM;
		goto error;
	}

	mtk_em_base_info->usram_base = usram_base;
	mtk_em_base_info->csram_base = csram_base;
	mtk_em_base_info->wl_base = wl_base;
	mtk_em_base_info->curve_adj_base = curve_adj_base;
	mtk_em_base_info->eemsn_log = eemsn_log_base;
	mtk_em_base_info->curve_adj_support = curve_adj_support;
	mtk_em_base_info->wl_support = wl_support;
	mtk_em_base_info->mtk_mapping = mtk_mapping;
	mtk_em_base_info->cpu_cluster_id = get_cpu_cluster_id_array();
	mtk_em_base_info->cpu_cluster_mask = get_cpu_cluster_mask_array();

	info.init = 0x5A5A;

	pr_info("[Static Power v3.0.0] Prepare MTK EM info done\n");

	return ret;

error:
	if (usram_base)
		iounmap(usram_base);
	if (csram_base)
		iounmap(csram_base);
	if (eemsn_log_base)
		iounmap(eemsn_log_base);
	return ret;
}

static const struct of_device_id mtk_static_power_match[] = {
	{ .compatible = "mediatek,mtk-lkg" },
	{}
};

static struct platform_driver mtk_static_power_driver = {
	.probe = mtk_static_power_probe,
	.driver = {
		.name = "mtk-lkg",
		.of_match_table = mtk_static_power_match,
	},
};

int __init mtk_static_power_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_static_power_driver);
	return ret;
}

MODULE_DESCRIPTION("MTK static power Platform Driver v2.1.1");
MODULE_AUTHOR("Chung-Kai Yang <Chung-kai.Yang@mediatek.com>");
MODULE_LICENSE("GPL");
