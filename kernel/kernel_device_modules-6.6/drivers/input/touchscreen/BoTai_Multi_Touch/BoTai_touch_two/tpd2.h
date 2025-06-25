/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __tpd2_H
#define __tpd2_H
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
#define tpd2_DEBUG
#define tpd2_DEBUG_CODE
/* #define tpd2_DEBUG_TRACK */
#define tpd2_DMESG(a, arg...) \
	pr_info(tpd2_DEVICE ":[%s:%d] " a, __func__, __LINE__, ##arg)
#if defined(tpd2_DEBUG)
#undef tpd2_DEBUG
#define tpd2_DEBUG(a, arg...) \
	pr_info(tpd2_DEVICE ":[%s:%d] " a, __func__, __LINE__, ##arg)
#else
#define tpd2_DEBUG(arg...)
#endif
#define SPLIT ", "

/* register, address, configurations */
#define tpd2_DEVICE            "mtk-tpd2"
#define tpd2_X                  0
#define tpd2_Y                  1
#define tpd2_Z1                 2
#define tpd2_Z2                 3
#define TP_DELAY              (2*HZ/100)
#define TP_DRV_MAX_COUNT          (20)
#define tpd2_WARP_CNT          (4)
#define tpd2_VIRTUAL_KEY_MAX   (10)

/* various mode */
#define tpd2_MODE_NORMAL        0
#define tpd2_MODE_KEYPAD        1
#define tpd2_MODE_SW 2
#define tpd2_MODE_FAV_SW 3
#define tpd2_MODE_FAV_HW 4
#define tpd2_MODE_RAW_DATA 5
#undef tpd2_RES_X
#undef tpd2_RES_Y
extern unsigned long tpd2_RES_X;
extern unsigned long tpd2_RES_Y;
extern int tpd2_load_status;	/* 0: failed, 1: success */
extern int tpd2_mode;
extern int tpd2_mode_axis;
extern int tpd2_mode_min;
extern int tpd2_mode_max;
extern int tpd2_mode_keypad_tolerance;
extern int tpd2_em_debounce_time;
extern int tpd2_em_debounce_time0;
extern int tpd2_em_debounce_time1;
extern int tpd2_em_asamp;
extern int tpd2_em_auto_time_interval;
extern int tpd2_em_sample_cnt;
extern int tpd2_calmat[];
extern int tpd2_def_calmat[];
extern int tpd2_def_calmat[];
extern int tpd2_DO_WARP;
extern int tpd2_wb_start[];
extern int tpd2_wb_end[];
extern int tpd2_v_magnify_x;
extern int tpd2_v_magnify_y;
extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);
#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_47) || \
	defined(CONFIG_MTK_S3320_50)
extern void synaptics_init_sysfs(void);
#endif /* CONFIG_MTK_S3320 */
extern void tpd2_button_init(void);
struct tpd2_device {
	struct device *tpd2_dev;
	struct regulator *reg;
	struct regulator *io_reg;
	struct input_dev *dev;
	struct input_dev *kpd;
	struct timer_list timer;
	struct tasklet_struct tasklet;
	int btn_state;
};
struct tpd2_key_dim_local {
	int key_x;
	int key_y;
	int key_width;
	int key_height;
};

struct tpd2_filter_t {
	int enable; /*0: disable, 1: enable*/
	int pixel_density; /*XXX pixel/cm*/
	int W_W[3][4];/*filter custom setting prameters*/
	unsigned int VECLOCITY_THRESHOLD[3];/*filter speed custom settings*/
};

struct tpd2_dts_info {
	int tpd2_resolution[2];
	int touch_max_num;
	int use_tpd2_button;
	int tpd2_key_num;
	int tpd2_key_local[4];
	bool tpd2_use_ext_gpio;
	int rst_ext_gpio_num;
	struct tpd2_key_dim_local tpd2_key_dim_local[4];
	struct tpd2_filter_t touch_filter;
};
extern struct tpd2_dts_info tpd2_dts_data;
struct tpd2_attrs {
	struct device_attribute **attr;
	int num;
};
struct tpd2_driver_t {
	char *tpd2_device_name;
	int (*tpd2_local_init)(void);
	void (*suspend)(struct device *h);
	void (*resume)(struct device *h);
	int tpd2_have_button;
	struct tpd2_attrs attrs;
};


				/* #ifdef tpd2_HAVE_BUTTON */
void tpd2_button(unsigned int x, unsigned int y, unsigned int down);
void tpd2_button_init(void);
ssize_t tpd2_virtual_key(char *buf);
/* #ifndef tpd2_BUTTON_HEIGHT */
/* #define tpd2_BUTTON_HEIGHT tpd2_RES_Y */
/* #endif */


extern int tpd2_driver_add(struct tpd2_driver_t *tpd2_drv);
extern int tpd2_driver_remove(struct tpd2_driver_t *tpd2_drv);
void tpd2_button_setting(int keycnt, void *keys, void *keys_dim);
extern int tpd2_em_spl_num;
extern int tpd2_em_pressure_threshold;
extern struct tpd2_device *tpd2;
extern void tpd2_get_dts_info(void);
#define GTP_RST_PORT    0
#define GTP_INT_PORT    1
extern void tpd2_gpio_as_int(int pin);
extern void tpd2_gpio_output(int pin, int level);
extern const struct of_device_id touch_of_match[];
/*hc --------------------add */
/*
#ifdef tpd2_DEBUG_CODE
#include "tpd2_debug.h"
#endif
#ifdef tpd2_DEBUG_TRACK
int DAL_Clean(void);
int DAL_Printf(const char *fmt, ...);
int LCD_LayerEnable(int id, BOOL enable);
#endif

#ifdef tpd2_HAVE_CALIBRATION
#include "tpd2_calibrate.h"
#endif

#include "tpd2_default.h"
*/
#define tpd2_DEBUG_PRINT_INT

/*hc --------------------add */

/* switch touch panel into different mode */
void _tpd2_switch_single_mode(void);
void _tpd2_switch_multiple_mode(void);
void _tpd2_switch_sleep_mode(void);
void _tpd2_switch_normal_mode(void);

extern int tpd2_log_init(void);
extern void tpd2_log_exit(void);
extern void ilitek_suspend(void);
extern void ilitek_resume(void);
#endif
extern void tpd2_button_setting_ti941(int keycnt, void *keys);
extern void tpd2_driver_init_two(void);
extern void tpd2_driver_exit_two(void);