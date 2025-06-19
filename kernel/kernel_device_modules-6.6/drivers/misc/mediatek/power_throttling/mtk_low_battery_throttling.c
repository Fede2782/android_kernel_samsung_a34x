// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"
#include "pmic_lvsys_notify.h"

#define CREATE_TRACE_POINTS
#include "mtk_low_battery_throttling_trace.h"
#include "../mbraink/mbraink_ioctl_struct_def.h"

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <../../drivers/battery/common/sec_charging_common.h>
#endif

#define LBCB_MAX_NUM 16
#define TEMP_MAX_STAGE_NUM 6
#define THD_VOLTS_LENGTH 30
#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100
#define POWER_INT3_VOLT 2700
#define LVSYS_THD_VOLT_H 3100
#define LVSYS_THD_VOLT_L 2900
#define MAX_INT 0x7FFFFFFF
#define MIN_LBAT_VOLT 2000
#define LBAT_PMIC_MAX_LEVEL LOW_BATTERY_LEVEL_3
#define LBAT_PMIC_LEVEL_NUM (LOW_BATTERY_LEVEL_3+1)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define VOLT_L_STR "thd-volts-l"
#define VOLT_H_STR "thd-volts-h"

struct lbat_intr_tbl {
	unsigned int volt;
	unsigned int ag_volt;
	unsigned int lt_en;
	unsigned int lt_lv;
	unsigned int ht_en;
	unsigned int ht_lv;
};

struct lbat_thd_tbl {
	unsigned int *thd_volts;
	unsigned int *ag_thd_volts;
	int thd_volts_size;
	struct lbat_intr_tbl *lbat_intr_info;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct lbat_thl_priv {
	struct tag_bootmode *tag;
	bool notify_flag;
	struct wait_queue_head notify_waiter;
	struct timer_list notify_timer;
	struct task_struct *notify_thread;
	struct device *dev;
	unsigned int bat_type;
	int lbat_thl_stop;
	int lbat_thd_modify;
	unsigned int lvsys_thd_volt_l;
	unsigned int lvsys_thd_volt_h;
	unsigned int ppb_mode;
	unsigned int hpt_mode;
	unsigned int pt_shutdown_en;
	struct work_struct psy_work;
	struct power_supply *psy;
	int *temp_thd;
	int temp_max_stage;
	int temp_cur_stage;
	int temp_reg_stage;
	int *aging_thd;
	int *aging_volts;
	int aging_max_stage;
	int aging_cur_stage;
	unsigned int *aging_factor_volts;
	unsigned int max_thl_lv[INTR_MAX_NUM];
	unsigned int lbat_intr_num;
	unsigned int lbat_lv[INTR_MAX_NUM];    /* charger pmic(vbat) notify level */
	unsigned int l_lbat_lv;                /* largest charger pmic notify level */
	unsigned int lvsys_lv;                 /* main pmic(vsys) notify level */
	unsigned int l_pmic_lv;                /* largest pmic level for charger and main pmic */
	unsigned int *thl_lv;                  /* pmic notify level to throttle level mapping table */
	unsigned int cur_thl_lv;               /* current throttle level to each module */
	unsigned int cur_cg_thl_lv;            /* current cpu/gpu throttle level to each module */
	unsigned int thl_cnt[LOW_BATTERY_USER_NUM][LOW_BATTERY_LEVEL_NUM];
	struct lbat_mbrain lbat_mbrain_info;
	struct lbat_user *lbat_pt[INTR_MAX_NUM];
	struct lbat_thd_tbl lbat_thd[INTR_MAX_NUM][TEMP_MAX_STAGE_NUM];
};

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG, void *data);
	void *data;
};

static struct notifier_block lbat_nb;
static struct notifier_block bp_nb;
static struct lbat_thl_priv *lbat_data;
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0}, {0} };
static low_battery_mbrain_callback lb_mbrain_cb;
#if defined(CONFIG_MTK_NO_BAT_BOOT_SUPPORT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
static int low_battery_f_mode;
#endif
#endif
static DEFINE_MUTEX(exe_thr_lock);

static int rearrange_volt(struct lbat_intr_tbl *intr_info, unsigned int *volt_l, unsigned int *volt_h,
	unsigned int num)
{
	unsigned int idx_l = 0, idx_h = 0, idx_t = 0, i;
	unsigned int volt_l_next, volt_h_next;

	for (i = 0; i < num - 1; i++) {
		if (volt_l[i] < volt_l[i+1] || volt_h[i] < volt_h[i+1]) {
			pr_notice("[%s] i=%d volt_l(%d, %d) volt_h(%d, %d) error\n",
				__func__, i, volt_l[i], volt_l[i+1], volt_h[i], volt_h[i+1]);
			return -EINVAL;
		}
	}
	for (i = 0; i < num * 2; i++) {
		volt_l_next = (idx_l < num) ? volt_l[idx_l] : 0;
		volt_h_next = (idx_h < num) ? volt_h[idx_h] : 0;
		if (volt_l_next > volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt = volt_l_next;
			intr_info[idx_t].ag_volt = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			idx_l++;
			idx_t++;
		} else if (volt_l_next == volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt = volt_l_next;
			intr_info[idx_t].ag_volt = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_l++;
			idx_h++;
			idx_t++;
		} else if (volt_h_next > 0) {
			intr_info[idx_t].volt = volt_h_next;
			intr_info[idx_t].ag_volt = volt_h_next;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_h++;
			idx_t++;
		} else
			break;
	}
	for (i = 0; i < idx_t; i++) {
		pr_info("[%s] intr_info[%d] = (v:%d ag_v:%d, trig l[%d %d] h[%d %d])\n",
			__func__, i, intr_info[i].volt, intr_info[i].ag_volt, intr_info[i].lt_en,
			intr_info[i].lt_lv, intr_info[i].ht_en, intr_info[i].ht_lv);
	}
	return idx_t;
}

static void __used dump_thd_volts(struct device *dev, unsigned int *thd_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	dev_notice(dev, "%s Done\n", str);
}

static void __used dump_thd_volts_ext(unsigned int *thd_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	pr_info("%s Done\n", str);
}

