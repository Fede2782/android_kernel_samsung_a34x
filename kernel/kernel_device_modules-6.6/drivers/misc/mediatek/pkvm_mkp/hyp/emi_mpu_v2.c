// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "emi_mpu.h"

#define MTK_SIP_EMIMPU_CONTROL_KERNEL		0xC2008825
#define emi_kp_smc_set_kernel(start, end, region, res) \
	arm_smccc_1_1_smc(MTK_SIP_EMIMPU_CONTROL_KERNEL, KP_SET, start, end, region, 0, 0, 0, &res)

int emi_kp_set_protection(unsigned int start, unsigned int end, unsigned int region, enum smc_source src)
{
	struct arm_smccc_res res;

	// rkp_printf("%s: src:%d region:%u 0x%lx-0x%lx\n", __func__, src, region, start, end);
	if (region >= EMI_KP_REGION_NUM) {
		// rkp_printf("%s: kernel protection can not support region %u\n", __func__, region);
		return -1;
	}

	/* Issue smc call according to the origination */
	if (src == MKP_KERNEL)
		emi_kp_smc_set_kernel(start, end, region, res);
	else
		return -1;

	return (int)res.a0;
}
