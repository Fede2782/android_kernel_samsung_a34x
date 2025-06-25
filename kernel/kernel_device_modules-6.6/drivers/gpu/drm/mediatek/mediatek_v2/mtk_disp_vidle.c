// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#include "mtk_disp_vidle.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "platform/mtk_drm_platform.h"
#include <mt-plat/mtk_irq_mon.h>

static atomic_t g_ff_enabled = ATOMIC_INIT(0);
static bool vidle_paused;

static struct mtk_disp_vidle_para mtk_disp_vidle_flag = {
	0,	/* vidle_en */
	0,	/* vidle_init */
	0,	/* vidle_stop */
	0,	/* rc_en */
	0,	/* wdt_en */
};

static struct dpc_funcs disp_dpc_driver;
struct mtk_vdisp_funcs vdisp_func;

static atomic_t g_vidle_pq_ref = ATOMIC_INIT(0);
static DEFINE_MUTEX(g_vidle_pq_ref_lock);
static DECLARE_COMPLETION(dpc_registered);

struct mtk_disp_vidle {
	u8 level;
	u32 hrt_bw;
	u32 srt_bw;
	u32 te_duration;
	u32 vb_duration;
	s8 dpc_hw_mode;
	enum mtk_panel_type panel_type;
	struct mtk_drm_private *drm_priv;
	u32 channel_bw;
	enum mtk_dpc_version dpc_version;
	enum mtk_vdisp_version vdisp_version;
	int pm_ret_crtc;	/* for DISP_VIDLE_USER_CRTC only, already locked by crtc_lock */
	int pm_ret_pq;		/* for DISP_VIDLE_USER_PQ only, protected by pq ref cnt */
};

static struct mtk_disp_vidle vidle_data = {
	.level = U8_MAX,
	.hrt_bw = U32_MAX,
	.srt_bw = U32_MAX,
	.te_duration = U32_MAX,
	.vb_duration = U32_MAX,
	.dpc_hw_mode = -1,
	.panel_type = PANEL_TYPE_COUNT,
	.drm_priv = NULL,
	.channel_bw = U32_MAX,
	.dpc_version = DPC_VER_CNT,
	.vdisp_version = VDISP_VER_CNT,
	.pm_ret_crtc = 0,
};

void mtk_vidle_flag_init(void *_crtc)
{
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_crtc *crtc = NULL;
	enum mtk_panel_type type = PANEL_TYPE_COUNT;

	mtk_disp_vidle_flag.vidle_en = 0;

	if (_crtc == NULL)
		return;
	crtc = (struct drm_crtc *)_crtc;

	if (drm_crtc_index(crtc) != 0)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	priv = crtc->dev->dev_private;
	if (priv == NULL || output_comp == NULL)
		return;
	vidle_data.drm_priv = priv;

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_TOP_EN))
		mtk_disp_vidle_flag.vidle_en |= DISP_VIDLE_TOP_EN;

	if (!mtk_drm_lcm_is_connect(mtk_crtc))
		DDPMSG("%s, lcm is not connected\n", __func__);
	else if (mtk_dsi_is_cmd_mode(output_comp))
		type = PANEL_TYPE_CMD;
	else if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_FULL_SCENARIO) ||
		mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_VDO_PANEL))
		type = PANEL_TYPE_VDO;
	else
		DDPMSG("%s, invalid panel type:%d\n", __func__, type);

	mtk_vidle_set_panel_type(type);

	if (mtk_vidle_update_dt_by_type(crtc, type) < 0) {
		// mtk_disp_vidle_flag.vidle_en = 0;
		DDPMSG("%s panel:%d te duration is not set, disable vidle\n", __func__, type);
		return;
	}
}

static unsigned int mtk_vidle_check(unsigned int vidle_item)
{
	return mtk_disp_vidle_flag.vidle_en & vidle_item;
}

