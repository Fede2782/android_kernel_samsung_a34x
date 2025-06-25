/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SCHEDULER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHEDULER_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/compat.h>

#ifdef CREATE_TRACE_POINTS
int sched_cgroup_state(struct task_struct *p, int subsys_id)
{
#ifdef CONFIG_CGROUPS
	int cgrp_id = -1;
	struct cgroup_subsys_state *css;

	rcu_read_lock();
	css = task_css(p, subsys_id);
	if (!css)
		goto out;

	cgrp_id = css->id;

out:
	rcu_read_unlock();

	return cgrp_id;
#else
	return -1;
#endif
}
#endif

TRACE_EVENT(sched_find_cpu_in_irq,

	TP_PROTO(struct task_struct *tsk, int policy, int target_cpu,
		int prev_cpu, unsigned int fit_cpus, unsigned int idle_cpus,
		int best_idle_cpu, unsigned long best_idle_pwr, unsigned int min_exit_lat,
		int max_spare_cpu, unsigned long best_pwr, long max_spare_cap),

	TP_ARGS(tsk, policy, target_cpu,
			prev_cpu, fit_cpus, idle_cpus,
			best_idle_cpu, best_idle_pwr, min_exit_lat,
			max_spare_cpu, best_pwr, max_spare_cap),

	TP_STRUCT__entry(
		__field(pid_t,         pid)
		__field(int,           policy)
		__field(int,           target_cpu)
		__field(int,           prev_cpu)
		__field(unsigned int,  fit_cpus)
		__field(unsigned int,  idle_cpus)
		__field(int,           best_idle_cpu)
		__field(unsigned long, best_idle_pwr)
		__field(unsigned int,  min_exit_lat)
		__field(int,           max_spare_cpu)
		__field(unsigned long, best_pwr)
		__field(long,          max_spare_cap)
		),

	TP_fast_assign(
		__entry->pid                     = tsk->pid;
		__entry->policy                  = policy;
		__entry->target_cpu              = target_cpu;
		__entry->prev_cpu                = prev_cpu;
		__entry->fit_cpus                = fit_cpus;
		__entry->idle_cpus               = idle_cpus;
		__entry->best_idle_cpu           = best_idle_cpu;
		__entry->best_idle_pwr           = best_idle_pwr;
		__entry->min_exit_lat            = min_exit_lat;
		__entry->max_spare_cpu           = max_spare_cpu;
		__entry->best_pwr                = best_pwr;
		__entry->max_spare_cap           = max_spare_cap;
		),

	TP_printk("pid=%4d policy=0x%08x target_cpu=%d task_cpu=%d fit_cpus=0x%x idle_cpus=0x%x best_idle_cpu=%d best_idle_pwr=%lu min_exit_lat=%u max_spare_cpu=%d best_pwr=%lu max_spare_cap=%ld",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->prev_cpu,
		__entry->fit_cpus,
		__entry->idle_cpus,
		__entry->best_idle_cpu,
		__entry->best_idle_pwr,
		__entry->min_exit_lat,
		__entry->max_spare_cpu,
		__entry->best_pwr,
		__entry->max_spare_cap)
);

#if IS_ENABLED(CONFIG_MTK_SCHED_UPDOWN_MIGRATE)
TRACE_EVENT(sched_fits_cap_ceiling,

	TP_PROTO(int fit, int cpu, unsigned long util, unsigned long uclamp_min,
		unsigned long uclamp_max, unsigned long cap,
		unsigned long ceiling, unsigned int sugov_margin,
		unsigned int capacity_dn_margin, unsigned int capacity_up_margin, bool AM_enabled,
		int uclamp_involve),

	TP_ARGS(fit, cpu, util, uclamp_min, uclamp_max, cap,
			ceiling, sugov_margin,
			capacity_dn_margin, capacity_up_margin, AM_enabled, uclamp_involve),

	TP_STRUCT__entry(
		__field(int, fit)
		__field(int, cpu)
		__field(unsigned long,   util)
		__field(unsigned long,   uclamp_min)
		__field(unsigned long,   uclamp_max)
		__field(unsigned long,   cap)
		__field(unsigned long,   thermal_pressure)
		__field(unsigned long,   ceiling)
		__field(unsigned int,   sugov_margin)
		__field(unsigned int,   capacity_dn_margin)
		__field(unsigned int,   capacity_up_margin)
		__field(unsigned long,   capacity_orig)
		__field(bool,			AM_enabled)
		__field(int,			uclamp_involve)
		),

	TP_fast_assign(
		__entry->fit				= fit;
		__entry->cpu				= cpu;
		__entry->util				= util;
		__entry->uclamp_min			= uclamp_min;
		__entry->uclamp_max			= uclamp_max;
		__entry->cap				= cap;
		__entry->thermal_pressure	= arch_scale_thermal_pressure(cpu);
		__entry->ceiling			= ceiling;
		__entry->sugov_margin	= sugov_margin;
		__entry->capacity_dn_margin	= capacity_dn_margin;
		__entry->capacity_up_margin	= capacity_up_margin;
		__entry->capacity_orig		= capacity_orig_of(cpu);
		__entry->AM_enabled			= AM_enabled;
		__entry->uclamp_involve			= uclamp_involve;
		),

	TP_printk(
		"fit=%d cpu=%d util=%ld uclamp_min=%lu uclamp_max=%lu cap_normal=%lu thermal=%lu ceiling=%ld capacity_dn_margin=%d capacity_up_margin=%d sugov_margin=%d cap_origin=%ld uclamp_involve=%d adaptive_margin_ctrl=%d",
		__entry->fit,
		__entry->cpu,
		__entry->util,
		__entry->uclamp_min,
		__entry->uclamp_max,
		__entry->cap,
		__entry->thermal_pressure,
		__entry->ceiling,
		__entry->capacity_dn_margin,
		__entry->capacity_up_margin,
		__entry->sugov_margin,
		__entry->capacity_orig,
		__entry->uclamp_involve,
		__entry->AM_enabled)
);
#endif


