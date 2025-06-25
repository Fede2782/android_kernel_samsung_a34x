/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DRV_H
#define MTK_DRM_DRV_H

#include <drm/drm_fb_helper.h>
#include <uapi/drm/mediatek_drm.h>
#include <linux/types.h>
#include <linux/io.h>
#include <drm/drm_atomic.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_session.h"
#include "mtk_drm_helper.h"
#include "../mml/mtk-mml-drm-adaptor.h"
#include "mtk_notify.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#define MAX_CONNECTOR 7
#else
#define MAX_CONNECTOR 3
#endif
//#define CONFIG_MTK_DISP_NO_LK
//#define DRM_BYPASS_PQ
//#define DRM_OVL_SELF_PATTERN
//#define MTK_DSI1_SUPPORT_DSC1

#define MTK_DRM_FENCE_SUPPORT
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#define MTK_DRM_CMDQ_ASYNC
#define CONFIG_MTK_DISPLAY_CMDQ
#define MTK_FILL_MIPI_IMPEDANCE
#endif
#define MTK_DRM_ASYNC_HANDLE

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct drm_property;
struct regmap;

struct mtk_atomic_state {
	struct drm_atomic_state base;
	struct list_head list;
	struct kref kref;
};

struct mtk_fake_eng_reg {
	unsigned int CG_idx;
	unsigned int CG_bit;
	bool share_port;
};

struct mtk_fake_eng_data {
	int fake_eng_num;
	const struct mtk_fake_eng_reg *fake_eng_reg;
};

struct mtk_mmsys_driver_data {
	const struct mtk_crtc_path_data *main_path_data;
	const struct mtk_crtc_path_data *main_bypass_pc_path_data;
	const struct mtk_crtc_path_data *main_bypass_pc_dual_path_data;
	const struct mtk_crtc_path_data *main_dual_path_data;
	const struct mtk_crtc_path_data *ext_path_data;
	const struct mtk_crtc_path_data *ext_alter_path_data;
	const struct mtk_crtc_path_data *third_path_data;
	const struct mtk_crtc_path_data *third_path_data_wo_tdshp;
	const struct mtk_crtc_path_data *fourth_path_data_secondary;
	const struct mtk_crtc_path_data *fourth_path_data_discrete;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	const struct mtk_crtc_path_data *fifth_path_data;
	const struct mtk_crtc_path_data *sixth_path_data;
	const struct mtk_crtc_path_data *seventh_path_data;
#endif
	enum mtk_mmsys_id mmsys_id;
	bool shadow_register;
	const struct mtk_session_mode_tb *mode_tb;
	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			struct cmdq_pkt *handle, void *data);
	void (*sodi_apsrc_config)(struct drm_crtc *crtc,
			struct cmdq_pkt *_cmdq_handle, bool first_init, bool check_reset,
			unsigned int crtc_id, bool enable);
	const struct mtk_fake_eng_data *fake_eng_data;
	bool bypass_infra_ddr_control;
	bool use_infra_mem_res;
	bool has_smi_limitation;
	bool doze_ctrl_pmic;
	bool can_compress_rgb565;
	bool need_emi_eff;
	void (*disable_merge_irq)(struct drm_device *drm);
	void (*gce_event_config)(struct drm_device *drm);
	void (*vdisp_ao_irq_config)(struct drm_device *drm);
	enum PF_TS_TYPE pf_ts_type;
	bool respective_ostdl;
	bool ovl_exdma_rule;
	bool real_srt_ostdl;
	bool skip_trans;
	bool wb_skip_sec_buf;
	void (*update_channel_hrt)(struct mtk_drm_crtc *mtk_crtc,
			unsigned int bw_base, unsigned int channel_bw[]);
	unsigned int (*get_channel_idx)(enum CHANNEL_TYPE type, unsigned int i);
	void (*update_channel_bw_by_layer)(unsigned int layer, unsigned int bpp,
			unsigned int *subcomm_bw_sum, unsigned int size,
			unsigned int bw_base, enum CHANNEL_TYPE type);
	void (*update_channel_bw_by_larb)(struct mtk_larb_port_bw *port_bw,
			unsigned int *subcomm_bw_sum, unsigned int size, enum CHANNEL_TYPE type);
};

