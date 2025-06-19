// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include "sugov/cpufreq.h"

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fps_composer.h"
#include "fbt_cpu.h"
#include "fbt_cpu_ux.h"
#include "fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fbt_cpu_platform.h"
#include "fpsgo_frame_info.h"

#define MAX_FPSGO_CB_NUM 5

static struct kobject *comp_kobj;
static struct workqueue_struct *composer_wq;
static struct hrtimer recycle_hrt;
static struct rb_root fpsgo_com_policy_cmd_tree;
static int bypass_non_SF = 1;
static int fpsgo_control;
static int control_hwui;
/* EGL, CPU, CAMREA */
static int control_api_mask = 22;
static int recycle_idle_cnt;
static int recycle_active = 1;
static int fps_align_margin = 5;
static int total_fpsgo_com_policy_cmd_num;
static int fpsgo_is_boosting;
static int jank_detection_is_ready;
static unsigned long long last_update_sbe_dep_ts;

//UX SCROLLING
static int ux_general_policy;
static int ux_scroll_count;

// touch latency
static int fpsgo_touch_latency_ko_ready;

// mfrc
static int mfrc_active;
static int mfrc_by_pass_frame_num = 2;

static int cam_bypass_window_ms;

static void fpsgo_com_notify_to_do_recycle(struct work_struct *work);
static DECLARE_WORK(do_recycle_work, fpsgo_com_notify_to_do_recycle);
static DEFINE_MUTEX(recycle_lock);
static DEFINE_MUTEX(fpsgo_com_policy_cmd_lock);
static DEFINE_MUTEX(fpsgo_boost_cb_lock);
static DEFINE_MUTEX(fpsgo_boost_lock);
static DEFINE_MUTEX(user_hint_lock);
static DEFINE_MUTEX(fpsgo_frame_info_cb_lock);

static fpsgo_notify_is_boost_cb notify_fpsgo_boost_cb_list[MAX_FPSGO_CB_NUM];
static struct render_frame_info_cb fpsgo_frame_info_cb_list[FPSGO_MAX_CALLBACK_NUM];

typedef void (*heavy_fp)(int jank, int pid);
int (*fpsgo2jank_detection_register_callback_fp)(heavy_fp cb);
EXPORT_SYMBOL(fpsgo2jank_detection_register_callback_fp);
int (*fpsgo2jank_detection_unregister_callback_fp)(heavy_fp cb);
EXPORT_SYMBOL(fpsgo2jank_detection_unregister_callback_fp);

static enum hrtimer_restart prepare_do_recycle(struct hrtimer *timer)
{
	if (composer_wq)
		queue_work(composer_wq, &do_recycle_work);
	else
		schedule_work(&do_recycle_work);

	return HRTIMER_NORESTART;
}

int fpsgo_get_fpsgo_is_boosting(void)
{
	int is_boosting;

	mutex_lock(&fpsgo_boost_lock);
	is_boosting = fpsgo_is_boosting;
	mutex_unlock(&fpsgo_boost_lock);

	return is_boosting;
}

void init_fpsgo_is_boosting_callback(void)
{
	int i;

	mutex_lock(&fpsgo_boost_cb_lock);

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++)
		notify_fpsgo_boost_cb_list[i] = NULL;

	mutex_unlock(&fpsgo_boost_cb_lock);
}

/*
 *	int register_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb)
 *	Users cannot use lock outside register function.
 */
int register_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb)
{
	int i, ret = 1;

	mutex_lock(&fpsgo_boost_cb_lock);

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++) {
		if (notify_fpsgo_boost_cb_list[i] == NULL) {
			notify_fpsgo_boost_cb_list[i] = func_cb;
			ret = 0;
			break;
		}
	}
	if (ret)
		fpsgo_main_trace("[%s] Cannot register!, func: %p", __func__, func_cb);
	else
		notify_fpsgo_boost_cb_list[i](fpsgo_get_fpsgo_is_boosting());

	mutex_unlock(&fpsgo_boost_cb_lock);

	return ret;
}
EXPORT_SYMBOL(register_get_fpsgo_is_boosting);

int unregister_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb)
{
	int i, ret = 1;

	mutex_lock(&fpsgo_boost_cb_lock);

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++) {
		if (notify_fpsgo_boost_cb_list[i] == func_cb) {
			notify_fpsgo_boost_cb_list[i] = NULL;
			ret = 0;
			break;
		}
	}
	if (ret) {
		fpsgo_main_trace("[%s] Cannot unregister!, func: %p",
			__func__, func_cb);
	}
	mutex_unlock(&fpsgo_boost_cb_lock);

	return ret;
}
EXPORT_SYMBOL(unregister_get_fpsgo_is_boosting);

int fpsgo_com2other_notify_fpsgo_is_boosting(int boost)
{
	int i, ret = 1;

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++) {
		if (notify_fpsgo_boost_cb_list[i]) {
			notify_fpsgo_boost_cb_list[i](boost);
			fpsgo_main_trace("[%s] Call %d func_cb: %p",
				__func__, i, notify_fpsgo_boost_cb_list[i]);
		}
	}

	return ret;
}

void fpsgo_com_notify_fpsgo_is_boost(int enable)
{
	mutex_lock(&fpsgo_boost_cb_lock);
	mutex_lock(&fpsgo_boost_lock);
	if (!fpsgo_is_boosting && enable) {
		fpsgo_is_boosting = 1;
		fpsgo_com2other_notify_fpsgo_is_boosting(1);
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		if (fpsgo_notify_fbt_is_boost_fp)
			fpsgo_notify_fbt_is_boost_fp(1);
#endif
	} else if (fpsgo_is_boosting && !enable) {
		fpsgo_is_boosting = 0;
		fpsgo_com2other_notify_fpsgo_is_boosting(0);
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		if (fpsgo_notify_fbt_is_boost_fp)
			fpsgo_notify_fbt_is_boost_fp(0);
#endif
	}
	mutex_unlock(&fpsgo_boost_lock);
	mutex_unlock(&fpsgo_boost_cb_lock);
}

int fpsgo_register_frame_info_callback(unsigned long mask, fpsgo_frame_info_callback cb)
{
	int i = 0;
	int ret = -ENOMEM;
	struct render_frame_info_cb *iter = NULL;

	mutex_lock(&fpsgo_frame_info_cb_lock);
	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		if (fpsgo_frame_info_cb_list[i].func_cb == NULL)
			break;
	}

	if (i >= FPSGO_MAX_CALLBACK_NUM || i < 0)
		goto out;

	iter = &fpsgo_frame_info_cb_list[i];
	iter->mask = mask;
	iter->func_cb = cb;
	memset(&iter->info_iter, 0, sizeof(struct render_frame_info));
	ret = i;

out:
	mutex_unlock(&fpsgo_frame_info_cb_lock);
	return ret;
}
EXPORT_SYMBOL(fpsgo_register_frame_info_callback);

int fpsgo_unregister_frame_info_callback(fpsgo_frame_info_callback cb)
{
	int i = 0;
	int ret = -ESPIPE;
	struct render_frame_info_cb *iter = NULL;

	mutex_lock(&fpsgo_frame_info_cb_lock);
	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		iter = &fpsgo_frame_info_cb_list[i];
		if (iter->func_cb == cb) {
			iter->mask = 0;
			iter->func_cb = NULL;
			memset(&iter->info_iter, 0, sizeof(struct render_frame_info));
			ret = i;
		}
	}
	mutex_unlock(&fpsgo_frame_info_cb_lock);

	return ret;
}
EXPORT_SYMBOL(fpsgo_unregister_frame_info_callback);

void fpsgo_notify_frame_info_callback(unsigned long cmd, struct render_info *r_iter)
{
	int i;
	struct render_frame_info_cb *iter = NULL;

	mutex_lock(&fpsgo_frame_info_cb_lock);
	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		iter = &fpsgo_frame_info_cb_list[i];

		if (iter->func_cb && cmd & iter->mask) {
			iter->info_iter.tgid = r_iter->tgid;
			iter->info_iter.pid = r_iter->pid;
			iter->info_iter.buffer_id = r_iter->buffer_id;

			if (test_bit(GET_FPSGO_Q2Q_TIME, &cmd))
				iter->info_iter.q2q_time = r_iter->Q2Q_time;
			if (test_bit(GET_FPSGO_PERF_IDX, &cmd))
				iter->info_iter.blc = r_iter->boost_info.last_normal_blc;
			if (test_bit(GET_SBE_CTRL, &cmd))
				iter->info_iter.sbe_control_flag =
					r_iter->frame_type == FRAME_HINT_TYPE || r_iter->sbe_control_flag;
			if (test_bit(GET_FPSGO_JERK_BOOST, &cmd))
				iter->info_iter.jerk_boost_flag = r_iter->boost_info.cur_stage + 1;

			iter->func_cb(cmd, &iter->info_iter);
		}
	}
	mutex_unlock(&fpsgo_frame_info_cb_lock);
}

static void fpsgo_com_do_jank_detection_hint(struct work_struct *psWork)
{
	int local_jank;
	int local_pid;
	struct jank_detection_hint *hint = NULL;

	hint = container_of(psWork, struct jank_detection_hint, sWork);
	local_jank = hint->jank;
	local_pid = hint->pid;

	fpsgo_render_tree_lock(__func__);
	fpsgo_systrace_c_fbt(local_pid, 0, local_jank, "jank_detection_hint");
	fpsgo_comp2fbt_jank_thread_boost(local_jank, local_pid);
	fpsgo_render_tree_unlock(__func__);

	kfree(hint);
}

