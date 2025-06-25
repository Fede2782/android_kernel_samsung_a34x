/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6989_GPU_H
#define MBRAINK_V6989_GPU_H
#include <mbraink_ioctl_struct_def.h>

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_v6989_gpu_init(void);
int mbraink_v6989_gpu_deinit(void);

void mbraink_v6989_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS);
void mbraink_v6989_gpu_setPerfIdxTimeoutInNS(unsigned long long perfIdxTimeoutInNS);
void mbraink_v6989_gpu_setPerfIdxLimit(int perfIdxLimit);
void mbraink_v6989_gpu_dumpPerfIdxList(void);

#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)

void fpsgo2mbrain_hint_frameinfo(int pid, unsigned long long bufID,
	int fps, unsigned long long time);

void fpsgo2mbrain_hint_perfinfo(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts);

void fpsgo2mbrain_hint_deleteperfinfo(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts);

#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
void gpu2mbrain_hint_fenceTimeoutNotify(int pid, void *data, unsigned long long time);
void gpu2mbrain_hint_GpuResetDoneNotify(unsigned long long time);
#endif

#endif /*end of MBRAINK_V6989_GPU_H*/
