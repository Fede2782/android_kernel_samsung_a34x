// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>

#include "apu_dbg.h"
#include "apu_power_table.h"

#ifdef MT6877_APU_PWR_DRV
#define APUSYS_VPU_NUM  (2)
#define APUSYS_MDLA_NUM (1)
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 212},
	{APU_OPP_1, 176},
	{APU_OPP_2, 133},
	{APU_OPP_3, 98},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(vpu_power_table);

/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 161},
	{APU_OPP_1, 161},
	{APU_OPP_2, 120},
	{APU_OPP_3, 86},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(mdla_power_table);

#elif defined(MT6893_APU_PWR_DRV)
#define APUSYS_VPU_NUM	(3)
#define APUSYS_MDLA_NUM	(2)
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 242},
	{APU_OPP_1, 188},
	{APU_OPP_2, 142},
	{APU_OPP_3, 136},
	{APU_OPP_4, 127},
	{APU_OPP_5, 100},
};
EXPORT_SYMBOL(vpu_power_table);

/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 200},
	{APU_OPP_1, 200},
	{APU_OPP_2, 159},
	{APU_OPP_3, 157},
	{APU_OPP_4, 117},
	{APU_OPP_5, 110},
};
EXPORT_SYMBOL(mdla_power_table);

#elif defined(MT6853_APU_PWR_DRV)
#define APUSYS_VPU_NUM  (2)
#define APUSYS_MDLA_NUM (0)

/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 212},
	{APU_OPP_1, 176},
	{APU_OPP_2, 133},
	{APU_OPP_3, 98},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(vpu_power_table);

/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 0},
	{APU_OPP_1, 0},
	{APU_OPP_2, 0},
	{APU_OPP_3, 0},
	{APU_OPP_4, 0},
};
EXPORT_SYMBOL(mdla_power_table);

#else
#define APUSYS_VPU_NUM  (0)
#define APUSYS_MDLA_NUM (0)
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 0},
	{APU_OPP_1, 0},
	{APU_OPP_2, 0},
	{APU_OPP_3, 0},
	{APU_OPP_4, 0},
};
EXPORT_SYMBOL(vpu_power_table);

/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 0},
	{APU_OPP_1, 0},
	{APU_OPP_2, 0},
	{APU_OPP_3, 0},
	{APU_OPP_4, 0},
};
EXPORT_SYMBOL(mdla_power_table);
#endif

static int _thermal_throttle(enum DVFS_USER limit_user, enum APU_OPP_INDEX opp)
{
	enum DVFS_USER user;

	switch (limit_user) {
	case VPU0:
		for (user = VPU0 ; user < VPU0 + APUSYS_VPU_NUM ; user++)
			apupw_thermal_limit(user, opp);
	break;
	case MDLA0:
		for (user = MDLA0 ; user < MDLA0 + APUSYS_MDLA_NUM ; user++)
			apupw_thermal_limit(user, opp);
	break;
	default:
		pr_info("%s invalid limit_user : %d\n", __func__, limit_user);
		return -1;
	}

	return 0;
}

int32_t apusys_thermal_en_throttle_cb(enum DVFS_USER limit_user,
		enum APU_OPP_INDEX opp)
{
	int ret = 0;

	pr_info("%s limit_user : %d, limit_opp : %d\n",
			__func__, limit_user, opp);

	ret = _thermal_throttle(limit_user, opp);

	return ret;
}
EXPORT_SYMBOL(apusys_thermal_en_throttle_cb);

int32_t apusys_thermal_dis_throttle_cb(enum DVFS_USER limit_user)
{
	int ret = 0;

	pr_info("%s limit_user : %d\n", __func__, limit_user);

	ret = _thermal_throttle(limit_user, 0);

	return ret;
}
EXPORT_SYMBOL(apusys_thermal_dis_throttle_cb);
