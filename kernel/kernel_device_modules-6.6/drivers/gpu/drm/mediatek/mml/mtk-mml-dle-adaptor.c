// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <mtk_drm_drv.h>

#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-adaptor.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-pq-core.h"

int dle_max_cache_task = 1;
module_param(dle_max_cache_task, int, 0644);

int dle_max_cache_cfg = 2;
module_param(dle_max_cache_cfg, int, 0644);

struct mml_dle_ctx {
	struct mml_ctx ctx;
};

/* dle extension of mml_frame_config */
struct mml_dle_frame_config {
	struct mml_frame_config c;
	/* tile struct in frame mode for each pipe */
	struct mml_tile_config tile[MML_PIPE_CNT];
};

static struct mml_dle_frame_config *frame_config_to_dle(struct mml_frame_config *cfg)
{
	return container_of(cfg, struct mml_dle_frame_config, c);
}

static struct mml_frame_config *frame_config_find_current(struct mml_ctx *ctx)
{
	return list_first_entry_or_null(
		&ctx->configs, struct mml_frame_config, entry);
}

static struct mml_task *task_find_running(struct mml_frame_config *cfg)
{
	return list_first_entry_or_null(
		&cfg->tasks, struct mml_task, entry);
}

static struct mml_frame_config *dle_frame_config_create(
	struct mml_ctx *ctx,
	struct mml_submit *submit)
{
	struct mml_dle_frame_config *dle_cfg = kzalloc(sizeof(*dle_cfg), GFP_KERNEL);
	struct mml_frame_config *cfg;

	if (!dle_cfg)
		return ERR_PTR(-ENOMEM);
	cfg = &dle_cfg->c;
	frame_config_init(cfg, ctx, submit);
	return cfg;
}

static struct mml_task *task_get_idle_or_running(struct mml_frame_config *cfg)
{
	struct mml_task *task = task_get_idle(cfg);

	if (task)
		return task;
	task = task_find_running(cfg);
	if (task) {
		/* stop this running task */
		mml_msg("[dle]stop task %p state %u job %u",
			task, task->state, task->job.jobid);

		task_buf_put(task);

		list_del_init(&task->entry);
		cfg->run_task_cnt--;
		memset(&task->buf, 0, sizeof(task->buf));
		if (cfg->info.dest[0].pq_config.en)
			mml_pq_reset_hist_status(task);
	}
	return task;
}

static void dle_task_frame_err(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_ctx *ctx = task->ctx;

	mml_trace_ex_begin("%s", __func__);

	mml_err("[dle]config %p err task %p state %u job %u",
		cfg, task, task->state, task->job.jobid);

	/* clean up */
	task_buf_put(task);

	mutex_lock(&ctx->config_mutex);

	task_state_dec(cfg, task, __func__);
	mml_err("[dle]%s task cnt (%u %u %hhu) error from state %d",
		__func__,
		cfg->await_task_cnt,
		cfg->run_task_cnt,
		cfg->done_task_cnt,
		task->state);

	task->err = true;
	cfg->err = true;

	mutex_unlock(&ctx->config_mutex);

	/* mml_lock_wake_lock(mml, false); */

	mml_trace_ex_end();
}

void mml_dle_start(struct mml_dle_ctx *dctx)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	struct mml_frame_config *tmp;
	struct mml_task *task;

	mml_trace_ex_begin("%s", __func__);

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_current(ctx);
	if (!cfg) {
		mml_log("%s cannot find current cfg", __func__);
		goto done;
	}

	/* clean up */
	if (cfg->done_task_cnt > dle_max_cache_task) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task), entry);
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		mml_msg("[dle]%s task cnt (%u %u %hhu)",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);
		kref_put(&task->ref, task_move_to_destroy);
	}

	/* still have room to cache, done */
	if (ctx->config_cnt <= dle_max_cache_cfg)
		goto done;

	/* must pick cfg from list which is not running */
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* only remove config not running */
		if (!list_empty(&cfg->tasks) || !list_empty(&cfg->await_tasks))
			continue;
		list_del_init(&cfg->entry);
		frame_config_queue_destroy(cfg);
		ctx->config_cnt--;
		mml_msg("[dle]config %p send destroy remain %u",
			cfg, ctx->config_cnt);

		/* check cache num again */
		if (ctx->config_cnt <= dle_max_cache_cfg)
			break;
	}

