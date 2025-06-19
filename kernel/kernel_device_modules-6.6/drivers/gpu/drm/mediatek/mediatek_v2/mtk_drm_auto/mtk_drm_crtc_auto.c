// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_crtc.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_log.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_crtc_auto.h"
#include "../mtk_drm_drv.h"

static struct drm_device *drm_dev;

bool mtk_drm_crtc_layer_need_swap(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *output_comp;

	/* dsi0 and dp_intf0 path, primary plane on the top of all plane
	 * other path, primary plane at the bottom of all plane
	 */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp &&
	    (output_comp->id == DDP_COMPONENT_DSI0 ||
	     output_comp->id == DDP_COMPONENT_DP_INTF0))
		return true;
	else
		return false;
}

struct mtk_ddp_comp *mtk_crtc_get_comp_with_index(struct mtk_drm_crtc *mtk_crtc,
						  struct mtk_plane_state *plane_state)
{
	struct drm_plane *plane = plane_state->base.plane;
	struct drm_plane *base_plane = &mtk_crtc->planes[0].base;
	unsigned int local_index = 0, exdma_idx = 0;

	unsigned int i, j;
	struct mtk_ddp_comp *comp;

	if (!plane) {
		local_index = plane_state->comp_state.lye_id;
	} else {

		local_index = plane->index - base_plane->index;

		if (mtk_drm_crtc_layer_need_swap(mtk_crtc))
			local_index = mtk_crtc->layer_nr - 1 - local_index;

		DDPINFO("%s plane index %d base %d\n", __func__, plane->index, base_plane->index);
	}

	DDPINFO("%s crtc %d local_index %d\n", __func__, drm_crtc_index(&mtk_crtc->base), local_index);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) && !mtk_ddp_comp_is_virt(comp)) {
			if (exdma_idx == local_index) {
				DDPINFO("%s get comp %s\n",
					__func__, mtk_dump_comp_str(comp));
				return comp;
			}

			exdma_idx++;
		}
	}

	return NULL;
}

int mtk_drm_crtc_init_plane(struct drm_device *drm_dev, struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	unsigned int zpos;
	enum drm_plane_type type;
	int ret;

	DDPMSG("%s CRTC%d layer_nr %d\n",
	       __func__, pipe, mtk_crtc->layer_nr);

	for (zpos = 0; zpos < mtk_crtc->layer_nr; zpos++) {
		if (zpos == 0)
			type = DRM_PLANE_TYPE_PRIMARY;
		else
			type = DRM_PLANE_TYPE_OVERLAY;

		ret = mtk_plane_init(drm_dev, &mtk_crtc->planes[zpos], zpos,
				     BIT(pipe), type);
		if (ret) {
			DDPMSG("[E] %s CRTC%d plane %d init fail!\n", __func__, pipe, zpos);
			return ret;
		}
	}

	return 0;
}

void mtk_get_output_timing(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct mtk_ddp_comp *comp;
	struct drm_display_mode timing;
	unsigned int connector_enable = 0;
	int ret = 0;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL)
		return;

	drm_mode_copy(&timing, &crtc->state->adjusted_mode);

	// backup panel timing for android using.
	mtk_drm_backup_default_timing(mtk_crtc, &timing);

	mtk_ddp_comp_io_cmd(comp, NULL, CONNECTOR_IS_ENABLE, &connector_enable);
	mtk_drm_connector_notify_guest(mtk_crtc, connector_enable);
}

static void set_value_to_regs_field(u16 in_value, void __iomem *base_addr, int field)
{
	u32 val = 0, value = 0, mask = 0;

	SET_VAL_MASK(value, mask,
		in_value, field);

	val = readl(base_addr);
	val = (val & ~mask) | (value & mask);
	writel(val, base_addr);
}

