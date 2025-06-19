/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ak7709ctl.h  --  audio driver for AK7709
 *
 * Copyright (C) 2022 Asahi Kasei Microdevices Corporation
 * Author            Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                   22/09/21    0.1  Kernel 4.19
 *                   22/12/07    1.0  Kernel 4.19
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#ifndef __AK7709_CTRL_H__
#define __AK7709_CTRL_H__

/* IO CONTROL definition of AK7709 */
#define AK7709_IOCTL_MAGIC             's'

#define AK7709_MAGIC                   0xD0


struct ak7709_msbox_handle{
	unsigned int hifi4;
	unsigned int msbox_num;
	unsigned int addr;
	unsigned int len;
	unsigned char *msbox;
	unsigned int gpioNo;
};

struct ak7709_wcram_handle{
	int    dsp_no;
	int    addr;
	unsigned char *cram;
	unsigned int len;
};

struct ak7709_mir_handle{
	int    dsp_no;
	unsigned char *mir;
	unsigned int len;
};

#define AK7709_IOCTL_MSBOX_WRITE      _IOW(AK7709_MAGIC, 0x11, struct ak7709_msbox_handle)
#define AK7709_IOCTL_MSBOX_READ       _IOR(AK7709_MAGIC, 0x12, struct ak7709_msbox_handle)
#define AK7709_IOCTL_WRITECRAM        _IOW(AK7709_MAGIC, 0x13, struct ak7709_wcram_handle)
#define AK7709_IOCTL_GETMIR           _IOR(AK7709_MAGIC, 0x14, struct ak7709_mir_handle)

#endif
