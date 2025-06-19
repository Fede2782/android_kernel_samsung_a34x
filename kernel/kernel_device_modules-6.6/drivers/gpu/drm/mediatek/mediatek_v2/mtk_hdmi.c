// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/pm_wakeup.h>

#include <drm/drm_scdc_helper.h>
#include <drm/drm_displayid.h>
#include <drm/drm_probe_helper.h>
#include <drm/mediatek_drm.h>

#include <media/cec.h>
#include <media/cec-notifier.h>
#include "mtk_drm_drv.h"

#include "mtk_hdmi.h"
#include "mtk_hdmi_ddc.h"
#include "mtk_hdmi_phy.h"
#include "mtk_hdmi_regs.h"
#include "mtk_hdmi_hdcp.h"
#include "mtk_hdmi_ca.h"
#include "mtk_hdmi_debug.h"
#include "mtk_hdmi_hdr.h"
#include "mtk_hdmi_edid.h"

#include <linux/extcon.h>
#include <../../../extcon/extcon.h>

#include <linux/debugfs.h>

#define HDMI_DE_LOG 0x01
#define HDMI_DE_FUNC 0x02
#define HDMI_DE_DETAIL_LOG 0x04

void (*ethdr_notify_hdmi_hpd)(bool hpd) = NULL;
EXPORT_SYMBOL_GPL(ethdr_notify_hdmi_hpd);

static const char * const mtk_hdmi_clk_names[MTK_HDMI_CLK_COUNT] = {
	[MTK_HDMI_CLK_HDMI_TXPLL] = "txpll",
	[MTK_HDMI_CLK_UNIVPLL_D6D4] = "univpll_d6_d4",
	[MTK_HDMI_CLK_MSDCPLL_D2] = "msdcpll_d2",
	[MTK_HDMI_CLK_HDMI_APB_SEL] = "hdmi_apb_sel",
	[MTK_HDMI_UNIVPLL_D4D8] = "univpll_d4_d8",
	[MTK_HDIM_HDCP_SEL] = "hdcp_sel",
	[MTK_HDMI_HDCP_24M_SEL] = "hdcp24_sel",
	[MTK_HDMI_VPP_SPLIT_HDMI] = "split_hdmi",
};

static const char default_edid[] = {
// block 0
0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x51, 0xf0, 0x60, 0x24, 0x00, 0x00, 0x00, 0x00,
0x01, 0x1c, 0x01, 0x03, 0x80, 0x34, 0x1d, 0x78, 0x2a, 0xa2, 0x65, 0xa6, 0x55, 0x55, 0x9f, 0x28,
0x0d, 0x50, 0x54, 0xbf, 0xee, 0x00, 0xd1, 0xc0, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x64, 0x19, 0x00, 0x40, 0x41, 0x00, 0x26, 0x30, 0x18, 0x88,
0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x32, 0x4c, 0x1e,
0x53, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x48,
0x44, 0x4d, 0x49, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xff,
0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xc6,
// block 1
0x02, 0x03, 0x1f, 0xf2, 0x4c, 0x03, 0x84, 0x05, 0x07, 0x0e, 0x10, 0x12, 0x93, 0x14, 0x16, 0x1e,
0x1f, 0x23, 0x09, 0x17, 0x07, 0x83, 0x01, 0x00, 0x00, 0x65, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x8c,
0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00, 0x13, 0x8e, 0x21, 0x00, 0x00,
0x18, 0x01, 0x1d, 0x00, 0xbc, 0x52, 0xd0, 0x1e, 0x20, 0xb8, 0x28, 0x55, 0x40, 0xc4, 0x8e, 0x21,
0x00, 0x00, 0x1e, 0x01, 0x1d, 0x80, 0xd0, 0x72, 0x1c, 0x16, 0x20, 0x10, 0x2c, 0x25, 0x80, 0xc4,
0x8e, 0x21, 0x00, 0x00, 0x9e, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96,
0x00, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x18, 0x8c, 0x0a, 0xd0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0c,
0x40, 0x55, 0x00, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd1,
};

//can.zeng todo verify
struct mtk_hdmi *global_mtk_hdmi;

struct extcon_dev *hdmi_extcon;
static const unsigned int hdmi_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

unsigned char mtk_hdmi_log = HDMI_DE_LOG | HDMI_DE_FUNC;

atomic_t hdmi_hpd_event = ATOMIC_INIT(0);
atomic_t hdmi_hpd_work = ATOMIC_INIT(0);
#define TIME_HPD_PORD_STABLE_MS 30

#define HDMI_LOG(fmt, arg...) \
	do {	if (mtk_hdmi_log & HDMI_DE_LOG) { \
		pr_info("[HDMI] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

#define HDMI_FUNC()	\
	do {	if (mtk_hdmi_log & HDMI_DE_FUNC) \
		pr_info("[HDMI] %s\n", __func__); \
	} while (0)

#define HDMI_DETAIL_LOG(fmt, arg...) \
	do {	if (mtk_hdmi_log & HDMI_DE_DETAIL_LOG) { \
		pr_info("[HDMI] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

static inline struct mtk_hdmi *hdmi_ctx_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_hdmi, bridge);
}

static inline struct mtk_hdmi *hdmi_ctx_from_conn(struct drm_connector *c)
{
	return container_of(c, struct mtk_hdmi, conn);
}

struct mtk_hdmi_ddc *hdmi_ddc_ctx_from_mtk_hdmi(struct mtk_hdmi *hdmi)
{
	return container_of(hdmi->ddc_adpt, struct mtk_hdmi_ddc, adap);
}

u32 mtk_hdmi_read(struct mtk_hdmi *hdmi, u32 offset)
{
	return readl(hdmi->regs + offset);
}

void mtk_hdmi_write(struct mtk_hdmi *hdmi, u32 offset, u32 val)
{
#if IS_ENABLED(CONFIG_OPTEE)
	vCaHDMIWriteReg(offset, val);
#else
	writel(val, hdmi->regs + offset);
#endif
}

void mtk_hdmi_mask(struct mtk_hdmi *hdmi,
	u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = hdmi->regs + offset;
	u32 tmp;

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
#if IS_ENABLED(CONFIG_OPTEE)
	vCaHDMIWriteReg(offset, tmp);
#else
	writel(tmp, reg);
#endif
}

/****************switch uevent**************************/
struct notify_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;
	int crtc;

	ssize_t (*print_name)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_state)(struct notify_dev *sdev, char *buf);
};

struct notify_dev hdmi_switch_data;
struct notify_dev hdmi_cec_switch_data;

struct class *switch_class_hdmi;
static atomic_t device_count_hdmi;

static ssize_t state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}

static DEVICE_ATTR_RO(state);
static DEVICE_ATTR_RO(name);

static int create_switch_class(void)
{
	if (!switch_class_hdmi) {
		switch_class_hdmi = class_create(THIS_MODULE, "switch");
		if (IS_ERR(switch_class_hdmi))
			return PTR_ERR(switch_class_hdmi);
		atomic_set(&device_count_hdmi, 0);
	}
	return 0;
}

static int hdmitx_uevent_dev_register(struct notify_dev *sdev)
{
	int ret;

	if (!switch_class_hdmi) {
		ret = create_switch_class();

		if (ret == 0)
			HDMI_DETAIL_LOG("create_switch_class susesess\n");
		else {
			HDMI_LOG("create_switch_class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&device_count_hdmi);
	sdev->dev = device_create(switch_class_hdmi, NULL,
			MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		HDMI_DETAIL_LOG("device create ok,index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		HDMI_LOG("device create fail,index:0x%x\n", sdev->index);
		ret = -1;
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0) {
		device_destroy(switch_class_hdmi, MKDEV(0, sdev->index));
		HDMI_LOG("switch: Failed to register driver %s\n",
				sdev->name);
	}

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		HDMI_LOG("switch: Failed to register driver %s\n",
				sdev->name);
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	return ret;
}

static int hdmi_notify_uevent_user(struct notify_dev *sdev, int state)
{
	char *envp[4];
	char name_buf[120];
	char state_buf[120];
	char crtc_buf[16];
	int ret = 0;

	if (global_mtk_hdmi->notify_user_ready == false) {
		HDMI_LOG("user is not ready\n");
		return -1;
	}

	if (sdev == NULL)
		return -1;

	if (sdev->state != state)
		sdev->state = state;

	ret = snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	if (ret <= 0) {
		HDMI_LOG("snprintf error\n");
		return -1;
	}
	envp[0] = name_buf;
	ret = snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", sdev->state);
	if (ret <= 0) {
		HDMI_LOG("snprintf error\n");
		return -1;
	}
	envp[1] = state_buf;
	ret = snprintf(crtc_buf, sizeof(crtc_buf), "CRTC=%d", sdev->crtc);
	if (ret <= 0) {
		HDMI_LOG("snprintf error\n");
		return -1;
	}
	envp[2] = crtc_buf;
	envp[3] = NULL;
	HDMI_LOG("uevent name:%s ,state:%s, dev:%s\n", envp[0], envp[1], envp[2]);

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

/****************switch uevent**************************/

void force_hpd_uevent(unsigned char force_hpd)
{
	if (force_hpd != 0)
		hdmi_notify_uevent_user(&hdmi_switch_data, 1);
	else
		hdmi_notify_uevent_user(&hdmi_switch_data, 0);
}

static inline void mtk_hdmi_clr_all_int_status(struct mtk_hdmi *hdmi)
{
	/*clear all tx irq*/
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, mtk_hdmi_read(hdmi, TOP_INT_MASK00));
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00000000);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, mtk_hdmi_read(hdmi, TOP_INT_MASK01));
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x00000000);
}

static inline void mtk_hdmi_disable_all_int(struct mtk_hdmi *hdmi)
{
	/*disable all tx irq*/
	mtk_hdmi_write(hdmi, TOP_INT_MASK00, 0x00000000);
	mtk_hdmi_write(hdmi, TOP_INT_MASK01, 0x00000000);
}

static inline void mtk_hdmi_en_hdcp_reauth_int(
	struct mtk_hdmi *hdmi,  bool enable)
{
	if (enable == true)
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK);
	else
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_MASK,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK);
}

inline void mtk_hdmi_enable_hpd_pord_irq(struct mtk_hdmi *hdmi, bool enable)
{
	if (enable == true)
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00, 0x0000000f, 0x0000000f);
	else
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00, 0x00000000, 0x0000000f);
}

inline void mtk_hdmi_clr_htplg_pord_irq(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, TOP_INT_CLR00, 0x0000000f, 0x0000000f);
	mtk_hdmi_mask(hdmi, TOP_INT_CLR00, 0x00000000, 0x0000000f);
}

inline void mtk_hdmi_set_sw_hpd(struct mtk_hdmi *hdmi, bool high)
{
	/* HDMITX_CONFIG BIT(28) BIT(29) cannot be read, if read, always 0.
	 * if use mtk_hdmi_mask(), it will read the register fisrt,
	 * then write register. if want to set BIT(28) BIT(29) 1,
	 * every time write HDMITX_CONFIG shall set BIT(28) BIT(29) to 1.
	 */
	if (high == true)
		mtk_hdmi_mask(hdmi, hdmi->driver_data->reg_hdmitx_config_ofs,
			0x1 << HDMITX_SW_HPD_SHIFT, HDMITX_SW_HPD);
	else
		mtk_hdmi_mask(hdmi, hdmi->driver_data->reg_hdmitx_config_ofs,
			0x0 << HDMITX_SW_HPD_SHIFT, HDMITX_SW_HPD);
}

//can.zeng modification done->void HDMIForceHDCPHPCLevel(void)
static inline void mtk_hdmi_force_hdcp_hpd(struct mtk_hdmi *hdmi)
{
	/* force HDCP HPD to 1*/
	mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_HPD_OVR, HDCP2X_HPD_OVR);
	mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_HPD_SW, HDCP2X_HPD_SW);
}


//can.zeng modification done->void vDisable_HDCP_Encrypt(void)
void mtk_hdmi_disable_hdcp_encrypt(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();

	mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0,
		0x0 << HDCP2X_ENCRYPT_EN_SHIFT, HDCP2X_ENCRYPT_EN);
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL,
		0x0 << HDCP1X_ENC_EN_SHIFT, HDCP1X_ENC_EN);
}

void mtk_hdmi_yuv422_downsample(struct mtk_hdmi *hdmi, bool enable)
{
	HDMI_FUNC();

	if (enable == true) {
		HDMI_LOG("Color Space: YUV444 to YUV422 downsample\n");
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG, C444_C422_CONFIG_ENABLE,
			C444_C422_CONFIG_ENABLE);
	} else
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG, 0, C444_C422_CONFIG_ENABLE);

}

void mtk_hdmi_yuv420_downsample(struct mtk_hdmi *hdmi, bool enable)
{
	HDMI_FUNC();

	if (enable == true) {
		HDMI_LOG("Color Space: YUV444 toYUV420 downsample\n");
		mtk_hdmi_mask(hdmi, hdmi->driver_data->reg_hdmitx_config_ofs,
			HDMI_YUV420_MODE | HDMITX_SW_HPD, HDMI_YUV420_MODE | HDMITX_SW_HPD);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			C444_C422_CONFIG_ENABLE, C444_C422_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			C422_C420_CONFIG_ENABLE, C422_C420_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			0, C422_C420_CONFIG_BYPASS);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			C422_C420_CONFIG_OUT_CB_OR_CR,
			C422_C420_CONFIG_OUT_CB_OR_CR);
		mtk_hdmi_mask(hdmi, VID_OUT_FORMAT,
			OUTPUT_FORMAT_DEMUX_420_ENABLE,
			OUTPUT_FORMAT_DEMUX_420_ENABLE);
	} else {
		mtk_hdmi_mask(hdmi, hdmi->driver_data->reg_hdmitx_config_ofs,
			0 | HDMITX_SW_HPD, HDMI_YUV420_MODE | HDMITX_SW_HPD);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			0, C444_C422_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			0, C422_C420_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			C422_C420_CONFIG_BYPASS, C422_C420_CONFIG_BYPASS);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			0, C422_C420_CONFIG_OUT_CB_OR_CR);
		mtk_hdmi_mask(hdmi, VID_OUT_FORMAT,
			0, OUTPUT_FORMAT_DEMUX_420_ENABLE);
	}
}

bool mtk_hdmi_tmds_over_340M(struct mtk_hdmi *hdmi)
{
	unsigned long pixel_clk, tmds_clk;

	pixel_clk = hdmi->mode.clock * 1000;//in HZ

/* TMDS clk frequency */
	if (hdmi->color_depth == HDMI_8_BIT)
		tmds_clk = pixel_clk;
	else if (hdmi->color_depth == HDMI_10_BIT)
		tmds_clk = pixel_clk * 5 / 4; // *1.25
	else if (hdmi->color_depth == HDMI_12_BIT)
		tmds_clk = pixel_clk * 3 / 2; // *1.5
	else if (hdmi->color_depth == HDMI_16_BIT)
		tmds_clk = pixel_clk * 2; // *2
	else {
		HDMI_LOG("%s, unknown color depth\n", __func__);
		return -EINVAL;
	}

	if (hdmi->csp == HDMI_COLORSPACE_YUV420)
		tmds_clk = tmds_clk / 2;

	if (tmds_clk >= 340000000) {
		HDMI_LOG("TMDS over 340M!\n");
		return true;
	}
	HDMI_LOG("TMDS under 340M\n");
	return false;
}

static inline void mtk_hdmi_enable_scrambling(
	struct mtk_hdmi *hdmi, bool enable)
{
	/* DDC write scrambing enable */
	//drm_scdc_set_scrambling(hdmi->ddc_adpt, enable);
	udelay(100);

	if (enable == true)
		mtk_hdmi_mask(hdmi, TOP_CFG00, SCR_ON | HDMI2_ON,
			SCR_ON | HDMI2_ON);
	else
		mtk_hdmi_mask(hdmi, TOP_CFG00, SCR_OFF | HDMI2_OFF,
			SCR_ON | HDMI2_ON);
}

static inline void mtk_hdmi_high_tmds_clock_ratio(
	struct mtk_hdmi *hdmi, bool enable)
{
	/* DDC write scrambing enable */
	//drm_scdc_set_high_tmds_clock_ratio(hdmi->ddc_adpt, enable);
	/* hw set high bit clock ratio*/
	mtk_tmds_high_bit_clk_ratio(hdmi, enable);
}

//can.zeng done -> void vBlackHDMIOnly(),void vUnBlackHDMIOnly()
void mtk_hdmi_hw_vid_black(struct mtk_hdmi *hdmi, bool black)
{
	if (black == true)
		mtk_hdmi_mask(hdmi, TOP_VMUTE_CFG1, REG_VMUTE_EN, REG_VMUTE_EN);
	else
		mtk_hdmi_mask(hdmi, TOP_VMUTE_CFG1, 0, REG_VMUTE_EN);
}

//can.zeng modification done->void MuteHDMIAudio(void)
void mtk_hdmi_hw_aud_mute(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();

	if (mtk_hdmi_read(hdmi, AIP_CTRL) & DSD_EN)
		mtk_hdmi_mask(hdmi, AIP_TXCTRL,
			DSD_MUTE_DATA | AUD_MUTE_FIFO_EN,
			DSD_MUTE_DATA | AUD_MUTE_FIFO_EN);
	else
		mtk_hdmi_mask(hdmi, AIP_TXCTRL,	AUD_MUTE_FIFO_EN,
		AUD_MUTE_FIFO_EN);
}

//can.zeng modification done->void UnMuteHDMIAudio(void)
void mtk_hdmi_hw_aud_unmute(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, AIP_TXCTRL, AUD_MUTE_DIS, AUD_MUTE_FIFO_EN);
}

void mtk_hdmi_AV_mute(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_hw_vid_black(hdmi, true);
}

void mtk_hdmi_AV_unmute(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_aud_unmute(hdmi);
	mtk_hdmi_hw_vid_black(hdmi, false);
}

