// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq_work.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <mt-plat/mtk_irq_mon.h>

#include "internal.h"
/* TIMER_SOFTIRQ duration warning test */

static struct timer_list timer;
static void delayed_timer(struct timer_list *t)
{
	mdelay(600);
}

void irq_mon_test_TIMER_SOFTIRQ(void)
{
	timer_setup(&timer, delayed_timer, 0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(100));
}

/* TASKLET_SOFTIRQ duration warning test */

static struct tasklet_struct irq_mon_tasklet;
static void irq_mon_tasklet_func(unsigned long data)
{
	mdelay(600);
}

void irq_mon_test_TASKLET_SOFTIRQ(void)
{
	tasklet_init(&irq_mon_tasklet, irq_mon_tasklet_func, 0);
	tasklet_schedule(&irq_mon_tasklet);
}

/* RCU_SOFTIRQ duration warning test */

struct irq_mon_rcu_t {
	int val;
};

static struct irq_mon_rcu_t __rcu *irq_mon_rcu_g;
static struct rcu_head irq_mon_rcu_head;
static void delayed_rcu_callback(struct rcu_head *r)
{
	/* RCU_SOFTIRQ handler */
	mdelay(600);
}

void irq_mon_test_RCU_SOFTIRQ(void)
{
	struct irq_mon_rcu_t *irq_mon_rcu;

	irq_mon_rcu = kmalloc(sizeof(*irq_mon_rcu), GFP_KERNEL);
	if (!irq_mon_rcu)
		return;
	irq_mon_rcu->val = 100;

	RCU_INIT_POINTER(irq_mon_rcu_g, irq_mon_rcu);
	call_rcu(&irq_mon_rcu_head, delayed_rcu_callback);
}

/* irq_work monitor test */
static void __irq_mon_irq_work(struct irq_work *work)
{
	irq_log_store();
	mdelay(100);
	irq_log_store();
	mdelay(400);
	irq_log_store();
	mdelay(100);
	irq_log_store();
}

static void irq_mon_irq_work(struct irq_work *work)
{
	__irq_mon_irq_work(work);
}

/* for testing kprobes */
static void irq_mon_irq_work2(struct irq_work *work)
{
	__irq_mon_irq_work(work);
}

static DEFINE_IRQ_WORK(irq_mon_irqwork, irq_mon_irq_work);
static DEFINE_IRQ_WORK(irq_mon_irqwork2, irq_mon_irq_work2);

void irq_mon_test_irq_work(void)
{
	irq_work_queue(&irq_mon_irqwork);
	irq_work_sync(&irq_mon_irqwork);
	irq_work_queue(&irq_mon_irqwork2);
}

/* IRQ disable monitor test */

void irq_mon_test_irq_disable(void)
{
	unsigned long flags;

	local_irq_save(flags);
	mdelay(1000);
	local_irq_restore(flags);

	/* The test will trigger KernelAPI Dump */
}

/* hrtimer monitor test */

static struct hrtimer irq_mon_hrtimer;
static enum hrtimer_restart irq_mon_hrtimer_func(struct hrtimer *unused)
{
	irq_log_store();
	mdelay(100);
	irq_log_store();
	mdelay(400);
	irq_log_store();
	mdelay(100);
	irq_log_store();
	return HRTIMER_NORESTART;
}

void irq_mon_test_HRTIMER(void)
{
	hrtimer_init(&irq_mon_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	irq_mon_hrtimer.function = irq_mon_hrtimer_func;
	hrtimer_start(&irq_mon_hrtimer, ms_to_ktime(0),
		      HRTIMER_MODE_REL_PINNED);
}

/* preempt count monitor test */

static struct hrtimer irq_mon_hrtimer2;
static enum hrtimer_restart irq_mon_hrtimer_func2(struct hrtimer *unused)
{
	preempt_disable();
	return HRTIMER_NORESTART;
}

void irq_mon_test_PREEMPT_COUNT(void)
{
	hrtimer_init(&irq_mon_hrtimer2, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	irq_mon_hrtimer2.function = irq_mon_hrtimer_func2;
	hrtimer_start(&irq_mon_hrtimer2, ms_to_ktime(0),
		      HRTIMER_MODE_REL_PINNED);
}

void irq_mon_delay(void *unused)
{
	mdelay(600);
}

void irq_mon_test_smp_func(void)
{
	smp_call_function(irq_mon_delay, 0, 0);
}

extern void show_irq_count_info(unsigned int output);
void irq_mon_test_show_irq_count(void)
{
	show_irq_count_info(TO_BOTH);
}

static int test_aee_callback_fn(unsigned int irq, enum irq_mon_aee_type type)
{
	pr_info("%s, irq: %u, type:%d\n",__func__ , irq , type);
	return 0;
}

#define TEST_IRQ 32343
extern void irq_mon_aee_callback(unsigned int irq, enum irq_mon_aee_type type);
void irq_mon_test_aee_callback(void)
{
	irq_mon_aee_callback(TEST_IRQ, IRQ_MON_AEE_TYPE_BURST_IRQ);
	irq_mon_aee_callback_register(TEST_IRQ, test_aee_callback_fn);
	irq_mon_aee_callback(TEST_IRQ, IRQ_MON_AEE_TYPE_BURST_IRQ);
	irq_mon_aee_callback(TEST_IRQ, IRQ_MON_AEE_TYPE_IRQ_LONG);
	irq_mon_aee_callback(TEST_IRQ, IRQ_MON_AEE_TYPE_LONG_IRQOFF);
	irq_mon_aee_callback_unregister(TEST_IRQ);
	irq_mon_aee_callback(TEST_IRQ, IRQ_MON_AEE_TYPE_LONG_IRQOFF);
}

struct irq_mon_test_func {
	char name[32];
	void (*func)(void);
};

struct irq_mon_test_func irq_mon_test_list[] = {
	{"sirq_timer_dur", irq_mon_test_TIMER_SOFTIRQ},
	{"sirq_tasklet_dur", irq_mon_test_TASKLET_SOFTIRQ},
	{"sirq_rcu_dur", irq_mon_test_RCU_SOFTIRQ},
	{"irq_work_dur", irq_mon_test_irq_work},
	{"hirq_disable", irq_mon_test_irq_disable},
	{"hrtimer_dur", irq_mon_test_HRTIMER},
	{"preempt_count", irq_mon_test_PREEMPT_COUNT},
	{"smp_func_dur", irq_mon_test_smp_func},
	{"show_irq_count", irq_mon_test_show_irq_count},
	{"aee_callback", irq_mon_test_aee_callback},
};

static ssize_t
irq_mon_test_write(struct file *file, const char *ubuf,
		     size_t count, loff_t *ppos)
{
	size_t i;
	char buf[32];

	if (count >= sizeof(buf) || count == 0)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	buf[count] = '\0';

	for (i = 0; i < ARRAY_SIZE(irq_mon_test_list); i++) {
		if (!strncmp(buf, irq_mon_test_list[i].name,
			     strlen(irq_mon_test_list[i].name)))
			irq_mon_test_list[i].func();
	}

	return count;
}

static const struct proc_ops proc_irq_monitor_test_fops = {
	.proc_open  = simple_open,
	.proc_write = irq_mon_test_write,
};

void mt_irq_monitor_test_init(struct proc_dir_entry *dir)
{
	proc_create("irq_mon_test", 0220, dir,
		    &proc_irq_monitor_test_fops);
}
