// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/led-class-flash.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <media/v4l2-flash-led-class.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"
#endif

struct mt6379_data;

enum mt6379_fled_idx {
	MT6379_FLASH_LED1 = 0,
	MT6379_FLASH_LED2,
	MT6379_FLASH_LED3,
	MT6379_FLASH_MAX_LED
};

#define MT6379_REG_FLED_STAT1		0x80
#define MT6379_REG_STRBTO		0x202
#define MT6379_REG_ISTRB(_id)		(0x203 + (_id) * 3)
#define MT6379_REG_ITORCH(_id)		(0x204 + (_id) * 3)
#define MT6379_REG_FLED_EN		0x20E

#define MT6379_FL_TORCH_MASK		BIT(4)
#define MT6379_FL_STROBE_MASK		BIT(3)
#define MT6379_FL_CSEN_MASK(_id)	BIT(MT6379_FLASH_LED3 - (_id))
#define MT6379_FL_VIN_OVERLOAD_MASK	BIT(7)
#define MT6379_FL_OVERTEMP_MASK		BIT(6)
#define MT6379_FL_VIN_LOW_MASK		BIT(5)
#define MT6379_FL_LVF_STAT_MASK		BIT(3)
#define MT6379_FL_SHORT_STAT_MASK(_id)	BIT(4 - (_id))
#define MT6379_FL_STRBTO_STAT_MASK(_id)	BIT(7 - (_id))

#define MT6379_ITOR_MINUA		25000
#define MT6379_ITOR_MAXUA		400000
#define MT6379_ITOR_STEPUA		5000
#define MT6379_ISTRB_MINUA		50000
#define MT6379_ISTRB_MAXUA		1500000
#define MT6379_ISTRB_STEPUA		10000
#define MT6379_STRBTO_MINUS		64000
#define MT6379_STRBTO_MAXUS		2432000
#define MT6379_STRBTO_STEPUS		32000
#define MT6379_FLEDST_REGSIZE		3
#define MT6379_STRBON_MIN_WAIT_US	5000
#define MT6379_STRBOFF_MIN_WAIT_US	2000

struct mt6379_flash {
	struct led_classdev_flash flash;
	struct v4l2_flash *v4l2;
	enum mt6379_fled_idx idx;
	void *driver_data;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id dev_id;
	int need_cooler;
	unsigned long target_current;
	unsigned long ori_current;
#endif
};

struct mt6379_data {
	struct device *dev;
	struct regmap *regmap;
	/* Use lock to prevent critical section on shared led controller HW */
	struct mutex lock;
	u8 flash_used;
	unsigned long strobe_enabled;
	unsigned long torch_enabled;
	struct mt6379_flash mtflash[];
};

static int mt6379_torch_set_brightness(struct led_classdev *led_cdev,
				       enum led_brightness brightness)
{
	struct mt6379_flash *mtflash = container_of(led_cdev,
						    struct mt6379_flash,
						    flash.led_cdev);
	struct mt6379_data *data = mtflash->driver_data;
	struct regmap *regmap = data->regmap;
	unsigned int mask, enable = 0;
	unsigned int cur;
	int ret;

	mutex_lock(&data->lock);

	if (data->strobe_enabled) {
		ret = -EINVAL;
		dev_err(led_cdev->dev, "Storbe in used [%lu]\n", data->strobe_enabled);
		goto out_torch_set;
	}

	mask = MT6379_FL_TORCH_MASK | MT6379_FL_CSEN_MASK(mtflash->idx);

