// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd1.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include "mtk_disp_notify.h"
#include <linux/notifier.h>

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mtk_6306_gpio.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_50) \
	|| defined(CONFIG_MTK_S3320_47) || defined(CONFIG_MTK_MIT200) \
	|| defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) \
	|| defined(CONFIG_MTK_S7020) \
	|| defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
#include <linux/input/mt.h>
#endif /* CONFIG_MTK_S3320 */
/* for magnify velocity******************************************** */
#define TOUCH_IOC_MAGIC 'A'

#define tpd1_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC, 0)
#define tpd1_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC, 1)
#define tpd1_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, 2, struct tpd1_filter_t)
#ifdef CONFIG_COMPAT
#define COMPAT_tpd1_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, \
						2, struct tpd1_filter_t)
#endif

unsigned long tpd1_RES_X;
unsigned long tpd1_RES_Y;
int tpd1_load_status;	/* 0: failed, 1: success */
int tpd1_mode;
int tpd1_mode_axis;
int tpd1_mode_min;
int tpd1_mode_max;
int tpd1_mode_keypad_tolerance;
int tpd1_em_debounce_time;
int tpd1_em_debounce_time0;
int tpd1_em_debounce_time1;
int tpd1_em_asamp;
int tpd1_em_auto_time_interval;
int tpd1_em_sample_cnt;
int tpd1_debug_time;

//int tpd1_def_calmat[];
int tpd1_calmat[];
int tpd1_def_calmat[];
//int tpd1_DO_WARP;
//int tpd1_wb_start[];
//int tpd1_wb_end[];
int tpd1_v_magnify_x;
int tpd1_v_magnify_y;
int tpd1_def_calmat[8] = { 0 };
int tpd1_calmat[8] = { 0 };

struct tpd1_filter_t tpd1_filter;
struct tpd1_dts_info tpd1_dts_data;
struct pinctrl *pinctrl1;
struct pinctrl_state *pins_default;
struct pinctrl_state *eint_as_int, *eint_output0,
		*eint_output1, *rst_output0, *rst_output1;
const struct of_device_id touch_of_match[] = {
	{.compatible = "mediatek,cap_touch_one"},
	{},
};

