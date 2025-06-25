// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/rbtree.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/math64.h>
#include <linux/math.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <mt-plat/fpsgo_common.h>

#include "eas/grp_awr.h"
#include "eas/group.h"
#include "fpsgo_usedext.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fbt_usedext.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "../fstb/fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fps_composer.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu_ctrl.h"
#include "fbt_cpu_ux.h"
#include "sugov/cpufreq.h"

#define TARGET_UNLIMITED_FPS 240
#define NSEC_PER_HUSEC 100000
#define SBE_RESCUE_MODE_UNTIL_QUEUE_END 2
#define RESCUE_MAX_MONITOR_DROP_ARR_SIZE 10
#define UX_TARGET_DEFAULT_FPS 60
#define GAS_ENABLE_FPS 70
#define SBE_BUFFER_FILTER_DROP_THREASHOLD 2
#define SBE_BUFFER_FILTER_THREASHOLD 2
#define SBE_RESCUE_MORE_THREASHOLD 5

enum FPSGO_HARD_LIMIT_POLICY {
	FPSGO_HARD_NONE = 0,
	FPSGO_HARD_MARGIN = 1,
	FPSGO_HARD_CEILING = 2,
	FPSGO_HARD_LIMIT = 3,
};

enum FPSGO_JERK_STAGE {
	FPSGO_JERK_INACTIVE = 0,
	FPSGO_JERK_FIRST,
	FPSGO_JERK_SBE,
	FPSGO_JERK_SECOND,
};

enum FPSGO_TASK_POLICY {
	FPSGO_TASK_NONE = 0,
	FPSGO_TASK_VIP = 1,
};

static DEFINE_MUTEX(fbt_mlock);

static struct kmem_cache *frame_info_cachep __ro_after_init;
static struct kmem_cache *ux_scroll_info_cachep __ro_after_init;
static struct kmem_cache *hwui_frame_info_cachep __ro_after_init;

static int fpsgo_ux_gcc_enable;
static int sbe_rescue_enable;
static int sbe_rescuing_frame_id_legacy;
static int sbe_enhance_f;
static int sbe_dy_max_enhance;
static int sbe_dy_enhance_margin;
static int sbe_dy_rescue_enable;
static int sbe_dy_frame_threshold;
static int scroll_cnt;
static int global_ux_blc;
static int global_ux_max_pid;
static int set_deplist_vip;
static int ux_general_policy;
static int ux_general_policy_type;
static int ux_general_policy_dpt_setwl;
static int gas_threshold;
static int gas_threshold_for_low_TLP;
static int gas_threshold_for_high_TLP;
static int global_sbe_dy_enhance;
static int global_sbe_dy_enhance_max_pid;
static int global_dfrc_fps_limit;

static struct fpsgo_loading temp_blc_dep[MAX_DEP_NUM];
static struct fbt_setting_info sinfo;

#if IS_ENABLED(CONFIG_ARM64)
#define SMART_LAUNCH_BOOST_SUPPORT_CLUSTER_NUM 3
static int smart_launch_off_on;
static int cluster_num;
static int nr_freq_opp_cnt;
struct smart_launch_capacity_info {
	unsigned int *capacity;
	int first_cpu_id;
	int num_opp;
};
struct smart_launch_capacity_info *capacity_info;
#endif

module_param(fpsgo_ux_gcc_enable, int, 0644);
module_param(sbe_enhance_f, int, 0644);
module_param(sbe_dy_frame_threshold, int, 0644);
module_param(sbe_dy_rescue_enable, int, 0644);
module_param(sbe_dy_max_enhance, int, 0644);
module_param(sbe_dy_enhance_margin, int, 0644);
module_param(scroll_cnt, int, 0644);
module_param(set_deplist_vip, int, 0644);
module_param(ux_general_policy, int, 0644);
module_param(ux_general_policy_type, int, 0644);
module_param(ux_general_policy_dpt_setwl, int, 0644);
module_param(gas_threshold_for_low_TLP, int, 0644);
module_param(gas_threshold_for_high_TLP, int, 0644);

static void update_hwui_frame_info(struct render_info *info,
		struct hwui_frame_info *frame, unsigned long long id,
		unsigned long long start_ts, unsigned long long end_ts,
		unsigned long long rsc_start_ts, unsigned long long rsc_end_ts,
		int rescue_reason);
static struct ux_scroll_info *get_latest_ux_scroll_info(struct render_info *thr);

/* main function*/
static int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}

int get_ux_general_policy(void)
{
	return ux_general_policy;
}

int fpsgo_ctrl2ux_get_perf(void)
{
	return global_ux_blc;
}

void fbt_ux_set_perf(int cur_pid, int cur_blc)
{
	global_ux_blc = cur_blc;
	global_ux_max_pid = cur_pid;
}

void fbt_set_global_sbe_dy_enhance(int cur_pid, int cur_dy_enhance)
{
	global_sbe_dy_enhance = cur_dy_enhance;
	global_sbe_dy_enhance_max_pid = cur_pid;
}

void fpsgo_set_affnity_on_rescue(int pid, int r_cpu_mask)
{
	if (fbt_set_affinity(pid, r_cpu_mask))
		FPSGO_LOGE("[comp] %s %d setaffinity fail\n",
				__func__, r_cpu_mask);
}

static void fpsgo_set_deplist_policy(struct render_info *thr, int policy)
{
	int i;
	int local_dep_size = 0;
	struct fpsgo_loading *local_dep_arr = NULL;

	if (!thr)
		return;

	if (!set_deplist_vip)
		return;

	local_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!local_dep_arr)
		return;

	local_dep_size = fbt_determine_final_dep_list(thr, local_dep_arr);

	for (i = 0; i < local_dep_size; i++) {
		if (local_dep_arr[i].pid <= 0)
			continue;

		if (policy == FPSGO_TASK_NONE) {
#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
			unset_task_basic_vip(local_dep_arr[i].pid);
#endif

#if SBE_AFFNITY_TASK
			fpsgo_set_affnity_on_rescue(local_dep_arr[i].pid, FPSGO_PREFER_NONE);
#endif //SBE_AFFNITY_TASK
		}

		if (policy == FPSGO_TASK_VIP) {
#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
			set_task_basic_vip(local_dep_arr[i].pid);
#endif

#if SBE_AFFNITY_TASK
			fpsgo_set_affnity_on_rescue(local_dep_arr[i].pid, FPSGO_PREFER_M);
#endif //SBE_AFFNITY_TASK
		}
	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, policy, "[ux]set_vip");

	kfree(local_dep_arr);
}

static int fbt_ux_cal_perf(
	long long t_cpu_cur,
	long long target_time,
	unsigned int target_fps,
	unsigned int target_fps_ori,
	unsigned int fps_margin,
	struct render_info *thread_info,
	unsigned long long ts,
	long aa, unsigned int target_fpks, int cooler_on)
{
	unsigned int blc_wt = 0U;
	unsigned int last_blc_wt = 0U;
	struct fbt_boost_info *boost_info;
	int pid;
	unsigned long long buffer_id;
	unsigned long long t1, t2, t_Q2Q;
	long aa_n;

	if (!thread_info) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0;
	}

	pid = thread_info->pid;
	buffer_id = thread_info->buffer_id;
	boost_info = &(thread_info->boost_info);

	mutex_lock(&fbt_mlock);

	t1 = (unsigned long long)t_cpu_cur;
	t1 = nsec_to_100usec(t1);
	t2 = target_time;
	t_Q2Q = thread_info->Q2Q_time;
	t_Q2Q = nsec_to_100usec(t_Q2Q);
	aa_n = aa;

	if (thread_info->p_blc) {
		fpsgo_get_blc_mlock(__func__);
		last_blc_wt = thread_info->p_blc->blc;
		fpsgo_put_blc_mlock(__func__);
	}

	if (aa_n < 0) {
		blc_wt = last_blc_wt;
		aa_n = 0;
	} else {
		fbt_cal_aa(aa, t1, t_Q2Q, &aa_n);

		if (fpsgo_ux_gcc_enable == 2) {
			fbt_cal_target_time_ns(thread_info->pid, thread_info->buffer_id,
				fbt_get_rl_ko_is_ready(), 2, target_fps_ori,
				thread_info->target_fps_origin, target_fpks,
				target_time, 0, boost_info->last_target_time_ns, thread_info->Q2Q_time,
				0, 0, thread_info->attr.expected_fps_margin_by_pid, 10, 10,
				0, thread_info->attr.quota_v2_diff_clamp_min_by_pid,
				thread_info->attr.quota_v2_diff_clamp_max_by_pid, 0, 0,
				0, aa_n, aa_n, aa_n, 100, 100, 100, 0, 0, 0, 0, &t2);
			boost_info->last_target_time_ns = t2;
		}

		t2 = nsec_to_100usec(t2);

		fbt_cal_blc(aa_n, t2, last_blc_wt, t_Q2Q, 0, &blc_wt);
	}

	fpsgo_systrace_c_fbt(pid, buffer_id, aa_n, "[ux]aa");

	blc_wt = clamp(blc_wt, 1U, 100U);

	boost_info->target_fps = target_fps;
	boost_info->target_time = target_time;
	boost_info->last_blc = blc_wt;
	boost_info->last_normal_blc = blc_wt;
	thread_info->target_fps_origin = target_fps_ori;
	//boost_info->cur_stage = FPSGO_JERK_INACTIVE;
	mutex_unlock(&fbt_mlock);
	return blc_wt;
}

