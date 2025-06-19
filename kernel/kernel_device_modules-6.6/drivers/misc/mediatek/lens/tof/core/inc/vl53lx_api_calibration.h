/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_API_CALIBRATION_H_
#define _VL53LX_API_CALIBRATION_H_

#include "vl53lx_platform.h"

#ifdef __cplusplus
extern "C" {
#endif




VL53LX_Error VL53LX_run_ref_spad_char(struct VL53LX_Dev_t *Dev,
		VL53LX_Error           *pcal_status);




VL53LX_Error VL53LX_run_device_test(
	struct VL53LX_Dev_t                 *Dev,
	VL53LX_DeviceTestMode      device_test_mode);




VL53LX_Error VL53LX_get_and_avg_xtalk_samples(
		struct VL53LX_Dev_t          *Dev,
		uint8_t                       num_of_samples,
		uint8_t                       measurement_mode,
		int16_t                       xtalk_filter_thresh_max_mm,
		int16_t                       xtalk_filter_thresh_min_mm,
		uint16_t                      xtalk_max_valid_rate_kcps,
		uint8_t                       xtalk_result_id,
		uint8_t                       xtalk_histo_id,
		struct VL53LX_xtalk_range_results_t *pxtalk_results,
		struct VL53LX_histogram_bin_data_t  *psum_histo,
		struct VL53LX_histogram_bin_data_t  *pavg_histo);





VL53LX_Error   VL53LX_run_phasecal_average(
	struct VL53LX_Dev_t    *Dev,
	uint8_t                 measurement_mode,
	uint8_t                 phasecal_result__vcsel_start,
	uint16_t                phasecal_num_of_samples,
	struct VL53LX_range_results_t *prange_results,
	uint16_t               *pphasecal_result__reference_phase,
	uint16_t               *pzero_distance_phase);




void VL53LX_hist_xtalk_extract_data_init(
	struct VL53LX_hist_xtalk_extract_data_t   *pxtalk_data);



VL53LX_Error VL53LX_hist_xtalk_extract_update(
	int16_t                             target_distance_mm,
	uint16_t                            target_width_oversize,
	struct VL53LX_histogram_bin_data_t        *phist_bins,
	struct VL53LX_hist_xtalk_extract_data_t   *pxtalk_data);



VL53LX_Error VL53LX_hist_xtalk_extract_fini(
	struct VL53LX_histogram_bin_data_t        *phist_bins,
	struct VL53LX_hist_xtalk_extract_data_t   *pxtalk_data,
	struct VL53LX_xtalk_calibration_results_t *pxtalk_cal,
	struct VL53LX_xtalk_histogram_shape_t     *pxtalk_shape);




VL53LX_Error   VL53LX_run_hist_xtalk_extraction(
	struct VL53LX_Dev_t                  *Dev,
	int16_t                       cal_distance_mm,
	VL53LX_Error                 *pcal_status);


#ifdef __cplusplus
}
#endif

#endif

