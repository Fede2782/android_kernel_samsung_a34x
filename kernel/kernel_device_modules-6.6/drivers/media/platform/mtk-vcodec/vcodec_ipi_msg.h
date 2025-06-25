/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 */

#ifndef _VCODEC_IPI_MSG_H_
#define _VCODEC_IPI_MSG_H_

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#ifdef VIDEO_MAX_FRAME
#undef VIDEO_MAX_FRAME
#define VIDEO_MAX_FRAME 64
#endif

#define IPIMSG_BASE_BITS 0xFF000
#define IPIMSG_TYPE_BITS 0x00F00
#define IPIMSG_ID_BITS   0x000FF
#define IPIMSG_NO_INST_OFFSET 0x100

#define AP_IPIMSG_VDEC_SEND_BASE 0xA000
#define VCU_IPIMSG_VDEC_ACK_BASE 0xB000
#define VCU_IPIMSG_VDEC_SEND_BASE 0xC000
#define AP_IPIMSG_VDEC_ACK_BASE 0xD000
#define VCU_IPIMSG_VDEC_SEND_ASYNC_BASE 0xE000

#define AP_IPIMSG_VENC_SEND_BASE 0x1000
#define VCU_IPIMSG_VENC_ACK_BASE 0x2000
#define VCU_IPIMSG_VENC_SEND_BASE 0x3000
#define AP_IPIMSG_VENC_ACK_BASE 0x4000

enum mtk_venc_hw_id {
	MTK_VENC_CORE_0 = 0,
	MTK_VENC_CORE_1 = 1,
	MTK_VENC_CORE_2 = 2,
	MTK_VENC_HW_NUM = 3,
};

enum mtk_vdec_hw_id {
	MTK_VDEC_CORE = 0,
	MTK_VDEC_LAT = 1,
	MTK_VDEC_CORE1 = 2,
	MTK_VDEC_LAT1 = 3,
	MTK_VDEC_HW_NUM = 4,
	MTK_VDEC_LINE_COUNT = 4,
	MTK_VDEC_IRQ_NUM = 5,
};

enum mtk_fmt_type {
	MTK_FMT_DEC = 0,
	MTK_FMT_ENC = 1,
	MTK_FMT_FRAME = 2,
};

enum mtk_frame_type {
	MTK_FRAME_NONE = 0,
	MTK_FRAME_I = 1,
	MTK_FRAME_P = 2,
	MTK_FRAME_B = 3,
};

enum mtk_bandwidth_type {
	MTK_AV1_COMPRESS = 0,
	MTK_AV1_NO_COMPRESS = 1,
	MTK_HEVC_COMPRESS = 2,
	MTK_HEVC_NO_COMPRESS = 3,
	MTK_VSD = 4,
	MTK_BW_COUNT = 5,
};

enum v4l2_vdec_trick_mode {
	/* decode all frame */
	V4L2_VDEC_TRICK_MODE_ALL = 0,
	/* decode all except of non-reference frame */
	V4L2_VDEC_TRICK_MODE_IP,
	/* only decode I frame */
	V4L2_VDEC_TRICK_MODE_I
};

enum mtk_dec_dtsi_m4u_port_idx {
	VDEC_M4U_PORT_MC,
	VDEC_M4U_PORT_UFO,
	VDEC_M4U_PORT_PP,
	VDEC_M4U_PORT_PRED_RD,
	VDEC_M4U_PORT_PRED_WR,
	VDEC_M4U_PORT_PPWRAP,
	VDEC_M4U_PORT_TILE,
	VDEC_M4U_PORT_VLD,
	VDEC_M4U_PORT_VLD2,
	VDEC_M4U_PORT_AVC_MV,
	VDEC_M4U_PORT_RG_CTRL_DMA,
	VDEC_M4U_PORT_UFO_ENC,
	VDEC_M4U_PORT_LAT0_VLD,
	VDEC_M4U_PORT_LAT0_VLD2,
	VDEC_M4U_PORT_LAT0_AVC_MV,
	VDEC_M4U_PORT_LAT0_PRED_RD,
	VDEC_M4U_PORT_LAT0_TILE,
	VDEC_M4U_PORT_LAT0_WDMA,
	VDEC_M4U_PORT_LAT0_RG_CTRL_DMA,
	VDEC_M4U_PORT_LAT0_MC,
	VDEC_M4U_PORT_LAT0_UFO,
	VDEC_M4U_PORT_LAT0_UFO_C,
	VDEC_M4U_PORT_LAT0_UNIWRAP,
	VDEC_M4U_PORT_UNIWRAP,
	VDEC_M4U_PORT_VIDEO_UP_SEC,
	VDEC_M4U_PORT_VIDEO_UP_NOR,
	VDEC_M4U_PORT_UP_1,
	VDEC_M4U_PORT_UP_2,
	VDEC_M4U_PORT_UP_3,
	VDEC_M4U_PORT_UP_4,
	NUM_MAX_VDEC_M4U_PORT
};

