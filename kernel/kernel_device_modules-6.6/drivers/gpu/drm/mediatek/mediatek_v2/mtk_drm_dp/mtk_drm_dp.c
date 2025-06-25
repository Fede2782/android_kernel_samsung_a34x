// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/bpf.h>
#include <linux/capability.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/extcon.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/linkage.h>
#include <linux/media-bus-format.h>
#include <linux/mfd/syscon.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/printk.h>
#include <linux/regmap.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/set_memory.h>
#include <linux/skbuff.h>
#include <linux/sockptr.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include <drm/display/drm_dp.h>
#include <drm/display/drm_dp_aux_bus.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_atomic_helper.h>

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_modes.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge_connector.h>

#include <sound/hdmi-codec.h>
#include <uapi/drm/mediatek_drm.h>
#include <video/videomode.h>
#include <../../../extcon/extcon.h>

#include "../mtk_drm_helper.h"
#include "mtk_drm_dp.h"
#include "mtk_drm_dp_reg.h"
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
#include "mtk_drm_dp_mst.h"
#include "mtk_drm_dp_mst_drv.h"
#endif

#define YUV422_PRIORITY		0x0
#define DEPTH_10BIT_PRIORITY		0x0

#define AUX_CMD_I2C_R			0x05
#define AUX_CMD_I2C_R_MOT0		0x01
#define AUX_CMD_I2C_W			0x04
#define AUX_CMD_I2C_W_MOT0		0x00
#define AUX_CMD_NATIVE_R		0x09
#define AUX_CMD_NATIVE_W		0x08
#define AUX_EDID_SEGMENT_ADDR	0x30
#define AUX_EDID_SLAVE_ADDR	0x50
#define AUX_MCCS_SLAVE_ADDR	0x37
#define AUX_NO_REPLY_WAIT_TIME          3200
#define AUX_WRITE_READ_WAIT_TIME        20 /* us */
#define AUX_WAIT_REPLY_LP_CNT_NUM		20000
#define CP2520_PATTERN2                 0x6
#define CP2520_PATTERN3                 0x7
#define DP_BITWIDTH_16    BIT(0)
#define DP_BITWIDTH_20    BIT(1)
#define DP_BITWIDTH_24    BIT(2)
#define DP_CAPABILITY_BITWIDTH_MASK             0x07
#define DP_CAPABILITY_BITWIDTH_SFT              16
#define DP_CAPABILITY_CHANNEL_MASK              0x7F
#define DP_CAPABILITY_CHANNEL_SFT               0
#define DP_CAPABILITY_SAMPLERATE_MASK           0x1F
#define DP_CAPABILITY_SAMPLERATE_SFT            8
#define DP_CHANNEL_2      BIT(0)
#define DP_CHANNEL_3      BIT(1)
#define DP_CHANNEL_4      BIT(2)
#define DP_CHANNEL_5      BIT(3)
#define DP_CHANNEL_6      BIT(4)
#define DP_CHANNEL_7      BIT(5)
#define DP_CHANNEL_8      BIT(6)
#define DP_COLOR_DEPTH_MASK 0x0000ff00
#define DP_COLOR_DEPTH_SFT 8
#define DP_COLOR_FORMAT_MASK 0x00ff0000
#define DP_COLOR_FORMAT_SFT 16

#define DP_SAMPLERATE_192 BIT(4)
#define DP_SAMPLERATE_32  BIT(0)
#define DP_SAMPLERATE_44  BIT(1)
#define DP_SAMPLERATE_48  BIT(2)
#define DP_SAMPLERATE_96  BIT(3)
#define DP_VIDEO_TIMING_MASK 0x000000ff
#define DP_VIDEO_TIMING_SFT 0
#define DP_AUTO_TEST_ENABLE		0x0
#define ENABLE_DP_EF_MODE		0x1
#if (ENABLE_DP_EF_MODE == 0x01)
#define DP_AUX_SET_ENAHNCED_FRAME	0x80
#else
#define DP_AUX_SET_ENAHNCED_FRAME	0x00
#endif
#define DP_CheckSinkCap_TimeOutCnt		0x3

#define DP_PHY_REG_COUNT              6
#define DP_SUPPORT_DSC                0 /* confirm DSC scenario before open this */
#define DP_TBC_BUF_ReadStartAdrThrd	0x08
#define DP_TBC_BUF_SIZE		DP_TBC_SELBUF_CASE
#define DP_TBC_SELBUF_CASE		2
#define DP_TRAIN_MAX_ITERATION	0x5
#define DP_TRAIN_RETRY_LIMIT		0x8
#define EDID_SIZE 0x200
#define ENABLE_DP_EF_MODE		0x1
#define ENABLE_DP_FIX_LRLC		0x0
#define ENABLE_DP_FIX_TPS2		0x0
#define ENABLE_DP_SSC_FORCEON		0
#define ENABLE_DP_SSC_OUTPUT		0
#define ENCODER_1_IRQ_MSK				BIT(3)
#define ENCODER_IRQ				BIT(0)
#define ENCODER_IRQ_MSK				BIT(0)
#define FAKE_DEFAULT_RES 0xFF

#define IEC_CH_STATUS_LEN 5
#define MTK_DP_SIP_CONTROL_AARCH32	MTK_SIP_SMC_CMD(0x523)
#define MTK_SIP_DP_CONTROL \
	(0x82000523 | 0x40000000)
#define PANEL_HEIGHT_MM_NODE_NAME "panel-mode-height_mm"
#define PANEL_HEIGHT_NODE_NAME "panel-mode-height"
#define PANEL_HFP_NODE_NAME "panel-mode-hfp"
#define PANEL_HSA_NODE_NAME "panel-mode-hsa"
#define PANEL_HBP_NODE_NAME "panel-mode-hbp"
#define PANEL_MODE_NODE_NAME "panel-mode-setting"
#define PANEL_VBP_NODE_NAME "panel-mode-vbp"
#define PANEL_VFP_NODE_NAME "panel-mode-vfp"
#define PANEL_VREFRESH_NODE_NAME "panel-mode-vrefresh"
#define PANEL_VSA_NODE_NAME "panel-mode-vsa"
#define PANEL_WIDTH_MM_NODE_NAME "panel-mode-width_mm"
#define PANEL_WIDTH_NODE_NAME "panel-mode-width"
#define PATTERN_80B			0x4
#define PATTERN_D10_2			0x1
#define PATTERN_HBR2_COM_EYE		0x5
#define PATTERN_NONE			0x0
#define PATTERN_PRBS7			0x3
#define PATTERN_SYMBOL_ERR		0x2
#define TRANS_IRQ				BIT(1)
#define TRANS_IRQ_MSK				BIT(1)
#define VS_VOTER_EN_LO 0x0
#define VS_VOTER_EN_LO_CLR 0x2
#define VS_VOTER_EN_LO_SET 0x1
#define DP_CTS_RETRAIN_TIMES_14		12
#define DP_CTS_RETRAIN_TIMES_DEFAULT	6
#define DP_LT_RETRY_LIMIT					0x8
#define DP_LT_MAX_LOOP						0x4
#define DP_LT_MAX_CR_LOOP					0x9
#define DP_LT_MAX_EQ_LOOP					0x6

enum aux_reply_cmd {
	AUX_REPLY_ACK = 0x00,
	AUX_DPCD_NACK = BIT(0),
	AUX_DPCD_DEFER = BIT(1),
	AUX_EDID_NACK = BIT(2),
	AUX_EDID_DEFER = BIT(3),
	AUX_HW_FAILED = BIT(4),
	AUX_INVALID_CMD = BIT(5),
};

enum dp_atf_cmd {
	DP_ATF_DUMP = 0x20,
	DP_ATF_VIDEO_UNMUTE,
	DP_ATF_CMD_COUNT
};

enum dp_disp_state {
	DP_DISP_STATE_NONE = 0,
	DP_DISP_STATE_RESUME = 1,
	DP_DISP_STATE_SUSPEND = 2,
	DP_DISP_STATE_SUSPENDING = 3,
};

enum dp_fec_error_count_type {
	FEC_ERROR_COUNT_DISABLE = 0x0,
	FEC_UNCORRECTED_BLOCK_ERROR_COUNT = 0x1,
	FEC_CORRECTED_BLOCK_ERROR_COUNT = 0x2,
	FEC_BIT_ERROR_COUNT = 0x3,
	FEC_PARITY_BLOCK_ERROR_COUNT = 0x4,
	FEC_PARITY_BIT_ERROR_COUNT = 0x5,
};

enum dp_lane_num {
	DP_LANE0 = 0x0,
	DP_LANE1 = 0x1,
	DP_LANE2 = 0x2,
	DP_LANE3 = 0x3,
	DP_LANE_MAX,
};

enum dp_lt_pattern {
	DP_0 = 0,
	DP_TPS1 = BIT(4),
	DP_TPS2 = BIT(5),
	DP_TPS3 = BIT(6),
	DP_TPS4 = BIT(7),
	DP_20_TPS1 = BIT(14),
	DP_20_TPS2 = BIT(15),
};

enum dp_notify_state {
	DP_NOTIFY_STATE_NO_DEVICE,
	DP_NOTIFY_STATE_ACTIVE,
};

enum dp_pg_location {
	DP_PG_LOCATION_NONE = 0x0,
	DP_PG_LOCATION_ALL = 0x1,
	DP_PG_LOCATION_TOP = 0x2,
	DP_PG_LOCATION_BOTTOM = 0x3,
	DP_PG_LOCATION_LEFT_OF_TOP = 0x4,
	DP_PG_LOCATION_LEFT_OF_BOTTOM = 0x5,
	DP_PG_LOCATION_LEFT = 0x6,
	DP_PG_LOCATION_RIGHT = 0x7,
	DP_PG_LOCATION_LEFT_OF_LEFT = 0x8,
	DP_PG_LOCATION_RIGHT_OF_LEFT = 0x9,
	DP_PG_LOCATION_LEFT_OF_RIGHT = 0xA,
	DP_PG_LOCATION_RIGHT_OF_RIGHT = 0xB,
	DP_PG_LOCATION_MAX,
};

enum dp_pg_pixel_mask {
	DP_PG_PIXEL_MASK_NONE = 0x0,
	DP_PG_PIXEL_ODD_MASK = 0x1,
	DP_PG_PIXEL_EVEN_MASK = 0x2,
	DP_PG_PIXEL_MASK_MAX,
};

enum dp_pg_purecolor {
	DP_PG_PURECOLOR_NONE = 0x0,
	DP_PG_PURECOLOR_BLUE = 0x1,
	DP_PG_PURECOLOR_GREEN = 0x2,
	DP_PG_PURECOLOR_RED = 0x3,
	DP_PG_PURECOLOR_MAX,
};

enum dp_pg_sel {
	DP_PG_20BIT = 0,
	DP_PG_80BIT = 1,
	DP_PG_11BIT = 2,
	DP_PG_8BIT = 3,
	DP_PG_PRBS7 = 4,
};

enum dp_power_status_type {
	DP_POWER_STATUS_NONE = 0,
	DP_POWER_STATUS_AC_ON,
	DP_POWER_STATUS_DC_ON,
	DP_POWER_STATUS_PS_ON,
	DP_POWER_STATUS_DC_OFF,
	DP_POWER_STATUS_POWER_SAVING,
};

enum dp_preemphasis_num {
	DP_PREEMPHASIS0 = 0x00,
	DP_PREEMPHASIS1 = 0x01,
	DP_PREEMPHASIS2 = 0x02,
	DP_PREEMPHASIS3 = 0x03,
};

enum dp_sdp_asp_hb3_auch {
	DP_SDP_ASP_HB3_AU02CH = 0x01,
	DP_SDP_ASP_HB3_AU08CH = 0x07,
};

enum dp_sdp_hb1_pkg_type {
	DP_SDP_HB1_PKG_RESERVE = 0x00,
	DP_SDP_HB1_PKG_AUDIO_TS = 0x01,
	DP_SDP_HB1_PKG_AUDIO = 0x02,
	DP_SDP_HB1_PKG_EXT = 0x04,
	DP_SDP_HB1_PKG_ACM = 0x05,
	DP_SDP_HB1_PKG_ISRC = 0x06,
	DP_SDP_HB1_PKG_VSC = 0x07,
	DP_SDP_HB1_PKG_CAMERA = 0x08,
	DP_SDP_HB1_PKG_PPS = 0x10,
	DP_SDP_HB1_PKG_EXT_VESA = 0x20,
	DP_SDP_HB1_PKG_EXT_CEA = 0x21,
	DP_SDP_HB1_PKG_NON_AINFO = 0x80,
	DP_SDP_HB1_PKG_VS_INFO = 0x81,
	DP_SDP_HB1_PKG_AVI_INFO = 0x82,
	DP_SDP_HB1_PKG_SPD_INFO = 0x83,
	DP_SDP_HB1_PKG_AINFO = 0x84,
	DP_SDP_HB1_PKG_MPG_INFO = 0x85,
	DP_SDP_HB1_PKG_NTSC_INFO = 0x86,
	DP_SDP_HB1_PKG_DRM_INFO = 0x87,
	DP_SDP_HB1_PKG_MAX_NUM
};

enum dp_sdp_pkg_type {
	DP_SDP_PKG_NONE = 0x00,
	DP_SDP_PKG_ACM = 0x01,
	DP_SDP_PKG_ISRC = 0x02,
	DP_SDP_PKG_AVI = 0x03,
	DP_SDP_PKG_AUI = 0x04,
	DP_SDP_PKG_SPD = 0x05,
	DP_SDP_PKG_MPEG = 0x06,
	DP_SDP_PKG_NTSC = 0x07,
	DP_SDP_PKG_VSP = 0x08,
	DP_SDP_PKG_VSC = 0x09,
	DP_SDP_PKG_EXT = 0x0A,
	DP_SDP_PKG_PPS0 = 0x0B,
	DP_SDP_PKG_PPS1 = 0x0C,
	DP_SDP_PKG_PPS2 = 0x0D,
	DP_SDP_PKG_PPS3 = 0x0E,
	DP_SDP_PKG_RESERVED = 0x0F,
	DP_SDP_PKG_DRM = 0x10,
	DP_SDP_PKG_ADS = 0x11,
	DP_SDP_PKG_MAX_NUM
};

enum dp_swing_num {
	DP_SWING0 = 0x00,
	DP_SWING1 = 0x01,
	DP_SWING2 = 0x02,
	DP_SWING3 = 0x03,
};

enum dp_train_stage {
	DP_LT_NONE			= 0x0000,
	DP_LT_CR_L0_FAIL	= 0x0008,
	DP_LT_CR_L1_FAIL	= 0x0009,
	DP_LT_CR_L2_FAIL	= 0x000A,
	DP_LT_EQ_L0_FAIL	= 0x0080,
	DP_LT_EQ_L1_FAIL	= 0x0090,
	DP_LT_EQ_L2_FAIL	= 0x00A0,
	DP_LT_PASS			= 0x7777,
};

enum dp_usb_pin_assign_type {
	DP_USB_PIN_ASSIGNMENT_C = 4,
	DP_USB_PIN_ASSIGNMENT_D = 8,
	DP_USB_PIN_ASSIGNMENT_E = 16,
	DP_USB_PIN_ASSIGNMENT_F = 32,
	DP_USB_PIN_ASSIGNMENT_MAX_NUM,
};

enum dp_version {
	DP_VER_11 = 0x11,
	DP_VER_12 = 0x12,
	DP_VER_14 = 0x14,
	DP_VER_12_14 = 0x16,
	DP_VER_14_14 = 0x17,
	DP_VER_MAX,
};

enum dp_video_mute {
	DP_VIDEO_UNMUTE = 1,
	DP_VIDEO_MUTE = 2,
};

union dp_rx_audio_chsts {
	struct{
		u8 rev : 1;
		u8 is_lpcm : 1;
		u8 copy_right : 1;
		u8 addition_format_info : 3;
		u8 channel_status_mode : 2;
		u8 category_code;
		u8 source_number : 4;
		u8 channel_number : 4;
		u8 sampling_freq : 4;
		u8 clock_accuary : 2;
		u8 rev2 : 2;
		u8 word_len : 4;
		u8 original_sampling_freq : 4;
	} audio_chsts;

	u8 audio_chsts_raw[IEC_CH_STATUS_LEN];
};

struct notify_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;

	ssize_t (*print_name)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_state)(struct notify_dev *sdev, char *buf);
};

struct drm_display_limit_mode {
	int hdisplay;
	int vdisplay;
	int vrefresh;
	int clock;
	int valid;
};

struct mtk_dp *g_mtk_dp;
static bool fake_cable_in;
static int fake_res = FAKE_DEFAULT_RES;
static atomic_t device_count;

#if DEPTH_10BIT_PRIORITY
static int fake_bpc = DP_COLOR_DEPTH_10BIT;
#else
static int fake_bpc = DP_COLOR_DEPTH_8BIT;
#endif

static const unsigned int dp_cable[] = {
	EXTCON_DISP_HDMI, /* audio framework not support DP */
	EXTCON_NONE,
};

static const u32 mt8678_input_fmts[] = {
#if	YUV422_PRIORITY
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
#else
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
#endif
};

static const u32 mt8678_output_fmts[] = {
#if	YUV422_PRIORITY
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
#else
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
#endif
};

struct extcon_dev *dp_extcon;
struct mtk_dp *mtk_dp_mst;
/* Mutex for synchronizing access to the DisplayPort resources */
struct mutex dp_lock;
struct notify_dev dp_notify_data;
struct class *switch_class;
unsigned int force_ch, force_fs, force_len;
atomic_t dp_comm_event = ATOMIC_INIT(0);
u8 pps_4k60[128] = {
	0x12, 0x00, 0x00, 0x8d, 0x30, 0x80, 0x08, 0x70, 0x0f, 0x00, 0x00, 0x08,
	0x07, 0x80, 0x07, 0x80,	0x02, 0x00, 0x04, 0xc0, 0x00, 0x20, 0x01, 0x1e,
	0x00, 0x1a, 0x00, 0x0c, 0x0d, 0xb7, 0x03, 0x94,	0x18, 0x00, 0x10, 0xf0,
	0x03, 0x0c, 0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e, 0x01, 0x02,
	0x01, 0x00, 0x09, 0x40,	0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
	0x1a, 0x38, 0x1a, 0x78, 0x22, 0xb6, 0x2a, 0xb6, 0x2a, 0xf6, 0x2a, 0xf4,
	0x43, 0x34, 0x63, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

u64 get_system_time(void)
{
	return ktime_get_mono_fast_ns();
}

u64 get_time_diff(u64 pre_time)
{
	u64 post_time = get_system_time();

	return (post_time - pre_time);
}

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

static int mtk_dp_create_switch_class(void)
{
	if (!switch_class) {
		switch_class = class_create("switch");
		if (IS_ERR(switch_class))
			return PTR_ERR(switch_class);
		atomic_set(&device_count, 0);
	}
	return 0;
}

int mtk_dp_uevent_dev_register(struct notify_dev *sdev)
{
	int ret;

	if (!switch_class) {
		ret = mtk_dp_create_switch_class();

		if (ret == 0) {
			DP_DBG("create switch class success\n");
		} else {
			DP_ERR("create switch class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(switch_class, NULL,
				  MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		DP_DBG("device create ok, index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		DP_ERR("device create fail, index:0x%x\n", sdev->index);
		ret = -1;
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0) {
		device_destroy(switch_class, MKDEV(0, sdev->index));
		DP_ERR("switch: Failed to register driver %s\n",
		       sdev->name);
	}

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		DP_ERR("switch: Failed to register driver %s\n",
		       sdev->name);
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	return ret;
}

int mtk_dp_notify_uevent_user(struct notify_dev *sdev, int state)
{
	int ret;
	char *envp[3];
	char name_buf[120];
	char state_buf[120];

	if (!sdev)
		return -1;

	if (sdev->state != state)
		sdev->state = state;

	ret = snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	if (ret < 0) {
		DP_ERR("%s, snprintf fail\n", __func__);
		return ret;
	}
	envp[0] = name_buf;
	ret = snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", sdev->state);
	if (ret < 0) {
		DP_ERR("%s, snprintf fail\n", __func__);
		return ret;
	}
	envp[1] = state_buf;
	envp[2] = NULL;
	DP_MSG("uevent name:%s, state:%s\n", envp[0], envp[1]);

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

unsigned long mtk_dp_atf_call(unsigned int cmd, unsigned int para)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct arm_smccc_res res;
	u32 x3 = (cmd << 16) | para;

	arm_smccc_smc(MTK_DP_SIP_CONTROL_AARCH32, cmd, para,
		      x3, 0xFEFD, 0, 0, 0, &res);

	DP_DBG("%s, cmd:0x%x, p1:0x%x, ret:0x%lx-0x%lx",
	       __func__, cmd, para, res.a0, res.a1);
	return res.a1;
#else
	return 0;
#endif
}

u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;

	if (offset > 0x8000) {
		DP_ERR("%s, error reg:0x%p, offset:0x%x\n",
		       __func__, mtk_dp->regs, offset);
		return 0;
	}

	read_val = readl(mtk_dp->regs + offset - (offset % 4))
			>> ((offset % 4) * 8);

	return read_val;
}

void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	if ((offset % 4 != 0) || offset > 0x8000) {
		DP_ERR("%s, error reg:0x%p, offset:0x%x, value:0x%x\n",
		       __func__, mtk_dp->regs, offset, val);
		return;
	}

	writel(val, mtk_dp->regs + offset);
}

void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = mtk_dp->regs + offset;
	u32 tmp;

	if ((offset % 4 != 0) || offset > 0x8000) {
		DP_ERR("%s, error reg:0x%p, offset:0x%x, value:0x%x\n",
		       __func__, mtk_dp->regs, offset, val);
		return;
	}

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

void mtk_dp_write_byte(struct mtk_dp *mtk_dp,
		       u32 addr, u8 val, u32 mask)
{
	if (addr % 2) {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x12);
		mtk_dp_mask(mtk_dp, addr - 1, (u32)(val << 8), (mask << 8));
	} else {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x11);
		mtk_dp_mask(mtk_dp, addr, (u32)val, mask);
	}

	mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x00);
}

u32 mtk_dp_phy_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;

	if (offset > 0x1500) {
		DP_ERR("%s, error offset:0x%x\n",
		       __func__, offset);
		return 0;
	}

	read_val = readl(mtk_dp->phyd_regs + offset - (offset % 4))
			>> ((offset % 4) * 8);

	return read_val;
}

void mtk_dp_phy_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	if ((offset % 4 != 0) || offset > 0x1500) {
		DP_ERR("%s, error offset:0x%x, value:0x%x\n",
		       __func__, offset, val);
		return;
	}

	writel(val, mtk_dp->phyd_regs + offset);
}

void mtk_dp_phy_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = mtk_dp->phyd_regs + offset;
	u32 tmp;

	if ((offset % 4 != 0) || offset > 0x1500) {
		DP_ERR("%s, error reg:0x%p, offset:0x%x, value:0x%x\n",
		       __func__, mtk_dp->phyd_regs, offset, val);
		return;
	}

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

void mtk_dp_phy_write_byte(struct mtk_dp *mtk_dp,
			   u32 addr, u8 val, u32 mask)
{
	if (addr % 2) {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x12);
		mtk_dp_phy_mask(mtk_dp, addr - 1, (u32)(val << 8), (mask << 8));
	} else {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x11);
		mtk_dp_phy_mask(mtk_dp, addr, (u32)val, mask);
	}

	mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x00);
}

