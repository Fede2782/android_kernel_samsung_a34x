/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_XTALK_PRIVATE_STRUCTS_H_
#define _VL53LX_XTALK_PRIVATE_STRUCTS_H_

#include "vl53lx_types.h"
#include "vl53lx_hist_structs.h"


#ifdef __cplusplus
extern "C"
{
#endif

#define VL53LX_D_012  4



struct VL53LX_xtalk_algo_data_t {

	uint32_t VL53LX_p_061[VL53LX_D_012];

	int16_t  VL53LX_p_059;

	int16_t  VL53LX_p_060;

	struct VL53LX_histogram_bin_data_t VL53LX_p_056;

	struct VL53LX_histogram_bin_data_t VL53LX_p_057;

	uint32_t VL53LX_p_058;

	uint32_t VL53LX_p_062[VL53LX_XTALK_HISTO_BINS];
};

#ifdef __cplusplus
}
#endif

#endif

