/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __APUMMU_TBL_H__
#define __APUMMU_TBL_H__
#include <linux/types.h>

enum eAPUMMUPAGESIZE {
	eAPUMMU_PAGE_LEN_128KB = 0,
	eAPUMMU_PAGE_LEN_256KB,
	eAPUMMU_PAGE_LEN_512KB,
	eAPUMMU_PAGE_LEN_1MB,
	eAPUMMU_PAGE_LEN_128MB,
	eAPUMMU_PAGE_LEN_256MB,
	eAPUMMU_PAGE_LEN_512MB,
	eAPUMMU_PAGE_LEN_4GB,
};

int rv_boot(u32 uP_seg_output, u8 uP_hw_thread,
	u32 logger_seg_output, enum eAPUMMUPAGESIZE logger_page_size,
	u32 XPU_seg_output, enum eAPUMMUPAGESIZE XPU_page_size);

#endif /* __APUMMU_TBL_H__ */
