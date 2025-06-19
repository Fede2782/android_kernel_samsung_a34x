/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __tpd1_H
#define __tpd1_H
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <generated/autoconf.h>
#include <linux/kobject.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
/*********hc_add*************/
#include <linux/of.h>
#include <linux/of_irq.h>

/*********hc_add*************/
/*debug macros */
#define tpd1_DEBUG
#define tpd1_DEBUG_CODE
/* #define tpd1_DEBUG_TRACK */
#define tpd1_DMESG(a, arg...) \
	pr_info(tpd1_DEVICE ":[%s:%d] " a, __func__, __LINE__, ##arg)
#if defined(tpd1_DEBUG)
#undef tpd1_DEBUG
#define tpd1_DEBUG(a, arg...) \
	pr_info(tpd1_DEVICE ":[%s:%d] " a, __func__, __LINE__, ##arg)
#else
#define tpd1_DEBUG(arg...)
#endif
#define SPLIT ", "

/* register, address, configurations */
#define tpd1_DEVICE            "mtk-tpd1"
#define tpd1_X                  0
#define tpd1_Y                  1
#define tpd1_Z1                 2
#define tpd1_Z2                 3
#define TP_DELAY              (2*HZ/100)
#define TP_DRV_MAX_COUNT          (20)
#define tpd1_WARP_CNT          (4)
#define tpd1_VIRTUAL_KEY_MAX   (10)

/* various mode */
#define tpd1_MODE_NORMAL        0
#define tpd1_MODE_KEYPAD        1
#define tpd1_MODE_SW 2
#define tpd1_MODE_FAV_SW 3
#define tpd1_MODE_FAV_HW 4
#define tpd1_MODE_RAW_DATA 5
#undef tpd1_RES_X
#undef tpd1_RES_Y
extern unsigned long tpd1_RES_X;
extern unsigned long tpd1_RES_Y;
extern int tpd1_load_status;	/* 0: failed, 1: success */
extern int tpd1_mode;
extern int tpd1_mode_axis;
extern int tpd1_mode_min;
extern int tpd1_mode_max;
extern int tpd1_mode_keypad_tolerance;
extern int tpd1_em_debounce_time;
extern int tpd1_em_debounce_time0;
extern int tpd1_em_debounce_time1;
extern int tpd1_em_asamp;
extern int tpd1_em_auto_time_interval;
extern int tpd1_em_sample_cnt;
extern int tpd1_calmat[];
extern int tpd1_def_calmat[];
extern int tpd1_def_calmat[];
extern int tpd1_DO_WARP;
extern int tpd1_wb_start[];
extern int tpd1_wb_end[];
extern int tpd1_v_magnify_x;
extern int tpd1_v_magnify_y;
extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);
#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_47) || \
	defined(CONFIG_MTK_S3320_50)
extern void synaptics_init_sysfs(void);
#endif /* CONFIG_MTK_S3320 */
extern void tpd1_button_init(void);
struct tpd1_device {
	struct device *tpd1_dev;
	struct regulator *reg;
	struct regulator *io_reg;
	struct input_dev *dev;
	struct input_dev *kpd;
	struct timer_list timer;
	struct tasklet_struct tasklet;
	int btn_state;
};
struct tpd1_key_dim_local {
	int key_x;
	int key_y;
	int key_width;
	int key_height;
};

struct tpd1_filter_t {
	int enable; /*0: disable, 1: enable*/
	int pixel_density; /*XXX pixel/cm*/
	int W_W[3][4];/*filter custom setting prameters*/
	unsigned int VECLOCITY_THRESHOLD[3];/*filter speed custom settings*/
};

struct tpd1_dts_info {
	int tpd1_resolution[2];
	int touch_max_num;
	int use_tpd1_button;
	int tpd1_key_num;
	int tpd1_key_local[4];
	bool tpd1_use_ext_gpio;
	int rst_ext_gpio_num;
	struct tpd1_key_dim_local tpd1_key_dim_local[4];
	struct tpd1_filter_t touch_filter;
};
extern struct tpd1_dts_info tpd1_dts_data;
struct tpd1_attrs {
	struct device_attribute **attr;
	int num;
};
struct tpd1_driver_t {
	char *tpd1_device_name;
	int (*tpd1_local_init)(void);
	void (*suspend)(struct device *h);
	void (*resume)(struct device *h);
	int tpd1_have_button;
	struct tpd1_attrs attrs;
};


				/* #ifdef tpd1_HAVE_BUTTON */
void tpd1_button(unsigned int x, unsigned int y, unsigned int down);
void tpd1_button_init(void);
ssize_t tpd1_virtual_key(char *buf);
/* #ifndef tpd1_BUTTON_HEIGHT */
/* #define tpd1_BUTTON_HEIGHT tpd1_RES_Y */
/* #endif */


extern int tpd1_driver_add(struct tpd1_driver_t *tpd1_drv);
extern int tpd1_driver_remove(struct tpd1_driver_t *tpd1_drv);
void tpd1_button_setting(int keycnt, void *keys, void *keys_dim);
extern int tpd1_em_spl_num;
extern int tpd1_em_pressure_threshold;
extern struct tpd1_device *tpd1;
extern void tpd1_get_dts_info(void);
#define GTP_RST_PORT    0
#define GTP_INT_PORT    1
extern void tpd1_gpio_as_int(int pin);
extern void tpd1_gpio_output(int pin, int level);
extern const struct of_device_id touch_of_match[];
/*hc --------------------add */
/*
#ifdef tpd1_DEBUG_CODE
#include "tpd1_debug.h"
#endif
#ifdef tpd1_DEBUG_TRACK
int DAL_Clean(void);
int DAL_Printf(const char *fmt, ...);
int LCD_LayerEnable(int id, BOOL enable);
#endif

#ifdef tpd1_HAVE_CALIBRATION
#include "tpd1_calibrate.h"
#endif

#include "tpd1_default.h"
*/
#define tpd1_DEBUG_PRINT_INT

/*hc --------------------add */

/* switch touch panel into different mode */
void _tpd1_switch_single_mode(void);
void _tpd1_switch_multiple_mode(void);
void _tpd1_switch_sleep_mode(void);
void _tpd1_switch_normal_mode(void);

extern int tpd1_log_init(void);
extern void tpd1_log_exit(void);
extern void ilitek_suspend(void);
extern void ilitek_resume(void);
#endif
extern void tpd1_button_setting_ti941(int keycnt, void *keys);
extern void tpd1_driver_init_one(void);
extern void tpd1_driver_exit_one(void);