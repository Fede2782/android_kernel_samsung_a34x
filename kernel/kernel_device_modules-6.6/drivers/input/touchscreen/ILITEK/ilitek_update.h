/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of ILITEK CommonFlow
 *
 * Copyright (c) 2022 ILI Technology Corp.
 * Copyright (c) 2022 Luca Hsu <luca_hsu@ilitek.com>
 * Copyright (c) 2022 Joe Hung <joe_hung@ilitek.com>
 */

#ifndef __ILITEK_UPDATE_H__
#define __ILITEK_UPDATE_H__

#include "ilitek_protocol.h"

/*
 * [20221214] Linux_I2CDriver_ITS_V5_9_1_0
 * UPDATE_CODE_VERSION			0x00000002
 */
#define UPDATE_CODE_VERSION		0x00000002

#define UPDATE_UI_INFO(fmt, ...)				\
	do {							\
		TP_MSG(fmt, ##__VA_ARGS__);			\
		_sprintf(g_info, fmt, ##__VA_ARGS__);		\
		if (g_update_info)				\
			g_update_info(g_info, g_private);	\
	} while (0)

#define UPDATE_LEN			1024

#define ILITEK_FW_FILE_SIZE		(512 * 1024)
#define ILITEK_FW_BUF_SIZE		(256 * 1024)

PACKED(struct mapping_info_lego {
	uint8_t mcu_ver[4];
	uint8_t tuning_ver[4];
	uint8_t fw_ver[4];
	uint8_t core_test;
	uint8_t core_day;
	uint8_t core_month;
	uint8_t core_year;
	uint32_t core_ver;
	uint8_t vendor_ver[6];
	uint8_t _reserve_1[8];
	uint16_t customer_id;
	uint16_t fwid;
	uint16_t i2c_addr;
	uint8_t _reserve_2[2];
	char model_name[16];
	uint8_t _reserve_3[2];
	uint8_t ic_num;
	uint8_t total_tuning_num;
	uint16_t sizeof_tuning;
	uint16_t sizeof_tp_param;
	uint16_t sizeof_sys_info;
	uint16_t sizeof_sys_algo;
	uint16_t sizeof_key_info;
	uint8_t block_num;
	uint8_t support_tuning_num;
	uint8_t _reserve_4[2];

	PACKED(struct {
		uint8_t addr[3];
	}) blocks[10];

	uint8_t _reserve_5[9];
	uint8_t end_addr[3];
	uint8_t _reserve_6[2];
});

PACKED(union mapping_info {
	PACKED(struct {
		uint8_t mapping_ver[3];
		uint8_t protocol_ver[3];
		uint16_t ic_name;

		struct mapping_info_lego _lego;
	});
});

/* V3 231x 251x */
struct ilitek_section {
	uint32_t start;
	uint32_t end;
	uint32_t checksum;
};

/* V6 Lego series */
struct ilitek_block {
	uint32_t start;
	uint32_t end;
	uint16_t crc;

	bool crc_match;

	uint32_t offset;
};

/* return file size in # of bytes, or negative error code */
typedef int (*read_fw_t)(char *, unsigned char *, int, void *);
/* update progress of fw updating */
typedef void (*update_progress_t)(unsigned char, void *);
/* update fw verify info. and other decode status */
typedef void (*update_info_t)(char *, void *);

struct ilitek_update_callback {
	read_fw_t read_fw;
	update_progress_t update_progress;
	update_info_t update_info;
};

struct ilitek_fw_settings {
	int8_t retry;
	bool fw_check_only;
	bool force_update;

	bool fw_ver_check;
	uint8_t fw_ver[8];
};

struct ilitek_fw_handle {
	struct ilitek_ts_device *dev;
	void *_private;

	/* upgrade options */
	struct ilitek_fw_settings setting;

	/* common variable */
	char fw_name[512];
	uint8_t *fw_buf;
	uint16_t file_ic_name;

	/* V3 231x 251x */
	struct ilitek_section ap;
	struct ilitek_section df;

	/* V6 Lego series */
	uint8_t file_fw_ver[8];
	uint8_t block_num;
	struct ilitek_block blocks[ILTIEK_MAX_BLOCK_NUM];

	/* M3 + M2V */
	bool m2v;
	bool m2v_need_update;
	uint8_t *m2v_buf;
	uint32_t m2v_checksum;
	uint8_t m2v_fw_ver[8];

	/* upgrade status */
	unsigned int progress_curr;
	unsigned int progress_max;
	uint8_t progress;

	/* callbacks */
	struct ilitek_update_callback cb;
};

#ifdef __cplusplus
extern "C" {
#endif

void __DLL *ilitek_update_init(void *_dev,
			       struct ilitek_update_callback *callback,
			       void *_private);

void __DLL ilitek_update_exit(void *handle);

void __DLL ilitek_update_setting(void *handle,
				 struct ilitek_fw_settings *setting);

int __DLL ilitek_update_load_fw(void *handle, char *fw_name);

int __DLL ilitek_update_start(void *handle);

#ifdef __cplusplus
}
#endif


#endif

