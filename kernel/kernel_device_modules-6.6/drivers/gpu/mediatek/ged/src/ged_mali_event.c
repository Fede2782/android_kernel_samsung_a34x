// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/ktime.h>
#include <linux/mutex.h>
#include "ged_mali_event.h"

static DEFINE_MUTEX(ged_mali_event_callback_lock);

static fence_timeout_notify_callback fence_timeout_notify_callback_list[MAX_CALLBACK_NUM];
static gpu_reset_done_notify_callback gpu_reset_done_notify_callback_list[MAX_CALLBACK_NUM];
static struct ged_mali_event_info ged_mali_event_info;
static uint32_t cs_error_info_idx;
static uint32_t device_lost_info_idx;
static uint32_t gpu_reset_info_idx;

void ged_mali_event_init(void)
{
	int i = 0;

	mutex_lock(&ged_mali_event_callback_lock);

	// initialize the value of index
	cs_error_info_idx = 0;
	device_lost_info_idx = 0;
	gpu_reset_info_idx = 0;

	// clear all data to zero
	memset(&ged_mali_event_info, 0, sizeof(struct ged_mali_event_info));

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		fence_timeout_notify_callback_list[i] = NULL;
		gpu_reset_done_notify_callback_list[i] = NULL;
	}

	mutex_unlock(&ged_mali_event_callback_lock);
}
EXPORT_SYMBOL(ged_mali_event_init);

// only use the function under interrupt context with spinlock held
void ged_mali_event_update_cs_error_nolock(pid_t pid, u8 handle, u32 csg_nr, s8 csi_index, u32 cs_fatal_type,
	u32 cs_fatal_data, u64 cs_fatal_info_data)
{
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	if (cs_error_info_idx >= MAX_RECORD_DATA)
		return;

	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].pid = pid;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].ts = timestamp;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].group_handle = handle;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].csg_nr = csg_nr;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].csi_index = csi_index;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].cs_fatal_type = cs_fatal_type;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].cs_fatal_data = cs_fatal_data;
	ged_mali_event_info.cs_error_info_array[cs_error_info_idx].cs_fatal_info_data = cs_fatal_info_data;

	cs_error_info_idx = (cs_error_info_idx + 1) % MAX_RECORD_DATA;
}
EXPORT_SYMBOL(ged_mali_event_update_cs_error_nolock);

void ged_mali_event_update_device_lost(uint32_t reason)
{
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	mutex_lock(&ged_mali_event_callback_lock);

	if (device_lost_info_idx >= MAX_RECORD_DATA) {
		mutex_unlock(&ged_mali_event_callback_lock);
		return;
	}

	ged_mali_event_info.device_lost_info_array[device_lost_info_idx].ts = timestamp;
	ged_mali_event_info.device_lost_info_array[device_lost_info_idx].reason = reason;

	device_lost_info_idx = (device_lost_info_idx + 1) % MAX_RECORD_DATA;

	mutex_unlock(&ged_mali_event_callback_lock);
}
EXPORT_SYMBOL(ged_mali_event_update_device_lost);

void ged_mali_event_update_device_lost_nolock(uint32_t reason)
{
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	ged_mali_event_info.device_lost_info_array[device_lost_info_idx].ts = timestamp;
	ged_mali_event_info.device_lost_info_array[device_lost_info_idx].reason = reason;

	device_lost_info_idx = (device_lost_info_idx + 1) % MAX_RECORD_DATA;
}
EXPORT_SYMBOL(ged_mali_event_update_device_lost_nolock);

void ged_mali_event_update_gpu_reset(uint32_t reason)
{
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	mutex_lock(&ged_mali_event_callback_lock);

	if (gpu_reset_info_idx >= MAX_RECORD_DATA) {
		mutex_unlock(&ged_mali_event_callback_lock);
		return;
	}

	ged_mali_event_info.gpu_reset_info_array[gpu_reset_info_idx].ts = timestamp;
	ged_mali_event_info.gpu_reset_info_array[gpu_reset_info_idx].reason = reason;

	gpu_reset_info_idx = (gpu_reset_info_idx + 1) % MAX_RECORD_DATA;

	mutex_unlock(&ged_mali_event_callback_lock);
}
EXPORT_SYMBOL(ged_mali_event_update_gpu_reset);

void ged_mali_event_update_gpu_reset_nolock(uint32_t reason)
{
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	ged_mali_event_info.gpu_reset_info_array[gpu_reset_info_idx].ts = timestamp;
	ged_mali_event_info.gpu_reset_info_array[gpu_reset_info_idx].reason = reason;

	gpu_reset_info_idx = (gpu_reset_info_idx + 1) % MAX_RECORD_DATA;
}
EXPORT_SYMBOL(ged_mali_event_update_gpu_reset_nolock);

