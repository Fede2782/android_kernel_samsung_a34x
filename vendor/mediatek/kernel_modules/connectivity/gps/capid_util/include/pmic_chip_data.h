/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef PMIC_CHIP_DATA_H
#define PMIC_CHIP_DATA_H

#include <linux/types.h>

/*
 * Use kernel types instead of stdint types for compatibility.
 */
/*
 * Use kernel types instead of stdint types for compatibility.
 * Use __u32 for maximum compatibility in kernel headers.
 */
struct pmic_chip_data {
	__u32 dcxo_aac_cw1;
	__u32 dcxo_cdac_cw1;
	__u32 rg_xo_aac_fpm_swen_addr;
	__u32 rg_xo_aac_fpm_swen_mask;
	__u32 rg_xo_aac_fpm_swen_shift;
	__u32 rg_xo_cdac_fpm_addr;
	__u32 rg_xo_cdac_fpm_mask;
	__u32 rg_xo_cdac_fpm_shift;
};

const struct pmic_chip_data *get_pmic_chip_data(const char *compatible);

extern const struct pmic_chip_data mt6685_chip_data;
int is_mt6685_compatible(const char *compatible);

#endif /* PMIC_CHIP_DATA_H */
