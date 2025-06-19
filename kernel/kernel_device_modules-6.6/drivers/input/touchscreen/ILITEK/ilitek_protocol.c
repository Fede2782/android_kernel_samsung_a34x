// SPDX-License-Identifier: GPL-2.0
/*
 * This file is part of ILITEK CommonFlow
 *
 * Copyright (c) 2022 ILI Technology Corp.
 * Copyright (c) 2022 Luca Hsu <luca_hsu@ilitek.com>
 * Copyright (c) 2022 Joe Hung <joe_hung@ilitek.com>
 */
#include "ilitek_protocol.h"

/*TODO: should be adjustable by public APIs*/
int tp_log_level = ILITEK_LOG_MSG;

char g_str[4096];
msg_t g_msg;

typedef int (*protocol_func_t)(struct ilitek_ts_device *, void *);

struct protocol_map {
	uint8_t cmd;
	uint8_t flag;
	protocol_func_t func;
	const char *desc;
};

#define X(_cmd, _protocol, _cmd_id, _api) \
	static int _api(struct ilitek_ts_device *, void *);
ILITEK_CMD_MAP
#undef X

#define X(_cmd, _protocol, _cmd_id, _api) {_cmd, _protocol, _api, #_cmd_id},
struct protocol_map protocol_maps[] = { ILITEK_CMD_MAP };
#undef X

uint16_t le16(const uint8_t *p)
{
	return p[0] | p[1] << 8;
}

uint16_t be16(const uint8_t *p)
{
	return p[1] | p[0] << 8;
}

uint32_t le32(const uint8_t *p, int bytes)
{
	uint32_t val = 0;

	while (bytes--)
		val += (p[bytes] << (8 * bytes));

	return val;
}

uint32_t be32(const uint8_t *p, int bytes)
{
	uint32_t val = 0;

	while (bytes--)
		val = (val << 8) | (*p++);

	return val;
}

static uint16_t update_crc(uint16_t crc, uint8_t newbyte)
{
	char i;
	const uint16_t crc_poly = 0x8408;

	crc ^= newbyte;

	for (i = 0; i < 8; i++) {
		if (crc & 0x01)
			crc = (crc >> 1) ^ crc_poly;
		else
			crc = crc >> 1;
	}

	return crc;
}

uint16_t get_crc(uint32_t start, uint32_t end,
		 uint8_t *buf, uint32_t buf_size)
{
	uint16_t crc = 0;
	uint32_t i;

	if (end > buf_size || start > buf_size) {
		TP_ERR("start/end addr: %#x/%#x buf size: %#x OOB\n",
			start, end, buf_size);
	}

	for (i = start; i < end && i < buf_size; i++)
		crc = update_crc(crc, buf[i]);

	return crc;
}

uint32_t get_checksum(uint32_t start, uint32_t end,
			uint8_t *buf, uint32_t buf_size)
{
	uint32_t sum = 0;
	uint32_t i;

	if (end > buf_size || start > buf_size) {
		TP_ERR("start/end addr: %#x/%#x buf size: %#x OOB\n",
			start, end, buf_size);
	}

	for (i = start; i < end && i < buf_size; i++)
		sum += buf[i];

	return sum;
}

int reset_helper(struct ilitek_ts_device *dev)
{
	if (dev->_interface == interface_i2c) {
		/* sw reset if no reset-gpio found */
		if (!dev->cb.hw_reset ||
			dev->cb.hw_reset(dev->reset_time, dev->_private) < 0)
			return api_protocol_set_cmd(dev, SET_SW_RST, NULL);
		return 0;
	}

	return api_protocol_set_cmd(dev, SET_SW_RST, NULL);
}

static int re_enum_helper(struct ilitek_ts_device *dev)
{
	int error;
	int retry = 5;

	if (!dev->cb.re_enum)
		return -EINVAL;

	do {
		error = dev->cb.re_enum(dev->_private);
		if (!error)
			return 0;

		TP_ERR("re-enum failed, error: %d, retry: %d\n", error, retry);
		dev->cb.delay_ms(500);
	} while (retry--);

	return -ENODEV;
}

static int write_then_wait_ack(struct ilitek_ts_device *dev,
				uint8_t *cmd, int wlen, int timeout_ms)
{
	int error;
	struct ilitek_ts_callback *cb = &dev->cb;

	if (cb->init_ack)
		cb->init_ack(dev->_private);

	error = cb->write_then_read(cmd, wlen, NULL, 0, dev->_private);
	if (error < 0)
		return error;

	/* cmd[0] should be ILITEK cmd code */
	error = cb->wait_ack(cmd[0], timeout_ms, dev->_private);
	if (error  < 0)
		TP_ERR("wait ack %d ms timeout, err: %d\n", timeout_ms, error);

	error = api_check_busy(dev, timeout_ms, 100);
	if (error < 0)
		return error;

	return 0;
}

/* Common APIs */
static int api_protocol_get_scrn_res(struct ilitek_ts_device *dev, void *data)
{
	int error;
	struct ilitek_screen_info *screen_info;

	UNUSED(data);

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 28, dev->_private);
	if (error < 0)
		return error;

	screen_info = (struct ilitek_screen_info *)dev->rbuf;

	dev->screen_info.x_min = screen_info->x_min;
	dev->screen_info.y_min = screen_info->y_min;
	dev->screen_info.x_max = screen_info->x_max;
	dev->screen_info.y_max = screen_info->y_max;

	TP_DBG("screen x: %hu~%hu, screen y: %hu~%hu\n",
		dev->screen_info.x_min, dev->screen_info.x_max,
		dev->screen_info.y_min, dev->screen_info.y_max);

	dev->screen_info.pressure_min = 0;
	dev->screen_info.pressure_max = 0;
	dev->screen_info.x_tilt_min = 0;
	dev->screen_info.x_tilt_max = 0;
	dev->screen_info.y_tilt_min = 0;
	dev->screen_info.y_tilt_max = 0;
	if (dev->protocol.ver > 0x60006) {
		dev->screen_info.pressure_min = screen_info->pressure_min;
		dev->screen_info.pressure_max = screen_info->pressure_max;
		dev->screen_info.x_tilt_min = screen_info->x_tilt_min;
		dev->screen_info.x_tilt_max = screen_info->x_tilt_max;
		dev->screen_info.y_tilt_min = screen_info->y_tilt_min;
		dev->screen_info.y_tilt_max = screen_info->y_tilt_max;

		dev->screen_info.pen_x_min = screen_info->pen_x_min;
		dev->screen_info.pen_y_min = screen_info->pen_y_min;
		dev->screen_info.pen_x_max = screen_info->pen_x_max;
		dev->screen_info.pen_y_max = screen_info->pen_y_max;
	}

	return 0;
}

