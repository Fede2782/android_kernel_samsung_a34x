/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_GPS_H
#define MBRAINK_BRIDGE_GPS_H

#include <linux/types.h>

enum gnss2mbr_status {
	GNSS2MBR_NO_DATA,
	GNSS2MBR_OK_NO_MORE,
	GNSS2MBR_OK_MORE,
};

enum mbr2gnss_reason {
	MBR2GNSS_TEST,
	MBR2GNSS_PERIODIC,
	MBR2GNSS_APP_SWITCH,
	MBR2GNSS_AP_RESUME,
};

struct gnss2mbr_data_hdr {
	u8 major_ver;
	u8 minor_ver;
	u16 data_size;
	u32 reserved;
};

#define LP_DATA_MAJOR_VER (1)
#define LP_DATA_MINOR_VER (1)
struct gnss2mbr_lp_data {
	/* fill by the one to get data */
	struct gnss2mbr_data_hdr hdr_in;

	/* fill by the data provider */
	struct gnss2mbr_data_hdr hdr_out;
	u64 dump_ts;
	u32 dump_index;
	u32 gnss_mcu_sid;
	bool gnss_mcu_is_on;

	/* N/A if gnss_mcu_is_on = false */
	bool gnss_pwr_is_hi;
	bool gnss_pwr_wrn;

	/* history statistic */
	u32 gnss_pwr_wrn_cnt;
};

#define MCU_DATA_MAJOR_VER (1)
#define MCU_DATA_MINOR_VER (1)
struct gnss2mbr_mcu_data {
	/* fill by the one to get data */
	struct gnss2mbr_data_hdr hdr_in;

	/* fill by the data provider */
	struct gnss2mbr_data_hdr hdr_out;
	u32 gnss_mcu_sid; /* last finished one */
	u32 clock_cfg_val;

	u64 open_ts;
	u32 open_duration;

	bool has_exception;
	bool force_close;

	u64 close_ts;
	u32 close_duration;

	/* history statistic */
	u32 open_duration_max;
	u32 close_duration_max;
	u32 exception_cnt;
	u32 force_close_cnt;
};

struct mbraink2gps_ops {
	enum gnss2mbr_status (*get_lp_data)(enum mbr2gnss_reason reason,
					struct gnss2mbr_lp_data *lp_data);
	enum gnss2mbr_status (*get_mcu_data)(enum mbr2gnss_reason reason,
					struct gnss2mbr_mcu_data *mcu_data);
};

void mbraink_bridge_gps_init(void);
void mbraink_bridge_gps_deinit(void);
void register_gps2mbraink_ops(struct mbraink2gps_ops *ops);
void unregister_gps2mbraink_ops(void);
enum gnss2mbr_status mbraink_bridge_gps_get_lp_data(enum mbr2gnss_reason reason,
						struct gnss2mbr_lp_data *lp_data);
enum gnss2mbr_status mbraink_bridge_gps_get_mcu_data(enum mbr2gnss_reason reason,
						struct gnss2mbr_mcu_data *mcu_data);
#endif
