// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

// #include <windows.h>
// #ifdef _MSC_VER
// #define snprintf _snprintf
// #endif

#include "vl53lx_platform.h"
#include <vl53lx_platform_log.h>

#define VL53LX_get_register_name(VL53LX_p_007,VL53LX_p_032) VL53LX_COPYSTRING(VL53LX_p_032, "")

//#include "ranging_sensor_comms.h"
//#include "power_board_defs.h"
#include "vl53l4.h"
#include <linux/i2c.h>


#if VL53LX_LOG_POLL_TIMING
/**
 * helper to elapse time in polling
 * @param ptv pointer to start time_val

 */
#	define poll_timing_log(ptv) \
		vl53lx_dbgmsg("poll in %d us\n", tv_elapsed_us(ptv))
#else
#	define poll_timing_log(...) ((void)0)
#endif

#if VL53LX_LOG_CCI_TIMING
/**
 * compute elapsed time in micro  sec based on do_gettimeofday
 * @param tv pointer to start time_val
 * @return time elapsed in micro seconde
 */

/**
 * compute elapsed time in micro  sec based on do_gettimeofday
 * @param tv pointer to start time_val
 * @return time elapsed in  micro seconde
 */
static uint32_t tv_elapsed_us(struct timeval *tv)
{
	struct timeval now;

	do_gettimeofday(&now);
	return (now.tv_sec - tv->tv_sec)*1000000 + (now.tv_usec - tv->tv_usec);
}

#	define	cci_access_var struct timeval cci_log_start_tv
#	define cci_access_start()\
		do_gettimeofday(&cci_log_start_tv)
