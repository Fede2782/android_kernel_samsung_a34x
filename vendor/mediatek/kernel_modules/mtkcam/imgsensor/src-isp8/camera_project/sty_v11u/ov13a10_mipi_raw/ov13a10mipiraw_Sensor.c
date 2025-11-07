// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2025 Samsung Electronics Inc.

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
#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI_V2
#include "ov13a10_Sensor_adaptive_mipi.h"
#endif

static u16 get_gain2reg(u32 gain);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int ov13a10_close(struct subdrv_ctx *ctx);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static int ov13a10_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void ov13a10_set_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int ov13a10_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u32 ov13a10_dgain2reg(struct subdrv_ctx *ctx, u32 dgain);
static int ov13a10_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int ov13a10_open(struct subdrv_ctx *ctx);
static int ov13a10_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_night_hyperlapse(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov13a10_set_night_hyperlapse_sync_flag(struct subdrv_ctx *ctx, u8 *para, u32 *len);

static int sNightHyperlapseSpeed;
static int sNightHyperlapseCnt;
static bool night_hyperlapse_sync_flag = false;
static bool using_long_exp;

#define FRAME_SKIP_FOR_NIGHT_HYPERLAPSE (1)

#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI_V2
static int set_mipi_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id, struct subdrv_ctx *ctx);
static int ov13a10_get_mipi_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int update_mipi_info(enum SENSOR_SCENARIO_ID_ENUM scenario_id);
static void ov13a10_set_mipi_pixel_rate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *mipi_pixel_rate);
static u32 ov13a10_get_custom_mipi_pixel_rate(enum SENSOR_SCENARIO_ID_ENUM scenario_id);

static int adaptive_mipi_index = -1;
#endif

/* STRUCT */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0, //refer PD_OFFSET_X in ini file
	.i4OffsetY = 12, //refer PD_OFFSET_Y in ini file
	.i4PitchX = 32, //refer PD_PITCH_X in ini file
	.i4PitchY = 32, //refer PD_PITCH_Y in ini file
	.i4PairNum = 8, //(PitchY/Densityy_Y)*(PitchX/Densityy_X)
	.i4SubBlkW = 16, //refer PD_DENSITY_X in ini file
	.i4SubBlkH = 8, //refer PD_DENSITY_Y in ini file
	.i4PosL = {{14, 18}, {30, 18}, {6, 22}, {22, 22},
				{14, 34}, {30, 34}, {6, 38}, {22, 38}},
	.i4PosR = {{14, 14}, {30, 14}, {6, 26}, {22, 26},
				{14, 30}, {30, 30}, {6, 42}, {22, 42}},
	.i4BlockNumX = 129, //refer PD_BLOCK_NUM_X in ini file
	.i4BlockNumY = 96, //refer PD_BLOCK_NUM_Y in ini file
	.i4LeFirst = 0,
	.i4Crop = {
		// <prev> <cap> <vid> <hs_vid> <slim_vid>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		// <cust1> <<cust2>> <<cust3>>
		{0, 0}, {0, 0}, {0, 0},
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 4128,//RAW_WIDTH
	.i4FullRawH = 3096,//RAW_HEIGHT
	.i4ModeIndex = 0, //refer PD_MODE_INDEX in ini file

	.sPDMapInfo[0] = {
	  .i4VCFeature = 0,
		.i4PDPattern = 3, //non-intereleaved for vc format
		.i4PDRepetition = 4, //repition 4 (R L L R)
		.i4PDOrder = {1, 0, 0, 1}, // R = 1, L = 0
	},
};

