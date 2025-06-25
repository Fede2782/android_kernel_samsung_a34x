/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DRM_DP_COMMON_H__
#define __MTK_DRM_DP_COMMON_H__

#define ATTACH_BRIDGE 1

#define DP_SUPPORT_MAX_LINKRATE	DP_LINK_RATE_HBR3
#define DP_SUPPORT_MAX_LANECOUNT	DP_2LANE

#define CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT 1
#define DP_DRM_COMMON	0

#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>

#include "../mtk_drm_ddp_comp.h"

#define ENABLE_SERDES_MST 1

#define DP_ENCODER_NUM 2
#define DP_PAYLOAD_MAX	8

#define DP_PHY_LEVEL_COUNT            10
#define DP_REG_OFFSET(a)		((a) * 0xA00)

#define HPD_CONNECT			BIT(0)
#define HPD_DISCONNECT			BIT(10)
#define HPD_INITIAL_STATE		0
#define HPD_INT_EVNET			BIT(2)

#define PHY_READ_2BYTE(mtk_dp, reg) mtk_dp_phy_read(mtk_dp, reg)
#define PHY_READ_4BYTE(mtk_dp, reg) mtk_dp_phy_read(mtk_dp, reg)
#define PHY_READ_BYTE(mtk_dp, reg) mtk_dp_phy_read(mtk_dp, reg)
#define PHY_WRITE_2BYTE(mtk_dp, reg, val) \
	mtk_dp_phy_mask(mtk_dp, reg, val, 0xFFFF)
#define PHY_WRITE_4BYTE(mtk_dp, reg, val) \
	mtk_dp_phy_write(mtk_dp, reg, val)
#define PHY_WRITE_BYTE(mtk_dp, reg, val) \
	mtk_dp_phy_write_byte(mtk_dp, reg, val, 0xFF)
#define PHY_WRITE_2BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_phy_mask(mtk_dp, addr, val, mask)
#define PHY_WRITE_4BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_phy_mask(mtk_dp, addr, val, mask)
#define PHY_WRITE_BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_phy_write_byte(mtk_dp, addr, val, mask)
#define READ_2BYTE(mtk_dp, reg) mtk_dp_read(mtk_dp, reg)
#define READ_4BYTE(mtk_dp, reg) mtk_dp_read(mtk_dp, reg)
#define READ_BYTE(mtk_dp, reg) mtk_dp_read(mtk_dp, reg)
#define WRITE_2BYTE(mtk_dp, reg, val) \
	mtk_dp_mask(mtk_dp, reg, val, 0xFFFF)
#define WRITE_4BYTE(mtk_dp, reg, val) \
	mtk_dp_write(mtk_dp, reg, val)
#define WRITE_BYTE(mtk_dp, reg, val) \
	mtk_dp_write_byte(mtk_dp, reg, val, 0xFF)
#define WRITE_2BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_mask(mtk_dp, addr, val, mask)
#define WRITE_4BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_mask(mtk_dp, addr, val, mask)
#define WRITE_BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_write_byte(mtk_dp, addr, val, mask)

