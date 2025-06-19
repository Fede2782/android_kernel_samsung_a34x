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
#include <linux/sched/clock.h>

#include "common.h"
#include "eas_plus.h"

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "group.h"
#include "flt_cal.h"
extern const char *task_event_names[];
extern const char *add_task_demand_names[];
extern const char *history_event_names[];
extern const char *cpu_event_names[];
struct rq;
struct flt_task_struct;
struct flt_rq;
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
/*
 * Tracepoint for big task rotation
 */
TRACE_EVENT(sched_big_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid,
		int fin),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid, fin),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, src_pid)
		__field(int, dst_pid)
		__field(int, fin)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->src_pid	= src_pid;
		__entry->dst_pid	= dst_pid;
		__entry->fin		= fin;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d fin=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid,
		__entry->fin)
);
#endif

TRACE_EVENT(sched_leakage,

	TP_PROTO(int cpu, int opp, int buck_opp, unsigned int temp,
		unsigned long cpu_static_pwr, unsigned long static_pwr,
		unsigned long sum_util, unsigned long sum_cap),

	TP_ARGS(cpu, opp, buck_opp, temp, cpu_static_pwr, static_pwr, sum_util, sum_cap),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, buck_opp)
		__field(unsigned int, temp)
		__field(unsigned long, cpu_static_pwr)
		__field(unsigned long, static_pwr)
		__field(unsigned long, sum_util)
		__field(unsigned long, sum_cap)
		),

	TP_fast_assign(
		__entry->cpu       = cpu;
		__entry->opp        = opp;
		__entry->buck_opp   = buck_opp;
		__entry->temp       = temp;
		__entry->cpu_static_pwr = cpu_static_pwr;
		__entry->static_pwr = static_pwr;
		__entry->sum_util = sum_util;
		__entry->sum_cap = sum_cap;
		),

	TP_printk("cpu=%d opp=%d buck_opp=%d temp=%u lkg=%lu sum_lkg=%lu, sum_util=%lu sum_cap=%lu",
		__entry->cpu,
		__entry->opp,
		__entry->buck_opp,
		__entry->temp,
		__entry->cpu_static_pwr,
		__entry->static_pwr,
		__entry->sum_util,
		__entry->sum_cap)
);

TRACE_EVENT(sched_dsu_freq,

	TP_PROTO(int gear_id, int dst_cpu, int dsu_freq_new, int dsu_volt_new,
			int dsu_freq_base, int dsu_volt_base,
			unsigned long cpu_freq, unsigned long dsu_freq, unsigned int dsu_volt),

	TP_ARGS(gear_id, dst_cpu, dsu_freq_new, dsu_volt_new, dsu_freq_base, dsu_volt_base,
			cpu_freq, dsu_freq, dsu_volt),

	TP_STRUCT__entry(
		__field(int, gear_id)
		__field(int, dst_cpu)
		__field(int, dsu_freq_new)
		__field(int, dsu_volt_new)
		__field(int, dsu_freq_base)
		__field(int, dsu_volt_base)
		__field(unsigned long, cpu_freq)
		__field(unsigned long, dsu_freq)
		__field(unsigned int, dsu_volt)
		),

	TP_fast_assign(
		__entry->gear_id    = gear_id;
		__entry->dst_cpu    = dst_cpu;
		__entry->dsu_freq_new   = dsu_freq_new;
		__entry->dsu_volt_new   = dsu_volt_new;
		__entry->dsu_freq_base  = dsu_freq_base;
		__entry->dsu_volt_base  = dsu_volt_base;
		__entry->cpu_freq  = cpu_freq;
		__entry->dsu_freq  = dsu_freq;
		__entry->dsu_volt  = dsu_volt;
		),

	TP_printk("gear_id=%d dst_cpu=%d dsu_freq_new=%d dsu_volt_new=%d dsu_freq_base=%d dsu_volt_base=%d cpu_freq=%lu dsu_freq=%lu dsu_volt=%u",
		__entry->gear_id,
		__entry->dst_cpu,
		__entry->dsu_freq_new,
		__entry->dsu_volt_new,
		__entry->dsu_freq_base,
		__entry->dsu_volt_base,
		__entry->cpu_freq,
		__entry->dsu_freq,
		__entry->dsu_volt)
);

TRACE_EVENT(dsu_pwr_cal,

	TP_PROTO(int dst_cpu, unsigned long task_util, unsigned long total_util,
		unsigned int dsu_bw, unsigned int emi_bw,
		int dsu_temp, unsigned int dsu_freq, unsigned int dsu_volt,
		unsigned int extern_volt,
		unsigned int dsu_dyn_pwr, unsigned int dsu_lkg_pwr,
		unsigned int mcusys_dyn_pwr),

	TP_ARGS(dst_cpu, task_util, total_util, dsu_bw, emi_bw, dsu_temp, dsu_freq, dsu_volt,
		extern_volt, dsu_dyn_pwr, dsu_lkg_pwr, mcusys_dyn_pwr),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(unsigned long, task_util)
		__field(unsigned long, total_util)
		__field(unsigned int, dsu_bw)
		__field(unsigned int, emi_bw)
		__field(int, temp)
		__field(unsigned int, dsu_freq)
		__field(unsigned int, dsu_volt)
		__field(unsigned int, extern_volt)
		__field(unsigned int, dsu_dyn_pwr)
		__field(unsigned int, dsu_lkg_pwr)
		__field(unsigned int, mcusys_dyn_pwr)
	),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->task_util   = task_util;
		__entry->total_util     = total_util;
		__entry->dsu_bw   = dsu_bw;
		__entry->emi_bw   = emi_bw;
		__entry->temp    = dsu_temp;
		__entry->dsu_freq   = dsu_freq;
		__entry->dsu_volt     = dsu_volt;
		__entry->extern_volt   = extern_volt;
		__entry->dsu_dyn_pwr   = dsu_dyn_pwr;
		__entry->dsu_lkg_pwr   = dsu_lkg_pwr;
		__entry->mcusys_dyn_pwr   = mcusys_dyn_pwr;
	),

	TP_printk("dst_cpu=%d task_util=%lu total_util=%lu dsu_bw=%u emi_bw=%u temp=%d dsu_freq=%u dsu_volt=%u extern_volt=%u dsu_dyn_pwr=%u dsu_lkg_pwr=%u mcusys_dyn_pwr=%u",
		__entry->dst_cpu,
		__entry->task_util,
		__entry->total_util,
		__entry->dsu_bw,
		__entry->emi_bw,
		__entry->temp,
		__entry->dsu_freq,
		__entry->dsu_volt,
		__entry->extern_volt,
		__entry->dsu_dyn_pwr,
		__entry->dsu_lkg_pwr,
		__entry->mcusys_dyn_pwr)
);

TRACE_EVENT(sched_em_cpu_energy,

	TP_PROTO(int wl, int idx, unsigned long freq, const char *cost_type, unsigned long cost,
		unsigned long scale_cpu, unsigned long dyn_pwr, unsigned long static_pwr,
		unsigned long pd_volt, unsigned long extern_volt),

	TP_ARGS(wl, idx, freq, cost_type, cost, scale_cpu, dyn_pwr, static_pwr, pd_volt, extern_volt),

	TP_STRUCT__entry(
		__field(int, wl)
		__field(int, idx)
		__field(unsigned long, freq)
		__string(cost_type, cost_type)
		__field(unsigned long, cost)
		__field(unsigned long, scale_cpu)
		__field(unsigned long, dyn_pwr)
		__field(unsigned long, static_pwr)
		__field(unsigned long, pd_volt)
		__field(unsigned long, extern_volt)
		),

	TP_fast_assign(
		__entry->wl        = wl;
		__entry->idx        = idx;
		__entry->freq       = freq;
		__assign_str(cost_type, cost_type);
		__entry->cost       = cost;
		__entry->scale_cpu  = scale_cpu;
		__entry->dyn_pwr    = dyn_pwr;
		__entry->static_pwr = static_pwr;
		__entry->pd_volt        = pd_volt;
		__entry->extern_volt    = extern_volt;
		),

	TP_printk("wl=%d idx=%d freq=%lu %s=%lu scale_cpu=%lu dyn_pwr=%lu static_pwr=%lu pd_volt=%lu extern_volt=%lu",
		__entry->wl,
		__entry->idx,
		__entry->freq,
		__get_str(cost_type),
		__entry->cost,
		__entry->scale_cpu,
		__entry->dyn_pwr,
		__entry->static_pwr,
		__entry->pd_volt,
		__entry->extern_volt)
);

TRACE_EVENT(sched_calc_pwr_eff,

	TP_PROTO(int cpu, unsigned long cpu_util, int opp, int buck_opp, unsigned long cap,
		unsigned long dyn_pwr_eff, unsigned long static_pwr_eff, unsigned long pwr_eff,
		long pd_volt, long extern_volt),

	TP_ARGS(cpu, cpu_util, opp, buck_opp, cap, dyn_pwr_eff, static_pwr_eff, pwr_eff,
			pd_volt, extern_volt),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, cpu_util)
		__field(int, opp)
		__field(int, buck_opp)
		__field(unsigned long, cap)
		__field(unsigned long, dyn_pwr_eff)
		__field(unsigned long, static_pwr_eff)
		__field(unsigned long, pwr_eff)
		__field(long, pd_volt)
		__field(long, extern_volt)
		),

	TP_fast_assign(
		__entry->cpu            = cpu;
		__entry->cpu_util       = cpu_util;
		__entry->opp            = opp;
		__entry->buck_opp       = buck_opp;
		__entry->cap            = cap;
		__entry->dyn_pwr_eff    = dyn_pwr_eff;
		__entry->static_pwr_eff = static_pwr_eff;
		__entry->pwr_eff        = pwr_eff;
		__entry->pd_volt        = pd_volt;
		__entry->extern_volt    = extern_volt;
		),

	TP_printk("cpu=%d cpu_util=%lu opp=%d buck_opp=%d cap=%lu dyn_pwr_eff=%lu static_pwr_eff=%lu pwr_eff=%lu pd_volt=%ld extern_volt=%ld",
		__entry->cpu,
		__entry->cpu_util,
		__entry->opp,
		__entry->buck_opp,
		__entry->cap,
		__entry->dyn_pwr_eff,
		__entry->static_pwr_eff,
		__entry->pwr_eff,
		__entry->pd_volt,
		__entry->extern_volt)
);