static void mtk_vidle_dt_enable(unsigned int en)
{
	if (disp_dpc_driver.dpc_group_enable == NULL)
		return;

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MTCMOS,
		(en && mtk_vidle_check(DISP_VIDLE_MTCMOS_DT_EN)));
	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MTCMOS_DISP1,
		(en && mtk_vidle_check(DISP_VIDLE_MTCMOS_DT_EN)));

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MMINFRA_OFF,
		(en && mtk_vidle_check(DISP_VIDLE_MMINFRA_DT_EN)));
	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_INFRA_OFF,
		(en && mtk_vidle_check(DISP_VIDLE_MMINFRA_DT_EN)));

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_VDISP_DVFS,
		(en && mtk_vidle_check(DISP_VIDLE_DVFS_DT_EN)));

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_HRT_BW,
		(en && mtk_vidle_check(DISP_VIDLE_QOS_DT_EN)));
	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_SRT_BW,
		(en && mtk_vidle_check(DISP_VIDLE_QOS_DT_EN)));
}

void mtk_vidle_clear_wfe_event(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt, int event)
{
	if (disp_dpc_driver.dpc_clear_wfe_event == NULL)
		return;

	disp_dpc_driver.dpc_clear_wfe_event(pkt, user, event);
}

void mtk_vidle_user_power_keep_by_gce(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt, u16 gpr)
{
	if (disp_dpc_driver.dpc_vidle_power_keep_by_gce == NULL)
		return;

	disp_dpc_driver.dpc_vidle_power_keep_by_gce(pkt, user, gpr, NULL);
}

void mtk_vidle_user_power_release_by_gce(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt)
{
	if (disp_dpc_driver.dpc_vidle_power_release_by_gce == NULL)
		return;

	disp_dpc_driver.dpc_vidle_power_release_by_gce(pkt, user);
}

int mtk_vidle_force_power_ctrl_by_cpu(bool power_on)
{
	int ret = 0;

	if (disp_dpc_driver.dpc_vidle_power_keep == NULL ||
		disp_dpc_driver.dpc_vidle_power_release == NULL) {
		DDPMSG("%s power_on[%d] power ctrl api is null\n", __func__, power_on);
		return -1;
	}

	if (power_on)
		ret = disp_dpc_driver.dpc_vidle_power_keep(DISP_VIDLE_FORCE_KEEP);
	else
		disp_dpc_driver.dpc_vidle_power_release(DISP_VIDLE_FORCE_KEEP);

	return ret;
}

int mtk_vidle_user_power_keep_v1(enum mtk_vidle_voter_user user)
{
	if (disp_dpc_driver.dpc_vidle_power_keep == NULL || vidle_data.drm_priv == NULL)
		return 0;

	if (atomic_read(&vidle_data.drm_priv->kernel_pm.status) == KERNEL_SHUTDOWN)
		return -1;
	else if (atomic_read(&vidle_data.drm_priv->kernel_pm.wakelock_cnt) == 0)
		user |= VOTER_ONLY;

	return disp_dpc_driver.dpc_vidle_power_keep(user);
}

void mtk_vidle_user_power_release_v1(enum mtk_vidle_voter_user user)
{
	if (disp_dpc_driver.dpc_vidle_power_release == NULL || vidle_data.drm_priv == NULL)
		return;

	irq_log_store();
	if (atomic_read(&vidle_data.drm_priv->kernel_pm.status) == KERNEL_SHUTDOWN)
		return;
	else if (atomic_read(&vidle_data.drm_priv->kernel_pm.wakelock_cnt) == 0)
		user |= VOTER_ONLY;

	irq_log_store();
	disp_dpc_driver.dpc_vidle_power_release(user);
}

int mtk_vidle_user_power_keep_v2(enum mtk_vidle_voter_user user)
{
	int pm_ret = 0;

	if (disp_dpc_driver.dpc_vidle_power_keep == NULL || vidle_data.drm_priv == NULL)
		return 0;

	if (atomic_read(&vidle_data.drm_priv->kernel_pm.status) == KERNEL_SHUTDOWN)
		pm_ret = -1;
	else if (atomic_read(&vidle_data.drm_priv->kernel_pm.wakelock_cnt) == 0)
		pm_ret = disp_dpc_driver.dpc_vidle_power_keep(user | VOTER_ONLY);
	else
		pm_ret = disp_dpc_driver.dpc_vidle_power_keep(user);

	if (user == DISP_VIDLE_USER_CRTC)
		vidle_data.pm_ret_crtc = pm_ret;
	else if (user == DISP_VIDLE_USER_PQ)
		vidle_data.pm_ret_pq = pm_ret;

	return pm_ret;
}

