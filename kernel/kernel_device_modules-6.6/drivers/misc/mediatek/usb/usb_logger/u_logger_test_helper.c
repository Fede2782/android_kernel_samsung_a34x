// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Test Helper
 * *
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/string.h>
#include "u_logger.h"
#include "xhci-trace.h"

#if IS_ENABLED(CONFIG_MTK_USB_OFFLOAD)
#include "usb_offload_trace.h"
#endif

#define TESTER_NAME         "usb_tester"
#define TEST_CMD_NAME       "command"
#define RESULT_CMD_NAME     "result"
#define MAX_TRACE_ISR       5
#define MAX_RESULT_STR      100
#define MAX_CMD_NUM         20

struct xhci_isr {
	u16 slot;
	u16 ep;
	u8 dir;
	u16 class;
	u32 cnt;
};

struct u_tester {
	struct device *dev;
	struct device_attribute test_attr;
	char output[MAX_RESULT_STR];
	struct device_attribute result_attr;
	struct xhci_isr done[MAX_TRACE_ISR];
	u16 in_use;
	atomic_t running;
	atomic_t trace_xhci_irq;
} tester;

static inline void check_output_end(int cnt)
{
	if (cnt > 0 && cnt < MAX_RESULT_STR)
		tester.output[cnt] = '\0';
	else if (cnt == 0)
		dev_info(tester.dev, "not copy any word\n");
	else
		dev_info(tester.dev, "error:%d from snprintf\n", cnt);
}

#define set_output(fmt, args...) do { \
	cnt = snprintf(tester.output, MAX_RESULT_STR, fmt, ##args);\
	check_output_end(cnt); \
} while (0)

#define set_output_none() do { \
	cnt = snprintf(tester.output, MAX_RESULT_STR, "none");\
	check_output_end(cnt);\
} while (0)

static void reset_xhci_isr(struct xhci_isr *done)
{
	done->slot = 0;
	done->ep = 0;
	done->dir = 0;
	done->class = 0;
	done->cnt = 0;
}

