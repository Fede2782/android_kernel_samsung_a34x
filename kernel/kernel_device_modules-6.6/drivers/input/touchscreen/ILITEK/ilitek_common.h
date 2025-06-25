/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Luca Hsu <luca_hsu@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 */

#ifndef _ILITEK_COMMON_H_
#define _ILITEK_COMMON_H_
/* Includes of headers ------------------------------------------------------*/
#include <linux/sched.h>
#include <linux/firmware.h>

#include "ilitek_ts.h"
#include "ilitek_protocol.h"
#include "ilitek_update.h"

/* Extern define ------------------------------------------------------------*/
//driver information
#define DRIVER_VERSION_0 				5
#define DRIVER_VERSION_1 				9
#define DRIVER_VERSION_2 				1
#define DRIVER_VERSION_3				0
#define CUSTOMER_H_ID					0
#define CUSTOMER_L_ID					0
#define TEST_VERSION					0

#define ILITEK_ERR_LOG_LEVEL				1
#define ILITEK_INFO_LOG_LEVEL				3
#define ILITEK_DEBUG_LOG_LEVEL				4
#define ILITEK_DEFAULT_LOG_LEVEL			3

#define ILITEK_IOCTL_MAX_TRANSFER			5000

#define debug_level(level, fmt, arg...)							\
	do {				       						\
		if (level > ilitek_log_level_value)					\
			break;								\
		if (level == ILITEK_ERR_LOG_LEVEL)					\
			pr_info("[ILITEK][ERR][%s:%d] "fmt, __func__, __LINE__, ##arg); 	\
		else if (level == ILITEK_INFO_LOG_LEVEL)				\
			pr_info("[ILITEK][MSG][%s:%d] "fmt, __func__, __LINE__, ##arg); 	\
		else if (level == ILITEK_DEBUG_LOG_LEVEL)				\
			pr_info("[ILITEK][DBG][%s:%d] "fmt, __func__, __LINE__, ##arg); 	\
	} while (0)

#define tp_err(fmt, arg...) debug_level(ILITEK_ERR_LOG_LEVEL, fmt, ##arg)
#define tp_msg(fmt, arg...) debug_level(ILITEK_INFO_LOG_LEVEL, fmt, ##arg)
#define tp_dbg(fmt, arg...) debug_level(ILITEK_DEBUG_LOG_LEVEL, fmt, ##arg)

#define set_arr(arr, idx, val)			\
	do {					\
		if (idx < ARRAY_SIZE(arr))	\
			arr[idx] = val;		\
	} while (0)


/* i2c clock rate for rk3288 */
#if ILITEK_PLAT == ILITEK_PLAT_ROCKCHIP && \
    LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
#define SCL_RATE(rate)	.scl_rate = (rate),
#else
#define SCL_RATE(rate)
#endif

/* netlink */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#define NETLINK_KERNEL_CFG_DECLARE(cfg, func)	\
	struct netlink_kernel_cfg cfg = {	\
		.groups = 0,			\
		.input = func,			\
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#define NETLINK_KERNEL_CREATE(unit, cfg_ptr, func)	\
	netlink_kernel_create(&init_net, (unit), (cfg_ptr))
#else
#define NETLINK_KERNEL_CREATE(unit, cfg_ptr, func)	\
	netlink_kernel_create(&init_net, (unit), THIS_MODULE, (cfg_ptr))
#endif
#else
#define NETLINK_KERNEL_CFG_DECLARE(cfg, func)
#define NETLINK_KERNEL_CREATE(unit, cfg_ptr, func)	\
	netlink_kernel_create(&init_net, (unit), 0, (func), NULL, THIS_MODULE)
#endif

/* input_dev */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#define INPUT_MT_INIT_SLOTS(dev, num)	\
		input_mt_init_slots((dev), (num), INPUT_MT_DIRECT)
#else
#define INPUT_MT_INIT_SLOTS(dev, num)	input_mt_init_slots((dev), (num))
#endif

/* file_operations ioctl */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define FOPS_IOCTL	unlocked_ioctl
#define FOPS_IOCTL_FUNC(func, cmd, arg) \
		long func(struct file *fp, cmd, arg)
#else
#define FOPS_IOCTL	ioctl
#define FOPS_IOCTL_FUNC(func, cmd, arg) \
		int32_t func(struct inode *np, struct file *fp,	cmd, arg)

#endif

/* procfs */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
#define PROC_FOPS_T	file_operations
#define PROC_READ	read
#define PROC_WRITE	write
#define PROC_IOCTL	FOPS_IOCTL
#define PROC_OPEN	open
#define PROC_RELEASE	release
#else
#define PROC_FOPS_T	proc_ops
#define PROC_READ	proc_read
#define PROC_WRITE	proc_write
#define PROC_IOCTL	proc_ioctl
#define PROC_OPEN	proc_open
#define PROC_RELEASE	proc_release
#endif

#ifdef MTK_UNDTS
#define ISR_FUNC(func)	void func(void)
#define ISR_RETURN(val)
#else
#define ISR_FUNC(func)	irqreturn_t func(int irq, void *dev_id)
#define ISR_RETURN(val)	return (val)
#endif

enum ilitek_irq_handle_type {
	irq_type_normal = 0,
	irq_type_debug,
	irq_type_c_model,
};

struct ilitek_touch_info {
	uint16_t id;
	uint16_t x;
	uint16_t y;
	uint16_t p;
	uint16_t w;
	uint16_t h;
	uint16_t status;
};

struct ilitek_ts_data {
	void *client;
	struct device *device;
	struct ilitek_ts_device *dev;

	uint8_t buf[512];

	int (*process_and_report)(void);

	struct input_dev *input_dev;
	struct input_dev *pen_input_dev;
	struct regulator *vdd;
	struct regulator *vdd_i2c;
	struct regulator *vcc_io;
	struct ilitek_screen_info screen_info;   //hc
	int irq;
	int irq_gpio;
	int reset_gpio;
	int test_gpio;

	bool system_suspend;

	uint8_t irq_trigger_type;

	bool is_touched;
	bool touch_key_hold_press;
	int touch_flag[40];
	struct ilitek_touch_info tp[40];

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif

	struct task_struct *update_thread;

	atomic_t firmware_updating;
	bool operation_protection;
	bool unhandle_irq;
	unsigned int irq_handle_type;
	unsigned int irq_read_len;

	uint8_t gesture_status;
	uint8_t low_power_status;

	bool esd_check;
	bool esd_skip;
	struct workqueue_struct *esd_workq;
	struct delayed_work esd_work;
	unsigned long esd_delay;

	struct mutex ilitek_mutex;

	atomic_t irq_enabled;
	atomic_t get_INT;

	bool wake_irq_enabled;

	bool irq_registerred;
};
/* Extern macro -------------------------------------------------------------*/
#define CEIL(n, d) ((n % d) ? (n / d) + 1 : (n / d ))
/* Extern variables ---------------------------------------------------------*/

extern char driver_ver[];

extern int ilitek_log_level_value;

extern struct ilitek_ts_data *ts;

#ifdef ILITEK_TUNING_MESSAGE
extern bool ilitek_debug_flag;
#endif
/* Extern function prototypes -----------------------------------------------*/
/* Extern functions ---------------------------------------------------------*/
void ilitek_resume(void);
void ilitek_suspend(void);
int ilitek_main_probe(void *client, struct device *dev);
//int ilitek_main_remove(void *client);
void ilitek_main_remove(void *client);
void ilitek_reset(int delay);

int ilitek_write(uint8_t *cmd, int len);
int ilitek_read(uint8_t *buf, int len);
int ilitek_write_and_read(uint8_t *cmd, int w_len, int delay_ms,
			  uint8_t *buf, int r_len);

void ilitek_irq_enable(void);
void ilitek_irq_disable(void);

int ilitek_upgrade_firmware(char *filename);

int ilitek_create_tool_node(void);
int ilitek_remove_tool_node(void);

int ilitek_create_sysfsnode(void);
void ilitek_remove_sys_node(void);

void ilitek_gpio_dbg(void);

void ilitek_register_gesture(struct ilitek_ts_data *ts, bool init);

int ilitek_create_esd_check_workqueue(void);
void ilitek_remove_esd_check_workqueue(void);

#endif