void tpd1_get_dts_info(void)
{
	struct device_node *node1 = NULL;
	int key_dim_local[16], i;

	node1 = of_find_matching_node(node1, touch_of_match);
	if (node1) {
		of_property_read_u32(node1,
			"tpd1-max-touch-num", &tpd1_dts_data.touch_max_num);
		of_property_read_u32(node1,
			"use-tpd1-button", &tpd1_dts_data.use_tpd1_button);
		pr_debug("[tpd1]use-tpd1-button = %d\n",
			tpd1_dts_data.use_tpd1_button);
		if (of_property_read_u32_array(node1, "tpd1-resolution",
			tpd1_dts_data.tpd1_resolution,
			ARRAY_SIZE(tpd1_dts_data.tpd1_resolution))) {
			pr_debug("[tpd1] resulution is %d %d",
				tpd1_dts_data.tpd1_resolution[0],
				tpd1_dts_data.tpd1_resolution[1]);
		}
		if (tpd1_dts_data.use_tpd1_button) {
			of_property_read_u32(node1,
				"tpd1-key-num", &tpd1_dts_data.tpd1_key_num);
			if (of_property_read_u32_array(node1, "tpd1-key-local",
				tpd1_dts_data.tpd1_key_local,
				ARRAY_SIZE(tpd1_dts_data.tpd1_key_local)))
				pr_debug("tpd1-key-local: %d %d %d %d",
					tpd1_dts_data.tpd1_key_local[0],
					tpd1_dts_data.tpd1_key_local[1],
					tpd1_dts_data.tpd1_key_local[2],
					tpd1_dts_data.tpd1_key_local[3]);
			if (of_property_read_u32_array(node1,
				"tpd1-key-dim-local",
				key_dim_local, ARRAY_SIZE(key_dim_local))) {
				memcpy(tpd1_dts_data.tpd1_key_dim_local,
					key_dim_local, sizeof(key_dim_local));
				for (i = 0; i < 4; i++) {
					pr_debug("[tpd1]key[%d].key_x = %d\n", i,
						tpd1_dts_data
							.tpd1_key_dim_local[i]
							.key_x);
					pr_debug("[tpd1]key[%d].key_y = %d\n", i,
						tpd1_dts_data
							.tpd1_key_dim_local[i]
							.key_y);
					pr_debug("[tpd1]key[%d].key_W = %d\n", i,
						tpd1_dts_data
							.tpd1_key_dim_local[i]
							.key_width);
					pr_debug("[tpd1]key[%d].key_H = %d\n", i,
						tpd1_dts_data
							.tpd1_key_dim_local[i]
							.key_height);
				}
			}
		}
		of_property_read_u32(node1, "tpd1-filter-enable",
			&tpd1_dts_data.touch_filter.enable);
		if (tpd1_dts_data.touch_filter.enable) {
			of_property_read_u32(node1,
				"tpd1-filter-pixel-density",
				&tpd1_dts_data.touch_filter.pixel_density);
			if (of_property_read_u32_array(node1,
				"tpd1-filter-custom-prameters",
				(u32 *)tpd1_dts_data.touch_filter.W_W,
				ARRAY_SIZE(tpd1_dts_data.touch_filter.W_W)))
				pr_debug("get tpd1-filter-custom-parameters");
			if (of_property_read_u32_array(node1,
				"tpd1-filter-custom-speed",
				tpd1_dts_data.touch_filter.VECLOCITY_THRESHOLD,
				ARRAY_SIZE(tpd1_dts_data
						.touch_filter
						.VECLOCITY_THRESHOLD)))
				pr_debug("get tpd1-filter-custom-speed");
		}
		memcpy(&tpd1_filter,
			&tpd1_dts_data.touch_filter, sizeof(tpd1_filter));
		pr_debug("[tpd1]tpd1-filter-enable = %d, pixel_density = %d\n",
				tpd1_filter.enable, tpd1_filter.pixel_density);
		tpd1_dts_data.tpd1_use_ext_gpio =
			of_property_read_bool(node1, "tpd1-use-ext-gpio");
		of_property_read_u32(node1,
			"tpd1-rst-ext-gpio-num",
			&tpd1_dts_data.rst_ext_gpio_num);

	} else {
		tpd1_DMESG("can't find touch compatible custom node\n");
	}
}

static DEFINE_MUTEX(tpd1_set_gpio_mutex);
void tpd1_gpio_as_int(int pin)
{
	mutex_lock(&tpd1_set_gpio_mutex);
	tpd1_DEBUG("[tpd1]%s\n", __func__);
	if (pin == 1)
		pinctrl_select_state(pinctrl1, eint_as_int);
	mutex_unlock(&tpd1_set_gpio_mutex);
}

