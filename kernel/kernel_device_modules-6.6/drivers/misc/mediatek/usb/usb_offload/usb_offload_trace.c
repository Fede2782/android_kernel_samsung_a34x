// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Trace Support
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#define CREATE_TRACE_POINTS

#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "xhci.h"
#include "audio_task_manager.h"
#include "audio_task_usb_msg_id.h"
#include "usb_offload.h"
#include "usb_offload_trace.h"

unsigned int enable_trace;
module_param(enable_trace, uint, 0644);
MODULE_PARM_DESC(enable_trace, "Enable/Disable Audio Data Trace");

/* export for u_logger to register trace point */
EXPORT_TRACEPOINT_SYMBOL_GPL(usb_offload_trace_trigger);

#define TRACE_SUPPORT_OFT  (0)
#define TRACE_START_OFT    (1)
#define TRACE_OUT_INIT_OFT (2)
#define TRACE_IN_INIT_OFT  (3)
#define TRACE_SUPPORT      (1 << 0)
#define TRACE_START        (1 << 1)
#define TRACE_OUT_INIT     (1 << 2)
#define TRACE_IN_INIT      (1 << 3)

struct stream_info {
	int direction;
	u16 slot;
	u16 ep;
	struct usb_offload_buffer *memory;
	struct usb_endpoint_descriptor desc;
	void *transfer_buffer;
	int actual_length;
	struct usb_offload_buffer buf_trace;
};

struct trace_manager {
	struct device *dev;
	struct stream_info stream_in;
	struct stream_info stream_out;
	u8 flag;
} manager;

#define chk_flag(mag, bit)   ((mag)->flag & bit)
#define set_flag(mag, bit)   ((mag)->flag |= bit)
#define clr_flag(mag, bit)   ((mag)->flag &= ~(bit))

unsigned int debug_trace;
module_param(debug_trace, uint, 0644);
MODULE_PARM_DESC(debug_trace, "Enable/Disable Trace Log");

#define trace_info(dev, fmt, args...) dev_info(dev, "UO, "fmt, ##args)
#define trace_dbg(dev, fmt, args...) do { \
	if (debug_trace) \
		dev_info(dev, fmt, ##args); \
} while(0)

static void stop_trace_stream(struct stream_info *stream);
static int usb_offload_trace_trigger(struct trace_manager *mag,
		struct stream_info *stream);

static inline void stream_init(struct stream_info *stream, bool is_in)
{
	stream->direction = is_in ? 1 : 0;
	stream->memory = NULL;
	stream->transfer_buffer = NULL;
	stream->actual_length = 0;
	memset(&stream->desc, 0, sizeof(struct usb_endpoint_descriptor));
}

static inline struct stream_info *get_stream(struct trace_manager *mag, bool is_in)
{
	return is_in ? &mag->stream_in : &mag->stream_out;
}

static inline struct stream_info *find_stream(struct trace_manager *mag, dma_addr_t phys)
{
	struct stream_info *stream;

	if (mag->stream_in.memory && phys == mag->stream_in.memory->dma_addr)
		stream = &mag->stream_in;
	else if (mag->stream_out.memory && phys == mag->stream_out.memory->dma_addr)
		stream = &mag->stream_out;
	else
		stream = NULL;

	return stream;
}

static inline void dump_manager_flag(struct trace_manager *mag)
{
	trace_dbg(mag->dev, "support:%d start:%d in:%d out:%d\n",
		chk_flag(mag, TRACE_SUPPORT) >> TRACE_SUPPORT_OFT,
		chk_flag(mag, TRACE_START) >> TRACE_START_OFT,
		chk_flag(mag, TRACE_IN_INIT) >> TRACE_IN_INIT_OFT,
		chk_flag(mag, TRACE_OUT_INIT) >> TRACE_OUT_INIT_OFT);
}

int usb_offload_debug_init(struct usb_offload_dev *udev)
{
	struct trace_manager *mag = &manager;

	mag->dev = udev->dev;
	mag->flag = 0;

	stream_init(&mag->stream_in, true);
	stream_init(&mag->stream_out, false);
	set_flag(mag, TRACE_SUPPORT);
	clr_flag(mag, TRACE_START);
	clr_flag(mag, TRACE_IN_INIT);
	clr_flag(mag, TRACE_OUT_INIT);

	dump_manager_flag(mag);
	trace_info(mag->dev, "debug init success\n");

	return 0;
}

void usb_offload_trace_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	struct trace_manager *mag = &manager;
	struct stream_info *stream;
	u32 length;
	dma_addr_t phys;

	if (!ipi_msg) {
		trace_info(mag->dev, "%s null ipi_msg\n", __func__);
		return;
	}

	if (!chk_flag(mag, TRACE_START)) {
		trace_info(mag->dev, "%s trace doesn't start\n", __func__);
		return;
	}

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_TRACE_URB:
		phys = (dma_addr_t)ipi_msg->param1;
		length = ipi_msg->param2;

		stream = find_stream(mag, phys);
		if (!stream) {
			trace_dbg(mag->dev, "phys:0x%llx isn't trace buffer\n", phys);
			break;
		}

		stream->transfer_buffer = (void *)stream->memory->dma_area;
		stream->actual_length = length;

		if (stream->transfer_buffer)
			usb_offload_trace_trigger(mag, stream);
		else
			trace_dbg(mag->dev, "Invalid Address!! phys:0x%llx length:%d\n",
				phys, length);

		break;
	default:
		break;
	}
}

