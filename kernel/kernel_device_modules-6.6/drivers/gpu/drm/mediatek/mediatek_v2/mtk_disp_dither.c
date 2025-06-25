// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_disp_dither.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_gamma.h"
#include "mtk_disp_chist.h"

#define DISP_DITHER_EN		0x0
#define DISP_DITHER_INTEN	0x08
#define DISP_DITHER_INTSTA	0x0c
#define DISP_REG_DITHER_CFG	0x20
#define DISP_REG_DITHER_SIZE	0x30
#define DISP_DITHER_SHADOW	0x15C
#define DISP_DITHER_PURECOLOR0	0x0160
#define DITHER_REG(idx) (0x100 + (idx)*4)

#define DISP_DITHER_CLR_DET	BIT(0)
#define DISP_DITHER_CLR_FLAG	BIT(4)
#define DITHER_RELAY_MODE BIT(0)

enum COLOR_IOCTL_CMD {
	BYPASS_DITHER = 0,
	SET_INTERRUPT,
	SET_COLOR_DETECT,
};

static inline struct mtk_disp_dither *comp_to_dither(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dither, ddp_comp);
}

static int disp_dither_set_interrupt(struct mtk_ddp_comp *comp, int enabled, struct cmdq_pkt *handle)
{
	int ret = 0;

	DDPDBG("%s %d ++\n", __func__, __LINE__);
	if (enabled) {
		/* Enable output frame end interrupt */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_DITHER_INTEN, 1, ~0);
		DDPINFO("%s: Interrupt enabled\n", __func__);
	} else {
		/* Disable output frame end interrupt */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_DITHER_INTEN, 0, ~0);
		DDPINFO("%s: Interrupt disabled\n", __func__);
	}
	DDPDBG("%s %d --\n", __func__, __LINE__);
	return ret;
}

static void disp_dither_purecolor_detection(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	uint32_t clr_det, clr_flag, purecolor0;
	int enable = 0;

	if (atomic_read(&dither_data->is_clock_on) != 1) {
		DDPINFO("%s: clock is off.\n", __func__);
		return;
	}
	purecolor0 = dither_data->purecolor0;
	clr_det = purecolor0 & DISP_DITHER_CLR_DET;

	//TODO: check_trigger + irq will cause cmd mode always trigger when clr_det enable
	if (clr_det) {	/* pure color detection support */
		clr_flag = purecolor0 & DISP_DITHER_CLR_FLAG;
		DDPINFO("%s: clr_flag: 0x%x\n", __func__, clr_flag);
		if (clr_flag)
			disp_dither_set_bypass(crtc, 1);
		else
			disp_dither_set_bypass(crtc, 0);
		mtk_crtc_check_trigger(mtk_crtc, true, true);
	} else {
		DDPINFO("%s: clr_det: 0x%x\n", __func__, clr_det);
		disp_dither_set_bypass(crtc, 0);
		mtk_crtc_user_cmd(crtc, comp, SET_INTERRUPT, &enable);
	}
}

static void disp_dither_pure_detect_work(struct work_struct *work_item)
{
	struct work_struct_data *work_data = container_of(work_item,
			struct work_struct_data, pure_detect_task);

	if (!work_data->data)
		return;
	disp_dither_purecolor_detection((struct mtk_ddp_comp *)work_data->data);
}

static void disp_dither_on_end_of_frame(struct mtk_ddp_comp *comp, uint32_t status)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;

	DDPIRQ("%s:%s status: 0x%x\n", __func__, mtk_dump_comp_str(comp), status);
	if (status & 0x2) {
		dither_data->purecolor0 = readl(comp->regs + DISP_DITHER_PURECOLOR0);
		primary_data->work_data.data = (void *)comp;
		queue_work(primary_data->pure_detect_wq,
			&primary_data->work_data.pure_detect_task);
	}
}

static irqreturn_t disp_dither_irq_handler(int irq, void *dev_id)
{
	unsigned int status;
	struct mtk_ddp_comp *comp;
	struct mtk_disp_dither *dither_data = dev_id;

	if (IS_ERR_OR_NULL(dither_data))
		return IRQ_NONE;

	comp = &dither_data->ddp_comp;

	if (mtk_drm_top_clk_isr_get(comp) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}
	status = readl(comp->regs + DISP_DITHER_INTSTA);
	writel(status & ~0x3, comp->regs + DISP_DITHER_INTSTA);
	disp_dither_on_end_of_frame(comp, status);
	mtk_drm_top_clk_isr_put(comp);

	return IRQ_HANDLED;
}

