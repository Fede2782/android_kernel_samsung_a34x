/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef SCHED_SYS_COMMON_H
#define SCHED_SYS_COMMON_H
#include <linux/module.h>

#define DEFAULE_WL_TYPE 0
extern int init_sched_common_sysfs(void);
extern void cleanup_sched_common_sysfs(void);

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
extern struct kobj_attribute sched_core_pause_info_attr;
extern struct kobj_attribute sched_turn_point_freq_attr;
extern struct kobj_attribute sched_target_margin_attr;
extern struct kobj_attribute sched_target_margin_low_attr;
extern struct kobj_attribute sched_switch_adap_margin_low_attr;
extern struct kobj_attribute sched_runnable_boost_ctrl;
extern unsigned long get_turn_point_freq(int cpu);
extern unsigned long get_turn_point_freq_with_wl(int cpu, int wl_type);
extern int set_turn_point_freq(int cpu, unsigned long freq);
extern int set_turn_point_freq_with_wl(int cpu, unsigned long freq, int wl_type);
extern int set_target_margin(int cpu, int margin);
extern unsigned int get_target_margin(int cpu);
extern int set_target_margin_low(int cpu, int margin);
extern unsigned int get_target_margin_low(int cpu);
extern int set_switch_adap_margin_low(int cpu, bool enable);
extern bool get_switch_adap_margin_low(int cpu);
extern unsigned int get_turn_point_util(int cpu);
extern int sched_pause_cpu(int cpu);
extern int sched_resume_cpu(int cpu);
extern int resume_cpus(struct cpumask *cpus);
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
extern int set_cpu_active_bitmask(int mask);
#endif
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
extern void task_rotate_init(void);
extern void check_for_migration(struct task_struct *p);
#endif
#endif

#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
extern struct kobj_attribute sched_prio_control;
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
extern struct kobj_attribute sched_gas_for_ta;
extern void set_top_grp_aware(int val, int force_ctrl);
extern int get_top_grp_aware(void);
#endif