void mtk_vidle_user_power_release_v2(enum mtk_vidle_voter_user user)
{
	if (disp_dpc_driver.dpc_vidle_power_release == NULL || vidle_data.drm_priv == NULL)
		return;

	if (atomic_read(&vidle_data.drm_priv->kernel_pm.status) == KERNEL_SHUTDOWN)
		return;

	if ((user == DISP_VIDLE_USER_CRTC && vidle_data.pm_ret_crtc == VOTER_ONLY) ||
	    (user == DISP_VIDLE_USER_PQ && vidle_data.pm_ret_pq == VOTER_ONLY))
		user |= VOTER_ONLY;

	disp_dpc_driver.dpc_vidle_power_release(user);
}

int mtk_vidle_user_power_keep(enum mtk_vidle_voter_user user)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_vidle_user_power_keep_v1(user);
	else
		return mtk_vidle_user_power_keep_v2(user);
}

void mtk_vidle_user_power_release(enum mtk_vidle_voter_user user)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_vidle_user_power_release_v1(user);
	else
		return mtk_vidle_user_power_release_v2(user);
}

int mtk_vidle_pq_power_get(const char *caller)
{
	s32 ref;
	int pm_ret = 0;

	mutex_lock(&g_vidle_pq_ref_lock);
	ref = atomic_inc_return(&g_vidle_pq_ref);
	if (ref == 1) {
		pm_ret = mtk_vidle_user_power_keep(DISP_VIDLE_USER_PQ);
		if (pm_ret < 0)
			ref = atomic_dec_return(&g_vidle_pq_ref);
	}
	mutex_unlock(&g_vidle_pq_ref_lock);
	if (ref < 0) {
		DDPPR_ERR("%s  get invalid cnt %d\n", caller, ref);
		return ref;
	}
	return pm_ret;
}

void mtk_vidle_pq_power_put(const char *caller)
{
	s32 ref;

	mutex_lock(&g_vidle_pq_ref_lock);
	ref = atomic_dec_return(&g_vidle_pq_ref);
	if (!ref)
		mtk_vidle_user_power_release(DISP_VIDLE_USER_PQ);
	mutex_unlock(&g_vidle_pq_ref_lock);
	if (ref < 0)
		DDPPR_ERR("%s  put invalid cnt %d\n", caller, ref);
}

void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en)
{
	mtk_disp_vidle_flag.rc_en = rc_en;
	/* TODO: action for mmdvfsrc_status_rc */
}

void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en)
{
	mtk_disp_vidle_flag.wdt_en = wdt_en;
	/* TODO: action for mmdvfsrc_status_wdt */
}

/* for debug only, DONT use in flow */
void mtk_vidle_set_all_flag(unsigned int en, unsigned int stop)
{
	mtk_disp_vidle_flag.vidle_en = en;
	mtk_disp_vidle_flag.vidle_stop = stop;
}

void mtk_vidle_get_all_flag(unsigned int *en, unsigned int *stop)
{
	*en = mtk_disp_vidle_flag.vidle_en;
	*stop = mtk_disp_vidle_flag.vidle_stop;
}

static void mtk_vidle_pause(bool en)
{
	if (disp_dpc_driver.dpc_pause == NULL)
		return;

	if (!mtk_vidle_is_ff_enabled())
		return;

	if (en && !vidle_paused) {
		CRTC_MMP_EVENT_START(0, pause_vidle, mtk_disp_vidle_flag.vidle_stop,
				vidle_data.te_duration);
		disp_dpc_driver.dpc_pause(DPC_SUBSYS_DISP, en);
		vidle_paused = true;
	} else if (!en && vidle_paused) {
		disp_dpc_driver.dpc_pause(DPC_SUBSYS_DISP, en);
		CRTC_MMP_EVENT_END(0, pause_vidle, mtk_disp_vidle_flag.vidle_stop,
				vidle_data.te_duration);
		vidle_paused = false;
	}
}

