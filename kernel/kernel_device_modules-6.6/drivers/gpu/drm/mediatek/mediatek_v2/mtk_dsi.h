/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DSI_H__
#define __MTK_DSI_H__

#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/clk.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <video/videomode.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_panel_ext.h"
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

struct t_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

enum DSI_N_Version {
	VER_N12 = 0,
	VER_N7,
	VER_N6,
	VER_N5,
	VER_N4,
	VER_N3,
};

struct mtk_dsi_driver_data {
	const u32 reg_cmdq0_ofs;
	const u32 reg_cmdq1_ofs;
	const u32 reg_vm_cmd_con_ofs;
	const u32 reg_vm_cmd_data0_ofs;
	const u32 reg_vm_cmd_data10_ofs;
	const u32 reg_vm_cmd_data20_ofs;
	const u32 reg_vm_cmd_data30_ofs;
	const u32 reg_dsi_input_dbg_ofs;
	s32 (*poll_for_idle)(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	char *esd_eint_compat;
	bool support_shadow;
	bool need_bypass_shadow;
	bool need_wait_fifo;
	bool new_rst_dsi;
	const u32 buffer_unit;
	const u32 sram_unit;
	const u32 urgent_lo_fifo_us;
	const u32 urgent_hi_fifo_us;
	const u32 output_valid_fifo_us;
	bool dsi_buffer;
	bool smi_dbg_disable;
	bool require_phy_reset; /* reset phy before trigger DSI */
	bool keep_hs_eotp; /* keep HS eotp */
	bool support_pre_urgent;
	u32 max_vfp;
	void (*mmclk_by_datarate)(struct mtk_dsi *dsi,
		struct mtk_drm_crtc *mtk_crtc, unsigned int en);
	const unsigned int bubble_rate;
	const enum DSI_N_Version n_verion;
	const u32 reg_phy_base;
	const u32 reg_20_ofs;
	const u32 reg_30_ofs;
	const u32 reg_40_ofs;
	const u32 reg_100_ofs;
	const u32 dsi_size_con;
	const u32 dsi_vfp_early_stop;
	const u32 dsi_lfr_con;
	const u32 dsi_cmdq_con;
	const u32 dsi_type1_hs;
	const u32 dsi_hstx_ckl_wc;
	const u32 dsi_mem_conti;
	const u32 dsi_time_con;
	const u32 dsi_reserved;
	const u32 dsi_state_dbg6;
	const u32 dsi_dbg_sel;
	const u32 dsi_shadow_dbg;
	const u32 dsi_scramble_con;
	const u32 dsi_target_nl;
	const u32 dsi_buf_con_base;
	const u32 dsi_phy_syncon;
	//vdo ltpo
	const u32 dsi_ltpo_vdo_con;
	const u32 dsi_ltpo_vdo_sq0;
	bool support_512byte_rx;
	bool support_bl_at_te;
	const u32 dsi_rx_trig_sta;
	const u32 dsi_rx_con;
};

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;
	struct cmdq_pkt_buffer cmdq_buf;
	struct drm_bridge *bridge;
	struct phy *phy;
	bool is_slave;
	struct mtk_dsi *slave_dsi;
	struct mtk_dsi *master_dsi;
	struct mtk_drm_connector_caps connector_caps;
	uint32_t connector_caps_blob_id;
#if IS_ENABLED(CONFIG_ENABLE_DSI_HOTPLUG)
	struct task_struct *hotplug_task;
#endif

	void __iomem *regs;

	struct clk *engine_clk;
	struct clk *digital_clk;
	struct clk *hs_clk;

	u32 data_rate;
	u32 data_rate_khz;
	u32 d_rate;
	u32 ulps_wakeup_prd;
	u32 bdg_data_rate;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int clk_refcnt;
	bool output_en;
	bool doze_enabled;
	u32 irq_data;
	wait_queue_head_t irq_wait_queue;
	struct mtk_dsi_driver_data *driver_data;

	struct t_condition_wq enter_ulps_done;
	struct t_condition_wq exit_ulps_done;
	struct t_condition_wq te_rdy;
	struct t_condition_wq frame_done;
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int cont_det;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int hsa_byte;
	unsigned int hbp_byte;
	unsigned int hfp_byte;

#if IS_ENABLED(CONFIG_SMCDSD_PANEL)
	wait_queue_head_t framedone_wait;
	ktime_t framedone_timestamp;
	struct task_struct *framedone_thread;
	bool need_framedone_notify;
#endif

	/* for 6382 mipi hopping */
	bool bdg_mipi_hopping_sta;
	bool mipi_hopping_sta;
	bool panel_osc_hopping_sta;
	unsigned int data_phy_cycle;
	/* for Panel Master dcs read/write */
	struct mipi_dsi_device *dev_for_PM;
	atomic_t ulps_async;
	bool pending_switch;
	struct mtk_drm_esd_ctx *esd_ctx;
	unsigned int cnt;
	unsigned int skip_vblank;
	unsigned int force_resync_after_idle;
	unsigned int mode_switch_delay;
	unsigned int dummy_cmd_en;
	unsigned int set_partial_update;
	unsigned int roi_y_offset;
	unsigned int roi_height;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	enum drm_connector_status connect_status;
#endif
	atomic_t cmdq_option_enable;
	bool using_hs_transfer;
};

enum dsi_porch_type;

u16 mtk_get_gpr(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
s32 mtk_dsi_poll_for_idle(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
irqreturn_t mtk_dsi_irq_status(int irq, void *dev_id);
void mtk_dsi_set_mmclk_by_datarate_V1(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en);
void mtk_dsi_set_mmclk_by_datarate_V2(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en);
int mtk_dsi_get_virtual_width(struct mtk_dsi *dsi,
	struct drm_crtc *crtc);
int mtk_dsi_get_virtual_heigh(struct mtk_dsi *dsi,
	struct drm_crtc *crtc);
unsigned int mtk_dsi_default_rate(struct mtk_dsi *dsi);
struct mtk_panel_ext *mtk_dsi_get_panel_ext(struct mtk_ddp_comp *comp);
void mtk_disp_mutex_trigger(struct mtk_disp_mutex *mutex, void *handle);
void mtk_output_bdg_enable(struct mtk_dsi *dsi, int force_lcm_update);
unsigned int _dsi_get_pcw(unsigned long data_rate,
	unsigned int pcw_ratio);
int mtk_dsi_porch_setting(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum dsi_porch_type type, unsigned int value);
void mtk_dsi_porch_config(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
int mtk_drm_dummy_cmd_on_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
unsigned long long mtk_get_cur_backlight(struct drm_crtc *crtc);

#endif
