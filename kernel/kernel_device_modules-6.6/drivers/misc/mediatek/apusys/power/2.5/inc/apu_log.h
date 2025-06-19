/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_LOG_H__
#define __APU_LOG_H__
#include <linux/device.h>
#include <mt-plat/aee.h>

#include "apu_dbg.h"
#include "apu_common.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define apu_pwr_aee_warn(module, reason) \
	do { \
		char mod_name[150];\
		if (snprintf(mod_name, 150, "%s_%s", reason, module) > 0) { \
			pr_notice("CRDISPATCH_KEY %s: %s\n", reason, module); \
				aee_kernel_exception(mod_name, \
				"\nCRDISPATCH_KEY:%s\n", module); \
		} else { \
			pr_notice("%s: snprintf fail(%d)\n", \
				__func__, __LINE__); \
		} \
	} while (0)
#else
#define apu_pwr_aee_warn(module, reason)
#endif

#define APUCLK_PRE	"clk"
#define aclk_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APUCLK_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_CLK"); \
	} while (0)
#define aclk_info(dev, fmt, ...) \
	do { \
		if (apupw_dbg_get_loglvl() > NO_LVL) \
			pr_info("[%s:"APUCLK_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
	} while (0)
#define aclk_warn(dev, fmt, ...) \
	pr_info("[%s:"APUCLK_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define aclk_debug(dev, fmt, ...) \
	pr_debug("[%s:"APUCLK_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)

#define APURGU_PRE	"rgu"
#define argul_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APURGU_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_REGULATOR"); \
	} while (0)
#define argul_info(dev, fmt, ...) \
	do { \
		if (apupw_dbg_get_loglvl() > NO_LVL) \
			pr_info("[%s:"APURGU_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
	} while (0)
#define argul_warn(dev, fmt, ...) \
	pr_info("[%s:"APURGU_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define argul_debug(dev, fmt, ...) \
	pr_debug("[%s:"APURGU_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)

#define APUPROBE_PRE	"probe"
#define aprobe_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APUPROBE_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_PROBE"); \
	} while (0)
#define aprobe_info(dev, fmt, ...) \
	do { \
		if (apupw_dbg_get_loglvl() > NO_LVL) \
			pr_info("[%s:"APUPROBE_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
	} while (0)
#define aprobe_warn(dev, fmt, ...) \
	pr_info("[%s:"APUPROBE_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define aprobe_debug(dev, fmt, ...) \
	pr_debug("[%s:"APUPROBE_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)

#define APUPW_PRE	"power"
#define apower_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APUPW_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_PWR"); \
	} while (0)
#define apower_info(dev, fmt, ...) \
do { \
	if (apupw_dbg_get_loglvl() > NO_LVL) \
		pr_info("[%s:"APUPW_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
} while (0)
#define apower_warn(dev, fmt, ...) \
	pr_info("[%s:"APUPW_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define apower_debug(dev, fmt, ...) \
	pr_debug("[%s:"APUPW_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)

#define APURPC_PRE	"rpc"
#define arpc_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APURPC_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_RPC"); \
	} while (0)
#define arpc_info(dev, fmt, ...) \
do { \
	if (apupw_dbg_get_loglvl() > NO_LVL) \
		pr_info("[%s:"APURPC_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
} while (0)
#define arpc_warn(dev, fmt, ...) \
	pr_info("[%s:"APURPC_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define arpc_debug(dev, fmt, ...) \
	pr_debug("[%s:"APURPC_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)


#define APUSPM_PRE	"spm"
#define aspm_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APUSPM_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_SPM"); \
	} while (0)
#define aspm_info(dev, fmt, ...) \
do { \
	if (apupw_dbg_get_loglvl() > NO_LVL) \
		pr_info("[%s:"APUSPM_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
} while (0)
#define aspm_warn(dev, fmt, ...) \
	pr_info("[%s:"APUSPM_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define aspm_debug(dev, fmt, ...) \
	pr_debug("[%s:"APUSPM_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)

#define APUDVFS_PRE	"dvfs"
#define advfs_err(dev, fmt, ...) \
	do { \
		pr_info("[%s:"APUDVFS_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
		apu_pwr_aee_warn("APUSYS_POWER", "APU_DVFS"); \
	} while (0)
#define advfs_info(dev, fmt, ...) \
do { \
	if (apupw_dbg_get_loglvl() > NO_LVL) \
		pr_info("[%s:"APUDVFS_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__); \
} while (0)
#define advfs_warn(dev, fmt, ...) \
	pr_info("[%s:"APUDVFS_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)
#define advfs_debug(dev, fmt, ...) \
	pr_debug("[%s:"APUDVFS_PRE"]" fmt, apu_dev_name(dev), ##__VA_ARGS__)

#endif
