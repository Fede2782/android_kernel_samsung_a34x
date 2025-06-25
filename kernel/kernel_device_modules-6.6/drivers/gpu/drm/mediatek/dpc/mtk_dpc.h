/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_H__
#define __MTK_DPC_H__

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

/* NOTE: user 0 to 7 is reserved for genpd notifier enum disp_pd_id { ... } */
enum mtk_vidle_voter_user {
	DISP_VIDLE_USER_TOP_CLK_ISR = 15,
	DISP_VIDLE_USER_CRTC = 16,
	DISP_VIDLE_USER_PQ,
	DISP_VIDLE_USER_MML,
	DISP_VIDLE_USER_MML1 = DISP_VIDLE_USER_MML,
	DISP_VIDLE_USER_MDP,
	DISP_VIDLE_USER_MML0 = DISP_VIDLE_USER_MDP,
	DISP_VIDLE_USER_DISP_CMDQ,
	DISP_VIDLE_USER_DDIC_CMDQ, //21
	DISP_VIDLE_USER_PQ_CMDQ,
	DISP_VIDLE_USER_HSIDLE,
	DISP_VIDLE_USER_MML_CMDQ = 24,
	DISP_VIDLE_USER_MML1_CMDQ = DISP_VIDLE_USER_MML_CMDQ,
	DISP_VIDLE_USER_MML0_CMDQ = 25,
	DISP_VIDLE_USER_DISP_DPC_CFG = 26,
	DISP_VIDLE_USER_MML0_DPC_CFG = 27,
	DISP_VIDLE_USER_MML1_DPC_CFG = 28,
	DISP_VIDLE_USER_DPC_DUMP = 29,
	DISP_VIDLE_USER_SMI_DUMP = 30,
	DISP_VIDLE_FORCE_KEEP = 31,
	DISP_VIDLE_USER_MASK = 0x1f,
};

enum mtk_vidle_voter_status {
	VOTER_PM_FAILED = -1,
	VOTER_PM_DONE = 0,
	VOTER_PM_LATER,
	VOTER_ONLY = 0x1000,
};

enum mtk_dpc_mtcmos_mode {
	DPC_MTCMOS_MANUAL,
	DPC_MTCMOS_AUTO,
};

enum mtk_panel_type {
	PANEL_TYPE_CMD,
	PANEL_TYPE_VDO,
	PANEL_TYPE_COUNT,
};

enum mtk_dpc_version {
	DPC_VER_UNKNOWN,
	DPC_VER1,
	DPC_VER2,
	DPC_VER_CNT,
};

enum mtk_dpc_subsys {
	DPC_SUBSYS_DISP = 0,
	DPC_SUBSYS_DIS1 = 0,
	DPC_SUBSYS_DIS0 = 1,
	DPC_SUBSYS_OVL1 = 2,
	DPC_SUBSYS_OVL0 = 3,
	DPC_SUBSYS_MML = 4,
	DPC_SUBSYS_MML1 = 4,
	DPC_SUBSYS_MML0 = 5,
	DPC_SUBSYS_EDP = 6,
	DPC_SUBSYS_DPTX = 7,
	DPC_SUBSYS_CNT
};

enum mtk_dpc_disp_vidle {
	DPC_DISP_VIDLE_MTCMOS = 0,
	DPC_DISP_VIDLE_MTCMOS_DISP1 = 4,
	DPC_DISP_VIDLE_VDISP_DVFS = 8,
	DPC_DISP_VIDLE_HRT_BW = 11,
	DPC_DISP_VIDLE_SRT_BW = 14,
	DPC_DISP_VIDLE_MMINFRA_OFF = 17,
	DPC_DISP_VIDLE_INFRA_OFF = 20,
	DPC_DISP_VIDLE_MAINPLL_OFF = 23,
	DPC_DISP_VIDLE_MSYNC2_0 = 26,
	DPC_DISP_VIDLE_RESERVED = 29,
	DPC_DISP_VIDLE_MAX = 32,
};

enum mtk_dpc_mml_vidle {
	DPC_MML_VIDLE_MTCMOS = 32,
	DPC_MML_VIDLE_VDISP_DVFS = 36,
	DPC_MML_VIDLE_HRT_BW = 39,
	DPC_MML_VIDLE_SRT_BW = 42,
	DPC_MML_VIDLE_MMINFRA_OFF = 45,
	DPC_MML_VIDLE_INFRA_OFF = 48,
	DPC_MML_VIDLE_MAINPLL_OFF = 51,
	DPC_MML_VIDLE_RESERVED = 54,
};

struct dpc_funcs {
	/* only for display driver */
	void (*dpc_enable)(const u8 en);
	void (*dpc_duration_update)(u32 us);

	/* dpc driver internal use */
	void (*dpc_ddr_force_enable)(const enum mtk_dpc_subsys subsys, const bool en);

	/* resource auto mode control */
	void (*dpc_group_enable)(const u16 group, bool en);

	/* mtcmos auto mode control */
	void (*dpc_mtcmos_auto)(const enum mtk_dpc_subsys subsys, const enum mtk_dpc_mtcmos_mode mode);

	/* mtcmos and resource auto mode control */
	void (*dpc_pause)(const enum mtk_dpc_subsys subsys, bool en);
	int (*dpc_config)(const enum mtk_dpc_subsys subsys, bool en);

	/* exception power control */
	int (*dpc_vidle_power_keep)(const enum mtk_vidle_voter_user);
	void (*dpc_vidle_power_release)(const enum mtk_vidle_voter_user);
	void (*dpc_vidle_power_keep_by_gce)(struct cmdq_pkt *pkt,
					    const enum mtk_vidle_voter_user user, const u16 gpr,
					    struct cmdq_poll_reuse *reuse);
	void (*dpc_vidle_power_release_by_gce)(struct cmdq_pkt *pkt,
					    const enum mtk_vidle_voter_user user);

	/* dram dvfs
	 * @bw_in_mb: [U32_MAX]: read stored bw, otherwise update stored bw
	 * @force:    [TRUE]: trigger dvfs immediately
	 */
	void (*dpc_hrt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
	void (*dpc_srt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);

	/* vdisp dvfs
	 * @update_level: [TRUE]: update stored level, [FALSE]: only trigger dvfs
	 */
	void (*dpc_dvfs_set)(const enum mtk_dpc_subsys subsys, const u8 level, bool update_level);

	/* mminfra dvfs */
	void (*dpc_channel_bw_set_by_idx)(const enum mtk_dpc_subsys subsys, const u8 idx, const u32 bw_in_mb);

	/* trigger all dvfs immediately */
	void (*dpc_dvfs_trigger)(const char *caller);

	void (*dpc_clear_wfe_event)(struct cmdq_pkt *pkt, enum mtk_vidle_voter_user user, int event);
	void (*dpc_mtcmos_vote)(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
	void (*dpc_analysis)(void);
	void (*dpc_pm_analysis)(void);
	void (*dpc_debug_cmd)(const char *opt);

	/* V1 ONLY */
	void (*dpc_dc_force_enable)(const bool en);
	void (*dpc_init_panel_type)(enum mtk_panel_type);
	void (*dpc_dsi_pll_set)(const u32 value);
	int (*dpc_dt_set_all)(u32 dur_frame, u32 dur_vblank);
	void (*dpc_infra_force_enable)(const enum mtk_dpc_subsys subsys, const bool en);
	void (*dpc_dvfs_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
	void (*dpc_dvfs_both_set)(const enum mtk_dpc_subsys subsys, const u8 level, bool force,
		const u32 bw_in_mb);
};

#endif
