// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/miscdevice.h>
#include <linux/file.h>

#include <linux/atomic.h>
#include <linux/hashtable.h>

#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/uaccess.h>

#include <linux/ftrace.h>

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif

#include "mmevent.h"

/* If it is 64bit use __pa_nodebug, otherwise use __pa_symbol_nodebug or __pa */
#ifndef __pa_nodebug
#ifdef __pa_symbol_nodebug
#define __pa_nodebug __pa_symbol_nodebug
#else
#define __pa_nodebug __pa
#endif
#endif

static int bmme_init_buffer;

static struct mme_header_t mme_header = {
	.module_number = MME_MODULE_MAX,
	.module_info_size = sizeof(struct mme_module_t),
	.pid_info_size = sizeof(struct pid_info_t),
	.unit_size = sizeof(struct mme_unit_t),
};

bool mme_debug_on;

unsigned int g_flag_shifts[MME_DATA_MAX] = {0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42};
EXPORT_SYMBOL(g_flag_shifts);

unsigned long long mmevent_log(unsigned int length, unsigned long long flag,
								unsigned int module, unsigned int type,
								unsigned int log_level, unsigned int log_type)
{
	unsigned int unit_size = _MME_UNIT_NUM(length);
	unsigned long flags = 0;
	bool condition = (unit_size < MME_MAX_UNIT_NUM) && mme_globals[module].start;
	unsigned int cpu_id;
	bool is_irq;

	if (condition) {
		struct mme_unit_t *p_ring_buffer = p_mme_ring_buffer[module][type];
		unsigned long long index = (atomic_add_return(unit_size,
							(atomic_t *)&(mme_globals[module].write_pointer[type])) -
							unit_size);
		if ((mme_globals[module].write_pointer[type]) > g_ring_buffer_units[module][type]) {
			if (index >= (mme_globals[module].buffer_units[type] - unit_size))
				index = 0;

			spin_lock_irqsave(&g_ring_buffer_spinlock[module][type], flags);
			if ((mme_globals[module].write_pointer[type]) > g_ring_buffer_units[module][type])
				atomic_set((atomic_t *)&(mme_globals[module].write_pointer[type]), 0);

			spin_unlock_irqrestore(&g_ring_buffer_spinlock[module][type], flags);
		}
		p_ring_buffer[index].data = sched_clock();
		p_ring_buffer[index + 1].data = (flag | ((unsigned long long)unit_size<<FLAG_UNIT_NUM_OFFSET) |
								FLAG_HEADER_DATA |
								((unsigned long long)log_level<<FLAG_LOG_LEVEL_OFFSET));

		if (log_type == LOG_TYPE_CPU_ID) {
			cpu_id = raw_smp_processor_id() & 0xF;
			is_irq = (bool)in_irq();
			p_ring_buffer[index + 1].data |= (((unsigned long long)is_irq<<FLAG_IRQ_TOKEN_OFFSET) |
								((unsigned long long)cpu_id<<FLAG_CPU_ID_OFFSET));
		}

		return (unsigned long long)(&(p_ring_buffer[index + MME_HEADER_UNIT_SIZE]));
	}

	if (unit_size >= MME_MAX_UNIT_NUM)
		MMEERR("start:%d,unit_size:%d", mme_globals[module].start, unit_size);
	return 0;
}
EXPORT_SYMBOL(mmevent_log);
// ------------------------------ hash table ---------------------------------------------------

#define STR_HASH_BITS 10

DECLARE_HASHTABLE(str_hash_table, STR_HASH_BITS);

void add_hash_entry(unsigned long key, char *value)
{
	struct str_hash_entry *entry;
	char c;
	long ret;

	if (!value)
		return;

	ret = copy_from_kernel_nofault(&c, value, sizeof(c));
	if (ret != 0) {
		MMEERR("invalid value:%llx,ret:%ld", (unsigned long long)value, ret);
		return;
	}

	entry = kmalloc(sizeof(struct str_hash_entry), GFP_ATOMIC);
	if (!entry)
		return;

	entry->key = key;
	entry->value = value;

	MMEINFO("key: %lx, value: %s", key, value);
	hash_add(str_hash_table, &entry->hnode, key);
}

char *get_hash_value(unsigned long key)
{
	struct str_hash_entry *entry;
	char *value = NULL;

	hash_for_each_possible(str_hash_table, entry, hnode, key) {
		if (entry->key == key) {
			value = entry->value;
			break;
		}
	}
	return value;
}

#define MAX_INVALID_ENTRIES 128
char *hash_table_to_char_array(unsigned int *p_buffer_size)
{
	struct str_hash_entry *entry;
	unsigned int bkt;
	unsigned int total_size = 0;
	char *array, *ptr;
	int ret;
	char c;
	unsigned int invalid_indices[MAX_INVALID_ENTRIES];
	unsigned int invalid_count = 0;

	hash_for_each(str_hash_table, bkt, entry, hnode) {
		ret = copy_from_kernel_nofault(&c, entry->value, sizeof(c));
		if (ret != 0) {
			MMEERR("invalid value:%llx,ret:%d,invalid_count:%d",(unsigned long long)entry->value,
				ret, invalid_count);
			if (invalid_count < MAX_INVALID_ENTRIES) {
				invalid_indices[invalid_count++] = bkt;
			} else {
				MMEERR("invalid count exceeded max entries:%d", MAX_INVALID_ENTRIES);
				return NULL;
			}
			continue;
		}
		total_size += 16 + strlen(entry->value) + 2;
	}

	MMEINFO("total_size:%d", total_size);
	if (total_size == 0) {
		MMEERR("total_size is 0");
		return NULL;
	}

	array = kmalloc(total_size, GFP_ATOMIC);
	if (!array) {
		MMEERR("kmalloc failed, array is null");
		return NULL;
	}

	ptr = array;
	hash_for_each(str_hash_table, bkt, entry, hnode) {
		bool skip_entry = false;

		for (unsigned int i = 0; i < invalid_count; ++i) {
			if (invalid_indices[i] == bkt) {
				skip_entry = true;
				break;
			}
		}

		if (skip_entry)
			continue;

		ret = snprintf(ptr, total_size - (ptr - array), "%lx:", entry->key);
		if (ret < 0) {
			MMEERR("snprintf failed, ret:%d", ret);
			kfree(array);
			return NULL;
		}
		ptr += ret;

		ret = snprintf(ptr, total_size - (ptr - array), "%s", entry->value);
		if (ret < 0) {
			MMEERR("snprintf failed, ret:%d", ret);
			kfree(array);
			return NULL;
		}
		ptr += ret;
		*ptr = '\0';
		ptr += 1;
	}
	if (ptr != array)
		*(ptr - 1) = '\0';

	*p_buffer_size = total_size;
	return array;
}

// ------------------------------ buffer init ---------------------------------------------------

unsigned int g_ring_buffer_units[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX] = {0};
EXPORT_SYMBOL(g_ring_buffer_units);

struct mme_unit_t *p_mme_ring_buffer[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX] = {0};
EXPORT_SYMBOL(p_mme_ring_buffer);

spinlock_t g_ring_buffer_spinlock[MME_MODULE_MAX][MME_BUFFER_INDEX_MAX] ={0};
EXPORT_SYMBOL(g_ring_buffer_spinlock);

struct mme_module_t mme_globals[MME_MODULE_MAX] = {0};
EXPORT_SYMBOL(mme_globals);

bool g_print_mme_log[MME_MODULE_MAX] = {true};
EXPORT_SYMBOL(g_print_mme_log);

#define MME_MRDUMP_BUFFER_SIZE (3*1024*1024)
#define MAX_MODULE_BUFFER_SIZE (10*1024*1024)

#if !IS_ENABLED(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define DBG_BUFFER_INIT_SIZE (2880*1024)
#else
#define DBG_BUFFER_INIT_SIZE (4096+67*256)
#endif

char *g_mrdump_buffer;
char *g_dbg_buffer;
unsigned int g_dbg_buffer_pos;
static struct pid_info_t *p_pid_buffer;

static DEFINE_SPINLOCK(g_register_spinlock);

