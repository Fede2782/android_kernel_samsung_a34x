// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/kvm_pkvm_module.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "../include/pkvm_mgmt/pkvm_mgmt.h"
#include "pkvm_tmem_host.h"

static int __init test_nvhe_init(void)
{
	int ret;
	unsigned long token;
	struct arm_smccc_res res;

	if (!is_protected_kvm_enabled()) {
		pr_info("skip to load pkvm_tmem\n");
		return 0;
	}

	ret = pkvm_load_el2_module(__kvm_nvhe_hyp_tmem_init, &token);
	if (ret) {
		pr_info("%s: pkvm_load_el2_module() fail, ret=%d\n", __func__, ret);
		return ret;
	}

	pr_info("hyp_tmem_init done\n");

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_region_protect, token);
	if (ret < 0)
		return ret;
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC, SMC_ID_MTK_PKVM_TMEM_REGION_PROTECT,
				ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_region_unprotect, token);
	if (ret < 0)
		return ret;
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC, SMC_ID_MTK_PKVM_TMEM_REGION_UNPROTECT,
				ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_page_protect, token);
	if (ret < 0)
		return ret;
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC, SMC_ID_MTK_PKVM_TMEM_PAGE_PROTECT,
				ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_page_unprotect, token);
	if (ret < 0)
		return ret;
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC, SMC_ID_MTK_PKVM_TMEM_PAGE_UNPROTECT,
				ret, 0, 0, 0, 0, &res);

	return 0;
}
module_init(test_nvhe_init);
MODULE_LICENSE("GPL");
