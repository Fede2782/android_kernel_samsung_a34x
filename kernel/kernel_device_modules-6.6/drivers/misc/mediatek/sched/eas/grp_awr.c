// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
static int grp_awr_init_finished;
static int grp_awr_marg_ctrl;
static int grp_awr_marg;
static int **pcpu_pgrp_u;
static int **pger_pgrp_u;
static int **pcpu_pgrp_adpt_rto;
static int **pcpu_pgrp_tar_u;
static int **pcpu_pgrp_marg;
static int **pcpu_pgrp_act_rto_cap;
static int **userdefined_pcpu_pgrp_act_rto_cap;
static int *pgrp_hint;
static int *pcpu_o_u;
static int *pgrp_tar_act_rto_cap;
static int *pgrp_parallel_u;
static int *map_cpu_ger;
static int **pcpu_pgrp_wetin;
static int **pcpu_pgrp_tar_u_grp_m;
static int *pgrp_tar_u_m;
static int **grp_margin;
static int *cpu_tar_util;
static int *pcpu_grp_wetin_sum;
static int *cap_min;
static int *cap_max;
static int **converge_thr_cap;
static int **converge_thr_freq;
static int **margin_for_min_opp;
static DEFINE_MUTEX(ta_ctrl_mutex);

void (*grp_awr_update_group_util_hook)(int nr_cpus,
	int nr_grps, int **pcpu_pgrp_u, int **pger_pgrp_u,
	int *pgrp_hint, int **pcpu_pgrp_marg, int **pcpu_pgrp_adpt_rto,
	int **pcpu_pgrp_tar_u, int *map_cpu_ger,
	int top_grp_aware, int **pcpu_pgrp_wetin, int *pcpu_grp_wetin_sum,
	int **pcpu_pgrp_tar_u_grp_m, int *pcpu_o_u, int **margin_for_min_opp,
	int **converge_thr_cap, int **grp_margin, int *cap_min, int *pgrp_tar_u_m,
	int *cpu_tar_util, int shift, int weighting, int *pgrp_parallel_u);
EXPORT_SYMBOL(grp_awr_update_group_util_hook);
void (*grp_awr_update_cpu_tar_util_hook)(int cpu, int nr_grps, int *pcpu_tar_u,
	int *group_nr_running, int **pcpu_pgrp_tar_u, int *pcpu_o_u);
EXPORT_SYMBOL(grp_awr_update_cpu_tar_util_hook);

void set_grp_awr_marg_ctrl(int val)
{
	grp_awr_marg_ctrl = val; /* 20 for 20% margin */
	grp_awr_marg = ((SCHED_CAPACITY_SCALE * 100) / (100 - val));
}
EXPORT_SYMBOL(set_grp_awr_marg_ctrl);
int get_grp_awr_marg_ctrl(void)
{
	return grp_awr_marg_ctrl;
}
EXPORT_SYMBOL(get_grp_awr_marg_ctrl);

static int gas_enable;
static int gas_ctrl_refcnt;
static int gas_force_ctrl;

#include <linux/ftrace.h>
#include <linux/kallsyms.h>

/*
 * GAS enable/disable control function
 * force_ctrl = 0 => normal control GAS by val, enable/disable by refcnt
 * force_ctrl = 1
 * val:1 => force enable GAS
 * val:0 => force disable GAS
 * val:-1 => reset force ctrl,  enable/disable by refcnt
 */
