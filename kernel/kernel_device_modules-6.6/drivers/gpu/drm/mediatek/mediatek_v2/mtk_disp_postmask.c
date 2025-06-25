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
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_disp_postmask.h"
#include "platform/mtk_drm_platform.h"

int debug_postmask_bw;
module_param(debug_postmask_bw, int, 0644);

#define POSTMASK_MASK_MAX_NUM 96
#define POSTMASK_GRAD_MAX_NUM 192
#define POSTMASK_DRAM_MODE

#define DISP_POSTMASK_EN 0x0
#define DISP_POSTMASK_INTEN 0x8
#define INTEN_FLD_PM_IF_FME_END_INTEN REG_FLD_MSB_LSB(0, 0)
#define INTEN_FLD_PM_FME_CPL_INTEN REG_FLD_MSB_LSB(1, 1)
#define INTEN_FLD_PM_START_INTEN REG_FLD_MSB_LSB(2, 2)
#define INTEN_FLD_PM_ABNORMAL_SOF_INTEN REG_FLD_MSB_LSB(4, 4)
#define INTEN_FLD_RDMA_FME_UND_INTEN REG_FLD_MSB_LSB(8, 8)
#define INTEN_FLD_RDMA_FME_SWRST_DONE_INTEN REG_FLD_MSB_LSB(9, 9)
#define INTEN_FLD_RDMA_FME_HWRST_DONE_INTEN REG_FLD_MSB_LSB(10, 10)
#define INTEN_FLD_RDMA_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(11, 11)
#define INTEN_FLD_RDMA_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(12, 12)
#define DISP_POSTMASK_INTSTA 0xC
#define DISP_POSTMASK_CFG 0x20
#define DISP_POSTMASK_SHADOW_CTRL 0x24
#define READ_WRK_REG REG_FLD_MSB_LSB(2, 2)
#define BYPASS_SHADOW REG_FLD_MSB_LSB(1, 1)
#define FORCE_COMMIT REG_FLD_MSB_LSB(0, 0)
#define CFG_FLD_STALL_CG_ON REG_FLD_MSB_LSB(8, 8)
#define CFG_FLD_GCLAST_EN REG_FLD_MSB_LSB(6, 6)
#define CFG_FLD_BGCLR_IN_SEL REG_FLD_MSB_LSB(2, 2)
#define CFG_FLD_DRAM_MODE REG_FLD_MSB_LSB(1, 1)
#define CFG_FLD_RELAY_MODE REG_FLD_MSB_LSB(0, 0)
#define DISP_POSTMASK_SHADOW_CTRL 0x24
#define CFG_FLD_SHADOW_CTRL_BYPASS_SHADOW REG_FLD_MSB_LSB(1, 1)
#define DISP_POSTMASK_SIZE 0x30
#define DISP_POSTMASK_SRAM_CFG 0x40
#define SRAM_CFG_FLD_MASK_NUM_SW_SET REG_FLD_MSB_LSB(11, 4)
#define SRAM_CFG_FLD_MASK_L_TOP_EN REG_FLD_MSB_LSB(3, 3)
#define SRAM_CFG_FLD_MASK_L_BOTTOM_EN REG_FLD_MSB_LSB(2, 2)
#define SRAM_CFG_FLD_MASK_R_TOP_EN REG_FLD_MSB_LSB(1, 1)
#define SRAM_CFG_FLD_MASK_R_BOTTOM_EN REG_FLD_MSB_LSB(0, 0)
#define DISP_POSTMASK_BLEND_CFG 0x50
#define BLEND_CFG_FLD_CONST_BLD REG_FLD_MSB_LSB(2, 2)
#define BLEND_CFG_FLD_PARGB_BLD REG_FLD_MSB_LSB(1, 1)
#define BLEND_CFG_FLD_A_EN REG_FLD_MSB_LSB(0, 0)
#define DISP_POSTMASK_ROI_BGCLR 0x54
#define DISP_POSTMASK_MASK_CLR 0x58
#define DISP_REG_POSTMASK_SODI 0x60
#define PM_MASK_THRESHOLD_LOW_FOR_SODI  REG_FLD_MSB_LSB(13, 0)
#define PM_MASK_THRESHOLD_HIGH_FOR_SODI REG_FLD_MSB_LSB(29, 16)
#define DISP_POSTMASK_STATUS 0xA0
#define DISP_POSTMASK_INPUT_COUNT 0xA4
#define DISP_POSTMASK_MEM_ADDR 0x100
#define DISP_POSTMASK_MEM_LENGTH 0x104
#define DISP_POSTMASK_RDMA_FIFO_CTRL 0x108
#define DISP_POSTMASK_MEM_GMC_SETTING2 0x10C
#define MEM_GMC_FLD_FORCE_REQ_TH REG_FLD_MSB_LSB(30, 30)
#define MEM_GMC_FLD_REQ_TH_ULTRA REG_FLD_MSB_LSB(29, 29)
#define MEM_GMC_FLD_REQ_TH_PREULTRA REG_FLD_MSB_LSB(28, 28)
#define MEM_GMC_FLD_ISSUE_REQ_TH_URG REG_FLD_MSB_LSB(27, 16)
#define MEM_GMC_FLD_ISSUE_REQ_TH REG_FLD_MSB_LSB(11, 0)
#define DISP_POSTMASK_PAUSE_REGION 0x110
#define PAUSE_REGION_FLD_RDMA_PAUSE_END REG_FLD_MSB_LSB(27, 16)
#define PAUSE_REGION_FLD_RDMA_PAUSE_START REG_FLD_MSB_LSB(11, 0)
#define DISP_POSTMASK_MEM_ADDR_MSB 0x114
#define DISP_POSTMASK_RDMA_GREQ_NUM 0x130
#define GREQ_FLD_IOBUF_FLUSH_ULTRA REG_FLD_MSB_LSB(31, 31)
#define GREQ_FLD_IOBUF_FLUSH_PREULTRA REG_FLD_MSB_LSB(30, 30)
#define GREQ_FLD_GRP_BRK_STOP REG_FLD_MSB_LSB(29, 29)
#define GREQ_FLD_GRP_END_STOP REG_FLD_MSB_LSB(28, 28)
#define GREQ_FLD_GREQ_STOP_EN REG_FLD_MSB_LSB(27, 27)
#define GREQ_FLD_GREQ_DIS_CNT REG_FLD_MSB_LSB(26, 24)
#define GREQ_FLD_OSTD_GREQ_NUM REG_FLD_MSB_LSB(23, 16)
#define GREQ_FLD_GREQ_NUM_SHT REG_FLD_MSB_LSB(14, 13)
#define GREQ_FLD_GREQ_NUM_SHT_VAL REG_FLD_MSB_LSB(12, 12)
#define GREQ_FLD_GREQ_URG_NUM REG_FLD_MSB_LSB(7, 4)
#define GREQ_FLD_GREQ_NUM REG_FLD_MSB_LSB(3, 0)
#define DISP_POSTMASK_RDMA_GREQ_URG_NUM 0x134
#define GREQ_URG_FLD_ARB_URG_BIAS REG_FLD_MSB_LSB(12, 12)
#define GREQ_URG_FLD_ARB_GREQ_URG_TH REG_FLD_MSB_LSB(11, 0)
#define DISP_POSTMASK_RDMA_ULTRA_SRC 0x140
#define ULTRA_FLD_ULTRA_RDMA_SRC REG_FLD_MSB_LSB(15, 14)
#define ULTRA_FLD_ULTRA_ROI_END_SRC REG_FLD_MSB_LSB(13, 12)
#define ULTRA_FLD_ULTRA_SMI_SRC REG_FLD_MSB_LSB(11, 10)
#define ULTRA_FLD_ULTRA_BUF_SRC REG_FLD_MSB_LSB(9, 8)
#define ULTRA_FLD_PREULTRA_RDMA_SRC REG_FLD_MSB_LSB(7, 6)
#define ULTRA_FLD_PREULTRA_ROI_END_SRC REG_FLD_MSB_LSB(5, 4)
#define ULTRA_FLD_PREULTRA_SMI_SRC REG_FLD_MSB_LSB(3, 2)
#define ULTRA_FLD_PREULTRA_BUF_SRC REG_FLD_MSB_LSB(1, 0)
#define DISP_POSTMASK_RDMA_BUF_LOW_TH 0x144
#define TH_FLD_RDMA_PREULTRA_LOW_TH REG_FLD_MSB_LSB(23, 12)
#define TH_FLD_RDMA_ULTRA_LOW_TH REG_FLD_MSB_LSB(11, 0)
#define DISP_POSTMASK_RDMA_BUF_HIGH_TH 0x148
#define TH_FLD_RDMA_PREULTRA_HIGH_DIS REG_FLD_MSB_LSB(31, 31)
#define TH_FLD_RDMA_PREULTRA_HIGH_TH REG_FLD_MSB_LSB(23, 12)
#define DISP_POSTMASK_NUM_0 0x800
#define DISP_POSTMASK_NUM(n) (DISP_POSTMASK_NUM_0 + (0x4 * (n)))
#define DISP_POSTMASK_GRAD_VAL_0 0xA00
#define DISP_POSTMASK_GRAD_VAL(n) (DISP_POSTMASK_GRAD_VAL_0 + (0x4 * (n)))
#define DISP_POSTMASK_SLC (0x1FC)
#define POSTMASK_SLC REG_FLD_MSB_LSB(4, 0)
#define POSTMASK_VCSEL REG_FLD_MSB_LSB(5, 5)