static int fbt_ux_get_max_cap(int pid, unsigned long long bufID, int min_cap)
{
	int bhr_local = fbt_cpu_get_bhr();

	return fbt_get_max_cap(min_cap, 0, bhr_local, pid, bufID);
}

static void fbt_ux_set_cap(struct render_info *thr, int min_cap, int max_cap)
{
	int i;
	int local_dep_size = 0;
	char temp[7] = {"\0"};
	char *local_dep_str = NULL;
	struct fpsgo_loading *local_dep_arr = NULL;
	int ret = 0;

	local_dep_str = kcalloc(MAX_DEP_NUM + 1, 7 * sizeof(char), GFP_KERNEL);
	if (!local_dep_str)
		goto out;

	local_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!local_dep_arr)
		goto out;

	local_dep_size = fbt_determine_final_dep_list(thr, local_dep_arr);

	for (i = 0; i < local_dep_size; i++) {
		if (local_dep_arr[i].pid <= 0)
			continue;

		fbt_set_per_task_cap(local_dep_arr[i].pid, min_cap, max_cap, 1024);

		if (strlen(local_dep_str) == 0)
			ret = snprintf(temp, sizeof(temp), "%d", local_dep_arr[i].pid);
		else
			ret = snprintf(temp, sizeof(temp), ",%d", local_dep_arr[i].pid);

		if (ret > 0 && strlen(local_dep_str) + strlen(temp) < 256)
			strncat(local_dep_str, temp, strlen(temp));
	}

	fpsgo_main_trace("[%d] dep-list %s", thr->pid, local_dep_str);

out:
	kfree(local_dep_str);
	kfree(local_dep_arr);
}

static void fbt_ux_set_cap_with_sbe(struct render_info *thr)
{
	int set_blc_wt = 0;
	int local_min_cap = 0;
	int local_max_cap = 100;

	set_blc_wt = thr->ux_blc_cur + thr->sbe_enhance;
	set_blc_wt = clamp(set_blc_wt, 0, 100);

	fpsgo_get_blc_mlock(__func__);
	if (thr->p_blc)
		thr->p_blc->blc = set_blc_wt;
	fpsgo_put_blc_mlock(__func__);

	fpsgo_get_fbt_mlock(__func__);

	local_min_cap = set_blc_wt;

	if (!set_blc_wt || thr->sbe_enhance > 0)
		local_max_cap = 100;
	else
		local_max_cap = fbt_ux_get_max_cap(thr->pid, thr->buffer_id, set_blc_wt);

	if (local_min_cap == 0 && local_max_cap == 100)
		fbt_check_max_blc_locked(thr->pid);
	else
		fbt_set_limit(thr->pid, local_min_cap, thr->pid, thr->buffer_id,
			thr->dep_valid_size, thr->dep_arr, thr, 0);
	fpsgo_put_fbt_mlock(__func__);

	fbt_ux_set_cap(thr, local_min_cap, local_max_cap);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, local_min_cap, "[ux]perf_idx");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, local_max_cap, "[ux]perf_idx_max");
}

void fbt_ux_frame_start(struct render_info *thr, unsigned long long frameid, unsigned long long ts)
{
	if (!thr)
		return;

	thr->ux_blc_cur = thr->ux_blc_next;
	if (ux_general_policy)
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_VIP);

	fbt_ux_set_cap_with_sbe(thr);

	if (sbe_dy_rescue_enable && !list_empty(&thr->scroll_list)) {
		struct hwui_frame_info *frame = get_valid_hwui_frame_info_from_pool(thr);

		if (frame) {
			frame->frameID = frameid;
			frame->start_ts = ts;
		}
	}
}

void fbt_ux_frame_end(struct render_info *thr, unsigned long long frameid,
		unsigned long long start_ts, unsigned long long end_ts)
{
	struct fbt_boost_info *boost;
	struct sbe_info *s_info =NULL;
	long long runtime;
	int targettime, targetfps, targetfps_ori, targetfpks, fps_margin, cooler_on;
	int loading = 0L;
	int q_c_time = 0L, q_g_time = 0L;

	if (!thr)
		return;

	boost = &(thr->boost_info);

	runtime = thr->running_time;

	if (boost->f_iter < 0)
		boost->f_iter = 0;
	boost->frame_info[boost->f_iter].running_time = runtime;
	// fstb_query_dfrc
	fpsgo_fbt2fstb_query_fps(thr->pid, thr->buffer_id,
			&targetfps, &targetfps_ori, &targettime, &fps_margin,
			&q_c_time, &q_g_time, &targetfpks, &cooler_on);
	boost->quantile_cpu_time = q_c_time;
	boost->quantile_gpu_time = q_g_time;	// [ux] unavailable, for statistic only.

	if (!targetfps)
		targetfps = TARGET_UNLIMITED_FPS;

	if (start_ts == 0)
		goto EXIT;

	thr->Q2Q_time = end_ts - start_ts;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, targetfps, "[ux]target_fps");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		targettime, "[ux]target_time");

	fpsgo_get_fbt_mlock(__func__);
	if (ux_general_policy) {
		s_info = fpsgo_search_and_add_sbe_info(thr->tgid, 0);

		if (s_info) {
			fpsgo_set_deplist_policy(thr, FPSGO_TASK_NONE);
			if (s_info->ux_scrolling) {
				fbt_get_dep_list(thr);
				fpsgo_set_deplist_policy(thr, FPSGO_TASK_VIP);
			}
		} else {
			fpsgo_main_trace("%d: not find sbe_info ", __func__);
		}
	} else if (fbt_get_dep_list(thr)) {
		fpsgo_main_trace("[%d] fail get dep-list", thr->pid);
	}
	fpsgo_put_fbt_mlock(__func__);

	fpsgo_get_blc_mlock(__func__);
	if (thr->p_blc) {
		thr->p_blc->dep_num = thr->dep_valid_size;
		if (thr->dep_arr)
			memcpy(thr->p_blc->dep, thr->dep_arr,
					thr->dep_valid_size * sizeof(struct fpsgo_loading));
		else
			thr->p_blc->dep_num = 0;
	}

	if (sbe_dy_rescue_enable) {
		struct hwui_frame_info *new_frame;
		struct hwui_frame_info *frame =
			get_hwui_frame_info_by_frameid(thr, frameid);
		if (frame) {// only insert rescued frame
			if (frame->rescue) {
				update_hwui_frame_info(thr, frame, frameid, 0, end_ts, 0, 0, 0);
				new_frame = insert_hwui_frame_info_from_tmp(thr, frame);
				if (new_frame)
					count_scroll_rescue_info(thr, new_frame);
			}
			reset_hwui_frame_info(frame);
		}
	}

	fpsgo_put_blc_mlock(__func__);

	fbt_set_render_boost_attr(thr);
	loading = fbt_get_loading(thr, start_ts, end_ts);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, loading, "[ux]compute_loading");

	/* unreliable targetfps */
	if (targetfps == -1) {
		fbt_reset_boost(thr);
		runtime = -1;
		goto EXIT;
	}

	thr->ux_blc_next = fbt_ux_cal_perf(runtime,
			targettime, targetfps, targetfps_ori, fps_margin,
			thr, end_ts, loading, targetfpks, cooler_on);

	if ((thr->pid != global_ux_max_pid && thr->ux_blc_next > global_ux_blc) ||
		thr->pid == global_ux_max_pid)
		fbt_ux_set_perf(thr->pid, thr->ux_blc_next);

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->ux_blc_next, "[ux]ux_blc_next");

