/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Iris-SC Yang <iris-sc.yang@mediatek.com>
 */

#ifndef __MTK_MML_V4L2_COLOR_H__
#define __MTK_MML_V4L2_COLOR_H__

#include <mtk-v4l2-vcodec.h>

#include "mtk-mml-v4l2.h"
#include "mtk-mml-color.h"

#define MML_M2M_FMT_OUTPUT	BIT(0)
#define MML_M2M_FMT_CAPTURE	BIT(1)

struct mml_m2m_format {
	u32	pixelformat;
	u32	mml_color;
	u8	depth[VIDEO_MAX_PLANES];
	u8	row_depth[VIDEO_MAX_PLANES];
	u8	num_planes;
	u8	walign;
	u8	halign;
	u8	salign;
	u32	types;
	u32	flags;
};

struct mml_m2m_pix_limit {
	u32 wmin;
	u32 hmin;
	u32 wmax;
	u32 hmax;
};

struct mml_m2m_limit {
	struct mml_m2m_pix_limit out_limit;
	struct mml_m2m_pix_limit cap_limit;
	u32 h_scale_up_max;
	u32 v_scale_up_max;
	u32 h_scale_down_max;
	u32 v_scale_down_max;
};

static const struct mml_m2m_format mml_m2m_formats[] = {
	{
		.pixelformat	= V4L2_PIX_FMT_NV12_HYFBC,
		.mml_color	= MML_FMT_NV12_HYFBC,
		.depth		= { 12 },
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		.row_depth	= { 8 },
#else
		.row_depth	= { 12 },
#endif
		.num_planes	= 1,
		.walign		= 5,
		.halign		= 4,
		.salign		= 6,
		.types		= MML_M2M_FMT_OUTPUT,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_P010_HYFBC,
		.mml_color	= MML_FMT_P010_HYFBC,
		.depth		= { 18 },
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		.row_depth	= { 12 },
#else
		.row_depth	= { 18 },
#endif
		.num_planes	= 1,
		.walign		= 5,
		.halign		= 4,
		.salign		= 6,
		.types		= MML_M2M_FMT_OUTPUT,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB32_AFBC,
		.mml_color	= MML_FMT_RGBA8888_AFBC,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.walign		= 5,
		.halign		= 3,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGBA1010102_AFBC,
		.mml_color	= MML_FMT_RGBA1010102_AFBC,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.walign		= 5,
		.halign		= 3,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12_AFBC,
		.mml_color	= MML_FMT_YUV420_AFBC,
		.depth		= { 12 },
		.row_depth	= { 12 },
		.num_planes	= 1,
		.walign		= 4,
		.halign		= 4,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12_10B_AFBC,
		.mml_color	= MML_FMT_YUV420_10P_AFBC,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 4,
		.halign		= 4,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.mml_color	= MML_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565X,
		.mml_color	= MML_FMT_BGR565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.mml_color	= MML_FMT_RGB565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.mml_color	= MML_FMT_RGB888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.mml_color	= MML_FMT_BGR888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.mml_color	= MML_FMT_BGRA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	},  {
		.pixelformat	= V4L2_PIX_FMT_BGR32,
		.mml_color	= MML_FMT_BGRA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.mml_color	= MML_FMT_RGBA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGBA32,
		.mml_color	= MML_FMT_RGBA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_BGRA32,
		.mml_color	= MML_FMT_ABGR8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.mml_color	= MML_FMT_ARGB8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGBA1010102,
		.mml_color	= MML_FMT_ABGR2101010,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ARGB2101010,
		.mml_color	= MML_FMT_BGRA1010102,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUVA32,
		.mml_color	= MML_FMT_YUVA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_AYUV32,
		.mml_color	= MML_FMT_AYUV8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.mml_color	= MML_FMT_UYVY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.mml_color	= MML_FMT_VYUY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mml_color	= MML_FMT_YUYV,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.mml_color	= MML_FMT_YVYU,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420A,
		.mml_color	= MML_FMT_YV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.mml_color	= MML_FMT_I420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.mml_color	= MML_FMT_YV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.mml_color	= MML_FMT_NV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.mml_color	= MML_FMT_NV21,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_P010,
		.mml_color	= MML_FMT_NV12_10L,
		.depth		= { 24 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MT21C,
		.mml_color	= MML_FMT_BLK_UFO,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 4,
		.halign		= 5,
		.types		= MML_M2M_FMT_OUTPUT,
		.flags		= V4L2_FMT_FLAG_COMPRESSED,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MM21,
		.mml_color	= MML_FMT_BLK,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 4,
		.halign		= 5,
		.types		= MML_M2M_FMT_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.mml_color	= MML_FMT_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.mml_color	= MML_FMT_NV21,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.mml_color	= MML_FMT_I420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.mml_color	= MML_FMT_YV12,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.types		= MML_M2M_FMT_OUTPUT | MML_M2M_FMT_CAPTURE,
	},
};

static const struct mml_m2m_limit mml_m2m_def_limit = {
	.out_limit = {
		.wmin	= 32, /* max of HW limitation and all format steps */
		.hmin	= 32, /* max of HW limitation and all format steps */
		.wmax	= 65504,
		.hmax	= 65504,
	},
	.cap_limit = {
		.wmin	= 32,
		.hmin	= 32,
		.wmax	= 65504,
		.hmax	= 65504,
	},
	.h_scale_up_max = 32,
	.v_scale_up_max = 32,
	.h_scale_down_max = 20,
	.v_scale_down_max = 24,
};

#endif	/* __MTK_MML_V4L2_COLOR_H__ */