u8 dp_aux_read_bytes(struct mtk_dp *mtk_dp, u8 cmd,
		     u64  dpcd_addr, size_t length, u8 *rx_buf)
{
	bool vaild_cmd = false;
	u8 phy_status = 0x00;
	u8 reply_cmd = 0xFF;
	u8 rd_count = 0x0;
	u8 aux_irq_status = 0;
	u8 ret = AUX_HW_FAILED;
	unsigned int wait_reply_count = AUX_WAIT_REPLY_LP_CNT_NUM;

	WRITE_BYTE(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);
	usleep_range(AUX_WRITE_READ_WAIT_TIME, AUX_WRITE_READ_WAIT_TIME + 1);

	if (length > 16 ||
	    (cmd == AUX_CMD_NATIVE_R && length == 0x0))
		return AUX_INVALID_CMD;

	WRITE_BYTE(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
	WRITE_BYTE(mtk_dp, REG_3644_AUX_TX_P0, cmd);
	WRITE_2BYTE(mtk_dp, REG_3648_AUX_TX_P0, dpcd_addr & 0x0000FFFF);
	WRITE_BYTE_MASK(mtk_dp, REG_364C_AUX_TX_P0,
			dpcd_addr >> 16,
				MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_FLDMASK);

	if (length > 0) {
		WRITE_2BYTE_MASK(mtk_dp, REG_3650_AUX_TX_P0,
				 (length - 1) << MCU_REQUEST_DATA_NUM_AUX_TX_P0_FLDMASK_POS,
			MCU_REQUEST_DATA_NUM_AUX_TX_P0_FLDMASK);
		WRITE_BYTE(mtk_dp, REG_362C_AUX_TX_P0, 0x00);
	}

	if (cmd == AUX_CMD_I2C_R || cmd == AUX_CMD_I2C_R_MOT0)
		if (length == 0x0)
			WRITE_2BYTE_MASK(mtk_dp, REG_362C_AUX_TX_P0,
					 0x01 << AUX_NO_LENGTH_AUX_TX_P0_FLDMASK_POS,
				AUX_NO_LENGTH_AUX_TX_P0_FLDMASK);

	WRITE_2BYTE_MASK(mtk_dp, REG_3630_AUX_TX_P0,
			 0x01 << AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK_POS,
		AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK);

	while (--wait_reply_count) {
		aux_irq_status = READ_BYTE(mtk_dp, REG_3640_AUX_TX_P0);
		aux_irq_status = aux_irq_status & 0x7F;
		if (aux_irq_status & AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK) {
			DP_DBG("[AUX] Read Complete irq\n");
			vaild_cmd = true;
			break;
		}

		if (aux_irq_status & AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK) {
			vaild_cmd = true;
			break;
		}

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK) {
			/* for no reply should wait at least 3200 us */
			usleep_range(AUX_NO_REPLY_WAIT_TIME, AUX_NO_REPLY_WAIT_TIME + 1);
			DP_DBG("(AUX Read)HW Timeout 400us irq");
			break;
		}
	}

	if (wait_reply_count == 0x0) {
		phy_status = READ_BYTE(mtk_dp, REG_3628_AUX_TX_P0);
		if (phy_status != 0x01)
			DP_ERR("Aux R:Aux hang, need SW reset\n");

		WRITE_2BYTE_MASK(mtk_dp, REG_3650_AUX_TX_P0,
				 0x01 << MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK_POS,
			MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK);
		WRITE_BYTE(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

		DP_MSG("wait_reply_count:%x, TimeOut", wait_reply_count);
		return AUX_HW_FAILED;
	}

	reply_cmd = READ_BYTE(mtk_dp, REG_3624_AUX_TX_P0) & 0x0F;
	if (reply_cmd)
		DP_MSG("reply_cmd:%x, NACK or Defer\n", reply_cmd);

	if (length == 0)
		WRITE_BYTE(mtk_dp, REG_362C_AUX_TX_P0, 0x00);

	if (reply_cmd == AUX_REPLY_ACK) {
		WRITE_2BYTE_MASK(mtk_dp, REG_3620_AUX_TX_P0,
				 0x0 << AUX_RD_MODE_AUX_TX_P0_FLDMASK_POS,
			AUX_RD_MODE_AUX_TX_P0_FLDMASK);

		for (rd_count = 0x0; rd_count < length;
				rd_count++) {
			WRITE_2BYTE_MASK(mtk_dp, REG_3620_AUX_TX_P0,
					 0x01 << AUX_RX_FIFO_READ_PULSE_AUX_TX_P0_FLDMASK_POS,
			AUX_RX_FIFO_READ_PULSE_AUX_TX_P0_FLDMASK);

			*(rx_buf + rd_count) = READ_BYTE(mtk_dp, REG_3620_AUX_TX_P0);
		}
	}

	WRITE_2BYTE_MASK(mtk_dp, REG_3650_AUX_TX_P0,
			 0x01 << MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK_POS,
		MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK);
	WRITE_BYTE(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

	if (vaild_cmd) {
		DP_DBG("[AUX] Read reply_cmd:%d\n", reply_cmd);
		ret = reply_cmd;
	} else {
		DP_DBG("[AUX] Timeout Read reply_cmd:%d\n", reply_cmd);
		ret = AUX_HW_FAILED;
	}

	return ret;
}

u8 dp_aux_write_bytes(struct mtk_dp *mtk_dp,
		      u8 cmd, u64  dpcd_addr, size_t length, u8 *data)
{
	bool vaild_cmd = false;
	u8 reply_cmd = 0x0;
	u8 aux_irq_status;
	u8 phy_status = 0x00;
	u8 i, ret = AUX_HW_FAILED;
	u16 wait_reply_count = AUX_WAIT_REPLY_LP_CNT_NUM;
	u8 reg_index;

	if (length > 16 || (cmd == AUX_CMD_NATIVE_W && length == 0x0))
		return AUX_INVALID_CMD;

	WRITE_BYTE_MASK(mtk_dp, REG_3704_AUX_TX_P0,
			1 << AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK_POS,
		AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK);
	WRITE_BYTE(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
	WRITE_BYTE(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);
	usleep_range(AUX_WRITE_READ_WAIT_TIME, AUX_WRITE_READ_WAIT_TIME + 1);

	WRITE_BYTE(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
	WRITE_BYTE(mtk_dp, REG_3644_AUX_TX_P0, cmd);
	WRITE_BYTE(mtk_dp, REG_3648_AUX_TX_P0, dpcd_addr & 0x00FF);
	WRITE_BYTE(mtk_dp, REG_3648_AUX_TX_P0 + 1, (dpcd_addr >> 8) & 0x00FF);
	WRITE_BYTE_MASK(mtk_dp, REG_364C_AUX_TX_P0,
			dpcd_addr >> 16,
				MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_FLDMASK);

	if (length > 0) {
		WRITE_BYTE(mtk_dp, REG_362C_AUX_TX_P0, 0x00);
		for (i = 0x0; i < (length + 1) / 2; i++)
			for (reg_index = 0; reg_index < 2; reg_index++)
				if ((i * 2 + reg_index) < length)
					WRITE_BYTE(mtk_dp, REG_3708_AUX_TX_P0 + i * 4 + reg_index,
						   data[i * 2 + reg_index]);
		WRITE_BYTE(mtk_dp, REG_3650_AUX_TX_P0 + 1, ((length - 1) & 0x0F) << 4);
	} else {
		WRITE_BYTE(mtk_dp, REG_362C_AUX_TX_P0, 0x01);
	}

	WRITE_BYTE_MASK(mtk_dp, REG_3704_AUX_TX_P0,
			AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK,
		AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK);
	WRITE_BYTE(mtk_dp, REG_3630_AUX_TX_P0, 0x08);

	while (--wait_reply_count) {
		aux_irq_status = READ_BYTE(mtk_dp, REG_3640_AUX_TX_P0) & 0xFF;
		usleep_range(1, 2);
		if (aux_irq_status & AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK) {
			DP_DBG("[AUX] Write Complete irq\n");
			vaild_cmd = true;
			break;
		}

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK) {
			/* for no reply should wait at least 3200 us */
			usleep_range(AUX_NO_REPLY_WAIT_TIME, AUX_NO_REPLY_WAIT_TIME + 1);
			DP_DBG("(AUX write)HW Timeout 400us irq");
			break;
		}
	}

	if (wait_reply_count == 0x0) {
		phy_status = READ_BYTE(mtk_dp, REG_3628_AUX_TX_P0);
		if (phy_status != 0x01)
			DP_ERR("Aux Write:Aux hang, need SW reset!\n");

		WRITE_BYTE(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
		WRITE_BYTE(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

		/* usleep_range(AUX_WRITE_READ_WAIT_TIME, AUX_WRITE_READ_WAIT_TIME + 1); */

		DP_MSG("reply_cmd:0x%x, wait_reply_count:%d\n",
		       reply_cmd, wait_reply_count);
		return AUX_HW_FAILED;
	}

	reply_cmd = READ_BYTE(mtk_dp, REG_3624_AUX_TX_P0) & 0x0F;
	if (reply_cmd)
		DP_MSG("reply_cmd:%x, NACK or Defer\n", reply_cmd);

	WRITE_BYTE(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);

	if (length == 0)
		WRITE_BYTE(mtk_dp, REG_362C_AUX_TX_P0, 0x00);

	WRITE_BYTE(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

	if (vaild_cmd) {
		DP_DBG("[AUX] Write reply_cmd:%d\n", reply_cmd);
		ret = reply_cmd;
	} else {
		DP_MSG("[AUX] Timeout, Write reply_cmd:%d\n", reply_cmd);
		ret = AUX_HW_FAILED;
	}

	return ret;
}

bool mtk_dp_aux_write_bytes(struct mtk_dp *mtk_dp, u8 cmd,
			    u32  dpcd_addr, size_t length, u8 *data)
{
	u8 reply_status = false;
	u8 retry_limit = 0x7;

	if (!mtk_dp->training_info.cable_plug_in ||
	    ((mtk_dp->training_info.phy_status & (HPD_DISCONNECT)) != 0x0)) {
		if (!mtk_dp->training_info.dp_tx_auto_test_en)
			mtk_dp->training_state = DP_TRAINING_STATE_CHECKCAP;
		return false;
	}

	do {
		reply_status = dp_aux_write_bytes(mtk_dp, cmd,
						  dpcd_addr, length, data);
		retry_limit--;
		if (reply_status) {
			usleep_range(50, 51);
			DP_DBG("Retry Num:%d\n", retry_limit);
		} else {
			return true;
		}
	} while (retry_limit > 0);

	DP_ERR("Aux Write Fail, cmd:%d, addr:0x%x, len:%lu\n",
	       cmd, dpcd_addr, length);

	return false;
}

bool mtk_dp_aux_write_dpcd(struct mtk_dp *mtk_dp, u8 cmd,
			   u32 dpcd_addr, size_t length, u8 *data)
{
	bool ret = true;
	size_t times = 0;
	size_t remain = 0;
	size_t loop = 0;

	if (length > DP_AUX_MAX_PAYLOAD_BYTES) {
		times = length / DP_AUX_MAX_PAYLOAD_BYTES;
		remain = length % DP_AUX_MAX_PAYLOAD_BYTES;

		for (loop = 0; loop < times; loop++)
			ret &= mtk_dp_aux_write_bytes(mtk_dp,
				cmd,
				dpcd_addr + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				DP_AUX_MAX_PAYLOAD_BYTES,
				data + (loop * DP_AUX_MAX_PAYLOAD_BYTES));

		if (remain > 0)
			ret &= mtk_dp_aux_write_bytes(mtk_dp,
				cmd,
				dpcd_addr + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				remain,
				data + (times * DP_AUX_MAX_PAYLOAD_BYTES));
	} else {
		ret &= mtk_dp_aux_write_bytes(mtk_dp,
				cmd,
				dpcd_addr,
				length,
				data);
	}

	DP_DBG("Aux write cmd:%d, addr:0x%x, len:%lu, %s\n",
	       cmd, dpcd_addr, length, ret ? "Success" : "Fail");
	for (loop = 0; loop < length; loop++)
		DP_DBG("DPCD%lx:0x%x", dpcd_addr + loop, data[loop]);

	return ret;
}

bool mtk_dp_aux_read_bytes(struct mtk_dp *mtk_dp, u8 cmd,
			   u32 dpcd_addr, size_t length, u8 *data)
{
	u8 reply_status = false;
	u8 retry_limit = 7;

	if (!mtk_dp->training_info.cable_plug_in ||
	    ((mtk_dp->training_info.phy_status & (HPD_DISCONNECT)) != 0x0)) {
		if (!mtk_dp->training_info.dp_tx_auto_test_en)
			mtk_dp->training_state = DP_TRAINING_STATE_CHECKCAP;
		return false;
	}

	do {
		reply_status = dp_aux_read_bytes(mtk_dp, cmd,
						 dpcd_addr, length, data);
		if (reply_status) {
			usleep_range(50, 51);
			DP_DBG("Retry Num:%d\n", retry_limit);
		} else {
			return true;
		}

		retry_limit--;
	} while (retry_limit > 0);

	DP_ERR("Aux Read Fail, cmd:%d, addr:0x%x, len:%lu\n",
	       cmd, dpcd_addr, length);

	return false;
}

bool mtk_dp_aux_read_dpcd(struct mtk_dp *mtk_dp, u8 cmd,
			  u32 dpcd_addr, size_t length, u8 *rx_buf)
{
	bool ret = true;
	size_t times = 0;
	size_t remain = 0;
	size_t loop = 0;

	memset(rx_buf, 0, length);

	if (length > DP_AUX_MAX_PAYLOAD_BYTES) {
		times = length / DP_AUX_MAX_PAYLOAD_BYTES;
		remain = length % DP_AUX_MAX_PAYLOAD_BYTES;

		for (loop = 0; loop < times; loop++)
			ret &= mtk_dp_aux_read_bytes(mtk_dp,
				cmd,
				dpcd_addr + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				DP_AUX_MAX_PAYLOAD_BYTES,
				rx_buf + (loop * DP_AUX_MAX_PAYLOAD_BYTES));

		if (remain > 0)
			ret &= mtk_dp_aux_read_bytes(mtk_dp,
				cmd,
				dpcd_addr + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				remain,
				rx_buf + (times * DP_AUX_MAX_PAYLOAD_BYTES));
	} else {
		ret &= mtk_dp_aux_read_bytes(mtk_dp,
				cmd,
				dpcd_addr,
				length,
				rx_buf);
	}

	DP_DBG("Aux Read cmd:%d, addr:0x%x, len:%lu, %s\n",
	       cmd, dpcd_addr, length, ret ? "Success" : "Fail");
	for (loop = 0; loop < length; loop++)
		DP_DBG("DPCD%lx:0x%x", dpcd_addr + loop, rx_buf[loop]);

	return ret;
}

static ssize_t mtk_dp_aux_transfer(struct drm_dp_aux *mtk_aux,
				   struct drm_dp_aux_msg *msg)
{
	u8 cmd;
	void *data;
	size_t length, ret = 0;
	u32 addr;
	bool mot, ack = false;
	struct mtk_dp *mtk_dp;

	mtk_dp = container_of(mtk_aux, struct mtk_dp, aux);
	mot = (msg->request & DP_AUX_I2C_MOT) ? true : false;
	cmd = msg->request;
	addr = msg->address;
	length = msg->size;
	data = msg->buffer;

	if (mtk_dp->disp_state == DP_DISP_STATE_SUSPENDING ||
		mtk_dp->disp_state == DP_DISP_STATE_SUSPEND)
		return -EAGAIN;

	switch (cmd) {
	case DP_AUX_I2C_MOT:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE | DP_AUX_I2C_MOT:
		cmd &= ~DP_AUX_I2C_WRITE_STATUS_UPDATE;
		ack = mtk_dp_aux_write_dpcd(mtk_dp, cmd,
					    addr, length, data);
		break;

	case DP_AUX_I2C_READ:
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ | DP_AUX_I2C_MOT:
		ack = mtk_dp_aux_read_dpcd(mtk_dp, cmd,
					   addr, length, data);
		break;

	default:
		DP_ERR("invalid aux cmd:%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (ack) {
		msg->reply = DP_AUX_NATIVE_REPLY_ACK | DP_AUX_I2C_REPLY_ACK;
		ret = length;
	} else {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		ret = -EAGAIN;
	}

	return ret;
}

static void mtk_dp_aux_swap(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable) {
		WRITE_2BYTE_MASK(mtk_dp, REG_360C_AUX_TX_P0,
				 1 << AUX_SWAP_AUX_TX_P0_FLDMASK_POS,
			AUX_SWAP_AUX_TX_P0_FLDMASK);
		WRITE_BYTE_MASK(mtk_dp, REG_3680_AUX_TX_P0,
				1 << AUX_SWAP_TX_AUX_TX_P0_FLDMASK_POS,
			AUX_SWAP_TX_AUX_TX_P0_FLDMASK);
	} else {
		WRITE_2BYTE_MASK(mtk_dp, REG_360C_AUX_TX_P0, 0,
				 AUX_SWAP_AUX_TX_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_3680_AUX_TX_P0, 0,
				 AUX_SWAP_TX_AUX_TX_P0_FLDMASK);
	}
}

void mtk_dp_aux_setting(struct mtk_dp *mtk_dp)
{
	mtk_dp_aux_swap(mtk_dp, mtk_dp->swap_enable);

	/* modify timeout threshold = 1595 [12 : 8] */
	WRITE_2BYTE_MASK(mtk_dp, REG_360C_AUX_TX_P0, 0x1D0C, AUX_TIMEOUT_THR_AUX_TX_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, REG_3658_AUX_TX_P0, 0, BIT(0));

	WRITE_2BYTE(mtk_dp, REG_36A0_AUX_TX_P0, 0xFFFC);

	/* 26M */
	WRITE_2BYTE_MASK(mtk_dp, REG_3634_AUX_TX_P0,
			 0x19 << AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK_POS,
			AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, REG_3614_AUX_TX_P0,
			0x0D << AUX_RX_UI_CNT_THR_AUX_TX_P0_FLDMASK_POS,
			AUX_RX_UI_CNT_THR_AUX_TX_P0_FLDMASK);

	WRITE_4BYTE_MASK(mtk_dp, REG_37C8_AUX_TX_P0, MTK_ATOP_EN_AUX_TX_P0_FLDMASK,
			 MTK_ATOP_EN_AUX_TX_P0_FLDMASK);
	/* disable aux sync_stop detect function */
	WRITE_4BYTE_MASK(mtk_dp, REG_3690_AUX_TX_P0,
			 0x1 << RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK_POS,
			RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK);

	/* Con Thd = 1.5ms+Vx0.1ms */
	WRITE_4BYTE_MASK(mtk_dp, REG_367C_AUX_TX_P0,
			 5 << HPD_CONN_THD_AUX_TX_P0_FLDMASK_POS,
		HPD_CONN_THD_AUX_TX_P0_FLDMASK);
	/* DisCon Thd = 1.5ms+Vx0.1ms */
	WRITE_4BYTE_MASK(mtk_dp, REG_37A0_AUX_TX_P0,
			 5 << HPD_DISC_THD_AUX_TX_P0_FLDMASK_POS,
		HPD_DISC_THD_AUX_TX_P0_FLDMASK);

	WRITE_4BYTE_MASK(mtk_dp, REG_3690_AUX_TX_P0,
			 RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK,
		RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK);
}

static void mtk_dp_aux_init(struct mtk_dp *mtk_dp)
{
	drm_dp_aux_init(&mtk_dp->aux);
	DP_DBG("aux hw_mutex:0x%lx\n", (unsigned long)&mtk_dp->aux.hw_mutex);

	mtk_dp->aux.name = kasprintf(GFP_KERNEL, "DPDDC-MTK");
	mtk_dp->aux.transfer = mtk_dp_aux_transfer;
}

void mtk_dp_fec_init_setting(struct mtk_dp *mtk_dp)
{
	WRITE_4BYTE_MASK(mtk_dp, REG_3540_DP_TRANS_P0,
			 1 << FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK_POS,
				FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK);
	WRITE_4BYTE_MASK(mtk_dp, REG_3540_DP_TRANS_P0,
			 2 << FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK_POS,
				FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK);
}

void mtk_dp_fec_enable(struct mtk_dp *mtk_dp, bool enable)
{
	DP_FUNC("FEC enable:%d\n", enable);

	if (enable)
		WRITE_BYTE_MASK(mtk_dp, REG_3540_DP_TRANS_P0, BIT(0), BIT(0));
	else
		WRITE_BYTE_MASK(mtk_dp, REG_3540_DP_TRANS_P0, 0, BIT(0));
}

void mtk_dp_fec_ready(struct mtk_dp *mtk_dp, u8 err_cnt_sel)
{
	u8 data[3] = {0};

	drm_dp_dpcd_read(&mtk_dp->aux, 0x90, data, 0x1);

	/* FEC error count select 120[3:1]:         *
	 * 000b: FEC_ERROR_COUNT_DIS                *
	 * 001b: UNCORRECTED_BLOCK_ERROR_COUNT      *
	 * 010b: CORRECTED_BLOCK_ERROR_COUNT        *
	 * 011b: BIT_ERROR_COUNT                    *
	 * 100b: PARITY_BLOCK_ERROR_COUNT           *
	 * 101b: PARITY_BIT_ERROR_COUNT             *
	 */
	if (data[0] & BIT(0)) {
		mtk_dp->has_fec   = true;
		data[0] = (err_cnt_sel << 1) | 0x1;     /* FEC Ready */
		drm_dp_dpcd_write(&mtk_dp->aux, 0x120, data, 0x1);
		drm_dp_dpcd_read(&mtk_dp->aux, 0x280, data, 0x3);
		DP_MSG("FEC status & error Count:0x%x, 0x%x, 0x%x\n",
		       data[0], data[1], data[2]);
	}

	DP_MSG("SINK has_fec:%d\n", mtk_dp->has_fec);
}

void mtk_dp_msa_enable_bypass(struct mtk_dp *mtk_dp,
			      const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable)
		WRITE_2BYTE_MASK(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset, 0, 0x03FF);
	else
		WRITE_2BYTE_MASK(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset, 0x03FF, 0x03FF);
}

void mtk_dp_msa_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	struct dp_timing_parameter *dp_timing = &mtk_dp->info[encoder_id].dp_output_timing;

	WRITE_2BYTE(mtk_dp, REG_3010_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->htt);
	WRITE_2BYTE(mtk_dp, REG_3018_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->hsw + dp_timing->hbp);
	WRITE_2BYTE_MASK(mtk_dp, REG_3028_DP_ENCODER0_P0 + reg_offset,
			 dp_timing->hsw << HSW_SW_DP_ENCODER0_P0_FLDMASK_POS,
		HSW_SW_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3028_DP_ENCODER0_P0 + reg_offset,
			 dp_timing->hsp << HSP_SW_DP_ENCODER0_P0_FLDMASK_POS,
		HSP_SW_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE(mtk_dp, REG_3020_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->hde);
	WRITE_2BYTE(mtk_dp, REG_3014_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vtt);
	WRITE_2BYTE(mtk_dp, REG_301C_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vsw + dp_timing->vbp);
	WRITE_2BYTE_MASK(mtk_dp, REG_302C_DP_ENCODER0_P0 + reg_offset,
			 dp_timing->vsw << VSW_SW_DP_ENCODER0_P0_FLDMASK_POS,
		VSW_SW_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_302C_DP_ENCODER0_P0 + reg_offset,
			 dp_timing->vsp << VSP_SW_DP_ENCODER0_P0_FLDMASK_POS,
		VSP_SW_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE(mtk_dp, REG_3024_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vde);
	if (!mtk_dp->dsc_enable)
		WRITE_2BYTE(mtk_dp, REG_3064_DP_ENCODER0_P0 + reg_offset,
			    dp_timing->hde);
	WRITE_2BYTE(mtk_dp, REG_3154_DP_ENCODER0_P0 + reg_offset,
		    (dp_timing->htt));
	WRITE_2BYTE(mtk_dp, REG_3158_DP_ENCODER0_P0 + reg_offset,
		    (dp_timing->hfp));
	WRITE_2BYTE(mtk_dp, REG_315C_DP_ENCODER0_P0 + reg_offset,
		    (dp_timing->hsw));
	WRITE_2BYTE(mtk_dp, REG_3160_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->hbp + dp_timing->hsw);
	WRITE_2BYTE(mtk_dp, REG_3164_DP_ENCODER0_P0 + reg_offset,
		    (dp_timing->hde));
	WRITE_2BYTE(mtk_dp, REG_3168_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vtt);
	WRITE_2BYTE(mtk_dp, REG_316C_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vfp);
	WRITE_2BYTE(mtk_dp, REG_3170_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vsw);
	WRITE_2BYTE(mtk_dp, REG_3174_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vbp + dp_timing->vsw);
	WRITE_2BYTE(mtk_dp, REG_3178_DP_ENCODER0_P0 + reg_offset,
		    dp_timing->vde);

	DP_MSG("set MSA, Htt:%d, Vtt:%d, Hact:%d, Vact:%d, fps:%d\n",
	       dp_timing->htt, dp_timing->vtt,
			dp_timing->hde, dp_timing->vde, dp_timing->frame_rate);
}

bool mtk_dp_mn_overwrite(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			 bool enable, u64 video_m, u64 video_n)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable) {
		/* Turn-on overwrite MN */
		WRITE_2BYTE(mtk_dp, REG_3008_DP_ENCODER0_P0 + reg_offset,
			    video_m & 0xFFFF);
		WRITE_BYTE(mtk_dp, REG_300C_DP_ENCODER0_P0 + reg_offset,
			   ((video_m >> 16) & 0xFF));
		WRITE_2BYTE(mtk_dp, REG_3044_DP_ENCODER0_P0 + reg_offset,
			    video_n & 0xFFFF);
		WRITE_BYTE(mtk_dp, REG_3048_DP_ENCODER0_P0 + reg_offset,
			   (video_n >> 16) & 0xFF);
		WRITE_2BYTE(mtk_dp, REG_3050_DP_ENCODER0_P0 + reg_offset,
			    video_n & 0xFFFF);
		WRITE_BYTE(mtk_dp, REG_3054_DP_ENCODER0_P0 + reg_offset,
			   (video_n >> 16) & 0xFF);
		WRITE_BYTE_MASK(mtk_dp, REG_3004_DP_ENCODER0_P0 + 1 + reg_offset,
				BIT(0), BIT(0));
	} else {
		/* Turn-off overwrite MN */
		WRITE_BYTE_MASK(mtk_dp, REG_3004_DP_ENCODER0_P0 + 1 + reg_offset, 0, BIT(0));
	}

	return true;
}

void mtk_dp_mn_calculate(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	int target_frame_rate = 60;
	int target_pixel_clk = 148500000; /* default set FHD */

	if (mtk_dp->info[encoder_id].dp_output_timing.frame_rate > 0) {
		target_frame_rate = mtk_dp->info[encoder_id].dp_output_timing.frame_rate;
		target_pixel_clk = (int)mtk_dp->info[encoder_id].dp_output_timing.htt *
			(int)mtk_dp->info[encoder_id].dp_output_timing.vtt * target_frame_rate;
	} else if (mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate > 0) {
		target_pixel_clk = mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate * 1000;
	} else {
		target_pixel_clk = (int)mtk_dp->info[encoder_id].dp_output_timing.htt *
			(int)mtk_dp->info[encoder_id].dp_output_timing.vtt * target_frame_rate;
	}

	if (target_pixel_clk > 0)
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate = target_pixel_clk / 1000;
}

void mtk_dp_mvid_renew(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	u32 mvid, htt;

	htt = mtk_dp->info[encoder_id].dp_output_timing.htt;
	if (htt % 4 != 0) {
		mvid = READ_4BYTE(mtk_dp, REG_33C8_DP_ENCODER1_P0 + reg_offset);
		DP_MSG("Encoder:%d, Odd Htt:%d, Mvid:%d, overwrite\n",
		       encoder_id, htt, mvid);
		if (mtk_dp->info[encoder_id].input_src == DP_SRC_PG)
			mvid = mvid * htt / (htt - 2);
		mtk_dp_mn_overwrite(mtk_dp, encoder_id, true, mvid, 0x8000);
	}
}

void mtk_dp_mvid_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable)
		WRITE_BYTE_MASK(mtk_dp, REG_300C_DP_ENCODER0_P0 + 1 + reg_offset,
				BIT(4), BIT(6) | BIT(5) | BIT(4));
	else
		WRITE_BYTE_MASK(mtk_dp, REG_300C_DP_ENCODER0_P0 + 1 + reg_offset,
				0, BIT(6) | BIT(5) | BIT(4));
}

u8 mtk_dp_color_get_bpp(u8 color_format, u8 color_depth)
{
	u8 color_bpp;

	switch (color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 16;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 12;
		else
			color_bpp = 18;
		break;
	case DP_COLOR_DEPTH_8BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 16;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 12;
		else
			color_bpp = 24;
		break;
	case DP_COLOR_DEPTH_10BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 20;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 15;
		else
			color_bpp = 30;
		break;
	case DP_COLOR_DEPTH_12BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 24;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 18;
		else
			color_bpp = 36;
		break;
	case DP_COLOR_DEPTH_16BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 32;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 24;
		else
			color_bpp = 48;
		break;
	default:
		color_bpp = 24;
		DP_MSG("Set Wrong Bpp:%d\n", color_bpp);
		break;
	}

	return color_bpp;
}

void mtk_dp_color_set_format(struct mtk_dp *mtk_dp,
			     const enum dp_encoder_id encoder_id, u8 color_format)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	DP_MSG("Set Color Format:0x%x\n", color_format);

	if (color_format == DP_PIXELFORMAT_RGB ||
	    color_format == DP_PIXELFORMAT_YUV444)
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				(0), GENMASK(6, 4));
	else if (color_format == DP_PIXELFORMAT_YUV422)
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				(BIT(4)), GENMASK(6, 4));
	else if (color_format == DP_PIXELFORMAT_YUV420)
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				(BIT(5)), GENMASK(6, 4));
}

void mtk_dp_color_set_depth(struct mtk_dp *mtk_dp,
			    const enum dp_encoder_id encoder_id, u8 color_depth)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	DP_MSG("Set Color Depth:%d (0~4=6/8/10/12/16 bpp)\n", color_depth);

	switch (color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset, 4, 0x07);
		break;
	case DP_COLOR_DEPTH_8BIT:
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset, 3, 0x07);
		break;
	case DP_COLOR_DEPTH_10BIT:
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset, 2, 0x07);
		break;
	case DP_COLOR_DEPTH_12BIT:
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset, 1, 0x07);
		break;
	case DP_COLOR_DEPTH_16BIT:
		WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset, 0, 0x07);
		break;
	default:
		break;
	}
}

void mtk_dp_pg_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable)
		WRITE_BYTE_MASK(mtk_dp,
				REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	else
		WRITE_BYTE_MASK(mtk_dp,
				REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, 0, BIT(3));
}

void mtk_dp_pg_pure_color(struct mtk_dp *mtk_dp,
			  const enum dp_encoder_id encoder_id, u8 rgb, u32 color_depth)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* video select hw or pattern gen 0:HW 1:PG */
	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	/* reg_pattern_sel */
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset, 0, GENMASK(6, 4));

	switch (rgb) {
	case DP_PG_PURECOLOR_BLUE:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		break;
	case DP_PG_PURECOLOR_GREEN:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		break;
	case DP_PG_PURECOLOR_RED:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		break;
	default:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		break;
	}
}

void mtk_dp_pg_vertical_ramping(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				u8 rgb, u32 color_depth, u8 location)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			1 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			1 << PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			rgb << PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK);

	switch (location) {
	case DP_PG_LOCATION_ALL:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));

		WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset,
			    0x3FFF);
		break;
	case DP_PG_LOCATION_TOP:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset, 0x40);
		break;

	case DP_PG_LOCATION_BOTTOM:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset, 0x2FFF);
		break;
	default:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 color_depth, GENMASK(11, 0));
		WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset, 0x3FFF);
		break;
	}
}

void mtk_dp_pg_horizontal_ramping(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				  u8 rgb, u32 color_depth, u8 location)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	u64 ramp = 0x3FFF;

	/* video select hw or pattern gen 0:HW 1:PG */
	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			2 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			1 << PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			rgb << PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK);

	WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset, ramp);

	switch (location) {
	case DP_PG_LOCATION_ALL:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		break;
	case DP_PG_LOCATION_LEFT_OF_TOP:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset, 0x3FFF);
		break;
	case DP_PG_LOCATION_LEFT_OF_BOTTOM:
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset, 0x3FFF);
		break;
	default:
		break;
	}
}

void mtk_dp_pg_vertical_color_bar(struct mtk_dp *mtk_dp,
				  const enum dp_encoder_id encoder_id, u8 location)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			3 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);

	switch (location) {
	case DP_PG_LOCATION_ALL:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_LEFT:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_RIGHT:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(2), GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_LEFT_OF_LEFT:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_RIGHT_OF_LEFT:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(1), GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_LEFT_OF_RIGHT:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(2), GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_RIGHT_OF_RIGHT:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(2) | BIT(1), GENMASK(2, 0));
		break;
	default:
		break;
	}
}

void mtk_dp_pg_horizontal_color_bar(struct mtk_dp *mtk_dp,
				    const enum dp_encoder_id encoder_id, u8 location)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* video select hw or pattern gen 0:HW 1:PG */
	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			4 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);
	switch (location) {
	case DP_PG_LOCATION_ALL:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_TOP:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_BOTTOM:
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset,
				BIT(2), GENMASK(2, 0));
		break;
	default:
		break;
	}
}

void mtk_dp_pg_chessboard(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, u8 location,
			  u16 hde, u16 vde)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset, BIT(6) | BIT(4),
			GENMASK(6, 4));

	if (location == DP_PG_LOCATION_ALL) {
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3194_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3198_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_319C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_31A8_DP_ENCODER0_P0 + reg_offset,
				 (hde / 8), GENMASK(13, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_31AC_DP_ENCODER0_P0 + reg_offset,
				 (vde / 8), GENMASK(13, 0));
	}
}

void mtk_dp_pg_sub_pixel(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, u8 location)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset,
			BIT(6) | BIT(5), GENMASK(6, 4));

	switch (location) {
	case DP_PG_PIXEL_ODD_MASK:
		WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + 1 + reg_offset, 0, BIT(5));
		break;
	case DP_PG_PIXEL_EVEN_MASK:
		WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + 1 + reg_offset,
				BIT(5), BIT(5));
		break;
	default:
		break;
	}
}

