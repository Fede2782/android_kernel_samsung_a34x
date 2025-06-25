/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _HF_SENSOR_IO_H_
#define _HF_SENSOR_IO_H_

#include <linux/ioctl.h>
#include <linux/types.h>

enum {
	HF_MANAGER_SENSOR_DISABLE,
	HF_MANAGER_SENSOR_ENABLE,
	HF_MANAGER_SENSOR_FLUSH,
	HF_MANAGER_SENSOR_ENABLE_CALI,
	HF_MANAGER_SENSOR_CONFIG_CALI,
	HF_MANAGER_SENSOR_SELFTEST,
	HF_MANAGER_SENSOR_RAWDATA,
	HF_MANAGER_SENSOR_MAX_ACTION,
};

enum {
	DATA_ACTION,
	FLUSH_ACTION,
	BIAS_ACTION,
	CALI_ACTION,
	TEMP_ACTION,
	TEST_ACTION,
	RAW_ACTION,
	MAX_ACTION,
};

struct hf_manager_batch {
	int64_t delay;
	int64_t latency;
} __packed __aligned(4);

struct hf_manager_cmd {
	uint8_t sensor_type;
	uint8_t action : 4;
	uint8_t down_sample : 1;
	uint8_t length;
	uint8_t padding[1];
	int8_t data[48] __aligned(4);
} __packed __aligned(4);

struct hf_manager_event {
	int64_t timestamp;
	uint8_t sensor_type;
	uint8_t accurancy;
	uint8_t action;
	uint8_t padding[1];
	union {
		int32_t word[16];
		int8_t byte[64];
	};
} __packed __aligned(4);

struct sensor_info {
	uint8_t sensor_type;
	uint8_t padding[3];
	uint32_t gain;
	char name[16];
	char vendor[16];
} __packed __aligned(4);

struct custom_cmd {
	uint8_t command;
	uint8_t tx_len;
	uint8_t rx_len;
	uint8_t padding[1];
	union {
		int32_t data[15];
		int32_t word[15];
		int8_t byte[60];
	};
} __packed __aligned(4);

struct common_packet {
	uint8_t sensor_type;
	uint8_t padding[3];
	bool status;
} __packed __aligned(4);

struct info_packet {
	uint8_t sensor_type;
	uint8_t padding[3];
	struct sensor_info info;
} __packed __aligned(4);

struct cust_packet {
	uint8_t sensor_type;
	uint8_t padding[3];
	struct custom_cmd cust_cmd;
} __packed __aligned(4);

struct debug_packet {
	uint8_t sensor_type;
	uint8_t padding[3];
	uint8_t *write_buffer;
	uint32_t write_size;
	uint8_t *read_buffer;
	uint32_t read_size;
} __packed __aligned(4);

#define HF_MANAGER_REQUEST_REGISTER_STATUS  _IOWR('a', 1, struct common_packet)
#define HF_MANAGER_REQUEST_BIAS_DATA        _IOW('a', 2, struct common_packet)
#define HF_MANAGER_REQUEST_CALI_DATA        _IOW('a', 3, struct common_packet)
#define HF_MANAGER_REQUEST_TEMP_DATA        _IOW('a', 4, struct common_packet)
#define HF_MANAGER_REQUEST_TEST_DATA        _IOW('a', 5, struct common_packet)
#define HF_MANAGER_REQUEST_SENSOR_INFO      _IOWR('a', 6, struct info_packet)
#define HF_MANAGER_REQUEST_CUST_DATA        _IOWR('a', 7, struct cust_packet)
#define HF_MANAGER_REQUEST_READY_STATUS     _IOWR('a', 8, struct common_packet)
#define HF_MANAGER_REQUEST_DEBUG_INFO       _IOWR('a', 9, struct debug_packet)

#endif