EXIT:
	thr->ux_blc_cur = 0;
	fbt_ux_set_cap_with_sbe(thr);
	fpsgo_fbt2fstb_update_cpu_frame_info(thr->pid, thr->buffer_id,
		thr->tgid, thr->frame_type,
		0, thr->running_time, targettime,
		thr->ux_blc_next, 100, 0, 0);
}

void fbt_ux_frame_err(struct render_info *thr, int frame_count,
		unsigned long long frameID, unsigned long long ts)
{
	if (!thr)
		return;

	if (frame_count == 0) {
		if (ux_general_policy)
			fpsgo_set_deplist_policy(thr, FPSGO_TASK_NONE);
		thr->ux_blc_cur = 0;
		fbt_ux_set_cap_with_sbe(thr);
	}

	//try end this frame rescue when err
	if (thr->sbe_rescuing_frame_id == frameID
			&& thr->boost_info.sbe_rescue != 0)
		fpsgo_sbe_rescue(thr, 0, 0, 0, 0, frameID);

	if (sbe_dy_rescue_enable) {
		struct hwui_frame_info *frame =
			get_hwui_frame_info_by_frameid(thr, frameID);
		if (frame)
			reset_hwui_frame_info(frame);
	}
}

void fpsgo_ux_delete_frame_info(struct render_info *thr, struct ux_frame_info *info)
{
	if (!info)
		return;
	rb_erase(&info->entry, &(thr->ux_frame_info_tree));
	kmem_cache_free(frame_info_cachep, info);
}

void fpsgo_ux_doframe_end(struct render_info *thr, unsigned long long frame_id,
				long long frame_flags)
{
	int frame_status = (frame_flags & FRAME_MASK) >> 16;
	int rescue_type = frame_flags & RESCUE_MASK;
	struct hwui_frame_info *frame;
	unsigned long long rescue_time = 0;
	unsigned long long ts = 0;

	if (!thr || !sbe_dy_rescue_enable)
		return;

	frame = get_hwui_frame_info_by_frameid(thr, frame_id);
	if (frame) {
		struct ux_scroll_info *scroll_info = NULL;

		frame->rescue_reason = rescue_type;
		frame->frame_status = frame_status;
		//this frame meet buffer count filter
		if (!frame->rescue
				&& ((rescue_type & RESCUE_WAITING_ANIMATION_END) != 0
				|| (rescue_type & RESCUE_TYPE_TRAVERSAL_DYNAMIC) != 0
				|| (rescue_type & RESCUE_TYPE_OI) != 0)
				&& frame->start_ts != 0 && thr->boost_info.target_time > 0) {
			//thinking rescue time is 1/2 vsync rescue
			rescue_time = frame->start_ts + (thr->boost_info.target_time >> 1);
			update_hwui_frame_info(thr, frame, frame_id, 0, 0, rescue_time, 0, 0);
		}
		thr->rescue_start_time = 0;

		scroll_info = get_latest_ux_scroll_info(thr);

		if (!scroll_info)
			return;

		scroll_info->last_frame_ID = frame_id;
		ts = fpsgo_get_time();

		//try enable buffer count filter
		if ((frame->rescue || (rescue_type & RESCUE_TRAVERSAL_OVER_VSYNC) != 0)
				&& thr->buffer_count_filter == 0) {
			//check if rescue more, if rescue, but buffer count is 3 or more
			if (thr->cur_buffer_count > SBE_BUFFER_FILTER_THREASHOLD)
				thr->rescue_more_count++;

			//if resue more count is more than 5 times, enable buffer count filter
			if (thr->rescue_more_count >= SBE_RESCUE_MORE_THREASHOLD)
				thr->buffer_count_filter = SBE_BUFFER_FILTER_DROP_THREASHOLD;

		}

		// buffer_count filter lead to frame drop, disable it
		if (thr->buffer_count_filter > 0
				&& scroll_info->rescue_filter_buffer_time > 0
				&& (ts - frame->start_ts) > scroll_info->rescue_filter_buffer_time) {
			thr->buffer_count_filter--;
			//if drop more, do not check rescue more in this page
			//will be reset to 0 after activity resume
			if (thr->buffer_count_filter == 0) {
				thr->buffer_count_filter = -1;
				thr->rescue_more_count = 0;
			}

			fpsgo_main_trace("doframe end -> %d frame:%lld buffer_count:%d, drop more\n",
					thr->pid, frame_id, thr->cur_buffer_count);
		}
		scroll_info->rescue_filter_buffer_time = 0;
	}
}

struct ux_frame_info *fpsgo_ux_search_and_add_frame_info(struct render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action)
{
	struct rb_node **p = &(thr->ux_frame_info_tree).rb_node;
	struct rb_node *parent = NULL;
	struct ux_frame_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct ux_frame_info, entry);
		if (frameID < tmp->frameID)
			p = &(*p)->rb_left;
		else if (frameID > tmp->frameID)
			p = &(*p)->rb_right;
		else
			return tmp;
	}
	if (action == 0)
		return NULL;
	if (frame_info_cachep)
		tmp = kmem_cache_alloc(frame_info_cachep,
			GFP_KERNEL | __GFP_ZERO);
	if (!tmp)
		return NULL;

	tmp->frameID = frameID;
	tmp->start_ts = start_ts;
	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &(thr->ux_frame_info_tree));

	return tmp;
}

static struct ux_frame_info *fpsgo_ux_find_earliest_frame_info
	(struct render_info *thr)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL, *ret = NULL;
	unsigned long long min_ts = ULLONG_MAX;

	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		if (tmp->start_ts < min_ts) {
			min_ts = tmp->start_ts;
			ret = tmp;
		}
		cur = rb_next(cur);
	}
	return ret;

}

int fpsgo_ux_count_frame_info(struct render_info *thr, int target)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL;
	int ret = 0;
	int remove = 0;

	if (RB_EMPTY_ROOT(&thr->ux_frame_info_tree) == 1)
		return ret;

	cur = rb_first(&(thr->ux_frame_info_tree));
	while (cur) {
		cur = rb_next(cur);
		ret += 1;
	}
	/* error handling */
	while (ret > target) {
		tmp = fpsgo_ux_find_earliest_frame_info(thr);
		if (!tmp)
			break;

		fpsgo_ux_delete_frame_info(thr, tmp);
		ret -= 1;
		remove += 1;
	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, remove, "[ux]rb_err_remove");
	return ret;
}

void fpsgo_ux_reset(struct render_info *thr)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	if(!thr)
		return;


	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		rb_erase(&tmp->entry, &(thr->ux_frame_info_tree));
		kmem_cache_free(frame_info_cachep, tmp);
		cur = rb_first(&(thr->ux_frame_info_tree));
	}

}

void fpsgo_reset_deplist_task_priority(struct render_info *thr)
{
	if (!thr) {
		FPSGO_LOGE("%s: NON render info!!!!\n", __func__);
		return;
	}
	if (ux_general_policy)
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_NONE);

}

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
void fpsgo_set_group_dvfs(int start)
{
	if (start){
		//disable group dvfs when scroll begin
		group_set_mode(0);
		flt_ctrl_force_set(1);
		set_grp_dvfs_ctrl(0);
	}else{
		//enable group dvfs when scroll end
		flt_ctrl_force_set(0);
		group_set_mode(1);
	}
}

void fpsgo_set_gas_policy(int start)
{
	if (start){
		group_set_mode(1);
		group_set_threshold(0, gas_threshold);
		set_top_grp_aware(1,0);
		flt_ctrl_force_set(1);
		set_grp_dvfs_ctrl(9);
	}else{
		group_reset_threshold(0);
		set_top_grp_aware(0,0);
	}
	fpsgo_main_trace("gas enabled : %d, %d", start, gas_threshold);
}