TRACE_EVENT(sched_get_gear_indices,

	TP_PROTO(struct task_struct *tsk, int uclamp_task_util,
		bool gear_hints_enable, int gear_start, int num_gear,
		int gear_reverse, int num_sched_clusters, int max_gear_num,
		int order_index, int end_index, int reverse),

	TP_ARGS(tsk, uclamp_task_util, gear_hints_enable,
			gear_start, num_gear, gear_reverse,
			num_sched_clusters, max_gear_num,
			order_index, end_index, reverse),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int,   uclamp_task_util)
		__field(int,   gear_hints_enable)
		__field(int,   gear_start)
		__field(int,   num_gear)
		__field(int,   gear_reverse)
		__field(int,   num_sched_clusters)
		__field(int,   max_gear_num)
		__field(int,   order_index)
		__field(int,   end_index)
		__field(int,   reverse)
		),

	TP_fast_assign(
		__entry->pid				= tsk->pid;
		__entry->uclamp_task_util	= uclamp_task_util;
		__entry->gear_hints_enable	= gear_hints_enable;
		__entry->gear_start			= gear_start;
		__entry->num_gear			= num_gear;
		__entry->gear_reverse		= gear_reverse;
		__entry->num_sched_clusters	= num_sched_clusters;
		__entry->max_gear_num		= max_gear_num;
		__entry->order_index		= order_index;
		__entry->end_index			= end_index;
		__entry->reverse			= reverse;
		),

	TP_printk(
		"pid=%d uclamp_task_util=%d gear_hints_enable=%d gear_start=%d num_gear=%d gear_reverse=%d num_sched_clusters=%d max_gear_num=%d order_index=%d end_index=%d reverse=%d",
		__entry->pid,
		__entry->uclamp_task_util,
		__entry->gear_hints_enable,
		__entry->gear_start,
		__entry->num_gear,
		__entry->gear_reverse,
		__entry->num_sched_clusters,
		__entry->max_gear_num,
		__entry->order_index,
		__entry->end_index,
		__entry->reverse)
);

TRACE_EVENT(sched_util_fits_cpu,

	TP_PROTO(int cpu, unsigned long pre_clamped_util, unsigned long clamped_util,
		unsigned long cpu_cap, unsigned long min_cap, unsigned long max_cap, struct rq *rq),

	TP_ARGS(cpu, pre_clamped_util, clamped_util, cpu_cap, min_cap, max_cap, rq),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(unsigned long,	pre_clamped_util)
		__field(unsigned long,	clamped_util)
		__field(unsigned long,   cpu_cap)
		__field(unsigned long,   task_min_cap)
		__field(unsigned long,   task_max_cap)
		__field(unsigned long,   rq_min_cap)
		__field(unsigned long,   rq_max_cap)
		),

	TP_fast_assign(
		__entry->cpu				= cpu;
		__entry->pre_clamped_util	= pre_clamped_util;
		__entry->clamped_util		= clamped_util;
		__entry->cpu_cap			= cpu_cap;
		__entry->task_min_cap		= min_cap;
		__entry->task_max_cap		= max_cap;
		__entry->rq_min_cap			= READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
		__entry->rq_max_cap			= READ_ONCE(rq->uclamp[UCLAMP_MAX].value);
		),

	TP_printk(
		"cpu=%d pre_clamped_util=%ld clamped_util=%ld cap_normal=%ld task_min_cap=%ld task_max_cap=%ld rq_min_cap=%ld rq_max_cap=%ld",
		__entry->cpu,
		__entry->pre_clamped_util,
		__entry->clamped_util,
		__entry->cpu_cap,
		__entry->task_min_cap,
		__entry->task_max_cap,
		__entry->rq_min_cap,
		__entry->rq_max_cap)
);

