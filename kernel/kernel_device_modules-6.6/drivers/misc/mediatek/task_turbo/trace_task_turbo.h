/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM task_turbo

#if !defined(_TRACE_TASK_TURBO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TASK_TURBO_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <task_turbo.h>

TRACE_EVENT(binder_vip_set,
	TP_PROTO(pid_t a_pid, pid_t b_pid, int a_vip_prio, unsigned int a_throttle,
		int b_vip_prio, unsigned int b_throttle),
	TP_ARGS(a_pid, b_pid, a_vip_prio, a_throttle, b_vip_prio, b_throttle),

	TP_STRUCT__entry(
		__field(pid_t, a_pid)
		__field(pid_t, b_pid)
		__field(int, a_vip_prio)
		__field(unsigned int, a_throttle)
		__field(int, b_vip_prio)
		__field(unsigned int, b_throttle)
	),
	TP_fast_assign(
		__entry->a_pid = a_pid;
		__entry->b_pid = b_pid;
		__entry->a_vip_prio = a_vip_prio;
		__entry->a_throttle = a_throttle;
		__entry->b_vip_prio = b_vip_prio;
		__entry->b_throttle = b_throttle;
	),
	TP_printk("%d -> %d: (%d, %d) -> (%d, %d)",
		__entry->a_pid,
		__entry->b_pid,
		__entry->a_vip_prio,
		__entry->a_throttle,
		__entry->b_vip_prio,
		__entry->b_throttle)
);

TRACE_EVENT(binder_vip_restore,
	TP_PROTO(pid_t b_pid, int restore_vip_prio),
	TP_ARGS(b_pid, restore_vip_prio),

	TP_STRUCT__entry(
		__field(pid_t, b_pid)
		__field(int, restore_vip_prio)
	),
	TP_fast_assign(
		__entry->b_pid = b_pid;
		__entry->restore_vip_prio = restore_vip_prio;
	),
	TP_printk("%d: restore to: %d",
		__entry->b_pid,
		__entry->restore_vip_prio)
);

TRACE_EVENT(turbo_feats_set,
	TP_PROTO(unsigned int feats),
	TP_ARGS(feats),

	TP_STRUCT__entry(
		__field(unsigned int, feats)
	),
	TP_fast_assign(
		__entry->feats = feats;
	),
	TP_printk("feats=%d", __entry->feats)
);

TRACE_EVENT(turbo_set,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, prio)
		__field(unsigned int, turbo)
	),
	TP_fast_assign(
		__entry->pid = p->pid;
		__entry->prio = p->prio;
		__entry->turbo = get_task_turbo_t(p)->turbo;
	),
	TP_printk("pid=%d prio=%d turbo=%d",
		__entry->pid,
		__entry->prio,
		__entry->turbo)
);

TRACE_EVENT(turbo_inherit_failed,
	TP_PROTO(int turbo, unsigned int inherit_types,
		unsigned short inherit_cnt, int return_line),
	TP_ARGS(turbo, inherit_types, inherit_cnt, return_line),

	TP_STRUCT__entry(
		__field(int, turbo)
		__field(unsigned int, inherit_types)
		__field(unsigned short, inherit_cnt)
		__field(int, return_line)
	),
	TP_fast_assign(
		__entry->turbo = turbo;
		__entry->inherit_types = inherit_types;
		__entry->inherit_cnt = inherit_cnt;
		__entry->return_line = return_line;
	),
	TP_printk("turbo=%d inherit_types=%d inherit_cnt=%d line=%d",
			__entry->turbo,
			__entry->inherit_types,
			__entry->inherit_cnt,
			__entry->return_line)
);

