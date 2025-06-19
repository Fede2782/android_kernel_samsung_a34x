/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_WAIT_H_
#define _VL53LX_WAIT_H_

#include "vl53lx_platform.h"

#ifdef __cplusplus
extern "C" {
#endif




VL53LX_Error VL53LX_wait_for_boot_completion(
	struct VL53LX_Dev_t      *Dev);




VL53LX_Error VL53LX_wait_for_range_completion(
	struct VL53LX_Dev_t   *Dev);




VL53LX_Error VL53LX_wait_for_test_completion(
	struct VL53LX_Dev_t   *Dev);






VL53LX_Error VL53LX_is_boot_complete(
	struct VL53LX_Dev_t      *Dev,
	uint8_t        *pready);



VL53LX_Error VL53LX_is_firmware_ready(
	struct VL53LX_Dev_t      *Dev,
	uint8_t        *pready);




VL53LX_Error VL53LX_is_new_data_ready(
	struct VL53LX_Dev_t      *Dev,
	uint8_t        *pready);






VL53LX_Error VL53LX_poll_for_boot_completion(
	struct VL53LX_Dev_t      *Dev,
	uint32_t        timeout_ms);




VL53LX_Error VL53LX_poll_for_firmware_ready(
	struct VL53LX_Dev_t      *Dev,
	uint32_t        timeout_ms);




VL53LX_Error VL53LX_poll_for_range_completion(
	struct VL53LX_Dev_t   *Dev,
	uint32_t     timeout_ms);



#ifdef __cplusplus
}
#endif

#endif

