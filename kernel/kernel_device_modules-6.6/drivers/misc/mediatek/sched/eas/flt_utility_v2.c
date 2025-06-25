// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/energy_model.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <sugov/cpufreq.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"
#include "flt_api.h"
#include "group.h"
#include "flt_utility_api.h"
#include "flt_cal.h"
#include "eas_trace.h"

#define CPU_NUM		8
#define RESERVED_LEN	96
#define WP_LEN		16
#define WC_LEN		16
#define WC_MASK	0xffff
#define CPU_S		(0)
#define GP_NIDS		(CPU_S + (PER_ENTRY * CPU_NUM))
#define GP_NIDWP	(GP_NIDS + (PER_ENTRY))
#define CPU_R		(GP_NIDWP + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define GP_R		(CPU_R + (PER_ENTRY * CPU_NUM))
#define GP_H		(GP_R + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define FLT_VALID	(GP_H + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define GP_RWP		(FLT_VALID + PER_ENTRY)
#define AP_CPU_SETTING_ADDR (GP_RWP + (PER_ENTRY * GROUP_ID_RECORD_MAX) + RESERVED_LEN)
#define AP_GP_SETTING_STA_ADDR (AP_CPU_SETTING_ADDR + (PER_ENTRY * CPU_NUM))
#define AP_FLT_CTL (AP_GP_SETTING_STA_ADDR + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define AP_WS_CTL (AP_FLT_CTL + PER_ENTRY)

#define FLT_MODE2_EN 2
#define DEFAULT_WS 4
#define CPU_DEFAULT_WC GROUP_RAVG_HIST_SIZE_MAX
#define CPU_DEFAULT_WP WP_MODE_4

#undef FLT_MODE_SEL
#define FLT_MODE_SEL FLT_MODE2_EN
#undef FLT_VER
#define FLT_VER	BIT(1)

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
int (*grp_cal_tra)(int x, unsigned int y);
EXPORT_SYMBOL(grp_cal_tra);
#endif
static int FLT_FN(nid) = FLT_GP_NID;
static int FLT_FN(grp_wt) = FLT_GP_WT;

static int FLT_FN(is_valid)(void)
{
	int res = 0;

	res = flt_get_data(FLT_VALID);
	return res;
}

static int FLT_FN(get_window_size)(void)
{
	int res = 0;

	res = flt_get_data(AP_WS_CTL);

	return res;
}

static int FLT_FN(set_window_size)(int ws)
{
	int res = 0;

	flt_update_data(ws, AP_WS_CTL);

	return res;
}

static int FLT_FN(set_mode)(u32 mode)
{
	int res = 0;

	if (mode == FLT_MODE_SEL)
		flt_update_data(FLT_VER, AP_FLT_CTL);
	else
		flt_update_data(0, AP_FLT_CTL);
	return res;
}

static int FLT_FN(get_mode)(void)
{
	int res = 0;

	res = flt_get_data(AP_FLT_CTL);

	return res;
}

static int FLT_FN(sched_set_group_policy_eas)(int grp_id, int ws, int wp, int wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;

	offset = grp_id * PER_ENTRY;
	update_data = (wp << WP_LEN) | wc;
	flt_update_data(ws, AP_WS_CTL);
	flt_update_data(update_data, AP_GP_SETTING_STA_ADDR + offset);

	return res;
}

static int FLT_FN(sched_get_group_policy_eas)(int grp_id, int *ws, int *wp, int *wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;
	offset = grp_id * PER_ENTRY;
	update_data = flt_get_data(AP_GP_SETTING_STA_ADDR + offset);
	*ws = flt_get_data(AP_WS_CTL);
	*wp = update_data >> WP_LEN;
	*wc = update_data & WC_MASK;

	return res;
}

static int FLT_FN(sched_set_cpu_policy_eas)(int cpu, int ws, int wp, int wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -1;
	offset = cpu * PER_ENTRY;
	update_data = (wp << WP_LEN) | wc;
	flt_update_data(ws, AP_WS_CTL);
	flt_update_data(update_data, AP_CPU_SETTING_ADDR + offset);
	return res;
}

