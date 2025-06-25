// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/delay.h>

#include <video/videomode.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_simple_kms_helper.h>

#include "mtk_dvo_regs.h"
#include "../mtk_drm_drv.h"
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_disp_pmqos.h"
#include "../mtk_dump.h"

#define DVO_COLOR_BAR					0

/* DVO INPUT default value is 1T2P */
#define MTK_DVO_INPUT_MODE				2
/* DVO OUTPUT value is 1T4P*/
#define MTK_DVO_OUTPUT_MODE				4
/* 1 unit = 2 group data = 2 * 1T4P = 8P */
#define MTK_DISP_BUF_SRAM_UNIT_SIZE		8
#define MTK_DISP_LINE_BUF_DVO_US		40

#define MTK_BLANKING_RATIO				(5 / 4)

#define MTK_DVO_EDP_MAX_CLK				297
#define MTK_DVO_MAX_HACTIVE				3840

#define TWAIT_SLEEP						(6 / 5)
#define TWAKE_UP						5
#define PREULTRA_HIGH_US				26
#define PREULTRA_LOW_US					25
#define ULTRA_HIGH_US					25
#define ULTRA_LOW_US					23
#define URGENT_HIGH_US					12
#define URGENT_LOW_US					11

enum mtk_dvo_out_bit_num {
	MTK_DVO_OUT_BIT_NUM_8BITS,
	MTK_DVO_OUT_BIT_NUM_10BITS,
	MTK_DVO_OUT_BIT_NUM_12BITS,
	MTK_DVO_OUT_BIT_NUM_16BITS
};

enum mtk_dvo_out_yc_map {
	MTK_DVO_OUT_YC_MAP_RGB,
	MTK_DVO_OUT_YC_MAP_CYCY,
	MTK_DVO_OUT_YC_MAP_YCYC,
	MTK_DVO_OUT_YC_MAP_CY,
	MTK_DVO_OUT_YC_MAP_YC
};

enum mtk_dvo_out_channel_swap {
	MTK_DVO_OUT_CHANNEL_SWAP_RGB,
	MTK_DVO_OUT_CHANNEL_SWAP_GBR,
	MTK_DVO_OUT_CHANNEL_SWAP_BRG,
	MTK_DVO_OUT_CHANNEL_SWAP_RBG,
	MTK_DVO_OUT_CHANNEL_SWAP_GRB,
	MTK_DVO_OUT_CHANNEL_SWAP_BGR
};

enum mtk_dvo_out_color_format {
	MTK_DVO_COLOR_FORMAT_RGB,
	MTK_DVO_COLOR_FORMAT_YCBCR_422
};

enum TVDPLL_CLK {
	TVDPLL_PLL = 0,
	TVDPLL_D2 = 2,
	TVDPLL_D4 = 4,
	TVDPLL_D8 = 8,
	TVDPLL_D16 = 16,
};

struct mtk_dvo {
	struct mtk_ddp_comp ddp_comp;
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_connector *connector;
	void __iomem *regs;
	struct device *dev;
	struct device *mmsys_dev;
	struct clk *engine_clk;
	struct clk *pixel_clk;
	struct clk *tvd_clk;
	struct clk *dvo_clk;
	struct clk *hf_fdvo_clk;
	struct clk *pclk_src[5];
	int irq;
	struct drm_display_mode mode;
	const struct mtk_dvo_conf *conf;
	unsigned int color_depth;
	enum mtk_dvo_out_color_format color_format;
	enum mtk_dvo_out_yc_map yc_map;
	enum mtk_dvo_out_bit_num bit_num;
	enum mtk_dvo_out_channel_swap channel_swap;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_gpio;
	struct pinctrl_state *pins_dvo;
	u32 output_fmt;
	int refcount;
};

static inline struct mtk_dvo *bridge_to_dvo(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dvo, bridge);
}

enum mtk_dvo_polarity {
	MTK_DVO_POLARITY_RISING,
	MTK_DVO_POLARITY_FALLING,
};

struct mtk_dvo_polarities {
	enum mtk_dvo_polarity de_pol;
	enum mtk_dvo_polarity ck_pol;
	enum mtk_dvo_polarity hsync_pol;
	enum mtk_dvo_polarity vsync_pol;
};

struct mtk_dvo_sync_param {
	u32 sync_width;
	u32 front_porch;
	u32 back_porch;
	bool shift_half_line;
};

struct mtk_dvo_yc_limit {
	u16 y_top;
	u16 y_bottom;
	u16 c_top;
	u16 c_bottom;
};

/**
 * struct mtk_dvo_conf - Configuration of mediatek dvo.
 * @cal_factor: Callback function to calculate factor value.
 * @reg_h_fre_con: Register address of frequency control.
 * @max_clock_khz: Max clock frequency supported for this SoCs in khz units.
 * @edge_sel_en: Enable of edge selection.
 * @output_fmts: Array of supported output formats.
 * @num_output_fmts: Quantity of supported output formats.
 * @is_ck_de_pol: Support CK/DE polarity.
 * @swap_input_support: Support input swap function.
 * @support_direct_pin: IP supports direct connection to dvo panels.
 * @input_2pixel: Input pixel of dp_intf is 2 pixel per round, so enable this
 *		  config to enable this feature.
 * @dimension_mask: Mask used for HWIDTH, HPORCH, VSYNC_WIDTH and VSYNC_PORCH
 *		    (no shift).
 * @hvsize_mask: Mask of HSIZE and VSIZE mask (no shift).
 * @channel_swap_shift: Shift value of channel swap.
 * @yuv422_en_bit: Enable bit of yuv422.
 * @csc_enable_bit: Enable bit of CSC.
 * @pixels_per_iter: Quantity of transferred pixels per iteration.
 * @edge_cfg_in_mmsys: If the edge configuration for DVO's output needs to be set in MMSYS.
 */
struct mtk_dvo_conf {
	unsigned int (*cal_factor)(int clock);
	u32 reg_h_fre_con;
	u32 max_clock_khz;
	bool edge_sel_en;
	const u32 *output_fmts;
	u32 num_output_fmts;
	bool is_ck_de_pol;
	bool swap_input_support;
	bool support_direct_pin;
	bool input_2pixel;
	u32 out_np_sel;
	u32 dimension_mask;
	u32 hvsize_mask;
	u32 channel_swap_shift;
	u32 yuv422_en_bit;
	u32 csc_enable_bit;
	u32 pixels_per_iter;
	bool edge_cfg_in_mmsys;
	bool has_commit;
};

