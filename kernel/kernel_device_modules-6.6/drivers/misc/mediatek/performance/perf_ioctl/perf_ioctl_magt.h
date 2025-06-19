/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef PERF_IOCTL_MAGT_H
#define PERF_IOCTL_MAGT_H
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/cpumask.h>
#include <linux/mutex.h>
#include "fpsgo_frame_info.h"

#define max_cpus 8
#define MAX_MAGT_TARGET_FPS_NUM  10
#define MAGT_DEP_LIST_NUM  10
#define MAGT_GET_CPU_LOADING              _IOR('r', 0, struct cpu_info)
#define MAGT_GET_PERF_INDEX               _IOR('r', 1, struct cpu_info)
#define MAGT_SET_TARGET_FPS               _IOW('g', 2, struct target_fps_info)
//#define MAGT_SET_DEP_LIST                 _IOW('g', 3, struct dep_list_info)
#define MAGT_GET_FPSGO_SUPPORT            _IOWR('g', 4, struct fpsgo_pid_support)
#define MAGT_GET_FPSGO_STATUS             _IOWR('g', 5, struct fpsgo_render_status)//TODO
#define MAGT_GET_FPSGO_CRITICAL_THREAD_BG _IOWR('g', 6, struct fpsgo_bg_info)
#define MAGT_GET_FPSGO_CPU_FRAMETIME      _IOWR('g', 7, struct fpsgo_cpu_frametime)
#define MAGT_GET_FPSGO_THREAD_LOADING     _IOWR('g', 8, struct fpsgo_thread_loading)
#define MAGT_GET_FPSGO_RENDER_PERFIDX     _IOWR('g', 9, struct fpsgo_render_perf)
#define MAGT_NOTIFY_THREAD_STATUS         _IOW('g', 10, struct thread_status_info)
#define MAGT_SET_DEP_LIST_V3              _IOW('g', 11, struct dep_list_info_V3)

struct thread_param {
	int32_t tid;
	int32_t priority;
	int32_t preempt_time;
};

struct cpu_time {
	u64 time;
};

struct cpu_info {
	int cpu_loading[max_cpus];
	int perf_index[3];
};

struct target_fps_info {
	__u32 pid_arr[MAX_MAGT_TARGET_FPS_NUM ];
	__u32 tid_arr[MAX_MAGT_TARGET_FPS_NUM ];
	__u32 tfps_arr[MAX_MAGT_TARGET_FPS_NUM ];
	__u32 num;
};

struct dep_list_info_V3 {
	__u32 pid;
	struct thread_param user_dep_arr[MAGT_DEP_LIST_NUM];
	__u32 user_dep_num;
};

struct fpsgo_pid_support {
	int32_t pid;
	bool isSupport;
};

struct fpsgo_render_status {
	int32_t pid;
	int32_t curFps;
	int32_t targetFps;
	int32_t targetFps_diff;
	long long t_gpu;
};

struct fpsgo_bg_info {
	int32_t pid;
	int32_t bg_num;
	int32_t bg_pid[FPSGO_MAX_TASK_NUM];
	int32_t bg_loading[FPSGO_MAX_TASK_NUM];
};

struct fpsgo_cpu_frametime {
	int32_t pid;
	unsigned long long raw_t_cpu;
	unsigned long long ema_t_cpu;
};

struct fpsgo_thread_loading {
	int32_t pid;
	int32_t avg_freq;
	int32_t dep_num;
	int32_t dep_pid[FPSGO_MAX_TASK_NUM];
	int32_t dep_loading[FPSGO_MAX_TASK_NUM];
};

struct fpsgo_render_perf {
	int32_t pid;
	int32_t perf_idx;
};
struct thread_status_info {
	__u32 frameid;
	__u32 type;
	__u32 status;
	__u64 tv_ts;
};

extern int get_cpu_loading(struct cpu_info *_ci);
extern int get_perf_index(struct cpu_info *_ci);
extern u64 get_cpu_idle_time(unsigned int cpu, u64 *wall, int io_busy);

#endif
