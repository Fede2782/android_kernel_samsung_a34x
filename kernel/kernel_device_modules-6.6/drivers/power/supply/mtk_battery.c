// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/cdev.h>		/* cdev */
#include <linux/err.h>	/* IS_ERR, PTR_ERR */
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
#include "mtk_battery_table.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <../drivers/battery/common/sec_charging_common.h>
#define SEC_BATTERY_FAKE_CAPACITY 0
static char __read_mostly *f_mode;
module_param(f_mode, charp, 0444);
#endif

struct mtk_battery *gmb;

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

int __attribute__ ((weak))
	mtk_battery_daemon_init(struct platform_device *pdev)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;

	gm->algo.active = true;
	bm_err(gm, "[%s]: weak function,kernel algo=%d\n", __func__,
		gm->algo.active);
	return -EIO;
}

__weak void mtk_irq_thread_init(struct mtk_battery *gm)
{
	disable_fg(gm);
	bm_err(gm, "[%s]: weak function,kernel disable gauge=%d\n", __func__,
		gm->disableGM30);
}

int __attribute__ ((weak))
	wakeup_fg_daemon(struct mtk_battery *gm, unsigned int flow_state, int cmd, int para1)
{
	return 0;
}

void __attribute__ ((weak))
	fg_sw_bat_cycle_accu(struct mtk_battery *gm)
{
}

void __attribute__ ((weak))
	notify_fg_chr_full(struct mtk_battery *gm)
{
}

void __attribute__ ((weak))
	fg_drv_update_daemon(struct mtk_battery *gm)
{
}

void enable_gauge_irq(struct mtk_gauge *gauge,
	enum gauge_irq irq)
{
	struct irq_desc *desc;

	if (irq >= GAUGE_IRQ_MAX || gauge->gm->disableGM30)
		return;

	desc = irq_to_desc(gauge->irq_no[irq]);
	bm_debug(gauge->gm, "%s irq_no:%d:%d depth:%d\n",
		__func__, irq, gauge->irq_no[irq],
		desc->depth);
	if (desc->depth == 1)
		enable_irq(gauge->irq_no[irq]);
}

void disable_gauge_irq(struct mtk_gauge *gauge,
	enum gauge_irq irq)
{
	struct irq_desc *desc;

	if (irq >= GAUGE_IRQ_MAX)
		return;

	if (gauge->irq_no[irq] == 0)
		return;

	desc = irq_to_desc(gauge->irq_no[irq]);
	bm_debug(gauge->gm, "%s irq_no:%d:%d depth:%d\n",
		__func__, irq, gauge->irq_no[irq],
		desc->depth);
	if (desc->depth == 0)
		disable_irq_nosync(gauge->irq_no[irq]);
}

//Only return gauge1 for old platform
struct mtk_battery *get_mtk_battery(void)
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
			pr_debug("[%s] psy is not rdy\n", __func__);
			return NULL;
		}
		bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);
		power_supply_put(psy);
		if (bm == NULL) {
			pr_debug("[%s] mtk_battery_manager is not rdy\n", __func__);
			return NULL;
		}
	}

	return bm->gm1;
}

bool is_algo_active(struct mtk_battery *gm)
{
	return gm->algo.active;
}

int fgauge_get_profile_id(struct mtk_battery *gm)
{
	return gm->battery_id;
}

int get_iavg_gap(struct mtk_battery *gm)
{
	int i = 0;
	int temp = 0, ret_val = 0;
	int is_ascending = 0, is_descending = 0;

	temp = gm->cur_bat_temp;
	for (i = 0;i < gm->fg_table_cust_data.active_table_number - 1;i++) {
		if (gm->fg_table_cust_data.fg_profile[i].temperature >
			gm->fg_table_cust_data.fg_profile[i + 1].temperature)
			break;
		is_ascending = 1;
		is_descending = 0;
	}

	for (i = 0;i < gm->fg_table_cust_data.active_table_number - 1;i++) {
		if (gm->fg_table_cust_data.fg_profile[i].temperature <
			gm->fg_table_cust_data.fg_profile[i + 1].temperature)
			break;
		is_ascending = 0;
		is_descending = 1;
	}

	if(is_ascending == 0 && is_descending == 0)
		return gm->fg_cust_data.diff_iavg_th;

	for (i = 1; i <  gm->fg_table_cust_data.active_table_number; i++) {
		if (is_ascending) {
			if (temp <=  gm->fg_table_cust_data.fg_profile[i].temperature)
				break;
		} else {
			if (temp >=  gm->fg_table_cust_data.fg_profile[i].temperature)
				break;
		}
	}

	if (i > ( gm->fg_table_cust_data.active_table_number - 1))
		i =  gm->fg_table_cust_data.active_table_number - 1;


	if (is_ascending) {
		bm_debug(gm, "[%s] is_ascending i:%d temp:%d ivag_gap:%d\n",
			__func__, i, temp, gm->iavg_th[i-1]);
		ret_val = gm->iavg_th[i-1];
	} else {
		bm_debug(gm, "[%s] is_descending i:%d temp:%d ivag_gap:%d\n",
			__func__, i, temp, gm->iavg_th[i]);
		ret_val =  gm->iavg_th[i];
	}

	return ret_val;
}


int wakeup_fg_algo_cmd(
	struct mtk_battery *gm, unsigned int flow_state, int cmd, int para1)
{

	bm_debug(gm, "[%s] 0x%x %d %d\n", __func__, flow_state, cmd, para1);
	if (gm->disableGM30) {
		bm_err(gm, "FG daemon is disabled\n");
		return -1;
	}

	if (is_algo_active(gm) == true)
		do_fg_algo(gm, flow_state);
	else
		wakeup_fg_daemon(gm, flow_state, cmd, para1);

	return 0;
}

int wakeup_fg_algo(struct mtk_battery *gm, unsigned int flow_state)
{
	return wakeup_fg_algo_cmd(gm, flow_state, 0, 0);
}

bool is_recovery_mode(struct mtk_battery *gm)
{
	bm_err(gm, "%s, bootmdoe = %d\n", __func__, gm->bootmode);

	/* RECOVERY_BOOT */
	if (gm->bootmode == 2)
		return true;

	return false;
}

/* select gm->charge_power_sel to CHARGE_NORMAL ,CHARGE_R1,CHARGE_R2 */
/* example: gm->charge_power_sel = CHARGE_NORMAL */
bool set_charge_power_sel(struct mtk_battery *gm, enum charge_sel select)
{
	gm->charge_power_sel = select;

	wakeup_fg_algo_cmd(gm, FG_INTR_KERNEL_CMD,
		FG_KERNEL_CMD_FORCE_BAT_TEMP, select);

	return 0;
}


int dump_pseudo100(struct mtk_battery *gm, enum charge_sel select)
{
	int i = 0;

	bm_err(gm, "%s:select=%d\n", __func__, select);

	if (select >= MAX_CHARGE_RDC)
		return 0;

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err(gm, "%6d\n",
			gm->fg_table_cust_data.fg_profile[
				i].r_pseudo100.pseudo[select]);
	}

	return 0;
}

void reg_type_to_name(struct mtk_battery *gm, char *reg_type_name, unsigned int regmap_type)
{
	int ret = 0;

	switch (regmap_type) {
	case REGMAP_TYPE_I2C:
		ret = snprintf(reg_type_name, MAX_REGMAP_TYPE_LEN, "I2C");
		break;
	case REGMAP_TYPE_SPMI:
	case RGEMAP_TYPE_MMIO:
	case REGMAP_TYPE_SPI:
	default:
		ret = snprintf(reg_type_name, MAX_REGMAP_TYPE_LEN, "None_I2C_%d", regmap_type);
		break;
	}
	if (ret < 0)
		bm_err(gm, "[%s] something wrong %d\n", __func__, regmap_type);
}

void gp_number_to_name(struct mtk_battery *gm, char *gp_name, unsigned int gp_no)
{
	int ret = 0;

	switch (gp_no) {
	case GAUGE_PROP_INITIAL:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_INITIAL");
		break;

	case GAUGE_PROP_BATTERY_CURRENT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_CURRENT");
		break;

	case GAUGE_PROP_COULOMB:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_COULOMB");
		break;

	case GAUGE_PROP_COULOMB_HT_INTERRUPT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_COULOMB_HT_INTERRUPT");
		break;

	case GAUGE_PROP_COULOMB_LT_INTERRUPT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_COULOMB_LT_INTERRUPT");
		break;

	case GAUGE_PROP_BATTERY_EXIST:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_EXIST");
		break;

	case GAUGE_PROP_HW_VERSION:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_HW_VERSION");
		break;

	case GAUGE_PROP_BATTERY_VOLTAGE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_VOLTAGE");
		break;

	case GAUGE_PROP_BATTERY_TEMPERATURE_ADC:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_TEMPERATURE_ADC");
		break;

	case GAUGE_PROP_BIF_VOLTAGE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BIF_VOLTAGE");
		break;

	case GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT");
		break;

	case GAUGE_PROP_EN_LOW_VBAT_INTERRUPT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_LOW_VBAT_INTERRUPT");
		break;

	case GAUGE_PROP_VBAT_HT_INTR_THRESHOLD:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT_HT_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_VBAT_LT_INTR_THRESHOLD:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT_LT_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_RTC_UI_SOC:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_RTC_UI_SOC");
		break;

	case GAUGE_PROP_PTIM_BATTERY_VOLTAGE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_PTIM_BATTERY_VOLTAGE");
		break;

	case GAUGE_PROP_PTIM_RESIST:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_PTIM_RESIST");
		break;

	case GAUGE_PROP_RESET:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_RESET");
		break;

	case GAUGE_PROP_BOOT_ZCV:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BOOT_ZCV");
		break;

	case GAUGE_PROP_ZCV:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV");
		break;

	case GAUGE_PROP_ZCV_CURRENT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV_CURRENT");
		break;

	case GAUGE_PROP_NAFG_CNT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_CNT");
		break;

	case GAUGE_PROP_NAFG_DLTV:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_DLTV");
		break;

	case GAUGE_PROP_NAFG_C_DLTV:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_C_DLTV");
		break;

	case GAUGE_PROP_NAFG_EN:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_EN");
		break;

	case GAUGE_PROP_NAFG_ZCV:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_ZCV");
		break;

	case GAUGE_PROP_NAFG_VBAT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_VBAT");
		break;

	case GAUGE_PROP_RESET_FG_RTC:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_RESET_FG_RTC");
		break;

	case GAUGE_PROP_GAUGE_INITIALIZED:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_GAUGE_INITIALIZED");
		break;

	case GAUGE_PROP_AVERAGE_CURRENT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_AVERAGE_CURRENT");
		break;

	case GAUGE_PROP_BAT_PLUGOUT_EN:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_PLUGOUT_EN");
		break;

	case GAUGE_PROP_ZCV_INTR_THRESHOLD:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_ZCV_INTR_EN:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV_INTR_EN");
		break;

	case GAUGE_PROP_SOFF_RESET:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_SOFF_RESET");
		break;

	case GAUGE_PROP_NCAR_RESET:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NCAR_RESET");
		break;

	case GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_HW_INFO:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_HW_INFO");
		break;

	case GAUGE_PROP_EVENT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EVENT");
		break;

	case GAUGE_PROP_EN_BAT_TMP_HT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_BAT_TMP_HT");
		break;

	case GAUGE_PROP_EN_BAT_TMP_LT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_BAT_TMP_LT");
		break;

	case GAUGE_PROP_BAT_TMP_HT_THRESHOLD:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_TMP_HT_THRESHOLD");
		break;

	case GAUGE_PROP_BAT_TMP_LT_THRESHOLD:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_TMP_LT_THRESHOLD");
		break;

	case GAUGE_PROP_2SEC_REBOOT:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_2SEC_REBOOT");
		break;

	case GAUGE_PROP_PL_CHARGING_STATUS:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_PL_CHARGING_STATUS");
		break;

	case GAUGE_PROP_MONITER_PLCHG_STATUS:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_MONITER_PLCHG_STATUS");
		break;

	case GAUGE_PROP_BAT_PLUG_STATUS:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_PLUG_STATUS");
		break;

	case GAUGE_PROP_IS_NVRAM_FAIL_MODE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_IS_NVRAM_FAIL_MODE");
		break;

	case GAUGE_PROP_MONITOR_SOFF_VALIDTIME:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_MONITOR_SOFF_VALIDTIME");
		break;

	case GAUGE_PROP_CON0_SOC:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_CON0_SOC");
		break;

	case GAUGE_PROP_SHUTDOWN_CAR:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_SHUTDOWN_CAR");
		break;

	case GAUGE_PROP_CAR_TUNE_VALUE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_CAR_TUNE_VALUE");
		break;

	case GAUGE_PROP_R_FG_VALUE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_R_FG_VALUE");
		break;

	case GAUGE_PROP_VBAT2_DETECT_TIME:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT2_DETECT_TIME");
		break;

	case GAUGE_PROP_VBAT2_DETECT_COUNTER:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT2_DETECT_COUNTER");
		break;

	case GAUGE_PROP_BAT_TEMP_FROZE_EN:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_TEMP_FROZE_EN");
		break;

	case GAUGE_PROP_BAT_EOC:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_EOC");
		break;

	case GAUGE_PROP_REGMAP_TYPE:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_REGMAP_TYPE");
		break;

	case GAUGE_PROP_CIC2:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_CIC2");
		break;

	default:
		ret = snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"FG_PROP_UNKNOWN");
		break;
	}

	if (ret < 0)
		bm_err(gm, "[%s] something wrong %d\n", __func__, gp_no);
}