void update_ux_general_policy(void)
{
	if (ux_general_policy_type == 1) {
		//update threshold for multi window, multi window is hight TLP scenario
		group_set_threshold(0, gas_threshold_for_high_TLP);
	}
}
#endif

void fpsgo_ctrl2uxfbt_dfrc_fps(int fps_limit)
{
	if (!fps_limit || fps_limit > TARGET_UNLIMITED_FPS)
		return;

	mutex_lock(&fbt_mlock);
	global_dfrc_fps_limit = fps_limit;

	fpsgo_main_trace("global_dfrc_fps_limit %d", global_dfrc_fps_limit);

	mutex_unlock(&fbt_mlock);
}

void fpsgo_boost_non_hwui_policy(struct render_info *thr, int set_vip)
{
	if (!thr) {
		fpsgo_main_trace("%s: NON render info!!!!\n",
			__func__);
		return;
	}
	if (set_vip)
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_VIP);
	else
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_NONE);
}

void fpsgo_set_ux_general_policy(int scrolling, unsigned long ux_mask)
{
	int pid;
	int need_gas_policy __maybe_unused = 1;

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	set_ignore_idle_ctrl(scrolling);
	fpsgo_set_group_dvfs(scrolling);
#endif

	if (ux_general_policy_type == 0) {
		if (scrolling) {
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
			if (ux_general_policy_dpt_setwl) {
				set_wl_cpu_manual(0);
				fpsgo_main_trace("set_wl_cpu_manual: 0");
			}
#endif
			if (change_dpt_support_driver_hook)
				change_dpt_support_driver_hook(1);
			fpsgo_main_trace("start runtime power talbe");
		} else {
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
			if (ux_general_policy_dpt_setwl) {
				set_wl_cpu_manual(-1);
				fpsgo_main_trace("set_wl_cpu_manual: -1");
			}
#endif
			if (change_dpt_support_driver_hook)
				change_dpt_support_driver_hook(0);
			fpsgo_main_trace("end runtime power talbe");
		}
	} else if (ux_general_policy_type == 1) {
		fpsgo_main_trace("gas will call, fps = %d", global_dfrc_fps_limit);
		//If GAS policy, we will set gas_threshold for different ux type
		if (test_bit(FPSGO_HWUI, &ux_mask)) {
			gas_threshold = gas_threshold_for_low_TLP;
			// If dfrc < limit, dont enable gas policy
			if (global_dfrc_fps_limit < GAS_ENABLE_FPS)
				need_gas_policy = 0;
		} else if (test_bit(FPSGO_NON_HWUI, &ux_mask)) {
			gas_threshold = gas_threshold_for_high_TLP;
		}

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
		if (need_gas_policy)
			fpsgo_set_gas_policy(scrolling);
#endif
	}

#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	//set kfps process priority to vip
	pid = fpsgo_get_kfpsgo_tid();
	if (scrolling)
		set_task_basic_vip(pid);
	else
		unset_task_basic_vip(pid);
#endif
}

int fpsgo_sbe_dy_enhance(struct render_info *thr)
{
	struct ux_scroll_info *scroll_info;
	struct hwui_frame_info *hwui_info;
	int drop = 0;
	int all_rescue_cap_count = 0;
	unsigned long long all_rescue_frame_time_count = 0;
	int all_rescue_frame_count = 0;
	unsigned long long target_time = 0LLU;
	int last_enhance = 0;
	int new_enhance = 0;
	int result = -1;
	int scroll_count = 0;
	unsigned long long rescue_target_time = 0LLU;

	if (!sbe_dy_rescue_enable || !thr || IS_ERR_OR_NULL(&thr->scroll_list))
		return result;

	target_time = thr->boost_info.target_time;

	if (target_time <= 0)
		return result;

	rescue_target_time = thr->boost_info.sbe_rescue_target_time;
	if (rescue_target_time <= 0) {
		FPSGO_LOGE("[SBE_UX] rescue target is err:%llu\n", rescue_target_time);
		rescue_target_time = target_time >> 1;
	}

	scroll_count = get_ux_list_length(&thr->scroll_list);

	if (scroll_cnt > 0 && scroll_count < scroll_cnt) {
		//TODO: scroll not too much to check dynamic rescue, check drop rate and jank rate
		//if scroll refresh rate do not match target fps, start to increase rescue
		return result;
	}

	list_for_each_entry (scroll_info, &thr->scroll_list, queue_list) {
		if (IS_ERR_OR_NULL(&scroll_info->frame_list)
				|| scroll_info->jank_count <= 0) {
			continue;
		}

		all_rescue_cap_count += scroll_info->rescue_cap_count;
		all_rescue_frame_time_count += scroll_info->rescue_frame_time_count;
		all_rescue_frame_count += scroll_info->rescue_frame_count;
	}

	last_enhance = thr->sbe_dy_enhance_f > 0 ? thr->sbe_dy_enhance_f : sbe_enhance_f;

	if (all_rescue_frame_count > 0 && all_rescue_frame_time_count > 0) {
		int max_enhance = clamp(sbe_dy_max_enhance, 0, 100);
		int new = clamp((int)
			(div64_u64(all_rescue_cap_count, all_rescue_frame_count) + last_enhance), 0, 100);
		unsigned long long avg_frame_time =
				div64_u64(all_rescue_frame_time_count, all_rescue_frame_count);
		//TODO: consider msync case, how to compute new enhance
		new_enhance = (int)(div64_u64(new *avg_frame_time, target_time) - last_enhance);
		new_enhance = clamp(new_enhance, 0, max_enhance);
	}

	result = last_enhance;

	if (new_enhance > 0 && new_enhance != last_enhance) {
		int max_monitor_drop_frame = RESCUE_MAX_MONITOR_DROP_ARR_SIZE - 1;
		long long new_dur;
		long long old_tmp;
		int benifit_f_up = 0;
		int benifit_f_down = 1;
		int threshold = sbe_dy_frame_threshold > 0 ? sbe_dy_frame_threshold : 0;
		int tempScore[RESCUE_MAX_MONITOR_DROP_ARR_SIZE];

		list_for_each_entry (scroll_info, &thr->scroll_list, queue_list) {
			if (IS_ERR_OR_NULL(&scroll_info->frame_list)
					|| scroll_info->jank_count <= 0) {
				continue;
			}
			drop = 0;
			for (size_t i = 0; i < RESCUE_MAX_MONITOR_DROP_ARR_SIZE; i++)
				tempScore[i] = 0;

			list_for_each_entry (hwui_info, &scroll_info->frame_list, queue_list) {
				if (!hwui_info->rescue)
					continue;

				int before = clamp((hwui_info->perf_idx + last_enhance), 0, 100);
				int after = clamp((hwui_info->perf_idx + new_enhance), 0, 100);

				if (new_enhance > last_enhance && hwui_info->dur_ts <= target_time) {
					tempScore[0] += 1;
					drop = 0;
				} else {
					if (hwui_info->dur_ts < rescue_target_time) {
						old_tmp = before * (long long)hwui_info->dur_ts;
						new_dur = div64_s64(old_tmp, after);
					} else {
						old_tmp = before * ((long long)hwui_info->dur_ts
							- (long long)rescue_target_time);
						new_dur = div64_s64(old_tmp, after) + (long long)rescue_target_time;
					}
					drop = (int)div64_u64(new_dur, target_time);

					if (drop < 0) {
						FPSGO_LOGE("[SBE_UX] %s old_dur:%llu old_e:%d new_e:%d rescue_t %lld\n",
								__func__, hwui_info->dur_ts, last_enhance,
								new_enhance, rescue_target_time);
						drop = 0;
					}

					if (drop < max_monitor_drop_frame)
						tempScore[drop]++;
					else
						tempScore[max_monitor_drop_frame]++;
				}

				//compute drop diff with before
				if (new_enhance > last_enhance) {
					//increase enhance
					if (hwui_info->drop - drop > 0)
						benifit_f_up += (hwui_info->drop - drop);

					benifit_f_down = 0;
				} else {
					//decrease enhance, default allow, if drop more, disallow
					if (drop - hwui_info->drop > 0) {
						benifit_f_down = 0;
						break;
					}
				}
			}

			if (benifit_f_up > threshold || !benifit_f_down) {
				//already meet condition
				break;
			}
		}

		if (benifit_f_up >= threshold || benifit_f_down) {
			fpsgo_main_trace("SBE_UX enhance change from %d to %d",
					last_enhance, new_enhance);
			result = new_enhance;
		}
	}

	return result;
}

