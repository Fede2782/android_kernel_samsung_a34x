// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/bits.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define MT6379_BB_NORMAL_MODE		0
#define MT6379_BB_FPWM_MODE		1

#define MT6379_REG_BUBO_ENABLE		0x40
#define MT6379_REG_BB_CONTROL1		0x41
#define MT6379_REG_BB_VOUT_SEL		0x43
#define MT6379_REG_BUBO_STAT		0x84

#define MT6379_BUBO_BYP_ENABLE_MASK	BIT(1)
#define MT6379_BUBO_ENABLE_MASK		BIT(0)
#define MT6379_BB_FPWM_MASK		BIT(3)
#define MT6379_BB_RAMP_MASK		GENMASK(1, 0)
#define MT6379_BB_VOUT_MASK		GENMASK(6, 0)
#define MT6379_BUBO_UV_MASK		BIT(3)
#define MT6379_BUBO_OC_MASK		BIT(2)
#define MT6379_BUBO_TSD_MASK		BIT(1)
#define MT6379_BUBO_PG_MASK		BIT(0)

#define MT6379_BB_MIN_MICROVOLT	2025000
#define MT6379_BB_STP_MICROVOLT	25000
/* N_VOLT = ((MAX 5200000 - MIN 2025000)) / STEP 25000 + 1 */
#define MT6379_BB_N_VOLT	128

static int mt6379_bb_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int mode_val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		mode_val = MT6379_BB_FPWM_MASK;
		break;
	case REGULATOR_MODE_NORMAL:
		mode_val = 0;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(regmap, MT6379_REG_BB_CONTROL1,
				  MT6379_BB_FPWM_MASK, mode_val);
}

static unsigned int mt6379_bb_get_mode(struct regulator_dev *rdev)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int regval;
	int ret;

	ret = regmap_read(regmap, MT6379_REG_BB_CONTROL1, &regval);
	if (ret)
		return REGULATOR_MODE_INVALID;

	if (regval & MT6379_BB_FPWM_MASK)
		return REGULATOR_MODE_FAST;

	return REGULATOR_MODE_NORMAL;
}

static int mt6379_bb_get_error_flags(struct regulator_dev *rdev,
				     unsigned int *flags)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int regval, evts = 0;
	int ret;

	ret = regmap_read(regmap, MT6379_REG_BUBO_STAT, &regval);
	if (ret)
		return ret;

	if (!(regval & MT6379_BUBO_PG_MASK))
		goto out_get_error;

	if (regval & MT6379_BUBO_UV_MASK)
		evts |= REGULATOR_ERROR_UNDER_VOLTAGE;

	if (regval & MT6379_BUBO_OC_MASK)
		evts |= REGULATOR_ERROR_OVER_CURRENT;

	if (regval & MT6379_BUBO_TSD_MASK)
		evts |= REGULATOR_ERROR_OVER_TEMP;

out_get_error:
	*flags = evts;
	return 0;
}

static const struct regulator_ops mt6379_buckboost_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6379_bb_set_mode,
	.get_mode = mt6379_bb_get_mode,
	.get_error_flags = mt6379_bb_get_error_flags,
	.set_bypass = regulator_set_bypass_regmap,
	.get_bypass = regulator_get_bypass_regmap,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
};

static unsigned int mt6379_bb_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6379_BB_NORMAL_MODE:
		return REGULATOR_MODE_NORMAL;
	case MT6379_BB_FPWM_MODE:
		return REGULATOR_MODE_FAST;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const unsigned int mt6379_bb_vramp_table[] = { 1000, 2500, 5000, 10000 };

static const struct regulator_desc mt6379_buckboost_desc = {
	.name		= "mt6379-buckboost",
	.type		= REGULATOR_VOLTAGE,
	.owner		= THIS_MODULE,
	.ops		= &mt6379_buckboost_ops,
	.supply_name	= "VSYS",
	.min_uV		= MT6379_BB_MIN_MICROVOLT,
	.n_voltages	= MT6379_BB_N_VOLT,
	.uV_step	= MT6379_BB_STP_MICROVOLT,
	.vsel_reg	= MT6379_REG_BB_VOUT_SEL,
	.vsel_mask	= MT6379_BB_VOUT_MASK,
	.enable_reg	= MT6379_REG_BUBO_ENABLE,
	.enable_mask	= MT6379_BUBO_ENABLE_MASK,
	.bypass_reg	= MT6379_REG_BUBO_ENABLE,
	.bypass_mask	= MT6379_BUBO_BYP_ENABLE_MASK,
	.bypass_val_on	= MT6379_BUBO_BYP_ENABLE_MASK,
	.ramp_reg	= MT6379_REG_BB_CONTROL1,
	.ramp_mask	= MT6379_BB_RAMP_MASK,
	.ramp_delay_table = mt6379_bb_vramp_table,
	.n_ramp_values	= ARRAY_SIZE(mt6379_bb_vramp_table),
	.of_map_mode	= mt6379_bb_of_map_mode,
};

static int mt6379_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regulator_config cfg = {};
	struct regulator_dev *rdev;

	cfg.dev = dev;
	cfg.of_node = dev->of_node;
	cfg.init_data = of_get_regulator_init_data(dev, dev->of_node,
						   &mt6379_buckboost_desc);

	rdev = devm_regulator_register(dev, &mt6379_buckboost_desc, &cfg);
	if (IS_ERR(rdev))
		return dev_err_probe(dev, PTR_ERR(rdev), "Failed to register regulator\n");

	return 0;
}

static const struct of_device_id mt6379_regulator_dt_match[] = {
	{ .compatible = "mediatek,mt6379-regulator" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mt6379_regulator_dt_match);

static struct platform_driver mt6379_regulator_driver = {
	.driver = {
		.name = "mt6379-regulator",
		.of_match_table = mt6379_regulator_dt_match,
	},
	.probe = mt6379_regulator_probe,
};
module_platform_driver(mt6379_regulator_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Mediatek MT6379 Regulator Driver");
MODULE_LICENSE("GPL");
