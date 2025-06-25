// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "./serdes-dp.h"

static char *global_panel_dp_name;
static char *global_panel_dp_mode;
static char *global_panel_edp_name;
static char *global_panel_edp_mode;

static const char * const props[] = {DES_I2C_ADDR, BL_I2C_ADDR, DES_LINKA_I2C_ADDR,
					DES_LINKA_BL_I2C_ADDR, DES_LINKB_I2C_ADDR, DES_LINKB_BL_I2C_ADDR};
static const char * const init_cmds[] = {SERDES_INIT_LINKA_CMD, DES_LINKA_INIT_CMD,
					SERDES_INIT_LINKB_CMD, DES_LINKB_INIT_CMD, SER_SUPERFRAME_INIT_CMD};
static const char * const dp_mst_init_cmds[] = {SERDES_INIT_LINKA_CMD, DES_LINKA_INIT_CMD,
					SERDES_INIT_LINKB_CMD, DES_LINKB_INIT_CMD, DP_MST_INIT_CMD};

static DEFINE_MUTEX(edp_i2c_access);

int get_panel_name_and_mode(char *panel_name, char *panel_mode, bool is_dp)
{
	if (is_dp) {
		if (!global_panel_dp_name || !global_panel_dp_mode ||
			!panel_name || !panel_mode)
			return -EINVAL;

		strscpy(panel_name, global_panel_dp_name, PANEL_NAME_SIZE);
		strscpy(panel_mode, global_panel_dp_mode, PANEL_NAME_SIZE);
	} else {
		if (!global_panel_edp_name || !global_panel_edp_mode ||
			!panel_name || !panel_mode)
			return -EINVAL;

		strscpy(panel_name, global_panel_edp_name, PANEL_NAME_SIZE);
		strscpy(panel_mode, global_panel_edp_mode, PANEL_NAME_SIZE);
	}

	return 0;
}
EXPORT_SYMBOL(get_panel_name_and_mode);

static u8 serdes_read_byte(struct i2c_client *client,
								u16 addr)
{
	u8 buf;
	int ret = 0;
	char read_data[2] = { 0 };

	read_data[0] = addr >> 8;
	read_data[1] = addr & 0xFF;

	mutex_lock(&edp_i2c_access);

	ret = i2c_master_send(client, read_data, 2);
	if (ret <= 0) {
		mutex_unlock(&edp_i2c_access);
#ifdef SERDES_DEBUG
		pr_info("%s Failed to send i2c command, ret = %d\n", SERDES_DEBUG_INFO, ret);
#endif
		return 0;
	}

	ret = i2c_master_recv(client, &buf, 1);
	if (ret <= 0) {
		mutex_unlock(&edp_i2c_access);
#ifdef SERDES_DEBUG
		pr_info("%s Failed to recv i2c data, ret = %d\n", SERDES_DEBUG_INFO, ret);
#endif
		return 0;
	}

	mutex_unlock(&edp_i2c_access);

	return buf;
}

static int serdes_write_byte(struct i2c_client *client, u16 addr,
								unsigned char val)
{
	char write_data[3] = { 0 };
	int ret;

	if (!client) {
		pr_info("%s %s: client is NULL\n", SERDES_DEBUG_INFO, __func__);
		return -1;
	}

	mutex_lock(&edp_i2c_access);

	write_data[0] = addr >> 8;
	write_data[1] = addr & 0xFF;
	write_data[2] = val;

	ret = i2c_master_send(client, write_data, 3);
	if (ret <= 0) {
		mutex_unlock(&edp_i2c_access);
#ifdef SERDES_DEBUG
		pr_info("%s I2C write fail, addr:0x%x val:0x%x ret:%d\n",
				SERDES_DEBUG_INFO, addr, val, ret);
#endif
		return ret;
	}

	mutex_unlock(&edp_i2c_access);

	return 0;
}

static inline struct serdes_dp_bridge *
			bridge_to_serdes_dp(struct drm_bridge *bridge)
{
	return container_of(bridge, struct serdes_dp_bridge, bridge);
}

static int serdes_dp_create_i2c_client(struct serdes_dp_bridge *max_bridge)
{
	u16 addr = 0;

	addr = (u16)max_bridge->des_i2c_addr;
	max_bridge->max96752_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);

	addr = (u16)max_bridge->bl_i2c_addr;
	max_bridge->bl_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);

	if (!max_bridge->max96752_i2c || !max_bridge->bl_i2c) {
		pr_info("%s Failed to create i2c client for max96752 or BL!\n", SERDES_DEBUG_INFO);
		return -ENODEV;
	}

	if (max_bridge->superframe_support) {
		addr = (u16)max_bridge->linka_i2c_addr;
		max_bridge->max96752_linka_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		addr = (u16)max_bridge->linka_bl_addr;
		max_bridge->max96752_linka_bl_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		addr = (u16)max_bridge->linkb_i2c_addr;
		max_bridge->max96752_linkb_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		addr = (u16)max_bridge->linkb_bl_addr;
		max_bridge->max96752_linkb_bl_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
	} else if (max_bridge->double_pixel_support) {
		pr_info("%s double pixel support\n", SERDES_DEBUG_INFO);
	} else if (max_bridge->is_support_mst) {
		addr = (u16)max_bridge->linka_i2c_addr;
		max_bridge->max96752_linka_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		addr = (u16)max_bridge->linka_bl_addr;
		max_bridge->max96752_linka_bl_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		addr = (u16)max_bridge->linkb_i2c_addr;
		max_bridge->max96752_linkb_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		addr = (u16)max_bridge->linkb_bl_addr;
		max_bridge->max96752_linkb_bl_i2c = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, addr);
		pr_info("%s DP MST support\n", SERDES_DEBUG_INFO);
	}

	return 0;
}

static void ser_init_loop(struct serdes_dp_bridge *max_bridge, struct serdes_cmd_info *cmd_data)
{
	int i, ret;

	for (i = 0; i < cmd_data->length; i++) {
		ret = serdes_write_byte(max_bridge->serdes_dp_i2c, cmd_data->addr[i], cmd_data->data[i]);
		if (ret) {
#ifdef SERDES_DEBUG
			pr_info("%s Write ser command failed, addr=0x%x data=0x%x ret: %d\n",
					SERDES_DEBUG_INFO, cmd_data->addr[i], cmd_data->data[i], ret);
#endif
		}

		if (cmd_data->delay_ms[i])
			msleep(cmd_data->delay_ms[i]);
#ifdef SERDES_DEBUG
		pr_info("%s i2c client: 0, i2c addr:0x%04x value: 0x%04x delay:%d\n",
				SERDES_DEBUG_INFO, cmd_data->addr[i], cmd_data->data[i], cmd_data->delay_ms[i]);
#endif
	}
}

static void des_init_loop(struct serdes_dp_bridge *max_bridge, struct serdes_cmd_info *cmd_data)
{
	int i, ret;

	for (i = 0; i < cmd_data->length; i++) {
		ret = serdes_write_byte(max_bridge->max96752_i2c, cmd_data->addr[i], cmd_data->data[i]);
		if (ret) {
#ifdef SERDES_DEBUG
			pr_info("%s Write des command failed, addr=0x%x data=0x%x ret: %d\n",
					SERDES_DEBUG_INFO, cmd_data->addr[i], cmd_data->data[i], ret);
#endif
		}

		if (cmd_data->delay_ms[i])
			msleep(cmd_data->delay_ms[i]);
#ifdef SERDES_DEBUG
		pr_info("%s i2c client: 1, i2c addr:0x%04x value: 0x%04x delay:%d\n",
			SERDES_DEBUG_INFO, cmd_data->addr[i], cmd_data->data[i], cmd_data->delay_ms[i]);
#endif

	}
}