static struct mtk_dvo *g_mtk_dvo;

static int irq_intsa;
static int irq_vdesa;
static int irq_underflowsa;
static int irq_tl;

static void mtk_dvo_mask(struct mtk_dvo *dvo, u32 offset, u32 val, u32 mask)
{
	u32 tmp = readl(dvo->regs + offset) & ~mask;

	tmp |= (val & mask);
	writel(tmp, dvo->regs + offset);
}

static void mtk_dvo_sw_reset(struct mtk_dvo *dvo, bool reset)
{
	mtk_dvo_mask(dvo, DVO_RET, reset ? SWRST : 0, SWRST);
}

static void mtk_dvo_enable(struct mtk_dvo *dvo, bool enable)
{
	mtk_dvo_mask(dvo, DVO_EN, enable? EN : 0, EN);
}

static void mtk_dvo_irq_enable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_INTEN, INT_VDE_END_EN, INT_VDE_END_EN);
}

static void mtk_dvo_config_pol(struct mtk_dvo *dvo,
				struct mtk_dvo_polarities *dvo_pol)
{
	unsigned int pol;
	unsigned int mask;

	mask = HSYNC_POL | VSYNC_POL;
	pol = (dvo_pol->hsync_pol == MTK_DVO_POLARITY_RISING ? 0 : HSYNC_POL) |
			(dvo_pol->vsync_pol == MTK_DVO_POLARITY_RISING ? 0 : VSYNC_POL);

	if (dvo->conf->is_ck_de_pol) {
		mask |= CK_POL | DE_POL;
		pol |= (dvo_pol->ck_pol == MTK_DVO_POLARITY_RISING ? 0 : CK_POL) |
				(dvo_pol->de_pol == MTK_DVO_POLARITY_RISING ? 0 : DE_POL);
	}

	/* use default value for remain same */
	mtk_dvo_mask(dvo, DVO_OUTPUT_SET, 0, mask);
}

static void mtk_dvo_info_queue_start(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_TGEN_INFOQ_LATENCY, 0, INFOQ_START_LATENCY_MASK | INFOQ_END_LATENCY_MASK);
}

static void mtk_dvo_buffer_ctrl(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_BUF_CON0, DISP_BUF_EN, DISP_BUF_EN);
	mtk_dvo_mask(dvo, DVO_BUF_CON0, FIFO_UNDERFLOW_DONE_BLOCK, FIFO_UNDERFLOW_DONE_BLOCK);
}

static void mtk_dvo_pm_ctl(struct mtk_dvo *dvo, bool enable)
{
	/* DISP_EDPTX_PWR_CON */
	void *address = ioremap(0x31B50074, 0x1);

	if (enable)
		writel(0xC2FC224D, address);
	else
		writel(0xC2FC2372, address);

	if (address)
		iounmap(address);
}

static void mtk_dvo_trailing_blank_setting(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_TGEN_V_LAST_TRAILING_BLANK, 0x20, V_LAST_TRAILING_BLANK_MASK);
	mtk_dvo_mask(dvo, DVO_TGEN_OUTPUT_DELAY_LINE, 0x20, EXT_TG_DLY_LINE_MASK);
}

static void mtk_dvo_config_channel_swap(struct mtk_dvo *dvo,
							enum mtk_dvo_out_channel_swap swap)
{
	u32 val;

	switch (swap) {
	case MTK_DVO_OUT_CHANNEL_SWAP_RGB:
		val = SWAP_RGB;
		break;
	case MTK_DVO_OUT_CHANNEL_SWAP_GBR:
		val = SWAP_GBR;
		break;
	case MTK_DVO_OUT_CHANNEL_SWAP_BRG:
		val = SWAP_BRG;
		break;
	case MTK_DVO_OUT_CHANNEL_SWAP_RBG:
		val = SWAP_RBG;
		break;
	case MTK_DVO_OUT_CHANNEL_SWAP_GRB:
		val = SWAP_GRB;
		break;
	case MTK_DVO_OUT_CHANNEL_SWAP_BGR:
		val = SWAP_BGR;
		break;
	default:
		val = SWAP_RGB;
		break;
	}

	mtk_dvo_mask(dvo, DVO_OUTPUT_SET, val << CH_SWAP_SHIFT, CH_SWAP_MASK);
}

static void mtk_dvo_config_yuv422_enable(struct mtk_dvo *dvo, bool enable)
{
	mtk_dvo_mask(dvo, DVO_YUV422_SET, enable ? dvo->conf->yuv422_en_bit : 0,
			dvo->conf->yuv422_en_bit);
}

static void mtk_dvo_config_csc_enable(struct mtk_dvo *dvo, bool enable)
{
	mtk_dvo_mask(dvo, DVO_MATRIX_SET, enable ? dvo->conf->csc_enable_bit : 0,
			dvo->conf->csc_enable_bit);
}

static void mtk_dvo_config_swap_input(struct mtk_dvo *dvo, bool enable)
{
	mtk_dvo_mask(dvo, DVO_CON, enable ? 0 : 0,
			BIT(1));
}

static void mtk_dvo_config_color_format(struct mtk_dvo *dvo,
							enum mtk_dvo_out_color_format format)
{
	pr_info("[eDPTX] %s+\n", __func__);
	mtk_dvo_config_channel_swap(dvo, MTK_DVO_OUT_CHANNEL_SWAP_RGB);

	if (format == MTK_DVO_COLOR_FORMAT_YCBCR_422) {
		mtk_dvo_config_csc_enable(dvo, true);
		mtk_dvo_config_yuv422_enable(dvo, true);
		pr_info("[eDPTX] support MTK_DVO_COLOR_FORMAT_YCBCR_422\n");
	} else {
		mtk_dvo_config_csc_enable(dvo, false);
		mtk_dvo_config_yuv422_enable(dvo, false);
		if (dvo->conf->swap_input_support)
			mtk_dvo_config_swap_input(dvo, false);
	}
	pr_info("[eDPTX] %s-\n", __func__);
}

