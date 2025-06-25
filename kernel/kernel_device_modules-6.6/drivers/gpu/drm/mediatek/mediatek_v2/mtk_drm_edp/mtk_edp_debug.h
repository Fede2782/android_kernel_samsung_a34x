/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_EDP_DEBUG_H__
#define __MTK_EDP_DEBUG_H__
#include <linux/types.h>

void mtk_edp_debug_enable(bool enable);
bool mtk_edp_debug_get(void);
void mtk_edp_debug(const char *opt);
#ifdef MTK_eDPINFO
int mtk_edp_debugfs_init(void);
void mtk_edp_debugfs_deinit(void);
#endif

#define eDPTXFUNC(fmt, arg...)		\
	pr_info("[eDPTX][%s line:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg)

#define eDPTXDBG(fmt, arg...)              \
	do {                                 \
		if (mtk_dp_debug_get())                  \
			pr_info("[eDPTX]"pr_fmt(fmt), ##arg);     \
	} while (0)

#define eDPTXMSG(fmt, arg...)                                  \
		pr_info("[eDPTX]"pr_fmt(fmt), ##arg)
#endif

