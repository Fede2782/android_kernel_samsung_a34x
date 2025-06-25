/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_HDMI_CTRL_H
#define _MTK_HDMI_CTRL_H

#include <linux/hdmi.h>
#include <drm/drm_bridge.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <sound/hdmi-codec.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include "mtk_hdmi_hdcp.h"
#include <linux/kfifo.h>

#define RGB444_8bit  BIT(0)
#define RGB444_10bit BIT(1)
#define RGB444_12bit BIT(2)
#define RGB444_16bit BIT(3)
#define YCBCR444_8bit  BIT(4)
#define YCBCR444_10bit BIT(5)
#define YCBCR444_12bit BIT(6)
#define YCBCR444_16bit BIT(7)
#define YCBCR422_8bit  BIT(8)
#define YCBCR422_10bit BIT(9)
#define YCBCR422_12bit BIT(10)
#define YCBCR422_16bit_NO_SUPPORT BIT(11)
#define YCBCR420_8bit  BIT(12)
#define YCBCR420_10bit BIT(13)
#define YCBCR420_12bit BIT(14)
#define YCBCR420_16bit BIT(15)

#define NCTS_BYTES	7
#define MAX_BLOCK_NUM	8
#define HPD_WQ_NUM	4
#define HPD_FIFO_NUM	16

enum mtk_hdmi_clk_id {
	MTK_HDMI_CLK_HDMI_TXPLL,
	MTK_HDMI_CLK_UNIVPLL_D6D4,
	MTK_HDMI_CLK_MSDCPLL_D2,
	MTK_HDMI_CLK_HDMI_APB_SEL,
	MTK_HDMI_UNIVPLL_D4D8,
	MTK_HDIM_HDCP_SEL,
	MTK_HDMI_HDCP_24M_SEL,
	MTK_HDMI_VPP_SPLIT_HDMI,
	MTK_HDMI_CLK_COUNT,
};

enum hdmi_aud_input_type {
	HDMI_AUD_INPUT_I2S = 0,
	HDMI_AUD_INPUT_SPDIF,
};

enum hdmi_aud_i2s_fmt {
	HDMI_I2S_MODE_RJT_24BIT = 0,
	HDMI_I2S_MODE_RJT_16BIT,
	HDMI_I2S_MODE_LJT_24BIT,
	HDMI_I2S_MODE_LJT_16BIT,
	HDMI_I2S_MODE_I2S_24BIT,
	HDMI_I2S_MODE_I2S_16BIT
};

enum hdmi_aud_mclk {
	HDMI_AUD_MCLK_128FS,
	HDMI_AUD_MCLK_192FS,
	HDMI_AUD_MCLK_256FS,
	HDMI_AUD_MCLK_384FS,
	HDMI_AUD_MCLK_512FS,
	HDMI_AUD_MCLK_768FS,
	HDMI_AUD_MCLK_1152FS,
};

enum hdmi_aud_channel_type {
	HDMI_AUD_CHAN_TYPE_1_0 = 0,
	HDMI_AUD_CHAN_TYPE_1_1,
	HDMI_AUD_CHAN_TYPE_2_0,
	HDMI_AUD_CHAN_TYPE_2_1,
	HDMI_AUD_CHAN_TYPE_3_0,
	HDMI_AUD_CHAN_TYPE_3_1,
	HDMI_AUD_CHAN_TYPE_4_0,
	HDMI_AUD_CHAN_TYPE_4_1,
	HDMI_AUD_CHAN_TYPE_5_0,
	HDMI_AUD_CHAN_TYPE_5_1,
	HDMI_AUD_CHAN_TYPE_6_0,
	HDMI_AUD_CHAN_TYPE_6_1,
	HDMI_AUD_CHAN_TYPE_7_0,
	HDMI_AUD_CHAN_TYPE_7_1,
	HDMI_AUD_CHAN_TYPE_3_0_LRS,
	HDMI_AUD_CHAN_TYPE_3_1_LRS,
	HDMI_AUD_CHAN_TYPE_4_0_CLRS,
	HDMI_AUD_CHAN_TYPE_4_1_CLRS,
	HDMI_AUD_CHAN_TYPE_6_1_CS,
	HDMI_AUD_CHAN_TYPE_6_1_CH,
	HDMI_AUD_CHAN_TYPE_6_1_OH,
	HDMI_AUD_CHAN_TYPE_6_1_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_LH_RH,
	HDMI_AUD_CHAN_TYPE_7_1_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_7_1_LC_RC,
	HDMI_AUD_CHAN_TYPE_7_1_LW_RW,
	HDMI_AUD_CHAN_TYPE_7_1_LSD_RSD,
	HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS,
	HDMI_AUD_CHAN_TYPE_7_1_LHS_RHS,
	HDMI_AUD_CHAN_TYPE_7_1_CS_CH,
	HDMI_AUD_CHAN_TYPE_7_1_CS_OH,
	HDMI_AUD_CHAN_TYPE_7_1_CS_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_CH_OH,
	HDMI_AUD_CHAN_TYPE_7_1_CH_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_OH_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_6_0_CS,
	HDMI_AUD_CHAN_TYPE_6_0_CH,
	HDMI_AUD_CHAN_TYPE_6_0_OH,
	HDMI_AUD_CHAN_TYPE_6_0_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_LH_RH,
	HDMI_AUD_CHAN_TYPE_7_0_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_7_0_LC_RC,
	HDMI_AUD_CHAN_TYPE_7_0_LW_RW,
	HDMI_AUD_CHAN_TYPE_7_0_LSD_RSD,
	HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS,
	HDMI_AUD_CHAN_TYPE_7_0_LHS_RHS,
	HDMI_AUD_CHAN_TYPE_7_0_CS_CH,
	HDMI_AUD_CHAN_TYPE_7_0_CS_OH,
	HDMI_AUD_CHAN_TYPE_7_0_CS_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_CH_OH,
	HDMI_AUD_CHAN_TYPE_7_0_CH_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_OH_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_8_0_LH_RH_CS,
	HDMI_AUD_CHAN_TYPE_UNKNOWN = 0xFF
};