TRACE_EVENT(sched_find_best_candidates,

	TP_PROTO(struct task_struct *tsk, bool is_vip,
		struct cpumask *candidates, int order_index, int end_index,
		struct cpumask *allowed_cpu_mask),

	TP_ARGS(tsk, is_vip, candidates, order_index, end_index, allowed_cpu_mask),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(bool,  is_vip)
		__field(long,  candidates)
		__field(int,   order_index)
		__field(int,   end_index)
		__field(long,   active_mask)
		__field(long,   pause_mask)
		__field(long,   allowed_cpu_mask)
		),

	TP_fast_assign(
		__entry->pid        = tsk->pid;
		__entry->is_vip     = is_vip;
		__entry->candidates = candidates->bits[0];
		__entry->order_index   = order_index;
		__entry->end_index     = end_index;
		__entry->active_mask     = cpu_active_mask->bits[0];
		__entry->pause_mask     = cpu_pause_mask->bits[0];
		__entry->allowed_cpu_mask     = allowed_cpu_mask->bits[0];
		),

	TP_printk(
		"pid=%d is_vip=%d candidates=0x%lx order_index=%d end_index=%d active_mask=0x%lx pause_mask=0x%lx allowed_mask=0x%lx",
		__entry->pid,
		__entry->is_vip,
		__entry->candidates,
		__entry->order_index,
		__entry->end_index,
		__entry->active_mask,
		__entry->pause_mask,
		__entry->allowed_cpu_mask)
);

TRACE_EVENT(sched_target_max_spare_cpu,

	TP_PROTO(const char *type, int best_cpu, int new_cpu, int replace,
		long spare_cap, long target_max_spare_cap),

	TP_ARGS(type, best_cpu, new_cpu, replace,
		spare_cap, target_max_spare_cap),

	TP_STRUCT__entry(
		__string(type, type)
		__field(int, best_cpu)
		__field(int, new_cpu)
		__field(int, replace)
		__field(long, spare_cap)
		__field(long, target_max_spare_cap)
		),

	TP_fast_assign(
		__assign_str(type, type);
		__entry->best_cpu        = best_cpu;
		__entry->new_cpu        = new_cpu;
		__entry->replace        = replace;
		__entry->spare_cap        = spare_cap;
		__entry->target_max_spare_cap        = target_max_spare_cap;
		),

	TP_printk("type=%s best_cpu=%d new_cpu=%d replace=%d spare_cap=%ld target_max_spare_cap=%ld",
		__get_str(type),
		__entry->best_cpu,
		__entry->new_cpu,
		__entry->replace,
		__entry->spare_cap,
		__entry->target_max_spare_cap)
);

TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk, bool in_irq,
		int policy, int backup_reason, int prev_cpu, int target_cpu,
		int task_util, int task_util_est, int boost, bool prefer,
		int sync_flag, struct cpumask *effective_softmask),

	TP_ARGS(tsk, in_irq, policy, backup_reason, prev_cpu, target_cpu, task_util, task_util_est,
		boost, prefer, sync_flag, effective_softmask),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, compat_thread)
		__field(bool, in_irq)
		__field(int, policy)
		__field(int, backup_reason)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, task_util)
		__field(int, task_util_est)
		__field(int, boost)
		__field(long, task_mask)
		__field(long, effective_softmask)
		__field(bool, prefer)
		__field(int, sync_flag)
		__field(int, cpuctl_grp_id)
		__field(int, cpuset_grp_id)
		),

	TP_fast_assign(
		__entry->pid        = tsk->pid;
#if IS_ENABLED(CONFIG_ARM64)
		__entry->compat_thread = is_compat_thread(task_thread_info(tsk));
#else
		__entry->compat_thread = 0;
#endif
		__entry->in_irq     = in_irq;
		__entry->policy     = policy;
		__entry->backup_reason     = backup_reason;
		__entry->prev_cpu   = prev_cpu;
		__entry->target_cpu = target_cpu;
		__entry->task_util      = task_util;
		__entry->task_util_est  = task_util_est;
		__entry->boost          = boost;
		__entry->task_mask      = tsk->cpus_ptr->bits[0];
		__entry->effective_softmask = effective_softmask->bits[0];
		__entry->prefer         = prefer;
		__entry->sync_flag     = sync_flag;
		__entry->cpuctl_grp_id = sched_cgroup_state(tsk, cpu_cgrp_id);
		__entry->cpuset_grp_id = sched_cgroup_state(tsk, cpuset_cgrp_id);
		),

	TP_printk(
		"pid=%4d 32-bit=%d in_irq=%d policy=0x%08x backup_reason=0x%04x pre-cpu=%d target=%d util=%d util_est=%d uclamp=%d mask=0x%lx eff_softmask=0x%lx latency_sensitive=%d sync=%d cpuctl=%d cpuset=%d",
		__entry->pid,
		__entry->compat_thread,
		__entry->in_irq,
		__entry->policy,
		__entry->backup_reason,
		__entry->prev_cpu,
		__entry->target_cpu,
		__entry->task_util,
		__entry->task_util_est,
		__entry->boost,
		__entry->task_mask,
		__entry->effective_softmask,
		__entry->prefer,
		__entry->sync_flag,
		__entry->cpuctl_grp_id,
		__entry->cpuset_grp_id)
);

