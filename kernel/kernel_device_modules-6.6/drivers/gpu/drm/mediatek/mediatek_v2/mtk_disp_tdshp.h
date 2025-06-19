/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_TDSHP_H__
#define __MTK_DISP_TDSHP_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"

struct mtk_disp_tdshp_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_tdshp_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_tdshp_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_tdshp_primary {
	wait_queue_head_t size_wq;
	bool get_size_available;
	struct DISP_TDSHP_DISPLAY_SIZE tdshp_size;
	struct mutex data_lock;
	struct DISP_TDSHP_REG *tdshp_regs;
	int tdshp_reg_valid;
	int *aal_clarity_support;
	unsigned int relay_state;
};

struct mtk_disp_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_tdshp_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_tdshp_primary *primary_data;
	struct mtk_disp_tdshp_tile_overhead tile_overhead;
	struct mtk_disp_tdshp_tile_overhead_v tile_overhead_v;
	unsigned int set_partial_update;
	unsigned int roi_height;
};

void disp_tdshp_regdump(struct mtk_ddp_comp *comp);
// for displayPQ update to swpm tppa
unsigned int disp_tdshp_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

