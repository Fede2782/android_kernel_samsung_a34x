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

#include "mtk_disp_mdp_rsz.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "platform/mtk_drm_platform.h"

#define RSZ_ENABLE			0x000
#define FLD_RSZ_RST REG_FLD_MSB_LSB(16, 16)
#define FLD_RSZ_EN REG_FLD_MSB_LSB(0, 0)
#define RSZ_CON_1			0x004
#define FLD_RSZ_INT_WCLR_EN REG_FLD_MSB_LSB(31, 31)
#define FLD_RSZ_INTEN REG_FLD_MSB_LSB(30, 28)
#define FLD_RSZ_DCM_DIS REG_FLD_MSB_LSB(27, 27)
#define FLD_RSZ_VERTICAL_TABLE_SELECT REG_FLD_MSB_LSB(25, 21)
#define FLD_RSZ_HORIZONTAL_TABLE_SELECT REG_FLD_MSB_LSB(20, 16)
#define FLD_RSZ_VERTICAL_EN REG_FLD_MSB_LSB(1, 1)
#define FLD_RSZ_HORIZONTAL_EN REG_FLD_MSB_LSB(0, 0)
#define RSZ_CON_2			0x008
#define FLD_RSZ_RGB_BIT_MODE REG_FLD_MSB_LSB(28, 28)
#define FLD_RSZ_POWER_SAVING REG_FLD_MSB_LSB(9, 9)
#define RSZ_INT_FLAG			0x00c
#define FLD_RSZ_SOF_RESET REG_FLD_MSB_LSB(5, 5)
#define FLD_RSZ_SIZE_ERR REG_FLD_MSB_LSB(4, 4)
#define FLD_RSZ_FRAME_END REG_FLD_MSB_LSB(1, 1)
#define FLD_RSZ_FRAME_START REG_FLD_MSB_LSB(0, 0)
#define RSZ_INPUT_IMAGE			0x010
#define FLD_RSZ_INPUT_IMAGE_H REG_FLD_MSB_LSB(31, 16)
#define FLD_RSZ_INPUT_IMAGE_W REG_FLD_MSB_LSB(15, 0)
#define RSZ_OUTPUT_IMAGE		0x014
#define FLD_RSZ_OUTPUT_IMAGE_H REG_FLD_MSB_LSB(31, 16)
#define FLD_RSZ_OUTPUT_IMAGE_W REG_FLD_MSB_LSB(15, 0)
#define RSZ_HOR_COEFF_STEP		0x018
#define RSZ_VER_COEFF_STEP		0x01c
#define RSZ_LUMA_HOR_INT_OFFSET		0x020
#define RSZ_LUMA_HOR_SUB_OFFSET		0x024
#define RSZ_LUMA_VER_INT_OFFSET		0x028
#define RSZ_LUMA_VER_SUB_OFFSET		0x02c
#define RSZ_CHROMA_HOR_INT_OFFSET	0x030
#define RSZ_CHROMA_HOR_SUB_OFFSET	0x034
#define RSZ_RSV				0x040
#define RSZ_DEBUG_SEL			0x044
#define RSZ_DEBUG			0x048
#define RSZ_TAP_ADAPT			0x04c
#define RSZ_IBSE_SOFTCLIP		0x050
#define RSZ_IBSE_YLEVEL_1		0x054
#define RSZ_IBSE_YLEVEL_2		0x058
#define RSZ_IBSE_YLEVEL_3		0x05c
#define RSZ_IBSE_YLEVEL_4		0x060
#define RSZ_IBSE_YLEVEL_5		0x064
#define RSZ_IBSE_GAINCON_1		0x068
#define RSZ_IBSE_GAINCON_2		0x06c
#define RSZ_DEMO_IN_HMASK		0x070
#define RSZ_DEMO_IN_VMASK		0x074
#define RSZ_DEMO_OUT_HMASK		0x078
#define RSZ_DEMO_OUT_VMASK		0x07c
#define RSZ_CONTROL_3			0x084
#define RSZ_SHADOW_CTRL			0x0f0
#define FLD_RSZ_READ_WRK_REG REG_FLD_MSB_LSB(2, 2)
#define FLD_RSZ_FORCE_COMMIT REG_FLD_MSB_LSB(1, 1)
#define FLD_RSZ_BYPASS_SHADOW REG_FLD_MSB_LSB(0, 0)
#define RSZ_ATPG			0x0fc
#define RSZ_PAT1_GEN_SET		0x100
#define RSZ_PAT1_GEN_FRM_SIZE		0x104
#define RSZ_PAT1_GEN_COLOR0		0x108
#define RSZ_PAT1_GEN_COLOR1		0x10c
#define RSZ_PAT1_GEN_COLOR2		0x110
#define RSZ_PAT1_GEN_POS		0x114
#define RSZ_PAT1_GEN_TILE_POS		0x124
#define RSZ_PAT1_GEN_TILE_OV		0x128
#define RSZ_PAT2_GEN_SET		0x200
#define RSZ_PAT2_GEN_COLOR0		0x208
#define RSZ_PAT2_GEN_COLOR1		0x20c
#define RSZ_PAT2_GEN_POS		0x214
#define RSZ_PAT2_GEN_CURSOR_RB0		0x218
#define RSZ_PAT2_GEN_CURSOR_RB1		0x21c
#define RSZ_PAT2_GEN_TILE_POS		0x224
#define RSZ_PAT2_GEN_TILE_OV		0x228
#define RSZ_ETC_CONTROL			0x22c
#define RSZ_ETC_SWITCH_MAX_MIN_1	0x230
#define RSZ_ETC_SWITCH_MAX_MIN_2	0x234
#define RSZ_ETC_RING			0x238
#define RSZ_ETC_RING_GAINCON_1		0x23c
#define RSZ_ETC_RING_GAINCON_2		0x240
#define RSZ_ETC_RING_GAINCON_3		0x244
#define RSZ_ETC_SIM_PROT_GAINCON_1	0x248
#define RSZ_ETC_SIM_PROT_GAINCON_2	0x24c
#define RSZ_ETC_SIM_PROT_GAINCON_3	0x250
#define RSZ_ETC_BLEND			0x254

#define UNIT 32768
#define OUT_TILE_LOSS 0

enum mtk_rsz_color_format {
	ARGB2101010,
	RGB999,
	RGB888,
	UNKNOWN_RSZ_CFMT,
};

struct rsz_tile_params {
	u32 step;
	u32 int_offset;
	u32 sub_offset;
	u32 in_len;
	u32 out_len;
	u32 ori_in_len;
	u32 ori_out_len;
	u32 src_y;
	u32 dst_y;
	u32 overhead_y;
	bool par_update_y;
};

struct mtk_mdp_rsz_config_struct {
	struct rsz_tile_params tw[2];
	struct rsz_tile_params th[1];
	enum mtk_rsz_color_format fmt;
	u32 frm_in_w;
	u32 frm_in_h;
	u32 frm_out_w;
	u32 frm_out_h;
};

struct mtk_disp_mdp_rsz_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_mdp_rsz_tile_overhead {
	unsigned int left_in_width;
	unsigned int left_overhead;
	unsigned int left_comp_overhead;
	unsigned int left_out_tile_loss;
	unsigned int right_in_width;
	unsigned int right_overhead;
	unsigned int right_comp_overhead;
	unsigned int right_out_tile_loss;
	bool is_support;

	/* store rsz tile_overhead calc */
	struct rsz_tile_params tw[2];

	/* store rsz algo parameters */
	struct rsz_fw_in rsz_in;
	struct rsz_fw_out rsz_out;
};

