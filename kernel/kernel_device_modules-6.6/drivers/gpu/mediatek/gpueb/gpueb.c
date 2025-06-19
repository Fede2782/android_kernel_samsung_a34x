// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb.c
 * @brief   GPUEB driver init and probe
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/random.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <mboot_params.h>

#include "gpueb_common.h"
#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "gpueb_logger.h"
#include "gpueb_reserved_mem.h"
#include "gpueb_plat_service.h"
#include "gpueb_hwvoter_dbg.h"
#include "gpueb_debug.h"
#include "gpueb_timesync.h"
#include "gpueb_common.h"
#include "ghpm_wrapper.h"

/*
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */

static int __gpueb_pdrv_probe(struct platform_device *pdev);

/*
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static struct workqueue_struct *gpueb_logger_workqueue;
static void __iomem *g_gpueb_gpr_base;
static void __iomem *g_gpueb_cfgreg_base;
static void __iomem *g_mfg0_pwr_con;

#if IS_ENABLED(CONFIG_PM)
static int gpueb_suspend(struct device *dev)
{
	gpueb_timesync_suspend();
	return 0;
}

static int gpueb_resume(struct device *dev)
{
	gpueb_timesync_resume();
	return 0;
}

static const struct dev_pm_ops gpueb_dev_pm_ops = {
	.suspend = gpueb_suspend,
	.resume  = gpueb_resume,
};
#endif

static const struct of_device_id g_gpueb_of_match[] = {
	{ .compatible = "mediatek,gpueb" },
	{ /* sentinel */ }
};

static struct platform_driver g_gpueb_pdrv = {
	.probe = __gpueb_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpueb",
		.owner = THIS_MODULE,
		.of_match_table = g_gpueb_of_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &gpueb_dev_pm_ops,
#endif
	},
};

const struct file_operations gpueb_log_file_ops = {
	.owner = THIS_MODULE,
	.read = gpueb_log_if_read,
	.open = gpueb_log_if_open,
	.poll = gpueb_log_if_poll,
};

static struct miscdevice gpueb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gpueb",
	.fops = &gpueb_log_file_ops
};

