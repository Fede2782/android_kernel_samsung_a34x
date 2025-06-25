// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>

#include "apusys_power_user.h"
#include "apu_devfreq.h"
#include "apu_common.h"
#include "apu_clk.h"
#include "apu_log.h"

static DEFINE_SPINLOCK(adev_list_lock);

/* The list of all apu devices */
static LIST_HEAD(adev_list);

static bool _valid_ad(struct apu_dev *ad)
{
	if (IS_ERR(ad) || !ad->df ||
	    !ad->df->profile || !ad->df->profile->max_state) {
		if (IS_ERR(ad)) {
			apower_warn(ad->dev, "IS_ERR(ad)\n");
			goto _dump;
		}

		if (!ad->df) {
			apower_warn(ad->dev, "!ad->df\n");
			goto _dump;
		}

		if (!ad->df->profile) {
			apower_warn(ad->dev, "!ad->df->profile\n");
			goto _dump;
		}

		if (!ad->df->profile->max_state)
			apower_warn(ad->dev, "!ad->df->profile->max_state\n");

_dump:
		apower_warn(ad->dev, "%ps: Invalid input.\n",
			   __builtin_return_address(0));

		return false;
	}
	return true;
}

/**
 * apu_dev2_domain() - return volt domain by user
 * @user:	apu dvfs user.
 *
 * only use for mt6877
 */
const int apu_dev2_domain(enum DVFS_USER user)
{
	enum DVFS_VOLTAGE_DOMAIN {
		V_VPU0 = 0,
		V_VPU1,
		V_MDLA0,
		V_APU_CONN,
		V_TOP_IOMMU,
		V_VCORE,
		APUSYS_BUCK_DOMAIN_NUM,
	};

	static const int domain[] = {
		[VPU] = V_VPU0,
		[VPU0] = V_VPU0,
		[VPU1] = V_VPU1,
		[MDLA] = V_MDLA0,
		[MDLA0] = V_MDLA0,
		[APUIOMMU] = V_TOP_IOMMU,
		[APUCONN] = V_APU_CONN,
	};

	if ((user != VPU0) && (user != VPU1) && (user != VPU) &&
		 (user != MDLA0) && (user != MDLA) &&
		 (user != APUIOMMU) && (user != APUCONN))
		return -ENODEV;

	return domain[user];
}


/**
 * apu_dev_string() - return string of dvfs user
 * @user:	apu dvfs user.
 *
 * Return the name string of dvfs user
 */
const char *apu_dev_string(enum DVFS_USER user)
{
	static const char *const names[] = {
		[VPU] = "APUVPU",
		[VPU0] = "APUVPU0",
		[VPU1] = "APUVPU1",
		[VPU2] = "APUVPU2",
		[MDLA] = "APUMDLA",
		[MDLA0] = "APUMDLA0",
		[MDLA1] = "APUMDLA1",
		[APUIOMMU] = "APUIOMMU",
		[APUCONN] = "APUCONN",
		[APUCORE] = "APUCORE",
		[APUCB] = "APUCB",
		[EDMA] = "APUEDMA",
		[EDMA2] = "APUEDMA2",
		[REVISER] = "APUREVISER",
		[APUMNOC] = "APUMNOC",
		[APUVB]   = "APUVB",
	};

	if (user >= ARRAY_SIZE(names))
		return NULL;

	return names[user];
}

/**
 * apu_dump_opp_table() - dump opp table
 * @ad:	apu device
 * @fun_name: char print in log
 * @dir: 1:high --> low, 0: low --> high
 * Dump opp table belong to this apu_dev.
 */