/**
 * struct mtk_disp_rsz - DISP_MDP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_mdp_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_mdp_rsz_data *data;
	unsigned int set_partial_update;
	unsigned int roi_y_offset;
	unsigned int roi_height;
	struct mtk_disp_mdp_rsz_tile_overhead tile_overhead;
	struct mtk_disp_mdp_rsz_tile_overhead_v tile_overhead_v;
};

struct mtk_disp_mdp_rsz_tile_overhead mdp_rsz_tile_overhead = { 0 };

static inline struct mtk_disp_mdp_rsz *comp_to_mdp_rsz(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_mdp_rsz, ddp_comp);
}

static void mtk_mdp_rsz_back_taps(int outStart, int outEnd, int step, int prec,
	int crop, int crop_frac, int inMaxEnd, int alignment,
	int *inStart, int *inEnd)
{
	s64 startTemp, endTemp;
	int overhead = 3;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * prec) >> RSZ_PREC_SHIFT;

	startTemp = (s64)outStart * step + (s64)crop * prec + crop_frac;
	if (startTemp <= (s64)overhead * prec)
		*inStart = 0;
	else {
		startTemp = startTemp / prec - overhead;
		if (!(startTemp & 0x1) || alignment == 1)
			*inStart = (int)startTemp;
		else /* must be even */
			*inStart = (int)startTemp - 1;
	}

	endTemp = (s64)outEnd * step + (s64)crop * prec + crop_frac +
		(overhead + 2) * prec;
	if(endTemp > (s64)inMaxEnd * prec)
		*inEnd = inMaxEnd;
	else {
		endTemp /= prec;
		if ((endTemp & 0x1) || alignment == 1)
			*inEnd = (int)endTemp;
		else /* must be odd */
			*inEnd = (int)endTemp + 1;
	}
}

static void mtk_mdp_rsz_for_taps(int inStart, int inEnd, int inMaxEnd, int step, int prec,
	int crop, int crop_frac, int outMaxEnd, int alignment, int backOutStart,
	int *outStart, int *outEnd, int *int_offset, int *sub_offset)
{
	int overhead = 3;
	s64 subTemp;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * prec) >> RSZ_PREC_SHIFT;

	if (inEnd == inMaxEnd)
		*outEnd = outMaxEnd;
	else {
		s64 end = (s64)(inEnd - (overhead - 1) - 2) * prec - (s64)crop * prec - crop_frac;
		int tmp = (int)(end / step);

		if((s64)tmp * step == end) /* ceiling in forward */
			tmp -= 1;
		if(tmp & 0x1 || alignment == 1)
			*outEnd = tmp;
		else /* must be odd */
			*outEnd = tmp - 1;
	}

	*outStart = backOutStart;
	subTemp = (s64)*outStart * step + (s64)crop * prec + crop_frac -
		(s64)inStart * prec;
	*int_offset = (int)(subTemp / prec);
	*sub_offset = (int)(subTemp - prec * (*int_offset));
	if (*sub_offset < 0) {
		*int_offset -= 1;
		*sub_offset += prec;
	}

	if (*outEnd > outMaxEnd)
		*outEnd = outMaxEnd;
}

static void mtk_rsz_algo_config(struct rsz_fw_in *in, struct rsz_fw_out *out,
	bool is_hor, struct rsz_cal_param *cal_param)
{
	s32 prec = 0, max_nm = 0, max_nm_prec = 0;
	s32 shift = 0, coeff_index_approx_ini = 0;
	s32 n_m1, m_m1;
	s64 m_m1_zoom, n_m1_zoom;
	s64 offset; /* the offset defined by firmware */
	s64 ratio;
	s32 coeff_rs = 0;
	s32 coeff_step = 0;

	u32 dst_width = in->out_width;
	u32 dst_height = in->out_height;
	u32 crop_offset_x = in->crop.r.left;
	u32 crop_subpix_x = in->crop.x_sub_px;
	u32 crop_offset_y = in->crop.r.top;
	u32 crop_subpix_y = in->crop.y_sub_px;
	u32 crop_width = in->crop.r.width;
	u32 crop_subpix_w = in->crop.w_sub_px;
	u32 crop_height = in->crop.r.height;
	u32 crop_subpix_h = in->crop.h_sub_px;
	u32 alg;

	if (is_hor)
		alg = out->hori_algo;
	else
		alg = out->vert_algo;

	/* Load the parameters needed for resizer configuration */
	if (is_hor) { /* horizontal scaling */
		n_m1 = dst_width;
		m_m1 = crop_width;
		ratio = 1;

		/* 1048576x */
		n_m1_zoom = (n_m1 * ratio) << RSZ_PREC_SHIFT;
		m_m1_zoom = ((m_m1 * ratio) << RSZ_PREC_SHIFT) + crop_subpix_w;
		offset = ((s64)crop_offset_x << RSZ_PREC_SHIFT) + crop_subpix_x;
	} else { /* vertical scaling */
		n_m1 = dst_height;
		m_m1 = crop_height;
		ratio = 1;

		/* 1048576x */
		n_m1_zoom = (n_m1 * ratio) << RSZ_PREC_SHIFT;
		m_m1_zoom = ((m_m1 * ratio) << RSZ_PREC_SHIFT) + crop_subpix_h;
		offset = ((s64)crop_offset_y << RSZ_PREC_SHIFT) + crop_subpix_y;
	}

	DDPINFO("%s m_m1_zoom[%lld]  n_m1_zoom[%lld] prec[%d] offset[%lld]",
		__func__, m_m1_zoom, n_m1_zoom, prec, offset);
	m_m1_zoom -= (1 << RSZ_PREC_SHIFT);
	n_m1_zoom -= (1 << RSZ_PREC_SHIFT);

	/* Resizer parameter configuration */
	prec = 1 << RSZ_6TAP_STEPCOUNTER_BIT;
	max_nm = 1;
	max_nm_prec = max_nm * prec;

	coeff_step = mult_frac(max_nm_prec, m_m1_zoom, n_m1_zoom);

	shift = offset >> RSZ_PREC_SHIFT;
	coeff_index_approx_ini = (offset * prec -
		((u64)shift << RSZ_PREC_SHIFT) *
		prec) >> RSZ_PREC_SHIFT;

	cal_param->hori_trunc_bit = 0;
	cal_param->vert_trunc_bit = 0;
	/* Save the coefficients to the parameters */
	if (is_hor) { /* for horizontal */
		out->hori_step = coeff_step;
		cal_param->hori_luma_int_ofst = shift;
		cal_param->hori_luma_sub_ofst = coeff_index_approx_ini;
		cal_param->hori_trunc_bit = coeff_rs;
		out->precision_x = max_nm_prec;

		if (crop_width < dst_width)
			out->vert_first = 1;
		else
			out->vert_first = 0;
	} else { /* for vertical */
		out->vert_step = coeff_step;
		cal_param->vert_trunc_bit = coeff_rs;
		cal_param->vert_luma_int_ofst = shift;
		cal_param->vert_luma_sub_ofst = coeff_index_approx_ini;
		out->precision_y = max_nm_prec;
	}
}

static void mtk_rsz_algo_auto_align(struct rsz_fw_in *in, struct rsz_fw_out *out,
	bool is_hor, struct rsz_cal_param *cal_param)
{
	s32 prec, max_nm, max_nm_prec;
	s64 dst_width = in->out_width;
	s64 dst_height = in->out_height;
	s64 crop_width = in->crop.r.width;
	s64 crop_subpix_w = in->crop.w_sub_px;
	s64 crop_height = in->crop.r.height;
	s64 crop_subpix_h = in->crop.h_sub_px;
	u32 alg;

	if (is_hor)
		alg = out->hori_algo;
	else
		alg = out->vert_algo;

	prec = 1 << RSZ_6TAP_STEPCOUNTER_BIT;
	max_nm = 1;
	max_nm_prec = max_nm * prec;

	if (is_hor) { /* for horizontal */
		/*
		 * 6-tap FIR: prec=32768; max_nm=1; coeff_step =
		 * (int)((((M_m1_zoom*max_nm)/N_m1_zoom)*prec) + 0.5);
		 */
		cal_param->hori_luma_sub_ofst +=
		(prec * (crop_width - 1) +
		(crop_subpix_w >> 5) -
		(out->hori_step * (dst_width - 1))) / 2;


		/* hardware requirement: always positive subpixel offset */
		if (cal_param->hori_luma_sub_ofst < 0) {
			cal_param->hori_luma_int_ofst--;
			cal_param->hori_luma_sub_ofst = prec +
				cal_param->hori_luma_sub_ofst;
		}
		if (cal_param->hori_luma_sub_ofst >= prec) {
			cal_param->hori_luma_int_ofst++;
			cal_param->hori_luma_sub_ofst =
				cal_param->hori_luma_sub_ofst - prec;
		}
	} else { /* for vertical */
		/*
		 * 6-tap FIR: prec=32768; max_nm=1; coeff_step =
		 * (int)((((M_m1_zoom*max_nm)/N_m1_zoom)*prec) + 0.5);
		 */
		cal_param->vert_luma_sub_ofst +=
				(prec * (crop_height - 1) +
				(crop_subpix_h >> 5) -
				(out->vert_step * (dst_height - 1))) / 2;

		/* hardware requirement: always positive subpixel offset */
		if (cal_param->vert_luma_sub_ofst < 0) {
			cal_param->vert_luma_int_ofst--;
			cal_param->vert_luma_sub_ofst = prec +
				cal_param->vert_luma_sub_ofst;
		}
		if (cal_param->vert_luma_sub_ofst >= prec) {
			cal_param->vert_luma_int_ofst++;
			cal_param->vert_luma_sub_ofst =
				cal_param->vert_luma_sub_ofst - prec;
		}
	}
}