static irqreturn_t mtk_postmask_irq_handler(int irq, void *dev_id) __always_unused;

struct mtk_disp_postmask {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	unsigned int underflow_cnt;
	unsigned int abnormal_cnt;
	const struct mtk_disp_postmask_data *data;
	unsigned int postmask_force_relay;
	unsigned int postmask_debug;
	unsigned int set_partial_update;
	unsigned int roi_y_offset;
	unsigned int roi_height;
	unsigned int pu_force_relay;
	dma_addr_t pu_mem_addr;
	unsigned int pu_mem_length;
	unsigned int pu_pause_start;
	unsigned int pu_pause_end;
};

struct mtk_disp_postmark_tile_overhead {
	unsigned int left_in_width;
	unsigned int left_overhead;
	unsigned int left_comp_overhead;
	unsigned int right_in_width;
	unsigned int right_overhead;
	unsigned int right_comp_overhead;
};

struct mtk_disp_postmask_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_postmark_tile_overhead postmark_tile_overhead = { 0 };
struct mtk_disp_postmask_tile_overhead_v postmask_tile_overhead_v = { 0 };

static inline struct mtk_disp_postmask *comp_to_postmask(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_postmask, ddp_comp);
}

static irqreturn_t mtk_postmask_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_postmask *priv = dev_id;
	struct mtk_ddp_comp *postmask = NULL;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	postmask = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(postmask))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(postmask) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(postmask->regs + DISP_POSTMASK_INTSTA);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(IRQ, postmask->regs_pa, val);
	DRM_MMP_MARK(postmask0, val, 0);

	if (val & 0x110)
		DRM_MMP_MARK(abnormal_irq, val, postmask->id);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(postmask), val);

	writel(~val, postmask->regs + DISP_POSTMASK_INTSTA);

	if (val & (1 << 0))
		DDPIRQ("[IRQ] %s: input frame end!\n",
		       mtk_dump_comp_str(postmask));
	if (val & (1 << 1))
		DDPIRQ("[IRQ] %s: output frame end!\n",
		       mtk_dump_comp_str(postmask));
	if (val & (1 << 2))
		DDPIRQ("[IRQ] %s: frame start!\n", mtk_dump_comp_str(postmask));
	if (val & (1 << 4)) {
		DDPPR_ERR("[IRQ] %s: abnormal SOF! cnt=%d\n",
			  mtk_dump_comp_str(postmask), priv->abnormal_cnt);
		priv->abnormal_cnt++;
	}

	//if (val & (1 << 8)) {
		//DDPPR_ERR("[IRQ] %s: frame underflow! cnt=%d\n",
			  //mtk_dump_comp_str(postmask), priv->underflow_cnt);
		//priv->underflow_cnt++;
	//}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put(postmask);

	return ret;
}


