// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/cdev.h>		/* cdev */
#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/iio/consumer.h>	/* iio_device */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>	/*irq_to_desc*/
#include <linux/kernel.h>
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/math64.h>
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/netlink.h>	/* netlink */
#include <linux/of_fdt.h>	/*of_dt API*/
#include <linux/of.h>
#include <linux/platform_device.h>	/* platform device */
#include <linux/proc_fs.h>
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/sched.h>	/* For wait queue*/
#include <linux/skbuff.h>	/* netlink */
#include <linux/socket.h>	/* netlink */
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>		/* For wait queue*/
#include <net/sock.h>		/* netlink */
#include <linux/suspend.h>
#include "mtk_battery.h"

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <../drivers/battery/common/sec_charging_common.h>
#endif

/* ============================================================ */
/* gaugel hal interface */
/* ============================================================ */
int gauge_get_property(struct mtk_battery *gm, enum gauge_property gp,
	int *val)
{
	struct mtk_gauge *gauge;
	struct property_control *prop_control;
	ktime_t dtime;
	struct timespec64 diff;
	struct mtk_gauge_sysfs_field_info *attr;
	int ret = 0;

	if (gm == NULL || gm->gauge == NULL){
		pr_err("%s attr =NULL\n", __func__);
		return -ENODEV;
	}
	gauge = gm->gauge;
	attr = gm->gauge->attr;
	prop_control = &gm->prop_control;

	if (attr == NULL) {
		pr_err("%s attr =NULL\n", __func__);
		return -ENODEV;
	}
	if (attr[gp].prop == gp) {
		mutex_lock(&gauge->ops_lock);
		prop_control->start_get_prop_time = ktime_get_boottime();
		prop_control->curr_gp = gp;
		prop_control->end_get_prop_time = 0;
		ret = attr[gp].get(gauge, &attr[gp], val);
		prop_control->end_get_prop_time = ktime_get_boottime();
		dtime = ktime_sub(prop_control->end_get_prop_time,
			prop_control->start_get_prop_time);
		diff = ktime_to_timespec64(dtime);
		if (timespec64_compare(&diff, &prop_control->max_get_prop_time) > 0) {
			prop_control->max_gp = gp;
			prop_control->max_get_prop_time = diff;
		}
		if (diff.tv_sec > prop_control->i2c_fail_th)
			prop_control->i2c_fail_counter[gp] += 1;

		mutex_unlock(&gauge->ops_lock);
	} else {
		pr_err("%s gp:%d idx error\n", __func__, gp);
		return -EOPNOTSUPP;
	}

	return ret;
}

int gauge_get_int_property(struct mtk_battery *gm, enum gauge_property gp)
{
	int val = 0;

	gauge_get_property(gm, gp, &val);
	return val;
}

int prop_control_mapping(enum gauge_property gp)
{
	switch (gp) {
	case GAUGE_PROP_BATTERY_EXIST:
		return CONTROL_GAUGE_PROP_BATTERY_EXIST;
	case GAUGE_PROP_BATTERY_CURRENT:
		return CONTROL_GAUGE_PROP_BATTERY_CURRENT;
	case GAUGE_PROP_AVERAGE_CURRENT:
		return CONTROL_GAUGE_PROP_AVERAGE_CURRENT;
	case GAUGE_PROP_BATTERY_VOLTAGE:
		return CONTROL_GAUGE_PROP_BATTERY_VOLTAGE;
	case GAUGE_PROP_BATTERY_TEMPERATURE_ADC:
		return CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC;
	default:
		return -1;
	}
}