struct mtk_drm_lyeblob_ids {
	uint32_t lye_idx;
	uint32_t frame_weight;
	uint32_t frame_weight_of_bwm;
	uint32_t hrt_num;
	uint32_t disp_status;
	int32_t ddp_blob_id;
	int32_t ref_cnt;
	int32_t ref_cnt_mask;
	int32_t free_cnt_mask;
	int32_t lye_plane_blob_id[MAX_CRTC][OVL_LAYER_NR];
	int fbt_gles_head;
	int fbt_gles_tail;
	int fbt_layer_id;
	bool hrt_valid;
	struct list_head list;
};

enum disp_hrt_usage {
	DISP_DISABLE,
	DISP_ENABLE,
	DISP_OPENING,
};

enum MTK_CONNECTOR_PROP {
	CONNECTOR_PROP_CAPS_BLOB_ID,
	CONNECTOR_PROP_CSC_BL,        /* csc bylayer backlight */
	CONNECTOR_PROP_PANEL_NITS,    /* panel nits */
	CONNECTOR_PROP_MAX,
};

struct mtk_connector_state {
	struct drm_connector_state base;

	/* property */
	uint64_t prop_val[MAX_CONNECTOR][CONNECTOR_PROP_MAX];
};

#define to_mtk_connector_state(x) container_of(x, struct mtk_connector_state, base)

enum drm_kernel_pm_status {
	KERNEL_PM_SUSPEND,
	KERNEL_PM_RESUME,
	KERNEL_SHUTDOWN,
};
struct mtk_drm_kernel_pm {
	bool shutdown;
	struct notifier_block nb;	/* Kernel suspend and resume event */
	struct mutex lock;		/* To block any request after kernel suspend */
	atomic_t status;
	atomic_t wakelock_cnt;
	wait_queue_head_t wq;
};

struct lateinit_task {
	struct kthread_worker *worker;
	struct kthread_work work;
};

#define A0_CHIP	0
#define B0_CHIP	1

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

struct mtk_drm_private {
	struct drm_device *drm;
	struct device *dma_dev;
	struct device *mmsys_dev;
	struct device *side_mmsys_dev;
	struct device *ovlsys_dev;
	struct device *side_ovlsys_dev;
	struct device *dpc_dev;
	struct device *dsi_phy0_dev;
	struct device *dsi_phy1_dev;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	struct device *dsi_phy2_dev;
#endif

	struct drm_crtc *crtc[MAX_CRTC];
	unsigned int pre_defined_bw[MAX_CRTC];
	enum disp_hrt_usage usage[MAX_CRTC];
	unsigned int ovl_usage[MAX_CRTC]; //describe each CRTC OVL comp usage state
	unsigned int ovlsys_usage[MAX_CRTC]; //describe each CRTC OVLSYS connect state
	unsigned int req_hrt[MAX_CRTC];
	unsigned int req_hrt_channel_bw[MAX_CRTC][BW_CHANNEL_NR];
	unsigned int last_max_channel_req;
	unsigned int num_pipes;

	unsigned int sw_ver;

	unsigned int session_id[MAX_SESSION_COUNT];
	unsigned int num_sessions;
	enum MTK_DRM_SESSION_MODE session_mode;
	atomic_t crtc_present[MAX_CRTC];
	atomic_t crtc_sf_present[MAX_CRTC];
	atomic_t crtc_rel_present[MAX_CRTC];
	ktime_t crtc_last_present_ts[MAX_CRTC];
	ktime_t crtc_rel_present_ts[MAX_CRTC];

	struct device_node *mutex_node;
	struct device *mutex_dev;
	unsigned int dispsys_num;
	unsigned int ovlsys_num;
	unsigned int pq_path_sel;
	void __iomem *config_regs;
	resource_size_t config_regs_pa;
	void __iomem *side_config_regs;
	resource_size_t side_config_regs_pa;
	void __iomem *ovlsys0_regs;
	resource_size_t ovlsys0_regs_pa;
	void __iomem *ovlsys1_regs;
	resource_size_t ovlsys1_regs_pa;
	void __iomem *infra_regs;
	resource_size_t infra_regs_pa;
	const struct mtk_mmsys_reg_data *reg_data;
	struct device_node *comp_node[DDP_COMPONENT_ID_MAX];
	struct mtk_ddp_comp *ddp_comp[DDP_COMPONENT_ID_MAX];
	const struct mtk_mmsys_driver_data *data;

	struct {
		struct drm_atomic_state *state;
		struct work_struct work;
		struct mutex lock;
	} commit;

	struct drm_atomic_state *suspend_state;

	struct {
		struct work_struct work;
		struct list_head list;
		spinlock_t lock;
	} unreference;

	struct lateinit_task lateinit;

