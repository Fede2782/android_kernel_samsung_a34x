// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <usb_xhci/quirks.h>

#include "mbraink_modules_ops_def.h"
#include "mbraink_usb.h"

void usb_xhci_enum_info(struct xhci_mbrain data)
{
	unsigned int vid = data.vid;
	unsigned int pid = data.pid;
	unsigned int bcd = data.bcd;
	int state = (int)data.state;
	int speed = (int)data.speed;
	unsigned int error_reason = data.err_reason;

	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d:%d:%d:%d:%d:%d",
			NETLINK_EVENT_USB_ENUM,
			timestamp,
			vid,
			pid,
			bcd,
			state,
			speed,
			error_reason);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

int mbraink_usb_init(void)
{
	int ret = 0;

	ret = register_xhci_enum_mbrain_cb(usb_xhci_enum_info);
	return ret;
}

int mbraink_usb_deinit(void)
{
	int ret = 0;

	ret = unregister_xhci_enum_mbrain_cb();
	return ret;
}
