/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_MDP_RSZ_H__
#define __MTK_DISP_MDP_RSZ_H__

#define RSZ_6TAP_STEPCOUNTER_BIT 15
#define RSZ_PREC_SHIFT 20
#define RSZ_RATIO_SHIFT 10

struct mtk_disp_mdp_rsz_data {
	unsigned int tile_length;
	unsigned int in_max_height;
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mdp_rect {
	uint32_t left;
	uint32_t top;
	uint32_t width;
	uint32_t height;
};

struct mdp_crop {
	struct mdp_rect r;
	uint32_t x_sub_px;
	uint32_t y_sub_px;
	uint32_t w_sub_px;
	uint32_t h_sub_px;
};

struct rsz_fw_in {
	u32 in_width;
	u32 in_height;
	u32 out_width;
	u32 out_height;
	struct mdp_crop crop;
	bool power_saving;
	bool use121filter;
	bool drs_lclip_en;
	bool drs_padding_dis;
	bool urs_clip_en;
	bool alpha_enable;
	bool r2y_enable;
	bool y2r_enable;
};

struct rsz_fw_out {
	u32 hori_step;
	u32 vert_step;
	u32 precision_x;
	u32 precision_y;
	u32 hori_int_ofst;
	u32 hori_sub_ofst;
	u32 vert_int_ofst;
	u32 vert_sub_ofst;
	u32 hori_scale;
	u32 hori_algo;
	u32 vert_scale;
	u32 vert_algo;
	u32 vert_first;
	u32 vert_cubic_trunc;
	u32 con1;
	u32 con2;
	u32 con3;
	u32 tap_adapt;
	u32 etc_ctrl;
	u32 etc_switch_max_min1;
	u32 etc_switch_max_min2;
	u32 etc_ring;
	u32 etc_ring_gaincon1;
	u32 etc_ring_gaincon2;
	u32 etc_ring_gaincon3;
	u32 etc_sim_port_gaincon1;
	u32 etc_sim_port_gaincon2;
	u32 etc_sim_port_gaincon3;
	u32 etc_blend;
};

struct rsz_cal_param {
	u32 yuv_422_t_yuv_444;
	s32 hori_luma_int_ofst;
	s32 hori_luma_sub_ofst;
	s32 vert_luma_int_ofst;
	s32 vert_luma_sub_ofst;
	bool int_wclr_en;
	bool tap_adapt_en;
	s32 tap_adapt_slope;
	u32 tap_adapt_fallback_ratio;
	u32 tap_adapt_var_coring;
	u32 tap_adapt_dc_coring;
	u32 tap_adapt_edge_thr;
	u32 signal_enhance_mode;
	u32 hori_tbl;
	u32 vert_tbl;
	u32 hori_alpha_tbl;
	u32 vert_alpha_tbl;
	bool hori_cubic_trunc_en;
	u32 hori_luma_cubic_trunc_bit;
	u32 hori_chroma_cubic_trunc_bit;
	u32 vert_luma_cubic_trunc_bit;
	u32 vert_chroma_cubic_trunc_bit;
	s32 hori_trunc_bit;
	s32 vert_trunc_bit;
};

#endif
