/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __hdmiedid_h__
#define __hdmiedid_h__

#define EDID_SIZE 1024

#define EDID_ID     0x50	/* 0xA0 */
#define EDID_ID1    0x51	/* 0xA2 */

/* (5) Define the EDID relative information */
/* (5.1) Define one EDID block length */
#define EDID_BLOCK_LEN      128
/* (5.2) Define EDID header length */
#define EDID_HEADER_LEN     8
/* (5.3) Define the address for EDID info.
 *(ref. EDID Recommended Practive for EIA/CEA-861)
 * Base Block 0
 */
#define EDID_ADDR_HEADER                      0x00
#define EDID_ADDR_VERSION                     0x12
#define EDID_ADDR_REVISION                    0x13
#define EDID_IMAGE_HORIZONTAL_SIZE            0x15
#define EDID_IMAGE_VERTICAL_SIZE              0x16
#define EDID_ADDR_FEATURE_SUPPORT             0x18
#define EDID_ADDR_TIMING_DSPR_1               0x36
#define EDID_ADDR_TIMING_DSPR_2               0x48
#define EDID_ADDR_MONITOR_DSPR_1              0x5A
#define EDID_ADDR_MONITOR_DSPR_2              0x6C
#define EDID_ADDR_EXT_BLOCK_FLAG              0x7E
/* EDID address: 0x83 */
#define EDID_ADDR_EXTEND_BYTE3                0x03
/* for ID receiver if RGB, YCbCr 4:2:2 or 4:4:4 */
/* Extension Block 1: */
#define EXTEDID_ADDR_TAG                      0x00
#define EXTEDID_ADDR_REVISION                 0x01
#define EXTEDID_ADDR_OFST_TIME_DSPR           0x02

/* (5.4) Define the ID for descriptor block type */
/* Notice: reference Table 11 ~ 14 of "EDID
 *Recommended Practive for EIA/CEA-861"
 */
#define DETAIL_TIMING_DESCRIPTOR              -1
#define UNKNOWN_DESCRIPTOR                    -255
#define MONITOR_NAME_DESCRIPTOR               0xFC
#define MONITOR_RANGE_LIMITS_DESCRIPTOR       0xFD


/* (5.5) Define the offset address of info.
 *within detail timing descriptor block
 */
#define OFST_PXL_CLK_LO       0
#define OFST_PXL_CLK_HI       1
#define OFST_H_ACTIVE_LO      2
#define OFST_H_BLANKING_LO    3
#define OFST_H_ACT_BLA_HI     4
#define OFST_V_ACTIVE_LO      5
#define OFST_V_BLANKING_LO    6
#define OFST_V_ACTIVE_HI      7
#define OFST_FLAGS            17

/* (5.6) Define the ID for EDID extension type */
#define LCD_TIMING                  0x1
#define CEA_TIMING_EXTENSION        0x01
#define EDID_20_EXTENSION           0x20
#define COLOR_INFO_TYPE0            0x30
#define DVI_FEATURE_DATA            0x40
#define TOUCH_SCREEN_MAP            0x50
#define BLOCK_MAP                   0xF0
#define EXTENSION_DEFINITION        0xFF

/* (5.7) Define EDID VSDB header length */
#define EDID_VSDB_LEN               0x03
enum HDMI_SINK_DEEP_COLOR_T {
	HDMI_SINK_NO_DEEP_COLOR = 0,
	HDMI_SINK_DEEP_COLOR_10_BIT = (1 << 0),
	HDMI_SINK_DEEP_COLOR_12_BIT = (1 << 1),
	HDMI_SINK_DEEP_COLOR_16_BIT = (1 << 2)
};

