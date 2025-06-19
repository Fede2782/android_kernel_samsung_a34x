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
#include "mtk_disp_pq_helper.h"

#define DMDP_AAL_EN		0x0000
#define DMDP_AAL_CFG		0x0020
#define DMDP_AAL_CFG_MAIN	0x0200
#define DMDP_AAL_SIZE		0x0030
#define DMDP_AAL_OUTPUT_SIZE	0x0034
#define DMDP_AAL_OUTPUT_OFFSET  0x0038
#define DMDP_AAL_SHADOW_CTRL    0x0F0
#define AAL_BYPASS_SHADOW	BIT(0)
#define AAL_READ_WRK_REG	BIT(2)
#define DMDP_AAL_DRE_BITPLUS_00 0x048C
#define DMDP_AAL_DRE_BILATERAL	0x053C
#define DMDP_AAL_Y2R_00		0x04BC
#define DMDP_AAL_R2Y_00		0x04D4

// DMDP AAL REGISTER
#define DMDP_AAL_TILE_02			(0x0F4)
#define DMDP_AAL_TILE_00			(0x4EC)
#define DMDP_AAL_DRE_ROI_00			(0x520)
#define DMDP_AAL_DRE_ROI_01			(0x524)

#define DMDP_AAL_MISC_CTRL			(0x600)

struct mtk_dmdp_aal_data {
	bool support_shadow;
	bool need_bypass_shadow;
	bool roi_16_bit;
	u32 block_info_00_mask;
};

struct mtk_disp_mdp_primary {
	int dre30_support;
	int blk_num_y_start;
	int blk_num_y_end;
	int blk_cnt_y_start;
	int blk_cnt_y_end;
	int dre_blk_height;
	bool aal_param_valid;
	unsigned int relay_state;
};

struct mtk_disp_mdp_aal_tile_overhead {
	unsigned int width;
	unsigned int comp_overhead;
};

struct mtk_disp_mdp_aal_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_dmdp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_dmdp_aal_data *data;

	struct mtk_disp_mdp_aal_tile_overhead tile_overhead; //disp_mdp_aal_tile_overhead
	struct mtk_disp_mdp_aal_tile_overhead_v tile_overhead_v;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_mdp_primary *primary_data;
	unsigned int set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_dmdp_aal *comp_to_dmdp_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dmdp_aal, ddp_comp);
}

static void disp_mdp_aal_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN, 1, ~0);
}

void disp_mdp_aal_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle)
{
	struct mtk_dmdp_aal *data = comp_to_dmdp_aal(comp);
	struct mtk_disp_mdp_primary *primary_data = data->primary_data;
	struct mtk_ddp_comp *companion = data->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x, dre30_support: %d\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller,
		primary_data->relay_state, primary_data->dre30_support);

	if (!primary_data->dre30_support) {
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG, 0x00400003, ~0);
		return;
	}

	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_CFG, 0x00400003, ~0);
			if (comp->mtk_crtc->is_dual_pipe && companion)
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DMDP_AAL_CFG, 0x00400003, ~0);
		}
		primary_data->relay_state |= (1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~(1 << caller);
			if (primary_data->relay_state == 0) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DMDP_AAL_CFG, 0x00400026, ~0);
				if (comp->mtk_crtc->is_dual_pipe && companion)
					cmdq_pkt_write(handle, companion->cmdq_base,
						companion->regs_pa + DMDP_AAL_CFG, 0x00400026, ~0);
			}
		}
	}
}

static void disp_mdp_aal_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_dmdp_aal *data = comp_to_dmdp_aal(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!data->is_right_pipe) {
			data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			data->tile_overhead.width =
				cfg->tile_overhead.left_in_width;
		} else {
			data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			data->tile_overhead.width =
				cfg->tile_overhead.right_in_width;
		}
	}
}

