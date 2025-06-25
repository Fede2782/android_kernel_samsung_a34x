/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_LOW_BATTERY_THROTTLING_H__
#define __MTK_LOW_BATTERY_THROTTLING_H__

enum LOW_BATTERY_LEVEL_TAG {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2,
	LOW_BATTERY_LEVEL_3 = 3,
	LOW_BATTERY_LEVEL_4 = 4,
	LOW_BATTERY_LEVEL_5 = 5,
	LOW_BATTERY_LEVEL_NUM
};

enum LOW_BATTERY_PRIO_TAG {
	LOW_BATTERY_PRIO_CPU_B = 0,
	LOW_BATTERY_PRIO_CPU_L = 1,
	LOW_BATTERY_PRIO_GPU = 2,
	LOW_BATTERY_PRIO_MD = 3,
	LOW_BATTERY_PRIO_MD5 = 4,
	LOW_BATTERY_PRIO_FLASHLIGHT = 5,
	LOW_BATTERY_PRIO_VIDEO = 6,
	LOW_BATTERY_PRIO_WIFI = 7,
	LOW_BATTERY_PRIO_BACKLIGHT = 8,
	LOW_BATTERY_PRIO_DLPT = 9,
	LOW_BATTERY_PRIO_UFS = 10,
	LOW_BATTERY_PRIO_UT = 15
};

enum LOW_BATTERY_USER_TAG {
	LBAT_INTR_1 = 0,
	LVSYS_INTR = 1,
	PPB = 2,
	UT = 3,
	LBAT_INTR_2 = 4,
	HPT = 5,
	LOW_BATTERY_USER_NUM
};

enum LOW_BATTERY_INTR_TAG {
	INTR_1 = 0,
	INTR_2 = 1,
	INTR_MAX_NUM
};

/* boot type definitions */
enum boot_mode_t {
	NORMAL_BOOT = 0,
	META_BOOT = 1,
	RECOVERY_BOOT = 2,
	SW_REBOOT = 3,
	FACTORY_BOOT = 4,
	ADVMETA_BOOT = 5,
	ATE_FACTORY_BOOT = 6,
	ALARM_BOOT = 7,
	KERNEL_POWER_OFF_CHARGING_BOOT = 8,
	LOW_POWER_OFF_CHARGING_BOOT = 9,
	DONGLE_BOOT = 10,
	UNKNOWN_BOOT
};

struct lbat_mbrain {
	enum LOW_BATTERY_USER_TAG user;
	unsigned int thd_volt;
	unsigned int level;
	int soc;
	int bat_temp;
	unsigned int temp_stage;
};

typedef void (*low_battery_callback)(enum LOW_BATTERY_LEVEL_TAG tag, void *data);
typedef void (*low_battery_mbrain_callback)(struct lbat_mbrain lbat_mbrain);


#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val, void *data);
int register_low_battery_mbrain_cb(low_battery_mbrain_callback lb_mbrain_cb);
int lbat_set_ppb_mode(unsigned int mode);
int lbat_set_hpt_mode(unsigned int enable);
#endif

#endif /* __MTK_LOW_BATTERY_THROTTLING_H__ */