struct HDMI_SINK_AV_CAP_T {
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_cea_ntsc_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_cea_pal_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_org_cea_ntsc_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_org_cea_pal_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_ntsc_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_pal_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_ntsc_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_pal_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_native_ntsc_resolution;
	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_native_pal_resolution;
	/* use HDMI_SINK_VIDEO_COLORIMETRY_T */
	unsigned int ui4_sink_colorimetry;
	/* use HDMI_SINK_VCDB_T */
	unsigned short ui2_sink_vcdb_data;
	/* HDMI_SINK_AUDIO_DECODER_T */
	unsigned short ui2_sink_aud_dec;
	unsigned char ui1_sink_dsd_ch_num;
	unsigned char ui1_sink_pcm_ch_sampling[7];
	/* n: channel number index, value: each bit
	 *means sampling rate for this channel number
	 *(SINK_AUDIO_32k..)
	 */
	unsigned char ui1_sink_pcm_bit_size[7];
	/* //n: channel number index, value: each bit
	 *means bit size for this channel number
	 */
	unsigned char ui1_sink_dst_ch_sampling[7];
	/* n: channel number index, value: each bit
	 *means sampling rate for this channel number
	 *(SINK_AUDIO_32k..)
	 */
	unsigned char ui1_sink_dsd_ch_sampling[7];
	/* n: channel number index, value: each bit
	 *means sampling rate for this channel number
	 *(SINK_AUDIO_32k..)
	 */
	unsigned char ui1_sink_ac3_ch_sampling[7];
	/* n: channel number index, value: each bit
	 *means sampling rate for this channel number
	 *(SINK_AUDIO_32k..)
	 */
	unsigned char ui1_sink_ec3_ch_sampling[7];
	/* n: channel number index, value:
	 *each bit means sampling rate for this channel
	 *number (SINK_AUDIO_32k..)
	 */

	unsigned char ui1_sink_org_pcm_ch_sampling[7];
	unsigned char ui1_sink_org_pcm_bit_size[7];
	unsigned char ui1_sink_mpeg1_ch_sampling[7];
	unsigned char ui1_sink_mp3_ch_sampling[7];
	unsigned char ui1_sink_mpeg2_ch_sampling[7];
	unsigned char ui1_sink_aac_ch_sampling[7];
	unsigned char ui4_sink_dts_fs;
	unsigned char ui1_sink_dts_ch_sampling[7];
	unsigned char ui1_sink_atrac_ch_sampling[7];
	unsigned char ui1_sink_dolby_plus_ch_sampling[7];
	unsigned char ui1_sink_dts_hd_ch_sampling[7];
	unsigned char ui1_sink_mat_mlp_ch_sampling[7];
	unsigned char ui1_sink_wma_ch_sampling[7];
	unsigned char ui1_sink_support_dolby_atoms;
	unsigned short ui1_sink_max_tmds_clock;
	unsigned char ui1_sink_spk_allocation;
	unsigned char ui1_sink_content_cnc;
	unsigned char ui1_sink_p_latency_present;
	unsigned char ui1_sink_i_latency_present;
	unsigned int ui1_sink_p_audio_latency;
	unsigned int ui1_sink_p_video_latency;
	unsigned int ui1_sink_i_audio_latency;
	unsigned int ui1_sink_i_video_latency;
	unsigned char e_sink_rgb_color_bit;
	unsigned char e_sink_ycbcr_color_bit;
	unsigned char u1_sink_support_ai;
	unsigned char u1_sink_max_tmds;
	/* HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T */
	unsigned short ui2_edid_chksum_and_audio_sup;
	unsigned short ui2_sink_cec_address;
	unsigned char b_sink_edid_ready;
	unsigned char b_sink_support_hdmi_mode;
	unsigned char ui1_ExtEdid_Revision;
	unsigned char ui1_Edid_Version;
	unsigned char ui1_Edid_Revision;
	unsigned char ui1_sink_support_ai;
	unsigned char ui1_Display_Horizontal_Size;
	unsigned char ui1_Display_Vertical_Size;
	unsigned char b_sink_hdmi_video_present;
	unsigned char ui1_CNC;
	unsigned char b_sink_3D_present;
	unsigned int ui4_sink_cea_3D_resolution;
	unsigned char b_sink_SCDC_present;
	unsigned char b_sink_LTE_340M_sramble;
	unsigned int ui4_sink_hdmi_4k2kvic;