static void disp_mdp_aal_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_dmdp_aal *data = comp_to_dmdp_aal(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void disp_mdp_aal_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int overhead_v = 0;
	struct mtk_dmdp_aal *data = comp_to_dmdp_aal(comp);
	uint32_t out_hoffset = 0, out_width = cfg->w, width = cfg->w, size = 0, out_size = 0;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		width = data->tile_overhead.width;
		out_width = width - data->tile_overhead.comp_overhead;
		if (data->is_right_pipe)
			out_hoffset = data->tile_overhead.comp_overhead;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;

		out_width = width;
	}

	if (data->set_partial_update != 1) {
		size = (width << 16) | cfg->h;
		out_size = (out_width << 16) | cfg->h;
	} else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v) ? 0 : data->tile_overhead_v.overhead_v;
		size = (width << 16) | (data->roi_height + overhead_v * 2);
		out_size = (out_width << 16) | (data->roi_height + overhead_v * 2);
	}

	DDPINFO("%s: size 0x%08x\n", __func__, size);

	if (data->primary_data->dre30_support == 0 || data->primary_data->relay_state != 0)
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG, 0x00400003, ~0);
	else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG, 0x00400026, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SIZE, size, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_OUTPUT_SIZE, out_size, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_OUTPUT_OFFSET, (out_hoffset << 16) | 0, ~0);
	if (data->data->roi_16_bit) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_ROI_00, (out_width - 1) << 16 | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_ROI_01, (cfg->h - 1) << 16 | 0, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_ROI_00, (out_width - 1) << 13 | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_ROI_01, (cfg->h - 1) << 13 | 0, ~0);
	}
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_DRE_BITPLUS_00, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_Y2R_00, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_R2Y_00, 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_MISC_CTRL, 1, ~0);

	if (data->data->need_bypass_shadow)
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SHADOW_CTRL, 1, AAL_BYPASS_SHADOW);
	else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SHADOW_CTRL, 0, AAL_BYPASS_SHADOW);

	DDPINFO("%s [comp_id:%d]: g_dmdp_aal_relay_state[0x%x]\n",
		__func__, comp->id, data->primary_data->relay_state);
}

static void disp_mdp_aal_init_primary_data(struct mtk_ddp_comp *comp)
{
	struct mtk_dmdp_aal *aal_data = comp_to_dmdp_aal(comp);
	struct mtk_dmdp_aal *companion_data = comp_to_dmdp_aal(aal_data->companion);

	if (aal_data->is_right_pipe) {
		kfree(aal_data->primary_data);
		aal_data->primary_data = NULL;
		aal_data->primary_data = companion_data->primary_data;
		return;
	}

	// init primary data
	aal_data->primary_data->relay_state = 0x0 << PQ_FEATURE_DEFAULT;
}

void disp_mdp_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	disp_mdp_aal_config(comp, cfg, handle);
}

static void disp_mdp_aal_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s: call aal prepare\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);
}

int disp_mdp_aal_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_dmdp_aal *data = comp_to_dmdp_aal(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_dmdp_aal *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_dmdp_aal(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_mdp_aal_init_primary_data(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_mdp_aal_init_primary_data(data->companion);
	}
		break;
	default:
		break;
	}

	return 0;
}

static void disp_mdp_aal_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s: call aal unprepare\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void disp_mdp_aal_update_pu_region_setting(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi)
{
	int roi_height_y_start;
	int roi_height_y_end;
	int blk_height;
	int blk_num_y_start;
	int blk_num_y_end;
	int blk_cnt_y_start;
	int blk_cnt_y_end;
	unsigned int blk_y_start_idx;
	unsigned int blk_y_end_idx;

	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);

	roi_height_y_start = partial_roi.y;
	roi_height_y_end = partial_roi.y + partial_roi.height -1;
	blk_height = dmdp_aal->primary_data->dre_blk_height;

	blk_y_start_idx = roi_height_y_start / blk_height;
	blk_y_end_idx = roi_height_y_end / blk_height;

	if (dmdp_aal->set_partial_update == 1) {
		// blk_num_y
		blk_num_y_start = blk_y_start_idx;
		blk_num_y_end = blk_y_end_idx;

		// blk_cnt_y
		blk_cnt_y_start = roi_height_y_start - (blk_height * blk_y_start_idx);
		blk_cnt_y_end = roi_height_y_end - (blk_height * blk_y_end_idx);
	} else {
		// blk_num_y
		blk_num_y_start = dmdp_aal->primary_data->blk_num_y_start;
		blk_num_y_end = dmdp_aal->primary_data->blk_num_y_end;

		// blk_cnt_y
		blk_cnt_y_start = dmdp_aal->primary_data->blk_cnt_y_start;
		blk_cnt_y_end = dmdp_aal->primary_data->blk_cnt_y_end;
	}

	DDPDBG("blk_num_y_start: %d, blk_num_y_end: %d, blk_cnt_y_start: %d, blk_cnt_y_end: %d\n",
			blk_num_y_start, blk_num_y_end, blk_cnt_y_start, blk_cnt_y_end);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_TILE_00,
		(blk_num_y_end << 5) | blk_num_y_start, 0x3FF);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_TILE_02,
		(blk_cnt_y_end << 16) | blk_cnt_y_start, ~0);
}