void apu_dump_opp_table(struct apu_dev *ad, const char *fun_name, int dir)
{
	unsigned long freq = 0, volt = 0;

	if (dir)
		freq = ULONG_MAX;
	while(1) {
		if (dir) {
			if (IS_ERR(dev_pm_opp_find_freq_floor(ad->dev, &freq)))
				break;
		} else {
			if (IS_ERR(dev_pm_opp_find_freq_ceil(ad->dev, &freq)))
				break;
		}
		apu_get_recommend_freq_volt(ad->dev, &freq, &volt, dir);
		dev_info(ad->dev, " %s freq/volt %lu/%lu\n", fun_name, freq, volt);
		if (dir)
			freq--;
		else
			freq++;
	}
}

/**
 * apu_find_device() - find apu_dev struct using enum dvfs_user
 * @user:	apu dvfs user.
 *
 * Search the list of device adev_list and return the matched device's
 * apu_dev info.
 */
struct apu_dev *apu_find_device(enum DVFS_USER user)
{
	struct apu_dev *ret_dev = NULL;

	if (user > APUSYS_POWER_USER_NUM) {
		pr_info("%s: user %d Invalid parameters\n",
			__func__, user);
		return ERR_PTR(-EINVAL);
	}

	spin_lock(&adev_list_lock);
	list_for_each_entry(ret_dev, &adev_list, node) {
		if (ret_dev->user == user) {
			spin_unlock(&adev_list_lock);
			return ret_dev; /* Got it */
		}
	}
	spin_unlock(&adev_list_lock);
	return ERR_PTR(-ENODEV);
}

/**
 * apu_dev_user() - return dvfs user
 * @name: the name of apu_device
 *
 * Return the dvfs user of given name
 */
enum DVFS_USER apu_dev_user(const char *name)
{
	int idx = 0;
	static const char *const names[] = {
		[VPU] = "APUVPU",
		[VPU0] = "APUVPU0",
		[VPU1] = "APUVPU1",
		[VPU2] = "APUVPU2",
		[MDLA] = "APUMDLA",
		[MDLA0] = "APUMDLA0",
		[MDLA1] = "APUMDLA1",
		[APUCB] = "APUCB",
		[APUCONN] = "APUCONN",
		[APUCORE] = "APUCORE",
	};

	for (idx = 0; idx < ARRAY_SIZE(names); idx++)
		if (names[idx] && !strcmp(name, names[idx]))
			return idx;

	return APUSYS_POWER_USER_NUM;
}

/**
 * apu_add_device() - Add apu_dev
 * @add_dev: the apu_dev for adding.
 * @user:	 the dvfs user
 *
 * link dvfs user's dev with adev_list
 */
int apu_add_devfreq(struct apu_dev *ad)
{

	if (!ad)
		return -EINVAL;

	if (ad->user > APUSYS_POWER_USER_NUM) {
		if (ad->dev)
			apower_err(ad->dev, "%s: Invalid parameters.\n", __func__);
		return -EINVAL;
	}

	if (!IS_ERR(apu_find_device(ad->user))) {
		apower_warn(ad->dev, "%s: device %s already exist.\n",
			   __func__, apu_dev_string(ad->user));
		goto out;
	}

	spin_lock(&adev_list_lock);
	list_add(&ad->node, &adev_list);
	spin_unlock(&adev_list_lock);
out:
	return 0;
}


/**
 * apu_del_device() - Add apu_dev
 * @add_dev: the apu_dev for deleting.
 *
 * link dvfs user's dev with adev_list
 */
int apu_del_devfreq(struct apu_dev *del_dev)
{

	if (IS_ERR(del_dev)) {
		pr_info("%s: Invalid parameters.\n", __func__);
		return -EINVAL;
	}

	spin_lock(&adev_list_lock);
	list_del(&del_dev->node);
	spin_unlock(&adev_list_lock);

	return 0;
}

/**
 * apu_boost2opp() - get opp from boost
 * @ad: apu_dev
 * @boost: boost value (0 ~ 100)
 *
 */
