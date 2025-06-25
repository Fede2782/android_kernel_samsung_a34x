// SPDX-License-Identifier: GPL-2.0-only
/*
 * Device access for Mediatek MT6379
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <dt-bindings/mfd/mt6379.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/sched/clock.h>

#include "mt6379.h"

static const struct irq_ind_rg_map {
	u8 ind_mask;
	unsigned int evt_rg_base;
	unsigned int mask_rg_base;
	u8 rg_num;
} mt6379_irq_ind_rg_maps[] = {
	{ MT6379_INDM_CHG, 0x50, 0x90, 4 },
	{ MT6379_INDM_CHG, 0x57, 0x97, 2 },
	{ MT6379_INDM_ADC, 0x59, 0x99, 1 },
	{ MT6379_INDM_CHG, 0x5A, 0x9A, 1 },
	{ MT6379_INDM_BASE, 0x5C, 0x9C, 1 },
	{ MT6379_INDM_FLED, 0x60, 0xA0, 3 },
	{ MT6379_INDM_BOBU, 0x64, 0xA4, 1 },
	{ MT6379_INDM_GM30, 0x68, 0xA8, 1 },
	{ MT6379_INDM_BASE, 0x69, 0xA9, 1 },
	{ MT6379_INDM_USBPD, 0x6A, 0xAA, 1 },
};

static irqreturn_t mt6379_irq_threaded_handler(int irq, void *d)
{
	struct mt6379_data *data = d;
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap;
	const struct irq_ind_rg_map *map;
	unsigned int evt_offs = 0, rg_cnt = 0, ind_val = 0;
	u8 evts[MT6379_MAX_IRQ_REG] = {};
	long long t1 = 0, t2 = 0;
	int i, j, ret;

	t1 = local_clock();
	ret = regmap_read(regmap, MT6379_REG_IRQ_IND, &ind_val);
	if (ret)
		return IRQ_NONE;

	/* Read out all events by indicator */
	for (i = 0; i < ARRAY_SIZE(mt6379_irq_ind_rg_maps); i++) {
		map = mt6379_irq_ind_rg_maps + i;

		evt_offs = rg_cnt;
		rg_cnt += map->rg_num;

		if (!(ind_val & map->ind_mask))
			continue;

		ret = regmap_raw_read(regmap, map->evt_rg_base, evts + evt_offs,
				      map->rg_num);
		if (ret) {
			dev_err(dev, "Failed to read evts %d (%d)\n", i, ret);
			return IRQ_NONE;
		}

		for (j = 0; j < map->rg_num; j++)
			evts[evt_offs + j] &= ~data->mask_buf[evt_offs + j];

		ret = regmap_raw_write(regmap, map->evt_rg_base,
				       evts + evt_offs, map->rg_num);
		if (ret) {
			dev_err(dev, "Failed to write evts %d (%d)\n", i, ret);
			return IRQ_NONE;
		}
	}

	/* Write clear events */
	/* Do nested irq handling except hardirqs */
	for (i = 0; i < MT6379_MAX_IRQ_REG; i++) {
		unsigned int hwirq, virq;
		bool hardirq = false;

		for (j = 0; j < 8 && evts[i]; j++) {
			hwirq = i * 8 + j;

			switch (hwirq) {
			case MT6379_EVT_GM30_LDO ... MT6379_EVT_GM30_HK1:
			case MT6379_EVT_USBPD:
				hardirq = true;
				break;
			default:
				break;
			}

			if (hardirq || !(evts[i] & BIT(j)))
				continue;

			virq = irq_find_mapping(data->irq_domain, hwirq);
			handle_nested_irq(virq);
		}
	}

	/* For dispatch to eusb */
	if (ind_val & MT6379_INDM_BASE) {
		dev_info_ratelimited(dev, "%s, Dispatch to EUSB handler\n", __func__);
		handle_nested_irq(irq_find_mapping(data->irq_domain, MT6379_DUMMY_EVT_EUSB));
	}

	ret = regmap_write(regmap, MT6379_REG_SPMI_TXDRV2, MT6379_RCS_INT_DONE);
	if (ret)
		dev_err(dev, "Failed to set IRQ retrigger (%d)\n", ret);

	t2 = local_clock();
	dev_dbg(dev, "%s delta = %llu us\n", __func__, (t2 - t1) / NSEC_PER_USEC);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_irq_handler(int irq, void *d)
{
	struct mt6379_data *data = d;
	unsigned int hardirqs[] = { MT6379_EVT_GM30_LDO, MT6379_EVT_GM30_BM2,
				    MT6379_EVT_GM30_HK2, MT6379_EVT_GM30_BM1,
				    MT6379_EVT_GM30_HK1, MT6379_EVT_USBPD,
				    MT6379_DUMMY_EVT_UFCS };
	unsigned int virq;
	int i;

	for (i = 0; i < ARRAY_SIZE(hardirqs); i++) {
		virq = irq_find_mapping(data->irq_domain, hardirqs[i]);
		generic_handle_irq(virq);
	}

	return IRQ_WAKE_THREAD;
}

