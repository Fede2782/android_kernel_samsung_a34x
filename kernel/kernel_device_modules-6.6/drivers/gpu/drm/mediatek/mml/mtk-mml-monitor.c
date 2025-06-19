// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"
#include "mtk-mml.h"

#define SWPM_NEEDED_MOUDLE_CNT	5 /* for 1 sys and maximum 4 wrot */
#define VIDO_INT_EN		0x018
#define VIDO_DEBUG		0x0d0
#define SYS_CG_CON0		0x100
#define SYS_CG_CON1		0x110
#define WROT_DEBUG_19		0x00001900
#define INIT_INTERVAL		16000 /* us */
#define INIT_INTERVAL_BIAS	50

/* Monitor thread */
static struct task_struct *monitor_tsk;
static bool mml_monitor_enable;
static u32 monitor_cnt;
static u32 monitor_interval = INIT_INTERVAL;
module_param(monitor_interval, int, 0644);

/* Store status that will update to swpm */
static u32 mml_freq, mml_pq, cg_con0, cg_con1;

/* Function pointer assigned by swpm */
static struct mml_swpm_func swpm_funcs;

struct mml_swpm_func *mml_get_swpm_func(void)
{
	return &swpm_funcs;
}
EXPORT_SYMBOL(mml_get_swpm_func);

/* Called when freq change */
void mml_update_freq_status(u32 freq)
{
	if (!mml_monitor_enable || !mml_get_swpm_func()->set_func)
		return;

	mml_freq = freq;
}
/* Called when pq change */
void mml_update_pq_status(const void *pq)
{
	u32 pq_cfg = 0;
	const struct mml_pq_config *pq_config = pq;

	if (!pq_config)
		return;

	if (!mml_monitor_enable || !mml_get_swpm_func()->set_func)
		return;

	pq_cfg |= pq_config->en << 0;
	pq_cfg |= pq_config->en_sharp << 1;
	pq_cfg |= pq_config->en_ur << 2;
	pq_cfg |= pq_config->en_dc << 3;
	pq_cfg |= pq_config->en_color << 4;
	pq_cfg |= pq_config->en_hdr << 5;
	pq_cfg |= pq_config->en_ccorr << 6;
	pq_cfg |= pq_config->en_dre << 7;
	pq_cfg |= pq_config->en_region_pq << 8;
	pq_cfg |= pq_config->en_fg << 9;
	pq_cfg |= pq_config->en_c3d << 10;

	mml_pq = pq_cfg;
}

/* Called when mml change */
void mml_update_status_to_tppa(const struct mml_frame_info *info)
{
	u32 cg0 = 0;
	u32 cg1 = 0;
	const struct mml_frame_dest *dest0 = &(info->dest[0]);
	const struct mml_frame_dest *dest1 = &(info->dest[1]);

	if (dest0->crop.r.width != dest0->compose.width ||
			dest0->crop.r.height != dest0->compose.height) {
		cg0 |= (1 << 7);  // RSZ0
		cg0 |= (1 << 10); // WROT0
	}
	if (dest1->crop.r.width != dest1->compose.width ||
			dest1->crop.r.height != dest1->compose.height) {
		cg0 |= (1 << 24);  // RSZ2
		cg0 |= (1 << 25);  // WROT2
	}
	if (info->mode == MML_MODE_DIRECT_LINK)
		cg1 |= (1 << 8);  // RROT0
	else if (info->mode == MML_MODE_MML_DECOUPLE)
		cg0 |= (1 << 3);  // RDMA0

	cg_con0 = cg0;
	cg_con1 = cg1;
}

/* Monitor */
static int create_monitor(void *arg)
{

	while (!kthread_should_stop()) {
		/* Update status here */
		/* freq*/
		mml_get_swpm_func()->update_freq(mml_freq);

		/* pq*/
		mml_get_swpm_func()->update_pq(mml_pq);

		/* mml*/
		mml_get_swpm_func()->update_cg(cg_con0, cg_con1);

		/* Sleep 100us then take status again */
		usleep_range(monitor_interval, monitor_interval + INIT_INTERVAL_BIAS);
	}

	return 0;
}

/* Enable/Disable monitor thread by adb command */
static int enable_monitor_thread(const char *val, const struct kernel_param *kp)
{
	int ret;
	u32 enable;

	ret = kstrtoint(val, 0, &enable);
	if (ret) {
		mml_err("[MonitorThread] Fail to set monitor enable/disable");
		return ret;
	}

	if (!mml_get_swpm_func()->set_func) {
		mml_err("[MonitorThread] mml_get_swpm_func not set");
		return -EPERM;
	}

	mml_monitor_enable = enable;
	if (enable) {  /* enable = 1 */
		if (!monitor_tsk) {
			monitor_tsk = kthread_run(create_monitor, NULL, "mml_monitor_for_swpm");
			if (IS_ERR(monitor_tsk)) {
				ret = PTR_ERR(monitor_tsk);
				monitor_tsk = NULL;
				return ret;
			}
			monitor_cnt++;
			mml_log("[MonitorThread] thread: %s[PID = %d] Hi I'm on",
				monitor_tsk->comm, monitor_tsk->pid);
		} else
			mml_err("[MonitorThread] cur thread running: %s[PID = %d] no need a new one",
				monitor_tsk->comm, monitor_tsk->pid);
	} else { /* disable = 0 */
		if (monitor_tsk) {
			mml_log("[MonitorThread] stop thread");
			kthread_stop(monitor_tsk);
			monitor_tsk = NULL;
			monitor_cnt--;
		} else
			mml_err("[MonitorThread] no valid thread to stop");
	}

	return 0;
}

static struct kernel_param_ops monitor_ops = {
	.set = enable_monitor_thread,
};

module_param_cb(mml_monitor_for_swpm, &monitor_ops, NULL, 0644);
