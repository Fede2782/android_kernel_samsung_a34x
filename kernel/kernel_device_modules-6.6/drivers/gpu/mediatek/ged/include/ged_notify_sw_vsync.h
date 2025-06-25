/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_NOTIFY_SW_VSYNC_H__
#define __GED_NOTIFY_SW_VSYNC_H__

#include "ged_type.h"
#include "ged_dvfs.h"

extern unsigned int gpu_block;
extern unsigned int gpu_idle;
extern unsigned int gpu_av_loading;

#define GED_APO_AUTOSUSPEND_DELAY_MS 10
#define GED_APO_AUTOSUSPEND_DELAY_HFR_MS 16
#define GED_APO_AUTOSUSPEND_DELAY_MAX_MS 25

static atomic_t trigger_pm_callback_state = ATOMIC_INIT(0);


GED_ERROR ged_notify_sw_vsync(GED_VSYNC_TYPE eType,
	struct GED_DVFS_UM_QUERY_PACK *psQueryData);

GED_ERROR ged_notify_sw_vsync_system_init(void);

void ged_notify_sw_vsync_system_exit(void);

void ged_set_backup_timer_timeout(u64 time_out);
void ged_cancel_backup_timer(void);

u64 ged_get_fallback_time(void);

enum gpu_dvfs_policy_state {
	POLICY_STATE_INIT = -1,
	POLICY_STATE_LB = 0,
	POLICY_STATE_FORCE_LB,
	POLICY_STATE_FB,
	POLICY_STATE_LB_FALLBACK,
	POLICY_STATE_FORCE_LB_FALLBACK,
	POLICY_STATE_FB_FALLBACK,
};

enum gpu_fallback_mode {
	ALIGN_INTERVAL = 0,
	ALIGN_FB,
	ALIGN_LB,
	ALIGN_FAST_DVFS,
};

enum gpu_apo_support {
	APO_NORMAL_SUPPORT = 1,
	APO_2_0_NORMAL_SUPPORT,
	APO_NORMAL_AND_LP_SUPPORT,
	APO_INVALID_SUPPORT,
};

enum gpu_apo_hint {
	APO_NORMAL_HINT = 0,
	APO_LP_HINT,
	APO_INVALID_HINT,
};

enum gpu_dvfs_policy_state ged_get_policy_state(void);
enum gpu_dvfs_policy_state ged_get_prev_policy_state(void);
void ged_set_policy_state(enum gpu_dvfs_policy_state state);
void ged_set_prev_policy_state(enum gpu_dvfs_policy_state state);
unsigned long long ged_get_power_on_timestamp(void);
void ged_eb_dvfs_trace_dump(void);
int check_pm_callback_state(enum ged_gpu_power_state power_state);
void dump_pm_callback_kbase_info(void);

#if defined(CONFIG_GPU_MT8167) || defined(CONFIG_GPU_MT8173) ||\
defined(CONFIG_GPU_MT6739) || defined(CONFIG_GPU_MT6761) ||\
defined(CONFIG_GPU_MT6765)
extern void MTKFWDump(void);
#endif

//MBrain
extern unsigned long long g_ns_gpu_on_ts;
extern u32 g_curr_pwr_state;

#endif
