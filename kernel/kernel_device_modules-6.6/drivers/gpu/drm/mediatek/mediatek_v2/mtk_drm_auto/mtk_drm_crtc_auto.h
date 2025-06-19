/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_CRTC_AUTO_H
#define MTK_DRM_CRTC_AUTO_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>

#include <drm/drm_crtc.h>
//#include "mtk_drm_crtc.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#define DUMMY_REG_BASE 0x329D0000

#define MT6991_OVL_MDP_RSZ0_DUMMY0 0x10
#define DSI_HDISPLAY_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DSI_VDISPLAY_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY1 0x14
#define DSI_HSYNC_START_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DSI_HSYNC_END_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY2 0x40
#define DSI_VSYNC_START_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DSI_VSYNC_END_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY3 0x54
#define DSI_HSYNC_TOTAL_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DSI_VSYNC_TOTAL_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY4 0x58
#define DSI_CLOCK_VALUE	REG_FLD_MSB_LSB(31, 0)

#define MT6991_OVL_MDP_RSZ0_DUMMY5 0x5C
#define DSI_HEIGHT_MM_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DSI_WIDTH_MM_VALUE REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY6 0x60
#define DP_HDISPLAY_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DP_VDISPLAY_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY7 0x68
#define DP_HSYNC_START_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DP_HSYNC_END_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY8 0x70
#define DP_VSYNC_START_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DP_VSYNC_END_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY9 0x74
#define DP_HSYNC_TOTAL_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DP_VSYNC_TOTAL_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMMY10 0x78
#define DP_CLOCK_VALUE	REG_FLD_MSB_LSB(31, 0)

#define MT6991_OVL_MDP_RSZ0_DUMMY11 0x7C
#define DP_HEIGHT_MM_VALUE	REG_FLD_MSB_LSB(15, 0)
#define DP_WIDTH_MM_VALUE	REG_FLD_MSB_LSB(31, 16)

#define MT6991_OVL_MDP_RSZ0_DUMM20 0x104
#define DP_INTF0_CONNECTOR_READY	REG_FLD_MSB_LSB(1, 0)

void mtk_drm_crtc_init_layer_nr(struct mtk_drm_crtc *mtk_crtc, int pipe);
struct mtk_ddp_comp *mtk_crtc_get_comp_with_index(struct mtk_drm_crtc *mtk_crtc,
						  struct mtk_plane_state *plane_state);
bool mtk_crtc_layer_need_swap(struct mtk_drm_crtc *mtk_crtc);
int mtk_drm_crtc_init_plane(struct drm_device *drm_dev, struct mtk_drm_crtc *mtk_crtc, int pipe);

void mtk_get_output_timing(struct drm_crtc *crtc);
void mtk_drm_backup_default_timing(struct mtk_drm_crtc *mtk_crtc,
						struct drm_display_mode *timing);
void mtk_drm_connector_notify_guest(struct mtk_drm_crtc *mtk_crtc,
	unsigned int connector_enable);

void mtk_drm_crtc_dev_init(struct drm_device *dev);

#endif

#endif /* MTK_DRM_CRTC_AUTO_H */
