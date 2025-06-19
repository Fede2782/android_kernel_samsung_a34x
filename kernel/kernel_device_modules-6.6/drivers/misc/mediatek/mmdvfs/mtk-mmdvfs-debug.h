/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef MTK_MMDVFS_DEBUG_H
#define MTK_MMDVFS_DEBUG_H

#define MAX_OPP		(8)
#define MMDVFS_RES_DATA_MODULE_ID	8
#define MMDVFS_RES_DATA_VERSION		0

struct mmdvfs_res_mbrain_header {
	uint8_t mbrain_module;
	uint8_t version;
	uint16_t data_offset;
	uint32_t index_data_length;
};

struct mmdvfs_opp_record {
	uint64_t opp_duration[MAX_OPP];
};

enum {
	MMDVFS_POWER_0,
	MMDVFS_POWER_1,
	MMDVFS_POWER_2,
	MMDVFS_ALONE_CAM,
	MMDVFS_OPP_RECORD_NUM
};

struct mmdvfs_res_mbrain_debug_ops {
	unsigned int (*get_length)(void);
	int (*get_data)(void *address, uint32_t size);
};

struct mmdvfs_res_mbrain_debug_ops *get_mmdvfs_mbrain_dbg_ops(void);
void mtk_mmdvfs_debug_release_step0(void);
void mtk_mmdvfs_debug_ulposc_enable(const bool enable);
int mtk_mmdvfs_debug_force_vcore_notify(const u32 val);
bool mtk_is_mmdvfs_v3_debug_init_done(void);
void mmdvfs_debug_status_dump(struct seq_file *file);

inline void mmdvfs_vcp_cb_mutex_lock(void);
inline void mmdvfs_vcp_cb_mutex_unlock(void);
inline bool mmdvfs_vcp_cb_ready_get(void);
#endif