TRACE_EVENT(sched_shared_buck_calc_pwr_eff,

	TP_PROTO(int dst_cpu, int gear_id, struct cpumask *pd_mask,
		int wl, unsigned long pwr_eff, int shared_buck_mode,
		long gear_max_util, unsigned long pd_max_util,
		long gear_volt, long pd_volt, long dsu_volt, long extern_volt),

	TP_ARGS(dst_cpu, gear_id, pd_mask, wl, pwr_eff, shared_buck_mode, pd_max_util, gear_max_util,
			gear_volt, pd_volt, dsu_volt, extern_volt),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(int, gear_id)
		__field(long, cpu_mask)
		__field(int, wl)
		__field(unsigned long, pwr_eff)
		__field(int, shared_buck_mode)
		__field(long, gear_max_util)
		__field(unsigned long, pd_max_util)
		__field(long, gear_volt)
		__field(long, pd_volt)
		__field(long, dsu_volt)
		__field(long, extern_volt)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->gear_id    = gear_id;
		__entry->cpu_mask   = pd_mask->bits[0];
		__entry->wl     = wl;
		__entry->pwr_eff     = pwr_eff;
		__entry->shared_buck_mode = shared_buck_mode;
		__entry->gear_max_util   = gear_max_util;
		__entry->pd_max_util   = pd_max_util;
		__entry->gear_volt   = gear_volt;
		__entry->pd_volt   = pd_volt;
		__entry->dsu_volt   = dsu_volt;
		__entry->extern_volt   = extern_volt;
		),

	TP_printk("dst_cpu=%d gear_id=%d mask=0x%lx wl=%d pwr_eff=%lu shared_buck_mode=%d gear_max_util=%ld pd_max_util=%lu gear_volt=%ld pd_volt=%ld dsu_volt=%ld extern_volt=%ld",
		__entry->dst_cpu,
		__entry->gear_id,
		__entry->cpu_mask,
		__entry->wl,
		__entry->pwr_eff,
		__entry->shared_buck_mode,
		__entry->gear_max_util,
		__entry->pd_max_util,
		__entry->gear_volt,
		__entry->pd_volt,
		__entry->dsu_volt,
		__entry->extern_volt)
);

TRACE_EVENT(sched_find_busiest_group,

	TP_PROTO(int src_cpu, int dst_cpu,
		int out_balance, int reason),

	TP_ARGS(src_cpu, dst_cpu, out_balance, reason),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, out_balance)
		__field(int, reason)
		),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->out_balance	= out_balance;
		__entry->reason		= reason;
		),

	TP_printk("src_cpu=%d dst_cpu=%d out_balance=%d reason=0x%x",
		__entry->src_cpu,
		__entry->dst_cpu,
		__entry->out_balance,
		__entry->reason)
);

TRACE_EVENT(sched_cpu_overutilized,

	TP_PROTO(int cpu, struct cpumask *pd_mask,
		unsigned long sum_util, unsigned long sum_cap,
		int overutilized),

	TP_ARGS(cpu, pd_mask, sum_util, sum_cap,
		overutilized),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(long, cpu_mask)
		__field(unsigned long, sum_util)
		__field(unsigned long, sum_cap)
		__field(int, overutilized)
		),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->cpu_mask	= pd_mask->bits[0];
		__entry->sum_util	= sum_util;
		__entry->sum_cap	= sum_cap;
		__entry->overutilized	= overutilized;
		),

	TP_printk("cpu=%d mask=0x%lx sum_util=%lu sum_cap=%lu overutilized=%d",
		__entry->cpu,
		__entry->cpu_mask,
		__entry->sum_util,
		__entry->sum_cap,
		__entry->overutilized)
);

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_frequency_limits,

	TP_PROTO(int cpu_id, int freq_thermal),

	TP_ARGS(cpu_id, freq_thermal),

	TP_STRUCT__entry(
		__field(int,  cpu_id)
		__field(int,  freq_thermal)
		),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->freq_thermal = freq_thermal;
		),

	TP_printk("cpu=%d thermal=%d",
		__entry->cpu_id, __entry->freq_thermal)
);

TRACE_EVENT(sched_queue_task,
	TP_PROTO(int cpu, int pid, int enqueue,
		unsigned long cfs_util,
		unsigned int min, unsigned int max,
		unsigned int task_min, unsigned int task_max,
		unsigned int tsk_mask),
	TP_ARGS(cpu, pid, enqueue, cfs_util, min, max, task_min, task_max, tsk_mask),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(int, enqueue)
		__field(unsigned long, cfs_util)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(unsigned int, task_min)
		__field(unsigned int, task_max)
		__field(unsigned int, tsk_mask)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->enqueue = enqueue;
		__entry->cfs_util = cfs_util;
		__entry->min = min;
		__entry->max = max;
		__entry->task_min = task_min;
		__entry->task_max = task_max;
		__entry->tsk_mask = tsk_mask;
	),
	TP_printk(
		"cpu=%d pid=%d enqueue=%d cfs_util=%lu min=%u max=%u task_min=%u task_max=%u, cpus_ptr=0x%x",
		__entry->cpu,
		__entry->pid,
		__entry->enqueue,
		__entry->cfs_util,
		__entry->min,
		__entry->max,
		__entry->task_min,
		__entry->task_max,
		__entry->tsk_mask)
);

TRACE_EVENT(sched_task_util,
	TP_PROTO(int pid,
		unsigned long util,
		unsigned int util_enqueued),
	TP_ARGS(pid, util, util_enqueued),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned int, util_enqueued)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->util = util;
		__entry->util_enqueued = util_enqueued;
	),
	TP_printk(
		"pid=%d util=%lu util_enqueued=%u",
		__entry->pid,
		__entry->util,
		__entry->util_enqueued)
);

TRACE_EVENT(sched_task_uclamp,
	TP_PROTO(int pid, unsigned long util,
		unsigned int active,
		unsigned int min, unsigned int max,
		unsigned int min_ud, unsigned int min_req,
		unsigned int max_ud, unsigned int max_req),
	TP_ARGS(pid, util, active,
		min, max,
		min_ud, min_req,
		max_ud, max_req),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned int, active)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(unsigned int, min_ud)
		__field(unsigned int, min_req)
		__field(unsigned int, max_ud)
		__field(unsigned int, max_req)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->util = util;
		__entry->active = active;
		__entry->min = min;
		__entry->max = max;
		__entry->min_ud = min_ud;
		__entry->min_req = min_req;
		__entry->max_ud = max_ud;
		__entry->max_req = max_req;
	),
	TP_printk(
		"pid=%d util=%lu active=%u min=%u max=%u min_ud=%u min_req=%u max_ud=%u max_req=%u",
		__entry->pid,
		__entry->util,
		__entry->active,
		__entry->min,
		__entry->max,
		__entry->min_ud,
		__entry->min_req,
		__entry->max_ud,
		__entry->max_req)
);

TRACE_EVENT(sched_domain_flags,
	TP_PROTO(struct task_struct *task, int prev_cpu, int sd_flag, int wake_flags, int target_cpu),
	TP_ARGS(task, prev_cpu, sd_flag, wake_flags, target_cpu),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, prev_cpu)
		__field(int, sd_flag)
		__field(int, wake_flags)
		__field(int, target_cpu)
	),

	TP_fast_assign(
		__entry->pid = task->pid;
		__entry->prev_cpu = prev_cpu;
		__entry->sd_flag = sd_flag;
		__entry->wake_flags = wake_flags;
		__entry->target_cpu = target_cpu;
	),

	TP_printk("pid=%d prev_cpu=%d sd_flag=0x%08x wake_flags=0x%08x target_cpu=%d",
		__entry->pid,
		__entry->prev_cpu,
		__entry->sd_flag,
		__entry->wake_flags,
		__entry->target_cpu)
);

TRACE_EVENT(sched_rq_load,

	TP_PROTO(struct cfs_rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(unsigned int, nr_running)
		__field(unsigned int, h_nr_running)
		__field(unsigned int, idle_nr_running)
		__field(unsigned int, idle_h_nr_running)
		__field(u64, last_update_time)
		__field(u64, load_sum)
		__field(u64, runnable_sum)
		__field(u32, util_sum)
		__field(u32, period_contrib)
		__field(unsigned long, load_avg)
		__field(unsigned long, runnable_avg)
		__field(unsigned long, util_avg)
		__field(unsigned int, enqueued)
	),

	TP_fast_assign(
		__entry->nr_running = rq->nr_running;
		__entry->h_nr_running = rq->h_nr_running;
		__entry->idle_nr_running = rq->idle_nr_running;
		__entry->idle_h_nr_running = rq->idle_h_nr_running;
		__entry->last_update_time = rq->avg.last_update_time;
		__entry->load_sum = rq->avg.load_sum;
		__entry->runnable_sum = rq->avg.load_sum;
		__entry->util_sum = rq->avg.util_sum;
		__entry->period_contrib = rq->avg.period_contrib;
		__entry->load_avg = rq->avg.load_avg;
		__entry->runnable_avg = rq->avg.runnable_avg;
		__entry->util_avg = rq->avg.util_avg;
		__entry->enqueued = rq->avg.util_est;
	),

	TP_printk("nr_running=%u h_nr_running=%u idle_nr_running=%u idle_h_nr_running=%u last_update_time=%llu load_sum=%llu runnable_sum=%llu util_sum=%u period_contrib=%u load_avg=%lu runnable_avg=%lu util_avg=%lu enqueued=%d",
		__entry->nr_running,
		__entry->h_nr_running,
		__entry->idle_nr_running,
		__entry->idle_h_nr_running,
		__entry->last_update_time,
		__entry->load_sum,
		__entry->runnable_sum,
		__entry->util_sum,
		__entry->period_contrib,
		__entry->load_avg,
		__entry->runnable_avg,
		__entry->util_avg,
		__entry->enqueued
	)
);

#ifdef CREATE_TRACE_POINTS
int sched_cgroup_state_rt(struct task_struct *p, int subsys_id)
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

