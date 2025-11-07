/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __CPUFREQ_H__
#define __CPUFREQ_H__
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include <linux/cpufreq.h>

#define MAX_CAP_ENTRYIES 168

#define SRAM_REDZONE 0x55AA55AAAA55AA55
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2
#define REG_FREQ_SCALING 0x4cc
#define LUT_ROW_SIZE 0x4
#define SBB_ALL 0
#define SBB_GROUP 1
#define SBB_TASK 2
#define MAX_NR_CPUS CONFIG_MAX_NR_CPUS

DECLARE_PER_CPU(unsigned long, max_freq_scale);
DECLARE_PER_CPU(unsigned long, min_freq_scale);
DECLARE_PER_CPU(unsigned long, min_freq);

struct sbb_cpu_data {
	unsigned int active;
	unsigned int idle_time;
	unsigned int wall_time;
	unsigned int boost_factor;
	unsigned int tick_start;
	unsigned int cpu_utilize;
};

enum sugov_type {
	OPP,
	CAP,
	FREQ,
	PWR_EFF,
};

struct sugov_tunables {
	struct gov_attr_set	attr_set;
	unsigned int		up_rate_limit_us;
	unsigned int		down_rate_limit_us;
};

struct sugov_policy {
	struct cpufreq_policy	*policy;

	struct sugov_tunables	*tunables;
	struct list_head	tunables_hook;

	raw_spinlock_t		update_lock;	/* For shared policies */
	u64			last_freq_update_time;
	s64			min_rate_limit_ns;
	s64			up_rate_delay_ns;
	s64			down_rate_delay_ns;
	unsigned int		next_freq;
	unsigned int		cached_raw_freq;

	/* The next fields are only needed if fast switch cannot be used: */
	struct			irq_work irq_work;
	struct			kthread_work work;
	struct			mutex work_lock;
	struct			kthread_worker worker;
	struct task_struct	*thread;
	bool			work_in_progress;

	bool			limits_changed;
	bool			need_freq_update;
};

struct dsu_state {
	unsigned int freq;
	unsigned int volt;
	unsigned int dyn_pwr;
	unsigned int BW;
	unsigned int EMI_BW;
};

struct dsu_table {
	unsigned int nr_opp;
	unsigned int freq_max;
	unsigned int freq_min;
	struct dsu_state **tbl;

	/* for O1 Mapping */
	unsigned int min_gap_log2;
	unsigned int nr_opp_map;
	unsigned int *opp_map;
};

struct cpu_dsu_freq_state {
	bool is_eas_dsu_support;
	bool is_eas_dsu_ctrl;
	unsigned int pd_count;
	unsigned int dsu_target_freq;
	unsigned int *cpu_freq;
	unsigned int *dsu_freq_vote;
};

enum adaptive_type {
	ADAPTIVE_LOW,
	ADAPTIVE_HIGH,
	MIN_FREQ,
	MAX_FREQ,
};
extern unsigned int **adaptive_freq_array;
extern unsigned int get_adaptive_freq(unsigned int gear,enum adaptive_type type);
extern void set_adaptive_freq(unsigned int gear,unsigned int freq,enum adaptive_type type);
extern struct dsu_state *dsu_get_opp_ps(int wl, int opp);
extern unsigned int dsu_get_freq_opp(unsigned int freq);
extern void update_wl_tbl(unsigned int cpu, bool *is_cpu_to_update_thermal);
extern int get_curr_wl(void);
extern int get_curr_wl_dsu(void);
extern int get_classify_wl(void);
extern int get_em_wl(void);
extern void set_wl_manual(int val);
extern void set_wl_cpu_manual(int val);
extern void set_wl_dsu_manual(int val);
extern void set_wl_type_manual(int val);
extern int get_wl_manual(void);
extern int get_wl_dsu_manual(void);
extern int get_nr_wl(void);
extern int get_nr_wl_type(void);
extern int get_nr_cpu_type(void);
extern int get_cpu_type(int type);
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
int init_opp_cap_info(struct proc_dir_entry *dir);

extern int val_m;
extern void record_sched_pd_opp2cap(int cpu, int opp, int quant, int wl,
		int val_1, int val_2, int val_r, int *val_s, int r_o, int caller);
extern void record_sched_pd_opp2pwr_eff(int cpu, int opp, int quant, int wl,
		int val_1, int val_2, int val_3, int val_r1, int val_r2, int *val_s,
		int r_o, int caller);

