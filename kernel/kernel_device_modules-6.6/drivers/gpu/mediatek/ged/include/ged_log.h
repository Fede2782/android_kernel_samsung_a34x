/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_LOG_H__
#define __GED_LOG_H__

#include <linux/string_helpers.h>
#include "ged_type.h"

#if defined(__GNUC__)
#define GED_LOG_BUF_FORMAT_PRINTF(x, y) __printf(x, y)
#else
#define GED_LOG_BUF_FORMAT_PRINTF(x, y)
#endif

#define GED_LOG_BUF_NAME_LENGTH 64
#define GED_LOG_BUF_NODE_NAME_LENGTH 64


#define GED_LOG_BUF_TYPE_RINGBUFFER			        0
#define GED_LOG_BUF_TYPE_QUEUEBUFFER			    1
#define GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE	2
#define GED_LOG_BUF_TYPE                            int

GED_LOG_BUF_HANDLE ged_log_buf_alloc(int i32MaxLineCount,
	int i32MaxBufferSizeByte, GED_LOG_BUF_TYPE eType, const char *pszName,
	const char *pszNodeName);

GED_ERROR ged_log_buf_resize(GED_LOG_BUF_HANDLE hLogBuf, int i32NewMaxLineCount,
	int i32NewMaxBufferSizeByte);

GED_ERROR ged_log_buf_ignore_lines(GED_LOG_BUF_HANDLE hLogBuf,
	int i32LineCount);

GED_ERROR ged_log_buf_reset(GED_LOG_BUF_HANDLE hLogBuf);

void ged_log_buf_free(GED_LOG_BUF_HANDLE hLogBuf);

/* query by Name, return NULL if not found */
GED_LOG_BUF_HANDLE ged_log_buf_get(const char *pszName);

/* register a pointer,
 * it will be set after the corresponding buffer is allcated.
 */
int ged_log_buf_get_early(const char *pszName,
	GED_LOG_BUF_HANDLE *callback_set_handle);

GED_ERROR ged_log_buf_print(GED_LOG_BUF_HANDLE hLogBuf,
	const char *fmt, ...) GED_LOG_BUF_FORMAT_PRINTF(2, 3);

GED_ERROR
ged_log_buf_print2(GED_LOG_BUF_HANDLE hLogBuf, int i32LogAttrs,
	const char *fmt, ...) GED_LOG_BUF_FORMAT_PRINTF(3, 4);

GED_ERROR ged_log_system_init(void);

void ged_log_system_exit(void);

int ged_log_buf_write(GED_LOG_BUF_HANDLE hLogBuf,
	const char __user *pszBuffer, int i32Count);

void ged_log_dump(GED_LOG_BUF_HANDLE hLogBuf);
int ged_timer_or_trace_enable(void);

#if defined(CONFIG_GPU_MT8167) || defined(CONFIG_GPU_MT8173) ||\
defined(CONFIG_GPU_MT6739) || defined(CONFIG_GPU_MT6761)\
|| defined(CONFIG_GPU_MT6765)
extern void ged_dump_fw(void);
#endif

unsigned int is_gpu_ged_log_enable(void);

//debug_node info
#define MAX_NAME_SIZE 256
struct cmd_info {
	/* unit: ms */
	int pid;
	unsigned int value;
	unsigned int ori_value;
	unsigned long long ts;
	char buffer[MAX_NAME_SIZE];
	int user_id; //0: unexpected user, 1:sh (cmd), 2:powerhal
};

void init_cmd_info(struct cmd_info *cmd, unsigned int value);

// NOTICE: do not set_cmd_info with holding lock
void set_cmd_info(struct cmd_info *cmd, unsigned int ori_value, unsigned int value);
ssize_t get_cmd_info_dump(char *buf, int sz, ssize_t pos, struct cmd_info *cmd);

#endif