	unsigned int ui4_sink_id_serial_number;
	unsigned char ui1_sink_support_static_hdr;
	unsigned char ui1_sink_support_dynamic_hdr;
	unsigned char ui1_sink_hdr10plus_app_version;
	unsigned char ui1_sink_dv_block[32];
	unsigned char ui1_sink_hf_vsdb_exist;
	unsigned short ui2_sink_max_tmds_character_rate;
	unsigned char ui1_sink_hf_vsdb_info;
	unsigned char ui1_sink_dc420_color_bit;
	unsigned char ui1_sink_hdr_content_max_luminance;
	unsigned char ui1_sink_hdr_content_max_frame_average_luminance;
	unsigned char ui1_sink_hdr_content_min_luminance;
	unsigned char ui1_sink_hdr_block[7];	/* add for hdmi rx merge EDID */
	unsigned char ui1_sink_hfvsdb_block[8];	/* add for hdmi rx merge EDID */

	unsigned int ui4_sink_hdmi_4k2kvic_420_vdb;

	unsigned int ui4_sink_dv_vsvdb_length;
	unsigned int ui4_sink_dv_vsvdb_version;
	unsigned int ui4_sink_dv_vsvdb_v1_low_latency;
	unsigned int ui4_sink_dv_vsvdb_v2_interface;
	unsigned int ui4_sink_dv_vsvdb_low_latency_support;
	unsigned int ui4_sink_dv_vsvdb_v2_supports_10b_12b_444;
	unsigned int ui4_sink_dv_vsvdb_support_backlight_control;
	unsigned int ui4_sink_dv_vsvdb_backlt_min_lumal;
	unsigned int ui4_sink_dv_vsvdb_tmin;
	unsigned int ui4_sink_dv_vsvdb_tmax;
	unsigned int ui4_sink_dv_vsvdb_tminPQ;
	unsigned int ui4_sink_dv_vsvdb_tmaxPQ;
	unsigned int ui4_sink_dv_vsvdb_Rx;
	unsigned int ui4_sink_dv_vsvdb_Ry;
	unsigned int ui4_sink_dv_vsvdb_Gx;
	unsigned int ui4_sink_dv_vsvdb_Gy;
	unsigned int ui4_sink_dv_vsvdb_Bx;
	unsigned int ui4_sink_dv_vsvdb_By;
	unsigned int ui4_sink_dv_vsvdb_Wx;
	unsigned int ui4_sink_dv_vsvdb_Wy;
	unsigned char u1_sink_allm_support;
};

/* HDMI_SINK_AUDIO_DECODER_T define what kind of audio decoder */
/* can be supported by sink. */
#define   HDMI_SINK_AUDIO_DEC_LPCM        (1<<0)
#define   HDMI_SINK_AUDIO_DEC_AC3         (1<<1)
#define   HDMI_SINK_AUDIO_DEC_MPEG1       (1<<2)
#define   HDMI_SINK_AUDIO_DEC_MP3         (1<<3)
#define   HDMI_SINK_AUDIO_DEC_MPEG2       (1<<4)
#define   HDMI_SINK_AUDIO_DEC_AAC         (1<<5)
#define   HDMI_SINK_AUDIO_DEC_DTS         (1<<6)
#define   HDMI_SINK_AUDIO_DEC_ATRAC       (1<<7)
#define   HDMI_SINK_AUDIO_DEC_DSD         (1<<8)
#define   HDMI_SINK_AUDIO_DEC_DOLBY_PLUS   (1<<9)
#define   HDMI_SINK_AUDIO_DEC_DTS_HD      (1<<10)
#define   HDMI_SINK_AUDIO_DEC_MAT_MLP     (1<<11)
#define   HDMI_SINK_AUDIO_DEC_DST         (1<<12)
#define   HDMI_SINK_AUDIO_DEC_ATMOS       (1<<13)
#define   HDMI_SINK_AUDIO_DEC_WMA         (1<<14)


