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
#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

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

#include "panel-rm692h5-cmd.h"

#define lcm_err(fmt, ...) \
	pr_info("lcm_err: %s(%d): " fmt,  __func__, __LINE__, ##__VA_ARGS__)

#define lcm_info(fmt, ...) \
	pr_info("lcm_info: %s(%d): " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static struct mtk_panel_params ext_params[MODE_NUM];

#ifdef CONFIG_MTK_RES_SWITCH_ON_AP
static enum RES_SWITCH_TYPE res_switch_type = RES_SWITCH_NO_USE;
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	//struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddr_gpio;
	//struct gpio_desc *ason_0p8_gpio;
	//struct gpio_desc *ason_1p8_gpio;
	//struct gpio_desc *ason_1p2_gpio;
	//struct gpio_desc *ccm_1p2_gpio;

	//struct regulator *oled_vddi;
	//struct regulator *oled_vci;

	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	struct drm_display_mode *m;
	int error;
	bool readback_flag;

	u32 dvv; // only support dv2 & dv3
};

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
		lcm_err("error %d reading dcs seq:(%#x)\n", ret, cmd);
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
		lcm_info("return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static int get_mode_enum(struct drm_display_mode *m)
{
	int ret = 0, m_vrefresh = 0;

	if (m == NULL) {
		lcm_info("%s display mode is null\n", __func__);
		return -1;
	}

	m_vrefresh = drm_mode_vrefresh(m);
	if (m_vrefresh == 120)
		ret = FHD_120_360TE;
	else if (m_vrefresh == 60)
		ret = FHD_60_360TE;
	else if (m_vrefresh == 90)
		ret = FHD_90_360TE;
	else if (m_vrefresh == 72)
		ret = FHD_72_360TE;
	else
		lcm_info("Invalid fps\n");
	return ret;
}

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

	ctx->vddr_gpio = devm_gpiod_get_index(ctx->dev, "vddr", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddr_gpio)) {
		pr_info("%s: cannot get vddr_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddr_gpio));
		return PTR_ERR(ctx->vddr_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->vddr_gpio);

	return ret;
}

static int lcm_panel_power_enable(struct lcm *ctx)
{
	int ret = 0;

	pr_info("%s+\n", __func__);
	//lcm_set_regulator(ctx->oled_vddi, 1);
	usleep_range(2000, 2100);
	//lcm_set_regulator(ctx->oled_vci, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->vddr_gpio, 1);
	usleep_range(2000, 2100);
	//nvtchip_power_on(ctx);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(40000, 40100);

	return ret;
}

static int lcm_panel_power_disable(struct lcm *ctx)
{
	int ret = 0;

	pr_info("%s+\n", __func__);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(12000, 12100);
	gpiod_set_value(ctx->vddr_gpio, 0);
	usleep_range(2000, 2100);
	//lcm_set_regulator(ctx->oled_vddi, 0);
	usleep_range(5000, 5100);
	//lcm_set_regulator(ctx->oled_vci, 0);
	usleep_range(12000, 12100);
	pr_info("%s-\n", __func__);

	return ret;
}

static void lcm_panel_init(struct lcm *ctx)
{
	int mode_id = -1;
	struct drm_display_mode *m = ctx->m;

	mode_id = get_mode_enum(m);
	lcm_info("mode_id = %d, dvt_version:%d\n", mode_id, ctx->dvv);

	if (ctx->dvv == DV1)
		push_table(ctx, init_setting_fhd_120hz_dv1, ARRAY_SIZE(init_setting_fhd_120hz_dv1), 0);
	else if (ctx->dvv == DV2)
		push_table(ctx, init_setting_fhd_120hz_dv2, ARRAY_SIZE(init_setting_fhd_120hz_dv2), 0);
	else
		push_table(ctx, init_setting_fhd_120hz_dv3, ARRAY_SIZE(init_setting_fhd_120hz_dv3), 0);

	switch (mode_id) {
	case FHD_60:
		push_table(ctx, cmd_set_fps_60hz, ARRAY_SIZE(cmd_set_fps_60hz), 0);
		break;
	case FHD_90:
		push_table(ctx, cmd_set_fps_90hz, ARRAY_SIZE(cmd_set_fps_90hz), 0);
		break;
	case FHD_120:
		push_table(ctx, cmd_set_fps_120hz, ARRAY_SIZE(cmd_set_fps_120hz), 0);
		break;
	default:
		push_table(ctx, cmd_set_fps_120hz_360te, ARRAY_SIZE(cmd_set_fps_120hz_360te), 0);
		break;
	}
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

	lcm_info("+\n");

	if (!ctx->prepared)
		return 0;

	push_table(ctx, lcm_suspend_setting, ARRAY_SIZE(lcm_suspend_setting), 0);

	ctx->error = 0;
	ctx->prepared = false;
	lcm_panel_power_disable(ctx);
	lcm_info("-\n");

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	lcm_info("+\n");
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

	lcm_info("-\n");
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

static const struct drm_display_mode display_mode[MODE_NUM] = {
	//fhd_120hz_360hz
	[FHD_120_360TE] = {
		.clock = 439782,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//fhd_90hz
	[FHD_90_360TE] = {
		.clock = 329837,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//fhd_72hz
	[FHD_72_360TE] = {
		.clock = 263870,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//fhd_60hz
	[FHD_60_360TE] = {
		.clock = 219891,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//fhd_120hz
	[FHD_120] = {
		.clock = 437960,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//fhd_90hz
	[FHD_90] = {
		.clock = 328470,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//fhd_60hz
	[FHD_60] = {
		.clock = 218980,
		.hdisplay = FHD_FRAME_WIDTH,
		.hsync_start = FHD_FRAME_WIDTH + HFP,
		.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
		.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = FHD_FRAME_HEIGHT,
		.vsync_start = FHD_FRAME_HEIGHT + VFP,
		.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},

#ifdef CONFIG_MTK_RES_SWITCH_ON_AP
	//fhd_120hz_360hz
	[VFHD_120_360TE] = {
		.clock = 325134,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP, /* 1115 */
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP, /* 2430 */
	},
	//vfhd_90hz
	[VFHD_90_360TE] = {
		.clock = 243851,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//vfhd_72hz
	[VFHD_72_360TE] = {
		.clock = 195081,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//vfhd_60hz
	[VFHD_60_360TE] = {
		.clock = 162567,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//vfhd_120hz
	[VFHD_120] = {
		.clock = 324135,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP, /* 1115 */
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP, /* 2430 */
	},
	//vfhd_90hz
	[VFHD_90] = {
		.clock = 242852,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
	//vfhd_60hz
	[VFHD_60] = {
		.clock = 161568,
		.hdisplay = VFHD_FRAME_WIDTH,
		.hsync_start = VFHD_FRAME_WIDTH + HFP,
		.hsync_end = VFHD_FRAME_WIDTH + HFP + HSA,
		.htotal = VFHD_FRAME_WIDTH + HFP + HSA + HBP,
		.vdisplay = VFHD_FRAME_HEIGHT,
		.vsync_start = VFHD_FRAME_HEIGHT + VFP,
		.vsync_end = VFHD_FRAME_HEIGHT + VFP + VSA,
		.vtotal = VFHD_FRAME_HEIGHT + VFP + VSA + VBP,
	},
#endif
};

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
	int mode_id = -1;
	struct drm_display_mode *m;

	if (!connector || !panel) {
		pr_info("%s, invalid param\n", __func__);
		return -1;
	}
	m = get_mode_by_id(connector, mode);

	if (m)
		mode_id = get_mode_enum(m);
	if (mode_id >= 0 && mode_id < MODE_NUM)
		ext->params = &ext_params[mode_id];

	pr_info("%s mode_id=%d\n", __func__, mode_id);

	return 0;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
		struct drm_connector *connector,
		struct mtk_panel_params **ext_param,
		unsigned int mode)
{
	int mode_id = -1;
	struct drm_display_mode *m;

	if (!connector || !panel) {
		pr_info("%s, invalid param\n", __func__);
		return -1;
	}
	m = get_mode_by_id(connector, mode);

	if (m)
		mode_id = get_mode_enum(m);
	if (mode_id >= 0 && mode_id < MODE_NUM)
		*ext_param = &ext_params[mode_id];

	if (*ext_param == NULL)
		pr_info("%s mode_id=%d, ext_param is null\n", __func__, mode_id);

	return 0;
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	return 1;
}

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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	cmd_bl_level[0].para_list[1] = (unsigned char)((level>>8) & 0xF);
	cmd_bl_level[0].para_list[2] = (unsigned char)(level & 0xFF);
	pr_info("dsi set backlight level %d\n", level);

	push_table_cmdq(dsi, cb, handle, cmd_set_backlight_pre_set,
		sizeof(cmd_set_backlight_pre_set) / sizeof(struct LCD_setting_table), 1);
	push_table_cmdq(dsi, cb, handle, cmd_bl_level, sizeof(cmd_bl_level) / sizeof(struct LCD_setting_table), 1);

	init_setting_fhd_120hz_dv3[INIT_CODE_BACKLIGHT_INDEX_DV3].para_list[1] = cmd_bl_level[0].para_list[1];
	init_setting_fhd_120hz_dv3[INIT_CODE_BACKLIGHT_INDEX_DV3].para_list[2] = cmd_bl_level[0].para_list[2];

	return 0;
}

#ifdef IF_ZERO
static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
	if (!cb)
		return -1;

	if (en)
		push_table_cmdq(dsi, cb, handle, cmd_local_hbm_on_dv3,
			sizeof(cmd_local_hbm_on_dv3) / sizeof(struct LCD_setting_table), 1);
	else
		push_table_cmdq(dsi, cb, handle, cmd_local_hbm_off_dv3,
			sizeof(cmd_local_hbm_off_dv3) / sizeof(struct LCD_setting_table), 1);

	return 0;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	push_table_cmdq(dsi, cb, handle, cmd_enter_longh_30hz,
		sizeof(cmd_enter_longh_30hz) / sizeof(struct LCD_setting_table), 1);

	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	push_table_cmdq(dsi, cb, handle, cmd_exit_longh_30hz_360hz,
		sizeof(cmd_exit_longh_30hz_360hz) / sizeof(struct LCD_setting_table), 1);

	return 0;
}
#endif

#define TO_ROI_SETTING(addr, base, offset) \
		{\
			{0x05, {addr, (base >> 8) & 0xFF, base & 0xFF, ((base + offset - 1) >> 8) & 0xFF, \
				(base + offset - 1) & 0xFF}},\
		}

static int lcm_update_roi(struct drm_panel *panel,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	int ret = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	struct LCD_setting_table roi_x_setting[] = TO_ROI_SETTING(0x2A, x, w);
	struct LCD_setting_table roi_y_setting[] = TO_ROI_SETTING(0x2B, y, h);

	push_table(ctx, roi_x_setting, ARRAY_SIZE(roi_x_setting), 0);
	push_table(ctx, roi_y_setting, ARRAY_SIZE(roi_y_setting), 0);
	lcm_info("(x,y,w,h): (%d,%d,%d,%d)\n", x, y, w, h);

	return ret;
}

static int lcm_update_roi_cmdq(void *dsi, dcs_write_gce cb, void *handle,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	int ret = 0;
	int i = 0;
	struct LCD_setting_table roi_x_setting[] = TO_ROI_SETTING(0x2A, x, w);
	struct LCD_setting_table roi_y_setting[] = TO_ROI_SETTING(0x2B, y, h);

	if (!cb)
		return -1;

	for (i = 0; i < ARRAY_SIZE(roi_x_setting); i++)
		cb(dsi, handle, roi_x_setting[i].para_list, ARRAY_SIZE(roi_x_setting[i].para_list));

	for (i = 0; i < ARRAY_SIZE(roi_y_setting); i++)
		cb(dsi, handle, roi_y_setting[i].para_list, ARRAY_SIZE(roi_y_setting[i].para_list));

	lcm_info("(x,y,w,h): (%d,%d,%d,%d)\n", x, y, w, h);

	return ret;
}

static void rm692h5_lcm_valid_roi(struct mtk_panel_params *ext_param,
	unsigned int *x, unsigned int *y, unsigned int *w, unsigned int *h)
{
	if (!ext_param)
		return;

	unsigned int roi_y = *y, roi_h = *h;
	unsigned int slice_height = ext_param->dsc_params.slice_height;
	unsigned int lil_te1_line = 520;
	unsigned int lil_te2_line = 1560;
	unsigned int inteval = 399;
	int line_diff;

	//lcm_info("partial roi y:%d height:%d\n", roi_y, roi_h);

	if (ext_param
		&& ext_param->real_te_duration != 0
		&& ext_param->real_te_duration < 8333) {
		//lcm_info("lil_te1_line:%d lil_te2_line:%d real_te_duration:%d\n",
			//lil_te1_line, lil_te2_line, ext_param->real_te_duration);

		if (roi_y >= lil_te1_line && roi_y <= (lil_te1_line + inteval)) {
			line_diff = roi_y - lil_te1_line + slice_height * 2;
			roi_y -= line_diff;
			roi_h += line_diff;
		} else if (roi_y >= lil_te2_line && roi_y <= (lil_te2_line + inteval)) {
			line_diff = roi_y - lil_te2_line + slice_height * 2;
			roi_y -= line_diff;
			roi_h += line_diff;
		}
	}

	//lcm_info("validate partial roi y:%d height:%d\n", roi_y, roi_h);

	*y = roi_y;
	*h = roi_h;
}

static struct mtk_panel_params ext_params[MODE_NUM] = {
	//120hz 360TE
	[FHD_120_360TE] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 185,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.skip_vblank = 3,
		.msync2_enable = 1,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.msync_cmd_table = {
			.te_type = MULTI_TE,
			.is_gce_delay = 1,
			.te_step_time = 2777,
		},
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 2778,
	},
	//90hz
	[FHD_90_360TE] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 185,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.skip_vblank = 4,
		.msync2_enable = 1,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.msync_cmd_table = {
			.te_type = MULTI_TE,
			.is_gce_delay = 1,
			.te_step_time = 2777,
		},
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 2778,
	},
	//72hz
	[FHD_72_360TE] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 185,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.skip_vblank = 5,
		.msync2_enable = 1,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.msync_cmd_table = {
			.te_type = MULTI_TE,
			.is_gce_delay = 1,
			.te_step_time = 2777,
		},
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 2778,
	},
	//60hz
	[FHD_60_360TE] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 185,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.skip_vblank = 6,
		.msync2_enable = 1,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.msync_cmd_table = {
			.te_type = MULTI_TE,
			.is_gce_delay = 1,
			.te_step_time = 2777,
		},
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 2778,
	},
	//120hz
	[FHD_120] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 125,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 8333,
	},
	//90hz
	[FHD_90] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 90,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 125,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 11111,
	},
	//60hz
	[FHD_60] = {
		.pll_clk = MIPI_DATA_RATE_120HZ / 2,
		.cust_esd_check = CUST_ESD_CHECK,
		.esd_check_enable = ESD_CHECK_ENABLE,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0B,
			.count = 1,
			.para_list[0] = 0x00,
		},
		.lcm_esd_check_table[1] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9c,
		},
		.lcm_esd_check_table[2] = {
			.cmd = 0xFA,
			.count = 1,
			.para_list[0] = 0x01,
		},
		.is_support_dbi = true,
		.physical_width_um = PHYSICAL_WIDTH,
		.physical_height_um = PHYSICAL_HEIGHT,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.dsc_params = {
			.enable				   =  FHD_DSC_ENABLE,
			.ver				   =  FHD_DSC_VER,
			.slice_mode			   =  FHD_DSC_SLICE_MODE,
			.rgb_swap			   =  FHD_DSC_RGB_SWAP,
			.dsc_cfg			   =  FHD_DSC_DSC_CFG,
			.rct_on				   =  FHD_DSC_RCT_ON,
			.bit_per_channel	   =  FHD_DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth	   =  FHD_DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable			   =  FHD_DSC_BP_ENABLE,
			.bit_per_pixel		   =  FHD_DSC_BIT_PER_PIXEL,
			.pic_height			   =  FHD_FRAME_HEIGHT,
			.pic_width			   =  FHD_FRAME_WIDTH,
			.slice_height		   =  FHD_DSC_SLICE_HEIGHT,
			.slice_width		   =  FHD_DSC_SLICE_WIDTH,
			.chunk_size			   =  FHD_DSC_CHUNK_SIZE,
			.xmit_delay			   =  FHD_DSC_XMIT_DELAY,
			.dec_delay			   =  FHD_DSC_DEC_DELAY,
			.scale_value		   =  FHD_DSC_SCALE_VALUE,
			.increment_interval	   =  FHD_DSC_INCREMENT_INTERVAL,
			.decrement_interval	   =  FHD_DSC_DECREMENT_INTERVAL,
			.line_bpg_offset	   =  FHD_DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset		   =  FHD_DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset	   =  FHD_DSC_SLICE_BPG_OFFSET,
			.initial_offset		   =  FHD_DSC_INITIAL_OFFSET,
			.final_offset		   =  FHD_DSC_FINAL_OFFSET,
			.flatness_minqp		   =  FHD_DSC_FLATNESS_MINQP,
			.flatness_maxqp		   =  FHD_DSC_FLATNESS_MAXQP,
			.rc_model_size		   =  FHD_DSC_RC_MODEL_SIZE,
			.rc_edge_factor		   =  FHD_DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi	   =  FHD_DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo	   =  FHD_DSC_RC_TGT_OFFSET_LO,
			.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = panel_visionox_rm692h5_spr_off_rc_buf_thresh,
				.range_min_qp = panel_visionox_rm692h5_spr_off_range_min_qp,
				.range_max_qp = panel_visionox_rm692h5_spr_off_range_max_qp,
				.range_bpg_ofs = panel_visionox_rm692h5_spr_off_range_bpg_ofs,
			},
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 60,
		},
		.cmd_null_pkt_en = 1,
		.cmd_null_pkt_len = 125,
		.data_rate = MIPI_DATA_RATE_120HZ,
		.round_corner_en = 1,
		.corner_pattern_height = ROUND_CORNER_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
		.corner_pattern_tp_size = sizeof(top_rc_pattern),
		.corner_pattern_lt_addr = (void *)top_rc_pattern,
		.corner_pattern_size_per_line = (void *)top_rc_pattern_size_per_line,
		.phy_timcon = {
			.lpx = 8, // HS Entry: DATA TLPX, 65ns
		},
		.real_te_duration = 16667,
	},
};
#ifdef CONFIG_MTK_RES_SWITCH_ON_AP
static enum RES_SWITCH_TYPE mtk_get_res_switch_type(void)
{
	lcm_info("res_switch_type: %d\n", res_switch_type);
	return res_switch_type;
}
#endif

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
#ifdef CONFIG_MTK_RES_SWITCH_ON_AP
	.get_res_switch_type = mtk_get_res_switch_type,
#endif
	.mode_switch = mode_switch,
	/* Not real backlight cmd in AOD, just for QC purpose */
	.set_aod_light_mode = lcm_setbacklight_cmdq,
#ifdef IF_ZERO
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
#endif
	.lcm_update_roi = lcm_update_roi,
	.lcm_update_roi_cmdq = lcm_update_roi_cmdq,
	.lcm_valid_roi = rm692h5_lcm_valid_roi,
};
#endif

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode[MODE_NUM];
	int i =0;

	mode[0] = drm_mode_duplicate(connector->dev, &display_mode[0]);
	if (!mode[0]) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			display_mode[0].hdisplay, display_mode[0].vdisplay,
			drm_mode_vrefresh(&display_mode[0]));
		return -ENOMEM;
	}

	mode[0]->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode[0]);
	drm_mode_set_name(mode[0]);

	lcm_info("The number of display mode is %d\n", MODE_NUM);

	for (i = 1; i < MODE_NUM; i++) {
		mode[i] = drm_mode_duplicate(connector->dev, &display_mode[i]);
		if (!mode[i]) {
			dev_info(connector->dev->dev, "not enough memory\n");
			return -ENOMEM;
		}

		mode[i]->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode[i]);
		drm_mode_set_name(mode[i]);
	}
	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return MODE_NUM;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