int gauge_get_property_control(struct mtk_battery *gm, enum gauge_property gp,
	int *val, int mode)
{
	struct property_control *prop_control;
	ktime_t ctime = 0, dtime = 0, diff = 0;
	int prop_map, ret = 0;

	prop_control = &gm->prop_control;

	prop_map = prop_control_mapping(gp);

	if (prop_map == -1) {
		ret = gauge_get_property(gm, gp, val);
	} else if (mode == 0 || gm->no_prop_timeout_control) {
		ret = gauge_get_property(gm, gp, val);
		prop_control->val[prop_map] = *val;
		prop_control->last_prop_update_time[prop_map] = ktime_get_boottime();
	} else {
		ctime = ktime_get_boottime();
		dtime = ktime_sub(ctime, prop_control->last_prop_update_time[prop_map]);
		diff = ktime_to_ms(dtime);
		prop_control->binder_counter += 1;

		if (diff > prop_control->diff_time_th[prop_map]) {
			ret = gauge_get_property(gm, gp, val);
			prop_control->val[prop_map] = *val;
			prop_control->last_prop_update_time[prop_map] = ctime;
		} else {
			*val = prop_control->val[prop_map];
		}
	}
	return ret;
}

int get_charger_vbat(struct mtk_battery_manager *bm)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGE_NOW, &val);
		if (ret >= 0)
			ret = val.intval / 1000;
		else {
			ret = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE) / 10;
			pr_err("[%s] get POWER_SUPPLY_PROP_CHARGE_NOW fail\n", __func__);
		}

		power_supply_put(psy);
	} else {
		ret = gauge_get_int_property(bm->gm1,
			GAUGE_PROP_BATTERY_VOLTAGE) / 10;
		pr_err("[%s] get charger power supply fail\n", __func__);
	}

	return ret;
}

int bm_get_vsys(struct mtk_battery_manager *bm)
{
	int ret, val = 0;

	if (!IS_ERR(bm->chan_vsys)) {
		ret = iio_read_channel_processed(bm->chan_vsys, &val);
		if (ret < 0) {
			pr_err("[%s]read fail,ret=%d use chg_vbat\n", __func__, ret);
			val = get_charger_vbat(bm);
		}
	} else {
		pr_err("[%s]chan error use chg_vbat\n", __func__);
		val = get_charger_vbat(bm);
	}

	return val;
}

int bat_get_debug_level(struct mtk_battery *gm)
{
	if (gm == NULL) {
		pr_debug("[%s] can not get gauge log level\n", __func__);
		return BMLOG_DEBUG_LEVEL;
	} else
		return gm->log_level;
}

char *bat_get_gauge_name(struct mtk_battery *gm)
{

	if (gm == NULL) {
		pr_debug("[%s] can not get gauge name\n", __func__);
		return "BATTERY";
	} else if (gm->gauge == NULL) {
		pr_debug("[%s] can not get gauge name\n", __func__);
		return "BATTERY";
	} else
		return gm->gauge->name;
}

int gauge_set_property(struct mtk_battery *gm, enum gauge_property gp,
	int val)
{
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *attr;

	if (gm == NULL || gm->gauge == NULL){
		pr_err("%s attr =NULL\n", __func__);
		return -ENODEV;
	}
	gauge = gm->gauge;

	attr = gm->gauge->attr;
	if (attr == NULL) {
		pr_err("%s attr =NULL\n", __func__);
		return -ENODEV;
	}

	if (attr[gp].prop == gp) {
		mutex_lock(&gauge->ops_lock);
		attr[gp].set(gauge, &attr[gp], val);
		mutex_unlock(&gauge->ops_lock);
	} else {
		pr_err("%s gp:%d idx error\n", __func__, gp);
		return -EOPNOTSUPP;
	}

	return 0;
}

int battery_get_property(struct mtk_battery *gm, enum battery_property bp,
			    int *val)
{
	if (gm != NULL && gm->battery_sysfs != NULL) {
		if (gm->battery_sysfs[bp].prop == bp)
			gm->battery_sysfs[bp].get(gm,
				&gm->battery_sysfs[bp], val);
		else {
			pr_err("%s bp:%d idx error\n", __func__, bp);
			return -EOPNOTSUPP;
		}
	} else
		return -EOPNOTSUPP;

	return 0;
}

