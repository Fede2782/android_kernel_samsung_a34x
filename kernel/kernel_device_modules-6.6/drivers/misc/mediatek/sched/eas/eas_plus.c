// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/sched/cputime.h>
#include <linux/of_platform.h>
#include <sched/sched.h>
#include <sugov/cpufreq.h>
#include "common.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v3/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "eas_plus.h"
#include "eas_trace.h"
#include <linux/sort.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include "vip.h"
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "group.h"
#include "flt_cal.h"
#endif

#include <mt-plat/mtk_irq_mon.h>
#include "arch.h"

MODULE_LICENSE("GPL");

#define CORE_PAUSE_OUT		0
#define IB_ASYM_MISFIT		(0x02)
#define IB_SAME_CLUSTER		(0x01)
#define IB_OVERUTILIZATION	(0x04)

struct cpumask __cpu_pause_mask;
EXPORT_SYMBOL(__cpu_pause_mask);

struct perf_domain *find_pd(struct perf_domain *pd, int cpu)
{
	while (pd) {
		if (cpumask_test_cpu(cpu, perf_domain_span(pd)))
			return pd;

		pd = pd->next;
	}

	return NULL;
}

static inline bool check_faster_idle_balance(struct sched_group *busiest, struct rq *dst_rq)
{

	int src_cpu = cpumask_first(sched_group_span(busiest));
	int dst_cpu = cpu_of(dst_rq);
	int cpu;

	if (cpu_cap_ceiling(dst_cpu) <= cpu_cap_ceiling(src_cpu))
		return false;

	for_each_cpu(cpu, sched_group_span(busiest)) {
		if (cpu_rq(cpu)->misfit_task_load)
			return true;
	}

	return false;
}

static inline bool check_has_overutilize_cpu(struct cpumask *grp)
{

	int cpu;

	for_each_cpu(cpu, grp) {
		if (cpu_rq(cpu)->nr_running >= 2 &&
			!fits_capacity(mtk_cpu_util_cfs(cpu), capacity_of(cpu),
				get_adaptive_margin(cpu)))
			return true;
	}
	return false;
}

void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance)
{
	int src_cpu = -1;
	int dst_cpu = dst_rq->cpu;


	if (!get_eas_hook())
		return;

	if (cpu_paused(dst_cpu)) {
		*out_balance = 1;
		if (trace_sched_find_busiest_group_enabled())
			trace_sched_find_busiest_group(src_cpu, dst_cpu, *out_balance, CORE_PAUSE_OUT);
		return;
	}

	if (busiest) {
		struct perf_domain *pd = NULL;
		int dst_cpu = dst_rq->cpu;
		int fbg_reason = 0;

		pd = rcu_dereference(dst_rq->rd->pd);
		pd = find_pd(pd, dst_cpu);
		if (!pd)
			return;

		src_cpu = cpumask_first(sched_group_span(busiest));

		/*
		 *  1.same cluster
		 *  2.not same cluster but dst_cpu has a higher capacity and
		 *    busiest group has misfit task. The purpose of this condition
		 *    is trying to let misfit task goto hiehger cpu.
		 */
		if (cpumask_test_cpu(src_cpu, perf_domain_span(pd))) {
			*out_balance = 0;
			fbg_reason |= IB_SAME_CLUSTER;
		} else if (check_faster_idle_balance(busiest, dst_rq)) {
			*out_balance = 0;
			fbg_reason |= IB_ASYM_MISFIT;
		} else if (check_has_overutilize_cpu(sched_group_span(busiest))) {
			*out_balance = 0;
			fbg_reason |= IB_OVERUTILIZATION;
		}

		if (trace_sched_find_busiest_group_enabled())
			trace_sched_find_busiest_group(src_cpu, dst_cpu, *out_balance, fbg_reason);
	}
}

void mtk_cpu_overutilized(void *data, int cpu, int *overutilized)
{
	struct perf_domain *pd = NULL;
	struct rq *rq = cpu_rq(cpu);
	unsigned long sum_util = 0, sum_cap = 0;
	int i = 0;

	if (!get_eas_hook())
		return;

	rcu_read_lock();
	pd = rcu_dereference(rq->rd->pd);
	pd = find_pd(pd, cpu);
	if (!pd) {
		rcu_read_unlock();
		return;
	}

	if (cpumask_weight(perf_domain_span(pd)) == 1 &&
		topology_cluster_id(cpu) == get_sys_max_cap_cluster()) {
		*overutilized = 0;
		rcu_read_unlock();
		return;
	}

	for_each_cpu(i, perf_domain_span(pd)) {
		sum_util += mtk_cpu_util_cfs(i);
		sum_cap += capacity_of(i);
	}


	*overutilized = !fits_capacity(sum_util, sum_cap, get_adaptive_margin(cpu));
	if (trace_sched_cpu_overutilized_enabled())
		trace_sched_cpu_overutilized(cpu, perf_domain_span(pd), sum_util, sum_cap, *overutilized);

	rcu_read_unlock();
}

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
int __read_mostly thermal_headroom[MAX_NR_CPUS]  ____cacheline_aligned;
unsigned long next_update_thermal;
static DEFINE_SPINLOCK(thermal_headroom_lock);

