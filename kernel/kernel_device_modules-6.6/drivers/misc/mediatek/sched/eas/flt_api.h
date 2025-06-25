/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
 #ifndef _FLT_API_H
#define _FLT_API_H

#define PER_ENTRY	4

struct flt_pm {
	ktime_t ktime_last;
	bool ktime_suspended;
};

/* NID mode */
enum _flt_nid_mode {
	FLT_GP_NID = 0,
	FLT_GP_ID = 1,
	FLT_GP_NUM,
};

/* weight mode */
enum _flt_weight_mode {
	FLT_GP_NWT = 0,
	FLT_GP_WT = 1,
	FLT_GP_WT_NUM,
};

/* API Function pointer*/
struct flt_class {
	int (*flt_get_ws_api)(void);
	int (*flt_set_ws_api)(int ws);
	int (*flt_get_mode_api)(void);
	int (*flt_set_mode_api)(u32 mode);
	int (*flt_sched_set_group_policy_eas_api)(int grp_id, int ws, int wp, int wc);
	int (*flt_sched_get_group_policy_eas_api)(int grp_id, int *ws, int *wp, int *wc);
	int (*flt_sched_set_cpu_policy_eas_api)(int cpu, int ws, int wp, int wc);
	int (*flt_sched_get_cpu_policy_eas_api)(int cpu, int *ws, int *wp, int *wc);
	int (*flt_get_sum_group_api)(int grp_id);
	int (*flt_get_max_group_api)(int grp_id);
	int (*flt_get_gear_sum_pelt_group_api)(unsigned int gear_id, int grp_id);
	int (*flt_get_gear_max_pelt_group_api)(unsigned int gear_id, int grp_id);
	int (*flt_get_gear_sum_pelt_group_cnt_api)(unsigned int gear_id, int grp_id);
	int (*flt_get_gear_max_pelt_group_cnt_api)(unsigned int gear_id, int grp_id);
	int (*flt_sched_get_gear_sum_group_eas_api)(int gear_id, int grp_id);
	int (*flt_sched_get_gear_max_group_eas_api)(int gear_id, int grp_id);
	int (*flt_sched_get_cpu_group_eas_api)(int cpu, int grp_id);
	int (*flt_get_cpu_by_wp_api)(int cpu);
	int (*flt_get_task_by_wp_api)(struct task_struct *p, int wc, int task_wp);
	int (*flt_get_grp_h_eas_api)(int grp_id);
	int (*flt_get_cpu_r_api)(int cpu);
	int (*flt_get_cpu_o_eas_api)(int grp_id);
	int (*flt_get_total_gp_api)(void);
	int (*flt_get_grp_r_eas_api)(int grp_id);
	int (*flt_set_grp_dvfs_ctrl_api)(int set);
	int (*flt_setnid_eas_api)(u32 mode);
	u32 (*flt_getnid_eas_api)(void);
	int (*flt_res_init_api)(void);
	int (*flt_get_grp_weight_api)(void);
	int (*flt_set_grp_weight_api)(int set);
	int (*flt_get_grp_thr_weight_api)(void);
};

/* Note: ws setting related API */
int flt_get_ws(void);
int flt_set_ws(int ws);

/* Note: mode related API */
int flt_get_mode_io(void);
int flt_set_mode_io(u32 mode);

/* Note: Group/CPU setting related API */
int flt_sched_set_group_policy_eas(int grp_id, int ws, int wp, int wc);
int flt_sched_get_group_policy_eas(int grp_id, int *ws, int *wp, int *wc);
int flt_sched_set_cpu_policy_eas(int cpu, int ws, int wp, int wc);
int flt_sched_get_cpu_policy_eas(int cpu, int *ws, int *wp, int *wc);

/* Note: group related API */
int flt_get_sum_group(int grp_id);
int flt_get_max_group(int grp_id);
int flt_get_gear_sum_pelt_group(unsigned int gear_id, int grp_id);
int flt_get_gear_max_pelt_group(unsigned int gear_id, int grp_id);
int flt_get_gear_sum_pelt_group_cnt(unsigned int gear_id, int grp_id);
int flt_get_gear_max_pelt_group_cnt(unsigned int gear_id, int grp_id);
int flt_sched_get_gear_max_group_eas(int gear_id, int grp_id);
int flt_sched_get_gear_sum_group_eas(int gear_id, int grp_id);
int flt_get_gp_r(int grp_id);
int flt_set_grp_dvfs_ctrl(int set);
int flt_setnid(u32 mode);
u32 flt_getnid(void);
int flt_get_grp_weight(void);
int flt_set_grp_weight(int set);
int flt_get_grp_thr_weight(void);

/* Note: cpu related API */
int flt_get_cpu_by_wp(int cpu);
unsigned long flt_get_cpu(int cpu);
int flt_get_cpu_r(int cpu);

/* Note: task related API */
int flt_get_task_by_wp(struct task_struct *p, int wc, int task_wp);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
void register_sugov_hooks(void);
extern unsigned long (*flt_get_cpu_util_hook)(int cpu);
extern unsigned long (*flt_sched_get_cpu_group_util_eas_hook)(int cpu, int group_id);
extern void (*flt_get_fpsgo_boosting)(int fpsgo_flag);
extern void flt_ctrl_force_set(int set);
extern bool flt_ctrl_force_get(void);
#endif

/* suspend/resume api */
void flt_resume_notify(void);
void flt_suspend_notify(void);
void flt_get_pm_status(struct flt_pm *fltpm);

/* interface api */
void flt_update_data(unsigned int data, unsigned int offset);
unsigned int flt_get_data(unsigned int offset);

/* resource init api */
int flt_res_init(void);
#endif /* _FLT_API_H */
