/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_USB_H
#define MBRAINK_USB_H

#include "mbraink_ioctl_struct_def.h"

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_usb_init(void);
int mbraink_usb_deinit(void);

#endif /*end of MBRAINK_USB_H*/