	if (brightness == LED_OFF) {
		data->torch_enabled &= ~BIT(mtflash->idx);

		/* If anyone is still in use, keep torch mode enabled */
		if (data->torch_enabled)
			enable |= MT6379_FL_TORCH_MASK;
	} else {
#ifdef CONFIG_MTK_FLASHLIGHT_PT
		if (flashlight_pt_is_low()) {
			dev_info(led_cdev->dev, "pt is low\n");
			mutex_unlock(&data->lock);
			return 0;
		}

		cur = (brightness - 1) * MT6379_ITOR_STEPUA + MT6379_ITOR_MINUA;
		if (mtflash->need_cooler == 0) {
			mtflash->ori_current = cur;
		} else {
			if (cur > mtflash->target_current) {
				cur = mtflash->target_current - MT6379_ITOR_MINUA;
				brightness = cur / MT6379_ITOR_STEPUA;
				brightness++;
				pr_info("thermal limit current:%d\n", cur);
			}
		}
#endif

		ret = regmap_write(regmap, MT6379_REG_ITORCH(mtflash->idx),
				   brightness - 1);
		if (ret)
			goto out_torch_set;

		data->torch_enabled |= BIT(mtflash->idx);
		enable |= mask;
	}

	ret = regmap_update_bits(regmap, MT6379_REG_FLED_EN, mask, enable);

out_torch_set:
	mutex_unlock(&data->lock);
	return ret;
}

static int mt6379_flash_set_brightness(struct led_classdev_flash *flash,
				       u32 brightness)
{
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;
	struct led_flash_setting *s = &flash->brightness;
	unsigned int selector;

#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (mtflash->need_cooler == 1 &&
			brightness > mtflash->target_current) {
		brightness = mtflash->target_current;
		pr_info("thermal limit current:%u\n", brightness);
	}
#endif
	selector = (brightness - s->min) / s->step;
	return regmap_write(data->regmap, MT6379_REG_ISTRB(mtflash->idx),
			    selector);
}

static int mt6379_flash_get_brightness(struct led_classdev_flash *flash,
				       u32 *brightness)
{
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;
	struct led_flash_setting *s = &flash->brightness;
	unsigned int selector;
	int ret;

	ret = regmap_read(data->regmap, MT6379_REG_ISTRB(mtflash->idx),
			  &selector);
	if (ret)
		return ret;

	*brightness = selector * s->step + s->min;
	return 0;
}

static int mt6379_flash_set_strobe(struct led_classdev_flash *flash, bool state)
{
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;
	struct regmap *regmap = data->regmap;
	unsigned int mask, enable = 0;
	unsigned long min_wait_us = 0;
	int ret = 0, idx = mtflash->idx;

	mutex_lock(&data->lock);

	if (!(state ^ test_bit(idx, &data->strobe_enabled))) {
		dev_dbg(flash->led_cdev.dev, "no change for strobe, [%lu]\n", data->strobe_enabled);
		goto out_strobe_set;
	}

	if (data->torch_enabled) {
		ret = -EINVAL;
		dev_err(flash->led_cdev.dev, "Torch in used [%lu]\n",
			data->torch_enabled);
		goto out_strobe_set;
	}

	mask = MT6379_FL_STROBE_MASK | MT6379_FL_CSEN_MASK(mtflash->idx);

	if (state) {
		/* If first on, minimum wait on time */
		if (!data->strobe_enabled)
			min_wait_us = MT6379_STRBON_MIN_WAIT_US;

		data->strobe_enabled |= BIT(mtflash->idx);
		enable |= mask;
	} else {
		data->strobe_enabled &= ~BIT(mtflash->idx);

		/* If anyone is still in use, keep strobe mode enabled */
		if (data->strobe_enabled)
			enable |= MT6379_FL_STROBE_MASK;
		else /*If all off, minimum wait off time  */
			min_wait_us = MT6379_STRBOFF_MIN_WAIT_US;
	}

#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low()) {
		dev_info(flash->led_cdev.dev, "pt is low\n");
		mutex_unlock(&data->lock);
		return 0;
	}
#endif

	ret = regmap_update_bits(regmap, MT6379_REG_FLED_EN, mask, enable);
	if (ret) {
		dev_err(flash->led_cdev.dev, "Failed to set FLED_EN\n");
		goto out_strobe_set;
	}

	/* If wait_us not equal to zero, usleep_range(x, x + x / 2) */
	if (min_wait_us)
		usleep_range(min_wait_us, min_wait_us * 3 / 2);

