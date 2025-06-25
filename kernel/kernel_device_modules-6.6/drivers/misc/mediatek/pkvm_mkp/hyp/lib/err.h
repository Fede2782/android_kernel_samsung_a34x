/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __ERR_H
#define __ERR_H

#include <asm/kvm_pkvm_module.h>

// TODO:
#define NO_ERROR		(0)
#define ERR_GENERIC		(-1)
#define ERR_NOT_FOUND		(-2)
#define ERR_NOT_READY		(-3)
#define ERR_NO_MSG		(-4)
#define ERR_NO_MEMORY		(-5)
#define ERR_ALREADY_STARTED	(-6)
#define ERR_NOT_VALID		(-7)
#define ERR_INVALID_ARGS	(-8)

#define ERR_USER_BASE		(-16384)

#endif