bool mme_register_buffer(unsigned int module, char *module_buf_name, unsigned int type,
						unsigned int buffer_size)
{
	unsigned long va;
	unsigned long pa;
	char module_aee_name[256];
	unsigned long flags = 0;
	unsigned int module_buf_size = buffer_size;
	int ret;

	DEFINE_SPINLOCK(t_spinlock);

	if (module >= MME_MODULE_MAX || type >= MME_BUFFER_INDEX_MAX ||
		module_buf_size > MAX_MODULE_BUFFER_SIZE || !module_buf_name) {
		MMEERR("register fail, module:%d, type:%d, module_buf_size:%d, module_buf_name:%s",
				module, type, module_buf_size, module_buf_name);
		return false;
	}

	if (p_mme_ring_buffer[module][type]) {
		MMEERR("Duplicate register buf, module:%d, type:%d, module_buf_size:%d, module_buf_name:%s",
				module, type, module_buf_size, module_buf_name);
		return false;
	}

	spin_lock_irqsave(&g_register_spinlock, flags);
	if (!g_dbg_buffer) {
		g_dbg_buffer_pos = 0;
		g_dbg_buffer = kzalloc(DBG_BUFFER_INIT_SIZE, GFP_KERNEL);

		if (!g_dbg_buffer) {
			MMEERR("Failed to allocate g_dbg_buffer");
			spin_unlock_irqrestore(&g_register_spinlock, flags);
			return false;
		}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
		va = (unsigned long)g_dbg_buffer;
		pa = __pa_nodebug(va);

		mrdump_mini_add_extra_file(va, pa, DBG_BUFFER_INIT_SIZE, "MME_DEBUG");
		if (!g_mrdump_buffer) {
			g_mrdump_buffer = kzalloc(MME_MRDUMP_BUFFER_SIZE, GFP_KERNEL);
			if (!g_mrdump_buffer)
				MMEERR("Failed to allocate g_mrdump_buffer");
		}
#endif
	}

	if (!p_pid_buffer) {
		p_pid_buffer = kcalloc(MAX_PID_COUNT, sizeof(struct pid_info_t), GFP_KERNEL);
		if (!p_pid_buffer) {
			MMEERR("ERROR: allocate memory for pid_buffer failed");
			return false;
		}
	}

	MMEMSG("module:%d,module_buf_name:%s,type:%d,module_buf_size:%d, g_dbg_buffer_pos:%d",
			module, module_buf_name, type, module_buf_size, g_dbg_buffer_pos);

	if (g_dbg_buffer_pos + module_buf_size <= DBG_BUFFER_INIT_SIZE) {
		p_mme_ring_buffer[module][type] = (struct mme_unit_t *)(g_dbg_buffer + g_dbg_buffer_pos);
		mme_globals[module].buffer_start_pos[type] = g_dbg_buffer_pos + 1;
		g_dbg_buffer_pos += module_buf_size;
	} else {
		p_mme_ring_buffer[module][type] = kzalloc(module_buf_size, GFP_KERNEL);

		if (!p_mme_ring_buffer[module][type]) {
			MMEERR("Failed to allocate memory for ring buffer\n");
			spin_unlock_irqrestore(&g_register_spinlock, flags);
			return false;
		}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		va = (unsigned long)p_mme_ring_buffer[module][type];
		pa = __pa_nodebug(va);

		ret = snprintf(module_aee_name, sizeof(module_aee_name), "MME_%s", module_buf_name);
		if (ret < 0 || ret >= (sizeof(module_aee_name)-1)) {
			MMEERR("module aee name snprintf error, ret:%d", ret);
			spin_unlock_irqrestore(&g_register_spinlock, flags);
			return false;
		}
		mrdump_mini_add_extra_file(va, pa, mme_globals[module].buffer_bytes[type], module_aee_name);
#endif
	}

	mme_globals[module].module = module;
	memset(mme_globals[module].module_buffer_name[type], 0, MME_MODULE_NAME_LEN);
	if (module_buf_name)
		memcpy(mme_globals[module].module_buffer_name[type], module_buf_name,
				MIN(strlen(module_buf_name), (MME_MODULE_NAME_LEN-1)));
	mme_globals[module].write_pointer[type] = 0;
	mme_globals[module].buffer_bytes[type] = module_buf_size;
	mme_globals[module].buffer_units[type] = module_buf_size / MME_UNIT_SIZE;
	g_ring_buffer_units[module][type] = mme_globals[module].buffer_units[type] -
										RESERVE_BUFFER_UNITS;

	spin_unlock_irqrestore(&g_register_spinlock, flags);

	g_ring_buffer_spinlock[module][type] = t_spinlock;
	g_print_mme_log[module] = true;
	mme_globals[module].enable = 1;
	bmme_init_buffer = 1;

	return true;
}
EXPORT_SYMBOL(mme_register_buffer);

void mme_release_buffer(unsigned int module, unsigned int type)
{
	if (mme_globals[module].buffer_start_pos[type] == 0 && p_mme_ring_buffer[module][type])
		vfree(p_mme_ring_buffer[module][type]);

	p_mme_ring_buffer[module][type] = 0;
	mme_globals[module].write_pointer[type] = 0;
	mme_globals[module].buffer_bytes[type] = 0;
	mme_globals[module].buffer_units[type] = 0;
	g_ring_buffer_units[module][type] = 0;
}
EXPORT_SYMBOL(mme_release_buffer);

static void mme_release_all_buffer(void)
{
	unsigned int module, type;

	if (g_dbg_buffer) {
		vfree(g_dbg_buffer);
		g_dbg_buffer = 0;
		g_dbg_buffer_pos = 0;
	}

	for (module=0; module<MME_MODULE_MAX; module++) {
		for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
			if (p_mme_ring_buffer[module][type]) {
				if (mme_globals[module].buffer_start_pos[type] == 0)
					vfree(p_mme_ring_buffer[module][type]);
				else
					mme_globals[module].buffer_start_pos[type] = 0;

				p_mme_ring_buffer[module][type] = 0;
				g_ring_buffer_units[module][type] = 0;
				mme_globals[module].write_pointer[type] = 0;
				mme_globals[module].buffer_bytes[type] = 0;
				mme_globals[module].buffer_units[type] = 0;
			}
		}
	}
}

static void mme_log_pause(void)
{
	unsigned int module;

	for (module=0; module<MME_MODULE_MAX; module++) {
		MMEINFO("%s before pause, module:%d,start:%d",
				__func__, module, mme_globals[module].start);
		mme_globals[module].start = 0;
	}
}

static void mme_log_start(void)
{
	unsigned int module;

	for (module=0; module<MME_MODULE_MAX; module++)
		mme_globals[module].start = 1;
}