static ssize_t test_cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct xhci_isr *done;
	int i, cnt;
	char cmd[MAX_CMD_NUM];
	char *c = cmd, *name;
	char *status = "error";
	const char * const delim = " \0\n\t";

	/* set output to none first */
	set_output_none();

	/* check if input is valid */
	dev_info(dev, "======%s=====\n", __func__);
	if (count > MAX_CMD_NUM) {
		dev_info(dev, "wrong size (%zu>%d)\n", count, MAX_CMD_NUM);
		goto error;
	}
	strscpy(cmd, buf, sizeof(cmd));
	cmd[count - 1] = '\0';
	dev_info(dev, "input:%s", cmd);
	name = strsep(&c, delim);
	if (!name) {
		dev_info(dev, "wrong input\n");
		goto error;
	}
	dev_info(dev, "@[%zu]name:%s", strlen(name), name);

	/* start parsing input */
	status = "success";
	/* @start: show only */
	if (!strncmp(name, "start", 5)) {
		if (atomic_read(&tester.running))
			dev_info(dev, "tester has already started\n");
		else {
			atomic_set(&tester.running, 1);
			dev_info(dev, "tester start\n");
		}
	/* @stop: show only */
	} else if (!strncmp(name, "stop", 4)) {
		if (!atomic_read(&tester.running))
			dev_info(dev, "tester has already stopped\n");
		else {
			atomic_set(&tester.running, 0);
			dev_info(dev, "tester stop\n");
		}
	/* @reset: show only */
	} else if (!strncmp(name, "reset", 5)) {
		if (!atomic_read(&tester.running)) {
			for (i = 0; i < tester.in_use; i++) {
				done = &tester.done[i];
				dev_info(tester.dev, "delete done[%d]=>slot%d ep%d dir:%d class:%d\n",
					i, done->slot, done->ep, done->dir, done->class);
				reset_xhci_isr(done);
			}
			tester.in_use = 0;
			dev_info(dev, "tester reset\n");
		} else
			dev_info(tester.dev, "please stop tester first\n");
	/* @done_number: return in_use */
	} else if (!strncmp(name, "done_number", 11)) {
		dev_info(dev, "in_use: %d\n", tester.in_use);
		set_output("%d", tester.in_use);
	/* @all_done: return infos of all done */
	} else if (!strncmp(name, "all_done", 8)) {
		cnt = 0;
		for (i = 0; i < tester.in_use; i++) {
			done = &tester.done[i];
			cnt += snprintf(tester.output + cnt, MAX_RESULT_STR - cnt, "%d:%d-%d-%d ",
				i, done->slot, done->ep, done->cnt);
			if (cnt == MAX_RESULT_STR - 1) {
				dev_info(tester.dev, "output reach max:%d\n", MAX_RESULT_STR);
				break;
			}
		}
		check_output_end(cnt);
	/* @all_done: return info of specific index */
	/* [format] result <token1:index>(essential) <toekn2:item>(optional) */
	} else if (!strncmp(name, "result", 6)) {
		struct xhci_isr *done;
		char *token1, *token2;
		int ret;
		long idx;

		/* parsing token1 */
		token1 = strsep(&c, delim);
		if (token1) {
			dev_info(dev, "@[%zu]token1:%s", strlen(token1), token1);
			ret = kstrtol(token1, 0, &idx);
			if (ret != 0) {
				dev_info(dev, "kstrtol ret:%d\n", ret);
				goto error;
			}

			if (idx >= tester.in_use || idx >= MAX_TRACE_ISR) {
				dev_info(dev, "wrong index, idx:%ld in_use:%d\n", idx, tester.in_use);
				goto error;
			}
		} else {
			dev_info(dev, "token should follow result\n");
			goto error;
		}
		done = &tester.done[idx];

		/* parsing token2 */
		token2 = strsep(&c, delim);
		if (token2) {
			/* get partial info(token2) of token1 */
			dev_info(dev, "@[%zu]token2:%s\n", strlen(token2), token2);
			if (!strncmp(token2, "ep", 2))
				set_output("%d", done->ep);
			else if (!strncmp(token2, "slot", 4))
				set_output("%d", done->slot);
			else if (!strncmp(token2, "dir", 3))
				set_output("%d", done->dir);
			else if (!strncmp(token2, "class", 5))
				set_output("%d", done->class);
			else if (!strncmp(token2, "cnt", 3))
				set_output("%d", done->cnt);
			else {
				status = "error";
				dev_info(tester.dev, "unknown token2\n");
			}
		} else
			/* no toekn2, get full info of specific index */
			set_output("slot%d-ep%d-dir%d-class%d-cnt%d",
				done->slot, done->ep, done->dir, done->class, done->cnt);
	/* @enable_trace: show only */
	/* xHCI IRQ here means ap xhci, about adsp one, please refers to usb_offload.ko */
	} else if (!strncmp(name, "enable_trace", 12)) {
		atomic_set(&tester.trace_xhci_irq, 1);
		dev_info(dev, "start tracing xhci irq\n");
	/* @disable_trace: show only */
	} else if (!strncmp(name, "disable_trace", 13)) {
		atomic_set(&tester.trace_xhci_irq, 0);
		dev_info(dev, "stop tracing xhci irq\n");
	} else {
		status = "error";
		dev_info(dev, "unknown command\n");
	}
error:
	dev_info(dev, "@output: %s\n", tester.output);
	dev_info(dev, "status: %s\n", status);
	return count;
}

static ssize_t test_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	dev_info(dev, "======%s=====\n", __func__);
	dev_info(dev, "@output: %s\n", tester.output);

	return sprintf(buf, "%s", tester.output);
}

static int u_tester_create_sysfs(struct u_logger *logger)
{
	struct device_attribute *attr;
	int ret = 0;

	tester.dev = device_create(logger->class, NULL, MKDEV(0, 0), NULL, TESTER_NAME);

	/* test command */
	attr = &tester.test_attr;
	attr->attr.name = TEST_CMD_NAME;
	attr->attr.mode = 0664;
	attr->show = test_cmd_show;
	attr->store = test_cmd_store;
	ret = device_create_file(tester.dev, attr);
	if (ret)
		dev_info(logger->dev, "fail creating file %s\n", TEST_CMD_NAME);
	return ret;
}


static inline bool is_match(u16 slot, u16 ep, int *index)
{
	struct xhci_isr *done;
	int i;

	for (i = 0; i < tester.in_use; i++) {
		done = &tester.done[i];
		dev_dbg(tester.dev, "done[%d]=>slot:%d ep%d dir%d class%d\n",
			i, done->slot, done->ep, done->dir, done->class);
		if (done->slot == slot && done->ep == ep) {
			dev_dbg(tester.dev, "match id:%d for slot%d ep%d\n", i, slot, ep);
			*index = i;
			return true;
		}
	}

	*index = i;
	return false;
}

#if IS_ENABLED(CONFIG_MTK_USB_OFFLOAD)
/* trace interrupters from adsp xhci */
static void usb_offload_monitor_interrupt(void *data, void *stream,
	void *buffer, int length, u16 slot, u16 ep,
	struct usb_endpoint_descriptor *desc)
{
	struct xhci_isr *done;
	int i, idx;
	bool match = false;

