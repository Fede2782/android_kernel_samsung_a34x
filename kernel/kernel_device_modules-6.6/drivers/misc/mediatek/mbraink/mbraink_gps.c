// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include "mbraink_modules_ops_def.h"

static struct mbraink_gps_ops _mbraink_gps_ops;

int mbraink_gps_init(void)
{
	_mbraink_gps_ops.get_gnss_lp_data = NULL;
	_mbraink_gps_ops.get_gnss_mcu_data = NULL;
	return 0;
}

int mbraink_gps_deinit(void)
{
	_mbraink_gps_ops.get_gnss_lp_data = NULL;
	_mbraink_gps_ops.get_gnss_mcu_data = NULL;
	return 0;
}

int register_mbraink_gps_ops(struct mbraink_gps_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_gps_ops.get_gnss_lp_data = ops->get_gnss_lp_data;
	_mbraink_gps_ops.get_gnss_mcu_data = ops->get_gnss_mcu_data;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_gps_ops);

int unregister_mbraink_gps_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_gps_ops.get_gnss_lp_data = NULL;
	_mbraink_gps_ops.get_gnss_mcu_data = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_gps_ops);

void mbraink_get_gnss_lp_data(struct mbraink_gnss2mbr_lp_data *gnss_lp_buffer)
{
	if (_mbraink_gps_ops.get_gnss_lp_data)
		_mbraink_gps_ops.get_gnss_lp_data(gnss_lp_buffer);
	else
		pr_info("%s: Do not support ioctl get_gnss_lp_data.\n", __func__);

}

void mbraink_get_gnss_mcu_data(struct mbraink_gnss2mbr_mcu_data *gnss_mcu_buffer)
{

	if (_mbraink_gps_ops.get_gnss_mcu_data)
		_mbraink_gps_ops.get_gnss_mcu_data(gnss_mcu_buffer);
	else
		pr_info("%s: Do not support ioctl get_gnss_mcu_data.\n", __func__);
}

