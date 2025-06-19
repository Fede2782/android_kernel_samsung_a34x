/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SUGOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SUGOV_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sched_runnable_boost,
	TP_PROTO(bool is_runnable_boost_enable, int boost, unsigned long rq_util_avg,
		unsigned long rq_util_est, unsigned long rq_load, unsigned long cpu_util_next),
	TP_ARGS(is_runnable_boost_enable, boost, rq_util_avg, rq_util_est, rq_load, cpu_util_next),
	TP_STRUCT__entry(
		__field(bool, is_runnable_boost_enable)
		__field(int, boost)
		__field(unsigned long, rq_util_avg)
		__field(unsigned long, rq_util_est)
		__field(unsigned long, rq_load)
		__field(unsigned long, cpu_util_next)
	),
	TP_fast_assign(
		__entry->is_runnable_boost_enable = is_runnable_boost_enable;
		__entry->boost = boost;
		__entry->rq_util_avg = rq_util_avg;
		__entry->rq_util_est = rq_util_est;
		__entry->rq_load = rq_load;
		__entry->cpu_util_next = cpu_util_next;
	),
	TP_printk("is_runnable_boost_enable=%d boost=%d rq_util_avg=%lu rq_util_est=%lu rq_load=%lu cpu_util_next=%lu",
		__entry->is_runnable_boost_enable,
		__entry->boost,
		__entry->rq_util_avg,
		__entry->rq_util_est,
		__entry->rq_load,
		__entry->cpu_util_next
	)
);

TRACE_EVENT(sugov_ext_act_sbb,
	TP_PROTO(int flag, int pid,
		int set, int success, int gear_id,
		int sbb_active_ratio),
	TP_ARGS(flag, pid, set, success, gear_id, sbb_active_ratio),
	TP_STRUCT__entry(
		__field(int, flag)
		__field(int, pid)
		__field(int, set)
		__field(int, success)
		__field(int, gear_id)
		__field(int, sbb_active_ratio)
	),
	TP_fast_assign(
		__entry->flag = flag;
		__entry->pid = pid;
		__entry->set = set;
		__entry->success = success;
		__entry->gear_id = gear_id;
		__entry->sbb_active_ratio = sbb_active_ratio;
	),
	TP_printk(
		"flag=%d pid=%d set=%d success=%d gear_id=%d, sbb_active_ratio=%d",
		__entry->flag,
		__entry->pid,
		__entry->set,
		__entry->success,
		__entry->gear_id,
		__entry->sbb_active_ratio)
);

TRACE_EVENT(sugov_ext_curr_uclamp,
	TP_PROTO(int cpu, int pid,
		int util_ori, int util, int u_min,
		int u_max),
	TP_ARGS(cpu, pid, util_ori, util, u_min, u_max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, pid)
		__field(unsigned int, util_ori)
		__field(unsigned int, util)
		__field(unsigned int, u_min)
		__field(unsigned int, u_max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->util_ori = util_ori;
		__entry->util = util;
		__entry->u_min = u_min;
		__entry->u_max = u_max;
	),
	TP_printk(
		"cpu=%d pid=%d util_ori=%d util_eff=%d u_min=%d, u_max=%d",
		__entry->cpu,
		__entry->pid,
		__entry->util_ori,
		__entry->util,
		__entry->u_min,
		__entry->u_max)
);

TRACE_EVENT(sugov_ext_gear_uclamp,
	TP_PROTO(int cpu, int util_ori,
		int umin, int umax, int util,
		int umax_gear),
	TP_ARGS(cpu, util_ori, umin, umax, util, umax_gear),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, util_ori)
		__field(unsigned int, umin)
		__field(unsigned int, umax)
		__field(unsigned int, util)
		__field(unsigned int, umax_gear)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util_ori = util_ori;
		__entry->umin = umin;
		__entry->umax = umax;
		__entry->util = util;
		__entry->umax_gear = umax_gear;
	),
	TP_printk(
		"cpu=%d util_ori=%d umin=%d umax=%d util_ret=%d, umax_gear=%d",
		__entry->cpu,
		__entry->util_ori,
		__entry->umin,
		__entry->umax,
		__entry->util,
		__entry->umax_gear)
);