/* Sink audio channel ability for a fixed Fs */
#define SINK_AUDIO_2CH   (1<<0)
#define SINK_AUDIO_3CH   (1<<1)
#define SINK_AUDIO_4CH   (1<<2)
#define SINK_AUDIO_5CH   (1<<3)
#define SINK_AUDIO_6CH   (1<<4)
#define SINK_AUDIO_7CH   (1<<5)
#define SINK_AUDIO_8CH   (1<<6)

/* Sink supported sampling rate for a fixed channel number */
#define SINK_AUDIO_32k (1<<0)
#define SINK_AUDIO_44k (1<<1)
#define SINK_AUDIO_48k (1<<2)
#define SINK_AUDIO_88k (1<<3)
#define SINK_AUDIO_96k (1<<4)
#define SINK_AUDIO_176k (1<<5)
#define SINK_AUDIO_192k (1<<6)

/*The following definition is for Sink speaker allocation data block .*/
#define SINK_AUDIO_FL_FR   (1<<0)
#define SINK_AUDIO_LFE     (1<<1)
#define SINK_AUDIO_FC      (1<<2)
#define SINK_AUDIO_RL_RR   (1<<3)
#define SINK_AUDIO_RC      (1<<4)
#define SINK_AUDIO_FLC_FRC (1<<5)
#define SINK_AUDIO_RLC_RRC (1<<6)

/* The following definition is */
/* For EDID Audio Support, //HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T */
#define SINK_BASIC_AUDIO_NO_SUP    (1<<0)
#define SINK_SAD_NO_EXIST          (1<<1)/* short audio descriptor */
#define SINK_BASE_BLK_CHKSUM_ERR   (1<<2)
#define SINK_EXT_BLK_CHKSUM_ERR    (1<<3)


#define RES_480i_60      (1 << 0)
#define RES_576i_50    (1 << 1)
#define RES_480p_60   (1 << 2)
#define RES_576p_50   (1 << 3)
#define RES_720p_60 (1 << 4)
#define RES_720p_50 (1 << 5)
#define RES_1080i_60      (1 << 6)
#define RES_1080i_50 (1 << 7)
#define RES_1080p_30 (1 << 8)
#define RES_1080p_25   (1 << 9)
#define RES_1080p_24      (1 << 10)
#define RES_1080p_23    (1 << 11)
#define RES_1080p_29   (1 << 12)
#define RES_1080p_60   (1 << 13)
#define RES_1080p_50 (1 << 14)
#define RES_2160P_23_976   (1 << 21)
#define RES_2160P_24   (1 << 22)
#define RES_2160P_25       (1 << 23)
#define RES_2160P_29_97   (1 << 24)
#define RES_2160P_30   (1 << 25)
#define RES_2161P_24 (1 << 26)
#define RES_2160P_60   (1 << 27)
#define RES_2160P_50   (1 << 28)
#define RES_2161P_60 (1 << 29)
#define RES_2161P_50       (1<<30)
#define RES_720P_59_94	(1LL << 31)
#define RES_1080P_59_94	(1LL << 32)
#define RES_2160P_59_94	(1LL << 33)
#define RES_2161P_59_94	(1LL << 34)