static void mtk_rsz_algo_ofst_check(struct rsz_fw_out *out,
	struct rsz_cal_param *cal_param, bool is_hor)
{
	s32 step_size_6tap = 1 << RSZ_6TAP_STEPCOUNTER_BIT;

	if (is_hor) {
		if (cal_param->hori_luma_sub_ofst >= step_size_6tap) {
			cal_param->hori_luma_int_ofst +=
				cal_param->hori_luma_sub_ofst / step_size_6tap;
			cal_param->hori_luma_sub_ofst =
				cal_param->hori_luma_sub_ofst % step_size_6tap;
		}
	} else {
		if (cal_param->vert_luma_sub_ofst >= step_size_6tap) {
			cal_param->vert_luma_int_ofst +=
				cal_param->vert_luma_sub_ofst / step_size_6tap;
			cal_param->vert_luma_sub_ofst =
				cal_param->vert_luma_sub_ofst % step_size_6tap;
		}
	}
}

static void mtk_rsz_algo_init(struct rsz_fw_out *out,
	struct rsz_cal_param *cal_param)
{
	out->con1 = out->con2 = out->tap_adapt = 0;
	cal_param->int_wclr_en = 1;
	cal_param->tap_adapt_edge_thr = 2;
	cal_param->tap_adapt_dc_coring = 2;
	cal_param->tap_adapt_var_coring = 2;
	cal_param->tap_adapt_fallback_ratio = 0;
	cal_param->tap_adapt_slope = 8;
	cal_param->signal_enhance_mode = 1;
	out->etc_ctrl              = 0x14220000;
	out->etc_switch_max_min1   = 0x23012ac0;
	out->etc_switch_max_min2   = 0x1e232800;
	out->etc_ring              = 0x05260c17;
	out->etc_ring_gaincon1     = 0x1400600d;
	out->etc_ring_gaincon2     = 0x141b3a00;
	out->etc_ring_gaincon3     = 0x0e01000a;
	out->etc_sim_port_gaincon1 = 0x05040f16;
	out->etc_sim_port_gaincon2 = 0x021d0500;
	out->etc_sim_port_gaincon3 = 0x04004000;
	out->etc_blend             = 0x28000000;
}

static void mtk_rsz_fw(struct rsz_fw_in *in, struct rsz_fw_out *out,
				u32 in_len, u32 out_len, bool is_hor)
{
	struct rsz_cal_param cal_param = {0};

	mtk_rsz_algo_init(out, &cal_param);
	in->use121filter = 1;

	out->vert_cubic_trunc = 0;
	cal_param.yuv_422_t_yuv_444 = 0;
	cal_param.tap_adapt_en = 1;
	cal_param.hori_tbl = cal_param.hori_alpha_tbl = 9;
	cal_param.vert_tbl = cal_param.vert_alpha_tbl = 9;
	if (is_hor) {
		in->out_width = out_len;
		in->crop.r.width = in_len;
		in->crop.r.left = in->crop.x_sub_px = in->crop.w_sub_px = 0;
		out->hori_algo = 0;
		mtk_rsz_algo_config(in, out, true, &cal_param);
		mtk_rsz_algo_auto_align(in, out, true, &cal_param);
		mtk_rsz_algo_ofst_check(out, &cal_param, true);
	} else {
		in->out_height = out_len;
		in->crop.r.height = in_len;
		in->crop.r.top = in->crop.y_sub_px = in->crop.h_sub_px = 0;
		out->vert_algo = 0;
		mtk_rsz_algo_config(in, out, false, &cal_param);
		mtk_rsz_algo_auto_align(in, out, false, &cal_param);
		mtk_rsz_algo_ofst_check(out, &cal_param, false);
	}

	cal_param.hori_cubic_trunc_en = 0;
	cal_param.hori_luma_cubic_trunc_bit = 0;
	cal_param.hori_chroma_cubic_trunc_bit = 0;
	out->vert_cubic_trunc = 0;
	cal_param.vert_luma_cubic_trunc_bit = 0;
	cal_param.vert_chroma_cubic_trunc_bit = 0;
	cal_param.tap_adapt_slope = 8;

	if (is_hor) {
		out->hori_int_ofst = cal_param.hori_luma_int_ofst;
		out->hori_sub_ofst = cal_param.hori_luma_sub_ofst;
	} else {
		out->vert_int_ofst = cal_param.vert_luma_int_ofst;
		out->vert_sub_ofst = cal_param.vert_luma_sub_ofst;
	}

	/* always enable hor and ver */
	out->hori_scale = 1;
	out->vert_scale = 1;
	/* Scaling size is 1, need to bound input */
	if (in->crop.r.width == in->out_width)
		out->vert_first = 1;

	out->con1 = out->hori_scale |
		    out->vert_scale << 1 |
		    out->vert_first << 4 |
		    out->hori_algo << 5 |
		    out->vert_algo << 7 |
		    cal_param.hori_trunc_bit << 10 |
		    cal_param.vert_trunc_bit << 13 |
		    cal_param.hori_tbl << 16 |
		    cal_param.vert_tbl << 21 |
		    in->use121filter << 26 |
		    cal_param.int_wclr_en << 31;
	out->con2 = cal_param.tap_adapt_en << 7 |
		    in->power_saving << 9 |
		    in->drs_lclip_en << 11 |
		    in->drs_padding_dis << 12 |
		    in->urs_clip_en << 13 |
		    cal_param.hori_chroma_cubic_trunc_bit << 14 |
		    cal_param.hori_luma_cubic_trunc_bit << 17 |
		    cal_param.hori_cubic_trunc_en << 20 |
		    cal_param.vert_chroma_cubic_trunc_bit << 21 |
		    cal_param.vert_luma_cubic_trunc_bit << 24 |
		    out->vert_cubic_trunc << 27 |
			in->y2r_enable << 30 |
			in->r2y_enable << 31;
	out->con3 = cal_param.hori_alpha_tbl |
			cal_param.vert_alpha_tbl << 5 |
			in->alpha_enable << 10;
	out->tap_adapt = cal_param.tap_adapt_slope |
			 cal_param.tap_adapt_fallback_ratio << 4 |
			 cal_param.tap_adapt_var_coring << 10 |
			 cal_param.tap_adapt_dc_coring << 15 |
			 cal_param.tap_adapt_edge_thr << 20;
	out->etc_ctrl |= (cal_param.signal_enhance_mode << 30);
	out->hori_step &= 0x007fffff;
	out->vert_step &= 0x007fffff;
}