//can.zeng modification done-> void vResetHDMI(char bRst)
static void mtk_hdmi_hw_reset(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();
	mtk_hdmi_mask(hdmi, hdmi->driver_data->reg_hdmitx_config_ofs,
		0x0 << HDMITX_SW_RSTB_SHIFT, HDMITX_SW_RSTB);
	udelay(1);
	mtk_hdmi_mask(hdmi, hdmi->driver_data->reg_hdmitx_config_ofs,
		0x1 << HDMITX_SW_RSTB_SHIFT, HDMITX_SW_RSTB);
}

//can.zeng modification done->void vEnableHdmiMode(char bOn)
static void mtk_hdmi_enable_hdmi_mode(struct mtk_hdmi *hdmi, bool enable)
{
	if (enable == true) {
		mtk_hdmi_mask(hdmi, TOP_CFG00, HDMI_MODE_HDMI, HDMI_MODE_HDMI);
		HDMI_LOG("HDMI Mode\n");
	} else {
		mtk_hdmi_mask(hdmi, TOP_CFG00, HDMI_MODE_DVI, HDMI_MODE_HDMI);
		HDMI_LOG("DVI Mode\n");
	}
}

//can.zeng modification done->bool fgTVisHDMI(void)
static bool mtk_hdmi_sink_is_hdmi_device(struct mtk_hdmi *hdmi)
{
	if (hdmi->dvi_mode == true)
		return false;
	else
		return true;
}

//can.zeng modification done->void vEnableDeepColor()
static void mtk_hdmi_set_deep_color(struct mtk_hdmi *hdmi,
	bool is_hdmi_sink)
{
	unsigned int deep_color = 0;

	if (hdmi->csp == HDMI_COLORSPACE_YUV422) {
		//ycbcr422 12bit not deep color
		deep_color = DEEPCOLOR_MODE_8BIT;
		HDMI_LOG("YUV422 not DEEP COLOR\n");
	} else {
		if (hdmi->color_depth == HDMI_8_BIT) {
			deep_color = DEEPCOLOR_MODE_8BIT;
			HDMI_LOG("not DEEP COLOR\n");
		} else if (hdmi->color_depth == HDMI_10_BIT) {
			deep_color = DEEPCOLOR_MODE_10BIT;
			HDMI_LOG("DEEP COLOR 10-bit\n");
		} else if (hdmi->color_depth == HDMI_12_BIT) {
			deep_color = DEEPCOLOR_MODE_12BIT;
			HDMI_LOG("DEEP COLOR 12-bit\n");
		} else if (hdmi->color_depth == HDMI_16_BIT) {
			deep_color = DEEPCOLOR_MODE_16BIT;
			HDMI_LOG("DEEP COLOR 16-bit\n");
		} else {
			HDMI_LOG("color depth err\n");
			WARN_ON(1);
		}
	}
	mtk_hdmi_mask(hdmi, TOP_CFG00, deep_color, DEEPCOLOR_MODE_MASKBIT);

	/* GCP */
	mtk_hdmi_mask(hdmi, TOP_CFG00, 0, DEEPCOLOR_PAT_EN);
	if ((is_hdmi_sink == true) && (deep_color != DEEPCOLOR_MODE_8BIT))
		mtk_hdmi_mask(hdmi, TOP_MISC_CTLR, DEEP_COLOR_ADD,
			DEEP_COLOR_ADD);
	else
		mtk_hdmi_mask(hdmi, TOP_MISC_CTLR, 0, DEEP_COLOR_ADD);
}

//can.zeng done->void vHalSendAudioInfoFrame()
static void mtk_hdmi_hw_audio_infoframe(
	struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{
	enum hdmi_infoframe_type frame_type;
	u8 frame_ver;
	u8 frame_len;
	u8 checksum;

	frame_type = buffer[0];
	frame_ver = buffer[1];
	frame_len = buffer[2];
	checksum = buffer[3];

	HDMI_LOG("frame_type:0x%x,version:0x%x,length:0x%x,checksum:0x%x\n",
		frame_type, frame_ver, frame_len, checksum);

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AUD_DIS_WR | AUD_DIS,
		AUD_EN_WR | AUD_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AUD_RPT_DIS, AUD_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_AIF_HEADER, (frame_len << 16) +
		(frame_ver << 8) + (frame_type << 0));
	mtk_hdmi_write(hdmi, TOP_AIF_PKT00,
		(buffer[6] << 24) + (buffer[5] << 16) +
		(buffer[4] << 8) + (buffer[3] << 0));
	mtk_hdmi_write(hdmi, TOP_AIF_PKT01,
		(buffer[8] << 8) + (buffer[7] << 0));
	mtk_hdmi_write(hdmi, TOP_AIF_PKT02, 0);
	mtk_hdmi_write(hdmi, TOP_AIF_PKT03, 0);

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AUD_RPT_EN, AUD_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AUD_EN_WR | AUD_EN,
		AUD_EN_WR | AUD_EN);

}

//can.zeng done->void vHalSendAVIInfoFrame(unsigned char *pr_bData)
static void mtk_hdmi_hw_avi_infoframe(
	struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{

	enum hdmi_infoframe_type frame_type = buffer[0];
	u8 frame_ver = buffer[1];
	u8 frame_len = buffer[2];
	u8 checksum = buffer[3];

	HDMI_LOG("frame_type:0x%x,version:0x%x,length:0x%x,checksum:0x%x\n",
		frame_type, frame_ver, frame_len, checksum);

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AVI_DIS_WR | AVI_DIS,
		AVI_EN_WR | AVI_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AVI_RPT_DIS, AVI_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_AVI_HEADER, (buffer[2] << 16) +
		(buffer[1] << 8) + (buffer[0] << 0));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT00,
		(buffer[6] << 24) + (buffer[5] << 16) +
		(buffer[4] << 8) + (buffer[3] << 0));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT01,
		(buffer[9] << 16) + (buffer[8] << 8) +
		(buffer[7] << 0));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT02,
		(buffer[13] << 24) + (buffer[12] << 16) +
		(buffer[11] << 8) + (buffer[10] << 0));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT03,
		(buffer[16] << 16) + (buffer[15] << 8) +
		(buffer[14] << 0));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT04, 0);
	mtk_hdmi_write(hdmi, TOP_AVI_PKT05, 0);

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AVI_RPT_EN, AVI_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AVI_EN_WR | AVI_EN,
		AVI_EN_WR | AVI_EN);

}

static void mtk_hdmi_hw_spd_infoframe(struct mtk_hdmi *hdmi, u8 *buffer,
					u8 len)
{
	enum hdmi_infoframe_type frame_type = buffer[0];
	u8 frame_ver = buffer[1];
	u8 frame_len = buffer[2];
	u8 checksum = buffer[3];

	HDMI_LOG("frame_type:0x%x,version:0x%x,length:0x%x,checksum:0x%x\n",
		frame_type, frame_ver, frame_len, checksum);

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, SPD_DIS_WR | SPD_DIS,
		SPD_EN_WR | SPD_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, SPD_RPT_DIS, SPD_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_SPDIF_HEADER, (buffer[2] << 16) +
		(buffer[1] << 8) + (buffer[0] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT00,
		(buffer[6] << 24) + (buffer[5] << 16) +
		(buffer[4] << 8) + (buffer[3] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT01,
		(buffer[9] << 16) + (buffer[8] << 8) +
		(buffer[7] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT02,
		(buffer[13] << 24) + (buffer[12] << 16) +
		(buffer[11] << 8) + (buffer[10] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT03,
		(buffer[16] << 16) + (buffer[15] << 8) +
		(buffer[14] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT04,
		(buffer[20] << 24) + (buffer[19] << 16) +
		(buffer[18] << 8) + (buffer[17] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT05,
		(buffer[23] << 16) + (buffer[22] << 8) +
		(buffer[21] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT06,
		(buffer[27] << 24) + (buffer[26] << 16) +
		(buffer[25] << 8) + (buffer[24] << 0));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT07,
		(buffer[30] << 16) + (buffer[29] << 8) +
		(buffer[28] << 0));

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, SPD_RPT_EN, SPD_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, SPD_EN_WR | SPD_EN,
		SPD_EN_WR | SPD_EN);

}

/*for 3D and standard DV info;
 *low-latency DV use DV VSIF
 */
 //can.zeng todo verify the buffer length
static void mtk_hdmi_hw_h14b_vsif(
	struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{

	enum hdmi_infoframe_type frame_type = buffer[0];
	u8 frame_ver = buffer[1];
	u8 frame_len = buffer[2];
	u8 checksum = buffer[3];

	HDMI_LOG("frame_type:0x%x,version:0x%x,length:0x%x,checksum:0x%x\n",
		frame_type, frame_ver, frame_len, checksum);

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, VSIF_DIS | VSIF_DIS_WR,
		VSIF_EN | VSIF_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, VSIF_RPT_DIS, VSIF_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_VSIF_HEADER,
		(buffer[2] << 16) + (buffer[1] << 8) +
		(buffer[0] << 0));

	mtk_hdmi_write(hdmi, TOP_VSIF_PKT00,
		(buffer[6] << 24) + (buffer[5] << 16) +
		(buffer[4] << 8) + (buffer[3] << 0));
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT01,
		(buffer[8] << 8) + (buffer[7] << 0));

	mtk_hdmi_write(hdmi, TOP_VSIF_PKT02, 0);
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT03, 0);
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT04, 0);
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT05, 0);
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT06, 0);
	mtk_hdmi_write(hdmi, TOP_VSIF_PKT07, 0);

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, VSIF_RPT_EN, VSIF_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, VSIF_EN | VSIF_EN_WR,
		VSIF_EN | VSIF_EN_WR);
}

//can.zeng todo verify, which TOP_GEN_PCK to be used?
static void mtk_hdmi_hw_hf_vsif(
	struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{
	unsigned char bHFVS_CHSUM = 0;
	unsigned char bData1 = 0, bData2 = 0, bData3 = 0,
		bData4 = 0, bData5 = 0;
	unsigned int u4Data;
	bool fgEnable;

	fgEnable = hdmi->ALLM_enabled &
		_HdmiSinkAvCap.u1_sink_allm_support;

	HDMI_LOG("ALLM enabled:%d, support:%d\n",
		hdmi->ALLM_enabled, _HdmiSinkAvCap.u1_sink_allm_support);

	u4Data = mtk_hdmi_read(hdmi, TOP_INFO_EN) & MPEG_EN;

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, MPEG_EN_WR | MPEG_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, MPEG_RPT_EN);

	bData1 = *buffer;
	bData2 = *(buffer + 1);
	bData3 = *(buffer + 2);
	bData4 = *(buffer + 3);
	bData5 = *(buffer + 4);
	bHFVS_CHSUM = HFVS_TYPE + HFVS_VERS + HFVS_LEN;
	bHFVS_CHSUM += bData1;
	bHFVS_CHSUM += bData2;
	bHFVS_CHSUM += bData3;
	bHFVS_CHSUM += bData4;
	bHFVS_CHSUM += bData5;
	bHFVS_CHSUM = 0x100 - bHFVS_CHSUM;

	mtk_hdmi_write(hdmi, TOP_MPEG_HEADER, (HFVS_LEN << 16) +
		(HFVS_VERS << 8) + (HFVS_TYPE << 0));
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT00,
			  (bData3 << 24) + (bData2 << 16) + (bData1 << 8) +
			  (bHFVS_CHSUM << 0));
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT01, (bData5 << 8) + (bData4 << 0));
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT02, 0);
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT03, 0);
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT04, 0);
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT05, 0);
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT06, 0);
	mtk_hdmi_write(hdmi, TOP_MPEG_PKT07, 0);
	if (fgEnable || u4Data) {
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, MPEG_RPT_EN, MPEG_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, MPEG_EN_WR | MPEG_EN,
		MPEG_EN_WR | MPEG_EN);
	}
}

//can.zeng modification done
static int mtk_hdmi_setup_audio_infoframe(struct mtk_hdmi *hdmi)
{
	struct hdmi_codec_params *params = &hdmi->aud_param.codec_params;
	struct hdmi_audio_infoframe frame;
	u8 buffer[14];
	ssize_t err;

	HDMI_LOG("type:%d,version:%d,length:%d,channels:%d,coding_type:%d\n",
		params->cea.type,
		params->cea.version,
		params->cea.length,
		params->cea.channels,
		params->cea.coding_type);

	HDMI_LOG("sample_size:%d,sample_frequency:%d,coding_type_ext:%d,channel_allocation:%d\n",
		params->cea.sample_size,
		params->cea.sample_frequency,
		params->cea.coding_type_ext,
		params->cea.channel_allocation);

	memcpy(&frame, &hdmi->aud_param.codec_params.cea, sizeof(struct hdmi_audio_infoframe));

	err = hdmi_audio_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		HDMI_LOG("Failed to pack audio infoframe: %zd\n", err);
		return err;
	}

	mtk_hdmi_hw_audio_infoframe(hdmi, buffer, sizeof(buffer));
	return 0;
}

#define H14bVSIF_DV_LENGTH  0x18

//can.zeng todo add special process:
//1: 4K defined in HDMI 1.4 spec
//2: 3D defined in HDMI 1.4 spec
//3: DV indicated by HDMI14b VSIF
int mtk_hdmi_setup_h14b_vsif(struct mtk_hdmi *hdmi,
						struct drm_display_mode *mode)
{
	struct hdmi_vendor_infoframe frame;
	u8 buffer[22];
	ssize_t err;
	unsigned char check_sum;
	int i;

	memset(&frame, 0, sizeof(frame));
	err = drm_hdmi_vendor_infoframe_from_display_mode(
		&frame, &hdmi->conn, mode);
	if (err) {
		HDMI_LOG("Failed to get VSIF from mode: %zd\n", err);
		return err;
	}

	err = hdmi_vendor_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		HDMI_LOG("Failed to pack VSIF: %zd\n", err);
		return err;
	}

	//special process for DV H14b Vsif
	if (_fgDVHdrEnable) {
		buffer[2] = H14bVSIF_DV_LENGTH;
		buffer[3] = 0; // firstly, make checksum = 0
		/* re-calculate checksum */
		for (i = 0, check_sum = 0; i < sizeof(buffer); i++)
			check_sum += buffer[i];
		check_sum = 0x100 - check_sum;
		buffer[3] = check_sum;
	}

	mtk_hdmi_hw_h14b_vsif(hdmi, buffer, sizeof(buffer));
	return 0;
}

//can.zeng todo function'implementation
int mtk_hdmi_setup_hf_vsif(struct mtk_hdmi *hdmi,
	struct drm_display_mode *mode)
{
	u8 buffer[5];
	bool allmenable;

	allmenable = hdmi->ALLM_enabled &
		_HdmiSinkAvCap.u1_sink_allm_support;

	buffer[0] = 0xD8;//IEEE
	buffer[1] = 0x5D;
	buffer[2] = 0xC4;
	buffer[3] = 0x01;//Version
	if (allmenable)
		buffer[4] = 0x02;
	else
		buffer[4] = 0x0;

	mtk_hdmi_hw_hf_vsif(hdmi, buffer, sizeof(buffer));

	return 0;
}

//can.zeng modification done->void vAudioPacketOff(unsigned char bOn)
void mtk_hdmi_hw_send_aud_packet(struct mtk_hdmi *hdmi, bool enable)
{
	if (enable == false) {
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, AUD_PACKET_DROP,
			AUD_PACKET_DROP);
		HDMI_LOG("audio packet Off\n");
	} else {
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, 0, AUD_PACKET_DROP);
		HDMI_LOG("audio packet On\n");
	}
}

//can.zeng modification done-> void vSend_AVMUTE(void)
void mtk_hdmi_hw_send_av_mute(struct mtk_hdmi *hdmi)
{
	/*GCP packet */
	HDMI_FUNC();

	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_SET_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, CP_EN | CP_EN_WR);

	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, CP_SET_MUTE_EN, CP_SET_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, CP_RPT_EN, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, CP_EN | CP_EN_WR, CP_EN | CP_EN_WR);

}
EXPORT_SYMBOL(mtk_hdmi_hw_send_av_mute);

//can.zeng modification done-> void vSend_AVUNMUTE(void)
void mtk_hdmi_hw_send_av_unmute(struct mtk_hdmi *hdmi)
{
	/*GCP packet */
	HDMI_FUNC();

	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_SET_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, CP_EN | CP_EN_WR);

	mtk_hdmi_mask(hdmi, TOP_CFG01, CP_CLR_MUTE_EN, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_SET_MUTE_DIS);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, CP_RPT_EN, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, CP_EN | CP_EN_WR, CP_EN | CP_EN_WR);

}
EXPORT_SYMBOL(mtk_hdmi_hw_send_av_unmute);

static void mtk_hdmi_hw_ncts_enable(struct mtk_hdmi *hdmi, bool enable)
{
	unsigned int data;

	data = mtk_hdmi_read(hdmi, AIP_CTRL);

	if (enable == false)
		data |= CTS_SW_SEL;
	else
		data &= ~CTS_SW_SEL;

	mtk_hdmi_write(hdmi, AIP_CTRL, data);
}

static void mtk_hdmi_hw_aud_set_channel_status(
	struct mtk_hdmi *hdmi, u8 *channel_status)
{
	HDMI_LOG("Channel Status Bit: %x, %x, %x, %x, %x, %x, %x\n",
		channel_status[0], channel_status[1], channel_status[2],
		channel_status[3], channel_status[4], channel_status[5],
		channel_status[6]);

	/* actually, only the first 5 or 7 bytes of Channel Status
	 * contain useful information
	 */
	mtk_hdmi_write(hdmi, AIP_I2S_CHST0,
		(channel_status[3] << 24) + (channel_status[2] << 16) +
		(channel_status[1] << 8) + (channel_status[0] << 0));
	mtk_hdmi_write(hdmi, AIP_I2S_CHST1,
		(channel_status[6] << 16) + (channel_status[5] << 8) +
		(channel_status[4] << 0));
}

struct hdmi_acr_n {
	unsigned int clock;
	unsigned int n[3];
};

/* Recommended N values from HDMI specification, tables 7-1 to 7-3 */
static const struct hdmi_acr_n hdmi_rec_n_table[] = {
	/* Clock, N: 32kHz 44.1kHz 48kHz */
	{  25175, {  4576,  7007,  6864 } },
	{  74176, { 11648, 17836, 11648 } },
	{ 148352, { 11648,  8918,  5824 } },
	{ 296703, {  5824,  4459,  5824 } },
	{ 297000, {  3072,  4704,  5120 } },
	{      0, {  4096,  6272,  6144 } }, /* all other TMDS clocks */
};

