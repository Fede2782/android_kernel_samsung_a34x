// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "mbraink_audio.h"
#include <mbraink_modules_ops_def.h>

static struct mbraink_audio_ops _mbraink_audio_ops;

int mbraink_audio_init(void)
{
	_mbraink_audio_ops.setUdmFeatureEn = NULL;
	_mbraink_audio_ops.getIdleRatioInfo = NULL;
	return 0;
}

int mbraink_audio_deinit(void)
{
	_mbraink_audio_ops.setUdmFeatureEn = NULL;
	_mbraink_audio_ops.getIdleRatioInfo = NULL;
	return 0;
}

int register_mbraink_audio_ops(struct mbraink_audio_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_audio_ops.getIdleRatioInfo = ops->getIdleRatioInfo;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_audio_ops);

int unregister_mbraink_audio_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_audio_ops.setUdmFeatureEn = NULL;
	_mbraink_audio_ops.getIdleRatioInfo = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_audio_ops);

int mbraink_audio_setUdmFeatureEn(bool bEnable)
{
	int ret = 0;
	if (_mbraink_audio_ops.setUdmFeatureEn)
		ret = _mbraink_audio_ops.setUdmFeatureEn(bEnable);
	else
		pr_info("%s: Do not support udm info feature.\n", __func__);

	return ret;
}

int mbraink_audio_getIdleRatioInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	int ret = 0;
	if (pmbrainkAudioIdleRatioInfo == NULL) {
		pr_info("%s: Audio Idle Ratio Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_audio_ops.getIdleRatioInfo)
		ret = _mbraink_audio_ops.getIdleRatioInfo(pmbrainkAudioIdleRatioInfo);
	else
		pr_info("%s: Do not support ioctl idle ratio info query.\n", __func__);

	return ret;
}

