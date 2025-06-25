/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_PLATFORM_USER_DEFINES_H_
#define _VL53LX_PLATFORM_USER_DEFINES_H_

#ifdef __cplusplus
extern "C"
{
#endif





#define do_division_u(dividend, divisor) (dividend / divisor)



#define do_division_s(dividend, divisor) (dividend / divisor)



#define WARN_OVERRIDE_STATUS(__X__)\
	trace_print (VL53LX_TRACE_LEVEL_WARNING, #__X__)


#ifdef _MSC_VER
#define DISABLE_WARNINGS() { \
	__pragma (warning (push)); \
	__pragma (warning (disable:4127)); \
	}
#define ENABLE_WARNINGS() { \
	__pragma (warning (pop)); \
	}
#else
	#define DISABLE_WARNINGS()
	#define ENABLE_WARNINGS()
#endif


#ifdef __cplusplus
}
#endif

#endif