void fpsgo_ux_scrolling_end(struct render_info *thr)
{
	struct ux_scroll_info *scroll_info;
	if (!thr || !sbe_dy_rescue_enable)
		return;

	//reset rescue affnity if needed
	scroll_info = get_latest_ux_scroll_info(thr);
	if (scroll_info && scroll_info->rescue_with_perf_mode > 0) {
		fpsgo_set_affnity_on_rescue(thr->tgid, FPSGO_PREFER_NONE);
		fpsgo_set_affnity_on_rescue(thr->pid, FPSGO_PREFER_NONE);
	}

	thr->sbe_dy_enhance_f = fpsgo_sbe_dy_enhance(thr);
	if (thr->sbe_dy_enhance_f > 0
			&& ((thr->pid != global_sbe_dy_enhance_max_pid
			&& thr->sbe_dy_enhance_f > global_sbe_dy_enhance)
			|| thr->pid == global_sbe_dy_enhance_max_pid))
		fbt_set_global_sbe_dy_enhance(thr->pid, thr->sbe_dy_enhance_f);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_dy_enhance_f, "[ux]sbe_dy_enhance_f");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "[ux]sbe_dy_enhance_f");
}

static void release_scroll(struct ux_scroll_info *info)
{
	struct hwui_frame_info *frame, *tmpFrame;

	list_for_each_entry_safe(frame, tmpFrame, &info->frame_list, queue_list) {
		list_del(&frame->queue_list);
		kmem_cache_free(hwui_frame_info_cachep, frame);
	}

	list_del(&info->queue_list);
	kfree(info->score);
	kmem_cache_free(ux_scroll_info_cachep, info);
}

static void release_all_ux_info(struct render_info *thr)
{
	struct ux_scroll_info *pos, *tmp;

	// clear ux scroll infos for new activity
	fpsgo_main_trace("SBE_UX clear_ux %d", thr->pid);
	list_for_each_entry_safe(pos, tmp, &thr->scroll_list, queue_list) {
		release_scroll(pos);
	}
	// reset sbe dy enhance
	thr->sbe_dy_enhance_f = -1;
}

void clear_ux_info(struct render_info *thr)
{
	//when activity resume, will call this function
	global_sbe_dy_enhance = 0;
	global_sbe_dy_enhance_max_pid = 0;
	thr->buffer_count_filter = 0;
	thr->rescue_more_count = 0;
	release_all_ux_info(thr);
}

static void fpsgo_update_sbe_dy_rescue(struct render_info *thr, int sbe_dy_enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frame_id)
{
	unsigned long long ts = 0;

	if ((rescue_type & RESCUE_COUNTINUE_RESCUE) != 0
			&& (rescue_type & RESCUE_TYPE_PRE_ANIMATION) != 0
			&& (rescue_type & RESCUE_RUNNING) != 0
			&& thr->rescue_start_time > 0) {
		//pre-animation case, think frame start time same as rescue time
		struct hwui_frame_info *frame = get_hwui_frame_info_by_frameid(thr, frame_id);

		if (frame) {
			update_hwui_frame_info(thr, frame, frame_id,
					0, 0, thr->rescue_start_time, 0, 0);
			//update rescue start time
			frame->start_ts = thr->rescue_start_time;
		}

	} else if (((rescue_type & RESCUE_WAITING_ANIMATION_END) != 0
			|| (rescue_type & RESCUE_TYPE_TRAVERSAL_DYNAMIC) != 0
			|| (rescue_type & RESCUE_TYPE_OI) != 0
			|| (rescue_type & RESCUE_TYPE_PRE_ANIMATION) != 0)
			&& (rescue_type & RESCUE_COUNTINUE_RESCUE) == 0) {
		unsigned long long target_time = 0;
		struct hwui_frame_info *frame = get_hwui_frame_info_by_frameid(thr, frame_id);

		//update rescue start time
		ts = fpsgo_get_time();
		if (frame)
			update_hwui_frame_info(thr, frame, frame_id, 0, 0, ts, 0, 0);

		fpsgo_main_trace("%d dy_rescue id %lld sbe_dy_e:%d final_e %d rescue_t:%llu ts:%llu",
				thr->pid, frame_id, thr->sbe_dy_enhance_f,
				sbe_dy_enhance, rescue_target, ts);

		target_time = thr->boost_info.target_time;

		if (target_time > 0) {
			//update rescue_target, default is 1/2 target_time
			if (rescue_target > 0 && rescue_target < target_time) {
				if(thr->boost_info.sbe_rescue_target_time != rescue_target)
					thr->boost_info.sbe_rescue_target_time = rescue_target;
			} else
				thr->boost_info.sbe_rescue_target_time = (target_time >> 1);
		}

		thr->rescue_start_time = ts;

	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, rescue_type, "[ux]rescue_type");
}

static struct ux_scroll_info *get_latest_ux_scroll_info(struct render_info *thr)
{
	struct ux_scroll_info *result = NULL;

	if (!thr || list_empty(&thr->scroll_list))
		return result;

	// get latest scrolling worker
	return list_first_entry_or_null(&(thr->scroll_list), struct ux_scroll_info, queue_list);
}

static void update_rescue_filter_info(struct render_info *thr)
{
	struct ux_scroll_info *last_scroll = get_latest_ux_scroll_info(thr);
	unsigned long long target_time = 0;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			thr->cur_buffer_count, "[ux]rescue_filter");

	if (last_scroll) {
		target_time = thr->boost_info.target_time;
		//target_time max thinking is 10hz, incase overflow
		// 100000000ns = 100ms * 1000 000, 100ms = 1s / 10hz
		if (target_time > 0 && target_time <= 100000000)
			last_scroll->rescue_filter_buffer_time =
					(thr->cur_buffer_count - 1) * target_time - (target_time >> 1);

	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "[ux]rescue_filter");
}

