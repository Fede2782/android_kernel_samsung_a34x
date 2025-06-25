/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GC08A3MIPI_SENSOR_H__
#define __GC08A3MIPI_SENSOR_H__

#define MULTI_WRITE             1
#if MULTI_WRITE
#define I2C_BUFFER_LEN  765 /* Max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN  3
#endif

/* SENSOR MIRROR FLIP INFO */
#define GC08A3_MIRROR_NORMAL    0
#define GC08A3_MIRROR_H         0
#define GC08A3_MIRROR_V         0
#define GC08A3_MIRROR_HV        1

#if GC08A3_MIRROR_NORMAL
#define GC08A3_MIRROR	        0x00
#elif GC08A3_MIRROR_H
#define GC08A3_MIRROR	        0x01
#elif GC08A3_MIRROR_V
#define GC08A3_MIRROR	        0x02
#elif GC08A3_MIRROR_HV
#define GC08A3_MIRROR	        0x03
#else
#define GC08A3_MIRROR	        0x00
#endif


#define SENSOR_BASE_GAIN           0x400
#define SENSOR_MAX_GAIN            (16 * SENSOR_BASE_GAIN)

enum{
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;
	kal_uint8 startx;
	kal_uint8 starty;
	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;
	kal_uint32 mipi_pixel_rate;
	kal_uint8 mipi_data_lp2hs_settle_dc;
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
struct imgsensor_struct {
	kal_uint8 mirror;
	kal_uint8 sensor_mode;
	kal_uint32 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint32 frame_length;
	kal_uint32 line_length;
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;
	kal_uint16 dummy_line;
	kal_uint16 current_fps;
	kal_bool   autoflicker_en;
	kal_uint8 test_pattern;			//record test pattern mode or not
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id; /* current scenario id */
	kal_uint8  ihdr_en;
	kal_uint8 i2c_write_id;
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id;
	kal_uint32 checksum_value;
	struct imgsensor_mode_struct pre;
	struct imgsensor_mode_struct cap;

	struct imgsensor_mode_struct normal_video;
	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;
	kal_uint8  ae_shut_delay_frame;
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;
	kal_uint8  sensor_mode_num;
	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;
	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;
	kal_uint8  isp_driving_current;
	kal_uint8  sensor_interface_type;
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;
	kal_uint8  mipi_lane_num;
	kal_uint8  i2c_addr_table[5];
	kal_uint32 i2c_speed;
};

struct gc08a3_otp_struct {
	kal_uint8 af_group_flag;
	kal_uint8 af_info[8];
	kal_uint8 af_checksum;
	kal_uint8 awb_group_flag;
	kal_uint8 awb_info[12];
	kal_uint8 awb_checksum;
	kal_uint8 lsc_group_flag;
	kal_uint8 lsc_info[1868];
	kal_uint8 lsc_checksum;
};


extern int iReadRegI2C(u8 *a_pSendData,
			u16 a_sizeSendData,
			u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);

unsigned int gc08a3_read_region(struct i2c_client *client, unsigned int addr,
			unsigned char *data, unsigned int size);

#endif
