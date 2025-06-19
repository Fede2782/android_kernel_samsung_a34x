// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_layering_rule.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"

#include "mtk_disp_vidle.h"

#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <soc/mediatek/mmqos.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
#include <linux/interconnect.h>
#include "mtk_disp_bdg.h"
extern u32 *disp_perfs;
#endif
#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif

#include <linux/module.h>
int debug_vidle_bw;
module_param(debug_vidle_bw, int, 0644);

int debug_channel_bw[4];
module_param_array(debug_channel_bw, int, NULL, 0644);

int debug_mmqos;
module_param(debug_mmqos, int, 0644);

int debug_deteriorate;
module_param(debug_deteriorate, int, 0644);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#define CRTC_NUM		7
#else
#define CRTC_NUM		4
#endif
static struct drm_crtc *dev_crtc;
/* add for mm qos */
static u8 vdisp_opp = U8_MAX;
static struct regulator *mm_freq_request;
static unsigned long *g_freq_steps;
static unsigned int lp_freq;
static int g_freq_level[CRTC_NUM] = {-1, -1, -1, -1};
static bool g_freq_lp[CRTC_NUM] = {false, false, false, false};
static long g_freq;
static int step_size = 1;

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
struct hrt_mmclk_request {
	int layer_num;
	int volt;
};

/* available bw per emi port limited*/
static unsigned int hrt_level_bw_mt6768[] = {
	2240, /* 200MHz*16byte*0.7 */
	3360, /* 300MHz*16byte*0.7 */
	4480  /* 400MHz*16byte*0.7 */
};