static void serdes_init_loop(struct serdes_dp_bridge *max_bridge, struct serdes_cmd_info *cmd_data)
{
	int i, ret;
	struct i2c_client *client = NULL;

	for (i = 0; i < cmd_data->length; i++) {
		switch (cmd_data->obj[i]) {
		case 0:
			client = max_bridge->serdes_dp_i2c;
			break;
		case 1:
			client = max_bridge->max96752_i2c;
			break;
		case 2:
			client = max_bridge->max96752_linka_i2c;
			break;
		case 3:
			client = max_bridge->max96752_linkb_i2c;
			break;
		default:
			pr_info("%s Invalid obj id: %d\n", SERDES_DEBUG_INFO, cmd_data->obj[i]);
			return;
		}
		ret = serdes_write_byte(client, cmd_data->addr[i], cmd_data->data[i]);
		if (ret) {
#ifdef SERDES_DEBUG
			pr_info("%s Write seres command failed, addr=0x%x data=0x%x ret: %d\n",
					SERDES_DEBUG_INFO, cmd_data->addr[i], cmd_data->data[i], ret);
#endif
		}

		if (cmd_data->delay_ms[i])
			msleep(cmd_data->delay_ms[i]);
#ifdef SERDES_DEBUG
		pr_info("%s i2c client:%d i2c addr:0x%04x value:0x%04x delay_ms:%d\n",
			SERDES_DEBUG_INFO, cmd_data->obj[i], cmd_data->addr[i], cmd_data->data[i],
			cmd_data->delay_ms[i]);
#endif
	}
}


static int parse_data_by_prop_from_dts(struct device_node *np, const char *prop,
										u32 *data)
{
	int ret = 0;

	ret = of_property_read_u32(np, prop, data);
	if (ret)
		return ret;
	return 0;
}

static int parse_init_cmd_by_prop_from_dts(struct serdes_dp_bridge *max_bridge,
					struct device_node *np, const char *init_cmd_prop,
					struct serdes_cmd_info *init_cmd)
{
	u32 *array = NULL;
	int ret = 0, num = 0, i = 0;

	num = of_property_count_u32_elems(np, init_cmd_prop);
	if (num < 0) {
		pr_info("%s %s read fail\n", SERDES_DEBUG_INFO, init_cmd_prop);
		return -EINVAL;
	} else if (num % 4 != 0) {
		pr_info("%s %s data error\n", SERDES_DEBUG_INFO, init_cmd_prop);
		return -EINVAL;
	}

	array = devm_kzalloc(max_bridge->dev, num * sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	init_cmd->length = num / 4;
	init_cmd->obj = devm_kzalloc(max_bridge->dev, num * sizeof(u8) / 4, GFP_KERNEL);
	init_cmd->addr = devm_kzalloc(max_bridge->dev, num * sizeof(u16) / 4, GFP_KERNEL);
	init_cmd->data = devm_kzalloc(max_bridge->dev, num * sizeof(u8) / 4, GFP_KERNEL);
	init_cmd->delay_ms = devm_kzalloc(max_bridge->dev, num * sizeof(u16) / 4, GFP_KERNEL);

	if (!init_cmd->addr || !init_cmd->data || !init_cmd->delay_ms)
		goto err;

	ret = of_property_read_u32_array(np, init_cmd_prop, array, num);
	if (ret)
		goto err;

	for (i = 0; i < init_cmd->length; i++) {
		for (ret = 0; ret < 4; ret++) {
			if (ret == 0)
				init_cmd->obj[i] = (u8)array[i*4 + ret];
			else if (ret == 1)
				init_cmd->addr[i] = (u16)(array[i*4 + ret]);
			else if (ret == 2)
				init_cmd->data[i] = (u8)array[i*4 + ret];
			else
				init_cmd->delay_ms[i] = (u16)array[i*4 + ret];
		}
	}

	devm_kfree(max_bridge->dev, array);
	return 0;
err:
	devm_kfree(max_bridge->dev, array);
	return ret;
}

static int parse_feature_cmd_by_prop_from_dts(struct serdes_dp_bridge *max_bridge,
			struct feature_info *feature_info, const char *feature_cmd_prop,
			struct device_node *feature_handle_np)
{
	u32 *array = NULL;
	int ret = 0, num = 0, i = 0, j = 0, k = 0;

	num = of_property_count_u32_elems(feature_handle_np, feature_cmd_prop);
	if (num < 0) {
		pr_info("%s %s read fail\n", SERDES_DEBUG_INFO, feature_cmd_prop);
		return -EINVAL;
	}

	array = devm_kzalloc(max_bridge->dev, num * sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	ret = of_property_read_u32_array(feature_handle_np, feature_cmd_prop, array, num);
	if (ret)
		goto err;

	/* cmd group*/
	feature_info->group_num = (u8)array[0];

	feature_info->feature_cmd = devm_kzalloc(max_bridge->dev,
						feature_info->group_num * sizeof(struct feature_info), GFP_KERNEL);

	/* parse cmd */
	for (i = 0; i < feature_info->group_num; i++) {
		for (j = 1; j < num; j++) {
			feature_info->feature_cmd[i].cmd_type = (u8)array[j++];
			feature_info->feature_cmd[i].i2c_addr = (u16)array[j++];
			feature_info->feature_cmd[i].cmd_length = (u8)array[j++];
			feature_info->feature_cmd[i].data = devm_kzalloc(max_bridge->dev,
				feature_info->feature_cmd[i].cmd_length * sizeof(u8), GFP_KERNEL);
			for (k = 0; k < feature_info->feature_cmd[i].cmd_length; k++)
				feature_info->feature_cmd[i].data[k] = (u8)array[j++];
		}
	}

	devm_kfree(max_bridge->dev, array);
	return 0;
err:
	devm_kfree(max_bridge->dev, array);
	return ret;

}

static int excute_feature_check_cmd(struct serdes_dp_bridge *max_bridge,
				struct feature_cmd feature_cmd)
{
	struct i2c_client *client = NULL;
	int ret = 0;

	client = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, feature_cmd.i2c_addr);
	if (IS_ERR(client)) {
		pr_info("%s Failed to create dummy I2C device 0x%x\n", SERDES_DEBUG_INFO, feature_cmd.i2c_addr);
		return PTR_ERR(client);
	}

	if (feature_cmd.cmd_type) {
		ret = i2c_master_send(client, feature_cmd.data, feature_cmd.cmd_length);
		if (ret < 0) {
			pr_info("%s Failed to send data to device 0x%x %d\n", SERDES_DEBUG_INFO,
					feature_cmd.i2c_addr, ret);
			goto err;
		}
	}

