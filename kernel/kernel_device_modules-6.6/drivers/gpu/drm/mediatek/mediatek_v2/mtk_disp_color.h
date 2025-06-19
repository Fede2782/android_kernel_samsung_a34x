/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_COLOR_H__
#define __MTK_DISP_COLOR_H__

// SW Reg
/* ------------------------------------------------------------------------- */
#define MIRAVISION_HW_VERSION_MASK  (0xFF000000)
#define MIRAVISION_SW_VERSION_MASK  (0x00FF0000)
#define MIRAVISION_SW_FEATURE_MASK  (0x0000FFFF)
#define MIRAVISION_HW_VERSION_SHIFT (24)
#define MIRAVISION_SW_VERSION_SHIFT (16)
#define MIRAVISION_SW_FEATURE_SHIFT (0)
#define MIRAVISION_HW_VERSION       (0)

#define MIRAVISION_SW_VERSION       (3)	/* 3:Android N*/
#define MIRAVISION_SW_FEATURE_VIDEO_DC	(0x1)
#define MIRAVISION_SW_FEATURE_AAL	(0x2)
#define MIRAVISION_SW_FEATURE_PQDS	(0x4)

#define MIRAVISION_VERSION \
	((MIRAVISION_HW_VERSION << MIRAVISION_HW_VERSION_SHIFT) | \
	(MIRAVISION_SW_VERSION << MIRAVISION_SW_VERSION_SHIFT) | \
	MIRAVISION_SW_FEATURE_VIDEO_DC | \
	MIRAVISION_SW_FEATURE_AAL | \
	MIRAVISION_SW_FEATURE_PQDS)

#define SW_VERSION_VIDEO_DC         (1)
#define SW_VERSION_AAL              (1)
#define SW_VERSION_PQDS             (1)

#define DISP_COLOR_SWREG_START          (0xFFFF0000)
#define DISP_COLOR_SWREG_COLOR_BASE     (DISP_COLOR_SWREG_START)
#define DISP_COLOR_SWREG_TDSHP_BASE     (DISP_COLOR_SWREG_COLOR_BASE + 0x1000)
#define DISP_COLOR_SWREG_PQDC_BASE      (DISP_COLOR_SWREG_TDSHP_BASE + 0x1000)

#define DISP_COLOR_SWREG_PQDS_BASE	(DISP_COLOR_SWREG_PQDC_BASE + 0x1000)
#define DISP_COLOR_SWREG_MDP_COLOR_BASE	(DISP_COLOR_SWREG_PQDS_BASE + 0x1000)
#define DISP_COLOR_SWREG_END		(DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x1000)

#define SWREG_COLOR_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0000)
#define SWREG_GAMMA_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0001)
#define SWREG_TDSHP_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0002)
#define SWREG_AAL_BASE_ADDRESS          (DISP_COLOR_SWREG_COLOR_BASE + 0x0003)
#define SWREG_MIRAVISION_VERSION        (DISP_COLOR_SWREG_COLOR_BASE + 0x0004)
#define SWREG_SW_VERSION_VIDEO_DC       (DISP_COLOR_SWREG_COLOR_BASE + 0x0005)
#define SWREG_SW_VERSION_AAL            (DISP_COLOR_SWREG_COLOR_BASE + 0x0006)
#define SWREG_CCORR_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0007)
#define SWREG_MDP_COLOR_BASE_ADDRESS    (DISP_COLOR_SWREG_COLOR_BASE + 0x0008)
#define SWREG_COLOR_MODE                (DISP_COLOR_SWREG_COLOR_BASE + 0x0009)
#define SWREG_RSZ_BASE_ADDRESS          (DISP_COLOR_SWREG_COLOR_BASE + 0x000A)
#define SWREG_MDP_RDMA_BASE_ADDRESS     (DISP_COLOR_SWREG_COLOR_BASE + 0x000B)
#define SWREG_MDP_AAL_BASE_ADDRESS      (DISP_COLOR_SWREG_COLOR_BASE + 0x000C)
#define SWREG_MDP_HDR_BASE_ADDRESS      (DISP_COLOR_SWREG_COLOR_BASE + 0x000D)
#define SWREG_MML_COLOR_BASE_ADDRESS    (DISP_COLOR_SWREG_COLOR_BASE + 0x000E)
#define SWREG_MML_TDSHP_BASE_ADDRESS    (DISP_COLOR_SWREG_COLOR_BASE + 0x000F)
#define SWREG_DISP_TDSHP_BASE_ADDRESS   (DISP_COLOR_SWREG_COLOR_BASE + 0x0010)
#define SWREG_MML_AAL_BASE_ADDRESS      (DISP_COLOR_SWREG_COLOR_BASE + 0x0011)
#define SWREG_MML_HDR_BASE_ADDRESS      (DISP_COLOR_SWREG_COLOR_BASE + 0x0012)

#define SWREG_TDSHP_TUNING_MODE         (DISP_COLOR_SWREG_TDSHP_BASE + 0x0000)
/* ------------------------------------------------------------------------- */

struct mtk_disp_color_data {
	unsigned int color_offset;
	bool support_color21;
	bool support_color30;
	unsigned int color_window;
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_color_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_color_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct color_backup {
	unsigned int COLOR_CFG_MAIN;
};

struct mtk_disp_color_primary {
	struct DISP_PQ_PARAM color_param;
	int ncs_tuning_mode;
	unsigned int split_en;
	unsigned int split_window_x_start;
	unsigned int split_window_y_start;
	unsigned int split_window_x_end;
	unsigned int split_window_y_end;
	struct DISPLAY_COLOR_REG color_reg;
	int color_reg_valid;
	unsigned int width;
	struct MDP_COLOR_CAP mdp_color_cap;
	struct DISPLAY_PQ_T color_index;
	struct color_backup color_backup;
	struct DISP_AAL_DRECOLOR_PARAM drecolor_param;
	struct mutex data_lock;
	unsigned int relay_state;
};

/**
 * struct mtk_disp_color - DISP_COLOR driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_color {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_color_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_color_primary *primary_data;
	struct mtk_disp_color_tile_overhead tile_overhead;
	struct mtk_disp_color_tile_overhead_v tile_overhead_v;
	unsigned long color_dst_w;
	unsigned long color_dst_h;
	atomic_t color_is_clock_on;
	spinlock_t clock_lock;
	unsigned int set_partial_update;
	unsigned int roi_height;
};

bool disp_color_reg_get(struct mtk_ddp_comp *comp,
	const char *reg_name, int *value);
void disp_color_bypass(struct mtk_ddp_comp *comp, int bypass, int caller,
	struct cmdq_pkt *handle);
void disp_color_regdump(struct mtk_ddp_comp *comp);
void disp_color_write_pos_main_for_dual_pipe(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_WRITE_REG *wParams,
	unsigned int pa, unsigned int pa1);
unsigned long disp_color_get_reg_offset(const char *reg_name);
// for displayPQ update to swpm tppa
unsigned int disp_color_bypass_info(struct mtk_drm_crtc *mtk_crtc);
#endif
