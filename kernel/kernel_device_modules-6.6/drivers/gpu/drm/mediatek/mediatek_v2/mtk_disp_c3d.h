/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_C3D_H__
#define __MTK_DISP_C3D_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#define C3D_3DLUT_SIZE_17BIN (17 * 17 * 17 * 3)
#define C3D_3DLUT_SIZE_9BIN (9 * 9 * 9 * 3)

#define DISP_C3D_SRAM_SIZE_17BIN (17 * 17 * 17)
#define DISP_C3D_SRAM_SIZE_9BIN (9 * 9 * 9)

struct DISP_C3D_REG_17BIN {
	unsigned int lut3d_reg[C3D_3DLUT_SIZE_17BIN];
};

struct DISP_C3D_REG_9BIN {
	unsigned int lut3d_reg[C3D_3DLUT_SIZE_9BIN];
};

struct mtk_disp_c3d_data {
	bool support_shadow;
	bool need_bypass_shadow;
	int def_bin_num;
	int def_sram_start_addr;
	int def_sram_end_addr;
};

struct mtk_disp_c3d_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_c3d_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_c3d_primary {
	struct DISP_C3D_REG_17BIN c3d_reg;
	unsigned int c3d_sram_cfg[DISP_C3D_SRAM_SIZE_17BIN];
	unsigned int c3d_lut1d[DISP_C3D_1DLUT_SIZE];
	bool set_lut_flag;
	bool update_sram_ignore;
	bool skip_update_sram;
	struct mutex clk_lock;
	struct mutex data_lock;
	struct cmdq_pkt *sram_pkt;
	atomic_t c3d_sram_hw_init;
	unsigned int relay_state;
};

struct mtk_disp_c3d {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_c3d_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_c3d_primary *primary_data;
	struct mtk_disp_c3d_tile_overhead tile_overhead;
	struct mtk_disp_c3d_tile_overhead_v tile_overhead_v;
	bool pkt_reused;
	struct cmdq_reuse reuse_c3d[4913 * 2];
	atomic_t c3d_is_clock_on;
	atomic_t c3d_force_sram_apb;
	bool has_set_1dlut;
	unsigned int set_partial_update;
	unsigned int roi_height;
	int bin_num;
	int sram_start_addr;
	int sram_end_addr;
	int c3dlut_size;
};

inline struct mtk_disp_c3d *comp_to_c3d(struct mtk_ddp_comp *comp);
void disp_c3d_debug(struct drm_crtc *crtc, const char *opt);
void disp_c3d_regdump(struct mtk_ddp_comp *comp);
// for displayPQ update to swpm tppa
unsigned int disp_c3d_bypass_info(struct mtk_drm_crtc *mtk_crtc, int num);

#endif