void set_top_grp_aware(int val, int force_ctrl)
{
	int i = 0;
	unsigned long ip;
	char sym[KSYM_SYMBOL_LEN];

	/* sanity check for input*/
	if (unlikely((force_ctrl != 0 && force_ctrl != 1) ||
		(val != 1 && val != 0 &&  val != -1)))
		return;

	mutex_lock(&ta_ctrl_mutex);

	/* force control enable/disable/reset */
	if (force_ctrl == 1) {
		if (val == -1) {
			gas_force_ctrl = 0;
			if (gas_ctrl_refcnt > 0) {
				if (!gas_enable) {
					flt_set_grp_ctrl(1);
					gas_enable = 1;
				}
			} else {
				if (gas_enable) {
					flt_set_grp_ctrl(0);
					gas_enable = 0;
					reset_grp_awr_margin();
				}
			}
		} else {
			gas_force_ctrl = 1;
			if (val == 1) {
				if (!gas_enable) {
					flt_set_grp_ctrl(1);
					gas_enable = 1;
				}
			} else if (val == 0) {
				if (gas_enable) {
					flt_set_grp_ctrl(0);
					gas_enable = 0;
					reset_grp_awr_margin();
				}
			}
		}
	} else {
		/* increase/decrease refcnt for normal control usage */
		if (val)
			++gas_ctrl_refcnt;
		else
			--gas_ctrl_refcnt;
	}

	/* normal control (enable/disable) GAS by refcnt */
	if (force_ctrl == 0 && gas_force_ctrl == 0) {
		/* if refcnt >0 , force on flt, else follow of setting */
		if (gas_ctrl_refcnt > 0) {
			if (!gas_enable) {
				flt_set_grp_ctrl(1);
				gas_enable = 1;
			}
		} else {
			if (gas_enable) {
				flt_set_grp_ctrl(0);
				gas_enable = 0;
				reset_grp_awr_margin();
			}
		}
	}
	if (trace_sugov_ext_ta_ctrl_enabled())
		trace_sugov_ext_ta_ctrl(val, force_ctrl, gas_ctrl_refcnt, gas_enable);

	if (trace_sugov_ext_ta_ctrl_caller_enabled()) {
		ip = (unsigned long) ftrace_return_address(i);
		memset(sym, 0, KSYM_SYMBOL_LEN);
		sprint_symbol(sym, ip);
		trace_sugov_ext_ta_ctrl_caller(sym);
	}
	mutex_unlock(&ta_ctrl_mutex);
}
EXPORT_SYMBOL(set_top_grp_aware);
int get_top_grp_aware(void)
{
	return gas_enable;
}
EXPORT_SYMBOL(get_top_grp_aware);

int get_top_grp_aware_refcnt(void)
{
	return gas_ctrl_refcnt;
}
EXPORT_SYMBOL(get_top_grp_aware_refcnt);

void set_grp_awr_thr(int gear_id, int group_id, int freq)
{
	unsigned int cpu_idx;
	int opp;

	if (grp_awr_init_finished == false ||
		gear_id == -1 ||
		gas_force_ctrl == 1 ||
		group_id >= GROUP_ID_RECORD_MAX)
		return;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		if (map_cpu_ger[cpu_idx] == gear_id) {
			if (freq == -1) {
				converge_thr_cap[cpu_idx][group_id] = (cap_max[cpu_idx] * 64) / 100;
				opp = pd_util2opp(cpu_idx, converge_thr_cap[cpu_idx][group_id],
					true, 0, NULL, true, DPT_CALL_SET_GRP_AWR_THR);
			} else
				opp = pd_get_freq_opp_legacy_type(0, cpu_idx, freq);
			converge_thr_cap[cpu_idx][group_id] = pd_opp2cap(cpu_idx, opp,
				true, 0, NULL, true, DPT_CALL_SET_GRP_AWR_THR);
			converge_thr_freq[cpu_idx][group_id] = pd_opp2freq(cpu_idx, opp, true, 0);
		}
}
EXPORT_SYMBOL(set_grp_awr_thr);
int get_grp_awr_thr(int gear_id, int group_id)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return -1;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		if (map_cpu_ger[cpu_idx] == gear_id)
			return converge_thr_cap[cpu_idx][group_id];
	return 0;
}
EXPORT_SYMBOL(get_grp_awr_thr);
int get_grp_awr_thr_freq(int gear_id, int group_id)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return -1;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		if (map_cpu_ger[cpu_idx] == gear_id)
			return converge_thr_freq[cpu_idx][group_id];
	return 0;
}
EXPORT_SYMBOL(get_grp_awr_thr_freq);

