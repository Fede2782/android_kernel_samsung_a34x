/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of ILITEK CommonFlow
 *
 * Copyright (c) 2022 ILI Technology Corp.
 * Copyright (c) 2022 Luca Hsu <luca_hsu@ilitek.com>
 * Copyright (c) 2022 Joe Hung <joe_hung@ilitek.com>
 */

#ifndef __ILITEK_PROTOCOL_H__
#define __ILITEK_PROTOCOL_H__

/*
 * [20221214] Linux_I2CDriver_ITS_V5_9_1_0
 * PROTOCOL_CODE_VERSION	0x00000003
 */
#define PROTOCOL_CODE_VERSION	0x00000003

#ifdef ILITEK_KERNEL_DRIVER
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#define MALLOC(size)		kmalloc(size, GFP_KERNEL)
#define CALLOC(num, size)	vmalloc(num * size)
#define FREE(ptr)		\
	do {			\
		kfree((ptr));	\
		(ptr) = NULL;	\
	} while (0)
#define CFREE(ptr)		\
	do {			\
		vfree((ptr));	\
		(ptr) = NULL;	\
	} while (0)
#define TP_DIV_ROUND_UP(a, b)	DIV_ROUND_UP(a, b)

#define TP_PRINT(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define TP_ERR_ARR(fmt, len, buf) \
	tp_log_arr(ILITEK_LOG_ERR, "[ILITEK][ERR] " fmt, len, buf)
#define TP_MSG_ARR(fmt, len, buf) \
	tp_log_arr(ILITEK_LOG_MSG, "[ILITEK][MSG] " fmt, len, buf)
#define TP_DBG_ARR(fmt, len, buf) \
	tp_log_arr(ILITEK_LOG_DBG, "[ILITEK][DBG] " fmt, len, buf)

#define tp_log_arr(level, str, len, buf)				\
	do {								\
		if ((level) > tp_log_level || !(buf))			\
			break;						\
		TP_PRINT(str" %*phD, len: %d\n", (len), (buf), (len));	\
	} while (0)

#else /* ILITEK_KERNEL_DRIVER */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MALLOC(size)		malloc(size)
#define CALLOC(num, size)	calloc(num, size)
#define FREE(ptr)		\
	do {			\
		free((ptr));	\
		(ptr) = NULL;	\
	} while (0)
#define CFREE(ptr)		FREE(ptr)
#define TP_DIV_ROUND_UP(a, b)	(((a) + (b) - 1) / (b))

