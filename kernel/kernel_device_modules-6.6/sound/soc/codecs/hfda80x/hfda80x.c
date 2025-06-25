// SPDX-License-Identifier: GPL-2.0
/*
 * hfda80x.c  --  driver for HFDA80x codec
 *
 * Copyright(C) 2023  STMicroelectronics Inc.
 * Author:
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "hfda80x.h"

static const struct reg_default hfda801_regs[] = {
	{ HFDA801_IB0, 0x46 },
	{ HFDA801_IB1, 0x0E },
	{ HFDA801_IB2, 0x00 },
	{ HFDA801_IB3, 0x00 },
	{ HFDA801_IB4, 0x21 },
	{ HFDA801_IB5, 0x00 },
	{ HFDA801_IB6, 0x00 },
	{ HFDA801_IB7, 0x00 },
	{ HFDA801_IB8, 0x00 },
	{ HFDA801_IB9, 0x00 },
	{ HFDA801_IB10, 0x00 },
	{ HFDA801_IB11, 0x10 },
	{ HFDA801_IB12, 0x00 },
	{ HFDA801_IB13, 0x01 },
	{ HFDA801_IB14, 0x11 },
	{ HFDA801_IB15, 0x11 },
	{ HFDA801_IB16, 0x11 },
	{ HFDA801_IB17, 0x11 },
	{ HFDA801_IB18, 0x01 },
	{ HFDA801_IB19, 0x9C },
	{ HFDA801_IB20, 0x00 },
	{ HFDA801_IB21, 0x00 },
	{ HFDA801_IB22, 0x00 },
	{ HFDA801_IB23, 0x08 },
	{ HFDA801_IB24, 0x00 },
};

static const struct reg_default hfda803_regs[] = {
	{ HFDA803_IB0, 0x20 },
	{ HFDA803_IB1, 0x40 },
	{ HFDA803_IB2, 0x01 },
	{ HFDA803_IB3, 0x00 },
	{ HFDA803_IB4, 0x00 },
	{ HFDA803_IB5, 0x00 },
	{ HFDA803_IB6, 0x00 },
	{ HFDA803_IB7, 0x00 },
	{ HFDA803_IB8, 0xE1 },
	{ HFDA803_IB9, 0x00 },
	{ HFDA803_IB10, 0x10 },
	{ HFDA803_IB11, 0x00 },
	{ HFDA803_IB12, 0x08 },
	{ HFDA803_IB13, 0x20 },
	{ HFDA803_IB14, 0x08 },
};

static int hfda80x_init_registers(struct hfda80x_priv *hfda80x)
{
	int i = 0, ret = 0;

	switch (hfda80x->type) {
	case HFDA801:
		// i2c enable, IB0[1:0]=10
		ret = regmap_write(hfda80x->regmap, HFDA801_IB0, 0x02);
		if (ret) {
			dev_info(hfda80x->dev, "%s, i2c enable failed, ret:%d\n", __func__, ret);
			return ret;
		}

		// init regs
		for (i = 0; i < ARRAY_SIZE(hfda801_regs); i++) {
			ret = regmap_write(hfda80x->regmap, hfda801_regs[i].reg, hfda801_regs[i].def);
			if (ret) {
				dev_info(hfda80x->dev, "%s, write register failed, ret:%d\n", __func__, ret);
				return ret;
			}
		}

		// first setup programmed - read to work, IB23[0]=1
		ret = regmap_update_bits(hfda80x->regmap, HFDA801_IB23, 0x01, 0x01);
		if (ret) {
			dev_info(hfda80x->dev, "%s, first setup programmed setting failed, ret:%d\n", __func__, ret);
			return ret;
		}

		break;
	case HFDA803:
		// init regs
		for (i = 0; i < ARRAY_SIZE(hfda803_regs); i++) {
			ret = regmap_write(hfda80x->regmap, hfda803_regs[i].reg, hfda803_regs[i].def);
			if (ret) {
				dev_info(hfda80x->dev, "%s, write register failed, ret:%d\n", __func__, ret);
				return ret;
			}
		}
		// first setup programmed - read to work, IB14[0]=1
		ret = regmap_update_bits(hfda80x->regmap, HFDA803_IB14, 0x01, 0x01);
		if (ret) {
			dev_info(hfda80x->dev, "%s, first setup programmed setting failed, ret:%d\n", __func__, ret);
			return ret;
		}
		break;
	}
	dev_info(hfda80x->dev, "%s done, type:%d\n", __func__, hfda80x->type);
	return 0;
}

static const struct regmap_config hfda801_i2c_regmap = {
	.name = "hfda801",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = HFDA801_DB25,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config hfda803_i2c_regmap = {
	.name = "hfda803",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = HFDA803_DB3,
	.cache_type = REGCACHE_NONE,
};

static int hfda80x_i2c_probe(struct i2c_client *i2c)
{
	const struct regmap_config *hfda80x_regmap_config = NULL;
	const struct i2c_device_id *id = i2c_client_get_device_id(i2c);
	struct hfda80x_priv *hfda80x;
	int ret;

	hfda80x = devm_kzalloc(&i2c->dev, sizeof(struct hfda80x_priv), GFP_KERNEL);
	if (hfda80x == NULL)
		return -ENOMEM;

	hfda80x->type = id->driver_data;
	hfda80x->dev = &i2c->dev;

	dev_info(&i2c->dev, "%s, type:%d, addr: 0x%x\n", __func__, hfda80x->type, i2c->addr);

	switch (hfda80x->type) {
	case HFDA801:
		hfda80x_regmap_config = &hfda801_i2c_regmap;
		break;
	case HFDA803:
		hfda80x_regmap_config = &hfda803_i2c_regmap;
		break;
	default:
		dev_info(&i2c->dev, "Unknown device type %d\n", hfda80x->type);
		break;
	}

	hfda80x->mute_gpio = devm_gpiod_get_optional(&i2c->dev, "st,mute", GPIOD_OUT_LOW);
	if (IS_ERR(hfda80x->mute_gpio)) {
		ret = PTR_ERR(hfda80x->mute_gpio);
		dev_info(&i2c->dev, "Failed to get mute gpio: %d\n", ret);
		goto err;
	}

	hfda80x->enable_gpio = devm_gpiod_get_optional(&i2c->dev, "st,enable", GPIOD_OUT_HIGH);
	if (IS_ERR(hfda80x->enable_gpio)) {
		ret = PTR_ERR(hfda80x->enable_gpio);
		dev_info(&i2c->dev, "Failed to get enable gpio: %d\n", ret);
		goto err;
	}

	hfda80x->regmap = devm_regmap_init_i2c(i2c, hfda80x_regmap_config);
	if (IS_ERR(hfda80x->regmap)) {
		ret = PTR_ERR(hfda80x->regmap);
		dev_info(&i2c->dev, "Failed to allocate register map: %d\n", ret);
		goto err;
	}

	i2c_set_clientdata(i2c, hfda80x);

	if (hfda80x->enable_gpio)
		gpiod_set_value(hfda80x->enable_gpio, 0);

	mdelay(10);

	ret = hfda80x_init_registers(hfda80x);

	if (hfda80x->mute_gpio)
		gpiod_set_value(hfda80x->mute_gpio, 0);

err:
	return ret;
}

static void hfda80x_i2c_remove(struct i2c_client *client)
{
	struct hfda80x_priv *hfda80x = i2c_get_clientdata(client);

	gpiod_set_value(hfda80x->mute_gpio, 1);
	gpiod_set_value(hfda80x->enable_gpio, 1);
}

static const struct i2c_device_id hfda80x_i2c_id[] = {
	{ "hfda801", HFDA801 },
	{ "hfda803", HFDA803 },
	{ }
};

#if defined(CONFIG_OF)
static const struct of_device_id hfda80x_of_match[] = {
	{ .compatible = "st,hfda801", },
	{ .compatible = "st,hfda803", },
	{},
};
#endif

static struct i2c_driver hfda80x_i2c_driver = {
	.driver = {
		.name = "hfda80x",
		.of_match_table = hfda80x_of_match,
	},
	.probe = hfda80x_i2c_probe,
	.remove = hfda80x_i2c_remove,
	.id_table = hfda80x_i2c_id,
};
module_i2c_driver(hfda80x_i2c_driver);

MODULE_DESCRIPTION("STMicroelectronics FDA80X driver");
MODULE_LICENSE("GPL");
