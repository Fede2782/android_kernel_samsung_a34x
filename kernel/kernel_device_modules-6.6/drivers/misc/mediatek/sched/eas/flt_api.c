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
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"
#include "flt_api.h"
#include "eas_trace.h"

static struct flt_pm fltpmrec;
static bool flt_ctrl_force;
const struct flt_class *flt_class_mode;

int flt_get_ws(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_ws_api)
		return flt_class_mode->flt_get_ws_api();
	else
		return -1;
}

int flt_set_ws(int ws)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_set_ws_api)
		return flt_class_mode->flt_set_ws_api(ws);
	else
		return -1;
}

int flt_get_mode_io(void)
{
	if (flt_class_mode && flt_class_mode->flt_get_mode_api)
		return flt_class_mode->flt_get_mode_api();
	else
		return -1;
}

int flt_set_mode_io(u32 mode)
{
	if (flt_class_mode && flt_class_mode->flt_set_mode_api)
		return flt_class_mode->flt_set_mode_api(mode);
	else
		return -1;
}

int flt_sched_set_group_policy_eas(int grp_id, int ws,
					int wp, int wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_set_group_policy_eas_api)
		return flt_class_mode->flt_sched_set_group_policy_eas_api(grp_id, ws, wp, wc);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_set_group_policy_eas);

int flt_sched_get_group_policy_eas(int grp_id, int *ws,
					int *wp, int *wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_get_group_policy_eas_api)
		return flt_class_mode->flt_sched_get_group_policy_eas_api(grp_id, ws, wp, wc);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_group_policy_eas);

int flt_sched_set_cpu_policy_eas(int cpu, int ws,
					int wp, int wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_set_cpu_policy_eas_api)
		return flt_class_mode->flt_sched_set_cpu_policy_eas_api(cpu, ws, wp, wc);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_set_cpu_policy_eas);

int flt_sched_get_cpu_policy_eas(int cpu, int *ws,
					int *wp, int *wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_get_cpu_policy_eas_api)
		return flt_class_mode->flt_sched_get_cpu_policy_eas_api(cpu, ws, wp, wc);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_cpu_policy_eas);

int flt_get_sum_group(int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_sum_group_api)
		return flt_class_mode->flt_get_sum_group_api(grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_sum_group);

int flt_get_max_group(int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_max_group_api)
		return flt_class_mode->flt_get_max_group_api(grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_max_group);

int flt_get_gear_sum_pelt_group(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_gear_sum_pelt_group_api)
		return flt_class_mode->flt_get_gear_sum_pelt_group_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_sum_pelt_group);

int flt_get_gear_max_pelt_group(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_gear_max_pelt_group_api)
		return flt_class_mode->flt_get_gear_max_pelt_group_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_max_pelt_group);

int flt_get_gear_sum_pelt_group_cnt(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_gear_sum_pelt_group_cnt_api)
		return flt_class_mode->flt_get_gear_sum_pelt_group_cnt_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_sum_pelt_group_cnt);

int flt_get_gear_max_pelt_group_cnt(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_gear_max_pelt_group_cnt_api)
		return flt_class_mode->flt_get_gear_max_pelt_group_cnt_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_max_pelt_group_cnt);

int flt_sched_get_gear_sum_group_eas(int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_get_gear_sum_group_eas_api)
		return flt_class_mode->flt_sched_get_gear_sum_group_eas_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_gear_sum_group_eas);

int flt_sched_get_gear_max_group_eas(int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_get_gear_max_group_eas_api)
		return flt_class_mode->flt_sched_get_gear_max_group_eas_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_gear_max_group_eas);

int flt_sched_get_cpu_group_eas(int cpu, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_sched_get_cpu_group_eas_api)
		return flt_class_mode->flt_sched_get_cpu_group_eas_api(cpu, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_cpu_group_eas);

int flt_get_cpu_by_wp(int cpu)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_cpu_by_wp_api)
		return flt_class_mode->flt_get_cpu_by_wp_api(cpu);
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_cpu_by_wp);

int flt_get_task_by_wp(struct task_struct *p, int wc, int task_wp)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_task_by_wp_api)
		return flt_class_mode->flt_get_task_by_wp_api(p, wc, task_wp);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_task_by_wp);

unsigned long flt_get_cpu(int cpu)
{
	int cpu_demand = 0;

	cpu_demand = flt_get_cpu_by_wp(cpu);
	/* sanity check */
	if (unlikely(cpu_demand < 0)) {
		cpu_demand = 0;
		goto out;
	} else {
		goto out;
	}
out:
	if (trace_sched_flt_get_cpu_enabled())
		trace_sched_flt_get_cpu(cpu, cpu_demand);
	return cpu_demand;
}
EXPORT_SYMBOL(flt_get_cpu);