static void disp_dither_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!dither_data->is_right_pipe) {
			dither_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				dither_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				dither_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			dither_data->tile_overhead.in_width =
					cfg->tile_overhead.left_in_width;
			dither_data->tile_overhead.overhead =
					cfg->tile_overhead.left_overhead;

			disp_chist_set_tile_overhead(comp->mtk_crtc,
				dither_data->tile_overhead.overhead, false);
		} else {
			dither_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				dither_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				dither_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			dither_data->tile_overhead.in_width =
					cfg->tile_overhead.right_in_width;
			dither_data->tile_overhead.overhead =
					cfg->tile_overhead.right_overhead;

			disp_chist_set_tile_overhead(comp->mtk_crtc,
				dither_data->tile_overhead.overhead, true);
		}
	}
}

static void disp_dither_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	dither_data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		dither_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	dither_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void disp_dither_config(struct mtk_ddp_comp *comp,
			      struct mtk_ddp_config *cfg,
			      struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;
	unsigned int enable = 1;
	unsigned int width;
	unsigned int overhead_v;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		width = dither_data->tile_overhead.in_width;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	DDPINFO("%s: bbp = %u width = %u height = %u\n", __func__, cfg->bpc, width, cfg->h);

	if (primary_data->gamma_data_mode && *primary_data->gamma_data_mode == 0) {
		if (cfg->bpc == 8) { /* 888 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x20200001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x20202020, ~0);
		} else if (cfg->bpc == 5) { /* 565 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x50500001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x50504040, ~0);
		} else if (cfg->bpc == 6) { /* 666 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x40400001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x40404040, ~0);
		} else if (cfg->bpc > 8) {
			/* High depth LCM, no need dither */
			DDPINFO("%s: High depth LCM (bpp = %u), no dither\n",
				__func__, cfg->bpc);
		} else {
			/* Invalid dither bpp, bypass dither */
			/* FIXME: this case would cause dither hang */
			DDPINFO("%s: Invalid dither bpp = %u\n",
				__func__, cfg->bpc);
			enable = 0;
		}
	} else {
		if (cfg->bpc == 10) { /* 101010 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x20200001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x20202020, ~0);
		} else if (cfg->bpc == 8) { /* 888 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x40400001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x40404040, ~0);
		} else if (cfg->bpc > 10) {
			/* High depth LCM, no need dither */
			DDPINFO("%s: High depth LCM (bpp = %u), no dither\n",
				__func__, cfg->bpc);
		} else {
			/* Invalid dither bpp, bypass dither */
			DDPINFO("%s: Invalid dither bpp = %u\n", __func__, cfg->bpc);
			enable = 0;
		}
	}

	if (enable == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(5),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(6),
			       0x00003000 | (0x1 << primary_data->dither_mode), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(7),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(8),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(9),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(10),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(11),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(12),
			       0x00000011, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(13),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(14),
			       0x00000000, ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_DITHER_CFG,
		enable << 1 | ((primary_data->relay_state != 0) || !enable),
		(0x1 << 1) | DITHER_RELAY_MODE);

	if (dither_data->set_partial_update != 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DITHER_SIZE,
			width << 16 | cfg->h, ~0);
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : dither_data->tile_overhead_v.overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DITHER_SIZE,
			width << 16 | (dither_data->roi_height + overhead_v * 2), ~0);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_DITHER_PURECOLOR0,
		primary_data->pure_clr_param->pure_clr_det, 0x1);
	if (primary_data->pure_clr_param->pure_clr_det && !dither_data->is_right_pipe)
		disp_dither_set_interrupt(comp, 1, handle);

	/* Bypass shadow register and read shadow register */
	if (dither_data->data->need_bypass_shadow)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_DITHER_SHADOW, 0x1, 0x1);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_DITHER_SHADOW, 0x0, 0x1);
}

