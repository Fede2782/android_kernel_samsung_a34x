// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/atomic.h>

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/timekeeping.h>
#include <asm/div64.h>

#include <mt-plat/mtk_gpu_utility.h>
#include "ged_notify_sw_vsync.h"
#include "ged_log.h"
#include "ged_tracepoint.h"
#include "ged_base.h"
#include "ged_monitor_3D_fence.h"
#include "ged.h"
#include "ged_dvfs.h"
#include "ged_dcs.h"
#include "ged_kpi.h"
#include "ged_eb.h"
#include "ged_log.h"

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

#define GED_DVFS_FB_TIMER_TIMEOUT 100000000
#define GED_DVFS_TIMER_TIMEOUT g_fallback_time_out

#ifndef ENABLE_TIMER_BACKUP

#undef GED_DVFS_TIMER_TIMEOUT

#define GED_DVFS_FB_TIMER_TIMEOUT 100000000
#define GED_DVFS_TIMER_TIMEOUT g_fallback_time_out

#endif /* GED_DVFS_TIMER_TIMEOUT */

static u64 g_fallback_time_out = GED_DVFS_FB_TIMER_TIMEOUT;

static struct hrtimer g_HT_hwvsync_emu;

/* MBrain */
static u64 g_last_pwr_ts;
static u64 g_last_pwr_update_ts_ms;
struct ged_gpu_power_state_time {
	//Unit: ns
	u64 start_ts;
	u64 end_ts;
	//Unit: ms
	u64 accumulate_time;
};

u32 g_curr_pwr_state;
static struct ged_gpu_power_state_time pwr_state_time[3];
/* MBrain end */

#include "ged_dvfs.h"
#include "ged_global.h"

static struct workqueue_struct *g_psNotifyWorkQueue;

static struct mutex gsVsyncStampLock;

struct GED_NOTIFY_SW_SYNC {
	struct work_struct	sWork;
	unsigned long t;
	long phase;
	unsigned long ul3DFenceDoneTime;
	bool bUsed;
};

#define MAX_NOTIFY_CNT 125
struct GED_NOTIFY_SW_SYNC loading_base_notify[MAX_NOTIFY_CNT];
int notify_index;
static enum gpu_dvfs_policy_state g_policy_state = POLICY_STATE_INIT;
static enum gpu_dvfs_policy_state g_prev_policy_state = POLICY_STATE_INIT;

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
#define GED_FRAME_TIME_CONFIG_NUM 4

struct ged_gpu_frame_time_table {
	unsigned int margin;     // target * 1.1
	unsigned int target;
};

static struct ged_gpu_frame_time_table g_ged_gpu_frame_time[GED_FRAME_TIME_CONFIG_NUM] = {
	{9166666, 8333333},      // 120fps
	{12222222, 11111111},    // 90fps
	{18333333, 16666666},    // 60fps
	{36666666, 33333333},    // 30fps
};

#define GED_APO_THR_NS 2000000

#define GED_APO_LP_THR_NS 4000000

#define GED_APO_WAKEUP_THR_NS (GED_APO_THR_NS + 1000000)

#define GED_APO_LONG_WAKEUP_THR_NS 100000000

#define GED_APO_AUTOSUSPEND_DELAY_TARGET_REF_COUNT 3

static spinlock_t g_sApoLock;


static unsigned long long g_apo_thr_ns;

static unsigned long long g_apo_wakeup_ns;
static unsigned long long g_apo_lp_thr_ns;
static unsigned long long g_gpu_frame_time_ns;

static unsigned int g_apo_autosuspend_delay_ms;

/*
 * prevA: Previous Active
 *     A: Active
 *     I: Idle
 */
static unsigned long long g_ns_gpu_A_ts;
static unsigned long long g_ns_gpu_I_ts;
static unsigned long long g_ns_gpu_I_to_A_duration;

static unsigned long long g_ns_gpu_predict_prevA_ts;
static unsigned long long g_ns_gpu_predict_A_ts;
static unsigned long long g_ns_gpu_predict_I_ts;
static unsigned long long g_ns_gpu_predict_A_to_I_duration;
static unsigned long long g_ns_gpu_predict_I_to_A_duration;
static unsigned long long g_ns_gpu_predict_A_to_A_duration;
static unsigned long long g_ns_gpu_predict_prev_A_to_A_duration;

static bool g_bGPUAPO;
static bool g_bGPUPredictAPO;
static int g_apo_hint;
static int g_apo_force_hint;

static int g_apo_autosuspend_delay_ref_count;
static int g_apo_autosuspend_delay_ctrl;
static int g_apo_autosuspend_delay_target_ref_count;
static enum ged_apo_legacy g_apo_legacy;

#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

static int g_whitebox_support_flag;
static int mcu_replace;
int stat_mcu_store[30][30]={0};

static int g_autosuspend_stress;

int (*ged_sw_vsync_event_fp)(bool bMode) = NULL;
EXPORT_SYMBOL(ged_sw_vsync_event_fp);
static struct mutex gsVsyncModeLock;
static int ged_sw_vsync_event(bool bMode)
{
	static bool bCurMode;
	int ret;

	ret = 0;
	mutex_lock(&gsVsyncModeLock);

	if (bCurMode != bMode) {
		bCurMode = bMode;
		if (ged_sw_vsync_event_fp) {
			ret = ged_sw_vsync_event_fp(bMode);
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] ALL mode change to %d ", bCurMode);
		} else
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] LOCAL mode change to %d ", bCurMode);
		if (bCurMode)
			ret = 1;
	}

	mutex_unlock(&gsVsyncModeLock);
	return ret;
}

u64 ged_get_fallback_time(void)
{
	u64 temp = 0;

	if (g_fallback_mode == ALIGN_INTERVAL)
		temp = (u64)g_fallback_time * 1000000;   //ms to ns
	else if (g_fallback_mode == ALIGN_FB)
		temp = div_u64(fb_timeout * g_fallback_time, 10);
	else if (g_fallback_mode == ALIGN_LB)
		temp = div_u64(lb_timeout * g_fallback_time, 10);
	else
		temp = (u64)g_fallback_time * 1000000;   //ms to ns

	return temp;
}


enum gpu_dvfs_policy_state ged_get_policy_state(void)
{
	return g_policy_state;
}

enum gpu_dvfs_policy_state ged_get_prev_policy_state(void)
{
	return g_prev_policy_state;
}

void ged_set_policy_state(enum gpu_dvfs_policy_state state)
{
	g_policy_state = state;
}

void ged_set_prev_policy_state(enum gpu_dvfs_policy_state state)
{
	g_prev_policy_state = state;
}