/* ============================================================ */
/* voltage to battery temperature */
/* ============================================================ */
int adc_battemp(struct mtk_battery *gm, int res)
{
	int i = 0;
	int res1 = 0, res2 = 0;
	int tbatt_value = -200, tmp1 = 0, tmp2 = 0;
	struct fg_temp *ptable;

	ptable = gm->tmp_table;
	if (res >= ptable[0].TemperatureR) {
		tbatt_value = -40;
	} else if (res <= ptable[20].TemperatureR) {
		tbatt_value = 60;
	} else {
		res1 = ptable[0].TemperatureR;
		tmp1 = ptable[0].BatteryTemp;

		for (i = 0; i <= 20; i++) {
			if (res >= ptable[i].TemperatureR) {
				res2 = ptable[i].TemperatureR;
				tmp2 = ptable[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				res1 = ptable[i].TemperatureR;
				tmp1 = ptable[i].BatteryTemp;
			}
		}

		tbatt_value = (((res - res2) * tmp1) +
			((res1 - res) * tmp2)) / (res1 - res2);
	}
	bm_debug(gm, "[%s] %d %d %d %d %d %d\n",
		__func__,
		res1, res2, res, tmp1,
		tmp2, tbatt_value);

	return tbatt_value;
}

int volttotemp(struct mtk_battery *gm, int dwVolt, int volt_cali)
{
	long long tres_temp;
	long long tres;
	int sbattmp = -100;
	int vbif28 = gm->rbat.rbat_pull_up_volt;
	int delta_v;
	int vbif28_raw = 0;
	int ret;

	tres_temp = (gm->rbat.rbat_pull_up_r * (long long) dwVolt);
	ret = gauge_get_property(gm, GAUGE_PROP_BIF_VOLTAGE,
		&vbif28_raw);

	if (ret != -ENOTSUPP) {
		vbif28 = vbif28_raw + volt_cali;
		delta_v = abs(vbif28 - dwVolt);
		if (delta_v == 0)
			delta_v = 1;
		tres_temp = div_s64(tres_temp, delta_v);
		if (vbif28 > 3000 || vbif28 < 1700)
			bm_debug(gm, "[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",
				vbif28_raw);
	} else {
		delta_v = abs(gm->rbat.rbat_pull_up_volt - dwVolt);
		if (delta_v == 0)
			delta_v = 1;
		tres_temp = div_s64(tres_temp, delta_v);
	}

#if IS_ENABLED(RBAT_PULL_DOWN_R)
	tres = (tres_temp * RBAT_PULL_DOWN_R);
	tres_temp = div_s64(tres, abs(RBAT_PULL_DOWN_R - tres_temp));

#else
	tres = tres_temp;
#endif
	sbattmp = adc_battemp(gm, (int)tres);

	bm_debug(gm, "[%s] %d %d %d %d\n",
		__func__,
		dwVolt, gm->rbat.rbat_pull_up_r,
		vbif28, volt_cali);
	return sbattmp;
}

int force_get_tbat_internal(struct mtk_battery *gm)
{
	int bat_temperature_volt = 2;
	int bat_temperature_val = 0;
	int fg_r_value = 0;
	int fg_meter_res_value = 0;
	int fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int orig_fg_current1 = 0, orig_fg_current2 = 0, orig_bat_temperature_volt = 0;
	int vol_cali = 0;
	ktime_t ctime = 0, dtime = 0, pre_time = 0;
	struct timespec64 tmp_time;
	int ret = 0;

	ret = gauge_get_property(gm, GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
		&bat_temperature_volt);

	if (ret == -EHOSTDOWN)
		return ret;

	gm->baton = bat_temperature_volt;

	if (bat_temperature_volt != 0) {
		fg_r_value = gm->fg_cust_data.com_r_fg_value;
		if (gm->no_bat_temp_compensate == 0)
			fg_meter_res_value =
			gm->fg_cust_data.com_fg_meter_resistance;
		else
			fg_meter_res_value = 0;

		gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
			&fg_current_temp, 0);

		gm->ibat = fg_current_temp;

		if (fg_current_temp > 0)
			fg_current_state = true;

		fg_current_temp = abs(fg_current_temp) / 10;

		if (fg_current_state == true) {
			bat_temperature_volt_temp =
				bat_temperature_volt;
			bat_temperature_volt =
			bat_temperature_volt -
			((fg_current_temp *
				(fg_meter_res_value + fg_r_value))
					/ 10000);
			vol_cali =
				-((fg_current_temp *
				(fg_meter_res_value + fg_r_value))
					/ 10000);
		} else {
			bat_temperature_volt_temp =
				bat_temperature_volt;
			bat_temperature_volt =
			bat_temperature_volt +
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
			vol_cali =
				((fg_current_temp *
				(fg_meter_res_value + fg_r_value))
				/ 10000);
		}

		bat_temperature_val =
			volttotemp(gm,
			bat_temperature_volt,
			vol_cali);
	}

	bm_notice(gm, "[%s] %d,%d,%d,%d,%d,%d r:%d %d %d\n",
		__func__,
		bat_temperature_volt_temp, bat_temperature_volt,
		fg_current_state, fg_current_temp,
		fg_r_value, bat_temperature_val,
		fg_meter_res_value, fg_r_value,
		gm->no_bat_temp_compensate);

	if (gm->pre_bat_temperature_val2 == 0) {
		gm->pre_bat_temperature_volt_temp =
			bat_temperature_volt_temp;
		gm->pre_bat_temperature_volt = bat_temperature_volt;
		gm->pre_fg_current_temp = fg_current_temp;
		gm->pre_fg_current_state = fg_current_state;
		gm->pre_fg_r_value = fg_r_value;
		gm->pre_bat_temperature_val2 = bat_temperature_val;
		pre_time = ktime_get_boottime();
	} else {
		ctime = ktime_get_boottime();
		dtime = ktime_sub(ctime, pre_time);
		tmp_time = ktime_to_timespec64(dtime);

		if ((tmp_time.tv_sec <= 20) &&
			(abs(gm->pre_bat_temperature_val2 -
			bat_temperature_val) >= 5)) {
			gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT, &orig_fg_current1, 0);
			gauge_get_property(gm, GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
				&orig_bat_temperature_volt);
			gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT, &orig_fg_current2, 0);
			orig_fg_current1 /= 10;
			orig_fg_current2 /= 10;
			bm_err(gm,
				"[%s][err] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d baton:%d ibat:%d,%d\n",
				__func__,
				bat_temperature_volt_temp,
				bat_temperature_volt,
				fg_current_state,
				fg_current_temp,
				fg_r_value,
				bat_temperature_val,
				gm->pre_bat_temperature_volt_temp,
				gm->pre_bat_temperature_volt,
				gm->pre_fg_current_state,
				gm->pre_fg_current_temp,
				gm->pre_fg_r_value,
				gm->pre_bat_temperature_val2,
				orig_bat_temperature_volt,
				orig_fg_current1,
				orig_fg_current2);
			/*pmic_auxadc_debug(1);*/
			WARN_ON(1);
		}

		bm_trace(gm,
			"[%s] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d time:%llu\n",
			__func__,
			bat_temperature_volt_temp, bat_temperature_volt,
			fg_current_state, fg_current_temp,
			fg_r_value, bat_temperature_val,
			gm->pre_bat_temperature_volt_temp,
			gm->pre_bat_temperature_volt,
			gm->pre_fg_current_state, gm->pre_fg_current_temp,
			gm->pre_fg_r_value,
			gm->pre_bat_temperature_val2, tmp_time.tv_sec);

		gm->pre_bat_temperature_volt_temp =
			bat_temperature_volt_temp;
		gm->pre_bat_temperature_volt = bat_temperature_volt;
		gm->pre_fg_current_temp = fg_current_temp;
		gm->pre_fg_current_state = fg_current_state;
		gm->pre_fg_r_value = fg_r_value;
		gm->pre_bat_temperature_val2 = bat_temperature_val;
		pre_time = ctime;

		tmp_time = ktime_to_timespec64(dtime);
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	gm->tbat_adc = bat_temperature_volt;
#endif
	return bat_temperature_val;
}

int force_get_tbat(struct mtk_battery *gm, bool update)
{
	struct property_control *prop_control;
	int prop_map = CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC;
	ktime_t ctime = 0, dtime = 0, diff = 0;
	int bat_temperature_val = 0;

	prop_control = &gm->prop_control;

	if (gm->is_probe_done == false) {
		gm->cur_bat_temp = 25;
		return 25;
	}

	if (gm->fixed_bat_tmp != 0xffff) {
		gm->cur_bat_temp = gm->fixed_bat_tmp;
		return gm->fixed_bat_tmp;
	}

	if (update == true || gm->no_prop_timeout_control) {
		bat_temperature_val = force_get_tbat_internal(gm);
		prop_control->val[prop_map] = bat_temperature_val;
		prop_control->last_prop_update_time[prop_map] = ktime_get_boottime();
	} else {
		ctime = ktime_get_boottime();
		dtime = ktime_sub(ctime, prop_control->last_prop_update_time[prop_map]);
		diff = ktime_to_ms(dtime);
		prop_control->binder_counter += 1;

		if (diff > prop_control->diff_time_th[prop_map]) {
			bat_temperature_val = force_get_tbat_internal(gm);
			prop_control->val[prop_map] = bat_temperature_val;
			prop_control->last_prop_update_time[prop_map] = ctime;
		} else {
			bat_temperature_val = prop_control->val[prop_map];
		}
	}

	if (bat_temperature_val == -EHOSTDOWN)
		return gm->cur_bat_temp;

	gm->cur_bat_temp = bat_temperature_val;

	return bat_temperature_val;
}

void fg_update_porp_control(struct property_control *prop_control)
{
	ktime_t ctime, diff;
	int i;

	prop_control->total_fail = 0;
	ctime = ktime_get_boottime();
	diff = ktime_sub(ctime, prop_control->pre_log_time);
	prop_control->last_period = ktime_to_timespec64(diff);
	if (prop_control->end_get_prop_time == 0) {
		diff = ktime_sub(ctime, prop_control->start_get_prop_time);
		prop_control->last_diff_time = ktime_to_timespec64(diff);
	} else
		prop_control->last_diff_time.tv_sec = 0;

	for (i = 0; i < GAUGE_PROP_MAX; i++)
		prop_control->total_fail += prop_control->i2c_fail_counter[i];

	prop_control->last_binder_counter = prop_control->binder_counter;
	prop_control->binder_counter = 0;
	prop_control->pre_log_time = ctime;
}
/* ============================================================ */
/* load .h/dtsi */
/* ============================================================ */

