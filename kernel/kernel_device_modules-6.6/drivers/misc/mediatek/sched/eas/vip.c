// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include "common.h"
#include "vip.h"
#include "sched_trace.h"
#include "eas_plus.h"

unsigned int ls_vip_threshold                   =  DEFAULT_VIP_PRIO_THRESHOLD;
bool vip_enable;

#define link_with_others(lh) (!list_empty(lh))
#define NUM_MAXIMUM_TGID 12
static int *tgid_vip_arr;
int tgid_vip_status;

DEFINE_PER_CPU(struct vip_rq, vip_rq);
inline unsigned int sum_num_vip_in_cpu(int cpu)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu);

	return vrq->sum_num_vip_tasks;
}

inline unsigned int num_vip_in_cpu(int cpu, int vip_prio)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu);

	return vrq->num_vip_tasks[vip_prio];
}

inline unsigned int get_num_higher_prio_vip(int cpu, int vip_prio)
{
	int sum_num = 0;

	for (vip_prio+=1; vip_prio<NUM_VIP_PRIO; vip_prio++)
		sum_num += num_vip_in_cpu(cpu, vip_prio);

	return sum_num;
}

struct task_struct *vts_to_ts(struct vip_task_struct *vts)
{
	struct mtk_task *mts = container_of(vts, struct mtk_task, vip_task);
	struct task_struct *ts = mts_to_ts(mts);
	return ts;
}

pid_t list_head_to_pid(struct list_head *lh)
{
	pid_t pid = vts_to_ts(container_of(lh, struct vip_task_struct, vip_list))->pid;

	/* means list_head is from rq */
	if (!pid)
		pid = 0;
	return pid;
}

int vip_in_gh;
void turn_on_vip_in_gh(void)
{
	vip_in_gh = 1;
}
EXPORT_SYMBOL_GPL(turn_on_vip_in_gh);

void turn_off_vip_in_gh(void)
{
	vip_in_gh = 0;
}
EXPORT_SYMBOL_GPL(turn_off_vip_in_gh);

struct cpumask find_min_num_vip_cpus_slow(int vip_prio, struct cpumask *allowed_cpu_mask_for_slow)
{
	int cpu, num_same_vip, num_higher_vip, min_num_same_vip = UINT_MAX, min_num_higher_vip = UINT_MAX;
	struct cpumask vip_candidate;

	for_each_cpu(cpu, allowed_cpu_mask_for_slow) {
		num_higher_vip = get_num_higher_prio_vip(cpu, vip_prio);
		if (num_higher_vip > min_num_higher_vip)
			continue;

		if (num_higher_vip < min_num_higher_vip) {
			min_num_higher_vip = num_higher_vip;
			min_num_same_vip = UINT_MAX;
		}
		num_same_vip = num_vip_in_cpu(cpu, vip_prio);

		if (num_same_vip <= min_num_same_vip) {
			if (num_same_vip < min_num_same_vip) {
				cpumask_clear(&vip_candidate);
				min_num_same_vip = num_same_vip;
			}
			/* only record min higher & min same */
			cpumask_set_cpu(cpu, &vip_candidate);
		}
	}

	return vip_candidate;
}

/* utilize 4 bit to represent max num VIP in CPU is 16. */
#define MAX_NUM_VIP_IN_CPU_BIT 4
/* the objective is to find min num same prio VIP within min num higher prio VIP
 * e.g. CPU0 HP(higher prio): 1, SP(same prio): 0, CPU1 HP:0 SP:1
 * we should choice CPU1 since VIP can task turns with same prio.
 */
#define ret_first_vip(vrq) (link_with_others(&vrq->vip_tasks) ? \
	container_of(vrq->vip_tasks.next, struct vip_task_struct, vip_list) : NULL)
struct cpumask find_min_num_vip_cpus(struct perf_domain *pd, struct task_struct *p,
		int vip_prio, struct cpumask *allowed_cpu_mask, int order_index, int end_index, int reverse) {
	unsigned int cpu, num_same_vip, min_num_same_vip = UINT_MAX;
	struct cpumask vip_candidate;
	struct perf_domain *pd_ptr = pd;
	unsigned int num_vip_in_cpu_arr[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = -1};
	bool failed = false;
	struct cpumask allowed_cpu_mask_for_slow, *pd_cpumask;
	int cluster;

	cpumask_clear(&vip_candidate);
	/* Remain this to prevent from crucial error. */
	if (!pd_ptr) {
		failed = true;
		goto backup;
	}

	cpumask_clear(&allowed_cpu_mask_for_slow);
	/* fast path: find min num SP(Same Prio) VIP within CPUs don't have higher prio */

	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		if (!vip_in_gh)
			pd_cpumask = get_gear_cpumask(cluster);
		else
			pd_cpumask = &cpu_array[order_index][cluster][reverse];

		for_each_cpu_and(cpu, pd_cpumask, cpu_active_mask) {
			struct vip_rq *vrq = &per_cpu(vip_rq, cpu);
			struct vip_task_struct *first_vip = NULL;

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			if (cpu_high_irqload(cpu))
				continue;

			cpumask_set_cpu(cpu, allowed_cpu_mask);

			if (cpu_rq(cpu)->rt.rt_nr_running >= 1 &&
						!rt_rq_throttled(&(cpu_rq(cpu)->rt)))
				continue;

			cpumask_set_cpu(cpu, &allowed_cpu_mask_for_slow);
			first_vip = ret_first_vip(vrq);
			if (first_vip && first_vip->vip_prio > vip_prio)
				continue;

			num_same_vip = vrq->num_vip_tasks[vip_prio];
			/* the VIP selecting CPU is on this CPU */
			if (task_is_vip(p, NOT_VIP) && task_cpu(p) == cpu)
				num_same_vip -= 1;
			num_vip_in_cpu_arr[cpu] = num_same_vip;
			if (num_same_vip > min_num_same_vip)
				continue;

			if (num_same_vip < min_num_same_vip) {
				cpumask_clear(&vip_candidate);
				min_num_same_vip = num_same_vip;
			}

			cpumask_set_cpu(cpu, &vip_candidate);
		}
		if (vip_in_gh && (cluster >= end_index))
			break;
	}

	if (!cpumask_empty(&vip_candidate))
		goto out;

	/* slow path: only enter when all allowed CPUs have HP(Higher Prio) VIP.
	 * compare num HP VIP with each CPUs to find min num HP CPUs,
	 * and find min num SP(Same Prio) CPUs within min num HP CPUs.
	 */
	if (cpumask_weight(&allowed_cpu_mask_for_slow) != 0)
		vip_candidate = find_min_num_vip_cpus_slow(vip_prio, &allowed_cpu_mask_for_slow);

