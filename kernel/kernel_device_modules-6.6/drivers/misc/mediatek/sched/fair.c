// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>
#include "eas/eas_plus.h"
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v3/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "common.h"
#include <sched/pelt.h>
#include <linux/stop_machine.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif
#include <mt-plat/mtk_irq_mon.h>
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include "eas/vip.h"
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "eas/group.h"
#endif
#define CREATE_TRACE_POINTS
#include "sched_trace.h"

MODULE_LICENSE("GPL");

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/* Runqueue only has SCHED_IDLE tasks enqueued */
static int sched_idle_rq(struct rq *rq)
{
	return unlikely(rq->nr_running == rq->cfs.idle_h_nr_running &&
			rq->nr_running);
}

#ifdef CONFIG_SMP
bool mtk_cpus_share_cache(unsigned int this_cpu, unsigned int that_cpu)
{
	if (this_cpu == that_cpu)
		return true;

	return topology_cluster_id(this_cpu) == topology_cluster_id(that_cpu);
}

static int sched_idle_cpu(int cpu)
{
	return sched_idle_rq(cpu_rq(cpu));
}

static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

/* cloned from kmainline _task_util_est() */
static inline unsigned long _task_util_est(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_est) & ~UTIL_AVG_UNCHANGED;
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	if (sched_feat(UTIL_EST) && is_util_est_enable())
		return max(task_util(p), _task_util_est(p));
	return task_util(p);
}

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(rt_task(p) ? 0 : task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return rt_task(p) ? 0 : task_util_est(p);
}
#endif

int task_fits_capacity(struct task_struct *p, long capacity, unsigned int margin)
{
	return fits_capacity(uclamp_task_util(p), capacity, margin);
}

unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;

}

#if IS_ENABLED(CONFIG_MTK_EAS)
/*
 * Compute the task busy time for compute_energy(). This time cannot be
 * injected directly into effective_cpu_util() because of the IRQ scaling.
 * The latter only makes sense with the most recent CPUs where the task has
 * run.
 */
static inline void eenv_task_busy_time(struct energy_env *eenv,
				       struct task_struct *p, int prev_cpu)
{
	unsigned long busy_time, max_cap = arch_scale_cpu_capacity(prev_cpu);
	unsigned long irq = cpu_util_irq(cpu_rq(prev_cpu));

	if (unlikely(irq >= max_cap))
		busy_time = max_cap;
	else
		busy_time = scale_irq_capacity(task_util_est(p), irq, max_cap);

	eenv->task_busy_time = busy_time;
}

/*
 * Compute the perf_domain (PD) busy time for compute_energy(). Based on the
 * utilization for each @pd_cpus, it however doesn't take into account
 * clamping since the ratio (utilization / cpu_capacity) is already enough to
 * scale the EM reported power consumption at the (eventually clamped)
 * cpu_capacity.
 *
 * The contribution of the task @p for which we want to estimate the
 * energy cost is removed (by cpu_util_next()) and must be calculated
 * separately (see eenv_task_busy_time). This ensures:
 *
 *   - A stable PD utilization, no matter which CPU of that PD we want to place
 *     the task on.
 *
 *   - A fair comparison between CPUs as the task contribution (task_util())
 *     will always be the same no matter which CPU utilization we rely on
 *     (util_avg or util_est).
 *
 * Set @eenv busy time for the PD that spans @pd_cpus. This busy time can't
 * exceed @eenv->pd_cap.
 */
static inline void eenv_pd_busy_time(struct energy_env *eenv,
				struct cpumask *pd_cpus,
				struct task_struct *p)
{
	unsigned long busy_time = 0;
	int pd_idx = cpumask_first(pd_cpus);
	int cpu;

	if (eenv->pds_busy_time[pd_idx] != -1)
		return;

	for_each_cpu(cpu, pd_cpus) {
		unsigned long util = mtk_cpu_util_next(cpu, p, -1, 0);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		busy_time += mtk_cpu_util(cpu, util, ENERGY_UTIL,
					NULL, 0, 1024);
#else
		busy_time += effective_cpu_util(cpu, util, ENERGY_UTIL, NULL);
#endif
	}

	eenv->pds_busy_time[pd_idx] = min(eenv->pds_cap[pd_idx], busy_time);
}

/*
 * Compute the maximum utilization for compute_energy() when the task @p
 * is placed on the cpu @dst_cpu.
 *
 * Returns the maximum utilization among @eenv->cpus. This utilization can't
 * exceed @eenv->cpu_cap.
 */
static inline unsigned long
eenv_pd_max_util(struct energy_env *eenv, struct cpumask *pd_cpus,
		 struct task_struct *p, int dst_cpu)
{
	unsigned long max_util = 0, gear_max_util = 0;
	int pd_idx = cpumask_first(pd_cpus);
	int cpu, dst_idx, pd_cpu = -1, gear_cpu = -1;