static void update_thermal_headroom(int this_cpu)
{
	int cpu;

	if (spin_trylock(&thermal_headroom_lock)) {
		if (time_before(jiffies, next_update_thermal)) {
			spin_unlock(&thermal_headroom_lock);
			return;
		}

		next_update_thermal = jiffies + thermal_headroom_interval_tick;
		for_each_cpu(cpu, cpu_possible_mask) {
			thermal_headroom[cpu] = get_thermal_headroom(cpu);
		}

		if (trace_sched_next_update_thermal_headroom_enabled())
			trace_sched_next_update_thermal_headroom(jiffies, next_update_thermal);

		spin_unlock(&thermal_headroom_lock);
	}

}

int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order, bool in_irq)
{
	int i, j, cpu, cnt = 0;
	int headroom_order[MAX_NR_CPUS] ____cacheline_aligned;

	if (cpumask_weight(cpus) == 1) {
		cpu = cpumask_first(cpus);
		*cpu_order = cpu;

		return 1;
	}

	if (in_irq) {
		i = 0;
		for_each_cpu_and(cpu, cpus, cpu_active_mask) {
			cpu_order[i] = cpu;
			cnt++;
			i++;
		}
		return cnt;
	}

	spin_lock(&thermal_headroom_lock);
	for_each_cpu_and(cpu, cpus, cpu_active_mask) {
		int headroom;

		headroom = thermal_headroom[cpu];

		for (i = 0; i < cnt; i++) {
			if (headroom > headroom_order[i])
				break;
		}

		for (j = cnt; j >= i; j--) {
			headroom_order[j+1] = headroom_order[j];
			cpu_order[j+1] = cpu_order[j];
		}

		headroom_order[i] = headroom;
		cpu_order[i] = cpu;
		cnt++;
	}
	spin_unlock(&thermal_headroom_lock);

	return cnt;
}

#endif

unsigned long pd_get_util_cpufreq(struct energy_env *eenv,
		struct cpumask *pd_cpus, unsigned long max_util,
		unsigned long allowed_cpu_cap, unsigned long scale_cpu)
{
	unsigned long freq;

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
	mtk_map_util_freq(NULL, max_util, pd_cpus,
		&freq);
#else
	max_util = map_util_perf(max_util);
	max_util = min(max_util, allowed_cpu_cap);
	freq = map_util_freq(max_util, pd_get_opp_freq(cpumask_first(pd_cpus), 0), scale_cpu);
#endif

	/* final freq aware outside min_freq ctrl*/
	freq = max(freq, per_cpu(min_freq, cpumask_first(pd_cpus)));

	return freq;
}
EXPORT_SYMBOL_GPL(pd_get_util_cpufreq);

static inline
int pd_get_efficient_state_opp(struct em_perf_domain *pd, int cpu,
						unsigned long freq, int wl)
{
	int opp = -1;

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	opp = pd_freq2opp(cpu, freq, false, wl);
#else
	int i;
	struct em_perf_state *ps;
	/*
	 * Find the lowest performance state of the Energy Model above the
	 * requested frequency.
	 */
	for (i = 0; i < pd->nr_perf_states; i++) {
		ps = &pd->table[i];
		if (ps->frequency >= freq)
			break;
	}

	i = min(i, pd->nr_perf_states - 1);
	opp = pd->nr_perf_states - i - 1;
#endif

	return opp;
}

DEFINE_PER_CPU(cpumask_var_t, em_energy_mask);