int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val, void *data)
{
	if (prio_val >= LBCB_MAX_NUM) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			  __func__, prio_val);
		return -EINVAL;
	}

	if (!lb_cb)
		return -EINVAL;

	if (lbcb_tb[prio_val].lbcb != 0)
		pr_info("[%s] Notice: LBCB has been registered\n", __func__);

	lbcb_tb[prio_val].lbcb = lb_cb;
	lbcb_tb[prio_val].data = data;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return 3;
	}

	if (lbat_data->cur_thl_lv && lbcb_tb[prio_val].lbcb) {
		lbcb_tb[prio_val].lbcb(lbat_data->cur_thl_lv, lbcb_tb[prio_val].data);
		pr_info("[%s] notify lv=%d\n", __func__, lbat_data->cur_thl_lv);
	}
	return 3;
}
EXPORT_SYMBOL(register_low_battery_notify);

int register_low_battery_mbrain_cb(low_battery_mbrain_callback cb)
{
	if (!cb)
		return -EINVAL;

	lb_mbrain_cb = cb;

	return 0;
}
EXPORT_SYMBOL(register_low_battery_mbrain_cb);

void exec_throttle(unsigned int thl_level, enum LOW_BATTERY_USER_TAG user, unsigned int thd_volt, unsigned int input)
{
	int i;

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return;
	}

	if (user == HPT && thl_level != lbat_data->cur_thl_lv) {
		for (i = 0; i <= LOW_BATTERY_PRIO_GPU; i++) {
			if (lbcb_tb[i].lbcb)
				lbcb_tb[i].lbcb(thl_level, lbcb_tb[i].data);
		}
		lbat_data->cur_cg_thl_lv = thl_level;
		trace_low_battery_cg_throttling_level(lbat_data->cur_cg_thl_lv);
		pr_info("[%s] [decide_and_throttle] user=%d input=%d thl_lv=%d, volt=%d cur_thl_lv/cg_thl_lv=%d, %d\n",
			__func__, user, input, thl_level, thd_volt, lbat_data->cur_thl_lv, lbat_data->cur_cg_thl_lv);
		return;
	}

	if (lbat_data->cur_thl_lv == thl_level && lbat_data->cur_cg_thl_lv == thl_level) {
		pr_info("[%s] same throttle thl_level=%d\n", __func__, thl_level);
		return;
	}

	lbat_data->lbat_mbrain_info.user = user;
	lbat_data->lbat_mbrain_info.level = thl_level;
	lbat_data->lbat_mbrain_info.thd_volt = thd_volt;
	lbat_data->cur_thl_lv = thl_level;

	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb) {
			if ((lbat_data->hpt_mode > 0 && i > LOW_BATTERY_PRIO_GPU) || !lbat_data->hpt_mode)
				lbcb_tb[i].lbcb(lbat_data->cur_thl_lv, lbcb_tb[i].data);
		}
	}
	trace_low_battery_throttling_level(lbat_data->cur_thl_lv);
	if (!lbat_data->hpt_mode && lbat_data->cur_cg_thl_lv != thl_level) {
		lbat_data->cur_cg_thl_lv = thl_level;
		trace_low_battery_cg_throttling_level(lbat_data->cur_cg_thl_lv);
	}

	if (lb_mbrain_cb)
		lb_mbrain_cb(lbat_data->lbat_mbrain_info);

	lbat_data->thl_cnt[user][thl_level] += 1;

	pr_info("[%s] [decide_and_throttle] user=%d input=%d thl_lv=%d, volt=%d cur_thl_lv/cg_thl_lv=%d, %d\n",
		__func__, user, input, thl_level, thd_volt, lbat_data->cur_thl_lv, lbat_data->cur_cg_thl_lv);
}

static unsigned int convert_to_thl_lv(enum LOW_BATTERY_USER_TAG intr_type, unsigned int temp_stage,
	unsigned int input_lv)
{
	unsigned int thl_lv = input_lv;

	if (!lbat_data || !lbat_data->thl_lv) {
		pr_info("can't find throttle level table, use pmic level=%d\n", thl_lv);
		return thl_lv;
	}

	if (intr_type == LBAT_INTR_1 || intr_type == LBAT_INTR_2) {
		if (input_lv < LBAT_PMIC_LEVEL_NUM  && temp_stage <= lbat_data->temp_max_stage)
			thl_lv = lbat_data->thl_lv[temp_stage * LBAT_PMIC_LEVEL_NUM + input_lv];
		else
			pr_info("%s:Out of boundary: intr_type=%d pmic_lv=%d temp_stage=%d return %d\n", __func__,
				intr_type, input_lv, temp_stage, input_lv);
	} else if (intr_type == LVSYS_INTR) {
		if (input_lv && temp_stage <= lbat_data->temp_max_stage)
			thl_lv = lbat_data->thl_lv[temp_stage * LBAT_PMIC_LEVEL_NUM + LBAT_PMIC_MAX_LEVEL];
		else if (temp_stage > lbat_data->temp_max_stage)
			pr_info("%s:Out of boundary: intr_type=%d pmic_lv=%d temp_stage=%d return %d\n", __func__,
				intr_type, input_lv, temp_stage, input_lv);
	} else if (intr_type == UT)
		thl_lv = input_lv;

	return thl_lv;
}

/* decide_and_throttle(): for multiple user to trigger PT, arbitrate PT throttle level then execute it
 *   user:
 user want to throttle modules
 *       LBAT_INTR: charger pmic low battery interrupt
 *       LVSYS_INTR: main pmic lvsys interrupt
 *       PPB: ppb module to enable/disable PT
 *       UT: user command to force throttle level
 *   input: user input parameters
 *       for LBAT_INTR and LVSYS_INTR: pmic notify level (lbat 0~3, lvsys 0~1)
 *       for PPB: ppb mode
 *       for UT: throttle level
 */