static int api_protocol_get_tp_info_v3(struct ilitek_ts_device *dev, void *data)
{
	int error;
	struct ilitek_tp_info_v3 *tp_info;

	UNUSED(data);

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 15, dev->_private);
	if (error < 0)
		return error;

	tp_info = (struct ilitek_tp_info_v3 *)dev->rbuf;
	dev->tp_info.x_resolution = tp_info->x_resolution;
	dev->tp_info.y_resolution = tp_info->y_resolution;
	dev->tp_info.x_ch = tp_info->x_ch;
	dev->tp_info.y_ch = tp_info->y_ch;
	dev->tp_info.max_fingers = tp_info->max_fingers;
	dev->tp_info.key_num = tp_info->key_num;

	dev->tp_info.support_modes = tp_info->support_modes;
	if (dev->tp_info.support_modes > 3 || !dev->tp_info.support_modes)
		dev->tp_info.support_modes = 1;

	return 0;
}

static int api_protocol_get_tp_info_v6(struct ilitek_ts_device *dev, void *data)
{
	int error;
	struct ilitek_tp_info_v6 *tp_info;
	uint8_t i;

#define X(_enum, _code, _name) {_code, _name},
	const struct {
		const int code;
		const char *str;
	} pen_modes[] = { STYLUS_MODES };
#undef X

	UNUSED(data);

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 21, dev->_private);
	if (error < 0)
		return error;

	tp_info = (struct ilitek_tp_info_v6 *)dev->rbuf;
	dev->tp_info.x_resolution = tp_info->x_resolution;
	dev->tp_info.y_resolution = tp_info->y_resolution;
	dev->tp_info.x_ch = tp_info->x_ch;
	dev->tp_info.y_ch = tp_info->y_ch;
	dev->tp_info.max_fingers = tp_info->max_fingers;
	dev->tp_info.key_num = tp_info->key_num;
	dev->tp_info.ic_num = tp_info->ic_num;
	dev->tp_info.format = tp_info->format;
	dev->tp_info.support_modes = tp_info->support_modes;

	if (dev->protocol.ver > 0x60002) {
		dev->tp_info.block_num = tp_info->block_num;
		TP_MSG("[Panel Information] Block Number: %hhu\n",
			dev->tp_info.block_num);
	}

	if (dev->tp_info.ic_num > ARRAY_SIZE(dev->ic)) {
		TP_ERR("invalid ic_num: %hhu\n", dev->tp_info.ic_num);
		return -EINVAL;
	}
	TP_MSG("[Panel Information] Chip count: %u\n", dev->tp_info.ic_num);

	dev->tp_info.pen_modes = 0;
	memset(dev->pen_mode, 0, sizeof(dev->pen_mode));
	if (dev->protocol.ver > 0x60006) {
		dev->tp_info.pen_modes = tp_info->pen_modes;
		if (!dev->tp_info.pen_modes)
			_strcpy(dev->pen_mode, "Disable",
				sizeof(dev->pen_mode));
		for (i = 0; i < ARRAY_SIZE(pen_modes); i++) {
			if (!(tp_info->pen_modes & pen_modes[i].code))
				continue;
			_strcat(dev->pen_mode, pen_modes[i].str);
			_strcat(dev->pen_mode, ".");
		}

		TP_DBG("pen_modes: %hhu\n", dev->tp_info.pen_modes);
		TP_MSG("[Panel Information] Pen Mode: %s\n", dev->pen_mode);

		dev->tp_info.pen_format = tp_info->pen_format;
		dev->tp_info.pen_x_resolution = tp_info->pen_x_resolution;
		dev->tp_info.pen_y_resolution = tp_info->pen_y_resolution;
		TP_MSG("[Panel Information] Pen Format: 0x%hhx\n",
			dev->tp_info.pen_format);
		TP_MSG("[Panel Information] Pen X/Y resolution: %hu/%hu\n",
			dev->tp_info.pen_x_resolution,
			dev->tp_info.pen_y_resolution);
	}

	if (dev->tp_info.max_fingers > 40) {
		TP_ERR("invalid max tp: %d > 40\n",
			dev->tp_info.max_fingers);
		return -EINVAL;
	}

	return 0;
}

static int api_protocol_get_tp_info(struct ilitek_ts_device *dev, void *data)
{
	int error;

#define X(_enum, _id, _size, _cnt)	{_size, _cnt},
	const struct {
		const unsigned int size;
		const unsigned int max_cnt;
	} finger_fmts[] = { ILITEK_TOUCH_REPORT_FORMAT };
#undef X

	if (dev->protocol.flag == PTL_V3)
		error = api_protocol_get_tp_info_v3(dev, data);
	else if (dev->protocol.flag == PTL_V6)
		error = api_protocol_get_tp_info_v6(dev, data);
	else
		return -EINVAL;

	if (error < 0)
		return error;

	if (dev->tp_info.max_fingers > 40) {
		TP_ERR("invalid max fingers: %d > 40\n",
			dev->tp_info.max_fingers);
		return -EINVAL;
	}

	switch (dev->tp_info.format) {
	case touch_fmt_1:
	case touch_fmt_2:
	case touch_fmt_3:
		dev->finger.size = finger_fmts[dev->tp_info.format].size;
		dev->finger.max_cnt = finger_fmts[dev->tp_info.format].max_cnt;
		break;
	default:
	case touch_fmt_0:
		dev->finger.size = finger_fmts[touch_fmt_0].size;
		dev->finger.max_cnt = finger_fmts[touch_fmt_0].max_cnt;
		break;
	}

	TP_MSG("[Panel Information] X/Y resolution: %hu/%hu\n",
		dev->tp_info.x_resolution, dev->tp_info.y_resolution);
	TP_MSG("[Panel Information] X/Y channel: %hu/%hu\n",
		dev->tp_info.x_ch, dev->tp_info.y_ch);
	TP_MSG("[Panel Information] Support %hhu Fingers\n",
		dev->tp_info.max_fingers);
	TP_MSG("[Panel Information] Support %hhu Keys\n", dev->tp_info.key_num);

	TP_MSG("[Panel Information] Support %hhu modes\n",
		dev->tp_info.support_modes);

	TP_DBG("touch format: %hhu, size: %u bytes, max cnt: %u per packet\n",
		dev->tp_info.format, dev->finger.size, dev->finger.max_cnt);

#ifdef ILITEK_KERNEL_DRIVER
	/*
	 * Only ILITEK I2C driver need to get key info.
	 */
	error = api_protocol_set_cmd(dev, GET_KEY_INFO, NULL);
	if (dev->tp_info.key_num > 0 &&
		(error < 0))
		return error;
#endif

	return 0;
}