static void mme_scale_buffer(unsigned int module, unsigned int scale_value)
{
	unsigned int type;
	char module_buf_name[MME_MODULE_NAME_LEN];

	MMEMSG("module:%d, scale_value:%d", module, scale_value);


	mme_log_pause();
	for (type = 0; type < MME_BUFFER_INDEX_MAX; type++) {
		memcpy(module_buf_name, mme_globals[module].module_buffer_name[type],
				MME_MODULE_NAME_LEN - 1);
		module_buf_name[MME_MODULE_NAME_LEN - 1] = '\0';

		if (mme_globals[module].buffer_bytes[type] != 0) {
			unsigned int new_buffer_size = mme_globals[module].buffer_bytes[type] *
											scale_value;

			mme_release_buffer(module, type);
			mme_register_buffer(module, module_buf_name, type,
							MIN(MAX_MODULE_BUFFER_SIZE, new_buffer_size));
		}
	}
	mme_log_start();
}

// -------------------------------------- Register Dump callback -----------------------------------
#define MME_MODULE_DUMP_SIZE (1024*6)

static char *p_dump_buffer[MME_MODULE_MAX] = {0};
static mme_dump_callback dump_callback_table[MME_MODULE_MAX] = {0};

void mme_register_dump_callback(unsigned int module, mme_dump_callback func)
{
	MMEMSG("module:%d", module);
	if (module >= MME_MODULE_MAX) {
		MMEERR("unsupport module:%d", module);
		return;
	}
	dump_callback_table[module] = func;
	if (!p_dump_buffer[module]) {
		p_dump_buffer[module] = kzalloc(MME_MODULE_DUMP_SIZE, GFP_KERNEL);
		if (!p_dump_buffer[module])
			MMEERR("Failed to allocate memory for p_dump_buffer,module:%d", module);
	}
}
EXPORT_SYMBOL(mme_register_dump_callback);
// ---------------------------------------- Dump section -------------------------------------------

#define MME_DUMP_BLOCK_SIZE (1024*4)
#define STRING_BUFFER_LEN 1024
#define INVALIDE_EVENT -1
#define SUCCESS 1

static unsigned char mme_dump_block[MME_DUMP_BLOCK_SIZE];
static char *p_str_buffer;
unsigned int g_str_buffer_size;


static unsigned int mme_fill_dump_block(void *p_src, void *p_dst,
	unsigned int *p_src_pos, unsigned int *p_dst_pos,
	unsigned int src_size, unsigned int dst_size)
{
	unsigned int src_left = src_size - *p_src_pos;
	unsigned int dst_left = dst_size - *p_dst_pos;

	if (!p_src || !p_dst)
		return 0;

	if ((src_left == 0) || (dst_left == 0))
		return 0;

	if (src_left < dst_left) {
		memcpy(((unsigned char *)p_dst) + *p_dst_pos,
			((unsigned char *)p_src) + *p_src_pos, src_left);
		*p_src_pos += src_left;
		*p_dst_pos += src_left;
		return src_left;
	}

	memcpy(((unsigned char *)p_dst) + *p_dst_pos,
		((unsigned char *)p_src) + *p_src_pos, dst_left);
	*p_src_pos += dst_left;
	*p_dst_pos += dst_left;
	return dst_left;
}

static bool check_log_flag(unsigned long long flag, unsigned int *p_code_region_num,
							unsigned int *p_error_token)
{
	unsigned int unit_size = (flag & FLAG_UNIT_NUM_MASK) >> FLAG_UNIT_NUM_OFFSET;
	unsigned int cpu_id = (flag & FLAG_CPU_ID_MASK) >> FLAG_CPU_ID_OFFSET;
	unsigned int log_level = (flag & FLAG_LOG_LEVEL_MASK) >> FLAG_LOG_LEVEL_OFFSET;
	unsigned int data_size = sizeof(char *) + MME_PID_SIZE;
	unsigned int data_unit_size = 0;
	unsigned int code_region_num = 0;
	unsigned int stack_region_num = 0;
	unsigned int i = 0;

	if((flag & FLAG_HEADER_DATA_MASK)!=FLAG_HEADER_DATA) {
		MMEINFO("invalid flag header, flag:%lld", flag);
		*p_error_token = INVALID_FLAG_HEADER;
		return false;
	}

	if (cpu_id >= MME_CPU_ID_MAX) {
		MMEMSG("invalid flag cpu id, flag:%lld, cpu_id:%d", flag, cpu_id);
		*p_error_token = INVALID_MODULE_TOKEN;
		return false;
	}

	if (unit_size < (MME_HEADER_UNIT_SIZE +
					DIV_ROUND_UP((MME_FMT_SIZE + MME_PID_SIZE), MME_UNIT_SIZE))) {
		MMEMSG("invalid unit_size, unit_size:%d", unit_size);
		*p_error_token = INVALID_SMALL_UNIT_SIZE;
		return false;
	}

	if (log_level >= LOG_LEVEL_MAX) {
		MMEMSG("invalid flag log level, flag:%lld", flag);
		*p_error_token = INVALID_LOG_LEVEL;
		return false;
	}

	for (i=0; i< MME_DATA_MAX; i++) {
		unsigned int shift = g_flag_shifts[i];
		unsigned int type = (flag >> shift) & 0x7;

		if (type == DATA_FLAG_INVALID)
			break;

		if (type == DATA_FLAG_CODE_REGION_STRING)
			code_region_num++;

		if (type == DATA_FLAG_STACK_REGION_STRING)
			stack_region_num++;

		data_size += data_size_table[type];
	}

	data_unit_size = _MME_UNIT_NUM(data_size);

	if ((stack_region_num==0 && unit_size != data_unit_size) ||
		unit_size < data_unit_size) {
		MMEERR("invalid unit_size, unit_size:%d, data_unit_size:%d, stack_region_num:%d",
				unit_size, data_unit_size, stack_region_num);
		*p_error_token = INVALID_MISMATCH_UNIT_SIZE;
		return false;
	}

	*p_code_region_num = code_region_num;
	return true;
}