/**
 * hdmi_recommended_n() - Return N value recommended by HDMI specification
 * @freq: audio sample rate in Hz
 * @clock: rounded TMDS clock in kHz
 */
static unsigned int hdmi_recommended_n(unsigned int freq, unsigned int clock)
{
	const struct hdmi_acr_n *recommended;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_rec_n_table) - 1; i++) {
		if (clock == hdmi_rec_n_table[i].clock)
			break;
	}
	recommended = hdmi_rec_n_table + i;

	switch (freq) {
	case 32000:
		return recommended->n[0];
	case 44100:
		return recommended->n[1];
	case 48000:
		return recommended->n[2];
	case 88200:
		return recommended->n[1] * 2;
	case 96000:
		return recommended->n[2] * 2;
	case 176400:
		return recommended->n[1] * 4;
	case 192000:
		return recommended->n[2] * 4;
	default:
		return (128 * freq) / 1000;
	}
}

static unsigned int hdmi_mode_clock_to_hz(unsigned int clock)
{
	switch (clock) {
	case 25175:
		return 25174825;	/* 25.2/1.001 MHz */
	case 74176:
		return 74175824;	/* 74.25/1.001 MHz */
	case 148352:
		return 148351648;	/* 148.5/1.001 MHz */
	case 296703:
		return 296703297;	/* 297/1.001 MHz */
	default:
		return clock * 1000;
	}
}

static unsigned int hdmi_expected_cts(unsigned int audio_sample_rate,
				      unsigned int tmds_clock, unsigned int n)
{
	return DIV_ROUND_CLOSEST_ULL((u64)hdmi_mode_clock_to_hz(tmds_clock) * n,
				     128 * audio_sample_rate);
}

static void mtk_hdmi_hw_aud_set_ncts(struct mtk_hdmi *hdmi,
				     unsigned int sample_rate,
				     unsigned int clock)
{
	unsigned int n, ncts;

	n = hdmi_recommended_n(sample_rate, clock);
	ncts = hdmi_expected_cts(sample_rate, clock, n);

	HDMI_LOG("sample_rate=%u, clock=%d, ncts=%u, n=%u\n",
		sample_rate, clock, n, ncts);

	mtk_hdmi_write(hdmi, AIP_N_VAL, n);
	mtk_hdmi_write(hdmi, AIP_CTS_SVAL, ncts);

}

static int mtk_hdmi_video_change_vpll(struct mtk_hdmi *hdmi, u32 clock)
{
	unsigned long rate;
	int ret;

	/* The DPI driver already should have set TVDPLL to the correct rate */
	ret = clk_set_rate(hdmi->clk[MTK_HDMI_CLK_HDMI_TXPLL], clock);
	if (ret) {
		HDMI_LOG("Failed to set PLL to %u Hz: %d\n", clock, ret);
		return ret;
	}

	rate = clk_get_rate(hdmi->clk[MTK_HDMI_CLK_HDMI_TXPLL]);

	if (DIV_ROUND_CLOSEST(rate, 1000) != DIV_ROUND_CLOSEST(clock, 1000))
		HDMI_LOG("Want PLL %u Hz, got %lu Hz\n", clock, rate);
	else
		HDMI_LOG("Want PLL %u Hz, got %lu Hz\n", clock, rate);

	return 0;
}

static int mtk_hdmi_aud_enable_packet(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_hw_send_aud_packet(hdmi, enable);
	return 0;
}

static int mtk_hdmi_aud_on_off_hw_ncts(struct mtk_hdmi *hdmi, bool on)
{
	mtk_hdmi_hw_ncts_enable(hdmi, on);
	return 0;
}

//can.zeng done->void vSetHdmiDsdConfig()
static void mtk_hdmi_audio_dsd_config(struct mtk_hdmi *hdmi,
	unsigned char chNum, bool dsd_bypass)
{
	HDMI_FUNC();

	mtk_hdmi_mask(hdmi, AIP_CTRL, DSD_EN, SPDIF_EN | DSD_EN | HBRA_ON);
	mtk_hdmi_mask(hdmi, AIP_TXCTRL, DSD_MUTE_DATA, DSD_MUTE_DATA);
	if (dsd_bypass == true)
		mtk_hdmi_write(hdmi, TOP_AUD_MAP, 0x75316420);
	/* 0x13570246 */
	else
		mtk_hdmi_write(hdmi, TOP_AUD_MAP, 0x04230150);
	/* 0 FL;1 SL;2 CENT;3 FR;4 SR;5 LFE 0x32400510 */

	/* rxtx bypass */
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0, I2S2DSD_EN);
}

//can.zeng done->void vSetHdmi2I2SfifoMap(unsigned int bFifoMap)
static inline void mtk_hdmi_hw_i2s_fifo_map(struct mtk_hdmi *hdmi,
	unsigned int fifo_mapping)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, fifo_mapping,
		FIFO3_MAP |	FIFO2_MAP | FIFO1_MAP | FIFO0_MAP);
}

//can.zeng done-> void vSetHdmi2I2SCH(unsigned int bCH)
static inline void mtk_hdmi_hw_i2s_ch_number(struct mtk_hdmi *hdmi,
	unsigned int chNum)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL, chNum << I2S_EN_SHIFT, I2S_EN);
}


//can.zeng done->void vSetHdmiI2SChNum()
static void mtk_hdmi_hw_i2s_ch_mapping(struct mtk_hdmi *hdmi,
	unsigned char chNum, unsigned char mapping)
{
	unsigned int bData, bData1, bData2, bData3;

	if (chNum == 2) {	/* I2S 2ch */
		bData = 0x1;	/* 2ch data */
		bData1 = 0x50;	/* data0 */

	} else if ((chNum == 3) || (chNum == 4)) {	/* I2S 2ch */
		if ((chNum == 4) && (mapping == 0x08))
			bData = 0x3;	/* 4ch data */
		else
			bData = 0x03;	/* 4ch data */

		bData1 = 0x50;	/* data0 */

	} else if ((chNum == 6) || (chNum == 5)) {	/* I2S 5.1ch */
		if ((chNum == 6) && (mapping == 0x0E)) {
			bData = 0xf;	/* 6.0 ch data */
			bData1 = 0x50;	/* data0 */
		} else {
			bData = 0x7;	/* 5.1ch data, 5/0ch */
			bData1 = 0x50;	/* data0 */
		}

	} else if (chNum == 8) {	/* I2S 5.1ch */
		bData = 0xf;	/* 7.1ch data */
		bData1 = 0x50;	/* data0 */
	} else if (chNum == 7) {	/* I2S 6.1ch */
		bData = 0xf;	/* 6.1ch data */
		bData1 = 0x50;	/* data0 */
	} else {
		bData = 0x01;	/* 2ch data */
		bData1 = 0x50;	/* data0 */
	}

	bData2 = 0xc6;
	bData3 = 0xfa;

	mtk_hdmi_hw_i2s_fifo_map(hdmi, (MAP_SD3 << 6) | (MAP_SD2 << 4) |
		(MAP_SD1 << 2) | (MAP_SD0 << 0));
	mtk_hdmi_hw_i2s_ch_number(hdmi, bData);

	if (chNum == 2)
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, LAYOUT0, LAYOUT1);
	else
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, LAYOUT1, LAYOUT1);

}

//can.zeng done->void vSetHdmiI2SDataFmt(unsigned char bFmt)
static void mtk_hdmi_i2s_data_fmt(struct mtk_hdmi *hdmi,
	unsigned char fmt)
{
	unsigned int u4Data;

	u4Data = mtk_hdmi_read(hdmi, AIP_I2S_CTRL);
	HDMI_LOG("fmt:%d, AIP_I2S_CTRL1 value:0x%x\n", fmt, u4Data);
	u4Data &= ~(WS_HIGH | I2S_1ST_BIT_NOSHIFT | JUSTIFY_RIGHT);

	switch (fmt) {
	case HDMI_I2S_MODE_RJT_24BIT:
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT | JUSTIFY_RIGHT);
		break;

	case HDMI_I2S_MODE_RJT_16BIT:
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT | JUSTIFY_RIGHT);
		break;

	case HDMI_I2S_MODE_LJT_24BIT:
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT);
		break;

	case HDMI_I2S_MODE_LJT_16BIT:
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT);
		break;

	case HDMI_I2S_MODE_I2S_24BIT:
		break;

	case HDMI_I2S_MODE_I2S_16BIT:
		break;

	default:
		break;
	}
	mtk_hdmi_write(hdmi, AIP_I2S_CTRL, u4Data);

	u4Data = mtk_hdmi_read(hdmi, AIP_I2S_CTRL);
	HDMI_LOG("AIP_I2S_CTRL2 value:0x%x\n", u4Data);
}

//can.zeng done->void vSetHdmiI2SSckEdge(unsigned int bEdge)
static inline void mtk_hdmi_i2s_sck_edge(
	struct mtk_hdmi *hdmi, unsigned int edge)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, edge, SCK_EDGE_RISE);
}

//can.zeng done->void vSetHdmiI2SCbitOrder(unsigned int bCbit)
static inline void mtk_hdmi_i2s_cbit_order(
	struct mtk_hdmi *hdmi, unsigned int cbit)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, cbit, CBIT_ORDER_SAME);
}

//can.zeng done->void vSetHdmiI2SVbit(unsigned int bVbit)
static inline void mtk_hdmi_i2s_vbit(
	struct mtk_hdmi *hdmi, unsigned int vbit)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, vbit, VBIT_COM);
}

//can.zeng done->void vSetHdmiI2SDataDir(unsigned int bDataDir)
static inline void mtk_hdmi_i2s_data_direction(
	struct mtk_hdmi *hdmi, unsigned int data_dir)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, data_dir, DATA_DIR_LSB);
}

//can.zeng done->void vEnableInputAudioType(unsigned int bspdifi2s)
static inline void mtk_hdmi_hw_audio_type(
	struct mtk_hdmi *hdmi, unsigned int spdif_i2s)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL, spdif_i2s << SPDIF_EN_SHIFT, SPDIF_EN);
}

//can.zeng done->unsigned char bGetChannelMapping(void)
//can.zeng done->void vUpdateAudSrcChType(unsigned char u1SrcChType)
static unsigned char mtk_hdmi_get_i2s_ch_mapping(
	struct mtk_hdmi *hdmi, unsigned char channel_type)
{
	unsigned char FR = 0, FL = 0, FC = 0, LFE = 0;
	unsigned char RR = 0, RL = 0, RRC = 0, RLC = 0, RC = 0;
	unsigned char ch_number = 0;
	unsigned char ChannelMap = 0x00;

	switch (channel_type) {
	case HDMI_AUD_CHAN_TYPE_1_0:
	case HDMI_AUD_CHAN_TYPE_2_0:
		FR = 1;
		FL = 1;
		LFE = 0;
		ch_number = 2;
		break;

	case HDMI_AUD_CHAN_TYPE_1_1:
	case HDMI_AUD_CHAN_TYPE_2_1:
		FR = 1;
		FL = 1;
		LFE = 1;
		ch_number = 3;
		break;

	case HDMI_AUD_CHAN_TYPE_3_0:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 0;
		ch_number = 3;
		break;

	case HDMI_AUD_CHAN_TYPE_3_0_LRS:
		FR = 1;
		FL = 1;
		RR = 1;
		RL = 1;
		LFE = 0;
		ch_number = 4;
		break;

	case HDMI_AUD_CHAN_TYPE_3_1_LRS:
		FR = 1;
		FL = 1;
		FC = 0;
		LFE = 1;
		RR = 1;
		RL = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_4_0_CLRS:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_4_1_CLRS:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		ch_number = 6;
		break;

	case HDMI_AUD_CHAN_TYPE_3_1:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 1;
		ch_number = 4;
		break;

	case HDMI_AUD_CHAN_TYPE_4_0:
		FR = 1;
		FL = 1;
		RR = 1;
		RL = 1;
		LFE = 0;
		ch_number = 4;
		break;

	case HDMI_AUD_CHAN_TYPE_4_1:
		FR = 1;
		FL = 1;
		RR = 1;
		RL = 1;
		LFE = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_5_0:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_5_1:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		ch_number = 6;
		break;

	case HDMI_AUD_CHAN_TYPE_6_0:
	case HDMI_AUD_CHAN_TYPE_6_0_CS:
	case HDMI_AUD_CHAN_TYPE_6_0_CH:
	case HDMI_AUD_CHAN_TYPE_6_0_OH:
	case HDMI_AUD_CHAN_TYPE_6_0_CHR:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		RC = 1;
		ch_number = 6;
		break;

	case HDMI_AUD_CHAN_TYPE_6_1:
	case HDMI_AUD_CHAN_TYPE_6_1_CS:
	case HDMI_AUD_CHAN_TYPE_6_1_CH:
	case HDMI_AUD_CHAN_TYPE_6_1_OH:
	case HDMI_AUD_CHAN_TYPE_6_1_CHR:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		RC = 1;
		ch_number = 7;
		break;

	case HDMI_AUD_CHAN_TYPE_7_0:
	case HDMI_AUD_CHAN_TYPE_7_0_LH_RH:
	case HDMI_AUD_CHAN_TYPE_7_0_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_7_0_LC_RC:
	case HDMI_AUD_CHAN_TYPE_7_0_LW_RW:
	case HDMI_AUD_CHAN_TYPE_7_0_LSD_RSD:
	case HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS:
	case HDMI_AUD_CHAN_TYPE_7_0_LHS_RHS:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_CH:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_OH:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_CH_OH:
	case HDMI_AUD_CHAN_TYPE_7_0_CH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_OH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_8_0_LH_RH_CS:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		RRC = 1;
		RLC = 1;
		ch_number = 7;
		break;

	case HDMI_AUD_CHAN_TYPE_7_1:
	case HDMI_AUD_CHAN_TYPE_7_1_LH_RH:
	case HDMI_AUD_CHAN_TYPE_7_1_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_7_1_LC_RC:
	case HDMI_AUD_CHAN_TYPE_7_1_LW_RW:
	case HDMI_AUD_CHAN_TYPE_7_1_LSD_RSD:
	case HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS:
	case HDMI_AUD_CHAN_TYPE_7_1_LHS_RHS:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_CH:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_OH:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_CH_OH:
	case HDMI_AUD_CHAN_TYPE_7_1_CH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_OH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS_LSR_RSR:
		FR = 1;
		FL = 1;
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		RRC = 1;
		RLC = 1;
		ch_number = 8;
		break;

	default:
		FR = 1;
		FL = 1;
		ch_number = 2;
		break;
	}


	switch (ch_number) {
	case 8:
		break;

	case 7:
		break;

	case 6:
		if ((FR == 1) && (FL == 1) && (FC == 1) &&
			(RR == 1) && (RL == 1) && (RC == 1) &&
			(LFE == 0)) {
			/* 6.0 */
			ChannelMap = 0x0E;
		} else if ((FR == 1) && (FL == 1) && (FC == 1) &&
			(RR == 1) && (RL == 1) && (RC == 0) && (LFE == 1)) {
			/* 5.1 */
			ChannelMap = 0x0B;
		}
		break;

	case 5:
		break;

	case 4:
		if ((FR == 1) && (FL == 1) && (RR == 1) &&
		    (RL == 1) && (LFE == 0))
			ChannelMap = 0x08;
		else if ((FR == 1) && (FL == 1) && (FC == 1) &&
			(LFE == 1))
			ChannelMap = 0x03;
		break;

	case 3:
		if ((FR == 1) && (FL == 1) && (FC == 1))
			ChannelMap = 0x02;
		else if ((FR == 1) && (FL == 1) && (LFE == 1))
			ChannelMap = 0x01;
		break;

	case 2:
		if ((FR == 1) && (FL == 1))
			ChannelMap = 0x00;
		break;

	default:
		break;
	}

	HDMI_LOG("Ch_Num=%d, FR %d, FL %d, FC %d, LFE %d\n",
		ch_number, FR, FL, FC, LFE);
	HDMI_LOG("RR %d, RL %d, RRC %d, RLC %d, RC %d\n",
		RR, RL, RRC, RLC, RC);

	return ChannelMap;
}

//can.zeng done->void vSetChannelSwap(unsigned char u1SwapBit)
static inline void mtk_hdmi_hw_i2s_ch_swap(
	struct mtk_hdmi *hdmi, unsigned char SwapBit)
{
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, SwapBit << 20, 0x0F << 20);
}

//can.zeng done->void vSetHdmiHbrConfig(bool fgRxDsdBypass)
static void mtk_hdmi_hbr_config(
	struct mtk_hdmi *hdmi, bool dsd_bypass)
{
	HDMI_LOG("dsd_bypass:%d\n", dsd_bypass);
	if (dsd_bypass == true) {
		mtk_hdmi_mask(hdmi, AIP_CTRL,
			HBRA_ON, SPDIF_EN | DSD_EN | HBRA_ON);
		mtk_hdmi_mask(hdmi, AIP_CTRL, I2S_EN, I2S_EN);
	} else {
		mtk_hdmi_mask(hdmi, AIP_CTRL,
			SPDIF_EN, SPDIF_EN | DSD_EN | HBRA_ON);
		mtk_hdmi_mask(hdmi, AIP_CTRL, SPDIF_INTERNAL_MODULE,
			SPDIF_INTERNAL_MODULE);
		mtk_hdmi_mask(hdmi, AIP_CTRL, HBR_FROM_SPDIF, HBR_FROM_SPDIF);
		mtk_hdmi_mask(hdmi, AIP_CTRL, CTS_CAL_N4, CTS_CAL_N4);

		/* can.zeng todo verify what is following setting for */
		/* vWriteHdmiTOPCKMsk(0xe8, (0x1 << 11),
		 *	(0x1 << 12) | (0x1 << 11));
		 */
	}
}

