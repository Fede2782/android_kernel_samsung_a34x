/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPU_MISC_H__
#define __GPU_MISC_H__

/**************************************************
 * External Function
 **************************************************/
/* PTPOD for legacy chip*/
extern unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size);
extern unsigned int mt_gpufreq_not_ready(void);
extern void mt_gpufreq_enable_by_ptpod(void);
extern void mt_gpufreq_disable_by_ptpod(void);
extern void mt_gpufreq_restore_default_volt(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx);

/* Thermal for legacy chip*/
extern struct mt_gpufreq_power_table_info *mt_gpufreq_get_power_table(void);
extern unsigned int mt_gpufreq_get_seg_max_opp_index(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_get_power_table_num(void);
extern void mt_gpufreq_set_gpu_wrap_fp(int (*gpu_wrap_fp)(void));
extern void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage);
#endif /* __GPU_MISC_H__ */
