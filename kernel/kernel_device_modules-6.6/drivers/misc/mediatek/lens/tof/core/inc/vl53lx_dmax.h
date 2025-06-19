/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_DMAX_H_
#define _VL53LX_DMAX_H_

#include "vl53lx_types.h"
#include "vl53lx_hist_structs.h"
#include "vl53lx_dmax_private_structs.h"
#include "vl53lx_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif




VL53LX_Error VL53LX_f_001(
	uint16_t                              target_reflectance,
	struct VL53LX_dmax_calibration_data_t	     *pcal,
	struct VL53LX_hist_gen3_dmax_config_t	     *pcfg,
	struct VL53LX_histogram_bin_data_t          *pbins,
	struct VL53LX_hist_gen3_dmax_private_data_t *pdata,
	int16_t                              *pambient_dmax_mm);




uint32_t VL53LX_f_002(
	uint32_t     events_threshold,
	uint32_t     ref_signal_events,
	uint32_t	 ref_distance_mm,
	uint32_t     signal_thresh_sigma);


#ifdef __cplusplus
}
#endif

#endif

