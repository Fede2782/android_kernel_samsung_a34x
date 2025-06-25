/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Iris-SC Yang <iris-sc.yang@mediatek.com>
 */

#ifndef __MTK_MML_M2M_ADAPTOR_H__
#define __MTK_MML_M2M_ADAPTOR_H__

#include <linux/device.h>

struct mml_v4l2_dev;

struct mml_v4l2_dev *mml_v4l2_dev_create(struct device *dev);
void mml_v4l2_dev_destroy(struct device *dev, struct mml_v4l2_dev *v4l2_dev);

#endif	/* __MTK_MML_M2M_ADAPTOR_H__ */