enum hdmi_aud_channel_swap_type {
	HDMI_AUD_SWAP_LR,
	HDMI_AUD_SWAP_LFE_CC,
	HDMI_AUD_SWAP_LSRS,
	HDMI_AUD_SWAP_RLS_RRS,
	HDMI_AUD_SWAP_LR_STATUS,
};

struct hdmi_audio_param {
	enum hdmi_audio_coding_type aud_codec;
	enum hdmi_audio_sample_size aud_sampe_size;
	enum hdmi_aud_input_type aud_input_type;
	enum hdmi_aud_i2s_fmt aud_i2s_fmt;
	enum hdmi_aud_mclk aud_mclk;
	enum hdmi_aud_channel_type aud_input_chan_type;
	struct hdmi_codec_params codec_params;
};

enum hdmi_color_depth {
	HDMI_8_BIT,
	HDMI_10_BIT,
	HDMI_12_BIT,
	HDMI_16_BIT
};

struct mtk_hdmi_edid {
	unsigned char edid[EDID_LENGTH * MAX_BLOCK_NUM];
	unsigned char blk_num;
};

struct mtk_hdmi_driver_data {
	const u32 reg_hdmitx_config_ofs;
};

enum HDMI_HPD_STATE {
	HDMI_PLUG_OUT = 0,
	HDMI_PLUG_IN_AND_SINK_POWER_ON,
	HDMI_PLUG_IN_ONLY,
};

struct mtk_hdmi {
	struct drm_bridge bridge;
	struct drm_connector conn;
	struct device *dev;
	struct phy *phy;
	struct mtk_hdmi_phy *hdmi_phy_base;
	struct device *cec_dev;
	struct cec_notifier *notifier;
	struct i2c_adapter *ddc_adpt;
	struct clk *clk[MTK_HDMI_CLK_COUNT];
	struct mtk_hdmi_driver_data *driver_data;
	struct drm_display_mode mode;
	bool dvi_mode;
	u32 max_hdisplay;
	u32 max_vdisplay;
	void __iomem *regs;
	spinlock_t property_lock;
	struct drm_property *hdmi_info_blob;
	struct drm_property_blob *hdmi_info_blob_ptr;
	struct drm_property *csp_depth_prop;
	uint64_t support_csp_depth;
	uint64_t set_csp_depth;
	enum hdmi_colorspace csp;
	enum hdmi_color_depth color_depth;
	enum hdmi_colorimetry colorimtery;
	enum hdmi_extended_colorimetry extended_colorimetry;
	enum hdmi_quantization_range quantization_range;
	enum hdmi_ycc_quantization_range ycc_quantization_range;
	struct mtk_hdmi_edid raw_edid;
	bool use_firmware_edid;
	unsigned char firmware_edid[EDID_LENGTH * 4];
	struct hdmi_audio_param aud_param;
	bool audio_enable;
	struct device *codec_dev;
	hdmi_codec_plugged_cb plugged_cb;

	bool powered;
	bool enabled;
	unsigned int hdmi_irq;
	enum HDMI_HPD_STATE hpd;
	struct workqueue_struct *hdmi_wq;
	struct workqueue_struct *hdmi_hpd_wq[HPD_WQ_NUM];
	struct delayed_work hpd_work[HPD_WQ_NUM];
	struct delayed_work hdr10_delay_work;
	struct delayed_work hdr10vsif_delay_work;
	struct mutex hdr_mutex;

	wait_queue_head_t hpd_waitqueue;
	struct task_struct *hpd_task;
	DECLARE_KFIFO(fifo, enum HDMI_HPD_STATE, HPD_FIFO_NUM);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	bool repeater_hdcp;
	wait_queue_head_t hdcp_wq;
	struct task_struct *hdcp_task;
	struct timer_list hdcp_timer;
	bool enable_hdcp;
	bool hdcp_2x_support;
	bool key_is_installed;
	bool bin_is_loaded;
	bool donwstream_is_repeater;
	enum HDCP_CTRL_STATE_T hdcp_ctrl_state;
#endif

