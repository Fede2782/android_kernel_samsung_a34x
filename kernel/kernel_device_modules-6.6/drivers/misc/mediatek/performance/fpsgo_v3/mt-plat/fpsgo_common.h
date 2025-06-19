/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _FPSGO_COMMON_H_
#define _FPSGO_COMMON_H_

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/fs.h>

#if defined(CONFIG_MTK_FPSGO_V3)
enum FPSGO_TRACE_TYPE {
	FPSGO_DEBUG_MANDATORY = 0,
	FPSGO_DEBUG_FBT,
	FPSGO_DEBUG_FSTB,
	FPSGO_DEBUG_XGF,
	FPSGO_DEBUG_FBT_CTRL,
	FPSGO_DEBUG_MAX,
};

extern int powerhal_tid;

void __fpsgo_systrace_c(int type, pid_t pid, unsigned long long bufID,
	int value, const char *name, ...);
void __fpsgo_systrace_b(int type, pid_t pid, const char *name, ...);
void __fpsgo_systrace_e(int type);

#define fpsgo_systrace_c_fbt(pid, bufID, val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, pid, bufID, val, fmt)
#define fpsgo_systrace_c_fbt_debug(pid, bufID, val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_FBT, pid, bufID, val, fmt)
#define fpsgo_systrace_c_fstb_man(pid, val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, pid, val, fmt)
#define fpsgo_systrace_c_fstb(pid, bufID, val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_FSTB, pid, 0, val, fmt)
#define fpsgo_systrace_c_xgf(pid, bufID, val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_XGF, pid, 0, val, fmt)
#define __cpu_ctrl_systrace(val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, powerhal_tid, 0, val, fmt)
#define __cpu_ctrl_systrace_debug(val, fmt...) \
	__fpsgo_systrace_c(FPSGO_DEBUG_FBT_CTRL, powerhal_tid, 0, val, fmt)

void fpsgo_switch_enable(int enable);
int fpsgo_is_enable(void);

int fbt_cpu_get_bhr(void);

#else
static inline void fpsgo_systrace_c_fbt(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }
static inline void fpsgo_systrace_c_fbt_debug(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }
static inline void fpsgo_systrace_c_fstb_man(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }
static inline void fpsgo_systrace_c_fstb(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }
static inline void fpsgo_systrace_c_xgf(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }
static inline void __cpu_ctrl_systrace(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }
static inline void __cpu_ctrl_systrace_debug(pid_t id,
	unsigned long long bufID, int val, const char *s, ...) { }

static inline void fpsgo_switch_enable(int enable) { }
static inline int fpsgo_is_enable(void) { return 0; }

#endif
#endif
