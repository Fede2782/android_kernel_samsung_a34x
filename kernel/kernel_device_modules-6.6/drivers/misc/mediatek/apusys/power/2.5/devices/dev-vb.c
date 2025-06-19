// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/devfreq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>

#include "apu_plat.h"
#include "apu_common.h"
#include "apu_devfreq.h"
#include "apu_clk.h"
#include "apu_regulator.h"
#include "apu_log.h"
#include "apu_rpc.h"
#include "apusys_power.h"
#include "apu_of.h"
#include "apu_trace.h"

#define MT6877_VPU_TBL_SZ   3
char *mt6877_vpu_tables[MT6877_VPU_TBL_SZ] = {
	"APUCORE:APUCONN", "apusys_power:APUVPU", "apusys_power:APUMDLA"
};

#define MT6893_BIN_OFSET  3
#define MT6893_VPU_TBL_SZ   3
char *mt6893_vpu_tables[MT6893_VPU_TBL_SZ] = {
	"soc:APUCORE:APUCONN","soc:APUCORE:APUCONN:APUIOMMU", "soc:apusys_power:APUVPU@0"
};

#define MT6893_MDLA_TBL_SZ   1
char *mt6893_mdla_tables[MT6893_MDLA_TBL_SZ] = {
	"soc:apusys_power:APUMDLA"
};

#define MT6853_VPU_TBL_SZ   1
char *mt6853_vpu_tables[MT6877_VPU_TBL_SZ] = {
	"soc:APUCORE:APUCONN",
};

/* index 0: vpu low bound of 0.65v
 * index 1: mdla low bound of 0.65v
 */
unsigned long mt6893_LB_650mv[2][4][6] = {
	/* vpu */
	{{800000, 750000, 700000, 650000, 575000, 575000}, /* bin 0 */
	 {775000, 750000, 700000, 650000, 575000, 575000}, /* bin 1 */
	 {762500, 737500, 700000, 650000, 575000, 575000}, /* bin 2 */
	 {750000, 731250, 700000, 650000, 575000, 575000}, /* bin 3 */
	},
	/* mdla */
	{{825000, 800000, 750000, 700000, 650000, 575000}, /* bin 0 */
	 {800000, 768750, 737500, 693750, 650000, 575000}, /* bin 1 */
	 {787500, 762500, 731250, 687500, 650000, 575000}, /* bin 2 */
	 {775000, 750000, 718750, 687500, 650000, 575000}, /* bin 3 */
	},
};

/**
 * _get_sign_v() - get signed voltage from freq
 * @ad: apu devcie
 * @f: input freq
 * @v: output find v
 *
 * according intput f, return singend off voltage.
 */
static int __get_sign_v(struct device *dev, unsigned long f, unsigned long *v)
{
	struct device_node *opp_np, *np;
	int ret = 0;
	u64 rate = 0;
	u32 tmp_v = 0;

	opp_np = of_parse_phandle(dev->of_node, "operating-points-v2", 0);
	if (!opp_np)
		return -ENOENT;

	/* We have opp-table node now, iterate over it and managing binning/aging */
	for_each_available_child_of_node(opp_np, np) {
		ret = of_property_read_u64(np, "opp-hz", &rate);
		if (ret < 0) {
			dev_info(dev, "%s: opp-hz not found\n", __func__);
			goto out;
		}

		if (rate == f) {
			ret = of_property_read_u32(np, "opp-microvolt", &tmp_v);
			if (ret < 0) {
				dev_info(dev, "%s: opp-microvolt not found\n", __func__);
				goto out;
			} else {
				*v = (unsigned long) tmp_v;
				break;
			}
		}
	}
out:
	return ret;
}

/**
 * _set_vb_v() - min(vb, lower bound 0.65v)
 * @dev: struct device, used for checking child number
 * @ofdev: array of struct device to compare
 * @dev_cnt: how many ofdev to compare
 * @lb: vpu or mdla lower bound
 * @bin_offset: offset of bin value
 *
 * min(vb, lower bound of 0.6v).
 */
static int _mt6893_set_vb_v(struct device *dev,
	struct device **ofdev, int dev_cnt, unsigned long lb[4][6], int bin_offset)
{
	struct dev_pm_opp **opp;
	int ret = 0, idx1 = 0, idx2 = 0, opp_cnt = 0;
	unsigned long *tmp_f = NULL, *tmp_v = NULL;
	unsigned long max_v = 0, sign_v = 0, lb_v;
	struct apu_dev *ad = NULL;

