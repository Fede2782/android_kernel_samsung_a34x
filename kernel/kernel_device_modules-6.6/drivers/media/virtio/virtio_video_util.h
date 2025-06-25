/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _VIRTIO_VIDEO_UTIL_H_
#define _VIRTIO_VIDEO_UTIL_H_

#include <aee.h>
#include <linux/types.h>
#include <linux/trace_events.h>

#include "virtio_video.h"

#define LOG_PROPERTY_SIZE 1024
#define LOG_PARAM_INFO_SIZE 64
#define MAX_SUPPORTED_LOG_PARAMS_COUNT 12

#define SNPRINTF(args...)							\
	do {											\
		if (snprintf(args) < 0)						\
			pr_notice("[ERROR] %s(),%d: snprintf error\n", __func__, __LINE__);	\
	} while (0)
#define SPRINTF(args...)							\
	do {											\
		if (sprintf(args) < 0)						\
			pr_notice("[ERROR] %s(),%d: sprintf error\n", __func__, __LINE__);	\
	} while (0)

extern int virtio_v4l2_dbg_level;
extern bool virtio_vcodec_dbg;
extern bool virtio_vcodec_perf;
extern bool virtio_vdec_trace_enable;

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
#define DEBUG   1
#endif

enum virtio_debug_level {
	VIRTIO_DBG_L0 = 0,
	VIRTIO_DBG_L1 = 1,
	VIRTIO_DBG_L2 = 2,
	VIRTIO_DBG_L3 = 3,
	VIRTIO_DBG_L4 = 4,
	VIRTIO_DBG_L5 = 5,
	VIRTIO_DBG_L6 = 6,
	VIRTIO_DBG_L7 = 7,
	VIRTIO_DBG_L8 = 8,
};

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_TRACE_DEBUG)
#define virtio_vcodec_trace_begin(fmt, args...) do { \
			if (virtio_vdec_trace_enable) { \
				virtio_vcodec_trace("B|%d|"fmt"\n", current->tgid, ##args); \
			} \
		} while (0)

#define virtio_vcodec_trace_begin_func() virtio_vcodec_trace_begin("%s", __func__)

#define virtio_vcodec_trace_end() do { \
			if (virtio_vdec_trace_enable) { \
				virtio_vcodec_trace("E\n"); \
			} \
		} while (0)

#define virtio_vcodec_trace_count(name, count) do { \
			if (virtio_vdec_trace_enable) { \
				virtio_vcodec_trace("C|%d|%s|%d\n", current->tgid, name, count); \
			} \
		} while (0)

void virtio_vcodec_trace(const char *fmt, ...);
#else
#define virtio_vcodec_trace_begin(fmt, args...)
#define virtio_vcodec_trace_begin_func()
#define virtio_vcodec_trace_end()
#define virtio_vcodec_trace_count(name, count)
#endif

#if defined(DEBUG)

#define virtio_v4l2_debug(level, fmt, args...)                              \
	do {                                                             \
		if (((virtio_v4l2_dbg_level) & (level)) == (level)) {          \
			virtio_vcodec_trace_begin("virtio_v4l2_debug_log"); \
			pr_notice("[VIRTIO_V4L2] level=%d %s(),%d: " fmt "\n",\
				level, __func__, __LINE__, ##args);      \
			virtio_vcodec_trace_end(); \
		} \
	} while (0)

#define virtio_v4l2_err(fmt, args...)                \
	pr_notice("[VIRTIO_V4L2][ERROR] %s:%d: " fmt "\n", __func__, __LINE__, \
		   ##args)

#define virtio_v4l2_debug_enter()  virtio_v4l2_debug(VIRTIO_DBG_L8, "+")
#define virtio_v4l2_debug_leave()  virtio_v4l2_debug(VIRTIO_DBG_L8, "-")

#define virtio_vcodec_debug(h, fmt, args...)                               \
	do {                                                            \
		if (virtio_vcodec_dbg)                                  \
			pr_notice("[VIRTIO_VCODEC][%d]: %s() " fmt "\n",     \
				((struct virtio_video *)h->vdev)->id,  \
				__func__, ##args);                      \
	} while (0)

#define virtio_vcodec_perf_log(fmt, args...)                               \
	do {                                                            \
		if (virtio_vcodec_perf)                          \
			pr_info("[VRITO_PERF] " fmt "\n", ##args);        \
	} while (0)


#define virtio_vcodec_err(h, fmt, args...)                                 \
	pr_notice("[VIRTIO_VCODEC][ERROR][%d]: %s() " fmt "\n",               \
		   ((struct virtio_video *)h->vdev)->id, __func__, ##args)

#define virtio_vcodec_debug_enter(h)  virtio_vcodec_debug(h, "+")
#define virtio_vcodec_debug_leave(h)  virtio_vcodec_debug(h, "-")
#else

#define virtio_v4l2_debug(level, fmt, args...)
#define virtio_v4l2_err(fmt, args...)
#define virtio_v4l2_debug_enter()
#define virtio_v4l2_debug_leave()

#define virtio_vcodec_debug(h, fmt, args...)
#define virtio_vcodec_err(h, fmt, args...)
#define virtio_vcodec_debug_enter(h)
#define virtio_vcodec_debug_leave(h)

#endif

#ifdef CONFIG_MTK_AEE_FEATURE
#define v4l2_aee_print(string, args...) do {\
	char vcu_name[100];\
	int ret;\
	ret = snprintf(vcu_name, 100, "[VIRTIO_V4L2] "string, ##args); \
	if (ret > 0)\
		pr_notice("[VIRTIO_V4L2] error:"string, ##args);  \
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_MMPROFILE_BUFFER | DB_OPT_NE_JBT_TRACES, \
			vcu_name, "[MTK_VIRTIO_V4L2] error:"string, ##args); \
	} while (0)
#else
#define v4l2_aee_print(string, args...) \
		pr_notice("[VIRTIO_V4L2] error:"string, ##args)
#endif

//todo, set log to B/E
void virtio_vcodec_set_log(struct virtio_video *vv, const char *val);

#endif /* _VIRTIO_VIDEO_UTIL_H_ */