TRACE_EVENT(sched_cpu_util,
	TP_PROTO(struct task_struct *p, int cpu, bool skip_hiIRQ_enable,
		unsigned long min_cap, unsigned long max_cap),
	TP_ARGS(p, cpu, skip_hiIRQ_enable, min_cap, max_cap),
	TP_STRUCT__entry(
		__field(unsigned int,	cpu)
		__field(unsigned int,	nr_running)
		__field(long,		cpu_util)
		__field(long,		cpu_max_util)
		__field(unsigned int,	capacity)
		__field(unsigned int,	capacity_orig)
		__field(unsigned int,	idle_exit_latency)
		__field(int,		online)
		__field(int,		paused)
		__field(unsigned int,	nr_rtg_high_prio_tasks)
		__field(unsigned int,	busy_with_softirqs)
		__field(int,		high_irq_ctrl)
		__field(int,		high_irq_load)
		__field(long,		irqload)
		__field(unsigned int,	min_highirq_load)
		__field(unsigned int,	irq_ratio)
	),
	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->nr_running = cpu_rq(cpu)->nr_running;
		__entry->cpu_util	= mtk_sched_cpu_util(cpu);
		__entry->cpu_max_util	= mtk_sched_max_util(p, cpu, min_cap, max_cap);
		__entry->capacity	= capacity_of(cpu);
		__entry->capacity_orig	= capacity_orig_of(cpu);
		__entry->idle_exit_latency	= mtk_get_idle_exit_latency(cpu, NULL);
		__entry->online			= cpu_online(cpu);
		__entry->paused			= cpu_paused(cpu);
		__entry->nr_rtg_high_prio_tasks = 0;
		__entry->busy_with_softirqs	= cpu_busy_with_softirqs(cpu);
		__entry->high_irq_ctrl	= skip_hiIRQ_enable;
		__entry->high_irq_load	= cpu_high_irqload(cpu);
		__entry->irqload		= cpu_util_irq(cpu_rq(cpu));
		__entry->min_highirq_load	= get_cpu_irqUtil_threshold(cpu);
		__entry->irq_ratio			= get_cpu_irqRatio_threshold(cpu);
	),
	TP_printk("cpu=%d nr_running=%d cpu_util=%ld cpu_max_util=%ld capacity=%u capacity_orig=%u idle_exit_latency=%u online=%u paused=%u nr_rtg_hp=%u busy_with_softirq=%d high_irq_ctrl=%d high_irq_load=%u irqload=%ld min_highirq_load=%u irq_ratio=%u",
		__entry->cpu,
		__entry->nr_running,
		__entry->cpu_util,
		__entry->cpu_max_util,
		__entry->capacity,
		__entry->capacity_orig,
		__entry->idle_exit_latency,
		__entry->online,
		__entry->paused,
		__entry->nr_rtg_high_prio_tasks,
		__entry->busy_with_softirqs,
		__entry->high_irq_ctrl,
		__entry->high_irq_load,
		__entry->irqload,
		__entry->min_highirq_load,
		__entry->irq_ratio)
);

TRACE_EVENT(sched_select_task_rq_rt,
	TP_PROTO(struct task_struct *tsk, int policy,
		int target_cpu, struct rt_energy_aware_output *rt_ea_output,
		struct cpumask *lowest_mask, int sd_flag, bool sync,
		unsigned long task_util_est, unsigned long uclamp_task_util),
	TP_ARGS(tsk, policy, target_cpu, rt_ea_output, lowest_mask,
		sd_flag, sync, task_util_est, uclamp_task_util),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, target_cpu)
		__field(long, lowest_mask)
		__field(int,  rt_aggre_preempt_enable)
		__field(unsigned int,  idle_cpus)
		__field(unsigned int,  cfs_cpus)
		__field(unsigned int,  rt_cpus)
		__field(int, cfs_lowest_cpu)
		__field(int, cfs_lowest_prio)
		__field(int, cfs_lowest_pid)
		__field(int, rt_lowest_cpu)
		__field(int, rt_lowest_prio)
		__field(int, rt_lowest_pid)
		__field(unsigned long, task_util_est)
		__field(unsigned long, uclamp_min)
		__field(unsigned long, uclamp_max)
		__field(unsigned long, uclamp_task_util)
		__field(int, sd_flag)
		__field(bool, sync)
		__field(long, task_mask)
		__field(int, cpuctl_grp_id)
		__field(int, cpuset_grp_id)
		__field(long, act_mask)
	),
	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->policy = policy;
		__entry->target_cpu = target_cpu;
		__entry->lowest_mask = lowest_mask->bits[0];
		__entry->rt_aggre_preempt_enable = rt_ea_output->rt_aggre_preempt_enable;
		__entry->idle_cpus  = rt_ea_output->idle_cpus;
		__entry->cfs_cpus   = rt_ea_output->cfs_cpus;
		__entry->rt_cpus   = rt_ea_output->rt_cpus;
		__entry->cfs_lowest_cpu  = rt_ea_output->cfs_lowest_cpu;
		__entry->cfs_lowest_prio   = rt_ea_output->cfs_lowest_prio;
		__entry->cfs_lowest_pid   = rt_ea_output->cfs_lowest_pid;
		__entry->rt_lowest_cpu  = rt_ea_output->rt_lowest_cpu;
		__entry->rt_lowest_prio   = rt_ea_output->rt_lowest_prio;
		__entry->rt_lowest_pid   = rt_ea_output->rt_lowest_pid;
		__entry->task_util_est = task_util_est;
		__entry->uclamp_min = uclamp_eff_value(tsk, UCLAMP_MIN);
		__entry->uclamp_max = uclamp_eff_value(tsk, UCLAMP_MAX);
		__entry->uclamp_task_util = uclamp_task_util;
		__entry->sd_flag = sd_flag;
		__entry->sync = sync;
		__entry->task_mask = tsk->cpus_ptr->bits[0];
		__entry->cpuctl_grp_id = sched_cgroup_state_rt(tsk, cpu_cgrp_id);
		__entry->cpuset_grp_id = sched_cgroup_state_rt(tsk, cpuset_cgrp_id);
		__entry->act_mask = cpu_active_mask->bits[0];
	),
	TP_printk(
		"pid=%4d policy=0x%08x target=%d lowest_mask=0x%lx rt_preempt_ctrl=%d idle_cpus=0x%x cfs_cpus=0x%x rt_cpus=0x%x cfs_lowest_cpu=%d cfs_lowest_prio=%d cfs_lowest_pid=%d rt_lowest_cpu=%d rt_lowest_prio=%d rt_lowest_pid=%d util_est=%lu uclamp_min=%lu uclamp_max=%lu uclamp=%lu sd_flag=%d sync=%d mask=0x%lx cpuctl=%d cpuset=%d act_mask=0x%lx",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->lowest_mask,
		__entry->rt_aggre_preempt_enable,
		__entry->idle_cpus,
		__entry->cfs_cpus,
		__entry->rt_cpus,
		__entry->cfs_lowest_cpu,
		__entry->cfs_lowest_prio,
		__entry->cfs_lowest_pid,
		__entry->rt_lowest_cpu,
		__entry->rt_lowest_prio,
		__entry->rt_lowest_pid,
		__entry->task_util_est,
		__entry->uclamp_min,
		__entry->uclamp_max,
		__entry->uclamp_task_util,
		__entry->sd_flag,
		__entry->sync,
		__entry->task_mask,
		__entry->cpuctl_grp_id,
		__entry->cpuset_grp_id,
		__entry->act_mask)
);

TRACE_EVENT(sched_aware_energy_rt,
	TP_PROTO(int wl, int target_cpu, unsigned long this_pwr_eff, unsigned long pwr_eff,
			unsigned int task_util),
	TP_ARGS(wl, target_cpu, this_pwr_eff, pwr_eff, task_util),
	TP_STRUCT__entry(
		__field(int, wl)
		__field(int, target_cpu)
		__field(unsigned long, this_pwr_eff)
		__field(unsigned long, pwr_eff)
		__field(unsigned int, task_util)
	),
	TP_fast_assign(
		__entry->wl        = wl;
		__entry->target_cpu	= target_cpu;
		__entry->this_pwr_eff	= this_pwr_eff;
		__entry->pwr_eff	= pwr_eff;
		__entry->task_util	= task_util;
	),
	TP_printk("wl=%d, target=%d this_pwr_eff=%lu pwr_eff=%lu util=%u",
		__entry->wl,
		__entry->target_cpu,
		__entry->this_pwr_eff,
		__entry->pwr_eff,
		__entry->task_util)
);

TRACE_EVENT(sched_next_update_thermal_headroom,
	TP_PROTO(unsigned long now, unsigned long next_update_thermal),
	TP_ARGS(now, next_update_thermal),
	TP_STRUCT__entry(
		__field(unsigned long, now)
		__field(unsigned long, next_update_thermal)
	),
	TP_fast_assign(
		__entry->now = now;
		__entry->next_update_thermal = next_update_thermal;
	),
	TP_printk(
		"now_tick=%lu next_update_thermal=%lu",
		__entry->now,
		__entry->next_update_thermal)
);

TRACE_EVENT(sched_newly_idle_balance_interval,
	TP_PROTO(unsigned int interval_us),
	TP_ARGS(interval_us),
	TP_STRUCT__entry(
		__field(unsigned int, interval_us)
	),
	TP_fast_assign(
		__entry->interval_us = interval_us;
	),
	TP_printk(
		"interval_us=%u",
		__entry->interval_us)
);

TRACE_EVENT(sched_headroom_interval_tick,
	TP_PROTO(unsigned int tick),
	TP_ARGS(tick),
	TP_STRUCT__entry(
		__field(unsigned int, tick)
	),
	TP_fast_assign(
		__entry->tick = tick;
	),
	TP_printk(
		"interval =%u",
		__entry->tick)
);

TRACE_EVENT(sched_post_init_entity_util_avg,

	TP_PROTO(struct task_struct *p, unsigned long ori, unsigned long util_avg, unsigned long weight,
				unsigned int freq, unsigned long desired_cpufreq, int cpu),

	TP_ARGS(p, ori, util_avg, weight, freq, desired_cpufreq, cpu),

	TP_STRUCT__entry(
		__field(int, pid)
		__array(char,		comm, TASK_COMM_LEN)
		__field(unsigned long,	ori)
		__field(unsigned long,	util_avg)
		__field(unsigned long,	weight)
		__field(unsigned int,	freq)
		__field(unsigned long,	desired_cpufreq)
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->pid       = p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->ori	= ori;
		__entry->util_avg	= util_avg;
		__entry->weight	= weight;
		__entry->freq	= freq;
		__entry->desired_cpufreq	= desired_cpufreq;
		__entry->cpu       = cpu;
	),

	TP_printk("pid=%d task=%s ori=%lu to suppressed=%lu lw=%lu max_freq=%u suppressed_freq=%lu cpu=%d",
			__entry->pid, __entry->comm, __entry->ori,
			__entry->util_avg, __entry->weight,
			__entry->freq, __entry->desired_cpufreq, __entry->cpu)

);

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
TRACE_EVENT(sched_pause_cpus,
	TP_PROTO(struct cpumask *req_cpus, struct cpumask *last_cpus,
			u64 start_time, unsigned char pause,
			int err, struct cpumask *pause_cpus),

	TP_ARGS(req_cpus, last_cpus, start_time, pause, err, pause_cpus),

	TP_STRUCT__entry(
		__field(unsigned int, req_cpus)
		__field(unsigned int, last_cpus)
		__field(unsigned int, time)
		__field(unsigned char, pause)
		__field(int, err)
		__field(unsigned int, pause_cpus)
		__field(unsigned int, online_cpus)
		__field(unsigned int, active_cpus)
	),

	TP_fast_assign(
		__entry->req_cpus    = cpumask_bits(req_cpus)[0];
		__entry->last_cpus = cpumask_bits(last_cpus)[0];
		__entry->time        = div64_u64(sched_clock() - start_time, 1000);
		__entry->pause	     = pause;
		__entry->err         = err;
		__entry->pause_cpus    = cpumask_bits(pause_cpus)[0];
		__entry->online_cpus    = cpumask_bits(cpu_online_mask)[0];
		__entry->active_cpus    = cpumask_bits(cpu_active_mask)[0];
	),

	TP_printk("req=0x%x cpus=0x%x time=%u us paused=%d, err=%d, pause=0x%x, online=0x%x, active=0x%x",
		  __entry->req_cpus, __entry->last_cpus, __entry->time, __entry->pause,
		  __entry->err, __entry->pause_cpus, __entry->online_cpus, __entry->active_cpus)
);