	i2c_unregister_device(client);
	return 0;
err:
	i2c_unregister_device(client);
	return ret;
}

static int excute_feature_verify_cmd(struct serdes_dp_bridge *max_bridge, struct feature_cmd feature_cmd)
{
	struct i2c_client *client = NULL;
	int ret = 0, i = 0;
	u8 value[32] = {0};

	client = i2c_new_dummy_device(max_bridge->serdes_dp_i2c->adapter, feature_cmd.i2c_addr);
	if (IS_ERR(client)) {
		pr_info("%s Failed to create dummy I2C device 0x%x\n", SERDES_DEBUG_INFO, feature_cmd.i2c_addr);
		return PTR_ERR(client);
	}

	if (!feature_cmd.cmd_type) {
		ret = i2c_master_recv(client, value, feature_cmd.cmd_length / 2);
		if (ret < 0) {
			pr_info("%s Failed to receive data from device 0x%x %d\n",
					SERDES_DEBUG_INFO, feature_cmd.i2c_addr, ret);
			goto err;
		}
	}

	/* verify the received data */
	for (i = 0; i < feature_cmd.cmd_length; i += 2) {
		pr_info("%s value:%x mask:%x verify:%x\n", SERDES_DEBUG_INFO,
				value[i/2],feature_cmd.data[i], feature_cmd.data[i+1]);
		if ((value[i/2] & feature_cmd.data[i]) != feature_cmd.data[i+1]) {
			pr_info("%s Received data does not match with expected value, doesn't apply this setting\n",
					SERDES_DEBUG_INFO);
			ret = -EINVAL;
			goto err;
		}
	}

	i2c_unregister_device(client);
	return 0;
err:
	i2c_unregister_device(client);
	return ret;
}

static int feature_handle(struct serdes_dp_bridge *max_bridge, struct device_node *feature_handle_np)
{
	struct feature_info check_cmd;
	struct feature_info verify_cmd;
	char *feature_setting = NULL;
	int ret = 0, i = 0;

	ret = parse_feature_cmd_by_prop_from_dts(max_bridge, &check_cmd, FEATURE_CHECK_CMD, feature_handle_np);
	if (ret)
		return ret;

	ret = parse_feature_cmd_by_prop_from_dts(max_bridge, &verify_cmd, FEATURE_VERIFY_CMD, feature_handle_np);
	if (ret) {
		devm_kfree(max_bridge->dev, check_cmd.feature_cmd->data);
		devm_kfree(max_bridge->dev, check_cmd.feature_cmd);
		return ret;
	}

	if (check_cmd.group_num != verify_cmd.group_num)
		goto err;

	for (i = 0; i < check_cmd.group_num; i++) {
		ret = excute_feature_check_cmd(max_bridge, check_cmd.feature_cmd[i]);
		if (ret)
			goto err;
		ret = excute_feature_verify_cmd(max_bridge, verify_cmd.feature_cmd[i]);
		if (!ret)
			break;
	}

	/* apply feature setting */
	if (!ret) {
		feature_setting = (char *)of_get_property(feature_handle_np, FEATURE_SETTING, NULL);
		if (feature_setting) {
			memset(max_bridge->panel_name, 0, PANEL_NAME_SIZE);
			strscpy(max_bridge->panel_name, feature_setting, PANEL_NAME_SIZE);
		}
	}

	devm_kfree(max_bridge->dev, check_cmd.feature_cmd->data);
	devm_kfree(max_bridge->dev, check_cmd.feature_cmd);
	devm_kfree(max_bridge->dev, verify_cmd.feature_cmd->data);
	devm_kfree(max_bridge->dev, verify_cmd.feature_cmd);
	return 0;
err:
	devm_kfree(max_bridge->dev, check_cmd.feature_cmd->data);
	devm_kfree(max_bridge->dev, check_cmd.feature_cmd);
	devm_kfree(max_bridge->dev, verify_cmd.feature_cmd->data);
	devm_kfree(max_bridge->dev, verify_cmd.feature_cmd);
	return ret;
}

static int parse_feature_info_from_dts(struct serdes_dp_bridge *max_bridge, struct device_node *feature_np,
									u32 feature_handle_num)
{
	int ret = 0, i = 0;
	char feature_handle_name[64] = {0};
	struct device_node *feature_handle_np = NULL;

	/* feature-handle number process */
	for (i = 0; i < feature_handle_num; i++) {
		memset(feature_handle_name, 0, 64);
		ret = sprintf(feature_handle_name, "%s%d", PANEL_FEATURE_HANDLE, i);
		if (ret < 0) {
			pr_info("%s %s failed to sprintf feature handle name %d\n", SERDES_DEBUG_INFO, __func__, ret);
			return ret;
		}

		feature_handle_np = of_get_child_by_name(feature_np, feature_handle_name);
		if (!feature_handle_np) {
			pr_info("%s %s failed to find feature handle node %s\n",
					SERDES_DEBUG_INFO, __func__, feature_handle_name);
			return -EINVAL;
		}

		/* parse feature handle info from dts */
		ret = feature_handle(max_bridge, feature_handle_np);
		if (ret) {
			pr_info("%s %s failed to handle feature\n", SERDES_DEBUG_INFO, __func__);
			goto err;
		}
	}

	of_node_put(feature_handle_np);
	return 0;
err:
	of_node_put(feature_handle_np);
	return -EINVAL;
}

static int parse_and_process_bl_cmd_from_dts(struct serdes_dp_bridge *max_bridge,
				struct device_node *np, const char *prop_name, struct bl_cmd_info *bl_cmd)
{
	u32 *array;
	int ret = 0, num = 0, i = 0;

	num = of_property_count_u32_elems(np, prop_name);
	if (num < 0) {
		pr_info("%s %s read fail\n", SERDES_DEBUG_INFO, prop_name);
		return -EINVAL;
	}

	bl_cmd->length = num;
	array = devm_kzalloc(max_bridge->dev, num * sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	bl_cmd->data = devm_kzalloc(max_bridge->dev, num * sizeof(u8), GFP_KERNEL);
	if (!bl_cmd->data)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, prop_name, array, num);
	if (ret)
		return ret;

	for (i = 0; i < num; i++)
		bl_cmd->data[i] = (u8)array[i];

	devm_kfree(max_bridge->dev, array);

	return 0;
}

static int get_bl_on_off_cmd_info_from_dts(struct serdes_dp_bridge *max_bridge, struct device_node *np)
{
	int ret;

	ret = parse_and_process_bl_cmd_from_dts(max_bridge, np, BL_ON_CMD, &max_bridge->bl_on_cmd);
	if (ret)
		return ret;

	ret = parse_and_process_bl_cmd_from_dts(max_bridge, np, BL_OFF_CMD, &max_bridge->bl_off_cmd);
	if (ret)
		return ret;

	return 0;
}

static int parse_multi_view_cmd_from_dts(struct serdes_dp_bridge *max_bridge, struct device_node *multi_view_setting_np)
{
	int ret = 0;

	for (int i = 0; i < ARRAY_SIZE(props); i++) {
		ret = parse_data_by_prop_from_dts(multi_view_setting_np, props[i], &max_bridge->des_i2c_addr + i);
		if (ret) {
			pr_info("%s failed to parse %s property\n", SERDES_DEBUG_INFO, props[i]);
			return ret;
		}
	}

	pr_info("%s des-i2c-addr:0x%x bl-i2c-addr:0x%x\n", SERDES_DEBUG_INFO,
			max_bridge->des_i2c_addr, max_bridge->bl_i2c_addr);
	pr_info("%s des-linka-i2c-addr:0x%x des-linka-bl-i2c-addr:0x%x\n",
			SERDES_DEBUG_INFO, max_bridge->linka_i2c_addr,
			max_bridge->linka_bl_addr);
	pr_info("%s des-linkb-i2c-addr:0x%x des-linkb-bl-i2c-addr:0x%x\n",
			SERDES_DEBUG_INFO, max_bridge->linkb_i2c_addr, max_bridge->linkb_bl_addr);

	/* Parse init commands */

	for (int i = 0; i < ARRAY_SIZE(init_cmds); i++) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, multi_view_setting_np, init_cmds[i],
					&max_bridge->serdes_init_linka_cmd + i);
		if (ret) {
			pr_info("%s failed to parse %s property\n", SERDES_DEBUG_INFO, init_cmds[i]);
			return ret;
		}
	}

	if (max_bridge->is_support_touch) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, multi_view_setting_np,
										SERDES_SUPERFRAME_TOUCH_INIT_CMD,
								&max_bridge->serdes_superframe_touch_init_cmd);
		if (ret) {
			pr_info("%s failed to parse %s property\n", SERDES_DEBUG_INFO,
					SERDES_SUPERFRAME_TOUCH_INIT_CMD);
			return ret;
		}
	}

	/* Parse backlight settings */
	ret = get_bl_on_off_cmd_info_from_dts(max_bridge, multi_view_setting_np);
	if (ret) {
		pr_info("%s Failed to parse superframe backlight setting\n", SERDES_DEBUG_INFO);
		return ret;
	}

	return 0;
}


static int parse_multi_view_setting(struct serdes_dp_bridge *max_bridge,
							struct device_node *superframe_setting_np)
{
	struct device_node *multi_view_np;
	u32 read_value = 0;
	int ret;

	/* asymmetric-multi-view check */
	ret = of_property_read_u32(superframe_setting_np, ASYMMETRIC_MULTI_VIEW, &read_value);
	if (!ret)
		max_bridge->asymmetric_multi_view = read_value ? true : false;
	else
		max_bridge->asymmetric_multi_view = false;