static int FLT_FN(sched_get_cpu_policy_eas)(int cpu, int *ws, int *wp, int *wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -1;
	offset = cpu * PER_ENTRY;
	update_data = flt_get_data(AP_CPU_SETTING_ADDR + offset);
	*ws = flt_get_data(AP_WS_CTL);
	*wp = update_data >> WP_LEN;
	*wc = update_data & WC_MASK;

	return res;
}

static int FLT_FN(get_sum_group)(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || FLT_FN(is_valid)() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	if (FLT_FN(nid) == FLT_GP_NID)
		res = flt_get_data(GP_NIDWP + offset);
	else
		res = flt_get_data(GP_RWP + offset);

	return res;
}

static int FLT_FN(get_total_group)(void)
{
	int res = 0;

	if (FLT_FN(is_valid)() != 1)
		goto out;

	res = flt_get_data(GP_NIDS);
out:
	return res;
}

static int FLT_FN(get_grp_hint)(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || FLT_FN(is_valid)() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	res = flt_get_data(GP_H + offset);

	return res;
}

static int FLT_FN(get_cpu_r)(int cpu)
{
	int res = 0;
	unsigned int offset;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask) || FLT_FN(is_valid)() != 1)
		return -1;

	offset = cpu * PER_ENTRY;
	res = flt_get_data(CPU_R + offset);
	return res;
}

static int FLT_FN(get_grp_r)(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || FLT_FN(is_valid)() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	res = flt_get_data(GP_R + offset);

	return res;
}

int FLT_FN(sched_get_gear_sum_group_eas)(int gear_id, int group_id)
{
	unsigned int nr_gear, gear_idx;
	int pelt_util = 0, res = 0;
	u64 flt_util = 0, gear_util = 0, total_util = 0;

	if (group_id >= GROUP_ID_RECORD_MAX || group_id < 0)
		return -1;

	nr_gear = get_nr_gears();

	if (gear_id >= nr_gear || gear_id < 0)
		return -1;

	flt_util = flt_get_sum_group(group_id);

	for (gear_idx = 0; gear_idx < nr_gear; gear_idx++) {
		pelt_util = flt_get_gear_sum_pelt_group(gear_idx, group_id);
		if (gear_idx == gear_id)
			gear_util	= pelt_util;
		total_util += pelt_util;
	}
	if (total_util)
		res = (int)div64_u64(flt_util * gear_util, total_util);
	return res;
}

static int FLT_FN(get_cpu_by_wp)(int cpu)
{
	struct flt_rq *fsrq;
	int cpu_dmand_util = 0;

	if (unlikely(!cpumask_test_cpu(cpu, cpu_possible_mask)))
		return -1;

	fsrq = &per_cpu(flt_rq, cpu);

	cpu_dmand_util = READ_ONCE(fsrq->cpu_tar_util);
	return cpu_dmand_util;
}

static int FLT_FN(sched_get_cpu_group_eas)(int cpu_idx, int group_id)
{
	int res = 0, flt_util = 0;
	struct flt_rq *fsrq;
	u32 util_ratio = 0;

	if (group_id >= GROUP_ID_RECORD_MAX ||
		group_id < 0 ||
		!cpumask_test_cpu(cpu_idx, cpu_possible_mask))
		return -1;

	flt_util = FLT_FN(get_sum_group)(group_id);

	fsrq = &per_cpu(flt_rq, cpu_idx);
	if (FLT_FN(nid) == FLT_GP_NID)
		util_ratio = READ_ONCE(fsrq->group_util_ratio[group_id]);
	else
		util_ratio = READ_ONCE(fsrq->group_raw_util_ratio[group_id]);
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	if (grp_cal_tra)
		res = grp_cal_tra(flt_util, util_ratio);
#endif
	res = clamp_t(int, res, 0, cpu_cap_ceiling(cpu_idx));

	return res;
}

