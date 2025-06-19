/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#ifndef __MTK_BATTERY_INTF_H__
#define __MTK_BATTERY_INTF_H__

#include <linux/alarmtimer.h>
#include <linux/atomic.h>
#include <linux/extcon.h>
#include <linux/hrtimer.h>
#include <linux/nvmem-consumer.h>
#include <linux/power_supply.h>
#include <linux/sysfs.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/ctype.h>
#include "mtk_gauge.h"
#include "mtk_battery_daemon.h"



#define NETLINK_FGD 26
#define UNIT_TRANS_10	10
#define UNIT_TRANS_100	100
#define UNIT_TRANS_1000	1000
#define UNIT_TRANS_60	60
#define MAX_R_TABLE		100
#define ACTIVE_R_TABLE	21
#define MAX_TABLE		10
#define MAX_CHARGE_RDC 5

#define BMLOG_ERROR_LEVEL   3
#define BMLOG_WARNING_LEVEL 4
#define BMLOG_NOTICE_LEVEL  5
#define BMLOG_INFO_LEVEL    6
#define BMLOG_DEBUG_LEVEL   7
#define BMLOG_TRACE_LEVEL   8

/*0.1s*/
#define PROP_BATTERY_EXIST_TIMEOUT 10
#define	PROP_BATTERY_CURRENT_TIMEOUT 5
#define	PROP_AVERAGE_CURRENT_TIMEOUT 10
#define	PROP_BATTERY_VOLTAGE_TIMEOUT 10
#define	PROP_BATTERY_TEMPERATURE_ADC_TIMEOUT 10
#define MAX_PROP_NAME_LEN 50

#define BMLOG_DEFAULT_LEVEL BMLOG_DEBUG_LEVEL

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define FG_BATT_DUMP_SIZE 128
#endif

