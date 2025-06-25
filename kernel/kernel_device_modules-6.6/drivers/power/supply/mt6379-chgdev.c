// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt6379-chgdev.c -- Mediatek MT6379 Charger class Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: SHIH CHIA CHANG <jeff_chang@richtek.com>
 */

#include "mt6379-charger.h"

#define PE_POLL_TIME_US		(100 * 1000)

static int mt6379_plug_in(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	int ret = 0;

	if (pdata->wdt_en) {
		ret = mt6379_charger_field_set(cdata, F_WDT_EN, 1);
		if (ret)
			dev_info(cdata->dev, "%s, Failed to enable WDT\n", __func__);
	}

	return ret;
}

static int mt6379_plug_out(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	int ret = 0;

	if (pdata->wdt_en) {
		ret = mt6379_charger_field_set(cdata, F_WDT_EN, 0);
		if (ret)
			dev_info(cdata->dev, "%s, Failed to disable WDT\n", __func__);
	}

	return ret;
}

static int mt6379_enable_charging(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->cv_lock);

	ret = mt6379_charger_field_set(cdata, F_CHG_EN, en);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to %s CHG_EN\n",
			 __func__, en ? "enable" : "disable");

	mutex_unlock(&cdata->cv_lock);

	return ret;
}

static int mt6379_is_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = 0;
	int ret = 0;

	ret = mt6379_charger_field_get(cdata, F_CHG_EN, &val);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get CHG_EN\n", __func__);
		return ret;
	}

	*en = val;
	return ret;
}

static int mt6379_set_ichg(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };
	int ret = 0;

	ret = power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					&val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to set ICHG to %umA\n", __func__, U_TO_M(uA));

	return ret;
}

static int mt6379_get_ichg(struct charger_device *chgdev, u32 *uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					&val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get ICHG\n", __func__);

	*uA = ret == 0 ? val.intval : 0;

	return ret;
}

static int mt6379_get_min_ichg(struct charger_device *chgdev, u32 *uA)
{
	*uA = 300000;
	return 0;
}

#define RECHG_THRESHOLD		100000 /* uV */
static int mt6379_charger_set_cv(struct mt6379_charger_data *cdata, u32 uV)
{
	union power_supply_propval val;
	bool done = false;
	u32 enabled = 0;
	int ret = 0;

	val.intval = POWER_SUPPLY_STATUS_UNKNOWN;
	mutex_lock(&cdata->cv_lock);

	if (cdata->batprotect_en) {
		dev_info(cdata->dev, "%s, batprotect enabled, should not set CV\n", __func__);
		goto out;
	}

	if (uV <= cdata->cv || uV >= cdata->cv + RECHG_THRESHOLD)
		goto out_cv;

	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret || val.intval != POWER_SUPPLY_STATUS_FULL)
		goto out_cv;

	done = (val.intval == POWER_SUPPLY_STATUS_FULL);

	ret = mt6379_charger_field_get(cdata, F_CHG_EN, &enabled);
	if (ret || !enabled)
		goto out_cv;

	ret = mt6379_charger_field_set(cdata, F_CHG_EN, false);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to disable CHG_EN\n", __func__);
out_cv:
	val.intval = uV;
	ret = power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to set CV\n", __func__);
	else
		cdata->cv = uV;

	mt_dbg(cdata->dev, "%s, CV = %d uV\n", __func__, cdata->cv);

	if (done && enabled) {
		ret = mt6379_charger_field_set(cdata, F_CHG_EN, true);
		if (ret)
			dev_info(cdata->dev, "%s, Failed to enable CHG_EN\n", __func__);
	}

out:
	mutex_unlock(&cdata->cv_lock);
	return ret;
}

static int mt6379_set_cv(struct charger_device *chgdev, u32 uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_set_cv(cdata, uV);
}

static int mt6379_get_cv(struct charger_device *chgdev, u32 *uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	mutex_lock(&cdata->cv_lock);
	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get cv\n", __func__);

	*uV = ret == 0 ? val.intval : 0;
	mutex_unlock(&cdata->cv_lock);

	return ret;
}

static int mt6379_set_aicr(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to set AICR to %umA\n", __func__, U_TO_M(uA));

	mutex_unlock(&cdata->pe_lock);

	return ret;
}

static int mt6379_get_aicr(struct charger_device *chgdev, u32 *uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get AICR\n", __func__);

	*uA = ret == 0 ? val.intval : 0;
	mutex_unlock(&cdata->pe_lock);

	return ret;
}

