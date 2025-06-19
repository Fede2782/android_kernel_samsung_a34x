/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6899_MEMORY_H
#define MBRAINK_V6899_MEMORY_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
#include "ufs-mediatek-mbrain.h"
#endif

extern int mbraink_netlink_send_msg(const char *msg);


int mbraink_v6899_memory_init(struct device *dev);
int mbraink_v6899_memory_deinit(struct device *dev);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
int ufs2mbrain_event_notify(struct ufs_mbrain_event *event);
#endif

#endif /*end of MBRAINK_V6899_MEMORY_H*/

