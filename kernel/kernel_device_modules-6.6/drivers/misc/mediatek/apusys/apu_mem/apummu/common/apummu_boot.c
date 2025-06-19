// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "apummu_boot.h"

static struct ammu_hw_ops *hw_ops = (void *)&mt6899_ammudata.ops;


int rv_boot(u32 uP_seg_output, u8 uP_hw_thread,
	u32 logger_seg_output, enum eAPUMMUPAGESIZE logger_page_size,
	u32 XPU_seg_output, enum eAPUMMUPAGESIZE XPU_page_size)
{
	return hw_ops->rv_boot(uP_seg_output, uP_hw_thread, logger_seg_output,
		logger_page_size, XPU_seg_output, XPU_page_size);
}

