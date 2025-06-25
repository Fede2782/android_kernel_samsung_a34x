// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
/* necessary header */
#include "mtk_pe4.h"
#include "charger_class.h"

/* dependent on platform */
#include "mtk_charger.h"

#define MAX_WAIT_TA_TIMES 2

enum adapter_driver_type {
	MTK_RUN_ON_PD,
	MTK_RUN_ON_UFCS,
};

struct pe40_hal {
	struct charger_device *chg1_dev;
	struct charger_device *chg2_dev;
	struct adapter_device *ufcs_adapter;
	struct adapter_device *pd_adapter;
	struct adapter_device *adapter;
	int adapter_type;
	int wait_times;
};

int pe4_hal_init_hardware(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
	struct pe40_hal *hal;


	pe4_dbg("%s\n", __func__);
	if (alg == NULL) {
		pe4_err("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	pe4 = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (hal == NULL) {
		hal = devm_kzalloc(&pe4->pdev->dev, sizeof(*hal), GFP_KERNEL);
		if (!hal)
			return -ENOMEM;
		chg_alg_dev_set_drv_hal_data(alg, hal);
	}

	hal->chg1_dev = get_charger_by_name("primary_chg");
	if (hal->chg1_dev)
		pe4_dbg("%s: Found primary charger\n", __func__);
	else {
		pe4_err("%s: Error : can't find primary charger\n",
			__func__);
		return -ENODEV;
	}

	hal->chg2_dev = get_charger_by_name("secondary_chg");
	if (hal->chg2_dev)
		pe4_dbg("%s: Found secondary charger\n", __func__);
	else
		pe4_err("%s: Error : can't find secondary charger\n",
			__func__);

	hal->pd_adapter = get_adapter_by_name("pd_adapter");
	if (hal->pd_adapter)
		pe4_dbg("%s: Found pd adapter\n", __func__);
	else
		pe4_err("%s: note : can't find pd adapter\n",
			__func__);


	hal->ufcs_adapter = get_adapter_by_name("ufcs_adapter");
	if (hal->ufcs_adapter)
		pe4_dbg("%s: Found ufcs adapter\n", __func__);
	else
		pe4_err("%s: note : can't find ufcs adapter\n",
			__func__);

	if (!hal->pd_adapter && !hal->ufcs_adapter) {
		pe4_err("%s: Error : can't find pd, ufcs adapter\n",
			__func__);
		return -ENODEV;
	}

	return 0;
}

int pe4_hal_set_adapter_cap_end(struct chg_alg_device *alg,
	int mV, int mA, int exit_mode)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (exit_mode) {
		ret = adapter_dev_exit_mode(hal->adapter);
		pe4_dbg("%s: exit_mode\n", __func__);
	}
	if (ret < 0 || !exit_mode) {
		ret = adapter_dev_set_cap(hal->adapter, MTK_PD_APDO_END, mV, mA);
		pe4_dbg("%s %d %d\n", __func__, mV, mA);
	}
	pe4_dbg("%s: ret = %d\n", __func__, ret);
	return 0;
}

int pe4_hal_set_mivr(struct chg_alg_device *alg, enum chg_idx chgidx, int uV)
{
	int ret = 0;
	bool chg2_chip_enabled = false;
	struct mtk_pe40 *pe4;
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	pe4 = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);

	ret = charger_dev_set_mivr(hal->chg1_dev, uV);
	if (ret < 0)
		pe4_err("%s: failed, ret = %d\n", __func__, ret);

	if (hal->chg2_dev) {
		charger_dev_is_chip_enabled(hal->chg2_dev,
			&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			ret = charger_dev_set_mivr(hal->chg2_dev,
				uV + pe4->slave_mivr_diff);
			if (ret < 0)
				pr_info("%s: chg2 failed, ret = %d\n", __func__,
					ret);
		}
	}

	return ret;
}

int pe4_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable)
{
	//wy fix me
	mtk_chg_enable_vbus_ovp(enable);

	return 0;
}

int pe4_hal_enable_termination(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL) {
		ret = charger_dev_enable_termination(hal->chg1_dev, enable);
		if (ret < 0)
			return ret;
	}
	if (chgidx == CHG2 && hal->chg2_dev != NULL) {
		ret = charger_dev_enable_termination(hal->chg2_dev, enable);
		if (ret < 0)
			return ret;
	}
	pe4_dbg("%s idx:%d %d\n", __func__, chgidx, enable);
	return 0;
}


