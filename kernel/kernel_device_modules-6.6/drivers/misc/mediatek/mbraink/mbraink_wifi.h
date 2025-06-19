/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef MBRAINK_WIFI_H
#define MBRAINK_WIFI_H

#include <mbraink_ioctl_struct_def.h>

int mbraink_wifi_init(void);
int mbraink_wifi_deinit(void);
void mbraink_get_wifi_rate_data(int current_idx,
				struct mbraink_wifi2mbr_lls_rate_data *rate_buffer);
void mbraink_get_wifi_radio_data(struct mbraink_wifi2mbr_lls_radio_data *radio_buffer);
void mbraink_get_wifi_ac_data(struct mbraink_wifi2mbr_lls_ac_data *ac_buffer);
void mbraink_get_wifi_lp_data(struct mbraink_wifi2mbr_lp_ratio_data *lp_buffer);
void mbraink_get_wifi_txtimeout_data(int current_idx,
				struct mbraink_wifi2mbr_txtimeout_data *txtimeout_buffer);
#endif

