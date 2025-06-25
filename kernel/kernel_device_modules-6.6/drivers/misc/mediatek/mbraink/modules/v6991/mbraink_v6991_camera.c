// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include <mbraink_ioctl_struct_def.h>
#include <mbraink_modules_ops_def.h>
#include <bridge/mbraink_bridge_camera.h>
#include "mbraink_v6991_camera.h"

static int mbraink_v6991_set_camera_hw_info(struct ht_mbrain ht_data_to_mbrain)
{
	int req_fd = ht_data_to_mbrain.req_fd;
	int req_no = ht_data_to_mbrain.req_no;
	int frm_no = ht_data_to_mbrain.frm_no;
	unsigned int hw_comb = ht_data_to_mbrain.hw_comb;
	int group_id = ht_data_to_mbrain.group_id;
	long long tsHwTime = ht_data_to_mbrain.tsHwTime;

	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d:%d:%d:%d:%d:%llu",
			NETLINK_EVENT_IMGSYS_NOTIFY,
			timestamp,
			req_fd,
			req_no,
			frm_no,
			hw_comb,
			group_id,
			tsHwTime);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return -1;
	mbraink_netlink_send_msg(netlink_buf);

	return 0;
}

static struct bridge2platform_ops mbraink_bridge_v6991_camera_ops = {
	.set_data = mbraink_v6991_set_camera_hw_info,
};

int mbraink_v6991_camera_init(void)
{
	int ret = 0;

	ret = register_mbraink_bridge_platform_camera_ops(&mbraink_bridge_v6991_camera_ops);
	if (ret != 0) {
		pr_info("register platform callback to bridge failed by: %d", ret);
		return ret;
	}
	return ret;
}

int mbraink_v6991_camera_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_bridge_platform_camera_ops();

	return ret;
}

