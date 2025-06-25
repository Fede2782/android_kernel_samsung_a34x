// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/* system includes */
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interconnect.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/time.h>
#include <linux/topology.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_ipi.h"
#include "mtk_cm_mgr_common.h"
#include "mtk_cm_mgr_mt6991.h"

/*****************************************************************************
 *  Variables
 *****************************************************************************/
spinlock_t cm_mgr_lock;
static int cm_mgr_idx;
static int cm_chip_ver;
static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];

static struct cm_mgr_hook local_hk;

u32 *cm_mgr_perfs;

void __iomem *csram_base;
u32 cm_vendor_id, cm_mem_info, cm_num_opp, nr_vc0, nr_vc1, nr_bound;
u32 *cm_mem_support, *cm_mem_dynamic, *cm_mem_capacity, *cm_mem_bound;
/*****************************************************************************
 *  Platform functions
 *****************************************************************************/
static void cm_get_chipid(void)
{
	struct device_node *dn = of_find_node_by_path("/chosen");
	struct tag_chipid *chipid;

	if (!dn)
		dn = of_find_node_by_path("/chosen@0");
	if (dn) {
		chipid = (struct tag_chipid *) of_get_property(dn,"atag,chipid", NULL);
		if (!chipid)
			pr_info("%s(%d): could not found atag,chipid in chosen\n",  __func__, __LINE__);
		else
			cm_chip_ver = (int)chipid->sw_ver;
	} else
		pr_info("%s(%d): chosen node not found in device tree\n", __func__, __LINE__);
}

static void cm_hw_update(void)
{
	unsigned int opp, offset, type, value,i, l;
	//Dynamic Table
	if (cm_mem_dynamic) {
		type = 1;
		l = cm_num_opp * nr_vc0;
		for(i=l * cm_vendor_id; i< l * (cm_vendor_id + 1); i++) {
			opp = (i - l * cm_vendor_id) / nr_vc0;
			offset = i % nr_vc0;
			value = (type << 30) | ((opp & 0xF) << 26) | ((offset & 0x7) << 23) | *(cm_mem_dynamic + i);
			cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_THD, value);
		}
		kfree(cm_mem_dynamic);
	}
	//Capacity Table
	if (cm_mem_capacity) {
		type = 2;
		l = cm_num_opp * nr_vc1;
		for(i = 0; i< l; i++) {
			opp = i / nr_vc1;
			offset = i % nr_vc1;
			value = (type << 30) | ((opp & 0xF) << 26) | ((offset & 0x7) << 23) | *(cm_mem_capacity + i);
			cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_THD, value);
		}
		kfree(cm_mem_capacity);
	}
	//Bound Table
	if (cm_mem_bound) {
		type = 3;
		l = nr_bound / cm_num_opp;
		for (i = 0; i<nr_bound; i++) {
			opp = i / l;
			if (i % l == cm_vendor_id) {
				offset = 0;
			} else {
				if (i % l == l-1)
					offset = 1;
			}
			if (offset == 0 || offset ==1) {
				value = (type << 30) | ((opp & 0xF) << 26) |
					((offset & 0x7) << 23) | *(cm_mem_bound + i);
				cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_THD, value);
			}
		}
		kfree(cm_mem_bound);
	}
	//Finish Table
	value = (0xF << 26) | (0x7 << 23) | (0xbabe + i);
	cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_THD, value);
}