/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_bdg_mt6768[] = {
	{20, 700000},
	{40, 700000},
	{45, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_bdg_fhdp_mt6768[] = {
	{20, 700000},
	{30, 700000},
	{40, 800000},
};

/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_mt6768[] = {
	{35, 650000},
	{60, 700000},
	{70, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_fhdp_mt6768[] = {
	{30, 650000},
	{50, 700000},
	{60, 800000},
};

struct hrt_mmclk_request hrt_req_level_mt6761[] = {
	{40, 650000},
	{60, 700000},
	{70, 800000},
};

//LPDDR3
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr3_hd_mt6765[] = {
	{75, 650000},
	{75, 700000},
	{75, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr3_fhd_mt6765[] = {
	{35, 650000},
	{35, 700000},
	{35, 800000},
};

//LPDDR4
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_hd_mt6765[] = {
	{75, 650000},
	{135, 700000},
	{155, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_fhd_mt6765[] = {
	{35, 650000},
	{60, 700000},
	{70, 800000},
};

//LPDDR4 and high fps
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_hd_hfps_mt6765[] = {
	{50, 650000},
	{90, 700000},
	{100, 800000},
};
/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_fhd_hfps_mt6765[] = {
	{20, 650000},/*23*/
	{40, 700000},/*40*/
	{45, 800000},/*47*/
};
#endif
void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event)
{
	int len;

	/* mtk_dump_comp_str return shorter comp name, add prefix to match icc name in dts */
	len = snprintf(buf, buf_len, "DDP_COMPONENT_%s_%s", mtk_dump_comp_str(comp), qos_event);
	if (!(len > -1 && len < buf_len))
		DDPINFO("%s: snprintf return error", __func__);
}

int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode)
{
	switch (comp_id) {
	case DDP_COMPONENT_OVL0:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_BW;
	case DDP_COMPONENT_OVL1:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_BW;
	case DDP_COMPONENT_OVL0_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_BW;
	case DDP_COMPONENT_OVL1_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_BW;
	case DDP_COMPONENT_OVL2_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL2_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL2_2L_BW;
	case DDP_COMPONENT_OVL3_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL3_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL3_2L_BW;
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL2_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_NWCG_BW;
	case DDP_COMPONENT_OVL1_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_NWCG_BW;
	case DDP_COMPONENT_RDMA0:
		return DISP_PMQOS_RDMA0_BW;
	case DDP_COMPONENT_RDMA1:
		return DISP_PMQOS_RDMA1_BW;
	case DDP_COMPONENT_RDMA2:
		return DISP_PMQOS_RDMA2_BW;
	case DDP_COMPONENT_WDMA0:
		return DISP_PMQOS_WDMA0_BW;
	case DDP_COMPONENT_WDMA1:
		return DISP_PMQOS_WDMA1_BW;
	case DDP_COMPONENT_OVLSYS_WDMA2:
		return DISP_PMQOS_OVLSYS_WDMA2_BW;
	default:
		DDPPR_ERR("%s, unknown comp %d\n", __func__, comp_id);
		break;
	}

	return -EINVAL;
}

void mtk_disp_clr_debug_deteriorate(void)
{
	debug_deteriorate = 0;
}

int __mtk_disp_set_module_srt(struct icc_path *request, int comp_id,
				unsigned int bandwidth, unsigned int peak_bw, unsigned int bw_mode,
				bool real_srt_ostdl)
{
	DDPQOS("%s set %s/%d bw = %u peak %u, srt_ostdl %d\n", __func__,
			mtk_dump_comp_str_id(comp_id), comp_id, bandwidth, peak_bw, real_srt_ostdl);
	if (real_srt_ostdl != true)
		bandwidth = bandwidth * 133 / 100;

	mtk_icc_set_bw(request, MBps_to_icc(bandwidth), MBps_to_icc(peak_bw));

	if (debug_mmqos)
		DRM_MMP_MARK(pmqos, (comp_id << 16) | bandwidth,  (comp_id << 16) | peak_bw);

	return 0;
}

void __mtk_disp_set_module_hrt(struct icc_path *request, int comp_id,
				unsigned int bandwidth, bool respective_ostdl)
{
	unsigned int icc = MBps_to_icc(bandwidth);

	DDPQOS("%s set %s/%d peak %u\n", __func__,
			mtk_dump_comp_str_id(comp_id), comp_id, bandwidth);

	if (bandwidth > 0 && respective_ostdl != true)
		icc = MTK_MMQOS_MAX_BW;
	else if (debug_deteriorate)
		icc = 0;

	mtk_icc_set_bw(request, 0, icc);
	if (debug_mmqos)
		DRM_MMP_MARK(ostdl, (comp_id << 16) | bandwidth, respective_ostdl);
}

static bool mtk_disp_check_segment(struct mtk_drm_crtc *mtk_crtc,
				struct mtk_drm_private *priv)
{
	bool ret = true;
	int hact = 0;
	int vact = 0;
	int vrefresh = 0;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s, mtk_crtc is NULL\n", __func__);
		return ret;
	}

	if (IS_ERR_OR_NULL(priv)) {
		DDPPR_ERR("%s, private is NULL\n", __func__);
		return ret;
	}

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);

	if (priv->data->mmsys_id == MMSYS_MT6897 && !priv->is_tablet) {
		switch (priv->seg_id) {
		case 1:
			if (hact >= 1440 && vrefresh > 120)
				ret = false;
			break;
		case 2:
			if (hact >= 1440 && vrefresh > 144)
				ret = false;
			break;
		default:
			ret = true;
			break;
		}
	}

/*
 *	DDPMSG("%s, segment:%d, mode(%d, %d, %d)\n",
 *			__func__, priv->seg_id, hact, vact, vrefresh);
 */

	if (ret == false)
		DDPPR_ERR("%s, check sement fail: segment:%d, mode(%d, %d, %d)\n",
			__func__, priv->seg_id, hact, vact, vrefresh);

	return ret;
}

int mtk_disp_get_port_hrt_bw(struct mtk_ddp_comp *comp, enum CHANNEL_TYPE type)
{
	struct mtk_larb_port_bw port_bw = { 0 };
	int ret = 0;

	if (IS_ERR_OR_NULL(comp))
		return -EINVAL;

	port_bw.larb_id = -1;
	port_bw.bw = 0;
	port_bw.type = type;
	ret = mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_GET_LARB_PORT_HRT_BW, &port_bw);
	if (port_bw.larb_id >= 0)
		return port_bw.bw;

	return 0;
}

static unsigned int mtk_disp_calc_max_channel_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	unsigned int ch_bw[BW_CHANNEL_NR] = { 0 };
	unsigned int i, max_ch_bw = 0;

	for (i = 0 ; i < BW_CHANNEL_NR ; i++) {
		for (crtc_idx = 0; crtc_idx < MAX_CRTC; crtc_idx++)
			ch_bw[i] += priv->req_hrt_channel_bw[crtc_idx][i];
	}

	for (int i = 0; i < BW_CHANNEL_NR; i++) {
		if (ch_bw[i] > max_ch_bw)
			max_ch_bw = ch_bw[i];
	}

	DDPDBG("%s max_ch_bw:%u, ch:%u,%u,%u,%u\n",
		__func__, max_ch_bw, ch_bw[0], ch_bw[1], ch_bw[2], ch_bw[3]);

	return max_ch_bw;
}

void mtk_disp_update_channel_bw_by_layer_MT6989(unsigned int layer, unsigned int bpp,
		unsigned int *subcomm_bw_sum, unsigned int size,
		unsigned int bw_base, enum CHANNEL_TYPE type)
{
	/* sub_comm0: layer0 + layer4 + layer9
	 * sub_comm1: layer1 + layer5 + layer8
	 * sub_comm2: layer2 + layer7 + layer11
	 * sub_comm3: layer3 + layer6 + layer10
	 */
	if (!bpp || IS_ERR_OR_NULL(subcomm_bw_sum) || size < 4)
		return;

	if (layer == 0 || layer == 4 || layer == 9)
		subcomm_bw_sum[0] += bw_base * bpp / 4;
	else if (layer == 1 || layer == 5 || layer == 8)
		subcomm_bw_sum[1] += bw_base * bpp / 4;
	else if (layer == 2 || layer == 7 || layer == 11)
		subcomm_bw_sum[2] += bw_base * bpp / 4;
	else if (layer == 3 || layer == 6 || layer == 10)
		subcomm_bw_sum[3] += bw_base * bpp / 4;
}

void mtk_disp_update_channel_bw_by_larb_MT6989(struct mtk_larb_port_bw *port_bw,
	unsigned int *subcomm_bw_sum, unsigned int size, enum CHANNEL_TYPE type)
{
	if (IS_ERR_OR_NULL(port_bw) || IS_ERR_OR_NULL(subcomm_bw_sum) || size < 4)
		return;

	if (port_bw->larb_id == 0 || port_bw->larb_id == 37)
		subcomm_bw_sum[0] += port_bw->bw;
	if (port_bw->larb_id == 1 || port_bw->larb_id == 32 || port_bw->larb_id == 36)
		subcomm_bw_sum[1] += port_bw->bw;
	if (port_bw->larb_id == 20 || port_bw->larb_id == 33 || port_bw->larb_id == 35)
		subcomm_bw_sum[2] += port_bw->bw;
	if (port_bw->larb_id == 21 || port_bw->larb_id == 34)
		subcomm_bw_sum[3] += port_bw->bw;
}

void mtk_disp_update_channel_bw_by_layer_MT6899(unsigned int layer, unsigned int bpp,
		unsigned int *subcomm_bw_sum, unsigned int size,
		unsigned int bw_base, enum CHANNEL_TYPE type)
{
	/* sub_comm0: layer0 + layer4
	 * sub_comm1: layer1 + layer5
	 * sub_comm2: layer2 + layer6
	 * sub_comm3: layer3 + layer7
	 */
	if (!bpp || IS_ERR_OR_NULL(subcomm_bw_sum) || size < 4)
		return;

	if (layer == 0 || layer == 4)
		subcomm_bw_sum[0] += bw_base * bpp / 4;
	else if (layer == 1 || layer == 5)
		subcomm_bw_sum[1] += bw_base * bpp / 4;
	else if (layer == 2 || layer == 6)
		subcomm_bw_sum[2] += bw_base * bpp / 4;
	else if (layer == 3 || layer == 7)
		subcomm_bw_sum[3] += bw_base * bpp / 4;
}

void mtk_disp_update_channel_bw_by_larb_MT6899(struct mtk_larb_port_bw *port_bw,
	unsigned int *subcomm_bw_sum, unsigned int size, enum CHANNEL_TYPE type)
{
	if (IS_ERR_OR_NULL(port_bw) || IS_ERR_OR_NULL(subcomm_bw_sum) || size < 4)
		return;

	if (port_bw->larb_id == 0 || port_bw->larb_id == 2)
		subcomm_bw_sum[0] += port_bw->bw;
	if (port_bw->larb_id == 1 || port_bw->larb_id == 33)
		subcomm_bw_sum[1] += port_bw->bw;
	if (port_bw->larb_id == 20 || port_bw->larb_id == 32)
		subcomm_bw_sum[2] += port_bw->bw;
	if (port_bw->larb_id == 21 || port_bw->larb_id == 3)
		subcomm_bw_sum[3] += port_bw->bw;
}

static void mtk_disp_get_channel_bw_of_ovl(struct mtk_drm_crtc *mtk_crtc,
		unsigned int *subcomm_bw_sum, unsigned int size,
		unsigned int bw_base, enum CHANNEL_TYPE type)
{
	unsigned int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	int i = 0;

	if (!priv || IS_ERR_OR_NULL(priv->data->update_channel_bw_by_layer) ||
		IS_ERR_OR_NULL(subcomm_bw_sum))
		return;

	for (i = 0; i < MAX_LAYER_NR; i++)
		priv->data->update_channel_bw_by_layer(i, mtk_crtc->usage_ovl_fmt[i],
				subcomm_bw_sum, size, bw_base, type);

	if (size >= 4)
		DDPQOS("%s, crtc:%d, OVL channel BW:%u,%u,%u,%u, type:%d\n",
			__func__, crtc_idx, subcomm_bw_sum[0], subcomm_bw_sum[1],
			subcomm_bw_sum[2], subcomm_bw_sum[3], type);
}

static void mtk_disp_get_channel_bw_of_pq(struct mtk_drm_crtc *mtk_crtc,
		unsigned int *subcomm_bw_sum, unsigned int size, enum CHANNEL_TYPE type)
{
	unsigned int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_larb_port_bw port_bw;
	struct mtk_ddp_comp *comp = NULL;
	int i = 0, j = 0, ret = 0;

	if (!priv || IS_ERR_OR_NULL(priv->data->update_channel_bw_by_larb) ||
		IS_ERR_OR_NULL(subcomm_bw_sum))
		return;

	for (i = 0; i < DDP_PATH_NR; i++) {
		if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
			continue;

		for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, i) {
			port_bw.larb_id = -1;
			port_bw.bw = 0;
			port_bw.type = CHANNEL_HRT_RW;
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_GET_LARB_PORT_HRT_BW,&port_bw);
			if (port_bw.larb_id >= 0 && port_bw.bw > 0)
				priv->data->update_channel_bw_by_larb(&port_bw, subcomm_bw_sum,
						size, type);
		}
	}

	if (size >= 4)
		DDPQOS("%s, crtc:%d, PQ channel BW:%u,%u,%u,%u type:%d ret:%d\n",
			__func__, crtc_idx, subcomm_bw_sum[0], subcomm_bw_sum[1],
			subcomm_bw_sum[2], subcomm_bw_sum[3], type, ret);
}