static int mt6379_get_min_aicr(struct charger_device *chgdev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int mt6379_set_mivr(struct charger_device *chgdev, u32 uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uV };
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to set MIVR to %umV\n", __func__, U_TO_M(uV));

	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_get_mivr(struct charger_device *chgdev, u32 *uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get MIVR\n", __func__);

	*uV = ret == 0 ? val.intval : 0;

	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_get_mivr_state(struct charger_device *chgdev, bool *active)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = 0;
	int ret = 0;

	ret = mt6379_charger_field_get(cdata, F_ST_MIVR, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get MIVR state\n", __func__);

	*active = val;

	return ret;
}

static int mt6379_get_adc(struct charger_device *chgdev, enum adc_channel chan, int *min, int *max)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	enum mt6379_adc_chan adc_chan;
	int ret = 0;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		adc_chan = ADC_CHAN_CHGVIN;
		break;
	case ADC_CHANNEL_VSYS:
		adc_chan = ADC_CHAN_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		adc_chan = ADC_CHAN_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		adc_chan = ADC_CHAN_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		adc_chan = ADC_CHAN_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		adc_chan = ADC_CHAN_TEMPJC;
		break;
	case ADC_CHANNEL_USBID:
		adc_chan = ADC_CHAN_SBU2;
		break;
	default:
		return -EINVAL;
	}

	ret = iio_read_channel_processed(&cdata->iio_adcs[adc_chan], min);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to read chg_adc \"%s(%d)\" channel\n",
			 __func__, mt6379_adc_chan_names[adc_chan], adc_chan);
	else
		*max = *min;

	return 0;
}

static int mt6379_get_vbus(struct charger_device *chgdev, u32 *vbus)
{
	return mt6379_get_adc(chgdev, ADC_CHANNEL_VBUS, vbus, vbus);
}

static int mt6379_get_ibus(struct charger_device *chgdev, u32 *ibus)
{
	return mt6379_get_adc(chgdev, ADC_CHANNEL_IBUS, ibus, ibus);
}

static int mt6379_get_ibat(struct charger_device *chgdev, u32 *ibat)
{
	return mt6379_get_adc(chgdev, ADC_CHANNEL_IBAT, ibat, ibat);
}

#define ABNORMAL_TEMP_JC	120
static int mt6379_get_tchg(struct charger_device *chgdev, int *min, int *max)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int temp_jc = 0, ret = 0, retry_cnt = 3;

	/* temp abnormal workaround */
	do {
		ret = mt6379_get_adc(chgdev, ADC_CHANNEL_TEMP_JC, &temp_jc, &temp_jc);
	} while ((ret < 0 || temp_jc >= ABNORMAL_TEMP_JC) && (--retry_cnt) > 0);

	if (ret < 0) {
		dev_info(cdata->dev, "%s, Failed to get temp jc\n", __func__);
		return ret;
	}

	/* use last result if temp_jc is abnormal */
	if (temp_jc >= ABNORMAL_TEMP_JC)
		temp_jc = atomic_read(&cdata->tchg);
	else
		atomic_set(&cdata->tchg, temp_jc);

	*min = *max = temp_jc;
	mt_dbg(cdata->dev, "%s, tchg = %d\n", __func__, temp_jc);

	return 0;
}

static int mt6379_get_zcv(struct charger_device *chgdev, u32 *uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	*uV = M_TO_U(cdata->zcv);
	return 0;
}

static int mt6379_set_ieoc(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };
	int ret = 0;

	ret = power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to set IEOC to %umA\n", __func__, U_TO_M(uA));

	return ret;
}

static int mt6379_enable_te(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_TE, en);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to %s TE\n", __func__, en ? "enable" : "disable");

	return ret;
}

static int mt6379_reset_eoc_state(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_EOC_RST, 1);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to reset eoc state\n", __func__);

	return ret;
}

static int mt6379_sw_check_eoc(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0, ibat = 0;

	ret = mt6379_get_ibat(chgdev, &ibat);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get ibat\n", __func__);
		return ret;
	}

	if (ibat <= uA) {
		/* if  it happens 3 times, trigger EOC event */
		if (atomic_read(&cdata->eoc_cnt) == 2) {
			atomic_set(&cdata->eoc_cnt, 0);
			mt_dbg(cdata->dev, "%s, ieoc = %d, ibat = %d\n", __func__, uA, ibat);
			charger_dev_notify(cdata->chgdev, CHARGER_DEV_NOTIFY_EOC);
		} else
			atomic_inc(&cdata->eoc_cnt);
	} else
		atomic_set(&cdata->eoc_cnt, 0);

	return 0;
}

static int mt6379_is_charge_done(struct charger_device *chgdev, bool *done)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	val.intval = POWER_SUPPLY_STATUS_UNKNOWN;
	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret) {
		*done = false;
		dev_info(cdata->dev, "%s, Failed to get charger status\n", __func__);
	} else
		*done = (val.intval == POWER_SUPPLY_STATUS_FULL);

	return ret;
}