void ged_mali_event_update_pmode_flag(bool flag)
{
	mutex_lock(&ged_mali_event_callback_lock);
	ged_mali_event_info.pmode_flag = flag;
	mutex_unlock(&ged_mali_event_callback_lock);
}
EXPORT_SYMBOL(ged_mali_event_update_pmode_flag);

void ged_mali_event_update_pmode_flag_nolock(bool flag)
{
	ged_mali_event_info.pmode_flag = flag;
}
EXPORT_SYMBOL(ged_mali_event_update_pmode_flag_nolock);

int ged_mali_event_notify_fence_timeout_event(int pid, uint32_t fenceType, uint32_t fenceTimeoutSec)
{
	int i = 0;
	int ret = 0;
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	mutex_lock(&ged_mali_event_callback_lock);

	ged_mali_event_info.fenceType = fenceType;
	ged_mali_event_info.fenceTimeoutSec = fenceTimeoutSec;

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		if (fence_timeout_notify_callback_list[i]) {
			fence_timeout_notify_callback_list[i](pid, (void *)(&ged_mali_event_info), timestamp);
			ret = 1;
		}
	}

	mutex_unlock(&ged_mali_event_callback_lock);
	return ret;
}
EXPORT_SYMBOL(ged_mali_event_notify_fence_timeout_event);

int ged_mali_event_register_fence_timeout_callback(fence_timeout_notify_callback func_cb)
{
	int i = 0;
	int empty_idx = -1;
	int ret = 0;

	mutex_lock(&ged_mali_event_callback_lock);

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		if (fence_timeout_notify_callback_list[i] == func_cb)
			break;
		if (fence_timeout_notify_callback_list[i] == NULL && empty_idx == -1)
			empty_idx = i;
	}
	if (i >= MAX_CALLBACK_NUM) {
		if (empty_idx < 0 || empty_idx >= MAX_CALLBACK_NUM)
			ret = -ENOMEM;
		else
			fence_timeout_notify_callback_list[empty_idx] = func_cb;
	}

	mutex_unlock(&ged_mali_event_callback_lock);
	return ret;
}
EXPORT_SYMBOL(ged_mali_event_register_fence_timeout_callback);

int ged_mali_event_unregister_fence_timeout_callback(fence_timeout_notify_callback func_cb)
{
	int i = 0;
	int ret = -ESPIPE;

	mutex_lock(&ged_mali_event_callback_lock);

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		if (fence_timeout_notify_callback_list[i] == func_cb) {
			fence_timeout_notify_callback_list[i] = NULL;
			ret = 0;
			break;
		}
	}

	mutex_unlock(&ged_mali_event_callback_lock);
	return ret;
}
EXPORT_SYMBOL(ged_mali_event_unregister_fence_timeout_callback);

int ged_mali_event_notify_gpu_reset_done(void)
{
	int i = 0;
	int ret = 0;
	unsigned long long timestamp = 0;
	struct timespec64 tv = { 0 };

	ktime_get_real_ts64(&tv);

	timestamp = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);

	mutex_lock(&ged_mali_event_callback_lock);

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		if (gpu_reset_done_notify_callback_list[i]) {
			gpu_reset_done_notify_callback_list[i](timestamp);
			ret = 1;
		}
	}

	mutex_unlock(&ged_mali_event_callback_lock);
	return ret;
}
EXPORT_SYMBOL(ged_mali_event_notify_gpu_reset_done);

int ged_mali_event_register_gpu_reset_done_callback(gpu_reset_done_notify_callback func_cb)
{
	int i = 0;
	int empty_idx = -1;
	int ret = 0;

	mutex_lock(&ged_mali_event_callback_lock);

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		if (gpu_reset_done_notify_callback_list[i] == func_cb)
			break;
		if (gpu_reset_done_notify_callback_list[i] == NULL && empty_idx == -1)
			empty_idx = i;
	}
	if (i >= MAX_CALLBACK_NUM) {
		if (empty_idx < 0 || empty_idx >= MAX_CALLBACK_NUM)
			ret = -ENOMEM;
		else
			gpu_reset_done_notify_callback_list[empty_idx] = func_cb;
	}

	mutex_unlock(&ged_mali_event_callback_lock);
	return ret;
}
EXPORT_SYMBOL(ged_mali_event_register_gpu_reset_done_callback);

int ged_mali_event_unregister_gpu_reset_done_callback(gpu_reset_done_notify_callback func_cb)
{
	int i = 0;
	int ret = -ESPIPE;

	mutex_lock(&ged_mali_event_callback_lock);

	for (i = 0; i < MAX_CALLBACK_NUM; i++) {
		if (gpu_reset_done_notify_callback_list[i] == func_cb) {
			gpu_reset_done_notify_callback_list[i] = NULL;
			ret = 0;
			break;
		}
	}

	mutex_unlock(&ged_mali_event_callback_lock);
	return ret;
}
EXPORT_SYMBOL(ged_mali_event_unregister_gpu_reset_done_callback);