static int api_protocol_get_key_info_v3(struct ilitek_ts_device *dev,
					void *data)
{
	int error;
	struct ilitek_key_info_v3 *key_info;
	unsigned int i;

	UNUSED(data);

	/* Only i2c interface has key for V3 */
	if (dev->_interface != interface_i2c)
		return 0;

	if (dev->tp_info.key_num > 20) {
		TP_ERR("key count: %hhu invalid\n", dev->tp_info.key_num);
		return -EINVAL;
	}

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 29, dev->_private);
	if (error < 0)
		return error;

	for (i = 0; dev->tp_info.key_num > 5U &&
		i < TP_DIV_ROUND_UP(dev->tp_info.key_num, 5U) - 1U; i++) {
		TP_MSG("read keyinfo again, i: %u\n", i);
		error = dev->cb.write_then_read(NULL, 0, dev->rbuf + 29 + 5 * i, 25, dev->_private);
		if (error < 0)
			return error;
	}

	key_info = (struct ilitek_key_info_v3 *)dev->rbuf;
	dev->key.info.x_len = be16(key_info->x_len);
	dev->key.info.y_len = be16(key_info->y_len);
	TP_MSG("key_x_len: %hu, key_y_len: %hu\n",
		dev->key.info.x_len, dev->key.info.y_len);

	for (i = 0; i < dev->tp_info.key_num; i++) {
		dev->key.info.keys[i].id = key_info->keys[i].id;
		dev->key.info.keys[i].x = be16(key_info->keys[i].x);
		dev->key.info.keys[i].y = be16(key_info->keys[i].y);
		TP_MSG("key[%u] id: %hhu, x: %hu, y: %hu\n", i,
			dev->key.info.keys[i].id, dev->key.info.keys[i].x,
			dev->key.info.keys[i].y);
	}

	return 0;
}

static int api_protocol_get_key_info_v6(struct ilitek_ts_device *dev,
					void *data)
{
	int error;
	struct ilitek_key_info_v6 *key_info;
	unsigned int i;

	UNUSED(data);

	if (dev->tp_info.key_num > ARRAY_SIZE(dev->key.info.keys)) {
		TP_ERR("exception keycount %hhu > %d\n", dev->tp_info.key_num,
			(int)ARRAY_SIZE(dev->key.info.keys));
		return -EINVAL;
	}

	switch (dev->_interface) {
	case interface_i2c:
		error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf,
			5 + dev->tp_info.key_num * 5, dev->_private);
		if (error < 0)
			return error;
		key_info = (struct ilitek_key_info_v6 *)dev->rbuf;
		break;

	case interface_usb:
		error = dev->cb.write_then_read(dev->wbuf, 1,
			NULL, 0, dev->_private);
		if (error < 0 ||
			(dev->cb.write_then_read(NULL, 0,
				dev->rbuf, 256, dev->_private)) < 0)
			return error;
		key_info = (struct ilitek_key_info_v6 *)(dev->rbuf + 6);
		break;
	case interface_hid_over_i2c:
		error = dev->cb.write_then_read(dev->wbuf, 1,
				NULL, 0, dev->_private);
		if (error < 0 ||
			(dev->cb.write_then_read(NULL, 0,
				dev->rbuf, 256, dev->_private)) < 0)
			return error;
		key_info = (struct ilitek_key_info_v6 *)(dev->rbuf + 4);
		break;
	default:
		return -EINVAL;
	};

	dev->key.info.mode = key_info->mode;
	TP_MSG("[Panel Information] key mode: %hhu\n", dev->key.info.mode);

	dev->key.info.x_len = key_info->x_len;
	dev->key.info.y_len = key_info->y_len;
	TP_MSG("key_x_len: %hu, key_y_len: %hu\n",
		dev->key.info.x_len, dev->key.info.y_len);

	for (i = 0; i < dev->tp_info.key_num; i++) {
		dev->key.info.keys[i].id = key_info->keys[i].id;
		dev->key.info.keys[i].x = key_info->keys[i].x;
		dev->key.info.keys[i].y = key_info->keys[i].y;
		TP_MSG("key[%u] id: %hhu, x: %hu, y: %hu\n", i,
			dev->key.info.keys[i].id, dev->key.info.keys[i].x,
			dev->key.info.keys[i].y);
	}

	return 0;
}

static int api_protocol_get_key_info(struct ilitek_ts_device *dev, void *data)
{
	if (dev->protocol.flag == PTL_V3)
		return api_protocol_get_key_info_v3(dev, data);
	else if (dev->protocol.flag == PTL_V6)
		return api_protocol_get_key_info_v6(dev, data);

	return -EINVAL;
}

static int api_protocol_get_ptl_ver(struct ilitek_ts_device *dev, void *data)
{
	int error;

	UNUSED(data);

	dev->protocol.flag = PTL_V6;
	dev->reset_time = 1000;
	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 3,
						dev->_private);
	if (error < 0)
		return error;

	dev->protocol.ver = (dev->rbuf[0] << 16) + (dev->rbuf[1] << 8) +
				dev->rbuf[2];
	TP_MSG("[Protocol Version]: %x.%x.%x\n",
		(dev->protocol.ver >> 16) & 0xFF,
		(dev->protocol.ver >> 8) & 0xFF,
		dev->protocol.ver & 0xFF);

	if (((dev->protocol.ver >> 16) & 0xFF) == 0x3 ||
		(dev->protocol.ver & 0xFFFF00) == BL_PROTOCOL_V1_6 ||
		(dev->protocol.ver & 0xFFFF00) == BL_PROTOCOL_V1_7) {
		dev->reset_time = 200;
		dev->protocol.flag = PTL_V3;
	} else if (((dev->protocol.ver >> 16) & 0xFF) == 0x6 ||
		(dev->protocol.ver & 0xFFFF00) == BL_PROTOCOL_V1_8) {
		dev->reset_time = 600;
		dev->protocol.flag = PTL_V6;
	}

	return 0;
}

static int api_protocol_get_fw_ver(struct ilitek_ts_device *dev, void *data)
{
	int error;

	UNUSED(data);
	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 8,
						dev->_private);
	if (error < 0)
		return error;

	memcpy(dev->fw_ver, dev->rbuf, 8);

	if (dev->ic[0].mode == 0x55)
		TP_MSG_ARR("[BL Firmware Version]", 8, dev->fw_ver);
	else
		TP_MSG_ARR("[AP Firmware Version]", 8, dev->fw_ver);

	return 0;
}

static int api_protocol_get_mcu_mode(struct ilitek_ts_device *dev, void *data)
{
	int error;
	uint8_t i, ic_num = (data) ? *(uint8_t *)data : 1;

	if (ic_num > ARRAY_SIZE(dev->ic))
		return -EINVAL;

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf,
						2 * ic_num, dev->_private);
	if (error < 0)
		return error;

	for (i = 0; i < ic_num; i++) {
		dev->ic[i].mode = dev->rbuf[i * 2];

		if (dev->ic[i].mode == 0x5a)
			_sprintf(dev->ic[i].mode_str, "AP");
		else if (dev->ic[i].mode == 0x55)
			_sprintf(dev->ic[i].mode_str, "BL");
		else
			_sprintf(dev->ic[i].mode_str, "UNKNOWN");
	}

	TP_MSG("[Current Mode] Master: 0x%hhx %s\n", dev->ic[0].mode,
		dev->ic[0].mode_str);
	for (i = 1; i < ic_num; i++)
		TP_MSG("[Current Mode] Slave[%hhu]: 0x%hhx %s\n", i,
			dev->ic[i].mode, dev->ic[i].mode_str);

	return 0;
}