#	define cci_access_over(fmt, ...) \
		vl53lx_dbgmsg("cci_timing %d us" fmt "\n", \
				tv_elapsed_us(&cci_log_start_tv), ##__VA_ARGS__)
#else
#	define cci_access_var
#	define cci_access_start(...) ((void)0)
#	define cci_access_over(...) ((void)0)
#endif

const uint32_t _power_board_in_use ;

uint32_t _power_board_extended;

uint8_t global_comms_type;

#define  VL53LX_COMMS_CHUNK_SIZE  56
#define  VL53LX_COMMS_BUFFER_SIZE 64

//#define GPIO_INTERRUPT          RS_GPIO62
//#define GPIO_POWER_ENABLE       RS_GPIO60
//#define GPIO_XSHUTDOWN          RS_GPIO61
//#define GPIO_SPI_CHIP_SELECT    RS_GPIO51



#define trace_print(level, ...) \
	_LOG_TRACE_PRINT(VL53LX_TRACE_MODULE_PLATFORM, \
	level, VL53LX_TRACE_FUNCTION_NONE, ##__VA_ARGS__)

#define trace_i2c(...) \
	_LOG_TRACE_PRINT(VL53LX_TRACE_MODULE_NONE, \
	VL53LX_TRACE_LEVEL_NONE, VL53LX_TRACE_FUNCTION_I2C, ##__VA_ARGS__)


#define CP_STATUS int
#define CP_STATUS_OK 1
#define ERROR_TEXT_LENGTH 256
#define RANGING_SENSOR_COMMS_Get_Error_Text


VL53LX_Error VL53LX_CommsInitialise(
	struct VL53LX_Dev_t *pdev,
	uint8_t       comms_type,
	uint16_t      comms_speed_khz)
{
	VL53LX_Error status = VL53LX_ERROR_NONE;

	SUPPRESS_UNUSED_WARNING(pdev);
	SUPPRESS_UNUSED_WARNING(comms_speed_khz);

	global_comms_type = comms_type;

	if (global_comms_type == VL53LX_I2C) {
	} else if (global_comms_type == VL53LX_SPI) {
	} else {
		trace_i2c("%s: Comms must be one of VL53LX_I2C or VL53LX_SPI\n", __func__);
		status = VL53LX_ERROR_CONTROL_INTERFACE;
	}

	return status;
}


VL53LX_Error VL53LX_CommsClose(
	struct VL53LX_Dev_t *pdev)
{
	VL53LX_Error status = VL53LX_ERROR_NONE;

	SUPPRESS_UNUSED_WARNING(pdev);

	if (global_comms_type == VL53LX_I2C) {
	} else if (global_comms_type == VL53LX_SPI) {
	} else {
		trace_i2c("%s: Comms must be one of VL53LX_I2C or VL53LX_SPI\n", __func__);
		status = VL53LX_ERROR_CONTROL_INTERFACE;
	}

	return status;
}


static int cci_write(struct i2c_client *client, int index,
		uint8_t *data, uint16_t len)
{
	uint8_t buffer[VL53LX_MAX_CCI_XFER_SZ+2];
	struct i2c_msg msg;
	int rc;

	cci_access_var;
	if (len > VL53LX_MAX_CCI_XFER_SZ || len == 0) {
		vl53lx_errmsg("invalid len %d\n", len);
		return -1;
	}
	cci_access_start();
	/* build up little endian index in buffer */
	buffer[0] = (index >> 8) & 0xFF;
	buffer[1] = (index >> 0) & 0xFF;
	/* copy write data to buffer after index  */
	memcpy(buffer+2, data, len);
	/* set i2c msg */
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buffer;
	msg.len = len+2;

	rc = i2c_transfer(client->adapter, &msg, 1);
	if (rc != 1) {
		vl53lx_errmsg("wr i2c_transfer err:%d, index 0x%x len %d\n",
			 rc, index, len);
	}
	cci_access_over("rd status %d long %d ", rc != 1, len);
	return rc != 1;
}

static int cci_read(struct i2c_client *client, int index,
		uint8_t *data, uint16_t len)
{
	uint8_t buffer[2];
	struct i2c_msg msg[2];
	int rc;

	cci_access_var;
	if (len > VL53LX_MAX_CCI_XFER_SZ || len == 0) {
		vl53lx_errmsg("invalid len %d\n", len);
		return -1;
	}
	cci_access_start();

	/* build up little endian index in buffer */
	buffer[0] = (index >> 8) & 0xFF;
	buffer[1] = (index >> 0) & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;/* Write */
	msg[0].buf = buffer;
	msg[0].len = 2;
	/* read part of the i2c transaction */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD|client->flags;
	msg[1].buf = data;
	msg[1].len = len;

	rc = i2c_transfer(client->adapter, msg, 2);
	if (rc != 2) {
		vl53lx_errmsg("%s: i2c_transfer :%d, @%x index 0x%x len %d\n",
			__func__, rc, client->addr, index, len);

	}
	cci_access_over(" wr len %d status %d", rc != 2, len);
	return rc != 2;
}

VL53LX_Error VL53LX_WriteMulti(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint8_t      *pdata,
	uint32_t      count)
{
	return cci_write(pdev->client, index, pdata, count) ?
			VL53LX_ERROR_CONTROL_INTERFACE : VL53LX_ERROR_NONE;
}


VL53LX_Error VL53LX_ReadMulti(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint8_t      *pdata,
	uint32_t      count)
{
	return cci_read(pdev->client, index, pdata, count) ?
			VL53LX_ERROR_CONTROL_INTERFACE : VL53LX_ERROR_NONE;
}


VL53LX_Error VL53LX_WrByte(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint8_t       VL53LX_p_003)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	uint8_t  buffer[1];


	buffer[0] = (uint8_t)(VL53LX_p_003);

	status = VL53LX_WriteMulti(pdev, index, buffer, 1);

	return status;
}


VL53LX_Error VL53LX_WrWord(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint16_t      VL53LX_p_003)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	uint8_t  buffer[2];


	buffer[0] = (uint8_t)(VL53LX_p_003 >> 8);
	buffer[1] = (uint8_t)(VL53LX_p_003 &  0x00FF);

	status = VL53LX_WriteMulti(pdev, index, buffer, VL53LX_BYTES_PER_WORD);

	return status;
}


VL53LX_Error VL53LX_WrDWord(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint32_t      VL53LX_p_003)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	uint8_t  buffer[4];


	buffer[0] = (uint8_t) (VL53LX_p_003 >> 24);
	buffer[1] = (uint8_t)((VL53LX_p_003 &  0x00FF0000) >> 16);
	buffer[2] = (uint8_t)((VL53LX_p_003 &  0x0000FF00) >> 8);
	buffer[3] = (uint8_t) (VL53LX_p_003 &  0x000000FF);

	status = VL53LX_WriteMulti(pdev, index, buffer, VL53LX_BYTES_PER_DWORD);

	return status;
}


VL53LX_Error VL53LX_RdByte(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint8_t      *pdata)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	uint8_t  buffer[1];

	status = VL53LX_ReadMulti(pdev, index, buffer, 1);

	*pdata = buffer[0];

	return status;
}
EXPORT_SYMBOL(VL53LX_RdByte);

VL53LX_Error VL53LX_RdWord(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint16_t     *pdata)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	uint8_t  buffer[2];

	status = VL53LX_ReadMulti(
					pdev,
					index,
					buffer,
					VL53LX_BYTES_PER_WORD);

	*pdata = (uint16_t)(((uint16_t)(buffer[0])<<8) + (uint16_t)buffer[1]);

	return status;
}


VL53LX_Error VL53LX_RdDWord(
	struct VL53LX_Dev_t *pdev,
	uint16_t      index,
	uint32_t     *pdata)
{
	VL53LX_Error status = VL53LX_ERROR_NONE;
	uint8_t  buffer[4];

	status = VL53LX_ReadMulti(
					pdev,
					index,
					buffer,
					VL53LX_BYTES_PER_DWORD);

	*pdata = ((uint32_t)buffer[0]<<24) + ((uint32_t)buffer[1]<<16) + ((uint32_t)buffer[2]<<8) + (uint32_t)buffer[3];

	return status;
}



VL53LX_Error VL53LX_WaitUs(
	struct VL53LX_Dev_t *pdev,
	int32_t       wait_us)
{
	/* follow Documentation/timers/timers-howto.txt recommendations */
	if (wait_us < 10)
		udelay(wait_us);
	else if (wait_us < 20000)
		usleep_range(wait_us, wait_us + 1);
	else
		msleep(wait_us / 1000);

	return VL53LX_ERROR_NONE;
}


