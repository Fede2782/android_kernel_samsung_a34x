// SPDX-License-Identifier: GPL-2.0
/* afc_charger.c
 *
 * Copyright (C) 2024 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)    "[AFC] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/ktime.h>
#include <linux/pinctrl/consumer.h>

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
#include <linux/muic/common/vt_muic/vt_muic.h>
#endif
#include <linux/muic/afc_gpio/gpio_afc_charger.h>
#include <linux/muic/afc_gpio/afc_charger_kunit.h>

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <dt-bindings/battery/sec-battery.h>
#include <linux/power_supply.h>
#include <../drivers/battery/common/sec_charging_common.h>
#include <../drivers/battery/common/sec_battery.h>
#endif

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
#include <../drivers/battery/charger/syv660_charger/syv660_charger.h>
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_param.h>
#endif

struct gpio_afc_ddata *g_ddata;

#ifdef AFC_DEBUG
static void afc_udelay(int delay)
{
	s64 start = 0, duration = 0;

	start = ktime_to_us(ktime_get());

	udelay(delay);

	duration = ktime_to_us(ktime_get()) - start;

	if (duration > delay + 20)
		pr_err("%s %dus -> %dus\n", __func__, delay, duration);
}
#else
#define afc_udelay(d)	udelay(d)
#endif

__visible_for_testing int __mockable afc_d_minus_control_high(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int ret = 0;

	if (gpio_is_valid(gpio)) {
		gpio_set_value(gpio, 1);
	} else {
		pr_err("%s gpio invalid(%d)\n", __func__, gpio);
		ret = -ENOENT;
	}
	return ret;
}

__visible_for_testing int __mockable afc_d_minus_control_low(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int ret = 0;

	if (gpio_is_valid(gpio)) {
		gpio_set_value(gpio, 0);
	} else {
		pr_err("%s gpio invalid(%d)\n", __func__, gpio);
		ret = -ENOENT;
	}
	return ret;

}

static void afc_d_minus_set_gpio_output_low(struct gpio_afc_ddata *ddata)
{
	if (ddata->gpio_input) {
		ddata->gpio_input = false;
		gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
	}
}

static void afc_d_minus_set_gpio_input(struct gpio_afc_ddata *ddata)
{
	if (!ddata->gpio_input) {
		ddata->gpio_input = true;
		gpio_direction_input(ddata->pdata->gpio_afc_data);
	}
}

static int afc_d_minus_get_level(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int level = 0;

	if (gpio_is_valid(gpio))
		level = gpio_get_value(gpio);

	return level;
}

__visible_for_testing void gpio_afc_send_parity_bit(struct gpio_afc_ddata *ddata, int data)
{
	int cnt = 0, i = 0;

	if (data < 0 || data > 0xFF) {
		pr_err("%s data error. data=%x\n", __func__, data);
		return;
	}

	for (i = 0; i < 8; i++) {
		if (data & (0x1 << i))
			cnt++;
	}

	cnt %= 2;

	if (cnt)
		afc_d_minus_control_low(ddata);
	else
		afc_d_minus_control_high(ddata);

	afc_udelay(UI);

	if (!cnt) {
		afc_d_minus_control_low(ddata);
		afc_udelay(SYNC_PULSE);
	}
}

static void gpio_afc_sync_pulse(struct gpio_afc_ddata *ddata)
{
	afc_d_minus_control_high(ddata);
	afc_udelay(SYNC_PULSE);
	afc_d_minus_control_low(ddata);
	afc_udelay(SYNC_PULSE);
}

static int gpio_afc_send_mping(struct gpio_afc_ddata *ddata)
{
	unsigned long flags;
	int delayed_time = 0;
	s64 start = 0, end = 0;

	afc_d_minus_set_gpio_output_low(ddata);

	/* send mping */
	spin_lock_irqsave(&ddata->spin_lock, flags);
	afc_d_minus_control_high(ddata);
	afc_udelay(MPING);
	afc_d_minus_control_low(ddata);

	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&ddata->spin_lock, flags);
	end = ktime_to_us(ktime_get());

	delayed_time = (int)end-start;
	return delayed_time;
}