void set_grp_awr_min_opp_margin(int gear_id, int group_id, int val)
{
	int cpu_idx;

	if (grp_awr_init_finished == false ||
		gear_id == -1 ||
		gas_force_ctrl == 1 ||
		group_id >= GROUP_ID_RECORD_MAX)
		return;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		if (map_cpu_ger[cpu_idx] == gear_id) {
			if (val == -1) {
				if (cap_max[cpu_idx] == SCHED_CAPACITY_SCALE)
					margin_for_min_opp[cpu_idx][group_id] =
						SCHED_CAPACITY_SCALE;
				else
					margin_for_min_opp[cpu_idx][group_id] =
						SCHED_CAPACITY_SCALE + (SCHED_CAPACITY_SCALE >> 2);
			} else
				margin_for_min_opp[cpu_idx][group_id] = val;
		}
}
EXPORT_SYMBOL(set_grp_awr_min_opp_margin);

int get_grp_awr_min_opp_margin(int gear_id, int group_id)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return -1;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		if (map_cpu_ger[cpu_idx] == gear_id)
			return margin_for_min_opp[cpu_idx][group_id];
	return 0;
}
EXPORT_SYMBOL(get_grp_awr_min_opp_margin);

int reset_grp_awr_margin(void)
{
	int cpu_idx, grp_idx, tmp = -1;


	if (grp_awr_init_finished == false)
		return -1;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		if (tmp == map_cpu_ger[cpu_idx])
			continue;
		tmp = map_cpu_ger[cpu_idx];
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++) {
			set_grp_awr_thr(map_cpu_ger[cpu_idx], grp_idx, -1);
			set_grp_awr_min_opp_margin(map_cpu_ger[cpu_idx], grp_idx, -1);
		}
	}
	return 0;
}
EXPORT_SYMBOL(reset_grp_awr_margin);

