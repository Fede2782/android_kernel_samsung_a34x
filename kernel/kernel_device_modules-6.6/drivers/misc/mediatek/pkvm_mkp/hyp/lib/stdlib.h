/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __STDLIB_H
#define __STDLIB_H

#define ROUNDUP(a, b)		(((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b)		((a) & ~((b)-1))
//#define ALIGN(a, b)		ROUNDUP(a, b)
//#define IS_ALIGNED(a, b)	(!((a) & ((b)-1)))

#endif