void fg_custom_init_from_header(struct mtk_battery *gm)
{
	int i, j;
	struct fuel_gauge_custom_data *fg_cust_data;
	struct fuel_gauge_table_custom_data *fg_table_cust_data;
	int version = 0, bat_id = 0;

	fg_cust_data = &gm->fg_cust_data;
	fg_table_cust_data = &gm->fg_table_cust_data;

	bat_id = fgauge_get_profile_id(gm);
	bm_debug(gm, "[%s] init form bat id %d\n", __func__, bat_id);

	fg_cust_data->versionID1 = FG_DAEMON_CMD_FROM_USER_NUMBER;
	fg_cust_data->versionID2 = sizeof(gm->fg_cust_data);
	fg_cust_data->versionID3 = FG_KERNEL_CMD_FROM_USER_NUMBER;

	if (gm->gauge != NULL) {
		gauge_get_property(gm, GAUGE_PROP_HW_VERSION, &version);
		fg_cust_data->hardwareVersion = version;
		fg_cust_data->pl_charger_status =
			gm->gauge->hw_status.pl_charger_status;
	}

	fg_cust_data->q_max_L_current = Q_MAX_L_CURRENT;
	fg_cust_data->q_max_H_current = Q_MAX_H_CURRENT;
	fg_cust_data->q_max_sys_voltage =
		UNIT_TRANS_10 * g_Q_MAX_SYS_VOLTAGE[gm->battery_id];

	fg_cust_data->pseudo1_en = PSEUDO1_EN;
	fg_cust_data->pseudo100_en = PSEUDO100_EN;
	fg_cust_data->pseudo100_en_dis = PSEUDO100_EN_DIS;
	fg_cust_data->pseudo1_iq_offset = UNIT_TRANS_100 *
		g_FG_PSEUDO1_OFFSET[gm->battery_id];

	/* iboot related */
	fg_cust_data->qmax_sel = QMAX_SEL;
	fg_cust_data->iboot_sel = IBOOT_SEL;
	fg_cust_data->shutdown_system_iboot = SHUTDOWN_SYSTEM_IBOOT;

	/* multi-temp gague 0% related */
	fg_cust_data->multi_temp_gauge0 = MULTI_TEMP_GAUGE0;

	/*hw related */
	fg_cust_data->car_tune_value = UNIT_TRANS_10 * CAR_TUNE_VALUE;
	fg_cust_data->fg_meter_resistance = FG_METER_RESISTANCE;
	fg_cust_data->com_fg_meter_resistance = FG_METER_RESISTANCE;
	fg_cust_data->r_fg_value = UNIT_TRANS_10 * R_FG_VALUE;
	fg_cust_data->com_r_fg_value = UNIT_TRANS_10 * R_FG_VALUE;
	fg_cust_data->unit_multiple = UNIT_MULTIPLE;

	/* Dynamic CV */
	fg_cust_data->dynamic_cv_factor = DYNAMIC_CV_FACTOR;
	fg_cust_data->charger_ieoc = CHARGER_IEOC;

	/* Aging Compensation */
	fg_cust_data->aging_one_en = AGING_ONE_EN;
	fg_cust_data->aging1_update_soc = UNIT_TRANS_100 * AGING1_UPDATE_SOC;
	fg_cust_data->aging1_load_soc = UNIT_TRANS_100 * AGING1_LOAD_SOC;
	fg_cust_data->aging4_update_soc = UNIT_TRANS_100 * AGING4_UPDATE_SOC;
	fg_cust_data->aging4_load_soc = UNIT_TRANS_100 * AGING4_LOAD_SOC;
	fg_cust_data->aging5_update_soc = UNIT_TRANS_100 * AGING5_UPDATE_SOC;
	fg_cust_data->aging5_load_soc = UNIT_TRANS_100 * AGING5_LOAD_SOC;
	fg_cust_data->aging6_update_soc = UNIT_TRANS_100 * AGING6_UPDATE_SOC;
	fg_cust_data->aging6_load_soc = UNIT_TRANS_100 * AGING6_LOAD_SOC;
	fg_cust_data->aging_temp_diff = AGING_TEMP_DIFF;
	fg_cust_data->aging_temp_low_limit = AGING_TEMP_LOW_LIMIT;
	fg_cust_data->aging_temp_high_limit = AGING_TEMP_HIGH_LIMIT;
	fg_cust_data->aging_100_en = AGING_100_EN;
	fg_cust_data->difference_voltage_update = DIFFERENCE_VOLTAGE_UPDATE;
	fg_cust_data->aging_factor_min = UNIT_TRANS_100 * AGING_FACTOR_MIN;
	fg_cust_data->aging_factor_diff = UNIT_TRANS_100 * AGING_FACTOR_DIFF;
	/* Aging Compensation 2*/
	fg_cust_data->aging_two_en = AGING_TWO_EN;
	/* Aging Compensation 3*/
	fg_cust_data->aging_third_en = AGING_THIRD_EN;
	fg_cust_data->aging_4_en = AGING_4_EN;
	fg_cust_data->aging_5_en = AGING_5_EN;
	fg_cust_data->aging_6_en = AGING_6_EN;

	/* ui_soc related */
	fg_cust_data->diff_soc_setting = DIFF_SOC_SETTING;
	fg_cust_data->keep_100_percent = UNIT_TRANS_100 * KEEP_100_PERCENT;
	fg_cust_data->difference_full_cv = DIFFERENCE_FULL_CV;
	fg_cust_data->diff_bat_temp_setting = DIFF_BAT_TEMP_SETTING;
	fg_cust_data->diff_bat_temp_setting_c = DIFF_BAT_TEMP_SETTING_C;
	fg_cust_data->discharge_tracking_time = DISCHARGE_TRACKING_TIME;
	fg_cust_data->charge_tracking_time = CHARGE_TRACKING_TIME;
	fg_cust_data->difference_fullocv_vth = DIFFERENCE_FULLOCV_VTH;
	fg_cust_data->difference_fullocv_ith =
		UNIT_TRANS_10 * DIFFERENCE_FULLOCV_ITH;
	fg_cust_data->charge_pseudo_full_level = CHARGE_PSEUDO_FULL_LEVEL;
	fg_cust_data->over_discharge_level = OVER_DISCHARGE_LEVEL;
	fg_cust_data->full_tracking_bat_int2_multiply =
		FULL_TRACKING_BAT_INT2_MULTIPLY;

	/* pre tracking */
	fg_cust_data->fg_pre_tracking_en = FG_PRE_TRACKING_EN;
	fg_cust_data->vbat2_det_time = VBAT2_DET_TIME;
	fg_cust_data->vbat2_det_counter = VBAT2_DET_COUNTER;
	fg_cust_data->vbat2_det_voltage1 = VBAT2_DET_VOLTAGE1;
	fg_cust_data->vbat2_det_voltage2 = VBAT2_DET_VOLTAGE2;
	fg_cust_data->vbat2_det_voltage3 = VBAT2_DET_VOLTAGE3;

	/* sw fg */
	fg_cust_data->difference_fgc_fgv_th1 = DIFFERENCE_FGC_FGV_TH1;
	fg_cust_data->difference_fgc_fgv_th2 = DIFFERENCE_FGC_FGV_TH2;
	fg_cust_data->difference_fgc_fgv_th3 = DIFFERENCE_FGC_FGV_TH3;
	fg_cust_data->difference_fgc_fgv_th_soc1 = DIFFERENCE_FGC_FGV_TH_SOC1;
	fg_cust_data->difference_fgc_fgv_th_soc2 = DIFFERENCE_FGC_FGV_TH_SOC2;
	fg_cust_data->nafg_time_setting = NAFG_TIME_SETTING;
	fg_cust_data->nafg_ratio = NAFG_RATIO;
	fg_cust_data->nafg_ratio_en = NAFG_RATIO_EN;
	fg_cust_data->nafg_ratio_tmp_thr = NAFG_RATIO_TMP_THR;
	fg_cust_data->nafg_resistance = NAFG_RESISTANCE;

	/* ADC resistor  */
	fg_cust_data->r_charger_1 = R_CHARGER_1;
	fg_cust_data->r_charger_2 = R_CHARGER_2;

	/* mode select */
	fg_cust_data->pmic_shutdown_current = PMIC_SHUTDOWN_CURRENT;
	fg_cust_data->pmic_shutdown_sw_en = PMIC_SHUTDOWN_SW_EN;
	fg_cust_data->force_vc_mode = FORCE_VC_MODE;
	fg_cust_data->embedded_sel = EMBEDDED_SEL;
	fg_cust_data->loading_1_en = LOADING_1_EN;
	fg_cust_data->loading_2_en = LOADING_2_EN;
	fg_cust_data->diff_iavg_th = DIFF_IAVG_TH;

	fg_cust_data->shutdown_gauge0 = SHUTDOWN_GAUGE0;
	fg_cust_data->shutdown_1_time = SHUTDOWN_1_TIME;
	fg_cust_data->shutdown_gauge1_xmins = SHUTDOWN_GAUGE1_XMINS;
	fg_cust_data->shutdown_gauge0_voltage = SHUTDOWN_GAUGE0_VOLTAGE;
	fg_cust_data->shutdown_gauge1_vbat_en = SHUTDOWN_GAUGE1_VBAT_EN;
	fg_cust_data->shutdown_gauge1_vbat = SHUTDOWN_GAUGE1_VBAT;
	fg_cust_data->power_on_car_chr = POWER_ON_CAR_CHR;
	fg_cust_data->power_on_car_nochr = POWER_ON_CAR_NOCHR;
	fg_cust_data->shutdown_car_ratio = SHUTDOWN_CAR_RATIO;

	/* log level*/
#if IS_ENABLED(CONFIG_MTK_CHGLOG_SUPPORT)
	fg_cust_data->daemon_log_level = BMLOG_DEBUG_LEVEL;
#else
	fg_cust_data->daemon_log_level = BMLOG_ERROR_LEVEL;
#endif

	/* ZCV update */
	fg_cust_data->zcv_suspend_time = ZCV_SUSPEND_TIME;
	fg_cust_data->sleep_current_avg = SLEEP_CURRENT_AVG;
	fg_cust_data->zcv_com_vol_limit = ZCV_COM_VOL_LIMIT;
	fg_cust_data->zcv_car_gap_percentage = ZCV_CAR_GAP_PERCENTAGE;

	/* dod_init */
	fg_cust_data->hwocv_oldocv_diff = HWOCV_OLDOCV_DIFF;
	fg_cust_data->hwocv_oldocv_diff_chr = HWOCV_OLDOCV_DIFF_CHR;
	fg_cust_data->hwocv_swocv_diff = HWOCV_SWOCV_DIFF;
	fg_cust_data->hwocv_swocv_diff_lt = HWOCV_SWOCV_DIFF_LT;
	fg_cust_data->hwocv_swocv_diff_lt_temp = HWOCV_SWOCV_DIFF_LT_TEMP;
	fg_cust_data->swocv_oldocv_diff = SWOCV_OLDOCV_DIFF;
	fg_cust_data->swocv_oldocv_diff_chr = SWOCV_OLDOCV_DIFF_CHR;
	fg_cust_data->vbat_oldocv_diff = VBAT_OLDOCV_DIFF;
	fg_cust_data->swocv_oldocv_diff_emb = SWOCV_OLDOCV_DIFF_EMB;
	fg_cust_data->vir_oldocv_diff_emb = VIR_OLDOCV_DIFF_EMB;
	fg_cust_data->vir_oldocv_diff_emb_lt = VIR_OLDOCV_DIFF_EMB_LT;
	fg_cust_data->vir_oldocv_diff_emb_tmp = VIR_OLDOCV_DIFF_EMB_TMP;

	fg_cust_data->pmic_shutdown_time = UNIT_TRANS_60 * PMIC_SHUTDOWN_TIME;
	fg_cust_data->tnew_told_pon_diff = TNEW_TOLD_PON_DIFF;
	fg_cust_data->tnew_told_pon_diff2 = TNEW_TOLD_PON_DIFF2;
	gm->ext_hwocv_swocv = EXT_HWOCV_SWOCV;
	gm->ext_hwocv_swocv_lt = EXT_HWOCV_SWOCV_LT;
	gm->ext_hwocv_swocv_lt_temp = EXT_HWOCV_SWOCV_LT_TEMP;

	gm->no_prop_timeout_control = NO_PROP_TIMEOUT_CONTROL;

	gm->bat_voltage_low_bound = BAT_VOLTAGE_LOW_BOUND;
	gm->low_tmp_bat_voltage_low_bound = LOW_TMP_BAT_VOLTAGE_LOW_BOUND;

	fg_cust_data->dc_ratio_sel = DC_RATIO_SEL;
	fg_cust_data->dc_r_cnt = DC_R_CNT;

	fg_cust_data->pseudo1_sel = PSEUDO1_SEL;

	fg_cust_data->d0_sel = D0_SEL;
	fg_cust_data->dlpt_ui_remap_en = DLPT_UI_REMAP_EN;

	fg_cust_data->aging_sel = AGING_SEL;
	fg_cust_data->bat_par_i = BAT_PAR_I;

	fg_cust_data->fg_tracking_current = FG_TRACKING_CURRENT;
	fg_cust_data->fg_tracking_current_iboot_en =
		FG_TRACKING_CURRENT_IBOOT_EN;
	fg_cust_data->ui_fast_tracking_en = UI_FAST_TRACKING_EN;
	fg_cust_data->ui_fast_tracking_gap = UI_FAST_TRACKING_GAP;

	fg_cust_data->bat_plug_out_time = BAT_PLUG_OUT_TIME;
	fg_cust_data->keep_100_percent_minsoc = KEEP_100_PERCENT_MINSOC;

	fg_cust_data->uisoc_update_type = UISOC_UPDATE_TYPE;

	fg_cust_data->battery_tmp_to_disable_gm30 = BATTERY_TMP_TO_DISABLE_GM30;
	fg_cust_data->battery_tmp_to_disable_nafg = BATTERY_TMP_TO_DISABLE_NAFG;
	fg_cust_data->battery_tmp_to_enable_nafg = BATTERY_TMP_TO_ENABLE_NAFG;

	fg_cust_data->low_temp_mode = LOW_TEMP_MODE;
	fg_cust_data->low_temp_mode_temp = LOW_TEMP_MODE_TEMP;

	/* current limit for uisoc 100% */
	fg_cust_data->ui_full_limit_en = UI_FULL_LIMIT_EN;
	fg_cust_data->ui_full_limit_soc0 = UI_FULL_LIMIT_SOC0;
	fg_cust_data->ui_full_limit_ith0 = UI_FULL_LIMIT_ITH0;
	fg_cust_data->ui_full_limit_soc1 = UI_FULL_LIMIT_SOC1;
	fg_cust_data->ui_full_limit_ith1 = UI_FULL_LIMIT_ITH1;
	fg_cust_data->ui_full_limit_soc2 = UI_FULL_LIMIT_SOC2;
	fg_cust_data->ui_full_limit_ith2 = UI_FULL_LIMIT_ITH2;
	fg_cust_data->ui_full_limit_soc3 = UI_FULL_LIMIT_SOC3;
	fg_cust_data->ui_full_limit_ith3 = UI_FULL_LIMIT_ITH3;
	fg_cust_data->ui_full_limit_soc4 = UI_FULL_LIMIT_SOC4;
	fg_cust_data->ui_full_limit_ith4 = UI_FULL_LIMIT_ITH4;
	fg_cust_data->ui_full_limit_time = UI_FULL_LIMIT_TIME;

	fg_cust_data->ui_full_limit_fc_soc0 = UI_FULL_LIMIT_FC_SOC0;
	fg_cust_data->ui_full_limit_fc_ith0 = UI_FULL_LIMIT_FC_ITH0;
	fg_cust_data->ui_full_limit_fc_soc1 = UI_FULL_LIMIT_FC_SOC1;
	fg_cust_data->ui_full_limit_fc_ith1 = UI_FULL_LIMIT_FC_ITH1;
	fg_cust_data->ui_full_limit_fc_soc2 = UI_FULL_LIMIT_FC_SOC2;
	fg_cust_data->ui_full_limit_fc_ith2 = UI_FULL_LIMIT_FC_ITH2;
	fg_cust_data->ui_full_limit_fc_soc3 = UI_FULL_LIMIT_FC_SOC3;
	fg_cust_data->ui_full_limit_fc_ith3 = UI_FULL_LIMIT_FC_ITH3;
	fg_cust_data->ui_full_limit_fc_soc4 = UI_FULL_LIMIT_FC_SOC4;
	fg_cust_data->ui_full_limit_fc_ith4 = UI_FULL_LIMIT_FC_ITH4;

	/* voltage limit for uisoc 1% */
	fg_cust_data->ui_low_limit_en = UI_LOW_LIMIT_EN;
	fg_cust_data->ui_low_limit_soc0 = UI_LOW_LIMIT_SOC0;
	fg_cust_data->ui_low_limit_vth0 = UI_LOW_LIMIT_VTH0;
	fg_cust_data->ui_low_limit_soc1 = UI_LOW_LIMIT_SOC1;
	fg_cust_data->ui_low_limit_vth1 = UI_LOW_LIMIT_VTH1;
	fg_cust_data->ui_low_limit_soc2 = UI_LOW_LIMIT_SOC2;
	fg_cust_data->ui_low_limit_vth2 = UI_LOW_LIMIT_VTH2;
	fg_cust_data->ui_low_limit_soc3 = UI_LOW_LIMIT_SOC3;
	fg_cust_data->ui_low_limit_vth3 = UI_LOW_LIMIT_VTH3;
	fg_cust_data->ui_low_limit_soc4 = UI_LOW_LIMIT_SOC4;
	fg_cust_data->ui_low_limit_vth4 = UI_LOW_LIMIT_VTH4;
	fg_cust_data->ui_low_limit_time = UI_LOW_LIMIT_TIME;

	fg_cust_data->moving_battemp_en = MOVING_BATTEMP_EN;
	fg_cust_data->moving_battemp_thr = MOVING_BATTEMP_THR;

	gm->no_prop_timeout_control = NO_PROP_TIMEOUT_CONTROL;

	gm->avgvbat_array_size = AVGVBAT_ARRAY_SIZE;
	/* battery healthd */
	fg_cust_data->bat_bh_en = BAT_BH_EN;
	fg_cust_data->aging_diff_max_threshold = AGING_DIFF_MAX_THRESHOLD;
	fg_cust_data->aging_diff_max_level = AGING_DIFF_MAX_LEVEL;
	fg_cust_data->aging_factor_t_min = AGING_FACTOR_T_MIN;
	fg_cust_data->cycle_diff = CYCLE_DIFF;
	fg_cust_data->aging_count_min = AGING_COUNT_MIN;
	fg_cust_data->default_score = DEFAULT_SCORE;
	fg_cust_data->default_score_quantity = DEFAULT_SCORE_QUANTITY;
	fg_cust_data->fast_cycle_set = FAST_CYCLE_SET;
	fg_cust_data->level_max_change_bat = LEVEL_MAX_CHANGE_BAT;
	fg_cust_data->diff_max_change_bat = DIFF_MAX_CHANGE_BAT;
	fg_cust_data->aging_tracking_start = AGING_TRACKING_START;
	fg_cust_data->max_aging_data = MAX_AGING_DATA;
	fg_cust_data->max_fast_data = MAX_FAST_DATA;
	fg_cust_data->fast_data_threshold_score = FAST_DATA_THRESHOLD_SCORE;
	fg_cust_data->show_aging_period = SHOW_AGING_PERIOD;
	gm->daemon_version = 0;

	if (version == GAUGE_HW_V2001) {
		bm_debug(gm, "GAUGE_HW_V2001 disable nafg\n");
		fg_cust_data->disable_nafg = 1;
	}

	fg_table_cust_data->active_table_number = ACTIVE_TABLE;

	if (fg_table_cust_data->active_table_number == 0)
		fg_table_cust_data->active_table_number = 5;

	bm_debug(gm, "fg active table:%d\n",
		fg_table_cust_data->active_table_number);

	fg_table_cust_data->enable_r_ratio = ENABLE_R_RATIO;
	fg_table_cust_data->max_ratio_temp = MAX_RATIO_TEMP;
	fg_table_cust_data->min_ratio_temp = MIN_RATIO_TEMP;
	fg_table_cust_data->enable_charging_ratio = ENABLE_R_RATIO;
	fg_table_cust_data->max_charging_ratio_temp = MAX_RATIO_TEMP;
	fg_table_cust_data->min_charging_ratio_temp = MIN_RATIO_TEMP;

	fg_table_cust_data->temperature_tb0 = TEMPERATURE_TB0;
	fg_table_cust_data->temperature_tb1 = TEMPERATURE_TB1;

	/* shutdown jumping*/
	fg_cust_data->low_tracking_jump = LOW_TRACKING_JUMP;
	fg_cust_data->pre_tracking_jump = PRE_TRACKING_JUMP;
	fg_cust_data->last_mode_reset = LAST_MODE_RESET;
	fg_cust_data->pre_tracking_soc_reset = PRE_TRACKING_SOC_RESET;

	fg_table_cust_data->fg_profile[0].size =
		sizeof(fg_profile_t0[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[0].fg_profile,
			&fg_profile_t0[gm->battery_id],
			sizeof(fg_profile_t0[gm->battery_id]));

	fg_table_cust_data->fg_profile[1].size =
		sizeof(fg_profile_t1[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[1].fg_profile,
			&fg_profile_t1[gm->battery_id],
			sizeof(fg_profile_t1[gm->battery_id]));

	fg_table_cust_data->fg_profile[2].size =
		sizeof(fg_profile_t2[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[2].fg_profile,
			&fg_profile_t2[gm->battery_id],
			sizeof(fg_profile_t2[gm->battery_id]));

	fg_table_cust_data->fg_profile[3].size =
		sizeof(fg_profile_t3[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[3].fg_profile,
			&fg_profile_t3[gm->battery_id],
			sizeof(fg_profile_t3[gm->battery_id]));

	fg_table_cust_data->fg_profile[4].size =
		sizeof(fg_profile_t4[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[4].fg_profile,
			&fg_profile_t4[gm->battery_id],
			sizeof(fg_profile_t4[gm->battery_id]));

	fg_table_cust_data->fg_profile[5].size =
		sizeof(fg_profile_t5[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[5].fg_profile,
			&fg_profile_t5[gm->battery_id],
			sizeof(fg_profile_t5[gm->battery_id]));

	fg_table_cust_data->fg_profile[6].size =
		sizeof(fg_profile_t6[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[6].fg_profile,
			&fg_profile_t6[gm->battery_id],
			sizeof(fg_profile_t6[gm->battery_id]));

	fg_table_cust_data->fg_profile[7].size =
		sizeof(fg_profile_t7[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[7].fg_profile,
			&fg_profile_t7[gm->battery_id],
			sizeof(fg_profile_t7[gm->battery_id]));

	fg_table_cust_data->fg_profile[8].size =
		sizeof(fg_profile_t8[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[8].fg_profile,
			&fg_profile_t8[gm->battery_id],
			sizeof(fg_profile_t8[gm->battery_id]));

	fg_table_cust_data->fg_profile[9].size =
		sizeof(fg_profile_t9[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[9].fg_profile,
			&fg_profile_t9[gm->battery_id],
			sizeof(fg_profile_t9[gm->battery_id]));

	memcpy(&fg_table_cust_data->fg_profile[0].fg_r_profile,
			&fg_r_profile_t0[gm->battery_id],
			sizeof(fg_r_profile_t0[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[1].fg_r_profile,
			&fg_r_profile_t1[gm->battery_id],
			sizeof(fg_r_profile_t1[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[2].fg_r_profile,
			&fg_r_profile_t2[gm->battery_id],
			sizeof(fg_r_profile_t2[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[3].fg_r_profile,
			&fg_r_profile_t3[gm->battery_id],
			sizeof(fg_r_profile_t3[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[4].fg_r_profile,
			&fg_r_profile_t4[gm->battery_id],
			sizeof(fg_r_profile_t4[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[5].fg_r_profile,
			&fg_r_profile_t5[gm->battery_id],
			sizeof(fg_r_profile_t5[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[6].fg_r_profile,
			&fg_r_profile_t6[gm->battery_id],
			sizeof(fg_r_profile_t6[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[7].fg_r_profile,
			&fg_r_profile_t7[gm->battery_id],
			sizeof(fg_r_profile_t7[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[8].fg_r_profile,
			&fg_r_profile_t8[gm->battery_id],
			sizeof(fg_r_profile_t8[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[9].fg_r_profile,
			&fg_r_profile_t9[gm->battery_id],
			sizeof(fg_r_profile_t9[gm->battery_id]));

	memcpy(&fg_table_cust_data->fg_profile[0].fg_charging_r_profile,
			&fg_r_profile_t0[gm->battery_id],
			sizeof(fg_r_profile_t0[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[1].fg_charging_r_profile,
			&fg_r_profile_t1[gm->battery_id],
			sizeof(fg_r_profile_t1[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[2].fg_charging_r_profile,
			&fg_r_profile_t2[gm->battery_id],
			sizeof(fg_r_profile_t2[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[3].fg_charging_r_profile,
			&fg_r_profile_t3[gm->battery_id],
			sizeof(fg_r_profile_t3[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[4].fg_charging_r_profile,
			&fg_r_profile_t4[gm->battery_id],
			sizeof(fg_r_profile_t4[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[5].fg_charging_r_profile,
			&fg_r_profile_t5[gm->battery_id],
			sizeof(fg_r_profile_t5[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[6].fg_charging_r_profile,
			&fg_r_profile_t6[gm->battery_id],
			sizeof(fg_r_profile_t6[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[7].fg_charging_r_profile,
			&fg_r_profile_t7[gm->battery_id],
			sizeof(fg_r_profile_t7[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[8].fg_charging_r_profile,
			&fg_r_profile_t8[gm->battery_id],
			sizeof(fg_r_profile_t8[gm->battery_id]));
	memcpy(&fg_table_cust_data->fg_profile[9].fg_charging_r_profile,
			&fg_r_profile_t9[gm->battery_id],
			sizeof(fg_r_profile_t9[gm->battery_id]));

	for (i = 0; i < MAX_TABLE; i++) {
		struct fuelgauge_profile_struct *p;

		p = &fg_table_cust_data->fg_profile[i].fg_profile[0];
		fg_table_cust_data->fg_profile[i].temperature =
			g_temperature[i];
		fg_table_cust_data->fg_profile[i].q_max =
			g_Q_MAX[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].q_max_h_current =
			g_Q_MAX_H_CURRENT[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pseudo1 =
			UNIT_TRANS_100 * g_FG_PSEUDO1[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pseudo100 =
			UNIT_TRANS_100 * g_FG_PSEUDO100[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pmic_min_vol =
			g_PMIC_MIN_VOL[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pon_iboot =
			g_PON_SYS_IBOOT[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].qmax_sys_vol =
			g_QMAX_SYS_VOL[i][gm->battery_id];
		/* shutdown_hl_zcv */
		fg_table_cust_data->fg_profile[i].shutdown_hl_zcv =
			g_SHUTDOWN_HL_ZCV[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].r_ratio_active_table =
			ACTIVE_R_TABLE;
		fg_table_cust_data->fg_profile[i].charging_r_ratio_active_table =
			ACTIVE_R_TABLE;
		gm->iavg_th[i] = g_iavg_th[i][gm->battery_id];

		for (j = 0; j < 100; j++)
			if (p[j].charge_r.rdc[0] == 0)
				p[j].charge_r.rdc[0] = p[j].resistance;
	}

	/* init battery temperature table */
	gm->rbat.type = 10;
	gm->rbat.rbat_pull_up_r = RBAT_PULL_UP_R;
	gm->rbat.rbat_pull_up_volt = RBAT_PULL_UP_VOLT;
	gm->rbat.bif_ntc_r = BIF_NTC_R;

	if (IS_ENABLED(BAT_NTC_47)) {
		gm->rbat.type = 47;
		gm->rbat.rbat_pull_up_r = RBAT_PULL_UP_R;
	}
}

void fg_convert_prop_tolower(char *s)
{
	int loop = 0;

	while (*s && loop <= MAX_PROP_NAME_LEN) {
		if (*s == '_')
			*s = '-';
		else
			*s = tolower(*s);
		loop++;
		s++;
	}
}

#if IS_ENABLED(CONFIG_OF)
static int fg_read_dts_val(struct mtk_battery *gm, const struct device_node *np,
		const char *node_srting,
		int *param, int unit)
{
	unsigned int val;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);
	if (temp == NULL)
		return -1;

	strncpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);
	if (!of_property_read_u32(np, temp, &val)) {
		*param = (int)val * unit;
		bm_debug(gm, "Get %s: %d\n",
			 temp, *param);
	} else if (!of_property_read_u32(np, node_srting, &val)) {
		*param = (int)val * unit;
		bm_debug(gm, "Get %s: %d\n",
			 node_srting, *param);
	} else {
		bm_debug(gm, "Get %s %s no data\n", temp, node_srting);
		kvfree(temp);
		return -1;
	}
	kvfree(temp);
	return 0;
}

static int fg_read_dts_val_by_idx(struct mtk_battery *gm, const struct device_node *np,
		const char *node_srting,
		int idx, int *param, int unit)
{
	unsigned int val = 0;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);
	if (temp == NULL)
		return -1;

	strncpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);
	if (!of_property_read_u32_index(np, temp, idx, &val)) {
		*param = (int)val * unit;
		bm_debug(gm, "Get %s %d: %d\n",
			 temp, idx, *param);
	} else if (!of_property_read_u32_index(np, node_srting, idx, &val)) {
		*param = (int)val * unit;
		bm_debug(gm, "Get %s %d: %d\n",
			 node_srting, idx, *param);
	}  else {
		bm_debug(gm, "Get %s %s no data, idx %d\n", node_srting, temp, idx);
		kvfree(temp);
		return -1;
	}
	kvfree(temp);
	return 0;
}

static void fg_custom_parse_ratio_table(struct mtk_battery *gm,
		const struct device_node *np,
		const char *node_srting,
		struct fuel_gauge_r_ratio_table *profile_struct, int column, int saddles)
{
	int battery_r_ratio_ma = 0, battery_r_ratio = 1000;
	int idx;
	struct fuel_gauge_r_ratio_table *profile_p;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);

	if (temp == NULL)
		return;

	idx = 0;
	strscpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);

	if (!of_property_read_u32_index(np, temp, idx, &battery_r_ratio_ma))
		node_srting = temp;

	profile_p = profile_struct;

	idx = 0;

	bm_debug(gm, "%s: %s, %d, column:%d\n",
		__func__,
		node_srting, saddles, column);

	while (!of_property_read_u32_index(np, node_srting, idx, &battery_r_ratio_ma)) {
		idx++;
		if (!of_property_read_u32_index(
			np, node_srting, idx, &battery_r_ratio)) {
		}
		idx++;

		bm_debug(gm, "%s: battery_r_ratio_ma: %d, battery_r_ratio: %d\n",
			__func__, battery_r_ratio_ma, battery_r_ratio);

		profile_p->battery_r_ratio_ma = battery_r_ratio_ma;
		profile_p->battery_r_ratio = battery_r_ratio;
		profile_p++;

		if (idx >= (saddles * column))
			break;
	}

	if (idx == 0) {
		bm_err(gm, "[%s] cannot find %s in dts\n",
			__func__, node_srting);
		kvfree(temp);
		return;
	}

	kvfree(temp);
}

static void fg_custom_parse_table(struct mtk_battery *gm,
		const struct device_node *np,
		const char *node_srting,
		struct fuelgauge_profile_struct *profile_struct, int column)
{
	int mah = 0, voltage = 0, resistance = 0, idx, saddles;
	int i = 0, charge_rdc[MAX_CHARGE_RDC] = {0};
	struct fuelgauge_profile_struct *profile_p;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);
	if (temp == NULL)
		return;

	idx = 0;
	strncpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);

	if (!of_property_read_u32_index(np, temp, idx, &mah))
		node_srting = temp;

	profile_p = profile_struct;

	saddles = gm->fg_table_cust_data.fg_profile[0].size;
	idx = 0;

	bm_debug(gm, "%s: %s, %d, column:%d\n",
		__func__,
		node_srting, saddles, column);

	while (!of_property_read_u32_index(np, node_srting, idx, &mah)) {
		idx++;
		if (!of_property_read_u32_index(
			np, node_srting, idx, &voltage)) {
		}
		idx++;
		if (!of_property_read_u32_index(
				np, node_srting, idx, &resistance)) {
		}
		idx++;

		if (column == 3) {
			for (i = 0; i < MAX_CHARGE_RDC; i++)
				charge_rdc[i] = resistance;
		} else if (column >= 4) {
			if (!of_property_read_u32_index(
				np, node_srting, idx, &charge_rdc[0]))
				idx++;
		}

		/* read more for column >4 case */
		if (column > 4) {
			for (i = 1; i <= column - 4; i++) {
				if (!of_property_read_u32_index(
					np, node_srting, idx, &charge_rdc[i]))
					idx++;
			}
		}

		bm_debug(gm, "%s: mah: %d, voltage: %d, resistance: %d, rdc0:%d rdc:%d %d %d %d\n",
			__func__, mah, voltage, resistance, charge_rdc[0],
			charge_rdc[1], charge_rdc[2], charge_rdc[3],
			charge_rdc[4]);

		profile_p->mah = mah;
		profile_p->voltage = voltage;
		profile_p->resistance = resistance;

		for (i = 0; i < MAX_CHARGE_RDC; i++)
			profile_p->charge_r.rdc[i] = charge_rdc[i];

		profile_p++;

		if (idx >= (saddles * column))
			break;
	}

	if (idx == 0) {
		bm_err(gm, "[%s] cannot find %s in dts\n",
			__func__, node_srting);
		kvfree(temp);
		return;
	}

	profile_p--;

	while (idx < (100 * column)) {
		profile_p++;
		profile_p->mah = mah;
		profile_p->voltage = voltage;
		profile_p->resistance = resistance;

		for (i = 0; i < MAX_CHARGE_RDC; i++)
			profile_p->charge_r.rdc[i] = charge_rdc[i];

		idx = idx + column;
	}
	kvfree(temp);
}

void reload_battery_zcv_table(
	struct mtk_battery *gm, int select_zcv)
{
	int ret = 0, column = 0, i;
	char node_name[128];
	int bat_id;
	struct device_node *np = gm->gauge->pdev->dev.of_node;
	struct fuel_gauge_table_custom_data *fg_table_cust_data;

	bm_err(gm, "[%s] reload_ZCV by select_zcv %d\n", __func__, select_zcv);
	fg_table_cust_data = &gm->fg_table_cust_data;
	bat_id = fgauge_get_profile_id(gm);
	//reload ZCV TABLE BY VAL
	for (i = 0; i < fg_table_cust_data->active_table_number; i++) {
		if (select_zcv == 0)
			ret = sprintf(node_name, "battery%d_profile_t%d_num", bat_id, i);
		else
			ret = sprintf(node_name, "battery%d_profile_t%d_cycle%d_num", bat_id, i, select_zcv);
		if (ret >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_table_cust_data->fg_profile[i].size), 1);

		/* compatiable with old dtsi table*/
		if (select_zcv == 0)
			ret = sprintf(node_name, "battery%d_profile_t%d_col", bat_id, i);
		else
			ret = sprintf(node_name, "battery%d_profile_t%d_cycle%d_col", bat_id, i, select_zcv);
		if (ret >= 0) {
			ret = fg_read_dts_val(gm, np, node_name, &(column), 1);
			if (ret == -1)
				column = 3;
		} else
			column = 3;

		if (column < 3 || column > 8) {
			bm_err(gm, "%s, %s,column:%d ERROR!",
				__func__, node_name, column);
			/* correction */
			column = 3;
		}
		if (select_zcv == 0)
			ret = sprintf(node_name, "battery%d_profile_t%d", bat_id, i);
		else
			ret = sprintf(node_name, "battery%d_profile_t%d_cycle%d", bat_id, i, select_zcv);
		if (ret >= 0)
			fg_custom_parse_table(gm, np, node_name,
				fg_table_cust_data->fg_profile[i].fg_profile, column);
	}
}

void fg_check_bat_type(struct platform_device *dev,
	struct mtk_battery *gm)
{
	int _battery_id_gpio_0, _battery_id_gpio_1;
	int _battery_id_0, _battery_id_1;
	int val = 0, bat_dect = 0;
	struct device_node *np = dev->dev.of_node;

	gm->battery_id = 0;

	_battery_id_gpio_1 = of_get_named_gpio(np, "batteryid-gpio1", 0);
	_battery_id_gpio_0 = of_get_named_gpio(np, "batteryid-gpio", 0);

	fg_read_dts_val(gm, np, "DETECT_BAT_TYPE", &(bat_dect), 1);
	/* check batteryid gpio */
	if (_battery_id_gpio_0 < 0) {//no use battery id gpio
		if (bat_dect) {
			fg_read_dts_val(gm, np, "bat_type", &(val), 1);
			gm->battery_id = val;
		}
		bm_err(gm, "[%s] init battery type val:%d %d\n",
			__func__, gm->battery_id, bat_dect);

		return;
	}

	_battery_id_0 = gpio_get_value(_battery_id_gpio_0);

	if (_battery_id_gpio_1 < 0) {
		gm->battery_id = _battery_id_0;
		bm_err(gm, "[%s]GPIO Battery id (%d) (battery_id_gpio= %d)\n",
			__func__, gm->battery_id, _battery_id_gpio_0);
	} else {
		_battery_id_1 = gpio_get_value(_battery_id_gpio_1);
		gm->battery_id = (_battery_id_1 << 1) + (_battery_id_0);
		bm_err(gm, "[%s]GPIO Battery id (%d) (battery_id_gpio= %d %d)\n",
			__func__, gm->battery_id, _battery_id_gpio_1, _battery_id_gpio_0);
	}
}

static void fg_custom_part_ntc_table(struct mtk_battery *gm,
	const struct device_node *np)
{
	struct fg_temp *p_fg_temp_table;
	int bat_temp = 0, temperature_r = 0;
	int saddles = 0, idx = 0, ret = 0, ret_a = 0;
	int i;

	p_fg_temp_table = gm->tmp_table;

	ret = fg_read_dts_val(gm, np, "RBAT_TYPE", &(gm->rbat.type), 1);
	ret_a = fg_read_dts_val(gm, np, "RBAT_PULL_UP_R",
			&(gm->rbat.rbat_pull_up_r), 1);
	if ((ret == -1) || (ret_a == -1)) {
		bm_err(gm, "Fail to get ntc type from dts.Keep default value\t");
		bm_err(gm, "RBAT_TYPE=%d, RBAT_PULL_UP_R=%d\n",
			gm->rbat.type, gm->rbat.rbat_pull_up_r);
		return;
	}
	bm_err(gm, "From DTS. RBAT_TYPE = %d, RBAT_PULL_UP_R=%d\n",
		gm->rbat.type, gm->rbat.rbat_pull_up_r);

	fg_read_dts_val(gm, np, "rbat_temperature_table_num", &saddles, 1);
	bm_err(gm, "%s : rbat_temperature_table_num(%d)\n", __func__, saddles);

	idx = 0;

	while (1) {
		if (idx >= (saddles * 2))
			break;
		ret = of_property_read_u32_index(np, "rbat_battery_temperature",
							idx, &bat_temp);

		idx++;
		if (!of_property_read_u32_index( np, "rbat_battery_temperature",
							idx, &temperature_r))
			bm_debug(gm, "bat_temp = %d, temperature_r=%d\n",
					bat_temp, temperature_r);

		p_fg_temp_table->BatteryTemp = bat_temp;
		p_fg_temp_table->TemperatureR = temperature_r;

		idx++;
		p_fg_temp_table++;
	}

	bm_debug(gm, "[after]gm->tmp_table - bat_temp : temperature_r\n");
	for (i = 0; i < saddles; i++) {
		bm_debug(gm, "%d : %d %d\n", i, gm->tmp_table[i].BatteryTemp,
			gm->tmp_table[i].TemperatureR);
	}
}

void fg_custom_init_from_dts(struct platform_device *dev,
	struct mtk_battery *gm)
{
	struct device_node *np = dev->dev.of_node;
	unsigned int val = 0;
	int bat_id, multi_battery = 0, active_table = 0;
	int i, j, ret, column = 0;
	int r_pseudo100_raw = 0, r_pseudo100_col = 0;
	int lk_v = 0, lk_i = 0, shuttime = 0;
	int is_evb_board = 0;
	char node_name[128];
	struct fuel_gauge_custom_data *fg_cust_data;
	struct fuel_gauge_table_custom_data *fg_table_cust_data;

	bat_id = fgauge_get_profile_id(gm);
	fg_cust_data = &gm->fg_cust_data;
	fg_table_cust_data = &gm->fg_table_cust_data;


	if (gm->ptim_lk_v == 0) {
		fg_read_dts_val(gm, np, "fg_swocv_v", &(lk_v), 1);
		gm->ptim_lk_v = lk_v;
	}

	if (gm->ptim_lk_i == 0) {
		fg_read_dts_val(gm, np, "fg_swocv_i", &(lk_i), 1);
		gm->ptim_lk_i = lk_i;
	}

	if (gm->pl_shutdown_time == 0) {
		fg_read_dts_val(gm, np, "shutdown_time", &(shuttime), 1);
		gm->pl_shutdown_time = shuttime;
	}

	fg_read_dts_val(gm, np, "is_evb_board", &(is_evb_board), 1);
	gm->is_evb_board = is_evb_board;

	bm_err(gm, "%s swocv_v:%d swocv_i:%d shutdown_time:%d is_evb_board:%d\n",
		__func__, gm->ptim_lk_v, gm->ptim_lk_i, gm->pl_shutdown_time, gm->is_evb_board);

	fg_cust_data->disable_nafg =
		of_property_read_bool(np, "DISABLE_NAFG");
	fg_cust_data->disable_nafg |=
		of_property_read_bool(np, "disable-nafg");
	bm_err(gm, "disable_nafg:%d\n",
		fg_cust_data->disable_nafg);

	fg_read_dts_val(gm, np, "MULTI_BATTERY", &(multi_battery), 1);
	fg_read_dts_val(gm, np, "ACTIVE_TABLE", &(active_table), 1);

	fg_read_dts_val(gm, np, "Q_MAX_L_CURRENT", &(fg_cust_data->q_max_L_current),
		1);
	fg_read_dts_val(gm, np, "Q_MAX_H_CURRENT", &(fg_cust_data->q_max_H_current),
		1);
	fg_read_dts_val_by_idx(gm, np, "g_Q_MAX_SYS_VOLTAGE", gm->battery_id,
		&(fg_cust_data->q_max_sys_voltage), UNIT_TRANS_10);

	fg_read_dts_val(gm, np, "PSEUDO1_EN", &(fg_cust_data->pseudo1_en), 1);
	fg_read_dts_val(gm, np, "PSEUDO100_EN", &(fg_cust_data->pseudo100_en), 1);
	fg_read_dts_val(gm, np, "PSEUDO100_EN_DIS",
		&(fg_cust_data->pseudo100_en_dis), 1);
	fg_read_dts_val_by_idx(gm, np, "g_FG_PSEUDO1_OFFSET", gm->battery_id,
		&(fg_cust_data->pseudo1_iq_offset), UNIT_TRANS_100);

	/* iboot related */
	fg_read_dts_val(gm, np, "QMAX_SEL", &(fg_cust_data->qmax_sel), 1);
	fg_read_dts_val(gm, np, "IBOOT_SEL", &(fg_cust_data->iboot_sel), 1);
	fg_read_dts_val(gm, np, "SHUTDOWN_SYSTEM_IBOOT",
		&(fg_cust_data->shutdown_system_iboot), 1);

	/*hw related */
	fg_read_dts_val(gm, np, "CAR_TUNE_VALUE", &(fg_cust_data->car_tune_value),
		UNIT_TRANS_10);
	gm->gauge->hw_status.car_tune_value =
		fg_cust_data->car_tune_value;

	fg_read_dts_val(gm, np, "FG_METER_RESISTANCE",
		&(fg_cust_data->fg_meter_resistance), 1);
	ret = fg_read_dts_val(gm, np, "COM_FG_METER_RESISTANCE",
		&(fg_cust_data->com_fg_meter_resistance), 1);
	if (ret == -1)
		fg_cust_data->com_fg_meter_resistance =
			fg_cust_data->fg_meter_resistance;

	fg_read_dts_val(gm, np, "NO_BAT_TEMP_COMPENSATE",
		&(gm->no_bat_temp_compensate), 1);

	fg_read_dts_val(gm, np, "CURR_MEASURE_20A", &(fg_cust_data->curr_measure_20a), 1);

	fg_read_dts_val(gm, np, "UNIT_MULTIPLE", &(fg_cust_data->unit_multiple), 1);

	fg_read_dts_val(gm, np, "R_FG_VALUE", &(fg_cust_data->r_fg_value),
		UNIT_TRANS_10);

	fg_read_dts_val(gm, np, "CURR_MEASURE_20A",
		&(fg_cust_data->curr_measure_20a), 1);
	fg_read_dts_val(gm, np, "UNIT_MULTIPLE",
		&(fg_cust_data->unit_multiple), 1);

	gm->gauge->hw_status.r_fg_value =
		fg_cust_data->r_fg_value;

	ret = fg_read_dts_val(gm, np, "COM_R_FG_VALUE",
		&(fg_cust_data->com_r_fg_value), UNIT_TRANS_10);
	if (ret == -1)
		fg_cust_data->com_r_fg_value = fg_cust_data->r_fg_value;

	/* note [ALPS05395813] battery: add custom ntc_table */
	fg_custom_part_ntc_table(gm, np);

	fg_read_dts_val(gm, np, "FULL_TRACKING_BAT_INT2_MULTIPLY",
		&(fg_cust_data->full_tracking_bat_int2_multiply), 1);
	fg_read_dts_val(gm, np, "enable_tmp_intr_suspend",
		&(gm->enable_tmp_intr_suspend), 1);

	/* dynamic CV */
	fg_read_dts_val(gm, np, "DYNAMIC_CV_FACTOR", &(fg_cust_data->dynamic_cv_factor), 1);
	fg_read_dts_val(gm, np, "CHARGER_IEOC", &(fg_cust_data->charger_ieoc), 1);

	/* Aging Compensation */
	fg_read_dts_val(gm, np, "AGING_ONE_EN", &(fg_cust_data->aging_one_en), 1);
	fg_read_dts_val(gm, np, "AGING1_UPDATE_SOC",
		&(fg_cust_data->aging1_update_soc), UNIT_TRANS_100);
	fg_read_dts_val(gm, np, "AGING1_LOAD_SOC",
		&(fg_cust_data->aging1_load_soc), UNIT_TRANS_100);
	fg_read_dts_val(gm, np, "AGING_TEMP_DIFF",
		&(fg_cust_data->aging_temp_diff), 1);
	fg_read_dts_val(gm, np, "AGING_100_EN", &(fg_cust_data->aging_100_en), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_VOLTAGE_UPDATE",
		&(fg_cust_data->difference_voltage_update), 1);
	fg_read_dts_val(gm, np, "AGING_FACTOR_MIN",
		&(fg_cust_data->aging_factor_min), UNIT_TRANS_100);
	fg_read_dts_val(gm, np, "AGING_FACTOR_DIFF",
		&(fg_cust_data->aging_factor_diff), UNIT_TRANS_100);
	/* Aging Compensation 2*/
	fg_read_dts_val(gm, np, "AGING_TWO_EN", &(fg_cust_data->aging_two_en), 1);
	/* Aging Compensation 3*/
	fg_read_dts_val(gm, np, "AGING_THIRD_EN", &(fg_cust_data->aging_third_en),
		1);

	/* ui_soc related */
	fg_read_dts_val(gm, np, "DIFF_SOC_SETTING",
		&(fg_cust_data->diff_soc_setting), 1);
	fg_read_dts_val(gm, np, "KEEP_100_PERCENT",
		&(fg_cust_data->keep_100_percent), UNIT_TRANS_100);
	fg_read_dts_val(gm, np, "DIFFERENCE_FULL_CV",
		&(fg_cust_data->difference_full_cv), 1);
	fg_read_dts_val(gm, np, "DIFF_BAT_TEMP_SETTING",
		&(fg_cust_data->diff_bat_temp_setting), 1);
	fg_read_dts_val(gm, np, "DIFF_BAT_TEMP_SETTING_C",
		&(fg_cust_data->diff_bat_temp_setting_c), 1);
	fg_read_dts_val(gm, np, "DISCHARGE_TRACKING_TIME",
		&(fg_cust_data->discharge_tracking_time), 1);
	fg_read_dts_val(gm, np, "CHARGE_TRACKING_TIME",
		&(fg_cust_data->charge_tracking_time), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_FULLOCV_VTH",
		&(fg_cust_data->difference_fullocv_vth), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_FULLOCV_ITH",
		&(fg_cust_data->difference_fullocv_ith), UNIT_TRANS_10);
	fg_read_dts_val(gm, np, "CHARGE_PSEUDO_FULL_LEVEL",
		&(fg_cust_data->charge_pseudo_full_level), 1);
	fg_read_dts_val(gm, np, "OVER_DISCHARGE_LEVEL",
		&(fg_cust_data->over_discharge_level), 1);

	/* pre tracking */
	fg_read_dts_val(gm, np, "FG_PRE_TRACKING_EN",
		&(fg_cust_data->fg_pre_tracking_en), 1);
	fg_read_dts_val(gm, np, "VBAT2_DET_TIME",
		&(fg_cust_data->vbat2_det_time), 1);
	fg_read_dts_val(gm, np, "VBAT2_DET_COUNTER",
		&(fg_cust_data->vbat2_det_counter), 1);
	fg_read_dts_val(gm, np, "VBAT2_DET_VOLTAGE1",
		&(fg_cust_data->vbat2_det_voltage1), 1);
	fg_read_dts_val(gm, np, "VBAT2_DET_VOLTAGE2",
		&(fg_cust_data->vbat2_det_voltage2), 1);
	fg_read_dts_val(gm, np, "VBAT2_DET_VOLTAGE3",
		&(fg_cust_data->vbat2_det_voltage3), 1);
	gm->gauge->hw_status.vbat2_det_time =
		fg_cust_data->vbat2_det_time;
	gm->gauge->hw_status.vbat2_det_counter =
		fg_cust_data->vbat2_det_counter;

	/* sw fg */
	fg_read_dts_val(gm, np, "DIFFERENCE_FGC_FGV_TH1",
		&(fg_cust_data->difference_fgc_fgv_th1), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_FGC_FGV_TH2",
		&(fg_cust_data->difference_fgc_fgv_th2), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_FGC_FGV_TH3",
		&(fg_cust_data->difference_fgc_fgv_th3), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_FGC_FGV_TH_SOC1",
		&(fg_cust_data->difference_fgc_fgv_th_soc1), 1);
	fg_read_dts_val(gm, np, "DIFFERENCE_FGC_FGV_TH_SOC2",
		&(fg_cust_data->difference_fgc_fgv_th_soc2), 1);
	fg_read_dts_val(gm, np, "NAFG_TIME_SETTING",
		&(fg_cust_data->nafg_time_setting), 1);
	fg_read_dts_val(gm, np, "NAFG_RATIO", &(fg_cust_data->nafg_ratio), 1);
	fg_read_dts_val(gm, np, "NAFG_RATIO_EN", &(fg_cust_data->nafg_ratio_en), 1);
	fg_read_dts_val(gm, np, "NAFG_RATIO_TMP_THR",
		&(fg_cust_data->nafg_ratio_tmp_thr), 1);
	fg_read_dts_val(gm, np, "NAFG_RESISTANCE", &(fg_cust_data->nafg_resistance),
		1);

	/* shutdown jumping*/
	fg_read_dts_val(gm, np, "LOW_TRACKING_JUMP",
		&(fg_cust_data->low_tracking_jump), 1);
	fg_read_dts_val(gm, np, "PRE_TRACKING_JUMP",
		&(fg_cust_data->pre_tracking_jump), 1);
	fg_read_dts_val(gm, np, "LAST_MODE_RESET",
		&(fg_cust_data->last_mode_reset), 1);
	fg_read_dts_val(gm, np, "PRE_TRACKING_SOC_RESET",
		&(fg_cust_data->pre_tracking_soc_reset), 1);

	/* mode select */
	fg_read_dts_val(gm, np, "PMIC_SHUTDOWN_CURRENT",
		&(fg_cust_data->pmic_shutdown_current), 1);
	fg_read_dts_val(gm, np, "PMIC_SHUTDOWN_SW_EN",
		&(fg_cust_data->pmic_shutdown_sw_en), 1);
	fg_read_dts_val(gm, np, "FORCE_VC_MODE", &(fg_cust_data->force_vc_mode), 1);
	fg_read_dts_val(gm, np, "EMBEDDED_SEL", &(fg_cust_data->embedded_sel), 1);
	fg_read_dts_val(gm, np, "LOADING_1_EN", &(fg_cust_data->loading_1_en), 1);
	fg_read_dts_val(gm, np, "LOADING_2_EN", &(fg_cust_data->loading_2_en), 1);
	fg_read_dts_val(gm, np, "DIFF_IAVG_TH", &(fg_cust_data->diff_iavg_th), 1);

	fg_read_dts_val(gm, np, "SHUTDOWN_GAUGE0", &(fg_cust_data->shutdown_gauge0),
		1);
	fg_read_dts_val(gm, np, "SHUTDOWN_1_TIME", &(fg_cust_data->shutdown_1_time),
		1);
	fg_read_dts_val(gm, np, "SHUTDOWN_GAUGE1_XMINS",
		&(fg_cust_data->shutdown_gauge1_xmins), 1);
	fg_read_dts_val(gm, np, "SHUTDOWN_GAUGE0_VOLTAGE",
		&(fg_cust_data->shutdown_gauge0_voltage), 1);
	fg_read_dts_val(gm, np, "SHUTDOWN_GAUGE1_VBAT_EN",
		&(fg_cust_data->shutdown_gauge1_vbat_en), 1);
	fg_read_dts_val(gm, np, "SHUTDOWN_GAUGE1_VBAT",
		&(fg_cust_data->shutdown_gauge1_vbat), 1);

	/* ZCV update */
	fg_read_dts_val(gm, np, "ZCV_SUSPEND_TIME",
		&(fg_cust_data->zcv_suspend_time), 1);
	fg_read_dts_val(gm, np, "SLEEP_CURRENT_AVG",
		&(fg_cust_data->sleep_current_avg), 1);
	fg_read_dts_val(gm, np, "ZCV_COM_VOL_LIMIT",
		&(fg_cust_data->zcv_com_vol_limit), 1);
	fg_read_dts_val(gm, np, "ZCV_CAR_GAP_PERCENTAGE",
		&(fg_cust_data->zcv_car_gap_percentage), 1);

	/* dod_init */
	fg_read_dts_val(gm, np, "HWOCV_OLDOCV_DIFF",
		&(fg_cust_data->hwocv_oldocv_diff), 1);
	fg_read_dts_val(gm, np, "HWOCV_OLDOCV_DIFF_CHR",
		&(fg_cust_data->hwocv_oldocv_diff_chr), 1);
	fg_read_dts_val(gm, np, "HWOCV_SWOCV_DIFF",
		&(fg_cust_data->hwocv_swocv_diff), 1);
	fg_read_dts_val(gm, np, "HWOCV_SWOCV_DIFF_LT",
		&(fg_cust_data->hwocv_swocv_diff_lt), 1);
	fg_read_dts_val(gm, np, "HWOCV_SWOCV_DIFF_LT_TEMP",
		&(fg_cust_data->hwocv_swocv_diff_lt_temp), 1);
	fg_read_dts_val(gm, np, "SWOCV_OLDOCV_DIFF",
		&(fg_cust_data->swocv_oldocv_diff), 1);
	fg_read_dts_val(gm, np, "SWOCV_OLDOCV_DIFF_CHR",
		&(fg_cust_data->swocv_oldocv_diff_chr), 1);
	fg_read_dts_val(gm, np, "VBAT_OLDOCV_DIFF",
		&(fg_cust_data->vbat_oldocv_diff), 1);
	fg_read_dts_val(gm, np, "SWOCV_OLDOCV_DIFF_EMB",
		&(fg_cust_data->swocv_oldocv_diff_emb), 1);

	fg_read_dts_val(gm, np, "PMIC_SHUTDOWN_TIME",
		&(fg_cust_data->pmic_shutdown_time), UNIT_TRANS_60);
	fg_read_dts_val(gm, np, "TNEW_TOLD_PON_DIFF",
		&(fg_cust_data->tnew_told_pon_diff), 1);
	fg_read_dts_val(gm, np, "TNEW_TOLD_PON_DIFF2",
		&(fg_cust_data->tnew_told_pon_diff2), 1);
	fg_read_dts_val(gm, np, "EXT_HWOCV_SWOCV",
		&(gm->ext_hwocv_swocv), 1);
	fg_read_dts_val(gm, np, "EXT_HWOCV_SWOCV_LT",
		&(gm->ext_hwocv_swocv_lt), 1);
	fg_read_dts_val(gm, np, "EXT_HWOCV_SWOCV_LT_TEMP",
		&(gm->ext_hwocv_swocv_lt_temp), 1);
	fg_read_dts_val(gm, np, "daemon-version", &(gm->daemon_version), 1);

	fg_read_dts_val(gm, np, "DC_RATIO_SEL", &(fg_cust_data->dc_ratio_sel), 1);
	fg_read_dts_val(gm, np, "DC_R_CNT", &(fg_cust_data->dc_r_cnt), 1);

	fg_read_dts_val(gm, np, "PSEUDO1_SEL", &(fg_cust_data->pseudo1_sel), 1);

	fg_read_dts_val(gm, np, "D0_SEL", &(fg_cust_data->d0_sel), 1);
	fg_read_dts_val(gm, np, "AGING_SEL", &(fg_cust_data->aging_sel), 1);
	fg_read_dts_val(gm, np, "BAT_PAR_I", &(fg_cust_data->bat_par_i), 1);
	fg_read_dts_val(gm, np, "RECORD_LOG", &(fg_cust_data->record_log), 1);


	fg_read_dts_val(gm, np, "FG_TRACKING_CURRENT",
		&(fg_cust_data->fg_tracking_current), 1);
	fg_read_dts_val(gm, np, "FG_TRACKING_CURRENT_IBOOT_EN",
		&(fg_cust_data->fg_tracking_current_iboot_en), 1);
	fg_read_dts_val(gm, np, "UI_FAST_TRACKING_EN",
		&(fg_cust_data->ui_fast_tracking_en), 1);
	fg_read_dts_val(gm, np, "UI_FAST_TRACKING_GAP",
		&(fg_cust_data->ui_fast_tracking_gap), 1);

	fg_read_dts_val(gm, np, "BAT_PLUG_OUT_TIME",
		&(fg_cust_data->bat_plug_out_time), 1);
	fg_read_dts_val(gm, np, "KEEP_100_PERCENT_MINSOC",
		&(fg_cust_data->keep_100_percent_minsoc), 1);

	fg_read_dts_val(gm, np, "UISOC_UPDATE_TYPE",
		&(fg_cust_data->uisoc_update_type), 1);

	fg_read_dts_val(gm, np, "BATTERY_TMP_TO_DISABLE_GM30",
		&(fg_cust_data->battery_tmp_to_disable_gm30), 1);
	fg_read_dts_val(gm, np, "BATTERY_TMP_TO_DISABLE_NAFG",
		&(fg_cust_data->battery_tmp_to_disable_nafg), 1);
	fg_read_dts_val(gm, np, "BATTERY_TMP_TO_ENABLE_NAFG",
		&(fg_cust_data->battery_tmp_to_enable_nafg), 1);

	fg_read_dts_val(gm, np, "LOW_TEMP_MODE", &(fg_cust_data->low_temp_mode), 1);
	fg_read_dts_val(gm, np, "LOW_TEMP_MODE_TEMP",
		&(fg_cust_data->low_temp_mode_temp), 1);

	/* current limit for uisoc 100% */
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_EN",
		&(fg_cust_data->ui_full_limit_en), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_SOC0",
		&(fg_cust_data->ui_full_limit_soc0), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_ITH0",
		&(fg_cust_data->ui_full_limit_ith0), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_SOC1",
		&(fg_cust_data->ui_full_limit_soc1), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_ITH1",
		&(fg_cust_data->ui_full_limit_ith1), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_SOC2",
		&(fg_cust_data->ui_full_limit_soc2), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_ITH2",
		&(fg_cust_data->ui_full_limit_ith2), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_SOC3",
		&(fg_cust_data->ui_full_limit_soc3), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_ITH3",
		&(fg_cust_data->ui_full_limit_ith3), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_SOC4",
		&(fg_cust_data->ui_full_limit_soc4), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_ITH4",
		&(fg_cust_data->ui_full_limit_ith4), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_TIME",
		&(fg_cust_data->ui_full_limit_time), 1);

	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_SOC0",
		&(fg_cust_data->ui_full_limit_fc_soc0), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_ITH0",
		&(fg_cust_data->ui_full_limit_fc_ith0), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_SOC1",
		&(fg_cust_data->ui_full_limit_fc_soc1), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_ITH1",
		&(fg_cust_data->ui_full_limit_fc_ith1), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_SOC2",
		&(fg_cust_data->ui_full_limit_fc_soc2), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_ITH2",
		&(fg_cust_data->ui_full_limit_fc_ith2), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_SOC3",
		&(fg_cust_data->ui_full_limit_fc_soc3), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_ITH3",
		&(fg_cust_data->ui_full_limit_fc_ith3), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_SOC4",
		&(fg_cust_data->ui_full_limit_fc_soc4), 1);
	fg_read_dts_val(gm, np, "UI_FULL_LIMIT_FC_ITH4",
		&(fg_cust_data->ui_full_limit_fc_ith4), 1);

	/* voltage limit for uisoc 1% */
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_EN", &(fg_cust_data->ui_low_limit_en),
		1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_SOC0",
		&(fg_cust_data->ui_low_limit_soc0), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_VTH0",
		&(fg_cust_data->ui_low_limit_vth0), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_SOC1",
		&(fg_cust_data->ui_low_limit_soc1), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_VTH1",
		&(fg_cust_data->ui_low_limit_vth1), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_SOC2",
		&(fg_cust_data->ui_low_limit_soc2), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_VTH2",
		&(fg_cust_data->ui_low_limit_vth2), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_SOC3",
		&(fg_cust_data->ui_low_limit_soc3), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_VTH3",
		&(fg_cust_data->ui_low_limit_vth3), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_SOC4",
		&(fg_cust_data->ui_low_limit_soc4), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_VTH4",
		&(fg_cust_data->ui_low_limit_vth4), 1);
	fg_read_dts_val(gm, np, "UI_LOW_LIMIT_TIME",
		&(fg_cust_data->ui_low_limit_time), 1);

	/* battery healthd */
	fg_read_dts_val(gm, np, "BAT_BH_EN",
		&(fg_cust_data->bat_bh_en), 1);
	fg_read_dts_val(gm, np, "AGING_DIFF_MAX_THRESHOLD",
		&(fg_cust_data->aging_diff_max_threshold), 1);
	fg_read_dts_val(gm, np, "AGING_DIFF_MAX_LEVEL",
		&(fg_cust_data->aging_diff_max_level), 1);
	fg_read_dts_val(gm, np, "AGING_FACTOR_T_MIN",
		&(fg_cust_data->aging_factor_t_min), 1);
	fg_read_dts_val(gm, np, "CYCLE_DIFF",
		&(fg_cust_data->cycle_diff), 1);
	fg_read_dts_val(gm, np, "AGING_COUNT_MIN",
		&(fg_cust_data->aging_count_min), 1);
	fg_read_dts_val(gm, np, "DEFAULT_SCORE",
		&(fg_cust_data->default_score), 1);
	fg_read_dts_val(gm, np, "DEFAULT_SCORE_QUANTITY",
		&(fg_cust_data->default_score_quantity), 1);
	fg_read_dts_val(gm, np, "FAST_CYCLE_SET",
		&(fg_cust_data->fast_cycle_set), 1);
	fg_read_dts_val(gm, np, "LEVEL_MAX_CHANGE_BAT",
		&(fg_cust_data->level_max_change_bat), 1);
	fg_read_dts_val(gm, np, "DIFF_MAX_CHANGE_BAT",
		&(fg_cust_data->diff_max_change_bat), 1);
	fg_read_dts_val(gm, np, "AGING_TRACKING_START",
		&(fg_cust_data->aging_tracking_start), 1);
	fg_read_dts_val(gm, np, "MAX_AGING_DATA",
		&(fg_cust_data->max_aging_data), 1);
	fg_read_dts_val(gm, np, "MAX_FAST_DATA",
		&(fg_cust_data->max_fast_data), 1);
	fg_read_dts_val(gm, np, "FAST_DATA_THRESHOLD_SCORE",
		&(fg_cust_data->fast_data_threshold_score), 1);
	fg_read_dts_val(gm, np, "SHOW_AGING_PERIOD",
		&(fg_cust_data->show_aging_period), 1);

	fg_read_dts_val(gm, np, "AVGVBAT_ARRAY_SIZE",
		&(gm->avgvbat_array_size), 1);

	/* average battemp */
	fg_read_dts_val(gm, np, "MOVING_BATTEMP_EN",
		&(fg_cust_data->moving_battemp_en), 1);
	fg_read_dts_val(gm, np, "MOVING_BATTEMP_THR",
		&(fg_cust_data->moving_battemp_thr), 1);

	gm->disableGM30 = of_property_read_bool(
		np, "DISABLE_MTKBATTERY");
	gm->disableGM30 |= of_property_read_bool(
		np, "disable-mtkbattery");
	gm->disableGM30 |= gm->is_evb_board;
	gm->nouse_baton_undet = of_property_read_bool(
		np, "nouse-baton-undet");

	fg_read_dts_val(gm, np, "MULTI_TEMP_GAUGE0",
		&(fg_cust_data->multi_temp_gauge0), 1);
	fg_read_dts_val(gm, np, "FGC_FGV_TH1",
		&(fg_cust_data->difference_fgc_fgv_th1), 1);
	fg_read_dts_val(gm, np, "FGC_FGV_TH2",
		&(fg_cust_data->difference_fgc_fgv_th2), 1);
	fg_read_dts_val(gm, np, "FGC_FGV_TH3",
		&(fg_cust_data->difference_fgc_fgv_th3), 1);
	fg_read_dts_val(gm, np, "UISOC_UPDATE_T",
		&(fg_cust_data->uisoc_update_type), 1);
	fg_read_dts_val(gm, np, "UIFULLLIMIT_EN",
		&(fg_cust_data->ui_full_limit_en), 1);
	fg_read_dts_val(gm, np, "MTK_CHR_EXIST", &(fg_cust_data->mtk_chr_exist), 1);

	fg_read_dts_val(gm, np, "GM30_DISABLE_NAFG", &(fg_cust_data->disable_nafg),
		1);
	fg_read_dts_val(gm, np, "FIXED_BATTERY_TEMPERATURE", &(gm->fixed_bat_tmp),
		1);

	fg_read_dts_val(gm, np, "NO_PROP_TIMEOUT_CONTROL",
		&(gm->no_prop_timeout_control), 1);

	fg_read_dts_val(gm, np, "ACTIVE_TABLE",
		&(fg_table_cust_data->active_table_number), 1);

#if IS_ENABLED(CONFIG_MTK_ADDITIONAL_BATTERY_TABLE)
	if (fg_table_cust_data->active_table_number == 0)
		fg_table_cust_data->active_table_number = 5;
#else
	if (fg_table_cust_data->active_table_number == 0)
		fg_table_cust_data->active_table_number = 4;
#endif

	bm_err(gm, "fg active table:%d\n",
		fg_table_cust_data->active_table_number);

	fg_read_dts_val(gm, np, "ENABLE_R_RATIO",
		&(fg_table_cust_data->enable_r_ratio), 1);
	fg_read_dts_val(gm, np, "MAX_RATIO_TEMP",
		&(fg_table_cust_data->max_ratio_temp), 1);
	fg_read_dts_val(gm, np, "MIN_RATIO_TEMP",
		&(fg_table_cust_data->min_ratio_temp), 1);
	fg_read_dts_val(gm, np, "ENABLE_CHARGING_RATIO",
		&(fg_table_cust_data->enable_charging_ratio), 1);
	fg_read_dts_val(gm, np, "MAX_CHARGING_RATIO_TEMP",
		&(fg_table_cust_data->max_charging_ratio_temp), 1);
	fg_read_dts_val(gm, np, "MIN_CHARGING_RATIO_TEMP",
		&(fg_table_cust_data->min_charging_ratio_temp), 1);

	/* low bat bound*/
	fg_read_dts_val(gm, np, "BAT_VOLTAGE_LOW_BOUND",
		&(gm->bat_voltage_low_bound), 1);
	fg_read_dts_val(gm, np, "LOW_TMP_BAT_VOLTAGE_LOW_BOUND",
		&(gm->low_tmp_bat_voltage_low_bound), 1);
	gm->bat_voltage_low_bound_orig = gm->bat_voltage_low_bound;
	gm->low_tmp_bat_voltage_low_bound_orig = gm->low_tmp_bat_voltage_low_bound;
	/* battery temperature  related*/
	fg_read_dts_val(gm, np, "RBAT_PULL_UP_R", &(gm->rbat.rbat_pull_up_r), 1);
	fg_read_dts_val(gm, np, "RBAT_PULL_UP_VOLT",
		&(gm->rbat.rbat_pull_up_volt), 1);

	/* battery temperature, TEMPERATURE_T0 ~ T9 */
	for (i = 0; i < fg_table_cust_data->active_table_number; i++) {
		ret = sprintf(node_name, "TEMPERATURE_T%d", i);
		if (ret >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_table_cust_data->fg_profile[i].temperature), 1);
		}

	fg_read_dts_val(gm, np, "TEMPERATURE_TB0",
		&(fg_table_cust_data->temperature_tb0), 1);
	fg_read_dts_val(gm, np, "TEMPERATURE_TB1",
		&(fg_table_cust_data->temperature_tb1), 1);

	for (i = 0; i < MAX_TABLE; i++) {
		struct fuelgauge_profile_struct *p;

		p = &fg_table_cust_data->fg_profile[i].fg_profile[0];
		fg_read_dts_val_by_idx(gm, np, "g_temperature", i,
			&(fg_table_cust_data->fg_profile[i].temperature), 1);
		fg_read_dts_val_by_idx(gm, np, "g_Q_MAX",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].q_max), 1);
		fg_read_dts_val_by_idx(gm, np, "g_Q_MAX_H_CURRENT",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].q_max_h_current),
			1);
		fg_read_dts_val_by_idx(gm, np, "g_FG_PSEUDO1",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pseudo1),
			UNIT_TRANS_100);
		fg_read_dts_val_by_idx(gm, np, "g_FG_PSEUDO100",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val_by_idx(gm, np, "g_PMIC_MIN_VOL",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pmic_min_vol), 1);
		fg_read_dts_val_by_idx(gm, np, "g_PON_SYS_IBOOT",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pon_iboot), 1);
		fg_read_dts_val_by_idx(gm, np, "g_QMAX_SYS_VOL",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].qmax_sys_vol), 1);
		fg_read_dts_val_by_idx(gm, np, "g_SHUTDOWN_HL_ZCV",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].shutdown_hl_zcv),
			1);
		fg_read_dts_val_by_idx(gm, np, "g_iavg_th",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(gm->iavg_th[i]),
			1);
		for (j = 0; j < 100; j++) {
			if (p[j].charge_r.rdc[0] == 0)
				p[j].charge_r.rdc[0] = p[j].resistance;
	}
	}

	if (bat_id >= 0 && bat_id < TOTAL_BATTERY_NUMBER) {
		ret = sprintf(node_name, "Q_MAX_SYS_VOLTAGE_BAT%d", bat_id);
		if (ret  >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_cust_data->q_max_sys_voltage), UNIT_TRANS_10);

		ret = sprintf(node_name, "PSEUDO1_IQ_OFFSET_BAT%d", bat_id);
		if (ret >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_cust_data->pseudo1_iq_offset), UNIT_TRANS_100);
	} else
		bm_err(gm,
		"get Q_MAX_SYS_VOLTAGE_BAT, PSEUDO1_IQ_OFFSET_BAT %d no data\n",
		bat_id);

	if (fg_cust_data->multi_temp_gauge0 == 0) {
		int i = 0;
		int min_vol;

		min_vol = fg_table_cust_data->fg_profile[0].pmic_min_vol;
		if (!fg_read_dts_val(gm, np, "PMIC_MIN_VOL", &val, 1)) {
			for (i = 0; i < MAX_TABLE; i++)
				fg_table_cust_data->fg_profile[i].pmic_min_vol =
				(int)val;
				bm_debug(gm, "Get PMIC_MIN_VOL: %d\n",
					min_vol);
		} else {
			bm_err(gm, "Get PMIC_MIN_VOL no data\n");
		}

		if (!fg_read_dts_val(gm, np, "POWERON_SYSTEM_IBOOT", &val, 1)) {
			for (i = 0; i < MAX_TABLE; i++)
				fg_table_cust_data->fg_profile[i].pon_iboot =
				(int)val * UNIT_TRANS_10;

			bm_debug(gm, "Get POWERON_SYSTEM_IBOOT: %d\n",
				fg_table_cust_data->fg_profile[0].pon_iboot);
		} else {
			bm_err(gm, "Get POWERON_SYSTEM_IBOOT no data\n");
		}
	}

	if (active_table == 0 && multi_battery == 0) {
		fg_read_dts_val(gm, np, "g_FG_PSEUDO100_T0",
			&(fg_table_cust_data->fg_profile[0].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(gm, np, "g_FG_PSEUDO100_T1",
			&(fg_table_cust_data->fg_profile[1].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(gm, np, "g_FG_PSEUDO100_T2",
			&(fg_table_cust_data->fg_profile[2].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(gm, np, "g_FG_PSEUDO100_T3",
			&(fg_table_cust_data->fg_profile[3].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(gm, np, "g_FG_PSEUDO100_T4",
			&(fg_table_cust_data->fg_profile[4].pseudo100),
			UNIT_TRANS_100);
	}

	/* compatiable with old dtsi*/
	if (active_table == 0) {
		fg_read_dts_val(gm, np, "TEMPERATURE_T0",
			&(fg_table_cust_data->fg_profile[0].temperature), 1);
		fg_read_dts_val(gm, np, "TEMPERATURE_T1",
			&(fg_table_cust_data->fg_profile[1].temperature), 1);
		fg_read_dts_val(gm, np, "TEMPERATURE_T2",
			&(fg_table_cust_data->fg_profile[2].temperature), 1);
		fg_read_dts_val(gm, np, "TEMPERATURE_T3",
			&(fg_table_cust_data->fg_profile[3].temperature), 1);
		fg_read_dts_val(gm, np, "TEMPERATURE_T4",
			&(fg_table_cust_data->fg_profile[4].temperature), 1);
	}

	fg_read_dts_val(gm, np, "g_FG_charge_PSEUDO100_row",
		&(r_pseudo100_raw), 1);
	fg_read_dts_val(gm, np, "g_FG_charge_PSEUDO100_col",
		&(r_pseudo100_col), 1);

	/* init for pseudo100 */
	for (i = 0; i < MAX_TABLE; i++) {
		for (j = 0; j < MAX_CHARGE_RDC; j++)
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[j]
				= fg_table_cust_data->fg_profile[i].pseudo100;
	}

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err(gm, "%6d %6d %6d %6d %6d\n",
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[0],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[1],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[2],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[3],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[4]
			);
	}
	/* read dtsi from pseudo100 */
	for (i = 0; i < MAX_TABLE; i++) {
		for (j = 0; j < r_pseudo100_raw; j++) {
			fg_read_dts_val_by_idx(gm, np, "g_FG_charge_PSEUDO100",
				i*r_pseudo100_raw+j,
				&(fg_table_cust_data->fg_profile[
					i].r_pseudo100.pseudo[j+1]),
					UNIT_TRANS_100);
		}
	}


	bm_err(gm, "g_FG_charge_PSEUDO100_row:%d g_FG_charge_PSEUDO100_col:%d\n",
		r_pseudo100_raw, r_pseudo100_col);

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err(gm, "%6d %6d %6d %6d %6d\n",
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[0],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[1],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[2],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[3],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[4]
			);
	}

	// END of pseudo100


	for (i = 0; i < fg_table_cust_data->active_table_number; i++) {
		ret = sprintf(node_name, "battery%d_profile_t%d_num", bat_id, i);
		if (ret >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_table_cust_data->fg_profile[i].size), 1);

		/* compatiable with old dtsi table*/
		ret = sprintf(node_name, "battery%d_profile_t%d_col", bat_id, i);
		if (ret >= 0) {
			ret = fg_read_dts_val(gm, np, node_name, &(column), 1);
			if (ret == -1)
				column = 3;
		} else
			column = 3;

		if (column < 3 || column > 8) {
			bm_err(gm, "%s, %s,column:%d ERROR!",
				__func__, node_name, column);
			/* correction */
			column = 3;
		}

		ret = sprintf(node_name, "battery%d_profile_t%d", bat_id, i);
		if (ret >= 0)
			fg_custom_parse_table(gm, np, node_name,
				fg_table_cust_data->fg_profile[i].fg_profile, column);

		ret = snprintf(node_name, 50, "battery%d_r_ratio_profile_t%d_num", bat_id, i);
		if (ret >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_table_cust_data->fg_profile[i].r_ratio_active_table), 1);

		ret = snprintf(node_name, 50, "battery%d_r_ratio_profile_t%d", bat_id, i);
		if (ret >= 0)
			fg_custom_parse_ratio_table(gm, np, node_name,
				fg_table_cust_data->fg_profile[i].fg_r_profile, 2,
				fg_table_cust_data->fg_profile[i].r_ratio_active_table);

		ret = snprintf(node_name, 50, "battery%d_charging_r_ratio_profile_t%d_num", bat_id, i);
		if (ret >= 0)
			fg_read_dts_val(gm, np, node_name,
				&(fg_table_cust_data->fg_profile[i].charging_r_ratio_active_table), 1);

		ret = snprintf(node_name, 50, "battery%d_charging_r_ratio_profile_t%d", bat_id, i);
		if (ret >= 0)
			fg_custom_parse_ratio_table(gm, np, node_name,
				fg_table_cust_data->fg_profile[i].fg_charging_r_profile, 2,
				fg_table_cust_data->fg_profile[i].charging_r_ratio_active_table);
	}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	bm_err(gm, "%s oldocv diff thr set 0 in only factory bin\n", __func__);
	fg_cust_data->hwocv_oldocv_diff = 0;
	fg_cust_data->hwocv_oldocv_diff_chr = 0;
	fg_cust_data->hwocv_swocv_diff = 1500;
	fg_cust_data->hwocv_swocv_diff_lt = 0;
	fg_cust_data->swocv_oldocv_diff = 0;
	fg_cust_data->swocv_oldocv_diff_chr = 0;
	fg_cust_data->vbat_oldocv_diff = 0;
	fg_cust_data->swocv_oldocv_diff_emb = 0;
	fg_cust_data->vir_oldocv_diff_emb = 0;
	fg_cust_data->vir_oldocv_diff_emb_lt = 0;
	fg_cust_data->tnew_told_pon_diff = -1;
	gm->ext_hwocv_swocv = 1500;
#endif
}

#endif	/* end of CONFIG_OF */


/* ============================================================ */
/* interrupt handler */
/* ============================================================ */
void disable_fg(struct mtk_battery *gm)
{
	gm->disableGM30 = true;
	gm->ui_soc = 50;

	disable_all_irq(gm);
}

bool fg_interrupt_check(struct mtk_battery *gm)
{
	int is_battery_exist = 0;

	if (gm->disableGM30) {
		disable_fg(gm);
		return false;
	}

	gauge_get_property_control(gm, GAUGE_PROP_BATTERY_EXIST,
		&is_battery_exist, 0);

	if (!is_battery_exist)
		return false;

	return true;
}

int fg_coulomb_int_h_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);

	gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
	gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;

	gauge_coulomb_start(gm, &gm->coulomb_plus, gm->coulomb_int_gap);
	gauge_coulomb_start(gm, &gm->coulomb_minus, -gm->coulomb_int_gap);

	bm_err(gm, "[%s] car:%d ht:%d lt:%d gap:%d\n",
		__func__,
		fg_coulomb, gm->coulomb_int_ht,
		gm->coulomb_int_lt, gm->coulomb_int_gap);

	wakeup_fg_algo(gm, FG_INTR_BAT_INT1_HT);

	return 0;
}

int fg_coulomb_int_l_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);

	fg_sw_bat_cycle_accu(gm);
	gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
	gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;

	gauge_coulomb_start(gm, &gm->coulomb_plus, gm->coulomb_int_gap);
	gauge_coulomb_start(gm, &gm->coulomb_minus, -gm->coulomb_int_gap);

	bm_err(gm, "[%s] car:%d ht:%d lt:%d gap:%d\n",
		__func__,
		fg_coulomb, gm->coulomb_int_ht,
		gm->coulomb_int_lt, gm->coulomb_int_gap);
	wakeup_fg_algo(gm, FG_INTR_BAT_INT1_LT);

	return 0;
}

int fg_bat_int2_h_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);
	bm_debug(gm, "[%s] car:%d ht:%d\n",
		__func__,
		fg_coulomb, gm->uisoc_int_ht_en);
	fg_sw_bat_cycle_accu(gm);
	wakeup_fg_algo(gm, FG_INTR_BAT_INT2_HT);
	return 0;
}

int fg_bat_int2_l_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);
	bm_debug(gm, "[%s] car:%d ht:%d\n",
		__func__,
		fg_coulomb, gm->uisoc_int_lt_gap);
	fg_sw_bat_cycle_accu(gm);
	wakeup_fg_algo(gm, FG_INTR_BAT_INT2_LT);
	return 0;
}

/* ============================================================ */
/* sysfs */
/* ============================================================ */
static int temperature_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	gm->battery_temp= force_get_tbat(gm, true);
	*val = gm->battery_temp;
	bm_debug(gm, "%s %d\n", __func__, *val);
	return 0;
}

static int temperature_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->fixed_bat_tmp = val;
	bm_debug(gm, "%s %d\n", __func__, val);
	return 0;
}

static int log_level_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	*val = gm->log_level;
	return 0;
}

static int log_level_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->log_level = val;
	return 0;
}

static int coulomb_int_gap_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	int fg_coulomb = 0;

	gauge_get_property(gm, GAUGE_PROP_COULOMB, &fg_coulomb);
	gm->coulomb_int_gap = val;

	gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
	gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;
	gauge_coulomb_start(gm, &gm->coulomb_plus, gm->coulomb_int_gap);
	gauge_coulomb_start(gm, &gm->coulomb_minus, -gm->coulomb_int_gap);

	bm_debug(gm, "[%s]BAT_PROP_COULOMB_INT_GAP = %d car:%d\n",
		__func__,
		gm->coulomb_int_gap, fg_coulomb);
	return 0;
}

static int uisoc_ht_int_gap_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_ht_gap = val;
	gauge_coulomb_start(gm, &gm->uisoc_plus, gm->uisoc_int_ht_gap);
	bm_debug(gm, "[%s]BATTERY_UISOC_INT_HT_GAP = %d\n",
		__func__,
		gm->uisoc_int_ht_gap);
	return 0;
}

static int uisoc_lt_int_gap_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_lt_gap = val;
	gauge_coulomb_start(gm, &gm->uisoc_minus, -gm->uisoc_int_lt_gap);
	bm_debug(gm, "[%s]BATTERY_UISOC_INT_LT_GAP = %d\n",
		__func__,
		gm->uisoc_int_lt_gap);
	return 0;
}

static int en_uisoc_ht_int_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_ht_en = val;
	if (gm->uisoc_int_ht_en == 0)
		gauge_coulomb_stop(gm, &gm->uisoc_plus);
	bm_debug(gm, "[%s][fg_bat_int2] FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT = %d\n",
		__func__,
		gm->uisoc_int_ht_en);

	return 0;
}

static int en_uisoc_lt_int_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_lt_en = val;
	if (gm->uisoc_int_lt_en == 0)
		gauge_coulomb_stop(gm, &gm->uisoc_minus);
	bm_debug(gm, "[%s][fg_bat_int2] FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT = %d\n",
		__func__,
		gm->uisoc_int_lt_en);

	return 0;
}

static int uisoc_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	int daemon_ui_soc;
	int old_uisoc;
	ktime_t now_time, diff;
	struct timespec64 tmp_time;
	struct fuel_gauge_custom_data *pdata;

	pdata = &gm->fg_cust_data;
	daemon_ui_soc = val;

	if (daemon_ui_soc < 0) {
		bm_debug(gm, "[%s] error,daemon_ui_soc:%d\n",
			__func__,
			daemon_ui_soc);
		daemon_ui_soc = 0;
	}

	pdata->ui_old_soc = daemon_ui_soc;
	old_uisoc = gm->ui_soc;

	if (gm->disableGM30 == true)
		gm->ui_soc = 50;
	else
		gm->ui_soc = (daemon_ui_soc + 50) / 100;

	/* when UISOC changes, check the diff time for smooth */
	if (old_uisoc != gm->ui_soc) {
		now_time = ktime_get_boottime();
		diff = ktime_sub(now_time, gm->uisoc_oldtime);

		tmp_time = ktime_to_timespec64(diff);

		bm_debug(gm, "[%s] FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d old:%d diff=%lld\n",
			__func__,
			daemon_ui_soc, gm->ui_soc,
			gm->disableGM30, old_uisoc, tmp_time.tv_sec);
		gm->uisoc_oldtime = now_time;
		battery_update(gm->bm);
	} else {
		bm_debug(gm, "[%s] FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d\n",
			__func__,
			daemon_ui_soc, gm->ui_soc, gm->disableGM30);
		battery_update(gm->bm);
	}
	return 0;
}

static int disable_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	*val = gm->disableGM30;
	return 0;
}

static int disable_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->disableGM30 = val;
	if (gm->disableGM30 == true)
		battery_update(gm->bm);
	return 0;
}

static int init_done_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	*val = gm->init_flag;
	return 0;
}

static int init_done_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->init_flag = val;

	bm_debug(gm, "[%s] init_flag = %d\n",
		__func__,
		gm->init_flag);

	return 0;
}

static int reset_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	int car;

	if (gm->disableGM30)
		return 0;

	/* must handle sw_ncar before reset car */
	fg_sw_bat_cycle_accu(gm);
	gm->bat_cycle_car = 0;
	car = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);
	gm->log.car_diff += car;

	bm_err(gm, "%s car:%d\n",
		__func__, car);

	gauge_coulomb_before_reset(gm);
	gauge_set_property(gm, GAUGE_PROP_RESET, 0);
	gauge_coulomb_after_reset(gm);

	gm->sw_iavg_time = ktime_get_boottime();
	gm->sw_iavg_car = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);
	gm->bat_cycle_car = 0;

	return 0;
}

static ssize_t bat_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_battery *gm;
	struct mtk_battery_sysfs_field_info *battery_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

	battery_attr = container_of(attr,
		struct mtk_battery_sysfs_field_info, attr);
	if (battery_attr->set != NULL)
		battery_attr->set(gm, battery_attr, val);

	return count;
}

static ssize_t bat_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_battery *gm;
	struct mtk_battery_sysfs_field_info *battery_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

	battery_attr = container_of(attr,
		struct mtk_battery_sysfs_field_info, attr);
	if (battery_attr->get != NULL)
		battery_attr->get(gm, battery_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static int wakeup_fg_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	wakeup_fg_algo(gm, val);
	return 0;
}

/* Must be in the same order as BAT_PROP_* */
static struct mtk_battery_sysfs_field_info battery_sysfs_field_tbl[] = {
	BAT_SYSFS_FIELD_RW(temperature, BAT_PROP_TEMPERATURE),
	BAT_SYSFS_FIELD_WO(coulomb_int_gap, BAT_PROP_COULOMB_INT_GAP),
	BAT_SYSFS_FIELD_WO(uisoc_ht_int_gap, BAT_PROP_UISOC_HT_INT_GAP),
	BAT_SYSFS_FIELD_WO(uisoc_lt_int_gap, BAT_PROP_UISOC_LT_INT_GAP),
	BAT_SYSFS_FIELD_WO(en_uisoc_ht_int, BAT_PROP_ENABLE_UISOC_HT_INT),
	BAT_SYSFS_FIELD_WO(en_uisoc_lt_int, BAT_PROP_ENABLE_UISOC_LT_INT),
	BAT_SYSFS_FIELD_WO(uisoc, BAT_PROP_UISOC),
	BAT_SYSFS_FIELD_RW(disable, BAT_PROP_DISABLE),
	BAT_SYSFS_FIELD_RW(init_done, BAT_PROP_INIT_DONE),
	BAT_SYSFS_FIELD_WO(reset, BAT_PROP_FG_RESET),
	BAT_SYSFS_FIELD_RW(log_level, BAT_PROP_LOG_LEVEL),
	BAT_SYSFS_FIELD_WO(wakeup_fg, BAT_PROP_WAKEUP_FG_ALGO),
};

static struct attribute *
	battery_sysfs_attrs[ARRAY_SIZE(battery_sysfs_field_tbl) + 1];

static void battery_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(battery_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		battery_sysfs_attrs[i] = &battery_sysfs_field_tbl[i].attr.attr;

	battery_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int battery_sysfs_create_group(struct mtk_battery *gm)
{
	battery_sysfs_init_attrs();
	return 0;
}

/* ============================================================ */
/* nafg monitor */
/* ============================================================ */
void fg_nafg_monitor(struct mtk_battery *gm)
{
	int nafg_cnt = 0;
	ktime_t now_time = 0, dtime = 0;
	struct timespec64 tmp_dtime, tmp_now_time, tmp_last_time;
	signed int nafg_dltv = 0;
	signed int nafg_c_dltv = 0, vbat = 0, nafg_vbat= 0;

	if (gm->disableGM30 || gm->cmd_disable_nafg || gm->ntc_disable_nafg)
		return;

	tmp_now_time.tv_sec = 0;
	tmp_now_time.tv_nsec = 0;
	tmp_dtime.tv_sec = 0;
	tmp_dtime.tv_nsec = 0;

	nafg_cnt = gauge_get_int_property(gm, GAUGE_PROP_NAFG_CNT);
	nafg_dltv = gauge_get_int_property(gm, GAUGE_PROP_NAFG_DLTV);
	nafg_c_dltv = gauge_get_int_property(gm, GAUGE_PROP_NAFG_C_DLTV);
	nafg_vbat = gauge_get_int_property(gm, GAUGE_PROP_NAFG_VBAT);
	vbat = gauge_get_int_property(gm, GAUGE_PROP_BATTERY_VOLTAGE);

	if (gm->last_nafg_cnt != nafg_cnt) {
		gm->last_nafg_cnt = nafg_cnt;
		gm->last_nafg_update_time = ktime_get_boottime();
	} else {
		now_time = ktime_get_boottime();
		dtime = ktime_sub(now_time, gm->last_nafg_update_time);
		tmp_dtime = ktime_to_timespec64(dtime);

		if (tmp_dtime.tv_sec >= 600) {
			gm->is_nafg_broken = true;
			wakeup_fg_algo_cmd(
				gm,
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG,
				true);
		}
	}

	tmp_now_time = ktime_to_timespec64(now_time);
	tmp_last_time = ktime_to_timespec64(gm->last_nafg_update_time);
	bm_debug(gm,
		"[%s]vbat:%d, nafg_vbat:%d, nafg_zcv:%d, nafg_cnt:%d, nafg_dltv:%d, nafg_c_dltv:%d, nafg_c_dltv_thr:%d, nafg_en:%d, corner:%d",
		__func__, vbat, nafg_vbat, gm->gauge->hw_status.nafg_zcv, nafg_cnt, nafg_dltv, nafg_c_dltv,
		gm->gauge->hw_status.nafg_c_dltv_th, gm->gauge->hw_status.nafg_en, gm->gauge->nafg_corner);
	bm_debug(gm, "[%s]diff_time:%d nafg_cnt:%d, now:%d, last_t:%d dltv:%d cdltv:%d\n",
		__func__,
		(int)tmp_dtime.tv_sec,
		gm->last_nafg_cnt,
		(int)tmp_now_time.tv_sec,
		(int)tmp_last_time.tv_sec,
		nafg_dltv,
		nafg_c_dltv);

}

/* ============================================================ */
/* periodic timer */
/* ============================================================ */
static void fg_drv_update_hw_status(struct mtk_battery *gm)
{
	ktime_t ktime;
	struct property_control *prop_control;
	char gp_name[MAX_GAUGE_PROP_LEN];
	char reg_type_name[MAX_REGMAP_TYPE_LEN];
	int i, regmap_type, is_battery_exist = 0;
	long car;

	prop_control = &gm->prop_control;
	fg_update_porp_control(prop_control);

	gm->tbat = force_get_tbat_internal(gm);
	gm->vbat = gauge_get_int_property(gm,
				GAUGE_PROP_BATTERY_VOLTAGE);
	gm->ibat = gauge_get_int_property(gm,
				GAUGE_PROP_BATTERY_CURRENT);
	car = gauge_get_int_property(gm, GAUGE_PROP_COULOMB);
	gauge_get_property_control(gm, GAUGE_PROP_BATTERY_EXIST,
		&is_battery_exist, 0);

	bm_err(gm, "ss_precise_soc: %d.%d\n",  gm->ss_precise_soc/10, gm->ss_precise_soc%10);
	bm_err(gm, "ss_precise_ui_soc: %d.%d\n",  gm->ss_precise_ui_soc/10, gm->ss_precise_ui_soc%10);
	bm_err(gm, "[%s] car[%ld,%ld,%ld,%ld,%ld] tmp:%d soc:%d uisoc:%d vbat:%d ibat:%d baton:%d algo:%d gm3:%d %d %d %d %d %d, get_prop:%d %lld %d %d %d %lld %ld %lld, boot:%d rac:%d\n",
		gm->gauge->name,
		car,
		gm->coulomb_plus.end, gm->coulomb_minus.end,
		gm->uisoc_plus.end, gm->uisoc_minus.end,
		gm->tbat,
		gm->soc, gm->ui_soc,
		gm->vbat,
		gm->ibat,
		gm->baton,
		gm->algo.active,
		gm->disableGM30, gm->fg_cust_data.disable_nafg,
		gm->ntc_disable_nafg, gm->cmd_disable_nafg, gm->vbat0_flag, gm->is_evb_board,
		gm->no_prop_timeout_control, prop_control->last_period.tv_sec,
		prop_control->last_binder_counter, prop_control->total_fail,
		prop_control->max_gp, prop_control->max_get_prop_time.tv_sec,
		prop_control->max_get_prop_time.tv_nsec/1000000,
		prop_control->last_diff_time.tv_sec, gm->bootmode,
		gauge_get_int_property(gm, GAUGE_PROP_PTIM_RESIST));

	fg_drv_update_daemon(gm);

	prop_control->max_get_prop_time = ktime_to_timespec64(0);
	if (prop_control->end_get_prop_time == 0 &&
		prop_control->last_diff_time.tv_sec > prop_control->i2c_fail_th) {
		gp_number_to_name(gm, gp_name, prop_control->curr_gp);
		regmap_type = gauge_get_int_property(gm, GAUGE_PROP_REGMAP_TYPE);
		reg_type_to_name(gm, reg_type_name, regmap_type);

		bm_err(gm, "[%s_Error] get %s hang over 3 sec, time:%lld\n",
			reg_type_name, gp_name, prop_control->last_diff_time.tv_sec);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		if (!gm->disableGM30)
			aee_kernel_warning("BATTERY", "gauge get prop over 3 sec\n");
#endif
	}
	if (!gm->disableGM30 && prop_control->total_fail > 20) {
		regmap_type = gauge_get_int_property(gm,GAUGE_PROP_REGMAP_TYPE);
		reg_type_to_name(gm, reg_type_name, regmap_type);

		bm_err(gm, "[%s_Error] Binder last counter: %d, period: %lld", reg_type_name,
			prop_control->last_binder_counter, prop_control->last_period.tv_sec);
		for (i = 0; i < GAUGE_PROP_MAX; i++) {
			gp_number_to_name(gm, gp_name, i);
			bm_err(gm, "[%s_Error] %s, fail_counter: %d\n",
				reg_type_name, gp_name, prop_control->i2c_fail_counter[i]);
			prop_control->i2c_fail_counter[i] = 0;
		}

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning("BATTERY",  "gauge get prop fail case over 20 times\n");
#endif
	}

	gauge_coulomb_dump_list(gm);

	if ((car < gm->coulomb_minus.end-30 || car > gm->coulomb_plus.end+30) && is_battery_exist) {
		bm_err(gm, "coulomb service stop %ld %ld %d %d %d",
			car, gm->coulomb_minus.end, car > gm->coulomb_plus.end,
			car < gm->coulomb_minus.end, car > gm->coulomb_plus.end);
		wake_up_gauge_coulomb(gm);
	}

	if (gm->algo.active == true)
		battery_update(gm->bm);

	if (gm->log_level >= BMLOG_DEBUG_LEVEL)
		ktime = ktime_set(10, 0);
	else
		ktime = ktime_set(60, 0);

#if defined(CONFIG_MTK_NO_BAT_BOOT_SUPPORT)
	/* Factory bin & OB Mode(nobattery condition) */
	/* need regular update for vsys_to_vsoc_for_factory_mode */
#if IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_SEC_MTK_CHARGER)
	if (gm->f_mode == OB_MODE) {
		if (gm->disableGM30)
			battery_update(gm->bm);
		/* set ktime 10s only for Factory bin & OB Mode(nobattery FG disable condition) */
		ktime = ktime_set(10, 0);
	}
#endif
#endif

	hrtimer_start(&gm->fg_hrtimer, ktime, HRTIMER_MODE_REL);
}

int battery_update_routine(void *arg)
{
	struct mtk_battery *gm = (struct mtk_battery *)arg;
	int ret = 0, retry = 0;

	while (1) {
		ret = wait_event_interruptible(gm->wait_que,
			(gm->fg_update_flag > 0) && !gm->in_sleep);
		if (ret < 0) {
			retry++;
			if (retry < 0xFFFFFFFF)
				continue;
			else {
				bm_err(gm, "%s something wrong retry: %d\n", __func__, retry);
				break;
			}
		}
		retry = 0;
		mutex_lock(&gm->fg_update_lock);
		if (gm->in_sleep)
			goto in_sleep;
		gm->fg_update_flag = 0;
		fg_drv_update_hw_status(gm);
in_sleep:
		mutex_unlock(&gm->fg_update_lock);
	}

	return 0;
}

static int system_pm_notify(struct notifier_block *nb,
			    unsigned long mode, void *_unused)
{
	struct mtk_battery *gm =
			container_of(nb, struct mtk_battery, pm_nb);

	bm_err(gm, "%s mode2:%lu\n", __func__, mode);

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:

		if (!mutex_trylock(&gm->fg_update_lock))
			return NOTIFY_BAD;
		gm->in_sleep = true;
		mutex_unlock(&gm->fg_update_lock);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&gm->fg_update_lock);
		gm->in_sleep = false;
		mutex_unlock(&gm->fg_update_lock);
		wake_up(&gm->wait_que);
		wake_up(&gm->irq_ctrl.wait_que);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

void fg_update_routine_wakeup(struct mtk_battery *gm)
{
	gm->fg_update_flag = 1;
	wake_up(&gm->wait_que);
}

enum hrtimer_restart fg_drv_thread_hrtimer_func(struct hrtimer *timer)
{
	struct mtk_battery *gm;

	gm = container_of(timer,
		struct mtk_battery, fg_hrtimer);
	fg_update_routine_wakeup(gm);
	return HRTIMER_NORESTART;
}

void fg_drv_thread_hrtimer_init(struct mtk_battery *gm)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	hrtimer_init(&gm->fg_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gm->fg_hrtimer.function = fg_drv_thread_hrtimer_func;
	hrtimer_start(&gm->fg_hrtimer, ktime, HRTIMER_MODE_REL);
}

/* ============================================================ */
/* alarm timer handler */
/* ============================================================ */
static void tracking_timer_work_handler(struct work_struct *data)
{
	struct mtk_battery *gm;

	gm = container_of(data,
		struct mtk_battery, tracking_timer_work);
	bm_debug(gm, "[%s] into\n", __func__);
	wakeup_fg_algo(gm, FG_INTR_FG_TIME);
}

static enum alarmtimer_restart tracking_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery *gm;

	gm = container_of(alarm,
		struct mtk_battery, tracking_timer);
	bm_debug(gm, "[%s] into\n", __func__);
	schedule_work(&gm->tracking_timer_work);
	return ALARMTIMER_NORESTART;
}

static void one_percent_timer_work_handler(struct work_struct *data)
{
	struct mtk_battery *gm;

	gm = container_of(data,
		struct mtk_battery, one_percent_timer_work);
	bm_debug(gm, "[%s] into\n", __func__);
	wakeup_fg_algo_cmd(gm, FG_INTR_FG_TIME, 0, 1);
}

static enum alarmtimer_restart one_percent_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery *gm;

	gm = container_of(alarm,
		struct mtk_battery, one_percent_timer);
	bm_debug(gm, "[%s] into\n", __func__);
	schedule_work(&gm->one_percent_timer_work);
	return ALARMTIMER_NORESTART;
}

