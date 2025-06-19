// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/* Driver for virtio video device.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 */

#include "virtio_video.h"

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
#include "virtio_video_util.h"
#endif

struct virtio_video_convert_table {
	uint32_t virtio_value;
	uint32_t v4l2_value;
};

static struct virtio_video_convert_table level_table[] = {
	{ VIRTIO_VIDEO_LEVEL_H264_1_0, V4L2_MPEG_VIDEO_H264_LEVEL_1_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_1_1, V4L2_MPEG_VIDEO_H264_LEVEL_1_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_1_2, V4L2_MPEG_VIDEO_H264_LEVEL_1_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_1_3, V4L2_MPEG_VIDEO_H264_LEVEL_1_3 },
	{ VIRTIO_VIDEO_LEVEL_H264_2_0, V4L2_MPEG_VIDEO_H264_LEVEL_2_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_2_1, V4L2_MPEG_VIDEO_H264_LEVEL_2_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_2_2, V4L2_MPEG_VIDEO_H264_LEVEL_2_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_3_0, V4L2_MPEG_VIDEO_H264_LEVEL_3_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_3_1, V4L2_MPEG_VIDEO_H264_LEVEL_3_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_3_2, V4L2_MPEG_VIDEO_H264_LEVEL_3_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_4_0, V4L2_MPEG_VIDEO_H264_LEVEL_4_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_4_1, V4L2_MPEG_VIDEO_H264_LEVEL_4_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_4_2, V4L2_MPEG_VIDEO_H264_LEVEL_4_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_5_0, V4L2_MPEG_VIDEO_H264_LEVEL_5_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_5_1, V4L2_MPEG_VIDEO_H264_LEVEL_5_1 },
	{ 0 },
};

uint32_t virtio_video_level_to_v4l2(uint32_t level)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(level_table); idx++) {
		if (level_table[idx].virtio_value == level)
			return level_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_level_to_virtio(uint32_t v4l2_level)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(level_table); idx++) {
		if (level_table[idx].v4l2_value == v4l2_level)
			return level_table[idx].virtio_value;
	}

	return 0;
}

static struct virtio_video_convert_table profile_table[] = {
	{ VIRTIO_VIDEO_PROFILE_H264_BASELINE,
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE },
	{ VIRTIO_VIDEO_PROFILE_H264_MAIN, V4L2_MPEG_VIDEO_H264_PROFILE_MAIN },
	{ VIRTIO_VIDEO_PROFILE_H264_EXTENDED,
		V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED },
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH, V4L2_MPEG_VIDEO_H264_PROFILE_HIGH },
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH10PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10 },
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH422PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422},
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH444PREDICTIVEPROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE },
	{ VIRTIO_VIDEO_PROFILE_H264_SCALABLEBASELINE,
		V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_BASELINE },
	{ VIRTIO_VIDEO_PROFILE_H264_SCALABLEHIGH,
		V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_HIGH },
	{ VIRTIO_VIDEO_PROFILE_H264_STEREOHIGH,
		V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH },
	{ VIRTIO_VIDEO_PROFILE_H264_MULTIVIEWHIGH,
		V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH },
	{ 0 },
};

uint32_t virtio_video_profile_to_v4l2(uint32_t profile)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(profile_table); idx++) {
		if (profile_table[idx].virtio_value == profile)
			return profile_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_profile_to_virtio(uint32_t v4l2_profile)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(profile_table); idx++) {
		if (profile_table[idx].v4l2_value == v4l2_profile)
			return profile_table[idx].virtio_value;
	}

	return 0;
}