static void disp_dither_init_primary_data(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither *companion_data = comp_to_dither(dither_data->companion);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;
	struct mtk_ddp_comp *gamma_comp;
	struct mtk_disp_gamma *gamma_data = NULL;
	int total_num = 0;

	if (dither_data->is_right_pipe) {
		kfree(primary_data->pure_clr_param);
		kfree(dither_data->primary_data);
		dither_data->primary_data = companion_data->primary_data;
		return;
	}
	gamma_comp = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc, MTK_DISP_GAMMA, 0);
	if (gamma_comp) {
		gamma_data = comp_to_gamma(gamma_comp);
		primary_data->gamma_data_mode = &gamma_data->primary_data->data_mode;
	}
	primary_data->dither_mode = 1;
	primary_data->pure_detect_wq =
		create_singlethread_workqueue("pure_detect_wq");
	INIT_WORK(&primary_data->work_data.pure_detect_task, disp_dither_pure_detect_work);
	total_num = mtk_ddp_comp_get_total_num_by_type(comp->mtk_crtc, MTK_DISP_DITHER);
	primary_data->relay_state = 0x0 << PQ_FEATURE_DEFAULT;
	// if current crtc has two dither: first is for pq, second is for pc,
	// relay first dither hw to avoid loss of accuracy for pc
	DDPINFO("%s: comp: %s, path_order: %d, total_num: %d\n",
		__func__, mtk_dump_comp_str(comp), dither_data->path_order, total_num);
	if ((dither_data->path_order == 0) && (total_num == 2))
		primary_data->relay_state = 0x1 << PQ_FEATURE_DEFAULT;
}

static void disp_dither_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s cfg->bpc=%d\n", __func__, cfg->bpc);

	disp_dither_config(comp, cfg, handle);
}

static void disp_dither_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_DITHER_EN, 0x1, 0x1);
}

static void disp_dither_stop(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;

	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_DITHER_EN, 0x0, 0x1);

	if (primary_data->pure_clr_param->pure_clr_det)
		disp_dither_set_interrupt(comp, 0, handle);
}

static void disp_dither_bypass(struct mtk_ddp_comp *comp, int bypass,
			      int caller, struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;
	struct mtk_ddp_comp *companion = dither_data->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller, primary_data->relay_state);

	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_DITHER_CFG,
				DITHER_RELAY_MODE, DITHER_RELAY_MODE);
			if (comp->mtk_crtc->is_dual_pipe && companion)
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_REG_DITHER_CFG,
					DITHER_RELAY_MODE, DITHER_RELAY_MODE);
		}
		primary_data->relay_state |= (0x1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~(1 << caller);
			if (primary_data->relay_state == 0) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_REG_DITHER_CFG, 0x0, DITHER_RELAY_MODE);
				if (comp->mtk_crtc->is_dual_pipe && companion)
					cmdq_pkt_write(handle, companion->cmdq_base,
						companion->regs_pa + DISP_REG_DITHER_CFG,
						0x0, DITHER_RELAY_MODE);
			}
		}
	}
}

static int disp_dither_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{

	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_dither *data = comp_to_dither(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_dither *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_dither(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_dither_init_primary_data(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_dither_init_primary_data(data->companion);
	}
		break;
	default:
		break;
	}
	return 0;
}

static void disp_dither_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&dither_data->is_clock_on, 1);
}

static void disp_dither_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);

	DDPINFO("%s %d ++\n", __func__, __LINE__);
	atomic_set(&dither_data->is_clock_on, 0);
	mtk_ddp_comp_clk_unprepare(comp);
	DDPINFO("%s %d --\n", __func__, __LINE__);
}

/* TODO */
/* partial update
 * static int _dither_partial_update(struct mtk_ddp_comp *comp, void *arg,
 * struct cmdq_pkt *handle)
 * {
 * struct disp_rect *roi = (struct disp_rect *) arg;
 * int width = roi->width;
 * int height = roi->height;
 *
 * cmdq_pkt_write(handle, comp->cmdq_base,
 * comp->regs_pa + DISP_REG_DITHER_SIZE, width << 16 | height, ~0);
 * return 0;
 * }
 *
 * static int disp_dither_io_cmd(struct mtk_ddp_comp *comp,
 * struct cmdq_pkt *handle,
 * enum mtk_ddp_io_cmd io_cmd,
 * void *params)
 * {
 * int ret = -1;
 * if (io_cmd == DDP_PARTIAL_UPDATE) {
 * _dither_partial_update(comp, params, handle);
 * ret = 0;
 * }
 * return ret;
 * }
 */

void disp_dither_set_param(struct mtk_ddp_comp *comp,
			struct cmdq_pkt *handle,
			bool relay, uint32_t mode)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;
	bool bypass = relay;
	uint32_t dither_mode = 0x00003000 | (0x1 << mode);

	pr_notice("%s: bypass: %d, dither_mode: %x", __func__, bypass, dither_mode);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DITHER_REG(6), dither_mode, ~0);

	disp_dither_bypass(comp, bypass, PQ_FEATURE_HAL_SET_DITHER_PARAM, handle);
}

