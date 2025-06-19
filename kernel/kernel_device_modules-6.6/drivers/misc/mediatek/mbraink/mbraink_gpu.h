/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_GPU_H
#define MBRAINK_GPU_H
#include <mbraink_ioctl_struct_def.h>

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_gpu_init(void);
int mbraink_gpu_deinit(void);
int mbraink_gpu_featureEnable(bool bEnable);

ssize_t getTimeoutCouterReport(char *pBuf);

int mbraink_gpu_getOppInfo(struct mbraink_gpu_opp_info *gOppInfo);
int mbraink_gpu_getStateInfo(struct mbraink_gpu_state_info *gStateInfo);
int mbraink_gpu_getLoadingInfo(struct mbraink_gpu_loading_info *gLoadingInfo);

void mbraink_gpu_setOpMode(int OpMode);
#endif /*end of MBRAINK_GPU_H*/
