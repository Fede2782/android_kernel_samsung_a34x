/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_HW_VOTER_DBG_H__
#define __VCP_HW_VOTER_DBG_H__

#define RES_NAME_LEN		(26)

enum {
	HW_VOTER_DBG_CMD_TEST,
	HW_VOTER_DBG_CMD_ENABLE,
};

struct hwvoter_ipi_test_t {
	unsigned char cmd;
	unsigned char op;
	unsigned char res_name[RES_NAME_LEN];
	unsigned short val;
};

extern int vcp_hw_voter_dbg_init(void);

#endif  /* __VCP_HW_VOTER_DBG_H__ */
