/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Iris-SC Yang <iris-sc.yang@mediatek.com>
 */

#ifndef __MTK_MML_V4L2_H__
#define __MTK_MML_V4L2_H__

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "mtk-mml.h"

/* three planes - Y, Cb, Cr */
#define V4L2_PIX_FMT_YVU420A v4l2_fourcc('Y', 'A', '2', '1') /* 12  YVU 4:2:0 16-pixel stride */
	/* According to Android AIDL YV12 = 0x32315659,
	 * YV12 is a 4:2:0 YCrCb planar format.
	 * This format assumes a horizontal stride multiple of 16 pixels, and
	 *   y_size = stride * height
	 *   c_stride = ALIGN(stride/2, 16)
	 *   c_size = c_stride * height/2
	 *   size = y_size + c_size * 2
	 *   cr_offset = y_size
	 *   cb_offset = y_size + c_size
	 */

#define MML_M2M_MODULE_NAME	"mtk-mml-m2m"
#define MML_M2M_DEVICE_NAME	"MediaTek mml-m2m"
#define V4L2_CID_MTK_MML_BASE	(V4L2_CTRL_CLASS_USER | 0x2000)

enum {
	MML_M2M_CID_PQPARAM = V4L2_CID_MTK_MML_BASE,
	MML_M2M_CID_SECURE,
};

struct v4l2_pq_submit {
	uint32_t id;
	struct mml_pq_config pq_config;
	struct mml_pq_param pq_param;
};

#endif	/* __MTK_MML_V4L2_H__ */
