/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_CAMERA_H
#define MBRAINK_BRIDGE_CAMERA_H

struct ht_mbrain {
	int req_fd;
	int req_no;
	int frm_no;
	u32 hw_comb;
	s32 group_id;
	u64 tsHwTime;
};

struct bridge2platform_ops {
	int (*set_data)(struct ht_mbrain);
};

void mbraink_bridge_camera_init(void);
void mbraink_bridge_camera_deinit(void);
int  register_mbraink_bridge_platform_camera_ops(struct bridge2platform_ops *ops);
int  unregister_mbraink_bridge_platform_camera_ops(void);
void imgsys2mbrain_notify_hw_time_info(struct ht_mbrain ht_mbrain);
#endif /*MBRAINK_BRIDGE_CAMERA_H*/
