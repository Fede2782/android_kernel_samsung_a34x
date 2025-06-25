/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_CCORR_H__
#define __MTK_DISP_CCORR_H__

#include <uapi/drm/mediatek_drm.h>

struct mtk_disp_ccorr_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_ccorr_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_ccorr_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_ccorr_primary {
	unsigned int ccorr_8bit_switch;
	struct drm_mtk_ccorr_caps disp_ccorr_caps;
	int ccorr_offset_base;
	int ccorr_max_negative;
	int ccorr_max_positive;
	int ccorr_fullbit_mask;
	int ccorr_offset_mask;
	unsigned int disp_ccorr_number;
	bool sbd_on;
	atomic_t ccorr_irq_en;
	struct DRM_DISP_CCORR_COEF_T *disp_ccorr_coef;
	int ccorr_color_matrix[3][3];
	int ccorr_prev_matrix[3][3];
	int rgb_matrix[3][3];
	struct DRM_DISP_CCORR_COEF_T multiply_matrix_coef;
	int disp_ccorr_without_gamma;
	wait_queue_head_t ccorr_get_irq_wq;
	atomic_t ccorr_get_irq;
	int old_pq_backlight;
	int pq_backlight;
	int pq_backlight_db;
	atomic_t ccorr_is_init_valid;
	struct mutex data_lock;
	struct mutex bl_lock;
	unsigned int ccorr_hw_valid;
	unsigned int relay_state;
};

struct mtk_disp_ccorr {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_ccorr_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	enum drm_disp_ccorr_linear_t is_linear;// each comp property
	struct mtk_disp_ccorr_primary *primary_data;
	struct mtk_disp_ccorr_tile_overhead tile_overhead;
	struct mtk_disp_ccorr_tile_overhead_v tile_overhead_v;
	bool bypass_color;
	struct mtk_ddp_comp *color_comp;
	unsigned int set_partial_update;
	unsigned int roi_height;
};

inline struct mtk_disp_ccorr *comp_to_ccorr(struct mtk_ddp_comp *comp);
void disp_ccorr_notify_backlight_changed(struct mtk_ddp_comp *comp, int bl_1024);
int disp_ccorr_set_color_matrix(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	int32_t matrix[16], int32_t hint, bool fte_flag, bool linear);
int disp_ccorr_set_RGB_Gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int r, int g, int b);
int mtk_drm_ioctl_ccorr_support_color_matrix(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_ccorr_get_pq_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int disp_ccorr_act_get_ccorr_caps(struct mtk_ddp_comp *comp, struct drm_mtk_ccorr_caps *ccorr_caps);
void disp_ccorr_regdump(struct mtk_ddp_comp *comp);
int disp_ccorr_act_get_irq(struct mtk_ddp_comp *comp, void *data);
// for displayPQ update to swpm tppa
unsigned int disp_ccorr_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

