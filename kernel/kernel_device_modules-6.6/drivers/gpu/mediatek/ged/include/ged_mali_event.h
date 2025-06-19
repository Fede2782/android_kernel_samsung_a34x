/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GED_MALI_EVENT_H__
#define __GED_MALI_EVENT_H__

#define MAX_CALLBACK_NUM 3
#define MAX_RECORD_DATA 8

struct basic_error_info {
	unsigned long long ts;
	uint32_t reason;
};

struct cs_error_info {
	pid_t pid;
	unsigned long long ts;
	u8 group_handle;
	u32 csg_nr;
	s8 csi_index;
	u32 cs_fatal_type;
	u32 cs_fatal_data;
	u64 cs_fatal_info_data;
};

struct ged_mali_event_info {
	uint32_t fenceType;
	uint32_t fenceTimeoutSec;
	bool pmode_flag;
	struct cs_error_info cs_error_info_array[MAX_RECORD_DATA];
	struct basic_error_info device_lost_info_array[MAX_RECORD_DATA];
	struct basic_error_info gpu_reset_info_array[MAX_RECORD_DATA];
};

typedef void (*fence_timeout_notify_callback)(int pid, void *data, unsigned long long time);
typedef void (*gpu_reset_done_notify_callback)(unsigned long long time);

void ged_mali_event_init(void);
void ged_mali_event_update_cs_error_nolock(pid_t pid, u8 handle, u32 csg_nr, s8 csi_index, u32 cs_fatal_type,
	u32 cs_fatal_data, u64 cs_fatal_info_data);
void ged_mali_event_update_device_lost(uint32_t reason);
void ged_mali_event_update_device_lost_nolock(uint32_t reason);
void ged_mali_event_update_gpu_reset(uint32_t reason);
void ged_mali_event_update_gpu_reset_nolock(uint32_t reason);
void ged_mali_event_update_pmode_flag(bool flag);
void ged_mali_event_update_pmode_flag_nolock(bool flag);
int ged_mali_event_notify_fence_timeout_event(int pid, uint32_t fenceType, uint32_t fenceTimeoutSec);
int ged_mali_event_register_fence_timeout_callback(fence_timeout_notify_callback func_cb);
int ged_mali_event_unregister_fence_timeout_callback(fence_timeout_notify_callback func_cb);
int ged_mali_event_notify_gpu_reset_done(void);
int ged_mali_event_register_gpu_reset_done_callback(gpu_reset_done_notify_callback func_cb);
int ged_mali_event_unregister_gpu_reset_done_callback(gpu_reset_done_notify_callback func_cb);

#endif /* __GED_MALI_EVENT_H__ */