backup:
	if (!cpumask_weight(&vip_candidate)) {
		/* no cpu selected above, use available CPUs */
		cpumask_andnot(&vip_candidate, p->cpus_ptr, cpu_pause_mask);
		cpumask_and(&vip_candidate, &vip_candidate, cpu_active_mask);
		cpumask_copy(allowed_cpu_mask, &vip_candidate);
	}

out:
	if (trace_sched_find_min_num_vip_cpus_enabled()) {
		u64 num_vip_in_cpu_bit = 0;

		for(cpu=0; cpu<MAX_NR_CPUS; cpu++) {
			num_vip_in_cpu_bit <<= MAX_NUM_VIP_IN_CPU_BIT;
			if (num_vip_in_cpu_arr[cpu] != -1)
				num_vip_in_cpu_bit |= (num_vip_in_cpu_arr[cpu]<<1);
			else
				num_vip_in_cpu_bit |= 1;
		}
		trace_sched_find_min_num_vip_cpus(failed, p->pid, &vip_candidate, num_vip_in_cpu_bit);
	}
	return vip_candidate;
}

int find_vip_backup_cpu(struct task_struct *p, struct cpumask *allowed_cpu_mask, int prev_cpu, int target)
{
	unsigned long best_cap = 0;
	int cpu, best_cpu = -1;

	/* Search CPUs starting from the first bit of the mask until meet target.
	 * 0 1 2 3 4 5 6 7
	 * |-start |-target
	 * CPU4~7 have searched in mtk_select_idle_capacity, here we search CPUs0~3
	 */
	for_each_cpu(cpu, allowed_cpu_mask) {
		unsigned long cpu_cap = capacity_of(cpu);

		if (cpu == target)
			break;

		if (task_fits_capacity(p, cpu_cap, get_adaptive_margin(cpu)))
			return cpu;

		if (cpu_cap > best_cap) {
			best_cap = cpu_cap;
			best_cpu = cpu;
		}
	}

	if (best_cpu != -1)
		return best_cpu;

	/* all CPUs have no spare capacity, find cache benefit CPU */
	if (cpumask_test_cpu(target, allowed_cpu_mask))
		return target;
	else if (prev_cpu != target && cpumask_test_cpu(prev_cpu, allowed_cpu_mask))
		return prev_cpu;

	return cpumask_first(allowed_cpu_mask);
}

struct task_struct *next_vip_runnable_in_cpu(struct rq *rq, int type)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct list_head *pos;
	struct task_struct *p;

	list_for_each(pos, &vrq->vip_tasks) {
		struct vip_task_struct *tmp_vts = container_of(pos, struct vip_task_struct,
								vip_list);

		if (tmp_vts->vip_prio != type)
			continue;

		p = vts_to_ts(tmp_vts);
		/* we should pull runnable here, so don't pull curr*/
		if (!rq->curr || p->pid != rq->curr->pid)
			return p;
	}

	return NULL;
}

bool balance_vvip_overutilied;
void turn_on_vvip_balance_overutilized(void)
{
	balance_vvip_overutilied = true;
}
EXPORT_SYMBOL_GPL(turn_on_vvip_balance_overutilized);

void turn_off_vvip_balance_overutilized(void)
{
	balance_vvip_overutilied = false;
}
EXPORT_SYMBOL_GPL(turn_off_vvip_balance_overutilized);

bool balance_vip_overutilized;
void turn_on_vip_balance_overutilized(void)
{
	balance_vip_overutilized = true;
}
EXPORT_SYMBOL_GPL(turn_on_vip_balance_overutilized);

void turn_off_vip_balance_overutilized(void)
{
	balance_vip_overutilized = false;
}
EXPORT_SYMBOL_GPL(turn_off_vip_balance_overutilized);

