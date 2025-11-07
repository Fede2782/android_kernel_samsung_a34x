// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_H

#include "imgsensor_vendor_specific.h"

#if defined(CONFIG_CAMERA_STY_V11)
#include "./sty_v11/imgsensor_vendor_rom_config_sty_v11.h"
#elif defined(CONFIG_CAMERA_STY_V11U)
#include "./sty_v11u/imgsensor_vendor_rom_config_sty_v11u.h"
#else
//default
#include "imgsensor_vendor_rom_config_default.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
