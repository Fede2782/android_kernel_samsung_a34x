/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DRM_DP_DEBUG_H__
#define __MTK_DRM_DP_DEBUG_H__
#include <linux/types.h>

void mtk_dp_debug_enable(bool enable);
bool mtk_dp_debug_get(void);
void mtk_dp_debug(const char *opt);
#ifdef MTK_DPINFO
int mtk_dp_debugfs_init(void);
void mtk_dp_debugfs_deinit(void);
#endif
#endif
