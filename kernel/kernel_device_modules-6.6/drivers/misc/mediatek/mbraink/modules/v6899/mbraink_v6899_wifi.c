// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include <mbraink_ioctl_struct_def.h>
#include <mbraink_modules_ops_def.h>
#include <bridge/mbraink_bridge_wifi.h>

#define MAX_WIFI_DATA_CNT 8192

void mbraink_v6899_get_wifi_rate_data(int current_idx,
				struct mbraink_wifi2mbr_lls_rate_data *rate_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_llsRateInfo lls_rate;
	int loop = 0;
	int cnt = 0;

	memset(rate_buffer, 0, sizeof(struct mbraink_wifi2mbr_lls_rate_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_BIG_DATA,
						WIFI2MBR_TAG_LLS_RATE,
						(void *)(&lls_rate), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = rate_buffer->count;

			if (cnt < MAX_WIFI_RATE_SZ) {
				rate_buffer->rate_data[cnt].timestamp = lls_rate.timestamp;
				rate_buffer->rate_data[cnt].rate_idx = lls_rate.rate_idx;
				rate_buffer->rate_data[cnt].bitrate = lls_rate.bitrate;
				rate_buffer->rate_data[cnt].tx_mpdu = lls_rate.tx_mpdu;
				rate_buffer->rate_data[cnt].rx_mpdu = lls_rate.rx_mpdu;
				rate_buffer->rate_data[cnt].mpdu_lost = lls_rate.mpdu_lost;
				rate_buffer->rate_data[cnt].retries = lls_rate.retries;
				rate_buffer->count++;

				if (cnt == MAX_WIFI_RATE_SZ - 1) {
					rate_buffer->idx = current_idx + rate_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			rate_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

void mbraink_v6899_get_wifi_radio_data(struct mbraink_wifi2mbr_lls_radio_data *radio_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_llsRadioInfo lls_radio;
	int loop = 0;
	int cnt = 0;

	memset(radio_buffer, 0, sizeof(struct mbraink_wifi2mbr_lls_radio_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_BIG_DATA,
						WIFI2MBR_TAG_LLS_RADIO,
						(void *)(&lls_radio), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = radio_buffer->count;

			if (cnt < MAX_WIFI_RADIO_SZ) {
				radio_buffer->radio_data[cnt].timestamp = lls_radio.timestamp;
				radio_buffer->radio_data[cnt].radio = lls_radio.radio;
				radio_buffer->radio_data[cnt].on_time = lls_radio.on_time;
				radio_buffer->radio_data[cnt].tx_time = lls_radio.tx_time;
				radio_buffer->radio_data[cnt].rx_time = lls_radio.rx_time;
				radio_buffer->radio_data[cnt].on_time_scan = lls_radio.on_time_scan;
				radio_buffer->radio_data[cnt].on_time_roam_scan =
					lls_radio.on_time_roam_scan;
				radio_buffer->radio_data[cnt].on_time_pno_scan =
					lls_radio.on_time_pno_scan;
				radio_buffer->count++;
			} else {
				pr_info("%s: index is invalid\n", __func__);
				break;
			}
		} else if (ret ==  WIFI2MBR_END)
			break;
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);

	radio_buffer->idx = 0;
}

void mbraink_v6899_get_wifi_ac_data(struct mbraink_wifi2mbr_lls_ac_data *ac_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_llsAcInfo lls_ac;
	int loop = 0;
	int cnt = 0;

	memset(ac_buffer, 0, sizeof(struct mbraink_wifi2mbr_lls_ac_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_BIG_DATA,
						WIFI2MBR_TAG_LLS_AC,
						(void *)(&lls_ac), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = ac_buffer->count;

			if (cnt < MBRAINK_MBR_WIFI_AC_MAX) {
				ac_buffer->ac_data[cnt].timestamp = lls_ac.timestamp;
				ac_buffer->ac_data[cnt].ac =
					(enum mbraink_enum_mbr_wifi_ac)(lls_ac.ac);
				ac_buffer->ac_data[cnt].tx_mpdu = lls_ac.tx_mpdu;
				ac_buffer->ac_data[cnt].rx_mpdu = lls_ac.rx_mpdu;
				ac_buffer->ac_data[cnt].tx_mcast = lls_ac.tx_mcast;
				ac_buffer->ac_data[cnt].tx_ampdu = lls_ac.tx_ampdu;
				ac_buffer->ac_data[cnt].mpdu_lost = lls_ac.mpdu_lost;
				ac_buffer->ac_data[cnt].retries = lls_ac.retries;
				ac_buffer->ac_data[cnt].contention_time_min =
					lls_ac.contention_time_min;
				ac_buffer->ac_data[cnt].contention_time_max =
					lls_ac.contention_time_max;
				ac_buffer->ac_data[cnt].contention_time_avg =
					lls_ac.contention_time_avg;
				ac_buffer->ac_data[cnt].contention_num_samples =
					lls_ac.contention_num_samples;
				ac_buffer->count++;
			} else {
				pr_info("%s: index is invalid\n", __func__);
				break;
			}
		} else if (ret ==  WIFI2MBR_END)
			break;
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);

	ac_buffer->idx = 0;
}

void mbraink_v6899_get_wifi_lp_data(struct mbraink_wifi2mbr_lp_ratio_data *lp_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_lpRatioInfo lp_ratio;
	int loop = 0;
	int cnt = 0;

	memset(lp_buffer, 0, sizeof(struct mbraink_wifi2mbr_lp_ratio_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TEST_LP_RATIO,
						WIFI2MBR_TAG_LP_RATIO,
						(void *)(&lp_ratio), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = lp_buffer->count;
			if (cnt < MAX_WIFI_LP_SZ) {
				lp_buffer->lp_data[cnt].timestamp = lp_ratio.timestamp;
				lp_buffer->lp_data[cnt].radio = lp_ratio.radio;
				lp_buffer->lp_data[cnt].total_time = lp_ratio.total_time;
				lp_buffer->lp_data[cnt].tx_time = lp_ratio.tx_time;
				lp_buffer->lp_data[cnt].rx_time = lp_ratio.rx_time;
				lp_buffer->lp_data[cnt].rx_listen_time = lp_ratio.rx_listen_time;
				lp_buffer->lp_data[cnt].sleep_time = lp_ratio.sleep_time;
				lp_buffer->count++;
			} else {
				pr_info("%s: index is invalid\n", __func__);
				break;
			}
		} else if (ret ==  WIFI2MBR_END)
			break;
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);

	lp_buffer->idx = 0;
}

void mbraink_v6899_get_wifi_txtimeout_data(int current_idx,
				struct mbraink_wifi2mbr_txtimeout_data *txtimeout_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_TxTimeoutInfo tx_timeout;
	int loop = 0;
	int cnt = 0;

	memset(txtimeout_buffer, 0, sizeof(struct mbraink_wifi2mbr_txtimeout_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TX_TIMEOUT,
						WIFI2MBR_TAG_TXTIMEOUT,
						(void *)(&tx_timeout), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = txtimeout_buffer->count;

			if (cnt < MAX_WIFI_TXTIMEOUT_SZ) {
				txtimeout_buffer->txtimeout_data[cnt].timestamp =
								tx_timeout.timestamp;
				txtimeout_buffer->txtimeout_data[cnt].token_id =
								tx_timeout.token_id;
				txtimeout_buffer->txtimeout_data[cnt].wlan_index =
								tx_timeout.wlan_index;
				txtimeout_buffer->txtimeout_data[cnt].bss_index =
								tx_timeout.bss_index;
				txtimeout_buffer->txtimeout_data[cnt].timeout_duration =
								tx_timeout.timeout_duration;
				txtimeout_buffer->txtimeout_data[cnt].operation_mode =
								tx_timeout.operation_mode;
				txtimeout_buffer->txtimeout_data[cnt].idle_slot_diff_cnt =
								tx_timeout.idle_slot_diff_cnt;
				txtimeout_buffer->count++;

				if (cnt == MAX_WIFI_TXTIMEOUT_SZ - 1) {
					txtimeout_buffer->idx = current_idx + txtimeout_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			txtimeout_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

static struct mbraink_wifi_ops mbraink_v6899_wifi_ops = {
	.get_wifi_rate_data = mbraink_v6899_get_wifi_rate_data,
	.get_wifi_radio_data = mbraink_v6899_get_wifi_radio_data,
	.get_wifi_ac_data = mbraink_v6899_get_wifi_ac_data,
	.get_wifi_lp_data = mbraink_v6899_get_wifi_lp_data,
	.get_wifi_txtimeout_data = mbraink_v6899_get_wifi_txtimeout_data,
};

int mbraink_v6899_wifi_init(void)
{
	int ret = 0;

	ret = register_mbraink_wifi_ops(&mbraink_v6899_wifi_ops);

	return ret;
}

int mbraink_v6899_wifi_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_wifi_ops();

	return ret;
}

