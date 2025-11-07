/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef PMIC_OP_H
#define PMIC_OP_H

#include <linux/types.h>

int pmic_op_of_init(void);
int pmic_op_set_capid_pre1(int acc);
int pmic_op_set_capid_pre2(int capid);

#endif /* PMIC_OP_H */