#define TP_PRINT(fmt, ...)			\
	do {					\
		printf(fmt, ##__VA_ARGS__);	\
		fflush(stdout);			\
	} while (0)
#define TP_ERR_ARR(fmt, len, buf) \
	tp_log_arr(ILITEK_LOG_ERR, "[ILITEK][ERR] " fmt, len, buf)
#define TP_MSG_ARR(fmt, len, buf) \
	tp_log_arr(ILITEK_LOG_MSG, "[ILITEK][MSG] " fmt, len, buf)
#define TP_DBG_ARR(fmt, len, buf) \
	tp_log_arr(ILITEK_LOG_DBG, "[ILITEK][DBG] " fmt, len, buf)

#define tp_log_arr(level, str, len, buf)		\
	do {						\
		int i;					\
							\
		if ((level) > tp_log_level ||		\
		    (buf) == NULL)			\
			break;				\
		TP_PRINT(str);				\
		TP_PRINT(" %02x", (buf)[0]);		\
		for (i = 1; i < (int)(len); i++)	\
			TP_PRINT("-%02x", (buf)[i]);	\
		TP_PRINT(", len: %d\n", (int)(len));	\
		fflush(stdout);				\
	} while (0)

#endif /* ILITEK_KERNEL_DRIVER */

#ifdef _WIN32
#define PACKED(_declare_)	\
	__pragma(pack(push, 1)) _declare_ __pragma(pack(pop))
#define _sprintf(str, fmt, ...)	\
	sprintf_s((str), sizeof((str)), (fmt), ##__VA_ARGS__)
#define _strcpy(dst, src, dst_size)	strcpy_s((dst), (dst_size), (src))
#define _strcasecmp(l, r)		_stricmp((l), (r))
#define _strcat(dst, src)		strcat_s((dst), sizeof((dst)), (src))
#else
#define PACKED(_declare_)	\
	_declare_ __attribute__((__packed__))
#define _sprintf(str, fmt, ...)		sprintf((str), (fmt), ##__VA_ARGS__)
#define _strcpy(dst, src, dst_size)	strcpy((dst), (src));
#define _strcasecmp(l, r)		strcasecmp((l), (r))
#define _strcat(dst, src)		strcat((dst), (src))
#endif /* _WIN32 */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)			((sizeof(a) / sizeof(*(a))))
#endif

#define ILITEK_LOG_ERR		0
#define ILITEK_LOG_MSG		1
#define ILITEK_LOG_DBG		2

#define TP_ERR(fmt, ...)	\
	tp_log(ILITEK_LOG_ERR, "[ILITEK][ERR] " fmt, ##__VA_ARGS__)
#define TP_MSG(fmt, ...)	\
	tp_log(ILITEK_LOG_MSG, "[ILITEK][MSG] " fmt, ##__VA_ARGS__)
#define TP_DBG(fmt, ...)	\
	tp_log(ILITEK_LOG_DBG, "[ILITEK][DBG] " fmt, ##__VA_ARGS__)

extern int tp_log_level;
extern char g_str[4096];

#define tp_log(level, fmt, ...) 			\
	do {						\
		if (level > tp_log_level)		\
			break;				\
		TP_PRINT(fmt, ##__VA_ARGS__);		\
		_sprintf(g_str, fmt, ##__VA_ARGS__);	\
		if (g_msg)				\
			g_msg(g_str);			\
	} while (0)

#ifndef UNUSED
#define UNUSED(x)	(void)(x)
#endif

#ifndef MIN
#define MIN(l, r)				(((l) > (r)) ? (r) : (l))
#endif

#ifndef MAX
#define MAX(l, r)				(((l) > (r)) ? (l) : (r))
#endif

#define START_ADDR_LEGO			0x3000
#define START_ADDR_29XX			0x4000
#define END_ADDR_LEGO			0x40000

#define MM_ADDR_LEGO			0x3020
#define MM_ADDR_29XX			0x4020

#define DF_START_ADDR_LEGO		0x3C000
#define DF_START_ADDR_29XX		0x2C000

#define SLAVE_SET_AP			0xC1
#define SLAVE_SET_BL			0xC2
#define SLAVE_WRITE_AP			0xC3
#define M2V_GET_MOD			0xC0
#define M2V_WRITE_ENABLE		0xC4
#define M2V_GET_CHECKSUM		0xC7
#define M2V_GET_FW_VER			0x40


#define ILITEK_TP_SYSTEM_READY				0x50
#define ILITEK_TP_NO_NEED				0
#define ILITEK_TP_SYSTEM_BUSY				0x1
#define ILITEK_TP_INITIAL_BUSY				(0x1 << 1)

#define CRC_CALCULATE			0
#define CRC_GET				1

#define ILTIEK_MAX_BLOCK_NUM			10

#define PTL_V3					0x01
#define PTL_V6					0x02

#define BL_PROTOCOL_V1_8				0x10800
#define BL_PROTOCOL_V1_7				0x10700
#define BL_PROTOCOL_V1_6				0x10600

#define STYLUS_MODES			\
	X(STYLUS_WGP,	0x1,	"WGP")	\
	X(STYLUS_USI,	0x2,	"USI")	\
	X(STYLUS_MPP,	0x4,	"MPP")

#define ILITEK_CMD_MAP							\
	X(0x20, PTL_V3|PTL_V6, GET_TP_INFO, api_protocol_get_tp_info)	\
	X(0x21, PTL_V3|PTL_V6, GET_SCRN_RES, api_protocol_get_scrn_res)	\
	X(0x22, PTL_V3|PTL_V6, GET_KEY_INFO, api_protocol_get_key_info)	\
	X(0x30, PTL_V3|PTL_V6, SET_IC_SLEEP, api_protocol_set_sleep)	\
	X(0x31, PTL_V3|PTL_V6, SET_IC_WAKE, api_protocol_set_wakeup)	\
	X(0x34, PTL_V3|PTL_V6, SET_MCU_IDLE, api_protocol_set_idle)	\
	X(0x40, PTL_V3|PTL_V6, GET_FW_VER, api_protocol_get_fw_ver)	\
	X(0x42, PTL_V3|PTL_V6, GET_PTL_VER, api_protocol_get_ptl_ver)	\
	X(0x43, PTL_V3|PTL_V6, GET_CORE_VER, api_protocol_get_core_ver)	\
	X(0x60, PTL_V3|PTL_V6, SET_SW_RST, api_protocol_set_sw_reset)	\
	X(0x61, PTL_V3|PTL_V6, GET_MCU_VER, api_protocol_get_mcu_ver)	\
	X(0x68, PTL_V3|PTL_V6, SET_FUNC_MOD, api_protocol_set_func_mode)\
	X(0x80, PTL_V3|PTL_V6, GET_SYS_BUSY, api_protocol_get_sys_busy)	\
	X(0xC0, PTL_V3|PTL_V6, GET_MCU_MOD, api_protocol_get_mcu_mode)	\
	X(0xC1, PTL_V3|PTL_V6, SET_AP_MODE, api_protocol_set_ap_mode)	\
	X(0xC2, PTL_V3|PTL_V6, SET_BL_MODE, api_protocol_set_bl_mode)	\
	X(0xC7, PTL_V3|PTL_V6, GET_AP_CRC, api_protocol_get_ap_crc)	\
									\
	/* v3 only cmds */						\
	X(0x63, PTL_V3, TUNING_PARA_V3, api_protocol_tuning_para_v3)	\
	X(0xC3, PTL_V3, WRITE_DATA_V3, api_protocol_write_data_v3)	\
	X(0xC4, PTL_V3, WRITE_ENABLE, api_protocol_write_enable)	\
	X(0xCA, PTL_V3, GET_DF_CRC, api_protocol_get_df_crc)		\
	X(0xF2, PTL_V3, SET_TEST_MOD, api_protocol_set_mode_v3)		\
									\
	/* v6 only cmds */						\
	X(0x45, PTL_V6, GET_PRODUCT_INFO, api_protocol_get_product_info)\
	X(0x46, PTL_V6, GET_FWID, api_protocol_get_fwid)		\
	X(0x62, PTL_V6, GET_MCU_INFO, api_protocol_get_mcu_info)	\
	X(0x6B, PTL_V6, C_MODEL_INFO, api_protocol_c_model_info)	\
	X(0xB0, PTL_V6, WRITE_DATA_M2V, api_protocol_write_data_m2v)	\
	X(0xC3, PTL_V6, WRITE_DATA_V6, api_protocol_write_data_v6)	\
	X(0xC9, PTL_V6, SET_DATA_LEN, api_protocol_set_data_len)	\
	X(0xCB, PTL_V6, ACCESS_SLAVE, api_protocol_access_slave)	\
	X(0xCC, PTL_V6, SET_FLASH_EN, api_protocol_set_flash_enable)	\
	X(0xCD, PTL_V6, GET_BLK_CRC, api_protocol_get_block_crc_by_addr)\
	X(0xF0, PTL_V6, SET_MOD_CTRL, api_protocol_set_mode_v6)


#define X(_cmd, _protocol, _cmd_id, _api)	_cmd_id,
enum ilitek_cmd_ids {
	ILITEK_CMD_MAP
	/* ALWAYS keep at the end */
	MAX_CMD_CNT
};
#undef X

#define X(_cmd, _protocol, _cmd_id, _api)	CMD_##_cmd_id = _cmd,
enum ilitek_cmds { ILITEK_CMD_MAP };
#undef X

enum ilitek_interfaces {
	interface_i2c = 0,
	interface_hid_over_i2c,
	interface_usb,
};

enum ilitek_modes {
	mode_normal = 0,
	mode_test,
	mode_debug,
	mode_suspend,
};

#define ILITEK_TOUCH_REPORT_FORMAT 	\
	X(touch_fmt_0, 0, 5, 10)	\
	X(touch_fmt_1, 1, 6, 10)	\
	X(touch_fmt_2, 2, 10, 5)	\
	X(touch_fmt_3, 3, 10, 5)

#define X(_enum, _id, _size, _cnt)	_enum = _id,
enum ilitek_touch_fmts { ILITEK_TOUCH_REPORT_FORMAT };
#undef X

PACKED(struct touch_fmt {
	uint8_t id:6;
	uint8_t status:1;
	uint8_t reserve:1;
	uint16_t x;
	uint16_t y;
	uint8_t p;
	uint16_t w;
	uint16_t h;
});

struct ilitek_report_info {
	unsigned int size;
	unsigned int max_cnt;
};

PACKED(struct pen_fmt {
	PACKED(union {
		uint8_t modes;
		PACKED(struct {
			uint8_t tip_sw:1;
			uint8_t barrel_sw:1;
			uint8_t eraser:1;
			uint8_t invert:1;
			uint8_t in_range:1;
			uint8_t reserve:3;
		});
	});
	uint16_t x;
	uint16_t y;
	uint16_t pressure;
	int16_t x_tilt;
	int16_t y_tilt;
});

PACKED(struct ilitek_screen_info {
	uint16_t x_min;
	uint16_t y_min;
	uint16_t x_max;
	uint16_t y_max;
	uint16_t pressure_min;
	uint16_t pressure_max;
	int16_t x_tilt_min;
	int16_t x_tilt_max;
	int16_t y_tilt_min;
	int16_t y_tilt_max;
	uint16_t pen_x_min;
	uint16_t pen_y_min;
	uint16_t pen_x_max;
	uint16_t pen_y_max;
});

PACKED(struct ilitek_tp_info_v6 {
	uint16_t x_resolution;
	uint16_t y_resolution;
	uint16_t x_ch;
	uint16_t y_ch;
	uint8_t max_fingers;
	uint8_t key_num;
	uint8_t ic_num;
	uint8_t support_modes;
	uint8_t format;
	uint8_t die_num;
	uint8_t block_num;
	uint8_t pen_modes;
	uint8_t pen_format;
	uint16_t pen_x_resolution;
	uint16_t pen_y_resolution;
});

PACKED(struct ilitek_tp_info_v3 {
	uint16_t x_resolution;
	uint16_t y_resolution;
	uint8_t x_ch;
	uint8_t y_ch;
	uint8_t max_fingers;
	uint8_t reserve;
	uint8_t key_num;
	uint8_t reserve_1[5];
	uint8_t support_modes;
});

PACKED(struct ilitek_key_info_v6 {
	uint8_t mode;
	uint16_t x_len;
	uint16_t y_len;

	PACKED(struct{
		uint8_t id;
		uint16_t x;
		uint16_t y;
	}) keys[50];
});

PACKED(struct ilitek_key_info_v3 {
	uint8_t x_len[2];
	uint8_t y_len[2];

	PACKED(struct{
		uint8_t id;
		uint8_t x[2];
		uint8_t y[2];
	}) keys[20];
});

struct ilitek_ts_kernel_ver {
	PACKED(struct {
		uint16_t ic_name;
		uint8_t df_start_addr[3];
		uint8_t df_size;

		char module_name[26];
	}) parser;

	uint16_t ic_name;
	uint32_t df_start_addr;
	char module_name[27];
};

struct ilitek_ts_kernel_info {
	PACKED(struct {
		char product_id[7];
		uint8_t mm_addr[3];
		char module_name[18];
		uint8_t reserve[4];
	}) parser;

	char product_id[8];
	uint32_t mm_addr;
	uint32_t min_addr;
	uint32_t max_addr;
	char module_name[19];
};

struct ilitek_key_info {
	struct ilitek_key_info_v6 info;
	bool clicked[50];
};

struct ilitek_ts_protocol {
	uint32_t ver;
	uint8_t flag;
};

struct ilitek_ts_ic {
	uint8_t mode;
	uint16_t crc[ILTIEK_MAX_BLOCK_NUM];

	char mode_str[32];
};

struct ilitek_slave_access {
	uint8_t slave_id;
	uint8_t func;
	void *data;
};

typedef int(*write_then_read_t)(unsigned char *, int, unsigned char *, int,
				void *);
typedef int(*read_interrupt_in_t)(unsigned char *, int, unsigned int, void *);
typedef void(*init_ack_t)(void *);
typedef int(*wait_ack_t)(uint8_t, unsigned int, void *);
typedef int(*hw_reset_t)(unsigned int, void *);
typedef int(*re_enum_t)(void *);
typedef void(*delay_ms_t)(unsigned int);
typedef void(*msg_t)(char *);

struct ilitek_ts_callback {
	write_then_read_t write_then_read;
	read_interrupt_in_t read_interrupt_in;
	init_ack_t init_ack;
	wait_ack_t wait_ack;
	hw_reset_t hw_reset;
	re_enum_t re_enum;
	delay_ms_t delay_ms;
	msg_t msg;
};

extern msg_t g_msg;

struct ilitek_ts_device {
	void *_private;

	uint8_t _interface;
	struct ilitek_ts_protocol protocol;
	unsigned int reset_time;

	uint8_t fw_ver[8];
	uint8_t core_ver[8];

	uint32_t ap_crc_v3;
	uint32_t df_crc_v3;

	struct ilitek_ts_ic ic[32];
	struct ilitek_screen_info screen_info;
	struct ilitek_tp_info_v6 tp_info;
	struct ilitek_key_info key;
	struct ilitek_ts_kernel_ver mcu;
	struct ilitek_ts_kernel_info kernel_info;

	uint8_t func_mode;

	char pen_mode[64];

	struct ilitek_report_info finger;

	uint8_t wbuf[4096];
	uint8_t rbuf[4096];
	struct ilitek_ts_callback cb;
};

#ifdef _WIN32
#define __DLL __declspec(dllexport)
#else
#define __DLL
#endif

#ifdef __cplusplus
extern "C" {
#endif

void __DLL __ilitek_get_ts_info(void *handle,
				struct ilitek_tp_info_v6 *tp_info);

void __DLL *ilitek_dev_init(uint8_t _interface,
			    struct ilitek_ts_callback *callback,
			    void *_private);
void __DLL ilitek_dev_exit(void *handle);

int __DLL api_update_ts_info(void *handle);

int __DLL api_protocol_set_cmd(struct ilitek_ts_device *dev, uint8_t idx,
			       void *data);
int __DLL api_set_ctrl_mode(struct ilitek_ts_device *dev, uint8_t mode,
			    bool eng);

uint16_t __DLL api_get_block_crc_by_addr(struct ilitek_ts_device *dev,
					 uint8_t type, uint32_t start,
					 uint32_t end);
int __DLL api_set_data_len(struct ilitek_ts_device *dev, uint16_t data_len);
int __DLL api_write_enable_v6(struct ilitek_ts_device *dev, bool in_ap,
			      bool is_slave, uint32_t start, uint32_t end);
int __DLL api_write_data_v6(struct ilitek_ts_device *dev, int wlen);
int __DLL api_access_slave(struct ilitek_ts_device *dev, uint8_t id,
			   uint8_t func, void *data);
int __DLL api_check_busy(struct ilitek_ts_device *dev, int timeout_ms,
			 int delay_ms);
int __DLL api_write_enable_v3(struct ilitek_ts_device *dev, bool in_ap,
			      bool write_ap, uint32_t end, uint32_t checksum);
int __DLL api_write_data_v3(struct ilitek_ts_device *dev);

uint16_t __DLL le16(const uint8_t *p);
uint16_t __DLL be16(const uint8_t *p);
uint32_t __DLL le32(const uint8_t *p, int bytes);
uint32_t __DLL be32(const uint8_t *p, int bytes);

int __DLL api_to_bl_mode(struct ilitek_ts_device *dev, bool bl, uint32_t start,
			 uint32_t end);

int __DLL api_write_data_m2v(struct ilitek_ts_device *dev, int wlen);
int __DLL api_to_bl_mode_m2v(struct ilitek_ts_device *dev, bool to_bl);

uint16_t __DLL get_crc(uint32_t start, uint32_t end, uint8_t *buf,
		       uint32_t buf_size);

uint32_t __DLL  get_checksum(uint32_t start, uint32_t end, uint8_t *buf,
			     uint32_t buf_size);

int __DLL reset_helper(struct ilitek_ts_device *dev);

int __DLL api_set_idle(struct ilitek_ts_device *dev, bool enable);
int __DLL api_set_func_mode(struct ilitek_ts_device *dev, uint8_t mode);
int __DLL api_get_func_mode(struct ilitek_ts_device *dev);

int __DLL api_erase_data_v3(struct ilitek_ts_device *dev);

#ifdef __cplusplus
}
#endif

#endif
