// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Iris-SC Yang <iris-sc.yang@mediatek.com>
 */
#include <linux/time.h>

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk-mml-m2m-adaptor.h"
#include "mtk-mml-adaptor.h"
#include "mtk-mml-v4l2.h"
#include "mtk-mml-v4l2-color.h"
#include "mtk-mml-mmp.h"

int m2m_max_cache_task = 4;
module_param(m2m_max_cache_task, int, 0644);

int m2m_max_cache_cfg = 2;
module_param(m2m_max_cache_cfg, int, 0644);

struct mml_v4l2_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *m2m_vdev;
	struct v4l2_m2m_dev *m2m_dev;
	struct mutex m2m_mutex;
};

#define MML_M2M_MAX_CTRLS	10

enum {
	MML_M2M_FRAME_SRC = 0,
	MML_M2M_FRAME_DST,
	MML_M2M_FRAME_MAX,
};

struct mml_m2m_frame {
	struct v4l2_format format;
	const struct mml_m2m_format *mml_fmt;
};

struct mml_m2m_param {
	struct list_head entry;
	s32 rotation;
	u32 hflip:1;
	u32 vflip:1;
	u32 secure:1;
	u32 alpha:1;
	struct v4l2_pq_submit pq_submit;
};

struct mml_m2m_ctx {
	struct mml_ctx ctx;
	struct mml_submit submit;
	const struct mml_m2m_limit *limit;
	struct mml_m2m_frame output;	/* src buffer */
	struct mml_m2m_frame capture;	/* dst buffer */
	struct mml_m2m_param param;
	struct list_head params;
	struct mutex param_mutex;

	/* v4l2 m2m context */
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler ctrl_handler;
	struct mml_m2m_ctrls {
		struct v4l2_ctrl *hflip;
		struct v4l2_ctrl *vflip;
		struct v4l2_ctrl *rotate;
		struct v4l2_ctrl *pq;
		struct v4l2_ctrl *secure;
	} ctrls;
	struct v4l2_m2m_ctx *m2m_ctx;
	u32 frame_count[MML_M2M_FRAME_MAX];

	struct mutex q_mutex;
	struct kref ref;
	struct completion destroy;	/* ready for destroy */
};

static void m2m_ctx_complete(struct kref *ref)
{
	struct mml_m2m_ctx *ctx = container_of(ref, struct mml_m2m_ctx, ref);

	complete(&ctx->destroy);
}

static void m2m_param_queue(struct mml_m2m_ctx *ctx)
{
	struct mml_m2m_param *param;

	param = kzalloc(sizeof(*param), GFP_KERNEL);
	*param = ctx->param;
	mutex_lock(&ctx->param_mutex);
	list_add_tail(&param->entry, &ctx->params);
	mutex_unlock(&ctx->param_mutex);
}

static void m2m_param_remove(struct mml_m2m_ctx *ctx)
{
	struct mml_m2m_param *param;

	mutex_lock(&ctx->param_mutex);
	param = list_first_entry_or_null(&ctx->params, struct mml_m2m_param, entry);
	if (param) {
		list_del(&param->entry);
		kfree(param);
	}
	mutex_unlock(&ctx->param_mutex);
}

static void mml_m2m_process_done(struct mml_m2m_ctx *mctx, enum vb2_buffer_state vb_state)
{
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct mml_v4l2_dev *v4l2_dev = mml_get_v4l2_dev(mctx->ctx.mml);

	m2m_param_remove(mctx);

	src_buf = v4l2_m2m_src_buf_remove(mctx->m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(mctx->m2m_ctx);
	if (!src_buf ||!dst_buf) {
		mml_err("[m2m]%s no src or dst buffer found", __func__);
		return;
	}
	src_buf->sequence = mctx->frame_count[MML_M2M_FRAME_SRC]++;
	dst_buf->sequence = mctx->frame_count[MML_M2M_FRAME_DST]++;
	v4l2_m2m_buf_copy_metadata(src_buf, dst_buf, true);

	v4l2_m2m_buf_done(src_buf, vb_state);
	v4l2_m2m_buf_done(dst_buf, vb_state);
	v4l2_m2m_job_finish(v4l2_dev->m2m_dev, mctx->m2m_ctx);
}

static void m2m_task_frame_done(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_config *tmp;
	struct mml_ctx *ctx = task->ctx;
	struct mml_dev *mml = cfg->mml;
	enum vb2_buffer_state vb_state = VB2_BUF_STATE_DONE;
	struct mml_m2m_ctx *mctx = container_of(ctx, struct mml_m2m_ctx, ctx);

	mml_trace_ex_begin("%s", __func__);

	mml_msg("[m2m]frame done task %p state %u job %u",
		task, task->state, task->job.jobid);

	/* clean up */
	task_buf_put(task);

	mutex_lock(&ctx->config_mutex);

	if (unlikely(!task->pkts[0] || (cfg->dual && !task->pkts[1]))) {
		task_state_dec(cfg, task, __func__);
		mml_err("[m2m]%s task cnt (%u %u %u) error from state %d",
			__func__,
			cfg->await_task_cnt,
			cfg->run_task_cnt,
			cfg->done_task_cnt,
			task->state);
		task->err = true;
		cfg->err = true;
		vb_state = VB2_BUF_STATE_ERROR;
		mml_record_track(mml, task);
		kref_put(&task->ref, task_move_to_destroy);
	} else {
		/* works fine, safe to move */
		task_move_to_idle(task);
		mml_record_track(mml, task);
	}

	if (cfg->done_task_cnt > m2m_max_cache_task) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task), entry);
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		mml_msg("[m2m]%s task cnt (%u %u %hhu)",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);
		kref_put(&task->ref, task_move_to_destroy);
	}

	/* still have room to cache, done */
	if (ctx->config_cnt <= m2m_max_cache_cfg)
		goto done;

	/* must pick cfg from list which is not running */
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* only remove config not running */
		if (!list_empty(&cfg->tasks) || !list_empty(&cfg->await_tasks))
			continue;
		list_del_init(&cfg->entry);
		frame_config_queue_destroy(cfg);
		ctx->config_cnt--;
		mml_msg("[m2m]config %p send destroy remain %u",
			cfg, ctx->config_cnt);

		/* check cache num again */
		if (ctx->config_cnt <= m2m_max_cache_cfg)
			break;
	}

done:
	mutex_unlock(&ctx->config_mutex);

	/* notice frame done to v4l2 */
	mml_m2m_process_done(mctx, vb_state);

	/* kref_get mctx->ref in mml_m2m_device_run */
	kref_put(&mctx->ref, m2m_ctx_complete);

	mml_lock_wake_lock(mml, false);

	mml_trace_ex_end();
}

static const struct mml_task_ops m2m_task_ops = {
	.submit_done = task_submit_done,
	.frame_done = m2m_task_frame_done,
	.dup_task = task_dup,
	.get_tile_cache = task_get_tile_cache,
};

static const struct mml_config_ops m2m_config_ops = {
	.get = frame_config_get,
	.put = frame_config_put,
	.free = frame_config_free,
};

static struct mml_m2m_frame *ctx_get_frame(struct mml_m2m_ctx *ctx,
					   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->output;
	else
		return &ctx->capture;
}

static struct mml_frame_dest *ctx_get_submit_dest(struct mml_m2m_ctx *ctx, int index)
{
	return &ctx->submit.info.dest[index];
}

static struct mml_frame_data *ctx_get_submit_frame(struct mml_m2m_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->submit.info.src;
	else
		return &ctx->submit.info.dest[0].data;
}

static struct mml_buffer *ctx_get_submit_buffer(struct mml_m2m_ctx *ctx,
						enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->submit.buffer.src;
	else
		return &ctx->submit.buffer.dest[0];
}

static void get_fmt_str(char *fmt, size_t sz, enum mml_color f)
{
	int ret;

	ret = snprintf(fmt, sz, "%u%s%s%s%s%s%s%s%s",
		MML_FMT_HW_FORMAT(f),
		MML_FMT_SWAP(f) ? "s" : "",
		MML_FMT_ALPHA(f) && MML_FMT_IS_YUV(f) ? "y" : "",
		MML_FMT_BLOCK(f) ? "b" : "",
		MML_FMT_INTERLACED(f) ? "i" : "",
		MML_FMT_UFO(f) ? "u" : "",
		MML_FMT_10BIT_TILE(f) ? "t" :
		MML_FMT_10BIT_PACKED(f) ? "p" :
		MML_FMT_10BIT_LOOSE(f) ? "l" : "",
		MML_FMT_10BIT_JUMP(f) ? "j" : "",
		MML_FMT_AFBC(f) ? "c" :
		MML_FMT_HYFBC(f) ? "h" : "");
	if (ret < 0)
		fmt[0] = '\0';
}

