/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2022 MediaTek Inc. */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov13a10mipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include "ov13a10mipiraw_Sensor.h"
#include "adaptor-subdrv.h"
static u16 get_gain2reg(u32 gain);
static int init_ctx(struct subdrv_ctx *ctx, struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int ov13a10_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void ov13a10_set_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int ov13a10_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u32 ov13a10_dgain2reg(struct subdrv_ctx *ctx, u32 dgain);
static int ov13a10_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_STREAMING_RESUME, ov13a10_set_streaming_resume},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, ov13a10_set_streaming_suspend},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, ov13a10_set_multi_dig_gain},
	{SENSOR_FEATURE_SET_ESHUTTER, ov13a10_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, ov13a10_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, ov13a10_set_multi_shutter_frame_length},
};


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};


static struct subdrv_mode_struct mode_struct[] = {
	/* frame_desc_prev */
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_cap */
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_vid */
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_hs_vid */
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_slim_vid */
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_cus1 */
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_cus2 */
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_cus3 */
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_cus4 */
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
	/* frame_desc_cus5 */
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = ov13a10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 740,
		.framelength = 3244,
		.max_framerate = 300,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4128,
			.full_h = 3096,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4128,
			.h0_size = 3096,
			.scale_w = 4128,
			.scale_h = 3096,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 3,
		.csi_param = {
			.dphy_trail = 98, /* Fill correct Ths-trail (in ns unit) value */
		},
		.dpc_enabled = true,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = OV13A10_SENSOR_ID,
	.reg_addr_sensor_id = {0x0716, 0x0717},
	.i2c_addr_table = {0x40, 0x42, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = 0,
	.resolution = {4128, 3096},
	.mirror = IMAGE_NORMAL,

	.mclk = 26,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_CSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 1,
	.ana_gain_step = 64,
	.ana_gain_table = ov13a10_ana_gain_table,
	.ana_gain_table_size = sizeof(ov13a10_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 8,
	.exposure_max = 0xFFFF,
	.exposure_step = 2,
	.exposure_margin = 24,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = 16383,
	.dig_gain_step = 1,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 5500000,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = PARAM_UNDEFINED,
	.s_cali = PARAM_UNDEFINED,

	.reg_addr_stream = 0x0b00,
	.reg_addr_mirror_flip = 0x0c34,
	.reg_addr_exposure = {{0x3501, 0x3502},},

	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x3508, 0x3509},},
	.reg_addr_dig_gain = {{0x350A, 0x350B, 0x350C},},
	.reg_addr_frame_length = {0x0211, 0x020E, 0x020F},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,
	.reg_addr_fast_mode = PARAM_UNDEFINED,

	.init_setting_table = ov13a10_init_setting,
	.init_setting_len = ARRAY_SIZE(ov13a10_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.checksum_value = 0xd086e5a5,
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 1000},
	{HW_ID_DVDD, {1}, 1000},
	{HW_ID_AVDD, {1}, 0},
	{HW_ID_DOVDD, {1800000,1800000}, 0},
	{HW_ID_AFVDD, {1}, 1000},
	{HW_ID_MCLK, {26}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 1000},
	{HW_ID_RST, {1}, 2000},
};

const struct subdrv_entry ov13a10_mipi_raw_entry = {
	.name = "ov13a10_mipi_raw",
	.id = OV13A10_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static u16 get_gain2reg(u32 gain)
{
	if (gain > BASEGAIN * 15.5)
		gain = BASEGAIN * 15.5;
	else if (gain < BASEGAIN * 1)
		gain = BASEGAIN * 1;
	else if (gain <= BASEGAIN * 2)
		gain = gain & 0xFFC0;
	else if (gain <= BASEGAIN * 4)
		gain = gain & 0xFF80;
	else if (gain <= BASEGAIN * 8)
		gain = gain & 0xFF00;
	else
		gain = gain & 0xFE00;

	return gain * 256 / BASEGAIN;
}

static int init_ctx(struct subdrv_ctx *ctx, struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	return 0;
}

static u32 ov13a10_dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	u32 ret = dgain << 6;

	DRV_LOG(ctx, "dgain reg = 0x06%x\n", ret);

	return ret;
}