//can.zeng done->void vSetHdmiSpdifConfig(void)
static inline void mtk_hdmi_hw_spdif_config(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, WR_1UI_UNLOCK, WR_1UI_LOCK);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, FS_UNOVERRIDE, FS_OVERRIDE_WRITE);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, WR_2UI_UNLOCK, WR_2UI_LOCK);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0x4 <<
		MAX_1UI_WRITE_SHIFT, MAX_1UI_WRITE);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0x9 <<
		MAX_2UI_WRITE_SHIFT, MAX_2UI_WRITE);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0x4 <<
		AUD_ERR_THRESH_SHIFT, AUD_ERR_THRESH);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, I2S2DSD_EN, I2S2DSD_EN);
}

//can.zeng modification done->void vSetHDMIAudioIn(void)
static int mtk_hdmi_aud_set_input(struct mtk_hdmi *hdmi)
{
	unsigned char ChMapping;

	HDMI_FUNC();

	mtk_hdmi_write(hdmi, TOP_AUD_MAP,
			  C_SD7 + C_SD6 + C_SD5 + C_SD4 +
			  C_SD3 + C_SD2 + C_SD1 + C_SD0);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0, 0x0F << 20);
	mtk_hdmi_mask(hdmi, AIP_CTRL, 0, SPDIF_EN | DSD_EN | HBRA_ON |
			 CTS_CAL_N4 | HBR_FROM_SPDIF | SPDIF_INTERNAL_MODULE);
	mtk_hdmi_mask(hdmi, AIP_TXCTRL, 0, DSD_MUTE_DATA | LAYOUT1);

	HDMI_LOG("aud_input_type:%d, aud_codec:%d, coding_type:%d\n",
		hdmi->aud_param.aud_input_type,
		hdmi->aud_param.aud_codec,
		hdmi->aud_param.codec_params.cea.coding_type);

	if (hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_I2S) {
		if ((hdmi->aud_param.codec_params.cea.coding_type == HDMI_AUDIO_CODING_TYPE_DTS_HD)
			|| (hdmi->aud_param.codec_params.cea.coding_type
			== HDMI_AUDIO_CODING_TYPE_MLP)) {
			mtk_hdmi_i2s_data_fmt(hdmi,
				hdmi->aud_param.aud_i2s_fmt);
			mtk_hdmi_hbr_config(hdmi, true);
		} else if (hdmi->aud_param.aud_codec == HDMI_AUDIO_CODING_TYPE_DSD) {
			mtk_hdmi_audio_dsd_config(hdmi,
				hdmi->aud_param.codec_params.channels, 0);
			mtk_hdmi_hw_i2s_ch_mapping(hdmi,
				hdmi->aud_param.codec_params.channels, 1);
		} else {
			mtk_hdmi_i2s_data_fmt(hdmi,
				hdmi->aud_param.aud_i2s_fmt);
			mtk_hdmi_i2s_sck_edge(hdmi, SCK_EDGE_RISE);
			mtk_hdmi_i2s_cbit_order(hdmi, CBIT_ORDER_SAME);
			mtk_hdmi_i2s_vbit(hdmi, VBIT_PCM);
			mtk_hdmi_i2s_data_direction(hdmi, DATA_DIR_MSB);
			mtk_hdmi_hw_audio_type(hdmi, HDMI_AUD_INPUT_I2S);
			ChMapping = mtk_hdmi_get_i2s_ch_mapping(hdmi,
				hdmi->aud_param.aud_input_chan_type);
			mtk_hdmi_hw_i2s_ch_mapping(hdmi,
				hdmi->aud_param.codec_params.channels,
				ChMapping);
			mtk_hdmi_hw_i2s_ch_swap(hdmi, LFE_CC_SWAP);
		}
	} else {
		if ((hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_SPDIF) &&
		((hdmi->aud_param.aud_codec == HDMI_AUDIO_CODING_TYPE_DTS_HD)
		|| (hdmi->aud_param.aud_codec == HDMI_AUDIO_CODING_TYPE_MLP))
		&& (hdmi->aud_param.codec_params.sample_rate == 768000)) {
			mtk_hdmi_hbr_config(hdmi, false);
		} else {
			mtk_hdmi_hw_spdif_config(hdmi);
			mtk_hdmi_hw_i2s_ch_mapping(hdmi, 2, 0);
		}
	}

	return 0;
}

static int mtk_hdmi_aud_set_sw_ncts(struct mtk_hdmi *hdmi,
				struct drm_display_mode *display_mode)
{
	unsigned int sample_rate = hdmi->aud_param.codec_params.sample_rate;

	mtk_hdmi_aud_on_off_hw_ncts(hdmi, false);

	mtk_hdmi_hw_aud_set_ncts(hdmi, sample_rate, display_mode->clock);

	return 0;
}

//can.zeng done->void vEnableAudio(unsigned int bOn)
static inline void mtk_hdmi_hw_audio_input_enable(
	struct mtk_hdmi *hdmi, unsigned int enable)
{
	if (enable == true)
		mtk_hdmi_mask(hdmi, AIP_CTRL, AUD_IN_EN, AUD_IN_EN);
	else
		mtk_hdmi_mask(hdmi, AIP_CTRL, 0x0 << AUD_IN_EN_SHIFT,
			AUD_IN_EN);
}

//void vAipCtrlInit(void)
static void mtk_hdmi_aip_ctrl_init(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL,
		AUD_SEL_OWRT | NO_MCLK_CTSGEN_SEL | CTS_REQ_EN,
		AUD_SEL_OWRT | NO_MCLK_CTSGEN_SEL | MCLK_EN | CTS_REQ_EN);
	mtk_hdmi_mask(hdmi, AIP_TPI_CTRL, TPI_AUDIO_LOOKUP_DIS,
		TPI_AUDIO_LOOKUP_EN);
}

//can.zeng modification done->void vResetAudioHDMI(unsigned char bRst)
void mtk_hdmi_audio_reset(struct mtk_hdmi *hdmi, bool rst)
{
	HDMI_FUNC();

	if (rst == true) {
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, RST4AUDIO |
			RST4AUDIO_FIFO | RST4AUDIO_ACR,
			RST4AUDIO | RST4AUDIO_FIFO | RST4AUDIO_ACR);
	} else {
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, 0,
			RST4AUDIO | RST4AUDIO_FIFO | RST4AUDIO_ACR);
	}
}

//can.zeng modification done-> vChgHDMIAudioOutput()
static int mtk_hdmi_aud_output_config(struct mtk_hdmi *hdmi,
				      struct drm_display_mode *display_mode)
{
	HDMI_FUNC();

	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_aud_enable_packet(hdmi, false);
	mtk_hdmi_audio_reset(hdmi, true);
	mtk_hdmi_aip_ctrl_init(hdmi);

	mtk_hdmi_aud_set_input(hdmi);

	mtk_hdmi_hw_aud_set_channel_status(hdmi,
			hdmi->aud_param.codec_params.iec.status);

	mtk_hdmi_setup_audio_infoframe(hdmi);

	mtk_hdmi_hw_audio_input_enable(hdmi, true);

	mtk_hdmi_audio_reset(hdmi, false);

	mtk_hdmi_aud_set_sw_ncts(hdmi, display_mode);

	udelay(25);
	mtk_hdmi_aud_on_off_hw_ncts(hdmi, true);

	mtk_hdmi_aud_enable_packet(hdmi, true);
	mtk_hdmi_hw_aud_unmute(hdmi);
	return 0;
}

int mtk_hdmi_setup_avi_infoframe(struct mtk_hdmi *hdmi,
					struct drm_display_mode *mode)
{
	struct hdmi_avi_infoframe frame;
	u8 buffer[17];
	ssize_t err;

	//can.zeng todo verify
	if (hdmi->conn.display_info.hdmi.scdc.supported == true) {
		//if support scdc, then the sink support HDMI2.0
		HDMI_LOG("HDMI2.x sink\n");
	}
	err = drm_hdmi_avi_infoframe_from_display_mode(&frame, &hdmi->conn, mode);

	if (err < 0) {
		dev_err(hdmi->dev,
			"Failed to get AVI infoframe from mode: %zd\n", err);
		return err;
	}

	frame.colorspace = hdmi->csp;
	frame.colorimetry = hdmi->colorimtery;
	frame.extended_colorimetry = hdmi->extended_colorimetry;
	frame.quantization_range = hdmi->quantization_range;
	frame.ycc_quantization_range = hdmi->ycc_quantization_range;

	//add information that not be included in drm_display_mode

	if (_fgDVHdrEnable) {
		//force DVHDR colorspace RGB
		frame.colorspace = HDMI_COLORSPACE_RGB;
		//standard DV quantization range must be full
		hdmi->quantization_range = HDMI_QUANTIZATION_RANGE_FULL;
		frame.quantization_range = HDMI_QUANTIZATION_RANGE_FULL;
	}

	if (_fgLowLatencyDVEnable) {
		//Lowlatency DV, ETHDR only send colorspace format ycbcr422
		HDMI_LOG("low latency DV, set_csp_depth=0x%x\n", YCBCR422_12bit);
		frame.colorspace = HDMI_COLORSPACE_YUV422;
		hdmi->set_csp_depth = YCBCR422_12bit;
	}

	if (_fgBT2020Enable) {
		HDMI_LOG("BT2020\n");
		frame.colorimetry = HDMI_COLORIMETRY_EXTENDED;
		hdmi->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020;
		frame.extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020;
	}

	err = hdmi_avi_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		HDMI_LOG(hdmi->dev, "Failed to pack AVI infoframe: %zd\n", err);
		return err;
	}

	mtk_hdmi_hw_avi_infoframe(hdmi, buffer, sizeof(buffer));
	return 0;
}

static int mtk_hdmi_setup_spd_infoframe(struct mtk_hdmi *hdmi,
					const char *vendor,
					const char *product)
{
	struct hdmi_spd_infoframe frame;
	u8 buffer[31];
	ssize_t err;

	err = hdmi_spd_infoframe_init(&frame, vendor, product);
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to initialize SPD infoframe: %zd\n",
			err);
		return err;
	}

	err = hdmi_spd_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack SDP infoframe: %zd\n", err);
		return err;
	}

	mtk_hdmi_hw_spd_infoframe(hdmi, buffer, sizeof(buffer));
	return 0;
}

static int mtk_hdmi_reset_colorspace_setting(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();

	hdmi->set_csp_depth = RGB444_8bit;
	hdmi->csp = HDMI_COLORSPACE_RGB;
	hdmi->color_depth = HDMI_8_BIT;
	hdmi->colorimtery = HDMI_COLORIMETRY_ITU_601;
	hdmi->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_RESERVED;
	hdmi->quantization_range = HDMI_QUANTIZATION_RANGE_LIMITED;
	hdmi->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;

	return 0;
}

static int mtk_hdmi_output_init(struct mtk_hdmi *hdmi)
{
	struct hdmi_audio_param *aud_param = &hdmi->aud_param;

	aud_param->aud_codec = HDMI_AUDIO_CODING_TYPE_PCM;
	aud_param->aud_sampe_size = HDMI_AUDIO_SAMPLE_SIZE_16;
	aud_param->aud_input_type = HDMI_AUD_INPUT_I2S;
	aud_param->aud_i2s_fmt = HDMI_I2S_MODE_I2S_24BIT;
	aud_param->aud_mclk = HDMI_AUD_MCLK_128FS;
	aud_param->aud_input_chan_type = HDMI_AUD_CHAN_TYPE_2_0;

	hdmi->hpd = HDMI_PLUG_OUT;
	mtk_hdmi_reset_colorspace_setting(hdmi);

	return 0;
}

static void mtk_hdmi_audio_enable(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();

	mtk_hdmi_aud_enable_packet(hdmi, true);
	hdmi->audio_enable = true;
}

static void mtk_hdmi_audio_disable(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();

	mtk_hdmi_aud_enable_packet(hdmi, false);
	hdmi->audio_enable = false;
}

static int mtk_hdmi_audio_set_param(struct mtk_hdmi *hdmi,
				    struct hdmi_audio_param *param)
{
	if (!hdmi->audio_enable) {
		HDMI_LOG("hdmi audio is in disable state!\n");
		return -EINVAL;
	}
	HDMI_LOG("codec:%d, input:%d, channel:%d, fs:%d\n",
		param->aud_codec, param->aud_input_type,
		param->aud_input_chan_type, param->codec_params.sample_rate);
	memcpy(&hdmi->aud_param, param, sizeof(*param));
	return mtk_hdmi_aud_output_config(hdmi, &hdmi->mode);
}

void mtk_hdmi_send_TMDS_configuration(struct mtk_hdmi *hdmi, unsigned char enscramble)
{
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	HDMI_LOG("enscramble = %d\n", enscramble);
	fgDDCDataWrite(ddc, RX_REG_SCRAMBLE >> 1, RX_REG_TMDS_CONFIG, 1,
		&enscramble);
}

//can.zeng modification done-> void vChgHDMIVideoResolution(void)
static void mtk_hdmi_change_video_resolution(struct mtk_hdmi *hdmi)
{
	bool is_over_340M = false;
	bool is_hdmi_sink = false;

	HDMI_FUNC();

	mtk_hdmi_hw_reset(hdmi);
	mtk_hdmi_set_sw_hpd(hdmi, true);
	mtk_hdmi_colorspace_setting(hdmi);
	udelay(2);

	mtk_hdmi_write(hdmi, HDCP_TOP_CTRL, 0x0);
	mtk_hdmi_en_hdcp_reauth_int(hdmi, true);
	mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
	mtk_hdmi_force_hdcp_hpd(hdmi);

	is_hdmi_sink = mtk_hdmi_sink_is_hdmi_device(hdmi);
	mtk_hdmi_set_deep_color(hdmi, is_hdmi_sink);
	mtk_hdmi_enable_hdmi_mode(hdmi, is_hdmi_sink);

	udelay(5);
	mtk_hdmi_hw_vid_black(hdmi, true);
	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_hw_send_av_unmute(hdmi);

	mtk_hdmi_mask(hdmi, TOP_CFG01, NULL_PKT_VSYNC_HIGH_EN,
		NULL_PKT_VSYNC_HIGH_EN | NULL_PKT_EN);

	is_over_340M = mtk_hdmi_tmds_over_340M(hdmi);
	if (is_over_340M == true)
		mtk_hdmi_send_TMDS_configuration(hdmi, SCRAMBLING_ENABLE | TMDS_BIT_CLOCK_RATION);
	else
		mtk_hdmi_send_TMDS_configuration(hdmi, 0);
	mtk_hdmi_enable_scrambling(hdmi, is_over_340M);
	mtk_hdmi_high_tmds_clock_ratio(hdmi, is_over_340M);

	mtk_hdmi_yuv420_downsample(hdmi, false);
	mtk_hdmi_yuv422_downsample(hdmi, false);

	if ((hdmi->csp == HDMI_COLORSPACE_YUV422) && (_fgDVHdrEnable == false)
		&& (_fgLowLatencyDVEnable == false)) {
		//if DV, hdmi no need to downsample YUV444 to YUV422
		mtk_hdmi_yuv422_downsample(hdmi, true);
	} else if (hdmi->csp == HDMI_COLORSPACE_YUV420)
		mtk_hdmi_yuv420_downsample(hdmi, true);
}

static int mtk_hdmi_output_set_display_mode(struct mtk_hdmi *hdmi,
					    struct drm_display_mode *mode)
{
	int ret;

	ret = mtk_hdmi_video_change_vpll(hdmi, mode->clock * 1000);
	if (ret) {
		dev_err(hdmi->dev, "Failed to set vpll: %d\n", ret);
		return ret;
	}

	mtk_hdmi_change_video_resolution(hdmi);
	mtk_hdmi_aud_output_config(hdmi, mode);

	return 0;
}

static int mtk_hdmi_get_all_clk(struct mtk_hdmi *hdmi,
				struct device_node *np)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_clk_names); i++) {
		if (i == MTK_HDMI_CLK_HDMI_TXPLL)
			hdmi->clk[i] = hdmi->hdmi_phy_base->txpll;
		else
			hdmi->clk[i] = of_clk_get_by_name(np,
				mtk_hdmi_clk_names[i]);

		if (IS_ERR(hdmi->clk[i]))
			return PTR_ERR(hdmi->clk[i]);
	}

	return 0;
}

void mtk_hdmi_clk_enable(struct mtk_hdmi *hdmi)
{
	int i;

	HDMI_FUNC();

	clk_set_parent(hdmi->clk[MTK_HDIM_HDCP_SEL], hdmi->clk[MTK_HDMI_UNIVPLL_D4D8]);

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_clk_names); i++) {
		if ((i == MTK_HDMI_CLK_HDMI_TXPLL) || (i == MTK_HDMI_UNIVPLL_D4D8) ||
		(i == MTK_HDMI_CLK_UNIVPLL_D6D4) || (i == MTK_HDMI_CLK_MSDCPLL_D2) ||
		(i == MTK_HDMI_CLK_HDMI_APB_SEL))
			continue;
		else
			clk_prepare_enable(hdmi->clk[i]);
	}
}

void mtk_hdmi_clk_disable(struct mtk_hdmi *hdmi)
{
	int i;

	HDMI_FUNC();

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_clk_names); i++) {
		if ((i == MTK_HDMI_CLK_HDMI_TXPLL) || (i == MTK_HDMI_UNIVPLL_D4D8) ||
		(i == MTK_HDMI_CLK_UNIVPLL_D6D4) || (i == MTK_HDMI_CLK_MSDCPLL_D2) ||
		(i == MTK_HDMI_CLK_HDMI_APB_SEL))
			continue;
		else
			clk_disable_unprepare(hdmi->clk[i]);
	}
}

static void mtk_hdmi_hpd_event(enum HDMI_HPD_STATE hpd, struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	if (hdmi && hdmi->bridge.encoder && hdmi->bridge.encoder->dev) {
		HDMI_LOG("hpd irq event\n");
		drm_helper_hpd_irq_event(hdmi->bridge.encoder->dev);
	}

	if ((hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) && (ethdr_notify_hdmi_hpd != NULL))
		ethdr_notify_hdmi_hpd(false);
}

enum HDMI_HPD_STATE mtk_hdmi_hpd_pord_status(struct mtk_hdmi *hdmi)
{
	unsigned int hpd_status;
	enum HDMI_HPD_STATE hpd;

