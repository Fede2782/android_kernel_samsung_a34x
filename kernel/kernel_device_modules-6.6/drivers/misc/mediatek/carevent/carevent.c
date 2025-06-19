// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/connector.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <net/sock.h>

#include "carevent.h"

static u32 get_status(u32 gpio_num);
static int report_car_sleep_status(u32 portid);
static int send_car_sleep_status(u32 portid, u32 pinval);

static struct sock *nl_sk;
static u32 target_pids[PIDS_NR] = {0};

static u8 has_reverse;

static int car_reverse_gpio;
static u32 car_reverse_irq;
static u32 car_reverse_gpio_value;
static u32 car_reverse_debounce;
static u32 car_reverse_status = CAR_STATUS_UNKNOWN;
static struct hrtimer reverse_gpio_stable_timer;
static int reverse_gpio_timer_time = 1; /* 1s */
static u32 reverse_gpio_timer_running;

static u32 sleep_target_pid;
static u32 car_switch_irq;
static int car_switch_gpio;
static u32 car_switch_debounce;
static u32 car_sleep_status = CAR_SLEEP_STATUS_UNKNOWN;
static struct hrtimer sleep_gpio_stable_timer;
static int sleep_gpio_timer_time = 1; /* 1s */
static u32 sleep_gpio_timer_running;
static u32 car_switch_gpio_value;

static struct wakeup_source *switch_irq_lock;

static struct workqueue_struct *send_status_wq;
static struct work_struct sleep_stable_work;
static struct work_struct reverse_stable_work;
static struct work_struct switch_on_work;
static struct work_struct reverse_work;

static u32 get_status(u32 gpio_num)
{
	return gpio_get_value(gpio_num);
}

static int send_response(u32 portid, struct car_status *pStatus)
{
	void *data;
	int ret;
	struct nlmsghdr *nlh;
	struct sk_buff *msg_skb;

	pr_info("[carevent] send %d response: %d, value: %d\n", portid,
		pStatus->response_type, pStatus->status);

	msg_skb = nlmsg_new(sizeof(struct car_status), GFP_KERNEL);
	if (!msg_skb)
		return -ENOMEM;

	nlh = nlmsg_put(msg_skb, portid, 0, 0, sizeof(struct car_status), 0);
	if (!nlh) {
		nlmsg_free(msg_skb);
		return -ENOMEM;
	}

	data = nlmsg_data(nlh);

	memcpy(data, pStatus, sizeof(struct car_status));
	ret = nlmsg_unicast(nl_sk, msg_skb, portid);
	if (ret)
		pr_info("[carevent] failed to send msg!\n");

	/* During the netlink transfer, it would be kernel panic if sender delete
	 * the skb buffer after nlmsg_unicast() function call because the receiver
	 * might receive the NULL data. Therefore, Linux kernel will take care of the
	 * skb buffer release after the receiver received the skb buffer data. sender
	 * just allocation the new skb buffer and send the buffer to the receiver.
	 */
	/* nlmsg_free(msg_skb); */
	return ret;
}


static int send_car_reverse_status(u32 portid, u32 pinval)
{
	struct car_status status;
	int result = 0;

	status.response_type = EVENT_CAR_REVERSE;
	status.status = (pinval ? CAR_STATUS_REVERSE : CAR_STATUS_NORMAL);

	result = send_response(portid, &status);

	return result;
}

static int send_car_sleep_status(u32 portid, u32 pinval)
{
	struct car_status status;
	int result = 0;

	status.response_type = EVENT_CAR_SLEEP;
	status.status = (pinval ? CAR_STATUS_SLEEP : CAR_STATUS_WAKEUP);

	result = send_response(portid, &status);
	if (portid == sleep_target_pid)
		/* save status when send to smartplatform server */
		car_sleep_status = pinval;
	return result;
}

static void  sleep_gpio_stable_worker(struct work_struct *work)
{
	u32 pin_val = get_status(car_switch_gpio);

	if (car_sleep_status == pin_val)
		pr_info("[carevent] previous status is the same with the current(%d)\n",
			pin_val);
	else
		send_car_sleep_status(sleep_target_pid, pin_val);

	pr_info("[carevent] sleep gpio stable worker\n");
	sleep_gpio_timer_running = 0;
}