static struct virtio_video_convert_table format_table[] = {
	{VIRTIO_VIDEO_FORMAT_ARGB8888, V4L2_PIX_FMT_ARGB32},
	{VIRTIO_VIDEO_FORMAT_BGRA8888, V4L2_PIX_FMT_ABGR32},
	{VIRTIO_VIDEO_FORMAT_RGB32, V4L2_PIX_FMT_RGB32},
	{VIRTIO_VIDEO_FORMAT_BGR24, V4L2_PIX_FMT_BGR24},
	{VIRTIO_VIDEO_FORMAT_RGB24, V4L2_PIX_FMT_RGB24},
	{VIRTIO_VIDEO_FORMAT_NV12M, V4L2_PIX_FMT_NV12M},
	{VIRTIO_VIDEO_FORMAT_NV21M, V4L2_PIX_FMT_NV21M},
	{VIRTIO_VIDEO_FORMAT_YUV420M, V4L2_PIX_FMT_YUV420M},
	{VIRTIO_VIDEO_FORMAT_YVU420M, V4L2_PIX_FMT_YVU420M},
	{VIRTIO_VIDEO_FORMAT_YUV420, V4L2_PIX_FMT_YUV420},
	{VIRTIO_VIDEO_FORMAT_YVU420, V4L2_PIX_FMT_YVU420},
	{VIRTIO_VIDEO_FORMAT_NV12, V4L2_PIX_FMT_NV12},
	{VIRTIO_VIDEO_FORMAT_NV21, V4L2_PIX_FMT_NV21},
	{VIRTIO_VIDEO_FORMAT_BGR32, V4L2_PIX_FMT_BGR32},
	{VIRTIO_VIDEO_FORMAT_RGB32_AFBC, V4L2_PIX_FMT_RGB32_AFBC},
	{VIRTIO_VIDEO_FORMAT_BGR32_AFBC, V4L2_PIX_FMT_BGR32_AFBC},
	{VIRTIO_VIDEO_FORMAT_RGBA1010102_AFBC, V4L2_PIX_FMT_RGBA1010102_AFBC},
	{VIRTIO_VIDEO_FORMAT_BGRA1010102_AFBC, V4L2_PIX_FMT_BGRA1010102_AFBC},
	{VIRTIO_VIDEO_FORMAT_NV12_AFBC, V4L2_PIX_FMT_NV12_AFBC},
	{VIRTIO_VIDEO_FORMAT_NV12_10B_AFBC, V4L2_PIX_FMT_NV12_10B_AFBC},

	{VIRTIO_VIDEO_FORMAT_MT21, V4L2_PIX_FMT_MT21},
	{VIRTIO_VIDEO_FORMAT_MT2110T, V4L2_PIX_FMT_MT2110T},
	{VIRTIO_VIDEO_FORMAT_MT2110R, V4L2_PIX_FMT_MT2110R},
	{VIRTIO_VIDEO_FORMAT_MT21C10T, V4L2_PIX_FMT_MT21C10T},
	{VIRTIO_VIDEO_FORMAT_MT21C10R, V4L2_PIX_FMT_MT21C10R},
	{VIRTIO_VIDEO_FORMAT_MT21S, V4L2_PIX_FMT_MT21S},
	{VIRTIO_VIDEO_FORMAT_MT21S10T, V4L2_PIX_FMT_MT21S10T},
	{VIRTIO_VIDEO_FORMAT_MT21S10R, V4L2_PIX_FMT_MT21S10R},
	{VIRTIO_VIDEO_FORMAT_MT21S10RJ, V4L2_PIX_FMT_MT21S10RJ},
	{VIRTIO_VIDEO_FORMAT_MT21S10TJ, V4L2_PIX_FMT_MT21S10TJ},
	{VIRTIO_VIDEO_FORMAT_MT21CS, V4L2_PIX_FMT_MT21CS},
	{VIRTIO_VIDEO_FORMAT_MT21CSA, V4L2_PIX_FMT_MT21CSA},
	{VIRTIO_VIDEO_FORMAT_MT21CS10R, V4L2_PIX_FMT_MT21CS10R},
	{VIRTIO_VIDEO_FORMAT_MT21CS10T, V4L2_PIX_FMT_MT21CS10T},
	{VIRTIO_VIDEO_FORMAT_MT21CS10RJ, V4L2_PIX_FMT_MT21CS10RJ},
	{VIRTIO_VIDEO_FORMAT_MT21CS10TJ, V4L2_PIX_FMT_MT21CS10TJ},
	{VIRTIO_VIDEO_FORMAT_MT10S, V4L2_PIX_FMT_MT10S},
	{VIRTIO_VIDEO_FORMAT_MT10, V4L2_PIX_FMT_MT10},
	{VIRTIO_VIDEO_FORMAT_P010S, V4L2_PIX_FMT_P010S},
	{VIRTIO_VIDEO_FORMAT_NV21_AFBC, V4L2_PIX_FMT_NV21_AFBC},
	{VIRTIO_VIDEO_FORMAT_NV12_HYFBC, V4L2_PIX_FMT_NV12_HYFBC},
	{VIRTIO_VIDEO_FORMAT_P010_HYFBC, V4L2_PIX_FMT_P010_HYFBC},