static int mt6379_enable_buck(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	struct device *dev = cdata->dev;
	int ret = 0;
	u32 buck_en = 0;

	dev_info(cdata->dev, "%s, en = %d\n", __func__, en);
	ret = mt6379_charger_field_get(cdata, F_BUCK_EN, &buck_en);
	if (ret) {
		dev_info(dev, "%s, Failed to get BUCK_EN\n", __func__);
		return ret;
	}

	if (en == buck_en)
		return 0;

	ret = mt6379_charger_field_set(cdata, F_BUCK_EN, en);
	if (ret)
		dev_info(dev, "%s, Failed to %s BUCK_EN\n",
			 __func__, en ? "enable" : "disable");

	if (en) {
		schedule_delayed_work(&cdata->switching_work, msecs_to_jiffies(1 * 1000));
	} else {
		cancel_delayed_work(&cdata->switching_work);
		ret = mt6379_charger_set_non_switching_setting(cdata);
		if (ret)
			dev_info(dev, "%s, set non switching ramp failed\n", __func__);
	}

	return ret;
}

static int mt6379_is_buck_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = 0;
	int ret = 0;

	ret = mt6379_charger_field_get(cdata, F_BUCK_EN, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get BUCK_EN status\n", __func__);

	*en = val;
	return ret;
}

static int mt6379_enable_charger_timer(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_CHG_TMR_EN, en);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to %s CHG_TMR_EN\n",
			 __func__, en ? "enable" : "disable");

	return ret;
}

static int mt6379_is_charger_timer_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = 0;
	int ret = 0;

	ret = mt6379_charger_field_get(cdata, F_CHG_TMR_EN, &val);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get CHG_TMT_EN\n", __func__);

	*en = val;

	return ret;
}

static int mt6379_kick_wdt(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_WDT_RST, 1);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to kick wdt\n", __func__);

	return ret;
}

static int mt6379_charger_check_aicc_running(struct mt6379_charger_data *cdata, bool *active)
{
	u32 val = 0;
	int ret = 0;

	*active = false;

	ret = mt6379_charger_field_get(cdata, F_ST_PWR_RDY, &val);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get power ready stat\n", __func__);
		return ret;
	}

	if (!val) /* power not rdy -> detach */
		return -EINVAL;

	ret = mt6379_charger_field_get(cdata, F_ST_AICC_DONE, &val);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get AICC_DONE stat\n", __func__);
		return ret;
	}

	*active = val;

	return 0;
}

static int mt6379_run_aicc(struct charger_device *chgdev, u32 *uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	ktime_t timeout = ktime_add(ktime_get(), ms_to_ktime(7000));
	bool active = false;
	int ret = 0;

	ret = mt6379_get_mivr_state(chgdev, &active);
	if (ret)
		return ret;

	if (!active) {
		mt_dbg(cdata->dev, "%s, MIVR loop (AICC) is not active\n", __func__);
		return 0;
	}

	mutex_lock(&cdata->pe_lock);
	ret = mt6379_charger_field_set(cdata, F_AICC_EN, 1);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to enable AICC_EN\n", __func__);
		goto out;
	}

	active = false;
	while (!ktime_after(ktime_get(), timeout)) {
		ret = mt6379_charger_check_aicc_running(cdata, &active);
		if (ret) {
			dev_info(cdata->dev, "%s, Failed to check AICC running state\n", __func__);
			goto out;
		}

		if (active)
			break;

		msleep(128);
	}

	/* Timeout */
	if (!active) {
		dev_info(cdata->dev, "%s, Checking AICC state timed-out\n", __func__);
		goto out;
	}

	ret = mt6379_charger_field_get(cdata, F_AICC_RPT, uA);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get AICC report\n", __func__);
		goto out;
	}

	dev_info(cdata->dev, "%s, AICC Report = %d mA\n", __func__, U_TO_M(*uA));
out:
	ret = mt6379_charger_field_set(cdata, F_AICC_EN, 0);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to disable AICC_EN\n", __func__);

	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_run_pe(struct mt6379_charger_data *cdata, bool pe20)
{
	unsigned long timeout = pe20 ? 1400 : 2800; /* ms */
	unsigned int regval = 0;
	int ret = 0;

	/* Set AICR to 800 mA */
	ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 800000);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set AICR to 800 mA\n", __func__);
		return ret;
	}

	/* Set CC to 2000 mA */
	ret = mt6379_charger_field_set(cdata, F_CC, 2000000);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set CC to 2000 mA\n", __func__);
		return ret;
	}

	ret = mt6379_charger_field_set(cdata, F_CHG_EN, 1);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to enable CHG_EN\n", __func__);
		return ret;
	}

	ret = mt6379_charger_field_set(cdata, F_PE_SEL, pe20);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to select %s\n", __func__, pe20 ? "PE10" : "PE20");
		return ret;
	}

	ret = mt6379_charger_field_set(cdata, F_PE_EN, 1);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to enable PE10/PE20\n", __func__);
		return ret;
	}

	return regmap_read_poll_timeout(cdata->rmap, MT6379_REG_CHG_PUMPX, regval,
					!(regval & BIT(7)), PE_POLL_TIME_US, timeout * 1000);
}