static void mt6379_irq_bus_lock(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);

	mutex_lock(&data->irq_lock);
	memcpy(data->tmp_buf, data->mask_buf, MT6379_MAX_IRQ_REG);
}

static unsigned int mt6379_find_irq_hwreg(unsigned int hwirq)
{
	unsigned int offs_idx, offs_cal, rg_cnt = 0;
	unsigned int hwirq_reg = 0;
	int i;

	offs_idx = offs_cal = hwirq / 8;

	/* Lookup hwirq reg from table */
	for (i = 0; i < ARRAY_SIZE(mt6379_irq_ind_rg_maps); i++) {
		if (offs_cal >= mt6379_irq_ind_rg_maps[i].rg_num)
			offs_cal -= mt6379_irq_ind_rg_maps[i].rg_num;

		rg_cnt += mt6379_irq_ind_rg_maps[i].rg_num;

		if (offs_idx < rg_cnt) {
			hwirq_reg = mt6379_irq_ind_rg_maps[i].mask_rg_base;
			hwirq_reg += offs_cal;
			break;
		}
	}

	return hwirq_reg;
}

static void mt6379_irq_bus_sync_unlock(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);
	struct device *dev = data->dev;
	unsigned int hwirq_reg;
	int ret;

	if (data->tmp_buf[d->hwirq / 8] == data->mask_buf[d->hwirq / 8])
		goto out_sync_unlock;

	memcpy(data->mask_buf, data->tmp_buf, MT6379_MAX_IRQ_REG);

	hwirq_reg = mt6379_find_irq_hwreg(d->hwirq);
	ret = regmap_write(data->regmap, hwirq_reg, data->mask_buf[d->hwirq / 8]);
	if (ret)
		dev_warn(dev, "Failed to config for hwirq %ld\n", d->hwirq);

out_sync_unlock:
	mutex_unlock(&data->irq_lock);
}

static void mt6379_irq_enable(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);

	data->tmp_buf[d->hwirq / 8] &= ~BIT(d->hwirq % 8);
}

static void mt6379_irq_disable(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);

	data->tmp_buf[d->hwirq / 8] |= BIT(d->hwirq % 8);
}

static void mt6379_irq_mask(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);
	unsigned int hwirq_reg = mt6379_find_irq_hwreg(d->hwirq);
	int ret;

	if (d->hwirq == MT6379_DUMMY_EVT_UFCS) {
		ret = regmap_update_bits(data->regmap, MT6379_REG_IRQ_MASK, MT6379_INDM_UFCS, 0xFF);
		if (ret)
			dev_info(data->dev, "%s, Failed to mask UFCS IND_MASK\n", __func__);
	}

	ret = regmap_update_bits(data->regmap, hwirq_reg, BIT(d->hwirq % 8), 0xFF);
	if (ret)
		dev_info(data->dev, "%s, Failed to mask 0x%03X irq mask\n", __func__, hwirq_reg);
}

static void mt6379_irq_unmask(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);
	unsigned int hwirq_reg = mt6379_find_irq_hwreg(d->hwirq);
	int ret;

	if (d->hwirq == MT6379_DUMMY_EVT_UFCS) {
		ret = regmap_update_bits(data->regmap, MT6379_REG_IRQ_MASK, MT6379_INDM_UFCS, 0);
		if (ret)
			dev_info(data->dev, "%s, Failed to unmask UFCS IND_MASK\n", __func__);
	}

	ret = regmap_update_bits(data->regmap, hwirq_reg, BIT(d->hwirq % 8), 0);
	if (ret)
		dev_info(data->dev, "%s, Failed to unmask 0x%03X irq mask\n", __func__, hwirq_reg);
}