static inline
unsigned long estimate_energy(struct em_perf_domain *pd,
		int opp, unsigned long sum_util, struct energy_env *eenv,
		unsigned long scale_cpu, unsigned long freq, unsigned long extern_volt, unsigned long max_util)
{
	int cpu, this_cpu, wl = 0;
	unsigned long cap;
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	unsigned long freq_legacy;
#else
	struct em_perf_state *ps;
#endif
	unsigned long energy;
	int *cpu_temp = eenv->cpu_temp;
	struct cpumask *pd_cpus = to_cpumask(pd->cpus);
	unsigned int mtk_em, get_lkg;
	unsigned long output[MAX_NR_CPUS + 6] = {0};
	unsigned long data[4] = {0};
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(em_energy_mask);

	this_cpu = cpu = cpumask_first(to_cpumask(pd->cpus));

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	cap = pd_opp2cap(cpu, opp, false, eenv->wl_cpu, eenv->val_s,
			false, DPT_CALL_MTK_EM_CPU_ENERGY);
#else
	ps = &pd->table[pd->nr_perf_states - 1];
	cap = freq * scale_cpu / ps->frequency;
#endif

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	mtk_em = 1;
#else
	mtk_em = 0;
	ps = &pd->table[pd->nr_perf_states - opp - 1];
	output[2] = ps->cost;
#endif

#if IS_ENABLED(CONFIG_MTK_LEAKAGE_AWARE_TEMP)
	get_lkg = 1;
#else
	get_lkg = 0;
#endif

	cpumask_and(cpus, pd_cpus, cpu_online_mask);

	data[0] = sum_util;
	data[1] = extern_volt;
	data[2] = cap;
	data[3] = scale_cpu;
	energy = get_cpu_power(mtk_em, get_lkg, false, eenv->wl_cpu,
		eenv->val_s, false, DPT_CALL_MTK_EM_CPU_ENERGY,
		this_cpu, cpu_temp, opp, cpus->bits[0],
		data, output, pd, freq, max_util);

	if (get_lkg) {
		for_each_cpu_and(cpu, pd_cpus, cpu_online_mask) {
			if (trace_sched_leakage_enabled())
				trace_sched_leakage(cpu, opp, output[5], cpu_temp[cpu],
					output[cpu + 6], output[1], sum_util, output[3]);
		}
	}

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	wl = eenv->wl_cpu;

	/* for pd_opp_capacity is scaled based on maximum scale 1024, so cost = pwr_eff * 1024 */
	if (trace_sched_em_cpu_energy_enabled()) {
		freq_legacy = pd_get_opp_freq_legacy(this_cpu, pd_get_freq_opp_legacy(this_cpu,
											freq));
		trace_sched_em_cpu_energy(wl, opp, freq_legacy, "pwr_eff", output[2],
			scale_cpu, output[0], output[1], output[4], extern_volt);
	}
#else
	if (trace_sched_em_cpu_energy_enabled())
		trace_sched_em_cpu_energy(wl, opp, freq, "ps->cost", output[2],
			scale_cpu, output[0], output[1], output[4], extern_volt);
#endif

	return energy;
}

/**
 * em_cpu_energy() - Estimates the energy consumed by the CPUs of a
		performance domain
 * @pd		: performance domain for which energy has to be estimated
 * @max_util	: highest utilization among CPUs of the domain
 * @sum_util	: sum of the utilization of all CPUs in the domain
 *
 * This function must be used only for CPU devices. There is no validation,
 * i.e. if the EM is a CPU type and has cpumask allocated. It is called from
 * the scheduler code quite frequently and that is why there is not checks.
 *
 * Return: the sum of the energy consumed by the CPUs of the domain assuming
 * a capacity state satisfying the max utilization of the domain.
 */
unsigned long mtk_em_cpu_energy(struct em_perf_domain *pd,
		unsigned long pd_freq, unsigned long sum_util,
		unsigned long scale_cpu, struct energy_env *eenv,
		unsigned long extern_volt, unsigned long max_util)
{
	unsigned long freq;
	int cpu, opp = -1;
	struct cpumask *pd_cpus = to_cpumask(pd->cpus);

	if (!sum_util)
		return 0;

	/*
	 * In order to predict the performance state, map the utilization of
	 * the most utilized CPU of the performance domain to a requested
	 * frequency, like schedutil.
	 */
	cpu = cpumask_first(pd_cpus);
	freq = max(pd_freq, per_cpu(min_freq, cpu));

	opp = pd_get_efficient_state_opp(pd, cpu, freq, eenv->wl_cpu);

	/*
	 * The capacity of a CPU in the domain at the performance state (ps)
	 * can be computed as:
	 *
	 *             ps->freq * scale_cpu
	 *   ps->cap = --------------------                          (1)
	 *                 cpu_max_freq
	 *
	 * So, ignoring the costs of idle states (which are not available in
	 * the EM), the energy consumed by this CPU at that performance state
	 * is estimated as:
	 *
	 *             ps->power * cpu_util
	 *   cpu_nrg = --------------------                          (2)
	 *                   ps->cap
	 *
	 * since 'cpu_util / ps->cap' represents its percentage of busy time.
	 *
	 *   NOTE: Although the result of this computation actually is in
	 *         units of power, it can be manipulated as an energy value
	 *         over a scheduling period, since it is assumed to be
	 *         constant during that interval.
	 *
	 * By injecting (1) in (2), 'cpu_nrg' can be re-expressed as a product
	 * of two terms:
	 *
	 *             ps->power * cpu_max_freq   cpu_util
	 *   cpu_nrg = ------------------------ * ---------          (3)
	 *                    ps->freq            scale_cpu
	 *
	 * The first term is static, and is stored in the em_perf_state struct
	 * as 'ps->cost'.
	 *
	 * Since all CPUs of the domain have the same micro-architecture, they
	 * share the same 'ps->cost', and the same CPU capacity. Hence, the
	 * total energy of the domain (which is the simple sum of the energy of
	 * all of its CPUs) can be factorized as:
	 *
	 *            ps->cost * \Sum cpu_util
	 *   pd_nrg = ------------------------                       (4)
	 *                  scale_cpu
	 */
	return estimate_energy(pd, opp, sum_util, eenv, scale_cpu, freq, extern_volt, max_util);
}

