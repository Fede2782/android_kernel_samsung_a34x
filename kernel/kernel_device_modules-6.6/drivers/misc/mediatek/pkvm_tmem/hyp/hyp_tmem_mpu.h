/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __HYP_TMEM_MPU_H__
#define __HYP_TMEM_MPU_H__

#include <linux/kernel.h>
#include <linux/types.h>

/**
 * Return code
 *
 * This global return code is used for both REE and TEE.
 * Implementation-Defined 0x00000001 - 0xFFFEFFFF
 * Reserved for Future Use 0xFFFF0011 V 0xFFFFFFFF
 *
 * @see TZ_RESULT
 */
#define TZ_RESULT_SUCCESS 0x00000000  // The operation was successful.
#define TZ_RESULT_ERROR_GENERIC 0xFFFF0000  // Non-specific cause.
#define TZ_RESULT_ERROR_ACCESS_DENIED 0xFFFF0001  // Access privileges are not sufficient.
#define TZ_RESULT_ERROR_CANCEL 0xFFFF0002  // The operation was cancelled.
#define TZ_RESULT_ERROR_ACCESS_CONFLICT 0xFFFF0003  // Concurrent accesses caused conflict.
#define TZ_RESULT_ERROR_EXCESS_DATA 0xFFFF0004  // Too much data for the requested operation was passed.
#define TZ_RESULT_ERROR_BAD_FORMAT 0xFFFF0005  // Input data was of invalid format.
#define TZ_RESULT_ERROR_BAD_PARAMETERS 0xFFFF0006  // Input parameters were invalid.
#define TZ_RESULT_ERROR_BAD_STATE 0xFFFF0007  // Operation is not valid in the current state.
#define TZ_RESULT_ERROR_ITEM_NOT_FOUND 0xFFFF0008  // The requested data item is not found.
#define TZ_RESULT_ERROR_NOT_IMPLEMENTED 0xFFFF0009  // The requested operation should exist but is not yet implemented.
#define TZ_RESULT_ERROR_NOT_SUPPORTED 0xFFFF000A  // The requested operation is valid but is not supported.
#define TZ_RESULT_ERROR_NO_DATA 0xFFFF000B  // Expected data was missing.
#define TZ_RESULT_ERROR_OUT_OF_MEMORY 0xFFFF000C  // System ran out of resources.
#define TZ_RESULT_ERROR_BUSY 0xFFFF000D  // The system is busy working on something else.
#define TZ_RESULT_ERROR_COMMUNICATION 0xFFFF000E  // Communication with a remote party failed.
#define TZ_RESULT_ERROR_SECURITY 0xFFFF000F  // A security fault was detected.
#define TZ_RESULT_ERROR_SHORT_BUFFER 0xFFFF0010  // The supplied buffer is too short for the generated output.
#define TZ_RESULT_ERROR_INVALID_HANDLE 0xFFFF0011  // The handle is invalid.

/* 64-KB align */
#define PLATFORM_MPU_ALIGN_BITS 16
#define PLATFORM_MPU_ALIGN_MASK ((0x1 << PLATFORM_MPU_ALIGN_BITS) - 1)

#define MTK_SIP_HYP_MPU_PERM_SET_AARCH64               0xC2008803

/* It is the same with enum MTEE_MCHUNKS_ID at tmem_ffa.c */
enum MPU_REQ_ORIGIN_ZONE_ID {
	MPU_REQ_ORIGIN_EL2_ZONE_PROT = 0,
	MPU_REQ_ORIGIN_EL2_ZONE_SVP = 8,
	MPU_REQ_ORIGIN_EL2_ZONE_WFD = 9,
	MPU_REQ_ORIGIN_EL2_ZONE_TUI = 12,

	MPU_REQ_ORIGIN_EL2_ZONE_MAX = 13,
};

/* return code */
enum MPU_STATUS {
	MPU_SUCCESS,
	MPU_ERROR_ALIGNMENT,
	MPU_ERROR_INVALID_ZONE,
	MPU_ERROR_INVALID_RANGE,
	MPU_ERROR_PERM_SET_SMC_FAIL,
	MPU_ERROR_PERM_CLR_SMC_FAIL,
	MPU_ERROR_PERM_GET_SMC_FAIL,

	MPU_ERROR_MAX = 0x7FFFFFFF,
};

int enable_region_protection(uint32_t region_id, uint64_t addr, uint64_t size,
				const struct pkvm_module_ops *tmem_ops);
int disable_region_protection(uint32_t region_id,
				const struct pkvm_module_ops *tmem_ops);

#endif