void mtk_dp_pg_frame(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, u8 location,
		     u16 hde, u16 vde)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset, BIT(6) | BIT(5) | BIT(4),
			GENMASK(6, 4));

	if (location == DP_PG_PIXEL_ODD_MASK) {
		WRITE_BYTE_MASK(mtk_dp, REG_31B0_DP_ENCODER0_P0 + 1 + reg_offset,
				0, BIT(5));
		WRITE_2BYTE_MASK(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3194_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_3198_DP_ENCODER0_P0 + reg_offset,
				 0xFFF, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_319C_DP_ENCODER0_P0 + reg_offset,
				 0, GENMASK(11, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_31A8_DP_ENCODER0_P0 + reg_offset,
				 ((hde / 8) - 12), GENMASK(13, 0));
		WRITE_2BYTE_MASK(mtk_dp, REG_31AC_DP_ENCODER0_P0 + reg_offset,
				 ((vde / 8) - 12), GENMASK(13, 0));
		WRITE_BYTE_MASK(mtk_dp, REG_31B4_DP_ENCODER0_P0 + reg_offset,
				0x0B, GENMASK(3, 0));
	}
}

void mtk_dp_pg_type_sel(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			int pattern_type, u8 bgr, u32 color_depth, u8 location)
{
	u16 hde, vde;

	hde = mtk_dp->info[encoder_id].dp_output_timing.hde;
	vde = mtk_dp->info[encoder_id].dp_output_timing.vde;

	switch (pattern_type) {
	case DP_PG_PURE_COLOR:
		mtk_dp_pg_pure_color(mtk_dp, encoder_id, bgr, color_depth);
		break;

	case DP_PG_VERTICAL_RAMPING:
		mtk_dp_pg_vertical_ramping(mtk_dp, encoder_id, bgr, color_depth, location);
		break;

	case DP_PG_HORIZONTAL_RAMPING:
		mtk_dp_pg_horizontal_ramping(mtk_dp, encoder_id, bgr,
					     color_depth, location);
		break;

	case DP_PG_VERTICAL_COLOR_BAR:
		mtk_dp_pg_vertical_color_bar(mtk_dp, encoder_id, location);
		break;

	case DP_PG_HORIZONTAL_COLOR_BAR:
		mtk_dp_pg_horizontal_color_bar(mtk_dp, encoder_id, location);
		break;

	case DP_PG_CHESSBOARD_PATTERN:
		mtk_dp_pg_chessboard(mtk_dp, encoder_id, location, hde, vde);
		break;

	case DP_PG_SUB_PIXEL_PATTERN:
		mtk_dp_pg_sub_pixel(mtk_dp, encoder_id, location);
		break;

	case DP_PG_FRAME_PATTERN:
		mtk_dp_pg_frame(mtk_dp, encoder_id, location, hde, vde);
		break;

	default:
		break;
	}
}

void mtk_dp_spkg_asp_hb32(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			  u8 enable, u8 HB3, u8 HB2)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset,
			 (enable ? 0x01 : 0x00) << ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset,
			 HB2 << ASP_HB2_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB2_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset,
			 HB3 << ASP_HB3_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB3_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dp_spkg_sdp(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
		     bool enable,
			   u8 sdp_type,
			   u8 *hb,
			   u8 *db)
{
	u8  offset;
	u16 st_offset;
	u8  hb_offset;
	u8  reg_index;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable) {
		for (offset = 0; offset < 0x10; offset++)
			for (reg_index = 0; reg_index < 2; reg_index++) {
				u32 addr = REG_3200_DP_ENCODER1_P0
					      + offset * 4 + reg_index + reg_offset;

				WRITE_BYTE(mtk_dp, addr, (db[offset * 2 + reg_index]));
				DP_DBG("SDP address:%u, data:%d\n",
				       addr, db[offset * 2 + reg_index]);
			}

		if (sdp_type == DP_SDP_PKG_DRM) {
			for (hb_offset = 0; hb_offset < 4 / 2; hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_3138_DP_ENCODER0_P0
					+ hb_offset * 4 + reg_index + reg_offset;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, (hb[offset]));
					DP_DBG("W Reg addr:%x, index:%d\n", addr, offset);
				}
		} else if (sdp_type >= DP_SDP_PKG_PPS0 &&
			   sdp_type <= DP_SDP_PKG_PPS3) {
			for (hb_offset = 0; hb_offset < (4 / 2); hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_3130_DP_ENCODER0_P0
					+ hb_offset * 4 + reg_index + reg_offset;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, hb[offset]);
					DP_DBG("W H1 Reg addr:%x, index:%d\n", addr, offset);
				}
		} else if (sdp_type == DP_SDP_PKG_ADS) {
			for (hb_offset = 0; hb_offset < (4 >> 1); hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_31F0_DP_ENCODER0_P0 + reg_offset
									+ hb_offset * 8 + reg_index;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, hb[offset]);
				}
		} else {
			st_offset = (sdp_type - DP_SDP_PKG_ACM) * 8;

			for (hb_offset = 0; hb_offset < 4 / 2; hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_30D8_DP_ENCODER0_P0
					+ st_offset
					+ hb_offset * 4 + reg_index + reg_offset;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, hb[offset]);
					DP_DBG("W H2 Reg addr:%x, index:%d\n", addr, offset);
				}
		}
	}

	switch (sdp_type) {
	case DP_SDP_PKG_NONE:
		break;

	case DP_SDP_PKG_ACM:
		WRITE_BYTE(mtk_dp, REG_30B4_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_ACM,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30B4_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE ACM\n");
		}

		break;

	case DP_SDP_PKG_ISRC:
		WRITE_BYTE(mtk_dp, (REG_30B4_DP_ENCODER0_P0 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE(mtk_dp, (REG_31EC_DP_ENCODER0_P0 + 1 + reg_offset), 0x1C);
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_ISRC,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));

			if (hb[3] & BIT(2))
				WRITE_BYTE_MASK(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset,
						BIT(0), BIT(0));
			else
				WRITE_BYTE_MASK(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset,
						0, BIT(0));

			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, (REG_30B4_DP_ENCODER0_P0 + 1 + reg_offset), 0x05);
			DP_MSG("SENT SDP TYPE ISRC\n");
		}

		break;

	case DP_SDP_PKG_AVI:
		WRITE_BYTE(mtk_dp, (REG_30A4_DP_ENCODER0_P0 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_AVI,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, (REG_30A4_DP_ENCODER0_P0 + 1 + reg_offset), 0x05);
			DP_MSG("SENT SDP TYPE AVI\n");
		}

		break;

	case DP_SDP_PKG_AUI:
		WRITE_BYTE(mtk_dp, REG_30A8_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_AUI,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30A8_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE AUI\n");
		}

		break;

	case DP_SDP_PKG_SPD:
		WRITE_BYTE(mtk_dp, (REG_30A8_DP_ENCODER0_P0 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_SPD,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30A8_DP_ENCODER0_P0 + 1 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE SPD\n");
		}

		break;

	case DP_SDP_PKG_MPEG:
		WRITE_BYTE(mtk_dp, REG_30AC_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_MPEG,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30AC_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE MPEG\n");
		}

		break;

	case DP_SDP_PKG_NTSC:
		WRITE_BYTE(mtk_dp, (REG_30AC_DP_ENCODER0_P0 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_NTSC,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, (REG_30AC_DP_ENCODER0_P0 + 1 + reg_offset), 0x05);
			DP_MSG("SENT SDP TYPE NTSC\n");
		}

		break;

	case DP_SDP_PKG_VSP:
		WRITE_BYTE(mtk_dp, REG_30B0_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_VSP,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30B0_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE VSP\n");
		}

		break;

	case DP_SDP_PKG_VSC:
		WRITE_BYTE(mtk_dp, REG_30B8_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_VSC,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30B8_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE VSC\n");
		}

		break;

	case DP_SDP_PKG_EXT:
		WRITE_BYTE(mtk_dp, (REG_30B0_DP_ENCODER0_P0 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_EXT,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_30B0_DP_ENCODER0_P0 + 1 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE EXT\n");
		}

		break;

	case DP_SDP_PKG_PPS0:
		WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_PPS0,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE PPS0\n");
		}

		break;

	case DP_SDP_PKG_PPS1:
		WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_PPS1,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE PPS1\n");
		}

		break;

	case DP_SDP_PKG_PPS2:
		WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_PPS2,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE PPS2\n");
		}

		break;

	case DP_SDP_PKG_PPS3:
		WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_PPS3,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE PPS3\n");
		}

		break;

	case DP_SDP_PKG_DRM:
		WRITE_BYTE(mtk_dp, REG_31DC_DP_ENCODER0_P0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE(mtk_dp, REG_3138_DP_ENCODER0_P0 + reg_offset, hb[0]);
			WRITE_BYTE(mtk_dp, (REG_3138_DP_ENCODER0_P0 + 1 + reg_offset), hb[1]);
			WRITE_BYTE(mtk_dp, REG_313C_DP_ENCODER0_P0 + reg_offset, hb[2]);
			WRITE_BYTE(mtk_dp, REG_313C_DP_ENCODER0_P0 + 1 + reg_offset, hb[3]);
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_DRM,
								BIT(4) | BIT(3) | BIT(2)
								| BIT(1) | BIT(0));
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, REG_31DC_DP_ENCODER0_P0 + reg_offset, 0x05);
			DP_MSG("SENT SDP TYPE DRM\n");
		}

		break;

	case DP_SDP_PKG_ADS:
		/* adaptive sync SDP transmit disable */
		WRITE_BYTE_MASK(mtk_dp, REG_31EC_DP_ENCODER0_P0 + reg_offset, 0,
				ADS_CFG_DP_ENCODER0_P0_FLDMASK);
		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					DP_SDP_PKG_ADS,
						SDP_PACKET_TYPE_DP_ENCODER1_P0_FLDMASK);
			/* write sdp data trigger */
			WRITE_BYTE_MASK(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset,
					1 << SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK_POS,
				SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK);
			/* adaptive sync SDP transmit enable */
			WRITE_BYTE_MASK(mtk_dp, REG_31EC_DP_ENCODER0_P0 + reg_offset,
					1 << ADS_CFG_DP_ENCODER0_P0_FLDMASK_POS,
						ADS_CFG_DP_ENCODER0_P0_FLDMASK);
			DP_MSG("SENT SDP TYPE ADS\n");
		}

		break;

	default:
		break;
	}
}

void mtk_dp_spkg_vsc_ext_vesa(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			      bool enable,
				    u8 hdr_num,
				    u8 *db)
{
	u8  vsc_hb1 = 0x20;	/* VESA : 0x20; CEA : 0x21 */
	u8  vsc_hb2;
	u8  pkg_cnt;
	u8  loop;
	u8  offset;
	u8  reg_index;
	u16 sdp_offset;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (!enable) {
		WRITE_BYTE_MASK(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1 + reg_offset), 0, BIT(0));
		WRITE_BYTE_MASK(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset, 0, BIT(7));
		return;
	}

	vsc_hb2 = (hdr_num > 0) ? BIT(6) : 0x00;

	WRITE_BYTE(mtk_dp, REG_31C8_DP_ENCODER0_P0 + reg_offset, 0x00);
	WRITE_BYTE(mtk_dp, (REG_31C8_DP_ENCODER0_P0 + 1 + reg_offset), vsc_hb1);
	WRITE_BYTE(mtk_dp, REG_31CC_DP_ENCODER0_P0 + reg_offset, vsc_hb2);
	WRITE_BYTE(mtk_dp, (REG_31CC_DP_ENCODER0_P0 + 1 + reg_offset), 0x00);
	WRITE_BYTE(mtk_dp, REG_31D8_DP_ENCODER0_P0 + reg_offset, hdr_num);

	WRITE_BYTE_MASK(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset, BIT(0), BIT(0));
	WRITE_BYTE_MASK(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset, BIT(2), BIT(2));

	usleep_range(50, 51);
	WRITE_BYTE_MASK(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset, 0, BIT(2));
	usleep_range(50, 51);

	for (pkg_cnt = 0; pkg_cnt < (hdr_num + 1); pkg_cnt++) {
		sdp_offset = 0;

		for (loop = 0; loop < 4; loop++) {
			for (offset = 0; offset < 8 / 2; offset++) {
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_3290_DP_ENCODER1_P0
					+ offset * 4 + reg_index + reg_offset;
					u8 tmp = sdp_offset
							+ offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, db[tmp]);
				}
			}

			WRITE_BYTE_MASK(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset,
					BIT(6), BIT(6));
			sdp_offset += 8;
		}
	}

	WRITE_BYTE_MASK(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1 + reg_offset), BIT(0), BIT(0));
	WRITE_BYTE_MASK(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset, BIT(7), BIT(7));
}

void mtk_dp_spkg_vsc_ext_cea(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			     bool enable,
				   u8 hdr_num,
				   u8 *db)
{
	u8  vsc_hb1 = 0x21;
	u8  vsc_hb2;
	u8  pkg_cnt;
	u8  loop;
	u8  offset;
	u8  reg_index;
	u16 sdp_offset;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (!enable) {
		WRITE_BYTE_MASK(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1  + reg_offset), 0, BIT(4));
		WRITE_BYTE_MASK(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset, 0, BIT(7));
		return;
	}

	vsc_hb2 = (hdr_num > 0) ? 0x40 : 0x00;

	WRITE_BYTE(mtk_dp, REG_31D0_DP_ENCODER0_P0  + reg_offset, 0x00);
	WRITE_BYTE(mtk_dp, (REG_31D0_DP_ENCODER0_P0 + 1  + reg_offset), vsc_hb1);
	WRITE_BYTE(mtk_dp, REG_31D4_DP_ENCODER0_P0  + reg_offset, vsc_hb2);
	WRITE_BYTE(mtk_dp, (REG_31D4_DP_ENCODER0_P0 + 1  + reg_offset), 0x00);
	WRITE_BYTE(mtk_dp, (REG_31D8_DP_ENCODER0_P0 + 1  + reg_offset), hdr_num);

	WRITE_BYTE_MASK(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset, BIT(0), BIT(0));
	WRITE_BYTE_MASK(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset, BIT(2), BIT(2));
	usleep_range(50, 51);

	WRITE_BYTE_MASK(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset, 0, BIT(2));

	for (pkg_cnt = 0; pkg_cnt < (hdr_num + 1); pkg_cnt++) {
		sdp_offset = 0;

		for (loop = 0; loop < 4; loop++) {
			for (offset = 0; offset < 4; offset++) {
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_32A4_DP_ENCODER1_P0
					+ offset * 4 + reg_index  + reg_offset;
					u8 tmp = sdp_offset
							+ offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, db[tmp]);
				}
			}

			WRITE_BYTE_MASK(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset,
					BIT(6), BIT(6));
			sdp_offset += 8;
		}
	}

	WRITE_BYTE_MASK(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1  + reg_offset), BIT(4), BIT(4));
	WRITE_BYTE_MASK(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset, BIT(7), BIT(7));
}

unsigned int mtk_dp_audio_get_caps(struct mtk_dp *mtk_dp)
{
	struct cea_sad *sads;
	int sad_count, i, j;
	unsigned int caps = 0;

	if (!mtk_dp->edid) {
		DP_ERR("EDID not found\n");
		return 0;
	}

	sad_count = drm_edid_to_sad(mtk_dp->edid, &sads);
	if (sad_count <= 0) {
		DP_MSG("The SADs is NULL\n");
		return 0;
	}

	for (i = 0; i < sad_count; i++) {
		if (sads[i].format == 0x01)	{
			for (j = 0; j < sads[i].channels; j++)
				caps |= ((1 << j) <<
					DP_CAPABILITY_CHANNEL_SFT) &
					(DP_CAPABILITY_CHANNEL_MASK <<
					DP_CAPABILITY_CHANNEL_SFT);

			caps |= (sads[i].freq << DP_CAPABILITY_SAMPLERATE_SFT) &
				(DP_CAPABILITY_SAMPLERATE_MASK <<
				DP_CAPABILITY_SAMPLERATE_SFT);
			caps |= (sads[i].byte2 << DP_CAPABILITY_BITWIDTH_SFT) &
				(DP_CAPABILITY_BITWIDTH_MASK <<
				DP_CAPABILITY_BITWIDTH_SFT);
		}
	}

	DP_MSG("audio caps:0x%x", caps);
	return caps;
}

void mtk_dp_audio_sample_arrange(struct mtk_dp *mtk_dp,
				 const enum dp_encoder_id encoder_id, u8 enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	u32 value = 0;

	/* hblank * link_rate(MHZ) / pix_clk(MHZ) / 4 * 0.8 */
	value = (mtk_dp->info[encoder_id].dp_output_timing.htt
	- mtk_dp->info[encoder_id].dp_output_timing.hde) *
		mtk_dp->training_info.link_rate * 27 * 200 /
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate;

	if (enable) {
		WRITE_4BYTE_MASK(mtk_dp,
				 REG_3370_DP_ENCODER1_P0 + 4 + reg_offset, BIT(12), BIT(12));
		WRITE_4BYTE_MASK(mtk_dp,
				 REG_3370_DP_ENCODER1_P0 + 4 + reg_offset,
				 (u16)value, GENMASK(11, 0));
	} else {
		WRITE_4BYTE_MASK(mtk_dp,
				 REG_3370_DP_ENCODER1_P0 + 4 + reg_offset, 0, BIT(12));
		WRITE_4BYTE_MASK(mtk_dp,
				 REG_3370_DP_ENCODER1_P0 + 4 + reg_offset, 0, GENMASK(11, 0));
	}
	DP_MSG("audio sample arrange, Htt:%d, hde:%d, link_rate:%d, pixcel_rate:%llu\n",
	       mtk_dp->info[encoder_id].dp_output_timing.htt,
	      mtk_dp->info[encoder_id].dp_output_timing.hde,
		mtk_dp->training_info.link_rate,
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate);

	DP_MSG("Audio arrange patch, enable:%d, value:0x%x\n", enable, value);
}

void mtk_dp_audio_pg_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, u8 channel,
			    u8 fs, u8 enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, REG_307C_DP_ENCODER0_P0 + reg_offset, 0,
			 HBLANK_SPACE_FOR_SDP_HW_EN_DP_ENCODER0_P0_FLDMASK);

	if (enable) {
		WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);

		/* [9 : 8] set 0x3 : PG	mtk_dp */
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 0x3 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS,
				AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset,
				 0x0, TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK);
	} else {
		WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
				 0, AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		/* [ 9 : 8] set 0x0 : dprx, for Source project, it means for front-end audio */
		/* [10 : 8] set 0x4 : TDM after (include) Posnot */
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 0x4 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS
				, AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		/* [0]: TDM to DP transfer enable */
		WRITE_2BYTE_MASK(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset,
				 TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK,
				TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK);
		/* [12:8]: TDM audio data 32 bit */
		/* 32bit:0x1F */
		/* 24bit:0x17 */
		/* 20bit:0x13 */
		/* 16bit:0x0F */
		WRITE_2BYTE_MASK(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset,
				 (0x1F << TDM_AUDIO_DATA_BIT_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_BIT_DP_ENCODER1_P0_FLDMASK);
		WRITE_BYTE_MASK(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset, BIT(0), BIT(0));
	}

	DP_MSG("fs:%d, ch:%d\n", fs, channel);

	/* audio channel count change reset */
	WRITE_BYTE_MASK(mtk_dp, (REG_33F4_DP_ENCODER1_P0 + 1 + reg_offset), BIT(1), BIT(1));

	WRITE_2BYTE_MASK(mtk_dp, REG_3304_DP_ENCODER1_P0 + reg_offset,
			 AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3304_DP_ENCODER1_P0 + reg_offset,
			 AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3304_DP_ENCODER1_P0 + reg_offset,
			 0x1000, 0x1000);

	WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
			 AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
			 AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
			 AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
			 AU_EN_DP_ENCODER0_P0_FLDMASK,
			 AU_EN_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset,
			 AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK,
		AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset,
			 AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK,
		AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK);

	switch (fs) {
	case FS_44K:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	case FS_48K:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x1 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	case FS_192K:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x2 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	default:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;
	}

	WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset, 0,
			 AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset, 0,
			 AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset, 0,
			 AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset, 0,
			 AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK);

	switch (channel) {
	case 2:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x0 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
				 (0x1 << AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
		if (!enable)	/* TDM audio interface, audio channel number, 1: 2ch */
			WRITE_2BYTE_MASK(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset,
					 (0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		mtk_dp_spkg_asp_hb32(mtk_dp, encoder_id, true, DP_SDP_ASP_HB3_AU02CH, 0x0);
		break;

	case 8:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x1 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
				 (0x1 << AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);
		/* TDM audio interface, audio channel number, 7: 8ch */
		/* Always 1 no matter how many channels */
		WRITE_2BYTE_MASK(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset,
				 (0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		mtk_dp_spkg_asp_hb32(mtk_dp, encoder_id, true, DP_SDP_ASP_HB3_AU08CH, 0x0);
		break;

	case 16:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x2 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset,
				 (0x1 << AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;

	case 32:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x3 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset,
				 (0x1 << AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;

	default:
		WRITE_2BYTE_MASK(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset,
				 (0x0 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		WRITE_2BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
				 (0x1 << AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
			AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
		/* TDM audio interface, audio channel number, 1: 2ch */
		WRITE_2BYTE_MASK(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset,
				 (0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
			TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		mtk_dp_spkg_asp_hb32(mtk_dp, encoder_id, true, DP_SDP_ASP_HB3_AU02CH, 0x0);
		break;
	}

	/* TDM to DP reset [1] */
	WRITE_BYTE_MASK(mtk_dp, (REG_331C_DP_ENCODER1_P0 + reg_offset),
			TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK,
			TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset),
			 0x1 << SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS,
			SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	usleep_range(5, 6);
	WRITE_BYTE_MASK(mtk_dp, (REG_331C_DP_ENCODER1_P0 + reg_offset),
			0x0, TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset),
			 (0x0 << SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS),
			SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK);

	/* audio channel count change reset */
	WRITE_BYTE_MASK(mtk_dp, (REG_33F4_DP_ENCODER1_P0 + 1 + reg_offset), 0, BIT(1));
}

void mtk_dp_audio_ch_status_set(struct mtk_dp *mtk_dp,
				const enum dp_encoder_id encoder_id,
				u8 channel, u8 fs, u8 word_length)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	union dp_rx_audio_chsts aud_ch_sts;

	memset(&aud_ch_sts, 0, sizeof(aud_ch_sts));

	switch (channel) {
	case 2:
		aud_ch_sts.audio_chsts.channel_number = 2;
		break;

	case 8:
		aud_ch_sts.audio_chsts.channel_number = 8;
		break;

	default:
		aud_ch_sts.audio_chsts.channel_number = 2;
		break;
	}

	switch (fs) {
	case FS_32K:
		aud_ch_sts.audio_chsts.sampling_freq = 3;
		aud_ch_sts.audio_chsts.original_sampling_freq = 0xC;
		break;
	case FS_44K:
		aud_ch_sts.audio_chsts.sampling_freq = 0;
		aud_ch_sts.audio_chsts.original_sampling_freq = 0xF;
		break;
	case FS_48K:
		aud_ch_sts.audio_chsts.sampling_freq = 2;
		aud_ch_sts.audio_chsts.original_sampling_freq = 0xD;
		break;
	case FS_88K:
		aud_ch_sts.audio_chsts.sampling_freq = 8;
		aud_ch_sts.audio_chsts.original_sampling_freq = 7;
		break;
	case FS_96K:
		aud_ch_sts.audio_chsts.sampling_freq = 0xA;
		aud_ch_sts.audio_chsts.original_sampling_freq = 5;
		break;
	case FS_192K:
		aud_ch_sts.audio_chsts.sampling_freq = 0xE;
		aud_ch_sts.audio_chsts.original_sampling_freq = 1;
		break;
	default:
		aud_ch_sts.audio_chsts.sampling_freq = 0x1;
		aud_ch_sts.audio_chsts.original_sampling_freq = 0xD;
		break;
	}

	switch (word_length) {
	case WL_16BIT:
		aud_ch_sts.audio_chsts.word_len = 0x02;
		break;
	case WL_20BIT:
		aud_ch_sts.audio_chsts.word_len = 0x03;
		break;
	case WL_24BIT:
		aud_ch_sts.audio_chsts.word_len = 0x0B;
		break;
	}

	WRITE_2BYTE(mtk_dp, REG_308C_DP_ENCODER0_P0 + reg_offset,
		    aud_ch_sts.audio_chsts_raw[1] << 8 | aud_ch_sts.audio_chsts_raw[0]);
	WRITE_2BYTE(mtk_dp, REG_3090_DP_ENCODER0_P0 + reg_offset,
		    aud_ch_sts.audio_chsts_raw[3] << 8 | aud_ch_sts.audio_chsts_raw[2]);
	WRITE_BYTE(mtk_dp, REG_3094_DP_ENCODER0_P0 + reg_offset, aud_ch_sts.audio_chsts_raw[4]);

	mdelay(1);
}

void mtk_dp_audio_sdp_setting(struct mtk_dp *mtk_dp,
			      const enum dp_encoder_id encoder_id, u8 channel)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* [7 : 0] HB2 */
	WRITE_BYTE_MASK(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset, 0x00, 0xFF);

	if (channel == 8)
		/* [15 : 8]channel-1 */
		WRITE_2BYTE_MASK(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset, 0x0700, 0xFF00);
	else
		WRITE_2BYTE_MASK(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset, 0x0100, 0xFF00);
}

void mtk_dp_audio_mdiv_set(struct mtk_dp *mtk_dp,
			   const enum dp_encoder_id encoder_id, u8 div)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset,
			 div << AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dp_i2s_audio_set_m_div(struct mtk_dp *mtk_dp,
				const enum dp_encoder_id encoder_id, u8 div)
{
	char table[7][5] = {"X2", "X4", "X8", "/2", "/4", "N/A", "/8"};

	DP_MSG("I2S Set Audio, M divider:%s\n", table[div - 1]);
	mtk_dp_audio_mdiv_set(mtk_dp, encoder_id, div);
}

void mtk_dp_i2s_audio_sdp_channel_setting(struct mtk_dp *mtk_dp,
					  const enum dp_encoder_id encoder_id,
					  u8 channel, u8 fs, u8 word_length)
{
	u8 sdp_db[32] = {0};
	u8 sdp_hb[4] = {0};

	sdp_hb[1] = DP_SDP_HB1_PKG_AINFO;
	sdp_hb[2] = 0x1B;
	sdp_hb[3] = 0x48;

	sdp_db[0x0] = 0x10 | (channel - 1); /* L-PCM[7:4], channel-1[2:0] */
	sdp_db[0x1] = fs << 2 | word_length; /* fs[4:2], len[1:0] */
	sdp_db[0x2] = 0x0;

	if (channel == 8)
		sdp_db[0x3] = 0x13;
	else
		sdp_db[0x3] = 0x00;

	mtk_dp_audio_sdp_setting(mtk_dp, encoder_id, channel);
	DP_MSG("I2S Set Audio, channel:%d\n", channel);
	mtk_dp_spkg_sdp(mtk_dp, encoder_id, true, DP_SDP_PKG_AUI, sdp_hb, sdp_db);
}

void mtk_dp_i2s_audio_ch_status_set(struct mtk_dp *mtk_dp,
				    const enum dp_encoder_id encoder_id, u8 channel,
				    u8 fs, u8 word_length)
{
	mtk_dp_audio_ch_status_set(mtk_dp, encoder_id, channel, fs, word_length);
}

void mtk_dp_i2s_audio_config(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u8 channel, fs, word_length;
	unsigned int tmp = mtk_dp->info[encoder_id].audio_config;

	if (!mtk_dp->dp_ready) {
		DP_ERR("%s, DP is not ready\n", __func__);
		return;
	}

	if (fake_cable_in) {
		channel = BIT(force_ch);
		fs = BIT(force_fs);
		word_length = BIT(force_len);
	} else {
		channel = (tmp >> DP_CAPABILITY_CHANNEL_SFT)
			& DP_CAPABILITY_CHANNEL_MASK;
		fs = (tmp >> DP_CAPABILITY_SAMPLERATE_SFT)
			& DP_CAPABILITY_SAMPLERATE_MASK;
		word_length = (tmp >> DP_CAPABILITY_BITWIDTH_SFT)
				& DP_CAPABILITY_BITWIDTH_MASK;
	}

	switch (channel) {
	case DP_CHANNEL_2:
		channel = 2;
		break;
	case DP_CHANNEL_8:
		channel = 8;
		break;
	default:
		channel = 2;
		break;
	}

	switch (fs) {
	case DP_SAMPLERATE_32:
		fs = FS_32K;
		break;
	case DP_SAMPLERATE_44:
		fs = FS_44K;
		break;
	case DP_SAMPLERATE_48:
		fs = FS_48K;
		break;
	case DP_SAMPLERATE_96:
		fs = FS_96K;
		break;
	case DP_SAMPLERATE_192:
		fs = FS_192K;
		break;
	default:
		fs = FS_48K;
		break;
	}

	switch (word_length) {
	case DP_BITWIDTH_16:
		word_length = WL_16BIT;
		break;
	case DP_BITWIDTH_20:
		word_length = WL_20BIT;
		break;
	case DP_BITWIDTH_24:
		word_length = WL_24BIT;
		break;
	default:
		word_length = WL_24BIT;
		break;
	}

	mtk_dp_i2s_audio_sdp_channel_setting(mtk_dp, encoder_id, channel,
					     fs, word_length);
	mtk_dp_i2s_audio_ch_status_set(mtk_dp, encoder_id, channel,
				       fs, word_length);
	mtk_dp_audio_pg_enable(mtk_dp, encoder_id, channel, fs, true);
	mtk_dp_i2s_audio_set_m_div(mtk_dp, encoder_id, 4);
}

void mtk_dp_dsc_get_capability(u8 *dsc_cap)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	if (!g_mtk_dp->dp_ready) {
		DP_MSG("%s, DP is not ready\n", __func__);
		return;
	}

	drm_dp_dpcd_read(&g_mtk_dp->aux, DPCD_00060, dsc_cap, 16);
}

void mtk_dp_dsc_support(struct mtk_dp *mtk_dp)
{
#if DP_SUPPORT_DSC
	u8 data[3];

	drm_dp_dpcd_read(&mtk_dp->aux, 0x60, data, 1);
	if (data[0] & BIT(0))
		mtk_dp->has_dsc   = true;
	else
		mtk_dp->has_dsc   = false;

	DP_MSG("Sink has_dsc:%d\n", mtk_dp->has_dsc);
#endif
}

void mtk_dp_dsc_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	DP_FUNC("DSC enable:%d\n", enable);

	mtk_dp->dsc_enable = enable;

	WRITE_2BYTE_MASK(mtk_dp, REG_31C4_DP_ENCODER0_P0 + reg_offset,
			 0,
		PPS_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK);

	if (enable) {
		/* [0] : DSC Enable */
		WRITE_BYTE_MASK(mtk_dp,
				REG_336C_DP_ENCODER1_P0 + reg_offset, BIT(0), BIT(0));
		/* 300C [9] : VB-ID[6] DSC enable */
		WRITE_BYTE_MASK(mtk_dp,
				REG_300C_DP_ENCODER0_P0 + 1 + reg_offset, BIT(1), BIT(1));
		/* 303C[10 : 8] : DSC color depth */
		WRITE_BYTE_MASK(mtk_dp,
				REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				0x7, GENMASK(2, 0));
		/* 303C[14 : 12] : DSC color format */
		WRITE_BYTE_MASK(mtk_dp,
				REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				0x7 << 4, GENMASK(6, 4));
		/* 31FC[12] : HDE last num control */
		WRITE_2BYTE_MASK(mtk_dp, REG_31FC_DP_ENCODER0_P0 + reg_offset,
				 0x2 << DE_LAST_NUM_SW_DP_ENCODER0_P0_FLDMASK_POS,
				DE_LAST_NUM_SW_DP_ENCODER0_P0_FLDMASK);
	} else {
		/* DSC Disable */
		WRITE_BYTE_MASK(mtk_dp,
				REG_336C_DP_ENCODER1_P0 + reg_offset, 0, BIT(0));
		WRITE_BYTE_MASK(mtk_dp,
				REG_300C_DP_ENCODER0_P0 + 1 + reg_offset, 0, BIT(1));
		/* default 8bit */
		WRITE_BYTE_MASK(mtk_dp,
				REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				0x3, GENMASK(2, 0));
		/* default RGB */
		WRITE_BYTE_MASK(mtk_dp,
				REG_303C_DP_ENCODER0_P0 + 1 + reg_offset,
				0x0, GENMASK(6, 4));

		/* 31FC[12] : HDE last num control */
		/* 31FC[12] : HDE last num control */
		WRITE_2BYTE_MASK(mtk_dp, REG_31FC_DP_ENCODER0_P0 + reg_offset,
				 0, DE_LAST_NUM_SW_DP_ENCODER0_P0_FLDMASK);
	}
}

void mtk_dp_set_chunk_size(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			   u8 slice_num, u16 chunk_num, u8 remainder,
	u8 lane_count, u32 hde_last_num, u8 hde_num_even)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp,
			REG_336C_DP_ENCODER1_P0 + reg_offset,
		slice_num << 4, GENMASK(7, 4));
	WRITE_BYTE_MASK(mtk_dp,
			REG_336C_DP_ENCODER1_P0 + 1 + reg_offset,
		remainder, GENMASK(3, 0));
	WRITE_2BYTE(mtk_dp,
		    REG_3370_DP_ENCODER1_P0 + reg_offset, chunk_num - 1); /* set chunk_num */

	/* msdbg for compiler error */
	WRITE_2BYTE(mtk_dp, REG_3064_DP_ENCODER0_P0 + reg_offset, hde_num_even);
	/* 0x31FC replaced by 0x3064 */
	WRITE_2BYTE(mtk_dp, REG_3064_DP_ENCODER0_P0 + reg_offset, hde_last_num);
}

void mtk_dp_dsc_set_param(struct mtk_dp *mtk_dp,
			  const enum dp_encoder_id encoder_id, u8 slice_num, u16 chunk_num)
{
	u8 r8, r16;
	u8 q16[16] = {0x6, 0x01, 0x01, 0x03, 0x03, 0x05, 0x05, 0x07,
		0x07, 0x00, 0x00, 0x02, 0x02, 0x04, 0x04, 0x06};
	u8 q8[8] = {0x6, 0x01, 0x03, 0x05, 0x07, 0x00, 0x02, 0x04};
	u8 hde_last_num, hde_num_even;

	DP_MSG("lane count:%d\n", mtk_dp->training_info.link_lane_count);
	if (mtk_dp->training_info.link_lane_count == DP_2LANE) {
		if (chunk_num % 2)
			r16 = ((chunk_num + 1 + 2) * slice_num / 3) % 16;
		else
			r16 = ((chunk_num + 2) * slice_num / 3) % 16;
		DP_MSG("r16:%d\n", r16);
		/* r16 = 1; test for 1080p */
		hde_last_num = (q16[r16] & (BIT(1) | BIT(2))) >> 1;
		hde_num_even = q16[r16] & BIT(0);
	} else {
		r8 = ((chunk_num + 1) * slice_num / 3) % 8;
		DP_MSG("r8:%d\n", r8);
		/* r8 = 1; test for 1080p */
		hde_last_num = (q8[r8] & (BIT(1) | BIT(2))) >> 1;
		hde_num_even = q8[r8] & BIT(0);
	}

	mtk_dp_set_chunk_size(mtk_dp, encoder_id, slice_num - 1, chunk_num, chunk_num % 12,
			      mtk_dp->training_info.link_lane_count,
		hde_last_num, hde_num_even);
}

void mtk_dp_dsc_set_pps(struct mtk_dp *mtk_dp,
			const enum dp_encoder_id encoder_id, u8 *PPS, bool enable)
{
	u8 HB[4] = {0x0, 0x10, 0x7F, 0x0};

	mtk_dp_spkg_sdp(mtk_dp, encoder_id, enable, DP_SDP_PKG_PPS0, HB, PPS +  0);
	mtk_dp_spkg_sdp(mtk_dp, encoder_id, enable, DP_SDP_PKG_PPS1, HB, PPS + 32);
	mtk_dp_spkg_sdp(mtk_dp, encoder_id, enable, DP_SDP_PKG_PPS2, HB, PPS + 64);
	mtk_dp_spkg_sdp(mtk_dp, encoder_id, enable, DP_SDP_PKG_PPS3, HB, PPS + 96);
}

void mtk_dp_dsc_pps_send(const enum dp_encoder_id encoder_id, u8 *PPS_128)
{
	u8 dsc_cap[16] = {0};
	u8 slice_num = 0;
	u16 chunk_size = PPS_128[14] << 8 | PPS_128[15];
	u16 pic_width = PPS_128[8] << 8 | PPS_128[9];
	u16 slice_width = PPS_128[12] << 8 | PPS_128[13];

	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	if (!g_mtk_dp->dp_ready) {
		DP_MSG("%s, DP is not ready\n", __func__);
		return;
	}

	mtk_dp_dsc_get_capability(dsc_cap);

	PPS_128[0x0] =
		((dsc_cap[0x1] & 0xf) << 4) | ((dsc_cap[0x1] & 0xf0) >> 4);
	if (dsc_cap[0x6] & BIT(0))
		PPS_128[0x4] |=  (0x1 << 5);
	else
		PPS_128[0x4] &= ~(0x1 << 5);
	if (slice_width <= 0)
		return;
	slice_num = (pic_width / slice_width);
	mtk_dp_dsc_set_pps(g_mtk_dp, encoder_id, PPS_128, true);
	mtk_dp_dsc_set_param(g_mtk_dp, encoder_id, slice_num, chunk_size);
}

void mtk_dp_phy_set_rate_param(struct mtk_dp *mtk_dp, enum dp_link_rate val)
{
	switch (val) {
	case DP_LINK_RATE_RBR:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x0);
		break;

	case DP_LINK_RATE_HBR:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x1);
		break;

	case DP_LINK_RATE_HBR2:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x2);
		break;

	case DP_LINK_RATE_HBR3:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x3);
		break;
	default:
		break;
	}
}

