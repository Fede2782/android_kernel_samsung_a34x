// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/atomic.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/sync_file.h>
#include <linux/time64.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <mtk_sync.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-adaptor.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-pq-core.h"

#define MML_MAX_DUR	3300000
#define MML_MAX_W	4096
#define MML_MAX_H	2176
#define MML_MAX_PIXEL	8355840		/* 3840*2176 */
#define mml_sz_out(w, h)	\
	(w > h ? (w > MML_MAX_W || h > MML_MAX_H) : ((h > MML_MAX_W || w > MML_MAX_H)))

int drm_max_cache_task = 4;
module_param(drm_max_cache_task, int, 0644);

int drm_max_cache_cfg = 2;
module_param(drm_max_cache_cfg, int, 0644);

/* dc mode enable control
 * 0: disable
 * bit 0: enable dc mode primary path
 * bit 1: enable dc mode 2
 */
int mml_dc = 0x3;
module_param(mml_dc, int, 0644);

int mml_hrt_overhead = 100;
module_param(mml_hrt_overhead, int, 0644);

int mml_max_layers = MML_MAX_LAYER;
module_param(mml_max_layers, int, 0644);

struct mml_drm_ctx {
	struct mml_ctx ctx;
	struct sync_timeline *timeline;
	u16 panel_width;
	u16 panel_height;
	bool racing_begin;
	void (*ddren_cb)(struct cmdq_pkt *pkt, bool enable, void *disp_crtc);
	void *disp_crtc;
	void (*dispen_cb)(bool enable, void *dispen_param);
	void *dispen_param;
	struct completion idle;
};

static struct mml_drm_ctx *task_ctx_to_drm(struct mml_task *task)
{
	return container_of(task->ctx, struct mml_drm_ctx, ctx);
}

static u32 afbc_drm_to_mml(u32 drm_format)
{
	switch (drm_format) {
	case MML_FMT_RGBA8888:
		return MML_FMT_RGBA8888_AFBC;
	case MML_FMT_RGBA1010102:
		return MML_FMT_RGBA1010102_AFBC;
	case MML_FMT_NV12:
		return MML_FMT_YUV420_AFBC;
	case MML_FMT_NV12_10L:
		return MML_FMT_YUV420_10P_AFBC;
	default:
		mml_err("[drm]%s unknown drm format %#x", __func__, drm_format);
		return drm_format;
	}
}

#define MML_AFBC	DRM_FORMAT_MOD_ARM_AFBC( \
	AFBC_FORMAT_MOD_BLOCK_SIZE_32x8 | AFBC_FORMAT_MOD_SPLIT)

static u32 format_drm_to_mml(u32 drm_format, u64 modifier)
{
	/* check afbc modifier with rdma/wrot supported
	 * 32x8 block and split mode
	 */
	if (modifier == MML_AFBC && !MML_FMT_COMPRESS(drm_format))
		return afbc_drm_to_mml(drm_format);

	return drm_format;
}

int mml_drm_get_hw_caps(u32 *mode_caps, u32 *pq_caps)
{
	*mode_caps = mml_topology_get_mode_caps();
	*pq_caps = mml_topology_get_hw_caps();
	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_get_hw_caps);

bool mml_drm_query_hw_support(const struct mml_frame_info *info)
{
	static u32 hw_caps;
	u32 i;

	hw_caps = mml_topology_get_hw_caps();
	if (!hw_caps) {
		mml_err("[drm]mml platform not ready");
		goto not_support;
	}

	if (!info->src.format) {
		mml_err("[drm]invalid src mml color format %#010x", info->src.format);
		goto not_support;
	}

	if (info->src.modifier && info->src.modifier != MML_AFBC) {
		mml_err("[drm]invalid src mml color format modifier %#010llx", info->src.modifier);
		goto not_support;
	}

	if (info->src.modifier &&
		!MML_FMT_AFBC(format_drm_to_mml(info->src.format, info->src.modifier))) {
		mml_err("[drm]invalid src afbc fmt modifier: %#010llx, format %#x",
			info->src.modifier, info->src.format);
		goto not_support;
	}

	if (MML_FMT_BLOCK(info->src.format)) {
		if ((info->src.width & 0x0f) || (info->src.height & 0x1f)) {
			mml_err(
				"[drm]invalid blk width %u height %u must alignment width 16x height 32x",
				info->src.width, info->src.height);
			goto not_support;
		}
	}

	if (mml_sz_out(info->src.width, info->src.height)) {
		mml_msg("[drm]not support src size %u %u", info->src.width, info->src.height);
		goto not_support;
	}

	if (info->dest_cnt > MML_MAX_OUTPUTS) {
		mml_msg("[drm]dest count exceed %u", info->dest_cnt);
		goto not_support;
	}

	for (i = 0; i < info->dest_cnt; i++) {
		const struct mml_frame_dest *dest = &info->dest[i];
		u32 destw = dest->data.width;
		u32 desth = dest->data.height;
		u32 crop_srcw = dest->crop.r.width ? dest->crop.r.width : info->src.width;
		u32 crop_srch = dest->crop.r.height ? dest->crop.r.height : info->src.height;

		if (!destw || !desth || !crop_srcw || !crop_srch) {
			mml_msg("[drm]not support empty size %u %u %u %u",
				crop_srcw, crop_srch, destw, desth);
			goto not_support;
		}

		if (mml_sz_out(destw, desth)) {
			mml_msg("[drm]not support dest size %u %u", destw, desth);
			goto not_support;
		}

		/* color space not support for destination */
		if (dest->data.profile == MML_YCBCR_PROFILE_BT2020 ||
			dest->data.profile == MML_YCBCR_PROFILE_FULL_BT709 ||
			dest->data.profile == MML_YCBCR_PROFILE_FULL_BT2020) {
			mml_err("[drm]not support output profile %d", dest->data.profile);
			goto not_support;
		}

		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
			swap(destw, desth);

		if (crop_srcw > destw * 20 || crop_srch > desth * 24 ||
			destw > crop_srcw * 32 || desth > crop_srch * 32) {
			mml_err("[drm]exceed HW limitation src %ux%u dest %ux%u",
				crop_srcw, crop_srch, destw, desth);
			goto not_support;
		}

		if (crop_srcw * desth > destw * crop_srch * 16 ||
			destw * crop_srch > crop_srcw * desth * 16) {
			mml_err("[drm]exceed tile ratio limitation src %ux%u dest %ux%u",
				crop_srcw, crop_srch, destw, desth);
			goto not_support;
		}

		/* check crop and pq combination */
		if (dest->pq_config.en && dest->crop.r.width < 48) {
			mml_err("[drm]exceed HW limitation crop width %u < 48 with pq",
				dest->crop.r.width);
			goto not_support;
		}

		if ((dest->compose.width || dest->compose.height) &&
			(dest->compose.width != dest->data.width ||
			dest->compose.height != dest->data.height)) {
			/* compress format or rgb format, compose must same as out size */
			if (MML_FMT_COMPRESS(dest->data.format) ||
				!MML_FMT_IS_YUV(dest->data.format))
				goto not_support;

			/* set compose, use h/v subsample (420/422), out size must even */
			if (MML_FMT_H_SUBSAMPLE(dest->data.format) && dest->data.width & 0x1)
				goto not_support;
			if (MML_FMT_V_SUBSAMPLE(dest->data.format) && dest->data.height & 0x1)
				goto not_support;
		}
	}

	if (hw_caps & MML_HW_ALPHARSZ) {
		/* hardware support alpha resize case */
		if (info->alpha && !MML_FMT_ALPHA(info->src.format)) {
			mml_err("[drm]alpha enable without alpha input format %#010x",
				info->src.format);
			goto not_support;
		}
	} else if (info->alpha &&
		MML_FMT_ALPHA(info->src.format) &&
		MML_FMT_ALPHA(info->dest[0].data.format)) {
		/* for alpha rotate */
		const struct mml_frame_dest *dest = &info->dest[0];
		u32 srccw = dest->crop.r.width;
		u32 srcch = dest->crop.r.height;
		u32 destw = dest->data.width;
		u32 desth = dest->data.height;

		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
			swap(destw, desth);

		if (srccw < 9) {
			mml_err("[drm]exceed HW limitation src width %u < 9", srccw);
			goto not_support;
		}
		if (srccw != destw || srcch != desth) {
			mml_err(
				"[drm]unsupport alpha rotation for resize case crop %u,%u to dest %u,%u",
				srccw, srcch, destw, desth);
			goto not_support;
		}
	}

	return true;

not_support:
	return false;
}
EXPORT_SYMBOL_GPL(mml_drm_query_hw_support);

