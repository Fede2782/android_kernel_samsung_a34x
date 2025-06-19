// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#include "mtk-mml-dpc.h"
#include "mtk-mml-core.h"

int mml_dl_dpc = MML_DPC_PKT_VOTE;
module_param(mml_dl_dpc, int, 0644);

static struct dpc_funcs mml_dpc_funcs;
static enum mtk_dpc_version mml_dpc_version;

#define mml_sysid_to_dpc_subsys(sysid)	\
	(sysid == mml_sys_frame ? DPC_SUBSYS_MML1 : DPC_SUBSYS_MML0)
#define mml_sysid_to_dpc_user(sysid)	\
	(sysid == mml_sys_frame ? DISP_VIDLE_USER_MML1 : DISP_VIDLE_USER_MML0)
#define mml_sysid_to_dpc_user_cmdq(sysid)	\
	(sysid == mml_sys_frame ? DISP_VIDLE_USER_MML1_CMDQ : DISP_VIDLE_USER_MML0_CMDQ)
#define mml_sysid_to_dpc_srt_read_idx(sysid)	\
	(sysid == mml_sys_frame ? 8 : 0)
#define mml_sysid_to_dpc_hrt_read_idx(sysid)	\
	(sysid == mml_sys_frame ? 10 : 2)

void mml_dpc_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version)
{
	mml_dpc_funcs = *funcs;
	mml_dpc_version = version;
}
EXPORT_SYMBOL_GPL(mml_dpc_register);

