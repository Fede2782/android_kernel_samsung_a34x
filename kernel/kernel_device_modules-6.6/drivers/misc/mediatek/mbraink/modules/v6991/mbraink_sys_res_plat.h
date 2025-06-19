/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MBRAINK_PLAT_SYS_RES_SIGNAL_H__
#define __MBRAINK_PLAT_SYS_RES_SIGNAL_H__

#include <swpm_v6991_ext.h>
#include "mbraink_sys_res.h"


enum mbraink_sys_res_record_plat_op_id {
	MBRAINK_SYS_RES_DURATION = 0,
	MBRAINK_SYS_RES_SUSPEND_TIME,
	MBRAINK_SYS_RES_SIG_TIME,
	MBRAINK_SYS_RES_SIG_ID,
	MBRAINK_SYS_RES_SIG_GROUP_ID,
	MBRAINK_SYS_RES_SIG_OVERALL_RATIO,
	MBRAINK_SYS_RES_SIG_SUSPEND_RATIO,
	MBRAINK_SYS_RES_SIG_ADDR,
};

extern struct mbraink_sys_res_group_info sys_res_group_info[NR_SPM_GRP];

int mbraink_sys_res_plat_init(void);
void mbraink_sys_res_plat_deinit(void);


#endif