static bool is_valid_addr(char *p)
{
	if (p == NULL)
		return false;

	if (sizeof(p) > sizeof(int)) {
		unsigned long long addr = (unsigned long long)p;

		if ((addr >> 60) != 0xF)
			return false;

		if ((addr & 0xFFFFFFFF) == 0x0)
			return false;
	}

	return true;
}

static bool is_valid_index(unsigned int index, struct mme_unit_t *p_ring_buffer,
							unsigned int buffer_units)
{
	unsigned int i, sum = 0;

	if (index >= (buffer_units - MME_HEADER_UNIT_SIZE)) {
		MMEERR("invalid index %d,buffer_units:%d", index, buffer_units);
		return false;
	}

	if (p_ring_buffer[index].data == 0 && p_ring_buffer[index+1].data == 0) {
		for (i=index+2; i<MIN(buffer_units, index+20); i++)
			sum += p_ring_buffer[i].data;

		return (sum != 0);
	}
	return true;
}

/**
 * @p_src: source buffer
 * @p_dst: destination buffer
 * @src_size: size of source buffer
 * @dst_size: size of destination buffer
 * @p_total_pos: file position
 * @p_region_base: total buffer size in front
 * @p_dst_pos: current position in destination buffer
 */
static void mme_dump_buffer(void *p_src, void *p_dst, unsigned int src_size, unsigned int dst_size,
					unsigned int *p_total_pos, unsigned int *p_region_base, unsigned int *p_dst_pos)
{
	unsigned int src_pos = 0;

	if (*p_total_pos < (*p_region_base + src_size)) {
		src_pos = *p_total_pos - *p_region_base;

		mme_fill_dump_block(p_src,
					p_dst,
					&src_pos, p_dst_pos,
					src_size,
					dst_size);
		*p_total_pos = *p_region_base + src_size;
	}
	*p_region_base += src_size;
}

static void get_pid_info(struct mme_unit_t *p_ring_buffer, unsigned int buffer_units,
						struct pid_info_t *p_pid_buffer, unsigned int *p_pid_count)
{
	unsigned long long flag = 0, time=0, p = 0;
	unsigned int code_region_num = 0, unit_size = 0, index=0, flag_error_token = 0;
	unsigned int pid, pid_index, i, data_size = 0;
	char *p_str;
	int ret;

