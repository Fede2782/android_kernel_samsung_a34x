/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _VIRTIO_VIDEO_V4L2_H_
#define _VIRTIO_VIDEO_V4L2_H_

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#define V4L2_EVENT_MTK_VCODEC_START	(V4L2_EVENT_PRIVATE_START + 0x00002000)
#define V4L2_EVENT_MTK_VDEC_ERROR	(V4L2_EVENT_MTK_VCODEC_START + 1)
#define V4L2_EVENT_MTK_VDEC_NOHEADER	(V4L2_EVENT_MTK_VCODEC_START + 2)
#define V4L2_EVENT_MTK_VENC_ERROR	(V4L2_EVENT_MTK_VCODEC_START + 3)

/* Mediatek control IDs */
#define V4L2_CID_CODEC_MTK_BASE \
	(V4L2_CTRL_CLASS_CODEC | 0x2000)
#define V4L2_CID_MPEG_MTK_DECODE_MODE \
	(V4L2_CID_CODEC_MTK_BASE+2)

#define V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER \
	(V4L2_CID_CODEC_MTK_BASE+4)
#define V4L2_CID_MPEG_MTK_CRC_PATH \
	(V4L2_CID_CODEC_MTK_BASE+5)
#define V4L2_CID_MPEG_MTK_GOLDEN_PATH \
	(V4L2_CID_CODEC_MTK_BASE+6)
#define V4L2_CID_MPEG_MTK_COLOR_DESC \
	(V4L2_CID_CODEC_MTK_BASE+7)

#define V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME \
	(V4L2_CID_CODEC_MTK_BASE+9)

#define V4L2_CID_MPEG_MTK_SEC_DECODE \
	(V4L2_CID_CODEC_MTK_BASE+11)
#define V4L2_CID_MPEG_MTK_FIX_BUFFERS \
	(V4L2_CID_CODEC_MTK_BASE+12)

#define V4L2_CID_MPEG_MTK_INTERLACING \
	(V4L2_CID_CODEC_MTK_BASE+14)

#define V4L2_CID_MPEG_MTK_OPERATING_RATE \
	(V4L2_CID_CODEC_MTK_BASE+16)
#define V4L2_CID_MPEG_MTK_SEC_ENCODE \
	(V4L2_CID_CODEC_MTK_BASE+17)
#define V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT \
	(V4L2_CID_CODEC_MTK_BASE+18)
#define V4L2_CID_VDEC_COMPRESSED_MODE \
	(V4L2_CID_CODEC_MTK_BASE+19)
#define V4L2_CID_MPEG_MTK_ENCODE_SCENARIO \
	(V4L2_CID_CODEC_MTK_BASE+20)
#define V4L2_CID_MPEG_MTK_ENCODE_NONREFP \
	(V4L2_CID_CODEC_MTK_BASE+21)
#define V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE \
	(V4L2_CID_CODEC_MTK_BASE+22)
#define V4L2_CID_MPEG_MTK_ENCODE_RFS_ON \
	(V4L2_CID_CODEC_MTK_BASE+23)
#define V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE \
	(V4L2_CID_CODEC_MTK_BASE+24)
#define V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP \
	(V4L2_CID_CODEC_MTK_BASE+25)
#define V4L2_CID_MPEG_MTK_ENCODE_ROI_ON \
	(V4L2_CID_CODEC_MTK_BASE+26)
#define V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE \
	(V4L2_CID_CODEC_MTK_BASE+27)
#define V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE \
	(V4L2_CID_CODEC_MTK_BASE+28)
#define V4L2_CID_MPEG_MTK_MAX_WIDTH \
	(V4L2_CID_CODEC_MTK_BASE+29)
#define V4L2_CID_MPEG_MTK_MAX_HEIGHT \
	(V4L2_CID_CODEC_MTK_BASE+30)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP \
	(V4L2_CID_CODEC_MTK_BASE+31)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP \
	(V4L2_CID_CODEC_MTK_BASE+32)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP \
	(V4L2_CID_CODEC_MTK_BASE+33)
