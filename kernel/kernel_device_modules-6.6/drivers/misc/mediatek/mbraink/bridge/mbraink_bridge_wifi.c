// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/mutex.h>

#include "mbraink_bridge_wifi.h"

static struct mbraink2wifi_ops g_mbraink2wifi_ops;
static DEFINE_MUTEX(bridge_wifi_lock);

void mbraink_bridge_wifi_init(void)
{

	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = NULL;
	g_mbraink2wifi_ops.priv = NULL;

	mutex_unlock(&bridge_wifi_lock);
}

void mbraink_bridge_wifi_deinit(void)
{
	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = NULL;
	g_mbraink2wifi_ops.priv = NULL;

	mutex_unlock(&bridge_wifi_lock);
}

/*register callback from wifi driver*/
void register_wifi2mbraink_ops(struct mbraink2wifi_ops *ops)
{
	if (!ops)
		return;

	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = ops->get_data;
	g_mbraink2wifi_ops.priv = ops->priv;

	mutex_unlock(&bridge_wifi_lock);
}
EXPORT_SYMBOL_GPL(register_wifi2mbraink_ops);

void unregister_wifi2mbraink_ops(void)
{
	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = NULL;
	g_mbraink2wifi_ops.priv = NULL;

	mutex_unlock(&bridge_wifi_lock);
}
EXPORT_SYMBOL_GPL(unregister_wifi2mbraink_ops);

enum wifi2mbr_status
mbraink_bridge_wifi_get_data(enum mbr2wifi_reason reason,
			enum wifi2mbr_tag tag,
			void *data,
			unsigned short *real_len)
{
	enum wifi2mbr_status ret = WIFI2MBR_NO_OPS;

	mutex_lock(&bridge_wifi_lock);

	if (!g_mbraink2wifi_ops.get_data || !g_mbraink2wifi_ops.priv)
		pr_info("%s: get_data || priv is NULL\n", __func__);
	else
		ret = g_mbraink2wifi_ops.get_data(g_mbraink2wifi_ops.priv,
						reason, tag, data, real_len);

	mutex_unlock(&bridge_wifi_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mbraink_bridge_wifi_get_data);