void mml_dpc_enable(bool en)
{
	if (mml_dpc_funcs.dpc_enable == NULL) {
		mml_msg_dpc("%s dpc_enable not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_enable(en);
}

void mml_dpc_dc_force_enable(bool en)
{
	if (mml_dpc_funcs.dpc_dc_force_enable == NULL) {
		mml_msg_dpc("%s dpc_dc_force_enable not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dc_force_enable(en);
}

void mml_dpc_group_enable(u32 sysid, bool en)
{
	if (mml_dpc_funcs.dpc_group_enable == NULL) {
		mml_msg_dpc("%s dpc_group_enable not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_group_enable(mml_sysid_to_dpc_subsys(sysid), en);
}

void mml_dpc_mtcmos_auto(u32 sysid, const bool en, const s8 mode)
{
	enum mtk_dpc_mtcmos_mode mtcmos_mode = en ? DPC_MTCMOS_AUTO : DPC_MTCMOS_MANUAL;

	if (mml_dpc_version == DPC_VER2)
		return;

	if (mml_dpc_funcs.dpc_mtcmos_auto == NULL) {
		mml_msg_dpc("%s dpc_mtcmos_auto not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_mtcmos_auto(mml_sysid_to_dpc_subsys(sysid), mtcmos_mode);
}

void mml_dpc_config(const enum mtk_dpc_subsys subsys, bool en)
{
	if (mml_dpc_funcs.dpc_config == NULL) {
		mml_err("%s dpc_config not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_config(subsys, en);
}

void mml_dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys,
			 const u8 thread, const bool en)
{
	if (mml_dpc_funcs.dpc_mtcmos_vote == NULL) {
		mml_msg_dpc("%s dpc_mtcmos_vote not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_mtcmos_vote(subsys, thread, en);
}

int mml_dpc_power_keep(u32 sysid)
{
	const enum mtk_vidle_voter_user user = mml_sysid_to_dpc_user(sysid);

	if (mml_dpc_funcs.dpc_vidle_power_keep == NULL) {
		mml_msg_dpc("%s dpc_power_keep not exist", __func__);
		return -1;
	}

	mml_msg_dpc("%s exception flow keep sys id %u user %d", __func__, sysid, user);
	return mml_dpc_funcs.dpc_vidle_power_keep(user);
}

void mml_dpc_power_release(u32 sysid)
{
	const enum mtk_vidle_voter_user user = mml_sysid_to_dpc_user(sysid);

	if (mml_dpc_funcs.dpc_vidle_power_release == NULL) {
		mml_msg_dpc("%s dpc_power_release not exist", __func__);
		return;
	}

	mml_msg_dpc("%s exception flow release sys id %u user %d", __func__, sysid, user);
	mml_dpc_funcs.dpc_vidle_power_release(user);
}

int mml_dpc_power_keep_gce(u32 sysid, struct cmdq_pkt *pkt, u16 gpr, struct cmdq_poll_reuse *reuse)
{
	const enum mtk_vidle_voter_user user = mml_sysid_to_dpc_user_cmdq(sysid);

	if (!mml_dpc_funcs.dpc_vidle_power_keep_by_gce) {
		mml_msg_dpc("%s dpc_vidle_power_keep_by_gce not exist", __func__);
		return -1;
	}

	mml_msg_dpc("%s exception flow gce user %d keep", __func__, user);

	mml_dpc_funcs.dpc_vidle_power_keep_by_gce(pkt, user, gpr, reuse);
	return 0;
}

void mml_dpc_power_release_gce(u32 sysid, struct cmdq_pkt *pkt)
{
	const enum mtk_vidle_voter_user user = mml_sysid_to_dpc_user_cmdq(sysid);

	if (!mml_dpc_funcs.dpc_vidle_power_release_by_gce) {
		mml_msg_dpc("%s dpc_vidle_power_release_by_gce not exist", __func__);
		return;
	}

	mml_msg_dpc("%s exception flow gce user %d release", __func__, user);

	mml_dpc_funcs.dpc_vidle_power_release_by_gce(pkt, user);
}

void mml_dpc_hrt_bw_set(u32 sysid, const u32 bw_in_mb, bool force_keep)
{
	if (mml_dpc_funcs.dpc_hrt_bw_set == NULL) {
		mml_msg_dpc("%s dpc_hrt_bw_set not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_hrt_bw_set(mml_sysid_to_dpc_subsys(sysid), bw_in_mb, force_keep);

	/* report hrt read channel bw */
	if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
		mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_channel_bw_set_by_idx(mml_sysid_to_dpc_subsys(sysid),
		mml_sysid_to_dpc_hrt_read_idx(sysid), bw_in_mb);
}

void mml_dpc_srt_bw_set(u32 sysid, const u32 bw_in_mb, bool force_keep)
{
	if (mml_dpc_funcs.dpc_srt_bw_set == NULL) {
		mml_msg_dpc("%s dpc_srt_bw_set not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_srt_bw_set(mml_sysid_to_dpc_subsys(sysid), bw_in_mb, force_keep);

	/* report srt read channel bw */
	if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
		mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_channel_bw_set_by_idx(mml_sysid_to_dpc_subsys(sysid),
		mml_sysid_to_dpc_srt_read_idx(sysid), bw_in_mb);
}

void mml_dpc_dvfs_bw_set(u32 sysid, const u32 bw_in_mb)
{
	if (mml_dpc_funcs.dpc_dvfs_bw_set == NULL) {
		mml_msg_dpc("%s dpc_dvfs_bw_set not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dvfs_bw_set(mml_sysid_to_dpc_subsys(sysid), bw_in_mb);
}

void mml_dpc_dvfs_set(const u8 level, bool force)
{
	if (mml_dpc_funcs.dpc_dvfs_set == NULL) {
		mml_msg_dpc("%s dpc_dvfs_set not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dvfs_set(DPC_SUBSYS_MML, level, force);
}

void mml_dpc_dvfs_both_set(u32 sysid, const u8 level, bool force_keep, const u32 bw_in_mb)
{
	if (mml_dpc_funcs.dpc_dvfs_both_set == NULL) {
		mml_msg_dpc("%s dpc_dvfs_both_set not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dvfs_both_set(mml_sysid_to_dpc_subsys(sysid),
		level, force_keep, bw_in_mb);
}

void mml_dpc_dvfs_trigger(void)
{
	if (mml_dpc_funcs.dpc_dvfs_trigger == NULL) {
		mml_msg_dpc("%s dpc_dvfs_trigger not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dvfs_trigger("MML");
}

void mml_dpc_channel_bw_set_by_idx(u32 sysid, u32 bw, bool hrt)
{
	u8 idx;

	if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
		mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist", __func__);
		return;
	}

	if (sysid == mml_sys_tile)
		idx = hrt ? 3 : 1;
	else
		idx = hrt ? 11 : 9;

	mml_dpc_funcs.dpc_channel_bw_set_by_idx(mml_sysid_to_dpc_subsys(sysid), idx, bw);
}

void mml_dpc_dump(void)
{
	if (!mml_dpc_funcs.dpc_analysis) {
		mml_msg_dpc("%s dpc_analysis not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_analysis();
}