static int disp_mdp_aal_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_dmdp_aal *data = comp_to_dmdp_aal(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s set partial update(enable: %d), roi: (x: %d, y: %d, width: %d, height: %d)\n",
			mtk_dump_comp_str(comp), enable,
			partial_roi.x, partial_roi.y, partial_roi.width, partial_roi.height);

	data->set_partial_update = enable;
	data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : data->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (data->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SIZE,
				data->roi_height + overhead_v * 2, 0x0FFFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_OUTPUT_SIZE, data->roi_height + overhead_v * 2, 0x0FFFF);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SIZE,
				full_height, 0x0FFFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_OUTPUT_SIZE, full_height, 0x0FFFF);
	}

	disp_mdp_aal_update_pu_region_setting(comp, handle, partial_roi);

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dmdp_aal_funcs = {
	.config = disp_mdp_aal_config,
	.first_cfg = disp_mdp_aal_first_cfg,
	.start = disp_mdp_aal_start,
	.bypass = disp_mdp_aal_bypass,
	.prepare = disp_mdp_aal_prepare,
	.unprepare = disp_mdp_aal_unprepare,
	.config_overhead = disp_mdp_aal_config_overhead,
	.config_overhead_v = disp_mdp_aal_config_overhead_v,
	.io_cmd = disp_mdp_aal_io_cmd,
	.partial_update = disp_mdp_aal_set_partial_update,
};

static int disp_mdp_aal_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		DDPMSG("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void disp_mdp_aal_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_dmdp_aal_component_ops = {
	.bind = disp_mdp_aal_bind, .unbind = disp_mdp_aal_unbind,
};

void disp_mdp_aal_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x200, 0xf4, 0xf8, 0x468);
	mtk_cust_dump_reg(baddr, 0x46c, 0x470, 0x474, 0x478);
	mtk_cust_dump_reg(baddr, 0x4ec, 0x4f0, 0x528, 0x52c);
}

void disp_mdp_aal_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp),
			dmdp_aal->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x600; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && dmdp_aal->companion) {
		baddr = dmdp_aal->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(dmdp_aal->companion),
				&dmdp_aal->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(dmdp_aal->companion));
		for (k = 0; k <= 0x600; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(dmdp_aal->companion));
	}
}

void disp_mdp_aal_init_data_update(struct mtk_ddp_comp *comp,
		const struct DISP_AAL_INITREG *init_regs)
{
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);

	dmdp_aal->primary_data->dre_blk_height = init_regs->dre_blk_height;
	dmdp_aal->primary_data->blk_num_y_start = init_regs->blk_num_y_start;
	dmdp_aal->primary_data->blk_num_y_end = init_regs->blk_num_y_end;
	dmdp_aal->primary_data->blk_cnt_y_start = init_regs->blk_cnt_y_start;
	dmdp_aal->primary_data->blk_cnt_y_end = init_regs->blk_cnt_y_end;
}

void disp_mdp_aal_set_valid(struct mtk_ddp_comp *comp, bool valid)
{
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);

	dmdp_aal->primary_data->aal_param_valid = valid;
}

static int disp_mdp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dmdp_aal *priv;
	struct device_node *aal_node;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPMSG("%s\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DMDP_AAL);
	if ((int)comp_id < 0) {
		DDPMSG("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_dmdp_aal_funcs);
	if (ret != 0) {
		DDPMSG("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	aal_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_aal0");
	if (of_property_read_u32(aal_node, "mtk-dre30-support",
		&priv->primary_data->dre30_support)) {
		DDPMSG("comp_id: %d, mtk_dre30_support = %d\n",
			comp_id, priv->primary_data->dre30_support);
		ret = -EINVAL;
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_dmdp_aal_component_ops);
	if (ret != 0) {
		DDPMSG("Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int disp_mdp_aal_remove(struct platform_device *pdev)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_dmdp_aal_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_dmdp_aal_data mt6885_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = false,
	.roi_16_bit = false,
	.block_info_00_mask = 0x3FFFFFF,
};

static const struct mtk_dmdp_aal_data mt6895_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6983_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6985_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6897_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6989_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6991_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6899_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.roi_16_bit = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct of_device_id mtk_dmdp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-dmdp-aal", .data = &mt6885_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6983-dmdp-aal", .data = &mt6983_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6895-dmdp-aal", .data = &mt6895_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6985-dmdp-aal", .data = &mt6985_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6897-dmdp-aal", .data = &mt6897_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6989-dmdp-aal", .data = &mt6989_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6991-dmdp-aal", .data = &mt6991_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6899-dmdp-aal", .data = &mt6899_dmdp_aal_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_dmdp_aal_driver_dt_match);

struct platform_driver mtk_dmdp_aal_driver = {
	.probe = disp_mdp_aal_probe,
	.remove = disp_mdp_aal_remove,
	.driver = {

			.name = "mediatek-dmdp-aal",
			.owner = THIS_MODULE,
			.of_match_table = mtk_dmdp_aal_driver_dt_match,
		},
};

unsigned int disp_mdp_aal_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_dmdp_aal *aal_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DMDP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	aal_data = comp_to_dmdp_aal(comp);

	return aal_data->primary_data->relay_state != 0 ? 1 : 0;
}