TRACE_EVENT(sched_effective_mask,

	TP_PROTO(struct task_struct *tsk, int target_cpu, bool prefer,
			struct cpumask *effective_softmask, struct cpumask *tsk_softmask,
			struct cpumask *tg_softmask),

	TP_ARGS(tsk, target_cpu, prefer, effective_softmask, tsk_softmask, tg_softmask),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, target_cpu)
		__field(bool, prefer)
		__field(long, effective_softmask)
		__field(long, tsk_softmask)
		__field(long, tg_softmask)
		__field(int, cpuctl)
		),

	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->target_cpu = target_cpu;
		__entry->prefer         = prefer;
		__entry->effective_softmask = effective_softmask->bits[0];
		__entry->tsk_softmask = tsk_softmask->bits[0];
		__entry->tg_softmask = tg_softmask->bits[0];
		__entry->cpuctl = sched_cgroup_state(tsk, cpu_cgrp_id);
		),

	TP_printk(
		"pid=%4d target=%d latency_sensitive=%d eff_softmask=0x%lx tsk_softmask=0x%lx tg_softmask=0x%lx, cpuctl=%d",
		__entry->pid,
		__entry->target_cpu,
		__entry->prefer,
		__entry->effective_softmask,
		__entry->tsk_softmask,
		__entry->tg_softmask,
		__entry->cpuctl)
);

TRACE_EVENT(sched_energy_init,

	TP_PROTO(struct cpumask *pd_mask, unsigned int gear_idx, unsigned long cpu_cap,
		unsigned long pds_cap),

	TP_ARGS(pd_mask, gear_idx, cpu_cap, pds_cap),

	TP_STRUCT__entry(
		__field(long, cpu_mask)
		__field(unsigned int, gear_idx)
		__field(unsigned long, cpu_cap)
		__field(unsigned long, pds_cap)
		),

	TP_fast_assign(
		__entry->cpu_mask   = pd_mask->bits[0];
		__entry->gear_idx   = gear_idx;
		__entry->cpu_cap    = cpu_cap;
		__entry->pds_cap    = pds_cap;
		),

	TP_printk("mask=0x%lx gear_idx=%d cpu_cap=%lu pds_cap=%lu",
		__entry->cpu_mask,
		__entry->gear_idx,
		__entry->cpu_cap,
		__entry->pds_cap)
);

TRACE_EVENT(sched_eenv_init,

	TP_PROTO(unsigned int dsu_freq_base, unsigned int dsu_volt_base,
			unsigned int dsu_freq_floor, unsigned int dsu_freq_ceil,
			unsigned int dsu_freq_thermal, unsigned int dsu_bw_base,
			unsigned int emi_bw_base, unsigned int gear_idx),

	TP_ARGS(dsu_freq_base, dsu_volt_base, dsu_freq_floor, dsu_freq_ceil,
			dsu_freq_thermal, dsu_bw_base, emi_bw_base, gear_idx),

	TP_STRUCT__entry(
		__field(int, dsu_freq_base)
		__field(int, dsu_volt_base)
		__field(int, dsu_freq_floor)
		__field(int, dsu_freq_ceil)
		__field(int, dsu_freq_thermal)
		__field(int, dsu_bw_base)
		__field(int, emi_bw_base)
		__field(unsigned int, gear_idx)
		),

	TP_fast_assign(
		__entry->dsu_freq_base = (int) dsu_freq_base;
		__entry->dsu_volt_base = (int) dsu_volt_base;
		__entry->dsu_freq_floor = (int) dsu_freq_floor;
		__entry->dsu_freq_ceil = (int) dsu_freq_ceil;
		__entry->dsu_freq_thermal = (int) dsu_freq_thermal;
		__entry->dsu_bw_base = (int) dsu_bw_base;
		__entry->emi_bw_base = (int) emi_bw_base;
		__entry->gear_idx = gear_idx;
		),

	TP_printk("dsu_freq_base=%d dsu_volt_base=%d dsu_freq_floor=%d dsu_freq_ceil=%d dsu_freq_thermal=%d dsu_bw_base=%d emi_bw_base=%d share_buck_idx=%u",
		__entry->dsu_freq_base,
		__entry->dsu_volt_base,
		__entry->dsu_freq_floor,
		__entry->dsu_freq_ceil,
		__entry->dsu_freq_thermal,
		__entry->dsu_bw_base,
		__entry->emi_bw_base,
		__entry->gear_idx)
);