static int gpio_afc_check_sping(struct gpio_afc_ddata *ddata, int delay)
{
	int delayed_time = 0;
	s64 start = 0, end = 0, duration = 0;
	unsigned long flags;

	if (delay > (MPING + MPING_GUARD_TIME)) {
		delayed_time = delay - (MPING + MPING_GUARD_TIME);
		return delayed_time;
	}

	afc_d_minus_set_gpio_input(ddata);

	spin_lock_irqsave(&ddata->spin_lock, flags);
	if (delay < (MPING - MPING_GUARD_TIME))
		afc_udelay(MPING - MPING_GUARD_TIME - delay);

	if (!afc_d_minus_get_level(ddata)) {
		delayed_time = -EAGAIN;
		goto out;
	}

	start = ktime_to_us(ktime_get());
	while (afc_d_minus_get_level(ddata)) {
		duration = ktime_to_us(ktime_get()) - start;
		if (duration > DATA_DELAY)
			break;
		afc_udelay(UI);
	}
out:
	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&ddata->spin_lock, flags);
	end = ktime_to_us(ktime_get());

	if (delayed_time == 0)
		delayed_time = (int)end-start;

	return delayed_time;
}

__visible_for_testing void gpio_afc_data_control(struct gpio_afc_ddata *ddata, int data)
{
	int i;

	for (i = (0x1 << 7); i > 0; i = i >> 1) {
		if (data & i)
			afc_d_minus_control_high(ddata);
		else
			afc_d_minus_control_low(ddata);
		afc_udelay(UI);
	}
}

static int gpio_afc_send_data(struct gpio_afc_ddata *ddata, int data)
{
	int delayed_time = 0;
	unsigned long flags;
	s64 start = 0, end = 0;

	if (data < 0 || data > 0xFF) {
		pr_err("%s data error. data=%x\n", __func__, data);
		return 0;
	}

	afc_d_minus_set_gpio_output_low(ddata);

	spin_lock_irqsave(&ddata->spin_lock, flags);

	afc_udelay(UI);

	/* start of transfer */
	gpio_afc_sync_pulse(ddata);
	if (!(data & 0x80)) {
		afc_d_minus_control_high(ddata);
		afc_udelay(SYNC_PULSE);
	}

	gpio_afc_data_control(ddata, data);

	gpio_afc_send_parity_bit(ddata, data);
	gpio_afc_sync_pulse(ddata);

	afc_d_minus_control_high(ddata);
	afc_udelay(MPING);
	afc_d_minus_control_low(ddata);

	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&ddata->spin_lock, flags);
	end = ktime_to_us(ktime_get());

	delayed_time = (int)end-start;

	return delayed_time;
}

static void gpio_afc_wait_data_complete(struct gpio_afc_ddata *ddata, int delay)
{
	s64 limit_start = 0, start = 0, end = 0, duration = 0;
	int gpio_value = 0, reset = 1;
	unsigned long flags;

	spin_lock_irqsave(&ddata->spin_lock, flags);

	afc_udelay(DATA_DELAY-delay);

	limit_start = ktime_to_us(ktime_get());
	while (duration < MPING - MPING_GUARD_TIME) {
		if (reset) {
			start = 0;
			end = 0;
			duration = 0;
		}
		gpio_value = afc_d_minus_get_level(ddata);
		if (!gpio_value && !reset) {
			end = ktime_to_us(ktime_get());
			duration = end - start;
			reset = 1;
		} else if (gpio_value) {
			if (reset) {
				start = ktime_to_us(ktime_get());
				reset = 0;
			}
		}
		afc_udelay(UI);
		if ((ktime_to_us(ktime_get()) - limit_start) > (MPING + DATA_DELAY*2))
			break;
	}

	spin_unlock_irqrestore(&ddata->spin_lock, flags);
}

