/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MAXIAM_SERDES_DP_H_
#define _MAXIAM_SERDES_DP_H_

#include <drm/drm_modes.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/pm_runtime.h>

#define SERDES_DEBUG_INFO					"[serdes-dp]"
#define SERDES_POLL_TIMEOUT_MS				2000

#define I2C_WRITE							0x1
#define PANEL_NAME_SIZE						64
#define DEVICE_IDENTIFIER_ADDR				0x0D
#define SER_ACTIVE_STATUS_VALUE				0xF
#define SER_ACTIVE_STATUS_CHECK_REG			0x2CE

#define PANEL_NAME							"panel-name"
#define USE_FOR_DP							"use-for-dp"
#define SERDES_SUPPORT_HOTPLUG				"ser-support-hotplug"
#define SERDES_SUPER_FRAME					"ser-super-frame"
#define SERDES_DOUBLE_PIXEL					"ser-double-pixel"
#define SUPPORT_MST							"ser-dp-mst"
#define SUPPORT_TOUCH						"support-touch"
#define USE_DEFAULT_SETTING					"use-default-setting"

#define PANEL_FEATURE						"feature"
#define PANEL_FEATURE_HANDLE				"feature-handle"
#define PANEL_FEATURE_HANDLE_NUM			"feature-handle-num"
#define FEATURE_CHECK_CMD					"check-cmd"
#define FEATURE_VERIFY_CMD					"verify-cmd"
#define FEATURE_HANDLE_CMD					"handle-cmd"
#define FEATURE_SETTING						"feature-setting"

#define SERDES_DP_SST_SETTING				"serdes-dp-sst-settting"
#define DES_I2C_ADDR						"des-i2c-addr"
#define BL_I2C_ADDR							"bl-i2c-addr"
#define SER_INIT_CMD_NAME					"ser-init-cmd"
#define DES_INIT_CMD_NAME					"des-init-cmd"
#define TOUCH_INIT_CMD_ADDR					"touch-init-cmd"

#define SERDES_DP_SUPERFRAME_SETTING		"serdes-dp-superframe-setting"
#define ASYMMETRIC_MULTI_VIEW				"asymmetric-multi-view"
#define SYSMMETRIC_MULTI_VIEW_SETTING		"symmetric-multi-view-setting"
#define ASYSMMETRIC_MULTI_VIEW_SETTING		"asymmetric-multi-view-setting"
#define DES_LINKA_I2C_ADDR					"des-linka-i2c-addr"
#define DES_LINKA_BL_I2C_ADDR				"des-linka-bl-i2c-addr"
#define DES_LINKB_I2C_ADDR					"des-linkb-i2c-addr"
#define DES_LINKB_BL_I2C_ADDR				"des-linkb-bl-i2c-addr"
#define SERDES_INIT_LINKA_CMD				"serdes-init-linka-cmd"
#define DES_LINKA_INIT_CMD					"des-linka-init-cmd"
#define SERDES_INIT_LINKB_CMD				"serdes-init-linkb-cmd"
#define DES_LINKB_INIT_CMD					"des-linkb-init-cmd"
#define SER_SUPERFRAME_INIT_CMD				"ser-superframe-init-cmd"
#define SERDES_SUPERFRAME_TOUCH_INIT_CMD	"serdes-superframe-touch-init-cmd"

#define SERDES_DP_DOUBLE_PIXEL_SETTING		"serdes-dp-double-pixel-setting"

#define SERDES_DP_MST_SETTING				"serdes-dp-mst-setting"
#define DP_MST_INIT_CMD						"ser-mst-init-cmd"
#define serdes_dp_DP_MST_TOUCH_I2C_ADDR		"dp-mst-touch-i2c-addr"
#define serdes_dp_DP_MST_TOUCH_INIT_CMD		"dp-mst-touch-init-cmd"

#define BL_ON_CMD							"bl-on-cmd"
#define BL_OFF_CMD							"bl-off-cmd"

#define SINGAL_SETTING						"signal-setting"
#define SUPERFRAME_SETTING					"superframe-setting"