TRACE_EVENT(sched_set_cpus_allowed,
	TP_PROTO(struct task_struct *p, unsigned int *dest_cpu,
		struct cpumask *new_mask, struct cpumask *valid_mask,
		struct cpumask *pause_cpus),

	TP_ARGS(p, dest_cpu, new_mask, valid_mask, pause_cpus),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned int, dest_cpu)
		__field(bool, kthread)
		__field(unsigned int, new_mask)
		__field(unsigned int, valid_mask)
		__field(unsigned int, pause_cpus)
	),

	TP_fast_assign(
		__entry->pid = p->pid;
		__entry->dest_cpu = *dest_cpu;
		__entry->kthread = p->flags & PF_KTHREAD;
		__entry->new_mask = cpumask_bits(new_mask)[0];
		__entry->valid_mask = cpumask_bits(valid_mask)[0];
		__entry->pause_cpus = cpumask_bits(pause_cpus)[0];
	),

	TP_printk("p=%d, dest_cpu=%d, k=%d, new_mask=0x%x, valid=0x%x, pause=0x%x",
		  __entry->pid, __entry->dest_cpu, __entry->kthread,
		  __entry->new_mask, __entry->valid_mask,
		  __entry->pause_cpus)
);

TRACE_EVENT(sched_find_lowest_rq,
	TP_PROTO(struct task_struct *tsk, int policy, int target_cpu,
		struct cpumask *avail_lowest_mask, struct cpumask *lowest_mask,
		const struct cpumask *active_mask),

	TP_ARGS(tsk, policy, target_cpu,
		avail_lowest_mask, lowest_mask, active_mask),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, target_cpu)
		__field(unsigned int, avail_lowest_mask)
		__field(unsigned int, lowest_mask)
		__field(unsigned int, active_mask)
	),

	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->policy = policy;
		__entry->target_cpu = target_cpu;
		__entry->avail_lowest_mask = cpumask_bits(avail_lowest_mask)[0];
		__entry->lowest_mask = cpumask_bits(lowest_mask)[0];
		__entry->active_mask = cpumask_bits(active_mask)[0];
	),

	TP_printk(
		"pid=%4d policy=0x%08x target=%d avail_lowest_mask=0x%x lowest_mask=0x%x, active_mask:0x%x",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->avail_lowest_mask,
		__entry->lowest_mask,
		__entry->active_mask)
);
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
TRACE_EVENT(sched_flt_set_cpu,

	TP_PROTO(int cpu, int util),

	TP_ARGS(cpu, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
	),

	TP_printk("cpu=%d util=%d",
		__entry->cpu, __entry->util)
);

TRACE_EVENT(sched_flt_get_cpu,

	TP_PROTO(int cpu, int util),

	TP_ARGS(cpu, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
	),

	TP_printk("cpu=%d util=%d",
		__entry->cpu, __entry->util)
);

TRACE_EVENT(sched_flt_get_cpu_group,

	TP_PROTO(int cpu, int grp_id, int util),

	TP_ARGS(cpu, grp_id, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		grp_id)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->grp_id		= grp_id;
		__entry->util		= util;
	),

	TP_printk("cpu=%d gp=%d util=%d",
		__entry->cpu, __entry->grp_id, __entry->util)
);

TRACE_EVENT(sched_get_pelt_group_util,

	TP_PROTO(int cpu, unsigned long long delta,
			unsigned long gp0_util, unsigned long gp1_util,
			unsigned long gp2_util, unsigned long gp3_util),

	TP_ARGS(cpu, delta, gp0_util, gp1_util, gp2_util, gp3_util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned long long,	delta)
		__field(unsigned long,	gp0_util)
		__field(unsigned long,	gp1_util)
		__field(unsigned long,	gp2_util)
		__field(unsigned long,	gp3_util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->delta		= delta;
		__entry->gp0_util		= gp0_util;
		__entry->gp1_util		= gp1_util;
		__entry->gp2_util		= gp2_util;
		__entry->gp3_util		= gp3_util;
	),

	TP_printk("cpu=%d= delta=%llu= gp_util[0]=%lu gp_util[1]=%lu gp_util[2] =%lu gp_util[3]=%lu",
		__entry->cpu, __entry->delta,
		__entry->gp0_util, __entry->gp1_util,
		__entry->gp2_util, __entry->gp3_util)
);
TRACE_EVENT(sched_gather_pelt_group_util,

	TP_PROTO(struct task_struct *p, unsigned long tsk_util, int grp_id, int cpu),

	TP_ARGS(p, tsk_util, grp_id, cpu),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned long,	tsk_util)
		__field(int,		grp_id)
		__field(int,		cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid	= p->pid;
		__entry->tsk_util	= tsk_util;
		__entry->grp_id	= grp_id;
		__entry->cpu	= cpu;
	),

	TP_printk("comm=%s pid=%d util=%lu grp_id=%d cpu=%d",
			__entry->comm, __entry->pid,
			__entry->tsk_util, __entry->grp_id,
			__entry->cpu)

);

TRACE_EVENT(sched_task_to_grp,

	TP_PROTO(struct task_struct *p, int grp_id, int ret, int type),

	TP_ARGS(p, grp_id, ret, type),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		grp_id)
		__field(int,		ret)
		__field(int,		type)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid	= p->pid;
		__entry->grp_id	= grp_id;
		__entry->ret	= ret;
		__entry->type	= type;
	),

	TP_printk("comm=%s pid=%d grp_id=%d ret=%d type=%d",
			__entry->comm, __entry->pid,
			__entry->grp_id, __entry->ret,
			__entry->type)

);

TRACE_EVENT(sched_cgrp_to_fltgrp,

	TP_PROTO(int cgrp_id, int grp_id, const char *caller0),

	TP_ARGS(cgrp_id, grp_id, caller0),

	TP_STRUCT__entry(
		__field(int,	cgrp_id)
		__field(int,	grp_id)
		__string(caller0,	caller0)
		),

	TP_fast_assign(
		__entry->cgrp_id	= cgrp_id;
		__entry->grp_id	= grp_id;
		__assign_str(caller0, caller0);
		),

	TP_printk("cgrp_id[%d] to flt grp[%d] caller =%s",
		__entry->cgrp_id, __entry->grp_id, __get_str(caller0)
		)
);

TRACE_EVENT(sched_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p, u32 runtime, int samples,
		enum task_event evt, struct flt_rq *fsrq, struct flt_task_struct *fts,
		enum history_event hisevt),

	TP_ARGS(rq, p, runtime, samples, evt, fsrq, fts, hisevt),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(unsigned long,		util_avg)
		__field(unsigned int,		enqueued)
		__field(unsigned int,		demand)
		__field(u32,			util_demand)
		__field(unsigned int,		runtime)
		__field(int,			samples)
		__field(enum task_event,	evt)
		__field(enum history_event,	hisevt)
		__field(u32,			hist0)
		__field(u32,			hist1)
		__field(u32,			hist2)
		__field(u32,			hist3)
		__field(u32,			hist4)
		__field(u32,			util_sum_hist0)
		__field(u32,			util_sum_hist1)
		__field(u32,			util_sum_hist2)
		__field(u32,			util_sum_hist3)
		__field(u32,			util_sum_hist4)
		__field(u32,			util_avg_history0)
		__field(u32,			util_avg_history1)
		__field(u32,			util_avg_history2)
		__field(u32,			util_avg_history3)
		__field(u32,			util_avg_history4)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util_avg		= p->se.avg.util_avg;
		__entry->enqueued	= p->se.avg.util_est;
		__entry->demand		= fts->demand;
		__entry->util_demand	= fts->util_demand;
		__entry->runtime		= runtime;
		__entry->samples		= samples;
		__entry->evt		= evt;
		__entry->hisevt		= hisevt;
		__entry->hist0	= fts->sum_history[0];
		__entry->hist1	= fts->sum_history[1];
		__entry->hist2	= fts->sum_history[2];
		__entry->hist3	= fts->sum_history[3];
		__entry->hist4	= fts->sum_history[4];
		__entry->util_sum_hist0	= fts->util_sum_history[0];
		__entry->util_sum_hist1	= fts->util_sum_history[1];
		__entry->util_sum_hist2	= fts->util_sum_history[2];
		__entry->util_sum_hist3	= fts->util_sum_history[3];
		__entry->util_sum_hist4	= fts->util_sum_history[4];
		__entry->util_avg_history0	= fts->util_avg_history[0];
		__entry->util_avg_history1	= fts->util_avg_history[1];
		__entry->util_avg_history2	= fts->util_avg_history[2];
		__entry->util_avg_history3	= fts->util_avg_history[3];
		__entry->util_avg_history4	= fts->util_avg_history[4];
	),

	TP_printk("pid=%d name=%s: runtime=%u samples=%d event=%s hisevt=%s demand=%u util_demand=%u (hist[0]=%u hist[1]=%u hist[2]=%u hist[3]=%u hist[4]=%u) (util_sum_hist[0]=%u util_sum_hist[1]=%u util_sum_hist[2]=%u util_sum_hist[3]=%u util_sum_hist[4]=%u) (util_avg_history[0]=%u util_avg_history[1]=%u util_avg_history[2]=%u util_avg_history[3]=%u util_avg_history[4]=%u) pelt(util=%lu util_enqueued=%u)",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples,
		task_event_names[__entry->evt], history_event_names[__entry->hisevt],
		__entry->demand, __entry->util_demand,
		__entry->hist0, __entry->hist1,
		__entry->hist2, __entry->hist3,
		__entry->hist4,
		__entry->util_sum_hist0, __entry->util_sum_hist1,
		__entry->util_sum_hist2, __entry->util_sum_hist3,
		__entry->util_sum_hist4,
		__entry->util_avg_history0, __entry->util_avg_history1,
		__entry->util_avg_history2, __entry->util_avg_history3,
		__entry->util_avg_history4,
		__entry->util_avg,
		__entry->enqueued)
);

