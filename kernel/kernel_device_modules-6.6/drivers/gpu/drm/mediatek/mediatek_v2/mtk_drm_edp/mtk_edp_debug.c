// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_edp_debug.h"
#include "mtk_edp.h"
#include "mtk_edp_api.h"
#include "mtk_edp_hal.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <drm/drm_modes.h>

static bool g_edptx_log;

static struct drm_display_mode edid_cea_modes[] = {
	/* 3 - 720x480@60Hz 16:9 */
	{ DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		   798, 858, 0, 480, 489, 495, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 4 - 1280x720@60Hz 16:9 */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 16 - 1920x1080@60Hz 16:9 */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 34 - 1920x1080@30Hz 16:9 */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 95 - 3840x2160@30Hz 16:9 */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 97 - 3840x2160@60Hz 16:9 */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
};

void mtk_edp_debug_enable(bool enable)
{
	g_edptx_log = enable;
}

bool mtk_edp_debug_get(void)
{
	return g_dptx_log;
}

void mtk_edp_debug(const char *opt)
{
	eDPTXFUNC("[eDPTX-debug]: %s\n", opt);

	if (strncmp(opt, "fakecablein:", 12) == 0) {
		if (strncmp(opt + 12, "enable", 6) == 0) {
			if (strncmp(opt + 12, "enable4k30P6", 12) == 0)
				mtk_dp_fake_plugin(SINK_3840_2160_30, 0);
			else if (strncmp(opt + 12, "enable4k30p8", 12) == 0)
				mtk_dp_fake_plugin(SINK_3840_2160_30, 1);
			else if (strncmp(opt + 12, "enable4k30p10", 13) == 0)
				mtk_dp_fake_plugin(SINK_3840_2160_30, 2);

			else if (strncmp(opt + 12, "enable4k60p6", 12) == 0)
				mtk_dp_fake_plugin(SINK_3840_2160, 0);
			else if (strncmp(opt + 12, "enable4k60p8", 12) == 0)
				mtk_dp_fake_plugin(SINK_3840_2160, 1);
			else if (strncmp(opt + 12, "enable4k60p10", 13) == 0)
				mtk_dp_fake_plugin(SINK_3840_2160, 2);

			else if (strncmp(opt + 12, "enable720p6", 11) == 0)
				mtk_dp_fake_plugin(SINK_1280_720, 0);
			else if (strncmp(opt + 12, "enable720p8", 11) == 0)
				mtk_dp_fake_plugin(SINK_1280_720, 1);
			else if (strncmp(opt + 12, "enable720p10", 12) == 0)
				mtk_dp_fake_plugin(SINK_1280_720, 2);

			else if (strncmp(opt + 12, "enable480p6", 11) == 0)
				mtk_dp_fake_plugin(SINK_640_480, 0);
			else if (strncmp(opt + 12, "enable480p8", 11) == 0)
				mtk_dp_fake_plugin(SINK_640_480, 1);
			else if (strncmp(opt + 12, "enable480p10", 12) == 0)
				mtk_dp_fake_plugin(SINK_640_480, 2);

			else if (strncmp(opt + 12, "enable1080p6", 12) == 0)
				mtk_dp_fake_plugin(SINK_1920_1080, 0);
			else if (strncmp(opt + 12, "enable1080p8", 12) == 0)
				mtk_dp_fake_plugin(SINK_1920_1080, 1);
			else if (strncmp(opt + 12, "enable1080p10", 13) == 0)
				mtk_dp_fake_plugin(SINK_1920_1080, 2);
		} else
			DDPINFO("fakecablein error msg\n");
	} else if (strncmp(opt, "fec:", 4) == 0) {
		if (strncmp(opt + 4, "enable", 6) == 0)
			mtk_dp_fec_enable(1);
		else if (strncmp(opt + 4, "disable", 7) == 0)
			mtk_dp_fec_enable(0);
		else
			DDPINFO("fec:enable/disable error msg\n");

	} else if (strncmp(opt, "power:", 6) == 0) {
		if (strncmp(opt + 6, "on", 2) == 0)
			mtk_dp_power_save(0x1);
		else if (strncmp(opt + 6, "off", 3) == 0)
			mtk_dp_power_save(0x0);

	} else if (strncmp(opt, "audio:", 6) == 0) {
		if (strncmp(opt + 6, "2ch", 3) == 0)
			mtk_dp_force_audio(0, 0xff, 0xff);
		else if (strncmp(opt + 6, "8ch", 3) == 0)
			mtk_dp_force_audio(6, 0xff, 0xff);

		if (strncmp(opt + min_t(int, strlen(opt), 9), "32fs", 4) == 0)
			mtk_dp_force_audio(0xff, 0, 0xff);
		else if (strncmp(opt + min_t(int, strlen(opt), 9), "44fs", 4) == 0)
			mtk_dp_force_audio(0xff, 1, 0xff);
		else if (strncmp(opt + min_t(int, strlen(opt), 9), "48fs", 4) == 0)
			mtk_dp_force_audio(0xff, 2, 0xff);
		else if (strncmp(opt + min_t(int, strlen(opt), 9), "96fs", 4) == 0)
			mtk_dp_force_audio(0xff, 3, 0xff);
		else if (strncmp(opt + min_t(int, strlen(opt), 9), "192f", 4) == 0)
			mtk_dp_force_audio(0xff, 4, 0xff);

		if (strncmp(opt + min_t(int, strlen(opt), 13), "16bit", 5) == 0)
			mtk_dp_force_audio(0xff, 0xff, 0);
		else if (strncmp(opt + min_t(int, strlen(opt), 13), "20bit", 5) == 0)
			mtk_dp_force_audio(0xff, 0xff, 1);
		else if (strncmp(opt + min_t(int, strlen(opt), 13), "24bit", 5) == 0)
			mtk_dp_force_audio(0xff, 0xff, 2);

	} else if (strncmp(opt, "dptest:", 7) == 0) {
		if (strncmp(opt + 7, "2", 1) == 0)
			mtk_dp_test(2);
		else if (strncmp(opt + 7, "4", 1) == 0)
			mtk_dp_test(4);
		else if (strncmp(opt + 7, "8", 1) == 0)
			mtk_dp_test(8);
		else
			DDPINFO("dptest error msg\n");
	} else if (strncmp(opt, "debug_log:", 10) == 0) {
		if (strncmp(opt + 10, "on", 2) == 0)
			mtk_dp_debug_enable(true);
		else if (strncmp(opt + 10, "off", 3) == 0)
			mtk_dp_debug_enable(false);
	} else if (strncmp(opt, "force_hdcp13:", 13) == 0) {
		if (strncmp(opt + 13, "enable", 6) == 0)
			mtk_dp_force_hdcp1x(true);
		else
			mtk_dp_force_hdcp1x(false);

	} else if (strncmp(opt, "hdcp:", 5) == 0) {
		if (strncmp(opt + 5, "enable", 6) == 0)
			mtk_dp_hdcp_enable(true);
		else if (strncmp(opt + 5, "disable", 7) == 0)
			mtk_dp_hdcp_enable(false);
	} else if (strncmp(opt, "adjust_phy:", 11) == 0) {
		int ret = 0;
		int index, c0, cp1;

		ret = sscanf(opt, "adjust_phy:%d,%d,%d\n", &index, &c0, &cp1);
		if (ret != 3) {
			DPTXERR("ret = %d\n", ret);
			return;
		}

		mtk_dp_set_adjust_phy(index, c0, cp1);
	} else if (strncmp(opt, "setpowermode", 12) == 0) {
		mtk_dp_SWInterruptSet(2);
		mdelay(100);
		mtk_dp_SWInterruptSet(4);
	} else if (strncmp(opt, "pattern:", 8) == 0) {
		int ret = 0;
		int enable, resolution;

		ret = sscanf(opt, "pattern:%d,%d\n", &enable, &resolution);
		if (ret != 2) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}

		DPTXMSG("Paterrn Gen:enable = %d, resolution =%d\n",
			enable, resolution);
		mdrv_DPTx_PatternSet(enable, resolution);
	} else if (strncmp(opt, "maxlinkrate:", 12) == 0) {
		int ret = 0;
		int enable, maxlinkrate;

		ret = sscanf(opt, "maxlinkrate:%d,%d\n", &enable, &maxlinkrate);
		if (ret != 2) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}

