/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FAN_COOLING_H
#define _FAN_COOLING_H

#include <linux/thermal.h>

/*===========================================================
 *  Macro Definitions
 *===========================================================
 */
#define FAN_COOLING_UNLIMITED_STATE	(0)
#define FAN_STATE_NUM 4
#define MAX_FAN_COOLER_NAME_LEN		(20)

/**
 * struct fan_cooling_device - data for fan cooling device
 * @name: naming string for this cooling device
 * @target_state: target cooling state which is set in set_cur_state()
 *	callback.
 * @max_state: maximum state supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle: callback function to handle throttle request
 * @dev: device node pointer
 */
struct fan_cooling_device {
	char name[MAX_FAN_COOLER_NAME_LEN];
	unsigned long target_state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	int (*throttle)(struct fan_cooling_device *fan_cdev, unsigned long state);
	struct device *dev;
};

#endif