static struct SET_PD_BLOCK_INFO_T  imgsensor_pd_info_16_9  = {
	.i4OffsetX = 0, //refer PD_OFFSET_X in ini file
	.i4OffsetY = 10, //refer PD_OFFSET_Y in ini file
	.i4PitchX = 32, //refer PD_PITCH_X in ini file
	.i4PitchY = 32, //refer PD_PITCH_Y in ini file
	.i4PairNum = 8, //(PitchY/Densityy_Y)*(PitchX/Densityy_X)
	.i4SubBlkW = 16, //refer PD_DENSITY_X in ini file
	.i4SubBlkH = 8, //refer PD_DENSITY_Y in ini file
	.i4PosL = {{14, 16}, {30, 16}, {6, 20}, {22, 20},
	            {14, 32}, {30, 32}, {6, 36}, {22, 36}},
	.i4PosR = {{14, 12}, {30, 12}, {6, 24}, {22, 24},
	            {14, 28}, {30, 28}, {6, 40}, {22, 40}},
	.i4BlockNumX = 129, //refer PD_BLOCK_NUM_X in ini file
	.i4BlockNumY = 72, //refer PD_BLOCK_NUM_Y in ini file
	.i4LeFirst = 0,
	.i4Crop = {
		// <prev> <cap> <vid> <hs_vid> <slim_vid>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		// <cust1> <<cust2>> <<cust3>>
		{0, 0}, {0, 0}, {0, 0},
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 4128,//RAW_WIDTH
	.i4FullRawH = 2324,//RAW_HEIGHT
	.i4ModeIndex = 0, //refer PD_MODE_INDEX in ini file

	.sPDMapInfo[0] = {
	  .i4VCFeature = 0,
		.i4PDPattern = 3, //non-intereleaved for vc format
		.i4PDRepetition = 4, //repition 4 (R L L R)
		.i4PDOrder = {1, 0, 0, 1}, // R = 1, L = 0
	},
};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_STREAMING_RESUME, ov13a10_set_streaming_resume},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, ov13a10_set_streaming_suspend},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, ov13a10_set_multi_dig_gain},
	{SENSOR_FEATURE_SET_TEST_PATTERN, ov13a10_set_test_pattern},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, ov13a10_check_sensor_id},
	{SENSOR_FEATURE_SET_ESHUTTER, ov13a10_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, ov13a10_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, ov13a10_set_multi_shutter_frame_length},
	{SENSOR_FEATURE_SET_NIGHT_HYPERLAPSE_MODE, ov13a10_set_night_hyperlapse},
	{SENSOR_FEATURE_SET_NIGHT_HYPERLAPSE_SYNC, ov13a10_set_night_hyperlapse_sync_flag},
#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI_V2
	{SENSOR_FEATURE_GET_MIPI_PIXEL_RATE, ov13a10_get_mipi_pixel_rate},
#endif
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020, /* 4128 */
			.vsize = 0x0C18, /* 3096 */
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
			.hsize = 0x1020, /* 4128 */
			.vsize = 0x0C18, /* 3096 */
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
			.hsize = 0x1020, /* 4128 */
			.vsize = 0x0914, /* 2324 */
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
			.hsize = 0x03E0, //992
			.vsize = 0x02E8, //744
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
	// frame_desc_prev
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000, /* 1195.2 Mbps */
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cap
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000, /* 1195.2 Mbps */
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_vid
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = ov13a10_video_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000, /* 1195.2 Mbps */
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 406,
			.w0_size = 4128,
			.h0_size = 2324,
			.scale_w = 4128,
			.scale_h = 2324,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4128,
			.h1_size = 2324,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 2324,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_16_9,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_hs_vid - not used mode
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_slim_vid - not used mode
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus1
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = ov13a10_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(ov13a10_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 112000000,
		.linelength = 572,
		.framelength = 1631,
		.max_framerate = 1200,
		.mipi_pixel_rate = 478080000, /* 1195.2 Mbps */
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 128,
			.y0_offset = 80,
			.w0_size = 3968,
			.h0_size = 2976,
			.scale_w = 992, /* 2x2 binnig */
			.scale_h = 744,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 992,
			.h1_size = 744,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 992,
			.h2_tg_size = 744,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000, // by IQ request, actual 2x2 binning
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus2 - not used mode
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus3 - not used mode
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus4 - not used mode
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus5 - not used mode
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
		.pclk = 112000000,
		.linelength = 582,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 478080000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4224,
			.full_h = 3136,
			.x0_offset = 48,
			.y0_offset = 20,
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
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 82, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = OV13A10_SENSOR_ID,
	.reg_addr_sensor_id = {0xFF, 0xFF}, /* not used */
	.i2c_addr_table = {0x6C, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = 0,
	.resolution = {4128, 3096},
	.mirror = IMAGE_NORMAL,

	.mclk = 19,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_CSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 1, /* OV */
	.ana_gain_step = 64,
	.ana_gain_table = ov13a10_ana_gain_table,
	.ana_gain_table_size = sizeof(ov13a10_ana_gain_table),
	.tuning_iso_base = 35,
	.exposure_def = 0x3D0,
	.exposure_min = 8,
	.exposure_max = 0xFFE7, /* 0xFFFF - 24(exposure margin)*/
	.exposure_step = 2,
	.exposure_margin = 24,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = 16383,
	.dig_gain_step = 1,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 700000,

	.pdaf_type = PDAF_SUPPORT_RAW,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = PARAM_UNDEFINED,
	.s_cali = PARAM_UNDEFINED,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, /* not used */
	.reg_addr_exposure = {{0x3501, 0x3502},},

	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x3508, 0x3509},},
	.reg_addr_dig_gain = {{0x350A, 0x350B, 0x350C},},
	.reg_addr_frame_length = {0x380E, 0x380F},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,
	.reg_addr_fast_mode = PARAM_UNDEFINED,
	.chk_s_off_end = 1,

	.init_setting_table = ov13a10_init_setting,
	.init_setting_len = ARRAY_SIZE(ov13a10_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.checksum_value = 0xd086e5a5,
};