enum mml_mode mml_drm_query_frame(struct mml_drm_ctx *dctx, struct mml_frame_info *info,
	struct mml_frame_info_cache *info_cache)
{
	u8 i;
	struct mml_topology_cache *tp = mml_topology_get_cache(dctx->ctx.mml);
	enum mml_mode mode;
	u32 reason = 0;

	if (mml_pq_disable) {
		for (i = 0; i < MML_MAX_OUTPUTS; i++) {
			memset(&info->dest[i].pq_config, 0,
				sizeof(info->dest[i].pq_config));
		}
	}

	if (info->dest_cnt > MML_MAX_OUTPUTS)
		info->dest_cnt = MML_MAX_OUTPUTS;

	info->src.format = format_drm_to_mml(info->src.format, info->src.modifier);

	if (!mml_drm_query_hw_support(info)) {
		reason = mml_query_not_support;
		goto not_support;
	}

	if (!tp || (!tp->op->query_mode && !tp->op->query_mode2)) {
		reason = mml_query_tp;
		goto not_support;
	}

	if (tp->op->query_mode2)
		mode = tp->op->query_mode2(dctx->ctx.mml, info, &reason,
			dctx->panel_width, dctx->panel_height, info_cache);
	else
		mode = tp->op->query_mode(dctx->ctx.mml, info, &reason);
	if (mode == MML_MODE_MML_DECOUPLE && mml_dev_get_couple_cnt(dctx->ctx.mml)) {
		/* if mml hw running racing mode and query info need dc,
		 * go back to MDP decouple to avoid hw conflict.
		 *
		 * Note: no mutex lock here cause disp call query/submit on
		 * same kernel thread, thus couple count can only decrease and
		 * not increase after read. And it's safe to do one more mdp
		 * decouple w/o mml couple/dc conflict.
		 */
		mml_log("[drm]%s mode %u to mdp dc or mml dc2 couple %d",
			__func__, mode, mml_dev_get_couple_cnt(dctx->ctx.mml));
		if (tp->op->support_dc2 && tp->op->support_dc2())
			mode = MML_MODE_MML_DECOUPLE2;
		else
			mode = MML_MODE_MDP_DECOUPLE;
	}

	if (mode == MML_MODE_MML_DECOUPLE && !(mml_dc & 0x1)) {
		mode = tp->op->support_dc2() ?
			MML_MODE_MML_DECOUPLE2 : MML_MODE_MDP_DECOUPLE;
		reason = mml_query_dc_off;
	}

	if (mode == MML_MODE_MML_DECOUPLE2 && !(mml_dc & 0x2)) {
		mode = MML_MODE_NOT_SUPPORT;
		reason = mml_query_dc_off;
	}

	mml_mmp2(query_mode, MMPROFILE_FLAG_PULSE, info->mode, mode, 0, reason);
	mml_msg("[drm]query mode %u result mode %u reason %d", info->mode, mode, (s32)reason);
	return mode;

not_support:
	mml_mmp2(query_mode, MMPROFILE_FLAG_PULSE,
		info->mode, (MML_MODE_NOT_SUPPORT & 0xffff), 0, reason);
	mml_msg("[drm]query mode not support reason %d", (s32)reason);
	return MML_MODE_NOT_SUPPORT;
}

enum mml_mode mml_drm_query_cap(struct mml_drm_ctx *dctx,
				struct mml_frame_info *info)
{
	return mml_drm_query_frame(dctx, info, NULL);
}
EXPORT_SYMBOL_GPL(mml_drm_query_cap);

/* dc mode reserve time in us */
int dc_layer_reserve = 1500;
module_param(dc_layer_reserve, int, 0644);

int mml_drm_query_multi_layer(struct mml_drm_ctx *dctx,
	struct mml_frame_info *infos, u32 cnt, u32 duration_us)
{
	/* TODO: put into drm context */
	struct mml_frame_info_cache info_cache[MML_MAX_LAYER] = {0};

	enum mml_mode mode = MML_MODE_UNKNOWN;
	u32 remain[mml_max_sys] = {0};
	u32 mml_layer_cnt = 0;
	u32 i;
	s32 max_layer = min(mml_max_layers, MML_MAX_LAYER);
	bool couple_used = false;

	if (!duration_us)
		duration_us = MML_MAX_DUR;
	mml_msg("[drm][query]%s duration %u", __func__, duration_us);

	remain[mml_sys_frame] = duration_us -  dc_layer_reserve;
	remain[mml_sys_tile] = duration_us -  dc_layer_reserve;

