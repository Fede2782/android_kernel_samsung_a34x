/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_BATTERY_H
#define MBRAINK_BATTERY_H
#include <mbraink_ioctl_struct_def.h>

int mbraink_battery_init(void);
int mbraink_battery_deinit(void);
void mbraink_get_battery_info(struct mbraink_battery_data *battery_buffer,
			      long long timestamp);

#endif
