// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <swpm_audio_v6991.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

static ssize_t audio_scenario_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	void *ptr;

	if (!ToUser)
		return -EINVAL;

	ptr = audio_get_power_scenario();
	if (ptr) {
		audio_data = *(struct audio_swpm_data *)ptr;
		swpm_dbg_log(
			"%s(), ON %u, user %u, out %u, in %u, adda %u, rate %u, ch %u, freq %u, D0 %u\n",
			__func__, audio_data.afe_on,
			audio_data.user_case, audio_data.output_device,
			audio_data.input_device, audio_data.adda_mode,
			audio_data.sample_rate, audio_data.channel_num,
			audio_data.freq_clock, audio_data.D0_ratio);
	} else {
		swpm_dbg_log("return value is null pointer\n");
	}
	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op audio_scenario_fops = {
	.fs_read = audio_scenario_read,
};

int __init swpm_audio_v6991_dbg_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("audio_scenario"
			, 0444, &audio_scenario_fops, NULL, NULL);

	swpm_audio_v6991_init();

	pr_notice("%s(), swpm audio install success\n", __func__);

	return 0;
}

void __exit swpm_audio_v6991_dbg_exit(void)
{
	swpm_audio_v6991_exit();
}

module_init(swpm_audio_v6991_dbg_init);
module_exit(swpm_audio_v6991_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6991 SWPM audio debug module");
MODULE_AUTHOR("MediaTek Inc.");