static int disp_dither_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;

	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case BYPASS_DITHER:
	{
		int *value = data;

		disp_dither_bypass(comp, *value, PQ_FEATURE_HAL_COLOR_DETECT, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_dither1 = dither_data->companion;

			disp_dither_bypass(comp_dither1, *value, PQ_FEATURE_HAL_COLOR_DETECT, handle);
		}
	}
	break;
	case SET_INTERRUPT:
	{
		int *value = data;

		disp_dither_set_interrupt(comp, *value, handle);
	}
	break;
	case SET_COLOR_DETECT:
	{
		int *value = data;

		primary_data->pure_clr_param->pure_clr_det = *value;
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_DITHER_PURECOLOR0,
				primary_data->pure_clr_param->pure_clr_det, DISP_DITHER_CLR_DET);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_dither1 = dither_data->companion;
			cmdq_pkt_write(handle, comp_dither1->cmdq_base,
					comp_dither1->regs_pa + DISP_DITHER_PURECOLOR0,
					primary_data->pure_clr_param->pure_clr_det, DISP_DITHER_CLR_DET);
		}
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

int disp_dither_cfg_set_dither_param(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;
	int ret = 0;

	struct DISP_DITHER_PARAM *ditherParam = (struct DISP_DITHER_PARAM *)data;
	bool relay = ditherParam->relay;
	uint32_t mode = ditherParam->mode;
	struct mtk_disp_dither *dither = comp_to_dither(comp);

	primary_data->dither_mode = (unsigned int)(ditherParam->mode);
	DDPINFO("%s: relay: %d, mode: %d\n", __func__, relay, mode);

	disp_dither_set_param(comp, handle, relay, mode);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_dither1 = dither->companion;

		disp_dither_set_param(comp_dither1, handle, relay, mode);
	}

	return ret;
}

static int disp_dither_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_DITHER_SET_DITHER_PARAM:
		ret = disp_dither_cfg_set_dither_param(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_dither_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	dither_data->set_partial_update = enable;
	dither_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : dither_data->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (dither_data->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DITHER_SIZE,
			dither_data->roi_height + overhead_v * 2, 0x1fff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DITHER_SIZE,
			full_height, 0x1fff);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_dither_funcs = {
	.config = disp_dither_config,
	.first_cfg = disp_dither_first_cfg,
	.start = disp_dither_start,
	.stop = disp_dither_stop,
	.bypass = disp_dither_bypass,
	.user_cmd = disp_dither_user_cmd,
	.prepare = disp_dither_prepare,
	.unprepare = disp_dither_unprepare,
	.config_overhead = disp_dither_config_overhead,
	.config_overhead_v = disp_dither_config_overhead_v,
	/* partial update
	 * .io_cmd = disp_dither_io_cmd,
	 */
	.io_cmd = disp_dither_io_cmd,
	.pq_frame_config = disp_dither_frame_config,
	.partial_update = disp_dither_set_partial_update,
};

static int disp_dither_bind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void disp_dither_unbind(struct device *dev, struct device *master,
				   void *data)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dither_component_ops = {
	.bind	= disp_dither_bind,
	.unbind = disp_dither_unbind,
};

void disp_dither_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

void disp_dither_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *dither = comp_to_dither(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp),
			comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp),
			dither->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x164; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && dither->companion) {
		baddr = dither->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(dither->companion),
				dither->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(dither->companion));
		for (k = 0; k <= 0x164; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(dither->companion));
	}
}

static void disp_dither_parse_dts(const struct device_node *np,
	enum mtk_ddp_comp_id comp_id, struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *dither_data = comp_to_dither(comp);
	struct mtk_disp_dither_primary *primary_data = dither_data->primary_data;

	if (of_property_read_u32(np, "pure-clr-det",
		&primary_data->pure_clr_param->pure_clr_det)) {
		DDPPR_ERR("comp_id: %d, pure_clr_det = %d\n",
			comp_id, primary_data->pure_clr_param->pure_clr_det);
		primary_data->pure_clr_param->pure_clr_det = 0;
	}

	if (of_property_read_u32(np, "pure-clr-num",
		&primary_data->pure_clr_param->pure_clr_num)) {
		DDPPR_ERR("comp_id: %d, pure_clr_num = %d\n",
			comp_id, primary_data->pure_clr_param->pure_clr_num);
		primary_data->pure_clr_param->pure_clr_num = 0;
	}

	if (of_property_read_u32_array(np, "pure-clr-rgb",
		&primary_data->pure_clr_param->pure_clr[0][0],
		PURE_CLR_RGB * primary_data->pure_clr_param->pure_clr_num)) {
		DDPPR_ERR("comp_id: %d, get pure_clr error\n", comp_id);
		memset(&primary_data->pure_clr_param->pure_clr,
			0, sizeof(primary_data->pure_clr_param->pure_clr));
	}
}