static int fpsgo_comp_make_pair_l2q(struct render_info *f_render,
	unsigned long long cur_queue_end, unsigned long long logic_head_ts, int has_logic_head,
	int is_logic_valid, unsigned long long sf_buf_id)
{
	struct FSTB_FRAME_L2Q_INFO *prev_l2q_info, *cur_l2q_info;
	int prev_l2q_index = -1, cur_l2q_index = -1;
	int ret = 0;
	unsigned long long expected_fps = 0, expected_time = 0;

	prev_l2q_index = f_render->l2q_index;
	cur_l2q_index = (f_render->l2q_index + 1) % MAX_SF_BUFFER_SIZE;
	prev_l2q_info = &(f_render->l2q_info[prev_l2q_index]);
	cur_l2q_info = &(f_render->l2q_info[cur_l2q_index]);
	f_render->l2q_index = cur_l2q_index;
	if (!prev_l2q_info || !cur_l2q_info) {
		ret = -EINVAL;
		goto out;
	}

	expected_fps = (unsigned long long) f_render->boost_info.target_fps;
	expected_time = div64_u64(NSEC_PER_SEC, expected_fps);
	cur_l2q_info->sf_buf_id = sf_buf_id;
	cur_l2q_info->queue_end_ns = cur_queue_end;

	if (logic_head_ts && cur_queue_end > logic_head_ts) {
		if (is_logic_valid != 0) {
			fpsgo_main_trace("[%s]pid=%d, cur_que_end=%llu, logic_head_ts=%llu", __func__,
				f_render->pid, cur_queue_end, logic_head_ts);
			ret = 1;
		}
		cur_l2q_info->logic_head_fixed_ts = logic_head_ts;
		cur_l2q_info->logic_head_ts = logic_head_ts;
	} else {
		cur_l2q_info->logic_head_fixed_ts = prev_l2q_info->logic_head_fixed_ts +
			expected_time;
		cur_l2q_info->logic_head_ts = 0;
	}
	cur_l2q_info->is_logic_head_alive = has_logic_head;

	if (cur_queue_end > cur_l2q_info->logic_head_fixed_ts)
		cur_l2q_info->l2q_ts = cur_queue_end - cur_l2q_info->logic_head_fixed_ts;
	else  // Error handling 拿前一框L2Q
		cur_l2q_info->l2q_ts = prev_l2q_info->l2q_ts;

	fpsgo_main_trace("[fstb_logical][%d]l_ts=%llu,l_fixed_ts=%llu,q=%llu,l2q_ts=%llu,exp_t=%llu,has=%d",
		f_render->pid, cur_l2q_info->logic_head_ts, cur_l2q_info->logic_head_fixed_ts,
		cur_queue_end, cur_l2q_info->l2q_ts, expected_time,
		cur_l2q_info->is_logic_head_alive);
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id,
			div64_u64(cur_l2q_info->logic_head_fixed_ts, 1000000), "L_fixed");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id, cur_l2q_info->sf_buf_id,
		"L2Q_sf_buf_id");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id, cur_l2q_info->l2q_ts,
		"L2Q_ts");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id,
		div64_u64(cur_queue_end, 1000000), "L2Q_q_end_ts");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id,
		div64_u64(logic_head_ts, 1000000), "Logical ts");
out:
	return ret;
}

static void fpsgo_com_receive_jank_detection(int jank, int pid)
{
	struct jank_detection_hint *hint = NULL;

	fpsgo_systrace_c_fbt(pid, 0, jank, "jank_detection_hint_ori");

	if (!composer_wq)
		return;

	hint = kmalloc(sizeof(struct jank_detection_hint), GFP_ATOMIC);
	if (!hint)
		return;

	hint->jank = jank;
	hint->pid = pid;

	INIT_WORK(&hint->sWork, fpsgo_com_do_jank_detection_hint);
	queue_work(composer_wq, &hint->sWork);
}

static void fpsgo_com_get_l2q_time(int pid, unsigned long long buf_id, int tgid,
		unsigned long long enqueue_end_time, unsigned long long prev_queue_end_ts,
		unsigned long long pprev_queue_end_ts, unsigned long long dequeue_start_ts,
		unsigned long long sf_buf_id, struct render_info *f_render)
{
		unsigned long long logic_head_ts = 0;
		int has_logic_head = 0, is_logic_valid = 0;
		int l2q_enable_pid_final = 0;

		if (!f_render)
			return;

		l2q_enable_pid_final = f_render->attr.l2q_enable_by_pid;
		if (fpsgo_touch_latency_ko_ready && l2q_enable_pid_final) {
			is_logic_valid = fpsgo_comp2fstb_get_logic_head(pid, buf_id,
				tgid, enqueue_end_time, prev_queue_end_ts, pprev_queue_end_ts,
				dequeue_start_ts, &logic_head_ts, &has_logic_head);
			fpsgo_comp_make_pair_l2q(f_render, enqueue_end_time, logic_head_ts,
				has_logic_head, is_logic_valid, sf_buf_id);
		}
}

static void fpsgo_com_delete_policy_cmd(struct fpsgo_com_policy_cmd *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct fpsgo_com_policy_cmd *tmp_iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	if (iter) {
		if (iter->bypass_non_SF_by_pid == BY_PID_DEFAULT_VAL &&
			iter->control_api_mask_by_pid == BY_PID_DEFAULT_VAL &&
			iter->control_hwui_by_pid == BY_PID_DEFAULT_VAL &&
			iter->app_cam_meta_min_fps == BY_PID_DEFAULT_VAL &&
			iter->dep_loading_thr_by_pid == BY_PID_DEFAULT_VAL &&
			iter->cam_bypass_window_ms_by_pid == BY_PID_DEFAULT_VAL) {
			min_iter = iter;
			goto delete;
		} else
			return;
	}

	if (RB_EMPTY_ROOT(&fpsgo_com_policy_cmd_tree))
		return;

	rbn = rb_first(&fpsgo_com_policy_cmd_tree);
	while (rbn) {
		tmp_iter = rb_entry(rbn, struct fpsgo_com_policy_cmd, rb_node);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
		rbn = rb_next(rbn);
	}

	if (!min_iter)
		return;

delete:
	rb_erase(&min_iter->rb_node, &fpsgo_com_policy_cmd_tree);
	kfree(min_iter);
	total_fpsgo_com_policy_cmd_num--;
}

static struct fpsgo_com_policy_cmd *fpsgo_com_get_policy_cmd(int tgid,
	unsigned long long ts, int create)
{
	struct rb_node **p = &fpsgo_com_policy_cmd_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fpsgo_com_policy_cmd *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct fpsgo_com_policy_cmd, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!create)
		return NULL;

	iter = kzalloc(sizeof(struct fpsgo_com_policy_cmd), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->bypass_non_SF_by_pid = BY_PID_DEFAULT_VAL;
	iter->control_api_mask_by_pid = BY_PID_DEFAULT_VAL;
	iter->control_hwui_by_pid = BY_PID_DEFAULT_VAL;
	iter->app_cam_meta_min_fps = BY_PID_DEFAULT_VAL;
	iter->dep_loading_thr_by_pid = BY_PID_DEFAULT_VAL;
	iter->cam_bypass_window_ms_by_pid = BY_PID_DEFAULT_VAL;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fpsgo_com_policy_cmd_tree);
	total_fpsgo_com_policy_cmd_num++;

	if (total_fpsgo_com_policy_cmd_num > FPSGO_MAX_TREE_SIZE)
		fpsgo_com_delete_policy_cmd(NULL);

	return iter;
}

static void fpsgo_com_set_policy_cmd(int cmd, int value, int tgid,
	unsigned long long ts, int op)
{
	struct fpsgo_com_policy_cmd *iter = NULL;

	iter = fpsgo_com_get_policy_cmd(tgid, ts, op);
	if (iter) {
		if (cmd == 0)
			iter->bypass_non_SF_by_pid = value;
		else if (cmd == 1)
			iter->control_api_mask_by_pid = value;
		else if (cmd == 2)
			iter->control_hwui_by_pid = value;
		else if (cmd == 3)
			iter->dep_loading_thr_by_pid = value;
		else if (cmd == 4)
			iter->mfrc_active_by_pid = value;
		else if (cmd == 5)
			iter->cam_bypass_window_ms_by_pid = value;

		if (!op)
			fpsgo_com_delete_policy_cmd(iter);
	}
}

/*
 * UX: sbe_ctrl
 * VP: control_hwui
 * Camera: fpsgo_control
 * Game: fpsgo_control_pid
 */
int fpsgo_com_check_frame_type(int pid, int tgid, int queue_SF, int api,
	int hwui, int sbe_ctrl, int fpsgo_control_pid)
{
	int local_bypass_non_SF = bypass_non_SF;
	int local_control_api_mask = control_api_mask;
	int local_control_hwui = control_hwui;
	int local_mfrc_active = mfrc_active;
	struct fpsgo_com_policy_cmd *iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(tgid, 0, 0);
	if (iter) {
		if (iter->bypass_non_SF_by_pid != BY_PID_DEFAULT_VAL)
			local_bypass_non_SF = iter->bypass_non_SF_by_pid;
		if (iter->control_api_mask_by_pid != BY_PID_DEFAULT_VAL)
			local_control_api_mask = iter->control_api_mask_by_pid;
		if (iter->control_hwui_by_pid != BY_PID_DEFAULT_VAL)
			local_control_hwui = iter->control_hwui_by_pid;
		if (iter->mfrc_active_by_pid != BY_PID_DEFAULT_VAL)
			local_mfrc_active = iter->mfrc_active_by_pid;
	}
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	if (sbe_ctrl)
		return NON_VSYNC_ALIGNED_TYPE;

	if (local_bypass_non_SF && !queue_SF)
		return BY_PASS_TYPE;

	if ((local_control_api_mask & (1 << api)) == 0)
		return BY_PASS_TYPE;

	if (hwui == RENDER_INFO_HWUI_TYPE) {
		if (local_control_hwui)
			return NON_VSYNC_ALIGNED_TYPE;
		else
			return BY_PASS_TYPE;
	}

	if (!fpsgo_control && !fpsgo_control_pid)
		return BY_PASS_TYPE;

	if (pid == tgid && pid != fpsgo_get_kfpsgo_tid())
		return BY_PASS_TYPE;

	if (local_mfrc_active)
		return MFRC_FRAME;

	return NON_VSYNC_ALIGNED_TYPE;
}