	hpd_status = mtk_hdmi_read(hdmi, HPD_DDC_STATUS);
	if ((hpd_status & (HPD_PIN_STA | PORD_PIN_STA)) == (HPD_PIN_STA | PORD_PIN_STA))
		hpd = HDMI_PLUG_IN_AND_SINK_POWER_ON;
	else if ((hpd_status & (HPD_PIN_STA | PORD_PIN_STA)) == HPD_PIN_STA)
		hpd = HDMI_PLUG_IN_ONLY;
	else
		hpd = HDMI_PLUG_OUT;

	return hpd;
}

static irqreturn_t mtk_hdmi_isr(int irq, void *arg)
{
	//struct device *dev = arg;
	//can.zeng todo verify
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	unsigned int int_status;
	int ret = 0;
	int i = 0;

	if (hdmi->ca_init != true) {
		msleep(30);
		return IRQ_HANDLED;
	}

	HDMI_LOG("INT MASK0=0x%08x, INT MASK1=0x%08x, STATUS0=0x%08x, INT STATUS1=0x%08x\n",
		mtk_hdmi_read(hdmi, TOP_INT_MASK00),
		mtk_hdmi_read(hdmi, TOP_INT_MASK01),
		mtk_hdmi_read(hdmi, TOP_INT_STA00),
		mtk_hdmi_read(hdmi, TOP_INT_STA01));
	int_status = mtk_hdmi_read(hdmi, TOP_INT_STA00);

	/*clear all tx irq*/
	mtk_hdmi_clr_all_int_status(hdmi);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	/* handle HDCP 2.x re-auth interrupt */
	if (int_status & HDCP2X_RX_REAUTH_REQ_DDCM_INT_STA) {
		if ((hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON) && (hdmi->hdcp_ctrl_state !=
			HDCP2x_AUTHENTICATION)) {
			mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0xFFFFFF00);
			mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00000000);
			vSetHDCPState(HDCP2x_AUTHENTICATION);
			atomic_set(&hdmi_hdcp_event, 1);
			wake_up_interruptible(&hdmi->hdcp_wq);
			HDMI_LOG("reauth_req\n");
		}

		mtk_hdmi_mask(hdmi, TOP_INT_CLR00,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_CLR,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_CLR);
		mtk_hdmi_mask(hdmi, TOP_INT_CLR00,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNCLR,
			HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNCLR);
	}
#endif

	/* handle hpd interrupt */
	if (int_status & (PORD_F_INT_STA | PORD_R_INT_STA |
		HTPLG_F_INT_STA | HTPLG_R_INT_STA)) {
		HDMI_LOG("HPD/PORD IRQ\n");
		for (i = 0; i < HPD_WQ_NUM; i++) {
			ret = queue_delayed_work(hdmi->hdmi_hpd_wq[i], &hdmi->hpd_work[i],
				msecs_to_jiffies(TIME_HPD_PORD_STABLE_MS));
			if (ret == 1) {
				atomic_inc(&hdmi_hpd_work);
				HDMI_LOG("delayed work[%d] with %dms\n",
					i, TIME_HPD_PORD_STABLE_MS);
				break;
			}
		}

		if (i == HPD_WQ_NUM)
			HDMI_LOG("error: fail to queue delayed work\n");

		mtk_hdmi_enable_hpd_pord_irq(hdmi, false);
		mtk_hdmi_clr_htplg_pord_irq(hdmi);
	}

	return IRQ_HANDLED;
}

static inline void mtk_hdmi_audio_extcon_state(
	struct mtk_hdmi *hdmi, bool state)
{
	extcon_set_state_sync(hdmi_extcon, EXTCON_DISP_HDMI, state);
}

static int hdmi_get_edid(struct mtk_hdmi *hdmi, unsigned char *edid_buff, u32 count)
{
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct edid *edid;
	unsigned char retry_times = 0;
	unsigned char count_read = 0, count_tmp = 0;
	unsigned char bIdx, bBlockIdx = 0;
	unsigned char bExtBlockNo = 0;
	unsigned char bOOEDBBlockNo = 0;

	HDMI_LOG("start\n");

	if (!hdmi->ddc_adpt)
		return 0;

	if (hdmi->use_firmware_edid == false) {
edid_retry:
		hdmi_ddc_request(ddc, 1);
		hdmi_ddc_free(ddc, 1);

		count_tmp = (count < MAX_BLOCK_NUM) ? count : MAX_BLOCK_NUM;
		bExtBlockNo = 0xff;

		for (bBlockIdx = 0; bBlockIdx <= bExtBlockNo; bBlockIdx++) {
			if (bBlockIdx < count_tmp) {
				for (bIdx = 0; bIdx < 5; bIdx++) {
					if (fgDDCDataRead(ddc, EDID_ID + (bBlockIdx >> 1),
						0x00 + (bBlockIdx & 0x01) *
						EDID_LENGTH, EDID_LENGTH,
						&edid_buff[bBlockIdx *
						EDID_LENGTH]) == true) {
						HDMI_DETAIL_LOG(
							"[HDMI]EDID block[%d]: CheckSum = 0x%x\n",
							bBlockIdx, edid_buff[(bBlockIdx + 1) *
							EDID_LENGTH - 1]);
						break;
					}

					HDMI_LOG("read gain\n");

					if (bIdx == 4) {
						HDMI_LOG("read fail with [%d] blocks\n", bBlockIdx);
						goto check_valid;
					}
				}
			} else {
				HDMI_LOG("get [%d] blocks\n", bBlockIdx);
				goto check_valid;
			}

			bExtBlockNo = edid_buff[EDID_ADDR_EXT_BLOCK_FLAG];
			vSetSharedInfo(SI_EDID_EXT_BLOCK_NO, bExtBlockNo);

			// find OOEDB
			if (((edid_buff[0x84] & 0xE0) == 0xE0) && (edid_buff[0x85] == 0x78))
				bOOEDBBlockNo = edid_buff[0x86];
			if (bOOEDBBlockNo > 7)
				bOOEDBBlockNo = 7;
			if (bOOEDBBlockNo > bExtBlockNo) {
				bExtBlockNo = bOOEDBBlockNo;
				vSetSharedInfo(SI_EDID_EXT_BLOCK_NO, bExtBlockNo);
			}
		}
		count_read = bBlockIdx;

check_valid:
		retry_times++;

		edid = (struct edid *)edid_buff;
		if (!drm_edid_is_valid(edid)) {
			if (retry_times <= 5) {
				HDMI_LOG("EDID invalid, retry=%d\n", retry_times);
				msleep(20);
				goto edid_retry;
			} else {
				count_tmp = ((ARRAY_SIZE(default_edid) / EDID_LENGTH) < count) ?
					(ARRAY_SIZE(default_edid) / EDID_LENGTH) : count;
				HDMI_LOG("EDID invalid, use default EDID with [%d] blocks\n",
					count_tmp);
				memcpy(edid_buff, default_edid, count_tmp * EDID_LENGTH);
				count_read = count_tmp;
			}
		}
	} else {
		HDMI_LOG("use firmware EDID\n");
		memcpy(edid_buff, hdmi->firmware_edid, (EDID_LENGTH * 4));
		count_read = 4;
	}

	HDMI_LOG("end with [%d] blocks\n", count_read);

	return count_read;
}

void mtk_hdmi_hpd_work_handle(enum HDMI_HPD_STATE hpd)
{
	//struct mtk_hdmi *hdmi = container_of(data, struct mtk_hdmi, hpd_work);
	//can.zeng todo verify
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	unsigned char *edid_buff;
	struct edid *edid;

	if ((hpd != hdmi->hpd) || (hdmi->hpd_force_set == true)) {
		if ((hdmi->hpd_force_set == true) && (hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON))
			hdmi->hpd_force_set = false;
		HDMI_LOG("Hot Plug State Change! old=%d, new=%d\n", hdmi->hpd, hpd);
		hdmi->hpd = hpd;
		mtk_hdmi_hpd_event(hpd, hdmi->dev);
		if (hdmi->attached == true)
			drm_connector_update_edid_property(&hdmi->conn, NULL);

		if (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			HDMI_LOG("HPD High\n");
			hdmi_notify_uevent_user(&hdmi_switch_data, 1);
		} else {
			/* Plug out or Plug in Only, clear all the edid info */
			HDMI_LOG("HPD low\n");
			vClearEdidInfo();
			if (hdmi->attached == true)
				mtk_hdmi_update_hdmi_info_property(hdmi);
			hdmi_notify_uevent_user(&hdmi_switch_data, 0);
			memset(hdmi->raw_edid.edid, 0,
				hdmi->raw_edid.blk_num * EDID_LENGTH);
			hdmi->raw_edid.blk_num = 0;
			hdmi->csp = HDMI_COLORSPACE_RGB;
			hdmi->color_depth = HDMI_8_BIT;
			hdmi->colorimtery = HDMI_COLORIMETRY_NONE;
			hdmi->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_RESERVED;
			hdmi->quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
			hdmi->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
			if (hdmi->hpd == HDMI_PLUG_OUT) {
				//cec_notifier_set_phys_addr(hdmi->notifier, CEC_PHYS_ADDR_INVALID);
				/* not clear physical address to make sure cec HAL
				 * can send <One touch Play> message to wake up TV with
				 * former valid physical address
				 */
				hdmi_notify_uevent_user(&hdmi_cec_switch_data, 0);
			} else if (hdmi->hpd == HDMI_PLUG_IN_ONLY) {
				//not clear valid physical address
				edid_buff = kzalloc(EDID_LENGTH * MAX_BLOCK_NUM, GFP_KERNEL);

				if (hdmi_get_edid(hdmi, edid_buff, MAX_BLOCK_NUM) > 0) {
					edid = (struct edid *)edid_buff;

					cec_notifier_set_phys_addr_from_edid(hdmi->notifier, edid);
					hdmi_notify_uevent_user(&hdmi_cec_switch_data, 1);
				} else {
					HDMI_LOG("hdmi_get_edid fail\n");
				}
				kfree(edid_buff);
			}
		}
	} else
		HDMI_LOG("Hot Plug State NOT Change! hpd=%d\n", hdmi->hpd);

	HDMI_LOG("Hot Plug work done hpd=%d\n", hdmi->hpd);
}

void mtk_hdmi_hpd_work(struct work_struct *data)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	enum HDMI_HPD_STATE hpd;

	hpd = mtk_hdmi_hpd_pord_status(hdmi);
	HDMI_LOG("Hot Plug hpd=%d\n", hpd);

	kfifo_put(&hdmi->fifo, hpd);

	atomic_set(&hdmi_hpd_event, 1);
	wake_up_interruptible(&hdmi->hpd_waitqueue);

	if (atomic_dec_and_test(&hdmi_hpd_work))
		mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
	else
		HDMI_LOG("not enable hpd_pord irq\n");
}

static int mtk_hdmi_init_workqueue(struct mtk_hdmi *hdmi)
{
	int i;
	char name[64];
	int ret = 0;

	HDMI_FUNC();
	hdmi->hdmi_wq = create_singlethread_workqueue("hdmitx_wq");
	if (!hdmi->hdmi_wq) {
		HDMI_LOG("Failed to create hdmitx workqueue\n");
		return -ENOMEM;
	}

	for (i = 0; i < HPD_WQ_NUM; i++) {
		ret = sprintf(name, "hdmitx_hpd_wq_%d", i);
		if (ret < 0) {
			HDMI_LOG("sprintf error\n");
			return -1;
		}
		hdmi->hdmi_hpd_wq[i] = create_singlethread_workqueue(name);
		if (!hdmi->hdmi_hpd_wq[i]) {
			HDMI_LOG("Failed to create hdmitx workqueue[%d]\n", i);
			return -ENOMEM;
		}
	}

	for (i = 0; i < HPD_WQ_NUM; i++)
		INIT_DELAYED_WORK(&hdmi->hpd_work[i], mtk_hdmi_hpd_work);

	INIT_DELAYED_WORK(&hdmi->hdr10_delay_work, Hdr10DelayOffHandler);
	INIT_DELAYED_WORK(&hdmi->hdr10vsif_delay_work, Hdr10pVsifDelayOffHandler);
	return 0;
}

static int hdmi_hpd_kthread(void *data)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	enum HDMI_HPD_STATE hpd = HDMI_PLUG_OUT;

	for (;;) {
		wait_event_interruptible(hdmi->hpd_waitqueue,
			atomic_read(&hdmi_hpd_event));
		atomic_set(&hdmi_hpd_event, 0);

		while (kfifo_get(&hdmi->fifo, &hpd))
			mtk_hdmi_hpd_work_handle(hpd);

		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int mtk_create_hdmi_hpd_task(struct mtk_hdmi *hdmi)
{
	HDMI_FUNC();

	init_waitqueue_head(&hdmi->hpd_waitqueue);
	hdmi->hpd_task = kthread_create(hdmi_hpd_kthread,
					NULL, "hdmi_hpd_kthread");
	if (hdmi->hpd_task == NULL) {
		HDMI_LOG("error: fail to create hpd task\n");
		return -EINVAL;
	}
	wake_up_process(hdmi->hpd_task);

	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
#define HDMI_HDCP_INTERVAL 10
void hdmi_hdcp_timer_isr(struct timer_list *timer)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	atomic_set(&hdmi_hdcp_event, 1);
	wake_up_interruptible(&hdmi->hdcp_wq);
	mod_timer(&hdmi->hdcp_timer, jiffies + HDMI_HDCP_INTERVAL / (1000 / HZ));

	hdcp_delay_time -= HDMI_HDCP_INTERVAL;
}

void hdmi_hdcp_start_task(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_FUNC();
	memset((void *)&(hdmi->hdcp_timer), 0, sizeof(hdmi->hdcp_timer));
	timer_setup(&hdmi->hdcp_timer, hdmi_hdcp_timer_isr, 0);
	hdmi->hdcp_timer.expires = jiffies + 50 / (1000 / HZ);
	add_timer(&hdmi->hdcp_timer);
	hdmi->repeater_hdcp = false;
	vHDCPInitAuth();
}

void hdmi_hdcp_stop_task(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_FUNC();
	if (hdmi->hdcp_timer.function)
		del_timer_sync(&hdmi->hdcp_timer);
	memset((void *)&hdmi->hdcp_timer, 0, sizeof(hdmi->hdcp_timer));
}

void hdmi_hdcp_task(enum HDCP_CTRL_STATE_T e_hdcp_state)
{
	unsigned int bStatus;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	bool checkLink = false;

	bStatus = mtk_hdmi_read(hdmi, TOP_INT_STA00);
	if (bStatus & HDCP_RI_128_INT_STA) {
		/* clear ri irq */
		mtk_hdmi_mask(hdmi, TOP_INT_CLR00, HDCP_RI_128_INT_CLR,
			HDCP_RI_128_INT_CLR);
		mtk_hdmi_mask(hdmi, TOP_INT_CLR00, 0x0, HDCP_RI_128_INT_CLR);

		if ((hdmi->hdcp_ctrl_state == HDCP_WAIT_RI) ||
			(hdmi->hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY)) {
			vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
			checkLink = true;
		}
	}

	if (hdmi->hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY) {
		if (checkLink == true)
			checkLink = false;
		else
			return;
	}

	if (fgIsHDCPCtrlTimeOut(hdmi->hdcp_ctrl_state))
		hdcp_service(hdmi->hdcp_ctrl_state);

}

static int hdmi_hdcp_kthread(void *data)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	for (;;) {
		wait_event_interruptible(hdmi->hdcp_wq,
			atomic_read(&hdmi_hdcp_event));
		atomic_set(&hdmi_hdcp_event, 0);
		hdmi_hdcp_task(hdmi->hdcp_ctrl_state);
		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int mtk_create_hdmi_hdcp_task(struct mtk_hdmi *hdmi)
{
	init_waitqueue_head(&hdmi->hdcp_wq);
	hdmi->hdcp_task = kthread_create(hdmi_hdcp_kthread,
					NULL, "hdmi_hdcp_kthread");
	if (hdmi->hdcp_task == NULL) {
		HDMI_LOG("Failed to create hdcp task\n");
		return -EINVAL;
	}
	wake_up_process(hdmi->hdcp_task);

	return 0;
}
#endif

static enum drm_connector_status hdmi_conn_detect(struct drm_connector *conn,
						  bool force)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	HDMI_LOG("hpd state = %d\n", hdmi->hpd);

	return (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON) ?
		connector_status_connected : connector_status_disconnected;
}

static void hdmi_conn_destroy(struct drm_connector *conn)
{
	drm_connector_cleanup(conn);
}

static int hdmi_conn_atomic_set_property(struct drm_connector *conn,
	struct drm_connector_state *state, struct drm_property *property,
	uint64_t val)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	spin_lock(&hdmi->property_lock);

	if (property == hdmi->csp_depth_prop) {
		if (val & (hdmi->support_csp_depth)) {
			hdmi->set_csp_depth = val;
			HDMI_LOG("set_csp_depth:0x%x\n", hdmi->set_csp_depth);
		} else {
			HDMI_LOG("un-support colorspace & colordepth, 0x%x\n", val);
			mtk_hdmi_reset_colorspace_setting(hdmi);
			spin_unlock(&hdmi->property_lock);
			return -EINVAL;
		}
	} else {
		HDMI_LOG("un-support property\n");
		spin_unlock(&hdmi->property_lock);
		return -EINVAL;
	}

	spin_unlock(&hdmi->property_lock);
	return 0;
}

static int hdmi_conn_atomic_get_property(struct drm_connector *conn,
	const struct drm_connector_state *state, struct drm_property *property,
	uint64_t *val)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	spin_lock(&hdmi->property_lock);

	if (property == hdmi->csp_depth_prop) {
		*val = hdmi->support_csp_depth;
		HDMI_LOG("support_csp_depth=%x\n", hdmi->support_csp_depth);
	} else {
		HDMI_LOG("un-support property\n");
		spin_unlock(&hdmi->property_lock);
		return -EINVAL;
	}

	spin_unlock(&hdmi->property_lock);
	return 0;
}