static void sw_uisoc_timer_work_handler(struct work_struct *data)
{
	struct mtk_battery *gm;

	gm = container_of(data,
		struct mtk_battery, one_percent_timer_work);
	bm_debug(gm, "[%s] %d %d\n", __func__,
		gm->soc, gm->ui_soc);
	if (gm->soc > gm->ui_soc)
		wakeup_fg_algo(gm, FG_INTR_BAT_INT2_HT);
	else if (gm->soc < gm->ui_soc)
		wakeup_fg_algo(gm, FG_INTR_BAT_INT2_LT);
}

static enum alarmtimer_restart sw_uisoc_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery *gm;

	gm = container_of(alarm,
		struct mtk_battery, sw_uisoc_timer);
	bm_debug(gm, "[%s] into\n", __func__);
	schedule_work(&gm->sw_uisoc_timer_work);
	return ALARMTIMER_NORESTART;
}

int battery_psy_init(struct platform_device *pdev)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gm = devm_kzalloc(&pdev->dev, sizeof(*gm), GFP_KERNEL);
	if (!gm)
		return -ENOMEM;
	bm_err(gm, "[%s] into\n", __func__);

	gauge = dev_get_drvdata(&pdev->dev);
	gauge->gm = gm;
	gm->gauge = gauge;
	mutex_init(&gm->ops_lock);
	gmb = gm;

	return 0;
}