int pe4_hal_get_uisoc(struct chg_alg_device *alg)
{
	union power_supply_propval prop = {0};
	struct power_supply *bat_manager_psy = NULL;
	int ret = 0;
	struct mtk_pe40 *pe4;

	if (alg == NULL)
		return -EINVAL;

	pe4 = dev_get_drvdata(&alg->dev);
	bat_manager_psy = pe4->bat_manager_psy;

	if (IS_ERR_OR_NULL(bat_manager_psy)) {
		pr_notice("%s retry to get pe4->bat_manager_psy\n", __func__);
		bat_manager_psy = power_supply_get_by_name("battery");
		pe4->bat_manager_psy = bat_manager_psy;
	}

	if (IS_ERR_OR_NULL(bat_manager_psy)) {
		pr_notice("%s Couldn't get bat_psy\n", __func__);
		ret = 50;
	} else {
		ret = power_supply_get_property(bat_manager_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
		if (ret < 0) {
			pr_notice("%s Couldn't get bat_capacity\n", __func__);
			ret = 50;
			return ret;
		}
		ret = prop.intval;
	}

	pr_debug("%s:%d\n", __func__,
		ret);
	return ret;
}

int pe4_hal_is_adapter_ready(struct chg_alg_device *alg)
{
	struct mtk_charger *info = NULL;
	struct power_supply *chg_psy = NULL;
	struct pe40_hal *hal;

	if (alg == NULL) {
		pe4_err("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	hal = chg_alg_dev_get_drv_hal_data(alg);
	chg_psy = power_supply_get_by_name("mtk-master-charger");

	if (chg_psy == NULL || IS_ERR(chg_psy))
		pe4_err("%s Couldn't get chg_psy\n", __func__);
	else {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		if (info->select_adapter) {
			pe4_dbg("%s ta_cap:%d\n", __func__, info->ta_capability);
			hal->adapter = info->select_adapter;
			if (info->ta_capability == APDO_TA ||
				info->ta_capability == ONLY_APDO_TA)
				return ALG_READY;
			else
				return ALG_TA_NOT_SUPPORT;
		}
	}
	return ALG_TA_CHECKING;
}

int pe4_hal_get_battery_temperature(struct chg_alg_device *alg)
{
	union power_supply_propval prop = {0};
	struct power_supply *bat_manager_psy = NULL;
	int ret = 0;
	struct mtk_pe40 *pe4;

	if (alg == NULL)
		return -EINVAL;

	pe4 = dev_get_drvdata(&alg->dev);
	bat_manager_psy = pe4->bat_manager_psy;

	if (IS_ERR_OR_NULL(bat_manager_psy)) {
		pr_notice("%s retry to get pe4->bat_manager_psy\n", __func__);
		bat_manager_psy = power_supply_get_by_name("battery");
		pe4->bat_manager_psy = bat_manager_psy;
	}

	if (bat_manager_psy == NULL || IS_ERR(bat_manager_psy)) {
		chr_err("%s Couldn't get bat_manager_psy\n", __func__);
		ret = 27;
	} else {
		ret = power_supply_get_property(bat_manager_psy,
			POWER_SUPPLY_PROP_TEMP, &prop);
		if (ret != -EINVAL)
			ret = prop.intval / 10;
	}

	chr_debug("%s:%d\n", __func__,
		ret);
	return ret;
}


int pe4_hal_get_adapter_cap(struct chg_alg_device *alg,
	struct pe4_power_cap *cap)
{
	struct pe40_hal *hal;
	struct adapter_power_cap acap;
	int i, ret;

	if (alg == NULL) {
		pe4_err("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	hal = chg_alg_dev_get_drv_hal_data(alg);

	memset(&acap, 0, sizeof(struct adapter_power_cap));

	ret = adapter_dev_get_cap(hal->adapter, MTK_PD_APDO, &acap);
	cap->selected_cap_idx = acap.selected_cap_idx;
	cap->nr = acap.nr;
	cap->pdp = acap.pdp;
	for (i = 0; i < 10; i++) {
		cap->pwr_limit[i] = acap.pwr_limit[i];
		cap->min_mv[i] = acap.min_mv[i];
		cap->max_mv[i] = acap.max_mv[i];
		cap->ma[i] = acap.ma[i];
		cap->maxwatt[i] = acap.maxwatt[i];
		cap->minwatt[i] = acap.minwatt[i];
		cap->type[i] = acap.type[i];
		cap->info[i] = acap.info[i];
	}
	return ret;
}

int pe4_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1)
		charger_dev_set_input_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2)
		charger_dev_set_input_current(hal->chg2_dev, ua);

	return 0;
}

int pe4_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1)
		charger_dev_set_charging_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2)
		charger_dev_set_charging_current(hal->chg2_dev, ua);

	return 0;
}

