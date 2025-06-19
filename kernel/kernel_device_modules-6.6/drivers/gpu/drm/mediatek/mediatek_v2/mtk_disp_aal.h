/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_AAL_H__
#define __MTK_DISP_AAL_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#define AAL_DRE30_GAIN_REGISTER_NUM		(544)
#define AAL_DRE30_HIST_REGISTER_NUM		(768)

enum MTK_LED_TYPE {
	TYPE_FILE = 0,
	TYPE_ATOMIC = 1,
	TYPE_FACTORY = 2,
	TYPE_MAX = 3,
};

enum AAL_CMDQ_DMA_MAP {
	AAL_LOCAL_HIST = 0,
	AAL_DUAL_PIPE_INFO,
	AAL_MAX_GHIST,
	AAL_Y_GHIST,
	DMDP_AAL_MAX_GHIST,
	DMDP_AAL_Y_GHIST,
	COLOR_HIST,
	DMDP_AAL_CLARITY,
	TDSHP_CLARITY,
	AAL_DMA_MAX,
};

struct DISP_DRE30_HIST {
	unsigned int aal0_dre_hist[AAL_DRE30_HIST_REGISTER_NUM];
	unsigned int aal1_dre_hist[AAL_DRE30_HIST_REGISTER_NUM];
	int dre_blk_x_num;
	int dre_blk_y_num;
};

struct DISP_DRE30_PARAM {
	unsigned int dre30_gain[AAL_DRE30_GAIN_REGISTER_NUM];
};

struct mtk_disp_aal_data {
	bool support_shadow;
	bool need_bypass_shadow;
	int aal_dre_hist_start;
	int aal_dre_hist_end;
	int aal_dre_gain_start;
	int aal_dre_gain_end;
	bool aal_dre3_curve_sram;
	bool aal_dre3_auto_inc;
	bool mdp_aal_ghist_support;
	int bitShift;
};

struct dre3_node {
	struct device *dev;
	void __iomem *va;
	phys_addr_t pa;
	struct clk *clk;
};

struct _mtk_disp_aal_tile_overhead {
	unsigned int in_width;
	unsigned int comp_overhead;
	unsigned int total_overhead;
};

struct _mtk_disp_aal_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_aal_feature_option {
	unsigned int mtk_aal_support;
	unsigned int mtk_dre30_support;
	unsigned int mtk_cabc_no_support;
};

struct work_struct_aal_data {
	void *data;
	struct work_struct task;
};

struct mtk_disp_aal_primary {
	struct task_struct *sof_irq_event_task;
	struct timespec64 start;
	struct timespec64 end;
	int dbg_en;
	struct wait_queue_head hist_wq;
	struct wait_queue_head sof_irq_wq;
	struct mutex config_lock;
	struct mutex clk_lock;
	spinlock_t hist_lock;
	struct DISP_AAL_HIST hist;
	atomic_t event_en;
	atomic_t force_event_en;
	atomic_t eof_irq_skip;
	atomic_t sof_irq_available;
	atomic_t is_init_regs_valid;
	atomic_t backlight_notified;
	atomic_t initialed;
	atomic_t allowPartial;
	atomic_t should_stop;
	atomic_t dre30_write;
	atomic_t force_delay_check_trig;
	struct workqueue_struct *refresh_wq;
	int backlight_set;
	int elvsspn_set;
	atomic_t dre_halt;
	struct DISP_DRE30_PARAM dre30_gain;
	struct DISP_DRE30_HIST dre30_hist;
	atomic_t change_to_dre30;
	struct wait_queue_head size_wq;
	bool get_size_available;
	struct DISP_AAL_DISPLAY_SIZE size;
	struct DISP_AAL_DISPLAY_SIZE dual_size;
	atomic_t panel_type;
	int ess_level;
	int dre_en;
	int ess_en;
	int ess_level_cmd_id;
	int dre_en_cmd_id;
	int ess_en_cmd_id;
	bool dre30_enabled;
	bool prv_dre30_enabled;
	unsigned int dre30_en;
	struct DISP_CLARITY_REG *disp_clarity_regs;
	struct mtk_aal_feature_option *aal_fo;
	struct DISP_AAL_PARAM aal_param;
	bool aal_param_valid;
	struct DISP_AAL_ESS20_SPECT_PARAM ess20_spect_param;
	int aal_clarity_support;
	int tdshp_clarity_support;
	int disp_clarity_support;
	struct DISP_AAL_INITREG init_regs;
	struct work_struct_aal_data refresh_task;
	enum MTK_LED_TYPE led_type;
	unsigned int fps;
	unsigned int relay_state;
	atomic_t eof_irq_en;
	atomic_t func_flag;  // 0: ess & dre off; 1: ess | dre on
	atomic_t hal_force_update;
	int pre_enable;
};

struct mtk_disp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct dre3_node dre3_hw;
	atomic_t is_clock_on;
	const struct mtk_disp_aal_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_aal_primary *primary_data;
	struct _mtk_disp_aal_tile_overhead overhead;
	struct _mtk_disp_aal_tile_overhead_v tile_overhead_v;
	atomic_t hist_available;
	atomic_t dre20_hist_is_ready;
	atomic_t hw_hist_ready;
	atomic_t first_frame;
	atomic_t force_curve_sram_apb;
	atomic_t force_hist_apb;
	atomic_t dre_config;
	struct mtk_ddp_comp *comp_tdshp;
	struct mtk_ddp_comp *comp_gamma;
	struct mtk_ddp_comp *comp_color;
	struct mtk_ddp_comp *comp_dmdp_aal;
	unsigned int set_partial_update;
	unsigned int roi_height;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	bool dre3_curve_need_reset;
#endif
};

static inline struct mtk_disp_aal *comp_to_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_aal, ddp_comp);
}

void disp_aal_debug(struct drm_crtc *crtc, const char *opt);
/* Provide for LED */
void disp_aal_notify_backlight_changed(struct mtk_ddp_comp *comp, int trans_backlight,
	int panel_nits, int max_backlight, int need_lock);

/* AAL Control API in Kernel */
void disp_aal_set_ess_level(struct mtk_ddp_comp *comp, int level);
void disp_aal_set_ess_en(struct mtk_ddp_comp *comp, int enable);
void disp_aal_set_dre_en(struct mtk_ddp_comp *comp, int enable);
void disp_aal_regdump(struct mtk_ddp_comp *comp);

// for displayPQ update to swpm tppa
unsigned int disp_aal_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