static int __used decide_and_throttle(enum LOW_BATTERY_USER_TAG user, unsigned int input, unsigned int thd_volt)
{
	struct lbat_thd_tbl *thd_info[INTR_MAX_NUM];
	unsigned int low_thd_volts[LBAT_PMIC_LEVEL_NUM] = {MIN_LBAT_VOLT+40, MIN_LBAT_VOLT+30, MIN_LBAT_VOLT+20,
		MIN_LBAT_VOLT+10};
	int temp_cur_stage = 0;
	unsigned int lbat_thl_lv = 0, lvsys_thl_lv = 0;

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return -ENODATA;
	}

	mutex_lock(&exe_thr_lock);
	if (user == LBAT_INTR_1 || user == LBAT_INTR_2) {
		if (user == LBAT_INTR_1)
			lbat_data->lbat_lv[INTR_1] = input;
		else if (user == LBAT_INTR_2)
			lbat_data->lbat_lv[INTR_2] = input;

		lbat_data->l_lbat_lv = MAX(lbat_data->lbat_lv[INTR_1], lbat_data->lbat_lv[INTR_2]);

		if (lbat_data->lbat_thl_stop > 0 || lbat_data->ppb_mode == 1) {
			pr_info("[%s] user=%d input=%d not apply, stop/ppb=%d/%d\n", __func__,
				user, input, lbat_data->lbat_thl_stop, lbat_data->ppb_mode);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			lbat_data->l_pmic_lv = MAX(lbat_data->l_lbat_lv,
				lbat_data->lvsys_lv ? LBAT_PMIC_MAX_LEVEL : 0);
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
		}
		mutex_unlock(&exe_thr_lock);
	} else if (user == LVSYS_INTR) {
		lbat_data->lvsys_lv = input;
		if (lbat_data->lbat_thl_stop > 0 || lbat_data->ppb_mode == 1) {
			pr_info("[%s] user=%d input=%d not apply, stop=%d, ppb_mode=%d\n", __func__,
				user, input, lbat_data->lbat_thl_stop, lbat_data->ppb_mode);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			lbat_data->l_pmic_lv = MAX(lbat_data->l_lbat_lv,
				lbat_data->lvsys_lv ? LBAT_PMIC_MAX_LEVEL : 0);
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
		}
		mutex_unlock(&exe_thr_lock);
	} else if (user == PPB) {
		lbat_data->ppb_mode = input;
		if (lbat_data->lbat_thl_stop > 0) {
			pr_info("[%s] user=%d input=%d not apply, stop=%d\n", __func__, user, input,
				lbat_data->lbat_thl_stop);
			mutex_unlock(&exe_thr_lock);
		} else if (lbat_data->ppb_mode == 1) {
			exec_throttle(LOW_BATTERY_LEVEL_0, user, thd_volt, input);
			mutex_unlock(&exe_thr_lock);
			lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1], &low_thd_volts[0],
				LBAT_PMIC_LEVEL_NUM);
#ifdef LBAT2_ENABLE
			if (lbat_data->lbat_intr_num == 2) {
				dual_lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_2], &low_thd_volts[0],
					LBAT_PMIC_LEVEL_NUM);
			}
#endif
			dump_thd_volts_ext(&low_thd_volts[0], LBAT_PMIC_LEVEL_NUM);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
			temp_cur_stage = lbat_data->temp_cur_stage;
			thd_info[INTR_1] = &lbat_data->lbat_thd[INTR_1][temp_cur_stage];
			thd_info[INTR_2] = &lbat_data->lbat_thd[INTR_2][temp_cur_stage];
			lbat_data->temp_reg_stage = temp_cur_stage;
			mutex_unlock(&exe_thr_lock);
			lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1],
				thd_info[INTR_1]->ag_thd_volts, thd_info[INTR_1]->thd_volts_size);
			dump_thd_volts_ext(thd_info[INTR_1]->ag_thd_volts, thd_info[INTR_1]->thd_volts_size);
#ifdef LBAT2_ENABLE
			if (lbat_data->lbat_intr_num == 2) {
				dual_lbat_user_modify_thd_ext_locked( lbat_data->lbat_pt[INTR_2],
					thd_info[INTR_2]->ag_thd_volts, thd_info[INTR_2]->thd_volts_size);
				dump_thd_volts_ext(thd_info[INTR_2]->ag_thd_volts, thd_info[INTR_2]->thd_volts_size);
			}
#endif
		}
	} else if (user == HPT) {
		lbat_data->hpt_mode = input;
		if (lbat_data->lbat_thl_stop > 0) {
			pr_info("[%s] user=%d input=%d not apply, stop=%d\n", __func__, user, input,
				lbat_data->lbat_thl_stop);
			mutex_unlock(&exe_thr_lock);
		} else if (lbat_data->hpt_mode > 0) {
			exec_throttle(LOW_BATTERY_LEVEL_0, user, thd_volt, input);
			mutex_unlock(&exe_thr_lock);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
			mutex_unlock(&exe_thr_lock);
		}
	} else if (user == UT) {
		lbat_data->lbat_thl_stop = 1;
		exec_throttle(input, user, thd_volt, input);
		mutex_unlock(&exe_thr_lock);
	} else {
		mutex_unlock(&exe_thr_lock);
	}

	return 0;
}

static int lbat_thd_to_lv(unsigned int thd, unsigned int temp_stage, enum LOW_BATTERY_USER_TAG intr_type)
{
	unsigned int i, level = 0;
	struct lbat_intr_tbl *info;
	struct lbat_thd_tbl *thd_info;

	if (!lbat_data)
		return 0;

	if (intr_type == LBAT_INTR_1)
		thd_info = &lbat_data->lbat_thd[INTR_1][temp_stage];
	else if (intr_type == LBAT_INTR_2)
		thd_info = &lbat_data->lbat_thd[INTR_2][temp_stage];
	else {
		pr_notice("[%s] wrong intr_type=%d\n", __func__, intr_type);
		return -1;
	}

	for (i = 0; i < thd_info->thd_volts_size; i++) {
		info = &thd_info->lbat_intr_info[i];
		if (thd == thd_info->ag_thd_volts[i]) {
			if (info->ht_en == 1)
				level = info->ht_lv;
			else if (info->lt_en == 1)
				level = info->lt_lv;
			break;
		}
	}

	if (i == thd_info->thd_volts_size) {
		pr_notice("[%s] wrong threshold=%d\n", __func__, thd);
		return -1;
	}

	return level;
}

void exec_low_battery_callback(unsigned int thd)
{
	unsigned int lbat_lv = 0;

#if defined(CONFIG_MTK_NO_BAT_BOOT_SUPPORT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	if (low_battery_f_mode == OB_MODE) {
		pr_info("[%s] force return for no bat boot support\n", __func__);
		return;
	}
#endif
#endif

	if (!lbat_data) {
		pr_info("[%s] lbat_data not allocate\n", __func__);
		return;
	}

	lbat_lv = lbat_thd_to_lv(thd, lbat_data->temp_reg_stage, LBAT_INTR_1);
	if (lbat_lv == -1)
		return;

	decide_and_throttle(LBAT_INTR_1, lbat_lv, thd);
}

