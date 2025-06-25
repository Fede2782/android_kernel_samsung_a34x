// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#include "include/panel-nt36672c-fhdp-dsi-vdo-dsc-txd-boe.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int error;
};

static int current_fps = 60;
static u32 bdg_support = 1;
static struct AW37501_SETTING_TABLE aw37501_cmd_data[5] = {
	{ 0x00, 0x12 }, //AVDD 5.8
	{ 0x01, 0x12 }, //AVEE 5.8
	{ 0x03, 0x43 }, //Applications configure register,default:0x43
	{ 0x04, 0x01 }, //Control State configure register,default:0x01
	{ 0x21, 0x00 }  //Written protect functional register,default:0x00
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif
static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}


#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;

static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		dev_info("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		dev_info("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
	if (ret < 0)
		dev_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		dev_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		dev_info("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		dev_info("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		dev_info("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		dev_info("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
	aw37501_write_bytes(aw37501_cmd_data[0].cmd, aw37501_cmd_data[0].data);
	udelay(1000);
	aw37501_write_bytes(aw37501_cmd_data[1].cmd, aw37501_cmd_data[1].data);
	udelay(1000);
	aw37501_write_bytes(aw37501_cmd_data[2].cmd, aw37501_cmd_data[2].data);
	udelay(1000);
	aw37501_write_bytes(aw37501_cmd_data[3].cmd, aw37501_cmd_data[3].data);
	udelay(1000);
	aw37501_write_bytes(aw37501_cmd_data[4].cmd, aw37501_cmd_data[4].data);
	udelay(5000);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	pr_info("%s, fps:%d\n", __func__, current_fps);

	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xC1, 0x89,0x28,0x00,0x14,0x00,0xAA,0x02,0x0E,0x00,0x71,0x00,0x07,0x05,0x0E,0x05,0x16);
	udelay(1000);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x1B,0xA0);
	udelay(1000);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x66);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x3C);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0x90);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0xE6);
	lcm_dcs_write_seq_static(ctx, 0x69, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0x95, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0x96, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x64);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x64);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x64);
	lcm_dcs_write_seq_static(ctx, 0xF8, 0x64);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x06, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x07, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x2F);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x2E);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x14, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x23, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x2F);
	lcm_dcs_write_seq_static(ctx, 0x28, 0x2E);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x2D, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x30, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x32, 0x44);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x34, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x37, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x48);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x48);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x48);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0x83);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x93);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xA0, 0x13);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x13);
	lcm_dcs_write_seq_static(ctx, 0xA3, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA4, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x03);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0x19, 0xE4);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x58, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x66, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6C, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x62);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x90);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x6D);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0xD5);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xC5, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xEF, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x04);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x01, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x02, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x04, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x14, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x74, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0x42);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x27);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x9C);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x21, 0xDF);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x82);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x6E, 0x9A);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x78);
	lcm_dcs_write_seq_static(ctx, 0x70, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x23);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x83, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x02);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x91);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x50);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0xF5);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x71);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x45);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x36);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x4D);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x63);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x4D);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x28, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x55);
	lcm_dcs_write_seq_static(ctx, 0x2D, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x30, 0x45);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x3E);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x68);
	lcm_dcs_write_seq_static(ctx, 0x34, 0xF9);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x33);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x37, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x39, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x45);
	lcm_dcs_write_seq_static(ctx, 0xEE, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0xF5, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2B);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0x13);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x01);


	lcm_dcs_write_seq_static(ctx, 0xFF, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x82);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x52);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x02);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x11);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);

	lcm_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(100);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x00);
	msleep(20);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;
	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	if (ctx->gate_ic == 0) {
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		udelay(5000);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#else
	if (ctx->gate_ic == 0) {

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		udelay(1000);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	}
#endif

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 291004,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_1 = {
	.clock = 291004,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};
#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.vfp_low_power = 1321,//60hz
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver   =  DSC_VER,
		.slice_mode =  DSC_SLICE_MODE,
		.rgb_swap   =  DSC_RGB_SWAP,
		.dsc_cfg    =  DSC_DSC_CFG,
		.rct_on     =  DSC_RCT_ON,
		.bit_per_channel    =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable =  DSC_BP_ENABLE,
		.bit_per_pixel =  DSC_BIT_PER_PIXEL,
		.pic_height =  FRAME_HEIGHT,
		.pic_width  =  FRAME_WIDTH,
		.slice_height  =  DSC_SLICE_HEIGHT,
		.slice_width   =  DSC_SLICE_WIDTH,
		.chunk_size =  DSC_CHUNK_SIZE,
		.xmit_delay =  DSC_XMIT_DELAY,
		.dec_delay  =  DSC_DEC_DELAY,
		.scale_value   =  DSC_SCALE_VALUE,
		.increment_interval =  DSC_INCREMENT_INTERVAL,
		.decrement_interval =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset   =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset  =  DSC_SLICE_BPG_OFFSET,
		.initial_offset =  DSC_INITIAL_OFFSET,
		.final_offset   =  DSC_FINAL_OFFSET,
		.flatness_minqp =  DSC_FLATNESS_MINQP,
		.flatness_maxqp =  DSC_FLATNESS_MAXQP,
		.rc_model_size  =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi  =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo  =  DSC_RC_TGT_OFFSET_LO,
	},
	.data_rate = DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	/* dsi_hbp is hbp_wc, cal hbp according to hbp_wc, ref:4997538 */
	.dyn = {
		.switch_en = 0,
		.data_rate = 544*2,
		.hfp = MODE_0_HFP,
		.vfp_lp_dyn = MODE_0_VFP,
	},
};