int pe4_hal_1st_set_adapter_cap(struct chg_alg_device *alg,
	int mV, int mA)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	pe4_dbg("%s %d %d\n", __func__, mV, mA);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	adapter_dev_set_cap(hal->adapter, MTK_PD_APDO_START, mV, mA);
	return 0;
}

int pe4_hal_set_adapter_cap(struct chg_alg_device *alg,
	int mV, int mA)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	pe4_dbg("%s %d %d\n", __func__, mV, mA);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	return adapter_dev_set_cap(hal->adapter, MTK_PD_APDO, mV, mA);
}

int pe4_hal_get_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		charger_dev_get_input_current(hal->chg1_dev,
		ua);
	else if (chgidx == CHG2)
		charger_dev_get_input_current(hal->chg2_dev,
		ua);



	return 0;
}

int pe4_hal_enable_powerpath(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		charger_dev_enable_powerpath(hal->chg1_dev, enable);
	else if (chgidx == CHG2)
		charger_dev_enable_powerpath(hal->chg2_dev, enable);

	return 0;
}

int pe4_hal_force_disable_powerpath(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool disable)
{
	struct power_supply *chg_psy = NULL;
	union power_supply_propval prop = {0};
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	if (chgidx == CHG1)
		chg_psy = power_supply_get_by_name("mtk-master-charger");
	else if (chgidx == CHG2)
		chg_psy = power_supply_get_by_name("mtk-slave-charger");

	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pe4_err("%s Couldn't get chg_psy %d\n", __func__, chgidx);
		return -EINVAL;
	}

	prop.intval = (int)disable;
	ret = power_supply_set_property(chg_psy,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, &prop);
	pe4_dbg("%s disable_powerpath:%d\n", __func__, prop.intval);

	return ret;
}

int pe4_hal_get_charger_cnt(struct chg_alg_device *alg)
{
	struct pe40_hal *hal;
	int cnt = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (hal->chg1_dev != NULL)
		cnt++;
	if (hal->chg2_dev != NULL)
		cnt++;

	return cnt;
}

bool pe4_hal_is_chip_enable(struct chg_alg_device *alg, enum chg_idx chgidx)
{
	struct pe40_hal *hal;
	bool is_chip_enable = false;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		charger_dev_is_chip_enabled(hal->chg1_dev,
		&is_chip_enable);
	else if (chgidx == CHG2)
		charger_dev_is_chip_enabled(hal->chg2_dev,
		&is_chip_enable);

	return is_chip_enable;
}

int pe4_hal_enable_charger(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		ret = charger_dev_enable(hal->chg1_dev, en);
	else if (chgidx == CHG2)
		ret = charger_dev_enable(hal->chg2_dev, en);

	return ret;
}


/**
 *
 * RETURNS:
 * 0: success
 * 1: not support
 * other: error
 */
int pe40_hal_get_adapter_output(struct chg_alg_device *alg,
	struct pe4_pps_status *pe4_status)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = adapter_dev_get_output(hal->adapter,
		&pe4_status->output_mv,
		&pe4_status->output_ma);

	return ret;
}



int pe40_hal_get_adapter_status(struct chg_alg_device *alg,
	struct pe4_adapter_status *pe4_sta)
{
	struct adapter_status sta;
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	sta.temperature = 25;
	sta.ocp = false;
	sta.otp = false;
	sta.ovp = false;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = adapter_dev_get_status(hal->adapter, &sta);
	pe4_sta->temperature = sta.temperature;
	pe4_sta->ocp = sta.ocp;
	pe4_sta->otp = sta.otp;
	pe4_sta->ovp = sta.ovp;

	return ret;
}


static int get_pmic_vbus(int *vchr)
{
	union power_supply_propval prop = {0};
	struct power_supply *chg_psy = NULL;
	int ret = 0;

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk_charger_type");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pe4_err("%s Couldn't get chg_psy\n", __func__);
		ret = -1;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	}
	*vchr = prop.intval;

	pe4_dbg("%s vbus:%d\n", __func__,
		prop.intval);
	return ret;
}