TRACE_EVENT(sugov_ext_util,
	TP_PROTO(int cpu, unsigned long util,
		unsigned int min, unsigned int max, int idle),
	TP_ARGS(cpu, util, min, max, idle),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(int, idle)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->max = max;
		__entry->idle = idle;
	),
	TP_printk(
		"cpu=%d util=%lu min=%u max=%u ignore_idle_util=%d",
		__entry->cpu,
		__entry->util,
		__entry->min,
		__entry->max,
		__entry->idle)
);

TRACE_EVENT(sugov_ext_wl,
	TP_PROTO(unsigned int gear_id, unsigned int cpu, int wl_tcm,
		int wl_cpu_curr, int wl_cpu_delay, int wl_cpu_manual, int wl_dsu_curr,
		int wl_dsu_delay,int wl_dsu_manual),
	TP_ARGS(gear_id, cpu, wl_tcm, wl_cpu_curr, wl_cpu_delay, wl_cpu_manual, wl_dsu_curr,
		wl_dsu_delay, wl_dsu_manual),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, cpu)
		__field(int, wl_tcm)
		__field(int, wl_cpu_curr)
		__field(int, wl_cpu_delay)
		__field(int, wl_cpu_manual)
		__field(int, wl_dsu_curr)
		__field(int, wl_dsu_delay)
		__field(int, wl_dsu_manual)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->cpu = cpu;
		__entry->wl_tcm= wl_tcm;
		__entry->wl_cpu_curr = wl_cpu_curr;
		__entry->wl_cpu_delay = wl_cpu_delay;
		__entry->wl_cpu_manual = wl_cpu_manual;
		__entry->wl_dsu_curr = wl_dsu_curr;
		__entry->wl_dsu_delay = wl_dsu_delay;
		__entry->wl_dsu_manual = wl_dsu_manual;
	),
	TP_printk(
		"gear_id=%u cpu=%u wl_tcm=%d wl_cpu_curr=%d wl_cpu_delay=%d wl_cpu_manual=%d wl_dsu_curr=%d wl_dsu_delay=%d wl_dsu_manual=%d",
		__entry->gear_id,
		__entry->cpu,
		__entry->wl_tcm,
		__entry->wl_cpu_curr,
		__entry->wl_cpu_delay,
		__entry->wl_cpu_manual,
		__entry->wl_dsu_curr,
		__entry->wl_dsu_delay,
		__entry->wl_dsu_manual)
);

TRACE_EVENT(sugov_ext_dsu_freq_vote,
	TP_PROTO(unsigned int wl, unsigned int gear_id, bool dsu_idle_ctrl,
		unsigned int cpu_freq, unsigned int dsu_freq_vote, unsigned int freq_thermal),
	TP_ARGS(wl, gear_id, dsu_idle_ctrl, cpu_freq, dsu_freq_vote, freq_thermal),
	TP_STRUCT__entry(
		__field(unsigned int, wl)
		__field(unsigned int, gear_id)
		__field(bool, dsu_idle_ctrl)
		__field(unsigned int, cpu_freq)
		__field(unsigned int, dsu_freq_vote)
		__field(unsigned int, freq_thermal)
	),
	TP_fast_assign(
		__entry->wl = wl;
		__entry->gear_id = gear_id;
		__entry->dsu_idle_ctrl = dsu_idle_ctrl;
		__entry->cpu_freq = cpu_freq;
		__entry->dsu_freq_vote = dsu_freq_vote;
		__entry->freq_thermal = freq_thermal;
	),
	TP_printk(
		"wl=%u gear_id=%u dsu_idle_ctrl=%d cpu_freq=%u dsu_freq_vote=%u freq_thermal=%u",
		__entry->wl,
		__entry->gear_id,
		__entry->dsu_idle_ctrl,
		__entry->cpu_freq,
		__entry->dsu_freq_vote,
		__entry->freq_thermal)
);

