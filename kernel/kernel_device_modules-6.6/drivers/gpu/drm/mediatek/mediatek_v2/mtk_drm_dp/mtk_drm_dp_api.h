/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DRM_DP_API_H__
#define __MTK_DRM_DP_API_H__

int mtk_drm_dp_get_dev_info(struct drm_device *dev,
			    void *data, struct drm_file *file_priv);
int mtk_drm_dp_audio_enable(struct drm_device *dev,
			    void *data, struct drm_file *file_priv);
int mtk_drm_dp_audio_config(struct drm_device *dev,
			    void *data, struct drm_file *file_priv);
int mtk_drm_dp_get_cap(struct drm_device *dev,
		       void *data, struct drm_file *file_priv);
int mtk_drm_dp_get_info_by_id(struct drm_device *dev,
			struct drm_mtk_session_info *info, int dp_encoder_id);
void mtk_dp_get_dsc_capability(u8 *dsc_cap);
#endif

