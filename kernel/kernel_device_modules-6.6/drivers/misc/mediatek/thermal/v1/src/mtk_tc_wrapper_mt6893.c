// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#if IS_ENABLED(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

#include <tscpu_settings.h>
#include "mtk_idle.h"
#if CFG_THERMAL_KERNEL_IGNORE_HOT_SENSOR
#include <mt-plat/mtk_devinfo.h>
#endif
#if CONFIG_LVTS_ERROR_AEE_WARNING
#include <mt-plat/aee.h>
#include <linux/delay.h>
#if DUMP_VCORE_VOLTAGE
#include <linux/regulator/consumer.h>
#endif
struct lvts_error_data {
	int ts_temp[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
	int ts_temp_r[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
	int ts_temp_v[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
#if DUMP_VCORE_VOLTAGE
	int vcore_voltage[R_BUFFER_SIZE]; /* A ring buffer */
#endif
	int c_index; /* Current index points to the space to replace.*/
	int e_occurred; /* 1: An error occurred, 0: Nothing happened*/
	int f_count; /* Future count */
	enum thermal_sensor e_mcu;
	enum thermal_sensor e_lvts;
};
struct lvts_error_data g_lvts_e_data;
int tscpu_ts_mcu_temp_v[L_TS_MCU_NUM];
int tscpu_ts_lvts_temp_v[L_TS_LVTS_NUM];
#endif
#if CFG_THERM_LVTS
int tscpu_ts_lvts_temp[L_TS_LVTS_NUM];
int tscpu_ts_lvts_temp_r[L_TS_LVTS_NUM];
#endif
int tscpu_init_done;
int tscpu_curr_cpu_temp;
int tscpu_curr_gpu_temp;
#if CFG_THERMAL_KERNEL_IGNORE_HOT_SENSOR
static int ignore_hot_sensor;
#endif
static int tscpu_curr_max_ts_temp;



static __s32 g_degc_cali;
static __s32 g_o_slope;
static __s32 g_o_slope_sign;

static __s32 g_oe;
static __s32 g_gain;
static __s32 g_x_roomt[TS_ENUM_MAX] = { 0 };


int get_immediate_none_wrap(void)
{
	return -127000;
}

/* chip dependent */
int get_immediate_cpuL_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS3_0],
			tscpu_ts_lvts_temp[L_TS_LVTS3_1]),
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS3_2],
			tscpu_ts_lvts_temp[L_TS_LVTS3_3])
		);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_cpuB_LVTS1_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS1_0],
			tscpu_ts_lvts_temp[L_TS_LVTS1_1]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}
void set_tscpu_init_done(int complete)
{
	tscpu_init_done = complete;
}
int get_immediate_cpuB_LVTS2_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS2_0],
			tscpu_ts_lvts_temp[L_TS_LVTS2_1]);


	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}


int get_immediate_vpu_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS4_0],
		tscpu_ts_lvts_temp[L_TS_LVTS4_1]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_gpu_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS5_0],
		tscpu_ts_lvts_temp[L_TS_LVTS5_1]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;

}

int get_immediate_infa_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS6_0];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_camsys_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS6_1];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_md_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(MAX(tscpu_ts_lvts_temp[L_TS_LVTS7_0],
		tscpu_ts_lvts_temp[L_TS_LVTS7_1]),
		tscpu_ts_lvts_temp[L_TS_LVTS7_2]);

	return curr_temp;

}


/*=============================================================*/

