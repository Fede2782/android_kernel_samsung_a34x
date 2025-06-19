/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_DITHER_H__
#define __MTK_DISP_DITHER_H__

#define PURE_CLR_RGB (3)
#define PURE_CLR_NUM_MAX (7)

struct mtk_disp_dither_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct work_struct_data {
	void *data;
	struct work_struct pure_detect_task;
};

struct mtk_disp_pure_clr_data {
	unsigned int pure_clr_det;
	unsigned int pure_clr_num;
	unsigned int pure_clr[PURE_CLR_NUM_MAX][PURE_CLR_RGB];
};

struct mtk_disp_dither_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_dither_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_dither_primary {
	struct workqueue_struct *pure_detect_wq;
	struct work_struct_data work_data;
	unsigned int dither_mode;
	struct mtk_disp_pure_clr_data *pure_clr_param;
	unsigned int *gamma_data_mode;
	unsigned int relay_state;
};

struct mtk_disp_dither {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_dither_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_dither_primary *primary_data;
	atomic_t is_clock_on;
	struct mtk_disp_dither_tile_overhead tile_overhead;
	struct mtk_disp_dither_tile_overhead_v tile_overhead_v;
	unsigned int set_partial_update;
	unsigned int roi_height;
	uint32_t purecolor0;
};

void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass);
void disp_dither_set_color_detect(struct drm_crtc *crtc, int enable);
void disp_dither_regdump(struct mtk_ddp_comp *comp);
// for displayPQ update to swpm tppa
unsigned int disp_dither_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif
