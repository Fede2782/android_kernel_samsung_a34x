// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/cdev.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/suspend.h>
#include <net/sock.h>

#include "mtk_battery.h"
#include "mtk_gauge.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <../drivers/battery/common/sec_charging_common.h>
#define SEC_BATTERY_FAKE_CAPACITY 0
#endif

//#define BM_NO_SLEEP
//#define BM_USE_ALARM_TIMER
#define BM_USE_HRTIMER

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct mtk_battery_manager *get_mtk_battery_manager(void)
{
	static struct mtk_battery_manager *bm;
	struct power_supply *psy;

	if (bm == NULL) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		psy = power_supply_get_by_name("mtk-fg-battery");
#else
		psy = power_supply_get_by_name("battery");
#endif
		if (psy == NULL) {
			pr_err("[%s]psy is not rdy\n", __func__);
			return NULL;
		}
		bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);
		power_supply_put(psy);
		if (bm == NULL) {
			pr_err("[%s]mtk_battery_manager is not rdy\n", __func__);
			return NULL;
		}
	}

	return bm;
}

/* ============================================================ */
/* power supply: battery */
/* ============================================================ */
int check_cap_level(int uisoc)
{
	if (uisoc >= 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc >= 80 && uisoc < 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc >= 20 && uisoc < 80)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > 0 && uisoc < 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (uisoc == 0)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
}

int next_waketime(int polling)
{
	if (polling <= 0)
		return 0;
	else
		return 10;
}

