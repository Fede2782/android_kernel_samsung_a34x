// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "s5kjn1_otp"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include "kd_camera_typedef.h"


#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5kjn1mipiraw_Sensor.h"


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define S5KJN1_EEPROM_READ_ID    0xA0
#define S5KJN1_EEPROM_WRITE_ID   0xA1
#define S5KJN1_I2C_SPEED         100
//#define s5kjn1_MAX_OFFSET		0xFFFF

//#define DATA_SIZE 2048
#define GGC_START_ADDR (0x3987)

static bool get_done_ggc;
static int last_size_ggc;

//4cell data
#define FCELL_START_ADDR (0x1A41)

static bool get_done_4cell;
static int last_size_4cell;

/*
 * static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
 * {
 * char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
 *
 * if (addr > s5kjn1_MAX_OFFSET)
 *  return false;
 *
 * if (iReadRegI2C(pu_send_cmd,
 *  2,
 *  (u8 *) data,
 *  1,
 *  S5KJN1_EEPROM_READ_ID) < 0)
 *  return false;
 * return true;
 * }
 */
static bool _read_s5kjn1_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;
	int ret;
	u8 pu_send_cmd[2];

	#define MAX_READ_WRITE_SIZE 255
	for (i = 0; i < size; i += MAX_READ_WRITE_SIZE) {
		pu_send_cmd[0] = (u8) (offset >> 8);
		pu_send_cmd[1] = (u8) (offset & 0xFF);

		if (i + MAX_READ_WRITE_SIZE > size) {
			ret = iReadRegI2C(pu_send_cmd, 2,
					 (u8 *) (data + i),
					 (size - i),
					 S5KJN1_EEPROM_READ_ID);

		} else {
			ret = iReadRegI2C(pu_send_cmd, 2,
					 (u8 *) (data + i),
					 MAX_READ_WRITE_SIZE,
					 S5KJN1_EEPROM_READ_ID);
		}
		if (ret < 0) {
			pr_debug("read lrc failed!\n");
			return false;
		}

		offset += MAX_READ_WRITE_SIZE;
	}

	if (addr == GGC_START_ADDR) {
		get_done_ggc = true;
		last_size_ggc = size;
	}

	if (addr == FCELL_START_ADDR) {
		get_done_4cell = true;
		last_size_4cell = size;
	}
	pr_debug("exit _read_eeprom size = %d\n", size);
	return true;
}


bool s5kjn1_read_4cell_otp(char *data, int size, int manual)
{
	int addr = FCELL_START_ADDR;

	pr_debug("read s5kjn1 cell, size = %d\n", size);
	if (manual || !get_done_4cell || last_size_4cell != size) {
		if (!_read_s5kjn1_eeprom(addr, data, size)) {
			get_done_4cell = 0;
			last_size_4cell = 0;
			return false;
		}
	}
	return true;
}

bool s5kjn1_read_otp_ggc(BYTE *data)
{

	int addr = GGC_START_ADDR;
	int size = 346;

	pr_debug("read s5kjn1 ggc, size = %d\n", size);

	if (!get_done_ggc || last_size_ggc != size) {
		if (!_read_s5kjn1_eeprom(addr, data, size)) {
			get_done_ggc = 0;
			last_size_ggc = 0;
		return false;
		}
	}

	return true;
	/* return true; */
}