void tpd1_gpio_output(int pin, int level)
{
	mutex_lock(&tpd1_set_gpio_mutex);
	tpd1_DEBUG("%s pin = %d, level = %d\n", __func__, pin, level);
	if (pin == 1) {
		if (level)
			pinctrl_select_state(pinctrl1, eint_output1);
		else
			pinctrl_select_state(pinctrl1, eint_output0);
	} else {
		if (tpd1_dts_data.tpd1_use_ext_gpio) {
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
			mt6306_set_gpio_dir(
				tpd1_dts_data.rst_ext_gpio_num, 1);
			mt6306_set_gpio_out(
				tpd1_dts_data.rst_ext_gpio_num, level);
#endif
		} else {
			if (level)
				pinctrl_select_state(pinctrl1, rst_output1);
			else
				pinctrl_select_state(pinctrl1, rst_output0);
		}
	}
	mutex_unlock(&tpd1_set_gpio_mutex);
}
int tpd1_get_gpio_info(struct platform_device *pdev)
{
	int ret;

	tpd1_DEBUG("[tpd1 %d] mt_tpd1_pinctrl+++++++++++++++++\n", pdev->id);
	pinctrl1 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl1)) {
		ret = PTR_ERR(pinctrl1);
		dev_info(&pdev->dev, "fwq Cannot find pinctrl1!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		tpd1_DMESG("Cannot find pinctrl default %d!\n", ret);
	}
	eint_as_int = pinctrl_lookup_state(pinctrl1, "state_eint_as_int");
	if (IS_ERR(eint_as_int)) {
		ret = PTR_ERR(eint_as_int);
		tpd1_DMESG("Cannot find pinctrl state_eint_as_int!\n");
		return ret;
	}
	eint_output0 = pinctrl_lookup_state(pinctrl1, "state_eint_output0");
	if (IS_ERR(eint_output0)) {
		ret = PTR_ERR(eint_output0);
		tpd1_DMESG("Cannot find pinctrl state_eint_output0!\n");
		return ret;
	}
	eint_output1 = pinctrl_lookup_state(pinctrl1, "state_eint_output1");
	if (IS_ERR(eint_output1)) {
		ret = PTR_ERR(eint_output1);
		tpd1_DMESG("Cannot find pinctrl state_eint_output1!\n");
		return ret;
	}
	if (tpd1_dts_data.tpd1_use_ext_gpio == false) {
		rst_output0 =
			pinctrl_lookup_state(pinctrl1, "state_rst_output0");
		if (IS_ERR(rst_output0)) {
			ret = PTR_ERR(rst_output0);
			tpd1_DMESG("Cannot find pinctrl state_rst_output0!\n");
			return ret;
		}
		rst_output1 =
			pinctrl_lookup_state(pinctrl1, "state_rst_output1");
		if (IS_ERR(rst_output1)) {
			ret = PTR_ERR(rst_output1);
			tpd1_DMESG("Cannot find pinctrl state_rst_output1!\n");
			return ret;
		}
	}
	tpd1_DEBUG("[tpd1%d] mt_tpd1_pinctrl----------\n", pdev->id);
	return 0;
}