int battery_get_int_property(struct mtk_battery *gm, enum battery_property bp)
{
	int val = 0;

	battery_get_property(gm, bp, &val);
	return val;
}

int battery_set_property(struct mtk_battery *gm, enum battery_property bp,
			    int val)
{
	if (gm != NULL && gm->battery_sysfs != NULL) {
		if (gm->battery_sysfs[bp].prop == bp)
			gm->battery_sysfs[bp].set(gm,
				&gm->battery_sysfs[bp], val);
		else {
			pr_err("%s bp:%d idx error\n", __func__, bp);
			return -EOPNOTSUPP;
		}
	} else
		return -EOPNOTSUPP;
	return 0;
}

bool is_kernel_power_off_charging(struct mtk_battery *gm)
{
	pr_err("%s, bootmdoe = %u\n", __func__,gm->bootmode);

	/* KERNEL_POWER_OFF_CHARGING_BOOT */
	if (gm->bootmode == 8)
		return true;

	return false;
}
/* ============================================================ */
/* power misc */
/* ============================================================ */



void set_shutdown_vbat_lt(struct mtk_battery *gm, int vbat_lt, int vbat_lt_lv1)
{
	struct battery_shutdown_unit *sdu = &gm->bm->sdc.bat[gm->id];

	sdu->vbat_lt = vbat_lt;
	sdu->vbat_lt_lv1 = vbat_lt_lv1;
}

int disable_shutdown_cond(struct mtk_battery *gm, int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc = 0;
	struct battery_shutdown_unit *sdu;
	int vbat = 0;

	if (gm->bm == NULL) {
		pr_err("[%s]battery manager is not rdy\n",
				__func__);
		return 0;
	}

	sdu = &gm->bm->sdc.bat[gm->id];

	now_current = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_CURRENT);
	now_is_kpoc = is_kernel_power_off_charging(gm);

	vbat = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_VOLTAGE);


	pr_err("%s %d, is kpoc %d curr %d is_charging %d flag:%d lb:%d\n",
		__func__,
		shutdown_cond, now_is_kpoc, now_current, now_is_charging,
		sdu->shutdown_cond_flag,
		vbat);

	switch (shutdown_cond) {
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		sdu->shutdown_status.is_under_shutdown_voltage = false;
		sdu->lowbatteryshutdown = false;
		pr_err("disable LOW_BAT_VOLT avgvbat %d ,threshold:%d %d %d\n",
		sdu->avgvbat,
		BAT_VOLTAGE_HIGH_BOUND,
		sdu->vbat_lt,
		sdu->vbat_lt_lv1);
		break;
#endif
	default:
		break;
	}
	return 0;
}

void wake_up_power_misc(struct shutdown_controller *sdc)
{
	unsigned long flags;

	pr_debug("[%s] into\n", __func__);

	spin_lock_irqsave(&sdc->slock, flags);
	if (!sdc->sdc_wakelock->active)
		__pm_stay_awake(sdc->sdc_wakelock);
	spin_unlock_irqrestore(&sdc->slock, flags);
	wake_up(&sdc->wait_que);
}

