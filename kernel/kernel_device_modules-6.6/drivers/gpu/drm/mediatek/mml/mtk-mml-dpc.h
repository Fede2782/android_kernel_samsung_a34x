/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#ifndef __MTK_MML_DPC_H__
#define __MTK_MML_DPC_H__

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk_dpc.h"

extern int mml_dl_dpc;

enum mml_dl_dpc_config {
	MML_DPC_PKT_VOTE = 0x1,	/* vote dpc before write and after done */
	MML_DPC_MUTEX_VOTE = 0x2,	/* do release and vote between sync disp */
};

#define VLP_VOTE_SET		0x414
#define VLP_VOTE_CLR		0x418

/*
 * mml_dpc_register - register dpc driver functions.
 *
 * @funcs:	DPC driver functions.
 */
void mml_dpc_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version);

void mml_dpc_enable(bool en);
void mml_dpc_dc_force_enable(bool en);
void mml_dpc_group_enable(u32 sysid, bool en);
void mml_dpc_mtcmos_auto(u32 sysid, const bool en, const s8 mode);
void mml_dpc_config(const enum mtk_dpc_subsys subsys, bool en);
void mml_dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys,
			 const u8 thread, const bool en);
int mml_dpc_power_keep(u32 sysid);
void mml_dpc_power_release(u32 sysid);
int mml_dpc_power_keep_gce(u32 sysid, struct cmdq_pkt *pkt, u16 gpr, struct cmdq_poll_reuse *reuse);
void mml_dpc_power_release_gce(u32 sysid, struct cmdq_pkt *pkt);
void mml_dpc_hrt_bw_set(u32 sysid, const u32 bw_in_mb, bool force_keep);
void mml_dpc_srt_bw_set(u32 sysid, const u32 bw_in_mb, bool force_keep);
void mml_dpc_dvfs_bw_set(u32 sysid, const u32 bw_in_mb);
void mml_dpc_dvfs_set(const u8 level, bool force);
void mml_dpc_dvfs_both_set(u32 sysid, const u8 level, bool force_keep, const u32 bw_in_mb);
void mml_dpc_dvfs_trigger(void);
void mml_dpc_channel_bw_set_by_idx(u32 sysid, u32 bw, bool hrt);

void mml_dpc_dump(void);

#endif	/* __MTK_MML_H__ */
