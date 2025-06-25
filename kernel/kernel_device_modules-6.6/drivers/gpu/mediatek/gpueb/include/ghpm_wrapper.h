/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GHPM_WRAPPER_H__
#define __GHPM_WRAPPER_H__

#include <linux/platform_device.h>

#define GHPM_TEST                          (0)       /* use proc node to test ghpm on/off */
#define GPUEB_SLEEP_IPI_MAGIC_NUMBER       (0x55667788)

#define GHPM_PROFILE_TYPE_STRING(type) \
	( \
		type == PROF_GHPM_CTRL_OFF ? __stringify(CTRL_OFF) : \
		type == PROF_GHPM_CTRL_ON ? __stringify(CTRL_ON) : \
		type == PROF_GHPM_WAIT_OFF ? __stringify(WAIT_OFF) : \
		type == PROF_GHPM_WAIT_ON ? __stringify(WAIT_ON) : \
		"UNKNOWN" \
	)

enum ghpm_init_ret {
	GHPM_INIT_SUCCESS,
	GHPM_INIT_ERR
};

enum ghpm_ctrl_ret {
	GHPM_SUCCESS,
	GHPM_ERR,
	GHPM_STATE_ERR,
	GHPM_PWR_STATE_ERR,
	GHPM_DUPLICATE_ON_ERR,
	GHPM_INPUT_ERR,
	GHPM_OFF_EB_IPI_ERR,
	GHPM_SWWA_ERR,
	GHPM_CLK_EB_CK_ERR
};

enum ghpm_state {
	GHPM_OFF,
	GHPM_ON
};

enum gpueb_low_power_event {
	SUSPEND_POWER_OFF,  /* Suspend */
	SUSPEND_POWER_ON    /* Resume */
};

enum gpueb_low_power_state {
	GPUEB_ON_SUSPEND,
	GPUEB_ON_RESUME,
};

enum mfg0_off_state {
	MFG1_OFF,  /* legacy on/off, mfg0 off and mfg1 off */
	MFG1_ON    /* vcore off allow state, mfg0 off but mfg1 on */
};

enum wait_gpueb_ret {
	WAIT_DONE,
	WAIT_TIMEOUT,
	WAIT_INPUT_ERROR
};

enum progress_status {
	NOT_IN_PROGRESS,
	POWER_ON_IN_PROGRESS,
	POWER_OFF_IN_PROGRESS
};

/* Kbase record GHPM status */
enum gpu_ghpm_state {
	GHPM_POWER_OFF = 0,
	GHPM_POWER_ON = 1,
};

enum ghpm_profile_type {
	PROF_GHPM_CTRL_OFF  = 0,
	PROF_GHPM_CTRL_ON   = 1,
	PROF_GHPM_WAIT_OFF  = 2,
	PROF_GHPM_WAIT_ON   = 3,
	PROF_GHPM_TYPE_NUM  = 4
};

enum ghpm_profile_index {
	PROF_GHPM_IDX_START = 0,
	PROF_GHPM_IDX_END   = 1,
	PROF_GHPM_IDX_COUNT = 2,
	PROF_GHPM_IDX_LAST  = 3,
	PROF_GHPM_IDX_TOTAL = 4,
	PROF_GHPM_IDX_AVG   = 5,
	PROF_GHPM_IDX_MAX   = 6,
	PROF_GHPM_IDX_MIN   = 7,
	PROF_GHPM_IDX_NUM   = 8,
};

enum ghpm_profile_op {
	PROF_GHPM_OP_START  = 0,
	PROF_GHPM_OP_END    = 1,
};

struct gpueb_slp_ipi_data {
	enum gpueb_low_power_event event;
	enum mfg0_off_state off_state;
	int magic;
};

struct ghpm_platform_fp {
	int (*ghpm_ctrl)(enum ghpm_state, enum mfg0_off_state);
	int (*wait_gpueb)(enum gpueb_low_power_event);
	void (*dump_ghpm_info)(void);
};

extern unsigned int g_ghpm_ready;
extern unsigned int g_ghpm_support;
extern unsigned long long g_ghpm_profile[PROF_GHPM_TYPE_NUM][PROF_GHPM_IDX_NUM];

void ghpm_wrapper_init(struct platform_device *pdev);

void ghpm_register_ghpm_fp(struct ghpm_platform_fp *platform_fp);
int ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state);
int wait_gpueb(enum gpueb_low_power_event event);
int gpueb_ctrl(enum ghpm_state power,
	enum mfg0_off_state off_state, enum gpueb_low_power_event event);
void dump_ghpm_info(void);
void ghpm_profile(unsigned int type, unsigned int op);

#endif /* __GHPM_WRAPPER_H__ */