	for (index = 0; index < buffer_units;) {
		if (!(is_valid_index(index, p_ring_buffer, buffer_units))) {
			MMEINFO("invalid index, index:%d,buffer_units:%d", index, buffer_units);
			break;
		}

		time = p_ring_buffer[index].data;
		flag = p_ring_buffer[index + 1].data;
		unit_size = (flag & FLAG_UNIT_NUM_MASK) >> FLAG_UNIT_NUM_OFFSET;

		if (time==0 ||
			((index + unit_size) >= buffer_units) ||
			!check_log_flag(flag, &code_region_num, &flag_error_token)) {
			index += 1;
			MMEINFO("invalid event, index:%d,unit_size:%d,buffer_units:%d", index, unit_size, buffer_units);
			continue;
		}

		p = (unsigned long long)&(p_ring_buffer[index + MME_HEADER_UNIT_SIZE]);
		pid = *((unsigned int *)p);

		for (pid_index = 0; pid_index < *p_pid_count; pid_index++) {
			if (p_pid_buffer[pid_index].pid == pid)
				break;
		}
		if (pid_index == *p_pid_count && *p_pid_count < MAX_PID_COUNT) {
			struct task_struct *task;
			unsigned int cpu_id = (flag & FLAG_CPU_ID_MASK) >> FLAG_CPU_ID_OFFSET;

			p_pid_buffer[pid_index].pid = pid;
			if (pid == 0) {
				memset(p_pid_buffer[pid_index].name, 0, TASK_COMM_LEN);
				ret = sprintf(p_pid_buffer[pid_index].name, "swapper/%d", cpu_id);
				if (ret < 0)
					MMEERR("pid buf name sprintf error,ret:%d", ret);
			} else {
				rcu_read_lock();
				task = find_task_by_vpid(pid);
				if (task != NULL)
					get_task_comm(p_pid_buffer[pid_index].name, task);
				rcu_read_unlock();
			}

			MMEINFO("pid:%d, pid_name:%s", pid, p_pid_buffer[pid_index].name);
			*p_pid_count += 1;
		}

		p +=  MME_PID_SIZE; // PID
		data_size = MME_PID_SIZE;
		// format
		p_str = *((char **)p);
		if (!is_valid_addr(p_str)) {
			MMEERR("invalid format p_str, index:%d, p_str:%llx", index, (unsigned long long)p_str);
			index += 2;
			continue;
		}
		MMEINFO("format addr:%p, format:%s, len:%zu,index:%d,unit_size:%d,flag:%llx",
				p_str, p_str, strlen(p_str), index, unit_size, flag);

		if (!get_hash_value((unsigned long)p_str))
			add_hash_entry((unsigned long)p_str, p_str);

		data_size += sizeof(char *);
		p += sizeof(char *);

		if (code_region_num == 0) {
			index += unit_size;
			continue;
		}

		// dump code region string data
		for (i=0; i< MME_DATA_MAX; i++) {
			unsigned int shift = g_flag_shifts[i];
			unsigned int type = (flag >> shift) & 0x7;
			int type_size = data_size_table[type];

			if (type == DATA_FLAG_INVALID)
				break;

			if (type == DATA_FLAG_CODE_REGION_STRING) {
				p_str = *((char **)p);
				if (is_valid_addr(p_str)) {
					MMEINFO("str data addr:%llx, str data:%s, len:%zu",
							(unsigned long long)p_str, p_str, strlen(p_str));

					if (!get_hash_value((unsigned long)p_str))
						add_hash_entry((unsigned long)p_str, p_str);
				} else {
					MMEERR("invalid p_str, index:%d, p_str:%llx, unit_size:%d, i:%d",
							index, (unsigned long long)p_str, unit_size, i);
				}
			}

			if (type == DATA_FLAG_STACK_REGION_STRING) {
				int max_length = (unit_size - MME_HEADER_UNIT_SIZE) * MME_UNIT_SIZE - data_size;

				max_length = MIN(max_length, MAX_STACK_STR_SIZE);
				p_str = (char *)p;
				type_size = sizeof(char *);
				p_str += type_size;

				while (*p_str != '\0' && type_size < max_length) {
					type_size += 1;
					p_str += 1;
				}
				if (type_size == max_length && *p_str != '\0') {
					MMEERR("invalid stack str,index:%d,type_size:%d,unit_size:%d,data_size:%d,i:%d",
							index, type_size, unit_size, data_size, i);
					break;
				}
				type_size = _ALIGN_4_BYTES(type_size + 1);
			}

			data_size += type_size;
			p += type_size;
		}

		index += unit_size;
		MMEINFO("END, data_size = %d, unit size = %d, index:%d",
				data_size, unit_size, index);
	}
}

static void mme_init_process_info(struct pid_info_t *p_pid_buffer, unsigned int *p_pid_count)
{
	unsigned int module, type;

	for (module=0; module<MME_MODULE_MAX; module++) {
		if (mme_globals[module].enable) {
			for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
				get_pid_info(p_mme_ring_buffer[module][type], mme_globals[module].buffer_units[type],
							p_pid_buffer, p_pid_count);
			}
		}
	}
}

static void mme_init_android_time(void)
{
	struct tm tm;
	struct timespec64 tv = { 0 };

	mme_header.kernel_time = sched_clock();
	ktime_get_real_ts64(&tv);
	time64_to_tm(tv.tv_sec, 0, &tm);

	mme_header.android_time.year = tm.tm_year + 1900;
	mme_header.android_time.month = tm.tm_mon + 1;
	mme_header.android_time.day = tm.tm_mday;
	mme_header.android_time.hour = tm.tm_hour;
	mme_header.android_time.minute = tm.tm_min;
	mme_header.android_time.second = tm.tm_sec;
	mme_header.android_time.microsecond = (unsigned int)(tv.tv_nsec / 1000);
}