int find_imbalanced_vvip_gear(void)
{
	int gear = -1;
	struct cpumask cpus;
	int cpu;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd;
	int num_vvip_in_gear = 0;
	int num_cpu = 0;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto out;
	for (gear = num_sched_clusters-1; gear >= 0 ; gear--) {
		cpumask_and(&cpus, perf_domain_span(pd), cpu_active_mask);
		for_each_cpu(cpu, &cpus) {
			num_vvip_in_gear += num_vip_in_cpu(cpu, VVIP);
			num_cpu += 1;

			if (trace_sched_find_imbalanced_vvip_gear_enabled())
				trace_sched_find_imbalanced_vvip_gear(cpu, num_vvip_in_gear);
		}

		/* Choice it since it's beggiest gaar without VVIP*/
		if (num_vvip_in_gear == 0)
			goto out;

		/* Choice it since it's biggest imbalanced gear */
		if (num_vvip_in_gear % num_cpu != 0)
			goto out;

		num_vvip_in_gear = 0;
		num_cpu = 0;
		pd = pd->next;
	}

	/* choice biggest gear when all gear balanced and have VVIP*/
	gear = num_sched_clusters - 1;

out:
	rcu_read_unlock();
	return gear;
}

bool prio_is_vip(int vip_prio, int type)
{
	if (type == VVIP)
		return (vip_prio == VVIP);

	return (vip_prio != NOT_VIP);
}
EXPORT_SYMBOL_GPL(prio_is_vip);

bool task_is_vip(struct task_struct *p, int type)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	if (rt_task(p))
		return false;

	if (type == VVIP)
		return (vts->vip_prio == VVIP);

	if (type == MAX_PRIORITY_BASED_VIP)
		return ((vts->vip_prio <= MAX_PRIORITY_BASED_VIP) && (vts->vip_prio >= MIN_PRIORITY_BASED_VIP));

	return (vts->vip_prio != NOT_VIP);
}
EXPORT_SYMBOL_GPL(task_is_vip);

static inline unsigned int vip_task_limit(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	return vts->throttle_time;
}

void check_vip_num(struct rq *rq)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	int vip_prio = 0;

	/* temp patch for counter issue*/
	if (list_empty(&vrq->vip_tasks)) {
		if (vrq->sum_num_vip_tasks != 0) {
			vrq->sum_num_vip_tasks = 0;
			pr_info("cpu=%d error VIP number\n", cpu_of(rq));
		}
		for (; vip_prio<NUM_VIP_PRIO; vip_prio++) {
			if (vrq->num_vip_tasks[vip_prio] != 0) {
				vrq->num_vip_tasks[vip_prio] = 0;
			pr_info("cpu=%d error vip_prio=%d number\n", cpu_of(rq), vrq->num_vip_tasks[vip_prio]);
			}
		}
	}
	/* end of temp patch*/
}

static void insert_vip_task(struct rq *rq, struct vip_task_struct *vts,
					bool at_front, bool requeue, int vip_prio)
{
	struct list_head *pos;
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));

	/* change vip_prio inside lock to prevent NOT_VIP inserted.
	 * it could happened if we set vip_prio outside lock, and user unset,
	 * then insert VIP.
	 */
	if (vip_prio != NOT_VIP)
		vts->vip_prio = vip_prio;

	if (vts->vip_prio == NOT_VIP)
		return;

	list_for_each(pos, &vrq->vip_tasks) {
		struct vip_task_struct *tmp_vts = container_of(pos, struct vip_task_struct,
								vip_list);
		if (at_front) {
			if (vts->vip_prio >= tmp_vts->vip_prio)
				break;
		} else {
			if (vts->vip_prio > tmp_vts->vip_prio)
				break;
		}
	}
	list_add(&vts->vip_list, pos->prev);
	if (!requeue) {
		vrq->num_vip_tasks[vts->vip_prio] += 1;
		vrq->sum_num_vip_tasks += 1;
	}

	/* vip inserted trace event */
	if (trace_sched_insert_vip_task_enabled()) {
		pid_t prev_pid = list_head_to_pid(vts->vip_list.prev);
		pid_t next_pid = list_head_to_pid(vts->vip_list.next);
		bool is_first_entry = (prev_pid == 0) ? true : false;
		struct task_struct *p = vts_to_ts(vts);

		trace_sched_insert_vip_task(p, cpu_of(rq), vts->vip_prio,
			at_front, prev_pid, next_pid, requeue, is_first_entry, vrq->sum_num_vip_tasks);
	}
}

static void deactivate_vip_task(struct task_struct *p, struct rq *rq)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct list_head *prev = vts->vip_list.prev;
	struct list_head *next = vts->vip_list.next;

	if (vts->vip_list.next == NULL || !link_with_others(&vts->vip_list))
		return;

	list_del_init(&vts->vip_list);

	if (vts->vip_prio != NOT_VIP) {
		vrq->num_vip_tasks[vts->vip_prio] -= 1;
		vrq->sum_num_vip_tasks -= 1;
	}

	vts->vip_prio = NOT_VIP;
	vts->faster_compute_eng = false;

	/* for insurance */
	check_vip_num(rq);

	if (trace_sched_deactivate_vip_task_enabled()) {
		pid_t prev_pid = list_head_to_pid(prev);
		pid_t next_pid = list_head_to_pid(next);

		trace_sched_deactivate_vip_task(p->pid, task_cpu(p), prev_pid, next_pid, vrq->sum_num_vip_tasks);
	}
}