#define UNIT_NAME_LEN  10
TRACE_EVENT(sched_check_temp,

	TP_PROTO(char *unit_name, int id, int temp),

	TP_ARGS(unit_name, id, temp),

	TP_STRUCT__entry(
		__array(char, unit_name, UNIT_NAME_LEN)
		__field(int, id)
		__field(int, temp)
		),

	TP_fast_assign(
		strncpy(__entry->unit_name, unit_name, 4);
		__entry->id = id;
		__entry->temp = temp;
		),

	TP_printk("%s temperature error!! %s=%d temp=%d",
		__entry->unit_name,
		__entry->unit_name,
		__entry->id,
		__entry->temp)
);

TRACE_EVENT(sched_per_core_BW,

	TP_PROTO(int cpu, unsigned int bw, unsigned int sum_bw),

	TP_ARGS(cpu, bw, sum_bw),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, bw)
		__field(int, sum_bw)
		),

	TP_fast_assign(
		__entry->cpu        = cpu;
		__entry->bw         = (int) bw;
		__entry->sum_bw     = (int) sum_bw;
		),

	TP_printk("cpu=%d bw=%d sum_bw=%d",
		__entry->cpu,
		__entry->bw,
		__entry->sum_bw)
);

TRACE_EVENT(sched_max_util,

	TP_PROTO(const char *domain_name, int idx, int dst_cpu, int dst_idx,
		unsigned long max_util, int cpu, unsigned long util, unsigned long cpu_util),

	TP_ARGS(domain_name, idx, dst_cpu, dst_idx, max_util, cpu, util, cpu_util),

	TP_STRUCT__entry(
		__string(domain_name, domain_name)
		__field(int, idx)
		__field(int, dst_cpu)
		__field(int, dst_idx)
		__field(unsigned long, max_util)
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned long, cpu_util)
		),

	TP_fast_assign(
		__assign_str(domain_name, domain_name);
		__entry->idx   = idx;
		__entry->dst_cpu    = dst_cpu;
		__entry->dst_idx    = dst_idx;
		__entry->max_util   = max_util;
		__entry->cpu        = cpu;
		__entry->util       = util;
		__entry->cpu_util   = cpu_util;
		),

	TP_printk("%s_idx=%d dst_cpu=%d dst_idx=%d %s_max_util[%s_idx][dst_idx]=%lu cpu=%d util=%ld cpu_util=%ld",
		__get_str(domain_name),
		__entry->idx,
		__entry->dst_cpu,
		__entry->dst_idx,
		__get_str(domain_name),
		__get_str(domain_name),
		__entry->max_util,
		__entry->cpu,
		__entry->util,
		__entry->cpu_util)
);

TRACE_EVENT(sched_compute_energy,

	TP_PROTO(int dst_cpu, int gear_id, struct cpumask *pd_mask,
		unsigned long energy, int shared_buck_mode,
		long gear_max_util, unsigned long pd_max_util, unsigned long sum_util,
		long gear_volt, long pd_volt, long dsu_volt, long extern_volt),

	TP_ARGS(dst_cpu, gear_id, pd_mask, energy, shared_buck_mode, pd_max_util, gear_max_util,
			sum_util, gear_volt, pd_volt, dsu_volt, extern_volt),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(int, gear_id)
		__field(long, cpu_mask)
		__field(unsigned long, energy)
		__field(int, shared_buck_mode)
		__field(long, gear_max_util)
		__field(unsigned long, pd_max_util)
		__field(unsigned long, sum_util)
		__field(long, gear_volt)
		__field(long, pd_volt)
		__field(long, dsu_volt)
		__field(long, extern_volt)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->gear_id    = gear_id;
		__entry->cpu_mask   = pd_mask->bits[0];
		__entry->energy     = energy;
		__entry->shared_buck_mode = shared_buck_mode;
		__entry->gear_max_util   = gear_max_util;
		__entry->pd_max_util   = pd_max_util;
		__entry->sum_util   = sum_util;
		__entry->gear_volt   = gear_volt;
		__entry->pd_volt   = pd_volt;
		__entry->dsu_volt   = dsu_volt;
		__entry->extern_volt   = extern_volt;
		),

	TP_printk("dst_cpu=%d gear_id=%d mask=0x%lx energy=%lu shared_buck_mode=%d gear_max_util=%ld pd_max_util=%lu sum_util=%lu gear_volt=%ld pd_volt=%ld dsu_volt=%ld extern_volt=%ld",
		__entry->dst_cpu,
		__entry->gear_id,
		__entry->cpu_mask,
		__entry->energy,
		__entry->shared_buck_mode,
		__entry->gear_max_util,
		__entry->pd_max_util,
		__entry->sum_util,
		__entry->gear_volt,
		__entry->pd_volt,
		__entry->dsu_volt,
		__entry->extern_volt)
);

