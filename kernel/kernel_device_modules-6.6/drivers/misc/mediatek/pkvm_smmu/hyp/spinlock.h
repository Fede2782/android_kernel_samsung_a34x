/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2018 The Hafnium Authors.
 */

#pragma once

/*
 * Includes the arch-specific definition of 'struct spinlock' and
 * implementations of:
 *  - SPINLOCK_INIT
 *  - sl_lock()
 *  - sl_unlock()
 */
#include "arch_spinlock.h"

/**
 * Locks both locks, enforcing the lowest address first ordering for locks of
 * the same kind.
 */
static inline void sl_lock_both(struct spinlock_t *a, struct spinlock_t *b)
{
	if (a < b) {
		sl_lock(a);
		sl_lock(b);
	} else {
		sl_lock(b);
		sl_lock(a);
	}
}
