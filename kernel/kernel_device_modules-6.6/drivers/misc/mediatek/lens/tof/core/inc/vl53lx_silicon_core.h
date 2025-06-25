/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_SILICON_CORE_H_
#define _VL53LX_SILICON_CORE_H_

#include "vl53lx_platform.h"

#ifdef __cplusplus
extern "C" {
#endif




VL53LX_Error VL53LX_is_firmware_ready_silicon(
	struct VL53LX_Dev_t      *Dev,
	uint8_t        *pready);


#ifdef __cplusplus
}
#endif

#endif

