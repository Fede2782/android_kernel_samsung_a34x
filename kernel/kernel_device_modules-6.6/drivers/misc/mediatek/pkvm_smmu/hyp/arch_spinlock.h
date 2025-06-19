/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2019 The Hafnium Authors.
 */

#pragma once

/**
 * Spinlock implementation using Armv8.0 LDXR/STXR pair and a WFE pause.
 *
 * Implementation using C11 atomics also generates a LDXR/STXR pair but no WFE.
 * Without it we observe that Cortex A72 can easily livelock and not make
 * forward progress.
 *
 * TODO(b/141087046): Forward progress is still not guaranteed as even with WFE
 * we see that A72 can livelock for extremely tight loops. We should investigate
 * the guarantees provided by atomic instructions introduced in Armv8.1 LSE.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include "arch_types.h"

struct spinlock_t {
	volatile uint32_t v;
};

#define SPINLOCK_INIT ((struct spinlock_t){ .v = 0 })

static inline void sl_init(struct spinlock_t *l)
{
	*l = SPINLOCK_INIT;
}

static inline void sl_lock(struct spinlock_t *l)
{
	register uintreg_t tmp1;
	register uintreg_t tmp2;

	/*
	 * Acquire the lock with a LDAXR/STXR pair (acquire semantics on the
	 * load instruction). Pause using WFE if the lock is currently taken.
	 * This is NOT guaranteed to make progress.
	 */
	__asm__ volatile(
		"	mov	%w2, #1\n"
		"	sevl\n" /* set event bit */
		"1:	wfe\n" /* wait for event, clear event bit */
		"2:	ldaxr	%w1, [%3]\n" /* load lock value */
		"	cbnz	%w1, 1b\n" /* if lock taken, goto WFE */
		"	stxr	%w1, %w2, [%3]\n" /* try to take lock */
		"	cbnz	%w1, 2b\n" /* loop if unsuccessful */
		: "+m"(*l), "=&r"(tmp1), "=&r"(tmp2)
		: "r"(l)
		: "cc");
}

static inline void sl_unlock(struct spinlock_t *l)
{
	/*
	 * Store zero to lock's value with release semantics. This triggers an
	 * event which wakes up other threads waiting on a lock (no SEV needed).
	 */
	__asm__ volatile("stlr wzr, [%1]" : "=m"(*l) : "r"(l) : "cc");
}