enum mtk_venc_smi_dump_mode {
	MTK_VENC_POWERCTL_IN_KERNEL = 0,
	MTK_VENC_POWERCTL_IN_VCP = 1
};

enum mtk_smi_pwr_ctrl_type {
	MTK_SMI_GET,
	MTK_SMI_GET_IF_IN_USE,
	MTK_SMI_PUT,
	MTK_SMI_CTRL_TYPE_MAX
};

// smaller value means higher priority
enum vdec_priority {
	VDEC_PRIORITY_REAL_TIME = 0,
	VDEC_PRIORITY_NON_REAL_TIME1,
	VDEC_PRIORITY_NON_REAL_TIME2,
	VDEC_PRIORITY_NON_REAL_TIME3,
	VDEC_PRIORITY_NUM,
};

/**
 * struct mtk_video_fmt - Structure used to store information about pixelformats
 */
struct mtk_video_fmt {
	__u32	fourcc;
	__u32	type;   /* enum mtk_fmt_type */
	__u32	num_planes;
	__u32	reserved;
};

/**
 * struct mtk_codec_framesizes - Structure used to store information about
 *							framesizes
 */
struct mtk_codec_framesizes {
	__u32	fourcc;
	__u32	profile;
	__u32	level;
	__u32	reserved;
	struct	v4l2_frmsize_stepwise	stepwise;
};

/**
 * struct mtk_codec_capability - Structure used to store information about capability
 */
struct mtk_codec_capability {
	__u32	max_b;
	__u32	max_temporal_layer;
};

struct mtk_tf_info {
	__u32	hw_id;
	__u32	port;
	__u64	tf_mva;
	__u32	has_tf;
};

struct mtk_vio_info {
	__u32	has_emi_vio; // emi mpu violation flag
};

struct mtk_smi_pwr_ctrl_info {
	__u32	type;
	__u32	hw_id;
	__s32	ret;
};

/**
 * struct mtk_video_frame_frameintervals - Structure used to store information about
 *							frameintervals
 * fourcc/width/height are input parameters
 * stepwise is output parameter
 */
struct mtk_video_frame_frameintervals {
	__u32   fourcc;
	__u32   width;
	__u32   height;
	struct v4l2_frmival_stepwise stepwise;
};

struct mtk_color_desc {
	__u32	color_primaries;
	__u32	transform_character;
	__u32	matrix_coeffs;
	__u32	display_primaries_x[3];
	__u32	display_primaries_y[3];
	__u32	white_point_x;
	__u32	white_point_y;
	__u32	max_display_mastering_luminance;
	__u32	min_display_mastering_luminance;
	__u32	max_content_light_level;
	__u32	max_pic_light_level;
	__u32	hdr_type;
	__u32	full_range;
	__u32	reserved;
};

struct v4l2_vdec_hdr10_info {
	__u8 matrix_coefficients;
	__u8 bits_per_channel;
	__u8 chroma_subsampling_horz;
	__u8 chroma_subsampling_vert;
	__u8 cb_subsampling_horz;
	__u8 cb_subsampling_vert;
	__u8 chroma_siting_horz;
	__u8 chroma_siting_vert;
	__u8 color_range;
	__u8 transfer_characteristics;
	__u8 colour_primaries;
	__u16 max_CLL;  // CLL: Content Light Level
	__u16 max_FALL; // FALL: Frame Average Light Level
	__u16 primaries[3][2];
	__u16 white_point[2];
	__u32 max_luminance;
	__u32 min_luminance;
};

struct v4l2_vdec_hdr10plus_data {
	__u64 addr; // user pointer
	__u32 size;
};

struct vdec_resource_info {
	__u32 usage; /* unit is 1/1000 */
	bool hw_used[MTK_VDEC_HW_NUM]; /* index MTK_VDEC_LAT is not used for now */
	bool gce;
	enum vdec_priority priority;
};

struct vdec_max_buf_info {
	/* set by user*/
	__u32 pixelformat;
	__u32 max_width;
	__u32 max_height;
	/* report by vdec*/
	__u32 max_internal_buf_size;
	__u32 max_dpb_count;
	__u32 max_frame_buf_size;
};

