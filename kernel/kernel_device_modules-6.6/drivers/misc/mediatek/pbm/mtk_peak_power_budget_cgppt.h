/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_PEAK_POWER_CGPPT_H__
#define __MTK_PEAK_POWER_CGPPT_H__

struct ppb_cgppt_dbg_operation {
	int (*get_cpub_sf)(void);
	int (*get_cpum_sf)(void);
	int (*get_gpu_sf)(void);
	int (*get_cg_pwr)(void);
	int (*get_combo)(void);
	int (*get_cpucb_cnt)(int idx);
	int (*get_gpucb_cnt)(int idx);
	int (*get_cg_bgt)(void);
};

extern int register_ppb_cgppt_cb(struct ppb_cgppt_dbg_operation *ops);

#endif /* __MTK_PEAK_POWER_CGPPT_H__ */
