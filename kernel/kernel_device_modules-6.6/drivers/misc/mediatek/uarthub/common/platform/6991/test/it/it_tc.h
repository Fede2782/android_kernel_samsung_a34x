/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef IT_TC_H
#define IT_TC_H

/* IT Test API level test */
extern int uarthub_it_tc_000_help_info(void);

static int (*dvt_it_funcs[])(void) = {
	uarthub_it_tc_000_help_info
};

static const char * const dvt_it_funcs_name[] = {
	"help info"
};

#endif
