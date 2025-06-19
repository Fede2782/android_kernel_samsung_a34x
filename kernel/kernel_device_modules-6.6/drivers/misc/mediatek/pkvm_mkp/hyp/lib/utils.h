/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __UTILS_H
#define __UTILS_H

#include <asm/kvm_pkvm_module.h>

#define container_of(ptr, type, member) ({			\
		void *__mptr = (void *)(ptr);			\
		((type *)(__mptr - offsetof(type, member))); })

#define READ_ONCE(x)						\
({								\
	(*(const volatile typeof(x) *)&(x));			\
})

#define WRITE_ONCE(x, val)					\
do {								\
	*(volatile typeof(x) *)&(x) = (val);			\
} while(0)

#endif