struct vdec_fmt_modifier {
	union {
		struct {
			__u8 tile:1;
			__u8 raster:1;
			__u8 compress:1;
			__u8 jump:1;
			__u8 vsd_mode:3; /* enum fmt_modifier_vsd */
			__u8 vsd_ce_mode:1;
			__u8 is_vsd_buf_layout:1;
			__u8 is_10bit:1;
			__u8 compress_engine:2; /* 0 : no compress, 1 : ufo, 2 : mfc */
			__u8 color_space_num:2; /* luma, chroma */
			__u8 size_multiplier:3; /* mpeg2 new deblocking mode would double fb size*/
			__u8 luma_target;  /* temp : 87 */
			__u8 chroma_target; /* temp : 66 */
		};
		/* TODO: Reserve several bits in MSB for version control */
		__u64 padding;
	};
};

struct vdec_bandwidth_info {
	/* unit is 1/1000 */
	/*[0] : av1 compress, [1] : av1 no compress */
	/*[2] : hevc compress, [3] : hevc no compress */
	/*[4] : vsd*/
	__u32 bandwidth_per_pixel[MTK_BW_COUNT];
	bool compress;
	bool vsd;
	struct vdec_fmt_modifier modifier;
};

struct mtk_venc_multi_ref {
	__u32	multi_ref_en;
	__u32	intra_period;
	__u32	superp_period;
	__u32	superp_ref_type;
	__u32	ref0_distance;
	__u32	ref1_dsitance;
	__u32	max_distance;
	__u32	reserved;
};

struct mtk_venc_vui_info {
	__u32	aspect_ratio_idc;
	__u32	sar_width;
	__u32	sar_height;
	__u32	reserved;
};

struct mtk_hdr_dynamic_info {
	__u32    max_sc_lR;
		// u(17); Max R Nits *10; in the range of 0x00000-0x186A0
	__u32    max_sc_lG;
		// u(17); Max G Nits *10; in the range of 0x00000-0x186A0
	__u32    max_sc_lB;
		// u(17); Max B Nits *10; in the range of 0x00000-0x186A0
	__u32    avg_max_rgb;
		// u(17); Average maxRGB Nits *10; in 0x00000-0x186A0
	__u32    distribution_values[9];
		/* u(17)
		 * 0=1% percentile maxRGB Nits *10
		 * 1=Maximum Nits of 99YF *10
		 * 2=Average Nits of DPY100F
		 * 3=25% percentile maxRGB Nits *10
		 * 4=50% percentile maxRGB Nits *10
		 * 5=75% percentile maxRGB Nits *10
		 * 6=90% percentile maxRGB Nits *10
		 * 7=95% percentile maxRGB Nits *10
		 * 8=99.95% percentile maxRGB Nits *10
		 */
	__u32   reserved;
};

struct dynamicinfo_change_flag {
	__u32 bitrate_status;
	__u32 frameqp_status;
	__u32 slice_header_spacing_status;
	__u32 forceI_status;
	__u32 baselayer_pid_status;
	__u32 markLTR_status;
	__u32 useLTR_status;
	__u32 temporal_Layer_Count_status;
};

struct temporal_layer_count {
	__u32 p_layercount;
	__u32 b_layercount;
};

struct inputqueue_dynamic_info {
	struct dynamicinfo_change_flag changed;
	__u32 bitrate;
	__u32 frameQp;
	__u32 slice_header_spacing;
	__u32 forceI;
	__u32 baselayer_pid;
	__u32 markLTR;
	__u32 useLTR;
	__u32 reserved;
	struct temporal_layer_count temporal_layer_count;
	__u64 timestamp;
};


enum mtk_dec_param {
	MTK_DEC_PARAM_NONE = 0,
	MTK_DEC_PARAM_DECODE_MODE = (1 << 0),
	MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE = (1 << 1),
	MTK_DEC_PARAM_CRC_PATH = (1 << 2),
	MTK_DEC_PARAM_GOLDEN_PATH = (1 << 3),
	MTK_DEC_PARAM_WAIT_KEY_FRAME = (1 << 4),
	MTK_DEC_PARAM_OPERATING_RATE = (1 << 5),
	MTK_DEC_PARAM_DECODE_ERROR_HANDLE_MODE = (1 << 6),
	MTK_DEC_PARAM_LINECOUNT_THRESHOLD = (1 << 7)
};

