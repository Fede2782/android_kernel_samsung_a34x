/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 Mediatek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#ifndef __MFD_MT6379_H__
#define __MFD_MT6379_H__

#include <linux/bits.h>
#include <linux/irq.h>
#include <linux/mutex.h>

#define MT6379_REG_DEV_INFO	0x00
#define MT6379_REG_TM_PASS_CODE	0x07
#define MT6379_REG_IRQ_IND	0x0B
#define MT6379_REG_IRQ_MASK	0x0C
#define MT6379_REG_SPMI_TXDRV2	0x2B

#define MT6379_VENID_MASK	GENMASK(7, 4)
#define MT6379_RCS_INT_DONE	BIT(0)
#define MT6379_INDM_CHG		BIT(7)
#define MT6379_INDM_BOBU	BIT(6)
#define MT6379_INDM_FLED	BIT(5)
#define MT6379_INDM_ADC		BIT(4)
#define MT6379_INDM_GM30	BIT(3)
#define MT6379_INDM_USBPD	BIT(2)
#define MT6379_INDM_BASE	BIT(1)
#define MT6379_INDM_UFCS	BIT(0)

#define MT6379_VENDOR_ID	0x70
#define MT6379_MAX_IRQ_REG	16 /* except ufcs(Read Only), use dummy hwirq 127(reserved) */

struct mt6379_data {
	struct device *dev;
	struct regmap *regmap;
	struct irq_domain *irq_domain;
	void *priv;
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	u8 mask_buf[MT6379_MAX_IRQ_REG];
	u8 tmp_buf[MT6379_MAX_IRQ_REG];
	bool test_mode_entered;
	int irq;
};

extern int mt6379_device_init(struct mt6379_data *data);

#endif