TRACE_EVENT(sched_update_task_ravg,

	TP_PROTO(struct rq *rq, struct task_struct *p, u64 wallclock,
		enum task_event evt, struct flt_rq *fsrq,
		struct flt_task_struct *fts, int group_id, u64 irqtime),

	TP_ARGS(rq, p, wallclock, evt, fsrq, fts, group_id, irqtime),

	TP_STRUCT__entry(
		__field(pid_t,		pid)
		__array(char,		comm, TASK_COMM_LEN)
		__field(enum task_event,	evt)
		__field(u64,		irqtime)
		__field(int,		group_id)
		__field(u64,		wallclock)
		__field(u64,		mark_start)
		__field(u64,		window_start)
		__field(int,		cpu)
		__field(u64,		prev_runnable_sum)
		__field(u64,		curr_runnable_sum)
	),

	TP_fast_assign(
		__entry->pid			= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->evt			= evt;
		__entry->irqtime			= irqtime;
		__entry->group_id			= group_id;
		__entry->wallclock		= wallclock;
		__entry->mark_start		= fts->mark_start;
		__entry->window_start		= fsrq->window_start;
		__entry->cpu			= rq->cpu;
		__entry->prev_runnable_sum	= fsrq->prev_runnable_sum;
		__entry->curr_runnable_sum	= fsrq->curr_runnable_sum;
	),

	TP_printk("pid=%d name =%s event=%s flt_groupid=%d irqtime=%llu wc=%llu ms=%llu ws = %llu cpu=%d prev=%llu curr=%llu",
		__entry->pid, __entry->comm, task_event_names[__entry->evt],
		__entry->group_id, __entry->irqtime,
		__entry->wallclock, __entry->mark_start, __entry->window_start,
		__entry->cpu, __entry->prev_runnable_sum, __entry->curr_runnable_sum)
);

TRACE_EVENT(sched_add_to_task_demand,

	TP_PROTO(struct rq *rq, struct task_struct *p,
		struct flt_task_struct *fts, u64 delta,
		unsigned long cie, unsigned long fie,
		u64 util_sum, enum win_event evt),

	TP_ARGS(rq, p, fts, delta, cie, fie, util_sum, evt),

	TP_STRUCT__entry(
		__field(pid_t,			pid)
		__array(char,			comm, TASK_COMM_LEN)
		__field(enum win_event,		evt)
		__field(u64,			delta)
		__field(u64,			util_sum)
		__field(unsigned long,		cie)
		__field(unsigned long,		fie)
	),

	TP_fast_assign(
		__entry->pid		= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->evt		= evt;
		__entry->delta		= delta;
		__entry->util_sum		= util_sum;
		__entry->cie		= cie;
		__entry->fie		= fie;
	),

	TP_printk("pid=%d name=%s: event=%s delta = %llu util_sum = %llu CIE = %lu FIE = %lu",
		__entry->pid, __entry->comm,
		add_task_demand_names[__entry->evt], __entry->delta,
		__entry->util_sum, __entry->cie, __entry->fie)
);

TRACE_EVENT(sched_update_cpu_busy_time,

	TP_PROTO(struct rq *rq, struct task_struct *p,
		struct flt_task_struct *fts, enum cpu_update_event evt,
		u64 prev_delta, u64 curr_delta),

	TP_ARGS(rq, p, fts, evt, prev_delta, curr_delta),

	TP_STRUCT__entry(
		__field(int,			cpu)
		__field(pid_t,			pid)
		__array(char,			comm, TASK_COMM_LEN)
		__field(enum cpu_update_event,	evt)
		__field(u64,			prev_delta)
		__field(u64,			curr_delta)

	),

	TP_fast_assign(
		__entry->cpu		= rq->cpu;
		__entry->pid		= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->evt		= evt;
		__entry->prev_delta	= prev_delta;
		__entry->curr_delta	= curr_delta;
	),

	TP_printk("cpu=%d pid=%d name=%s event=%s prev_delta = %llu curr_delta = %llu",
		__entry->cpu, __entry->pid, __entry->comm,
		cpu_event_names[__entry->evt], __entry->prev_delta, __entry->curr_delta)
);

TRACE_EVENT(sched_update_cpu_history,

	TP_PROTO(struct rq *rq, struct flt_rq *fsrq, u32 samples, u64 runtime),

	TP_ARGS(rq, fsrq, samples, runtime),

	TP_STRUCT__entry(
		__field(int,			cpu)
		__field(u32,			samples)
		__field(u64,			runtime)
		__field(u32,			sum_hist0)
		__field(u32,			sum_hist1)
		__field(u32,			sum_hist2)
		__field(u32,			sum_hist3)
		__field(u32,			sum_hist4)
		__field(u32,			util_hist0)
		__field(u32,			util_hist1)
		__field(u32,			util_hist2)
		__field(u32,			util_hist3)
		__field(u32,			util_hist4)
		__field(u32,			group_sum_hist0)
		__field(u32,			group_sum_hist1)
		__field(u32,			group_sum_hist2)
		__field(u32,			group_sum_hist3)
		__field(u32,			group_sum_hist4)
		__field(u32,			group_sum_hist5)
		__field(u32,			group_sum_hist6)
		__field(u32,			group_sum_hist7)
		__field(u32,			group_sum_hist8)
		__field(u32,			group_sum_hist9)
		__field(u32,			group_sum_hist10)
		__field(u32,			group_sum_hist11)
		__field(u32,			group_sum_hist12)
		__field(u32,			group_sum_hist13)
		__field(u32,			group_sum_hist14)
		__field(u32,			group_sum_hist15)
		__field(u32,			group_sum_hist16)
		__field(u32,			group_sum_hist17)
		__field(u32,			group_sum_hist18)
		__field(u32,			group_sum_hist19)
		__field(u32,			grp_util_his0)
		__field(u32,			grp_util_his1)
		__field(u32,			grp_util_his2)
		__field(u32,			grp_util_his3)
		__field(u32,			grp_util_his4)
		__field(u32,			grp_util_his5)
		__field(u32,			grp_util_his6)
		__field(u32,			grp_util_his7)
		__field(u32,			grp_util_his8)
		__field(u32,			grp_util_his9)
		__field(u32,			grp_util_his10)
		__field(u32,			grp_util_his11)
		__field(u32,			grp_util_his12)
		__field(u32,			grp_util_his13)
		__field(u32,			grp_util_his14)
		__field(u32,			grp_util_his15)
		__field(u32,			grp_util_his16)
		__field(u32,			grp_util_his17)
		__field(u32,			grp_util_his18)
		__field(u32,			grp_util_his19)
		__field(u32,			grp_util_act_his0)
		__field(u32,			grp_util_act_his1)
		__field(u32,			grp_util_act_his2)
		__field(u32,			grp_util_act_his3)
		__field(u32,			grp_util_act_his4)
		__field(u32,			grp_util_act_his5)
		__field(u32,			grp_util_act_his6)
		__field(u32,			grp_util_act_his7)
		__field(u32,			grp_util_act_his8)
		__field(u32,			grp_util_act_his9)
		__field(u32,			grp_util_act_his10)
		__field(u32,			grp_util_act_his11)
		__field(u32,			grp_util_act_his12)
		__field(u32,			grp_util_act_his13)
		__field(u32,			grp_util_act_his14)
		__field(u32,			grp_util_act_his15)
		__field(u32,			grp_util_act_his16)
		__field(u32,			grp_util_act_his17)
		__field(u32,			grp_util_act_his18)
		__field(u32,			grp_util_act_his19)
	),

	TP_fast_assign(
		__entry->cpu		= rq->cpu;
		__entry->samples		= samples;
		__entry->runtime		= runtime;
		__entry->sum_hist0	= fsrq->sum_history[0];
		__entry->sum_hist1	= fsrq->sum_history[1];
		__entry->sum_hist2	= fsrq->sum_history[2];
		__entry->sum_hist3	= fsrq->sum_history[3];
		__entry->sum_hist4	= fsrq->sum_history[4];
		__entry->util_hist0	= fsrq->util_history[0];
		__entry->util_hist1	= fsrq->util_history[1];
		__entry->util_hist2	= fsrq->util_history[2];
		__entry->util_hist3	= fsrq->util_history[3];
		__entry->util_hist4	= fsrq->util_history[4];
		__entry->group_sum_hist0	= fsrq->group_sum_history[0][0];
		__entry->group_sum_hist1	= fsrq->group_sum_history[0][1];
		__entry->group_sum_hist2	= fsrq->group_sum_history[0][2];
		__entry->group_sum_hist3	= fsrq->group_sum_history[0][3];
		__entry->group_sum_hist4	= fsrq->group_sum_history[1][0];
		__entry->group_sum_hist5	= fsrq->group_sum_history[1][1];
		__entry->group_sum_hist6	= fsrq->group_sum_history[1][2];
		__entry->group_sum_hist7	= fsrq->group_sum_history[1][3];
		__entry->group_sum_hist8	= fsrq->group_sum_history[2][0];
		__entry->group_sum_hist9	= fsrq->group_sum_history[2][1];
		__entry->group_sum_hist10	= fsrq->group_sum_history[2][2];
		__entry->group_sum_hist11	= fsrq->group_sum_history[2][3];
		__entry->group_sum_hist12	= fsrq->group_sum_history[3][0];
		__entry->group_sum_hist13	= fsrq->group_sum_history[3][1];
		__entry->group_sum_hist14	= fsrq->group_sum_history[3][2];
		__entry->group_sum_hist15	= fsrq->group_sum_history[3][3];
		__entry->group_sum_hist16	= fsrq->group_sum_history[4][0];
		__entry->group_sum_hist17	= fsrq->group_sum_history[4][1];
		__entry->group_sum_hist18	= fsrq->group_sum_history[4][2];
		__entry->group_sum_hist19	= fsrq->group_sum_history[4][3];
		__entry->grp_util_his0	= fsrq->group_util_history[0][0];
		__entry->grp_util_his1	= fsrq->group_util_history[0][1];
		__entry->grp_util_his2	= fsrq->group_util_history[0][2];
		__entry->grp_util_his3	= fsrq->group_util_history[0][3];
		__entry->grp_util_his4	= fsrq->group_util_history[1][0];
		__entry->grp_util_his5	= fsrq->group_util_history[1][1];
		__entry->grp_util_his6	= fsrq->group_util_history[1][2];
		__entry->grp_util_his7	= fsrq->group_util_history[1][3];
		__entry->grp_util_his8	= fsrq->group_util_history[2][0];
		__entry->grp_util_his9	= fsrq->group_util_history[2][1];
		__entry->grp_util_his10	= fsrq->group_util_history[2][2];
		__entry->grp_util_his11	= fsrq->group_util_history[2][3];
		__entry->grp_util_his12	= fsrq->group_util_history[3][0];
		__entry->grp_util_his13	= fsrq->group_util_history[3][1];
		__entry->grp_util_his14	= fsrq->group_util_history[3][2];
		__entry->grp_util_his15	= fsrq->group_util_history[3][3];
		__entry->grp_util_his16	= fsrq->group_util_history[4][0];
		__entry->grp_util_his17	= fsrq->group_util_history[4][1];
		__entry->grp_util_his18	= fsrq->group_util_history[4][2];
		__entry->grp_util_his19	= fsrq->group_util_history[4][3];
		__entry->grp_util_act_his0	= fsrq->group_util_active_history[0][0];
		__entry->grp_util_act_his1	= fsrq->group_util_active_history[0][1];
		__entry->grp_util_act_his2	= fsrq->group_util_active_history[0][2];
		__entry->grp_util_act_his3	= fsrq->group_util_active_history[0][3];
		__entry->grp_util_act_his4	= fsrq->group_util_active_history[0][4];
		__entry->grp_util_act_his5	= fsrq->group_util_active_history[1][0];
		__entry->grp_util_act_his6	= fsrq->group_util_active_history[1][1];
		__entry->grp_util_act_his7	= fsrq->group_util_active_history[1][2];
		__entry->grp_util_act_his8	= fsrq->group_util_active_history[1][3];
		__entry->grp_util_act_his9	= fsrq->group_util_active_history[1][4];
		__entry->grp_util_act_his10	= fsrq->group_util_active_history[2][0];
		__entry->grp_util_act_his11	= fsrq->group_util_active_history[2][1];
		__entry->grp_util_act_his12	= fsrq->group_util_active_history[2][2];
		__entry->grp_util_act_his13	= fsrq->group_util_active_history[2][3];
		__entry->grp_util_act_his14	= fsrq->group_util_active_history[2][4];
		__entry->grp_util_act_his15	= fsrq->group_util_active_history[3][0];
		__entry->grp_util_act_his16	= fsrq->group_util_active_history[3][1];
		__entry->grp_util_act_his17	= fsrq->group_util_active_history[3][2];
		__entry->grp_util_act_his18	= fsrq->group_util_active_history[3][3];
		__entry->grp_util_act_his19	= fsrq->group_util_active_history[3][4];
	),

	TP_printk("cpu=%d, sa=%u rt=%llu rqs[0]=%u rqs[1]=%u rqs[2]=%u rqs[3]=%u rqs[4]=%u  rqu[0]=%u rqu[1]=%u rqu[2]=%u rqu[3]=%u rqu[4]=%u grp1s[0]=%u grp1s[1]=%u grp1s[2]=%u grp1s[3]=%u grp1s[4]=%u grp2s[0]=%u grp2s[1]=%u grp2s[2]=%u grp2s[3]=%u grp2s[4]=%u grp3s[0]=%u grp3s[1]=%u grp3s[2]=%u grp3s[3]=%u grp3s[4]=%u grp4s[0]=%u grp4s[1]=%u grp4s[2]=%u grp4s[3]=%u grp4s[4]=%u  grp1u[0]=%u grp1u[1]=%u grp1u[2]=%u grp1u[3]=%u grp1u[4]=%u grp2u[0]=%u grp2u[1]=%u grp2u[2]=%u grp2u[3]=%u grp2u[4]=%u grp3u[0]=%u grp3u[1]=%u grp3u[2]=%u grp3u[3]=%u grp3u[4]=%u grp4u[0]=%u grp4u[1]=%u grp4u[2]=%u grp4u[3]=%u grp4u[4]=%u grp1a[0]=%u grp1a[1]=%u grp1a[2]=%u grp1a[3]=%u grp1a[4]=%u grp2a[0]=%u grp2a[1]=%u grp2a[2]=%u grp2a[3]=%u grp2a[4]=%u grp3a[0]=%u grp3a[1]=%u grp3a[2]=%u grp3a[3]=%u grp3a[4]=%u grp4a[0]=%u grp4a[1]=%u grp4a[2]=%u grp4a[3]=%u grp4a[4]=%u",
		__entry->cpu, __entry->samples,  __entry->runtime,
		__entry->sum_hist0, __entry->sum_hist1,
		__entry->sum_hist2, __entry->sum_hist3,
		__entry->sum_hist4,
		__entry->util_hist0, __entry->util_hist1,
		__entry->util_hist2, __entry->util_hist3,
		__entry->util_hist4,
		__entry->group_sum_hist0, __entry->group_sum_hist4,
		__entry->group_sum_hist8, __entry->group_sum_hist12,
		__entry->group_sum_hist16,
		__entry->group_sum_hist1, __entry->group_sum_hist5,
		__entry->group_sum_hist9, __entry->group_sum_hist13,
		__entry->group_sum_hist17,
		__entry->group_sum_hist2, __entry->group_sum_hist6,
		__entry->group_sum_hist10, __entry->group_sum_hist14,
		__entry->group_sum_hist18,
		__entry->group_sum_hist3, __entry->group_sum_hist7,
		__entry->group_sum_hist11, __entry->group_sum_hist15,
		__entry->group_sum_hist19,
		__entry->grp_util_his0, __entry->grp_util_his4,
		__entry->grp_util_his8, __entry->grp_util_his12,
		__entry->grp_util_his16,
		__entry->grp_util_his1, __entry->grp_util_his5,
		__entry->grp_util_his9, __entry->grp_util_his13,
		__entry->grp_util_his17,
		__entry->grp_util_his2, __entry->grp_util_his6,
		__entry->grp_util_his10, __entry->grp_util_his14,
		__entry->grp_util_his18,
		__entry->grp_util_his3, __entry->grp_util_his7,
		__entry->grp_util_his11, __entry->grp_util_his15,
		__entry->grp_util_his19,
		__entry->grp_util_act_his0, __entry->grp_util_act_his1,
		__entry->grp_util_act_his2, __entry->grp_util_act_his3,
		__entry->grp_util_act_his4,
		__entry->grp_util_act_his5, __entry->grp_util_act_his6,
		__entry->grp_util_act_his7, __entry->grp_util_act_his8,
		__entry->grp_util_act_his9,
		__entry->grp_util_act_his10, __entry->grp_util_act_his11,
		__entry->grp_util_act_his12, __entry->grp_util_act_his13,
		__entry->grp_util_act_his14,
		__entry->grp_util_act_his15, __entry->grp_util_act_his16,
		__entry->grp_util_act_his17, __entry->grp_util_act_his18,
		__entry->grp_util_act_his19)
);