//for ETHDR get supported ColorSpace and ColorDepth
//return 0 success, else failure
unsigned int get_hdmi_supported_colorspace_depth(
	struct drm_bridge *bridge, uint64_t *colorspace_depth)
{
	struct mtk_hdmi *hdmi =	hdmi_ctx_from_bridge(bridge);
	struct drm_connector *connector = &hdmi->conn;
	struct drm_property *prop = hdmi->csp_depth_prop;
	unsigned int result;

	HDMI_LOG("Get ColorSpace,ColorDepth in kernel side\n");

	if (connector->funcs->atomic_get_property) {
		result = connector->funcs->atomic_get_property(connector,
			NULL, prop, colorspace_depth);
		if (result != 0)
			return -EINVAL;
	} else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(get_hdmi_supported_colorspace_depth);

//for ETHDR set supported ColorSpace and ColorDepth
//return 0 success, else failure
unsigned int set_hdmi_colorspace_depth(
	struct drm_bridge *bridge, uint64_t colorspace_depth)
{
	struct mtk_hdmi *hdmi =	hdmi_ctx_from_bridge(bridge);
	struct drm_connector *connector = &hdmi->conn;
	struct drm_property *prop = hdmi->csp_depth_prop;
	unsigned int result;

	HDMI_LOG("Set ColorSpace,ColorDepth in kernel side\n");

	if (connector->funcs->atomic_set_property) {
		result = connector->funcs->atomic_set_property(connector,
			NULL, prop, colorspace_depth);
		if (result != 0)
			return -EINVAL;
	} else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(set_hdmi_colorspace_depth);

//for ETHDR set Colorimetry and QuantizationRange
//return 0 success, else failure
unsigned int set_hdmi_colorimetry_range(struct drm_bridge *bridge,
	enum hdmi_colorimetry colorimtery, enum hdmi_extended_colorimetry extended_colorimetry,
	enum hdmi_quantization_range quantization_range,
	enum hdmi_ycc_quantization_range ycc_quantization_range)
{
	struct mtk_hdmi *hdmi =	hdmi_ctx_from_bridge(bridge);

	HDMI_LOG("Set Colorimetry,QuantizationRange in kernel side\n");

	hdmi->colorimtery = colorimtery;
	hdmi->extended_colorimetry = extended_colorimetry;
	hdmi->quantization_range = quantization_range;
	hdmi->ycc_quantization_range = ycc_quantization_range;

	return 0;
}
EXPORT_SYMBOL(set_hdmi_colorimetry_range);

//for DPI get output ColorSpace and Colorimetry
//return 0 success, else failure
unsigned int get_hdmi_colorspace_colorimetry(struct drm_bridge *bridge,
	enum hdmi_colorspace *colorspace, enum hdmi_colorimetry *colorimtery,
	enum hdmi_extended_colorimetry *extended_colorimetry,
	enum hdmi_quantization_range *quantization_range,
	enum hdmi_ycc_quantization_range *ycc_quantization_range)
{
	struct mtk_hdmi *hdmi =	hdmi_ctx_from_bridge(bridge);

	HDMI_LOG("DPI get ColorSpace and Colorimetry\n");
	*colorspace = hdmi->csp;
	*colorimtery = hdmi->colorimtery;
	*extended_colorimetry = hdmi->extended_colorimetry;
	*quantization_range = hdmi->quantization_range;
	*ycc_quantization_range = hdmi->ycc_quantization_range;
	//DPI cannot support BT2020 conversion, so no need extended_colorimetry information
	return 0;
}
EXPORT_SYMBOL(get_hdmi_colorspace_colorimetry);


struct mtk_hdmi_edid *mtk_hdmi_get_raw_edid(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi =	hdmi_ctx_from_bridge(bridge);
	struct edid *edid = (struct edid *)(hdmi->raw_edid.edid);

	if (!drm_edid_is_valid(edid)) {
		HDMI_LOG("EDID invalid\n");
		return NULL;
	}

	if (!hdmi->hdmi_enabled)
		return NULL;

	return &(hdmi->raw_edid);
}
EXPORT_SYMBOL(mtk_hdmi_get_raw_edid);


static int
hdmi_cea_db_offsets(const u8 *cea, int *start, int *end)
{
	/* DisplayID CTA extension blocks and top-level CEA EDID
	 * block header definitions differ in the following bytes:
	 *   1) Byte 2 of the header specifies length differently,
	 *   2) Byte 3 is only present in the CEA top level block.
	 *
	 * The different definitions for byte 2 follow.
	 *
	 * DisplayID CTA extension block defines byte 2 as:
	 *   Number of payload bytes
	 *
	 * CEA EDID block defines byte 2 as:
	 *   Byte number (decimal) within this block where the 18-byte
	 *   DTDs begin. If no non-DTD data is present in this extension
	 *   block, the value should be set to 04h (the byte after next).
	 *   If set to 00h, there are no DTDs present in this block and
	 *   no non-DTD data.
	 */
	if (cea[0] == DATA_BLOCK_CTA) {
		*start = 3;
		*end = *start + cea[2];
	} else if (cea[0] == CEA_EXT) {
		/* Data block offset in CEA extension block */
		*start = 4;
		*end = cea[2];
		if (*end == 0)
			*end = 127;
		if (*end < 4 || *end > 127)
			return -ERANGE;
	} else {
		return -EOPNOTSUPP;
	}

	return 0;
}

static int
hdmi_cea_db_payload_len(const u8 *db)
{
	return db[0] & 0x1f;
}

#define for_each_hdmi_cea_db(cea, i, start, end) \
	for ((i) = (start); \
	(i) < (end) && (i) + hdmi_cea_db_payload_len(&(cea)[(i)]) < (end); \
	(i) += hdmi_cea_db_payload_len(&(cea)[(i)]) + 1)

#define VENDOR_BLOCK    0x03

static int
hdmi_cea_db_tag(const u8 *db)
{
	return db[0] >> 5;
}

static bool hdmi_cea_db_is_hdmi_vsdb(const u8 *db)
{
	int hdmi_id;

	if (hdmi_cea_db_tag(db) != VENDOR_BLOCK)
		return false;

	if (hdmi_cea_db_payload_len(db) < 5)
		return false;

	hdmi_id = db[1] | (db[2] << 8) | (db[3] << 16);

	return hdmi_id == HDMI_IEEE_OUI;
}


static bool parse_hdmi_ycbcr444_deep_color(const u8 *hdmi)
{
	if (hdmi_cea_db_payload_len(hdmi) < 6)
		return false;

	/* YCRCB444 is optional according to spec. */
	if (hdmi[6] & DRM_EDID_HDMI_DC_Y444) {
		HDMI_LOG("ycbcr444 support deep color\n");
		return true;
	}

	HDMI_LOG("ycbcr444 NOT support deep color\n");
	return false;
}

static bool is_ycbcr444_support_deep_color(struct edid *edid)
{
	const u8 *cea, *edid_ext;
	const u8 *db = NULL;
	int i;
	int j, start, end;
	bool result = 0;

	/* No EDID or EDID extensions */
	if (edid == NULL || edid->extensions == 0)
		return false;

	/* Find CEA extension start */
	for (i = 0; i < edid->extensions; i++) {
		edid_ext = (u8 *)edid + EDID_LENGTH * (i + 1);
		if (edid_ext[0] == CEA_EXT)
			break;
	}
	if (i == edid->extensions)
		return false;

	cea = edid_ext;
	/* Find CEA extension end */

	// CEA REVISION >=3
	if (cea && cea[1] >= 3) {

		if (hdmi_cea_db_offsets(cea, &start, &end))
			return false;

		for_each_hdmi_cea_db(cea, j, start, end) {
			db = &cea[j];

			if (hdmi_cea_db_tag(db) == VENDOR_BLOCK) {
				/* HDMI Vendor-Specific Data Block */
				if (hdmi_cea_db_is_hdmi_vsdb(db))
					result = parse_hdmi_ycbcr444_deep_color(db);
			}
		}
	}

	return result;
}

//update color space & deep color property
static unsigned int mtk_hdmi_update_sink_csp_depth(
	struct drm_connector *conn, struct edid *edid)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);
	struct drm_display_info *disp_info = &conn->display_info;
	struct drm_hdmi_info *hdmi_info = &disp_info->hdmi;
	unsigned int prop_value = 0;

	if (disp_info->color_formats & DRM_COLOR_FORMAT_RGB444) {
		prop_value |= RGB444_8bit;
		if (disp_info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
			prop_value |= RGB444_10bit;
		if (disp_info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_36)
			prop_value |= RGB444_12bit;
		if (disp_info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_48)
			prop_value |= RGB444_16bit;
	}

	if (disp_info->color_formats & DRM_COLOR_FORMAT_YCRCB444) {
		prop_value |= YCBCR444_8bit;
		if (is_ycbcr444_support_deep_color(edid) == true) {
			if (disp_info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_30)
				prop_value |= YCBCR444_10bit;
			if (disp_info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_36)
				prop_value |= YCBCR444_12bit;
			if (disp_info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_48)
				prop_value |= YCBCR444_16bit;
		}
	}

	if (disp_info->color_formats & DRM_COLOR_FORMAT_YCRCB422) {
		prop_value |= YCBCR422_8bit;
		prop_value |= YCBCR422_10bit;
		prop_value |= YCBCR422_12bit;
	}

	if (disp_info->color_formats & DRM_COLOR_FORMAT_YCRCB420) {
		prop_value |= YCBCR420_8bit;
		if (hdmi_info->y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
			prop_value |= YCBCR420_10bit;
		if (hdmi_info->y420_dc_modes & DRM_EDID_YCBCR420_DC_36)
			prop_value |= YCBCR420_12bit;
		if (hdmi_info->y420_dc_modes & DRM_EDID_YCBCR420_DC_48)
			prop_value |= YCBCR420_16bit;
	}

	hdmi->support_csp_depth = prop_value;

	HDMI_LOG("hdmi->support_csp_depth = 0x%x\n", hdmi->support_csp_depth);

	return prop_value;
}

static const struct drm_prop_enum_list csp_depth_props[] = {
	{ __builtin_ffs(RGB444_8bit),  "RGB444_8bit" },
	{ __builtin_ffs(RGB444_10bit), "RGB444_10bit" },
	{ __builtin_ffs(RGB444_12bit), "RGB444_10bit" },
	{ __builtin_ffs(RGB444_16bit), "RGB444_16bit" },
	{ __builtin_ffs(YCBCR444_8bit),  "YCBCR444_8bit" },
	{ __builtin_ffs(YCBCR444_10bit),  "YCBCR444_10bit" },
	{ __builtin_ffs(YCBCR444_12bit),  "YCBCR444_12bit" },
	{ __builtin_ffs(YCBCR444_16bit),  "YCBCR444_16bit" },
	{ __builtin_ffs(YCBCR422_8bit),  "YCBCR422_8bit" },
	{ __builtin_ffs(YCBCR422_10bit),  "YCBCR422_10bit" },
	{ __builtin_ffs(YCBCR422_12bit),  "YCBCR422_12bit" },
	{ __builtin_ffs(YCBCR422_16bit_NO_SUPPORT),  "YCBCR422_16bit_NO_SUPPORT" },
	{ __builtin_ffs(YCBCR420_8bit),  "YCBCR420_8bit" },
	{ __builtin_ffs(YCBCR420_10bit),  "YCBCR420_10bit" },
	{ __builtin_ffs(YCBCR420_12bit),  "YCBCR420_12bit" },
	{ __builtin_ffs(YCBCR420_16bit),  "YCBCR420_16bit" },
};

static void mtk_hdmi_connector_init_property(
	struct drm_device *drm_dev, struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);
	struct drm_property *prop;
	/* ycbcr422 cannot support 8,10,16bit */
	unsigned int supported_csp_depth_mask =
		RGB444_8bit | RGB444_10bit | RGB444_12bit |
		RGB444_16bit | YCBCR444_8bit | YCBCR444_10bit |
		YCBCR444_12bit | YCBCR444_16bit | YCBCR422_8bit |
		YCBCR422_10bit | YCBCR422_12bit |
		YCBCR420_8bit | YCBCR420_10bit | YCBCR420_12bit |
		YCBCR420_16bit;

	spin_lock_init(&hdmi->property_lock);

	/* create colorspace_depth bitmask property */
	prop = drm_property_create_bitmask(conn->dev, 0, "hdmi_colorspace_depth",
					   csp_depth_props, ARRAY_SIZE(csp_depth_props),
					   supported_csp_depth_mask);

	if (!prop) {
		HDMI_LOG("cannot create colorspace_depth property\n");
		return;
	}
	HDMI_LOG("create colorspace & colordepth property\n");
	hdmi->csp_depth_prop = prop;
	drm_object_attach_property(&conn->base, prop, 0);

	/* create mtk_hdmi_blob property, include EDID parser info,
	 * such as max_tmds_clock_rate, max_tmds_character_rate, support DV
	 */
	prop = drm_property_create(conn->dev, DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "HDMI_INFO", 0);
	if (!prop) {
		HDMI_LOG("cannot create HDMI_INFO blob property\n");
		return;
	}
	hdmi->hdmi_info_blob = prop;
	hdmi->hdmi_info_blob_ptr = NULL;
	drm_object_attach_property(&conn->base, prop, 0);
}

int mtk_hdmi_update_hdmi_info_property(struct mtk_hdmi *hdmi)
{
	struct drm_device *dev = hdmi->conn.dev;
	size_t size = 0;
	int ret;
	struct mtk_hdmi_info info;
	struct drm_display_info *disp_info = &hdmi->conn.display_info;
	unsigned int sink_colorimetry;

	sink_colorimetry = _HdmiSinkAvCap.ui4_sink_colorimetry;
	if (disp_info->color_formats & DRM_COLOR_FORMAT_RGB444)
		sink_colorimetry |= SINK_RGB;
	else
		sink_colorimetry = sink_colorimetry & ~SINK_RGB;

	if (disp_info->color_formats & DRM_COLOR_FORMAT_YCRCB444)
		sink_colorimetry |= SINK_YCBCR_444;
	else
		sink_colorimetry = sink_colorimetry & ~SINK_YCBCR_444;

	if (disp_info->color_formats & DRM_COLOR_FORMAT_YCRCB422)
		sink_colorimetry |= SINK_YCBCR_422;
	else
		sink_colorimetry = sink_colorimetry & ~SINK_YCBCR_422;

	if (disp_info->color_formats & DRM_COLOR_FORMAT_YCRCB420)
		sink_colorimetry |= SINK_YCBCR_420;
	else {
		sink_colorimetry = sink_colorimetry & ~SINK_YCBCR_420;
		sink_colorimetry = sink_colorimetry & ~SINK_YCBCR_420_CAPABILITY;
	}
	HDMI_LOG("colorimetry:0x%x, 0x%x\n", _HdmiSinkAvCap.ui4_sink_colorimetry, sink_colorimetry);

	info.edid_sink_colorimetry = sink_colorimetry;
	info.edid_sink_rgb_color_bit = _HdmiSinkAvCap.e_sink_rgb_color_bit;
	info.edid_sink_ycbcr_color_bit = _HdmiSinkAvCap.e_sink_ycbcr_color_bit;
	info.ui1_sink_dc420_color_bit = _HdmiSinkAvCap.ui1_sink_dc420_color_bit;
	info.edid_sink_max_tmds_clock = _HdmiSinkAvCap.ui1_sink_max_tmds_clock;
	info.edid_sink_max_tmds_character_rate = _HdmiSinkAvCap.ui2_sink_max_tmds_character_rate;
	info.edid_sink_support_dynamic_hdr = _HdmiSinkAvCap.ui1_sink_support_dynamic_hdr;
	info.edid_sink_support_static_hdr = _HdmiSinkAvCap.ui1_sink_support_static_hdr;

	HDMI_LOG("color:0x%x,rgb:0x%x,ycbcr:0x%x,dc420:0x%x,max:[%d,%d],hdr:[0x%x,0x%x]\n",
		info.edid_sink_colorimetry,
		info.edid_sink_rgb_color_bit,
		info.edid_sink_ycbcr_color_bit,
		info.ui1_sink_dc420_color_bit,
		info.edid_sink_max_tmds_clock,
		info.edid_sink_max_tmds_character_rate,
		info.edid_sink_support_dynamic_hdr,
		info.edid_sink_support_static_hdr);

	size = sizeof(struct mtk_hdmi_info);

	ret = drm_property_replace_global_blob(dev, &hdmi->hdmi_info_blob_ptr,
		size, &info, &hdmi->conn.base, hdmi->hdmi_info_blob);
	return ret;
}

void mtk_hdmi_convert_colorspace_depth(struct mtk_hdmi *hdmi)
{
	if (hdmi->debug_csp_depth == true) {
		HDMI_LOG("debug color space:%d, color depth:%d\n",
			hdmi->csp, hdmi->color_depth);
		return;
	}

	switch (hdmi->set_csp_depth) {
	case RGB444_8bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case RGB444_10bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case RGB444_12bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case RGB444_16bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_16_BIT;
		break;
	case YCBCR444_8bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case YCBCR444_10bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case YCBCR444_12bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case YCBCR444_16bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_16_BIT;
		break;
	case YCBCR422_8bit:
		hdmi->csp = HDMI_COLORSPACE_YUV422;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case YCBCR422_10bit:
		hdmi->csp = HDMI_COLORSPACE_YUV422;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case YCBCR422_12bit:
		hdmi->csp = HDMI_COLORSPACE_YUV422;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case YCBCR420_8bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case YCBCR420_10bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case YCBCR420_12bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case YCBCR420_16bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_16_BIT;
		break;
	default:
		HDMI_LOG("ERROR:wrong ColorSpace or ColorDepth\n");
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_8_BIT;
	}
	HDMI_LOG("color space:%d, color depth:%d\n", hdmi->csp, hdmi->color_depth);
}
EXPORT_SYMBOL(mtk_hdmi_convert_colorspace_depth);

void hdmi_show_EDID_raw_data(unsigned char *edid, u32 count)
{
	unsigned short bTemp, i, j;
	unsigned char *p;

	for (bTemp = 0; bTemp < count; bTemp++) {
		HDMI_LOG(
"===================================================================\n");
		HDMI_LOG("   EDID Block Number=#%d\n", bTemp);
		HDMI_LOG(
"   | 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f\n");
		HDMI_LOG(
"===================================================================\n");
		j = bTemp * EDID_LENGTH;
		for (i = 0; i < 8; i++) {
			p = &edid[j + i * 16];
			HDMI_LOG(
"%02x:  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  ",
i * 16 + j, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
			HDMI_LOG("%02x  %02x  %02x  %02x  %02x  %02x\n",
				p[10], p[11], p[12], p[13], p[14], p[15]);
		}
	}
	HDMI_LOG("===========================================\n");
}

static int mtk_hdmi_conn_get_modes(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);
	struct edid *edid;
	int ret;

	HDMI_LOG("start\n");

	if (hdmi->raw_edid.blk_num == 0) {
		ret = hdmi_get_edid(hdmi, hdmi->raw_edid.edid, MAX_BLOCK_NUM);
		if (ret < 1) {
			HDMI_LOG("hdmi_get_edid fail\n");
			return ret;
		}

		hdmi->raw_edid.blk_num = ret;

		hdmi_show_EDID_raw_data(hdmi->raw_edid.edid, ret);
	}

	edid = (struct edid *)hdmi->raw_edid.edid;

	hdmi->dvi_mode = !drm_detect_hdmi_monitor(edid);

	drm_connector_update_edid_property(conn, edid);
	cec_notifier_set_phys_addr_from_edid(hdmi->notifier, edid);
	hdmi_notify_uevent_user(&hdmi_cec_switch_data, 1);

	ret = drm_add_edid_modes(conn, edid);

	mtk_hdmi_update_sink_csp_depth(conn, edid);

	hdmi_checkedid();
	mtk_hdmi_update_hdmi_info_property(hdmi);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	/* acquire sink's hdcp version */
	vReadHdcpVersion();
	/* check dvi */
	if (hdmi->dvi_mode == true)
		hdmi->hdcp_2x_support = false;
#endif

	HDMI_LOG("end\n");

	return ret;
}

static enum drm_mode_status mtk_hdmi_conn_mode_valid(struct drm_connector *conn,
	struct drm_display_mode *mode)
{
	HDMI_LOG("xres:%d, yres:%d, refresh:%d, intl:%d, clock:%d, vic:%d\n",
		mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode),
		!!(mode->flags & DRM_MODE_FLAG_INTERLACE), mode->clock * 1000,
		drm_match_cea_mode(mode));

