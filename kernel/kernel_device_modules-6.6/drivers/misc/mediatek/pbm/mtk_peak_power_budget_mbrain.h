/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_PEAK_POWER_MBRAIN_H__
#define __MTK_PEAK_POWER_MBRAIN_H__

struct ppb_mbrain_data {
	unsigned long kernel_time;
	unsigned long duration;
	int soc;
	int temp;
	int soc_rdc;
	int soc_rac;
	int hpt_bat_budget;
	int hpt_cg_budget;
	int ppb_cg_budget;
	int hpt_cpub_thr_cnt;
	int hpt_cpub_thr_time;
	int hpt_cpum_thr_cnt;
	int hpt_cpum_thr_time;
	int hpt_gpu_thr_cnt;
	int hpt_gpu_thr_time;
	int hpt_cpub_sf;
	int hpt_cpum_sf;
	int hpt_gpu_sf;
	int ppb_combo;
	int ppb_c_combo0;
	int ppb_g_combo0;
	int ppb_g_flavor;
};

typedef void (*ppb_mbrain_func)(void);

extern int register_ppb_mbrian_cb(ppb_mbrain_func func_p);
extern int unregister_ppb_mbrian_cb(void);
extern int get_ppb_mbrain_data(struct ppb_mbrain_data *data);

#endif /* __MTK_PEAK_POWER_MBRAIN_H__ */