static struct subdrv_ops ops = {
	.get_id = ov13a10_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = ov13a10_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = ov13a10_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 1000},
	{HW_ID_AFVDD, {2800000, 2800000}, 0},
	{HW_ID_AVDD, {1}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 0},
	{HW_ID_DVDD, {1}, 1000},
	{HW_ID_RST, {1}, 5000},
	{HW_ID_MCLK, {19}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 1000},
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

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int ov13a10_close(struct subdrv_ctx *ctx)
{
	DRV_LOG_MUST(ctx, "ov13a10 closed\n");
	if(sNightHyperlapseSpeed){
		sNightHyperlapseSpeed = 0;
	}
	sNightHyperlapseCnt = 0;
	using_long_exp = false;
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
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

static void ov13a10_set_dgain_reg(struct subdrv_ctx *ctx, u8 data1, u8 data2, u8 data3)
{
	subdrv_i2c_wr_u8(ctx, 0x350A, data1);
	subdrv_i2c_wr_u8(ctx, 0x350B, data2);
	subdrv_i2c_wr_u8(ctx, 0x350C, data3);
	DRV_LOG(ctx, "set dgain register: %#x, %#x, %#x\n", data1, data2, data3);
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
	if (!ctx->s_ctx.reg_addr_dig_gain[0].addr[0]) {
		ov13a10_set_dgain_reg(ctx, 0x00, 0x00, 0x00);
		DRV_LOG(ctx, "dgain disable\n");
		return ERROR_NONE;
	}

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

		if (!ctx->s_ctx.reg_addr_dig_gain[i].addr[0])
			ov13a10_set_dgain_reg(ctx, 0x00, 0x00, 0x00);
		else
			ov13a10_set_dgain_reg(ctx, (rg_gains[i] >> 16) & 0x0F, (rg_gains[i] >> 8) & 0xFF, rg_gains[i] & 0xC0);
	}

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
	}

	DRV_LOG(ctx, "dgain reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n",
		rg_gains[0], rg_gains[1], rg_gains[2]);

	return ERROR_NONE;
}

static int ov13a10_set_night_hyperlapse(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;

	sNightHyperlapseSpeed = (int)*feature_data;
	DRV_LOG_MUST(ctx,"sNightHyperlapseSpeed:%d\n", sNightHyperlapseSpeed);

	return ERROR_NONE;
}

static int ov13a10_set_night_hyperlapse_sync_flag(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;

	night_hyperlapse_sync_flag = (bool)*feature_data;
	DRV_LOG(ctx,"night_hyperlapse_sync_flag:%d\n", night_hyperlapse_sync_flag);

	if (sNightHyperlapseCnt < FRAME_SKIP_FOR_NIGHT_HYPERLAPSE)
		night_hyperlapse_sync_flag = false;

	sNightHyperlapseCnt++;
	return ERROR_NONE;
}

void ov13a10_sensor_wait_stream_off(struct subdrv_ctx *ctx)
{
	u32 i = 0;
	u32 timeout = 0;
	u16 frame_cnt = 0;
	u32 init_delay = 2;
	u32 check_delay = 1;

	ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;

	mDELAY(init_delay);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x485E, 0x485F)
	 * Stream on: 0x0001 ~ 0xFFFF
	 * stream off: 0x0000 (Change after entering sw standby state after stream off command)
	 */
	for (i = 0; i < timeout; i++) {
		frame_cnt = (subdrv_i2c_rd_u8(ctx, 0x485E) << 8) | subdrv_i2c_rd_u8(ctx, 0x485F);
		DRV_LOG(ctx, "wait_streamoff, frame count:%#x\n", frame_cnt);

		if (frame_cnt == 0x0000) {
			DRV_LOG_MUST(ctx, "wait_streamoff delay:%d ms\n", init_delay + check_delay * i);
			return;
		}
		mDELAY(check_delay);
		DRV_LOG(ctx, "wait_streamoff delay:%d ms\n", init_delay + check_delay * i);
	}
	DRV_LOGE(ctx, "stream off fail!,cur_fps:%u,timeout:%u ms\n", ctx->current_fps, timeout);
}