static void mtk_disp_postmark_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (comp->id == DDP_COMPONENT_POSTMASK0) {
			postmark_tile_overhead.left_comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				postmark_tile_overhead.left_comp_overhead;
			cfg->tile_overhead.left_in_width +=
				postmark_tile_overhead.left_comp_overhead;
			/*copy from total overhead info*/
			postmark_tile_overhead.left_in_width = cfg->tile_overhead.left_in_width;
			postmark_tile_overhead.left_overhead = cfg->tile_overhead.left_overhead;
		}
		if (comp->id == DDP_COMPONENT_POSTMASK1) {
			postmark_tile_overhead.right_comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				postmark_tile_overhead.right_comp_overhead;
			cfg->tile_overhead.right_in_width +=
				postmark_tile_overhead.right_comp_overhead;
			/*copy from total overhead info*/
			postmark_tile_overhead.right_in_width = cfg->tile_overhead.right_in_width;
			postmark_tile_overhead.right_overhead = cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_postmask_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	postmask_tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		postmask_tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	postmask_tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_postmask_config(struct mtk_ddp_comp *comp,
				struct mtk_ddp_config *cfg,
				struct cmdq_pkt *handle)
{
	unsigned int value;
	struct mtk_panel_params *panel_ext =
		mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
#ifndef POSTMASK_DRAM_MODE
	unsigned int i = 0;
	unsigned int num = 0;
#else
	struct mtk_drm_gem_obj *gem;
	unsigned int size = 0;
	dma_addr_t addr = 0;
	unsigned int force_relay = 0;
	struct mtk_disp_postmask *postmask = comp_to_postmask(comp);
#endif
	unsigned int width;
	unsigned int overhead_v;

	if (comp->mtk_crtc->is_dual_pipe) {
		width = cfg->w / 2;
		if (cfg->tile_overhead.is_support)
			width = postmark_tile_overhead.left_in_width;

	} else
		width = cfg->w;

	mtk_ddp_write_mask(comp, REG_FLD_VAL((CFG_FLD_SHADOW_CTRL_BYPASS_SHADOW), 1),
		DISP_POSTMASK_SHADOW_CTRL,
		REG_FLD_MASK(CFG_FLD_SHADOW_CTRL_BYPASS_SHADOW), handle);

	value = (REG_FLD_VAL((BLEND_CFG_FLD_A_EN), 1) |
		 REG_FLD_VAL((BLEND_CFG_FLD_PARGB_BLD), 0) |
		 REG_FLD_VAL((BLEND_CFG_FLD_CONST_BLD), 0));
	mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_BLEND_CFG, handle);

	mtk_ddp_write_relaxed(comp, 0xff000000, DISP_POSTMASK_ROI_BGCLR,
			      handle);
	mtk_ddp_write_relaxed(comp, 0xff000000, DISP_POSTMASK_MASK_CLR, handle);

	value = REG_FLD_VAL((FORCE_COMMIT), 1);
	if (postmask->data->need_bypass_shadow)
		value |= REG_FLD_VAL((BYPASS_SHADOW), 1);
	mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_SHADOW_CTRL, handle);

	if (postmask->set_partial_update != 1)
		value = (width << 16) + cfg->h;
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : postmask_tile_overhead_v.overhead_v;
		value = (width << 16) + (postmask->roi_height + overhead_v * 2);
	}

	mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_SIZE, handle);

	if (!panel_ext)
		DDPPR_ERR("%s:panel_ext not found\n", __func__);
	else
		DDPINFO("postmask_en[%d]\n", panel_ext->round_corner_en);

	if (panel_ext && panel_ext->round_corner_en) {
		if (postmask->set_partial_update != 1) {
			value = (REG_FLD_VAL((PAUSE_REGION_FLD_RDMA_PAUSE_START),
				     panel_ext->corner_pattern_height) |
			 REG_FLD_VAL(
				 (PAUSE_REGION_FLD_RDMA_PAUSE_END),
				 cfg->h -
					 panel_ext->corner_pattern_height_bot));
		} else {
			value = (REG_FLD_VAL((PAUSE_REGION_FLD_RDMA_PAUSE_START),
				     postmask->pu_pause_start) |
			 REG_FLD_VAL(
				 (PAUSE_REGION_FLD_RDMA_PAUSE_END),
				 postmask->pu_pause_end));
		}
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_PAUSE_REGION,
				      handle);

		value = (REG_FLD_VAL((MEM_GMC_FLD_ISSUE_REQ_TH), 63) |
			 REG_FLD_VAL((MEM_GMC_FLD_ISSUE_REQ_TH_URG), 63) |
			 REG_FLD_VAL((MEM_GMC_FLD_REQ_TH_PREULTRA), 0) |
			 REG_FLD_VAL((MEM_GMC_FLD_REQ_TH_ULTRA), 1) |
			 REG_FLD_VAL((MEM_GMC_FLD_FORCE_REQ_TH), 0));
		mtk_ddp_write_relaxed(comp, value,
				      DISP_POSTMASK_MEM_GMC_SETTING2, handle);

		value = (REG_FLD_VAL((GREQ_FLD_GREQ_NUM), 7) |
			 REG_FLD_VAL((GREQ_FLD_GREQ_URG_NUM), 7) |
			 REG_FLD_VAL((GREQ_FLD_GREQ_NUM_SHT_VAL), 1) |
			 REG_FLD_VAL((GREQ_FLD_GREQ_NUM_SHT), 0) |
			 REG_FLD_VAL((GREQ_FLD_OSTD_GREQ_NUM), 0xFF) |
			 REG_FLD_VAL((GREQ_FLD_GREQ_DIS_CNT), 1) |
			 REG_FLD_VAL((GREQ_FLD_GREQ_STOP_EN), 0) |
			 REG_FLD_VAL((GREQ_FLD_GRP_END_STOP), 1) |
			 REG_FLD_VAL((GREQ_FLD_GRP_BRK_STOP), 1) |
			 REG_FLD_VAL((GREQ_FLD_IOBUF_FLUSH_PREULTRA), 1) |
			 REG_FLD_VAL((GREQ_FLD_IOBUF_FLUSH_ULTRA), 1));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_RDMA_GREQ_NUM,
				      handle);

		value = (REG_FLD_VAL((GREQ_URG_FLD_ARB_GREQ_URG_TH), 0) |
			 REG_FLD_VAL((GREQ_URG_FLD_ARB_URG_BIAS), 0));
		mtk_ddp_write_relaxed(comp, value,
				      DISP_POSTMASK_RDMA_GREQ_URG_NUM, handle);

		value = (REG_FLD_VAL((ULTRA_FLD_PREULTRA_BUF_SRC), 0) |
			 REG_FLD_VAL((ULTRA_FLD_PREULTRA_SMI_SRC), 1) |
			 REG_FLD_VAL((ULTRA_FLD_PREULTRA_ROI_END_SRC), 0) |
			 REG_FLD_VAL((ULTRA_FLD_PREULTRA_RDMA_SRC), 0) |
			 REG_FLD_VAL((ULTRA_FLD_ULTRA_BUF_SRC), 0) |
			 REG_FLD_VAL((ULTRA_FLD_ULTRA_SMI_SRC), 1) |
			 REG_FLD_VAL((ULTRA_FLD_ULTRA_ROI_END_SRC), 0) |
			 REG_FLD_VAL((ULTRA_FLD_ULTRA_RDMA_SRC), 0));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_RDMA_ULTRA_SRC,
				      handle);

		value = (REG_FLD_VAL((TH_FLD_RDMA_ULTRA_LOW_TH), 0xFFF) |
			 REG_FLD_VAL((TH_FLD_RDMA_PREULTRA_LOW_TH), 0xFFF));
		mtk_ddp_write_relaxed(comp, value,
				      DISP_POSTMASK_RDMA_BUF_LOW_TH, handle);

		value = (REG_FLD_VAL((TH_FLD_RDMA_PREULTRA_HIGH_TH), 0xFFF) |
			 REG_FLD_VAL((TH_FLD_RDMA_PREULTRA_HIGH_DIS), 0));
		mtk_ddp_write_relaxed(comp, value,
				      DISP_POSTMASK_RDMA_BUF_HIGH_TH, handle);

