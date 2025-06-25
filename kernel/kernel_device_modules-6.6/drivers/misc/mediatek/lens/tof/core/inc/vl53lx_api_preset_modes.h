/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_API_PRESET_MODES_H_
#define _VL53LX_API_PRESET_MODES_H_

#include "vl53lx_ll_def.h"
#include "vl53lx_dmax_structs.h"

#ifdef __cplusplus
extern "C" {
#endif




VL53LX_Error VL53LX_init_refspadchar_config_struct(
	struct VL53LX_refspadchar_config_t     *pdata);




VL53LX_Error VL53LX_init_ssc_config_struct(
	struct VL53LX_ssc_config_t     *pdata);




VL53LX_Error VL53LX_init_xtalk_config_struct(
		struct VL53LX_customer_nvm_managed_t *pnvm,
		struct VL53LX_xtalk_config_t   *pdata);



VL53LX_Error VL53LX_init_xtalk_extract_config_struct(
		struct VL53LX_xtalkextract_config_t   *pdata);



VL53LX_Error VL53LX_init_offset_cal_config_struct(
	struct VL53LX_offsetcal_config_t   *pdata);



VL53LX_Error VL53LX_init_zone_cal_config_struct(
	struct VL53LX_zonecal_config_t   *pdata);



VL53LX_Error VL53LX_init_hist_post_process_config_struct(
	uint8_t                              xtalk_compensation_enable,
	struct VL53LX_hist_post_process_config_t   *pdata);




VL53LX_Error VL53LX_init_dmax_calibration_data_struct(
	struct VL53LX_dmax_calibration_data_t   *pdata);




VL53LX_Error VL53LX_init_tuning_parm_storage_struct(
	struct VL53LX_tuning_parm_storage_t   *pdata);



VL53LX_Error VL53LX_init_hist_gen3_dmax_config_struct(
	struct VL53LX_hist_gen3_dmax_config_t   *pdata);




VL53LX_Error VL53LX_preset_mode_standard_ranging(
	struct VL53LX_static_config_t     *pstatic,
	struct VL53LX_histogram_config_t  *phistogram,
	struct VL53LX_general_config_t    *pgeneral,
	struct VL53LX_timing_config_t     *ptiming,
	struct VL53LX_dynamic_config_t    *pdynamic,
	struct VL53LX_system_control_t    *psystem,
	struct VL53LX_tuning_parm_storage_t *ptuning_parms,
	struct VL53LX_zone_config_t       *pzone_cfg);




VL53LX_Error VL53LX_preset_mode_histogram_ranging(
	struct VL53LX_hist_post_process_config_t *phistpostprocess,
	struct VL53LX_static_config_t            *pstatic,
	struct VL53LX_histogram_config_t         *phistogram,
	struct VL53LX_general_config_t           *pgeneral,
	struct VL53LX_timing_config_t            *ptiming,
	struct VL53LX_dynamic_config_t           *pdynamic,
	struct VL53LX_system_control_t           *psystem,
	struct VL53LX_tuning_parm_storage_t      *ptuning_parms,
	struct VL53LX_zone_config_t              *pzone_cfg);




VL53LX_Error VL53LX_preset_mode_histogram_long_range(
	struct VL53LX_hist_post_process_config_t *phistpostprocess,
	struct VL53LX_static_config_t            *pstatic,
	struct VL53LX_histogram_config_t         *phistogram,
	struct VL53LX_general_config_t           *pgeneral,
	struct VL53LX_timing_config_t            *ptiming,
	struct VL53LX_dynamic_config_t           *pdynamic,
	struct VL53LX_system_control_t           *psystem,
	struct VL53LX_tuning_parm_storage_t      *ptuning_parms,
	struct VL53LX_zone_config_t              *pzone_cfg);




VL53LX_Error VL53LX_preset_mode_histogram_medium_range(
	struct VL53LX_hist_post_process_config_t *phistpostprocess,
	struct VL53LX_static_config_t            *pstatic,
	struct VL53LX_histogram_config_t         *phistogram,
	struct VL53LX_general_config_t           *pgeneral,
	struct VL53LX_timing_config_t            *ptiming,
	struct VL53LX_dynamic_config_t           *pdynamic,
	struct VL53LX_system_control_t           *psystem,
	struct VL53LX_tuning_parm_storage_t      *ptuning_parms,
	struct VL53LX_zone_config_t              *pzone_cfg);




VL53LX_Error VL53LX_preset_mode_histogram_short_range(
	struct VL53LX_hist_post_process_config_t *phistpostprocess,
	struct VL53LX_static_config_t            *pstatic,
	struct VL53LX_histogram_config_t         *phistogram,
	struct VL53LX_general_config_t           *pgeneral,
	struct VL53LX_timing_config_t            *ptiming,
	struct VL53LX_dynamic_config_t           *pdynamic,
	struct VL53LX_system_control_t           *psystem,
	struct VL53LX_tuning_parm_storage_t      *ptuning_parms,
	struct VL53LX_zone_config_t              *pzone_cfg);




void VL53LX_copy_hist_cfg_to_static_cfg(
	struct VL53LX_histogram_config_t  *phistogram,
	struct VL53LX_static_config_t     *pstatic,
	struct VL53LX_general_config_t    *pgeneral,
	struct VL53LX_timing_config_t     *ptiming,
	struct VL53LX_dynamic_config_t    *pdynamic);



void VL53LX_copy_hist_bins_to_static_cfg(
	struct VL53LX_histogram_config_t *phistogram,
	struct VL53LX_static_config_t    *pstatic,
	struct VL53LX_timing_config_t    *ptiming);

#ifdef __cplusplus
}
#endif

#endif