	{VIRTIO_VIDEO_FORMAT_MPEG2, V4L2_PIX_FMT_MPEG2},
	{VIRTIO_VIDEO_FORMAT_MPEG4, V4L2_PIX_FMT_MPEG4},
	{VIRTIO_VIDEO_FORMAT_H263, V4L2_PIX_FMT_H263},
	{VIRTIO_VIDEO_FORMAT_H264, V4L2_PIX_FMT_H264},
	{VIRTIO_VIDEO_FORMAT_HEVC, V4L2_PIX_FMT_HEVC},
	{VIRTIO_VIDEO_FORMAT_HEIF, V4L2_PIX_FMT_HEIF},
	{VIRTIO_VIDEO_FORMAT_VP8, V4L2_PIX_FMT_VP8},
	{VIRTIO_VIDEO_FORMAT_VP9, V4L2_PIX_FMT_VP9},
	{VIRTIO_VIDEO_FORMAT_AV1, V4L2_PIX_FMT_AV1},
	{0},
};

uint32_t virtio_video_format_to_v4l2(uint32_t format)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(format_table); idx++) {
		if (format_table[idx].virtio_value == format)
			return format_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_format_to_virtio(uint32_t v4l2_format)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(format_table); idx++) {
		if (format_table[idx].v4l2_value == v4l2_format)
			return format_table[idx].virtio_value;
	}

	return 0;
}

static struct virtio_video_convert_table control_table[] = {
	{ VIRTIO_VIDEO_CONTROL_BITRATE, V4L2_CID_MPEG_VIDEO_BITRATE },
	{ VIRTIO_VIDEO_CONTROL_PROFILE, V4L2_CID_MPEG_VIDEO_H264_PROFILE },
	{ VIRTIO_VIDEO_CONTROL_LEVEL, V4L2_CID_MPEG_VIDEO_H264_LEVEL },
	{ VIRTIO_VIDEO_CONTROL_FORCE_KEYFRAME,
			V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME },
	{ 0 },
};

uint32_t virtio_video_control_to_v4l2(uint32_t control)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(control_table); idx++) {
		if (control_table[idx].virtio_value == control)
			return control_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_control_to_virtio(uint32_t v4l2_control)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(control_table); idx++) {
		if (control_table[idx].v4l2_value == v4l2_control)
			return control_table[idx].virtio_value;
	}

	return 0;
}

struct video_format *find_video_format(struct list_head *fmts_list,
				       uint32_t format)
{
	struct video_format *fmt = NULL;

	list_for_each_entry(fmt, fmts_list, formats_list_entry) {
		if (fmt->desc.format == format)
			return fmt;
	}

	return NULL;
}

void virtio_video_format_from_info(struct video_format_info *info,
				   struct v4l2_pix_format_mplane *pix_mp)
{
	int i;

	pix_mp->width = info->frame_width;
	pix_mp->height = info->frame_height;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->colorspace = V4L2_COLORSPACE_REC709;
	pix_mp->xfer_func = 0;
	pix_mp->ycbcr_enc = 0;
	pix_mp->quantization = 0;
	memset(pix_mp->reserved, 0, sizeof(pix_mp->reserved));
	memset(pix_mp->plane_fmt[0].reserved, 0,
	       sizeof(pix_mp->plane_fmt[0].reserved));

	pix_mp->num_planes = info->num_planes;
	pix_mp->pixelformat = info->fourcc_format;

	for (i = 0; i < info->num_planes; i++) {
		pix_mp->plane_fmt[i].bytesperline =
					 info->plane_format[i].stride;
		pix_mp->plane_fmt[i].sizeimage =
					 info->plane_format[i].plane_size;
	}
}

void virtio_video_format_fill_default_info(struct video_format_info *dst_info,
					  struct video_format_info *src_info)
{
	memcpy(dst_info, src_info, sizeof(*dst_info));
}
