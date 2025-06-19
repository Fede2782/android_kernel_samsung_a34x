/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _nt50358a_SW_H_
#define _nt50358a_SW_H_

#ifndef BUILD_LK
struct nt50358a_setting_table {
	unsigned char cmd;
	unsigned char data;
};

extern int nt50358a_read_byte(unsigned char cmd, unsigned char *returnData);
extern int nt50358a_write_byte(unsigned char cmd, unsigned char writeData);
#endif

#endif