	for_each_cpu_and(cpu, get_gear_cpumask(eenv->gear_idx), cpu_active_mask) {
		struct task_struct *tsk = (cpu == dst_cpu) ? p : NULL;
		unsigned long util = -1, cpu_util = -1;

		dst_idx = (cpu == dst_cpu) ? 1 : 0;
		if (eenv->cpu_max_util[cpu][dst_idx] == -1) {
			util = mtk_cpu_util_next(cpu, p, dst_cpu, 1);

			/*
			 * Performance domain frequency: utilization clamping
			 * must be considered since it affects the selection
			 * of the performance domain frequency.
			 * NOTE: in case RT tasks are running, by default the
			 * FREQUENCY_UTIL's utilization can be max OPP.
			 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
			if (tsk)
				cpu_util = mtk_cpu_util(cpu, util, FREQUENCY_UTIL, tsk,
						eenv->min_cap, eenv->max_cap);
			else
				cpu_util = mtk_cpu_util(cpu, util, FREQUENCY_UTIL, tsk, 0, 1024);
#else
			cpu_util = effective_cpu_util(cpu, util, FREQUENCY_UTIL, tsk);
#endif
			eenv->cpu_max_util[cpu][dst_idx] = cpu_util;
		} else
			cpu_util = eenv->cpu_max_util[cpu][dst_idx];

		if (cpumask_test_cpu(cpu, pd_cpus)) {
			pd_cpu = (max_util < cpu_util) ? cpu : pd_cpu;
			max_util = max(max_util, cpu_util);
		}

		gear_cpu = (gear_max_util < cpu_util) ? cpu : gear_cpu;
		gear_max_util = max(gear_max_util, cpu_util);

		/*get dst_cpu base utilization*/
		if (cpu == dst_cpu) {
			unsigned long util_base = mtk_cpu_util_next(cpu, p, -1, 1);
			unsigned long cpu_util_base;

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
			cpu_util_base = mtk_cpu_util(cpu, util_base, FREQUENCY_UTIL, NULL,
					0, 1024);
#else
			cpu_util_base = effective_cpu_util(cpu, util, FREQUENCY_UTIL, tsk);
#endif
			eenv->cpu_max_util[cpu][0] = cpu_util_base;
			if (trace_sched_max_util_enabled())
				trace_sched_max_util("cpu", cpu, dst_cpu, 0,
					eenv->cpu_max_util[cpu][0], cpu, util_base, cpu_util_base);
		}

		if (trace_sched_max_util_enabled())
			trace_sched_max_util("cpu", cpu, dst_cpu, dst_idx,
				eenv->cpu_max_util[cpu][dst_idx], cpu, util, cpu_util);
	}

	dst_idx = (dst_cpu != -1) ? 1 : 0;
	if (trace_sched_max_util_enabled())
		trace_sched_max_util("pd", pd_idx, dst_cpu, dst_idx, max_util, pd_cpu, -1, -1);

	eenv->gear_max_util[eenv->gear_idx][dst_idx] = min(gear_max_util,
					eenv->pds_cpu_cap[pd_idx]);

	if (trace_sched_max_util_enabled()) {
		trace_sched_max_util("gear", eenv->gear_idx, dst_cpu, dst_idx,
			eenv->gear_max_util[eenv->gear_idx][dst_idx], gear_cpu, -1, -1);
	}

	return min(max_util, eenv->pds_cpu_cap[pd_idx]);
}

inline int reasonable_temp(int temp)
{
	if ((temp > 125) || (temp < -40))
		return 0;

	return 1;
}

bool dsu_pwr_enable;

void init_dsu_pwr_enable(void)
{
	dsu_pwr_enable = sched_dsu_pwr_enable_get();
}

inline bool is_dsu_pwr_concerned(int wl)
{
	if (wl == 4)
		return false;

	if (is_dpt_support_driver_hook) {
		if (is_dpt_support_driver_hook())
			return false;
	}

	return true;
}

inline bool is_dsu_pwr_triggered(int wl)
{
	return dsu_pwr_enable && is_dsu_pwr_concerned(wl);
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static inline void eenv_init(struct energy_env *eenv, struct task_struct *p,
			int prev_cpu, struct perf_domain *pd, bool in_irq)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned int cpu, pd_idx;
	struct perf_domain *pd_ptr = pd;

	eenv_task_busy_time(eenv, p, prev_cpu);

	for_each_cpu(cpu, cpu_possible_mask) {
		eenv->pds_busy_time[cpu] =  -1;
		eenv->cpu_max_util[cpu][0] =  -1;
		eenv->cpu_max_util[cpu][1] =  -1;
		eenv->gear_max_util[cpu][0] =  -1;
		eenv->gear_max_util[cpu][1] =  -1;
		eenv->pds_cpu_cap[cpu] = -1;
		eenv->pds_cap[cpu] = -1;
		eenv->pd_base_max_util[cpu] = 0;
		eenv->pd_base_freq[cpu] = 0;
	}

	eenv->wl_support = get_eas_dsu_ctrl();
	eenv->total_util = 0;

	/* get wl snapshot*/
	if (eenv->wl_support) {
		eenv->wl_cpu = get_em_wl();
		eenv->wl_dsu = get_wl_dsu();
	} else {
		eenv->wl_cpu = 0;
		eenv->wl_dsu = 0;
	}

	for (; pd_ptr; pd_ptr = pd_ptr->next) {
		unsigned long cpu_thermal_cap;
		unsigned long max_util, pd_freq;

		cpumask_and(cpus, perf_domain_span(pd_ptr), cpu_active_mask);
		if (cpumask_empty(cpus))
			continue;

		/* Account thermal pressure for the energy estimation */
		pd_idx = cpu = cpumask_first(cpus);
		eenv->gear_idx = topology_cluster_id(pd_idx);
		cpu_thermal_cap = arch_scale_cpu_capacity(cpu);
		/* copy arch_scale_thermal_pressure() code and add read_once to avoid data-racing */
		cpu_thermal_cap -= READ_ONCE(per_cpu(thermal_pressure, cpu));

		eenv->pds_cpu_cap[pd_idx] = cpu_thermal_cap;
		eenv->pds_cap[pd_idx] = 0;
		for_each_cpu(cpu, cpus) {
			eenv->pds_cap[pd_idx] += cpu_thermal_cap;
		}

		if (trace_sched_energy_init_enabled()) {
			trace_sched_energy_init(cpus, eenv->gear_idx, eenv->pds_cpu_cap[pd_idx],
				eenv->pds_cap[pd_idx]);
		}

		if (eenv->wl_support) {
			eenv_pd_busy_time(eenv, cpus, p);
			eenv->total_util += eenv->pds_busy_time[pd_idx];

			/* get dsu_freq_base by max_util voting */
			max_util = eenv_pd_max_util(eenv, cpus, p, -1);
			pd_freq = pd_get_util_cpufreq(eenv, cpus, max_util,
					eenv->pds_cpu_cap[pd_idx], arch_scale_cpu_capacity(pd_idx));
			eenv->pd_base_freq[pd_idx] = max(pd_freq, per_cpu(min_freq, pd_idx));
			eenv->pd_base_max_util[pd_idx] = max_util;
		}
	}

	if (unlikely(in_irq)) {
		return;
	}


	for_each_cpu(cpu, cpu_possible_mask) {
#if IS_ENABLED(CONFIG_MTK_LEAKAGE_AWARE_TEMP)
		eenv->cpu_temp[cpu] = get_cpu_temp(cpu);
		eenv->cpu_temp[cpu] /= 1000;
#else
		eenv->cpu_temp[cpu] = -1;
#endif
		if (!reasonable_temp(eenv->cpu_temp[cpu])) {
			if (trace_sched_check_temp_enabled())
				trace_sched_check_temp("cpu", cpu, eenv->cpu_temp[cpu]);
		}
	}

	if (eenv->wl_support) {
		unsigned int output[8] = {[0 ... 7] = -1};
		unsigned int val[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = -1};

		if (is_dsu_pwr_triggered(eenv->wl_dsu)) {
			eenv_dsu_init(eenv->android_vendor_data1, false, eenv->wl_dsu,
					PERCORE_L3_BW, cpu_active_mask->bits[0], eenv->pd_base_freq,
					val, output);
		}

		if (PERCORE_L3_BW) {
			unsigned int sum_val = 0;
			for_each_cpu(cpu, cpu_active_mask) {
				sum_val += val[cpu];
				if (trace_sched_per_core_BW_enabled())
					trace_sched_per_core_BW(cpu, val[cpu], sum_val);
			}
		}

		if (!reasonable_temp(output[0])) {
			if (trace_sched_check_temp_enabled())
				trace_sched_check_temp("dsu", -1, (int) output[0]);
		}

		if (trace_sched_eenv_init_enabled())
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
			trace_sched_eenv_init(output[1], output[2], output[6], output[7],
					output[3], output[4], output[5],
					share_buck.gear_idx);
#else
			trace_sched_eenv_init(output[1], output[2], output[6], output[7],
					0, output[4], output[5], share_buck.gear_idx);
#endif
	}
}

static inline unsigned long
mtk_compute_energy_cpu(struct energy_env *eenv, struct perf_domain *pd,
		       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{
	unsigned long pd_max_util = eenv_pd_max_util(eenv, pd_cpus, p, dst_cpu);
	unsigned long gear_max_util = -1;
	int pd_idx = cpumask_first(pd_cpus);
	unsigned long busy_time = eenv->pds_busy_time[pd_idx];
	unsigned long energy, extern_volt = -1;
	unsigned long dsu_volt = -1, pd_volt = -1, gear_volt = -1;
	int dst_idx, shared_buck_mode;
	unsigned long pd_freq = 0, gear_freq, scale_cpu;

	scale_cpu = arch_scale_cpu_capacity(pd_idx);

	if (dst_cpu >= 0)
		busy_time = min(eenv->pds_cap[pd_idx], busy_time + eenv->task_busy_time);

	if (pd_max_util == eenv->pd_base_max_util[pd_idx]) {
		pd_freq = eenv->pd_base_freq[pd_idx];
	} else {
		pd_freq = pd_get_util_cpufreq(eenv, pd_cpus, pd_max_util,
				eenv->pds_cpu_cap[pd_idx], scale_cpu);
	}

	if (eenv->wl_support && is_dsu_pwr_triggered(eenv->wl_dsu)) {
		dsu_volt = update_dsu_status(eenv, false, pd_freq, pd_idx, dst_cpu);

		if (share_buck.gear_idx != eenv->gear_idx)
			dsu_volt = 0;
	} else {
		dsu_volt = 0;
	}

	/* dvfs power overhead */
	if (!cpumask_equal(pd_cpus, get_gear_cpumask(eenv->gear_idx))) {
		/* dvfs Vin/Vout */
		pd_volt = pd_get_freq_volt(pd_idx, pd_freq, false, eenv->wl_cpu);

		dst_idx = (dst_cpu >= 0) ? 1 : 0;
		gear_max_util = eenv->gear_max_util[eenv->gear_idx][dst_idx];
		gear_freq = pd_get_util_cpufreq(eenv, pd_cpus, gear_max_util,
				eenv->pds_cpu_cap[pd_idx], scale_cpu);
		gear_volt = pd_get_freq_volt(pd_idx, gear_freq, false, eenv->wl_cpu);

		if (gear_volt-pd_volt < volt_diff) {
			extern_volt = max(gear_volt, dsu_volt);
			energy =  mtk_em_cpu_energy(pd->em_pd, pd_freq, busy_time,
					scale_cpu, eenv, extern_volt, pd_max_util);
			shared_buck_mode = 1;
		} else {
			extern_volt = 0;
			energy =  mtk_em_cpu_energy(pd->em_pd, pd_freq, busy_time,
					scale_cpu, eenv, extern_volt, pd_max_util);
			energy = ((pd_volt) ? energy * max(gear_volt, dsu_volt) / pd_volt : energy);
			shared_buck_mode = 2;
		}
	} else {
		extern_volt = dsu_volt;
		energy =  mtk_em_cpu_energy(pd->em_pd, pd_freq, busy_time,
				scale_cpu, eenv, extern_volt, pd_max_util);
		shared_buck_mode = 0;
	}

	if (trace_sched_compute_energy_enabled())
		trace_sched_compute_energy(dst_cpu, pd_idx, pd_cpus, energy, shared_buck_mode,
			gear_max_util, pd_max_util, busy_time,
			gear_volt, pd_volt, dsu_volt, extern_volt);

	return energy;
}

struct share_buck_info share_buck;
int get_share_buck(void)
{
	return share_buck.gear_idx;
}
EXPORT_SYMBOL_GPL(get_share_buck);

int init_share_buck(void)
{
	int ret;
	struct device_node *eas_node;

	/* Default share buck gear_idx=0 */
	share_buck.gear_idx = 0;
	eas_node = of_find_node_by_name(NULL, "eas-info");
	if (eas_node == NULL)
		pr_info("failed to find node @ %s\n", __func__);
	else {
		ret = of_property_read_u32(eas_node, "share-buck", &share_buck.gear_idx);
		if (ret < 0)
			pr_info("no share_buck err_code=%d %s\n", ret,  __func__);
	}

	share_buck.cpus = get_gear_cpumask(share_buck.gear_idx);


	return 0;
}

static inline int shared_gear(int gear_idx)
{
	return gear_idx == share_buck.gear_idx;
}

/*
 * compute_energy(): Use the Energy Model to estimate the energy that @pd would
 * consume for a given utilization landscape @eenv. When @dst_cpu < 0, the task
 * contribution is ignored.
 */
static inline unsigned long
mtk_compute_energy(struct energy_env *eenv, struct perf_domain *pd,
	       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{
	unsigned long cpu_pwr = 0, dsu_pwr = 0;
	unsigned long shared_pwr = 0, shared_pwr_dvfs = 0;
	unsigned int gear_idx;
	int dst_idx;
	int pd_idx = cpumask_first(pd_cpus);
	unsigned long total_util;
	unsigned long share_buck_freq;
	unsigned long dsu_extern_volt = 0;

	cpu_pwr = mtk_compute_energy_cpu(eenv, pd, pd_cpus, p, dst_cpu);

	/* indirect dvfs power overhead (when gear_max_util changes)*/
	if (!cpumask_equal(pd_cpus, get_gear_cpumask(eenv->gear_idx)) &&
		eenv->gear_max_util[eenv->gear_idx][1] > eenv->gear_max_util[eenv->gear_idx][0]) {
		struct root_domain *rd = this_rq()->rd;
		struct perf_domain *pd_ptr;

		rcu_read_lock();
		pd_ptr = rcu_dereference(rd->pd);
		for (; pd_ptr; pd_ptr = pd_ptr->next) {
			struct cpumask *pd_mask = perf_domain_span(pd_ptr);
			unsigned int cpu = cpumask_first(pd_mask);

			if (cpu != dst_cpu && cpumask_test_cpu(cpu,
						get_gear_cpumask(eenv->gear_idx))) {
				gear_idx = eenv->gear_idx;
				eenv->gear_idx = topology_cluster_id(cpu);
				if (dst_cpu >= 0)
					shared_pwr_dvfs += mtk_compute_energy_cpu(eenv, pd_ptr,
									pd_mask, p, -2);
				else
					shared_pwr_dvfs += mtk_compute_energy_cpu(eenv, pd_ptr,
									pd_mask, p, -1);
				eenv->gear_idx = gear_idx;
			}
		}
		rcu_read_unlock();
	}

	if (!eenv->wl_support)
		goto done;

	/* calc indirect DSU share_buck */

	if (is_dsu_pwr_triggered(eenv->wl_dsu)) {
		if ((share_buck.gear_idx != -1) && !(shared_gear(eenv->gear_idx))
				&& dsu_freq_changed(eenv->android_vendor_data1)) {
			struct root_domain *rd = this_rq()->rd;
			struct perf_domain *pd_ptr, *share_buck_pd = 0;

			rcu_read_lock();
			pd_ptr = rcu_dereference(rd->pd);
			if (!pd_ptr)
				goto calc_sharebuck_done;

			for (; pd_ptr; pd_ptr = pd_ptr->next) {
				struct cpumask *pd_mask = perf_domain_span(pd_ptr);
				unsigned int cpu = cpumask_first(pd_mask);

				if (share_buck.gear_idx == topology_cluster_id(cpu)) {
					share_buck_pd = pd_ptr;
					break;
				}
			}

			if (!share_buck_pd)
				goto calc_sharebuck_done;

			/* calculate share_buck gear pwr with new DSU freq */
			gear_idx = eenv->gear_idx;
			eenv->gear_idx = share_buck.gear_idx;

			if (dst_cpu >= 0)
				shared_pwr = mtk_compute_energy_cpu(eenv, share_buck_pd,
								share_buck.cpus, p, -2);
			else
				shared_pwr = mtk_compute_energy_cpu(eenv, share_buck_pd,
								share_buck.cpus, p, -1);

			eenv->gear_idx = gear_idx;

calc_sharebuck_done:
			rcu_read_unlock();
		}
	}

	/* calc DSU power */
	if (dst_cpu >= 0 && (shared_gear(eenv->gear_idx))) {
		dst_idx = 1;
	} else {
		dst_idx = 0;
	}

	if ((share_buck.gear_idx != -1)) {
		gear_idx = eenv->gear_idx;
		eenv->gear_idx = share_buck.gear_idx;
		pd_idx = cpumask_first(share_buck.cpus);
		if (eenv->gear_max_util[share_buck.gear_idx][dst_idx] == eenv->pd_base_max_util[pd_idx]) {
			share_buck_freq = eenv->pd_base_freq[pd_idx];
		} else {
			share_buck_freq = pd_get_util_cpufreq(eenv, pd_cpus,
					eenv->gear_max_util[share_buck.gear_idx][dst_idx],
					eenv->pds_cpu_cap[pd_idx], arch_scale_cpu_capacity(pd_idx));
		}
		dsu_extern_volt = pd_get_freq_volt(cpumask_first(share_buck.cpus),
				share_buck_freq, false, eenv->wl_dsu);
		eenv->gear_idx = gear_idx;
	}

	if (PERCORE_L3_BW)
		total_util = eenv->cpu_max_util[eenv->dst_cpu][0];
	else
		total_util = eenv->total_util;

	dsu_pwr = get_dsu_pwr(eenv->wl_dsu, dst_cpu, eenv->task_busy_time, total_util,
			eenv->android_vendor_data1, dsu_extern_volt, is_dsu_pwr_triggered(eenv->wl_dsu));

done:
	if (trace_sched_compute_energy_cpu_dsu_enabled())
		trace_sched_compute_energy_cpu_dsu(dst_cpu, eenv->wl_cpu, cpu_pwr, shared_pwr_dvfs,
					shared_pwr, dsu_pwr, cpu_pwr + shared_pwr + dsu_pwr);

	return cpu_pwr + shared_pwr_dvfs + shared_pwr + dsu_pwr;
}
#endif

static unsigned int uclamp_min_ls;
void set_uclamp_min_ls(unsigned int val)
{
	uclamp_min_ls = val;
}
EXPORT_SYMBOL_GPL(set_uclamp_min_ls);

unsigned int get_uclamp_min_ls(void)
{
	return uclamp_min_ls;
}
EXPORT_SYMBOL_GPL(get_uclamp_min_ls);

/*
 * attach_task() -- attach the task detached by detach_task() to its new rq.
 */
static void attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	attach_task(rq, p);
	rq_unlock(rq, &rf);
}

#if IS_ENABLED(CONFIG_MTK_EAS)
static void __sched_fork_init(struct task_struct *p)
{
	struct soft_affinity_task *sa_task = &((struct mtk_task *)
		p->android_vendor_data1)->sa_task;

	sa_task->latency_sensitive = false;
	cpumask_copy(&sa_task->soft_cpumask, cpu_possible_mask);
}

static void android_rvh_sched_fork_init(void *unused, struct task_struct *p)
{
	__sched_fork_init(p);
}

void init_task_soft_affinity(void)
{
	struct task_struct *g, *p;
	int ret;

	/* init soft affinity related value to exist tasks */
	read_lock(&tasklist_lock);
	for_each_process_thread(g, p) {
		__sched_fork_init(p);
	}
	read_unlock(&tasklist_lock);

	/* init soft affinity related value to newly forked tasks */
	ret = register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	if (ret)
		pr_info("register sched_fork_init hooks failed, returned %d\n", ret);
}

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

void _init_tg_mask(struct cgroup_subsys_state *css)
{
	struct task_group *tg = css_tg(css);
	struct soft_affinity_tg *sa_tg = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg;

	cpumask_copy(&sa_tg->soft_cpumask, cpu_possible_mask);
}

void init_tg_soft_affinity(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	/* init soft affinity related value to exist cgroups */
	rcu_read_lock();
	_init_tg_mask(&root_task_group.css);
	css_for_each_child(css, top_css)
		_init_tg_mask(css);
	rcu_read_unlock();

	/* init soft affinity related value to newly created cgroups in vip.c*/
}

void soft_affinity_init(void)
{
	init_task_soft_affinity();
	init_tg_soft_affinity();
}

void __set_group_prefer_cpus(struct task_group *tg, unsigned int cpumask_val)
{
	struct cpumask *tg_mask;
	unsigned long cpumask_ulval = cpumask_val;

	tg_mask = &(((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask);
	cpumask_copy(tg_mask, to_cpumask(&cpumask_ulval));
}

struct task_group *search_tg_by_name(char *group_name)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	int ret = 0;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		if (!strcmp(css->cgroup->kn->name, group_name)) {
			ret = 1;
			break;
		}
	rcu_read_unlock();

	if (ret)
		return css_tg(css);

	return &root_task_group;
}

void set_group_prefer_cpus_by_name(unsigned int cpumask_val, char *group_name)
{
	struct task_group *tg = search_tg_by_name(group_name);

	if (tg == &root_task_group)
		return;

	__set_group_prefer_cpus(tg, cpumask_val);
}

void get_task_group_cpumask_by_name(char *group_name, struct cpumask *__tg_mask)
{
	struct task_group *tg = search_tg_by_name(group_name);
	struct cpumask *tg_mask;

	tg_mask = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask;
	cpumask_copy(__tg_mask, tg_mask);
}

struct task_group *search_tg_by_cpuctl_id(unsigned int cpuctl_id)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	int ret = 0;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		if (css->id == cpuctl_id) {
			ret = 1;
			break;
		}
	rcu_read_unlock();

	if (ret)
		return css_tg(css);

	return &root_task_group;
}

/* start of soft affinity interface */
int set_group_prefer_cpus(unsigned int cpuctl_id, unsigned int cpumask_val)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);

