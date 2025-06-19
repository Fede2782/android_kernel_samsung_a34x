// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>


#define I2C_I2C_LCD_BIAS_CHANNEL 6  //for I2C channel 6
#define DCDC_I2C_ID_NAME "aw37501"
#define DCDC_I2C_ADDR 0x3E

static const struct of_device_id i2c_lcm_of_match[] = {
	{.compatible = "mediatek,I2C_LCD_BIAS"},
	{},
};

/*static struct i2c_client *aw37501_i2c_client;*/
struct i2c_client *aw37501_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int aw37501_probe(struct i2c_client *client/*,  const struct i2c_device_id *id*/);
static void aw37501_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/

struct aw37501_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id aw37501_id[] = {
	{DCDC_I2C_ID_NAME, 0},
	{}
};

/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)) */
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
/* #endif */
static struct i2c_driver aw37501_iic_driver = {
	.id_table = aw37501_id,
	.probe = aw37501_probe,
	.remove = aw37501_remove,
	/* .detect               = mt6605_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = DCDC_I2C_ID_NAME,
			.of_match_table = i2c_lcm_of_match,
		   },
};

static int aw37501_probe(struct i2c_client *client/*, const struct i2c_device_id *id*/)
{
	pr_info("%s\n", __func__);
	pr_info("name=%s addr=0x%x\n", client->name, client->addr);
	aw37501_i2c_client = client;
	return 0;
}

static void aw37501_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	aw37501_i2c_client = NULL;
	i2c_unregister_device(client);
}

/*static int aw37501_write_bytes(unsigned char addr, unsigned char value)*/
int aw37501_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = aw37501_i2c_client;
	char write_data[2] = { 0 };

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("aw37501 write data fail !!\n");
	return ret;
}
EXPORT_SYMBOL(aw37501_write_bytes);

static int __init aw37501_iic_init(void)
{
	pr_info("%s\n", __func__);
	i2c_add_driver(&aw37501_iic_driver);
	return 0;
}

static void __exit aw37501_iic_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&aw37501_iic_driver);
}


module_init(aw37501_iic_init);
module_exit(aw37501_iic_exit);

MODULE_AUTHOR("Xiuhai Deng <xiuhai.deng@mediatek.com>");
MODULE_DESCRIPTION("MTK AW37501 I2C Driver");
MODULE_LICENSE("GPL");