static void get_frame_str(char *frame, size_t sz, const struct mml_frame_data *data)
{
	char fmt[24];
	int ret;

	get_fmt_str(fmt, sizeof(fmt), data->format);
	ret = snprintf(frame, sz, "(%u, %u)[%u %u] %#010x C%s P%hu%s",
		data->width, data->height, data->y_stride,
		MML_FMT_AFBC(data->format) ? data->vert_stride : data->uv_stride,
		data->format, fmt,
		data->profile,
		data->secure ? " sec" : "");
	if (ret < 0)
		frame[0] = '\0';
}

static void dump_m2m_ctx(struct mml_m2m_ctx *ctx)
{
	const struct mml_m2m_frame *output;
	const struct mml_m2m_frame *capture;
	const struct mml_frame_data *src;
	const struct mml_frame_dest *dest;
	const struct mml_pq_config *pq_config;
	char frame[60];

	output = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	capture = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	src = ctx_get_submit_frame(ctx, output->format.type);
	dest = ctx_get_submit_dest(ctx, 0);
	pq_config = &ctx->param.pq_submit.pq_config;

	get_frame_str(frame, sizeof(frame), src);
	mml_log("[m2m] in v4l2:(%u, %u) mml:%s plane:%hhu",
		output->format.fmt.pix_mp.width, output->format.fmt.pix_mp.height,
		frame,
		src->plane_cnt);

	get_frame_str(frame, sizeof(frame), &dest->data);
	mml_log("[m2m]out v4l2:(%u, %u) mml:%s plane:%hhu r:%hu%s",
		capture->format.fmt.pix_mp.width, capture->format.fmt.pix_mp.height,
		frame,
		dest->data.plane_cnt,
		dest->rotate,
		dest->flip ? " flip" : "");
	mml_log("[m2m]    crop:(%u, %u, %u, %u) compose:(%u, %u, %u, %u)",
		dest->crop.r.left,
		dest->crop.r.top,
		dest->crop.r.width,
		dest->crop.r.height,
		dest->compose.left,
		dest->compose.top,
		dest->compose.width,
		dest->compose.height);

	mml_log("[m2m]v4l2-ctrl: r:%d%s%s%s alpha:%d pq %u:%s%s%s%s%s%s%s%s%s%s",
		ctx->param.rotation,
		ctx->param.hflip ? " hflip" : "",
		ctx->param.vflip ? " vflip" : "",
		ctx->param.secure ? " sec" : "",
		ctx->param.alpha,
		ctx->param.pq_submit.id,
		pq_config->en ? " PQ" : "",
		pq_config->en_fg ? " FG" : "",
		pq_config->en_hdr ? " HDR" : "",
		pq_config->en_ccorr ? " CCORR" : "",
		pq_config->en_dre ? " DRE" : "",
		pq_config->en_sharp ? " SHP" : "",
		pq_config->en_ur ? " UR" : "",
		pq_config->en_dc ? " DC" : "",
		pq_config->en_color ? " COLOR" : "",
		pq_config->en_c3d ? " C3D" : "");
}

static int mml_check_scaling_ratio(const struct mml_rect *crop,
	const struct mml_rect *compose, s32 rotation,
	const struct mml_m2m_limit *limit)
{
	u32 crop_w, crop_h, comp_w, comp_h;

	crop_w = crop->width;
	crop_h = crop->height;
	if (90 == rotation || 270 == rotation) {
		comp_w = compose->height;
		comp_h = compose->width;
	} else {
		comp_w = compose->width;
		comp_h = compose->height;
	}

	if ((crop_w / comp_w) > limit->h_scale_down_max ||
	    (crop_h / comp_h) > limit->v_scale_down_max ||
	    (comp_w / crop_w) > limit->h_scale_up_max ||
	    (comp_h / crop_h) > limit->v_scale_up_max)
		return -ERANGE;
	return 0;
}

static int mml_m2m_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mml_m2m_ctx *ctx = vb2_get_drv_priv(q);
	struct mml_frame_dest *dest;
	int ret;
	bool out_streaming, cap_streaming;

	if (V4L2_TYPE_IS_OUTPUT(q->type))
		ctx->frame_count[MML_M2M_FRAME_SRC] = 0;
	if (V4L2_TYPE_IS_CAPTURE(q->type))
		ctx->frame_count[MML_M2M_FRAME_DST] = 0;

	dest = ctx_get_submit_dest(ctx, 0);
	out_streaming = vb2_is_streaming(v4l2_m2m_get_src_vq(ctx->m2m_ctx));
	cap_streaming = vb2_is_streaming(v4l2_m2m_get_dst_vq(ctx->m2m_ctx));

	/* Check to see if scaling ratio is within supported range */
	if ((V4L2_TYPE_IS_OUTPUT(q->type) && cap_streaming) ||
	    (V4L2_TYPE_IS_CAPTURE(q->type) && out_streaming)) {
		ret = mml_check_scaling_ratio(&dest->crop.r,
					      &dest->compose,
					      ctx->param.rotation,
					      ctx->limit);
		if (ret) {
			mml_err("[m2m]%s out of scaling range crop(%u,%u) compose(%u,%u)",
				__func__, dest->crop.r.width, dest->crop.r.height,
				dest->compose.width, dest->compose.height);
			return ret;
		}
	}

	return 0;
}

static struct vb2_v4l2_buffer *mml_m2m_buf_remove(struct mml_m2m_ctx *ctx,
						  unsigned int type)
{
	struct vb2_v4l2_buffer *vbuf;

	if (V4L2_TYPE_IS_OUTPUT(type)) {
		vbuf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (vbuf)
			m2m_param_remove(ctx);
		return vbuf;
	} else {
		return v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	}
}

static void mml_m2m_stop_streaming(struct vb2_queue *q)
{
	struct mml_m2m_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vbuf;

	vbuf = mml_m2m_buf_remove(ctx, q->type);
	while (vbuf) {
		v4l2_m2m_buf_done(vbuf, VB2_BUF_STATE_ERROR);
		vbuf = mml_m2m_buf_remove(ctx, q->type);
	}
}

static int mml_m2m_queue_setup(struct vb2_queue *q, unsigned int *num_buffers,
			       unsigned int *num_planes, unsigned int sizes[],
			       struct device *alloc_devs[])
{
	struct mml_m2m_ctx *ctx = vb2_get_drv_priv(q);
	struct v4l2_pix_format_mplane *pix_mp;
	u32 i;

	pix_mp = &ctx_get_frame(ctx, q->type)->format.fmt.pix_mp;

	if (*num_planes) {
		/* from VIDIOC_CREATE_BUFS */
		if (*num_planes != pix_mp->num_planes)
			return -EINVAL;
		for (i = 0; i < pix_mp->num_planes; i++)
			if (sizes[i] < pix_mp->plane_fmt[i].sizeimage)
				return -EINVAL;
	} else {
		/* from VIDIOC_REQBUFS */
		*num_planes = pix_mp->num_planes;
		for (i = 0; i < pix_mp->num_planes; i++)
			sizes[i] = pix_mp->plane_fmt[i].sizeimage;
	}

	return 0;
}

static int mml_m2m_buf_prepare(struct vb2_buffer *vb)
{
	struct mml_m2m_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_pix_format_mplane *pix_mp;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	u32 i;

	vbuf->field = V4L2_FIELD_NONE;

	if (V4L2_TYPE_IS_CAPTURE(vb->type)) {
		pix_mp = &ctx_get_frame(ctx, vb->type)->format.fmt.pix_mp;
		for (i = 0; i < pix_mp->num_planes; i++) {
			vb2_set_plane_payload(vb, i,
					      pix_mp->plane_fmt[i].sizeimage);
		}
	}
	return 0;
}

static int mml_m2m_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	vbuf->field = V4L2_FIELD_NONE;
	return 0;
}

static void mml_m2m_buf_queue(struct vb2_buffer *vb)
{
	struct mml_m2m_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	vbuf->field = V4L2_FIELD_NONE;
	v4l2_m2m_buf_queue(ctx->m2m_ctx, vbuf);
	if (V4L2_TYPE_IS_OUTPUT(vb->type))
		m2m_param_queue(ctx);
}

static const struct vb2_ops mml_m2m_qops = {
	.queue_setup	= mml_m2m_queue_setup,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.buf_prepare	= mml_m2m_buf_prepare,
	.start_streaming = mml_m2m_start_streaming,
	.stop_streaming	= mml_m2m_stop_streaming,
	.buf_queue	= mml_m2m_buf_queue,
	.buf_out_validate = mml_m2m_buf_out_validate,
};

static struct mml_m2m_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mml_m2m_ctx, fh);
}

static const struct mml_m2m_format *m2m_find_fmt(u32 pixelformat, u32 type)
{
	u32 i;

	type = V4L2_TYPE_IS_OUTPUT(type) ? MML_M2M_FMT_OUTPUT :
					   MML_M2M_FMT_CAPTURE;
	for (i = 0; i < ARRAY_SIZE(mml_m2m_formats); i++) {
		if (!(mml_m2m_formats[i].types & type))
			continue;
		if (mml_m2m_formats[i].pixelformat == pixelformat)
			return &mml_m2m_formats[i];
	}
	return NULL;
}

