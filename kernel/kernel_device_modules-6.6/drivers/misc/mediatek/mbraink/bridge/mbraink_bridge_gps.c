// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/mutex.h>

#include "mbraink_bridge_gps.h"

static struct mbraink2gps_ops g_mbraink2gps_ops;
static DEFINE_MUTEX(bridge_gps_lock);

void mbraink_bridge_gps_init(void)
{

	mutex_lock(&bridge_gps_lock);

	g_mbraink2gps_ops.get_lp_data = NULL;
	g_mbraink2gps_ops.get_mcu_data = NULL;

	mutex_unlock(&bridge_gps_lock);
}

void mbraink_bridge_gps_deinit(void)
{
	mutex_lock(&bridge_gps_lock);

	g_mbraink2gps_ops.get_lp_data = NULL;
	g_mbraink2gps_ops.get_mcu_data = NULL;

	mutex_unlock(&bridge_gps_lock);
}

/*register callback from gps driver*/
void register_gps2mbraink_ops(struct mbraink2gps_ops *ops)
{
	if (!ops)
		return;

	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_gps_lock);

	g_mbraink2gps_ops.get_lp_data = ops->get_lp_data;
	g_mbraink2gps_ops.get_mcu_data = ops->get_mcu_data;

	mutex_unlock(&bridge_gps_lock);
}
EXPORT_SYMBOL_GPL(register_gps2mbraink_ops);

void unregister_gps2mbraink_ops(void)
{
	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_gps_lock);

	g_mbraink2gps_ops.get_lp_data = NULL;
	g_mbraink2gps_ops.get_mcu_data = NULL;

	mutex_unlock(&bridge_gps_lock);
}
EXPORT_SYMBOL_GPL(unregister_gps2mbraink_ops);

enum gnss2mbr_status mbraink_bridge_gps_get_lp_data(enum mbr2gnss_reason reason,
						struct gnss2mbr_lp_data *lp_data)
{
	enum gnss2mbr_status ret = GNSS2MBR_NO_DATA;

	mutex_lock(&bridge_gps_lock);

	if (!g_mbraink2gps_ops.get_lp_data)
		pr_info("%s: get_lp_data is NULL\n", __func__);
	else
		ret = g_mbraink2gps_ops.get_lp_data(reason, lp_data);

	mutex_unlock(&bridge_gps_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mbraink_bridge_gps_get_lp_data);

enum gnss2mbr_status mbraink_bridge_gps_get_mcu_data(enum mbr2gnss_reason reason,
						struct gnss2mbr_mcu_data *mcu_data)
{
	enum gnss2mbr_status ret = GNSS2MBR_NO_DATA;

	mutex_lock(&bridge_gps_lock);

	if (!g_mbraink2gps_ops.get_mcu_data)
		pr_info("%s: get_mcu_data is NULL\n", __func__);
	else
		ret = g_mbraink2gps_ops.get_mcu_data(reason, mcu_data);

	mutex_unlock(&bridge_gps_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mbraink_bridge_gps_get_mcu_data);