static int tpd1_misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int tpd1_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long tpd1_compat_ioctl(
			struct file *file, unsigned int cmd,
			unsigned long arg)
{
	long ret;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_tpd1_GET_FILTER_PARA:
		if (arg32 == NULL) {
			pr_info("invalid argument.");
			return -EINVAL;
		}
		ret = file->f_op->unlocked_ioctl(file, tpd1_GET_FILTER_PARA,
					   (unsigned long)arg32);
		if (ret) {
			pr_info("tpd1_GET_FILTER_PARA unlocked_ioctl failed.");
			return ret;
		}
		break;
	default:
		pr_info("tpd1: unknown IOCTL: 0x%08x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
static long tpd1_unlocked_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	/* char strbuf[256]; */
	void __user *data;

	long err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(
			(void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(
			(void __user *)arg, _IOC_SIZE(cmd));
	if (err) {
		pr_info("tpd1: access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case tpd1_GET_VELOCITY_CUSTOM_X:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd1_v_magnify_x, sizeof(tpd1_v_magnify_x))) {
			err = -EFAULT;
			break;
		}

		break;

	case tpd1_GET_VELOCITY_CUSTOM_Y:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd1_v_magnify_y, sizeof(tpd1_v_magnify_y))) {
			err = -EFAULT;
			break;
		}

		break;
	case tpd1_GET_FILTER_PARA:
			data = (void __user *) arg;

			if (data == NULL) {
				err = -EINVAL;
				tpd1_DMESG("GET_FILTER_PARA: data is null\n");
				break;
			}

			if (copy_to_user(data, &tpd1_filter,
					sizeof(struct tpd1_filter_t))) {
				tpd1_DMESG("GET_FILTER_PARA: copy data error\n");
				err = -EFAULT;
				break;
			}
			break;
	default:
		pr_info("tpd1: unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}
static struct work_struct touch_resume_work;
static struct workqueue_struct *touch_resume_workqueue;
static const struct file_operations tpd1_fops = {
/* .owner = THIS_MODULE, */
	.open = tpd1_misc_open,
	.release = tpd1_misc_release,
	.unlocked_ioctl = tpd1_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tpd1_compat_ioctl,
#endif
};

/*---------------------------------------------------------------------------*/
static struct miscdevice tpd1_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tpd1_DEVICE",
	.fops = &tpd1_fops,
};

/* ********************************************** */
/* #endif */


/* function definitions */
static int __init tpd1_device_init_one(void);
static void __exit tpd1_device_exit_one(void);
static int tpd1_probe(struct platform_device *pdev);
static int tpd1_remove(struct platform_device *pdev);
static struct work_struct tpd1_init_work;
static struct workqueue_struct *tpd1_init_workqueue;
static int tpd1_suspend_flag;
int tpd1_register_flag;
/* global variable definitions */
struct tpd1_device *tpd1;
static struct tpd1_driver_t tpd1_driver_list[TP_DRV_MAX_COUNT];	/* = {0}; */

struct platform_device tpd1_device = {
	.name		= tpd1_DEVICE,
	.id			= -1,
};
const struct dev_pm_ops tpd1_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
};
static struct platform_driver tpd1_driver = {
	.remove = tpd1_remove,
	.shutdown = NULL,
	.probe = tpd1_probe,
	.driver = {
			.name = tpd1_DEVICE,
			.pm = &tpd1_pm_ops,
			.owner = THIS_MODULE,
			.of_match_table = touch_of_match,
	},
};
static struct tpd1_driver_t *g_tpd1_drv;
/* hh: use fb_notifier */
static struct notifier_block tpd1_fb_notifier;
/* use fb_notifier */
static void touch_resume_workqueue_callback(struct work_struct *work)
{
	tpd1_DEBUG("GTP %s\n", __func__);
	//ilitek_resume();
	tpd1_suspend_flag = 0;
}

static int tpd1_fb_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *v)
{
	int *data = (int *)v;
	int err = 0;

	tpd1_DEBUG("%s\n", __func__);
	pr_info("tpd1_fb_notifier_callback event=[%lu]\n", event);

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != MTK_DISP_EVENT_BLANK)
		return 0;

	if (*data == MTK_DISP_BLANK_UNBLANK) {
		pr_info("LCD ON Notify\n");
		if (tpd1_suspend_flag) {
			err = queue_work(touch_resume_workqueue,
						&touch_resume_work);
			if (!err) {
				pr_info("start resume_workqueue failed\n");
				return err;
			}
		}
	} else if (*data == MTK_DISP_BLANK_POWERDOWN) {
		pr_info("LCD OFF Notify\n");
		if (!tpd1_suspend_flag) {
			err = cancel_work_sync(&touch_resume_work);
			if (!err)
				pr_info("cancel resume_workqueue failed\n");
			//ilitek_suspend();
		}
		tpd1_suspend_flag = 1;
	}

	pr_info("%s-\n", __func__);
	return 0;
}

/* Add driver: if find tpd1_TYPE_CAPACITIVE driver successfully, loading it */
int tpd1_driver_add(struct tpd1_driver_t *tpd1_drv)
{
	int i;
	if (g_tpd1_drv != NULL) {
		tpd1_DMESG("touch driver exist\n");
		return -1;
	}
	/* check parameter */
	if (tpd1_drv == NULL)
		return -1;
	tpd1_drv->tpd1_have_button = tpd1_dts_data.use_tpd1_button;
	/* R-touch */
	if (strcmp(tpd1_drv->tpd1_device_name, "generic") == 0) {
		tpd1_driver_list[0].tpd1_device_name = tpd1_drv->tpd1_device_name;
		tpd1_driver_list[0].tpd1_local_init = tpd1_drv->tpd1_local_init;
		tpd1_driver_list[0].suspend = tpd1_drv->suspend;
		tpd1_driver_list[0].resume = tpd1_drv->resume;
		tpd1_driver_list[0].tpd1_have_button = tpd1_drv->tpd1_have_button;
		return 0;
	}
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd1 driver into list */
		if (tpd1_driver_list[i].tpd1_device_name == NULL) {
			tpd1_driver_list[i].tpd1_device_name =
				tpd1_drv->tpd1_device_name;
			tpd1_driver_list[i].tpd1_local_init =
				tpd1_drv->tpd1_local_init;
			tpd1_driver_list[i].suspend = tpd1_drv->suspend;
			tpd1_driver_list[i].resume = tpd1_drv->resume;
			tpd1_driver_list[i].tpd1_have_button =
				tpd1_drv->tpd1_have_button;
			tpd1_driver_list[i].attrs = tpd1_drv->attrs;
			break;
		}
		if (strcmp(tpd1_driver_list[i].tpd1_device_name,
			tpd1_drv->tpd1_device_name) == 0){
			return 1;	/* driver exist */
			}
	}
	return 0;
}