static int disp_dither_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dither *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;
	int irq;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		goto error_primary;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DITHER);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dither_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, disp_dither_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	priv->primary_data->pure_clr_param = kzalloc(
			sizeof(*priv->primary_data->pure_clr_param), GFP_KERNEL);
	if (priv->primary_data->pure_clr_param == NULL) {
		ret = -1;
		goto error_primary;
	}
	disp_dither_parse_dts(dev->of_node, comp_id, &priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_dither_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int disp_dither_remove(struct platform_device *pdev)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dither_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_dither_data mt6768_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_dither_data mt6761_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_dither_data mt6765_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6885_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_dither_data mt6877_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6853_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6833_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6781_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6983_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6895_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6879_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6985_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6897_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6899_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6886_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6989_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6878_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6991_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_dither_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6761-disp-dither",
	  .data = &mt6761_dither_driver_data},
	{ .compatible = "mediatek,mt6765-disp-dither",
	  .data = &mt6765_dither_driver_data},
	{ .compatible = "mediatek,mt6768-disp-dither",
	  .data = &mt6768_dither_driver_data},
	{ .compatible = "mediatek,mt6781-disp-dither",
	  .data = &mt6781_dither_driver_data},
	{ .compatible = "mediatek,mt6885-disp-dither",
	  .data = &mt6885_dither_driver_data},
	{ .compatible = "mediatek,mt6877-disp-dither",
	  .data = &mt6877_dither_driver_data},
	{ .compatible = "mediatek,mt6853-disp-dither",
	  .data = &mt6853_dither_driver_data},
	{ .compatible = "mediatek,mt6833-disp-dither",
	  .data = &mt6833_dither_driver_data},
	{ .compatible = "mediatek,mt6983-disp-dither",
	  .data = &mt6983_dither_driver_data},
	{ .compatible = "mediatek,mt6895-disp-dither",
	  .data = &mt6895_dither_driver_data},
	{ .compatible = "mediatek,mt6879-disp-dither",
	  .data = &mt6879_dither_driver_data},
	{ .compatible = "mediatek,mt6985-disp-dither",
	  .data = &mt6985_dither_driver_data},
	{ .compatible = "mediatek,mt6886-disp-dither",
	  .data = &mt6886_dither_driver_data},
	{ .compatible = "mediatek,mt6835-disp-dither",
	  .data = &mt6835_dither_driver_data},
	{ .compatible = "mediatek,mt6897-disp-dither",
	  .data = &mt6897_dither_driver_data},
	{ .compatible = "mediatek,mt6989-disp-dither",
	  .data = &mt6989_dither_driver_data},
	{ .compatible = "mediatek,mt6878-disp-dither",
	  .data = &mt6878_dither_driver_data},
	{ .compatible = "mediatek,mt6991-disp-dither",
	  .data = &mt6991_dither_driver_data},
	{ .compatible = "mediatek,mt6899-disp-dither",
	  .data = &mt6899_dither_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_dither_driver_dt_match);

struct platform_driver mtk_disp_dither_driver = {
	.probe = disp_dither_probe,
	.remove = disp_dither_remove,
	.driver = {
			.name = "mediatek-disp-dither",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_dither_driver_dt_match,
		},
};

void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_DITHER, 0);
	int ret;

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_DITHER, &bypass);

	DDPINFO("%s : ret = %d\n", __func__, ret);
}

void disp_dither_set_color_detect(struct drm_crtc *crtc, int enable)
{
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int num = mtk_ddp_comp_get_total_num_by_type(mtk_crtc, MTK_DISP_DITHER);
	int index = 0;

	if (num == 2)
		index = 1;
	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_DITHER, index);

	mtk_crtc_user_cmd(crtc, comp, SET_COLOR_DETECT, &enable);
	mtk_crtc_user_cmd(crtc, comp, SET_INTERRUPT, &enable);
	mtk_crtc_check_trigger(mtk_crtc, true, true);
}

unsigned int disp_dither_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_dither *dither_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_DITHER, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	dither_data = comp_to_dither(comp);

	return dither_data->primary_data->relay_state != 0 ? 1 : 0;
}