void fpsgo_sbe_rescue(struct render_info *thr, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frame_id)
{
	unsigned long long ts = 0;
	int sbe_dy_enhance = -1;

	if (!thr || !sbe_rescue_enable)	//thr must find the 5566 one.
		return;

	mutex_lock(&fbt_mlock);
	if (start) {
		//before rescue check buffer count
		if (sbe_dy_rescue_enable && thr->buffer_count_filter > 0
				&& thr->cur_buffer_count > SBE_BUFFER_FILTER_THREASHOLD
				&& (rescue_type & RESCUE_TYPE_OI) == 0
				&& (rescue_type & RESCUE_RUNNING) == 0
				&& (rescue_type & RESCUE_TYPE_SECOND_RESCUE) == 0) {
			update_rescue_filter_info(thr);
			goto leave;
		}

		thr->sbe_rescuing_frame_id = frame_id;

		//update sbe rescue enhance
		if (sbe_dy_max_enhance > 0 && ((rescue_type & RESCUE_TYPE_MAX_ENHANCE) != 0))
			sbe_dy_enhance = sbe_dy_max_enhance;
		else if (!sbe_dy_rescue_enable)
			sbe_dy_enhance = sbe_enhance_f;
		else if (thr->sbe_dy_enhance_f <= 0) {
			//dy_rescue is enable, try use global_sbe_dy_enhance
			if (global_sbe_dy_enhance > 0)
				sbe_dy_enhance = thr->sbe_dy_enhance_f = global_sbe_dy_enhance;
			else
				sbe_dy_enhance = sbe_enhance_f;
		} else
			sbe_dy_enhance = thr->sbe_dy_enhance_f;

		if (sbe_dy_rescue_enable && sbe_dy_max_enhance > 0
				&& (rescue_type & RESCUE_TYPE_ENABLE_MARGIN) != 0) {
			sbe_dy_enhance += sbe_dy_enhance_margin;
			sbe_dy_enhance = clamp(sbe_dy_enhance, 0, sbe_dy_max_enhance);
		}

		thr->sbe_enhance = enhance < 0 ?  sbe_dy_enhance : (enhance + sbe_dy_enhance);
		thr->sbe_enhance = clamp(thr->sbe_enhance, 0, 100);

		if (sbe_dy_rescue_enable)
			fpsgo_update_sbe_dy_rescue(thr, sbe_dy_enhance,
					rescue_type, rescue_target, frame_id);


		//if sbe mark second rescue, then rescue again
		if (thr->boost_info.sbe_rescue != 0)
			goto leave;
		thr->boost_info.sbe_rescue = 1;

		if (sbe_dy_rescue_enable) {
			struct ux_scroll_info *last_scroll = get_latest_ux_scroll_info(thr);

			if (last_scroll && !last_scroll->rescue_with_perf_mode)
				last_scroll->rescue_with_perf_mode = (rescue_type & RESCUE_TYPE_ENABLE_MARGIN);

			if (last_scroll && last_scroll->rescue_with_perf_mode > 0) {
				fpsgo_set_affnity_on_rescue(thr->tgid, FPSGO_PREFER_M);
				fpsgo_set_affnity_on_rescue(thr->pid, FPSGO_PREFER_M);
			}
		} else {
			#if SBE_AFFNITY_TASK
			fpsgo_set_affnity_on_rescue(thr->tgid, FPSGO_PREFER_M);
			fpsgo_set_affnity_on_rescue(thr->pid, FPSGO_PREFER_M);
			#endif
		}

		fbt_ux_set_cap_with_sbe(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_enhance, "[ux]sbe_rescue");
	} else {
		if (thr->boost_info.sbe_rescue == 0)
			goto leave;
		if (frame_id < thr->sbe_rescuing_frame_id)
			goto leave;
		thr->sbe_rescuing_frame_id = -1;
		thr->boost_info.sbe_rescue = 0;
		thr->sbe_enhance = 0;

		if (sbe_dy_rescue_enable ) {
			struct hwui_frame_info *frame = get_hwui_frame_info_by_frameid(thr, frame_id);
			struct ux_scroll_info *last_scroll = get_latest_ux_scroll_info(thr);
			//update rescue end time
			ts = fpsgo_get_time();
			update_hwui_frame_info(thr, frame, frame_id, 0, 0, 0, ts, 0);
			thr->rescue_start_time = 0;
			//frame->rescue_reason is update in doframe end
			if (last_scroll && last_scroll->rescue_with_perf_mode > 0) {
				fpsgo_set_affnity_on_rescue(thr->tgid, FPSGO_PREFER_NONE);
				fpsgo_set_affnity_on_rescue(thr->pid, FPSGO_PREFER_NONE);
			}
		} else {
			#if SBE_AFFNITY_TASK
			fpsgo_set_affnity_on_rescue(thr->tgid,FPSGO_PREFER_NONE);
			fpsgo_set_affnity_on_rescue(thr->pid, FPSGO_PREFER_NONE);
			#endif
		}

		fbt_ux_set_cap_with_sbe(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "[ux]rescue_type");
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_enhance, "[ux]sbe_rescue");
	}
leave:
	mutex_unlock(&fbt_mlock);
}

void fpsgo_sbe_rescue_legacy(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id)
{
	int floor, blc_wt = 0, blc_wt_b = 0, blc_wt_m = 0;
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;
	int max_util = 1024, max_util_b = 1024, max_util_m = 1024;
	struct cpu_ctrl_data *pld;
	int rescue_opp_c, rescue_opp_f;
	int new_enhance;
	unsigned int temp_blc = 0;
	int temp_blc_pid = 0;
	unsigned long long temp_blc_buffer_id = 0;
	int temp_blc_dep_num = 0;
	int separate_aa;

	if (!thr || !sbe_rescue_enable)
		return;

	fbt_get_setting_info(&sinfo);
	pld = kcalloc(sinfo.cluster_num, sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld)
		return;

	mutex_lock(&fbt_mlock);

	separate_aa = thr->attr.separate_aa_by_pid;

	if (start) {
		if (frame_id)
			sbe_rescuing_frame_id_legacy = frame_id;
		if (thr->boost_info.sbe_rescue != 0)
			goto leave;
		floor = thr->boost_info.last_blc;
		if (!floor)
			goto leave;

		rescue_opp_c = fbt_get_rescue_opp_c();
		new_enhance = enhance < 0 ?  sinfo.rescue_enhance_f : sbe_enhance_f;

		if (thr->boost_info.cur_stage == FPSGO_JERK_SECOND)
			rescue_opp_c = sinfo.rescue_second_copp;

		rescue_opp_f = fbt_get_rescue_opp_f();
		blc_wt = fbt_get_new_base_blc(pld, floor, new_enhance, rescue_opp_f, rescue_opp_c);
		if (separate_aa) {
			blc_wt_b = fbt_get_new_base_blc(pld, floor, new_enhance,
						rescue_opp_f, rescue_opp_c);
			blc_wt_m = fbt_get_new_base_blc(pld, floor, new_enhance,
						rescue_opp_f, rescue_opp_c);
		}

		if (!blc_wt)
			goto leave;
		thr->boost_info.sbe_rescue = 1;

		if (thr->boost_info.cur_stage != FPSGO_JERK_SECOND) {
			blc_wt = fbt_limit_capacity(blc_wt, 1);
			if (separate_aa) {
				blc_wt_b = fbt_limit_capacity(blc_wt_b, 1);
				blc_wt_m = fbt_limit_capacity(blc_wt_m, 1);
			}
			fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);

			thr->boost_info.cur_stage = FPSGO_JERK_SBE;

			if (thr->pid == sinfo.max_blc_pid && thr->buffer_id
				== sinfo.max_blc_buffer_id)
				fbt_set_max_blc_stage(FPSGO_JERK_SBE);
		}
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);

		if (sinfo.boost_ta) {
			fbt_set_boost_value(blc_wt);
			fbt_set_max_blc_cur(blc_wt);
		} else {
			fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, FPSGO_JERK_FIRST,
				thr->pid, thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m,
				&max_cap, &max_cap_b, &max_cap_m, &max_util,
				&max_util_b, &max_util_m);
			fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
				max_cap_b, max_cap_m, max_util, max_util_b,
				max_util_m, 0, FPSGO_JERK_FIRST);
		}

		thr->boost_info.last_blc = blc_wt;

		if (separate_aa) {
			thr->boost_info.last_blc_b = blc_wt_b;
			thr->boost_info.last_blc_m = blc_wt_m;
		}

		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, new_enhance, "sbe rescue");

		/* support mode: sbe rescue until queue end */
		if (start == SBE_RESCUE_MODE_UNTIL_QUEUE_END) {
			thr->boost_info.sbe_rescue = 0;
			sbe_rescuing_frame_id_legacy = -1;
			fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "sbe rescue");
		}

	} else {
		if (thr->boost_info.sbe_rescue == 0)
			goto leave;
		if (frame_id < sbe_rescuing_frame_id_legacy)
			goto leave;
		sbe_rescuing_frame_id_legacy = -1;
		thr->boost_info.sbe_rescue = 0;
		blc_wt = thr->boost_info.last_normal_blc;
		if (separate_aa) {
			blc_wt_b = thr->boost_info.last_normal_blc_b;
			blc_wt_m = thr->boost_info.last_normal_blc_m;
		}
		if (!blc_wt || (separate_aa && !(blc_wt_b && blc_wt_m)))
			goto leave;

		/* find max perf index */
		fbt_find_max_blc(&temp_blc, &temp_blc_pid, &temp_blc_buffer_id,
				&temp_blc_dep_num, temp_blc_dep);
		rescue_opp_f = fbt_get_rescue_opp_f();
		fbt_get_new_base_blc(pld, temp_blc, 0, rescue_opp_f, sinfo.bhr_opp);
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);
		if (sinfo.boost_ta) {
			fbt_set_boost_value(blc_wt);
			fbt_set_max_blc_cur(blc_wt);
		} else {
			fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, FPSGO_JERK_FIRST,
				thr->pid, thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m,
				&max_cap, &max_cap_b, &max_cap_m, &max_util,
				&max_util_b, &max_util_m);
			fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
				max_cap_b, max_cap_m, max_util, max_util_b,
				max_util_m, 0, FPSGO_JERK_FIRST);
		}
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "sbe rescue");
	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, blc_wt, "perf idx");

leave:
	mutex_unlock(&fbt_mlock);
	kfree(pld);
}

int get_ux_list_length(struct list_head *head)
{
	struct list_head *temp;
	int count = 0;

	list_for_each(temp, head) {
		count++;
	}
	return count;
}