static int mt6379_set_pe_current_pattern(struct charger_device *chgdev, bool inc)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->pe_lock);

	dev_info(cdata->dev, "%s, inc = %d\n", __func__, inc);
	ret = mt6379_charger_field_set(cdata, F_PE10_INC, inc);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set PE10 up/down\n", __func__);
		goto out;
	}

	ret = mt6379_run_pe(cdata, false);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to run PE10\n", __func__);
out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_set_pe20_efficiency_table(struct charger_device *chgdev)
{
	return 0;
}

static int mt6379_set_pe20_current_pattern(struct charger_device *chgdev, u32 uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->pe_lock);

	dev_info(cdata->dev, "%s, set pe20 level = %d mV\n", __func__, U_TO_M(uV));
	ret = mt6379_charger_field_set(cdata, F_PE20_CODE, uV);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set PE20 code\n", __func__);
		goto out;
	}

	ret = mt6379_run_pe(cdata, true);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to run PE20\n", __func__);
out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_reset_pe_ta(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = mt6379_charger_field_set(cdata, F_VBUS_MIVR, 4600);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set MIVR to 4600 mV\n", __func__);
		goto out;
	}

	ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 100);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set AICR to 100 mA\n", __func__);
		goto out;
	}

	msleep(250);

	ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 500);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to AICR to 500 mA\n", __func__);
out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_enable_pe_cable_drop_comp(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	if (en)
		return 0;

	mutex_lock(&cdata->pe_lock);

	dev_info(cdata->dev, "%s, en = %d\n", __func__, en);
	ret = mt6379_charger_field_set(cdata, F_PE20_CODE, 0x1F);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set cable drop comp code\n", __func__);
		goto out;
	}

	ret = mt6379_run_pe(cdata, true);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to run PE20\n", __func__);

out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_enable_discharge(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	const int dischg_retry_cnt = 3;
	int i = 0, hm_ret = 0, ret = 0;
	u32 val = 0;

	dev_info(cdata->dev, "%s, en = %d\n", __func__, en);
	hm_ret = mt6379_enable_tm(cdata, true);
	if (hm_ret) {
		dev_info(cdata->dev, "%s, Failed to enable hm\n", __func__);
		return hm_ret;
	}

	ret = mt6379_charger_field_set(cdata, F_FORCE_VBUS_SINK, en);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to %s discharging\n",
			 __func__, en ? "enable" : "disable");
		goto out;
	}

	/* for disable, make sure it is disabled */
	if (!en) {
		for (i = 0; i < dischg_retry_cnt; i++) {
			ret = mt6379_charger_field_get(cdata, F_FORCE_VBUS_SINK, &val);
			if (ret) {
				dev_info(cdata->dev, "%s, Failed to get FORCE_VBUS_SINK\n",
					 __func__);
				continue;
			}

			if (!val)
				break;

			ret = mt6379_charger_field_set(cdata, F_FORCE_VBUS_SINK, 0);
			if (ret)
				dev_info(cdata->dev, "%s, Failed to disable discharging\n",
					 __func__);
		}
	}
out:
	hm_ret = mt6379_enable_tm(cdata, false);
	if (hm_ret) {
		dev_info(cdata->dev, "%s, Failed to disable hm\n", __func__);
		return hm_ret;
	}

	if (!en && i == dischg_retry_cnt) {
		dev_info(cdata->dev, "%s, Failed to disable discharging\n", __func__);
		return -EINVAL;
	}

	return ret;
}

static int mt6379_enable_chg_type_det(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;

	dev_info(cdata->dev, "%s, en = %d\n", __func__, en);
	val.intval = en ? ATTACH_TYPE_TYPEC : ATTACH_TYPE_NONE;
	return power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_ONLINE, &val);
}

#define DUMP_REG_BUF_SIZE	1024
enum {
	ADC_DUMP_VBUS = 0, /* CHGVIN */
	ADC_DUMP_IBUS,
	ADC_DUMP_VBAT,
	ADC_DUMP_IBAT,
	ADC_DUMP_VSYS,
	ADC_DUMP_MAX,
};