TRACE_EVENT(sugov_ext_gear_state,
	TP_PROTO(unsigned int gear_id, unsigned int state),
	TP_ARGS(gear_id, state),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, state)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->state = state;
	),
	TP_printk(
		"gear_id=%u state=%u",
		__entry->gear_id,
		__entry->state)
);

TRACE_EVENT(sugov_ext_sbb,
	TP_PROTO(int cpu, int pid, unsigned int boost,
		unsigned int util, unsigned int util_boost, unsigned int active_ratio,
		unsigned int threshold),
	TP_ARGS(cpu, pid, boost, util, util_boost, active_ratio, threshold),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(unsigned int, boost)
		__field(unsigned int, util)
		__field(unsigned int, util_boost)
		__field(unsigned int, active_ratio)
		__field(unsigned int, threshold)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->boost = boost;
		__entry->util = util;
		__entry->util_boost = util_boost;
		__entry->active_ratio = active_ratio;
		__entry->threshold = threshold;
	),
	TP_printk(
		"cpu=%d pid=%d boost=%d util=%d util_boost=%d active_ratio=%d, threshold=%d",
		__entry->cpu,
		__entry->pid,
		__entry->boost,
		__entry->util,
		__entry->util_boost,
		__entry->active_ratio,
		__entry->threshold)
);

TRACE_EVENT(sugov_ext_adaptive_margin,
	TP_PROTO(unsigned int gear_id, unsigned int margin, unsigned int ratio),
	TP_ARGS(gear_id, margin, ratio),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, margin)
		__field(unsigned int, ratio)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->margin = margin;
		__entry->ratio = ratio;
	),
	TP_printk(
		"gear_id=%u margin=%u ratio=%u",
		__entry->gear_id,
		__entry->margin,
		__entry->ratio)
);

TRACE_EVENT(sugov_ext_tar,
	TP_PROTO(int cpu, unsigned long ret_util,
		unsigned long cpu_util, unsigned long umax, int am),
	TP_ARGS(cpu, ret_util, cpu_util, umax, am),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, ret_util)
		__field(unsigned long, cpu_util)
		__field(unsigned long, umax)
		__field(int, am)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->ret_util = ret_util;
		__entry->cpu_util = cpu_util;
		__entry->umax = umax;
		__entry->am = am;
	),
	TP_printk(
		"cpu=%d ret_util=%ld cpu_util=%ld umax=%ld am=%d",
		__entry->cpu,
		__entry->ret_util,
		__entry->cpu_util,
		__entry->umax,
		__entry->am)
);

TRACE_EVENT(sugov_ext_group_dvfs,
	TP_PROTO(int gearid, unsigned long util, unsigned long pelt_util_with_margin,
		unsigned long flt_util, unsigned long pelt_util, unsigned long pelt_margin,
		unsigned long freq),
	TP_ARGS(gearid, util, pelt_util_with_margin, flt_util, pelt_util, pelt_margin, freq),
	TP_STRUCT__entry(
		__field(int, gearid)
		__field(unsigned long, util)
		__field(unsigned long, pelt_util_with_margin)
		__field(unsigned long, flt_util)
		__field(unsigned long, pelt_util)
		__field(unsigned long, pelt_margin)
		__field(unsigned long, freq)
	),
	TP_fast_assign(
		__entry->gearid = gearid;
		__entry->util = util;
		__entry->pelt_util_with_margin = pelt_util_with_margin;
		__entry->flt_util = flt_util;
		__entry->pelt_util = pelt_util;
		__entry->pelt_margin = pelt_margin;
		__entry->freq = freq;
	),
	TP_printk(
		"gearid=%d ret=%lu pelt_with_margin=%lu tar=%lu pelt=%lu pelt_margin=%lu freq=%lu",
		__entry->gearid,
		__entry->util,
		__entry->pelt_util_with_margin,
		__entry->flt_util,
		__entry->pelt_util,
		__entry->pelt_margin,
		__entry->freq)
);