static int shutdown_event_handler(struct mtk_battery *gm)
{
	ktime_t now, duraction;
	struct timespec64 tmp_duraction;
	int polling = 0;
	int now_current = 0;
	int current_ui_soc = gm->ui_soc;
	int current_soc = gm->soc;
	int vbat = 0;
	int tmp = 25;
	bool is_single;
	//struct shutdown_controller *sdd = &gm->sdc;
	//struct shutdown_controller *sdc = &gm->bm->sdc;
	struct battery_shutdown_unit *sdu = &gm->bm->sdc.bat[gm->id];

	tmp_duraction.tv_sec = 0;
	tmp_duraction.tv_nsec = 0;

	now = ktime_get_boottime();

	pr_debug("%s, %s:soc_zero:%d,ui 1percent:%d,dlpt_shut:%d,under_shutdown_volt:%d\n",
		gm->gauge->name, __func__,
		sdu->shutdown_status.is_soc_zero_percent,
		sdu->shutdown_status.is_uisoc_one_percent,
		sdu->shutdown_status.is_dlpt_shutdown,
		sdu->shutdown_status.is_under_shutdown_voltage);

	if (gm->bm->gm_no == 1)
		is_single = true;
	else
		is_single = false;

	if (sdu->shutdown_status.is_soc_zero_percent) {
		if (current_ui_soc == 0) {
			duraction = ktime_sub(
				now, sdu->pre_time[SOC_ZERO_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			polling++;
			if (is_single && tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_err("soc zero shutdown\n");
				kernel_power_off();
				return next_waketime(polling);
			}
		} else if (current_soc > 0) {
			sdu->shutdown_status.is_soc_zero_percent = false;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
	}

	if (sdu->shutdown_status.is_uisoc_one_percent) {
		now_current = gauge_get_int_property(gm,
			GAUGE_PROP_BATTERY_CURRENT);

		if (current_ui_soc == 0) {
			duraction =
				ktime_sub(
				now, sdu->pre_time[UISOC_ONE_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			if (is_single && tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_err("uisoc one percent shutdown\n");
				kernel_power_off();
				return next_waketime(polling);
			}
		} else if (now_current > 0 && current_soc > 0) {
			sdu->shutdown_status.is_uisoc_one_percent = 0;
			pr_err("disable uisoc_one_percent shutdown cur:%d soc:%d\n",
				now_current, current_soc);
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}

	}

	if (sdu->shutdown_status.is_dlpt_shutdown) {
		duraction = ktime_sub(now, sdu->pre_time[DLPT_SHUTDOWN]);
		tmp_duraction = ktime_to_timespec64(duraction);
		polling++;
		if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
			pr_err("dlpt shutdown count, %d\n",
				(int)tmp_duraction.tv_sec);
			return next_waketime(polling);
		}
	}

	if (sdu->shutdown_status.is_under_shutdown_voltage) {

		int vbatcnt = 0, i;

		if (gm->disableGM30)
			vbat = 4000;
		else
			vbat = bm_get_vsys(gm->bm);

		sdu->batdata[sdu->batidx] = vbat;

		for (i = 0; i < gm->avgvbat_array_size; i++)
			vbatcnt += sdu->batdata[i];
		sdu->avgvbat = vbatcnt / gm->avgvbat_array_size;
		tmp = battery_get_int_property(gm, BAT_PROP_TEMPERATURE);

		pr_debug("%s, lbatcheck vbat:%d avgvbat:%d %d,%d tmp:%d,bound:%d,th:%d %d,en:%d\n",
			gm->gauge->name, vbat,
			sdu->avgvbat,
			sdu->vbat_lt,
			sdu->vbat_lt_lv1,
			tmp,
			gm->bat_voltage_low_bound,
			LOW_TEMP_THRESHOLD,
			gm->low_tmp_bat_voltage_low_bound,
			LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN);

		if (sdu->avgvbat < gm->bat_voltage_low_bound) {
			/* avg vbat less than 3.4v */
			sdu->lowbatteryshutdown = true;
			polling++;

			if (sdu->down_to_low_bat == 0) {
				if (IS_ENABLED(
					LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN)) {
					if (tmp >= LOW_TEMP_THRESHOLD) {
						sdu->down_to_low_bat = 1;
						pr_err("%s, normal tmp, battery voltage is low shutdown\n",
							gm->gauge->name);
						battery_set_property(gm,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else if (sdu->avgvbat <=
						gm->low_tmp_bat_voltage_low_bound) {
						sdu->down_to_low_bat = 1;
						pr_err("%s, cold tmp, battery voltage is low shutdown\n",
							gm->gauge->name);
						battery_set_property(gm,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else
						pr_err("%s, low temp disable low battery sd\n",
							gm->gauge->name);
				} else {
					sdu->down_to_low_bat = 1;
					pr_err("%s, [%s]avg vbat is low to shutdown\n",
						gm->gauge->name, __func__);
					battery_set_property(gm,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
				}
			}

			if ((current_ui_soc == 0) && (sdu->ui_zero_time_flag == 0)) {
				sdu->pre_time[LOW_BAT_VOLT] =
					ktime_get_boottime();
				sdu->ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				duraction = ktime_sub(
					now, sdu->pre_time[LOW_BAT_VOLT]);

				tmp_duraction  = ktime_to_timespec64(duraction);
				if (is_single && tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
					pr_err("low bat shutdown, over %d second\n",
						SHUTDOWN_TIME);
					kernel_power_off();
					return next_waketime(polling);
				}
			}
		} else {
			/* greater than 3.4v, clear status */
			sdu->down_to_low_bat = 0;
			sdu->ui_zero_time_flag = 0;
			sdu->pre_time[LOW_BAT_VOLT] = 0;
			sdu->lowbatteryshutdown = false;
			polling++;
		}

		polling++;
		pr_debug("%s, [%s][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d %d\n",
			gm->gauge->name, __func__,
			sdu->avgvbat, current_ui_soc,
			(int)tmp_duraction.tv_sec,
			sdu->down_to_low_bat, sdu->ui_zero_time_flag,
			(int)sdu->pre_time[LOW_BAT_VOLT],
			sdu->lowbatteryshutdown,
			sdu->batidx, sdu->batdata[sdu->batidx], gm->avgvbat_array_size);

		sdu->batidx++;
		if (sdu->batidx >= gm->avgvbat_array_size)
			sdu->batidx = 0;
	}

	pr_debug(
		"%s, %s %d avgvbat:%d sec:%d lowst:%d\n",
		gm->gauge->name, __func__,
		polling, sdu->avgvbat,
		(int)tmp_duraction.tv_sec, sdu->lowbatteryshutdown);

	return polling;
}

static int bm_shutdown_event_handler(struct mtk_battery_manager *bm)
{
	ktime_t now, duraction;
	struct timespec64 tmp_duraction;
	int polling = 0;
	int now_current1 = 0, now_current2 = 0;
	int current_ui_soc = bm->uisoc;
	int current_gm1_soc = bm->gm1->soc;
	int current_gm2_soc = bm->gm2->soc;
	int vbat1 = 0, vbat2 = 0, chg_vbat = 0;
	int tmp = 25;
	struct battery_shutdown_unit *sdu = &bm->sdc.bmsdu;
	struct battery_shutdown_unit *sdu1 = &bm->sdc.bat[bm->gm1->id];
	struct battery_shutdown_unit *sdu2 = &bm->sdc.bat[bm->gm2->id];

	tmp_duraction.tv_sec = 0;
	tmp_duraction.tv_nsec = 0;

	now = ktime_get_boottime();

	pr_debug("%s,BM:soc_zero:%d,ui 1percent:%d,dlpt_shut:%d,under_shutdown_volt:%d\n",
		__func__,
		sdu->shutdown_status.is_soc_zero_percent,
		sdu->shutdown_status.is_uisoc_one_percent,
		sdu->shutdown_status.is_dlpt_shutdown,
		sdu->shutdown_status.is_under_shutdown_voltage);

	if (sdu->shutdown_status.is_soc_zero_percent) {
		vbat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE);
		vbat2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_BATTERY_VOLTAGE);

		if (current_ui_soc == 0) {
			if (sdu->pre_time[SOC_ZERO_PERCENT] == 0)
				sdu->pre_time[SOC_ZERO_PERCENT] = now;

			duraction = ktime_sub(
				now, sdu->pre_time[SOC_ZERO_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			polling++;
			if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_err("soc zero shutdown\n");
				kernel_power_off();
				return polling;
			}
		} else if (current_gm1_soc > 0 && current_gm2_soc > 0) {
			sdu->shutdown_status.is_soc_zero_percent = false;
			sdu->pre_time[SOC_ZERO_PERCENT] = 0;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
		pr_debug("%s, !!! SOC_ZERO_PERCENT, vbat1:%d, vbat2:%d, current_gm1_soc:%d, current_gm2_soc:%d\n",
			__func__, vbat1, vbat2, current_gm1_soc, current_gm2_soc);
	}

	if (sdu->shutdown_status.is_uisoc_one_percent) {
		now_current1 = gauge_get_int_property(bm->gm1,
			GAUGE_PROP_BATTERY_CURRENT);
		now_current2 = gauge_get_int_property(bm->gm2,
			GAUGE_PROP_BATTERY_CURRENT);

		if (current_ui_soc == 0) {
			if (sdu->pre_time[UISOC_ONE_PERCENT] == 0)
				sdu->pre_time[UISOC_ONE_PERCENT] = now;

			duraction =
				ktime_sub(
				now, sdu->pre_time[UISOC_ONE_PERCENT]);

			polling++;
			tmp_duraction = ktime_to_timespec64(duraction);
			if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_err("uisoc one percent shutdown\n");
				kernel_power_off();
				return polling;
			}
		} else if (now_current1 > 0 && now_current2 > 0 &&
			current_gm1_soc > 0 && current_gm2_soc > 0) {
			sdu->shutdown_status.is_uisoc_one_percent = 0;
			sdu->pre_time[UISOC_ONE_PERCENT] = 0;
			sdu->pre_time[SHUTDOWN_1_TIME] = 0;
			bm->force_ui_zero = 0;
			return polling;
		}
		/* ui_soc is not zero, check it after 10s */
		polling++;

	}

	if (sdu->shutdown_status.is_dlpt_shutdown) {
		duraction = ktime_sub(now, sdu->pre_time[DLPT_SHUTDOWN]);
		tmp_duraction = ktime_to_timespec64(duraction);
		polling++;
		if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
			pr_err("dlpt shutdown count, %d\n",
				(int)tmp_duraction.tv_sec);
			return next_waketime(polling);
		}
	}

	if (sdu->shutdown_status.is_under_shutdown_voltage) {

		int vbatcnt = 0, i;

		if (bm->gm1->disableGM30)
			chg_vbat = 4000;
		else
			chg_vbat = bm_get_vsys(bm);

		sdu->batdata[sdu->batidx] = chg_vbat;

		for (i = 0; i < bm->gm1->avgvbat_array_size; i++)
			vbatcnt += sdu->batdata[i];
		sdu->avgvbat = vbatcnt / bm->gm1->avgvbat_array_size;
		tmp = battery_get_int_property(bm->gm1, BAT_PROP_TEMPERATURE);
		tmp += battery_get_int_property(bm->gm2, BAT_PROP_TEMPERATURE);
		tmp = tmp / 2;

		pr_debug("lbatcheck vbat:%d avgvbat:%d tmp:%d,bound:%d,th:%d %d,en:%d\n",
			chg_vbat,
			sdu->avgvbat,
			tmp,
			bm->gm1->bat_voltage_low_bound,
			LOW_TEMP_THRESHOLD,
			bm->gm1->low_tmp_bat_voltage_low_bound,
			LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN);

		if (sdu->avgvbat < bm->gm1->bat_voltage_low_bound) {
			/* avg vbat less than 3.4v */
			sdu->lowbatteryshutdown = true;
			polling++;

			if (sdu->down_to_low_bat == 0) {
				if (IS_ENABLED(
					LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN)) {
					if (tmp >= LOW_TEMP_THRESHOLD) {
						sdu->down_to_low_bat = 1;
						pr_err("normal tmp, battery voltage is low shutdown\n");
						battery_set_property(bm->gm1,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
						battery_set_property(bm->gm2,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else if (sdu->avgvbat <=
						bm->gm1->low_tmp_bat_voltage_low_bound) {
						sdu->down_to_low_bat = 1;
						pr_err("cold tmp, battery voltage is low shutdown\n");
						battery_set_property(bm->gm1,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
						battery_set_property(bm->gm2,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else
						pr_err("low temp disable low battery sd\n");
				} else {
					sdu->down_to_low_bat = 1;
					pr_err("[%s]avg vbat is low to shutdown\n",
						__func__);
					battery_set_property(bm->gm1,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					battery_set_property(bm->gm2,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
				}
			}

			if ((current_ui_soc == 0) && (sdu->ui_zero_time_flag == 0)) {
				sdu->pre_time[LOW_BAT_VOLT] =
					ktime_get_boottime();
				sdu->ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				duraction = ktime_sub(
					now, sdu->pre_time[LOW_BAT_VOLT]);

				tmp_duraction  = ktime_to_timespec64(duraction);
				polling++;
				if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
					pr_err("low bat shutdown, over %d second\n",
						SHUTDOWN_TIME);
					kernel_power_off();
					return polling;
				}
			}
		} else {
			/* greater than 3.4v, clear status */
			sdu->down_to_low_bat = 0;
			sdu->ui_zero_time_flag = 0;
			sdu->pre_time[LOW_BAT_VOLT] = 0;
			sdu->lowbatteryshutdown = false;
			if (sdu1->shutdown_status.is_under_shutdown_voltage == false &&
				sdu2->shutdown_status.is_under_shutdown_voltage == false)
				sdu->shutdown_status.is_under_shutdown_voltage = false;
			else
				polling++;
		}

		polling++;
		pr_debug("[%s][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d %d\n",
			__func__,
			sdu->avgvbat, current_ui_soc,
			(int)tmp_duraction.tv_sec,
			sdu->down_to_low_bat, sdu->ui_zero_time_flag,
			(int)sdu->pre_time[LOW_BAT_VOLT],
			sdu->lowbatteryshutdown,
			sdu->batidx, sdu->batdata[sdu->batidx], bm->gm1->avgvbat_array_size);

		sdu->batidx++;
		if (sdu->batidx >= bm->gm1->avgvbat_array_size)
			sdu->batidx = 0;
	}

	pr_debug(
		"%s %d avgvbat:%d sec:%d lowst:%d\n",
		__func__,
		polling, sdu->avgvbat,
		(int)tmp_duraction.tv_sec, sdu->lowbatteryshutdown);

	return polling;

}

static enum alarmtimer_restart power_misc_kthread_bm_timer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer[BATTERY_MANAGER]);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->timeout |= 0x1 << BATTERY_MANAGER;
	spin_unlock_irqrestore(&info->slock, flags);
	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart power_misc_kthread_gm2_timer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer[BATTERY_SLAVE]);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->timeout |= 0x1 << BATTERY_SLAVE;
	spin_unlock_irqrestore(&info->slock, flags);
	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart power_misc_kthread_gm1_timer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer[BATTERY_MAIN]);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->timeout |= 0x1 << BATTERY_MAIN;
	spin_unlock_irqrestore(&info->slock, flags);
	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}


static ktime_t check_power_misc_time(struct mtk_battery_manager *bm)
{
	ktime_t ktime;
	int vsys = 0;


	if (bm->disable_quick_shutdown == 1) {
		ktime = ktime_set(10, 0);
		goto out;
	}

	if (bm->gm_no == 1) {
		if (bm->sdc.bat[0].down_to_low_bat == 1) {
			ktime = ktime_set(10, 0);
			goto out;
		}
	} else {
		if (bm->sdc.bmsdu.down_to_low_bat == 1) {
			ktime = ktime_set(10, 0);
			goto out;
		}
	}

	vsys = bm_get_vsys(bm);
	if (vsys > bm->vsys_det_voltage1)
		ktime = ktime_set(10, 0);
	else if (vsys > bm->vsys_det_voltage2)
		ktime = ktime_set(1, 0);
	else
		ktime = ktime_set(0, 100 * NSEC_PER_MSEC);

out:
	pr_debug("%s check average timer vsys:%d, time(msec):%lld disable: %d bound: %d %d\n",
		__func__, vsys, ktime_to_ms(ktime),
			bm->disable_quick_shutdown, bm->vsys_det_voltage1, bm->vsys_det_voltage2);

	return ktime;
}

static int power_misc_routine_thread(void *arg)
{
	struct mtk_battery_manager *bm = arg;
	int ret = 0, i, retry = 0;
	unsigned long flags;
	int pending_flags;
	int polling[BATTERY_SDC_MAX] = {0};
	ktime_t ktime, time_now;

	while (1) {
		pr_debug("[%s] into\n", __func__);
		ret = wait_event_interruptible(bm->sdc.wait_que,
			(bm->sdc.timeout != 0) && !bm->is_suspend);

		if (ret < 0) {
			retry++;
			if (retry < 0xFFFFFFFF)
				continue;
			else {
				bm_err(bm->gm1, "%s something wrong retry: %d\n", __func__, retry);
				break;
			}
		}

		spin_lock_irqsave(&bm->sdc.slock, flags);
		pending_flags = bm->sdc.timeout;
		bm->sdc.timeout = 0;
		spin_unlock_irqrestore(&bm->sdc.slock, flags);

		pr_err("[%s] before %d\n", __func__, pending_flags);

		if(pending_flags & 1<<BATTERY_MAIN)
			polling[BATTERY_MAIN] = shutdown_event_handler(bm->gm1);

		if(pending_flags & 1<<BATTERY_SLAVE && bm->gm_no == 2)
			polling[BATTERY_SLAVE] = shutdown_event_handler(bm->gm2);

		if(pending_flags & 1<<BATTERY_MANAGER && bm->gm_no == 2)
			polling[BATTERY_MANAGER] = bm_shutdown_event_handler(bm);

		spin_lock_irqsave(&bm->sdc.slock, flags);
		pending_flags = bm->sdc.timeout;
		spin_unlock_irqrestore(&bm->sdc.slock, flags);

		pr_err("[%s] after %d M:%d F:%d S:%d\n", __func__,pending_flags, polling[0], polling[1], polling[2]);
		time_now  = ktime_get_boottime();
		ktime = check_power_misc_time(bm);
		for (i = 0; i < BATTERY_SDC_MAX; i++ ) {
			if (pending_flags & (1 << i) || polling[i]) {
				bm->sdc.endtime[i] = ktime_add(time_now, ktime);
				alarm_start(&bm->sdc.kthread_fgtimer[i], bm->sdc.endtime[i]);
			}

		}
		spin_lock_irqsave(&bm->sdc.slock, flags);
		__pm_relax(bm->sdc.sdc_wakelock);
		spin_unlock_irqrestore(&bm->sdc.slock, flags);
	}

	return 0;
}

void mtk_power_misc_init(struct mtk_battery_manager *bm, struct shutdown_controller *sdc)
{
	mutex_init(&sdc->lock);

	alarm_init(&sdc->kthread_fgtimer[BATTERY_MAIN], ALARM_BOOTTIME,
		power_misc_kthread_gm1_timer_func);

	if (bm->gm_no == 2) {
		alarm_init(&sdc->kthread_fgtimer[BATTERY_SLAVE], ALARM_BOOTTIME,
			power_misc_kthread_gm2_timer_func);
		alarm_init(&sdc->kthread_fgtimer[BATTERY_MANAGER], ALARM_BOOTTIME,
			power_misc_kthread_bm_timer_func);
	}

	init_waitqueue_head(&sdc->wait_que);

	if (bm->gm_no == 2) {
		sdc->bmsdu.type = BATTERY_MANAGER;
		sdc->bat[0].type = BATTERY_MAIN;
		sdc->bat[1].type = BATTERY_SLAVE;

		sdc->bat[0].gm = bm->gm1;
		sdc->bat[1].gm = bm->gm2;
	} else if (bm->gm_no == 1) {
		sdc->bmsdu.type = BATTERY_MANAGER;
		sdc->bat[0].type = BATTERY_MAIN;
		sdc->bat[0].gm = bm->gm1;
	}
	kthread_run(power_misc_routine_thread, bm, "power_misc_thread");
}


void bm_check_bootmode(struct device *dev,
	struct mtk_battery_manager *bm)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		pr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			pr_err("%s: failed to get atag,boot\n", __func__);
		else {
			pr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			bm->bootmode = tag->bootmode;
			bm->boottype = tag->boottype;
		}
	}
}

static void bm_update_status(struct mtk_battery_manager *bm)
{

	int vbat1, vbat2, ibat1, ibat2, vbat3 =0, vbat4 = 0;
	int car1, car2;
	struct mtk_battery *gm1;
	struct mtk_battery *gm2;
	struct fgd_cmd_daemon_data *d1;
	struct fgd_cmd_daemon_data *d2;

	if (bm->gm_no == 2) {

		gm1 = bm->gm1;
		gm2 = bm->gm2;
		d1 = &gm1->daemon_data;
		d2 = &gm2->daemon_data;
		vbat3 = get_charger_vbat(bm);
		vbat4 = bm_get_vsys(bm);

		vbat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE);
		ibat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_CURRENT);
		vbat2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_BATTERY_VOLTAGE);
		ibat2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_BATTERY_CURRENT);
		car1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_COULOMB);
		car2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_COULOMB);

		pr_err("[%s] uisoc:%d %d %d soc:%d %d vbat:%d %d %d %d ibat:%d %d car:%d %d\n", __func__,
			bm->uisoc,
			bm->gm1->ui_soc, bm->gm2->ui_soc,
			bm->gm1->soc, bm->gm2->soc,
			vbat1, vbat2, vbat3, vbat4,
			ibat1, ibat2,
			car1, car2);

		pr_err("[bm_update_daemon1][%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
			d1->uisoc, d1->fg_c_soc, d1->fg_v_soc, d1->soc, d1->fg_c_d0_soc, d1->car_c, d1->fg_v_d0_soc,
			d1->car_v, d1->qmxa_t_0ma, d1->quse, d1->tmp, d1->vbat, d1->iavg, d1->aging_factor,
			d1->loading_factor1, d1->loading_factor2, d1->g_zcv_data, d1->g_zcv_data_soc,
			d1->g_zcv_data_mah, d1->tmp_show_ag, d1->tmp_bh_ag);

		pr_err("[bm_update_daemon2][%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
			d2->uisoc, d2->fg_c_soc, d2->fg_v_soc, d2->soc, d2->fg_c_d0_soc, d2->car_c, d2->fg_v_d0_soc,
			d2->car_v, d2->qmxa_t_0ma, d2->quse, d2->tmp, d2->vbat, d2->iavg, d2->aging_factor,
			d2->loading_factor1, d2->loading_factor2, d2->g_zcv_data, d2->g_zcv_data_soc,
			d2->g_zcv_data_mah, d2->tmp_show_ag, d2->tmp_bh_ag);

	} else if (bm->gm_no ==1) {
		vbat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE);
		ibat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_CURRENT);
		car1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_COULOMB);

		pr_err("[%s] uisoc:%d %d soc:%d vbat:%d ibat:%d car:%d\n", __func__,
			bm->uisoc, bm->gm1->ui_soc,
			bm->gm1->soc,
			vbat1, ibat1, car1);
	}
}

static int battery_manager_routine_thread(void *arg)
{
	struct mtk_battery_manager *bm = arg;
	int ret = 0, retry = 0;
	struct timespec64 end_time, tmp_time_now;
	ktime_t ktime, time_now;
	unsigned long flags;

	while (1) {
		ret = wait_event_interruptible(bm->wait_que,
			(bm->bm_update_flag  > 0) && !bm->is_suspend);

		if (ret < 0) {
			retry++;
			if (retry < 0xFFFFFFFF)
				continue;
			else {
				bm_err(bm->gm1, "%s something wrong retry: %d\n", __func__, retry);
				break;
			}
		}

		spin_lock_irqsave(&bm->slock, flags);
		bm->bm_update_flag = 0;
		if (!bm->bm_wakelock->active)
			__pm_stay_awake(bm->bm_wakelock);
		spin_unlock_irqrestore(&bm->slock, flags);


		bm_update_status(bm);

		time_now  = ktime_get_boottime();
		tmp_time_now  = ktime_to_timespec64(time_now);
		end_time.tv_sec = tmp_time_now.tv_sec + 10;
		end_time.tv_nsec = tmp_time_now.tv_nsec;
		bm->endtime = end_time;
#ifdef BM_USE_HRTIMER
		ktime = ktime_set(10, 0);
		hrtimer_start(&bm->bm_hrtimer, ktime, HRTIMER_MODE_REL);
#endif
#ifdef BM_USE_ALARM_TIMER
		ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&bm->bm_alarmtimer, ktime);
#endif
		spin_lock_irqsave(&bm->slock, flags);
		__pm_relax(bm->bm_wakelock);
		spin_unlock_irqrestore(&bm->slock, flags);

	}

	return 0;
}

void battery_manager_routine_wakeup(struct mtk_battery_manager *bm)
{
	unsigned long flags;

	spin_lock_irqsave(&bm->slock, flags);
	bm->bm_update_flag = 1;
	if (!bm->bm_wakelock->active)
		__pm_stay_awake(bm->bm_wakelock);
	spin_unlock_irqrestore(&bm->slock, flags);
	wake_up(&bm->wait_que);
}

#ifdef BM_USE_HRTIMER
enum hrtimer_restart battery_manager_thread_hrtimer_func(struct hrtimer *timer)
{
	struct mtk_battery_manager *bm;

	bm = container_of(timer,
		struct mtk_battery_manager, bm_hrtimer);
	battery_manager_routine_wakeup(bm);
	return HRTIMER_NORESTART;
}

void battery_manager_thread_hrtimer_init(struct mtk_battery_manager *bm)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	hrtimer_init(&bm->bm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bm->bm_hrtimer.function = battery_manager_thread_hrtimer_func;
	hrtimer_start(&bm->bm_hrtimer, ktime, HRTIMER_MODE_REL);
}
#endif


#ifdef CONFIG_PM
static int bm_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	ktime_t ktime_now;
	struct timespec64 now;
	struct mtk_battery_manager *bm;
	unsigned long flags;
	int i, pending_flags, wake_up_power = 0;

	bm = container_of(notifier,
		struct mtk_battery_manager, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		bm->is_suspend = true;
		pr_err("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		bm->is_suspend = false;
		pr_err("%s: enter PM_POST_SUSPEND\n", __func__);
		ktime_now = ktime_get_boottime();
		now = ktime_to_timespec64(ktime_now);

		if (timespec64_compare(&now, &bm->endtime) >= 0 &&
			bm->endtime.tv_sec != 0 &&
			bm->endtime.tv_nsec != 0) {
			pr_err("%s: alarm timeout, wake up charger\n",
				__func__);

			spin_lock_irqsave(&bm->slock, flags);
			__pm_relax(bm->bm_wakelock);
			spin_unlock_irqrestore(&bm->slock, flags);
			bm->endtime.tv_sec = 0;
			bm->endtime.tv_nsec = 0;
			battery_manager_routine_wakeup(bm);
		}

		spin_lock_irqsave(&bm->sdc.slock, flags);
		pending_flags = bm->sdc.timeout;
		__pm_relax(bm->sdc.sdc_wakelock);
		spin_unlock_irqrestore(&bm->sdc.slock, flags);
		for (i = 0; i < BATTERY_SDC_MAX; i++ ) {
			if (pending_flags & (1 << i) &&
				ktime_compare(ktime_now, bm->sdc.endtime[i]) >= 0) {
				pr_err("%s: alarm timeout, wake up power %d\n",
					__func__, i);
				spin_lock_irqsave(&bm->sdc.slock, flags);
				bm->sdc.timeout |= 0x1 << i;
				spin_unlock_irqrestore(&bm->sdc.slock, flags);
				wake_up_power = 1;
			}

		}
		if (wake_up_power)
			wake_up_power_misc(&bm->sdc);

		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

#ifdef BM_USE_ALARM_TIMER
enum alarmtimer_restart battery_manager_thread_alarm_func(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery_manager *bm;
	unsigned long flags;

	bm = container_of(alarm,
		struct mtk_battery_manager, bm_alarmtimer);

	if (bm->is_suspend == false) {
		pr_err("%s: not suspend, wake up charger\n", __func__);
		battery_manager_routine_wakeup(bm);
	} else {
		pr_err("%s: alarm timer timeout\n", __func__);
			spin_lock_irqsave(&bm->slock, flags);
		if (!bm->bm_wakelock->active)
			__pm_stay_awake(bm->bm_wakelock);
		spin_unlock_irqrestore(&bm->slock, flags);
	}

	return ALARMTIMER_NORESTART;
}

void battery_manager_thread_alarm_init(struct mtk_battery_manager *bm)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	alarm_init(&bm->bm_alarmtimer, ALARM_BOOTTIME,
		battery_manager_thread_alarm_func);
	alarm_start(&bm->bm_alarmtimer, ktime);
}
#endif

static void bm_send_cmd(struct mtk_battery_manager *bm, enum manager_cmd cmd, int val)
{
	int ret = 0;

	if (bm->gm1 != NULL && bm->gm1->manager_send != NULL)
		ret = bm->gm1->manager_send(bm->gm1, cmd, val);
	else
		pr_err("%s gm1->manager_send is null\n", __func__);

	if (bm->gm_no == 2) {
		if (bm->gm2 != NULL && bm->gm2->manager_send != NULL)
			ret |= bm->gm2->manager_send(bm->gm2, cmd, val);
		else
			pr_err("%s gm2->manager_send is null\n", __func__);
	}

	if (ret < 0)
		pr_err("%s manager_send return faiil\n",__func__);
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

static int bm_update_psy_property(struct mtk_battery *gm, enum bm_psy_prop prop)
{
	int ret_val = 0, ret = 0;
	int curr_now = 0, curr_avg = 0, voltage_now = 0;

	switch (prop) {
	case CURRENT_NOW:
		ret = gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
			&curr_now, 1);

		if (ret == -EHOSTDOWN)
			ret_val = gm->ibat;
		else {
			ret_val = curr_now;
			gm->ibat = curr_now;
		}
		break;
	case CURRENT_AVG:
		ret = gauge_get_property_control(gm, GAUGE_PROP_AVERAGE_CURRENT,
			&curr_avg, 1);

		if (ret == -EHOSTDOWN)
			ret_val = gm->ibat;
		else
			ret_val = curr_avg;
		break;
	case VOLTAGE_NOW:
		/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
		/* 6= ATE_factory_boot */
		if (gm->bootmode == 1 || gm->bootmode == 4
			|| gm->bootmode == 5 || gm->bootmode == 6) {
			ret_val = 4000;
			break;
		}

		if (gm->disableGM30)
			voltage_now = 4000;
		else
			ret = gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
				&voltage_now, 1);

		if (ret == -EHOSTDOWN)
			ret_val = gm->vbat;
		else {
			gm->vbat = voltage_now;
			ret_val = voltage_now;
		}
		break;
	case TEMP:
		ret_val = battery_get_int_property(gm, BAT_PROP_TEMPERATURE);
		break;
	case QMAX_DESIGN:
		if (gm->battery_id < 0 || gm->battery_id >= TOTAL_BATTERY_NUMBER)
			ret_val = gm->fg_table_cust_data.fg_profile[0].q_max * 10;
		else
			ret_val = gm->fg_table_cust_data.fg_profile[gm->battery_id].q_max * 10;
		break;
	case QMAX:
		ret_val = gm->daemon_data.qmxa_t_0ma;
		break;
	default:
		break;
	}
	return ret_val;
}