TRACE_EVENT(sched_compute_energy_dsu,

	TP_PROTO(int dst_cpu, unsigned long task_util, unsigned long sum_util,
		unsigned int dsu_bw, unsigned int emi_bw, int temp, unsigned int dsu_freq,
		unsigned int dsu_volt, unsigned int dsu_extern_volt),

	TP_ARGS(dst_cpu, task_util, sum_util, dsu_bw, emi_bw, temp, dsu_freq,
			dsu_volt, dsu_extern_volt),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(unsigned long, task_util)
		__field(unsigned long, sum_util)
		__field(unsigned int, dsu_bw)
		__field(unsigned int, emi_bw)
		__field(int, temp)
		__field(unsigned int, dsu_freq)
		__field(unsigned int, dsu_volt)
		__field(unsigned int, dsu_extern_volt)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->task_util  = task_util;
		__entry->sum_util   = sum_util;
		__entry->dsu_bw     = dsu_bw;
		__entry->emi_bw     = emi_bw;
		__entry->temp       = temp;
		__entry->dsu_freq   = dsu_freq;
		__entry->dsu_volt   = dsu_volt;
		__entry->dsu_extern_volt   = dsu_extern_volt;
		),

	TP_printk("dst_cpu=%d task_util=%lu sum_util=%lu dsu_bw=%u emi_bw=%u temp=%d dsu_freq=%u dsu_volt=%u dsu_extern_volt=%u",
		__entry->dst_cpu,
		__entry->task_util,
		__entry->sum_util,
		__entry->dsu_bw,
		__entry->emi_bw,
		__entry->temp,
		__entry->dsu_freq,
		__entry->dsu_volt,
		__entry->dsu_extern_volt)
);

TRACE_EVENT(sched_compute_energy_cpu_dsu,

	TP_PROTO(int dst_cpu, int wl, unsigned long cpu_pwr,
		unsigned long shared_pwr_dvfs, unsigned long shared_pwr,
		unsigned long dsu_pwr, unsigned long sum_pwr),

	TP_ARGS(dst_cpu, wl, cpu_pwr, shared_pwr_dvfs, shared_pwr, dsu_pwr, sum_pwr),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(int, wl)
		__field(unsigned long, cpu_pwr)
		__field(unsigned long, shared_pwr_dvfs)
		__field(unsigned long, shared_pwr)
		__field(unsigned long, dsu_pwr)
		__field(unsigned long, sum_pwr)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->wl         = wl;
		__entry->cpu_pwr    = cpu_pwr;
		__entry->shared_pwr_dvfs = shared_pwr_dvfs;
		__entry->shared_pwr = shared_pwr;
		__entry->dsu_pwr    = dsu_pwr;
		__entry->sum_pwr    = sum_pwr;
		),

	TP_printk("dst_cpu=%d wl=%d cpu_pwr=%lu shared_pwr_dvfs=%lu share_buck_pd_pwr=%lu dsu_pwr=%lu sum=%lu",
		__entry->dst_cpu,
		__entry->wl,
		__entry->cpu_pwr,
		__entry->shared_pwr_dvfs,
		__entry->shared_pwr,
		__entry->dsu_pwr,
		__entry->sum_pwr)
);

TRACE_EVENT(sched_energy_delta,

	TP_PROTO(unsigned long pwr_delta),

	TP_ARGS(pwr_delta),

	TP_STRUCT__entry(
		__field(unsigned long, pwr_delta)
		),

	TP_fast_assign(
		__entry->pwr_delta  = pwr_delta;
		),

	TP_printk("pwr_delta=%lu",
		__entry->pwr_delta)
);

