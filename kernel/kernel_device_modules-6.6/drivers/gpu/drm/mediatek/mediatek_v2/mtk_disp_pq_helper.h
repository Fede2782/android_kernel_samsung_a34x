/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_DISP_PQ_HELPER_H_
#define _MTK_DISP_PQ_HELPER_H_

#include <uapi/drm/mediatek_drm.h>
#include "mtk_disp_vidle.h"

#define PQ_MAX_REG_NUM 0x800
#define PQ_MAX_DATA_SIZE 0x2000
#define PQ_MAX_DATA_SIZE_EXT (PQ_MAX_DATA_SIZE * 2) // extend to 16K for gamma/dbi/PQ_COLOR_SET_PQINDEX
#define PQ_CMD_WRITE 0x1
#define PQ_CMD_READ 0x2
#define PQ_CMD_RW 0x3
#define PQ_CMD_SIZE_ANY (~(uint32_t)0)

enum mtk_pq_persist_property {
	DISP_PQ_CCORR_SILKY_BRIGHTNESS,
	DISP_PQ_GAMMA_SILKY_BRIGHTNESS,
	DISP_PQ_DITHER_COLOR_DETECT,
	DISP_PQ_PROPERTY_MAX,
};

enum PQ_REG_TABLE_IDX {
	TUNING_DISP_COLOR = 0,
	TUNING_DISP_CCORR_LINEAR,
	TUNING_DISP_AAL,
	TUNING_DISP_GAMMA,
	TUNING_DISP_DITHER,
	TUNING_DISP_TDSHP,       // 5
	TUNING_DISP_C3D,
	TUNING_DISP_MDP_AAL,
	TUNING_DISP_ODDMR_TOP,
	TUNING_DISP_ODDMR_OD,
	TUNING_REG_MAX
};

struct pq_tuning_pa_base {
	enum mtk_ddp_comp_type type;
	resource_size_t pa_base;
	resource_size_t companion_pa_base;
};

struct pq_dma_map {
	uint32_t start;
	uint32_t size;
};

struct pq_dma_buffer {
	void *va;
	dma_addr_t pa;
	uint32_t size;
};

struct pq_cmd_prop {
	enum mtk_pq_frame_cfg_cmd cmd;
	uint32_t size;
	uint32_t direction;
};
extern const struct pq_cmd_prop g_pq_cmd_map[];

int mtk_drm_ioctl_pq_frame_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_pq_proxy(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int disp_pq_helper_frame_config(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle,
	void *data, bool user_lock);
int disp_pq_helper_fill_comp_pipe_info(struct mtk_ddp_comp *comp, int *path_order,
	bool *is_right_pipe, struct mtk_ddp_comp **companion);
struct drm_crtc *disp_pq_get_crtc_from_connector(int connector_id, struct drm_device *drm_dev);
int disp_pq_proxy_virtual_sw_write(struct drm_crtc *crtc, void *data);
int disp_pq_proxy_virtual_sw_read(struct drm_crtc *crtc, void *data);
int disp_pq_proxy_virtual_hw_read(struct drm_crtc *crtc, void *data);
int disp_pq_proxy_virtual_hw_write(struct drm_crtc *crtc, void *data);
int disp_pq_proxy_virtual_relay_engines(struct drm_crtc *crtc, void *data);
void disp_pq_path_sel_set(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *handle);

#endif /* _MTK_DISP_PQ_HELPER_H_ */