int tpd1_driver_remove(struct tpd1_driver_t *tpd1_drv)
{
	int i = 0;
	/* check parameter */
	if (tpd1_drv == NULL)
		return -1;
	for (i = 0; i < TP_DRV_MAX_COUNT; i++) {
		/* find it */
		if (strcmp(tpd1_driver_list[i].tpd1_device_name,
				tpd1_drv->tpd1_device_name) == 0) {
			memset(&tpd1_driver_list[i], 0,
				sizeof(struct tpd1_driver_t));
			break;
		}
	}
	return 0;
}
/*
static void tpd1_create_attributes(struct device *dev, struct tpd1_attrs *attrs)
{
	int num = attrs->num;

	for (; num > 0;) {
		if (device_create_file(dev, attrs->attr[--num]))
			pr_info("mtk_tpd1: tpd1 create attributes file failed\n");
	}
}
*/

/* touch panel probe */
static int tpd1_probe(struct platform_device *pdev)
{
	int touch_type = 1;	/* 0:R-touch, 1: Cap-touch */
	int i = 0;
#ifndef CONFIG_CUSTOM_LCM_X
#ifdef CONFIG_LCM_WIDTH
	unsigned long tpd1_res_x = 0, tpd1_res_y = 0;
	int ret = 0;
#endif
#endif

	tpd1_DMESG("enter %s, %d\n", __func__, __LINE__);
	if (misc_register(&tpd1_misc_device))
		pr_info("mtk_tpd1: tpd1_misc_device register failed\n");
	tpd1_get_gpio_info(pdev);
	tpd1 = kmalloc(sizeof(struct tpd1_device), GFP_KERNEL);
	if (tpd1 == NULL)
		return -ENOMEM;
	memset(tpd1, 0, sizeof(struct tpd1_device));
	/* allocate input device */
	tpd1->dev = input_allocate_device();
	if (tpd1->dev == NULL) {
		kfree(tpd1);
		return -ENOMEM;
	}
	/* tpd1_RES_X = simple_strtoul(LCM_WIDTH, NULL, 0); */
	/* tpd1_RES_Y = simple_strtoul(LCM_HEIGHT, NULL, 0); */

/*
	#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION
	if (strncmp(CONFIG_MTK_LCM_PHYSICAL_ROTATION, "90", 2) == 0
		|| strncmp(CONFIG_MTK_LCM_PHYSICAL_ROTATION, "270", 3) == 0) {
#ifdef CONFIG_MTK_FB
		tpd1_RES_Y = DISP_GetScreenWidth();
		tpd1_RES_X = DISP_GetScreenHeight();
#endif
	} else
    #endif
	{
#ifdef CONFIG_CUSTOM_LCM_X
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_FB) && defined(CONFIG_MTK_LCM)
		tpd1_RES_X = DISP_GetScreenWidth();
		tpd1_RES_Y = DISP_GetScreenHeight();
#else
	tpd1_RES_X = tpd1_dts_data.tpd1_resolution[0];
	tpd1_RES_Y = tpd1_dts_data.tpd1_resolution[1];
#endif
#endif
#else
#ifdef CONFIG_LCM_WIDTH
		ret = kstrtoul(CONFIG_LCM_WIDTH, 0, &tpd1_res_x);
		if (ret < 0) {
			pr_info("Touch down get lcm_x failed");
			return ret;
		}
		tpd1_RES_X = tpd1_res_x;
		ret = kstrtoul(CONFIG_LCM_HEIGHT, 0, &tpd1_res_x);
		if (ret < 0) {
			pr_info("Touch down get lcm_y failed");
			return ret;
		}
		tpd1_RES_Y = tpd1_res_y;
#endif
#endif
	}

	if (2560 == tpd1_RES_X)
		tpd1_RES_X = 2048;
	if (1600 == tpd1_RES_Y)
		tpd1_RES_Y = 1536;
*/
	tpd1_RES_X = tpd1_dts_data.tpd1_resolution[0];
	tpd1_RES_Y = tpd1_dts_data.tpd1_resolution[1];
	pr_debug("mtk_tpd1: tpd1_RES_X = %lu, tpd1_RES_Y = %lu\n",tpd1_RES_X, tpd1_RES_Y);
	tpd1_mode = tpd1_MODE_NORMAL;
	/* struct input_dev dev initialization and registration */
	tpd1->dev->name = tpd1_DEVICE;
	tpd1->dev->phys = "gt9xx_1/input0";
	set_bit(EV_ABS, tpd1->dev->evbit);
	set_bit(EV_KEY, tpd1->dev->evbit);
	set_bit(ABS_X, tpd1->dev->absbit);
	set_bit(ABS_Y, tpd1->dev->absbit);
	set_bit(ABS_PRESSURE, tpd1->dev->absbit);
#if !defined(CONFIG_MTK_S3320) && !defined(CONFIG_MTK_S3320_47)\
	&& !defined(CONFIG_MTK_S3320_50) && !defined(CONFIG_MTK_MIT200) \
	&& !defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) \
	&& !defined(CONFIG_MTK_S7020) \
	&& !defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
	set_bit(BTN_TOUCH, tpd1->dev->keybit);
