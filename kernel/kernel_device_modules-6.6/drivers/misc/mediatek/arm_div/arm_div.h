/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __ARM_DIV_H__
#define __ARM_DIV_H__

#if IS_ENABLED(CONFIG_ARM)
extern int __aeabi_idivmod(int, int);
extern unsigned int __aeabi_uidivmod(unsigned int, unsigned int);
#endif

#endif /* __ARM_DIV_H__ */
