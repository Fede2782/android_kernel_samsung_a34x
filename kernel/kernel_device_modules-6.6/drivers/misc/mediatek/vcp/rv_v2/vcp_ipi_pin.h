/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _VCP_IPI_PIN_H_
#define _VCP_IPI_PIN_H_

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "vcp.h"

/* vcp awake timeout count definition */
#define VCP_AWAKE_TIMEOUT 1000000

extern void vcp_reset_awake_counts(void);
extern int vcp_awake_counts[];

#endif
