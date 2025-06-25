/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __EMI_MPU_H
#define __EMI_MPU_H

#define MPU_INFO_VER		2
#define KP_SET			8

#if (MPU_INFO_VER == 1)
#define EMI_MPU_ALIGN_BITS      16
#define EMI_MPU_REGION_NUM      32
#elif (MPU_INFO_VER == 2)
#define EMI_MPU_ALIGN_BITS      12
#define EMI_MPU_REGION_NUM      48
#define EMI_KP_REGION_NUM       8
#endif

#define KP_REGION_0	0
#define KP_REGION_1	1

/* smc source */
enum smc_source {
	MKP_LK = 0,
	MKP_KERNEL = 1,
};

int emi_kp_set_protection(unsigned int start, unsigned int end, unsigned int region, enum smc_source);
#endif