	for (i = 0; i < cnt; i++) {
		bool balance = false;

		if (mml_layer_cnt >= max_layer) {
			infos[i].mode = MML_MODE_NOT_SUPPORT;
			continue;
		}
		if (couple_used)
			infos[i].mode = MML_MODE_MML_DECOUPLE2;
		else if (i > 0 && infos[i].mode == 0)
			infos[i].mode = MML_MODE_MML_DECOUPLE;
		/* use mml-frame remain time to compare dl/dc opp */
		info_cache[mml_layer_cnt].remain = remain[mml_sys_frame];
		mode = mml_drm_query_frame(dctx, &infos[i], &info_cache[mml_layer_cnt]);
		if (mode == MML_MODE_DIRECT_LINK)
			mml_msg("[drm][query]layer %u mode dl active time %u",
				i, infos[i].act_time);
		else if (mode == MML_MODE_MML_DECOUPLE) {
			if (remain[mml_sys_frame] < info_cache[mml_layer_cnt].duration) {
				mml_msg("[drm][query]layer %u dc not support remain %u need %u",
					i, remain[mml_sys_frame],
					info_cache[mml_layer_cnt].duration);
				mode = MML_MODE_MML_DECOUPLE2;
			} else if (remain[mml_sys_frame] < remain[mml_sys_tile]) {
				balance = true;
				mode = MML_MODE_MML_DECOUPLE2;
			} else {
				remain[mml_sys_frame] -= info_cache[mml_layer_cnt].duration;
				mml_msg("[drm][query]layer %u mode dc  remain %u",
					i, remain[mml_sys_frame]);
			}
		}

		if (mode == MML_MODE_MML_DECOUPLE2) {
			if (remain[mml_sys_tile] < info_cache[mml_layer_cnt].duration) {
				mode = MML_MODE_NOT_SUPPORT;
				mml_msg("[drm][query]layer %u dc2 not support remain %u need %u",
					i, remain[mml_sys_tile],
					info_cache[mml_layer_cnt].duration);
			} else {
				remain[mml_sys_tile] -= info_cache[mml_layer_cnt].duration;
				mml_msg("[drm][query]layer %u mode dc2 remain %u%s",
					i, remain[mml_sys_tile], balance ? " (balanced)" : "");
			}
		}

		infos[i].mode = mode;

		if (mode == MML_MODE_DIRECT_LINK || mode == MML_MODE_RACING)
			couple_used = true;

		mml_mmp(query_layer, MMPROFILE_FLAG_PULSE,
			i << 16 | (atomic_read(&dctx->ctx.job_serial) & 0xffff), mode);
		mml_layer_cnt++;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_query_multi_layer);

static void mml_adjust_src(struct mml_frame_data *src, struct mml_frame_dest *dest)
{
	const u32 srcw = src->width;
	const u32 srch = src->height;

	if (MML_FMT_H_SUBSAMPLE(src->format) && (srcw & 0x1)) {
		if (src->width == dest->crop.r.width)
			src->width += 1;
		else
			src->width &= ~1;
	}

	if (MML_FMT_V_SUBSAMPLE(src->format) && (srch & 0x1)) {
		if (src->height == dest->crop.r.height)
			src->height += 1;
		else
			src->height &= ~1;
	}
}

static void mml_adjust_dest(struct mml_frame_data *src, struct mml_frame_dest *dest)
{
	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		if (MML_FMT_H_SUBSAMPLE(dest->data.format)) {
			dest->data.width &= ~1; /* WROT HW constraint */
			dest->data.height &= ~1;
		} else if (MML_FMT_V_SUBSAMPLE(dest->data.format)) {
			dest->data.width &= ~1;
		}
	} else {
		if (MML_FMT_H_SUBSAMPLE(dest->data.format))
			dest->data.width &= ~1;

		if (MML_FMT_V_SUBSAMPLE(dest->data.format))
			dest->data.height &= ~1;
	}

	/* help user fill in crop if not give */
	if (!dest->crop.r.width && !dest->crop.r.height) {
		dest->crop.r.width = src->width;
		dest->crop.r.height = src->height;
	}

	if (!dest->compose.width && !dest->compose.height) {
		dest->compose.width = dest->data.width;
		dest->compose.height = dest->data.height;
	}
}

void mml_drm_try_frame(struct mml_drm_ctx *ctx, struct mml_frame_info *info)
{
	u32 i;

	mml_adjust_src(&info->src, &info->dest[0]);

	if (info->mode == MML_MODE_DIRECT_LINK) {
		struct mml_frame_dest *dest = &info->dest[0];

		if (dest->data.width != dest->compose.width && dest->compose.width)
			dest->data.width = dest->compose.width;
		if (dest->data.height != dest->compose.height && dest->compose.height)
			dest->data.height = dest->compose.height;
		i = 1;
	} else {
		i = 0;
	}

	for (; i < info->dest_cnt; i++) {
		/* adjust info data directly for user */
		mml_adjust_dest(&info->src, &info->dest[i]);
	}

	if ((MML_FMT_PLANE(info->src.format) > 1) && info->src.uv_stride <= 0)
		info->src.uv_stride = mml_color_get_min_uv_stride(
			info->src.format, info->src.width);
}
EXPORT_SYMBOL_GPL(mml_drm_try_frame);

static s32 drm_frame_buf_to_task_buf(struct mml_ctx *ctx,
	struct mml_file_buf *fbuf, struct mml_buffer *user_buf,
	bool secure, const char *name)
{
	s32 ret;
	struct device *mmu_dev = mml_get_mmu_dev(ctx->mml, secure);

	if (unlikely(!mmu_dev)) {
		mml_err("[drm]%s mmu_dev is null", __func__);
		return -EFAULT;
	}

	ret = frame_buf_to_task_buf(fbuf, user_buf, name);
	if (ret)
		return ret;

	if (user_buf->fence >= 0) {
		fbuf->fence = sync_file_get_fence(user_buf->fence);
		mml_msg("[drm]get dma fence %p by %d", fbuf->fence, user_buf->fence);
	}

	if (fbuf->dma[0].dmabuf) {
		mml_mmp(buf_map, MMPROFILE_FLAG_START,
			atomic_read(&ctx->job_serial), 0);

		/* get iova */
		ret = mml_buf_iova_get(mmu_dev, fbuf);
		if (ret < 0)
			mml_err("[drm]%s iova fail %d", __func__, ret);

		mml_mmp(buf_map, MMPROFILE_FLAG_END,
			atomic_read(&ctx->job_serial),
			(unsigned long)fbuf->dma[0].iova);

		mml_msg("[drm]%s %s dmabuf %p iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
			__func__, name, fbuf->dma[0].dmabuf,
			fbuf->dma[0].iova, fbuf->size[0],
			fbuf->dma[1].iova, fbuf->size[1],
			fbuf->dma[2].iova, fbuf->size[2]);
	}

	return ret;
}

static void drm_task_move_to_idle(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_drm_ctx *dctx = task_ctx_to_drm(task);

	/* Must lock ctx->config_mutex before call */
	task_move_to_idle(task);

	/* maintain racing ref count decrease after done */
	mml_dev_couple_dec(dctx->ctx.mml, cfg->info.mode);

	mml_msg("[drm]%s task cnt (%u %u %hhu) couple %d",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		mml_dev_get_couple_cnt(dctx->ctx.mml));
}

static bool mml_drm_check_configs_idle_locked(struct mml_drm_ctx *dctx, bool warn)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	bool idle = false;

	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (!list_empty(&cfg->await_tasks)) {
			if (warn)
				mml_log("[drm]%s await_tasks not empty", __func__);
			goto done;
		}

		if (!list_empty(&cfg->tasks)) {
			if (warn)
				mml_log("[drm]%s tasks not empty", __func__);
			goto done;
		}
	}

	idle = true;