static enum hrtimer_restart
sleep_gpio_stable_hrtimer_func(struct hrtimer *timer)
{
	int ret;

	pr_info("[carevent] sleep_gpio_stable_timer timeout\n");

	ret = queue_work(send_status_wq, &sleep_stable_work);
	if (!ret)
		pr_info("[carevent]  sleep stable work was already on a queue\n");

	return HRTIMER_NORESTART;
}

static void reverse_gpio_stable_worker(struct work_struct *work)
{
	u32 pin_val = get_status(car_reverse_gpio);
	int i;

	if (car_reverse_status == pin_val)
		pr_info("[carevent] previous status is the same with the current(%d)\n",
			pin_val);
	else {
		for (i = 0; i < PIDS_NR; i++) {
			if (target_pids[i] > 0)
				send_car_reverse_status(target_pids[i],
							pin_val);
		}
		car_reverse_status = pin_val;
		pr_info("[carevent] reverse gpio stable worker\n");
	}

	reverse_gpio_timer_running = 0;
}

static enum hrtimer_restart
reverse_gpio_stable_hrtimer_func(struct hrtimer *timer)
{
	int ret;

	pr_info("[carevent] reverse_gpio_stable_timer timeout\n");

	ret = queue_work(send_status_wq, &reverse_stable_work);
	if (!ret)
		pr_info("[carevent]  reverse stable work was already on a queue\n");

	return HRTIMER_NORESTART;
}

static void car_reverse_worker(struct work_struct *work)
{
	u32 pin_val = get_status(car_reverse_gpio);
	int i;

	for (i = 0; i < PIDS_NR; i++) {
		if (target_pids[i] > 0)
			send_car_reverse_status(target_pids[i], pin_val);
	}

	car_reverse_status = pin_val;

	pr_info("[carevent] car reverse worker\n");
}


static irqreturn_t car_reverse_isr(int irq, void *d)
{
	u32 pin_val = get_status(car_reverse_gpio);

	pr_info("[carevent] car reverse isr, pin_val = %u\n", pin_val);

	if (pin_val)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);

	gpiod_set_debounce(gpio_to_desc(car_reverse_gpio), car_reverse_debounce);

	if (reverse_gpio_timer_running == 0) {
		if (car_reverse_status == pin_val) {
			pr_info("[carevent] car_reverse_status is same with pin_val.\n");
			hrtimer_start(&reverse_gpio_stable_timer,
				      ktime_set(0, 100 * 1000 * 1000),
				      HRTIMER_MODE_REL);
		} else {
			queue_work(send_status_wq, &reverse_work);
			pr_info("[carevent] start reverse_gpio_stable_timer\n");
			hrtimer_start(&reverse_gpio_stable_timer,
				ktime_set(reverse_gpio_timer_time, 0),
				HRTIMER_MODE_REL);
		}
		reverse_gpio_timer_running = 1;
	}

	return IRQ_HANDLED;
}


static void car_switch_on_worker(struct work_struct *work)
{
	u32 pin_val;

	pin_val = get_status(car_switch_gpio);
	send_car_sleep_status(sleep_target_pid, pin_val);

	pr_info("[carevent] car switch on worker\n");
}

static irqreturn_t car_switch_on_isr(int irq, void *d)
{
	u32 pin_val;

	__pm_stay_awake(switch_irq_lock);

	pin_val = get_status(car_switch_gpio);

	pr_info("[carevent] car switch isr, pin_val = %u ,target pid = %u\n",
		pin_val, sleep_target_pid);

	if (pin_val)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);

	gpiod_set_debounce(gpio_to_desc(car_switch_gpio), car_switch_debounce);


	if (sleep_target_pid) {
		if (sleep_gpio_timer_running == 0) {
			if (car_sleep_status == pin_val) {
				pr_info("[carevent] car_sleep_status is same with pin_val.\n");
				hrtimer_start(&sleep_gpio_stable_timer,
					      ktime_set(0, 100 * 1000 * 1000),
					      HRTIMER_MODE_REL);
			} else {
				queue_work(send_status_wq, &switch_on_work);
				/* hrtimer_cancel(&sleep_gpio_stable_timer); */
				pr_info("[carevent] start sleep_gpio_stable_timer\n");
				hrtimer_start(&sleep_gpio_stable_timer,
					ktime_set(sleep_gpio_timer_time, 0),
					HRTIMER_MODE_REL);
			}
			sleep_gpio_timer_running = 1;
		}
	}

	__pm_relax(switch_irq_lock);

	return IRQ_HANDLED;
}