static void mtk_dvo_config_color_depth(struct mtk_dvo *dvo)
{
	pr_info("[eDPTX] %s+\n", __func__);
	switch (dvo->color_depth) {
	case 8:
		mtk_dvo_mask(dvo, DVO_MATRIX_SET,
			MATRIX_SEL_RGB_TO_BT709, INT_MTX_SEL_MASK);
		break;
	case 10:
		mtk_dvo_mask(dvo, DVO_MATRIX_SET,
			MATRIX_SEL_RGB_TO_BT601, INT_MTX_SEL_MASK);
		break;
	default:
		mtk_dvo_mask(dvo, DVO_MATRIX_SET,
			MATRIX_SEL_RGB_TO_BT709, INT_MTX_SEL_MASK);
		break;
	}
	pr_info("[eDPTX] color depth:%u\n", dvo->color_depth);
	pr_info("[eDPTX] %s-\n", __func__);
}

static void mtk_dvo_sodi_setting(struct mtk_dvo *dvo, struct drm_display_mode *mode)
{
	u32 mmsys_clk = 273;
	u64 data_rate = 0;
	u64 fill_rate_rem = 0, consume_rate_rem = 0;
	u64 fill_rate = 0, consume_rate = 0;
	u64 dvo_fifo_size = 0, fifo_size = 0, total_bit = 0;
	u64 sodi_high_rem = 0, sodi_low_rem = 0, tmp = 0;
	u64 sodi_high = 0, sodi_low = 0;

	mtk_drm_set_mmclk_by_pixclk(&dvo->ddp_comp.mtk_crtc->base,
						mode->clock, __func__);
	mmsys_clk = mtk_drm_get_mmclk(&dvo->ddp_comp.mtk_crtc->base, __func__) / 1000000;
	if (!mmsys_clk) {
		pr_info("[eDPTX] mmclk is zero, use default value\n");
		mmsys_clk = 273;
	}
	dev_info(dvo->dev, "[eDPTX] get mmclk from display %d\n", mmsys_clk);

	fill_rate = ((u64)mmsys_clk * MTK_DVO_INPUT_MODE * 30) / (32 * MTK_DISP_BUF_SRAM_UNIT_SIZE);
	fill_rate = div64_u64_rem(mmsys_clk * MTK_DVO_INPUT_MODE * 30,
				(u64)32 * MTK_DISP_BUF_SRAM_UNIT_SIZE, &fill_rate_rem);

	data_rate = (u64)mode->hdisplay * mode->vdisplay *
				((u64)mode->clock * 1000 / ((u64)mode->htotal * mode->vtotal));
	/* consume_rate = data_rate *  MTK_BLANKING_RATIO * 30 /(8 * 32 * 1000000) */
	consume_rate = div64_u64_rem(((data_rate * 30 * 5) / 4),
					(u64)(8 * 32 * 1000000), &consume_rate_rem);
	dev_info(dvo->dev, "[eDPTX] data_rate:%llu consume_rate_rem: %llu", data_rate, consume_rate_rem);
	total_bit = (u64)MTK_DVO_EDP_MAX_CLK * MTK_DVO_OUTPUT_MODE * 30 * MTK_DISP_LINE_BUF_DVO_US;

	fifo_size = total_bit / (MTK_DISP_BUF_SRAM_UNIT_SIZE * 30);

	/* 3 is dvo supports MSO mode, it adds three more line buffers */
	dvo_fifo_size = fifo_size + (( MTK_DVO_MAX_HACTIVE / MTK_DISP_BUF_SRAM_UNIT_SIZE) * 3 );

	/* 1 is to round up */
	sodi_high_rem = (u64)fill_rate_rem * (8 * 32 * 1000000);
	tmp = (u64)consume_rate_rem * (32 * MTK_DISP_BUF_SRAM_UNIT_SIZE);

	if (sodi_high_rem < tmp) {
		u64 total = (u64)8 * 32 * 1000000 * 32 * MTK_DISP_BUF_SRAM_UNIT_SIZE;
		fill_rate -= 1;
		sodi_high_rem = (total + sodi_high_rem - tmp) * 32 * 6 / total;
	} else {
		u64 total = (u64)8 * 32 * 1000000 * 32 * MTK_DISP_BUF_SRAM_UNIT_SIZE;
		sodi_high_rem = (sodi_high_rem - tmp) * 32 * 6 / total;
	}

	/* sodi_high = ((dvo_fifo_size * 30) / 32 ) - (fill_rate - consume_rate) * TWAIT_SLEEP  + 1; */
	sodi_high = ((dvo_fifo_size * 30 * 5) - (32 * 6 * (fill_rate - consume_rate)) -
				sodi_high_rem + 5 * 32 - 1) / (5 * 32);
	sodi_low_rem = (consume_rate_rem * (ULTRA_LOW_US + TWAKE_UP) + (u64)(8 * 32 * 1000000) - 1)
					/ (u64)(8 * 32 * 1000000);
	sodi_low = consume_rate * (ULTRA_LOW_US + TWAKE_UP) + sodi_low_rem;

	dev_info(dvo->dev, "[eDPTX] SODI high:%llu SOHI low: %llu", sodi_high, sodi_low);

	mtk_dvo_mask(dvo, DVO_BUF_SODI_HIGHT, sodi_high, DVO_DISP_BUF_MASK);
	mtk_dvo_mask(dvo, DVO_BUF_SODI_LOW, sodi_low, DVO_DISP_BUF_MASK);
}

static void mtk_dvo_shadow_ctrl(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_SHADOW_CTRL, 0, BYPASS_SHADOW);
	mtk_dvo_mask(dvo, DVO_SHADOW_CTRL, FORCE_COMMIT, FORCE_COMMIT);
}

static void mtk_dvo_config_hsync(struct mtk_dvo *dvo,
				 struct mtk_dvo_sync_param *sync)
{
	mtk_dvo_mask(dvo, DVO_TGEN_H0, sync->sync_width << HSYNC,
		     dvo->conf->dimension_mask << HSYNC);
	mtk_dvo_mask(dvo, DVO_TGEN_H0, sync->front_porch << HFP,
		     dvo->conf->dimension_mask << HFP);
	mtk_dvo_mask(dvo, DVO_TGEN_H1, (sync->back_porch + sync->sync_width) << HSYNC2ACT,
		 dvo->conf->dimension_mask << HSYNC2ACT);
}