static int gpio_afc_recv_data(struct gpio_afc_ddata *ddata, int delay)
{
	int ret = 0;

	afc_d_minus_set_gpio_input(ddata);

	if (delay > DATA_DELAY + MPING)
		ret = -EAGAIN;
	else if (delay > DATA_DELAY && delay <= DATA_DELAY + MPING)
		gpio_afc_check_sping(ddata, delay - DATA_DELAY);
	else if (delay <= DATA_DELAY)
		gpio_afc_wait_data_complete(ddata, delay);

	return ret;
}

static int gpio_afc_set_voltage(struct gpio_afc_ddata *ddata, u32 voltage)
{
	int ret = 0, preempted_time = 0, code = 0;

	preempted_time = gpio_afc_send_mping(ddata);
	preempted_time = gpio_afc_check_sping(ddata, preempted_time);
	if (preempted_time < 0) {
		pr_err("Start Mping NACK\n");
		goto out;
	}

	if (voltage == 0x9) {
		code = (AFC_9V << 4) | AFC_1650MA;
		preempted_time = gpio_afc_send_data(ddata, code);
	} else {
		code = (AFC_5V << 4) | AFC_1950MA;
		preempted_time = gpio_afc_send_data(ddata, code);
	}

	preempted_time = gpio_afc_check_sping(ddata, preempted_time);
	if (preempted_time < 0) {
		pr_err("sping err2 %d\n", ret);
		goto out;
	}

	ret = gpio_afc_recv_data(ddata, preempted_time);
	if (ret < 0)
		pr_err("sping err3 %d\n", ret);

	preempted_time = gpio_afc_send_mping(ddata);
	preempted_time = gpio_afc_check_sping(ddata, preempted_time);
	if (preempted_time < 0)
		pr_err("End Mping NACK\n");
out:
	return preempted_time;
}

static void gpio_afc_reset(struct gpio_afc_ddata *ddata)
{
	afc_d_minus_set_gpio_output_low(ddata);

	/* send mping */
	afc_d_minus_control_high(ddata);
	afc_udelay(RESET_DELAY);
	afc_d_minus_control_low(ddata);
}

void set_afc_voltage_for_performance(bool enable)
{
	struct gpio_afc_ddata *ddata = g_ddata;
	if (g_ddata == NULL)
		return;
	ddata->check_performance = enable;
}
EXPORT_SYMBOL_GPL(set_afc_voltage_for_performance);

static int gpio_afc_get_vbus_syv660(struct gpio_afc_ddata *ddata)
{
	int vbus = 0;
#if	IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval value = {0, };
	struct power_supply *psy;

	if (ddata->rp_currentlvl == RP_CURRENT_LEVEL2
			&& ddata->dpdm_ctrl_on) {
		pr_info("%s rp22k\n", __func__);
		vbus = 9000;
		return vbus;
	}

	psy = get_power_supply_by_name("bc12");

	if (!psy)
		return 0;

	if ((psy->desc->get_property != NULL) &&
			(psy->desc->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value) >= 0)) {
		vbus = value.intval;
		pr_info("gpio_afc_get_vbus: vbus=%d\n", vbus);
	}
#endif
	return vbus;
}

__visible_for_testing int __mockable gpio_afc_get_vbus(struct gpio_afc_ddata *ddata)
{
	int vbus = 0;

	if (IS_ENABLED(CONFIG_CHARGER_SYV660))
		vbus = gpio_afc_get_vbus_syv660(ddata);
	else if (IS_ENABLED(CONFIG_MTK_CHARGER)) {
		charger_dev_get_vbus(ddata->chg_dev, &vbus);
		vbus /= 1000;
	}

	return vbus;
}

__visible_for_testing int vbus_level_check(struct gpio_afc_ddata *ddata)
{
	int vbus = 0;

	vbus = gpio_afc_get_vbus(ddata);

	pr_info("%s %dmV \n", __func__, vbus);

	if (vbus > 3800 && vbus <= 7500)
		vbus = 0x5;
	else if (vbus > 7500 && vbus < 10500)
		vbus = 0x9;
	else
		vbus = 0x0;

	return vbus;
}

