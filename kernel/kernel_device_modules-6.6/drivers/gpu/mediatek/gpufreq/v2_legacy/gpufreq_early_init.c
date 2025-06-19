// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */
/**
 * @brief File used to enable vgpu and vsram
 * MFG0 and MFG1 will be enabled during preloader.
 * @See vendor/mediatek/proprietary/bootable/bootloader/preloader/platform/mt6877/src/drivers/pll.c
 * Vgpu and vsram will be disabled 30s later after regulator init
 * complete.
 * VGPU buck disable when MFG0/MFG1 still on will result GPU HW behave abnormally.
 * @see kernel-6.6/drivers/regulator/core.c
 * This will result in gpu check bus idle during probe.
 * So we add a gpufreq early init ko to enable vgpu and vsram at an
 * early time and disable vgpu and vsram until gpufreq probe done.
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <gpufreq_early_init.h>
#include <gpufreq_v2_legacy.h>
#include <gpufreq_common_legacy.h>

static int __gpufreq_early_init_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_early_init_pdrv_remove(struct platform_device *pdev);
static int __gpufreq_early_init_init_pmic(struct platform_device *pdev);
static int __gpufreq_early_init_buck_control(enum gpufreq_power_state power);

struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};
static struct g_pmic_info *g_pmic;

static const struct of_device_id g_gpufreq_early_init_of_match[] = {
	{ .compatible = "mediatek,gpufreq_early_init" },
	{ /* sentinel */ }
};

static struct platform_driver g_gpufreq_early_init_pdrv = {
	.probe = __gpufreq_early_init_pdrv_probe,
	.remove = __gpufreq_early_init_pdrv_remove,
	.driver = {
		.name = "gpufreq_early_init",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_early_init_of_match,
	},
};

/* API: gpufreq_early_init driver probe */
static int __gpufreq_early_init_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;
	struct device_node *node;

	GPUFREQ_LOGI("start to probe gpufreq_early_init platform driver");
	node = of_find_matching_node(NULL, g_gpufreq_early_init_of_match);
	if (!node)
		GPUFREQ_LOGE("@%s: find GPU early_init node failed\n", __func__);

	__gpufreq_early_init_init_pmic(pdev);
	/* control Buck */
	ret = __gpufreq_early_init_buck_control(GPU_PWR_ON);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
		ret = GPUFREQ_EINVAL;
	}
	return ret;
}

static int __gpufreq_early_init_init_pmic(struct platform_device *pdev)
{
	int ret = 0;

	g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL) {
		GPUFREQ_LOGE("@%s g_pmic is null\n",__func__);
		return -ENOMEM;
	}

	g_pmic->reg_vgpu = regulator_get(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		GPUFREQ_LOGE("@%s cannot get VGPU\n",__func__);
		return PTR_ERR(g_pmic->reg_vgpu);
	}

	g_pmic->reg_vsram_gpu = regulator_get(&pdev->dev, "vsram-gpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		GPUFREQ_LOGE("@%s cannot get VSRAM_GPU\n",__func__);
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}

	return ret;
}

static int __gpufreq_early_init_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGD("@%s: power=%d\n",__func__,power);
	/* power on */
	if (power == GPU_PWR_ON) {
		if(regulator_is_enabled(g_pmic->reg_vgpu)){
			GPUFREQ_LOGD("@%s g_pmic->reg_vgpu is already enabled\n",
					__func__);
		}else{
			GPUFREQ_LOGD("@%s g_pmic->reg_vgpu is disabled before enable\n",
					__func__);
		}
		ret = regulator_enable(g_pmic->reg_vgpu);
		if (unlikely(ret))
			__gpufreq_abort("fail to enable VGPU (%d)", ret);

		if(regulator_is_enabled(g_pmic->reg_vsram_gpu)){
			GPUFREQ_LOGD("@%s g_pmic->reg_vsram_gpu is already enabled\n",
					__func__);
		}else{
			GPUFREQ_LOGD("@%s g_pmic->reg_vsram_gpu is disabled before enable\n",
					__func__);
		}
		ret = regulator_enable(g_pmic->reg_vsram_gpu);
		if (unlikely(ret))
			__gpufreq_abort("fail to enable VSRAM_GPU (%d)",ret);

	/* power off */
	} else {
		if(regulator_is_enabled(g_pmic->reg_vsram_gpu)){
			GPUFREQ_LOGD("@%s g_pmic->reg_vsram_gpu is enabled before disable\n",
					__func__);
		}else{
			GPUFREQ_LOGD("@%s g_pmic->reg_vsram_gpu is already disabled!!!\n",
					__func__);
		}
		ret = regulator_disable(g_pmic->reg_vsram_gpu);
		if (unlikely(ret))
			__gpufreq_abort("fail to disable VSRAM_GPU (%d)",ret);

		if(!regulator_is_enabled(g_pmic->reg_vsram_gpu))
			GPUFREQ_LOGE("@%s g_pmic->reg_vsram_gpu is still enabled\n",__func__);

		if(regulator_is_enabled(g_pmic->reg_vgpu)){
			GPUFREQ_LOGD("@%s g_pmic->reg_vgpu is enabled before disable\n",
					__func__);
		}else{
			GPUFREQ_LOGD("@%s g_pmic->reg_vgpu is already disabled !!!\n",
					__func__);
		}
		ret = regulator_disable(g_pmic->reg_vgpu);
		if (unlikely(ret))
			__gpufreq_abort("fail to disable VGPU (%d)", ret);

		if(!regulator_is_enabled(g_pmic->reg_vgpu))
			GPUFREQ_LOGD("@%s g_pmic->reg_vgpu is still enabled\n",__func__);

	}

	return ret;
}

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{

}

void notify_gpufreq_probe_done(void)
{
	GPUFREQ_LOGD("notify_probe_done goto GPU_PWR_OFF");
	__gpufreq_early_init_buck_control(GPU_PWR_OFF);
}
EXPORT_SYMBOL(notify_gpufreq_probe_done);

/* API: gpufreq_early_init driver remove */
static int __gpufreq_early_init_pdrv_remove(struct platform_device *pdev)
{
	kfree(g_pmic);
	return GPUFREQ_SUCCESS;
}


/* API: register gpufreq_early_init platform driver */
static int __init __gpufreq_early_init_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGD("start to init gpufreq_early_init platform driver");
	ret = platform_driver_register(&g_gpufreq_early_init_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq_early_init platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGD("gpufreq_early_init platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq_early_init driver */
static void __exit __gpufreq_early_init_exit(void)
{
	platform_driver_unregister(&g_gpufreq_early_init_pdrv);
}

#if IS_BUILTIN(CONFIG_MTK_GPU_MT6877_SUPPORT)
rootfs_initcall(__gpufreq_early_init_init);
#else
module_init(__gpufreq_early_init_init);
#endif
module_exit(__gpufreq_early_init_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_early_init_of_match);
MODULE_DESCRIPTION("MediaTek gpufreq_early_init prepare vgpu and vsram");
MODULE_LICENSE("GPL");