TRACE_EVENT(turbo_inherit_start,
	TP_PROTO(struct task_struct *from, struct task_struct *to),
	TP_ARGS(from, to),

	TP_STRUCT__entry(
		__field(pid_t, fpid)
		__field(int, fprio)
		__field(unsigned int, f_turbo)
		__field(unsigned int, f_inherit_types)
		__field(int, tprio)
		__field(unsigned int, t_turbo)
		__field(unsigned int, t_inherit_types)
	),
	TP_fast_assign(
		__entry->fpid		 = from->pid;
		__entry->fprio		 = from->prio;
		__entry->f_turbo	 = get_task_turbo_t(from)->turbo;
		__entry->f_inherit_types = atomic_read(&get_task_turbo_t(from)->inherit_types);
		__entry->tprio		 = to->prio;
		__entry->t_turbo	 = get_task_turbo_t(to)->turbo;
		__entry->t_inherit_types = atomic_read(&get_task_turbo_t(to)->inherit_types);
	),
	TP_printk("pid=%d prio=%d turbo=%d inh=%d => prio=%d turbo=%d inh=%d",
		__entry->fpid,
		__entry->fprio,
		__entry->f_turbo,
		__entry->f_inherit_types,
		__entry->tprio,
		__entry->t_turbo,
		__entry->t_inherit_types)
);

TRACE_EVENT(turbo_inherit_end,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, prio)
		__field(unsigned int, turbo)
		__field(unsigned int, inherit_types)
	),
	TP_fast_assign(
		__entry->pid		 = p->pid;
		__entry->prio		 = p->prio;
		__entry->turbo		 = get_task_turbo_t(p)->turbo;
		__entry->inherit_types	 = atomic_read(&get_task_turbo_t(p)->inherit_types);
	),
	TP_printk("pid=%d prio=%d turbo=%d inherit_types=%d",
		__entry->pid,
		__entry->prio,
		__entry->turbo,
		__entry->inherit_types)
);

TRACE_EVENT(sched_turbo_nice_set,
	TP_PROTO(struct task_struct *task, int old_prio, int new_prio),
	TP_ARGS(task, old_prio, new_prio),
	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(int, pid)
		__field(int, old_prio)
		__field(int, new_prio)
	),

	TP_fast_assign(
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->pid	  = task->pid;
		__entry->old_prio = old_prio;
		__entry->new_prio = new_prio;
	),

	TP_printk("comm=%s pid=%d old_prio=%d new_prio=%d",
		__entry->comm,
		__entry->pid,
		__entry->old_prio,
		__entry->new_prio)
);

TRACE_EVENT(sched_set_user_nice,
	TP_PROTO(struct task_struct *task, int prio, int is_turbo),
	TP_ARGS(task, prio, is_turbo),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, prio)
		__field(int, is_turbo)
	),

	TP_fast_assign(
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->pid	  = task->pid;
		__entry->prio	  = prio;
		__entry->is_turbo = is_turbo;
	),

	TP_printk("comm=%s pid=%d prio=%d is_turbo=%d",
		__entry->comm, __entry->pid, __entry->prio, __entry->is_turbo)
)

TRACE_EVENT(select_turbo_cpu,
	TP_PROTO(int target_cpu, struct task_struct *task, int max_spare_cap, int max_spare_cpu),
	TP_ARGS(target_cpu, task, max_spare_cap, max_spare_cpu),
	TP_STRUCT__entry(
		__field(int, target_cpu)
		__field(int, pid)
		__field(int, max_spare_cap)
		__field(int, max_spare_cpu)
	),

	TP_fast_assign(
		__entry->target_cpu = target_cpu;
		__entry->pid = task->pid;
		__entry->max_spare_cap = max_spare_cap;
		__entry->max_spare_cpu = max_spare_cpu;
	),

	TP_printk("target_cpu=%d pid=%d max_spare_cap=%d max_spare_cpu=%d",
		__entry->target_cpu,
		__entry->pid,
		__entry->max_spare_cap,
		__entry->max_spare_cpu)
);