static bool is_vbus_disconnected_syv660(struct gpio_afc_ddata *data)
{
	struct gpio_afc_ddata *ddata = data;

	if (!ddata->dpdm_ctrl_on)
		return true;
	return false;
}

__visible_for_testing bool is_vbus_disconnected(struct gpio_afc_ddata *ddata)
{
	int ret = false;

	if (IS_ENABLED(CONFIG_CHARGER_SYV660)) {
		return is_vbus_disconnected_syv660(ddata);
	} else {
		if (!vbus_level_check(ddata))
			ret = true;
	}

	return ret;
}

static int gpio_afc_try_3times(struct gpio_afc_ddata *ddata, int set_vol)
{
	int i, cur_vol = 0, ret = -EAGAIN;

	for (i = 0 ; i < AFC_OP_CNT ; i++) {
		gpio_afc_set_voltage(ddata, set_vol);
		msleep(20);
		cur_vol = vbus_level_check(ddata);
		ddata->curr_voltage = cur_vol;
		pr_info("%s current %dV\n", __func__, cur_vol);
		if (cur_vol == set_vol) {
			ret = 0;
			break;
		}
	}
	return ret;
}

static void gpio_afc_process_set_voltage(struct gpio_afc_ddata *ddata, int set_vol)
{
	int retry, cur_vol = 0;

	for (retry = 0; retry < AFC_RETRY_CNT; retry++) {

		gpio_afc_try_3times(ddata, set_vol);

		msleep(50);

		cur_vol = vbus_level_check(ddata);
		ddata->curr_voltage = cur_vol;
		pr_info("%s current %dV\n", __func__, cur_vol);

		if (cur_vol == set_vol) {
			pr_info("%s success\n", __func__);
			break;
		}

		if (is_vbus_disconnected(ddata)) {
			pr_err("%s ta disconnected\n", __func__);
			break;
		}
		pr_err("%s retry %d\n", __func__, retry + 1);
	}
}

static int gpio_afc_retry_check_vbus(struct gpio_afc_ddata *ddata, int set_vol)
{
	int i, cur_vol = 0, ret = -EAGAIN;

	for (i = 0 ; i < VBUS_RETRY_MAX ; i++) {
		cur_vol = vbus_level_check(ddata);
		ddata->curr_voltage = cur_vol;
		if (cur_vol == set_vol) {
			ret = 0;
			pr_info("%s vbus retry success\n", __func__);
			break;
		}

		if (is_vbus_disconnected(ddata)) {
			pr_err("%s retry ta disconnected\n", __func__);
			break;
		}

		msleep(100);
		pr_info("%s vbus recheck cnt=%d\n", __func__, i + 1);
	}

	return ret;
}

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
static void gpio_afc_process_vt_muic(int set_vol, int ret)
{
	if (set_vol == 0x9) {
		if (!ret)
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
		else
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
	} else if (set_vol == 0x5) {
		if (!ret) {
			if (vt_muic_get_afc_disable())
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
			else
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		} else
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC);
	}
}
#endif