#ifdef POSTMASK_DRAM_MODE
		if (comp->mtk_crtc->is_dual_pipe) {
			if (comp->id == DDP_COMPONENT_POSTMASK0 &&
			    comp->mtk_crtc->round_corner_gem_l &&
			    panel_ext->corner_pattern_tp_size_l) {
				gem = comp->mtk_crtc->round_corner_gem_l;
				addr = gem->dma_addr;
				size = panel_ext->corner_pattern_tp_size_l;
			} else if (comp->id == DDP_COMPONENT_POSTMASK1 &&
			    comp->mtk_crtc->round_corner_gem_r &&
			    panel_ext->corner_pattern_tp_size_r) {
				gem = comp->mtk_crtc->round_corner_gem_r;
				addr = gem->dma_addr;
				size = panel_ext->corner_pattern_tp_size_r;
			}
		} else if (comp->mtk_crtc->round_corner_gem &&
			   panel_ext->corner_pattern_tp_size) {
			gem = comp->mtk_crtc->round_corner_gem;
			if (postmask->set_partial_update != 1) {
				addr = gem->dma_addr;
				size = panel_ext->corner_pattern_tp_size;
			} else {
				addr = postmask->pu_mem_addr;
				size = postmask->pu_mem_length;
			}
		}
		DDPINFO("POSTMASK_DRAM_MODE\n");

		if (postmask->set_partial_update == 1)
			force_relay = postmask->pu_force_relay;
		if (addr == 0 || size == 0) {
			DDPPR_ERR("invalid postmaks addr/size\n");
			force_relay = 1;
		} else if (postmask->postmask_force_relay) {
			DDPMSG("postmask force relay\n");
			force_relay = 1;
		}

		value = (REG_FLD_VAL((CFG_FLD_RELAY_MODE), force_relay) |
			 REG_FLD_VAL((CFG_FLD_DRAM_MODE), 1) |
			 REG_FLD_VAL((CFG_FLD_BGCLR_IN_SEL), 1) |
			 REG_FLD_VAL((CFG_FLD_GCLAST_EN), 1) |
			 REG_FLD_VAL((CFG_FLD_STALL_CG_ON), 1));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_CFG, handle);

		mtk_ddp_write_relaxed(comp, addr, DISP_POSTMASK_MEM_ADDR,
				      handle);

		if (postmask->data->is_support_34bits)
			mtk_ddp_write_relaxed(comp, DO_SHIFT_RIGHT(addr, 32),
					DISP_POSTMASK_MEM_ADDR_MSB, handle);

		mtk_ddp_write_relaxed(comp, size, DISP_POSTMASK_MEM_LENGTH,
				      handle);
		value = (REG_FLD_VAL((POSTMASK_SLC), 0) |
			 REG_FLD_VAL((POSTMASK_VCSEL), 1));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_SLC, handle);

#else
		value = (REG_FLD_VAL((CFG_FLD_RELAY_MODE), 0) |
			 REG_FLD_VAL((CFG_FLD_DRAM_MODE), 0) |
			 REG_FLD_VAL((CFG_FLD_BGCLR_IN_SEL), 1) |
			 REG_FLD_VAL((CFG_FLD_GCLAST_EN), 1) |
			 REG_FLD_VAL((CFG_FLD_STALL_CG_ON), 1));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_CFG, handle);

		value = (REG_FLD_VAL((SRAM_CFG_FLD_MASK_NUM_SW_SET),
				     panel_ext->corner_pattern_height) |
			 REG_FLD_VAL((SRAM_CFG_FLD_MASK_L_TOP_EN), 1) |
			 REG_FLD_VAL((SRAM_CFG_FLD_MASK_L_BOTTOM_EN), 1) |
			 REG_FLD_VAL((SRAM_CFG_FLD_MASK_R_TOP_EN), 1) |
			 REG_FLD_VAL((SRAM_CFG_FLD_MASK_R_BOTTOM_EN), 1));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_SRAM_CFG,
				      handle);

		num = POSTMASK_MASK_MAX_NUM;
		for (i = 0; i < num; i++) {
			mtk_ddp_write_relaxed(comp, 0x1F001F00,
					      DISP_POSTMASK_NUM(i), handle);
		}

		num = POSTMASK_GRAD_MAX_NUM;
		for (i = 0; i < num; i++) {
			mtk_ddp_write_relaxed(
				comp, 0x0, DISP_POSTMASK_GRAD_VAL(i), handle);
		}