	if (mode->clock < 27000) {
		HDMI_LOG("MODE_CLOCK_LOW!\n");
		return MODE_CLOCK_LOW;
	}
	if (mode->clock > 594000) {
		HDMI_LOG("MODE_CLOCK_HIGH!\n");
		return MODE_CLOCK_HIGH;
	}
	if (drm_match_cea_mode(mode) == 0) {
		HDMI_DETAIL_LOG("MODE BAD!\n");
		return MODE_BAD;
	}

	return drm_mode_validate_size(mode, 0x1fff, 0x1fff);
}

static struct drm_encoder *mtk_hdmi_conn_best_enc(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	HDMI_DETAIL_LOG();

	return hdmi->bridge.encoder;
}

static const struct drm_connector_funcs mtk_hdmi_connector_funcs = {
	/* .dpms = drm_atomic_helper_connector_dpms, */
	.detect = hdmi_conn_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = hdmi_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.atomic_set_property = hdmi_conn_atomic_set_property,
	.atomic_get_property = hdmi_conn_atomic_get_property,
};

static const struct drm_connector_helper_funcs
		mtk_hdmi_connector_helper_funcs = {
	.get_modes = mtk_hdmi_conn_get_modes,
	.mode_valid = mtk_hdmi_conn_mode_valid,
	.best_encoder = mtk_hdmi_conn_best_enc,
};

#if IS_ENABLED(CONFIG_OPTEE)
void mtk_hdmi_ca_timer(struct timer_list *timer)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_FUNC();

	if (hdmi_ca_init() == true) {
		/* disable all tx interrupts */
		mtk_hdmi_disable_all_int(hdmi);
		/* enable hpd interrupt */
		mtk_hdmi_set_sw_hpd(hdmi, true);
		mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
		hdmi->hdmi_enabled = true;
		hdmi->ca_init = true;
	} else {
		mod_timer(&hdmi->ca_timer, jiffies + msecs_to_jiffies(50));
	}
}
#endif

/*
 * Bridge callbacks
 */

static int mtk_hdmi_bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	int ret, i;

#if IS_ENABLED(CONFIG_MTK_SUB_AS_MAIN)
	bool swap_display = true;
#else
	bool swap_display = false;
#endif

	HDMI_LOG("start\n");

	ret = drm_connector_init(bridge->encoder->dev, &hdmi->conn,
				 &mtk_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		HDMI_LOG("Failed to initialize connector: %d\n", ret);
		return ret;
	}
	drm_connector_helper_add(&hdmi->conn, &mtk_hdmi_connector_helper_funcs);

	hdmi->conn.polled = DRM_CONNECTOR_POLL_HPD;
	hdmi->conn.interlace_allowed = false;
	hdmi->conn.doublescan_allowed = false;
	hdmi->conn.ycbcr_420_allowed = true;

	ret = drm_connector_attach_encoder(&hdmi->conn,
						bridge->encoder);
	if (ret) {
		HDMI_LOG("Failed to attach connector to encoder: %d\n", ret);
		return ret;
	}

	mtk_hdmi_connector_init_property(bridge->dev, &hdmi->conn);

	for (i = 0; i < MAX_CRTC; i++)
		if ((bridge->encoder->possible_crtcs >> i) & 0x1) {
			if ((i == 1) && swap_display) {
				hdmi_switch_data.crtc = 0;
				hdmi_cec_switch_data.crtc = 0;
			} else {
				hdmi_switch_data.crtc = i;
				hdmi_cec_switch_data.crtc = i;
			}
		}

#if IS_ENABLED(CONFIG_OPTEE)
	//initialize HDMI CA session
	if (hdmi_ca_init() == true) {
		/* disable all tx interrupts */
		mtk_hdmi_disable_all_int(hdmi);
		/* enable hpd interrupt */
		mtk_hdmi_set_sw_hpd(hdmi, true);
		mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
		hdmi->hdmi_enabled = true;
		hdmi->ca_init = true;
	} else {
		memset((void *)&(hdmi->ca_timer), 0, sizeof(hdmi->ca_timer));
		timer_setup(&hdmi->ca_timer, mtk_hdmi_ca_timer, 0);
		hdmi->ca_timer.expires = jiffies + msecs_to_jiffies(50);
		add_timer(&hdmi->ca_timer);
	}
#else
	/* disable all tx interrupts */
	mtk_hdmi_disable_all_int(hdmi);
	/* enable hpd interrupt */
	mtk_hdmi_set_sw_hpd(hdmi, true);
	mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
	hdmi->hdmi_enabled = true;
	hdmi->ca_init = true;
#endif

	/* request irq */
	hdmi->hdmi_irq = irq_of_parse_and_map(hdmi->dev->of_node, 0);
	if (request_threaded_irq(hdmi->hdmi_irq, NULL, mtk_hdmi_isr,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "hdmiirq", hdmi) < 0)
		HDMI_LOG("request hdmi interrupt failed.\n");
	else
		HDMI_LOG("request hdmi interrupt success\n");
	hdmi->irq_registered = true;

	if (hdmi->attached == false)
		hdmi->attached = true;

	for (i = 0; i < MAX_CRTC; i++)
		if ((bridge->encoder->possible_crtcs >> i) & 0x1) {
			if ((i == 1) && swap_display) {
				hdmi_switch_data.crtc = 0;
				hdmi_cec_switch_data.crtc = 0;
			} else {
				hdmi_switch_data.crtc = i;
				hdmi_cec_switch_data.crtc = i;
			}
		}

	HDMI_LOG("end\n");

	return 0;
}

static bool mtk_hdmi_bridge_mode_fixup(struct drm_bridge *bridge,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	HDMI_DETAIL_LOG();

	return true;
}

static void mtk_hdmi_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	HDMI_FUNC();

	if (!hdmi->enabled)
		return;

	mtk_hdmi_hw_send_av_mute(hdmi);
	usleep_range(50000, 50050);
	mtk_hdmi_hw_vid_black(hdmi, true);
	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_disable_hdcp_encrypt(hdmi);
	usleep_range(50000, 50050);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	notify_hpd_to_hdcp(hdmi->hpd);

	hdmi_hdcp_stop_task();
	vHDCPReset();
#endif
	hdmi->enabled = false;

	HDMI_LOG("end\n");
}

static void mtk_hdmi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	HDMI_LOG("start\n");

	if (!hdmi->powered)
		return;

	phy_power_off(hdmi->phy);

	hdmi->powered = false;

	reset_hdmi_hdr_status();

	/* no need audio output, notify WiredAccessoryManager hdmi plug out */
	mtk_hdmi_audio_extcon_state(hdmi, false);
	udelay(1);
	/* signal the disconnect event to audio codec */
	mtk_hdmi_handle_plugged_change(hdmi, false);

	HDMI_LOG("end\n");
}

static void mtk_hdmi_bridge_mode_set(struct drm_bridge *bridge,
				     const struct drm_display_mode *mode,
				     const struct drm_display_mode *adjusted_mode)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	HDMI_LOG("vic:%d,name:%s,hdisplay:%d,fresh_rate:%d\n",
		drm_match_cea_mode(adjusted_mode),
		adjusted_mode->name,
		adjusted_mode->hdisplay,
		drm_mode_vrefresh(adjusted_mode));

	HDMI_LOG("hsync_start:%d,hsync_end:%d,htotal:%d,hskew:%d,vdisplay:%d",
		adjusted_mode->hsync_start,
		adjusted_mode->hsync_end,
		adjusted_mode->htotal,
		adjusted_mode->hskew,
		adjusted_mode->vdisplay);

	HDMI_LOG("vsync_start:%d,vsync_end:%d,vtotal:%d,vscan:%d,flag:%d",
		adjusted_mode->vsync_start,
		adjusted_mode->vsync_end,
		adjusted_mode->vtotal,
		adjusted_mode->vscan,
		adjusted_mode->flags);

	drm_mode_copy(&hdmi->mode, adjusted_mode);
}

static void mtk_hdmi_send_infoframe(struct mtk_hdmi *hdmi,
				    struct drm_display_mode *mode)
{
	struct hdmi_vendor_infoframe frame;
	ssize_t err;

	HDMI_FUNC();
	/* mtk_hdmi_setup_audio_infoframe(hdmi); */
	mtk_hdmi_setup_avi_infoframe(hdmi, mode);
	if (hdmi->spd_exist)
		mtk_hdmi_setup_spd_infoframe(hdmi, hdmi->spd_vendor, hdmi->spd_product);
	else
		mtk_hdmi_setup_spd_infoframe(hdmi, "mediatek", "On-chip HDMI");

	memset(&frame, 0, sizeof(frame));
	err = drm_hdmi_vendor_infoframe_from_display_mode(&frame, &hdmi->conn, mode);
	if (err)
		HDMI_LOG("Failed to get VSIF from mode: %zd\n", err);

	if ((mode->flags & DRM_MODE_FLAG_3D_MASK) ||
		((frame.vic != 0) && (!(_fgLowLatencyDVEnable ||
		((_fgDVHdrEnable == true) && (_use_DV_vsif == true))))) ||
		((_fgDVHdrEnable == true) && (_use_DV_vsif == false)))
		mtk_hdmi_setup_h14b_vsif(hdmi, mode);
	if (_fgLowLatencyDVEnable ||
		((_fgDVHdrEnable == true) && (_use_DV_vsif == true)))
		mtk_hdmi_hw_dv_vsif(hdmi, true, _fgLowLatencyDVEnable,
			true, _fgBackltCtrlMDPresent, _u4EffTmaxPQ);
	mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus, _bHdrMetadataBuff);

	if (0) //can.zeng todo verify the condition
		mtk_hdmi_setup_hf_vsif(hdmi, mode);
}

static void mtk_hdmi_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	HDMI_FUNC();

	if (hdmi->lk_fastlogo) {
		hdmi->powered = true;
		hdmi->lk_fastlogo = false;
		return;
	}

	vTxSignalOnOff(hdmi->hdmi_phy_base, false);
	if ((hdmi->hdmi_phy_base->clk_parent != HDMIRX) && (ethdr_notify_hdmi_hpd != NULL)) {
		//clock from HDMIRx, repeater mode, no need ethdr setting
		ethdr_notify_hdmi_hpd(true);
	}

	mtk_hdmi_convert_colorspace_depth(hdmi);

	mtk_hdmi_output_set_display_mode(hdmi, &hdmi->mode);
	mtk_hdmi_send_infoframe(hdmi, &hdmi->mode);

	/* dpi hardware depends on hdmi txpll clock,
	 * drm call flow: bridge_pre_enable -> encoder_enable -> bridge_enable,
	 * dpi need write register in 'encoder_enable' with hdmi txpll clock,
	 * so hdmi txpll need be enabled in 'bridge_pre_enable'
	 */
	if (__clk_is_enabled(hdmi->clk[MTK_HDMI_CLK_HDMI_TXPLL]) == false)
		clk_prepare_enable(hdmi->clk[MTK_HDMI_CLK_HDMI_TXPLL]);

	hdmi->powered = true;

	HDMI_LOG("end\n");
}

static unsigned char mtk_hdmi_get_abist_format(struct mtk_hdmi *hdmi)
{
	unsigned char abist_format;
	int vic;

	vic = drm_match_cea_mode(&hdmi->mode);

	switch (vic) {
	case 2:
		HDMI_LOG("2: 720*480P@60\n");
		abist_format = 0x2;
		break;
	case 4:
		HDMI_LOG("4: 1280*720P@60\n");
		abist_format = 0x3;
		break;
	case 19:
		HDMI_LOG("19: 1280*720P@50\n");
		abist_format = 0xc;
		break;
	case 16:
		HDMI_LOG("16: 1920*1080P@60\n");
		abist_format = 0xa;
		break;
	case 31:
		HDMI_LOG("31: 1920*1080P@50\n");
		abist_format = 0x13;
		break;
	case 95:
		HDMI_LOG("95: 3840*2160P@30\n");
		abist_format = 0x19;
		break;
	case 96:
		HDMI_LOG("96: 3840*2160P@50\n");
		abist_format = 0x18;
		break;
	case 97:
		HDMI_LOG("97: 3840*2160P@60\n");
		abist_format = 0x19;
		break;
	case 100:
		HDMI_LOG("100: 4096*2160P@30\n");
		abist_format = 0x1a;
		break;
	default:
		HDMI_LOG("no abist\n");
		abist_format = 0;
	}

	return abist_format;
}

static void mtk_hdmi_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	unsigned char abist_format;

	HDMI_FUNC();

	phy_power_on(hdmi->phy);

	if (hdmi->debug_abist_OnOff == 1) {
		abist_format = mtk_hdmi_get_abist_format(hdmi);
		if (abist_format != 0) {
			mtk_hdmi_mask(hdmi, TOP_CFG00,
				(abist_format << 16), ABIST_VIDEO_FORMAT_MASKBIT);
			mtk_hdmi_mask(hdmi, TOP_CFG00,
				(hdmi->debug_abist_select_tclk << 30), ABIST_SELECT_TCLK);
			mtk_hdmi_mask(hdmi, TOP_CFG00,
				(hdmi->debug_abist_OnOff << 31), ABIST_ENABLE);
		}
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	// disable mute for HDCP
	mtk_hdmi_hw_vid_black(hdmi, false);
	mtk_hdmi_hw_aud_unmute(hdmi);
	enable_mute_for_hdcp(false);

	notify_hpd_to_hdcp(hdmi->hpd);

	hdmi_hdcp_start_task();
#else
	mtk_hdmi_hw_vid_black(hdmi, false);
	mtk_hdmi_hw_aud_unmute(hdmi);
#endif

	/* signal the connect event to audio codec */
	mtk_hdmi_handle_plugged_change(hdmi, true);
	udelay(1);
	/* need audio output, notify WiredAccessoryManager hdmi plug in */
	mtk_hdmi_audio_extcon_state(hdmi, true);

	hdmi->enabled = true;

	HDMI_LOG("end\n");
}

static const struct drm_bridge_funcs mtk_hdmi_bridge_funcs = {
	.attach = mtk_hdmi_bridge_attach,
	.mode_fixup = mtk_hdmi_bridge_mode_fixup,
	.disable = mtk_hdmi_bridge_disable,
	.post_disable = mtk_hdmi_bridge_post_disable,
	.mode_set = mtk_hdmi_bridge_mode_set,
	.pre_enable = mtk_hdmi_bridge_pre_enable,
	.enable = mtk_hdmi_bridge_enable,
};

static int mtk_hdmi_dt_parse_pdata(struct mtk_hdmi *hdmi,
				   struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	//struct device_node *cec_np,
	//struct device_node *port, *ep, *remote, *i2c_np;
	//struct device_node *remote,
	struct device_node *i2c_np;
	//struct platform_device *cec_pdev;
	struct resource *mem;
	int ret;
	struct mtk_hdmi_ddc *ddc;
	const char *spd_vendor;
	const char *spd_product;

	ret = mtk_hdmi_get_all_clk(hdmi, np);
	if (ret) {
		HDMI_LOG("Failed to get clocks: %d\n", ret);
		return ret;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdmi->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(hdmi->regs))
		return PTR_ERR(hdmi->regs);

	i2c_np = of_parse_phandle(pdev->dev.of_node, "ddc-i2c-bus", 0);
	if (!i2c_np) {
		HDMI_LOG("Failed to find ddc-i2c-bus node in %s\n",
			(pdev->dev.of_node)->full_name);
		of_node_put(pdev->dev.of_node);
		return -EINVAL;
	}
	of_node_put(pdev->dev.of_node);

	hdmi->ddc_adpt = of_find_i2c_adapter_by_node(i2c_np);
	if (!hdmi->ddc_adpt) {
		HDMI_LOG("Failed to get ddc i2c adapter by node\n");
		return -EINVAL;
	}

	if (!of_property_read_string(pdev->dev.of_node,
			"spd-vendor", &spd_vendor) &&
		!of_property_read_string(pdev->dev.of_node,
			"spd-product", &spd_product)) {
		strscpy(hdmi->spd_vendor, spd_vendor, sizeof(hdmi->spd_vendor));
		strscpy(hdmi->spd_product, spd_product, sizeof(hdmi->spd_product));
		hdmi->spd_exist = true;
	}

	//can.zeng todo verify
	ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	ddc->regs = hdmi->regs;

	return 0;
}