struct mtk_dec_params {
	__u32	dec_param_change;
	__u32	decode_mode;
	__u32	fixed_max_frame_size_width;
	__u32	fixed_max_frame_size_height;
	__u32	fixed_max_frame_buffer_mode;
	union {
		char	*crc_path;
		__u64	crc_path_64;
	};
	union {
		char	*golden_path;
		__u64	golden_path_64;
	};
	__u32	wait_key_frame;
	__u32	svp_mode;
	__u32	operating_rate;
	__u32	decode_error_handle_mode;
	__u32	queued_frame_buf_count;
	__s32	priority;
	__s32	slice_count;
	__u32	vpeek;
	__u32	linecount_threshold_mode;
};

/**
 * struct vdec_pic_info  - picture size information
 * @pic_w: picture width
 * @pic_h: picture height
 * @buf_w   : picture buffer width (codec aligned up from pic_w)
 * @buf_h   : picture buffer heiht (codec aligned up from pic_h)
 * @fb_sz: frame buffer size
 * @bitdepth: Sequence bitdepth
 * @layout_mode: mediatek frame layout mode
 * @fourcc: frame buffer color format
 * @field: enum v4l2_field, field type of this sequence
 * E.g. suppose picture size is 176x144,
 *      buffer size will be aligned to 176x160.
 */
struct vdec_pic_info {
	__u32 pic_w;
	__u32 pic_h;
	__u32 buf_w;
	__u32 buf_h;
	__u32 fb_sz[VIDEO_MAX_PLANES];
	__u32 bitdepth;
	__u32 layout_mode;
	__u32 fourcc;
	__u32 field;
};

/**
 * struct vdec_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @vdec_changed_info  : some changed flags
 * @bs_dma		: Input bit-stream buffer dma address
 * @bs_fd               : Input bit-stream buffer dmabuf fd
 * @fb_dma		: Y frame buffer dma address
 * @fb_fd             : Y frame buffer dmabuf fd
 * @vdec_bs_va		: VDEC bitstream buffer struct virtual address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 * @fb_num_planes	: frame buffer plane count
 * @reserved		: reserved variable for 64bit align
 */
struct vdec_dec_info {
	__u32 dpb_sz;
	__u32 vdec_changed_info;
	__u64 bs_dma;
	__u64 bs_fd;
	__u64 bs_non_acp_dma; // for acp debug
	__u64 fb_dma[VIDEO_MAX_PLANES];
	__u64 fb_fd[VIDEO_MAX_PLANES];
	__u64 vdec_bs_va;
	__u64 vdec_fb_va;
	__u32 fb_num_planes;
	__u32 index;
	__u32 error_map;
	__u32 error_code[MTK_VDEC_HW_NUM];
	__u64 timestamp;
};

#define HDR10_PLUS_MAX_SIZE              (128)

struct hdr10plus_info {
	__u8 data[HDR10_PLUS_MAX_SIZE];
	__u32 size;
};

enum vcodec_mem_type {
	MEM_TYPE_FOR_SW,                        /* /< External memory for SW */
	MEM_TYPE_FOR_HW,                        /* /< External memory for HW  */
	MEM_TYPE_FOR_HW_CACHE,                  /* /< External memory for HW which kernel touch so need cacheable */
	MEM_TYPE_FOR_UBE_HW,                    /* /< External memory for UBE reserved memory */
	MEM_TYPE_FOR_SEC_SW,                    /* /< External memory for secure SW */
	MEM_TYPE_FOR_SEC_HW,                    /* /< External memory for secure HW */
	MEM_TYPE_FOR_SEC_UBE_HW,                /* /< External memory for secure UBE */
	MEM_TYPE_FOR_SEC_WFD_HW,                /* /< External memory for secure WFD */
	MEM_TYPE_FOR_SHM,                       /* /< External memory for share memory */
	MEM_TYPE_MAX                            /* /< Max memory type */
};

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @flag:	flag of buffer
 * @iova:	iova of buffer
 * @len:	buffer length
 * @pa:	physical address
 * @va: kernel virtual address
 */
struct vcodec_mem_obj {
	__u32 type;
	__u32 len;
	__u64 iova;
	__u64 pa;
	__u64 va;
};

struct mtk_venc_visual_quality {
	__s32	quant;
	__s32	rd;
};

struct mtk_venc_init_qp {
	__s32	enable;
	__s32	qpi;
	__s32	qpp;
	__s32	qpb;
};

struct mtk_venc_frame_qp_range {
	__s32	enable;
	__s32	max;
	__s32	min;
};

struct mtk_venc_nal_length {
	__u32	prefer;
	__u32	bytes;
};

#endif