static void gpio_afc_kwork(struct kthread_work *work)
{
	struct gpio_afc_ddata *ddata =
	    container_of(work, struct gpio_afc_ddata, kwork);

	int ret = -EAGAIN;
	int set_vol = 0;

	if (!ddata) {
		pr_err("driver is not ready\n");
		return;
	}

	set_vol = ddata->set_voltage;
	pr_info("%s set vol[%dV]\n", __func__, set_vol);

	if (set_vol == 0x9) {
		msleep(1200);
	} else {
		gpio_afc_reset(ddata);
		msleep(200);
	}

	if (is_vbus_disconnected(ddata)) {
		pr_err("%s disconnected\n", __func__);
		return;
	}

	mutex_lock(&ddata->mutex);
	__pm_stay_awake(&ddata->ws);

	gpio_set_value(ddata->pdata->gpio_afc_switch, 1);

	if (set_vol) {
		gpio_afc_process_set_voltage(ddata, set_vol);
	} else {
		ret = 0;
		goto err;
	}

	ret = gpio_afc_retry_check_vbus(ddata, set_vol);
	if (ret < 0)
		goto err;

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	gpio_afc_process_vt_muic(set_vol, ret);
#endif

err:
	gpio_set_value(ddata->pdata->gpio_afc_switch, 0);
	__pm_relax(&ddata->ws);
	mutex_unlock(&ddata->mutex);
}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) && IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int gpio_afc_tcm_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct gpio_afc_ddata *ddata =
			container_of(nb, struct gpio_afc_ddata, typec_nb);

	PD_NOTI_TYPEDEF *pdata = (PD_NOTI_TYPEDEF *)data;
	struct pdic_notifier_struct *pd_noti = pdata->pd;

	if (pdata->dest != PDIC_NOTIFY_DEV_BATT) {
		return 0;
	}

	ddata->rp_currentlvl = RP_CURRENT_LEVEL_NONE;

	if (!pd_noti) {
		pr_info("%s: pd_noti(pdata->pd) is NULL\n", __func__);
	} else {
		ddata->rp_currentlvl = pd_noti->sink_status.rp_currentlvl;
		pr_info("%s rplvl=%d\n", __func__, ddata->rp_currentlvl);
	}

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
static int afc_charger_set_voltage(int voltage)
{
	struct gpio_afc_ddata *ddata = g_ddata;
	int vbus = 0;

	if (!ddata) {
		pr_err("driver is not ready\n");
		return -EAGAIN;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	if (voltage == 0x9 && vt_muic_get_afc_disable()) {
		pr_err("AFC is disabled by USER\n");
		return - EINVAL;
	}
#endif

	kthread_flush_work(&ddata->kwork);

	vbus = vbus_level_check(ddata);

	pr_info("%s %d(%d) => %dV\n", __func__,
		ddata->curr_voltage, vbus, voltage);

	if (ddata->curr_voltage == voltage) {
		if (ddata->curr_voltage == vbus) {
			msleep(20);
			vbus = vbus_level_check(ddata);
			if (ddata->curr_voltage == vbus)
				return 0;
		}
	}

	ddata->curr_voltage = vbus;
	ddata->set_voltage = voltage;

	kthread_queue_work(&ddata->kworker, &ddata->kwork);

	return 0;
}

static int afc_charger_get_adc(void *mdata)
{
	int result = 0;

#if 0 /* TBD IS_ENABLED(CONFIG_TCPC_MT6360) */
	result = mt6360_usbid_check();
#endif

	pr_info("%s %d\n", __func__, result);

	return result;
}

static int afc_charger_get_vbus_value(void *mdata)
{
	struct gpio_afc_ddata *ddata = mdata;
	int retval = -ENODEV;

	if (!ddata) {
		pr_err("%s : driver data is null\n", __func__);
		return retval;
	}

	if (ddata->curr_voltage) {
		pr_info("%s %dV\n", __func__, ddata->curr_voltage);
		return ddata->curr_voltage;
	}

	pr_info("%s No value", __func__);
	return 0;
}

static void afc_charger_set_afc_disable(void *mdata)
{
	struct gpio_afc_ddata *ddata = mdata;
	int attached_dev = vt_muic_get_attached_dev();
	int vbus = 0, curr = 0;

	pr_info("%s afc_disable(%d)\n", __func__, vt_muic_get_afc_disable());

	if (!ddata) {
		pr_err("%s gpio_afc_ddata is null\n", __func__);
		return;
	}

	switch (attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC ... ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
		kthread_flush_work(&ddata->kwork);

		curr = vbus_level_check(ddata);
		vbus = vt_muic_get_afc_disable() ? 5 : 9;

		if (curr == vbus) {
			if (vt_muic_get_afc_disable() && attached_dev != ATTACHED_DEV_TA_MUIC)
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
		} else {
			ddata->curr_voltage = curr;
			ddata->set_voltage = vbus;
			kthread_queue_work(&ddata->kworker, &ddata->kwork);
		}
		break;
	default:
		break;
	}
}

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int afc_charger_set_hiccup_mode(void *mdata, bool en)
{
	struct gpio_afc_ddata *ddata = mdata;
	struct gpio_afc_pdata *pdata;
	int ret = -ENODEV;

	if (!ddata) {
		pr_err("%s gpio_afc_ddata is null\n", __func__);
		return ret;
	}

	pdata = ddata->pdata;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	if (is_lpcharge_pdic_param()) {
		pr_info("%s %d, lpm mode\n", __func__, en);
		return ret;
	}
#endif

	if (ddata->hiccup_mode && en) {
		pr_info("%s hiccup_mode already enabled, en = %d\n", __func__, en);
		return ret;
	}

	ddata->hiccup_mode = en;

	gpio_set_value(pdata->gpio_hiccup, en);

	ret = gpio_get_value(pdata->gpio_hiccup);

#if IS_ENABLED(CONFIG_TCPC_MT6360) && IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	if (ddata->water_state == CC_WATER_CC_HIGH) {
		PD_NOTI_TYPEDEF pdic_noti;

		pdic_noti.src = PDIC_NOTIFY_DEV_PDIC;
		pdic_noti.dest = PDIC_NOTIFY_DEV_MUIC;
		pdic_noti.id = PDIC_NOTIFY_ID_WATER_CABLE;
		pdic_noti.sub1 = 1;
		pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, 0, 0);
	}