#endif /* CONFIG_MTK_S3320 */
	set_bit(INPUT_PROP_DIRECT, tpd1->dev->propbit);
	/* save dev for regulator_get() before tpd1_local_init() */
	tpd1->tpd1_dev = &pdev->dev;
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd1 driver into list */
		if (tpd1_driver_list[i].tpd1_device_name != NULL) {
			tpd1_driver_list[i].tpd1_local_init();
			/* msleep(1); */
			if (tpd1_load_status == 1) {
				tpd1_DMESG("%s, tpd1_driver_name=%s\n", __func__,
					  tpd1_driver_list[i].tpd1_device_name);
				g_tpd1_drv = &tpd1_driver_list[i];
				break;
			}
		}
	}
	if (g_tpd1_drv == NULL) {
		if (tpd1_driver_list[0].tpd1_device_name != NULL) {
			g_tpd1_drv = &tpd1_driver_list[0];
			/* touch_type:0: r-touch, 1: C-touch */
			touch_type = 0;
			g_tpd1_drv->tpd1_local_init();
			tpd1_DMESG("Generic touch panel driver\n");
		} else {
			tpd1_DMESG("no touch driver is loaded!!\n");
			return 0;
		}
	}
	touch_resume_workqueue = create_singlethread_workqueue("touch_resume");
	INIT_WORK(&touch_resume_work, touch_resume_workqueue_callback);
	/* use fb_notifier */
	tpd1_fb_notifier.notifier_call = tpd1_fb_notifier_callback;
	if (mtk_disp_notifier_register("Touch_driver", &tpd1_fb_notifier))
		tpd1_DMESG("register mtk_disp_notifier fail!\n");
	/* tpd1_TYPE_CAPACITIVE handle */
	if (touch_type == 1) {
		set_bit(ABS_MT_TRACKING_ID, tpd1->dev->absbit);
		set_bit(ABS_MT_TOUCH_MAJOR, tpd1->dev->absbit);
		set_bit(ABS_MT_TOUCH_MINOR, tpd1->dev->absbit);
		set_bit(ABS_MT_POSITION_X, tpd1->dev->absbit);
		set_bit(ABS_MT_POSITION_Y, tpd1->dev->absbit);
		input_set_abs_params(tpd1->dev,
			ABS_MT_POSITION_X, 0, tpd1_RES_X, 0, 0);
		input_set_abs_params(tpd1->dev,
			ABS_MT_POSITION_Y, 0, tpd1_RES_Y, 0, 0);
#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_47) \
	|| defined(CONFIG_MTK_S3320_50) || defined(CONFIG_MTK_MIT200) \
	|| defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) \
	|| defined(CONFIG_MTK_S7020) \
	|| defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
		input_set_abs_params(tpd1->dev,
		ABS_MT_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(tpd1->dev,
			ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
		input_set_abs_params(tpd1->dev, ABS_MT_WIDTH_MINOR, 0, 15, 0, 0);
		input_mt_init_slots(tpd1->dev, 10, 0);
#else
		input_set_abs_params(tpd1->dev,
			ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
		input_set_abs_params(tpd1->dev,
			ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
#endif
		tpd1_DMESG("Cap touch panel driver\n");
	}
	//input_set_abs_params(tpd1->dev, ABS_MT_POSITION_X, 0, tpd1_RES_X, 0, 0);
	input_set_abs_params(tpd1->dev, ABS_MT_POSITION_X, 0, tpd1_RES_X, 0, 0);
	input_set_abs_params(tpd1->dev, ABS_MT_POSITION_Y, 0, tpd1_RES_Y, 0, 0);
	input_abs_set_res(tpd1->dev, ABS_MT_POSITION_X, tpd1_RES_X);
	input_abs_set_res(tpd1->dev, ABS_MT_POSITION_Y, tpd1_RES_Y);

	input_set_abs_params(tpd1->dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(tpd1->dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	if (input_register_device(tpd1->dev))
		tpd1_DMESG("input_register_device failed.(tpd1)\n");
	else
		tpd1_register_flag = 1;
	/*if (g_tpd1_drv->tpd1_have_button)
		tpd1_button_init();

	if (g_tpd1_drv->attrs.num)
		tpd1_create_attributes(&pdev->dev, &g_tpd1_drv->attrs);*/

	return 0;
}
static int tpd1_remove(struct platform_device *pdev)
{
	input_unregister_device(tpd1->dev);
	return 0;
}

/* called when loaded into kernel */
static void tpd1_init_work_callback(struct work_struct *work)
{
	tpd1_DEBUG("MediaTek touch panel driver init\n");
	if (platform_driver_register(&tpd1_driver) != 0){
		tpd1_DMESG("unable to register touch panel driver.\n");
	}
}
static int __init tpd1_device_init_one(void)
{
	int res = 0;
	/* load touch driver first  */
	tpd1_driver_init_one();
	//tpd1_log_init();
	///tpd1_init_workqueue = create_singlethread_workqueue("mtk-tpd1");
	tpd1_init_workqueue = create_singlethread_workqueue(tpd1_DEVICE);
	INIT_WORK(&tpd1_init_work, tpd1_init_work_callback);

	res = queue_work(tpd1_init_workqueue, &tpd1_init_work);
	if (!res)
		pr_info("tpd1 : touch device init failed res:%d\n", res);
	return 0;
}
/* should never be called */
static void __exit tpd1_device_exit_one(void)
{
	tpd1_DMESG("MediaTek touch panel driver exit\n");
	//tpd1_log_exit();
	tpd1_driver_exit_one();
	/* input_unregister_device(tpd1->dev); */
	misc_deregister(&tpd1_misc_device);
	mtk_disp_notifier_unregister(&tpd1_fb_notifier);
	cancel_work_sync(&tpd1_init_work);
	destroy_workqueue(tpd1_init_workqueue);
	platform_driver_unregister(&tpd1_driver);
	tpd1_DEBUG("Touch exit done\n");
}

late_initcall(tpd1_device_init_one);
module_exit(tpd1_device_exit_one);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_AUTHOR("Kirby Wu<kirby.wu@mediatek.com>");