done:
	mutex_unlock(&ctx->config_mutex);

	/* mml_lock_wake_lock(mml, false); */

	mml_trace_ex_end();
}

s32 mml_dle_config(struct mml_dle_ctx *dctx, struct mml_submit *submit,
		   void *cb_param)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;
	s32 result;
	u32 i;

	mml_trace_begin("%s", __func__);

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

	/* always fixup plane offset */
	frame_calc_plane_offset(&submit->info.src, &submit->buffer.src);
	frame_calc_plane_offset(&submit->info.seg_map, &submit->buffer.seg_map);
	for (i = 0; i < submit->info.dest_cnt; i++)
		frame_calc_plane_offset(&submit->info.dest[i].data,
			&submit->buffer.dest[i]);

	/* mml_mmp(submit, MMPROFILE_FLAG_PULSE, atomic_read(&ctx->job_serial), 0); */

	mutex_lock(&ctx->config_mutex);

	mml_mmp(dle_config_create, MMPROFILE_FLAG_PULSE, 0, 0);

	cfg = frame_config_find_reuse(ctx, submit);
	if (cfg) {
		mml_msg("[dle]%s reuse config %p", __func__, cfg);
		task = task_get_idle_or_running(cfg);
		if (task) {
			/* reuse case change state IDLE to REUSE */
			task->state = MML_TASK_REUSE;
			mml_msg("[dle]reuse task %p", task);
		} else {
			task = mml_core_create_task(atomic_read(&ctx->job_serial));
			if (IS_ERR(task)) {
				result = PTR_ERR(task);
				mml_err("%s create task for reuse frame fail", __func__);
				goto err_unlock_exit;
			}
			task->config = cfg;
			task->state = MML_TASK_DUPLICATE;
			/* add more count for new task create */
			cfg->cfg_ops->get(cfg);
		}
	} else {
		cfg = dle_frame_config_create(ctx, submit);

		mml_msg("[dle]%s create config %p", __func__, cfg);
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
			mml_err("%s create task fail", __func__);
			goto err_unlock_exit;
		}
		task->config = cfg;
		/* add more count for new task create */
		cfg->cfg_ops->get(cfg);
	}

	/* make sure id unique and cached last */
	task->job.jobid = atomic_inc_return(&ctx->job_serial);
	task->adaptor_type = MML_ADAPTOR_DLE;
	task->cb_param = cb_param;
	cfg->last_jobid = task->job.jobid;
	list_add_tail(&task->entry, &cfg->await_tasks);
	cfg->await_task_cnt++;
	mml_msg("[dle]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;
	result = frame_buf_to_task_buf(&task->buf.src,
		&submit->buffer.src,
		"mml_dle_rdma");
	if (result) {
		mml_err("[dle]%s get dma buf fail", __func__);
		goto err_buf_exit;
	}

	if (submit->info.dest[0].pq_config.en_region_pq) {
		result = frame_buf_to_task_buf(&task->buf.seg_map,
			&submit->buffer.seg_map,
			"mml_dle_rdma_seg");
		if (result) {
			mml_err("[dle]%s get dma buf fail", __func__);
			goto err_buf_exit;
		}
	}

	task->buf.dest_cnt = submit->buffer.dest_cnt;
	for (i = 0; i < submit->buffer.dest_cnt; i++) {
		result = frame_buf_to_task_buf(&task->buf.dest[i],
			&submit->buffer.dest[i],
			"mml_dle_wrot");
		if (result) {
			mml_err("[dle]%s get dma buf fail", __func__);
			goto err_buf_exit;
		}
	}

	/* no fence for dle task */
	task->job.fence = -1;
	mml_msg("[dle]mml job %u task %p config %p mode %hhu",
		task->job.jobid, task, cfg, cfg->info.mode);

	/* copy job content back */
	if (submit->job)
		*submit->job = task->job;

	/* copy pq parameters */
	for (i = 0; i < submit->buffer.dest_cnt && submit->pq_param[i]; i++)
		task->pq_param[i] = *submit->pq_param[i];

	/* wake lock */
	/* mml_lock_wake_lock(task->config->mml, true); */

	/* get config from core */
	mml_core_submit_task(cfg, task);
	if (cfg->err)
		result = -EINVAL;

	mml_trace_end();
	return result;

err_unlock_exit:
	mutex_unlock(&ctx->config_mutex);
err_buf_exit:
	mml_trace_end();
	mml_log("%s fail result %d", __func__, result);
	return result;
}

