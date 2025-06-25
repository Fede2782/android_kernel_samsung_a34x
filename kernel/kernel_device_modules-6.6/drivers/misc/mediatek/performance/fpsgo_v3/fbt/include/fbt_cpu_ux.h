/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/rbtree.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>

#define SBE_AFFNITY_TASK 0

//high 16bit for frame status
#define FRAME_MASK 0xFFFF0000
#define FRAME_STATUS_OBTAINVIEW    (1 << 0)
#define FRAME_STATUS_INFLATE       (1 << 1)
#define FRAME_STATUS_INPUT         (1 << 2)
#define FRAME_STATUS_FLING         (1 << 3)
#define FRAME_STATUS_OTHERS        (1 << 4)
//low 16bit for rescue reason
#define RESCUE_MASK 0x0000FFFF
#define RESCUE_WAITING_ANIMATION_END      (1 << 0)
#define RESCUE_TRAVERSAL_OVER_VSYNC       (1 << 1)
#define RESCUE_RUNNING                    (1 << 2)
#define RESCUE_COUNTINUE_RESCUE           (1 << 3)
#define RESCUE_WAITING_VSYNC              (1 << 4)
#define RESCUE_TYPE_TRAVERSAL_DYNAMIC     (1 << 5)
#define RESCUE_TYPE_OI                    (1 << 6)
#define RESCUE_TYPE_MAX_ENHANCE           (1 << 7)
#define RESCUE_TYPE_SECOND_RESCUE         (1 << 8)
#define RESCUE_TYPE_BUFFER_FOUNT_FITLER   (1 << 9)
#define RESCUE_TYPE_PRE_ANIMATION         (1 << 10)
#define RESCUE_TYPE_ENABLE_MARGIN         (1 << 11)

extern void set_task_basic_vip(int pid);
extern void unset_task_basic_vip(int pid);

void fbt_ux_frame_start(struct render_info *thr, unsigned long long frameid, unsigned long long ts);
void fbt_ux_frame_end(struct render_info *thr, unsigned long long frameid,
		unsigned long long start_ts, unsigned long long end_ts);
void fbt_ux_frame_err(struct render_info *thr, int frame_count,
		unsigned long long frameID,unsigned long long ts);
void fpsgo_ux_reset(struct render_info *thr);

struct ux_frame_info {
	unsigned long long frameID;
	unsigned long long start_ts;
	struct rb_node entry;
};

struct ux_rescue_check {
	unsigned long long pid;
	unsigned long long frameID;
	int rescue_type;
	unsigned long long rsc_hint_ts;
	struct hrtimer timer;
	struct work_struct work;
};

struct ux_scroll_info {
	struct list_head queue_list;
	struct list_head frame_list;
	unsigned long long start_ts;
	unsigned long long end_ts;
	unsigned long long dur_ts;
	int frame_count;
	int jank_count;
	int enhance;
	int type;

	int checked;
	int rescue_cap_count;
	int rescue_frame_oi_count;
	unsigned long long rescue_frame_time_count;
	int rescue_frame_count;
	int rescue_with_perf_mode;
	int *score;

	unsigned long long last_frame_ID;
	unsigned long long rescue_filter_buffer_time;
};

struct hwui_frame_info{
	struct list_head queue_list;
	unsigned long long frameID;
	unsigned long long start_ts;
	unsigned long long end_ts;
	unsigned long long dur_ts;
	unsigned long long rsc_start_ts;
	unsigned long long rsc_dur_ts;
	int display_rate;

	int perf_idx;
	int rescue_reason;
	int frame_status;
	bool overtime;
	int drop;
	bool rescue;
	bool rescue_suc;
	bool promotion;
	int using;
};

void fbt_init_ux(struct render_info *info);
void fbt_del_ux(struct render_info *info);
void reset_hwui_frame_info(struct hwui_frame_info *frame);
struct hwui_frame_info *get_valid_hwui_frame_info_from_pool(struct render_info *info);
struct hwui_frame_info *get_hwui_frame_info_by_frameid(struct render_info *info, unsigned long long frameid);
struct hwui_frame_info *insert_hwui_frame_info_from_tmp(struct render_info *thr, struct hwui_frame_info *frame);
void count_scroll_rescue_info(struct render_info *thr, struct hwui_frame_info *hwui_info);

int get_ux_list_length(struct list_head *head);
void fpsgo_ux_doframe_end(struct render_info *thr, unsigned long long frame_id,
			long long frame_flags);
void clear_ux_info(struct render_info *thr);
void enqueue_ux_scroll_info(int type, unsigned long long start_ts, struct render_info *thr);
struct ux_scroll_info *search_ux_scroll_info(unsigned long long ts, struct render_info *thr);
int fpsgo_sbe_dy_enhance(struct render_info *thr);
void fpsgo_ux_scrolling_end(struct render_info *thr);

void fpsgo_ux_delete_frame_info(struct render_info *thr, struct ux_frame_info *info);
struct ux_frame_info *fpsgo_ux_search_and_add_frame_info(struct render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action);
struct ux_frame_info *fpsgo_ux_get_next_frame_info(struct render_info *thr);
int fpsgo_ux_count_frame_info(struct render_info *thr, int target);
void fpsgo_boost_non_hwui_policy(struct render_info *thr, int set_vip);
void fpsgo_set_ux_general_policy(int scrolling, unsigned long ux_mask);
int get_ux_general_policy(void);
void fpsgo_reset_deplist_task_priority(struct render_info *thr);
void fpsgo_set_group_dvfs(int start);
void fpsgo_set_gas_policy(int start);
void update_ux_general_policy(void);

void fpsgo_sbe_rescue(struct render_info *thr, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frame_id);
void fpsgo_sbe_rescue_legacy(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id);

int fpsgo_ctrl2ux_get_perf(void);
void fbt_ux_set_perf(int cur_pid, int cur_blc);
void fbt_set_global_sbe_dy_enhance(int cur_pid, int cur_dy_enhance);
void fpsgo_ctrl2uxfbt_dfrc_fps(int fps_limit);

extern int group_set_threshold(int grp_id, int val);
extern int group_reset_threshold(int grp_id);
extern int get_dpt_default_status(void);
extern void set_ignore_idle_ctrl(bool val);
extern void set_grp_dvfs_ctrl(int set);
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
extern void flt_ctrl_force_set(int set);
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
extern void group_set_mode(u32 mode);
#endif

void __exit fbt_cpu_ux_exit(void);
int __init fbt_cpu_ux_init(void);