#endif
		/* config relay mode */
	} else {
		value = (REG_FLD_VAL((CFG_FLD_RELAY_MODE), 1) |
			 REG_FLD_VAL((CFG_FLD_DRAM_MODE), 1) |
			 REG_FLD_VAL((CFG_FLD_BGCLR_IN_SEL), 1) |
			 REG_FLD_VAL((CFG_FLD_GCLAST_EN), 1) |
			 REG_FLD_VAL((CFG_FLD_STALL_CG_ON), 1));
		mtk_ddp_write_relaxed(comp, value, DISP_POSTMASK_CFG, handle);
	}
}

int mtk_postmask_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	mtk_serial_dump_reg(baddr, 0x0, 4);
	mtk_serial_dump_reg(baddr, 0x20, 2);
	mtk_serial_dump_reg(baddr, 0x30, 1);
	mtk_serial_dump_reg(baddr, 0x40, 3);
	mtk_serial_dump_reg(baddr, 0x50, 4);
	mtk_serial_dump_reg(baddr, 0xA0, 4);
	mtk_serial_dump_reg(baddr, 0xB0, 4);
	mtk_serial_dump_reg(baddr, 0xC0, 3);
	mtk_serial_dump_reg(baddr, 0xD0, 3);
	mtk_serial_dump_reg(baddr, 0x100, 4);
	mtk_serial_dump_reg(baddr, 0x110, 2);
	mtk_serial_dump_reg(baddr, 0x120, 3);
	mtk_serial_dump_reg(baddr, 0x130, 2);
	mtk_serial_dump_reg(baddr, 0x140, 3);

	return 0;
}

int mtk_postmask_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	struct mtk_disp_postmask *postmask = comp_to_postmask(comp);
	dma_addr_t addr = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("en=%d,cfg=0x%x,size=(%dx%d),shadowbypass=%d\n",
		readl(DISP_POSTMASK_EN + baddr) & 0x1,
		readl(DISP_POSTMASK_CFG + baddr),
		(readl(DISP_POSTMASK_SIZE + baddr) >> 16) & 0x1fff,
		readl(DISP_POSTMASK_SIZE + baddr) & 0x1fff,
		(readl(DISP_POSTMASK_SHADOW_CTRL + baddr) & 0x2) >> 1);
	DDPDUMP("blend_cfg=0x%x,bg=0x%x,mask=0x%x\n",
		readl(DISP_POSTMASK_BLEND_CFG + baddr),
		readl(DISP_POSTMASK_ROI_BGCLR + baddr),
		readl(DISP_POSTMASK_MASK_CLR + baddr));
	DDPDUMP("fifo_cfg=%d,gmc=0x%x,threshold=(0x%x,0x%x)\n",
		readl(DISP_POSTMASK_RDMA_FIFO_CTRL + baddr),
		readl(DISP_POSTMASK_MEM_GMC_SETTING2 + baddr),
		readl(DISP_POSTMASK_RDMA_BUF_LOW_TH + baddr),
		readl(DISP_POSTMASK_RDMA_BUF_HIGH_TH + baddr));

	if (postmask->data->is_support_34bits) {
		addr = readl(DISP_POSTMASK_MEM_ADDR_MSB + baddr);
		addr = DO_SHIFT_LEFT(addr, 32);
	}

	addr += readl(DISP_POSTMASK_MEM_ADDR + baddr);

	DDPDUMP("mem_addr=0x%pa,length=0x%x\n",
		&addr, readl(DISP_POSTMASK_MEM_LENGTH + baddr));

	DDPDUMP("status=0x%x,cur_pos=0x%x\n",
		readl(DISP_POSTMASK_STATUS + baddr),
		readl(DISP_POSTMASK_INPUT_COUNT + baddr));
	DDPDUMP("slc=0x%x\n",
		readl(DISP_POSTMASK_SLC + baddr));

	return 0;
}

static int mtk_postmask_io_cmd(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle,
			       enum mtk_ddp_io_cmd io_cmd, void *params);

static void mtk_postmask_start(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle)
{
	DDPDBG("%s\n", __func__);

#ifdef IF_ZERO	/* enable only if irq can be handled */
	mtk_postmask_io_cmd(comp, handle, IRQ_LEVEL_NORMAL, NULL);
#endif

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_POSTMASK_EN, 1, ~0);
}

static void mtk_postmask_stop(struct mtk_ddp_comp *comp,
			      struct cmdq_pkt *handle)
{
	DDPDBG("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_POSTMASK_INTEN, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_POSTMASK_EN, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_POSTMASK_INTSTA, 0, ~0);
}

static void mtk_postmask_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle)
{
	struct mtk_disp_postmask *postmask = comp_to_postmask(comp);

	DDPINFO("%s, comp_id: %d, bypass: %d\n",
			__func__, comp->id, bypass);

	/* postmask bypass control by round_corner_en and bebug flag */
	//if (postmask->postmask_debug)
		//return;

	postmask->postmask_force_relay = bypass;

	/* config relay mode */
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_POSTMASK_CFG, bypass, 0x1);
}

void mtk_postmask_relay_debug(struct mtk_ddp_comp *comp, unsigned int relay)
{
	struct mtk_disp_postmask *postmask = comp_to_postmask(comp);

	if (!comp) {
		DDPPR_ERR("postmask comp is NULL\n");
		return;
	}

	postmask->postmask_force_relay = relay;
	postmask->postmask_debug = 1;
}

static int mtk_disp_postmask_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_postmask *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;
	char buf[50];

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hrt_qos");
		priv->ddp_comp.hrt_qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hrt_qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));
	}

	return 0;
}

static void mtk_disp_postmask_unbind(struct device *dev, struct device *master,
				     void *data)
{
	struct mtk_disp_postmask *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static void mtk_postmask_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_postmask_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static int mtk_postmask_io_cmd(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle,
			       enum mtk_ddp_io_cmd io_cmd, void *params)
{
	struct mtk_drm_private *priv;

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPINFO("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), io_cmd);
		return -INVALID;
	}

	priv = comp->mtk_crtc->base.dev->dev_private;
	switch (io_cmd) {
#ifdef IF_ZERO	/* enable only if irq can be handled */
	case IRQ_LEVEL_NORMAL: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_PM_IF_FME_END_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_PM_FME_CPL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_PM_START_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_PM_ABNORMAL_SOF_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA_FME_UND_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA_EOF_ABNORMAL_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_POSTMASK_INTEN, inten,
			       inten);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_PM_IF_FME_END_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_PM_FME_CPL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_PM_START_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_POSTMASK_INTEN, 0, inten);
		break;
	}