static void mtk_disp_get_channel_bw_of_wdma(struct mtk_drm_crtc *mtk_crtc, unsigned int *subcomm_bw_sum,
		unsigned int size, enum CHANNEL_TYPE type, enum addon_scenario scn)
{
	unsigned int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_larb_port_bw port_bw;
	struct mtk_ddp_comp *comp = NULL;
	int ret = 0;
	struct mtk_crtc_state *mtk_crtc_state = to_mtk_crtc_state(crtc->state);

	if (!priv || IS_ERR_OR_NULL(priv->data->update_channel_bw_by_larb) ||
		IS_ERR_OR_NULL(subcomm_bw_sum))
		return;

	if (scn == IDLE_WDMA_WRITE_BACK &&
		(!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLEMGR_BY_WB) ||
		mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLEMGR_BY_REPAINT)))
		return;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	/* only vdo panel support IWB*/
	if (!comp || (scn == IDLE_WDMA_WRITE_BACK && mtk_dsi_is_cmd_mode(comp)))
		return;

	if (scn == WDMA_WRITE_BACK && !mtk_crtc_state->prop_val[CRTC_PROP_OUTPUT_ENABLE])
		return;

	comp = mtk_disp_get_wdma_comp_by_scn(crtc, scn);
	if (!comp)
		return;

	port_bw.larb_id = -1;
	port_bw.bw = 0;
	port_bw.type = CHANNEL_HRT_RW;
	ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_GET_LARB_PORT_HRT_BW,&port_bw);
	if (port_bw.larb_id >= 0 && port_bw.bw > 0)
		priv->data->update_channel_bw_by_larb(&port_bw, subcomm_bw_sum,
					size, type);

	if (size >= 4)
		DDPQOS("%s, crtc:%d, wdma:%u scn:%d channel BW:%u,%u,%u,%u type:%d ret:%d\n",
			__func__, crtc_idx, comp->id, scn, subcomm_bw_sum[0],
			subcomm_bw_sum[1], subcomm_bw_sum[2], subcomm_bw_sum[3], type, ret);
}

static void __mtk_disp_get_channel_hrt_bw_by_scope(struct mtk_drm_crtc *mtk_crtc,
			unsigned int scope, unsigned int *result, unsigned int size)
{
	unsigned int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	unsigned int ovl_bw = 0, i;
	unsigned int bw_base = mtk_drm_primary_frame_bw(&mtk_crtc->base);
	unsigned int subcomm_bw_sum[BW_CHANNEL_NR] = { 0 };
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;

	if (size < BW_CHANNEL_NR)
		return;

	if (bw_base == MAX_MMCLK) {
		result[0] = MAX_MMCLK;
		return;
	}

	if (scope & CHANNEL_BW_OF_OVL) {
		mtk_disp_get_channel_bw_of_ovl(mtk_crtc, subcomm_bw_sum,
					ARRAY_SIZE(subcomm_bw_sum), bw_base, CHANNEL_HRT_RW);
	} else {
		/*only layer0 works at home screen idle*/
		ovl_bw =  bw_base * 125 / 100;
		subcomm_bw_sum[0] += ovl_bw;
		DDPQOS("%s, crtc:%d, OVL channel BW:%u,%u,%u,%u\n",
			__func__, crtc_idx, subcomm_bw_sum[0], subcomm_bw_sum[1],
			subcomm_bw_sum[2], subcomm_bw_sum[3]);
	}

	if (crtc_idx == 0 && (scope & CHANNEL_BW_OF_PQ))
		mtk_disp_get_channel_bw_of_pq(mtk_crtc, subcomm_bw_sum,
					ARRAY_SIZE(subcomm_bw_sum), CHANNEL_HRT_RW);

	if (crtc_idx == 0 && (scope & CHANNEL_BW_OF_WDMA_IWB))
		mtk_disp_get_channel_bw_of_wdma(mtk_crtc, subcomm_bw_sum,
					ARRAY_SIZE(subcomm_bw_sum), CHANNEL_HRT_RW, IDLE_WDMA_WRITE_BACK);

	if (priv->data->mmsys_id == MMSYS_MT6899 && crtc_idx == 0 && (scope & CHANNEL_BW_OF_WDMA_CWB))
		mtk_disp_get_channel_bw_of_wdma(mtk_crtc, subcomm_bw_sum,
					ARRAY_SIZE(subcomm_bw_sum), CHANNEL_HRT_RW, WDMA_WRITE_BACK);

	for (i = 0 ; i < BW_CHANNEL_NR ; i++)
		result[i] = subcomm_bw_sum[i];
}

unsigned int mtk_disp_get_channel_idx_MT6991(enum CHANNEL_TYPE type, unsigned int i)
{
	unsigned int idx = 0;

	switch (type) {
	case CHANNEL_SRT_READ:
		idx = i * 4;//0,4,8,12
		break;
	case CHANNEL_SRT_WRITE:
		idx = i * 4 + 1;//1,5,9,13
		break;
	case CHANNEL_HRT_READ:
		idx = i * 4 + 2;//2,6,10,14
		break;
	case CHANNEL_HRT_WRITE:
		idx = i * 4 + 3;//3,7,11,15
		break;
	}

	return idx;
}