TRACE_EVENT(sched_find_energy_efficient_cpu,

	TP_PROTO(bool in_irq, unsigned long best_delta,
		int best_energy_cpu, int best_idle_cpu, int idle_max_spare_cap_cpu,
		int sys_max_spare_cap_cpu),

	TP_ARGS(in_irq, best_delta, best_energy_cpu, best_idle_cpu,
		idle_max_spare_cap_cpu, sys_max_spare_cap_cpu),

	TP_STRUCT__entry(
		__field(bool, in_irq)
		__field(unsigned long, best_delta)
		__field(int, best_energy_cpu)
		__field(int, best_idle_cpu)
		__field(int, idle_max_spare_cap_cpu)
		__field(int, sys_max_spare_cap_cpu)
		),

	TP_fast_assign(
		__entry->in_irq          = in_irq;
		__entry->best_delta      = best_delta;
		__entry->best_energy_cpu = best_energy_cpu;
		__entry->best_idle_cpu   = best_idle_cpu;
		__entry->idle_max_spare_cap_cpu = idle_max_spare_cap_cpu;
		__entry->sys_max_spare_cap_cpu = sys_max_spare_cap_cpu;
		),

	TP_printk("in_irq=%d best_delta=%lu best_energy_cpu=%d best_idle_cpu=%d idle_max_spare_cap_cpu=%d sys_max_spare_cpu=%d",
		__entry->in_irq,
		__entry->best_delta,
		__entry->best_energy_cpu,
		__entry->best_idle_cpu,
		__entry->idle_max_spare_cap_cpu,
		__entry->sys_max_spare_cap_cpu)
);

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_force_migrate,

	TP_PROTO(struct task_struct *tsk, int dest, int force),

	TP_ARGS(tsk, dest, force),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,  dest)
		__field(int,  force)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid   = tsk->pid;
		__entry->dest  = dest;
		__entry->force = force;
		),

	TP_printk("comm=%s pid=%d dest=%d force=%d",
		__entry->comm, __entry->pid,
		__entry->dest, __entry->force)
);

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_next_new_balance,

	TP_PROTO(u64 now_ns, u64 next_balance),

	TP_ARGS(now_ns, next_balance),

	TP_STRUCT__entry(
		__field(u64, now_ns)
		__field(u64, next_balance)
		),

	TP_fast_assign(
		__entry->now_ns = now_ns;
		__entry->next_balance = next_balance;
		),

	TP_printk("now_ns=%llu next_balance=%lld",
		__entry->now_ns, __entry->next_balance)
);

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
TRACE_EVENT(sched_find_imbalanced_vvip_gear,
	TP_PROTO(int cpu, int num_vvip_in_gear),

	TP_ARGS(cpu, num_vvip_in_gear),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, num_vvip_in_gear)
	),

	TP_fast_assign(
		__entry->cpu               = cpu;
		__entry->num_vvip_in_gear  = num_vvip_in_gear;
	),

	TP_printk("cpu=%d num_vvip_in_gear=%d",
		  __entry->cpu, __entry->num_vvip_in_gear)
);

TRACE_EVENT(sched_find_min_num_vip_cpus,
	TP_PROTO(bool failed, int pid, struct cpumask *vip_candidate, u64 num_vip_in_cpu_bit),

	TP_ARGS(failed, pid, vip_candidate, num_vip_in_cpu_bit),

	TP_STRUCT__entry(
		__field(bool, failed)
		__field(int, pid)
		__field(long, vip_candidate)
		__field(u64, num_vip_in_cpu_bit)
	),

	TP_fast_assign(
		__entry->failed					= failed;
		__entry->pid					= pid;
		__entry->vip_candidate          = cpumask_bits(vip_candidate)[0];
		__entry->num_vip_in_cpu_bit     = num_vip_in_cpu_bit
	),

	TP_printk("failed=%d pid=%d vip_candidate=0x%lx num_vip_in_cpu_bit=%llu",
		  __entry->failed, __entry->pid, __entry->vip_candidate, __entry->num_vip_in_cpu_bit)
);

TRACE_EVENT(sched_get_vip_task_prio,
	TP_PROTO(struct task_struct *p, int vip_prio, bool is_ls, unsigned int ls_vip_threshold,
			unsigned int group_threshold, bool is_basic_vip),

	TP_ARGS(p, vip_prio, is_ls, ls_vip_threshold, group_threshold, is_basic_vip),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, vip_prio)
		__field(int, prio)
		__field(bool, is_ls)
		__field(unsigned int, ls_vip_threshold)
		__field(int, cpuctl)
		__field(unsigned int, group_threshold)
		__field(bool, is_basic_vip)
	),

	TP_fast_assign(
		__entry->pid               = p->pid;
		__entry->vip_prio          = vip_prio;
		__entry->prio              = p->prio;
		__entry->is_ls             = is_ls;
		__entry->ls_vip_threshold  = ls_vip_threshold;
		__entry->cpuctl            = sched_cgroup_state(p, cpu_cgrp_id);
		__entry->group_threshold   = group_threshold;
		__entry->is_basic_vip      = is_basic_vip;
	),

	TP_printk("pid=%d vip_prio=%d prio=%d is_ls=%d ls_vip_threshold=%d cpuctl=%d group_threshold=%d is_basic_vip=%d",
		  __entry->pid, __entry->vip_prio, __entry->prio, __entry->is_ls,
		  __entry->ls_vip_threshold, __entry->cpuctl, __entry->group_threshold,
		  __entry->is_basic_vip)
);
TRACE_EVENT(sched_insert_vip_task,
	TP_PROTO(struct task_struct *p, int cpu, int vip_prio, bool at_front,
			pid_t prev_pid, pid_t next_pid, bool requeue, bool is_first_entry, int num_vip),

	TP_ARGS(p, cpu, vip_prio, at_front, prev_pid, next_pid, requeue,
		is_first_entry, num_vip),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, cpu)
		__field(int, vip_prio)
		__field(bool, at_front)
		__field(int, prev_pid)
		__field(int, next_pid)
		__field(bool, requeue)
		__field(bool, is_first_entry)
		__field(int, prio)
		__field(int, cpuctl)
		__field(int, num_vip)
	),

	TP_fast_assign(
		__entry->pid       = p->pid;
		__entry->cpu       = cpu;
		__entry->vip_prio  = vip_prio;
		__entry->at_front  = at_front;
		__entry->prev_pid  = prev_pid;
		__entry->next_pid  = next_pid;
		__entry->requeue   = requeue;
		__entry->is_first_entry = is_first_entry;
		__entry->prio    = p->prio;
		__entry->cpuctl  = sched_cgroup_state(p, cpu_cgrp_id);
		__entry->num_vip = num_vip;
	),

	TP_printk("pid=%d cpu=%d num_vip=%d vip_prio=%d at_front=%d prev_pid=%d next_pid=%d requeue=%d, is_first_entry=%d, prio=%d, cpuctl=%d",
		  __entry->pid, __entry->cpu, __entry->num_vip, __entry->vip_prio, __entry->at_front,
		  __entry->prev_pid, __entry->next_pid, __entry->requeue, __entry->is_first_entry,
		__entry->prio, __entry->cpuctl)
);

