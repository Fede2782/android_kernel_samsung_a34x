/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MBRAINK_HYPERVISOR_VIRTIO_H
#define MBRAINK_HYPERVISOR_VIRTIO_H

#include "mbraink_auto_ioctl_struct_def.h"

#define MAX_VIRTIO_SEND_BYTE 1024

#define H2C_CMD_StaticInfo              '0'
#define H2C_CMD_ClientTraceCatch        '1'

int vhost_mbraink_init(void);
void vhost_mbraink_deinit(void);

int h2c_send_msg(u32 cmdType, void *cmdData);

extern int mbraink_netlink_send_msg(const char *msg);

#endif /*end of MBRAINK_HYPERVISOR_VIRTIO_H*/