void __set_group_vip_prio(struct task_group *tg, unsigned int prio)
{
	struct vip_task_group *vtg;

	if (tg == &root_task_group)
		return;

	vtg = &((struct mtk_tg *) tg->android_vendor_data1)->vtg;
	vtg->threshold = prio;
}

int unset_group_vip_prio(unsigned int cpuctl_id)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);

	if (tg == &root_task_group)
		return 0;

	__set_group_vip_prio(tg, DEFAULT_VIP_PRIO_THRESHOLD);
	return 1;
}
EXPORT_SYMBOL_GPL(unset_group_vip_prio);

void set_group_vip_prio_by_name(char *group_name, unsigned int prio)
{
	struct task_group *tg = search_tg_by_name(group_name);

	if (tg == &root_task_group)
		return;

	__set_group_vip_prio(tg, prio);
}

int set_group_vip_prio(unsigned int cpuctl_id, unsigned int prio)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);

	if (tg == &root_task_group)
		return 0;

	__set_group_vip_prio(tg, prio);
	return 1;
}
EXPORT_SYMBOL_GPL(set_group_vip_prio);

/* top-app interface */
void set_top_app_vip(unsigned int prio)
{
	set_group_vip_prio_by_name("top-app", prio);
}
EXPORT_SYMBOL_GPL(set_top_app_vip);

void unset_top_app_vip(void)
{
	set_group_vip_prio_by_name("top-app", DEFAULT_VIP_PRIO_THRESHOLD);
}
EXPORT_SYMBOL_GPL(unset_top_app_vip);
/* end of top-app interface */

/* foreground interface */
void set_foreground_vip(unsigned int prio)
{
	set_group_vip_prio_by_name("foreground", prio);
}
EXPORT_SYMBOL_GPL(set_foreground_vip);

void unset_foreground_vip(void)
{
	set_group_vip_prio_by_name("foreground", DEFAULT_VIP_PRIO_THRESHOLD);
}
EXPORT_SYMBOL_GPL(unset_foreground_vip);
/* end of foreground interface */

/* background interface */
void set_background_vip(unsigned int prio)
{
	set_group_vip_prio_by_name("background", prio);
}
EXPORT_SYMBOL_GPL(set_background_vip);

void unset_background_vip(void)
{
	set_group_vip_prio_by_name("background", DEFAULT_VIP_PRIO_THRESHOLD);
}
EXPORT_SYMBOL_GPL(unset_background_vip);
/* end of background interface */

int get_group_threshold(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	struct task_group *tg = container_of(css, struct task_group, css);
	struct vip_task_group *vtg = &((struct mtk_tg *) tg->android_vendor_data1)->vtg;

	if (vtg)
		return vtg->threshold;

	return -1;
}

bool is_VIP_task_group(struct task_struct *p)
{
	if (p->prio <= get_group_threshold(p))
		return true;

	return false;
}

/* ls vip interface */
void set_ls_task_vip(unsigned int prio)
{
	ls_vip_threshold = prio;
}
EXPORT_SYMBOL_GPL(set_ls_task_vip);

void unset_ls_task_vip(void)
{
	ls_vip_threshold = DEFAULT_VIP_PRIO_THRESHOLD;
}
EXPORT_SYMBOL_GPL(unset_ls_task_vip);
/* end of ls vip interface */

bool is_VIP_latency_sensitive(struct task_struct *p)
{
	if (is_task_latency_sensitive(p) && p->prio <= ls_vip_threshold)
		return true;

	return false;
}

void set_task_vvip_and_throttle(int pid, unsigned int throttle_time)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->vvip = true;
		vts->throttle_time = min(throttle_time * 1000000, VIP_TIME_LIMIT_MAX);
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(pid, done, "vvip_throttle", VVIP, throttle_time, 0);
}
EXPORT_SYMBOL(set_task_vvip_and_throttle);

void set_task_vvip(int pid)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->vvip = true;
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(pid, done, "vvip", VVIP, VIP_TIME_LIMIT_DEFAULT/1000000, 0);
}
EXPORT_SYMBOL_GPL(set_task_vvip);

void unset_task_vvip(int pid)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->vvip = false;
		vts->throttle_time = VIP_TIME_LIMIT_DEFAULT;
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_unset_vip_enabled())
		trace_sched_unset_vip(pid, done, "VVIP", 0);

}
EXPORT_SYMBOL(unset_task_vvip);

/* priority based VIP interface */
void set_task_priority_based_vip_and_throttle(int pid, int prio, unsigned int throttle_time)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	prio = clamp(prio, MIN_PRIORITY_BASED_VIP, MAX_PRIORITY_BASED_VIP);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->priority_based_prio = prio;
		vts->throttle_time = min(throttle_time * 1000000, VIP_TIME_LIMIT_MAX);
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(pid, done, "priority_based_vip_throttle", prio, throttle_time, 0);
}
EXPORT_SYMBOL(set_task_priority_based_vip_and_throttle);