#endif

	pr_info("%s %d, %d\n", __func__, en, ret);

	return ret;
}

static int afc_charger_get_hiccup_mode(void *mdata)
{
	struct gpio_afc_ddata *ddata = mdata;

	if (!ddata) {
		pr_err("%s gpio_afc_ddata is null\n", __func__);
		return -1;
	}

	return ddata->hiccup_mode;
}

static void afc_charger_set_cc_high_state(void *mdata, int state, bool en)
{
	struct gpio_afc_ddata *ddata = mdata;
	int attached_dev = vt_muic_get_attached_dev();

	if (en)
		ddata->water_state |= state;
	else
		ddata->water_state &= ~state;

	if (is_lpcharge_pdic_param() && en) {
		if (ddata->water_state & CC_WATER_STATE) {
			pr_info("%s ddata->water_state %d\n", __func__, ddata->water_state);
			if (attached_dev >= ATTACHED_DEV_AFC_CHARGER_9V_MUIC &&
					attached_dev <= ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC)
				afc_charger_set_voltage(5);
		}
	}

	pr_info("%s %d, %d\n", __func__, en, ddata->water_state);
}
#endif /* CONFIG_HICCUP_CHARGER */

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
static void afc_dpdm_ctrl(bool onoff)
{
	struct gpio_afc_ddata *ddata = g_ddata;
	struct gpio_afc_pdata *pdata;
	int value = 0;

	if (!ddata) {
		pr_err("%s gpio_afc_ddata is null\n", __func__);
		return;
	}

	pdata = ddata->pdata;

	ddata->dpdm_ctrl_on = onoff;
	gpio_set_value(pdata->gpio_afc_hvdcp, onoff);
	value = gpio_get_value(pdata->gpio_afc_hvdcp);

	if (!onoff)
		ddata->curr_voltage = 0;

	pr_info("%s: onoff=%d, pdata->gpio_afc_hvdcp=%d value=%d\n",
				__func__, onoff, pdata->gpio_afc_hvdcp, value);
}
#endif