TRACE_EVENT(turbo_futex_plist_add,
	TP_PROTO(int prev_pid, bool prev_turbo, int next_pid, bool next_turbo),
	TP_ARGS(prev_pid, prev_turbo, next_pid, next_turbo),
	TP_STRUCT__entry(
		__field(int, prev_pid)
		__field(bool, prev_turbo)
		__field(int, next_pid)
		__field(bool, next_turbo)
	),

	TP_fast_assign(
		__entry->prev_pid = prev_pid;
		__entry->prev_turbo = prev_turbo;
		__entry->next_pid = next_pid;
		__entry->next_turbo = next_turbo;
	),

	TP_printk("prev_pid=%d prev_turbo=%d next_pid=%d next_turbo=%d",
		__entry->prev_pid,
		__entry->prev_turbo,
		__entry->next_pid,
		__entry->next_turbo)
);

TRACE_EVENT(turbo_prepare_prio_fork,
	TP_PROTO(struct task_turbo_t *turbo_data, struct task_struct *p),
	TP_ARGS(turbo_data, p),
	TP_STRUCT__entry(
		__field(int, parent_prio)
		__field(int, child_prio)
	),

	TP_fast_assign(
		__entry->parent_prio = turbo_data->nice_backup + 120;
		__entry->child_prio = p->static_prio;
	),

	TP_printk("parent_prio=%d child_prio=%d",
		__entry->parent_prio,
		__entry->child_prio)
);

TRACE_EVENT(turbo_rtmutex_prepare_setprio,
	TP_PROTO(struct task_turbo_t *turbo_data, struct task_struct *p),
	TP_ARGS(turbo_data, p),
	TP_STRUCT__entry(
		__field(int, original_prio)
		__field(int, prio)
	),

	TP_fast_assign(
		__entry->original_prio = turbo_data->nice_backup + 120;
		__entry->prio = p->static_prio;
	),

	TP_printk("original_prio=%d prio=%d",
		__entry->original_prio,
		__entry->prio)
);

TRACE_EVENT(turbo_vvip_set,
	TP_PROTO(int pid),
	TP_ARGS(pid),
	TP_STRUCT__entry(
		__field(int, pid)
	),

	TP_fast_assign(
		__entry->pid = pid;
	),

	TP_printk("pid=%d",
		__entry->pid)
);

TRACE_EVENT(turbo_vvip_unset,
	TP_PROTO(int pid),
	TP_ARGS(pid),
	TP_STRUCT__entry(
		__field(int, pid)
	),

	TP_fast_assign(
		__entry->pid = pid;
	),

	TP_printk("pid=%d",
		__entry->pid)
);

TRACE_EVENT(turbo_vip,
	TP_PROTO(int avg_cpu_loading, int cpu_loading_thres, const char *vip_desc, int pid,
				const char *caller, int enf_val, u64 enf_mask),
	TP_ARGS(avg_cpu_loading, cpu_loading_thres, vip_desc, pid, caller, enf_val, enf_mask),
	TP_STRUCT__entry(
		__field(int, avg_cpu_loading)
		__field(int, cpu_loading_thres)
		__string(vip_desc, vip_desc)
		__field(int, pid)
		__string(caller, caller)
		__field(int, enf_val)
		__field(u64, enf_mask)
	),

	TP_fast_assign(
		__entry->avg_cpu_loading = avg_cpu_loading;
		__entry->cpu_loading_thres = cpu_loading_thres;
		__assign_str(vip_desc, vip_desc);
		__entry->pid = pid;
		__assign_str(caller, caller);
		__entry->enf_val = enf_val;
		__entry->enf_mask = enf_mask;
	),

	TP_printk("avg_cpu_loading=%d cpu_loading_thres=%d %s tgid=%d enforce caller=%s val=%d mask=%llu",
		__entry->avg_cpu_loading,
		__entry->cpu_loading_thres,
		__get_str(vip_desc),
		__entry->pid,
		__get_str(caller),
		__entry->enf_val,
		__entry->enf_mask)
);

#endif /*_TRACE_TASK_TURBO_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_task_turbo
/* This part must be outside protection */
#include <trace/define_trace.h>