void set_task_priority_based_vip(int pid, int prio)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	prio = clamp(prio, MIN_PRIORITY_BASED_VIP, MAX_PRIORITY_BASED_VIP);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->priority_based_prio = prio;
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(pid, done, "priority_based_vip", prio, VIP_TIME_LIMIT_DEFAULT/1000000, 0);
}
EXPORT_SYMBOL_GPL(set_task_priority_based_vip);

void unset_task_priority_based_vip(int pid)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->priority_based_prio = NOT_VIP;
		vts->throttle_time = VIP_TIME_LIMIT_DEFAULT;
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_unset_vip_enabled())
		trace_sched_unset_vip(pid, done, "priority_based_vip", 0);
}
EXPORT_SYMBOL(unset_task_priority_based_vip);
/* priority based VIP interface */

/* TGID */
void set_tgid_basic_vip(int tgid)
{
	struct task_struct *group_leader, *p;
	struct vip_task_struct *vts;

	rcu_read_lock();
	group_leader = find_task_by_vpid(tgid);
	if (group_leader) {
		list_for_each_entry(p, &group_leader->thread_group, thread_group) {
			get_task_struct(p);
			vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
			vts->basic_vip = true;
			put_task_struct(p);
		}
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_tgid_basic_vip);

void unset_tgid_basic_vip(int tgid)
{
	struct task_struct *group_leader, *p;
	struct vip_task_struct *vts;

	rcu_read_lock();
	group_leader = find_task_by_vpid(tgid);
	if (group_leader) {
		list_for_each_entry(p, &group_leader->thread_group, thread_group) {
			get_task_struct(p);
			vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
			vts->basic_vip = false;
			put_task_struct(p);
		}
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_tgid_basic_vip);

int show_tgid(int slot_id)
{
	return tgid_vip_arr[slot_id];
}
EXPORT_SYMBOL_GPL(show_tgid);

int set_tgid_vip(int tgid)
{
	int slot_id = 0, set_state;

	rcu_read_lock();
	if (find_task_by_vpid(tgid) == NULL) {
		rcu_read_unlock();
		set_state = TGID_NOT_FOUND;
		goto out;
	}
	rcu_read_unlock();

	for (slot_id = 0; slot_id < NUM_MAXIMUM_TGID; slot_id++) {
		if (tgid_vip_arr[slot_id] == -1) {
			tgid_vip_arr[slot_id] = tgid;
			set_state = TGID_SET_SUCCESS;
			goto out;
		}
	}

	set_state = TGID_SLOT_EXCEED;

out:
	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(tgid, set_state, "tgid", WORKER_VIP, VIP_TIME_LIMIT_DEFAULT/1000000, slot_id);

	return set_state;
}
EXPORT_SYMBOL(set_tgid_vip);

int unset_tgid_vip(int tgid)
{
	int slot_id = 0, unset_state;

	for (slot_id = 0; slot_id < NUM_MAXIMUM_TGID; slot_id++) {
		if (tgid_vip_arr[slot_id] == tgid) {
			tgid_vip_arr[slot_id] = -1;
			unset_state = TGID_SET_SUCCESS;
			goto out;
		}
	}

	unset_state = TGID_SLOT_EXCEED;

out:
	if (trace_sched_unset_vip_enabled())
		trace_sched_unset_vip(tgid, unset_state, "tgid", slot_id);

	return unset_state;
}
EXPORT_SYMBOL(unset_tgid_vip);

bool is_VIP_tgid(struct task_struct *p)
{
	int slot_id = 0;

	for (slot_id = 0; slot_id < NUM_MAXIMUM_TGID; slot_id++) {
		if (p->tgid == tgid_vip_arr[slot_id])
			return true;
	}

	return false;
}

void turn_on_tgid_vip(void)
{
	tgid_vip_status = 1;
}
EXPORT_SYMBOL(turn_on_tgid_vip);

void turn_off_tgid_vip(void)
{
	tgid_vip_status = 0;
}
EXPORT_SYMBOL(turn_off_tgid_vip);

int tgid_vip_on(void)
{
	return tgid_vip_status;
}
/* end of TGID */

/* basic vip interace */
void set_task_basic_vip_and_throttle(int pid, unsigned int throttle_time)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->basic_vip = true;
		vts->throttle_time = min(throttle_time * 1000000, VIP_TIME_LIMIT_MAX);
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(pid, done, "basic_vip_and_throttle", WORKER_VIP, throttle_time, 0);
}
EXPORT_SYMBOL(set_task_basic_vip_and_throttle);

void set_task_basic_vip(int pid)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->basic_vip = true;
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_set_vip_enabled())
		trace_sched_set_vip(pid, done, "basic_vip", WORKER_VIP, VIP_TIME_LIMIT_DEFAULT/1000000, 0);
}
EXPORT_SYMBOL(set_task_basic_vip);

void unset_task_basic_vip(int pid)
{
	struct task_struct *p;
	struct vip_task_struct *vts;
	int done = 0;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
		vts->basic_vip = false;
		vts->throttle_time = VIP_TIME_LIMIT_DEFAULT;
		done = 1;
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (trace_sched_unset_vip_enabled())
		trace_sched_unset_vip(pid, done, "basic_vip", 0);
}
EXPORT_SYMBOL(unset_task_basic_vip);
/* end of basic vip interface */