int get_shutdown_cond(struct mtk_battery *gm)
{
	int ret = 0;
	int vbat = 0;
	struct battery_shutdown_unit *sdu;

	vbat = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_VOLTAGE);

	if (gm->bm == NULL) {
		pr_err("[%s]battery manager is not rdy\n",
				__func__);
		return 1;
	}

	sdu = &gm->bm->sdc.bat[gm->id];

	if (sdu->shutdown_status.is_soc_zero_percent)
		ret |= 1;
	if (sdu->shutdown_status.is_uisoc_one_percent)
		ret |= 1;
	if (sdu->lowbatteryshutdown)
		ret |= 1;

	if (gm->bm->gm_no == 2) {
		if (gm->bm->sdc.bmsdu.lowbatteryshutdown)
			ret |= 1;
		if (gm->bm->sdc.bmsdu.shutdown_status.is_soc_zero_percent)
			ret |= 1;
		if (gm->bm->sdc.bmsdu.shutdown_status.is_uisoc_one_percent)
			ret |= 1;
	}

	pr_debug("%s gm_no:%d ret:%d %d %d %d %d %d %d vbat:%d\n",
		__func__, gm->bm->gm_no,
	ret, sdu->shutdown_status.is_soc_zero_percent,
	sdu->shutdown_status.is_uisoc_one_percent,
	sdu->lowbatteryshutdown, gm->bm->sdc.bmsdu.lowbatteryshutdown,
	gm->bm->sdc.bmsdu.shutdown_status.is_soc_zero_percent,
	gm->bm->sdc.bmsdu.shutdown_status.is_uisoc_one_percent,
	vbat);

	return ret;
}

int set_bm_shutdown_cond(struct mtk_battery_manager *bm, int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc = 0;
	struct shutdown_controller *sdc = &bm->sdc;
	struct battery_shutdown_unit *bmsdu = &bm->sdc.bmsdu;
	int i, enable_timer = 0;
	ktime_t now;

	now_is_kpoc = is_kernel_power_off_charging(bm->gm1);
	now_current = gauge_get_int_property(bm->gm1, GAUGE_PROP_BATTERY_CURRENT);
	now_current += gauge_get_int_property(bm->gm2, GAUGE_PROP_BATTERY_CURRENT);

	if (now_current >= 0)
		now_is_charging = 1;

	pr_err("%s %d kpoc %d flag:%d\n",
		__func__,
		shutdown_cond, now_is_kpoc,
		bmsdu->shutdown_cond_flag);

	if (bmsdu->shutdown_cond_flag == 1)
		return 0;

	if (bmsdu->shutdown_cond_flag == 2 && shutdown_cond != LOW_BAT_VOLT)
		return 0;

	if (bmsdu->shutdown_cond_flag == 3 && shutdown_cond != DLPT_SHUTDOWN)
		return 0;

	switch (shutdown_cond) {
	case SOC_ZERO_PERCENT:
		if (bmsdu->shutdown_status.is_soc_zero_percent != true) {
			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					bmsdu->shutdown_status.is_soc_zero_percent = true;
					battery_set_property(bm->gm1,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					battery_set_property(bm->gm2,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					pr_err("[%s]soc_zero_percent shutdown\n",
						__func__);
				}
			}
			mutex_unlock(&sdc->lock);
			enable_timer = 1;
		}
		break;
	case UISOC_ONE_PERCENT:
		if (bmsdu->shutdown_status.is_uisoc_one_percent != true) {
			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					bmsdu->shutdown_status.is_uisoc_one_percent = true;
					battery_set_property(bm->gm1,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					battery_set_property(bm->gm2,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					pr_err("[%s]uisoc 1 percent shutdown\n",
						__func__);
				}
			}
			mutex_unlock(&sdc->lock);
			enable_timer = 1;
		}
		break;
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		if (bmsdu->shutdown_status.is_under_shutdown_voltage != true) {

			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				bmsdu->shutdown_status.is_under_shutdown_voltage = true;
				for (i = 0; i < bm->gm1->avgvbat_array_size; i++)
					bmsdu->batdata[i] =
						bm->gm1->fg_cust_data.vbat2_det_voltage1 / 10;
				bmsdu->batidx = 0;
				enable_timer = 1;
			}
			pr_err("LOW_BAT_VOLT:vbat %d",
				bm->gm1->fg_cust_data.vbat2_det_voltage1 / 10);
			mutex_unlock(&sdc->lock);
		}
		break;
#endif
	case DLPT_SHUTDOWN:
		if (bmsdu->shutdown_status.is_dlpt_shutdown != true) {
			mutex_lock(&sdc->lock);
			bmsdu->shutdown_status.is_dlpt_shutdown = true;
			bmsdu->pre_time[DLPT_SHUTDOWN] = ktime_get_boottime();
			battery_set_property(bm->gm1,
				BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_DLPT_SD);
			battery_set_property(bm->gm2,
				BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_DLPT_SD);
			mutex_unlock(&sdc->lock);
			enable_timer = 1;
		}
		break;

	default:
		break;
	}

	if (enable_timer) {
		now = ktime_get_boottime();
		alarm_start(&sdc->kthread_fgtimer[bmsdu->type], now);
	}

	return 0;
}