extern int get_eas_hook(void);
extern int pd_opp2freq(int cpu, int opp, int quant, int wl);
extern int pd_opp2cap(int cpu, int opp, int quant, int wl, int *val_s, int r_o, int caller);
extern int pd_opp2pwr_eff(int cpu, int opp, int quant, int wl, int *val_s, int r_o, int caller);
extern int pd_opp2dyn_pwr(int cpu, int opp, int quant, int wl, int *val_s, int r_o, int caller);
extern int pd_opp2volt(int cpu, int opp, int quant, int wl);
extern int pd_util2opp(int cpu, int util, int quant, int wl, int *val_s, int r_o, int caller);
extern int pd_freq2opp(int cpu, int freq, int quant, int wl);
extern int pd_cpu_volt2opp(int cpu, int volt, int quant, int wl);
extern int pd_freq2util(unsigned int cpu, int freq, bool quant, int wl, int *val_s, int r_o);
extern int pd_util2freq(unsigned int cpu, int util, bool quant, int wl);
extern int pd_cpu_opp2dsu_freq(int cpu, int opp, int quant, int wl);
extern int pd_dsu_volt2opp(int volt);
extern int pd_get_dsu_freq(void);
extern unsigned long pd_X2Y(int cpu, unsigned long input, enum sugov_type in_type,
		enum sugov_type out_type, bool quant, int caller);

extern unsigned long pd_get_opp_capacity(unsigned int cpu, int opp);
extern unsigned long pd_get_opp_capacity_legacy(unsigned int cpu, int opp);
extern unsigned long pd_get_opp_freq(unsigned int cpu, int opp);
extern unsigned long pd_get_opp_freq_legacy(unsigned int cpu, int opp);