#endif
	case PMQOS_GET_LARB_PORT_HRT_BW: {
		struct mtk_larb_port_bw *data = (struct mtk_larb_port_bw *)params;
		unsigned int bpp = 1;
		unsigned int tmp;

		tmp = mtk_drm_primary_frame_bw(&comp->mtk_crtc->base);

		data->larb_id = -1;
		data->bw = 0;
		if (data->type != CHANNEL_HRT_RW)
			break;

		if (comp->larb_num == 1)
			data->larb_id = comp->larb_id;
		else if (comp->larb_num > 1)
			data->larb_id = comp->larb_ids[0];

		if (data->larb_id < 0)
			break;

		tmp = (tmp * bpp) >> 2;
		if (debug_postmask_bw)
			tmp = debug_postmask_bw;

		data->bw = tmp;
		break;
	}
	case PMQOS_SET_HRT_BW:
	case PMQOS_SET_HRT_BW_DELAY:
	{
		u32 bw_val = *(unsigned int *)params;
		unsigned int bpp = 1;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl)
			break;

		if (IS_ERR(comp->hdr_qos_req))
			break;

		bw_val = (bw_val * bpp) >> 2;
		if (debug_postmask_bw)
			bw_val = debug_postmask_bw;

		if (comp->last_hrt_bw == bw_val)
			break;
		comp->last_hrt_bw = bw_val;
		if (bw_val > 0)
			DDPDBG("%s,PMQOS_SET_HRT_BW, postmask comp:%d,bw:%d\n",	__func__, comp->id, bw_val);

		__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, bw_val,
				priv->data->respective_ostdl);
		break;
	}
	default:
		break;
	}

	return 0;
}

static unsigned int sum_corner_pattern_per_line
		(unsigned int line_num_start, unsigned int line_num_end, unsigned int arr[])
{
	unsigned int sum = 0, i = 0;

	if (line_num_start > line_num_end)
		return 0;

	for (i = line_num_start; i <= line_num_end; i++)
		sum = sum + arr[i];

	return sum;
}

