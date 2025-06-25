/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MTK_IRQ_MON__
#define __MTK_IRQ_MON__

enum irq_mon_aee_type {
	IRQ_MON_AEE_TYPE_BURST_IRQ = 0,
	IRQ_MON_AEE_TYPE_IRQ_LONG,
	IRQ_MON_AEE_TYPE_LONG_IRQOFF,
};
typedef int (*aee_callback_t)(unsigned int irq, enum irq_mon_aee_type type);

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR)
int irq_mon_aee_callback_register(unsigned int irq, aee_callback_t fn);
void irq_mon_aee_callback_unregister(unsigned int irq);

extern void mt_aee_dump_irq_info(void);
void __irq_log_store(const char *func, int line);
#define irq_log_store() __irq_log_store(__func__, __LINE__)

#else
#define irq_mon_aee_callback_register(a, b) do {} while (0)
#define irq_mon_aee_callback_unregister(a) do {} while (0)

#define mt_aee_dump_irq_info() do {} while (0)
static inline void __irq_log_store(const char *func, int line)
{
}

#define irq_log_store() do {} while (0)
#endif
#endif
