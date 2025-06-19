/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _VL53LX_PLATFORM_USER_DATA_H_
#define _VL53LX_PLATFORM_USER_DATA_H_

#ifndef __KERNEL__
#include <stdlib.h>
#endif

#include "vl53lx_def.h"

#ifdef __cplusplus
extern "C"
{
#endif


struct VL53LX_Dev_t {

	struct VL53LX_DevData_t   Data;

	// add by po-hao
	struct i2c_client *client;

	uint8_t   i2c_slave_address;

	uint8_t   comms_type;

	uint16_t  comms_speed_khz;


	uint32_t  new_data_ready_poll_duration_ms;

};



//typedef VL53LX_Dev_t *VL53LX_DEV;


//#define VL53LX_Dev_t VL53L1_DevData_t
//#define VL53LX_DEV VL53L1_DevData_t *


#define VL53LXDevDataGet(Dev, field) (Dev->Data.field)



#define VL53LXDevDataSet(Dev, field, VL53LX_p_003) ((Dev->Data.field) = (VL53LX_p_003))



#define VL53LXDevStructGetLLDriverHandle(Dev) (&Dev->Data.LLData)



#define VL53LXDevStructGetLLResultsHandle(Dev) (&Dev->Data.llresults)


#ifdef __cplusplus
}
#endif

#endif


