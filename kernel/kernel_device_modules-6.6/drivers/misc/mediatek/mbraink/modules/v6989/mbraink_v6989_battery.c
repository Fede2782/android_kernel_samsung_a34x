// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/power_supply.h>
#include <mbraink_modules_ops_def.h>
#include "mbraink_v6989_battery.h"

struct battery_drv_data drv_data;

static void mbraink_v6989_get_battery_info(struct mbraink_battery_data *battery_buffer,
			      long long timestamp)
{
	union power_supply_propval prop;

	memset(&prop, 0x00, sizeof(prop));
	if (drv_data.bat1_psy != NULL && !IS_ERR(drv_data.bat1_psy)) {
		battery_buffer->timestamp = timestamp;

		power_supply_get_property(drv_data.bat1_psy,
			POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, &prop);
		battery_buffer->qmaxt = prop.intval;

		power_supply_get_property(drv_data.bat1_psy,
			POWER_SUPPLY_PROP_ENERGY_FULL, &prop);
		battery_buffer->quse = prop.intval;

		power_supply_get_property(drv_data.bat1_psy,
			POWER_SUPPLY_PROP_ENERGY_NOW, &prop);
		battery_buffer->precise_soc = prop.intval;

		power_supply_get_property(drv_data.bat1_psy,
			POWER_SUPPLY_PROP_CAPACITY_LEVEL, &prop);
		battery_buffer->precise_uisoc = prop.intval;

		/**************************************************************************
		 *	pr_info("%s: timestamp=%lld qmaxt=%d, qusec=%d, socc=%d, uisocc=%d\n",
		 *	__func__,
		 *	battery_buffer->timestamp,
		 *	battery_buffer->qmaxt,
		 *	battery_buffer->quse,
		 *	battery_buffer->precise_soc,
		 *	battery_buffer->precise_uisoc);
		 **************************************************************************/
	}
}

static struct mbraink_battery_ops mbraink_v6989_battery_ops = {
	.getBatteryInfo = mbraink_v6989_get_battery_info,
};

int mbraink_v6989_battery_init(struct device *dev)
{
	int ret = 0;

	if (drv_data.bat1_psy == NULL) {
		pr_info("%s get phandle from bat1_psy\n", __func__);
		drv_data.bat1_psy = devm_power_supply_get_by_phandle(dev, "gauge");
		if (drv_data.bat1_psy == NULL || IS_ERR(drv_data.bat1_psy))
			pr_info("%s Couldn't get bat1_psy\n", __func__);
	}

	ret = register_mbraink_battery_ops(&mbraink_v6989_battery_ops);
	return ret;
}

int mbraink_v6989_battery_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_battery_ops();
	return ret;
}

