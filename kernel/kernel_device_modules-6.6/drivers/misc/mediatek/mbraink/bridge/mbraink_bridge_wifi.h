/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_WIFI_H
#define MBRAINK_BRIDGE_WIFI_H

enum wifi2mbr_status {
	WIFI2MBR_SUCCESS,
	WIFI2MBR_FAILURE,
	WIFI2MBR_END,
	WIFI2MBR_NO_OPS,
};

enum mbr2wifi_reason {
	MBR2WIFI_TRX_BIG_DATA,
	MBR2WIFI_TEST_LP_RATIO,
	MBR2WIFI_TX_TIMEOUT,
};

struct wifi2mbr_hdr {
	unsigned short tag;
	unsigned short ver;
	unsigned char ucReserved[4];
};

/* naming rule: WIFI2MBR_TAG_<module/feature>_XXX */
enum wifi2mbr_tag {
	WIFI2MBR_TAG_LLS_RATE,
	WIFI2MBR_TAG_LLS_RADIO,
	WIFI2MBR_TAG_LLS_AC,
	WIFI2MBR_TAG_LP_RATIO,
	WIFI2MBR_TAG_TXTIMEOUT,
	WIFI2MBR_TAG_MAX
};

#define LLS_RATE_PREAMBIE_MASK (GENMASK(2, 0))
#define LLS_RATE_NSS_MASK (GENMASK(4, 3))
#define LLS_RATE_BW_MASK (GENMASK(7, 5))
#define LLS_RATE_MCS_IDX_MASK (GENMASK(15, 8))
#define LLS_RATE_RSVD_MASK (GENMASK(31, 16))

/* struct for WIFI2MBR_TAG_LLS_RATE */
struct wifi2mbr_llsRateInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	unsigned int rate_idx; /* rate idx, ref: LLS_RATE_XXX_MASK */
	unsigned int bitrate;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int mpdu_lost;
	unsigned int retries;
};

/* struct for WIFI2MBR_TAG_LLS_RADIO */
struct wifi2mbr_llsRadioInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	int radio;
	unsigned int on_time;
	unsigned int tx_time;
	unsigned int rx_time;
	unsigned int on_time_scan;
	unsigned int on_time_roam_scan;
	unsigned int on_time_pno_scan;
};

enum enum_mbr_wifi_ac {
	MBR_WIFI_AC_VO,
	MBR_WIFI_AC_VI,
	MBR_WIFI_AC_BE,
	MBR_WIFI_AC_BK,
	MBR_WIFI_AC_MAX,
};

/* struct for WIFI2MBR_TAG_LLS_AC */
struct wifi2mbr_llsAcInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	enum enum_mbr_wifi_ac ac;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int tx_mcast;
	unsigned int tx_ampdu;
	unsigned int mpdu_lost;
	unsigned int retries;
	unsigned int contention_time_min;
	unsigned int contention_time_max;
	unsigned int contention_time_avg;
	unsigned int contention_num_samples;
};

/* struct for WIFI2MBR_TAG_LP_RATIO */
struct wifi2mbr_lpRatioInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	int radio;
	unsigned int total_time;
	unsigned int tx_time;
	unsigned int rx_time;
	unsigned int rx_listen_time;
	unsigned int sleep_time;
};

struct wifi2mbr_TxTimeoutInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	unsigned int token_id;
	unsigned int wlan_index;
	unsigned int bss_index;
	unsigned int timeout_duration;
	unsigned int operation_mode;
	unsigned int idle_slot_diff_cnt;
};

struct mbraink2wifi_ops {
	enum wifi2mbr_status (*get_data)(void *priv,
				enum mbr2wifi_reason reason,
				enum wifi2mbr_tag tag,
				void *data, unsigned short *real_len);
	void *priv;
};

void mbraink_bridge_wifi_init(void);
void mbraink_bridge_wifi_deinit(void);
void register_wifi2mbraink_ops(struct mbraink2wifi_ops *ops);
void unregister_wifi2mbraink_ops(void);
enum wifi2mbr_status
mbraink_bridge_wifi_get_data(enum mbr2wifi_reason reason,
			enum wifi2mbr_tag tag,
			void *data,
			unsigned short *real_len);
#endif /*MBRAINK_BRIDGE_WIFI_H*/