struct mml_task *mml_dle_stop(struct mml_dle_ctx *dctx)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;
	s32 cnt;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_current(ctx);
	if (!cfg)
		mml_err("[dle]The current cfg not found to stop");
	else
		task = task_find_running(cfg);
	if (!task) {
		mml_log("%s cannot find running task", __func__);
		goto done;
	}

	/* cnt can be 1 or 2, if dual on and count 2 means pipes done */
	cnt = atomic_inc_return(&task->pipe_done);
	if (task->config->dual && cnt == 1)
		goto done;

	mml_msg("[dle]stop task %p state %u job %u pipes %d",
		task, task->state, task->job.jobid, cnt);

	task_buf_put(task);

	task_move_to_idle(task);

done:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return task;
}

struct mml_task *mml_dle_disable(struct mml_dle_ctx *dctx)
{
	struct mml_ctx *ctx = &dctx->ctx;
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_current(ctx);
	if (cfg) {
		task = list_first_entry_or_null(
			&cfg->done_tasks, struct mml_task, entry);
		if (!task)
			task = task_find_running(cfg);
	}

	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return task;
}

static struct mml_tile_cache *dle_task_get_tile_cache(struct mml_task *task, u32 pipe)
{
	struct mml_ctx *ctx = task->ctx;
	struct mml_dle_frame_config *dle_cfg = frame_config_to_dle(task->config);

	ctx->tile_cache[pipe].tiles = &dle_cfg->tile[pipe];
	return &ctx->tile_cache[pipe];
}

static const struct mml_task_ops dle_task_ops = {
	.submit_done = task_submit_done,
	.frame_err = dle_task_frame_err,
	.dup_task = task_dup,
	.get_tile_cache = dle_task_get_tile_cache,
};

static void dle_config_free(struct mml_frame_config *cfg)
{
	kfree(frame_config_to_dle(cfg));
}

static const struct mml_config_ops dle_config_ops = {
	.get = frame_config_get,
	.put = frame_config_put,
	.free = dle_config_free,
};

struct mml_dle_ctx *mml_dle_ctx_create(struct mml_dev *mml)
{
	static const char * const threads[] = {
		"mml_dle_done", "mml_destroy_dl",
		NULL, NULL,
	};
	struct mml_dle_ctx *ctx;
	int ret;

	mml_msg("[dle]%s on dev %p", __func__, mml);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ret = mml_ctx_init(&ctx->ctx, mml, threads);
	if (ret) {
		kfree(ctx);
		return ERR_PTR(ret);
	}

	ctx->ctx.task_ops = &dle_task_ops;
	ctx->ctx.cfg_ops = &dle_config_ops;

	return ctx;
}

static void dle_ctx_setup(struct mml_dle_ctx *ctx, struct mml_dle_param *dl)
{
	ctx->ctx.disp_dual = dl->dual;
	ctx->ctx.config_cb = dl->config_cb;
}

struct mml_dle_ctx *mml_dle_get_context(struct device *dev,
					struct mml_dle_param *dl)
{
	struct mml_dev *mml = dev_get_drvdata(dev);