out_strobe_set:
	mutex_unlock(&data->lock);
	return ret;
}

static int mt6379_flash_get_strobe(struct led_classdev_flash *flash,
				   bool *state)
{
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;

	mutex_lock(&data->lock);
	*state = data->strobe_enabled & BIT(mtflash->idx);
	mutex_unlock(&data->lock);

	return 0;
}

static int mt6379_flash_set_timeout(struct led_classdev_flash *flash,
				    u32 timeout)
{
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;
	struct led_flash_setting *s = &flash->timeout;
	unsigned int selector;

	selector = (timeout - s->min) / s->step;
	return regmap_write(data->regmap, MT6379_REG_STRBTO, selector);
}

static int mt6379_flash_get_fault(struct led_classdev_flash *flash, u32 *fault)
{
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;
	u8 fled_stat[MT6379_FLEDST_REGSIZE];
	u32 rpt_fault = 0;
	int ret;

	ret = regmap_raw_read(data->regmap, MT6379_REG_FLED_STAT1, fled_stat,
			      MT6379_FLEDST_REGSIZE);
	if (ret)
		return ret;

	if (fled_stat[0] & MT6379_FL_VIN_OVERLOAD_MASK ||
	    fled_stat[0] & MT6379_FL_VIN_LOW_MASK)
		rpt_fault |= LED_FAULT_INPUT_VOLTAGE;

	if (fled_stat[0] & MT6379_FL_OVERTEMP_MASK)
		rpt_fault |= LED_FAULT_OVER_TEMPERATURE;

	if (fled_stat[0] & MT6379_FL_LVF_STAT_MASK)
		rpt_fault |= LED_FAULT_OVER_VOLTAGE;

	if (fled_stat[1] & MT6379_FL_SHORT_STAT_MASK(mtflash->idx))
		rpt_fault |= LED_FAULT_SHORT_CIRCUIT;

	if (fled_stat[2] & MT6379_FL_STRBTO_STAT_MASK(mtflash->idx))
		rpt_fault |= LED_FAULT_TIMEOUT;

	*fault = rpt_fault;
	return 0;
}

static const struct led_flash_ops mt6379_flash_ops = {
	.flash_brightness_set	= mt6379_flash_set_brightness,
	.flash_brightness_get	= mt6379_flash_get_brightness,
	.strobe_set		= mt6379_flash_set_strobe,
	.strobe_get		= mt6379_flash_get_strobe,
	.timeout_set		= mt6379_flash_set_timeout,
	.fault_get		= mt6379_flash_get_fault,
};

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
static struct led_classdev_flash *mt6379_flash_class[MT6379_FLASH_MAX_LED];
static DEFINE_MUTEX(mt6379_mutex);
/* define usage count */
static int fd_use_count;

#define MT6379_VIN (3.6)

#if !IS_ENABLED(CONFIG_MTK_FLASHLIGHT_THERMAL)
#define FLASHLIGHT_COOLER_MAX_STATE 4
#endif

static int flash_state_to_current_limit[FLASHLIGHT_COOLER_MAX_STATE] = {
	150000, 100000, 50000, 25000
};

static int mt6379_set_scenario(int scenario)
{
	struct mt6379_flash *mtflash = container_of(
					mt6379_flash_class[MT6379_FLASH_LED1],
					struct mt6379_flash,
					flash);
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT_DLPT)
	if (scenario & FLASHLIGHT_SCENARIO_CAMERA_MASK) {
		flashlight_kicker_pbm_by_device_id(&mtflash->dev_id,
			MT6379_ISTRB_MAXUA / 1000 * MT6379_VIN);
	} else {
		flashlight_kicker_pbm_by_device_id(&mtflash->dev_id,
			MT6379_ITOR_MAXUA / 1000 * MT6379_VIN * 2);
	}