#define OFFS_THERMAL_LIMIT_S 0x1208
#define THERMAL_INFO_SIZE 200

static void __iomem *sram_base_addr;
static struct eas_info eas_node;

int init_sram_info(void)
{
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;
	struct resource *csram_res;

	// first try to read sram_base_addr from cpuhvfs
	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node != NULL) {
		pdev_temp = of_find_device_by_node(dvfs_node);
		if (pdev_temp == NULL) {
			pr_info("failed to find pdev @ %s\n", __func__);
			return -EINVAL;
		}

		csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);

		if (csram_res)
			sram_base_addr =
				ioremap(csram_res->start + OFFS_THERMAL_LIMIT_S, THERMAL_INFO_SIZE);
		else {
			pr_info("%s can't get resource\n", __func__);
			return -ENODEV;
		}

		if (!sram_base_addr) {
			pr_info("Remap thermal info failed in cpuhvfs node\n");

			return -EIO;
		}
	} else {
		pr_info("failed to find node @ %s, try to read eas-info from dts.\n", __func__);
		// second try to read sram_base_addr from eas-info
		parse_eas_data(&eas_node);
		if (eas_node.available) {
			sram_base_addr = ioremap(eas_node.csram_base + eas_node.offs_thermal_limit_s,
						THERMAL_INFO_SIZE);
			if (!sram_base_addr) {
				pr_info("Remap thermal info failed in eas-info node\n");
				return -EIO;
			}
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
inline void update_thermal_pressure_capacity(bool update_all, int this_cpu)
{
	unsigned int gear_id, cpu, first_cpu;
	unsigned long max_capacity, thermal_max_capacity;
	unsigned long last_th_pressure, th_pressure;
	struct cpumask *cpus;
	unsigned int freq_thermal;
	int wl = get_em_wl();

	for (gear_id = 0; gear_id < num_sched_clusters; gear_id++) {
		cpus = get_gear_cpumask(gear_id);
		first_cpu = cpumask_first(cpus);
		if (!update_all && (this_cpu != first_cpu))
			continue;

		freq_thermal = get_cpu_ceiling_freq(gear_id);
		trace_sched_frequency_limits(first_cpu, freq_thermal);

		thermal_max_capacity = pd_freq2util(first_cpu, freq_thermal, true, wl, NULL, 0);
		max_capacity = arch_scale_cpu_capacity(first_cpu);
		th_pressure = max_capacity - thermal_max_capacity;
		last_th_pressure = READ_ONCE(per_cpu(thermal_pressure, first_cpu));

		if (trace_sched_update_thermal_pressure_capacity_enabled())
			trace_sched_update_thermal_pressure_capacity(first_cpu,
				th_pressure, max_capacity, thermal_max_capacity, wl);

		if (th_pressure == last_th_pressure)
			continue;

		for_each_cpu(cpu, cpus)
			WRITE_ONCE(per_cpu(thermal_pressure, cpu), th_pressure);
	}
}
#endif

void mtk_tick_entry(void *data, struct rq *rq)
{
	unsigned int this_cpu = cpu_of(rq);
	bool sbb_trigger, is_cpu_to_update_thermal = false, update_all __maybe_unused = true;
	u64 idle_time, wall_time, cpu_utilize;
	struct sbb_cpu_data *sbb_data = per_cpu(sbb, rq->cpu);
	if (!get_eas_hook())
		return;

	irq_log_store();

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	/* WL & DPT need update here.
	 * If WL supported, update DPT after WL is finish.
	 * If WL not supported, update DPT independently.
	 */
	if (is_wl_support())
		update_wl_tbl(this_cpu, &is_cpu_to_update_thermal);
	else
		update_curr_collab_state(&is_cpu_to_update_thermal);
#else
	/* If gearless is not support, update collab state dependently. */
	update_curr_collab_state(&is_cpu_to_update_thermal);
#endif

	sbb_trigger = is_sbb_trigger(rq);

	if (sbb_trigger) {
		if (sbb_data->tick_start) {
			idle_time = get_cpu_idle_time(rq->cpu, &wall_time, 1);

			cpu_utilize = 100 - div_u64((100 * (idle_time - sbb_data->idle_time)),
				wall_time - sbb_data->wall_time);

			sbb_data->idle_time = idle_time;
			sbb_data->wall_time = wall_time;

			if (cpu_utilize >=
				get_sbb_active_ratio_gear(topology_cluster_id(this_cpu))) {
				sbb_data->active = 1;

				sbb_data->boost_factor =
				min_t(u32, sbb_data->boost_factor * 2, 4);

				sbb_data->cpu_utilize = cpu_utilize;
			} else {
				sbb_data->active = 0;
				sbb_data->boost_factor = 1;
			}
		} else {
			sbb_data->active = 0;
			sbb_data->tick_start = 1;
			sbb_data->boost_factor = 1;
		}
	} else {
		sbb_data->active = 0;
		sbb_data->tick_start = 0;
		sbb_data->boost_factor = 1;
	}

	/* Check who should update thermal pressure.
	 * if WL or DPT is on, the CPU which update WL or DPT should update thermal for all CPUs.
	 * if both are off, the first CPU of the pd should update thermal for pd CPUs.
	 */
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	if (!is_wl_support() && !DPT_TURN_ON)
#else
	if (!DPT_TURN_ON)
#endif
		update_all = false;
	else if (!is_cpu_to_update_thermal)
		return;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
	update_thermal_headroom(this_cpu);
#endif

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	irq_log_store();
	update_thermal_pressure_capacity(update_all, this_cpu);
	irq_log_store();
#endif
}

/*
 * Enable/Disable honoring sync flag in energy-aware wakeups
 */
unsigned int sched_sync_hint_enable = 1;
void set_wake_sync(unsigned int sync)
{
	sched_sync_hint_enable = sync;
}
EXPORT_SYMBOL_GPL(set_wake_sync);

unsigned int get_wake_sync(void)
{
	return sched_sync_hint_enable;
}
EXPORT_SYMBOL_GPL(get_wake_sync);

void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode)
{
	if (!get_eas_hook())
		return;

	if (!sched_sync_hint_enable)
		*wake_flags &= ~WF_SYNC;
}

unsigned int new_idle_balance_interval_ns  =  1000000;
unsigned int thermal_headroom_interval_tick =  1;

void set_newly_idle_balance_interval_us(unsigned int interval_us)
{
	new_idle_balance_interval_ns = interval_us * 1000;

	if (trace_sched_newly_idle_balance_interval_enabled())
		trace_sched_newly_idle_balance_interval(interval_us);
}
EXPORT_SYMBOL_GPL(set_newly_idle_balance_interval_us);

unsigned int get_newly_idle_balance_interval_us(void)
{
	return new_idle_balance_interval_ns / 1000;
}
EXPORT_SYMBOL_GPL(get_newly_idle_balance_interval_us);

void set_get_thermal_headroom_interval_tick(unsigned int tick)
{
	thermal_headroom_interval_tick = tick;

	if (trace_sched_headroom_interval_tick_enabled())
		trace_sched_headroom_interval_tick(tick);
}
EXPORT_SYMBOL_GPL(set_get_thermal_headroom_interval_tick);

unsigned int get_thermal_headroom_interval_tick(void)
{
	return thermal_headroom_interval_tick;
}
EXPORT_SYMBOL_GPL(get_thermal_headroom_interval_tick);

static DEFINE_RAW_SPINLOCK(migration_lock);

int select_idle_cpu_from_domains(struct task_struct *p,
					struct perf_domain **prefer_pds, unsigned int len)
{
	unsigned int i = 0;
	struct perf_domain *pd;
	int cpu, best_cpu = -1;

	for (; i < len; i++) {
		pd = prefer_pds[i];
		for_each_cpu_and(cpu, perf_domain_span(pd),
						cpu_active_mask) {
			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;
			if (available_idle_cpu(cpu)) {
				best_cpu = cpu;
				break;
			}
		}
		if (best_cpu != -1)
			break;
	}

	return best_cpu;
}

int select_bigger_idle_cpu(struct task_struct *p)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd, *prefer_pds[MAX_NR_CPUS];
	int cpu = task_cpu(p), bigger_idle_cpu = -1;
	unsigned int i = 0;
	long max_capacity = cpu_cap_ceiling(cpu);
	long capacity;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);

	for (; pd; pd = pd->next) {
		capacity = cpu_cap_ceiling(cpumask_first(perf_domain_span(pd)));
		if (capacity > max_capacity &&
			cpumask_intersects(p->cpus_ptr, perf_domain_span(pd))) {
			prefer_pds[i++] = pd;
		}
	}

	if (i != 0)
		bigger_idle_cpu = select_idle_cpu_from_domains(p, prefer_pds, i);

	rcu_read_unlock();
	return bigger_idle_cpu;
}

