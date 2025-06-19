// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "mtk_dpc_mmp.h"

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#else

static struct dpc_mmp_events_t dpc_mmp_events;

struct dpc_mmp_events_t *dpc_mmp_get_event(void)
{
	return &dpc_mmp_events;
}

void dpc_mmp_init(void)
{
	mmp_event folder;

	if (dpc_mmp_events.folder)
		return;

	mmprofile_enable(1);
	folder = mmprofile_register_event(MMP_ROOT_EVENT, "DPC");
	dpc_mmp_events.folder = folder;
	dpc_mmp_events.config = mmprofile_register_event(folder, "config");
	dpc_mmp_events.prete = mmprofile_register_event(folder, "prete");
	dpc_mmp_events.mminfra = mmprofile_register_event(folder, "mminfra");
	dpc_mmp_events.vlp_vote = mmprofile_register_event(folder, "vlp_vote");
	dpc_mmp_events.mml_sof = mmprofile_register_event(folder, "mml_sof");
	dpc_mmp_events.mml_rrot_done = mmprofile_register_event(folder, "mml_rrot_done");
	dpc_mmp_events.idle_off = mmprofile_register_event(folder, "idle_off");
	dpc_mmp_events.mtcmos_ovl0 = mmprofile_register_event(folder, "mtcmos_ovl0");
	dpc_mmp_events.mtcmos_disp1 = mmprofile_register_event(folder, "mtcmos_disp1");
	dpc_mmp_events.mtcmos_mml1 = mmprofile_register_event(folder, "mtcmos_mml1");
	dpc_mmp_events.vdisp_level = mmprofile_register_event(folder, "vdisp_level");
	dpc_mmp_events.mtcmos_auto = mmprofile_register_event(folder, "mtcmos_auto");
	dpc_mmp_events.ch_bw = mmprofile_register_event(folder, "ch_bw");
	dpc_mmp_events.hrt_bw = mmprofile_register_event(folder, "hrt_bw");
	dpc_mmp_events.srt_bw = mmprofile_register_event(folder, "srt_bw");

	mmprofile_enable_event_recursive(folder, 1);
	mmprofile_start(1);
}

#endif
