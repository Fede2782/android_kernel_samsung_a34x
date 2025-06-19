// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    ghpm_wrapper.c
 * @brief   GHPM Wrapper Init and API
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>
#include <uapi/asm-generic/errno-base.h>

#include "gpueb_common.h"
#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "ghpm_wrapper.h"
#include "ghpm_debug.h"

static struct ghpm_platform_fp *ghpm_fp;
unsigned int g_ghpm_support;
EXPORT_SYMBOL(g_ghpm_support);

unsigned long long g_ghpm_profile[PROF_GHPM_TYPE_NUM][PROF_GHPM_IDX_NUM];
EXPORT_SYMBOL(g_ghpm_profile);

int ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state)
{
	int ret = -ENOENT;

	if (!g_ghpm_support)
		return 0;

	if (ghpm_fp && ghpm_fp->ghpm_ctrl)
		ret = ghpm_fp->ghpm_ctrl(power, off_state);
	else
		gpueb_log_e(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");

	return ret;
}
EXPORT_SYMBOL(ghpm_ctrl);

int wait_gpueb(enum gpueb_low_power_event event)
{
	int ret = -ENOENT;

	if (!g_ghpm_support)
		return 0;

	if (ghpm_fp && ghpm_fp->wait_gpueb)
		ret = ghpm_fp->wait_gpueb(event);
	else
		gpueb_log_e(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");

	return ret;
}
EXPORT_SYMBOL(wait_gpueb);

int gpueb_ctrl(enum ghpm_state power,
	enum mfg0_off_state off_state, enum gpueb_low_power_event event)
{
	int ret = -ENOENT;

	if (!g_ghpm_support)
		return 0;

	ret = ghpm_ctrl(power, off_state);
	if (ret) {
		gpueb_log_e(GHPM_TAG, "gpueb ctrl fail, power=%d, off_state=%d, event=%d, ret=%d",
			power, off_state, event, ret);
		return ret;
	}

	ret = wait_gpueb(event);
	if (ret) {
		gpueb_log_e(GHPM_TAG, "wait gpueb fail, power=%d, off_state=%d, event=%d, ret=%d",
			power, off_state, event, ret);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(gpueb_ctrl);

void dump_ghpm_info(void)
{
	if (!g_ghpm_support)
		return;

	if (ghpm_fp && ghpm_fp->dump_ghpm_info)
		ghpm_fp->dump_ghpm_info();
	else
		gpueb_log_e(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");
}
EXPORT_SYMBOL(dump_ghpm_info);

void ghpm_profile(unsigned int type, unsigned int op)
{
	if (g_ghpm_profile_enable == 0)
		return;

	if (type >= PROF_GHPM_TYPE_NUM) {
		gpueb_log_e(GHPM_TAG, "incorrect type: %d", type);
	} else if (op == PROF_GHPM_OP_START) {
		g_ghpm_profile[type][PROF_GHPM_IDX_START] = sched_clock();
	} else if (op == PROF_GHPM_OP_END) {
		g_ghpm_profile[type][PROF_GHPM_IDX_END] = sched_clock();
		g_ghpm_profile[type][PROF_GHPM_IDX_COUNT]++;
		g_ghpm_profile[type][PROF_GHPM_IDX_LAST] =
			(g_ghpm_profile[type][PROF_GHPM_IDX_END] - g_ghpm_profile[type][PROF_GHPM_IDX_START]) / 1000;
		g_ghpm_profile[type][PROF_GHPM_IDX_TOTAL] += g_ghpm_profile[type][PROF_GHPM_IDX_LAST];
		g_ghpm_profile[type][PROF_GHPM_IDX_AVG] =
			g_ghpm_profile[type][PROF_GHPM_IDX_TOTAL] / g_ghpm_profile[type][PROF_GHPM_IDX_COUNT];
		if (g_ghpm_profile[type][PROF_GHPM_IDX_LAST] > g_ghpm_profile[type][PROF_GHPM_IDX_MAX])
			g_ghpm_profile[type][PROF_GHPM_IDX_MAX] = g_ghpm_profile[type][PROF_GHPM_IDX_LAST];
		if (g_ghpm_profile[type][PROF_GHPM_IDX_LAST] < g_ghpm_profile[type][PROF_GHPM_IDX_MIN] ||
			g_ghpm_profile[type][PROF_GHPM_IDX_MIN] == 0)
			g_ghpm_profile[type][PROF_GHPM_IDX_MIN] = g_ghpm_profile[type][PROF_GHPM_IDX_LAST];
	}
}
EXPORT_SYMBOL(ghpm_profile);

void ghpm_register_ghpm_fp(struct ghpm_platform_fp *platform_fp)
{
	if (!platform_fp) {
		gpueb_log_e(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");
		return;
	}

	ghpm_fp = platform_fp;
	gpueb_log_d(GHPM_TAG, "Hook ghpm platform function pointer done");
}
EXPORT_SYMBOL(ghpm_register_ghpm_fp);

void ghpm_wrapper_init(struct platform_device *pdev)
{
	of_property_read_u32(pdev->dev.of_node, "ghpm-support", &g_ghpm_support);

	if (g_ghpm_support == 0)
		gpueb_log_i(GHPM_TAG, "no ghpm support");
	else
		ghpm_debug_init(pdev);
}