void ged_eb_dvfs_trace_dump(void)
{
#if defined(MTK_GPU_EB_SUPPORT)
	int ui32CeilingID = ged_get_cur_limit_idx_ceil();
	int ui32FloorID = ged_get_cur_limit_idx_floor();
	u64 eb_timeout_value = ged_get_fallback_time();
	int eb_policy_state = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_POLICY_STATE);
	int ged_policy_state =  ged_get_policy_state();
	int freq_id = ged_get_cur_oppidx();
	static int pre_eb_policy_state;
	static int pre_ged_policy_state;
	static int pre_freq_id;
	GED_DVFS_COMMIT_TYPE eCommitType;
	static int apply_lb_async;
	int top_freq_diff = 0, sc_freq_diff = 0;
	struct cmd_info custom_ceiling_info ={0};
	struct cmd_info custom_boost_info ={0};

	//struct GpuUtilization_Ex util_ex;

	if (ged_policy_state == POLICY_STATE_LB ||
			ged_policy_state == POLICY_STATE_FORCE_LB)
		eCommitType = GED_DVFS_LOADING_BASE_COMMIT;
	else if (ged_policy_state == POLICY_STATE_FB)
		eCommitType = GED_DVFS_FRAME_BASE_COMMIT;
	else
		eCommitType = GED_DVFS_FALLBACK_COMMIT;

	if (eb_policy_state != pre_eb_policy_state ||
		ged_policy_state != pre_ged_policy_state ||
		pre_freq_id != freq_id) {

		trace_tracing_mark_write(5566, "commit_type",
			eCommitType);

		trace_tracing_mark_write(5566, "EB_commit_type",
			eb_policy_state);

		trace_tracing_mark_write(5566, "gpu_freq",
			(long long) ged_get_cur_stack_freq() / 1000);

		sc_freq_diff = ged_get_cur_stack_out_freq() > 0 ?
			ged_get_cur_stack_out_freq() - ged_get_cur_real_stack_freq() : 0;
		top_freq_diff = ged_get_cur_top_out_freq() > 0 ?
			ged_get_cur_top_out_freq() - ged_get_cur_top_freq() : 0;

		trace_GPU_DVFS__Frequency(ged_get_cur_stack_freq() / 1000,
			ged_get_cur_real_stack_freq() / 1000, ged_get_cur_top_freq() / 1000,
			sc_freq_diff / 1000, top_freq_diff / 1000);

		trace_tracing_mark_write(5566, "gpu_freq_ceil",
			ged_get_freq_by_idx(ui32CeilingID) / 1000);
		trace_tracing_mark_write(5566, "gpu_freq_floor",
			ged_get_freq_by_idx(ui32FloorID) / 1000);
		trace_tracing_mark_write(5566, "limitter_ceil",
			ged_get_cur_limiter_ceil());
		trace_tracing_mark_write(5566, "limitter_floor",
			ged_get_cur_limiter_floor());
		if (ged_get_cur_limiter_ceil() == LIMIT_POWERHAL) {
			custom_ceiling_info = ged_dvfs_get_custom_ceiling_gpu_freq_info();
			trace_tracing_mark_write(5566, "limitter_ceil_pid",
				custom_ceiling_info.pid);
			trace_tracing_mark_write(5566, "limitter_ceil_id",
				custom_ceiling_info.user_id);
			trace_tracing_mark_write(5566, "limitter_ceil_cus_val",
				custom_ceiling_info.value);
		}

		if (ged_get_cur_limiter_floor() == LIMIT_POWERHAL) {
			custom_boost_info = ged_dvfs_get_custom_boost_gpu_freq_info();
			trace_tracing_mark_write(5566, "limitter_floor_pid",
				custom_boost_info.pid);
			trace_tracing_mark_write(5566, "limitter_floor_id",
				custom_boost_info.user_id);
			trace_tracing_mark_write(5566, "limitter_floor_cus_val",
				custom_boost_info.value);
		}


		if (dcs_get_adjust_support() % 2 != 0)
			trace_tracing_mark_write(5566, "preserve", g_force_disable_dcs);

		trace_tracing_mark_write(5566, "26m_replace",
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_26M_REPLACE));
	}

	if (eb_policy_state != POLICY_STATE_FB) {
		trace_tracing_mark_write(5566, "t_gpu",
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_T_GPU));
		trace_tracing_mark_write(5566, "t_gpu_target",
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_TARGET_GPU));
		trace_GPU_DVFS__EB_Loading_dump(
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_GPU_LOADING),
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_MCU_LOADING),
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_ITER_LOADING),
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_ITER_U_MCU_LOADING));
	}

	// LB async ratio on EB
	if (eb_policy_state == GED_DVFS_LOADING_BASE_COMMIT) {
		apply_lb_async = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_APPLY_LB_ASYNC);
		if (apply_lb_async) {
			trace_tracing_mark_write(5566, "async_perf_high",
				mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_PERF_IMPROVE));
		}
	}
	if (eb_policy_state != GED_DVFS_FRAME_BASE_COMMIT) {
		trace_tracing_mark_write(5566, "async_opp_diff",
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_ASYNC_OPP_DIFF));
	}

	if (eb_policy_state == GED_DVFS_LOADING_BASE_COMMIT)
		eb_timeout_value = lb_timeout;
	else if (eb_policy_state == GED_DVFS_FRAME_BASE_COMMIT)
		eb_timeout_value = fb_timeout;

	ged_set_backup_timer_timeout(eb_timeout_value);

	pre_eb_policy_state = eb_policy_state;
	pre_ged_policy_state = ged_policy_state;
	pre_freq_id = freq_id;
#endif
}