static int api_protocol_get_product_info(struct ilitek_ts_device *dev, void *data)
{
	int error;

	UNUSED(data);

	/*
	 * 0x45 cmd support after BL ver 1.8.3 and AP 6.0.7
	 * return 0 to skip error check
	 */
	if ((dev->ic[0].mode == 0x55 && dev->protocol.ver < 0x010803) ||
		(dev->ic[0].mode == 0x5a && dev->protocol.ver < 0x060007))
		return 0;

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 8,
						dev->_private);
	if (error < 0)
		return error;

	TP_MSG_ARR("[Production Info]", 8, dev->rbuf);

	return 0;
}

static int api_protocol_get_fwid(struct ilitek_ts_device *dev, void *data)
{
	int error;

	UNUSED(data);

	/*
	 * 0x46 cmd support after BL ver 1.8.2 and AP 6.0.7
	 * return 0 to skip error check
	 */
	if ((dev->ic[0].mode == 0x55 && dev->protocol.ver < 0x010802) ||
		(dev->ic[0].mode == 0x5a && dev->protocol.ver < 0x060007))
		return 0;

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 4,
						dev->_private);
	if (error < 0)
		return error;

	TP_MSG("[Customer ID] %#x\n", le16(dev->rbuf));
	TP_MSG("[FWID] %#x\n", le16(dev->rbuf + 2));

	return 0;
}

static int api_protocol_get_mcu_ver(struct ilitek_ts_device *dev, void *data)
{
	int error;
	unsigned int i;

	UNUSED(data);

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 32,
						dev->_private);
	if (error < 0)
		return error;

	memcpy(&dev->mcu.parser, dev->rbuf, sizeof(dev->mcu.parser));

	dev->mcu.ic_name = dev->mcu.parser.ic_name;
	TP_MSG("[Kernel Version] IC: 0x%04X\n", dev->mcu.ic_name);

	memset(dev->mcu.module_name, 0, sizeof(dev->mcu.module_name));
	memcpy(dev->mcu.module_name, dev->mcu.parser.module_name,
		sizeof(dev->mcu.parser.module_name));
	for (i = 0; i < sizeof(dev->mcu.module_name); i++) {
		if (dev->mcu.module_name[i] != 0xff)
			continue;
		dev->mcu.module_name[i] = 0;
	}

	TP_MSG("[Module Name]: [%s]\n", dev->mcu.module_name);


	dev->mcu.df_start_addr = 0;
	if ((dev->protocol.ver & 0xFFFF00) == BL_PROTOCOL_V1_7 ||
		(dev->protocol.ver & 0xFFFF00) == BL_PROTOCOL_V1_6) {
		dev->mcu.df_start_addr = be32(dev->mcu.parser.df_start_addr, 3);
	} else if (dev->protocol.flag == PTL_V6) {
		dev->mcu.df_start_addr = le32(dev->mcu.parser.df_start_addr, 3);

		/*
		 * for old protocol version, check df start addr.
		 * to determine it is 29xx series or Lego series.
		 */
		if ((dev->ic[0].mode == 0x55 && dev->protocol.ver < 0x010803) ||
			(dev->ic[0].mode == 0x5a && dev->protocol.ver < 0x060008)) {
			if (dev->mcu.df_start_addr == DF_START_ADDR_29XX) {
				dev->kernel_info.mm_addr = MM_ADDR_29XX;
				dev->kernel_info.min_addr = START_ADDR_29XX;
				dev->kernel_info.max_addr = END_ADDR_LEGO;
			} else {
				dev->kernel_info.mm_addr = MM_ADDR_LEGO;
				dev->kernel_info.min_addr = START_ADDR_LEGO;
				dev->kernel_info.max_addr = END_ADDR_LEGO;
			}
		}
	}

	TP_MSG("[Flash Start Addr.] %#x\n", dev->mcu.df_start_addr);

	return 0;
}

static int api_protocol_get_mcu_info(struct ilitek_ts_device *dev, void *data)
{
	int error;
	unsigned int i;

	UNUSED(data);

	/*
	 * 0x62 command support after BL ver 1.8.3 and AP 6.0.8,
	 * return 0 to skip error check
	 */
	if ((dev->ic[0].mode == 0x55 && dev->protocol.ver < 0x010803) ||
		(dev->ic[0].mode == 0x5a && dev->protocol.ver < 0x060008))
		return 0;

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 32,
						dev->_private);
	if (error < 0)
		return error;

	memcpy(&dev->kernel_info.parser, dev->rbuf,
		sizeof(dev->kernel_info.parser));

	memset(dev->kernel_info.product_id, 0,
		sizeof(dev->kernel_info.product_id));
	memset(dev->kernel_info.module_name, 0,
		sizeof(dev->kernel_info.module_name));

	memcpy(dev->kernel_info.product_id, dev->kernel_info.parser.product_id,
		sizeof(dev->kernel_info.parser.product_id));
	memcpy(dev->kernel_info.module_name,
		dev->kernel_info.parser.module_name,
		sizeof(dev->kernel_info.parser.module_name));
	dev->kernel_info.mm_addr = le32(dev->kernel_info.parser.mm_addr, 3);

	for (i = 0; i < sizeof(dev->kernel_info.module_name); i++) {
		if (dev->kernel_info.module_name[i] != 0xff)
			continue;
		dev->kernel_info.module_name[i] = 0;
	}

	TP_MSG("[Product ID] %s\n", dev->kernel_info.product_id);
	TP_MSG("[Memory Mapping Addr.] %#x\n", dev->kernel_info.mm_addr);
	TP_MSG("[MM. Module Name]: [%s]\n", dev->kernel_info.module_name);

	/*
	 * check memory mapping start addr.
	 * to determine it is 29xx series or Lego series.
	 */
	if (dev->kernel_info.mm_addr == MM_ADDR_29XX) {
		dev->kernel_info.min_addr = START_ADDR_29XX;
		dev->kernel_info.max_addr = END_ADDR_LEGO;
	} else {
		dev->kernel_info.min_addr = START_ADDR_LEGO;
		dev->kernel_info.max_addr = END_ADDR_LEGO;
	}

	return 0;
}


static int api_protocol_get_core_ver(struct ilitek_ts_device *dev, void *data)
{
	int error;

	UNUSED(data);

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 8,
						dev->_private);
	if (error < 0)
		return error;

	memcpy(dev->core_ver, dev->rbuf, 8);

	TP_MSG_ARR("[CoreVersion]", 4, dev->core_ver);

	return 0;
}

