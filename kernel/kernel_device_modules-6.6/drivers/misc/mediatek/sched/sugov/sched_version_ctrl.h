/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

enum {
	EAS_5_5 = 550,
	EAS_5_5_1 = 551,
	EAS_6_1 = 600,
	EAS_6_5 = 650,
};

int init_sched_ctrl(void);
extern bool legacy_api_support_get(void);
extern bool sched_vip_enable_get(void);
extern bool sched_gear_hints_enable_get(void);
extern bool sched_updown_migration_enable_get(void);
extern bool sched_skip_hiIRQ_enable_get(void);
extern bool sched_rt_aggre_preempt_enable_get(void);
extern bool sched_post_init_util_enable_get(void);
extern bool sched_percore_l3_bw_get(void);
extern bool sched_dsu_pwr_enable_get(void);