void get_thermal_slope_intercept(struct TS_PTPOD *ts_info, enum
thermal_bank_name ts_bank)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	__s32 x_roomt;

	/* chip dependent */

	/* If there are two or more sensors in a bank, choose the sensor
	 * calibration value of the dominant sensor. You can observe it in the
	 * thermal document provided by Thermal DE.  For example, Bank 1 is
	 * for SOC + GPU. Observe all scenarios related to GPU simulation test
	 * cases to decide which sensor is the highest temperature in all cases.
	 * Then, It is the dominant sensor.(Confirmed by Thermal DE Alfred Tsai)
	 */

	/*
	 * PTP#	module		TSMCU Plan
	 *  0	MCU_LITTLE	TSMCU-5,6,7
	 *  1	MCU_BIG		TSMCU-8,9
	 *  2	MCU_CCI		TSMCU-5,6,7
	 *  3	MFG(GPU)	TSMCU-1,2
	 *  4	VPU		TSMCU-4
	 *  No PTP bank 5
	 *  6	TOP		TSMCU-1,2,4
	 *  7	MD		TSMCU-0
	 */

	switch (ts_bank) {
	case THERMAL_BANK0:
		x_roomt = g_x_roomt[L_TS_MCU6];
		break;
	case THERMAL_BANK1:
		x_roomt = g_x_roomt[L_TS_MCU9];
		break;
	case THERMAL_BANK2:
		x_roomt = g_x_roomt[L_TS_MCU6];
		break;
	case THERMAL_BANK3:
		x_roomt = g_x_roomt[L_TS_MCU2];
		break;
	case THERMAL_BANK4:
		x_roomt = g_x_roomt[L_TS_MCU4];
		break;
	/* No bank 5 */
	case THERMAL_BANK6:
		x_roomt = g_x_roomt[L_TS_MCU4];
		break;
	case THERMAL_BANK7:
		x_roomt = g_x_roomt[L_TS_MCU0];
		break;
	default: /* choose the highest simulation hot-spot */
		x_roomt = g_x_roomt[L_TS_MCU9];
		break;
	}


	/*
	 *   The equations in this function are confirmed by Thermal DE Alfred
	 *   Tsai.  Don't have to change until using next generation thermal
	 *   sensors.
	 */

	temp0 = (10000 * 100000 / g_gain) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp1 = (temp0 * 10) / (1534 + g_o_slope * 10);
	else
		temp1 = (temp0 * 10) / (1534 - g_o_slope * 10);

	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 =
	((10000 * 100000 / 4096 / g_gain) * g_oe + x_roomt * 10) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp2 = temp1 * 100 / (1534 + g_o_slope * 10);
	else
		temp2 = temp1 * 100 / (1534 - g_o_slope * 10);

	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;


	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	tscpu_dprintk("ts_MTS=%d, ts_BTS=%d\n",
		ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);
}
EXPORT_SYMBOL(get_thermal_slope_intercept);

/*
 * THERMAL_BANK0,	//B CPU (LVTS1)
 * THERMAL_BANK1,	//B CPU (LVTS2)
 * THERMAL_BANK2,	//L CPU (LVTS3)
 * THERMAL_BANK3,	//VPU   (LVTS4)
 * THERMAL_BANK4,	//GPU   (LVTS5)
 * THERMAL_BANK5,	//IFRA  (LVTS6)
 */

int (*max_temperature_in_bank[THERMAL_BANK_NUM])(void) = {
	get_immediate_cpuB_LVTS1_wrap,
	get_immediate_cpuB_LVTS2_wrap,
	get_immediate_cpuL_wrap,
	get_immediate_vpu_wrap,
	get_immediate_gpu_wrap,
	get_immediate_infa_wrap
};

int get_immediate_cpuB_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(get_immediate_cpuB_LVTS1_wrap(),
			get_immediate_cpuB_LVTS2_wrap());


	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS2_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts4_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS4_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts5_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS5_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts6_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS6_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts7_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS6_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts8_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS7_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts9_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS7_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

#if CFG_THERM_LVTS
int get_immediate_tslvts1_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts1_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS1_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}
EXPORT_SYMBOL(get_immediate_tslvts1_1_wrap);

int get_immediate_tslvts2_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS2_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts2_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS2_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS3_3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS4_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS4_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts5_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS5_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}


int get_immediate_tslvts5_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS5_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}


int get_immediate_tslvts6_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS6_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts6_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS6_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}


int get_immediate_tslvts7_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS7_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts7_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS7_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts7_2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_lvts_temp[L_TS_LVTS7_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}
#endif

int get_immediate_tsabb_wrap(void)
{

	return 0;
}

