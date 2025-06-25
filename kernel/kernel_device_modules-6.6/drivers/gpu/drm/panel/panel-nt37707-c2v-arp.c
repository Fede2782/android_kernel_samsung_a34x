// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
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

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "panel-nt37707-c2v-arp.h"


#define lcm_err(fmt, ...) \
	pr_info("lcm_err: %s(%d): " fmt,  __func__, __LINE__, ##__VA_ARGS__)

#define lcm_info(fmt, ...) \
	pr_info("lcm_info: %s(%d): " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF
#define MDSS_MAX_PANEL_LEN	256

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddr_gpio;
	struct gpio_desc *ason_0p8_gpio;
	struct gpio_desc *ason_1p8_gpio;

	struct regulator *oled_vddi;
	struct regulator *oled_vci;

	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

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
		pr_info("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

inline void push_table(struct lcm *ctx, struct LCD_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	char delay_ms;

	for (i = 0; i < count; i++) {
		if (!table[i].count) {
			delay_ms = table[i].para_list[0];
			lcm_info("delay %dms(max 255ms once)\n", delay_ms);
			usleep_range(delay_ms * 1000, delay_ms * 1000 + 100);
		} else {
			lcm_dcs_write(ctx, &table[i].para_list,
					table[i].count);
		}
	}
}

inline void push_table_cmdq(struct mtk_dsi *dsi, dcs_write_gce cb, void *handle, struct LCD_setting_table *table,
		unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		cb(dsi, handle, &table[i].para_list, table[i].count);

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
		pr_info("error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif



static int lcm_panel_power_regulator_init(struct device *dev, struct lcm *ctx)
{
	int ret = 0;


	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return ret;
}


static int lcm_panel_power_enable(struct lcm *ctx)
{
	int ret = 0;

	pr_info("%s+\n", __func__);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12000, 12100);

	return ret;
}

static int lcm_panel_power_disable(struct lcm *ctx)
{
	int ret = 0;

	pr_info("%s+\n", __func__);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(12000, 12100);

	return ret;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s+\n", __func__);
	push_table(ctx, init_setting_fhd_120hz, sizeof(init_setting_fhd_120hz) / sizeof(struct LCD_setting_table), 1);
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

	push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCD_setting_table), 1);

	ctx->error = 0;
	ctx->prepared = false;
	lcm_panel_power_disable(ctx);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;


	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_power_enable(ctx);
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
	.clock = 519912,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + HFP,//HFP
	.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,//HSA
	.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + VFP,//VFP
	.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,//VSA
	.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static void mipi_dcs_write_backlight_command(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int level)
{
	cmd_bl_level[0].para_list[1] = (unsigned char)((level>>8) & 0xF);
	cmd_bl_level[0].para_list[2] = (unsigned char)(level & 0xFF);
	pr_info("dsi set backlight level %d\n", level);
	push_table_cmdq(dsi, cb, handle, cmd_bl_level, sizeof(cmd_bl_level) / sizeof(struct LCD_setting_table), 1);
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	if (!cb)
		return -1;

	mipi_dcs_write_backlight_command(dsi, cb, handle, level);

	return 0;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = MIPI_DATA_RATE_120HZ / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  FHD_DSC_ENABLE,
		.ver                   =  FHD_DSC_VER,
		.slice_mode            =  FHD_DSC_SLICE_MODE,
		.rgb_swap              =  FHD_DSC_RGB_SWAP,
		.dsc_cfg               =  FHD_DSC_DSC_CFG,
		.rct_on                =  FHD_DSC_RCT_ON,
		.bit_per_channel       =  FHD_DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  FHD_DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  FHD_DSC_BP_ENABLE,
		.bit_per_pixel         =  FHD_DSC_BIT_PER_PIXEL,
		.pic_height            =  FHD_FRAME_HEIGHT,
		.pic_width             =  FHD_FRAME_WIDTH,
		.slice_height          =  FHD_DSC_SLICE_HEIGHT,
		.slice_width           =  FHD_DSC_SLICE_WIDTH,
		.chunk_size            =  FHD_DSC_CHUNK_SIZE,
		.xmit_delay            =  FHD_DSC_XMIT_DELAY,
		.dec_delay             =  FHD_DSC_DEC_DELAY,
		.scale_value           =  FHD_DSC_SCALE_VALUE,
		.increment_interval    =  FHD_DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  FHD_DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  FHD_DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  FHD_DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  FHD_DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  FHD_DSC_INITIAL_OFFSET,
		.final_offset          =  FHD_DSC_FINAL_OFFSET,
		.flatness_minqp        =  FHD_DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  FHD_DSC_FLATNESS_MAXQP,
		.rc_model_size         =  FHD_DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  FHD_DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  FHD_DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  FHD_DSC_RC_TGT_OFFSET_LO,
	},
	.dyn_fps = {
			.switch_en = 0, .vact_timing_fps = 120,
		},
	.data_rate = MIPI_DATA_RATE_120HZ,
	.real_te_duration = 2778,
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	 /**vdo ltpo**/
	.ltpo_vm_enable = 1,
	.ltpo_vm_minimum_fps = 1,
};


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
};
#endif

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;

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

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_info("%s+\n", __func__);
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	lcm_panel_power_regulator_init(dev, ctx);

#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

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

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "boe,nt37707,c2v,arp", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-nt37707-c2v-arp",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Haizhen Wang <haizhen.wang@mediatek.com>");
MODULE_DESCRIPTION("BOE NT37707 CMD Panel Driver");
MODULE_LICENSE("GPL");