#define bm_err(gm, fmt, args...)   \
do {\
	if (bat_get_debug_level(gm) >= BMLOG_ERROR_LEVEL) {	\
		pr_notice("%s:" fmt, bat_get_gauge_name(gm), ##args);	\
	}	\
} while (0)

#define bm_warn(gm, fmt, args...)   \
do {\
	if (bat_get_debug_level(gm) >= BMLOG_WARNING_LEVEL) {	\
		pr_notice("%s:" fmt, bat_get_gauge_name(gm), ##args);	\
	}	\
} while (0)

#define bm_notice(gm, fmt, args...)   \
do {\
	if (bat_get_debug_level(gm) >= BMLOG_NOTICE_LEVEL) {	\
		pr_notice("%s:" fmt, bat_get_gauge_name(gm), ##args);	\
	}	\
} while (0)

#define bm_info(gm, fmt, args...)   \
do {\
	if (bat_get_debug_level(gm) >= BMLOG_INFO_LEVEL) {	\
		pr_notice("%s:" fmt, bat_get_gauge_name(gm), ##args);	\
	}	\
} while (0)

#define bm_debug(gm, fmt, args...)   \
do {\
	if (bat_get_debug_level(gm) >= BMLOG_DEBUG_LEVEL) {	\
		pr_notice("%s:" fmt, bat_get_gauge_name(gm), ##args);	\
	}	\
} while (0)

#define bm_trace(gm, fmt, args...)\
do {\
	if (bat_get_debug_level(gm) >= BMLOG_TRACE_LEVEL) {	\
		pr_notice("%s:" fmt, bat_get_gauge_name(gm), ##args);	\
	}	\
} while (0)

#define bm_annotation(gm, fmt, args...)\
do {\
} while (0)

#define BAT_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr	= __ATTR(_name, 0644, bat_sysfs_show, bat_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}

#define BAT_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, bat_sysfs_show, bat_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}

#define BAT_SYSFS_FIELD_WO(_name, _prop)	\
{								   \
	.attr	= __ATTR(_name, 0200, bat_sysfs_show, bat_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
}
enum manager_cmd {
	MANAGER_WAKE_UP_ALGO,
	MANAGER_NOTIFY_CHR_FULL,
	MANAGER_SW_BAT_CYCLE_ACCU,
	MANAGER_DYNAMIC_CV,
	MANAGER_DISABLE_FG,
};

enum battery_property {
	BAT_PROP_TEMPERATURE,
	BAT_PROP_COULOMB_INT_GAP,
	BAT_PROP_UISOC_HT_INT_GAP,
	BAT_PROP_UISOC_LT_INT_GAP,
	BAT_PROP_ENABLE_UISOC_HT_INT,
	BAT_PROP_ENABLE_UISOC_LT_INT,
	BAT_PROP_UISOC,
	BAT_PROP_DISABLE,
	BAT_PROP_INIT_DONE,
	BAT_PROP_FG_RESET,
	BAT_PROP_LOG_LEVEL,
	BAT_PROP_WAKEUP_FG_ALGO,
};

enum property_control_data {
	CONTROL_GAUGE_PROP_BATTERY_EXIST,
	CONTROL_GAUGE_PROP_BATTERY_CURRENT,
	CONTROL_GAUGE_PROP_AVERAGE_CURRENT,
	CONTROL_GAUGE_PROP_BATTERY_VOLTAGE,
	CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
	CONTROL_MAX,
};

#define I2C_FAIL_TH 3
struct property_control {
	int val[CONTROL_MAX];
	ktime_t last_prop_update_time[CONTROL_MAX];
	int diff_time_th[CONTROL_MAX];

	ktime_t start_get_prop_time;
	ktime_t end_get_prop_time;
	struct timespec64 max_get_prop_time;
	struct timespec64 last_period;
	struct timespec64 last_diff_time;
	int curr_gp;
	int max_gp;

	int i2c_fail_th;
	int i2c_fail_counter[GAUGE_PROP_MAX];
	int total_fail;
	int binder_counter;
	int last_binder_counter;
	ktime_t pre_log_time;
};
struct battery_data {
	struct power_supply_desc psd;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	struct power_supply *chg_psy;
	struct notifier_block battery_nb;
	int bat_status;
	int bat_health;
	int bat_present;
	int bat_technology;
	int bat_capacity;
	/* Add for Battery Service */
	int bat_batt_vol;
	int bat_batt_temp;
};

struct VersionControl {
	int androidVersion;
	int daemon_cmds;
	int kernel_cmds;
	int custom_data_len;
	int custom_table_len;
};

#define AFW_SERVICE_ID	0x10000000
#define FG_SERVICE_ID	0x00000000

enum afw_sys_cmds{
	AFW_CMD_SET_PID = AFW_SERVICE_ID,
	AFW_CMD_PRINT_LOG,
};

enum fg_daemon_cmds {
	FG_DAEMON_CMD_PRINT_LOG = FG_SERVICE_ID,
	FG_DAEMON_CMD_SET_DAEMON_PID,
	FG_DAEMON_CMD_GET_CUSTOM_SETTING,
	FG_DAEMON_CMD_GET_CUSTOM_TABLE,
	FG_DAEMON_CMD_SEND_CUSTOM_TABLE,
	FG_DAEMON_CMD_SEND_VERSION_CONTROL,
	FG_DAEMON_CMD_GET_VERSION_CONTROL,

	FG_DAEMON_CMD_IS_BAT_EXIST,
	FG_DAEMON_CMD_GET_INIT_FLAG,
	FG_DAEMON_CMD_SET_INIT_FLAG,
	FG_DAEMON_CMD_NOTIFY_DAEMON,
	FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION,
	FG_DAEMON_CMD_FGADC_RESET,
	FG_DAEMON_CMD_GET_TEMPERTURE,
	FG_DAEMON_CMD_GET_RAC,
	FG_DAEMON_CMD_GET_PTIM_VBAT,
	FG_DAEMON_CMD_GET_PTIM_I,
	FG_DAEMON_CMD_IS_CHARGER_EXIST,
	FG_DAEMON_CMD_GET_HW_OCV,
	FG_DAEMON_CMD_GET_FG_HW_CAR,
	FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP,
	FG_DAEMON_CMD_SET_FG_BAT_TMP_GAP,
	FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP,
	FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT,
	FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP,
	FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT,
	FG_DAEMON_CMD_IS_BAT_PLUGOUT,
	FG_DAEMON_CMD_IS_BAT_CHARGING,
	FG_DAEMON_CMD_GET_CHARGER_STATUS,
	FG_DAEMON_CMD_SET_SW_OCV,
	FG_DAEMON_CMD_GET_SHUTDOWN_DURATION_TIME,
	FG_DAEMON_CMD_GET_BAT_PLUG_OUT_TIME,
	FG_DAEMON_CMD_GET_IS_FG_INITIALIZED,
	FG_DAEMON_CMD_SET_IS_FG_INITIALIZED,
	FG_DAEMON_CMD_SET_FG_RESET_RTC_STATUS,
	FG_DAEMON_CMD_IS_HWOCV_UNRELIABLE,
	FG_DAEMON_CMD_GET_FG_CURRENT_AVG,
	FG_DAEMON_CMD_SET_FG_TIME,
	FG_DAEMON_CMD_GET_FG_TIME,
	FG_DAEMON_CMD_GET_ZCV,
	FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_CNT,
	FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_DLTV,
	FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_C_DLTV,
	FG_DAEMON_CMD_SET_NAG_ZCV,
	FG_DAEMON_CMD_SET_NAG_ZCV_EN,
	FG_DAEMON_CMD_SET_NAG_C_DLTV,
	FG_DAEMON_CMD_SET_ZCV_INTR,
	FG_DAEMON_CMD_SET_FG_QUSE,
	FG_DAEMON_CMD_SET_FG_RESISTANCE,/*remove*/
	FG_DAEMON_CMD_SET_FG_DC_RATIO,
	FG_DAEMON_CMD_SET_BATTERY_CYCLE_THRESHOLD,
	FG_DAEMON_CMD_SOFF_RESET,
	FG_DAEMON_CMD_NCAR_RESET,
	FG_DAEMON_CMD_GET_IMIX,
	FG_DAEMON_CMD_GET_AGING_FACTOR_CUST,
	FG_DAEMON_CMD_GET_D0_C_SOC_CUST,
	FG_DAEMON_CMD_GET_D0_V_SOC_CUST,
	FG_DAEMON_CMD_GET_UISOC_CUST,
	FG_DAEMON_CMD_IS_KPOC,
	FG_DAEMON_CMD_GET_NAFG_VBAT,
	FG_DAEMON_CMD_GET_HW_INFO,
	FG_DAEMON_CMD_SET_KERNEL_SOC,
	FG_DAEMON_CMD_SET_KERNEL_UISOC,
	FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT,
	FG_DAEMON_CMD_SET_BAT_PLUGOUT_INTR,
	FG_DAEMON_CMD_SET_IAVG_INTR,
	FG_DAEMON_CMD_SET_FG_SHUTDOWN_COND,
	FG_DAEMON_CMD_GET_FG_SHUTDOWN_COND,
	FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT,
	FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT,
	FG_DAEMON_CMD_SET_FG_VBAT_L_TH,
	FG_DAEMON_CMD_SET_FG_VBAT_H_TH,
	FG_DAEMON_CMD_SET_CAR_TUNE_VALUE,
	FG_DAEMON_CMD_GET_FG_CURRENT_IAVG_VALID,
	FG_DAEMON_CMD_GET_RTC_UI_SOC,
	FG_DAEMON_CMD_SET_RTC_UI_SOC,
	FG_DAEMON_CMD_GET_CON0_SOC,
	FG_DAEMON_CMD_SET_CON0_SOC,
	FG_DAEMON_CMD_GET_NVRAM_FAIL_STATUS,
	FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS,
	FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP,
	FG_DAEMON_CMD_IS_BATTERY_CYCLE_RESET,
	FG_DAEMON_CMD_GET_RTC_TWO_SEC_REBOOT,
	FG_DAEMON_CMD_GET_RTC_INVALID,
	FG_DAEMON_CMD_GET_VBAT,
	FG_DAEMON_CMD_GET_DISABLE_NAFG,
	FG_DAEMON_CMD_DUMP_LOG,

	FG_DAEMON_CMD_GET_SHUTDOWN_CAR,
	FG_DAEMON_CMD_GET_NCAR,
	FG_DAEMON_CMD_GET_CURR_1,
	FG_DAEMON_CMD_GET_CURR_2,
	FG_DAEMON_CMD_GET_REFRESH,
	FG_DAEMON_CMD_GET_IS_AGING_RESET,
	FG_DAEMON_CMD_SET_SOC,
	FG_DAEMON_CMD_SET_C_D0_SOC,
	FG_DAEMON_CMD_SET_V_D0_SOC,
	FG_DAEMON_CMD_SET_C_SOC,
	FG_DAEMON_CMD_SET_V_SOC,
	FG_DAEMON_CMD_SET_QMAX_T_AGING,
	FG_DAEMON_CMD_SET_SAVED_CAR,
	FG_DAEMON_CMD_SET_AGING_FACTOR,
	FG_DAEMON_CMD_SET_QMAX,
	FG_DAEMON_CMD_SET_BAT_CYCLES,
	FG_DAEMON_CMD_SET_NCAR,
	FG_DAEMON_CMD_SET_OCV_MAH,
	FG_DAEMON_CMD_SET_OCV_VTEMP,
	FG_DAEMON_CMD_SET_OCV_SOC,
	FG_DAEMON_CMD_SET_CON0_SOFF_VALID,
	FG_DAEMON_CMD_SET_AGING_INFO,
	FG_DAEMON_CMD_GET_SOC_DECIMAL_RATE,
	FG_DAEMON_CMD_GET_DIFF_SOC_SET,
	FG_DAEMON_CMD_SET_ZCV_INTR_EN,
	FG_DAEMON_CMD_GET_IS_FORCE_FULL,
	FG_DAEMON_CMD_GET_ZCV_INTR_CURR,
	FG_DAEMON_CMD_GET_CHARGE_POWER_SEL,
	FG_DAEMON_CMD_COMMUNICATION_INT,
	FG_DAEMON_CMD_SET_BATTERY_CAPACITY,
	FG_DAEMON_CMD_GET_BH_DATA,
	FG_DAEMON_CMD_SEND_DAEMON_DATA,
	FG_DAEMON_CMD_GET_SD_DATA,
	FG_DAEMON_CMD_SEND_SD_DATA,
	FG_DAEMON_CMD_SET_SELECT_ZCV,
	FG_DAEMON_CMD_SET_SHOW_AGING_FACTOR,

	FG_DAEMON_CMD_FROM_USER_NUMBER

}; //must sync to kernel: (battery_meter.c) FG_DAEMON_CTRL_CMD_FROM_USER

/* kernel cmd */
enum Fg_kernel_cmds {
	FG_KERNEL_CMD_NO_ACTION,
	FG_KERNEL_CMD_DUMP_REGULAR_LOG,
	FG_KERNEL_CMD_DISABLE_NAFG,
	FG_KERNEL_CMD_DUMP_LOG,
	FG_KERNEL_CMD_UISOC_UPDATE_TYPE,
	FG_KERNEL_CMD_CHANG_LOGLEVEL,
	FG_KERNEL_CMD_REQ_ALGO_DATA,
	FG_KERNEL_CMD_RESET_AGING_FACTOR,
	FG_KERNEL_CMD_BUILD_SEL_BATTEMP,
	FG_KERNEL_CMD_UPDATE_AVG_BATTEMP,
	FG_KERNEL_CMD_SAVE_DEBUG_PARAM,
	FG_KERNEL_CMD_REQ_CHANGE_AGING_DATA,
	FG_KERNEL_CMD_AG_LOG_TEST,
	FG_KERNEL_CMD_CHG_DECIMAL_RATE,
	FG_KERNEL_CMD_FORCE_BAT_TEMP,
	FG_KERNEL_CMD_SEND_BH_DATA,
	FG_KERNEL_CMD_GET_DYNAMIC_CV,
	FG_KERNEL_CMD_GET_DYNAMIC_GAUGE0,
	FG_KERNEL_CMD_SEND_SHUTDOWN_DATA,
	FG_KERNEL_CMD_GET_DYNAMIC_ZCV_TABLE,
	FG_KERNEL_CMD_REQ_CHANGE_BAT_CYCLE,
	FG_KERNEL_CMD_FROM_USER_NUMBER

};

enum {
	NORMAL = 0,
	OVERHEAT,
	SOC_ZERO_PERCENT,
	UISOC_ONE_PERCENT,
	LOW_BAT_VOLT,
	DLPT_SHUTDOWN,
	SHUTDOWN_1_TIME,
	SHUTDOWN_FACTOR_MAX
};

enum gauge_event {
	EVT_INT_CHR_FULL,
	EVT_INT_ZCV,
	EVT_INT_BAT_CYCLE,
	EVT_INT_IAVG,
	EVT_INT_BAT_PLUGOUT,
	EVT_INT_BAT_PLUGIN,
	EVT_INT_NAFG,
	EVT_INT_BAT_INT1_HT,
	EVT_INT_BAT_INT1_LT,
	EVT_INT_BAT_INT2_HT,
	EVT_INT_BAT_INT2_LT,
	EVT_INT_VBAT_L,
	EVT_INT_VBAT_H,
	EVT_INT_NAFG_CHECK,
	EVB_PERIODIC_CHECK,
	GAUGE_EVT_MAX
};


enum bm_psy_prop {
	CURRENT_NOW,
	CURRENT_AVG,
	VOLTAGE_NOW,
	TEMP,
	QMAX,
	QMAX_DESIGN,
};

enum charge_sel {
	CHARGE_NORMAL,
	CHARGE_R1,
	CHARGE_R2,
	CHARGE_R3,
	CHARGE_R4,
};

struct fuelgauge_charger_struct {
	int rdc[MAX_CHARGE_RDC];
};

struct fuelgauge_charge_pseudo100_s {
	int pseudo[MAX_CHARGE_RDC];
};


struct fuelgauge_profile_struct {
	int mah;
	unsigned short voltage;
	unsigned short resistance; /* Ohm*/
	int percentage;
	struct fuelgauge_charger_struct charge_r;
};

struct fuel_gauge_r_ratio_table {
	int battery_r_ratio_ma;
	int battery_r_ratio;
};

struct fuel_gauge_table {
	int temperature;
	int q_max;
	int q_max_h_current;
	int pseudo1;
	int pseudo100;
	int pmic_min_vol;
	int pon_iboot;
	int qmax_sys_vol;
	int shutdown_hl_zcv;
	int r_ratio_active_table;
	int charging_r_ratio_active_table;

	int size;
	struct fuelgauge_charge_pseudo100_s r_pseudo100;
	struct fuelgauge_profile_struct fg_profile[100];
	struct fuel_gauge_r_ratio_table fg_r_profile[MAX_R_TABLE];
	struct fuel_gauge_r_ratio_table fg_charging_r_profile[MAX_R_TABLE];
};

struct fuel_gauge_table_cust_temperture_table {
	int table_ID;
	int tb_size;
	int tb_construct_temp;
	struct fuelgauge_profile_struct fg_profile_node[100];
};

struct fuel_gauge_table_custom_data {
	/* cust_battery_meter_table.h */
	int active_table_number;
	int enable_r_ratio;
	int max_ratio_temp;
	int min_ratio_temp;
	int enable_charging_ratio;
	int max_charging_ratio_temp;
	int min_charging_ratio_temp;
	struct fuel_gauge_table fg_profile[MAX_TABLE];

	int temperature_tb0;
	int fg_profile_temperature_0_size;
	struct fuelgauge_profile_struct fg_profile_temperature_0[100];

	int temperature_tb1;
	int fg_profile_temperature_1_size;
	struct fuelgauge_profile_struct fg_profile_temperature_1[100];
};

struct fuel_gauge_custom_data {
	int versionID1;
	int versionID2;
	int versionID3;
	int hardwareVersion;

	int low_temp_mode;
	int low_temp_mode_temp;

	/* Qmax for battery  */
	int q_max_L_current;
	int q_max_H_current;
	int q_max_sys_voltage;

	int pseudo1_en;
	int pseudo100_en;
	int pseudo100_en_dis;
	int pseudo1_iq_offset;

	/* vboot related */
	int qmax_sel;
	int iboot_sel;
	int shutdown_system_iboot;

	/* multi temp gauge 0% */
	int multi_temp_gauge0;

	/* hw related */
	int car_tune_value;
	int fg_meter_resistance;
	int com_fg_meter_resistance;
	int curr_measure_20a;
	int unit_multiple;
	int r_fg_value;
	int com_r_fg_value;
	int mtk_chr_exist;

	/* Dynamic cv*/
	int dynamic_cv_factor;
	int charger_ieoc;

	/* Aging Compensation 1*/
	int aging_one_en;
	int aging1_update_soc;
	int aging1_load_soc;
	int aging4_update_soc;
	int aging4_load_soc;
	int aging5_update_soc;
	int aging5_load_soc;
	int aging6_update_soc;
	int aging6_load_soc;

	int aging_temp_diff;
	int aging_temp_low_limit;
	int aging_temp_high_limit;
	int aging_100_en;
	int difference_voltage_update;

	/* Aging Compensation 2*/
	int aging_two_en;

	/* Aging Compensation 3*/
	int aging_third_en;
	int aging_4_en;
	int aging_5_en;
	int aging_6_en;

	/* ui_soc */
	int diff_soc_setting;
	int keep_100_percent;
	int difference_full_cv;
	int diff_bat_temp_setting;
	int diff_bat_temp_setting_c;
	int discharge_tracking_time;
	int charge_tracking_time;
	int difference_fullocv_vth;
	int difference_fullocv_ith;
	int charge_pseudo_full_level;
	int over_discharge_level;
	int full_tracking_bat_int2_multiply;

	/* threshold */
	int hwocv_swocv_diff;	/* 0.1 mv */
	int hwocv_swocv_diff_lt;	/* 0.1 mv */
	int hwocv_swocv_diff_lt_temp;	/* degree */
	int hwocv_oldocv_diff;	/* 0.1 mv */
	int hwocv_oldocv_diff_chr;	/* 0.1 mv */
	int swocv_oldocv_diff;	/* 0.1 mv */
	int swocv_oldocv_diff_chr;	/* 0.1 mv */
	int vbat_oldocv_diff;	/* 0.1 mv */
	int tnew_told_pon_diff;	/* degree */
	int tnew_told_pon_diff2;/* degree */
	int pmic_shutdown_time;	/* secs */
	int bat_plug_out_time;	/* min */
	int swocv_oldocv_diff_emb;	/* 0.1 mv */
	int vir_oldocv_diff_emb;	/* 0.1 mv */
	int vir_oldocv_diff_emb_lt;
	int vir_oldocv_diff_emb_tmp;

	/* fgc & fgv threshold */
	int difference_fgc_fgv_th1;
	int difference_fgc_fgv_th2;
	int difference_fgc_fgv_th3;
	int difference_fgc_fgv_th_soc1;
	int difference_fgc_fgv_th_soc2;
	int nafg_time_setting;
	int nafg_ratio;
	int nafg_ratio_en;
	int nafg_ratio_tmp_thr;
	int nafg_resistance;

	/* mode select */
	int pmic_shutdown_current;
	int pmic_shutdown_sw_en;
	int force_vc_mode;
	int embedded_sel;
	int loading_1_en;
	int loading_2_en;
	int diff_iavg_th;

	/* ADC resister */
	int r_bat_sense;	/*is it used?*/
	int r_i_sense;	/*is it used?*/
	int r_charger_1;
	int r_charger_2;

	/* pre_tracking */
	int fg_pre_tracking_en;
	int vbat2_det_time;
	int vbat2_det_counter;
	int vbat2_det_voltage1;
	int vbat2_det_voltage2;
	int vbat2_det_voltage3;

	int shutdown_1_time;
	int shutdown_gauge0;
	int shutdown_gauge1_xmins;
	int shutdown_gauge1_mins;
	int shutdown_gauge0_voltage;
	int shutdown_gauge1_vbat_en;
	int shutdown_gauge1_vbat;

	/* ZCV update */
	int zcv_suspend_time;
	int sleep_current_avg;
	int zcv_com_vol_limit;

	int dc_ratio_sel;
	int dc_r_cnt;

	int pseudo1_sel;

	/* using current to limit uisoc in 100% case */
	int ui_full_limit_en;
	int ui_full_limit_soc0;
	int ui_full_limit_ith0;
	int ui_full_limit_soc1;
	int ui_full_limit_ith1;
	int ui_full_limit_soc2;
	int ui_full_limit_ith2;
	int ui_full_limit_soc3;
	int ui_full_limit_ith3;
	int ui_full_limit_soc4;
	int ui_full_limit_ith4;
	int ui_full_limit_time;

	int ui_full_limit_fc_soc0;
	int ui_full_limit_fc_ith0;
	int ui_full_limit_fc_soc1;
	int ui_full_limit_fc_ith1;
	int ui_full_limit_fc_soc2;
	int ui_full_limit_fc_ith2;
	int ui_full_limit_fc_soc3;
	int ui_full_limit_fc_ith3;
	int ui_full_limit_fc_soc4;
	int ui_full_limit_fc_ith4;

	/* using voltage to limit uisoc in 1% case */
	int ui_low_limit_en;
	int ui_low_limit_soc0;
	int ui_low_limit_vth0;
	int ui_low_limit_soc1;
	int ui_low_limit_vth1;
	int ui_low_limit_soc2;
	int ui_low_limit_vth2;
	int ui_low_limit_soc3;
	int ui_low_limit_vth3;
	int ui_low_limit_soc4;
	int ui_low_limit_vth4;
	int ui_low_limit_time;

	/* moving average bat_temp */
	int moving_battemp_en;
	int moving_battemp_thr;

	int d0_sel;
	int dod_init_sel;
	int aging_sel;
	int fg_tracking_current;
	int fg_tracking_current_iboot_en;
	int ui_fast_tracking_en;
	int ui_fast_tracking_gap;
	int bat_par_i;
	int c_old_d0;
	int v_old_d0;
	int c_soc;
	int v_soc;
	int ui_old_soc;
	int dlpt_ui_remap_en;

	int aging_factor_min;
	int aging_factor_diff;
	int keep_100_percent_minsoc;
	int battery_tmp_to_disable_gm30;
	int battery_tmp_to_disable_nafg;
	int battery_tmp_to_enable_nafg;
	int disable_nafg;

	int zcv_car_gap_percentage;
	int uisoc_update_type;

	/* boot status */
	int pl_charger_status;
	int power_on_car_chr;
	int power_on_car_nochr;
	int shutdown_car_ratio;

	/* battery health */
	int bat_bh_en;
	int aging_diff_max_threshold;
	int aging_diff_max_level;
	int aging_factor_t_min;
	int cycle_diff;
	int aging_count_min;
	int default_score;
	int default_score_quantity;
	int fast_cycle_set;
	int level_max_change_bat;
	int diff_max_change_bat;
	int aging_tracking_start;
	int max_aging_data;
	int max_fast_data;
	int fast_data_threshold_score;
	int show_aging_period;
	int min_uisoc_at_kpoc;

	/* log_level */
	int daemon_log_level;
	int record_log;

	/* shutdown jumping*/
	int low_tracking_jump;
	int pre_tracking_jump;
	int last_mode_reset;
	int pre_tracking_soc_reset;
};

struct fgd_cmd_param_t_custom {
	struct fuel_gauge_custom_data fg_cust_data;
	struct fuel_gauge_table_custom_data fg_table_cust_data;
};

struct fg_temp {
	signed int BatteryTemp;
	signed int TemperatureR;
};

/* coulomb service */
struct gauge_consumer {
	char *name;
	struct device *dev;
	long start;
	long end;
	int variable;

	int (*callback)(struct mtk_battery *gm,
		struct gauge_consumer *consumer);
	struct list_head list;
};

struct mtk_coulomb_service {
	struct mtk_battery *gm;
	struct list_head coulomb_head_plus;
	struct list_head coulomb_head_minus;
	struct mutex coulomb_lock;
	struct mutex hw_coulomb_lock;
	unsigned long reset_coulomb;
	spinlock_t slock;
	struct wakeup_source *wlock;
	wait_queue_head_t wait_que;
	bool coulomb_thread_timeout;
	atomic_t in_sleep;
	struct notifier_block pm_nb;
	int fgclog_level;
	int pre_coulomb;
	bool init;
	char name[20];
};

struct battery_temperature_table {
	int type;
	unsigned int rbat_pull_up_r;
	unsigned int rbat_pull_up_volt;
	unsigned int bif_ntc_r;
};

enum Fg_interrupt_flags {
	FG_INTR_0 = 0,
	FG_INTR_TIMER_UPDATE  = 1,
	FG_INTR_BAT_CYCLE = 2,
	FG_INTR_CHARGER_OUT = 4,
	FG_INTR_CHARGER_IN = 8,
	FG_INTR_FG_TIME = 16,
	FG_INTR_BAT_INT1_HT = 32,
	FG_INTR_BAT_INT1_LT = 64,
	FG_INTR_BAT_INT2_HT = 128,
	FG_INTR_BAT_INT2_LT = 256,
	FG_INTR_BAT_TMP_HT = 512,
	FG_INTR_BAT_TMP_LT = 1024,
	FG_INTR_BAT_TIME_INT = 2048,
	FG_INTR_NAG_C_DLTV = 4096,
	FG_INTR_FG_ZCV = 8192,
	FG_INTR_SHUTDOWN = 16384,
	FG_INTR_RESET_NVRAM = 32768,
	FG_INTR_BAT_PLUGOUT = 65536,
	FG_INTR_IAVG = 0x20000,
	FG_INTR_VBAT2_L = 0x40000,
	FG_INTR_VBAT2_H = 0x80000,
	FG_INTR_CHR_FULL = 0x100000,
	FG_INTR_DLPT_SD = 0x200000,
	FG_INTR_BAT_TMP_C_HT = 0x400000,
	FG_INTR_BAT_TMP_C_LT = 0x800000,
	FG_INTR_BAT_INT1_CHECK = 0x1000000,
	FG_INTR_KERNEL_CMD = 0x2000000,
	FG_INTR_BAT_INT2_CHECK = 0x4000000,
	FG_INTR_BAT_PLUGIN = 0x8000000,
	FG_INTR_LAST_MODE = 0x10000000,
};

struct mtk_battery_algo {
	bool active;
	int last_temp;
	int T_table;
	int T_table_c;

	/*soc only follows c_soc */
	int soc;

	/* tempeture related*/
	int fg_bat_tmp_c_gap;

	/* CSOC related */
	int fg_c_d0_ocv;
	int fg_c_d0_dod;
	int fg_c_d0_soc;
	int fg_c_dod;
	int fg_c_soc;
	int fg_bat_int1_gap;
	int prev_car_bat0;

	/* UI related */
	int rtc_ui_soc;
	int ui_soc;
	int ui_d0_soc;
	int vboot;
	int vboot_c;
	int qmax_t_0ma; /* 0.1mA */
	int qmax_t_0ma_tb1; /* 0.1mA */
	int qmax_t_0ma_h;
	int qmax_t_Nma_h;
	int quse_tb0;
	int quse_tb1;
	int car;
	int batterypseudo1_h;
	int batterypseudo100;
	int shutdown_hl_zcv;
	int qmax_t_0ma_h_tb1;
	int qmax_t_Nma_h_tb1;
	int qmax_t_aging;
	int aging_factor;
	int fg_resistance_bat;
	int DC_ratio;
	int ht_gap;
	int lt_gap;
	int low_tracking_enable;
	int fg_vbat2_lt;
	int fg_vbat2_ht;

	/* Interrupt control */
	int uisoc_ht_en;
	int uisoc_lt_en;
};

struct simulator_log {
	int bat_full_int;
	int dlpt_sd_int;
	int chr_in_int;
	int zcv_int;
	int zcv_current;
	int zcv;
	int chr_status;
	int ptim_bat;
	int ptim_cur;
	int ptim_is_charging;

	int phone_state;
	int ps_system_time;
	unsigned long long ps_logtime;

	int nafg_zcv;

	/* initial */
	int fg_reset;
	int car_diff;

	/* rtc */
	int is_gauge_initialized;
	int rtc_ui_soc;
	int is_rtc_invalid;
	int is_bat_plugout;
	int bat_plugout_time;

	/* system info */
	int twosec_reboot;
	int pl_charging_status;
	int moniter_plchg_status;
	int bat_plug_status;
	int is_nvram_fail_mode;
	int con0_soc;

};

enum irq_handler_flag {
	VBAT_L_FLAG,
	VBAT_H_FLAG,
	BAT_TEMP_FLAG,
	IAVG_L_FLAG,
	IAVG_H_FLAG,
	CYCLE_FLAG,
	COULOMB_FLAG,
	ZCV_FLAG,
	NAFG_FLAG,
	BAT_PLUG_FLAG,
	BAT_PLUGIN_FLAG,
	NUMBER_IRQ_HANDLER,
};

struct irq_controller {
	wait_queue_head_t  wait_que;
	spinlock_t irq_lock;
	int irq_flags;
	int do_irq;
};
/* ============================================================ */
/* power misc related */
/* ============================================================ */
#define BAT_VOLTAGE_LOW_BOUND 3400
#define BAT_VOLTAGE_HIGH_BOUND 3450
#define LOW_TMP_BAT_VOLTAGE_LOW_BOUND 3250 /* ALPS05455882 pre-low tracking properly */
#define SHUTDOWN_TIME 40
#define AVGVBAT_ARRAY_SIZE 5 /* ALPS07387291 reduce avg size for fast shutdown tracking */
#define INIT_VOLTAGE 3450
#define BATTERY_SHUTDOWN_TEMPERATURE 60
#define DISABLE_POWER_PATH_VOLTAGE 2800

struct shutdown_condition {
	bool is_overheat;
	bool is_soc_zero_percent;
	bool is_uisoc_one_percent;
	bool is_under_shutdown_voltage;
	bool is_dlpt_shutdown;
};

/*
struct shutdown_controller {
	struct alarm kthread_fgtimer;
	bool timeout;
	bool overheat;
	wait_queue_head_t  wait_que;
	struct shutdown_condition shutdown_status;
	ktime_t pre_time[SHUTDOWN_FACTOR_MAX];
	int avgvbat;
	bool lowbatteryshutdown;
	int batdata[AVGVBAT_ARRAY_SIZE];
	int batidx;
	struct mutex lock;
	struct notifier_block psy_nb;
	int vbat_lt;
	int vbat_lt_lv1;
	int shutdown_cond_flag;
};
*/
enum battery_sdc_type {
	BATTERY_MANAGER,
	BATTERY_MAIN,
	BATTERY_SLAVE,
	BATTERY_SDC_MAX,
};

struct battery_shutdown_unit {
	enum battery_sdc_type type;
	struct mtk_battery *gm;

	int vbat_lt;
	int vbat_lt_lv1;
	int shutdown_cond_flag;
	bool lowbatteryshutdown;
	struct shutdown_condition shutdown_status;
	ktime_t pre_time[SHUTDOWN_FACTOR_MAX];
	int batdata[AVGVBAT_ARRAY_SIZE];
	int batidx;
	int avgvbat;
	int ui_zero_time_flag;
	int down_to_low_bat;
};

#define MAX_SDC 2

struct shutdown_controller {
	struct alarm kthread_fgtimer[BATTERY_SDC_MAX];
	int timeout;
	wait_queue_head_t  wait_que;
	struct mutex lock;
	ktime_t endtime[BATTERY_SDC_MAX];
	struct wakeup_source *sdc_wakelock;
	spinlock_t slock;

	struct battery_shutdown_unit bmsdu;
	struct battery_shutdown_unit bat[MAX_SDC];
};

struct BAT_EC_Struct {
	int fixed_temp_en;
	int fixed_temp_value;
	int debug_rac_en;
	int debug_rac_value;
	int debug_ptim_v_en;
	int debug_ptim_v_value;
	int debug_ptim_r_en;
	int debug_ptim_r_value;
	int debug_ptim_r_value_sign;
	int debug_fg_curr_en;
	int debug_fg_curr_value;
	int debug_bat_id_en;
	int debug_bat_id_value;
	int debug_d0_c_en;
	int debug_d0_c_value;
	int debug_d0_v_en;
	int debug_d0_v_value;
	int debug_uisoc_en;
	int debug_uisoc_value;
	int debug_kill_daemontest;
};

#define ZCV_LOG_LEN 10

struct zcv_log {
	struct timespec64 time;
	int car;
	int dtime;
	int dcar;
	int avgcurrent;
};

struct zcv_filter {
	int fidx;
	int lidx;
	int size;
	int zcvtime;
	int zcvcurrent;
	struct zcv_log log[ZCV_LOG_LEN];
};

struct fgd_cmd_daemon_data {
	int uisoc;
	int fg_c_soc;
	int fg_v_soc;
	int soc;

	int fg_c_d0_soc;
	int car_c;
	int fg_v_d0_soc;
	int car_v;

	int qmxa_t_0ma;
	int quse;
	int tmp;
	int vbat;
	int iavg;

	int aging_factor;
	int loading_factor1;
	int loading_factor2;

	int g_zcv_data;
	int g_zcv_data_soc;
	int g_zcv_data_mah;
	int tmp_show_ag;
	int tmp_bh_ag;
};

struct ag_center_data_st {
	int data[43];
	struct timespec64 times[3];
};

struct shutdown_data {
	int data[7];
};
struct mtk_battery {
	/*linux driver related*/
	wait_queue_head_t  wait_que;
	unsigned int fg_update_flag;
	struct hrtimer fg_hrtimer;
	struct mutex ops_lock;
	struct mutex fg_update_lock;
	int id;
	int type;

	struct property_control prop_control;

	int battery_temp;
	struct mtk_coulomb_service cs;
	struct mtk_gauge *gauge;
	struct mtk_battery_manager *bm;

	struct mtk_battery_algo algo;

	struct mtk_battery_sysfs_field_info *battery_sysfs;

	int fg_vbat_l_thr;
	int fg_vbat_h_thr;

	/*for irq thread*/
	struct irq_controller irq_ctrl;

	/*for bat prop*/
	int no_prop_timeout_control;

	/* adb */
	int fixed_bat_tmp;
	int fixed_uisoc;

	/* for test */
	struct BAT_EC_Struct Bat_EC_ctrl;
	int BAT_EC_cmd;
	int BAT_EC_param;

	/*battery flag*/
	bool init_flag;
	bool is_probe_done;
	bool disable_nafg_int;
	bool disableGM30;
	bool ntc_disable_nafg;
	bool cmd_disable_nafg;

	/*battery plug in out*/
	bool disable_plug_int;
	bool nouse_baton_undet;

	/*battery full*/
	bool is_force_full;
	int charge_power_sel;

	/*battery status*/
	int vbat0_flag;
	int present;
	int imix;
	int baton;
	int vbat;
	int ibat;
	int tbat;
	int soc;
	int ss_precise_soc;
	int ui_soc;
	int ss_precise_ui_soc;
	ktime_t uisoc_oldtime;
	int d_saved_car;
	struct zcv_filter zcvf;

	/*ai */
	int quse;
	int qmaxt;
	int precise_soc;
	int precise_uisoc;

	/*battery health*/
	struct ag_center_data_st bh_data;

	/*battery interrupt*/
	/* coulomb interrupt */
	int coulomb_int_gap;
	int coulomb_int_ht;
	int coulomb_int_lt;
	int soc_decimal_rate;
	struct gauge_consumer coulomb_plus;
	struct gauge_consumer coulomb_minus;

	/* uisoc interrupt */
	int uisoc_int_ht_gap;
	int uisoc_int_lt_gap;
	int uisoc_int_ht_en;
	int uisoc_int_lt_en;
	struct gauge_consumer uisoc_plus;
	struct gauge_consumer uisoc_minus;

	/* charge full interrupt */
	struct timespec64 chr_full_handler_time;

	/* battery temperature interrupt */
	int bat_tmp_int_gap;
	int bat_tmp_c_int_gap;
	int bat_tmp_ht;
	int bat_tmp_lt;
	int bat_tmp_c_ht;
	int bat_tmp_c_lt;
	int bat_tmp_int_ht;
	int bat_tmp_int_lt;
	int cur_bat_temp;

	/*nafg monitor */
	int last_nafg_cnt;
	ktime_t last_nafg_update_time;
	bool is_nafg_broken;

	/* information from LK */
	signed int ptim_lk_v;
	signed int ptim_lk_i;
	int lk_boot_coulomb;
	int pl_bat_vol;
	int pl_shutdown_time;
	int pl_two_sec_reboot;
	int plug_miss_count;
	int is_evb_board;

	/* suspend, resume notify */
	bool in_sleep;
	struct notifier_block pm_nb;

	/* gauge timer */
	struct alarm tracking_timer;
	struct work_struct tracking_timer_work;
	struct alarm one_percent_timer;
	struct work_struct one_percent_timer_work;

	/*UISOC timer for no hw*/
	struct alarm sw_uisoc_timer;
	struct work_struct sw_uisoc_timer_work;

	/* battery cycle */
	bool is_reset_battery_cycle;
	int bat_cycle;
	int bat_cycle_thr;
	int bat_cycle_car;
	int bat_cycle_ncar;

	/*sw average current*/
	ktime_t sw_iavg_time;
	int sw_iavg_car;
	int sw_iavg;
	int sw_iavg_ht;
	int sw_iavg_lt;
	int sw_iavg_gap;
	int iavg_th[MAX_TABLE];

	/*sw low battery interrupt*/
	struct lbat_user *lowbat_service;
	int sw_low_battery_ht_en;
	int sw_low_battery_ht_threshold;
	int sw_low_battery_lt_en;
	int sw_low_battery_lt_threshold;
	struct mutex sw_low_battery_mutex;

	/*simulator log*/
	struct simulator_log log;

	/* cust req ocv data */
	int algo_qmax;
	int algo_req_ocv;
	int algo_ocv_to_mah;
	int algo_ocv_to_soc;
	int algo_vtemp;

	/* aging */
	bool is_reset_aging_factor;
	int aging_factor;
	int show_ag;

	/* bootmode */
	u32 bootmode;
	u32 boottype;

	/*custom related*/
	int battery_id;
	struct fuel_gauge_custom_data fg_cust_data;
	struct fuel_gauge_table_custom_data fg_table_cust_data;
	struct fgd_cmd_param_t_custom fg_data;

	/*daemon version control*/
	struct VersionControl fg_version;
	int fg_mode;

	/* hwocv swocv */
	int ext_hwocv_swocv;
	int ext_hwocv_swocv_lt;
	int ext_hwocv_swocv_lt_temp;
	/* battery temperature table */
	int no_bat_temp_compensate;
	int enable_tmp_intr_suspend;
	struct battery_temperature_table rbat;
	struct fg_temp *tmp_table;

	int pre_bat_temperature_volt_temp;
	int pre_bat_temperature_volt;
	int pre_fg_current_temp;
	int pre_fg_current_state;
	int pre_fg_r_value;
	int pre_bat_temperature_val2;

	void (*shutdown)(struct mtk_battery *gm);
	int (*suspend)(struct mtk_battery *gm, pm_message_t state);
	int (*resume)(struct mtk_battery *gm);

	void (*netlink_handler)(struct mtk_battery *gm, void *nl_data, struct afw_header *ret_msg);
	void (*netlink_send)(struct mtk_battery *gm, int seq, struct afw_header *reply_msg);
	int log_level;

	/* for manager */
	int (*manager_send)(struct mtk_battery *gm, enum manager_cmd cmd, int val);

	/*daemon data*/
	struct fgd_cmd_daemon_data daemon_data;
	int daemon_version;

	/* low bat bound */
	int bat_voltage_low_bound;
	int bat_voltage_low_bound_orig;
	int low_tmp_bat_voltage_low_bound;
	int low_tmp_bat_voltage_low_bound_orig;

	/* for bat_plug_out*/
	int bat_plug_out;

	/* for low v avgbat threshold*/
	int avgvbat_array_size;

	/* for BatteryNotify*/
	unsigned int notify_code;

	struct shutdown_data sd_data;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int tbat_adc;
	int is_fake_soc;
	int is_full;
	unsigned int f_mode;
	char d_buf[FG_BATT_DUMP_SIZE];
#endif
};

struct mtk_battery_manager {
	struct device *dev;
	wait_queue_head_t  wait_que;
	unsigned int bm_update_flag;
	int log_level;

	struct iio_channel *chan_vsys;

	struct hrtimer bm_hrtimer;

	spinlock_t slock;
	struct timespec64 endtime;
	struct alarm bm_alarmtimer;
	struct wakeup_source *bm_wakelock;
	bool is_suspend;
	struct notifier_block pm_notifier;

	struct mtk_battery *gm1;
	struct mtk_battery *gm2;
	int gm_no;

	int force_ui_zero;
	int uisoc;
	int ibat;
	int vbat;

	/* EOC */
	bool b_EOC;
	/* plug in out */
	int chr_type;

	//netlink
	struct sock *mtk_bm_sk;
	u_int fgd_pid;

	struct battery_data bs_data;

	/* power misc */
	struct shutdown_controller sdc;

	/* bootmode */
	u32 bootmode;
	u32 boottype;

	/* vsys bound*/
	int disable_quick_shutdown;
	int vsys_det_voltage1;
	int vsys_det_voltage2;
};

struct mtk_battery_sysfs_field_info {
	struct device_attribute attr;
	enum battery_property prop;
	int (*set)(struct mtk_battery *gm,
		struct mtk_battery_sysfs_field_info *attr, int val);
	int (*get)(struct mtk_battery *gm,
		struct mtk_battery_sysfs_field_info *attr, int *val);
};

extern struct mtk_battery *gmb;

/* coulomb service */
extern void gauge_coulomb_service_init(struct mtk_battery *gm);
extern void gauge_coulomb_consumer_init(struct gauge_consumer *coulomb,
	struct device *dev, char *name);
extern void gauge_coulomb_start(struct mtk_battery *gm,
	struct gauge_consumer *coulomb, int car);
extern void gauge_coulomb_stop(struct mtk_battery *gm,
	struct gauge_consumer *coulomb);
extern void gauge_coulomb_dump_list(struct mtk_battery *gm);
extern void gauge_coulomb_before_reset(struct mtk_battery *gm);
extern void gauge_coulomb_after_reset(struct mtk_battery *gm);
extern void wake_up_gauge_coulomb(struct mtk_battery *gm);
/* coulomb sub system end */

/*mtk_battery.c */
extern void enable_gauge_irq(struct mtk_gauge *gauge,
	enum gauge_irq irq);
extern void disable_gauge_irq(struct mtk_gauge *gauge,
	enum gauge_irq irq);
extern int bat_get_debug_level(struct mtk_battery *gm);
extern char *bat_get_gauge_name(struct mtk_battery *gm);
extern int force_get_tbat(struct mtk_battery *gm, bool update);
extern int force_get_tbat_internal(struct mtk_battery *gm);
extern int wakeup_fg_algo_cmd(struct mtk_battery *gm,
	unsigned int flow_state, int cmd, int para1);
extern int get_iavg_gap(struct mtk_battery *gm);
extern int wakeup_fg_algo(struct mtk_battery *gm, unsigned int flow_state);

extern int gauge_get_int_property(struct mtk_battery *gm, enum gauge_property gp);
extern int gauge_get_property(struct mtk_battery *gm, enum gauge_property gp,
			    int *val);
extern int gauge_get_property_control(struct mtk_battery *gm,
	enum gauge_property gp, int *val, int mode);
extern int gauge_set_property(struct mtk_battery *gm, enum gauge_property gp,
			    int val);

extern void gp_number_to_name(struct mtk_battery *gm, char *gp_name, unsigned int gp_no);
extern void reg_type_to_name(struct mtk_battery *gm, char *reg_type_name, unsigned int regmap_type);
extern int battery_init(struct platform_device *pdev);
extern int battery_psy_init(struct platform_device *pdev);
extern struct mtk_battery *get_mtk_battery(void);
extern int battery_get_property(struct mtk_battery *gm, enum battery_property bp, int *val);
extern int battery_get_int_property(struct mtk_battery *gm, enum battery_property bp);
extern int battery_set_property(struct mtk_battery *gm, enum battery_property bp, int val);
extern bool fg_interrupt_check(struct mtk_battery *gm);
extern void fg_nafg_monitor(struct mtk_battery *gm);
extern bool is_algo_active(struct mtk_battery *gm);
extern int disable_shutdown_cond(struct mtk_battery *gm, int shutdown_cond);
extern int set_shutdown_cond(struct mtk_battery *gm, int shutdown_cond);
extern bool is_kernel_power_off_charging(struct mtk_battery *gm);
extern void set_shutdown_vbat_lt(struct mtk_battery *gm,
	int vbat_lt, int vbat_lt_lv1);
extern void fg_sw_bat_cycle_accu(struct mtk_battery *gm);
extern void notify_fg_chr_full(struct mtk_battery *gm);
extern int fgauge_get_profile_id(struct mtk_battery *gm);
extern void disable_fg(struct mtk_battery *gm);
extern int get_shutdown_cond(struct mtk_battery *gm);
extern void battery_update(struct mtk_battery_manager *bm);
extern int set_bm_shutdown_cond(struct mtk_battery_manager *bm, int shutdown_cond);
extern int get_shutdown_cond_flag(struct mtk_battery *gm);
extern void set_shutdown_cond_flag(struct mtk_battery *gm, int val);
extern bool set_charge_power_sel(struct mtk_battery *gm, enum charge_sel select);
extern int dump_pseudo100(struct mtk_battery *gm, enum charge_sel select);

extern int bm_get_vsys(struct mtk_battery_manager *bm);
extern int get_charger_vbat(struct mtk_battery_manager *bm);
extern void reload_battery_zcv_table(struct mtk_battery *gm, int select_zcv);
/*mtk_battery.c end */

/* mtk_battery_algo.c */
extern void battery_algo_init(struct mtk_battery *gm);
extern void do_fg_algo(struct mtk_battery *gm, unsigned int intr_num);
extern void fg_bat_temp_int_internal(struct mtk_battery *gm);
/* mtk_battery_algo.c end */
extern void disable_all_irq(struct mtk_battery *gm);

/*mtk_battery_daemon.c*/
extern void wake_up_bat_irq_controller(struct irq_controller *irq_ctrl, int flags);
/*mtk_battery_daemon.c end*/

/* */
extern void wake_up_power_misc(struct shutdown_controller *sdc);
/* */


/* mtk daemon related */
extern int mtk_battery_daemon_init(struct platform_device *pdev);
extern void mtk_irq_thread_init(struct mtk_battery *gm);
//extern int wakeup_fg_daemon(struct mtk_battery *gm, unsigned int flow_state, int cmd, int para1);

/* customize */
#define DIFFERENCE_FULLOCV_ITH	200	/* mA */
#define MTK_CHR_EXIST			1
#define KEEP_100_PERCENT		1

/* Rsense setting */
/* UNIT_MULTIPLE/CURR_MEASURE_20A MT6375 only */
#define R_FG_VALUE				5	/* mOhm */
#define UNIT_MULTIPLE			2
#define CURR_MEASURE_20A	1

#define EMBEDDED_SEL			0
#define PMIC_SHUTDOWN_CURRENT	20	/* 0.01 mA */
#define FG_METER_RESISTANCE		100
#define CAR_TUNE_VALUE			100 /*1.00 */
#define NO_BAT_TEMP_COMPENSATE	0
#define NO_PROP_TIMEOUT_CONTROL 0
/* NO_BAT_TEMP_COMPENSATE 1 = don't need bat_temper compensate, */
/* but fg_meter_resistance still use for SWOCV */

/* enable that soc = 0 , shutdown */
#define SHUTDOWN_GAUGE0			1

/* enable that uisoc = 1 and wait xmins then shutdown */
#define SHUTDOWN_GAUGE1_XMINS	1
/* define Xmins to shutdown*/
#define SHUTDOWN_1_TIME			5

#define SHUTDOWN_GAUGE1_VBAT_EN	0
#define SHUTDOWN_GAUGE1_VBAT	34000

#define SHUTDOWN_GAUGE0_VOLTAGE	34000

#define POWERON_SYSTEM_IBOOT	500	/* mA */


/* shutdown jumping*/
#define LOW_TRACKING_JUMP	0
#define PRE_TRACKING_JUMP	0
#define LAST_MODE_RESET	0
#define PRE_TRACKING_SOC_RESET	0

/*
 * LOW_TEMP_MODE = 0
 *	disable LOW_TEMP_MODE
 * LOW_TEMP_MODE = 1
 *	if battery temperautre < LOW_TEMP_MODE_TEMP
 *	when bootup , force C mode
 * LOW_TEMP_MODE = 2
 *	if battery temperautre < LOW_TEMP_MODE_TEMP
 *	force C mode
 */
#define LOW_TEMP_MODE			0
#define LOW_TEMP_MODE_TEMP		0

#define D0_SEL					0	/* not implement */
#define AGING_SEL				0	/* not implement */
#define DLPT_UI_REMAP_EN		0

/* ADC resistor  */
#define R_BAT_SENSE				4
#define R_I_SENSE				4
#define R_CHARGER_1				330
#define R_CHARGER_2				39

#define QMAX_SEL				1
#define IBOOT_SEL				0
#define SHUTDOWN_SYSTEM_IBOOT	15000	/* 0.1mA */
#define PMIC_MIN_VOL			33500

/*ui_soc related */
#define DIFFERENCE_FULL_CV		0 /*0.01%*/
/* UISOC should decrease, when plug in charger but battery keeps drain in HZ mode */
#define PSEUDO1_EN				1
#define PSEUDO100_EN			1
#define PSEUDO100_EN_DIS		0

#define DIFF_SOC_SETTING				50	/* 0.01% */
#define DIFF_BAT_TEMP_SETTING			1
#define DIFF_BAT_TEMP_SETTING_C			10
#define DISCHARGE_TRACKING_TIME			30 /* ALPS05455856 customize tracking time */
#define CHARGE_TRACKING_TIME			60
#define DIFFERENCE_FULLOCV_VTH			1000/* 0.1mV */
#define CHARGE_PSEUDO_FULL_LEVEL		8000
#define FULL_TRACKING_BAT_INT2_MULTIPLY 6

/* pre tracking */
#define FG_PRE_TRACKING_EN	1
#define VBAT2_DET_TIME		5
#define VBAT2_DET_COUNTER	6
#define VBAT2_DET_VOLTAGE1	34500
#define VBAT2_DET_VOLTAGE2	32000
#define VBAT2_DET_VOLTAGE3	35000
#define VSYS_DET_VOLTAGE1	3100
#define VSYS_DET_VOLTAGE2	3000

/* dynamic sd*/
#define DYNAMIC_SHUTDOWN_MAX 200		/* mv */

/* PCB setting */
#define CALI_CAR_TUNE_AVG_NUM	60

/* Dynamic CV */
#define DYNAMIC_CV_FACTOR      100     /* mV */
#define CHARGER_IEOC           150     /* mA */

/* Aging Compensation 1*/
#define AGING_FACTOR_MIN			90
#define AGING_FACTOR_DIFF			10
#define DIFFERENCE_VOLTAGE_UPDATE	50
#define AGING_ONE_EN				0
#define AGING1_UPDATE_SOC			30
#define AGING1_LOAD_SOC				70
#define AGING4_UPDATE_SOC			40
#define AGING4_LOAD_SOC				70
#define AGING5_UPDATE_SOC			30
#define AGING5_LOAD_SOC				70
#define AGING6_UPDATE_SOC			30
#define AGING6_LOAD_SOC				70
#define AGING_TEMP_DIFF				10
#define AGING_TEMP_LOW_LIMIT			15
#define AGING_TEMP_HIGH_LIMIT			50
#define AGING_100_EN				1

/* Aging Compensation 2*/
#define AGING_TWO_EN				0

/* Aging Compensation 3*/
#define AGING_THIRD_EN				0

#define AGING_4_EN				0
#define AGING_5_EN				0
#define AGING_6_EN				0

/* threshold */
#define HWOCV_SWOCV_DIFF			300
#define HWOCV_SWOCV_DIFF_LT			1500
#define HWOCV_SWOCV_DIFF_LT_TEMP	5
#define HWOCV_OLDOCV_DIFF			400
#define HWOCV_OLDOCV_DIFF_CHR		800
#define SWOCV_OLDOCV_DIFF			300
#define SWOCV_OLDOCV_DIFF_CHR		800
#define VBAT_OLDOCV_DIFF			1000
#define SWOCV_OLDOCV_DIFF_EMB		1000	/* 100mV */

#define VIR_OLDOCV_DIFF_EMB			10000	/* 1000mV */
#define VIR_OLDOCV_DIFF_EMB_LT		10000	/* 1000mV */
#define VIR_OLDOCV_DIFF_EMB_TMP		5

#define TNEW_TOLD_PON_DIFF			5
#define TNEW_TOLD_PON_DIFF2			15
#define PMIC_SHUTDOWN_TIME			30
#define BAT_PLUG_OUT_TIME			32
#define EXT_HWOCV_SWOCV				300
#define EXT_HWOCV_SWOCV_LT			1500
#define EXT_HWOCV_SWOCV_LT_TEMP		5

/* fgc & fgv threshold */
#define DIFFERENCE_FGC_FGV_TH1		300
#define DIFFERENCE_FGC_FGV_TH2		500
#define DIFFERENCE_FGC_FGV_TH3		300
#define DIFFERENCE_FGC_FGV_TH_SOC1	7000
#define DIFFERENCE_FGC_FGV_TH_SOC2	3000
#define NAFG_TIME_SETTING			10
#define NAFG_RATIO					100
#define NAFG_RATIO_EN				0
#define NAFG_RATIO_TMP_THR			1
#define NAFG_RESISTANCE				1500

#define PMIC_SHUTDOWN_SW_EN			1
/* 0: mix, 1:Coulomb, 2:voltage */
#define FORCE_VC_MODE				0

#define LOADING_1_EN				0
#define LOADING_2_EN				2
#define DIFF_IAVG_TH				3000

/* ZCV INTR */
#define ZCV_SUSPEND_TIME			7
#define SLEEP_CURRENT_AVG			200 /*0.1mA*/
#define ZCV_COM_VOL_LIMIT 50	/* 50mv */
#define ZCV_CAR_GAP_PERCENTAGE		5

/* Additional battery table */
#define ADDITIONAL_BATTERY_TABLE_EN 1

#define DC_RATIO_SEL				5
/* if set 0, dcr_start will not be 1*/
#define DC_R_CNT					1000

#define BAT_PAR_I					4000
#define PSEUDO1_SEL					2

#define FG_TRACKING_CURRENT				40000
#define FG_TRACKING_CURRENT_IBOOT_EN	0
#define UI_FAST_TRACKING_EN				0
#define UI_FAST_TRACKING_GAP			300
#define KEEP_100_PERCENT_MINSOC			9000


#define SHUTDOWN_CONDITION_LOW_BAT_VOLT
#define LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN	1
#define LOW_TEMP_THRESHOLD					5

#define BATTERY_TMP_TO_DISABLE_GM30				-50
#define BATTERY_TMP_TO_DISABLE_NAFG				-35
#define DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG	25
#define BATTERY_TMP_TO_ENABLE_NAFG				-20
/* #define GM30_DISABLE_NAFG */

#define POWER_ON_CAR_CHR		5
#define POWER_ON_CAR_NOCHR		-35

#define SHUTDOWN_CAR_RATIO		1


/* different temp using different gauge 0% */
#define MULTI_TEMP_GAUGE0		1

#define OVER_DISCHARGE_LEVEL	-1500

#define UISOC_UPDATE_TYPE		0
/*
 *	uisoc_update_type:
 *	0: only ui_soc interrupt update ui_soc
 *	1: coulomb/nafg will update ui_soc if delta car > ht/lt_gap /2
 *	2: coulomb/nafg will update ui_soc
 */

/* using current to limit uisoc in 100% case*/
/* UI_FULL_LIMIT_ITH0 3000 means 300ma */
#define UI_FULL_LIMIT_EN	0
#define UI_FULL_LIMIT_SOC0	9900
#define UI_FULL_LIMIT_ITH0	2200

#define UI_FULL_LIMIT_SOC1	9900
#define UI_FULL_LIMIT_ITH1	2200

#define UI_FULL_LIMIT_SOC2	9900
#define UI_FULL_LIMIT_ITH2	2200

#define UI_FULL_LIMIT_SOC3	9900
#define UI_FULL_LIMIT_ITH3	2200

#define UI_FULL_LIMIT_SOC4	9900
#define UI_FULL_LIMIT_ITH4	2200

#define UI_FULL_LIMIT_TIME	99999


#define UI_FULL_LIMIT_FC_SOC0	9900
#define UI_FULL_LIMIT_FC_ITH0	3000

#define UI_FULL_LIMIT_FC_SOC1	9900
#define UI_FULL_LIMIT_FC_ITH1	3100

#define UI_FULL_LIMIT_FC_SOC2	9900
#define UI_FULL_LIMIT_FC_ITH2	3200

#define UI_FULL_LIMIT_FC_SOC3	9900
#define UI_FULL_LIMIT_FC_ITH3	3300

#define UI_FULL_LIMIT_FC_SOC4	9900
#define UI_FULL_LIMIT_FC_ITH4	3400

/* using voltage to limit uisoc in 1% case */
/* UI_LOW_LIMIT_VTH0=36000 means 3.6v */
#define UI_LOW_LIMIT_EN		0
#define UI_LOW_LIMIT_SOC0	200
#define UI_LOW_LIMIT_VTH0	34500
#define UI_LOW_LIMIT_SOC1	200
#define UI_LOW_LIMIT_VTH1	34500
#define UI_LOW_LIMIT_SOC2	200
#define UI_LOW_LIMIT_VTH2	34500
#define UI_LOW_LIMIT_SOC3	200
#define UI_LOW_LIMIT_VTH3	34500
#define UI_LOW_LIMIT_SOC4	200
#define UI_LOW_LIMIT_VTH4	34500
#define UI_LOW_LIMIT_TIME	99999

#define MOVING_BATTEMP_EN	1
#define MOVING_BATTEMP_THR	20

/* Qmax for battery  */
#define Q_MAX_L_CURRENT		0
#define Q_MAX_H_CURRENT		10000

/* multiple battery profile compile options */
/*#define MTK_GET_BATTERY_ID_BY_AUXADC*/


/* if ACTIVE_TABLE == 0 && MULTI_BATTERY == 0
 * load g_FG_PSEUDO100_Tx from dtsi
 */
#define MULTI_BATTERY			0
#define BATTERY_ID_CHANNEL_NUM	1
#define BATTERY_PROFILE_ID		0
#define TOTAL_BATTERY_NUMBER	4

#endif /* __MTK_BATTERY_INTF_H__ */