		DPTXMSG("set max link rate:enable = %d, maxlinkrate =%d\n",
			enable, maxlinkrate);
		mdrv_DPTx_set_maxlinkrate(enable, maxlinkrate);
	} else if (strncmp(opt, "max2lane:", 9) == 0) {
		int ret = 0;
		int enable;

		ret = sscanf(opt, "max2lane:%d\n", &enable);
		if (ret != 1) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}

		DPTXMSG("set max 2lane enable = %d\n", enable);
		mtk_dp_set_force_2lane(enable ? true : false);
	} else if (strncmp(opt, "video_clock:", 12) == 0) {
		int ret = 0;
		unsigned int clksrc;
		unsigned int con1;

		ret = sscanf(opt, "video_clock:%x,%x\n", &clksrc, &con1);
		if (ret != 2) {
			DPTXERR("ret = %d\n", ret);
			return;
		}
		mtk_dp_clock_debug(clksrc, con1);
	} else if (strncmp(opt, "dump:", 5) == 0) {
		dptx_dump_reg();
	} else if (strncmp(opt, "write:", 6) == 0) {
		u32 data = 0;
		u32 addr = 0;
		int ret = 0;

		ret = sscanf(opt, "write:0x%x=0x%x", &addr, &data);
		if (ret != 2) {
			DPTXERR("ret = %d\n", ret);
			return;
		}

		dptx_write_reg(addr, data);
	} else if (strncmp(opt, "read:", 5) == 0) {
		u32 addr = 0;
		int ret = 0;

		ret = sscanf(opt, "read:0x%x", &addr);
		if (ret != 1) {
			DPTXERR("ret = %d\n", ret);
			return;
		}

		dptx_read_reg(addr);
	} else if (strncmp(opt, "phy_write:", 10) == 0) {
		u32 data = 0;
		u32 addr = 0;
		int ret = 0;

		ret = sscanf(opt, "phy_write:0x%x=0x%x", &addr, &data);
		if (ret != 2) {
			DPTXERR("ret = %d\n", ret);
			return;
		}

		dptx_phy_write_reg(addr, data);
	} else if (strncmp(opt, "phy_read:", 9) == 0) {
		u32 addr = 0;
		int ret = 0;

		ret = sscanf(opt, "phy_read:0x%x", &addr);
		if (ret != 1) {
			DPTXERR("ret = %d\n", ret);
			return;
		}

		dptx_phy_read_reg(addr);
	} else if (strncmp(opt, "dpintf_pattern:", 15) == 0) {
		u32 index = 0;
		int ret = 0;

		ret = sscanf(opt, "dpintf_pattern:%d", &index);
		if (ret != 1) {
			DPTXERR("ret = %d\n", ret);
			return;
		}

		if (index >= ARRAY_SIZE(edid_cea_modes)) {
			DPTXERR("wrong index[%d] >= array size[%lu]\n", index, ARRAY_SIZE(edid_cea_modes));
			return;
		}

		mtk_dp_mode_set(&edid_cea_modes[index]);

		mtk_dp_intf_pattern(&edid_cea_modes[index]);
	}
}