void mtk_dp_set_rate(struct mtk_dp *mtk_dp, int value)
{
	/* power off TPLL and Lane */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	DP_MSG("Set Tx Rate:0x%x\n", value);

	mtk_dp_phy_set_rate_param(mtk_dp, value);
	/* power on BandGap, TPLL and Lane */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);
}

static void mtk_dp_phy_set_link_rate(struct mtk_dp *mtk_dp, enum dp_link_rate val)
{
	mtk_dp_phy_set_rate_param(mtk_dp, val);
}

void mtk_dp_phy_set_lane_pwr(struct mtk_dp *mtk_dp, enum dp_lane_count lane_count)
{
	/* ***===power ON flow===***
	 * for 4Lane: -> 0x8 -> 0xC -> 0xE -> 0xF
	 * for 2Lane: -> 0x2 -> 0x3
	 * for 1Lane: -> 0x1
	 */
	int power_indx = lane_count - 1;
	u8 power_bmp = BIT(power_indx);

	do {
		power_bmp |= BIT(power_indx);
		PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_TX_CTL_0,
				    power_bmp << TX_LN_EN_FLDMASK_POS,
				TX_LN_EN_FLDMASK);
		DP_DBG("set lane pwr %x\n", (PHY_READ_BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET
						+ DP_PHY_DIG_TX_CTL_0) &
					TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS);
	} while (--power_indx >= 0);
}

static void mtk_dp_phy_clear_lane_pwr(struct mtk_dp *mtk_dp)
{
	/* ***===power OFF flow===***
	 * for 4Lane: -> 0x7 -> 0x3 -> 0x1 -> 0x0
	 * for 2Lane: -> 0x1 -> 0x0
	 * for 1Lane: -> 0x0
	 * u8 power_bmp; = (1 << lane_count) - 1;
	 */
	u8 power_bmp = (PHY_READ_BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET
							+ DP_PHY_DIG_TX_CTL_0) &
						TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS;

	do {
		power_bmp >>= 1;
		PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_TX_CTL_0,
				    power_bmp << TX_LN_EN_FLDMASK_POS,
				TX_LN_EN_FLDMASK);
		DP_DBG("clear lane pwr %x\n", (PHY_READ_BYTE(mtk_dp, PHYD_DIG_GLB_OFFSET
							+ DP_PHY_DIG_TX_CTL_0) &
						TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS);
	} while (power_bmp > 0);
}

void mtk_dp_phy_power_on(struct mtk_dp *mtk_dp)
{
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_EN_FLDMASK_POS, FORCE_PWR_STATE_EN_FLDMASK);
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	DP_MSG("DPTX PHYD power on\n");
}

static void mtk_dp_phy_power_down(struct mtk_dp *mtk_dp)
{
	mtk_dp_phy_clear_lane_pwr(mtk_dp);

	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_EN_FLDMASK_POS, FORCE_PWR_STATE_EN_FLDMASK);
	/* power off TPLL and Lane */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_SW_RST, 0, BIT(1) | BIT(3));
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_SW_RST,
			    BIT(1) | BIT(3), BIT(1) | BIT(3));

	DP_MSG("DPTX PHYD power down\n");
}

void mtk_dp_phyd_power_off(struct mtk_dp *mtk_dp)
{
	mtk_dp_phy_power_down(mtk_dp);

	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x0, FORCE_PWR_STATE_VAL_FLDMASK);

	DP_MSG("DPTX PHYD power off\n");
}

static void mtk_dp_phy_reset_swing_pre(struct mtk_dp *mtk_dp)
{
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);

	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x0  << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
}

static void mtk_dp_phy_ssc_enable(struct mtk_dp *mtk_dp, const u8 enable, const u8 ssc_delta)
{
	if (enable)
		PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				    0x1 << TPLL_SSC_EN_FLDMASK_POS, TPLL_SSC_EN_FLDMASK);
	else
		PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				    0x0, TPLL_SSC_EN_FLDMASK);

	DP_MSG("Phy SSC enable = %d\n", enable);
}

void mtk_dp_phy_param_init(struct mtk_dp *mtk_dp, u32 *buffer, u32 size)
{
	u32 i = 0;
	u8  mask = 0x3F;

	if (!buffer || size != DP_PHY_REG_COUNT) {
		DP_ERR("invalid param\n");
		return;
	}

	for (i = 0; i < DP_PHY_LEVEL_COUNT; i++) {
		mtk_dp->phy_params[i].c0 = (buffer[i / 4] >> (8 * (i % 4))) & mask;
		mtk_dp->phy_params[i].cp1 =
			(buffer[i / 4 + 3] >> (8 * (i % 4))) & mask;
	}
}

void mtk_dp_phy_4lane_enable(struct mtk_dp *mtk_dp, u8 enable)
{
	u8 i;
	u8 lane_count;
	u16 value;
	void *reg;
	u32 tmp;

	reg = ioremap(0x1002D600, 0x1000);

	/* 4-3. Inter-lane skew improvement for 4 lanes
	 * TX TFIFO read selection
	 * 00: Local path for 1 LANE configuration
	 * 01: Select DA_XTP_GLB_TX_DATA_EN_A/B for 2 LANE configuration
	 * 10: Select DA_XTP_GLB_TX_DATA_EN_C for 4 LANE configuration
	 * 11: Select DA_XTP_GLB_TX_DATA_EN_C or TX_DATA_EN_EXT for 8 LANE configuration
	 */
	if (enable) {
		lane_count = 4;
		value = (BIT(12) | BIT(13));

		tmp = readl(reg);
		tmp = (tmp & ~BIT(19)) | (BIT(19) & BIT(19));
		writel(tmp, reg);
	} else {
		lane_count = 2;
		value = BIT(12);

		tmp = readl(reg);
		tmp = (tmp & ~BIT(19)) | (0 & BIT(19));
		writel(tmp, reg);
	}

	for (i = 1; i <= lane_count; i++)
		PHY_WRITE_2BYTE_MASK(mtk_dp, 0x0100 * i, value, (BIT(12) | BIT(13)));
}

void mtk_dp_phy_flip_enable(struct mtk_dp *mtk_dp, const u8 flip_enable)
{
	void *reg;
	u32 tmp;

	reg = ioremap(0x1002D600, 0x1000);
	/* Lane select DATA_EN_EXT
	 * 0: from local GLB (select TX_DATA_EN_A/B/C)
	 * 1: from master GLB (select TX_DATA_EN_EXT)
	 */

	/* 4-3. Inter-lan skew improvement for 4 Lanes */
	if (flip_enable) {
		PHY_WRITE_BYTE(mtk_dp, 0x01A0, 0x47);
		PHY_WRITE_BYTE(mtk_dp, 0x02A0, 0x47);
		PHY_WRITE_BYTE(mtk_dp, 0x03A0, 0x46);
		PHY_WRITE_BYTE(mtk_dp, 0x04A0, 0x46);

		tmp = readl(reg);
		tmp = (tmp & ~BIT(18)) | (BIT(18) & BIT(18));
		writel(tmp, reg);
	} else {
		PHY_WRITE_BYTE(mtk_dp, 0x01A0, 0x46);
		PHY_WRITE_BYTE(mtk_dp, 0x02A0, 0x46);
		PHY_WRITE_BYTE(mtk_dp, 0x03A0, 0x47);
		PHY_WRITE_BYTE(mtk_dp, 0x04A0, 0x47);

		tmp = readl(reg);
		tmp = (tmp & ~BIT(18)) | (0 & BIT(18));
		writel(tmp, reg);
	}

	DP_MSG("Swap %s\n", flip_enable ? "enable" : "disable");
}

void mtk_dp_phy_set_param(struct mtk_dp *mtk_dp)
{
	u8 i;

	const u32 phyd_dig_lan_base_addr[4] = {
		PHYD_DIG_LAN0_OFFSET, PHYD_DIG_LAN1_OFFSET,
		PHYD_DIG_LAN2_OFFSET, PHYD_DIG_LAN3_OFFSET};

	/* 4-1. PLL Opitmization */
	PHY_WRITE_BYTE_MASK(mtk_dp, 0x0614, BIT(0), BIT(0));
	/* 4-2. Unused AUX TX High-Z */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x0700, 0x0, BIT(20));

	/* 4-4. Swing and Pre-emphasis Optimization */
	for (i = 0; i < 4; i++) {
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_3, 0x110E0C0A);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_4, 0x1212110E);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_5, 0x1815);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_6, 0x7040200);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_7, 0x60300);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_8, 0x3);
	}
}

void mtk_dp_phy_setting(struct mtk_dp *mtk_dp)
{
	/* step1: phy init */
	mtk_dp_phy_4lane_enable(mtk_dp, mtk_dp->training_info.max_link_lane_count == DP_4LANE);
	mtk_dp_phy_flip_enable(mtk_dp, mtk_dp->swap_enable);

	mtk_dp_phy_set_param(mtk_dp);
	/* step2: phy power ON */
	mtk_dp_phy_power_on(mtk_dp);
}

void mtk_dp_phy_training_config(struct mtk_dp *mtk_dp, const u8 link_rate,
				const u8 lane_count, const u8 ssc_enable)
{
	const u8 ssc_delta = mtk_dp->training_info.ssc_delta;

	mtk_dp_phy_reset_swing_pre(mtk_dp);
	mtk_dp_phy_ssc_enable(mtk_dp, ssc_enable, ssc_delta);

	/* step1: phy-d power down */
	mtk_dp_phy_power_down(mtk_dp);

	/* step2: phy-d set link rate */
	mtk_dp_phy_set_link_rate(mtk_dp, link_rate);
	mtk_dp_phy_power_on(mtk_dp);

	/* step3: phy-d enable lane */
	mtk_dp_phy_set_lane_pwr(mtk_dp, lane_count);
}

void mtk_dp_phy_set_idle_pattern(struct mtk_dp *mtk_dp, bool enable)
{
	DP_DBG("Idle pattern enable:%d\n", enable);
	if (enable)
		WRITE_BYTE_MASK(mtk_dp, REG_3580_DP_TRANS_P0 + 1, 0x0F, 0x0F);
	else
		WRITE_BYTE_MASK(mtk_dp, REG_3580_DP_TRANS_P0 + 1, 0x0, 0x0F);
}

void mtk_dp_ssc_on_off_setting(struct mtk_dp *mtk_dp, bool enable)
{
	DP_MSG("SSC_enable:%d\n", enable);

	/* power off TPLL and Lane; */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	/* Set SSC disable */
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
			     0x0, TPLL_SSC_EN_FLDMASK);

	/* delta1 = 0.05% and delta=0.05% */
	/* HBR3 8.1G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D4, 79 << 16, GENMASK(31, 16)); /* delta1 */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10DC, 49 << 16, GENMASK(31, 16)); /* delta */

	/* HBR2 5.4G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D4, 105, GENMASK(15, 0)); /* delta1 */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10DC, 65, GENMASK(15, 0)); /* delta */

	/* HBR 2.7G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D0, 105 << 16, GENMASK(31, 16)); /* delta1 */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D8, 65 << 16, GENMASK(31, 16)); /* delta */

	/* RBR 1.62G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D0, 63, GENMASK(15, 0)); /* delta1 */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D8, 39, GENMASK(15, 0)); /* delta */

	if (enable)
		/* Set SSC enable */
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				     TPLL_SSC_EN_FLDMASK, TPLL_SSC_EN_FLDMASK);
	else
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				     0, TPLL_SSC_EN_FLDMASK);

	/* power on BandGap, TPLL and Lane; */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	usleep_range(50, 51);
}

void mtk_dp_ssc_set_param(struct mtk_dp *mtk_dp, u8 ssc_delta)
{
#ifdef SSC_DELTA_PATCH
	/* 0: 0.25%, 1: 0.45% */
	u16 SSC_RBR[2][2] = {{393, 374}, {707, 688}};
	u16 SSC_HBR[2][2] = {{327, 311}, {589, 573}};
	u16 SSC_HBR2[2][2] = {{327, 311}, {589, 573}};
	u16 SSC_HBR3[2][2] = {{245, 234}, {442, 430}};

	/* HBR3 8.1G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D4,
			     SSC_HBR3[ssc_delta][0] << 16, GENMASK(31, 16));
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10DC,
			     SSC_HBR3[ssc_delta][1] << 16, GENMASK(31, 16));

	/* HBR2 5.4G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D4,
			     SSC_HBR2[ssc_delta][0], GENMASK(15, 0));
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10DC,
			     SSC_HBR2[ssc_delta][1], GENMASK(15, 0));

	/* HBR 2.7G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D0,
			     SSC_HBR[ssc_delta][0] << 16, GENMASK(31, 16));
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D8,
			     SSC_HBR[ssc_delta][1] << 16, GENMASK(31, 16));

	/* RBR 1.62G */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D0,
			     SSC_RBR[ssc_delta][0], GENMASK(15, 0));
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x10D8,
			     SSC_RBR[ssc_delta][1], GENMASK(15, 0));
#else
	DP_MSG("SSC use default delta:0.45\n");
#endif
}

void mtk_dp_ssc_enable(struct mtk_dp *mtk_dp, u8 enable, u8 ssc_delta)
{
	DP_MSG("SSC enable:%d\n", enable);

	/* power off TPLL and Lane; */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	/* Set SSC disable */
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
			     0x0, TPLL_SSC_EN_FLDMASK);

	mtk_dp_ssc_set_param(mtk_dp, ssc_delta);

	if (enable)
		/* Set SSC enable */
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				     TPLL_SSC_EN_FLDMASK, TPLL_SSC_EN_FLDMASK);

	/* power on BandGap, TPLL and Lane; */
	PHY_WRITE_BYTE_MASK(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			    0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);
	usleep_range(100, 101);
}

bool mtk_dp_ssc_check(struct mtk_dp *mtk_dp, u8 *p_enable)
{
	u8 status;
	u8 ret = 0;

#if (ENABLE_DP_SSC_OUTPUT == 0x1)
	if (mtk_dp->training_info.force_ssc_en) {
		*p_enable = mtk_dp->training_info.force_ssc;

		DP_MSG("SW force control SSC!!!\n");
	} else {
		*p_enable = mtk_dp->training_info.sink_ssc_en;
	}

	/* write DPCD_00107 = BIT4 when SSC enable */
#else
	*p_enable = false;

	DP_MSG("DPTX not support SSC, force off !\n");
#endif

	status = *p_enable ? BIT(4) : 0;
	ret = drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00107, &status, 0x1);

	if (ret > 0) {
		if (*p_enable)
			DP_MSG("Enable SSC via DPCD_00107\n");
		else
			DP_MSG("Disable SSC via DPCD_00107\n");

		return true;
	}

	DP_ERR("Write DPCD_00107 Fail!!!\n");
	return false;
}

void mtk_dp_video_mute(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	DP_FUNC("encoder:%d, enable:%d\n", encoder_id, enable);

	mtk_dp->info[encoder_id].video_mute = (mtk_dp->info[encoder_id].set_video_mute) ?
		true : enable;

	if (enable) {
		WRITE_BYTE_MASK(mtk_dp,
				REG_3000_DP_ENCODER0_P0 + reg_offset,
			BIT(3) | BIT(2),
			BIT(3) | BIT(2));
		/* Video mute enable */
		mtk_dp_atf_call(DP_ATF_VIDEO_UNMUTE, 1);
	} else {
		WRITE_BYTE_MASK(mtk_dp,
				REG_3000_DP_ENCODER0_P0 + reg_offset,
			BIT(3),
			BIT(3) | BIT(2));
		/* [3] Sw ov Mode [2] mute value */
		mtk_dp_atf_call(DP_ATF_VIDEO_UNMUTE, 0);
	}
	WRITE_BYTE_MASK(mtk_dp, 0x402C, 0, BIT(4));
	WRITE_BYTE_MASK(mtk_dp, 0x402C, 1, BIT(4));
}

void mtk_dp_video_mute_sw(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	DP_FUNC("encoder:%d, enable:%d\n", encoder_id, enable);

	if (enable)
		/* Video mute enable */
		WRITE_BYTE_MASK(mtk_dp, REG_304C_DP_ENCODER0_P0 + reg_offset, BIT(2), BIT(2));
	else
		/* [3] Sw ov Mode [2] mute value */
		WRITE_BYTE_MASK(mtk_dp, REG_304C_DP_ENCODER0_P0 + reg_offset, 0, BIT(2));
}

void mtk_dp_audio_mute(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	DP_FUNC();

	mtk_dp->info[encoder_id].audio_mute =
		(mtk_dp->info[encoder_id].set_audio_mute) ? true : enable;

	if (enable) {
		WRITE_2BYTE_MASK(mtk_dp,
				 REG_3030_DP_ENCODER0_P0 + reg_offset,
				 VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK,
			VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK);

		WRITE_2BYTE_MASK(mtk_dp,
				 REG_3030_DP_ENCODER0_P0 + reg_offset,
				 VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK,
			VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK);

		WRITE_BYTE_MASK(mtk_dp,
				REG_3088_DP_ENCODER0_P0 + reg_offset,
			0x0, AU_EN_DP_ENCODER0_P0_FLDMASK);
		WRITE_BYTE(mtk_dp,
			   REG_30A4_DP_ENCODER0_P0 + reg_offset, 0x00);

		/* a fifo reset */
		WRITE_2BYTE_MASK(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset, BIT(9), BIT(9));
		WRITE_2BYTE_MASK(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset, 0x0, BIT(9));
	} else {
		WRITE_2BYTE_MASK(mtk_dp,
				 REG_3030_DP_ENCODER0_P0 + reg_offset, 0x00,
			VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK);

		WRITE_BYTE_MASK(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset,
				AU_EN_DP_ENCODER0_P0_FLDMASK,
			AU_EN_DP_ENCODER0_P0_FLDMASK);
		WRITE_BYTE(mtk_dp,
			   REG_30A4_DP_ENCODER0_P0 + reg_offset, 0x0F);
	}
}

void mtk_dp_sdp_set_asp_count_init(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u16 down_asp = 0x0000;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	mtk_dp->info[encoder_id].dp_output_timing.hbk =
		mtk_dp->info[encoder_id].dp_output_timing.htt -
		mtk_dp->info[encoder_id].dp_output_timing.hde;

	if (mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate > 0) {
		if (mtk_dp->training_info.link_rate <= DP_LINK_RATE_HBR3)
			down_asp =
				mtk_dp->info[encoder_id].dp_output_timing.hbk *
				mtk_dp->training_info.link_rate * 27 * 250 /
					(mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate) *
					4 / 5;
		else
			down_asp =
				mtk_dp->info[encoder_id].dp_output_timing.hbk *
				mtk_dp->training_info.link_rate * 10 * 1000 / 32 * 250 /
					(mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate) *
					4 / 5;
	}

	/* [11 : 0] reg_sdp_down_asp_cnt_init */
	WRITE_2BYTE_MASK(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset,
			 down_asp << SDP_DOWN_ASP_CNT_INIT_DP_ENCODER1_P0_FLDMASK_POS,
		SDP_DOWN_ASP_CNT_INIT_DP_ENCODER1_P0_FLDMASK);
}