done:
	return idle;
}

static void drm_task_frame_done(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_config *tmp;
	struct mml_ctx *ctx = task->ctx;
	struct mml_drm_ctx *dctx = task_ctx_to_drm(task);
	struct mml_dev *mml = cfg->mml;

	mml_trace_ex_begin("%s", __func__);

	mml_msg("[drm]frame done task %p state %u job %u",
		task, task->state, task->job.jobid);

	/* clean up */
	task_buf_put(task);

	mutex_lock(&ctx->config_mutex);

	if (unlikely(!task->pkts[0] || (cfg->dual && !task->pkts[1]))) {
		task_state_dec(cfg, task, __func__);
		mml_err("[drm]%s task cnt (%u %u %hhu) error from state %d",
			__func__,
			cfg->await_task_cnt,
			cfg->run_task_cnt,
			cfg->done_task_cnt,
			task->state);
		task->err = true;
		mml_record_track(mml, task);
		kref_put(&task->ref, task_move_to_destroy);
	} else {
		/* works fine, safe to move */
		drm_task_move_to_idle(task);
		mml_record_track(mml, task);
	}

	if (cfg->done_task_cnt > drm_max_cache_task) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task), entry);
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		mml_msg("[drm]%s task cnt (%u %u %hhu)",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);
		kref_put(&task->ref, task_move_to_destroy);
	}

	/* still have room to cache, done */
	if (ctx->config_cnt <= drm_max_cache_cfg)
		goto done;

	/* must pick cfg from list which is not running */
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* only remove config not running */
		if (!list_empty(&cfg->tasks) || !list_empty(&cfg->await_tasks))
			continue;
		list_del_init(&cfg->entry);
		frame_config_queue_destroy(cfg);
		ctx->config_cnt--;
		mml_msg("[drm]config %p send destroy remain %u",
			cfg, ctx->config_cnt);

		/* check cache num again */
		if (ctx->config_cnt <= drm_max_cache_cfg)
			break;
	}

done:
	if (!mml_drm_check_configs_idle_locked(dctx, false))
		complete(&dctx->idle);
	mutex_unlock(&ctx->config_mutex);

	mml_lock_wake_lock(mml, false);

	mml_trace_ex_end();
}