static void ov13a10_set_streaming_control(struct subdrv_ctx *ctx, bool enable)
{
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	DRV_LOG_MUST(ctx, "E: stream[%s]\n", enable ? "ON" : "OFF");

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
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
				"please implement drive own streaming control!(senario id:%u)\n",
				ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
		DRV_LOG_MUST(ctx,
			"stream ctrl implement on scp side!(senario id:%u)\n",
			ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}

	if (enable) {
		if (sNightHyperlapseSpeed) {
			/* guide by ALPS10053346 */
			ctx->line_length = ctx->s_ctx.mode[ctx->current_scenario_id].linelength;
			ctx->frame_length = ctx->s_ctx.mode[ctx->current_scenario_id].framelength;
			ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;

			/* group param hold on */
			set_i2c_buffer(ctx, 0x3208, 0);

			subdrv_i2c_wr_u8(ctx, 0x380C, (ctx->line_length >> 8) & 0xFF);
			subdrv_i2c_wr_u8(ctx, 0x380D, ctx->line_length & 0xFF);
			subdrv_i2c_wr_u8(ctx, 0x380E, (ctx->frame_length >> 8) & 0xFF);
			subdrv_i2c_wr_u8(ctx, 0x380F, ctx->frame_length & 0xFF);

			/* group param hold off */
			set_i2c_buffer(ctx, 0x3208, 0x10);
			set_i2c_buffer(ctx, 0x3208, 0xa0);
			sNightHyperlapseCnt = 0;
		}
		set_dummy(ctx);
#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI_V2
		set_mipi_mode(ctx->current_scenario_id, ctx);
#endif
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x01);
		ctx->stream_ctrl_start_time = ktime_get_boottime_ns();
	} else {
		ctx->stream_ctrl_end_time = ktime_get_boottime_ns();
		if (sNightHyperlapseSpeed) {
			/* fast stream off for hyperlapse */
			subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x00);
		} else {		
			subdrv_i2c_wr_u8(ctx, 0x3208, 0x00);
			subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x00);
			subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
			subdrv_i2c_wr_u8(ctx, 0x3208, 0xA0);
		}

		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
		ctx->autoflicker_en = FALSE;
		ctx->extend_frame_length_en = 0;
		ctx->is_seamless = 0;
		if (ctx->s_ctx.chk_s_off_end)
			ov13a10_sensor_wait_stream_off(ctx); /*Wait streamoff sensor specific logic*/
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;
	}
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG_MUST(ctx, "X: stream[%s]\n", enable ? "ON" : "OFF");
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

