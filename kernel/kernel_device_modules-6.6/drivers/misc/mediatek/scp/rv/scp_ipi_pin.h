/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _SCP_IPI_PIN_H_
#define _SCP_IPI_PIN_H_

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "scp.h"

/* scp awake timeout count definition */
#define SCP_AWAKE_TIMEOUT scp_awake_timeout
/* scp ready to reboot count define*/
#define SCP_WFI_WAIT_TIMEOUT 100000
/* scp ipi timeout dump count define*/
#define  SCP_IPI_DUMP_TIMEOUT scp_ipi_dump_timout

/* this is only for ipi to distinguish core0 and core1 */
enum {
	SCP_CORE0_ID = 0,
	SCP_CORE1_ID = 1,
};

extern char *core_ids[SCP_CORE_TOTAL];

extern void scp_reset_awake_counts(void);
extern int scp_clr_spm_reg(void *unused);
extern int scp_awake_counts[];

#endif
