// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "it_tc.h"
#include "it_tc_common.h"

int uarthub_it_tc_000_help_info(void)
{
	int i = 0;

	UTLOG("IT help info : ");
	for(i = 0; i < sizeof(dvt_it_funcs_name) / sizeof(char *); i++)
		UTLOG("[%d] = %s", i, dvt_it_funcs_name[i]);
	return 0;
}
