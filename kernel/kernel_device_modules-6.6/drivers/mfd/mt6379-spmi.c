// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt6379-spmi.c -- SPMI access for Mediatek MT6379
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/pm_wakeirq.h>
#include <linux/regmap.h>
#include <linux/spmi.h>

#include "mt6379.h"

#define MT6379_WRRD_WAIT_US	8
#define MT6379_REG_SPMI_RCS1	0x26
#define MT6379_REG_SPMI_RCS2	0x27

struct mt6379_priv {
	bool bypass_retrigger;
	u32 svid;
	u16 access_reg;
	size_t access_len;
	ktime_t access_time;
};

static void check_rg_access_limit(struct mt6379_priv *priv, u16 addr, size_t len, ktime_t now)
{
	ktime_t guarantee_time = ktime_add_us(priv->access_time, MT6379_WRRD_WAIT_US);
	s64 min_wait_time;

	/* Check RG addr overlap */
	if (priv->access_reg >= addr + len || addr >= priv->access_reg + priv->access_len)
		return;

	/* Check RG aceess time is after previous time + 9us */
	if (ktime_after(now, guarantee_time))
		return;

	min_wait_time = ktime_us_delta(guarantee_time, now);
	udelay(min_wait_time);
}

static void put_rg_access_limit(struct mt6379_priv *priv, u16 addr, size_t len, ktime_t now)
{
	priv->access_reg = addr;
	priv->access_len = len;
	priv->access_time = now;
}

static int mt6379_spmi_read(void *context, const void *reg_buf, size_t reg_size, void *val_buf,
			    size_t val_size)
{
	struct mt6379_data *data = context;
	struct mt6379_priv *priv = data->priv;
	struct spmi_device *sdev = to_spmi_device(data->dev);
	u16 addr;

	/* The SPMI I/O limitation of MTK common platform is 2 bytes */
	WARN_ON_ONCE(reg_size != 2);

	addr = get_unaligned_be16(reg_buf);

	check_rg_access_limit(priv, addr, val_size, ktime_get());

	return spmi_ext_register_readl(sdev, addr, val_buf, val_size);
}

static int mt6379_spmi_write(void *context, const void *val, size_t val_len)
{
	struct mt6379_data *data = context;
	struct mt6379_priv *priv = data->priv;
	struct spmi_device *sdev = to_spmi_device(data->dev);
	u16 addr;
	int ret;

	WARN_ON_ONCE(val_len < 2);

	addr = get_unaligned_be16(val);

	/*
	 * If using gpio-eint for IRQ triggering,
	 * DO NOT ACCESS the address of RCS retrigger!
	 */
	if (priv->bypass_retrigger && addr <= MT6379_REG_SPMI_TXDRV2
	    && (addr + val_len - 2) > MT6379_REG_SPMI_TXDRV2)
		return 0;

	ret = spmi_ext_register_writel(sdev, addr, val + 2, val_len - 2);
	if (ret)
		return ret;

	put_rg_access_limit(priv, addr, val_len - 2, ktime_get());
	return 0;
}

static int mt6379_spmi_gather_write(void *context, const void *reg, size_t reg_len, const void *val,
				    size_t val_len)
{
	struct mt6379_data *data = context;
	struct mt6379_priv *priv = data->priv;
	struct spmi_device *sdev = to_spmi_device(data->dev);
	u16 addr;
	int ret;

	/* The SPMI I/O limitation of MTK common platform is 2 bytes */
	WARN_ON_ONCE(reg_len != 2);

	addr = get_unaligned_be16(reg);

	/*
	 * If using gpio-eint for IRQ triggering,
	 * DO NOT ACCESS the address of RCS retrigger!
	 */
	if (priv->bypass_retrigger && addr <= MT6379_REG_SPMI_TXDRV2
	    && (addr + val_len) > MT6379_REG_SPMI_TXDRV2)
		return 0;

	ret = spmi_ext_register_writel(sdev, addr, val, val_len);
	if (ret)
		return ret;

	put_rg_access_limit(priv, addr, val_len, ktime_get());
	return 0;
}

static const struct regmap_bus mt6379_spmi_bus = {
	.read = mt6379_spmi_read,
	.write = mt6379_spmi_write,
	.gather_write = mt6379_spmi_gather_write,
	.max_raw_read = 2,
	.max_raw_write = 2,
	.fast_io = true,
};

static const struct regmap_config mt6379_spmi_config = {
	.reg_bits		= 16,
	.val_bits		= 8,
	.reg_format_endian	= REGMAP_ENDIAN_BIG,
	.max_register		= 0xdff,
};

static void mt6379_spmi_check_of_irq(struct mt6379_data *data)
{
	struct mt6379_priv *priv = data->priv;
	struct device_node *parent;
	int ret;

	priv->bypass_retrigger = false;

	ret = device_property_read_u32(data->dev, "reg", &priv->svid);
	if (ret) {
		dev_info(data->dev, "%s, Failed to get MT6379 SPMI slave id, use default value\n",
			 __func__);
		priv->svid = 0x0E;
	}

	dev_info(data->dev, "%s, MT6379 SPMI slave id: 0x%02X\n", __func__, priv->svid);
	parent = of_irq_find_parent(data->dev->of_node);
	if (parent) {
		if (of_property_read_bool(parent, "gpio-controller"))
			priv->bypass_retrigger = true;

		of_node_put(parent);
	}

	dev_notice(data->dev, "%s, bypass_retrigger: %d\n", __func__, priv->bypass_retrigger);
}

static int mt6379_probe(struct spmi_device *sdev)
{
	struct mt6379_priv *priv;
	struct mt6379_data *data;
	struct device *dev = &sdev->dev;
	int irqno, ret;

	irqno = of_irq_get(dev->of_node, 0);
	if (irqno <= 0)
		return dev_err_probe(dev, -EINVAL, "Invalid irq number (%d)\n", irqno);

	device_init_wakeup(dev, true);

	ret = dev_pm_set_wake_irq(dev, irqno);
	if (ret)
		dev_warn(dev, "Failed to set up wakeup irq\n");

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	data->irq = irqno;
	data->priv = priv;

	mt6379_spmi_check_of_irq(data);

	data->regmap = devm_regmap_init(dev, &mt6379_spmi_bus, data, &mt6379_spmi_config);
	if (IS_ERR(data->regmap))
		return dev_err_probe(dev, PTR_ERR(data->regmap), "Failed to init regmap\n");

	/* If using RCS, should set RCS config */
	if (!priv->bypass_retrigger) {
		ret = regmap_write(data->regmap, MT6379_REG_SPMI_RCS2, priv->svid);
		if (ret)
			dev_info(dev, "%s, Failed to set rcs_addr\n", __func__);

		ret = regmap_write(data->regmap, MT6379_REG_SPMI_RCS1, 0x91);
		if (ret)
			dev_info(dev, "%s, Failed to enable MT6379 RCS\n", __func__);
	}

	return mt6379_device_init(data);
}

static const struct of_device_id mt6379_spmi_dt_match[] = {
	{ .compatible = "mediatek,mt6379" },
	{ /*sentinel */ }
};
MODULE_DEVICE_TABLE(of, mt6379_spmi_dt_match);

static struct spmi_driver mt6379_spmi_driver = {
	.driver = {
		.name = "mt6379",
		.of_match_table = mt6379_spmi_dt_match,
	},
	.probe = mt6379_probe
};
module_spmi_driver(mt6379_spmi_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Mediatek MT6379 SPMI Driver");
MODULE_LICENSE("GPL");