static int api_protocol_set_sw_reset(struct ilitek_ts_device *dev, void *data)
{
	int error;
	int wlen = 1;

	UNUSED(data);

	/* Do not software reset for I2C-HID interface */
	if (dev->_interface == interface_hid_over_i2c)
		return 0;

	dev->wbuf[1] = 0;
	if (dev->protocol.flag == PTL_V3)
		wlen = 2;

	error = dev->cb.write_then_read(dev->wbuf, wlen, dev->rbuf, 0,
						dev->_private);
	if (error < 0)
		return error;

	dev->cb.delay_ms(dev->reset_time);

	if (dev->_interface == interface_usb)
		return re_enum_helper(dev);

	return 0;
}

static int api_protocol_get_sys_busy(struct ilitek_ts_device *dev, void *data)
{
	int error;

	if (data)
		*(uint8_t *)data = 0;

	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 1,
						dev->_private);
	if (error < 0)
		return error;

	if (data)
		*(uint8_t *)data = dev->rbuf[0];

	return 0;
}

static int api_protocol_get_ap_crc_v6(struct ilitek_ts_device *dev, void *data)
{
	int error;
	uint8_t i, ic_num = (data) ? *(uint8_t *)data : 1;

	if (ic_num > ARRAY_SIZE(dev->ic))
		return -EINVAL;

	error = dev->cb.write_then_read(dev->wbuf, 1,
						dev->rbuf, 2 * ic_num,
						dev->_private);
	if (error < 0)
		return  error;

	dev->ic[0].crc[0] = le16(dev->rbuf);
	TP_MSG("[FW CRC] Master: 0x%hx\n", dev->ic[0].crc[0]);

	for (i = 1; i < ic_num; i++) {
		dev->ic[i].crc[0] = le16(dev->rbuf + 2 * i);
		TP_MSG("[FW CRC] Slave[%hhu]: 0x%hx\n", i, dev->ic[i].crc[0]);
	}

	return 0;
}

static int api_protocol_get_ap_crc_v3(struct ilitek_ts_device *dev, void *data)
{
	int error, rlen;

	UNUSED(data);

	rlen = 2;
	if (dev->mcu.ic_name == 0x2312 || dev->mcu.ic_name == 0x2315)
		rlen = 4;

	if (dev->_interface == interface_i2c) {
		error = dev->cb.write_then_read(dev->wbuf, 1, NULL, 0,
							dev->_private);
		if (error < 0)
			return  error;
		dev->cb.delay_ms(600);
		error = dev->cb.write_then_read(NULL, 0, dev->rbuf, rlen,
							dev->_private);
		if (error < 0)
			return error;
	} else {
		error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf,
							rlen, dev->_private);
		if (error < 0)
			return error;
	}

	dev->ap_crc_v3 = le16(dev->rbuf);
	if (dev->mcu.ic_name == 0x2312 || dev->mcu.ic_name == 0x2315)
		dev->ap_crc_v3 |= (le16(dev->rbuf + 2) << 16);

	TP_MSG("[Check Code] AP: %#x\n", dev->ap_crc_v3);

	return 0;
}


static int api_protocol_get_ap_crc(struct ilitek_ts_device *dev, void *data)
{

	if (dev->protocol.flag == PTL_V6)
		return api_protocol_get_ap_crc_v6(dev, data);
	else if (dev->protocol.flag == PTL_V3)
		return api_protocol_get_ap_crc_v3(dev, data);

	return -EINVAL;
}


static int api_protocol_set_mode_v3(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 2, NULL, 0, dev->_private);
}

static int api_protocol_write_enable(struct ilitek_ts_device *dev, void *data)
{
	int error;
	bool in_ap = (data) ? *(bool *)data : true;

	error = dev->cb.write_then_read(dev->wbuf, (in_ap) ? 3 : 10, NULL,
					0, dev->_private);
	if (error < 0)
		return error;

	dev->cb.delay_ms(200);

	return 0;
}

static int api_protocol_write_data_v3(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 33, NULL, 0, dev->_private);
}

static int api_protocol_get_df_crc(struct ilitek_ts_device *dev,  void *data)
{
	int error;

	UNUSED(data);

	dev->df_crc_v3 = 0;
	error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 4,
						dev->_private);
	if (error < 0)
		return error;

	dev->df_crc_v3 = le16(dev->rbuf + 2) << 16 | le16(dev->rbuf);
	TP_MSG("[Check Code] Data: %#x\n", dev->df_crc_v3);

	return 0;
}

static int api_protocol_set_mode_v6(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 3, NULL, 0, dev->_private);
}

static int api_protocol_get_block_crc_by_addr(struct ilitek_ts_device *dev,
						void *data)
{
	int error;
	uint8_t type = (data) ? *(uint8_t *)data : 0;

	dev->wbuf[1] = type;
	if (type == CRC_CALCULATE) {
		error = write_then_wait_ack(dev, dev->wbuf, 8, 5000);
		if (error < 0)
			return error;
		dev->wbuf[1] = CRC_GET;
	}

	return dev->cb.write_then_read(dev->wbuf, 2, dev->rbuf, 2,
					dev->_private);
}

static int api_protocol_set_data_len(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 3, NULL, 0, dev->_private);
}

static int api_protocol_set_flash_enable(struct ilitek_ts_device *dev,
					 void *data)
{
	int error;
	uint8_t type = (data) ? *(uint8_t *)data : 0;
	int wlen, rlen;
	bool in_ap = ((type & 0x1) != 0) ? true : false;
	bool is_slave = ((type & 0x2) != 0) ? true : false;

	uint32_t set_start, set_end, get_start, get_end;

	if (!is_slave) {
		wlen = (in_ap) ? 3 : 9;
		rlen = (in_ap || dev->protocol.ver < 0x010803) ? 0 : 6;

		set_start = le32(dev->wbuf + 3, 3);
		set_end = le32(dev->wbuf + 6, 3);

		error = dev->cb.write_then_read(dev->wbuf, wlen,
							dev->rbuf, rlen,
							dev->_private);
		if (error < 0)
			return error;

		if (in_ap || dev->protocol.ver < 0x010803)
			return 0;

		get_start = le32(dev->rbuf, 3);
		get_end = le32(dev->rbuf + 3, 3);

		if (set_start != get_start || set_end != get_end) {
			TP_ERR("start/end addr.: %#x/%#x vs. %#x/%#x not match\n",
				set_start, set_end, get_start, get_end);
			return -EINVAL;
		}

		return 0;
	}

	switch (dev->_interface) {
	case interface_i2c:
	case interface_hid_over_i2c:
		error = write_then_wait_ack(dev, dev->wbuf, 9, 20000);
		if (error < 0)
			return error;
		dev->cb.delay_ms(2000);
		break;

	case interface_usb:
		error = write_then_wait_ack(dev, dev->wbuf, 9, 20000);
		if (error < 0)
			return error;

		return re_enum_helper(dev);
	}

	return 0;
}

