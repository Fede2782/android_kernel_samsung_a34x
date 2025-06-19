/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/**
 * @file vl53lx.h header for vl53lx sensor driver
 */
#ifndef VL53LX_H
#define VL53LX_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/time.h>

#include <net/sock.h>
#include <linux/netlink.h>

#include "vl53lx_api.h"

#define VL53LX_MAX_CCI_XFER_SZ	256
#define VL53LX_DRV_NAME	"vl53lx"

#define vl53lx_info(str, args...) \
	pr_info("%s: " str "\n", __func__, ##args)

#define vl53lx_errmsg(str, args...) \
	pr_info("%s: " str "\n", __func__, ##args)

#define vl53lx_wanrmsg(str, args...) \
	pr_info("%s: " str "\n", __func__, ##args)

/* turn off poll log if not defined */
#ifndef VL53LX_LOG_POLL_TIMING
#	define VL53LX_LOG_POLL_TIMING	0
#endif
/* turn off cci log timing if not defined */
#ifndef VL53LX_LOG_CCI_TIMING
#	define VL53LX_LOG_CCI_TIMING	0
#endif


#endif /* VL53LX_H */
