// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "./panel-serdes-dp.h"
#include "../bridge/serdes-dp.h"

static inline struct panel_edp *to_panel_edp(struct drm_panel *panel)
{
	return container_of(panel, struct panel_edp, panel);
}

static int parse_timing_mode(struct panel_edp *edp, char *panel_name, char *panel_mode)
{
	struct device_node *np = edp->dev->of_node;
	struct device_node *panel = NULL;
	struct device_node *mode = NULL;
	struct display_timing timing;
	int ret;

	panel = of_get_child_by_name(np, panel_name);
	if (!panel) {
		dev_info(edp->dev, "%pOF: no %s node found\n", np, panel_name);
		return -EINVAL;
	}

	mode = of_get_child_by_name(panel, panel_mode);
	if (!mode) {
		dev_info(edp->dev, "%pOF: no %s node found\n", panel, panel_mode);
		of_node_put(panel);
		return -EINVAL;
	}

	ret = of_get_display_timing(mode, "panel-timing", &timing);
	if (ret < 0) {
		dev_info(edp->dev, "%pOF: problems parsing panel-timing (%d)\n",
			mode, ret);
		goto err;
	}

	videomode_from_timing(&timing, &edp->video_mode);

	ret = of_property_read_u32(mode, "width-mm", &edp->width);
	if (ret < 0) {
		dev_info(edp->dev, "%pOF: invalid or missing %s DT property\n",
			mode, "width-mm");
		goto err;
	}
	ret = of_property_read_u32(mode, "height-mm", &edp->height);
	if (ret < 0) {
		dev_info(edp->dev, "%pOF: invalid or missing %s DT property\n",
			mode, "height-mm");
		goto err;
	}

	of_node_put(mode);
	of_node_put(panel);
	return 0;
err:
	of_node_put(mode);
	of_node_put(panel);
	return ret;
}

static int panel_edp_disable(struct drm_panel *panel)
{
	struct panel_edp *edp = to_panel_edp(panel);

	dev_info(edp->dev, "%s %s\n", edp->debug_str, __func__);

	dev_info(edp->dev, "%s %s-\n", edp->debug_str, __func__);

	return 0;
}

static int panel_edp_unprepare(struct drm_panel *panel)
{
	struct panel_edp *edp = to_panel_edp(panel);

	dev_info(edp->dev, "%s %s+\n", edp->debug_str, __func__);

	dev_info(edp->dev, "%s %s-\n", edp->debug_str,  __func__);

	return 0;
}

static int panel_edp_prepare(struct drm_panel *panel)
{
	struct panel_edp *edp = to_panel_edp(panel);

	dev_info(edp->dev, "%s %s+\n", edp->debug_str, __func__);

	dev_info(edp->dev, "%s %s-\n", edp->debug_str, __func__);

	return 0;
}

static int panel_edp_enable(struct drm_panel *panel)
{
	struct panel_edp *edp = to_panel_edp(panel);

	dev_info(edp->dev, "%s %s+\n", edp->debug_str, __func__);

	dev_info(edp->dev, "%s %s-\n", edp->debug_str, __func__);

	return 0;
}

static int panel_edp_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct panel_edp *edp = to_panel_edp(panel);
	struct drm_display_mode *mode;
	int ret = 0;

	dev_info(edp->dev, "%s %s+\n", edp->debug_str, __func__);

	mode = drm_mode_create(connector->dev);
	if (!mode)
		return 0;

	if (!edp->use_default) {
		if (edp->is_dp)
			ret = get_panel_name_and_mode(edp->panel_name, edp->panel_mode, true);
		else
			ret = get_panel_name_and_mode(edp->panel_name, edp->panel_mode, false);

		if (ret)
			pr_info("%s get panel name or mode failed, use default setting\n", edp->debug_str);
		dev_info(edp->dev, "%s panel name: %s, panel mode: %s\n",
			edp->debug_str, edp->panel_name, edp->panel_mode);

		ret  = parse_timing_mode(edp, edp->panel_name, edp->panel_mode);
		if (ret)
			pr_info("%s parse timing mode error\n", edp->debug_str);
	}

	drm_display_mode_from_videomode(&edp->video_mode, mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = edp->width;
	connector->display_info.height_mm = edp->height;

	dev_info(edp->dev, "%s %s-\n", edp->debug_str, __func__);

	return 1;
}

static const struct drm_panel_funcs panel_edp_funcs = {
	.disable = panel_edp_disable,
	.unprepare = panel_edp_unprepare,
	.prepare = panel_edp_prepare,
	.enable = panel_edp_enable,
	.get_modes = panel_edp_get_modes,
};