static const struct mml_m2m_format *m2m_find_fmt_by_index(u32 index, u32 type)
{
	u32 i, num = 0;

	type = V4L2_TYPE_IS_OUTPUT(type) ? MML_M2M_FMT_OUTPUT :
					   MML_M2M_FMT_CAPTURE;
	for (i = 0; i < ARRAY_SIZE(mml_m2m_formats); i++) {
		if (!(mml_m2m_formats[i].types & type))
			continue;
		if (index == num)
			return &mml_m2m_formats[i];
		num++;
	}
	return NULL;
}

static int mml_clamp_align(s32 *x, int min, int max, unsigned int align)
{
	unsigned int mask;

	if (min < 0 || max < 0)
		return -ERANGE;

	/* Bits that must be zero to be aligned */
	mask = ~((1 << align) - 1);

	min = 0 ? 0 : ((min + ~mask) & mask);
	max = max & mask;
	if ((unsigned int)min > (unsigned int)max)
		return -ERANGE;

	/* Clamp to aligned min and max */
	*x = clamp(*x, min, max);

	/* Round to nearest aligned value */
	if (align)
		*x = (*x + (1 << (align - 1))) & mask;
	return 0;
}

static int m2m_clamp_start(s32 *x, int min, int max, unsigned int align, u32 flags)
{
	if (flags & V4L2_SEL_FLAG_GE)
		max = *x;
	if (flags & V4L2_SEL_FLAG_LE)
		min = *x;
	return mml_clamp_align(x, min, max, align);
}

static int m2m_clamp_end(s32 *x, int min, int max, unsigned int align, u32 flags)
{
	if (flags & V4L2_SEL_FLAG_GE)
		min = *x;
	if (flags & V4L2_SEL_FLAG_LE)
		max = *x;
	return mml_clamp_align(x, min, max, align);
}

static bool m2m_target_is_crop(u32 target)
{
	return (target == V4L2_SEL_TGT_CROP) ||
		(target == V4L2_SEL_TGT_CROP_DEFAULT) ||
		(target == V4L2_SEL_TGT_CROP_BOUNDS);
}

static bool m2m_target_is_compose(u32 target)
{
	return (target == V4L2_SEL_TGT_COMPOSE) ||
		(target == V4L2_SEL_TGT_COMPOSE_DEFAULT) ||
		(target == V4L2_SEL_TGT_COMPOSE_BOUNDS);
}

static int m2m_try_crop(struct mml_m2m_ctx *ctx, struct v4l2_rect *r,
	const struct v4l2_selection *s, struct mml_m2m_frame *frame)
{
	s32 left, top, right, bottom;
	u32 framew, frameh, walign = 0, halign = 0;
	int ret;

	mml_msg("%s target:%d, set:(%d,%d) %ux%u", __func__,
		s->target, s->r.left, s->r.top, s->r.width, s->r.height);

	left = s->r.left;
	top = s->r.top;
	right = s->r.left + s->r.width;
	bottom = s->r.top + s->r.height;
	framew = frame->format.fmt.pix_mp.width;
	frameh = frame->format.fmt.pix_mp.height;

	if (m2m_target_is_compose(s->target)) {
		walign = frame->mml_fmt->walign;
		halign = frame->mml_fmt->halign;
	}

	mml_msg("%s align:%u,%u, bound:%ux%u", __func__,
		walign, halign, framew, frameh);

	ret = m2m_clamp_start(&left, 0, right, walign, s->flags);
	if (ret)
		return ret;
	ret = m2m_clamp_start(&top, 0, bottom, halign, s->flags);
	if (ret)
		return ret;
	ret = m2m_clamp_end(&right, left, framew, walign, s->flags);
	if (ret)
		return ret;
	ret = m2m_clamp_end(&bottom, top, frameh, halign, s->flags);
	if (ret)
		return ret;

	r->left = left;
	r->top = top;
	r->width = right - left;
	r->height = bottom - top;

	mml_msg("%s crop:(%d,%d) %ux%u", __func__,
		r->left, r->top, r->width, r->height);
	return 0;
}

static enum mml_ycbcr_profile m2m_map_ycbcr_prof_mplane(
	const struct v4l2_pix_format_mplane *pix_mp, enum mml_color color)
{
	if (MML_FMT_IS_RGB(color))
		return MML_YCBCR_PROFILE_FULL_BT601;

	switch (pix_mp->colorspace) {
	case V4L2_COLORSPACE_JPEG:
		return MML_YCBCR_PROFILE_JPEG;
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_DCI_P3:
		if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
			return MML_YCBCR_PROFILE_FULL_BT709;
		return MML_YCBCR_PROFILE_BT709;
	case V4L2_COLORSPACE_BT2020:
		if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
			return MML_YCBCR_PROFILE_FULL_BT2020;
		return MML_YCBCR_PROFILE_BT2020;
	default:
		if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
			return MML_YCBCR_PROFILE_FULL_BT601;
		return MML_YCBCR_PROFILE_BT601;
	}
}

static enum mml_gamut m2m_map_mml_gamut(enum v4l2_colorspace colorspace)
{
	switch (colorspace) {
	case V4L2_COLORSPACE_DEFAULT:
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_SMPTE240M:
		return MML_GAMUT_BT601;

	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_JPEG:
	case V4L2_COLORSPACE_SRGB:
		return MML_GAMUT_BT709;

	case V4L2_COLORSPACE_BT2020:
		return MML_GAMUT_BT2020;

	case V4L2_COLORSPACE_DCI_P3:
		return MML_GAMUT_DISPLAY_P3;

	case V4L2_COLORSPACE_OPRGB:
		return MML_GAMUT_ADOBE_RGB;

	default:
		return MML_GAMUT_UNSUPPORTED;
	}
}

static enum mml_ycbcr_encoding m2m_map_mml_ycbcr_enc(enum v4l2_ycbcr_encoding ycbcr_enc)
{
	switch (ycbcr_enc) {
	case V4L2_YCBCR_ENC_DEFAULT:
	case V4L2_YCBCR_ENC_601:
		return MML_YCBCR_ENC_BT601;

	case V4L2_YCBCR_ENC_709:
		return MML_YCBCR_ENC_BT709;

	case V4L2_YCBCR_ENC_BT2020:
		return MML_YCBCR_ENC_BT2020;

	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
		return MML_YCBCR_ENC_BT2020_CON;

	default:
		return MML_YCBCR_ENC_UNSUPPORTED;
	}
}

static enum mml_color_range m2m_map_mml_color_range(enum v4l2_quantization quant)
{
	switch (quant) {
	case V4L2_QUANTIZATION_DEFAULT:
	case V4L2_QUANTIZATION_LIM_RANGE:
		return MML_COLOR_RANGE_LIMITED;

	case V4L2_QUANTIZATION_FULL_RANGE:
		return MML_COLOR_RANGE_FULL;

	default:
		return MML_COLOR_RANGE_UNSUPPORTED;
	}
}

static enum mml_gamma m2m_map_mml_gamma(enum v4l2_xfer_func xfer)
{
	switch (xfer) {
	case V4L2_XFER_FUNC_DEFAULT:
	case V4L2_XFER_FUNC_709:
		return MML_GAMMA_ITURBT709;

	case V4L2_XFER_FUNC_SRGB:
		return MML_GAMMA_GAMMA2_2CURVE;

	case V4L2_XFER_FUNC_NONE:
		return MML_GAMMA_LINEAR;

	case V4L2_XFER_FUNC_SMPTE2084:
		return MML_GAMMA_SMPTEST2084;

	case V4L2_XFER_FUNC_OPRGB:
		return MML_GAMMA_ADOBE_RGB;

	case V4L2_XFER_FUNC_DCI_P3:
		return MML_GAMMA_GAMMA2_6CURVE;

	default:
		return MML_GAMMA_UNSUPPORTED;
	}
}

static int m2m_try_colorspace_mplane(
	struct v4l2_pix_format_mplane *pix_mp, enum mml_color color)
{
	int err = 0;

	if (m2m_map_mml_gamut(pix_mp->colorspace) >= MML_GAMUT_UNSUPPORTED) {
		mml_log("[m2m] colorspace %u not support", pix_mp->colorspace);
		err = -EINVAL;
	}
	if (m2m_map_mml_gamma(pix_mp->xfer_func) >= MML_GAMMA_UNSUPPORTED) {
		mml_log("[m2m] xfer_func %u not support", pix_mp->xfer_func);
		err = -EINVAL;
	}

