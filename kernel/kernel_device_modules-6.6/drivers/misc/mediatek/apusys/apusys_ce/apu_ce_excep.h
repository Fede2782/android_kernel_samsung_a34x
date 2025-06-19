/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_CE_EXCEP_H
#define APU_CE_EXCEP_H

#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include "apu.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define apusys_ce_aee_warn(module, reason) \
	do { \
		char mod_name[150];\
		if (snprintf(mod_name, 150, "%s_%s", reason, module) > 0) { \
			dev_info(dev, "%s: %s\n", reason, module); \
			aee_kernel_exception(mod_name, \
				"\nCRDISPATCH_KEY:%s\n", module); \
		} else { \
			dev_info(dev, "%s: snprintf fail(%d)\n", __func__, __LINE__); \
		} \
	} while (0)

	#define apusys_ce_exception_aee_warn(module) \
	do { \
		dev_info(dev, "APUSYS_CE_EXCEPTION: %s\n", module); \
		aee_kernel_exception("APUSYS_CE_EXCEPTION_APUSYS_CE", \
			"\nCRDISPATCH_KEY:%s\n", module); \
	} while (0)
#else
#define apusys_ce_aee_warn(module, reason)
#define apusys_ce_exception_aee_warn(module, reason)
#endif

int is_apu_ce_excep_init(void);
int apu_ce_excep_init(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_excep_remove(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_mrdump_register(struct mtk_apu *apu);
void apu_ce_procfs_init(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
void apu_ce_procfs_remove(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
uint32_t apu_ce_reg_dump(struct device *dev);
uint32_t apu_ce_sram_dump(struct device *dev);

#endif /* APU_CE_EXCEP_H */
