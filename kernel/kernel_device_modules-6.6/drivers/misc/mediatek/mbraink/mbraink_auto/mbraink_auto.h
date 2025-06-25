/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_AUTO_H
#define MBRAINK_AUTO_H

long mbraink_auto_ioctl(unsigned long arg, void *mbraink_data);
int mbraink_auto_init(void);
void mbraink_auto_deinit(void);

#endif

