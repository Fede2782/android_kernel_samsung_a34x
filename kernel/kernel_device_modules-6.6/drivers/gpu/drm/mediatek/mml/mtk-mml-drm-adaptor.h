/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef __MTK_MML_DRM_ADAPTOR_H__
#define __MTK_MML_DRM_ADAPTOR_H__

#include <linux/platform_device.h>
#include <linux/types.h>

#include "mtk-mml.h"

struct mml_drm_ctx;
struct cmdq_pkt;

/* default lcm pixel, helps calculate HRT bandwidth */
#define MML_DEFAULT_PANEL_W	1440
#define MML_DEFAULT_PANEL_H	3200
#define MML_HRT_FPS		120

enum mml_query_mode_reason {
	mml_query_default,
	mml_query_userdc,
	mml_query_userdc2,
	mml_query_norsz,
	mml_query_sec,
	mml_query_pqen,		/* 5 */
	mml_query_2out,
	mml_query_opp_out,
	mml_query_acttime,
	mml_query_rot,
	mml_query_highpixel,	/* 10 */
	mml_query_lowpixel,
	mml_query_outwidth,
	mml_query_rszratio,
	mml_query_format,
	mml_query_apudc,	/* 15 */
	mml_query_inwidth,
	mml_query_inheight,
	mml_query_flip,
	mml_query_alpha,
	mml_query_min_size,	/* 20 */
	mml_query_dc_off,
	mml_query_tp,
	mml_query_lowpower,
	mml_query_not_support,
};

struct mml_drm_param {
	/* [in]set true if display uses dual pipe */
	bool dual;

	/* [in]set true if display uses vdo mode, false for cmd mode */
	bool vdo_mode;

	/* [in]submit done callback api */
	void (*submit_cb)(void *cb_param);

	/* [in]ddren callback api */
	void (*ddren_cb)(struct cmdq_pkt *pkt, bool enable, void *disp_crtc);

	/* [in]dispen callback api, helps mml driver turn on dispsys clock */
	void (*dispen_cb)(bool enable, void *dispen_param);

	/* [in]display kick idle interface, which helps mml ask display power on */
	void (*kick_idle_cb)(void *disp_crtc);

	/* [in]parameter send back to disp callback */
	void *dispen_param;
	void *disp_crtc;

	/* [out]The height of racing mode for each output tile in pixel. */
	u8 racing_height;
};

/*
 * mml_drm_get_hw_caps - Query mml supported modes. The mode bits refer to enum mml_mode
 *
 * @mode_caps:	Bits to represent enabled mode in current platform.
 * @pq_caps:	Bits to represent pq capability.
 *
 * Return:	Query success or not.
 */
int mml_drm_get_hw_caps(u32 *mode_caps, u32 *pq_caps);

/*
 * mml_drm_query_hw_support - Query frame info meet hardware spec.
 *
 * @info:	Frame info which describe frame process by mml.
 *
 * Return:	True for support, false for not support.
 */
bool mml_drm_query_hw_support(const struct mml_frame_info *info);

/*
 * mml_drm_query_cap - Query current running mode and possible support mode
 * for specific frame info.
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @info:	Frame info which describe frame process by mml.
 *
 * Return:	Capability result of target mode by giving info.
 */
enum mml_mode mml_drm_query_cap(struct mml_drm_ctx *dctx,
				struct mml_frame_info *info);

/*
 * mml_drm_query_multi_layer - Query multiple mml layer support mode in single query.
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @infos:	Frame info array which describe frame process by mml. The result mode will
 *		fill back into infos[i].mode variable.
 * @cnt:	Size of array.
 * @duration_ns:	Time limit to all frame.
 *
 * Return:	True if query success. False if any error happen.
 *
 * Note: The mml frme stand for display ovl layer. Display use N layers compose to single
 * display frame to screen.
 */
int mml_drm_query_multi_layer(struct mml_drm_ctx *dctx,
	struct mml_frame_info *infos, u32 cnt, u32 duration_us);

/*
 * mml_drm_try_frame - Try/adjust frame info to match mml hardware spec.
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @info:	Frame info which describe frame process by mml.
 */
void mml_drm_try_frame(struct mml_drm_ctx *dctx, struct mml_frame_info *info);