void exec_dual_low_battery_callback(unsigned int thd)
{
	unsigned int lbat_lv = 0;

	if (!lbat_data) {
		pr_info("[%s] lbat_data not allocate\n", __func__);
		return;
	}

	lbat_lv = lbat_thd_to_lv(thd, lbat_data->temp_reg_stage, LBAT_INTR_2);
	if (lbat_lv == -1)
		return;

	decide_and_throttle(LBAT_INTR_2, lbat_lv, thd);
}

int lbat_set_ppb_mode(unsigned int mode)
{
	decide_and_throttle(PPB, mode, 0);
	return 0;
}
EXPORT_SYMBOL(lbat_set_ppb_mode);

int lbat_set_hpt_mode(unsigned int enable)
{
	decide_and_throttle(HPT, enable, 0);
	return 0;
}
EXPORT_SYMBOL(lbat_set_hpt_mode);

/*****************************************************************************
 * low battery throttle cnt
 ******************************************************************************/
static ssize_t low_battery_throttle_cnt_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	unsigned int len = 0;
	int i = 0, j = 0;
	int user[] = {LBAT_INTR_1, LVSYS_INTR};


	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(user); i++) {
		len += snprintf(buf + len, PAGE_SIZE, "user%d: ",i);
		for ( j = 0; j < LOW_BATTERY_LEVEL_NUM; j++)
			len += snprintf(buf + len, PAGE_SIZE, "%d ", lbat_data->thl_cnt[i][j]);
	}

	return len;
}

static DEVICE_ATTR_RO(low_battery_throttle_cnt);

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "cur_thl_lv=%d\n", lbat_data->cur_thl_lv);
	return sprintf(buf, "%u\n", lbat_data->cur_thl_lv);
}

static ssize_t low_battery_protect_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (val > LOW_BATTERY_LEVEL_5) {
		dev_info(dev, "wrong number (%d)\n", val);
		return size;
	}
	dev_info(dev, "your input is %d\n", val);
	decide_and_throttle(UT, val, 0);

	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_ut);

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t low_battery_protect_stop_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "lbat_thl_stop=%d\n", lbat_data->lbat_thl_stop);
	return sprintf(buf, "%u\n", lbat_data->lbat_thl_stop);
}

static ssize_t low_battery_protect_stop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1)) {
		dev_info(dev, "stop value not correct\n");
		return size;
	}

	lbat_data->lbat_thl_stop = val;
	dev_info(dev, "lbat_thl_stop=%d\n", lbat_data->lbat_thl_stop);
	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_dbg(dev, "cur_thl_lv=%d cur_cg_thl_lv=%d\n", lbat_data->cur_thl_lv, lbat_data->cur_cg_thl_lv);
	return sprintf(buf, "%u, %u\n", lbat_data->cur_thl_lv,lbat_data->cur_cg_thl_lv);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	dev_dbg(dev, "cur_thl_lv=%d\n", lbat_data->cur_thl_lv);
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_level);

/*****************************************************************************
 * low battery modify threshold
 ******************************************************************************/
static ssize_t low_battery_modify_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lbat_thd_tbl *thd_info;
	int len = 0, i;

	len += snprintf(buf + len, PAGE_SIZE, "modify enable: %d\n", lbat_data->lbat_thd_modify);

	for (i = 0; i < lbat_data->lbat_intr_num; i++) {
		thd_info = &lbat_data->lbat_thd[i][0];
		len += snprintf(buf + len, PAGE_SIZE - len, "volts intr%d: %d %d %d %d\n",
			i + 1, thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_0],
			thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_1],
			thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_2],
			thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_3]);
	}

	return len;
}

static ssize_t low_battery_modify_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int volt_t_size = 0, j = 0;
	unsigned int thd_0, thd_1, thd_2, thd_3;
	unsigned int volt_l[LBAT_PMIC_MAX_LEVEL], volt_h[LBAT_PMIC_MAX_LEVEL];
	int intr_no;
	struct lbat_thd_tbl *thd_info;

	if (sscanf(buf, "%u %u %u %u %u\n", &intr_no, &thd_0, &thd_1, &thd_2, &thd_3) != 5) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (thd_0 <= 0 || thd_1 <= 0 || thd_2 <= 0 || thd_3 <= 0 || intr_no < 1 || intr_no > INTR_MAX_NUM) {
		dev_info(dev, "invalid input\n");
		return -EINVAL;
	}

	volt_l[0] = thd_1;
	volt_l[1] = thd_2;
	volt_l[2] = thd_3;
	volt_h[0] = thd_0;
	volt_h[1] = thd_1;
	volt_h[2] = thd_2;

	thd_info = &lbat_data->lbat_thd[intr_no-1][0];
	volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0], LBAT_PMIC_MAX_LEVEL);

	if (volt_t_size <= 0) {
		dev_notice(dev, "[%s] Failed to rearrange_volt\n", __func__);
		return -ENODATA;
	}

	thd_info->thd_volts_size = volt_t_size;
	for (j = 0; j < volt_t_size; j++)
		thd_info->ag_thd_volts[j] = thd_info->lbat_intr_info[j].ag_volt;

	dump_thd_volts(dev, thd_info->ag_thd_volts, thd_info->thd_volts_size);

	if (intr_no == 2 && lbat_data->lbat_intr_num == 2) {
#ifdef LBAT2_ENABLE
		dual_lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[intr_no-1],
			thd_info->ag_thd_volts, thd_info->thd_volts_size);
#endif
	} else if (intr_no == 1) {
		lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[intr_no-1],
			thd_info->ag_thd_volts, thd_info->thd_volts_size);
	}

	lbat_data->lbat_thd_modify = 0;
	lbat_data->temp_cur_stage = 0;
	lbat_data->temp_reg_stage = 0;
	dev_notice(dev, "modify_enable: %d, temp_cur_stage = %d\n", lbat_data->lbat_thd_modify,
		lbat_data->temp_cur_stage);

	return size;
}
static DEVICE_ATTR_RW(low_battery_modify_threshold);

static int lbat_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	if (!lbat_data)
		return NOTIFY_DONE;

	lbat_data->psy = v;
	schedule_work(&lbat_data->psy_work);
	return NOTIFY_DONE;
}

