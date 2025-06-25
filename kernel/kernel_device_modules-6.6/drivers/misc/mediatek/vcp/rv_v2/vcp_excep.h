/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_EXCEP_H__
#define __VCP_EXCEP_H__

#include <linux/sizes.h>
#include "vcp_helper.h"
#include "vcp_ipi_pin.h"

struct vcp_status_reg {
	uint32_t status;
	uint32_t pc;
	uint32_t lr;
	uint32_t sp;
	uint32_t pc_latch;
	uint32_t lr_latch;
	uint32_t sp_latch;
};

extern void vcp_dump_last_regs(int mmup_enable);
extern void vcp_aed(enum VCP_RESET_TYPE type, enum vcp_core_id id);
extern void vcp_get_log(enum vcp_core_id id);
extern char *vcp_pickup_log_for_aee(void);
extern void aed_vcp_exception_api(const int *log, int log_size,
		const int *phy, int phy_size, const char *detail,
		const int db_opt);
extern uint32_t vcp_dump_size_probe(struct platform_device *pdev);
extern int vcp_excep_mode;
extern unsigned int vcp_reset_counts;
extern void __iomem *vcp_res_req_status_reg;

extern struct vcp_status_reg *c0_m;
extern struct vcp_status_reg *c0_t1_m;
extern struct vcp_status_reg *c1_m;
extern struct vcp_status_reg *c1_t1_m;

enum MDUMP_t {
	MDUMP_DUMMY,
	MDUMP_L2TCM,
	MDUMP_L1C,
	MDUMP_REGDUMP,
	MDUMP_TBUF,
	MDUMP_DRAM,
	MDUMP_TOTAL
};

#endif