	if (tg == &root_task_group)
		return 0;

	__set_group_prefer_cpus(tg, cpumask_val);
	return 1;
}
EXPORT_SYMBOL_GPL(set_group_prefer_cpus);

void set_top_app_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "top-app");
}
EXPORT_SYMBOL_GPL(set_top_app_cpumask);

void set_foreground_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "foreground");
}
EXPORT_SYMBOL_GPL(set_foreground_cpumask);

void set_background_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "background");
}
EXPORT_SYMBOL_GPL(set_background_cpumask);

void get_group_prefer_cpus(unsigned int cpuctl_id, struct cpumask *__tg_mask)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);
	struct cpumask *tg_mask;

	tg_mask = &(((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask);
	cpumask_copy(__tg_mask, tg_mask);
}
EXPORT_SYMBOL_GPL(get_group_prefer_cpus);

struct cpumask top_app_cpumask;
struct cpumask foreground_cpumask;
struct cpumask background_cpumask;
struct cpumask *get_top_app_cpumask(void)
{
	get_task_group_cpumask_by_name("top-app", &top_app_cpumask);
	return &top_app_cpumask;
}
EXPORT_SYMBOL_GPL(get_top_app_cpumask);

struct cpumask *get_foreground_cpumask(void)
{
	get_task_group_cpumask_by_name("foreground", &foreground_cpumask);
	return &foreground_cpumask;
}
EXPORT_SYMBOL_GPL(get_foreground_cpumask);

struct cpumask *get_background_cpumask(void)
{
	get_task_group_cpumask_by_name("background", &background_cpumask);
	return &background_cpumask;
}
EXPORT_SYMBOL_GPL(get_background_cpumask);

void set_task_ls(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = true;
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_task_ls);

void unset_task_ls(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = false;
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_task_ls);

void set_task_ls_prefer_cpus(int pid, unsigned int cpumask_val)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;
	unsigned long cpumask_ulval = cpumask_val;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = true;
		cpumask_copy(&sa_task->soft_cpumask, to_cpumask(&cpumask_ulval));
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_task_ls_prefer_cpus);

void unset_task_ls_prefer_cpus(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;
	unsigned long cpumask_ulval = 0xff;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = false;
		cpumask_copy(&sa_task->soft_cpumask, to_cpumask(&cpumask_ulval));
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_task_ls_prefer_cpus);
/* end of soft affinity interface */

#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
static inline bool cloned_uclamp_latency_sensitive(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	struct task_group *tg;

	if (!css)
		return false;
	tg = container_of(css, struct task_group, css);

	return tg->latency_sensitive;
}
#else
static inline bool cloned_uclamp_latency_sensitive(struct task_struct *p)
{
	return false;
}
#endif /* CONFIG_UCLAMP_TASK_GROUP */

bool is_task_ls_uclamp(struct task_struct *p)
{
	bool latency_sensitive = false;

	if (!uclamp_min_ls)
		latency_sensitive = cloned_uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					cloned_uclamp_latency_sensitive(p);
	}
	return latency_sensitive;
}

inline bool is_task_latency_sensitive(struct task_struct *p)
{
	struct soft_affinity_task *sa_task;
	bool latency_sensitive = false;

	sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
	rcu_read_lock();
	latency_sensitive = sa_task->latency_sensitive || is_task_ls_uclamp(p);
	rcu_read_unlock();

	return latency_sensitive;
}
EXPORT_SYMBOL_GPL(is_task_latency_sensitive);

inline void compute_effective_softmask(struct task_struct *p,
		bool *latency_sensitive, struct cpumask *dst_mask)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct cpumask *task_mask;
	struct cpumask *tg_mask;
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	bool low_prio_task = false;

	if (sysctl_prio_control && (p->prio >= sysctl_prio_control))
		low_prio_task = true;
#endif
	*latency_sensitive = is_task_latency_sensitive(p);
	css = task_css(p, cpu_cgrp_id);
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	if (!low_prio_task && (!*latency_sensitive || !css)) {
#else
	if (!*latency_sensitive || !css) {
#endif
		cpumask_copy(dst_mask, cpu_possible_mask);
		return;
	}

	tg = container_of(css, struct task_group, css);
	tg_mask = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask;

	task_mask = &((struct mtk_task *) p->android_vendor_data1)->sa_task.soft_cpumask;

#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	if (low_prio_task) {
		cpumask_andnot(task_mask, task_mask, &bcpus);
	}
#endif
	if (!cpumask_and(dst_mask, task_mask, tg_mask)) {
		cpumask_copy(dst_mask, tg_mask);
		return;
	}
}

void mtk_can_migrate_task(void *data, struct task_struct *p,
	int dst_cpu, int *can_migrate)
{
	bool latency_sensitive;
	struct cpumask eff_mask;
	int src_cpu = task_cpu(p), num_vip_src, num_vip_dst;

	if (!get_eas_hook())
		return;

	if (cpu_paused(dst_cpu)) {
		*can_migrate = false;
		return;
	}

	if (task_is_vip(p, VVIP)) {
		num_vip_src = num_vip_in_cpu(src_cpu, VVIP);
		num_vip_dst = num_vip_in_cpu(dst_cpu, VVIP);
		if (num_vip_src-1 < num_vip_dst) {
			*can_migrate = 0;
			return;
		} else if ((num_vip_src-1 == num_vip_dst) &&
			(capacity_orig_of(src_cpu) > capacity_orig_of(dst_cpu))) {
			*can_migrate = 0;
			return;
		}
	}

	if (READ_ONCE(cpu_rq(src_cpu)->rd->overutilized)) {
		*can_migrate = 1;
		return;
	}

	compute_effective_softmask(p, &latency_sensitive, &eff_mask);
	if (latency_sensitive && !(cpumask_test_cpu(dst_cpu, &eff_mask)))
		*can_migrate = 0;
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	if (sysctl_prio_control && (p->prio >= sysctl_prio_control) &&
		!(cpumask_test_cpu(dst_cpu, &eff_mask)))
		*can_migrate = 0;
#endif
}