static void mtk_dvo_config_vsync(struct mtk_dvo *dvo,
				 struct mtk_dvo_sync_param *sync,
				 u32 width_addr, u32 porch_addr)
{
	mtk_dvo_mask(dvo, width_addr,
		     sync->sync_width << VSYNC,
		     dvo->conf->dimension_mask << VSYNC);
	mtk_dvo_mask(dvo, width_addr,
		     sync->front_porch << VFP,
		     dvo->conf->dimension_mask << VFP);
	mtk_dvo_mask(dvo, porch_addr,
	     (sync->back_porch + sync->sync_width) << VSYNC2ACT,
	     dvo->conf->dimension_mask << VSYNC2ACT);
}

static void mtk_dvo_config_vsync_lodd(struct mtk_dvo *dvo,
				      struct mtk_dvo_sync_param *sync)
{
	mtk_dvo_config_vsync(dvo, sync, DVO_TGEN_V0, DVO_TGEN_V1);
}

static irqreturn_t mtk_dvo_irq_status(int irq, void *dev_id)
{
	struct mtk_dvo *dvo = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc;

	status = readl(dvo->regs + DVO_INTSTA);

	status &= 0xFFFFFFFF;
	if (status) {
		mtk_dvo_mask(dvo, DVO_INTSTA, 0, status);
		if (status & INT_VSYNC_START_STA) {
			mtk_crtc = dvo->ddp_comp.mtk_crtc;
			if (mtk_crtc)
				mtk_crtc_vblank_irq(&mtk_crtc->base);
			irq_intsa++;
		}

		if (status & INT_VDE_START_STA)
			irq_vdesa++;

		if (status & INT_UNDERFLOW_STA)
			irq_underflowsa++;

		if (status & INT_TARGET_LINE0_STA)
			irq_tl++;
	}

	return IRQ_HANDLED;
}

static void mtk_dvo_config_interface(struct mtk_dvo *dvo, bool inter)
{
	mtk_dvo_mask(dvo, DVO_CON, inter ? INTL_EN : 0, INTL_EN);
}

static void mtk_dvo_config_fb_size(struct mtk_dvo *dvo, u32 width, u32 height)
{
	mtk_dvo_mask(dvo, DVO_SRC_SIZE, width << SRC_HSIZE,
		     dvo->conf->hvsize_mask << SRC_HSIZE);
	mtk_dvo_mask(dvo, DVO_SRC_SIZE, height << SRC_VSIZE,
		     dvo->conf->hvsize_mask << SRC_VSIZE);

	mtk_dvo_mask(dvo, DVO_PIC_SIZE, width << PIC_HSIZE,
		     dvo->conf->hvsize_mask << PIC_HSIZE);
	mtk_dvo_mask(dvo, DVO_PIC_SIZE, height << PIC_VSIZE,
		     dvo->conf->hvsize_mask << PIC_VSIZE);

	mtk_dvo_mask(dvo, DVO_TGEN_H1, (width/4) << HACT,
		     dvo->conf->hvsize_mask << HACT);
	mtk_dvo_mask(dvo, DVO_TGEN_V1, height << VACT,
		     dvo->conf->hvsize_mask << VACT);
}

static void mtk_dvo_power_off(struct mtk_dvo *dvo)
{
	if (WARN_ON(dvo->refcount == 0))
		return;

	if (--dvo->refcount != 0)
		return;

	mtk_dvo_enable(dvo, false);
	clk_disable_unprepare(dvo->engine_clk);
	clk_disable_unprepare(dvo->tvd_clk);
	clk_disable_unprepare(dvo->hf_fdvo_clk);
}

static int mtk_dvo_power_on(struct mtk_dvo *dvo)
{
	int ret;

	if (++dvo->refcount != 1)
		return 0;

	/* open MMSYS_CG_1 MOD6_HF_CK_CG for DVO */
	ret = clk_prepare_enable(dvo->hf_fdvo_clk);
	if (ret) {
		dev_info(dvo->dev, "Failed to enable hf_fdvo_clk: %d\n", ret);
		goto err_hf_fdvo;
	}

	/* set DVO switch 26Mhz crystal */
	clk_set_parent(dvo->pixel_clk, dvo->dvo_clk);

	ret = clk_prepare_enable(dvo->tvd_clk);
	if (ret) {
		dev_info(dvo->dev, "[eDPTX] Failed to enable tvd_clk clock: %d\n", ret);
		goto err_tvd;
	}

	ret = clk_prepare_enable(dvo->engine_clk);
	if (ret) {
		dev_info(dvo->dev, "[eDPTX] Failed to enable engine clock: %d\n", ret);
		goto err_refcount;
	}

	return 0;

err_refcount:
	clk_disable_unprepare(dvo->tvd_clk);
err_tvd:
	clk_disable_unprepare(dvo->hf_fdvo_clk);
err_hf_fdvo:
	dvo->refcount--;
	return ret;
}

static int mtk_dvo_set_display_mode(struct mtk_dvo *dvo,
				    struct drm_display_mode *mode)
{
	struct mtk_dvo_polarities dvo_pol;
	struct mtk_dvo_sync_param hsync;
	struct mtk_dvo_sync_param vsync_lodd = { 0 };
	struct mtk_dvo_sync_param vsync_leven = { 0 };
	struct mtk_dvo_sync_param vsync_rodd = { 0 };
	struct mtk_dvo_sync_param vsync_reven = { 0 };
	struct videomode vm = { 0 };
	unsigned long pll_rate = 0;
	unsigned int factor = 0;
	int ret = 0;

	pr_info("[eDPTX] %s+\n", __func__);
	mtk_dvo_enable(dvo, false);

	/* let pll_rate can fix the valid range of tvdpll (1G~2GHz) */
	factor = dvo->conf->cal_factor(mode->clock);
	drm_display_mode_to_videomode(mode, &vm);

	pr_notice("[eDPTX] vm.pixelclock=%lu\n", vm.pixelclock);
	pr_notice("[eDPTX] vm.hactive=%d vm.hfront_porch=%d vm.hback_porch=%d vm.hsync_len=%d\n",
			vm.hactive, vm.hfront_porch, vm.hback_porch, vm.hsync_len);

