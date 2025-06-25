/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __APUMMU_BOOT_H__
#define __APUMMU_BOOT_H__
#include <linux/types.h>
#include "apummu_tbl.h"

struct ammu_hw_ops {
	int (*rv_boot)(u32 uP_seg_output, u8 uP_hw_thread,
		u32 logger_seg_output, enum eAPUMMUPAGESIZE logger_page_size,
		u32 XPU_seg_output, enum eAPUMMUPAGESIZE XPU_page_size);
};

struct mtk_apu_ammudata {
	struct ammu_hw_ops ops;
};

extern const struct mtk_apu_ammudata mt6899_ammudata;

#endif /* __APUMMU_BOOT_H__ */