static int gpueb_create_files(void)
{
	int ret = 0;

	ret = misc_register(&gpueb_device);
	if (unlikely(ret != 0)) {
		gpueb_log_i(GPUEB_TAG, "misc register failed");
		return ret;
	}

	ret = device_create_file(gpueb_device.this_device,
			&dev_attr_gpueb_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}

void __iomem *gpueb_get_gpr_base(void)
{
	return g_gpueb_gpr_base;
}
EXPORT_SYMBOL(gpueb_get_gpr_base);

void __iomem *gpueb_get_gpr_addr(enum gpueb_sram_gpr_id gpr_id)
{
	return g_gpueb_gpr_base + gpr_id * SRAM_GPR_SIZE_4B;
}
EXPORT_SYMBOL(gpueb_get_gpr_addr);

void __iomem *gpueb_get_cfgreg_base(void)
{
	return g_gpueb_cfgreg_base;
}
EXPORT_SYMBOL(gpueb_get_cfgreg_base);

int get_mfg0_pwr_con(void)
{
	return readl(g_mfg0_pwr_con);
}
EXPORT_SYMBOL(get_mfg0_pwr_con);

int mfg0_pwr_sta(void)
{
	return ((readl(g_mfg0_pwr_con) & MFG0_PWR_ACK_BITS) == MFG0_PWR_ACK_BITS)?
		MFG0_PWR_ON : MFG0_PWR_OFF;
}
EXPORT_SYMBOL(mfg0_pwr_sta);

int is_gpueb_wfi(void)
{
	return ((readl(GPUEB_CFGREG_MDSP_CFG) & GPUEB_ON_WFI_BITS) == GPUEB_ON_WFI_BITS)? 1: 0;
}
EXPORT_SYMBOL(is_gpueb_wfi);

/*
 * GPUEB driver probe
 */
static int __gpueb_pdrv_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int gpueb_support = 0;
	unsigned int gpueb_logger_support = 0;
	struct device_node *node;
	struct resource *res = NULL;

	gpueb_log_i(GPUEB_TAG, "GPUEB driver probe start");

	node = of_find_matching_node(NULL, g_gpueb_of_match);
	if (!node)
		gpueb_log_i(GPUEB_TAG, "find GPUEB node failed");

	of_property_read_u32(pdev->dev.of_node, "gpueb-support",
			&gpueb_support);
	if (gpueb_support == 0) {
		gpueb_log_i(GPUEB_TAG, "Bypass the GPUEB driver probe");
		return 0;
	}

	/* get gpueb gpr base*/
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_gpr_base");
	if (unlikely(!res)) {
		gpueb_log_i(GPUEB_TAG, "fail to get resource GPUEB_GPR_BASE");
		goto err;
	}
	g_gpueb_gpr_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_gpr_base)) {
		gpueb_log_i(GPUEB_TAG, "fail to ioremap gpr base: 0x%llx", (u64) res->start);
		goto err;
	}

	/* get gpueb_cfgreg_base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_cfgreg_base");
	if (unlikely(!res)) {
		gpueb_log_i(GPUEB_TAG, "fail to get resource GPUEB_CFGREG_BASE");
		goto err;
	}
	g_gpueb_cfgreg_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_cfgreg_base)) {
		gpueb_log_i(GPUEB_TAG, "fail to ioremap cfgreg base: 0x%llx", (u64) res->start);
		goto err;
	}

	/* get mfg0_pwr_con */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg0_pwr_con");
	if (unlikely(!res)) {
		gpueb_log_i(GPUEB_TAG, "fail to get resource mfg0_pwr_con");
		goto err;
	}
	g_mfg0_pwr_con = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (unlikely(!g_mfg0_pwr_con)) {
		gpueb_log_i(GPUEB_TAG, "fail to ioremap mfg0_pwr_con: 0x%llx", (u64) res->start);
		goto err;
	}

	ret = gpueb_ipi_init(pdev);
	if (ret != 0)
		gpueb_log_i(GPUEB_TAG, "ipi init fail");

	ret = gpueb_reserved_mem_init(pdev);
	if (ret != 0)
		gpueb_log_i(GPUEB_TAG, "reserved mem init fail");

	of_property_read_u32(pdev->dev.of_node, "gpueb-logger-support",
			&gpueb_logger_support);
	if (gpueb_logger_support == 1) {
		gpueb_logger_workqueue = create_singlethread_workqueue("GPUEB_LOG_WQ");
		if (gpueb_logger_init(pdev,
				gpueb_get_reserve_mem_virt(0),
				gpueb_get_reserve_mem_size(0)) == -1) {
			gpueb_log_i(GPUEB_TAG, "logger init fail");
			goto err;
		}

		ret = gpueb_create_files();
		if (unlikely(ret != 0)) {
			gpueb_log_i(GPUEB_TAG, "create files fail");
			goto err;
		}
	} else {
		gpueb_log_i(GPUEB_TAG, "gpueb no logger support.");
	}

	/* init gpufreq debug */
	gpueb_debug_init(pdev);

	ret = gpueb_timesync_init();
	if (ret) {
		gpueb_log_e(GPUEB_TAG, "GPUEB timesync init fail, ret=%d", ret);
		goto err;
	}

#if !IPI_TEST
	gpueb_hw_voter_dbg_init();
#endif

	ghpm_wrapper_init(pdev);

	gpueb_log_i(GPUEB_TAG, "GPUEB driver probe done");

	return 0;

err:
	return -1;
}

/*
 * Register the GPUEB driver
 */
static int __init __gpueb_init(void)
{
	int ret = 0;

	gpueb_log_d(GPUEB_TAG, "start to initialize gpueb driver");

	// Register platform driver
	ret = platform_driver_register(&g_gpueb_pdrv);
	if (ret)
		gpueb_log_i(GPUEB_TAG, "fail to register gpueb driver");

	return ret;
}

/*
 * Unregister the GPUEB driver
 */
static void __exit __gpueb_exit(void)
{
	platform_driver_unregister(&g_gpueb_pdrv);
}

module_init(__gpueb_init);
module_exit(__gpueb_exit);

MODULE_DEVICE_TABLE(of, g_gpueb_of_match);
MODULE_DESCRIPTION("MediaTek GPUEB-PLAT driver");
MODULE_LICENSE("GPL");
