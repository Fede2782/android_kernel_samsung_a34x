/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#ifndef __MMEVENT_H__
#define __MMEVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/bug.h>
#include <linux/spinlock.h>

#define MME_DATA_MAX 15
#define MME_MODULE_NAME_LEN 16
#define MME_CPU_ID_MAX 12

enum mme_log_modules {
	MME_MODULE_DISP = 0,
	MME_MODULE_MMSYS,
	MME_MODULE_VIDEO,
	MME_MODULE_CAMERA,
	MME_MODULE_TEST,
	MME_MODULE_MAX
};

enum mme_buffer_index {
	MME_BUFFER_INDEX_0,
	MME_BUFFER_INDEX_1,
	MME_BUFFER_INDEX_2,
	MME_BUFFER_INDEX_3,
	MME_BUFFER_INDEX_4,
	MME_BUFFER_INDEX_5,
	MME_BUFFER_INDEX_6,
	MME_BUFFER_INDEX_7,
	MME_BUFFER_INDEX_8,
	MME_BUFFER_INDEX_9,
	MME_BUFFER_INDEX_MAX
};

enum mme_log_level {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_WARN,  // 1
	LOG_LEVEL_INFO,  // 2
	LOG_LEVEL_DEBUG, // 3
	LOG_LEVEL_MAX
};

enum mme_log_type {
	LOG_TYPE_BASIC = 0,
	LOG_TYPE_CPU_ID
};

enum mme_invalid_flag {
	INVALID_FLAG_HEADER = 100,
	INVALID_MODULE_TOKEN,
	INVALID_MISMATCH_UNIT_SIZE,
	INVALID_SMALL_UNIT_SIZE,
	INVALID_LOG_LEVEL,
	INVALID_FLAG_TYPE
};

struct android_time {
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;
	unsigned int microsecond;
};

struct mme_module_t {
	unsigned int enable;
	unsigned int start;
	unsigned int module;
	char module_buffer_name[MME_BUFFER_INDEX_MAX][MME_MODULE_NAME_LEN];
	unsigned int write_pointer[MME_BUFFER_INDEX_MAX];
	unsigned int buffer_units[MME_BUFFER_INDEX_MAX];
	unsigned int buffer_bytes[MME_BUFFER_INDEX_MAX];
	unsigned int buffer_start_pos[MME_BUFFER_INDEX_MAX]; // start postion of the debug buffer
	unsigned int module_dump_bytes;
	unsigned int reserve[2];
};

struct mme_unit_t {
	unsigned long long data;
};

struct mme_header_t {
	unsigned int module_number;
	unsigned int module_info_size;
	unsigned int pid_number;
	unsigned int pid_info_size;
	unsigned long long kernel_time;
	struct android_time android_time;
	unsigned int unit_size;
	unsigned int reserve[2];
};

struct pid_info_t {
	unsigned int pid;
	char name[TASK_COMM_LEN];
};

extern struct mme_module_t mme_globals[MME_MODULE_MAX];
extern struct mme_unit_t *p_mme_ring_buffer[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX];
extern spinlock_t g_ring_buffer_spinlock[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX];
extern unsigned int g_ring_buffer_units[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX];
extern unsigned int g_flag_shifts[MME_DATA_MAX];

extern bool debug_log_on;
extern bool mme_debug_on;
extern bool g_print_mme_log[MME_MODULE_MAX];

enum data_flag_t {
	DATA_FLAG_INVALID = 0,
	DATA_FLAG_SIZE_1 = 1,
	DATA_FLAG_SIZE_2 = 2,
	DATA_FLAG_SIZE_4 = 3,
	DATA_FLAG_SIZE_8 = 4,
	DATA_FLAG_STACK_REGION_STRING = 5,
	DATA_FLAG_INT_POINTER = 6,
	DATA_FLAG_CODE_REGION_STRING = 7
};

#define MIN(x, y)   ((x) <= (y) ? (x) : (y))
#define FLAG_INT_POINTER_SIZE 16
#define POINTER_SIZE 8
#define U64_POINTER 1
#define U32_POINTER 2

static const int data_size_table[] = {
	0,  // DATA_FLAG_INVALID
	1,  // DATA_FLAG_SIZE_1
	2,  // DATA_FLAG_SIZE_2
	4,  // DATA_FLAG_SIZE_4
	8,  // DATA_FLAG_SIZE_8
	0,  // DATA_FLAG_STACK_REGION_STRING
	FLAG_INT_POINTER_SIZE,  // DATA_FLAG_INT_POINTER
	sizeof(char *) // DATA_FLAG_CODE_REGION_STRING
};

typedef void (*mme_dump_callback)(char *buf, int buf_size, int *dump_size);

extern void mme_register_dump_callback(unsigned int module, mme_dump_callback func);
extern bool mme_register_buffer(
								unsigned int module,
								char *module_buffer_name,
								unsigned int type,
								unsigned int buffer_size
								);
extern void mme_release_buffer(unsigned int module, unsigned int type);
extern unsigned long long mmevent_log(
									unsigned int length,
									unsigned long long flag,
									unsigned int module,
									unsigned int type,
									unsigned int log_level,
									unsigned int log_type
									);

