/* SPDX-License-Identifier: GPL-2.0 */
/*
 * hfda80x.h  --  driver for ST HFDA80x codec
 *
 * Copyright(C) 2023  STMicroelectronics Ltd
 * Author:
 */

#ifndef __HFDA80X_H__
#define __HFDA80X_H__

enum hfda80x_type {
	HFDA801,
	HFDA803,
};

struct hfda80x_priv {
	struct device *dev;
	struct snd_soc_component *component;
	const struct cs42xx8_driver_data *drvdata;
	struct regmap *regmap;
	enum hfda80x_type type;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *mute_gpio;
};

#define HFDA801_IB0         0x00
#define HFDA801_IB1         0x01
#define HFDA801_IB2         0x02
#define HFDA801_IB3         0x03
#define HFDA801_IB4         0x04
#define HFDA801_IB5         0x05
#define HFDA801_IB6         0x06
#define HFDA801_IB7         0x07
#define HFDA801_IB8         0x08
#define HFDA801_IB9         0x09
#define HFDA801_IB10        0x0A
#define HFDA801_IB11        0x0B
#define HFDA801_IB12        0x0C
#define HFDA801_IB13        0x0D
#define HFDA801_IB14        0x0E
#define HFDA801_IB15        0x0F
#define HFDA801_IB16        0x10
#define HFDA801_IB17        0x11
#define HFDA801_IB18        0x12
#define HFDA801_IB19        0x13
#define HFDA801_IB20        0x14
#define HFDA801_IB21        0x15
#define HFDA801_IB22        0x16
#define HFDA801_IB23        0x17
#define HFDA801_IB24        0x18

#define HFDA801_DB0         0x20
#define HFDA801_DB1         0x21
#define HFDA801_DB2         0x22
#define HFDA801_DB3         0x23
#define HFDA801_DB4         0x24
#define HFDA801_DB5         0x25
#define HFDA801_DB6         0x26
#define HFDA801_DB7         0x27
#define HFDA801_DB8         0x28
#define HFDA801_DB9         0x29
#define HFDA801_DB10        0x2A
#define HFDA801_DB11        0x2B
#define HFDA801_DB12        0x2C
#define HFDA801_DB13        0x2D
#define HFDA801_DB14        0x2E
#define HFDA801_DB15        0x2F
#define HFDA801_DB16        0x30
#define HFDA801_DB17        0x31
#define HFDA801_DB18        0x32
#define HFDA801_DB19        0x33
#define HFDA801_DB20        0x34
#define HFDA801_DB21        0x35
#define HFDA801_DB22        0x36
#define HFDA801_DB23        0x37
#define HFDA801_DB24        0x3C
#define HFDA801_DB25        0x3D


#define HFDA803_IB0         0x00
#define HFDA803_IB1         0x01
#define HFDA803_IB2         0x02
#define HFDA803_IB3         0x03
#define HFDA803_IB4         0x04
#define HFDA803_IB5         0x05
#define HFDA803_IB6         0x06
#define HFDA803_IB7         0x07
#define HFDA803_IB8         0x08
#define HFDA803_IB9         0x09
#define HFDA803_IB10        0x0A
#define HFDA803_IB11        0x0B
#define HFDA803_IB12        0x0C
#define HFDA803_IB13        0x0D
#define HFDA803_IB14        0x0E

#define HFDA803_DB0         0x20
#define HFDA803_DB1         0x21
#define HFDA803_DB2         0x22
#define HFDA803_DB3         0x26

#endif /* __HFDA80X_H__ */