#define V4L2_CID_MPEG_VIDEO_ENABLE_TSVC \
	(V4L2_CID_CODEC_MTK_BASE+34)
#define V4L2_CID_MPEG_MTK_ENCODE_NONREFP_FREQ \
	(V4L2_CID_CODEC_MTK_BASE+35)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_MAX_QP \
	(V4L2_CID_CODEC_MTK_BASE+36)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_MIN_QP \
	(V4L2_CID_CODEC_MTK_BASE+37)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_I_P_QP_DELTA \
	(V4L2_CID_CODEC_MTK_BASE+38)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_QP_CONTROL_MODE \
	(V4L2_CID_CODEC_MTK_BASE+39)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_FRAME_LEVEL_QP \
	(V4L2_CID_CODEC_MTK_BASE+40)

#define V4L2_CID_MPEG_MTK_ENCODE_ENABLE_HIGHQUALITY \
	(V4L2_CID_CODEC_MTK_BASE+45)
#define V4L2_CID_MPEG_MTK_LOG \
	(V4L2_CID_CODEC_MTK_BASE+46)
#define V4L2_CID_MPEG_MTK_ENCODE_ENABLE_DUMMY_NAL \
	(V4L2_CID_CODEC_MTK_BASE+47)
#define V4L2_CID_MPEG_MTK_REAL_TIME_PRIORITY \
	(V4L2_CID_CODEC_MTK_BASE+48)
#define V4L2_CID_MPEG_MTK_VCP_PROP \
	(V4L2_CID_CODEC_MTK_BASE+49)
#define V4L2_CID_VDEC_DETECT_TIMESTAMP \
	(V4L2_CID_CODEC_MTK_BASE+50)

#define V4L2_CID_MPEG_MTK_ENCODE_MULTI_REF \
	(V4L2_CID_CODEC_MTK_BASE+51)
#define V4L2_CID_MPEG_MTK_INTERLACING_FIELD_SEQ \
	(V4L2_CID_CODEC_MTK_BASE+52)
#define V4L2_CID_MPEG_MTK_ENCODE_WPP_MODE \
		(V4L2_CID_CODEC_MTK_BASE+53)
#define V4L2_CID_MPEG_MTK_ENCODE_LOW_LATENCY_MODE \
		(V4L2_CID_CODEC_MTK_BASE+54)
#define V4L2_CID_MPEG_MTK_ENCODE_TEMPORAL_LAYER_COUNT \
	(V4L2_CID_CODEC_MTK_BASE+55)
#define V4L2_CID_MPEG_MTK_ENCODE_MAX_LTR_FRAMES \
	(V4L2_CID_CODEC_MTK_BASE+56)
#define V4L2_CID_MPEG_MTK_ENCODE_ENABLE_MLVEC_MODE \
	(V4L2_CID_CODEC_MTK_BASE+57)
#define V4L2_CID_VDEC_HDR10_INFO \
	(V4L2_CID_CODEC_MTK_BASE+58)
#define V4L2_CID_VDEC_HDR10PLUS_DATA \
	(V4L2_CID_CODEC_MTK_BASE+59)

// Need to align different use case between mobile, tablet and tv.
#define V4L2_CID_VDEC_TRICK_MODE \
	(V4L2_CID_CODEC_MTK_BASE+60)
#define V4L2_CID_VDEC_NO_REORDER \
	(V4L2_CID_CODEC_MTK_BASE+61)

#define V4L2_CID_MPEG_MTK_SET_DECODE_ERROR_HANDLE_MODE \
	(V4L2_CID_CODEC_MTK_BASE+62)

#define V4L2_CID_MPEG_MTK_GET_LOG \
	(V4L2_CID_CODEC_MTK_BASE+63)

#define V4L2_CID_MPEG_MTK_GET_VCP_PROP \
	(V4L2_CID_CODEC_MTK_BASE+64)

