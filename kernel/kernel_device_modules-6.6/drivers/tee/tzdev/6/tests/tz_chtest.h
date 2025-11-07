/*
 * Copyright (C) 2016 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CUSTOM_HANDLER_TEST_H_
#define _CUSTOM_HANDLER_TEST_H_

#include "tzdev_internal.h"

struct tz_chtest_data {
	unsigned long r0;
	unsigned long r1;
	unsigned long r2;
	unsigned long r3;
	unsigned long r4;
	unsigned long r5;
};

#define CUSTOM_HANDLER_IOC_MAGIC	'g'
#define CUSTOM_HANDLER_TEST_NAME	"tz_chtest"
#define CUSTOM_HANDLER_TEST_DEV		"/dev/"CUSTOM_HANDLER_TEST_NAME
#define IOCTL_CUSTOM_HANDLER_CMD	_IOWR(CUSTOM_HANDLER_IOC_MAGIC, \
					2, struct tz_chtest_data)

/* Starting raw base for Custom Handler commands */
#define CUSTOM_HANDLER_CMD_BASE_RAW	0x100U

/* Starting raw base for Custom Handlers */
#define CUSTOM_HANDLER_EXEC_BASE_RAW	0x200U

/* Number of custom handlers that could be registered */
#define CUSTOM_HANDLER_COUNT		16

/* Platform uses ARM calling convention (ATF is present) */
/* FIXME Use SMC64 calling convention for 64bit configuration when S.LSI updates SPD */
/*
 * #ifdef CONFIG_64BIT
 * #define CUSTOM_HANDLER_FID_BASE	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_64, SMC_TOS0_SERVICE_MASK, 0)
 * #else
 * #define CUSTOM_HANDLER_FID_BASE	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_TOS0_SERVICE_MASK, 0)
 * #endif
*/
#define CUSTOM_HANDLER_FID_BASE	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_TOS0_SERVICE_MASK, 0)

#endif /* _CUSTOM_HANDLER_TEST_H_ */