static int cm_hw_setting(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	unsigned int tmp, nr_vendor, nr_support;
	struct device_node *node = pdev->dev.of_node;
	/*Read Support Vendoint ret = 0, i = 0;r*/
	nr_vendor = of_property_count_elems_of_size(node,  "cm_mem_support", sizeof(u32));
	if (nr_vendor > 0) {
		ret = of_property_read_u32_array(node, "cm_mem_support", cm_mem_support, nr_vendor);
		if (ret) {
			kfree(cm_mem_support);
			pr_info("Find cm_mem_support failed!\n");
			goto ERROR;
		}
	} else {
		pr_info("Find cm_mem_support size !\n");
		ret = -ENOMEM;
		goto ERROR;
	}
	for(i = 0; i < nr_vendor; i++) {
		if (cm_mem_info == cm_mem_support[i]) {
			cm_vendor_id = i;
			break;
		}
	}
	kfree(cm_mem_support);

	/*Read Knot Information*/
	nr_support = of_property_count_elems_of_size(node,  "cm_mem_vc", sizeof(u32));
	if (nr_support <= 0) {
		pr_info("Find cm_mem_vc size !\n");
		ret = -ENOMEM;
		goto ERROR;
	} else {
		ret = of_property_read_u32_index(node, "cm_mem_vc", cm_vendor_id, &nr_vc0);
		if (ret) {
			pr_info("Find cm_mem_vc failed !\n");
			goto ERROR;
		}
		ret = of_property_read_u32_index(node, "cm_mem_vc", nr_support, &nr_vc1);
		if (ret) {
			pr_info("Find cm_mem_vc of capcity failed !\n");
			goto ERROR;
		}
	}

	/*Read Dynamic */
	tmp = of_property_count_elems_of_size(node,  "cm_mem_dynamic", sizeof(u32));
	if (tmp >0){
		ret = of_property_read_u32_array(node, "cm_mem_dynamic", cm_mem_dynamic, tmp);
		if (ret) {
			kfree(cm_mem_dynamic);
			pr_info("Find cm_mem_dynamic failed !\n");
			goto ERROR;
		}
	} else {
		pr_info("Find cm_mem_dynamic failed !\n");
		ret = -ENOMEM;
		goto ERROR;
	}

	/*Read Capacity*/
	tmp = of_property_count_elems_of_size(node,  "cm_mem_capacity", sizeof(u32));
	if (tmp >0){
		ret = of_property_read_u32_array(node, "cm_mem_capacity", cm_mem_capacity, tmp);
		if (ret) {
			kfree(cm_mem_capacity);
			pr_info("Find cm_mem_capacity failed !\n");
			goto ERROR;
		}
	} else {
		pr_info("Find cm_mem_capacity failed !\n");
		ret = -ENOMEM;
		goto ERROR;
	}

	/*Read Bound*/
	nr_bound = of_property_count_elems_of_size(node,  "cm_mem_bound", sizeof(u32));
	if (nr_bound >0){
		ret = of_property_read_u32_array(node, "cm_mem_bound", cm_mem_bound, nr_bound);
		if (ret) {
			kfree(cm_mem_bound);
			pr_info("Find cm_mem_bound failed !\n");
			goto ERROR;
		}
	} else {
		pr_info("Find cm_mem_bound failed !\n");
		ret = -ENOMEM;
		goto ERROR;
	}

	/*Send Finish Signal*/
	cm_hw_update();
ERROR:
	return ret;
}

static int cm_get_dram_info(void)
{
	int ret = 0, i;
	struct device_node *dn = NULL;
	struct platform_device *pdev = NULL;
	struct dramc_dev_t *dramc_dev_ptr;
	struct mr_info_t *mr_info_ptr;
	/* get dram info node */
	//0. find node
	dn = of_find_node_by_name(NULL, "dramc");
	if (!dn) {
		ret = -ENOMEM;
		pr_info("Find dramc node failed!\n");
		goto ERROR;
	}
	pdev = of_find_device_by_node(dn);
	of_node_put(dn);
	if (!pdev) {
		ret = -ENODEV;
		pr_info("dramc is not ready\n");
		goto ERROR;
	}

	//1. get dramc dev info
	dramc_dev_ptr = (struct dramc_dev_t *)platform_get_drvdata(pdev);
	if (!dramc_dev_ptr){
		ret = -ENOMEM;
		pr_info("find dramc dev ptr failed\n");
		goto ERROR;
	}
	mr_info_ptr = dramc_dev_ptr->mr_info_ptr;
	for (ret = 0, i = 0; i < dramc_dev_ptr->mr_cnt; i++) {
		if (mr_info_ptr[i].mr_index == 5)
			cm_mem_info = (u32) mr_info_ptr[i].mr_value;
		pr_info("mr %d: 0x%x\n", mr_info_ptr[i].mr_index, mr_info_ptr[i].mr_value);
	}

ERROR:
	return ret;
}

static int cm_get_base_addr(void)
{
	int ret = 0;
	struct device_node *dn = NULL;
	struct platform_device *pdev = NULL;
	struct resource *csram_res = NULL;

	/* get cpufreq driver base address */
	dn = of_find_node_by_name(NULL, "cpuhvfs");
	if (!dn) {
		ret = -ENOMEM;
		pr_info("find cpuhvfs node failed\n");
		goto ERROR;
	}

	pdev = of_find_device_by_node(dn);
	of_node_put(dn);
	if (!pdev) {
		ret = -ENODEV;
		pr_info("cpuhvfs is not ready\n");
		goto ERROR;
	}

	csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!csram_res) {
		ret = -ENODEV;
		pr_info("cpuhvfs resource is not found\n");
		goto ERROR;
	}

	csram_base = ioremap(csram_res->start, resource_size(csram_res));
	if (IS_ERR_OR_NULL((void *)csram_base)) {
		ret = -ENOMEM;
		pr_info("find csram base failed\n");
		goto ERROR;
	}

ERROR:
	return ret;
}