#define V4L2_CID_MPEG_MTK_ENCODE_LOW_LATENCY_WFD \
		(V4L2_CID_CODEC_MTK_BASE+65)

#define V4L2_CID_MPEG_MTK_ENCODE_SLICE_CNT \
	(V4L2_CID_CODEC_MTK_BASE+66)

#define V4L2_CID_MPEG_MTK_ENCODE_QPVBR \
	(V4L2_CID_CODEC_MTK_BASE+67)

#define V4L2_CID_MPEG_MTK_ENCODE_CHROMA_QP \
	(V4L2_CID_CODEC_MTK_BASE+68)

#define V4L2_CID_MPEG_MTK_ENCODE_MB_RC_TK_SPD \
	(V4L2_CID_CODEC_MTK_BASE+69)

#define V4L2_CID_MPEG_MTK_ENCODE_FRM_QP_LTR \
	(V4L2_CID_CODEC_MTK_BASE+70)

#define V4L2_CID_MPEG_MTK_ENCODE_VISUAL_QUALITY \
	(V4L2_CID_CODEC_MTK_BASE+71)

#define V4L2_CID_MPEG_MTK_ENCODE_INIT_QP \
	(V4L2_CID_CODEC_MTK_BASE+72)

#define V4L2_CID_VDEC_SLC_SUPPORT_VER \
	(V4L2_CID_CODEC_MTK_BASE+73)

#define V4L2_CID_MPEG_MTK_ENCODE_FRAME_QP_RANGE \
	(V4L2_CID_CODEC_MTK_BASE+74)

#define V4L2_CID_VDEC_SUBSAMPLE_MODE \
	(V4L2_CID_CODEC_MTK_BASE + 75)

#define V4L2_CID_VDEC_ACQUIRE_RESOURCE \
	(V4L2_CID_CODEC_MTK_BASE + 76)

#define V4L2_CID_VDEC_RESOURCE_METRICS \
	(V4L2_CID_CODEC_MTK_BASE + 77)

#define V4L2_CID_VDEC_VPEEK_MODE \
	(V4L2_CID_CODEC_MTK_BASE + 78)

#define V4L2_CID_VDEC_PLUS_DROP_RATIO \
	(V4L2_CID_CODEC_MTK_BASE + 79)

#define V4L2_CID_VDEC_CONTAINER_FRAMERATE \
	(V4L2_CID_CODEC_MTK_BASE + 80)

#define V4L2_CID_VDEC_DISABLE_DEBLOCK \
	(V4L2_CID_CODEC_MTK_BASE + 81)

#define V4L2_CID_VDEC_LOW_LATENCY \
	(V4L2_CID_CODEC_MTK_BASE + 82)

#define V4L2_CID_VDEC_MAX_BUF_INFO \
	(V4L2_CID_CODEC_MTK_BASE + 83)

#define V4L2_CID_VDEC_BANDWIDTH_INFO \
	(V4L2_CID_CODEC_MTK_BASE + 84)

#define V4L2_CID_MPEG_MTK_CALLING_PID \
	(V4L2_CID_CODEC_MTK_BASE + 85)

#define V4L2_PIX_FMT_HEIF     v4l2_fourcc('H', 'E', 'I', 'F') /* HEIF */

#define V4L2_PIX_FMT_AV1      v4l2_fourcc('A', 'V', '1', '0') /* AV1 */

	/* Mediatek compressed block mode  */