static int mt6379_dump_registers(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int i = 0, ret = 0, offset = 0;
	char buf[DUMP_REG_BUF_SIZE] = "\0";
	struct device *dev = cdata->dev;
	u32 val = 0;
	static const struct {
		const char *name;
		const char *unit;
		enum mt6379_charger_reg_field fd;
	} settings[] = {
		{ .fd = F_CC, .name = "CC", .unit = "mA" },
		{ .fd = F_IBUS_AICR, .name = "IBUS-AICR", .unit = "mA" },
		{ .fd = F_VBUS_MIVR, .name = "VBUS-MIVR", .unit = "mV" },
		{ .fd = F_IEOC, .name = "IEOC", .unit = "mA" },
		{ .fd = F_CV, .name = "CV", .unit = "mV" },
		{ .fd = F_WLIN_AICR, .name = "WLIN-AICR", .unit = "mA" },
		{ .fd = F_WLIN_MIVR, .name = "WLIN-MIVR", .unit = "mV" },
	};
	static struct {
		const char *name;
		const char *unit;
		enum mt6379_adc_chan chan;
		u32 value;
	} adcs[ADC_DUMP_MAX] = {
		{ .chan = ADC_CHAN_CHGVIN, .name = "VBUS", .unit = "mV" },
		{ .chan = ADC_CHAN_IBUS, .name = "IBUS", .unit = "mA" },
		{ .chan = ADC_CHAN_VBAT, .name = "VBAT", .unit = "mV" },
		{ .chan = ADC_CHAN_IBAT, .name = "IBAT", .unit = "mA" },
		{ .chan = ADC_CHAN_VSYS, .name = "VSYS", .unit = "mV" },
	};
	static const struct {
		const u16 reg;
		const char *name;
	} regs[] = {
		{ .reg = MT6379_REG_CHG_STAT, .name = "CHG_STAT" },
		{ .reg = MT6379_REG_CHG_STAT0, .name = "CHG_STAT0" },
		{ .reg = MT6379_REG_CHG_STAT1, .name = "CHG_STAT1" },
		{ .reg = MT6379_REG_CHG_TOP1, .name = "CHG_TOP1" },
		{ .reg = MT6379_REG_CHG_TOP2, .name = "CHG_TOP2" },
		{ .reg = MT6379_REG_CHG_EOC2, .name = "CHG_EOC2" },
		{ .reg = MT6379_REG_CHG_WDT, .name = "CHG_WDT" },
		{ .reg = MT6379_REG_CHG_HD_BUBO5, .name = "HD_BUBO5" },
		{ .reg = MT6379_REG_CHG_HD_TRIM6, .name = "HD_TRIM6" },
	};

	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = mt6379_charger_field_get(cdata, settings[i].fd, &val);
		if (ret) {
			dev_info(dev, "%s, Failed to get %s\n", __func__, settings[i].name);
			return ret;
		}

		val = U_TO_M(val);
		if (i % 4 == 0)
			offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "%s%s, ",
					    i != 0 ? "\n" : "", __func__);

		offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "%s = %d%s, ",
				    settings[i].name, val, settings[i].unit);
	}

	offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "\n");

	for (i = 0; i < ARRAY_SIZE(adcs); i++) {
		ret = iio_read_channel_processed(&cdata->iio_adcs[adcs[i].chan], &adcs[i].value);
		if (ret) {
			dev_info(dev, "%s, Failed to read adc %s\n", __func__, adcs[i].name);
			return ret;
		}

		adcs[i].value = U_TO_M(adcs[i].value);
		if (i % 4 == 0)
			offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "%s%s, ",
					    i != 0 ? "\n" : "", __func__);

		offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "%s = %d%s, ",
				    adcs[i].name, adcs[i].value, adcs[i].unit);
	}

	if (cdata->batprotect_en) {
		ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_VBATMON], &val);
		if (ret) {
			dev_info(dev, "%s, Failed to read VBATMON(ret:%d)\n", __func__, ret);
			return ret;
		}

		offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset,
				    "VBATCELL(VBAT_MON) = %dmV\n", U_TO_M(val));
	}

	offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "\n");

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_read(cdata->rmap, regs[i].reg, &val);
		if (ret) {
			dev_info(dev, "%s, Failed to read %s\n", __func__, regs[i].name);
			return ret;
		}

		if (i % 4 == 0)
			offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset, "%s%s, ",
					    i != 0 ? "\n" : "", __func__);

		offset += scnprintf(buf + offset, DUMP_REG_BUF_SIZE - offset,
				    "%s = 0x%02x, ", regs[i].name, val);
	}

	ret = mt6379_charger_fsw_control(cdata);
	if (ret)
		dev_info(dev, "%s, fsw control failed\n", __func__);

	dev_info(dev, "%s", buf);
	return 0;
}