static unsigned long long sw_vsync_ts;
static void ged_notify_sw_sync_work_handle(struct work_struct *psWork)
{
	struct GED_NOTIFY_SW_SYNC *psNotify =
		GED_CONTAINER_OF(psWork, struct GED_NOTIFY_SW_SYNC, sWork);
	unsigned long long temp;
	GED_DVFS_COMMIT_TYPE eCommitType;
	u64 timeout_value;
	/*only one policy at a time*/
	if (psNotify) {
		mutex_lock(&gsPolicyLock);
		timeout_value = lb_timeout;
		psNotify->bUsed = false;

		if (hrtimer_get_remaining(&g_HT_hwvsync_emu) < 0) {
			enum gpu_dvfs_policy_state policy_state;

			if (!is_fdvfs_enable()) {
				policy_state = ged_get_policy_state();
				if (policy_state == POLICY_STATE_FB ||
						policy_state == POLICY_STATE_FB_FALLBACK ||
						policy_state == POLICY_STATE_LB_FALLBACK ||
						policy_state == POLICY_STATE_FORCE_LB_FALLBACK) {
					eCommitType = GED_DVFS_FALLBACK_COMMIT;
					if (policy_state == POLICY_STATE_LB_FALLBACK)
						ged_set_policy_state(POLICY_STATE_LB_FALLBACK);
					else if (policy_state == POLICY_STATE_FORCE_LB_FALLBACK)
						ged_set_policy_state(POLICY_STATE_FORCE_LB_FALLBACK);
					else
						ged_set_policy_state(POLICY_STATE_FB_FALLBACK);
					timeout_value = ged_get_fallback_time();
				} else {
					eCommitType = GED_DVFS_LOADING_BASE_COMMIT;
				}
				ged_set_backup_timer_timeout(timeout_value);   // set init value

				temp = 0;
				/* if callback is queued, send mode off to real driver */
				ged_sw_vsync_event(false);
#ifdef ENABLE_TIMER_BACKUP
				temp = ged_get_time();
				if (temp-sw_vsync_ts > GED_DVFS_TIMER_TIMEOUT) {
					do_div(temp, 1000);
					psNotify->t = temp;
					ged_dvfs_run(psNotify->t, psNotify->phase,
						psNotify->ul3DFenceDoneTime, eCommitType);
					ged_log_buf_print(ghLogBuf_DVFS,
						"[GED_K] Timer kicked	(ts=%llu) ", temp);
				} else {
					ged_log_buf_print(ghLogBuf_DVFS,
						"[GED_K] Timer kick giveup (ts=%llu)", temp);
				}
#endif
			hrtimer_start(&g_HT_hwvsync_emu,
				ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);

			} else if (ged_timer_or_trace_enable()) {
				ged_eb_dvfs_trace_dump();
				hrtimer_start(&g_HT_hwvsync_emu,
					ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			}
		}
		mutex_unlock(&gsPolicyLock);
	}
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	/* set initial idle time to 5ms if runtime policy stay default flavor */
	if (ged_kpi_is_fw_idle_policy_enable() == -1)
		mtk_set_gpu_idle(5);
#endif /* MTK_GPU_FW_IDLE */
}

#define GED_VSYNC_MISS_QUANTUM_NS 16666666

#ifdef ENABLE_COMMON_DVFS
static unsigned long long hw_vsync_ts;
#endif

static unsigned long long g_ns_gpu_off_ts;

static bool g_timer_on;
static unsigned long long g_timer_on_ts;
static bool g_bGPUClock;

/*
 * void timer_switch(bool bTock)
 * only set the staus, not really operating on real timer
 */
void timer_switch(bool bTock)
{
	mutex_lock(&gsVsyncStampLock);
	g_timer_on = bTock;
	if (bTock)
		g_timer_on_ts = ged_get_time();
	mutex_unlock(&gsVsyncStampLock);
}

void timer_switch_locked(bool bTock)
{
	g_timer_on = bTock;
	if (bTock)
		g_timer_on_ts = ged_get_time();
}

static void ged_timer_switch_work_handle(struct work_struct *psWork)
{
	struct GED_NOTIFY_SW_SYNC *psNotify =
		GED_CONTAINER_OF(psWork, struct GED_NOTIFY_SW_SYNC, sWork);
	if (psNotify) {
		ged_sw_vsync_event(false);

		psNotify->bUsed = false;
	}
}


void ged_set_backup_timer_timeout(u64 time_out)
{
	if (time_out != 0 && time_out < GED_DVFS_FB_TIMER_TIMEOUT)
		g_fallback_time_out = time_out;
	else
		g_fallback_time_out = GED_DVFS_FB_TIMER_TIMEOUT;
}


void ged_cancel_backup_timer(void)
{
	unsigned long long temp;

	temp = ged_get_time();
#ifdef ENABLE_TIMER_BACKUP
	if ((g_ged_frame_base_optimize == 0 || g_bGPUClock) && ged_timer_or_trace_enable()) {
		if (hrtimer_try_to_cancel(&g_HT_hwvsync_emu)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_hwvsync_emu);
			hrtimer_start(&g_HT_hwvsync_emu,
				ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer Restart (ts=%llu)", temp);
		} else {
			/*
			 * Timer is not existed
			 */
			hrtimer_start(&g_HT_hwvsync_emu,
				ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] New Timer Start (ts=%llu)", temp);
			timer_switch_locked(true);
		}
	}
#endif /*	#ifdef ENABLE_TIMER_BACKUP	*/
}


GED_ERROR ged_notify_sw_vsync(GED_VSYNC_TYPE eType,
	struct GED_DVFS_UM_QUERY_PACK *psQueryData)
{
	ged_notification(GED_NOTIFICATION_TYPE_SW_VSYNC);

	{
#ifdef ENABLE_COMMON_DVFS

	unsigned long long temp;
	unsigned long ul3DFenceDoneTime;


	psQueryData->bFirstBorn = ged_sw_vsync_event(true);

	ul3DFenceDoneTime = ged_monitor_3D_fence_done_time();

	psQueryData->ul3DFenceDoneTime = ul3DFenceDoneTime;

	hw_vsync_ts = temp = ged_get_time();

	if (g_gpu_timer_based_emu) {
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] Vsync ignored (ts=%llu)", temp);
		return GED_ERROR_INTENTIONAL_BLOCK;
	}

/* TODO: temp defined to disable vsync_dvfs when COMMON_DVFS on*/
#ifdef ENABLE_COMMON_DVFS
	return GED_ERROR_INTENTIONAL_BLOCK;
#else
	long phase = 0;
	unsigned long t;
	bool bHWEventKick = false;
	long long llDiff = 0;

	/*critical session begin*/
	mutex_lock(&gsVsyncStampLock);

	if (eType == GED_VSYNC_SW_EVENT) {
		sw_vsync_ts = temp;
#ifdef ENABLE_TIMER_BACKUP
		if (hrtimer_try_to_cancel(&g_HT_hwvsync_emu)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_hwvsync_emu);
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer Restart (ts=%llu)", temp);
		} else {
			/*
			 * Timer is not existed
			 */
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] New Timer Start (ts=%llu)", temp);
			timer_switch_locked(true);
		}
#endif // #ifdef ENABLE_TIMER_BACKUP
	} else {
		hw_vsync_ts = temp;

		llDiff = (long long)(hw_vsync_ts - sw_vsync_ts);

		if (llDiff > GED_VSYNC_MISS_QUANTUM_NS)
			bHWEventKick = true;
	}
#ifdef GED_DVFS_DEBUG
	if (eType == GED_VSYNC_HW_EVENT)
		GED_LOGD("HW VSYNC: llDiff=",
		"%lld, hw_vsync_ts=%llu, sw_vsync_ts=%llu\n", llDiff,
		hw_vsync_ts, sw_vsync_ts);
	else
		GED_LOGD("SW VSYNC: llDiff=",
		"%lld, hw_vsync_ts=%llu, sw_vsync_ts=%llu\n", llDiff,
		hw_vsync_ts, sw_vsync_ts);