int usb_offload_trace_stream_init(struct usb_offload_buffer *trace_buffer,
	u16 slot, u16 ep, bool is_in, struct usb_endpoint_descriptor *desc)
{
	struct trace_manager *mag = &manager;
	struct stream_info *stream;

	if (!chk_flag(mag, TRACE_SUPPORT))
		return -EOPNOTSUPP;

	if ((is_in && chk_flag(mag, TRACE_IN_INIT)) ||
		(!is_in && chk_flag(mag, TRACE_OUT_INIT)) ||
		!desc || !trace_buffer || !trace_buffer->allocated) {
		trace_info(mag->dev, "stream init fail, dir:%d\n", is_in);
		return -EINVAL;
	}

	stream = get_stream(mag, is_in);
	stream->memory = trace_buffer;
	stream->slot = slot;
	stream->ep = ep;
	memcpy(&stream->desc, desc, sizeof(struct usb_endpoint_descriptor));

	trace_info(mag->dev, "%s is_in:%d ep_type:%d\n", __func__,
		is_in, usb_endpoint_type(desc));

	if (is_in)
		set_flag(mag, TRACE_IN_INIT);
	else
		set_flag(mag, TRACE_OUT_INIT);

	if (!chk_flag(mag, TRACE_START))
		set_flag(mag, TRACE_START);

	dump_manager_flag(mag);
	return 0;
}

int usb_offload_trace_stream_deinit(struct stream_info *stream)
{
	struct trace_manager *mag = &manager;
	bool is_in;

	if (!stream)
		return -EINVAL;

	is_in = stream->direction;

	if (!chk_flag(mag, TRACE_SUPPORT))
		return -EOPNOTSUPP;

	if ((is_in && !chk_flag(mag, TRACE_IN_INIT)) ||
		(!is_in && !chk_flag(mag, TRACE_OUT_INIT))) {
		trace_info(mag->dev, "stream deinit fail, dir:%d\n", is_in);
		return -EINVAL;
	}

	trace_info(mag->dev, "%s is_in:%d ep_type:%d\n", __func__,
		is_in, usb_endpoint_type(&stream->desc));

	stream->memory = NULL;
	stream->actual_length = 0;
	memset(&stream->desc, 0, sizeof(struct usb_endpoint_descriptor));

	if (is_in)
		clr_flag(mag, TRACE_IN_INIT);
	else
		clr_flag(mag, TRACE_OUT_INIT);

	if (!chk_flag(mag, TRACE_IN_INIT) && !chk_flag(mag, TRACE_OUT_INIT))
		clr_flag(mag, TRACE_START);

	dump_manager_flag(mag);
	return 0;
}

static int usb_offload_trace_trigger(struct trace_manager *mag,
		struct stream_info *stream)
{
	bool is_in;

	if (!mag || !stream)
		return -EINVAL;

	is_in = (stream->direction == 1);

	if (!chk_flag(mag, TRACE_SUPPORT))
		return -EOPNOTSUPP;

	if ((is_in && !chk_flag(mag, TRACE_IN_INIT)) ||
		(!is_in && !chk_flag(mag, TRACE_OUT_INIT)))
		return -EINVAL;

	trace_dbg(mag->dev, "%s stream:%p buffer:%p len:%d des:%p is_in:%d\n",
		__func__, stream, stream->transfer_buffer,
		stream->actual_length, &stream->desc, is_in);

	trace_usb_offload_trace_trigger((void *)stream,
		stream->transfer_buffer, stream->actual_length,
		stream->slot, stream->ep, &stream->desc);

	return 0;
}

void usb_offload_ipi_trace_handler(void *param)
{
	struct ipi_msg_t *ipi_msg = NULL;
	struct trace_manager *mag = &manager;
	struct stream_info *stream;
	u32 length;
	dma_addr_t phys;

	ipi_msg = (struct ipi_msg_t *)param;
	if (!ipi_msg) {
		trace_info(mag->dev, "%s null ipi_msg\n", __func__);
		return;
	}

	if (!chk_flag(mag, TRACE_START)) {
		trace_info(mag->dev, "%s trace doesn't start\n", __func__);
		return;
	}

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_TRACE_URB:
		phys = (dma_addr_t)ipi_msg->param1;
		length = ipi_msg->param2;

		stream = find_stream(mag, phys);
		if (!stream) {
			trace_dbg(mag->dev, "phys:0x%llx isn't trace buffer\n", phys);
			break;
		}

		stream->transfer_buffer = (void *)stream->memory->dma_area;
		stream->actual_length = length;

		if (stream->transfer_buffer)
			usb_offload_trace_trigger(mag, stream);
		else
			trace_dbg(mag->dev, "Invalid Address!! phys:0x%llx length:%d\n",
				phys, length);

		break;
	default:
		break;
	}

}

