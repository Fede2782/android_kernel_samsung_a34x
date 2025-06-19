/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __HYP_SPMI_H__
#define __HYP_SPMI_H__

#include <linux/spmi.h>

struct pmic_core {
	struct device *dev;
	struct spmi_device *sdev;
	struct regmap *regmap;
	u16 chip_id;
	int irq;
	bool *enable_hwirq;
	bool *cache_hwirq;
	struct mutex irqlock;
	struct irq_domain *irq_domain;
	const struct mtk_spmi_pmic_data *chip_data;
};

#define MAX_SPMI_DEVICE_NUM 16
struct pmic_core *hyp_pmic_core[MAX_SPMI_DEVICE_NUM];


#endif /*__HYP_SPMI_H__*/