static int bs_psy_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0, qmax = 0, cycle = 0;
	int curr_now = 0;
	int curr_avg = 0;
	int remain_ui = 0, remain_mah = 0;
	int time_to_full = 0;
	int q_max_uah = 0;
	int volt_now = 0;
	int count = 0;
	int temp = 0;
	struct mtk_battery_manager *bm;
	struct battery_data *bs_data;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#if IS_ENABLED(CONFIG_SEC_MTK_CHARGER)
	union power_supply_propval value = {0, };
#endif
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
#endif

	bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);
	bs_data = &bm->bs_data;

	/* gauge_get_property should check return value */
	/* to avoid i2c suspend but query by other module */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	switch ((int)psp) {
#else
	switch (psp) {
#endif
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bs_data->bat_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bs_data->bat_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bs_data->bat_present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = bs_data->bat_technology;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT: //sum(cycle * qmax) / sum(qmax)
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				cycle += (bm->gm1->bat_cycle + 1) *
					bm_update_psy_property(bm->gm1, QMAX_DESIGN);
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				cycle += (bm->gm2->bat_cycle + 1) *
					bm_update_psy_property(bm->gm2, QMAX_DESIGN);
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);
			}
		if (qmax != 0)
			val->intval = cycle / qmax;
		break;
	case POWER_SUPPLY_PROP_CAPACITY: //sum(uisoc)
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = bm->gm1->ss_precise_soc * 10;
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			val->intval = bm->gm1->ss_precise_ui_soc;
			pr_info("%s : ui SOC(%d%%)\n", __func__, val->intval);
		} else {
#else
		{
#endif
			/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
			/* 6= ATE_factory_boot */
			if (bm->bootmode == 1 || bm->bootmode == 4
				|| bm->bootmode == 5 || bm->bootmode == 6) {
				val->intval = 75;
				break;
			}

			if (bm->gm1->fixed_uisoc != 0xffff)
				val->intval = bm->gm1->fixed_uisoc;
			else
				val->intval = bs_data->bat_capacity;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			if (val->intval < 0) {
				pr_info("%s : real / fake capacity(%d%%/%d%%)\n",
					__func__, val->intval,
				SEC_BATTERY_FAKE_CAPACITY);
				val->intval = SEC_BATTERY_FAKE_CAPACITY;
				bm->gm1->is_fake_soc = 1;
			}
#endif
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				curr_now += bm_update_psy_property(bm->gm1, CURRENT_NOW);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				curr_now += bm_update_psy_property(bm->gm2, CURRENT_NOW);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = curr_now * 100;
		else
			val->intval = curr_now / 10;
#else
		val->intval = curr_now * 100;
#endif
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				curr_avg += bm_update_psy_property(bm->gm1, CURRENT_AVG);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				curr_avg += bm_update_psy_property(bm->gm2, CURRENT_AVG);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = curr_avg * 100;
		else
			val->intval = curr_avg / 10;
#else
		val->intval = curr_avg * 100;
#endif
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);

		val->intval = qmax * 100;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);

		val->intval = bs_data->bat_capacity * qmax;
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
#endif
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_SEC_MTK_CHARGER)
		if (bm->gm1->f_mode == OB_MODE) {
			psy_do_property("sec-mtk-charger", get,
				POWER_SUPPLY_EXT_PROP_BATT_VSYS, value);
			val->intval = value.intval;
			ret = 0;
			break;
		}
#endif
		count = 0;
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				volt_now += bm_update_psy_property(bm->gm1, VOLTAGE_NOW);
				count += 1;
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				volt_now += bm_update_psy_property(bm->gm2, VOLTAGE_NOW);
				count += 1;
			}
		if (count != 0)
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		{
			if (val->intval == SEC_BATTERY_VOLTAGE_MV)
				val->intval = volt_now / count;
			else
				val->intval = volt_now / count * 1000;
		}
#else
			val->intval = volt_now / count * 1000;
#endif
		ret = 0;
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
#endif
	case POWER_SUPPLY_PROP_TEMP:
		count = 0;
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				temp += bm_update_psy_property(bm->gm1, TEMP);
				count += 1;
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				temp += bm_update_psy_property(bm->gm2, TEMP);
				count += 1;
			}
		if (count != 0)
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		{
			if (val->intval == SEC_BATTERY_TEMP_ADC) {
				pr_info("%s : mtk tbat temp(%d), temp adc(%d)\n",
					__func__, val->intval, bm->gm1->tbat_adc);
				val->intval = bm->gm1->tbat_adc;
			} else
				val->intval = temp / count * 10;
		}
#else
			val->intval = temp / count * 10;
#endif
		ret = 0;
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			pr_info("[%s] gm->fg_table_cust_data.fg_profile[gm->battery_id].q_max %d\n",
				__func__, val->intval);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = bm->gm1->daemon_data.aging_factor / 100;
		break;
#endif
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = check_cap_level(bs_data->bat_capacity);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		/* full or unknown must return 0 */
		ret = check_cap_level(bs_data->bat_capacity);
		if ((ret == POWER_SUPPLY_CAPACITY_LEVEL_FULL) ||
			(ret == POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN)) {
			val->intval = 0;
			break;
		}

		remain_ui = 100 - bs_data->bat_capacity;
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				curr_avg += bm_update_psy_property(bm->gm1, CURRENT_AVG);
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				curr_avg += bm_update_psy_property(bm->gm2, CURRENT_AVG);
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);
			}

		remain_mah = remain_ui * qmax / 10;
		if (curr_avg != 0)
			time_to_full = remain_mah * 3600 / curr_avg / 10;

		val->intval = abs(time_to_full);
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (check_cap_level(bs_data->bat_capacity) ==
			POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN) {
			val->intval = 0;
			break;
		}

		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);

		q_max_uah = qmax * 100;
		if (q_max_uah <= 100000) {
			pr_debug("%s gm_no:%d, q_max:%d q_max_uah:%d\n",
				__func__, bm->gm_no, qmax, q_max_uah);
			q_max_uah = 100001;
		}
		val->intval = q_max_uah;

		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (IS_ERR_OR_NULL(bs_data->chg_psy)) {
			bs_data->chg_psy = devm_power_supply_get_by_phandle(
				bm->dev, "charger");
			pr_err("%s retry to get chg_psy\n", __func__);
		}
		if (IS_ERR_OR_NULL(bs_data->chg_psy)) {
			pr_err("%s Couldn't get chg_psy\n", __func__);
			ret = 4350;
		} else {
			ret = power_supply_get_property(bs_data->chg_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
			if (ret < 0) {
				pr_err("get CV property fail\n");
				ret = 4350;
			}
		}
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_PMIC_BAT_VOLTAGE:
		{
			int j, k, ocv, ocv_data[10];

			for (j = 0; j < 10; j++) {
				/* There is no OCV value which can read after booting complete. */
				/* There is only OCV value which was saved in booting time. */
				/* ====>  gauge_get_int_property(GAUGE_PROP_BOOT_ZCV) */
				ret = gauge_get_property_control(bm->gm1, GAUGE_PROP_BATTERY_VOLTAGE,
				&bs_data->bat_batt_vol, 1);

				if (ret == -EHOSTDOWN)
					ocv_data[j] = bm->gm1->vbat;
				else
					ocv_data[j] = bs_data->bat_batt_vol;
			}
			for (j = 1; j < 10; j++) {
				ocv = ocv_data[j];
				k = j;
				while (k > 0 && ocv_data[k-1] > ocv) {
					ocv_data[k] = ocv_data[k-1];
					k--;
				}
				ocv_data[k] = ocv;
			}
			for (j = 0; j < 10; j++)
				pr_info("%s: [%d] %d\n", __func__, j, ocv_data[j]);

			ocv = 0;
			for (j = 2; j < 8; j++)
				ocv += ocv_data[j];

			val->intval = ocv / 6;
		}
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			break;
		case POWER_SUPPLY_EXT_PROP_BATTERY_ID:
			val->intval = bm->gm1->battery_id;
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_DUMP:
			memset(bm->gm1->d_buf, 0x0, sizeof(bm->gm1->d_buf));
			/* capacity_max not supported, set to 0 */
			snprintf(bm->gm1->d_buf, sizeof(bm->gm1->d_buf),
				"%d,%d,%d", bm->gm1->vbat * 1000, bm->gm1->ui_soc * 10, 0);
			val->strval = bm->gm1->d_buf;
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	default:
		ret = -EINVAL;
		break;
		}

	//pr_err("%s psp:%d ret:%d val:%d", __func__, psp, ret, val->intval);

	return ret;
}

