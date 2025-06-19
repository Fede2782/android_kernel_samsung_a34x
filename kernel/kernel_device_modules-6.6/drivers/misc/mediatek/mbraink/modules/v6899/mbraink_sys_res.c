// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/fs.h>
#include "mbraink_sys_res.h"

static struct mbraink_sys_res_ops _mbriank_sys_res_ops;

int mbraink_sys_res_init(void)
{
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
EXPORT_SYMBOL(mbraink_sys_res_init);

void mbraink_sys_res_exit(void)
{
	pr_info("%s %d: finish", __func__, __LINE__);
}
EXPORT_SYMBOL(mbraink_sys_res_exit);

struct mbraink_sys_res_ops *get_mbraink_sys_res_ops(void)
{
	return &_mbriank_sys_res_ops;
}
EXPORT_SYMBOL(get_mbraink_sys_res_ops);

int register_mbraink_sys_res_ops(struct mbraink_sys_res_ops *ops)
{
	if (!ops)
		return -1;

	_mbriank_sys_res_ops.get = ops->get;
	_mbriank_sys_res_ops.update = ops->update;
	_mbriank_sys_res_ops.get_detail = ops->get_detail;
	_mbriank_sys_res_ops.get_threshold = ops->get_threshold;
	_mbriank_sys_res_ops.set_threshold = ops->set_threshold;
	_mbriank_sys_res_ops.enable_common_log = ops->enable_common_log;
	_mbriank_sys_res_ops.get_log_enable = ops->get_log_enable;
	_mbriank_sys_res_ops.log = ops->log;
	_mbriank_sys_res_ops.lock = ops->lock;
	_mbriank_sys_res_ops.get_id_name = ops->get_id_name;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_sys_res_ops);

void unregister_mbraink_sys_res_ops(void)
{
	_mbriank_sys_res_ops.get = NULL;
	_mbriank_sys_res_ops.update = NULL;
	_mbriank_sys_res_ops.get_detail = NULL;
	_mbriank_sys_res_ops.get_threshold = NULL;
	_mbriank_sys_res_ops.set_threshold = NULL;
	_mbriank_sys_res_ops.enable_common_log = NULL;
	_mbriank_sys_res_ops.get_log_enable = NULL;
	_mbriank_sys_res_ops.log = NULL;
	_mbriank_sys_res_ops.lock = NULL;
	_mbriank_sys_res_ops.get_id_name = NULL;
}
EXPORT_SYMBOL(unregister_mbraink_sys_res_ops);
