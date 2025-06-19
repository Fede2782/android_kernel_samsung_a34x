/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6991_AUDIO_H
#define MBRAINK_V6991_AUDIO_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mbraink_ioctl_struct_def.h>

int mbraink_v6991_audio_init(void);
int mbraink_v6991_audio_deinit(void);

#endif /*end of MBRAINK_V6991_AUDIO_H*/