#ifdef IF_ZERO
u32 read_lcm_id_form_cmdline(void)
{
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node,
			"atag,videolfb", (int *)&size);
		if (videolfb_tag)
			return videolfb_tag->lcm_id;

		pr_info("[DT][videolfb] videolfb_tag not found\n");
	} else {
		pr_info("[DT][videolfb] of_chosen not found\n");
	}

	return false;
}
#endif

static u32 lcm_dvt_version_get(struct lcm *ctx)
{
#ifdef IF_ZERO
	u32 id1 = 0;
	u32 id3 = 0;
	u32 lcm_id = read_lcm_id_form_cmdline();

	ctx->dvv = DV1;
	id1 = lcm_id & 0xFF;
	id3 = lcm_id >> 16 & 0xFF;

	if (id1 == 0x89) {
		if (id3 < 0x07)
			ctx->dvv = DV2;
		else
			ctx->dvv = DV3;
	}
	pr_info("%s: lcm_id = %#x, dv = %#x\n", __func__, lcm_id, ctx->dvv);
#else
	ctx->dvv = DV3;
	pr_info("%s: lcm_id = %#x, dv = %#x\n", __func__, 0, ctx->dvv);
#endif

	return ctx->dvv;
}

int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	int ret;
#ifdef CONFIG_MTK_RES_SWITCH_ON_AP
	int value = 0;
#endif

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

	if (lcm_dvt_version_get(ctx) == DEFAULT_LCM_ID)
		return -ENODEV;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

#ifdef CONFIG_MTK_RES_SWITCH_ON_AP
	ret = of_property_read_u32(dev->of_node, "res-switch", &value);
	if (!ret)
		res_switch_type = (enum RES_SWITCH_TYPE)value;
	else
		lcm_err("res-switch read failure\n");
	lcm_info("res_switch_type = %d\n", res_switch_type);
#endif

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	if (lcm_panel_power_regulator_init(dev, ctx))
		pr_info("%s, lcm_panel_power_regulator_init Failed\n", __func__);
	//lcm_set_regulator(ctx->oled_vddi, 1);
	usleep_range(2000, 2100);
	//lcm_set_regulator(ctx->oled_vci, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->vddr_gpio, 1);
	usleep_range(2000, 2100);
	//nvtchip_power_on(ctx);

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
	ret = mtk_panel_ext_create(dev, &ext_params[0], &ext_funcs, &ctx->panel);
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
	{ .compatible = "mt6991,rm692h5,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-rm692h5-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Hugh Hsieh <hugh.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MT6991 RM692H5 CMD Panel Driver");
MODULE_LICENSE("GPL");