TRACE_EVENT(sched_rollover_task_window,

	TP_PROTO(struct task_struct *p, struct flt_task_struct *fts, bool full_window),

	TP_ARGS(p, fts, full_window),

	TP_STRUCT__entry(
		__field(pid_t,			pid)
		__array(char,			comm, TASK_COMM_LEN)
		__field(bool,			full_window)
		__field(u32,			prev_window)
		__field(u32,			curr_window)
	),

	TP_fast_assign(
		__entry->pid			= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->full_window		= full_window;
		__entry->prev_window		= fts->prev_window;
		__entry->curr_window		= fts->curr_window;
	),

	TP_printk("pid=%d name=%s  full_window=%d prev_window=%u curr_window=%u",
		__entry->pid, __entry->comm, __entry->full_window,
		__entry->prev_window, __entry->curr_window)
);

TRACE_EVENT(sched_enq_deq_task,

	TP_PROTO(struct task_struct *p, bool enqueue,
			unsigned int cpus_allowed, struct flt_rq *fsrq),

	TP_ARGS(p, enqueue, cpus_allowed, fsrq),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		prio)
		__field(int,		cpu)
		__field(int,		gp0_cnt)
		__field(int,		gp1_cnt)
		__field(int,		gp2_cnt)
		__field(int,		gp3_cnt)
		__field(bool,		enqueue)
		__field(unsigned int,	nr_running)
		__field(unsigned int,	rt_nr_running)
		__field(unsigned int,	cpus_allowed)
		__field(u64,		flt_cpu_util)
		__field(unsigned long,	cfs_util)
		__field(unsigned long,	rt_util)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->cpu		= task_cpu(p);
		__entry->enqueue		= enqueue;
		__entry->nr_running	= task_rq(p)->nr_running;
		__entry->rt_nr_running	= task_rq(p)->rt.rt_nr_running;
		__entry->cpus_allowed	= cpus_allowed;
		__entry->gp0_cnt		= fsrq->group_nr_running[0];
		__entry->gp1_cnt		= fsrq->group_nr_running[1];
		__entry->gp2_cnt		= fsrq->group_nr_running[2];
		__entry->gp3_cnt		= fsrq->group_nr_running[3];
		__entry->cfs_util		= mtk_cpu_util_cfs(task_rq(p)->cpu);
		__entry->rt_util		= cpu_util_rt(task_rq(p));
	),

	TP_printk("cpu=%d name=%s comm=%s pid=%d prio=%d nr_running=%u rt_nr_running=%u affine=%x cfs_util=%lu rt_util=%lu gp0=[%d] gp1=[%d] gp2=[%d] gp3=[%d]",
		__entry->cpu,
		__entry->enqueue ? "enqueue" : "dequeue",
		__entry->comm, __entry->pid,
		__entry->prio, __entry->nr_running,
		__entry->rt_nr_running, __entry->cpus_allowed,
		__entry->cfs_util, __entry->rt_util,
		__entry->gp0_cnt, __entry->gp1_cnt,
		__entry->gp2_cnt, __entry->gp3_cnt)
);

TRACE_EVENT(sched_get_group_running_task_cnt,

	TP_PROTO(int group_id, int win_idx, int cnt),

	TP_ARGS(group_id, win_idx, cnt),

	TP_STRUCT__entry(
		__field(int,		group_id)
		__field(int,		win_idx)
		__field(int,		cnt)
	),

	TP_fast_assign(
		__entry->group_id		= group_id;
		__entry->win_idx		= win_idx;
		__entry->cnt		= cnt;
	),

	TP_printk("grp_id=%d wc=%d  res=%d",
		__entry->group_id, __entry->win_idx, __entry->cnt)
);

TRACE_EVENT(sched_get_group_util,

	TP_PROTO(int group_id, int window_count, int weight_policy, int res, int type, int hint),

	TP_ARGS(group_id, window_count, weight_policy, res, type, hint),

	TP_STRUCT__entry(
		__field(int,		type)
		__field(int,		group_id)
		__field(int,		window_count)
		__field(int,		weight_policy)
		__field(int,		res)
		__field(int,		hint)
	),

	TP_fast_assign(
		__entry->type		= type;
		__entry->group_id		= group_id;
		__entry->window_count	= window_count;
		__entry->weight_policy	= weight_policy;
		__entry->res		= res;
		__entry->hint		= hint;
	),

	TP_printk("type=%d grp_id=%d wc=%d wp=%d res=%d hint %d",
		__entry->type, __entry->group_id, __entry->window_count,
		__entry->weight_policy, __entry->res, __entry->hint)
);

