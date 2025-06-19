/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_MMP_H__
#define __MTK_DPC_MMP_H__

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#define dpc_mmp(args...)

static inline void dpc_v1_mmp_init(void) {}
#else

#include <mmprofile.h>
#include <mmprofile_function.h>

#define dpc_mmp(event, flag, v1, v2) \
	mmprofile_log_ex(dpc_v1_mmp_get_event()->event, flag, v1, v2)

#define MTK_DPC_UPDATE_MMP_RANGE(tag, cnt, data0, data1) \
{ \
	if (cnt == 0) \
		dpc_mmp(tag, MMPROFILE_FLAG_END, data0, data1); \
	else if (cnt == 1) \
		dpc_mmp(tag, MMPROFILE_FLAG_START, data0, data1); \
	else if (cnt > 1)\
		dpc_mmp(tag, MMPROFILE_FLAG_PULSE, data0, data1); \
}

struct dpc_v1_mmp_events_t {
	mmp_event dpc;
	mmp_event group;
	mmp_event disp_group_auto;
	mmp_event mml_group_auto;
	mmp_event dt;
	mmp_event disp_dt;
	mmp_event mml_dt;
	mmp_event disp_irq;
	mmp_event mml_irq;
	// mmp_event dt_te;
	// mmp_event dt_sof;
	// mmp_event dsi_start;
	mmp_event window;
	mmp_event mml_window;
	mmp_event disp_window;
	mmp_event prete;
	mmp_event mminfra_off;
	mmp_event apsrc_off;
	//mmp_event emi_off;
	mmp_event vlp_vote;
	mmp_event cpu_vote;
	mmp_event gce_vote;
	mmp_event skip_vote;
	mmp_event mml_rrot_done;
	mmp_event mml_sof;
	// mmp_event mml_mutex40;
	// mmp_event mml_mutex48;
	// mmp_event mml_mutex50;
	mmp_event mtcmos_off;
	mmp_event mtcmos_ovl0;
	// mmp_event mtcmos_ovl1;
	// mmp_event mtcmos_disp0;
	mmp_event mtcmos_mml1;
	mmp_event disp_mtcmos_auto;
	mmp_event mml_mtcmos_auto;
	mmp_event ovl0_vote;
	mmp_event disp1_vote;
	mmp_event mml1_vote;
	mmp_event dvfs_off;
	mmp_event vdisp_off;
	mmp_event vdisp_level;
	mmp_event vdisp_disp;
	mmp_event vdisp_mml;
	mmp_event hrt_bw;
	mmp_event srt_bw;
	mmp_event mmdvfs_dead;
};


struct dpc_v1_mmp_events_t *dpc_v1_mmp_get_event(void);
void dpc_v1_mmp_init(void);
#endif

#endif