#define is_VIP_basic(vts) (vts->basic_vip)
#define is_VVIP(vts) (vts->vvip)
#define is_priority_based_vip(vts) ((vts->priority_based_prio <= MAX_PRIORITY_BASED_VIP) &&	\
	(vts->priority_based_prio >= MIN_PRIORITY_BASED_VIP))
inline int get_vip_task_prio(struct task_struct *p)
{
	int vip_prio = NOT_VIP;
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	if (rt_task(p))
		return NOT_VIP;

	/* prio = 4 */
	if (is_VVIP(vts)) {
		vip_prio = VVIP;
		goto out;
	}

	/* prio = 1~3*/
	if (is_priority_based_vip(vts)) {
		vip_prio = vts->priority_based_prio;
		goto out;
	}

	/* prio = 0 */
	if (is_VIP_task_group(p) || is_VIP_latency_sensitive(p) || is_VIP_basic(vts) ||
		(tgid_vip_on() && is_VIP_tgid(p)))
		vip_prio = WORKER_VIP;

out:
	if (trace_sched_get_vip_task_prio_enabled()) {
		trace_sched_get_vip_task_prio(p, vip_prio, is_task_latency_sensitive(p),
			ls_vip_threshold, get_group_threshold(p), is_VIP_basic(vts));
	}
	return vip_prio;
}
EXPORT_SYMBOL_GPL(get_vip_task_prio);

void vip_enqueue_task(struct rq *rq, struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	int vip_prio = get_vip_task_prio(p);

	if (unlikely(!vip_enable))
		return;

	if (vip_prio == NOT_VIP)
		return;

	/*
	 * This can happen during migration or enq/deq for prio/class change.
	 * it was once VIP but got demoted, it will not be VIP until
	 * it goes to sleep again.
	 */
	if (vts->total_exec > vip_task_limit(p))
		return;

	insert_vip_task(rq, vts, task_on_cpu(rq, p), false, vip_prio);

	/*
	 * We inserted the task at the appropriate position. Take the
	 * task runtime snapshot. From now onwards we use this point as a
	 * baseline to enforce the slice and demotion.
	 */
	if (!vts->total_exec) /* queue after sleep */
		vts->sum_exec_snapshot = p->se.sum_exec_runtime;
}

/*
 * VIP task runtime update happens here. Three possibilities:
 *
 * de-activated: The VIP consumed its runtime. Non VIP can preempt.
 * slice expired: VIP slice is expired and other VIP can preempt.
 * slice not expired: This VIP task can continue to run.
 */