int (*get_immediate_tsX[L_TS_LVTS_NUM])(void) = {

#if CFG_THERM_LVTS
	get_immediate_tslvts1_0_wrap,
	get_immediate_tslvts1_1_wrap,
	get_immediate_tslvts2_0_wrap,
	get_immediate_tslvts2_1_wrap,
	get_immediate_tslvts3_0_wrap,
	get_immediate_tslvts3_1_wrap,
	get_immediate_tslvts3_2_wrap,
	get_immediate_tslvts3_3_wrap,
	get_immediate_tslvts4_0_wrap,
	get_immediate_tslvts4_1_wrap,
	get_immediate_tslvts5_0_wrap,
	get_immediate_tslvts5_1_wrap,
	get_immediate_tslvts6_0_wrap,
	get_immediate_tslvts6_1_wrap,
	get_immediate_tslvts7_0_wrap,
	get_immediate_tslvts7_1_wrap,
	get_immediate_tslvts7_2_wrap
#endif
};

/**
 * this only returns latest stored max ts temp but not updated from TC.
 */
int tscpu_get_curr_max_ts_temp(void)
{
	return tscpu_curr_max_ts_temp;
}

#if CFG_THERM_LVTS
int tscpu_max_temperature(void)
{
	int i, j, max = 0;

#if CFG_LVTS_DOMINATOR
#if CFG_THERM_LVTS
	tscpu_dprintk("lvts_max_temperature %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		for (j = 0; j < lvts_tscpu_g_tc[i].ts_number; j++) {
			if (i == 0 && j == 0) {
				max = tscpu_ts_lvts_temp[
					lvts_tscpu_g_tc[i].ts[j]];
			} else {
				if (max < tscpu_ts_lvts_temp[
						lvts_tscpu_g_tc[i].ts[j]])
					max = tscpu_ts_lvts_temp[
						lvts_tscpu_g_tc[i].ts[j]];
			}
		}
	}
#endif /* CFG_THERM_LVTS */
#else
	tscpu_dprintk("tscpu_get_temp %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		for (j = 0; j < tscpu_g_tc[i].ts_number; j++) {
			if (i == 0 && j == 0) {
				max = tscpu_ts_mcu_temp[tscpu_g_tc[i].ts[j]];
			} else {
				if (max < tscpu_ts_mcu_temp[
						tscpu_g_tc[i].ts[j]])
					max = tscpu_ts_mcu_temp[
						tscpu_g_tc[i].ts[j]];
			}
		}
	}
#endif /* CFG_LVTS_DOMINATOR */
	return max;
}
#endif

int tscpu_get_curr_temp(void)
{
	//if (!tscpu_init_done)
	//	return 0;
	tscpu_update_tempinfo();

#if PRECISE_HYBRID_POWER_BUDGET
	/* update CPU/GPU temp data whenever TZ times out...
	 * If the update timing is aligned to TZ polling,
	 * this segment should be moved to TZ code instead of thermal
	 * controller driver
	 */
/*
 * module			LVTS Plan
 *=====================================================
 * MCU_BIG(T1)			LVTS1-0
 * MCU_BIGBIG(T2)		LVTS1-1
 * MCU_BIG(T3,T4)		LVTS2-0, LVTS2-1
 * MCU_LITTLE(T5,T6,T7,T8)		LVTS3-0, LVTS3-1, LVTS3-2, LVTS3-3
 * VPU_MLDA(T9,T10)		LVTS4-0, LVTS4-1
 * GPU(T11,T12)			LVTS5-0, LVTS5-1
 * INFA(T13)			LVTS6-0
 * CAMSYS(T18)			LVTS6-1
 * MDSYS(T14,T15,T20)		LVTS7-0, LVTS7-1, LVTS7-2
 */

#if CFG_LVTS_DOMINATOR
#if CFG_THERM_LVTS
	tscpu_curr_cpu_temp =
	MAX(
		(MAX(
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS1_0],
			tscpu_ts_lvts_temp[L_TS_LVTS1_1]),
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS2_0],
			tscpu_ts_lvts_temp[L_TS_LVTS2_1])
		)),
		(MAX(
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS3_0],
			tscpu_ts_lvts_temp[L_TS_LVTS3_1]),
		MAX(tscpu_ts_lvts_temp[L_TS_LVTS3_2],
			tscpu_ts_lvts_temp[L_TS_LVTS3_3])
		))
	);

	tscpu_curr_gpu_temp = MAX(tscpu_ts_lvts_temp[L_TS_LVTS5_0],
		tscpu_ts_lvts_temp[L_TS_LVTS5_1]);