int mtk_mdp_rsz_calc_tile_params(struct mtk_ddp_comp *comp, u32 frm_in_len, u32 frm_out_len, bool tile_mode,
			     struct rsz_tile_params t[], bool is_hor)
{
	u32 out_tile_loss[2] = {OUT_TILE_LOSS, OUT_TILE_LOSS};
	u32 in_tile_loss[2] = {out_tile_loss[0] + 4, out_tile_loss[1] + 4};
	u32 step = 0;
	//s32 init_phase = 0;
	s32 offset[2] = {0};
	s32 int_offset[2] = {0};
	s32 sub_offset[2] = {0};
	u32 tile_in_len[2] = {0};
	u32 tile_out_len[2] = {0};
	u32 y_in_start, y_in_end, y_out_start, y_out_end, dst_y, dst_height;
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);

	DDPINFO("%s:%s:in_len:%u,out_len:%u,ori_in_len:%u,ori_out_len:%u\n", __func__,
		t[0].par_update_y ? "pUpdate:on" : "pUpdate:off", t[0].in_len, t[0].out_len,
		t[0].ori_in_len, t[0].ori_out_len);

	rsz->tile_overhead.rsz_in.r2y_enable = rsz->tile_overhead.rsz_in.y2r_enable = 1;
	rsz->tile_overhead.rsz_in.alpha_enable = 1;
	rsz->tile_overhead.rsz_in.drs_lclip_en = 0;
	rsz->tile_overhead.rsz_in.urs_clip_en = 0;
	if (is_hor)
		rsz->tile_overhead.rsz_in.drs_padding_dis = (frm_in_len - 1) & 0x1;

	if (t[0].par_update_y) {
		is_hor = false;
		mtk_rsz_fw(&(rsz->tile_overhead.rsz_in), &(rsz->tile_overhead.rsz_out),
			t[0].ori_in_len, t[0].ori_out_len, false);
	} else {
		mtk_rsz_fw(&(rsz->tile_overhead.rsz_in), &(rsz->tile_overhead.rsz_out),
			frm_in_len, frm_out_len, is_hor);
	}


	if (is_hor) {
		step = rsz->tile_overhead.rsz_out.hori_step;
		int_offset[0] = rsz->tile_overhead.rsz_out.hori_int_ofst;
		sub_offset[0] = rsz->tile_overhead.rsz_out.hori_sub_ofst;
	} else {
		step = rsz->tile_overhead.rsz_out.vert_step;
		int_offset[0] = rsz->tile_overhead.rsz_out.vert_int_ofst;
		sub_offset[0] = rsz->tile_overhead.rsz_out.vert_sub_ofst;
	}

	if (t[0].par_update_y) {
		dst_y = t[0].dst_y - t[0].overhead_y;
		dst_height = frm_out_len + (t[0].overhead_y << 1);
		sub_offset[0] = ((s64)sub_offset[0] << RSZ_PREC_SHIFT) / UNIT;

		mtk_mdp_rsz_back_taps(dst_y, dst_y + dst_height - 1, step, UNIT,
			int_offset[0], sub_offset[0], t[0].ori_in_len - 1, 2,
			&y_in_start, &y_in_end);
		if(y_in_end > y_in_start + dst_height - 1)
			y_in_end = y_in_start + dst_height - 1;

		mtk_mdp_rsz_for_taps(y_in_start, y_in_end, t[0].ori_in_len - 1, step, UNIT,
			int_offset[0], sub_offset[0], t[0].ori_out_len - 1, 2, dst_y,
			&y_out_start, &y_out_end, &(int_offset[0]), &(sub_offset[0]));
		y_out_end = y_out_start + dst_height - 1;

		t[0].src_y = y_in_start;
		t[0].step = step;
		t[0].int_offset = int_offset[0];
		t[0].sub_offset = sub_offset[0];
		t[0].in_len = y_in_end - y_in_start + 1;
		t[0].out_len = y_out_end - y_out_start + 1;
		DDPINFO("%s:%s:step:%u,offset:%u.%u,len:%u->%u,yin:%u->%u,dst_y:%u\n", __func__,
			"pUpdate", t[0].step, t[0].int_offset, t[0].sub_offset,
			t[0].in_len, t[0].out_len, y_in_start, y_in_end, t[0].dst_y);
		return 0;
	}

	/* left half */
	if (tile_mode) {
		out_tile_loss[0] =
			(rsz->tile_overhead.is_support ? rsz->tile_overhead.left_out_tile_loss : 0);
		in_tile_loss[0] = out_tile_loss[0] + 4;
		DDPINFO("%s :out_tile_loss[0]:%d, in_tile_loss[0]:%d\n", __func__,
			out_tile_loss[0], in_tile_loss[0]);

		tile_in_len[0] = (((frm_out_len / 2) * frm_in_len * 10) /
			frm_out_len + 5) / 10;
		if (tile_in_len[0] + in_tile_loss[0] >= frm_in_len)
			in_tile_loss[0] = frm_in_len - tile_in_len[0];

		tile_out_len[0] = frm_out_len / 2 + out_tile_loss[0];
		if (tile_in_len[0] + in_tile_loss[0] > tile_out_len[0])
			in_tile_loss[0] = tile_out_len[0] - tile_in_len[0];
		tile_in_len[0] += in_tile_loss[0];
	} else {
		tile_in_len[0] = frm_in_len;
		tile_out_len[0] = frm_out_len;
	}

	t[0].step = step;
	t[0].int_offset = int_offset[0];
	t[0].sub_offset = sub_offset[0];
	t[0].in_len = tile_in_len[0];
	t[0].out_len = tile_out_len[0];

	DDPINFO("%s:%s:step:%u,offset:%u.%u,len:%u->%u, is_hor=%d\n", __func__,
		   tile_mode ? "dual" : "single", t[0].step, t[0].int_offset,
		   t[0].sub_offset, t[0].in_len, t[0].out_len, is_hor);

	if (int_offset[0] < -1)
		DDPINFO("%s :pipe0_scale_err\n", __func__);
	if (!tile_mode)
		return 0;

	/* right half */
	out_tile_loss[1] =
		(rsz->tile_overhead.is_support ? rsz->tile_overhead.right_out_tile_loss : 0);
	in_tile_loss[1] = out_tile_loss[1] + 4;
	DDPINFO("%s :out_tile_loss[1]:%d, in_tile_loss[1]:%d\n", __func__,
		out_tile_loss[1], in_tile_loss[1]);

	tile_out_len[1] = frm_out_len - (tile_out_len[0] - out_tile_loss[0]) + out_tile_loss[1];
	tile_in_len[1] = (((tile_out_len[1] - out_tile_loss[1]) * frm_in_len * 10) /
		frm_out_len + 5) / 10;
	if (tile_in_len[1] + in_tile_loss[1] >= frm_in_len)
		in_tile_loss[1] = frm_in_len - tile_in_len[1];

	if (tile_in_len[1] + in_tile_loss[1] > tile_out_len[1])
		in_tile_loss[1] = tile_out_len[1] - tile_in_len[1];
	tile_in_len[1] += in_tile_loss[1];

	offset[1] = (-offset[0]) + ((tile_out_len[0] - out_tile_loss[0] - out_tile_loss[1]) *
		step) - (frm_in_len - tile_in_len[1]) * UNIT;

	int_offset[1] = offset[1] / UNIT;
	if (offset[1] >= 0)
		sub_offset[1] = offset[1] - UNIT * int_offset[1];
	else
		sub_offset[1] = UNIT * int_offset[1] - offset[1];

	t[1].step = step;
	t[1].int_offset = (u32)(int_offset[1] & 0xffff);
	t[1].sub_offset = (u32)(sub_offset[1] & 0x1fffff);
	t[1].in_len = tile_in_len[1];
	t[1].out_len = tile_out_len[1];

	DDPDBG("%s:%s:step:%u,offset:%u.%u,len:%u->%u\n", __func__,
		   tile_mode ? "dual" : "single", t[1].step, t[1].int_offset,
		   t[1].sub_offset, t[1].in_len, t[1].out_len);

	if (int_offset[1] < -1 || tile_out_len[1] >= frm_out_len)
		DDPINFO("%s :pipe1_scale_err\n", __func__);
	return 0;
}

static int mtk_rsz_check_params(struct mtk_mdp_rsz_config_struct *rsz_config,
				unsigned int tile_length)
{
	if ((rsz_config->frm_in_w != rsz_config->frm_out_w ||
	     rsz_config->frm_in_h != rsz_config->frm_out_h) &&
	    rsz_config->frm_in_w > tile_length) {
		DDPPR_ERR("%s:need rsz but input width(%u) > limit(%u)\n",
			  __func__, rsz_config->frm_in_w, tile_length);
		return -EINVAL;
	}

	return 0;
}

static void mtk_mdp_rsz_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);

	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
	if (rsz->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, FLD_RSZ_BYPASS_SHADOW,
			RSZ_SHADOW_CTRL, FLD_RSZ_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			RSZ_SHADOW_CTRL, FLD_RSZ_BYPASS_SHADOW);
}