void set_shutdown_cond_flag(struct mtk_battery *gm, int val)
{
	struct battery_shutdown_unit *sdu = &gm->bm->sdc.bat[gm->id];
	struct battery_shutdown_unit *bmsdu = &gm->bm->sdc.bmsdu;

	sdu->shutdown_cond_flag = val;
	if (gm->bm->gm_no == 2)
		bmsdu->shutdown_cond_flag = val;
}

int get_shutdown_cond_flag(struct mtk_battery *gm)
{
	struct battery_shutdown_unit *sdu = &gm->bm->sdc.bat[gm->id];

	return sdu->shutdown_cond_flag;
}

int set_shutdown_cond(struct mtk_battery *gm, int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc = 0;
	int vbat = 0;
	struct shutdown_controller *sdc;
	struct battery_shutdown_unit *sdu;
	struct shutdown_condition *sds;
	int enable_lbat_shutdown;
	int is_single = 0;
	int i, enable_timer = 0;
	ktime_t now;

	if (gm->bm == NULL) {
		pr_err("[%s]battery manager is not rdy\n",
				__func__);
		return 0;
	}

	sdc = &gm->bm->sdc;
	sdu = &gm->bm->sdc.bat[gm->id];

#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	enable_lbat_shutdown = 1;
#else
	enable_lbat_shutdown = 0;
#endif

	now_current = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_CURRENT);
	now_is_kpoc = is_kernel_power_off_charging(gm);

	if (gm->bm->gm_no == 1)
		is_single = true;
	else
		is_single = false;

	vbat = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_VOLTAGE);


	sds = &sdu->shutdown_status;

	if (now_current >= 0)
		now_is_charging = 1;

	pr_err("%s %d %d kpoc %d curr %d is_charging %d flag:%d lb:%d\n",
		__func__,
		shutdown_cond, enable_lbat_shutdown,
		now_is_kpoc, now_current, now_is_charging,
		sdu->shutdown_cond_flag, vbat);

	if (sdu->shutdown_cond_flag == 1)
		return 0;

	if (sdu->shutdown_cond_flag == 2 && shutdown_cond != LOW_BAT_VOLT)
		return 0;

	if (sdu->shutdown_cond_flag == 3 && shutdown_cond != DLPT_SHUTDOWN)
		return 0;

	switch (shutdown_cond) {
	case OVERHEAT:
		mutex_lock(&sdc->lock);
		sdu->shutdown_status.is_overheat = true;
		mutex_unlock(&sdc->lock);
		pr_err("[%s]OVERHEAT shutdown!\n", __func__);
		kernel_power_off();
		break;
	case SOC_ZERO_PERCENT:
		if (sdu->shutdown_status.is_soc_zero_percent != true) {
			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sds->is_soc_zero_percent =
						true;
					enable_timer = 1;
					sdu->pre_time[SOC_ZERO_PERCENT] =
						ktime_get_boottime();
					pr_err("[%s]soc_zero_percent shutdown\n",
						__func__);

					if (is_single == true)
						battery_set_property(gm,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
				}
			}
			mutex_unlock(&sdc->lock);
			if (is_single == false)
				set_bm_shutdown_cond(gm->bm, SOC_ZERO_PERCENT);
		}
		break;
	case UISOC_ONE_PERCENT:
		if (sdu->shutdown_status.is_uisoc_one_percent != true) {
			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sds->is_uisoc_one_percent =
						true;
					enable_timer = 1;
					sdu->pre_time[UISOC_ONE_PERCENT] =
						ktime_get_boottime();

					pr_err("[%s]uisoc 1 percent shutdown\n",
						__func__);
					battery_set_property(gm,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
				}
			}
			mutex_unlock(&sdc->lock);
		}
		break;
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		if (sdu->shutdown_status.is_under_shutdown_voltage != true) {

			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				sds->is_under_shutdown_voltage = true;
				for (i = 0; i < gm->avgvbat_array_size; i++)
					sdu->batdata[i] =
						gm->fg_cust_data.vbat2_det_voltage1 / 10;
				sdu->batidx = 0;
				enable_timer = 1;
			}
			pr_err("LOW_BAT_VOLT:vbat %d %d",
				vbat, gm->fg_cust_data.vbat2_det_voltage1 / 10);
			mutex_unlock(&sdc->lock);
		}

		if (is_single == false)
			set_bm_shutdown_cond(gm->bm, LOW_BAT_VOLT);

		break;