	mml_msg("[dle]%s", __func__);
	if (!mml) {
		mml_err("[dle]%s not init mml", __func__);
		return ERR_PTR(-EPERM);
	}
	return mml_dev_get_dle_ctx(mml, dl, dle_ctx_setup);
}

static void dle_ctx_release(struct mml_dle_ctx *ctx)
{
	mml_msg("[dle]%s on ctx %p", __func__, ctx);

	mml_ctx_deinit(&ctx->ctx);
	/* no need for ctx->tile_cache[i].tiles, since dle adaptor
	 * use tile struct in mml_dle_frame_config
	 */

	kfree(ctx);
}

void mml_dle_put_context(struct mml_dle_ctx *ctx)
{
	if (IS_ERR_OR_NULL(ctx))
		return;
	mml_log("[dle]%s", __func__);
	mml_dev_put_dle_ctx(ctx->ctx.mml, dle_ctx_release);
}

struct mml_ddp_comp_match {
	enum mtk_ddp_comp_id id;
	enum mtk_ddp_comp_type type;
	const char *name;
};

static const struct mml_ddp_comp_match mml_ddp_matches[] = {
	{ DDP_COMPONENT_MML_MML0, MTK_MML_MML, "mmlsys" },
	{ DDP_COMPONENT_MML_MUTEX0, MTK_MML_MUTEX, "mutex0" },

	{ DDP_COMPONENT_MML_MML0, MTK_MML_MML, "mml_mmlsys" },
	{ DDP_COMPONENT_MML_MML0, MTK_MML_MML, "mml0_mmlsys" },
	{ DDP_COMPONENT_MML_MML0, MTK_MML_MML, "mml1_mmlsys" },
	{ DDP_COMPONENT_MML_MUTEX0, MTK_MML_MUTEX, "mml_mutex0" },
	{ DDP_COMPONENT_MML_MUTEX1, MTK_MML_MUTEX, "mml0_mutex0" },
	{ DDP_COMPONENT_MML_MUTEX0, MTK_MML_MUTEX, "mml1_mutex0" },
};

static s32 mml_ddp_comp_get_id(struct device_node *node, const char *name)
{
	u32 i;

	if (!name) {
		mml_err("no comp-names in component %s for ddp binding",
			node->full_name);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mml_ddp_matches); i++) {
		if (!strcmp(name, mml_ddp_matches[i].name))
			return mml_ddp_matches[i].id;
	}
	mml_err("no ddp component matches: %s", name);
	return -ENODATA;
}

int mml_ddp_comp_init(struct device *dev,
		      struct mtk_ddp_comp *ddp_comp, struct mml_comp *mml_comp,
		      const struct mtk_ddp_comp_funcs *funcs)
{
	if (unlikely(!funcs))
		return -EINVAL;

	ddp_comp->id = mml_ddp_comp_get_id(dev->of_node, mml_comp->name);
	if (IS_ERR_VALUE(ddp_comp->id))
		return ddp_comp->id;

	mml_log("%s match %s comp %u ddp %u",
		__func__, mml_comp->name, mml_comp->id, ddp_comp->id);

	ddp_comp->funcs = funcs;
	ddp_comp->dev = dev;
	/* ddp_comp->clk = mml_comp->clks[0]; */
	ddp_comp->regs_pa = mml_comp->base_pa;
	ddp_comp->regs = mml_comp->base;
	/* ddp_comp->cmdq_base = cmdq_register_device(dev); */
	ddp_comp->larb_dev = mml_comp->larb_dev;
	ddp_comp->larb_id = mml_comp->larb_port;
	ddp_comp->sub_idx = mml_comp->sub_idx;
	return 0;
}

int mml_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	if (IS_ERR_VALUE(comp->id) || comp->id >= DDP_COMPONENT_ID_MAX)
		return -EINVAL;
	if (private->ddp_comp[comp->id])
		return -EBUSY;

	private->ddp_comp[comp->id] = comp;
	return 0;
}

void mml_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	if (comp && comp->id < DDP_COMPONENT_ID_MAX && !IS_ERR_VALUE(comp->id))
		private->ddp_comp[comp->id] = NULL;
}

