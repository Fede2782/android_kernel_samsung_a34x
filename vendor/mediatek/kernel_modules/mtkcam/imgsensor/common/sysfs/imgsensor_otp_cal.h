// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Samsung Electronics Co., Ltd
 */

#ifndef _IMGSENSOR_OTP_CAL_H_
#define _IMGSENSOR_OTP_CAL_H_

#include <linux/i2c.h>

//read functions of OTP cal
int hi847_read_otp_cal(struct i2c_client *client, unsigned char *data, unsigned int size);

#endif /*_IMGSENSOR_OTP_CAL_H_*/
