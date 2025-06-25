/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_MBRAINK_SYS_RES__
#define __MTK_MBRAINK_SYS_RES__


#include <linux/types.h>
#include <linux/spinlock.h>

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
#include <swpm_module_psp.h>
#endif

#define MBRAINK_SYS_RES_SYS_RESOURCE_NUM (8)

enum _mbraink_sys_res_scene {
	MBRAINK_SYS_RES_SCENE_COMMON = 0,
	MBRAINK_SYS_RES_SCENE_SUSPEND,
	MBRAINK_SYS_RES_SCENE_LAST_SUSPEND_DIFF,
	MBRAINK_SYS_RES_SCENE_LAST_DIFF,
	MBRAINK_SYS_RES_SCENE_LAST_SYNC,
	MBRAINK_SYS_RES_SCENE_TEMP,
	MBRAINK_SYS_RES_SCENE_NUM,
};

enum _mbraink_sys_res_get_scene {
	MBRAINK_SYS_RES_COMMON = 0,
	MBRAINK_SYS_RES_SUSPEND,
	MBRAINK_SYS_RES_LAST_SUSPEND,
	MBRAINK_SYS_RES_LAST,
	MBRAINK_SYS_RES_GET_SCENE_NUM,
};

struct mbraink_sys_res_record {
	struct res_sig_stats *spm_res_sig_stats_ptr;
};

#define MBRAINK_SYS_RES_SYS_NAME_LEN (10)

struct mbraink_sys_res_group_info {
	unsigned int sys_index;
	unsigned int sig_table_index;
	unsigned int group_num;
	unsigned int threshold;
};

#define MBRAINK_SYS_RES_NAME_LEN (10)
struct mbriank_sys_res_mapping {
	unsigned int id;
	char name[MBRAINK_SYS_RES_NAME_LEN];
};

struct mbraink_sys_res_ops {
	struct mbraink_sys_res_record* (*get)(unsigned int scene);
	int (*update)(void);
	uint64_t (*get_detail)(struct mbraink_sys_res_record *record, int op, unsigned int val);
	unsigned int (*get_threshold)(void);
	void (*set_threshold)(unsigned int val);
	void (*enable_common_log)(int en);
	int (*get_log_enable)(void);
	void (*log)(unsigned int scene);
	spinlock_t *lock;
	int (*get_id_name)(struct mbriank_sys_res_mapping **map, unsigned int *size);
};

int mbraink_sys_res_init(void);
void mbraink_sys_res_exit(void);

int register_mbraink_sys_res_ops(struct mbraink_sys_res_ops *ops);
void unregister_mbraink_sys_res_ops(void);
struct mbraink_sys_res_ops *get_mbraink_sys_res_ops(void);

#endif