static struct mtk_panel_params ext_params_mode_1 = {
	.vfp_low_power = 1321,//60hz
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver   =  DSC_VER,
		.slice_mode =  DSC_SLICE_MODE,
		.rgb_swap   =  DSC_RGB_SWAP,
		.dsc_cfg    =  DSC_DSC_CFG,
		.rct_on     =  DSC_RCT_ON,
		.bit_per_channel    =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable =  DSC_BP_ENABLE,
		.bit_per_pixel =  DSC_BIT_PER_PIXEL,
		.pic_height =  FRAME_HEIGHT,
		.pic_width  =  FRAME_WIDTH,
		.slice_height  =  DSC_SLICE_HEIGHT,
		.slice_width   =  DSC_SLICE_WIDTH,
		.chunk_size =  DSC_CHUNK_SIZE,
		.xmit_delay =  DSC_XMIT_DELAY,
		.dec_delay  =  DSC_DEC_DELAY,
		.scale_value   =  DSC_SCALE_VALUE,
		.increment_interval =  DSC_INCREMENT_INTERVAL,
		.decrement_interval =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset   =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset  =  DSC_SLICE_BPG_OFFSET,
		.initial_offset =  DSC_INITIAL_OFFSET,
		.final_offset   =  DSC_FINAL_OFFSET,
		.flatness_minqp =  DSC_FLATNESS_MINQP,
		.flatness_maxqp =  DSC_FLATNESS_MAXQP,
		.rc_model_size  =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi  =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo  =  DSC_RC_TGT_OFFSET_LO,
	},
	.data_rate = DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.dyn = {
		.switch_en = 0,
		.data_rate = 544*2,
		.hfp = MODE_1_HFP,
		.vfp_lp_dyn = MODE_1_VFP,
	},
};

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}
static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_info("%s: invalid display_mode\n", __func__);
		return -1;
	}

	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_mode_1;
	else
		ret = 1;
	if (!ret) {
		current_fps = drm_mode_vrefresh(m);
		pr_info("%s, current_fps:%d\n", __func__, current_fps);
	}
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
#ifdef BDG_PORTING_DBG
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x6e, 0x48, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		dev_info("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);
#endif
	return 1;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay,
			performance_mode_1.vdisplay,
			drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	connector->display_info.width_mm = (unsigned int)PHYSICAL_WIDTH/1000;
	connector->display_info.height_mm = (unsigned int)PHYSICAL_HEIGHT/1000;

	return 2;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_bdg_support(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "bdg-support", &bdg_support);
	if (!ret && bdg_support == 1) {
		pr_info("%s, bdg support 1", __func__);
	} else {
		pr_info("%s, bdg support 0", __func__);
		bdg_support = 0;
	}
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+ HJ_DEBUG 003\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	if (ctx->gate_ic == 0) {
		ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_info(dev, "%s: cannot get bias-pos 0 %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		devm_gpiod_put(dev, ctx->bias_pos);

		ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_info(dev, "%s: cannot get bias-neg 1 %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		devm_gpiod_put(dev, ctx->bias_neg);
	}

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
	check_is_bdg_support(dev);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	pr_info("%s-\n", __func__);

	return ret;
}
static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "nt36672c,vdo,dsc,boe", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-nt36672c-fhdp-dsi-vdo-dsc-txd-boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("nt36672c fhdp VDO DSC LCD Panel Driver");
MODULE_LICENSE("GPL v2");