void mtk_disp_set_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw, int i)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	unsigned int total = 0, j, idx;

	if (i < 0) {
		DDPPR_ERR("%s i invalid\n", __func__);
		return;
	}

	if (!priv) {
		DDPPR_ERR("%s priv is not assigned\n", __func__);
		return;
	}

	if (debug_channel_bw[i])
		bw = debug_channel_bw[i];

	if (priv->req_hrt_channel_bw[crtc_idx][i] == bw)
		return;

	priv->req_hrt_channel_bw[crtc_idx][i] = bw;

	for (j = 0; j < MAX_CRTC; j++)
		total += priv->req_hrt_channel_bw[j][i];

	if (debug_deteriorate)
		total = 0;

	idx = priv->data->get_channel_idx(CHANNEL_HRT_READ, i);
	mtk_vidle_channel_bw_set(total, idx);
	DRM_MMP_MARK(channel_bw, total, i);

	DDPINFO("%s, CRTC%d chan[%d] bw=%u, total=%u\n",
						__func__, crtc_idx, i, bw, total);
}

void mtk_disp_update_channel_hrt_MT6991(struct mtk_drm_crtc *mtk_crtc,
						unsigned int bw_base, unsigned int channel_bw[])
{
	int i = 0, j;
	int max_ovl_phy_layer = 12; // 6991 phy ovl layer num
	unsigned int subcomm_bw_sum[4] = {0};
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	unsigned int compr_ratio = 90;
	int oddmr_hrt = 0;
	struct mtk_ddp_comp *comp;

	if (!mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[DDP_FIRST_PATH])
		return;

	/* sub_comm0: exdma2(0) + exdma7(5) + 1_exdma5(11) + (1_exdma8)(14)
	 * sub_comm1: exdma3(1) + exdma6(4) + 1_exdma4(10) + (1_exdma9)(15)
	 * sub_comm2: exdma4(2) + exdma9(7) + 1_exdma3(9) + 1_exdma6(12)
	 * sub_comm3: exdma5(3) + exdma8(6) + 1_exdma2(8) + 1_exdma7(13)
	 */
	for (i = 0; i < MAX_LAYER_NR; i++) {
		if (mtk_crtc->usage_ovl_fmt[i]) {
			unsigned int bw = bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;

			if (mtk_crtc->usage_ovl_compr[i])
				bw = bw * compr_ratio / 100;

			if (i == 0 || i == 5 || i == 11 || i == 14)
				subcomm_bw_sum[0] += bw;
			else if (i == 1 || i == 4 || i == 10 || i == 15)
				subcomm_bw_sum[1] += bw;
			else if (i == 2 || i == 7 || i == 9 || i == 12)
				subcomm_bw_sum[2] += bw;
			else if (i == 3 || i == 6 || i == 8 || i == 13)
				subcomm_bw_sum[3] += bw;
		}
	}

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_ODDMR)
			oddmr_hrt += mtk_disp_get_port_hrt_bw(comp, CHANNEL_HRT_RW);
	}

	if (oddmr_hrt > 0)
		subcomm_bw_sum[2] += oddmr_hrt;

	//TODO: consider discrete path
	/* channel_bw[0]: comm0_ch0: sub_comm0
	 * channel_bw[1]: comm0_ch1: sub_comm1
	 * channel_bw[2]: comm1_ch0: sub_comm3
	 * channel_bw[3]: comm1_ch1: sub_comm2
	 */
	channel_bw[0] = subcomm_bw_sum[0];
	channel_bw[1] = subcomm_bw_sum[1];
	channel_bw[2] = subcomm_bw_sum[3];
	channel_bw[3] = subcomm_bw_sum[2];
}

static void mtk_disp_channel_srt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc)
{
	int i,j;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int channel_sum = 0;
	char dbg_msg[512] = {0};
	int written = 0;

	if (mtk_disp_get_logger_enable())
		written = scnprintf(dbg_msg, 512, "%s = ", __func__);

	for (i = 0; i < BW_CHANNEL_NR; i++) {
		for (j = 0; j < MAX_CRTC; j++)
			channel_sum += priv->srt_channel_bw_sum[j][i];
		mtk_vidle_channel_bw_set(channel_sum, (i * 4)); //0, 4, 8, 12

		if (mtk_disp_get_logger_enable())
			written += scnprintf(dbg_msg + written, 512 - written, "[%d]", channel_sum);

		channel_sum = 0;
	}
	DDPINFO("%s\n", dbg_msg);
}

static void mtk_disp_clear_channel_srt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc)
{
	int i;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);

	for (i = 0; i < BW_CHANNEL_NR; i++)
		priv->srt_channel_bw_sum[crtc_idx][i] = 0;
}

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
void mtk_disp_hrt_mmclk_request_mt6768(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	int layer_num;
	int ret;
	unsigned long long bw_base;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct hrt_mmclk_request *req_level;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (bw_base != 0)
		layer_num = bw * 10 / bw_base;
	else {
		DDPINFO("%s-error: frame_bw is zero, skip request mmclk\n", __func__);
		return;
	}

	if (is_bdg_supported()) {
		if (mtk_crtc->base.mode.vdisplay / mtk_crtc->base.mode.hdisplay > 18 / 9)
			req_level = hrt_req_level_bdg_fhdp_mt6768;
		else
			req_level = hrt_req_level_bdg_mt6768;
	} else {
		if (mtk_crtc->base.mode.vdisplay / mtk_crtc->base.mode.hdisplay > 18 / 9)
			req_level = hrt_req_level_fhdp_mt6768;
		else
			req_level = hrt_req_level_mt6768;
	}

	if (layer_num <= req_level[0].layer_num && bw <= hrt_level_bw_mt6768[0]) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
		ret = regulator_set_voltage(mm_freq_request, req_level[0].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[0].volt);
	} else if ((layer_num > req_level[0].layer_num || bw > hrt_level_bw_mt6768[0])
			&& layer_num <= req_level[1].layer_num && bw <= hrt_level_bw_mt6768[1]) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
		ret = regulator_set_voltage(mm_freq_request, req_level[1].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[1].volt);
	} else if (layer_num > req_level[1].layer_num || bw > hrt_level_bw_mt6768[1]) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
		ret = regulator_set_voltage(mm_freq_request, req_level[2].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[2].volt);
	}
}

void mtk_disp_hrt_mmclk_request_mt6761(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	int layer_num;
	int ret;
	unsigned long long bw_base;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct hrt_mmclk_request *req_level = hrt_req_level_mt6761;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (bw_base != 0)
		layer_num = bw * 10 / bw_base;
	else {
		DDPMSG("%s-error: frame_bw is zero, skip request mmclk\n", __func__);
		return;
	}

	if (layer_num <= req_level[0].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
		ret = regulator_set_voltage(mm_freq_request, req_level[0].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[0].volt);
	} else if (layer_num > req_level[0].layer_num && layer_num <= req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
		ret = regulator_set_voltage(mm_freq_request, req_level[1].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[1].volt);
	} else if (layer_num > req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
		ret = regulator_set_voltage(mm_freq_request, req_level[2].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[2].volt);
	}
}

