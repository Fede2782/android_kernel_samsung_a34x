/* drivers/motor/isa1000.c

 * Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/vibrator/sec_vibrator.h>
#include "isa1000_vibrator.h"


static int isa1000_vib_set_frequency(struct device *dev, int num)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;

	if (num >= 0 && num < pdata->freq_nums) {
		pdata->frequency = pdata->freq_array[num];
	} else if (num >= HAPTIC_ENGINE_FREQ_MIN &&
			num <= HAPTIC_ENGINE_FREQ_MAX) {
		pdata->frequency = num;
	} else {
		isa_err("Out of range %d", num);
		return -EINVAL;
	}

	isa_info("freq= %d", pdata->frequency);
	ddata->period = FREQ_DIVIDER / pdata->frequency;
	ddata->duty = ddata->max_duty =
		(ddata->period * ddata->pdata->duty_ratio) / 100;

	return 0;
}

static int isa1000_vib_set_freq_stored(struct device *dev, int freq)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;

	if (freq >= HAPTIC_ENGINE_FREQ_MIN && freq <= HAPTIC_ENGINE_FREQ_MAX)
		pdata->freq_array[0] = pdata->frequency = freq;
	else {
		isa_err("freq_stored out of range %d", freq);
		return -EINVAL;
	}

	isa_info("freq_stored = %d", freq);
	ddata->period = FREQ_DIVIDER / pdata->frequency;
	ddata->duty = ddata->max_duty =
		ddata->period * pdata->duty_ratio / 100;

	return 0;
}

static int isa1000_vib_get_freq_stored(struct device *dev, char *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int ret;

	isa_info("freq_stored = %d", pdata->freq_array[0]);
	ret = snprintf(buf, VIB_BUFSIZE, "%d\n",
		pdata->freq_array[0]);

	return ret;
}

static int isa1000_vib_get_freq_default(struct device *dev, char *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int ret;

	ret = snprintf(buf, VIB_BUFSIZE, "%d\n",
		pdata->freq_default);

	return ret;
}

#if IS_ENABLED(CONFIG_MTK_PWM)
static int isa1000_vib_set_ratio(int ratio)
{
	int send_data1_bits_limit;
	int bit_size = motor_pwm_config.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE + 1;

	send_data1_bits_limit = bit_size * ratio / 100;
	send_data1_bits_limit -= (bit_size / 2);

	isa_info("send_data1_bits_limit set to %d", send_data1_bits_limit);

	return send_data1_bits_limit;
}
#endif

static int isa1000_vib_set_intensity(struct device *dev, int intensity)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int ret = 0;
	int ratio = 100;
#if IS_ENABLED(CONFIG_MTK_PWM)
	int send_data1_bits_limit;
#else
	int duty = ddata->period >> 1;
#endif

	if (intensity < -(MAX_INTENSITY) || MAX_INTENSITY < intensity) {
		isa_err("out of range %d", intensity);
		return -EINVAL;
	}

	ratio = sec_vibrator_recheck_ratio(&ddata->sec_vib_ddata);

#if IS_ENABLED(CONFIG_MTK_PWM)
	send_data1_bits_limit = isa1000_vib_set_ratio(ratio);
	motor_pwm_config.PWM_MODE_FIFO_REGS.SEND_DATA1 = (1 << (send_data1_bits_limit * intensity / MAX_INTENSITY)) - 1;
	mt_pwm_power_on(motor_pwm_config.pwm_no, 0);
	mt_pwm_clk_sel_hal(0, CLK_26M);
#else
	intensity = intensity * ratio / 100;
	if (intensity == MAX_INTENSITY)
		duty = ddata->max_duty;
	else if (intensity == -(MAX_INTENSITY))
		duty = ddata->period - ddata->max_duty;
	else if (intensity != 0)
		duty += (ddata->max_duty - duty) * intensity / MAX_INTENSITY;

	ddata->duty = duty;

	isa_info("intensity= %d, duty= %d, period= %d", intensity, ddata->duty, ddata->period);
	ret = pwm_config(ddata->pdata->pwm, duty, ddata->period);
	if (ret < 0) {
		isa_err("failed to config pwm %d", ret);
		return ret;
	}
#endif
	if (intensity != 0) {
#if IS_ENABLED(CONFIG_MTK_PWM)
		ret = pwm_set_spec_config(&motor_pwm_config);
#else
		ret = pwm_enable(ddata->pdata->pwm);
#endif
		if (ret < 0)
			isa_err("failed to enable pwm %d", ret);
	} else {
#if IS_ENABLED(CONFIG_MTK_PWM)
		mt_pwm_disable(motor_pwm_config.pwm_no, motor_pwm_config.pmic_pad);
#else
		pwm_disable(ddata->pdata->pwm);
#endif
	}

	return ret;
}

static void isa1000_regulator_en(struct isa1000_ddata *ddata, bool en)
{
	int ret;

	if (ddata->pdata->regulator == NULL)
		return;

	if (en && !regulator_is_enabled(ddata->pdata->regulator)) {
		ret = regulator_enable(ddata->pdata->regulator);
		if (ret)
			isa_err("regulator_enable returns: %d", ret);
	} else if (!en && regulator_is_enabled(ddata->pdata->regulator)) {
		ret = regulator_disable(ddata->pdata->regulator);
		if (ret)
			isa_err("regulator_disable returns: %d", ret);
	}
}

static void isa1000_gpio_en(struct isa1000_ddata *ddata, bool en)
{
	if (gpio_is_valid(ddata->pdata->gpio_en))
		gpio_direction_output(ddata->pdata->gpio_en, en);
}

static int isa1000_vib_enable(struct device *dev, bool en)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);

	if (en) {
		isa1000_regulator_en(ddata, en);
		isa1000_gpio_en(ddata, en);
	} else {
		isa1000_gpio_en(ddata, en);
		isa1000_regulator_en(ddata, en);
	}

	return 0;
}

static int isa1000_get_motor_type(struct device *dev, char *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int ret = snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);

	return ret;
}

#if defined(CONFIG_SEC_VIBRATOR)
static bool isa1000_get_calibration(struct device *dev)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;

	return pdata->calibration;
}

static int isa1000_get_step_size(struct device *dev, int *step_size)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;

	isa_info("step_size=%d", pdata->steps);

	if (pdata->steps == 0)
		return -ENODATA;

	*step_size = pdata->steps;

	return 0;
}

static int isa1000_get_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->intensities[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->intensities[i];

	return 0;
}

static int isa1000_set_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->intensities[i] = buf[i];

	return 0;
}

static int isa1000_get_haptic_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->haptic_intensities[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->haptic_intensities[i];

	return 0;
}

static int isa1000_set_haptic_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->haptic_intensities[i] = buf[i];

	return 0;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

static const struct sec_vibrator_ops isa1000_vib_ops = {
	.enable = isa1000_vib_enable,
	.set_intensity = isa1000_vib_set_intensity,
	.set_frequency = isa1000_vib_set_frequency,
	.set_freq_stored = isa1000_vib_set_freq_stored,
	.get_freq_stored = isa1000_vib_get_freq_stored,
	.get_freq_default = isa1000_vib_get_freq_default,
	.get_motor_type = isa1000_get_motor_type,
#if defined(CONFIG_SEC_VIBRATOR)
	.get_calibration = isa1000_get_calibration,
	.get_step_size = isa1000_get_step_size,
	.get_intensities = isa1000_get_intensities,
	.set_intensities = isa1000_set_intensities,
	.get_haptic_intensities = isa1000_get_haptic_intensities,
	.set_haptic_intensities = isa1000_set_haptic_intensities,
#endif
};

#if defined(CONFIG_SEC_VIBRATOR)
static int of_sec_vibrator_dt(struct isa1000_pdata *pdata, struct device_node *np)
{
	int ret = 0;
	int i;
	unsigned int val = 0;
	int *intensities = NULL;

	isa_info("start");
	pdata->calibration = false;

	/* number of steps */
	ret = of_property_read_u32(np, "samsung,steps", &val);
	if (ret) {
		isa_err("out of range(%d)", val);
		return -EINVAL;
	}
	pdata->steps = (int)val;

	/* allocate memory for intensities */
	pdata->intensities = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->intensities)
		return -ENOMEM;
	intensities = pdata->intensities;

	/* intensities */
	ret = of_property_read_u32_array(np, "samsung,intensities", intensities, pdata->steps);
	if (ret) {
		isa_err("intensities are not specified");
		ret = -EINVAL;
		goto err_getting_int;
	}

	for (i = 0; i < pdata->steps; i++) {
		if ((intensities[i] < 0) || (intensities[i] > MAX_INTENSITY)) {
			isa_err("out of range(%d)", intensities[i]);
			ret = -EINVAL;
			goto err_getting_int;
		}
		}
	intensities = NULL;

	/* allocate memory for haptic_intensities */
	pdata->haptic_intensities = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->haptic_intensities) {
		ret = -ENOMEM;
		goto err_alloc_haptic;
	}
	intensities = pdata->haptic_intensities;

	/* haptic intensities */
	ret = of_property_read_u32_array(np, "samsung,haptic_intensities", intensities, pdata->steps);
	if (ret) {
		isa_err("haptic_intensities are not specified");
		ret = -EINVAL;
		goto err_haptic;
	}
	for (i = 0; i < pdata->steps; i++) {
		if ((intensities[i] < 0) || (intensities[i] > MAX_INTENSITY)) {
			isa_err("out of range(%d)", intensities[i]);
			ret = -EINVAL;
			goto err_haptic;
		}
	}

	/* update calibration statue */
	pdata->calibration = true;

	return ret;