	pr_info("%s Asymmetric multi-view: %d\n", SERDES_DEBUG_INFO, max_bridge->asymmetric_multi_view);

	if (max_bridge->asymmetric_multi_view)
		multi_view_np = of_get_child_by_name(superframe_setting_np, ASYSMMETRIC_MULTI_VIEW_SETTING);
	else
		multi_view_np = of_get_child_by_name(superframe_setting_np, SYSMMETRIC_MULTI_VIEW_SETTING);

	if (!multi_view_np) {
		pr_info("%s No multi-view setting node found!\n", SERDES_DEBUG_INFO);
		return -EINVAL;
	}

	ret = parse_multi_view_cmd_from_dts(max_bridge, multi_view_np);
	of_node_put(multi_view_np);

	return ret;
}

static void get_general_info_from_dts(struct serdes_dp_bridge *max_bridge)
{
	struct device_node *np = max_bridge->dev->of_node;
	u32 read_value = 0;
	int ret;

	ret = of_property_read_u32(np, USE_FOR_DP, &read_value);
	max_bridge->is_dp = (!ret) ? !!read_value : false;
	pr_info("%s Serdes for: %d [0: eDP, 1: DP]\n", SERDES_DEBUG_INFO, max_bridge->is_dp);

	ret = of_property_read_u32(np, SERDES_SUPPORT_HOTPLUG, &read_value);
	max_bridge->is_support_hotplug = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SERDES_SUPER_FRAME, &read_value);
	max_bridge->superframe_support = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SERDES_DOUBLE_PIXEL, &read_value);
	max_bridge->double_pixel_support = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SUPPORT_MST, &read_value);
	max_bridge->is_support_mst = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SUPPORT_TOUCH, &read_value);
	max_bridge->is_support_touch = (!ret) ? !!read_value : false;

	pr_info("%s ser-super-frame: %d ser-double-pixel: %d ser-dp-mst: %d support-touch: %d\n",
			SERDES_DEBUG_INFO, max_bridge->superframe_support, max_bridge->double_pixel_support,
			max_bridge->is_support_mst, max_bridge->is_support_touch);

	pr_info("%s is_support_hotplug: %d\n", SERDES_DEBUG_INFO, max_bridge->is_support_hotplug);
}

static int get_panel_feature_info_from_dts(struct serdes_dp_bridge *max_bridge)
{
	struct device_node *np = max_bridge->dev->of_node;
	struct device_node *feature_np = NULL;
	u32 feature_handle_num = 0, read_value;
	int ret = 0;

	ret = of_property_read_u32(max_bridge->dev->of_node, USE_DEFAULT_SETTING,
							&read_value);
	if (!!read_value)
		return read_value;

	/* Get child node feature */
	feature_np = of_get_child_by_name(np, PANEL_FEATURE);
	if (!feature_np) {
		pr_info("%s Failed to find panel feature use default setting\n", SERDES_DEBUG_INFO);
		return -EINVAL;
	}

	ret = of_property_read_u32(feature_np, PANEL_FEATURE_HANDLE_NUM, &feature_handle_num);
	if (ret || !feature_handle_num) {
		pr_info("%s feature-handle-num not exist or value is zero, use  default setting\n", SERDES_DEBUG_INFO);
		goto err;
	}

	ret = parse_feature_info_from_dts(max_bridge, feature_np, feature_handle_num);
	if (ret) {
		pr_info("%s failed to parse panel feature info from dts, use default setting\n", SERDES_DEBUG_INFO);
		goto err;
	}

	of_node_put(feature_np);
	pr_info("%s Supported panel: %s\n", max_bridge->panel_name, SERDES_DEBUG_INFO);
	return 0;
err:
	of_node_put(feature_np);
	pr_info("%s Supported panel: %s\n", max_bridge->panel_name, SERDES_DEBUG_INFO);
	return ret;
}

static int get_signal_setting_from_dts(struct serdes_dp_bridge *max_bridge,
									  struct device_node *panel_setting_np)
{
	struct device_node *signal_setting_np;
	int ret;

	/* Get child node serdes_dp_signal_setting */
	signal_setting_np = of_get_child_by_name(panel_setting_np, SERDES_DP_SST_SETTING);
	if (!signal_setting_np) {
		pr_info("%s %s No signal setting node!\n", SERDES_DEBUG_INFO, __func__);
		return -EINVAL;
	}

	/* Parse DES and BL I2C addresses */
	ret = parse_data_by_prop_from_dts(signal_setting_np, DES_I2C_ADDR, &max_bridge->des_i2c_addr);
	if (ret) {
		pr_info("%s Failed to parse %s property\n", SERDES_DEBUG_INFO, DES_I2C_ADDR);
		goto err;
	}

	ret = parse_data_by_prop_from_dts(signal_setting_np, BL_I2C_ADDR, &max_bridge->bl_i2c_addr);
	if (ret) {
		pr_info("%s Failed to parse %s property\n", SERDES_DEBUG_INFO, BL_I2C_ADDR);
		goto err;
	}

	pr_info("%s DES I2C Addr: 0x%x, BL I2C Addr: 0x%x\n",
			SERDES_DEBUG_INFO, max_bridge->des_i2c_addr, max_bridge->bl_i2c_addr);

	/* Parse signal init command info from dts */
	ret = parse_init_cmd_by_prop_from_dts(max_bridge, signal_setting_np,
				SER_INIT_CMD_NAME, &max_bridge->signal_ser_init_cmd);
	if (ret) {
		pr_info("%s Failed to parse %s property\n", SERDES_DEBUG_INFO, SER_INIT_CMD_NAME);
		goto err;
	}

	ret = parse_init_cmd_by_prop_from_dts(max_bridge, signal_setting_np,
					DES_INIT_CMD_NAME, &max_bridge->signal_des_init_cmd);
	if (ret) {
		pr_info("%s Failed to parse %s property\n", SERDES_DEBUG_INFO, DES_INIT_CMD_NAME);
		goto err;
	}

	if (max_bridge->is_support_touch) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, signal_setting_np, TOUCH_INIT_CMD_ADDR,
							&max_bridge->signal_touch_init_cmd);
		if (ret) {
			pr_info("%s Failed to parse %s property\n", SERDES_DEBUG_INFO, TOUCH_INIT_CMD_ADDR);
			goto err;
		}
	}

	/* Parse signal backlight settings from dts */
	ret = get_bl_on_off_cmd_info_from_dts(max_bridge, signal_setting_np);
	if (ret) {
		pr_info("%s Failed to parse backlight setting\n", SERDES_DEBUG_INFO);
		goto err;
	}

	of_node_put(signal_setting_np);
	return 0;

err:
	of_node_put(signal_setting_np);
	return ret;
}

static int get_superframe_setting_from_dts(struct serdes_dp_bridge *max_bridge, struct device_node *panel_setting_np)
{
	struct device_node *superframe_setting_np;
	int ret = 0;

	superframe_setting_np = of_get_child_by_name(panel_setting_np, SERDES_DP_SUPERFRAME_SETTING);
	if (!superframe_setting_np) {
		pr_info("%s %s No superframe setting node found!\n", SERDES_DEBUG_INFO, __func__);
		return -EINVAL;
	}

	ret = parse_multi_view_setting(max_bridge, superframe_setting_np);
	if (ret)
		pr_info("%s %s Failed to parse multi view setting from DTS!\n", SERDES_DEBUG_INFO, __func__);

	of_node_put(superframe_setting_np);

	return ret;
}

static int get_double_pixel_setting_from_dts(struct serdes_dp_bridge *max_bridge, struct device_node *panel_setting_np)
{
	return 0;
}