/*
 * mml_drm_get_context - Get mml drm context to control mml.
 *
 * pdev:	The mml driver platform device pointer. Client driver must call
 *		mml_get_plat_device by giving user client driver platoform
 *		device which contains "mediatek,mml" property link to mml node
 *		in dts.
 * @disp:	Display parameters. See struct.
 *
 * Return:	The drm context pointer to represent mml driver instance.
 *
 */
struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev,
	struct mml_drm_param *disp);

/*
 * mml_drm_ctx_idle - Check if all tasks in this drm ctx stop.
 *
 * @dctx:	The drm context instance.
 */
bool mml_drm_ctx_idle(struct mml_drm_ctx *dctx);

/*
 * mml_drm_put_context - Release mml drm context and related cached info
 * inside this context.
 *
 * @dctx:	The drm context instance.
 */
void mml_drm_put_context(struct mml_drm_ctx *dctx);

/*
 * mml_drm_kick_done - Check all cmdq client hold by mml, to complete tasks.
 *
 * @dctx:	The drm context instance.
 */
void mml_drm_kick_done(struct mml_drm_ctx *dctx);

/*
 * mml_drm_set_panel_pixel - Set pixel count of display panel (lcm) pixel count
 * This value helps mml calculate HRT bandwidth. See frame_calc_layer_hrt for
 * more detail.
 *
 * Note this API also update currecnt existing frame config HRT base on new
 * panel pixel count.
 *
 * @dctx:	The drm context instance.
 * @panel_width:	Pixel count width of panel. Default value is 1440 (wqhd).
 * @panel_height:	Pixel count height of panel. Default value is 3200 (wqhd).
 */
void mml_drm_set_panel_pixel(struct mml_drm_ctx *dctx, u32 panel_width, u32 panel_height);

/*
 * mml_drm_racing_config_sync - append event sync instructions to disp pkt
 *
 * @dctx:	The drm context instance.
 * @pkt:	The pkt to append cmdq instructions, which helps this pkt
 *		and mml pkt execute at same time.
 *
 * return:	0 if success and < 0 error no if fail
 */
s32 mml_drm_racing_config_sync(struct mml_drm_ctx *dctx, struct cmdq_pkt *pkt);

/*
 * mml_drm_racing_stop_sync - append event sync instructions to disp pkt
 *
 * @dctx:	The drm context instance.
 * @pkt:	The pkt to append cmdq instructions, which helps this pkt
 *		and mml pkt execute at same time.
 *
 * return:	0 if success and < 0 error no if fail
 */
s32 mml_drm_racing_stop_sync(struct mml_drm_ctx *dctx, struct cmdq_pkt *pkt);

/*
 * mml_drm_split_info - split submit info to racing info and pq info
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	[in/out]Frame info which want mml driver to execute. The info
 *		data inside this submit will change for racing mode
 * @submit_pq:	[out]Frame info for pq engines separate from submit
 */
void mml_drm_split_info(struct mml_submit *submit, struct mml_submit *submit_pq);

/*
 * mml_drm_submit - submit mml job
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @cb_param:	The parameter used in submit done callback (if registered).
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
s32 mml_drm_submit(struct mml_drm_ctx *dctx, struct mml_submit *submit,
	void *cb_param);

/*
 * mml_drm_stop - stop mml task (for racing mode)
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @force:	true to use cmdq stop gce hardware thread, false to set next_spr
 *		to next only.
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
s32 mml_drm_stop(struct mml_drm_ctx *dctx, struct mml_submit *submit, bool force);

/*
 * mml_drm_config_rdone - append instruction to config mmlsys rdone sel to
 *		default, which makes mmlsys always active rdone. This avoid
 *		mml task hang if disp stop.
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @pkt:	The pkt to append cmdq instructions.
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
void mml_drm_config_rdone(struct mml_drm_ctx *dctx, struct mml_submit *submit,
	struct cmdq_pkt *pkt);

/*
 * mml_drm_dump - dump cmdq thread status for mml
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 */
void mml_drm_dump(struct mml_drm_ctx *dctx, struct mml_submit *submit);

/*
 * mml_drm_query_dl_path - query direct link path
 *
 * @dctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @pipe:	path pipe
 */
const struct mml_topology_path *mml_drm_query_dl_path(struct mml_drm_ctx *dctx,
	struct mml_submit *submit, u32 pipe);

/*
 * mml_drm_submit_timeout - trigger aee for mml
 *
 */
void mml_drm_submit_timeout(void);

#endif	/* __MTK_MML_DRM_ADAPTOR_H__ */