void fg_check_bootmode(struct device *dev,
	struct mtk_battery *gm)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		bm_err(gm, "%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			bm_err(gm, "%s: failed to get atag,boot\n", __func__);
		else {
			bm_err(gm, "%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			gm->bootmode = tag->bootmode;
			gm->boottype = tag->boottype;
		}
	}
}

int fg_check_lk_swocv(struct device *dev,
	struct mtk_battery *gm)
{
	struct device_node *boot_node = NULL;
	int len = 0;
	char temp[10];
	int *prop;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		bm_err(gm, "%s: failed to get boot mode phandle\n", __func__);
	else {
		prop = (void *)of_get_property(
			boot_node, "atag,fg_swocv_v", &len);

		if (prop == NULL) {
			bm_err(gm, "fg_swocv_v prop == NULL, len=%d\n", len);
		} else {
			snprintf(temp, (len + 1), "%s", (char *)prop);
			if (kstrtoint(temp, 10, &gm->ptim_lk_v))
				return -EINVAL;

			bm_err(gm, "temp %s gm->ptim_lk_v=%d\n",
					temp, gm->ptim_lk_v);
		}

		prop = (void *)of_get_property(
			boot_node, "atag,fg_swocv_i", &len);

		if (prop == NULL) {
			bm_err(gm, "fg_swocv_i prop == NULL, len=%d\n", len);
		} else {
			snprintf(temp, (len + 1), "%s", (char *)prop);
			if (kstrtoint(temp, 10, &gm->ptim_lk_i))
				return -EINVAL;

			bm_err(gm, "temp %s gm->ptim_lk_i=%d\n",
					temp, gm->ptim_lk_i);
		}
		prop = (void *)of_get_property(
			boot_node, "atag,shutdown_time", &len);

		if (prop == NULL) {
			bm_err(gm, "shutdown_time prop == NULL, len=%d\n", len);
		} else {
			snprintf(temp, (len + 1), "%s", (char *)prop);
			if (kstrtoint(temp, 10, &gm->pl_shutdown_time))
				return -EINVAL;

			bm_err(gm, "temp %s gm->pl_shutdown_time=%d\n",
					temp, gm->pl_shutdown_time);
		}
	}