static int parse_dp_mst_setting(struct serdes_dp_bridge *max_bridge, struct device_node *dp_mst_setting_np)
{
	int ret = 0;

	for (int i = 0; i < ARRAY_SIZE(props); i++) {
		ret = parse_data_by_prop_from_dts(dp_mst_setting_np, props[i], &max_bridge->des_i2c_addr + i);
		if (ret) {
			pr_info("%s failed to parse %s property\n", SERDES_DEBUG_INFO, props[i]);
			return ret;
		}
	}

	pr_info("%s des-i2c-addr:0x%x bl-i2c-addr:0x%x\n", SERDES_DEBUG_INFO,
			max_bridge->des_i2c_addr, max_bridge->bl_i2c_addr);
	pr_info("%s des-linka-i2c-addr:0x%x des-linka-bl-i2c-addr:0x%x\n",
			SERDES_DEBUG_INFO, max_bridge->linka_i2c_addr,
			max_bridge->linka_bl_addr);
	pr_info("%s des-linkb-i2c-addr:0x%x des-linkb-bl-i2c-addr:0x%x\n",
			SERDES_DEBUG_INFO, max_bridge->linkb_i2c_addr, max_bridge->linkb_bl_addr);

	/* Parse init commands */

	for (int i = 0; i < ARRAY_SIZE(dp_mst_init_cmds); i++) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, dp_mst_setting_np, dp_mst_init_cmds[i],
					&max_bridge->mst_serdes_init_linka_cmd + i);
		if (ret) {
			pr_info("%s failed to parse %s property\n", SERDES_DEBUG_INFO, dp_mst_init_cmds[i]);
			return ret;
		}
	}

	if (max_bridge->is_support_touch) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, dp_mst_setting_np,
										serdes_dp_DP_MST_TOUCH_INIT_CMD,
								&max_bridge->dp_mst_touch_init_cmd);
		if (ret) {
			pr_info("%s failed to parse %s property\n", SERDES_DEBUG_INFO, serdes_dp_DP_MST_TOUCH_INIT_CMD);
			return ret;
		}
	}

	/* Parse backlight settings */
	ret = get_bl_on_off_cmd_info_from_dts(max_bridge, dp_mst_setting_np);
	if (ret) {
		pr_info("%s Failed to parse DP MST backlight setting\n", SERDES_DEBUG_INFO);
		return ret;
	}

	return 0;
}

static int get_mst_setting_from_dts(struct serdes_dp_bridge *max_bridge,
										struct device_node *panel_setting_np)
{
	struct device_node *dp_mst_setting_np;
	int ret = 0;

	dp_mst_setting_np = of_get_child_by_name(panel_setting_np, SERDES_DP_MST_SETTING);
	if (!dp_mst_setting_np) {
		pr_info("%s %s No dp mst setting node found!\n", SERDES_DEBUG_INFO, __func__);
		return -EINVAL;
	}

	ret = parse_dp_mst_setting(max_bridge, dp_mst_setting_np);
	if (ret)
		pr_info("%s %s Failed to parse multi view setting from DTS!\n", SERDES_DEBUG_INFO, __func__);

	of_node_put(dp_mst_setting_np);
	return ret;
}

static int get_serdes_setting_from_dts(struct serdes_dp_bridge *max_bridge)
{
	int ret = 0;
	struct device_node *np = max_bridge->dev->of_node;
	struct device_node *panel_setting_np;
	const char *panel_name;

	panel_name = of_get_property(np, PANEL_NAME, NULL);
	if (!panel_name) {
		pr_info("%s Panel name is not provided\n", SERDES_DEBUG_INFO);
		return -EINVAL;
	}

	max_bridge->panel_name = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
	if (!max_bridge->panel_name)
		return -ENOMEM;

	strscpy(max_bridge->panel_name, panel_name, PANEL_NAME_SIZE);
	pr_info("%s support panel %s\n", SERDES_DEBUG_INFO, max_bridge->panel_name);

	panel_setting_np = of_get_child_by_name(np, max_bridge->panel_name);
	if (!panel_setting_np) {
		pr_info("%s %s failed to find %s node\n", SERDES_DEBUG_INFO, __func__, max_bridge->panel_name);
		return -EINVAL;
	}

	if (max_bridge->is_dp) {
		global_panel_dp_name = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_dp_name)
			return -ENOMEM;

		global_panel_dp_mode = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_dp_mode)
			return -ENOMEM;

		strscpy(global_panel_dp_name, max_bridge->panel_name, PANEL_NAME_SIZE);
	} else {
		global_panel_edp_name = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_edp_name)
			return -ENOMEM;

		global_panel_edp_mode = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_edp_mode)
			return -ENOMEM;

		strscpy(global_panel_edp_name, max_bridge->panel_name, PANEL_NAME_SIZE);
	}

	if (max_bridge->superframe_support) {
		/* parse superframe setting */
		if (max_bridge->is_dp)
			strscpy(global_panel_dp_mode, SUPERFRAME_SETTING, PANEL_NAME_SIZE);
		else
			strscpy(global_panel_edp_mode, SUPERFRAME_SETTING, PANEL_NAME_SIZE);
		ret = get_superframe_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
		else
			return 0;
	} else if (max_bridge->double_pixel_support) {
		/* parse dual link setting */
		ret = get_double_pixel_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
	} else if (max_bridge->is_support_mst) {
		/* parse mst setting */
		if (!max_bridge->is_dp)
			return -EINVAL;
		ret = get_mst_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
	} else {
		/* parse signal setting */
		if (max_bridge->is_dp)
			strscpy(global_panel_dp_mode, SINGAL_SETTING, PANEL_NAME_SIZE);
		else
			strscpy(global_panel_edp_mode, SINGAL_SETTING, PANEL_NAME_SIZE);
		ret = get_signal_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
	}

	of_node_put(panel_setting_np);
	return 0;
}

static int serdes_dp_dt_parse(struct serdes_dp_bridge *max_bridge, struct device *dev)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	get_general_info_from_dts(max_bridge);
	return 0;
}

static int turn_on_off_bl(struct serdes_dp_bridge *max_bridge, bool enable, u8 type)
{
	int ret = 0;
	struct i2c_client *client = NULL;

	if (max_bridge->superframe_support) {
		if (type == 0x1)
			client = max_bridge->max96752_linka_bl_i2c;
		else if (type == 0x2)
			client = max_bridge->max96752_linkb_bl_i2c;
	} else if (max_bridge->double_pixel_support) {
		client = max_bridge->bl_i2c;
	} else if (max_bridge->is_support_mst) {
		if (type == 0x1)
			client = max_bridge->max96752_linka_bl_i2c;
		else if (type == 0x2)
			client = max_bridge->max96752_linkb_bl_i2c;
	} else
		client = max_bridge->bl_i2c;

	pr_info("%s %s enable: %d\n", SERDES_DEBUG_INFO, __func__, enable);
	if (!client) {
		pr_info("%s No client found for BL\n", SERDES_DEBUG_INFO);
		return -ENODEV;
	}

	if (enable) {
		ret = i2c_master_send(client, max_bridge->bl_on_cmd.data, max_bridge->bl_on_cmd.length);
		if (ret < 0) {
			pr_info("%s Failed to send bl on cmd\n", SERDES_DEBUG_INFO);
			return ret;
		}
	} else {
		ret = i2c_master_send(client, max_bridge->bl_off_cmd.data, max_bridge->bl_off_cmd.length);
		if (ret < 0) {
			pr_info("%s Failed to send bl off cmd\n", SERDES_DEBUG_INFO);
			return ret;
		}
	}

	return 0;
}

static void serdes_init_by_signal(struct serdes_dp_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "[serdes_dp] %s+\n", __func__);
	/* ser init by default */
	ser_init_loop(max_bridge, &max_bridge->signal_ser_init_cmd);

	/* des init by default */
	des_init_loop(max_bridge, &max_bridge->signal_des_init_cmd);

	/* touch init by default */
	serdes_init_loop(max_bridge, &max_bridge->signal_touch_init_cmd);

	dev_info(max_bridge->dev, "[serdes_dp] %s-\n", __func__);
}