unsigned long flt_sched_get_cpu_group(int cpu, int grp_id)
{
	int gp_demand = 0;

	gp_demand = flt_sched_get_cpu_group_eas(cpu, grp_id);
	/* sanity check */
	if (unlikely(gp_demand < 0)) {
		gp_demand = 0;
		goto out;
	} else {
		goto out;
	}
out:
	if (trace_sched_flt_get_cpu_group_enabled())
		trace_sched_flt_get_cpu_group(cpu, grp_id, gp_demand);
	return gp_demand;
}
EXPORT_SYMBOL(flt_sched_get_cpu_group);

int flt_get_gp_hint(int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_grp_h_eas_api)
		return flt_class_mode->flt_get_grp_h_eas_api(grp_id);
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_gp_hint);

int flt_get_cpu_r(int cpu)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_cpu_r_api)
		return flt_class_mode->flt_get_cpu_r_api(cpu);
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_cpu_r);

int flt_get_cpu_o(int cpu)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_cpu_o_eas_api)
		return flt_class_mode->flt_get_cpu_o_eas_api(cpu);
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_cpu_o);

int flt_get_total_gp(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_total_gp_api)
		return flt_class_mode->flt_get_total_gp_api();
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_total_gp);

int flt_get_gp_r(int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_grp_r_eas_api)
		return flt_class_mode->flt_get_grp_r_eas_api(grp_id);
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_gp_r);

u32 flt_getnid(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return 0xffff;
	else if (flt_class_mode && flt_class_mode->flt_getnid_eas_api)
		return flt_class_mode->flt_getnid_eas_api();
	else
		return 0xffff;
}
EXPORT_SYMBOL(flt_getnid);

int flt_setnid(u32 mode)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_setnid_eas_api)
		return flt_class_mode->flt_setnid_eas_api(mode);
	else
		return -1;
}
EXPORT_SYMBOL(flt_setnid);

int flt_get_grp_weight(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_grp_weight_api)
		return flt_class_mode->flt_get_grp_weight_api();
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_grp_weight);

int flt_set_grp_weight(int set)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_set_grp_weight_api)
		return flt_class_mode->flt_set_grp_weight_api(set);
	else
		return -1;
}
EXPORT_SYMBOL(flt_set_grp_weight);

int flt_get_grp_thr_weight(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_get_grp_thr_weight_api)
		return flt_class_mode->flt_get_grp_thr_weight_api();
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_grp_thr_weight);

int flt_res_init(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_res_init_api)
		return flt_class_mode->flt_res_init_api();
	else
		return -1;
}
EXPORT_SYMBOL(flt_res_init);

int flt_set_grp_dvfs_ctrl(int set)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_class_mode && flt_class_mode->flt_set_grp_dvfs_ctrl_api)
		return flt_class_mode->flt_set_grp_dvfs_ctrl_api(set);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_set_grp_dvfs_ctrl);

void flt_ctl(int set)
{
	if (!flt_ctrl_force)
		flt_set_grp_dvfs_ctrl(set);
}
EXPORT_SYMBOL_GPL(flt_ctl);

void flt_ctrl_force_set(int set)
{
	flt_ctrl_force = (set == 0) ? false : true;
}
EXPORT_SYMBOL_GPL(flt_ctrl_force_set);

bool flt_ctrl_force_get(void)
{
	return flt_ctrl_force;
}
EXPORT_SYMBOL_GPL(flt_ctrl_force_get);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
void register_sugov_hooks(void)
{
	flt_get_cpu_util_hook = flt_get_cpu;
	flt_sched_get_cpu_group_util_eas_hook = flt_sched_get_cpu_group;
	flt_get_fpsgo_boosting = flt_ctl;
}
#endif

void flt_resume_notify(void)
{
	fltpmrec.ktime_suspended = false;
}

void flt_suspend_notify(void)
{
	fltpmrec.ktime_last = ktime_get();
	fltpmrec.ktime_suspended = true;
}

void flt_get_pm_status(struct flt_pm *fltpm)
{
	if (fltpm) {
		fltpm->ktime_suspended = fltpmrec.ktime_suspended;
		fltpm->ktime_last = fltpmrec.ktime_last;
	}
}
EXPORT_SYMBOL(flt_get_pm_status);

void flt_update_data(unsigned int data, unsigned int offset)
{
	void __iomem *flt_adr;
	unsigned long long len;

	if (!IS_ALIGNED(offset, PER_ENTRY))
		return;
	len = get_flt_xrg_size();
	flt_adr = get_flt_xrg();
	if (flt_adr && offset <= len)
		iowrite32(data, flt_adr + offset);
}

unsigned int flt_get_data(unsigned int offset)
{
	void __iomem *flt_adr;
	unsigned int res = 0;
	unsigned long long len;

	if (!IS_ALIGNED(offset, PER_ENTRY))
		return res;
	len = get_flt_xrg_size();
	flt_adr = get_flt_xrg();
	if (flt_adr && offset <= len)
		res = ioread32(flt_adr + offset);

	return res;
}
