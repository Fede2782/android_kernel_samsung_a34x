/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MMEVENT_FUNCTION_H__
#define __MMEVENT_FUNCTION_H__

#include "mmevent.h"

#define MME_REGISTER_DUMP_FUNC(module, func) \
		mme_register_dump_callback(module, func)

#define MME_REGISTER_BUFFER(module, module_name, type, buffer_size) \
		mme_register_buffer(module, module_name, type, buffer_size)

#define MME_RELEASE_BUFFER(module, type) \
		mme_release_buffer(module, type)

#define MME_EXTEND_ERROR(module, buf_index, fmt, ...) \
	do { \
		char buf[STRING_BUFFER_SIZE]; \
		mme_vsprintf(buf, fmt, ##__VA_ARGS__); \
		MME_ERROR(module, buf_index, "%s", buf); \
	} while(0)

#define MME_EXTEND_WARN(module, buf_index, fmt, ...) \
	do { \
		char buf[STRING_BUFFER_SIZE]; \
		mme_vsprintf(buf, fmt, ##__VA_ARGS__); \
		MME_WARN(module, buf_index, "%s", buf); \
	} while(0)

#define MME_EXTEND_INFO(module, buf_index, fmt, ...) \
	do { \
		char buf[STRING_BUFFER_SIZE]; \
		mme_vsprintf(buf, fmt, ##__VA_ARGS__); \
		MME_INFO(module, buf_index, "%s", buf); \
	} while(0)

#define MME_EXTEND_DEBUG(module, buf_index, fmt, ...) \
	do { \
		char buf[STRING_BUFFER_SIZE]; \
		mme_vsprintf(buf, fmt, ##__VA_ARGS__); \
		MME_DEBUG(module, buf_index, "%s", buf); \
	} while(0)

#define _MME_EXTEND_LOG(module, buf_index, log_level, log_type, fmt, ...) \
	do { \
		char buf[STRING_BUFFER_SIZE]; \
		mme_vsprintf(buf, fmt, ##__VA_ARGS__); \
		_MMEVENT_LOG_CHOICE_2(module, buf_index, log_level, log_type, "%s", buf); \
	} while(0)

#define MME_LOG(module, buf_index, log_level, log_type, ...) \
	_MMEVENT_LOG(_NUM_ARGS(__VA_ARGS__), module, buf_index, log_level, log_type, ##__VA_ARGS__)

#define MME_ERROR(module, buf_index, ...) \
	_MMEVENT_LOG(_NUM_ARGS(__VA_ARGS__), module, buf_index, LOG_LEVEL_ERROR, LOG_TYPE_BASIC, ##__VA_ARGS__)

#define MME_WARN(module, buf_index, ...) \
	_MMEVENT_LOG(_NUM_ARGS(__VA_ARGS__), module, buf_index, LOG_LEVEL_WARN, LOG_TYPE_BASIC, ##__VA_ARGS__)

#define MME_INFO(module, buf_index, ...) \
	_MMEVENT_LOG(_NUM_ARGS(__VA_ARGS__), module, buf_index, LOG_LEVEL_INFO, LOG_TYPE_BASIC, ##__VA_ARGS__)

#define MME_DEBUG(module, buf_index, ...) \
	_MMEVENT_LOG(_NUM_ARGS(__VA_ARGS__), module, buf_index, LOG_LEVEL_DEBUG, LOG_TYPE_BASIC, ##__VA_ARGS__)

#define _MMEVENT_LOG(N, module, buf_index, log_level, log_type, ...) \
	_MMEVENT_LOG_CHOICE(N, module, buf_index, log_level, log_type, ##__VA_ARGS__)

#define _MMEVENT_LOG_CHOICE(N, module, buf_index, log_level, log_type, ...) \
	do { \
		if (g_print_mme_log[module]) \
			_MMEVENT_LOG_CHOICE_##N(module, buf_index, log_level, log_type, ##__VA_ARGS__); \
		else \
			_print_kernel_log(log_level, ##__VA_ARGS__); \
	} while(0)

#define _print_kernel_log(log_level, fmt, ...) \
	do { \
		if (log_level == LOG_LEVEL_ERROR) \
			pr_info("ERROR:"fmt, ##__VA_ARGS__);    \
		else \
			pr_info(fmt, ##__VA_ARGS__);    \
	} while(0)

#define _MMEVENT_LOG_CHOICE_1(module, type, log_level, log_type, format) \
	do { \
		if ((module < MME_MODULE_MAX) && (type < MME_BUFFER_INDEX_MAX) && \
			(g_ring_buffer_units[module][type] > 0)) { \
			unsigned int mme_format_size = sizeof(char *); \
			unsigned long long p_mme_buf; \
			p_mme_buf = mmevent_log((mme_format_size + MME_PID_SIZE), \
									0, module, type, log_level, log_type); \
			if (p_mme_buf) { \
				SAVE_PROCESS(p_mme_buf); \
				SAVE_FORMAT(p_mme_buf, format, mme_format_size); \
			} \
		} \
	} while(0)

#define _MMEVENT_LOG_CHOICE_2(module, type, log_level, log_type, format, data1) \
	do { \
		if ((module < MME_MODULE_MAX) && (type < MME_BUFFER_INDEX_MAX) && \
			(g_ring_buffer_units[module][type] > 0)) { \
			unsigned int mme_format_size = sizeof(char *); \
			unsigned int mme_data1_size=0, mme_data1_flag=0, mme_str_len=0; \
			unsigned long long p_mme_buf; \
			PROCESS_DATA(data1, mme_data1_size, mme_data1_flag, mme_str_len); \
			p_mme_buf = mmevent_log((mme_format_size + mme_data1_size + MME_PID_SIZE), \
								mme_data1_flag, module , type, log_level, log_type); \
			if (p_mme_buf) { \
				SAVE_PROCESS(p_mme_buf); \
				SAVE_FORMAT(p_mme_buf, format, mme_format_size); \
				SAVE_DATA(p_mme_buf, data1, mme_data1_size, mme_data1_flag, mme_str_len); \
			} \
		} \
	} while(0)

#define _MMEVENT_LOG_PORXY(N, module, type, log_level, log_type, format, ...) \
	do { \
		if ((module < MME_MODULE_MAX) && (type < MME_BUFFER_INDEX_MAX) && \
			(g_ring_buffer_units[module][type] > 0)) { \
			unsigned int mme_format_size = sizeof(char *); \
			unsigned int mme_data_size[N]={0}, mme_data_flag[N]={0}, mme_str_len[N]={0}; \
			unsigned int mme_log_length = mme_format_size + MME_PID_SIZE, mme_index; \
			unsigned long long mme_log_flag = 0, p_mme_buf = 0; \
			PROCESS_DATA_##N(mme_data_size, mme_data_flag, mme_str_len, ##__VA_ARGS__); \
			for (mme_index=0; mme_index<N; mme_index++) { \
				mme_log_length += mme_data_size[mme_index]; \
				mme_log_flag |= ((unsigned long long)mme_data_flag[mme_index] << \
								g_flag_shifts[mme_index]); \
			} \
			p_mme_buf = mmevent_log(mme_log_length, mme_log_flag, module, type, log_level, log_type); \
			if (p_mme_buf) { \
				SAVE_PROCESS(p_mme_buf); \
				SAVE_FORMAT(p_mme_buf, format, mme_format_size); \
				SAVE_DATA_##N(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, ##__VA_ARGS__); \
			} \
		} \
	} while(0)

#define SAVE_PROCESS(p) \
	do { \
		*((char **)p) = (char *)(unsigned long)(current->tgid); \
		p += MME_PID_SIZE; \
	} while(0)

#define _MMEVENT_LOG_CHOICE_3(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(2, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_4(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(3, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_5(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(4, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_6(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(5, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_7(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(6, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_8(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(7, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_9(module, type, log_level, log_type, format, ...)  \
		_MMEVENT_LOG_PORXY(8, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_10(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(9, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_11(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(10, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_12(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(11, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_13(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(12, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_14(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(13, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_15(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(14, module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_16(module, type, log_level, log_type, format, ...) \
		_MMEVENT_LOG_PORXY(15, module, type, log_level, log_type, format, ##__VA_ARGS__)

#define _MMEVENT_LOG_CHOICE_17(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_18(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_19(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_20(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_21(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_22(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_23(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_24(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_25(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_26(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_27(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_28(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_29(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_30(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_31(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)
#define _MMEVENT_LOG_CHOICE_32(module, type, log_level, log_type, format, ...) \
		_MME_EXTEND_LOG(module, type, log_level, log_type, format, ##__VA_ARGS__)

#define MME_UNSUPPORTED_NUM_ARGS(type, ...)

#define _NUM_ARGS(...)  _NUM_ARGS_IMPL(__VA_ARGS__, \
						32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16, \
						15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define _NUM_ARGS_IMPL(e1,e2,e3,e4,e5,e6,e7,e8,e9,e10,e11,e12,e13,e14,e15,e16, \
					e17,e18,e19,e20,e21,e22,e23,e24,e25,e26,e27,e28,e29,e30,e31,e32,N,...) N

#define _TYPE_OF_STRING(x) \
	_Generic((x), \
			const char*: 1, \
			char*: 2, \
			char (*)[]: 3, \
			default: \
			0)

#define _TYPE_OF_INT_POINTER(x) \
	_Generic((x), \
			u64 *: U64_POINTER, \
			u32 *: U32_POINTER, \
			default: \
			0)

#define DATA_FLAG(size)  ((size) == 4 ? DATA_FLAG_SIZE_4 : \
						((size) == 8 ? DATA_FLAG_SIZE_8 : \
						((size) == 1 ? DATA_FLAG_SIZE_1 : \
						((size) == 2 ? DATA_FLAG_SIZE_2 : \
						((size) == FLAG_INT_POINTER_SIZE ? DATA_FLAG_INT_POINTER : \
						0)))))

#define MME_STR_OFFSET(ptr1, ptr2) abs(((uintptr_t)(ptr1) - (uintptr_t)(ptr2)))

#define PROCESS_DATA(data, size_data, flag_data, str_len) \
	do { \
		bool is_stack_str = false; \
		bool is_str_data = _TYPE_OF_STRING(data); \
		bool is_int_pointer_data = _TYPE_OF_INT_POINTER(data); \
		if (is_str_data && (MME_STR_OFFSET(__func__, data) > 0x100000)) \
			is_stack_str = true; \
		size_data = (is_str_data ? (is_stack_str ? \
					(_ALIGN_4_BYTES(sizeof(char *) + \
					(str_len=MIN(strlen((char *)(unsigned long)(data)), \
					MAX_STACK_STR_SIZE)) + 1)) : \
					sizeof(char *)): (is_int_pointer_data ? FLAG_INT_POINTER_SIZE : \
					sizeof(typeof(data)))); \
		flag_data = (is_str_data ? \
					(is_stack_str ? \
					DATA_FLAG_STACK_REGION_STRING : DATA_FLAG_CODE_REGION_STRING) : \
					DATA_FLAG(size_data)); \
	} while(0)

#define SAVE_FORMAT(p, data, size_data) \
	do { \
		*((char **)p) = (char *)(unsigned long)(data); \
		p += size_data; \
	} while(0)

#define SAVE_DATA(p, data, size_data, flag_data, str_len) \
	do { \
		switch (flag_data) { \
		case DATA_FLAG_STACK_REGION_STRING: \
			if ((char *)(unsigned long)(data)) { \
				*((char **)p) = (char *)(unsigned long)(data); \
				strscpy((char *)(p+sizeof(char *)), (char *)(unsigned long)(data), \
						str_len+1); \
			} \
			break; \
		case DATA_FLAG_SIZE_1: \
		case DATA_FLAG_SIZE_2: \
		case DATA_FLAG_SIZE_4: \
			*((unsigned int *)p) = (unsigned int)(unsigned long)(data); \
			break; \
		case DATA_FLAG_INT_POINTER: \
			*((char **)p) = (char *)(unsigned long)(data); \
			save_int_pointer_data(p+POINTER_SIZE, (unsigned long)(data), \
								_TYPE_OF_INT_POINTER(data)); \
			break; \
		default: \
			*((char **)p) = (char *)(unsigned long)(data); \
			break; \
		} \
		p += size_data; \
	} while(0)

#define PROCESS_DATA_2(mme_data_size, mme_data_flag, mme_str_len, data1, data2) \
	do { \
		PROCESS_DATA(data1, mme_data_size[0], mme_data_flag[0], mme_str_len[0]); \
		PROCESS_DATA(data2, mme_data_size[1], mme_data_flag[1], mme_str_len[1]); \
	} while(0)

#define PROCESS_DATA_3(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3) \
	do { \
		PROCESS_DATA_2(mme_data_size, mme_data_flag, mme_str_len, data1, data2); \
		PROCESS_DATA(data3, mme_data_size[2], mme_data_flag[2], mme_str_len[2]); \
	} while(0)

#define PROCESS_DATA_4(mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
						data3, data4) \
	do { \
		PROCESS_DATA_3(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3); \
		PROCESS_DATA(data4, mme_data_size[3], mme_data_flag[3], mme_str_len[3]); \
	} while(0)

#define PROCESS_DATA_5(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5) \
	do { \
		PROCESS_DATA_4(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, data4); \
		PROCESS_DATA(data5, mme_data_size[4], mme_data_flag[4], mme_str_len[4]); \
	} while(0)

#define PROCESS_DATA_6(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6) \
	do { \
		PROCESS_DATA_5(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5); \
		PROCESS_DATA(data6, mme_data_size[5], mme_data_flag[5], mme_str_len[5]); \
	} while(0)

#define PROCESS_DATA_7(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7) \
	do { \
		PROCESS_DATA_6(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6); \
		PROCESS_DATA(data7, mme_data_size[6], mme_data_flag[6], mme_str_len[6]); \
	} while(0)

#define PROCESS_DATA_8(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8) \
	do { \
		PROCESS_DATA_7(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7); \
		PROCESS_DATA(data8, mme_data_size[7], mme_data_flag[7], mme_str_len[7]); \
	} while(0)

#define PROCESS_DATA_9(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9) \
	do { \
		PROCESS_DATA_8(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8); \
		PROCESS_DATA(data9, mme_data_size[8], mme_data_flag[8], mme_str_len[8]); \
	} while(0)

#define PROCESS_DATA_10(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10) \
	do { \
		PROCESS_DATA_9(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9); \
		PROCESS_DATA(data10, mme_data_size[9], mme_data_flag[9], mme_str_len[9]); \
	} while(0)

#define PROCESS_DATA_11(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11) \
	do { \
		PROCESS_DATA_10(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10); \
		PROCESS_DATA(data11, mme_data_size[10], mme_data_flag[10], mme_str_len[10]); \
	} while(0)

#define PROCESS_DATA_12(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, data12) \
	do { \
		PROCESS_DATA_11(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11); \
		PROCESS_DATA(data12, mme_data_size[11], mme_data_flag[11], mme_str_len[11]); \
	} while(0)

#define PROCESS_DATA_13(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, \
						data12, data13) \
	do { \
		PROCESS_DATA_12(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, data12); \
		PROCESS_DATA(data13, mme_data_size[12], mme_data_flag[12], mme_str_len[12]); \
	} while(0)

#define PROCESS_DATA_14(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, data12, \
						data13, data14) \
	do { \
		PROCESS_DATA_13(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, \
						data12, data13); \
		PROCESS_DATA(data14, mme_data_size[13], mme_data_flag[13], mme_str_len[13]); \
	} while(0)

#define PROCESS_DATA_15(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, data4, \
						data5, data6, data7, data8, data9, data10, data11, data12, data13, \
						data14, data15) \
	do { \
		PROCESS_DATA_14(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, data4, \
						data5, data6, data7, data8, data9, data10, data11, data12, \
						data13, data14); \
		PROCESS_DATA(data15, mme_data_size[14], mme_data_flag[14], mme_str_len[14]); \
	} while(0)

#define PROCESS_DATA_16(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, data12, \
						data13, data14, data15, data16) \
	do { \
		PROCESS_DATA_15(mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
						data4, data5, data6, data7, data8, data9, data10, data11, data12, \
						data13, data14, data15); \
		PROCESS_DATA(data16, mme_data_size[15], mme_data_flag[15], mme_str_len[15]); \
	} while(0)

#define SAVE_DATA_2(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2) \
	do { \
		SAVE_DATA(p_mme_buf, data1, mme_data_size[0], mme_data_flag[0], mme_str_len[0]); \
		SAVE_DATA(p_mme_buf, data2, mme_data_size[1], mme_data_flag[1], mme_str_len[1]); \
	} while(0)

#define SAVE_DATA_3(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3) \
	do { \
		SAVE_DATA_2(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2); \
		SAVE_DATA(p_mme_buf, data3, mme_data_size[2], mme_data_flag[2], mme_str_len[2]); \
	} while(0)

#define SAVE_DATA_4(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
					data3, data4) \
	do { \
		SAVE_DATA_3(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3); \
		SAVE_DATA(p_mme_buf, data4, mme_data_size[3], mme_data_flag[3], mme_str_len[3]); \
	} while(0)

#define SAVE_DATA_5(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
					data3, data4, data5) \
	do { \
		SAVE_DATA_4(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
					data3, data4); \
		SAVE_DATA(p_mme_buf, data5, mme_data_size[4], mme_data_flag[4], mme_str_len[4]); \
	} while(0)

#define SAVE_DATA_6(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
					data3, data4, data5, data6) \
	do { \
		SAVE_DATA_5(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5); \
		SAVE_DATA(p_mme_buf, data6, mme_data_size[5], mme_data_flag[5], mme_str_len[5]); \
	} while(0)

#define SAVE_DATA_7(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7) \
	do { \
		SAVE_DATA_6(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
					data3, data4, data5, data6); \
		SAVE_DATA(p_mme_buf, data7, mme_data_size[6], mme_data_flag[6], mme_str_len[6]); \
	} while(0)

#define SAVE_DATA_8(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8) \
	do { \
		SAVE_DATA_7(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7); \
		SAVE_DATA(p_mme_buf, data8, mme_data_size[7], mme_data_flag[7], mme_str_len[7]); \
	} while(0)

#define SAVE_DATA_9(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9) \
	do { \
		SAVE_DATA_8(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8); \
		SAVE_DATA(p_mme_buf, data9, mme_data_size[8], mme_data_flag[8], mme_str_len[8]); \
	} while(0)

#define SAVE_DATA_10(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10) \
	do { \
		SAVE_DATA_9(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, \
					data3, data4, data5, data6, data7, data8, data9); \
		SAVE_DATA(p_mme_buf, data10, mme_data_size[9], mme_data_flag[9], mme_str_len[9]); \
	} while(0)

#define SAVE_DATA_11(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11) \
	do { \
		SAVE_DATA_10(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10); \
		SAVE_DATA(p_mme_buf, data11, mme_data_size[10], mme_data_flag[10], mme_str_len[10]); \
	} while(0)

#define SAVE_DATA_12(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12) \
	do { \
		SAVE_DATA_11(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11); \
		SAVE_DATA(p_mme_buf, data12, mme_data_size[11], mme_data_flag[11], mme_str_len[11]); \
	} while(0)

#define SAVE_DATA_13(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, data13) \
	do { \
		SAVE_DATA_12(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12); \
		SAVE_DATA(p_mme_buf, data13, mme_data_size[12], mme_data_flag[12], mme_str_len[12]); \
	} while(0)

#define SAVE_DATA_14(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, \
					data13, data14) \
	do { \
		SAVE_DATA_13(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, data13); \
		SAVE_DATA(p_mme_buf, data14, mme_data_size[13], mme_data_flag[13], mme_str_len[13]); \
	} while(0)

#define SAVE_DATA_15(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, data13, \
					data14, data15) \
	do { \
		SAVE_DATA_14(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, \
					data13, data14); \
		SAVE_DATA(p_mme_buf, data15, mme_data_size[14], mme_data_flag[14], mme_str_len[14]); \
	} while(0)

#define SAVE_DATA_16(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, data13, \
					data14, data15, data16) \
	do { \
		SAVE_DATA_15(p_mme_buf, mme_data_size, mme_data_flag, mme_str_len, data1, data2, data3, \
					data4, data5, data6, data7, data8, data9, data10, data11, data12, data13, \
					data14, data15); \
		SAVE_DATA(p_mme_buf, data16, mme_data_size[15], mme_data_flag[15], mme_str_len[15]); \
	} while(0)
#endif

