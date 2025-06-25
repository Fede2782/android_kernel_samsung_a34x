/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/drm_bridge.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

struct vdo_timing {
	u32 width;
	u32 height;
	u32 hfp;
	u32 hsa;
	u32 hbp;
	u32 vfp;
	u32 vsa;
	u32 vbp;
	u32 fps;
	u32 pll;
	u32 lppf;
	u32 physcial_w;
	u32 physcial_h;
	u32 crop_width[2];
	u32 crop_height[2];
};

void serdes_enable(struct drm_bridge *bridge, u8 port);
void serdes_pre_enable(struct drm_bridge *bridge, u8 port);
void serdes_disable(struct drm_bridge *bridge, u8 port);
void serdes_get_modes(struct drm_bridge *bridge, struct vdo_timing *disp_mode, u8 port);
int serdes_get_link_status(struct drm_bridge *bridge);