static int panel_edp_parse_dt(struct panel_edp *edp)
{
	struct device_node *np = edp->dev->of_node;
	const char *panel_name = NULL;
	const char *panel_mode = NULL;
	int ret;
	u32 read_value;

	if (of_device_is_compatible(np, "panel-serdes-dp")) {
		edp->is_dp = true;
		strscpy(edp->debug_str, DEBUG_DP_INFO, 32);
	} else if (of_device_is_compatible(np, "panel-serdes-dp-mst1")) {
		edp->is_dp = true;
		strscpy(edp->debug_str, DEBUG_DP_MST1_INFO, 32);
	} else {
		edp->is_dp = false;
		strscpy(edp->debug_str, DEBUG_EDP_INFO, 32);
	}

	ret = of_property_read_u32(np, USE_DEFAULT_SETTING, &read_value);
	edp->use_default = (!ret) ? !!read_value : false;

	panel_name = of_get_property(np, PANEL_NAME, NULL);
	if (!panel_name) {
		dev_info(edp->dev, "%pOF: no panel name specified\n", np);
		return -EINVAL;
	}

	panel_mode = of_get_property(np, PANEL_MODE, NULL);
	if (!panel_mode) {
		dev_info(edp->dev, "%pOF: no panel mode specified\n", np);
		return -EINVAL;
	}

	memset(edp->panel_name, 0, PANEL_NAME_SIZE);
	strscpy(edp->panel_name, panel_name, PANEL_NAME_SIZE);
	memset(edp->panel_mode, 0, PANEL_NAME_SIZE);
	strscpy(edp->panel_mode, panel_mode, PANEL_NAME_SIZE);

	/* parse timing mode from default panel*/
	ret = parse_timing_mode(edp, edp->panel_name, edp->panel_mode);
	if (ret) {
		dev_info(edp->dev, "%pOF: failed parse timing mode from %s node\n", np, panel_name);
		return ret;
	}
	dev_info(edp->dev, "%s default panel: %s, mode: %s\n", edp->debug_str, edp->panel_name, edp->panel_mode);
	return 0;
}

static int panel_edp_probe(struct platform_device *pdev)
{
	struct panel_edp *edp;
	int ret;

	edp = devm_kzalloc(&pdev->dev, sizeof(*edp), GFP_KERNEL);
	if (!edp)
		return -ENOMEM;

	edp->dev = &pdev->dev;
	ret = panel_edp_parse_dt(edp);
	if (ret < 0)
		return ret;

	dev_info(&pdev->dev, "%s %s+\n", edp->debug_str, __func__);

	edp->supply = devm_regulator_get_optional(edp->dev, "power");
	if (IS_ERR(edp->supply)) {
		ret = PTR_ERR(edp->supply);

		if (ret != -ENODEV) {
			if (ret != -EPROBE_DEFER)
				dev_info(edp->dev, "failed to request regulator: %d\n",
					ret);
			return ret;
		}

		edp->supply = NULL;
	}

	/* Get GPIOs and backlight controller. */
	edp->enable_gpio = devm_gpiod_get_optional(edp->dev, "enable",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(edp->enable_gpio)) {
		ret = PTR_ERR(edp->enable_gpio);
		dev_info(edp->dev, "failed to request %s GPIO: %d\n",
			"enable", ret);
		return ret;
	}

	edp->power3v3_gpio = devm_gpiod_get_optional(edp->dev, "power3v3",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(edp->power3v3_gpio)) {
		ret = PTR_ERR(edp->power3v3_gpio);
		dev_info(edp->dev, "failed to request %s GPIO: %d\n",
			"power3v3", ret);
		return ret;
	}

	edp->reset_gpio = devm_gpiod_get_optional(edp->dev, "reset",
							GPIOD_OUT_HIGH);
	if (IS_ERR(edp->reset_gpio)) {
		ret = PTR_ERR(edp->reset_gpio);
		dev_info(edp->dev, "failed to request %s GPIO: %d\n",
			"reset", ret);
		return ret;
	}

	/*
	 * TODO: Handle all power supplies specified in the DT node in a generic
	 * way for panels that don't care about power supply ordering. eDP
	 * panels that require a specific power sequence will need a dedicated
	 * driver.
	 */

	/* Register the panel. */
	if (edp->is_dp)
		drm_panel_init(&edp->panel, edp->dev, &panel_edp_funcs,
					DRM_MODE_CONNECTOR_DisplayPort);
	else
		drm_panel_init(&edp->panel, edp->dev, &panel_edp_funcs,
					DRM_MODE_CONNECTOR_eDP);

	drm_panel_add(&edp->panel);

	dev_info(&pdev->dev, "%s %s-\n", edp->debug_str, __func__);

	dev_set_drvdata(edp->dev, edp);
	return 0;
}

static int panel_edp_remove(struct platform_device *pdev)
{
	struct panel_edp *edp = dev_get_drvdata(&pdev->dev);

	drm_panel_remove(&edp->panel);
	panel_edp_disable(&edp->panel);

	return 0;
}

static const struct of_device_id panel_edp_of_table[] = {
	{ .compatible = "panel-edp", },
	{ .compatible = "panel-serdes-dp", },
	{ .compatible = "panel-serdes-dp-mst1", },
	{ /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, panel_edp_of_table);

static struct platform_driver panel_edp_driver = {
	.probe		= panel_edp_probe,
	.remove		= panel_edp_remove,
	.driver		= {
		.name	= "panel-edp",
		.of_match_table = panel_edp_of_table,
	},
};

module_platform_driver(panel_edp_driver);

MODULE_AUTHOR("Jacky Hu <jie-h.hu@mediatek.com>");
MODULE_DESCRIPTION("Panel Driver For DP and EDP");
MODULE_LICENSE("GPL");