int apu_boost2opp(struct apu_dev *ad, int boost)
{
	u32 max_st = 0, opp = 0;
	u64 max_freq;

	if (!_valid_ad(ad))
		return -EINVAL;

	/* minus 1 for opp inex starts from 0 */
	max_st = ad->df->profile->max_state - 1;
	if (boost >= 100) {
		opp = 0;
	} else {
		max_freq = ad->df->profile->freq_table[max_st];
		max_freq = boost * (max_freq / 100);
		for (opp = 0; opp <= max_st; opp++)
			if (max_freq > ad->df->profile->freq_table[max_st - opp])
				break;
	}
	apower_info(ad->dev, "[%s] boost %d --> opp %d\n",
		    __func__, boost, opp);

	return opp;
}

/**
 * apu_boost2freq() - get freq from boost
 * @ad: apu_dev
 * @boost: boost value (0 ~ 100)
 *
 * frq = freq_table[opp]
 */
int apu_boost2freq(struct apu_dev *ad, int boost)
{
	int opp = 0;
	int max_st = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	opp = apu_boost2opp(ad, boost);
	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest freq. So we need to swap them here.
	 */
	max_st = ad->df->profile->max_state - 1;
	return ad->df->profile->freq_table[max_st - opp];
}


/**
 * apu_opp2freq() - get freq from opp
 * @ad: apu_dev
 * @opp: opp value (0 means the fastest)
 *
 */
int apu_opp2freq(struct apu_dev *ad, int opp)
{
	int max_st = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */
	return ad->df->profile->freq_table[max_st - opp];
}

/**
 * apu_opp2volt() - get volt from opp
 * @ad: apu_dev
 * @opp: opp value (0 means the fastest)
 *
 */
unsigned long apu_opp2volt(struct apu_dev *ad, int opp)
{
	int max_st = 0;
	unsigned long freq = 0, volt = 0;
	struct dev_pm_opp *pm_opp = NULL;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */
	freq = ad->df->profile->freq_table[max_st - opp] + 1;
	pm_opp = dev_pm_opp_find_freq_floor(ad->dev, &freq);
	if (IS_ERR(pm_opp))
		return PTR_ERR(pm_opp);

	volt = dev_pm_opp_get_voltage(pm_opp);
	dev_pm_opp_put(pm_opp);
	return volt;
}

/**
 * apu_opp2freq() - get freq from opp
 * @ad: apu_dev
 * @opp: opp value (0 means the fastest)
 *
 */
unsigned long apu_opp2freq_n_df(struct apu_dev *ad, int opp)
{
	int max_st = 0, ret = 0;
	unsigned long freq = ULONG_MAX;
	struct dev_pm_opp *pm_opp = NULL;

	if (IS_ERR(ad) || IS_ERR_OR_NULL(ad->dev))
		return -EINVAL;

	max_st = dev_pm_opp_get_opp_count(ad->dev) - 1;

	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	while(1) {
		pm_opp = dev_pm_opp_find_freq_floor(ad->dev, &freq);
		if (IS_ERR(pm_opp))
			break;
		dev_pm_opp_put(pm_opp);
		if (ret == opp)
			break;
		freq --;
		ret ++;
	}

	return freq;
}

/**
 * apu_opp2volt() - get volt from opp
 * @ad: apu_dev
 * @opp: opp value (0 means the fastest)
 *
 */
unsigned long apu_opp2volt_n_df(struct apu_dev *ad, int opp)
{
	int max_st = 0, ret = 0;
	unsigned long freq = ULONG_MAX, ret_v = 0;
	struct dev_pm_opp *pm_opp = NULL;

	if (IS_ERR(ad) || IS_ERR_OR_NULL(ad->dev))
		return -EINVAL;

	max_st = dev_pm_opp_get_opp_count(ad->dev) - 1;
	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	while(1) {
		pm_opp = dev_pm_opp_find_freq_floor(ad->dev, &freq);
		if (IS_ERR(pm_opp))
			break;
		ret_v = dev_pm_opp_get_voltage(pm_opp);
		dev_pm_opp_put(pm_opp);
		if (ret == opp)
			break;
		freq --;
		ret ++;
	}

	return ret_v;
}