static int start_montior(void)
{
	int ret;
	unsigned long flags;
	u32 pin_val;

	pin_val = get_status(car_switch_gpio);

	if (pin_val)
		flags = IRQ_TYPE_LEVEL_LOW | IRQF_ONESHOT;
	else
		flags = IRQ_TYPE_LEVEL_HIGH | IRQF_ONESHOT;

	ret = request_threaded_irq(car_switch_irq, NULL, car_switch_on_isr,
				   flags, "car_switch", NULL);
	if (ret)
		pr_info("failed to request car switch on irq %u, return %d\n",
			car_switch_irq, ret);

	enable_irq_wake(car_switch_irq);

	if (has_reverse) {
		pin_val = get_status(car_reverse_gpio);

		if (pin_val)
			flags = IRQ_TYPE_LEVEL_LOW | IRQF_ONESHOT;
		else
			flags = IRQ_TYPE_LEVEL_HIGH | IRQF_ONESHOT;

		ret = request_threaded_irq(car_reverse_irq, NULL, car_reverse_isr,
					flags, "car_reverse", NULL);
		if (ret)
			pr_info("failed to request car reverse on irq %u, return %d\n",
				car_reverse_irq, ret);
	}

	return ret;
}

static int stop_report(void)
{

	free_irq(car_switch_irq, NULL);

	if (has_reverse)
		free_irq(car_reverse_irq, NULL);
	return 0;
}

static int report_car_reverse_status(u32 portid)
{
	u32 pin_val = get_status(car_reverse_gpio);

	send_car_reverse_status(portid, pin_val);

	return 0;
}


static int report_car_sleep_status(u32 portid)
{
	u32 pin_val = get_status(car_switch_gpio);

	send_car_sleep_status(portid, pin_val);

	return 0;
}


static void handle_input(struct sk_buff *skb)
{

	struct nlmsghdr *nlh = NULL;
	int i;

	if (skb->len >= nlmsg_total_size(0)) {
		nlh = nlmsg_hdr(skb);
		switch (nlh->nlmsg_type) {
		case CAR_REVERSE_REQ_STATUS:
			pr_info("[carevent]reverse req status pid %d\n",
				nlh->nlmsg_pid);
			if (has_reverse)
				report_car_reverse_status(nlh->nlmsg_pid);
			break;
		case CAR_REVERSE_SET_RCV:
			pr_info("[carevent]reverse set rev pid %d\n",
				nlh->nlmsg_pid);
			for (i = 0; i < PIDS_NR; i++) {
				if (target_pids[i] == 0) {
					target_pids[i] = nlh->nlmsg_pid;
					break;
				}
			}
			break;
		case CAR_SLEEP_SET_RCV:
			pr_info("[carevent]sleep set rev pid %d\n",
				nlh->nlmsg_pid);
			sleep_target_pid = nlh->nlmsg_pid;
			break;

		case CAR_SLEEP_REQ_STATUS:
			pr_info("[carevent]sleep req status pid %d\n",
				nlh->nlmsg_pid);
			report_car_sleep_status(nlh->nlmsg_pid);
			break;
		case CAR_REVERSE_REMOVE_RCV:
			pr_info("[carevent]remove rev pid %d\n",
				nlh->nlmsg_pid);
			for (i = 0; i < PIDS_NR; i++) {
				if (nlh->nlmsg_pid == target_pids[i]) {
					target_pids[i] =
						0; /* set pid 0 mean remove */
					break;
				}
			}
			break;
		default:
			pr_info("[carevent]warnning unknown msg! %u\n", nlh->nlmsg_type);
		}
	}
}