static void mtk_mdp_rsz_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_disp_mdp_rsz_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	bool tile_mode = true;
	u32 tile_idx = 0;
	u32 frm_in_len, frm_out_len;
	u32 in_w = 0, out_w = 0;
	u32 left_in_w = 0, right_in_w = 0;
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);

	DDPINFO("%s comp:%s, scaling_en:%d, cfg:(%ux%u)->(%ux%u)\n", __func__,
		mtk_dump_comp_str(comp), comp->mtk_crtc->scaling_ctx.scaling_en,
		cfg->rsz_src_w, cfg->rsz_src_h, cfg->w, cfg->h);

	if (!comp->mtk_crtc->scaling_ctx.scaling_en)
		return;

	if (cfg->tile_overhead.is_support) {

		frm_in_len = cfg->rsz_src_w;
		frm_out_len = cfg->w;

		left_in_w = cfg->rsz_src_w / 2;
		right_in_w =  cfg->rsz_src_w / 2;

		/*set component overhead*/
		if (comp->id == DDP_COMPONENT_MDP_RSZ0) {
			/* copy from post-accumulation */
			rsz->tile_overhead.left_out_tile_loss = cfg->tile_overhead.left_overhead;
			rsz->tile_overhead.is_support = cfg->tile_overhead.is_support;

			mtk_mdp_rsz_calc_tile_params(comp, frm_in_len, frm_out_len,
				tile_mode, rsz->tile_overhead.tw, true);

			tile_idx = 0;
			in_w = rsz->tile_overhead.tw[tile_idx].in_len;
			out_w = rsz->tile_overhead.tw[tile_idx].out_len;

			rsz->tile_overhead.left_comp_overhead = in_w - left_in_w;

			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead =
				rsz->tile_overhead.left_comp_overhead;
			cfg->tile_overhead.left_in_width =
				rsz->tile_overhead.left_comp_overhead + left_in_w;
			cfg->tile_overhead.left_overhead_scaling =
				rsz->tile_overhead.left_out_tile_loss;

			/*copy from total overhead info*/
			rsz->tile_overhead.left_in_width = cfg->tile_overhead.left_in_width;
			rsz->tile_overhead.left_overhead = cfg->tile_overhead.left_overhead;
		}
		if (comp->id == DDP_COMPONENT_MDP_RSZ1) {
			/* copy from post-accumulation */
			rsz->tile_overhead.right_out_tile_loss = cfg->tile_overhead.right_overhead;
			rsz->tile_overhead.is_support = cfg->tile_overhead.is_support;

			mtk_mdp_rsz_calc_tile_params(comp, frm_in_len, frm_out_len,
				tile_mode, rsz->tile_overhead.tw, true);

			tile_idx = 1;
			in_w = rsz->tile_overhead.tw[tile_idx].in_len;
			out_w = rsz->tile_overhead.tw[tile_idx].out_len;

			rsz->tile_overhead.right_comp_overhead = in_w - right_in_w;

			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead =
				rsz->tile_overhead.right_comp_overhead;
			cfg->tile_overhead.right_in_width =
				rsz->tile_overhead.right_comp_overhead + right_in_w;
			cfg->tile_overhead.right_overhead_scaling =
				rsz->tile_overhead.right_out_tile_loss;

			/*copy from total overhead info*/
			rsz->tile_overhead.right_in_width = cfg->tile_overhead.right_in_width;
			rsz->tile_overhead.right_overhead = cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_mdp_rsz_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);

	DDPDBG("line: %d\n", __LINE__);

	if (!comp->mtk_crtc->scaling_ctx.scaling_en)
		return;

	/*set component overhead*/
	rsz->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		rsz->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	rsz->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_mdp_rsz_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_mdp_rsz_config_struct *rsz_config = NULL;
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);
	//enum mtk_rsz_color_format fmt = ARGB2101010;
	bool tile_mode = false;
	u32 reg_val = 0;
	u32 tile_idx = 0;
	u32 in_w = 0, in_h = 0, out_w = 0, out_h = 0;
	unsigned int overhead_v;
	u32 con3 = 0;
	//bool drs_lclip_en;
	//bool drs_padding_dis;
	//bool urs_clip_en;
	if (!mtk_crtc_check_is_scaling_comp(comp->mtk_crtc, comp->id)) {
		DDPINFO("%s only for res switch on ap, return\n", __func__);
		return;
	}
	DDPINFO("%s comp:%s, scaling_en:%d, cfg:(%ux%u)->(%ux%u)\n", __func__,
		mtk_dump_comp_str(comp), comp->mtk_crtc->scaling_ctx.scaling_en,
		cfg->rsz_src_w, cfg->rsz_src_h, cfg->w, cfg->h);

	if (!comp->mtk_crtc->scaling_ctx.scaling_en) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + RSZ_ENABLE, 0x0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + RSZ_INPUT_IMAGE, 0x0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + RSZ_OUTPUT_IMAGE, 0x0, ~0);
		return;
	}

	rsz_config = kzalloc(sizeof(struct mtk_mdp_rsz_config_struct), GFP_KERNEL);
	if (!rsz_config) {
		DDPPR_ERR("fail to create rsz_config!\n");
		return;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		rsz_config->frm_in_w = cfg->rsz_src_w / 2;
		rsz_config->frm_out_w = cfg->w / 2;
	} else {
		rsz_config->frm_in_w = cfg->rsz_src_w;
		rsz_config->frm_out_w = cfg->w;
	}

	rsz_config->frm_in_h = cfg->rsz_src_h;
	rsz_config->frm_out_h = cfg->h;

	if (mtk_rsz_check_params(rsz_config, rsz->data->tile_length)) {
		kfree(rsz_config);
		return;
	}

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (comp->id == DDP_COMPONENT_MDP_RSZ0)
			tile_idx = 0;
		else if (comp->id == DDP_COMPONENT_MDP_RSZ1)
			tile_idx = 1;

		rsz_config->tw[tile_idx].in_len =
			rsz->tile_overhead.tw[tile_idx].in_len;
		rsz_config->tw[tile_idx].out_len =
			rsz->tile_overhead.tw[tile_idx].out_len;
		rsz_config->tw[tile_idx].step =
			rsz->tile_overhead.tw[tile_idx].step;
		rsz_config->tw[tile_idx].int_offset =
			rsz->tile_overhead.tw[tile_idx].int_offset;
		rsz_config->tw[tile_idx].sub_offset =
			rsz->tile_overhead.tw[tile_idx].sub_offset;
	} else {
		/* dual pipe without tile_overhead or single pipe */
		mtk_mdp_rsz_calc_tile_params(comp, rsz_config->frm_in_w, rsz_config->frm_out_w,
					 tile_mode, rsz_config->tw, true);
	}
	rsz_config->th[0].ori_in_len = rsz_config->frm_in_h;
	rsz_config->th[0].ori_out_len = rsz_config->frm_out_h;
	if (rsz->set_partial_update != 1) {
		rsz_config->frm_in_h = cfg->rsz_src_h;
		rsz_config->frm_out_h = cfg->h;
		rsz_config->th[0].par_update_y =  false;
		rsz_config->th[0].src_y = 0;
		rsz_config->th[0].dst_y = 0;
		rsz_config->th[0].overhead_y = 0;
	} else {
		rsz_config->frm_in_h = cfg->rsz_src_h;
		rsz_config->frm_out_h = rsz->roi_height;
		rsz_config->th[0].par_update_y = true;
		rsz_config->th[0].dst_y = rsz->roi_y_offset;
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : rsz->tile_overhead_v.overhead_v;
		rsz_config->th[0].overhead_y = overhead_v;
	}

	mtk_mdp_rsz_calc_tile_params(comp, rsz_config->frm_in_h, rsz_config->frm_out_h,
		tile_mode, rsz_config->th, false);

	in_w = rsz_config->tw[tile_idx].in_len;
	in_h = rsz_config->th[0].in_len;
	out_w = rsz_config->tw[tile_idx].out_len;
	out_h = rsz_config->th[0].out_len;

	if (in_w > out_w || in_h > out_h) {
		DDPPR_ERR("DISP_RSZ only supports scale-up,(%ux%u)->(%ux%u)\n",
			  in_w, in_h, out_w, out_h);
		kfree(rsz_config);
		return;
	}

	comp->mtk_crtc->tile_overhead_v.in_height = in_h;
	comp->mtk_crtc->tile_overhead_v.src_y = rsz_config->th[0].src_y;

	con3 = rsz->tile_overhead.rsz_out.con3;
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_CONTROL, 0x0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_CONTROL_3, con3, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_CONTROL,
			rsz->tile_overhead.rsz_out.etc_ctrl, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SWITCH_MAX_MIN_1,
			rsz->tile_overhead.rsz_out.etc_switch_max_min1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SWITCH_MAX_MIN_2,
			rsz->tile_overhead.rsz_out.etc_switch_max_min2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING,
			rsz->tile_overhead.rsz_out.etc_ring, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING_GAINCON_1,
			rsz->tile_overhead.rsz_out.etc_ring_gaincon1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING_GAINCON_2,
			rsz->tile_overhead.rsz_out.etc_ring_gaincon2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING_GAINCON_3,
			rsz->tile_overhead.rsz_out.etc_ring_gaincon3, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SIM_PROT_GAINCON_1,
			rsz->tile_overhead.rsz_out.etc_sim_port_gaincon1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SIM_PROT_GAINCON_2,
			rsz->tile_overhead.rsz_out.etc_sim_port_gaincon2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SIM_PROT_GAINCON_3,
			rsz->tile_overhead.rsz_out.etc_sim_port_gaincon3, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_BLEND,
			rsz->tile_overhead.rsz_out.etc_blend, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_TAP_ADAPT,
			rsz->tile_overhead.rsz_out.tap_adapt, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_CON_1,
		       rsz->tile_overhead.rsz_out.con1, ~0);
	DDPDBG("%s:CONTROL_1:0x%x\n", __func__, reg_val);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_CON_2,
		       rsz->tile_overhead.rsz_out.con2, ~0);
	DDPDBG("%s:CONTROL_2:0x%x\n", __func__, reg_val);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_INPUT_IMAGE,
		       in_h << 16 | in_w, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_OUTPUT_IMAGE,
		       out_h << 16 | out_w, ~0);
	DDPDBG("%s:%s:(%ux%u)->(%ux%u)\n", __func__, mtk_dump_comp_str(comp),
	       in_w, in_h, out_w, out_h);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_HOR_COEFF_STEP,
		       rsz_config->tw[tile_idx].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_VER_COEFF_STEP,
		       rsz_config->th[0].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       RSZ_LUMA_HOR_INT_OFFSET,
		       rsz_config->tw[tile_idx].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       RSZ_LUMA_HOR_SUB_OFFSET,
		       rsz_config->tw[tile_idx].sub_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       RSZ_LUMA_VER_INT_OFFSET,
		       rsz_config->th[0].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       RSZ_LUMA_VER_SUB_OFFSET,
		       rsz_config->th[0].sub_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_CHROMA_HOR_INT_OFFSET,
		       rsz_config->tw[tile_idx].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_CHROMA_HOR_SUB_OFFSET,
		       rsz_config->tw[tile_idx].sub_offset, ~0);
	kfree(rsz_config);
}