static int mt6379_enable_hz(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_HZ, en ? 1 : 0);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to %s HZ\n", __func__, en ? "enable" : "disable");

	return ret;
}

static int mt6379_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		mt_dbg(cdata->dev, "%s, power_supply_changed, event:%d\n", __func__, event);
		power_supply_changed(cdata->psy);
		return 0;
	default:
		return 0;
	}
}

static int mt6379_enable_6pin_battery_charging(struct mt6379_charger_data *cdata,
					       enum mt6379_batpro_src src, bool en)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	u32 vbat = 0, cv = 0, vbat_mon_en_field = 0, adc_chan = 0, stat = 0;
	u16 batend_code = 0;
	int ret = 0;

	mutex_lock(&cdata->cv_lock);

	vbat_mon_en_field = src == MT6379_BATPRO_SRC_VBAT_MON2 ? F_VBAT_MON2_EN : F_VBAT_MON_EN;
	adc_chan = src == MT6379_BATPRO_SRC_VBAT_MON2 ? ADC_CHAN_VBATMON2 : ADC_CHAN_VBATMON;

	mt_dbg(cdata->dev, "%s, cdata->batprotect_en = %d, en = %d\n",
	       __func__, cdata->batprotect_en, en);

	if (cdata->batprotect_en == en)
		goto out;

	if (!en)
		goto dis_pro;

	/* If no 6pin used, always bypass this function */
	if (atomic_read(&cdata->no_6pin_used) == 1)
		goto dis_pro;

	/* Select the source of batprotect */
	ret =  mt6379_charger_field_set(cdata, F_BATPROTECT_SOURCE, src);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to select the source of batprotect(ret:%d)!\n",
			 __func__, ret);
		goto dis_pro;
	}

	/* Enable vbat mon */
	ret = mt6379_charger_field_set(cdata, vbat_mon_en_field, 1);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to enable vbat_mon%s(ret:%d)!\n",
			 __func__, src == MT6379_BATPRO_SRC_VBAT_MON2 ? "2" : "", ret);
		goto dis_mon;
	}

	/* Read vbat mon adc by chg_adc */
	ret = iio_read_channel_processed(&cdata->iio_adcs[adc_chan], &vbat);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to read vbat_mon%s(ret:%d)\n",
			 __func__, src == MT6379_BATPRO_SRC_VBAT_MON2 ? "2" : "", ret);
		goto dis_mon;
	}

	ret = mt6379_charger_field_get(cdata, F_CV, &cv);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get cv\n", __func__);
		goto dis_mon;
	}

	mt_dbg(cdata->dev, "%s, vbat = %dmV, cv = %dmV\n", __func__, vbat / 1000, cv / 1000);

	if (vbat >= cv) {
		mt_dbg(cdata->dev, "%s, vbat(%d) >= cv(%d), should not start\n",
		       __func__, vbat, cv);
		goto dis_mon;
	} else if (vbat <= pdata->pmic_uvlo) {
		/*
		 * If no 6pin used, vbat detected by vbat mon will be much
		 * lower than the PMIC UVLO.
		 */
		atomic_set(&cdata->no_6pin_used, 1);
		mt_dbg(cdata->dev, "%s, vbat <= PMIC UVLO(%d mV), should not start\n",
		       __func__, pdata->pmic_uvlo);
		goto dis_mon;
	}

	ret = mt6379_charger_field_set(cdata, F_BATINT, cv);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set batint(ret:%d)\n", __func__, ret);
		goto dis_mon;
	}

	batend_code = ADC_TO_VBAT_RAW(cv);
	batend_code = cpu_to_be16(batend_code);
	mt_dbg(cdata->dev, "%s, batend code = 0x%04x\n", __func__, batend_code);

	ret = regmap_bulk_write(cdata->rmap, MT6379_REG_BATEND_CODE, &batend_code, 2);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set batend code(ret:%d)\n", __func__, ret);
		goto dis_mon;
	}

	ret = mt6379_charger_field_set(cdata, F_BATPROTECT_EN, 1);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to enable bat protect(ret:%d)\n", __func__, ret);
		goto dis_mon;
	}

	/* set Max CV */
	ret = mt6379_charger_field_set(cdata, F_CV, 4710000);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to set maximum cv(ret:%d)\n", __func__, ret);
		goto dis_pro;
	}

	cdata->batprotect_en = true;
	mt_dbg(cdata->dev, "%s, enable 6pin charging successfully!!\n", __func__);
	goto out;

