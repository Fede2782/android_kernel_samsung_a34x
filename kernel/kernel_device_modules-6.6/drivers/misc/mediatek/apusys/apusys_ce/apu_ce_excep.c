// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "apu.h"
#include "apu_ce_excep.h"
#include "apu_ce_excep_v1.h"

enum APU_CE_VER {
	APU_CE_VER_UNINIT,
	APU_CE_VER_1,
};

static enum APU_CE_VER g_apu_ce_ver;

uint32_t apu_ce_reg_dump(struct device *dev)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		apu_ce_reg_dump_v1(dev);

	return 0;
}

uint32_t apu_ce_sram_dump(struct device *dev)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		apu_ce_sram_dump_v1(dev);

	return 0;
}

int apu_ce_excep_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	if (apu_ce_excep_is_compatible_v1(pdev)) {
		g_apu_ce_ver = APU_CE_VER_1;
		return apu_ce_excep_init_v1(pdev, apu);
	}

	return -ENODEV;
}

void apu_ce_excep_remove(struct platform_device *pdev, struct mtk_apu *apu)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		apu_ce_excep_init_v1(pdev, apu);

	g_apu_ce_ver = APU_CE_VER_UNINIT;
}

int is_apu_ce_excep_init(void)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		return is_apu_ce_excep_init_v1();

	return 0;
}

void apu_ce_mrdump_register(struct mtk_apu *apu)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		return apu_ce_mrdump_register_v1(apu);
}

void apu_ce_procfs_init(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		return apu_ce_procfs_init_v1(pdev, procfs_root);
}

void apu_ce_procfs_remove(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root)
{
	if (g_apu_ce_ver == APU_CE_VER_1)
		return apu_ce_procfs_remove_v1(pdev, procfs_root);
}
