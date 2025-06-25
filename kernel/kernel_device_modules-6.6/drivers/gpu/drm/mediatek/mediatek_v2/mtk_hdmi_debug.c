// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>

#include "mtk_hdmi_ddc.h"
#include "mtk_hdmi_phy.h"
#include "mtk_hdmi.h"
#include "mtk_hdmi_regs.h"
#include "mtk_hdmi_hdr.h"
#include "mtk_hdmi_edid.h"
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
#include "mtk_hdmi_hdcp.h"
#endif

unsigned char mtk_hdmi_debug_log = 1;

#define HDMI_DEBUG_LOG(fmt, arg...) \
	do {	if (mtk_hdmi_debug_log) { \
		pr_info("[HDMI][DEBUG] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

#define HDMI_DEBUG_FUNC()	\
	do {	if (mtk_hdmi_debug_log) \
		pr_info("[HDMI][DEBUG] %s\n", __func__); \
	} while (0)


//void (*dpi_pattern_en)(bool enable) = NULL;
//EXPORT_SYMBOL_GPL(dpi_pattern_en);

/* 1 - 640x480@60Hz 4:3 */
struct drm_display_mode mode_640x480_60hz_4v3 = {
	DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656,
	752, 800, 0, 480, 490, 492, 525, 0,
	 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,};
EXPORT_SYMBOL(mode_640x480_60hz_4v3);

/* 2 - 720x480@60Hz 4:3 */
struct drm_display_mode mode_720x480_60hz_4v3 = {
	DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
	798, 858, 0, 480, 489, 495, 525, 0,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,};
EXPORT_SYMBOL(mode_720x480_60hz_4v3);

/* 18 - 720x576@50Hz 16:9 */
struct drm_display_mode mode_720x576_50hz_16v9 = {
	DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
	796, 864, 0, 576, 581, 586, 625, 0,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_720x576_50hz_16v9);

/* 19 - 1280x720@50Hz 16:9 */
struct drm_display_mode mode_1280x720_50hz_16v9 = {
	DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
	1760, 1980, 0, 720, 725, 730, 750, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1280x720_50hz_16v9);

/* 4 - 1280x720@60Hz 16:9 */
struct drm_display_mode mode_1280x720_60hz_16v9 = {
	DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
	1430, 1650, 0, 720, 725, 730, 750, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1280x720_60hz_16v9);

/* 16 - 1920x1080@60Hz 16:9 */
struct drm_display_mode mode_1920x1080_60hz_16v9 = {
	DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
	2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1920x1080_60hz_16v9);

/* 31 - 1920x1080@50Hz 16:9 */
struct drm_display_mode mode_1920x1080_50hz_16v9 = {
	DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
	2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1920x1080_50hz_16v9);

/* 32 - 1920x1080@24Hz 16:9 */
struct drm_display_mode mode_1920x1080_24hz_16v9 = {
	DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2558,
	2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1920x1080_24hz_16v9);

/* 33 - 1920x1080@25Hz 16:9 */
struct drm_display_mode mode_1920x1080_25hz_16v9 = {
	DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
	2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1920x1080_25hz_16v9);

/* 34 - 1920x1080@30Hz 16:9 */
struct drm_display_mode mode_1920x1080_30hz_16v9 = {
	DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
	2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_1920x1080_30hz_16v9);

/* 93 - 3840x2160@24Hz 16:9 */
struct drm_display_mode mode_3840x2160_24hz_16v9 = {
	DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 5116,
	5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_3840x2160_24hz_16v9);

/* 94 - 3840x2160@25Hz 16:9 */
struct drm_display_mode mode_3840x2160_25hz_16v9 = {
	DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4896,
	4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_3840x2160_25hz_16v9);

/* 95 - 3840x2160@30Hz 16:9 */
struct drm_display_mode mode_3840x2160_30hz_16v9 = {
	DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
	4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_3840x2160_30hz_16v9);

/* 96 - 3840x2160@50Hz 16:9 */
struct drm_display_mode mode_3840x2160_50hz_16v9 = {
	DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4896,
	4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_3840x2160_50hz_16v9);

/* 97 - 3840x2160@60Hz 16:9 */
struct drm_display_mode mode_3840x2160_60hz_16v9 = {
	DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
	4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
EXPORT_SYMBOL(mode_3840x2160_60hz_16v9);

/* 98 - 4096x2160@24Hz 256:135 */
struct drm_display_mode mode_4096x2160_24hz = {
	DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 5116,
	5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,};
EXPORT_SYMBOL(mode_4096x2160_24hz);

/* 99 - 4096x2160@25Hz 256:135 */
struct drm_display_mode mode_4096x2160_25hz = {
	DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 5064,
	5152, 5280, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,};
EXPORT_SYMBOL(mode_4096x2160_25hz);

/* 100 - 4096x2160@30Hz 256:135 */
struct drm_display_mode mode_4096x2160_30hz = {
	DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 4184,
	4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,};
EXPORT_SYMBOL(mode_4096x2160_30hz);

/* 101 - 4096x2160@50Hz 256:135 */
struct drm_display_mode mode_4096x2160_50hz = {
	DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 594000, 4096, 5064,
	5152, 5280, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,};
EXPORT_SYMBOL(mode_4096x2160_50hz);

/* 102 - 4096x2160@60Hz 256:135 */
struct drm_display_mode mode_4096x2160_60hz = {
	DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 594000, 4096, 4184,
	4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
	DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,};
EXPORT_SYMBOL(mode_4096x2160_60hz);


static char debug_buffer[2048] = {0};
static unsigned int temp_len, buf_offset;

#define HDMI_ATTR_SPRINTF(fmt, arg...)  \
do { \
	HDMI_DEBUG_LOG(fmt, ##arg); \
	if (buf_offset < (sizeof(debug_buffer) - 1)) { \
		temp_len = \
		snprintf(debug_buffer + buf_offset, \
		sizeof(debug_buffer) - buf_offset, fmt, ##arg); \
		if ((temp_len > 0) && (temp_len < (sizeof(debug_buffer) - buf_offset))) { \
			buf_offset += temp_len; \
		} else if (temp_len >= (sizeof(debug_buffer) - buf_offset)) { \
			buf_offset = 0; \
		} \
	} \
} while (0)

static void mtk_hdmi_freq_meter(struct mtk_hdmi *hdmi,
	unsigned int mode)
{
	unsigned int clockFreq;

	if (mode == 1) //pixel clock
		mtk_hdmi_mask(hdmi, 0x1a4, 0x0 << 5, 0x3 << 5);
	else if (mode == 2) //tmds clock
		mtk_hdmi_mask(hdmi, 0x1a4, 0x1 << 5, 0x3 << 5);
	else if (mode == 3) //i2s sck
		mtk_hdmi_mask(hdmi, 0x1a4, 0x2 << 5, 0x3 << 5);
	else //i2s lrck
		mtk_hdmi_mask(hdmi, 0x1a4, 0x3 << 5, 0x3 << 5);

	mtk_hdmi_write(hdmi, 0x1d4, 0x000007ff);
	mtk_hdmi_mask(hdmi, 0x1d0, 0x1 << 0, 0x1 << 0);

	udelay(40);

	clockFreq = mtk_hdmi_read(hdmi, 0x1d4);
	clockFreq = clockFreq >> 16;
	clockFreq = clockFreq * 24000 / 1024;

	if (mode == 1)
		HDMI_ATTR_SPRINTF("pixel clock freq meter is %d khz\n",
			clockFreq);
	else if (mode == 2)
		HDMI_ATTR_SPRINTF("tmds clock freq meter is %d khz\n",
			clockFreq);
	else if (mode == 3)
		HDMI_ATTR_SPRINTF("i2s Sck freq meter is %d khz\n",
			clockFreq);
	else
		HDMI_ATTR_SPRINTF("i2s LRck freq meter is %d khz\n",
			clockFreq);

	HDMI_DEBUG_LOG("clock freq meter is %d khz\n", clockFreq);
}

static unsigned int mtk_hdmi_set_txpll(
	struct mtk_hdmi *hdmi, char *str_p)
{
	unsigned long clkrate; // in HZ
	int ret;

	ret = kstrtol(str_p, 0, &clkrate);
	if (ret < 1) {
		HDMI_ATTR_SPRINTF("parameter error\n");
		return -EINVAL;
	}
	HDMI_ATTR_SPRINTF("set txpll rate: %ld HZ\n", clkrate);

	vTxSignalOnOff(hdmi->hdmi_phy_base, false);
	clk_disable_unprepare(hdmi->hdmi_phy_base->txpll);
	udelay(10);

	clk_set_rate(hdmi->hdmi_phy_base->txpll, clkrate);
	udelay(10);

	clk_prepare_enable(hdmi->hdmi_phy_base->txpll);
	vTxSignalOnOff(hdmi->hdmi_phy_base, true);

	mdelay(10);
	mtk_hdmi_freq_meter(hdmi, 2);//tmds clock
	return 0;
}

static unsigned int set_colordeep(struct mtk_hdmi *hdmi,
	char *str_p)
{
	unsigned int colorspace;
	unsigned int deepcolor;
	int ret;

	ret = sscanf(str_p, "0x%x/0x%x", &colorspace,
		&deepcolor);
	if (ret != 2) {
		HDMI_ATTR_SPRINTF("parameter error\n");
		hdmi->debug_csp_depth = false;
		return -EINVAL;
	}

	if (colorspace == HDMI_COLORSPACE_RGB) {
		hdmi->csp = HDMI_COLORSPACE_RGB;
		HDMI_ATTR_SPRINTF("HDMI_COLORSPACE_RGB\n");
	} else if (colorspace == HDMI_COLORSPACE_YUV422) {
		hdmi->csp = HDMI_COLORSPACE_YUV422;
		HDMI_ATTR_SPRINTF("HDMI_COLORSPACE_YUV422\n");
	} else if (colorspace == HDMI_COLORSPACE_YUV444) {
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		HDMI_ATTR_SPRINTF("HDMI_COLORSPACE_YUV444\n");
	} else if (colorspace == HDMI_COLORSPACE_YUV420) {
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		HDMI_ATTR_SPRINTF("HDMI_COLORSPACE_YUV420\n");
	} else {
		HDMI_ATTR_SPRINTF("Wrong Parameters\n");
		return -EINVAL;
	}

	if (deepcolor == HDMI_8_BIT) {
		hdmi->color_depth = HDMI_8_BIT;
		HDMI_ATTR_SPRINTF("HDMI_8_BIT\n");
	} else if (deepcolor == HDMI_10_BIT) {
		hdmi->color_depth = HDMI_10_BIT;
		HDMI_ATTR_SPRINTF("HDMI_10_BIT\n");
	} else if (deepcolor == HDMI_12_BIT) {
		hdmi->color_depth = HDMI_12_BIT;
		HDMI_ATTR_SPRINTF("HDMI_12_BIT\n");
	} else if (deepcolor == HDMI_16_BIT) {
		hdmi->color_depth = HDMI_16_BIT;
		HDMI_ATTR_SPRINTF("HDMI_16_BIT\n");
	} else {
		HDMI_ATTR_SPRINTF("Wrong Parameters\n");
		return -EINVAL;
	}

	hdmi->debug_csp_depth = true;

	return 0;
}

static unsigned int hdmi_read_reg(struct mtk_hdmi *hdmi, char *str_p)
{
	unsigned int u4Data = 0;
	unsigned int u4Addr = 0;
	int ret = 0;

	if (strncmp(str_p, "help", 4) == 0) {
		HDMI_ATTR_SPRINTF("rg:0xPPPPPPPP\n");
		return 0;
	}

	ret = kstrtouint(str_p, 0, &u4Addr);

	if (u4Addr >= 0x1c300000)
		u4Data = mtk_hdmi_read(hdmi, u4Addr - 0x1c300000);
	else if (u4Addr >= 0x11d5f000)
		u4Data = mtk_hdmi_phy_read(hdmi->hdmi_phy_base,
			u4Addr - 0x11d5f000);
	else
		HDMI_ATTR_SPRINTF(
	"%s the address is over of the range: 0x%08x\n",
	__func__, u4Addr);

	HDMI_ATTR_SPRINTF("0x%08x = 0x%08x\n", u4Addr, u4Data);

	return 0;
}

static unsigned int hdmi_read_reg_range(struct mtk_hdmi *hdmi, char *str_p)
{
	unsigned int u4Data = 0;
	unsigned int u4Addr = 0;
	unsigned int u4Count = 0;
	int ret = 0;

	if (strncmp(str_p, "help", 4) == 0) {
		HDMI_ATTR_SPRINTF("rg:0xPPPPPPPP/0xPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x/0x%x", &u4Addr, &u4Count);
	if (ret < 1)
		return 0;

	while (u4Count >= 4) {
		if (u4Addr >= 0x1c300000)
			u4Data = mtk_hdmi_read(hdmi, u4Addr - 0x1c300000);
		else if (u4Addr >= 0x11d5f000)
			u4Data = mtk_hdmi_phy_read(hdmi->hdmi_phy_base,
				u4Addr - 0x11d5f000);
		else
			HDMI_ATTR_SPRINTF(
	"%s the address is over of the range: 0x%08x\n",
	__func__, u4Addr);

		HDMI_ATTR_SPRINTF("0x%08x = 0x%08x\n", u4Addr, u4Data);
		udelay(2);
		u4Addr = u4Addr + 4;
		u4Count = u4Count - 4;
	}

	return 0;
}

static unsigned int hdmi_write_reg(struct mtk_hdmi *hdmi, char *str_p)
{
	unsigned int u4Data = 0;
	unsigned int u4Addr = 0;
	int ret = 0;

	if (strncmp(str_p, "help", 4) == 0) {
		HDMI_ATTR_SPRINTF("wg:0xPPPPPPPP=0xPPPPPPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x=0x%x", &u4Addr, &u4Data);
	if (ret < 1)
		return 0;

	if (u4Addr >= 0x1c300000)
		mtk_hdmi_write(hdmi, u4Addr - 0x1c300000, u4Data);
	else if (u4Addr >= 0x11d5f000)
		mtk_hdmi_phy_write(hdmi->hdmi_phy_base,
			u4Addr - 0x11d5f000, u4Data);
	else
		HDMI_ATTR_SPRINTF(
	"%s the address is over of the range: 0x%08x\n",
	__func__, u4Addr);

	HDMI_ATTR_SPRINTF("0x%08x = 0x%08x\n", u4Addr, u4Data);

	return 0;
}

static unsigned int set_abist_pattern(
	struct mtk_hdmi *hdmi, char *str_p)
{
	unsigned int ret;
	unsigned int mode_num, OnOff, select_tclk;
	unsigned char abist_format;
	struct drm_display_mode *mode;

	ret = sscanf(str_p, "0x%x/0x%x/0x%x", &OnOff, &mode_num, &select_tclk);
	if (ret != 3)
		return 0;

	HDMI_DEBUG_LOG("OnOff:%d, mode_num:%d, select_tclk:%d\n", OnOff, mode_num, select_tclk);

	switch (mode_num) {
	case 0x2:
		HDMI_ATTR_SPRINTF("0x2: 720*480P@60\n");
		mode = &mode_720x480_60hz_4v3;
		abist_format = 0x2;
		break;
	case 0x3:
		HDMI_ATTR_SPRINTF("0x3: 1280*720P@60\n");
		mode = &mode_1280x720_60hz_16v9;
		abist_format = 0x3;
		break;
	case 0x4:
		HDMI_ATTR_SPRINTF("0x4: 1280*720P@50\n");
		mode = &mode_1280x720_50hz_16v9;
		abist_format = 0xc;
		break;
	case 0x5:
		HDMI_ATTR_SPRINTF("0x5: 1920*1080P@60\n");
		mode = &mode_1920x1080_60hz_16v9;
		abist_format = 0xa;
		break;
	case 0x6:
		HDMI_ATTR_SPRINTF("0x6: 1920*1080P@50\n");
		mode = &mode_1920x1080_50hz_16v9;
		abist_format = 0x13;
		break;
	case 0x7:
		HDMI_ATTR_SPRINTF("0x7: 3840*2160P@30\n");
		mode = &mode_3840x2160_30hz_16v9;
		abist_format = 0x19;
		break;
	case 0x8:
		HDMI_ATTR_SPRINTF("0x8: 3840*2160P@50\n");
		mode = &mode_3840x2160_50hz_16v9;
		abist_format = 0x18;
		break;
	case 0x9:
		HDMI_ATTR_SPRINTF("0x9: 3840*2160P@60\n");
		mode = &mode_3840x2160_60hz_16v9;
		abist_format = 0x19;
		break;
	case 0xa:
		HDMI_ATTR_SPRINTF("0xa: 4096*2160P@30\n");
		mode = &mode_4096x2160_30hz;
		abist_format = 0x1a;
		break;
	default:
		HDMI_ATTR_SPRINTF("Wrong Mode!!\n");
		mode = &mode_720x480_60hz_4v3;
		abist_format = 0x2;
	}

	if (hdmi->enabled == true)
		drm_bridge_chain_disable(&hdmi->bridge);
	if (hdmi->powered == true)
		drm_bridge_chain_post_disable(&hdmi->bridge);

	msleep(50);

	drm_bridge_chain_mode_set(&hdmi->bridge, NULL, mode);
	drm_bridge_chain_pre_enable(&hdmi->bridge);
	drm_bridge_chain_enable(&hdmi->bridge);

	mtk_hdmi_mask(hdmi, TOP_CFG00, (abist_format << 16),
		ABIST_VIDEO_FORMAT_MASKBIT);
	mtk_hdmi_mask(hdmi, TOP_CFG00, (select_tclk << 30), ABIST_SELECT_TCLK);
	mtk_hdmi_mask(hdmi, TOP_CFG00, (OnOff << 31), ABIST_ENABLE);

	return 0;
}

static unsigned int enable_abist_pattern(
	struct mtk_hdmi *hdmi, char *str_p)
{
	unsigned int ret;
	unsigned int OnOff, select_tclk;

	ret = sscanf(str_p, "0x%x/0x%x", &OnOff, &select_tclk);
	if (ret != 2)
		return 0;

	HDMI_DEBUG_LOG("OnOff:%d, select_tclk:%d\n", OnOff, select_tclk);

	hdmi->debug_abist_OnOff = OnOff;
	hdmi->debug_abist_select_tclk = select_tclk;

	return 0;
}

void set_dpi_pattern(struct mtk_hdmi *hdmi, char *str_p)
{
	const struct drm_encoder_helper_funcs *funcs;
	struct drm_bridge *bridge;
	struct drm_encoder *dpi_encoder;
	struct drm_display_mode *mode;
	unsigned int OnOff, mode_num;
	int ret;

	ret = sscanf(str_p, "0x%x/0x%x", &OnOff, &mode_num);
	if (ret < 1)
		return;

	HDMI_DEBUG_LOG("OnOff:%d, mode_num:%d\n", OnOff, mode_num);

	bridge = &hdmi->bridge;
	dpi_encoder = bridge->encoder;
	funcs = dpi_encoder->helper_private;

	switch (mode_num) {
	case 0x2:
		HDMI_ATTR_SPRINTF("0x2: 720*480P@60\n");
		mode = &mode_720x480_60hz_4v3;
		break;
	case 0x3:
		HDMI_ATTR_SPRINTF("0x3: 1280*720P@60\n");
		mode = &mode_1280x720_60hz_16v9;
		break;
	case 0x4:
		HDMI_ATTR_SPRINTF("0x4: 1280*720P@50\n");
		mode = &mode_1280x720_50hz_16v9;
		break;
	case 0x5:
		HDMI_ATTR_SPRINTF("0x5: 1920*1080P@60\n");
		mode = &mode_1920x1080_60hz_16v9;
		break;
	case 0x6:
		HDMI_ATTR_SPRINTF("0x6: 1920*1080P@50\n");
		mode = &mode_1920x1080_50hz_16v9;
		break;
	case 0x7:
		HDMI_ATTR_SPRINTF("0x7: 3840*2160P@30\n");
		mode = &mode_3840x2160_30hz_16v9;
		break;
	case 0x8:
		HDMI_ATTR_SPRINTF("0x8: 3840*2160P@50\n");
		mode = &mode_3840x2160_50hz_16v9;
		break;
	case 0x9:
		HDMI_ATTR_SPRINTF("0x9: 3840*2160P@60\n");
		mode = &mode_3840x2160_60hz_16v9;
		break;
	case 0xa:
		HDMI_ATTR_SPRINTF("0xa: 4096*2160P@30\n");
		mode = &mode_4096x2160_30hz;
		break;
	default:
		HDMI_ATTR_SPRINTF("Wrong Mode!!\n");
		mode = &mode_720x480_60hz_4v3;
	}

	if (hdmi->enabled == true)
		drm_bridge_chain_disable(&hdmi->bridge);
	if (hdmi->powered == true)
		drm_bridge_chain_post_disable(&hdmi->bridge);

	msleep(50);

	funcs->mode_set(dpi_encoder, NULL, mode);
	drm_bridge_chain_mode_set(bridge, NULL, mode);

	drm_bridge_chain_pre_enable(bridge);
	funcs->enable(dpi_encoder);

	if (dpi_pattern_en != NULL) {
		if (OnOff != 0)
			dpi_pattern_en(true);
		else
			dpi_pattern_en(false);
	}

	drm_bridge_chain_enable(bridge);
	mtk_hdmi_mask(hdmi, TOP_CFG00, (0x0 << 31), ABIST_ENABLE);
}

void vShowEdidRawData(unsigned char *edid)
{
	unsigned short bTemp, i, j;
	unsigned char *p;

	for (bTemp = 0; bTemp < 2; bTemp++) {
		HDMI_ATTR_SPRINTF(
"===================================================================\n");
		HDMI_ATTR_SPRINTF("   EDID Block Number=#%d\n", bTemp);
		HDMI_ATTR_SPRINTF(
"   | 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f\n");
		HDMI_ATTR_SPRINTF(
"===================================================================\n");
		j = bTemp * EDID_BLOCK_LEN;
		for (i = 0; i < 8; i++) {
			p = &edid[j + i * 16];
			HDMI_ATTR_SPRINTF(
"%02x:  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  ",
i * 16 + j, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
			HDMI_ATTR_SPRINTF("%02x  %02x  %02x  %02x  %02x  %02x\n",
				p[10], p[11], p[12], p[13], p[14], p[15]);
		}
	}
	HDMI_ATTR_SPRINTF("===========================================\n");

}

void hdmi_load_firmware_edid(struct mtk_hdmi *hdmi, char *str_p, size_t count)
{
	unsigned int ret, i;

	memset(hdmi->firmware_edid, 0, EDID_LENGTH * 4);

	for (i = 0; (i < count) && (i < (EDID_LENGTH * 4)); i++) {
		ret = sscanf(str_p + (i * 3), "%x", &hdmi->firmware_edid[i]);
		if (ret < 1) {
			HDMI_ATTR_SPRINTF("parameter error\n");
			return;
		}
	}

	vShowEdidRawData(hdmi->firmware_edid);
}

void hdmi_force_use_firmware_edid(struct mtk_hdmi *hdmi, char *str_p)
{
	int ret;
	unsigned int is_firmware_edid = 0;

	ret = kstrtouint(str_p, 0, &is_firmware_edid);

	if (is_firmware_edid == 0) {
		hdmi->use_firmware_edid = false;
		HDMI_ATTR_SPRINTF("NOT use firmware EDID\n");
	} else {
		hdmi->use_firmware_edid = true;
		HDMI_ATTR_SPRINTF("use firmware EDID\n");
	}
}

void hdmi_force_hpd(struct mtk_hdmi *hdmi, char *str_p)
{
	int ret;
	unsigned int force_hpd = 0;

	ret = kstrtouint(str_p, 0, &force_hpd);

	if (force_hpd == 0) {
		hdmi->hpd = HDMI_PLUG_OUT;
		force_hpd_uevent(0);
		HDMI_ATTR_SPRINTF("force notify hdmi plug out\n");
	} else {
		hdmi->hpd = HDMI_PLUG_IN_AND_SINK_POWER_ON;
		force_hpd_uevent(1);
		HDMI_ATTR_SPRINTF("force notify hdmi plug in\n");
	}
}

static void _HdrEnable(char *str_p)
{
	unsigned int ret;
	unsigned int fgHdrEnable, fgBT2020Enable, fgDVHdrEnable;
	unsigned int use_DV_vsif;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	ret = sscanf(str_p, "0x%x/0x%x/0x%x/0x%x", &fgHdrEnable,
		&fgBT2020Enable, &fgDVHdrEnable, &use_DV_vsif);
	if (ret < 1)
		return;

	if (fgHdrEnable == 3) {
		_bStaticHdrStatus = HDR_PACKET_DISABLE;
		mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus,
		_bHdrMetadataBuff);
	} else if (fgHdrEnable == 4) {
		_bStaticHdrStatus = HDR_PACKET_ACTIVE;
		mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus,
		_bHdrMetadataBuff);
	} else if (fgHdrEnable == 5) {
		_bStaticHdrStatus = HDR_PACKET_ZERO;
		mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus,
		_bHdrMetadataBuff);
	} else {
		vHdr10Enable(fgHdrEnable);
		vBT2020Enable(fgBT2020Enable);
		if (use_DV_vsif != 0)
			vDVHdrEnable(fgDVHdrEnable, true);
		else
			vDVHdrEnable(fgDVHdrEnable, false);
	}
}

void mtk_hdmi_power_clock_enable(struct mtk_hdmi *hdmi, bool enable)
{
	HDMI_DEBUG_LOG("enable: %d\n", enable);

	if (enable == true) {
		if (hdmi->power_clk_enabled == false) {
			pm_runtime_get_sync(hdmi->dev);
			mtk_hdmi_clk_enable(hdmi);
			hdmi->power_clk_enabled = true;
		}
	} else {
		if (hdmi->power_clk_enabled == true) {
			mtk_hdmi_clk_disable(hdmi);
			pm_runtime_put_sync(hdmi->dev);
			hdmi->power_clk_enabled = false;
		}
	}
}

void ShowHpdStatus(struct mtk_hdmi *hdmi)
{
	if (hdmi->hpd == HDMI_PLUG_OUT)
		HDMI_ATTR_SPRINTF("HDMI_PLUG_OUT\n");
	else if (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON)
		HDMI_ATTR_SPRINTF("HDMI_PLUG_IN_AND_SINK_POWER_ON\n");
	else if (hdmi->hpd == HDMI_PLUG_IN_ONLY)
		HDMI_ATTR_SPRINTF("HDMI_PLUG_IN_ONLY\n");
	else
		HDMI_ATTR_SPRINTF("HPD unknown state\n");
}

void ShowVideoInfo(struct mtk_hdmi *hdmi)
{
	if (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON) {
		if (hdmi->use_firmware_edid == true)
			HDMI_ATTR_SPRINTF("Use Firmware EDID\n");
		else
			HDMI_ATTR_SPRINTF("Use Real EDID\n");

		if (hdmi->dvi_mode == true)
			HDMI_ATTR_SPRINTF("DVI mode\n");
		else
			HDMI_ATTR_SPRINTF("HDMI mode\n");

		HDMI_ATTR_SPRINTF("Resolution: %s, fresh_rate %dHZ\n",
			hdmi->mode.name, drm_mode_vrefresh(&hdmi->mode));

		if (hdmi->csp == HDMI_COLORSPACE_RGB)
			HDMI_ATTR_SPRINTF("Color Space: RGB\n");
		else if (hdmi->csp == HDMI_COLORSPACE_YUV422)
			HDMI_ATTR_SPRINTF("Color Space: YUV422\n");
		else if (hdmi->csp == HDMI_COLORSPACE_YUV444)
			HDMI_ATTR_SPRINTF("Color Space: YUV444\n");
		else if (hdmi->csp == HDMI_COLORSPACE_YUV420)
			HDMI_ATTR_SPRINTF("Color Space: YUV420\n");

		if (hdmi->color_depth == HDMI_8_BIT)
			HDMI_ATTR_SPRINTF("Color Depth: 8 Bit\n");
		else if (hdmi->color_depth == HDMI_10_BIT)
			HDMI_ATTR_SPRINTF("Color Depth: 10 Bit\n");
		else if (hdmi->color_depth == HDMI_12_BIT)
			HDMI_ATTR_SPRINTF("Color Depth: 12 Bit\n");
		else if (hdmi->color_depth == HDMI_16_BIT)
			HDMI_ATTR_SPRINTF("Color Depth: 16 Bit\n");
	}
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
void vShowHdcpStatus(struct mtk_hdmi *hdmi)
{
	HDMI_ATTR_SPRINTF("[hdcp]enalbe_hdcp=%d\n", hdmi->enable_hdcp);
	HDMI_ATTR_SPRINTF("[hdcp]hdcp2_version_flag=%d\n", hdmi->hdcp_2x_support);
	HDMI_ATTR_SPRINTF("[hdcp]hdcp_key_load_status=%d\n", hdmi->key_is_installed);

	if ((mtk_hdmi_read(hdmi, HDCP1x_STATUS) &
		HDCP_ENCRYPTING_ON) == HDCP_ENCRYPTING_ON)
		HDMI_ATTR_SPRINTF("[hdcp1]HDCP_ENCRYPTING_ON\n");
	else
		HDMI_ATTR_SPRINTF("[hdcp1]HDCP_ENCRYPTING_OFF\n");
	if ((mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) &
		HDCP2X_ENCRYPTING_ON) == HDCP2X_ENCRYPTING_ON)
		HDMI_ATTR_SPRINTF("[hdcp2]HDCP2X_ENCRYPTING_ON\n");
	else
		HDMI_ATTR_SPRINTF("[hdcp2]HDCP2X_ENCRYPTING_OFF\n");
}
#endif

void hdr_status(void)
{
	switch (_bHdrType) {
	case VID_PLA_DR_TYPE_SDR:
		HDMI_ATTR_SPRINTF("HDR status: SDR\n");
		break;
	case VID_PLA_DR_TYPE_DOVI:
		HDMI_ATTR_SPRINTF("HDR status: DolbyVision-Std\n");
		break;
	case VID_PLA_DR_TYPE_DOVI_LOWLATENCY:
		HDMI_ATTR_SPRINTF("HDR status: DolbyVision-Lowlatency\n");
		break;
	case VID_PLA_DR_TYPE_HDR10:
		if (_bStaticHdrType == GAMMA_HLG)
			HDMI_ATTR_SPRINTF("HDR status: HDR10-GAMMA_HLG\n");
		else if (_bStaticHdrType == GAMMA_ST2084)
			HDMI_ATTR_SPRINTF("HDR status: HDR10-GAMMA_ST2084\n");
		else
			HDMI_ATTR_SPRINTF("HDR status: HDR10-others\n");
		break;
	case VID_PLA_DR_TYPE_HDR10_PLUS:
		HDMI_ATTR_SPRINTF("HDR status: HDR10Plus-EMP\n");
		break;
	case VID_PLA_DR_TYPE_HDR10_PLUS_VSIF:
		HDMI_ATTR_SPRINTF("HDR status: HDR10Plus-VSIF\n");
		break;
	default:
		HDMI_ATTR_SPRINTF("HDR status: Error HDR type\n");
	}

}

void hdmi_status(struct mtk_hdmi *hdmi)
{
	//HPD status
	ShowHpdStatus(hdmi);
	//Resolution Timing parameter, Color Space, Color Depth
	ShowVideoInfo(hdmi);
	//HDR status
	hdr_status();
	//HDCP status
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
	vShowHdcpStatus(hdmi);
#endif
	//EDID info
	vShowEdidRawData(hdmi->raw_edid.edid);
	vShowEdidInformation();
}

static void process_dbg_cmd(char *opt, size_t count)
{
	unsigned int ret, i, size, en;
	char testbuf[500] = {0};
	unsigned int use_DV_vsif;
	struct VID_PLA_HDR_METADATA_INFO_T hdr_metadata;
	struct mtk_hdmi *hdmi;

	if (global_mtk_hdmi != NULL)
		hdmi = global_mtk_hdmi;
	else {
		HDMI_ATTR_SPRINTF("mtk_hdmi structure is NULL\n");
		HDMI_DEBUG_LOG("mtk_hdmi structure is NULL\n");
		return;
	}

	if (strncmp(opt, "settxpll:", 9) == 0)
		mtk_hdmi_set_txpll(hdmi, opt + 9);
	else if (strncmp(opt, "codeep:", 7) == 0)
		set_colordeep(hdmi, opt + 7);
	else if (strncmp(opt, "abist:", 6) == 0)
		set_abist_pattern(hdmi, opt + 6);
	else if (strncmp(opt, "enable_abist:", 13) == 0)
		enable_abist_pattern(hdmi, opt + 13);
	else if (strncmp(opt, "dpi_pattern:", 12) == 0)
		set_dpi_pattern(hdmi, opt + 12);
	else if (strncmp(opt, "color_bar_on", 12) == 0) {
		if (dpi_pattern_en != NULL) {
			HDMI_DEBUG_LOG("dpi pattern on\n");
			dpi_pattern_en(true);
		}
	} else if (strncmp(opt, "color_bar_off", 13) == 0) {
		if (dpi_pattern_en != NULL) {
			HDMI_DEBUG_LOG("dpi pattern off\n");
			dpi_pattern_en(false);
		}
	} else if (strncmp(opt, "power_clock_on", 14) == 0)
		mtk_hdmi_power_clock_enable(hdmi, true);
	else if (strncmp(opt, "power_clock_off", 15) == 0)
		mtk_hdmi_power_clock_enable(hdmi, false);
	else if (strncmp(opt, "status", 6) == 0)
		hdmi_status(hdmi);
	else if (strncmp(opt, "firmware_edid:", 14) == 0)
		hdmi_load_firmware_edid(hdmi, opt + 14, count - 14);
	else if (strncmp(opt, "use_firmware_edid:", 18) == 0)
		hdmi_force_use_firmware_edid(hdmi, opt + 18);
	else if (strncmp(opt, "force_hpd:", 10) == 0)
		hdmi_force_hpd(hdmi, opt + 10);
	else if (strncmp(opt, "audreset", 8) == 0) {
		mtk_hdmi_audio_reset(hdmi, true);
	} else if (strncmp(opt, "audpacket:", 10) == 0) {
		ret = sscanf(opt+10, "%x", &en);
		if (ret < 1)
			return;
		mtk_hdmi_hw_send_aud_packet(hdmi, !!en);

	} else if (strncmp(opt, "vidmute:", 8) == 0) {
		ret = sscanf(opt+8, "%x", &en);
		if (ret < 1)
			return;
		mtk_hdmi_hw_vid_black(hdmi, !!en);

	} else if (strncmp(opt, "audmute:", 8) == 0) {
		ret = sscanf(opt+8, "%x", &en);
		if (ret < 1)
			return;
		if (en)
			mtk_hdmi_hw_aud_mute(hdmi);
		else
			mtk_hdmi_hw_aud_unmute(hdmi);
	} else if (strncmp(opt, "getfreqmeter:", 13) == 0) {
		unsigned int mode;

		ret = sscanf(opt+13, "%x", &mode);
		if (ret < 1)
			return;
		mtk_hdmi_freq_meter(hdmi, mode);
	} else if (strncmp(opt, "rrg:", 4) == 0)
		hdmi_read_reg_range(hdmi, opt + 4);
	else if (strncmp(opt, "rg:", 3) == 0)
		hdmi_read_reg(hdmi, opt + 3);
	else if (strncmp(opt, "wg:", 3) == 0)
		hdmi_write_reg(hdmi, opt + 3);
	else if (strncmp(opt, "hdren:", 6) == 0)
		_HdrEnable(opt + 6);
	else if (strncmp(opt, "sdr", 3) == 0)
		vHdr10PlusEnable(false);
	else if (strncmp(opt, "dovistd:", 8) == 0) {
		ret = sscanf(opt+8, "%x/%x", &en, &use_DV_vsif);
		if (ret < 1)
			return;
		vHdr10PlusEnable(false);
		if (use_DV_vsif != 0)
			vDVHdrEnable(en, true);
		else
			vDVHdrEnable(en, false);
	} else if (strncmp(opt, "dovill:", 7) == 0) {
		ret = sscanf(opt+7, "%x", &en);
		if (ret < 1)
			return;
		if (en) {
			vHdr10PlusEnable(false);
			vLowLatencyDVEnable(false);
			hdr_metadata.e_DynamicRangeType =
			 VID_PLA_DR_TYPE_DOVI_LOWLATENCY;
			hdr_metadata.metadata_info
			 .dovi_lowlatency_metadata
			 .fgBackltCtrlMdPresent = 0;
			hdr_metadata.metadata_info
			 .dovi_lowlatency_metadata.ui4_EffTmaxPQ = 0;
			hdr_metadata.fgIsMetadata = 1;
			vVdpSetHdrMetadata(true, hdr_metadata);
			vLowLatencyDVEnable(true);
		} else
			vLowLatencyDVEnable(false);
	} else if (strncmp(opt, "hlg:", 4) == 0) {
		ret = sscanf(opt+4, "%x", &en);
		if (ret < 1)
			return;
		if (en) {
			vHdr10PlusEnable(false);
			vHdr10Enable(false);
			hdr_metadata.e_DynamicRangeType = VID_PLA_DR_TYPE_HDR10;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_DisplayPrimariesX[0] = 0x8a48;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_DisplayPrimariesY[0] = 0x3908;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_DisplayPrimariesX[1] = 0x2134;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_DisplayPrimariesY[1] = 0x9baa;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_DisplayPrimariesX[2] = 0x1996;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_DisplayPrimariesY[2] = 0x1996;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_WhitePointX = 0x3d13;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_WhitePointY = 0x4042;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_MaxDisplayMasteringLuminance = 0xfa0;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_MinDisplayMasteringLuminance = 0x32;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_MaxCLL = 0x2222;
			hdr_metadata.metadata_info.hdr10_metadata
			 .ui2_MaxFALL = 0x79;
			hdr_metadata.fgIsMetadata = 1;
			vSetStaticHdrType(GAMMA_HLG);
			vVdpSetHdrMetadata(true, hdr_metadata);
			vHdr10Enable(true);
		} else
			vHdr10Enable(false);
	} else if (strncmp(opt, "hdr10gst:", 9) == 0) {
		ret = sscanf(opt+9, "%x", &en);
		if (ret < 1)
			return;
		HDMI_ATTR_SPRINTF("en=%x\n", en);
		if (en) {
			vHdr10PlusEnable(false);
			vHdr10Enable(false);
			hdr_metadata.e_DynamicRangeType = VID_PLA_DR_TYPE_HDR10;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_DisplayPrimariesX[0] = 0x8a48;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_DisplayPrimariesY[0] = 0x3908;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_DisplayPrimariesX[1] = 0x2134;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_DisplayPrimariesY[1] = 0x9baa;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_DisplayPrimariesX[2] = 0x1996;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_DisplayPrimariesY[2] = 0x1996;
			hdr_metadata.metadata_info
			 .hdr10_metadata.ui2_WhitePointX = 0x3d13;
			hdr_metadata.metadata_info
			 .hdr10_metadata.ui2_WhitePointY = 0x4042;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_MaxDisplayMasteringLuminance = 0xfa0;
			hdr_metadata.metadata_info
			 .hdr10_metadata
			 .ui2_MinDisplayMasteringLuminance = 0x32;
			hdr_metadata.metadata_info
			 .hdr10_metadata.ui2_MaxCLL = 0x2222;
			hdr_metadata.metadata_info
			 .hdr10_metadata.ui2_MaxFALL = 0x79;
			hdr_metadata.fgIsMetadata = 1;
			vSetStaticHdrType(GAMMA_ST2084);
			vVdpSetHdrMetadata(true, hdr_metadata);
			vHdr10Enable(true);
		} else
			vHdr10Enable(false);
	} else if (strncmp(opt, "hdr10emp:", 9) == 0) {
		ret = sscanf(opt+9, "%x", &en);
		if (ret < 1)
			return;
		HDMI_ATTR_SPRINTF("en=%x\n", en);
		if (en) {
			vHdr10PlusEnable(false);
			hdr_metadata.e_DynamicRangeType =
				VID_PLA_DR_TYPE_HDR10_PLUS;
			size = 300;
			hdr_metadata.metadata_info
				.hdr10_plus_metadata.hdr10p_metadata_info
				.ui4_Hdr10PlusSize =
					size;
			hdr_metadata.metadata_info
				.hdr10_plus_metadata.hdr10p_metadata_info
				.ui4_Hdr10PlusAddr =
				(unsigned long)(&testbuf);
			for (i = 0; i < size; i++)
				testbuf[i] = (i % 255);
			hdr_metadata.fgIsMetadata = 1;
			vVdpSetHdrMetadata(true, hdr_metadata);
			vHdr10PlusEnable(true);
		} else
			vHdr10PlusEnable(false);
	} else if (strncmp(opt, "hdr10pvsif:", 11) == 0) {
		ret = sscanf(opt+11, "%x", &en);
		if (ret < 1)
			return;
		if (en) {
			vHdr10PlusEnable(false);
			vHdr10PlusVSIFEnable(false);
			hdr_metadata.e_DynamicRangeType =
				VID_PLA_DR_TYPE_HDR10_PLUS_VSIF;
			size = 27;
			hdr_metadata.metadata_info
				.hdr10_plus_metadata
				.hdr10p_metadata_info.ui4_Hdr10PlusSize =
				size;
			hdr_metadata.metadata_info
				.hdr10_plus_metadata
				.hdr10p_metadata_info.ui4_Hdr10PlusAddr =
				(unsigned long)(&testbuf);
			for (i = 0; i < size; i++)
				testbuf[i] = (i % 255);
			hdr_metadata.fgIsMetadata = 1;
			vVdpSetHdrMetadata(true, hdr_metadata);
			vHdr10PlusVSIFEnable(true);
		} else
			vHdr10PlusVSIFEnable(false);
	} else if (strncmp(opt, "power:", 6) == 0) {
		ret = sscanf(opt+6, "%x", &en);
		if (ret < 1)
			return;
		if (hdmi->hdmi_phy_base->phy_data->power_control_available == true) {
			if (en)
				mtk_hdmi_power_control(hdmi->hdmi_phy_base, true);
			else
				mtk_hdmi_power_control(hdmi->hdmi_phy_base, false);
		}
	} else if (strncmp(opt, "ALLM:", 5) == 0) {
		ret = sscanf(opt+5, "%x", &en);
		if (ret < 1)
			return;
		HDMI_DEBUG_LOG("sink allm support:%d\n", _HdmiSinkAvCap.u1_sink_allm_support);
		if (en) {
			_HdmiSinkAvCap.u1_sink_allm_support = 1;
			hdmi->ALLM_enabled = true;
			mtk_hdmi_setup_hf_vsif(hdmi, &hdmi->mode);
		} else {
			hdmi->ALLM_enabled = false;
			mtk_hdmi_setup_hf_vsif(hdmi, &hdmi->mode);
		}
	}
}

struct dentry *hdmitx_debugfs;

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;

	n = strlen(debug_buffer);
	if ((n >= 0) && (n < sizeof(debug_buffer)))
		debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	ssize_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;
	buf_offset = 0;

	process_dbg_cmd(debug_buffer, count);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

int hdmitx_debug_init(void)
{
	hdmitx_debugfs = debugfs_create_file("hdmitx",
		S_IFREG | 0444, NULL, (void *)0, &debug_fops);

	if (IS_ERR(hdmitx_debugfs))
		return PTR_ERR(hdmitx_debugfs);

	return 0;
}

void hdmitx_debug_uninit(void)
{
	debugfs_remove(hdmitx_debugfs);
}