TRACE_EVENT(sugov_ext_turn_point_margin,
	TP_PROTO(unsigned int cpu, unsigned int orig_util, unsigned int margin_util,
	unsigned int turn_point, unsigned int target_margin, unsigned int target_margin_low),
	TP_ARGS(cpu, orig_util, margin_util, turn_point, target_margin, target_margin_low),
	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, orig_util)
		__field(unsigned int, margin_util)
		__field(unsigned int, turn_point)
		__field(unsigned int, target_margin)
		__field(unsigned int, target_margin_low)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->orig_util = orig_util;
		__entry->margin_util = margin_util;
		__entry->turn_point = turn_point;
		__entry->target_margin = target_margin;
		__entry->target_margin_low = target_margin_low;
	),
	TP_printk(
		"cpu=%u orig_util=%u margin_util=%u turn_point=%d target_margin=%d target_margin_low=%d",
		__entry->cpu,
		__entry->orig_util,
		__entry->margin_util,
		__entry->turn_point,
		__entry->target_margin,
		__entry->target_margin_low)
);

TRACE_EVENT(collab_type_0_ret_function,

	TP_PROTO(unsigned int val, unsigned int val1, unsigned int val2, unsigned int status),

	TP_ARGS(val, val1, val2, status),

	TP_STRUCT__entry(
		__field(unsigned int, val)
		__field(unsigned int, val1)
		__field(unsigned int, val2)
		__field(unsigned int, status)
	),
	TP_fast_assign(
		__entry->val = val;
		__entry->val1 = val1;
		__entry->val2 = val2;
		__entry->status = status;
	),
	TP_printk(
		"val=%d val1=%d val2=%d status=%d",
		__entry->val,
		__entry->val1,
		__entry->val2,
		__entry->status)
);