static int api_protocol_write_data_v6(struct ilitek_ts_device *dev, void *data)
{
	int wlen;

	if (!data)
		return -EINVAL;

	wlen = *(int *)data;

	return write_then_wait_ack(dev, dev->wbuf, wlen, 5000);
}

static int api_protocol_write_data_m2v(struct ilitek_ts_device *dev, void *data)
{
	int wlen;

	if (!data)
		return -EINVAL;

	wlen = *(int *)data;

	return write_then_wait_ack(dev, dev->wbuf, wlen, 30000);
}

static int api_protocol_access_slave(struct ilitek_ts_device *dev, void *data)
{
	int error;
	struct ilitek_slave_access *access;

	if (!data)
		return -EINVAL;

	access = (struct ilitek_slave_access *)data;

	dev->wbuf[1] = access->slave_id;
	dev->wbuf[2] = access->func;
	memset(dev->rbuf, 0, sizeof(dev->rbuf));

	switch (access->func) {
	case M2V_GET_CHECKSUM:
		error = dev->cb.write_then_read(dev->wbuf, 3, dev->rbuf, 3,
						dev->_private);

		*((uint32_t *)access->data) = (le16(dev->rbuf + 1) << 8) +
			dev->rbuf[0];

		break;

	case M2V_GET_MOD:
		error = dev->cb.write_then_read(dev->wbuf, 3, dev->rbuf, 1,
						dev->_private);

		*((uint8_t *)access->data) = dev->rbuf[0];

		break;

	case M2V_GET_FW_VER:
		error = dev->cb.write_then_read(dev->wbuf, 3, dev->rbuf, 8,
						dev->_private);

		memcpy((uint8_t *)access->data, dev->rbuf, 8);

		break;

	case M2V_WRITE_ENABLE:
		dev->wbuf[3] = ((uint8_t *)access->data)[0];
		dev->wbuf[4] = ((uint8_t *)access->data)[1];
		dev->wbuf[5] = ((uint8_t *)access->data)[2];
		dev->wbuf[6] = ((uint8_t *)access->data)[3];
		dev->wbuf[7] = ((uint8_t *)access->data)[4];
		dev->wbuf[8] = ((uint8_t *)access->data)[5];

		error = dev->cb.write_then_read(dev->wbuf, 9, NULL, 0,
						dev->_private);
		break;

	default:
		error = write_then_wait_ack(dev, dev->wbuf, 3, 5000);
		break;
	};

	return error;
}

static int api_protocol_set_ap_mode(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 1, NULL, 0, dev->_private);
}

static int api_protocol_set_bl_mode(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 1, NULL, 0, dev->_private);
}

static int api_protocol_set_idle(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 2, NULL, 0, dev->_private);
}

static int api_protocol_set_sleep(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 1, NULL, 0, dev->_private);
}

static int api_protocol_set_wakeup(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 1, NULL, 0, dev->_private);
}

static int api_protocol_set_func_mode(struct ilitek_ts_device *dev, void *data)
{
	int error;
	bool get = (data) ? *(bool *)data : true;

	if (!data)
		return -EINVAL;

	if (get) {
		error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 3,
							dev->_private);
		if (error < 0)
			return error;

		dev->func_mode = dev->rbuf[2];
		/*TP_MSG("[FW Mode] 0x%hhu\n", dev->func_mode);*/

		return 0;
	}

	if (dev->protocol.flag == PTL_V3) {
		error = dev->cb.write_then_read(dev->wbuf, 4, NULL, 0,
							dev->_private);
		if (error < 0 ||
			api_check_busy(dev, 1000, 100) < 0)
			return error;
		return 0;
	} else if (dev->protocol.flag == PTL_V6) {
		error = write_then_wait_ack(dev, dev->wbuf, 4, 1000);
		if (error < 0)
			return error;
		return 0;
	}

	return -EINVAL;
}

static int api_protocol_c_model_info(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 12, NULL, 0, dev->_private);
}

static int api_protocol_tuning_para_v3(struct ilitek_ts_device *dev, void *data)
{
	UNUSED(data);

	return dev->cb.write_then_read(dev->wbuf, 2, NULL, 0, dev->_private);
}

int api_protocol_set_cmd(struct ilitek_ts_device *dev, uint8_t idx, void *data)
{
	int error;

	if (!dev || idx >= ARRAY_SIZE(protocol_maps))
		return -EINVAL;

	if (!(dev->protocol.flag & protocol_maps[idx].flag)) {
		TP_ERR("Unexpected cmd: %s for 0x%hhx only, now is 0x%hhx\n",
			protocol_maps[idx].desc, protocol_maps[idx].flag,
			dev->protocol.flag);
		return -EINVAL;
	}

	dev->wbuf[0] = protocol_maps[idx].cmd;
	error = protocol_maps[idx].func(dev, data);
	if (error < 0) {
		TP_ERR("failed to execute cmd: 0x%hhx %s, err: %d\n",
			protocol_maps[idx].cmd, protocol_maps[idx].desc, error);
		return error;
	}

	return 0;
}

int api_set_ctrl_mode(struct ilitek_ts_device *dev, uint8_t mode, bool eng)
{
	int error;
	uint8_t cmd = 0;

	memset(dev->wbuf, 0, 3);

	if (dev->protocol.flag == PTL_V3) {
		/* V3 only support suspend and normal mode */
		if (mode != mode_normal && mode != mode_suspend)
			return -EPROTONOSUPPORT;
		dev->wbuf[1] = (mode == mode_suspend) ? 0x01 : 0x00;
		cmd = SET_TEST_MOD;
	} else if (dev->protocol.flag == PTL_V6) {
		dev->wbuf[1] = mode;
		dev->wbuf[2] = (eng) ? 0x01 : 0x00;
		cmd = SET_MOD_CTRL;
	}

	error = api_protocol_set_cmd(dev, cmd, NULL);
	if (error < 0)
		return error;

	dev->cb.delay_ms(100);

	return 0;
}

uint16_t api_get_block_crc_by_addr(struct ilitek_ts_device *dev, uint8_t type,
					uint32_t start, uint32_t end)
{
	memset(dev->wbuf, 0, 64);

	dev->wbuf[2] = start;
	dev->wbuf[3] = (start >> 8) & 0xFF;
	dev->wbuf[4] = (start >> 16) & 0xFF;
	dev->wbuf[5] = end & 0xFF;
	dev->wbuf[6] = (end >> 8) & 0xFF;
	dev->wbuf[7] = (end >> 16) & 0xFF;
	if (api_protocol_set_cmd(dev, GET_BLK_CRC, &type) < 0)
		return 0;

	return le16(dev->rbuf);
}

int api_set_data_len(struct ilitek_ts_device *dev, uint16_t data_len)
{
	memset(dev->wbuf, 0, 64);

	dev->wbuf[1] = data_len & 0xFF;
	dev->wbuf[2] = (data_len >> 8) & 0xFF;

	return api_protocol_set_cmd(dev, SET_DATA_LEN, NULL);
}