static int bs_psy_set_property(struct power_supply *psy,
	enum power_supply_property psp,
	const union power_supply_propval *val)
{
	int ret = 0;
	struct mtk_battery_manager *bm;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
#endif

	bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	switch ((int)psp) {
#else
	switch (psp) {
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL) {
			bm_send_cmd(bm, MANAGER_NOTIFY_CHR_FULL, 0);
			bm->gm1->is_full = 0;
			pr_info("%s: Battery Full!\n", __func__);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		/* No need to run if SOC is already 100% */
		if (val->intval == 100)
			break;
		if (bm->gm1->is_full != 1) {
			bm_send_cmd(bm, MANAGER_NOTIFY_CHR_FULL, 0);
			bm->gm1->is_full = 1;
			pr_info("%s: Force Battery Full!\n", __func__);
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == SEC_BAT_CHG_MODE_CHARGING) {
				pr_info("%s: Battery Charging!\n", __func__);
				bm_send_cmd(bm, MANAGER_SW_BAT_CYCLE_ACCU, 0);
				bm->bs_data.bat_status = POWER_SUPPLY_STATUS_CHARGING;
				battery_update(bm);
			} else {
				pr_info("%s: Battery Discharging!\n", __func__);
				bm->gm1->is_full = 0;
				bm_send_cmd(bm, MANAGER_SW_BAT_CYCLE_ACCU, 0);
				bm->bs_data.bat_status =
					POWER_SUPPLY_STATUS_DISCHARGING;
				battery_update(bm);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_F_MODE:
			bm->gm1->f_mode = val->intval;
#if IS_ENABLED(CONFIG_SEC_MTK_CHARGER)
			pr_info("%s: mtk-fg-battery: FG f_mode: %s\n", __func__,
					BOOT_MODE_STRING[bm->gm1->f_mode]);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
			if (bm->gm1->f_mode == OB_MODE)
				bm_send_cmd(bm, MANAGER_DISABLE_FG, 0);
#endif
#endif
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (val->intval > 0)
			bm_send_cmd(bm, MANAGER_DYNAMIC_CV, val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
		}

	pr_err("%s psp:%d ret:%d val:%d",
		__func__, psp, ret, val->intval);

	return ret;
}

static void mtk_battery_external_power_changed(struct power_supply *psy)
{
	struct mtk_battery_manager *bm;
	struct battery_data *bs_data;
	union power_supply_propval online = {0}, status = {0}, vbat0 = {0};
	union power_supply_propval prop_type = {0};
	int cur_chr_type = 0, old_vbat0 = 0;

	struct power_supply *chg_psy = NULL;
	struct power_supply *dv2_chg_psy = NULL;
	int ret = 0;

	bm = psy->drv_data;
	bs_data = &bm->bs_data;
	chg_psy = bs_data->chg_psy;

	if (bm->gm1->is_probe_done == false) {
		pr_err("[%s] gm_no:%d battery probe is not rdy:%d\n",
			__func__, bm->gm_no, bm->gm1->is_probe_done);
		return;
	}

	if (bm->gm_no==2) {
		if (bm->gm2->is_probe_done == false) {
			pr_err("[%s] gm_no:%d battery probe is not rdy:%d\n",
				__func__, bm->gm_no, bm->gm2->is_probe_done);
			return;
		}
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(bm->dev,
						       "charger");
		pr_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		ret |= power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);

		ret |= power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);

		ret |= power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);

		if (ret < 0)
			pr_debug("%s ret: %d\n", __func__, ret);

		if (!online.intval) {
			bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
				bs_data->bat_status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;

				dv2_chg_psy = power_supply_get_by_name("mtk-mst-div-chg");
				if (!IS_ERR_OR_NULL(dv2_chg_psy)) {
					ret = power_supply_get_property(dv2_chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &online);
					if (online.intval) {
						bs_data->bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
						status.intval =
							POWER_SUPPLY_STATUS_CHARGING;
					}
					power_supply_put(dv2_chg_psy);
				}
			} else {
				bs_data->bat_status =
					POWER_SUPPLY_STATUS_CHARGING;
			}
			bm_send_cmd(bm, MANAGER_SW_BAT_CYCLE_ACCU, 0);
		}

		if (status.intval == POWER_SUPPLY_STATUS_FULL
			&& bm->b_EOC != true) {
			pr_err("POWER_SUPPLY_STATUS_FULL, EOC\n");
			gauge_get_int_property(bm->gm1, GAUGE_PROP_BAT_EOC);
			bm_send_cmd(bm, MANAGER_NOTIFY_CHR_FULL, 0);
			pr_err("GAUGE_PROP_BAT_EOC done\n");
			bm->b_EOC = true;
		} else
			bm->b_EOC = false;

		battery_update(bm);

		/* check charger type */
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop_type);

		/* plug in out */
		cur_chr_type = prop_type.intval;

		if (cur_chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			if (bm->chr_type != POWER_SUPPLY_TYPE_UNKNOWN)
				pr_err("%s chr plug out\n", __func__);
		} else {
			if (bm->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
				bm_send_cmd(bm, MANAGER_WAKE_UP_ALGO, FG_INTR_CHARGER_IN);
		}

		if (bm->gm1->vbat0_flag != vbat0.intval) {
			old_vbat0 = bm->gm1->vbat0_flag;
			bm->gm1->vbat0_flag = vbat0.intval;
			if (bm->gm_no == 2)
				bm->gm2->vbat0_flag = vbat0.intval;
			bm_send_cmd(bm, MANAGER_WAKE_UP_ALGO, FG_INTR_NAG_C_DLTV);
			pr_err("fuelgauge NAFG for calibration,vbat0[o:%d n:%d]\n",
				old_vbat0, vbat0.intval);
		}
	}

	pr_err("%s event, name:%s online:%d, status:%d, EOC:%d, cur_chr_type:%d old:%d, vbat0:[o:%d n:%d]\n",
		__func__, psy->desc->name, online.intval, status.intval,
		bm->b_EOC, cur_chr_type, bm->chr_type,
		old_vbat0, vbat0.intval);

	bm->chr_type = cur_chr_type;
}