#endif /* CFG_THERM_LVTS */
#else
	/* It is platform dependent which TS is better to present CPU/GPU
	 * temperature
	 */
	tscpu_curr_cpu_temp = MAX(
		MAX(MAX(tscpu_ts_temp[TS_MCU5], tscpu_ts_temp[TS_MCU6]),
		MAX(tscpu_ts_temp[TS_MCU7], tscpu_ts_temp[TS_MCU8])),
		tscpu_ts_temp[TS_MCU9]);

	tscpu_curr_gpu_temp = MAX(tscpu_ts_temp[TS_MCU1],
		tscpu_ts_temp[TS_MCU2]);
#endif /* CFG_LVTS_DOMINATOR */
#endif /* PRECISE_HYBRID_POWER_BUDGET */

	/* though tscpu_max_temperature is common, put it in mtk_ts_cpu.c is
	 * weird.
	 */

	tscpu_curr_max_ts_temp = tscpu_max_temperature();

	return tscpu_curr_max_ts_temp;
}

#if CONFIG_LVTS_ERROR_AEE_WARNING
char mcu_s_array[TS_ENUM_MAX][17] = {
#if CFG_THERM_LVTS
	"TS_LVTS1_0",
	"TS_LVTS1_1",
	"TS_LVTS2_0",
	"TS_LVTS2_1",
	"TS_LVTS3_0",
	"TS_LVTS3_1",
	"TS_LVTS3_2",
	"TS_LVTS3_3",
	"TS_LVTS4_0",
	"TS_LVTS4_1",
	"TS_LVTS5_0",
	"TS_LVTS5_1",
	"TS_LVTS6_0",
	"TS_LVTS6_1",
	"TS_LVTS7_0",
	"TS_LVTS7_1",
	"TS_LVTS7_2"
 #endif
};

static void dump_lvts_error_info(void)
{
	int i, j, index, e_index, offset;
#if DUMP_LVTS_REGISTER
	int cnt, temp;
#endif
	enum thermal_sensor mcu_index, lvts_index;
	char buffer[512];

	mcu_index = g_lvts_e_data.e_mcu;
	lvts_index = g_lvts_e_data.e_lvts;
	index = g_lvts_e_data.c_index;
	e_index = (index + HISTORY_SAMPLES + 1) % R_BUFFER_SIZE;

	tscpu_printk("[LVTS_ERROR][DUMP] %s:%d and %s:%d error: |%d| > %d\n",
		mcu_s_array[mcu_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index],
		mcu_s_array[lvts_index],
		g_lvts_e_data.ts_temp[lvts_index][e_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index] -
		g_lvts_e_data.ts_temp[lvts_index][e_index],
		LVTS_ERROR_THRESHOLD);

	for (i = TS_MCU1; i <= TS_LVTS4_1; i++) {
		offset = sprintf(buffer, "[LVTS_ERROR][%s][DUMP] ",
				mcu_s_array[i]);

		for (j = 0; j < R_BUFFER_SIZE; j++) {
			index = (g_lvts_e_data.c_index + 1 + j)
					% R_BUFFER_SIZE;
			offset += sprintf(buffer + offset, "%d ",
					g_lvts_e_data.ts_temp[i][index]);

		}
		buffer[offset] = '\0';
		tscpu_printk("%s\n", buffer);
	}

	offset = sprintf(buffer, "[LVTS_ERROR][%s_R][DUMP] ",
			mcu_s_array[lvts_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_r[lvts_index][index]);
	}

	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_V][DUMP] ",
			mcu_s_array[mcu_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_v[mcu_index][index]);
	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_V][DUMP] ",
			mcu_s_array[lvts_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_v[lvts_index][index]);
	}

	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	dump_efuse_data();
#if DUMP_LVTS_REGISTER
	read_controller_reg_when_error();

	lvts_thermal_disable_all_periodoc_temp_sensing();
	lvts_wait_for_all_sensing_point_idle();

	read_device_reg_when_error();
	dump_lvts_register_value();
