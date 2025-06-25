/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __DRM_EDRTX_API_H__
#define __DRM_EDRTX_API_H__

#include <drm/mediatek_drm.h>

/* edp phy device driver */
extern struct platform_driver mtk_edp_phy_driver;

/* DVO related APIs */
int mtk_drm_dvo_get_info(struct drm_device *dev,
		struct drm_mtk_session_info *info);

/* eDP related APIs */
int mtk_drm_ioctl_enable_edp(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
#endif