__read_mostly int num_sched_clusters;
cpumask_t __read_mostly ***cpu_array;

void init_cpu_array(void)
{
	int i, j;
	num_sched_clusters = get_nr_gears();

	cpu_array = kcalloc(num_sched_clusters, sizeof(cpumask_t **),
			GFP_ATOMIC | __GFP_NOFAIL);
	if (!cpu_array)
		free_cpu_array();

	for (i = 0; i < num_sched_clusters; i++) {
		cpu_array[i] = kcalloc(num_sched_clusters, sizeof(cpumask_t *),
				GFP_ATOMIC | __GFP_NOFAIL);
		if (!cpu_array[i])
			free_cpu_array();

		for (j = 0; j < num_sched_clusters; j++) {
			cpu_array[i][j] = kcalloc(2, sizeof(cpumask_t),
					GFP_ATOMIC | __GFP_NOFAIL);
			if (!cpu_array[i][j])
				free_cpu_array();
		}
	}
}

void build_cpu_array(void)
{
	int i;

	if (!cpu_array)
		free_cpu_array();

	/* Construct cpu_array row by row */
	for (i = 0; i < num_sched_clusters; i++) {
		int j, k = 1;

		/* Fill out first column with appropriate cpu arrays */
		cpumask_copy(&cpu_array[i][0][0], get_gear_cpumask(i));
		cpumask_copy(&cpu_array[i][0][1], get_gear_cpumask(i));
		/*
		 * k starts from column 1 because 0 is filled
		 * Fill clusters for the rest of the row,
		 * above i in ascending order
		 */
		for (j = i + 1; j < num_sched_clusters; j++) {
			cpumask_copy(&cpu_array[i][k][0], get_gear_cpumask(j));
			cpumask_copy(&cpu_array[i][j][1], get_gear_cpumask(j));
			k++;
		}

		/*
		 * k starts from where we left off above.
		 * Fill cluster below i in descending order.
		 */
		for (j = i - 1; j >= 0; j--) {
			cpumask_copy(&cpu_array[i][k][0], get_gear_cpumask(j));
			cpumask_copy(&cpu_array[i][i-j][1], get_gear_cpumask(j));
			k++;
		}
	}
}

void free_cpu_array(void)
{
	int i, j;

	if (!cpu_array)
		return;

	for (i = 0; i < num_sched_clusters; i++) {
		for (j = 0; j < num_sched_clusters; j++) {
			kfree(cpu_array[i][j]);
			cpu_array[i][j] = NULL;
		}
		kfree(cpu_array[i]);
		cpu_array[i] = NULL;
	}
	kfree(cpu_array);
	cpu_array = NULL;
}

static int mtk_wake_affine_idle(int this_cpu, int prev_cpu, int sync)
{
	if (available_idle_cpu(this_cpu) && mtk_cpus_share_cache(this_cpu, prev_cpu))
		return available_idle_cpu(prev_cpu) ? prev_cpu : this_cpu;

	if (sync && cpu_rq(this_cpu)->nr_running == 1)
		return this_cpu;

	if (available_idle_cpu(prev_cpu))
		return prev_cpu;

	return nr_cpumask_bits;
}

static inline unsigned long cfs_rq_load_avg(struct cfs_rq *cfs_rq)
{
	return cfs_rq->avg.load_avg;
}

static unsigned long cpu_load(struct rq *rq)
{
	return cfs_rq_load_avg(&rq->cfs);
}

#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
static void update_cfs_rq_h_load(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);
	struct sched_entity *se = cfs_rq->tg->se[cpu_of(rq)];
	unsigned long now = jiffies;
	unsigned long load;

	if (cfs_rq->last_h_load_update == now)
		return;

	WRITE_ONCE(cfs_rq->h_load_next, NULL);
	for (; se; se = se->parent) {
		cfs_rq = cfs_rq_of(se);
		WRITE_ONCE(cfs_rq->h_load_next, se);
		if (cfs_rq->last_h_load_update == now)
			break;
	}

	if (!se) {
		cfs_rq->h_load = cfs_rq_load_avg(cfs_rq);
		cfs_rq->last_h_load_update = now;
	}

	while ((se = READ_ONCE(cfs_rq->h_load_next)) != NULL) {
		load = cfs_rq->h_load;
		load = div64_ul(load * se->avg.load_avg,
				cfs_rq_load_avg(cfs_rq) + 1);
		cfs_rq = group_cfs_rq(se);
		cfs_rq->h_load = load;
		cfs_rq->last_h_load_update = now;
	}
}

unsigned long task_h_load(struct task_struct *p)
{
	struct cfs_rq *cfs_rq = task_cfs_rq(p);

	update_cfs_rq_h_load(cfs_rq);
	return div64_ul(p->se.avg.load_avg * cfs_rq->h_load,
			cfs_rq_load_avg(cfs_rq) + 1);
}
#else
static unsigned long task_h_load(struct task_struct *p)
{
	return p->se.avg.load_avg;
}
#endif

static int mtk_wake_affine_weight(struct task_struct *p, int this_cpu, int prev_cpu, int sync)
{
	s64 this_eff_load, prev_eff_load;
	unsigned long task_load;
	//struct sched_domain *sd = NULL;

	/* since "sched_domain_mutex undefined" build error, comment search sched_domain */
	/* find least shared sched_domain for this_cpu and prev_cpu */
	//for_each_domain(this_cpu, sd) {
	//	if ((sd->flags & SD_WAKE_AFFINE) &&
	//		cpumask_test_cpu(prev_cpu, sched_domain_span(sd)))
	//		break;
	//}
	//if (unlikely(!sd))
	//	return nr_cpumask_bits;

	this_eff_load = cpu_load(cpu_rq(this_cpu));

	if (sync) {
		unsigned long current_load = task_h_load(current);

		if (current_load > this_eff_load)
			return this_cpu;

		this_eff_load -= current_load;
	}

	task_load = task_h_load(p);

	this_eff_load += task_load;
	if (sched_feat(WA_BIAS))
		this_eff_load *= 100;
	this_eff_load *= capacity_of(prev_cpu);

	prev_eff_load = cpu_load(cpu_rq(prev_cpu));
	prev_eff_load -= task_load;

	/* since "sched_domain_mutex undefined" build error, replace imbalance_pct with its val 117
	 * prev_eff_load *= 100 + (sd->imbalance_pct - 100) / 2;
	 */
	if (sched_feat(WA_BIAS))
		prev_eff_load *= 100 + (117 - 100) / 2;
	prev_eff_load *= capacity_of(this_cpu);

	/*
	 * If sync, adjust the weight of prev_eff_load such that if
	 * prev_eff == this_eff that select_idle_sibling() will consider
	 * stacking the wakee on top of the waker if no other CPU is
	 * idle.
	 */
	if (sync)
		prev_eff_load += 1;

	return this_eff_load < prev_eff_load ? this_cpu : nr_cpumask_bits;
}

static int mtk_wake_affine(struct task_struct *p, int this_cpu, int prev_cpu, int sync)
{
	int target = nr_cpumask_bits;

	if (sched_feat(WA_IDLE))
		target = mtk_wake_affine_idle(this_cpu, prev_cpu, sync);

	if (sched_feat(WA_WEIGHT) && target == nr_cpumask_bits)
		target = mtk_wake_affine_weight(p, this_cpu, prev_cpu, sync);

	if (target == nr_cpumask_bits)
		return prev_cpu;

	return target;
}

/*
 * Scan the asym_capacity domain for idle CPUs; pick the first idle one on whi
 * the task fits. If no CPU is big enough, but there are idle ones, try to
 * maximize capacity.
 */
static int
mtk_select_idle_capacity(struct task_struct *p, struct cpumask *allowed_cpumask, int target,
	bool is_vip)
{
	unsigned long task_util, best_cap = 0;
	int cpu, best_cpu = -1;

	task_util = uclamp_task_util(p);

	for_each_cpu_wrap(cpu, allowed_cpumask, target) {
		unsigned long cpu_cap = capacity_of(cpu);

		if (!is_vip && (!available_idle_cpu(cpu) && !sched_idle_cpu(cpu)))
			continue;
		if (fits_capacity(task_util, cpu_cap, get_adaptive_margin(cpu)))
			return cpu;

		if (cpu_cap > best_cap) {
			best_cap = cpu_cap;
			best_cpu = cpu;
		}
	}

	return best_cpu;
}

static struct cpumask bcpus;
static unsigned long util_Th;

void get_most_powerful_pd_and_util_Th(void)
{
	unsigned int nr_gear = get_nr_gears();

	/* no mutliple pd */
	if (nr_gear <= 1) {
		util_Th = 0;
		cpumask_clear(&bcpus);
		return;
	}

	/* pd_capacity_tbl is sorted by ascending order,
	 * so nr_gear-1 is most powerful gear and
	 * nr_gear is the second powerful gear.
	 */
	cpumask_copy(&bcpus, get_gear_cpumask(nr_gear-1));
	/* threshold is set to large capacity in mcpus */
	util_Th = (pd_get_opp_capacity(
		cpumask_first(get_gear_cpumask(nr_gear-2)), 0) >> 2);

}

bool gear_hints_enable;
static inline bool task_can_skip_this_cpu(struct task_struct *p, unsigned long p_uclamp_min,
		bool latency_sensitive, int cpu, struct cpumask *bcpus, int vip_prio)
{
	bool cpu_in_bcpus;
	unsigned long task_util;
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	if (prio_is_vip(vip_prio, NOT_VIP))
		return 0;

	if (latency_sensitive)
		return 0;

	if (gear_hints_enable &&
		ghts->gear_start >= 0 &&
		ghts->gear_start <= num_sched_clusters-1)
		return 0;

	if (p_uclamp_min > 0)
		return 0;

	if (cpumask_empty(bcpus))
		return 0;

	if (cpumask_weight(p->cpus_ptr) < 8)
		return 0;

	cpu_in_bcpus = cpumask_test_cpu(cpu, bcpus);
	task_util = task_util_est(p);
	if (!cpu_in_bcpus || !fits_capacity(task_util, util_Th, get_adaptive_margin(cpu)))
		return 0;

	return 1;
}

static inline bool is_target_max_spare_cpu(long spare_cap, long target_max_spare_cap,
			int best_cpu, int new_cpu, const char *type)
{
	bool replace = true;

	if (spare_cap <= target_max_spare_cap)
		replace = false;

	if (trace_sched_target_max_spare_cpu_enabled())
		trace_sched_target_max_spare_cpu(type, best_cpu, new_cpu, replace,
			spare_cap, target_max_spare_cap);

	return replace;
}

void init_gear_hints(void)
{
	gear_hints_enable = sched_gear_hints_enable_get();
}

void __set_gear_indices(struct task_struct *p, int gear_start, int num_gear, int reverse)
{
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	ghts->gear_start = gear_start;
	ghts->num_gear   = num_gear;
	ghts->reverse    = reverse;
}

