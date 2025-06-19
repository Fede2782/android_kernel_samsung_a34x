// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Jitao Shi <jitao.shi@mediatek.com>
 */

#ifndef BUILD_LK
#include <linux/interrupt.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/module.h>

#include "nt50358a_i2c.h"

/* I2C Slave Setting */
#define nt50358a_SLAVE_ADDR_WRITE	0x7C

static struct i2c_client *new_client;
static const struct i2c_device_id nt50358a_i2c_id[] = { {"nt50358a", 0}, {} };

static int nt50358a_driver_probe(struct i2c_client *client);
static void nt50358a_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id nt50358a_id[] = {
	{.compatible = "nt50358a"},
	{},
};

MODULE_DEVICE_TABLE(of, nt50358a_id);
#endif

static struct i2c_driver nt50358a_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "nt50358a",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(nt50358a_id),
#endif
	},
	.probe = nt50358a_driver_probe,
	.remove = nt50358a_driver_remove,
	.id_table = nt50358a_i2c_id,
};

static DEFINE_MUTEX(nt50358a_i2c_access);

/* I2C Function For Read/Write */
int nt50358a_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&nt50358a_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&nt50358a_i2c_access);
		return 0;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&nt50358a_i2c_access);

	return 1;
}
EXPORT_SYMBOL(nt50358a_read_byte);

int nt50358a_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_notice("[KE/NT50358A] %s\n", __func__);

	mutex_lock(&nt50358a_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&nt50358a_i2c_access);
		pr_notice("[NT50358A] I2C write fail!!!\n");

		return 0;
	}

	mutex_unlock(&nt50358a_i2c_access);

	return 1;
}
EXPORT_SYMBOL(nt50358a_write_byte);

static int nt50358a_driver_probe(struct i2c_client *client)
{
	int err = 0;

	pr_notice("[KE/NT50358A] name=%s addr=0x%x\n",
		client->name, client->addr);
	new_client = devm_kzalloc(&client->dev,
		sizeof(struct i2c_client), GFP_KERNEL);
	if (!new_client) {
		err = -ENOMEM;
		goto exit;
	}

	memset(new_client, 0, sizeof(struct i2c_client));
	new_client = client;

	return 0;

 exit:
	return err;

}

static void nt50358a_driver_remove(struct i2c_client *client)
{
	pr_notice("[KE/NT50358A] %s\n", __func__);

	new_client = NULL;
	i2c_unregister_device(client);
}

#define RT4801_BUSNUM 1

static int __init nt50358a_init(void)
{
	pr_notice("[KE/NT50358A] %s\n", __func__);

	if (i2c_add_driver(&nt50358a_driver) != 0)
		pr_notice("[KE/NT50358A] failed to register nt50358a i2c driver.\n");
	else
		pr_notice("[KE/NT50358A] Success to register nt50358a i2c driver.\n");

	return 0;
}

static void __exit nt50358a_exit(void)
{
	i2c_del_driver(&nt50358a_driver);
}

module_init(nt50358a_init);
module_exit(nt50358a_exit);
MODULE_LICENSE("GPL");
#endif