#define SINK_480P      (1 << 0)
#define SINK_720P60    (1 << 1)
#define SINK_1080I60   (1 << 2)
#define SINK_1080P60   (1 << 3)
#define SINK_480P_1440 (1 << 4)
#define SINK_480P_2880 (1 << 5)
#define SINK_480I      (1 << 6)
#define SINK_480I_1440 (1 << 7)
#define SINK_480I_2880 (1 << 8)
#define SINK_1080P30   (1 << 9)
#define SINK_576P      (1 << 10)
#define SINK_720P50    (1 << 11)
#define SINK_1080I50   (1 << 12)
#define SINK_1080P50   (1 << 13)
#define SINK_576P_1440 (1 << 14)
#define SINK_576P_2880 (1 << 15)
#define SINK_576I      (1 << 16)
#define SINK_576I_1440 (1 << 17)
#define SINK_576I_2880 (1 << 18)
#define SINK_1080P25   (1 << 19)
#define SINK_1080P24   (1 << 20)
#define SINK_1080P23976   (1 << 21)
#define SINK_1080P2997   (1 << 22)
#define SINK_VGA       (1 << 23)	/* 640x480P */
#define SINK_480I_4_3   (1 << 24)	/* 720x480I 4:3 */
#define SINK_480P_4_3   (1 << 25)	/* 720x480P 4:3 */
#define SINK_480P_2880_4_3 (1 << 26)
#define SINK_576I_4_3   (1 << 27)	/* 720x480I 4:3 */
#define SINK_576P_4_3   (1 << 28)	/* 720x480P 4:3 */
#define SINK_576P_2880_4_3 (1 << 29)
#define SINK_720P24       (1<<30)
#define SINK_720P23976 (1<<31)

/*the 2160 mean 3840x2160 */
#define SINK_2160P_23_976HZ (1 << 0)
#define SINK_2160P_24HZ (1 << 1)
#define SINK_2160P_25HZ (1 << 2)
#define SINK_2160P_29_97HZ (1 << 3)
#define SINK_2160P_30HZ (1 << 4)
/*the 2161 mean 4096x2160 */
#define SINK_2161P_24HZ (1 << 5)
#define SINK_2161P_25HZ (1 << 6)
#define SINK_2161P_30HZ (1 << 7)
#define SINK_2160P_50HZ (1 << 8)
#define SINK_2160P_60HZ (1 << 9)
#define SINK_2161P_50HZ (1 << 10)
#define SINK_2161P_60HZ (1 << 11)
#define SINK_2161P_23_976HZ (1 << 12)
#define SINK_2161P_29_97HZ (1 << 13)

enum AUDIO_BITSTREAM_TYPE_T {
	AVD_BITS_NONE = 0,
	AVD_LPCM = 1,
	AVD_AC3,
	AVD_MPEG1_AUD,
	AVD_MP3,
	AVD_MPEG2_AUD,
	AVD_AAC,
	AVD_DTS,
	AVD_ATRAC,
	AVD_DSD,
	AVD_DOLBY_PLUS,
	AVD_DTS_HD,
	AVD_MAT_MLP,
	AVD_DST,
	AVD_DOLBY_ATMOS,
	AVD_WMA,
	AVD_CDDA,
	AVD_SACD_PCM,
	AVD_HDCD = 0xfe,
	AVD_BITS_OTHERS = 0xff
};

enum EDID_HF_VSDB_INFO_T {
	EDID_HF_VSDB_3D_OSD_DISPARITY = (1 << 0),
	EDID_HF_VSDB_DUAL_VIEW = (1 << 1),
	EDID_HF_VSDB_INDEPENDENT_VIEW = (1 << 2),
	EDID_HF_VSDB_LTE_340MCSC_SCRAMBLE = (1 << 3),
	EDID_HF_VSDB_RR_CAPABLE = (1 << 6),
	EDID_HF_VSDB_SCDC_PRESENT = (1 << 7),
};

/* Dynamic hdr supported by Sink */
enum EDID_DYNAMIC_HDR_T {
	EDID_SUPPORT_PHILIPS_HDR = (1 << 0),
	EDID_SUPPORT_DV_HDR = (1 << 1),
	EDID_SUPPORT_YUV422_12BIT = (1 << 2),
	EDID_SUPPORT_DV_HDR_2160P60 = (1 << 3),
	EDID_SUPPORT_HDR10_PLUS = (1 << 4),
};