dis_pro:
	if (mt6379_charger_field_set(cdata, F_BATPROTECT_EN, 0) < 0)
		dev_notice(cdata->dev, "%s, Failed to disable bat protect\n", __func__);
	if (mt6379_charger_field_set(cdata, F_CV, cdata->cv) < 0)
		dev_notice(cdata->dev, "%s, Failed to set cv\n", __func__);

dis_mon:
	cdata->batprotect_en = false;
	ret = mt6379_charger_field_set(cdata, vbat_mon_en_field, 0);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to disable vbat_mon%s(ret:%d)!\n",
			 __func__, src == MT6379_BATPRO_SRC_VBAT_MON2 ? "2" : "", ret);

	ret = mt6379_charger_field_get(cdata, vbat_mon_en_field, &stat);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to get vbat_mon%s stat(ret:%d)!\n",
			 __func__, src == MT6379_BATPRO_SRC_VBAT_MON2 ? "2" : "", ret);

	dev_info(cdata->dev, "%s, Disable vbat_mon%s (current stat: %d)\n",
		 __func__, src == MT6379_BATPRO_SRC_VBAT_MON2 ? "2" : "", stat);

out:
	mutex_unlock(&cdata->cv_lock);
	return ret;
}

static inline int mt6379_enable_bat1_6pin_battery_charging(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_enable_6pin_battery_charging(cdata, MT6379_BATPRO_SRC_VBAT_MON, en);
}

static inline int __maybe_unused mt6379_enable_bat2_6pin_battery_charging(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_enable_6pin_battery_charging(cdata, MT6379_BATPRO_SRC_VBAT_MON2, en);
}

static int mt6379_enable_usbid(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_PD_MDEN, en ? 0 : 1);
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to %s PD_MDEN\n",
			 __func__, en ? "enable" : "disable");
		return ret;
	}

	ret = mt6379_charger_field_set(cdata, F_USBID_EN, en ? 1 : 0);
	if (ret)
		dev_info(cdata->dev, "%s, Failed to %s USBID_EN",
			 __func__, en ? "enable" : "disable");

	return ret;
}

static const u32 mt6379_usbid_rup[] = {
	500000, 75000, 5000, 1000,
};

static const u32 mt6379_usbid_src_ton[] = {
	400, 1000, 4000, 10000, 40000, 100000, 400000,
};

static inline u32 mt6379_trans_usbid_rup(u32 rup)
{
	int maxidx = ARRAY_SIZE(mt6379_usbid_rup) - 1;
	int i = 0;

	if (rup >= mt6379_usbid_rup[0])
		return 0;

	if (rup <= mt6379_usbid_rup[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (rup == mt6379_usbid_rup[i])
			return i;

		if (rup < mt6379_usbid_rup[i] && rup > mt6379_usbid_rup[i + 1]) {
			if ((mt6379_usbid_rup[i] - rup) <= (rup - mt6379_usbid_rup[i + 1]))
				return i;
			else
				return i + 1;
		}
	}

	return maxidx;
}

static int mt6379_set_usbid_rup(struct charger_device *chgdev, u32 rup)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = mt6379_trans_usbid_rup(rup);

	return mt6379_charger_field_set(cdata, F_ID_RUPSEL, val);
}

static inline u32 mt6379_trans_usbid_src_ton(u32 src_ton)
{
	int maxidx = ARRAY_SIZE(mt6379_usbid_src_ton) - 1;
	int i = 0;

	/* There is actually an option, always on, after 400000 */
	if (src_ton == 0)
		return maxidx + 1;

	if (src_ton <= mt6379_usbid_src_ton[0])
		return 0;

	if (src_ton >= mt6379_usbid_src_ton[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (src_ton == mt6379_usbid_src_ton[i])
			return i;

		if (src_ton > mt6379_usbid_src_ton[i] && src_ton < mt6379_usbid_src_ton[i + 1]) {
			if ((src_ton - mt6379_usbid_src_ton[i]) <=
			    (mt6379_usbid_src_ton[i + 1] - src_ton))
				return i;
			else
				return i + 1;
		}
	}

	return maxidx;
}

static int mt6379_set_usbid_src_ton(struct charger_device *chgdev, u32 src_ton)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = mt6379_trans_usbid_src_ton(src_ton);

	return mt6379_charger_field_set(cdata, F_IS_TDET, val);
}

static int mt6379_enable_usbid_floating(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_USBID_FLOATING, en);
}