int set_gear_indices(int pid, int gear_start, int num_gear, int reverse)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	/* check gear_start validity */
	if (gear_start < -1 || gear_start > num_sched_clusters-1)
		goto done;

	/* check num_gear validity */
	if ((num_gear != -1 && num_gear < 1) || num_gear > num_sched_clusters)
		goto done;

	/* check num_gear validity */
	if (reverse < 0 || reverse > 1)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__set_gear_indices(p, gear_start, num_gear, reverse);
		put_task_struct(p);
		ret = 1;
	}

	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_gear_indices);

int unset_gear_indices(int pid)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__set_gear_indices(p, GEAR_HINT_UNSET, GEAR_HINT_UNSET, 0);
		put_task_struct(p);
		ret = 1;
	}
	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_gear_indices);

void __get_gear_indices(struct task_struct *p, int *gear_start, int *num_gear, int *reverse)
{
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	*gear_start = ghts->gear_start;
	*num_gear = ghts->num_gear;
	*reverse = ghts->reverse;
}

int get_gear_indices(int pid, int *gear_start, int *num_gear, int *reverse)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__get_gear_indices(p, gear_start, num_gear, reverse);
		put_task_struct(p);
		ret = 1;
	}
	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(get_gear_indices);

#if IS_ENABLED(CONFIG_MTK_SCHED_UPDOWN_MIGRATE)

bool updown_migration_enable;
void init_updown_migration(void)
{
	updown_migration_enable = sched_updown_migration_enable_get();
}

/* Default Migration margin */
bool adaptive_margin_enabled[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = true /* default on */
};
unsigned int sched_capacity_up_margin[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = 1024 /* ~0% margin */
};
unsigned int sched_capacity_down_margin[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = 1024 /* ~0% margin */
};

int set_updown_migration_pct(int gear_idx, int dn_pct, int up_pct)
{
	int ret = 0, cpu;
	struct cpumask *cpus;

	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	/* check pct validity */
	if (dn_pct < 1 || dn_pct > 100)
		goto done;

	if (up_pct < 1 || up_pct > 100)
		goto done;

	if (dn_pct > up_pct)
		goto done;

	cpus = get_gear_cpumask(gear_idx);
	for_each_cpu(cpu, cpus) {
		sched_capacity_up_margin[cpu] =
			SCHED_CAPACITY_SCALE * 100 / up_pct;
		sched_capacity_down_margin[cpu] =
			SCHED_CAPACITY_SCALE * 100 / dn_pct;
		adaptive_margin_enabled[cpu] = false;
	}
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_updown_migration_pct);

int unset_updown_migration_pct(int gear_idx)
{
	int ret = 0, cpu;
	struct cpumask *cpus;

	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	cpus = get_gear_cpumask(gear_idx);
	for_each_cpu(cpu, cpus) {
		sched_capacity_up_margin[cpu] = 1024;
		sched_capacity_down_margin[cpu] = 1024;
		adaptive_margin_enabled[cpu] = true;
	}
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_updown_migration_pct);

int get_updown_migration_pct(int gear_idx, int *dn_pct, int *up_pct)
{
	int ret = 0, cpu;

	*dn_pct = *up_pct = -1;
	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	cpu = cpumask_first(get_gear_cpumask(gear_idx));

	*dn_pct = SCHED_CAPACITY_SCALE * 100 / sched_capacity_down_margin[cpu];
	*up_pct = SCHED_CAPACITY_SCALE * 100 / sched_capacity_up_margin[cpu];

	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(get_updown_migration_pct);

static inline bool task_demand_fits(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);
	unsigned int margin;
	bool AM_enabled;
	unsigned int sugov_margin;
	unsigned long dst_capacity = capacity_orig_of(dst_cpu);
	unsigned long src_capacity = capacity_orig_of(src_cpu);

	if (dst_capacity == SCHED_CAPACITY_SCALE)
		return true;

	/* if updown_migration is not enabled */
	if (!updown_migration_enable)
		return task_fits_capacity(p, dst_capacity, get_adaptive_margin(dst_cpu));

	/*
	 * Derive upmigration/downmigration margin wrt the src/dst CPU.
	 */
	if (src_capacity > dst_capacity) {
		margin = sched_capacity_down_margin[dst_cpu];
		AM_enabled = adaptive_margin_enabled[dst_cpu];
	} else {
		margin = sched_capacity_up_margin[src_cpu];
		AM_enabled = adaptive_margin_enabled[src_cpu];
	}

	/* bypass adaptive margin if capacity_updown_margin is enabled */
	sugov_margin = AM_enabled ? get_adaptive_margin(dst_cpu) : SCHED_CAPACITY_SCALE;

	return (dst_capacity * SCHED_CAPACITY_SCALE * SCHED_CAPACITY_SCALE
				> uclamp_task_util(p) * margin * sugov_margin);
}

/* util_fits_cpu: return fit status to check if util fits capacity.
 * fit = 1 : fit when considering PELT & uclamp & thermal
 * fit = -1: fit when considering PELT, not fit if ulcamp min > cap influenced by thermal(capacity_orig_thermal)
 * fit = 0 : not fit when considering PELT
 *
 * util : util before considering uclamp.
 * capacity: expect this capacity is already thermal-awared.
 */
inline int util_fits_capacity(unsigned long util, unsigned long uclamp_min,
	unsigned long uclamp_max, unsigned long capacity, int cpu)
{
	unsigned long ceiling, cap_after_ceiling;
	bool AM_enabled = adaptive_margin_enabled[cpu];
	unsigned int sugov_margin = AM_enabled ? get_adaptive_margin(cpu) : SCHED_CAPACITY_SCALE;
	unsigned long capacity_orig_thermal, capacity_orig = capacity_orig_of(cpu);
	int fit, uclamp_max_fits, uclamp_involve;


	uclamp_min = clamp((uclamp_min * DEFAULT_MARGIN) >> SCHED_FIXEDPOINT_SHIFT,
		0UL, (unsigned long) SCHED_CAPACITY_SCALE);
	uclamp_max = clamp((uclamp_max * DEFAULT_MARGIN) >> SCHED_FIXEDPOINT_SHIFT,
		0UL, (unsigned long) SCHED_CAPACITY_SCALE);

	uclamp_involve = mtk_uclamp_involve(uclamp_min, uclamp_max, true);
	if (uclamp_involve)
		sugov_margin = DEFAULT_MARGIN;

	/* ceiling shouldn't affect capacity since updown_migration is not enabled,  */
	if (!updown_migration_enable)
		ceiling = SCHED_CAPACITY_SCALE;
	else
		ceiling = SCHED_CAPACITY_SCALE * capacity_orig_of(cpu) / sched_capacity_up_margin[cpu];

	/* Whether PELT fit after considering up-down migration ? */
	cap_after_ceiling = min(ceiling, capacity);
	fit = fits_capacity(util, cap_after_ceiling, sugov_margin);

	/* Change fit status from 0 to 1 only if uclamp max restrict util. */
	uclamp_max_fits = (capacity_orig == SCHED_CAPACITY_SCALE) && (uclamp_max == SCHED_CAPACITY_SCALE);
	uclamp_max_fits = !uclamp_max_fits && (uclamp_max <= capacity_orig);
	fit = fit || uclamp_max_fits;

	/* Change fit status from 1 to -1 only if uclamp min raise util. */
	uclamp_min = min(uclamp_min, uclamp_max);
	capacity_orig_thermal = min(cap_after_ceiling, (capacity_orig - arch_scale_thermal_pressure(cpu)));
	if (fit && (util < uclamp_min) && (uclamp_min > capacity_orig_thermal))
		fit = -1;

	if (trace_sched_fits_cap_ceiling_enabled())
		trace_sched_fits_cap_ceiling(fit, cpu, util, uclamp_min, uclamp_max, capacity, ceiling, sugov_margin,
			sched_capacity_down_margin[cpu], sched_capacity_up_margin[cpu], AM_enabled, uclamp_involve);

	return fit;
}

#else
static inline bool task_demand_fits(struct task_struct *p, int cpu)
{
	unsigned long capacity = capacity_orig_of(cpu);

	if (capacity == SCHED_CAPACITY_SCALE)
		return true;

	return task_fits_capacity(p, capacity, get_adaptive_margin(cpu));
}

inline int util_fits_capacity(unsigned long util, unsigned long uclamp_min,
	unsigned long uclamp_max, unsigned long capacity, int cpu)
{
	bool AM_enabled = adaptive_margin_enabled[cpu];
	unsigned int sugov_margin = AM_enabled ? get_adaptive_margin(cpu) : SCHED_CAPACITY_SCALE;
	unsigned long capacity_orig_thermal, capacity_orig = capacity_orig_of(cpu);
	int fit, uclamp_max_fits, uclamp_involve;

	uclamp_min = clamp((uclamp_min * DEFAULT_MARGIN) >> SCHED_FIXEDPOINT_SHIFT,
		0UL, (unsigned long) SCHED_CAPACITY_SCALE);
	uclamp_max = clamp((uclamp_max * DEFAULT_MARGIN) >> SCHED_FIXEDPOINT_SHIFT,
		0UL, (unsigned long) SCHED_CAPACITY_SCALE);

	uclamp_involve = mtk_uclamp_involve(uclamp_min, uclamp_max, true);
	if (uclamp_involve)
		sugov_margin = DEFAULT_MARGIN;
	/* Whether PELT fit after considering up-down migration ? */
	fit = fits_capacity(util, capacity, sugov_margin);

	/* Change fit status from 0 to 1 only if uclamp max restrict util. */
	capacity_orig_thermal = (capacity_orig - arch_scale_thermal_pressure(cpu));
	uclamp_max_fits = (capacity_orig == SCHED_CAPACITY_SCALE) && (uclamp_max == SCHED_CAPACITY_SCALE);
	uclamp_max_fits = !uclamp_max_fits && (uclamp_max <= capacity_orig);
	fit = fit || uclamp_max_fits;

	/* Change fit status from 1 to -1 only if uclamp min raise util. */
	uclamp_min = min(uclamp_min, uclamp_max);
	if (fit && (util < uclamp_min) && (uclamp_min > capacity_orig_thermal))
		fit = -1;

	return fit;
}
#endif /* CONFIG_MTK_SCHED_UPDOWN_MIGRATE */

/*default value: gear_start = -1, num_gear = -1, reverse = 0 */
static inline bool gear_hints_unset(struct task_gear_hints *ghts)
{
	if (ghts->gear_start >= 0)
		return false;

	if (ghts->num_gear > 0 && ghts->num_gear <= num_sched_clusters)
		return false;

	if (ghts->reverse)
		return false;

	return true;
}