#endif

	return 0;
}

static int mt6379_open(void)
{
	struct mt6379_flash *mtflash = container_of(
					mt6379_flash_class[MT6379_FLASH_LED1],
					struct mt6379_flash,
					flash);
	mutex_lock(&mt6379_mutex);
	fd_use_count++;

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT_DLPT)
	flashlight_kicker_pbm_by_device_id(&mtflash->dev_id,
				MT6379_ITOR_MAXUA / 1000 * MT6379_VIN * 2);
	mdelay(1);
#endif

	mutex_unlock(&mt6379_mutex);

	return 0;
}

static int mt6379_release(void)
{
	struct mt6379_flash *mtflash = container_of(
					mt6379_flash_class[MT6379_FLASH_LED1],
					struct mt6379_flash,
					flash);

	mutex_lock(&mt6379_mutex);
	fd_use_count--;

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT_DLPT)
	flashlight_kicker_pbm_by_device_id(&mtflash->dev_id, 0);
#endif

	mutex_unlock(&mt6379_mutex);

	return 0;
}

static int mt6379_cooling_set_cur_state(int channel, unsigned long state)
{
	struct mt6379_flash *mtflash;
	struct mt6379_data *data;
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;
	enum led_brightness brt;

	/* Request state should be less than max_state */
	if (state > FLASHLIGHT_COOLER_MAX_STATE)
		return -EINVAL;

	if (channel < 0 || channel >= MT6379_FLASH_MAX_LED) {
		pr_info("channel error\n");
		return -EINVAL;
	}

	flcdev = mt6379_flash_class[channel];
	if (flcdev == NULL)
		return -EINVAL;

	lcdev = &flcdev->led_cdev;
	if (lcdev == NULL)
		return -EINVAL;

	mtflash = container_of(	mt6379_flash_class[channel],
				struct mt6379_flash, flash);
	data = mtflash->driver_data;
	pr_info("set thermal current:%lu torch_enabled:%lu\n",
		 state, data->torch_enabled);

	if (state == 0) {
		mtflash->need_cooler = 0;
		mtflash->target_current = MT6379_ISTRB_MAXUA;
		if (data->torch_enabled & BIT(mtflash->idx)) {
			brt = MT6379_ITOR_MAXUA;
			brt /= (u32) MT6379_ITOR_STEPUA;
			mt6379_torch_set_brightness(lcdev, brt);
		}
	} else {
		mtflash->need_cooler = 1;
		brt = flash_state_to_current_limit[state - 1];
		mtflash->target_current = brt;
		if (data->torch_enabled & BIT(mtflash->idx)) {
			brt /= (u32) MT6379_ITOR_STEPUA;
			mt6379_torch_set_brightness(lcdev, brt);
		}
	}

	return 0;
}

