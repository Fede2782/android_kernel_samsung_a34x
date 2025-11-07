/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "../include/pmic_chip_data.h"
#include <linux/string.h>


int is_mt6685_compatible(const char *compatible)
{
	if (!compatible)
		return 0;
	return strstr(compatible, "mt6685") ? 1 : 0;
}

const struct pmic_chip_data *get_pmic_chip_data(const char *compatible)
{
	if (!compatible)
		return NULL;

	if (is_mt6685_compatible(compatible))
		return &mt6685_chip_data;

	return NULL;
}
