/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_dmabuf_events

#if !defined(_TRACE_MTK_DMABUF_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_DMABUF_EVENTS_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(dmabuf_trace_template,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf),
	TP_STRUCT__entry(__vstring(msg, vaf->fmt, vaf->va)),
	TP_fast_assign(
		__assign_vstr(msg, vaf->fmt, vaf->va);
	),
	TP_printk("%s", __get_str(msg))
);

#define DEFINE_DMABUF_EVENT(name)		\
DEFINE_EVENT(dmabuf_trace_template, name,	\
	TP_PROTO(struct va_format *vaf),	\
	TP_ARGS(vaf))

#ifdef CONFIG_ARM64
DEFINE_DMABUF_EVENT(tracing_mark_write);
#elif CONFIG_ARM
DEFINE_DMABUF_EVENT(tracing_mark_write_dma32);
#endif

#endif /* _TRACE_MTK_DMABUF_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/dma-buf/heaps
#define TRACE_INCLUDE_FILE mtk_dmabuf_events

/* This part must be outside protection */
#include <trace/define_trace.h>