#endif
#if DUMP_VCORE_VOLTAGE
	offset = sprintf(buffer, "[LVTS_ERROR][Vcore_V][DUMP] ",
			mcu_s_array[lvts_index]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.vcore_voltage[index]);
	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);
#endif
#if DUMP_LVTS_REGISTER
	lvts_reset_device_and_stop_clk();
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, __func__,
		"LVTS_ERROR: %s, %s diff: %d\n", mcu_s_array[mcu_index],
		mcu_s_array[lvts_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index] -
		g_lvts_e_data.ts_temp[lvts_index][e_index]);
#endif

	g_lvts_e_data.e_occurred = 0;
	g_lvts_e_data.f_count = -1;
#if DUMP_LVTS_REGISTER
	clear_lvts_register_value_array();

	lvts_device_identification();
	lvts_Device_Enable_Init_all_Devices();
	lvts_device_read_count_RC_N();
	lvts_efuse_setting();

	lvts_thermal_disable_all_periodoc_temp_sensing();
	Enable_LVTS_CTRL_for_thermal_Data_Fetch();
	lvts_tscpu_thermal_initial_all_tc();
#endif
}

static void check_lvts_error(enum thermal_sensor mcu_index,
	enum thermal_sensor lvts_index)
{
	int temp;

	temp = tscpu_ts_temp[mcu_index] - tscpu_ts_temp[lvts_index];

	if (temp < 0)
		temp = temp * -1;

	/*Skip if LVTS thermal controllers doens't ready */
	if (temp > 100000)
		return;

	if (temp > LVTS_ERROR_THRESHOLD) {
		tscpu_printk("[LVTS_ERROR] %s:%d and %s:%d error: |%d| > %d\n",
			mcu_s_array[mcu_index],
			tscpu_ts_temp[mcu_index],
			mcu_s_array[lvts_index],
			tscpu_ts_temp[lvts_index],
			tscpu_ts_temp[mcu_index] -
			tscpu_ts_temp[lvts_index],
			LVTS_ERROR_THRESHOLD);
		g_lvts_e_data.e_occurred = 1;
		g_lvts_e_data.e_mcu = mcu_index;
		g_lvts_e_data.e_lvts = lvts_index;
		g_lvts_e_data.f_count = -1;
	}
}
void dump_lvts_error_data_info(void)
{
	char buffer[512];
	int offset, j;

	tscpu_printk("[LVTS_ERROR] c_index %d, e_occurred %d, f_count %d\n",
			g_lvts_e_data.c_index, g_lvts_e_data.e_occurred,
			g_lvts_e_data.f_count);
	tscpu_printk("[LVTS_ERROR] e_mcu %d, e_lvts %d\n", g_lvts_e_data.e_mcu,
			g_lvts_e_data.e_lvts);

	offset = sprintf(buffer, "[LVTS_ERROR][%s][DUMP] ",
			mcu_s_array[TS_LVTS1_0]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		offset += sprintf(buffer + offset, "%d ",
				g_lvts_e_data.ts_temp[TS_LVTS1_0][j]);

	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_raw][DUMP] ",
			mcu_s_array[TS_LVTS1_0]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		offset += sprintf(buffer + offset, "%d ",
				g_lvts_e_data.ts_temp_r[TS_LVTS1_0][j]);

	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);
}
#endif