#endif		///	#ifdef GED_DVFS_DEBUG


	if (eType == GED_VSYNC_HW_EVENT)
		ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] HW VSYNC (ts=%llu) ", hw_vsync_ts);
	else
		ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] SW VSYNC (ts=%llu) ", sw_vsync_ts);

	mutex_unlock(&gsVsyncStampLock);
	/*critical session end*/

	if (eType == GED_VSYNC_SW_EVENT) {
		do_div(temp, 1000);
		t = (unsigned long)(temp);

		// for some cases just align vsync to FenceDoneTime
		if (ul3DFenceDoneTime > t) {
			if (ul3DFenceDoneTime - t < GED_DVFS_DIFF_THRESHOLD)
				t = ul3DFenceDoneTime;
		}
		psQueryData->usT = t;
		ged_dvfs_run(t, phase, ul3DFenceDoneTime,
			GED_DVFS_LOADING_BASE_COMMIT);
		ged_dvfs_sw_vsync_query_data(psQueryData);
	} else {
		if (bHWEventKick) {
#ifdef GED_DVFS_DEBUG
			GED_LOGD("HW Event: kick!\n");
#endif							/// GED_DVFS_DEBUG
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] HW VSync: mending kick!");
			ged_dvfs_run(0, 0, 0, 0);
		}
	}
#endif
#else
	unsigned long long temp;

	temp = ged_get_time();
	ged_sw_vsync_event(true);
	return GED_ERROR_INTENTIONAL_BLOCK;
#endif

	return GED_OK;
	}
}


extern unsigned int gpu_loading;
enum hrtimer_restart ged_sw_vsync_check_cb(struct hrtimer *timer)
{
	unsigned long long temp;
	long long llDiff;
	unsigned int loading_mode = 0;
	unsigned int gpu_loading_temp = 0;
	struct GED_NOTIFY_SW_SYNC *psNotify;
	struct GpuUtilization_Ex util_ex;

	memset(&util_ex, 0, sizeof(util_ex));

	temp = cpu_clock(smp_processor_id());

	llDiff = (long long)(temp - sw_vsync_ts);

	if (llDiff > GED_VSYNC_MISS_QUANTUM_NS) {
		psNotify = &(loading_base_notify[((notify_index++) % MAX_NOTIFY_CNT)]);

		if (notify_index >= MAX_NOTIFY_CNT)
			notify_index = 0;

#ifndef ENABLE_TIMER_BACKUP
		ged_dvfs_cal_gpu_utilization_ex(&gpu_av_loading,
			&gpu_block, &gpu_idle, &util_ex);
		gpu_loading = gpu_av_loading;
#endif

		if (is_fdvfs_enable()) {
			gpu_loading_temp = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_GPU_LOADING);
		} else {
			ged_get_gpu_utli_ex(&util_ex);
			mtk_get_dvfs_loading_mode(&loading_mode);
			if (loading_mode == LOADING_MAX_3DTA_COM) {
				gpu_loading_temp =
				MAX(util_ex.util_3d, util_ex.util_ta) +
				util_ex.util_compute;
			} else if (loading_mode == LOADING_MAX_3DTA) {
				gpu_loading_temp =
				MAX(util_ex.util_3d, util_ex.util_ta);
			} else if (loading_mode == LOADING_ITER) {
				gpu_loading_temp = util_ex.util_iter;
			} else if (loading_mode == LOADING_MAX_ITERMCU) {
				gpu_loading_temp = MAX(util_ex.util_iter, util_ex.util_mcu);
			} else {   // LOADING_ACTIVE or unknown mode
				gpu_loading_temp = util_ex.util_active;
			}
		}

		if (false == g_bGPUClock && 0 == gpu_loading_temp
			&& (temp - g_ns_gpu_on_ts > GED_DVFS_TIMER_TIMEOUT)) {
			if (psNotify && psNotify->bUsed == false) {
				psNotify->bUsed = true;
				INIT_WORK(&psNotify->sWork,
					ged_timer_switch_work_handle);
				queue_work(g_psNotifyWorkQueue,
					&psNotify->sWork);
				timer_switch_locked(false);
			}
#ifdef GED_DVFS_DEBUG
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer removed	(ts=%llu) ", temp);
#endif
			return HRTIMER_NORESTART;
		}

		if (psNotify) {
			if (psNotify->bUsed == false && ged_kpi_enabled()) {
				psNotify->bUsed = true;
				INIT_WORK(&psNotify->sWork,
					ged_notify_sw_sync_work_handle);
				psNotify->phase = GED_DVFS_TIMER_BACKUP;
				psNotify->ul3DFenceDoneTime = 0;
				queue_work(g_psNotifyWorkQueue, &psNotify->sWork);
#ifdef GED_DVFS_DEBUG
				ged_log_buf_print(ghLogBuf_DVFS,
					"[GED_K] Timer queue to kick (ts=%llu)", temp);
#endif
			}
			g_timer_on_ts = temp;
		}
	}
	return HRTIMER_NORESTART;
}

#if IS_ENABLED(CONFIG_MTK_GPU_POWER_ON_OFF_TEST)
unsigned int ged_gpu_power_stress_test_enable(void)
{
	return g_ged_power_stress_test_support;
}
EXPORT_SYMBOL(ged_gpu_power_stress_test_enable);
#endif /* MTK_GPU_POWER_ON_OFF_TEST */

unsigned int ged_gpu_whitebox_power_test_support(int support_flag)
{
	g_whitebox_support_flag = support_flag;

	return g_whitebox_support_flag;
}
EXPORT_SYMBOL(ged_gpu_whitebox_power_test_support);

unsigned int ged_gpu_whitebox_power_test_case(int replace)
{
	mcu_replace = replace;

	return mcu_replace;
}
EXPORT_SYMBOL(ged_gpu_whitebox_power_test_case);

unsigned int ged_get_whitebox_power_test_support(void)
{
	return g_whitebox_support_flag;
}
EXPORT_SYMBOL(ged_get_whitebox_power_test_support);


unsigned int ged_get_whitebox_power_test_case(void)
{
	return mcu_replace;
}
EXPORT_SYMBOL(ged_get_whitebox_power_test_case);


unsigned int ged_get_whitebox_power_test_case_clear(void)
{
	mcu_replace = 0;

	return mcu_replace;
}
EXPORT_SYMBOL(ged_get_whitebox_power_test_case_clear);


void ged_set_whitebox_power_state_store(int first, int second)
{
	if (first >= 0 && second >= 0)
		stat_mcu_store[first][second]++;
}
EXPORT_SYMBOL(ged_set_whitebox_power_state_store);

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
unsigned int ged_gpu_apo_support(void)
{
	return g_ged_apo_support;
}
EXPORT_SYMBOL(ged_gpu_apo_support);

unsigned long long ged_get_apo_thr_ns(void)
{
	return g_apo_thr_ns;
}
EXPORT_SYMBOL(ged_get_apo_thr_ns);

void ged_set_apo_thr_ns(unsigned long long apo_thr_ns)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_apo_thr_ns = apo_thr_ns;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_thr_ns);