static void mtk_mdp_rsz_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	struct mtk_addon_rsz_config config = addon_config->addon_rsz_config;
	struct mtk_mdp_rsz_config_struct *rsz_config = kzalloc(sizeof(struct mtk_mdp_rsz_config_struct), GFP_KERNEL);
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);
	bool tile_mode = false;
	u32 reg_val = 0;
	u32 tile_idx = 0;
	u32 in_w = 0, in_h = 0, out_w = 0, out_h = 0;
	unsigned int overhead_v;
	u32 con3 = 0;

	if (!rsz_config) {
		DDPPR_ERR("fail to create rsz_config!\n");
		return;
	}

	rsz_config->frm_in_w = config.rsz_src_roi.width;
	rsz_config->frm_in_h = config.rsz_src_roi.height;
	rsz_config->frm_out_w = config.rsz_dst_roi.width;
	rsz_config->frm_out_h = config.rsz_dst_roi.height;

	if (mtk_rsz_check_params(rsz_config, rsz->data->tile_length)) {
		kfree(rsz_config);
		return;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		rsz_config->tw[tile_idx].in_len =
			addon_config->addon_rsz_config.rsz_param.in_len;
		rsz_config->tw[tile_idx].out_len =
			addon_config->addon_rsz_config.rsz_param.out_len;
		rsz_config->tw[tile_idx].step =
			addon_config->addon_rsz_config.rsz_param.step;
		rsz_config->tw[tile_idx].int_offset =
			addon_config->addon_rsz_config.rsz_param.int_offset;
		rsz_config->tw[tile_idx].sub_offset =
			addon_config->addon_rsz_config.rsz_param.sub_offset;
	} else {
		mtk_mdp_rsz_calc_tile_params(comp, rsz_config->frm_in_w, rsz_config->frm_out_w,
					 tile_mode, rsz_config->tw, true);
	}
	rsz_config->th[0].ori_in_len = rsz_config->frm_in_h;
	rsz_config->th[0].ori_out_len = rsz_config->frm_out_h;
	rsz_config->th[0].par_update_y =  false;
	rsz_config->th[0].src_y = 0;
	rsz_config->th[0].dst_y = 0;
	rsz_config->th[0].overhead_y = 0;

	mtk_mdp_rsz_calc_tile_params(comp, rsz_config->frm_in_h, rsz_config->frm_out_h,
				 tile_mode, rsz_config->th, false);
	in_w = rsz_config->tw[tile_idx].in_len;
	in_h = rsz_config->th[0].in_len;
	out_w = rsz_config->tw[tile_idx].out_len;
	out_h = rsz_config->th[0].out_len;

	if (in_w > out_w || in_h > out_h) {
		DDPPR_ERR("DISP_RSZ only supports scale-up,(%ux%u)->(%ux%u)\n",
			  in_w, in_h, out_w, out_h);
		kfree(rsz_config);
		return;
	}

	comp->mtk_crtc->tile_overhead_v.in_height = in_h;
	comp->mtk_crtc->tile_overhead_v.src_y = rsz_config->th[0].src_y;

	//con3 = task->config->info.alpha ? 1 << 10 | rsz_config->fw_out.con3 : 0;
	con3 = rsz->tile_overhead.rsz_out.con3;
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_CONTROL, 0x0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_CONTROL_3, con3, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_CONTROL,
			rsz->tile_overhead.rsz_out.etc_ctrl, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SWITCH_MAX_MIN_1,
			rsz->tile_overhead.rsz_out.etc_switch_max_min1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SWITCH_MAX_MIN_2,
			rsz->tile_overhead.rsz_out.etc_switch_max_min2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING,
			rsz->tile_overhead.rsz_out.etc_ring, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING_GAINCON_1,
			rsz->tile_overhead.rsz_out.etc_ring_gaincon1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING_GAINCON_2,
			rsz->tile_overhead.rsz_out.etc_ring_gaincon2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_RING_GAINCON_3,
			rsz->tile_overhead.rsz_out.etc_ring_gaincon3, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SIM_PROT_GAINCON_1,
			rsz->tile_overhead.rsz_out.etc_sim_port_gaincon1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SIM_PROT_GAINCON_2,
			rsz->tile_overhead.rsz_out.etc_sim_port_gaincon2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_SIM_PROT_GAINCON_3,
			rsz->tile_overhead.rsz_out.etc_sim_port_gaincon3, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ETC_BLEND,
			rsz->tile_overhead.rsz_out.etc_blend, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_TAP_ADAPT,
			rsz->tile_overhead.rsz_out.tap_adapt, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_CON_1,
			   rsz->tile_overhead.rsz_out.con1, ~0);
	DDPDBG("%s:CONTROL_1:0x%x\n", __func__, reg_val);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_CON_2,
			   rsz->tile_overhead.rsz_out.con2, ~0);
	DDPDBG("%s:CONTROL_2:0x%x\n", __func__, reg_val);

	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_INPUT_IMAGE,
			   in_h << 16 | in_w, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_OUTPUT_IMAGE,
			   out_h << 16 | out_w, ~0);
	DDPDBG("%s:%s:(%ux%u)->(%ux%u)\n", __func__, mtk_dump_comp_str(comp),
		   in_w, in_h, out_w, out_h);

	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_HOR_COEFF_STEP,
			   rsz_config->tw[tile_idx].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_VER_COEFF_STEP,
			   rsz_config->th[0].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa +
				   RSZ_LUMA_HOR_INT_OFFSET,
			   rsz_config->tw[tile_idx].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa +
				   RSZ_LUMA_HOR_SUB_OFFSET,
			   rsz_config->tw[tile_idx].sub_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa +
				   RSZ_LUMA_VER_INT_OFFSET,
			   rsz_config->th[0].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa +
				   RSZ_LUMA_VER_SUB_OFFSET,
			   rsz_config->th[0].sub_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_CHROMA_HOR_INT_OFFSET,
			   rsz_config->tw[tile_idx].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_CHROMA_HOR_SUB_OFFSET,
			   rsz_config->tw[tile_idx].sub_offset, ~0);
	kfree(rsz_config);
}

