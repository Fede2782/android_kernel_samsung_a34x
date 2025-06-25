// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef LINUX_POWER_MTK_CHG_TYPE_DET_H
#define LINUX_POWER_MTK_CHG_TYPE_DET_H

#define FAST_CHG_WATT 7500000 /* mW */

enum attach_type {
	ATTACH_TYPE_NONE = 0,
	ATTACH_TYPE_PWR_RDY,
	ATTACH_TYPE_TYPEC,
	ATTACH_TYPE_PD, /* Invalid type, JUST FOR DISTINCTION */
	ATTACH_TYPE_PD_SDP,
	ATTACH_TYPE_PD_DCP,
	ATTACH_TYPE_PD_NONSTD,
	ATTACH_TYPE_MAX
};

static const char *const attach_type_names[ATTACH_TYPE_MAX] = {
	[ATTACH_TYPE_NONE] = "Detach",
	[ATTACH_TYPE_PWR_RDY] = "Power Ready",
	[ATTACH_TYPE_TYPEC] = "Type-C",
	[ATTACH_TYPE_PD_SDP] = "PD - SDP",
	[ATTACH_TYPE_PD_DCP] = "PD - DCP",
	[ATTACH_TYPE_PD_NONSTD] = "PD - Non-Standard",
};

static inline const char *const get_attach_type_name(enum attach_type attach_type)
{
	if (attach_type >= ATTACH_TYPE_MAX || attach_type == ATTACH_TYPE_PD)
		return "Invalid Type";
	else
		return attach_type_names[attach_type];
}

#endif /*LINUX_POWER_MTK_CHG_TYPE_DET_H*/