int api_write_enable_v6(struct ilitek_ts_device *dev, bool in_ap, bool is_slave,
			uint32_t start, uint32_t end)
{
	uint8_t type;

	memset(dev->wbuf, 0, 64);
	dev->wbuf[1] = 0x5A;
	dev->wbuf[2] = 0xA5;
	dev->wbuf[3] = start & 0xFF;
	dev->wbuf[4] = (start >> 8) & 0xFF;
	dev->wbuf[5] = start >> 16;
	dev->wbuf[6] = end & 0xFF;
	dev->wbuf[7] = (end >> 8) & 0xFF;
	dev->wbuf[8] = end >> 16;

	type = (in_ap) ? 0x1 : 0x0;
	type |= (is_slave) ? 0x2 : 0x0;

	return api_protocol_set_cmd(dev, SET_FLASH_EN, &type);
}

int api_write_data_v6(struct ilitek_ts_device *dev, int wlen)
{
	return api_protocol_set_cmd(dev, WRITE_DATA_V6, &wlen);
}

int api_access_slave(struct ilitek_ts_device *dev, uint8_t id, uint8_t func,
			void *data)
{
	struct ilitek_slave_access access;

	access.slave_id = id;
	access.func = func;
	access.data = data;

	return api_protocol_set_cmd(dev, ACCESS_SLAVE, &access);
}

int api_write_enable_v3(struct ilitek_ts_device *dev, bool in_ap, bool write_ap,
			uint32_t end, uint32_t checksum)
{
	memset(dev->wbuf, 0, 64);
	dev->wbuf[1] = 0x5A;
	dev->wbuf[2] = 0xA5;
	dev->wbuf[3] = (write_ap) ? 0x0 : 0x1;
	dev->wbuf[4] = (end >> 16) & 0xFF;
	dev->wbuf[5] = (end >> 8) & 0xFF;
	dev->wbuf[6] = end & 0xFF;
	dev->wbuf[7] = (checksum >> 16) & 0xFF;
	dev->wbuf[8] = (checksum >> 8) & 0xFF;
	dev->wbuf[9] = checksum & 0xFF;

	return api_protocol_set_cmd(dev, WRITE_ENABLE, &in_ap);
}

int api_write_data_v3(struct ilitek_ts_device *dev)
{
	return api_protocol_set_cmd(dev, WRITE_DATA_V3, NULL);
}

int api_check_busy(struct ilitek_ts_device *dev, int timeout_ms, int delay_ms)
{
	uint8_t busy;

	memset(dev->wbuf, 0, 64);

	dev->cb.delay_ms(5);

	while (timeout_ms > 0) {
		api_protocol_set_cmd(dev, GET_SYS_BUSY, &busy);
		if (busy == ILITEK_TP_SYSTEM_READY)
			return 0;

		/* delay ms for each check busy */
		dev->cb.delay_ms(delay_ms);
		timeout_ms -= delay_ms;
	}

	return -ETIME;
}

int api_to_bl_mode(struct ilitek_ts_device *dev, bool to_bl,
			uint32_t start, uint32_t end)
{
	int cnt = 0, retry = 15;
	const uint8_t target_mode = (to_bl) ? 0x55 : 0x5A;

	do {
		if (api_protocol_set_cmd(dev, GET_MCU_MOD, NULL) < 0)
			continue;

		if (dev->ic[0].mode == target_mode)
			goto success_change_mode;

		if (to_bl) {
			if (dev->protocol.flag == PTL_V3 &&
				api_write_enable_v3(dev, true, false, 0, 0) < 0)
				continue;
			else if (dev->protocol.flag == PTL_V6 &&
				 api_write_enable_v6(dev, true, false,
							0, 0) < 0)
				continue;

			api_protocol_set_cmd(dev, SET_BL_MODE, NULL);

			/*
			 * Lego's old BL may trigger unexpected INT after 0xC2,
			 * so make I2C driver handle it ASAP.
			 */
			if (dev->protocol.flag == PTL_V6 &&
				dev->_interface == interface_i2c)
				dev->cb.init_ack(dev->_private);
		} else {
			if (dev->protocol.flag == PTL_V3 &&
				api_write_enable_v3(dev, true, false, 0, 0) < 0)
				continue;
			else if (dev->protocol.flag == PTL_V6 &&
				 api_write_enable_v6(dev, false, false,
							start, end) < 0)
				continue;

			api_protocol_set_cmd(dev, SET_AP_MODE, NULL);
		}

		switch (dev->_interface) {
		case interface_hid_over_i2c:
		case interface_i2c:
			dev->cb.delay_ms(1000 + 100 * cnt);
			break;
		case interface_usb:
			do {
				if (!re_enum_helper(dev) &&
					!api_protocol_set_cmd(dev, GET_MCU_MOD,
								NULL) &&
					dev->ic[0].mode == target_mode)
					goto success_change_mode;
			} while (cnt++ < retry);
			break;
		}
	} while (cnt++ < retry);

	TP_ERR("current mode: 0x%hhx, change to %s mode failed\n",
		dev->ic[0].mode, (to_bl) ? "BL" : "AP");
	return -EFAULT;

success_change_mode:
	TP_MSG("current mode: 0x%hhx %s mode\n", dev->ic[0].mode,
		(to_bl) ? "BL" : "AP");

	api_protocol_set_cmd(dev, GET_FW_VER, NULL);

	return 0;
}

int api_set_idle(struct ilitek_ts_device *dev, bool enable)
{
	memset(dev->wbuf, 0, 64);
	dev->wbuf[1] = (enable) ? 1 : 0;
	return api_protocol_set_cmd(dev, SET_MCU_IDLE, NULL);
}

int api_set_func_mode(struct ilitek_ts_device *dev, uint8_t mode)
{
	int error;
	bool get = false;

	memset(dev->wbuf, 0, 64);

	switch (dev->protocol.flag) {
	case PTL_V3:
		dev->wbuf[1] = 0x55;
		dev->wbuf[2] = 0xAA;
		break;
	case PTL_V6:
		dev->wbuf[1] = 0x5A;
		dev->wbuf[2] = 0xA5;
		break;
	default:
		TP_ERR("unrecognized protocol: %x, flag: %hhu",
			dev->protocol.ver, dev->protocol.flag);
		return -EINVAL;
	}
	dev->wbuf[3] = mode;

	if (dev->protocol.ver < 0x30400) {
		TP_ERR("protocol: 0x%x not support\n", dev->protocol.ver);
		return -EINVAL;
	}

	error = api_protocol_set_cmd(dev, SET_FUNC_MOD, &get);
	if (error < 0 ||
		(api_get_func_mode(dev)) < 0)
		return error;

	return (dev->func_mode == mode) ? 0 : -EFAULT;
}