static int check_duplicate(unsigned int *volt_thd)
{
	int i, j;

	for (i = 0; i < LBAT_PMIC_MAX_LEVEL; i++) {
		for (j = i + 1; j < LBAT_PMIC_MAX_LEVEL; j++) {
			if (volt_thd[i] == volt_thd[j]) {
				pr_notice("[%s] volt_thd duplicate = %d\n", __func__, volt_thd[i]);
				return -1;
			}
		}
	}
	return 0;
}

static int __used pt_check_power_off(void)
{
	int ret = 0, pt_power_off_lv = LBAT_PMIC_MAX_LEVEL;
	static int pt_power_off_cnt;
	struct lbat_thd_tbl *thd_info;

	if (!lbat_data)
		return 0;

	thd_info = &lbat_data->lbat_thd[INTR_1][lbat_data->temp_reg_stage];
	pt_power_off_lv = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
#ifdef LBAT2_ENABLE
	if (lbat_data->lbat_intr_num == 2) {
		thd_info =
			&lbat_data->lbat_thd[INTR_2][lbat_data->temp_reg_stage];
		lbat2_level = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
		pt_power_off_lv = MAX(pt_power_off_lv, lbat2_level)
	}
#endif
	if (!lbat_data->tag)
		return 0;

	if (lbat_data->cur_thl_lv == pt_power_off_lv &&
		lbat_data->tag->bootmode != KERNEL_POWER_OFF_CHARGING_BOOT) {
		if (pt_power_off_cnt == 0)
			ret = 0;
		else
			ret = 1;
		pt_power_off_cnt++;
		pr_info("[%s] %d ret:%d\n", __func__, pt_power_off_cnt, ret);
	} else
		pt_power_off_cnt = 0;

	if (pt_power_off_cnt >= 4) {
		pr_info("Powering off by PT.\n");
		kernel_power_off();
	}

	return ret;
}

static void __used pt_set_shutdown_condition(void)
{
	static struct power_supply *bat_psy;
	union power_supply_propval prop;
	int ret;

	bat_psy = power_supply_get_by_name("mtk-gauge");
	if (!bat_psy || IS_ERR(bat_psy)) {
		bat_psy = devm_power_supply_get_by_phandle(lbat_data->dev, "gauge");
		if (!bat_psy || IS_ERR(bat_psy)) {
			pr_info("%s psy is not rdy\n", __func__);
			return;
		}

	}

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_PRESENT,
					&prop);
	if (!ret && prop.intval == 0) {
		/* gauge enabled */
		prop.intval = 1;
		ret = power_supply_set_property(bat_psy, POWER_SUPPLY_PROP_ENERGY_EMPTY,
						&prop);
		if (ret)
			pr_info("%s fail\n", __func__);
	}
}

static int pt_notify_handler(void *unused)
{
	do {
		wait_event_interruptible(lbat_data->notify_waiter,
			(lbat_data->notify_flag == true));

		if (pt_check_power_off()) {
			/* notify battery driver to power off by SOC=0 */
			pt_set_shutdown_condition();
			pr_info("[PT] notify battery SOC=0 to power off.\n");
		}
		mod_timer(&lbat_data->notify_timer, jiffies + HZ * 20);
		lbat_data->notify_flag = false;
	} while (!kthread_should_stop());
	return 0;
}

static void pt_timer_func(struct timer_list *t)
{
	lbat_data->notify_flag = true;
	wake_up_interruptible(&lbat_data->notify_waiter);
}

int pt_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int ret = 0, soc = 2;
	struct power_supply *psy = v;
	union power_supply_propval val;

	if (!lbat_data) {
		pr_info("[%s] lbat_data not init\n", __func__);
		return NOTIFY_DONE;
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (strcmp(psy->desc->name, "mtk-fg-battery") != 0)
#else
	if (strcmp(psy->desc->name, "battery") != 0)
#endif
		return NOTIFY_DONE;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;
	soc = val.intval;
	lbat_data->lbat_mbrain_info.soc = soc;

	if (lbat_data->pt_shutdown_en) {
		if (soc <= 1 && soc >= 0 && !timer_pending(&lbat_data->notify_timer)) {
			mod_timer(&lbat_data->notify_timer, jiffies);
		} else if (soc < 0 || soc > 1) {
			if (timer_pending(&lbat_data->notify_timer))
				del_timer_sync(&lbat_data->notify_timer);
		}
	}

	return NOTIFY_DONE;
}

static void pt_notify_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	np = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
	if (!np)
		dev_notice(&pdev->dev, "get bootmode fail\n");
	else {
		lbat_data->tag =
			(struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
		if (!lbat_data->tag)
			dev_notice(&pdev->dev, "failed to get atag,boot\n");
		else
			dev_notice(&pdev->dev, "bootmode:0x%x\n", lbat_data->tag->bootmode);
	}

	init_waitqueue_head(&lbat_data->notify_waiter);
	timer_setup(&lbat_data->notify_timer, pt_timer_func, TIMER_DEFERRABLE);
	lbat_data->notify_thread = kthread_run(pt_notify_handler, 0,
					 "pt_notify_thread");
	if (IS_ERR(lbat_data->notify_thread)) {
		pr_notice("Failed to create notify_thread\n");
		return;
	}
	bp_nb.notifier_call = pt_psy_event;
	ret = power_supply_reg_notifier(&bp_nb);
	if (ret) {
		kthread_stop(lbat_data->notify_thread);
		pr_notice("power_supply_reg_notifier fail\n");
		return;
	}
}

static void psy_handler(struct work_struct *work)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, temp, temp_stage, temp_thd, cycle = 0, i, aging_stage = 0;
	static int last_temp = MAX_INT;
	bool loop;
	struct lbat_thd_tbl *thd_info;
	unsigned int pre_thl_lv, cur_thl_lv, ag_offset, thl_lv_idx;

	if (!lbat_data)
		return;

	if (!lbat_data->psy)
		return;

	psy = lbat_data->psy;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (strcmp(psy->desc->name, "mtk-fg-battery") != 0)
#else
	if (strcmp(psy->desc->name, "battery") != 0)
#endif
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;

	temp = val.intval / 10;
#ifdef LBAT2_ENABLE
	temp = val.intval; // because of battery bug, remove me if battery driver fix