	if (MML_FMT_IS_YUV(color)) {
		if (m2m_map_mml_ycbcr_enc(pix_mp->ycbcr_enc) >= MML_YCBCR_ENC_UNSUPPORTED) {
			mml_log("[m2m] ycbcr_enc %u not support", pix_mp->ycbcr_enc);
			err = -EINVAL;
		}
		if (m2m_map_mml_color_range(pix_mp->quantization) >= MML_COLOR_RANGE_UNSUPPORTED) {
			mml_log("[m2m] quantization %u not support", pix_mp->quantization);
			err = -EINVAL;
		}
	} else {
		if (pix_mp->ycbcr_enc != V4L2_YCBCR_ENC_DEFAULT) {
			pix_mp->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
			mml_log("[m2m] RGB only support V4L2_YCBCR_ENC_DEFAULT");
		}
		if (pix_mp->quantization != V4L2_QUANTIZATION_DEFAULT) {
			pix_mp->quantization = V4L2_QUANTIZATION_DEFAULT;
			mml_log("[m2m] RGB only support V4L2_QUANTIZATION_DEFAULT");
		}
	}

	return err;
}

static const struct mml_m2m_format *m2m_try_fmt_mplane(struct v4l2_format *f,
	const struct mml_m2m_ctx *ctx)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct mml_m2m_format *fmt;
	const struct mml_m2m_pix_limit *pix_limit;
	u32 org_w, org_h, wmin, hmin;
	unsigned int i;

	fmt = m2m_find_fmt(pix_mp->pixelformat, f->type);
	if (!fmt) {
		fmt = m2m_find_fmt_by_index(0, f->type);
		if (!fmt) {
			mml_err("[m2m]%s pixelformat %c%c%c%c invalid", __func__,
				(pix_mp->pixelformat & 0xff),
				(pix_mp->pixelformat >>  8) & 0xff,
				(pix_mp->pixelformat >> 16) & 0xff,
				(pix_mp->pixelformat >> 24) & 0xff);
			return NULL;
		}
	}

	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->pixelformat = fmt->pixelformat;
	if (V4L2_TYPE_IS_CAPTURE(f->type)) {
		if (!(pix_mp->flags & V4L2_PIX_FMT_FLAG_SET_CSC) ||
		    m2m_try_colorspace_mplane(pix_mp, fmt->mml_color)) {
			pix_mp->flags &= ~V4L2_PIX_FMT_FLAG_SET_CSC;
			pix_mp->colorspace = V4L2_COLORSPACE_DEFAULT;
			pix_mp->xfer_func = V4L2_XFER_FUNC_DEFAULT;
			pix_mp->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
			pix_mp->quantization = V4L2_QUANTIZATION_DEFAULT;
			mml_msg("[m2m] reset CSC to default");
		}
	}

	pix_limit = V4L2_TYPE_IS_OUTPUT(f->type) ? &ctx->limit->out_limit :
						&ctx->limit->cap_limit;
	org_w = pix_mp->width;
	org_h = pix_mp->height;

	/* v4l_bound_align_image rounds value to nearest aligned value at first.
	 * While we want to round always up, use clamped value as min.
	 */
	wmin = clamp(pix_mp->width, pix_limit->wmin, pix_limit->wmax);
	hmin = clamp(pix_mp->height, pix_limit->hmin, pix_limit->hmax);
	v4l_bound_align_image(&pix_mp->width, wmin, pix_limit->wmax, fmt->walign,
			      &pix_mp->height, hmin, pix_limit->hmax, fmt->halign,
			      fmt->salign);
	if (org_w != pix_mp->width || org_h != pix_mp->height)
		mml_msg("[m2m]%s size change: %ux%u to %ux%u", __func__,
			org_w, org_h, pix_mp->width, pix_mp->height);
	if (pix_mp->num_planes && pix_mp->num_planes != fmt->num_planes)
		mml_msg("[m2m]%s num of planes change: %u to %u", __func__,
			pix_mp->num_planes, fmt->num_planes);
	pix_mp->num_planes = fmt->num_planes;

	for (i = 0; i < pix_mp->num_planes; i++) {
		u32 min_bpl = (pix_mp->width * fmt->row_depth[i]) >> 3;
		u32 max_bpl = (pix_limit->wmax * fmt->row_depth[i]) >> 3;
		u32 bpl = pix_mp->plane_fmt[i].bytesperline;
		u32 min_si, max_si;
		u32 si = pix_mp->plane_fmt[i].sizeimage;

		if (fmt->pixelformat == V4L2_PIX_FMT_YVU420A)
			min_bpl = round_up(min_bpl, 16);

		bpl = clamp(bpl, min_bpl, max_bpl);
		pix_mp->plane_fmt[i].bytesperline = bpl;

		min_si = (bpl * pix_mp->height * fmt->depth[i]) /
			 fmt->row_depth[i];
		max_si = (bpl * pix_limit->hmax * fmt->depth[i]) /
			 fmt->row_depth[i];

		if (fmt->pixelformat == V4L2_PIX_FMT_YVU420A)
			min_si = (bpl + round_up(bpl / 2, 16)) * pix_mp->height;

		si = clamp(si, min_si, max_si);
		pix_mp->plane_fmt[i].sizeimage = si;

		mml_msg("[m2m]%s p%u, bpl:%u [%u, %u], sizeimage:%u [%u, %u]",
			__func__, i, bpl, min_bpl, max_bpl, si, min_si, max_si);
	}

	return fmt;
}

static int mml_m2m_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mml_m2m_ctx *ctx = container_of(ctrl->handler, struct mml_m2m_ctx, ctrl_handler);
	struct device *mmu_dev;
	struct vb2_queue *src_vq, *dst_vq;

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		ctx->param.hflip = ctrl->val;
		mml_msg("[m2m]%s set hflip: %d", __func__, ctx->param.hflip);
		break;
	case V4L2_CID_VFLIP:
		ctx->param.vflip = ctrl->val;
		mml_msg("[m2m]%s set vflip: %d", __func__, ctx->param.vflip);
		break;
	case V4L2_CID_ROTATE:
		ctx->param.rotation = ctrl->val;
		mml_msg("[m2m]%s set rotation: %d", __func__, ctx->param.rotation);
		break;
	case MML_M2M_CID_PQPARAM:
		ctx->param.pq_submit = *(struct v4l2_pq_submit *)ctrl->p_new.p;
		mml_msg("[m2m]%s set pq_submit: %u fg_grain_seed: %d", __func__,
			ctx->param.pq_submit.id,
			ctx->param.pq_submit.pq_param.video_param.fg_meta.grain_seed);
		break;
	case MML_M2M_CID_SECURE:
		ctx->param.secure = ctrl->val;
		mmu_dev = mml_get_mmu_dev(ctx->ctx.mml, ctrl->val);
		src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		src_vq->dev = mmu_dev;
		dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		dst_vq->dev = mmu_dev;
		mml_msg("[m2m]%s set secure: %d", __func__, ctx->param.secure);
		break;
	default:
		mml_err("[m2m]%s not supported s_ctrl ctrl->id: %u", __func__, ctrl->id);
	}
	return 0;
}

static const struct v4l2_ctrl_ops mml_m2m_ctrl_ops = {
	.s_ctrl = mml_m2m_s_ctrl,
};

static bool mml_m2m_ctrl_type_op_equal(const struct v4l2_ctrl *ctrl,
	union v4l2_ctrl_ptr ptr1, union v4l2_ctrl_ptr ptr2)
{
	return !memcmp(ptr1.p_const, ptr2.p_const,
		ctrl->elems * ctrl->elem_size);
}

static void mml_m2m_ctrl_type_op_init(const struct v4l2_ctrl *ctrl,
	u32 from_idx, union v4l2_ctrl_ptr ptr)
{
	unsigned int i;
	u32 tot_elems = ctrl->elems;

	if (from_idx >= tot_elems)
		return;

	for (i = from_idx; i < tot_elems; i++) {
		void *p = ptr.p + i * ctrl->elem_size;

		if (ctrl->p_def.p_const)
			memcpy(p, ctrl->p_def.p_const, ctrl->elem_size);
		else
			memset(p, 0, ctrl->elem_size);
	}
}

static void mml_m2m_ctrl_type_op_log(const struct v4l2_ctrl *ctrl)
{
	pr_cont("MML_M2M_PQPARAM");
}

static int mml_m2m_ctrl_type_op_validate(const struct v4l2_ctrl *ctrl,
	union v4l2_ctrl_ptr ptr)
{
	unsigned int i;