void bm_battery_service_init(struct mtk_battery_manager *bm)
{
	struct battery_data *bs_data;

	bs_data = &bm->bs_data;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	bs_data->psd.name = "mtk-fg-battery";
#else
	bs_data->psd.name = "battery";
#endif
	bs_data->psd.type = POWER_SUPPLY_TYPE_BATTERY;
	bs_data->psd.properties = battery_props;
	bs_data->psd.num_properties = ARRAY_SIZE(battery_props);
	bs_data->psd.get_property = bs_psy_get_property;
	bs_data->psd.set_property = bs_psy_set_property;
	bs_data->psd.external_power_changed =
		mtk_battery_external_power_changed;
	bs_data->psy_cfg.drv_data = bm;

	bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING,
	bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD,
	bs_data->bat_present = 1,
	bs_data->bat_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	bs_data->bat_capacity = -1,
	bs_data->bat_batt_vol = 0,
	bs_data->bat_batt_temp = 0,

	bm->bs_data.psy =
	power_supply_register(
		bm->dev, &bm->bs_data.psd, &bm->bs_data.psy_cfg);
	if (IS_ERR(bm->bs_data.psy))
		pr_err("[BAT_probe] power_supply_register Battery Fail !!\n");

	bm->gm1->fixed_uisoc = 0xffff;
	if (bm->gm_no == 2)
		bm->gm2->fixed_uisoc = 0xffff;

	mtk_battery_external_power_changed(bm->bs_data.psy);
}

void mtk_bm_send_to_user(struct mtk_battery_manager *bm, u32 pid,
	int seq, struct afw_header *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size = reply_msg->data_len + AFW_MSG_HEADER_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret = -1;

	if (bm == NULL)
		return;

	//pr_err("[%s]id:%d cmd:%d datalen:%d\n", __func__,
	//	reply_msg->instance_id, reply_msg->cmd, reply_msg->data_len);

	if (pid == 0) {
		pr_err("[%s]=>pid is 0 , id:%d cmd:%d\n", __func__,
			reply_msg->instance_id, reply_msg->cmd);
		return;
	}


	reply_msg->identity = AFW_MAGIC;

	if (in_interrupt())
		skb = alloc_skb(len, GFP_ATOMIC);
	else
		skb = alloc_skb(len, GFP_KERNEL);

	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	if (bm->mtk_bm_sk != NULL) {
		ret = netlink_unicast
			(bm->mtk_bm_sk, skb, pid, MSG_DONTWAIT);
		//pr_err("[%s]netlink_unicast , id:%d cmd:%d\n",
		//	__func__, reply_msg->instance_id, reply_msg->cmd);
	} else
		pr_err("[%s]bm->mtk_bm_sk is  NULL\n", __func__);
	if (ret < 0) {
		pr_err("[%s]send failed ret=%d pid=%d\n", __func__, ret, pid);
		return;
	}
}

void mtk_bm_handler(struct mtk_battery *gm,
	int seq, struct afw_header *reply_msg)
{
	static struct mtk_battery_manager *bm;

	//pr_err("[%s]id:%d =>cmd:%d subcmd:%d %d hash %d\n", __func__,
	//	gm->id, reply_msg->cmd, reply_msg->subcmd, reply_msg->subcmd_para1, reply_msg->hash);

	bm = gm->bm;
	if (bm == NULL)
		bm = get_mtk_battery_manager();

	if (bm != NULL) {
		if (bm->gm1 == gm) {
			reply_msg->instance_id = gm->id;
			mtk_bm_send_to_user(bm, bm->fgd_pid,
				seq, reply_msg);
		} else if (bm->gm2 == gm) {
			reply_msg->instance_id = gm->id;
			mtk_bm_send_to_user(bm, bm->fgd_pid,
				seq, reply_msg);
		} else
			pr_err("[%s]gm is incorrect !n", __func__);
	} else
		pr_err("[%s]bm is incorrect !n", __func__);
}

static void fg_cmd_check(struct afw_header *msg)
{
	while (msg->subcmd == 0 &&
		msg->subcmd_para1 != AFW_MSG_HEADER_LEN) {
		pr_err("fuel gauge version error cmd:%d %d\n",
			msg->cmd,
			msg->subcmd);
		msleep(10000);
		break;
	}
}

static void mtk_battery_manager_handler(struct mtk_battery_manager *bm, void *nl_data,
	struct afw_header *ret_msg, int seq)
{
	struct afw_header *msg;
	struct mtk_battery *gm;
	bool send = true;

	if (bm == NULL) {
		bm = get_mtk_battery_manager();
		if (bm == NULL)
			return;
	}
	msg = nl_data;
//	ret_msg->nl_cmd = msg->nl_cmd;
	ret_msg->cmd = msg->cmd;
	ret_msg->instance_id = msg->instance_id;
	ret_msg->hash = msg->hash;
	ret_msg->datatype = msg->datatype;


	//pr_err("[%s] gm id:%d cmd:%d type:%d hash:%d\n",
	//	__func__, msg->instance_id, msg->cmd, msg->datatype, msg->hash);

	//msleep(3000);
	//mdelay(3000);
	if (msg->instance_id == 0)
		gm = bm->gm1;
	else if (msg->instance_id == 1)
		gm = bm->gm2;
	else {
		pr_err("[%s] can not find gm, id:%d cmd:%d\n", __func__, msg->instance_id, msg->cmd);
		return;
	}

	switch (msg->cmd) {

	case AFW_CMD_PRINT_LOG:
	case FG_DAEMON_CMD_PRINT_LOG:
	{
		fg_cmd_check(msg);
		pr_err("[%sd]%s", gm->gauge->name,&msg->data[0]);

	}
	break;
	case AFW_CMD_SET_PID:
	//case FG_DAEMON_CMD_SET_DAEMON_PID:
	{
		unsigned int ino = bm->gm_no;

		fg_cmd_check(msg);
		/* check is daemon killed case*/
		if (bm->fgd_pid == 0) {
			memcpy(&bm->fgd_pid, &msg->data[0],
				sizeof(bm->fgd_pid));
			pr_err("[K]FG_DAEMON_CMD_SET_DAEMON_PID = %d(first launch) ino:%d\n",
				bm->fgd_pid, ino);
		} else {
			memcpy(&bm->fgd_pid, &msg->data[0],
				sizeof(bm->fgd_pid));

		/* WY_FIX: */
			pr_err("[K]FG_DAEMON_CMD_SET_DAEMON_PID=%d,kill daemon:%d init_flag:%d (re-launch) ino:%d\n",
				bm->fgd_pid,
				gm->Bat_EC_ctrl.debug_kill_daemontest,
				gm->init_flag,
				ino);
			if (gm->Bat_EC_ctrl.debug_kill_daemontest != 1 &&
				gm->init_flag == 1)
				gm->fg_cust_data.dod_init_sel = 14;
			else
				gm->fg_cust_data.dod_init_sel = 0;
		}
		ret_msg->data_len += sizeof(ino);
		memcpy(ret_msg->data, &ino, sizeof(ino));
	}
	break;
	default:
	{
		if (msg->instance_id == 0) {
			if (bm->gm1 != NULL && bm->gm1->netlink_handler != NULL) {
				bm->gm1->netlink_handler(bm->gm1, nl_data, ret_msg);
			} else {
				pr_err("[%s]gm1 netlink_handler is NULL\n", __func__);
				send = false;
			}
		} else if (msg->instance_id == 1) {
			if (bm->gm2 != NULL && bm->gm2->netlink_handler != NULL) {
				bm->gm2->netlink_handler(bm->gm2, nl_data, ret_msg);
			} else {
				pr_err("[%s]gm2 netlink_handler is NULL\n", __func__);
				send = false;
			}
		} else {
			pr_err("[%s]gm instance id is not supported:%d\n", __func__, msg->instance_id);
			send = false;
		}
	}
	break;
	}

	if (send == true)
		mtk_bm_send_to_user(bm, bm->fgd_pid, seq, ret_msg);

}