	pr_notice("[eDPTX] vm.vactive=%d vm.vfront_porch=%d vm.vback_porch=%d vm.vsync_len=%d\n",
			vm.vactive, vm.vfront_porch, vm.vback_porch, vm.vsync_len);

	pll_rate = vm.pixelclock * factor;

	pr_info("[eDPTX] Want PLL %lu Hz, pixel clock %lu Hz\n", pll_rate, vm.pixelclock);

	ret = clk_set_rate(dvo->tvd_clk, pll_rate);
	if (ret) {
		pr_info("[eDPTX] Failed to set dvo->tvd_clk rate: %d\n", ret);
		return ret;
	}

	pll_rate = clk_get_rate(dvo->tvd_clk);

	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	vm.pixelclock = pll_rate / factor;
	vm.pixelclock /= dvo->conf->pixels_per_iter;

	if (factor == 1)
		ret = clk_set_parent(dvo->pixel_clk, dvo->pclk_src[2]);
	else if (factor == 2)
		ret = clk_set_parent(dvo->pixel_clk, dvo->pclk_src[3]);
	else if (factor == 4)
		ret = clk_set_parent(dvo->pixel_clk, dvo->pclk_src[4]);
	else
		ret = clk_set_parent(dvo->pixel_clk, dvo->pclk_src[2]);

	if (ret) {
		pr_info("[eDPTX] Failed to set pixel clock parent %d\n", ret);
		return ret;
	}

	vm.pixelclock = clk_get_rate(dvo->pixel_clk);

	pr_info("[eDPTX] Got  PLL %lu Hz, pixel clock %lu Hz\n",
		pll_rate, vm.pixelclock);

	dvo_pol.ck_pol = MTK_DVO_POLARITY_FALLING;
	dvo_pol.de_pol = MTK_DVO_POLARITY_RISING;
	dvo_pol.hsync_pol = vm.flags & DISPLAY_FLAGS_HSYNC_HIGH ?
						MTK_DVO_POLARITY_FALLING : MTK_DVO_POLARITY_RISING;
	dvo_pol.vsync_pol = vm.flags & DISPLAY_FLAGS_VSYNC_HIGH ?
						MTK_DVO_POLARITY_FALLING : MTK_DVO_POLARITY_RISING;
	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	hsync.sync_width = vm.hsync_len / dvo->conf->pixels_per_iter;
	hsync.back_porch = vm.hback_porch / dvo->conf->pixels_per_iter;
	hsync.front_porch = vm.hfront_porch / dvo->conf->pixels_per_iter;

	hsync.shift_half_line = false;
	vsync_lodd.sync_width = vm.vsync_len;
	vsync_lodd.back_porch = vm.vback_porch;
	vsync_lodd.front_porch = vm.vfront_porch;
	vsync_lodd.shift_half_line = false;

	if (vm.flags & DISPLAY_FLAGS_INTERLACED &&
		mode->flags & DRM_MODE_FLAG_3D_MASK) {
		vsync_leven = vsync_lodd;
		vsync_rodd = vsync_lodd;
		vsync_reven = vsync_lodd;
		vsync_leven.shift_half_line = true;
		vsync_reven.shift_half_line = true;
	} else if (vm.flags & DISPLAY_FLAGS_INTERLACED &&
				!(mode->flags & DRM_MODE_FLAG_3D_MASK)) {
		vsync_leven = vsync_lodd;
		vsync_leven.shift_half_line = true;
	} else if (!(vm.flags & DISPLAY_FLAGS_INTERLACED) &&
				mode->flags & DRM_MODE_FLAG_3D_MASK) {
		vsync_reven = vsync_lodd;
	}

	mtk_dvo_sw_reset(dvo, true);
	mtk_dvo_config_pol(dvo, &dvo_pol);

	mtk_dvo_irq_enable(dvo);
	if (dvo->conf->out_np_sel) {
		mtk_dvo_mask(dvo, DVO_OUTPUT_SET, dvo->conf->out_np_sel,
			     OUT_NP_SEL_MASK);
	}

	mtk_dvo_config_hsync(dvo, &hsync);
	mtk_dvo_config_vsync_lodd(dvo, &vsync_lodd);

	mtk_dvo_config_interface(dvo, !!(vm.flags &
					 DISPLAY_FLAGS_INTERLACED));

	if (vm.flags & DISPLAY_FLAGS_INTERLACED)
		mtk_dvo_config_fb_size(dvo, vm.hactive, vm.vactive >> 1);
	else
		mtk_dvo_config_fb_size(dvo, vm.hactive, vm.vactive);

	mtk_dvo_info_queue_start(dvo);
	mtk_dvo_buffer_ctrl(dvo);
	mtk_dvo_trailing_blank_setting(dvo);
	mtk_dvo_config_color_format(dvo, dvo->color_format);
	mtk_dvo_config_color_depth(dvo);
	mtk_dvo_sodi_setting(dvo, mode);

	if (dvo->conf->pixels_per_iter)
		mtk_dvo_shadow_ctrl(dvo);

	mtk_dvo_sw_reset(dvo, false);

	pr_info("[eDPTX] %s-\n", __func__);

	return 0;
}

static u32 *mtk_dvo_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						      struct drm_bridge_state *bridge_state,
						      struct drm_crtc_state *crtc_state,
						      struct drm_connector_state *conn_state,
						      unsigned int *num_output_fmts)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	u32 *output_fmts;

	*num_output_fmts = 0;
	if (!dvo->conf->output_fmts) {
		dev_info(dvo->dev, "output_fmts should not be null\n");
		return NULL;
	}

	output_fmts = kcalloc(dvo->conf->num_output_fmts, sizeof(*output_fmts),
			     GFP_KERNEL);
	if (!output_fmts)
		return NULL;

	*num_output_fmts = dvo->conf->num_output_fmts;

	memcpy(output_fmts, dvo->conf->output_fmts,
	       sizeof(*output_fmts) * dvo->conf->num_output_fmts);

	return output_fmts;
}