	for (i = 0; i < ctrl->new_elems; i++) {
		struct v4l2_pq_submit *pq_submit = ptr.p + i * ctrl->elem_size;
		u32 pqen = 0;

		memcpy(&pqen, &pq_submit->pq_config,
			min(sizeof(pq_submit->pq_config), sizeof(pqen)));
		if (!pqen && pq_submit->pq_param.enable) {
			mml_err("[m2m] pq en = 0 but pq_param enable, not match!");
			return -EINVAL;
		}
		if (pq_submit->pq_config.en_sharp || pq_submit->pq_config.en_ur ||
			pq_submit->pq_config.en_dc || pq_submit->pq_config.en_color ||
			pq_submit->pq_config.en_ccorr || pq_submit->pq_config.en_dre ||
			pq_submit->pq_config.en_region_pq || pq_submit->pq_config.en_c3d) {
			mml_err("[m2m] unsupport PQ func! (Support: HDR, FG)");
			return -EINVAL;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_type_ops mml_m2m_type_ops = {
	.equal = mml_m2m_ctrl_type_op_equal,
	.init = mml_m2m_ctrl_type_op_init,
	.log = mml_m2m_ctrl_type_op_log,
	.validate = mml_m2m_ctrl_type_op_validate,
};

static const struct v4l2_ctrl_config m2m_sec_cfg = {
	.ops = &mml_m2m_ctrl_ops,
	.id = MML_M2M_CID_SECURE,
	.name = "Secure Mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.max = 1,
	.step = 1,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
};

static const struct v4l2_ctrl_config m2m_pq_cfg = {
	.ops = &mml_m2m_ctrl_ops,
	.type_ops = &mml_m2m_type_ops,
	.id = MML_M2M_CID_PQPARAM,
	.name = "MML PQ Parameters",
	.type = V4L2_CTRL_COMPOUND_TYPES | 0x0300,
	.step = 1,
	.elem_size = sizeof(struct v4l2_pq_submit),
	.flags = V4L2_CTRL_FLAG_HAS_PAYLOAD,
};

static int m2m_ctrls_create(struct mml_m2m_ctx *ctx)
{
	v4l2_ctrl_handler_init(&ctx->ctrl_handler, MML_M2M_MAX_CTRLS);
	ctx->ctrls.hflip = v4l2_ctrl_new_std(&ctx->ctrl_handler,
		&mml_m2m_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	ctx->ctrls.vflip = v4l2_ctrl_new_std(&ctx->ctrl_handler,
		&mml_m2m_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);
	ctx->ctrls.rotate = v4l2_ctrl_new_std(&ctx->ctrl_handler,
		&mml_m2m_ctrl_ops, V4L2_CID_ROTATE, 0, 270, 90, 0);

	/* set customer config to handler */
	ctx->ctrls.secure = v4l2_ctrl_new_custom(&ctx->ctrl_handler, &m2m_sec_cfg, NULL);
	mml_msg("[m2m] set ctrl MML_M2M_CID_SECURE:%x ctrl type:%x, elem_size:%u",
		MML_M2M_CID_SECURE, m2m_sec_cfg.type, m2m_sec_cfg.elem_size);

	ctx->ctrls.pq = v4l2_ctrl_new_custom(&ctx->ctrl_handler, &m2m_pq_cfg, NULL);
	mml_msg("[m2m] set ctrl MML_M2M_CID_PQPARAM:%x ctrl type:%x, elem_size:%u",
		MML_M2M_CID_PQPARAM, m2m_pq_cfg.type, m2m_pq_cfg.elem_size);

	if (ctx->ctrl_handler.error) {
		int err = ctx->ctrl_handler.error;

		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		mml_err("Failed to register controls %d", err);
		return err;
	}
	return 0;
}

static int mml_m2m_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	strscpy(cap->driver, MML_M2M_MODULE_NAME, sizeof(cap->driver));
	strscpy(cap->card, MML_M2M_DEVICE_NAME, sizeof(cap->card));
	return 0;
}

static void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_NV12_HYFBC:
		descr = "Mediatek hybrid compressed NV12 Y/UV 4:2:0"; break;
	case V4L2_PIX_FMT_P010_HYFBC:
		descr = "Mediatek hybrid compressed P010 10-bit Y/UV 4:2:0"; break;
	case V4L2_PIX_FMT_RGB32_AFBC:
		descr = "Arm compressed 32-bit A/XRGB 8-8-8-8"; break;
	case V4L2_PIX_FMT_RGBA1010102_AFBC:
		descr = "Arm compressed 32-bit RGBA 10-10-10-2"; break;
	case V4L2_PIX_FMT_NV12_AFBC:
		descr = "Arm compressed NV12 Y/UV 4:2:0"; break;
	case V4L2_PIX_FMT_NV12_10B_AFBC:
		descr = "Arm compressed 10-bit NV12"; break;
	case V4L2_PIX_FMT_YVU420A:
		descr = "Planar YVU 4:2:0 16-pixel stride"; break;
	}

	if (descr)
		WARN_ON(strscpy(fmt->description, descr, sizeof(fmt->description)) < 0);
}

static int mml_m2m_enum_fmt_mplane(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	const struct mml_m2m_format *fmt;

	fmt = m2m_find_fmt_by_index(f->index, f->type);
	if (!fmt)
		return -EINVAL;

	f->pixelformat = fmt->pixelformat;
	f->flags = fmt->flags; /* compressed flag */
	if (V4L2_TYPE_IS_CAPTURE(f->type)) {
		f->flags |= V4L2_FMT_FLAG_CSC_COLORSPACE | V4L2_FMT_FLAG_CSC_XFER_FUNC;
		if (MML_FMT_IS_YUV(fmt->mml_color))
			f->flags |= V4L2_FMT_FLAG_CSC_YCBCR_ENC | V4L2_FMT_FLAG_CSC_QUANTIZATION;
	}

	v4l_fill_mtk_fmtdesc(f);
	return 0;
}

static int mml_m2m_g_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mml_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mml_m2m_frame *frame = ctx_get_frame(ctx, f->type);

	*f = frame->format;
	return 0;
}

/* Stride that is accepted by MML HW */
static u32 mml_fmt_get_stride(const struct mml_m2m_format *fmt,
			      u32 bytesperline, u32 plane)
{
	enum mml_color c = fmt->mml_color;
	u32 stride;

	stride = (bytesperline * MML_FMT_BITS_PER_PIXEL(c)) / fmt->row_depth[0];
	if (fmt->pixelformat == V4L2_PIX_FMT_YVU420A)
		stride = round_up(stride, 16);
	if (plane == 0)
		return stride;
	if (plane < MML_FMT_PLANE(c)) {
		if (MML_FMT_BLOCK(c))
			stride = stride / 2;
		return stride;
	}
	return 0;
}

/* Stride that is accepted by MML HW of format with contiguous planes */
static u32 mml_fmt_get_stride_contig(const struct mml_m2m_format *fmt,
				     u32 pix_stride, u32 plane)
{
	enum mml_color c = fmt->mml_color;
	u32 stride = pix_stride;

	if (plane == 0)
		return stride;
	if (plane < MML_FMT_PLANE(c)) {
		stride = stride >> MML_FMT_H_SUBSAMPLE(c);
		if (fmt->pixelformat == V4L2_PIX_FMT_YVU420A)
			stride = round_up(stride, 16);
		else if (MML_FMT_UV_COPLANE(c) && !MML_FMT_BLOCK(c))
			stride = stride * 2;
		return stride;
	}
	return 0;
}

/* Plane size that is accepted by MML HW */
static u32 mml_fmt_get_plane_size(const struct mml_m2m_format *fmt,
				  u32 stride, u32 height, u32 plane)
{
	enum mml_color c = fmt->mml_color;
	u32 bytesperline;

	bytesperline = (stride * fmt->row_depth[0]) / MML_FMT_BITS_PER_PIXEL(c);
	if (plane == 0)
		return bytesperline * height;
	if (plane < MML_FMT_PLANE(c)) {
		height = height >> MML_FMT_V_SUBSAMPLE(c);
		if (MML_FMT_BLOCK(c))
			bytesperline = bytesperline * 2;
		return bytesperline * height;
	}
	return 0;
}

static void m2m_set_format(struct mml_frame_data *data, struct mml_buffer *buf,
	const struct v4l2_pix_format_mplane *pix_mp,
	const struct mml_m2m_format *fmt)
{
	u32 i, stride;

	data->width = pix_mp->width;
	data->height = pix_mp->height;
	data->format = fmt->mml_color;
	data->plane_cnt = fmt->num_planes;
	data->profile = m2m_map_ycbcr_prof_mplane(pix_mp, fmt->mml_color);
	data->color.gamut = m2m_map_mml_gamut(pix_mp->colorspace);
	data->color.ycbcr_enc = m2m_map_mml_ycbcr_enc(pix_mp->ycbcr_enc);
	data->color.color_range = m2m_map_mml_color_range(pix_mp->quantization);
	data->color.gamma = m2m_map_mml_gamma(pix_mp->xfer_func);

	data->y_stride = 0;
	data->uv_stride = 0;
	memset(data->plane_offset, 0, sizeof(data->plane_offset));
	memset(buf->size, 0, sizeof(buf->size));

	buf->cnt = MML_FMT_PLANE(fmt->mml_color);
	for (i = 0; i < pix_mp->num_planes; i++) {
		stride = mml_fmt_get_stride(fmt,
			pix_mp->plane_fmt[i].bytesperline, i);
		if (i == 0)
			data->y_stride = stride;
		else if (i == 1)
			data->uv_stride = stride;
		if (i == 0 && MML_FMT_COMPRESS(fmt->mml_color))
			buf->size[i] = pix_mp->plane_fmt[i].sizeimage;
		else
			buf->size[i] = mml_fmt_get_plane_size(fmt,
				stride, pix_mp->height, i);
		data->plane_offset[i] = 0;
	}
	for (; i < buf->cnt; i++) {
		stride = mml_fmt_get_stride_contig(fmt,
			data->y_stride, i);
		if (i == 1)
			data->uv_stride = stride;
		buf->size[i] = mml_fmt_get_plane_size(fmt,
			stride, pix_mp->height, i);
		data->plane_offset[i] = data->plane_offset[i-1] +
					buf->size[i-1];
	}
}