static int fpsgo_com_check_fps_align(int pid, unsigned long long buffer_id)
{
	int ret = NON_VSYNC_ALIGNED_TYPE;
	int local_tgid = 0;
	int local_app_meta_min_fps = 0;
	int local_qfps_arr_num = 0;
	int local_tfps_arr_num = 0;
	int *local_qfps_arr = NULL;
	int *local_tfps_arr = NULL;
	struct fpsgo_com_policy_cmd *iter = NULL;

	local_qfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_qfps_arr)
		goto out;

	local_tfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_tfps_arr)
		goto out;

	local_tgid = fpsgo_get_tgid(pid);
	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(local_tgid, 0, 0);
	if (iter)
		local_app_meta_min_fps = iter->app_cam_meta_min_fps;
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	fpsgo_other2fstb_get_fps(pid, buffer_id,
		local_qfps_arr, &local_qfps_arr_num, 1,
		local_tfps_arr, &local_tfps_arr_num, 1,
		NULL, NULL, 0);

	if (local_qfps_arr[0] > local_tfps_arr[0] + fps_align_margin ||
		(local_app_meta_min_fps > 0 && local_qfps_arr[0] <= local_app_meta_min_fps)) {
		ret = BY_PASS_TYPE;
		fpsgo_systrace_c_fbt_debug(pid, buffer_id, 1, "fps_no_align");
	} else
		fpsgo_systrace_c_fbt_debug(pid, buffer_id, 0, "fps_no_align");

	xgf_trace("[comp][%d][0x%llx] | %s local_qfps:%d local_tfps:%d",
		pid, buffer_id, __func__, local_qfps_arr[0], local_tfps_arr[0]);

out:
	kfree(local_qfps_arr);
	kfree(local_tfps_arr);
	return ret;
}

static int fpsgo_com_check_BQ_type(int *bq_type,
	int pid, unsigned long long buffer_id)
{
	int ret = BY_PASS_TYPE;
	int local_bq_type = ACQUIRE_UNKNOWN_TYPE;
	int local_dep_arr_num = 0;
	int *local_dep_arr = NULL;

	local_bq_type = fpsgo_get_acquire_queue_pair_by_self(pid, buffer_id);
	if (local_bq_type == ACQUIRE_SELF_TYPE) {
		fpsgo_systrace_c_fbt(pid, buffer_id, local_bq_type, "bypass_acquire");
		goto out;
	}

	local_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(int), GFP_KERNEL);
	if (!local_dep_arr) {
		fpsgo_main_trace("[comp][%d][0x%llx] | %s dep_arr malloc err",
			pid, buffer_id, __func__);
		goto out;
	}
	local_dep_arr_num = fpsgo_comp2xgf_get_dep_list(pid, MAX_DEP_NUM,
		local_dep_arr, buffer_id);

	local_bq_type = fpsgo_get_acquire_queue_pair_by_group(pid,
			local_dep_arr, local_dep_arr_num, buffer_id);
	kfree(local_dep_arr);
	if (local_bq_type == ACQUIRE_CAMERA_TYPE)
		goto out;

	if (fpsgo_other2fstb_check_cam_do_frame())
		local_bq_type = fpsgo_check_all_render_blc(pid, buffer_id);
	if (local_bq_type == ACQUIRE_OTHER_TYPE)
		fpsgo_systrace_c_fbt(pid, buffer_id, local_bq_type, "bypass_acquire");

out:
	*bq_type = local_bq_type;
	ret = (local_bq_type == ACQUIRE_CAMERA_TYPE) ?
			NON_VSYNC_ALIGNED_TYPE : BY_PASS_TYPE;
	if (ret == NON_VSYNC_ALIGNED_TYPE)
		ret = fpsgo_com_check_fps_align(pid, buffer_id);
	return ret;
}

static void fpsgo_com_check_bypass_closed_loop(struct render_info *iter)
{
	int local_loading = 0;
	int loading_thr;
	int window_ms;
	struct fpsgo_com_policy_cmd *policy_iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	policy_iter = fpsgo_com_get_policy_cmd(iter->tgid, 0, 0);
	loading_thr = policy_iter ? policy_iter->dep_loading_thr_by_pid : -1;
	window_ms = policy_iter ? policy_iter->cam_bypass_window_ms_by_pid : cam_bypass_window_ms;
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	if (loading_thr < 0)
		return;

	if (iter->running_time > 0 && iter->Q2Q_time > 0) {
		iter->sum_cpu_time_us += iter->running_time >> 10;
		iter->sum_q2q_time_us += iter->Q2Q_time >> 10;
		if (!iter->sum_reset_ts)
			iter->sum_reset_ts = iter->t_enqueue_end;

		if (iter->t_enqueue_end - iter->sum_reset_ts >= window_ms * NSEC_PER_MSEC) {
			if (iter->sum_cpu_time_us > 0 && iter->sum_q2q_time_us > 0)
				local_loading = (int)div64_u64(iter->sum_cpu_time_us * 100, iter->sum_q2q_time_us);
			iter->bypass_closed_loop = local_loading < loading_thr;
			iter->sum_cpu_time_us = 0;
			iter->sum_q2q_time_us = 0;
			iter->sum_reset_ts = iter->t_enqueue_end;
		}
	}

	fpsgo_systrace_c_fbt(iter->pid, iter->buffer_id,
		iter->bypass_closed_loop, "bypass_closed_loop");
	fpsgo_main_trace("[comp][%d][0x%llx] run:%llu(%llu) q2q:%llu(%llu) thr:%d(%d) bypass:%d ts:%llu",
		iter->pid, iter->buffer_id,
		iter->running_time, iter->sum_cpu_time_us, iter->Q2Q_time, iter->sum_q2q_time_us,
		loading_thr, window_ms, iter->bypass_closed_loop, iter->sum_reset_ts);
}

static void fpsgo_com_determine_cam_object(struct render_info *iter)
{
	int pre_cond = 0;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(iter->thr_mlock));

	pre_cond = fpsgo_get_acquire_hint_is_enable() &&
		!iter->sbe_control_flag && iter->hwui == RENDER_INFO_HWUI_NONE &&
		iter->frame_type == NON_VSYNC_ALIGNED_TYPE && fpsgo_check_is_cam_apk(iter->tgid);

	if (pre_cond) {
		iter->frame_type = fpsgo_com_check_BQ_type(&iter->bq_type,
			iter->pid, iter->buffer_id);

		if (iter->frame_type == NON_VSYNC_ALIGNED_TYPE)
			iter->frame_type = fpsgo_check_exist_queue_SF(iter->tgid);

		if (iter->frame_type == NON_VSYNC_ALIGNED_TYPE)
			fpsgo_comp2fstb_detect_app_self_ctrl(iter->tgid, iter->pid,
				iter->buffer_id, iter->t_enqueue_end);
	}
}

int fpsgo_com_update_render_api_info(struct render_info *f_render)
{
	struct connect_api_info *connect_api;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(f_render->thr_mlock));

	connect_api = fpsgo_search_and_add_connect_api_info(f_render->pid,
			f_render->buffer_id, 0);

	if (!connect_api)
		return 0;

	f_render->api = connect_api->api;
	list_add(&(f_render->bufferid_list), &(connect_api->render_list));

	return 1;
}

static int fpsgo_com_refetch_buffer(struct render_info *f_render, int pid,
		unsigned long long identifier, int enqueue)
{
	int ret;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;

	if (!f_render)
		return 0;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(f_render->thr_mlock));

	ret = fpsgo_get_BQid_pair(pid, f_render->tgid,
		identifier, &buffer_id, &queue_SF, enqueue);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("refetch %d: %llu, %d, %llu\n",
			pid, buffer_id, queue_SF, identifier);
		fpsgo_main_trace("COMP: refetch %d: %llu, %d, %llu\n",
			pid, buffer_id, queue_SF, identifier);
		return 0;
	}

	f_render->buffer_id = buffer_id;
	f_render->queue_SF = queue_SF;

	if (!f_render->p_blc)
		fpsgo_base2fbt_node_init(f_render);

	return 1;
}

void fpsgo_ctrl2comp_enqueue_start(int pid,
	unsigned long long enqueue_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int ret;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 1);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	/* @buffer_id and @queue_SF MUST be initialized
	 * with @api at the same time
	 */
	if (!f_render->api && identifier) {
		ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 1);
		if (!ret) {
			goto exit;
		}

		ret = fpsgo_com_update_render_api_info(f_render);
		if (!ret) {
			goto exit;
		}
	} else if (identifier) {
		ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 1);
		if (!ret) {
			goto exit;
		}
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	if (f_render->frame_type == MFRC_FRAME) {
		if (f_render->frame_count % mfrc_by_pass_frame_num == 1)
			f_render->frame_type = MFRC_BY_PASS_FRAME;
	}

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
	case MFRC_FRAME:
		f_render->t_enqueue_start = enqueue_start_time;
		fpsgo_com_notify_fpsgo_is_boost(1);
		break;
	case BY_PASS_TYPE:
		f_render->t_enqueue_start = enqueue_start_time;
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->queue_SF, "bypass_sf");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->api, "bypass_api");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->hwui, "bypass_hwui");
		break;
	case MFRC_BY_PASS_FRAME:
		break;
	default:
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

	mutex_lock(&recycle_lock);
	if (recycle_idle_cnt) {
		recycle_idle_cnt = 0;
		if (!recycle_active) {
			recycle_active = 1;
			hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&recycle_lock);

	if (!jank_detection_is_ready &&
		fpsgo2jank_detection_register_callback_fp) {
		fpsgo2jank_detection_register_callback_fp(fpsgo_com_receive_jank_detection);
		jank_detection_is_ready = 1;
		FPSGO_LOGE("fpsgo2jank_detection_register_callback_fp finish\n");
	}
}

