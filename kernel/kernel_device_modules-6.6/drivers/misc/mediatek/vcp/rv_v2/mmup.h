/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __MMUP_H__
#define __MMUP_H__

void mmup_enable_irqs(void);
void mmup_disable_irqs(void);
void dump_mmup_irq_status(void);
struct mtk_ipi_device *mmup_get_ipidev(void);
void mmup_dump_last_regs(void);
void mmup_do_tbufdump_RV33(void);
int mmup_init(void);
void mmup_exit(void);

#endif
