/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_MBRAINK_SYS_RES_MBRAIN_DBG__
#define __MTK_MBRAINK_SYS_RES_MBRAIN_DBG__

#include <linux/types.h>

enum _mbraink_release_scene {
	MBRAINK_SYS_RES_RELEASE_SCENE_COMMON = 0,
	MBRAINK_SYS_RES_RELEASE_SCENE_SUSPEND,
	MBRAINK_SYS_RES_RELEASE_SCENE_LAST_SUSPEND,
	MBRAINK_SCENE_RELEASE_NUM,
};

enum _mbraink_release_bool {
	MBRAINK_NOT_RELEASE = 0,
	MBRAINK_RELEASE_GROUP,
};

struct mbraink_sys_res_mbrain_header {
	uint8_t module;
	uint8_t version;
	uint16_t data_offset;
	uint32_t index_data_length;
};

struct mbraink_sys_res_scene_info {
	uint64_t duration_time;
	uint64_t suspend_time;
	uint32_t res_sig_num;
};

struct mbraink_sys_res_sig_info {
	uint64_t active_time;
	uint32_t sig_id;
	uint32_t grp_id;
};

struct mbraink_sys_res_mbrain_dbg_ops {
	unsigned int (*get_length)(void);
	int (*get_data)(void *address, uint32_t size);
	int (*get_last_suspend_res_data)(void *address, uint32_t size);
	int (*get_over_threshold_num)(void *addr, uint32_t size,
		uint32_t *thr, uint32_t thr_num);
	int (*get_over_threshold_data)(void *addr, uint32_t size);
	int (*update)(void);
};

struct mbraink_sys_res_mbrain_dbg_ops *get_mbraink_dbg_ops(void);
int register_mbraink_dbg_ops(struct mbraink_sys_res_mbrain_dbg_ops *ops);
void unregister_mbraink_dbg_ops(void);

#endif
