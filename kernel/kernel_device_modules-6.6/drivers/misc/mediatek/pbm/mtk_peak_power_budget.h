/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_PEAK_POWER_BUDGETING_H__
#define __MTK_PEAK_POWER_BUDGETING_H__


enum ppb_kicker {
	KR_BUDGET,
	KR_FLASHLIGHT,
	KR_AUDIO,
	KR_CAMERA,
	KR_DISPLAY,
	KR_APU,
	KR_NUM
};

enum ppb_sram_offset {
	PPB_VSYS_PWR_NOERR,
	HPT_SF_ENABLE,
	HPT_CPU_B_SF_L1,
	HPT_CPU_B_SF_L2,
	HPT_CPU_M_SF_L1,
	HPT_CPU_M_SF_L2,
	HPT_CPU_L_SF_L1,
	HPT_CPU_L_SF_L2,
	HPT_GPU_SF_L1,
	HPT_GPU_SF_L2,
	HPT_DELAY_TIME,
	PPB_MODE = 32,
	PPB_CG_PWR,
	PPB_VSYS_PWR,
	PPB_VSYS_ACK,
	PPB_FLASH_PWR,
	PPB_AUDIO_PWR,
	PPB_CAMERA_PWR,
	PPB_APU_PWR,
	PPB_DISPLAY_PWR,
	PPB_DRAM_PWR,
	PPB_MD_PWR,
	PPB_WIFI_PWR,
	PPB_RESERVE4,
	PPB_RESERVE5,
	PPB_APU_PWR_ACK,
	PPB_BOOT_MODE,
	PPB_MD_SMEM_ADDR,
	PPB_WIFI_SMEM_ADDR,
	PPB_CG_PWR_THD,
	PPB_CG_PWR_CNT,
	PPB_OFFSET_NUM
};

struct ppb_ctrl {
	u8 ppb_stop;
	u8 ppb_drv_done;
	u8 manual_mode;
	u8 ppb_mode;
};

enum hpt_ctrl_reg {
	HPT_CTRL,
	HPT_CTRL_SET,
	HPT_CTRL_CLR
};

struct ppb {
	unsigned int loading_flash;
	unsigned int loading_audio;
	unsigned int loading_camera;
	unsigned int loading_display;
	unsigned int loading_apu;
	unsigned int loading_dram;
	unsigned int vsys_budget;
	unsigned int vsys_budget_noerr;
	unsigned int remain_budget;
	unsigned int cg_budget_thd;
	unsigned int cg_budget_cnt;
};

struct power_budget_t {
	unsigned int version;
	int hpt_max_lv;
	int hpt_cur_lv;
	int hpt_lv_t[10];
	int soc;
	int uisoc;
	int combo0_uisoc;
	int fix_combo0[50];
	unsigned int uisoc_cur_stage;
	unsigned int uisoc_max_stage;
	int uisoc_thd[6];
	int temp;
	unsigned int soc_err;
	unsigned int temp_cur_stage;
	unsigned int temp_max_stage;
	int temp_thd[6];
	unsigned int aging_cur_stage;
	unsigned int aging_max_stage;
	unsigned int circuit_rdc;
	unsigned int rdc[7];
	unsigned int rac[7];
	unsigned int aging_thd[10];
	unsigned int aging_multi[10];
	unsigned int aging_rdc;
	unsigned int uvlo;
	unsigned int ocp;
	unsigned int cur_rdc;
	unsigned int cur_rac;
	unsigned int imax;
	unsigned int ocv_noerr;
	unsigned int sys_power_noerr;
	unsigned int bat_power_noerr;
	unsigned int ocv;
	unsigned int sys_power;
	unsigned int bat_power;
	struct work_struct bat_work;
	struct power_supply *psy;
	struct device *dev;
	unsigned int hpt_exclude_lbat_cg_thl;
};

struct ocv_table_t {
	unsigned int mah;
	unsigned int dod;
	unsigned int voltage;
	unsigned int rdc;
};

struct fg_info_t {
	int temp;
	int qmax;
	int ocv_table_size;
	struct ocv_table_t ocv_table[100];
};

struct fg_cus_data {
	unsigned int fg_info_size;
	unsigned int bat_type;
	struct fg_info_t fg_info[11];
};

struct ppb_ipi_data {
	unsigned int cmd;
};

struct xpu_dbg_t {
	unsigned int cpub_len;
	unsigned int cpum_len;
	unsigned int gpu_len;
	unsigned int cpub_cnt;
	unsigned int cpum_cnt;
	unsigned int gpu_cnt;
	unsigned int cpub_th_t;
	unsigned int cpum_th_t;
	unsigned int gpu_th_t;
};

extern void kicker_ppb_request_power(enum ppb_kicker kicker, unsigned int power);
extern int ppb_set_wifi_pwr_addr(unsigned int val);

#endif /* __MTK_PEAK_POWER_BUDGETING_H__ */
