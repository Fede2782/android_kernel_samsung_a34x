// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include <mbraink_ioctl_struct_def.h>
#include <mbraink_modules_ops_def.h>
#include <bridge/mbraink_bridge_gps.h>

void mbraink_v6899_get_gnss_lp_data(struct mbraink_gnss2mbr_lp_data *gnss_lp_buffer)
{
	enum gnss2mbr_status ret;
	struct gnss2mbr_lp_data lp_data;
	unsigned short count = 0;

	do {
		memset(&lp_data, 0, sizeof(lp_data));
		lp_data.hdr_in.data_size = sizeof(lp_data);
		lp_data.hdr_in.major_ver = LP_DATA_MAJOR_VER;
		lp_data.hdr_in.minor_ver = LP_DATA_MINOR_VER;

		ret = mbraink_bridge_gps_get_lp_data(MBR2GNSS_TEST, &lp_data);
		if (ret == GNSS2MBR_NO_DATA)
			break;

		if (gnss_lp_buffer->count >= MAX_GNSS_DATA_SZ)
			break;

		if (lp_data.hdr_out.data_size != lp_data.hdr_in.data_size) {
			pr_info("%s: out.data_size is not match in.data_size : %u, %u\n",
				__func__,
				lp_data.hdr_out.data_size,
				lp_data.hdr_in.data_size);
			break;
		}

		count = gnss_lp_buffer->count;

		gnss_lp_buffer->lp_data[count].dump_ts = lp_data.dump_ts;
		gnss_lp_buffer->lp_data[count].dump_index = lp_data.dump_index;
		gnss_lp_buffer->lp_data[count].gnss_mcu_sid = lp_data.gnss_mcu_sid;
		gnss_lp_buffer->lp_data[count].gnss_mcu_is_on = (u8)(lp_data.gnss_mcu_is_on);
		gnss_lp_buffer->lp_data[count].gnss_pwr_is_hi = (u8)(lp_data.gnss_pwr_is_hi);
		gnss_lp_buffer->lp_data[count].gnss_pwr_wrn = (u8)(lp_data.gnss_pwr_wrn);
		gnss_lp_buffer->lp_data[count].gnss_pwr_wrn_cnt = (u8)(lp_data.gnss_pwr_wrn_cnt);
		gnss_lp_buffer->count++;
	} while (ret == GNSS2MBR_OK_MORE);

}

void mbraink_v6899_get_gnss_mcu_data(struct mbraink_gnss2mbr_mcu_data *gnss_mcu_buffer)
{
	enum gnss2mbr_status ret;
	struct gnss2mbr_mcu_data mcu_data;
	unsigned short count = 0;

	do {
		memset(&mcu_data, 0, sizeof(mcu_data));
		mcu_data.hdr_in.data_size = sizeof(mcu_data);
		mcu_data.hdr_in.major_ver = MCU_DATA_MAJOR_VER;
		mcu_data.hdr_in.minor_ver = MCU_DATA_MINOR_VER;

		ret = mbraink_bridge_gps_get_mcu_data(MBR2GNSS_TEST, &mcu_data);
		if (ret == GNSS2MBR_NO_DATA)
			break;

		if (gnss_mcu_buffer->count >= MAX_GNSS_DATA_SZ)
			break;

		if (mcu_data.hdr_out.data_size != mcu_data.hdr_in.data_size) {
			pr_info("%s: out.data_size is not match in.data_size : %u, %u\n",
				__func__,
				mcu_data.hdr_out.data_size,
				mcu_data.hdr_in.data_size);
			break;
		}

		count = gnss_mcu_buffer->count;

		gnss_mcu_buffer->mcu_data[count].gnss_mcu_sid = mcu_data.gnss_mcu_sid;
		gnss_mcu_buffer->mcu_data[count].clock_cfg_val = mcu_data.clock_cfg_val;
		gnss_mcu_buffer->mcu_data[count].open_ts = mcu_data.open_ts;
		gnss_mcu_buffer->mcu_data[count].open_duration = mcu_data.open_duration;
		gnss_mcu_buffer->mcu_data[count].has_exception = (u8)(mcu_data.has_exception);
		gnss_mcu_buffer->mcu_data[count].force_close = (u8)(mcu_data.force_close);
		gnss_mcu_buffer->mcu_data[count].close_ts = mcu_data.close_ts;
		gnss_mcu_buffer->mcu_data[count].close_duration = mcu_data.close_duration;
		gnss_mcu_buffer->mcu_data[count].open_duration_max = mcu_data.open_duration_max;
		gnss_mcu_buffer->mcu_data[count].close_duration_max = mcu_data.close_duration_max;
		gnss_mcu_buffer->mcu_data[count].exception_cnt = mcu_data.exception_cnt;
		gnss_mcu_buffer->mcu_data[count].force_close_cnt = mcu_data.force_close_cnt;
		gnss_mcu_buffer->count++;
	} while (ret == GNSS2MBR_OK_MORE);

}

static struct mbraink_gps_ops mbraink_v6899_gps_ops = {
	.get_gnss_lp_data = mbraink_v6899_get_gnss_lp_data,
	.get_gnss_mcu_data = mbraink_v6899_get_gnss_mcu_data,
};

int mbraink_v6899_gps_init(void)
{
	int ret = 0;

	ret = register_mbraink_gps_ops(&mbraink_v6899_gps_ops);

	return ret;
}

int mbraink_v6899_gps_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_gps_ops();

	return ret;
}