VL53LX_Error VL53LX_WaitMs(
	struct VL53LX_Dev_t *pdev,
	int32_t       wait_ms)
{
	return VL53LX_WaitUs(pdev, wait_ms * 1000);
}


VL53LX_Error VL53LX_GetTimerFrequency(int32_t *ptimer_freq_hz)
{
	*ptimer_freq_hz = 0;

	trace_print(VL53LX_TRACE_LEVEL_INFO, "%s: Freq : %dHz\n", __func__, *ptimer_freq_hz);
	return VL53LX_ERROR_NONE;
}


VL53LX_Error VL53LX_GetTimerValue(int32_t *ptimer_count)
{
	*ptimer_count = 0;

	trace_print(VL53LX_TRACE_LEVEL_INFO, "%s: Freq : %dHz\n", __func__, *ptimer_count);
	return VL53LX_ERROR_NONE;
}


VL53LX_Error VL53LX_GpioSetMode(uint8_t pin, uint8_t mode)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;

	trace_print(VL53LX_TRACE_LEVEL_INFO, "Status %d. Pin %d, Mode %d\n", status, pin, mode);
	return status;
}


VL53LX_Error  VL53LX_GpioSetValue(uint8_t pin, uint8_t value)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;

	trace_print(VL53LX_TRACE_LEVEL_INFO, "Status %d. Pin %d, Mode %d\n", status, pin, value);
	return status;

}


VL53LX_Error  VL53LX_GpioGetValue(uint8_t pin, uint8_t *pvalue)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	return status;
}



VL53LX_Error  VL53LX_GpioXshutdown(uint8_t value)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	return status;
}


VL53LX_Error  VL53LX_GpioCommsSelect(uint8_t value)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	return status;
}


VL53LX_Error  VL53LX_GpioPowerEnable(uint8_t value)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;
	return status;
}


VL53LX_Error  VL53LX_GpioInterruptEnable(void (*function)(void), uint8_t edge_type)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;

	SUPPRESS_UNUSED_WARNING(function);
	SUPPRESS_UNUSED_WARNING(edge_type);

	return status;
}


VL53LX_Error  VL53LX_GpioInterruptDisable(void)
{
	VL53LX_Error status         = VL53LX_ERROR_NONE;

	return status;
}


VL53LX_Error VL53LX_GetTickCount(
		struct VL53LX_Dev_t *pdev,
		uint32_t *ptick_count_ms)
{
	VL53LX_Error status  = VL53LX_ERROR_NONE;
	(void) pdev;

	//*ptick_count_ms = timeGetTime();
	*ptick_count_ms = 0;

	trace_print(
	VL53LX_TRACE_LEVEL_DEBUG,
	"%s() = %5u ms;\n", __func__
	*ptick_count_ms);

	return status;

}


VL53LX_Error VL53LX_WaitValueMaskEx(
	struct VL53LX_Dev_t *pdev,
	uint32_t      timeout_ms,
	uint16_t      index,
	uint8_t       value,
	uint8_t       mask,
	uint32_t      poll_delay_ms)
{


	VL53LX_Error status         = VL53LX_ERROR_NONE;
	uint32_t     start_time_ms   = 0;
	uint32_t     current_time_ms = 0;
	uint8_t      byte_value      = 0;
	uint8_t      found           = 0;
#ifdef VL53LX_LOG_ENABLE
	uint32_t     trace_functions = 0;
#endif

	_LOG_STRING_BUFFER(register_name);

	SUPPRESS_UNUSED_WARNING(poll_delay_ms);

#ifdef VL53LX_LOG_ENABLE

	VL53LX_get_register_name(
			index,
			register_name);


	trace_i2c("WaitValueMaskEx(%5d, %s, 0x%02X, 0x%02X, %5d);\n",
		timeout_ms, register_name, value, mask, poll_delay_ms);
#endif



	VL53LX_GetTickCount(pdev, &start_time_ms);
	pdev->new_data_ready_poll_duration_ms = 0;



#ifdef VL53LX_LOG_ENABLE
	trace_functions = _LOG_GET_TRACE_FUNCTIONS();
#endif
	_LOG_SET_TRACE_FUNCTIONS(VL53LX_TRACE_FUNCTION_NONE);



	while ((status == VL53LX_ERROR_NONE) &&
		   (pdev->new_data_ready_poll_duration_ms < timeout_ms) &&
		   (found == 0)) {
		status = VL53LX_RdByte(
						pdev,
						index,
						&byte_value);

		if ((byte_value & mask) == value)
			found = 1;

		VL53LX_GetTickCount(pdev, &current_time_ms);
		pdev->new_data_ready_poll_duration_ms = current_time_ms - start_time_ms;
	}


	_LOG_SET_TRACE_FUNCTIONS(trace_functions);

	if (found == 0 && status == VL53LX_ERROR_NONE)
		status = VL53LX_ERROR_TIME_OUT;

	return status;
}