/* This HDMI_SINK_VIDEO_COLORIMETRY_T will define what kind of YCBCR */
/* can be supported by sink. */
/* And each bit also defines the colorimetry data block of EDID. */
#define SINK_YCBCR_444 (1<<0)
#define SINK_YCBCR_422 (1<<1)
#define SINK_XV_YCC709 (1<<2)
#define SINK_XV_YCC601 (1<<3)
#define SINK_METADATA0 (1<<4)
#define SINK_METADATA1 (1<<5)
#define SINK_METADATA2 (1<<6)
#define SINK_RGB       (1<<7)
#define SINK_COLOR_SPACE_BT2020_CYCC  (1<<8)
#define SINK_COLOR_SPACE_BT2020_YCC  (1<<9)
#define SINK_COLOR_SPACE_BT2020_RGB  (1<<10)
#define SINK_YCBCR_420 (1<<11)
#define SINK_YCBCR_420_CAPABILITY (1<<12)

#define SINK_METADATA3 (1<<13)
#define SINK_S_YCC601     (1<<14)
#define SINK_ADOBE_YCC601  (1<<15)
#define SINK_ADOBE_RGB  (1<<16)

/* Static hdr supported by Sink */
enum EDID_STATIC_HDR_T {
	EDID_SUPPORT_SDR = (1 << 0),
	EDID_SUPPORT_HDR = (1 << 1),
	EDID_SUPPORT_SMPTE_ST_2084 = (1 << 2),
	EDID_SUPPORT_FUTURE_EOTF = (1 << 3),
	EDID_SUPPORT_ET_4 = (1 << 4),
	EDID_SUPPORT_ET_5 = (1 << 5),
};

/* HDMI_SINK_VCDB_T Each bit defines the
 *VIDEO Capability Data Block of EDID.
 */
#define SINK_CE_ALWAYS_OVERSCANNED                  (1<<0)
#define SINK_CE_ALWAYS_UNDERSCANNED                 (1<<1)
#define SINK_CE_BOTH_OVER_AND_UNDER_SCAN            (1<<2)
#define SINK_IT_ALWAYS_OVERSCANNED                  (1<<3)
#define SINK_IT_ALWAYS_UNDERSCANNED                 (1<<4)
#define SINK_IT_BOTH_OVER_AND_UNDER_SCAN            (1<<5)
#define SINK_PT_ALWAYS_OVERSCANNED                  (1<<6)
#define SINK_PT_ALWAYS_UNDERSCANNED                 (1<<7)
#define SINK_PT_BOTH_OVER_AND_UNDER_SCAN            (1<<8)
#define SINK_RGB_SELECTABLE                         (1<<9)

enum HDMI_AUDIO_SAMPLING_T {
	HDMI_FS_32K = 0,
	HDMI_FS_44K,
	HDMI_FS_48K,
	HDMI_FS_88K,
	HDMI_FS_96K,
	HDMI_FS_176K,
	HDMI_FS_192K
};

enum PCM_BIT_SIZE_T {
	PCM_16BIT = 0,
	PCM_20BIT,
	PCM_24BIT
};