void fpsgo_ctrl2comp_enqueue_end(int pid,
	unsigned long long enqueue_end_time,
	unsigned long long identifier,
	unsigned long long sf_buf_id)
{
	int ret;
	unsigned long cb_mask = 0;
	unsigned long long raw_runtime = 0;
	unsigned long long running_time = 0;
	unsigned long long enq_running_time = 0;
	unsigned long long pprev_enqueue_end = 0, prev_enqueue_end = 0;
	struct render_info *f_render = NULL;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret)
		goto exit;

	f_render->hwui = fpsgo_search_and_add_hwui_info(f_render->pid, 0) ?
			RENDER_INFO_HWUI_TYPE : RENDER_INFO_HWUI_NONE;
	f_render->sbe_control_flag =
			fpsgo_search_and_add_sbe_info(f_render->pid, 0) ? 1 : 0;
	f_render->control_pid_flag =
			fpsgo_search_and_add_fps_control_pid(f_render->tgid, 0) ? 1 : 0;

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	fpsgo_com_determine_cam_object(f_render);

	if (f_render->frame_type == MFRC_FRAME) {
		if (f_render->frame_count % mfrc_by_pass_frame_num == 1)
			f_render->frame_type = MFRC_BY_PASS_FRAME;
	}

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
	case MFRC_FRAME:
		pprev_enqueue_end = f_render->prev_t_enqueue_end;
		prev_enqueue_end = f_render->t_enqueue_end;
		if (f_render->t_enqueue_end)
			f_render->Q2Q_time = enqueue_end_time - f_render->t_enqueue_end;
		f_render->prev_t_enqueue_end = f_render->t_enqueue_end;
		f_render->t_enqueue_end = enqueue_end_time;
		f_render->enqueue_length = enqueue_end_time - f_render->t_enqueue_start;
		f_render->enqueue_length_real = f_render->enqueue_length;

		fpsgo_comp2fstb_prepare_calculate_target_fps(pid, f_render->buffer_id,
			enqueue_end_time);

		//reset vip if ux scrolling
		if (get_ux_general_policy() && f_render->sbe_control_flag)
			fpsgo_boost_non_hwui_policy(f_render, 0);

		fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
			&raw_runtime, &running_time, &enq_running_time,
			0, f_render->t_enqueue_end,
			f_render->t_dequeue_start, f_render->t_dequeue_end,
			f_render->t_enqueue_start, f_render->t_enqueue_end, 0);
		f_render->enqueue_length_real = f_render->enqueue_length > enq_running_time ?
						f_render->enqueue_length - enq_running_time : 0;
		fpsgo_systrace_c_fbt_debug(pid, f_render->buffer_id,
			f_render->enqueue_length_real, "enq_length_real");
		f_render->raw_runtime = raw_runtime;
		if (running_time != 0)
			f_render->running_time = running_time;
		if (f_render->bq_type == ACQUIRE_CAMERA_TYPE)
			fpsgo_com_check_bypass_closed_loop(f_render);

		fpsgo_check_jank_detection_info_status();

		fpsgo_com_get_l2q_time(pid, f_render->buffer_id, f_render->tgid,
			enqueue_end_time, prev_enqueue_end, pprev_enqueue_end,
			f_render->t_dequeue_start, sf_buf_id, f_render);

		fpsgo_comp2fbt_frame_start(f_render,
				enqueue_end_time);

		//set vip if ux scrolling
		if (get_ux_general_policy() && f_render->sbe_control_flag)
			fpsgo_boost_non_hwui_policy(f_render, 1);

		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			f_render->frame_type,
			enqueue_end_time,
			f_render->api,
			f_render->hwui);
		fpsgo_comp2fstb_notify_info(pid, f_render->buffer_id,
			f_render->Q2Q_time, f_render->enqueue_length, f_render->dequeue_length);
		fpsgo_comp2minitop_queue_update(enqueue_end_time);

		f_render->frame_count++;

		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->enqueue_length,
			"%d_%d-enqueue_length", pid, f_render->frame_type);
		break;
	case BY_PASS_TYPE:
		pprev_enqueue_end = f_render->prev_t_enqueue_end;
		prev_enqueue_end = f_render->t_enqueue_end;
		if (f_render->t_enqueue_end)
			f_render->Q2Q_time = enqueue_end_time - f_render->t_enqueue_end;
		f_render->prev_t_enqueue_end = f_render->t_enqueue_end;
		f_render->t_enqueue_end = enqueue_end_time;
		f_render->enqueue_length = enqueue_end_time - f_render->t_enqueue_start;
		f_render->enqueue_length_real = f_render->enqueue_length;

		fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
			&raw_runtime, &running_time, &enq_running_time,
			0, f_render->t_enqueue_end,
			f_render->t_dequeue_start, f_render->t_dequeue_end,
			f_render->t_enqueue_start, f_render->t_enqueue_end, 1);
		fpsgo_stop_boost_by_render(f_render);
		fbt_set_render_last_cb(f_render, enqueue_end_time);
		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			f_render->frame_type,
			enqueue_end_time,
			f_render->api,
			f_render->hwui);
		break;
	case MFRC_BY_PASS_FRAME:
		f_render->frame_count++;

		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->enqueue_length,
			"%d_%d-enqueue_length", pid, f_render->frame_type);
		break;
	default:
		break;
	}

	// legacy version, phase out in future
	fpsgo_fstb2other_info_update(f_render->pid, f_render->buffer_id,
		FPSGO_PERF_IDX, 0, 0, f_render->boost_info.last_blc,
		f_render->sbe_control_flag);

	cb_mask = 1 << GET_FPSGO_Q2Q_TIME | 1 << GET_FPSGO_PERF_IDX |
				1 << GET_SBE_CTRL;
	fpsgo_notify_frame_info_callback(cb_mask, f_render);

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_dequeue_start(int pid,
	unsigned long long dequeue_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int ret;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		struct BQ_id *pair;

		pair = fpsgo_find_BQ_id(pid, 0, identifier, ACTION_FIND);
		if (pair) {
			pid = pair->queue_pid;
			f_render = fpsgo_search_and_add_render_info(pid,
				identifier, 0);
			if (!f_render) {
				fpsgo_render_tree_unlock(__func__);
				return;
			}
		} else {
			fpsgo_render_tree_unlock(__func__);
			return;
		}
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret)
		goto exit;

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	if (f_render->frame_type == MFRC_FRAME) {
		if (f_render->frame_count % mfrc_by_pass_frame_num == 1)
			f_render->frame_type = MFRC_BY_PASS_FRAME;
		else
			f_render->t_dequeue_start = dequeue_start_time;
	} else {
		f_render->t_dequeue_start = dequeue_start_time;
	}


exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

}

void fpsgo_ctrl2comp_dequeue_end(int pid,
	unsigned long long dequeue_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int ret;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		struct BQ_id *pair;

		pair = fpsgo_find_BQ_id(pid, 0, identifier, ACTION_FIND);
		if (pair) {
			pid = pair->queue_pid;
			f_render = fpsgo_search_and_add_render_info(pid,
				identifier, 0);
		}

		if (!f_render) {
			fpsgo_render_tree_unlock(__func__);
			return;
		}
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret)
		goto exit;

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	if (f_render->frame_type == MFRC_FRAME) {
		if (f_render->frame_count % mfrc_by_pass_frame_num == 1)
			f_render->frame_type = MFRC_BY_PASS_FRAME;
	}

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
	case MFRC_FRAME:
		f_render->t_dequeue_end = dequeue_end_time;
		f_render->dequeue_length = dequeue_end_time - f_render->t_dequeue_start;
		fpsgo_comp2fbt_deq_end(f_render, dequeue_end_time);
		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->dequeue_length,
			"%d_%d-dequeue_length", pid, f_render->frame_type);
		break;
	case BY_PASS_TYPE:
		f_render->t_dequeue_end = dequeue_end_time;
		f_render->dequeue_length = dequeue_end_time - f_render->t_dequeue_start;
		break;
	case MFRC_BY_PASS_FRAME:
		break;
	default:
		break;
	}

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_frame_start(struct render_info *f_render,
	unsigned long long frameID,
	unsigned long long frame_start_time,
	unsigned long long bufID)
{
	fpsgo_systrace_c_fbt(f_render->pid, bufID, 1, "[ux]sbe_set_ctrl");
	fbt_ux_frame_start(f_render, frameID, frame_start_time);
}

void fpsgo_frame_end(struct render_info *f_render,
	unsigned long long frame_start_time,
	unsigned long long frame_end_time,
	unsigned long long frameid,
	unsigned long long bufID)
{
	unsigned long long running_time = 0, raw_runtime = 0;
	unsigned long long enq_running_time = 0;

	if (f_render->t_enqueue_end)
		f_render->Q2Q_time =
			frame_end_time - f_render->t_enqueue_end;
	f_render->t_enqueue_end = frame_end_time;
	f_render->enqueue_length =
		frame_end_time - f_render->t_enqueue_start;

	fpsgo_comp2fstb_queue_time_update(f_render->pid,
		f_render->buffer_id,
		f_render->frame_type,
		frame_end_time,
		f_render->api,
		f_render->hwui);
	switch_thread_max_fps(f_render->pid, 1);

	fpsgo_comp2xgf_qudeq_notify(f_render->pid, f_render->buffer_id,
		&raw_runtime, &running_time, &enq_running_time,
		frame_start_time, frame_end_time,
		0, 0, 0, 0, 0);
	f_render->raw_runtime = raw_runtime;
	if (running_time != 0)
		f_render->running_time = running_time;
	fbt_ux_frame_end(f_render, frameid, frame_start_time, frame_end_time);
	fpsgo_comp2fstb_notify_info(f_render->pid, f_render->buffer_id,
		f_render->Q2Q_time, 0, 0);
	fpsgo_systrace_c_fbt(f_render->pid, bufID, 0, "[ux]sbe_set_ctrl");
}