s32 mml_drm_submit(struct mml_drm_ctx *dctx, struct mml_submit *submit,
		   void *cb_param)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;
	s32 result = -EINVAL;
	u32 i;
	struct fence_data fence = {0};

	mml_trace_begin("%s", __func__);

	mml_mmp_raw(submit, MMPROFILE_FLAG_PULSE,
		atomic_read(&ctx->job_serial) + 1, submit->info.mode,
		&submit->info, sizeof(submit->info));

	if (mtk_mml_msg || mml_pq_disable) {
		for (i = 0; i < MML_MAX_OUTPUTS; i++) {
			dump_pq_en(i, submit->pq_param[i],
				&submit->info.dest[i].pq_config);

			if (mml_pq_disable) {
				submit->pq_param[i] = NULL;
				memset(&submit->info.dest[i].pq_config, 0,
					sizeof(submit->info.dest[i].pq_config));
			}
		}
	}

	/* TODO: remove after issue fix */
	if (submit->info.ovlsys_id != MML_DLO_OVLSYS0)
		mml_err("[drm]%s submit ovlsys id %u", __func__, submit->info.ovlsys_id);

	if (submit->info.mode == MML_MODE_DIRECT_LINK && !submit->info.act_time) {
		static bool logonce;

		if (!logonce) {
			mml_err("[drm]empty active time will result wrong bandwidth");
			logonce = true;
		}
		submit->info.act_time = 1000;
	}

	/* always fixup dest_cnt > MML_MAX_OUTPUTS */
	if (submit->info.dest_cnt > MML_MAX_OUTPUTS)
		submit->info.dest_cnt = MML_MAX_OUTPUTS;
	if (submit->buffer.dest_cnt > MML_MAX_OUTPUTS)
		submit->buffer.dest_cnt = MML_MAX_OUTPUTS;

	/* always fixup format/modifier for afbc case
	 * the format in info should change to fourcc format in future design
	 * and store mml format in another structure
	 */
	submit->info.src.format = format_drm_to_mml(
		submit->info.src.format, submit->info.src.modifier);

	/* always fixup plane offset */
	if (likely(submit->info.mode != MML_MODE_SRAM_READ)) {
		frame_calc_plane_offset(&submit->info.src, &submit->buffer.src);
		frame_calc_plane_offset(&submit->info.seg_map, &submit->buffer.seg_map);
		for (i = 0; i < submit->info.dest_cnt; i++)
			frame_calc_plane_offset(&submit->info.dest[i].data,
				&submit->buffer.dest[i]);
	}

	if (MML_FMT_AFBC_YUV(submit->info.src.format)) {
		submit->info.src.y_stride =
			mml_color_get_min_y_stride(submit->info.src.format, submit->info.src.width);
		submit->info.src.uv_stride = 0;
		submit->info.src.plane_cnt = 1;
		submit->buffer.src.cnt = 1;
		submit->buffer.src.fd[1] = -1;
		submit->buffer.src.fd[2] = -1;
	}

	for (i = 0; i < submit->info.dest_cnt; i++)
		submit->info.dest[i].data.format = format_drm_to_mml(
			submit->info.dest[i].data.format,
			submit->info.dest[i].data.modifier);

	/* give default act time in case something wrong in disp
	 * the 2604 base on cmd mode
	 *	ns / fps / vtotal = line_time, which expend to
	 *	1000000000 / 120 / 3228 = 2581
	 */
	if (submit->info.mode == MML_MODE_RACING && !submit->info.act_time)
		submit->info.act_time = 2581 * submit->info.dest[0].compose.height;

	/* always do frame info adjust for now
	 * but this flow should call from hwc/disp in future version
	 */
	mml_drm_try_frame(dctx, &submit->info);

	/* +1 for real id assign to next task */
	mml_mmp(submit, MMPROFILE_FLAG_START,
		atomic_read(&ctx->job_serial) + 1, submit->info.mode);

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (cfg) {
		mml_msg("[drm]%s reuse config %p", __func__, cfg);
		task = task_get_idle(cfg);
		if (task) {
			/* reuse case change state IDLE to REUSE */
			task->state = MML_TASK_REUSE;
			init_completion(&task->pkts[0]->cmplt);
			if (task->pkts[1])
				init_completion(&task->pkts[1]->cmplt);
			mml_msg("[drm]reuse task %p pkt %p %p",
				task, task->pkts[0], task->pkts[1]);
		} else {
			task = mml_core_create_task(atomic_read(&ctx->job_serial));
			if (IS_ERR(task)) {
				result = PTR_ERR(task);
				mml_err("[drm]%s create task for reuse frame fail", __func__);
				task = NULL;
				goto err_unlock_exit;
			}
			task->config = cfg;
			task->state = MML_TASK_DUPLICATE;
			/* add more count for new task create */
			cfg->cfg_ops->get(cfg);
		}
	} else {
		cfg = frame_config_create(ctx, submit);

		mml_msg("[drm]%s create config %p", __func__, cfg);
		if (IS_ERR(cfg)) {
			result = PTR_ERR(cfg);
			mml_err("[drm]%s create frame config fail", __func__);
			goto err_unlock_exit;
		}
		task = mml_core_create_task(atomic_read(&ctx->job_serial));
		if (IS_ERR(task)) {
			list_del_init(&cfg->entry);
			frame_config_destroy(cfg);
			result = PTR_ERR(task);
			task = NULL;
			mml_err("[drm]%s create task fail", __func__);
			goto err_unlock_exit;
		}
		task->config = cfg;
		if (submit->info.mode == MML_MODE_RACING ||
			submit->info.mode == MML_MODE_DIRECT_LINK) {
			cfg->layer_w = submit->layer.width;
			if (unlikely(!cfg->layer_w))
				cfg->layer_w = submit->info.dest[0].compose.width;
			cfg->layer_h = submit->layer.height;
			if (unlikely(!cfg->layer_h))
				cfg->layer_h = submit->info.dest[0].compose.height;
			cfg->panel_w = dctx->panel_width;
			cfg->panel_h = dctx->panel_height;
		}

		if (submit->info.mode == MML_MODE_DIRECT_LINK) {
			/* TODO: remove it, workaround for direct link,
			 * the dlo roi should fill by disp
			 */
			if (!cfg->dl_out[0].width || !cfg->dl_out[0].height) {
				cfg->dl_out[0].width = submit->info.dest[0].compose.width / 2;
				cfg->dl_out[0].height = submit->info.dest[0].compose.height;
				cfg->dl_out[1].width = submit->info.dest[0].compose.width -
					cfg->dl_out[0].width;
				cfg->dl_out[1].height = submit->info.dest[0].compose.height;

				mml_log("[wan][drm]auto fill in dl out by compose");
			}
		}

		/* add more count for new task create */
		cfg->cfg_ops->get(cfg);
	}

	/* maintain racing ref count for easy query mode */
	if (cfg->info.mode == MML_MODE_RACING) {
		/* also mark begin so that disp clear target line event */
		if (mml_dev_couple_inc(ctx->mml, cfg->info.mode) == 1)
			dctx->racing_begin = true;
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK)
		mml_dev_couple_inc(ctx->mml, cfg->info.mode);

	/* make sure id unique and cached last */
	task->job.jobid = atomic_inc_return(&ctx->job_serial);
	task->adaptor_type = MML_ADAPTOR_DRM;
	task->cb_param = cb_param;
	cfg->last_jobid = task->job.jobid;
	list_add_tail(&task->entry, &cfg->await_tasks);
	cfg->await_task_cnt++;
	mml_msg("[drm]%s task cnt (%u %u %hhu) couple %d",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		mml_dev_get_couple_cnt(ctx->mml));

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;
	if (mml_isdc(cfg->info.mode)) {
		task->end_time.tv_sec = submit->end.sec;
		task->end_time.tv_nsec = submit->end.nsec;
		/* give default time if empty */
		frame_check_end_time(&task->end_time);
		mml_msg("[drm]mml job %u end %2u.%03llu",
			task->job.jobid,
			(u32)task->end_time.tv_sec, div_u64(task->end_time.tv_nsec, 1000000));
	}

	if (cfg->info.mode == MML_MODE_APUDC) {
		task->buf.src.apu_handle = mml_get_apu_handle(&submit->buffer.src);
	} else {
		result = drm_frame_buf_to_task_buf(ctx, &task->buf.src,
			&submit->buffer.src, submit->info.src.secure,
			"mml_drm_rdma");
		if (result) {
			mml_err("[drm]%s get src dma buf fail", __func__);
			goto err_buf_exit;
		}
	}

	if (submit->info.dest[0].pq_config.en_region_pq) {
		result = drm_frame_buf_to_task_buf(ctx, &task->buf.seg_map,
			&submit->buffer.seg_map, submit->info.seg_map.secure,
			"mml_drm_rdma_seg");
		if (result) {
			mml_err("[drm]%s get seg map dma buf fail", __func__);
			goto err_buf_exit;
		}
	}

	task->buf.dest_cnt = submit->buffer.dest_cnt;
	for (i = 0; i < submit->buffer.dest_cnt; i++) {
		result = drm_frame_buf_to_task_buf(ctx, &task->buf.dest[i],
			&submit->buffer.dest[i], submit->info.dest[i].data.secure,
			"mml_drm_wrot");
		if (result) {
			mml_err("[drm]%s get dest %u dma buf fail", __func__, i);
			goto err_buf_exit;
		}
	}

	/* create fence for this task */
	fence.value = task->job.jobid;
#ifndef MML_FPGA
	if (submit->job && dctx->timeline &&
		submit->info.mode != MML_MODE_RACING &&
		submit->info.mode != MML_MODE_DIRECT_LINK &&
		mtk_sync_fence_create(dctx->timeline, &fence) >= 0) {
		task->job.fence = fence.fence;
		task->fence = sync_file_get_fence(task->job.fence);
	} else {
		task->job.fence = -1;
	}
#endif
	mml_msg("[drm]mml job %u fence fd %d task %p fence %p config %p mode %hhu%s act_t %u",
		task->job.jobid, task->job.fence, task, task->fence, cfg,
		cfg->info.mode,
		(cfg->info.mode == MML_MODE_RACING && cfg->disp_dual) ? " disp dual" : "",
		submit->info.act_time);

	/* copy job content back, must do before call submit */
	if (submit->job)
		*submit->job = task->job;

	/* copy pq parameters */
	for (i = 0; i < submit->buffer.dest_cnt && submit->pq_param[i]; i++)
		task->pq_param[i] = *submit->pq_param[i];

	/* wake lock */
	mml_lock_wake_lock(task->config->mml, true);

	/* submit to core */
	mml_core_submit_task(cfg, task);

	mml_mmp(submit, MMPROFILE_FLAG_END, atomic_read(&ctx->job_serial), 0);
	mml_trace_end();
	return 0;

err_unlock_exit:
	mutex_unlock(&ctx->config_mutex);
err_buf_exit:
	mml_mmp(submit, MMPROFILE_FLAG_END, atomic_read(&ctx->job_serial), 0);
	mml_trace_end();
	mml_log("[drm]%s fail result %d task %p", __func__, result, task);
	if (task) {
		bool is_init_state = task->state == MML_TASK_INITIAL;

		mutex_lock(&ctx->config_mutex);

		list_del_init(&task->entry);
		cfg->await_task_cnt--;

		if (is_init_state) {
			mml_log("[drm]dec config %p and del", cfg);

			list_del_init(&cfg->entry);
			ctx->config_cnt--;

			/* revert racing ref count decrease after done */
			mml_dev_couple_dec(ctx->mml, cfg->info.mode);
			dctx->racing_begin = false;
		} else
			mml_log("[drm]dec config %p", cfg);

		mutex_unlock(&ctx->config_mutex);
		kref_put(&task->ref, task_move_to_destroy);

		if (is_init_state)
			cfg->cfg_ops->put(cfg);
	}
	return result;
}
EXPORT_SYMBOL_GPL(mml_drm_submit);