void mtk_hdmi_handle_plugged_change(struct mtk_hdmi *hdmi,
		bool plugged)
{
	if (hdmi->plugged_cb && hdmi->codec_dev) {
		HDMI_LOG("plugged = %d\n", plugged);
		hdmi->plugged_cb(hdmi->codec_dev, plugged);
	}
}

int mtk_hdmi_set_plugged_cb(struct mtk_hdmi *hdmi,
		hdmi_codec_plugged_cb fn, struct device *codec_dev)
{
	bool plugged;

	hdmi->plugged_cb = fn;
	hdmi->codec_dev = codec_dev;
	plugged = (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON) ? true : false;
	mtk_hdmi_handle_plugged_change(hdmi, plugged);

	return 0;
}

/*
 * HDMI audio codec callbacks
 */
static int mtk_hdmi_audio_hook_plugged_cb(struct device *dev,
	void *data, hdmi_codec_plugged_cb fn, struct device *codec_dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	HDMI_FUNC();

	if (!hdmi) {
		HDMI_LOG("invalid input\n");
		return -ENODEV;
	}

	return mtk_hdmi_set_plugged_cb(hdmi, fn, codec_dev);
}


static int mtk_hdmi_audio_hw_params(struct device *dev, void *data,
				    struct hdmi_codec_daifmt *daifmt,
				    struct hdmi_codec_params *params)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct hdmi_audio_param hdmi_params;
	unsigned int chan = params->cea.channels;

	HDMI_LOG("%u Hz, %d bit, %d channels\n",
		params->sample_rate, params->sample_width, chan);

	if (!hdmi->bridge.encoder)
		return -ENODEV;

	switch (chan) {
	case 2:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_2_0;
		break;
	case 4:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_4_0;
		break;
	case 6:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_5_1;
		break;
	case 8:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_7_1;
		break;
	default:
		HDMI_LOG("ERROR: channel[%d] not supported!\n", chan);
		return -EINVAL;
	}

	switch (params->sample_rate) {
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
	case 176400:
	case 192000:
		break;
	default:
		HDMI_LOG("ERROR: rate[%d] not supported!\n", params->sample_rate);
		return -EINVAL;
	}

	switch (daifmt->fmt) {
	case HDMI_I2S:
		hdmi_params.aud_codec = HDMI_AUDIO_CODING_TYPE_PCM;
		hdmi_params.aud_sampe_size = HDMI_AUDIO_SAMPLE_SIZE_16;
		hdmi_params.aud_input_type = HDMI_AUD_INPUT_I2S;
		hdmi_params.aud_i2s_fmt = HDMI_I2S_MODE_I2S_24BIT;
		hdmi_params.aud_mclk = HDMI_AUD_MCLK_128FS;
		break;
	default:
		HDMI_LOG("ERROR: Invalid DAI format %d\n", daifmt->fmt);
		return -EINVAL;
	}

	memcpy(&hdmi_params.codec_params, params,
	       sizeof(hdmi_params.codec_params));

	mtk_hdmi_audio_set_param(hdmi, &hdmi_params);

	return 0;
}

static int mtk_hdmi_audio_startup(struct device *dev, void *data)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	HDMI_FUNC();

	mtk_hdmi_audio_enable(hdmi);

	return 0;
}

static void mtk_hdmi_audio_shutdown(struct device *dev, void *data)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	HDMI_FUNC();

	mtk_hdmi_audio_disable(hdmi);
}


static int
mtk_hdmi_audio_mute(struct device *dev, void *data, bool enable, int direction)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	if (direction != SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	HDMI_LOG("enable=%d\n", enable);

	if (enable)
		mtk_hdmi_hw_aud_mute(hdmi);
	else
		mtk_hdmi_hw_aud_unmute(hdmi);

	return 0;
}

static int mtk_hdmi_audio_get_eld(struct device *dev, void *data, uint8_t *buf, size_t len)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct drm_connector *conn = &hdmi->conn;
	unsigned int i;

	HDMI_FUNC();

	dev_dbg(dev, "ELD data start\n");
	for (i = 0; i < 128; i += 8) {
		dev_dbg(dev, "%2x %2x %2x %2x %2x %2x %2x %2x\n",
			conn->eld[i], conn->eld[i+1], conn->eld[i+2], conn->eld[i+3],
			conn->eld[i+4], conn->eld[i+5], conn->eld[i+6], conn->eld[i+7]);
	}
	dev_dbg(dev, "ELD data end\n");

	memcpy(buf, hdmi->conn.eld, min(sizeof(hdmi->conn.eld), len));

	return 0;
}

static const struct hdmi_codec_ops mtk_hdmi_audio_codec_ops = {
	.hw_params = mtk_hdmi_audio_hw_params,
	.audio_startup = mtk_hdmi_audio_startup,
	.audio_shutdown = mtk_hdmi_audio_shutdown,
	.mute_stream = mtk_hdmi_audio_mute,
	.get_eld = mtk_hdmi_audio_get_eld,
	.hook_plugged_cb = mtk_hdmi_audio_hook_plugged_cb,
};

static void mtk_hdmi_register_audio_driver(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct hdmi_codec_pdata codec_data = {
		.ops = &mtk_hdmi_audio_codec_ops,
		.max_i2s_channels = 8,
		.i2s = 1,
		.data = hdmi,
	};
	struct platform_device *pdev;

	pdev = platform_device_register_data(dev, HDMI_CODEC_DRV_NAME,
					     PLATFORM_DEVID_AUTO, &codec_data,
					     sizeof(codec_data));
	if (IS_ERR(pdev))
		return;

	HDMI_LOG("%s driver bound to HDMI\n", HDMI_CODEC_DRV_NAME);
}

int mtk_hdmi_enable_disable(struct mtk_hdmi *hdmi, bool enable)
{
	int ret;

	HDMI_LOG("enable = %d\n", enable);

	if (enable == true) {
		if (hdmi->hdmi_phy_base->phy_data->power_control_available == true)
			mtk_hdmi_power_control(hdmi->hdmi_phy_base, true);

		if (hdmi->power_clk_enabled == false) {
			/* power domain on */
			ret = pm_runtime_get_sync(hdmi->dev);
			HDMI_LOG("[pm]Power Domain On %d\n", ret);
			/* clk on */
			mtk_hdmi_clk_enable(hdmi);

			hdmi->power_clk_enabled = true;
		} else
			HDMI_LOG("Power and clock already On\n");

		if (hdmi->irq_registered == false) {
			/* disable all tx interrupts */
			mtk_hdmi_disable_all_int(hdmi);
			/* request irq */
			hdmi->hdmi_irq = irq_of_parse_and_map(hdmi->dev->of_node, 0);
			if (request_threaded_irq(hdmi->hdmi_irq, NULL, mtk_hdmi_isr,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "hdmiirq", hdmi) < 0)
				HDMI_LOG("request hdmi interrupt failed.\n");
			else
				HDMI_LOG("request hdmi interrupt success\n");
			hdmi->irq_registered = true;
			/* enable hpd interrupt */
			mtk_hdmi_set_sw_hpd(hdmi, true);
			mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
		} else
			HDMI_LOG("IRQ already registered\n");
	} else {
		if (hdmi->irq_registered == true) {
			/* free irq */
			free_irq(hdmi->hdmi_irq, hdmi);
			hdmi->irq_registered = false;
		} else
			HDMI_LOG("IRQ already un-registered\n");

		if (hdmi->power_clk_enabled == true) {
			/* clk disable */
			mtk_hdmi_clk_disable(hdmi);
			/* power domain off */
			ret = pm_runtime_put_sync(hdmi->dev);
			HDMI_LOG("[pm]Power Domain Off %d\n", ret);

			hdmi->power_clk_enabled = false;
		} else
			HDMI_LOG("Power and clock already Off\n");

		if (hdmi->hdmi_phy_base->phy_data->power_control_available == true)
			mtk_hdmi_power_control(hdmi->hdmi_phy_base, false);
	}
	return 0;
}

int mtk_drm_ioctl_enable_hdmi(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_hdmi_enable *hdmi_enable = data;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_FUNC();
	if ((hdmi_enable == NULL) || (hdmi == NULL)) {
		HDMI_LOG("IOCTL: ERROR!\n");
		return -EFAULT;
	}

	if (hdmi->notify_user_ready == false)
		hdmi->notify_user_ready = true;

	if (hdmi_enable->enable == true) {
		if (hdmi->hdmi_enabled == true) {
			if (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON)
				hdmi_notify_uevent_user(&hdmi_switch_data, 1);
			else if (hdmi->hpd == HDMI_PLUG_IN_ONLY)
				hdmi_notify_uevent_user(&hdmi_cec_switch_data, 1);

			HDMI_LOG("HDMI has already been enabled, return\n");
			return 0;
		}
		mtk_hdmi_enable_disable(hdmi, true);
		hdmi->hdmi_enabled = true;
	} else {
		if (hdmi->hdmi_enabled == false) {
			HDMI_LOG("HDMI has already been disabled, return\n");
			return 0;
		}
		mtk_hdmi_enable_disable(hdmi, false);
		hdmi->hdmi_enabled = false;
		hdmi->hpd = HDMI_PLUG_OUT;
		hdmi->hpd_force_set = true;
		/* notify HDMI service hdmi off */
		hdmi_notify_uevent_user(&hdmi_switch_data, 0);
	}

	return 0;
}
EXPORT_SYMBOL(mtk_drm_ioctl_enable_hdmi);

static const struct mtk_hdmi_driver_data mt8195_hdmi_driver_data = {
	.reg_hdmitx_config_ofs = 0x900,
};

static const struct mtk_hdmi_driver_data mt8188_hdmi_driver_data = {
	.reg_hdmitx_config_ofs = 0xEA0,
};

static const struct of_device_id mtk_drm_hdmi_of_ids[] = {
	{ .compatible = "mediatek,mt8195-hdmi", .data = &mt8195_hdmi_driver_data},
	{ .compatible = "mediatek,mt8188-hdmi", .data = &mt8188_hdmi_driver_data},
	{}
};

static const char LK_TAG_NAME[] = "mediatek_drm.lkdisplay=";

bool mtk_hdmi_get_lk_display(void)
{
	struct device_node *of_chosen;
	char *bootargs;
	char *ptr;
	char *display_token = NULL;
	char cmd[128];

#ifdef CONFIG_MTK_DISP_NO_LK
	return false;
#endif

	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		return false;

	bootargs = (char *)of_get_property(of_chosen,
		"bootargs", NULL);
	if (!bootargs)
		return false;

	ptr = strstr(bootargs, LK_TAG_NAME);
	if (!ptr)
		return false;

	strscpy(cmd, ptr, sizeof(cmd));

	ptr = cmd;
	display_token = strsep(&ptr, " ");

	if (!display_token)
		return false;

	if (strnstr(display_token, "dpi1", strlen(display_token)))
		return true;
	return false;
}

static int mtk_drm_hdmi_probe(struct platform_device *pdev)
{
	struct mtk_hdmi *hdmi;
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	int ret;

	HDMI_LOG("probe start\n");

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->dev = dev;

	global_mtk_hdmi = hdmi;

	hdmi->phy = devm_phy_get(dev, "hdmi");
	if (IS_ERR(hdmi->phy)) {
		ret = PTR_ERR(hdmi->phy);
		HDMI_LOG("Failed to get HDMI PHY: %d\n", ret);
		return ret;
	}
	hdmi->hdmi_phy_base = phy_get_drvdata(hdmi->phy);

	of_id = of_match_device(mtk_drm_hdmi_of_ids, &pdev->dev);
	if (!of_id) {
		HDMI_LOG("HDMI device match failed\n");
		return -EPROBE_DEFER;
	}

	hdmi->driver_data = (struct mtk_hdmi_driver_data *)of_id->data;

	ret = mtk_hdmi_dt_parse_pdata(hdmi, pdev);
	if (ret)
		return ret;

	hdmi->notifier = cec_notifier_conn_register(&pdev->dev, NULL, NULL);
	if (!hdmi->notifier) {
		HDMI_LOG("Cannot get cec notifier\n");
		return -EPROBE_DEFER;
	}

	platform_set_drvdata(pdev, hdmi);

	ret = mtk_hdmi_output_init(hdmi);
	if (ret) {
		HDMI_LOG("Failed to initialize hdmi output\n");
		return ret;
	}

	mtk_hdmi_register_audio_driver(dev);

	/* register audio extcon for WiredAccessoryManager start */
	hdmi_extcon = devm_extcon_dev_allocate(&pdev->dev, hdmi_cable);
	if (IS_ERR(hdmi_extcon)) {
		HDMI_LOG("Couldn't allocate HDMI extcon device\n");
		return PTR_ERR(hdmi_extcon);
	}

	hdmi_extcon->dev.init_name = "HDMI_audio_extcon";

	ret = devm_extcon_dev_register(&pdev->dev, hdmi_extcon);
	if (ret) {
		HDMI_LOG("failed to register HDMI extcon: %d\n", ret);
		return ret;
	}
	/* register audio extcon for WiredAccessoryManager end */

	/* register uevent device */
	hdmi_switch_data.name = "hdmi";
	hdmi_switch_data.index = 0;
	hdmi_switch_data.state = 0;
	hdmi_switch_data.crtc = -1;
	ret = hdmitx_uevent_dev_register(&hdmi_switch_data);

	hdmi_cec_switch_data.name = "cec_hdmi";
	hdmi_cec_switch_data.index = 0;
	hdmi_cec_switch_data.state = 0;
	hdmi_cec_switch_data.crtc = -1;
	ret = hdmitx_uevent_dev_register(&hdmi_cec_switch_data);
	/* register uevent device */

	/* create hdmi debugfs */
	hdmitx_debug_init();

	mtk_hdmi_init_workqueue(hdmi);

	/* initialize HDR */
	vInitHdr();

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	mtk_create_hdmi_hdcp_task(hdmi);
	hdmi->enable_hdcp = true;
#endif

	INIT_KFIFO(hdmi->fifo);
	mtk_create_hdmi_hpd_task(hdmi);

	hdmi->bridge.funcs = &mtk_hdmi_bridge_funcs;
	hdmi->bridge.of_node = pdev->dev.of_node;
	drm_bridge_add(&hdmi->bridge);

	pm_runtime_enable(hdmi->dev);
	/* power domain on */
	ret = pm_runtime_get_sync(hdmi->dev);
	HDMI_LOG("[pm]Power Domain On %d\n", ret);
	/* clk on */
	mtk_hdmi_clk_enable(hdmi);
	hdmi->power_clk_enabled = true;

	if (mtk_hdmi_get_lk_display()) {
		hdmi->lk_fastlogo = true;
		HDMI_LOG("lk display HDMI");
	}
	HDMI_LOG("probe success\n");
	return 0;
}

static int mtk_drm_hdmi_remove(struct platform_device *pdev)
{
	struct mtk_hdmi *hdmi = platform_get_drvdata(pdev);

	cec_notifier_set_phys_addr(hdmi->notifier, CEC_PHYS_ADDR_INVALID);
	cec_notifier_conn_unregister(hdmi->notifier);
	drm_bridge_remove(&hdmi->bridge);
	mtk_hdmi_clk_disable(hdmi);
	hdmitx_debug_uninit();

	return 0;
}

struct ipi_cmd_s {
	unsigned int id;
	unsigned int data;
	unsigned int sus_flag;
};

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int mtk_hdmi_suspend(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	int ret;

	if (hdmi->power_clk_enabled == true) {
		/* clk disable */
		mtk_hdmi_clk_disable(hdmi);
		/* power domain off */
		ret = pm_runtime_put_sync(hdmi->dev);
		HDMI_LOG("[pm]Power Domain Off %d\n", ret);
		hdmi->power_clk_enabled = false;
	} else
		HDMI_LOG("Power and clock already Off\n");

	HDMI_LOG("hdmi suspend success\n");
	return 0;
}

static int mtk_hdmi_resume(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	int ret;

	if (hdmi->hdmi_enabled == true) {
		if (hdmi->power_clk_enabled == false) {
			/* power domain on */
			ret = pm_runtime_get_sync(hdmi->dev);
			HDMI_LOG("[pm]Power Domain On %d\n", ret);
			/* clk on */
			mtk_hdmi_clk_enable(hdmi);
			hdmi->power_clk_enabled = true;
		} else
			HDMI_LOG("Power and clock already On\n");
	}

	HDMI_LOG("hdmi resume success\n");
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_hdmi_pm_ops, mtk_hdmi_suspend, mtk_hdmi_resume);

static struct platform_driver mtk_hdmi_driver = {
	.probe = mtk_drm_hdmi_probe,
	.remove = mtk_drm_hdmi_remove,
	.driver = {
		.name = "mediatek-drm-hdmi",
		.of_match_table = mtk_drm_hdmi_of_ids,
		.pm = &mtk_hdmi_pm_ops,
	},
};

static struct platform_driver * const mtk_hdmi_drivers[] = {
	&mtk_hdmi_phy_driver,
	&mtk_hdmi_ddc_driver,
	&mtk_hdmi_driver,
};

static int __init mtk_hdmitx_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_drivers); i++) {
		ret = platform_driver_register(mtk_hdmi_drivers[i]);
		if (ret < 0) {
			HDMI_LOG("Failed to register %s driver: %d\n",
			       mtk_hdmi_drivers[i]->driver.name, ret);
			goto err;
		}
	}

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_hdmi_drivers[i]);

	return ret;
}

static void __exit mtk_hdmitx_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_hdmi_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_hdmi_drivers[i]);
}

module_init(mtk_hdmitx_init);
module_exit(mtk_hdmitx_exit);

MODULE_AUTHOR("Can Zeng <can.zeng@mediatek.com>");
MODULE_DESCRIPTION("MediaTek HDMI Driver");
MODULE_LICENSE("GPL v2");