err_haptic:
	kfree(pdata->haptic_intensities);
err_alloc_haptic:
	pdata->haptic_intensities = NULL;
err_getting_int:
	kfree(pdata->intensities);
	pdata->intensities = NULL;
	pdata->steps = 0;

	return ret;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

static struct isa1000_pdata *isa1000_get_devtree_pdata(struct device *dev)
{
	struct device_node *node;
	struct isa1000_pdata *pdata;
	u32 temp;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	node = dev->of_node;
	if (!node) {
		isa_err("error to get dt node");
		goto err_out;
	}

	ret = of_property_read_u32(node, "isa1000,multi_frequency", &temp);
	if (ret) {
		isa_info("multi_frequency isn't used");
		pdata->freq_nums = 0;
	} else
		pdata->freq_nums = (int)temp;

	if (pdata->freq_nums) {
		pdata->freq_array = devm_kzalloc(dev,
				sizeof(u32)*pdata->freq_nums, GFP_KERNEL);
		if (!pdata->freq_array) {
			isa_err("failed to allocate freq_array data");
			goto err_out;
		}

		ret = of_property_read_u32_array(node, "isa1000,frequency",
				pdata->freq_array, pdata->freq_nums);
		if (ret) {
			isa_err("error to get dt node frequency");
			goto err_out;
		}

		pdata->frequency = pdata->freq_default = pdata->freq_array[0];
	} else {
		isa_err("error to get frequency (%d)", pdata->freq_nums);
		goto err_out;
	}

