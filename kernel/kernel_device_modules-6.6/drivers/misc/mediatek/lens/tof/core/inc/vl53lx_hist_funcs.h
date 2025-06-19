/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_HIST_FUNCS_H_
#define _VL53LX_HIST_FUNCS_H_

#include "vl53lx_types.h"
#include "vl53lx_ll_def.h"

#ifdef __cplusplus
extern "C"
{
#endif




VL53LX_Error VL53LX_hist_process_data(
	struct VL53LX_dmax_calibration_data_t    *pdmax_cal,
	struct VL53LX_hist_gen3_dmax_config_t    *pdmax_cfg,
	struct VL53LX_hist_post_process_config_t *ppost_cfg,
	struct VL53LX_histogram_bin_data_t       *pbins,
	struct VL53LX_xtalk_histogram_data_t     *pxtalk,
	uint8_t                           *pArea1,
	uint8_t                           *pArea2,
	struct VL53LX_range_results_t            *presults,
	uint8_t                           *HistMergeNumber);




VL53LX_Error VL53LX_hist_ambient_dmax(
	uint16_t                            target_reflectance,
	struct VL53LX_dmax_calibration_data_t     *pdmax_cal,
	struct VL53LX_hist_gen3_dmax_config_t     *pdmax_cfg,
	struct VL53LX_histogram_bin_data_t        *pbins,
	int16_t                            *pambient_dmax_mm);


#ifdef __cplusplus
}
#endif

#endif

