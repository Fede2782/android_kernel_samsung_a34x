// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include "nt50358a_i2c.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#define NT50358A_REG_WRITE(add, data) nt50358a_write_byte(add, data)
#define NT50358A_REG_READ(add) nt50358a_read_byte(add)

struct boe_panel {
	struct device *dev;
	struct mipi_dsi_device *dsi;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *avee;
	struct gpio_desc *avdd;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *leden_gpio;
	bool enabled;
	bool prepared;
	int error;
};

#define boe_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		boe_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})
#define boe_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		boe_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct boe_panel *to_boe_panel(struct drm_panel *panel)
{
	return container_of(panel, struct boe_panel, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int boe_dcs_read(struct boe_panel *ctx, u8 cmd, void *data, size_t len)
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

static void boe_panel_get_data(struct boe_panel *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = boe_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void boe_dcs_write(struct boe_panel *ctx, const void *data, size_t len)
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

static void boe_panel_init(struct boe_panel *ctx)
{
	pr_info("%s +\n", __func__);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(52000, 55000);

	boe_dcs_write_seq_static(ctx, 0x11);
	msleep(20);
	boe_dcs_write_seq_static(ctx, 0x50, 0x5a, 0x0c);
	boe_dcs_write_seq_static(ctx, 0x80, 0xfd);
	msleep(120);
	boe_dcs_write_seq_static(ctx, 0x29);
	msleep(20);
	boe_dcs_write_seq_static(ctx, 0x50);

	pr_info("%s -\n", __func__);
}


static int boe_panel_unprepare(struct drm_panel *panel)
{
	struct boe_panel *ctx = to_boe_panel(panel);

	if (!ctx->prepared)
		return 0;

	boe_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(20);
	boe_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1000, 1100);
	/* Enable AVDD & AVEE discharge when power off */
	NT50358A_REG_WRITE(0x03, 0x33);
	if (ctx->avee)
		gpiod_set_value(ctx->avee, 0);
	usleep_range(1000, 1100);

	if (ctx->avdd)
		gpiod_set_value(ctx->avdd, 0);
	usleep_range(5000, 7000);

	ctx->prepared = false;

	return 0;
}

static int boe_panel_prepare(struct drm_panel *panel)
{
	struct boe_panel *ctx = to_boe_panel(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	if (ctx->avdd)
		gpiod_set_value(ctx->avdd, 1);
	usleep_range(1000, 2000);

	if (ctx->avee)
		gpiod_set_value(ctx->avee, 1);
	usleep_range(5000, 6000);

	/* Set AVDD = 5.8V */
	NT50358A_REG_WRITE(0x00, 0x12);
	/* Set AVEE = -5.8V */
	NT50358A_REG_WRITE(0x01, 0x12);
	/* Disable AVDD & AVEE discharge when power on*/
	NT50358A_REG_WRITE(0x03, 0x30);

	boe_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0) {
		pr_info("Send initial code error!\n");
		boe_panel_unprepare(panel);
	}

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

	return 0;
}

static int boe_panel_enable(struct drm_panel *panel)
{
	struct boe_panel *ctx = to_boe_panel(panel);

	if (ctx->enabled)
		return 0;

	msleep(130);

	if (ctx->backlight) {
		ctx->backlight->props.state &= ~BL_CORE_FBBLANK;
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	if (ctx->leden_gpio)
		gpiod_set_value(ctx->leden_gpio, 1);
	ctx->enabled = true;

	return 0;
}

static int boe_panel_disable(struct drm_panel *panel)
{
	struct boe_panel *ctx = to_boe_panel(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		ctx->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(ctx->backlight);
	}

	if (ctx->leden_gpio)
		gpiod_set_value(ctx->leden_gpio, 0);

	ctx->enabled = false;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 94210,
	.hdisplay = 800,
	.hsync_start = 800 + 170,
	.hsync_end = 800 + 170 + 14,
	.htotal = 800 + 170 + 14 + 60,
	.vdisplay = 1280,
	.vsync_start = 1280 + 184,
	.vsync_end = 1280 + 184 + 8,
	.vtotal = 1280 + 184 + 8 + 32,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct boe_panel *ctx = to_boe_panel(panel);

	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 1;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 296,
	.physical_width_um = 107640,
	.physical_height_um = 172224,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

static int boe_panel_get_modes(struct drm_panel *panel,
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
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 107640;
	connector->display_info.height_mm = 172224;

	return 1;
}

static const struct drm_panel_funcs boe_panel_funcs = {
	.unprepare = boe_panel_unprepare,
	.prepare = boe_panel_prepare,
	.disable = boe_panel_disable,
	.enable = boe_panel_enable,
	.get_modes = boe_panel_get_modes,
};

static int boe_panel_add(struct boe_panel *ctx)
{
	struct device *dev = &ctx->dsi->dev;
	struct device_node *backlight;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->leden_gpio = devm_gpiod_get(dev, "leden", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->leden_gpio)) {
		dev_err(dev, "cannot get lcden-gpios %ld\n",
			PTR_ERR(ctx->leden_gpio));
		ctx->leden_gpio = NULL;
	}

	ctx->avdd = devm_gpiod_get(dev, "avdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd)) {
		dev_err(dev, "cannot get avdd-gpios %ld\n",
			PTR_ERR(ctx->avdd));
		ctx->avdd = NULL;
	}

	ctx->avee = devm_gpiod_get(dev, "avee", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee)) {
		dev_err(dev, "cannot get avee-gpios %ld\n",
			PTR_ERR(ctx->avee));
		ctx->avee = NULL;
	}

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &boe_panel_funcs, DRM_MODE_CONNECTOR_DSI);

	ctx->panel.dev = dev;
	ctx->panel.funcs = &boe_panel_funcs;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	drm_panel_add(&ctx->panel);

	return 0;
}

static int boe_panel_probe(struct mipi_dsi_device *dsi)
{
	struct boe_panel *ctx;
	struct device *dev = &dsi->dev;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(struct boe_panel), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	ctx->dsi = dsi;
	ret = boe_panel_add(ctx);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		drm_panel_remove(&ctx->panel);
		return -EPROBE_DEFER;
	}
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	return ret;
}

static void boe_panel_remove(struct mipi_dsi_device *dsi)
{
	struct boe_panel *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx != NULL) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif
}

static const struct of_device_id boe_of_match[] = {
	{
		.compatible = "boe,tv080wxmlls",
	},
	{}
};
MODULE_DEVICE_TABLE(of, boe_of_match);

static struct mipi_dsi_driver boe_panel_driver = {
	.driver = {
		.name = "panel-boe-tv080wxmlls",
		.owner = THIS_MODULE,
		.of_match_table = boe_of_match,
	},
	.probe = boe_panel_probe,
	.remove = boe_panel_remove,
};
module_mipi_dsi_driver(boe_panel_driver);

MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("BOE tv080wxm-lls 800x1280 video mode panel driver");
MODULE_LICENSE("GPL");
