/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_H__
#define __VCP_H__

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#define VCP_SYNC_TIMEOUT_MS             (1000)
#define VCP_MBOX_TOTAL 5

/* core0 system */
/* definition of slot size for send PINs */
#define PIN_OUT_C_SIZE_SLEEP_0           2
#define PIN_OUT_SIZE_TEST_0              3
#define PIN_OUT_SIZE_LOGGER_CTRL_0       6
#define PIN_OUT_SIZE_VCPCTL_0            2

/* definition of slot size for received PINs */
#define PIN_OUT_R_SIZE_SLEEP_0           1
#define PIN_IN_SIZE_LOGGER_CTRL_0        6
#define PIN_IN_SIZE_VCP_READY_0          1
/* ============================================================ */

/* core1 system */
/* definition of slot size for send PINs */
#define PIN_OUT_C_SIZE_SLEEP_1           2
#define PIN_OUT_SIZE_TEST_1              3
#define PIN_OUT_SIZE_LOGGER_CTRL_1       6
#define PIN_OUT_SIZE_VCPCTL_1            2

/* definition of slot size for received PINs */
#define PIN_OUT_R_SIZE_SLEEP_1           1
#define PIN_IN_SIZE_LOGGER_CTRL_1        6
#define PIN_IN_SIZE_VCP_READY_1          1
/* ============================================================ */

/* user pin */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_VDEC               18
#define PIN_OUT_SIZE_VENC               18
#define PIN_OUT_SIZE_MMDVFS              2
#define PIN_OUT_SIZE_MMQOS               2
#define PIN_OUT_SIZE_MMDEBUG             2
#define PIN_OUT_C_SIZE_HWVOTER           8
#define PIN_OUT_SIZE_VDISP               2

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_VDEC                18
#define PIN_IN_SIZE_VENC                18
#define PIN_IN_SIZE_MMDVFS               2
#define PIN_IN_SIZE_MMQOS                2
#define PIN_IN_SIZE_MMDEBUG              2
#define PIN_OUT_R_SIZE_HWVOTER           8
/* ============================================================ */
enum {
	IPI_OUT_VDEC_1                 =  0,
	IPI_IN_VDEC_1                  =  1,
	IPI_OUT_C_SLEEP_0              =  2,
	IPI_OUT_TEST_0                 =  3,
//	IPI_IN_VCP_ERROR_INFO_0        =  4,
	IPI_IN_VCP_READY_0             =  5,
//	IPI_IN_VCP_RAM_DUMP_0          =  6,
//	IPI_OUT_VCP_CONNSYS            =  7,
//	IPI_IN_VCP_CONNSYS             =  8,
	IPI_OUT_MMDVFS_VCP             =  9,
	IPI_IN_MMDVFS_VCP              = 10,
	IPI_OUT_MMQOS                  = 11,
	IPI_IN_MMQOS                   = 12,
	IPI_OUT_MMDEBUG                = 13,
	IPI_IN_MMDEBUG                 = 14,
	IPI_OUT_C_VCP_HWVOTER_DEBUG    = 15,
	IPI_OUT_VENC_0                 = 16,
	IPI_IN_VENC_0                  = 17,
//	IPI_OUT_VCP_MPOOL_0            = 18,
//	IPI_IN_VCP_MPOOL_0             = 19,
	IPI_OUT_C_SLEEP_1              = 20,
	IPI_OUT_TEST_1                 = 21,
	IPI_OUT_LOGGER_CTRL_0          = 22,
	IPI_OUT_VCPCTL_1               = 23,
//	IPI_IN_VCP_ERROR_INFO_1        = 24,
	IPI_IN_LOGGER_CTRL_0           = 25,
	IPI_IN_VCP_READY_1             = 26,
//	IPI_IN_VCP_RAM_DUMP_1          = 27,
//	IPI_OUT_VCP_MPOOL_1            = 28,
//	IPI_IN_VCP_MPOOL_1             = 29,
	IPI_OUT_LOGGER_CTRL_1          = 30,
	IPI_IN_LOGGER_CTRL_1           = 31,
	IPI_OUT_VCPCTL_0               = 32,
	IPI_OUT_MMDVFS_MMUP            = 33,
	IPI_IN_MMDVFS_MMUP             = 34,
	IPI_OUT_VDISP                  = 35,

	VCP_IPI_COUNT
};

/* vcp notify event */
enum VCP_NOTIFY_EVENT {
	VCP_EVENT_READY = 0,
	VCP_EVENT_STOP,
	VCP_EVENT_SUSPEND,
	VCP_EVENT_RESUME,
};

/* vcp iommus */
enum VCP_IOMMU_DEV {
	VCP_IOMMU_VCP = 0,
	VCP_IOMMU_VDEC = 1,
	VCP_IOMMU_VENC = 2,
	VCP_IOMMU_WORK = 3,
	VCP_IOMMU_UBE_LAT = 4,
	VCP_IOMMU_UBE_CORE = 5,
	VCP_IOMMU_SEC = 6,
	VCP_IOMMU_ACP_VDEC = 7,
	VCP_IOMMU_ACP_VENC = 8,
	VCP_IOMMU_ACP_CODEC = 9,
#if IS_ENABLED(CONFIG_MTK_SENTRY_MODE)
	VCP_IOMMU_SENTRY_MODE = 10,
	VCP_IOMMU_SENTRY_MODE_EXTRA = 11,
#endif
	VCP_IOMMU_DEV_NUM,
};

/* vcp reserve memory ID definition*/
enum vcp_reserve_mem_id_t {
	VDEC_MEM_ID,
	VENC_MEM_ID,
	VCP_A_LOGGER_MEM_ID,
	VDEC_SET_PROP_MEM_ID,
	VENC_SET_PROP_MEM_ID,
	VDEC_VCP_LOG_INFO_ID,
	VENC_VCP_LOG_INFO_ID,
	GCE_MEM_ID,
	MMDVFS_VCP_MEM_ID,
	MMQOS_MEM_ID,
	VCP_SECURE_DUMP_ID,
	MMDVFS_MMUP_MEM_ID,
	NUMS_MEM_ID,
};

/* vcp feature ID list */
enum feature_id {
	RTOS_FEATURE_ID,
	VDEC_FEATURE_ID,
	VENC_FEATURE_ID,
	GCE_FEATURE_ID,
	MMDVFS_MMUP_FEATURE_ID,
	MMDVFS_VCP_FEATURE_ID,
	MMQOS_FEATURE_ID,
	MMDEBUG_FEATURE_ID,
	HWCCF_FEATURE_ID,
	HWCCF_DEBUG_FEATURE_ID,
	IMGSYS_FEATURE_ID,
	VDISP_FEATURE_ID,
	NUM_FEATURE_ID,
};

/* vcp cmd ID definition */
enum vcp_cmd_id {
	VCP_SET_HALT         = 0,
	VCP_GET_GEN          = 1,
	VCP_SET_HALT_MMINFRA = 2,
	VCP_DUMP             = 3,
	VCP_DUMP_MMINFRA     = 4,
};

enum vcp_excep_mode_id {
	VCP_NO_EXCEP         = 0,
	VCP_KE_ENABLE        = 1,
	VCP_EE_ENABLE        = 2,
	VCP_EXCEP_MAX,
};

extern struct mtk_mbox_device vcp_mboxdev;
extern struct mtk_ipi_device vcp_ipidev;
extern struct mtk_mbox_info *vcp_mbox_info;
extern struct mtk_mbox_pin_send *vcp_mbox_pin_send;
extern struct mtk_mbox_pin_recv *vcp_mbox_pin_recv;

#endif
