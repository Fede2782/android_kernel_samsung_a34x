/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */
#ifndef _MTK_MDPM_PLATFORM_COMMOM_H_
#define _MTK_MDPM_PLATFORM_COMMOM_H_

#include "../../pbm/mtk_pbm.h"

enum mdpm_power_type {
	MAX_POWER = 0,
	AVG_POWER,
	POWER_TYPE_NUM
};

extern void init_md_section_level(enum pbm_kicker kicker, u32 *share_mem);
extern int get_md1_power(enum mdpm_power_type power_type, bool need_update);

#endif /* _MTK_MDPM_PLATFORM_COMMOM_H_ */