void grp_awr_update_grp_awr_util(void)
{
	int cpu_idx, grp_idx, tmp = -1;
	int weighting = flt_get_grp_weight();

	if (grp_awr_init_finished == false)
		return;
	for_each_possible_cpu(cpu_idx) {
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
			if(userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] == -1)
				pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] =
					get_cpu_active_ratio_cap(cpu_idx);
			else
				pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] =
					userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx];
		if (map_cpu_ger[cpu_idx] == tmp)
			continue;
		tmp = map_cpu_ger[cpu_idx];
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
			pger_pgrp_u[map_cpu_ger[cpu_idx]][grp_idx] = 0;
	}

	for_each_possible_cpu(cpu_idx) {
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++) {
			pcpu_pgrp_u[cpu_idx][grp_idx] =
				flt_sched_get_cpu_group(cpu_idx, grp_idx);

			pger_pgrp_u[map_cpu_ger[cpu_idx]][grp_idx] +=
				pcpu_pgrp_u[cpu_idx][grp_idx];

			if (grp_awr_marg_ctrl) {
				pcpu_pgrp_marg[cpu_idx][grp_idx] = grp_awr_marg;
				pcpu_pgrp_adpt_rto[cpu_idx][grp_idx] = SCHED_CAPACITY_SCALE;
			} else {
				pcpu_pgrp_adpt_rto[cpu_idx][grp_idx] =
					((pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx]
					<< SCHED_CAPACITY_SHIFT)
					/ pgrp_tar_act_rto_cap[grp_idx]);
			}
		}
	}

	if (trace_sugov_ext_pger_pgrp_u_enabled()) {
		for_each_possible_cpu(cpu_idx) {
			if (map_cpu_ger[cpu_idx] == tmp)
				continue;
			tmp = map_cpu_ger[cpu_idx];
			trace_sugov_ext_pger_pgrp_u(map_cpu_ger[cpu_idx],
				cpu_idx, pger_pgrp_u[map_cpu_ger[cpu_idx]],
				converge_thr_cap[cpu_idx],
				margin_for_min_opp[cpu_idx]);
		}
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++) {
		pgrp_parallel_u[grp_idx] =
			clamp_val(flt_get_max_group(grp_idx), 0, SCHED_CAPACITY_SCALE);
		if (grp_awr_marg_ctrl)
			pgrp_hint[grp_idx] = 0;
		else
			pgrp_hint[grp_idx] = flt_get_gp_hint(grp_idx);
	}

	if (trace_sugov_ext_pgrp_hint_enabled())
		trace_sugov_ext_pgrp_hint(pgrp_hint);

	for_each_possible_cpu(cpu_idx) {
		pcpu_o_u[cpu_idx] = flt_get_cpu_o(cpu_idx);
		set_grp_high_freq(cpu_idx, false);
	}

	if (grp_awr_update_group_util_hook)
		grp_awr_update_group_util_hook(FLT_NR_CPUS, GROUP_ID_RECORD_MAX,
			pcpu_pgrp_u, pger_pgrp_u, pgrp_hint,
			pcpu_pgrp_marg, pcpu_pgrp_adpt_rto, pcpu_pgrp_tar_u, map_cpu_ger,
			gas_enable, pcpu_pgrp_wetin, pcpu_grp_wetin_sum, pcpu_pgrp_tar_u_grp_m,
			pcpu_o_u, margin_for_min_opp, converge_thr_cap, grp_margin,
			cap_min, pgrp_tar_u_m, cpu_tar_util, 3, weighting, pgrp_parallel_u);

	if (weighting) {
		for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
			if (cpu_tar_util[cpu_idx] > cap_max[cpu_idx] && gas_enable)
				set_grp_high_freq(map_cpu_ger[cpu_idx], true);
	} else {
		for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
			if (gas_enable)
				set_grp_high_freq(map_cpu_ger[cpu_idx], true);
	}

	if (trace_sugov_ext_pcpu_pgrp_u_rto_marg_enabled()) {
		for_each_possible_cpu(cpu_idx)
			trace_sugov_ext_pcpu_pgrp_u_rto_marg(cpu_idx, pcpu_pgrp_u[cpu_idx],
				pcpu_pgrp_adpt_rto[cpu_idx],
				pcpu_pgrp_marg[cpu_idx], pcpu_o_u[cpu_idx],
				pcpu_pgrp_wetin[cpu_idx], pcpu_pgrp_tar_u[cpu_idx],
				cpu_tar_util[cpu_idx], grp_margin[cpu_idx],
				weighting, pgrp_parallel_u,
				get_grp_high_freq(map_cpu_ger[cpu_idx]));
	}
}

int grp_awr_get_grp_tar_util(int grp_idx)
{

	if (grp_awr_init_finished == false)
		return 0;

	return pgrp_tar_u_m[grp_idx];
}

void grp_awr_update_cpu_tar_util(int cpu)
{
	struct flt_rq *fsrq;

	if (grp_awr_init_finished == false)
		return;
	fsrq = &per_cpu(flt_rq, cpu);

	if (grp_awr_update_cpu_tar_util_hook)
		grp_awr_update_cpu_tar_util_hook(cpu, GROUP_ID_RECORD_MAX, &fsrq->cpu_tar_util,
			fsrq->group_nr_running, pcpu_pgrp_tar_u, pcpu_o_u);

	if (trace_sugov_ext_tar_cal_enabled())
		trace_sugov_ext_tar_cal(cpu, fsrq->cpu_tar_util, pcpu_pgrp_tar_u[cpu],
			fsrq->group_nr_running, pcpu_o_u[cpu]);
}

void set_group_target_active_ratio_pct(int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	pgrp_tar_act_rto_cap[grp_idx] = ((clamp_val(val, 1, 100) << SCHED_CAPACITY_SHIFT) / 100);
}
EXPORT_SYMBOL(set_group_target_active_ratio_pct);