struct HDMI_EDID_T {
	/* use EDID_VIDEO_RES_T, there are many resolution */
	unsigned int ui4_ntsc_resolution;
	unsigned int ui4_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_native_ntsc_resolution;
	/* use EDID_VIDEO_RES_T, only one NTSC resolution,
	 *Zero means none native NTSC resolution is aviable
	 */
	unsigned int ui4_sink_native_pal_resolution;
	/* use EDID_VIDEO_RES_T, only one resolution,
	 *Zero means none native PAL resolution is aviable
	 */
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_cea_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_cea_pal_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_pal_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_pal_resolution;
	/* use EDID_VIDEO_COLORIMETRY_T */
	unsigned int ui4_sink_colorimetry;
	unsigned char ui1_sink_rgb_color_bit;	/* color bit for RGB */
	unsigned char ui1_sink_ycbcr_color_bit;	/* color bit for YCbCr */
	unsigned char ui1_sink_dc420_color_bit;
	unsigned short ui2_sink_aud_dec;	/* use EDID_AUDIO_DECODER_T */
	unsigned char ui1_sink_is_plug_in;	/* 1: Plug in 0:Plug Out */
	unsigned int ui4_hdmi_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	/* use EDID_A_FMT_CH_TYPE1 */
	unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_type;
	unsigned int ui4_hdmi_ac3_ch_type;	/* use AVD_AC3_CH_TYPE */
	/* use AVD_AC3_CH_TYPE1 */
	unsigned int ui4_hdmi_ac3_ch3ch4ch5ch7_type;
	unsigned int ui4_hdmi_ec3_ch_type;	/* AVD_DOLBY_PLUS_CH_TYPE */
	/* AVD_DOLBY_PLUS_CH_TYPE1 */
	unsigned int ui4_hdmi_ec3_ch3ch4ch5ch7_type;
	unsigned int ui4_hdmi_dts_ch_type;
	unsigned int ui4_hdmi_dts_ch3ch4ch5ch7_type;
	unsigned int ui4_hdmi_dts_hd_ch_type;
	unsigned int ui4_hdmi_dts_hd_ch3ch4ch5ch7_type;
	unsigned int ui4_hdmi_pcm_bit_size;
	unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_bit_size;
	/* use EDID_A_FMT_CH_TYPE */
	unsigned int ui4_dac_pcm_ch_type;
	unsigned char ui1_sink_support_dolby_atoms;
	unsigned char ui1_sink_i_latency_present;
	unsigned int ui1_sink_p_audio_latency;
	unsigned int ui1_sink_p_video_latency;
	unsigned int ui1_sink_i_audio_latency;
	unsigned int ui1_sink_i_video_latency;
	unsigned char ui1ExtEdid_Revision;
	unsigned char ui1Edid_Version;
	unsigned char ui1Edid_Revision;
	unsigned char ui1_Display_Horizontal_Size;
	unsigned char ui1_Display_Vertical_Size;
	unsigned int ui4_ID_Serial_Number;
	unsigned int ui4_sink_cea_3D_resolution;
	/* 0: not support AI, 1:support AI */
	unsigned char ui1_sink_support_ai;
	unsigned short ui2_sink_cec_address;
	unsigned short ui1_sink_max_tmds_clock;
	unsigned short ui2_sink_3D_structure;
	unsigned int ui4_sink_cea_FP_SUP_3D_resolution;
	unsigned int ui4_sink_cea_TOB_SUP_3D_resolution;
	unsigned int ui4_sink_cea_SBS_SUP_3D_resolution;
	unsigned short ui2_sink_ID_manufacturer_name;/* (08H~09H) */
	unsigned short ui2_sink_ID_product_code;	/* (0aH~0bH) */
	unsigned int ui4_sink_ID_serial_number;	/* (0cH~0fH) */
	unsigned char ui1_sink_week_of_manufacture;	/* (10H) */
	/* (11H)  base on year 1990 */
	unsigned char ui1_sink_year_of_manufacture;
	unsigned char b_sink_SCDC_present;
	unsigned char b_sink_LTE_340M_sramble;
	unsigned int ui4_sink_hdmi_4k2kvic;
	unsigned short ui2_sink_max_tmds_character_rate;
	unsigned char ui1_sink_support_static_hdr;
	unsigned char ui1_sink_support_dynamic_hdr;
	unsigned char ui1_sink_hdr10plus_app_version;
	unsigned char ui1_sink_hdr_content_max_luminance;
	unsigned char ui1_sink_hdr_content_max_frame_average_luminance;
	unsigned char ui1_sink_hdr_content_min_luminance;
	unsigned int ui4_sink_dv_vsvdb_length;
	unsigned int ui4_sink_dv_vsvdb_version;
	unsigned int ui4_sink_dv_vsvdb_v1_low_latency;
	unsigned int ui4_sink_dv_vsvdb_v2_interface;
	unsigned int ui4_sink_dv_vsvdb_low_latency_support;
	unsigned int ui4_sink_dv_vsvdb_v2_supports_10b_12b_444;
	unsigned int ui4_sink_dv_vsvdb_support_backlight_control;
	unsigned int ui4_sink_dv_vsvdb_backlt_min_lumal;
	unsigned int ui4_sink_dv_vsvdb_tmin;
	unsigned int ui4_sink_dv_vsvdb_tmax;
	unsigned int ui4_sink_dv_vsvdb_tminPQ;
	unsigned int ui4_sink_dv_vsvdb_tmaxPQ;
	unsigned int ui4_sink_dv_vsvdb_Rx;
	unsigned int ui4_sink_dv_vsvdb_Ry;
	unsigned int ui4_sink_dv_vsvdb_Gx;
	unsigned int ui4_sink_dv_vsvdb_Gy;
	unsigned int ui4_sink_dv_vsvdb_Bx;
	unsigned int ui4_sink_dv_vsvdb_By;
	unsigned int ui4_sink_dv_vsvdb_Wx;
	unsigned int ui4_sink_dv_vsvdb_Wy;
	unsigned char ui1_sink_dv_block[32];
	unsigned char ui1rawdata_edid[256];
	unsigned char u1_sink_allm_support;
};