	/* property */
	struct drm_property *crtc_property[MAX_CRTC][CRTC_PROP_MAX];
	struct drm_property *connector_property[MAX_CONNECTOR][CONNECTOR_PROP_MAX];
	struct mtk_ddp_fb_info fb_info;

	struct list_head lyeblob_head;
	struct mutex lyeblob_list_mutex;
	struct task_struct *fence_release_thread[MAX_CRTC-1];

	/* variable for repaint */
	struct {
		wait_queue_head_t wq;
		struct list_head job_queue;
		struct list_head job_pool;
	} repaint_data;

	struct mtk_drm_helper *helper_opt;

	atomic_t idle_need_repaint;
	atomic_t rollback_all;

	unsigned int top_clk_num;
	struct clk **top_clk;
	bool power_state;
	struct mtk_drm_kernel_pm kernel_pm;

	/* for rpo caps info */
	unsigned int rsz_in_max[2];

	struct icc_path *hrt_bw_request, *hrt_by_larb, *dp_hrt_by_larb;

	struct pinctrl *pctrl;

#ifdef DRM_MMPATH
	int HWC_gpid; // for mmpath auto gen
#endif

	int need_vds_path_switch;
	int vds_path_switch_dirty;
	int vds_path_switch_done;
	int vds_path_enable;

	bool need_cwb_path_disconnect;
	bool cwb_is_preempted;

	bool dma_parms_allocated;

	bool already_first_config;
	bool is_tablet;

	/*
	 * When legacy chip HDCP and SVP is enabled,
	 * Prime display always uses OVL0,Virtual display always uses OVL0_2L.
	 */
	bool secure_static_path_switch;

	struct mml_drm_ctx *mml_ctx;
	atomic_t need_recover;

	unsigned int seg_id;

	unsigned int srt_channel_bw_sum[MAX_CRTC][BW_CHANNEL_NR];
	unsigned int total_srt[MAX_CRTC];

	struct mtk_uevent_dev uevent_data;
};

struct mtk_drm_property {
	int flags;
	char *name;
	unsigned long min;
	unsigned long max;
	unsigned long val;
};

struct repaint_job_t {
	struct list_head link;
	unsigned int type;
};

#define LCM_FPS_ARRAY_SIZE  32
struct lcm_fps_ctx_t {
	atomic_t is_inited;
	spinlock_t lock;
	atomic_t skip_update;
	unsigned int dsi_mode;
	unsigned int head_idx;
	unsigned int num;
	unsigned int fps;
	unsigned long long last_ns;
	unsigned long long array[LCM_FPS_ARRAY_SIZE];
};

#define DISP_LARB_COUNT 1
struct disp_iommu_device {
	struct platform_device *larb_pdev[DISP_LARB_COUNT];
	struct platform_device *iommu_pdev;
	unsigned int inited;
};

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1]; /* this is the minimum size */
};

struct mtk_drm_disp_sec_cb {
	int (*cb)(int value, struct cmdq_pkt *handle, resource_size_t dummy_larb,
				struct dma_buf *buf_hnd);
};

struct mtk_aod_scp_cb {
	int (*send_ipi)(int value);
	void (*module_backup)(struct drm_crtc *crtc, unsigned int ulps_wakeup_prd);
};

struct mtk_drm_disp_mtee_cb {
	struct drm_device *dev;
	int (*cb)(int value, int fd, struct mtk_drm_gem_obj *mtk_gem_obj,
	struct cmdq_pkt *handle, struct mtk_ddp_comp *comp, u32 crtc_id,
	u32 regs_addr, u32 lye_addr, u32 offset, u32 size);
};

enum DISP_SEC_SIGNAL {
	DISP_SEC_START = 0,
	DISP_SEC_STOP,
	DISP_SEC_ENABLE,
	DISP_SEC_DISABLE,
	DISP_SEC_CHECK,
	DISP_SEC_FD_TO_SEC_HDL,
};

struct layer_compress_ratio_data {
	__u32 frame_idx;
	__u64 key_value;
	__u32 *average_ratio;
	__u32 *peak_ratio;
	__u32 valid;
	__u32 active;
};

struct bwm_plane_info {
	__u32 plane_index;
	__u32 width;
	__u32 height;
	__u32 lye_id;
	__u32 ext_lye_id;
	__u32 dst_x;
};

struct layer_compress_ratio_item {
	__u32 frame_idx;
	__u64 key_value;
	__u32 average_ratio;
	__u32 peak_ratio;
	__u32 valid;
	__u32 active;
};

