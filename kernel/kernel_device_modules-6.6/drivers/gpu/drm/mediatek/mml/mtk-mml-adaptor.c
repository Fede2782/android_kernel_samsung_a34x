// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Iris-sc Yang <iris-sc.yang@mediatek.com>
 */

#include <linux/sched.h>
#include <linux/time64.h>
#include <uapi/linux/sched/types.h>

#include "mtk-mml-adaptor.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-pq-core.h"

#define MML_DEFAULT_END_NS	15000000
#define MML_DURATION_RESERVE	2000
#define MML_MIN_FPS		120

/* set to 0 to disable reuse config */
int mml_reuse = 1;
module_param(mml_reuse, int, 0644);

static bool check_frame_wo_change(struct mml_submit *submit,
	struct mml_frame_config *cfg)
{
	/* Only when both of frame info and dl_out are not changed, return true,
	 * else return false
	 */
	return !memcmp(&submit->info, &cfg->info, sizeof(submit->info)) &&
		!memcmp(submit->dl_out, cfg->dl_out, sizeof(submit->dl_out));
}

struct mml_frame_config *frame_config_find_reuse(struct mml_ctx *ctx,
	struct mml_submit *submit)
{
	struct mml_frame_config *cfg;
	u32 idx = 0, mode = MML_MODE_UNKNOWN;

	if (!mml_reuse)
		return NULL;

	mml_trace_ex_begin("mml_%s", __func__);

	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (!idx)
			mode = cfg->info.mode;

		if (submit->update && cfg->last_jobid == submit->job->jobid)
			goto done;

		if (check_frame_wo_change(submit, cfg) && !cfg->err)
			goto done;

		idx++;
	}

	/* not found, give return value to NULL */
	cfg = NULL;

done:
	if (cfg && idx) {
		if (mode != cfg->info.mode)
			mml_log("[adpt]mode change to %hhu", cfg->info.mode);
		list_rotate_to_front(&cfg->entry, &ctx->configs);
	}

	mml_trace_ex_end();
	return cfg;
}

struct mml_task *task_get_idle(struct mml_frame_config *cfg)
{
	struct mml_task *task = list_first_entry_or_null(
		&cfg->done_tasks, struct mml_task, entry);

	if (task) {
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		memset(&task->buf, 0, sizeof(task->buf));
		if (cfg->info.dest[0].pq_config.en)
			mml_pq_reset_hist_status(task);
	}
	return task;
}

void task_move_to_destroy(struct kref *kref)
{
	struct mml_task *task = container_of(kref, struct mml_task, ref);

	if (task->config) {
		struct mml_frame_config *cfg = task->config;

		cfg->cfg_ops->put(cfg);
		task->config = NULL;
	}

	mml_core_destroy_task(task);
}

void frame_config_destroy(struct mml_frame_config *cfg)
{
	struct mml_task *task, *tmp;

	mml_msg("[adpt]%s frame config %p task cnt (%u %u %hhu)",
		__func__, cfg, cfg->await_task_cnt, cfg->run_task_cnt, cfg->done_task_cnt);

	if (WARN_ON(!list_empty(&cfg->await_tasks))) {
		mml_err("[adpt]still waiting tasks in wq during destroy config %p", cfg);
		list_for_each_entry_safe(task, tmp, &cfg->await_tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("[adpt]busy task:%p", task);
			kref_put(&task->ref, task_move_to_destroy);
		}
	}

	if (WARN_ON(!list_empty(&cfg->tasks))) {
		mml_err("[adpt]still busy tasks during destroy config %p", cfg);
		list_for_each_entry_safe(task, tmp, &cfg->tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("[adpt]busy task:%p", task);
			kref_put(&task->ref, task_move_to_destroy);
		}
	}

	list_for_each_entry_safe(task, tmp, &cfg->done_tasks, entry) {
		list_del_init(&task->entry);
		kref_put(&task->ref, task_move_to_destroy);
	}

	cfg->cfg_ops->put(cfg);
}

static void frame_config_release(struct kref *kref)
{
	struct mml_frame_config *cfg = container_of(kref,
		struct mml_frame_config, ref);

	mml_core_deinit_config(cfg);
	cfg->cfg_ops->free(cfg);
}

static void frame_config_destroy_work(struct work_struct *work)
{
	struct mml_frame_config *cfg = container_of(work,
		struct mml_frame_config, work_destroy);

	frame_config_destroy(cfg);
}

void frame_config_queue_destroy(struct mml_frame_config *cfg)
{
	queue_work(cfg->ctx->wq_destroy, &cfg->work_destroy);
}

