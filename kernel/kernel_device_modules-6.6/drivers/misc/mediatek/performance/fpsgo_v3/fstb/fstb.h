/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef FSTB_H
#define FSTB_H

enum FSTB_INFO {
	FPSGO_Q2Q_TIME = 0,
	FPSGO_CPU_TIME = 1,
	FPSGO_QUEUE_FPS = 2,
	FPSGO_TARGET_FPS = 3,
	FPSGO_PERF_IDX = 4,
	FPSGO_DELETE = 5,
};

enum FPSGO_FSTB_KERNEL_NODE {
	FPSGO_STATUS,
	FSTB_DEBUG,
	FSTB_FPS_LIST,
	FSTB_POLICY_CMD,
	FSTB_TFPS_INFO,
	FSTB_FRS_INFO,
	FSTB_SELF_CTRL_FPS_ENABLE_GLOBAL,
	FSTB_SELF_CTRL_FPS_ENABLE_BY_PID,
	NOTIFY_FSTB_TARGET_FPS_BY_PID,
	FSTB_NO_R_TIMER_ENABLE,
	SET_RENDER_MAX_FPS,
	FSTB_MARGIN_MODE,
	FSTB_MARGIN_MODE_DBNC_A,
	FSTB_MARGIN_MODE_DBNC_B,
	FSTB_RESET_TOLERENCE,
	FSTB_TUNE_QUANTILE,
	GPU_SLOWDOWN_CHECK,
	FSTB_SOFT_LEVEL,
};

typedef void (*time_notify_callback)(int pid, unsigned long long bufID,
	int fps, unsigned long long time);
typedef void (*perf_notify_callback)(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts);

int mtk_fstb_exit(void);
int mtk_fstb_init(void);
void fpsgo_comp2fstb_queue_time_update(
	int pid, unsigned long long bufID, int frame_type,
	unsigned long long ts,
	int api, int hwui_flag);
void fpsgo_comp2fstb_prepare_calculate_target_fps(int pid,
	unsigned long long bufID,
	unsigned long long cur_queue_end_ts);
void fpsgo_comp2fstb_detect_app_self_ctrl(int tgid, int pid,
	unsigned long long bufID, unsigned long long ts);
unsigned long long fpsgo_other2fstb_get_app_self_ctrl_time(int pid,
	unsigned long long bufID, unsigned long long ts);
void fpsgo_comp2fstb_notify_info(int pid, unsigned long long bufID,
	unsigned long long q2q_time, unsigned long long enq_length,
	unsigned long long deq_length);
void fpsgo_ctrl2fstb_get_fps(int *pid, int *fps);
void fpsgo_ctrl2fstb_dfrc_fps(int fps);
int fpsgo_ctrl2fstb_wait_fstb_active(void);
void fpsgo_ctrl2fstb_cam_queue_time_update(unsigned long long ts);
int fpsgo_other2fstb_check_cam_do_frame(void);
int fpsgo_comp2fstb_do_recycle(void);
int fpsgo_other2fstb_set_target_time(int tgid, int rtid, unsigned long long bufID,
	unsigned long long target_time, int create);
int fpsgo_comp2fstb_get_logic_head(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end, unsigned long long prev_queue_end_ts,
	unsigned long long pprev_queue_end_ts, unsigned long long dequeue_start_ts,
	unsigned long long *logic_head_ts, int *has_logic_head);
int fpsgo_ctrl2fstb_magt_set_target_fps(int *pid_arr, int *tid_arr, int *tfps_arr, int num);
int fpsgo_other2fstb_register_info_callback(int mode, time_notify_callback func_cb);
int fpsgo_other2fstb_unregister_info_callback(int mode, time_notify_callback func_cb);
int fpsgo_other2fstb_register_perf_callback(int mode, perf_notify_callback func_cb);
int fpsgo_other2fstb_unregister_perf_callback(int mode, perf_notify_callback func_cb);
int fpsgo_fstb2other_info_update(int pid, unsigned long long bufID,
	int mode, int fps, unsigned long long time, int blc, int sbe_ctrl);
int fpsgo_other2fstb_get_fps(int pid, unsigned long long bufID,
	int *qfps_arr, int *qfps_num, int max_qfps_num,
	int *tfps_arr, int *tfps_num, int max_tfps_num,
	int *diff_arr, int *diff_num, int max_diff_num);
int fpsgo_ktf2fstb_add_delete_render_info(int mode, int pid, unsigned long long bufID,
	int target_fps, int queue_fps);
int switch_thread_max_fps(int pid, int set_max);

#if IS_ENABLED(CONFIG_MTK_FPSGO) || IS_ENABLED(CONFIG_MTK_FPSGO_V3)
int is_fstb_active(long long time_diff);
int fpsgo_ctrl2fstb_switch_fstb(int value);
int fpsgo_fbt2fstb_update_cpu_frame_info(
	int pid,
	unsigned long long bufID,
	int tgid,
	int frame_type,
	unsigned long long Q2Q_time,
	long long Runnging_time,
	int Target_time,
	unsigned int Curr_cap,
	unsigned int Max_cap,
	unsigned long long enqueue_length,
	unsigned long long dequeue_length);
void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_fps_ori, int *target_cpu_time, int *fps_margin,
		int *quantile_cpu_time, int *quantile_gpu_time,
		int *target_fpks, int *cooler_on);
long long fpsgo_base2fstb_get_gpu_time(int pid, unsigned long long bufID);

/* EARA */
void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
				int *tfps, int *rftp, int *hwui, char name[][16], int *proc_id);
void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
				int tfps);

#else
static inline int fpsgo_ctrl2fstb_switch_fstb(int en) { return 0; }
static inline int fpsgo_fbt2fstb_update_cpu_frame_info(
	int pid, unsigned long long bufID,
	int tgid, int frame_type, unsigned long long Q2Q_time,
	long long Runnging_time, int Target_time, unsigned int Curr_cap,
	unsigned int Max_cap, unsigned long long enqueue_length,
	unsigned long long dequeue_length) { return 0; }
static inline void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_fps_ori, int *target_cpu_time, int *fps_margin,
		int *quantile_cpu_time, int *quantile_gpu_time,
		int *target_fpks, int *cooler_on) { }
long long fpsgo_base2fstb_get_gpu_time(int pid, unsigned long long bufID) { }

/* EARA */
static inline void eara2fstb_get_tfps(int max_cnt, int *pid,
		unsigned long long *buf_id, int *tfps, int *hwui,
		char name[][16], int *proc_id) { }
static inline void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id,
		int diff, int tfps) { }

#endif

#endif

