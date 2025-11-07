// SPDX-License-Identifier: GPL-2.0
/*
 * MTK xhci quirk driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Denis Hsu <denis.hsu@mediatek.com>
 */
#include <linux/platform_device.h>
#include <linux/usb/audio.h>
#include <linux/usb/quirks.h>
#include <linux/spinlock.h>
#include <linux/stringhash.h>
#include "quirks.h"
#include "xhci-mtk.h"
#include "xhci-trace.h"

#include <sound/asound.h>
#include <sound/core.h>
#include "card.h"

struct usb_audio_quirk_flags_table {
	u32 id;
	u32 flags;
};

static struct snd_usb_audio *usb_chip[SNDRV_CARDS];

#define DEVICE_FLG(vid, pid, _flags) \
	{ .id = USB_ID(vid, pid), .flags = (_flags) }
#define VENDOR_FLG(vid, _flags) DEVICE_FLG(vid, 0, _flags)

static DEFINE_SPINLOCK(lock_mbrain_update_db);

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
		DEVICE_FLG(0x22d9, 0x9101,	/* MH147 */
		   QUIRK_FLAG_CTL_MSG_DELAY_5M),
		/* Vendor matches */
		VENDOR_FLG(0x2fc6,		/* Comtrue Devices */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		{} /* terminator */
};


static xhci_enum_mbrain_callback xhci_enum_mbrain_cb;

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

static void xhci_mtk_usb_free_format(struct audioformat *fp)
{
	list_del(&fp->list);
	kfree(fp->rate_table);
	kfree(fp->chmap);
	kfree(fp);
}

static void xhci_mtk_usb_format_quirk(struct snd_usb_audio *chip)
{
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs;
	struct audioformat *fp, *n;

	/* Restrict the playback format for bestechnic audio device */
	if (chip->usb_id == USB_ID(0xbe57, 0x0238)) {
		dev_info(&chip->dev->dev, "Restrict the playback format to 16 bits\n");
		/* list all streams */
		list_for_each_entry(as, &chip->pcm_list, list) {
			subs = &as->substream[SNDRV_PCM_STREAM_PLAYBACK];
			/* check if the stream is initialized */
			if (subs->num_formats) {
				/* check and remove the unsupported format from the list */
				list_for_each_entry_safe(fp, n, &subs->fmt_list, list) {
					if (fp->fmt_bits > 16) {
						subs->num_formats--;
						subs->formats &= ~(fp->formats);
						xhci_mtk_usb_free_format(fp);
					}
				}
			}
		}
	}
}

void xhci_mtk_init_snd_quirk(struct snd_usb_audio *chip)
{
	const struct usb_audio_quirk_flags_table *p;

	if (chip->index >= 0 && chip->index <SNDRV_CARDS)
		usb_chip[chip->index] = chip;

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

	xhci_mtk_usb_format_quirk(chip);
}

void xhci_mtk_deinit_snd_quirk(struct snd_usb_audio *chip)
{
	if (chip->index >= 0 && chip->index <SNDRV_CARDS)
		usb_chip[chip->index] = NULL;
}

static bool xhci_mtk_is_valid_uac_dev(struct usb_device *udev)
{
	int i;

	for (i = 0; i < SNDRV_CARDS; i++) {
		if (usb_chip[i] && usb_chip[i]->dev == udev)
			return true;
	}
	return false;
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
	struct usb_device *udev = urb->dev;
	struct device *dev = &udev->dev;
	struct usb_ctrlrequest *ctrl = NULL;
	struct usb_interface *iface = NULL;
	struct usb_host_interface *alt = NULL;
	struct snd_usb_audio *chip;
	struct snd_usb_endpoint *ep, *en;
	struct snd_urb_ctx *ctx;
	unsigned int i, j;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != USB_REQ_SET_INTERFACE || ctrl->wValue == 0)
		return;

	iface = usb_ifnum_to_if(urb->dev, ctrl->wIndex);
	if (!iface)
		return;

	alt = usb_altnum_to_altsetting(iface, ctrl->wValue);
	if (!alt)
		return;

	if (alt->desc.bInterfaceClass != USB_CLASS_AUDIO)
		return;

	if (!xhci_mtk_is_valid_uac_dev(udev))
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
	struct usb_interface *iface = NULL;
	struct usb_host_interface *alt = NULL;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != USB_REQ_SET_INTERFACE || ctrl->wValue == 0)
		return;

	iface = usb_ifnum_to_if(urb->dev, ctrl->wIndex);
	if (!iface)
		return;

	alt = usb_altnum_to_altsetting(iface, ctrl->wValue);
	if (!alt)
		return;

	if (alt->desc.bInterfaceClass != USB_CLASS_AUDIO)
		return;

	dev_dbg(dev, "delay 5ms for UAC device\n");
	mdelay(5);
}