static int mt6379_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	if (channel >= MT6379_FLASH_MAX_LED || channel < 0)
		return -EINVAL;

	flcdev = mt6379_flash_class[channel];
	if (flcdev == NULL)
		return -EINVAL;

	lcdev = &flcdev->led_cdev;
	if (lcdev == NULL)
		return -EINVAL;

	switch (cmd) {
	case FLASH_IOC_SET_ONOFF:
		mt6379_torch_set_brightness(lcdev, (int)fl_arg->arg);
		break;
	case FLASH_IOC_SET_SCENARIO:
		mt6379_set_scenario(fl_arg->arg);
		break;
	case FLASH_IOC_SET_THERMAL_CUR_STATE:
		mt6379_cooling_set_cur_state(channel, fl_arg->arg);
		break;
	default:
		dev_info(lcdev->dev, "No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static ssize_t mt6379_strobe_store(struct flashlight_arg arg)
{
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	if (arg.channel < 0 || arg.channel >= MT6379_FLASH_MAX_LED)
		return -EINVAL;

	flcdev = mt6379_flash_class[arg.channel];
	lcdev = &flcdev->led_cdev;
	mt6379_torch_set_brightness(lcdev, LED_ON);
	msleep(arg.dur);
	mt6379_torch_set_brightness(lcdev, LED_OFF);
	return 0;
}

static int mt6379_set_driver(int set)
{
	return 0;
}

static struct flashlight_operations mt6379_ops = {
	mt6379_open,
	mt6379_release,
	mt6379_ioctl,
	mt6379_strobe_store,
	mt6379_set_driver
};


static int mt6379_init_proprietary_properties(struct fwnode_handle *fwnode,
					struct mt6379_flash *mtflash)
{
	struct led_classdev_flash *flash = &mtflash->flash;
	struct led_classdev *lcdev = &flash->led_cdev;
	int ret;

	ret = fwnode_property_read_u32(fwnode,
			"reg", &mtflash->dev_id.channel);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(fwnode,
			"type", &mtflash->dev_id.type);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(fwnode,
			"ct", &mtflash->dev_id.ct);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(fwnode,
			"part", &mtflash->dev_id.part);
	if (ret)
		return ret;

	strscpy(mtflash->dev_id.name, lcdev->dev->kobj.name,
		sizeof(mtflash->dev_id.name));

	mtflash->dev_id.decouple = 0;

	if (mtflash->dev_id.channel < 0 ||
			 mtflash->dev_id.channel >= MT6379_FLASH_MAX_LED)
		return -EINVAL;
	mt6379_flash_class[mtflash->dev_id.channel] = &mtflash->flash;

	if (flashlight_dev_register_by_device_id(&mtflash->dev_id, &mt6379_ops))
		return -EFAULT;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_V4L2_FLASH_LED_CLASS)
static int mt6379_flash_set_external_strobe(struct v4l2_flash *v4l2_flash,
					    bool state)
{
	struct led_classdev_flash *flash = v4l2_flash->fled_cdev;
	struct mt6379_flash *mtflash = (void *)flash;
	struct mt6379_data *data = mtflash->driver_data;
	struct regmap *regmap = data->regmap;
	unsigned int mask, enable = 0;
	int ret = 0, idx = mtflash->idx;

	mutex_lock(&data->lock);

	if (!(state ^ test_bit(idx, &data->strobe_enabled))) {
		dev_dbg(flash->led_cdev.dev, "no change for strobe, [%lu]\n", data->strobe_enabled);
		goto out_external_strobe_set;
	}

	if (data->torch_enabled) {
		ret = -EINVAL;
		dev_err(flash->led_cdev.dev, "Torch in used [%lu]\n", data->torch_enabled);
		goto out_external_strobe_set;
	}

	mask = MT6379_FL_CSEN_MASK(mtflash->idx);

	if (state) {
		data->strobe_enabled |= BIT(mtflash->idx);
		enable |= mask;
	} else {
		data->strobe_enabled &= ~BIT(mtflash->idx);
		enable &= ~mask;
	}

	ret = regmap_update_bits(regmap, MT6379_REG_FLED_EN, mask, enable);

out_external_strobe_set:
	mutex_unlock(&data->lock);
	return ret;
}

static const struct v4l2_flash_ops mt6379_v4l2_flash_ops = {
	.external_strobe_set = mt6379_flash_set_external_strobe,
};

static void mt6379_init_v4l2_config(struct led_classdev_flash *flash,
				    struct v4l2_flash_config *cfg)
{
	struct led_classdev *led = &flash->led_cdev;
	struct led_flash_setting *s = &cfg->intensity;

	s->min = MT6379_ITOR_MINUA;
	s->step = MT6379_ITOR_STEPUA;
	s->val = s->max = s->min + (led->max_brightness - 1) * s->step;

	cfg->has_external_strobe = 1;
	strscpy(cfg->dev_name, led->dev->kobj.name, sizeof(cfg->dev_name));
	cfg->flash_faults = LED_FAULT_TIMEOUT | LED_FAULT_SHORT_CIRCUIT |
			    LED_FAULT_OVER_VOLTAGE;
}
#else
static const struct v4l2_flash_ops mt6379_v4l2_flash_ops;
static inline void mt6379_init_v4l2_config(struct led_classdev_flash *flash,
					   struct v4l2_flash_config *cfg)
{
}
#endif

static void mt6379_v4l2_flash_release(void *d)
{
	struct mt6379_flash *mtflash = d;

	v4l2_flash_release(mtflash->v4l2);
}

static int mt6379_init_flash_properties(struct device *dev,
					struct fwnode_handle *fwnode,
					struct mt6379_flash *mtflash)
{
	struct mt6379_data *data = mtflash->driver_data;
	struct led_classdev_flash *flash = &mtflash->flash;
	struct led_classdev *led = &flash->led_cdev;
	struct led_flash_setting *s;
	u32 src, imax;
	int ret;

	ret = fwnode_property_read_u32(fwnode, "reg", &src);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get reg prop\n");

	if (src >= MT6379_FLASH_MAX_LED || data->flash_used & BIT(src))
		return dev_err_probe(dev, -EINVAL, "Invalid reg prop\n");

	mtflash->idx = src;

	imax = MT6379_ITOR_MINUA;
	ret = fwnode_property_read_u32(fwnode, "led-max-microamp", &imax);
	if (ret) {
		dev_info(dev, "read led-max-microamp property failed\n");
		return ret;
	}

	imax = clamp_val(imax, MT6379_ITOR_MINUA, MT6379_ITOR_MAXUA);
	imax = rounddown(imax - MT6379_ITOR_MINUA, MT6379_ITOR_STEPUA);
	imax += MT6379_ITOR_MINUA;

	led->max_brightness = (imax - MT6379_ITOR_MINUA) / MT6379_ITOR_STEPUA;
	led->brightness_set_blocking = mt6379_torch_set_brightness;
	led->flags |= LED_DEV_CAP_FLASH;

	flash->ops = &mt6379_flash_ops;

	imax = MT6379_ISTRB_MINUA;
	ret = fwnode_property_read_u32(fwnode, "flash-max-microamp", &imax);
	if (ret) {
		dev_info(dev, "read flash-max-microamp failed\n");
		return ret;
	}

	imax = clamp_val(imax, MT6379_ISTRB_MINUA, MT6379_ISTRB_MAXUA);
	imax = rounddown(imax - MT6379_ISTRB_MINUA, MT6379_ISTRB_STEPUA);
	imax += MT6379_ISTRB_MINUA;

	s = &flash->brightness;
	s->min = MT6379_ISTRB_MINUA;
	s->step = MT6379_ISTRB_STEPUA;
	s->val = s->max = imax;

	s = &flash->timeout;
	s->min = MT6379_STRBTO_MINUS;
	s->step = MT6379_STRBTO_STEPUS;
	s->val = s->max = MT6379_STRBTO_MAXUS;

	return 0;
}

#define MT6379_FLED_IRQH(_name) \
static irqreturn_t mt6379_##_name##_irq_handler(int irq, void *irqdata)\
{\
	struct mt6379_data *data = irqdata;\
\
	dev_err(data->dev, "%s, irq = %d\n", __func__, irq);\
	return IRQ_HANDLED;\
}

MT6379_FLED_IRQH(fled_lvf);
MT6379_FLED_IRQH(fled_bubo_vin_thro);
MT6379_FLED_IRQH(fled_thermal_thro);
MT6379_FLED_IRQH(fled_bubo_il_thro);
MT6379_FLED_IRQH(fled3_short);
MT6379_FLED_IRQH(fled2_short);
MT6379_FLED_IRQH(fled1_short);

#define MT6379_IRQ_DECLARE(_name) \
{\
	.name = #_name,\
	.irqh = mt6379_##_name##_irq_handler,\
}

struct mt6379_led_irqt {
	const char *name;
	irq_handler_t irqh;
};

static const struct mt6379_led_irqt irqts[] = {
	MT6379_IRQ_DECLARE(fled_lvf),
	MT6379_IRQ_DECLARE(fled_bubo_vin_thro),
	MT6379_IRQ_DECLARE(fled_thermal_thro),
	MT6379_IRQ_DECLARE(fled_bubo_il_thro),
	MT6379_IRQ_DECLARE(fled3_short),
	MT6379_IRQ_DECLARE(fled2_short),
	MT6379_IRQ_DECLARE(fled1_short),
};

static int mt6379_leds_irq_register(struct platform_device *pdev, void *irqdata)
{
	int i, irq, rv;

	for (i = 0; i < ARRAY_SIZE(irqts); i++) {
		irq = platform_get_irq_byname(pdev, irqts[i].name);
		if (irq <= 0)
			continue;

		rv = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					       irqts[i].irqh, 0, irqts[i].name, irqdata);
		if (rv) {
			dev_err(&pdev->dev,
				"failed to request irq [%s]\n", irqts[i].name);
			return rv;
		}
	}

	return 0;
}

static int mt6379_flash_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fwnode_handle *fwnode = dev_fwnode(dev), *child;
	struct mt6379_data *data;
	int count = 0, ret;

	fwnode_for_each_available_child_node(fwnode, child)
		count++;

	if (!count || count > MT6379_FLASH_MAX_LED)
		return -EINVAL;

	data = devm_kzalloc(dev, struct_size(data, mtflash, count), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &pdev->dev;
	mutex_init(&data->lock);

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap)
		return dev_err_probe(dev, -EINVAL, "Failed to init regmap\n");

	count = 0;
	fwnode_for_each_available_child_node(fwnode, child) {
		struct mt6379_flash *mtflash;
		struct led_init_data init_data = {};
		struct v4l2_flash_config v4l2_config = {};

		mtflash = data->mtflash + count;
		mtflash->driver_data = data;

		ret = mt6379_init_flash_properties(dev, child, mtflash);
		if (ret) {
			fwnode_handle_put(child);
			return ret;
		}

		init_data.fwnode = child;

		ret = devm_led_classdev_flash_register_ext(dev, &mtflash->flash,
							   &init_data);
		if (ret) {
			fwnode_handle_put(child);
			return dev_err_probe(dev, ret, "Failed to register flash %d\n", count);
		}

		mt6379_init_v4l2_config(&mtflash->flash, &v4l2_config);

		mtflash->v4l2 = v4l2_flash_init(dev, child, &mtflash->flash,
						&mt6379_v4l2_flash_ops,
						&v4l2_config);
		if (IS_ERR(mtflash->v4l2)) {
			fwnode_handle_put(child);
			ret = PTR_ERR(mtflash->v4l2);
			return dev_err_probe(dev, ret, "Failed to register v4l2 flash %d\n", count);
		}

		ret = devm_add_action_or_reset(dev, mt6379_v4l2_flash_release,
					       mtflash);
		if (ret) {
			fwnode_handle_put(child);
			return dev_err_probe(dev, ret, "Failed to add release action\n");
		}

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
		ret = mt6379_init_proprietary_properties(child, mtflash);
		if (ret)
			return ret;
#endif

		data->flash_used |= BIT(mtflash->idx);
		count++;
	}

	ret = mt6379_leds_irq_register(pdev, data);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to register led irqs\n");

	return 0;
}

static const struct of_device_id mt6379_flash_dt_match[] = {
	{ .compatible = "mediatek,mt6379-flash" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mt6379_flash_dt_match);

static struct platform_driver mt6379_flash_driver = {
	.driver = {
		.name = "mt6379-flash",
		.of_match_table = mt6379_flash_dt_match,
	},
	.probe = mt6379_flash_probe,
};
module_platform_driver(mt6379_flash_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6379 Flash LED Driver");
MODULE_LICENSE("GPL");
