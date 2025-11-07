/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "tzdev_internal.h"
#include "core/log.h"
#include "ffa.h"

static struct platform_device *tzdev_pdev;

static int tzdev_probe(struct platform_device *pdev)
{
	int ret;

	(void) pdev;

	ret = tzdev_run_init_sequence();
	if (ret) {
		log_error(tzdev_platform, "tzdev initialization failed, error=%d.\n", ret);
		return ret;
	}

	log_info(tzdev_platform, "tzdev initialization done.\n");

	return 0;
}

static void tzdev_shutdown(struct platform_device *pdev)
{
	(void) pdev;

	tzdev_run_fini_sequence();

	log_info(tzdev_platform, "tzdev finalization done.\n");
}

struct platform_driver tzdev_pdrv = {
	.probe = tzdev_probe,
	.shutdown = tzdev_shutdown,

	.driver = {
		.name = "tzdev",
		.owner = THIS_MODULE,
	},
};

int tzdev_platform_register(void)
{
	int ret;

	ret = platform_driver_register(&tzdev_pdrv);
	if (ret) {
		log_error(tzdev_platform, "failed to register tzdev platform driver, error=%d\n", ret);
		return ret;
	}

	tzdev_pdev = platform_device_register_resndata(NULL, "tzdev", -1, NULL, 0, NULL, 0);
	if (IS_ERR(tzdev_pdev)) {
		ret = PTR_ERR(tzdev_pdev);
		log_error(tzdev_platform, "failed to register tzdev platform device, error=%d\n", ret);
		return ret;
	}

	return 0;
}

void tzdev_platform_unregister(void)
{
	platform_driver_unregister(&tzdev_pdrv);
}


#ifdef CONFIG_TZDEV_FFA_DRIVER_WRAPPER
int tzdev_ffa_drv_direct_msg(struct tzdev_smc_data *data);

int tzdev_platform_smc_call(struct tzdev_smc_data *data)
{
	return tzdev_ffa_drv_direct_msg(data);
}

#else
static int __tzdev_platform_smc_call(struct tzdev_smc_data *data)
{
#ifndef CONFIG_TZDEV_FFA
	data->args[0] |= TZDEV_SMC_MAGIC;
#else
	data->args[7] = data->args[4];
	data->args[6] = data->args[3];
	data->args[5] = data->args[2];
	data->args[4] = data->args[1];
	data->args[3] = data->args[0] | TZDEV_SMC_MAGIC;

	data->args[0] = FFA_MSG_SEND_DIRECT_REQ;
	data->args[1] = (tzdev_ffa_id() << 16) | tzdev_ffa_sp_id();
	data->args[2] = 0;
#endif

	__asm__ __volatile__ (
		"mov x8, %0\n"
		"ldp x0, x1, [x8]\n"
		"ldp x2, x3, [x8, #16]\n"
		"ldp x4, x5, [x8, #32]\n"
		"ldp x6, x7, [x8, #48]\n"
		"stp x8, x16, [sp, #-16]!\n"
		"smc #0\n"
		"ldp x8, x16, [sp], #16\n"
		"stp x0, x1, [x8]\n"
		"stp x2, x3, [x8, #16]\n"
		"stp x4, x5, [x8, #32]\n"
		"stp x6, x7, [x8, #48]"::
		"r" (data):
		"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", PARAM_REGISTERS,
		"memory"
	);

#ifdef CONFIG_TZDEV_FFA
	data->args[0] = data->args[3];
	data->args[1] = data->args[4];
	data->args[2] = data->args[5];
	data->args[3] = data->args[6];
	data->args[4] = data->args[7];
#endif

	return 0;
}

int tzdev_platform_smc_call(struct tzdev_smc_data *data)
{
#ifdef CONFIG_TZDEV_FFA
	int ret;

	do {
		ret = __tzdev_platform_smc_call(data);
		if (data->args[0] == FFA_INTERRUPT || data->args[0] == FFA_YIELD) {
			data->args[0] = FFA_RUN;
			data->args[2] = 0;
			data->args[3] = 0;
			data->args[4] = 0;
			data->args[5] = 0;
			data->args[6] = 0;
			data->args[7] = 0;
		}
	} while (data->args[0] == FFA_RUN);

	return ret;
#else
	return __tzdev_platform_smc_call(data);
#endif
}
#endif

void *tzdev_platform_open(void)
{
	return NULL;
}

void tzdev_platform_release(void *data)
{
	return;
}

long tzdev_platform_ioctl(void *data, unsigned int cmd, unsigned long arg)
{
	(void) data;
	(void) cmd;
	(void) arg;

	return -ENOTTY;
}