s32 mml_drm_stop(struct mml_drm_ctx *dctx, struct mml_submit *submit, bool force)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (!cfg) {
		mml_err("[drm]The submit info not found for stop");
		goto done;
	}

	mml_log("[drm]stop config %p", cfg);
	mml_core_stop_racing(cfg, force);

done:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_stop);

void mml_drm_config_rdone(struct mml_drm_ctx *dctx, struct mml_submit *submit,
	struct cmdq_pkt *pkt)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (!cfg) {
		mml_err("[drm]The submit info not found for stop");
		goto done;
	}

	cmdq_pkt_write_value_addr(pkt,
		cfg->path[0]->mmlsys->base_pa +
		mml_sys_get_reg_ready_sel(cfg->path[0]->mmlsys),
		0x24, U32_MAX);

done:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
}
EXPORT_SYMBOL_GPL(mml_drm_config_rdone);

void mml_drm_dump(struct mml_drm_ctx *ctx, struct mml_submit *submit)
{
	mml_log("[drm]dump threads for mml, submit job %u",
		submit->job ? submit->job->jobid : 0);
	mml_dump_thread(ctx->ctx.mml);
}
EXPORT_SYMBOL_GPL(mml_drm_dump);

static void drm_task_ddren(struct mml_task *task, struct cmdq_pkt *pkt, bool enable)
{
	struct mml_drm_ctx *ctx = task_ctx_to_drm(task);
	enum mml_mode mode = task->config->info.mode;

	if (!ctx->ddren_cb)
		return;

	/* no need ddren for srt case */
	if (mml_isdc(mode))
		return;

	ctx->ddren_cb(pkt, enable, ctx->disp_crtc);
}

static void drm_task_dispen(struct mml_task *task, bool enable)
{
	struct mml_drm_ctx *ctx = task_ctx_to_drm(task);
	enum mml_mode mode = task->config->info.mode;

	if (!ctx->dispen_cb)
		return;

	/* no need ddren so no dispen */
	if (mml_isdc(mode))
		return;

	ctx->dispen_cb(enable, ctx->dispen_param);
}

static const struct mml_task_ops drm_task_ops = {
	.queue = task_queue,
	.submit_done = task_submit_done,
	.frame_done = drm_task_frame_done,
	.dup_task = task_dup,
	.get_tile_cache = task_get_tile_cache,
	.kt_setsched = ctx_kt_setsched,
	.ddren = drm_task_ddren,
	.dispen = drm_task_dispen,
};

static const struct mml_config_ops drm_config_ops = {
	.get = frame_config_get,
	.put = frame_config_put,
	.free = frame_config_free,
};

static struct mml_drm_ctx *drm_ctx_create(struct mml_dev *mml,
					  struct mml_drm_param *disp)
{
	static const char * const threads[] = {
		"mml_drm_done", "mml_destroy",
		"mml_work0", "mml_work1",
	};
	struct mml_drm_ctx *ctx;
	int ret;

	mml_msg("[drm]%s on dev %p", __func__, mml);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ret = mml_ctx_init(&ctx->ctx, mml, threads);
	if (ret) {
		kfree(ctx);
		return ERR_PTR(ret);
	}

	ctx->ctx.task_ops = &drm_task_ops;
	ctx->ctx.cfg_ops = &drm_config_ops;
	ctx->ctx.disp_dual = disp->dual;
	ctx->ctx.disp_vdo = disp->vdo_mode;
	ctx->ctx.submit_cb = disp->submit_cb;
	ctx->ddren_cb = disp->ddren_cb;
	ctx->disp_crtc = disp->disp_crtc;
	ctx->dispen_cb = disp->dispen_cb;
	ctx->dispen_param = disp->dispen_param;
	ctx->panel_width = MML_DEFAULT_PANEL_W;
	ctx->panel_height = MML_DEFAULT_PANEL_H;

#ifndef MML_FPGA
	ctx->timeline = mtk_sync_timeline_create("mml_timeline");
#endif
	if (!ctx->timeline)
		mml_err("[drm]fail to create timeline");
	else
		mml_msg("[drm]timeline for mml %p", ctx->timeline);

	/* return info to display */
	disp->racing_height = mml_sram_get_racing_height(mml);

	/* install kick idle callback to mml driver */
	mml_pw_set_kick_cb(mml, disp->kick_idle_cb, disp->disp_crtc);

	/* idle complete event to prevent display ignore put context */
	init_completion(&ctx->idle);

	return ctx;
}

struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev,
					struct mml_drm_param *disp)
{
	struct mml_dev *mml = platform_get_drvdata(pdev);

	mml_msg("[drm]%s", __func__);
	if (!mml) {
		mml_err("[drm]%s not init mml", __func__);
		return ERR_PTR(-EPERM);
	}
	return mml_dev_get_drm_ctx(mml, disp, drm_ctx_create);
}
EXPORT_SYMBOL_GPL(mml_drm_get_context);

