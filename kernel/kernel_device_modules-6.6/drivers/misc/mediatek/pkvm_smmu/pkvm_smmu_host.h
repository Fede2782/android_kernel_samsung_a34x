/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
void kvm_nvhe_sym(add_smmu_device)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_share)(struct user_pt_regs *);
void kvm_nvhe_sym(pctrl_setup)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_host_debug)(struct user_pt_regs *);
void kvm_nvhe_sym(setup_vm)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_iommu_init)(struct user_pt_regs *);
void kvm_nvhe_sym(smmu_finalise)(struct user_pt_regs *);
/* For page base mapping */
void kvm_nvhe_sym(mtk_smmu_secure_v2)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_unsecure_v2)(struct user_pt_regs *);
/* For region base mapping */
void kvm_nvhe_sym(mtk_smmu_secure)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_unsecure)(struct user_pt_regs *);
/* For smmu s2 page table merge */
void kvm_nvhe_sym(smmu_merge_s2_table)(struct user_pt_regs *);
extern struct kvm_iommu_ops kvm_nvhe_sym(smmu_ops);
