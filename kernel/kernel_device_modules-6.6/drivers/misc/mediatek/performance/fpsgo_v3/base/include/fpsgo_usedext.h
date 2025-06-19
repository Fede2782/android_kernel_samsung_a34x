/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_USEDEXT_H__
#define __FPSGO_USEDEXT_H__

extern void (*cpufreq_notifier_fp)(int cid, unsigned long freq);
extern int (*fpsgo_notify_qudeq_fp)(int qudeq, unsigned int startend, int pid,
		unsigned long long identifier, unsigned long long sf_buf_id);
extern int (*fpsgo_notify_frame_hint_fp)(int qudeq,
		int pid, int frameID,
		unsigned long long id,
		int dep_mode, char *dep_name, int dep_num, long long frame_flags);
extern void (*fpsgo_notify_connect_fp)(int pid, int connectedAPI,
		unsigned long long identifier);
extern void (*fpsgo_notify_bqid_fp)(int pid, unsigned long long bufID,
		int queue_SF,
		unsigned long long identifier, int create);
extern void (*fpsgo_notify_vsync_fp)(void);
extern void (*fpsgo_notify_vsync_period_fp)(unsigned long long period);
extern void (*fpsgo_notify_swap_buffer_fp)(int pid);
extern void (*fpsgo_get_fps_fp)(int *pid, int *fps);
extern void (*fpsgo_get_cmd_fp)(int *cmd, int *value1, int *value2);
extern int (*fpsgo_get_fstb_active_fp)(long long time_diff);
extern int (*fpsgo_wait_fstb_active_fp)(void);
extern void (*fpsgo_notify_sbe_rescue_fp)(int pid, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frameID);
extern void (*fpsgo_notify_acquire_fp)(int c_pid, int p_pid,
	int connectedAPI, unsigned long long buffer_id);
extern void (*fpsgo_notify_buffer_quota_fp)(int pid, int quota,
		unsigned long long identifier);
extern void (*fpsgo_get_pid_fp)(int cmd, int *pid, int value1, int value2);
extern int (*fpsgo_notify_sbe_policy_fp)(int pid,  char *name,
	unsigned long mask, int start, char *specific_name, int num);
extern int (*fpsgo_notify_smart_launch_algorithm_fp)(int feedback_time,
	int target_time, int pre_opp, int capabilty_ration);
extern int (*fpsgo_notify_ux_buffer_count_fp)(int pid, int count, int maxBufferCount);

extern int (*magt2fpsgo_notify_target_fps_fp)(int *pid_arr, int *tid_arr,
	int *tfps_arr, int num);
extern int (*magt2fpsgo_notify_dep_list_fp)(int pid, void *dep_task_arr,
	int dep_task_num);

int fpsgo_is_force_enable(void);
void fpsgo_force_switch_enable(int enable);

int fpsgo_perfserv_ta_value(void);
void fpsgo_set_perfserv_ta(int value);

int fpsgo_get_kfpsgo_tid(void);

extern int (*xgff_frame_startend_fp)(unsigned int startend,
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist);
extern void (*xgff_frame_getdeplist_maxsize_fp)
		(unsigned int *pdeplistsize);
extern void (*xgff_frame_min_cap_fp)(unsigned int min_cap);

extern int (*fpsgo_get_lr_pair_fp)(unsigned long long sf_buffer_id,
	unsigned long long *cur_queue_ts,
	unsigned long long *l2q_ns, unsigned long long *logic_head_ts,
	unsigned int *is_logic_head_alive, unsigned long long *now_ts);
extern void (*fpsgo_set_rl_expected_l2q_us_fp)(int vsync_multiple,
	unsigned long long user_expected_l2q_us);
extern void (*fpsgo_set_rl_l2q_enable_fp)(int enable);

#endif
