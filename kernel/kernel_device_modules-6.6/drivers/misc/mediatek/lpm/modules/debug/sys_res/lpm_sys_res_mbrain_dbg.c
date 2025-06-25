// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <lpm_module.h>
#include <lpm_sys_res_mbrain_dbg.h>
#include <lpm_dbg_common_v2.h>

static struct lpm_sys_res_mbrain_dbg_ops _lpm_sys_res_mbrain_dbg_ops;

struct lpm_sys_res_mbrain_dbg_ops *get_lpm_mbrain_dbg_ops(void)
{
	return &_lpm_sys_res_mbrain_dbg_ops;
}
EXPORT_SYMBOL(get_lpm_mbrain_dbg_ops);

int register_lpm_mbrain_dbg_ops(struct lpm_sys_res_mbrain_dbg_ops *ops)
{
	if (!ops)
		return -1;

	_lpm_sys_res_mbrain_dbg_ops.get_length = ops->get_length;
	_lpm_sys_res_mbrain_dbg_ops.get_data = ops->get_data;
	_lpm_sys_res_mbrain_dbg_ops.get_last_suspend_res_data = ops->get_last_suspend_res_data;
	_lpm_sys_res_mbrain_dbg_ops.get_over_threshold_num = ops->get_over_threshold_num;
	_lpm_sys_res_mbrain_dbg_ops.get_over_threshold_data = ops->get_over_threshold_data;

	return 0;
}
EXPORT_SYMBOL(register_lpm_mbrain_dbg_ops);

void unregister_lpm_mbrain_dbg_ops(void)
{
	_lpm_sys_res_mbrain_dbg_ops.get_length = NULL;
	_lpm_sys_res_mbrain_dbg_ops.get_data = NULL;
	_lpm_sys_res_mbrain_dbg_ops.get_last_suspend_res_data = NULL;
	_lpm_sys_res_mbrain_dbg_ops.get_over_threshold_num = NULL;
	_lpm_sys_res_mbrain_dbg_ops.get_over_threshold_data = NULL;
}
EXPORT_SYMBOL(unregister_lpm_mbrain_dbg_ops);

void lpm_get_suspend_event_info(struct lpm_dbg_lp_info *info)
{
	unsigned int smc_id, i;

	if(!info)
		return;

	smc_id = MT_SPM_DBG_SMC_SUSPEND_PWR_STAT;

	for (i = 0; i < NUM_SPM_STAT; i++) {
		info->record[i].count = lpm_smc_spm_dbg(smc_id,
			MT_LPM_SMC_ACT_GET, i, SPM_SLP_COUNT);
		info->record[i].duration = lpm_smc_spm_dbg(smc_id,
			MT_LPM_SMC_ACT_GET, i, SPM_SLP_DURATION);
	}
}
EXPORT_SYMBOL(lpm_get_suspend_event_info);
