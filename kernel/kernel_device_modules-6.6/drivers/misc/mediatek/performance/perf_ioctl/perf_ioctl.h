/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef PERF_IOCTL_H
#define PERF_IOCTL_H
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
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
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <linux/platform_device.h>

#include <linux/ioctl.h>

struct _FPSGO_PACKAGE {
	union {
		__u32 tid;
		__s32 fps;
		__s32 cmd;
		__s32 active;
		__u32 pid1;
	};
	union {
		__u32 start;
		__u32 connectedAPI;
		__u32 value1;
	};
	union {
		__u64 frame_time;
		__u64 bufID;
		__s64 time_diff;
		__u64 sf_buf_id;
	};
	__u64 frame_id;
	union {
		__s32 queue_SF;
		__s32 value2;
		__u32 pid2;
	};
	__u64 identifier;
};

struct _FPSGO_SBE_PACKAGE {
	__u32 pid;
	__u32 rtid;
	__u64 frame_id;
	__u64 identifier;
	__u32 start;
	__u32 blc;
	__u64 mask;
	__u8 name[16];
	__u8 specific_name[1000];
	__s32 num;
	__u32 mode;
};

struct _XGFFRAME_PACKAGE {
	__u32 tid;
	__u64 queueid;
	__u64 frameid;

	__u64 cputime;
	__u32 area;
	__u32 deplist_size;

	union {
		__u32 *deplist;
		__u64 p_dummy_deplist;
		__u32 min_cap;
	};
};

struct _SMART_LAUNCH_PACKAGE {
	int target_time;
	int feedback_time;
	int pre_opp;
	int next_opp;
	int capabilty_ration;
};

struct _FPSGO_LR_PAIR_PACKAGE {
	union {
		__u32 tid;
	};
	union {
		__u64 surface_id;
	};
	union {
		__u64 buffer_id;
		__u64 exp_l2q_ns;
		__u64 rl_exp_l2q_us;
	};
	__u64 queue_ts;
	__u64 logic_head_ts;
	__u64 l2q_ns;
	union {
		__u32 is_logic_head_valid;
		__u32 fpsgo_l2q_enable;
		__u32 exp_vsync_multiple;
	};
	__u64 ktime_now_ns;
};


#define FPSGO_QUEUE                  _IOW('g', 1,  struct _FPSGO_PACKAGE)
#define FPSGO_DEQUEUE                _IOW('g', 3,  struct _FPSGO_PACKAGE)
#define FPSGO_VSYNC                  _IOW('g', 5,  struct _FPSGO_PACKAGE)
#define FPSGO_TOUCH                  _IOW('g', 10, struct _FPSGO_PACKAGE)
#define FPSGO_SWAP_BUFFER            _IOW('g', 14, struct _FPSGO_PACKAGE)
#define FPSGO_QUEUE_CONNECT          _IOW('g', 15, struct _FPSGO_PACKAGE)
#define FPSGO_BQID                   _IOW('g', 16, struct _FPSGO_PACKAGE)
#define FPSGO_GET_FPS                _IOW('g', 17, struct _FPSGO_PACKAGE)
#define FPSGO_GET_CMD                _IOW('g', 18, struct _FPSGO_PACKAGE)
#define FPSGO_GET_FSTB_ACTIVE        _IOW('g', 20, struct _FPSGO_PACKAGE)
#define FPSGO_WAIT_FSTB_ACTIVE       _IOW('g', 21, struct _FPSGO_PACKAGE)
#define FPSGO_SBE_RESCUE             _IOW('g', 22, struct _FPSGO_PACKAGE)
#define FPSGO_ACQUIRE                _IOW('g', 23, struct _FPSGO_PACKAGE)
#define FPSGO_BUFFER_QUOTA           _IOW('g', 24, struct _FPSGO_PACKAGE)
#define FPSGO_GET_CAM_APK_PID        _IOW('g', 25, struct _FPSGO_PACKAGE)
#define FPSGO_GET_CAM_SERVER_PID     _IOW('g', 26, struct _FPSGO_PACKAGE)
#define FPSGO_SBE_SET_POLICY         _IOW('g', 27, struct _FPSGO_SBE_PACKAGE)
#define FPSGO_HINT_FRAME             _IOW('g', 28, struct _FPSGO_SBE_PACKAGE)
#define FPSGO_VSYNC_PERIOD           _IOW('g', 29, struct _FPSGO_PACKAGE)
#define FPSGO_SBE_BUFFER_COUNT       _IOW('g', 30, struct _FPSGO_PACKAGE)

#define XGFFRAME_START              _IOW('g', 1, struct _XGFFRAME_PACKAGE)
#define XGFFRAME_END                _IOW('g', 2, struct _XGFFRAME_PACKAGE)
#define XGFFRAME_MIN_CAP            _IOW('g', 3, struct _XGFFRAME_PACKAGE)

#define SMART_LAUNCH_ALGORITHM      _IOW('g', 1, struct _SMART_LAUNCH_PACKAGE)

#define FPSGO_LR_PAIR               _IOW('g', 1, struct _FPSGO_LR_PAIR_PACKAGE)
#define FPSGO_SF_TOUCH_ACTIVE       _IOW('g', 2, struct _FPSGO_LR_PAIR_PACKAGE)
#define FPSGO_SF_EXP_L2Q            _IOW('g', 3, struct _FPSGO_LR_PAIR_PACKAGE)

#endif