void mtk_disp_hrt_mmclk_request_mt6765(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	int layer_num;
	int ret;
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	unsigned int ddr_type;
#endif
	unsigned long long bw_base;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	struct hrt_mmclk_request *req_level = hrt_req_level_ddr3_fhd_mt6765;
#else
	struct hrt_mmclk_request *req_level = hrt_req_level_ddr4_fhd_mt6765;
#endif
	struct mtk_ddp_comp *output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct drm_display_mode *mode = NULL;
	unsigned int max_fps = 0;
	bool is_tall_aspect_ratio = ((mtk_crtc->base.mode.vdisplay*100) /
					mtk_crtc->base.mode.hdisplay) > 200 /*18:9*/;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (bw_base != 0)
		layer_num = bw * 10 / bw_base;
	else {
		DDPINFO("%s-error: frame_bw is zero, skip request mmclk\n", __func__);
		return;
	}

	mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
	if (mode)
		max_fps = drm_mode_vrefresh(mode);
	if (max_fps >= 90) {
		if (is_tall_aspect_ratio ||
			(mtk_crtc->base.mode.hdisplay > 800)/*fhdp*/) {
			DDPINFO("%s DDR4, fhd@90, h:%d", __func__, mtk_crtc->base.mode.hdisplay);
			req_level = hrt_req_level_ddr4_fhd_hfps_mt6765;
		} else {
			DDPINFO("%s DDR4, hd@90, h:%d", __func__, mtk_crtc->base.mode.hdisplay);
			req_level = hrt_req_level_ddr4_hd_hfps_mt6765;
		}
	} else { /*(max_fps < 90)*/
#if IS_ENABLED(CONFIG_MTK_DRAMC)
		ddr_type = mtk_dramc_get_ddr_type();
		if (ddr_type == TYPE_LPDDR4 || ddr_type == TYPE_LPDDR4X) {
			if (is_tall_aspect_ratio ||
				(mtk_crtc->base.mode.hdisplay > 800)/*fhdp*/) {
				DDPINFO("%s DDR4, fhd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr4_fhd_mt6765;
			} else {
				DDPINFO("%s DDR4, hd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr4_hd_mt6765;
			}
		} else { /*ddr3*/
			if (is_tall_aspect_ratio ||
				(mtk_crtc->base.mode.hdisplay > 800)/*fhdp*/) {
				DDPINFO("%s DDR3, fhd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr3_fhd_mt6765;
			} else {
				DDPINFO("%s DDR3, hd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr3_hd_mt6765;
			}
		}
#endif
	} /*(max_fps >= 90)*/

	if (layer_num <= req_level[0].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
		ret = regulator_set_voltage(mm_freq_request, req_level[0].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[0].volt);
	} else if (layer_num > req_level[0].layer_num && layer_num <= req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
		ret = regulator_set_voltage(mm_freq_request, req_level[1].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[1].volt);
	} else if (layer_num > req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
		ret = regulator_set_voltage(mm_freq_request, req_level[2].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[2].volt);
	}
}
#endif

void mtk_disp_channel_srt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6991)
		mtk_disp_channel_srt_bw_MT6991(mtk_crtc);

}

void mtk_disp_clear_channel_srt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6991)
		mtk_disp_clear_channel_srt_bw_MT6991(mtk_crtc);
}

void mtk_disp_total_srt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	unsigned int crtc_idx = 0;
	unsigned int total_srt_sum = 0;

	if(!mtk_crtc) {
		DDPPR_ERR("%s:mtk_crtc is NULL\n", __func__);
		return;
	}

	crtc = &mtk_crtc->base;
	crtc_idx = drm_crtc_index(crtc);
	priv = crtc->dev->dev_private;

	if(!priv) {
		DDPPR_ERR("%s:priv is NULL\n", __func__);
		return;
	}

	if (priv->total_srt[crtc_idx] == bw)
		return;

	priv->total_srt[crtc_idx] = bw;
	for (int i = 0; i < MAX_CRTC; i++)
		total_srt_sum += priv->total_srt[i];

	DDPINFO("%s crtc%d=%d total=%d\n", __func__, crtc_idx, bw, total_srt_sum);
	mtk_vidle_srt_bw_set(total_srt_sum);
}

int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp;
	unsigned int tmp, total = 0, tmp1 = 0, bw_base  = 0;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	int i, ret = 0, ovl_num = 0;

	tmp = bw;

	if (mtk_crtc == NULL)
		return 0;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return 0;

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6761 ||
		priv->data->mmsys_id == MMSYS_MT6765) {
		DDPMSG("%s: no need to set module hrt bw for legacy!\n", __func__);
	} else if (!priv->data->respective_ostdl)
		mtk_disp_set_module_hrt(mtk_crtc, bw, NULL, PMQOS_SET_HRT_BW);

	/* skip same HRT BW */
	if (priv->req_hrt[crtc_idx] == tmp)
		return 0;

	priv->req_hrt[crtc_idx] = tmp;

	for (i = 0; i < MAX_CRTC; ++i)
		total += priv->req_hrt[i];

	if (bw == MAX_MMCLK) {
		DDPMSG("%s,total:%d->65535\n", __func__, total);
		total = 65535;
	} else if (debug_deteriorate) {
		DDPMSG("%s,total:%d->0\n", __func__, total);
		total = 0;
	}

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
	if (priv->data->mmsys_id == MMSYS_MT6768)
		mtk_disp_hrt_mmclk_request_mt6768(mtk_crtc, tmp);
	else if (priv->data->mmsys_id == MMSYS_MT6761)
		mtk_disp_hrt_mmclk_request_mt6761(mtk_crtc, tmp);
	else if (priv->data->mmsys_id == MMSYS_MT6765)
		mtk_disp_hrt_mmclk_request_mt6765(mtk_crtc, tmp);
#else
	if (!IS_ERR_OR_NULL(priv->hrt_bw_request)) {
		if ((priv->data->mmsys_id == MMSYS_MT6897) &&
			(mtk_disp_check_segment(mtk_crtc, priv) == false))
			mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(1));
		else
			mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(total));
	}
	if (debug_vidle_bw)
		total = debug_vidle_bw;

	mtk_vidle_hrt_bw_set(total);
