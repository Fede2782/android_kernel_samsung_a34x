/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_COMP_AUTO_H
#define MTK_DRM_DDP_COMP_AUTO_H

#include <linux/io.h>
#include <linux/kernel.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
struct mtk_ddp_comp_match {
	enum mtk_ddp_comp_id index;
	enum mtk_ddp_comp_type type;
	int alias_id;
	const struct mtk_ddp_comp_funcs *funcs;
	bool is_output;
	bool is_virt_comp;
};

extern const struct mtk_ddp_comp_match mtk_ddp_matches[DDP_COMPONENT_ID_MAX];

bool mtk_ddp_comp_is_rdma(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_virt(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_virt_by_id(enum mtk_ddp_comp_id id);
#endif
#endif /* MTK_DRM_DDP_COMP_AUTO_H */
