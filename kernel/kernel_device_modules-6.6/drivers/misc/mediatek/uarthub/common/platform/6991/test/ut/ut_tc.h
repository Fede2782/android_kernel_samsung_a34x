/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef UT_TC_H
#define UT_TC_H

/* UT Test API for IP level test */
extern int uarthub_ut_tc_000_help_info(void);

static int (*dvt_ut_funcs[])(void) = {
	uarthub_ut_tc_000_help_info
};

static const char * const dvt_ut_funcs_name[] = {
	"help info"
};

#endif