void check_for_migration(struct task_struct *p)
{
	int new_cpu = -1, better_idle_cpu = -1;
	int cpu = task_cpu(p);
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	bool is_low_prio_task = false;
#endif
	struct rq *rq = cpu_rq(cpu);

#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	if (sysctl_prio_control && (p->prio >= sysctl_prio_control))
		is_low_prio_task = true;
#endif

	irq_log_store();

	if (rq->misfit_task_load) {
		struct em_perf_domain *pd;
		struct cpufreq_policy *policy;
		int opp_curr = 0, thre = 0, thre_idx = 0;

		if (rq->curr->__state != TASK_RUNNING ||
			rq->curr->nr_cpus_allowed == 1)
			return;

		pd = em_cpu_get(cpu);
		if (!pd)
			return;

		thre_idx = (pd->nr_perf_states >> 3) - 1;
		if (thre_idx >= 0)
			thre = pd->em_table->state[thre_idx].frequency;

		policy = cpufreq_cpu_get(cpu);
		irq_log_store();

		if (policy) {
			opp_curr = policy->cur;
			cpufreq_cpu_put(policy);
		}

		if (opp_curr <= thre) {
			irq_log_store();
			return;
		}

		raw_spin_lock(&migration_lock);
		irq_log_store();
		raw_spin_lock(&p->pi_lock);
		irq_log_store();

		new_cpu = p->sched_class->select_task_rq(p, cpu, WF_TTWU);
		irq_log_store();

		raw_spin_unlock(&p->pi_lock);

		if ((new_cpu < 0) || new_cpu >= MAX_NR_CPUS ||
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
			((cpu_cap_ceiling(new_cpu) <= cpu_cap_ceiling(cpu)) && !is_low_prio_task))
#else
			(cpu_cap_ceiling(new_cpu) <= cpu_cap_ceiling(cpu)))
#endif
			better_idle_cpu = select_bigger_idle_cpu(p);

		if (better_idle_cpu >= 0)
			new_cpu = better_idle_cpu;

		if (new_cpu < 0) {
			raw_spin_unlock(&migration_lock);
			irq_log_store();
			return;
		}

		irq_log_store();
		if ((better_idle_cpu >= 0) ||
			(new_cpu < MAX_NR_CPUS && new_cpu >= 0 &&
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
			(cpu_cap_ceiling(new_cpu) > cpu_cap_ceiling(cpu)) && !is_low_prio_task)) {
#else
			(cpu_cap_ceiling(new_cpu) <= cpu_cap_ceiling(cpu)))
#endif
			raw_spin_unlock(&migration_lock);

			migrate_running_task(new_cpu, p, rq, MIGR_TICK_PULL_MISFIT_RUNNING);
			irq_log_store();
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
		} else if (new_cpu < MAX_NR_CPUS && new_cpu >= 0 && is_low_prio_task) {
			raw_spin_unlock(&migration_lock);

			migrate_running_task(new_cpu, p, rq, MIGR_TICK_PULL_MISFIT_RUNNING);
			irq_log_store();
#endif
		} else {
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
			int thre_rot = 0, thre_rot_idx = 0;

			thre_rot_idx = (pd->nr_perf_states >> 1) - 1;
			if (thre_rot_idx >= 0)
				thre_rot = pd->em_table->state[thre_rot_idx].frequency;

			if (opp_curr > thre_rot) {
				task_check_for_rotation(rq);
				irq_log_store();
			}

#endif
			raw_spin_unlock(&migration_lock);
		}
	}
	irq_log_store();
}

