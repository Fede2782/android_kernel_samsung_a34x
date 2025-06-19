// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_adpater_switch.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   Support Multiple adapter protocol switch.
 *
 * Author:
 * -------
 *   KS Wang
 *
 */
#include "adapter_class.h"
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>

#include "charger_class.h"

/* dependent on platform */
#include "mtk_charger.h"
#include "mtk_charger_algorithm_class.h"

#define ADAPTER_SWITCH_ERROR_LEVEL	1
#define ADAPTER_SWITCH_INFO_LEVEL	2
#define ADAPTER_SWITCH_DEBUG_LEVEL	3
#define MAX_WAIT_TA_TIMES 3

static int adapter_switch_dbg_level =
	ADAPTER_SWITCH_DEBUG_LEVEL;

int adapter_switch_get_debug_level(void)
{
	return adapter_switch_dbg_level;
}
#define adapter_switch_err(fmt, args...)					\
do {								\
	if (adapter_switch_get_debug_level() >= ADAPTER_SWITCH_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define adapter_switch_info(fmt, args...)					\
do {								\
	if (adapter_switch_get_debug_level() >= ADAPTER_SWITCH_INFO_LEVEL) { \
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define adapter_switch_dbg(fmt, args...)					\
do {								\
	if (adapter_switch_get_debug_level() >= ADAPTER_SWITCH_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

int mtk_adapter_protocol_init(struct mtk_charger *info)
{
	int i = 0;
	for (i = 0;i < MAX_TA_IDX;i++)
		info->ta_status[i] = TA_DETACH;
	info->protocol_state = FIRST_HANDSHAKE;
	info->wait_times = 0;
	info->ta_capability = STD_TA;
	info->select_adapter = NULL;

	return 1;
}

void mtk_check_ta_status(struct mtk_charger *info)
{
	int state = 0;

	if (info->protocol_state == FIRST_HANDSHAKE ||
		(info->wait_times > 0 && !info->select_adapter)) {
		state = mtk_adapter_switch_control(info);
		adapter_switch_err("%s ret_ta_switch:%d, prtocol_state:%d\n",
			__func__, state, info->protocol_state);
		mtk_selected_adapter_ready(info);
	}
}

int mtk_selected_adapter_ready(struct mtk_charger *info)
{
	int ret = 0;
	int ta_type = 0;

	switch(info->protocol_state) {
	case RUN_ON_PD:
		ta_type = adapter_dev_get_property(info->adapter_dev[PD], CAP_TYPE);
		if (info->adapter_dev[PD] &&
			ta_type == MTK_PD_APDO) {
			adapter_switch_dbg("%s: run pd pps adapter\n", __func__);
			info->select_adapter = info->adapter_dev[PD];
			info->select_adapter_idx = PD;
			info->ta_capability = APDO_TA;
			ret = 1;
		} else if (info->adapter_dev[PD] &&
			ta_type != MTK_PD_APDO) {
			adapter_switch_dbg("%s: run pd adapter\n", __func__);
			info->select_adapter = info->adapter_dev[PD];
			info->select_adapter_idx = PD;
			info->ta_capability = WO_APDO_TA;
			ret = 1;
		} else {
			adapter_switch_dbg("%s: note : can't find pd adapter\n",
				__func__);
			info->ta_capability = STD_TA;
			ret = -1;
		}
	break;
	case RUN_ON_UFCS:
		if (info->adapter_dev[UFCS]) {
			adapter_switch_dbg("%s: run ufcs_adapter\n", __func__);
			info->select_adapter = info->adapter_dev[UFCS];
			info->select_adapter_idx = UFCS;
			info->ta_capability = APDO_TA;
			ret = 1;
		} else {
			adapter_switch_dbg("%s: note : can't find ufcs_adapter\n",
				__func__);
			info->ta_capability = STD_TA;
			return -1;
		}
	break;
	}
	return ret;
}

int mtk_adapter_switch_control(struct mtk_charger *info)
{
	int ret = 0;

	switch (info->setting.adapter_priority) {
	case UFCS_FIRST:
		if (info->ta_status[UFCS] == TA_ATTACH) {		// UFCS
			ret = TA_READY;
			info->protocol_state = RUN_ON_UFCS;
		} else if (info->ta_status[PD] == TA_ATTACH) {		// PD
			ret = TA_READY;
			info->protocol_state = RUN_ON_PD;
		} else if (info->ta_status[UFCS] == TA_DETECT_FAIL
			&& info->ta_status[PD] == TA_DETECT_FAIL)		// BOTH FAILED
			ret = TA_NOT_SUPPORT;
		else
			ret = TA_NOT_SUPPORT;
	break;
	case UFCS_FIRST_AND_WAIT:
		if (info->ta_status[UFCS] == TA_ATTACH) {
			ret = TA_READY;
			info->protocol_state = RUN_ON_UFCS;
		} else if (info->ta_status[PD] == TA_ATTACH) {
			if (info->ta_status[UFCS] == TA_DETECT_FAIL) {
				ret = TA_READY;
				info->protocol_state = RUN_ON_PD;
			} else {
				if (info->wait_times >= MAX_WAIT_TA_TIMES) {
					ret = TA_READY;
					info->protocol_state = RUN_ON_PD;
				} else {
					info->wait_times++;
					ret = TA_NOT_READY;
				}
			}
		} else if (info->ta_status[UFCS] == TA_DETECT_FAIL
			&& info->ta_status[PD] == TA_DETECT_FAIL)
			ret = TA_NOT_SUPPORT;
		else
			ret = TA_NOT_SUPPORT;
	break;
	case PD_FIRST_AND_WAIT:
		if (info->ta_status[PD] == TA_ATTACH) {
			ret = TA_READY;
			info->protocol_state = RUN_ON_PD;
		} else if (info->ta_status[UFCS] == TA_ATTACH) {
			if (info->ta_status[PD] == TA_DETECT_FAIL) {
				ret = TA_READY;
				info->protocol_state = RUN_ON_UFCS;
			} else {
				if (info->wait_times >= MAX_WAIT_TA_TIMES) {
					ret = TA_READY;
					info->protocol_state = RUN_ON_UFCS;
				} else {
					info->wait_times++;
					ret = TA_NOT_READY;
				}
			}
		} else if (info->ta_status[UFCS] == TA_DETECT_FAIL
			&& info->ta_status[PD] == TA_DETECT_FAIL)
			ret = TA_NOT_SUPPORT;
		else
			ret = TA_NOT_SUPPORT;
	break;
	default:
		if (info->ta_status[PD] == TA_ATTACH) {
			ret = TA_READY;
			info->protocol_state = RUN_ON_PD;
		} else if (info->ta_status[UFCS] == TA_ATTACH) {
			ret = TA_READY;
			info->protocol_state = RUN_ON_UFCS;
		} else if (info->ta_status[UFCS] == TA_DETECT_FAIL
			&& info->ta_status[PD] == TA_DETECT_FAIL)
			ret = TA_NOT_SUPPORT;
		else
			ret = TA_NOT_SUPPORT;
	break;
	}

	adapter_switch_err("%s pd_type:%d, ufcs_type:%d, ret:%d, state: %d, set_pri:%d, cnt:%d\n"
		, __func__, info->ta_status[PD], info->ta_status[UFCS], ret
		, info->protocol_state, info->setting.adapter_priority, info->wait_times);

	return ret;
}