static u32 *mtk_dvo_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     u32 output_fmt,
						     unsigned int *num_input_fmts)
{
	u32 *input_fmts;

	*num_input_fmts = 0;

	input_fmts = kcalloc(1, sizeof(*input_fmts),
			     GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	*num_input_fmts = 1;
	input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;

#ifdef EDPTX_DEBUG
	pr_info("[eDPTX] %s num_input_fmts:%d input_fmts:0x%04x\n",
			__func__, *num_input_fmts, input_fmts[0]);
#endif

	return input_fmts;
}

static int mtk_dvo_bridge_atomic_check(struct drm_bridge *bridge,
				       struct drm_bridge_state *bridge_state,
				       struct drm_crtc_state *crtc_state,
				       struct drm_connector_state *conn_state)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	struct drm_display_info *display_info =
		&conn_state->connector->display_info;
	unsigned int out_bus_format;

	out_bus_format = bridge_state->output_bus_cfg.format;
	if (display_info->bpc)
		dvo->color_depth = display_info->bpc;
	else
		dvo->color_depth = 8;
#ifdef EDPTX_DEBUG
	pr_info("[eDPTX] %s+ bridge_state out_bus_format:0x%04x\n", __func__, out_bus_format);
#endif

	if (out_bus_format == MEDIA_BUS_FMT_FIXED)
		if (dvo->conf->num_output_fmts)
			out_bus_format = dvo->conf->output_fmts[0];

#ifdef EDPTX_DEBUG
	dev_info(dvo->dev, "[eDPTX] %s input format 0x%04x, output format 0x%04x\n",
		__func__,
		bridge_state->input_bus_cfg.format,
		out_bus_format);
#endif

	dvo->output_fmt = out_bus_format;
	dvo->bit_num = MTK_DVO_OUT_BIT_NUM_8BITS;
	dvo->channel_swap = MTK_DVO_OUT_CHANNEL_SWAP_RGB;
	dvo->yc_map = MTK_DVO_OUT_YC_MAP_RGB;
	if (out_bus_format == MEDIA_BUS_FMT_YUYV8_1X16)
		dvo->color_format = MTK_DVO_COLOR_FORMAT_YCBCR_422;
	else
		dvo->color_format = MTK_DVO_COLOR_FORMAT_RGB;

	return 0;
}

static int mtk_dvo_bridge_attach(struct drm_bridge *bridge,
				 enum drm_bridge_attach_flags flags)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	int ret = 0, retry = 7;

	/* set DVO switch 26Mhz crystal */
	clk_set_parent(dvo->pixel_clk, dvo->dvo_clk);

	while (retry) {
		retry--;
		dvo->next_bridge = devm_drm_of_get_bridge(dvo->dev, dvo->dev->of_node, 0, 0);
		if (IS_ERR(dvo->next_bridge)) {
			pr_info("[eDPTX] Failed to get bridge\n");
			usleep_range(500, 800);
			continue;
		}

		dev_info(dvo->dev, "[eDPTX] Found bridge node: %pOF\n", dvo->next_bridge->of_node);
		break;
	}

	if (!retry)
		return -EINVAL;

	ret = drm_bridge_attach(bridge->encoder, dvo->next_bridge,
				 &dvo->bridge, flags);
	if (ret)
		dev_info(dvo->dev, "[eDPTX] Failed to call attach ret = %d\n", ret);

	return ret;
}

static void mtk_dvo_bridge_mode_set(struct drm_bridge *bridge,
				const struct drm_display_mode *mode,
				const struct drm_display_mode *adjusted_mode)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	pr_info("[eDPTX] %s\n", __func__);
	drm_mode_copy(&dvo->mode, adjusted_mode);
}

static void mtk_dvo_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	pr_info("[eDPTX] %s\n", __func__);

	clk_set_parent(dvo->pixel_clk, dvo->dvo_clk);
	mtk_dvo_power_off(dvo);

	if (dvo->pinctrl && dvo->pins_gpio)
		pinctrl_select_state(dvo->pinctrl, dvo->pins_gpio);

	mtk_dvo_pm_ctl(dvo, false);
}

static void mtk_dvo_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	dev_info(dvo->dev, "[eDPTX] %s+\n", __func__);
	mtk_dvo_pm_ctl(dvo, true);
	if (dvo->pinctrl && dvo->pins_dvo)
		pinctrl_select_state(dvo->pinctrl, dvo->pins_dvo);

	mtk_dvo_power_on(dvo);
	mtk_dvo_set_display_mode(dvo, &dvo->mode);

#if DVO_COLOR_BAR
	mtk_dvo_mask(dvo, DVO_PATTERN_CTRL, PRE_PAT_EN, PRE_PAT_EN);
	mtk_dvo_mask(dvo, DVO_PATTERN_CTRL, COLOR_BAR, PRE_PAT_SEL_MASK);
	mtk_dvo_mask(dvo, DVO_PATTERN_CTRL, PRE_PAT_FORCE_ON, PRE_PAT_FORCE_ON);
	mtk_dvo_mask(dvo, DVO_PATTERN_COLOR, PAT_G, PAT_G);
#endif

	mtk_dvo_enable(dvo, true);

	dev_info(dvo->dev, "[eDPTX] %s-\n", __func__);
}

static enum drm_mode_status
mtk_dvo_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_info *info,
			  const struct drm_display_mode *mode)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	if (mode->clock > dvo->conf->max_clock_khz) {
		pr_info("[eDPTX] Invalid mode mode->clock= %d\n", mode->clock);
		return MODE_CLOCK_HIGH;
	}

#ifdef EDPTX_DEBUG
	pr_info("[eDPTX] %s mode->clock=%d\n", __func__, mode->clock);
#endif

	return MODE_OK;
}

static const struct drm_bridge_funcs mtk_dvo_bridge_funcs = {
	.attach = mtk_dvo_bridge_attach,
	.mode_set = mtk_dvo_bridge_mode_set,
	.mode_valid = mtk_dvo_bridge_mode_valid,
	.disable = mtk_dvo_bridge_disable,
	.enable = mtk_dvo_bridge_enable,
	.atomic_check = mtk_dvo_bridge_atomic_check,
	.atomic_get_output_bus_fmts = mtk_dvo_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dvo_bridge_atomic_get_input_bus_fmts,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
};