unsigned long long ged_get_apo_wakeup_ns(void)
{
	return g_apo_wakeup_ns;
}
EXPORT_SYMBOL(ged_get_apo_wakeup_ns);

void ged_set_apo_wakeup_ns_nolock(unsigned long long apo_wakeup_ns)
{
	g_apo_wakeup_ns = apo_wakeup_ns;
}
EXPORT_SYMBOL(ged_set_apo_wakeup_ns_nolock);

void ged_set_apo_wakeup_ns(unsigned long long apo_wakeup_ns)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	ged_set_apo_wakeup_ns_nolock(apo_wakeup_ns);

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_wakeup_ns);

unsigned long long ged_get_apo_lp_thr_ns(void)
{
	return g_apo_lp_thr_ns;
}
EXPORT_SYMBOL(ged_get_apo_lp_thr_ns);

void ged_set_apo_lp_thr_ns(unsigned long long apo_lp_thr_ns)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_apo_lp_thr_ns = apo_lp_thr_ns;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_lp_thr_ns);

void ged_set_all_apo_thr_ns_nolock(unsigned long long apo_thr_ns)
{
	g_apo_thr_ns = apo_thr_ns;
	g_apo_wakeup_ns = apo_thr_ns + 1000000;
}
EXPORT_SYMBOL(ged_set_all_apo_thr_ns_nolock);

int ged_get_apo_hint(void)
{
	return g_apo_hint;
}
EXPORT_SYMBOL(ged_get_apo_hint);

int ged_get_apo_force_hint(void)
{
	return g_apo_force_hint;
}
EXPORT_SYMBOL(ged_get_apo_force_hint);

void ged_set_apo_force_hint(int apo_force_hint)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_apo_force_hint = apo_force_hint;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_force_hint);

int ged_get_apo_autosuspend_delay_ref_count(void)
{
	return g_apo_autosuspend_delay_ref_count;
}
EXPORT_SYMBOL(ged_get_apo_autosuspend_delay_ref_count);

void ged_set_apo_autosuspend_delay_ctrl(int ctrl)
{
	g_apo_autosuspend_delay_ctrl = ctrl;
}
EXPORT_SYMBOL(ged_set_apo_autosuspend_delay_ctrl);

int ged_get_apo_autosuspend_delay_target_ref_count(void)
{
	return g_apo_autosuspend_delay_target_ref_count;
}
EXPORT_SYMBOL(ged_get_apo_autosuspend_delay_target_ref_count);

void ged_set_apo_autosuspend_delay_target_ref_count(int apo_autosuspend_delay_target_ref_count)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_apo_autosuspend_delay_target_ref_count = apo_autosuspend_delay_target_ref_count;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_autosuspend_delay_target_ref_count);

unsigned int ged_get_apo_autosuspend_delay_ms(void)
{
	return g_apo_autosuspend_delay_ms;
}
EXPORT_SYMBOL(ged_get_apo_autosuspend_delay_ms);

void ged_set_apo_autosuspend_delay_ms_ref_idletime_nolock(long long idle_time)
{
	/* autosuspend_delay setting */
	if (g_apo_autosuspend_delay_ctrl == 0) {
		if (g_apo_legacy == GED_APO_LEGACY_VER1) {
			if (g_gpu_frame_time_ns >= g_ged_gpu_frame_time[2].target)
				g_apo_autosuspend_delay_ms = GED_APO_AUTOSUSPEND_DELAY_MS;
			else
				g_apo_autosuspend_delay_ms = GED_APO_AUTOSUSPEND_DELAY_MAX_MS;
		} else {
			if ((g_gpu_frame_time_ns >= g_ged_gpu_frame_time[2].target) &&
				(g_gpu_frame_time_ns <= g_ged_gpu_frame_time[3].target)) {
				if (idle_time > (long long)(div_u64(g_gpu_frame_time_ns, 2) + 1000000))
					g_apo_autosuspend_delay_ms = 0;
				else
					g_apo_autosuspend_delay_ms = GED_APO_AUTOSUSPEND_DELAY_MS;
			} else {
				if (idle_time > (long long)(div_u64(g_gpu_frame_time_ns, 2) + 1000000))
					g_apo_autosuspend_delay_ms = GED_APO_AUTOSUSPEND_DELAY_MS;
				else
					g_apo_autosuspend_delay_ms = GED_APO_AUTOSUSPEND_DELAY_HFR_MS;
			}
		}
	}
}
EXPORT_SYMBOL(ged_set_apo_autosuspend_delay_ms_ref_idletime_nolock);

void ged_set_apo_autosuspend_delay_ms_ref_idletime(long long idle_time)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	ged_set_apo_autosuspend_delay_ms_ref_idletime_nolock(idle_time);

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_autosuspend_delay_ms_ref_idletime);

void ged_set_apo_autosuspend_delay_ms_nolock(unsigned int apo_autosuspend_delay_ms)
{
	g_apo_autosuspend_delay_ms = apo_autosuspend_delay_ms;
}
EXPORT_SYMBOL(ged_set_apo_autosuspend_delay_ms_nolock);

void ged_set_apo_autosuspend_delay_ms(unsigned int apo_autosuspend_delay_ms)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	ged_set_apo_autosuspend_delay_ms_nolock(apo_autosuspend_delay_ms);

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_autosuspend_delay_ms);

void ged_get_gpu_frame_time(int frame_time)
{
	/* initialization */
	g_gpu_frame_time_ns = g_ged_gpu_frame_time[0].target;

	for (int i = 0; i < GED_FRAME_TIME_CONFIG_NUM; i++) {
		if (frame_time <= g_ged_gpu_frame_time[i].margin) {
			g_gpu_frame_time_ns = g_ged_gpu_frame_time[i].target;
			break;
		}
	}
	trace_GPU_Power__Policy__APO_Frame_Time(g_gpu_frame_time_ns);
}
EXPORT_SYMBOL(ged_get_gpu_frame_time);


void ged_set_apo_status(int apo_status)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_ged_apo_support = apo_status;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_status);

void ged_set_apo_legacy(enum ged_apo_legacy apo_legacy)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_apo_legacy = apo_legacy;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_apo_legacy);

enum ged_apo_legacy ged_get_apo_legacy(void)
{
	return g_apo_legacy;
}
EXPORT_SYMBOL(ged_get_apo_legacy);

void ged_get_active_time(void)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_ns_gpu_A_ts = ged_get_time();

	if (g_ns_gpu_I_ts > 0) {
		g_ns_gpu_I_to_A_duration =
			g_ns_gpu_A_ts - g_ns_gpu_I_ts;
	} else
		g_ns_gpu_I_to_A_duration = 0;

	g_ns_gpu_I_ts = 0;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_get_active_time);

void ged_get_idle_time(void)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	if (g_ns_gpu_I_ts == 0)
		g_ns_gpu_I_ts = ged_get_time();

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_get_idle_time);