int tscpu_read_temperature_info(struct seq_file *m, void *v)
{
	seq_printf(m, "curr_temp = %d\n", tscpu_get_curr_max_ts_temp());

#if !defined(CFG_THERM_NO_AUXADC)
	tscpu_dump_cali_info(m, v);
#endif

#if CFG_THERM_LVTS
	seq_puts(m, "-----------------\n");
	lvts_tscpu_dump_cali_info(m, v);
#endif
	return 0;
}
#if !IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
static int thermal_idle_notify_call(struct notifier_block *nfb,
				unsigned long id,
				void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
		break;
	case NOTIFY_SOIDLE_ENTER:
		break;
	case NOTIFY_DPIDLE_LEAVE:
		break;
	case NOTIFY_SOIDLE_LEAVE:
		break;
	case NOTIFY_SOIDLE3_ENTER:
#if LVTS_VALID_DATA_TIME_PROFILING
		SODI3_count++;
		if (SODI3_count != 1 && isTempValid == 0)
			noValid_count++;
		if (isTempValid == 1 || SODI3_count == 1)
			start_timestamp = thermal_get_current_time_us();
		isTempValid = 0;
#endif
		break;
	case NOTIFY_SOIDLE3_LEAVE:
#if CFG_THERM_LVTS
		lvts_sodi3_release_thermal_controller();
#endif
		break;
	default:
		/* do nothing */
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block thermal_idle_nfb = {
	.notifier_call = thermal_idle_notify_call,
};

#endif

#if IS_ENABLED(CONFIG_OF)
int get_io_reg_base(void)
{
	struct device_node *node = NULL;
	//struct device_node *rgu_node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6893-lvts");
	//node = of_find_node_by_name(NULL, "therm_ctrl");;
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);
	}

	/*tscpu_printk("[THERM_CTRL] rgu_base = 0x%px\n",rgu_base);*/
	/*get thermal irq num */
	thermal_irq_number = irq_of_parse_and_map(node, 0);
	tscpu_printk("[THERM_CTRL] thermal_irq_number=%d\n",
				thermal_irq_number);
	if (!thermal_irq_number) {
		/*TODO: need check "irq number"*/
		tscpu_printk("[THERM_CTRL] get irqnr failed=%d\n",
				thermal_irq_number);
		return 0;
	}

	/*get thermal irq num */
	thermal_mcu_irq_number = irq_of_parse_and_map(node, 1);
	tscpu_printk("[THERM_CTRL] mcu_irq_num = %d\n",
				thermal_mcu_irq_number);
	if (!thermal_mcu_irq_number) {
		/*TODO: need check "irq number"*/
		tscpu_printk("[THERM_CTRL] get irqnr failed=%d\n",
				thermal_mcu_irq_number);
		return 0;
	}


	if (of_property_read_u32_index(node, "reg", 1, &thermal_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error thermal_phy_base\n");
		return 0;
	}

	tscpu_printk("[THERM_CTRL] phy_base=0x%x\n", thermal_phy_base);



	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6765-auxadc");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		auxadc_ts_base = of_iomap(node, 0);
	}

	if (of_property_read_u32_index(node, "reg", 1, &auxadc_ts_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error auxadc_ts_phy_base\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL,  "mediatek,mt6893-infracfg_ao");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		infracfg_ao_base = of_iomap(node, 0);
#if !IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
		if (infracfg_ao_base)
			mtk_idle_notifier_register(&thermal_idle_nfb);
#endif
	}

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6893-apmixedsys");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		th_apmixed_base = of_iomap(node, 0);
	}

	if (of_property_read_u32_index(node, "reg", 1, &apmixed_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error apmixed_phy_base=\n");
		return 0;
	}

#if THERMAL_GET_AHB_BUS_CLOCK
	/* TODO: If this is required, it needs to confirm which node to read. */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infrasys");
	if (!node) {
		pr_notice("[CLK_INFRACFG_AO] find node failed\n");
		return 0;
	}
	therm_clk_infracfg_ao_base = of_iomap(node, 0);
	if (!therm_clk_infracfg_ao_base) {
		pr_notice("[CLK_INFRACFG_AO] base failed\n");
		return 0;
	}
#endif
	return 1;
}
#endif

/* chip dependent */
int tscpu_thermal_clock_on(void)
{
	int ret = -1;

	/* Use CCF */
	ret = clk_prepare_enable(therm_main);
	if (ret)
		tscpu_printk("Cannot enable thermal clock.\n");

	return ret;
}

/* chip dependent */
int tscpu_thermal_clock_off(void)
{
	int ret = -1;

	/* Use CCF */
	clk_disable_unprepare(therm_main);
	return ret;
}
#if CFG_THERMAL_KERNEL_IGNORE_HOT_SENSOR
#define CPU_SEGMENT 7
int tscpu_check_cpu_segment(void)
{
	int val = (get_devinfo_with_index(CPU_SEGMENT) & 0xFF);

	if (val == 0x30)
		ignore_hot_sensor = 1;
	else
		ignore_hot_sensor = 0;
	return val;
}
#endif
