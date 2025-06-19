/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _LM3643_H
#define _LM3643_H

#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

enum lm3643_led_id {
	LM3643_LED0 = 0,
	LM3643_LED1,
	LM3643_LED_MAX
};

/**
 * struct lm3643_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct lm3643_flash {
	struct device *dev;
	struct lm3643_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led[LM3643_LED_MAX];
	struct v4l2_subdev subdev_led[LM3643_LED_MAX];
	struct device_node *dnode[LM3643_LED_MAX];
	struct pinctrl *lm3643_hwen_pinctrl;
	struct pinctrl_state *lm3643_hwen_high;
	struct pinctrl_state *lm3643_hwen_low;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id[LM3643_LED_MAX];
#endif
	int need_cooler;
	unsigned long target_current[LM3643_LED_MAX];
	unsigned long ori_current[LM3643_LED_MAX];
	unsigned int torch_power;
	unsigned int camera_power;
};

#endif /* _LM3643_H */
