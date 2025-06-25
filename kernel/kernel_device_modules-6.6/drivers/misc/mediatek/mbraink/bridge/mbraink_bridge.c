// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include "mbraink_bridge.h"

static int mbraink_bridge_init(void)
{
	mbraink_bridge_gps_init();
	mbraink_bridge_wifi_init();
	mbraink_bridge_camera_init();

	return 0;
}

static void mbraink_bridge_exit(void)
{
	mbraink_bridge_wifi_deinit();
	mbraink_bridge_gps_deinit();
	mbraink_bridge_camera_deinit();
}

module_init(mbraink_bridge_init);
module_exit(mbraink_bridge_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK Bridge Linux Device Driver");
MODULE_VERSION("1.0");