void mtk_dp_sdp_set_down_cnt_init(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				  u16 sram_read_start)
{
	u32 sdp_down_cnt = 0;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* sram_read_start * lane_cnt * 2(pixelperaddr) * link_rate / pixel_clock * 0.8(margin) */
	sdp_down_cnt = (u32)(sram_read_start * mtk_dp->training_info.link_lane_count * 2
			* mtk_dp->training_info.link_rate * 2700 * 8)
			/ mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate;

	if (mtk_dp->info[encoder_id].format == DP_PIXELFORMAT_YUV420)
		sdp_down_cnt = sdp_down_cnt / 2;

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_1LANE:
		sdp_down_cnt = (sdp_down_cnt > 0x1E) ? sdp_down_cnt : 0x1E;
		break;

	case DP_2LANE:
		sdp_down_cnt = (sdp_down_cnt > 0x14) ? sdp_down_cnt : 0x14;
		break;

	case DP_4LANE:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;

	default:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;
	}

	DP_DBG("pixcel_rate:%llu sdp_down_cnt:%x\n",
	       mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate, sdp_down_cnt);

	/* [11 : 0]REG_sdp_down_cnt_init */
	WRITE_2BYTE_MASK(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset, sdp_down_cnt, 0x0FFF);
}

void mtk_dp_sdp_set_down_cnt_init_in_hblanking(struct mtk_dp *mtk_dp,
					       const enum dp_encoder_id encoder_id)
{
	u32 sdp_down_cnt;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* hblank * link_rate / pixel_clock * 0.8(margin) / 4(1T4B) */
	sdp_down_cnt = (u32)((mtk_dp->info[encoder_id].dp_output_timing.htt -
	mtk_dp->info[encoder_id].dp_output_timing.hde)
			* mtk_dp->training_info.link_rate * 2700 * 2)
			/ mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate;

	DP_MSG("htt:%d, hde:%d, link_rate:%d, pixcel_rate%llu, color_format:%d\n",
	       mtk_dp->info[encoder_id].dp_output_timing.htt,
		mtk_dp->info[encoder_id].dp_output_timing.hde,
		mtk_dp->training_info.link_rate,
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate,
		mtk_dp->info[encoder_id].format);

	if (mtk_dp->info[encoder_id].format == DP_PIXELFORMAT_YUV420)
		sdp_down_cnt = sdp_down_cnt / 2;

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_1LANE:
		sdp_down_cnt = (sdp_down_cnt > 0x1E) ? sdp_down_cnt : 0x1E;
		break;

	case DP_2LANE:
		sdp_down_cnt = (sdp_down_cnt > 0x14) ? sdp_down_cnt : 0x14;
		break;

	case DP_4LANE:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;
	default:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;
	}
	DP_MSG("sdp_down_cnt_blank:%x\n", sdp_down_cnt);

	/* [11 : 0]REG_sdp_down_cnt_init_in_hblank */
	WRITE_2BYTE_MASK(mtk_dp, REG_3364_DP_ENCODER1_P0 + reg_offset, sdp_down_cnt, 0x0FFF);
}

void mtk_dp_tu_set_sram_rd_start(struct mtk_dp *mtk_dp,
				 const enum dp_encoder_id encoder_id, u16 val)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* [5:0]video sram start address */
	WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + reg_offset, val, 0x3F);
}

void mtk_dp_tu_set_encoder(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset, BIT(7), BIT(7));
	WRITE_2BYTE(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset, 0x2020);
	WRITE_2BYTE_MASK(mtk_dp, REG_3364_DP_ENCODER1_P0 + reg_offset, 0x2020, 0x0FFF);
	WRITE_BYTE_MASK(mtk_dp, REG_3300_DP_ENCODER1_P0 + 1 + reg_offset, 0x02, BIT(1) | BIT(0));
	WRITE_BYTE_MASK(mtk_dp, REG_3364_DP_ENCODER1_P0 + 1 + reg_offset, 0x40, 0x70);
}

void mtk_dp_tu_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	int tu_size = 0;
	int n_value = 0;
	int f_value = 0;
	int pixcel_rate = 0;
	u8 color_bpp;
	u16 sram_read_start = 0;

	color_bpp = mtk_dp_color_get_bpp(mtk_dp->info[encoder_id].format,
					 mtk_dp->info[encoder_id].depth);
	pixcel_rate = mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate / 1000;
	tu_size = (640 * (pixcel_rate) * color_bpp) /
			(mtk_dp->training_info.link_rate * 27 *
				mtk_dp->training_info.link_lane_count * 8);

	n_value = tu_size / 10;
	f_value = tu_size - n_value * 10;

	DP_MSG("tu_size:%d\n", tu_size);
	if (mtk_dp->training_info.link_lane_count > 0) {
		sram_read_start = mtk_dp->info[encoder_id].dp_output_timing.hde /
			(mtk_dp->training_info.link_lane_count * 4 * 2 * 2);
		sram_read_start =
			(sram_read_start < DP_TBC_BUF_ReadStartAdrThrd) ?
			sram_read_start : DP_TBC_BUF_ReadStartAdrThrd;
		mtk_dp_tu_set_sram_rd_start(mtk_dp, encoder_id, sram_read_start);
	}

	mtk_dp_tu_set_encoder(mtk_dp, encoder_id);
	mtk_dp_audio_sample_arrange(mtk_dp, encoder_id, true);
	mtk_dp_sdp_set_down_cnt_init_in_hblanking(mtk_dp, encoder_id);
	mtk_dp_sdp_set_down_cnt_init(mtk_dp, encoder_id, sram_read_start);
	mtk_dp_sdp_set_asp_count_init(mtk_dp, encoder_id);
}

bool mtk_dp_swingt_set_pre_emphasis(struct mtk_dp *mtk_dp,
				    enum dp_lane_num lane_num,
	enum dp_swing_num swing_level,
	enum dp_preemphasis_num pre_emphasis_level)
{
	DP_DBG("lane:%d, set Swing:0x%x, Emp:0x%x\n",
	       lane_num, swing_level, pre_emphasis_level);

	switch (lane_num) {
	case DP_LANE0:
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	case DP_LANE1:
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	case DP_LANE2:
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	case DP_LANE3:
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	default:
		DP_ERR("lane number is error\n");
		return false;
	}

	return true;
}

bool mtk_dp_swingt_reset_pre_emphasis(struct mtk_dp *mtk_dp)
{
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);

	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			     (0x0  << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
			 DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			     (0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
			 DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);

	return true;
}

void mtk_dp_set_efuse_value(struct mtk_dp *mtk_dp)
{
	u32 efuse = 0;
	/* get_devinfo_with_index(114); */

	DP_DBG("DP efuse(0x11C101B8):0x%x\n", efuse);
	DP_MSG("DP lane:0x%x\n", READ_BYTE(mtk_dp, REG_3000_DP_ENCODER0_P0));

	if (efuse) {
		WRITE_4BYTE_MASK(mtk_dp, 0x0008, efuse >> 1, GENMASK(23, 20));
		WRITE_4BYTE_MASK(mtk_dp, 0x0000, efuse, GENMASK(20, 16));
		WRITE_4BYTE_MASK(mtk_dp, 0x0104, efuse, GENMASK(15, 12));
		WRITE_4BYTE_MASK(mtk_dp, 0x0104, efuse << 8, GENMASK(19, 16));
		WRITE_4BYTE_MASK(mtk_dp, 0x0204, efuse << 8, GENMASK(15, 12));
		WRITE_4BYTE_MASK(mtk_dp, 0x0204, efuse << 16, GENMASK(19, 16));
	}
}

void mtk_dp_set_video_interlance(struct mtk_dp *mtk_dp,
				 const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable) {
		WRITE_BYTE_MASK(mtk_dp, REG_3030_DP_ENCODER0_P0 + 1 + reg_offset,
				BIT(6) | BIT(5), BIT(6) | BIT(5));
		DP_MSG("DP imode force-ov\n");
	} else {
		WRITE_BYTE_MASK(mtk_dp, REG_3030_DP_ENCODER0_P0 + 1 + reg_offset,
				BIT(6), BIT(6) | BIT(5));
		DP_MSG("DP pmode force-ov\n");
	}
}

void mtk_dp_set_lane_count(struct mtk_dp *mtk_dp, const enum dp_lane_count lane_count)
{
	const u8 value = lane_count >> 1;
	enum dp_encoder_id encoder_id;
	u32 reg_offset;

	if (value == 0) {
		WRITE_BYTE_MASK(mtk_dp, REG_35F0_DP_TRANS_P0, 0, BIT(3) | BIT(2));
	} else if (value < mtk_dp->training_info.max_link_lane_count) {
		WRITE_BYTE_MASK(mtk_dp, REG_35F0_DP_TRANS_P0, BIT(3), BIT(3) | BIT(2));
	} else {
		DP_MSG("Un-expected lane count:%d\n", lane_count);
		return;
	}

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset = DP_REG_OFFSET(encoder_id);

		WRITE_BYTE_MASK(mtk_dp, REG_3000_DP_ENCODER0_P0 + reg_offset,
				value << LANE_NUM_DP_ENCODER0_P0_FLDMASK_POS,
		LANE_NUM_DP_ENCODER0_P0_FLDMASK);
	}

	WRITE_BYTE_MASK(mtk_dp, REG_34A4_DP_TRANS_P0,
			value << LANE_NUM_DP_TRANS_P0_FLDMASK_POS,
		LANE_NUM_DP_TRANS_P0_FLDMASK);
}

void mtk_dp_set_training_pattern(struct mtk_dp *mtk_dp, int value)
{
	DP_MSG("Set Train Pattern:0x%x\n", value);

	if (value <= DP_TPS4) {
		if (value == DP_TPS1) /* if Set TPS1 */
			mtk_dp_phy_set_idle_pattern(mtk_dp, false);

		WRITE_BYTE_MASK(mtk_dp, (REG_3400_DP_TRANS_P0 + 1), value, GENMASK(7, 4));
	}
	mdelay(20);
}

void mtk_dp_set_enhanced_frame_mode(struct mtk_dp *mtk_dp, bool enable)
{
	enum dp_encoder_id encoder_id;
	u32 reg_offset;

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset = DP_REG_OFFSET(encoder_id);

		if (enable)
			/* [4] enhanced_frame_mode [1 : 0] lane_num */
			WRITE_BYTE_MASK(mtk_dp, REG_3000_DP_ENCODER0_P0 + reg_offset,
					BIT(4), BIT(4));
		else
			/* [4] enhanced_frame_mode [1 : 0] lane_num */
			WRITE_BYTE_MASK(mtk_dp, REG_3000_DP_ENCODER0_P0 + reg_offset, 0, BIT(4));
	}
}

void mtk_dp_set_scramble(struct mtk_dp *mtk_dp, bool  enable)
{
	if (enable)
		WRITE_BYTE_MASK(mtk_dp, REG_3404_DP_TRANS_P0, BIT(0), BIT(0));
	else
		WRITE_BYTE_MASK(mtk_dp, REG_3404_DP_TRANS_P0, 0, BIT(0));
}

void mtk_dp_set_misc(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u8 format, depth;
	union dp_misc DP_MISC;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	format = mtk_dp->info[encoder_id].format;
	depth = mtk_dp->info[encoder_id].depth;

	/* MISC 0/1 refernce to spec 1.4a p143 Table 2-96 */
	/* MISC0[7:5] color depth */
	switch (depth) {
	case DP_COLOR_DEPTH_6BIT:
	case DP_COLOR_DEPTH_8BIT:
	case DP_COLOR_DEPTH_10BIT:
	case DP_COLOR_DEPTH_12BIT:
	case DP_COLOR_DEPTH_16BIT:
	default:
		DP_MISC.misc.color_depth = depth;
		break;
	}

	/* MISC0[3]: 0->RGB, 1->YUV */
	/* MISC0[2:1]: 01b->4:2:2, 10b->4:4:4 */
	switch (format) {
	case DP_PIXELFORMAT_YUV444:
		DP_MISC.misc.color_format = 0x2;
		DP_MISC.misc.spec_def1 = 0x1;
		break;

	case DP_PIXELFORMAT_YUV422:
		DP_MISC.misc.color_format = 0x1;
		DP_MISC.misc.spec_def1 = 0x1;
		break;

	case DP_PIXELFORMAT_YUV420:
		/* not support */
		break;

	case DP_PIXELFORMAT_RAW:
		DP_MISC.misc.color_format = 0x1;
		DP_MISC.misc.spec_def2 = 0x1;
		break;
	case DP_PIXELFORMAT_Y_ONLY:
		DP_MISC.misc.color_format = 0x0;
		DP_MISC.misc.spec_def2 = 0x1;
		break;

	case DP_COLOR_FORMAT_RGB_444:
	default:
		DP_MISC.misc.color_format = 0x0;
		DP_MISC.misc.spec_def2 = 0x0;
		break;
	}

	WRITE_BYTE_MASK(mtk_dp, REG_3034_DP_ENCODER0_P0 + reg_offset, DP_MISC.misc_raw[0], 0xFE);
	WRITE_BYTE_MASK(mtk_dp, REG_3034_DP_ENCODER0_P0 + 1 + reg_offset,
			DP_MISC.misc_raw[1], 0xFF);
}

void mtk_dp_set_output_timing(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	if (mtk_dp->info[encoder_id].dp_output_timing.video_ip_mode == DP_VIDEO_INTERLACE)
		mtk_dp_set_video_interlance(mtk_dp, encoder_id, true);
	else
		mtk_dp_set_video_interlance(mtk_dp, encoder_id, false);

	mtk_dp_msa_set(mtk_dp, encoder_id);
}

void mtk_dp_set_dp_out(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	mtk_dp_msa_enable_bypass(mtk_dp, encoder_id, false);
	mtk_dp_set_output_timing(mtk_dp, encoder_id);
	mtk_dp_mn_calculate(mtk_dp, encoder_id);

	switch (mtk_dp->info[encoder_id].input_src) {
	case DP_SRC_PG:
		mtk_dp_pg_enable(mtk_dp, encoder_id, true);
		mtk_dp_mvid_set(mtk_dp, encoder_id, false);
		DP_MSG("Set Pattern Gen output\n");
		break;

	case DP_SRC_DPINTF:
		mtk_dp_pg_enable(mtk_dp, encoder_id, false);
		DP_MSG("Set dpintf output\n");
		break;

	default:
		mtk_dp_pg_enable(mtk_dp, encoder_id, true);
		break;
	}

	mtk_dp_mvid_renew(mtk_dp, encoder_id);
	mtk_dp_tu_set(mtk_dp, encoder_id);
}

void mtk_dp_verify_clock(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u64 m, n, ls_clk, pix_clk;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	m = READ_4BYTE(mtk_dp, REG_33C8_DP_ENCODER1_P0 + reg_offset);
	n = 0x8000;
	ls_clk = mtk_dp->training_info.link_rate;
	ls_clk *= 27;

	pix_clk = m * ls_clk * 1000 / n;
	DP_MSG("Encoder:%d, DP calc pixel clock:%llu Hz, dp_intf clock:%lluHz\n",
	       encoder_id, pix_clk, pix_clk / 4);
}

void mtk_dp_video_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id, bool enable)
{
	DP_MSG("Output Video:%s\n", enable ? "enable" : "disable");

	if (enable) {
		mtk_dp_set_dp_out(mtk_dp, encoder_id);
		mtk_dp_verify_clock(mtk_dp, encoder_id);
	}
}

void mtk_dp_video_config(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	struct dp_timing_parameter *dp_timing = &mtk_dp->info[encoder_id].dp_output_timing;
	u32 mvid = 0;
	bool overwrite = false;
	struct videomode vm = {0};

	if (!mtk_dp->dp_ready) {
		DP_ERR("%s, DP is not ready\n", __func__);
		return;
	}

	mtk_dp_mn_overwrite(mtk_dp, encoder_id, false, 0x0, 0x8000);

	if (fake_cable_in) {
		if (mtk_dp->info[encoder_id].resolution == SINK_1280_720) {
			/* patch */
			switch (mtk_dp->training_info.link_rate) {
			case DP_LINK_RATE_RBR:
				mvid = 0x3AAB;
				break;
			case DP_LINK_RATE_HBR:
				mvid = 0x2333;
				break;
			case DP_LINK_RATE_HBR2:
				mvid = 0x1199;
				break;
			case DP_LINK_RATE_HBR3:
				mvid = 0xBBB;
				break;
			}
			overwrite = true;
		}
		mtk_dp->info[encoder_id].depth = fake_bpc;
	}

	vm.hactive = mtk_dp->mode[encoder_id].hdisplay;
	vm.hfront_porch = mtk_dp->mode[encoder_id].hsync_start - mtk_dp->mode[encoder_id].hdisplay;
	vm.hsync_len = mtk_dp->mode[encoder_id].hsync_end - mtk_dp->mode[encoder_id].hsync_start;
	vm.hback_porch = mtk_dp->mode[encoder_id].htotal - mtk_dp->mode[encoder_id].hsync_end;
	vm.vactive = mtk_dp->mode[encoder_id].vdisplay;
	vm.vfront_porch = mtk_dp->mode[encoder_id].vsync_start - mtk_dp->mode[encoder_id].vdisplay;
	vm.vsync_len = mtk_dp->mode[encoder_id].vsync_end - mtk_dp->mode[encoder_id].vsync_start;
	vm.vback_porch = mtk_dp->mode[encoder_id].vtotal - mtk_dp->mode[encoder_id].vsync_end;
	vm.pixelclock = mtk_dp->mode[encoder_id].clock * 1000;

	dp_timing->frame_rate = mtk_dp->mode[encoder_id].clock * 1000 /
		mtk_dp->mode[encoder_id].htotal / mtk_dp->mode[encoder_id].vtotal;
	dp_timing->htt = mtk_dp->mode[encoder_id].htotal;
	dp_timing->hbp = vm.hback_porch;
	dp_timing->hsw = vm.hsync_len;
	dp_timing->hsp = 1; /* todo */
	dp_timing->hfp = vm.hfront_porch;
	dp_timing->hde = vm.hactive;
	dp_timing->vtt = mtk_dp->mode[encoder_id].vtotal;
	dp_timing->vbp = vm.vback_porch;
	dp_timing->vsw = vm.vsync_len;
	dp_timing->vsp = 1; /* todo */
	dp_timing->vfp = vm.vfront_porch;
	dp_timing->vde = vm.vactive;

	if (mtk_dp->info[encoder_id].resolution == SINK_3840_2160) {
		/* patch for 4k@60 with DSC 3 times compress */
		switch (mtk_dp->training_info.link_rate) {
		case DP_LINK_RATE_HBR3:
			mvid = 0x5DDE;
			break;
		case DP_LINK_RATE_HBR2:
			mvid = 0x8CCD;
			break;
		}
		overwrite = true;
	}

	if (mtk_dp->dsc_enable)
		mtk_dp_mn_overwrite(mtk_dp, encoder_id, overwrite, mvid, 0x8000);

	/* interlace not support */
	dp_timing->video_ip_mode = DP_VIDEO_PROGRESSIVE;
	mtk_dp_msa_set(mtk_dp, encoder_id);

	mtk_dp_set_misc(mtk_dp, encoder_id);
	if (mtk_dp->info[encoder_id].pattern_gen)
		mtk_dp_pg_type_sel(mtk_dp, encoder_id,
				   DP_PG_VERTICAL_COLOR_BAR,
			DP_PG_PURECOLOR_BLUE,
			0xFFF,
			DP_PG_LOCATION_ALL);

	if (!mtk_dp->dsc_enable) {
		mtk_dp_color_set_depth(mtk_dp, encoder_id, mtk_dp->info[encoder_id].depth);
		mtk_dp_color_set_format(mtk_dp, encoder_id, mtk_dp->info[encoder_id].format);
	} else {
		mtk_dp_dsc_pps_send(encoder_id, pps_4k60);
		mtk_dp_dsc_enable(mtk_dp, encoder_id, true);
	}
}

void mtk_dp_deinit(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	mtk_dp_video_mute(mtk_dp, encoder_id, true);
	mtk_dp_audio_mute(mtk_dp, encoder_id, true);
	mtk_dp_video_mute_sw(mtk_dp, encoder_id, true);

	mtk_dp->training_info.check_cap_times = 0;
	mtk_dp->video_enable = false;
	mtk_dp->dp_ready = false;
	mtk_dp_phy_set_idle_pattern(mtk_dp, true);
	if (mtk_dp->has_fec) {
		mtk_dp_fec_enable(mtk_dp, false);
		mtk_dp->has_fec = false;
	}

	kfree(mtk_dp->edid);
	mtk_dp->edid = NULL;
}

void mtk_dp_stop_sent_sdp(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u8 pkg_type;

	for (pkg_type = DP_SDP_PKG_ACM ; pkg_type < DP_SDP_PKG_MAX_NUM;
		pkg_type++)
		mtk_dp_spkg_sdp(mtk_dp, encoder_id, false, pkg_type, NULL, NULL);

	mtk_dp_spkg_vsc_ext_vesa(mtk_dp, encoder_id, false, 0x00, NULL);
	mtk_dp_spkg_vsc_ext_cea(mtk_dp, encoder_id, false, 0x00, NULL);
}

u8 mtk_dp_get_sink_count(struct mtk_dp *mtk_dp)
{
	u8 tmp = 0;

	if (mtk_dp->training_info.sink_ext_cap_en)
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002, &tmp, 0x1);
	else
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, &tmp, 0x1);

	DP_MSG("sink count:%d\n", DP_GET_SINK_COUNT(tmp));
	return DP_GET_SINK_COUNT(tmp);
}

bool mtk_dp_check_sink_lock(struct mtk_dp *mtk_dp, u8 *dpcd_20x, u8 *dpcd_200c)
{
	bool locked = true;

	if (mtk_dp->training_info.sink_ext_cap_en) {
		switch (mtk_dp->training_info.link_lane_count) {
		case DP_1LANE:
			if ((dpcd_200c[0] & 0x07) != 0x07) {
				locked = false;
				DP_MSG("1L Lose LCOK\n");
			}
			break;
		case DP_2LANE:
			if ((dpcd_200c[0] & 0x77) != 0x77) {
				locked = false;
				DP_MSG("2L Lose LCOK\n");
			}
			break;
		case DP_4LANE:
			if (dpcd_200c[0] != 0x77 || dpcd_200c[1] != 0x77) {
				locked = false;
				DP_MSG("4L Lose LCOK\n");
			}
			break;
		}

		if ((dpcd_200c[2] & BIT(0)) == 0) {
			locked = false;
			DP_MSG("Interskew Lose LCOK\n");
		}
	} else {
		switch (mtk_dp->training_info.link_lane_count) {
		case DP_1LANE:
			if ((dpcd_20x[2] & 0x07) != 0x07) {
				locked = false;
				DP_MSG("1L Lose LCOK\n");
			}
			break;
		case DP_2LANE:
			if ((dpcd_20x[2] & 0x77) != 0x77) {
				locked = false;
				DP_MSG("2L Lose LCOK\n");
			}
			break;
		case DP_4LANE:
			if ((dpcd_20x[2] != 0x77 || dpcd_20x[3] != 0x77)) {
				locked = false;
				DP_MSG("4L Lose LCOK\n");
			}
			break;
		}

		if ((dpcd_20x[4] & BIT(0)) == 0) {
			locked = false;
			DP_MSG("Interskew Lose LCOK\n");
		}
	}

	if (!locked) {
		if (mtk_dp->dp_ready)
			mtk_dp->training_state = DP_TRAINING_STATE_TRAINING;
	}

	return locked;
}

void mtk_dp_check_sink_esi(struct mtk_dp *mtk_dp,
			   const enum dp_encoder_id encoder_id, u8 *dpcd_20x, u8 *dpcd_2002)
{
	u8 tmp;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	bool handled = false;
#endif

	if ((dpcd_20x[0x1] & BIT(1)) || (dpcd_2002[0x1] & BIT(1))) {
#if (DP_AUTO_TEST_ENABLE == 0x1)
		if (!mtk_dp_phy_auto_test(mtk_dp, DP_ENCODER_ID_0,
					  dpcd_20x[0x1] | dpcd_2002[0x1])) {
			if (mtk_dp->training_state > DP_TRAINING_STATE_TRAINING_PRE)
				mtk_dp->training_state =
					DP_TRAINING_STATE_TRAINING_PRE;
		}
#endif
	}

	if (dpcd_20x[0x1] & BIT(0)) { /* not support, clrear it. */
		tmp = BIT(0);
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &tmp, 0x1);
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	if (mtk_dp->is_mst_start) {
		if (dpcd_2002[0x1] & (BIT(4) | BIT(5))) {
			/* BIT(4):DOWN_REP_MSG_RDY; BIT(5): UP_REQ_MSG_RDY */
			mtk_drm_dp_mst_hpd_irq(&mtk_dp->mtk_mgr, dpcd_2002, &handled);
			tmp = (dpcd_2002[0x1] & (BIT(4) | BIT(5)));
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_02003, &tmp, 0x1);
		} else if (dpcd_20x[0x1] & (BIT(4) | BIT(5))) {
			/* BIT(4):DOWN_REP_MSG_RDY; BIT(5): UP_REQ_MSG_RDY */
			mtk_drm_dp_mst_hpd_irq(&mtk_dp->mtk_mgr, dpcd_20x, &handled);
			tmp = (dpcd_20x[0x1] & (BIT(4) | BIT(5)));
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &tmp, 0x1);
		}
	}
#endif

	if (dpcd_20x[0x1] & BIT(0)) { /* not support, clrear it */
		tmp = BIT(0);
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &tmp, 0x1);
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	if (dpcd_20x[0x4] & BIT(6)) {
		/* DOWNSTREAM_PORT_STATUS_CHANGED */
		if (mtk_dp->training_state > DP_TRAINING_STATE_TRAINING) {
			mtk_dp->training_state = DP_TRAINING_STATE_CHECKCAP;
			DP_MSG("Rx Link Status Change!!\n");
			mtk_dp_mst_drv_video_mute_all(mtk_dp);
		}
	}
#endif
}

bool mtk_dp_hpd_get_pin_level(struct mtk_dp *mtk_dp)
{
	u8 ret = ((READ_2BYTE(mtk_dp, REG_364C_AUX_TX_P0) &
		HPD_STATUS_AUX_TX_P0_FLDMASK) >>
		HPD_STATUS_AUX_TX_P0_FLDMASK_POS);

	return ret;
}

bool mtk_dp_check_sink_cap(struct mtk_dp *mtk_dp)
{
	u8 tmp[0x10];

	if (!mtk_dp_hpd_get_pin_level(mtk_dp))
		return false;

	memset(tmp, 0x0, sizeof(tmp));

	tmp[0x0] = 0x1;
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, tmp, 0x1);
	mdelay(2);

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00000, tmp, 0x10);

	mtk_dp->training_info.sink_ext_cap_en = (tmp[0x0E] & BIT(7)) ?
		true : false;
	if (mtk_dp->training_info.sink_ext_cap_en)
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02200, tmp, 0x10);

	mtk_dp->training_info.dpcd_rev = tmp[0x0];
	DP_MSG("SINK DPCD version:0x%x\n", mtk_dp->training_info.dpcd_rev);

	memcpy(mtk_dp->rx_cap, tmp, 0x10);
	mtk_dp->rx_cap[0xe] &= 0x7F;

	if (mtk_dp->training_info.dpcd_rev >= 0x14) {
		mtk_dp_fec_ready(mtk_dp, FEC_BIT_ERROR_COUNT);
		mtk_dp_dsc_support(mtk_dp);
	}

#if ENABLE_DP_FIX_TPS2
	mtk_dp->training_info.tps3_support = 0;
	mtk_dp->training_info.tps4_support = 0;
#else
	mtk_dp->training_info.tps3_support = (tmp[0x2] & BIT(6)) >> 0x6;
	mtk_dp->training_info.tps4_support = (tmp[0x3] & BIT(7)) >> 0x7;
#endif
	mtk_dp->training_info.dwn_strm_port_present =
			(tmp[0x5] & BIT(0));

#if (ENABLE_DP_SSC_OUTPUT == 0x1)
	if ((tmp[0x3] & BIT(0)) == 0x1) {
		mtk_dp->training_info.sink_ssc_en = true;
		DP_MSG("SINK SUPPORT SSC\n");
	} else {
		mtk_dp->training_info.sink_ssc_en = false;
		DP_MSG("SINK NOT SUPPORT SSC\n");
	}
#endif

#if ENABLE_DP_SSC_FORCEON
	DP_MSG("FORCE SSC ON\n");
	mtk_dp->training_info.sink_ssc_en = true;
#endif

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00021, tmp, 0x1);
	mtk_dp->training_info.dp_mst_cap = (tmp[0x0] & BIT(0));
	mtk_dp->training_info.dp_mst_branch = false;

	if (mtk_dp->training_info.dp_mst_cap == BIT(0)) {
		if (mtk_dp->training_info.dwn_strm_port_present == 0x1)
			mtk_dp->training_info.dp_mst_branch = true;

		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02003, tmp, 0x1);

		if (tmp[0x0] != 0x0)
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_02003,
					  tmp, 0x1);
	}

	DP_MSG("mst_cap:%d, mst_branch:%d, dwn_strm_port_present:%d\n",
	       mtk_dp->training_info.dp_mst_cap,
		mtk_dp->training_info.dp_mst_branch,
		mtk_dp->training_info.dwn_strm_port_present);

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00600, tmp, 0x1);
	if (tmp[0x0] != 0x1) {
		tmp[0x0] = 0x1;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, tmp, 0x1);
	}

	mtk_dp->training_info.sink_count = mtk_dp_get_sink_count(mtk_dp);

	if (!mtk_dp->training_info.dp_mst_branch) {
		u8 dpcd_201 = 0;

		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00201, &dpcd_201, 1);
		if (dpcd_201 & BIT(1)) {
#if (DP_AUTO_TEST_ENABLE == 0x1)
			mtk_dp_phy_auto_test(mtk_dp, DP_ENCODER_ID_0, dpcd_201);
#endif
		}
	}

	return true;
}

