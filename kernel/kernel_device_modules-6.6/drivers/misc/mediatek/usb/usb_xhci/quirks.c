// SPDX-License-Identifier: GPL-2.0
/*
 * MTK xhci quirk driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Denis Hsu <denis.hsu@mediatek.com>
 */
#include <linux/usb/audio.h>
#include <linux/usb/quirks.h>
#include <linux/stringhash.h>
#include "quirks.h"
#include "xhci-mtk.h"
#include "xhci-trace.h"

#include <sound/asound.h>
#include "card.h"

struct usb_audio_quirk_flags_table {
	u32 id;
	u32 flags;
};

#define DEVICE_FLG(vid, pid, _flags) \
	{ .id = USB_ID(vid, pid), .flags = (_flags) }
#define VENDOR_FLG(vid, _flags) DEVICE_FLG(vid, 0, _flags)

/* quirk list in usbcore */
static const struct usb_device_id mtk_usb_quirk_list[] = {
	/* AM33/CM33 HeadSet */
	{USB_DEVICE(0x12d1, 0x3a07), .driver_info = USB_QUIRK_IGNORE_REMOTE_WAKEUP|USB_QUIRK_RESET},

	{ }  /* terminating entry must be last */
};

/* quirk list in /sound/usb */
static const struct usb_audio_quirk_flags_table mtk_snd_quirk_flags_table[] = {
		/* Device matches */
		DEVICE_FLG(0x2d99, 0xa026, /* EDIFIER H180 Plus */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		DEVICE_FLG(0x12d1, 0x3a07,	/* AM33/CM33 HeadSet */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		DEVICE_FLG(0x04e8, 0xa051,      /* SS USBC Headset (AKG) */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		DEVICE_FLG(0x04e8, 0xa057,
		   QUIRK_FLAG_CTL_MSG_DELAY),
		/* Vendor matches */
		VENDOR_FLG(0x2fc6,		/* Comtrue Devices */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		{} /* terminator */
};


static xhci_enum_mbrain_callback xhci_enum_mbrain_cb;
DEFINE_HASHTABLE(mbrain_hash_table, 3);

static int usb_match_device(struct usb_device *dev, const struct usb_device_id *id)
{
	if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
		return 0;

	/* No need to test id->bcdDevice_lo != 0, since 0 is never */
	/*   greater than any unsigned number. */
	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
		(id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
		(id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
		(id->bDeviceClass != dev->descriptor.bDeviceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
		(id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
		(id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
		return 0;

	return 1;
}

static u32 usb_detect_static_quirks(struct usb_device *udev,
					const struct usb_device_id *id)
{
	u32 quirks = 0;

	for (; id->match_flags; id++) {
		if (!usb_match_device(udev, id))
			continue;

		quirks |= (u32)(id->driver_info);
		dev_info(&udev->dev,
			  "Set usbcore quirk_flags 0x%x for device %04x:%04x\n",
			  (u32)id->driver_info, id->idVendor,
			  id->idProduct);
	}

	return quirks;
}

void xhci_mtk_init_snd_quirk(struct snd_usb_audio *chip)
{
	const struct usb_audio_quirk_flags_table *p;

	for (p = mtk_snd_quirk_flags_table; p->id; p++) {
		if (chip->usb_id == p->id ||
			(!USB_ID_PRODUCT(p->id) &&
			 USB_ID_VENDOR(chip->usb_id) == USB_ID_VENDOR(p->id))) {
			dev_info(&chip->dev->dev,
					  "Set audio quirk_flags 0x%x for device %04x:%04x\n",
					  p->flags, USB_ID_VENDOR(chip->usb_id),
					  USB_ID_PRODUCT(chip->usb_id));
			chip->quirk_flags |= p->flags;
			return;
		}
	}
}

/* update mtk usbcore quirk */
void xhci_mtk_apply_quirk(struct usb_device *udev)
{
	if (!udev)
		return;

	udev->quirks = usb_detect_static_quirks(udev, mtk_usb_quirk_list);
}

static void xhci_mtk_usb_clear_packet_size_quirk(struct urb *urb)
{
	struct device *dev = &urb->dev->dev;
	struct usb_ctrlrequest *ctrl = NULL;
	struct snd_usb_audio *chip;
	struct snd_usb_endpoint *ep, *en;
	struct snd_urb_ctx *ctx;
	unsigned int i, j;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != USB_REQ_SET_INTERFACE || ctrl->wValue == 0)
		return;

	chip = usb_get_intfdata(to_usb_interface(dev));
	if (!chip)
		return;

	if (!(chip->quirk_flags & QUIRK_FLAG_PLAYBACK_FIRST))
		return;

	dev_info(dev, "%s clear urb ctx packet_size\n", __func__);

	list_for_each_entry_safe(ep, en, &chip->ep_list, list) {
		for (i = 0; i < MAX_URBS; i++) {
			ctx = &ep->urb[i];
			if (!ctx)
				continue;
			/* set default urb ctx packet_size */
			for (j = 0; j < MAX_PACKS_HS; j++)
				ctx->packet_size[j] = 0;
		}
	}
}

static void xhci_mtk_usb_set_interface_quirk(struct urb *urb)
{
	struct device *dev = &urb->dev->dev;
	struct usb_ctrlrequest *ctrl = NULL;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != USB_REQ_SET_INTERFACE || ctrl->wValue == 0)
		return;

	dev_dbg(dev, "delay 5ms for UAC device\n");
	mdelay(5);
}

static void xhci_mtk_usb_set_sample_rate_quirk(struct urb *urb)
{
	struct device *dev = &urb->dev->dev;
	struct usb_ctrlrequest *ctrl = NULL;
	struct snd_usb_audio *chip;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != UAC_SET_CUR || ctrl->wValue == 0)
		return;

	chip = usb_get_intfdata(to_usb_interface(dev));
	if (!chip)
		return;

	if (chip->usb_id == USB_ID(0x2717, 0x3801)) {
		dev_dbg(dev, "delay 50ms after set sample rate\n");
		mdelay(50);
	}
}

static bool xhci_mtk_is_usb_audio(struct urb *urb)
{
	struct usb_host_config *config = NULL;
	struct usb_interface_descriptor *intf_desc = NULL;
	int config_num, i;

	config = urb->dev->config;
	if (!config)
		return false;
	config_num = urb->dev->descriptor.bNumConfigurations;

	for (i = 0; i < config_num; i++, config++) {
		if (config && config->desc.bNumInterfaces > 0)
			intf_desc = &config->intf_cache[0]->altsetting->desc;
		if (intf_desc && intf_desc->bInterfaceClass == USB_CLASS_AUDIO)
			return true;
	}

	return false;
}

int register_xhci_enum_mbrain_cb(xhci_enum_mbrain_callback cb)
{
	if (!cb)
		return -EINVAL;

	xhci_enum_mbrain_cb = cb;
	return 0;
}
EXPORT_SYMBOL(register_xhci_enum_mbrain_cb);

int unregister_xhci_enum_mbrain_cb(void)
{
	xhci_enum_mbrain_cb = NULL ;
	return 0;
}
EXPORT_SYMBOL(unregister_xhci_enum_mbrain_cb);

static struct xhci_mbrain_hash_node *xhci_mtk_mbrain_get_hash_node(struct usb_device *udev)
{
	struct xhci_mbrain_hash_node *item;
	const char *key = dev_name(&udev->dev);
	unsigned int hash_key = full_name_hash(NULL, key, strlen(key));
	char *dev_name_backup;

	hash_for_each_possible(mbrain_hash_table, item, node, hash_key) {
		if (strcmp(item->dev_name, key) == 0) {
			// dev_dbg(&udev->dev, "mbrain: use the exist node: mbrain_data=0x%p\n", &item->mbrain_data);

			if (udev->state == USB_STATE_DEFAULT) {
				dev_name_backup = item->dev_name;
				memset(item, 0x00, sizeof(struct xhci_mbrain_hash_node));
				item->dev_name = dev_name_backup;
			}
			return item;
		}
	}

	item = kzalloc(sizeof(struct xhci_mbrain_hash_node), GFP_ATOMIC);
	item->dev_name = kstrdup(key, GFP_ATOMIC);
	dev_info(&udev->dev, "mbrain: allocate new node: mbrain_data=0x%p\n", &item->mbrain_data);
	hash_add(mbrain_hash_table, &item->node, hash_key);
	return item;
}

static void xhci_mtk_mbrain_action(struct urb *urb)
{
	if (urb->setup_packet) {
		struct usb_device *udev = urb->dev;
		u16 bcdDevice = le16_to_cpu(udev->descriptor.bcdDevice);
		struct xhci_mbrain_hash_node *hash_node;
		struct xhci_mbrain *mbrain_data;

		hash_node = xhci_mtk_mbrain_get_hash_node(udev);
		mbrain_data = &hash_node->mbrain_data;
		if (hash_node->updated_db)
			return;

		mbrain_data->vid = le16_to_cpu(udev->descriptor.idVendor);
		mbrain_data->pid = le16_to_cpu(udev->descriptor.idProduct);
		mbrain_data->bcd = bcdDevice;
		mbrain_data->speed= udev->speed;

		if (mbrain_data->state != udev->state) {
			mbrain_data->state = udev->state;
			hash_node->jiffies = jiffies;
			dev_info(&udev->dev,
				"mbrain: idVendor=%04x, idProduct=%04x, bcdDevice=%2x.%02x, speed=%d, state=%d\n",
					mbrain_data->vid, mbrain_data->pid,
					mbrain_data->bcd >> 8, mbrain_data->bcd & 0xff,
					mbrain_data->speed, mbrain_data->state
				);
		}

		if (mbrain_data->state == USB_STATE_CONFIGURED || time_after(jiffies, hash_node->jiffies + 2*HZ)) {
			hash_node->updated_db = true;
			dev_info(&udev->dev, "mbrain: configured or already stay in state(%d) over 2s and try to update mbrain db\n",
									mbrain_data->state);
			if (xhci_enum_mbrain_cb) {
				xhci_enum_mbrain_cb(*mbrain_data);
			}
		}
	}


}

static void xhci_mtk_mbrain_init(struct device *dev)
{
	hash_init(mbrain_hash_table);
}

static void xhci_mtk_mbrain_cleanup(struct device *dev)
{
	struct xhci_mbrain_hash_node *item;
	struct hlist_node *tmp;
	int bkt;

	dev_info(dev, "mbrain: cleanup hash\n");
	hash_for_each_safe(mbrain_hash_table, bkt, tmp, item, node) {
		hash_del(&item->node);
		kfree(item->dev_name);
		kfree(item);
	}
}

static void xhci_trace_ep_urb_enqueue(void *data, struct urb *urb)
{
	if (!urb || !urb->setup_packet || !urb->dev)
		return;

	if (xhci_mtk_is_usb_audio(urb)) {
		/* apply clear packet size */
		xhci_mtk_usb_clear_packet_size_quirk(urb);
		/* apply set interface face delay */
		xhci_mtk_usb_set_interface_quirk(urb);
	}
}

static void xhci_trace_ep_urb_giveback(void *data, struct urb *urb)
{
	if (!urb || !urb->setup_packet || !urb->dev)
		return;

	if (xhci_mtk_is_usb_audio(urb)) {
		/* apply set sample rate delay */
		xhci_mtk_usb_set_sample_rate_quirk(urb);
	}

	xhci_mtk_mbrain_action(urb);
}

void xhci_mtk_trace_init(struct device *dev)
{
	WARN_ON(register_trace_xhci_urb_enqueue_(xhci_trace_ep_urb_enqueue, dev));
	WARN_ON(register_trace_xhci_urb_giveback_(xhci_trace_ep_urb_giveback, dev));

	xhci_mtk_mbrain_init(dev);
}

void xhci_mtk_trace_deinit(struct device *dev)
{
	WARN_ON(unregister_trace_xhci_urb_enqueue_(xhci_trace_ep_urb_enqueue, dev));
	WARN_ON(unregister_trace_xhci_urb_giveback_(xhci_trace_ep_urb_giveback, dev));

	xhci_mtk_mbrain_cleanup(dev);
}