void mtk_dvo_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_dvo *dvo = container_of(comp, struct mtk_dvo, ddp_comp);

	dev_info(dvo->dev, "[eDPTX] %s\n", __func__);
}

void mtk_dvo_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_dvo *dvo = container_of(comp, struct mtk_dvo, ddp_comp);

	dev_info(dvo->dev, "[eDPTX] %s\n", __func__);

	//mtk_dvo_power_off(dvo);
}

unsigned long long mtk_dvo_get_frame_hrt_bw_base_by_datarate(
		struct mtk_drm_crtc *mtk_crtc,
		struct mtk_dvo *dvo)
{
	static unsigned long long bw_base;
	int hact, vtotal, vact, vrefresh;

	if (!mtk_crtc) {
		DDPDBG("%s mtk_crtc is null\n", __func__);
		return 0;
	}

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);

	bw_base = (unsigned long long)vact * (unsigned long long)hact * vrefresh * 4 / 1000;
	bw_base = bw_base * vtotal / vact;
	bw_base = bw_base / 1000;

	DDPDBG("Frame Bw:%llu",	bw_base);
	return bw_base;
}

static int mtk_dvo_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_dvo *dvo = container_of(comp, struct mtk_dvo, ddp_comp);
	struct videomode vm = { 0 };
	struct drm_display_mode **mode;
	unsigned int vfporch, vbporch, vsync, hfporch, hbporch, hsync;

	drm_display_mode_to_videomode(&dvo->mode, &vm);

	switch (cmd) {
	case DSI_GET_TIMING:
	{
		mode = (struct drm_display_mode **)params;

		dvo->mode.hdisplay = readl(dvo->regs + DVO_SIZE) & 0xFFFF;
		dvo->mode.vdisplay = readl(dvo->regs + DVO_SIZE) >> 16;
		vfporch = readl(dvo->regs + DVO_TGEN_VPORCH) >> 16;
		vbporch = readl(dvo->regs + DVO_TGEN_VPORCH) & 0xFFFF;
		vsync = readl(dvo->regs + DVO_TGEN_VWIDTH) & 0xFFFF;
		dvo->mode.vtotal = dvo->mode.vdisplay + vfporch + vbporch + vsync;
		hfporch = readl(dvo->regs + DVO_TGEN_HPORCH) >> 16;
		hbporch = readl(dvo->regs + DVO_TGEN_HPORCH) & 0xFFFF;
		hsync = readl(dvo->regs + DVO_TGEN_HWIDTH) & 0xFFFF;
		dvo->mode.htotal = dvo->mode.hdisplay + hfporch + hbporch + hsync;
		dvo->mode.clock = clk_get_rate(dvo->pixel_clk) * 4 / 1000;
		dev_info(dvo->dev, "[eDPTX] %s cmd:%d\n", __func__, cmd);
		dev_info(dvo->dev, "[eDPTX] dvo->hdisplay = %d\n", dvo->mode.hdisplay);
		dev_info(dvo->dev, "[eDPTX] dvo->vdisplay = %d\n", dvo->mode.vdisplay);
		dev_info(dvo->dev, "[eDPTX] dvo->clock = %d, fps: %d\n",
			dvo->mode.clock, drm_mode_vrefresh(&dvo->mode));
		*mode = &dvo->mode;
	}
		break;
	case SET_MMCLK_BY_DATARATE:
	{
#ifndef CONFIG_FPGA_EARLY_PORTING
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned int pixelclock = vm.pixelclock / 1000000;

		mtk_drm_set_mmclk_by_pixclk(&(mtk_crtc->base),
			pixelclock, __func__);
#endif
	}
		break;
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dvo_get_frame_hrt_bw_base_by_datarate(crtc, dvo);
	}
		break;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	case SET_CRTC_ID:
	{
		DDPMSG("%s set %s possible crtcs 0x%x\n", __func__,
			mtk_dump_comp_str(comp), *(unsigned int *)params);
		dvo->encoder.possible_crtcs = *(unsigned int *)params;
	}
		break;
#endif
	default:
		break;
	}
	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dvo_funcs = {
	.start = mtk_dvo_start,
	.stop = mtk_dvo_stop,
	.io_cmd = mtk_dvo_io_cmd,
};

static int mtk_dvo_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	int ret;

	dev_info(dev, "[eDPTX] %s+\n", __func__);

	dvo->mmsys_dev = priv->mmsys_dev;
	ret = mtk_ddp_comp_register(drm_dev, &dvo->ddp_comp);
	if (ret < 0) {
		dev_info(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	ret = drm_simple_encoder_init(drm_dev, &dvo->encoder,
				      DRM_MODE_ENCODER_TMDS);
	if (ret) {
		dev_info(dev, "Failed to initialize decoder: %d\n", ret);
		return ret;
	}

	dvo->encoder.possible_crtcs = mtk_drm_find_possible_crtc_by_comp(drm_dev, dvo->ddp_comp);

	ret = drm_bridge_attach(&dvo->encoder, &dvo->bridge, NULL,
				DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret)
		goto err_cleanup;

	dvo->connector = drm_bridge_connector_init(drm_dev, &dvo->encoder);
	if (IS_ERR(dvo->connector)) {
		dev_info(dev, "Unable to create bridge connector\n");
		ret = PTR_ERR(dvo->connector);
		goto err_cleanup;
	}
	drm_connector_attach_encoder(dvo->connector, &dvo->encoder);

	dev_info(dev, "[eDPTX] %s-\n", __func__);

	return 0;

err_cleanup:
	drm_encoder_cleanup(&dvo->encoder);
	return ret;
}

static void mtk_dvo_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);

	drm_encoder_cleanup(&dvo->encoder);
}

static const struct component_ops mtk_dvo_component_ops = {
	.bind = mtk_dvo_bind,
	.unbind = mtk_dvo_unbind,
};

static unsigned int mt6991_calculate_factor(int clock)
{
	if (clock < 70000)
		return 4;
	else if (clock < 200000)
		return 2;
	else
		return 1;

}