static void mme_get_dump_buffer(unsigned int start, void *p_block_buf, unsigned int block_buf_size,
								unsigned int *p_copy_size, bool is_mrdump)
{
	unsigned int total_pos = start;
	unsigned int total_index = 0;
	unsigned int block_pos = 0;
	unsigned int region_base = 0;
	unsigned int module, type;
	unsigned int pid_count = 0;

	*p_copy_size = 0;
	if (!p_block_buf || block_buf_size==0) {
		MMEERR("ERROR: NULL p_block_buf, p_block_buf:%p, block_buf_size:%u",
				p_block_buf, block_buf_size);
		return;
	}

	if (total_pos == 0) {
		unsigned int dump_size = 0;

		mme_init_android_time();

		mme_init_process_info(p_pid_buffer, &pid_count);
		mme_header.pid_number = ALIGN(pid_count, 4);

		g_str_buffer_size = 0;
		p_str_buffer = hash_table_to_char_array(&g_str_buffer_size);
		if (!p_str_buffer || g_str_buffer_size == 0) {
			MMEERR("ERROR: allocate p_str_buffer failed, p_str_buffer:%p, g_str_buffer_size:%d",
				p_str_buffer, g_str_buffer_size);
			return;
		}

		for (module=0; module<MME_MODULE_MAX; module++) {
			if (mme_globals[module].enable) {
				if (dump_callback_table[module]) {
					if (p_dump_buffer[module]) {
						dump_size = 0;
						memset(p_dump_buffer[module], 0, MME_MODULE_DUMP_SIZE);
						dump_callback_table[module](p_dump_buffer[module],
								MME_MODULE_DUMP_SIZE, &dump_size);
						mme_globals[module].module_dump_bytes = ALIGN(dump_size, 16);
						MMEMSG("module:%d,dump_size:%d", module, dump_size);
					}

				}
			}
		}
	}

	*p_copy_size = block_buf_size;

	mme_dump_buffer(&mme_header, p_block_buf, sizeof(mme_header),
					block_buf_size, &total_pos, &region_base, &block_pos);
	if (block_pos == block_buf_size)
		return;

	mme_dump_buffer(&mme_globals, p_block_buf, sizeof(mme_globals),
					block_buf_size, &total_pos, &region_base, &block_pos);
	if (block_pos == block_buf_size)
		return;

	mme_dump_buffer(p_pid_buffer, p_block_buf, mme_header.pid_number * sizeof(struct pid_info_t),
					block_buf_size, &total_pos, &region_base, &block_pos);
	if (block_pos == block_buf_size)
		return;

	for (module=0; module<MME_MODULE_MAX; module++) {
		if (mme_globals[module].enable) {
			mme_dump_buffer(p_dump_buffer[module], p_block_buf, mme_globals[module].module_dump_bytes,
							block_buf_size, &total_pos, &region_base, &block_pos);
			if (block_pos == block_buf_size)
				return;

			// If is_mrdump = true, there is no need to dump the contents of p_mme_ring_buffer.
			if (!is_mrdump) {
				for (type=0; type<MME_BUFFER_INDEX_MAX; type++) {
					MMEINFO("module=%d,type=%d,total_index:0x%x,region_base:0x%x,block_pos:0x%x",
							module, type, total_index, region_base, block_pos);
					mme_dump_buffer(p_mme_ring_buffer[module][type], p_block_buf,
							mme_globals[module].buffer_bytes[type],
							block_buf_size, &total_pos, &region_base, &block_pos);
					if (block_pos == block_buf_size)
						return;
				}
			}
		}
	}

	MMEINFO("p_str_buffer:%px, g_str_buffer_size:%d", p_str_buffer, g_str_buffer_size);
	mme_dump_buffer(p_str_buffer, p_block_buf, g_str_buffer_size,
					block_buf_size, &total_pos, &region_base, &block_pos);
	if (block_pos == block_buf_size)
		return;

	*p_copy_size = block_pos;
}

static ssize_t mmevent_dbgfs_buffer_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	unsigned int copy_size = 0;
	unsigned int total_copy = 0;
	unsigned long addr;

	if (!bmme_init_buffer) {
		MMEERR("RingBuffer is not initialized");
		return -EFAULT;
	}

	if (*ppos == 0)
		mme_log_pause();

	MMEINFO("size=%ld ppos=%lld", (unsigned long)size, *ppos);
	while (size > 0) {
		addr = (unsigned long)mme_dump_block;
		memset(mme_dump_block, 0, MME_DUMP_BLOCK_SIZE);
		mme_get_dump_buffer(*ppos, mme_dump_block, MME_DUMP_BLOCK_SIZE, &copy_size, false);
		if (copy_size == 0) {
			mme_log_start();
			break;
		}
		if (size >= copy_size) {
			size -= copy_size;
		} else {
			copy_size = size;
			size = 0;
		}

		if (copy_to_user(buf + total_copy, (void *)addr, copy_size)) {
			MMEERR("fail to copytouser total_copy=%u", total_copy);
			break;
		}
		*ppos += copy_size;
		total_copy += copy_size;
	}

	return total_copy;
}

void mmevent_mrdump_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned int copy_size = 0;

	MMEMSG("mrdump +");
	if (!bmme_init_buffer) {
		MMEERR("RingBuffer is not initialized");
		return;
	}

	if (!g_mrdump_buffer) {
		MMEERR("g_mrdump_buffer is not create");
		return;
	}

	mme_log_pause();

	*vaddr = (unsigned long)g_mrdump_buffer;
	mme_get_dump_buffer(0, g_mrdump_buffer, MME_MRDUMP_BUFFER_SIZE, &copy_size, true);
	*size = copy_size;
	MMEMSG("mrdump - size:%lu", *size);
}