enum {
	DISP_PLAT_DBG_INIT,
	DISP_PLAT_DBG_ENABLE,
	DISP_PLAT_DBG_PROFILE,
	DISP_PLAT_MAX,
};

struct disp_plat_dbg_scmi_data {
	unsigned int cmd;
	unsigned int p1;
	unsigned int p2;
	unsigned int p3;
	unsigned int p4;
};

enum disp_pm_action {
	DISP_PM_ENABLE,
	DISP_PM_DISABLE,
	DISP_PM_GET,
	DISP_PM_PUT,
	DISP_PM_PUT_SYNC,
	DISP_PM_CHECK,
};

static const struct mtk_addon_module_data addon_rsz_data[] = {
	{DISP_RSZ, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v2[] = {
	{DISP_RSZ_v2, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v3[] = {
	{DISP_RSZ_v3, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v4[] = {
	{DISP_RSZ_v4, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v5[] = {
	{DISP_RSZ_v5, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v6[] = {
	{DISP_RSZ_v6, ADDON_BETWEEN, DDP_COMPONENT_OVL3_2L},
};

static const struct mtk_addon_module_data addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_DITHER0},
};

static const struct mtk_addon_module_data addon_wdma1_data[] = {
	{DISP_WDMA1, ADDON_AFTER, DDP_COMPONENT_DITHER1},
};

struct disp_iommu_device *disp_get_iommu_dev(void);

extern struct platform_driver mtk_ddp_driver;
extern struct platform_driver mtk_disp_tdshp_driver;
extern struct platform_driver mtk_disp_color_driver;
extern struct platform_driver mtk_disp_ccorr_driver;
extern struct platform_driver mtk_disp_c3d_driver;
extern struct platform_driver mtk_disp_gamma_driver;
extern struct platform_driver mtk_disp_aal_driver;
extern struct platform_driver mtk_dmdp_aal_driver;
extern struct platform_driver mtk_disp_postmask_driver;
extern struct platform_driver mtk_disp_dither_driver;
extern struct platform_driver mtk_disp_chist_driver;
extern struct platform_driver mtk_disp_ovl_driver;
extern struct platform_driver mtk_ovl_exdma_driver;
extern struct platform_driver mtk_ovl_blender_driver;
extern struct platform_driver mtk_ovl_outproc_driver;
extern struct platform_driver mtk_vdisp_ao_driver;
extern struct platform_driver mtk_disp_rdma_driver;
extern struct platform_driver mtk_disp_mdp_rdma_driver;
extern struct platform_driver mtk_disp_wdma_driver;
extern struct platform_driver mtk_disp_rsz_driver;
extern struct platform_driver mtk_disp_mdp_rsz_driver;
extern struct platform_driver mtk_dpi_driver;
extern struct platform_driver mtk_cec_driver;
extern struct platform_driver mtk_dsi_driver;
extern struct platform_driver mtk_mipi_tx_driver;
extern struct platform_driver mtk_lvds_driver;
extern struct platform_driver mtk_lvds_tx_driver;
extern struct platform_driver mtk_disp_dsc_driver;
extern struct platform_driver mtk_disp_cm_driver;
extern struct platform_driver mtk_disp_spr_driver;
extern struct platform_driver mtk_disp_oddmr_driver;
extern struct lcm_fps_ctx_t lcm_fps_ctx[MAX_CRTC];
extern struct platform_driver mtk_disp_merge_driver;
extern struct platform_driver mtk_disp_splitter_driver;
extern struct platform_driver mtk_dp_tx_driver;
extern struct platform_driver mtk_dp_intf_driver;
extern struct platform_driver mtk_disp_y2r_driver;
extern struct platform_driver mtk_disp_r2y_driver;
extern struct platform_driver mtk_disp_dlo_async_driver;
extern struct platform_driver mtk_disp_dli_async_driver;
extern struct platform_driver mtk_disp_inlinerotate_driver;
extern struct platform_driver mtk_mmlsys_bypass_driver;
extern struct platform_driver mtk_disp_postalign_driver;
extern struct mtk_drm_disp_sec_cb disp_sec_cb;
extern struct mtk_aod_scp_cb aod_scp_ipi;
extern struct mtk_vdisp_funcs vdisp_func;
extern struct mtk_drm_disp_mtee_cb disp_mtee_cb;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT)
extern struct platform_driver mtk_dvo_driver;
#endif

/* For overlay bandwidth monitor */
extern struct layer_compress_ratio_data
display_compress_ratio_table[MAX_LAYER_RATIO_NUMBER];
extern struct layer_compress_ratio_data
display_fbt_compress_ratio_table;
extern struct layer_compress_ratio_item
normal_layer_compress_ratio_tb[MAX_FRAME_RATIO_NUMBER*MAX_LAYER_RATIO_NUMBER];
extern struct layer_compress_ratio_item
fbt_layer_compress_ratio_tb[MAX_FRAME_RATIO_NUMBER];
extern struct layer_compress_ratio_item
unchanged_compress_ratio_table[MAX_LAYER_RATIO_NUMBER];
extern struct layer_compress_ratio_item
fbt_compress_ratio_table[MAX_FRAME_RATIO_NUMBER];
extern unsigned int ovl_win_size;
extern unsigned int default_emi_eff;
extern unsigned int emi_eff_tb[MAX_EMI_EFF_LEVEL];
extern int aod_scp_flag;
extern int mtkfb_set_backlight_level(unsigned int level, unsigned int panel_ext_param,
				 unsigned int cfg_flag);
extern int mtkfb_set_backlight_level_AOD(unsigned int level);
extern void mtk_ovl_set_aod_scp_hrt(void);
void mtk_atomic_state_put_queue(struct drm_atomic_state *state);
void mtk_drm_fence_update(unsigned int fence_idx, unsigned int index);
void drm_trigger_repaint(enum DRM_REPAINT_TYPE type,
			 struct drm_device *drm_dev);
int mtk_drm_suspend_release_fence(struct device *dev);
void mtk_drm_suspend_release_present_fence(struct device *dev,
					   unsigned int index);
void mtk_drm_suspend_release_sf_present_fence(struct device *dev,
					      unsigned int index);
void mtk_drm_top_clk_prepare_enable(struct drm_crtc *crtc);
void mtk_drm_top_clk_disable_unprepare(struct drm_device *drm);
struct mtk_panel_params *mtk_drm_get_lcm_ext_params(struct drm_crtc *crtc);
struct mtk_panel_funcs *mtk_drm_get_lcm_ext_funcs(struct drm_crtc *crtc);
bool mtk_drm_session_mode_is_decouple_mode(struct drm_device *dev);
bool mtk_drm_session_mode_is_mirror_mode(struct drm_device *dev);
bool mtk_drm_top_clk_isr_get(struct mtk_ddp_comp *comp);
void mtk_drm_top_clk_isr_put(struct mtk_ddp_comp *comp);
int lcm_fps_ctx_init(struct drm_crtc *crtc);
int lcm_fps_ctx_reset(struct drm_crtc *crtc);
int lcm_fps_ctx_update(unsigned long long cur_ns,
		unsigned int crtc_id, unsigned int mode);
int mtk_mipi_clk_change(struct drm_crtc *crtc, unsigned int data_rate);
bool mtk_drm_lcm_is_connect(struct mtk_drm_crtc *mtk_crtc);
unsigned int mtk_disp_num_from_atag(void);
int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
	unsigned int *fps);
struct mml_drm_ctx *mtk_drm_get_mml_drm_ctx(struct drm_device *dev,
	struct drm_crtc *crtc);
void mtk_drm_wait_mml_submit_done(struct mtk_mml_cb_para *cb_para);
void mtk_aod_scp_ipi_init(struct mtk_aod_scp_cb *cb);
void mtk_free_mml_submit(struct mml_submit *temp);
int copy_mml_submit(struct mml_submit *src, struct mml_submit *dst);
void **mtk_drm_disp_sec_cb_init(void);
void mtk_crtc_v_idle_apsrc_control(struct drm_crtc *crtc,
	struct cmdq_pkt *_cmdq_handle, bool reset, bool condition_check,
	unsigned int crtc_id, bool enable);
int scmi_set(void *buffer);
int mtk_drm_get_master_info_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv);
void mtk_vidle_multi_crtc_stop(unsigned int crtc_id);
int mtk_drm_pm_ctrl(struct mtk_drm_private *priv, enum disp_pm_action);
void **mtk_drm_disp_mtee_cb_init(void);
bool mtk_disp_is_svp_on_mtee(void);
void _mtk_sent_aod_scp_sema(void __iomem *_SPM_SEMA_AP);
int mtk_drm_get_mml_mode_caps(void);
int mtk_drm_get_mml_hw_caps(void);

extern int mtk_notifier_call_chain(unsigned long val, void *v);
#endif /* MTK_DRM_DRV_H */
