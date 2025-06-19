/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DRM_DP_MST_H__
#define __MTK_DRM_DP_MST_H__

#include "mtk_drm_dp_common.h"

u8 *mtk_dp_mst_drv_send_remote_i2c_read(struct mtk_drm_dp_mst_branch *mstb,
					struct mtk_drm_dp_mst_port *port);
void mtk_dp_mst_drv_reset(struct mtk_dp *mtk_dp, struct mtk_drm_dp_mst_topology_mgr *mgr,
			  u8 is_plug);
void mtk_dp_mst_drv_sideband_msg_irq_clear(struct mtk_dp *mtk_dp);
void mtk_dp_mst_drv_sideband_msg_rdy_clear(struct mtk_dp *mtk_dp);
u8 mtk_dp_mst_drv_handler(struct mtk_dp *mtk_dp);
void mtk_dp_mst_drv_video_mute_all(struct mtk_dp *mtk_dp);
void mtk_dp_mst_drv_audio_mute_all(struct mtk_dp *mtk_dp);

#endif
