/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DRM_DP_H__
#define __MTK_DRM_DP_H__

#include "mtk_drm_dp_common.h"

u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset);
void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val);
void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask);
void mtk_dp_write_byte(struct mtk_dp *mtk_dp, u32 addr, u8 val, u32 mask);
u32 mtk_dp_phy_read(struct mtk_dp *mtk_dp, u32 offset);
void mtk_dp_phy_write(struct mtk_dp *mtk_dp, u32 offset, u32 val);
void mtk_dp_phy_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask);
void mtk_dp_phy_write_byte(struct mtk_dp *mtk_dp, u32 addr, u8 val, u32 mask);
void mtk_dp_fec_enable(struct mtk_dp *mtk_dp, bool enable);
void mtk_dp_video_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable);
void mtk_dp_audio_pg_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, u8 channel,
			    u8 fs, u8 enable);
void mtk_dp_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				u8 channel, u8 fs, u8 word_length);
void mtk_dp_audio_mdiv_set(struct mtk_dp *mtk_dp,
			   const enum dp_encoder_id encoder_id, u8 div);
void mtk_dp_audio_sdp_setting(struct mtk_dp *mtk_dp,
			      const enum dp_encoder_id encoder_id, u8 channel);
void mtk_dp_i2s_audio_config(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id);
void mtk_dp_audio_mute(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable);
void mtk_dp_video_mute(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable);
u8 mtk_dp_color_get_bpp(u8 color_format, u8 color_depth);
void mtk_dp_encoder_reset(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id);
struct edid *mtk_dp_handle_edid(struct mtk_dp *mtk_dp, struct drm_connector *connector);
void mtk_dp_hpd_check_sink_event(struct mtk_dp *mtk_dp);
u8 mtk_dp_get_sink_count(struct mtk_dp *mtk_dp);
struct drm_connector *mtk_dp_add_connector(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_dp_mst_port *port, const char *path);

void mtk_drm_dpi_suspend(void);
void mtk_drm_dpi_resume(void);

/*  dp tx api for debug start */
int mtk_dp_phy_get_info(char *buffer, int size);
/*	dp tx api for debug end */
#endif