#endif
	DRM_MMP_MARK(hrt_bw, 0, tmp);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_HRT_BY_LARB)) {

		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DPTX) {
			if (!IS_ERR_OR_NULL(priv->dp_hrt_by_larb)) {
				tmp = tmp / (mtk_crtc->is_dual_pipe + 1);
				mtk_icc_set_bw(priv->dp_hrt_by_larb, 0, MBps_to_icc(tmp));
				DDPINFO("%s, CRTC%d(DP) HRT total=%u larb bw=%u dual=%d\n",
					__func__, crtc_idx, total, tmp, mtk_crtc->is_dual_pipe);
			}
		} else if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI &&
				(priv->data->mmsys_id != MMSYS_MT6989) &&
				(priv->data->mmsys_id != MMSYS_MT6899) &&
				(priv->data->mmsys_id != MMSYS_MT6991)) {
			if (total > 0) {
				bw_base = mtk_drm_primary_frame_bw(crtc);
				ovl_num = bw_base > 0 ? total / bw_base : 0;
				tmp1 = ((bw_base / 2) > total) ? total : (ovl_num < 3) ?
					(bw_base / 2) : (ovl_num < 5) ?
					bw_base : (bw_base * 3 / 2);
			}

			if (!IS_ERR_OR_NULL(priv->hrt_by_larb)) {
				if ((priv->data->mmsys_id == MMSYS_MT6897) &&
					(mtk_disp_check_segment(mtk_crtc, priv) == false))
					mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(1));
				else
					mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(tmp1));
			}

			mtk_vidle_dvfs_bw_set(tmp1);
			mtk_crtc->qos_ctx->last_larb_hrt_req = tmp1;
			DDPINFO("%s, CRTC%d HRT bw=%u total=%u larb bw=%u ovl_num=%d bw_base=%d\n",
				__func__, crtc_idx, tmp, total, tmp1, ovl_num, bw_base);
		} else
			DDPINFO("set CRTC %d HRT bw %u %u\n", crtc_idx, tmp, total);
	} else
		DDPINFO("set CRTC %d HRT bw %u %u\n", crtc_idx, tmp, total);

	return ret;
}

void mtk_aod_scp_set_BW(void)
{
	mtk_ovl_set_aod_scp_hrt();
	mtk_vidle_hrt_bw_set(7000);
	mtk_vidle_srt_bw_set(7000);
	mtk_vidle_dvfs_set(5);
	mtk_vidle_dvfs_bw_set(7000);
	mtk_vidle_channel_bw_set(7000, 0);
	mtk_vidle_channel_bw_set(7000, 1);
	mtk_vidle_channel_bw_set(7000, 2);
	mtk_vidle_channel_bw_set(7000, 3);
}
EXPORT_SYMBOL(mtk_aod_scp_set_BW);

void mtk_disp_get_channel_hrt_bw_by_scope(struct mtk_drm_crtc *mtk_crtc,
		unsigned int scope, unsigned int *result, unsigned int size)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *comp;
	unsigned int crtc_idx = 0;

	if (IS_ERR_OR_NULL(mtk_crtc) ||
		mtk_crtc->ddp_mode >= DDP_MODE_NR ||
		IS_ERR_OR_NULL(result) || size == 0)
		return;

	crtc = &mtk_crtc->base;
	crtc_idx = drm_crtc_index(crtc);
	priv = crtc->dev->dev_private;
	if (!priv ||
		!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MAX_CHANNEL_HRT))
		return;

	comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (comp && (mtk_ddp_comp_get_type(comp->id) == MTK_DSI ||
		mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DPTX))
		__mtk_disp_get_channel_hrt_bw_by_scope(mtk_crtc, scope, result, size);

	return;
}

void mtk_disp_get_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
		unsigned int *result, unsigned int size)
{
	mtk_disp_get_channel_hrt_bw_by_scope(mtk_crtc, CHANNEL_BW_DEFAULT, result, size);
}

unsigned int mtk_disp_set_per_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
		unsigned int bw, unsigned int ch_idx, bool force, const char *master)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	unsigned int last_ch_bw = 0, max_channel_bw = 0;
	int crtc_idx = 0;

	if (IS_ERR_OR_NULL(mtk_crtc) || ch_idx >= BW_CHANNEL_NR )
		goto out;

	crtc = &mtk_crtc->base;
	crtc_idx = drm_crtc_index(crtc);
	if (IS_ERR_OR_NULL(crtc->dev) || IS_ERR_OR_NULL(crtc->dev->dev_private))
		goto out;

	priv = crtc->dev->dev_private;
	if (!priv ||
		!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MAX_CHANNEL_HRT))
		goto out;

	/* ch bw keep unchanged */
	if (priv->req_hrt_channel_bw[crtc_idx][ch_idx] == bw)
		goto out;

	last_ch_bw = priv->req_hrt_channel_bw[crtc_idx][ch_idx];
	/* ch bw slow down */
	if (!force && priv->req_hrt_channel_bw[crtc_idx][ch_idx] > bw) {
		if (debug_mmqos)
			DDPMSG("%s, CRTC%d SLOW-DOWN channel:%u(%u->%u) max:%u(%u,%u,%u,%u) force:%d\n",
				IS_ERR_OR_NULL(master) ? "unknown" : master,
				crtc_idx, ch_idx, last_ch_bw, bw,
				priv->last_max_channel_req,
				priv->req_hrt_channel_bw[crtc_idx][0],
				priv->req_hrt_channel_bw[crtc_idx][1],
				priv->req_hrt_channel_bw[crtc_idx][2],
				priv->req_hrt_channel_bw[crtc_idx][3], force);
		return bw;
	}

	/* ch bw fast up or final down */
	priv->req_hrt_channel_bw[crtc_idx][ch_idx] = bw;
	CRTC_MMP_MARK(crtc_idx, channel_bw, ch_idx, bw);
	max_channel_bw = mtk_disp_calc_max_channel_bw(mtk_crtc);
	if (debug_mmqos)
		DDPMSG("%s, CRTC%d %s channel:%u(%u->%u) max:%u->%u(%u,%u,%u,%u) force:%d\n",
			IS_ERR_OR_NULL(master) ? "unknown" : master,
			crtc_idx, force ? "FINAL-DOWN" : "FAST-UP",
			ch_idx, last_ch_bw, bw,
			priv->last_max_channel_req, max_channel_bw,
			priv->req_hrt_channel_bw[crtc_idx][0],
			priv->req_hrt_channel_bw[crtc_idx][1],
			priv->req_hrt_channel_bw[crtc_idx][2],
			priv->req_hrt_channel_bw[crtc_idx][3], force);

	/* max channel bw keep unchanged */
	if (priv->last_max_channel_req == max_channel_bw)
		goto out;

	/* max channel bw fast up or final down */
	DRM_MMP_MARK(channel_bw, max_channel_bw, crtc_idx);
	mtk_vidle_dvfs_bw_set(max_channel_bw);
	priv->last_max_channel_req = max_channel_bw;

out:
	return NO_PENDING_HRT;
}

