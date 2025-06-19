// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include "mbraink_sys_res_mbrain_dbg.h"

static struct mbraink_sys_res_mbrain_dbg_ops _mbraink_sys_res_mbrain_dbg_ops;

struct mbraink_sys_res_mbrain_dbg_ops *get_mbraink_dbg_ops(void)
{
	return &_mbraink_sys_res_mbrain_dbg_ops;
}
EXPORT_SYMBOL(get_mbraink_dbg_ops);

int register_mbraink_dbg_ops(struct mbraink_sys_res_mbrain_dbg_ops *ops)
{
	if (!ops)
		return -1;

	_mbraink_sys_res_mbrain_dbg_ops.get_length = ops->get_length;
	_mbraink_sys_res_mbrain_dbg_ops.get_data = ops->get_data;
	_mbraink_sys_res_mbrain_dbg_ops.get_last_suspend_res_data = ops->get_last_suspend_res_data;
	_mbraink_sys_res_mbrain_dbg_ops.get_over_threshold_num = ops->get_over_threshold_num;
	_mbraink_sys_res_mbrain_dbg_ops.get_over_threshold_data = ops->get_over_threshold_data;
	_mbraink_sys_res_mbrain_dbg_ops.update= ops->update;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_dbg_ops);

void unregister_mbraink_dbg_ops(void)
{
	_mbraink_sys_res_mbrain_dbg_ops.get_length = NULL;
	_mbraink_sys_res_mbrain_dbg_ops.get_data = NULL;
	_mbraink_sys_res_mbrain_dbg_ops.get_last_suspend_res_data = NULL;
	_mbraink_sys_res_mbrain_dbg_ops.get_over_threshold_num = NULL;
	_mbraink_sys_res_mbrain_dbg_ops.get_over_threshold_data = NULL;
	_mbraink_sys_res_mbrain_dbg_ops.update= NULL;
}
EXPORT_SYMBOL(unregister_mbraink_dbg_ops);
