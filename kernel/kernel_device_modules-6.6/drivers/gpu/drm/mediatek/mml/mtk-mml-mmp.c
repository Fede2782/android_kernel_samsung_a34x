// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-mml-mmp.h"

#ifndef MML_FPGA

static struct mml_mmp_events_t mml_mmp_events;

struct mml_mmp_events_t *mml_mmp_get_event(void)
{
	return &mml_mmp_events;
}

void mml_mmp_init(void)
{
	mmp_event parent;

	if (mml_mmp_events.mml)
		return;

	mmprofile_enable(1);
	mml_mmp_events.mml = mmprofile_register_event(MMP_ROOT_EVENT, "MML");
	parent = mml_mmp_events.mml;
	mml_mmp_events.query_mode = mmprofile_register_event(parent, "query_mode");
	mml_mmp_events.query_layer = mmprofile_register_event(parent, "query_layer");
	mml_mmp_events.submit = mmprofile_register_event(parent, "submit");
	mml_mmp_events.config = mmprofile_register_event(parent, "config");
	mml_mmp_events.flush = mmprofile_register_event(parent, "flush");
	mml_mmp_events.submit_cb = mmprofile_register_event(parent, "submit_cb");
	mml_mmp_events.taskdone = mmprofile_register_event(parent, "taskdone");
	mml_mmp_events.exec = mmprofile_register_event(parent, "exec");
	mml_mmp_events.racing_enter = mmprofile_register_event(parent, "racing_enter");
	mml_mmp_events.racing_stop = mmprofile_register_event(parent, "racing_stop");
	mml_mmp_events.racing_stop_sync = mmprofile_register_event(parent, "racing_stop_sync");
	mml_mmp_events.couple = mmprofile_register_event(parent, "couple");
	mml_mmp_events.kick = mmprofile_register_event(parent, "kick");
	mml_mmp_events.dvfs = mmprofile_register_event(parent, "dvfs");
	mml_mmp_events.addon = mmprofile_register_event(parent, "addon");
	mml_mmp_events.dle = mmprofile_register_event(parent, "dle");
	mml_mmp_events.dpc = mmprofile_register_event(parent, "dpc");
	mml_mmp_events.clock = mmprofile_register_event(parent, "clock");

	parent = mml_mmp_events.submit;
	mml_mmp_events.task_create = mmprofile_register_event(parent, "task_create");
	mml_mmp_events.buf_map = mmprofile_register_event(parent, "buf_map");

	parent = mml_mmp_events.config;
	mml_mmp_events.config_dle = mmprofile_register_event(parent, "config_dle");
	mml_mmp_events.dumpinfo = mmprofile_register_event(parent, "dumpinfo");
	mml_mmp_events.comp_prepare = mmprofile_register_event(parent, "comp_prepare");
	mml_mmp_events.command = mmprofile_register_event(parent, "command");
	mml_mmp_events.fence = mmprofile_register_event(parent, "fence");
	mml_mmp_events.fence_timeout = mmprofile_register_event(parent, "fence_timeout");
	mml_mmp_events.wait_ready = mmprofile_register_event(parent, "wait_ready");

	parent = mml_mmp_events.command;
	mml_mmp_events.buf_prepare = mmprofile_register_event(parent, "buf_prepare");
	mml_mmp_events.command0 = mmprofile_register_event(parent, "command0");
	mml_mmp_events.command1 = mmprofile_register_event(parent, "command1");
	mml_mmp_events.tile_alloc = mmprofile_register_event(parent, "tile_alloc");
	mml_mmp_events.tile_calc = mmprofile_register_event(parent, "tile_calc");
	mml_mmp_events.tile_calc_frame = mmprofile_register_event(parent, "tile_calc_frame");
	mml_mmp_events.tile_prepare_tile = mmprofile_register_event(parent, "tile_prepare_tile");
	mml_mmp_events.mutex_mod = mmprofile_register_event(parent, "mutex_mod");
	mml_mmp_events.mutex_en = mmprofile_register_event(parent, "mutex_enable");
	mml_mmp_events.mutex_dis = mmprofile_register_event(parent, "mutex_disable");

	parent = mml_mmp_events.taskdone;
	mml_mmp_events.irq_loop = mmprofile_register_event(parent, "irq_loop");
	mml_mmp_events.irq_err = mmprofile_register_event(parent, "irq_err");
	mml_mmp_events.irq_done = mmprofile_register_event(parent, "irq_done");
	mml_mmp_events.irq_stop = mmprofile_register_event(parent, "irq_stop");
	mml_mmp_events.fence_sig = mmprofile_register_event(parent, "fence_sig");

	parent = mml_mmp_events.dvfs;
	mml_mmp_events.throughput = mmprofile_register_event(parent, "throughput");
	mml_mmp_events.bandwidth = mmprofile_register_event(parent, "bandwidth");
	mml_mmp_events.mmdvfs = mmprofile_register_event(parent, "vcp_mmdvfs");

	parent = mml_mmp_events.addon;
	mml_mmp_events.addon_mml_calc_cfg = mmprofile_register_event(parent, "mml_calc_cfg");
	mml_mmp_events.addon_addon_config = mmprofile_register_event(parent, "addon_config");
	mml_mmp_events.addon_start = mmprofile_register_event(parent, "start");
	mml_mmp_events.addon_unprepare = mmprofile_register_event(parent, "unprepare");
	mml_mmp_events.addon_dle_config = mmprofile_register_event(parent, "dle_config");

	parent = mml_mmp_events.dle;
	mml_mmp_events.dle_config_create = mmprofile_register_event(parent, "config_create");
	mml_mmp_events.dle_aal_irq_done = mmprofile_register_event(parent, "aal_irq_done");

	parent = mml_mmp_events.dpc;
	mml_mmp_events.dpc_exception_flow = mmprofile_register_event(parent, "dpc_exception_flow");
	mml_mmp_events.dpc_dc = mmprofile_register_event(parent, "dpc_dc");
	mml_mmp_events.dpc_bw_hrt = mmprofile_register_event(parent, "dpc_bw_hrt");
	mml_mmp_events.dpc_bw_srt = mmprofile_register_event(parent, "dpc_bw_srt");
	mml_mmp_events.dpc_dvfs = mmprofile_register_event(parent, "dpc_dvfs");

	parent = mml_mmp_events.clock;
	mml_mmp_events.wake_lock = mmprofile_register_event(parent, "wake_lock");
	mml_mmp_events.wake_unlock = mmprofile_register_event(parent, "wake_unlock");
	mml_mmp_events.mminfra_enable = mmprofile_register_event(parent, "mminfra_enable");
	mml_mmp_events.mminfra_disable = mmprofile_register_event(parent, "mminfra_disable");
	mml_mmp_events.pw_get = mmprofile_register_event(parent, "pw_get");
	mml_mmp_events.pw_put = mmprofile_register_event(parent, "pw_put");
	mml_mmp_events.clk_enable = mmprofile_register_event(parent, "clk_enable");
	mml_mmp_events.clk_disable = mmprofile_register_event(parent, "clk_disable");

	mmprofile_enable_event_recursive(mml_mmp_events.mml, 1);
	mmprofile_start(1);
}

#endif
