// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload IPI
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include "usb_offload.h"
#include "audio_task_usb_msg_id.h"
#include "audio_task_manager.h"

static bool register_ipi_receiver;

static void usb_offload_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	if (!ipi_msg) {
		USB_OFFLOAD_ERR("%s null ipi_msg\n", __func__);
		return;
	}

	USB_OFFLOAD_DBG("msg_id:0x%x\n", ipi_msg->msg_id);

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_XHCI_IRQ:
	case AUD_USB_MSG_D2A_XHCI_EP_INFO:
		usb_offload_ipi_hid_handler((void *)ipi_msg);
		break;
	case AUD_USB_MSG_D2A_TRACE_URB:
		usb_offload_ipi_trace_handler((void *)ipi_msg);
		break;
	default:
		USB_OFFLOAD_ERR("unknown msg_id:0x%x\n", ipi_msg->msg_id);
		break;
	}
}

void usb_offload_register_ipi_recv(void)
{
	if (!register_ipi_receiver) {
		audio_task_register_callback(
			TASK_SCENE_USB_DL, usb_offload_ipi_recv);
		register_ipi_receiver = true;
	}
}