void mtk_dp_hotplug_uevent(unsigned int event)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	if (g_mtk_dp->drm_dev) {
		DP_FUNC("notify drm framework hotplug event\n");
		drm_helper_hpd_irq_event(g_mtk_dp->drm_dev);
	} else {
		DP_FUNC("there is no drm dev\n");
	}

	DP_FUNC("fake:%d, event:%d\n", fake_cable_in, event);
	mtk_dp_notify_uevent_user(&dp_notify_data,
				  event > 0 ? DP_NOTIFY_STATE_ACTIVE : DP_NOTIFY_STATE_NO_DEVICE);

	if (g_mtk_dp->info[0].audio_cap != 0) /* todo */
		extcon_set_state_sync(dp_extcon, EXTCON_DISP_HDMI,
				      event > 0 ? true : false);
}

u16 mtk_dp_hpd_get_irq_status(struct mtk_dp *mtk_dp)
{
	return 0xffff & READ_2BYTE(mtk_dp, REG_3608_AUX_TX_P0);
}

void mtk_dp_hpd_interrupt_clr(struct mtk_dp *mtk_dp, u16 status)
{
	DP_FUNC();

	WRITE_2BYTE_MASK(mtk_dp, REG_3668_AUX_TX_P0, status, status);
	WRITE_2BYTE_MASK(mtk_dp, REG_3668_AUX_TX_P0, 0, status);

	DP_MSG("HPD ISR status:0x%x\n", mtk_dp_hpd_get_irq_status(mtk_dp));
}

void mtk_dp_hpd_interrupt_enable(struct mtk_dp *mtk_dp, bool enable)
{
	DP_FUNC();

	WRITE_4BYTE_MASK(mtk_dp, DP_TX_TOP_IRQ_MASK,
			 TRANS_IRQ_MSK | ENCODER_IRQ_MSK,
			TRANS_IRQ_MSK | ENCODER_IRQ_MSK);

	/* [7]:int[6]:Con[5]DisCon[4]No-Use:UnMASK HPD Port */
	if (enable)
		WRITE_2BYTE_MASK(mtk_dp, REG_3660_AUX_TX_P0, 0x0,
				 HPD_DISCONNECT | HPD_CONNECT | HPD_INT_EVNET);
	else
		WRITE_2BYTE_MASK(mtk_dp, REG_3660_AUX_TX_P0,
				 DP_TX_INT_MASK_AUX_TX_P0_FLDMASK,
			DP_TX_INT_MASK_AUX_TX_P0_FLDMASK);
}

void mtk_dp_hpd_detect_setting(struct mtk_dp *mtk_dp)
{
	/* Crystal frequency value for 1us timing normalization */
	/* [7:2]: Integer value */
	/* [1:0]: Fractional value */
	/* 0x30: 12.0us, 0x68: 26us */
	WRITE_2BYTE_MASK(mtk_dp, REG_366C_AUX_TX_P0,
			 0x68 << XTAL_FREQ_AUX_TX_P0_FLDMASK_POS,
			XTAL_FREQ_AUX_TX_P0_FLDMASK);

	/* Adjust Tx reg_hpd_disc_thd to 2ms, it is because of the spec. "HPD pulse" description */
	/* Low Bound: 3'b010 ~ 500us */
	/* Up Bound: 3'b110 ~1.9ms */
	WRITE_2BYTE_MASK(mtk_dp, REG_364C_AUX_TX_P0,
			 (0x32 << HPD_INT_THD_AUX_TX_P0_FLDMASK_POS),
		HPD_INT_THD_AUX_TX_P0_FLDMASK);
}

void mtk_dp_hpd_check_sink_event(struct mtk_dp *mtk_dp)
{
	u8 dpcd_20x[6];
	u8 dpcd_2002[2];
	u8 dpcd_200c[4];
	u8 sink_cnt = 0;
	bool ret;
	enum dp_encoder_id encoder_id;

	memset(dpcd_20x, 0x0, sizeof(dpcd_20x));
	memset(dpcd_2002, 0x0, sizeof(dpcd_2002));
	memset(dpcd_200c, 0x0, sizeof(dpcd_200c));

	if (mtk_dp->training_info.sink_ext_cap_en) {
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002,
				       dpcd_2002, 0x2);
		if (!ret) {
			DP_MSG("Read DPCD_02002 Fail\n");
			return;
		}

		ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C,
				       dpcd_200c, 0x4);
		if (!ret) {
			DP_MSG("Read DPCD_0200C Fail\n");
			return;
		}
	}

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, dpcd_20x, 0x6);
	if (!ret) {
		DP_MSG("Read DPCD200 Fail\n");
		return;
	}

	sink_cnt = mtk_dp_get_sink_count(mtk_dp);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	if (!mtk_dp->is_mst_start)
#endif
	{
		if (sink_cnt != mtk_dp->training_info.sink_count ||
		    (dpcd_200c[0x2] & BIT(6) || dpcd_20x[0x4] & BIT(6))) {
			DP_MSG("New Branch Device Detection!!\n");

			if (!mtk_dp->uevent_to_hwc) {
				mtk_dp->disp_state = DP_DISP_STATE_NONE;
				mtk_dp_hotplug_uevent(0);
				mtk_dp->uevent_to_hwc = true;
			}

			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
				mtk_dp_deinit(mtk_dp, encoder_id);

			mtk_dp->training_info.sink_count = sink_cnt;
			mtk_dp->training_state = DP_TRAINING_STATE_STARTUP;
			mdelay(20);
			return;
		}

		if (sink_cnt == 0) {
			mtk_dp->training_state = DP_TRAINING_STATE_STARTUP;
			mdelay(200);
			return;
		}
	}
	mtk_dp_check_sink_lock(mtk_dp, dpcd_20x, dpcd_200c);
	mtk_dp_check_sink_esi(mtk_dp, DP_ENCODER_ID_0, dpcd_20x, dpcd_2002); /* todo */
}

void mtk_dp_hpd_handle_in_isr(struct mtk_dp *mtk_dp)
{
	bool current_hpd = mtk_dp_hpd_get_pin_level(mtk_dp);

	DP_MSG("current_hpd:0x%x\n", current_hpd);

	if (mtk_dp->training_info.phy_status == HPD_INITIAL_STATE)
		return;

	if ((mtk_dp->training_info.phy_status & (HPD_CONNECT | HPD_DISCONNECT))
		== (HPD_CONNECT | HPD_DISCONNECT)) {
		if (current_hpd)
			mtk_dp->training_info.phy_status &= ~HPD_DISCONNECT;
		else
			mtk_dp->training_info.phy_status &= ~HPD_CONNECT;
	}

	if ((mtk_dp->training_info.phy_status & (HPD_INT_EVNET | HPD_DISCONNECT))
		== (HPD_INT_EVNET | HPD_DISCONNECT)) {
		if (current_hpd)
			mtk_dp->training_info.phy_status &= ~HPD_DISCONNECT;
	}

	/* ignore plug-in --> plug-in event */
	if (mtk_dp->training_info.cable_plug_in)
		mtk_dp->training_info.phy_status &= ~HPD_CONNECT;
	else
		mtk_dp->training_info.phy_status &= ~HPD_DISCONNECT;

	if (mtk_dp->training_info.phy_status & HPD_CONNECT) {
		mtk_dp->training_info.phy_status &= ~HPD_CONNECT;
		mtk_dp->training_info.cable_plug_in = true;
		mtk_dp->training_info.cable_state_change = true;
		mtk_dp->uevent_to_hwc = true;
		mtk_dp->power_on = true;

		DP_MSG("HPD_CON_ISR\n");
	}

	if (mtk_dp->training_info.phy_status & HPD_DISCONNECT) {
		mtk_dp->training_info.phy_status &= ~HPD_DISCONNECT;

		mtk_dp->training_info.cable_plug_in = false;
		mtk_dp->training_info.cable_state_change = true;
		mtk_dp->uevent_to_hwc = true;
		mtk_dp->power_on = false;

		DP_MSG("HPD_DISCON_ISR\n");
	}

	/* handle IRQ in thread */
	if (mtk_dp->training_info.phy_status & HPD_INT_EVNET)
		DP_MSG("****** [DPTX] HPD_INT ****** \r\n\n");
}

void mtk_dp_hpd_isr_event(struct mtk_dp *mtk_dp)
{
	u16 hw_status = mtk_dp_hpd_get_irq_status(mtk_dp);

	if (hw_status != 0)
		DP_MSG("hw status:0x%x\n", hw_status);

	mtk_dp->training_info.phy_status |= hw_status;

	mtk_dp_hpd_handle_in_isr(mtk_dp);

	if (mtk_dp->training_info.cable_state_change)
		DP_MSG("cable_state_change:0x%x, hw_status:%x\n",
		       mtk_dp->training_info.cable_state_change, hw_status);

	if (hw_status)
		mtk_dp_hpd_interrupt_clr(mtk_dp, hw_status);

	if (mtk_dp->training_info.cable_state_change ||
	    (mtk_dp->training_info.phy_status & HPD_INT_EVNET)) {
		DP_MSG("dp_work, hw_status:0x%x\n", hw_status);
		queue_work(mtk_dp->dp_wq, &mtk_dp->dp_work);
	}
}

irqreturn_t mtk_dp_hpd_event(int hpd, void *dev)
{
	struct mtk_dp *mtk_dp = dev;
	u32 int_status;

	int_status = READ_4BYTE(mtk_dp, DP_TX_TOP_IRQ_STATUS);

	/* DP_MSG("int_status = 0x%x\n", int_status); */

	if (int_status & BIT(3))
		WRITE_4BYTE_MASK(mtk_dp, DP_TX_TOP_IRQ_MASK, ENCODER_1_IRQ_MSK, ENCODER_1_IRQ_MSK);

	if (int_status & BIT(2))
		mtk_dp_hpd_isr_event(mtk_dp);

	if (int_status & BIT(1)) {
		WRITE_4BYTE_MASK(mtk_dp, DP_TX_TOP_IRQ_MASK, TRANS_IRQ_MSK, TRANS_IRQ_MSK);
		mtk_dp_hpd_isr_event(mtk_dp);
	}

	if (int_status & BIT(0))
		WRITE_4BYTE_MASK(mtk_dp, DP_TX_TOP_IRQ_MASK, ENCODER_IRQ_MSK, ENCODER_IRQ_MSK);

	return IRQ_HANDLED;
}

void mtk_dp_usbc_hpd(struct mtk_dp *mtk_dp, bool conn)
{
	WRITE_BYTE_MASK(mtk_dp, REG_3414_DP_TRANS_P0, HPD_OVR_EN_DP_TRANS_P0_FLDMASK,
			HPD_OVR_EN_DP_TRANS_P0_FLDMASK);

	if (conn)
		WRITE_BYTE_MASK(mtk_dp, REG_3414_DP_TRANS_P0, HPD_SET_DP_TRANS_P0_FLDMASK,
				HPD_SET_DP_TRANS_P0_FLDMASK);
	else
		WRITE_BYTE_MASK(mtk_dp, REG_3414_DP_TRANS_P0, 0,
				HPD_SET_DP_TRANS_P0_FLDMASK);

	DP_FUNC("REG3414:0x%x\n", READ_BYTE(mtk_dp, REG_3414_DP_TRANS_P0));
}

void mtk_dp_usbc_hpd_event(struct mtk_dp *mtk_dp, u16 status)
{
	mtk_dp->training_info.phy_status |= status;
	DP_MSG("status:0x%x, phy status:0x%x\n", status, mtk_dp->training_info.phy_status);

	mtk_dp_hpd_handle_in_isr(mtk_dp);

	if (mtk_dp->training_info.cable_state_change || status == HPD_INT_EVNET)
		queue_work(mtk_dp->dp_wq, &mtk_dp->dp_work);
}

void mtk_dp_hpd_interrupt_set(int status)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	DP_MSG("%s, status:%d[2:DISCONNECT, 4:CONNECT, 8:IRQ], Power:%d, uevent:%d\n",
	       __func__, status, g_mtk_dp->power_on, g_mtk_dp->uevent_to_hwc);

	/* delay to prevent from slow connecting */
	msleep(500);

	if ((status == HPD_CONNECT && !g_mtk_dp->power_on) ||
	    (status == HPD_DISCONNECT && g_mtk_dp->power_on) ||
		(status == HPD_INT_EVNET && g_mtk_dp->power_on)) {
		if (status == HPD_CONNECT) {
			mtk_dp_usbc_hpd(g_mtk_dp, true);
			g_mtk_dp->power_on = true;
		} else if (status == HPD_DISCONNECT) {
			mtk_dp_usbc_hpd(g_mtk_dp, false);
			g_mtk_dp->power_on = false;
		}

		mtk_dp_usbc_hpd_event(g_mtk_dp, status);
		return;
	}

	if (status == HPD_CONNECT && g_mtk_dp->power_on && g_mtk_dp->uevent_to_hwc) {
		DP_MSG("force send uevent\n");
		mtk_dp_hotplug_uevent(1);
		g_mtk_dp->uevent_to_hwc = false;
	}
}

struct edid *mtk_dp_handle_edid(struct mtk_dp *mtk_dp, struct drm_connector *connector)
{
	/* use cached edid if we have one */
	if (mtk_dp->edid) {
		/* invalid edid */
		if (IS_ERR(mtk_dp->edid))
			return NULL;

		DP_MSG("%s, duplicate edid from mtk_dp->edid\n", __func__);
		return drm_edid_duplicate(mtk_dp->edid);
	}

	DP_MSG("Get edid from RX\n");
	return drm_get_edid(connector, &mtk_dp->aux.ddc);
}

void mtk_dp_init_variable(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	mtk_dp->training_info.dp_version = DP_VER_14;
	mtk_dp->training_info.max_link_rate = DP_SUPPORT_MAX_LINKRATE;
	mtk_dp->training_info.max_link_lane_count = DP_SUPPORT_MAX_LANECOUNT;
	mtk_dp->training_info.sink_ext_cap_en = false;
	mtk_dp->training_info.sink_ssc_en = false;
	mtk_dp->training_info.tps3_support = true;
	mtk_dp->training_info.tps4_support = true;
	mtk_dp->training_info.phy_status = HPD_INITIAL_STATE;
	mtk_dp->training_state = DP_TRAINING_STATE_STARTUP;
	mtk_dp->training_state_pre = DP_TRAINING_STATE_STARTUP;
	mtk_dp->state = DP_STATE_INITIAL;
	mtk_dp->state_pre = DP_STATE_INITIAL;
	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		mtk_dp->info[encoder_id].input_src = DP_SRC_DPINTF;
		mtk_dp->info[encoder_id].format = DP_COLOR_FORMAT_RGB_444;
#if DEPTH_10BIT_PRIORITY
		mtk_dp->info[encoder_id].depth = DP_COLOR_DEPTH_10BIT;
#else
		mtk_dp->info[encoder_id].depth = DP_COLOR_DEPTH_8BIT;
#endif
		if (!mtk_dp->info[encoder_id].pattern_gen)
			mtk_dp->info[encoder_id].resolution = SINK_1920_1080;
		mtk_dp->info[encoder_id].set_audio_mute = false;
		mtk_dp->info[encoder_id].set_video_mute = false;
		memset(&mtk_dp->info[encoder_id].dp_output_timing, 0,
		       sizeof(struct dp_timing_parameter));
		mtk_dp->info[encoder_id].dp_output_timing.frame_rate = 60;
	}
	mtk_dp->power_on = false;
	mtk_dp->dp_ready = false;
	mtk_dp->has_dsc   = false;
	mtk_dp->has_fec   = false;
	mtk_dp->dsc_enable = false;
}

void mtk_dp_initial_setting(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;
	u32 reg_offset;

	WRITE_4BYTE_MASK(mtk_dp, DP_TX_TOP_PWR_STATE,
			 (0x3 << DP_PWR_STATE_FLDMASK_POS), DP_PWR_STATE_FLDMASK);

	WRITE_BYTE(mtk_dp, REG_342C_DP_TRANS_P0, 0x68); /* 26M xtal clock */

	mtk_dp_fec_init_setting(mtk_dp);

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset = DP_REG_OFFSET(encoder_id);
		WRITE_4BYTE_MASK(mtk_dp, REG_31EC_DP_ENCODER0_P0  + reg_offset, BIT(4), BIT(4));
		WRITE_4BYTE_MASK(mtk_dp, REG_304C_DP_ENCODER0_P0  + reg_offset, 0, BIT(8));
		WRITE_4BYTE_MASK(mtk_dp, REG_304C_DP_ENCODER0_P0  + reg_offset, BIT(3), BIT(3));
	}

	/* 31C4[13] : DSC bypass [11]pps bypass */
	WRITE_2BYTE_MASK(mtk_dp, REG_31C4_DP_ENCODER0_P0 + reg_offset,
			 0,
			PPS_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK);

	WRITE_2BYTE_MASK(mtk_dp, REG_31C4_DP_ENCODER0_P0 + reg_offset,
			 0,
			DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK);

	WRITE_2BYTE_MASK(mtk_dp, REG_336C_DP_ENCODER1_P0 + reg_offset,
			 0,
			DSC_BYTE_SWAP_DP_ENCODER1_P0_FLDMASK);
}

void mtk_dp_encoder_reset(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* dp tx encoder reset all sw */
	WRITE_2BYTE_MASK(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset),
			 1 << DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS,
			DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	mdelay(1);

	/* dp tx encoder reset all sw */
	WRITE_2BYTE_MASK(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset),
			 0,
			DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dp_digital_setting(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	mtk_dp_spkg_asp_hb32(mtk_dp, encoder_id, false, DP_SDP_ASP_HB3_AU02CH, 0x0);
	/* Mengkun suggest: disable reg_sdp_down_cnt_new_mode */
	WRITE_BYTE_MASK(mtk_dp, REG_304C_DP_ENCODER0_P0 + reg_offset, 0,
			SDP_DOWN_CNT_NEW_MODE_DP_ENCODER0_P0_FLDMASK);
	/* reg_sdp_asp_insert_in_hblank: default = 1 */
	WRITE_2BYTE_MASK(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset,
			 0x1 << SDP_ASP_INSERT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK_POS,
		SDP_ASP_INSERT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK);

	WRITE_BYTE_MASK(mtk_dp, REG_304C_DP_ENCODER0_P0 + reg_offset, 0,
			VBID_VIDEO_MUTE_DP_ENCODER0_P0_FLDMASK);
	/* MISC0 */
	mtk_dp_color_set_format(mtk_dp, encoder_id, mtk_dp->info[encoder_id].format);

	/* [13 : 12] : = 2b'01 VDE check BS2BS & set min value */
	mtk_dp_color_set_depth(mtk_dp, encoder_id, mtk_dp->info[encoder_id].depth);
	WRITE_4BYTE(mtk_dp, REG_3368_DP_ENCODER1_P0 + reg_offset,
		    (0x1 << 15) |
		(0x4 << BS2BS_MODE_DP_ENCODER1_P0_FLDMASK_POS) |
		(0x1 << SDP_DP13_EN_DP_ENCODER1_P0_FLDMASK_POS) |
		(0x1 << VIDEO_STABLE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS) |
		(0x1 << VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENCODER1_P0_FLDMASK_POS));

	mtk_dp_encoder_reset(mtk_dp, encoder_id);
}

void mtk_dp_digital_sw_reset(struct mtk_dp *mtk_dp)
{
	WRITE_BYTE_MASK(mtk_dp, REG_340C_DP_TRANS_P0 + 1, BIT(5), BIT(5));
	mdelay(1);
	WRITE_BYTE_MASK(mtk_dp, REG_340C_DP_TRANS_P0 + 1, 0, BIT(5));
}

void mtk_dp_analog_power_on_off(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable) {
		WRITE_BYTE_MASK(mtk_dp, DP_TX_TOP_RESET_AND_PROBE, 0, BIT(4));
		usleep_range(10, 11);
		WRITE_BYTE_MASK(mtk_dp, DP_TX_TOP_RESET_AND_PROBE, BIT(4), BIT(4));
	} else {
		WRITE_2BYTE(mtk_dp, TOP_OFFSET, 0x0);
		usleep_range(10, 11);
		PHY_WRITE_2BYTE(mtk_dp, 0x0034, 0x4AA);
		PHY_WRITE_2BYTE(mtk_dp, 0x1040, 0x0);
		PHY_WRITE_2BYTE(mtk_dp, 0x0038, 0x555);
	}
}

void mtk_dp_init_port(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	mtk_dp_phy_set_idle_pattern(mtk_dp, true);
	mtk_dp_init_variable(mtk_dp);

	mtk_dp_fec_enable(mtk_dp, false);
	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_dsc_enable(mtk_dp, encoder_id, false);

	mtk_dp_initial_setting(mtk_dp);
	mtk_dp_aux_setting(mtk_dp);
	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_digital_setting(mtk_dp, encoder_id);

	mtk_dp_analog_power_on_off(mtk_dp, true);
	mtk_dp_phy_setting(mtk_dp);
	mtk_dp_hpd_detect_setting(mtk_dp);

	mtk_dp_digital_sw_reset(mtk_dp);
	mtk_dp_set_efuse_value(mtk_dp);
}

void mtk_dp_vsvoter_set(struct mtk_dp *mtk_dp)
{
	u32 reg, msk, val;

	if (IS_ERR_OR_NULL(mtk_dp->vsv))
		return;

	/* write 1 to set and clr, update reg address */
	reg = mtk_dp->vsv_reg + VS_VOTER_EN_LO_SET;
	msk = mtk_dp->vsv_mask;
	val = mtk_dp->vsv_mask;

	regmap_update_bits(mtk_dp->vsv, reg, msk, val);
	dev_info(mtk_dp->dev, "%s, set voter for vs\n", __func__);
}

void mtk_dp_vsvoter_clr(struct mtk_dp *mtk_dp)
{
	u32 reg, msk, val;

	if (IS_ERR_OR_NULL(mtk_dp->vsv))
		return;

	/* write 1 to set and clr, update reg address */
	reg = mtk_dp->vsv_reg + VS_VOTER_EN_LO_CLR;
	msk = mtk_dp->vsv_mask;
	val = mtk_dp->vsv_mask;

	regmap_update_bits(mtk_dp->vsv, reg, msk, val);
	dev_info(mtk_dp->dev, "%s, clr voter for vs\n", __func__);
}

static int mtk_dp_vsvoter_parse(struct mtk_dp *mtk_dp, struct device_node *node)
{
	struct of_phandle_args args;
	struct platform_device *pdev;
	int ret;

	/* vs vote function is optional */
	if (!of_property_read_bool(node, "mediatek,vs-voter"))
		return 0;

	ret = of_parse_phandle_with_fixed_args(node,
					       "mediatek,vs-voter", 3, 0, &args);
	if (ret)
		return ret;

	pdev = of_find_device_by_node(args.np->child);
	if (!pdev)
		return -ENODEV;

	mtk_dp->vsv = dev_get_regmap(pdev->dev.parent, NULL);
	if (!mtk_dp->vsv)
		return -ENODEV;

	mtk_dp->vsv_reg = args.args[0];
	mtk_dp->vsv_mask = args.args[1];
	mtk_dp->vsv_vers = args.args[2];
	dev_info(mtk_dp->dev, "vsv_reg:0x%x, mask:0x%x, version:%d\n",
		 mtk_dp->vsv_reg, mtk_dp->vsv_mask, mtk_dp->vsv_vers);

	return PTR_ERR_OR_ZERO(mtk_dp->vsv);
}

void mtk_dp_disconnect_release(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		mtk_dp_video_mute(mtk_dp, encoder_id, true);
		mtk_dp_audio_mute(mtk_dp, encoder_id, true);
	}

	mtk_dp_init_variable(mtk_dp);
	mtk_dp_phy_set_idle_pattern(mtk_dp, true);

	if (mtk_dp->has_fec)
		mtk_dp_fec_enable(mtk_dp, false);

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_dsc_enable(mtk_dp, encoder_id, false);

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_stop_sent_sdp(mtk_dp, encoder_id);

	DP_MSG("Power OFF:%d", mtk_dp->power_on);
	mtk_dp_analog_power_on_off(mtk_dp, false);

	mtk_dp_vsvoter_clr(mtk_dp);
}

static inline struct mtk_dp *encoder_to_dp(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct mtk_dp_connector *mtk_connector = NULL;
	struct mtk_dp *mtk_dp = NULL;
	struct drm_device *dev = encoder->dev;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->possible_encoders & drm_encoder_mask(encoder)) {
			mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
			mtk_dp = mtk_connector->mtk_dp;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return mtk_dp;
}

static bool mtk_dp_encoder_mode_fixup(struct drm_encoder *encoder,
				      const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	struct mtk_dp *mtk_dp = encoder_to_dp(encoder);

	if (!mtk_dp) {
		DP_ERR("can not find the mtk dp by the encoder");
		return false;
	}

	return true;
}

static void mtk_dp_encoder_mode_set(struct drm_encoder *encoder,
				    struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted)
{
	int i;
	int encoder_id;
	struct drm_bridge *bridge;
	unsigned int out_bus_format;
	struct drm_bridge_state *bridge_state;
	struct mtk_dp *mtk_dp = encoder_to_dp(encoder);

	if (!mtk_dp) {
		DP_ERR("can not find the mtk dp by the encoder");
		return;
	}

	for (i = 0; i < DP_ENCODER_NUM; i++) {
		if (mtk_dp->mtk_connector[i] && mtk_dp->mtk_connector[i]->encoder == encoder) {
			encoder_id = i;
			drm_mode_copy(&mtk_dp->mode[i], adjusted);

			DP_MSG("[%d] mode set, Htt:%d, Vtt:%d, Hact:%d, Vact:%d, fps:%d, clk:%d\n",
+			       encoder_id, mtk_dp->mode[i].htotal, mtk_dp->mode[i].vtotal,
			mtk_dp->mode[i].hdisplay, mtk_dp->mode[i].vdisplay,
			drm_mode_vrefresh(&mtk_dp->mode[i]), mtk_dp->mode[i].clock);
			break;
		}
	}

	bridge = list_first_entry_or_null(&encoder->bridge_chain,
					  struct drm_bridge, chain_node);
	if (bridge) {
		bridge_state = drm_priv_to_bridge_state(bridge->base.state);
		out_bus_format = bridge_state->output_bus_cfg.format;
		DP_FUNC("[%d] input format 0x%04x, output format 0x%04x\n",
			encoder_id,
			bridge_state->input_bus_cfg.format,
			bridge_state->output_bus_cfg.format);

		if (out_bus_format == MEDIA_BUS_FMT_YUYV8_1X16)
			mtk_dp->info[encoder_id].format = DP_PIXELFORMAT_YUV422;
		else
			mtk_dp->info[encoder_id].format = DP_PIXELFORMAT_RGB;
	}
}

static void mtk_dp_encoder_disable(struct drm_encoder *encoder)
{
	struct mtk_dp *mtk_dp = encoder_to_dp(encoder);
	int i;

	DP_FUNC();

	if (!mtk_dp) {
		DP_ERR("can not find the mtk dp by the encoder");
		return;
	}

	mtk_dp->video_enable = false;
	for (i = 0; i < DP_ENCODER_NUM; i++) {
		mtk_dp_video_enable(mtk_dp, i, false);
		mtk_dp_video_mute(mtk_dp, i, true);
	}
}

static void mtk_dp_encoder_enable(struct drm_encoder *encoder)
{
	struct mtk_dp *mtk_dp = encoder_to_dp(encoder);

	DP_FUNC();

	if (!mtk_dp) {
		DP_ERR("can not find the mtk dp by the encoder");
		return;
	}

	mtk_dp->video_enable = true;
	queue_work(mtk_dp->dp_wq, &mtk_dp->dp_work);
}

static int mtk_dp_encoder_atomic_check(struct drm_encoder *encoder,
				       struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct mtk_dp *mtk_dp = encoder_to_dp(encoder);

	if (!mtk_dp) {
		DP_ERR("can not find the mtk dp by the encoder");
		return -1;
	}

	return 0;
}

static const struct drm_encoder_helper_funcs mtk_dp_encoder_helper_funcs = {
	.mode_fixup = mtk_dp_encoder_mode_fixup,
	.mode_set = mtk_dp_encoder_mode_set,
	.disable = mtk_dp_encoder_disable,
	.enable = mtk_dp_encoder_enable,
	.atomic_check = mtk_dp_encoder_atomic_check,
};