void store_timing_to_dummy(struct drm_display_mode *timing,
	void __iomem *regs_base, enum mtk_ddp_comp_id id)
{
	if ((timing != NULL) && (regs_base != NULL)) {
		if (id == DDP_COMPONENT_DSI0) {
			set_value_to_regs_field(timing->hdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY0, DSI_HDISPLAY_VALUE);
			set_value_to_regs_field(timing->vdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY0, DSI_VDISPLAY_VALUE);
			set_value_to_regs_field(timing->hsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY1, DSI_HSYNC_START_VALUE);
			set_value_to_regs_field(timing->hsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY1, DSI_HSYNC_END_VALUE);
			set_value_to_regs_field(timing->vsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY2, DSI_VSYNC_START_VALUE);
			set_value_to_regs_field(timing->vsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY2, DSI_VSYNC_END_VALUE);
			set_value_to_regs_field(timing->htotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY3, DSI_HSYNC_TOTAL_VALUE);
			set_value_to_regs_field(timing->vtotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY3, DSI_VSYNC_TOTAL_VALUE);
			writel(timing->clock, regs_base + MT6991_OVL_MDP_RSZ0_DUMMY4);
			set_value_to_regs_field(timing->height_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY5, DSI_HEIGHT_MM_VALUE);
			set_value_to_regs_field(timing->width_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY5, DSI_WIDTH_MM_VALUE);
		} else if (id == DDP_COMPONENT_DP_INTF0) {
			set_value_to_regs_field(timing->hdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY6, DP_HDISPLAY_VALUE);
			set_value_to_regs_field(timing->vdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY6, DP_VDISPLAY_VALUE);
			set_value_to_regs_field(timing->hsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY7, DP_HSYNC_START_VALUE);
			set_value_to_regs_field(timing->hsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY7, DP_HSYNC_END_VALUE);
			set_value_to_regs_field(timing->vsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY8, DP_VSYNC_START_VALUE);
			set_value_to_regs_field(timing->vsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY8, DP_VSYNC_END_VALUE);
			set_value_to_regs_field(timing->htotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY9, DP_HSYNC_TOTAL_VALUE);
			set_value_to_regs_field(timing->vtotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY9, DP_VSYNC_TOTAL_VALUE);
			writel(timing->clock, regs_base + MT6991_OVL_MDP_RSZ0_DUMMY10);
			set_value_to_regs_field(timing->height_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY11, DP_HEIGHT_MM_VALUE);
			set_value_to_regs_field(timing->width_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY11, DP_WIDTH_MM_VALUE);
		}

		DDPMSG("%s,%d,[%d]DUMMY 0=0x%x, 1=0x%x 2=0x%x, 3=0x%x 4=0x%x, 5=0x%x\n",
			__func__, __LINE__, id,
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY6),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY7),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY8),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY9),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY10),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY11));

	} else {
		DDPMSG("[E] %s timing or regs_base is error!", __func__);
	}
}

void mtk_drm_backup_default_timing(struct mtk_drm_crtc *mtk_crtc, struct drm_display_mode *timing)
{
	void __iomem *regs_base = NULL;
	struct mtk_ddp_comp *comp;

	DDPMSG("%s+, %d\n", __func__, __LINE__);

	if (timing == NULL) {
		DDPMSG("[E] %s invalid timing!", __func__);
		return;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL) {
		DDPMSG("[E] %s get comp fail!", __func__);
		return;
	}

	regs_base = ioremap(DUMMY_REG_BASE, 0x1000);
	if (regs_base == NULL) {
		DDPMSG("[E] %s regs base ioremap fail!", __func__);
		return;
	}

	store_timing_to_dummy(timing, regs_base, comp->id);

	iounmap(regs_base);
}

void mtk_drm_connector_notify_guest(struct mtk_drm_crtc *mtk_crtc, unsigned int connector_enable)
{
	void __iomem *regs_base = NULL;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL) {
		DDPMSG("[E] %s get comp fail!", __func__);
		return;
	}

	regs_base = ioremap(DUMMY_REG_BASE, 0x1000);
	if (regs_base == NULL) {
		DDPMSG("[E] %s regs base ioremap fail!", __func__);
		return;
	}

	if (comp->id == DDP_COMPONENT_DP_INTF0)
		set_value_to_regs_field(connector_enable, regs_base +
				MT6991_OVL_MDP_RSZ0_DUMM20, DP_INTF0_CONNECTOR_READY);

	iounmap(regs_base);
}

void mtk_drm_crtc_init_layer_nr(struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	unsigned int i, j;
	struct mtk_ddp_comp *comp;

	mtk_crtc->layer_nr = 0;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) && !mtk_ddp_comp_is_virt(comp)) {
			DDPINFO("%s layer %d comp %s\n",
				__func__, mtk_crtc->layer_nr, mtk_dump_comp_str(comp));
			mtk_crtc->layer_nr++;
		}
	}

	DDPINFO("%s crtc%d layer_nr %d\n", __func__, pipe, mtk_crtc->layer_nr);
}

void mtk_drm_crtc_dev_init(struct drm_device *dev)
{
	if (IS_ERR_OR_NULL(dev))
		DDPMSG("%s, invalid dev\n", __func__);

	drm_dev = dev;
}

static void mtk_drm_crtc_layer_off(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle)
{
	unsigned int i, j;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) && mtk_ddp_comp_is_virt(comp)) {
			DDPMSG("%s crtc%d layer off %s\n",
				__func__, drm_crtc_index(&mtk_crtc->base),
				mtk_dump_comp_str(comp));
			mtk_ddp_comp_stop(comp, handle);
		}
	}
}

void mtk_drm_crtc_vhost_layer_off(void)
{
	u32 i;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct cmdq_pkt *cmdq_handle = NULL;

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return;
	}

	priv = drm_dev->dev_private;
	if (IS_ERR_OR_NULL(priv)) {
		DDPMSG("%s, invalid priv\n", __func__);
		return;
	}

	DDPMSG("%s+\n", __func__);

	for (i = 0; i < MAX_CRTC; i++) {
		if (!priv->crtc[i])
			continue;

		mtk_crtc = to_mtk_crtc(priv->crtc[i]);
		if (!mtk_crtc)
			continue;

		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

		mtk_drm_crtc_layer_off(mtk_crtc, cmdq_handle);

		if (cmdq_handle) {
			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
		}
	}

	DDPMSG("%s-\n", __func__);
}
EXPORT_SYMBOL(mtk_drm_crtc_vhost_layer_off);

#endif