#define SER_REG_0x2A_LINKA_CTRL				0x2A
#define SER_CMSL_LINKA_LOCKED				BIT(0)

#define SER_REG_0x34_LINKB_CTRL				0x34
#define SER_CMSL_LINKB_LOCKED				BIT(0)

#define DES_REG_0x06ff_HOTPLUG_DETECT		0x06FF
#define DES_HOTPLUG_CHECK_VALUE				0x22

enum serdes_status {
	des_link_status_connected = 1,
	des_link_status_disconnected,
	des_linka_status_connected,
	des_linka_status_disconnected,
	des_linkb_status_connected,
	des_linkb_status_disconnected,
	des_link_status_unknown,
};

struct serdes_cmd_info {
	u8 *obj;
	u16 *addr;
	u8 *data;
	u16 *delay_ms;
	u16 length;
};

struct bl_cmd_info {
	u16 length;
	u8 *data;
};

struct feature_cmd {
	u8 cmd_type;
	u16 i2c_addr;
	u8 cmd_length;
	u8 *data;
};

struct feature_info {
	u8 group_num;
	struct feature_cmd *feature_cmd;
};

struct serdes_dp_bridge {
	struct device *dev;
	struct drm_connector connector;
	struct i2c_client *client;
	struct drm_bridge bridge;
	struct drm_panel *panel1;
	struct drm_panel *panel2;
	struct drm_bridge *panel_bridge;
	struct gpio_desc *gpio_pd_n;
	struct gpio_desc *gpio_rst_n;
	struct notifier_block nb;

	/* irq handle for hotplug */
	bool serdes_init_done;
	bool is_support_hotplug;
	int irq_num;
	wait_queue_head_t waitq;
	struct task_struct *serdes_hotplug_task;
	atomic_t hotplug_event;

	/* Serdes i2c client */
	struct i2c_client *serdes_dp_i2c;
	struct i2c_client *max96752_i2c;
	struct i2c_client *bl_i2c;
	struct i2c_client *max96752_linka_i2c;
	struct i2c_client *max96752_linka_bl_i2c;
	struct i2c_client *max96752_linkb_i2c;
	struct i2c_client *max96752_linkb_bl_i2c;

	/* signal setting */
	struct serdes_cmd_info signal_ser_init_cmd;
	struct serdes_cmd_info signal_des_init_cmd;
	struct serdes_cmd_info signal_touch_init_cmd;
	struct bl_cmd_info bl_on_cmd;
	struct bl_cmd_info bl_off_cmd;

	/* superframe setting */
	struct serdes_cmd_info serdes_init_linka_cmd;
	struct serdes_cmd_info des_linka_init_cmd;
	struct serdes_cmd_info serdes_init_linkb_cmd;
	struct serdes_cmd_info des_linkb_init_cmd;
	struct serdes_cmd_info ser_superframe_init_cmd;
	struct serdes_cmd_info serdes_superframe_touch_init_cmd;

	/* dual link setting */

	/* dp mst setting */
	struct serdes_cmd_info mst_serdes_init_linka_cmd;
	struct serdes_cmd_info mst_des_linka_init_cmd;
	struct serdes_cmd_info mst_serdes_init_linkb_cmd;
	struct serdes_cmd_info mst_des_linkb_init_cmd;
	struct serdes_cmd_info dp_mst_init_cmd;
	struct serdes_cmd_info dp_mst_touch_init_cmd;

	/* spin lock */
	int serdes_enable_index;
	spinlock_t enable_index_lock;

	bool boot_from_lk;
	bool double_pixel_support;
	bool superframe_support;
	bool asymmetric_multi_view;
	bool is_support_mst;
	bool is_support_touch;
	bool prepared;
	bool enabled;
	bool suspend;
	bool is_dp;
	bool inited;

	char *panel_name;
	u32 des_i2c_addr;
	u32	bl_i2c_addr;
	u32 linka_i2c_addr;
	u32 linka_bl_addr;
	u32 linkb_i2c_addr;
	u32 linkb_bl_addr;
};

int get_panel_name_and_mode(char *panel_name, char *panel_mode, bool is_dp);

#endif