static void mtk_mdp_rsz_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	if ((priv->data->mmsys_id == MMSYS_MT6989 && comp->id == DDP_COMPONENT_RSZ1) ||
		(priv->data->mmsys_id == MMSYS_MT6899) ||
		(priv->data->mmsys_id == MMSYS_MT6991 && comp->id == DDP_COMPONENT_MDP_RSZ1)) {
		cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_ENABLE, 0, ~0);
		return;
	}
	if ((!comp->mtk_crtc->scaling_ctx.scaling_en)
		&& mtk_crtc_check_is_scaling_comp(comp->mtk_crtc, comp->id)) {
		DDPDBG("%s: scaling-up disable, no need to start %s\n", __func__,
			mtk_dump_comp_str(comp));
		return;
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_ENABLE, 0x1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_DEBUG_SEL, 0x3, ~0);
}

static void mtk_mdp_rsz_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + RSZ_ENABLE, 0x0, ~0);
}

static int mtk_mdp_rsz_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);

	switch (io_cmd) {
	case RSZ_GET_TILE_LENGTH: {
		unsigned int *val = (unsigned int *)params;

		if (rsz && rsz->data) {
			*val = rsz->data->tile_length;
			DDPINFO("%s, tile_length[%u]\n", __func__, *val);
		} else
			ret = -1;
		break;
	}
	case RSZ_GET_IN_MAX_HEIGHT: {
		unsigned int *val = (unsigned int *)params;

		if (rsz && rsz->data) {
			*val = rsz->data->in_max_height;
			DDPINFO("%s, in_max_height[%u]\n", __func__, *val);
		} else
			ret = -1;
		break;
	}
	default:
		break;
	}

	return ret;
}

static int mtk_mdp_rsz_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_mdp_rsz *rsz = comp_to_mdp_rsz(comp);
	unsigned int full_in_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, NULL, false);
	unsigned int full_out_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, NULL, true);
	u32 frm_in_h, frm_out_h;
	u32 in_h = 0, out_h = 0;
	struct rsz_tile_params th[1] = {0};
	u32 tile_idx = 0;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	if (!comp->mtk_crtc->scaling_ctx.scaling_en)
		return 0;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	rsz->set_partial_update = enable;
	rsz->roi_height = partial_roi.height;
	rsz->roi_y_offset = partial_roi.y;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : rsz->tile_overhead_v.overhead_v;
	comp_overhead_v = (!overhead_v) ? 0 : rsz->tile_overhead_v.comp_overhead_v;

	DDPDBG("%s, %s overhead_v:%d, comp_overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v, comp_overhead_v);

	//set rsz params
	th[0].ori_in_len = full_in_height;
	th[0].ori_out_len = full_out_height;
	if (rsz->set_partial_update == 1) {
		frm_in_h = full_in_height;
		frm_out_h = rsz->roi_height;
		th[0].par_update_y = true;
		th[0].src_y = 0;
		th[0].dst_y = rsz->roi_y_offset;
		th[0].overhead_y = overhead_v;
	} else{
		frm_in_h = full_in_height;
		frm_out_h = full_out_height;
		th[0].par_update_y = false;
		th[0].src_y = 0;
		th[0].dst_y = 0;
		th[0].overhead_y = 0;
	}

	mtk_mdp_rsz_calc_tile_params(comp, frm_in_h, frm_out_h,
				tile_idx, th, false);

	DDPDBG("%s:%s:in_len:%u,out_len:%u,ori_in_len:%u,ori_out_len:%u\n", __func__,
		th[0].par_update_y ? "pUpdate:on" : "pUpdate:off",
		th[0].in_len, th[0].out_len,
		th[0].ori_in_len, th[0].ori_out_len);

	in_h = th[0].in_len;
	out_h = th[0].out_len;

	if (in_h > out_h) {
		DDPPR_ERR("DISP_RSZ only supports y scale-up,(%u)->(%u)\n",
			in_h, out_h);
		return 0;
	}

	comp->mtk_crtc->tile_overhead_v.in_height = in_h;
	comp->mtk_crtc->tile_overhead_v.src_y = th[0].src_y;

	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_INPUT_IMAGE,
			   in_h << 16, 0xffff0000);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_OUTPUT_IMAGE,
			   out_h << 16, 0xffff0000);

	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_VER_COEFF_STEP,
			   th[0].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_LUMA_VER_INT_OFFSET,
			   th[0].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + RSZ_LUMA_VER_SUB_OFFSET,
			   th[0].sub_offset, ~0);

	return 0;

}

static const struct mtk_ddp_comp_funcs mtk_disp_mdp_rsz_funcs = {
	.start = mtk_mdp_rsz_start,
	.stop = mtk_mdp_rsz_stop,
	.addon_config = mtk_mdp_rsz_addon_config,
	.config = mtk_mdp_rsz_config,
	.config_overhead = mtk_disp_mdp_rsz_config_overhead,
	.config_overhead_v = mtk_disp_mdp_rsz_config_overhead_v,
	.prepare = mtk_mdp_rsz_prepare,
	.unprepare = mtk_mdp_rsz_unprepare,
	.io_cmd = mtk_mdp_rsz_io_cmd,
	.partial_update = mtk_mdp_rsz_set_partial_update,
};

static int mtk_disp_mdp_rsz_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_mdp_rsz *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret = 0;

	DDPFUNC();
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		DDPPR_ERR("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	private->rsz_in_max[0] = priv->data->tile_length;
	private->rsz_in_max[1] = priv->data->in_max_height;

	return 0;
}

static void mtk_disp_mdp_rsz_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_mdp_rsz *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_mdp_rsz_component_ops = {
	.bind = mtk_disp_mdp_rsz_bind,
	.unbind = mtk_disp_mdp_rsz_unbind,
};

static int mtk_disp_mdp_rsz_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_mdp_rsz *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_MDP_RSZ);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_mdp_rsz_funcs);
	if (ret) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_mdp_rsz_component_ops);
	if (ret != 0) {
		DDPPR_ERR("Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_mdp_rsz_remove(struct platform_device *pdev)
{
	struct mtk_disp_mdp_rsz *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_mdp_rsz_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

int mtk_mdp_rsz_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	for (i = 0; i < 3; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 0x10,
			readl(baddr + i * 0x10),
			readl(baddr + i * 0x10 + 0x4),
			readl(baddr + i * 0x10 + 0x8),
			readl(baddr + i * 0x10 + 0xC));
	}
	DDPDUMP("0x030: 0x%08x 0x%08x\n", readl(baddr + 0x30),
		readl(baddr + 0x34));
	for (i = 0; i < 4; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 0x10 + 0x40,
			readl(baddr + i * 0x10 + 0x40),
			readl(baddr + i * 0x10 + 0x4 + 0x40),
			readl(baddr + i * 0x10 + 0x8 + 0x40),
			readl(baddr + i * 0x10 + 0xC + 0x40));
	}
	DDPDUMP("0x084: 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x84),
		readl(baddr + 0x88), readl(baddr + 0x8C));
	DDPDUMP("0x090 0x094: 0x%08x 0x%08x\n", readl(baddr + 0x090),
		readl(baddr + 0x094));
	DDPDUMP("0x0f0 0x0fc: 0x%08x 0x%08x\n", readl(baddr + 0x0f0),
		readl(baddr + 0x0fc));
	for (i = 0; i < 1; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 0x10 + 0x100,
			readl(baddr + i * 0x10 + 0x100),
			readl(baddr + i * 0x10 + 0x4 + 0x100),
			readl(baddr + i * 0x10 + 0x8 + 0x100),
			readl(baddr + i * 0x10 + 0xC + 0x100));
	}
	DDPDUMP("0x110 0x114: 0x%08x 0x%08x\n", readl(baddr + 0x110),
		readl(baddr + 0x114));
	DDPDUMP("0x124 0x128: 0x%08x 0x%08x\n", readl(baddr + 0x124),
		readl(baddr + 0x128));
	DDPDUMP("0x200 0x208 0x20C: 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x200),
		readl(baddr + 0x208), readl(baddr + 0x20C));
	DDPDUMP("0x214 0x218 0x21C: 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x214),
		readl(baddr + 0x218), readl(baddr + 0x21C));
	DDPDUMP("0x224 0x228 0x22C: 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x224),
		readl(baddr + 0x228), readl(baddr + 0x22C));
	for (i = 0; i < 2; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 0x10 + 0x230,
			readl(baddr + i * 0x10 + 0x230),
			readl(baddr + i * 0x10 + 0x4 + 0x230),
			readl(baddr + i * 0x10 + 0x8 + 0x230),
			readl(baddr + i * 0x10 + 0xC + 0x230));
	}
	DDPDUMP("0x250 0x254: 0x%08x 0x%08x\n", readl(baddr + 0x250),
		readl(baddr + 0x254));

	return 0;
}