// ------------------------------- Driver Section ---------------------------------------------
/* Debug FS begin */
static struct dentry *g_p_debug_fs_dir;
static struct dentry *g_p_debug_fs_buffer;

static void process_dbg_cmd(char *cmd)
{
	unsigned long value;

	MMEMSG("cmd:%s", cmd);
	if (strncmp(cmd, "mme_debug_on:", 13) == 0) {
		char *p = (char *)cmd + 13;

		if (kstrtoul(p, 10, &value) == 0 && 0 != value)
			mme_debug_on = 1;
		else
			mme_debug_on = 0;
		MMEMSG("mme_log_on=%d\n", mme_debug_on);
	} else if (strncmp(cmd, "scale_ring_buffer:", 18) == 0) {
		char *p = (char *)cmd + 18;
		char *scale_str = NULL;
		unsigned long scale_value = 0;
		unsigned long module = 0;

		p += strspn(p, " ");
		scale_str = strchr(p, ' ');
		MMEMSG("p:%s, scale_str:%s", p, scale_str);
		if (scale_str) {
			*scale_str = '\0';
			if (kstrtoul(p, 10, &module) == 0 && module < MME_MODULE_MAX) {
				if (kstrtoul(scale_str + 1, 10, &scale_value) == 0 && scale_value < 100)
					mme_scale_buffer(module, scale_value);
			}
		}

		MMEMSG("scale_ring_buffer, module:%lu, scale_value:%lu", module, scale_value);
	} else if (strncmp(cmd, "switch_kernel_log:", 18) == 0) {
		char *p = (char *)cmd + 18;
		char *mme_log_str = NULL;
		unsigned long is_kernel_log = 0;
		unsigned long module = 0;

		p += strspn(p, " ");
		mme_log_str = strchr(p, ' ');
		MMEMSG("p:%s, mme_log_str:%s", p, mme_log_str);
		if (mme_log_str) {
			*mme_log_str = '\0';
			if (kstrtoul(p, 10, &module) == 0 && module < MME_MODULE_MAX) {
				if (kstrtoul(mme_log_str + 1, 10, &is_kernel_log) == 0 && is_kernel_log < 100)
					g_print_mme_log[module] = is_kernel_log ? false : true;
			}
		}
		MMEMSG("switch_kernel_log, module:%lu, is_kernel_log:%lu", module, is_kernel_log);
	} else {
		MMEMSG("invalid mme debug command: %s\n",
			cmd != NULL ? cmd : "(empty)");
	}
}

static long mmevent_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return 0;
}

static int mmevent_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmevent_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mmevent_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t mmevent_write(struct file *file, const char __user *data, size_t len, loff_t *ppos)
{
	ssize_t ret;
	size_t length = len;
	char cmd_buf[128];

	if (length > 127)
		length = 127;
	ret = length;

	if (copy_from_user(&cmd_buf, data, length))
		return -EFAULT;

	cmd_buf[length] = 0;
	process_dbg_cmd(cmd_buf);

	return ret;
}

static int mmevent_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;

}

const struct file_operations mmevent_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmevent_ioctl,
	.open = mmevent_open,
	.release = mmevent_release,
	.read = mmevent_read,
	.write = mmevent_write,
	.mmap = mmevent_mmap,
};

static const struct file_operations mmevent_dbgfs_buffer_fops = {
	.read = mmevent_dbgfs_buffer_read,
	.llseek = generic_file_llseek,
};

struct miscdevice *mme_dev;

static int mmevent_probe(void)
{
	int ret = 0;

	g_p_debug_fs_dir = debugfs_create_dir("mme", NULL);
	if (g_p_debug_fs_dir) {
		g_p_debug_fs_buffer =
			debugfs_create_file("buffer", 0400,
								g_p_debug_fs_dir, NULL,
								&mmevent_dbgfs_buffer_fops);
	}

	mme_dev = kzalloc(sizeof(*mme_dev), GFP_KERNEL);
	if (!mme_dev)
		return -ENOMEM;

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
		mrdump_set_extra_dump(AEE_EXTRA_FILE_MME, mmevent_mrdump_buffer);
#endif

	mme_dev->minor = MISC_DYNAMIC_MINOR;
	mme_dev->name = "mme";
	mme_dev->fops = &mmevent_fops;
	mme_dev->parent = NULL;
	ret = misc_register(mme_dev);
	if (ret) {
		MMEERR("mme: failed to register misc device.\n");
		kfree(mme_dev);
		mme_dev = NULL;
		return ret;
	}
	mme_log_start();
	hash_init(str_hash_table);
	return 0;
}

static int __init mme_init(void)
{
	mmevent_probe();
	return 0;
}

static void __exit mme_exit(void)
{
	mme_release_all_buffer();
}

/* Driver specific end */
module_init(mme_init);
module_exit(mme_exit);
MODULE_AUTHOR("Zhongchao Xia <zhongchao.xia@mediatek.com>");
MODULE_DESCRIPTION("MME Driver");
MODULE_LICENSE("GPL");