static int mml_m2m_s_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mml_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mml_m2m_frame *frame = ctx_get_frame(ctx, f->type);
	struct mml_frame_data *mml_frame = ctx_get_submit_frame(ctx, f->type);
	struct mml_buffer *mml_buf = ctx_get_submit_buffer(ctx, f->type);
	struct v4l2_pix_format_mplane *pix_mp;
	const struct mml_m2m_format *fmt;
	struct vb2_queue *vq;
	struct mml_frame_dest *dest;
	const struct mml_frame_data *mml_frame_ref; /* should be output (source) */

	fmt = m2m_try_fmt_mplane(f, ctx);
	if (!fmt)
		return -EINVAL;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (vb2_is_busy(vq))
		return -EBUSY;

	frame->format = *f;
	frame->mml_fmt = fmt;

	pix_mp = &f->fmt.pix_mp;
	m2m_set_format(mml_frame, mml_buf, pix_mp, fmt);

	/* Check colorspace */
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		if (mml_frame->color.gamut >= MML_GAMUT_UNSUPPORTED)
			mml_frame->color.gamut =
				m2m_map_mml_gamut(V4L2_COLORSPACE_DEFAULT);
		if (mml_frame->color.ycbcr_enc >= MML_YCBCR_ENC_UNSUPPORTED)
			mml_frame->color.ycbcr_enc =
				m2m_map_mml_ycbcr_enc(V4L2_YCBCR_ENC_DEFAULT);
		if (mml_frame->color.color_range >= MML_COLOR_RANGE_UNSUPPORTED)
			mml_frame->color.color_range =
				m2m_map_mml_color_range(V4L2_QUANTIZATION_DEFAULT);
		if (mml_frame->color.gamma >= MML_GAMMA_UNSUPPORTED)
			mml_frame->color.gamma =
				m2m_map_mml_gamma(V4L2_XFER_FUNC_DEFAULT);
	} else {
		mml_frame_ref = ctx_get_submit_frame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (pix_mp->colorspace == V4L2_COLORSPACE_DEFAULT)
			mml_frame->color.gamut = mml_frame_ref->color.gamut;
		if (pix_mp->ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT)
			mml_frame->color.ycbcr_enc = mml_frame_ref->color.ycbcr_enc;
		if (pix_mp->quantization == V4L2_QUANTIZATION_DEFAULT)
			mml_frame->color.color_range = mml_frame_ref->color.color_range;
		if (pix_mp->xfer_func == V4L2_XFER_FUNC_DEFAULT)
			mml_frame->color.gamma = mml_frame_ref->color.gamma;
	}

	dest = ctx_get_submit_dest(ctx, 0);
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		dest->crop.r.left = 0;
		dest->crop.r.top = 0;
		dest->crop.r.width = pix_mp->width;
		dest->crop.r.height = pix_mp->height;
	} else {
		dest->compose.left = 0;
		dest->compose.top = 0;
		dest->compose.width = pix_mp->width;
		dest->compose.height = pix_mp->height;
	}

	return 0;
}

static int mml_m2m_try_fmt_mplane(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mml_m2m_ctx *ctx = fh_to_ctx(fh);

	if (!m2m_try_fmt_mplane(f, ctx))
		return -EINVAL;
	return 0;
}

static void mml_rect_to_v4l2_rect(struct mml_rect *src, struct v4l2_rect *dst)
{
	dst->left = src->left;
	dst->top = src->top;
	dst->width = src->width;
	dst->height = src->height;
}

static void v4l2_rect_to_mml_rect(struct v4l2_rect *src, struct mml_rect *dst)
{
	dst->left = src->left;
	dst->top = src->top;
	dst->width = src->width;
	dst->height = src->height;
}

static int mml_m2m_g_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	struct mml_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mml_m2m_frame *frame;
	struct mml_frame_dest *dest;
	bool valid = false;

	if (s->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		valid = m2m_target_is_crop(s->target);
	else if (s->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		valid = m2m_target_is_compose(s->target);

	if (!valid)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		dest = ctx_get_submit_dest(ctx, 0);
		mml_rect_to_v4l2_rect(&dest->crop.r, &s->r);
		return 0;
	case V4L2_SEL_TGT_COMPOSE:
		dest = ctx_get_submit_dest(ctx, 0);
		mml_rect_to_v4l2_rect(&dest->compose, &s->r);
		return 0;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		frame = ctx_get_frame(ctx, s->type);
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = frame->format.fmt.pix_mp.width;
		s->r.height = frame->format.fmt.pix_mp.height;
		return 0;
	}
	return -EINVAL;
}