unsigned int csram_read(unsigned int offs)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return 0;
	return __raw_readl(csram_base + (offs));
}

void csram_write(unsigned int offs, unsigned int val)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return;
	__raw_writel(val, csram_base + (offs));
}

u32 cm_mgr_get_perfs_mt6991(int num)
{
	if (num < 0 || num >= cm_mgr_get_num_perf())
		return 0;
	return cm_mgr_perfs[num];
}

static int cm_mgr_check_dram_type(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_DRAMC)
	int ddr_type = mtk_dramc_get_ddr_type();
	int ddr_hz = mtk_dramc_get_steps_freq(0);

	if (ddr_type == TYPE_LPDDR5)
		cm_mgr_idx = CM_MGR_LP5;
	else if (ddr_type == TYPE_LPDDR5X)
		cm_mgr_idx = CM_MGR_LP5X;
	else {
		cm_mgr_idx = -1;
		ret = -1;
	}
	pr_info("%s(%d): ddr_type %d, ddr_hz %d, cm_mgr_idx %d\n", __func__,
		__LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("%s(%d): NO CONFIG_MTK_DRAMC !!! set cm_mgr_idx to %d\n",
		__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC */
	if (cm_mgr_idx >= 0)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_TYPE, cm_mgr_idx);
	return ret;
}

static void check_cm_mgr_status_mt6991(unsigned int cluster, unsigned int freq,
					   unsigned int idx)
{
	unsigned int bcpu_opp_max;
	unsigned long spinlock_save_flag;

	if (!cm_mgr_get_enable())
		return;

	spin_lock_irqsave(&cm_mgr_lock, spinlock_save_flag);

	prev_freq_idx[cluster] = idx;
	prev_freq[cluster] = freq;

	if (prev_freq_idx[CM_MGR_B] < prev_freq_idx[CM_MGR_BB])
		bcpu_opp_max = prev_freq_idx[CM_MGR_B];
	else
		bcpu_opp_max = prev_freq_idx[CM_MGR_BB];

	spin_unlock_irqrestore(&cm_mgr_lock, spinlock_save_flag);
	cm_mgr_update_dram_by_cpu_opp(bcpu_opp_max);
}

static void cm_mgr_thermal_hint(int is_thermal)
{
	pr_info("%s(%d): is_thermal %d.\n", __func__, __LINE__, is_thermal);
	cm_mgr_set_perf_mode_enable(!is_thermal);
	cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_ENABLE,
				   cm_mgr_get_perf_mode_enable());
}

