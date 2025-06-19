// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/sched.h>

#include "virtio_video_util.h"

/* For vcodec, this will enable logs in vcodec/*/
bool virtio_vcodec_dbg;
EXPORT_SYMBOL_GPL(virtio_vcodec_dbg);

/* For vcodec performance measure */
bool virtio_vcodec_perf;
EXPORT_SYMBOL_GPL(virtio_vcodec_perf);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under virtio/.
 */
int virtio_v4l2_dbg_level;
EXPORT_SYMBOL_GPL(virtio_v4l2_dbg_level);

/* For vdec kernel driver trace enable */
bool virtio_vdec_trace_enable;
EXPORT_SYMBOL_GPL(virtio_vdec_trace_enable);


/* VIRTIO VIDEO FTRACE */
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_TRACE_DEBUG)
void virtio_vcodec_trace(const char *fmt, ...)
{
	char buf[256] = {0};
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_puts(buf);
}
EXPORT_SYMBOL(virtio_vcodec_trace);
#endif

void virtio_vcodec_set_log(struct virtio_video *vv, const char *val)
{
	int i, argc = 0;
	char (*argv)[LOG_PARAM_INFO_SIZE] = NULL;
	char *temp = NULL;
	char *token = NULL;
	long temp_val = 0;
	char *log = NULL;

	if (vv == NULL || val == NULL || strlen(val) == 0)
		return;

	virtio_v4l2_debug(0, "val: %s", val);

	argv = kzalloc(MAX_SUPPORTED_LOG_PARAMS_COUNT * 2 * LOG_PARAM_INFO_SIZE, GFP_KERNEL);
	if (!argv)
		return;
	log = kzalloc(LOG_PROPERTY_SIZE, GFP_KERNEL);
	if (!log) {
		kfree(argv);
		return;
	}

	SNPRINTF(log, LOG_PROPERTY_SIZE, "%s", val);
	temp = log;
	for (token = strsep(&temp, "\n\r ");
	     token != NULL && argc < MAX_SUPPORTED_LOG_PARAMS_COUNT * 2;
	     token = strsep(&temp, "\n\r ")) {
		if (strlen(token) == 0)
			continue;
		SNPRINTF(argv[argc], LOG_PARAM_INFO_SIZE, "%s", token);
		argc++;
	}

	for (i = 0; i < argc-1; i += 2) {
		if (strlen(argv[i]) == 0)
			continue;
		if (strcmp("-virtio_vcodec_dbg", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				virtio_vcodec_dbg = temp_val;
				virtio_v4l2_debug(0, "virtio_vcodec_dbg: %d\n", virtio_vcodec_dbg);
			}
		} else if (strcmp("-virtio_vcodec_perf", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				virtio_vcodec_perf = temp_val;
				virtio_v4l2_debug(0, "virtio_vcodec_perf: %d\n", virtio_vcodec_perf);
			}
		} else if (strcmp("-virtio_v4l2_dbg_level", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				virtio_v4l2_dbg_level = temp_val;
				virtio_v4l2_debug(0, "virtio_v4l2_dbg_level: %d\n", virtio_v4l2_dbg_level);
			}
		} else {
			virtio_v4l2_debug(0, "unknown log setting\n");
		}
	}
	kfree(argv);
	kfree(log);
}
EXPORT_SYMBOL_GPL(virtio_vcodec_set_log);

MODULE_LICENSE("GPL");