void mtk_get_gear_indicies(struct task_struct *p, int *order_index, int *end_index,
		int *reverse)
{
	int i = 0;
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;
	int max_num_gear = -1;

	*order_index = 0;
	*end_index = 0;
	*reverse = 0;
	if (num_sched_clusters <= 1)
		goto out;

	/* gear_start's range -1~num_sched_clusters */
	if (ghts->gear_start > num_sched_clusters || ghts->gear_start < -1)
		goto out;
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	/* group based prefer MCPU */
	if (gear_hints_enable && gear_hints_unset(ghts) && group_get_gear_hint(p)) {
		*order_index = 1;
		*end_index = 0;
		*reverse = 1;
		goto out;
	}
#endif
	/* task has customized gear prefer */
	if (gear_hints_enable && ghts->gear_start >= 0) {
		*order_index = ghts->gear_start;
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	} else if (sysctl_prio_control && (p->prio >= sysctl_prio_control)) {
		*order_index = 0;
#endif
	} else {
		for (i = *order_index; i < num_sched_clusters - 1; i++) {
			if (task_demand_fits(p, cpumask_first(&cpu_array[i][0][0])))
				break;
		}

		*order_index = i;
	}

	if (gear_hints_enable && ghts->reverse)
		max_num_gear = *order_index + 1;
	else
		max_num_gear = num_sched_clusters - *order_index;

	if (gear_hints_enable && ghts->num_gear > 0 && ghts->num_gear <= max_num_gear)
		*end_index = ghts->num_gear - 1;
	else
		*end_index = max_num_gear - 1;

	if (gear_hints_enable)
		*reverse     = ghts->reverse;

out:
	if (trace_sched_get_gear_indices_enabled())
		trace_sched_get_gear_indices(p, uclamp_task_util(p), gear_hints_enable,
				ghts->gear_start, ghts->num_gear, ghts->reverse,
				num_sched_clusters, max_num_gear, *order_index, *end_index, *reverse);
}

inline void init_val_s(struct energy_env *eenv)
{
	int collab_type = 0;

	if (is_dpt_support_driver_hook == NULL)
		return;

	else if (!is_dpt_support_driver_hook())
		return;

	for (collab_type = 0; collab_type < get_nr_collab_type(); collab_type++)
		eenv->val_s[collab_type] = curr_collab_state[collab_type].state;
}

struct find_best_candidates_parameters {
	bool in_irq;
	bool latency_sensitive;
	int prev_cpu;
	int order_index;
	int end_index;
	int reverse;
	int fbc_reason;
	bool is_vip;
	int vip_prio;
	struct cpumask vip_candidate;
};

DEFINE_PER_CPU(cpumask_var_t, mtk_fbc_mask);

static void mtk_find_best_candidates(struct cpumask *candidates, struct task_struct *p,
		struct cpumask *effective_softmask, struct cpumask *allowed_cpu_mask,
		struct energy_env *eenv, struct find_best_candidates_parameters *fbc_params,
		int *sys_max_spare_cap_cpu, int *idle_max_spare_cap_cpu, unsigned long *cpu_utils)
{
	int cluster, cpu;
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_fbc_mask);
	unsigned long target_cap = 0;
	unsigned long cpu_cap, cpu_util, cpu_util_without_p;
	unsigned long cpu_util_without_uclamp, rq_uclamp_max, rq_uclamp_min;
	bool not_in_softmask;
	struct cpuidle_state *idle;
	long sys_max_spare_cap = LONG_MIN, idle_max_spare_cap = LONG_MIN;
	unsigned long min_cap = eenv->min_cap;
	unsigned long max_cap = eenv->max_cap;
	bool is_vvip = false;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	int target_balance_cluster;
#endif
	bool latency_sensitive = fbc_params->latency_sensitive;
	bool in_irq = fbc_params->in_irq;
	int prev_cpu = fbc_params->prev_cpu;
	int order_index = fbc_params->order_index;
	int end_index = fbc_params->end_index;
	int reverse = fbc_params->reverse;
	bool is_vip = fbc_params->is_vip;
	int vip_prio = fbc_params->vip_prio;
	struct cpumask vip_candidate = fbc_params->vip_candidate;

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	is_vvip = prio_is_vip(vip_prio, VVIP);

	if (is_vvip && !cpumask_empty(&vip_candidate)) {
		target_balance_cluster = topology_cluster_id(cpumask_last(&vip_candidate));
		order_index = target_balance_cluster;
		end_index = 0;
	}
#endif

	/* find best candidate */
	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		int fit;
		unsigned int uint_cpu;
		long spare_cap, spare_cap_without_p, pd_max_spare_cap = LONG_MIN;
		long pd_max_spare_cap_ls_idle = LONG_MIN;
		unsigned int pd_min_exit_lat = UINT_MAX;
		int pd_max_spare_cap_cpu = -1;
		int pd_max_spare_cap_cpu_ls_idle = -1;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		int cpu_order[MAX_NR_CPUS]  ____cacheline_aligned, cnt, i;

#endif

#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
		if (((cluster + 1) == num_sched_clusters) && sysctl_prio_control && (p->prio >= sysctl_prio_control))
			continue;
#endif

		cpumask_and(cpus, &cpu_array[order_index][cluster][reverse], cpu_active_mask);

		if (cpumask_empty(cpus))
			continue;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		cnt = sort_thermal_headroom(cpus, cpu_order, in_irq);

		for (i = 0; i < cnt; i++) {
			cpu = cpu_order[i];
#else
		for_each_cpu(cpu, cpus) {
#endif
			track_sched_cpu_util(p, cpu, min_cap, max_cap);

			if (!is_vip) {
				if (!cpumask_test_cpu(cpu, p->cpus_ptr))
					continue;

				if (cpu_paused(cpu))
					continue;

				if (cpu_high_irqload(cpu))
					continue;

				cpumask_set_cpu(cpu, allowed_cpu_mask);

				if (in_irq &&
					task_can_skip_this_cpu(p, min_cap, latency_sensitive, cpu, &bcpus, vip_prio))
					continue;

				if (cpu_rq(cpu)->rt.rt_nr_running >= 1 &&
							!rt_rq_throttled(&(cpu_rq(cpu)->rt)))
					continue;
			} else  {
				if (!cpumask_test_cpu(cpu, &vip_candidate))
					continue;
			}

			cpu_util = mtk_cpu_util_next(cpu, p, cpu, 0);
			cpu_util_without_uclamp = cpu_util;
			cpu_util_without_p = mtk_cpu_util_next(cpu, p, -1, 0);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap;
			spare_cap_without_p = cpu_cap;
			lsub_positive(&spare_cap, cpu_util);
			lsub_positive(&spare_cap_without_p, cpu_util_without_p);
			not_in_softmask = (latency_sensitive &&
						!cpumask_test_cpu(cpu, effective_softmask));

			if (not_in_softmask)
				continue;

			if (cpu == prev_cpu)
				spare_cap += spare_cap >> 6;

			if (is_target_max_spare_cpu(spare_cap_without_p, sys_max_spare_cap,
					*sys_max_spare_cap_cpu, cpu, "sys_max_spare")) {
				sys_max_spare_cap = spare_cap_without_p;
				*sys_max_spare_cap_cpu = cpu;
			}

			/*
			 * if there is no best idle cpu, then select max spare cap
			 * and idle cpu for latency_sensitive task to avoid runnable.
			 * Because this is just a backup option, we do not take care
			 * of exit latency.
			 */
			if (latency_sensitive && available_idle_cpu(cpu)) {
				if (is_target_max_spare_cpu(spare_cap_without_p, idle_max_spare_cap,
					*idle_max_spare_cap_cpu, cpu, "idle_max_spare")) {
					idle_max_spare_cap = spare_cap_without_p;
					*idle_max_spare_cap_cpu = cpu;
				}
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with effective_cpu_util().
			 */
			uint_cpu = cpu;
			/* record pre-clamping cpu_util */
			cpu_utils[uint_cpu] = cpu_util;
			cpu_util = mtk_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p,
							min_cap, max_cap, &rq_uclamp_min, &rq_uclamp_max, true);

			if (trace_sched_util_fits_cpu_enabled())
				trace_sched_util_fits_cpu(cpu, cpu_utils[uint_cpu], cpu_util, cpu_cap,
						min_cap, max_cap, cpu_rq(cpu));

			/* replace with post-clamping cpu_util */
			cpu_utils[uint_cpu] = cpu_util;

			fit = util_fits_capacity(cpu_util_without_uclamp, rq_uclamp_min, rq_uclamp_max, cpu_cap, cpu);
			if (fit <= 0)
				continue;

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */
			if (!latency_sensitive && is_target_max_spare_cpu(spare_cap, pd_max_spare_cap,
					pd_max_spare_cap_cpu, cpu, "pd_max_spare")) {
				pd_max_spare_cap = spare_cap;
				pd_max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (available_idle_cpu(cpu)) {
				cpu_cap = capacity_orig_of(cpu);
				idle = idle_get_state(cpu_rq(cpu));
				if (idle && idle->exit_latency > pd_min_exit_lat &&
						cpu_cap == target_cap)
					continue;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
				if (!in_irq && idle && idle->exit_latency == pd_min_exit_lat
						&& cpu_cap == target_cap)
					continue;
#endif

				if (!is_target_max_spare_cpu(spare_cap, pd_max_spare_cap_ls_idle,
					pd_max_spare_cap_cpu_ls_idle, cpu, "pd_max_spare_is_idle"))
					continue;

				pd_min_exit_lat = idle ? idle->exit_latency : 0;

				pd_max_spare_cap_ls_idle = spare_cap;
				target_cap = cpu_cap;
				pd_max_spare_cap_cpu_ls_idle = cpu;
			}
		}

		if (pd_max_spare_cap_cpu_ls_idle != -1)
			cpumask_set_cpu(pd_max_spare_cap_cpu_ls_idle, candidates);
		else if (pd_max_spare_cap_cpu != -1)
			cpumask_set_cpu(pd_max_spare_cap_cpu, candidates);

		if ((cluster >= end_index) && (!cpumask_empty(candidates)))
			break;
		else if (is_vvip &&
			(*idle_max_spare_cap_cpu>=0 || *sys_max_spare_cap_cpu>=0)) {
			/*
			 * Don't calc energy if we found max spare cpu
			 * since we have search a target balance gear for VVIP
			 */
			break;
		}
	}

	if ((cluster > end_index) && !cpumask_empty(cpus))
		fbc_params->fbc_reason = LB_FAIL;

	if (trace_sched_find_best_candidates_enabled())
		trace_sched_find_best_candidates(p, is_vip, candidates, order_index, end_index,
				allowed_cpu_mask);
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static DEFINE_PER_CPU(cpumask_t, energy_cpus);

void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p, int prev_cpu, int sync,
					int *new_cpu)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned long best_delta = ULONG_MAX;
	int this_cpu = smp_processor_id();
	struct root_domain *rd = cpu_rq(this_cpu)->rd;
	int sys_max_spare_cap_cpu = -1;
	int idle_max_spare_cap_cpu = -1;
	bool latency_sensitive = false;
	int best_energy_cpu = -1;
	unsigned int cpu;
	struct perf_domain *pd;
	int select_reason = -1, backup_reason = 0;
	unsigned long min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_cap = uclamp_eff_value(p, UCLAMP_MAX);
	struct energy_env eenv;
	struct cpumask effective_softmask;
	bool in_irq = in_interrupt();
	struct cpumask allowed_cpu_mask;
	int order_index = 0, end_index = 0, weight, reverse = 0;
	cpumask_t *candidates;
	struct find_best_candidates_parameters fbc_params;
	unsigned long cpu_utils[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = ULONG_MAX};
	int recent_used_cpu, target;
	bool is_vip = false;
	int vip_prio = NOT_VIP;
	struct cpumask vip_candidate;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	vip_prio = get_vip_task_prio(p);
	is_vip = prio_is_vip(vip_prio, NOT_VIP);
	if (vts->faster_compute_eng) {
		in_irq = true;
		vts->faster_compute_eng = false;
	}