int pe4_hal_get_vbus(struct chg_alg_device *alg)
{
	int ret = 0;
	int vchr = 0;
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);

	ret = charger_dev_get_vbus(hal->chg1_dev, &vchr);
	vchr = vchr / 1000;
	if (ret < 0) {
		ret = get_pmic_vbus(&vchr);
		if (ret < 0)
			pe4_err("%s: get vbus failed: %d\n", __func__, ret);
	}

	return vchr;
}

int pe4_hal_get_vbat(struct chg_alg_device *alg)
{
	union power_supply_propval prop = {0};
	struct power_supply *bat1_psy = NULL;
	int ret = 0;
	struct mtk_pe40 *pe4;

	if (alg == NULL)
		return -EINVAL;

	pe4 = dev_get_drvdata(&alg->dev);
	bat1_psy = pe4->bat1_psy;

	if (IS_ERR_OR_NULL(bat1_psy)) {
		pr_notice("%s retry to get pe4->bat1_psy\n", __func__);
		bat1_psy = devm_power_supply_get_by_phandle(&pe4->pdev->dev, "gauge");
		pe4->bat1_psy = bat1_psy;
	}

	if (IS_ERR_OR_NULL(bat1_psy)) {
		pr_notice("%s Couldn't get bat1_psy\n", __func__);
		ret = 3999;
	} else {
		ret = power_supply_get_property(bat1_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
		if (ret != -EINVAL)
			ret = prop.intval / 1000;
	}

	pr_debug("%s:%d\n", __func__,
		ret);
	return ret;
}

int pe4_hal_get_ibus(struct chg_alg_device *alg, int *ibus)
{
	int ret = 0;
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = charger_dev_get_ibus(hal->chg1_dev, ibus);
	if (ret < 0)
		pe4_err("%s: get vbus failed: %d\n", __func__, ret);

	return ret;
}

int pe4_hal_dump_registers(struct chg_alg_device *alg)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_dump_registers(hal->chg1_dev);
	charger_dev_dump_registers(hal->chg2_dev);

	return 0;
}

int pe4_hal_get_ibat(struct chg_alg_device *alg)
{
	union power_supply_propval prop = {0};
	struct power_supply *bat1_psy = NULL;
	int ret = 0;
	struct mtk_pe40 *pe4;


	if (alg == NULL)
		return -EINVAL;

	pe4 = dev_get_drvdata(&alg->dev);
	bat1_psy = pe4->bat1_psy;

	if (IS_ERR_OR_NULL(bat1_psy)) {
		pr_notice("%s retry to get pe4->bat1_psy\n", __func__);
		bat1_psy = devm_power_supply_get_by_phandle(&pe4->pdev->dev, "gauge");
		pe4->bat1_psy = bat1_psy;
	}

	if (IS_ERR_OR_NULL(bat1_psy)) {
		pr_notice("%s Couldn't get bat1_psy\n", __func__);
		ret = 0;
	} else {
		ret = power_supply_get_property(bat1_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
		if (ret != -EINVAL)
			ret = prop.intval;

	}

	pr_debug("%s:%d\n", __func__,
		ret);
	return ret / 1000;
}

int pe4_hal_get_charger_type(struct chg_alg_device *alg)
{
	struct mtk_charger *info = NULL;
	struct power_supply *chg_psy = NULL;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pe4_err("%s Couldn't get chg_psy\n", __func__);
		return 0;
	} else {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		ret = info->chr_type;
	}

	pe4_dbg("%s type:%d\n", __func__, ret);
	return info->chr_type;
}

int pe4_hal_reset_eoc_state(struct chg_alg_device *alg)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = charger_dev_reset_eoc_state(hal->chg1_dev);
	if (ret != 0) {
		pe4_err("%s: fail ,ret=%d\n", __func__, ret);
		return -1;
	}

	return 0;
}

int pe4_hal_reset_ta(struct chg_alg_device *alg, enum chg_idx chgidx)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = charger_dev_reset_ta(hal->chg1_dev);
	if (ret != 0) {
		pe4_err("%s: fail ,ret=%d\n", __func__, ret);
		return -1;
	}
	return 0;
}

int pe4_hal_enable_cable_drop_comp(struct chg_alg_device *alg,
	bool en)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	return charger_dev_enable_cable_drop_comp(hal->chg1_dev, false);
}

int pe4_hal_vbat_mon_en(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en)
{

	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	ret = charger_dev_enable_6pin_battery_charging(
		hal->chg1_dev, en);

	pe4_err("%s en=%d ret=%d\n", __func__, en, ret);

	return ret;
}