struct ux_scroll_info *search_ux_scroll_info(unsigned long long ts,
						 struct render_info *thr)
{
	struct ux_scroll_info *scr;

	list_for_each_entry (scr, &thr->scroll_list, queue_list) {
		if (scr->start_ts == ts)
			return scr;
	}
	return NULL;
}

void enqueue_ux_scroll_info(int type, unsigned long long start_ts, struct render_info *thr)
{
	int list_length;
	struct ux_scroll_info *new_node;
	struct ux_scroll_info *tmp;
	struct ux_scroll_info *first_node = NULL;

	if (!sbe_dy_rescue_enable)
		return;

	if (!thr)
		return;

	new_node = kmem_cache_alloc(ux_scroll_info_cachep, GFP_KERNEL);
	if (!new_node)
		return;

	new_node->score = kmalloc(
					(RESCUE_MAX_MONITOR_DROP_ARR_SIZE) * sizeof(int)
					, GFP_KERNEL | __GFP_ZERO);

	if (!new_node->score) {
		kmem_cache_free(ux_scroll_info_cachep, new_node);
		return;
	}

	INIT_LIST_HEAD(&new_node->frame_list);
	new_node->start_ts = start_ts;
	new_node->type = type;

	list_length = get_ux_list_length(&thr->scroll_list);
	if (list_length == 0) {
		for (size_t i = 0; i < HWUI_MAX_FRAME_SAME_TIME; i++) {
			if (thr->tmp_hwui_frame_info_arr[i])
				continue;

			struct hwui_frame_info *frame = kmem_cache_alloc(hwui_frame_info_cachep, GFP_KERNEL);

			if (!frame)//allocate memory failed
				break;

			thr->tmp_hwui_frame_info_arr[i] = frame;
		}
		thr->hwui_arr_idx = 0;
	}

	if (list_length >= scroll_cnt) {
		list_for_each_entry_reverse(tmp, &thr->scroll_list, queue_list) {
			first_node = tmp;
			break;
		}
		if (first_node != NULL)
			release_scroll(first_node);

	}
	list_add(&new_node->queue_list, &thr->scroll_list);

}

static void update_hwui_frame_info(struct render_info *info,
		struct hwui_frame_info *frame, unsigned long long id,
		unsigned long long start_ts, unsigned long long end_ts,
		unsigned long long rsc_start_ts, unsigned long long rsc_end_ts,
		int rescue_reason)
{
	if (frame && frame->frameID == id) {
		// Update the fields for this frame.
		if (start_ts) {
			frame->start_ts = start_ts;
			frame->display_rate = info->boost_info.target_fps;
		}

		if (rsc_start_ts) {
			frame->rescue = true;
			frame->rsc_start_ts = rsc_start_ts;
		}
		if (rsc_end_ts)
			frame->rsc_dur_ts = rsc_end_ts - frame->rsc_start_ts;
		if (rescue_reason)
			frame->rescue_reason = rescue_reason;
		if (end_ts) {
			frame->end_ts = end_ts;
			frame->dur_ts = end_ts - frame->start_ts;
			if (frame->dur_ts > info->boost_info.target_time) {
				frame->overtime = true;
				frame->rescue_suc = false;
			} else {
				frame->overtime = false;
				if(frame->rescue)
					frame->rescue_suc = true;
			}
			if (frame->rescue && !frame->rsc_dur_ts)
				frame->rsc_dur_ts = end_ts - frame->rsc_start_ts;

		}
	}
}

void reset_hwui_frame_info(struct hwui_frame_info *frame)
{
	if (frame) {
		//make frameID max
		frame->frameID = -1;
		frame->start_ts = 0;
		frame->end_ts = 0;
		frame->dur_ts = 0;
		frame->rsc_start_ts = 0;
		frame->rsc_dur_ts = 0;
		frame->overtime = false;
		frame->rescue = false;
		frame->rescue_suc = false;
		frame->rescue_reason = 0;
		frame->perf_idx = -1;
		frame->frame_status = 0;
		frame->display_rate = 0;
		frame->promotion = false;
		frame->drop = -1;
	}
}

struct hwui_frame_info *get_valid_hwui_frame_info_from_pool(struct render_info *info)
{
	struct hwui_frame_info *frame;

	if (!info || list_empty(&info->scroll_list))
		return NULL;

	if (info->hwui_arr_idx < 0
			|| info->hwui_arr_idx >= HWUI_MAX_FRAME_SAME_TIME)
		info->hwui_arr_idx = 0;

	frame = info->tmp_hwui_frame_info_arr[info->hwui_arr_idx];

	info->hwui_arr_idx++;
	return frame;
}

struct hwui_frame_info *get_hwui_frame_info_by_frameid(
		struct render_info *info, unsigned long long frameid)
{
	if (!info || list_empty(&info->scroll_list))
		return NULL;

	for (size_t i = 0; i < HWUI_MAX_FRAME_SAME_TIME; i++) {
		struct hwui_frame_info *frame = info->tmp_hwui_frame_info_arr[i];

		if (frame && frameid == frame->frameID)
			return frame;
	}
	return NULL;
}

struct hwui_frame_info *insert_hwui_frame_info_from_tmp(struct render_info *thr, struct hwui_frame_info *frame)
{
	struct hwui_frame_info *new_frame;
	struct ux_scroll_info *last = NULL;

	if (!thr  || list_empty(&thr->scroll_list) || !frame)
		return NULL;

	// add to the frame list.
	last = list_first_entry_or_null(&(thr->scroll_list), struct ux_scroll_info, queue_list);

	if (!last)
		return NULL;

	new_frame = kmem_cache_alloc(hwui_frame_info_cachep, GFP_KERNEL);
	if (!new_frame)
		return NULL;

	new_frame->frameID = frame->frameID;
	new_frame->start_ts = frame->start_ts;
	new_frame->end_ts = frame->end_ts;
	new_frame->dur_ts = frame->dur_ts;
	new_frame->rsc_start_ts = frame->rsc_start_ts;
	new_frame->rsc_dur_ts = frame->rsc_dur_ts;
	new_frame->display_rate = frame->display_rate;
	new_frame->overtime = frame->overtime;
	new_frame->rescue = frame->rescue;
	new_frame->rescue_suc = frame->rescue_suc;
	new_frame->rescue_reason = frame->rescue_reason;
	new_frame->frame_status = frame->frame_status;
	new_frame->perf_idx = frame->perf_idx;
	new_frame->promotion = frame->promotion;
	new_frame->drop = frame->drop;
	list_add_tail(&(new_frame->queue_list), &(last->frame_list));
	return new_frame;
}

void count_scroll_rescue_info(struct render_info *thr, struct hwui_frame_info *hwui_info)
{
	struct ux_scroll_info *scroll_info = NULL;
	unsigned long long target_time;
	int drop = 0;
	int max_monitor_drop_frame;

	if (!thr  || list_empty(&thr->scroll_list))
		return;

	// add to the frame list.
	scroll_info = list_first_entry_or_null(&(thr->scroll_list), struct ux_scroll_info, queue_list);

	if (!scroll_info)
		return;

	target_time = thr->boost_info.target_time;
	if (target_time <= 0)
		return;

	max_monitor_drop_frame = RESCUE_MAX_MONITOR_DROP_ARR_SIZE - 1;
	if (hwui_info->rescue) {
		// a. check is running or continue rescue
		if ((hwui_info->rescue_reason & RESCUE_COUNTINUE_RESCUE) != 0
				|| hwui_info->dur_ts <= 0) {
			return;
		}
		scroll_info->rescue_cap_count += hwui_info->perf_idx;
		scroll_info->rescue_frame_time_count += hwui_info->dur_ts;
		scroll_info->rescue_frame_count += 1;

		if (hwui_info->dur_ts <= target_time) {
			scroll_info->score[0] += 1;
			hwui_info->drop = 0;
		} else {
			drop = (int)div64_u64(hwui_info->dur_ts, target_time);
			if (drop < 0)
				drop = 0;

			hwui_info->drop = drop;
			if (drop < max_monitor_drop_frame)
				scroll_info->score[drop] += 1;
			else
				scroll_info->score[max_monitor_drop_frame] += 1;
		}
		scroll_info->jank_count++;
		fpsgo_main_trace("%d SBE_UX: frameid %lld cap:%d dur_ts %lld drop %d",
					thr->pid, hwui_info->frameID,
					hwui_info->perf_idx, hwui_info->dur_ts, drop);
	}

}

