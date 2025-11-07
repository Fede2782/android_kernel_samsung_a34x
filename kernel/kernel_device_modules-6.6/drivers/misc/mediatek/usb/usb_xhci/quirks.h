/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK xhci quirk driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Denis Hsu <denis.hsu@mediatek.com>
 */

#ifndef __XHCI_MTK_QUIRKS_H
#define __XHCI_MTK_QUIRKS_H

#include <linux/usb.h>
#include "usbaudio.h"
#include <linux/hashtable.h>
#include <linux/jiffies.h>

void xhci_mtk_init_snd_quirk(struct snd_usb_audio *chip);
void xhci_mtk_deinit_snd_quirk(struct snd_usb_audio *chip);
void xhci_mtk_apply_quirk(struct usb_device *udev);
void xhci_mtk_trace_init(struct device *dev);
void xhci_mtk_trace_deinit(struct device *dev);

#define XHCI_MBRAIN_STATE_TIMEOUT_MS 2000
struct xhci_mbrain {
	u16 vid;
	u16 pid;
	u16 bcd;
	enum usb_device_state	state;
	enum usb_device_speed	speed;
	u16 err_reason;
};

struct xhci_mbrain_hash_node {
	char *dev_name;
	struct xhci_mbrain mbrain_data;
	bool updated_db;
	struct delayed_work updated_db_work;
	unsigned long updated_db_work_delay;
	struct hlist_node node;
};

typedef void (*xhci_enum_mbrain_callback)(struct xhci_mbrain data);
int register_xhci_enum_mbrain_cb(xhci_enum_mbrain_callback cb);
int unregister_xhci_enum_mbrain_cb(void);
#endif