static int ov13a10_use_long_exp(struct subdrv_ctx *ctx, bool enable, u32 shutter)
{
	u32 current_fll = 0;
	u32 current_exposure = 0;
	u32 reg_current_exposure = 0;
	u64 long_exposure_val = 0;
	u32 calculated_time_ms[4] = {0, };
	bool enable_stream_onoff = false;

#if 1
	/*disable control for stream on/off  */
	enable_stream_onoff = false;
#else
	if (enable && using_long_exp)
		enable_stream_onoff = false;
	else
		enable_stream_onoff = true;
#endif

	/* fast stream off */
	if (enable_stream_onoff) {
		subdrv_i2c_wr_u8(ctx, 0x0100, 0x00);
		DRV_LOG(ctx, "fast stream off\n");
	}

	if (enable == false) {
		subdrv_i2c_wr_u8(ctx, 0x3208, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3412, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3413, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3414, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3415, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3410, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
		subdrv_i2c_wr_u8(ctx, 0x3208, 0xA0);
		DRV_LOG(ctx, "long exposure mode: disable\n");
		using_long_exp = false;

	} else {
		current_fll = ctx->frame_length;
		current_exposure = ctx->frame_length - ctx->s_ctx.exposure_margin;
		reg_current_exposure = current_exposure >> 1; /* divide 2 */

		/* input shutter: llp unit, long_exposure_val: 256 unit */
		long_exposure_val = ((shutter - current_exposure) * ctx->line_length) / 256; /* target exposure - current exposure */

		subdrv_i2c_wr_u8(ctx, 0x3208, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x380E, (current_fll >> 8) & 0xFF); /* current FLL */
		subdrv_i2c_wr_u8(ctx, 0x380F, current_fll & 0xFF);
		subdrv_i2c_wr_u8(ctx, 0x3501, (reg_current_exposure >> 8) & 0xFF); /* current exposure */
		subdrv_i2c_wr_u8(ctx, 0x3502, reg_current_exposure & 0xFF);

		subdrv_i2c_wr_u8(ctx, 0x3412, (long_exposure_val >> 24) & 0xFF);
		subdrv_i2c_wr_u8(ctx, 0x3413, (long_exposure_val >> 16) & 0xFF);
		subdrv_i2c_wr_u8(ctx, 0x3414, (long_exposure_val >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx, 0x3415, long_exposure_val & 0xFF);

		if (!using_long_exp) {
			subdrv_i2c_wr_u8(ctx, 0x3410, 0x01); /* long_expo_mode_en */
			DRV_LOG(ctx, "long exposure mode: enable\n");
		}

		subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
		subdrv_i2c_wr_u8(ctx, 0x3208, 0xA0);
		using_long_exp = true;

		calculated_time_ms[0] = (ctx->line_length * shutter) / (ctx->pclk / 1000); /* input shutter*/
		calculated_time_ms[1] = (ctx->line_length * current_exposure) / (ctx->pclk / 1000); /* current exposure */
		calculated_time_ms[2] = (256 * long_exposure_val) / (ctx->pclk / 1000); /* long exposure setting */
		calculated_time_ms[3] = calculated_time_ms[1] + calculated_time_ms[2]; /* total exposure */
		DRV_LOG_MUST(ctx, "input shutter: %#0x(%d ms), FLL:%#0x, current exposure:%#0x(%d ms), remain long_exposure:%#0x(%d ms), total: %d ms\n",
				shutter, calculated_time_ms[0], current_fll, current_exposure, calculated_time_ms[1],
				(u32)long_exposure_val, calculated_time_ms[2], calculated_time_ms[3]);
	}

	/* stream on */
	if (enable_stream_onoff) {
		subdrv_i2c_wr_u8(ctx, 0x0100, 0x01);
		DRV_LOG(ctx, "stream on\n");
	}
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = shutter;
	return ERROR_NONE;
}

static int ov13a10_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 shutter = *feature_data;
	u32 frame_length = *(feature_data + 1);
	u32 cust_rg_shutters = 0;
	u32 calculated_exposure_time_ms = 0;
	u32 set_llp = 0;

	int fine_integ_line = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	DRV_LOG(ctx,"input shutter: %llu(%#llx)\n", shutter, shutter);

	if (shutter > ctx->s_ctx.exposure_max && (sNightHyperlapseSpeed == 0)) {
		ov13a10_use_long_exp(ctx, true, shutter);
		DRV_LOG_MUST(ctx, "enable long exposure\n");
		return ERROR_NONE;
	} else if (using_long_exp) {
		ov13a10_use_long_exp(ctx, false, 0);
		DRV_LOG_MUST(ctx, "disable long exposure\n");
	}

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

	/* normal -> hyperlapse */
	if (sNightHyperlapseSpeed && night_hyperlapse_sync_flag) {
	 	if (sNightHyperlapseSpeed == 15) {
			/* set_llp, 0x048C: 0x246 X 2 */
			set_llp = ctx->s_ctx.mode[ctx->current_scenario_id].linelength << 1;
		} else if (sNightHyperlapseSpeed == 45) {
			/* set_llp, 0x1230: 0x246 X 8 */
			set_llp = ctx->s_ctx.mode[ctx->current_scenario_id].linelength << 3;
		} else {
			set_llp = ctx->s_ctx.mode[ctx->current_scenario_id].linelength;
		}

		if (set_llp != ctx->line_length) {
			ctx->line_length = set_llp;

			//group hold on
			subdrv_i2c_wr_u8(ctx, 0x3208, 0);

			subdrv_i2c_wr_u8(ctx, 0x380c, (ctx->line_length >> 8) & 0xFF);
			subdrv_i2c_wr_u8(ctx, 0x380d, ctx->line_length & 0xFF);

			//group hold off
			subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
			subdrv_i2c_wr_u8(ctx, 0x3208, 0xa0);
			DRV_LOG_MUST(ctx,"sNightHyperlapseSpeed(%d) - change LLP: %#0x, sNightHyperlapseCnt:%d\n",
					sNightHyperlapseSpeed, set_llp, sNightHyperlapseCnt);
		}

		// ex)fps = 1/1.5 (Frame Duration : 1.5sec), 15: 2fps(500ms), 45: 2/3fps(1.5sec)
		// frame_length = pclk / (line_length * fps)
		ctx->frame_length = (((ctx->pclk / 30) * sNightHyperlapseSpeed)/ctx->line_length);
		ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;

		// shutter <= frame length - exposure margin
		shutter = min_t(u64, shutter, ctx->frame_length - ctx->s_ctx.exposure_margin);

		DRV_LOG_MUST(ctx, "Night Hyperlapse - LLP:%#0x, FLL:%#0x, exp:%#x\n",
				ctx->line_length, ctx->frame_length, (u32) shutter);
	}

	/* hyperlapse -> normal */
	if ((sNightHyperlapseSpeed == 0) && !night_hyperlapse_sync_flag &&
		(ctx->line_length != ctx->s_ctx.mode[ctx->current_scenario_id].linelength)) {
		ctx->line_length = ctx->s_ctx.mode[ctx->current_scenario_id].linelength;
		DRV_LOG_MUST(ctx,"sNightHyperlapseSpeed(%d) - change LLP: %#0x\n", sNightHyperlapseSpeed, ctx->line_length);

		//group hold on
		subdrv_i2c_wr_u8(ctx, 0x3208, 0);

		subdrv_i2c_wr_u8(ctx, 0x380c, (ctx->line_length >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx, 0x380d, ctx->line_length & 0xFF);

		//group hold off
		subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
		subdrv_i2c_wr_u8(ctx, 0x3208, 0xa0);
		sNightHyperlapseCnt = 0;
	}

	/* reset night hyperlapse flag */
	night_hyperlapse_sync_flag = false;

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

	calculated_exposure_time_ms = (ctx->line_length * ctx->exposure[0]) / (ctx->pclk / 1000);
	DRV_LOG(ctx, "exposure time: %d ms, exp[0x%x(0x%x)], fll(input/output):%u/%u, flick_en:%d\n",
			calculated_exposure_time_ms, ctx->exposure[0], cust_rg_shutters, frame_length,
			ctx->frame_length, ctx->autoflicker_en);
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
	u32 calculated_exposure_time_ms = 0;
	u32 set_llp = 0;

	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[4] = {0};
	int readout_diff = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;
	u32 fll = 0, fll_temp = 0, s_fll;

	if (shutters[0] > ctx->s_ctx.exposure_max && (sNightHyperlapseSpeed == 0)) {
		ov13a10_use_long_exp(ctx, true, shutters[0]);
		DRV_LOG_MUST(ctx, "enable long exposure\n");
		return ERROR_NONE;
	} else if (using_long_exp) {
		ov13a10_use_long_exp(ctx, false, 0);
		DRV_LOG_MUST(ctx, "disable long exposure\n");
	}

	fll = frame_length ? frame_length : ctx->min_frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	DRV_LOG(ctx,"input shutter: %llu(%#llx)\n", shutters[0], shutters[0]);

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

	/* normal -> hyperlapse */
	if (sNightHyperlapseSpeed && night_hyperlapse_sync_flag) {
	 	if (sNightHyperlapseSpeed == 15) {
			/* set_llp, 0x048C: 0x246 X 2 */
			set_llp = ctx->s_ctx.mode[ctx->current_scenario_id].linelength << 1;
		} else if (sNightHyperlapseSpeed == 45) {
			/* set_llp, 0x1230: 0x246 X 8 */
			set_llp = ctx->s_ctx.mode[ctx->current_scenario_id].linelength << 3;
		} else {
			set_llp = ctx->s_ctx.mode[ctx->current_scenario_id].linelength;
		}

		if (set_llp != ctx->line_length) {
			ctx->line_length = set_llp;

			//group hold on
			subdrv_i2c_wr_u8(ctx, 0x3208, 0);


			subdrv_i2c_wr_u8(ctx, 0x380c, (ctx->line_length >> 8) & 0xFF);
			subdrv_i2c_wr_u8(ctx, 0x380d, ctx->line_length & 0xFF);

			//group hold off
			subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
			subdrv_i2c_wr_u8(ctx, 0x3208, 0xa0);
			DRV_LOG_MUST(ctx,"sNightHyperlapseSpeed(%d) - change LLP: %#0x, sNightHyperlapseCnt:%d\n",
					sNightHyperlapseSpeed, set_llp, sNightHyperlapseCnt);
		}

		// ex)fps = 1/1.5 (Frame Duration : 1.5sec), 15: 2fps(500ms), 45: 2/3fps(1.5sec)
		// frame_length = pclk / (line_length * fps)
		ctx->frame_length = (((ctx->pclk / 30) * sNightHyperlapseSpeed)/ctx->line_length);
		fll = ctx->frame_length;
		ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;

		// shutter <= frame length - exposure margin
		shutters[0] = min_t(u64, shutters[0], ctx->frame_length - ctx->s_ctx.exposure_margin);

		DRV_LOG_MUST(ctx, "Night Hyperlapse - LLP:%#0x, FLL:%#0x, exp:%#x\n",
				ctx->line_length, ctx->frame_length, (u32) shutters[0]);
	}

	/* hyperlapse -> normal */
	if ((sNightHyperlapseSpeed == 0) && !night_hyperlapse_sync_flag &&
		(ctx->line_length != ctx->s_ctx.mode[ctx->current_scenario_id].linelength)) {
		ctx->line_length = ctx->s_ctx.mode[ctx->current_scenario_id].linelength;
		DRV_LOG_MUST(ctx,"sNightHyperlapseSpeed(%d) - change LLP: %#0x\n", sNightHyperlapseSpeed, ctx->line_length);

		//group hold on
		subdrv_i2c_wr_u8(ctx, 0x3208, 0);

		subdrv_i2c_wr_u8(ctx, 0x380c, (ctx->line_length >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx, 0x380d, ctx->line_length & 0xFF);

		//group hold off
		subdrv_i2c_wr_u8(ctx, 0x3208, 0x10);
		subdrv_i2c_wr_u8(ctx, 0x3208, 0xa0);

		sNightHyperlapseCnt = 0;
	}

	/* reset night hyperlapse flag */
	night_hyperlapse_sync_flag = false;

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

	calculated_exposure_time_ms = (ctx->line_length * ctx->exposure[0]) / (ctx->pclk / 1000);
	DRV_LOG(ctx, "exposure[0] time: %d ms, exp[0x%x(0x%x)/0x%x(0x%x)/0x%x(0x%x)], fll(input/output):%u/%u, flick_en:%d\n",
		calculated_exposure_time_ms,
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

static int ov13a10_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);

	/* only support black, 1: solid pattern 5: black pattern */
	if (mode == 0x1 || mode == 0x5) {
		/* disable dgain */
		ctx->s_ctx.reg_addr_dig_gain[0].addr[0] = PARAM_UNDEFINED;
		ctx->s_ctx.reg_addr_dig_gain[0].addr[1] = PARAM_UNDEFINED;
		DRV_LOG(ctx, "dgain control: disable\n");
		mDELAY(1);

		/* dgain: x0.0 */
		ov13a10_set_dgain_reg(ctx, 0x00, 0x00, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x5001, subdrv_i2c_rd_u8(ctx, 0x5001) & ~0x10);
	} else {
		/* dgain: x1.0 */
		ov13a10_set_dgain_reg(ctx, 0x01, 0x00, 0x00);

		if (ctx->current_scenario_id != SENSOR_SCENARIO_ID_CUSTOM1) {
			subdrv_i2c_wr_u8(ctx, 0x5001, subdrv_i2c_rd_u8(ctx, 0x5001) | 0x10);
			DRV_LOG_MUST(ctx, "PD restore mode: enable\n");
		}
		/* enable dgain */
		ctx->s_ctx.reg_addr_dig_gain[0].addr[0] = 0x350A;
		ctx->s_ctx.reg_addr_dig_gain[0].addr[1] = 0x350B;
		DRV_LOG(ctx, "dgain control: enable\n");
	}
	DRV_LOG_MUST(ctx, "test pattern: %d\n", mode);

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int ov13a10_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return ov13a10_get_imgsensor_id(ctx, (u32 *)para);
}

static int ov13a10_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			/* S/W reset */
			subdrv_i2c_wr_u8(ctx, 0x0103, 0x01);
			mDELAY(1);

			/* OV13A10 sensor id, 0x300A:0x56, 0x300B:0x13, 0x300C:0x41 */
			*sensor_id = ((subdrv_i2c_rd_u8(ctx, 0x300A) & 0xFF) << 16) |
				((subdrv_i2c_rd_u8(ctx, 0x300B) & 0xFF) << 8) |
				(subdrv_i2c_rd_u8(ctx, 0x300C) & 0xFF);

			if (*sensor_id == 0x561341)
				*sensor_id = OV13A10_SENSOR_ID;

			DRV_LOG(ctx, "i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);

			if (*sensor_id == ctx->s_ctx.sensor_id)
				return ERROR_NONE;
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}

	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static int ov13a10_open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (ov13a10_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE) {
#ifdef IMGSENSOR_HW_PARAM
		imgsensor_increase_hw_param_sensor_err_cnt(imgsensor_get_sensor_position(ctx->s_ctx.sensor_id));
#endif
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	/* initial setting */
	/* already S/W reset(0x0103) in ov13a10_get_imgsensor_id */
	mDELAY(15); /* total delay 16ms: get sensor id 1ms + 15ms */

	if (ctx->s_ctx.aov_sensor_support && !ctx->s_ctx.init_in_open)
		DRV_LOG_MUST(ctx, "sensor init not in open stage!\n");
	else
		sensor_init(ctx);

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->frame_length_rg = ctx->frame_length;
	ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;
	sNightHyperlapseSpeed = 0;
	sNightHyperlapseCnt = 0;
	using_long_exp = false;

	return ERROR_NONE;
}

#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI_V2
static int ov13a10_get_mipi_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	ov13a10_set_mipi_pixel_rate_by_scenario(ctx, scenario_id, (u32 *)(uintptr_t)(*(feature_data + 1)));
	return ERROR_NONE;
}
static u32 ov13a10_get_custom_mipi_pixel_rate(enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	u32 mipi_pixel_rate = 0;

	mipi_pixel_rate = update_mipi_info(scenario_id);
	return mipi_pixel_rate;
}
static void ov13a10_set_mipi_pixel_rate_by_scenario(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *mipi_pixel_rate)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
		*mipi_pixel_rate = ov13a10_get_custom_mipi_pixel_rate(scenario_id);
		break;
	default:
		break;
	}
	if (*mipi_pixel_rate == 0)
		*mipi_pixel_rate = ctx->s_ctx.mode[scenario_id].mipi_pixel_rate;
	DRV_LOG_MUST(ctx, "mipi_pixel_rate changed %u->%u\n",
		ctx->s_ctx.mode[scenario_id].mipi_pixel_rate, *mipi_pixel_rate);
}
static int update_mipi_info(enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int sensor_mode = scenario_id;

	pr_info("[%s], scenario=%d\n", __func__, sensor_mode);
	if (sensor_mode == SENSOR_SCENARIO_ID_CUSTOM1) {
		pr_info("skip adaptive mipi , mode invalid:%d\n", sensor_mode);
		return 0;
	}
	cur_mipi_sensor_mode = &ov13a10_adaptive_mipi_sensor_mode[sensor_mode];
	if (cur_mipi_sensor_mode->mipi_cell_ratings_size == 0 ||
		cur_mipi_sensor_mode->mipi_cell_ratings == NULL) {
		pr_info("skip select mipi channel\n");
		return 0;
	}
	adaptive_mipi_index = imgsensor_select_mipi_by_rf_cell_infos(cur_mipi_sensor_mode);
	pr_info("adaptive_mipi_index : %d\n", adaptive_mipi_index);
	if (adaptive_mipi_index != -1) {
		if (adaptive_mipi_index < cur_mipi_sensor_mode->sensor_setting_size) {
			pr_info("mipi_rate:%d\n", cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].mipi_rate);
			return cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].mipi_rate;
		}
	}
	pr_info("adaptive_mipi_index invalid: %d\n", adaptive_mipi_index);
	return 0;
};
static int set_mipi_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id, struct subdrv_ctx *ctx)
{
	int ret = 0;
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;

	DRV_LOG(ctx, "[%s] scenario_id=%d\n", __func__, scenario_id);
	if (scenario_id == SENSOR_SCENARIO_ID_CUSTOM1) {
		DRV_LOG_MUST(ctx, "NOT supported sensor mode : %d", scenario_id);
		return -1;
	}
	cur_mipi_sensor_mode = &ov13a10_adaptive_mipi_sensor_mode[scenario_id];
	if (cur_mipi_sensor_mode->sensor_setting == NULL) {
		DRV_LOG_MUST(ctx, "no mipi setting for current sensor mode\n");
		return -1;
	}
	if (adaptive_mipi_index <= CAM_OV13A10_SET_A_all_1195p2_MHZ || adaptive_mipi_index >= CAM_OV13A10_SET_MAX_NUM) {
		DRV_LOG_MUST(ctx, "adaptive Mipi is set to the default values 1195p2Mbps");
		subdrv_i2c_wr_regs_u8(ctx,
			cur_mipi_sensor_mode->sensor_setting[0].setting,
			cur_mipi_sensor_mode->sensor_setting[0].setting_size);
	} else {
		DRV_LOG_MUST(ctx, "adaptive mipi settings: %d: mipi clock [%d]\n",
			 scenario_id, cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].mipi_rate);
		subdrv_i2c_wr_regs_u8(ctx,
			cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].setting,
			cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].setting_size);
	}
	DRV_LOG(ctx, "[%s]-X\n", __func__);
	return ret;
}
#endif
