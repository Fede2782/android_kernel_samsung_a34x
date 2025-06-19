/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MAPPING_ATTR_H
#define __MAPPING_ATTR_H

#include <asm/kvm_pgtable.h>

// TODO: add comment
#define MT_X	KVM_PGTABLE_PROT_X
#define MT_RD	KVM_PGTABLE_PROT_R
#define MT_WR	KVM_PGTABLE_PROT_W
#define MT_UXN	KVM_PGTABLE_PROT_UXN
#define MT_PXN	KVM_PGTABLE_PROT_PXN
#define MT_RW	(MT_RD | MT_WR)
#define MT_AXN	(MT_UXN | MT_PXN)

#define MT_MEM	0x0
#define MT_DEV	KVM_PGTABLE_PROT_DEVICE

/* S2 permission */
#define S2AP_R	(0x1UL << 6)
#define S2AP_W	(0x1UL << 7)
#define S2XN	(0x1UL << 54)

/* S2 MemAttr[5..2] */
#define S2MT_NORMAL	(0x3UL << 4)

/* Default S2 attribute for MT_MEM */
#define S2_MEM_DEFAULT_ATTR	(MT_MEM | MT_RW | MT_X)

/* swbits, Bit 55-58 [reserved bits] */
#define HW_IS_FORBIDDEN	KVM_PGTABLE_PROT_SW3

#define KP_RGN_KERNEL_CODE	0
#define KP_RGN_KERNEL_RODATA	1

#endif