	ad = dev_get_drvdata(ofdev[0]);

	if (ad->seg_idx)
		goto out;

	opp_cnt = dev_pm_opp_get_opp_count(ofdev[0]);
	if (!opp_cnt)
		goto out;
	tmp_f = devm_kzalloc(dev, (sizeof(tmp_f) * dev_cnt), GFP_KERNEL);
	if (!tmp_f)
		goto free_f;
	tmp_v = devm_kzalloc(dev, (sizeof(tmp_f) * dev_cnt), GFP_KERNEL);
	if (!tmp_v)
		goto free_v;
	opp = devm_kzalloc(dev, (sizeof(*opp) * dev_cnt), GFP_KERNEL);
	if (!opp)
		goto free_opp;

	for (idx2 = 0; idx2 < dev_cnt; idx2++)
		tmp_f[idx2] = ULONG_MAX;

	/* align bin_idx to 0 */
	lb_v = ULONG_MAX;
	if (ad->bin_idx > bin_offset)
		ad->bin_idx -= bin_offset;
	else
		ad->bin_idx = 0;

	 /* Bypass slowest opp, since it will be raised and comparison is no need */
	for (idx1 = 0; idx1 < opp_cnt -1; idx1++) {
		max_v = 0;
		/* record f/v with opp on differnt table */
		for (idx2 = 0; idx2 < dev_cnt; idx2++) {
			opp[idx2] = dev_pm_opp_find_freq_floor(ofdev[idx2], &tmp_f[idx2]);
			tmp_f[idx2] = dev_pm_opp_get_freq(opp[idx2]);
			tmp_v[idx2] = dev_pm_opp_get_voltage(opp[idx2]);
			max_v = max3(0UL, max_v, tmp_v[idx2]);
		}
		ret = __get_sign_v(ofdev[0], tmp_f[0], &sign_v);
		if (ret)
			goto free_opp;

		/* only bin > 0 and seg_idx == 0 need lower bound table */
		if (ad->bin_idx && !ad->seg_idx)
			lb_v = lb[ad->bin_idx][idx1];

		dev_info(dev, "opp %d, %s bin/seg %d/%d t_f/m_v/s_v/lb_v = %lu/%lu/%lu/%lu\n",
				idx1, ad->name, ad->bin_idx, ad->seg_idx,
				tmp_f[0], max_v, sign_v, lb_v);
		max_v = min3(max_v, sign_v, lb_v);

		/* update index 0 */
		ret = dev_pm_opp_adjust_voltage(ofdev[0], tmp_f[0],
						(ulong)max_v,
						(ulong)max_v,
						(ulong)max_v);
		for (idx2 = 0; idx2 < dev_cnt; idx2++)
			tmp_f[idx2] --;
	}

free_opp:
	devm_kfree(dev, opp);
free_f:
	devm_kfree(dev, tmp_f);
free_v:
	devm_kfree(dev, tmp_v);
out:
	return ret;
}


static int mt6893_vb_lb(struct device *dev)
{
	int err = 0, idx = 0;
	struct device **ofdev0 = NULL, **ofdev1 = NULL;

	ofdev0 = devm_kzalloc(dev, (sizeof(*ofdev0) * MT6893_VPU_TBL_SZ), GFP_KERNEL);
	if (!ofdev0) {
		err = -ENOMEM;
		goto out;
	}

	ofdev1 = devm_kzalloc(dev, (sizeof(*ofdev0) * MT6893_MDLA_TBL_SZ), GFP_KERNEL);
	if (!ofdev1) {
		err = -ENOMEM;
		goto out;
	}

	/* get interpolate need devs */
	for (idx = 0; idx < MT6893_VPU_TBL_SZ; idx++)
		ofdev0[idx] = bus_find_device_by_name(&platform_bus_type,
									NULL, mt6893_vpu_tables[idx]);
	err = _mt6893_set_vb_v(dev, ofdev0, MT6893_VPU_TBL_SZ,
				mt6893_LB_650mv[0], MT6893_BIN_OFSET);
	if (err)
		goto free_ofdev;

	dev_info(dev, "Final opp table %s\n", dev_name(ofdev0[0]));
	apu_dump_opp_table((struct apu_dev *)dev_get_drvdata(ofdev0[0]), "APUVB final", 1);
	dev_info(dev, "APUVB final ----------------------\n");

	/* get interpolate need devs */
	for (idx = 0; idx < MT6893_MDLA_TBL_SZ; idx++)
		ofdev1[idx] = bus_find_device_by_name(&platform_bus_type,
									NULL, mt6893_mdla_tables[idx]);
	err = _mt6893_set_vb_v(dev, ofdev1, MT6893_MDLA_TBL_SZ,
				mt6893_LB_650mv[1], MT6893_BIN_OFSET);
	if (err)
		goto free_ofdev;

	dev_info(dev, "Final opp table %s\n", dev_name(ofdev1[0]));
	apu_dump_opp_table((struct apu_dev *)dev_get_drvdata(ofdev1[0]), "APUVB final", 1);
	dev_info(dev, "APUVB final ----------------------\n");

free_ofdev:
	devm_kfree(dev, ofdev0);
	devm_kfree(dev, ofdev1);
out:
	return err;
}

