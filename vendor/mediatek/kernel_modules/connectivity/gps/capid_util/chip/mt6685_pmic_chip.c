/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "../include/pmic_chip_data.h"
#include <linux/string.h>

/* Chip-specific register and mask definitions */
#define DCXO_AAC_CW1              (0x7c2)
#define DCXO_CDAC_CW1             (0x7c0)
#define RG_XO_AAC_FPM_SWEN_ADDR   (DCXO_AAC_CW1)
#define RG_XO_AAC_FPM_SWEN_MASK   (0x7f)
#define RG_XO_AAC_FPM_SWEN_SHIFT  (0)
#define RG_XO_CDAC_FPM_ADDR       (DCXO_CDAC_CW1)
#define RG_XO_CDAC_FPM_MASK       (0xff)
#define RG_XO_CDAC_FPM_SHIFT      (0)

/* Exported chip data for mt6685 */
const struct pmic_chip_data mt6685_chip_data = {
	.dcxo_aac_cw1 = DCXO_AAC_CW1,
	.dcxo_cdac_cw1 = DCXO_CDAC_CW1,
	.rg_xo_aac_fpm_swen_addr = RG_XO_AAC_FPM_SWEN_ADDR,
	.rg_xo_aac_fpm_swen_mask = RG_XO_AAC_FPM_SWEN_MASK,
	.rg_xo_aac_fpm_swen_shift = RG_XO_AAC_FPM_SWEN_SHIFT,
	.rg_xo_cdac_fpm_addr = RG_XO_CDAC_FPM_ADDR,
	.rg_xo_cdac_fpm_mask = RG_XO_CDAC_FPM_MASK,
	.rg_xo_cdac_fpm_shift = RG_XO_CDAC_FPM_SHIFT,
};