void mtk_bm_netlink_handler(struct sk_buff *skb)
{
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct afw_header *fgd_msg, *fgd_ret_msg;
	int size = 0;
	static struct mtk_battery_manager *bm;

	if (bm == NULL)
		bm = get_mtk_battery_manager();

	nlh = (struct nlmsghdr *)skb->data;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	fgd_msg = (struct afw_header *)data;

	if (fgd_msg->identity != AFW_MAGIC) {
		pr_err("[%s]not correct MTKFG netlink packet!%d\n",
			__func__, fgd_msg->identity);
		return;
	}

	size = fgd_msg->ret_data_len + AFW_MSG_HEADER_LEN;

	if (size > (PAGE_SIZE << 1))
		fgd_ret_msg = vmalloc(size);
	else {
		if (in_interrupt())
			fgd_ret_msg = kmalloc(size, GFP_ATOMIC);
		else
			fgd_ret_msg = kmalloc(size, GFP_KERNEL);
	}

	if (fgd_ret_msg == NULL) {
		if (size > PAGE_SIZE)
			fgd_ret_msg = vmalloc(size);

		if (fgd_ret_msg == NULL)
			return;
	}

	memset(fgd_ret_msg, 0, size);

	mtk_battery_manager_handler(bm, data, fgd_ret_msg, seq);

	kvfree(fgd_ret_msg);

#ifdef WY_FIX
	if (fgd_msg->instance_id == 0) {
		if (bm->gm1 != NULL && bm->gm1->netlink_handler != NULL) {
			bm->gm1->netlink_handler(bm->gm1, data, fgd_ret_msg);
			mtk_bm_send_to_user(bm, pid, seq, fgd_ret_msg);
		} else
			pr_err("[%s]gm1 netlink_handler is NULL\n", __func__);
	} else if (fgd_msg->instance_id == 1) {
		if (bm->gm2 != NULL && bm->gm2->netlink_handler != NULL) {
			bm->gm2->netlink_handler(bm->gm2, data, fgd_ret_msg);
			mtk_bm_send_to_user(bm, pid, seq, fgd_ret_msg);
		} else
			pr_err("[%s]gm2 netlink_handler is NULL\n", __func__);
	} else {
		pr_err("[%s]gm instance id is not supported:%d\n", __func__, fgd_msg->instance_id);
	}
#endif

}

static int mtk_bm_create_netlink(struct platform_device *pdev)
{
	struct mtk_battery_manager *bm = platform_get_drvdata(pdev);
	struct netlink_kernel_cfg cfg = {
		.input = mtk_bm_netlink_handler,
	};

	bm->mtk_bm_sk =
		netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);

	if (bm->mtk_bm_sk == NULL) {
		pr_err("netlink_kernel_create error\n");
		return -EIO;
	}

	pr_err("[%s]netlink_kernel_create protol= %d\n",
		__func__, NETLINK_FGD);

	return 0;
}

void bm_custom_init_from_header(struct mtk_battery_manager *bm)
{
	bm->vsys_det_voltage1  = VSYS_DET_VOLTAGE1;
	bm->vsys_det_voltage2  = VSYS_DET_VOLTAGE2;
}

void bm_custom_init_from_dts(struct platform_device *pdev, struct mtk_battery_manager *bm)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	ret = device_property_read_u32(dev, "disable-quick-shutdown", &bm->disable_quick_shutdown);
	if (ret)
		pr_debug("%s: disable-quick-shutdown get fail\n", __func__);

	ret = device_property_read_u32(dev, "vsys-det-voltage1", &bm->vsys_det_voltage1);
	if (ret)
		pr_debug("%s: vsys-det-voltage1 get fail\n", __func__);

	ret = device_property_read_u32(dev, "vsys-det-voltage2", &bm->vsys_det_voltage2);
	if (ret)
		pr_debug("%s: vsys-det-voltage2 get fail\n", __func__);

	pr_debug("%s: %d %d %d n", __func__,
		bm->disable_quick_shutdown, bm->vsys_det_voltage1, bm->vsys_det_voltage2);
}

static int mtk_bm_probe(struct platform_device *pdev)
{
	struct mtk_battery_manager *bm;
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	int ret = 0;

	pr_err("[%s] 20231205-1\n", __func__);
	bm = devm_kzalloc(&pdev->dev, sizeof(*bm), GFP_KERNEL);
	if (!bm)
		return -ENOMEM;

	platform_set_drvdata(pdev, bm);
	bm->dev = &pdev->dev;

	bm_custom_init_from_header(bm);
	bm_custom_init_from_dts(pdev, bm);

	psy = devm_power_supply_get_by_phandle(&pdev->dev,
								 "gauge1");

	if (psy == NULL || IS_ERR(psy)) {
		pr_err("[%s]can not get gauge1 psy\n", __func__);
	} else {
		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge != NULL) {
			bm->gm1 = gauge->gm;
			bm->gm1->id = 0;
			bm->gm1->bm = bm;
			bm->gm1->netlink_send = mtk_bm_handler;
		} else
			pr_err("[%s]gauge1 is not rdy\n", __func__);
	}
	psy = devm_power_supply_get_by_phandle(&pdev->dev,
								 "gauge2");

	if (psy == NULL || IS_ERR(psy)) {
		pr_err("[%s]can not get gauge2 psy\n", __func__);
	} else {
		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge != NULL) {
			bm->gm2 = gauge->gm;
			bm->gm2->id = 1;
			bm->gm2->bm = bm;
			bm->gm2->netlink_send = mtk_bm_handler;
		} else
			pr_err("[%s]gauge2 is not rdy\n", __func__);
	}

	if (bm->gm1 != NULL) {
		if (bm->gm2 != NULL) {
			pr_err("[%s]dual gauges is enabled\n", __func__);
			bm->gm_no = 2;
		} else {
			pr_err("[%s]single gauge is enabled\n", __func__);
			bm->gm_no = 1;
		}
	} else
		pr_err("[%s]gauge configuration is incorrect\n", __func__);

	if (bm->gm1 == NULL && bm->gm2 == NULL) {
		pr_err("[%s]disable gauge because can not find gm!\n", __func__);
		return 0;
	}


	bm->chan_vsys = devm_iio_channel_get(
		&pdev->dev, "vsys");
	if (IS_ERR(bm->chan_vsys)) {
		ret = PTR_ERR(bm->chan_vsys);
		pr_err("vsys auxadc get fail, ret=%d\n", ret);
	}

	bm_check_bootmode(&pdev->dev, bm);

	init_waitqueue_head(&bm->wait_que);


	bm->bm_wakelock = wakeup_source_register(NULL, "battery_manager_wakelock");
	spin_lock_init(&bm->slock);

#ifdef CONFIG_PM
	bm->pm_notifier.notifier_call = bm_pm_event;
	ret = register_pm_notifier(&bm->pm_notifier);
	if (ret) {
		pr_err("%s failed to register system pm notify\n", __func__);
		unregister_pm_notifier(&bm->pm_notifier);
	}
#endif /* CONFIG_PM */

#ifdef BM_USE_HRTIMER
	battery_manager_thread_hrtimer_init(bm);
#endif
#ifdef BM_USE_ALARM_TIMER
	battery_manager_thread_alarm_init(bm);
#endif

	kthread_run(battery_manager_routine_thread, bm, "battery_manager_thread");

	bm->bs_data.chg_psy = devm_power_supply_get_by_phandle(&pdev->dev, "charger");
	if (IS_ERR_OR_NULL(bm->bs_data.chg_psy))
		pr_err("[%s]Fail to get chg_psy!\n", __func__);

	bm_battery_service_init(bm);
	mtk_bm_create_netlink(pdev);

	mtk_power_misc_init(bm, &bm->sdc);

	bm->sdc.sdc_wakelock = wakeup_source_register(NULL, "battery_manager_sdc_wakelock");
	spin_lock_init(&bm->sdc.slock);

	return 0;
}

static int mtk_bm_remove(struct platform_device *pdev)
{
	return 0;
}

static void mtk_bm_shutdown(struct platform_device *pdev)
{
}

static int __maybe_unused mtk_bm_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mtk_bm_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_bm_pm_ops, mtk_bm_suspend, mtk_bm_resume);

static const struct of_device_id __maybe_unused mtk_bm_of_match[] = {
	{ .compatible = "mediatek,battery manager", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_bm_of_match);

static struct platform_driver mtk_battery_manager_driver = {
	.probe = mtk_bm_probe,
	.remove = mtk_bm_remove,
	.shutdown = mtk_bm_shutdown,
	.driver = {
		.name = "mtk_battery_manager",
		.pm = &mtk_bm_pm_ops,
		.of_match_table = mtk_bm_of_match,
	},
};
module_platform_driver(mtk_battery_manager_driver);

MODULE_AUTHOR("Wy Chuang<Wy.Chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Battery Manager");
MODULE_LICENSE("GPL");
