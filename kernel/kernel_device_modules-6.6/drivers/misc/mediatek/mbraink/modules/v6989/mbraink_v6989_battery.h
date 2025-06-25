/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_V6989_BATTERY_H
#define MBRAINK_V6989_BATTERY_H
#include <linux/power_supply.h>
#include <mbraink_ioctl_struct_def.h>

struct battery_drv_data {
	struct power_supply *bat1_psy;
};

int mbraink_v6989_battery_init(struct device *dev);
int mbraink_v6989_battery_deinit(void);

extern int power_supply_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val);

#endif //MBRAINK_V6989_BATTERY_H