int pe4_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1)
		charger_dev_set_constant_voltage(hal->chg1_dev,
			uv);
	else if (chgidx == CHG2)
		charger_dev_set_constant_voltage(hal->chg2_dev,
			uv);

	return 0;
}

int pe4_hal_get_mivr_state(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *in_loop)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1)
		charger_dev_get_mivr_state(hal->chg1_dev, in_loop);
	else if (chgidx == CHG2)
		charger_dev_get_mivr_state(hal->chg2_dev, in_loop);

	return 0;
}

int pe4_hal_get_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int *mivr1)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1)
		charger_dev_get_mivr(hal->chg1_dev, mivr1);
	else if (chgidx == CHG2)
		charger_dev_get_mivr(hal->chg2_dev, mivr1);

	return 0;
}

int pe4_hal_charger_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1 && hal->chg1_dev != NULL) {
		ret = charger_dev_enable_chip(hal->chg1_dev, enable);
		if (ret < 0)
			return ret;
	} else if (chgidx == CHG2 && hal->chg2_dev != NULL) {
		ret = charger_dev_enable_chip(hal->chg2_dev, enable);
		if (ret < 0)
			return ret;
	}
	pe4_dbg("%s idx:%d %d %d\n", __func__, chgidx, enable,
		hal->chg2_dev != NULL);
	return 0;
}

int pe4_hal_is_charger_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *en)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1  && hal->chg1_dev != NULL) {
		ret = charger_dev_is_enabled(hal->chg1_dev, en);
		if (ret < 0)
			return ret;
	} else if (chgidx == CHG2  && hal->chg2_dev != NULL) {
		ret = charger_dev_is_enabled(hal->chg2_dev, en);
		if (ret < 0)
			return ret;
	}
	pe4_dbg("%s idx:%d %d\n", __func__, chgidx, *en);
	return 0;
}

int pe4_hal_get_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_get_charging_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_get_charging_current(hal->chg2_dev, ua);

	pe4_dbg("%s idx:%d %lu\n", __func__, chgidx, (unsigned long)*ua);

	return 0;
}

int pe4_hal_get_min_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL) {
		ret = charger_dev_get_min_charging_current(hal->chg1_dev, uA);
		if (ret < 0)
			return ret;
	}
	if (chgidx == CHG2 && hal->chg2_dev != NULL) {
		ret = charger_dev_get_min_charging_current(hal->chg2_dev, uA);
		if (ret < 0)
			return ret;
	}
	pe4_dbg("%s idx:%d %d\n", __func__, chgidx, *uA);
	return 0;
}

int pe4_hal_set_eoc_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uA)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1 && hal->chg1_dev != NULL) {
		ret = charger_dev_set_eoc_current(hal->chg1_dev, uA);
		if (ret < 0)
			return ret;
	}
	if (chgidx == CHG2 && hal->chg2_dev != NULL) {
		ret = charger_dev_set_eoc_current(hal->chg2_dev, uA);
		if (ret < 0)
			return ret;
	}
	pe4_dbg("%s idx:%d %d\n", __func__, chgidx, uA);
	return 0;
}

int pe4_hal_get_min_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA)
{
	struct pe40_hal *hal;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL) {
		ret = charger_dev_get_min_input_current(hal->chg1_dev, uA);
		if (ret < 0)
			return ret;
	}
	if (chgidx == CHG2 && hal->chg2_dev != NULL) {
		ret = charger_dev_get_min_input_current(hal->chg2_dev, uA);
		if (ret < 0)
			return ret;
	}
	pe4_dbg("%s idx:%d %d\n", __func__, chgidx, *uA);
	return 0;
}

int pe4_hal_safety_check(struct chg_alg_device *alg,
	int ieoc)
{
	struct pe40_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_safety_check(hal->chg1_dev,
				 ieoc);
	return 0;
}

int pe4_hal_get_log_level(struct chg_alg_device *alg)
{
	struct mtk_charger *info = NULL;
	struct power_supply *chg_psy = NULL;
	int ret = 0;

	if (alg == NULL)
		return -EINVAL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (IS_ERR_OR_NULL(chg_psy)) {
		pe4_err("%s Couldn't get chg_psy\n", __func__);
		return -1;
	} else {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		if (info == NULL) {
			pe4_err("%s info is NULL\n", __func__);
			return -1;
		}
		ret = info->log_level;
	}

	return ret;
}