static int mt6379_irq_domain_map(struct irq_domain *h, unsigned int virq,
				 irq_hw_number_t hwirq)
{
	struct mt6379_data *data = h->host_data;
	struct irq_chip *irqc = &data->irq_chip;

	irq_set_chip_data(virq, data);

	switch (hwirq) {
	case MT6379_EVT_GM30_LDO ... MT6379_EVT_GM30_HK1:
	case MT6379_EVT_USBPD:
	case MT6379_DUMMY_EVT_UFCS:
		irq_set_chip_and_handler(virq, irqc, handle_level_irq);
		break;
	default:
		irq_set_chip(virq, irqc);
		irq_set_nested_thread(virq, true);
		break;
	}

	irq_set_parent(virq, data->irq);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops mt6379_irq_domain_ops = {
	.map = mt6379_irq_domain_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static void mt6379_free_irq_chip(void *d)
{
	struct mt6379_data *data = d;

	irq_domain_remove(data->irq_domain);
	mutex_destroy(&data->irq_lock);
}

static int mt6379_init_irq_chip(struct mt6379_data *data)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap;
	struct irq_chip *irqc = &data->irq_chip;
	const struct irq_ind_rg_map *map;
	unsigned int evt_offs = 0;
	char *irqc_name;
	int i, ret;

	irqc_name = devm_kasprintf(dev, GFP_KERNEL, "mt6379-%s", dev_name(dev));
	if (!irqc_name)
		return -ENOMEM;

	memset(data->mask_buf, 0xff, MT6379_MAX_IRQ_REG);
	mutex_init(&data->irq_lock);

	/* By default, mask all & write clear evts */
	for (i = 0; i < ARRAY_SIZE(mt6379_irq_ind_rg_maps); i++) {
		map = mt6379_irq_ind_rg_maps + i;

		ret = regmap_raw_write(regmap, map->mask_rg_base,
				       data->mask_buf + evt_offs, map->rg_num);
		if (ret)
			return ret;

		ret = regmap_raw_write(regmap, map->evt_rg_base,
				       data->mask_buf + evt_offs, map->rg_num);
		if (ret)
			return ret;

		evt_offs += map->rg_num;
	}

	irqc->name = irqc_name;
	irqc->irq_bus_lock = mt6379_irq_bus_lock;
	irqc->irq_bus_sync_unlock = mt6379_irq_bus_sync_unlock;
	irqc->irq_enable = mt6379_irq_enable;
	irqc->irq_disable = mt6379_irq_disable;
	irqc->irq_mask = mt6379_irq_mask;
	irqc->irq_unmask = mt6379_irq_unmask;
	irqc->flags = IRQCHIP_SKIP_SET_WAKE;

	data->irq_domain = irq_domain_add_linear(dev->of_node,
						 MT6379_MAX_IRQ_REG * 8,
						 &mt6379_irq_domain_ops, data);
	if (!data->irq_domain)
		return dev_err_probe(dev, -ENOMEM, "Failed to create IRQ domain\n");

	ret = devm_add_action_or_reset(dev, mt6379_free_irq_chip, data);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add devm for irq chip release\n");

	return devm_request_threaded_irq(dev, data->irq, mt6379_irq_handler,
					 mt6379_irq_threaded_handler,
					 IRQF_ONESHOT, dev_name(dev), data);
}

static ssize_t test_mode_entered_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct mt6379_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", data->test_mode_entered ? "Y" : "N");
}

static ssize_t test_mode_entered_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	static const u8 test_mode_on[] = { 0x69, 0x96, 0x63, 0x79 };
	struct mt6379_data *data = dev_get_drvdata(dev);
	struct regmap *regmap = data->regmap;
	bool enter_state;
	int ret;

	ret = kstrtobool(buf, &enter_state);
	if (ret)
		return ret;

	if (enter_state == data->test_mode_entered)
		return count;

	if (enter_state)
		ret = regmap_raw_write(regmap, MT6379_REG_TM_PASS_CODE,
				      test_mode_on, ARRAY_SIZE(test_mode_on));
	else
		ret = regmap_write(regmap, MT6379_REG_TM_PASS_CODE, 0);

	if (ret)
		return ret;

	data->test_mode_entered = enter_state;
	return count;
}
static const DEVICE_ATTR_RW(test_mode_entered);

int mt6379_device_init(struct mt6379_data *data)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap;
	unsigned int vid;
	int ret;

	ret = regmap_read(regmap, MT6379_REG_DEV_INFO, &vid);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to read vendor info\n");

	if ((vid & MT6379_VENID_MASK) != MT6379_VENDOR_ID)
		return dev_err_probe(dev, -ENODEV, "VID not matched 0x%02x\n", vid);

	dev_set_drvdata(dev, data);

	ret = mt6379_init_irq_chip(data);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init irq chip\n");

	ret = regmap_write(regmap, MT6379_REG_SPMI_TXDRV2, MT6379_RCS_INT_DONE);
	if (ret)
		dev_err(dev, "Failed to set IRQ retrigger (%d)\n", ret);

	/*
	 * Caution: There's the potential risk to enter TM.
	 * Be careful to use it. For the manufacturing information, only enter
	 * TM can get them
	 */
	ret = device_create_file(dev, &dev_attr_test_mode_entered);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to create test_mode attribute\n");

	return devm_of_platform_populate(dev);
}
