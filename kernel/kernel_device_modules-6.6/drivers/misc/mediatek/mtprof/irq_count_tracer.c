// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

/*
 * If a irq is frequently triggered, it could result in problems.
 * The purpose of this feature is to catch the condition. When the
 * average time interval of a irq is below the threshold, we judge
 * the irq is triggered abnormally and print a message for reference.
 *
 * average time interval =
 *     statistics time / irq count increase during the statistics time
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/kernel_stat.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/percpu-defs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/seq_buf.h>
#include <linux/slab.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_irq_mon.h>

#include "internal.h"

static bool irq_count_tracer __read_mostly;
static unsigned int irq_period_th1_ns = 666666; /* log */
static unsigned int irq_period_th2_ns = 200000; /* aee */
static unsigned int irq_count_aee_limit = 1;
/* period setting for specific irqs */
struct irq_count_period_setting {
	const char *name;
	unsigned int period;
} irq_count_plist[] = {
	{"usb0", 16666}, /* 60000 irqs per sec*/
	{"ufshcd", 10000}, /* 100000 irqs per sec*/
	{"arch_timer", 50000}, /* 20000 irqs per sec*/
	{"musb-hdrc", 16666}, /* 60000 irqs per sec*/
	{"11201000.usb0", 16666}, /* 60000 irqs per sec*/
	{"16701000.usb0", 16666}, /* 60000 irqs per sec*/
	{"xhci-hcd:usb1", 16666}, /* 60000 irqs per sec*/
	{"wlan0", 12500}, /* 80000 irqs per sec*/
	{"DPMAIF_AP", 1837}, /* 544125 irqs per sec */ /* data tput */
	{"DPMAIF_AP_RX0", 1498}, /* 667230 irqs per sec */ /* data tput */
	{"CCIF_AP_DATA0", 0}, /* No limit for MD EE to save debug logs. */
	{"mtk_uart_apdma", 40000}, /* 25000 irqs per sec*/
	{"48000000.mali", 71428}, /* 14000 irqs per sec*/
	{"13000000.mali", 71428}, /* 14000 irqs per sec*/
};

const char *irq_to_name(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	if (desc && desc->action && desc->action->name)
		return desc->action->name;
	return NULL;
}

const void *irq_to_handler(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	if (desc && desc->action && desc->action->handler)
		return (void *)desc->action->handler;
	return NULL;
}

#if !IS_ENABLED(CONFIG_ARM64)
const int desc_to_ipi_type(struct irq_desc *desc)
{
	int temp_irq = 0;
	int num = 0;
	struct irq_desc *temp_desc;

	for_each_irq_desc(temp_irq, temp_desc) {
		if (temp_desc->action && temp_desc->action->name) {
			if (!strcmp(temp_desc->action->name, "IPI")) {
				if (temp_desc == desc)
					return num;
				else
					num++;
			}
		}
	}
	return -1;
}
#else
const int desc_to_ipi_type(struct irq_desc *desc)
{
	struct irq_desc **ipi_desc = ipi_desc_get();
	int nr_ipi = nr_ipi_get();
	int i = 0;

	for (i = 0; i < nr_ipi; i++)
		if (ipi_desc[i] == desc)
			return i;
	return -1;
}
#endif

const int irq_to_ipi_type(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	return desc_to_ipi_type(desc);
}

/*
 * return true: not in debounce (will do aee) and update time if update is true
 * return false: in debounce period (not do aee) and do not update time.
 */
bool irq_mon_aee_debounce_check(bool update)
{
	static unsigned long long irq_mon_aee_debounce = 5000000000; /* 5s */
	static unsigned long long t_prev_aee;
	static int in_debounce_check;
	unsigned long long t_check = 0;
	bool ret = true;

	/*
	 * if in_debounce_check = 0, set to 1 and return 0 (continue checking)
	 * if in_debounce_check = 1, return 1 (return false to caller)
	 */
	if (cmpxchg(&in_debounce_check, 0, 1))
		return false;

	t_check = sched_clock();

	if (t_prev_aee && irq_mon_aee_debounce &&
	    (t_check - t_prev_aee) < irq_mon_aee_debounce)
		ret = false;
	else if (update)
		t_prev_aee = t_check;

	xchg(&in_debounce_check, 0);

	return ret;
}

