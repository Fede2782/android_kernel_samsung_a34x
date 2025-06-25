// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2021 The Hafnium Authors.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "smmu_mgmt.h"
#include "arm_smmuv3.h"

int smmuv3_config_ste_stg2(uint64_t s2_ps_bits, uint64_t pa_bits, uint64_t ias,
			   uint16_t vmid, struct mm_ptable_b ptable,
			   uint64_t *ste_data)
{
	uint64_t sl0;
	uint64_t vttbr;

  /*
   * Determine sl0, starting level of the page table, based on the number
   * of bits.
   *
   * - 0 => start at level 1
   * - 1 => start at level 2
   * - 2 => start at level 3
   */
	if (pa_bits >= 44)
		sl0 = 2;
	else if (pa_bits >= 35)
		sl0 = 1;
	else
		sl0 = 0;

  /* The following fields have to be programmed for Stage 2 translation:
   * Fields common to Secure and Non-Secure STE
   Bits		Name		Description
   ------------------------------------------------------------------------
   178:176		S2PS		PA size of stg2 PA range
   179		S2AA64		Select between AArch32 or AArch64 format
   180		S2ENDI		Endianness for stg2 translation tables
   181		S2AFFD		Disable access flag for stg2 translation
   167:166		S2SL0		Starting level of stg2 translation table
   walk
   169:168		S2IR0		Stg2 Inner region cachebility
   171:170		S2OR0		Stg2 Outer region cachebility
   173:172		S2SH0		Shareability
   143:128		S2VMID		VMID associated with current translation
   182		S2PTW		Protected Table Walk
   243:196		S2TTB		Stg2 translation table base address
   165:160		S2T0SZ		Size of IPA input region covered by stg2
   175:174		S2TG		Translation granularity

   * Fields specific to Secure STE
   Bits		Name		Description
   ------------------------------------------------------------------------
   192		S2NSW		NS bit used for all stg2 translation
   table walks for secure stream Non-secure IPA space
   193		S2NSA		NS bit output for all stg2 secure stream
   non-secure IPA translations
   435:388		S_S2TTB		Secure Stg2 TTB
   293:288		S_S2T0SZ	Secure version of S2T0SZ
   303:302		S_S2TG		Secure version of S2TG
   384		S2SW		NS bit used for all stg2 translation
   table walks for Secure IPA space
   385		S2SA		NS bit output for all stg2 Secure IPA translations
   */
	/* BITS 63:0 */
	ste_data[0] =
		STE_VALID | COMPOSE(STE_CFG_STG2, STE_CFG_SHIFT, STE_CFG_MASK);

	/* BITS 191:128 */
	ste_data[2] = COMPOSE(vmid, STE_VMID_SHIFT, STE_VMID_MASK);
	ste_data[2] |= COMPOSE(64 - ias, STE_S2T0SZ_SHIFT, STE_S2T0SZ_MASK);
	ste_data[2] |= COMPOSE(sl0, STE_S2SL0_SHIFT, STE_S2SL0_MASK);
	ste_data[2] |= COMPOSE(WB_CACHEABLE, STE_S2IR0_SHIFT, STE_S2IR0_MASK);
	ste_data[2] |= COMPOSE(WB_CACHEABLE, STE_S2OR0_SHIFT, STE_S2OR0_MASK);
	ste_data[2] |=
		COMPOSE(INNER_SHAREABLE, STE_S2SH0_SHIFT, STE_S2SH0_MASK);
	ste_data[2] |= COMPOSE(S2TF_4KB, STE_S2TG_SHIFT, STE_S2TG_MASK);
	ste_data[2] |= COMPOSE(s2_ps_bits, STE_S2PS_SHIFT, STE_S2PS_MASK);
	ste_data[2] |= COMPOSE(S2AA64, STE_S2AA64_SHIFT, STE_S2AA64_MASK);
	ste_data[2] |=
		COMPOSE(S2_LITTLEENDIAN, STE_S2ENDI_SHIFT, STE_S2ENDI_MASK);
	ste_data[2] |= COMPOSE(AF_DISABLED, STE_S2AFFD_SHIFT, STE_S2AFFD_MASK);
	ste_data[2] |=
		COMPOSE(PTW_DEVICE_FAULT, STE_S2PTW_SHIFT, STE_S2PTW_MASK);
	ste_data[2] |= COMPOSE(S2RS_RECORD, STE_S2RS_SHIFT, STE_S2RS_MASK);

	/* BITS 255:192 */
	vttbr = (ptable.root & GEN_MASK(51, 4)) >> 4;
	ste_data[3] = COMPOSE(vttbr, STE_S2TTB_SHIFT, STE_S2TTB_MASK);

	return 0;
}