	struct timer_list ca_timer;
	bool ca_init;

	bool hdmi_enabled;
	bool power_clk_enabled;
	bool irq_registered;
	bool hpd_force_set;
	bool ALLM_enabled;
	bool attached;
	bool notify_user_ready;

	bool spd_exist;
	char spd_vendor[8];
	char spd_product[16];
	bool lk_fastlogo;
	bool debug_csp_depth;
	unsigned int debug_abist_OnOff;
	unsigned int debug_abist_select_tclk;
};

extern struct platform_driver mtk_hdmi_ddc_driver;
extern struct platform_driver mtk_hdmi_phy_driver;

extern struct mtk_hdmi *global_mtk_hdmi;

u32 mtk_hdmi_read(struct mtk_hdmi *hdmi, u32 offset);
void mtk_hdmi_write(struct mtk_hdmi *hdmi, u32 offset, u32 val);
void mtk_hdmi_mask(struct mtk_hdmi *hdmi,
	u32 offset, u32 val, u32 mask);
void mtk_hdmi_clk_enable(struct mtk_hdmi *hdmi);
void mtk_hdmi_clk_disable(struct mtk_hdmi *hdmi);
void mtk_hdmi_AV_mute(struct mtk_hdmi *hdmi);
void mtk_hdmi_AV_unmute(struct mtk_hdmi *hdmi);
bool mtk_hdmi_tmds_over_340M(struct mtk_hdmi *hdmi);
struct mtk_hdmi_ddc *hdmi_ddc_ctx_from_mtk_hdmi(struct mtk_hdmi *hdmi);
void mtk_hdmi_colorspace_setting(struct mtk_hdmi *hdmi);
int mtk_hdmi_setup_h14b_vsif(struct mtk_hdmi *hdmi, struct drm_display_mode *mode);
int mtk_hdmi_setup_hf_vsif(struct mtk_hdmi *hdmi, struct drm_display_mode *mode);
int mtk_hdmi_setup_avi_infoframe(struct mtk_hdmi *hdmi, struct drm_display_mode *mode);
void mtk_hdmi_audio_reset(struct mtk_hdmi *hdmi, bool rst);
void mtk_hdmi_hw_send_aud_packet(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_vid_black(struct mtk_hdmi *hdmi, bool black);
void mtk_hdmi_hw_aud_mute(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_aud_unmute(struct mtk_hdmi *hdmi);
void mtk_hdmi_handle_plugged_change(struct mtk_hdmi *hdmi, bool plugged);
void mtk_hdmi_hw_send_av_mute(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_send_av_unmute(struct mtk_hdmi *hdmi);
void mtk_hdmi_convert_colorspace_depth(struct mtk_hdmi *hdmi);

struct mtk_hdmi_edid *mtk_hdmi_get_raw_edid(struct drm_bridge *bridge);
unsigned int set_hdmi_colorspace_depth(struct drm_bridge *bridge, uint64_t colorspace_depth);
unsigned int set_hdmi_colorimetry_range(struct drm_bridge *bridge,
	enum hdmi_colorimetry colorimtery, enum hdmi_extended_colorimetry extended_colorimetry,
	enum hdmi_quantization_range quantization_range,
	enum hdmi_ycc_quantization_range ycc_quantization_range);
unsigned int get_hdmi_colorspace_colorimetry(struct drm_bridge *bridge,
	enum hdmi_colorspace *colorspace, enum hdmi_colorimetry *colorimtery,
	enum hdmi_extended_colorimetry *extended_colorimetry,
	enum hdmi_quantization_range *quantization_range,
	enum hdmi_ycc_quantization_range *ycc_quantization_range);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
void hdmi_hdcp_start(void);
void hdmi_hdcp_stop(void);
#endif

int mtk_drm_ioctl_enable_hdmi(struct drm_device *dev, void *data, struct drm_file *file_priv);

/* struct mtk_hdmi_info is used to propagate blob property to userspace */
struct mtk_hdmi_info {
	unsigned int edid_sink_colorimetry;
	unsigned char edid_sink_rgb_color_bit;
	unsigned char edid_sink_ycbcr_color_bit;
	unsigned char ui1_sink_dc420_color_bit;
	unsigned short edid_sink_max_tmds_clock;
	unsigned short edid_sink_max_tmds_character_rate;
	unsigned char edid_sink_support_dynamic_hdr;
	unsigned char edid_sink_support_static_hdr;
};

int mtk_hdmi_update_hdmi_info_property(struct mtk_hdmi *hdmi);
void mtk_hdmi_send_TMDS_configuration(struct mtk_hdmi *hdmi, unsigned char enscramble);

void force_hpd_uevent(unsigned char force_hpd);
#endif /* _MTK_HDMI_CTRL_H */