static void serdes_init_by_superframe(struct serdes_dp_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "[serdes_dp] %s+\n", __func__);
	/* serdes to init linka */
	serdes_init_loop(max_bridge, &max_bridge->serdes_init_linka_cmd);

	/*des to init linka */
	serdes_init_loop(max_bridge, &max_bridge->des_linka_init_cmd);

	/*serdes to init linkb */
	serdes_init_loop(max_bridge, &max_bridge->serdes_init_linkb_cmd);

	/* des to init linkb */
	serdes_init_loop(max_bridge, &max_bridge->des_linkb_init_cmd);

	/* ser to init superframe */
	ser_init_loop(max_bridge, &max_bridge->ser_superframe_init_cmd);

	/* serdes to init superframe touch */
	serdes_init_loop(max_bridge, &max_bridge->serdes_superframe_touch_init_cmd);

	dev_info(max_bridge->dev, "[serdes_dp] %s-\n", __func__);
}

static void serdes_init_by_double_pixel(struct serdes_dp_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "[serdes_dp] %s+\n", __func__);

	dev_info(max_bridge->dev, "[serdes_dp] %s-\n", __func__);
}

static void serdes_init_by_mst(struct serdes_dp_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "[serdes_dp] %s+\n", __func__);

	/* serdes to init linka */
	serdes_init_loop(max_bridge, &max_bridge->mst_serdes_init_linka_cmd);

	/*des to init linka */
	serdes_init_loop(max_bridge, &max_bridge->mst_des_linka_init_cmd);

	/*serdes to init linkb */
	serdes_init_loop(max_bridge, &max_bridge->mst_serdes_init_linkb_cmd);

	/* des to init linkb */
	serdes_init_loop(max_bridge, &max_bridge->mst_des_linkb_init_cmd);

	/* ser to init dp mst */
	ser_init_loop(max_bridge, &max_bridge->dp_mst_init_cmd);

	/* serdes to init dp mst touch */
	if (max_bridge->is_support_touch)
		serdes_init_loop(max_bridge, &max_bridge->dp_mst_touch_init_cmd);

	dev_info(max_bridge->dev, "[serdes_dp] %s-\n", __func__);
}

static int check_des_cable_status(struct i2c_client *des_client)
{
	u8 serdes_cable_state_change = 0x0;
	int ret = 0;

	serdes_cable_state_change = serdes_read_byte(des_client, DES_REG_0x06ff_HOTPLUG_DETECT);
	if (serdes_cable_state_change != DES_HOTPLUG_CHECK_VALUE) {
		ret = serdes_write_byte(des_client, DES_REG_0x06ff_HOTPLUG_DETECT,
								DES_HOTPLUG_CHECK_VALUE);
		if (ret)
			pr_info("%s %s: write hotplug check value failed %d\n", SERDES_DEBUG_INFO, __func__, ret);

		return 1;
	}

	return 0;
}

static void serdes_link_status_handler(struct serdes_dp_bridge *max_bridge)
{
	int ret = 0;
	u8 linka_status = 0x0, linkb_status = 0x0;

	linka_status = serdes_read_byte(max_bridge->serdes_dp_i2c, SER_REG_0x2A_LINKA_CTRL);
	linkb_status = serdes_read_byte(max_bridge->serdes_dp_i2c, SER_REG_0x34_LINKB_CTRL);
	pr_info("%s DP %d %s: linka_status:0x%x linkb_status:0x%x\n", SERDES_DEBUG_INFO,
			max_bridge->is_dp, __func__, linka_status, linkb_status);

	if (max_bridge->double_pixel_support) {
		if (linka_status & SER_CMSL_LINKA_LOCKED)
			ret = check_des_cable_status(max_bridge->max96752_linka_i2c);
		if (linkb_status & SER_CMSL_LINKB_LOCKED)
			ret = check_des_cable_status(max_bridge->max96752_linkb_i2c);
	} else if (max_bridge->superframe_support) {
		if (linka_status & SER_CMSL_LINKA_LOCKED) {
			ret = check_des_cable_status(max_bridge->max96752_linka_i2c);
			if (ret) {
				serdes_init_loop(max_bridge, &max_bridge->serdes_init_linka_cmd);
				serdes_init_loop(max_bridge, &max_bridge->des_linka_init_cmd);
				turn_on_off_bl(max_bridge, true, 0x01);
			}
		}
		if (linkb_status & SER_CMSL_LINKB_LOCKED) {
			ret = check_des_cable_status(max_bridge->max96752_linka_i2c);
			if (ret) {
				serdes_init_loop(max_bridge, &max_bridge->serdes_init_linkb_cmd);
				serdes_init_loop(max_bridge, &max_bridge->des_linkb_init_cmd);
				turn_on_off_bl(max_bridge, true, 0x02);
			}
		}
	} else if (max_bridge->is_support_mst) {
		if (linka_status & SER_CMSL_LINKA_LOCKED) {
			serdes_init_loop(max_bridge, &max_bridge->mst_des_linka_init_cmd);
			ret = check_des_cable_status(max_bridge->max96752_linka_i2c);
			if (ret) {
				serdes_init_loop(max_bridge, &max_bridge->mst_serdes_init_linka_cmd);
				turn_on_off_bl(max_bridge, true, 0x01);
			}
		}
		if (linkb_status & SER_CMSL_LINKB_LOCKED) {
			serdes_init_loop(max_bridge, &max_bridge->mst_des_linkb_init_cmd);
			ret = check_des_cable_status(max_bridge->max96752_linkb_i2c);
			if (ret) {
				serdes_init_loop(max_bridge, &max_bridge->mst_serdes_init_linkb_cmd);
				turn_on_off_bl(max_bridge, true, 0x02);
			}
		}
	} else {
		if ((linka_status & SER_CMSL_LINKA_LOCKED) ||
			(linkb_status & SER_CMSL_LINKB_LOCKED)) {
			ret = check_des_cable_status(max_bridge->max96752_i2c);
			if (ret) {
				msleep(500);
				des_init_loop(max_bridge, &max_bridge->signal_des_init_cmd);
				turn_on_off_bl(max_bridge, true, 0x0);
			}
		}
	}
}

static void serdes_init_by_dts(struct serdes_dp_bridge *max_bridge)
{
	if (max_bridge->inited) {
		dev_info(max_bridge->dev, "[serdes_dp] serdes already init!\n");
		return;
	}

	if (max_bridge->superframe_support)
		serdes_init_by_superframe(max_bridge);
	else if (max_bridge->double_pixel_support)
		serdes_init_by_double_pixel(max_bridge);
	else if (max_bridge->is_support_mst)
		serdes_init_by_mst(max_bridge);
	else
		serdes_init_by_signal(max_bridge);

	max_bridge->inited = true;
}

static int serdes_hotplug_kthread(void *data)
{
	struct serdes_dp_bridge *max_bridge = (struct serdes_dp_bridge *)data;

	/* Wait for hotplug event */
	while (!kthread_should_stop()) {
		if (max_bridge->irq_num > 0) {
			wait_event_interruptible(max_bridge->waitq, atomic_read(&max_bridge->hotplug_event));
			atomic_set(&max_bridge->hotplug_event, 0);
			disable_irq(max_bridge->irq_num);
		} else {
			/* polling mode for hotplug */
			wait_event_interruptible(max_bridge->waitq, atomic_read(&max_bridge->hotplug_event));
			msleep(SERDES_POLL_TIMEOUT_MS);
		}

		/* some deserializer device require buffering time to start */
		msleep(2000);
		serdes_link_status_handler(max_bridge);
		if (max_bridge->irq_num > 0)
			enable_irq(max_bridge->irq_num);
	}

	return 0;
}

static void serdes_init_wait_queue(struct serdes_dp_bridge *max_bridge)
{
	atomic_set(&max_bridge->hotplug_event, 0);
	init_waitqueue_head(&max_bridge->waitq);
	max_bridge->serdes_hotplug_task = kthread_run(serdes_hotplug_kthread,
									(void *)max_bridge, "serdes_dp");
}

static irqreturn_t serdes_interrupt_handler(int irq, void *data)
{
	struct serdes_dp_bridge *max_bridge = (struct serdes_dp_bridge *)data;

	atomic_set(&max_bridge->hotplug_event, 1);
	wake_up_interruptible(&max_bridge->waitq);
	return IRQ_HANDLED;
}