TRACE_EVENT(sched_pd_opp2cap,
	TP_PROTO(int cpu, int opp, int quant, int wl,
		int val_1, int val_2, int val_r, int val_s, int val_m, bool r_o, int caller),

	TP_ARGS(cpu, opp, quant, wl, val_1, val_2, val_r, val_s, val_m, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_2)
		__field(int, val_r)
		__field(int, val_s)
		__field(int, val_m)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->opp = opp;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_2 = val_2;
		__entry->val_r = val_r;
		__entry->val_s = val_s;
		__entry->val_m = val_m;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d opp=%d quant=%d wl=%d val_1=%d val_2=%d val_r=%d val_s=%d val_m=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->opp,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_2,
		__entry->val_r,
		__entry->val_s,
		__entry->val_m,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_pd_opp2pwr_eff,
	TP_PROTO(int cpu, int opp, int quant, int wl,
		int val_1, int val_2, int val_3, int val_r1, int val_r2, int val_s, bool r_o, int caller),

	TP_ARGS(cpu, opp, quant, wl, val_1, val_2, val_3, val_r1, val_r2, val_s, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_2)
		__field(int, val_3)
		__field(int, val_r1)
		__field(int, val_r2)
		__field(int, val_s)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->opp = opp;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_2 = val_2;
		__entry->val_3 = val_3;
		__entry->val_r1 = val_r1;
		__entry->val_r2 = val_r2;
		__entry->val_s = val_s;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d opp=%d quant=%d wl=%d val_1=%d val_2=%d val_3=%d val_r1=%d val_r2=%d val_s=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->opp,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_2,
		__entry->val_3,
		__entry->val_r1,
		__entry->val_r2,
		__entry->val_s,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_pd_opp2dyn_pwr,
	TP_PROTO(int cpu, int opp, int quant, int wl,
		int val_1, int val_r, int val_s, int val_m, bool r_o, int caller),

	TP_ARGS(cpu, opp, quant, wl, val_1, val_r, val_s, val_m, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_r)
		__field(int, val_s)
		__field(int, val_m)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->opp = opp;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_r = val_r;
		__entry->val_s = val_s;
		__entry->val_m = val_m;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d opp=%d quant=%d wl=%d val_1=%d val_r=%d val_s=%d val_m=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->opp,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_r,
		__entry->val_s,
		__entry->val_m,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_pd_util2opp,
	TP_PROTO(int cpu, int quant, int wl,
		int val_1, int val_2, int val_3, int val_4, int val_r, int val_s, int val_m, bool r_o, int caller),

	TP_ARGS(cpu, quant, wl, val_1, val_2, val_3, val_4, val_r, val_s, val_m, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_2)
		__field(int, val_3)
		__field(int, val_4)
		__field(int, val_r)
		__field(int, val_s)
		__field(int, val_m)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_2 = val_2;
		__entry->val_3 = val_3;
		__entry->val_4 = val_4;
		__entry->val_r = val_r;
		__entry->val_s = val_s;
		__entry->val_m = val_m;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d quant=%d wl=%d val_1=%d val_2=%d val_3=%d val_4=%d val_r=%d val_s=%d val_m=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_2,
		__entry->val_3,
		__entry->val_4,
		__entry->val_r,
		__entry->val_s,
		__entry->val_m,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_update_cpu_capacity,
	TP_PROTO(int cpu, struct rq *rq, int wl, int caller),

	TP_ARGS(cpu, rq, wl, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, wl)
		__field(unsigned long, cap_orig)
		__field(unsigned long, cap_of)
		__field(int, caller)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->wl = wl;
		__entry->cap_orig = rq->cpu_capacity_orig;
		__entry->cap_of = rq->cpu_capacity;
		__entry->caller = caller;
	),

	TP_printk("cpu=%d wl=%d cap_origin=%lu cap_normal=%lu caller=%d",
		__entry->cpu,
		__entry->wl,
		__entry->cap_orig,
		__entry->cap_of,
		__entry->caller)
);

TRACE_EVENT(sugov_ext_limits_changed,
	TP_PROTO(unsigned int cpu, unsigned int cur,
		unsigned int min, unsigned int max),
	TP_ARGS(cpu, cur, min, max),
	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, cur)
		__field(unsigned int, min)
		__field(unsigned int, max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->cur = cur;
		__entry->min = min;
		__entry->max = max;
	),
	TP_printk(
		"cpu=%u cur=%u min=%u max=%u",
		__entry->cpu,
		__entry->cur,
		__entry->min,
		__entry->max)
);

TRACE_EVENT(sugov_ext_util_debug,
	TP_PROTO(int cpu, unsigned long util_cfs,
		unsigned long util_rt, unsigned long util_dl, unsigned long util_irq,
		unsigned long util_before, unsigned long scale_irq, unsigned long bw_dl),
	TP_ARGS(cpu, util_cfs, util_rt, util_dl, util_irq, util_before, scale_irq, bw_dl),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util_cfs)
		__field(unsigned long, util_rt)
		__field(unsigned long, util_dl)
		__field(unsigned long, util_irq)
		__field(unsigned long, util_before)
		__field(unsigned long, scale_irq)
		__field(unsigned long, bw_dl)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util_cfs = util_cfs;
		__entry->util_rt = util_rt;
		__entry->util_dl = util_dl;
		__entry->util_irq = util_irq;
		__entry->util_before = util_before;
		__entry->scale_irq = scale_irq;
		__entry->bw_dl = bw_dl;
	),
	TP_printk(
		"cpu=%d cfs=%lu rt=%lu dl=%lu irq=%lu before=%lu scale_irq=%lu bw_dl=%lu",
		__entry->cpu,
		__entry->util_cfs,
		__entry->util_rt,
		__entry->util_dl,
		__entry->util_irq,
		__entry->util_before,
		__entry->scale_irq,
		__entry->bw_dl)
);
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH sugov
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sugov_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