static void account_vip_runtime(struct rq *rq, struct task_struct *curr)
{
	struct vip_task_struct *vts = &((struct mtk_task *) curr->android_vendor_data1)->vip_task;
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	s64 delta;
	unsigned int limit;

	lockdep_assert_held(&rq->__lock);

	/*
	 * RQ clock update happens in tick path in the scheduler.
	 * Since we drop the lock in the scheduler before calling
	 * into vendor hook, it is possible that update flags are
	 * reset by another rq lock and unlock. Do the update here
	 * if required.
	 */
	if (!(rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(rq);

	/* sum_exec_snapshot can be ahead. See below increment */
	delta = curr->se.sum_exec_runtime - vts->sum_exec_snapshot;
	if (delta < 0)
		delta = 0;
	else
		delta += rq_clock_task(rq) - curr->se.exec_start;
	/* slice is not expired */
	if (delta < VIP_TIME_SLICE)
		return;

	/*
	 * slice is expired, check if we have to deactivate the
	 * VIP task, otherwise requeue the task in the list so
	 * that other VIP tasks gets a chance.
	 */
	vts->sum_exec_snapshot += delta;
	vts->total_exec += delta;

	limit = vip_task_limit(curr);
	if (vts->total_exec > limit) {
		deactivate_vip_task(curr, rq);
		if (trace_sched_vip_throttled_enabled())
			trace_sched_vip_throttled(curr->pid, cpu_of(rq), vts->vip_prio,
				vts->throttle_time/1000000, vts->total_exec/1000000);
		return;
	}

	/* only this vip task in rq, skip re-queue section */
	if (vrq->sum_num_vip_tasks == 1)
		return;

	/* slice expired. re-queue the task */
	if (vts->vip_list.next == NULL || !link_with_others(&vts->vip_list))
		return;

	list_del_init(&vts->vip_list);
	insert_vip_task(rq, vts, false, true, NOT_VIP);
}

void vip_check_preempt_wakeup(void *unused, struct rq *rq, struct task_struct *p,
				bool *preempt, bool *nopreempt, int wake_flags,
				struct sched_entity *se, struct sched_entity *pse,
				int next_buddy_marked)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct vip_task_struct *vts_p = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	struct task_struct *c = rq->curr;
	struct vip_task_struct *vts_c;
	bool resched = false;
	bool p_is_vip, curr_is_vip;

	vts_c = &((struct mtk_task *) rq->curr->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	p_is_vip = vts_p->vip_list.next && link_with_others(&vts_p->vip_list);
	curr_is_vip = vts_c->vip_list.next && link_with_others(&vts_c->vip_list);
	/*
	 * current is not VIP, so preemption decision
	 * is simple.
	 */
	if (!curr_is_vip) {
		if (p_is_vip)
			goto preempt;
		return; /* CFS decides preemption */
	}

	/*
	 * current is VIP. update its runtime before deciding the
	 * preemption.
	 */
	account_vip_runtime(rq, c);
	resched = (vrq->vip_tasks.next != &vts_c->vip_list);
	/*
	 * current is no longer eligible to run. It must have been
	 * picked (because of VIP) ahead of other tasks in the CFS
	 * tree, so drive preemption to pick up the next task from
	 * the tree, which also includes picking up the first in
	 * the VIP queue.
	 */
	if (resched)
		goto preempt;

	/* current is the first in the queue, so no preemption */
	*nopreempt = true;
	return;
preempt:
	*preempt = true;
}

void vip_cfs_tick(struct rq *rq)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct vip_task_struct *vts;
	struct rq_flags rf;

	vts = &((struct mtk_task *) rq->curr->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	rq_lock(rq, &rf);

	if (vts->vip_list.next && !link_with_others(&vts->vip_list))
		goto out;
	account_vip_runtime(rq, rq->curr);
	/*
	 * If the current is not VIP means, we have to re-schedule to
	 * see if we can run any other task including VIP tasks.
	 */
	if ((vrq->vip_tasks.next != &vts->vip_list) && rq->cfs.h_nr_running > 1)
		resched_curr(rq);

out:
	rq_unlock(rq, &rf);
}

void vip_lb_tick(struct rq *rq)
{
	vip_cfs_tick(rq);
}

void vip_scheduler_tick(void *unused, struct rq *rq)
{
	struct task_struct *p = rq->curr;

	if (unlikely(!vip_enable))
		return;

	if (!vip_fair_task(p))
		return;

	vip_lb_tick(rq);
}
#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
/* Walk up scheduling entities hierarchy */
#define for_each_sched_entity(se) \
	for (; se; se = se->parent)
#else
#define for_each_sched_entity(se) \
	for (; se; se = NULL)
#endif

extern void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
void vip_replace_next_task_fair(void *unused, struct rq *rq, struct task_struct **p,
				struct sched_entity **se, bool *repick, bool simple,
				struct task_struct *prev)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct vip_task_struct *vts;
	struct task_struct *vip;


	if (unlikely(!vip_enable))
		return;

	/* We don't have VIP tasks queued */
	if (!link_with_others(&vrq->vip_tasks)) {
		/* we should pull VIPs from other CPU */
		return;
	}

	/* Return the first task from VIP queue */
	vts = list_first_entry(&vrq->vip_tasks, struct vip_task_struct, vip_list);
	vip = vts_to_ts(vts);

	*p = vip;
	*se = &vip->se;
	*repick = true;

	if (simple) {
		for_each_sched_entity((*se))
			set_next_entity(cfs_rq_of(*se), *se);
	}
}

__no_kcsan
void vip_dequeue_task(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	if (link_with_others(&vts->vip_list))
		deactivate_vip_task(p, rq);

	/*
	 * Reset the exec time during sleep so that it starts
	 * from scratch upon next wakeup. total_exec should
	 * be preserved when task is enq/deq while it is on
	 * runqueue.
	 */

	if (READ_ONCE(p->__state) != TASK_RUNNING)
		vts->total_exec = 0;
}

inline bool vip_fair_task(struct task_struct *p)
{
	return p->prio >= MAX_RT_PRIO && !is_idle_task(p);
}

void init_vip_task_struct(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	INIT_LIST_HEAD(&vts->vip_list);
	vts->sum_exec_snapshot = 0;
	vts->total_exec = 0;
	vts->vip_prio = NOT_VIP;
	vts->basic_vip = false;
	vts->vvip = false;
	vts->faster_compute_eng = false;
	vts->priority_based_prio = NOT_VIP;
	vts->throttle_time = VIP_TIME_LIMIT_DEFAULT;
}

void init_task_gear_hints(struct task_struct *p)
{
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	ghts->gear_start = GEAR_HINT_UNSET;
	ghts->num_gear   = GEAR_HINT_UNSET;
	ghts->reverse    = 0;
}

static void vip_new_tasks(void *unused, struct task_struct *new)
{
	init_vip_task_struct(new);
	init_task_gear_hints(new);
}

void __init_vip_group(struct cgroup_subsys_state *css)
{
	struct task_group *tg = container_of(css, struct task_group, css);
	struct vip_task_group *vtg = &((struct mtk_tg *) tg->android_vendor_data1)->vtg;

	vtg->threshold = DEFAULT_VIP_PRIO_THRESHOLD;
}

static void vip_rvh_cpu_cgroup_online(void *unused, struct cgroup_subsys_state *css)
{
	__init_vip_group(css);
	_init_tg_mask(css);
}

void init_vip_group(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	rcu_read_lock();
	__init_vip_group(&root_task_group.css);
	css_for_each_child(css, top_css)
		__init_vip_group(css);
	rcu_read_unlock();
}

DEFINE_PER_CPU(struct task_struct *, runnable_vip);
void vip_push_runnable(struct rq *src_rq)
{
	int this_cpu = cpu_of(src_rq);
	int new_cpu = -1;
	struct task_struct *task_to_pushed = per_cpu(runnable_vip, src_rq->cpu);
	struct vip_task_struct *vts;
	struct rq *dst_rq;

	if (in_interrupt())
		goto put_task;

	if (cpumask_weight(task_to_pushed->cpus_ptr) <= 1)
		goto put_task;

	vts = &((struct mtk_task *) task_to_pushed->android_vendor_data1)->vip_task;
	vts->faster_compute_eng = true;
	mtk_find_energy_efficient_cpu(NULL, task_to_pushed, this_cpu, 0, &new_cpu);

	if (new_cpu < 0)
		goto put_task;

	if (new_cpu == this_cpu)
		goto put_task;

	dst_rq = cpu_rq(new_cpu);
	double_lock_balance(src_rq, dst_rq);

	if (in_interrupt())
		goto unlock;

	if ((task_rq(task_to_pushed) != src_rq) ||
		(cpu_rq(task_cpu(task_to_pushed))->curr->pid == task_to_pushed->pid) ||
		(!task_on_rq_queued(task_to_pushed)))
		goto unlock;

	/* de-queue from curr rq */
	update_rq_clock(src_rq);

	if (!cpu_online(new_cpu) || cpu_paused(new_cpu))
		goto unlock;

	deactivate_task(src_rq, task_to_pushed, DEQUEUE_NOCLOCK);
	set_task_cpu(task_to_pushed, new_cpu);

	/* en-queue to dst rq */
	update_rq_clock(dst_rq);
	lockdep_assert_rq_held(dst_rq);
	activate_task(dst_rq, task_to_pushed, ENQUEUE_NOCLOCK);
	check_preempt_curr(dst_rq, task_to_pushed, 0);

	trace_sched_force_migrate(task_to_pushed, new_cpu, MIGR_SWITCH_PUSH_VIP);

unlock:
	double_unlock_balance(src_rq, dst_rq);
put_task:
	put_task_struct(task_to_pushed);
}

DEFINE_PER_CPU(struct balance_callback, vip_push_head);
void vip_sched_switch(struct task_struct *prev, struct task_struct *next, struct rq *rq)
{
	if (in_interrupt())
		return;

	if (!task_is_vip(prev, NOT_VIP))
		return;

	if (READ_ONCE(prev->__state) != TASK_RUNNING)
		return;

	if (next->prio == 0)
		return;

	/* VIP task is runnable, push it. */
	get_task_struct(prev);
	per_cpu(runnable_vip, rq->cpu) = prev;
	queue_balance_callback(rq, &per_cpu(vip_push_head, rq->cpu), vip_push_runnable);
}

void register_vip_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_cpu_cgroup_online(vip_rvh_cpu_cgroup_online,
		NULL);
	if (ret)
		pr_info("register cpu_cgroup_online hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_check_preempt_wakeup(vip_check_preempt_wakeup, NULL);
	if (ret)
		pr_info("register check_preempt_wakeup hooks failed, returned %d\n", ret);

	ret = register_trace_android_vh_scheduler_tick(vip_scheduler_tick, NULL);
	if (ret)
		pr_info("register scheduler_tick failed\n");

	ret = register_trace_android_rvh_replace_next_task_fair(vip_replace_next_task_fair, NULL);
	if (ret)
		pr_info("register replace_next_task_fair hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_after_dequeue_task(vip_dequeue_task, NULL);
	if (ret)
		pr_info("register after_dequeue_task hooks failed, returned %d\n", ret);
}

void vip_init(void)
{
	struct task_struct *g, *p;
	int cpu, slot_id, ret = 0;

	balance_vvip_overutilied = false;
	balance_vip_overutilized = false;

	/* init vip related value to group*/
	init_vip_group();

	ret = register_trace_android_rvh_wake_up_new_task(vip_new_tasks, NULL);
	if (ret)
		pr_info("register wake_up_new_task hooks failed, returned %d\n", ret);

	/* init vip related value to exist tasks */
	read_lock(&tasklist_lock);
	for_each_process_thread(g, p) {
		init_vip_task_struct(p);
		init_task_gear_hints(p);
	}
	read_unlock(&tasklist_lock);

	/* init vip related value to each rq */
	for_each_possible_cpu(cpu) {
		struct vip_rq *vrq = &per_cpu(vip_rq, cpu);
		int vip_prio = 0;

		INIT_LIST_HEAD(&vrq->vip_tasks);
		vrq->sum_num_vip_tasks = 0;
		for (; vip_prio<NUM_VIP_PRIO; vip_prio++) {
			vrq->num_vip_tasks[vip_prio] = 0;
			vrq->sum_num_vip_tasks = 0;
		}

		/*
		 * init vip related value to idle thread.
		 * some times we'll reference VIP variables from idle process,
		 * so initial it's value to prevent KE.
		 */
		init_vip_task_struct(cpu_rq(cpu)->idle);
	}

	tgid_vip_status = 0;
	tgid_vip_arr = kcalloc(NUM_MAXIMUM_TGID, sizeof(int),  GFP_KERNEL);
	for (slot_id = 0; slot_id < NUM_MAXIMUM_TGID; slot_id++)
		tgid_vip_arr[slot_id] = -1;

	/* init vip related value to newly forked tasks */
	register_vip_hooks();
	vip_enable = sched_vip_enable_get();
}