/**
 * apu_opp2boost() - get opp from boost
 * @ad: apu_dev
 * @opp: opp value
 *
 *  opp = abs(max_state - boost/opp_div)
 *  boost = (max_state - opp + 1) * opp_div
 */
int apu_opp2boost(struct apu_dev *ad, int opp)
{
	int max_st = 0;
	unsigned long freq = 0, max_freq = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	max_freq = ad->df->profile->freq_table[max_st];
	freq = ad->df->profile->freq_table[max_st - opp];

	return (freq * 100) / max_freq;
}

/**
 * apu_freq2opp() - get freq from opp
 * @ad: apu_dev
 * @freq: frequency
 *
 */
int apu_freq2opp(struct apu_dev *ad, unsigned long freq)
{
	int max_st = 0;
	int opp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (freq > ad->df->profile->freq_table[max_st])
		return -EINVAL;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */

	for (opp = 0; max_st >= 0; max_st--, opp++) {
		if (round_khz(ad->df->profile->freq_table[max_st], freq))
			break;
	}
	return opp;
}

/**
 * apu_freq2boost() - get freq from opp
 * @ad: apu_dev
 * @freq: frequency
 *
 */
int apu_freq2boost(struct apu_dev *ad, unsigned long freq)
{
	int max_st = 0;
	int opp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (freq > ad->df->profile->freq_table[max_st])
		return -EINVAL;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */

	for (opp = 0; max_st >= 0; max_st--, opp++) {
		if (round_khz(ad->df->profile->freq_table[max_st], freq))
			break;
	}
	return apu_opp2boost(ad, opp);
}

/**
 * apu_volt2opp() - get freq from opp
 * @ad: apu_dev
 * @volt: volt
 *
 */
int apu_volt2opp(struct apu_dev *ad, int volt)
{
	int max_st = 0, ret = -ERANGE;
	ulong freq = 0, tmp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	/* search from slowest rate/volt and opp is reverse of max_state*/
	max_st = ad->df->profile->max_state - 1;

	do {
		ret = apu_get_recommend_freq_volt(ad->dev, &freq, &tmp, 0);
		if (ret)
			break;
		if (tmp != volt)
			max_st--;
		else
			goto out;
		freq++;
	} while (!ret && max_st >= 0);

	apower_err(ad->dev, "[%s] fail to find opp for %dmV.\n",
		   __func__, TOMV(volt));
	return -EINVAL;

out:
	return max_st;
}

/**
 * apu_volt2boost() - get freq from opp
 * @ad: apu_dev
 * @volt: volt
 *
 */
int apu_volt2boost(struct apu_dev *ad, int volt)
{
	int max_st = 0, ret = -ERANGE;
	ulong freq = 0, tmp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	/* search from slowest rate/volt and opp is reverse of max_state*/
	max_st = ad->df->profile->max_state - 1;

	do {
		ret = apu_get_recommend_freq_volt(ad->dev, &freq, &tmp, 0);
		if (ret)
			break;
		if (tmp != volt)
			max_st--;
		else
			goto out;
		freq++;
	} while (!ret && max_st >= 0);

	apower_err(ad->dev, "[%s] fail to find boost for %dmV.\n",
		   __func__, TOMV(volt));
	return -EINVAL;
out:
	return apu_opp2boost(ad, max_st);
}

int apu_get_recommend_freq_volt(struct device *dev, unsigned long *freq,
				unsigned long *volt, int flag)
{
	struct dev_pm_opp *opp;

	if (!freq)
		return -EINVAL;

	/* get the slowest frq in opp */
	opp = devfreq_recommended_opp(dev, freq, flag);
	if (IS_ERR(opp)) {
		apower_err(dev, "[%s] no opp for %luMHz, ret %lu\n",
			   __func__, TOMHZ(*freq), PTR_ERR(opp));
		return PTR_ERR(opp);
	}

	if (volt)
		*volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	return 0;
}