#define BOOT_TIMES_MASK         0xC0
#define BOOT_TIMES_SHIFT        6
static int mt6379_set_boot_volt_times(struct charger_device *chgdev, u32 val)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u16 data = 0, ret;

	ret = regmap_bulk_read(cdata->rmap, MT6379_REG_FGADC_SYS_INFO_CON0, &data, sizeof(data));
	if (ret) {
		dev_info(cdata->dev, "%s, Failed to get boot_times\n", __func__);
		return ret;
	}

	data &= ~BOOT_TIMES_MASK;
	data |= (val << BOOT_TIMES_SHIFT & BOOT_TIMES_MASK);

	ret = regmap_bulk_write(cdata->rmap, MT6379_REG_FGADC_SYS_INFO_CON0, &data, sizeof(data));
	if (ret)
		dev_info(cdata->dev, "%s, Failed to write boot_times\n", __func__);

	return ret;
}

static const struct charger_ops mt6379_charger_ops = {
	/* cable plug in/out */
	.plug_in = mt6379_plug_in,
	.plug_out = mt6379_plug_out,
	/* enable */
	.enable = mt6379_enable_charging,
	.is_enabled = mt6379_is_enabled,

	/* charging current */
	.set_charging_current = mt6379_set_ichg,
	.get_charging_current = mt6379_get_ichg,
	.get_min_charging_current = mt6379_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = mt6379_set_cv,
	.get_constant_voltage = mt6379_get_cv,
	/* input current limit */
	.set_input_current = mt6379_set_aicr,
	.get_input_current = mt6379_get_aicr,
	.get_min_input_current = mt6379_get_min_aicr,
	/* MIVR */
	.set_mivr = mt6379_set_mivr,
	.get_mivr = mt6379_get_mivr,
	.get_mivr_state = mt6379_get_mivr_state,
	/* ADC */
	.get_adc = mt6379_get_adc,
	.get_vbus_adc = mt6379_get_vbus,
	.get_ibus_adc = mt6379_get_ibus,
	.get_ibat_adc = mt6379_get_ibat,
	.get_tchg_adc = mt6379_get_tchg,
	.get_zcv = mt6379_get_zcv,
	/* charging termination */
	.set_eoc_current = mt6379_set_ieoc,
	.enable_termination = mt6379_enable_te,
	.reset_eoc_state = mt6379_reset_eoc_state,
	.safety_check = mt6379_sw_check_eoc,
	.is_charging_done = mt6379_is_charge_done,
	/* power path */
	.enable_powerpath = mt6379_enable_buck,
	.is_powerpath_enabled = mt6379_is_buck_enabled,
	/* timer */
	.enable_safety_timer = mt6379_enable_charger_timer,
	.is_safety_timer_enabled = mt6379_is_charger_timer_enabled,
	.kick_wdt = mt6379_kick_wdt,
	/* AICL */
	.run_aicl = mt6379_run_aicc,
	/* PE+/PE+20 */
	.send_ta_current_pattern = mt6379_set_pe_current_pattern,
	.set_pe20_efficiency_table = mt6379_set_pe20_efficiency_table,
	.send_ta20_current_pattern = mt6379_set_pe20_current_pattern,
	.reset_ta = mt6379_reset_pe_ta,
	.enable_cable_drop_comp = mt6379_enable_pe_cable_drop_comp,
	.enable_discharge = mt6379_enable_discharge,
	/* charger type detection */
	.enable_chg_type_det = mt6379_enable_chg_type_det,
	/* misc */
	.dump_registers = mt6379_dump_registers,
	.enable_hz = mt6379_enable_hz,
	/* event */
	.event = mt6379_do_event,
	.enable_6pin_battery_charging = mt6379_enable_bat1_6pin_battery_charging,
	/* TypeC */
	.enable_usbid = mt6379_enable_usbid,
	.set_usbid_rup = mt6379_set_usbid_rup,
	.set_usbid_src_ton = mt6379_set_usbid_src_ton,
	.enable_usbid_floating = mt6379_enable_usbid_floating,
	/* set boot volt times */
	.set_boot_volt_times = mt6379_set_boot_volt_times,
};

static const struct charger_properties mt6379_charger_props = {
	.alias_name = "mt6379_chg",
};

static void mt6379_charger_init_destroy(void *data)
{
	struct charger_device *charger_dev = data;

	charger_device_unregister(charger_dev);
}

int mt6379_charger_init_chgdev(struct mt6379_charger_data *cdata)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);

	cdata->chgdev = charger_device_register(pdata->chgdev_name, cdata->dev,
						cdata, &mt6379_charger_ops,
						&mt6379_charger_props);
	if (IS_ERR(cdata->chgdev)) {
		dev_info(cdata->dev, "%s, Failed to register charger device\n", __func__);
		return PTR_ERR(cdata->chgdev);
	}

	return devm_add_action_or_reset(cdata->dev, mt6379_charger_init_destroy, cdata->chgdev);
}
