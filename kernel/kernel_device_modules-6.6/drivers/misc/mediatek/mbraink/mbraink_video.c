// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>

#include "mbraink_video.h"

#if IS_ENABLED(CONFIG_MTK_LPM_MT6985) && \
	IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCODEC) && \
	IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCODEC_V2) && \
	IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
int mbraink_get_video_info(char *buffer)
{
	int n = 0;

	n += sprintf(buffer + n,
		"%d\n%d\n%d\n%d\n%d\n%d\n", mbraink_dec_is_power_on[0],
		mbraink_dec_is_power_on[1], mbraink_dec_is_power_on[2],
		mbraink_dec_is_power_on[3], mbraink_dec_is_power_on[4],
		mbraink_vcp_is_power_on);

	/*pr_info("Video Info size: %d, sec = %lld\n", n, tv.tv_sec);*/

	buffer[n] = '\0';

	return n;
}
#else
int mbraink_get_video_info(char *buffer)
{
	pr_info("%s: Do not support ioctl video query.\n", __func__);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC)
#include <mtk_vcodec_drv.h>
int mtk_vcodec_get_op_by_pid(enum mtk_instance_type type, int pid);

int mbraink_get_vdec_fps_info(unsigned short pid)
{
	int fps = 0;

	fps = mtk_vcodec_get_op_by_pid(0, (int)(pid));
	pr_info("%s: pid=%u, fps = %d\n", __func__, pid, fps);
	return fps;
}
#else
int mbraink_get_vdec_fps_info(unsigned short pid)
{
	return 0;
}
#endif
