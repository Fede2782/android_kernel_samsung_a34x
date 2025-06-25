// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <mbraink_modules_ops_def.h>
#include "mbraink_v6991_audio.h"

#include <swpm_module_psp.h>

static int mbraink_v6991_audio_getIdleRatioInfo(
	struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	int ret = 0;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr = NULL;
	struct timespec64 tv = { 0 };

	ddr_sr_pd_times_ptr = kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	if (!ddr_sr_pd_times_ptr) {
		ret = -1;
		goto End;
	}

	sync_latest_data();
	ktime_get_real_ts64(&tv);

	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);

	pmbrainkAudioIdleRatioInfo->timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	pmbrainkAudioIdleRatioInfo->adsp_active_time = 0;
	pmbrainkAudioIdleRatioInfo->adsp_wfi_time = 0;
	pmbrainkAudioIdleRatioInfo->adsp_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->s0_time = ddr_sr_pd_times_ptr->pd_time;
	pmbrainkAudioIdleRatioInfo->s1_time = ddr_sr_pd_times_ptr->sr_time;
	pmbrainkAudioIdleRatioInfo->mcusys_active_time = 0;
	pmbrainkAudioIdleRatioInfo->mcusys_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_active_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_idle_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->audio_hw_time = 0;

End:

	if (ddr_sr_pd_times_ptr != NULL)
		kfree(ddr_sr_pd_times_ptr);

	return ret;
}

static struct mbraink_audio_ops mbraink_v6991_audio_ops = {
	.setUdmFeatureEn = NULL,
	.getIdleRatioInfo = mbraink_v6991_audio_getIdleRatioInfo,
};

int mbraink_v6991_audio_init(void)
{
	int ret = 0;

	ret = register_mbraink_audio_ops(&mbraink_v6991_audio_ops);
	return ret;
}

int mbraink_v6991_audio_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_audio_ops();
	return ret;
}