static int mml_m2m_s_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	struct mml_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mml_m2m_frame *frame = ctx_get_frame(ctx, s->type);
	struct mml_frame_dest *dest;
	struct v4l2_rect r;
	bool valid = false;
	int ret;

	if (s->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		valid = (s->target == V4L2_SEL_TGT_CROP);
	else if (s->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		valid = (s->target == V4L2_SEL_TGT_COMPOSE);

	if (!valid) {
		mml_err("[m2m]%s invalid type:%u target:%u", __func__, s->type, s->target);
		return -EINVAL;
	}

	ret = m2m_try_crop(ctx, &r, s, frame);
	if (ret)
		return ret;
	dest = ctx_get_submit_dest(ctx, 0);

	if (m2m_target_is_crop(s->target)) {
		v4l2_rect_to_mml_rect(&r, &dest->crop.r);
	} else {
		v4l2_rect_to_mml_rect(&r, &dest->compose);
		dest->data.width = r.width;
		dest->data.height = r.height;
	}

	s->r = r;
	return 0;
}

static const struct v4l2_ioctl_ops mml_m2m_ioctl_ops __maybe_unused = {
	.vidioc_querycap		= mml_m2m_querycap,
	.vidioc_enum_fmt_vid_cap	= mml_m2m_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_out	= mml_m2m_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= mml_m2m_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= mml_m2m_g_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= mml_m2m_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= mml_m2m_s_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= mml_m2m_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mml_m2m_try_fmt_mplane,
	.vidioc_reqbufs			= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf			= v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	/* .vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf, */
	.vidioc_streamon		= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff		= v4l2_m2m_ioctl_streamoff,
	.vidioc_g_selection		= mml_m2m_g_selection,
	.vidioc_s_selection		= mml_m2m_s_selection,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static struct mml_m2m_ctx *m2m_ctx_create(struct mml_dev *mml)
{
	static const char * const threads[] = {
		"mml_m2m_done", "mml_destroy_m2m",
		NULL, NULL,
	};
	struct mml_m2m_ctx *ctx;
	struct mml_m2m_frame *frame;
	int ret;

	mml_msg("[m2m]%s on dev %p", __func__, mml);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ret = mml_ctx_init(&ctx->ctx, mml, threads);
	if (ret) {
		kfree(ctx);
		return ERR_PTR(ret);
	}

	ctx->ctx.task_ops = &m2m_task_ops;
	ctx->ctx.cfg_ops = &m2m_config_ops;
	ctx->limit = &mml_m2m_def_limit;
	frame = &ctx->output;
	frame->format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	frame->mml_fmt = m2m_try_fmt_mplane(&frame->format, ctx);
	frame = &ctx->capture;
	frame->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	frame->mml_fmt = m2m_try_fmt_mplane(&frame->format, ctx);
	INIT_LIST_HEAD(&ctx->params);
	mutex_init(&ctx->param_mutex);
	mutex_init(&ctx->q_mutex);
	kref_init(&ctx->ref);
	init_completion(&ctx->destroy);

	return ctx;
}

static int m2m_queue_init(void *priv,
			  struct vb2_queue *src_vq,
			  struct vb2_queue *dst_vq)
{
	struct mml_m2m_ctx *ctx = priv;
	struct device *mmu_dev = mml_get_mmu_dev(ctx->ctx.mml, ctx->param.secure);
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->ops = &mml_m2m_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->dev = mmu_dev;
	src_vq->lock = &ctx->q_mutex;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->ops = &mml_m2m_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->dev = mmu_dev;
	dst_vq->lock = &ctx->q_mutex;

	return vb2_queue_init(dst_vq);
}

static int mml_m2m_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct mml_dev *mml = video_get_drvdata(vdev);
	struct mml_v4l2_dev *v4l2_dev = mml_get_v4l2_dev(mml);
	struct mml_m2m_ctx *ctx;
	int ret;
	struct v4l2_format default_format = {};

	ctx = mml_dev_create_m2m_ctx(mml, m2m_ctx_create);
	if (IS_ERR(ctx)) {
		mml_err("[m2m] fail to create mml_m2m_ctx");
		ret = PTR_ERR(ctx);
		goto err_ret;
	}

	ctx->m2m_ctx = v4l2_m2m_ctx_init(v4l2_dev->m2m_dev, ctx, m2m_queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		mml_err("Failed to initialize m2m context");
		ret = PTR_ERR(ctx->m2m_ctx);
		goto err_free_ctx;
	}

	/* Use separate control handler per file handle */
	ret = m2m_ctrls_create(ctx);
	if (ret)
		goto err_release_m2m_ctx;
	ret = v4l2_ctrl_handler_setup(&ctx->ctrl_handler);
	if (ret)
		goto err_free_handler;

	if (mutex_lock_interruptible(&v4l2_dev->m2m_mutex)) {
		ret = -ERESTARTSYS;
		goto err_free_handler;
	}

	v4l2_fh_init(&ctx->fh, vdev);
	file->private_data = &ctx->fh;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	v4l2_fh_add(&ctx->fh);

	mutex_unlock(&v4l2_dev->m2m_mutex);

	/* Default format */
	default_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	default_format.fmt.pix_mp.width = 32;
	default_format.fmt.pix_mp.height = 32;
	default_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
	mml_m2m_s_fmt_mplane(file, &ctx->fh, &default_format);
	default_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	mml_m2m_s_fmt_mplane(file, &ctx->fh, &default_format);

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
err_release_m2m_ctx:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
err_free_ctx:
	mml_ctx_deinit(&ctx->ctx);
	kfree(ctx);
err_ret:
	return ret;
}

static void m2m_ctx_destroy(struct mml_m2m_ctx *mctx)
{
	struct mml_ctx *ctx = &mctx->ctx;
	struct mml_m2m_param *param, *tmp;
	u32 i;

	mml_msg("[m2m]%s on ctx %p", __func__, mctx);
	v4l2_ctrl_handler_free(&mctx->ctrl_handler);
	v4l2_m2m_ctx_release(mctx->m2m_ctx);

	mutex_lock(&mctx->param_mutex);
	list_for_each_entry_safe(param, tmp, &mctx->params, entry) {
		list_del(&param->entry);
		kfree(param);
	}
	mutex_unlock(&mctx->param_mutex);

	mml_ctx_deinit(ctx);
	for (i = 0; i < ARRAY_SIZE(ctx->tile_cache); i++)
		if (ctx->tile_cache[i].tiles)
			vfree(ctx->tile_cache[i].tiles);
	kfree(mctx);
}

static int mml_m2m_release(struct file *file)
{
	struct v4l2_fh *fh = file->private_data;

	if (fh) {
		struct mml_m2m_ctx *ctx = fh_to_ctx(fh);
		struct mml_v4l2_dev *v4l2_dev = mml_get_v4l2_dev(video_drvdata(file));

		mutex_lock(&v4l2_dev->m2m_mutex);
		file->private_data = NULL;
		v4l2_fh_del(&ctx->fh);
		v4l2_fh_exit(&ctx->fh);
		mutex_unlock(&v4l2_dev->m2m_mutex);

		kref_put(&ctx->ref, m2m_ctx_complete);
		wait_for_completion(&ctx->destroy);
		m2m_ctx_destroy(ctx);
	}
	return 0;
}

static const struct v4l2_file_operations mml_m2m_fops __maybe_unused = {
	.owner		= THIS_MODULE,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
	.open		= mml_m2m_open,
	.release	= mml_m2m_release,
};

static int m2m_set_orientation(struct mml_frame_dest *dest,
			       s32 rotation, bool hflip, bool vflip)
{
	s32 rotate = rotation;
	u8 flip = 0;

	if (hflip)
		flip ^= 1;
	if (vflip) {
		/*
		 * A vertical flip is equivalent to
		 * a 180-degree rotation with a horizontal flip
		 */
		rotate += 180;
		flip ^= 1;
	}
	rotate = rotate % 360;

	/* mapping to MML_ROT_X */
	switch (rotate) {
	case 0:
		dest->rotate = MML_ROT_0;
		break;
	case 90:
		dest->rotate = MML_ROT_90;
		break;
	case 180:
		dest->rotate = MML_ROT_180;
		break;
	case 270:
		dest->rotate = MML_ROT_270;
		break;
	default:
		mml_err("[m2m]%s not reasonable rotation %d", __func__, rotation);
		return -ERANGE;
	}

	dest->flip = flip;
	return 0;
}

static s32 m2m_set_submit(struct mml_m2m_ctx *mctx, struct mml_submit *submit)
{
	int ret = 0;
	struct device *mmu_dev;
	struct vb2_queue *src_vq, *dst_vq;
	struct mml_m2m_param *param;

	mutex_lock(&mctx->param_mutex);
	if (list_empty(&mctx->params)) {
		mml_err("No control parameters available");
		ret = -EINVAL;
		goto unlock_param;
	}

	param = list_first_entry(&mctx->params, struct mml_m2m_param, entry);

	ret = m2m_set_orientation(&submit->info.dest[0],
		param->rotation, param->hflip, param->vflip);
	if (ret < 0)
		goto unlock_param;

	mmu_dev = mml_get_mmu_dev(mctx->ctx.mml, param->secure);
	submit->info.src.secure = param->secure;
	src_vq = v4l2_m2m_get_vq(mctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	src_vq->dev = mmu_dev;

	submit->info.dest[0].pq_config = param->pq_submit.pq_config;
	submit->info.dest[0].data.secure = param->secure;
	dst_vq = v4l2_m2m_get_vq(mctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	dst_vq->dev = mmu_dev;
	submit->info.dest_cnt = 1;
	submit->info.mode = MML_MODE_MML_DECOUPLE2;

	submit->buffer.dest_cnt = 1;
	submit->pq_param[0] = &param->pq_submit.pq_param;

unlock_param:
	mutex_unlock(&mctx->param_mutex);
	return ret;
}

static s32 m2m_frame_buf_to_task_buf(struct mml_ctx *ctx,
	struct mml_file_buf *fbuf, struct mml_buffer *user_buf,
	struct vb2_v4l2_buffer *vbuf, const char *name, bool secure)
{
	struct vb2_buffer *vb = &vbuf->vb2_buf;
	void *dbufs[MML_MAX_PLANES];
	struct device *mmu_dev = mml_get_mmu_dev(ctx->mml, secure);
	u8 i;
	s32 ret = 0;

	if (unlikely(!mmu_dev)) {
		mml_err("%s mmu_dev is null", __func__);
		return -EFAULT;
	}

	if (vb->vb2_queue->memory == VB2_MEMORY_MMAP) {
		/* use vb2 buffer iova */
		for (i = 0; i < vb->num_planes; i++) {
			fbuf->dma[i].dmabuf = vb->planes[i].dbuf;
			fbuf->dma[i].iova = vb2_dma_contig_plane_dma_addr(vb, i);
		}

		/* also copy size for later use */
		for (i = 0; i < user_buf->cnt; i++) {
			fbuf->size[i] = user_buf->size[i];
			if (!fbuf->dma[i].iova) {
				/* no iova but need this plane, use previous iova */
				if (i)
					fbuf->dma[i].iova = fbuf->dma[i-1].iova;
			}
		}
		fbuf->cnt = user_buf->cnt;

		return 0;
	}

	for (i = 0; i < vb->num_planes; i++)
		dbufs[i] = vb->planes[i].dbuf;
		/* fbuf->dma[i].iova = vb2_dma_contig_plane_dma_addr(vb, i); use vb2 addr */
	mml_buf_get(fbuf, dbufs, vb->num_planes, name);

	/* also copy size for later use */
	for (i = 0; i < user_buf->cnt; i++)
		fbuf->size[i] = user_buf->size[i];
	fbuf->cnt = user_buf->cnt;
	fbuf->flush = !(vbuf->flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
	fbuf->invalid = !(vbuf->flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);

	if (fbuf->dma[0].dmabuf) {
		mml_mmp(buf_map, MMPROFILE_FLAG_START,
			atomic_read(&ctx->job_serial), 0);

		/* get iova */
		ret = mml_buf_iova_get(mmu_dev, fbuf);
		if (ret < 0)
			mml_err("%s iova fail %d", __func__, ret);

		mml_mmp(buf_map, MMPROFILE_FLAG_END,
			atomic_read(&ctx->job_serial),
			(unsigned long)fbuf->dma[0].iova);

		mml_msg("%s %s dmabuf %p iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
			__func__, name, fbuf->dma[0].dmabuf,
			fbuf->dma[0].iova, fbuf->size[0],
			fbuf->dma[1].iova, fbuf->size[1],
			fbuf->dma[2].iova, fbuf->size[2]);
	}

	return ret;
}

static void mml_m2m_device_run(void *priv)
{
	struct mml_m2m_ctx *mctx = priv;
	struct mml_submit *submit = &mctx->submit;
	struct mml_ctx *ctx = &mctx->ctx;
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;
	s32 result;
	u32 i;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state vb_state = VB2_BUF_STATE_ERROR;

	mml_trace_begin("%s", __func__);

	/* hold mctx to avoid release from v4l2 before call submit_done */
	kref_get(&mctx->ref);

	result = m2m_set_submit(mctx, submit);
	if (result < 0)
		goto err_buf_exit;

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

	/* plane offset set on m2m_set_format */

	if (mtk_mml_msg)
		dump_m2m_ctx(mctx);

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (cfg) {
		mml_msg("[m2m]%s reuse config %p", __func__, cfg);
		task = task_get_idle(cfg);
		if (task) {
			/* reuse case change state IDLE to REUSE */
			task->state = MML_TASK_REUSE;
			init_completion(&task->pkts[0]->cmplt);
			if (task->pkts[1])
				init_completion(&task->pkts[1]->cmplt);
			mml_msg("[m2m]reuse task %p pkt %p %p",
				task, task->pkts[0], task->pkts[1]);
		} else {
			task = mml_core_create_task(atomic_read(&ctx->job_serial));
			if (IS_ERR(task)) {
				result = PTR_ERR(task);
				mml_err("%s create task for reuse frame fail", __func__);
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

		mml_msg("[m2m]%s create config %p", __func__, cfg);
		if (IS_ERR(cfg)) {
			result = PTR_ERR(cfg);
			mml_err("%s create frame config fail", __func__);
			goto err_unlock_exit;
		}
		task = mml_core_create_task(atomic_read(&ctx->job_serial));
		if (IS_ERR(task)) {
			list_del_init(&cfg->entry);
			frame_config_destroy(cfg);
			result = PTR_ERR(task);
			task = NULL;
			mml_err("%s create task fail", __func__);
			goto err_unlock_exit;
		}
		task->config = cfg;
		/* add more count for new task create */
		cfg->cfg_ops->get(cfg);
	}

	/* make sure id unique and cached last */
	task->job.jobid = atomic_inc_return(&ctx->job_serial);
	cfg->last_jobid = task->job.jobid;
	list_add_tail(&task->entry, &cfg->await_tasks);
	cfg->await_task_cnt++;
	mml_msg("[m2m]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;

	src_buf = v4l2_m2m_next_src_buf(mctx->m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(mctx->m2m_ctx);
	if (!src_buf || !dst_buf) {
		mml_err("[m2m]%s get next buf fail src %p dst %p", __func__,
			src_buf, dst_buf);
		goto err_buf_exit;
	}

	task->adaptor_type = MML_ADAPTOR_M2M;
	/* update endTime here */
	task->end_time = ns_to_timespec64(src_buf->vb2_buf.timestamp);
	/* give default time if empty */
	frame_check_end_time(&task->end_time);
	mml_msg("[m2m] mml job %u endTime: %2u.%03llu",
		task->job.jobid,
		(u32)task->end_time.tv_sec, div_u64(task->end_time.tv_nsec, 1000000));

	result = m2m_frame_buf_to_task_buf(ctx, &task->buf.src,
		&submit->buffer.src, src_buf,
		"mml_m2m_rdma", submit->info.src.secure);
	if (result) {
		mml_err("[m2m]%s get src dma buf fail", __func__);
		goto err_buf_exit;
	}

	task->buf.dest_cnt = submit->buffer.dest_cnt;
	result = m2m_frame_buf_to_task_buf(ctx, &task->buf.dest[0],
		&submit->buffer.dest[0], dst_buf,
		"mml_m2m_wrot", submit->info.dest[0].data.secure);
	if (result) {
		mml_err("[m2m]%s get dest dma buf fail", __func__);
		goto err_buf_exit;
	}

	/* no fence for m2m task */
	task->job.fence = -1;
	mml_msg("[m2m]mml job %u task %p config %p mode %hhu",
		task->job.jobid, task, cfg, cfg->info.mode);

	/* copy pq parameters */
	for (i = 0; i < submit->buffer.dest_cnt && submit->pq_param[i]; i++)
		task->pq_param[i] = *submit->pq_param[i];

	/* wake lock */
	mml_lock_wake_lock(task->config->mml, true);

	/* kick vdisp power on */
	mml_pw_kick_idle(task->config->mml);

	/* hold mctx to avoid release from v4l2 before call frame_done */
	kref_get(&mctx->ref);
	/* kref_put mctx->ref in m2m_task_frame_done */

	/* config to core */
	mml_core_submit_task(cfg, task);

	if (cfg->err) {
		result = -EINVAL;
		goto err_buf_exit;
	}

	/* note that m2m task is not queued to another thread so we can put mctx here */
	kref_put(&mctx->ref, m2m_ctx_complete);

	mml_trace_end();
	return;

err_unlock_exit:
	mutex_unlock(&ctx->config_mutex);
err_buf_exit:
	mml_trace_end();
	mml_log("%s fail result %d task %p", __func__, result, task);
	if (task) {
		bool is_init_state = task->state == MML_TASK_INITIAL;

		mutex_lock(&ctx->config_mutex);

		list_del_init(&task->entry);
		cfg->await_task_cnt--;

		if (is_init_state) {
			mml_log("dec config %p and del", cfg);

			list_del_init(&cfg->entry);
			ctx->config_cnt--;
		} else
			mml_log("dec config %p", cfg);

		mutex_unlock(&ctx->config_mutex);
		kref_put(&task->ref, task_move_to_destroy);

		if (is_init_state)
			cfg->cfg_ops->put(cfg);
	}
	mml_m2m_process_done(mctx, vb_state);

	kref_put(&mctx->ref, m2m_ctx_complete);
}

static const struct v4l2_m2m_ops mml_m2m_ops __maybe_unused = {
	.device_run	= mml_m2m_device_run,
};

#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
static int mml_m2m_device_register(struct device *dev, struct mml_v4l2_dev *v4l2_dev)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct video_device *vdev;
	int ret;

	vdev = video_device_alloc();
	if (!vdev) {
		dev_err(dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_ret;
	}
	vdev->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	vdev->fops = &mml_m2m_fops;
	vdev->ioctl_ops = &mml_m2m_ioctl_ops;
	vdev->release = video_device_release;
	vdev->lock = &v4l2_dev->m2m_mutex;
	vdev->vfl_dir = VFL_DIR_M2M;
	vdev->v4l2_dev = &v4l2_dev->v4l2_dev;
	if (snprintf(vdev->name, sizeof(vdev->name), "%s:m2m", MML_M2M_MODULE_NAME) < 0) {
		dev_err(dev, "Failed to get the name of video device\n");
		ret = -EINVAL;
		goto err_release;
	}
	video_set_drvdata(vdev, mml);
	v4l2_dev->m2m_vdev = vdev;

	v4l2_dev->m2m_dev = v4l2_m2m_init(&mml_m2m_ops);
	if (IS_ERR(v4l2_dev->m2m_dev)) {
		dev_err(dev, "Failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(v4l2_dev->m2m_dev);
		goto err_release;
	}

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		dev_err(dev, "Failed to register video device\n");
		goto err_m2m;
	}

	v4l2_info(&v4l2_dev->v4l2_dev, "Driver registered as /dev/video%d",
		  vdev->num);
	return 0;

err_m2m:
	v4l2_m2m_release(v4l2_dev->m2m_dev);
err_release:
	video_device_release(vdev);
err_ret:
	return ret;
}

static void mml_m2m_device_unregister(struct mml_v4l2_dev *v4l2_dev)
{
	video_unregister_device(v4l2_dev->m2m_vdev);
	v4l2_m2m_release(v4l2_dev->m2m_dev);
}
#endif

struct mml_v4l2_dev *mml_v4l2_dev_create(struct device *dev)
{
#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
	struct mml_v4l2_dev *v4l2_dev;
	int ret;

	v4l2_dev = devm_kzalloc(dev, sizeof(*v4l2_dev), GFP_KERNEL);
	if (!v4l2_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&v4l2_dev->m2m_mutex);

	ret = v4l2_device_register(dev, &v4l2_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		goto err_free;
	}

	ret = mml_m2m_device_register(dev, v4l2_dev);
	if (ret) {
		v4l2_err(&v4l2_dev->v4l2_dev, "Failed to register m2m device\n");
		goto err_unregister;
	}

	return v4l2_dev;

err_unregister:
	v4l2_device_unregister(&v4l2_dev->v4l2_dev);
err_free:
	devm_kfree(dev, v4l2_dev);
	return ERR_PTR(ret);
#else
	return ERR_PTR(-EFAULT);
#endif
}

void mml_v4l2_dev_destroy(struct device *dev, struct mml_v4l2_dev *v4l2_dev)
{
	if (IS_ERR_OR_NULL(v4l2_dev))
		return;

#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
	mml_m2m_device_unregister(v4l2_dev);
	v4l2_device_unregister(&v4l2_dev->v4l2_dev);
#endif
	devm_kfree(dev, v4l2_dev);
}