	ret = of_property_read_u32(node, "isa1000,duty_ratio",
			&pdata->duty_ratio);
	if (ret) {
		isa_err("error to get dt node duty_ratio");
		goto err_out;
	}

	ret = of_property_read_string(node, "isa1000,regulator_name",
			&pdata->regulator_name);
	if (!ret) {
		pdata->regulator = regulator_get(NULL, pdata->regulator_name);
		if (IS_ERR(pdata->regulator)) {
			ret = PTR_ERR(pdata->regulator);
			pdata->regulator = NULL;
			isa_err("regulator get fail");
			goto err_out;
		}
	} else {
		isa_info("regulator isn't used");
		pdata->regulator = NULL;
	}

	pdata->gpio_en = of_get_named_gpio(node, "isa1000,gpio_en", 0);
	if (gpio_is_valid(pdata->gpio_en)) {
		ret = gpio_request(pdata->gpio_en, "isa1000,gpio_en");
		if (ret) {
			isa_err("motor gpio request fail(%d)", ret);
			goto err_out;
		}
		ret = gpio_direction_output(pdata->gpio_en, 0);
	} else {
		isa_info("gpio isn't used");
	}

#if IS_ENABLED(CONFIG_MTK_PWM)
	mt_pwm_power_on(0, 0);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	pdata->pwm = devm_of_pwm_get(dev, node, NULL);
#else
	pdata->pwm = devm_pwm_get(dev, NULL);
#endif
	if (IS_ERR(pdata->pwm)) {
		isa_err("error to get pwms(%d)", IS_ERR(pdata->pwm));
		ret = PTR_ERR(pdata->pwm);
		goto err_out;
	}
#endif

	ret = of_property_read_string(node, "isa1000,motor_type",
			&pdata->motor_type);
	if (ret)
		isa_err("motor_type is undefined");

#if defined(CONFIG_SEC_VIBRATOR)
	ret = of_sec_vibrator_dt(pdata, node);
	if (ret < 0)
		isa_err("sec_vibrator dt read fail : %d", ret);
#endif

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int isa1000_probe(struct platform_device *pdev)
{
	struct isa1000_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct isa1000_ddata *ddata;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = isa1000_get_devtree_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			isa_err("there is no device tree!");
			return -ENODEV;
		}
#else
		isa_err("there is no platform data!");
#endif
	}

	ddata = devm_kzalloc(&pdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		isa_err("failed to alloc");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ddata);
	ddata->pdata = pdata;
	ddata->period = FREQ_DIVIDER / pdata->frequency;
	ddata->max_duty = ddata->duty =
		ddata->period * ddata->pdata->duty_ratio / 100;

	ddata->sec_vib_ddata.dev = &pdev->dev;
	ddata->sec_vib_ddata.vib_ops = &isa1000_vib_ops;
	sec_vibrator_register(&ddata->sec_vib_ddata);

	isa_info("done");

	return 0;
}

static int isa1000_remove(struct platform_device *pdev)
{
	struct isa1000_ddata *ddata = platform_get_drvdata(pdev);

	sec_vibrator_unregister(&ddata->sec_vib_ddata);
	isa1000_vib_enable(&pdev->dev, false);
	return 0;
}

static int isa1000_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	isa1000_vib_enable(&pdev->dev, false);
	return 0;
}

static int isa1000_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id isa1000_dt_ids[] = {
	{ .compatible = "imagis,isa1000_vibrator" },
	{ }
};
MODULE_DEVICE_TABLE(of, isa1000_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver isa1000_driver = {
	.probe		= isa1000_probe,
	.remove		= isa1000_remove,
	.suspend	= isa1000_suspend,
	.resume		= isa1000_resume,
	.driver = {
		.name	= "isa1000",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(isa1000_dt_ids),
	},
};

static int __init isa1000_init(void)
{
	return platform_driver_register(&isa1000_driver);
}
module_init(isa1000_init);

static void __exit isa1000_exit(void)
{
	platform_driver_unregister(&isa1000_driver);
}
module_exit(isa1000_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("isa1000 vibrator driver");