static int ov13a10_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 *gains = (u32 *)(*feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));

	int i = 0;
	u32 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_dig_gain_in_lut(ctx, gains, exp_cnt);
		return ERROR_NONE;
	}
	/* skip if no porting digital gain */
	if (!ctx->s_ctx.reg_addr_dig_gain[0].addr[0])
		return ERROR_NONE;

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = ov13a10_dgain2reg(ctx, gains[i]);
	}

	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain */
	switch (exp_cnt) {
	case 1:
		rg_gains[0] = gains[0];
		break;
	case 2:
		rg_gains[0] = gains[0];
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		break;
	}
	for (i = 0;
	     (i < ARRAY_SIZE(rg_gains)) && (i < ARRAY_SIZE(ctx->s_ctx.reg_addr_dig_gain));
	     i++) {
		if (!rg_gains[i])
			continue; /* skip zero gain setting */

		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[0],
				(rg_gains[i] >> 16) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				(rg_gains[i] >> 8) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[2]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[2],
				rg_gains[i] & 0xC0);
		}
	}

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}

	DRV_LOG(ctx, "dgain reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n",
		rg_gains[0], rg_gains[1], rg_gains[2]);

	return ERROR_NONE;
}

static void ov13a10_set_streaming_control(struct subdrv_ctx *ctx, bool enable)
{
	u64 stream_ctrl_delay_timing = 0;
	u64 stream_ctrl_delay = 0;
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	DRV_LOG(ctx, "E! enable:%u\n", enable);

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (ctx->ixc_client.protocol)
		sd = adaptor_ixc_get_clientdata(&ctx->ixc_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx) {
		DRV_LOGE(ctx, "null _adaptor_ctx\n");
		return;
	}

	check_current_scenario_id_bound(ctx);
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.streaming_ctrl_imp) {
		if (ctx->s_ctx.s_streaming_control != NULL)
			ctx->s_ctx.s_streaming_control((void *) ctx, enable);
		else
			DRV_LOG_MUST(ctx,
				"please implement drive own streaming control!(sid:%u)\n",
				ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
		DRV_LOG_MUST(ctx,
			"stream ctrl implement on scp side!(sid:%u)\n",
			ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}

	if (enable) {
		/* MCSS low power mode update para */
		if (ctx->s_ctx.mcss_update_subdrv_para != NULL)
			ctx->s_ctx.mcss_update_subdrv_para((void *) ctx, ctx->current_scenario_id);
		/* MCSS register init */
		if (ctx->s_ctx.mcss_init != NULL)
			ctx->s_ctx.mcss_init((void *) ctx);

		set_dummy(ctx);
		subdrv_ixc_wr_u16(ctx, ctx->s_ctx.reg_addr_stream, 0x0100);
		ctx->stream_ctrl_start_time = ktime_get_boottime_ns();
		ctx->stream_ctrl_start_time_mono = ktime_get_ns();
	} else {
		ctx->stream_ctrl_end_time = ktime_get_boottime_ns();
		if (ctx->s_ctx.custom_stream_ctrl_delay &&
			ctx->stream_ctrl_start_time && ctx->stream_ctrl_end_time) {
			stream_ctrl_delay_timing =
				(ctx->stream_ctrl_end_time - ctx->stream_ctrl_start_time) / 1000000;
			stream_ctrl_delay = (u64)get_sof_timeout(_adaptor_ctx, _adaptor_ctx->cur_mode) / 1000;
			DRV_LOG_MUST(ctx,
				"stream_ctrl_delay(sof)/stream_ctrl_delay_timing(end-start):%llums/%llums\n",
				stream_ctrl_delay,
				stream_ctrl_delay_timing);
			if (stream_ctrl_delay_timing < stream_ctrl_delay)
				mdelay(stream_ctrl_delay - stream_ctrl_delay_timing);
		}
		subdrv_ixc_wr_u16(ctx, ctx->s_ctx.reg_addr_stream, 0x0000);
		if (ctx->s_ctx.reg_addr_fast_mode && ctx->fast_mode_on) {
			DRV_LOG(ctx, "seamless_switch disabled.");
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
			commit_i2c_buffer(ctx);
		}
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
		ctx->autoflicker_en = FALSE;
		ctx->extend_frame_length_en = 0;
		ctx->is_seamless = 0;
		if (ctx->s_ctx.chk_s_off_end)
			check_stream_off(ctx);
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;
		ctx->stream_ctrl_start_time_mono = 0;

		ctx->mcss_init_info.enable_mcss = 0;
		if (ctx->s_ctx.mcss_init != NULL)
			ctx->s_ctx.mcss_init((void *) ctx); /* disable MCSS */
	}
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG(ctx, "X! enable:%u\n", enable);
}

static int ov13a10_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;

	if (*feature_data) {
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae shutter control!\n");
		else {
			*(feature_data + 1) = 0;
			ov13a10_set_shutter_frame_length(ctx, para, len);
		}
	}

	ov13a10_set_streaming_control(ctx, TRUE);
	return 0;
}
static int ov13a10_set_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	ov13a10_set_streaming_control(ctx, FALSE);
	return 0;
}

