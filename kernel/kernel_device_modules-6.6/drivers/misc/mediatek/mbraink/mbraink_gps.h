/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef MBRAINK_GPS_H
#define MBRAINK_GPS_H

#include <mbraink_ioctl_struct_def.h>

int mbraink_gps_init(void);
int mbraink_gps_deinit(void);
void mbraink_get_gnss_lp_data(struct mbraink_gnss2mbr_lp_data *gnss_lp_buffer);
void mbraink_get_gnss_mcu_data(struct mbraink_gnss2mbr_mcu_data *gnss_mcu_buffer);
#endif