bool ged_gpu_is_heavy(void)
{
	unsigned int gpu_loading = 0;
	int cur_oppidx = ged_get_cur_oppidx();
	int ui32CeilingID = ged_get_cur_limit_idx_ceil();

	mtk_get_gpu_loading(&gpu_loading);

	return ((gpu_loading >= 85) && (cur_oppidx <= ui32CeilingID));
}
EXPORT_SYMBOL(ged_gpu_is_heavy);

void ged_check_power_duration(void)
{
	unsigned long ulIRQFlags;
	bool bforce = false;
	bool bLast_I_to_A = false;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	/* Condition-1 */
	bforce = ged_gpu_is_heavy();
	if (true == bforce)
		goto direct_check;

	if (g_ns_gpu_I_to_A_duration <= 0) { // directly return when L2 off as GPU sleep to GPU power-down
		spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
		return;
	}

	/* Condition */
	bLast_I_to_A = g_ns_gpu_I_to_A_duration < g_apo_thr_ns;

direct_check:
	if (bforce || bLast_I_to_A)
		g_bGPUAPO = true;
	else {
		if (g_apo_thr_ns == 0) {
			if (g_ged_apo_support == APO_NORMAL_AND_LP_SUPPORT &&
				(g_ns_gpu_I_to_A_duration > g_apo_thr_ns) &&
				(g_ns_gpu_I_to_A_duration < g_apo_lp_thr_ns))
				g_apo_hint = APO_LP_HINT;
			else
				g_apo_hint = APO_NORMAL_HINT;

			if (g_apo_force_hint >= APO_NORMAL_HINT &&
				g_apo_force_hint < APO_INVALID_HINT)
				g_apo_hint = g_apo_force_hint;

			ged_write_sysram_pwr_hint(g_apo_hint);
		}

		ged_gpu_apo_reset_nolock();
	}

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_check_power_duration);

unsigned long long ged_get_power_duration(void)
{
	return g_ns_gpu_I_to_A_duration;
}
EXPORT_SYMBOL(ged_get_power_duration);

void ged_gpu_apo_init_nolock(void)
{
	g_bGPUAPO = false;
	g_ns_gpu_A_ts = 0;
	g_ns_gpu_I_ts = 0;
	g_ns_gpu_I_to_A_duration = 0;
}
EXPORT_SYMBOL(ged_gpu_apo_init_nolock);

void ged_gpu_apo_reset_nolock(void)
{
	g_bGPUAPO = false;
	g_ns_gpu_I_to_A_duration = 0;
}
EXPORT_SYMBOL(ged_gpu_apo_reset_nolock);

void ged_gpu_apo_reset(void)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	ged_gpu_apo_reset_nolock();

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_gpu_apo_reset);

bool ged_gpu_apo_notify(void)
{
	if (g_ged_apo_support) {
		ged_check_power_duration();
		return g_bGPUAPO;
	} else
		return false;
}
EXPORT_SYMBOL(ged_gpu_apo_notify);

void ged_get_predict_active_time(void)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	g_ns_gpu_predict_prevA_ts = g_ns_gpu_predict_A_ts;
	g_ns_gpu_predict_A_ts = ged_get_time();
	trace_GPU_Power__Policy__APO(0);
	trace_GPU_Power__Policy__APO__IdleActive(0);

	if (g_ns_gpu_predict_I_ts > 0) {
		g_ns_gpu_predict_I_to_A_duration =
			g_ns_gpu_predict_A_ts - g_ns_gpu_predict_I_ts;
	} else
		g_ns_gpu_predict_I_to_A_duration = 0;

	g_ns_gpu_predict_prev_A_to_A_duration = g_ns_gpu_predict_A_to_A_duration;

	if (g_ns_gpu_predict_prevA_ts > 0) {
		g_ns_gpu_predict_A_to_A_duration =
			g_ns_gpu_predict_A_ts - g_ns_gpu_predict_prevA_ts;
	} else
		g_ns_gpu_predict_A_to_A_duration = 0;

	g_ns_gpu_predict_I_ts = 0;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_get_predict_active_time);

void ged_get_predict_idle_time(void)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	if (g_ns_gpu_predict_I_ts == 0) {
		g_ns_gpu_predict_I_ts = ged_get_time();
		trace_GPU_Power__Policy__APO__IdleActive(1);

		if (g_ns_gpu_predict_A_ts > 0) {
			g_ns_gpu_predict_A_to_I_duration =
				g_ns_gpu_predict_I_ts - g_ns_gpu_predict_A_ts;
		} else
			g_ns_gpu_predict_A_to_I_duration = 0;
	}

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_get_predict_idle_time);

bool ged_check_predict_power_autosuspend_nolock(void)
{
	long long llDiff = 0;
	bool bPredict_current_I_to_A = true;

	if (g_ged_apo_support == APO_2_0_NORMAL_SUPPORT) {
		/* Condition-1 */
		if (g_ns_gpu_predict_A_to_A_duration > 0 &&
			g_ns_gpu_predict_prev_A_to_A_duration > 0) {
			// Not predict idle when A_to_A is too short.
			if (!((g_ns_gpu_predict_A_to_A_duration < div_u64(g_apo_wakeup_ns, 2)) ||
				(g_ns_gpu_predict_prev_A_to_A_duration < div_u64(g_apo_wakeup_ns, 2)) ||
				(g_ns_gpu_predict_A_to_A_duration < g_apo_wakeup_ns &&
				g_ns_gpu_predict_prev_A_to_A_duration < g_apo_wakeup_ns))) {
				// Jobs in one frame or similar power-durations
				if (((g_ns_gpu_predict_A_to_A_duration +
					g_ns_gpu_predict_prev_A_to_A_duration) <= g_gpu_frame_time_ns) ||
					abs(g_ns_gpu_predict_A_to_A_duration -
						g_ns_gpu_predict_prev_A_to_A_duration) <
					min(g_ns_gpu_predict_A_to_A_duration,
						g_ns_gpu_predict_prev_A_to_A_duration)) {
					trace_GPU_Power__Policy__APO_irregular(0);
					// Calculate for autosuspend_delay setting
					llDiff = (long long)(g_ns_gpu_predict_prev_A_to_A_duration -
							g_ns_gpu_predict_A_to_I_duration);
					bPredict_current_I_to_A = llDiff < (long long)g_apo_thr_ns;
				} else {
					// set "false" when power-durations are irregular.
					bPredict_current_I_to_A = false;
					trace_GPU_Power__Policy__APO_irregular(1);
				}
			}
		}

		trace_GPU_Power__Policy__APO__Predicted_Idle_Time(div_u64(llDiff, 1000));

		/* autosuspend_delay setting */
		ged_set_apo_autosuspend_delay_ms_ref_idletime_nolock(llDiff);
	}

	return bPredict_current_I_to_A;
}
EXPORT_SYMBOL(ged_check_predict_power_autosuspend_nolock);