int carevent_probe(struct platform_device *pdev)
{

	struct device_node *np = pdev->dev.of_node;
	struct netlink_kernel_cfg cfg = {
		.input = handle_input,
	};
	int ret = 0;

	has_reverse = 0;

	car_switch_gpio = of_get_named_gpio(np, "carevent-gpio", 0);
	if (car_switch_gpio < 0) {
		pr_info("[carevent]Can't find switch-gpio\n");
		return car_switch_gpio;
	}

	ret = gpio_request(car_switch_gpio, "switch-gpio");
	if (ret) {
		pr_info("[carevent]Can't request switch-gpio\n");
		return ret;
	}

	ret = gpio_direction_input(car_switch_gpio);
	if (ret) {
		pr_info("[carevent]set switch-gpio input failed\n");
		return ret;
	}

	car_switch_gpio_value = get_status(car_switch_gpio);
	pr_info("[carevent]car_switch_gpio_value: %u,car_switch_gpio: %d\n",
		car_switch_gpio_value, car_switch_gpio);

	ret = of_property_read_u32(np, "debounce", &car_switch_debounce);

	pr_info("[carevent]car_switch_debounce: %u\n",
		car_switch_debounce);

	car_switch_irq = irq_of_parse_and_map(np, 0);
	if (!car_switch_irq) {
		pr_info("[carevent]failed to get irq num! %u\n",
			car_switch_irq);
		return -ENODEV;
	}

	pr_info("[carevent]car_switch_irq: %u\n",
		car_switch_irq);

	car_reverse_gpio = of_get_named_gpio(np, "carevent-gpio", 1);
	if (car_reverse_gpio < 0)
		pr_info("[carevent]reverse-gpio is not set\n");
	else
		has_reverse = 1;

	if (has_reverse) {
		ret = gpio_request(car_reverse_gpio, "reverse-gpio");
		if (ret) {
			pr_info("[carevent]Can't request reverse-gpio\n");
			return ret;
		}

		ret = gpio_direction_input(car_reverse_gpio);
		if (ret) {
			pr_info("[carevent]set reverse-gpio input failed\n");
			return ret;
		}

		car_reverse_gpio_value = get_status(car_reverse_gpio);
		pr_info("[carevent]car_reverse_gpio_value: %u,car_reverse_gpio:%d\n",
			car_reverse_gpio_value, car_reverse_gpio);

		car_reverse_irq = irq_of_parse_and_map(np, 1);
		if (!car_reverse_irq) {
			pr_info("[carevent]failed to get irq num! %u\n",
				car_reverse_irq);
			return -ENODEV;
		}

		pr_info("[carevent]car_reverse_irq: %u\n",
			car_reverse_irq);

		ret = of_property_read_u32(np, "debounce", &car_reverse_debounce);

		pr_info("[carevent]car_reverse_debounce: %u\n",
			car_reverse_debounce);
	}

	nl_sk = netlink_kernel_create(&init_net, NETLINK_CAR_EVENT, &cfg);
	if (!nl_sk) {
		pr_info("[carevent]car event create netlink failed!\n");
		return -EIO;
	}

	hrtimer_init(&sleep_gpio_stable_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	sleep_gpio_stable_timer.function = sleep_gpio_stable_hrtimer_func;

	if (has_reverse) {
		hrtimer_init(&reverse_gpio_stable_timer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
		reverse_gpio_stable_timer.function = reverse_gpio_stable_hrtimer_func;
	}

	switch_irq_lock = wakeup_source_register(NULL, "carevent_acc_irq_lock");

	send_status_wq = alloc_workqueue("carevent_send_status",
								WQ_UNBOUND | WQ_HIGHPRI, 1);
	if (!send_status_wq) {
		pr_info("[carevent] failed to create worker thread\n");
		return -ENOMEM;
	}

	INIT_WORK(&switch_on_work, car_switch_on_worker);
	INIT_WORK(&sleep_stable_work, sleep_gpio_stable_worker);
	INIT_WORK(&reverse_stable_work, reverse_gpio_stable_worker);
	INIT_WORK(&reverse_work, car_reverse_worker);

	return start_montior();
}

static const struct of_device_id carevent_of_match[] = {
	{
		.compatible = "mediatek,spm-carevent",
	},
	{},
};

static struct platform_driver mtk_carevent_driver = {
	.probe = carevent_probe,
	/* .suspend = carevent_suspend, */
	/* .resume = carevent_resume, */
	/*.remove = carevent_remove, */
	.driver = {
			.name = "Carevent_Driver",
			.of_match_table = carevent_of_match,
		},
};

static int __init car_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_carevent_driver);
	if (ret)
		pr_info("[carevent]platform_driver_register error:(%d)\n", ret);
	else
		pr_info("[carevent]platform_driver_register done!\n");

	return ret;
}

static void __exit car_exit(void)
{
	stop_report();

	if (nl_sk)
		netlink_kernel_release(nl_sk);

	flush_workqueue(send_status_wq);
	destroy_workqueue(send_status_wq);
}
module_init(car_init);
module_exit(car_exit);


MODULE_DESCRIPTION("MEDIATEK Module Car Event Detect Driver");
MODULE_AUTHOR("Lei.Huang<Lei.Huang@mediatek.com>");
MODULE_LICENSE("GPL");
