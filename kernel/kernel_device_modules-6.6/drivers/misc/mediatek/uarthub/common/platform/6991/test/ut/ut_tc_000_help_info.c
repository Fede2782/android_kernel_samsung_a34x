// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "ut_tc.h"
#include "ut_tc_common.h"

int uarthub_ut_tc_000_help_info(void)
{
	int i = 0;

	UTLOG("UT help info:");
	for(i = 0; i < sizeof(dvt_ut_funcs_name) / sizeof(char *); i++)
		UTLOG("[%d] = %s", i, dvt_ut_funcs_name[i]);
	return 0;
}