TRACE_EVENT(sched_get_cpu_group_util,

	TP_PROTO(int cpu, int group_id, int util),

	TP_ARGS(cpu, group_id, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		group_id)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->group_id		= group_id;
		__entry->util		= util;
	),

	TP_printk("cpu =%d gp=%d gp_util=%d",
		__entry->cpu, __entry->group_id, __entry->util)
);

TRACE_EVENT(sugov_ext_tar_cal,

	TP_PROTO(int cpu, int pcpu_tar_u, int *pcpu_pgrp_tar_u,
		int *group_nr_running, int pcpu_o_u),

	TP_ARGS(cpu, pcpu_tar_u, pcpu_pgrp_tar_u, group_nr_running, pcpu_o_u),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		pcpu_tar_u)
		__field(int,		gt0)
		__field(int,		gt1)
		__field(int,		gt2)
		__field(int,		gt3)
		__field(int,		nr0)
		__field(int,		nr1)
		__field(int,		nr2)
		__field(int,		nr3)
		__field(int,		ot)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->pcpu_tar_u	= pcpu_tar_u;
		__entry->gt0		= pcpu_pgrp_tar_u[0];
		__entry->gt1		= pcpu_pgrp_tar_u[1];
		__entry->gt2		= pcpu_pgrp_tar_u[2];
		__entry->gt3		= pcpu_pgrp_tar_u[3];
		__entry->nr0		= group_nr_running[0];
		__entry->nr1		= group_nr_running[1];
		__entry->nr2		= group_nr_running[2];
		__entry->nr3		= group_nr_running[3];
		__entry->ot			= pcpu_o_u;
	),

	TP_printk("cpu=%d tar=%d gt[0]=%d gt[1]=%d gt[2]=%d gt[3]=%d nr[0]=%d nr[1]=%d nr[2]=%d nr[3]=%d ot=%d",
		__entry->cpu,
		__entry->pcpu_tar_u,
		__entry->gt0,
		__entry->gt1,
		__entry->gt2,
		__entry->gt3,
		__entry->nr0,
		__entry->nr1,
		__entry->nr2,
		__entry->nr3,
		__entry->ot)
);

TRACE_EVENT(sugov_ext_pcpu_pgrp_u_rto_marg,

	TP_PROTO(int cpu, int *pcpu_pgrp_u, int *pcpu_pgrp_adpt_rto, int *pcpu_pgrp_marg,
	int pcpu_o_u, int *pcpu_pgrp_wetin, int *pcpu_pgrp_tar_u,
	int cpu_tar_util, int *grp_margin, int weighting, int *pgrp_parallel_u, int grp_high_freq),

	TP_ARGS(cpu, pcpu_pgrp_u, pcpu_pgrp_adpt_rto, pcpu_pgrp_marg, pcpu_o_u,
	pcpu_pgrp_wetin, pcpu_pgrp_tar_u, cpu_tar_util, grp_margin,
	weighting, pgrp_parallel_u, grp_high_freq),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		gu0)
		__field(int,		gu1)
		__field(int,		gu2)
		__field(int,		gu3)
		__field(int,		rto0)
		__field(int,		rto1)
		__field(int,		rto2)
		__field(int,		rto3)
		__field(int,		marg0)
		__field(int,		marg1)
		__field(int,		marg2)
		__field(int,		marg3)
		__field(int,		ot)
		__field(int,		pcpu_pgrp_wetin0)
		__field(int,		pcpu_pgrp_wetin1)
		__field(int,		pcpu_pgrp_wetin2)
		__field(int,		pcpu_pgrp_wetin3)
		__field(int,		pcpu_pgrp_tar_u0)
		__field(int,		pcpu_pgrp_tar_u1)
		__field(int,		pcpu_pgrp_tar_u2)
		__field(int,		pcpu_pgrp_tar_u3)
		__field(int,		cpu_tar_util)
		__field(int,		grp_margin0)
		__field(int,		grp_margin1)
		__field(int,		grp_margin2)
		__field(int,		grp_margin3)
		__field(int,		weighting)
		__field(int,		pgrp_parallel_u0)
		__field(int,		pgrp_parallel_u1)
		__field(int,		pgrp_parallel_u2)
		__field(int,		pgrp_parallel_u3)
		__field(int,		grp_high_freq)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->gu0		= pcpu_pgrp_u[0];
		__entry->gu1		= pcpu_pgrp_u[1];
		__entry->gu2		= pcpu_pgrp_u[2];
		__entry->gu3		= pcpu_pgrp_u[3];
		__entry->rto0		= pcpu_pgrp_adpt_rto[0];
		__entry->rto1		= pcpu_pgrp_adpt_rto[1];
		__entry->rto2		= pcpu_pgrp_adpt_rto[2];
		__entry->rto3		= pcpu_pgrp_adpt_rto[3];
		__entry->marg0		= pcpu_pgrp_marg[0];
		__entry->marg1		= pcpu_pgrp_marg[1];
		__entry->marg2		= pcpu_pgrp_marg[2];
		__entry->marg3		= pcpu_pgrp_marg[3];
		__entry->ot			= pcpu_o_u;
		__entry->pcpu_pgrp_wetin0		= pcpu_pgrp_wetin[0];
		__entry->pcpu_pgrp_wetin1		= pcpu_pgrp_wetin[1];
		__entry->pcpu_pgrp_wetin2		= pcpu_pgrp_wetin[2];
		__entry->pcpu_pgrp_wetin3		= pcpu_pgrp_wetin[3];
		__entry->pcpu_pgrp_tar_u0		= pcpu_pgrp_tar_u[0];
		__entry->pcpu_pgrp_tar_u1		= pcpu_pgrp_tar_u[1];
		__entry->pcpu_pgrp_tar_u2		= pcpu_pgrp_tar_u[2];
		__entry->pcpu_pgrp_tar_u3		= pcpu_pgrp_tar_u[3];
		__entry->cpu_tar_util		= cpu_tar_util;
		__entry->grp_margin0		= grp_margin[0];
		__entry->grp_margin1		= grp_margin[1];
		__entry->grp_margin2		= grp_margin[2];
		__entry->grp_margin3		= grp_margin[3];
		__entry->weighting		= weighting;
		__entry->pgrp_parallel_u0		= pgrp_parallel_u[0];
		__entry->pgrp_parallel_u1		= pgrp_parallel_u[1];
		__entry->pgrp_parallel_u2		= pgrp_parallel_u[2];
		__entry->pgrp_parallel_u3		= pgrp_parallel_u[3];
		__entry->grp_high_freq		= grp_high_freq;
	),

	TP_printk("cpu=%d gu[0]=%d gu[1]=%d gu[2]=%d gu[3]=%d rto[0]=%d rto[1]=%d rto[2]=%d rto[3]=%d marg[0]=%d marg[1]=%d marg[2]=%d marg[3]=%d ot=%d wt[0]=%d wt[1]=%d wt[2]=%d wt[3]=%d tar_u[0]=%d tar_u[1]=%d tar_u[2]=%d tar_u[3]=%d cpu_tar_util=%d grp_m[0]=%d grp_m[1]=%d grp_m[2]=%d grp_m[3]=%d wt=%d para_u[0]=%d para_u[1]=%d para_u[2]=%d para_u[3]=%d grp_high_freq=%d",
		__entry->cpu,
		__entry->gu0,
		__entry->gu1,
		__entry->gu2,
		__entry->gu3,
		__entry->rto0,
		__entry->rto1,
		__entry->rto2,
		__entry->rto3,
		__entry->marg0,
		__entry->marg1,
		__entry->marg2,
		__entry->marg3,
		__entry->ot,
		__entry->pcpu_pgrp_wetin0,
		__entry->pcpu_pgrp_wetin1,
		__entry->pcpu_pgrp_wetin2,
		__entry->pcpu_pgrp_wetin3,
		__entry->pcpu_pgrp_tar_u0,
		__entry->pcpu_pgrp_tar_u1,
		__entry->pcpu_pgrp_tar_u2,
		__entry->pcpu_pgrp_tar_u3,
		__entry->cpu_tar_util,
		__entry->grp_margin0,
		__entry->grp_margin1,
		__entry->grp_margin2,
		__entry->grp_margin3,
		__entry->weighting,
		__entry->pgrp_parallel_u0,
		__entry->pgrp_parallel_u1,
		__entry->pgrp_parallel_u2,
		__entry->pgrp_parallel_u3,
		__entry->grp_high_freq)
);

TRACE_EVENT(sugov_ext_pger_pgrp_u,

	TP_PROTO(int gear_id, int cpu, int *pger_pgrp_u, int *converge_thr, int *margin_for_min_opp),

	TP_ARGS(gear_id, cpu, pger_pgrp_u, converge_thr, margin_for_min_opp),

	TP_STRUCT__entry(
		__field(int,		gear_id)
		__field(int,		cpu)
		__field(int,		gu0)
		__field(int,		gu1)
		__field(int,		gu2)
		__field(int,		gu3)
		__field(int,		converge_thr0)
		__field(int,		converge_thr1)
		__field(int,		converge_thr2)
		__field(int,		converge_thr3)
		__field(int,		margin_for_min_opp0)
		__field(int,		margin_for_min_opp1)
		__field(int,		margin_for_min_opp2)
		__field(int,		margin_for_min_opp3)
	),

	TP_fast_assign(
		__entry->gear_id		= gear_id;
		__entry->cpu		= cpu;
		__entry->gu0		= pger_pgrp_u[0];
		__entry->gu1		= pger_pgrp_u[1];
		__entry->gu2		= pger_pgrp_u[2];
		__entry->gu3		= pger_pgrp_u[3];
		__entry->converge_thr0		= converge_thr[0];
		__entry->converge_thr1		= converge_thr[1];
		__entry->converge_thr2		= converge_thr[2];
		__entry->converge_thr3		= converge_thr[3];
		__entry->margin_for_min_opp0		= margin_for_min_opp[0];
		__entry->margin_for_min_opp1		= margin_for_min_opp[1];
		__entry->margin_for_min_opp2		= margin_for_min_opp[2];
		__entry->margin_for_min_opp3		= margin_for_min_opp[3];
	),

	TP_printk("gear_id=%d cpu=%d gu[0]=%d gu[1]=%d gu[2]=%d gu[3]=%d ct[0]=%d ct[1]=%d ct[2]=%d ct[3]=%d margin_min_opp[0]=%d margin_min_opp[1]=%d margin_min_opp[2]=%d margin_min_opp[3]=%d",
		__entry->gear_id,
		__entry->cpu,
		__entry->gu0,
		__entry->gu1,
		__entry->gu2,
		__entry->gu3,
		__entry->converge_thr0,
		__entry->converge_thr1,
		__entry->converge_thr2,
		__entry->converge_thr3,
		__entry->margin_for_min_opp0,
		__entry->margin_for_min_opp1,
		__entry->margin_for_min_opp2,
		__entry->margin_for_min_opp3)
);