#endif
	case DLPT_SHUTDOWN:
		if (is_single == false)
			set_bm_shutdown_cond(gm->bm, DLPT_SHUTDOWN);
		else {
			if (sdu->shutdown_status.is_dlpt_shutdown != true) {
				mutex_lock(&sdc->lock);
				sdu->shutdown_status.is_dlpt_shutdown = true;
				sdu->pre_time[DLPT_SHUTDOWN] = ktime_get_boottime();
				battery_set_property(gm,
					BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_DLPT_SD);
				mutex_unlock(&sdc->lock);
				enable_timer = 1;
			}
		}
		break;

	default:
		break;
	}

	if (enable_timer) {
		now = ktime_get_boottime();
		alarm_start(&sdc->kthread_fgtimer[sdu->type], now);
	}

	return 0;
}

/* ============================================================ */
/* power misc end*/
/* ============================================================ */

#if IS_ENABLED(CONFIG_SEC_FACTORY)
static int vsys_to_vsoc_for_factory_mode(struct mtk_battery_manager *bm)
{
	struct battery_data *bat_data;
	union power_supply_propval val;

	int vsys = 0;
	int index_value, vsoc = 0;
	int ret;

	bat_data = &bm->bs_data;
	if (IS_ERR_OR_NULL(bat_data->chg_psy)) {
		bat_data->chg_psy = devm_power_supply_get_by_phandle(bm->dev, "charger");
		pr_info("%s try to get chg_psy\n", __func__);
	}

	if (IS_ERR_OR_NULL(bat_data->chg_psy)) {
		vsoc = 100;
		pr_info("%s Couldn't get chg_psy, vsoc fixed %d\n", __func__, vsoc);
		return vsoc;
	}

	ret = power_supply_get_property(bat_data->chg_psy, POWER_SUPPLY_PROP_VOLTAGE_MIN, &val);
	if (ret < 0) {
		vsoc = 100;
		pr_info("%s get vsys property fail, vsoc fixed %d\n", __func__, vsoc);
		return vsoc;
	}

	vsys = val.intval / 1000;

	for (index_value = 0; index_value < 100; index_value++) {
		if ((bm->gm1->fg_table_cust_data.fg_profile[1].fg_profile[index_value].voltage/10) < vsys) {
			vsoc =  100 - index_value;
			break;
		}
	}

	pr_info("%s vsys: %d, VSOC[%d] : %d\n", __func__,
		vsys, vsoc, bm->gm1->fg_table_cust_data.fg_profile[1].fg_profile[index_value].voltage/10);

	return vsoc;
}
#endif