	/* enable of adsp xhci irq was in usb_offload.ko
	 * here, we only check if tester were running or not
	 */
	if (!atomic_read(&tester.running))
		return;

	dev_dbg(tester.dev, "======%s slot:%d ep:%d======\n", __func__, slot, ep);

	match = is_match(slot, ep, &i);

	dev_dbg(tester.dev, "%s match:%d i:%d in_use:%d\n", __func__, match, i, tester.in_use);

	if (match) {
		if (i < MAX_TRACE_ISR && i >= 0) {
			if (tester.done[i].cnt + 1 <= UINT_MAX)
				tester.done[i].cnt++;
		} else
			dev_info(tester.dev, "matched exceed MAX_TRACE_ISR\n");
	} else {
		if (tester.in_use + 1 > MAX_TRACE_ISR) {
			dev_dbg(tester.dev, "not enough space for slot%d ep%d\n", slot, ep);
		} else {
			tester.in_use++;
			idx = tester.in_use - 1;
			done = &tester.done[idx];

			/* fix me, consider all interrupts from adsp were UAC */
			done->class = USB_CLASS_AUDIO;
			done->slot = slot;
			done->ep = ep;
			done->dir = usb_endpoint_dir_in(desc);
			done->cnt = 1;
			dev_dbg(tester.dev, "create done[%d]=>slot%d ep%d dir:%d class:%d in_use:%d\n",
				idx, done->slot, done->ep, done->dir, done->class, tester.in_use);
		}
	}
}
#endif

/* trace interrupts from ap xhci */
static void xhci_monitor_interrupt(void *data, struct urb *urb)
{
	struct xhci_isr *done;
	int i;
	bool match = false;
	u16 ep, slot;

	if (!atomic_read(&tester.trace_xhci_irq) || !atomic_read(&tester.running))
		return;

	if (!urb || !urb->dev || urb->setup_packet || !urb->ep)
		return;

	ep = urb->dev->slot_id;
	slot = xhci_get_endpoint_index_(&urb->ep->desc);

	dev_dbg(tester.dev, "======%s slot:%d ep:%d======\n", __func__, slot, ep);

	match = is_match(slot, ep, &i);

	dev_dbg(tester.dev, "%s match:%d i:%d in_use:%d\n", __func__, match, i, tester.in_use);

	if (match) {
		if (i < MAX_TRACE_ISR && i >= 0) {
			if (tester.done[i].cnt + 1 <= UINT_MAX)
				tester.done[i].cnt++;
		} else
			dev_info(tester.dev, "matched exceed MAX_TRACE_ISR\n");
	} else {
		if (tester.in_use + 1 > MAX_TRACE_ISR) {
			dev_dbg(tester.dev, "not enough space for slot%d ep%d\n", slot, ep);
		} else {
			struct usb_host_config *actconfig = NULL;
			struct usb_host_interface *intf;
			int intf_num, i, idx;

			actconfig = urb->dev->actconfig;
			if (!actconfig)
				return;

			tester.in_use++;
			idx = tester.in_use - 1;
			done = &tester.done[idx];
			intf_num = actconfig->desc.bNumInterfaces;
			for (i = 0; i < intf_num; i++) {
				if (!actconfig->interface[i]->cur_altsetting)
					continue;
				intf = actconfig->interface[i]->cur_altsetting;
				if (intf->endpoint == urb->ep)
					done->class = intf->desc.bInterfaceClass;
			}
			done->slot = slot;
			done->ep = ep;
			done->dir = usb_endpoint_dir_in(&urb->ep->desc);
			done->cnt = 1;
			dev_dbg(tester.dev, "create done[%d]=>slot%d ep%d dir:%d class:%d in_use:%d\n",
				idx, done->slot, done->ep, done->dir, done->class, tester.in_use);
		}
	}
}

int u_tester_init(struct u_logger *logger)
{
	int ret = 0;

	ret = u_tester_create_sysfs(logger);
	if (ret) {
		dev_info(logger->dev, "fail creating sysfs\n");
		ret = -EOPNOTSUPP;
		goto exit;
	}

	atomic_set(&tester.running, 0);
	tester.in_use = 0;

	WARN_ON(register_trace_xhci_urb_giveback_(xhci_monitor_interrupt, &tester));
#if IS_ENABLED(CONFIG_MTK_USB_OFFLOAD)
	WARN_ON(register_trace_usb_offload_trace_trigger(
		usb_offload_monitor_interrupt, &tester));
#endif

exit:
	return ret;
}