/**
 * _mt6877_set_vb_v() - min(VPU_vb, TOP_vb, DLA_vb)
 * @dev: struct device, used for checking child number
 * @ofdev: array of struct device to compare
 * @dev_cnt: how many ofdev to compare
 *
 * min(VPU_vb, TOP_vb, DLA_vb).
 */
static int _mt6877_set_vb_v(struct device *dev,
	struct device **ofdev, int dev_cnt)
{
	struct dev_pm_opp **opp;
	int ret = 0, idx1 = 0, idx2 = 0, opp_cnt = 0;
	unsigned long *tmp_f = NULL, *tmp_v = NULL;
	unsigned long max_v = 0, sign_v = 0, raise_v = 0;
	struct apu_dev *ad = NULL;

	ad = dev_get_drvdata(ofdev[0]);

	if (ad->seg_idx)
		goto out;

	opp_cnt = dev_pm_opp_get_opp_count(ofdev[0]);
	if (!opp_cnt)
		goto out;
	tmp_f = devm_kzalloc(dev, (sizeof(tmp_f) * dev_cnt), GFP_KERNEL);
	if (!tmp_f)
		goto free_f;
	tmp_v = devm_kzalloc(dev, (sizeof(tmp_f) * dev_cnt), GFP_KERNEL);
	if (!tmp_v)
		goto free_v;
	opp = devm_kzalloc(dev, (sizeof(*opp) * dev_cnt), GFP_KERNEL);
	if (!opp)
		goto free_opp;

	/* Get Raise v 1st */
	tmp_f[0] = 0;
	opp[0] = dev_pm_opp_find_freq_ceil(ofdev[0], &tmp_f[0]);
	raise_v = dev_pm_opp_get_voltage(opp[0]);

	for (idx2 = 0; idx2 < dev_cnt; idx2++)
		tmp_f[idx2] = ULONG_MAX;

	 /* Bypass slowest opp, since it will be raised and comparison is no need */
	for (idx1 = 0; idx1 < opp_cnt -1; idx1++) {
		max_v = 0;
		/* record f/v with opp on differnt table */
		for (idx2 = 0; idx2 < dev_cnt; idx2++) {
			opp[idx2] = dev_pm_opp_find_freq_floor(ofdev[idx2], &tmp_f[idx2]);
			tmp_f[idx2] = dev_pm_opp_get_freq(opp[idx2]);
			tmp_v[idx2] = dev_pm_opp_get_voltage(opp[idx2]);
			max_v = max3(0UL, max_v, tmp_v[idx2]);
		}
		ret = __get_sign_v(ofdev[0], tmp_f[0], &sign_v);
		if (ret)
			goto free_opp;

		dev_info(dev, "opp %d, %s bin_h/bin_m/seg %d/%d/%d\n",
				idx1, ad->name, ad->bin_h_idx, ad->bin_m_idx, ad->seg_idx);

		dev_info(dev, "opp %d, %s t_f/m_v/s_v/r_v = %lu/%lu/%lu/%lu\n",
				idx1, ad->name, tmp_f[0], max_v, sign_v, raise_v);

		/* (A) HV <-> MV
		 * (B) MV <-> LV and raise_v = 575000
		 *
		 * Above 2 cased need to min(vmin, signed)
		 */
		if (idx1 <= VVPU_BIN_MIDV_OPP ||
			(idx1 > VVPU_BIN_MIDV_OPP && raise_v == 575000))
			max_v = min(max_v, sign_v);

		/* update index 0 */
		ret = dev_pm_opp_adjust_voltage(ofdev[0], tmp_f[0],
						(ulong)max_v,
						(ulong)max_v,
						(ulong)max_v);

		/* decrease freq for dev_pm_opp_find_freq_floor to get next opp */
		for (idx2 = 0; idx2 < dev_cnt; idx2++)
			tmp_f[idx2] --;
	}

free_opp:
	devm_kfree(dev, opp);
free_f:
	devm_kfree(dev, tmp_f);
free_v:
	devm_kfree(dev, tmp_v);
out:
	return ret;
}

