/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Iris-sc Yang <iris-sc.yang@mediatek.com>
 */

#ifndef __MTK_MML_ADAPTOR_H__
#define __MTK_MML_ADAPTOR_H__

#include "mtk-mml-core.h"
#include "mtk-mml.h"

struct mml_ctx {
	struct list_head configs;
	u32 config_cnt;
	struct mutex config_mutex;
	struct mml_dev *mml;
	const struct mml_task_ops *task_ops;
	const struct mml_config_ops *cfg_ops;
	atomic_t job_serial;
	atomic_t config_serial;
	struct kthread_worker *kt_config[MML_PIPE_CNT];
	struct workqueue_struct *wq_destroy;
	struct kthread_worker *kt_done;
	bool kt_priority;
	bool disp_dual;
	bool disp_vdo;
	void (*submit_cb)(void *cb_param);
	void (*config_cb)(struct mml_task *task, void *cb_param);
	struct mml_tile_cache tile_cache[MML_PIPE_CNT];
};

struct mml_frame_config *frame_config_find_reuse(struct mml_ctx *ctx,
	struct mml_submit *submit);
void frame_config_destroy(struct mml_frame_config *cfg);
void frame_config_queue_destroy(struct mml_frame_config *cfg);
void frame_config_init(struct mml_frame_config *cfg,
	struct mml_ctx *ctx,
	struct mml_submit *submit);
struct mml_frame_config *frame_config_create(
	struct mml_ctx *ctx,
	struct mml_submit *submit);
/* common config operations */
void frame_config_get(struct mml_frame_config *cfg);
void frame_config_put(struct mml_frame_config *cfg);
void frame_config_free(struct mml_frame_config *cfg);

struct mml_task *task_get_idle(struct mml_frame_config *cfg);
void task_move_to_destroy(struct kref *kref);
void task_move_to_running(struct mml_task *task);
void task_move_to_idle(struct mml_task *task);
/* common task operations */
void task_queue(struct mml_task *task, u32 pipe);
void task_submit_done(struct mml_task *task);
s32 task_dup(struct mml_task *task, u32 pipe);
struct mml_tile_cache *task_get_tile_cache(struct mml_task *task, u32 pipe);
void ctx_kt_setsched(struct mml_ctx *ctx);

void task_state_dec(struct mml_frame_config *cfg, struct mml_task *task,
	const char *api);
void dump_pq_en(u32 idx, struct mml_pq_param *pq_param,
	struct mml_pq_config *pq_config);

s32 frame_buf_to_task_buf(struct mml_file_buf *fbuf,
			  struct mml_buffer *user_buf,
			  const char *name);
void task_buf_put(struct mml_task *task);
void frame_calc_plane_offset(struct mml_frame_data *data,
	struct mml_buffer *buf);

int mml_ctx_init(struct mml_ctx *ctx, struct mml_dev *mml,
	const char * const threads[]);
void mml_ctx_deinit(struct mml_ctx *ctx);

void frame_check_end_time(struct timespec64 *endtime);

#endif	/* __MTK_MML_ADAPTOR_H__ */