void fpsgo_ctrl2comp_hint_frame_start(int pid,
	unsigned long long frameID,
	unsigned long long frame_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct ux_frame_info *frame_info;
	int ux_frame_cnt = 0;

	fpsgo_render_tree_lock(__func__);

	// prepare render info
	f_render = fpsgo_search_and_add_render_info(pid, identifier, 1);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	// fill the frame info.
	f_render->frame_type = FRAME_HINT_TYPE;
	f_render->buffer_id = identifier;	//FPSGO UX: using a magic number.
	f_render->t_enqueue_start = frame_start_time; // for recycle only.
	f_render->t_last_start = frame_start_time;

	if (!f_render->p_blc)
		fpsgo_base2fbt_node_init(f_render);

	mutex_lock(&f_render->ux_mlock);
	frame_info = fpsgo_ux_search_and_add_frame_info(f_render, frameID, frame_start_time, 1);
	if (!frame_info) {
		fpsgo_systrace_c_fbt(pid, identifier, frameID, "[ux]start_malloc_fail");
		fpsgo_systrace_c_fbt(pid, identifier, 0, "[ux]start_malloc_fail");
	}
	ux_frame_cnt = fpsgo_ux_count_frame_info(f_render, 2);
	fpsgo_systrace_c_fbt(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");
	mutex_unlock(&f_render->ux_mlock);

	// if not overlap, call frame start.
	if (ux_frame_cnt == 1)
		fpsgo_frame_start(f_render, frameID, frame_start_time, identifier);

	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

	fpsgo_com_notify_fpsgo_is_boost(1);

	mutex_lock(&recycle_lock);
	if (recycle_idle_cnt) {
		recycle_idle_cnt = 0;
		if (!recycle_active) {
			recycle_active = 1;
			hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&recycle_lock);
}

void fpsgo_ctrl2comp_hint_doframe_end(int pid,
	unsigned long long frameID,
	unsigned long long frame_end_time,
	unsigned long long identifier, long long frame_flags)
{
	struct render_info *f_render;

	fpsgo_render_tree_lock(__func__);
	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_ux_doframe_end(f_render, frameID, frame_flags);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_hint_buffer_count(int pid, int count, int max_count)
{
	struct render_info *f_render;

	fpsgo_render_tree_lock(__func__);
	//find HWUI render infos, 5566 HWUI magic num
	f_render = fpsgo_search_and_add_render_info(pid, 5566, 0);
	if (!f_render || count < 0) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	if (max_count < count)
		max_count = count;

	f_render->cur_buffer_count = count;
	f_render->max_buffer_count = max_count;
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_hint_frame_end(int pid,
	unsigned long long frameID,
	unsigned long long frame_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct ux_frame_info *frame_info;
	unsigned long long frame_start_time = 0;
	int ux_frame_cnt = 0;
	unsigned long cb_mask = 0;

	fpsgo_render_tree_lock(__func__);

	// prepare frame info.
	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	// fill the frame info.
	f_render->frame_type = FRAME_HINT_TYPE;
	f_render->t_enqueue_start = frame_end_time; // for recycle only.

	mutex_lock(&f_render->ux_mlock);
	frame_info = fpsgo_ux_search_and_add_frame_info(f_render, frameID, frame_start_time, 0);
	if (!frame_info) {
		fpsgo_systrace_c_fbt(pid, identifier, frameID, "[ux]start_not_found");
		fpsgo_systrace_c_fbt(pid, identifier, 0, "[ux]start_not_found");
	} else {
		frame_start_time = frame_info->start_ts;
		fpsgo_ux_delete_frame_info(f_render, frame_info);
	}

	ux_frame_cnt = fpsgo_ux_count_frame_info(f_render, 1);
	mutex_unlock(&f_render->ux_mlock);

	// frame end.
	fpsgo_frame_end(f_render, frame_start_time, frame_end_time, frameID, identifier);
	if (ux_frame_cnt == 1)
		fpsgo_frame_start(f_render, frameID, frame_end_time, identifier);
	fpsgo_systrace_c_fbt(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");

	// legacy version, phase out in future
	fpsgo_fstb2other_info_update(f_render->pid, f_render->buffer_id, FPSGO_PERF_IDX,
		0, 0, f_render->boost_info.last_blc, 1);

	cb_mask = 1 << GET_FPSGO_PERF_IDX | 1 << GET_SBE_CTRL;
	fpsgo_notify_frame_info_callback(cb_mask, f_render);

	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_hint_frame_err(int pid,
	unsigned long long frameID,
	unsigned long long time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct ux_frame_info *frame_info;
	int ux_frame_cnt = 0;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	f_render->t_enqueue_start = time; // for recycle only.

	fpsgo_thread_lock(&f_render->thr_mlock);

	mutex_lock(&f_render->ux_mlock);
	frame_info = fpsgo_ux_search_and_add_frame_info(f_render, frameID, time, 0);
	if (!frame_info) {
		fpsgo_systrace_c_fbt(pid, identifier, frameID, "[ux]start_not_found");
		fpsgo_systrace_c_fbt(pid, identifier, 0, "[ux]start_not_found");
	} else
		fpsgo_ux_delete_frame_info(f_render, frame_info);
	ux_frame_cnt = fpsgo_ux_count_frame_info(f_render, 1);
	if (ux_frame_cnt == 0) {
		fpsgo_systrace_c_fbt(f_render->pid, identifier, 0, "[ux]sbe_set_ctrl");
	}
	fbt_ux_frame_err(f_render, ux_frame_cnt, frameID, time);
	fpsgo_systrace_c_fbt(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");
	mutex_unlock(&f_render->ux_mlock);

	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_hint_frame_dep_task(int rtid, unsigned long long identifier,
	int dep_mode, char *dep_name, int dep_num)
{
	int local_action = 0;
	int local_tgid = 0;
	int local_rtid[1];
	unsigned long long local_bufID[1];
	unsigned long long cur_ts;

	cur_ts = fpsgo_get_time();
	if (cur_ts <= last_update_sbe_dep_ts ||
		cur_ts - last_update_sbe_dep_ts < 500 * NSEC_PER_MSEC)
		return;
	last_update_sbe_dep_ts = cur_ts;
	fpsgo_main_trace("[comp] last_update_sbe_dep_ts:%llu", last_update_sbe_dep_ts);

	local_tgid = fpsgo_get_tgid(rtid);
	local_rtid[0] = rtid;
	local_bufID[0] = identifier;

	switch (dep_mode) {
	case 1:
		local_action = XGF_DEL_DEP;
		break;
	case 2:
		local_action = XGF_ADD_DEP_NO_LLF;
		break;
	default:
		return;
	}

	fpsgo_other2xgf_set_dep_list(local_tgid, local_rtid, local_bufID, 1,
		dep_name, dep_num, local_action);
}

void fpsgo_ctrl2comp_bqid(int pid, unsigned long long buffer_id,
	int queue_SF, unsigned long long identifier, int create)
{
	struct BQ_id *pair;

	if (!identifier || !pid)
		return;

	if (!buffer_id && create)
		return;

	fpsgo_render_tree_lock(__func__);

	if (create) {
		pair = fpsgo_find_BQ_id(pid, 0,
				identifier, ACTION_FIND_ADD);

		if (!pair) {
			fpsgo_render_tree_unlock(__func__);
			return;
		}

		if (pair->pid != pid)
			FPSGO_LOGI("%d: diff render same key %d\n",
				pid, pair->pid);

		pair->buffer_id = buffer_id;
		pair->queue_SF = queue_SF;
	} else
		fpsgo_find_BQ_id(pid, 0,
			identifier, ACTION_FIND_DEL);

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_connect_api(int pid, int api,
		unsigned long long identifier)
{
	struct connect_api_info *connect_api = NULL;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;
	int ret;

	fpsgo_render_tree_lock(__func__);

	ret = fpsgo_get_BQid_pair(pid, 0, identifier, &buffer_id, &queue_SF, 0);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("connect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_main_trace("COMP: connect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	connect_api =
		fpsgo_search_and_add_connect_api_info(pid, buffer_id, 1);
	if (!connect_api) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	connect_api->api = api;

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_disconnect_api(
	int pid, int api, unsigned long long identifier)
{
	unsigned long long buffer_id = 0;
	int queue_SF = 0;
	int ret;

	fpsgo_render_tree_lock(__func__);

	ret = fpsgo_get_BQid_pair(pid, 0, identifier, &buffer_id, &queue_SF, 0);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("[Disconnect] NoBQid %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_main_trace("COMP: disconnect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_delete_connect_api_info(pid, buffer_id);

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts)
{
	struct acquire_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);

	if (api == WINDOW_DISCONNECT)
		fpsgo_delete_acquire_info(0, c_tid, buffer_id);
	else {
		iter = fpsgo_add_acquire_info(p_pid, c_pid, c_tid,
			api, buffer_id, ts);
		if (iter)
			iter->ts = ts;
	}

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_set_app_meta_fps(int tgid, int fps, unsigned long long ts)
{
	struct fpsgo_com_policy_cmd *iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(tgid, ts, 1);
	if (!iter)
		goto out;

	if (fps > 0) {
		iter->app_cam_meta_min_fps = fps;
		iter->ts = ts;
	} else {
		iter->app_cam_meta_min_fps = BY_PID_DEFAULT_VAL;
		fpsgo_com_delete_policy_cmd(iter);
	}

out:
	mutex_unlock(&fpsgo_com_policy_cmd_lock);
}

int fpsgo_ctrl2comp_set_sbe_policy(int tgid, char *name, unsigned long mask,
				unsigned long long ts, int start,
				char *specific_name, int num)
{
	char *thread_name = NULL;
	int ret;
	int i;
	int *final_pid_arr =  NULL;
	unsigned long long *final_bufID_arr = NULL;
	unsigned long long *final_idf_arr = NULL;
	int final_pid_arr_idx = 0;
	int *local_specific_tid_arr = NULL;
	int local_specific_tid_num = 0;
	struct fpsgo_attr_by_pid *attr_iter = NULL;
	struct render_info *thr = NULL;
	struct sbe_info *sbe_info = NULL;

	if (tgid <= 0 || !name || !mask) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENOMEM;
	thread_name = kcalloc(16, sizeof(char), GFP_KERNEL);
	if (!thread_name)
		goto out;

	if (!strncpy(thread_name, name, 16))
		goto out;
	thread_name[15] = '\0';

	final_pid_arr = kcalloc(10, sizeof(int), GFP_KERNEL);
	if (!final_pid_arr)
		goto out;

	final_bufID_arr = kcalloc(10, sizeof(unsigned long long), GFP_KERNEL);
	if (!final_bufID_arr)
		goto out;

	local_specific_tid_arr = kcalloc(num, sizeof(int), GFP_KERNEL);
	if (!local_specific_tid_arr)
		goto out;
	ret = 0;

	final_idf_arr = kcalloc(10, sizeof(unsigned long long), GFP_KERNEL);
	if (!final_idf_arr) {
		ret = -ENOMEM;
		goto out;
	}

	fpsgo_get_render_tid_by_render_name(tgid, thread_name,
		final_pid_arr, final_bufID_arr, final_idf_arr, &final_pid_arr_idx, 10);

	ux_general_policy = get_ux_general_policy();
	fpsgo_main_trace("[comp] sbe tgid:%d name:%s mask:%lu start:%d final_pid_arr_idx:%d ux_general_policy:%d",
				tgid, name, mask, start, final_pid_arr_idx, ux_general_policy);

	if (ux_general_policy) {
		fpsgo_render_tree_lock(__func__);
		sbe_info = fpsgo_search_and_add_sbe_info(tgid, 1);

		if (sbe_info) {
			if (test_bit(FPSGO_HWUI, &mask) || test_bit(FPSGO_NON_HWUI, &mask)) {
				if (start && !sbe_info->ux_scrolling) {
					sbe_info->ux_scrolling = start;
					if (!ux_scroll_count) {
						fpsgo_set_ux_general_policy(start, mask);
						fpsgo_systrace_c_fbt(tgid, 0, start, "ux_policy");
					} else {
						//If multi window case, need update ux general policy.
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
						update_ux_general_policy();
#endif
					}
					ux_scroll_count++;
				}

				if (!start && sbe_info->ux_scrolling) {
					sbe_info->ux_scrolling = start;
					ux_scroll_count--;
					if (!ux_scroll_count) {
						fpsgo_set_ux_general_policy(start, mask);
						fpsgo_systrace_c_fbt(tgid, 0, start, "ux_policy");
					}
				}
			}
		}
		fpsgo_render_tree_unlock(__func__);
	}

	for (i = 0; i < final_pid_arr_idx; i++) {
		if (test_bit(FPSGO_CONTROL, &mask))
			switch_ui_ctrl(final_pid_arr[i], start);
		if (test_bit(FPSGO_MAX_TARGET_FPS, &mask))
			switch_thread_max_fps(final_pid_arr[i], start);

		fpsgo_render_tree_lock(__func__);
		thr = fpsgo_search_and_add_render_info(final_pid_arr[i], final_bufID_arr[i], 0);

		if (test_bit(FPSGO_CLEAR_SCROLLING_INFO, &mask) && thr != NULL) {
			clear_ux_info(thr);
			fpsgo_render_tree_unlock(__func__);
			goto out;
		}
		if (start) {
			attr_iter = fpsgo_find_attr_by_tid(final_pid_arr[i], 1);
			if (attr_iter) {
				if (test_bit(FPSGO_RESCUE_ENABLE, &mask))
					attr_iter->attr.rescue_enable_by_pid = 1;
				if (test_bit(FPSGO_RL_ENABLE, &mask))
					attr_iter->attr.gcc_enable_by_pid = 2;
				if (test_bit(FPSGO_GCC_DISABLE, &mask))
					attr_iter->attr.gcc_enable_by_pid = 0;
				if (test_bit(FPSGO_QUOTA_DISABLE, &mask))
					attr_iter->attr.qr_enable_by_pid = 0;
			}

			//get render_info struct for this tid and buffer_id
			if (test_bit(FPSGO_HWUI, &mask) && thr != NULL) {
				int add_new_scrolling = 1;
				int type = test_bit(FPSGO_MOVEING, &mask) ?
						FPSGO_MOVEING : (test_bit(FPSGO_FLING, &mask) ? FPSGO_FLING : 0);
				if (get_ux_list_length(&thr->scroll_list) >= 1) {
					struct ux_scroll_info *last =
						list_first_entry(
							&thr->scroll_list,
							struct ux_scroll_info,
							queue_list);
					if (last->type == FPSGO_MOVEING && type == FPSGO_FLING)
						add_new_scrolling = 0;

					if (add_new_scrolling && !last->end_ts) {
						last->end_ts = ts; // last scroll endtime is current ts
						fpsgo_ux_scrolling_end(thr);
					} else if (!add_new_scrolling) {
						last->type = FPSGO_FLING;
					}
				}
				if (add_new_scrolling) {
					//add new scroll_info into render_info struct
					enqueue_ux_scroll_info(type, ts, thr);
				}
			}
		} else {
			if (ux_general_policy && thr)
				fpsgo_reset_deplist_task_priority(thr);

			//update scroll_info when scroll end
			if (test_bit(FPSGO_HWUI, &mask) && thr != NULL) {
				if (get_ux_list_length(&thr->scroll_list) > 0) {
					struct ux_scroll_info *last =
						list_first_entry(
							&thr->scroll_list,
							struct ux_scroll_info,
							queue_list);
					last->end_ts = ts;
					last->dur_ts = last->end_ts - last->start_ts;
					fpsgo_ux_scrolling_end(thr);
				}
			}

			delete_attr_by_pid(final_pid_arr[i]);
		}
		fpsgo_render_tree_unlock(__func__);

		fpsgo_main_trace("[comp] sbe %dth rtid:%d buffer_id:0x%llx",
			i+1, final_pid_arr[i], final_bufID_arr[i]);
	}

	if (test_bit(FPSGO_RUNNING_CHECK, &mask)) {
		if (start) {
			local_specific_tid_num = xgf_split_dep_name(tgid,
				specific_name, num, local_specific_tid_arr);
			fpsgo_update_sbe_spid_loading(local_specific_tid_arr,
				local_specific_tid_num, tgid);
		} else
			fpsgo_delete_sbe_spid_loading(tgid);
	} else if (final_pid_arr_idx > 0 && start)
		fpsgo_other2xgf_set_dep_list(tgid, final_pid_arr,
			final_bufID_arr, final_pid_arr_idx,
			specific_name, num, XGF_ADD_DEP);

	ret = final_pid_arr_idx;

out:
	kfree(thread_name);
	kfree(final_pid_arr);
	kfree(final_bufID_arr);
	kfree(final_idf_arr);
	kfree(local_specific_tid_arr);
	return ret;
}

static void fpsgo_user_boost(int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts, int skip)
{
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter)
		goto out;

	fpsgo_thread_lock(&iter->thr_mlock);
	if (!iter->p_blc)
		fpsgo_base2fbt_node_init(iter);
	iter->enqueue_length = 0;
	iter->enqueue_length_real = 0;
	iter->dequeue_length = 0;
	if (iter->t_enqueue_end && !skip) {
		iter->raw_runtime = tcpu;
		iter->running_time = tcpu;
		iter->Q2Q_time = ts - iter->t_enqueue_end;
		fpsgo_comp2fbt_frame_start(iter, ts);
	}
	iter->t_enqueue_end = ts;
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);
	fpsgo_com_notify_fpsgo_is_boost(1);
}

static void fpsgo_user_deboost(int render_tid, unsigned long long buffer_id)
{
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter)
		goto out;
	fpsgo_thread_lock(&iter->thr_mlock);
	fpsgo_stop_boost_by_render(iter);
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_ctrl2comp_set_target_time(int tgid, int render_tid,
	unsigned long long buffer_id, unsigned long long target_time)
{
	int ret = 0;

	if (!target_time)
		return -EINVAL;

	mutex_lock(&user_hint_lock);

	ret = fpsgo_other2fstb_set_target_time(tgid, render_tid, buffer_id, target_time, 0);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, (int)target_time, "user_set_target_time");

	mutex_unlock(&user_hint_lock);

	return ret;
}

int fpsgo_ctrl2comp_set_dep_list(int tgid, int render_tid,
	unsigned long long buffer_id, int *dep_arr, int dep_num)
{
	int ret = 0;

	if (!dep_arr)
		return -EFAULT;

	mutex_lock(&user_hint_lock);

	ret = fpsgo_other2xgf_set_critical_tasks(tgid, render_tid, buffer_id, dep_arr, dep_num, 0);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, dep_num, "user_set_critical_threads");

	mutex_unlock(&user_hint_lock);

	return ret;
}

void fpsgo_ctrl2comp_control_resume(int render_tid, unsigned long long buffer_id)
{
	unsigned long long ts = fpsgo_get_time();

	mutex_lock(&user_hint_lock);
	fpsgo_user_boost(render_tid, buffer_id, 0, ts, 1);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "user_control_resume");
	mutex_unlock(&user_hint_lock);
}

void fpsgo_ctrl2comp_control_pause(int render_tid, unsigned long long buffer_id)
{
	mutex_lock(&user_hint_lock);
	fpsgo_user_deboost(render_tid, buffer_id);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "user_control_pause");
	mutex_unlock(&user_hint_lock);
}

void fpsgo_ctrl2comp_user_close(int tgid, int render_tid, unsigned long long buffer_id)
{
	mutex_lock(&user_hint_lock);

	fpsgo_other2xgf_set_critical_tasks(tgid, render_tid, buffer_id, NULL, 0, -1);
	fpsgo_other2fstb_set_target_time(tgid, render_tid, buffer_id, 0, -1);

	fpsgo_user_deboost(render_tid, buffer_id);

	fpsgo_render_tree_lock(__func__);
	fpsgo_delete_render_info(render_tid, buffer_id, buffer_id);
	delete_attr_by_tid(render_tid);
	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "user_close");

	mutex_unlock(&user_hint_lock);
}

int fpsgo_ctrl2comp_user_create(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time)
{
	int ret = 0;
	unsigned long local_master_type = 0;
	struct render_info *iter = NULL;
	struct fpsgo_attr_by_pid *attr_iter = NULL;

	if (!dep_arr)
		return -EFAULT;
	if (!target_time)
		return -EINVAL;

	mutex_lock(&user_hint_lock);

	ret = fpsgo_other2xgf_set_critical_tasks(tgid, render_tid, buffer_id, dep_arr, dep_num, 1);
	if (ret)
		goto out;

	ret = fpsgo_other2fstb_set_target_time(tgid, render_tid, buffer_id, target_time, 1);
	if (ret) {
		fpsgo_other2xgf_set_critical_tasks(tgid, render_tid, buffer_id, NULL, 0, -1);
		goto out;
	}

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 1);
	if (iter) {
		fpsgo_thread_lock(&iter->thr_mlock);
		iter->tgid = tgid;
		iter->buffer_id = buffer_id;
		iter->api = NATIVE_WINDOW_API_EGL;
		iter->frame_type = NON_VSYNC_ALIGNED_TYPE;
		set_bit(USER_TYPE, &local_master_type);
		iter->master_type = local_master_type;
		fpsgo_thread_unlock(&iter->thr_mlock);
	} else {
		fpsgo_other2xgf_set_critical_tasks(tgid, render_tid, buffer_id, NULL, 0, -1);
		fpsgo_other2fstb_set_target_time(tgid, render_tid, buffer_id, 0, -1);
	}

	attr_iter = fpsgo_find_attr_by_tid(render_tid, 1);
	if (attr_iter) {
		attr_iter->attr.rescue_enable_by_pid = 1;
	} else {
		fpsgo_delete_render_info(render_tid, buffer_id, buffer_id);
		fpsgo_other2xgf_set_critical_tasks(tgid, render_tid, buffer_id, NULL, 0, -1);
		fpsgo_other2fstb_set_target_time(tgid, render_tid, buffer_id, 0, -1);
	}
	fpsgo_render_tree_unlock(__func__);

out:
	fpsgo_systrace_c_fbt(render_tid, buffer_id, ret, "user_create");
	mutex_unlock(&user_hint_lock);
	return ret;
}

int fpsgo_ctrl2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long *tcpu_arr, unsigned long long *ts_arr, int num)
{
	int i;
	int ret = 0;
	unsigned long long local_tcpu = 0;
	unsigned long long local_ts = 0;

	mutex_lock(&user_hint_lock);

	if (num > 1) {
		for (i = 0; i < num; i++) {
			if (local_ts < ts_arr[i])
				local_ts = ts_arr[i];
			local_tcpu += tcpu_arr[i];
			xgf_trace("[user][xgf][%d][0x%llx] | %dth t_cpu:%llu ts:%llu",
				render_tid, buffer_id, i, tcpu_arr[i], ts_arr[i]);
		}
	} else if (num == 1) {
		local_tcpu = tcpu_arr[0];
		local_ts = ts_arr[0];
	} else {
		ret = -EINVAL;
		goto out;
	}
	xgf_trace("[user][xgf][%d][0x%llx] | local_tcpu:%llu local_ts:%llu",
		render_tid, buffer_id, local_tcpu, local_ts);

	local_ts = fpsgo_get_time();
	fpsgo_user_boost(render_tid, buffer_id, local_tcpu, local_ts, 0);

out:
	fpsgo_systrace_c_fbt(render_tid, buffer_id, local_tcpu, "user_report_workload");
	mutex_unlock(&user_hint_lock);
	return ret;
}

int switch_ui_ctrl(int pid, int set_ctrl)
{
	fpsgo_render_tree_lock(__func__);
	if (set_ctrl)
		fpsgo_search_and_add_sbe_info(pid, 1);
	else {
		fpsgo_delete_sbe_info(pid);

		fpsgo_stop_boost_by_pid(pid);
	}
	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(pid, 0,
			set_ctrl, "sbe_set_ctrl");
	fpsgo_systrace_c_fbt(pid, 0,
			0, "sbe_state");

	return 0;
}

static int switch_fpsgo_control_pid(int pid, int set_ctrl)
{
	fpsgo_render_tree_lock(__func__);
	if (set_ctrl)
		fpsgo_search_and_add_fps_control_pid(pid, 1);
	else
		fpsgo_delete_fpsgo_control_pid(pid);

	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(pid, 0,
			set_ctrl, "fpsgo_control_pid");

	return 0;
}

static void fpsgo_com_notify_to_do_recycle(struct work_struct *work)
{
	int ret1, ret2, ret3;

	ret1 = fpsgo_check_thread_status();
	ret2 = fpsgo_comp2fstb_do_recycle();
	ret3 = fpsgo_comp2xgf_do_recycle();

	mutex_lock(&recycle_lock);

	if (ret1 && ret2 && ret3) {
		recycle_idle_cnt++;
		if (recycle_idle_cnt >= FPSGO_MAX_RECYCLE_IDLE_CNT) {
			recycle_active = 0;
			goto out;
		}
	}

	hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

out:
	mutex_unlock(&recycle_lock);
}

int fpsgo_com_get_mfrc_is_on(void)
{
	return mfrc_active;
}

int notify_fpsgo_touch_latency_ko_ready(void)
{
	fpsgo_touch_latency_ko_ready = 1;
	return 0;
}
EXPORT_SYMBOL(notify_fpsgo_touch_latency_ko_ready);

int fpsgo_ktf2comp_test_check_BQ_type(int *a_p_pid_arr,  int *a_c_pid_arr,
	int *a_c_tid_arr, int *a_api_arr, unsigned long long *a_bufID_arr, int a_num,
	int *r_tgid_arr, int *r_pid_arr, unsigned long long *r_bufID_arr, int *r_api_arr,
	int *r_frame_type_arr, int *r_hwui_arr, int *r_tfps_arr, int *r_qfps_arr,
	int *final_bq_type_arr, int r_num)
{
	int i;
	int ret = 0;
	int r_ret = 0;
	int local_correct_cnt = 0;
	int local_bq_type = ACQUIRE_UNKNOWN_TYPE;
	struct render_info *r_iter = NULL;
	struct acquire_info *a_iter = NULL;

	fpsgo_render_tree_lock(__func__);

	for (i = 0; i < a_num; i++) {
		a_iter = fpsgo_add_acquire_info(a_p_pid_arr[i], a_c_pid_arr[i],
			a_c_tid_arr[i], a_api_arr[i], a_bufID_arr[i], 0);
		if (!a_iter) {
			ret = -ENOMEM;
			break;
		}
	}

	if (ret)
		goto out;

#if IS_ENABLED(CONFIG_ARM64)
	FPSGO_LOGE("struct render_info size:%lu\n", sizeof(struct render_info));
#else
	FPSGO_LOGE("struct render_info size:%u\n", sizeof(struct render_info));
#endif

	for (i = 0; i < r_num; i++) {
		r_iter = fpsgo_search_and_add_render_info(r_pid_arr[i], r_bufID_arr[i], 1);
		if (r_iter) {
			r_iter->tgid = r_tgid_arr[i];
			r_iter->pid = r_pid_arr[i];
			r_iter->buffer_id = r_bufID_arr[i];
			r_iter->api = r_api_arr[i];
			r_iter->frame_type = r_frame_type_arr[i];
			r_iter->hwui = r_hwui_arr[i];
		} else {
			ret = -ENOMEM;
			break;
		}

		r_ret = fpsgo_ktf2fstb_add_delete_render_info(0, r_pid_arr[i], r_bufID_arr[i],
					r_tfps_arr[i], r_qfps_arr[i]);
		if (!r_ret) {
			ret = -ENOMEM;
			break;
		}

		r_ret = fpsgo_ktf2xgf_add_delete_render_info(0, r_pid_arr[i], r_bufID_arr[i]);
		if (!r_ret) {
			ret = -ENOMEM;
			break;
		}
	}

	if (ret)
		goto out;

	for (i = 0; i < r_num; i++) {
		local_bq_type = ACQUIRE_UNKNOWN_TYPE;

		if (r_frame_type_arr[i] == NON_VSYNC_ALIGNED_TYPE)
			fpsgo_com_check_BQ_type(&local_bq_type, r_pid_arr[i], r_bufID_arr[i]);

		if (final_bq_type_arr[i] == local_bq_type)
			local_correct_cnt++;

		FPSGO_LOGE("%dth expected_bq_type:%d local_bq_type:%d",
			i+1, final_bq_type_arr[i], local_bq_type);
	}

	ret = (local_correct_cnt == r_num) ? 1 : 0;

out:
	for (i = 0; i < a_num; i++)
		fpsgo_delete_acquire_info(0, a_c_tid_arr[i], a_bufID_arr[i]);
	for (i = 0; i < r_num; i++) {
		fpsgo_delete_render_info(r_pid_arr[i], r_bufID_arr[i], r_bufID_arr[i]);
		fpsgo_ktf2fstb_add_delete_render_info(1, r_pid_arr[i], r_bufID_arr[i], 0, 0);
		fpsgo_ktf2xgf_add_delete_render_info(1, r_pid_arr[i], r_bufID_arr[i]);
	}

	fpsgo_render_tree_unlock(__func__);

	return ret;
}
EXPORT_SYMBOL(fpsgo_ktf2comp_test_check_BQ_type);

int fpsgo_ktf2comp_test_queue_dequeue(int pid, unsigned long long bufID, int frame_num,
	unsigned long enq_length_us, unsigned long deq_length_us, unsigned long q2q_us)
{
	int i;
	unsigned long long ts;
	struct fpsgo_attr_by_pid *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_find_attr_by_pid(pid, 0);
	if (iter) {
		iter->attr.rescue_enable_by_pid = 1;
		iter->attr.rescue_second_enable_by_pid = 1;
		iter->attr.filter_frame_enable_by_pid = 1;
		iter->attr.separate_aa_by_pid = 1;
		iter->attr.boost_affinity_by_pid = 2;
		iter->attr.gcc_enable_by_pid = 2;
	}
	fpsgo_render_tree_unlock(__func__);

	for (i = 0; i < frame_num; i++) {
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_enqueue_start(pid, ts, bufID);
		usleep_range(enq_length_us, enq_length_us * 2);
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_enqueue_end(pid, ts, bufID, 0);
		usleep_range(1000, 2000);
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_dequeue_start(pid, ts, bufID);
		usleep_range(deq_length_us, deq_length_us * 2);
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_dequeue_end(pid, ts, bufID);
		usleep_range(q2q_us, q2q_us * 2);
	}

	return 0;
}
EXPORT_SYMBOL(fpsgo_ktf2comp_test_queue_dequeue);

#define FPSGO_COM_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FPSGO_COM_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					(variable) = arg; \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FPSGO_COM_SYSFS_WRITE_POLICY_CMD(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int tgid; \
	int arg; \
	unsigned long long ts = fpsgo_get_time(); \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				mutex_lock(&fpsgo_com_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fpsgo_com_set_policy_cmd(cmd, arg, tgid, ts, 1); \
				else \
					fpsgo_com_set_policy_cmd(cmd, BY_PID_DEFAULT_VAL, \
						tgid, ts, 0); \
				mutex_unlock(&fpsgo_com_policy_cmd_lock); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

FPSGO_COM_SYSFS_READ(fpsgo_control, 1, fpsgo_control);
FPSGO_COM_SYSFS_WRITE_VALUE(fpsgo_control, fpsgo_control, 0, 1);
static KOBJ_ATTR_RW(fpsgo_control);

FPSGO_COM_SYSFS_READ(control_hwui, 1, control_hwui);
FPSGO_COM_SYSFS_WRITE_VALUE(control_hwui, control_hwui, 0, 1);
static KOBJ_ATTR_RW(control_hwui);

FPSGO_COM_SYSFS_READ(control_api_mask, 1, control_api_mask);
FPSGO_COM_SYSFS_WRITE_VALUE(control_api_mask, control_api_mask, 0, 32);
static KOBJ_ATTR_RW(control_api_mask);

FPSGO_COM_SYSFS_READ(bypass_non_SF, 1, bypass_non_SF);
FPSGO_COM_SYSFS_WRITE_VALUE(bypass_non_SF, bypass_non_SF, 0, 1);
static KOBJ_ATTR_RW(bypass_non_SF);

FPSGO_COM_SYSFS_READ(fps_align_margin, 1, fps_align_margin);
FPSGO_COM_SYSFS_WRITE_VALUE(fps_align_margin, fps_align_margin, 0, 10);
static KOBJ_ATTR_RW(fps_align_margin);

FPSGO_COM_SYSFS_READ(mfrc_active, 1, mfrc_active);
FPSGO_COM_SYSFS_WRITE_VALUE(mfrc_active, mfrc_active, 0, 1);
static KOBJ_ATTR_RW(mfrc_active);

FPSGO_COM_SYSFS_READ(cam_bypass_window_ms, 1, cam_bypass_window_ms);
FPSGO_COM_SYSFS_WRITE_VALUE(cam_bypass_window_ms, cam_bypass_window_ms, 0, 60000);
static KOBJ_ATTR_RW(cam_bypass_window_ms);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(bypass_non_SF_by_pid, 0, 0, 1);
static KOBJ_ATTR_WO(bypass_non_SF_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(control_api_mask_by_pid, 1, 0, 31);
static KOBJ_ATTR_WO(control_api_mask_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(control_hwui_by_pid, 2, 0, 1);
static KOBJ_ATTR_WO(control_hwui_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(dep_loading_thr_by_pid, 3, 0, 100);
static KOBJ_ATTR_WO(dep_loading_thr_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(mfrc_active_by_pid, 4, 0, 1);
static KOBJ_ATTR_WO(mfrc_active_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(cam_bypass_window_ms_by_pid, 5, 0, 60000);
static KOBJ_ATTR_WO(cam_bypass_window_ms_by_pid);

static ssize_t fpsgo_com_policy_cmd_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct fpsgo_com_policy_cmd *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fpsgo_com_policy_cmd_lock);

	rbr = &fpsgo_com_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct fpsgo_com_policy_cmd, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tbypass_non_SF:%d\tcontrol_api_mask:%d\tcontrol_hwui:%d\tapp_min_fps:%d\tdep_loading_thr_by_pid:%d(%d)\tmfrc%d\tts:%llu\n",
			i,
			iter->tgid,
			iter->bypass_non_SF_by_pid,
			iter->control_api_mask_by_pid,
			iter->control_hwui_by_pid,
			iter->app_cam_meta_min_fps,
			iter->dep_loading_thr_by_pid,
			iter->cam_bypass_window_ms_by_pid,
			iter->mfrc_active_by_pid,
			iter->ts);
		pos += length;
		i++;
	}

	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fpsgo_com_policy_cmd);

static ssize_t set_ui_ctrl_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int pos = 0;
	int length = 0;
	int total = 0;
	struct sbe_info *arr = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	arr = kcalloc(FPSGO_MAX_TREE_SIZE, sizeof(struct sbe_info), GFP_KERNEL);
	if (!arr)
		goto out;

	fpsgo_render_tree_lock(__func__);
	total = fpsgo_get_all_sbe_info(arr);
	fpsgo_render_tree_unlock(__func__);

	for (i = 0; i < total; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\trender_tid:%d\n", i+1, arr[i].pid);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(arr);
	kfree(temp);
	return length;
}

static ssize_t set_ui_ctrl_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			fpsgo_systrace_c_fbt(arg > 0 ? arg : -arg,
				0, arg > 0, "force_ctrl");
			if (arg > 0)
				switch_ui_ctrl(arg, 1);
			else
				switch_ui_ctrl(-arg, 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(set_ui_ctrl);

static ssize_t fpsgo_control_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int total = 0;
	int pos = 0;
	int length = 0;
	struct fps_control_pid_info *arr = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	arr = kcalloc(FPSGO_MAX_TREE_SIZE, sizeof(struct fps_control_pid_info), GFP_KERNEL);
	if (!arr)
		goto out;

	fpsgo_render_tree_lock(__func__);

	total = fpsgo_get_all_fps_control_pid_info(arr);

	fpsgo_render_tree_unlock(__func__);

	for (i = 0; i < total; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tts:%llu\n", i+1, arr[i].pid, arr[i].ts);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(arr);
	kfree(temp);
	return length;
}

static ssize_t fpsgo_control_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int pid = 0, value = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &pid, &value) == 2)
				switch_fpsgo_control_pid(pid, !!value);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fpsgo_control_pid);

static ssize_t is_fpsgo_boosting_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fpsgo_boost_lock);
	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"fpsgo is boosting = %d\n", fpsgo_is_boosting);
	posi += length;
	mutex_unlock(&fpsgo_boost_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(is_fpsgo_boosting);

void fpsgo_ktf2comp_fuzz_test_node(char *input_data, int op, int cmd)
{
	struct kobject *kobj = NULL;
	struct kobj_attribute *attr = NULL;
	char *buf = NULL;

	kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!kobj)
		goto out;

	attr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
	if (!attr)
		goto out;

	buf = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!buf)
		goto out;

	if (input_data && op)
		scnprintf(buf, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", input_data);

	switch (cmd) {
	case BYPASS_NON_SF_GLOBAL:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, bypass_non_SF_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, bypass_non_SF_show);
		break;
	case BYPASS_NON_SF_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, bypass_non_SF_by_pid_store);
		break;
	case CONTROL_API_MASK_GLOBAL:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, control_api_mask_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, control_api_mask_show);
		break;
	case CONTROL_API_MASK_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, control_api_mask_by_pid_store);
		break;
	case CONTROL_HWUI_GLOBAL:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, control_hwui_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, control_hwui_show);
		break;
	case CONTROL_HWUI_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, control_hwui_by_pid_store);
		break;
	case SET_UI_CTRL:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, set_ui_ctrl_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, set_ui_ctrl_show);
		break;
	case FPSGO_CONTROL_GLOBAL:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fpsgo_control_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, fpsgo_control_show);
		break;
	case FPSGO_CONTROL_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fpsgo_control_pid_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, fpsgo_control_pid_show);
		break;
	case FPS_ALIGN_MARGIN:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fps_align_margin_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, fps_align_margin_show);
		break;
	case FPSGO_COM_POLICY_CMD:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, fpsgo_com_policy_cmd_show);
		break;
	case IS_FPSGO_BOOSTING:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, is_fpsgo_boosting_show);
		break;
	default:
		break;
	}