extern unsigned long pd_get_freq_util(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_opp(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_pwr_eff(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_opp_legacy(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_opp_legacy_type(int wl, unsigned int cpu, unsigned long freq);

extern unsigned long pd_get_util_freq(unsigned int cpu, unsigned long util);
extern unsigned long pd_get_util_pwr_eff(unsigned int cpu, unsigned long util, int caller);
extern unsigned long pd_get_util_opp(unsigned int cpu, unsigned long util);
extern unsigned long pd_get_util_opp_legacy(unsigned int cpu, unsigned long util);

extern unsigned int pd_get_cpu_opp(unsigned int cpu);
extern unsigned int pd_get_opp_leakage(unsigned int cpu, unsigned int opp,
	unsigned int temperature);
extern unsigned int pd_get_dsu_weighting(int wl, unsigned int cpu);
extern unsigned int pd_get_emi_weighting(int wl, unsigned int cpu);
extern unsigned int get_curr_cap(unsigned int cpu);
extern int get_fpsgo_bypass_flag(void);
extern void (*fpsgo_notify_fbt_is_boost_fp)(int fpsgo_is_boost);
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
void mtk_cpufreq_fast_switch(void *data, struct cpufreq_policy *policy,
				unsigned int *target_freq, unsigned int old_target_freq);
void mtk_cpufreq_target(void *data, struct cpufreq_policy *policy,
				unsigned int *target_freq, unsigned int old_target_freq);
void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
				unsigned long freq, unsigned long max, unsigned long *scale);
extern int set_sched_capacity_margin_dvfs(int capacity_margin);
extern int get_sched_capacity_margin_dvfs(void);
#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
extern void mtk_uclamp_eff_get(void *data, struct task_struct *p, enum uclamp_id clamp_id,
		struct uclamp_se *uc_max, struct uclamp_se *uc_eff, int *ret);
#endif
extern bool cu_ctrl;
extern bool get_curr_uclamp_ctrl(void);
extern void set_curr_uclamp_ctrl(int val);
extern int set_curr_uclamp_hint(int pid, int set);
extern int get_curr_uclamp_hint(int pid);
extern bool gu_ctrl;
extern bool get_gear_uclamp_ctrl(void);
extern void set_gear_uclamp_ctrl(int val);
extern int get_gear_uclamp_max(int gearid);
extern int get_cpu_gear_uclamp_max(unsigned int cpu);
extern int get_cpu_gear_uclamp_max_capacity(unsigned int cpu);
extern void set_gear_uclamp_max(int gearid, int val);
#endif
#endif
/* ignore idle util */
extern bool ignore_idle_ctrl;
extern void set_ignore_idle_ctrl(bool val);
extern bool get_ignore_idle_ctrl(void);
/* sbb */
extern void set_target_active_ratio_pct(int gear_id, int val);
extern void set_sbb(int flag, int pid, bool set);
extern void set_sbb_active_ratio_gear(int gear_id, int val);
extern void set_sbb_active_ratio(int val);
extern int get_sbb_active_ratio_gear(int gear_id);
extern bool is_sbb_trigger(struct rq *rq);
extern unsigned int get_nr_gears(void);
extern struct cpumask *get_gear_cpumask(unsigned int gear);
extern bool is_gearless_support(void);
/* dsu ctrl */
extern int wl_dsu_delay_ch_cnt;
extern int wl_cpu_delay_ch_cnt;
extern bool get_eas_dsu_ctrl(void);
extern void set_eas_dsu_ctrl(bool set);
extern void set_dsu_ctrl(bool set);
extern struct cpu_dsu_freq_state *get_dsu_freq_state(void);
bool enq_force_update_freq(struct sugov_policy *sg_policy);
extern bool is_dsu_idle_enable(void);

/* adaptive margin */
extern int am_support;
extern int get_am_ctrl(void);
extern void set_am_ctrl(int set);
extern unsigned int get_adaptive_margin(unsigned int cpu);
extern int get_cpu_util_with_margin(int cpu, int cpu_util);
extern int get_gear_max_active_ratio_cap(int gear);
extern int get_cpu_active_ratio_cap(int cpu);
extern void update_active_ratio_all(void);
extern void set_am_ceiling(int val);
extern int get_am_ceiling(void);

/* group aware dvfs */
extern int grp_dvfs_support_mode;
extern int grp_dvfs_ctrl_mode;
extern int get_grp_dvfs_ctrl(void);
extern void set_grp_dvfs_ctrl(int set);
extern bool get_grp_high_freq(int cluster_id);
extern void set_grp_high_freq(int cluster_id, bool set);


extern unsigned long get_turn_point_freq(int gearid);
DECLARE_PER_CPU(unsigned int, gear_id);
DECLARE_PER_CPU(struct sbb_cpu_data *, sbb);
DECLARE_PER_CPU(struct mtk_rq *, rq_data);

#define DEFAULT_MARGIN 1280
extern int mtk_uclamp_involve(unsigned long uclamp_min, unsigned long uclamp_max, int is_multiply_by_margin);
/* DPT */
struct curr_collab_state_struct {
	int state;
	int (*ret_function)(void);
};

void hook_update_cpu_capacity(void *data, int cpu, unsigned long *capacity);
extern int get_wl_dsu(void);
extern void *get_dpt_sram_base(void);
extern struct curr_collab_state_struct *get_curr_collab_state(void);
extern void update_curr_collab_state(bool *is_cpu_to_update_thermal);
extern struct curr_collab_state_struct *curr_collab_state;
extern int nr_collab_type;
extern int get_nr_collab_type(void);
extern void set_collab_state_manual(int type, int state);
extern void (*change_dpt_support_driver_hook) (int turn_on);
extern int (*is_dpt_support_driver_hook) (void);
extern void (*set_v_driver_hook)(int v);
#define for_each_collab_type(collab_type) for (collab_type = 0; collab_type < nr_collab_type; collab_type++)
int collab_type_0_ret_function(void);
extern int get_sys_max_cap_cluster(void);

#define DPT_CALL_UPDATE_WL_TBL 0
#define DPT_CALL_UPDATE_CURR_COLLAB_STATE 1
#define DPT_CALL_PD_FREQ2UTIL 2
#define DPT_CALL_PD_GET_FRE_UTIL 3
#define DPT_CALL_PD_GET_FREQ_PWR_EFF 4
#define DPT_CALL_PD_GET_OPP_CAPACITY 5
#define DPT_CALL_PD_GET_OPP_CAPACITY_LEGACY 6
#define DPT_CALL_ESTIMATE_ENERGY 7
#define DPT_CALL_CALC_PWR_EFF 8
#define DPT_CALL_CALC_PWR_EFF_V2 9
#define DPT_CALL_FLT_INIT 10
#define DPT_CALL_GROUP_UPDATE_THRESHOLD_UTIL 11
#define DPT_CALL_SET_GRP_AWR_THR 12
#define DPT_CALL_GRP_AWR_INIT 13
#define DPT_CALL_MTK_ARCH_SET_FREQ_SCALE_GEARLESS 14
#define DPT_CALL_CPUFREQ_UPDATE_TARGET_FREQ 15
#define DPT_CALL_MTK_ARCH_SET_FREQ_SCALE 16
#define DPT_CALL_MTK_MAP_UTIL_FREQ 17
#define DPT_CALL_FBT_CLUSTER_X2Y 18
#define DPT_CALL_MTK_SET_CPU_MIN_OPP 19
#define DPT_CALL_PD_GET_OPP_PWR_EFF 20
#define DPT_CALL_PD_UTIL2FREQ 21
#define DPT_CALL_PD_GET_UTIL_OPP 22
#define DPT_CALL_PD_GET_UTIL_OPP_LEGACY 23
#define DPT_CALL_PD_GET_UTIL_FREQ 24
#define DPT_CALL_PD_GET_CPU_OPP 25
#define DPT_CALL_MTK_EM_CPU_ENERGY 26
#define DPT_CALL_INIT_UCLAMP_INVOLVE 27

#define DPT_CALL_DEBUG1 98
#define DPT_CALL_DEBUG2 99

/* End of DPT */

#endif /* __CPUFREQ_H__ */
