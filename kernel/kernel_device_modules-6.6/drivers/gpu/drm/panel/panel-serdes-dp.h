/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _PANEL_SERDES_DP_H_
#define _PANEL_SERDES_DP_H_

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

#define PANEL_NAME_SIZE		64
#define USE_DEFAULT_SETTING	"use-default-setting"
#define PANEL_NAME			"panel-name"
#define PANEL_MODE			"panel-mode"
#define DEBUG_DP_INFO		"[serdes-dp-panel]"
#define DEBUG_DP_MST1_INFO	"[serdes-dp-mst1-panel]"
#define DEBUG_EDP_INFO		"[serdes-edp-panel]"

struct panel_edp {
	struct drm_panel panel;
	struct device *dev;
	char panel_name[PANEL_NAME_SIZE];
	char panel_mode[PANEL_NAME_SIZE];

	const char *label;
	unsigned int width;
	unsigned int height;
	struct videomode video_mode;
	struct backlight_device *backlight;
	struct regulator *supply;

	struct gpio_desc *power3v3_gpio;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	bool is_dp;
	bool use_default;
	char debug_str[32];
};

#endif