static void register_muic_ops(struct vt_muic_ic_data *ic_data)
{
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	ic_data->m_ops.afc_dpdm_ctrl = afc_dpdm_ctrl;
#endif
	ic_data->m_ops.afc_set_voltage = afc_charger_set_voltage;
	ic_data->m_ops.get_adc = afc_charger_get_adc;
	ic_data->m_ops.get_vbus_value = afc_charger_get_vbus_value;
	ic_data->m_ops.set_afc_disable = afc_charger_set_afc_disable;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	ic_data->m_ops.set_hiccup_mode = afc_charger_set_hiccup_mode;
	ic_data->m_ops.get_hiccup_mode = afc_charger_get_hiccup_mode;
	ic_data->m_ops.set_cc_high_state = afc_charger_set_cc_high_state;
#endif
}
#endif

static int register_gpio(struct gpio_afc_pdata *pdata)
{
	int ret = 0;

	if (gpio_is_valid(pdata->gpio_afc_switch)) {
		ret = gpio_request(pdata->gpio_afc_switch, "gpio_afc_switch");
		if (ret < 0) {
			pr_err("%s: failed to request afc switch gpio\n", __func__);
			goto err_gpio_afc_switch;
		}
		gpio_direction_output(pdata->gpio_afc_switch, 0);
	}

	if (gpio_is_valid(pdata->gpio_afc_data)) {
		ret = gpio_request(pdata->gpio_afc_data, "gpio_afc_data");
		if (ret < 0) {
			pr_err("%s: failed to request afc data gpio\n", __func__);
			goto err_gpio_afc_data;
		}
		gpio_direction_output(pdata->gpio_afc_data, 0);
	}

	if (gpio_is_valid(pdata->gpio_afc_hvdcp)) {
		ret = gpio_request(pdata->gpio_afc_hvdcp, "gpio_afc_hvdcp");
		if (ret < 0) {
			pr_err("%s: failed to request afc hvdcp gpio\n", __func__);
			goto err_gpio_afc_hvdcp;
		}
		gpio_direction_output(pdata->gpio_afc_hvdcp, 0);
	}

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	if (gpio_is_valid(pdata->gpio_hiccup)) {
		ret = gpio_request(pdata->gpio_hiccup, "hiccup");
		if (ret < 0) {
			pr_err("%s: failed to request hiccup gpio\n", __func__);
			goto err_gpio_hiccup;
		}
		gpio_direction_output(pdata->gpio_hiccup, 0);
	}
#endif	/* CONFIG_HICCUP_CHARGER */

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	afc_dpdm_ctrl(0);
#endif

	return 0;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
err_gpio_hiccup:
	if (gpio_is_valid(pdata->gpio_afc_hvdcp))
		gpio_free(pdata->gpio_afc_hvdcp);
#endif	/* CONFIG_HICCUP_CHARGER */
err_gpio_afc_hvdcp:
	if (gpio_is_valid(pdata->gpio_afc_data))
		gpio_free(pdata->gpio_afc_data);
err_gpio_afc_data:
	if (gpio_is_valid(pdata->gpio_afc_switch))
		gpio_free(pdata->gpio_afc_switch);
err_gpio_afc_switch:
	return ret;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static void set_gpio_afc_disable_work(struct work_struct *work)
{
	union power_supply_propval psy_val;
	int ret = 0;

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	psy_val.intval = vt_muic_get_afc_disable() ? '1' : '0';
#endif
	ret = psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
	pr_info("set_gpio_afc_disable_work: ret=%d\n", ret);
}
#endif

static struct gpio_afc_pdata *gpio_afc_get_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct gpio_afc_pdata *pdata;

	if (!np)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_afc_switch = of_get_named_gpio(np, "gpio_afc_switch", 0);
	pdata->gpio_afc_data = of_get_named_gpio(np, "gpio_afc_data", 0);
	pr_info("gpio_afc_switch %d, gpio_afc_data %d\n",
		pdata->gpio_afc_switch, pdata->gpio_afc_data);

	pdata->gpio_afc_hvdcp = of_get_named_gpio(np, "gpio_afc_hvdcp", 0);
	if (gpio_is_valid(pdata->gpio_afc_hvdcp))
		pr_info("gpio_afc_hvdcp %d\n", pdata->gpio_afc_hvdcp);

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	pdata->gpio_hiccup = of_get_named_gpio(np, "gpio_hiccup", 0);
	if (gpio_is_valid(pdata->gpio_hiccup))
		pr_info("gpio_hiccup : %d\n", pdata->gpio_hiccup);
#endif	/* CONFIG_HICCUP_CHARGER */
	return pdata;
}