#endif

	if (!get_eas_hook())
		return;

	cpumask_clear(&allowed_cpu_mask);

	irq_log_store();

	rcu_read_lock();
	compute_effective_softmask(p, &latency_sensitive, &effective_softmask);

	pd = rcu_dereference(rd->pd);
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (is_vip) {
		if (vip_in_gh)
			mtk_get_gear_indicies(p, &order_index, &end_index, &reverse);
		vip_candidate = find_min_num_vip_cpus(pd, p, vip_prio, &allowed_cpu_mask,
			order_index, end_index, reverse);
	}
#endif
	if (!pd || READ_ONCE(rd->overutilized)) {
		select_reason = LB_FAIL;
		rcu_read_unlock();
		goto fail;
	}

	irq_log_store();

	if (sync && cpu_rq(this_cpu)->nr_running == 1 &&
	    cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
	    task_fits_capacity(p, capacity_of(this_cpu), get_adaptive_margin(this_cpu)) &&
	    !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask))) {
		rcu_read_unlock();
		*new_cpu = this_cpu;
		select_reason = LB_SYNC;
		goto done;
	}

	irq_log_store();

	if (!task_util_est(p)) {
		select_reason = LB_ZERO_UTIL;
		rcu_read_unlock();
		goto fail;
	}

	irq_log_store();

	mtk_get_gear_indicies(p, &order_index, &end_index, &reverse);

	irq_log_store();

	eenv.min_cap = min_cap;
	eenv.max_cap = max_cap;

	eenv_task_busy_time(&eenv, p, prev_cpu);

	eenv_init(&eenv, p, prev_cpu, pd, in_irq);

	if (!eenv.task_busy_time) {
		select_reason = LB_ZERO_EENV_UTIL;
		rcu_read_unlock();
		goto fail;
	}

	init_val_s(&eenv);

	irq_log_store();

	fbc_params.in_irq = in_irq;
	fbc_params.latency_sensitive = latency_sensitive;
	fbc_params.prev_cpu = prev_cpu;
	fbc_params.order_index = order_index;
	fbc_params.end_index = end_index;
	fbc_params.reverse = reverse;
	fbc_params.fbc_reason = 0;
	fbc_params.is_vip = is_vip;
	fbc_params.vip_prio = vip_prio;
	fbc_params.vip_candidate = vip_candidate;

	/* Pre-select a set of candidate CPUs. */
	candidates = this_cpu_ptr(&energy_cpus);
	cpumask_clear(candidates);

	mtk_find_best_candidates(candidates, p, &effective_softmask, &allowed_cpu_mask,
		&eenv, &fbc_params, &sys_max_spare_cap_cpu, &idle_max_spare_cap_cpu, cpu_utils);

	irq_log_store();

	/* select best energy cpu in candidates */
	weight = cpumask_weight(candidates);
	if (!weight)
		goto unlock;
	if (weight == 1) {
		best_energy_cpu = cpumask_first(candidates);
		goto unlock;
	}

	for_each_cpu(cpu, candidates) {
		unsigned long cur_delta, base_energy;
		struct perf_domain *target_pd = rcu_dereference(pd);

		target_pd = find_pd(target_pd, cpu);
		if (!target_pd)
			continue;

		eenv.gear_idx = topology_cluster_id(cpu);
		cpus = to_cpumask(target_pd->em_pd->cpus);
		eenv.dst_cpu = cpu;

		/* Evaluate the energy impact of using this CPU. */
		if (unlikely(in_irq)) {
			unsigned long max_util;

			max_util = eenv_pd_max_util(&eenv, cpus, p, cpu);

			cur_delta = shared_buck_calc_pwr_eff(&eenv, cpu, max_util, cpus,
				is_dsu_pwr_triggered(eenv.wl_dsu));
			base_energy = 0;
		} else {
			eenv_pd_busy_time(&eenv, cpus, p);
			cur_delta = mtk_compute_energy(&eenv, target_pd, cpus, p,
								cpu);
			base_energy = mtk_compute_energy(&eenv, target_pd, cpus, p, -1);
		}
		cur_delta = max(cur_delta, base_energy) - base_energy;
		if (cur_delta < best_delta) {
			best_delta = cur_delta;
			best_energy_cpu = cpu;
		}
	}

unlock:
	rcu_read_unlock();

	irq_log_store();

	if (latency_sensitive) {
		if (best_energy_cpu >= 0) {
			*new_cpu = best_energy_cpu;
			select_reason = LB_LATENCY_SENSITIVE_BEST_IDLE_CPU | fbc_params.fbc_reason;
			goto done;
		}
		if (idle_max_spare_cap_cpu >= 0) {
			*new_cpu = idle_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU;
			goto done;
		}
		if (sys_max_spare_cap_cpu >= 0) {
			*new_cpu = sys_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_MAX_SPARE_CPU;
			goto done;
		}
	}

	irq_log_store();

	/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
	if (best_energy_cpu >= 0) {
		*new_cpu = best_energy_cpu;
		select_reason = LB_BEST_ENERGY_CPU | fbc_params.fbc_reason;
		goto done;
	}

	irq_log_store();

	if (sys_max_spare_cap_cpu >= 0) {
		*new_cpu = sys_max_spare_cap_cpu;
		select_reason = LB_MAX_SPARE_CPU;
		goto done;
	}

	irq_log_store();

fail:
	/* All cpu failed, even sys_max_spare_cap_cpu is not captured*/

	/* from overutilized & zero_util */
	if (cpumask_empty(&allowed_cpu_mask)) {
		cpumask_andnot(&allowed_cpu_mask, p->cpus_ptr, cpu_pause_mask);
		cpumask_and(&allowed_cpu_mask, &allowed_cpu_mask, cpu_active_mask);
	} else {
		if (select_reason != LB_FAIL)
			select_reason = LB_FAIL_IN_REGULAR;
	}

	rcu_read_lock();

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (is_vip) {
		struct cpumask temp_mask;

		/* for VVIP, select biggest CPU */
		if (prio_is_vip(vip_prio , VVIP) && !cpumask_empty(&vip_candidate)) {
			*new_cpu = cpumask_last(&vip_candidate);
			backup_reason = LB_BACKUP_VVIP;
			goto backup_unlock;
		}

		/* for other VIPs */
		cpumask_copy(&temp_mask, &allowed_cpu_mask);
		if (!cpumask_and(&allowed_cpu_mask, &allowed_cpu_mask, &vip_candidate))
			cpumask_copy(&allowed_cpu_mask, &temp_mask);
	}
#endif

	if (cpumask_test_cpu(this_cpu, p->cpus_ptr) && (this_cpu != prev_cpu))
		target = mtk_wake_affine(p, this_cpu, prev_cpu, sync);
	else
		target = prev_cpu;

	if (cpumask_test_cpu(target, &allowed_cpu_mask) &&
		/* Don't check CPU idle state if task is VIP, since idle is not critical for VIP*/
	    (is_vip || (available_idle_cpu(target) || sched_idle_cpu(target))) &&
	    task_fits_capacity(p, capacity_of(target), get_adaptive_margin(target))) {
		*new_cpu = target;
		backup_reason = LB_BACKUP_AFFINE_IDLE_FIT;
		goto backup_unlock;
	}

	/*
	 * If the previous CPU fit_capacity and idle, don't be stupid:
	 */
	if (prev_cpu != target && mtk_cpus_share_cache(prev_cpu, target) &&
		cpumask_test_cpu(prev_cpu, &allowed_cpu_mask) &&
	    (is_vip || (available_idle_cpu(prev_cpu) || sched_idle_cpu(prev_cpu))) &&
	    task_fits_capacity(p, capacity_of(prev_cpu), get_adaptive_margin(prev_cpu))) {
		*new_cpu = prev_cpu;
		backup_reason = LB_BACKUP_PREV;
		goto backup_unlock;
	}

	irq_log_store();

	/*
	 * Allow a per-cpu kthread to stack with the wakee if the
	 * kworker thread and the tasks previous CPUs are the same.
	 * The assumption is that the wakee queued work for the
	 * per-cpu kthread that is now complete and the wakeup is
	 * essentially a sync wakeup. An obvious example of this
	 * pattern is IO completions.
	 */
	if (cpumask_test_cpu(this_cpu, &allowed_cpu_mask) &&
		is_per_cpu_kthread(current) && in_task() &&
	    prev_cpu == this_cpu &&
	    this_rq()->nr_running <= 1 &&
	    task_fits_capacity(p, capacity_of(this_cpu), get_adaptive_margin(this_cpu))) {
		*new_cpu = this_cpu;
		backup_reason = LB_BACKUP_CURR;
		goto backup_unlock;
	}

	irq_log_store();

	/* Check a recently used CPU as a potential idle candidate: */
	recent_used_cpu = p->recent_used_cpu;
	p->recent_used_cpu = prev_cpu;
	if (recent_used_cpu != prev_cpu && recent_used_cpu != target &&
		mtk_cpus_share_cache(recent_used_cpu, target) &&
		cpumask_test_cpu(recent_used_cpu, &allowed_cpu_mask) &&
	    (is_vip || (available_idle_cpu(recent_used_cpu) || sched_idle_cpu(recent_used_cpu))) &&
	    task_fits_capacity(p, capacity_of(recent_used_cpu), get_adaptive_margin(recent_used_cpu))) {
		*new_cpu = recent_used_cpu;
		/*
		 * Replace recent_used_cpu
		 * candidate for the next
		 */
		p->recent_used_cpu = prev_cpu;
		backup_reason = LB_BACKUP_RECENT_USED_CPU;
		goto backup_unlock;
	}

	irq_log_store();

	*new_cpu = mtk_select_idle_capacity(p, &allowed_cpu_mask, target, is_vip);
	if (*new_cpu < nr_cpumask_bits) {
		backup_reason = LB_BACKUP_IDLE_CAP;
		goto backup_unlock;
	}

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (is_vip) {
		*new_cpu = find_vip_backup_cpu(p, &allowed_cpu_mask, prev_cpu, target);
		if (*new_cpu < nr_cpumask_bits) {
			backup_reason = LB_BACKUP_VIP_IN_MASK;
			goto backup_unlock;
		}
	}
#endif

	if (*new_cpu == -1 && cpumask_test_cpu(target, p->cpus_ptr)) {
		*new_cpu = target;
		backup_reason = LB_BACKUP_AFFINE_WITHOUT_IDLE_CAP;
		goto backup_unlock;
	}

	*new_cpu = -1;
backup_unlock:
	rcu_read_unlock();

