/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_H__
#define __SCP_H__

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT)
#include "scp_cm4.h"
#else
#include "scp_rv.h"
#endif

#if IS_ENABLED(CONFIG_SHUB)
extern int get_scp_dump_size(void);

struct shub_dump {
	int reason;
	int size;
	char *dump;
	char *mini_dump;
};
extern int shub_dump_notifier_register(struct notifier_block *nb);
#endif

#endif
