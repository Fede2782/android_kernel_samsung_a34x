/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

#define _HW_REORDER_SW_WORKAROUND_
//#define CCCI_GEN98_LRO_NEW_FEATURE

//#define ENABLE_CPU_AFFINITY
#define REFINE_BAT_OFFSET_REMOVE
#define PIT_USING_CACHE_MEM

//#define CCCI_LOG_LEVEL  CCCI_LOG_ALL_UART
#define USING_PM_RUNTIME

/* AMMS DRDI bank4 share memory size */
#define BANK4_DRDI_SMEM_SIZE (64*1024)


/* This feature will stores the md ee information to specific file. */

#define MTK_TC10_FEATURE_MD_EE_INFO

/*
 * This feature set the md debug level.
 * After MD EE.
 *  1. LEVEL_LOW: reset md.
 *  2. LEVEL_MID/LEVEL_HIGH: trigger crash.
 */
#define MTK_TC10_FEATURE_SET_DEBUG_LEVEL



/*
 * This feature for titan port
 */
#define MTK_TC10_FEATURE_PORT


/*
 * This feature change tx_power_mode.
 * when MD actively queries the status of tx_power_mode,
 * set tx_power_mode to SWTP_DO_TX_POWER.
 */
#define MTK_TC10_FEATURE_CHANGE_TX_POWER

/*
 * This feature send carkit status to md.
 */
#define MTK_TC10_FEATURE_CARKIT

#endif
