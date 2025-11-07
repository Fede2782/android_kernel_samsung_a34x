/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_VDISP_H__
#define __MTK_VDISP_H__

#include "mtk_dpc_v2.h"
#include "mtk_vdisp_common.h"
#include "mtk_vdisp_avs.h"

#define VDISPDBG(fmt, args...) \
	pr_info("[vdisp] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define VDISPERR(fmt, args...) \
	pr_info("[vdisp][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define VDISPAEE(fmt, args...)                                                                   \
	do {                                                                                     \
		char str[200];                                                                   \
		int r;                                                                           \
		pr_info("[vdisp][err] %s:%d " fmt "\n", __func__, __LINE__, ##args);             \
		r = snprintf(str, 199, "VDISP:" fmt, ##args);                                    \
		if (r < 0)                                                                       \
			pr_info("snprintf error\n");                                             \
		aee_kernel_warning_api(__FILE__, __LINE__,                                       \
				       DB_OPT_DEFAULT | DB_OPT_FTRACE | DB_OPT_MMPROFILE_BUFFER, \
				       str, fmt, ##args);                                        \
	} while (0)
#else /* !CONFIG_MTK_AEE_FEATURE */
#define VDISPAEE(fmt, args...) \
	pr_info("[vdisp][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)
#endif

/* This id is only for disp internal use */
enum disp_pd_id {
	DISP_PD_DISP_VCORE,
	DISP_PD_DISP1,
	DISP_PD_DISP0,
	DISP_PD_OVL1,
	DISP_PD_OVL0,
	DISP_PD_MML1,
	DISP_PD_MML0,
	DISP_PD_EDP,
	DISP_PD_DPTX,
	DISP_PD_NUM,
};

void mtk_vdisp_dpc_register(const struct dpc_funcs *funcs);

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct mtk_vdisp_data {
	const struct mtk_vdisp_avs_data *avs;
};

#endif