static int ov13a10_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;

	if (ctx->s_ctx.aov_sensor_support &&
		ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
		(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
			IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
		DRV_LOG_MUST(ctx,
			"AOV mode not support ae shutter control!\n");
	else {
		*(feature_data + 1) = 0;
		return ov13a10_set_shutter_frame_length(ctx, para, len);
	}

	return ERROR_NONE;
}
static int ov13a10_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 shutter = *feature_data;
	u32 frame_length = *(feature_data + 1);
	u32 cust_rg_shutters = 0;

	int fine_integ_line = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	check_current_scenario_id_bound(ctx);
	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].min);
	shutter = min_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].max);
	/* check boundary of framelength */
	ctx->frame_length = max(
		(u32)shutter + ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin,
			ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = (u32) shutter;
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	else if (ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->min_frame_length);
	/* write shutter */
	set_long_exposure(ctx);
	/* exposure line divide by 2 */
	cust_rg_shutters = ctx->exposure[0] >> 1;
	if (ctx->s_ctx.reg_addr_exposure[0].addr[2]) {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(cust_rg_shutters >> 16) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			(cust_rg_shutters >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[2],
			cust_rg_shutters & 0xFF);
	} else {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(cust_rg_shutters >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			cust_rg_shutters & 0xFF);
	}
	DRV_LOG(ctx, "exp[0x%x(0x%x)], fll(input/output):%u/%u, flick_en:%d\n",
		ctx->exposure[0], cust_rg_shutters, frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */

	return ERROR_NONE;
}
static int ov13a10_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 *shutters = (u64 *)(*feature_data);
	u16 exp_cnt = *(feature_data + 1);
	u16 frame_length = *(feature_data + 2);
	u32 cust_rg_shutters[3] = {0};

	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[4] = {0};
	int readout_diff = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;
	u32 fll = 0, fll_temp = 0, s_fll;

	fll = frame_length ? frame_length : ctx->min_frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = roundup(shutters[i], cit_step);
	}

	/* check boundary of framelength */
	/* - (1) previous se + previous me + current le */
	calc_fl[0] = (u32) shutters[0];
	for (i = 1; i < last_exp_cnt; i++)
		calc_fl[0] += ctx->exposure[i];
	calc_fl[0] += ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin*exp_cnt*exp_cnt;

	/* - (2) current se + current me + current le */
	calc_fl[1] = (u32) shutters[0];
	for (i = 1; i < exp_cnt; i++)
		calc_fl[1] += (u32) shutters[i];
	calc_fl[1] += ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin*exp_cnt*exp_cnt;

	/* - (3) readout time cannot be overlapped */
	calc_fl[2] =
		(ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
		ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
	if (last_exp_cnt == exp_cnt)
		for (i = 1; i < exp_cnt; i++) {
			readout_diff = ctx->exposure[i] - (u32) shutters[i];
			calc_fl[2] += readout_diff > 0 ? readout_diff : 0;
		}
	/* - (4) For DOL (non-FDOL), N-th frame SE and N+1-th frame LE readout cannot be overlapped */
	if ((ctx->s_ctx.hdr_type & HDR_SUPPORT_STAGGER_DOL) &&
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER) {
		for (i = 1; i < last_exp_cnt; i++)
			calc_fl[3] += ctx->exposure[i];
		calc_fl[3] += ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin*exp_cnt*(exp_cnt-1);
		calc_fl[3] += ctx->readout_length + ctx->min_vblanking_line;
		DRV_LOG(ctx,
			"calc_fl[3]: %u, pre-LE/ME/SE (%u/%u/%u), cur-LE/ME/SE (%llu/%llu/%llu), readout_length:%u, min_vblanking_line:%u\n",
			calc_fl[3],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2],
			shutters[0], shutters[1], shutters[2],
			ctx->readout_length,
			ctx->min_vblanking_line);
	}

	for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
		fll = max(fll, calc_fl[i]);
	fll =	max(fll, ctx->min_frame_length);
	fll =	min(fll, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	for (i = 0; i < exp_cnt; i++)
		ctx->exposure[i] = (u32) shutters[i];
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].sw_fl_delay) {
		fll_temp = ctx->frame_length_next;
		ctx->frame_length_next = fll;
		s_fll = calc_fl[0];
		for (i = 1; i < ARRAY_SIZE(calc_fl); i++)
			s_fll = max(s_fll, calc_fl[i]);
		if (s_fll > frame_length) {
			fll = s_fll;
			fll = max(fll, ctx->min_frame_length);
			fll = min(fll, ctx->s_ctx.frame_length_max);
			if(fll<fll_temp)
				fll = fll_temp;
		} else {
			if (s_fll > fll_temp)
				fll = s_fll;
			else
				fll = fll_temp;
		}
		DRV_LOG(ctx, "fll:%u, s_fll:%u, fll_temp:%u, frame_length:%u\n",
			fll, s_fll, fll_temp, frame_length);
	}
	ctx->frame_length = fll;
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	else if (ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->min_frame_length);
	/* write shutter */
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		break;
	case 2:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[2] = (u32) shutters[1] / exp_cnt;
		break;
	case 3:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[1] = (u32) shutters[1] / exp_cnt;
		rg_shutters[2] = (u32) shutters[2] / exp_cnt;
		break;
	default:
		break;
	}
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
		ctx->l_shift = 0;
	}
	for (i = 0; i < 3; i++) {
		if (rg_shutters[i]) {
			/* exposure line divide by 2 */
			cust_rg_shutters[i] = rg_shutters[i] >> 1;
			if (ctx->s_ctx.reg_addr_exposure[i].addr[2]) {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(cust_rg_shutters[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					(cust_rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[2],
					cust_rg_shutters[i] & 0xFF);
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(cust_rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					cust_rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG(ctx, "exp[0x%x(0x%x)/0x%x(0x%x)/0x%x(0x%x)], fll(input/output):%u/%u, flick_en:%d\n",
		rg_shutters[0], cust_rg_shutters[0], rg_shutters[1], cust_rg_shutters[1],
		rg_shutters[2], cust_rg_shutters[2], frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */

	return ERROR_NONE;
}
