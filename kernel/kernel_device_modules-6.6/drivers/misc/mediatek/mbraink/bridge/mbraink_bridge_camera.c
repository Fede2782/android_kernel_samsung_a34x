// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/mutex.h>

#include "mbraink_bridge_camera.h"

static struct bridge2platform_ops g_bridge2platform_ops;
static DEFINE_MUTEX(bridge_camera_lock);

void mbraink_bridge_camera_init(void)
{

	mutex_lock(&bridge_camera_lock);

	g_bridge2platform_ops.set_data = NULL;

	mutex_unlock(&bridge_camera_lock);
}

void mbraink_bridge_camera_deinit(void)
{
	mutex_lock(&bridge_camera_lock);

	g_bridge2platform_ops.set_data = NULL;

	mutex_unlock(&bridge_camera_lock);
}

/* register callback from mbraink platform driver */
int register_mbraink_bridge_platform_camera_ops(struct bridge2platform_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_camera_lock);

	g_bridge2platform_ops.set_data = ops->set_data;

	mutex_unlock(&bridge_camera_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(register_mbraink_bridge_platform_camera_ops);

int unregister_mbraink_bridge_platform_camera_ops(void)
{
	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_camera_lock);

	g_bridge2platform_ops.set_data = NULL;

	mutex_unlock(&bridge_camera_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(unregister_mbraink_bridge_platform_camera_ops);

void imgsys2mbrain_notify_hw_time_info(struct ht_mbrain ht_mbrain)
{
	mutex_lock(&bridge_camera_lock);

	if (!g_bridge2platform_ops.set_data)
		pr_info("%s: set_data is NULL\n", __func__);
	else
		g_bridge2platform_ops.set_data(ht_mbrain);

	mutex_unlock(&bridge_camera_lock);
}
EXPORT_SYMBOL_GPL(imgsys2mbrain_notify_hw_time_info);
