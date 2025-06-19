// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include "sugov/cpufreq.h"
#include "dsu_interface.h"
/* You can define module value in sched_version_ctrl.h */
#include "sched_version_ctrl.h"

bool _vip_enable;
bool _gear_hints_enable;
bool _updown_migration_enable;
bool _skip_hiIRQ_enable;
bool _rt_aggre_preempt_enable;
bool _post_init_util_ctl;
bool _percore_l3_bw;
bool _dsu_pwr_enable;
bool _legacy_api_support;

int init_sched_ctrl(void)
{
	struct device_node *eas_node;
	int sched_ctrl = 0;
	int ret = 0;
	int legacy_api_support = 0;

	eas_node = of_find_node_by_name(NULL, "eas-info");
	if (eas_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
	} else {
		ret = of_property_read_u32(eas_node, "version", &sched_ctrl);
		if (ret < 0)
			pr_info("no share_buck err_code=%d %s\n", ret,  __func__);

		_legacy_api_support = false;
		ret = of_property_read_u32(eas_node, "legacy-api-support", &legacy_api_support);
		if (ret < 0)
			pr_info("no legacy-api-support err_code=%d %s\n", ret,  __func__);
		if (legacy_api_support)
			_legacy_api_support = true;
		pr_info("legacy_api_support = %d %s\n", _legacy_api_support,  __func__);
	}
	switch(sched_ctrl) {
	case EAS_5_5:
		am_support = 0;
		grp_dvfs_support_mode = 0;
		// TODO
		_gear_hints_enable = false;
		_updown_migration_enable = false;
		_skip_hiIRQ_enable = false;
		_rt_aggre_preempt_enable = false;
		_vip_enable = false;
		_post_init_util_ctl = false;
		_percore_l3_bw = false;
		_dsu_pwr_enable = false;
		break;
	case EAS_5_5_1:
		am_support = 0;
		grp_dvfs_support_mode = 0;
		// TODO
		_gear_hints_enable = false;
		_updown_migration_enable = true;
		_skip_hiIRQ_enable = false;
		_rt_aggre_preempt_enable = false;
		_vip_enable = false;
		_post_init_util_ctl = false;
		_percore_l3_bw = false;
		_dsu_pwr_enable = false;
		break;
	case EAS_6_1:
		am_support = 1;
		grp_dvfs_support_mode = 1;
		// TODO
		_gear_hints_enable = true;
		_updown_migration_enable = true;
		_skip_hiIRQ_enable = true;
		_rt_aggre_preempt_enable = false;
		_vip_enable = true;
		_post_init_util_ctl = true;
		_percore_l3_bw = false;
		_dsu_pwr_enable = true;
		break;
	case EAS_6_5:
		am_support = 1;
		grp_dvfs_support_mode = 1;
		// TODO
		_gear_hints_enable = true;
		_updown_migration_enable = true;
		_skip_hiIRQ_enable = true;
		_rt_aggre_preempt_enable = false;
		_vip_enable = true;
		_post_init_util_ctl = true;
		_percore_l3_bw = true;
		_dsu_pwr_enable = true;
		break;
	default:
		am_support = 0;
		grp_dvfs_support_mode = 0;
		// TODO
		_gear_hints_enable = false;
		_updown_migration_enable = false;
		_skip_hiIRQ_enable = false;
		_rt_aggre_preempt_enable = false;
		_vip_enable = false;
		_post_init_util_ctl = false;
		_percore_l3_bw = false;
		break;
	}
	return 0;
}

bool legacy_api_support_get(void)
{
	return _legacy_api_support;
}
EXPORT_SYMBOL_GPL(legacy_api_support_get);

bool sched_vip_enable_get(void)
{
	return _vip_enable;
}
EXPORT_SYMBOL_GPL(sched_vip_enable_get);

bool sched_gear_hints_enable_get(void)
{
	return _gear_hints_enable;
}
EXPORT_SYMBOL_GPL(sched_gear_hints_enable_get);

bool sched_updown_migration_enable_get(void)
{
	return _updown_migration_enable;
}
EXPORT_SYMBOL_GPL(sched_updown_migration_enable_get);

bool sched_skip_hiIRQ_enable_get(void)
{
	return _skip_hiIRQ_enable;
}
EXPORT_SYMBOL_GPL(sched_skip_hiIRQ_enable_get);

bool sched_rt_aggre_preempt_enable_get(void)
{
	return _rt_aggre_preempt_enable;
}
EXPORT_SYMBOL_GPL(sched_rt_aggre_preempt_enable_get);

bool sched_post_init_util_enable_get(void)
{
	return _post_init_util_ctl;
}
EXPORT_SYMBOL_GPL(sched_post_init_util_enable_get);

void sched_post_init_util_set(bool enable)
{
	_post_init_util_ctl = enable;
}
EXPORT_SYMBOL_GPL(sched_post_init_util_set);

bool sched_percore_l3_bw_get(void)
{
	return _percore_l3_bw;
}
EXPORT_SYMBOL_GPL(sched_percore_l3_bw_get);

void sched_percore_l3_bw_set(bool enable)
{
	_percore_l3_bw = enable;
}
EXPORT_SYMBOL_GPL(sched_percore_l3_bw_set);

bool sched_dsu_pwr_enable_get(void)
{
	return _dsu_pwr_enable;
}
EXPORT_SYMBOL_GPL(sched_dsu_pwr_enable_get);

void sched_dsu_pwr_enable_set(bool enable)
{
	_dsu_pwr_enable = enable;
}
EXPORT_SYMBOL_GPL(sched_dsu_pwr_enable_set);