void frame_config_init(struct mml_frame_config *cfg,
	struct mml_ctx *ctx,
	struct mml_submit *submit)
{
	struct timespec64 curr_time;
	struct timespec64 end_time = {
		.tv_sec = submit->end.sec,
		.tv_nsec = submit->end.nsec,
	};

	mml_core_init_config(cfg);

	list_add(&cfg->entry, &ctx->configs);
	ctx->config_cnt++;

	cfg->job_id = atomic_inc_return(&ctx->config_serial);
	cfg->info = submit->info;
	cfg->disp_dual = ctx->disp_dual;
	cfg->disp_vdo = ctx->disp_vdo;
	cfg->ctx = ctx;
	cfg->mml = ctx->mml;
	cfg->task_ops = ctx->task_ops;
	cfg->cfg_ops = ctx->cfg_ops;
	cfg->ctx_kt_done = ctx->kt_done;
	memcpy(cfg->dl_out, submit->dl_out, sizeof(cfg->dl_out));
	INIT_WORK(&cfg->work_destroy, frame_config_destroy_work);
	kref_init(&cfg->ref);

	ktime_get_real_ts64(&curr_time);
	cfg->duration = (u32)mml_core_time_dur_us(&end_time, &curr_time);
	cfg->duration = cfg->duration > MML_DURATION_RESERVE ?
		cfg->duration - MML_DURATION_RESERVE : 1;
	cfg->fps = max_t(u32, 1000000 / cfg->duration, 120);
}

struct mml_frame_config *frame_config_create(
	struct mml_ctx *ctx,
	struct mml_submit *submit)
{
	struct mml_frame_config *cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);

	if (!cfg)
		return ERR_PTR(-ENOMEM);
	frame_config_init(cfg, ctx, submit);
	return cfg;
}

s32 frame_buf_to_task_buf(struct mml_file_buf *fbuf,
			  struct mml_buffer *user_buf,
			  const char *name)
{
	u8 i;
	s32 ret = 0;

	if (user_buf->use_dma)
		mml_buf_get(fbuf, user_buf->dmabuf, user_buf->cnt, name);
	else
		ret = mml_buf_get_fd(fbuf, user_buf->fd, user_buf->cnt, name);

	/* also copy size for later use */
	for (i = 0; i < user_buf->cnt; i++)
		fbuf->size[i] = user_buf->size[i];
	fbuf->cnt = user_buf->cnt;
	fbuf->flush = user_buf->flush;
	fbuf->invalid = user_buf->invalid;

	return ret;
}