void hook_scheduler_tick(void *data, struct rq *rq)
{
	if (!get_eas_hook())
		return;

	struct root_domain *rd = rq->rd;

	rcu_read_lock();
	rd->android_vendor_data1 = system_has_many_heavy_task();
	rcu_read_unlock();

	if (rq->curr->policy == SCHED_NORMAL)
		check_for_migration(rq->curr);
}

void mtk_hook_after_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags)
{
	int this_cpu = smp_processor_id();
	struct sugov_rq_data *sugov_data_ptr;
	struct sugov_rq_data *sugov_data_ptr2;

	if (!get_eas_hook())
		return;

	irq_log_store();

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	rotat_after_enqueue_task(data, rq, p);
#endif
	irq_log_store();

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (vip_fair_task(p))
		vip_enqueue_task(rq, p);
#endif

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	irq_log_store();
	sugov_data_ptr = &per_cpu(rq_data, rq->cpu)->sugov_data;
	sugov_data_ptr2 = &per_cpu(rq_data, this_cpu)->sugov_data;
	if (READ_ONCE(sugov_data_ptr->enq_update_dsu_freq) == true
			|| READ_ONCE(sugov_data_ptr2->enq_dvfs) == true) {
		cpufreq_update_util(rq, 0);

		WRITE_ONCE(sugov_data_ptr2->enq_dvfs, false);
	}
	WRITE_ONCE(sugov_data_ptr->enq_ing, false);
#endif
	irq_log_store();
}

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO) && IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static inline
unsigned long aligned_freq_to_legacy_freq(int cpu, unsigned long freq)
{
	return pd_get_opp_freq_legacy(cpu, pd_get_freq_opp_legacy(cpu, freq));
}