#define DP_ERR(fmt, arg...)	\
		pr_info("[DP]"pr_fmt(fmt), ##arg)
#define DP_FUNC(fmt, arg...)	\
		pr_info("[DP][%s]"pr_fmt(fmt), __func__, ##arg)
#define DP_DBG(fmt, arg...)	\
		//pr_info("[DP]"pr_fmt(fmt), ##arg)
#define DP_MSG(fmt, arg...)	\
		pr_info("[DP]"pr_fmt(fmt), ##arg)

enum audio_fs {
	FS_22K = 0x0,
	FS_32K = 0x1,
	FS_44K = 0x2,
	FS_48K = 0x3,
	FS_88K = 0x4,
	FS_96K = 0x5,
	FS_176K = 0x6,
	FS_192K = 0x7,
	FS_MAX,
};

enum audio_len {
	WL_16BIT = 1,
	WL_20BIT = 2,
	WL_24BIT = 3,
	WL_MAX,
};

enum dp_color_depth {
	DP_COLOR_DEPTH_6BIT = 0,
	DP_COLOR_DEPTH_8BIT = 1,
	DP_COLOR_DEPTH_10BIT = 2,
	DP_COLOR_DEPTH_12BIT = 3,
	DP_COLOR_DEPTH_16BIT = 4,
	DP_COLOR_DEPTH_UNKNOWN = 5,
};

enum dp_color_format {
	DP_COLOR_FORMAT_RGB_444 = 0,
	DP_COLOR_FORMAT_YUV_422 = 1,
	DP_COLOR_FORMAT_YUV_444 = 2,
	DP_COLOR_FORMAT_YUV_420 = 3,
	DP_COLOR_FORMAT_YONLY = 4,
	DP_COLOR_FORMAT_RAW = 5,
	DP_COLOR_FORMAT_RESERVED = 6,
	DP_COLOR_FORMAT_DEFAULT = DP_COLOR_FORMAT_RGB_444,
	DP_COLOR_FORMAT_UNKNOWN = 15,
};

enum dp_stream_id {
	DP_STREAM_ID_0 = 0x0,
#if (DP_ENCODER_NUM > 1)
	DP_STREAM_ID_1 = 0x1,
#endif
	DP_STREAM_MAX,
};

enum dp_encoder_id {
	DP_ENCODER_ID_0 = 0,
#if (DP_ENCODER_NUM > 1)
	DP_ENCODER_ID_1 = 1,
#endif
	DP_ENCODER_ID_MAX
};

enum dp_lane_count {
	DP_1LANE = 0x01,
	DP_2LANE = 0x02,
	DP_4LANE = 0x04,
};

enum dp_link_rate {
	DP_LINK_RATE_RBR = 0x6,
	DP_LINK_RATE_HBR = 0xA,
	DP_LINK_RATE_HBR2 = 0x14,
	DP_LINK_RATE_HBR25 = 0x19,
	DP_LINK_RATE_HBR3 = 0x1E,
};

enum dp_pg_typesel {
	DP_PG_NONE = 0x0,
	DP_PG_PURE_COLOR = 0x1,
	DP_PG_VERTICAL_RAMPING = 0x2,
	DP_PG_HORIZONTAL_RAMPING = 0x3,
	DP_PG_VERTICAL_COLOR_BAR = 0x4,
	DP_PG_HORIZONTAL_COLOR_BAR = 0x5,
	DP_PG_CHESSBOARD_PATTERN = 0x6,
	DP_PG_SUB_PIXEL_PATTERN = 0x7,
	DP_PG_FRAME_PATTERN = 0x8,
	DP_PG_MAX,
};

enum dp_source_type {
	DP_SRC_DPINTF = 0,
	DP_SRC_PG = 1,
};

enum dp_training_state {
	DP_TRAINING_STATE_STARTUP = 0x0,
	DP_TRAINING_STATE_CHECKCAP = 0x1,
	DP_TRAINING_STATE_CHECKEDID = 0x2,
	DP_TRAINING_STATE_TRAINING_PRE = 0x3,
	DP_TRAINING_STATE_TRAINING = 0x4,
	DP_TRAINING_STATE_CHECKTIMING = 0x5,
	DP_TRAINING_STATE_NORMAL = 0x6,
	DP_TRAINING_STATE_POWERSAVE = 0x7,
	DP_TRAINING_STATE_DPIDLE = 0x8,
	DP_TRAINING_STATE_MAX,
};

enum dp_return_status {
	DP_RET_NOERR = 0,
	DP_RET_PLUG_OUT = 1,
	DP_RET_TIMEOUT = 2,
	DP_RET_AUTH_FAIL = 3,
	DP_RET_EDID_FAIL = 4,
	DP_RET_TRANING_FAIL = 5,
	DP_RET_RETRANING = 6,
	DP_RET_WAIT_TRIGGER = 7,
};

enum dp_state {
	DP_STATE_INITIAL = 0,
	DP_STATE_IDLE = 1,
	DP_STATE_PREPARE = 2,
	DP_STATE_NORMAL = 3,
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	DP_STATE_AUTH = 4,
#endif
};

enum dp_video_mode {
	DP_VIDEO_INTERLACE = 0,
	DP_VIDEO_PROGRESSIVE = 1,
};

enum dp_video_timing {
	SINK_640_480 = 0,
	SINK_800_600,
	SINK_1280_720,
	SINK_1280_960,
	SINK_1280_1024,
	SINK_1920_1080,
	SINK_1920_1080_120_RB,
	SINK_1920_1080_120,
	SINK_1080_2460,
	SINK_1920_1200,
	SINK_1920_1440,
	SINK_2560_1440,
	SINK_2560_1600,
	SINK_3840_2160_30,
	SINK_3840_2160,
	SINK_7680_4320,
	SINK_MAX,
};

union dp_misc {
	struct {
		u8 is_sync_clock : 1;
		u8 color_format : 2;
		u8 spec_def1 : 2;
		u8 color_depth : 3;

		u8 interlaced : 1;
		u8 stereo_attr : 2;
		u8 reserved : 3;
		u8 is_vsc_sdp : 1;
		u8 spec_def2 : 1;
	} misc;
	u8 misc_raw[2];
};

union dp_pps {
	struct{
		u8 major : 4;
		u8 minor : 4;               /* pps0 */
		u8 pps_id : 8;              /* pps1 */
		u8 reserved1 : 8;           /* pps2 */
		u8 color_depth : 4;
		u8 buffer_depth : 4;        /* pps3 */
		u8 reserved2 : 2;
		bool bp_enable : 1;
		bool convert_rgb : 1;
		bool simple_422 : 1;
		bool vbr_enable : 1;
		u16 bit_per_pixel : 10;      /* pps4-5 */
		u16 pic_height : 16;         /* pps6-7 */
		u16 pic_width : 16;          /* pps8-9 */
		u16 slice_height : 16;       /* pps10-11 */
		u16 slice_width : 16;        /* pps12-13 */
		u16 chunk_size : 16;         /* pps14-15 */
		u8 reserved3 : 6;
		u16 init_xmit_delay : 10;    /* pps16-17 */
		u16 init_dec_delay : 16;     /* pps18-19 */
		u16 reserved4 : 10;
		u8 init_scale_val : 6;       /* pps20-21 */
		u16 scale_inc_interval : 16; /* pps22-23 */
		u8 reserved5 : 4;
		u16 scale_dec_interval : 12; /* pps24-25 */
		u16 reserved6 : 11;
		u8 first_line_offset : 5;    /* pps26-27 */
		u16 nfl_bpg_offset : 16;     /* pps28-29 */
		u16 slice_bpg_offset : 16;   /* pps30-31 */
		u16 init_offset : 16;        /* pps32-33 */
		u16 final_offset : 16;       /* pps34-35 */
		u8 reserved7 : 3;
		u8 min_qp : 5;               /* pps36 */
		u8 reserved8 : 3;
		u8 max_qp : 5;               /* pps37 */
		u8 rc_param_set[50];         /* pps38-87 */

		u8 reserved9 : 6;
		bool native_420 : 1;
		bool native_422 : 1;         /* pps88 */
		u8 reserved10 : 3;
		u8 sec_line_bpg_offset : 5;  /* pps89 */
		u16 nsl_bpg_offset : 16;     /* pps90-91 */
		u16 sec_line_offset : 16;    /* pps92-93 */
		u8 reserved11[34];           /* pps94-127 */
	} pps;

	u8 pps_raw[128];
};

struct mtk_dp_efuse_fmt {
	unsigned short idx;
	unsigned short shift;
	unsigned short mask;
	unsigned short min_val;
	unsigned short max_val;
	unsigned short default_val;
};

struct mtk_dp_data {
	int bridge_type;
	unsigned int smc_cmd;
	const struct mtk_dp_efuse_fmt *efuse_fmt;
	bool audio_support;
	bool audio_pkt_in_hblank_area;
	u16 audio_m_div2_bit;
};

struct dp_timing_parameter {
	u16 htt;
	u16 hde;
	u16 hbk;
	u16 hfp;
	u16 hsw;

	bool hsp;
	u16 hbp;
	u16 vtt;
	u16 vde;
	u16 vbk;
	u16 vfp;
	u16 vsw;

	bool vsp;
	u16 vbp;
	u8 frame_rate;
	u64 pixcel_rate;
	int video_ip_mode;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	union dp_misc misc;
#endif
};

struct dp_info {
	u8 input_src;
	u8 depth;
	u8 format;
	u8 resolution;
	unsigned int audio_cap;
	unsigned int audio_config;
	struct dp_timing_parameter dp_output_timing;
	unsigned int clk_debug;
	unsigned int clk_src;
	unsigned int con;

	bool pattern_gen : 1;
	bool sink_ssc_en : 1;
	bool set_audio_mute : 1;
	bool set_video_mute : 1;
	bool audio_mute : 1;
	bool video_mute : 1;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	u8 pattern_id;
#endif
};

struct dp_training_info {
	bool sink_ext_cap_en : 1;
	bool tps3_support : 1;
	bool tps4_support : 1;
	bool sink_ssc_en : 1;
	bool dp_tx_auto_test_en : 1;
	bool cable_plug_in : 1;
	bool cable_state_change : 1;
	bool dp_mst_cap : 1;
	bool dp_mst_branch : 1;
	bool dwn_strm_port_present : 1;
	bool cr_done : 1;
	bool eq_done : 1;

	u8 dp_version;
	u8 max_link_rate;
	u8 max_link_lane_count;
	u8 link_rate;
	u8 link_lane_count;
	u16 phy_status;
	u8 dpcd_rev;
	u8 sink_count;
	u8 check_cap_times;
	u8 ssc_delta;
};

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
struct dp_stream_info {
	u8 ideal_timing;
	u8 final_timing;
	u8 color_depth;
	u8 color_format;
	u8 pg_type;
	u8 audio_freq;
	u8 audio_ch;
	bool is_dsc;
	struct mtk_drm_dp_mst_port *port;
};
#endif

struct dp_phy_parameter {
	unsigned char c0;
	unsigned char cp1;
};

struct mtk_dp_connector {
	struct mtk_dp *mtk_dp;
	struct drm_dp_mst_port *port;
	struct drm_connector connector;
	struct drm_encoder *encoder;

	struct drm_display_mode mode;
	struct edid *edid;
};

struct mtk_drm_dp_vcpi {
	int vcpi;
	int pbn;
	int aligned_pbn;
	int num_slots;
};

struct mtk_drm_dp_mst_port {
	/**
	 * @topology_kref: refcount for this port's lifetime in the topology,
	 * only the DP MST helpers should need to touch this
	 */
	struct kref topology_kref;

	/**
	 * @malloc_kref: refcount for the memory allocation containing this
	 * structure. See mtk_drm_dp_mst_get_port_malloc() and
	 * mtk_drm_dp_mst_put_port_malloc().
	 */
	struct kref malloc_kref;

	/**
	 * @topology_ref_history: A history of each topology
	 * reference/dereference. See CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS.
	 */
//	struct mtk_drm_dp_mst_topology_ref_history topology_ref_history;

	u8 port_num;
	bool input;
	bool mcs;
	bool ddps;
	u8 pdt;
	bool ldps;
	u8 dpcd_rev;
	u8 num_sdp_streams;
	u8 num_sdp_stream_sinks;
	u16 full_pbn;
	struct list_head next;
	/**
	 * @mstb: the branch device connected to this port, if there is one.
	 * This should be considered protected for reading by
	 * &mtk_drm_dp_mst_topology_mgr.lock. There are two exceptions to this:
	 * &mtk_drm_dp_mst_topology_mgr.up_req_work and
	 * &mtk_drm_dp_mst_topology_mgr.work, which do not grab
	 * &mtk_drm_dp_mst_topology_mgr.lock during reads but are the only
	 * updaters of this list and are protected from writing concurrently
	 * by &mtk_drm_dp_mst_topology_mgr.probe_lock.
	 */
	struct mtk_drm_dp_mst_branch *mstb;
	struct mtk_drm_dp_mst_branch *parent;

	struct mtk_drm_dp_vcpi vcpi;
	struct mtk_drm_dp_mst_topology_mgr *mgr;

	u8 *cached_edid;

	bool has_audio;
	bool fec_capable;
};

/* sideband msg header - not bit struct */
struct mtk_drm_dp_sideband_msg_hdr {
	u8 lct;
	u8 lcr;
	u8 rad[8];
	bool broadcast;
	bool path_msg;
	u8 msg_len;
	bool somt;
	bool eomt;
	bool seqno;
};

struct mtk_drm_dp_sideband_msg_rx {
	u8 chunk[48];
	u8 msg[256];
	u8 curchunk_len;
	u8 curchunk_idx; /* chunk we are parsing now */
	u8 curchunk_hdrlen;
	u8 curlen; /* total length of the msg */
	bool have_somt;
	bool have_eomt;
	struct mtk_drm_dp_sideband_msg_hdr initial_hdr;
};

struct mtk_drm_dp_mst_branch {
	/**
	 * @topology_kref: refcount for this branch device's lifetime in the
	 * topology, only the DP MST helpers should need to touch this
	 */
	struct kref topology_kref;

	/**
	 * @malloc_kref: refcount for the memory allocation containing this
	 * structure. See mtk_drm_dp_mst_get_mstb_malloc() and
	 * mtk_drm_dp_mst_put_mstb_malloc().
	 */
	struct kref malloc_kref;

	/**
	 * @topology_ref_history: A history of each topology
	 * reference/dereference. See CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS.
	 */
//	struct mtk_drm_dp_mst_topology_ref_history topology_ref_history;

	/**
	 * @destroy_next: linked-list entry used by
	 * mtk_drm_dp_delayed_destroy_work()
	 */
	struct list_head destroy_next;

	u8 rad[8];
	u8 lct;
	int num_ports;

	struct list_head ports;
	struct mtk_drm_dp_mst_port *port_parent;
	struct mtk_drm_dp_mst_topology_mgr *mgr;

	bool link_address_sent;

	/* global unique identifier to identify branch devices */
	u8 guid[16];
};

struct mtk_drm_dp_nak_reply {
	u8 guid[16];
	u8 reason;
	u8 nak_data;
};

struct mtk_drm_dp_link_address_ack_reply {
	u8 guid[16];
	u8 nports;
	struct mtk_drm_dp_link_addr_reply_port {
		bool input_port;
		u8 peer_device_type;
		u8 port_number;
		bool mcs;
		bool ddps;
		bool legacy_device_plug_status;
		u8 dpcd_revision;
		u8 peer_guid[16];
		u8 num_sdp_streams;
		u8 num_sdp_stream_sinks;
	} ports[16];
};

struct mtk_drm_dp_remote_dpcd_read_ack_reply {
	u8 port_number;
	u8 num_bytes;
	u8 bytes[255];
};

struct mtk_drm_dp_remote_dpcd_write_ack_reply {
	u8 port_number;
};

struct mtk_drm_dp_remote_dpcd_write_nak_reply {
	u8 port_number;
	u8 reason;
	u8 bytes_written_before_failure;
};

struct mtk_drm_dp_remote_i2c_read_ack_reply {
	u8 port_number;
	u8 num_bytes;
	u8 bytes[255];
};

struct mtk_drm_dp_remote_i2c_read_nak_reply {
	u8 port_number;
	u8 nak_reason;
	u8 i2c_nak_transaction;
};

struct mtk_drm_dp_remote_i2c_write_ack_reply {
	u8 port_number;
};

struct mtk_drm_dp_query_stream_enc_status_ack_reply {
	/* Bit[23:16]- Stream Id */
	u8 stream_id;

	/* Bit[15]- Signed */
	bool reply_signed;

	/* Bit[10:8]- Stream Output Sink Type */
	bool unauthorizable_device_present;
	bool legacy_device_present;
	bool query_capable_device_present;

	/* Bit[12:11]- Stream Output CP Type */
	bool hdcp_1x_device_present;
	bool hdcp_2x_device_present;

	/* Bit[4]- Stream Authentication */
	bool auth_completed;

	/* Bit[3]- Stream Encryption */
	bool encryption_enabled;

	/* Bit[2]- Stream Repeater Function Present */
	bool repeater_present;

	/* Bit[1:0]- Stream State */
	u8 state;
};

#define DRM_DP_MAX_SDP_STREAMS 16
struct mtk_drm_dp_allocate_payload {
	u8 port_number;
	u8 number_sdp_streams;
	u8 vcpi;
	u16 pbn;
	u8 sdp_stream_sink[DRM_DP_MAX_SDP_STREAMS];
};

struct mtk_drm_dp_allocate_payload_ack_reply {
	u8 port_number;
	u8 vcpi;
	u16 allocated_pbn;
};

struct mtk_drm_dp_connection_status_notify {
	u8 guid[16];
	u8 port_number;
	bool legacy_device_plug_status;
	bool displayport_device_plug_status;
	bool message_capability_status;
	bool input_port;
	u8 peer_device_type;
};

struct mtk_drm_dp_remote_dpcd_read {
	u8 port_number;
	u32 dpcd_address;
	u8 num_bytes;
};

struct mtk_drm_dp_remote_dpcd_write {
	u8 port_number;
	u32 dpcd_address;
	u8 num_bytes;
	u8 *bytes;
};

#define DP_REMOTE_I2C_READ_MAX_TRANSACTIONS 4
struct mtk_drm_dp_remote_i2c_read {
	u8 num_transactions;
	u8 port_number;
	struct mtk_drm_dp_remote_i2c_read_tx {
		u8 i2c_dev_id;
		u8 num_bytes;
		u8 *bytes;
		u8 no_stop_bit;
		u8 i2c_transaction_delay;
	} transactions[DP_REMOTE_I2C_READ_MAX_TRANSACTIONS];
	u8 read_i2c_device_id;
	u8 num_bytes_read;
};

struct mtk_drm_dp_remote_i2c_write {
	u8 port_number;
	u8 write_i2c_device_id;
	u8 num_bytes;
	u8 *bytes;
};

struct mtk_drm_dp_query_stream_enc_status {
	u8 stream_id;
	u8 client_id[7];	/* 56-bit nonce */
	u8 stream_event;
	bool valid_stream_event;
	u8 stream_behavior;
	u8 valid_stream_behavior;
};

/* this covers ENUM_RESOURCES, POWER_DOWN_PHY, POWER_UP_PHY */
struct mtk_drm_dp_port_number_req {
	u8 port_number;
};

struct mtk_drm_dp_enum_path_resources_ack_reply {
	u8 port_number;
	bool fec_capable;
	u16 full_payload_bw_number;
	u16 avail_payload_bw_number;
};

/* covers POWER_DOWN_PHY, POWER_UP_PHY */
struct mtk_drm_dp_port_number_rep {
	u8 port_number;
};

struct mtk_drm_dp_query_payload {
	u8 port_number;
	u8 vcpi;
};

struct mtk_drm_dp_resource_status_notify {
	u8 port_number;
	u8 guid[16];
	u16 available_pbn;
};

struct mtk_drm_dp_query_payload_ack_reply {
	u8 port_number;
	u16 allocated_pbn;
};

struct mtk_drm_dp_sideband_msg_req_body {
	u8 req_type;
	union mtk_ack_req {
		struct mtk_drm_dp_connection_status_notify conn_stat;
		struct mtk_drm_dp_port_number_req port_num;
		struct mtk_drm_dp_resource_status_notify resource_stat;

		struct mtk_drm_dp_query_payload query_payload;
		struct mtk_drm_dp_allocate_payload allocate_payload;

		struct mtk_drm_dp_remote_dpcd_read dpcd_read;
		struct mtk_drm_dp_remote_dpcd_write dpcd_write;

		struct mtk_drm_dp_remote_i2c_read i2c_read;
		struct mtk_drm_dp_remote_i2c_write i2c_write;

		struct mtk_drm_dp_query_stream_enc_status enc_status;
	} u;
};

struct mtk_drm_dp_sideband_msg_reply_body {
	u8 reply_type;
	u8 req_type;
	union mtk_ack_replies {
		struct mtk_drm_dp_nak_reply nak;
		struct mtk_drm_dp_link_address_ack_reply link_addr;
		struct mtk_drm_dp_port_number_rep port_number;

		struct mtk_drm_dp_enum_path_resources_ack_reply path_resources;
		struct mtk_drm_dp_allocate_payload_ack_reply allocate_payload;
		struct mtk_drm_dp_query_payload_ack_reply query_payload;

		struct mtk_drm_dp_remote_dpcd_read_ack_reply remote_dpcd_read_ack;
		struct mtk_drm_dp_remote_dpcd_write_ack_reply remote_dpcd_write_ack;
		struct mtk_drm_dp_remote_dpcd_write_nak_reply remote_dpcd_write_nack;

		struct mtk_drm_dp_remote_i2c_read_ack_reply remote_i2c_read_ack;
		struct mtk_drm_dp_remote_i2c_read_nak_reply remote_i2c_read_nack;
		struct mtk_drm_dp_remote_i2c_write_ack_reply remote_i2c_write_ack;

		struct mtk_drm_dp_query_stream_enc_status_ack_reply enc_status;
	} u;
};

/* msg is queued to be put into a slot */
#define DRM_DP_SIDEBAND_TX_QUEUED 0
/* msg has started transmitting on a slot - still on msgq */
#define DRM_DP_SIDEBAND_TX_START_SEND 1
/* msg has finished transmitting on a slot - removed from msgq only in slot */
#define DRM_DP_SIDEBAND_TX_SENT 2
/* msg has received a response - removed from slot */
#define DRM_DP_SIDEBAND_TX_RX 3
#define DRM_DP_SIDEBAND_TX_TIMEOUT 4

struct mtk_drm_dp_sideband_msg_tx {
	u8 msg[256];
	u8 chunk[48];
	u8 cur_offset;
	u8 cur_len;
	struct mtk_drm_dp_mst_branch *dst;
	struct list_head next;
	int seqno;
	int state;
	bool path_msg;
	struct mtk_drm_dp_sideband_msg_reply_body reply;
};

struct mtk_drm_dp_pending_up_req {
	struct mtk_drm_dp_sideband_msg_hdr hdr;
	struct mtk_drm_dp_sideband_msg_req_body msg;
	struct list_head next;
};

/* sideband msg handler */
struct mtk_drm_dp_mst_topology_mgr;
struct mtk_drm_dp_mst_topology_cbs {
	/*
	 * Checks for any pending MST interrupts, passing them to MST core for
	 * processing, the same way an HPD IRQ pulse handler would do this.
	 * If provided MST core calls this callback from a poll-waiting loop
	 * when waiting for MST down message replies. The driver is expected
	 * to guard against a race between this callback and the driver's HPD
	 * IRQ pulse handler.
	 */
	void (*poll_hpd_irq)(struct mtk_drm_dp_mst_topology_mgr *mgr);
};

#define DP_MAX_PAYLOAD (sizeof(unsigned long) * 8)

#define DP_PAYLOAD_LOCAL 1
#define DP_PAYLOAD_REMOTE 2
#define DP_PAYLOAD_DELETE_LOCAL 3

struct mtk_drm_dp_payload {
	int payload_state;
	int start_slot;
	int num_slots;
	int vcpi;
};

//#define to_dp_mst_topology_state(x) container_of(x, struct mtk_drm_dp_mst_topology_state, base)

struct mtk_drm_dp_vcpi_allocation {
	struct mtk_drm_dp_mst_port *port;
	int vcpi;
	int pbn;
	bool dsc_enabled;
	struct list_head next;
};

struct mtk_drm_dp_mst_topology_state {
	struct list_head vcpis;
	struct mtk_drm_dp_mst_topology_mgr *mgr;
	u8 total_avail_slots;
	u8 start_slot;
};

//#define to_dp_mst_topology_mgr(x) container_of(x, struct mtk_drm_dp_mst_topology_mgr, base)

/**
 * struct mtk_drm_dp_mst_topology_mgr - DisplayPort MST manager
 *
 * This struct represents the toplevel displayport MST topology manager.
 * There should be one instance of this for every MST capable DP connector
 * on the GPU.
 */
struct mtk_drm_dp_mst_topology_mgr {
	const struct mtk_drm_dp_mst_topology_cbs *cbs;
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	u8 sink_count;
	int pbn_div;
	int max_dpcd_transaction_bytes;
	int max_payloads;
	int max_lane_count;
	int max_link_rate;
	int conn_base_id;
	bool mst_state : 1;
	bool payload_id_table_cleared : 1;
	unsigned long payload_mask;
	unsigned long vcpi_mask;
	int avail_pbn_int;
	int avail_pbn_ext;

	struct mtk_drm_dp_sideband_msg_rx up_req_recv;
	struct mtk_drm_dp_sideband_msg_rx down_rep_recv;
	struct mtk_drm_dp_mst_branch *mst_primary;
	struct list_head tx_msg_downq;
	struct mtk_drm_dp_vcpi **proposed_vcpis;
	struct mtk_drm_dp_payload *payloads;
	struct list_head destroy_port_list;
	struct list_head destroy_branch_device_list;
	struct list_head up_req_list;
	struct mtk_drm_dp_mst_topology_state state;
};

struct mtk_dp {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	const struct mtk_dp_data *data;
	struct drm_connector *conn;
	struct drm_encoder *enc;
	int id;
	struct edid *edid;
	struct drm_dp_aux aux;
	u8 rx_cap[16];
	struct drm_display_mode mode[DP_ENCODER_NUM];
	struct dp_info info[DP_ENCODER_NUM];
	int state;
	int state_pre;
	struct dp_training_info training_info;
	int training_state;
	int training_state_pre;

	struct workqueue_struct *dp_wq;
	struct work_struct dp_work;

	u32 min_clock;
	u32 max_clock;
	u32 max_hdisplay;
	u32 max_vdisplay;

	void __iomem *regs;
	void __iomem *phyd_regs;
	struct clk *dp_tx_clk;

	u32 uevent_to_hwc;
	int disp_state;
	bool power_on;
	bool audio_enable;
	bool video_enable;
	bool dp_ready;
	bool has_dsc;
	bool has_fec;
	bool dsc_enable;
	struct mtk_drm_private *priv;
	struct dp_phy_parameter phy_params[DP_PHY_LEVEL_COUNT];

	/* pmic vs voter */
	struct regmap *vsv;
	u32 vsv_reg;
	u32 vsv_mask;
	u32 vsv_vers;
	bool swap_enable;
	struct notifier_block nb;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	bool mst_enable;
	bool is_mst_start;
	bool is_mst_fec_en;
	struct drm_dp_mst_topology_mgr mgr;
	struct mtk_drm_dp_mst_topology_mgr mtk_mgr;
	struct dp_stream_info stream_info[DP_STREAM_MAX];
	struct mtk_dp_connector *mtk_connector[DP_ENCODER_NUM];
	struct drm_display_mode disp_mode;
#endif
};

u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset);
void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val);
void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask);
void mtk_dp_write_byte(struct mtk_dp *mtk_dp, u32 addr, u8 val, u32 mask);
u32 mtk_dp_phy_read(struct mtk_dp *mtk_dp, u32 offset);
void mtk_dp_phy_write(struct mtk_dp *mtk_dp, u32 offset, u32 val);
void mtk_dp_phy_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask);
void mtk_dp_phy_write_byte(struct mtk_dp *mtk_dp, u32 addr, u8 val, u32 mask);
void mtk_dp_fec_enable(struct mtk_dp *mtk_dp, bool enable);
void mtk_dp_video_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable);
void mtk_dp_audio_pg_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, u8 channel,
			    u8 fs, u8 enable);
void mtk_dp_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				u8 channel, u8 fs, u8 word_length);
void mtk_dp_audio_mdiv_set(struct mtk_dp *mtk_dp,
			   const enum dp_encoder_id encoder_id, u8 div);
void mtk_dp_audio_sdp_setting(struct mtk_dp *mtk_dp,
			      const enum dp_encoder_id encoder_id, u8 channel);
void mtk_dp_i2s_audio_config(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id);
void mtk_dp_audio_mute(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable);
void mtk_dp_video_mute(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable);
u8 mtk_dp_color_get_bpp(u8 color_format, u8 color_depth);
void mtk_dp_encoder_reset(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id);
#endif