static int FLT_FN(get_o_util)(int cpu)
{
	int cpu_r = 0, grp_idx = 0, res, flt_util = 0;
	struct flt_rq *fsrq;
	u32 util_ratio[GROUP_ID_RECORD_MAX] = {0}, grp_r[GROUP_ID_RECORD_MAX] = {0}, total = 0;

	fsrq = &per_cpu(flt_rq, cpu);

	cpu_r = flt_get_cpu_r(cpu);

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		util_ratio[grp_idx] = READ_ONCE(fsrq->group_util_rtratio[grp_idx]);
		flt_util = flt_get_gp_r(grp_idx);
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (grp_cal_tra)
			grp_r[grp_idx] = grp_cal_tra(flt_util, util_ratio[grp_idx]);
#endif
		total += grp_r[grp_idx];
		if (trace_sched_flt_get_o_util_enabled())
			trace_sched_flt_get_o_util(cpu, cpu_r, grp_idx, util_ratio[grp_idx],
						flt_util, grp_r[grp_idx], total);
	}
	res = cpu_r - total;
	res = clamp_t(int, res, 0, cpu_cap_ceiling(cpu));

	return res;
}

static int FLT_FN(set_grp_dvfs_ctrl)(int set)
{
	/* control grp dvfs reference signal*/
	if (set)
		set_grp_dvfs_ctrl(FLT_MODE_SEL);
	else
		set_grp_dvfs_ctrl(0);
	return 0;
}

static int FLT_FN(setnid)(u32 mode)
{
	if (mode >= FLT_GP_NUM)
		return -1;
	FLT_FN(nid) = mode;
	return 0;
}

static u32 FLT_FN(getnid)(void)
{
	return FLT_FN(nid);
}

static int FLT_FN(res_init)(void)
{
	int cpu, i;

	flt_set_ws(DEFAULT_WS);
	for_each_possible_cpu(cpu)
		flt_sched_set_cpu_policy_eas(cpu, DEFAULT_WS, CPU_DEFAULT_WP, CPU_DEFAULT_WC);
	for (i = 0; i < GROUP_ID_RECORD_MAX; i++)
		flt_sched_set_group_policy_eas(i, DEFAULT_WS, GRP_DEFAULT_WP, GRP_DEFAULT_WC);
	flt_update_data(FLT_VER, AP_FLT_CTL);
	return 0;
}

static int FLT_FN(get_grp_weight)(void)
{
	return FLT_FN(grp_wt);
}

static int FLT_FN(get_grp_thr_weight)(void)
{
	int wt = 0;
	struct cpumask *gear_cpus;

	gear_cpus = get_gear_cpumask(0);
	wt = cpumask_weight(gear_cpus);

	return wt;
}

const struct flt_class FLT_FN(api_hooks) = {
	.flt_get_ws_api = FLT_FN(get_window_size),
	.flt_set_ws_api = FLT_FN(set_window_size),
	.flt_get_mode_api = FLT_FN(get_mode),
	.flt_set_mode_api = FLT_FN(set_mode),
	.flt_sched_set_group_policy_eas_api = FLT_FN(sched_set_group_policy_eas),
	.flt_sched_get_group_policy_eas_api = FLT_FN(sched_get_group_policy_eas),
	.flt_sched_set_cpu_policy_eas_api = FLT_FN(sched_set_cpu_policy_eas),
	.flt_sched_get_cpu_policy_eas_api = FLT_FN(sched_get_cpu_policy_eas),
	.flt_get_sum_group_api = FLT_FN(get_sum_group),
	.flt_sched_get_gear_sum_group_eas_api = FLT_FN(sched_get_gear_sum_group_eas),
	.flt_get_cpu_by_wp_api = FLT_FN(get_cpu_by_wp),
	.flt_sched_get_cpu_group_eas_api = FLT_FN(sched_get_cpu_group_eas),
	.flt_get_grp_h_eas_api = FLT_FN(get_grp_hint),
	.flt_get_cpu_r_api = FLT_FN(get_cpu_r),
	.flt_get_cpu_o_eas_api = FLT_FN(get_o_util),
	.flt_get_total_gp_api = FLT_FN(get_total_group),
	.flt_get_grp_r_eas_api = FLT_FN(get_grp_r),
	.flt_set_grp_dvfs_ctrl_api = FLT_FN(set_grp_dvfs_ctrl),
	.flt_setnid_eas_api = FLT_FN(setnid),
	.flt_getnid_eas_api = FLT_FN(getnid),
	.flt_res_init_api = FLT_FN(res_init),
	.flt_get_grp_weight_api = FLT_FN(get_grp_weight),
	.flt_get_grp_thr_weight_api = FLT_FN(get_grp_thr_weight),
};

void FLT_FN(register_api_hooks)(void) {
	flt_class_mode = &FLT_FN(api_hooks);
}