bool ged_check_predict_power_autosuspend(void)
{
	bool bPredict_current_I_to_A = true;
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	bPredict_current_I_to_A = ged_check_predict_power_autosuspend_nolock();

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);

	return bPredict_current_I_to_A;
}
EXPORT_SYMBOL(ged_check_predict_power_autosuspend);

void ged_check_predict_power_duration(void)
{
	unsigned long ulIRQFlags;
	bool bPredict_force = false;
	bool bPredict_current_I_to_A = true; /* Default set "true" to discard */
	bool bPredict_last_I_to_A = false;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	/* Condition-1 */
	bPredict_force = ged_gpu_is_heavy();
	if (true == bPredict_force) {
		ged_set_apo_wakeup_ns_nolock(GED_APO_LONG_WAKEUP_THR_NS);
		goto direct_check;
	} else
		ged_set_apo_wakeup_ns_nolock(GED_APO_WAKEUP_THR_NS);

	//Directly return when glb_idle_irq is received as GPU sleep to GPU power-down
	if (g_ns_gpu_predict_A_to_I_duration <= 0) {
		spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
		return;
	}

	/* Condition-2 */
	bPredict_current_I_to_A = ged_check_predict_power_autosuspend_nolock();

	/* Condition-3 */
	bPredict_last_I_to_A = ((g_ns_gpu_predict_I_to_A_duration > 0) &&
		(g_ns_gpu_predict_I_to_A_duration < g_apo_thr_ns));
	trace_GPU_Power__Policy__APO_cond_1(bPredict_current_I_to_A);
	trace_GPU_Power__Policy__APO_cond_2(bPredict_last_I_to_A);

direct_check:
	if (bPredict_force ||
		(bPredict_current_I_to_A && bPredict_last_I_to_A))
		g_bGPUPredictAPO = true;
	else {
		if (g_ged_apo_support == APO_NORMAL_AND_LP_SUPPORT &&
			(g_ns_gpu_predict_I_to_A_duration >= g_apo_thr_ns) &&
			(g_ns_gpu_predict_I_to_A_duration < g_apo_lp_thr_ns))
			g_apo_hint = APO_LP_HINT;
		else
			g_apo_hint = APO_NORMAL_HINT;

		if (g_apo_force_hint >= APO_NORMAL_HINT &&
			g_apo_force_hint < APO_INVALID_HINT)
			g_apo_hint = g_apo_force_hint;

		ged_write_sysram_pwr_hint(g_apo_hint);

		ged_gpu_predict_apo_reset_nolock();
		ged_gpu_apo_reset_nolock();
	}

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_check_predict_power_duration);

unsigned long long ged_get_predict_power_duration(void)
{
	return g_ns_gpu_predict_I_to_A_duration;
}
EXPORT_SYMBOL(ged_get_predict_power_duration);

void ged_gpu_predict_apo_init_nolock(void)
{
	g_bGPUPredictAPO = false;
	g_ns_gpu_predict_prevA_ts = 0;
	g_ns_gpu_predict_A_ts = 0;
	g_ns_gpu_predict_I_ts = 0;
	g_ns_gpu_predict_A_to_I_duration = 0;
	g_ns_gpu_predict_I_to_A_duration = 0;
	g_ns_gpu_predict_A_to_A_duration = 0;
	g_ns_gpu_predict_prev_A_to_A_duration = 0;
}
EXPORT_SYMBOL(ged_gpu_predict_apo_init_nolock);

void ged_gpu_predict_apo_reset_nolock(void)
{
	g_bGPUPredictAPO = false;
	g_ns_gpu_predict_A_to_I_duration = 0;
	g_ns_gpu_predict_I_to_A_duration = 0;
}
EXPORT_SYMBOL(ged_gpu_predict_apo_reset_nolock);

void ged_gpu_predict_apo_reset(void)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);

	ged_gpu_predict_apo_reset_nolock();
	trace_GPU_Power__Policy__APO(0);

	g_apo_hint = APO_NORMAL_HINT;

	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_gpu_predict_apo_reset);

bool ged_gpu_predict_apo_notify(void)
{
	if (g_ged_apo_support) {
		ged_check_predict_power_duration();
		trace_GPU_Power__Policy__APO(g_bGPUPredictAPO);
		return g_bGPUPredictAPO;
	} else
		return false;
}
EXPORT_SYMBOL(ged_gpu_predict_apo_notify);
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

int ged_get_autosuspend_stress(void)
{
	return g_autosuspend_stress;
}
EXPORT_SYMBOL(ged_get_autosuspend_stress);

void ged_set_autosuspend_stress(int enable)
{
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&g_sApoLock, ulIRQFlags);
	g_autosuspend_stress = enable;
	spin_unlock_irqrestore(&g_sApoLock, ulIRQFlags);
}
EXPORT_SYMBOL(ged_set_autosuspend_stress);

unsigned long long ged_get_power_on_timestamp(void)
{
	return g_ns_gpu_on_ts;
}

/* MBrain */
static void ged_dvfs_update_power_state_time(
		enum ged_gpu_power_state power_state)
{
	u64 delta_time = 0;
	struct timespec64 tv = {0};

	g_last_pwr_ts = ged_get_time();
	pwr_state_time[g_curr_pwr_state].end_ts = g_last_pwr_ts;

	ktime_get_real_ts64(&tv);
	g_last_pwr_update_ts_ms = tv.tv_sec * 1000 + div_u64(tv.tv_nsec, 1000000);

	// Check to prevent overflow error
	if (pwr_state_time[g_curr_pwr_state].end_ts >=
			pwr_state_time[g_curr_pwr_state].start_ts) {
		delta_time = pwr_state_time[g_curr_pwr_state].end_ts -
			pwr_state_time[g_curr_pwr_state].start_ts;
		pwr_state_time[g_curr_pwr_state].accumulate_time +=
			div_u64(delta_time, 1000000);
	} else {
		delta_time = (ULLONG_MAX -
				pwr_state_time[g_curr_pwr_state].start_ts) +
			pwr_state_time[g_curr_pwr_state].end_ts;
		pwr_state_time[g_curr_pwr_state].accumulate_time +=
			div_u64(delta_time, 1000000);
	}

	// Reset power state timestamps
	pwr_state_time[g_curr_pwr_state].start_ts = 0;
	pwr_state_time[power_state].start_ts =
		pwr_state_time[g_curr_pwr_state].end_ts;
	g_curr_pwr_state = power_state;
}
/* MBrain end */

