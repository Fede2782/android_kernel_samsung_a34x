// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include "mbraink_modules_ops_def.h"

static struct mbraink_wifi_ops _mbraink_wifi_ops;

int mbraink_wifi_init(void)
{
	_mbraink_wifi_ops.get_wifi_rate_data = NULL;
	_mbraink_wifi_ops.get_wifi_radio_data = NULL;
	_mbraink_wifi_ops.get_wifi_ac_data = NULL;
	_mbraink_wifi_ops.get_wifi_lp_data = NULL;
	_mbraink_wifi_ops.get_wifi_txtimeout_data = NULL;
	return 0;
}

int mbraink_wifi_deinit(void)
{
	_mbraink_wifi_ops.get_wifi_rate_data = NULL;
	_mbraink_wifi_ops.get_wifi_radio_data = NULL;
	_mbraink_wifi_ops.get_wifi_ac_data = NULL;
	_mbraink_wifi_ops.get_wifi_lp_data = NULL;
	_mbraink_wifi_ops.get_wifi_txtimeout_data = NULL;
	return 0;
}

int register_mbraink_wifi_ops(struct mbraink_wifi_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_wifi_ops.get_wifi_rate_data = ops->get_wifi_rate_data;
	_mbraink_wifi_ops.get_wifi_radio_data = ops->get_wifi_radio_data;
	_mbraink_wifi_ops.get_wifi_ac_data = ops->get_wifi_ac_data;
	_mbraink_wifi_ops.get_wifi_lp_data = ops->get_wifi_lp_data;
	_mbraink_wifi_ops.get_wifi_txtimeout_data = ops->get_wifi_txtimeout_data;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_wifi_ops);

int unregister_mbraink_wifi_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_wifi_ops.get_wifi_rate_data = NULL;
	_mbraink_wifi_ops.get_wifi_radio_data = NULL;
	_mbraink_wifi_ops.get_wifi_ac_data = NULL;
	_mbraink_wifi_ops.get_wifi_lp_data = NULL;
	_mbraink_wifi_ops.get_wifi_txtimeout_data = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_wifi_ops);

void mbraink_get_wifi_rate_data(int current_idx,
				struct mbraink_wifi2mbr_lls_rate_data *rate_buffer)
{

	if (_mbraink_wifi_ops.get_wifi_rate_data)
		_mbraink_wifi_ops.get_wifi_rate_data(current_idx, rate_buffer);
	else
		pr_info("%s: Do not support ioctl get_wifi_rate_data.\n", __func__);
}

void mbraink_get_wifi_radio_data(struct mbraink_wifi2mbr_lls_radio_data *radio_buffer)
{

	if (_mbraink_wifi_ops.get_wifi_radio_data)
		_mbraink_wifi_ops.get_wifi_radio_data(radio_buffer);
	else
		pr_info("%s: Do not support ioctl get_wifi_radio_data.\n", __func__);
}

void mbraink_get_wifi_ac_data(struct mbraink_wifi2mbr_lls_ac_data *ac_buffer)
{

	if (_mbraink_wifi_ops.get_wifi_ac_data)
		_mbraink_wifi_ops.get_wifi_ac_data(ac_buffer);
	else
		pr_info("%s: Do not support ioctl get_wifi_ac_data.\n", __func__);
}

void mbraink_get_wifi_lp_data(struct mbraink_wifi2mbr_lp_ratio_data *lp_buffer)
{

	if (_mbraink_wifi_ops.get_wifi_lp_data)
		_mbraink_wifi_ops.get_wifi_lp_data(lp_buffer);
	else
		pr_info("%s: Do not support ioctl get_wifi_lp_data.\n", __func__);
}

void mbraink_get_wifi_txtimeout_data(int current_idx,
				struct mbraink_wifi2mbr_txtimeout_data *txtimeout_buffer)
{

	if (_mbraink_wifi_ops.get_wifi_txtimeout_data)
		_mbraink_wifi_ops.get_wifi_txtimeout_data(current_idx, txtimeout_buffer);
	else
		pr_info("%s: Do not support ioctl get_wifi_txtimeout_data.\n", __func__);
}