void task_move_to_running(struct mml_task *task)
{
	/* Must lock ctx->config_mutex before call
	 * For INITIAL / DUPLICATE state move to running,
	 * otherwise do nothing.
	 */
	if (task->state != MML_TASK_INITIAL &&
		task->state != MML_TASK_DUPLICATE &&
		task->state != MML_TASK_REUSE) {
		mml_msg("[adpt]%s task %p state conflict %u",
			__func__, task, task->state);
		return;
	}

	if (list_empty(&task->entry)) {
		mml_err("[adpt]%s task %p already leave config",
			__func__, task);
		return;
	}

	list_del_init(&task->entry);
	task->config->await_task_cnt--;
	task->state = MML_TASK_RUNNING;
	list_add_tail(&task->entry, &task->config->tasks);
	task->config->run_task_cnt++;

	mml_msg("[adpt]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);
}

void task_move_to_idle(struct mml_task *task)
{
	/* Must lock ctx->config_mutex before call */
	if (task->state == MML_TASK_INITIAL ||
		task->state == MML_TASK_DUPLICATE ||
		task->state == MML_TASK_REUSE) {
		/* move out from awat list */
		task->config->await_task_cnt--;
	} else if (task->state == MML_TASK_RUNNING) {
		/* move out from tasks list */
		task->config->run_task_cnt--;
	} else {
		/* unknown state transition */
		mml_err("[adpt]%s state conflict %d", __func__, task->state);
	}

	list_del_init(&task->entry);
	task->state = MML_TASK_IDLE;

	if (unlikely(task->err)) {
		mml_err("[adpt]%s task cnt (%u %u %u) before error put",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);

		/* do not reuse this one, it error before */
		kref_put(&task->ref, task_move_to_destroy);
	} else {
		list_add_tail(&task->entry, &task->config->done_tasks);
		task->config->done_task_cnt++;

		mml_msg("[adpt]%s task cnt (%u %u %hhu)",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);
	}
}

void task_queue(struct mml_task *task, u32 pipe)
{
	kthread_queue_work(task->ctx->kt_config[pipe], &task->work_config[pipe]);
}

void task_submit_done(struct mml_task *task)
{
	struct mml_ctx *ctx = task->ctx;

	mml_msg("[adpt]%s task %p state %u", __func__, task, task->state);
	if (ctx->config_cb) {
		ctx->config_cb(task, task->cb_param);
	} else {
		mml_mmp(submit_cb, MMPROFILE_FLAG_PULSE, task->job.jobid, 0);
		if (ctx->submit_cb)
			ctx->submit_cb(task->cb_param);
	}

	mutex_lock(&ctx->config_mutex);
	task_move_to_running(task);
	kref_put(&task->ref, task_move_to_destroy);
	mutex_unlock(&ctx->config_mutex);
}

void task_buf_put(struct mml_task *task)
{
	u8 i;

	mml_trace_ex_begin("%s_putbuf", __func__);
	for (i = 0; i < task->buf.dest_cnt; i++) {
		mml_msg("[adpt]release dest %hhu iova %#011llx",
			i, task->buf.dest[i].dma[0].iova);
		mml_buf_put(&task->buf.dest[i]);
		if (task->buf.dest[i].fence)
			dma_fence_put(task->buf.dest[i].fence);
	}
	mml_msg("[adpt]release src iova %#011llx",
		task->buf.src.dma[0].iova);
	mml_buf_put(&task->buf.src);
	if (task->buf.src.fence)
		dma_fence_put(task->buf.src.fence);
	if (task->config->info.dest[0].pq_config.en_region_pq) {
		mml_msg("[adpt]release seg_map iova %#011llx",
			task->buf.seg_map.dma[0].iova);
		mml_buf_put(&task->buf.seg_map);
		if (task->buf.seg_map.fence)
			dma_fence_put(task->buf.seg_map.fence);
	}
	mml_trace_ex_end();
}

void task_state_dec(struct mml_frame_config *cfg, struct mml_task *task,
	const char *api)
{
	if (list_empty(&task->entry))
		return;

	list_del_init(&task->entry);

	switch (task->state) {
	case MML_TASK_INITIAL:
	case MML_TASK_DUPLICATE:
	case MML_TASK_REUSE:
		cfg->await_task_cnt--;
		break;
	case MML_TASK_RUNNING:
		cfg->run_task_cnt--;
		break;
	case MML_TASK_IDLE:
		cfg->done_task_cnt--;
		break;
	default:
		mml_err("%s conflict state %u", api, task->state);
	}
}

void dump_pq_en(u32 idx, struct mml_pq_param *pq_param,
	struct mml_pq_config *pq_config)
{
	u32 pqen = 0;

	memcpy(&pqen, pq_config, min(sizeof(*pq_config), sizeof(pqen)));

	if (pq_param)
		mml_msg("[adpt]PQ %u config %#x param en %u %s",
			idx, pqen, pq_param->enable,
			mml_pq_disable ? "FORCE DISABLE" : "");
	else
		mml_msg("[adpt]PQ %u config %#x param NULL %s",
			idx, pqen,
			mml_pq_disable ? "FORCE DISABLE" : "");
}

void frame_calc_plane_offset(struct mml_frame_data *data,
	struct mml_buffer *buf)
{
	u32 i;

	data->plane_offset[0] = 0;
	for (i = 1; i < MML_FMT_PLANE(data->format); i++) {
		if (buf->fd[i] != buf->fd[i-1] && buf->fd[i] >= 0) {
			/* different buffer for different plane, set to begin */
			data->plane_offset[i] = 0;
			continue;
		}
		data->plane_offset[i] = data->plane_offset[i-1] +
					buf->size[i-1];
	}
}

s32 task_dup(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_ctx *ctx = task->ctx;
	struct mml_task *src;
	int ret = 0;

	if (!cfg->nocmd &&		/* drm, m2m */
	    unlikely(task->pkts[pipe])) {
		mml_err("[adpt]%s task %p pipe %u already has pkt before dup",
			__func__, task, pipe);
		return -EINVAL;
	}

	mutex_lock(&ctx->config_mutex);

	mml_msg("[adpt]%s task cnt (%u %u %u) task %p pipe %u config %p",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		task, pipe, task->config);

	/* check if available done task first */
	src = list_first_entry_or_null(&cfg->done_tasks, struct mml_task,
				       entry);
	if (src) {
		if (cfg->nocmd)		/* dle */
			goto dup_pq;
		if (src->pkts[pipe])	/* drm, m2m */
			goto dup_command;
	}

	/* check running tasks, have to check it is valid task */
	list_for_each_entry(src, &cfg->tasks, entry) {
		if (cfg->nocmd)		/* dle */
			goto dup_pq;
		if (!src->pkts[0] ||	/* drm, m2m */
		    (src->config->dual && !src->pkts[1])) {
			mml_err("[adpt]%s error running task %p not able to copy",
				__func__, src);
			continue;
		}
		goto dup_command;
	}

	list_for_each_entry_reverse(src, &cfg->await_tasks, entry) {
		/* the first one should be current task, skip it */
		if (src == task) {
			mml_msg("[adpt]%s await task %p pkt %p",
				__func__, src, src->pkts[pipe]);
			continue;
		}

		if (cfg->nocmd)		/* dle */
			goto dup_pq;
		if (!src->pkts[0] ||	/* drm, m2m */
		    (src->config->dual && !src->pkts[1])) {
			mml_err("[adpt]%s error await task %p not able to copy",
				__func__, src);
			continue;
		}
		goto dup_command;
	}

	/* this config may have issue, do not reuse anymore */
	cfg->err = true;

	mutex_unlock(&ctx->config_mutex);
	return -EBUSY;

dup_command:
	/* drm, m2m */
	task->pkts[pipe] = cmdq_pkt_create(cfg->path[pipe]->clt);
	cmdq_pkt_copy(task->pkts[pipe], src->pkts[pipe]);
	task->pkts[pipe]->user_data = (void *)task;

	task->reuse[pipe].labels = vzalloc(cfg->cache[pipe].label_cnt *
		sizeof(*task->reuse[pipe].labels));
	task->reuse[pipe].label_mods = kcalloc(cfg->cache[pipe].label_cnt,
		sizeof(*task->reuse[pipe].label_mods), GFP_KERNEL);
	task->reuse[pipe].label_check = kcalloc(cfg->cache[pipe].label_cnt,
		sizeof(*task->reuse[pipe].label_check), GFP_KERNEL);
	memcpy(&task->backup_crc_rdma, &src->backup_crc_rdma, sizeof(task->backup_crc_rdma));
	memcpy(&task->backup_crc_wdma, &src->backup_crc_wdma, sizeof(task->backup_crc_wdma));
	task->perf_prete = src->perf_prete;
	task->perf_dispready = src->perf_dispready;
	task->perf_sof = src->perf_sof;
	if (task->reuse[pipe].labels && task->reuse[pipe].label_mods &&
		task->reuse[pipe].label_check) {
		memcpy(task->reuse[pipe].labels, src->reuse[pipe].labels,
			sizeof(*task->reuse[pipe].labels) * cfg->cache[pipe].label_cnt);
		memcpy(task->reuse[pipe].label_mods, src->reuse[pipe].label_mods,
			sizeof(*task->reuse[pipe].label_mods) * cfg->cache[pipe].label_cnt);
		task->reuse[pipe].label_idx = src->reuse[pipe].label_idx;
		cmdq_reuse_refresh(task->pkts[pipe], task->reuse[pipe].labels,
			task->reuse[pipe].label_idx);
		task->dpc_reuse_sys = src->dpc_reuse_sys;
		task->dpc_reuse_mutex = src->dpc_reuse_mutex;
		cmdq_reuse_refresh(task->pkts[pipe], &task->dpc_reuse_sys.jump_to_begin, 3);
		cmdq_reuse_refresh(task->pkts[pipe], &task->dpc_reuse_mutex.jump_to_begin, 3);
	} else {
		mml_err("[adpt]copy reuse labels fail");
		ret = -ENOMEM;
	}
dup_pq:
	/* dle TODO: copy pq_task results */
	/* mml_pq_dup_task(task->pq_task, src->pq_task) */
	mutex_unlock(&ctx->config_mutex);
	return ret;
}

struct mml_tile_cache *task_get_tile_cache(struct mml_task *task, u32 pipe)
{
	return &task->ctx->tile_cache[pipe];
}

void ctx_kt_setsched(struct mml_ctx *ctx)
{
	struct sched_param kt_param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret[3] = {0};

	if (ctx->kt_priority)
		return;

	ret[0] = sched_setscheduler(ctx->kt_done->task, SCHED_FIFO, &kt_param);
	if (ctx->kt_config[0])
		ret[1] = sched_setscheduler(ctx->kt_config[0]->task, SCHED_FIFO, &kt_param);
	if (ctx->kt_config[1])
		ret[2] = sched_setscheduler(ctx->kt_config[1]->task, SCHED_FIFO, &kt_param);
	mml_log("[adpt]%s set kt done priority %d ret %d %d %d",
		__func__, kt_param.sched_priority, ret[0], ret[1], ret[2]);
	ctx->kt_priority = true;
}

void frame_config_get(struct mml_frame_config *cfg)
{
	kref_get(&cfg->ref);
}

void frame_config_put(struct mml_frame_config *cfg)
{
	kref_put(&cfg->ref, frame_config_release);
}

void frame_config_free(struct mml_frame_config *cfg)
{
	kfree(cfg);
}

int mml_ctx_init(struct mml_ctx *ctx, struct mml_dev *mml,
	const char * const threads[])
{
	/* create taskdone kthread first cause it is more easy for fail case */
	ctx->kt_done = kthread_create_worker(0, "%s", threads[0]);
	if (IS_ERR(ctx->kt_done)) {
		mml_err("[adpt]fail to create kthread worker %d",
			(s32)PTR_ERR(ctx->kt_done));
		ctx->kt_done = NULL;
		goto err;

	}
	ctx->wq_destroy = alloc_ordered_workqueue("%s", 0, threads[1]);
	if (threads[2]) {
		ctx->kt_config[0] = kthread_create_worker(0, "%s", threads[2]);
		if (IS_ERR(ctx->kt_config[0])) {
			mml_err("[adpt]fail to create config thread 0 %s err %pe",
				threads[2], ctx->kt_config[0]);
			ctx->kt_config[0] = NULL;
			goto err;
		}
	}
	if (threads[3]) {
		ctx->kt_config[1] = kthread_create_worker(0, "%s", threads[3]);
		if (IS_ERR(ctx->kt_config[1])) {
			mml_err("[adpt]fail to create config thread 1 %s err %pe",
				threads[3], ctx->kt_config[1]);
			ctx->kt_config[1] = NULL;
			goto err;
		}
	}

	INIT_LIST_HEAD(&ctx->configs);
	mutex_init(&ctx->config_mutex);
	ctx->mml = mml;
	return 0;

err:
	if (ctx->kt_done) {
		kthread_destroy_worker(ctx->kt_done);
		ctx->kt_done = NULL;
	}
	if (ctx->wq_destroy) {
		destroy_workqueue(ctx->wq_destroy);
		ctx->wq_destroy = NULL;
	}
	if (ctx->kt_config[0]) {
		kthread_destroy_worker(ctx->kt_config[0]);
		ctx->kt_config[0] = NULL;
	}
	if (ctx->kt_config[1]) {
		kthread_destroy_worker(ctx->kt_config[1]);
		ctx->kt_config[1] = NULL;
	}
	return -EIO;
}

void mml_ctx_deinit(struct mml_ctx *ctx)
{
	struct mml_frame_config *cfg, *tmp;
	u32 i, j;
	struct list_head local_list;

	INIT_LIST_HEAD(&local_list);

	/* clone list_head first to aviod circular lock */
	mutex_lock(&ctx->config_mutex);
	list_splice_tail_init(&ctx->configs, &local_list);
	mutex_unlock(&ctx->config_mutex);

	list_for_each_entry_safe_reverse(cfg, tmp, &local_list, entry) {
		/* check and remove configs/tasks in this context */
		list_del_init(&cfg->entry);
		frame_config_queue_destroy(cfg);
	}

	destroy_workqueue(ctx->wq_destroy);
	ctx->wq_destroy = NULL;
	if (ctx->kt_config[0]) {
		kthread_destroy_worker(ctx->kt_config[0]);
		ctx->kt_config[0] = NULL;
	}
	if (ctx->kt_config[1]) {
		kthread_destroy_worker(ctx->kt_config[1]);
		ctx->kt_config[1] = NULL;
	}
	kthread_destroy_worker(ctx->kt_done);
	ctx->kt_done = NULL;

	for (i = 0; i < ARRAY_SIZE(ctx->tile_cache); i++)
		for (j = 0; j < ARRAY_SIZE(ctx->tile_cache[i].func_list); j++)
			kfree(ctx->tile_cache[i].func_list[j]);
}

void frame_check_end_time(struct timespec64 *endtime)
{
	if (!endtime->tv_sec && !endtime->tv_nsec) {
		ktime_get_real_ts64(endtime);
		timespec64_add_ns(endtime, MML_DEFAULT_END_NS);
	}
}