out:
	kfree(buf);
	kfree(attr);
	kfree(kobj);
}
EXPORT_SYMBOL(fpsgo_ktf2comp_fuzz_test_node);

void init_fpsgo_frame_info_cb_list(void)
{
	int i;
	struct render_frame_info_cb *iter = NULL;

	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		iter = &fpsgo_frame_info_cb_list[i];
		iter->mask = 0;
		iter->func_cb = NULL;
		memset(&iter->info_iter, 0, sizeof(struct render_frame_info));
	}
}

void __exit fpsgo_composer_exit(void)
{
	hrtimer_cancel(&recycle_hrt);
	destroy_workqueue(composer_wq);

	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_hwui);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_api_mask);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_bypass_non_SF);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_set_ui_ctrl);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fps_align_margin);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_bypass_non_SF_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_api_mask_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_hwui_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_dep_loading_thr_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_com_policy_cmd);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_is_fpsgo_boosting);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_mfrc_active);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_mfrc_active_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_cam_bypass_window_ms);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_cam_bypass_window_ms_by_pid);

	fpsgo_sysfs_remove_dir(&comp_kobj);
}

int __init fpsgo_composer_init(void)
{
	fpsgo_com_policy_cmd_tree = RB_ROOT;

	composer_wq = alloc_ordered_workqueue("composer_wq", WQ_MEM_RECLAIM | WQ_HIGHPRI);
	hrtimer_init(&recycle_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	recycle_hrt.function = &prepare_do_recycle;

	hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

	init_fpsgo_is_boosting_callback();
	init_fpsgo_frame_info_cb_list();

	if (!fpsgo_sysfs_create_dir(NULL, "composer", &comp_kobj)) {
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_hwui);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_api_mask);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_bypass_non_SF);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_set_ui_ctrl);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fps_align_margin);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_bypass_non_SF_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_api_mask_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_hwui_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_dep_loading_thr_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_com_policy_cmd);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_is_fpsgo_boosting);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_mfrc_active);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_mfrc_active_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_cam_bypass_window_ms);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_cam_bypass_window_ms_by_pid);
	}

	return 0;
}