static int cm_mgr_check_dts_setting_mt6991(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
	int i = 0;
#endif /* CONFIG_MTK_DVFSRC */
	int ret = 0;
	int temp = 0;
	struct device_node *node = pdev->dev.of_node;
	struct icc_path *bw_path;

	ret = of_count_phandle_with_args(node, "required-opps", NULL);
	if (ret > 0) {
		cm_mgr_set_num_perf(ret);
		pr_info("%s(%d): required_opps count %d\n", __func__, __LINE__,
			ret);
		cm_num_opp = ret;
	} else {
		ret = -1;
		pr_info("%s(%d): fail to get required_opps count from dts.\n",
			__func__, __LINE__);
		goto ERROR;
	}

	cm_mgr_perfs = devm_kzalloc(&pdev->dev, ret * sizeof(u32), GFP_KERNEL);
	if (!cm_mgr_perfs) {
		ret = -ENOMEM;
		pr_info("%s(%d): fail to kzalloc cm_mgr_perfs.\n", __func__,
			__LINE__);
		goto ERROR;
	}

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
	for (i = 0; i < ret; i++)
		cm_mgr_perfs[i] = dvfsrc_get_required_opp_peak_bw(node, i);
#endif /* CONFIG_MTK_DVFSRC */

	ret = of_property_read_u32(node, "cm-mgr-num-array", &temp);
	if (ret) {
		pr_info("%s(%d): fail to get cm_mgr_num_array from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	} else {
		cm_mgr_set_num_array(temp);
		pr_info("%s(%d): cm_mgr_num_array %d\n", __func__, __LINE__,
			cm_mgr_get_num_array());
	}

	bw_path = of_icc_get(&pdev->dev, "cm-perf-bw");
	if (IS_ERR(bw_path)) {
		ret = -1;
		dev_info(&pdev->dev, "get cm_perf_bw fail\n");
		cm_mgr_set_bw_path(NULL);
		pr_info("%s(%d): fail to get cm_perf_bw path from dts.\n",
			__func__, __LINE__);
		goto ERROR;
	} else
		cm_mgr_set_bw_path(bw_path);

	ret = of_property_read_u32(node, "cm-perf-mode-enable", &temp);
	if (ret) {
		pr_info("%s(%d): fail to get cm_perf_mode_enable from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	} else {
		cm_mgr_set_perf_mode_enable(temp);
		pr_info("%s(%d): cm_perf_mode_enable %d\n", __func__, __LINE__,
			cm_mgr_get_perf_mode_enable());
	}

	ret = of_property_read_u32(node, "cm-perf-mode-ceiling-opp", &temp);
	if (ret) {
		pr_info("%s(%d): fail to get cm_perf_mode_ceiling_opp from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	} else {
		cm_mgr_set_perf_mode_ceiling_opp(temp);
		pr_info("%s(%d): cm_perf_mode_ceiling_opp %d\n", __func__,
			__LINE__, cm_mgr_get_perf_mode_ceiling_opp());
	}

	ret = of_property_read_u32(node, "cm-perf-mode-thd", &temp);
	if (ret) {
		pr_info("%s(%d): fail to get cm_perf_mode_thd from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	} else {
		cm_mgr_set_perf_mode_thd(temp);
		pr_info("%s(%d): cm_perf_mode_thd %d\n", __func__, __LINE__,
			cm_mgr_get_perf_mode_thd());
	}

	return 0;

ERROR:
	return ret;
}

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret = 0;

	spin_lock_init(&cm_mgr_lock);
	ret = cm_mgr_check_dts_setting_mt6991(pdev);
	if (ret) {
		pr_info("%s(%d): fail to get platform data from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	}

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("%s(%d): fail to get common data from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	}

	cm_get_chipid();

	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("%s(%d): fail to common init. ret %d\n", __func__,
			__LINE__, ret);
		goto ERROR;
	}

	ret = cm_mgr_check_dram_type();
	if (ret) {
		pr_info("%s(%d): fail to check dram type. ret %d\n", __func__,
			__LINE__, ret);
		goto ERROR;
	}

	ret = cm_get_dram_info();
	if (ret) {
		pr_info("%s(%d): fail to check dram info. ret %d\n", __func__,
			__LINE__, ret);
	}

	ret = cm_hw_setting(pdev);
	if (ret) {
		pr_info("%s(%d): fail to update hw param. ret %d\n", __func__,
			__LINE__, ret);
	}

	ret = cm_get_base_addr();
	if (ret) {
		pr_info("%s(%d): fail to get cm csram base. ret %d\n", __func__,
			__LINE__, ret);
		goto ERROR;
	}

	local_hk.cm_mgr_get_perfs = cm_mgr_get_perfs_mt6991;
	local_hk.check_cm_mgr_status = check_cm_mgr_status_mt6991;

	cm_mgr_register_hook(&local_hk);
	dev_pm_genpd_set_performance_state(&pdev->dev, 0);

	cm_thermal_hint_register(cm_mgr_thermal_hint);

	cm_mgr_get_sspm_version();

	cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_ENABLE,
				   cm_mgr_get_perf_mode_enable());
	cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_CEILING_OPP,
				   cm_mgr_get_perf_mode_ceiling_opp());
	cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_THD,
				   cm_mgr_get_perf_mode_thd());
	cm_mgr_to_sspm_command(IPI_CM_MGR_CHIP_VER, cm_chip_ver);
	cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE, cm_mgr_get_enable());

	pr_info("%s(%d): platform-cm_mgr_probe Done.\n", __func__, __LINE__);

	return 0;

ERROR:
	return ret;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_unregister_hook(&local_hk);
	cm_thermal_hint_unregister();
	cm_mgr_common_exit();
	icc_put(cm_mgr_get_bw_path());

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{
		.compatible = "mediatek,mt6991-cm_mgr",
	},
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6991-cm_mgr", 0 },
	{},
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove = platform_cm_mgr_remove,
	.driver = {
		.name = "mt6991-cm_mgr",
		.owner = THIS_MODULE,
		.of_match_table = platform_cm_mgr_of_match,
	},
	.id_table = platform_cm_mgr_id_table,
};

/*
 * driver initialization entry point
 */
static int __init platform_cm_mgr_init(void)
{
	return platform_driver_register(&mtk_platform_cm_mgr_driver);
}

static void __exit platform_cm_mgr_exit(void)
{
	platform_driver_unregister(&mtk_platform_cm_mgr_driver);
	pr_info("%s(%d): platform-cm_mgr Exit.\n", __func__, __LINE__);
}

subsys_initcall(platform_cm_mgr_init);
module_exit(platform_cm_mgr_exit);

MODULE_SOFTDEP("pre: thermal_interface.ko");
MODULE_DESCRIPTION("Mediatek cm_mgr driver");
MODULE_AUTHOR("Carlos Hung<carlos.hung@mediatek.com>");
MODULE_LICENSE("GPL");