static int mtk_postmask_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_postmask *postmask = comp_to_postmask(comp);
	struct mtk_panel_params *panel_ext =
		mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
	struct mtk_drm_gem_obj *gem;
	unsigned int size = 0;
	unsigned int size_per_line_top = 0, size_per_line_bot = 0;
	unsigned int size_per_line_top_b = 0, size_per_line_bot_t = 0;
	unsigned int tmp_top = 0, tmp_bot = 0, tmp_top_b = 0, tmp_bot_t = 0;
	dma_addr_t addr = 0;
	unsigned int force_relay = 0;
	unsigned int pause_start = 0, pause_end = 0;
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPINFO("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	if (!panel_ext || !panel_ext->round_corner_en) {
		DDPDBG("%s:panel_ext not found or round_corner not enable\n", __func__);
		return 0;
	}

	if (!panel_ext->corner_pattern_size_per_line) {
		DDPINFO("%s, size_per_line table is null\n", __func__);
		return 0;
	}

	postmask->set_partial_update = enable;
	postmask->roi_height = partial_roi.height;
	postmask->roi_y_offset = partial_roi.y;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : postmask_tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (comp->mtk_crtc->round_corner_gem &&
			panel_ext->corner_pattern_tp_size &&
			panel_ext->corner_pattern_size_per_line) {
		gem = comp->mtk_crtc->round_corner_gem;
		addr = gem->dma_addr;
		size = panel_ext->corner_pattern_tp_size;
	}

	DDPDBG("%s, ori addr = 0x%pa, ori size = %d\n", __func__, &addr, size);

	if (postmask->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_SIZE,
			postmask->roi_height + overhead_v * 2, 0x1fff);

		if ((postmask->roi_y_offset - overhead_v) >= panel_ext->corner_pattern_height
			&& (postmask->roi_y_offset + postmask->roi_height + overhead_v)
			< full_height - panel_ext->corner_pattern_height_bot) {
			/*1. No overlapping cases*/
			DDPINFO("%s, 1. No overlapping cases\n",__func__);
			DDPDBG("%s, force_relay = 1\n",__func__);
			force_relay = 1;
		} else if ((postmask->roi_y_offset - overhead_v) < panel_ext->corner_pattern_height
			&& (postmask->roi_y_offset + postmask->roi_height + overhead_v)
			< full_height - panel_ext->corner_pattern_height_bot) {
			/*2. partial roi overlap with top corner and not overlap with bot corner*/
			tmp_top = postmask->roi_y_offset - overhead_v - 1;
			size_per_line_top = sum_corner_pattern_per_line(0, tmp_top,
							panel_ext->corner_pattern_size_per_line);
			DDPINFO("%s, 2. overlap with top and not overlap with bot\n",__func__);
			DDPDBG("%s, size_per_line_top: %d, num_start: %d, num_end: %d\n",
				__func__, size_per_line_top, 0, tmp_top);

			/*2-1. partial roi inside top corner*/
			if (postmask->roi_y_offset + postmask->roi_height + overhead_v
				<= panel_ext->corner_pattern_height) {
				tmp_top_b = (postmask->roi_y_offset + postmask->roi_height
							+ overhead_v);
				size_per_line_top_b = sum_corner_pattern_per_line(
						tmp_top_b, panel_ext->corner_pattern_height - 1,
						panel_ext->corner_pattern_size_per_line);
				DDPDBG("%s, size_per_line_top_b: %d, num_start: %d, num_end: %d\n",
				__func__, size_per_line_top_b,
				tmp_top_b, panel_ext->corner_pattern_height - 1);
			}

			tmp_bot = panel_ext->corner_pattern_height
						+ panel_ext->corner_pattern_height_bot - 1;
			size_per_line_bot = sum_corner_pattern_per_line(
							panel_ext->corner_pattern_height, tmp_bot,
							panel_ext->corner_pattern_size_per_line);
			DDPDBG("%s, size_per_line_bot: %d, num_start: %d, num_end: %d\n",
				__func__, size_per_line_bot,
				panel_ext->corner_pattern_height, tmp_bot);

			force_relay = 0;
			addr = addr + size_per_line_top;
			size = size - size_per_line_top - size_per_line_top_b - size_per_line_bot;
			pause_start = panel_ext->corner_pattern_height - (tmp_top + 1);
			pause_end = postmask->roi_height + overhead_v * 2;
			DDPDBG("%s, tmp_top: %d, tmp_top_b: %d, tmp_bot: %d\n",
				__func__, tmp_top, tmp_top_b, tmp_bot);
			DDPDBG("%s, addr: 0x%pa, size: %d, pause start: %d, pause end: %d\n",
				__func__, &addr, size, pause_start, pause_end);
		} else if ((postmask->roi_y_offset - overhead_v) >= panel_ext->corner_pattern_height
			&& (postmask->roi_y_offset + postmask->roi_height + overhead_v)
			>= full_height - panel_ext->corner_pattern_height_bot) {
			/*3. partial roi not overlap with top corner and overlap with bot corner*/
			tmp_top = panel_ext->corner_pattern_height - 1;
			size_per_line_top = sum_corner_pattern_per_line(0, tmp_top,
							panel_ext->corner_pattern_size_per_line);
			DDPINFO("%s, 3. not overlap with top and overlap with bot\n",__func__);
			DDPDBG("%s, size_per_line_top: %d, num_start: %d, num_end: %d\n",
				__func__, size_per_line_top, 0, tmp_top);

			/*3-1. partial roi inside bot corner*/
			if ((postmask->roi_y_offset - overhead_v) > full_height
			 - panel_ext->corner_pattern_height_bot) {
				tmp_bot_t = (postmask->roi_y_offset - overhead_v) -
					(full_height - panel_ext->corner_pattern_height_bot) - 1;
				size_per_line_bot_t = sum_corner_pattern_per_line(
						(panel_ext->corner_pattern_height),
						(panel_ext->corner_pattern_height + tmp_bot_t),
							panel_ext->corner_pattern_size_per_line);
				DDPDBG("%s, size_per_line_bot_t: %d, num_start: %d, num_end: %d\n",
				__func__, size_per_line_bot_t,
				(panel_ext->corner_pattern_height)
				, (panel_ext->corner_pattern_height + tmp_bot_t));
			}

			tmp_bot = full_height - (postmask->roi_y_offset
						+ postmask->roi_height + overhead_v);
			size_per_line_bot = sum_corner_pattern_per_line(
			(panel_ext->corner_pattern_height
				+ panel_ext->corner_pattern_height_bot - tmp_bot),
			(panel_ext->corner_pattern_height
				+ panel_ext->corner_pattern_height_bot - 1),
			panel_ext->corner_pattern_size_per_line);
			DDPDBG("%s, size_per_line_bot: %d, num_start: %d, num_end: %d\n",
			__func__, size_per_line_bot,
			(panel_ext->corner_pattern_height
				+ panel_ext->corner_pattern_height_bot - tmp_bot),
			(panel_ext->corner_pattern_height
				+ panel_ext->corner_pattern_height_bot - 1));

			force_relay = 0;
			addr = addr + size_per_line_top + size_per_line_bot_t;
			size = size - size_per_line_top - size_per_line_bot_t - size_per_line_bot;
			pause_start = 0;
			pause_end = tmp_bot_t ? 0 : postmask->roi_height + overhead_v * 2 -
						(panel_ext->corner_pattern_height_bot - tmp_bot);
			DDPDBG("%s, tmp_top: %d, tmp_bot_t: %d, tmp_bot: %d\n",
					__func__, tmp_top, tmp_bot_t, tmp_bot);
			DDPDBG("%s, addr: 0x%pa, size: %d, pause start: %d, pause end: %d\n",
				__func__, &addr, size, pause_start, pause_end);
		} else if ((postmask->roi_y_offset - overhead_v)
			< panel_ext->corner_pattern_height
			&& (postmask->roi_y_offset + postmask->roi_height + overhead_v)
			>= full_height - panel_ext->corner_pattern_height_bot) {
			/*4. partial roi overlap with top corner and overlap with bot corner*/
			tmp_top = postmask->roi_y_offset - overhead_v - 1;
			size_per_line_top = sum_corner_pattern_per_line(0, tmp_top,
							panel_ext->corner_pattern_size_per_line);
			DDPINFO("%s, 4. overlap with top and overlap with bot\n",__func__);
			DDPDBG("%s, size_per_line_top: %d, num_start: %d, num_end: %d\n",
				__func__, size_per_line_top, 0, tmp_top);

			tmp_bot = full_height - (postmask->roi_y_offset
						+ postmask->roi_height + overhead_v);
			size_per_line_bot = sum_corner_pattern_per_line(
				(panel_ext->corner_pattern_height
					+ panel_ext->corner_pattern_height_bot - tmp_bot),
				(panel_ext->corner_pattern_height
					+ panel_ext->corner_pattern_height_bot - 1),
				panel_ext->corner_pattern_size_per_line);
			DDPDBG("%s, size_per_line_bot: %d, num_start: %d, num_end: %d\n",
			__func__, size_per_line_bot,
			(panel_ext->corner_pattern_height
				+ panel_ext->corner_pattern_height_bot - tmp_bot),
			(panel_ext->corner_pattern_height
				+ panel_ext->corner_pattern_height_bot - 1));

			force_relay = 0;
			addr = addr + size_per_line_top;
			size = size - size_per_line_top - size_per_line_bot;
			pause_start = panel_ext->corner_pattern_height - (tmp_top + 1);
			pause_end = postmask->roi_height + overhead_v * 2 -
						(panel_ext->corner_pattern_height_bot - tmp_bot);
			DDPDBG("%s, tmp_top: %d, tmp_bot: %d\n", __func__, tmp_top, tmp_bot);
			DDPDBG("%s, addr: 0x%pa, size: %d, pause start: %d, pause end: %d\n",
				__func__, &addr, size, pause_start, pause_end);
		}

		if (addr == 0 || size == 0) {
			DDPPR_ERR("%s, invalid postmaks addr/size\n", __func__);
			force_relay = 1;
		} else if (postmask->postmask_force_relay) {
			DDPDBG("%s, postmask force relay\n", __func__);
			force_relay = 1;
		}

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_SIZE,
			postmask->roi_height + overhead_v * 2, 0x1fff);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_CFG,
			REG_FLD_VAL((CFG_FLD_RELAY_MODE), force_relay),
			REG_FLD_MASK(CFG_FLD_RELAY_MODE));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_MEM_ADDR,
			addr, ~0);

		if (postmask->data->is_support_34bits)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_POSTMASK_MEM_ADDR_MSB,
				(addr >> 32), ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_POSTMASK_MEM_LENGTH, size, ~0);

		if (panel_ext && panel_ext->round_corner_en) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_POSTMASK_PAUSE_REGION,
				REG_FLD_VAL((PAUSE_REGION_FLD_RDMA_PAUSE_START),
				pause_start),
				REG_FLD_MASK(PAUSE_REGION_FLD_RDMA_PAUSE_START));

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_POSTMASK_PAUSE_REGION,
				REG_FLD_VAL((PAUSE_REGION_FLD_RDMA_PAUSE_END),
				pause_end),
				REG_FLD_MASK(PAUSE_REGION_FLD_RDMA_PAUSE_END));
		}

		postmask->pu_force_relay = force_relay;
		postmask->pu_mem_addr = addr;
		postmask->pu_mem_length = size;
		postmask->pu_pause_start = pause_start;
		postmask->pu_pause_end = pause_end;
	} else {
		if (addr == 0 || size == 0) {
			DDPPR_ERR("%s, invalid postmaks addr/size: %d\n", __func__, size);
			force_relay = 1;
		} else if (postmask->postmask_force_relay) {
			DDPINFO("%s, postmask force relay\n", __func__);
			force_relay = 1;
		}

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_SIZE,
			full_height, 0x1fff);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_CFG,
			REG_FLD_VAL((CFG_FLD_RELAY_MODE), force_relay),
			REG_FLD_MASK(CFG_FLD_RELAY_MODE));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_POSTMASK_MEM_ADDR,
			addr, ~0);

		if (postmask->data->is_support_34bits)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_POSTMASK_MEM_ADDR_MSB,
				(addr >> 32), ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_POSTMASK_MEM_LENGTH, size, ~0);

		if (panel_ext && panel_ext->round_corner_en) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_POSTMASK_PAUSE_REGION,
				REG_FLD_VAL((PAUSE_REGION_FLD_RDMA_PAUSE_START),
				panel_ext->corner_pattern_height),
				REG_FLD_MASK(PAUSE_REGION_FLD_RDMA_PAUSE_START));

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_POSTMASK_PAUSE_REGION,
				REG_FLD_VAL((PAUSE_REGION_FLD_RDMA_PAUSE_END),
				full_height - panel_ext->corner_pattern_height_bot),
				REG_FLD_MASK(PAUSE_REGION_FLD_RDMA_PAUSE_END));
		}

		postmask->pu_force_relay = force_relay;
	}

	return 0;
}


