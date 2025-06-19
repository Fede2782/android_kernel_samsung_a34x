/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
//#include <linux/proc_fs.h>
//#include <linux/uaccess.h>

#ifndef __PMSR_H__
#define __PMSR_H__

#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define MONTYPE_RISING			(0)
#define MONTYPE_FALLING			(1)
#define MONTYPE_HIGH_LEVEL		(2)
#define MONITPE_LOW_LEVEL		(3)

#define SPEED_MODE_NORMAL		(0x0)
#define SPEED_MODE_SPEED		(0x1)

#define DEFAULT_SELTYPE			(0)
#define DEFAULT_MONTYPE			MONTYPE_HIGH_LEVEL
#define DEFAULT_SPEED_MODE		SPEED_MODE_NORMAL

#define PMSR_PERIOD_MS			(1000)
#define WINDOW_LEN_SPEED		(PMSR_PERIOD_MS * 0x65B8)
#define WINDOW_LEN_NORMAL		(PMSR_PERIOD_MS * 0xD)
#define GET_EVENT_RATIO_SPEED(x)	((x)/(WINDOW_LEN_SPEED/1000))
#define GET_EVENT_RATIO_NORMAL(x)	((x)/(WINDOW_LEN_NORMAL/1000))
#define PMSR_MET_CH 8
#define PMSR_MAX_SIG_CH 150
#define MTK_PMSR_BUF_WRITESZ 512

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
enum APMCU_PMSR_SCMI_UUID {
	APMCU_SCMI_UUID_PMSR = 0,

	APMCU_SCMI_UUID_NUM,
};

enum PMSR_TOOL_ACT {
	PMSR_TOOL_ACT_ENABLE = 1,
	PMSR_TOOL_ACT_DISABLE,
	PMSR_TOOL_ACT_WINDOW,
	PMSR_TOOL_ACT_SIGNAL,
	PMSR_TOOL_ACT_MONTYPE,
	PMSR_TOOL_ACT_SIGNUM,
	PMSR_TOOL_ACT_EN,
	PMSR_TOOL_ACT_TEST,
	PMSR_TOOL_ACT_SELTYPE,
	PMSR_TOOL_ACT_GET_SRAM,
	PMSR_TOOL_ACT_PROF_CNT_EN,
};
#endif

struct pmsr_channel {
	unsigned int dpmsr_id;
	unsigned int signal_id;
};

struct pmsr_dpmsr_cfg {
	unsigned int seltype;   /* dpmsr select type */
	unsigned int montype;	/* 2 bits, monitor type */
	unsigned int signum;
	unsigned int en;
};

struct pmsr_tool_mon_results {
	unsigned int results[PMSR_MAX_SIG_CH];
	unsigned int time_stamp;
	unsigned int winlen;
};

struct pmsr_tool_mon_results_acc {
	uint64_t results[PMSR_MAX_SIG_CH];
	unsigned int time_stamp;
	uint64_t winlen;
	unsigned int acc_num;
};

struct pmsr_tool_results {
	unsigned int oldest_idx;
};

struct pmsr_cfg {
	struct pmsr_dpmsr_cfg *dpmsr;
	struct pmsr_channel ch[PMSR_MET_CH];	/* channel 0~3 config */
	unsigned int pmsr_signal_id[PMSR_MAX_SIG_CH];
	unsigned int pmsr_speed_mode;		/* 0: normal, 1: high speed */
	unsigned int pmsr_window_len;
	unsigned int clean_records;
	unsigned int pmsr_sample_rate;
	unsigned int dpmsr_count;
	unsigned int pmsr_sig_count;
	bool enable;
	unsigned int err;
	unsigned int test;
	unsigned int prof_cnt;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct pmsr_tool_mon_results *share_buf;
	unsigned int pmsr_tool_buffer_max_space;
	struct pmsr_tool_results *pmsr_tool_share_results;
	struct pmsr_tool_mon_results_acc acc_results;
#endif
	/* pmsr_window_len
	 *	0: will be automatically updated for current speed mode
	 * for normal speed mode, set to WINDOW_LEN_NORMAL.
	 * for high speed mode, set to WINDOW_LEN_SPEED.
	 */
};
#endif /* __PMSR_H__ */