void set_group_target_active_ratio_cap(int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	pgrp_tar_act_rto_cap[grp_idx] = clamp_val(val, 1, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL(set_group_target_active_ratio_cap);

void set_cpu_group_active_ratio_pct(int cpu, int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	if (val < 0)
		userdefined_pcpu_pgrp_act_rto_cap[cpu][grp_idx] = -1;
	else
		userdefined_pcpu_pgrp_act_rto_cap[cpu][grp_idx] =
			((clamp_val(val, 0, 100) << SCHED_CAPACITY_SHIFT) / 100);
}
EXPORT_SYMBOL(set_cpu_group_active_ratio_pct);

void set_cpu_group_active_ratio_cap(int cpu, int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	if (val < 0)
		userdefined_pcpu_pgrp_act_rto_cap[cpu][grp_idx] = -1;
	else
		userdefined_pcpu_pgrp_act_rto_cap[cpu][grp_idx] =
			clamp_val(val, 0, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL(set_cpu_group_active_ratio_cap);

void set_group_active_ratio_pct(int grp_idx, int val)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return;
	for_each_possible_cpu(cpu_idx) {
		if (val < 0)
			userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] = -1;
		else
			userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] =
				((clamp_val(val, 0, 100) << SCHED_CAPACITY_SHIFT) / 100);
	}
}
EXPORT_SYMBOL(set_group_active_ratio_pct);

void set_group_active_ratio_cap(int grp_idx, int val)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return;
	for_each_possible_cpu(cpu_idx) {
		if (val < 0)
			userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] = -1;
		else
			userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] =
				clamp_val(val, 0, SCHED_CAPACITY_SCALE);
	}
}
EXPORT_SYMBOL(set_group_active_ratio_cap);

int grp_awr_init(void)
{
	unsigned int cpu_idx, grp_idx;

	pr_info("group aware init\n");
	/* per cpu per group data*/
	pcpu_pgrp_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pger_pgrp_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_adpt_rto = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_tar_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_marg = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_act_rto_cap = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	userdefined_pcpu_pgrp_act_rto_cap = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_wetin = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_tar_u_grp_m = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	grp_margin = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	cpu_tar_util = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	pcpu_grp_wetin_sum = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	cap_min = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	cap_max = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	converge_thr_cap = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	converge_thr_freq = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	margin_for_min_opp = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);

	/* per cpu data*/
	pcpu_o_u = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	map_cpu_ger = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);

	/* per group data*/
	pgrp_hint = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
	pgrp_tar_act_rto_cap = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
	pgrp_parallel_u = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
	pgrp_tar_u_m = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);

	/* per cpu per group data*/
	for_each_possible_cpu(cpu_idx) {
		pcpu_pgrp_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pger_pgrp_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_adpt_rto[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_tar_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_marg[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_act_rto_cap[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		userdefined_pcpu_pgrp_act_rto_cap[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		map_cpu_ger[cpu_idx] = topology_cluster_id(cpu_idx);
		grp_margin[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		margin_for_min_opp[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		converge_thr_cap[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		converge_thr_freq[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_wetin[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_tar_u_grp_m[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);

		cap_max[cpu_idx] = pd_opp2cap(cpu_idx, 0, true, 0, NULL, true, DPT_CALL_GRP_AWR_INIT);
		cap_min[cpu_idx] = pd_opp2cap(cpu_idx, INT_MAX, true, 0, NULL, true, DPT_CALL_GRP_AWR_INIT);
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
			userdefined_pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] = -1;
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
		pgrp_tar_act_rto_cap[grp_idx] = ((85 << SCHED_CAPACITY_SHIFT) / 100);

	sugov_grp_awr_update_cpu_tar_util_hook = grp_awr_update_cpu_tar_util;

	grp_awr_init_finished = true;
	reset_grp_awr_margin();
	return 0;
}
#endif