enum HDMI_SHARE_INFO_TYPE_T {
	SI_EDID_VSDB_EXIST = 0,
	SI_HDMI_RECEIVER_STATUS,
	SI_HDMI_PORD_OFF_PLUG_ONLY,
	SI_EDID_EXT_BLOCK_NO,
	SI_EDID_PARSING_RESULT,
	SI_HDMI_SUPPORTS_AI,
	SI_HDMI_HDCP_RESULT,
	SI_REPEATER_DEVICE_COUNT,
	SI_HDMI_CEC_LA,
	SI_HDMI_CEC_ACTIVE_SOURCE,
	SI_HDMI_CEC_PROCESS,
	SI_HDMI_CEC_PARA0,
	SI_HDMI_CEC_PARA1,
	SI_HDMI_CEC_PARA2,
	SI_HDMI_NO_HDCP_TEST,
	SI_HDMI_SRC_CONTROL,
	SI_A_CODE_MODE,
	SI_EDID_AUDIO_CAPABILITY,
	SI_HDMI_AUDIO_INPUT_SOURCE,
	SI_HDMI_AUDIO_CH_NUM,
	SI_HDMI_DVD_AUDIO_PROHIBIT,
	SI_DVD_HDCP_REVOCATION_RESULT
};
#define MAX_HDMI_SHAREINFO  64

extern void hdmi_checkedid(void);
extern unsigned char hdmi_fgreadedid(unsigned char i1noedid);
extern void vShowEdidInformation(void);
extern void vClearEdidInfo(void);
extern void hdmi_AppGetEdidInfo(struct HDMI_EDID_T *pv_get_info);
extern long long hdmi_DispGetEdidInfo(void);
extern long long vDispGetHdmiResolution(void);
extern unsigned char vCheckPcmBitSize(unsigned char ui1ChNumInx);
extern void hdmi_clear_edid_data(void);
extern unsigned char hdmi_check_edid_header(void);

extern unsigned int i4SharedInfo(unsigned int u4Index);
extern void vSetSharedInfo(unsigned int u4Index, unsigned int i4Value);

extern struct HDMI_SINK_AV_CAP_T _HdmiSinkAvCap;

#endif