int mtk_mdp_rsz_analysis(struct mtk_ddp_comp *comp)
{
#define LEN 100
	void __iomem *baddr = comp->regs;
	u32 enable = 0;
	u32 con1 = 0;
	u32 con2 = 0;
	u32 int_flag = 0;
	u32 in_size = 0;
	u32 out_size = 0;
	u32 in_pos = 0;
	u32 shadow = 0;
	char msg[LEN];
	int n = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	enable = readl(baddr + RSZ_ENABLE);
	con1 = readl(baddr + RSZ_CON_1);
	con2 = readl(baddr + RSZ_CON_2);
	int_flag = readl(baddr + RSZ_INT_FLAG);
	in_size = readl(baddr + RSZ_INPUT_IMAGE);
	out_size = readl(baddr + RSZ_OUTPUT_IMAGE);
	in_pos = readl(baddr + RSZ_DEBUG);
	shadow = readl(baddr + RSZ_SHADOW_CTRL);

	DDPDUMP("== DISP %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	writel(0x3, baddr + RSZ_DEBUG_SEL);
	n = snprintf(msg, LEN,
			 "en:%d,rst:%d,h_en:%d,v_en:%d,h_table:%d,v_table:%d,",
			 REG_FLD_VAL_GET(FLD_RSZ_EN, enable),
			 REG_FLD_VAL_GET(FLD_RSZ_RST, enable),
			 REG_FLD_VAL_GET(FLD_RSZ_HORIZONTAL_EN, con1),
			 REG_FLD_VAL_GET(FLD_RSZ_VERTICAL_EN, con1),
			 REG_FLD_VAL_GET(FLD_RSZ_HORIZONTAL_TABLE_SELECT, con1),
			 REG_FLD_VAL_GET(FLD_RSZ_VERTICAL_TABLE_SELECT, con1));
	n += snprintf(msg + n, LEN - n, "dcm_dis:%d,int_en:%d,wclr_en:%d\n",
			  REG_FLD_VAL_GET(FLD_RSZ_DCM_DIS, con1),
			  REG_FLD_VAL_GET(FLD_RSZ_INTEN, con1),
			  REG_FLD_VAL_GET(FLD_RSZ_INT_WCLR_EN, con1));
	DDPDUMP("%s", msg);

	n = snprintf(msg, LEN,
			 "power_saving:%d,rgb_bit_mode:%d,frm_start:%d,frm_end:%d,",
			 REG_FLD_VAL_GET(FLD_RSZ_POWER_SAVING, con2),
			 REG_FLD_VAL_GET(FLD_RSZ_RGB_BIT_MODE, con2),
			 REG_FLD_VAL_GET(FLD_RSZ_FRAME_START, int_flag),
			 REG_FLD_VAL_GET(FLD_RSZ_FRAME_END, int_flag));
	n += snprintf(msg + n, LEN - n, "size_err:%d,sof_rst:%d\n",
			  REG_FLD_VAL_GET(FLD_RSZ_SIZE_ERR, int_flag),
			  REG_FLD_VAL_GET(FLD_RSZ_SOF_RESET, int_flag));
	DDPDUMP("%s", msg);

	n = snprintf(msg, LEN, "in(%ux%u),out(%ux%u),h_step:%d,v_step:%d\n",
			 REG_FLD_VAL_GET(FLD_RSZ_INPUT_IMAGE_W, in_size),
			 REG_FLD_VAL_GET(FLD_RSZ_INPUT_IMAGE_H, in_size),
			 REG_FLD_VAL_GET(FLD_RSZ_OUTPUT_IMAGE_W, out_size),
			 REG_FLD_VAL_GET(FLD_RSZ_OUTPUT_IMAGE_H, out_size),
			 readl(baddr + RSZ_HOR_COEFF_STEP),
			 readl(baddr + RSZ_VER_COEFF_STEP));
	if (n >= 0)
		DDPDUMP("%s", msg);

	n = snprintf(
		msg, LEN, "luma_h:%d.%d,luma_v:%d.%d,chroma_h:%d.%d\n",
		readl(baddr + RSZ_LUMA_HOR_INT_OFFSET),
		readl(baddr + RSZ_LUMA_HOR_SUB_OFFSET),
		readl(baddr + RSZ_LUMA_VER_INT_OFFSET),
		readl(baddr + RSZ_LUMA_VER_SUB_OFFSET),
		readl(baddr + RSZ_CHROMA_HOR_INT_OFFSET),
		readl(baddr + RSZ_CHROMA_HOR_SUB_OFFSET));
	if (n >= 0)
		DDPDUMP("%s", msg);

	n = snprintf(msg, LEN,
			 "dbg_sel:%d, in(%u,%u);shadow_ctrl:bypass:%d,force:%d,",
			 readl(baddr + RSZ_DEBUG_SEL), in_pos & 0xFFFF,
			 (in_pos >> 16) & 0xFFFF,
			 REG_FLD_VAL_GET(FLD_RSZ_BYPASS_SHADOW, shadow),
			 REG_FLD_VAL_GET(FLD_RSZ_FORCE_COMMIT, shadow));
	n += snprintf(msg + n, LEN - n, "read_working:%d\n",
			  REG_FLD_VAL_GET(FLD_RSZ_READ_WRK_REG, shadow));
	DDPDUMP("%s", msg);

	writel(0x7, baddr + RSZ_DEBUG_SEL);
	n = snprintf(msg, LEN - n, "in_total_data_cnt:%d, TAP_ADAPT:%x, ETC_BLEND:%x",
		readl(baddr + RSZ_DEBUG), readl(baddr + RSZ_TAP_ADAPT),
		readl(baddr + RSZ_ETC_BLEND));

	writel(0xB, baddr + RSZ_DEBUG_SEL);
	n += snprintf(msg + n, LEN - n, ",out_total_data_cnt:%d\n",
			  readl(baddr + RSZ_DEBUG));
	DDPDUMP("%s", msg);

	return 0;
}

static const struct mtk_disp_mdp_rsz_data mt6985_mdp_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_mdp_rsz_data mt6897_mdp_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_mdp_rsz_data mt6989_mdp_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_mdp_rsz_data mt6899_mdp_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_mdp_rsz_data mt6991_mdp_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_mdp_rsz_driver_dt_match[] = {
	{.compatible = "mediatek,mt6985-disp-mdp-rsz",
	 .data = &mt6985_mdp_rsz_driver_data},
	{.compatible = "mediatek,mt6897-disp-mdp-rsz",
	 .data = &mt6897_mdp_rsz_driver_data},
	{.compatible = "mediatek,mt6989-disp-mdp-rsz",
	 .data = &mt6989_mdp_rsz_driver_data},
	{.compatible = "mediatek,mt6899-disp-mdp-rsz",
	 .data = &mt6899_mdp_rsz_driver_data},
	{.compatible = "mediatek,mt6991-disp-mdp-rsz",
	 .data = &mt6991_mdp_rsz_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_mdp_rsz_driver_dt_match);

struct platform_driver mtk_disp_mdp_rsz_driver = {
	.probe = mtk_disp_mdp_rsz_probe,
	.remove = mtk_disp_mdp_rsz_remove,
	.driver = {
			.name = "mediatek-disp-mdp-rsz",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_mdp_rsz_driver_dt_match,
		},
};