static enum drm_connector_status mtk_dp_connector_detect
	(struct drm_connector *connector, bool force)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;
	enum drm_connector_status ret = connector_status_disconnected;
	u8 sink_count = 0;

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mtk_dp = mtk_connector->mtk_dp;

	if (!mtk_dp->training_info.cable_plug_in)
		return ret;

	if (mtk_dp->training_info.dp_mst_cap) {
		mtk_dp->is_mst_start = true;

		ret = connector_status_connected;
	} else {
		/*
		 * Some dongles still source HPD when they do not connect to any
		 * sink device. To avoid this, we need to read the sink count
		 * to make sure we do connect to sink devices. After this detect
		 * function, we just need to check the HPD connection to check
		 * whether we connect to a sink device.
		 */
		drm_dp_dpcd_readb(&mtk_dp->aux, DP_SINK_COUNT, &sink_count);
		if (DP_GET_SINK_COUNT(sink_count))
			ret = connector_status_connected;
	}

	DP_MSG("detect, connector status:%d", ret);
	return ret;
}

static void mtk_dp_connector_destroy(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;
	int i;

	DP_FUNC();

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mtk_dp = mtk_connector->mtk_dp;

	drm_connector_cleanup(&mtk_connector->connector);

	if (mtk_dp->training_info.dp_mst_cap)
		drm_dp_mst_put_port_malloc(mtk_connector->port);

	for (i = 0; i < DP_ENCODER_NUM; i++) {
		if (mtk_dp->mtk_connector[i] == mtk_connector) {
			DP_MSG("destroy mtk connector[%d]\n", i);
			kfree(mtk_dp->mtk_connector[i]);
			mtk_dp->mtk_connector[i] = NULL;
		}
	}
}

static int
mtk_dp_connector_late_register(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;

	DP_FUNC();

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mtk_dp = mtk_connector->mtk_dp;

	if (connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort)
		mtk_dp->aux.dev = connector->kdev;

	return 0;
}

static void
mtk_dp_connector_early_unregister(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;

	DP_FUNC();

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mtk_dp = mtk_connector->mtk_dp;
}

static const struct drm_connector_funcs mtk_dp_connector_funcs = {
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = mtk_dp_connector_detect,
	.destroy = mtk_dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.late_register = mtk_dp_connector_late_register,
	.early_unregister = mtk_dp_connector_early_unregister,
};

static enum drm_mode_status mtk_dp_connector_mode_valid(struct drm_connector *connector,
							struct drm_display_mode *mode)
{
	u32 bpp;
	u32 rate;
	u32 lane_count_min;
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mtk_dp = mtk_connector->mtk_dp;

	DP_DBG("Htt:%d, Vtt:%d, Hact:%d, Vact:%d, fps:%d, clk:%d\n",
			mode->htotal, mode->vtotal,
			mode->hdisplay, mode->vdisplay,
			drm_mode_vrefresh(mode), mode->clock);

	bpp = connector->display_info.color_formats & DRM_COLOR_FORMAT_YCBCR422 ? 16 : 24;
	lane_count_min = mtk_dp->training_info.link_lane_count;
	rate = drm_dp_bw_code_to_link_rate(mtk_dp->training_info.link_rate) * lane_count_min;

	if (rate * 97 / 100 < (mode->clock * bpp / 8))
		return MODE_CLOCK_HIGH;

	DP_DBG("MODE_OK, Htt:%d, Vtt:%d, Hact:%d, Vact:%d, fps:%d, clk:%d\n",
			mode->htotal, mode->vtotal,
			mode->hdisplay, mode->vdisplay,
			drm_mode_vrefresh(mode), mode->clock);
	return MODE_OK;
}

static int mtk_dp_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;
	int ret, num_modes = 0;

	DP_FUNC();

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mtk_dp = mtk_connector->mtk_dp;

	if (mtk_dp->next_bridge) {
		DP_MSG("getting timing mode from next bridge\n");
		return mtk_dp->next_bridge->funcs->get_modes(mtk_dp->next_bridge,
			&mtk_connector->connector);
	}

	if (mtk_connector->edid)
		return drm_add_edid_modes(&mtk_connector->connector, mtk_connector->edid);

	mtk_connector->edid = drm_get_edid(connector, &mtk_dp->aux.ddc);

	if (!mtk_connector->edid) {
		DP_ERR("Failed to read EDID\n");
		goto fail;
	}

	ret = drm_connector_update_edid_property(&mtk_connector->connector, mtk_connector->edid);
	if (ret) {
		DP_ERR("Failed to update EDID property: %d\n", ret);
		goto fail;
	}

	num_modes = drm_add_edid_modes(&mtk_connector->connector, mtk_connector->edid);
	DP_MSG("num_modes:%d/n", num_modes);

fail:
	return num_modes;
}

static int mtk_dp_connector_atomic_check(struct drm_connector *connector,
					 struct drm_atomic_state *state)
{
	struct mtk_dp_connector *mtk_connector;
	struct drm_dp_mst_topology_mgr *mgr;
	struct drm_connector_state *conn_state;
	struct drm_crtc_state *crtc_state;

	mtk_connector = container_of(connector, struct mtk_dp_connector, connector);
	mgr = &mtk_connector->mtk_dp->mgr;

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (WARN_ON(!conn_state))
		return -ENODEV;

	crtc_state = drm_atomic_get_new_crtc_state(state, conn_state->crtc);
	if (!crtc_state)
		return 0;

	return 0;
}

static const struct drm_connector_helper_funcs mtk_dp_connector_helper_funcs = {
	.get_modes = mtk_dp_connector_get_modes,
	.mode_valid = mtk_dp_connector_mode_valid,
	.atomic_check = mtk_dp_connector_atomic_check,
};

struct drm_connector *mtk_dp_add_connector(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_dp_mst_port *port, const char *path)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_connector *mtk_connector;
	int ret;
	int i;

	DP_FUNC();

	mtk_dp = container_of(mgr, struct mtk_dp, mgr);

	mtk_connector = kzalloc(sizeof(*mtk_connector), GFP_KERNEL);
	if (!mtk_connector) {
		DP_ERR("fail to kzalloc!\n");
		return NULL;
	}

	mtk_connector->mtk_dp = mtk_dp;
	mtk_connector->port = port;

	ret = drm_connector_init(mtk_dp->drm_dev, &mtk_connector->connector,
				 &mtk_dp_connector_funcs, DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		kfree(mtk_connector);
		return NULL;
	}

	for (i = 0; i < DP_ENCODER_NUM; i++) {
		if (!mtk_dp->mtk_connector[i]) {
			mtk_dp->mtk_connector[i] = mtk_connector;
			DP_MSG("add mtk connector[%d]\n", i);
		}
	}

	drm_connector_helper_add(&mtk_connector->connector, &mtk_dp_connector_helper_funcs);

	drm_object_attach_property(&mtk_connector->connector.base,
				   mtk_dp->drm_dev->mode_config.path_property, 0);
	drm_object_attach_property(&mtk_connector->connector.base,
				   mtk_dp->drm_dev->mode_config.tile_property, 0);
	drm_connector_set_path_property(&mtk_connector->connector, path);
	drm_dp_mst_get_port_malloc(port);

	return &mtk_connector->connector;
}

void mtk_dp_connect_attach_encoder(struct mtk_dp *mtk_dp)
{
	struct mtk_dp_connector *mtk_connector;
	int ret;
	u8 i;
	u8 sink_count;
	u8 init_connector_count = 0;
	u8 index;
	struct drm_bridge *bridge;

	DP_FUNC();

	sink_count = mtk_dp_get_sink_count(mtk_dp);

#if ENABLE_SERDES_MST
	/* serdes always report 1 sink count, so set it as 2 here */
	if (mtk_dp->mst_enable)
		sink_count = DP_ENCODER_NUM;
#endif

	if (sink_count > DP_ENCODER_NUM)
		sink_count = DP_ENCODER_NUM;

	for (i = 0; i < sink_count; i++) {
		if (!mtk_dp->mtk_connector[i])
			init_connector_count++;
	}

	for (i = 0; i < DP_ENCODER_NUM; i++) {
		if (!mtk_dp->mtk_connector[i]) {
			index = i;
			break;
		}
	}

	for (i = 0; i < init_connector_count; i++) {
		bridge = devm_drm_of_get_bridge(mtk_dp->dev, mtk_dp->dev->of_node, index, 0);
		if (IS_ERR(bridge)) {
			DP_MSG("can not find bridge[%d, %d]", index, 0);
			return;
		}
		if (!bridge->encoder) {
			DP_MSG("bridge have no encoder[%d, %d]", index, 0);
			return;
		}
		DP_MSG("found dp_intf bridge node:%pOF\n", bridge->of_node);

		mtk_connector = kzalloc(sizeof(*mtk_connector), GFP_KERNEL);
		if (!mtk_connector) {
			DP_ERR("fail to kzalloc!\n");
			return;
		}

		mtk_connector->mtk_dp = mtk_dp;

		ret = drm_connector_init(bridge->dev, &mtk_connector->connector,
					 &mtk_dp_connector_funcs, DRM_MODE_CONNECTOR_DisplayPort);
		if (ret) {
			DP_MSG("failed to init connector:%d\n", ret);
			kfree(mtk_connector);
			return;
		}

		drm_display_info_set_bus_formats(&mtk_connector->connector.display_info,
						 mt8678_output_fmts,
						 ARRAY_SIZE(mt8678_output_fmts));

		mtk_dp->mtk_connector[index] = mtk_connector;
		DP_MSG("init mtk connector[%d]\n", index);

		drm_connector_helper_add(&mtk_connector->connector,
					 &mtk_dp_connector_helper_funcs);
		mtk_connector->connector.polled = DRM_CONNECTOR_POLL_HPD;

		ret = drm_connector_attach_encoder(&mtk_connector->connector, bridge->encoder);
		if (ret) {
			DP_MSG("Failed to attach encoder:%d\n", ret);
			kfree(mtk_connector);
			return;
		}

		mtk_connector->encoder = bridge->encoder;
		mtk_connector->connector.encoder = bridge->encoder;

		if (mtk_connector->connector.funcs->reset)
			mtk_connector->connector.funcs->reset(&mtk_connector->connector);

		ret = drm_connector_register(&mtk_connector->connector);
		if (ret) {
			DP_MSG("Failed to register connector:%d\n", ret);
			kfree(mtk_connector);
			return;
		}

		drm_encoder_helper_add(bridge->encoder, &mtk_dp_encoder_helper_funcs);

		index++;
	}
}

static struct mtk_dp *mtk_dp_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dp, bridge);
}

static int mtk_dp_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	int ret;

	DP_FUNC();

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		DP_MSG("Driver does not provide a connector");
		return -EINVAL;
	}

	mtk_dp->aux.drm_dev = bridge->dev;
	ret = drm_dp_aux_register(&mtk_dp->aux);
	if (ret) {
		DP_MSG("failed to register DP AUX channel:%d\n", ret);
		return ret;
	}

	if (mtk_dp->next_bridge) {
		ret = drm_bridge_attach(bridge->encoder, mtk_dp->next_bridge,
					&mtk_dp->bridge, flags);
		if (ret) {
			drm_warn(mtk_dp->drm_dev,
				 "Failed to attach external bridge:%d\n", ret);
			goto err_bridge_attach;
		}
	}

	mtk_dp->drm_dev = bridge->dev;

	mtk_dp_init_port(mtk_dp);
	mtk_dp_hpd_interrupt_enable(mtk_dp, true);

	return 0;

err_bridge_attach:
	drm_dp_aux_unregister(&mtk_dp->aux);
	return ret;
}

static u32 *mtk_dp_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     unsigned int *num_output_fmts)
{
	u32 *output_fmts;

	*num_output_fmts = 0;
	output_fmts = kmalloc(sizeof(*output_fmts), GFP_KERNEL);
	if (!output_fmts)
		return NULL;
	*num_output_fmts = 1;
	output_fmts[0] = MEDIA_BUS_FMT_FIXED;
	return output_fmts;
}

static u32 *mtk_dp_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						    struct drm_bridge_state *bridge_state,
						    struct drm_crtc_state *crtc_state,
						    struct drm_connector_state *conn_state,
						    u32 output_fmt,
						    unsigned int *num_input_fmts)
{
	u32 *input_fmts;
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	struct drm_display_info *display_info =
		&conn_state->connector->display_info;
	u32 lane_count_min = mtk_dp->training_info.link_lane_count;
	u32 rate = drm_dp_bw_code_to_link_rate(mtk_dp->training_info.link_rate) *
		lane_count_min;

	*num_input_fmts = 0;

	/*
	 * If the linkrate is smaller than datarate of RGB888, larger than
	 * datarate of YUV422 and sink device supports YUV422, we output YUV422
	 * format. Use this condition, we can support more resolution.
	 */
	if ((rate < (mode->clock * 24 / 8)) &&
	    (rate > (mode->clock * 16 / 8)) &&
	    (display_info->color_formats & DRM_COLOR_FORMAT_YCBCR422)) {
		input_fmts = kcalloc(1, sizeof(*input_fmts), GFP_KERNEL);
		if (!input_fmts)
			return NULL;
		*num_input_fmts = 1;
		input_fmts[0] = MEDIA_BUS_FMT_YUYV8_1X16;
	} else {
		input_fmts = kcalloc(ARRAY_SIZE(mt8678_input_fmts),
				     sizeof(*input_fmts),
				     GFP_KERNEL);
		if (!input_fmts)
			return NULL;

		*num_input_fmts = ARRAY_SIZE(mt8678_input_fmts);
		memcpy(input_fmts, mt8678_input_fmts, sizeof(mt8678_input_fmts));
	}

	return input_fmts;
}

static const struct drm_bridge_funcs mtk_dp_bridge_funcs = {
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_get_output_bus_fmts = mtk_dp_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dp_bridge_atomic_get_input_bus_fmts,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.attach = mtk_dp_bridge_attach,
};

int mtk_dp_hpd_handle_in_thread(struct mtk_dp *mtk_dp)
{
	int ret = DP_RET_NOERR;
	enum dp_encoder_id encoder_id;
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	bool mst_cap = false;

	if (mtk_dp->training_info.cable_state_change) {
		bool current_hpd = mtk_dp_hpd_get_pin_level(mtk_dp);

		mtk_dp->training_info.cable_state_change = false;

		if (mtk_dp->training_info.cable_plug_in && current_hpd) {
			DP_MSG("HPD_CON\n");
			mtk_dp_vsvoter_set(mtk_dp);
			mtk_dp_initial_setting(mtk_dp);
			mtk_dp_analog_power_on_off(mtk_dp, true);

			/* check capability */
			dpcd[0] = mtk_dp->training_info.dp_version;
			mst_cap = drm_dp_read_mst_cap(&mtk_dp->aux, dpcd);
			if (!mst_cap) {
				DP_MSG("support SST\n");
				mtk_dp->training_info.dp_mst_cap = false;
			} else {
				DP_MSG("support MST\n");
				mtk_dp->training_info.dp_mst_cap = true;
				mtk_dp->mst_enable = true;
			}

			mtk_dp_connect_attach_encoder(mtk_dp);
		} else {
			DP_MSG("HPD_DISCON\n");
			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
				mtk_dp_video_mute(mtk_dp, encoder_id, true);
				mtk_dp_audio_mute(mtk_dp, encoder_id, true);
			}

			if (mtk_dp->uevent_to_hwc) {
				mtk_dp_hotplug_uevent(0);
				mtk_dp->uevent_to_hwc = false;
				mtk_dp->disp_state = DP_DISP_STATE_NONE;
			} else {
				DP_MSG("Skip uevent(0)\n");
			}

			mtk_dp_init_variable(mtk_dp);
			mtk_dp_phy_set_idle_pattern(mtk_dp, true);
			if (mtk_dp->has_fec)
				mtk_dp_fec_enable(mtk_dp, false);
			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
				mtk_dp_stop_sent_sdp(mtk_dp, encoder_id);

			DP_MSG("Power OFF:%d", mtk_dp->power_on);
			mtk_dp_analog_power_on_off(mtk_dp, false);

			fake_cable_in = false;
			fake_res = FAKE_DEFAULT_RES;
#if DEPTH_10BIT_PRIORITY
			fake_bpc = DP_COLOR_DEPTH_10BIT;
#else
			fake_bpc = DP_COLOR_DEPTH_8BIT;
#endif

			kfree(mtk_dp->edid);
			mtk_dp->edid = NULL;
			ret = DP_RET_PLUG_OUT;
			mtk_dp_vsvoter_clr(mtk_dp);
		}
	}

	if (mtk_dp->training_info.phy_status & HPD_INT_EVNET) {
		DP_MSG("HPD_INT_EVNET\n");
		mtk_dp->training_info.phy_status &= ~HPD_INT_EVNET;
		mtk_dp_hpd_check_sink_event(mtk_dp);
	}

	return ret;
}

void mtk_dp_training_check_swing_pre(struct mtk_dp *mtk_dp,
				     u8 lane_count,
	u8 *dpcd_202,
	u8 *dpcd_buffer,
	u8 is_adjustable_swing_pre,
	u8 is_lttpr)
{
	u8 swing, emhasis;
	u8 lane01_adjust_offset, lane23_adjust_offset;

	if (is_lttpr) {
		lane01_adjust_offset = 3; /* F0033h-F0030h */
		lane23_adjust_offset = 4; /* F0034h-F0030h */
	} else {
		lane01_adjust_offset = 4; /* 206h-202h */
		lane23_adjust_offset = 5; /* 207h-202h */
	}

	if (lane_count >= 0x1) { /* lane0 */
		swing = (dpcd_202[lane01_adjust_offset] & 0x3);
		emhasis = ((dpcd_202[lane01_adjust_offset] & 0x0C) >> 2);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE0, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x0] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x0] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x0] |= BIT(5);
	}

	if (lane_count >= 0x2) { /* lane1 */
		swing = (dpcd_202[lane01_adjust_offset] & 0x30) >> 4;
		emhasis = ((dpcd_202[lane01_adjust_offset] & 0xC0) >> 6);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE1, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x1] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x1] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x1] |= BIT(5);
	}

	if (lane_count == 0x4) { /* lane 2,3 */
		swing = (dpcd_202[lane23_adjust_offset] & 0x3);
		emhasis = ((dpcd_202[lane23_adjust_offset] & 0x0C) >> 2);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE2, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x2] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x2] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x2] |= BIT(5);

		swing = (dpcd_202[lane23_adjust_offset] & 0x30) >> 4;
		emhasis = ((dpcd_202[lane23_adjust_offset] & 0xC0) >> 6);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE3, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x3] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x3] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x3] |= BIT(5);
	}

	/* Wait signal stable enough */
	mdelay(1);
}

void mtk_dp_print_training_state(u8 state)
{
	switch (state) {
	case DP_TRAINING_STATE_STARTUP:
		DP_MSG("DP_TRAINING_STATE_STARTUP!\n");
		break;
	case DP_TRAINING_STATE_CHECKCAP:
		DP_MSG("DP_TRAINING_STATE_CHECKCAP!\n");
		break;
	case DP_TRAINING_STATE_CHECKEDID:
		DP_MSG("DP_TRAINING_STATE_CHECKEDID!\n");
		break;
	case DP_TRAINING_STATE_TRAINING_PRE:
		DP_MSG("DP_TRAINING_STATE_TRAINING_PRE!\n");
		break;
	case DP_TRAINING_STATE_TRAINING:
		DP_MSG("DP_TRAINING_STATE_TRAINING!\n");
		break;
	case DP_TRAINING_STATE_CHECKTIMING:
		DP_MSG("DP_TRAINING_STATE_CHECKTIMING!\n");
		break;
	case DP_TRAINING_STATE_NORMAL:
		DP_MSG("DP_TRAINING_STATE_NORMAL!\n");
		break;
	case DP_TRAINING_STATE_POWERSAVE:
		DP_MSG("DP_TRAINING_STATE_POWERSAVE!\n");
		break;
	case DP_TRAINING_STATE_DPIDLE:
		DP_MSG("DP_TRAINING_STATE_DPIDLE!\n");
		break;
	}
}

void mtk_dp_check_and_set_power_state(struct mtk_dp *mtk_dp)
{
	u8 temp[0x1];

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00600, temp, 0x1);
	if (temp[0] != 0x01) {
		temp[0] = 0x01;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, temp, 0x1);
		mdelay(1);
	}
}

u8 mtk_dp_training_pre_check(struct mtk_dp *mtk_dp)
{
	if (!mtk_dp->training_info.cable_plug_in ||
	    ((mtk_dp->training_info.phy_status & HPD_DISCONNECT) != 0x0)) {
		DP_MSG("Training Abort, HPD is low\n");
		return false;
	}

	if (mtk_dp->training_info.phy_status & HPD_INT_EVNET)
		mtk_dp_hpd_handle_in_thread(mtk_dp);

	if (mtk_dp->training_state < DP_TRAINING_STATE_TRAINING) {
		DP_MSG("Retraining!\n");
		return false;
	}

	return true;
}

enum dp_train_stage mtk_dp_check_training_res(struct mtk_dp *mtk_dp, u8 dpcd_202)
{
	enum dp_train_stage res = DP_LT_PASS;

	if (mtk_dp->training_info.cr_done == 0x0) {
		if ((dpcd_202 & 0x01) != 0x01)
			res = DP_LT_CR_L0_FAIL;
		else if ((dpcd_202 & 0x11) != 0x11)
			res = DP_LT_CR_L1_FAIL;
		else
			res = DP_LT_CR_L2_FAIL;
	} else if (mtk_dp->training_info.eq_done == 0x0) {
		if ((dpcd_202 & 0x07) != 0x07)
			res = DP_LT_EQ_L0_FAIL;
		else if ((dpcd_202 & 0x77) != 0x77)
			res = DP_LT_EQ_L1_FAIL;
		else
			res = DP_LT_EQ_L2_FAIL;
	}

	return res;
}

enum dp_train_stage mtk_dp_training_flow(struct mtk_dp *mtk_dp, u8 link_rate, u8 lane_count)
{
	u8 dpcd_buffer[0x4], dpcd_202[0x6], temp[0x6], dpcd_200c[0x3];
	u8 dpcd_206 = 0xFF;
	u8 retry_times = 0;
	u8 control = 0;
	u8 loop = 0;
	u8 cr_loop = 0;
	u8 eq_loop = 0;
	u8 ssc_enable = false;
	enum dp_train_stage res = DP_LT_NONE;

	memset(temp, 0x0, sizeof(temp));
	memset(dpcd_buffer, 0x0, sizeof(dpcd_buffer));

	mtk_dp_check_and_set_power_state(mtk_dp);

	temp[0] = link_rate;
	temp[1] = (lane_count | DP_AUX_SET_ENAHNCED_FRAME);
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00100, temp, 0x2);

	mtk_dp_ssc_check(mtk_dp, &ssc_enable);
	mtk_dp_phy_training_config(mtk_dp, link_rate, lane_count, ssc_enable);
	mtk_dp_set_lane_count(mtk_dp, lane_count);
	mdelay(5);

	do {
		loop++;
		if (!mtk_dp_training_pre_check(mtk_dp))
			return DP_LT_NONE;

		if (mtk_dp->training_info.cr_done == 0x0) {
			DP_MSG("CR Training START\n");
			mtk_dp_set_scramble(mtk_dp, false);

			if (control == 0x0)	{
				mtk_dp_set_training_pattern(mtk_dp, BIT(4));
				control = 0x1;
				temp[0] = 0x21;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00102, temp, 0x1);
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00206, (temp + 4), 0x2);
				loop++;

				/* force use SWING = 0 & PRE = 0 to start 1st link training */
				temp[4] = 0x00;
				temp[5] = 0x00;
				mtk_dp_training_check_swing_pre(mtk_dp, lane_count, temp,
								dpcd_buffer, true, false);
			}

			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00103, dpcd_buffer, lane_count);
			drm_dp_link_train_clock_recovery_delay(&mtk_dp->aux, mtk_dp->rx_cap);
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00202, dpcd_202, 0x6);
			if (mtk_dp->training_info.sink_ext_cap_en) {
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C, dpcd_200c, 0x3);
				dpcd_202[0] = dpcd_200c[0]; /*  copy DPCD200C=>DCPD202 */
				dpcd_202[1] = dpcd_200c[1]; /*  copy DPCD200D=>DCPD203 */
				dpcd_202[2] = dpcd_200c[2]; /*  copy DPCD200E=>DCPD204 */
			}

			if (drm_dp_clock_recovery_ok(dpcd_202, lane_count)) {
				DP_MSG("CR Training Success\n");

				mtk_dp->training_info.cr_done = true;

				retry_times = 0x0;
				loop = 0x1;
				eq_loop = 0;
			} else {
				/* request swing & emp is the same eith last time */
				if (dpcd_206 == dpcd_202[0x4]) {
					if ((dpcd_206 & 0x3) == 0x3) /* lane0 match max swing */
						loop = DP_LT_MAX_LOOP;
					else
						loop++;
				} else {
					dpcd_206 = dpcd_202[0x4];
				}

				cr_loop++;
				DP_MSG("CR Training Fail\n");
			}
		} else if (mtk_dp->training_info.eq_done == 0x0) {
			DP_MSG("EQ Training START\n");

			if (control == 0x1) {
				if (mtk_dp->training_info.tps4_support) {
					mtk_dp_set_training_pattern(mtk_dp, DP_TPS4);
					temp[0] = 0x07;
				} else if (mtk_dp->training_info.tps3_support) {
					mtk_dp_set_training_pattern(mtk_dp, DP_TPS3);
					temp[0] = 0x23;
				} else {
					mtk_dp_set_training_pattern(mtk_dp, DP_TPS2);
					temp[0] = 0x22;
				}
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00102, temp, 0x1);

				control = 0x2;
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00206, (dpcd_202 + 4), 0x2);

				loop++;
				mtk_dp_training_check_swing_pre(mtk_dp, lane_count, dpcd_202,
								dpcd_buffer, true, false);
			}

			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00103, dpcd_buffer, lane_count);
			drm_dp_link_train_channel_eq_delay(&mtk_dp->aux, mtk_dp->rx_cap);

			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00202, dpcd_202, 0x6);
			if (mtk_dp->training_info.sink_ext_cap_en) {
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C, dpcd_200c, 0x3);
				dpcd_202[0] = dpcd_200c[0]; /* copy DPCD200C=>DCPD202 */
				dpcd_202[1] = dpcd_200c[1]; /* copy DPCD200D=>DCPD203 */
				dpcd_202[2] = dpcd_200c[2]; /* copy DPCD200E=>DCPD204 */
			}

			if (!drm_dp_clock_recovery_ok(dpcd_202, lane_count)) {
				mtk_dp->training_info.cr_done = false;
				mtk_dp->training_info.eq_done = false;
				break;
			}

			if (drm_dp_channel_eq_ok(dpcd_202, lane_count)) {
				DP_MSG("EQ Training Success\n");
				if (dpcd_202[2] & 0x1) {
					mtk_dp->training_info.eq_done = true;
					DP_MSG("Inter-lane skew Success\n");
					break;
				}
			}

			DP_MSG("EQ Training Fail\n");
			eq_loop++;
			if (dpcd_206 == dpcd_202[0x4])
				loop++;
			else
				dpcd_206 = dpcd_202[0x4];
		}

		mtk_dp_training_check_swing_pre(mtk_dp, lane_count, dpcd_202,
						dpcd_buffer, true, false);
		DP_MSG("retry_times:%d, loop:%d\n", retry_times, loop);
	} while ((loop < DP_LT_RETRY_LIMIT) &&
			(cr_loop < DP_LT_MAX_CR_LOOP) &&
			(eq_loop < DP_LT_MAX_EQ_LOOP));

	temp[0] = 0x0;
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00102, temp, 0x1);
	mtk_dp_set_training_pattern(mtk_dp, DP_0);

	if (mtk_dp->training_info.eq_done) {
		mtk_dp->training_info.link_rate = link_rate;
		mtk_dp->training_info.link_lane_count = lane_count;

		mtk_dp_set_scramble(mtk_dp, true);
		temp[0] = (lane_count | DP_AUX_SET_ENAHNCED_FRAME);
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00101, temp, 0x1);
		mtk_dp_set_enhanced_frame_mode(mtk_dp, ENABLE_DP_EF_MODE);

		DP_MSG("Training PASS, link rate:0x%x, lane count:%d\n",
		       mtk_dp->training_info.link_rate,
			mtk_dp->training_info.link_lane_count);
		return DP_LT_PASS;
	}

	DP_MSG("Training Fail\n");

	res = mtk_dp_check_training_res(mtk_dp, dpcd_202[0]);

	return res;
}

