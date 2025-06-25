// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "vcp_feature_define.h"
#include "vcp_ipi_pin.h"
#include "vcp.h"

/*vcp feature list*/
struct vcp_feature_tb feature_table[NUM_FEATURE_ID] = {
	{
		.feature    = RTOS_FEATURE_ID,
		.core_id    = VCP_CORE_TOTAL,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = VDEC_FEATURE_ID,
		.core_id    = VCP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = VENC_FEATURE_ID,
		.core_id    = VCP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = GCE_FEATURE_ID,
		.core_id    = MMUP_ID,
		.enable	    = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = MMDVFS_MMUP_FEATURE_ID,
		.core_id    = MMUP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = MMDVFS_VCP_FEATURE_ID,
		.core_id    = VCP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = MMQOS_FEATURE_ID,
		.core_id    = VCP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = MMDEBUG_FEATURE_ID,
		.core_id    = MMUP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = HWCCF_FEATURE_ID,
		.core_id    = MMUP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = HWCCF_DEBUG_FEATURE_ID,
		.core_id    = VCP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = IMGSYS_FEATURE_ID,
		.core_id    = MMUP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
	{
		.feature    = VDISP_FEATURE_ID,
		.core_id    = MMUP_ID,
		.enable     = 0,
		.sys_id     = VCPSYS_CORE0,
	},
};
