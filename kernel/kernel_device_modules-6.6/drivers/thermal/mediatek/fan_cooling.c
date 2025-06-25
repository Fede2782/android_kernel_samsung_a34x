// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <mtk_pwm.h>
#include <mtk_pwm_hal.h>
#include "fan_cooling.h"

struct mtk_pwm_fan {
	unsigned int pwm_ch;
};

static struct pinctrl *pinctrl;
static struct pinctrl_state *gpio_active;
static struct pinctrl_state *gpio_inactive;

static struct pwm_spec_config fan_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_OLD,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_OLD_MODE_32K,
	.pmic_pad = 0,
	.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_OLD_REGS.GDURATION = 0,
	.PWM_MODE_OLD_REGS.WAVE_NUM = 0,
	.PWM_MODE_OLD_REGS.DATA_WIDTH = 255,
	.PWM_MODE_OLD_REGS.THRESH = 1,
};

/*==================================================
 * cooler callback functions
 *==================================================
 */
static int fan_throttle(struct fan_cooling_device *fan_cdev, unsigned long state)
{
	struct device *dev = fan_cdev->dev;
	int ret;

	if (state == FAN_COOLING_UNLIMITED_STATE) {
		mt_pwm_disable(fan_pwm_config.pwm_no, fan_pwm_config.pmic_pad);
		ret = pinctrl_select_state(pinctrl, gpio_inactive);
		if (ret) {
			dev_info(dev, "%s(), error, can not set gpio type inactive\n",
				__func__);
			return ret;
		}
	} else {
		ret = pinctrl_select_state(pinctrl, gpio_active);
		if (ret) {
			dev_info(dev, "%s(), error, can not set gpio type active\n",
				__func__);
			return ret;
		}
		fan_pwm_config.PWM_MODE_OLD_REGS.THRESH = (fan_pwm_config.PWM_MODE_OLD_REGS.DATA_WIDTH)
			/(fan_cdev->max_state)*state;
		ret = pwm_set_spec_config(&fan_pwm_config);
		if (ret < 0) {
			dev_info(dev,"pwm_set_spec_config fail, ret: %d\n", ret);
			return ret;
		}
		dev_info(dev, "%s: set pwm-THRESH = %d done\n",
			fan_cdev->name, fan_pwm_config.PWM_MODE_OLD_REGS.THRESH);
	}
	fan_cdev->target_state = state;
	return 0;
}
static int fan_cooling_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct fan_cooling_device *fan_cdev = cdev->devdata;

	*state = fan_cdev->max_state;

	return 0;
}

static int fan_cooling_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct fan_cooling_device *fan_cdev = cdev->devdata;

	*state = fan_cdev->target_state;

	return 0;
}

static int fan_cooling_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct fan_cooling_device *fan_cdev = cdev->devdata;
	int ret;

	/* Request state should be less than max_state */
	if (WARN_ON(state > fan_cdev->max_state || !fan_cdev->throttle))
		return -EINVAL;

	if (fan_cdev->target_state == state)
		return 0;

	ret = fan_cdev->throttle(fan_cdev, state);

	return ret;
}