static inline void save_int_pointer_data(unsigned long long p_buf,
										unsigned long p_data,
										int type)
{
	if (type == U64_POINTER)
		*((u64 *)p_buf) = *((u64 *)p_data);
	else if (type == U32_POINTER)
		*((u64 *)p_buf) = (u64)*((u32 *)p_data);
}

#define _ALIGN_4_BYTES(x) (((x) + 3) & ~0x03)
#define _ALIGN_8_BYTES(x) (((x) + 7) & ~0x07)

#define MME_UNIT_SIZE sizeof(struct mme_unit_t)

#define MME_HEADER_UNIT_SIZE 2 // time(8bit), flag(8bit)
#define MME_PID_SIZE 4  // PID 4 byte
#define MME_FMT_SIZE sizeof(char *) // format 8 byte
#define MME_TIMER_SIZE 8
#define MME_FLAG_SIZE 8
#define MME_HEADER_SIZE 16

#define RESERVE_BUFFER_UNITS 64

#define STRING_BUFFER_SIZE 512

#define MAX_PID_COUNT 480

#define MAX_STACK_STR_SIZE 1015

#define MME_MAX_UNIT_NUM 256

#define _MME_UNIT_NUM(x) ((((x) + 7) >> 3) + MME_HEADER_UNIT_SIZE)

#define FLAG_HEADER_DATA_OFFSET 58
#define FLAG_CPU_ID_OFFSET 54
#define FLAG_UNIT_NUM_OFFSET 48
#define FLAG_IRQ_TOKEN_OFFSET 47
#define FLAG_LOG_LEVEL_OFFSET 45
#define FLAG_HEADER_DATA_MASK (0x3FULL << FLAG_HEADER_DATA_OFFSET)
#define FLAG_UNIT_NUM_MASK (0x3FULL << FLAG_UNIT_NUM_OFFSET)
#define FLAG_CPU_ID_MASK (0xFULL << FLAG_CPU_ID_OFFSET)
#define FLAG_IRQ_TOKEN_MASK (0x1ULL << FLAG_IRQ_TOKEN_OFFSET)
#define FLAG_LOG_LEVEL_MASK (0x3ULL << FLAG_LOG_LEVEL_OFFSET)
#define FLAG_HEADER_DATA (0x2AULL << FLAG_HEADER_DATA_OFFSET)

#define MMEINFO(fmt, arg...) \
	do { \
		if (mme_debug_on) \
			pr_info("MME:%s(): "fmt"\n", __func__, ##arg); \
	} while (0)

#define MMEMSG(fmt, arg...) pr_info("MME: %s(): "fmt"\n", __func__, ##arg)
#define MMEERR(fmt, arg...) pr_info("MME ERROR: %s(): "fmt"\n", __func__, ##arg)

#define MMEVENT_LOG(p_mme_buf, length, flag, module, type, log_level) \
	do { \
		unsigned int unit_size = _MME_UNIT_NUM(length); \
		unsigned long flags = 0; \
		bool condition = ((module < MME_MODULE_MAX) & \
					(type < MME_BUFFER_INDEX_MAX) & \
					(unit_size < MME_MAX_UNIT_NUM)) && \
					mme_globals[module].enable && mme_globals[module].start; \
		if (condition) { \
			struct mme_unit_t *p_ring_buffer = p_mme_ring_buffer[module][type]; \
			unsigned long long index = (atomic_add_return(unit_size, \
							(atomic_t *)&(mme_globals[module].write_pointer[type])) - \
							unit_size); \
			if ((mme_globals[module].write_pointer[type]) > g_ring_buffer_units[module][type]) { \
				if (index >= (mme_globals[module].buffer_units[type] - unit_size)) { \
					index = 0; \
				} \
				spin_lock_irqsave(&g_ring_buffer_spinlock[module][type], flags); \
				if ((mme_globals[module].write_pointer[type]) > g_ring_buffer_units[module][type]) { \
					atomic_set((atomic_t *)&(mme_globals[module].write_pointer[type]), 0); \
				} \
				spin_unlock_irqrestore(&g_ring_buffer_spinlock[module][type], flags); \
			} \
			p_ring_buffer[index].data = sched_clock(); \
			p_ring_buffer[index + 1].data = (flag | \
					((unsigned long long)unit_size<<FLAG_UNIT_NUM_OFFSET) | \
					FLAG_HEADER_DATA | \
					((unsigned long long)log_level<<FLAG_LOG_LEVEL_OFFSET) | \
					((unsigned long long)module<<FLAG_MODULE_TOKEN_OFFSET)); \
			p_mme_buf = (unsigned long long)(&(p_ring_buffer[index + 2])); \
		} else { \
			if (unit_size >= MME_MAX_UNIT_NUM) \
				MMEERR("start:%d,unit_size:%d", mme_globals[module].start, unit_size); \
			p_mme_buf = 0; \
		} \
	} while(0)

static inline void mme_vsprintf(char *buf, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, STRING_BUFFER_SIZE, fmt, args);
	va_end(args);
}

struct str_hash_entry {
	unsigned long key;
	char *value;
	struct hlist_node hnode;
};

#ifdef __cplusplus
}
#endif
#endif