static const u32 mt6991_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static const struct mtk_dvo_conf mt6991_conf = {
	.cal_factor = mt6991_calculate_factor,
	.out_np_sel = 0x2,
	.reg_h_fre_con = 0xb0,
	.max_clock_khz = 600000,
	.output_fmts = mt6991_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt6991_output_fmts),
	.pixels_per_iter = 4,
	.has_commit = true,
	.dimension_mask = HFP_MASK,
	.hvsize_mask = PIC_HSIZE_MASK,
	.csc_enable_bit = CSC_EN,
	.yuv422_en_bit = YUV422_EN | VYU_MAP,
	.swap_input_support = false,
};

int mtk_drm_dvo_get_info(struct drm_device *dev,
			struct drm_mtk_session_info *info)
{
	if (!g_mtk_dvo) {
		pr_info("[eDPTX] %s dvo not initial\n", __func__);
		return -EINVAL;
	}

	info->physical_width = g_mtk_dvo->mode.hdisplay;
	info->physical_height = g_mtk_dvo->mode.vdisplay;
	pr_info("[eDPTX] physical_width:%u physical_height:%u\n",
			info->physical_width, info->physical_height);

	return 0;
}
EXPORT_SYMBOL(mtk_drm_dvo_get_info);

static int mtk_dvo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dvo *dvo;
	int comp_id;
	int ret;

	dev_info(dev, "[eDPTX] %s+\n", __func__);
	dvo = devm_kzalloc(dev, sizeof(*dvo), GFP_KERNEL);
	if (!dvo)
		return -ENOMEM;

	g_mtk_dvo = dvo;
	dvo->dev = dev;
	dvo->conf = (struct mtk_dvo_conf *)of_device_get_match_data(dev);
	dvo->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	dvo->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dvo->pinctrl)) {
		dvo->pinctrl = NULL;
		dev_dbg(&pdev->dev, "Cannot find pinctrl!\n");
	}
	if (dvo->pinctrl) {
		dvo->pins_gpio = pinctrl_lookup_state(dvo->pinctrl, "sleep");
		if (IS_ERR(dvo->pins_gpio)) {
			dvo->pins_gpio = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl idle!\n");
		}
		if (dvo->pins_gpio)
			pinctrl_select_state(dvo->pinctrl, dvo->pins_gpio);

		dvo->pins_dvo = pinctrl_lookup_state(dvo->pinctrl, "default");
		if (IS_ERR(dvo->pins_dvo)) {
			dvo->pins_dvo = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl active!\n");
		}
	}
	dvo->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dvo->regs)) {
		pr_info("[eDPTX ] Failed to ioremap mem resource\n");
		return PTR_ERR(dvo->regs);
	}

	dvo->engine_clk = devm_clk_get(dev, "engine");
	if (IS_ERR(dvo->engine_clk)) {
		pr_info("[eDPTX ] Failed to get engine clock\n");
		return PTR_ERR(dvo->engine_clk);
	}

	dvo->pixel_clk = devm_clk_get(dev, "pixel");
	if (IS_ERR(dvo->pixel_clk)) {
		pr_info("[eDPTX ] Failed to get pixel clock\n");
		return PTR_ERR(dvo->pixel_clk);
	}

	dvo->tvd_clk = devm_clk_get(dev, "pll");
	if (IS_ERR(dvo->tvd_clk)) {
		pr_info("[eDPTX ] Failed to get tvdpll clock\n");
		return PTR_ERR(dvo->tvd_clk);
	}

	dvo->dvo_clk = devm_clk_get(dev, "dvo_clk");
	if (IS_ERR(dvo->dvo_clk)) {
		pr_info("[eDPTX ] Failed to get tck26_clk clock\n");
		return PTR_ERR(dvo->dvo_clk);
	}

	dvo->hf_fdvo_clk = devm_clk_get(dev, "hf_fdvo_clk");
	if (IS_ERR(dvo->hf_fdvo_clk)) {
		pr_info("[eDPTX ] Failed to get hf_fdvo_clk clock\n");
		return PTR_ERR(dvo->hf_fdvo_clk);
	}

	dvo->pclk_src[1] = devm_clk_get(dev, "TVDPLL_D2");
	dvo->pclk_src[2] = devm_clk_get(dev, "TVDPLL_D4");
	dvo->pclk_src[3] = devm_clk_get(dev, "TVDPLL_D8");
	dvo->pclk_src[4] = devm_clk_get(dev, "TVDPLL_D16");

	dvo->irq = platform_get_irq(pdev, 0);
	if (dvo->irq < 0) {
		pr_info("[eDPTX] failed to get edp dvo irq num\n");
		return dvo->irq;
	}

	irq_set_status_flags(dvo->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dvo->irq, mtk_dvo_irq_status,
		IRQ_TYPE_LEVEL_HIGH, dev_name(&pdev->dev), dvo);
	if (ret) {
		dev_info(&pdev->dev, "failed to request mediatek edp dvo irq\n");
		return -EPROBE_DEFER;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DVO);
	if (comp_id < 0) {
		dev_info(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dvo->ddp_comp, comp_id,
				&mtk_dvo_funcs);
	if (ret) {
		dev_info(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, dvo);

	dvo->bridge.funcs = &mtk_dvo_bridge_funcs;
	dvo->bridge.of_node = dev->of_node;
	dvo->bridge.type = DRM_MODE_CONNECTOR_DPI;

	ret = devm_drm_bridge_add(dev, &dvo->bridge);
	if (ret)
		return ret;

	mtk_dvo_pm_ctl(dvo, true);
	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dvo_component_ops);
	if (ret) {
		pr_info("[eDPTX] Failed to add component\n");
		return ret;
	}

	dev_info(dev, "[eDPTX] %s-\n", __func__);

	return 0;
}

static void mtk_dvo_remove(struct platform_device *pdev)
{
	struct mtk_dvo *dvo = platform_get_drvdata(pdev);

	mtk_dvo_pm_ctl(dvo, false);
	pm_runtime_disable(&pdev->dev);
	component_del(&pdev->dev, &mtk_dvo_component_ops);
}

static const struct of_device_id mtk_dvo_of_ids[] = {
	{ .compatible = "mediatek,mt6991-edp-dvo", .data = &mt6991_conf },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_dvo_of_ids);

struct platform_driver mtk_dvo_driver = {
	.probe = mtk_dvo_probe,
	.remove_new = mtk_dvo_remove,
	.driver = {
		.name = "mediatek-dvo",
		.of_match_table = mtk_dvo_of_ids,
	},
};