TRACE_EVENT(sched_deactivate_vip_task,
	TP_PROTO(pid_t pid, int cpu, pid_t prev_pid,
			pid_t next_pid, int num_vip),

	TP_ARGS(pid, cpu, prev_pid, next_pid, num_vip),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, cpu)
		__field(int, prev_pid)
		__field(int, next_pid)
		__field(int, num_vip)
	),

	TP_fast_assign(
		__entry->pid       = pid;
		__entry->cpu       = cpu;
		__entry->prev_pid  = prev_pid;
		__entry->next_pid  = next_pid;
		__entry->num_vip   = num_vip;
	),

	TP_printk("pid=%d cpu=%d orig_prev_pid=%d orig_next_pid=%d num_vip=%d",
		  __entry->pid, __entry->cpu, __entry->prev_pid,
		  __entry->next_pid, __entry->num_vip)
);

TRACE_EVENT(sched_set_vip,
	TP_PROTO(int id, int done, char *type, int vip_prio, int throttle_time, int slot_id),

	TP_ARGS(id, done, type, vip_prio, throttle_time, slot_id),

	TP_STRUCT__entry(
		__field(int, id)
		__field(int, done)
		__string(type, type)
		__field(int, vip_prio)
		__field(int, throttle_time)
		__field(int, slot_id)
	),

	TP_fast_assign(
		__entry->id       = id;
		__entry->done       = done;
		__assign_str(type, type);
		__entry->vip_prio       = vip_prio;
		__entry->throttle_time  = throttle_time;
		__entry->slot_id  = slot_id;
	),

	TP_printk("id=%d done=%d type=%s vip_prio=%d throttle_time=%d, slot_id=%d",
		  __entry->id, __entry->done,	__get_str(type),
		  __entry->vip_prio, __entry->throttle_time, __entry->slot_id)
);

TRACE_EVENT(sched_unset_vip,
	TP_PROTO(int id, int done, char *type, int slot_id),

	TP_ARGS(id, done, type, slot_id),

	TP_STRUCT__entry(
		__field(int, id)
		__field(int, done)
		__string(type, type)
		__field(int, slot_id)
	),

	TP_fast_assign(
		__entry->id       = id;
		__entry->done       = done;
		__assign_str(type, type);
		__entry->slot_id       = slot_id;
	),

	TP_printk("id=%d done=%d type=%s, slot_id=%d",
		  __entry->id, __entry->done,	__get_str(type), __entry->slot_id)
);

TRACE_EVENT(sched_vip_throttled,
	TP_PROTO(pid_t pid, int cpu, int vip_prio, int throttle_time, int exec_time),

	TP_ARGS(pid, cpu, vip_prio, throttle_time, exec_time),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, cpu)
		__field(int, vip_prio)
		__field(int, throttle_time)
		__field(int, exec_time)
	),

	TP_fast_assign(
		__entry->pid       = pid;
		__entry->cpu       = cpu;
		__entry->vip_prio  = vip_prio;
		__entry->throttle_time  = throttle_time;
		__entry->exec_time   = exec_time;
	),

	TP_printk("pid=%d cpu=%d vip_prio=%d throttle_time=%d exec_time=%d",
		  __entry->pid, __entry->cpu, __entry->vip_prio,
		  __entry->throttle_time, __entry->exec_time)
);

#endif /* CONFIG_MTK_SCHED_VIP_TASK */
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sched_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