static int gpio_afc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_afc_pdata *pdata = dev_get_platdata(dev);
	struct gpio_afc_ddata *ddata;
	struct task_struct *kworker_task;

	int ret = 0;

	pr_info("%s\n", __func__);

	if (!pdata)
		pdata = gpio_afc_get_dt(dev);

	if (!pdata)
		goto err_kzalloc;

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		goto err_kzalloc;

	g_ddata = ddata;

	ddata->ws.name = "afc_wakelock";
	wakeup_source_add(&ddata->ws);

	spin_lock_init(&ddata->spin_lock);
	mutex_init(&ddata->mutex);
	dev_set_drvdata(dev, ddata);

	kthread_init_worker(&ddata->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
		&ddata->kworker, "gpio_afc");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		goto err_kworker_task;
	}
	kthread_init_work(&ddata->kwork, gpio_afc_kwork);

	ddata->pdata = pdata;
	ddata->gpio_input = false;
	ddata->hiccup_mode = false;

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	ddata->afc_ic_data.mdata = ddata;
	register_muic_ops(&ddata->afc_ic_data);
	vt_muic_register_ic_data(&ddata->afc_ic_data);
#endif

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	ddata->chg_dev = get_charger_by_name("primary_chg");
	if (ddata->chg_dev)
		pr_info("Found primary charger [%s]\n",
			ddata->chg_dev->props.alias_name);
	else
		pr_err("*** Error : can't find primary charger ***\n");
#endif

	ret = register_gpio(pdata);
	if (ret)
		goto err_gpio;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&ddata->set_gpio_afc_disable,
			  set_gpio_afc_disable_work);
	schedule_delayed_work(&ddata->set_gpio_afc_disable,
		msecs_to_jiffies(1000));
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ddata->rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	manager_notifier_register(&ddata->typec_nb,
		gpio_afc_tcm_handle_notification, MANAGER_NOTIFY_PDIC_BATTERY);
#endif	/* CONFIG_USB_TYPEC_MANAGER_NOTIFIER */
#endif	/* CONFIG_BATTERY_SAMSUNG */

	return 0;
err_gpio:
	ddata->pdata = NULL;
err_kworker_task:
	dev_set_drvdata(dev, NULL);
	wakeup_source_remove(&ddata->ws);
	g_ddata = NULL;
err_kzalloc:
	return ret;
}

static void gpio_afc_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_afc_ddata *ddata = dev_get_drvdata(dev);

	kthread_destroy_worker(&ddata->kworker);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	cancel_delayed_work_sync(&ddata->set_gpio_afc_disable);
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) && IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	manager_notifier_unregister(&ddata->typec_nb);
#endif

	wakeup_source_remove(&ddata->ws);
	ddata->pdata = NULL;
	g_ddata = NULL;
	dev_set_drvdata(dev, NULL);
}

static const struct of_device_id gpio_afc_of_match[] = {
	{.compatible = "gpio_afc",},
	{},
};

MODULE_DEVICE_TABLE(of, gpio_afc_of_match);

static struct platform_driver gpio_afc_driver = {
	.shutdown = gpio_afc_shutdown,
	.driver = {
		   .name = "gpio_afc",
		   .of_match_table = gpio_afc_of_match,
	},
};

module_platform_driver_probe(gpio_afc_driver, gpio_afc_probe);

MODULE_DESCRIPTION("Samsung GPIO AFC driver");
MODULE_LICENSE("GPL");
