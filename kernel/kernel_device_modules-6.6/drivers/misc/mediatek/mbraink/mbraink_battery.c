// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/io.h>
#include <mbraink_modules_ops_def.h>

#include "mbraink_battery.h"

static struct mbraink_battery_ops _mbraink_battery_ops;

int mbraink_battery_init(void)
{
	_mbraink_battery_ops.getBatteryInfo = NULL;
	return 0;
}

int mbraink_battery_deinit(void)
{
	_mbraink_battery_ops.getBatteryInfo = NULL;
	return 0;
}

int register_mbraink_battery_ops(struct mbraink_battery_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_battery_ops.getBatteryInfo = ops->getBatteryInfo;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_battery_ops);

int unregister_mbraink_battery_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_battery_ops.getBatteryInfo = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_battery_ops);

void mbraink_get_battery_info(struct mbraink_battery_data *battery_buffer,
			      long long timestamp)
{
	if (battery_buffer == NULL) {
		pr_info("%s: battery Info is null.\n", __func__);
		return;
	}

	if (_mbraink_battery_ops.getBatteryInfo)
		_mbraink_battery_ops.getBatteryInfo(battery_buffer, timestamp);
	else
		pr_info("%s: Do not support ioctl battery info query.\n", __func__);
}

