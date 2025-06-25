/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _SLBC_SDK_H_
#define _SLBC_SDK_H_

#include <slbc_ops.h>

#if IS_ENABLED(CONFIG_MTK_SLBC)
extern int slbc_force_cache(enum slc_ach_uid uid, unsigned int size);
extern int slbc_force_cache_ratio(enum slc_ach_uid uid, unsigned int ratio);
extern int slbc_ceil(enum slc_ach_uid uid, unsigned int ceil);
extern int slbc_total_ceil(unsigned int ceil);
extern int slbc_window(unsigned int window);
extern int slbc_disable_dcc(bool disable);
extern int slbc_disable_slc(bool disable);
extern int slbc_cg_priority(bool gpu_first);
extern int slbc_get_cache_size(enum slc_ach_uid uid);
extern int slbc_get_cache_hit_rate(enum slc_ach_uid uid);
extern int slbc_get_cache_hit_bw(enum slc_ach_uid uid);
extern int slbc_get_cache_usage(int *cpu, int *gpu, int *other);
#else
__weak int slbc_force_cache(enum slc_ach_uid uid, unsigned int size) { return -EDISABLED; }
__weak int slbc_force_cache_ratio(enum slc_ach_uid uid, unsigned int ratio) { return -EDISABLED; }
__weak int slbc_ceil(enum slc_ach_uid uid, unsigned int ceil) { return -EDISABLED; }
__weak int slbc_total_ceil(unsigned int ceil) { return -EDISABLED; }
__weak int slbc_window(unsigned int window) { return -EDISABLED; }
__weak int slbc_disable_dcc(bool disable) { return -EDISABLED; }
__weak int slbc_disable_slc(bool disable) { return -EDISABLED; }
__weak int slbc_cg_priority(bool gpu_first) { return -EDISABLED; }
__weak int slbc_get_cache_size(enum slc_ach_uid uid) { return -EDISABLED; }
__weak int slbc_get_cache_hit_rate(enum slc_ach_uid uid) { return -EDISABLED; }
__weak int slbc_get_cache_hit_bw(enum slc_ach_uid uid) { return -EDISABLED; }
__weak int slbc_get_cache_usage(int *cpu, int *gpu, int *other) { return -EDISABLED; }
#endif /* CONFIG_MTK_SLBC */

#endif /* _SLBC_SDK_H_ */