static void xhci_mtk_usb_set_sample_rate_quirk(struct urb *urb)
{
	struct usb_device *udev = urb->dev;
	struct device *dev = &udev->dev;
	struct usb_ctrlrequest *ctrl = NULL;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != UAC_SET_CUR || ctrl->wValue == 0)
		return;

	if (le16_to_cpu(udev->descriptor.idVendor) == 0x2717 &&
			le16_to_cpu(udev->descriptor.idProduct) == 0x3801) {
		dev_info(dev, "delay 50ms after set sample rate\n");
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

static void xhci_update_mbrain_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct xhci_mbrain_hash_node *item = container_of(delayed_work, struct xhci_mbrain_hash_node, updated_db_work);

	spin_lock(&lock_mbrain_update_db);
	if (!item->updated_db) {
		item->updated_db = true;
		if (item->updated_db_work_delay)
			pr_info("usb %s: mbrain: already stay in state(%d) over %d ms and try to update mbrain db\n",
				item->dev_name, item->mbrain_data.state, XHCI_MBRAIN_STATE_TIMEOUT_MS);
		else
			pr_info("usb %s: mbrain: already stay in state(%d) and try to update mbrain db\n",
				item->dev_name, item->mbrain_data.state);

		if (xhci_enum_mbrain_cb)
			xhci_enum_mbrain_cb(item->mbrain_data);
	}
	spin_unlock(&lock_mbrain_update_db);
}

static struct xhci_mbrain_hash_node *xhci_mtk_mbrain_get_hash_node(struct usb_device *udev)
{
	struct xhci_hcd_mtk *mtk = NULL;
	struct usb_hcd *hcd;
	struct xhci_mbrain_hash_node *item;
	const char *key = dev_name(&udev->dev);
	unsigned int hash_key = full_name_hash(NULL, key, strlen(key));

	hcd = bus_to_hcd(udev->bus);
	mtk = hcd_to_mtk(hcd);
	hash_for_each_possible(mtk->mbrain_hash_table, item, node, hash_key) {
		if (strcmp(item->dev_name, key) == 0) {
			// dev_dbg(&udev->dev, "mbrain: use the exist node: mbrain_data=0x%p\n", &item->mbrain_data);

			if (udev->state == USB_STATE_DEFAULT) {
				memset(&item->mbrain_data, 0x00, sizeof(struct xhci_mbrain));
				item->updated_db = false;
			}
			return item;
		}
	}

	item = kzalloc(sizeof(struct xhci_mbrain_hash_node), GFP_ATOMIC);
	if (!item)
		return NULL;

	item->dev_name = kstrdup(key, GFP_ATOMIC);
	INIT_DELAYED_WORK(&item->updated_db_work, xhci_update_mbrain_work);
	dev_info(&udev->dev, "mbrain: allocate new node: mbrain_data=0x%p\n", &item->mbrain_data);
	hash_add(mtk->mbrain_hash_table, &item->node, hash_key);
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
		if (!hash_node || hash_node->updated_db)
			return;

		mbrain_data = &hash_node->mbrain_data;
		mbrain_data->vid = le16_to_cpu(udev->descriptor.idVendor);
		mbrain_data->pid = le16_to_cpu(udev->descriptor.idProduct);
		mbrain_data->bcd = bcdDevice;
		mbrain_data->speed= udev->speed;

		if (mbrain_data->state != udev->state) {
			mbrain_data->state = udev->state;
			dev_info(&udev->dev,
				"mbrain: idVendor=%04x, idProduct=%04x, bcdDevice=%2x.%02x, speed=%d, state=%d\n",
					mbrain_data->vid, mbrain_data->pid,
					mbrain_data->bcd >> 8, mbrain_data->bcd & 0xff,
					mbrain_data->speed, mbrain_data->state
				);

			if (mbrain_data->state == USB_STATE_CONFIGURED)
				hash_node->updated_db_work_delay = 0;

			else
				hash_node->updated_db_work_delay = XHCI_MBRAIN_STATE_TIMEOUT_MS;

			mod_delayed_work(system_wq, &hash_node->updated_db_work,
				msecs_to_jiffies(hash_node->updated_db_work_delay));
		}
	}
}

static void xhci_mtk_mbrain_init(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xhci_hcd_mtk *mtk = platform_get_drvdata(pdev);

	hash_init(mtk->mbrain_hash_table);
}

static void xhci_mtk_mbrain_cleanup(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xhci_hcd_mtk *mtk = platform_get_drvdata(pdev);
	struct xhci_mbrain_hash_node *item;
	struct hlist_node *tmp;
	int bkt;

	dev_info(dev, "mbrain: cleanup hash\n");
	hash_for_each_safe(mtk->mbrain_hash_table, bkt, tmp, item, node) {
		cancel_delayed_work_sync(&item->updated_db_work);
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