#endif
	lbat_data->lbat_mbrain_info.bat_temp = temp;
	temp_stage = lbat_data->temp_cur_stage;

	do {
		loop = false;
		if (temp < last_temp) {
			if (temp_stage < lbat_data->temp_max_stage) {
				temp_thd = lbat_data->temp_thd[temp_stage];
				if (temp < temp_thd) {
					temp_stage++;
					loop = true;
				}
			}
		} else if (temp > last_temp) {
			if (temp_stage > 0) {
				temp_thd = lbat_data->temp_thd[temp_stage-1];
				if (temp >= temp_thd) {
					temp_stage--;
					loop = true;
				}
			}
		}
	} while (loop);

	last_temp = temp;

	if (lbat_data->aging_max_stage > 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
		if (!ret)
			cycle = val.intval;

		for (i = 0; i < lbat_data->aging_max_stage; i++) {
			if (cycle < lbat_data->aging_thd[i])
				break;
		}
		aging_stage = i;
	}

	if ((temp_stage <= lbat_data->temp_max_stage && temp_stage != lbat_data->temp_cur_stage) ||
		(aging_stage <= lbat_data->aging_max_stage && aging_stage != lbat_data->aging_cur_stage)) {
		if (lbat_data->ppb_mode != 1 && !lbat_data->lbat_thd_modify) {
			thl_lv_idx = lbat_data->temp_cur_stage * LBAT_PMIC_LEVEL_NUM + lbat_data->l_pmic_lv;
			pre_thl_lv = lbat_data->thl_lv[thl_lv_idx];
			thl_lv_idx = temp_stage * LBAT_PMIC_LEVEL_NUM + lbat_data->l_pmic_lv;
			cur_thl_lv = lbat_data->thl_lv[thl_lv_idx];
			if (pre_thl_lv != cur_thl_lv) {
				lbat_data->temp_cur_stage = temp_stage;
				lbat_data->aging_cur_stage = aging_stage;
				decide_and_throttle(LBAT_INTR_1, lbat_data->lbat_lv[INTR_1], 0);
			}

			if (aging_stage == 0)
				ag_offset = 0;
			else
				ag_offset = lbat_data->aging_volts[aging_stage-1];

			thd_info = &lbat_data->lbat_thd[INTR_1][temp_stage];
			for (i = 0; i < thd_info->thd_volts_size; i++) {
				thd_info->ag_thd_volts[i] = thd_info->thd_volts[i] + ag_offset;
				thd_info->lbat_intr_info[i].ag_volt = thd_info->lbat_intr_info[i].volt + ag_offset;
			}

			lbat_data->temp_reg_stage = temp_stage;
			lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1],
				thd_info->ag_thd_volts, thd_info->thd_volts_size);
			dump_thd_volts_ext(thd_info->ag_thd_volts, thd_info->thd_volts_size);
#ifdef LBAT2_ENABLE
			if (lbat_data->lbat_intr_num == 2) {
				thd_info = &lbat_data->lbat_thd[INTR_2][temp_stage];
				if (aging_stage == 0)
					ag_offset = 0;
				else
					ag_offset = lbat_data->aging_volts[aging_stage-1];

				for (i = 0; i < thd_info->thd_volts_size; i++) {
					thd_info->ag_thd_volts[i] = thd_info->thd_volts[i] + ag_offset;
					thd_info->lbat_intr_info[i].ag_volt =
						thd_info->lbat_intr_info[i].volt + ag_offset;
				}

				dual_lbat_user_modify_thd_ext_locked(
					lbat_data->lbat_pt[INTR_2], thd_info->ag_thd_volts,
					thd_info->thd_volts_size);
				dump_thd_volts_ext(thd_info->ag_thd_volts, thd_info->thd_volts_size);
			}
#endif
		}
		lbat_data->temp_cur_stage = temp_stage;
		lbat_data->lbat_mbrain_info.temp_stage = temp_stage;
		lbat_data->aging_cur_stage = aging_stage;
	}
}

static int lvsys_notifier_call(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	if (!lbat_data)
		return NOTIFY_DONE;

	event = event & ~(1 << 15);

	if (event == lbat_data->lvsys_thd_volt_l)
		decide_and_throttle(LVSYS_INTR, 1, lbat_data->lvsys_thd_volt_l);
	else if (event == lbat_data->lvsys_thd_volt_h)
		decide_and_throttle(LVSYS_INTR, 0, lbat_data->lvsys_thd_volt_h);
	else
		pr_notice("[%s] wrong lvsys thd = %lu\n", __func__, event);

	return NOTIFY_DONE;
}

static struct notifier_block lvsys_notifier = {
	.notifier_call = lvsys_notifier_call,
};

static int fill_thd_info(struct platform_device *pdev, struct lbat_thl_priv *priv, u32 *volt_thd, int intr_no)
{
	int i, j, max_thr_lv, volt_t_size, ret;
	unsigned int volt_l[LOW_BATTERY_LEVEL_NUM-1], volt_h[LOW_BATTERY_LEVEL_NUM-1];
	struct lbat_thd_tbl *thd_info;
	int last_volt_t_size = 0;

	if (intr_no < 0 || intr_no >= INTR_MAX_NUM) {
		dev_notice(&pdev->dev, "[%s] invalid intr_no %d\n", __func__, intr_no);
		return -EINVAL;
	}

	max_thr_lv = LBAT_PMIC_MAX_LEVEL;

	for (i = 0; i <= priv->temp_max_stage; i++) {
		for (j = 0; j < max_thr_lv; j++) {
			volt_l[j] = volt_thd[i * max_thr_lv + j];
			volt_h[j] = volt_thd[max_thr_lv * (priv->temp_max_stage + 1) + i * max_thr_lv + j];
		}

		ret = check_duplicate(volt_l);
		ret |= check_duplicate(volt_h);
		if (ret < 0) {
			dev_notice(&pdev->dev, "[%s] check duplicate error, %d\n", __func__, ret);
			return -EINVAL;
		}

		thd_info = &priv->lbat_thd[intr_no][i];
		thd_info->thd_volts_size = max_thr_lv * 2;
		thd_info->lbat_intr_info = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
			sizeof(struct lbat_thd_tbl), GFP_KERNEL);
		if (!thd_info->lbat_intr_info)
			return -ENOMEM;

		volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0], max_thr_lv);

		if (volt_t_size <= 0) {
			dev_notice(&pdev->dev, "[%s] Failed to rearrange_volt\n", __func__);
			return -ENODATA;
		}

		// different temp stage volt size should be the same
		if (i != 0 && last_volt_t_size != volt_t_size) {
			dev_notice(&pdev->dev, "[%s] volt size should be the same, force trigger kernel panic\n",
				__func__);
			BUG_ON(1);
		}
		last_volt_t_size = volt_t_size;

		thd_info->thd_volts_size = volt_t_size;
		thd_info->thd_volts = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
			sizeof(u32), GFP_KERNEL);
		if (!thd_info->thd_volts)
			return -ENOMEM;

		thd_info->ag_thd_volts = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
			sizeof(u32), GFP_KERNEL);
		if (!thd_info->ag_thd_volts)
			return -ENOMEM;

		for (j = 0; j < volt_t_size; j++) {
			thd_info->thd_volts[j] = thd_info->lbat_intr_info[j].volt;
			thd_info->ag_thd_volts[j] = thd_info->lbat_intr_info[j].volt;
		}

		dump_thd_volts(&pdev->dev, thd_info->thd_volts, thd_info->thd_volts_size);
	}

	return 0;
}

