/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_NVM_DEBUG_H_
#define _VL53LX_NVM_DEBUG_H_

#include "vl53lx_ll_def.h"
#include "vl53lx_nvm_structs.h"



#ifdef __cplusplus
extern "C"
{
#endif

#ifdef VL53LX_LOG_ENABLE



void VL53LX_print_nvm_raw_data(
	uint8_t                       *pnvm_raw_data,
	uint32_t                       trace_flags);




void VL53LX_print_decoded_nvm_data(
	struct VL53LX_decoded_nvm_data_t *pdata,
	char                      *pprefix,
	uint32_t                   trace_flags);




void VL53LX_print_decoded_nvm_fmt_range_data(
	struct VL53LX_decoded_nvm_fmt_range_data_t *pdata,
	char                                *pprefix,
	uint32_t                             trace_flags);




void VL53LX_print_decoded_nvm_fmt_info(
	struct VL53LX_decoded_nvm_fmt_info_t *pdata,
	char                          *pprefix,
	uint32_t                       trace_flags);



void VL53LX_print_decoded_nvm_ews_info(
	struct VL53LX_decoded_nvm_ews_info_t *pdata,
	char                          *pprefix,
	uint32_t                       trace_flags);

#endif

#ifdef __cplusplus
}
#endif

#endif