static void mtk_set_vidle_stop_flag_v1(unsigned int flag, unsigned int stop)
{
	unsigned int origignal_flag = mtk_disp_vidle_flag.vidle_stop;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	if (stop)
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop | flag;
	else
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop & ~flag;

	if (origignal_flag != mtk_disp_vidle_flag.vidle_stop)
		CRTC_MMP_MARK(0, pause_vidle, mtk_disp_vidle_flag.vidle_stop,
				mtk_vidle_is_ff_enabled());

	if (mtk_disp_vidle_flag.vidle_stop)
		mtk_vidle_pause(true);
	else
		mtk_vidle_pause(false);
}

static void mtk_vidle_stop(void)
{
	// mtk_vidle_power_keep();
	mtk_vidle_dt_enable(0);
	/* TODO: stop timestamp */
}

static void mtk_set_vidle_stop_flag_v2(unsigned int flag, unsigned int stop)
{
	if (stop)
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop | flag;
	else
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop & ~flag;

	if (mtk_disp_vidle_flag.vidle_stop)
		mtk_vidle_stop();
}

void mtk_set_vidle_stop_flag(unsigned int flag, unsigned int stop)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_set_vidle_stop_flag_v1(flag, stop);
	else
		return mtk_set_vidle_stop_flag_v2(flag, stop);
}

static int mtk_set_dt_configure_all(unsigned int dur_frame, unsigned int dur_vblank)
{
	if (disp_dpc_driver.dpc_dt_set_all)
		return disp_dpc_driver.dpc_dt_set_all(dur_frame, dur_vblank);

	return 0;
}

int mtk_vidle_update_dt_v1_by_type(void *_crtc, enum mtk_panel_type type)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_panel_params *panel_ext = NULL;
	unsigned int dur_frame = 0, dur_vblank = 0, fps = 0;
	struct mtk_drm_private *priv = NULL;
	int ret = 0;

	if (!mtk_disp_vidle_flag.vidle_en)
		return -1;

	if (_crtc == NULL || !disp_dpc_driver.dpc_dt_set_all)
		return -1;

	crtc = (struct drm_crtc *)_crtc;
	if (drm_crtc_index(crtc) != 0)
		return -1;

	priv = crtc->dev->dev_private;
	if (priv == NULL) {
		DDPMSG("%s, invalid priv\n", __func__);
		return -1;
	}

	panel_ext = mtk_drm_get_lcm_ext_params(crtc);

	if (panel_ext && panel_ext->real_te_duration &&
		type == PANEL_TYPE_CMD) {
		dur_frame = panel_ext->real_te_duration;
		dur_vblank = 0;
	} else if (type == PANEL_TYPE_VDO &&
		mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_VDO_PANEL)) {
		fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (comp == NULL) {
			DDPMSG("%s, invalid output comp\n", __func__);
			return -2;
		}

		dur_frame = (fps == 0) ? 16666 : 1000000 / fps;
		mtk_ddp_comp_io_cmd(comp, NULL,
				DSI_GET_PANEL_VBLANK_PERIOD_US, &dur_vblank);
	}

	if (dur_frame == 0) {
		DDPINFO("%s, panel:%d, invalid dur_frame:%uus\n", __func__, type, dur_frame);
		return -1;
	}
	if (dur_frame == vidle_data.te_duration &&
		dur_vblank == vidle_data.vb_duration)
		return dur_frame;

	/* update DTs affected by TE dur_frame */
	ret = mtk_set_dt_configure_all(dur_frame, dur_vblank);
	vidle_data.te_duration = dur_frame;
	vidle_data.vb_duration = dur_vblank;

	if (ret < 0 &&
		!(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 1);
		DDPINFO("%s forbid vidle mode, panel:%d, dur_frame:%u, dur_vb:%uus, flag:0x%x\n",
			__func__, type, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	} else if (ret >= 0 &&
		(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 0);
		DDPINFO("%s allow vidle mode, panel:%d, dur_frame:%u, dur_vb:%uus, flag:0x%x\n",
			__func__, type, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	}

	return dur_frame;
}

static int mtk_vidle_update_dt_v2_by_period(unsigned int duration)
{
	if (!disp_dpc_driver.dpc_duration_update)
		return -1;

	if (duration == vidle_data.te_duration)
		return duration;
	else if (duration > vidle_data.te_duration)
		mtk_vidle_config_ff(false);

	disp_dpc_driver.dpc_duration_update(duration);

	DDPMSG("%s %d -> %d\n", __func__, vidle_data.te_duration, duration);
	vidle_data.te_duration = duration;

	return duration;
}

int mtk_vidle_update_dt_v2(void *_crtc)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_panel_params *panel_ext = NULL;
	unsigned int duration = 0;
	unsigned int fps = 0;

	if (!mtk_disp_vidle_flag.vidle_en) {
		DDPMSG("%s, %d\n", __func__, __LINE__);
		return -1;
	}

	if (_crtc == NULL) {
		DDPMSG("%s, %d\n", __func__, __LINE__);
		return -1;
	}

	crtc = (struct drm_crtc *)_crtc;
	if (drm_crtc_index(crtc) != 0) {
		DDPMSG("%s, %d\n", __func__, __LINE__);
		return -1;
	}

	panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	if (panel_ext && panel_ext->real_te_duration && panel_ext->real_te_duration != 0)
		duration = panel_ext->real_te_duration;
	else if (crtc->state) {
		fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
		duration = (fps == 0) ? 0 : (1000000 / fps);
	}

	if (duration == 0) {
		DDPMSG("duration is not set\n");
		return -1;
	}

	return mtk_vidle_update_dt_v2_by_period(duration);
}