	bm_err(gm, "%s swocv_v:%d swocv_i:%d shutdown_time:%d\n",
		__func__, gm->ptim_lk_v, gm->ptim_lk_i, gm->pl_shutdown_time);

	return 0;
}

int fg_prop_control_init(struct mtk_battery *gm)
{
	struct property_control	*prop_control;

	prop_control = &gm->prop_control;

	memset(prop_control->last_prop_update_time, 0,
		sizeof(prop_control->last_prop_update_time));
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_EXIST] =
		PROP_BATTERY_EXIST_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_CURRENT] =
		PROP_BATTERY_CURRENT_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_AVERAGE_CURRENT] =
		PROP_AVERAGE_CURRENT_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_VOLTAGE] =
		PROP_BATTERY_VOLTAGE_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC] =
		PROP_BATTERY_TEMPERATURE_ADC_TIMEOUT * 100;
	prop_control->i2c_fail_th = I2C_FAIL_TH;
	prop_control->binder_counter = 0;
	return 0;
}

int battery_init(struct platform_device *pdev)
{
	int ret = 0;
	bool b_recovery_mode = 0;
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;
	gm->fixed_bat_tmp = 0xffff;
	gm->tmp_table = fg_temp_table;
	gm->log_level = BMLOG_ERROR_LEVEL;
	gm->sw_iavg_gap = 3000;
	gm->in_sleep = false;

	mutex_init(&gm->fg_update_lock);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_SEC_MTK_CHARGER)
	if (!f_mode)
		gm->f_mode = NO_MODE;
	else if ((strncmp(f_mode, "OB", 2) == 0) || (strncmp(f_mode, "DL", 2) == 0))
		gm->f_mode = OB_MODE;
	else if (strncmp(f_mode, "IB", 2) == 0)
		gm->f_mode = IB_MODE;
	else
		gm->f_mode = NO_MODE;
	pr_info("[BAT] %s: f_mode: %s\n", __func__, BOOT_MODE_STRING[gm->f_mode]);