unsigned int mtk_disp_set_max_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
		unsigned int *bw, unsigned int size, const char *master)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	unsigned int max_channel_bw = 0, i;
	int crtc_idx = 0;

	if (IS_ERR_OR_NULL(mtk_crtc))
		return 0;

	crtc = &mtk_crtc->base;
	crtc_idx = drm_crtc_index(crtc);
	if (IS_ERR_OR_NULL(crtc->dev) || IS_ERR_OR_NULL(crtc->dev->dev_private))
		return 0;

	priv = crtc->dev->dev_private;
	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MAX_CHANNEL_HRT) ||
		size > BW_CHANNEL_NR || size == 0 || IS_ERR_OR_NULL(bw))
		goto out;

	/* update all channel hrt bw */
	for (i = 0; i< size; i++) {
		if (priv->req_hrt_channel_bw[crtc_idx][i] != bw[i]) {
			priv->req_hrt_channel_bw[crtc_idx][i] = bw[i];
			CRTC_MMP_MARK(crtc_idx, channel_bw, (i | 0xf0ce0000), bw[i]);
		}
	}
	max_channel_bw = mtk_disp_calc_max_channel_bw(mtk_crtc);

	if (debug_mmqos)
		DDPMSG("%s, CRTC%d UPDATE channel max:%u->%u(%u,%u,%u,%u) size:%u\n",
			IS_ERR_OR_NULL(master) ? "unknown" : master, crtc_idx,
			priv->last_max_channel_req, max_channel_bw,
			priv->req_hrt_channel_bw[crtc_idx][0],
			priv->req_hrt_channel_bw[crtc_idx][1],
			priv->req_hrt_channel_bw[crtc_idx][2],
			priv->req_hrt_channel_bw[crtc_idx][3], size);

	/* max channel bw keep unchanged */
	if (priv->last_max_channel_req == max_channel_bw)
		goto out;

	/* max channel force update */
	DRM_MMP_MARK(channel_bw, max_channel_bw, (crtc_idx | 0xf0ce0000));
	mtk_vidle_dvfs_bw_set(max_channel_bw);
	priv->last_max_channel_req = max_channel_bw;

out:
	return priv->last_max_channel_req;
}

void mtk_drm_pan_disp_set_hrt_bw(struct drm_crtc *crtc, const char *caller)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_display_mode *mode;
	unsigned int bw = 0, bw_base = 0, i;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int channel_hrt[BW_CHANNEL_NR] = {0};

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);
	mode = &crtc->state->adjusted_mode;

	bw = _layering_get_frame_bw(crtc, mode);
	mtk_disp_set_hrt_bw(mtk_crtc, bw);
	DDPINFO("%s:pan_disp_set_hrt_bw: %u\n", caller, bw);

	/* FIXME: this value is zero when booting, will be assigned in exdma_layer_config */
	if (priv->data->mmsys_id == MMSYS_MT6991)
		mtk_crtc->usage_ovl_fmt[1] = 4;

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MAX_CHANNEL_HRT)) {
		mtk_crtc->usage_ovl_fmt[0] = 4;
		mtk_disp_get_channel_hrt_bw(mtk_crtc, channel_hrt, ARRAY_SIZE(channel_hrt));
		mtk_disp_set_max_channel_hrt_bw(mtk_crtc, channel_hrt,
					ARRAY_SIZE(channel_hrt), __func__);
		for (i = 0 ; i < ARRAY_SIZE(channel_hrt); i++)
			mtk_crtc->qos_ctx->last_channel_req[i] = channel_hrt[i];
	}

	if (priv->data->update_channel_hrt) {
		priv->data->update_channel_hrt(mtk_crtc, bw, channel_hrt);
		DDPINFO("%s channel[%u][%u][%u][%u]", __func__,
			channel_hrt[0], channel_hrt[1], channel_hrt[2], channel_hrt[3]);
		for (i = 0; i < BW_CHANNEL_NR; i++)
			mtk_disp_set_channel_hrt_bw(mtk_crtc, channel_hrt[i], i);
	}

	if (priv->data->respective_ostdl) {
		bw_base = mtk_drm_primary_frame_bw(crtc);
		mtk_disp_set_module_hrt(mtk_crtc, bw_base, NULL, PMQOS_SET_HRT_BW);
	}
}

void mtk_disp_hrt_repaint_blocking(const unsigned int hrt_idx)
{
	int i, ret;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);

	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev_crtc->dev);
	for (i = 0; i < 5; ++i) {
		ret = wait_event_timeout(
			mtk_crtc->qos_ctx->hrt_cond_wq,
			atomic_read(&mtk_crtc->qos_ctx->hrt_cond_sig),
			HZ / 5);
		if (ret == 0)
			DDPINFO("wait repaint timeout %d\n", i);
		atomic_set(&mtk_crtc->qos_ctx->hrt_cond_sig, 0);
		if (atomic_read(&mtk_crtc->qos_ctx->last_hrt_idx) >= hrt_idx)
			break;
	}
}

/* force report all display's mmqos BW include SRT & HRT */
void mtk_disp_mmqos_bw_repaint(struct mtk_drm_private *priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	unsigned int i, j, k, c, tmp = 1, flag = DISP_BW_FORCE_UPDATE;
	int ret = 0;
	bool is_hrt;

	for (c = 0 ; c < MAX_CRTC ; ++c) {
		crtc = priv->crtc[c];
		if (crtc == NULL)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);

		DDP_MUTEX_LOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);

		if (!(mtk_crtc->enabled)) {
			DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);
			continue;
		}

		for (k = 0; k < DDP_PATH_NR; k++) {
			is_hrt = (mtk_crtc->ddp_mode < DDP_MODE_NR) ?
				mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[k] : false;
			tmp = mtk_drm_primary_frame_bw(crtc);
			for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, k) {
				//report SRT BW
				ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_UPDATE_BW,
						&flag);
				//report HRT BW if path is HRT
				if (is_hrt)
					ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
							&tmp);
			}
			if (!mtk_crtc->is_dual_pipe)
				continue;
			for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i) {
				//report SRT BW
				ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_UPDATE_BW,
						&flag);
				//report HRT BW if path is HRT
				if (is_hrt)
					ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
							&tmp);
			}
		}

		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);
	}
}

int mtk_disp_hrt_cond_change_cb(struct notifier_block *nb, unsigned long value,
				void *v)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);
	unsigned int hrt_idx;
	bool locked = true;

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

	/* No need to repaint when display suspend */
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

		return 0;
	}

	switch (value) {
	case BW_THROTTLE_START: /* CAM on */
		DDPMSG("DISP BW Throttle start\n");
		/* TODO: concider memory session */
		DDPINFO("CAM trigger repaint\n");
		hrt_idx = _layering_rule_get_hrt_idx(drm_crtc_index(dev_crtc));
		hrt_idx++;
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
		mtk_disp_hrt_repaint_blocking(hrt_idx);
		mtk_disp_mmqos_bw_repaint(dev_crtc->dev->dev_private);
		locked = false;
		break;
	case BW_THROTTLE_END: /* CAM off */
		DDPMSG("DISP BW Throttle end\n");
		/* TODO: switch DC */
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

		/* bw repaint might hold all crtc's mutex, need unlock current mutex first */
		mtk_disp_mmqos_bw_repaint(dev_crtc->dev->dev_private);
		locked = false;
		break;
	default:
		break;
	}

	if (locked)
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

	return 0;
}