int mtk_vidle_update_dt_by_type(void *_crtc, enum mtk_panel_type type)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_vidle_update_dt_v1_by_type(_crtc, type);
	else
		return mtk_vidle_update_dt_v2(_crtc);
}

int mtk_vidle_update_dt_v1_by_period(void *_crtc, unsigned int dur_frame, unsigned int dur_vblank)
{
	struct drm_crtc *crtc = NULL;
	int ret = 0;

	if (!mtk_disp_vidle_flag.vidle_en) {
		DDPMSG("%s, invalid vidle_en:0x%x\n", __func__,
			mtk_disp_vidle_flag.vidle_en);
		return -1;
	}

	if (_crtc == NULL || dur_frame == 0) {
		DDPMSG("%s, invalid crtc, dur_frame:%uus\n", __func__, dur_frame);
		return -1;
	}

	crtc = (struct drm_crtc *)_crtc;
	if (drm_crtc_index(crtc) != 0) {
		DDPMSG("%s, invalid crtc id:%d\n", __func__,
			drm_crtc_index(crtc));
		return -1;
	}

	if (dur_frame == vidle_data.te_duration &&
		dur_vblank == vidle_data.vb_duration)
		return dur_frame;

	/* update DTs affected by TE dur_frame */
	ret = mtk_set_dt_configure_all(dur_frame, dur_vblank);
	vidle_data.te_duration = dur_frame;
	vidle_data.vb_duration = dur_vblank;

	if (ret < 0 &&
		!(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 1);
		DDPINFO("%s forbid vidle mode, dur_frame:%uus, dur_vb:%uus, flag:0x%x\n",
			__func__, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	} else if (ret >= 0 &&
		(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 0);
		DDPINFO("%s allow vidle mode, dur_frame:%uus, dur_vb:%uus, flag:0x%x\n",
			__func__, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	}

	return dur_frame;
}

int mtk_vidle_update_dt_by_period(void *_crtc, unsigned int dur_frame, unsigned int dur_vblank)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_vidle_update_dt_v1_by_period(_crtc, dur_frame, dur_vblank);
	else
		return mtk_vidle_update_dt_v2_by_period(dur_frame);

	return 0;
}

bool mtk_vidle_is_ff_enabled(void)
{
	return (bool)atomic_read(&g_ff_enabled);
}

void mtk_vidle_force_enable_mml_v1(bool en)
{
	if (!disp_dpc_driver.dpc_dc_force_enable)
		return;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	/* some case, like multi crtc we need to stop V-idle */
	if (mtk_disp_vidle_flag.vidle_stop)
		return;

	disp_dpc_driver.dpc_dc_force_enable(en);
}

void mtk_vidle_force_enable_mml(bool en)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_vidle_force_enable_mml_v1(en);
}