struct irq_count_stat {
	unsigned int index;
	unsigned long long t_start;
	unsigned long long t_end;
	unsigned long long t_diff;
};

static struct irq_count_stat irq_count_stat;
static DEFINE_SPINLOCK(aee_callback_lock);

/* per irq */
struct irq_mon_desc {
	unsigned int irq;
	unsigned int __percpu (*count)[2];
	u64 __percpu *time;
	unsigned int __percpu *long_count;
	aee_callback_t fn;
};

static DEFINE_XARRAY(imdesc_xa);
#define IMDESC_IRQ(imdesc, cpu, i) ((*per_cpu_ptr((imdesc)->count, cpu))[i])

static struct irq_mon_desc *irq_mon_desc_lookup(unsigned int irq)
{
	return xa_load(&imdesc_xa, irq);
}

static struct irq_mon_desc *irq_mon_desc_alloc(unsigned int irq)
{
	struct irq_mon_desc *desc;
	int err = 0;

	desc = kzalloc(sizeof(*desc), GFP_ATOMIC);
	if (!desc)
		goto out;
	desc->count = alloc_percpu_gfp((*desc->count), GFP_ATOMIC);
	if (!desc->count)
		goto out_free_desc;
	desc->time = alloc_percpu_gfp((*desc->time), GFP_ATOMIC);
	if (!desc->time)
		goto out_free_count;
	desc->long_count = alloc_percpu_gfp((*desc->long_count), GFP_ATOMIC);
	if (!desc->long_count)
		goto out_free_time;
	desc->irq = irq;
	/*
	 * This entry might be stored by concurrent irq_mon_desc_alloc()
	 * Use xa_insert() to prevent override the entry.
	 */
	scoped_guard(irqsave)
		err = xa_insert(&imdesc_xa, irq, desc, GFP_ATOMIC);
	if (!err)
		goto out;

	free_percpu(desc->long_count);
out_free_time:
	free_percpu(desc->time);
out_free_count:
	free_percpu(desc->count);
out_free_desc:
	kfree(desc);

	/* Try to return the entry if it is present. */
	desc =  (err == -EBUSY) ? xa_load(&imdesc_xa, irq) : NULL;
out:
	return desc;
}

/* account the irq time for current cpu */
void irq_mon_account_irq_time(u64 time, int irq)
{
	struct irq_mon_desc *desc = irq_mon_desc_lookup(irq);

	desc = (desc) ? : irq_mon_desc_alloc(irq);
	if (!desc)
		return;

	__this_cpu_add(*desc->time, time);
	if (time >= 5000000ULL)
		__this_cpu_inc(*desc->long_count);
}

static int irq_time_proc_show(struct seq_file *m, void *v)
{
	unsigned long index;
	struct irq_mon_desc *desc;
	int cpu;

	xa_for_each(&imdesc_xa, index, desc) {
		seq_printf(m, "%u", (unsigned int)index);
		for_each_possible_cpu(cpu)
			seq_printf(m, " %llu", *per_cpu_ptr(desc->time, cpu));
		seq_putc(m, '\n');
	}
	return 0;
}

static int irq_long_count_proc_show(struct seq_file *m, void *v)
{
	unsigned long index;
	struct irq_mon_desc *desc;
	int cpu;

	xa_for_each(&imdesc_xa, index, desc) {
		seq_printf(m, "%u", (unsigned int)index);
		for_each_possible_cpu(cpu)
			seq_printf(m, " %u",
				   *per_cpu_ptr(desc->long_count, cpu));
		seq_putc(m, '\n');
	}
	return 0;
}

