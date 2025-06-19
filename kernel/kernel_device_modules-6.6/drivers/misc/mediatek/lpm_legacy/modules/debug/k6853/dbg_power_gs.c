// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_power_gs_array.h"
#include <lpm_call.h>
#include <lpm_call_type.h>

#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>

int mt6853_pwr_gs_set(unsigned int type, const struct lpm_data *val)
{
	int ret = 0;

	if (val->d.v_u32 & GS_PMIC)
		ret = lpm_pwr_gs_compare(LPM_GS_CMP_PMIC, type);

	if (ret)
		return ret;

	return ret;
}

struct lpm_callee mt6853_pwr_gs_callee = {
	.uid = LPM_CALLEE_PWR_GS,
	.i.simple = {
		.set = mt6853_pwr_gs_set,
	},
};

/* PMIC */

struct lpm_gs_pmic mt6853_pmic6359p = {
	.type = GS_PMIC,
	.regulator = "vcore",
	.pwr_domain = "6359P",
};

#ifdef PMIC_6360
struct lpm_gs_pmic mt6853_pmic6315 = {
	.type = GS_PMIC,
	.regulator = "3_vbuck1",
	.pwr_domain = "6315",
};

struct lpm_gs_pmic *mt6853_pmic[] = {
	&mt6853_pmic6359p,
	&mt6853_pmic6315,
	NULL,
};
#else /* pmic 6362 */
struct lpm_gs_pmic mt6853_pmic6362 = {
	.type = GS_PMIC,
	.regulator = "mt6362-buck1",
	.pwr_domain = "6362",
};

struct lpm_gs_pmic *mt6853_pmic[] = {
	&mt6853_pmic6359p,
	&mt6853_pmic6362,
	NULL,
};
#endif /* CONFIG_REGULATOR_MT6362 */

int mt6853_power_gs_pmic_user_attach(struct lpm_gs_pmic *p)
{
	if (!p || !p->regulator)
		return -EINVAL;

	/* Set compare golden setting for scenario */
	if (!strcmp(p->regulator, "vcore")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_6359P_gs_suspend_32kless;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_6359P_gs_suspend_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_6359P_gs_sodi3p0_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_6359P_gs_sodi3p0_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_6359P_gs_deepidle___lp_mp3_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_6359P_gs_deepidle___lp_mp3_32kless_len;

	} else if (!strcmp(p->regulator, "3_vbuck1")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";

		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_MT6315_gs_suspend_32kless;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_MT6315_gs_suspend_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_MT6315_gs_sodi3p0_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_MT6315_gs_sodi3p0_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_MT6315_gs_deepidle___lp_mp3_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_MT6315_gs_deepidle___lp_mp3_32kless_len;

	} else if (!strcmp(p->regulator, "mt6362-buck1")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_MT6362_gs_suspend_32kless;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_MT6362_gs_suspend_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_MT6362_gs_sodi3p0_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_MT6362_gs_sodi3p0_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_MT6362_gs_deepidle___lp_mp3_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_MT6362_gs_deepidle___lp_mp3_32kless_len;

	} else
		return -EINVAL;

	return 0;
}

struct lpm_gs_pmic_info mt6853_pmic_infos = {
	.pmic = mt6853_pmic,
	.attach = mt6853_power_gs_pmic_user_attach,
};

int power_gs_init(void)
{
	lpm_callee_registry(&mt6853_pwr_gs_callee);

	/* initial gs compare method */
	lpm_pwr_gs_common_init();

	/* initial gs pmic information */
	lpm_pwr_gs_compare_init(LPM_GS_CMP_PMIC, &mt6853_pmic_infos);

	return 0;
}

int power_gs_deinit(void)
{
	lpm_pwr_gs_common_deinit();
	return 0;
}

