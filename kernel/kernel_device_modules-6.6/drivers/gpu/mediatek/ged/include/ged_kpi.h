/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_KPI_H__
#define __GED_KPI_H__

#include "ged_type.h"
#include "ged_bridge_id.h"
/* To-Do: EAS*/
/*#include "eas_ctrl.h"*/
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>

GED_ERROR ged_kpi_dequeue_buffer_ts(int pid,
		u64 ullWdnd,
		int i32FrameID,
		int fence_fd,
		int isSF);
GED_ERROR ged_kpi_queue_buffer_ts(int pid,
		u64 ullWdnd,
		int i32FrameID,
		int fence,
		int QedBuffer_length);
GED_ERROR ged_kpi_acquire_buffer_ts(int pid, u64 ullWdnd, int i32FrameID);
GED_ERROR ged_kpi_sw_vsync(void);
GED_ERROR ged_kpi_hw_vsync(void);
int ged_kpi_get_uncompleted_count(void);

unsigned int ged_kpi_get_cur_fps(void);
unsigned int dump_gpu_fps_table(void);
void update_gpu_fps_table(int i32MarginValue);
unsigned int ged_kpi_get_cur_avg_cpu_time(void);
unsigned int ged_kpi_get_cur_avg_gpu_time(void);
unsigned int ged_kpi_get_cur_avg_response_time(void);
unsigned int ged_kpi_get_cur_avg_gpu_remained_time(void);
unsigned int ged_kpi_get_cur_avg_cpu_remained_time(void);
unsigned int ged_kpi_get_cur_avg_gpu_freq(void);

void ged_kpi_get_latest_perf_state(long long *t_cpu_remained,
		long long *t_gpu_remained,
		long *t_cpu_target,
		long *t_gpu_target);

GED_ERROR ged_kpi_system_init(void);
void ged_kpi_system_exit(void);
bool ged_kpi_set_cpu_remained_time(long long t_cpu_remained,
		int QedBuffer_length);
bool ged_kpi_set_gpu_dvfs_hint(int t_gpu_target, int t_gpu_cur);
unsigned int ged_kpi_enabled(void);
void ged_kpi_set_target_FPS(u64 ulID, int target_FPS);
void ged_kpi_set_target_FPS_margin(u64 ulID, int target_FPS,
		int target_FPS_margin, int eara_fps_margin, int cpu_time);
void ged_kpi_set_target_FPS_api(u64 ulID, int target_FPS,
		int target_FPS_margin);

u64 ged_kpi_get_taget_time(void);

#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
int ged_kpi_get_fw_idle_mode(void);
int ged_kpi_is_fw_idle_policy_enable(void);
void ged_kpi_set_fw_idle_mode(unsigned int mode);
void ged_kpi_enable_fw_idle_policy(unsigned int mode);
#endif /* MTK_GPU_FW_IDLE */
int ged_kpi_get_panel_refresh_rate(void);
int ged_kpi_get_target_fps(void);

void ged_kpi_update_t_gpu_latest_uncompleted(void);
struct ged_risky_bq_info {
	struct {
		long long t_gpu;
		int t_gpu_target;
		int t_gpu_fps;
		unsigned long long risk;
		unsigned long long ullWnd;
		bool useTimeStampD;
		int pid;
	} completed_bq, uncompleted_bq;

	unsigned int total_gpu_completed_count;
	bool smallframe;
};

struct ged_sysram_info {
	unsigned int last_tgpu_uncompleted;
	unsigned int uncompleted_count;
	unsigned int last_tgpu_uncompleted_target;
	unsigned long long last_uncomplete_soc_timer;
	unsigned long long current_timestamp;
	bool smallframe;
};

void ged_kpi_update_sysram_uncompleted_tgpu(struct ged_sysram_info *info);
void ged_kpi_set_loading_mode(unsigned int mode, unsigned int stride_size);

GED_ERROR ged_kpi_timer_based_pick_riskyBQ(struct ged_risky_bq_info *info);
int ged_kpi_get_main_bq_uncomplete_count(void);
GED_ERROR ged_kpi_eb_dvfs_get_time(struct ged_risky_bq_info *info);

/* For Gift Usage */
GED_ERROR ged_kpi_query_dvfs_freq_pred(int *gpu_freq_cur
	, int *gpu_freq_max, int *gpu_freq_pred);
GED_ERROR ged_kpi_query_gpu_dvfs_info(struct GED_BRIDGE_OUT_QUERY_GPU_DVFS_INFO *out);
GED_ERROR ged_kpi_hint_frame_info(struct GED_BRIDGE_OUT_HINT_FRAME_INFO *out);
GED_ERROR ged_kpi_set_gift_status(int mode);
GED_ERROR ged_kpi_set_gift_target_pid(int pid);
/* promotion use */
GED_ERROR ged_kpi_target_fps_hint(int promotion_enable, int target_fps);

unsigned long long ged_kpi_get_fb_timestamp(void);
unsigned long ged_kpi_get_fb_ulMask(void);
void update_fb_timer_set_count(void);
void reset_fb_timer_set_count(void);
void ged_kpi_fastdvfs_update_dcs(void);
bool ged_kpi_get_stable_lb(void);

extern spinlock_t gsGpuUtilLock;

// extern unsigned int g_gpufreqv2;
#endif