void ged_dvfs_gpu_clock_switch_notify(enum ged_gpu_power_state power_state)
{
	enum gpu_dvfs_policy_state policy_state;

	policy_state = ged_get_policy_state();

	if (power_state == GED_POWER_ON) {
		g_ns_gpu_on_ts = ged_get_time();
		g_bGPUClock = true;
		/* check overdue if timestamp is queuebuffer(0x2) */
		if (g_ged_frame_base_optimize &&
			(policy_state == POLICY_STATE_FB ||
			policy_state == POLICY_STATE_FB_FALLBACK) &&
			ged_kpi_get_fb_ulMask() == 0x2) {
			unsigned long long uncomplete_time =
				g_ns_gpu_on_ts - ged_kpi_get_fb_timestamp();
			/* get in fallback if uncomplete time is overdue */
			if (uncomplete_time >= fb_timeout) {
				if (is_fdvfs_enable() == 0) {
					ged_set_policy_state(POLICY_STATE_FB_FALLBACK);
					ged_set_backup_timer_timeout(ged_get_fallback_time());
				}
			} else {
				u64 fb_tmp_timeout = fb_timeout - uncomplete_time;
				u64 timeout_val = ged_get_fallback_time();
				/* keep minimum value of fallback time*/
				if (fb_tmp_timeout < timeout_val)
					fb_tmp_timeout = timeout_val;
				ged_set_policy_state(POLICY_STATE_FB);
				ged_eb_dvfs_task(EB_UPDATE_POLICY_STATE, GED_DVFS_FRAME_BASE_COMMIT);
				ged_set_backup_timer_timeout(fb_tmp_timeout);

			}
		}
		if (g_timer_on) {
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer Already Start");
		} else if (ged_timer_or_trace_enable()) {
			hrtimer_start(&g_HT_hwvsync_emu,
				ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] HW Start Timer");
			timer_switch(true);
		}
	} else if (power_state == GED_POWER_OFF ||
			power_state == GED_SLEEP) {
		g_ns_gpu_off_ts = ged_get_time();
		g_bGPUClock = false;
		if (g_ged_frame_base_optimize &&
			(policy_state == POLICY_STATE_FB ||
			 policy_state == POLICY_STATE_FB_FALLBACK ||
			 ged_timer_or_trace_enable())) {
			int timer_flag = 0;
			if (hrtimer_try_to_cancel(&g_HT_hwvsync_emu)) {
				/* frame base pass power off timer*/
				hrtimer_cancel(&g_HT_hwvsync_emu);
				timer_flag = 1;
			}
			/* avoid multi lock*/
			if (timer_flag)
				timer_switch(false);
		}
		ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Buck-off");
		ged_dvfs_notify_power_off();
	}
	// Update power on/off state
	trace_tracing_mark_write(5566, "gpu_state", power_state);
	//MBrain
	ged_dvfs_update_power_state_time(power_state);
}
EXPORT_SYMBOL(ged_dvfs_gpu_clock_switch_notify);

/* MBrain */
int ged_dvfs_query_power_state_time(u64 *off_time, u64 *idle_time,
		u64 *on_time, u64 *last_ts)
{
	if (off_time == NULL || idle_time == NULL || on_time == NULL ||
		last_ts == NULL) {
		WARN(1, "Invalid parameters");
		return -EINVAL;
	}

	*last_ts = g_last_pwr_update_ts_ms;
	*off_time = pwr_state_time[GED_POWER_OFF].accumulate_time;
	*idle_time = pwr_state_time[GED_SLEEP].accumulate_time;
	*on_time = pwr_state_time[GED_POWER_ON].accumulate_time;

	return 0;
}
EXPORT_SYMBOL(ged_dvfs_query_power_state_time);
/* MBrain end */

void ged_gpu_autosuspend_timeout_notify(int autosuspend_timeout_ms)
{
	trace_GPU_Power__Policy__APO__AST(autosuspend_timeout_ms);
}
EXPORT_SYMBOL(ged_gpu_autosuspend_timeout_notify);


/* Check PM callback state in Kbase */
int check_pm_callback_state(enum ged_gpu_power_state power_state)
{
	/* check now pm_callback state */
	if (atomic_read(&trigger_pm_callback_state) == power_state)
		return 1;
	else
		atomic_set(&trigger_pm_callback_state, power_state);

	return 0;
}
EXPORT_SYMBOL(check_pm_callback_state);


/* If power transition timeout, dump info */
void dump_pm_callback_kbase_info(void)
{
	/* check trigger ghpm status */
	pr_info("%s, trigger pm_callback state=%d", __func__,
		atomic_read(&trigger_pm_callback_state));
}
EXPORT_SYMBOL(dump_pm_callback_kbase_info);


#if IS_BUILTIN(CONFIG_MTK_GPU_SUPPORT)
#endif /* CONFIG_MTK_GPU_SUPPORT */

GED_ERROR ged_notify_sw_vsync_system_init(void)
{
	g_psNotifyWorkQueue = alloc_ordered_workqueue("ged_notify_sw_vsync",
						WQ_HIGHPRI | WQ_FREEZABLE | WQ_MEM_RECLAIM);

	if (g_psNotifyWorkQueue == NULL)
		return GED_ERROR_OOM;

	mutex_init(&gsVsyncStampLock);
	mutex_init(&gsVsyncModeLock);

	hrtimer_init(&g_HT_hwvsync_emu, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_hwvsync_emu.function = ged_sw_vsync_check_cb;

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	ged_gpu_apo_init_nolock();
	ged_gpu_predict_apo_init_nolock();

	g_apo_hint = APO_NORMAL_HINT;
	g_apo_force_hint = APO_INVALID_HINT;

	g_apo_thr_ns = GED_APO_THR_NS;
	g_apo_wakeup_ns = GED_APO_WAKEUP_THR_NS;

	g_apo_autosuspend_delay_ref_count = 0;
	g_apo_autosuspend_delay_ctrl = 0;
	g_apo_autosuspend_delay_target_ref_count = GED_APO_AUTOSUSPEND_DELAY_TARGET_REF_COUNT;

	g_apo_autosuspend_delay_ms = GED_APO_AUTOSUSPEND_DELAY_MS;

	g_apo_lp_thr_ns = GED_APO_LP_THR_NS;

	g_apo_legacy = GED_APO_LEGACY_INVALID;

	spin_lock_init(&g_sApoLock);
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

	g_autosuspend_stress = 0;

	//MBarin: initialize current power state timestamp
	g_curr_pwr_state = (u32) gpufreq_get_power_state();
	pwr_state_time[g_curr_pwr_state].start_ts = ged_get_time();

	return GED_OK;
}

void ged_notify_sw_vsync_system_exit(void)
{
	if (g_psNotifyWorkQueue != NULL) {
		flush_workqueue(g_psNotifyWorkQueue);
		destroy_workqueue(g_psNotifyWorkQueue);
		g_psNotifyWorkQueue = NULL;
	}

#ifdef ENABLE_COMMON_DVFS
	hrtimer_cancel(&g_HT_hwvsync_emu);
#endif
	mutex_destroy(&gsVsyncModeLock);
	mutex_destroy(&gsVsyncStampLock);
}
