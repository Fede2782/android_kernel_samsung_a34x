// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Samsung Electronics Inc.
 */

#include "imgsensor_vendor_rom_config_sty_v11.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,   OV13A10_SENSOR_ID,   &rear_ov13a10_cal_addr},
	{SENSOR_POSITION_FRONT,  OV13A10F_SENSOR_ID,  &front_ov13a10f_cal_addr},
};