static int mt6877_vb_lb(struct device *dev)
{
	int err = 0, idx = 0;
	struct device **ofdev0 = NULL;

	ofdev0 = devm_kzalloc(dev, (sizeof(*ofdev0) * MT6877_VPU_TBL_SZ), GFP_KERNEL);
	if (!ofdev0) {
		err = -ENOMEM;
		goto out;
	}

	/* get interpolate need devs */
	for (idx = 0; idx < MT6877_VPU_TBL_SZ; idx++)
		ofdev0[idx] = bus_find_device_by_name(&platform_bus_type,
									NULL, mt6877_vpu_tables[idx]);

	err = _mt6877_set_vb_v(dev, ofdev0, MT6877_VPU_TBL_SZ);
	if (err)
		goto free_ofdev;

	dev_info(dev, "Final opp table %s\n", dev_name(ofdev0[0]));
	apu_dump_opp_table((struct apu_dev *)dev_get_drvdata(ofdev0[0]), "APUVB final", 1);
	dev_info(dev, "APUVB final ----------------------\n");

free_ofdev:
	devm_kfree(dev, ofdev0);
out:
	return err;
}

static int mt6853_vb_lb(struct device *dev)
{
	int err = 0, idx = 0;
	struct device **ofdev0 = NULL;

	ofdev0 = devm_kzalloc(dev, (sizeof(*ofdev0) * MT6853_VPU_TBL_SZ), GFP_KERNEL);
	if (!ofdev0) {
		err = -ENOMEM;
		goto out;
	}

	/* get interpolate need devs */
	for (idx = 0; idx < MT6853_VPU_TBL_SZ; idx++)
		ofdev0[idx] = bus_find_device_by_name(&platform_bus_type,
									NULL, mt6853_vpu_tables[idx]);

	dev_info(dev, "Final opp table %s\n", dev_name(ofdev0[0]));
	apu_dump_opp_table((struct apu_dev *)dev_get_drvdata(ofdev0[0]), "APUVB final", 1);
	dev_info(dev, "APUVB final ----------------------\n");

	devm_kfree(dev, ofdev0);
out:
	return err;
}

static int vb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct apu_plat_data *apu_data = NULL;
	int err = 0;

	dev_info(&pdev->dev, "%s dev->name %s\n", __func__, dev_name(&pdev->dev));
	apu_data = of_device_get_match_data(&pdev->dev);
	if (!apu_data) {
		dev_info(dev, " has no platform data, ret %d\n", err);
		err = -ENODEV;
		goto out;
	}

	err = apu_data->vb_lb(dev);
out:
	return err;
}


static int vb_remove(struct platform_device *pdev)
{
	struct apu_dev *ad = platform_get_drvdata(pdev);

	of_platform_depopulate(ad->dev);
	devm_kfree(ad->dev, ad);
	return 0;
}

#define MT688x_VB_TABLE_CNT    3
static const struct apu_plat_data mt6893_vb_data = {
	.user = APUVB,
	.vb_lb = mt6893_vb_lb,
};

static const struct apu_plat_data mt6877_vb_data = {
	.user = APUVB,
	.vb_lb = mt6877_vb_lb,
};

static const struct apu_plat_data mt6853_vb_data = {
	.user = APUVB,
	.vb_lb = mt6853_vb_lb,
};

static const struct of_device_id vb_of_match[] = {
	{ .compatible = "mtk6893,vb", .data = &mt6893_vb_data},
	{ .compatible = "mtk6877,vb", .data = &mt6877_vb_data},
	{ .compatible = "mtk6853,vb", .data = &mt6853_vb_data},
	{ },
};

MODULE_DEVICE_TABLE(of, vb_of_match);

struct platform_driver vb_driver = {
	.probe	= vb_probe,
	.remove	= vb_remove,
	.driver = {
		.name = "mtk68xx,vb",
		.of_match_table = vb_of_match,
	},
};

