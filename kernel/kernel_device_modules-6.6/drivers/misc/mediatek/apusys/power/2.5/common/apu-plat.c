// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/pm_opp.h>

#include "apu_plat.h"
#include "apu_devfreq.h"
#include "apu_log.h"
#include "apu_gov.h"
#include "apu_of.h"
#include "apu_common.h"
#include "apu_rpc.h"

int fake_seg;
module_param(fake_seg, int, 0444);
MODULE_PARM_DESC(fake_seg, "fake segment");

int fake_bin;
module_param(fake_bin, int, 0444);
MODULE_PARM_DESC(fake_bin, "fake binning");

int fake_raise;
module_param(fake_raise, int, 0444);
MODULE_PARM_DESC(fake_raise, "fake raise");

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

/* Below is for debugging use
 * real AGING_MARGIN comes from Makefile
 */
//#define AGING_MARGIN

static const char *efuse_field[EFUSE_CNT_MAX] = {
	"efuse_seg", "efuse_bin", "efuse_raise", "efuse_bin_h", "efuse_bin_m"
};

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
static noinline u32 _get_devinfo(struct apu_dev *ad, const char *name)
{
	int ret = 0, oft = 0, mask = 0XFF;
	struct nvmem_cell *cell;
	unsigned int *buf = NULL;
	size_t len;

	if (!strcmp(name, efuse_field[EFUSE_SEG])) {
		/* segment name match */
		cell = nvmem_cell_get(ad->dev, efuse_field[EFUSE_SEG]);
		if (IS_ERR(cell)) {
			aprobe_warn(ad->dev, "has no \"%s\" efuse cell (%d)",
				   efuse_field[EFUSE_SEG], ret);
			goto out;
		}

		buf = (unsigned int *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR_OR_NULL(buf)) {
			ret = PTR_ERR(buf);
			aprobe_warn(ad->dev, "fail to get %s efuse data (%d)",
				   efuse_field[EFUSE_SEG], ret);
			goto out;
		} else {
			ret = (*buf >> oft) & mask;
			aprobe_info(ad->dev, "get %s efuse cell, *buf = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_SEG], *buf, oft, mask, ret);
		}

		if (fake_seg) {
			ret = (fake_seg >> oft) & mask;
			aprobe_info(ad->dev, "get FAKE %s efuse cell, fake_seg = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_SEG], fake_seg, oft, mask, ret);
		}
	} else if (!strcmp(name, efuse_field[EFUSE_BIN])) {
		/* bin name match */
		cell = nvmem_cell_get(ad->dev, efuse_field[EFUSE_BIN]);
		if (IS_ERR(cell)) {
			aprobe_warn(ad->dev, "has no \"%s\" efuse cell (%d)",
				   efuse_field[EFUSE_BIN], ret);
			goto out;
		}
		buf = (unsigned int *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR_OR_NULL(buf)) {
			ret = PTR_ERR(buf);
			aprobe_warn(ad->dev, "fail to get %s efuse data (%d)",
				   efuse_field[EFUSE_BIN], ret);
			goto out;
		} else {
			ret = of_property_read_u32(ad->dev->of_node, "bin-offset", &oft);
			if (ret < 0)
				goto out;
			ret = of_property_read_u32(ad->dev->of_node, "bin-mask", &mask);
			if (ret < 0)
				goto out;
			ret = (*buf >> oft) & mask;
			aprobe_info(ad->dev, "get %s efuse cell, *buf = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_BIN], *buf, oft, mask, ret);
		}

		if (fake_bin) {
			ret = (fake_bin >> oft) & mask;
			aprobe_info(ad->dev, "get FAKE %s efuse cell, fake_bin = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_BIN], fake_bin, oft, mask, ret);
		}
	} else if (!strcmp(name, efuse_field[EFUSE_RAISE])) {
		/* raise name match */
		cell = nvmem_cell_get(ad->dev, efuse_field[EFUSE_RAISE]);
		if (IS_ERR(cell)) {
			aprobe_warn(ad->dev, "has no \"%s\" efuse cell (%d)",
				  efuse_field[EFUSE_RAISE], ret);
			goto out;
		}
		buf = (unsigned int *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR_OR_NULL(buf)) {
			ret = PTR_ERR(buf);
			aprobe_warn(ad->dev, "fail to get %s efuse data (%d)",
				   efuse_field[EFUSE_RAISE], ret);
			goto out;
		} else {
			ret = of_property_read_u32(ad->dev->of_node, "raise-offset", &oft);
			if (ret < 0)
				goto out;
			ret = of_property_read_u32(ad->dev->of_node, "raise-mask", &mask);
			if (ret < 0)
				goto out;
			ret = (*buf >> oft) & mask;
			aprobe_info(ad->dev, "get %s efuse cell, *buf = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_RAISE], *buf, oft, mask, ret);
		}

		if (fake_raise) {
			ret = (fake_raise >> oft) & mask;
			aprobe_info(ad->dev, "get FAKE %s efuse cell, fake_raise = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_RAISE], fake_raise, oft, mask, ret);
		}
	}else if (!strcmp(name, efuse_field[EFUSE_BIN_H])) {
		/* bin name match */
		cell = nvmem_cell_get(ad->dev, efuse_field[EFUSE_BIN_H]);
		if (IS_ERR(cell)) {
			aprobe_warn(ad->dev, "has no \"%s\" efuse cell (%d)",
				   efuse_field[EFUSE_BIN_H], ret);
			goto out;
		}
		buf = (unsigned int *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR_OR_NULL(buf)) {
			ret = PTR_ERR(buf);
			aprobe_warn(ad->dev, "fail to get %s efuse data (%d)",
				   efuse_field[EFUSE_BIN_H], ret);
			goto out;
		} else {
			ret = of_property_read_u32(ad->dev->of_node, "bin_h-offset", &oft);
			if (ret < 0)
				goto out;
			ret = of_property_read_u32(ad->dev->of_node, "bin_h-mask", &mask);
			if (ret < 0)
				goto out;
			ret = (*buf >> oft) & mask;
			aprobe_info(ad->dev, "get %s efuse cell, *buf = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_BIN_H], *buf, oft, mask, ret);
		}

		if (fake_bin) {
			ret = (fake_bin >> oft) & mask;
			aprobe_info(ad->dev, "get FAKE %s efuse cell, fake_bin = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_BIN_H], fake_bin, oft, mask, ret);
		}
	}else if (!strcmp(name, efuse_field[EFUSE_BIN_M])) {
		/* bin name match */
		cell = nvmem_cell_get(ad->dev, efuse_field[EFUSE_BIN_M]);
		if (IS_ERR(cell)) {
			aprobe_warn(ad->dev, "has no \"%s\" efuse cell (%d)",
				   efuse_field[EFUSE_BIN_M], ret);
			goto out;
		}

		buf = (unsigned int *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR_OR_NULL(buf)) {
			ret = PTR_ERR(buf);
			aprobe_warn(ad->dev, "fail to get %s efuse data (%d)",
				   efuse_field[EFUSE_BIN_M], ret);
			goto out;
		} else {
			ret = of_property_read_u32(ad->dev->of_node, "bin_m-offset", &oft);
			if (ret < 0)
				goto out;
			ret = of_property_read_u32(ad->dev->of_node, "bin_m-mask", &mask);
			if (ret < 0)
				goto out;
			ret = (*buf >> oft) & mask;
			aprobe_info(ad->dev, "get %s efuse cell, *buf = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_BIN_M], *buf, oft, mask, ret);
		}

		if (fake_bin) {
			ret = (fake_bin >> oft) & mask;
			aprobe_info(ad->dev, "get FAKE %s efuse cell, fake_bin = 0x%x, oft = %d, mask = 0x%x (ret = %d)",
				    efuse_field[EFUSE_BIN_M], fake_bin, oft, mask, ret);
		}
	}

out:
	kfree(buf);
	return ret;
}
#else
static u32 _get_devinfo(struct apu_dev *ad, const char *name) { return 0; }
#endif

static int _get_opp_from_v(struct apu_dev *ad, int v)
{
	unsigned long freq = ULONG_MAX, volt = 0;
	struct dev_pm_opp *opp = NULL;
	int ret = 0;

	while(1) {
		opp = dev_pm_opp_find_freq_floor(ad->dev, &freq);
		if (IS_ERR(opp))
			return PTR_ERR(opp);
		volt = dev_pm_opp_get_voltage(opp);
		if (volt == v)
			goto out;
		freq --;
		ret ++;
	}
out:
	return ret;
}

#if defined(AGING_MARGIN)
/**
 * apu_age_opp() - minus aging voltae on opp table
 * @dev: struct device
 *
 * per dts's define, aging opp voltage.
 */
static int _age_opp(struct apu_dev *ad)
{
	struct device_node *opp_np, *np;
	int ori_vt, ret = 0;
	struct dev_pm_opp *opp = NULL;
	u64 rate = 0;
	u32 ag_vt = 0;

	opp_np = of_parse_phandle(ad->dev->of_node, "operating-points-v2", 0);
	if (!opp_np)
		return -ENOENT;

	/* We have opp-table node now, iterate over it and managing binning/aging */
	for_each_available_child_of_node(opp_np, np) {
		ret = of_property_read_u32(np, "aging-volt", &ag_vt);
		if (ret < 0)
			continue;

		ret = of_property_read_u64(np, "opp-hz", &rate);
		if (ret < 0) {
			aprobe_err(ad->dev, "%s: opp-hz not found\n", __func__);
			goto out;
		}

		/* get original voltage and minus aging voltage */
		opp = dev_pm_opp_find_freq_exact(ad->dev, rate, true);
		ori_vt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ori_vt -= ag_vt;
		ret = dev_pm_opp_adjust_voltage(ad->dev, rate,
						(ulong)ori_vt,
						(ulong)ori_vt,
						(ulong)ori_vt);

		if (ret) {
			aprobe_warn(ad->dev, "%s: Failed to set aging voltage, ret %d\n",
				   __func__, ret);
			goto out;
		}
	}
	apu_dump_opp_table(ad, __func__, 1);
out:
	return ret;
}
#else
static int _age_opp(struct apu_dev *ad) { return 0; }
#endif

/**
 * __inter_volt() - return bin/raise value from DTS array
 * @i1: low freq
 * @b1: low volt
 * @i2: high freq
 * @b2: high volt
 * @i : mid freq
 *
 * return corresponding mid voltage of mid freq.
 * EX:
 *    freq(728000) = _inter_volt(275000, 575000, 832000, 800000, 728000);
 *                                low_f   low_v  high_f  high_v   min_f
 */
static int __inter_volt(int i1, int b1, int i2, int b2, int i)
{
	int ret;
	int scaling_ratio = 1000;
	int normalize = 6250; // 0.00625
	int tmp1, tmp2;

	/* input unit is hz, need to change Khz */
	i1 = TOKHZ(i1);
	i2 = TOKHZ(i2);
	i = TOKHZ(i);
	tmp1 = DIV_ROUND_CLOSEST((i - i1) * scaling_ratio, i2 - i1);
	tmp2 = ((b2 - b1) * tmp1) / scaling_ratio + b1;
	ret = DIV_ROUND_UP(tmp2, normalize) * normalize;
	return ret;
}


/**
 * __get_bin_raise_from_dts() - return bin/raise value from DTS array
 * @ad: apu device
 * @name: name of dts array
 * @idx: index of dts array
 *
 * Suppose bin array in dts is <0 0 0 0 775000 762500 750000>;
 * and bin index = 4, this function will return bin[4] = 775000;
 */
static noinline int __get_bin_raise_from_dts(struct apu_dev *ad, char *name, int idx)
{
	int ret = 0;
	u32 count = 0;
	u32 *tmp = NULL;
	struct property *prop = NULL;

	prop = of_find_property(ad->dev->of_node, name, NULL);
	if (!prop)
		return -ENODEV;

	count = prop->length / sizeof(u32);

	/* if idx is out of bound, change it as no bin/raise */
	if (idx > (count - 1))
		idx = 0;

	tmp = kmalloc_array(count, sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = of_property_read_u32_array(ad->dev->of_node, name, tmp, count);
	if (ret) {
		aprobe_warn(ad->dev, "%s: Error parsing %s: %d\n", __func__, name, ret);
		goto free_mem;
	}
	ret = tmp[idx];

free_mem:
	kfree(tmp);
	return ret;
}

/**
 * __get_v_f_from_opp() - return bin/raise voltage/freq
 * @ad: apu device
 * @v: voltage expect write to pm_opp
 * @opp: opp idx
 *
 */
static noinline int _update_v_f_2_opp(struct apu_dev *ad, unsigned long v, int opp)
{
	int ret = 0;
	struct dev_pm_opp *pm_opp = NULL;
	unsigned long freq = 0;

	freq = apu_opp2freq_n_df(ad, opp) + 1;
	pm_opp = dev_pm_opp_find_freq_floor(ad->dev, &freq);
	if (IS_ERR(pm_opp))
		return PTR_ERR(pm_opp);

	freq = dev_pm_opp_get_freq(pm_opp);
	ret = dev_pm_opp_adjust_voltage(ad->dev,
					freq,
					(ulong)(v),
					(ulong)(v),
					(ulong)(v));
	dev_pm_opp_put(pm_opp);
	return ret;
}

/**
 * __get_bin_raise_v_f() - return bin/raise voltage/freq
 * @ad: apu device
 * @bin_v: final bin voltage
 * @bin_f: final bin freq
 * @raise_v: final raise voltage
 * @raise_f: final raise freq
 *
 */
static noinline int __get_bin_raise_v_f(struct apu_dev *ad, int *bin_v, unsigned long *bin_f,
							int *raise_v, unsigned long *raise_f)
{
	int ret = 0;
	struct dev_pm_opp *opp = NULL;

	/* bin_f = UL_MAX, and find opp0 to change bin voltage */
	opp = dev_pm_opp_find_freq_floor(ad->dev, bin_f);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	if (*bin_v != 0xDEADBEEF)
		ret = dev_pm_opp_adjust_voltage(ad->dev,
						*bin_f,
						(ulong)(*bin_v),
						(ulong)(*bin_v),
						(ulong)(*bin_v));
	else
		*bin_v = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	if (ret)
		goto out;

	/* raise_f = 0, and find opp10 to change raise voltage */
	opp = dev_pm_opp_find_freq_ceil(ad->dev, raise_f);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	if (*raise_v != 0xDEADBEEF)
		ret = dev_pm_opp_adjust_voltage(ad->dev,
						*raise_f,
						(ulong)(*raise_v),
						(ulong)(*raise_v),
						(ulong)(*raise_v));
	else
		*raise_v = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	if (ret)
		goto out;

out:
	return ret;
}

/**
 * _bin_3p_2l() - mt6877 3p2l AVS
 * @dev: struct device, used for checking child number
 *
 * rm unnecessary opp by different segment.
 */
static noinline int _bin_3p_2l(struct apu_dev *ad)
{

	int idx = 0;
	unsigned long bin_h_v = 0, bin_m_v = 0, raise_v = 0, tmp_v = 0;
	unsigned long bin_h_f = 0, bin_m_f = 0, raise_f = 0, tmp_f = 0;
	const char *vb_mtd_name = NULL;


	if (of_property_read_string(ad->dev->of_node, "vb_mtd", &vb_mtd_name))
		goto out;

	aprobe_info(ad->dev, "%s %d vb_mtd_name = %s\n", __func__, __LINE__, vb_mtd_name);

	if (strncmp(vb_mtd_name, VB_MTD_3P2L, VB_MTD_LEN))
		goto out;

	/* get bin_h voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_BIN_H]);
	if (idx < 0)
		goto out;
	bin_h_v = __get_bin_raise_from_dts(ad, "bin_h", idx);
	ad->bin_h_idx = idx;

	/* get bin_m voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_BIN_M]);
	if (idx < 0)
		goto out;
	bin_m_v = __get_bin_raise_from_dts(ad, "bin_m", idx);
	ad->bin_m_idx = idx;

	/* get raise voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_RAISE]);
	if (idx < 0)
		goto out;
	raise_v = __get_bin_raise_from_dts(ad, "raise", idx);
	if ((bin_h_v == -ENODEV) && (bin_h_v == -ENODEV) && (raise_v == -ENODEV))
		goto out;

	/* LV rising ceiling limited by MV */
	if (raise_v > bin_m_v)
		raise_v = bin_m_v;

	/* update bin_h, bin_m, raise into pm_opp */
	_update_v_f_2_opp(ad, bin_h_v, VVPU_BIN_HIGHV_OPP);
	_update_v_f_2_opp(ad, bin_m_v, VVPU_BIN_MIDV_OPP);
	_update_v_f_2_opp(ad, raise_v, VVPU_BIN_LOWV_OPP);

	bin_h_f = apu_opp2freq_n_df(ad, VVPU_BIN_HIGHV_OPP);
	bin_m_f = apu_opp2freq_n_df(ad, VVPU_BIN_MIDV_OPP);
	raise_f = apu_opp2freq_n_df(ad, VVPU_BIN_LOWV_OPP);

	aprobe_info(ad->dev, "%s: [int3p2l] b_h_v/bin_m_v/r_v/ = %lumV/%lumV/%lumV\n",
			__func__, TOMV(bin_h_v), TOMV(bin_m_v), TOMV(raise_v));
	aprobe_info(ad->dev, "%s: [int3p2l] b_h_f/bin_m_f/r_f/ = %luMhz/%luMhz/%luMhz\n",
			__func__, TOMHZ(bin_h_f), TOMHZ(bin_m_f), TOMHZ(raise_f));

	/* 2L (HV<->MV) */
	for (idx = VVPU_BIN_MIDV_OPP; idx >= VVPU_BIN_HIGHV_OPP; idx--) {
		tmp_f = apu_opp2freq_n_df(ad, idx);
		tmp_v = __inter_volt(bin_m_f, bin_m_v, bin_h_f, bin_h_v, tmp_f);
		_update_v_f_2_opp(ad, tmp_v, idx);
		aprobe_info(ad->dev, "%s: [int3p2l hv<->mv] idx %d v/f = %lumV/%luKhz\n",
				__func__, idx,
				TOMV(apu_opp2volt_n_df(ad, idx)),
				TOKHZ(apu_opp2freq_n_df(ad,idx)));
	}

	/* 2L (MV<->LV) */
	for (idx = VVPU_BIN_LOWV_OPP; idx >= VVPU_BIN_MIDV_OPP; idx--) {
		tmp_f = apu_opp2freq_n_df(ad, idx);
		tmp_v = __inter_volt(raise_f, raise_v, bin_m_f, bin_m_v, tmp_f);
		_update_v_f_2_opp(ad, tmp_v, idx);
		aprobe_info(ad->dev, "%s: [int3p2l mv<->lv] idx %d v/f = %lumV/%luKhz\n",
				__func__, idx,
				TOMV(apu_opp2volt_n_df(ad, idx)),
				TOKHZ(apu_opp2freq_n_df(ad,idx)));
	}

	apu_dump_opp_table(ad, __func__, 1);
out:
	return 0;
}

/**
 * _bin_intpl() - binning and raising opp
 * @dev: struct device, used for checking child numbers
 *
 * rm unnecessary opp by different segment info.
 */
static noinline int _bin_intpl(struct apu_dev *ad)
{
	struct dev_pm_opp *opp;
	int ret = 0, idx = 0, intpl = 1;
	int bin_v = 0, raise_v = 0, tmp_v = 0;
	unsigned long bin_f = 0, raise_f = 0, tmp_f = 0, sign_v = 0;
	const char *vb_mtd_name;

	if (of_property_read_string(ad->dev->of_node, "vb_mtd", &vb_mtd_name))
		goto out;

	aprobe_info(ad->dev, "%s %d vb_mtd_name = %s\n", __func__, __LINE__, vb_mtd_name);

	if (strncmp(vb_mtd_name, VB_MTD_INTPL, VB_MTD_LEN))
		goto out;

	/* get bin voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_BIN]);
	if (idx < 0)
		goto out;
	bin_v = __get_bin_raise_from_dts(ad, "bin", idx);
	ad->bin_idx = idx;

	/* get raise voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_RAISE]);
	if (idx < 0)
		goto out;
	raise_v = __get_bin_raise_from_dts(ad, "raise", idx);
	if ((bin_v == -ENODEV) && (raise_v == -ENODEV)) {
		goto out;
	} else if (bin_v == -ENODEV || bin_v == 0xDEADBEEF) {
		bin_v = 0xDEADBEEF;
		intpl = 0; /* no binning, no interpolate */
	} else if (raise_v == -ENODEV) {
		raise_v = 0xDEADBEEF;
	}

	aprobe_info(ad->dev, "%s: b_v/r_v/intpl = %d(0x%x)/%d(0x%x)/%d\n",
			__func__, bin_v, bin_v, raise_v, raise_v, intpl);
	/* get bin/raise voltage/freq from opp table */
	bin_f = ULONG_MAX;
	raise_f = 0;
	ret = __get_bin_raise_v_f(ad, &bin_v, &bin_f, &raise_v, &raise_f);
	if (ret)
		goto out;

	aprobe_info(ad->dev, "%s: b_v/b_f/r_v/r_f = %d/%lu/%d/%lu\n",
			__func__, bin_v, bin_f, raise_v, raise_f);

	if(!intpl)
		goto out;
	/* calculate other volt/bin except bin/raise points */
	tmp_f = raise_f + 1;
	opp = dev_pm_opp_find_freq_ceil(ad->dev, &tmp_f);
	while (!IS_ERR(opp)) {
		tmp_f = dev_pm_opp_get_freq(opp);
		sign_v = dev_pm_opp_get_voltage(opp);
		tmp_v = __inter_volt(raise_f, raise_v, bin_f, bin_v, tmp_f);
		aprobe_info(ad->dev,
			    "%s: \"%s\",r_f/r_v/b_f/b_v = %lu/%d/%lu/%d\n",
			    __func__, VB_MTD_INTPL,
			   raise_f, raise_v, bin_f, bin_v);
		aprobe_info(ad->dev,
			    "%s: \"%s\",t_f/t_v/s_v = %lu/%d/%lu\n",
			    __func__, VB_MTD_INTPL,
			   tmp_f, tmp_v, sign_v);
		/* change v if inerpolate_v < signed_v */
		if (tmp_v < sign_v) {
			ret = dev_pm_opp_adjust_voltage(ad->dev, tmp_f,
							(ulong)tmp_v,
							(ulong)tmp_v,
							(ulong)tmp_v);
			if (ret)
				goto out;
		}
		tmp_f ++;
		opp = dev_pm_opp_find_freq_ceil(ad->dev, &tmp_f);
	}

	apu_dump_opp_table(ad, __func__, 1);
out:
	return ret;
}


/**
 * _bin_ub_lb() - binning and raising opp in upper/lower bound
 * @dev: struct device, used for checking child numbers
 *
 * rm unnecessary opp by different segment info.
 */
static noinline int _bin_ub_lb(struct apu_dev *ad)
{
	struct dev_pm_opp *opp;
	int ret = 0, idx = 0;
	int bin_v = 0, raise_v = 0;
	unsigned long bin_f = 0, raise_f = 0, tmp_f = 0, sign_v = 0;
	const char *vb_mtd_name;

	if (of_property_read_string(ad->dev->of_node, "vb_mtd", &vb_mtd_name))
		goto out;

	aprobe_info(ad->dev, "%s %d vb_mtd_name = %s\n", __func__, __LINE__, vb_mtd_name);

	if (strncmp(vb_mtd_name, VB_MTD_UB_LB, VB_MTD_LEN))
		goto out;

	/* get bin voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_BIN]);
	if (idx < 0)
		goto out;
	bin_v = __get_bin_raise_from_dts(ad, "bin", idx);
	ad->bin_idx = idx;

	/* get raise voltage */
	idx = _get_devinfo(ad, efuse_field[EFUSE_RAISE]);
	if (idx < 0)
		goto out;
	raise_v = __get_bin_raise_from_dts(ad, "raise", idx);
	if ((bin_v == -ENODEV) && (raise_v == -ENODEV))
		goto out;
	else if (bin_v == -ENODEV)
		bin_v = 0xDEADBEEF;
	else if (raise_v == -ENODEV)
		raise_v = 0xDEADBEEF;

	aprobe_info(ad->dev, "%s: b_v/r_v = %d(0x%x)/%d(0x%x)\n",
			__func__, bin_v, bin_v, raise_v, raise_v);
	/* get bin/raise voltage/freq from opp table */
	bin_f = ULONG_MAX;
	raise_f = 0;
	ret = __get_bin_raise_v_f(ad, &bin_v, &bin_f, &raise_v, &raise_f);
	if (ret)
		goto out;

	aprobe_info(ad->dev, "%s: b_v/b_f/r_v/r_f = %d/%lu/%d/%lu\n",
			__func__, bin_v, bin_f, raise_v, raise_f);

	/* calculate other volt/bin except bin/raise points */
	tmp_f = raise_f + 1;
	opp = dev_pm_opp_find_freq_ceil(ad->dev, &tmp_f);
	while (!IS_ERR(opp)) {
		tmp_f = dev_pm_opp_get_freq(opp);
		sign_v = dev_pm_opp_get_voltage(opp);
		aprobe_info(ad->dev,
				"%s: \"%s\",r_f/r_v/b_f/b_v/s_v/t_f = %lu/%d/%lu/%d/%lu/%lu\n",
				__func__, VB_MTD_UB_LB,
				raise_f, raise_v, bin_f, bin_v, sign_v, tmp_f);

		/* change v if sign_v > bin_v */
		if (sign_v > bin_v)
			ret = dev_pm_opp_adjust_voltage(ad->dev, tmp_f,
								(ulong)bin_v,
								(ulong)bin_v,
								(ulong)bin_v);
		/* change v if sign_v < raise_v */
		if (sign_v < raise_v)
			ret = dev_pm_opp_adjust_voltage(ad->dev, tmp_f,
								(ulong)raise_v,
								(ulong)raise_v,
								(ulong)raise_v);

		if (ret)
			goto out;
		tmp_f ++;
		opp = dev_pm_opp_find_freq_ceil(ad->dev, &tmp_f);
	}

	apu_dump_opp_table(ad, __func__, 1);
out:
	if (ret)
		aprobe_err(ad->dev,
				"%s: \"%s\",r_f/r_v/b_f/b_v/s_v/t_f = %lu/%d/%lu/%d/%lu/%lu\n",
				__func__, VB_MTD_UB_LB,
				raise_f, raise_v, bin_f, bin_v, sign_v, tmp_f);
	return ret;
}

/**
 * _segment_opp() - rm opp by different segment
 * @dev: struct device, used for checking child numbers
 *
 * rm unnecessary opp by different segment info.
 */
static noinline int _segment_opp(struct apu_dev *ad)
{
	struct device_node *opp_np, *np;
	int seg_id = 0, ret = 0;
	u64 rate = 0;
	u32 dis_seg = 0;

	opp_np = of_parse_phandle(ad->dev->of_node, "operating-points-v2", 0);
	if (!opp_np)
		return -ENOENT;

	seg_id = _get_devinfo(ad, efuse_field[EFUSE_SEG]);
	if (seg_id < 0)
		goto out;

	/* We have opp-table node now, iterate over it and managing binning/aging */
	for_each_available_child_of_node(opp_np, np) {
		ret = of_property_read_u32(np, "dis_seg", &dis_seg);
		if (ret < 0) {
			ret = 0;
			continue;
		}
		ret = of_property_read_u64(np, "opp-hz", &rate);
		if (ret < 0) {
			aprobe_err(ad->dev, "%s: opp-hz not found\n", __func__);
			goto out;
		}

		/* get original voltage and minus aging voltage */
		if (seg_id == dis_seg) {
			ret = dev_pm_opp_disable(ad->dev, rate);
			if (ret) {
				aprobe_err(ad->dev, "%s: Failed to disable %lluMhz on seg %d, ret %d\n",
						__func__, TOMHZ(rate), seg_id,  ret);
				goto out;
			} else
				/* recording seg happen */
				ad->seg_idx = seg_id;
				aprobe_info(ad->dev, "%s: disable %lluMhz on seg %d, ret %d\n",
						__func__, TOMHZ(rate), seg_id,	ret);
		}
	}
	apu_dump_opp_table(ad, __func__, 1);
out:
	return ret;
}

static int apu_opp_init(struct apu_dev *ad)
{
	int ret = 0;
	const struct apu_plat_data *apu_data = NULL;

	apu_data = of_device_get_match_data(ad->dev);
	if (!apu_data) {
		aprobe_err(ad->dev, " has no platform data\n");
		ret = -ENODEV;
		goto out;
	}

	ret = dev_pm_opp_of_add_table_indexed(ad->dev, 0);
	if (ret)
		goto out;

	ad->threshold_opp = -EINVAL;
	ad->child_opp_limit = -EINVAL;

	/* has regulator dts and can bin/aging/segment */
	if (ad->user != APUCORE) {
		ret = _segment_opp(ad);
		if (ret == -ENOENT)
			goto out;

		/* if has segment, record threshold_opp/child_opp_limit */
		if (apu_data->threshold_volt)
			ad->threshold_opp = _get_opp_from_v(ad, apu_data->threshold_volt);
		if (apu_data->child_volt_limit)
			ad->child_opp_limit = _get_opp_from_v(ad, apu_data->child_volt_limit);

		ret = _bin_intpl(ad);
		if (ret)
			goto out;

		ret = _bin_3p_2l(ad);
		if (ret)
			goto out;

		ret = _bin_ub_lb(ad);
		if (ret)
			goto out;
	}

	/* only CONN and MDLA need to be aging */
	if (ad->user == APUCONN || ad->user == MDLA) {
		ret = _age_opp(ad);
		if (ret)
			goto out;
	}

	ad->oppt = dev_pm_opp_get_opp_table(ad->dev);
	if (IS_ERR_OR_NULL(ad->oppt)) {
		ret = PTR_ERR(ad->oppt);
		aprobe_err(ad->dev, "[%s] get opp table fail, ret = %d\n", __func__, ret);
		goto out;
	}

out:
	return ret;
}

static void apu_opp_uninit(struct apu_dev *ad)
{
	if (!IS_ERR_OR_NULL(ad->oppt))
		dev_pm_opp_put_opp_table(ad->oppt);
}

/**
 * apu_clk_init - and obtain a reference to a clock producer.
 * @ad: apu_dev
 *
 * CGs are controlled by apusys.
 * vpu_clks {
 *       pll = <&PLL, NPUPLL>;
 *       top-mux = <&mux_clk, VPU0_TOP_SEL>, <&mux_clk, VPU1_TOP_SEL>,
 *                 <&mux_clk, VPU2_TOP_SEL>;
 *		 sys-mux = <"&mux_clk, DEMO_MUX_CORE">;
 *};
 *
 * Take above vpu clks for example, this function will get phandle, vpu_clks,
 * parsing "pll", "top-mux", "sys-mux" and "cgs".
 * Drivers must assume that the clock source is not enabled.
 *
 * of_apu_clk_get should not be called from within interrupt context.
 */
static int apu_clk_init(struct apu_dev *ad)
{
	int ret = 0;
	struct apu_clk_gp *aclk = ad->aclk;
	struct apu_clk *dst;
	ulong rate = 0;

	/* clk related setting is necessary */
	if (IS_ERR_OR_NULL(aclk))
		return -ENODEV;

	mutex_init(&aclk->clk_lock);
	aclk->dev = ad->dev;
	ret = of_apu_clk_get(ad->dev, TOPMUX_NODE, &(aclk->top_mux));
	if (ret)
		goto out;
	ret = of_apu_clk_get(ad->dev, SYSMUX_NODE, &(aclk->sys_mux));
	if (ret)
		goto err_topmux;
	ret = of_apu_clk_get(ad->dev, SYSMUX_PARENT_NODE, &(aclk->sys_mux->parents));
	if (ret)
		goto err_sysmux;
	ret = of_apu_clk_get(ad->dev, APMIX_PLL_NODE, &(aclk->apmix_pll));
	if (ret)
		goto err_sysmux_parrent;
	ret = of_apu_clk_get(ad->dev, TOP_PLL_NODE, &(aclk->top_pll));
	if (ret)
		goto err_apmix_pll;

	ret = of_apu_cg_get(ad->dev, &(aclk->cg));
	if (ret)
		goto err_toppll;

	/* get the slowest frq in opp */
	ret = apu_get_recommend_freq_volt(ad->dev, &rate, NULL, 0);
	if (ret)
		goto err_cg;

	/* if there is no default/shutdown freq, take them as slowest opp */
	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "top_mux def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	dst = aclk->sys_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "sys_mux def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "top_pll def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "apmix_pll def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	return ret;

err_cg:
	of_apu_cg_put(&(aclk->cg));
err_toppll:
	of_apu_clk_put(&(aclk->top_pll));
err_apmix_pll:
	of_apu_clk_put(&(aclk->apmix_pll));
err_sysmux_parrent:
	of_apu_clk_put(&(aclk->sys_mux->parents));
err_sysmux:
	of_apu_clk_put(&(aclk->sys_mux));
err_topmux:
	of_apu_clk_put(&(aclk->top_mux));

out:
	return ret;
}


static void apu_clk_uninit(struct apu_dev *ad)
{
	struct apu_clk_gp *aclk = NULL;
	struct apu_clk *dst = NULL;

	aclk = ad->aclk;

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	if (!IS_ERR_OR_NULL(aclk->sys_mux)) {
		dst = aclk->sys_mux->parents;
		if (!IS_ERR_OR_NULL(dst))
			of_apu_clk_put(&dst);
		dst = aclk->sys_mux;
		of_apu_clk_put(&dst);
	}

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	if (!IS_ERR_OR_NULL(aclk->cg))
		of_apu_cg_put(&(aclk->cg));

}

static int apu_regulator_init(struct apu_dev *ad)
{
	int ret = 0;
	unsigned long volt = 0, def_freq = 0;
	struct apu_regulator *dst = NULL;

	if (IS_ERR_OR_NULL(ad->argul))
		goto out;

	/* initial regulator gp lock */
	mutex_init(&(ad->argul->rgulgp_lock));

	/* initial individual regulator lock */
	mutex_init(&(ad->argul->rgul->reg_lock));

	ad->argul->dev = ad->dev;
	ad->argul->rgul->dev = ad->dev;

	if (ad->argul->rgul_sup) {
		ad->argul->rgul_sup->dev = ad->dev;
		mutex_init(&(ad->argul->rgul_sup->reg_lock));
	}

	/* get the slowest frq in opp and set it as default frequency */
	ret = apu_get_recommend_freq_volt(ad->dev, &def_freq, &volt, 0);
	if (ret)
		goto out;

	/* get regulator */
	dst = ad->argul->rgul;
	ret = of_apu_regulator_get(ad->dev, dst, volt, def_freq);
	if (ret)
		goto out;

	/* get regulator's supply */
	dst = ad->argul->rgul_sup;
	ret = of_apu_regulator_get(ad->dev, dst, volt, def_freq);
	if (ret)
		goto out;
out:
	return ret;
}

static void apu_regulator_uninit(struct apu_dev *ad)
{
	struct apu_regulator *dst = NULL;

	if (!IS_ERR_OR_NULL(ad->argul)) {
		dst = ad->argul->rgul;
		if (!IS_ERR_OR_NULL(dst))
			of_apu_regulator_put(dst);

		dst = ad->argul->rgul_sup;
		if (!IS_ERR_OR_NULL(dst))
			of_apu_regulator_put(dst);
	}
}

static int apu_devfreq_init(struct apu_dev *ad, struct devfreq_dev_profile *pf, void *data)
{
	struct apu_gov_data *pgov_data;
	const char *gov_name = NULL;
	int err = 0;

	pgov_data = apu_gov_init(ad->dev, pf, &gov_name);
	if (IS_ERR(pgov_data)) {
		err = PTR_ERR(pgov_data);
		goto out;
	}

	ad->df = devm_devfreq_add_device(ad->dev, pf, gov_name, pgov_data);
	if (IS_ERR_OR_NULL(ad->df)) {
		err = PTR_ERR(ad->df);
		goto out;
	}

	err = apu_gov_setup(ad, data);

out:
	return err;
}

static void apu_devfreq_uninit(struct apu_dev *ad)
{
	struct apu_gov_data *pgov_data = NULL;

	pgov_data = ad->df->data;
	apu_gov_unsetup(ad);
	/* remove devfreq device */
	devm_devfreq_remove_device(ad->dev, ad->df);
	devm_kfree(ad->dev, pgov_data);
}

static int apu_misc_init(struct apu_dev *ad)
{
	int ret = 0;
	int boost, opp;
	ulong freq = 0, volt = 0;

	if (ad->user == APUCONN) {
		ret = apu_rpc_init_done(ad);
		if (ret)
			goto out;

	}

	/* for mt6877 init ACC, which is located in vcore ao domain */
	if (!IS_ERR_OR_NULL(ad->aclk->ops->acc_init)) {
		ret = ad->aclk->ops->acc_init(ad->aclk);
		if (ret)
			goto out;
	}

	for (;;) {
		if (apupw_dbg_get_loglvl() < VERBOSE_LVL)
			break;
		if (IS_ERR(dev_pm_opp_find_freq_ceil(ad->dev, &freq)))
			break;
		ret = apu_get_recommend_freq_volt(ad->dev, &freq, &volt, 0);
		if (ret)
			break;
		opp = apu_freq2opp(ad, freq);
		boost = apu_opp2boost(ad, opp);
		aprobe_info(ad->dev, "[%s] opp/boost/freq/volt %d/%d/%lu/%lu\n",
			    __func__, opp, boost, freq, volt);

		if (opp != apu_volt2opp(ad, volt))
			aprobe_err(ad->dev, "[%s] apu_volt2opp get %d is wrong\n",
				   __func__, apu_volt2opp(ad, volt));

		if (boost != apu_volt2boost(ad, volt))
			aprobe_err(ad->dev, "[%s] apu_volt2boost get %d is wrong\n",
				   __func__, apu_volt2boost(ad, volt));

		if (boost != apu_freq2boost(ad, freq))
			aprobe_err(ad->dev, "[%s] apu_freq2boost get %d is wrong\n",
				   __func__, apu_freq2boost(ad, freq));

		if (freq != apu_opp2freq(ad, opp))
			aprobe_err(ad->dev, "[%s] apu_opp2freq get %d is wrong\n",
				   __func__, apu_opp2freq(ad, opp));
		freq++;
	}
out:
	return ret;
}

static struct apu_plat_ops apu_plat_driver[] = {
	{
		.name = "mt68xx_platops",
		.init_misc = apu_misc_init,
		.init_opps = apu_opp_init,
		.uninit_opps = apu_opp_uninit,
		.init_clks = apu_clk_init,
		.uninit_clks = apu_clk_uninit,
		.init_rguls = apu_regulator_init,
		.uninit_rguls = apu_regulator_uninit,
		.init_devfreq = apu_devfreq_init,
		.uninit_devfreq = apu_devfreq_uninit,
	},
};

struct apu_plat_ops *apu_plat_get_ops(struct apu_dev *ad, const char *name)
{
	int i = 0;

	if (!name)
		goto out;

	for (i = 0; i < ARRAY_SIZE(apu_plat_driver); i++) {
		if (strcmp(name, apu_plat_driver[i].name) == 0)
			return &apu_plat_driver[i];
	}

	aprobe_err(ad->dev, "[%s] not found platform ops \"%s\"\n", __func__, name);

out:
	return ERR_PTR(-ENOENT);
}

