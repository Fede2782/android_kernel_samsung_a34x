/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_PMQOS_H_
#define _MTK_DRM_PMQOS_H_

//#include <linux/interconnect-provider.h>
#include "mtk-interconnect-provider.h"
#include <linux/pm_qos.h>

enum DISP_QOS_BW_MODE {
	DISP_BW_NORMAL_MODE = 0,
	DISP_BW_FBDC_MODE,
	DISP_BW_HRT_MODE,
	DISP_BW_FORCE_UPDATE,
	DISP_BW_UPDATE_PENDING,
};

enum CHANNEL_TYPE {
	CHANNEL_SRT_READ,
	CHANNEL_SRT_WRITE,
	CHANNEL_HRT_READ,
	CHANNEL_HRT_WRITE,
	CHANNEL_SRT_RW,
	CHANNEL_HRT_RW,
};

enum CHANNEL_BW_MODE {
	CHANNEL_BW_OF_OVL = 0x1,
	CHANNEL_BW_OF_PQ = 0x2,
	CHANNEL_BW_OF_WDMA_IWB = 0x4,
	CHANNEL_BW_OF_WDMA_CWB = 0x8,
	CHANNEL_BW_DEFAULT = CHANNEL_BW_OF_OVL | CHANNEL_BW_OF_PQ | CHANNEL_BW_OF_WDMA_CWB,
	CHANNEL_BW_HSIDLE_DC = CHANNEL_BW_OF_OVL | CHANNEL_BW_OF_PQ | CHANNEL_BW_OF_WDMA_IWB,
	CHANNEL_BW_HSIDLE_DL = CHANNEL_BW_OF_PQ,
};

#define NO_PENDING_HRT (0xFFFF)
#define OVL_REQ_HRT (0x1)
#define RDMA_REQ_HRT (0x2)
#define MDP_RDMA_REQ_HRT (0x3)
#define WDMA_REQ_HRT (0x4)

#define BW_CHANNEL_NR 4

#define MAX_MMCLK (7000)
struct drm_crtc;
struct mtk_drm_crtc;
struct mtk_ddp_comp;

struct mtk_larb_port_bw {
	int larb_id;
	unsigned int bw;
	enum CHANNEL_TYPE type;
};

struct mtk_drm_qos_ctx {
	unsigned int last_hrt_req;
	unsigned int last_mmclk_req_idx;
	unsigned int last_larb_hrt_req;
	unsigned int last_channel_req[BW_CHANNEL_NR];
	atomic_t last_hrt_idx;
	atomic_t hrt_cond_sig;
	wait_queue_head_t hrt_cond_wq;
};

void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event);
void mtk_disp_clr_debug_deteriorate(void);
int __mtk_disp_set_module_srt(struct icc_path *request, int comp_id,
				unsigned int bandwidth, unsigned int peak_bw, unsigned int bw_mode,
				bool real_srt_ostdl);
void __mtk_disp_set_module_hrt(struct icc_path *request, int comp_id,
				unsigned int bandwidth, bool respective_ostdl);
void mtk_disp_clr_module_hrt(struct mtk_drm_crtc *mtk_crtc);
int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
			unsigned int overlap_num);
void mtk_aod_scp_set_BW(void);
void mtk_drm_pan_disp_set_hrt_bw(struct drm_crtc *crtc, const char *caller);
int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode);
int mtk_disp_hrt_cond_init(struct drm_crtc *crtc);
void mtk_drm_mmdvfs_init(struct device *dev);
unsigned int mtk_drm_get_mmclk_step_size(void);
void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, bool lp_mode,
			const char *caller);
void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc, unsigned int pixclk,
			const char *caller);
unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller);
unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller);
unsigned int mtk_disp_get_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc);
void mtk_disp_update_channel_hrt_MT6991(struct mtk_drm_crtc *mtk_crtc,
						unsigned int bw_base, unsigned int channel_bw[]);
unsigned int mtk_disp_get_channel_idx_MT6991(enum CHANNEL_TYPE type, unsigned int i);
void mtk_disp_set_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw, int i);
void mtk_disp_channel_srt_bw(struct mtk_drm_crtc *mtk_crtc);
void mtk_disp_clear_channel_srt_bw(struct mtk_drm_crtc *mtk_crtc);
void mtk_disp_total_srt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw);

void mtk_disp_update_channel_bw_by_layer_MT6989(unsigned int layer, unsigned int bpp,
	unsigned int *subcomm_bw_sum, unsigned int size,
	unsigned int bw_base, enum CHANNEL_TYPE type);
void mtk_disp_update_channel_bw_by_layer_MT6899(unsigned int layer, unsigned int bpp,
	unsigned int *subcomm_bw_sum, unsigned int size,
	unsigned int bw_base, enum CHANNEL_TYPE type);
void mtk_disp_update_channel_bw_by_larb_MT6989(struct mtk_larb_port_bw *port_bw,
	unsigned int *subcomm_bw_sum, unsigned int size, enum CHANNEL_TYPE type);
void mtk_disp_update_channel_bw_by_larb_MT6899(struct mtk_larb_port_bw *port_bw,
	unsigned int *subcomm_bw_sum, unsigned int size, enum CHANNEL_TYPE type);

int mtk_disp_get_port_hrt_bw(struct mtk_ddp_comp *comp, enum CHANNEL_TYPE type);
void mtk_disp_get_channel_hrt_bw_by_scope(struct mtk_drm_crtc *mtk_crtc,
				unsigned int scope, unsigned int *result, unsigned int size);
void mtk_disp_get_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
				unsigned int *result, unsigned int size);
unsigned int mtk_disp_set_per_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
		unsigned int bw, unsigned int ch_idx, bool force, const char *master);
unsigned int mtk_disp_set_max_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
		unsigned int *bw, unsigned int size, const char *master);

void mtk_disp_hrt_repaint_blocking(const unsigned int hrt_idx);
#endif