static const struct mtk_ddp_comp_funcs mtk_disp_postmask_funcs = {
	.first_cfg = mtk_postmask_config,
	.config = mtk_postmask_config,
	.start = mtk_postmask_start,
	.stop = mtk_postmask_stop,
	.bypass = mtk_postmask_bypass,
	.prepare = mtk_postmask_prepare,
	.unprepare = mtk_postmask_unprepare,
	.io_cmd = mtk_postmask_io_cmd,
	.config_overhead = mtk_disp_postmark_config_overhead,
	.config_overhead_v = mtk_disp_postmask_config_overhead_v,
	.partial_update = mtk_postmask_set_partial_update,
};

static const struct component_ops mtk_disp_postmask_component_ops = {
	.bind = mtk_disp_postmask_bind, .unbind = mtk_disp_postmask_unbind,
};

static int mtk_disp_postmask_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_postmask *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_POSTMASK);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_postmask_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_postmask_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_postmask_remove(struct platform_device *pdev)
{
	struct mtk_disp_postmask *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_postmask_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_postmask_data mt6779_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6885_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6983_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6985_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6989_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6899_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6897_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6895_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6886_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6873_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6853_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6833_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6877_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6781_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6879_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6855_postmask_driver_data = {
	.is_support_34bits = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_postmask_data mt6991_postmask_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_postmask_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-postmask",
	  .data = &mt6779_postmask_driver_data},
	{ .compatible = "mediatek,mt6885-disp-postmask",
	  .data = &mt6885_postmask_driver_data},
	{ .compatible = "mediatek,mt6983-disp-postmask",
	  .data = &mt6983_postmask_driver_data},
	{ .compatible = "mediatek,mt6895-disp-postmask",
	  .data = &mt6895_postmask_driver_data},
	{ .compatible = "mediatek,mt6886-disp-postmask",
	  .data = &mt6886_postmask_driver_data},
	{ .compatible = "mediatek,mt6873-disp-postmask",
	  .data = &mt6873_postmask_driver_data},
	{ .compatible = "mediatek,mt6853-disp-postmask",
	  .data = &mt6853_postmask_driver_data},
	{ .compatible = "mediatek,mt6833-disp-postmask",
	  .data = &mt6833_postmask_driver_data},
	{ .compatible = "mediatek,mt6877-disp-postmask",
	  .data = &mt6877_postmask_driver_data},
	{ .compatible = "mediatek,mt6781-disp-postmask",
	  .data = &mt6781_postmask_driver_data},
	{ .compatible = "mediatek,mt6879-disp-postmask",
	  .data = &mt6879_postmask_driver_data},
	{ .compatible = "mediatek,mt6855-disp-postmask",
	  .data = &mt6855_postmask_driver_data},
	{ .compatible = "mediatek,mt6985-disp-postmask",
	  .data = &mt6985_postmask_driver_data},
	{ .compatible = "mediatek,mt6989-disp-postmask",
	  .data = &mt6989_postmask_driver_data},
	{ .compatible = "mediatek,mt6899-disp-postmask",
	  .data = &mt6899_postmask_driver_data},
	{ .compatible = "mediatek,mt6897-disp-postmask",
	  .data = &mt6897_postmask_driver_data},
	{ .compatible = "mediatek,mt6835-disp-postmask",
	  .data = &mt6835_postmask_driver_data},
	{ .compatible = "mediatek,mt6991-disp-postmask",
	  .data = &mt6991_postmask_driver_data},
	{},
};

struct platform_driver mtk_disp_postmask_driver = {
	.probe = mtk_disp_postmask_probe,
	.remove = mtk_disp_postmask_remove,
	.driver = {

			.name = "mediatek-disp-postmask",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_postmask_driver_dt_match,
		},
};
