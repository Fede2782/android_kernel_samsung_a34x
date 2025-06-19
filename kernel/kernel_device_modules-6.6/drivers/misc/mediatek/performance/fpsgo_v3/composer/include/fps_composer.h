/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __FPS_COMPOSER_H__
#define __FPS_COMPOSER_H__

#include <linux/rbtree.h>

enum FPSGO_SBE_MASK {
	FPSGO_CONTROL,
	FPSGO_MAX_TARGET_FPS,
	FPSGO_RESCUE_ENABLE,
	FPSGO_RL_ENABLE,
	FPSGO_GCC_DISABLE,
	FPSGO_QUOTA_DISABLE,
	FPSGO_RUNNING_CHECK,
	FPSGO_RUNNING_QUERY,
	FPSGO_NON_HWUI,
	FPSGO_HWUI,
	FPSGO_CLEAR_SCROLLING_INFO,
	FPSGO_MOVEING,
	FPSGO_FLING,
};

enum FPSGO_COM_KERNEL_NODE {
	BYPASS_NON_SF_GLOBAL,
	BYPASS_NON_SF_BY_PID,
	CONTROL_API_MASK_GLOBAL,
	CONTROL_API_MASK_BY_PID,
	CONTROL_HWUI_GLOBAL,
	CONTROL_HWUI_BY_PID,
	SET_UI_CTRL,
	FPSGO_CONTROL_GLOBAL,
	FPSGO_CONTROL_BY_PID,
	FPS_ALIGN_MARGIN,
	FPSGO_COM_POLICY_CMD,
	IS_FPSGO_BOOSTING,
};

struct fpsgo_com_policy_cmd {
	int tgid;
	int bypass_non_SF_by_pid;
	int control_api_mask_by_pid;
	int control_hwui_by_pid;
	int app_cam_meta_min_fps;
	int dep_loading_thr_by_pid;
	int cam_bypass_window_ms_by_pid;
	int mfrc_active_by_pid;
	unsigned long long ts;
	struct rb_node rb_node;
};

int fpsgo_composer_init(void);
void fpsgo_composer_exit(void);

void fpsgo_ctrl2comp_dequeue_end(int pid,
			unsigned long long dequeue_end_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_dequeue_start(int pid,
			unsigned long long dequeue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_enqueue_end(int pid,
			unsigned long long enqueue_end_time,
			unsigned long long identifier,
			unsigned long long sf_buf_id);
void fpsgo_ctrl2comp_enqueue_start(int pid,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_bqid(int pid, unsigned long long buffer_id,
			int queue_SF, unsigned long long identifier,
			int create);
void fpsgo_ctrl2comp_hint_frame_start(int pid,
			unsigned long long frameID,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_hint_frame_end(int pid,
			unsigned long long frameID,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_hint_doframe_end(int pid,
			unsigned long long frameID,
			unsigned long long enqueue_start_time,
			unsigned long long identifier, long long frame_flags);
void fpsgo_ctrl2comp_hint_frame_err(int pid,
			unsigned long long frameID,
			unsigned long long time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_hint_frame_dep_task(int rtid,
			unsigned long long identifier,
			int dep_mode,
			char *dep_name,
			int dep_num);

void fpsgo_ctrl2comp_connect_api(int pid, int api,
	unsigned long long identifier);
void fpsgo_ctrl2comp_disconnect_api(int pid, int api,
			unsigned long long identifier);
void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts);
void fpsgo_ctrl2comp_set_app_meta_fps(int tgid, int fps, unsigned long long ts);
int fpsgo_ctrl2comp_set_sbe_policy(int tgid, char *name, unsigned long mask, unsigned long long ts,
	int start, char *specific_name, int num);
void fpsgo_ctrl2comp_hint_buffer_count(int pid, int count, int max_count);
int switch_ui_ctrl(int pid, int set_ctrl);
int fpsgo_get_fpsgo_is_boosting(void);
int register_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb);
int unregister_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb);
int fpsgo_com2other_notify_fpsgo_is_boosting(int boost);
void fpsgo_com_notify_fpsgo_is_boost(int enable);

int fpsgo_ctrl2comp_user_create(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time);
int fpsgo_ctrl2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long *tcpu_arr, unsigned long long *ts_arr, int num);
void fpsgo_ctrl2comp_control_resume(int render_tid, unsigned long long buffer_id);
void fpsgo_ctrl2comp_control_pause(int render_tid, unsigned long long buffer_id);
void fpsgo_ctrl2comp_user_close(int tgid, int render_tid, unsigned long long buffer_id);
int fpsgo_ctrl2comp_set_target_time(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long target_time);
int fpsgo_ctrl2comp_set_dep_list(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num);
int notify_fpsgo_touch_latency_ko_ready(void);
int fpsgo_com_get_mfrc_is_on(void);

#endif