static void mtk_vidle_enable_v1(bool en, void *_drm_priv)
{
	static bool status;

	if (!disp_dpc_driver.dpc_enable)
		return;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	/* some case, like multi crtc we need to stop V-idle */
	if (mtk_disp_vidle_flag.vidle_stop && en)
		return;

	if (en == status)
		return;

	disp_dpc_driver.dpc_enable(en);
	if (!en && vidle_paused) {
		CRTC_MMP_EVENT_END(0, pause_vidle,
				mtk_disp_vidle_flag.vidle_stop, 0xFFFFFFFF);
		vidle_paused = false;
	}
	/* TODO: enable timestamp */

	status = en;
	DDPINFO("%s, en:%d, stop:0x%x, pause:%d, status:%d\n", __func__, en,
		mtk_disp_vidle_flag.vidle_stop, vidle_paused, status);
}

static void mtk_vidle_enable_v2(bool _en, void *_drm_priv)
{
	u8 en = 0;
	struct mtk_drm_private *drm_priv = NULL;

	if (!disp_dpc_driver.dpc_enable)
		return;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	if (unlikely(!_drm_priv))
		return;
	drm_priv = _drm_priv;

	if (_en)
		en = mtk_crtc_is_frame_trigger_mode(drm_priv->crtc[0]) ? 1 : 2;

	disp_dpc_driver.dpc_enable(en);
}

void mtk_vidle_enable(bool _en, void *_drm_priv)
{
	if (vidle_data.dpc_version == DPC_VER1)
		return mtk_vidle_enable_v1(_en, _drm_priv);
	else
		return mtk_vidle_enable_v2(_en, _drm_priv);
}

void mtk_vidle_dvfs_trigger(const char *caller)
{
	if (disp_dpc_driver.dpc_dvfs_trigger)
		disp_dpc_driver.dpc_dvfs_trigger(caller);
}

void mtk_vidle_set_panel_type(enum mtk_panel_type type)
{
	vidle_data.panel_type = type;

	if (!disp_dpc_driver.dpc_init_panel_type)
		return;
	disp_dpc_driver.dpc_init_panel_type(type);
}

void mtk_vidle_hrt_bw_set(const u32 bw_in_mb)
{
	vidle_data.hrt_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_hrt_bw_set) {
		if (vidle_data.dpc_version == DPC_VER1)
			disp_dpc_driver.dpc_hrt_bw_set(DPC_SUBSYS_DISP, bw_in_mb, !atomic_read(&g_ff_enabled));
		else
			disp_dpc_driver.dpc_hrt_bw_set(DPC_SUBSYS_DISP, bw_in_mb, true);
	} else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);

}
void mtk_vidle_srt_bw_set(const u32 bw_in_mb)
{
	vidle_data.srt_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_srt_bw_set) {
		if (vidle_data.dpc_version == DPC_VER1)
			disp_dpc_driver.dpc_srt_bw_set(DPC_SUBSYS_DISP, bw_in_mb, !atomic_read(&g_ff_enabled));
		else
			disp_dpc_driver.dpc_srt_bw_set(DPC_SUBSYS_DISP, bw_in_mb, true);
	} else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);
}
void mtk_vidle_dvfs_set(const u8 level)
{
	vidle_data.level = level;
	if (disp_dpc_driver.dpc_dvfs_set)
		disp_dpc_driver.dpc_dvfs_set(DPC_SUBSYS_DISP, level, true);
	else
		DDPINFO("%s NOT SET:%d\n", __func__, level);
}
void mtk_vidle_dvfs_bw_set(const u32 bw_in_mb)
{
	if (disp_dpc_driver.dpc_dvfs_bw_set)
		disp_dpc_driver.dpc_dvfs_bw_set(DPC_SUBSYS_DISP, bw_in_mb);
}
void mtk_vidle_channel_bw_set(const u32 bw_in_mb, const u32 idx)
{
	vidle_data.channel_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_channel_bw_set_by_idx)
		disp_dpc_driver.dpc_channel_bw_set_by_idx(DPC_SUBSYS_DISP, idx, bw_in_mb);
	else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);

}