int send_trace_ipi_msg_to_adsp(struct usb_trace_msg *trace_msg)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;

	USB_OFFLOAD_INFO("enable:%d disable_all:%d phys:0x%llx size:%d\n",
		trace_msg->enable, trace_msg->disable_all, trace_msg->buffer, trace_msg->size);

	send_result = audio_send_ipi_msg(
					 &ipi_msg, TASK_SCENE_USB_DL,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_DMA,
					 AUDIO_IPI_MSG_NEED_ACK,
					 AUD_USB_MSG_A2D_ENABLE_TRACE_URB,
					 sizeof(struct usb_trace_msg),
					 0,
					 trace_msg);
	if (send_result == 0)
		send_result = ipi_msg.param2;

	if (send_result != 0)
		USB_OFFLOAD_INFO("USB Offload trace IPI msg send fail\n");
	else
		USB_OFFLOAD_INFO("USB Offload trace ipi msg send succeed\n");

	return send_result;
}

void prepare_and_send_trace_ipi_msg(struct usb_audio_stream_msg *msg,
	bool enable, bool disable_all)
{
	struct trace_manager *mag = &manager;
	struct stream_info *stream;
	struct usb_trace_msg trace_msg = {0};
	struct usb_offload_buffer *trace_buf;
	int mem_id, dir;
	u16 slot, ep;
	struct usb_endpoint_descriptor *desc;

	if (!enable_trace) {
		trace_info(mag->dev, "Trace Disable\n");
		goto END_HANDLE_TRACE_MSG;
	}

	trace_dbg(mag->dev, "enable:%d disalbe_all:%d\n", enable, disable_all);

	if (disable_all) {
		if (!chk_flag(mag, TRACE_IN_INIT) && !chk_flag(mag, TRACE_OUT_INIT)) {
			trace_dbg(mag->dev, "no need to disalbe all\n");
			goto END_HANDLE_TRACE_MSG;
		}
		trace_msg.enable = 0;
		trace_msg.disable_all = 1;
		trace_msg.direction = 0;
		trace_msg.buffer = 0;
		trace_msg.size = 0;
		if (send_trace_ipi_msg_to_adsp(&trace_msg))
			goto END_HANDLE_TRACE_MSG;
		stop_trace_stream(&mag->stream_in);
		stop_trace_stream(&mag->stream_out);
	}

	if (!msg)
		goto END_HANDLE_TRACE_MSG;

	dir = msg->direction;
	stream = get_stream(mag, dir);
	trace_buf = &stream->buf_trace;

	if (enable) {
		mem_id = USB_OFFLOAD_MEM_DRAM_ID;
		desc = &msg->std_as_data_ep_desc;
		slot = msg->slot_id;
		ep = xhci_get_endpoint_index_(desc);
		/* allocate trace buffer */
		if (trace_buf->allocated && mtk_offload_free_mem(trace_buf))
			trace_dbg(mag->dev, "fail to free trace_buffer, dir:%d", dir);
		if (mtk_offload_alloc_mem(trace_buf, msg->urb_size, 64, mem_id, true))
			goto END_HANDLE_TRACE_MSG;
		/* init trace_stream */
		if (usb_offload_trace_stream_init(trace_buf, slot, ep, dir, desc)) {
			trace_dbg(mag->dev, "init stream fail");
			mtk_offload_free_mem(trace_buf);
			goto END_HANDLE_TRACE_MSG;
		}
		/* prepare trace_msg */
		trace_msg.enable = 1;
		trace_msg.disable_all = 0;
		trace_msg.direction = dir;
		trace_msg.buffer = (unsigned long long)trace_buf->dma_addr;
		trace_msg.size = (unsigned int)trace_buf->dma_bytes;

		/* send IPI_MSG(enable_trace:1) */
		if (send_trace_ipi_msg_to_adsp(&trace_msg))
			stop_trace_stream(stream);
	} else {
		if ((stream->direction && !chk_flag(mag, TRACE_IN_INIT)) ||
			(!stream->direction && !chk_flag(mag, TRACE_OUT_INIT))) {
			trace_dbg(mag->dev, "no need to disalbe dir:%d\n", stream->direction);
			goto END_HANDLE_TRACE_MSG;
		}
		trace_msg.enable = 0;
		trace_msg.disable_all = 0;
		trace_msg.direction = dir;
		trace_msg.buffer = 0;
		trace_msg.size = 0;

		/* send IPI_MSG(enable_trace:0) */
		if (send_trace_ipi_msg_to_adsp(&trace_msg))
			goto END_HANDLE_TRACE_MSG;
		stop_trace_stream(stream);
	}
END_HANDLE_TRACE_MSG:
	return;
}

static void stop_trace_stream(struct stream_info *stream)
{
	if (!stream)
		return;

	if (usb_offload_trace_stream_deinit(stream)) {
		USB_OFFLOAD_INFO("fail to deinit trace_stream, dir:%d", stream->direction);
		return;
	}

	if (mtk_offload_free_mem(&stream->buf_trace))
		USB_OFFLOAD_INFO("fail to free trace_buffer, dir:%d", stream->direction);
}
