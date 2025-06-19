/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_AUDIO_H
#define MBRAINK_AUDIO_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mbraink_ioctl_struct_def.h>

int mbraink_audio_init(void);
int mbraink_audio_deinit(void);
int mbraink_audio_setUdmFeatureEn(bool bEnable);
int mbraink_audio_getIdleRatioInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo);

#endif /*end of MBRAINK_AUDIO_H*/