done:
	irq_log_store();

	if (trace_sched_find_energy_efficient_cpu_enabled())
		trace_sched_find_energy_efficient_cpu(in_irq, best_delta, best_energy_cpu,
				best_energy_cpu, idle_max_spare_cap_cpu, sys_max_spare_cap_cpu);
	if (trace_sched_select_task_rq_enabled()) {
		trace_sched_select_task_rq(p, in_irq, select_reason, backup_reason, prev_cpu,
				*new_cpu, task_util(p), task_util_est(p), uclamp_task_util(p),
				latency_sensitive, sync, &effective_softmask);
	}
	if (trace_sched_effective_mask_enabled()) {
		struct soft_affinity_task *sa_task = &((struct mtk_task *)
			p->android_vendor_data1)->sa_task;
		struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
		struct cpumask softmask;

		if (css) {
			struct task_group *tg = container_of(css, struct task_group, css);
			struct soft_affinity_tg *sa_tg = &((struct mtk_tg *)
				tg->android_vendor_data1)->sa_tg;
			softmask = sa_tg->soft_cpumask;
		} else {
			cpumask_clear(&softmask);
			cpumask_copy(&softmask, cpu_possible_mask);
		}
		trace_sched_effective_mask(p, *new_cpu, latency_sensitive,
			&effective_softmask, &sa_task->soft_cpumask, &softmask);
	}

	irq_log_store();
}
#endif

#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
/* must hold runqueue lock for queue se is currently on */
static struct task_struct *detach_a_hint_task(struct rq *src_rq, int dst_cpu)
{
	struct task_struct *p, *best_task = NULL, *backup = NULL;
	int dst_capacity, src_capacity;
	unsigned int task_util;
	bool latency_sensitive = false, in_many_heavy_tasks;
	struct cpumask effective_softmask;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;

	lockdep_assert_rq_held(src_rq);

	rcu_read_lock();
	in_many_heavy_tasks = rd->android_vendor_data1;
	src_capacity = capacity_orig_of(src_rq->cpu);
	dst_capacity = cpu_cap_ceiling(dst_cpu);
	list_for_each_entry_reverse(p,
			&src_rq->cfs_tasks, se.group_node) {

		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		if (task_on_cpu(src_rq, p))
			continue;

		task_util = uclamp_task_util(p);

		compute_effective_softmask(p, &latency_sensitive, &effective_softmask);

		if (latency_sensitive && !cpumask_test_cpu(dst_cpu, &effective_softmask))
			continue;

#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
		if (sysctl_prio_control && (p->prio >= sysctl_prio_control) &&
			!cpumask_test_cpu(dst_cpu, &effective_softmask))
			continue;
#endif

		if (in_many_heavy_tasks &&
			!fits_capacity(task_util, src_capacity, get_adaptive_margin(src_rq->cpu))) {
			/* when too many big task, pull misfit runnable task */
			best_task = p;
			break;
		} else if (latency_sensitive &&
			task_util <= dst_capacity) {
			best_task = p;
			break;
		} else if (latency_sensitive && !backup) {
			backup = p;
		}
	}
	p = best_task ? best_task : backup;
	if (p) {
		/* detach_task */
		deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
		set_task_cpu(p, dst_cpu);
	}
	rcu_read_unlock();
	return p;
}
#endif

static int mtk_active_load_balance_cpu_stop(void *data)
{
	struct task_struct *target_task = data;
	int busiest_cpu = smp_processor_id();
	struct rq *busiest_rq = cpu_rq(busiest_cpu);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct rq_flags rf;
	int deactivated = 0;

	local_irq_disable();
	raw_spin_lock(&target_task->pi_lock);
	rq_lock(busiest_rq, &rf);

	if (task_cpu(target_task) != busiest_cpu ||
		(!cpumask_test_cpu(target_cpu, target_task->cpus_ptr)) ||
		task_on_cpu(busiest_rq, target_task) ||
		target_rq == busiest_rq)
		goto out_unlock;

	if (!task_on_rq_queued(target_task))
		goto out_unlock;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu))
		goto out_unlock;

	if (cpu_paused(busiest_cpu) || cpu_paused(target_cpu))
		goto out_unlock;

	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(!busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;

	update_rq_clock(busiest_rq);
	deactivate_task(busiest_rq, target_task, DEQUEUE_NOCLOCK);
	set_task_cpu(target_task, target_cpu);
	deactivated = 1;
out_unlock:
	busiest_rq->active_balance = 0;
	rq_unlock(busiest_rq, &rf);

	if (deactivated)
		attach_one_task(target_rq, target_task);

	raw_spin_unlock(&target_task->pi_lock);
	put_task_struct(target_task);

	local_irq_enable();
	return 0;
}

int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target, int reason)
{
	int active_balance = false;
	unsigned long flags;
	bool latency_sensitive = false;
	struct cpumask effective_softmask;

	compute_effective_softmask(p, &latency_sensitive, &effective_softmask);
	raw_spin_rq_lock_irqsave(target, flags);
	if (!target->active_balance &&
		(task_rq(p) == target) && READ_ONCE((p)->__state) != TASK_DEAD &&
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
		 !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask)) &&
		 !(sysctl_prio_control && (p->prio >= sysctl_prio_control) &&
		 !cpumask_test_cpu(this_cpu, &effective_softmask))) {
#else
		 !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask))) {
#endif
		target->active_balance = 1;
		target->push_cpu = this_cpu;
		active_balance = true;
		get_task_struct(p);
	}
	raw_spin_rq_unlock_irqrestore(target, flags);
	if (active_balance) {
		if (trace_sched_force_migrate_enabled())
			trace_sched_force_migrate(p, this_cpu, reason);
		stop_one_cpu_nowait(cpu_of(target),
				mtk_active_load_balance_cpu_stop,
				p, &target->active_balance_work);
	}

	return active_balance;
}

void try_to_pull_VVIP(int this_cpu, bool *had_pull_vvip, struct rq_flags *src_rf)
{
	struct root_domain *rd;
	struct perf_domain *pd;
	struct rq *src_rq, *this_rq;
	struct task_struct *p;
	int cpu, vip_prio;

	if (!cpumask_test_cpu(this_cpu, &bcpus))
		return;

	if (cpu_paused(this_cpu))
		return;

	if (!cpu_active(this_cpu))
		return;

	this_rq = cpu_rq(this_cpu);
	rd = this_rq->rd;
	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	for (; pd; pd = pd->next) {
		for_each_cpu(cpu, perf_domain_span(pd)) {

			if (cpu_paused(cpu))
				continue;

			if (!cpu_active(cpu))
				continue;

			if (cpu == this_cpu)
				continue;

			src_rq = cpu_rq(cpu);

			if (num_vip_in_cpu(cpu, VVIP) < 1)
				continue;

			else if (num_vip_in_cpu(cpu, VVIP) == 1) {
				/* the only one VVIP in cpu is running */
				if (src_rq->curr) {
					vip_prio = get_vip_task_prio(src_rq->curr);
					if (prio_is_vip(vip_prio, VVIP))
						continue;
				}
			}

			/* There are runnables in cpu */
			rq_lock_irqsave(src_rq, src_rf);
			if (src_rq->curr)
				update_rq_clock(src_rq);
			p = next_vip_runnable_in_cpu(src_rq, VVIP);
			if (p && cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
				deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
				set_task_cpu(p, this_cpu);
				rq_unlock_irqrestore(src_rq, src_rf);

				if (trace_sched_force_migrate_enabled())
					trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_PULL_VIP_RUNNABLE);
				attach_one_task(this_rq, p);
				*had_pull_vvip = true;
				goto unlock;
			}
			rq_unlock_irqrestore(src_rq, src_rf);
		}
	}
unlock:
	rcu_read_unlock();
}

#if IS_ENABLED(CONFIG_MTK_EAS)
static DEFINE_PER_CPU(u64, next_update_new_balance_time_ns);
void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf,
		int *pulled_task, int *done)
{
	int cpu;
	struct rq *src_rq, *misfit_task_rq = NULL;
	struct task_struct *p = NULL, *best_running_task = NULL;
	struct rq_flags src_rf;
	int this_cpu = this_rq->cpu;
	unsigned long misfit_load = 0;
	u64 now_ns;
	bool latency_sensitive = false;
	struct cpumask effective_softmask;
	bool had_pull_vvip = false;

	if (!get_eas_hook())
		return;

	if (cpu_paused(this_cpu)) {
		*done = 1;
		return;
	}

	/*
	 * There is a task waiting to run. No need to search for one.
	 * Return 0; the task will be enqueued when switching to idle.
	 */
	if (this_rq->ttwu_pending)
		return;

	/*
	 * We must set idle_stamp _before_ calling idle_balance(), such that we
	 * measure the duration of idle_balance() as idle time.
	 */
	this_rq->idle_stamp = rq_clock(this_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(this_cpu))
		return;

	now_ns = ktime_get_real_ns();

	if (now_ns < per_cpu(next_update_new_balance_time_ns, this_cpu))
		return;

	per_cpu(next_update_new_balance_time_ns, this_cpu) =
		now_ns + new_idle_balance_interval_ns;

	if (trace_sched_next_new_balance_enabled())
		trace_sched_next_new_balance(now_ns, per_cpu(next_update_new_balance_time_ns, this_cpu));

	/*
	 * This is OK, because current is on_cpu, which avoids it being picked
	 * for load-balance and preemption/IRQs are still disabled avoiding
	 * further scheduler activity on it and we're being very careful to
	 * re-start the picking loop.
	 */
	rq_unpin_lock(this_rq, rf);
	raw_spin_rq_unlock(this_rq);

	this_cpu = this_rq->cpu;

	/* try to pull runnable VVIP if this_cpu is in big gear */
	try_to_pull_VVIP(this_cpu, &had_pull_vvip, &src_rf);
	if (had_pull_vvip)
		goto out;

	for_each_cpu(cpu, cpu_active_mask) {
		if (cpu == this_cpu)
			continue;

		src_rq = cpu_rq(cpu);
		rq_lock_irqsave(src_rq, &src_rf);
		update_rq_clock(src_rq);
		if (src_rq->active_balance) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}
		if ((src_rq->misfit_task_load > misfit_load) &&
			(cpu_cap_ceiling(this_cpu) > cpu_cap_ceiling(cpu))) {
			p = src_rq->curr;
			if (p) {
				compute_effective_softmask(p, &latency_sensitive,
							&effective_softmask);
				if (p->policy == SCHED_NORMAL &&
					cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
					!(latency_sensitive &&
					!cpumask_test_cpu(this_cpu, &effective_softmask))) {

					misfit_task_rq = src_rq;
					misfit_load = src_rq->misfit_task_load;
					if (best_running_task)
						put_task_struct(best_running_task);
					best_running_task = p;
					get_task_struct(best_running_task);
				}
			}
			p = NULL;
		}

		if (src_rq->nr_running <= 1) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}

		p = detach_a_hint_task(src_rq, this_cpu);

		rq_unlock_irqrestore(src_rq, &src_rf);

		if (p) {
			if (trace_sched_force_migrate_enabled())
				trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_BALANCE);
			attach_one_task(this_rq, p);
			break;
		}
	}

	/*
	 * If p is null meaning that we have not pull a runnable task, we try to
	 * pull a latency sensitive running task.
	 */
	if (!p && misfit_task_rq)
		*done = migrate_running_task(this_cpu, best_running_task,
					misfit_task_rq, MIGR_IDLE_PULL_MISFIT_RUNNING);
	if (best_running_task)
		put_task_struct(best_running_task);
out:
	raw_spin_rq_lock(this_rq);
	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (this_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		this_rq->idle_stamp = 0;

	if (*pulled_task != 0)
		*done = 1;

	rq_repin_lock(this_rq, rf);

}
#endif