bool mml_drm_ctx_idle(struct mml_drm_ctx *dctx)
{
	struct mml_ctx *ctx = &dctx->ctx;
	bool idle = true;

	mutex_lock(&ctx->config_mutex);
	if (!mml_drm_check_configs_idle_locked(dctx, true)) {
		idle = false;
		init_completion(&dctx->idle);
	}
	mutex_unlock(&ctx->config_mutex);

	if (!idle) {
		if (!wait_for_completion_timeout(&dctx->idle, jiffies_to_msecs(5000))) {
			mml_err("[drm]wait idle timed out");
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL_GPL(mml_drm_ctx_idle);

static void drm_ctx_release(struct mml_drm_ctx *dctx)
{
	struct mml_ctx *ctx = &dctx->ctx;
	u32 i;

	mml_msg("[drm]%s on ctx %p", __func__, ctx);

	mml_ctx_deinit(ctx);
	for (i = 0; i < ARRAY_SIZE(ctx->tile_cache); i++)
		if (ctx->tile_cache[i].tiles)
			vfree(ctx->tile_cache[i].tiles);

#ifndef MML_FPGA
	mtk_sync_timeline_destroy(dctx->timeline);
#endif
	kfree(dctx);
}

void mml_drm_put_context(struct mml_drm_ctx *ctx)
{
	if (IS_ERR_OR_NULL(ctx))
		return;
	mml_log("[drm]%s instance %p", __func__, ctx);
	mml_sys_put_dle_ctx(ctx->ctx.mml);
	mml_dev_put_drm_ctx(ctx->ctx.mml, drm_ctx_release);
}
EXPORT_SYMBOL_GPL(mml_drm_put_context);

void mml_drm_kick_done(struct mml_drm_ctx *dctx)
{
	struct mml_ctx *ctx = &dctx->ctx;
	u32 jobid = atomic_read(&ctx->job_serial);
	u32 i;

	mml_mmp(kick, MMPROFILE_FLAG_START, jobid, 0);

	for (i = 0; i < MML_MAX_CMDQ_CLTS; i++) {
		struct cmdq_client *clt = mml_get_cmdq_clt(ctx->mml, i);

		if (!clt)
			continue;

		mml_mmp(kick, MMPROFILE_FLAG_PULSE, i, 0);
		cmdq_check_thread_complete(clt->chan);
	}

	mml_mmp(kick, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
	kthread_flush_worker(ctx->kt_done);

	mml_mmp(kick, MMPROFILE_FLAG_END, jobid, 0);
	mml_msg("[drm]%s kick done job id %u", __func__, jobid);
}
EXPORT_SYMBOL_GPL(mml_drm_kick_done);

void mml_drm_set_panel_pixel(struct mml_drm_ctx *dctx, u32 panel_width, u32 panel_height)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;

	dctx->panel_width = panel_width;
	dctx->panel_height = panel_height;
	mutex_lock(&ctx->config_mutex);
	list_for_each_entry(cfg, &ctx->configs, entry) {
		/* calculate hrt base on new pixel count */
		cfg->panel_w = panel_width;
		cfg->panel_h = panel_height;
	}
	mutex_unlock(&ctx->config_mutex);
}
EXPORT_SYMBOL_GPL(mml_drm_set_panel_pixel);

s32 mml_drm_racing_config_sync(struct mml_drm_ctx *dctx, struct cmdq_pkt *pkt)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct cmdq_operand lhs, rhs;
	u16 event_target = mml_ir_get_target_event(ctx->mml);

	mml_msg("[drm]%s for disp", __func__);

	/* debug current task idx */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX3,
		atomic_read(&ctx->job_serial) << 16);

	if (ctx->disp_vdo && event_target) {
		/* non-racing to racing case, force clear target line
		 * to make sure next racing begin from target line to sof
		 */
		if (dctx->racing_begin) {
			cmdq_pkt_clear_event(pkt, event_target);
			dctx->racing_begin = false;
			mml_mmp(racing_enter, MMPROFILE_FLAG_PULSE,
				atomic_read(&ctx->job_serial), 0);
		}
		cmdq_pkt_wait_no_clear(pkt, event_target);
	}

	/* set NEXT bit on, to let mml know should jump next */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	cmdq_pkt_set_event(pkt, mml_ir_get_disp_ready_event(ctx->mml));
	cmdq_pkt_wfe(pkt, mml_ir_get_mml_ready_event(ctx->mml));

	/* clear next bit since disp with new mml now */
	rhs.value = ~(u16)MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_racing_config_sync);

s32 mml_drm_racing_stop_sync(struct mml_drm_ctx *dctx, struct cmdq_pkt *pkt)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct cmdq_operand lhs, rhs;
	const struct mml_topology_path *tp_path;
	u32 jobid = atomic_read(&ctx->job_serial);

	/* debug current task idx */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX3, jobid << 16);

	mml_mmp(racing_stop_sync, MMPROFILE_FLAG_START, jobid, 0);

	/* set NEXT bit on, to let mml know should jump next */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	cmdq_pkt_wait_no_clear(pkt, mml_ir_get_mml_stop_event(ctx->mml));

	/* clear next bit since disp with new mml now */
	rhs.value = ~(u16)MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	tp_path = mml_drm_query_dl_path(dctx, NULL, 0);
	if (tp_path)
		cmdq_check_thread_complete(tp_path->clt->chan);
	tp_path = mml_drm_query_dl_path(dctx, NULL, 1);
	if (tp_path)
		cmdq_check_thread_complete(tp_path->clt->chan);

	mml_mmp(racing_stop_sync, MMPROFILE_FLAG_END, 0, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_racing_stop_sync);

enum split_side {
	rect_left,
	rect_top,
	rect_right,
	rect_bottom
};

static void mml_check_boundary_w(struct mml_frame_data *src,
	struct mml_frame_dest *dest, u8 *lrtb)
{
	if (!(dest->crop.r.width & 1))
		return;

	dest->crop.r.width += 1;
	if (dest->crop.r.left + dest->crop.r.width > src->width) {
		dest->crop.r.left -= 1;
		lrtb[rect_left] = 1;
	} else {
		lrtb[rect_right] = 1;
	}
}

static void mml_check_boundary_h(struct mml_frame_data *src,
	struct mml_frame_dest *dest, u8 *lrtb)
{
	if (!(dest->crop.r.height & 1))
		return;

	dest->crop.r.height += 1;
	if (dest->crop.r.top + dest->crop.r.height > src->height) {
		dest->crop.r.top -= 1;
		lrtb[rect_top] = 1;
	} else {
		lrtb[rect_bottom] = 1;
	}
}