static const struct of_device_id fan_cooling_of_match[] = {
	{
		.compatible = "mediatek,fan-cooler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fan_cooling_of_match);

static struct thermal_cooling_device_ops fan_cooling_ops = {
	.get_max_state		= fan_cooling_get_max_state,
	.get_cur_state		= fan_cooling_get_cur_state,
	.set_cur_state		= fan_cooling_set_cur_state,
};

static int fan_cooling_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct thermal_cooling_device *cdev;
	struct fan_cooling_device *fan_cdev;
	struct mtk_pwm_fan *pwm_fan;
	int len, offset;
	int ret = 0;
	fan_cdev = devm_kzalloc(dev, sizeof(*fan_cdev), GFP_KERNEL);
	if (!fan_cdev)
		return -ENOMEM;
	pwm_fan = devm_kmalloc(&pdev->dev, sizeof(*pwm_fan), GFP_KERNEL);
	if (!pwm_fan)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node, "pwm-ch",
		&pwm_fan->pwm_ch);
	len = (strlen(np->name) > (MAX_FAN_COOLER_NAME_LEN - 1)) ?
		(MAX_FAN_COOLER_NAME_LEN - 1) : strlen(np->name);
	offset = strscpy(fan_cdev->name, np->name, len + 1);
	if (offset < 0 ) {
		dev_info(dev, "%s: offset=%d, len=%d fail\n", fan_cdev->name, offset, len);
		return -EINVAL;
	}
	fan_cdev->target_state = FAN_COOLING_UNLIMITED_STATE;
	fan_cdev->dev = dev;
	fan_cdev->max_state = FAN_STATE_NUM - 1;
	fan_cdev->throttle = fan_throttle;
	fan_pwm_config.pwm_no = (unsigned int)pwm_fan->pwm_ch;
	mt_pwm_disable(fan_pwm_config.pwm_no, fan_pwm_config.pmic_pad);

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		dev_info(dev, "%s(), ret %d, cannot get pinctrl!\n",
			__func__, ret);
		return ret;
	}

	gpio_inactive = pinctrl_lookup_state(pinctrl, "default");
	if (IS_ERR(gpio_inactive)) {
		ret = PTR_ERR(gpio_inactive);
		dev_info(dev, "%s(), pinctrl_lookup_state %s fail, ret %d\n",
			__func__, "default", ret);
		return ret;
	}

	gpio_active = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(gpio_active)) {
		ret = PTR_ERR(gpio_active);
		dev_info(dev, "%s(), pinctrl_lookup_state %s fail, ret %d\n",
			__func__, "active", ret);
		return ret;
	}

	cdev = thermal_of_cooling_device_register(np, fan_cdev->name,
		fan_cdev, &fan_cooling_ops);
	if (IS_ERR(cdev))
		return -EINVAL;
	fan_cdev->cdev = cdev;

	platform_set_drvdata(pdev, fan_cdev);
	dev_info(dev, "register %s done\n", fan_cdev->name);

	return 0;
}

static int fan_cooling_remove(struct platform_device *pdev)
{
	struct fan_cooling_device *fan_cdev;

	fan_cdev = (struct fan_cooling_device *)platform_get_drvdata(pdev);

	thermal_cooling_device_unregister(fan_cdev->cdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}
static unsigned long saved_cooling_state;

static int fan_cooling_noirq_suspend(struct device *dev)
{
	struct fan_cooling_device *fan_cdev = dev_get_drvdata(dev);
	struct thermal_cooling_device *cdev = fan_cdev->cdev;

	if (cdev->ops->get_cur_state(cdev, &saved_cooling_state)) {
		dev_info(dev, "Failed to get current cooling state\n");
		return -EINVAL;
	}
	if (saved_cooling_state != FAN_COOLING_UNLIMITED_STATE) {
		if (cdev->ops->set_cur_state(cdev, FAN_COOLING_UNLIMITED_STATE)) {
			dev_info(dev, "Failed to release cooling state\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int fan_cooling_noirq_resume(struct device *dev)
{
	struct fan_cooling_device *fan_cdev = dev_get_drvdata(dev);
	struct thermal_cooling_device *cdev = fan_cdev->cdev;

	if (cdev->ops->set_cur_state(cdev, saved_cooling_state)) {
		dev_info(dev, "Failed to restore cooling state\n");
		return -EINVAL;
	}
	return 0;
}

const struct dev_pm_ops fan_cooling_pm_ops = {
	.suspend_noirq = fan_cooling_noirq_suspend,
	.resume_noirq = fan_cooling_noirq_resume,
};

static struct platform_driver fan_cooling_driver = {
	.probe = fan_cooling_probe,
	.remove = fan_cooling_remove,
	.driver = {
		.name = "mtk-fan-cooling",
		.of_match_table = fan_cooling_of_match,
		.pm = &fan_cooling_pm_ops,
	},
};
module_platform_driver(fan_cooling_driver);

MODULE_AUTHOR("Xing Fang <xing.fang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek fan cooling driver");
MODULE_LICENSE("GPL");