struct notifier_block pmqos_hrt_notifier = {
	.notifier_call = mtk_disp_hrt_cond_change_cb,
};

int mtk_disp_hrt_bw_dbg(void)
{
	mtk_disp_hrt_cond_change_cb(NULL, BW_THROTTLE_START, NULL);

	return 0;
}

int mtk_disp_hrt_cond_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;
	unsigned int i;

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s:mtk_crtc is NULL\n", __func__);
		return -EINVAL;
	}

	priv = mtk_crtc->base.dev->dev_private;

	mtk_crtc->qos_ctx = vmalloc(sizeof(struct mtk_drm_qos_ctx));
	if (mtk_crtc->qos_ctx == NULL) {
		DDPPR_ERR("%s:allocate qos_ctx failed\n", __func__);
		return -ENOMEM;
	}
	atomic_set(&mtk_crtc->qos_ctx->last_hrt_idx, 0);
	mtk_crtc->qos_ctx->last_hrt_req = 0;
	mtk_crtc->qos_ctx->last_mmclk_req_idx = 0;
	mtk_crtc->qos_ctx->last_larb_hrt_req = 0;
	for (i = 0; i < BW_CHANNEL_NR ; i++)
		mtk_crtc->qos_ctx->last_channel_req[i] = 0;

	if (drm_crtc_index(crtc) == 0 && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		mtk_mmqos_register_bw_throttle_notifier(&pmqos_hrt_notifier);

	return 0;
}

static void mtk_drm_mmdvfs_get_avail_freq(struct device *dev)
{
	int i = 0;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int ret;

	step_size = dev_pm_opp_get_opp_count(dev);
	g_freq_steps = kcalloc(step_size, sizeof(unsigned long), GFP_KERNEL);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		g_freq_steps[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	ret = of_property_read_u32(dev->of_node, "lp-mmclk-freq", &lp_freq);
	DDPINFO("%s lp_freq = %d\n", __func__, lp_freq);
}

void mtk_drm_mmdvfs_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	int ret = 0;

	dev_pm_opp_of_add_table(dev);
	mtk_drm_mmdvfs_get_avail_freq(dev);

	/* support DPC and VDISP */
	ret = of_property_read_u8(node, "vdisp-dvfs-opp", &vdisp_opp);
	if (ret == 0) {
		mm_freq_request = devm_regulator_get_optional(dev, "dis1-shutdown");
		if (mm_freq_request == NULL)
			DDPMSG("%s use vdisp opp(%u)\n", __func__, vdisp_opp);
		else if (IS_ERR(mm_freq_request))
			mm_freq_request = NULL;
		else
			DDPMSG("%s use vdisp but regulator flow\n", __func__);
		return;
	}

	/* MMDVFS V2 */
	DDPINFO("%s, try to use MMDVFS V2\n", __func__);
	mm_freq_request = devm_regulator_get_optional(dev, "mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(mm_freq_request))
		DDPPR_ERR("%s, get mmdvfs-dvfsrc-vcore failed\n", __func__);
}

unsigned int mtk_drm_get_mmclk_step_size(void)
{
	return step_size;
}

void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, bool lp_mode,
			const char *caller)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int volt, ret, idx, i;
	int final_lp_mode = true;
	int final_level = -1;
	int cnt = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	idx = drm_crtc_index(crtc);

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s invalid mtk_crtc\n", __func__);
		return;
	}

	/* memory session do not use */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (IS_ERR_OR_NULL(output_comp) ||
		mtk_ddp_comp_get_type(output_comp->id) == MTK_DISP_WDMA) {
		DDPINFO("crtc%d not support set mmclk\n", idx);
		return;
	}

	DDPINFO("%s[%d] g_freq_level[idx=%d]: %d, g_freq_lp[idx=%d]: %d\n",
		__func__, __LINE__, idx, level, idx, lp_mode);

	if (level < 0 || level > (step_size - 1))
		level = -1;

	if (level == g_freq_level[idx] && lp_mode == g_freq_lp[idx])
		return;

	g_freq_lp[idx] = lp_mode;
	g_freq_level[idx] = level;

	for (i = 0; i < CRTC_NUM; i++) {
		if (g_freq_level[i] == -1)
			cnt++;

		if (g_freq_level[i] != -1 && !g_freq_lp[i])
			final_lp_mode = false;

		if (g_freq_level[i] > final_level)
			final_level = g_freq_level[i];
	}

	if (cnt == CRTC_NUM)
		final_lp_mode = false;

	if (final_level >= 0)
		freq = g_freq_steps[final_level];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d] final_level(freq=%d, %lu) final_lp_mode:%d\n",
		__func__, __LINE__, final_level, freq, final_lp_mode);

	if ((vdisp_opp != U8_MAX) && (mm_freq_request == NULL)) {
		if (final_level >= 0)
			vdisp_opp = final_level;
		mtk_vidle_dvfs_set(vdisp_opp);
		return;
	}

	if (mm_freq_request) {
		if (vdisp_opp == U8_MAX) /* not support for vdisp platform */
			mmdvfs_set_lp_mode(final_lp_mode);

		opp = dev_pm_opp_find_freq_ceil(crtc->dev->dev, &freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
		ret = regulator_set_voltage(mm_freq_request, volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
	}
}

void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc,
	unsigned int pixclk, const char *caller)
{
	int i;
	unsigned long freq = pixclk * 1000000;

	g_freq = freq;

	if (freq > g_freq_steps[step_size - 1]) {
		DDPPR_ERR("%s:pixleclk (%lu) is to big for mmclk (%lu)\n",
			caller, freq, g_freq_steps[step_size - 1]);
		mtk_drm_set_mmclk(crtc, step_size - 1, false, caller);
		return;
	}
	if (!freq) {
		mtk_drm_set_mmclk(crtc, -1, false, caller);
		return;
	}
	for (i = step_size - 2 ; i >= 0; i--) {
		if (freq > g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i + 1, false, caller);
			break;
		}
		if (i == 0) {
			if (!lp_freq || (lp_freq && (freq > lp_freq)))
				mtk_drm_set_mmclk(crtc, 0, false, caller);
			else
				mtk_drm_set_mmclk(crtc, 0, true, caller);
		}
	}
}

unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller)
{
	return g_freq;
}

unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller)
{
	int idx;
	unsigned long freq;

	if (!crtc || !g_freq_steps)
		return 0;

	idx = drm_crtc_index(crtc);

	if (g_freq_level[idx] >= 0)
		freq = g_freq_steps[g_freq_level[idx]];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d]g_freq_level[idx=%d]: %d (freq=%lu)\n",
		__func__, __LINE__, idx, g_freq_level[idx], freq);

	return freq;
}