void battery_update(struct mtk_battery_manager *bm)
{
	ktime_t now, duraction;
	struct timespec64 tmp_duraction;
	struct battery_data *bat_data;
	struct power_supply *bat_psy;
	struct mtk_battery *gm1;
	struct mtk_battery *gm2;
	struct fgd_cmd_daemon_data *d1;
	struct fgd_cmd_daemon_data *d2;
	static int first;
	struct battery_shutdown_unit *sdu;
	int vbat1 = 0, vbat2 = 0, real_uisoc = 0, real_quse = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval value;
	struct power_supply *psy;
	int ret = 0;
#endif

	if (bm == NULL) {
		pr_err("[%s]battery manager is not rdy\n",
				__func__);
		return;
	}
	sdu = &bm->sdc.bmsdu;
	bat_data = &bm->bs_data;
	bat_psy = bat_data->psy;

	if (bm->gm_no == 2) {
		gm1 = bm->gm1;
		gm2 = bm->gm2;
		d1 = &gm1->daemon_data;
		d2 = &gm2->daemon_data;

		if (bm->gm1->is_probe_done == false ||
			bm->gm2->is_probe_done == false ||
			bat_psy == NULL) {
			pr_err("[%s]battery is not rdy:probe:%d %d\n", __func__,
				bm->gm1->is_probe_done, bm->gm2->is_probe_done);
			return;
		}

		if (!bm->gm1->bat_plug_out) {
			if (d1->quse == 0) {
				if (bm->gm1->battery_id < 0 || bm->gm1->battery_id >= TOTAL_BATTERY_NUMBER)
					d1->quse = bm->gm1->fg_table_cust_data.fg_profile[
								0].q_max * 10;
				else
					d1->quse = bm->gm1->fg_table_cust_data.fg_profile[
								bm->gm1->battery_id].q_max * 10;
			}
			real_uisoc += bm->gm1->ui_soc * d1->quse;
			real_quse += d1->quse;
		}
		if (!bm->gm2->bat_plug_out) {
			if (d2->quse == 0) {
				if (bm->gm2->battery_id < 0 || bm->gm2->battery_id >= TOTAL_BATTERY_NUMBER)
					d2->quse = bm->gm2->fg_table_cust_data.fg_profile[
								0].q_max * 10;
				else
					d2->quse = bm->gm2->fg_table_cust_data.fg_profile[
								bm->gm2->battery_id].q_max * 10;
			}
			real_uisoc += bm->gm2->ui_soc * d2->quse;
			real_quse += d2->quse;
		}

		if (real_quse != 0)
			real_uisoc = real_uisoc / real_quse;
		bm->uisoc = real_uisoc;

		now = ktime_get_boottime();
		if (bm->uisoc == 1) {
			if (sdu->pre_time[SHUTDOWN_1_TIME] == 0)
				sdu->pre_time[SHUTDOWN_1_TIME] = now;

			duraction =
				ktime_sub(
				now, sdu->pre_time[SHUTDOWN_1_TIME]);

			tmp_duraction = ktime_to_timespec64(duraction);
			if (bm->gm1->fg_cust_data.shutdown_gauge1_xmins == true &&
				tmp_duraction.tv_sec >= 60LL * bm->gm1->fg_cust_data.shutdown_1_time) {
				pr_err("force uisoc zero percent\n");
				set_bm_shutdown_cond(bm, UISOC_ONE_PERCENT);
			}
		}

		if (bm->gm1->disableGM30 || bm->gm2->disableGM30) {
			bat_data->bat_batt_vol = 4000;
			bm->uisoc = 50;
		} else {
			gauge_get_property_control(bm->gm1, GAUGE_PROP_BATTERY_VOLTAGE,
				&vbat1, 0);
			gauge_get_property_control(bm->gm2, GAUGE_PROP_BATTERY_VOLTAGE,
				&vbat2, 0);
			bat_data->bat_batt_vol = (vbat1 + vbat2) / 2;
		}

		if(((bm->gm1->init_flag ^ bm->gm1->bat_plug_out) &&
			(bm->gm2->init_flag ^ bm->gm2->bat_plug_out) &&
			(bm->gm1->init_flag || bm->gm2->init_flag))) {

			bat_data->bat_capacity = bm->uisoc;
			if (first == 0) {
				gauge_set_property(bm->gm1, GAUGE_PROP_RESET_FG_RTC, 0);
				first++;
			}
			gauge_set_property(bm->gm1, GAUGE_PROP_RTC_UI_SOC, real_uisoc);
		}

		if (bm->gm1->fixed_bat_tmp != 0xffff)
			bat_data->bat_batt_temp = bm->gm1->fixed_bat_tmp;
		else if (bm->gm2->fixed_bat_tmp != 0xffff)
			bat_data->bat_batt_temp = bm->gm2->fixed_bat_tmp;
		else
			bat_data->bat_batt_temp = (bm->gm1->battery_temp + bm->gm2->battery_temp) / 2;

	} else {
		if (bm->gm1->is_probe_done == false ||
			bat_psy == NULL) {
			pr_err("[%s]battery is not rdy:probe:%d\n", __func__,
				bm->gm1->is_probe_done);
			return;
		}

		if (bm->gm1->disableGM30) {
			bat_data->bat_batt_vol = 4000;
			bm->uisoc = 50;
		} else {
			gauge_get_property_control(bm->gm1, GAUGE_PROP_BATTERY_VOLTAGE,
				&bat_data->bat_batt_vol, 0);
			bm->uisoc = bm->gm1->ui_soc;
		}

		if (battery_get_int_property(bm->gm1, BAT_PROP_DISABLE))
			bat_data->bat_capacity = 50;
		else if(bm->gm1->init_flag == 1)
			bat_data->bat_capacity = bm->uisoc;

		bat_data->bat_batt_temp = bm->gm1->battery_temp;
	}

	bat_data->bat_technology = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->bat_present = 1;

#if defined(CONFIG_MTK_NO_BAT_BOOT_SUPPORT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	if (bm->gm1->f_mode == OB_MODE) {
		if (bm->gm1->disableGM30) {
			bat_data->bat_capacity = vsys_to_vsoc_for_factory_mode(bm);
			pr_err("[%s] bat_capacity =%d\n", __func__, bat_data->bat_capacity);
		}
	}