#ifdef MTK_eDPINFO
#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *mtkedp_dbgfs;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
static struct proc_dir_entry *mtkedp_procfs;
#endif

struct mtk_edp_debug_info {
	char *name;
	uint8_t index;
};

enum mtk_edp_debug_index {
	DP_INFO_HDCP      = 0,
	DP_INFO_PHY       = 1,
	DP_INFO_MAX
};

static struct mtk_edp_debug_info edp_info[DP_INFO_MAX] = {
	{"HDCP", DP_INFO_HDCP},
	{"PHY", DP_INFO_PHY},
};

static uint8_t g_infoIndex = DP_INFO_HDCP;

static int mtk_edp_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t mtk_edp_debug_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int ret = 0;
	char *buffer;

	buffer = kmalloc(PAGE_SIZE/8, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	switch (g_infoIndex) {
	case DP_INFO_HDCP:
		ret = mtk_dp_hdcp_getInfo(buffer, PAGE_SIZE/8);
		break;
	case DP_INFO_PHY:
		ret = mtk_dp_phy_getInfo(buffer, PAGE_SIZE/8);
		break;
	default:
		eDPTXERR("Invalid inedx!");
	}

	if (ret > 0)
		ret = simple_read_from_buffer(ubuf, count, ppos, buffer, ret);

	kfree(buffer);
	return ret;
}

static void mtk_edp_process_dbg_opt(const char *opt)
{
	int i = 0;

	for (i = 0; i < DP_INFO_MAX; i++) {
		if (!strncmp(opt, edp_info[i].name, strlen(edp_info[i].name))) {
			g_infoIndex = edp_info[i].index;
			break;
		}
	}

	if (g_infoIndex == DP_INFO_MAX)
		g_infoIndex = DP_INFO_HDCP;
}

static ssize_t mtk_edp_debug_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const int debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512];
	char *tok;
	char *cmd = cmd_buffer;

	ret = count;
	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count) != 0ULL)
		return -EFAULT;

	cmd_buffer[count] = '\0';

	eDPTXMSG("[mtkedp_dbg]%s!\n", cmd_buffer);
	while ((tok = strsep(&cmd, " ")) != NULL)
		mtk_edp_process_dbg_opt(tok);

	return ret;
}

static const struct file_operations edp_debug_fops = {
	.read = mtk_edp_debug_read,
	.write = mtk_edp_debug_write,
	.open = mtk_edp_debug_open,
};

static const struct proc_ops edp_debug_proc_fops = {
	.proc_read = mtk_edp_debug_read,
	.proc_write = mtk_edp_debug_write,
	.proc_open = mtk_edp_debug_open,
};

int mtk_dp_debugfs_init(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	mtkedp_dbgfs = debugfs_create_file("mtk_edpinfo", 0644,
		NULL, NULL, &edp_debug_fops);
	if (IS_ERR_OR_NULL(mtkedp_dbgfs))
		return -ENOMEM;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	mtkedp_procfs = proc_create("mtk_edpinfo", 0644, NULL,
		&edp_debug_proc_fops);
	if (IS_ERR_OR_NULL(mtkedp_procfs))
		return -ENOMEM;
#endif
	return 0;
}

void mtk_dp_debugfs_deinit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(mtkedp_dbgfs);
	mtkedp_dbgfs = NULL;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	if (mtkedp_procfs) {
		proc_remove(mtkedp_procfs);
		mtkedp_procfs = NULL;
	}
#endif
}
#endif