__always_inline
unsigned long calc_pwr_eff(int wl, int cpu, unsigned long cpu_util, int *val_s)
{
	int opp;
	unsigned long static_pwr_eff, pwr_eff;
	int util = get_cpu_util_with_margin(cpu, cpu_util);
	int cap;
	int pd_pwr_eff;

	opp = pd_util2opp(cpu, util, false, wl, val_s, false, DPT_CALL_CALC_PWR_EFF);
	cap = pd_opp2cap(cpu, opp, false, wl, val_s, false, DPT_CALL_CALC_PWR_EFF);
	pd_pwr_eff = pd_opp2pwr_eff(cpu, opp, false, wl, val_s, false, DPT_CALL_CALC_PWR_EFF);

	static_pwr_eff = pd_get_opp_leakage(cpu, opp, get_cpu_temp(cpu)/1000) / cap;
	pwr_eff = pd_pwr_eff + static_pwr_eff;

	if (trace_sched_calc_pwr_eff_enabled())
		trace_sched_calc_pwr_eff(cpu, cpu_util, opp, -1, cap,
				pd_pwr_eff, static_pwr_eff, pwr_eff, -1, -1);

	return pwr_eff;
}

__always_inline
unsigned long calc_pwr_eff_v2(struct energy_env *eenv, int cpu, unsigned long max_util,
		unsigned long pd_freq, struct cpumask *cpus, unsigned long extern_volt)
{
	unsigned long pwr_eff;
	unsigned long output[6] = {0};
	int temp = get_cpu_temp(cpu)/1000;

	pwr_eff = get_cpu_pwr_eff(cpu, pd_freq, false, eenv->wl_cpu,
			eenv->val_s, false, DPT_CALL_CALC_PWR_EFF, temp, extern_volt, output);

	if (trace_sched_calc_pwr_eff_enabled())
		trace_sched_calc_pwr_eff(cpu, max_util, (int) output[0], (int) output[5],
			(int) output[2], output[1], output[3], pwr_eff, output[4], extern_volt);

	return pwr_eff;
}

__always_inline
unsigned long shared_buck_calc_pwr_eff(struct energy_env *eenv, int dst_cpu,
		unsigned long max_util, struct cpumask *cpus, bool is_dsu_pwr_triggered)
{
	int pd_idx = cpumask_first(cpus);
	unsigned long pwr_eff;
	unsigned long gear_max_util = -1;
	unsigned long dsu_volt = -1, pd_volt = -1, gear_volt = -1, extern_volt = -1;
	int dst_idx, shared_buck_mode;
	unsigned long pd_freq = 0, gear_freq, scale_cpu;

	scale_cpu = arch_scale_cpu_capacity(pd_idx);

	pd_freq = pd_get_util_cpufreq(eenv, cpus, max_util,
			eenv->pds_cpu_cap[pd_idx], scale_cpu);

	if (eenv->wl_support && is_dsu_pwr_triggered) {
		dsu_volt = update_dsu_status(eenv, false, pd_freq, pd_idx, dst_cpu);

		if (share_buck.gear_idx != eenv->gear_idx)
			dsu_volt = 0;
	} else {
		dsu_volt = 0;
	}

	if (!cpumask_equal(cpus, get_gear_cpumask(eenv->gear_idx))) {
		/* dvfs Vin/Vout */
		pd_volt = pd_get_freq_volt(pd_idx, pd_freq, false, eenv->wl_cpu);

		dst_idx = (dst_cpu >= 0) ? 1 : 0;
		gear_max_util = eenv->gear_max_util[eenv->gear_idx][dst_idx];
		gear_freq = pd_get_util_cpufreq(eenv, cpus, gear_max_util,
				eenv->pds_cpu_cap[pd_idx], scale_cpu);
		gear_volt = pd_get_freq_volt(pd_idx, gear_freq, false, eenv->wl_cpu);

		if (gear_volt-pd_volt < volt_diff) {
			extern_volt = max(gear_volt, dsu_volt);
			pwr_eff = calc_pwr_eff_v2(eenv, dst_cpu, max_util, pd_freq,
					cpus, extern_volt);
			shared_buck_mode = 1;
		} else {
			extern_volt = 0;
			pwr_eff = calc_pwr_eff_v2(eenv, dst_cpu, max_util, pd_freq,
					cpus, extern_volt);
			pwr_eff = ((pd_volt) ? (pwr_eff * gear_volt / pd_volt) : pwr_eff);
			shared_buck_mode = 2;
		}
	} else {
		extern_volt = dsu_volt;
		pwr_eff = calc_pwr_eff_v2(eenv, dst_cpu, max_util, pd_freq,
				cpus, extern_volt);
		shared_buck_mode = 0;
	}

	if (trace_sched_shared_buck_calc_pwr_eff_enabled())
		trace_sched_shared_buck_calc_pwr_eff(dst_cpu, pd_idx, cpus,
			eenv->wl_cpu, pwr_eff, shared_buck_mode, gear_max_util, max_util,
			gear_volt, pd_volt, dsu_volt, extern_volt);

	return pwr_eff;
}