int api_get_func_mode(struct ilitek_ts_device *dev)
{
	bool get = true;

	return api_protocol_set_cmd(dev, SET_FUNC_MOD, &get);
}

int api_erase_data_v3(struct ilitek_ts_device *dev)
{
	int error;

	memset(dev->wbuf, 0xff, sizeof(dev->wbuf));

	TP_MSG("erase data flash for V3 protocol: %#x\n", dev->protocol.ver);

	if ((dev->protocol.ver & 0xFFFF00) == BL_PROTOCOL_V1_7) {
		error = api_write_enable_v3(dev, false, false,
						0xf01f, 0);
		if (error < 0)
			return error;

		dev->cb.delay_ms(10);

		error = api_protocol_set_cmd(dev, WRITE_DATA_V3,
							NULL);
		if (error < 0)
			return error;

		dev->cb.delay_ms(500);
	} else if (dev->protocol.flag == PTL_V3) {
		error = api_write_enable_v3(dev, true, false, 0, 0);
		if (error < 0)
			return error;

		dev->cb.delay_ms(100);

		dev->wbuf[1] = 0x02;
		error = api_protocol_set_cmd(dev, TUNING_PARA_V3,
							NULL);
		if (error < 0)
			return error;

		switch (dev->_interface) {
		case interface_usb:
			return re_enum_helper(dev);
		default:
			dev->cb.delay_ms(1500);
			break;
		}
	}

	return 0;
}

int api_write_data_m2v(struct ilitek_ts_device *dev, int wlen)
{
	return api_protocol_set_cmd(dev, WRITE_DATA_M2V, &wlen);
}

int api_to_bl_mode_m2v(struct ilitek_ts_device *dev, bool to_bl)
{
	int cnt = 0, retry = 15;
	const uint8_t target_mode = (to_bl) ? 0x55 : 0x5A;
	uint8_t mode;

	if (dev->_interface != interface_usb)
		return -EINVAL;

	do {
		if (api_access_slave(dev, 0x80, M2V_GET_MOD, &mode) < 0)
			continue;

		if (mode == target_mode)
			goto success_change_mode;

		if (to_bl && api_access_slave(dev, 0x80, SLAVE_SET_BL,
					NULL) < 0)
			continue;
		else if (api_access_slave(dev, 0x80, SLAVE_SET_AP, NULL) < 0)
			continue;

		do {
			if (!re_enum_helper(dev) &&
				!api_access_slave(dev, 0x80, M2V_GET_MOD, &mode) &&
				mode == target_mode)
				goto success_change_mode;
			dev->cb.delay_ms(5000);
		} while (cnt++ < retry);
		break;
	} while (cnt++ < retry);

	TP_ERR("M2V current mode: 0x%hhx, change to %s mode failed\n",
		mode, (to_bl) ? "BL" : "AP");
	return -EFAULT;

success_change_mode:
	TP_MSG("M2V current mode: 0x%hhx %s mode\n", mode,
		(to_bl) ? "BL" : "AP");

	return 0;
}

int api_update_ts_info(void *handle)
{
	int error;
	struct ilitek_ts_device *dev = (struct ilitek_ts_device *)handle;

	dev->protocol.flag = PTL_V6;
	dev->tp_info.ic_num = 1;

	error = api_set_ctrl_mode(dev, mode_suspend, false);
	if (error < 0)
		return error;
	error = api_protocol_set_cmd(dev, GET_PTL_VER, NULL);
	if (error < 0)
		return error;
	error = api_protocol_set_cmd(dev, GET_MCU_MOD, NULL);
	if (error < 0)
		return error;
	error = api_protocol_set_cmd(dev, GET_MCU_VER, NULL);
	if (error < 0)
		return error;
	error = api_protocol_set_cmd(dev, GET_FW_VER, NULL);
	if (error < 0)
		return error;
	error = api_protocol_set_cmd(dev, GET_AP_CRC, NULL);
	if (error < 0)
		return error;

	error = api_protocol_set_cmd(dev, GET_FWID, NULL);
	if (dev->protocol.flag == PTL_V6 && (
		api_protocol_set_cmd(dev, GET_MCU_INFO, NULL) < 0 ||
		api_protocol_set_cmd(dev, GET_PRODUCT_INFO, NULL) < 0 ||
		error < 0))
		return error;

	/* BL mode should perform FW upgrade afterward */
	if (dev->ic[0].mode != 0x5A)
		return 0;

	/* V3 need to get DF CRC */
	error = api_protocol_set_cmd(dev, GET_DF_CRC, NULL);
	if (dev->protocol.flag == PTL_V3 &&
		error < 0)
		return error;

	error = api_protocol_set_cmd(dev, GET_CORE_VER, NULL);
	if (error < 0 ||
		(api_protocol_set_cmd(dev, GET_SCRN_RES, NULL)) < 0 ||
		(api_protocol_set_cmd(dev, GET_TP_INFO, NULL)) < 0 ||
		(api_get_func_mode(dev)) < 0)
		return error;

	error = api_protocol_set_cmd(dev, GET_AP_CRC,
			&dev->tp_info.ic_num);
	if (dev->tp_info.ic_num > 1 &&
		(error < 0 ||
		(api_protocol_set_cmd(dev, GET_MCU_MOD,
		&dev->tp_info.ic_num)) < 0))
		return error;

	error = api_set_ctrl_mode(dev, mode_normal, false);
	if (error < 0)
		return error;

	return 0;
}

void __ilitek_get_ts_info(void *handle, struct ilitek_tp_info_v6 *tp_info)
{
	struct ilitek_ts_device *dev = (struct ilitek_ts_device *)handle;

	if (!tp_info || !dev)
		return;

	memcpy(tp_info, &dev->tp_info, sizeof(struct ilitek_tp_info_v6));
}

void *ilitek_dev_init(uint8_t _interface, struct ilitek_ts_callback *callback,
			void *_private)
{
	struct ilitek_ts_device *dev;

	TP_MSG("CommonFlow-Protocol code version: %#x\n",
		PROTOCOL_CODE_VERSION);

	dev = (struct ilitek_ts_device *)MALLOC(sizeof(*dev));
	if (!dev)
		return NULL;

	/* initial all member to 0/ false/ NULL */
	memset(dev, 0, sizeof(*dev));

	if (callback) {
		memcpy(&dev->cb, callback, sizeof(struct ilitek_ts_callback));
		if (dev->cb.msg)
			g_msg = dev->cb.msg;
	}

	dev->_interface = _interface;
	dev->_private = _private;

	/* get tp dev first */
	if (api_update_ts_info(dev) < 0)
		goto err_free;

	return dev;

err_free:
	FREE(dev);
	return NULL;
}

void ilitek_dev_exit(void *handle)
{
	struct ilitek_ts_device *dev = (struct ilitek_ts_device *)handle;

	if (dev)
		FREE(dev);
}