static int low_battery_thd_setting(struct platform_device *pdev, struct lbat_thl_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	char thd_volts_l[THD_VOLTS_LENGTH], thd_volts_h[THD_VOLTS_LENGTH], str[THD_VOLTS_LENGTH];
	char throttle_lv[THD_VOLTS_LENGTH];
	int ret = 0, intr_num = 0;
	int num, i, volt_size, thl_level_size;
	u32 *volt_thd;
	struct device_node *gauge_np = pdev->dev.parent->of_node;

	num = of_property_count_u32_elems(np, "temperature-stage-threshold");
	if (num < 0)
		num = 0;

	priv->temp_max_stage = num;

	if (priv->temp_max_stage > 0) {
		priv->temp_thd = devm_kmalloc_array(&pdev->dev, priv->temp_max_stage, sizeof(u32),
			GFP_KERNEL);
		if (!priv->temp_thd)
			return -ENOMEM;
		ret = of_property_read_u32_array(np, "temperature-stage-threshold", priv->temp_thd,
			num);
		if (ret) {
			pr_notice("get temperature-stage-threshold error %d, set stage=0\n", ret);
			priv->temp_max_stage = 0;
		}
	}

	gauge_np = of_find_node_by_name(gauge_np, "mtk-gauge");
	if (!gauge_np)
		pr_notice("[%s] get mtk-gauge node fail\n", __func__);
	else {
		ret = of_property_read_u32(gauge_np, "bat_type", &priv->bat_type);
		if (ret || priv->bat_type >= 10) {
			priv->bat_type = 0;
			dev_notice(&pdev->dev, "[%s] get bat_type fail, ret=%d bat_type=%d\n", __func__, ret,
				priv->bat_type);
		}
	}

	ret = snprintf(str, THD_VOLTS_LENGTH, "bat%d-aging-threshold", priv->bat_type);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	num = of_property_count_u32_elems(np, str);
	if (num < 0 || num > 10) {
		pr_info("error aging-threshold num %d, set to 0\n", num);
		num = 0;
	}
	priv->aging_max_stage = num;

	if (priv->aging_max_stage > 0) {
		priv->aging_thd = devm_kmalloc_array(&pdev->dev, priv->aging_max_stage, sizeof(u32), GFP_KERNEL);
		if (!priv->aging_thd)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, str, priv->aging_thd, num);
		if (ret) {
			pr_notice("get %s error %d, set aging_max_stage=0\n", str, ret);
			priv->aging_max_stage = 0;
		}

		priv->aging_volts = devm_kmalloc_array(&pdev->dev, priv->aging_max_stage * priv->temp_max_stage,
			sizeof(u32), GFP_KERNEL);
		if (!priv->aging_volts)
			return -ENOMEM;

		for (i = 0; i < priv->temp_max_stage; i++) {
			ret = snprintf(str, THD_VOLTS_LENGTH, "bat%d-aging-volts-t%d", priv->bat_type, i);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

			ret = of_property_read_u32_array(np, str, &priv->aging_volts[priv->aging_max_stage*i], num);
			if (ret) {
				pr_notice("get %s error %d, set aging_max_stage=0\n", str, ret);
				priv->aging_max_stage = 0;
				break;
			}
		}
	}
	thl_level_size = LBAT_PMIC_LEVEL_NUM * (priv->temp_max_stage + 1);
	priv->thl_lv = devm_kmalloc_array(&pdev->dev, thl_level_size, sizeof(u32), GFP_KERNEL);
	if (!priv->thl_lv)
		return -ENOMEM;

	ret = snprintf(throttle_lv, THD_VOLTS_LENGTH, "throttle-level");
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	if (of_property_read_u32_array(np, throttle_lv, priv->thl_lv, thl_level_size)) {
		pr_info("%s:%d: read throttle level error\n", __func__, __LINE__);
		return -EINVAL;
	}

	volt_size = LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1);
	volt_thd = devm_kmalloc_array(&pdev->dev, volt_size * 2, sizeof(u32), GFP_KERNEL);
	if (!volt_thd)
		return -ENOMEM;

	for (i = 1; i <= INTR_MAX_NUM; i++) {
		ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", priv->bat_type,
			i, VOLT_L_STR);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

		num = of_property_count_elems_of_size(np, thd_volts_l, sizeof(u32));
		if (num > 0)
			intr_num++;
		else
			break;
	}

	if (intr_num > 0 && intr_num <= INTR_MAX_NUM) {
		for (i = 0; i < intr_num; i++) {
			ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", priv->bat_type, i + 1,
				VOLT_L_STR);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			ret = snprintf(thd_volts_h, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", priv->bat_type,
				i + 1, VOLT_H_STR);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

			if (of_property_read_u32_array(np, thd_volts_l, &volt_thd[0], volt_size)
				|| of_property_read_u32_array(np, thd_volts_h,
				&volt_thd[volt_size], volt_size)) {
				pr_info("%s:%d: read volts error %s %s\n", __func__, __LINE__,
					thd_volts_l, thd_volts_h);
				return -EINVAL;
			}

			ret = fill_thd_info(pdev, priv, volt_thd, i);
			if (ret) {
				pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
				return ret;
			} else
				priv->lbat_intr_num++;
		}
	} else {
		ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-%s", priv->bat_type, VOLT_L_STR);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		ret = snprintf(thd_volts_h, THD_VOLTS_LENGTH, "bat%d-%s", priv->bat_type, VOLT_H_STR);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

		if (of_property_read_u32_array(np, thd_volts_l, &volt_thd[0], volt_size)
			|| of_property_read_u32_array(np, thd_volts_h,  &volt_thd[volt_size],
			volt_size)) {
			pr_info("%s:%d: read volts error %s %s\n", __func__, __LINE__,
				thd_volts_l, thd_volts_h);
			return -EINVAL;
		}

		ret = fill_thd_info(pdev, priv, volt_thd, INTR_1);
		if (ret) {
			pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
			return ret;
		} else
			priv->lbat_intr_num++;
	}

	return 0;
}