static int hotplug_by_interrupt(struct serdes_dp_bridge *max_bridge)
{
	int ret = 0;

	ret = request_irq(max_bridge->irq_num, serdes_interrupt_handler,
					IRQF_TRIGGER_RISING, "serdes_dp_irq", max_bridge);
	if (ret) {
		pr_info("%s %s: request irq failed %d\n", SERDES_DEBUG_INFO, __func__, ret);
		return ret;
	}

	return 0;
}

static int serdes_hotplug_handler(struct serdes_dp_bridge *max_bridge)
{
	int ret = 0;

	pr_info("%s %s+\n", SERDES_DEBUG_INFO, __func__);

	serdes_init_wait_queue(max_bridge);
	if (max_bridge->irq_num > 0) {
		pr_info("%s %s DP %d:irq mode\n", SERDES_DEBUG_INFO, __func__, max_bridge->is_dp);
		ret = hotplug_by_interrupt(max_bridge);
	} else
		pr_info("%s %s DP %d:polling mode\n", SERDES_DEBUG_INFO, __func__, max_bridge->is_dp);

	pr_info("%s %s-\n", SERDES_DEBUG_INFO, __func__);

	return ret;
}

static void serdes_dp_pre_enable(struct drm_bridge *bridge)
{
	struct serdes_dp_bridge *max_bridge = bridge_to_serdes_dp(bridge);
	int ret = 0;
	u8 dev_id = 0x0;

	pr_info("%s Serdes DP: %d %s+\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);

	if (max_bridge->prepared)
		return;

	dev_id = serdes_read_byte(max_bridge->serdes_dp_i2c, SER_ACTIVE_STATUS_CHECK_REG);
	if (dev_id != SER_ACTIVE_STATUS_VALUE) {
		ret = serdes_write_byte(max_bridge->serdes_dp_i2c, SER_ACTIVE_STATUS_CHECK_REG,
								SER_ACTIVE_STATUS_VALUE);
		if (ret)
			dev_info(max_bridge->dev, "%s Write ser status failed\n", SERDES_DEBUG_INFO);
		max_bridge->inited = false;
	}

	serdes_init_by_dts(max_bridge);

	max_bridge->prepared = true;
	max_bridge->suspend = false;
	pr_info("%s Serdes DP: %d %s-\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
}

static void serdes_dp_enable(struct drm_bridge *bridge)
{
	struct serdes_dp_bridge *max_bridge = bridge_to_serdes_dp(bridge);

	pr_info("%s Serdes DP: %d %s+\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);

	if (max_bridge->enabled)
		return;

	/* turn on backlight */
	if (max_bridge->superframe_support) {
		turn_on_off_bl(max_bridge, true, 0x01);
		turn_on_off_bl(max_bridge, true, 0x02);
	} else if (max_bridge->double_pixel_support) {
		turn_on_off_bl(max_bridge, true, 0x0);
	} else if (max_bridge->is_support_mst) {
		turn_on_off_bl(max_bridge, true, 0x01);
		turn_on_off_bl(max_bridge, true, 0x02);
	} else
		turn_on_off_bl(max_bridge, true, 0x0);

	if (max_bridge->is_support_hotplug) {
		if (max_bridge->irq_num <= 0) {
			atomic_set(&max_bridge->hotplug_event, 1);
			wake_up_interruptible(&max_bridge->waitq);
		}
	}

	max_bridge->enabled = true;
	pr_info("%s Serdes DP: %d %s-\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
}

static void serdes_dp_disable(struct drm_bridge *bridge)
{
	struct serdes_dp_bridge *max_bridge = bridge_to_serdes_dp(bridge);

	pr_info("%s Serdes DP: %d %s+\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
	if (max_bridge->suspend || !max_bridge->enabled)
		return;

	/* turn off backlight */
	if (max_bridge->superframe_support) {
		turn_on_off_bl(max_bridge, false, 0x01);
		turn_on_off_bl(max_bridge, false, 0x02);
	} else if (max_bridge->double_pixel_support) {
		turn_on_off_bl(max_bridge, false, 0x0);
	} else if (max_bridge->is_support_mst) {
		turn_on_off_bl(max_bridge, false, 0x01);
		turn_on_off_bl(max_bridge, false, 0x02);
	} else
		turn_on_off_bl(max_bridge, false, 0x0);

	if (max_bridge->is_support_hotplug) {
		if (max_bridge->irq_num <= 0) {
			atomic_set(&max_bridge->hotplug_event, 0);
			wake_up_interruptible(&max_bridge->waitq);
		}
	}

	max_bridge->enabled = false;
	max_bridge->prepared = false;
	pr_info("%s Serdes DP: %d %s-\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
}

static int serdes_dp_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	struct serdes_dp_bridge *max_bridge = bridge_to_serdes_dp(bridge);
	struct drm_bridge *panel_bridge = NULL;
	struct device *dev = NULL;
	u8 dev_id = 0x0;
	int ret = 0;

	pr_info("%s Serdes DP: %d %s+\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);

	dev = &max_bridge->serdes_dp_i2c->dev;
	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0, &max_bridge->panel1, NULL);
	if (!max_bridge->panel1) {
		pr_info("%s Failed to find panel %d\n", SERDES_DEBUG_INFO, ret);
		return -EINVAL;
	}

	dev_info(dev, "%s Found panel node: %pOF\n", SERDES_DEBUG_INFO, max_bridge->panel1->dev->of_node);

	panel_bridge = devm_drm_panel_bridge_add(dev, max_bridge->panel1);
	if (IS_ERR(panel_bridge)) {
		dev_info(dev, "[serdes_dp] Failed to create panel bridge\n");
		return PTR_ERR(panel_bridge);
	}
	max_bridge->panel_bridge = panel_bridge;

	if (max_bridge->is_support_mst) {
		ret = drm_of_find_panel_or_bridge(dev->of_node, 3, 0, &max_bridge->panel2, NULL);
		if (!max_bridge->panel2) {
			pr_info("%s Failed to find panel2 %d\n", SERDES_DEBUG_INFO, ret);
			return -EINVAL;
		}
		dev_info(dev, "%s Found panel2 node: %pOF\n", SERDES_DEBUG_INFO, max_bridge->panel2->dev->of_node);
	}

	ret = drm_bridge_attach(bridge->encoder, max_bridge->panel_bridge,
					bridge, flags | DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret < 0) {
		pr_info("%s Failed to attach panel_bridge: %d\n", SERDES_DEBUG_INFO, ret);
		return ret;
	}

	ret = get_panel_feature_info_from_dts(max_bridge);
	if (ret)
		pr_info("%s use default setting\n", SERDES_DEBUG_INFO);

	ret = get_serdes_setting_from_dts(max_bridge);
	if (ret) {
		pr_info("%s get serdes setting failed\n", SERDES_DEBUG_INFO);
		return ret;
	}

	ret = serdes_dp_create_i2c_client(max_bridge);
	if (ret) {
		dev_info(dev, "%s create i2c client failed\n", SERDES_DEBUG_INFO);
		return ret;
	}

	/* device identifier  0xC4: Without HDCP 0xC5: With HDCP */
	dev_id = serdes_read_byte(max_bridge->serdes_dp_i2c, DEVICE_IDENTIFIER_ADDR);
	pr_notice("%s Device Identifier = 0x%02x\n", SERDES_DEBUG_INFO, dev_id);

	/* use the unoccupied GPIO ID register to store the ser status */
	ret = serdes_write_byte(max_bridge->serdes_dp_i2c, SER_ACTIVE_STATUS_CHECK_REG, SER_ACTIVE_STATUS_VALUE);
	if (ret)
		dev_info(dev, "%s Write serdes failed\n", SERDES_DEBUG_INFO);
	else
		serdes_init_by_dts(max_bridge);

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		pr_info("%s Driver does not provide a connector\n", SERDES_DEBUG_INFO);
		return -EINVAL;
	}

	if (!bridge->encoder) {
		pr_info("%s Parent encoder object not found\n", SERDES_DEBUG_INFO);
		return -ENODEV;
	}

	/* SerDes irq or poll mode for hotplug */
	if (max_bridge->is_support_hotplug) {
		max_bridge->irq_num = irq_of_parse_and_map(dev->of_node, 0);
		if (max_bridge->irq_num <= 0)
			pr_info("%s %s: get irq failed, use polling mode\n", SERDES_DEBUG_INFO, __func__);

		serdes_hotplug_handler(max_bridge);
	}

	pr_info("%s Serdes DP: %d %s-\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);

	return 0;
}

static int serdes_dp_bridge_get_modes(struct drm_bridge *bridge,
			 struct drm_connector *connector)
{
	struct serdes_dp_bridge *max_bridge = bridge_to_serdes_dp(bridge);

	pr_info("%s %s\n", SERDES_DEBUG_INFO, __func__);

	if (max_bridge->is_support_mst) {
		if (connector->name)
			pr_info("%s get mst mode from %s panel\n", SERDES_DEBUG_INFO, connector->name);
		else {
			pr_info("%s connector name is null\n", SERDES_DEBUG_INFO);
			return -EINVAL;
		}

		if (!strncmp(connector->name, "DP-1", 4))
			return drm_panel_get_modes(max_bridge->panel1, connector);
		else if (!strncmp(connector->name, "DP-2", 4))
			return drm_panel_get_modes(max_bridge->panel2, connector);
		else
			return -EINVAL;
	}

	return drm_panel_get_modes(max_bridge->panel1, connector);
}

static const struct drm_bridge_funcs serdes_dp_bridge_funcs = {
	.pre_enable = serdes_dp_pre_enable,
	.enable = serdes_dp_enable,
	.disable = serdes_dp_disable,
	.attach = serdes_dp_bridge_attach,
	.get_modes = serdes_dp_bridge_get_modes,
};

static int serdes_dp_suspend(struct device *dev);
static int serdes_dp_resume(struct device *dev);

static int serdes_dp_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	struct serdes_dp_bridge *max_bridge = container_of(notifier, struct serdes_dp_bridge, nb);
	struct device *dev = max_bridge->dev;

	pr_info("%s %s pm_event %lu dev %s usage_count %d\n", SERDES_DEBUG_INFO,
	       __func__, pm_event, dev_name(dev), atomic_read(&dev->power.usage_count));

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		serdes_dp_suspend(dev);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		serdes_dp_resume(dev);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static int serdes_dp_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct serdes_dp_bridge *max_bridge = NULL;
	int ret;

	dev_info(dev, "%s %s+\n", SERDES_DEBUG_INFO, __func__);

	max_bridge = devm_kzalloc(dev, sizeof(*max_bridge), GFP_KERNEL);
	if (!max_bridge)
		return -ENOMEM;

	max_bridge->gpio_rst_n = devm_gpiod_get(dev, "reset",
							GPIOD_OUT_HIGH);
	if (IS_ERR(max_bridge->gpio_rst_n)) {
		ret = PTR_ERR(max_bridge->gpio_rst_n);
		dev_info(dev, "%s Cannot get reset gpio %d\n", SERDES_DEBUG_INFO, ret);
		return ret;
	}

	max_bridge->dev = dev;
	max_bridge->dev->driver_data = max_bridge;
	i2c_set_clientdata(client, max_bridge);
	max_bridge->client = client;
	max_bridge->serdes_dp_i2c = client;

	ret = serdes_dp_dt_parse(max_bridge, dev);
	if (ret) {
		dev_info(dev, "%s DT parse failed\n", SERDES_DEBUG_INFO);
		return ret;
	}

	if (max_bridge->is_dp)
		max_bridge->bridge.type = DRM_MODE_CONNECTOR_DisplayPort;
	else
		max_bridge->bridge.type = DRM_MODE_CONNECTOR_eDP;

	max_bridge->bridge.funcs = &serdes_dp_bridge_funcs;
	max_bridge->bridge.ops = DRM_BRIDGE_OP_MODES;
	max_bridge->bridge.of_node = dev->of_node;
	drm_bridge_add(&max_bridge->bridge);

	max_bridge->boot_from_lk = false;
	if (max_bridge->boot_from_lk) {
		max_bridge->inited = true;
		max_bridge->prepared = true;
		max_bridge->enabled = true;
	}

	/* register pm notifier */
	max_bridge->nb.notifier_call = serdes_dp_notifier;
	ret = register_pm_notifier(&max_bridge->nb);
	if (ret)
		pr_info("%s register_pm_notifier failed %d", SERDES_DEBUG_INFO, ret);

	pm_runtime_enable(max_bridge->dev);
	pm_runtime_get_sync(max_bridge->dev);
	dev_info(dev, "%s %s-\n", SERDES_DEBUG_INFO, __func__);
	return 0;
}

static void serdes_dp_remove(struct i2c_client *client)
{
	struct serdes_dp_bridge *max_bridge = i2c_get_clientdata(client);

	if (max_bridge->is_support_hotplug) {
		if (max_bridge->irq_num > 0)
			free_irq(max_bridge->irq_num, &client);
		kthread_stop(max_bridge->serdes_hotplug_task);
	}

	i2c_unregister_device(max_bridge->max96752_i2c);
	i2c_unregister_device(max_bridge->bl_i2c);
	if (max_bridge->double_pixel_support) {
		i2c_unregister_device(max_bridge->max96752_linka_i2c);
		i2c_unregister_device(max_bridge->max96752_linkb_i2c);
	}
	i2c_unregister_device(max_bridge->serdes_dp_i2c);

	drm_bridge_remove(&max_bridge->bridge);
}

#ifdef CONFIG_PM_SLEEP
static int serdes_dp_suspend(struct device *dev)
{
	struct serdes_dp_bridge *max_bridge = dev->driver_data;

	pr_info("%s Serdes DP: %d %s+\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
	if (max_bridge->suspend)
		return 0;

	gpiod_set_value(max_bridge->gpio_rst_n, 0);
	pm_runtime_put_sync(dev);
	max_bridge->suspend = true;
	max_bridge->inited = false;
	pr_info("%s Serdes DP: %d %s-\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);

	return 0;
}

static int serdes_dp_resume(struct device *dev)
{
	struct serdes_dp_bridge *max_bridge = dev->driver_data;

	pr_info("%s Serdes DP: %d %s+\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
	pm_runtime_get_sync(dev);
	gpiod_set_value(max_bridge->gpio_rst_n, 1);
	max_bridge->suspend = false;

	pr_info("%s Serdes DP: %d %s-\n", SERDES_DEBUG_INFO, max_bridge->is_dp, __func__);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(serdes_dp_pm_ops, serdes_dp_suspend, serdes_dp_resume);

static const struct i2c_device_id serdes_dp_edp_i2c_table[] = {
	{"serdes-dp", 0},
	{"serdes-edp", 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, serdes_dp_edp_i2c_table);

static const struct of_device_id serdes_dp_serdes_match[] = {
	{.compatible = "maxiam,serdes-dp"},
	{.compatible = "maxiam,max96851-dp"},
	{.compatible = "maxiam,max96851-edp"},
	{ },
};

MODULE_DEVICE_TABLE(of, serdes_dp_serdes_match);

static struct i2c_driver serdes_dp_edp_driver = {
	.id_table = serdes_dp_edp_i2c_table,
	.probe = serdes_dp_probe,
	.remove = serdes_dp_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "serdes-dp",
		.of_match_table = of_match_ptr(serdes_dp_serdes_match),
		.pm = &serdes_dp_pm_ops,
	},

};

module_i2c_driver(serdes_dp_edp_driver);

MODULE_AUTHOR("Jacky Hu <jie-h.hu@mediatek.com>");
MODULE_DESCRIPTION("MAXIAM96851 SerDes Driver");
MODULE_LICENSE("GPL");
