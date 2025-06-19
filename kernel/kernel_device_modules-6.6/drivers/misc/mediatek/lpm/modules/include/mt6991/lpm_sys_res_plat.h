/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __PLAT_SYS_RES_SIGNAL_H__
#define __PLAT_SYS_RES_SIGNAL_H__

#include <linux/types.h>

enum lpm_sys_res_record_plat_op_id {
	SYS_RES_DURATION = 0,
	SYS_RES_SUSPEND_TIME,
	SYS_RES_SIG_TIME,
	SYS_RES_SIG_ID,
	SYS_RES_SIG_GROUP_ID,
	SYS_RES_SIG_OVERALL_RATIO,
	SYS_RES_SIG_SUSPEND_RATIO,
	SYS_RES_SIG_ADDR,
};

extern struct sys_res_group_info *group_info;

int lpm_sys_res_plat_init(void);
void lpm_sys_res_plat_deinit(void);

#endif