static int low_battery_register_setting(struct platform_device *pdev,
		struct lbat_thl_priv *priv, enum LOW_BATTERY_INTR_TAG intr_type,
		unsigned int temp_stage)
{
	int ret;
	struct lbat_thd_tbl *thd_info;
	struct lbat_user *lbat_p = NULL;

	if (intr_type == INTR_1 && temp_stage <= priv->temp_max_stage) {
		thd_info = &priv->lbat_thd[INTR_1][temp_stage];
		priv->lbat_pt[INTR_1] = lbat_user_register_ext("power throttling", thd_info->ag_thd_volts,
		thd_info->thd_volts_size, exec_low_battery_callback);
		lbat_p = priv->lbat_pt[INTR_1];
	} else if (intr_type == INTR_2  && temp_stage <= priv->temp_max_stage) {
#ifdef LBAT2_ENABLE
		thd_info = &priv->lbat_thd[INTR_2][temp_stage];
		priv->lbat_pt[INTR_2] = dual_lbat_user_register_ext("power throttling",
			thd_info->ag_thd_volts, thd_info->thd_volts_size, exec_dual_low_battery_callback);
		lbat_p = priv->lbat_pt[INTR_2];
#endif
	} else {
		dev_notice(&pdev->dev, "[%s] invalid intr_type=%d temp_stage=%d\n", __func__,
		intr_type, temp_stage);
		return -1;
	}

	if (IS_ERR(lbat_p)) {
		ret = PTR_ERR(lbat_p);
		if (ret != -EPROBE_DEFER) {
			dev_notice(&pdev->dev, "[%s] error ret=%d\n", __func__, ret);
		}
		return ret;
	}

	dev_notice(&pdev->dev, "[%s] register intr_type=%d temp_stage=%d done\n", __func__,
		intr_type, temp_stage);

	return 0;
}

static int low_battery_throttling_probe(struct platform_device *pdev)
{
	int ret, i;
	int lvsys_thd_enable, vbat_thd_enable;
	struct lbat_thl_priv *priv;
	struct device_node *np = pdev->dev.of_node;
#if defined(CONFIG_MTK_NO_BAT_BOOT_SUPPORT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	union power_supply_propval value = {0, };
#endif
#endif

#if defined(CONFIG_MTK_NO_BAT_BOOT_SUPPORT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	psy_do_property("primary_chg", get,
				POWER_SUPPLY_EXT_PROP_BATT_F_MODE, value);
	low_battery_f_mode = value.intval;
#endif
#endif

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);

	INIT_WORK(&priv->psy_work, psy_handler);

	ret = of_property_read_u32(np, "lvsys-thd-enable", &lvsys_thd_enable);
	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get lvsys-thd-enable ret=%d\n", __func__, ret);
		lvsys_thd_enable = 0;
	}

	ret = of_property_read_u32(np, "vbat-thd-enable", &vbat_thd_enable);
	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get vbat-thd-enable ret=%d\n", __func__, ret);
		vbat_thd_enable = 1;
	}

	if (vbat_thd_enable) {
		ret = low_battery_thd_setting(pdev, priv);
		if (ret) {
			pr_info("[%s] low_battery_thd_setting error, ret=%d\n", __func__, ret);
			return ret;
		}

		for (i = 0; i < priv->lbat_intr_num; i++) {
			ret = low_battery_register_setting(pdev, priv, i, 0);
			if (ret) {
				pr_info("[%s] low_battery_register failed, intr_no=%d ret=%d\n",
					__func__, i, ret);
				return ret;
			}
		}

		if (priv->temp_max_stage > 0 || priv->aging_max_stage > 0) {
			lbat_nb.notifier_call = lbat_psy_event;
			ret = power_supply_reg_notifier(&lbat_nb);
			if (ret) {
				dev_notice(&pdev->dev, "[%s] power_supply_reg_notifier fail\n",
					__func__);
				return ret;
			}
		}
	}

	if (lvsys_thd_enable) {
		ret = of_property_read_u32(np, "lvsys-thd-volt-l", &priv->lvsys_thd_volt_l);
		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get lvsys-thd-volt-l ret=%d\n", __func__, ret);
			priv->lvsys_thd_volt_l = LVSYS_THD_VOLT_L;
		}

		ret = of_property_read_u32(np, "lvsys-thd-volt-h", &priv->lvsys_thd_volt_h);
		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get lvsys-thd-volt-h ret=%d\n", __func__, ret);
			priv->lvsys_thd_volt_h = LVSYS_THD_VOLT_H;
		}

		dev_notice(&pdev->dev, "lvsys_register: %d mV, %d mV\n",
			priv->lvsys_thd_volt_l, priv->lvsys_thd_volt_h);

		ret = lvsys_register_notifier(&lvsys_notifier);
		if (ret)
			dev_notice(&pdev->dev, "lvsys_register_notifier error ret=%d\n", ret);
	}

	ret = of_property_read_u32(np, "pt-shutdown-enable", &priv->pt_shutdown_en);
	if (ret) {
		dev_notice(&pdev->dev, "[%s] failed to getpt-shutdown, set to 1\n", __func__);
		priv->pt_shutdown_en = 1;
	}

	ret = device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_ut);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_stop);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_level);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_modify_threshold);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_throttle_cnt);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}

	priv->dev = &pdev->dev;
	lbat_data = priv;
	if (priv->pt_shutdown_en)
		pt_notify_init(pdev);

	return 0;
}

static const struct of_device_id low_bat_thl_of_match[] = {
	{ .compatible = "mediatek,low_battery_throttling", },
	{ },
};
MODULE_DEVICE_TABLE(of, low_bat_thl_of_match);

static struct platform_driver low_battery_throttling_driver = {
	.driver = {
		.name = "low_battery_throttling",
		.of_match_table = low_bat_thl_of_match,
	},
	.probe = low_battery_throttling_probe,
};

module_platform_driver(low_battery_throttling_driver);
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK low battery throttling driver");
MODULE_LICENSE("GPL");
