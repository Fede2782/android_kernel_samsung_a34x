/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#ifndef _MTK_CG_PEAK_POWER_THROTTLING__CORE_H_
#define _MTK_CG_PEAK_POWER_THROTTLING__CORE_H_

extern void cgppt_set_mo_multiscene(int value);

extern int cgppt_get_cpu_combo_usage_count(int idx);
extern int cgppt_get_gpu_combo_usage_count(int idx);
extern int cgppt_get_cpu_m_scaling_factor(void);
extern int cgppt_get_cpu_b_scaling_factor(void);
extern int cgppt_get_gpu_scaling_factor(void);
extern int cgppt_get_combo_idx(void);
extern int cgppt_get_cg_budget(void);

#endif
