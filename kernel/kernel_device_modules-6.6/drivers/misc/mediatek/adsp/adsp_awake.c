// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include "adsp_core.h"
#include "adsp_feature_define.h"
#include "adsp_reserved_mem.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_reg.h"
#include "adsp_ipic.h"
#include "adsp_dbg_dump.h"

#define WAIT_MS                     (1000)
#define AP_AWAKE_LOCK_BIT           (0)
#define AP_AWAKE_UNLOCK_BIT         (1)
#define AP_AWAKE_LOCK_MASK          (0x1 << AP_AWAKE_LOCK_BIT)
#define AP_AWAKE_UNLOCK_MASK        (0x1 << AP_AWAKE_UNLOCK_BIT)

struct adsp_sysevent_ctrl {
	struct adsp_priv *pdata;
	struct mutex lock;
};

static struct adsp_sysevent_ctrl sysevent_ctrls[ADSP_CORE_TOTAL];

static int adsp_send_sys_event(struct adsp_sysevent_ctrl *ctrl,
				u32 event, bool wait)
{
	ktime_t start_time;
	s64     time_ipc_us;

	if (mutex_trylock(&ctrl->lock) == 0) {
		pr_info("%s(), mutex_trylock busy", __func__);
		return ADSP_IPI_BUSY;
	}

	if (adsp_mt_check_swirq(ctrl->pdata->id)) {
		mutex_unlock(&ctrl->lock);
		return ADSP_IPI_BUSY;
	}

	adsp_copy_to_sharedmem(ctrl->pdata, ADSP_SHAREDMEM_WAKELOCK,
				&event, sizeof(event));

	adsp_mt_set_swirq(ctrl->pdata->id);

	if (wait) {
		start_time = ktime_get();
		while (adsp_mt_check_swirq(ctrl->pdata->id)) {
			time_ipc_us = ktime_us_delta(ktime_get(), start_time);
			if (time_ipc_us > 1000) /* 1 ms */
				break;
		}
	}

	mutex_unlock(&ctrl->lock);
	return ADSP_IPI_DONE;
}

int adsp_awake_init(struct adsp_priv *pdata)
{
	struct adsp_sysevent_ctrl *ctrl;

	if (!pdata)
		return -EINVAL;

	ctrl = &sysevent_ctrls[pdata->id];
	ctrl->pdata = pdata;
	mutex_init(&ctrl->lock);

	return 0;
}

int adsp_pre_wake_lock(u32 cid)
{
	unsigned long flags;
	int ret = 0, retry = 1000;
	struct adsp_priv *pdata;

	if (!adsp_is_pre_lock_support())
		return -ENOLCK;

	if (adsp_is_ipic_support())
		cid = ADSP_A_ID;

	pdata = get_adsp_core_by_id(cid);

	spin_lock_irqsave(&pdata->wakelock, flags);
	if (pdata->prelock_cnt == 0) {
		ret = adsp_mt_pre_lock(cid);
		if (ret <= 0)
			pr_warn("%s(%d) fail to set lock\n", __func__, cid);
	}
	pdata->prelock_cnt ++;
	spin_unlock_irqrestore(&pdata->wakelock, flags);

	/* wakeup */
	if (adsp_is_ipic_support()) {
		ret = adsp_ipic_send(NULL, false);
		if (ret) {
			adsp_check_adsppll_freq(ADSPPLLDIV);
			pr_warn("%s(%d) send awake ipic, fail ret: %d\n",
				__func__, cid, ret);
		}
	} else
		adsp_mt_set_swirq(cid);

	/* polling adsp status */
	while (!check_core_active(cid) && --retry) {
		if (!in_interrupt())
			usleep_range(20, 50);
		else
			retry = 1;
	}

	if (retry == 0) {
		pr_warn("%s cannot wakeup adsp, hifi_status: %x, retry: %d\n",
			__func__, check_core_active(cid), retry);
		if (!in_interrupt()) {
			adsp_check_adsppll_freq(ADSPPLLDIV);
			adsp_pow_clk_dump();
		}
		return -ETIME;
	}

	return 0;
}

int adsp_pre_wake_unlock(u32 cid)
{
	unsigned long flags;
	int ret = 0;
	struct adsp_priv *pdata;

	if (!adsp_is_pre_lock_support())
		return -1;

	if (adsp_is_ipic_support())
		cid = ADSP_A_ID;

	pdata = get_adsp_core_by_id(cid);

	spin_lock_irqsave(&pdata->wakelock, flags);
	if (pdata->prelock_cnt > 0)
		pdata->prelock_cnt --;

	if (pdata->prelock_cnt == 0) {
		ret = adsp_mt_pre_unlock(cid);
		if (ret != 0)
			pr_warn("%s(%d) fail to clear lock\n", __func__, cid);
	}

	spin_unlock_irqrestore(&pdata->wakelock, flags);

	ipic_clr_chn();

	return 0;
}

/*
 * acquire adsp lock flag, keep adsp awake
 * @param adsp_id: adsp core id
 * return 0     : get lock success
 *        non-0 : get lock fail
 */
int adsp_awake_lock(u32 cid)
{
	int msg = AP_AWAKE_LOCK_MASK;
	int ret = ADSP_IPI_BUSY;

	if (cid >= get_adsp_core_total())
		return -EINVAL;

	if (!is_adsp_ready(cid) || !adsp_feature_is_active(cid))
		return -ENODEV;

	if (adspsys->desc->version == 1)
		ret = adsp_send_sys_event(&sysevent_ctrls[cid],
					AP_AWAKE_LOCK_MASK, true);
	else {
		ret = adsp_push_message(ADSP_IPI_DVFS_WAKE, &msg, sizeof(u32), WAIT_MS, cid);
		if (ret)
			goto ERROR;

		return ADSP_IPI_DONE;
	}

ERROR:
	if (ret)
		pr_info("%s, lock fail, ret = %d", __func__, ret);

	return ret;
}

/*
 * release adsp awake lock flag
 * @param adsp_id: adsp core id
 * return 0     : release lock success
 *        non-0 : release lock fail
 */
int adsp_awake_unlock(u32 cid)
{
	int msg = AP_AWAKE_UNLOCK_MASK;
	int ret = ADSP_IPI_BUSY;

	if (cid >= get_adsp_core_total())
		return -EINVAL;

	if (!is_adsp_ready(cid) || !adsp_feature_is_active(cid))
		return -ENODEV;
	if (adspsys->desc->version == 1)
		ret = adsp_send_sys_event(&sysevent_ctrls[cid],
					AP_AWAKE_UNLOCK_MASK, true);
	else {
		ret = adsp_push_message(ADSP_IPI_DVFS_WAKE, &msg, sizeof(u32), WAIT_MS, cid);
		if (ret)
			goto ERROR;

		return ADSP_IPI_DONE;
	}

ERROR:
	if (ret)
		pr_info("%s, unlock fail", __func__);

	return ret;
}

