/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK USB Offload Trace
 * *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jeremy Chou <jeremy.chou@mediatek.com>
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM usb_offload

#if !defined(_USB_OFFLOAD_TRACE_H__) || defined(TRACE_HEADER_MULTI_READ)
#define _USB_OFFLOAD_TRACE_H__

#include <linux/types.h>
#include <linux/usb.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(usb_offload_log_trace,
	TP_PROTO(void *stream,
		void *buffer,
		int length,
		u16 slot,
		u16 ep,
		struct usb_endpoint_descriptor *desc),
	TP_ARGS(stream, buffer, length, slot, ep, desc),
	TP_STRUCT__entry(
		__field(void *, stream)
		__field(void *, buffer)
		__field(int,    length)
		__field(int,    type)
	),
	TP_fast_assign(
		__entry->stream = stream;
		__entry->buffer = buffer;
		__entry->length = length;
		__entry->type = usb_endpoint_type(desc);
	),
	TP_printk("stream:%p buffer:%p length:%d type:%d",
		__entry->stream,
		__entry->buffer,
		__entry->length,
		__entry->type)
);

DEFINE_EVENT(usb_offload_log_trace, usb_offload_trace_trigger,
	TP_PROTO(void *stream,
		void *buffer,
		int length,
		u16 slot,
		u16 ep,
		struct usb_endpoint_descriptor *desc),
	TP_ARGS(stream, buffer, length, slot, ep, desc)
);


#endif /* _USB_OFFLOAD_TRACE_H__ */
/* this part has to be here */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE usb_offload_trace

#include <trace/define_trace.h>
