// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include "ocp2138_i2c.h"

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#define OCP2183_REG_WRITE(add, data) ocp2138_write_byte(add, data)
#define OCP2183_REG_READ(add) ocp2138_read_byte(add)

struct himax_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct device *dev;
	struct backlight_device *backlight;
	struct gpio_desc *avee;
	struct gpio_desc *avdd;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *leden_gpio;
	bool prepared_power;
	bool prepared;
	bool enabled;
	int error;
};

#define himax_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		himax_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})
#define himax_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		himax_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct himax_panel *panel_to_himax(struct drm_panel *panel)
{
	return container_of(panel, struct himax_panel, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int himax_dcs_read(struct himax_panel *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void himax_panel_get_data(struct himax_panel *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = himax_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void himax_dcs_write(struct himax_panel *ctx, const void *data, size_t len)
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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}


static void himax_panel_init(struct himax_panel *ctx)
{
	pr_info("%s+++\n", __func__);

	himax_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x10, 0x2E);
	himax_dcs_write_seq_static(ctx, 0xE9, 0xCD);
	himax_dcs_write_seq_static(ctx, 0xBB, 0x01);
	usleep_range(5000, 7000);
	himax_dcs_write_seq_static(ctx, 0xE9, 0x00);
	himax_dcs_write_seq_static(ctx, 0xD1, 0x67, 0x0C, 0xFF, 0x05);
	himax_dcs_write_seq_static(ctx, 0xB1, 0x10, 0xFA, 0xAF, 0xAF, 0x2B,
			0x2B, 0xC1, 0x75, 0x39, 0x36, 0x36, 0x36, 0x36, 0x22,
			0x21, 0x15, 0x00);
	himax_dcs_write_seq_static(ctx, 0xB2, 0x00, 0xB0, 0x47, 0xD0, 0x00,
			0x2C, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x15, 0x20,
			0xD7, 0x00);
	himax_dcs_write_seq_static(ctx, 0xB4, 0x38, 0x47, 0x38, 0x47, 0x66,
			0x4E, 0x00, 0x00, 0x01, 0x72, 0x01, 0x58, 0x00, 0xFF,
			0x00, 0xFF);
	himax_dcs_write_seq_static(ctx, 0xBF, 0xFC, 0x85, 0x80);
	himax_dcs_write_seq_static(ctx, 0xD2, 0x2B, 0x2B);
	himax_dcs_write_seq_static(ctx, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x78,
			0x04, 0x00, 0x14, 0x00, 0x27, 0x00, 0x44, 0x4F, 0x29,
			0x29, 0x00, 0x00, 0x32, 0x10, 0x25, 0x00, 0x25, 0x32,
			0x10, 0x1F, 0x00, 0x1F, 0x32, 0x18, 0x10, 0x08, 0x10,
			0x00, 0x00, 0x20, 0x30, 0x01, 0x55, 0x21, 0x2E, 0x01,
			0x55, 0x0F);
	usleep_range(5000, 7000);
	himax_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x04, 0x0B, 0x11, 0x17,
			0x26, 0x3D, 0x45, 0x4D, 0x4A, 0x65, 0x6D, 0x75, 0x87,
			0x86, 0x92, 0x9D, 0xB0, 0xAF, 0x56, 0x5E, 0x68, 0x70,
			0x00, 0x04, 0x0B, 0x11, 0x17, 0x26, 0x3D, 0x45, 0x4D,
			0x4A, 0x65, 0x6D, 0x75, 0x87, 0x86, 0x92, 0x9D, 0xB0,
			0xAF, 0x56, 0x5E, 0x68, 0x70);
	usleep_range(5000, 7000);
	himax_dcs_write_seq_static(ctx, 0xCB, 0x00, 0x13, 0x08, 0x02, 0x34);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x01);
	himax_dcs_write_seq_static(ctx, 0xB1, 0x01, 0x9B, 0x01, 0x31);
	himax_dcs_write_seq_static(ctx, 0xCB, 0xF4, 0x36, 0x12, 0x16, 0xC0,
			0x28, 0x6C, 0x85, 0x3F, 0x04);
	himax_dcs_write_seq_static(ctx, 0xD3, 0x01, 0x00, 0x3C, 0x00, 0x00,
			0x11, 0x10, 0x00, 0x0E, 0x00, 0x01);
	usleep_range(5000, 7000);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x02);
	himax_dcs_write_seq_static(ctx, 0xB4, 0x4E, 0x00, 0x33, 0x11, 0x33,
			0x88);
	himax_dcs_write_seq_static(ctx, 0xBF, 0xF2, 0x00, 0x02);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x00);
	himax_dcs_write_seq_static(ctx, 0xC0, 0x23, 0x23, 0x22, 0x11, 0xA2,
			0x17, 0x00, 0x80, 0x00, 0x00, 0x08, 0x00, 0x63, 0x63);
	himax_dcs_write_seq_static(ctx, 0xC6, 0xF9);
	himax_dcs_write_seq_static(ctx, 0xC7, 0x30);
	himax_dcs_write_seq_static(ctx, 0xC8, 0x00, 0x04, 0x04, 0x00, 0x00,
			0x85, 0x43, 0xFF);
	himax_dcs_write_seq_static(ctx, 0xD0, 0x07, 0x04, 0x05);
	himax_dcs_write_seq_static(ctx, 0xD5, 0x21, 0x20, 0x21, 0x20, 0x25,
			0x24, 0x25, 0x24, 0x18, 0x18, 0x18, 0x18, 0x1A, 0x1A,
			0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1B, 0x03, 0x02, 0x03,
			0x02, 0x01, 0x00, 0x01, 0x00, 0x07, 0x06, 0x07, 0x06,
			0x05, 0x04, 0x05,0x04, 0x18, 0x18, 0x18, 0x18, 0x18,
			0x18, 0x18, 0x18);
	usleep_range(5000, 7000);
	himax_dcs_write_seq_static(ctx, 0xE7, 0x12, 0x13, 0x02, 0x02, 0x57,
			0x57, 0x0E, 0x0E, 0x1B, 0x28, 0x29, 0x74, 0x28, 0x74,
			0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x68);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x01);
	himax_dcs_write_seq_static(ctx, 0xE7, 0x02, 0x38, 0x01, 0x93, 0x0D,
			0xD9, 0x0E);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x02);
	himax_dcs_write_seq_static(ctx, 0xE7, 0xFF, 0x01, 0xFF, 0x01, 0x00,
			0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x81, 0x00, 0x02, 0x40);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x00);
	himax_dcs_write_seq_static(ctx, 0xBA, 0x70, 0x03, 0xA8, 0x83, 0xF2,
			0x00, 0xC0, 0x0D);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x02);
	himax_dcs_write_seq_static(ctx, 0xD8, 0xAF, 0xFF, 0xFF, 0xFF, 0xF0,
			0x00, 0xAF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x03);
	himax_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0,
			0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0x00, 0x55, 0x55,
			0x55, 0x55, 0x50, 0x00, 0x55, 0x55, 0x55, 0x55, 0x50,
			0x00);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x00);
	himax_dcs_write_seq_static(ctx, 0xE1, 0x01, 0x04);
	himax_dcs_write_seq_static(ctx, 0xCC, 0x02);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x03);
	himax_dcs_write_seq_static(ctx, 0xB2, 0x80);
	himax_dcs_write_seq_static(ctx, 0xBD, 0x00);
	himax_dcs_write_seq_static(ctx, 0x35, 0x00);
	himax_dcs_write_seq_static(ctx, 0x11);
	usleep_range(120000, 121000);
	himax_dcs_write_seq_static(ctx, 0xB2, 0x00, 0xB0, 0x47, 0xD0, 0x00,
			0x2C, 0x50, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x15, 0x20,
			0xD7, 0x00);
	himax_dcs_write_seq_static(ctx, 0x29);
	usleep_range(20000, 21000);

	pr_info("%s ---\n", __func__);
}