int irq_mon_aee_callback_register(unsigned int irq, aee_callback_t fn)
{
	struct irq_mon_desc *desc = irq_mon_desc_lookup(irq);
	unsigned long flags;

	desc = desc ?: irq_mon_desc_alloc(irq);
	if (!desc)
		return -ENOMEM;

	spin_lock_irqsave(&aee_callback_lock, flags);
	rcu_assign_pointer(desc->fn, fn);
	spin_unlock_irqrestore(&aee_callback_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(irq_mon_aee_callback_register);

void irq_mon_aee_callback_unregister(unsigned int irq)
{
	struct irq_mon_desc *desc = irq_mon_desc_lookup(irq);
	unsigned long flags;

	if (!desc)
		return;

	spin_lock_irqsave(&aee_callback_lock, flags);
	rcu_assign_pointer(desc->fn, NULL);
	spin_unlock_irqrestore(&aee_callback_lock, flags);
	synchronize_rcu();
}
EXPORT_SYMBOL_GPL(irq_mon_aee_callback_unregister);

void irq_mon_aee_callback(unsigned int irq, enum irq_mon_aee_type type)
{
	struct irq_mon_desc *desc = irq_mon_desc_lookup(irq);
	aee_callback_t fn;

	if (!desc)
		return;

	rcu_read_lock();
	fn = rcu_dereference(desc->fn);
	if (fn)
		fn(desc->irq, type);
	rcu_read_unlock();
}

static void update_irq_count(void)
{
	unsigned int irq;
	int cpu;
	struct irq_desc *desc;
	struct irq_mon_desc *imdesc;
	struct irq_count_stat *stat = &irq_count_stat;
	unsigned long flags;
	XA_STATE(xas, &imdesc_xa, 0);

	/* Step 1: pre-allocate irq_mon_desc */
	rcu_read_lock();
	for_each_irq_nr(irq) {
		imdesc = irq_mon_desc_lookup(irq);
		if (imdesc)
			continue;

		desc = irq_to_desc(irq);
		if (!desc || !desc->kstat_irqs)
			continue;

		for_each_online_cpu(cpu) {
			if (data_race(*per_cpu_ptr(desc->kstat_irqs, cpu))) {
				irq_mon_desc_alloc(irq);
				break;
			}
		}
	}
	rcu_read_unlock();

	/*
	 * Step 2: Take a snapshot of IRQ counts.
	 * To reduce the duration of critical section, do not check irq count
	 * or allocate memory here.
	 */
	stat->index = !stat->index;
	stat->t_start = stat->t_end;
	xas_lock_irqsave(&xas, flags);
	/*
	 * Get the timestamp when holding lock to reduce noise.
	 * The timestamp is not protected by xa_lock.
	 */
	stat->t_end = sched_clock();
	xas_for_each(&xas, imdesc, ULONG_MAX) {
		if (xas_retry(&xas, imdesc))
			continue;
		desc = irq_to_desc(imdesc->irq);
		if (!desc || !desc->kstat_irqs)
			continue;

		for_each_online_cpu(cpu)
			IMDESC_IRQ(imdesc, cpu, stat->index) =
				data_race(*per_cpu_ptr(desc->kstat_irqs, cpu));
	}
	xas_unlock_irqrestore(&xas, flags);
	stat->t_diff = stat->t_end - stat->t_start;
}

static void show_one_imdesc(unsigned int output, struct irq_mon_desc *imdesc,
			    int cpu, unsigned int index)
{
	unsigned int count, prev_count;
	char msg[MAX_MSG_LEN];
	struct seq_buf buf;
	struct irq_desc *desc;
	unsigned long flags;

	count = IMDESC_IRQ(imdesc, cpu, index);
	prev_count = IMDESC_IRQ(imdesc, cpu, !index);

	if (count - prev_count == 0)
		return;
	seq_buf_init(&buf, msg, sizeof(msg));
	seq_buf_printf(&buf, "%5u:%10u %10u %10u ", imdesc->irq,
		       prev_count, count, count - prev_count);
	desc = irq_to_desc(imdesc->irq);
	if (!desc) {
		irq_mon_msg(output, "%s{irq_desc not exist!}", msg);
		return;
	}
	raw_spin_lock_irqsave(&desc->lock, flags);
	if (desc->action && desc->action->name) {
		seq_buf_printf(&buf, "%s", desc->action->name);
		if (!strcmp(desc->action->name, "IPI"))
			seq_buf_printf(&buf, "%d", desc_to_ipi_type(desc));
	} else {
		seq_buf_printf(&buf, "%s", "NULL");
	}
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	irq_mon_msg(output, "%s", msg);
}

static void __show_irq_count_info(unsigned int output)
{
	struct irq_count_stat *stat = &irq_count_stat;
	XA_STATE(xas, &imdesc_xa, 0);
	int cpu;
	struct irq_mon_desc *imdesc;

	/* update the irq count if irq_count_task starved for more than 1s */
	if (sched_clock() - stat->t_end > 2 * NSEC_PER_SEC)
		update_irq_count();

	irq_mon_msg(output, "===== IRQ Status =====");
	irq_mon_msg(output, "from %lld.%06lu to %lld.%06lu, %lld ms",
		    sec_high(stat->t_start), sec_low(stat->t_start),
		    sec_high(stat->t_end), sec_low(stat->t_end),
		    msec_high(stat->t_diff));

	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		irq_mon_msg(output, "CPU%d", cpu);
		irq_mon_msg(output, "# IRQ ORIG-COUNT  NEW-COUNT  INCREASED IRQ-NAME");
		xas_set(&xas, 0);
		xas_for_each(&xas, imdesc, ULONG_MAX)
			show_one_imdesc(output, imdesc, cpu, stat->index);
		irq_mon_msg(output, "");
	}
	rcu_read_unlock();
}

void show_irq_count_info(unsigned int output)
{
	if (irq_count_tracer)
		__show_irq_count_info(output);
}

/* caller must holding desc->lock */
static unsigned int check_burst_irq(unsigned long count, struct irq_desc *desc)
{
	u64 t_avg = irq_count_stat.t_diff;
	unsigned int i, ret = 0;

	if (!desc->action || !desc->action->name || !desc->action->handler)
		return ret;

	do_div(t_avg, count);
	if (t_avg > irq_period_th1_ns)
		return ret;

	/* Print a log if anyone exceeds the threshold. */
	ret = TO_BOTH;

	for (i = 0; i < ARRAY_SIZE(irq_count_plist); i++) {
		if (!strcmp(desc->action->name, irq_count_plist[i].name))
			if (t_avg > irq_count_plist[i].period)
				return ret;
	}

	/* skip AEE for IPI */
	if (!strcmp(desc->action->name, "IPI"))
		return ret;

	if (irq_period_th2_ns && t_avg < irq_period_th2_ns &&
	    irq_count_aee_limit &&
	    irq_mon_aee_debounce_check(true))
		ret |= TO_AEE;
	return ret;
}

static void irq_count_core(void)
{
	XA_STATE(xas, &imdesc_xa, 0);
	struct irq_count_stat *stat = &irq_count_stat;
	unsigned long long t_diff_ms;
	struct irq_desc *desc;
	struct irq_mon_desc *imdesc;
	int cpu;

	if (!irq_count_tracer)
		return;

	update_irq_count();

	/* check irq counts */
	t_diff_ms = stat->t_diff;
	do_div(t_diff_ms, NSEC_PER_MSEC);

	xas_set(&xas, 0);
	rcu_read_lock();
	xas_for_each(&xas, imdesc, ULONG_MAX) {
		struct seq_buf buf_msg, buf_mod;
		unsigned long count, flags;
		char aee_msg[MAX_MSG_LEN] = {};
		char module[100] = {};
		unsigned int out;

		if (xas_retry(&xas, imdesc))
			continue;
		/* Skip the first time of checking. */
		if (!xas_get_mark(&xas, XA_MARK_0)) {
			xas_lock_irqsave(&xas, flags);
			xas_set_mark(&xas, XA_MARK_0);
			xas_unlock_irqrestore(&xas, flags);
			continue;
		}
		for_each_online_cpu(cpu) {
			count = IMDESC_IRQ(imdesc, cpu, stat->index) -
				IMDESC_IRQ(imdesc, cpu, !stat->index);
			/* The irq is not triggered in this period */
			if (count == 0)
				continue;

			/* The irq count is decreased */
			if (unlikely(count > UINT_MAX / 2))
				continue;

			desc = irq_to_desc(imdesc->irq);
			if (!desc)
				continue;

			raw_spin_lock_irqsave(&desc->lock, flags);
			out = check_burst_irq(count, desc);
			if (!out) {
				raw_spin_unlock_irqrestore(&desc->lock, flags);
				continue;
			}
			seq_buf_init(&buf_msg, aee_msg, sizeof(aee_msg));
			seq_buf_init(&buf_mod, module, sizeof(module));
			seq_buf_printf(&buf_msg, "irq: %u [<%px>]%ps, %s",
				       imdesc->irq, (void *)desc->action->handler,
				       (void *)desc->action->handler,
				       desc->action->name);
			seq_buf_printf(&buf_mod, "BURST IRQ:%u, %ps %s",
				       imdesc->irq, desc->action->handler,
				       desc->action->name);

			if (!strcmp(desc->action->name, "IPI")) {
				int ipi_type = desc_to_ipi_type(desc);

				seq_buf_printf(&buf_msg, "%d", ipi_type);
				seq_buf_printf(&buf_mod, "%d", ipi_type);
			}
			raw_spin_unlock_irqrestore(&desc->lock, flags);

			seq_buf_printf(&buf_msg, " count +%lu in %lld ms, from %lld.%06lu to %lld.%06lu on CPU:%d",
				       count, t_diff_ms,
				       sec_high(stat->t_start), sec_low(stat->t_start),
				       sec_high(stat->t_end), sec_low(stat->t_end),
				       cpu);

			irq_mon_msg(out, aee_msg);
			if (out & TO_AEE) {
				irq_mon_aee_callback(imdesc->irq,
						     IRQ_MON_AEE_TYPE_BURST_IRQ);
				aee_kernel_warning_api(__FILE__, __LINE__,
						       DB_OPT_DUMMY_DUMP
						       | DB_OPT_FTRACE,
						       module, aee_msg);
			}
		}
	}
	rcu_read_unlock();
}

static int irq_count_kthread(void *unused)
{
	while (!kthread_should_stop()) {
		irq_count_core();
		msleep(1000);
	}
	pr_debug("%s stopped\n", __func__);
	return 0;
}

static struct task_struct *irq_count_task;

static int start_irq_count_task(void)
{
	struct task_struct *t;

	t = kthread_run(irq_count_kthread, NULL, "%s", "irqcountmonitor");
	if (IS_ERR(t))
		return PTR_ERR(t);

	irq_count_task = t;
	return 0;
}

extern bool b_count_tracer_default_enabled;
int irq_count_tracer_init(void)
{
	/*
	 * At this stage, no one else will modify the irq_count_tracer, so it
	 * is unnecessary to hold a lock here.
	 */
	if (b_count_tracer_default_enabled) {
		irq_count_tracer = 1;
		return start_irq_count_task();
	}
	return 0;
}

void irq_count_tracer_exit(void)
{
	struct irq_mon_desc *desc;
	unsigned long index;

	if (irq_count_task)
		kthread_stop(irq_count_task);
	xa_for_each(&imdesc_xa, index, desc) {
		free_percpu(desc->long_count);
		free_percpu(desc->time);
		free_percpu(desc->count);
		kfree(desc);
	}
	xa_destroy(&imdesc_xa);
}

/* Caller must holding lock */
void irq_count_tracer_set(bool val)
{
	if (irq_count_tracer == val)
		return;

	irq_count_tracer = val;
	if (irq_count_tracer) {
		if (start_irq_count_task())
			irq_count_tracer = false;
	} else {
		kthread_stop(irq_count_task);
		irq_count_task = NULL;
	}
}

const struct proc_ops irq_mon_count_pops = {
	.proc_open = irq_mon_bool_open,
	.proc_write = irq_mon_count_set,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

void irq_count_tracer_proc_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *dir;

	proc_create_single("irq_time", 0444, parent, irq_time_proc_show);
	proc_create_single("irq_lcount", 0444, parent,
			   irq_long_count_proc_show);
	dir = proc_mkdir("irq_count_tracer", parent);
	if (!dir)
		return;

	proc_create_data("irq_count_tracer", 0644,
			 dir, &irq_mon_count_pops, (void *)&irq_count_tracer);
	IRQ_MON_TRACER_PROC_ENTRY(irq_period_th1_ns, 0644, uint, dir, &irq_period_th1_ns);
	IRQ_MON_TRACER_PROC_ENTRY(irq_period_th2_ns, 0644, uint, dir, &irq_period_th2_ns);
	IRQ_MON_TRACER_PROC_ENTRY(irq_count_aee_limit, 0644, uint, dir, &irq_count_aee_limit);
	return;
}