static void mml_drm_split_info_racing(struct mml_submit *submit, struct mml_submit *submit_pq)
{
	struct mml_frame_info *info = &submit->info;
	struct mml_frame_info *info_pq = &submit_pq->info;
	struct mml_frame_dest *dest = &info->dest[0];
	u8 lrtb[4] = {0};
	u32 i;

	/* display layer pixel */
	if (!submit->layer.width || !submit->layer.height) {
		submit->layer.width = dest->compose.width;
		submit->layer.height = dest->compose.height;
	}

	submit_pq->info = submit->info;
	submit_pq->buffer = submit->buffer;
	if (submit_pq->job && submit->job)
		*submit_pq->job = *submit->job;
	for (i = 0; i < MML_MAX_OUTPUTS; i++)
		if (submit_pq->pq_param[i] && submit->pq_param[i])
			*submit_pq->pq_param[i] = *submit->pq_param[i];

	if (dest->rotate == MML_ROT_0 ||
	    dest->rotate == MML_ROT_180) {
		dest->compose.left = 0;
		dest->compose.top = 0;
		dest->compose.width = dest->crop.r.width;
		dest->compose.height = dest->crop.r.height;

		if (MML_FMT_H_SUBSAMPLE(dest->data.format))
			mml_check_boundary_w(&info->src, dest, lrtb);
		if (MML_FMT_V_SUBSAMPLE(dest->data.format))
			mml_check_boundary_h(&info->src, dest, lrtb);

		dest->data.width = dest->crop.r.width;
		dest->data.height = dest->crop.r.height;
	} else {
		dest->compose.left = 0;
		dest->compose.top = 0;
		dest->compose.width = dest->crop.r.height; /* even or odd */
		dest->compose.height = dest->crop.r.width;

		if (MML_FMT_H_SUBSAMPLE(dest->data.format)) {
			mml_check_boundary_w(&info->src, dest, lrtb); /* (wrot) align to even */
			mml_check_boundary_h(&info->src, dest, lrtb);
		} else if (MML_FMT_V_SUBSAMPLE(dest->data.format)) {
			mml_check_boundary_w(&info->src, dest, lrtb);
		}

		dest->data.width = dest->crop.r.height; /* even */
		dest->data.height = dest->crop.r.width;
	}

	/* translate padding side to pq crop left/top */
	if (dest->flip) {
		switch (dest->rotate) {
		case MML_ROT_0:
			info_pq->dest[0].crop.r.left = lrtb[rect_right];
			info_pq->dest[0].crop.r.top = lrtb[rect_top];
			break;
		case MML_ROT_90:
			info_pq->dest[0].crop.r.left = lrtb[rect_top];
			info_pq->dest[0].crop.r.top = lrtb[rect_left];
			break;
		case MML_ROT_180:
			info_pq->dest[0].crop.r.left = lrtb[rect_left];
			info_pq->dest[0].crop.r.top = lrtb[rect_bottom];
			break;
		case MML_ROT_270:
			info_pq->dest[0].crop.r.left = lrtb[rect_bottom];
			info_pq->dest[0].crop.r.top = lrtb[rect_right];
			break;
		default:
			info_pq->dest[0].crop.r.left = 0;
			info_pq->dest[0].crop.r.top = 0;
			break;
		}
	} else {
		switch (dest->rotate) {
		case MML_ROT_0:
			info_pq->dest[0].crop.r.left = lrtb[rect_left];
			info_pq->dest[0].crop.r.top = lrtb[rect_top];
			break;
		case MML_ROT_90:
			info_pq->dest[0].crop.r.left = lrtb[rect_bottom];
			info_pq->dest[0].crop.r.top = lrtb[rect_left];
			break;
		case MML_ROT_180:
			info_pq->dest[0].crop.r.left = lrtb[rect_right];
			info_pq->dest[0].crop.r.top = lrtb[rect_bottom];
			break;
		case MML_ROT_270:
			info_pq->dest[0].crop.r.left = lrtb[rect_top];
			info_pq->dest[0].crop.r.top = lrtb[rect_right];
			break;
		default:
			info_pq->dest[0].crop.r.left = 0;
			info_pq->dest[0].crop.r.top = 0;
			break;
		}
	}

	dest->data.y_stride = mml_color_get_min_y_stride(
		dest->data.format, dest->data.width);
	dest->data.uv_stride = mml_color_get_min_uv_stride(
		dest->data.format, dest->data.width);

	info_pq->src = dest->data;
	/* for better wrot burst 16 bytes performance,
	 * always align output width to 16 pixel
	 */
	if (dest->data.y_stride & 0xf &&
		(dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)) {
		u32 align_w = round_up(dest->data.width, 16);

		dest->data.y_stride = mml_color_get_min_y_stride(
			dest->data.format, align_w);
		dest->compose.left = align_w - dest->data.width;
		/* same as
		 * dest->compose.left + dest->compose.width + info_pq->dest[0].crop.r.left
		 */
		info_pq->src.width = align_w;
		info_pq->src.y_stride = dest->data.y_stride;
	}

	memset(&dest->pq_config, 0, sizeof(dest->pq_config));

	info_pq->dest[0].crop.r.left += dest->compose.left;
	info_pq->dest[0].crop.r.width = dest->compose.width;
	info_pq->dest[0].crop.r.height = dest->compose.height;
	info_pq->dest[0].rotate = 0;
	info_pq->dest[0].flip = 0;
	info_pq->mode = MML_MODE_DDP_ADDON;
	submit_pq->buffer.src = submit->buffer.dest[0];
	submit_pq->buffer.src.cnt = 0;
	submit_pq->buffer.dest[0].cnt = 0;

	submit->buffer.seg_map.cnt = 0;
	submit->buffer.dest_cnt = 1;
	submit->buffer.dest[1].cnt = 0;
	info->dest_cnt = 1;
	info_pq->dest[1].crop = info_pq->dest[0].crop;

	if (MML_FMT_PLANE(dest->data.format) > 1)
		mml_err("[drm]%s dest plane should be 1 but format %#010x",
			__func__, dest->data.format);
}

static void mml_drm_split_info_dl(struct mml_submit *submit, struct mml_submit *submit_pq)
{
	u32 i;

	submit_pq->info = submit->info;
	submit_pq->layer = submit->layer;
	for (i = 0; i < MML_MAX_OUTPUTS; i++)
		if (submit_pq->pq_param[i] && submit->pq_param[i])
			*submit_pq->pq_param[i] = *submit->pq_param[i];
	submit_pq->info.mode = MML_MODE_DIRECT_LINK;

	/* display layer pixel */
	if (!submit->layer.width || !submit->layer.height) {
		const struct mml_frame_dest *dest = &submit->info.dest[0];

		if (dest->compose.width && dest->compose.height) {
			submit->layer.width = dest->compose.width;
			submit->layer.height = dest->compose.height;
		} else {
			submit->layer.width = dest->data.width;
			submit->layer.height = dest->data.height;
		}
	}
}

void mml_drm_split_info(struct mml_submit *submit, struct mml_submit *submit_pq)
{
	if (submit->info.mode == MML_MODE_RACING)
		mml_drm_split_info_racing(submit, submit_pq);
	else if (submit->info.mode == MML_MODE_DIRECT_LINK)
		mml_drm_split_info_dl(submit, submit_pq);
}
EXPORT_SYMBOL_GPL(mml_drm_split_info);

const struct mml_topology_path *mml_drm_query_dl_path(struct mml_drm_ctx *dctx,
	struct mml_submit *submit, u32 pipe)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	const struct mml_topology_path *path = NULL;
	struct mml_topology_cache *tp = mml_topology_get_cache(ctx->mml);
	struct mml_frame_size panel = {.width = dctx->panel_width, .height = dctx->panel_height};

	if (submit) {
		/* from mml_mutex ddp addon, construct sof, assume use last dl config */
		mutex_lock(&ctx->config_mutex);
		list_for_each_entry(cfg, &ctx->configs, entry) {
			if (cfg->info.mode != MML_MODE_DIRECT_LINK)
				continue;

			path = cfg->path[pipe];

			/* The tp path not select, yet, in first task.
			 * Hence use same info do query.
			 */
			if (!path && tp)
				path = tp->op->get_dl_path(tp, &cfg->info, pipe, &panel);

			break;
		}
		mutex_unlock(&ctx->config_mutex);

		if (path)
			return path;
	}

	if (!tp || !tp->op->get_dl_path)
		return NULL;

	return tp->op->get_dl_path(tp, submit ? &submit->info : NULL, pipe, &panel);
}

void mml_drm_submit_timeout(void)
{
	//mml_aee("mml", "mml_drm_submit timeout");
	mml_err("mml_drm_submit timeout");
}
EXPORT_SYMBOL_GPL(mml_drm_submit_timeout);