static int himax_panel_prepare_power(struct drm_panel *panel)
{
	struct himax_panel *himax = panel_to_himax(panel);

	pr_info("%s+++\n", __func__);

	if (himax->prepared_power)
		return 0;

	gpiod_set_value(himax->reset_gpio, 0);
	usleep_range(1000, 1500);

	/* Set AVDD = 5.7V */
	OCP2183_REG_WRITE(0x00, 0x11);
	/* Set AVEE = -5.7V */
	OCP2183_REG_WRITE(0x01, 0x11);
	gpiod_set_value(himax->avdd, 1);
	usleep_range(6000, 7000);
	gpiod_set_value(himax->avee, 1);
	usleep_range(20000, 21000);

	gpiod_set_value(himax->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value(himax->reset_gpio, 0);
	usleep_range(2000, 3000);
	gpiod_set_value(himax->reset_gpio, 1);
	usleep_range(50000, 51000);

	himax->prepared_power = true;

	pr_info("%s---\n", __func__);

	return 0;
}

static int himax_panel_unprepare_power(struct drm_panel *panel)
{
	struct himax_panel *himax = panel_to_himax(panel);

	pr_info("%s+++\n", __func__);

	if (!himax->prepared_power)
		return 0;

	gpiod_set_value(himax->reset_gpio, 0);
	usleep_range(500, 1000);

	/* Disable AVDD & AVEE when power off */
	gpiod_set_value(himax->avee, 0);
	gpiod_set_value(himax->avdd, 0);
	usleep_range(5000, 7000);

	himax->prepared_power = false;

	pr_info("%s---\n", __func__);

	return 0;
}

static int himax_panel_unprepare(struct drm_panel *panel)
{
	struct himax_panel *himax = panel_to_himax(panel);

	pr_info("%s+++\n", __func__);

	if (!himax->prepared)
		return 0;

	himax_dcs_write_seq_static(himax, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(20);
	himax_dcs_write_seq_static(himax, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	himax_panel_unprepare_power(panel);

	himax->prepared = false;

	pr_info("%s---\n", __func__);

	return 0;
}


static int himax_panel_prepare(struct drm_panel *panel)
{
	struct himax_panel *himax = panel_to_himax(panel);

	pr_info("%s+++\n", __func__);

	if (himax->prepared)
		return 0;

	himax_panel_prepare_power(panel);

	himax_panel_init(himax);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

	himax->prepared = true;

	pr_info("%s---\n", __func__);

	return 0;
}

static int himax_panel_enable(struct drm_panel *panel)
{
	struct himax_panel *himax = panel_to_himax(panel);

	pr_info("%s+++\n", __func__);

	if (himax->enabled)
		return 0;

	if (himax->backlight) {
		himax->backlight->props.state &= ~BL_CORE_FBBLANK;
		himax->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(himax->backlight);
	}

	gpiod_set_value(himax->leden_gpio, 1);
	himax->enabled = true;

	pr_info("%s---\n", __func__);

	return 0;
}

static int himax_panel_disable(struct drm_panel *panel)
{
	struct himax_panel *himax = panel_to_himax(panel);

	pr_info("%s+++\n", __func__);

	if (!himax->enabled)
		return 0;

	if (himax->backlight) {
		himax->backlight->props.power = FB_BLANK_POWERDOWN;
		himax->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(himax->backlight);
	}

	gpiod_set_value(himax->leden_gpio, 0);

	himax->enabled = false;

	pr_info("%s---\n", __func__);

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 163022,
	.hdisplay = 1200,
	.hsync_start = 1200 + 42,
	.hsync_end = 1200 + 42 + 8,
	.htotal = 1200 + 42 + 8 + 28,
	.vdisplay = 2000,
	.vsync_start = 2000 + 80,
	.vsync_end = 2000 + 80 + 8,
	.vtotal = 2000 + 80 + 8 + 38,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct himax_panel *himax = panel_to_himax(panel);

	gpiod_set_value(himax->reset_gpio, on);
	devm_gpiod_put(himax->dev, himax->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 1;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 520,
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

static int himax_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		pr_info("failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 107;
	connector->display_info.height_mm = 172;

	return 1;
}

static const struct drm_panel_funcs himax_panel_funcs = {
	.unprepare = himax_panel_unprepare,
	.prepare = himax_panel_prepare,
	.disable = himax_panel_disable,
	.enable = himax_panel_enable,
	.get_modes = himax_panel_get_modes,
};

static int himax_panel_probe(struct mipi_dsi_device *dsi)
{
	struct himax_panel *himax;
	struct device *dev = &dsi->dev;
	int ret;
	struct device_node *backlight;

	pr_info("%s+++\n", __func__);

	himax = devm_kzalloc(&dsi->dev, sizeof(*himax), GFP_KERNEL);
	if (!himax)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, himax);
	himax->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		himax->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!himax->backlight)
			return -EPROBE_DEFER;
	}

	himax->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(himax->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(himax->reset_gpio));
		return PTR_ERR(himax->reset_gpio);
	}

	himax->leden_gpio = devm_gpiod_get(dev, "leden", GPIOD_OUT_HIGH);
	if (IS_ERR(himax->leden_gpio)) {
		dev_err(dev, "cannot get leden-gpios %ld\n",
			PTR_ERR(himax->leden_gpio));
		return PTR_ERR(himax->leden_gpio);
	}

	himax->avdd = devm_gpiod_get(dev, "avdd", GPIOD_OUT_HIGH);
	if (IS_ERR(himax->avdd)) {
		dev_err(dev, "cannot get avdd-gpios %ld\n",
			PTR_ERR(himax->avdd));
		return PTR_ERR(himax->avdd);
	}

	himax->avee = devm_gpiod_get(dev, "avee", GPIOD_OUT_HIGH);
	if (IS_ERR(himax->avee)) {
		dev_err(dev, "cannot get avee-gpios %ld\n",
			PTR_ERR(himax->avee));
		return PTR_ERR(himax->avee);
	}

	himax->prepared = true;
	himax->enabled = true;
	himax->prepared_power = true;
	drm_panel_init(&himax->panel, dev, &himax_panel_funcs, DRM_MODE_CONNECTOR_DSI);

	himax->panel.funcs = &himax_panel_funcs;
	himax->panel.dev = dev;

	drm_panel_add(&himax->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&himax->panel);
		dev_err(dev, "mipi_dsi_attach fail, ret=%d\n", ret);
		return -EPROBE_DEFER;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&himax->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &himax->panel);
	if (ret < 0)
		return ret;
#endif
	pr_notice("%s---\n", __func__);

	return ret;
}

static void himax_panel_remove(struct mipi_dsi_device *dsi)
{
	struct himax_panel *himax = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&himax->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&himax->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx != NULL) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif
}

static const struct of_device_id himax_of_match[] = {
	{
		.compatible = "hx,hx83102p",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, himax_of_match);

static struct mipi_dsi_driver himax_panel_driver = {
	.driver = {
		.name = "panel-himax-hx83102p-vdo",
		.of_match_table = himax_of_match,
	},
	.probe = himax_panel_probe,
	.remove = himax_panel_remove,
};
module_mipi_dsi_driver(himax_panel_driver);

MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("Himax hx83102p 1200x2000 video mode panel driver");
MODULE_LICENSE("GPL");