TRACE_EVENT(sugov_ext_pgrp_hint,

	TP_PROTO(int *pgrp_hint),

	TP_ARGS(pgrp_hint),

	TP_STRUCT__entry(
		__field(int,		ht0)
		__field(int,		ht1)
		__field(int,		ht2)
		__field(int,		ht3)
	),

	TP_fast_assign(
		__entry->ht0		= pgrp_hint[0];
		__entry->ht1		= pgrp_hint[1];
		__entry->ht2		= pgrp_hint[2];
		__entry->ht3		= pgrp_hint[3];
	),

	TP_printk("gpht[0]=%d gpht[1]=%d gpht[2]=%d gpht[3]=%d",
		__entry->ht0,
		__entry->ht1,
		__entry->ht2,
		__entry->ht3)
);

TRACE_EVENT(sugov_ext_ta_ctrl,

	TP_PROTO(int val, int force_ctrl, int refcnt, int top_grp_aware),

	TP_ARGS(val, force_ctrl, refcnt, top_grp_aware),

	TP_STRUCT__entry(
		__field(int,		val)
		__field(int,		force_ctrl)
		__field(int,		refcnt)
		__field(int,		top_grp_aware)
	),

	TP_fast_assign(
		__entry->val		= val;
		__entry->force_ctrl	= force_ctrl;
		__entry->refcnt		= refcnt;
		__entry->top_grp_aware	= top_grp_aware;
	),

	TP_printk("val=%d force_ctrl=%d refcnt=%d top_grp_aware=%d",
		__entry->val,
		__entry->force_ctrl,
		__entry->refcnt,
		__entry->top_grp_aware)
);

TRACE_EVENT(sugov_ext_ta_ctrl_caller,

	TP_PROTO(const char *caller0),

	TP_ARGS(caller0),

	TP_STRUCT__entry(
		__string(caller0, caller0)
		),

	TP_fast_assign(
		__assign_str(caller0, caller0);
		),

	TP_printk("caller =%s",
		__get_str(caller0)
		)
);

TRACE_EVENT(sched_flt_get_o_util,

	TP_PROTO(int cpu, int cpu_r, int grp_idx, u32 util_ratio, int flt_util, u32 grp_r, u32 total),

	TP_ARGS(cpu, cpu_r, grp_idx, util_ratio, flt_util, grp_r, total),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		cpu_r)
		__field(int,		grp_idx)
		__field(u32,		util_ratio)
		__field(int,		flt_util)
		__field(u32,		grp_r)
		__field(u32,		total)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->cpu_r		= cpu_r;
		__entry->grp_idx		= grp_idx;
		__entry->util_ratio	= util_ratio;
		__entry->flt_util		= flt_util;
		__entry->grp_r		= grp_r;
		__entry->total		= total;
	),

	TP_printk("cpu=%d util=%d grp=%d ratio=%u gputil=%d gp_r=%u total=%u",
		__entry->cpu, __entry->cpu_r,
		__entry->grp_idx, __entry->util_ratio,
		__entry->flt_util, __entry->grp_r,
		__entry->total)
);

TRACE_EVENT(sched_set_preferred_cluster,

	TP_PROTO(int wl, int grp_id, int util,
			int threshold, bool gear_hint),

	TP_ARGS(wl, grp_id, util, threshold, gear_hint),

	TP_STRUCT__entry(
		__field(int,		wl)
		__field(int,		grp_id)
		__field(int,		util)
		__field(int,		threshold)
		__field(bool,		gear_hint)
	),

	TP_fast_assign(
		__entry->wl		= wl;
		__entry->grp_id		= grp_id;
		__entry->util		= util;
		__entry->threshold	= threshold;
		__entry->gear_hint	= gear_hint;
	),

	TP_printk("wl=%d grp_id=%d  util=%d threshold=%d gear_hint=%d",
		__entry->wl, __entry->grp_id, __entry->util,
		__entry->threshold, __entry->gear_hint)
);
#endif

TRACE_EVENT(sched_adpf_get_value,

	TP_PROTO(unsigned int cmd, unsigned int sid, unsigned int tgid, unsigned int uid, int threadIds_size,
	long targetDurationNanos, int eas_adpf_vip_ctrl),

	TP_ARGS(cmd, sid, tgid, uid, threadIds_size, targetDurationNanos, eas_adpf_vip_ctrl),

	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, sid)
		__field(unsigned int, tgid)
		__field(unsigned int, uid)
		__field(int, threadIds_size)
		__field(long, targetDurationNanos)
		__field(int, eas_adpf_vip_ctrl)
	),

	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->sid = sid;
		__entry->tgid = tgid;
		__entry->uid = uid;
		__entry->threadIds_size = threadIds_size;
		__entry->targetDurationNanos = targetDurationNanos;
		__entry->eas_adpf_vip_ctrl = eas_adpf_vip_ctrl;
	),

	TP_printk("cmd = %d sid = %d tgid = %d uid = %d threadIds_size = %d targetDurationNanos = %ld eas_adpf_vip_ctrl= %d\n",
		  __entry->cmd, __entry->sid, __entry->tgid, __entry->uid, __entry->threadIds_size,
		  __entry->targetDurationNanos, __entry->eas_adpf_vip_ctrl)
);

/* clamp_id: 0 = UCLAMP_MIN, 1 = UCLAMP_MAX */
TRACE_EVENT(sched_set_uclamp,
	TP_PROTO(int pid, int task_cpu, int task_on_rq_queued, unsigned int clamp_id, int value),
	TP_ARGS(pid, task_cpu, task_on_rq_queued, clamp_id, value),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, task_cpu)
		__field(int, task_on_rq_queued)
		__field(unsigned int, clamp_id)
		__field(int, value)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->task_cpu = task_cpu;
		__entry->task_on_rq_queued = task_on_rq_queued;
		__entry->clamp_id = clamp_id;
		__entry->value = value;
	),

	TP_printk("pid=%d task_cpu=%d task_on_rq_queued=%d clamp_id=%d value=%d",
		__entry->pid,
		__entry->task_cpu,
		__entry->task_on_rq_queued,
		__entry->clamp_id,
		__entry->value)
);

TRACE_EVENT(sched_mtk_update_misfit_status,
	TP_PROTO(int cpu, int fit, int pid, unsigned long util, unsigned long uclamp_min, unsigned long uclamp_max,
	unsigned long capacity_of, unsigned long misfit_task_load),

	TP_ARGS(cpu, fit, pid, util, uclamp_min, uclamp_max, capacity_of, misfit_task_load),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, fit)
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned long, uclamp_min)
		__field(unsigned long, uclamp_max)
		__field(unsigned long, capacity_of)
		__field(unsigned long, misfit_task_load)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->fit = fit;
		__entry->pid = pid;
		__entry->util = util;
		__entry->uclamp_min = uclamp_min;
		__entry->uclamp_max = uclamp_max;
		__entry->capacity_of = capacity_of;
		__entry->misfit_task_load = misfit_task_load;
	),

	TP_printk("cpu=%d fit=%d pid=%d util=%lu uclamp_min=%lu uclamp_max=%lu capacity_of=%lu misfit_task_load=%lu",
		__entry->cpu,
		__entry->fit,
		__entry->pid,
		__entry->util,
		__entry->uclamp_min,
		__entry->uclamp_max,
		__entry->capacity_of,
		__entry->misfit_task_load)
);

TRACE_EVENT(sched_stat_vdeadline,
	TP_PROTO(struct task_struct *prev, struct task_struct *next),
	TP_ARGS(prev, next),
	TP_STRUCT__entry(
		__field(int, prev_pid)
		__field(u64, prev_deadline)
		__field(u64, prev_slice)
		__field(int, next_pid)
		__field(u64, next_deadline)
		__field(u64, next_slice)
		__field(bool, expected)
	),

	TP_fast_assign(
		__entry->prev_pid = prev->pid;
		__entry->prev_deadline = (prev->se).deadline;
		__entry->prev_slice = (prev->se).slice;
		__entry->next_pid = next->pid;
		__entry->next_deadline = (next->se).deadline;
		__entry->next_slice = (next->se).slice;
		__entry->expected = (prev->se).deadline > (next->se).deadline;
	),

	TP_printk("prev_pid=%d prev_deadline=%llu prev_slice=%llu next_pid=%d next_deadline=%llu next_slice=%llu expected=%d",
		__entry->prev_pid,
		__entry->prev_deadline,
		__entry->prev_slice,
		__entry->next_pid,
		__entry->next_deadline,
		__entry->next_slice,
		__entry->expected
	)
);

TRACE_EVENT(sched_update_thermal_pressure_capacity,
	TP_PROTO(int cpu, unsigned long th_pressure, unsigned long max_capacity,
		unsigned long thermal_max_capacity, int wl),

	TP_ARGS(cpu, th_pressure, max_capacity, thermal_max_capacity, wl),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, th_pressure)
		__field(unsigned long, max_capacity)
		__field(unsigned long, thermal_max_capacity)
		__field(int, wl)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->th_pressure = th_pressure;
		__entry->max_capacity = max_capacity;
		__entry->thermal_max_capacity = thermal_max_capacity;
		__entry->wl = wl;
	),

	TP_printk("cpu=%d th_pressure=%lu max_capacity=%lu thermal_max_capacity=%lu wl=%d",
		__entry->cpu,
		__entry->th_pressure,
		__entry->max_capacity,
		__entry->thermal_max_capacity,
		__entry->wl)
);

TRACE_EVENT(sched_skip_user,
	TP_PROTO(struct task_struct *p, bool skip_user, struct cpumask *user_cpus_ptr,
		struct cpumask *kernel_allowed_mask, const struct cpumask *new_mask),

	TP_ARGS(p, skip_user, user_cpus_ptr, kernel_allowed_mask, new_mask),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(bool, skip_user)
		__field(unsigned int, user_mask)
		__field(unsigned int, kernel_allowed_mask)
		__field(unsigned int, new_mask)
	),

	TP_fast_assign(
		__entry->pid = p->pid;
		__entry->skip_user = skip_user;
		__entry->user_mask = cpumask_bits(user_cpus_ptr)[0];
		__entry->kernel_allowed_mask = cpumask_bits(kernel_allowed_mask)[0];
		__entry->new_mask = cpumask_bits(new_mask)[0];
	),

	TP_printk("pid=%d, skip_user=%d, user_mask=0x%x, kernel_allowed_mask=0x%x, new_mask=0x%x",
		__entry->pid,
		__entry->skip_user,
		__entry->user_mask,
		__entry->kernel_allowed_mask,
		__entry->new_mask
	)
);

#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH eas
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE eas_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
