// SPDX-License-Identifier: GPL-2.0
/*
 * Initialize a high-resolution timer (hrtimer) for each CPU during boot up. If an unexpected event
 * occurs, this hrtimer is used to find actual hrtimer_cpu_base then store information about each
 * CPU's pending timers, similar to the functionality provided by kernel/time/timer_list.c.
 *
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/hrtimer.h>
#include <linux/timerqueue.h>
#include <linux/rbtree.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <linux/stdarg.h>
#include <linux/percpu-defs.h>
#include <linux/printk.h>
#include <mt-plat/mrdump.h>
#include "hangdet.h"

static DEFINE_PER_CPU(struct hrtimer, debug_hrtimer);
static DEFINE_PER_CPU(int, debug_hrtimer_ready) = -1;
#define MAX_TIMER_LIST_FILE_SIZE SZ_128K
static char *timer_list_info;
static int write_timer_list_buf_index;

struct timerqueue_node *timerqueue_iterate_next_mtk(struct timerqueue_node *node)
{
	struct rb_node *next;

	if (!node)
		return NULL;
	next = rb_next(&node->node);
	if (!next)
		return NULL;
	return container_of(next, struct timerqueue_node, node);
}

static void log_timer_list_info(char *addr, const char *fmt, ...)
{
	unsigned long len;
	va_list ap;

	if (addr == NULL)
		return;
	if ((write_timer_list_buf_index + SZ_256) >= (unsigned long)MAX_TIMER_LIST_FILE_SIZE)
		return;

	va_start(ap, fmt);
	len = vscnprintf(&addr[write_timer_list_buf_index], SZ_256, fmt, ap);
	va_end(ap);
	write_timer_list_buf_index += len;
}

void timer_list_debug_init(void)
{
#if IS_ENABLED(CONFIG_MTK_HANG_DETECT_DB)
	timer_list_info = kmalloc(MAX_TIMER_LIST_FILE_SIZE, GFP_KERNEL);
	if (timer_list_info != NULL) {
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		int res = 0;

		res = mrdump_mini_add_extra_file((unsigned long)timer_list_info,
			__pa_nodebug(timer_list_info), MAX_TIMER_LIST_FILE_SIZE, "TIMER_LIST");
		if (res) {
			kfree(timer_list_info);
			timer_list_info = NULL;
			pr_info("SYS_TIMER_LIST_RAW file add fail...\n");
		}
#endif
	}
#endif
}

void timer_list_debug_exit(void)
{
	kfree(timer_list_info);
	timer_list_info = NULL;
}

void percpu_debug_timer_init(void)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&debug_hrtimer);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS_PINNED_HARD);
	per_cpu(debug_hrtimer_ready, smp_processor_id()) = 1;
}

#define SEQ_printf_timer(m, x...) { log_timer_list_info(m, x); }

static void
print_timer(char *m, struct hrtimer *taddr, struct hrtimer *timer,
	    int idx, u64 now)
{
	SEQ_printf_timer(m, " #%d: <%pK>, %ps", idx, taddr, timer->function);
	SEQ_printf_timer(m, ", S:%02x", timer->state);
	SEQ_printf_timer(m, "\n");
	SEQ_printf_timer(m, " # expires at %llu-%llu nsecs [in %lld to %lld nsecs]\n",
		(unsigned long long)ktime_to_ns(hrtimer_get_softexpires(timer)),
		(unsigned long long)ktime_to_ns(hrtimer_get_expires(timer)),
		(long long)(ktime_to_ns(hrtimer_get_softexpires(timer)) - now),
		(long long)(ktime_to_ns(hrtimer_get_expires(timer)) - now));
}

static void
print_active_timers(char *m, struct hrtimer_clock_base *base,
		    u64 now)
{
	struct hrtimer *timer, tmp;
	unsigned long next = 0, i;
	struct timerqueue_node *curr;
	unsigned long flags;

next_one:
	i = 0;

	// touch_nmi_watchdog();

	raw_spin_lock_irqsave(&base->cpu_base->lock, flags);

	curr = timerqueue_getnext(&base->active);
	/*
	 * Crude but we have to do this O(N*N) thing, because
	 * we have to unlock the base when printing:
	 */
	while (curr && i < next) {
		curr = timerqueue_iterate_next_mtk(curr);
		i++;
	}

	if (curr) {

		timer = container_of(curr, struct hrtimer, node);
		tmp = *timer;
		raw_spin_unlock_irqrestore(&base->cpu_base->lock, flags);

		print_timer(m, timer, &tmp, i, now);
		next++;
		goto next_one;
	}
	raw_spin_unlock_irqrestore(&base->cpu_base->lock, flags);
}

