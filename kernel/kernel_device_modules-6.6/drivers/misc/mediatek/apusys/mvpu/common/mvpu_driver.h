/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MVPU_DRIVER_H__
#define __MVPU_DRIVER_H__

#include <apusys_core.h>

int mvpu_init(struct apusys_core_info *info);
void mvpu_exit(void);

#endif /* __MVPU_DRIVER_H__ */
