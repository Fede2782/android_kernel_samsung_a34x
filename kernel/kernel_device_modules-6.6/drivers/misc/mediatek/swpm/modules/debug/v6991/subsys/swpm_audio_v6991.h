/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_AUDIO_V6991_H__
#define __SWPM_AUDIO_V6991_H__

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>

#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>

#include <sound/pcm_params.h>
#include <sound/soc.h>

/*****************************************************************************
 *  Type Definiations
 *****************************************************************************/
struct audio_swpm_index {
	unsigned int afe_on;
	unsigned int user_case;
	unsigned int output_device;
	unsigned int input_device;
	unsigned int adda_mode;
	unsigned int sample_rate;
	unsigned int channel_num;
	unsigned int freq_clock;
	unsigned int D0_ratio;
};

struct audio_swpm_data {
	unsigned int afe_on;
	unsigned int user_case;
	unsigned int output_device;
	unsigned int input_device;
	unsigned int adda_mode;
	unsigned int sample_rate;
	unsigned int channel_num;
	unsigned int freq_clock;
	unsigned int D0_ratio;
};

extern int swpm_audio_v6991_init(void);
extern void swpm_audio_v6991_exit(void);
extern void *audio_get_power_scenario(void);

extern struct audio_swpm_data audio_data;

#endif