#else
__always_inline
unsigned long calc_pwr_eff(int wl, int cpu, unsigned long task_util, int *val_s)
{
	return 0;
}

__always_inline
unsigned long calc_pwr_eff_v2(struct energy_env *eenv, int cpu, unsigned long max_util,
		struct cpumask *cpus, unsigned long extern_volt)
{
	return 0;
}

__always_inline
unsigned long shared_buck_calc_pwr_eff(struct energy_env *eenv, int dst_cpu,
		unsigned long max_util, struct cpumask *cpus, bool is_dsu_pwr_triggered)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
void mtk_pelt_rt_tp(void *data, struct rq *rq)
{
	if (!get_eas_hook())
		return;

	cpufreq_update_util(rq, 0);
}

static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

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

static inline s64 entity_key(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	return (s64)(se->vruntime - cfs_rq->min_vruntime);
}

int _entity_eligible(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	struct sched_entity *curr = cfs_rq->curr;
	s64 avg = cfs_rq->avg_vruntime;
	long load = cfs_rq->avg_load;

	if (curr && curr->on_rq) {
		unsigned long weight = scale_load_down(curr->load.weight);

		avg += entity_key(cfs_rq, curr) * weight;
		load += weight;
	}

	return avg >= entity_key(cfs_rq, se) * load;
}

void mtk_sched_switch(void *data, struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
	if (!get_eas_hook())
		return;

	if (next->pid == 0)
		per_cpu(sbb, rq->cpu)->active = 0;

	if (trace_sched_stat_vdeadline_enabled()) {
		if (prev->pid != 0 && next->pid != 0) {
			if (prev->prio > 99 && next->prio > 99) {
				if (_entity_eligible(&(rq->cfs), &(prev->se)))
					trace_sched_stat_vdeadline(prev, next);
			}
		}
	}

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	vip_sched_switch(prev, next, rq);
#endif /* CONFIG_MTK_SCHED_VIP_TASK */

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	flt_android_rvh_schedule(data, prev, next, rq);
#endif
}

void mtk_update_misfit_status(void *data, struct task_struct *p, struct rq *rq, bool *need_update)
{
	unsigned long util, uclamp_min, uclamp_max, capacity, misfit_task_load = 0;
	int fits;

	*need_update = false;

	if (!p || p->nr_cpus_allowed == 1) {
		rq->misfit_task_load = 0;
		rq->misfit_reason = -1;
		return;
	}

	util = task_util_est(p);
	uclamp_min = uclamp_eff_value(p, UCLAMP_MIN);
	uclamp_max = uclamp_eff_value(p, UCLAMP_MAX);
	capacity = capacity_of(cpu_of(rq));
	fits = util_fits_capacity(util, uclamp_min, uclamp_max, capacity, cpu_of(rq));
	if (fits > 0) {
		rq->misfit_task_load = 0;
		rq->misfit_reason = -1;
		goto out;
	}

	/*
	 * Make sure that misfit_task_load will not be null even if
	 * task_h_load() returns 0.
	 */
	misfit_task_load = task_h_load(p);
	rq->misfit_task_load = max_t(unsigned long, misfit_task_load, 1);
	rq->misfit_reason = MISFIT_PERF;

out:
	if (trace_sched_mtk_update_misfit_status_enabled())
		trace_sched_mtk_update_misfit_status(cpu_of(rq), fits, p->pid, util,
			uclamp_min, uclamp_max, capacity, misfit_task_load);

}
#endif /* CONFIG_MTK_EAS */

int set_util_est_ctrl(bool enable)
{
	sysctl_util_est = enable;
	return 0;
}

int set_cpus_allowed_ptr_by_kernel(struct task_struct *p, const struct cpumask *new_mask)
{
	struct cpumask *kernel_allowed_mask;
	int ret;

	if (!p)
		return -EINVAL;
	kernel_allowed_mask = &((struct mtk_task *) p->android_vendor_data1)->kernel_allowed_mask;
	cpumask_copy(kernel_allowed_mask, new_mask);
	ret = set_cpus_allowed_ptr(p, new_mask);
	return ret;
}
EXPORT_SYMBOL_GPL(set_cpus_allowed_ptr_by_kernel);