void fbt_init_ux(struct render_info *info)
{
	INIT_LIST_HEAD(&(info->scroll_list));
	info->hwui_arr_idx = 0;
	info->sbe_rescuing_frame_id = -1;
	info->rescue_start_time = 0;
	info->buffer_count_filter = 0;
	info->rescue_more_count = 0;
}

void fbt_del_ux(struct render_info *info)
{
	if (!info)
		return;

	if (ux_general_policy)
		fpsgo_reset_deplist_task_priority(info);

	if (sbe_dy_rescue_enable) {
		release_all_ux_info(info);
		for (size_t i = 0; i < HWUI_MAX_FRAME_SAME_TIME; i++) {
			if (info->tmp_hwui_frame_info_arr[i])
				kmem_cache_free(hwui_frame_info_cachep, info->tmp_hwui_frame_info_arr[i]);
		}
	}
	//reset sbe tag when render del
	fpsgo_systrace_c_fbt(info->pid, 0, 0, "sbe_set_ctrl");
	list_del(&(info->scroll_list));

	//delete ux_frame_info
	fpsgo_ux_reset(info);
}

#if IS_ENABLED(CONFIG_ARM64)
int updata_smart_launch_capacity_tb(void)
{
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	int i = 0;
	int big_clsuter_num = cluster_num -1;
	int big_cluster_cpu = capacity_info[big_clsuter_num].first_cpu_id;

	if ((cluster_num > 0) && capacity_info &&
		capacity_info[big_clsuter_num].capacity &&
		(nr_freq_opp_cnt > 0)) {
		for (i = 0; i < nr_freq_opp_cnt; i++) {
			capacity_info[big_clsuter_num].capacity[i]
			= pd_X2Y(big_cluster_cpu, i, OPP, CAP, true, DPT_CALL_FBT_CLUSTER_X2Y);
		}
	}
#endif
	return 0;
}

static inline int abs_f(int value)
{
	return value < 0 ? -value : value;
}

int findBestFitNumTabIdx(int *numbers, int size, int target)
{
	int idx = 0;
	int closest = 0;
	int min_diff = 0;
	int diff = 0;

	if (size == 0)
		return idx;

	closest = numbers[0];
	min_diff = abs_f(target - closest);

	for (int i = 0; i < size; ++i) {
		diff = abs_f(target - numbers[i]);
		if (diff < min_diff) {
			min_diff = diff;
			closest = numbers[i];
			idx = i;
		}
	}
	return idx;
}

int fpsgo_notify_smart_launch_algorithm(int feedback_time, int target_time,
			int pre_opp, int ration)
{
	int delta = 0;
	int gap_capacity = 0;
	int next_capacity = 0;
	int pre_capacity = 0;
	int min_cap = 0;
	int max_cap = 1024;
	int kp = 1;
	int big_cluster = cluster_num -1;
	int next_opp = pre_opp;

	if (target_time <= 0 || feedback_time <= 0
		|| pre_opp < 0 || ration < 0)
		return next_opp;

	if (capacity_info && big_cluster>= 0 &&
		capacity_info[big_cluster].capacity &&
		nr_freq_opp_cnt >= 1 &&
		pre_opp <= nr_freq_opp_cnt-1) {
		pre_capacity = capacity_info[big_cluster].capacity[pre_opp];
		next_capacity = pre_capacity;
		min_cap = capacity_info[big_cluster].capacity[nr_freq_opp_cnt -1];
		max_cap = capacity_info[big_cluster].capacity[0];
	} else {
		return next_opp;
	}

	delta = feedback_time - target_time;

	if (delta <= 0) {
		delta = abs(delta) << 10;
		kp = div64_u64(delta, target_time);
		gap_capacity = (kp * pre_capacity) >> 10;
		gap_capacity = gap_capacity * (100 - ration);
		gap_capacity = div64_u64(gap_capacity, 100);
		next_capacity = pre_capacity - gap_capacity;
	} else {
		delta = delta << 10;
		kp = div64_u64(delta, target_time);
		gap_capacity = (kp * pre_capacity) >> 10;
		gap_capacity = gap_capacity * (100 + ration);
		gap_capacity = div64_u64(gap_capacity, 100);
		next_capacity = pre_capacity + gap_capacity;
	}

	next_capacity = clamp(next_capacity, min_cap, max_cap);
	next_opp = findBestFitNumTabIdx(capacity_info[big_cluster].capacity,
				nr_freq_opp_cnt, next_capacity);
	return next_opp;
}

int init_smart_launch_capacity_tb(int cluster_num)
{
	struct cpufreq_policy *policy;
	int cluster = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		if (cluster_num <= 0 || cluster >= cluster_num)
			break;
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		capacity_info[cluster].first_cpu_id = cpumask_first(policy->related_cpus);
		capacity_info[cluster].num_opp =
		fpsgo_arch_nr_get_opp_cpu(capacity_info[cluster].first_cpu_id);
		capacity_info[cluster].capacity = kcalloc(nr_freq_opp_cnt,
					sizeof(unsigned int), GFP_KERNEL);

		cluster++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
	return 0;
}
#endif

void init_smart_launch_engine(void)
{
#if IS_ENABLED(CONFIG_ARM64)
	smart_launch_off_on = fbt_get_ux_smart_launch_enable();
	if (smart_launch_off_on) {
		cluster_num  = fpsgo_arch_nr_clusters();
		if (cluster_num != SMART_LAUNCH_BOOST_SUPPORT_CLUSTER_NUM)
			return;
		nr_freq_opp_cnt = fpsgo_arch_nr_max_opp_cpu();
		capacity_info = kcalloc(cluster_num,
				sizeof(struct smart_launch_capacity_info), GFP_KERNEL);
		init_smart_launch_capacity_tb(cluster_num);
		updata_smart_launch_capacity_tb();
		fpsgo_notify_smart_launch_algorithm_fp =
		fpsgo_notify_smart_launch_algorithm;
	}
#endif
}

void destroy_smart_launch_capinfo(void)
{
#if IS_ENABLED(CONFIG_ARM64)
	int i = 0;

	if (smart_launch_off_on) {
		for (i = 0; i < cluster_num; i++) {
			if (capacity_info && capacity_info[i].capacity)
				kfree(capacity_info[i].capacity);
		}
		kfree(capacity_info);
	}
#endif
}

void __exit fbt_cpu_ux_exit(void)
{
	destroy_smart_launch_capinfo();
	kmem_cache_destroy(frame_info_cachep);
	kmem_cache_destroy(ux_scroll_info_cachep);
	kmem_cache_destroy(hwui_frame_info_cachep);
}

int __init fbt_cpu_ux_init(void)
{
	fpsgo_ux_gcc_enable = 0;
	sbe_rescue_enable = fbt_get_default_sbe_rescue_enable();
	init_smart_launch_engine();
	ux_general_policy = fbt_get_ux_scroll_policy_type();
	ux_general_policy_type = 0;
	ux_general_policy_dpt_setwl = 0;
	sbe_rescuing_frame_id_legacy = -1;
	sbe_enhance_f = 50;
	sbe_dy_max_enhance = 70;
	sbe_dy_enhance_margin = 15;
	sbe_dy_frame_threshold = 3;
	sbe_dy_rescue_enable = 1;
	scroll_cnt = 6;
	set_deplist_vip = 1;
	gas_threshold = 10;
	gas_threshold_for_low_TLP = 10;
	gas_threshold_for_high_TLP = 5;
	global_dfrc_fps_limit = UX_TARGET_DEFAULT_FPS;

	frame_info_cachep = kmem_cache_create("ux_frame_info",
		sizeof(struct ux_frame_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	ux_scroll_info_cachep = kmem_cache_create("ux_scroll_info",
		sizeof(struct ux_scroll_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	hwui_frame_info_cachep = kmem_cache_create("hwui_frame_info",
		sizeof(struct hwui_frame_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!frame_info_cachep || !ux_scroll_info_cachep || !hwui_frame_info_cachep)
		return -1;

	return 0;
}