#endif

	init_waitqueue_head(&gm->wait_que);

	fg_check_bootmode(&pdev->dev, gm);
	fg_check_lk_swocv(&pdev->dev, gm);
	fg_prop_control_init(gm);
	fg_check_bat_type(pdev, gm);
	fg_custom_init_from_header(gm);
	fg_custom_init_from_dts(pdev, gm);
	mtk_irq_thread_init(gm);
	gauge_coulomb_service_init(gm);
	gm->coulomb_plus.callback = fg_coulomb_int_h_handler;
	gauge_coulomb_consumer_init(&gm->coulomb_plus, &pdev->dev, "car+1%");
	gm->coulomb_minus.callback = fg_coulomb_int_l_handler;
	gauge_coulomb_consumer_init(&gm->coulomb_minus, &pdev->dev, "car-1%");

	gauge_coulomb_consumer_init(&gm->uisoc_plus, &pdev->dev, "uisoc+1%");
	gm->uisoc_plus.callback = fg_bat_int2_h_handler;
	gauge_coulomb_consumer_init(&gm->uisoc_minus, &pdev->dev, "uisoc-1%");
	gm->uisoc_minus.callback = fg_bat_int2_l_handler;



	alarm_init(&gm->tracking_timer, ALARM_BOOTTIME,
		tracking_timer_callback);
	INIT_WORK(&gm->tracking_timer_work, tracking_timer_work_handler);
	alarm_init(&gm->one_percent_timer, ALARM_BOOTTIME,
		one_percent_timer_callback);
	INIT_WORK(&gm->one_percent_timer_work, one_percent_timer_work_handler);

	alarm_init(&gm->sw_uisoc_timer, ALARM_BOOTTIME,
		sw_uisoc_timer_callback);
	INIT_WORK(&gm->sw_uisoc_timer_work, sw_uisoc_timer_work_handler);


	kthread_run(battery_update_routine, gm, "%s", gm->gauge->name);
	gm->pm_nb.notifier_call = system_pm_notify;
	ret = register_pm_notifier(&gm->pm_nb);
	if (ret)
		bm_err(gm, "%s failed to register system pm notify\n", __func__);

	fg_drv_thread_hrtimer_init(gm);
	battery_sysfs_create_group(gm);
	gm->battery_sysfs = battery_sysfs_field_tbl;

	/* for gauge hal hw ocv */
	gm->battery_temp = force_get_tbat(gm, true);
	//mtk_power_misc_init(gm);

	ret = mtk_battery_daemon_init(pdev);
	b_recovery_mode = is_recovery_mode(gm);
	gm->is_probe_done = true;

	if (ret == 0 && b_recovery_mode == 0)
		bm_err(gm, "[%s]: daemon mode DONE\n", __func__);
	else {
		gm->algo.active = true;
		battery_algo_init(gm);
		bm_err(gm, "[%s]: enable Kernel mode Gauge\n", __func__);
	}

	return 0;
}
