/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MVPU20_HANDLER_H__
#define __MVPU20_HANDLER_H__

#include <linux/mutex.h>
#include <linux/kernel.h>
#include "apusys_device.h"

extern struct mutex mvpu20_pool_lock;

void mvpu20_handler_lite_init(void);
int mvpu20_handler_lite(int type, void *hnd, struct apusys_device *dev);

#endif /* __MVPU20_HANDLER_H__ */