static void
print_base(char *m, struct hrtimer_clock_base *base, u64 now)
{
	SEQ_printf_timer(m,   "active timers:\n");
	print_active_timers(m, base, now + ktime_to_ns(base->offset));
}

static void print_cpu(char *m, int cpu, u64 now)
{
	struct hrtimer *hrtimer = per_cpu_ptr(&debug_hrtimer, cpu);
	struct hrtimer_cpu_base *cpu_base;
	int i;

	if (per_cpu(debug_hrtimer_ready, cpu) == -1) {
		SEQ_printf_timer(m, "cpu: %d does not init debug hrtimer, exit...\n", cpu);
		return;
	}

	if (!hrtimer || !(hrtimer->base) || !(hrtimer->base->cpu_base)) {
		SEQ_printf_timer(m, "cpu: %d debug hrtimer abnormal, exit...\n", cpu);
		return;
	}

	cpu_base = hrtimer->base->cpu_base;
	SEQ_printf_timer(m, "cpu: %d\n", cpu);
	SEQ_printf_timer(m, "hrtimer_cpu_base->cpu: %d\n", cpu_base->cpu);
	for (i = 0; i < HRTIMER_MAX_CLOCK_BASES; i++) {
		SEQ_printf_timer(m, " clock %d:\n", i);
		print_base(m, cpu_base->clock_base + i, now);
	}
#define P(x) \
	SEQ_printf_timer(m, "  .%-15s: %llu\n", #x, \
		   (unsigned long long)(cpu_base->x))
#define P_ns(x) \
	SEQ_printf_timer(m, "  .%-15s: %llu nsecs\n", #x, \
		   (unsigned long long)(ktime_to_ns(cpu_base->x)))

#if IS_ENABLED(CONFIG_HIGH_RES_TIMERS)
	P_ns(expires_next);
	P(hres_active);
	P(nr_events);
	P(nr_retries);
	P(nr_hangs);
	P(max_hang_time);
#endif
#undef P
#undef P_ns

	SEQ_printf_timer(m, "\n");
}

static inline void timer_list_header(char *m, u64 now)
{
	SEQ_printf_timer(m, "Timer List Version: v0.9\n");
	SEQ_printf_timer(m, "HRTIMER_MAX_CLOCK_BASES: %d\n", HRTIMER_MAX_CLOCK_BASES);
	SEQ_printf_timer(m, "now at %lld nsecs\n", (unsigned long long)now);
	SEQ_printf_timer(m, "\n");
}

static void hangdet_timer_list_show(char *m)
{
	u64 now = ktime_to_ns(ktime_get());
	int cpu;

	timer_list_header(m, now);

	for_each_online_cpu(cpu)
		print_cpu(m, cpu, now);

	pr_info("%s %d done..\n", __func__, __LINE__);
}

void save_timer_list_info(void)
{
	if (timer_list_info == NULL)
		return;

	memset(timer_list_info, 0, MAX_TIMER_LIST_FILE_SIZE);
	write_timer_list_buf_index = 0;
	hangdet_timer_list_show(timer_list_info);
}