int mtk_dp_set_training_start(struct mtk_dp *mtk_dp)
{
	enum dp_link_rate max_link_rate = mtk_dp->training_info.max_link_rate;
	enum dp_lane_count max_lane_count = mtk_dp->training_info.max_link_lane_count;
	enum dp_link_rate link_rate;
	enum dp_lane_count lane_count;
	u32 loop;

	if (mtk_dp->training_info.dp_version == DP_VER_14)
		loop = DP_CTS_RETRAIN_TIMES_14;
	else
		loop = DP_CTS_RETRAIN_TIMES_DEFAULT;

	if (!mtk_dp_hpd_get_pin_level(mtk_dp)) {
		while (1) {
			mdelay(1);
			DP_MSG("Start Training Abort!=> HPD low ! remaining loop %d(delay)\n",
			       loop);
			if (mtk_dp_hpd_get_pin_level(mtk_dp))
				break;
			loop--;

			if (loop <= 0) {
				mtk_dp->training_state = DP_TRAINING_STATE_DPIDLE;
				return DP_RET_PLUG_OUT;
			}
		}
	} else {
		mtk_dp->training_info.cable_plug_in = true;
	}

	link_rate = mtk_dp->rx_cap[1];
	lane_count = mtk_dp->rx_cap[2] & 0x1F;
	DP_MSG("RX support link rate:0x%x, lane count:%d",
	       link_rate, lane_count);

	link_rate = (link_rate >= max_link_rate) ?
		max_link_rate : link_rate;
	lane_count = (lane_count >= max_lane_count) ?
		max_lane_count : lane_count;

	switch (link_rate) {
	case DP_LINK_RATE_RBR:
	case DP_LINK_RATE_HBR:
	case DP_LINK_RATE_HBR2:
	case DP_LINK_RATE_HBR25:
	case DP_LINK_RATE_HBR3:
		break;

	default:
		if (link_rate > DP_LINK_RATE_HBR3)
			link_rate = DP_LINK_RATE_HBR3;
		else if (link_rate > DP_LINK_RATE_HBR2)
			link_rate = DP_LINK_RATE_HBR2;
		else if (link_rate > DP_LINK_RATE_HBR)
			link_rate = DP_LINK_RATE_HBR;
		else
			link_rate = DP_LINK_RATE_RBR;
		break;
	};

	max_link_rate = link_rate;

	do {
		mtk_dp->training_info.cr_done = false;
		mtk_dp->training_info.eq_done = false;

		DP_MSG("training with link rate:0x%x, lane count:%d",
		       link_rate, lane_count);

		mtk_dp_training_flow(mtk_dp, link_rate, lane_count);

		if (!mtk_dp->training_info.cr_done) {
			switch (link_rate) {
			case DP_LINK_RATE_RBR:
				lane_count = lane_count / 2;
				link_rate = max_link_rate;

				if (lane_count == 0x0) {
					mtk_dp->training_state =
						DP_TRAINING_STATE_DPIDLE;
					return DP_RET_TRANING_FAIL;
				}

				break;

			case DP_LINK_RATE_HBR:
				link_rate = DP_LINK_RATE_RBR;
				break;

			case DP_LINK_RATE_HBR2:
				link_rate = DP_LINK_RATE_HBR;
				break;

			case DP_LINK_RATE_HBR3:
				link_rate = DP_LINK_RATE_HBR2;
				break;

			default:
				return DP_RET_TRANING_FAIL;
			};

			loop--;
		} else if (!mtk_dp->training_info.eq_done) {
			if (lane_count == DP_4LANE)
				lane_count = DP_2LANE;
			else if (lane_count >= DP_1LANE)
				lane_count = DP_1LANE;
			else
				return DP_RET_TRANING_FAIL;

			loop--;
		} else {
			return DP_RET_NOERR;
		}
	} while (loop > 0);

	return DP_RET_TRANING_FAIL;
}

int mtk_dp_training_handler(struct mtk_dp *mtk_dp)
{
	int ret = DP_RET_NOERR;
	enum dp_encoder_id encoder_id = 0;

	/* mtk_dp_print_training_state(mtk_dp->training_state); */

	if (!mtk_dp->training_info.cable_plug_in)
		return DP_RET_PLUG_OUT;

	if (mtk_dp->training_state == DP_TRAINING_STATE_NORMAL)
		return ret;

	if (mtk_dp->training_state_pre != mtk_dp->training_state) {
		mtk_dp_print_training_state(mtk_dp->training_state);

		mtk_dp->training_state_pre = mtk_dp->training_state;
	}

	switch (mtk_dp->training_state) {
	case DP_TRAINING_STATE_STARTUP:
		if (mtk_dp->next_bridge) {
			mtk_dp->next_bridge->funcs->pre_enable(mtk_dp->next_bridge);
			msleep(500);
		}
		mtk_dp->training_state = DP_TRAINING_STATE_CHECKCAP;
		break;

	case DP_TRAINING_STATE_CHECKCAP:
		if (mtk_dp_check_sink_cap(mtk_dp)) {
			mtk_dp->training_info.check_cap_times = 0;
			mtk_dp->training_state = DP_TRAINING_STATE_CHECKEDID;
		} else {
			u8 check_times = 0;

			mtk_dp->training_info.check_cap_times++;
			check_times = mtk_dp->training_info.check_cap_times;

			if (check_times > DP_CheckSinkCap_TimeOutCnt) {
				mtk_dp->training_info.check_cap_times = 0;
				DP_MSG("CheckCap Fail %d times",
				       DP_CheckSinkCap_TimeOutCnt);
				mtk_dp->training_state = DP_TRAINING_STATE_DPIDLE;
				ret = DP_RET_TIMEOUT;
			} else {
				DP_MSG("CheckCap Fail %d times", check_times);
			}
		}
		break;

	case DP_TRAINING_STATE_CHECKEDID:
		mtk_dp->training_state = DP_TRAINING_STATE_TRAINING_PRE;
		break;

	case DP_TRAINING_STATE_TRAINING_PRE:
		mtk_dp->training_state = DP_TRAINING_STATE_TRAINING;
		break;

	case DP_TRAINING_STATE_TRAINING:
		mtk_dp_fec_enable(mtk_dp, false);

		ret = mtk_dp_set_training_start(mtk_dp);
		if (ret == DP_RET_NOERR) {
			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
				mtk_dp_video_mute(mtk_dp, encoder_id, true);
				mtk_dp_audio_mute(mtk_dp, encoder_id, true);
			}
			mtk_dp->training_state = DP_TRAINING_STATE_CHECKTIMING;
			mtk_dp->dp_ready = true;
			mtk_dp_fec_enable(mtk_dp, mtk_dp->has_fec);
		} else if (ret == DP_RET_RETRANING) {
			ret = DP_RET_NOERR;
		} else {
			DP_ERR("Handle Training Fail 6 times\n");
		}
		break;
	case DP_TRAINING_STATE_CHECKTIMING:
		mtk_dp->training_state = DP_TRAINING_STATE_NORMAL;
		if (mtk_dp->training_info.sink_count == 0) {
			DP_MSG("no sink count, skip uevent\n");
			break;
		}

		if (mtk_dp->uevent_to_hwc) {
			mtk_dp_hotplug_uevent(1);
			mtk_dp->uevent_to_hwc = false;
		} else {
			DP_MSG("Skip uevent(1)\n");
		}

		break;
	case DP_TRAINING_STATE_NORMAL:
		break;
	case DP_TRAINING_STATE_POWERSAVE:
		break;
	case DP_TRAINING_STATE_DPIDLE:
		break;
	default:
		break;
	}

	return ret;
}

int mtk_dp_handle(struct mtk_dp *mtk_dp)
{
	int ret = DP_RET_NOERR;
	enum dp_encoder_id encoder_id = 0;

	if (!mtk_dp->dp_ready)
		return DP_RET_NOERR;

	if (!mtk_dp->training_info.cable_plug_in)
		return DP_RET_PLUG_OUT;

	if (mtk_dp->state != mtk_dp->state_pre) {
		DP_MSG("m_DPState:%d, m_DPStateTemp:%d\n",
		       mtk_dp->state, mtk_dp->state_pre);
		mtk_dp->state_pre = mtk_dp->state;
	}

	switch (mtk_dp->state) {
	case DP_STATE_INITIAL:
		for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
			mtk_dp_video_mute(mtk_dp, encoder_id, true);
			mtk_dp_audio_mute(mtk_dp, encoder_id, true);
		}
		mtk_dp->state = DP_STATE_IDLE;
		DP_MSG("DP_STATE_IDLE");
		break;

	case DP_STATE_IDLE:
		if (mtk_dp->training_state == DP_TRAINING_STATE_NORMAL)
			mtk_dp->state = DP_STATE_PREPARE;
		break;

	case DP_STATE_PREPARE:
		DP_DBG("pattern_gen:%d, video_enable:%d, audio_enable:%d, audio_cap:%d\n",
		       mtk_dp->info[0].pattern_gen, mtk_dp->video_enable,
			   mtk_dp->audio_enable, mtk_dp->info[0].audio_cap);

		if (mtk_dp->info[0].pattern_gen) {
			mtk_dp->video_enable = true;
			mtk_dp->info[0].input_src = DP_SRC_PG;
		}

		if (mtk_dp->video_enable) {
			msleep(1000);
			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
				mtk_dp_video_config(mtk_dp, encoder_id);
				mtk_dp_video_enable(mtk_dp, encoder_id, true);
				mtk_dp_video_mute(mtk_dp, encoder_id, false);
			}
		}

		if (mtk_dp->audio_enable && mtk_dp->info[0].audio_cap != 0) { /* todo */
			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
				/* mtk_dp_i2s_audio_config(mtk_dp, encoder_id); */
				/* mtk_dp_audio_mute(mtk_dp, encoder_id, false); */
			}
		}

		if (mtk_dp->video_enable || mtk_dp->audio_enable)
			mtk_dp->state = DP_STATE_NORMAL;
		else
			ret = DP_RET_WAIT_TRIGGER;

		break;

	case DP_STATE_NORMAL:
		if (mtk_dp->training_state != DP_TRAINING_STATE_NORMAL) {
			for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
				mtk_dp_video_mute(mtk_dp, encoder_id, true);
				mtk_dp_audio_mute(mtk_dp, encoder_id, true);
				mtk_dp_stop_sent_sdp(mtk_dp, encoder_id);
			}
			mtk_dp->state = DP_STATE_IDLE;
			DP_MSG("DP Link Status Change\n");
		}
		break;

	default:
		break;
	}

	return ret;
}

bool mtk_dp_done(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->state != DP_STATE_NORMAL)
		return false;

	if (mtk_dp->training_state != DP_TRAINING_STATE_NORMAL)
		return false;

	return true;
}

static void mtk_dp_main_handle(struct work_struct *data)
{
	struct mtk_dp *mtk_dp = container_of(data, struct mtk_dp, dp_work);
	u64 starttime = get_system_time();

	/* debounce */
	msleep(400);

	DP_MSG("handle go\n");

	do {
		if (get_time_diff(starttime) > 5000000000ULL) {
			DP_ERR("Handle time over 5s\n");
			break;
		}

		if (mtk_dp_hpd_handle_in_thread(mtk_dp) != DP_RET_NOERR)
			break;

		if (mtk_dp_training_handler(mtk_dp) != DP_RET_NOERR)
			break;

		if (mtk_dp->mst_enable) {
			if (mtk_dp_mst_drv_handler(mtk_dp) != DP_RET_NOERR)
				break;
		} else {
			if (mtk_dp_handle(mtk_dp) != DP_RET_NOERR)
				break;
		}
	} while (!mtk_dp_done(mtk_dp));

	DP_MSG("handle end\n");
}

static int mtk_dp_dt_parse_pdata(struct mtk_dp *mtk_dp,
				 struct platform_device *pdev)
{
	struct resource regs;
	struct device *dev = &pdev->dev;
	int ret = 0;
	u32 phy_params_int[DP_PHY_REG_COUNT] = {
		0x20181410, 0x20241e18, 0x00003028,
		0x10080400, 0x000c0600, 0x00000008
	};
	u32 phy_params_dts[DP_PHY_REG_COUNT];

	if (of_address_to_resource(dev->of_node, 0, &regs) != 0)
		DP_MSG("Missing reg[0] in %s node\n",
		       dev->of_node->full_name);

	if (of_address_to_resource(dev->of_node, 1, &regs) != 0)
		DP_MSG("Missing reg[1] in %s node\n",
		       dev->of_node->full_name);

	mtk_dp->regs = of_iomap(dev->of_node, 0);
	mtk_dp->phyd_regs = of_iomap(dev->of_node, 1);

	ret = of_property_read_u32_array(dev->of_node, "dptx,phy_params",
					 phy_params_dts, ARRAY_SIZE(phy_params_dts));
	if (ret) {
		DP_DBG("get phy_params fail, use default val, ret:%d\n", ret);
		mtk_dp_phy_param_init(mtk_dp,
				      phy_params_int, ARRAY_SIZE(phy_params_int));
	} else {
		mtk_dp_phy_param_init(mtk_dp,
				      phy_params_dts, ARRAY_SIZE(phy_params_dts));
	}

	ret = mtk_dp_vsvoter_parse(mtk_dp, dev->of_node);
	if (ret)
		DP_MSG("failed to parse vsv property\n");

	return 0;
}

/*  dp tx api for drm drv start */
int mtk_drm_dp_get_dev_info(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct mtk_dispif_info *info = data;
	struct mtk_dp *mtk_dp = g_mtk_dp;

	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return 0;
	}

	info->display_id = mtk_dp->id;
	info->displayFormat = mtk_dp->info[0].format; //todo
	info->displayHeight = mtk_dp->info[0].dp_output_timing.vde;
	info->displayWidth = mtk_dp->info[0].dp_output_timing.hde;
	info->displayMode = DISPIF_MODE_VIDEO;
	info->displayType = DISPLAYPORT;
	info->isConnected = (mtk_dp->state == DP_STATE_NORMAL) ? true : false;
	info->isHwVsyncAvailable = true;
	info->vsyncFPS = g_mtk_dp->info[0].dp_output_timing.frame_rate * 100;
	DP_MSG("%s, %d, fake:%d\n", __func__, __LINE__, fake_cable_in);

	return 0;
}

int mtk_drm_dp_audio_enable(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;
	enum dp_encoder_id encoder_id;

	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return 0;
	}

	mtk_dp->audio_enable = *(bool *)data;
	DP_MSG("audio_enable:%d\n", mtk_dp->audio_enable);

	if (!mtk_dp->dp_ready) {
		DP_ERR("%s, DP is not ready\n", __func__);
		return 0;
	}

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_audio_mute(mtk_dp, encoder_id, mtk_dp->audio_enable ? false : true);

	return 0;
}

int mtk_drm_dp_audio_config(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;
	enum dp_encoder_id encoder_id = 0;

	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return 0;
	}

	mtk_dp->info[encoder_id].audio_config = *(unsigned int *)data; //todo
	DP_MSG("audio_config:0x%x\n", mtk_dp->info[encoder_id].audio_config);

	if (!mtk_dp->dp_ready) {
		DP_ERR("%s, DP is not ready\n", __func__);
		return 0;
	}

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_i2s_audio_config(mtk_dp, encoder_id);

	return 0;
}

int mtk_drm_dp_get_cap(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	unsigned int ch[7] = {2, 3, 4, 5, 6, 7, 8};
	unsigned int fs[5] = {32, 44, 48, 96, 192};
	unsigned int len[3] = {16, 20, 24};
	unsigned int *dp_cap = data;

	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return 0;
	}

	if (fake_cable_in) {
		DP_MSG("force audio format %dCH, %dkHz, %dbit\n",
		       ch[force_ch], fs[force_fs], len[force_len]);
		*dp_cap = ((BIT(force_ch) << DP_CAPABILITY_CHANNEL_SFT)
			| (BIT(force_fs) << DP_CAPABILITY_SAMPLERATE_SFT)
			| (BIT(force_len) << DP_CAPABILITY_BITWIDTH_SFT));
		return 0;
	}

	if (g_mtk_dp->dp_ready)
		*dp_cap = g_mtk_dp->info[0].audio_cap; //todo

	if (*dp_cap == 0)
		*dp_cap = ((DP_CHANNEL_2 << DP_CAPABILITY_CHANNEL_SFT)
			| (DP_SAMPLERATE_192 << DP_CAPABILITY_SAMPLERATE_SFT)
			| (DP_BITWIDTH_24 << DP_CAPABILITY_BITWIDTH_SFT));

	return 0;
}

int mtk_drm_dp_get_info_by_id(struct drm_device *dev,
			struct drm_mtk_session_info *info, int dp_encoder_id)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return -EINVAL;
	}

	if (dp_encoder_id >=  0 && dp_encoder_id < 2) {
		info->physical_width = g_mtk_dp->mode[dp_encoder_id].hdisplay;
		info->physical_height = g_mtk_dp->mode[dp_encoder_id].vdisplay;
		DP_MSG("%s, physical_width:%u physical_height:%u\n",
		       __func__, info->physical_width, info->physical_height);
	} else {
		DP_ERR("%s, dp_encoder_id is invalid: %d\n", __func__, dp_encoder_id);
		return -EINVAL;
	}

	return 0;
}

void mtk_dp_get_dsc_capability(u8 *dsc_cap)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	if (!g_mtk_dp->dp_ready) {
		DP_MSG("%s, DP is not ready\n", __func__);
		return;
	}

	drm_dp_dpcd_read(&g_mtk_dp->aux, DPCD_00060, dsc_cap, 16);
}

/*  dp tx api for drm drv end */

/*  dp tx api for EXPORT_SYMBOL_GPL start */
void mtk_dp_aux_swap_enable(bool enable)
{
	if (!g_mtk_dp) {
		DP_ERR("%s: dp not initial\n", __func__);
		return;
	}

	DP_MSG("%s, enable=%d -> %d\n", __func__, g_mtk_dp->swap_enable, enable);

	g_mtk_dp->swap_enable = enable;
}
EXPORT_SYMBOL_GPL(mtk_dp_aux_swap_enable);

void mtk_dp_set_pin_assign(u8 type)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	DP_MSG("%s, pin assign:%d\n", __func__, type);

	switch (type) {
	case DP_USB_PIN_ASSIGNMENT_C:
	case DP_USB_PIN_ASSIGNMENT_E:
		g_mtk_dp->training_info.max_link_lane_count = DP_4LANE;
		break;
	case DP_USB_PIN_ASSIGNMENT_D:
	case DP_USB_PIN_ASSIGNMENT_F:
	default:
		g_mtk_dp->training_info.max_link_lane_count = DP_2LANE;
		break;
	}
}
EXPORT_SYMBOL_GPL(mtk_dp_set_pin_assign);

void mtk_dp_SWInterruptSet(int status)
{
	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return;
	}

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;

	mutex_lock(&dp_lock);

	if ((status == HPD_DISCONNECT && g_mtk_dp->power_on) ||
	    (status == HPD_CONNECT && !g_mtk_dp->power_on))
		g_mtk_dp->uevent_to_hwc = true;

	if (!g_mtk_dp->power_on && status == HPD_DISCONNECT &&
	    g_mtk_dp->disp_state == DP_DISP_STATE_SUSPEND) {
		DP_MSG("System is sleeping, Plug Out\n");
		mtk_dp_hotplug_uevent(0);
		g_mtk_dp->disp_state = DP_DISP_STATE_NONE;
		mutex_unlock(&dp_lock);
		return;
	}

	mtk_dp_hpd_interrupt_set(status);

	mutex_unlock(&dp_lock);
}
EXPORT_SYMBOL_GPL(mtk_dp_SWInterruptSet);
/*  dp tx api for EXPORT_SYMBOL_GPL end */

/*  dp tx api for debug start */
int mtk_dp_phy_get_info(char *buffer, int size)
{
	int len = 0;
	int i = 0;
	char *phy_names[10] = {
		"L0P0", "L0P1", "L0P2", "L0P3", "L1P0",
		"L1P1", "L1P2", "L2P0", "L2P1", "L3P0"};

	if (!g_mtk_dp) {
		DP_ERR("%s, dp not initial\n", __func__);
		return 0;
	}

	len = snprintf(buffer, size, "PHY INFO:\n");
	for (i = 0; i < DP_PHY_LEVEL_COUNT; i++)
		len += snprintf(buffer + len, size - len,
			"#%d(%s), C0:%#04X(%2d), CP1:%#04X(%2d)\n", i,
			phy_names[i],
			g_mtk_dp->phy_params[i].c0, g_mtk_dp->phy_params[i].c0,
			g_mtk_dp->phy_params[i].cp1,
			g_mtk_dp->phy_params[i].cp1);

	return len;
}

/*  dp tx api for debug end */
static int mtk_dp_suspend(struct device *dev);
static int mtk_dp_resume(struct device *dev);

static int mtk_drm_dp_notifier(struct notifier_block *notifier,
			       unsigned long pm_event, void *unused)
{
	struct mtk_dp *mtk_dp = container_of(notifier, struct mtk_dp, nb);
	struct device *dev = mtk_dp->dev;

	pr_info("%s pm_event %d dev %s usage_count %d nb priority %d\n",
		__func__, pm_event, dev_name(dev), atomic_read(&dev->power.usage_count),
	       notifier->priority);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mtk_dp_suspend(dev);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		mtk_dp_resume(dev);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static int mtk_drm_dp_probe(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp;
	struct device *dev = &pdev->dev;
	int ret;
	struct mtk_drm_private *mtk_priv = dev_get_drvdata(dev);
	int irq_num = 0;
	void *base;

	mtk_dp = devm_kmalloc(dev, sizeof(*mtk_dp), GFP_KERNEL | __GFP_ZERO);
	if (!mtk_dp)
		return -ENOMEM;

	memset(mtk_dp, 0, sizeof(struct mtk_dp));
	mtk_dp->id = 0x0;
	mtk_dp->dev = dev;
	mtk_dp->priv = mtk_priv;
	mtk_dp->uevent_to_hwc = false;
	mtk_dp->disp_state = DP_DISP_STATE_NONE;

#if ATTACH_BRIDGE
	ret = drm_of_find_panel_or_bridge(dev->of_node, 2, 0, NULL, &mtk_dp->next_bridge);
	if (!mtk_dp->next_bridge) {
		DP_DBG("Can not find next_bridge %d\n", ret);
		return -EPROBE_DEFER;
	}
	DP_MSG("Found next bridge node: %pOF\n", mtk_dp->next_bridge->of_node);
#endif

	pm_runtime_enable(mtk_dp->dev);
	pm_runtime_get_sync(mtk_dp->dev);

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		DP_MSG("failed to request dp irq resource\n");
		return -EPROBE_DEFER;
	}

	ret = mtk_dp_dt_parse_pdata(mtk_dp, pdev);
	if (ret)
		return ret;

	mtk_dp_aux_init(mtk_dp);

	DP_MSG("%s, type:%d, irq:%d\n", __func__, 0, irq_num);
	irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(&pdev->dev, irq_num, mtk_dp_hpd_event,
			       IRQ_TYPE_LEVEL_HIGH, dev_name(&pdev->dev), mtk_dp);
	if (ret) {
		DP_MSG("failed to request mediatek dptx irq\n");
		return -EPROBE_DEFER;
	}

	dp_notify_data.name = "hdmi";  /* now hwc not support DP */
	dp_notify_data.index = 0;
	dp_notify_data.state = DP_NOTIFY_STATE_NO_DEVICE;
	ret = mtk_dp_uevent_dev_register(&dp_notify_data);
	if (ret)
		DP_ERR("switch_dev_register failed, returned:%d!\n", ret);
	dp_extcon = devm_extcon_dev_allocate(&pdev->dev, dp_cable);
	if (IS_ERR(dp_extcon)) {
		DP_ERR("Couldn't allocate dptx extcon device\n");
		return PTR_ERR(dp_extcon);
	}
	dp_extcon->dev.init_name = "dp_audio";
	ret = devm_extcon_dev_register(&pdev->dev, dp_extcon);
	if (ret) {
		pr_info("failed to register dptx extcon:%d\n", ret);
		return ret;
	}
	g_mtk_dp = mtk_dp;

	mutex_init(&dp_lock);
	platform_set_drvdata(pdev, mtk_dp);
	mtk_dp->dp_wq = create_singlethread_workqueue("mtk_dp_wq");
	if (!mtk_dp->dp_wq) {
		DP_ERR("Failed to create dptx workqueue\n");
		return -ENOMEM;
	}

	INIT_WORK(&mtk_dp->dp_work, mtk_dp_main_handle);

	mtk_dp_vsvoter_clr(mtk_dp);

	mtk_dp->data = (struct mtk_dp_data *)of_device_get_match_data(dev);

	platform_set_drvdata(pdev, mtk_dp);

	mtk_dp->bridge.funcs = &mtk_dp_bridge_funcs;
	mtk_dp->bridge.of_node = dev->of_node;
	mtk_dp->bridge.type = mtk_dp->data->bridge_type;
	ret = devm_drm_bridge_add(dev, &mtk_dp->bridge);
	if (ret)
		return ret;

	mtk_dp->nb.notifier_call = mtk_drm_dp_notifier;
	ret = register_pm_notifier(&mtk_dp->nb);
	if (ret)
		DP_ERR("register_pm_notifier failed %d", ret);

	base = ioremap(0x31b50000, 0x1000);
	writel(0xc2fc224d, base + 0x78);

	DP_FUNC("done\n");

	return ret;
}

static int mtk_drm_dp_remove(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp = platform_get_drvdata(pdev);

	if (mtk_dp->dp_wq)
		destroy_workqueue(mtk_dp->dp_wq);

	mutex_destroy(&dp_lock);
	drm_connector_cleanup(mtk_dp->conn);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_dp_suspend(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	if (!mtk_dp) {
		DP_FUNC("[DP] suspend, dp not initial\n");
		return 0;
	}

	if (mtk_dp->disp_state == DP_DISP_STATE_SUSPEND) {
		DP_FUNC("[DP] already suspend\n");
		return 0;
	}

	mtk_dp->disp_state = DP_DISP_STATE_SUSPENDING;

	DP_FUNC("%s usage_count %d +\n",
		dev_name(dev), atomic_read(&dev->power.usage_count));

	mtk_dp_hpd_interrupt_enable(mtk_dp, false);
	mtk_dp_disconnect_release(mtk_dp);

	mtk_drm_dpi_suspend(); // dpintf

	mtk_dp->disp_state = DP_DISP_STATE_SUSPEND;
	pm_runtime_put_sync(mtk_dp->dev);

	DP_FUNC("%s usage_count %d -\n",
		dev_name(mtk_dp->dev), atomic_read(&dev->power.usage_count));

	return 0;
}

static int mtk_dp_resume(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	if (!mtk_dp) {
		DP_FUNC("[DP] resume, dp not initial\n");
		return 0;
	}

	if (mtk_dp->disp_state == DP_DISP_STATE_RESUME) {
		DP_FUNC("[DP] already resume\n");
		return 0;
	}

	DP_FUNC("%s usage_count %d +\n",
		dev_name(dev), atomic_read(&dev->power.usage_count));

	pm_runtime_get_sync(dev);
	mtk_dp->disp_state = DP_DISP_STATE_RESUME;

	mtk_drm_dpi_resume(); // dpintf

	mtk_dp_init_port(mtk_dp);
	mtk_dp_hpd_interrupt_enable(mtk_dp, true);

	DP_FUNC("%s usage_count %d -\n",
		dev_name(mtk_dp->dev), atomic_read(&dev->power.usage_count));

	return 0;
}

void mtk_drm_dp_suspend(void)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;

	if (!mtk_dp || !mtk_dp->dev) {
		pr_info("[DP] dp not initial\n");
		return;
	}

	DP_FUNC("+\n");

	mtk_dp_suspend(mtk_dp->dev);

	DP_FUNC("-\n");
}

void mtk_drm_dp_resume(void)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;

	if (!mtk_dp || !mtk_dp->dev) {
		pr_info("[DP] dp not initial\n");
		return;
	}

	DP_FUNC("+\n");

	mtk_dp_resume(mtk_dp->dev);

	DP_FUNC("-\n");
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_dp_pm_ops,
		mtk_dp_suspend, mtk_dp_resume);

static const struct mtk_dp_efuse_fmt mt8678_dp_efuse_fmt[5] = {0};

static const struct mtk_dp_data mt8678_dp_data = {
	.bridge_type = DRM_MODE_CONNECTOR_DisplayPort,
	.smc_cmd = BIT(5),
	.efuse_fmt = mt8678_dp_efuse_fmt,
	.audio_support = true,
	.audio_m_div2_bit = 0,
};

static const struct of_device_id mtk_dp_of_match[] = {
	{ .compatible = "mediatek,dp_tx",
		.data = &mt8678_dp_data,
	},
	{ },
};

MODULE_DEVICE_TABLE(of, mtk_dp_of_match);

struct platform_driver mtk_dp_tx_driver = {
	.probe = mtk_drm_dp_probe,
	.remove = mtk_drm_dp_remove,
	.driver = {
		.name = "mediatek-drm-dp",
		.of_match_table = mtk_dp_of_match,
		.pm = &mtk_dp_pm_ops,
	},
};