#endif
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy = power_supply_get_by_name("battery"); /*sec_battery.c */
	if (!psy) {
		pr_err("%s: Fail to get psy (battery)\n", __func__);
	} else {
		if (bm->gm1->is_fake_soc) {
			if (bat_data->bat_capacity  >= 0) {
				pr_info("%s : clear fake SOC\n", __func__);
				value.intval = bat_data->bat_capacity;
				ret = power_supply_set_property(psy,
					POWER_SUPPLY_PROP_CAPACITY, &value);
				if (ret < 0)
					pr_err("%s: psy capacity fail(%d)\n",
						__func__, ret);

#if IS_ENABLED(CONFIG_DIRECT_CHARGING) &&  IS_ENABLED(CONFIG_MTK_CHARGER)
				/* let DC know foc init complete */
				value.intval = 1;
				ret = power_supply_set_property(psy, (enum power_supply_property)
					POWER_SUPPLY_EXT_PROP_MTK_FG_INIT, &value);
				if (ret < 0)
					pr_err("%s: psy update fg state fail(%d)\n",
						__func__, ret);
#endif
				bm->gm1->is_fake_soc = 0;
				value.intval = 0;
				/* To report updated soc to UI */
				ret = power_supply_set_property(psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &value);
				if (ret < 0)
					pr_err("%s: psy online fail(%d)\n",
						__func__, ret);
			}
		}
	}
#endif

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	power_supply_changed(bat_psy);
#endif
}