#define V4L2_PIX_FMT_MT21         v4l2_fourcc('M', 'M', '2', '1')
	/* MTK 8-bit block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT2110T      v4l2_fourcc('M', 'T', '2', 'T')
	/* MTK 10-bit tile block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT2110R      v4l2_fourcc('M', 'T', '2', 'R')
	/* MTK 10-bit raster block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT21C10T     v4l2_fourcc('M', 'T', 'C', 'T')
	/* MTK 10-bit tile compressed block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT21C10R     v4l2_fourcc('M', 'T', 'C', 'R')
	/* MTK 10-bit raster compressed block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT21CS       v4l2_fourcc('M', '2', 'C', 'S')
	/* MTK 8-bit compressed block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S        v4l2_fourcc('M', '2', '1', 'S')
	/* MTK 8-bit block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10T     v4l2_fourcc('M', 'T', 'S', 'T')
	/* MTK 10-bit tile block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10R     v4l2_fourcc('M', 'T', 'S', 'R')
	/* MTK 10-bit raster block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10T    v4l2_fourcc('M', 'C', 'S', 'T')
	/* MTK 10-bit tile compressed block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10R    v4l2_fourcc('M', 'C', 'S', 'R')
	/* MTK 10-bit raster compressed block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CSA      v4l2_fourcc('M', 'A', 'C', 'S')
	/* MTK 8-bit compressed block au offset mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10TJ    v4l2_fourcc('M', 'J', 'S', 'T')
	/* MTK 10-bit tile block jump mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10RJ    v4l2_fourcc('M', 'J', 'S', 'R')
	/* MTK 10-bit raster block jump mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10TJ   v4l2_fourcc('J', 'C', 'S', 'T')
	/* MTK 10-bit tile compressed block jump mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10RJ   v4l2_fourcc('J', 'C', 'S', 'R')
	/* MTK 10-bit raster compressed block jump mode, two cont. planes */
#define V4L2_PIX_FMT_MT10S     v4l2_fourcc('M', '1', '0', 'S')
	/* MTK 10-bit compressed mode, three contiguous planes */
#define V4L2_PIX_FMT_MT10     v4l2_fourcc('M', 'T', '1', '0')
	/* MTK 10-bit compressed mode, three non-contiguous planes */
#define V4L2_PIX_FMT_P010S   v4l2_fourcc('P', '0', '1', 'S')
	/* 10-bit each pixel needs 2 bytes, LSB 6-bit is not used contiguous*/

	/* MTK 8-bit frame buffer compressed mode, single plane */
#define V4L2_PIX_FMT_RGB32_AFBC         v4l2_fourcc('M', 'C', 'R', '8')
#define V4L2_PIX_FMT_BGR32_AFBC         v4l2_fourcc('M', 'C', 'B', '8')
	/* MTK 10-bit frame buffer compressed mode, single plane */
#define V4L2_PIX_FMT_RGBA1010102_AFBC   v4l2_fourcc('M', 'C', 'R', 'X')
#define V4L2_PIX_FMT_BGRA1010102_AFBC   v4l2_fourcc('M', 'C', 'B', 'X')
	/* MTK 8-bit frame buffer compressed mode, two planes */
#define V4L2_PIX_FMT_NV12_AFBC          v4l2_fourcc('M', 'C', 'N', '8')
	/* MTK 8-bit frame buffer compressed mode, two planes */
#define V4L2_PIX_FMT_NV21_AFBC          v4l2_fourcc('M', 'N', '2', '8')
	/* MTK 10-bit frame buffer compressed mode, two planes */
#define V4L2_PIX_FMT_NV12_10B_AFBC      v4l2_fourcc('M', 'C', 'N', 'X')
/* Vendor specific - Mediatek ISP compressed formats */
#define V4L2_PIX_FMT_MTISP_B8	v4l2_fourcc('M', 'T', 'B', '8') /* 8 bit */

/* MTK 10-bit yuv 4:2:0 Hybrid FBC mode, two contiguous planes */
#define V4L2_PIX_FMT_NV12_HYFBC v4l2_fourcc('N', '1', '2', 'H')
#define V4L2_PIX_FMT_P010_HYFBC v4l2_fourcc('P', '0', '1', 'H')
/*< DV formats - used for DV query cap*/
#define V4L2_PIX_FMT_DVAV    v4l2_fourcc('D', 'V', 'A', 'V') /* DV AVC */
#define V4L2_PIX_FMT_DVHE    v4l2_fourcc('D', 'V', 'H', 'E') /* DV HEVC */

#endif /* _VIRTIO_VIDEO_V4L2_H_ */