void mtk_vidle_config_ff(bool en)
{
	int ret = 0;

	if (!disp_dpc_driver.dpc_config)
		return;

	if (en && !mtk_disp_vidle_flag.vidle_en)
		return;

	ret = disp_dpc_driver.dpc_config(DPC_SUBSYS_DISP, en);

	if (ret == 0)
		atomic_set(&g_ff_enabled, en);
}

void mtk_vidle_dpc_analysis(void)
{
	if (disp_dpc_driver.dpc_analysis)
		disp_dpc_driver.dpc_analysis();
}

void mtk_vidle_dpc_pm_analysis(void)
{
	if (disp_dpc_driver.dpc_pm_analysis)
		disp_dpc_driver.dpc_pm_analysis();
}

void mtk_vidle_debug_cmd_adapter(const char *opt)
{
	if (disp_dpc_driver.dpc_debug_cmd)
		disp_dpc_driver.dpc_debug_cmd(opt);
}

void mtk_vidle_wait_init(void)
{
	DDPFUNC("wait_for_completion +");
	wait_for_completion_interruptible_timeout(&dpc_registered, msecs_to_jiffies(5000));
	DDPFUNC("wait_for_completion -");
}

void mtk_vidle_dsi_pll_set(const u32 value)
{
	if (disp_dpc_driver.dpc_dsi_pll_set)
		disp_dpc_driver.dpc_dsi_pll_set(value);
}

void mtk_vidle_register_v1(const struct dpc_funcs *funcs)
{
	int ret = 0;

	if (vidle_data.panel_type != PANEL_TYPE_COUNT) {
		DDPINFO("%s set panel:%d\n", __func__, vidle_data.panel_type);
		mtk_vidle_set_panel_type(vidle_data.panel_type);
	}

	if(vidle_data.te_duration != U32_MAX ||
		vidle_data.vb_duration != U32_MAX) {
		ret = mtk_set_dt_configure_all(vidle_data.te_duration, vidle_data.vb_duration);
		DDPINFO("%s set duration:%u, ret:%d\n",
			__func__, vidle_data.te_duration, ret);
	}

	if(vidle_data.level != U8_MAX) {
		DDPINFO("%s need set level:%d\n", __func__, vidle_data.level);
		mtk_vidle_dvfs_set(vidle_data.level);
	}

	if(vidle_data.hrt_bw != U32_MAX) {
		DDPINFO("%s need set hrt bw:%d\n", __func__, vidle_data.hrt_bw);
		mtk_vidle_dvfs_set(vidle_data.hrt_bw);
	}

	if(vidle_data.srt_bw != U32_MAX) {
		DDPINFO("%s need set srt bw:%d\n", __func__, vidle_data.srt_bw);
		mtk_vidle_dvfs_set(vidle_data.srt_bw);
	}
}

void mtk_vidle_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version)
{
	DDPMSG("%s, panel:%d,version:%u,l:0x%x,hrt:0x%x,srt:0x%x\n",
		__func__, vidle_data.panel_type, version,
		vidle_data.level, vidle_data.hrt_bw, vidle_data.srt_bw);
	vidle_data.dpc_version = version;
	disp_dpc_driver = *funcs;

	if(version == DPC_VER1)
		mtk_vidle_register_v1(funcs);

	complete(&dpc_registered);
}
EXPORT_SYMBOL(mtk_vidle_register);

void mtk_vdisp_register(const struct mtk_vdisp_funcs *fp, enum mtk_vdisp_version version)
{
	vidle_data.vdisp_version = version;
	vdisp_func = *fp;
}
EXPORT_SYMBOL(mtk_vdisp_register);

int mtk_vidle_get_power_if_in_use(void)
{
	int ret = 0;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return 1;

	/* all disp subsys power on: return 1 */
	ret = mtk_drm_pm_ctrl(vidle_data.drm_priv, DISP_PM_CHECK);
	if (ret == 0)
		return 1;

	/* any disp subsys power off: return 0 */
	DDPMSG("%s, any disp mtcmos power off, ret:%d\n", __func__, ret);
	return 0;
}
EXPORT_SYMBOL(mtk_vidle_get_power_if_in_use);

void mtk_vidle_put_power(void)
{
	mtk_drm_pm_ctrl(vidle_data.drm_priv, DISP_PM_PUT);
}
EXPORT_SYMBOL(mtk_vidle_put_power);
