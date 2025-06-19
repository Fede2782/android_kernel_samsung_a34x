// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/reboot.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include "log_store_kernel.h"
#include "mrdump_helper.h"

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
#include <asm/div64.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/kthread.h>
#include <linux/kmsg_dump.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>

extern void register_bootprof_write_log(void (*fn)(char *str, size_t str_len));

#define EXPDB_PATH "/dev/block/by-name/expdb"
#define BOOT_BUFF_SIZE (64*1024)
#define BOOT_LOG_LEN 290
#define READ_KERNEL_LOG_SIZE 0x20000
#endif

static struct sram_log_header *sram_header;
static int sram_log_store_status = BUFF_NOT_READY;
static int dram_log_store_status = BUFF_NOT_READY;
static char *pbuff;
static struct pl_lk_log *dram_curlog_header;
static struct dram_buf_header *sram_dram_buff;
static struct proc_dir_entry *entry;
static u32 last_boot_phase = FLAG_INVALID;
static struct regmap *map;
static u32 pmic_addr;
static bool early_log_disable;

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
struct log_store_partition *expdb_logstore;
static struct task_struct *write_expdb_thread;


DECLARE_WAIT_QUEUE_HEAD(wait_queue);


static inline int logstore_trylock_buffer(struct buffer_head *bh)
{
	return likely(!test_and_set_bit_lock(BH_Lock, &bh->b_state));
}

static inline void logstore_lock_buffer(struct buffer_head *bh)
{
	might_sleep();
	if (!logstore_trylock_buffer(bh))
		__lock_buffer(bh);
}

static inline void logstore_wait_on_buffer(struct buffer_head *bh)
{
	might_sleep();
	if (buffer_locked(bh))
		__wait_on_buffer(bh);
}

static int get_partition_info(void)
{
	struct block_device *bdev;
	dev_t dev_num = 0;

	if (lookup_bdev(EXPDB_PATH, &dev_num) != 0 || dev_num == 0)
		return -EIO;

	bdev = blkdev_get_by_dev(dev_num, FMODE_WRITE | FMODE_READ, NULL, NULL);
	if (IS_ERR(bdev))
		return -EIO;

	expdb_logstore->bdev = bdev;
	expdb_logstore->block_size = block_size(bdev);
	expdb_logstore->part_size = bdev_nr_bytes(bdev);
	expdb_logstore->bootlog_offset = (expdb_logstore->part_size - expdb_logstore->logstore_size
			- expdb_logstore->bootlog_size) / expdb_logstore->block_size;
	expdb_logstore->logstore_offset = (expdb_logstore->part_size - expdb_logstore->logstore_size)
		/ expdb_logstore->block_size;
	expdb_logstore->logindex_offset = expdb_logstore->part_size / expdb_logstore->block_size -1;
	pr_info("log_store: partition %s size 0x%lx, block_size 0x%x, index_offset 0x%lx, 0x%lx, 0x%lx.\n",
			EXPDB_PATH, (unsigned long)expdb_logstore->part_size, expdb_logstore->block_size,
			(unsigned long)expdb_logstore->bootlog_offset, (unsigned long)expdb_logstore->logstore_offset,
			(unsigned long)expdb_logstore->logindex_offset);
	return 0;
}



static int partition_block_rw(struct block_device *bdev, int write, sector_t index,
		sector_t index_offset, void *buffer, size_t len)
{
	struct buffer_head *bh = NULL;
	int err = -EIO;

	if (bdev == NULL)
		if (get_partition_info() != 0)
			return err;

	if (len > expdb_logstore->block_size)
		return -EINVAL;

	if (bdev)
		bh = __getblk_gfp(bdev, index, expdb_logstore->block_size, __GFP_MOVABLE);
	if (bh) {
		clear_bit(BH_Uptodate, &bh->b_state);
		get_bh(bh);
		logstore_lock_buffer(bh);
		bh->b_end_io = end_buffer_read_sync;

		submit_bh(REQ_OP_READ, bh);
		logstore_wait_on_buffer(bh);

		if (unlikely(!buffer_uptodate(bh))) {
			pr_err("log_store: buffer up to date is error!\n");
			goto out;
		}

		if (write) {
			logstore_lock_buffer(bh);
			memcpy(bh->b_data + index_offset, buffer, len);
			bh->b_end_io = end_buffer_write_sync;
			get_bh(bh);
			submit_bh(REQ_OP_WRITE, bh);
			logstore_wait_on_buffer(bh);
			if (unlikely(!buffer_uptodate(bh))) {
				pr_err("log_store: write expdb error!!\n");
				goto out;
			}
		} else {
			memcpy(buffer, bh->b_data + index_offset, len);
		}
		err = 0;
	} else {
		err = -EIO;
		pr_info("log_store: logstore_getblk is error\n");
	}

out:
	if (bh)
		__brelse(bh);

	return err;
}

int set_emmc_config(int type, int value)
{
	int ret = 0;
	struct log_emmc_header pEmmc;

	if (type >= EMMC_STORE_FLAG_TYPE_NR || type < 0) {
		pr_notice("invalid config type: %d.\n", type);
		return -1;
	}

	if (expdb_logstore->bdev == NULL) {
		ret = get_partition_info();
		if (ret != 0)
			return ret;
	}

	memset(&pEmmc, 0, sizeof(struct log_emmc_header));
	ret = partition_block_rw(expdb_logstore->bdev, 0, expdb_logstore->logindex_offset, 0,
			&pEmmc, sizeof(struct log_emmc_header));
	if (ret == 0) {
		if (pEmmc.sig != LOG_EMMC_SIG) {
			pr_err("%s: header sig error.\n", __func__);
			return -1;
		}

		if (type == UART_LOG || type == PRINTK_RATELIMIT ||
			type == KEDUMP_CTL) {
			if (value)
				pEmmc.reserve_flag[type] = FLAG_ENABLE;
			else
				pEmmc.reserve_flag[type] = FLAG_DISABLE;
		} else {
			pEmmc.reserve_flag[type] = value;
		}

		ret = partition_block_rw(expdb_logstore->bdev, 1, expdb_logstore->logindex_offset, 0,
				&pEmmc, sizeof(struct log_emmc_header));
		if (ret)
			pr_err("%s: write partition is error!\n", __func__);
	}

	pr_info("%s: type %d, value %d.\n", __func__, type, value);
	return ret;
}
EXPORT_SYMBOL_GPL(set_emmc_config);

int read_emmc_config(struct log_emmc_header *log_header)
{
	int ret = 0;

	if (expdb_logstore->bdev == NULL)
		if (get_partition_info() != 0)
			return -EIO;

	if(expdb_logstore->bdev != 0) {
		ret = partition_block_rw(expdb_logstore->bdev, 0, expdb_logstore->logindex_offset, 0,
				log_header, sizeof(struct log_emmc_header));
		if (ret == 0) {
			if (log_header->sig != LOG_EMMC_SIG) {
				pr_err("%s: header sig error.\n", __func__);
				return -1;
			}
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(read_emmc_config);

static int log_store_to_emmc(char *buffer, size_t write_len, u32 log_type)
{
	char *logstore_pos = buffer;
	int ret = -1;
	size_t write_remain = write_len, remaining_space_in_block = 0;
	size_t write_chunk_size = 0;
	sector_t index_offset = 0;
	sector_t write_block_offset = 0;
	sector_t write_offset = 0;
	struct emmc_log log_config;
	struct log_emmc_header pEmmc;

	if (write_remain <= 0) {
		pr_err("the buffer is empty.\n");
		return -1;
	}

	if (expdb_logstore == NULL)
		return -1;

	if (expdb_logstore->bdev == NULL)
		if (get_partition_info() != 0)
			return -EIO;

		memset(&pEmmc, 0, sizeof(struct log_emmc_header));
		ret = partition_block_rw(expdb_logstore->bdev, 0, expdb_logstore->logindex_offset, 0,
				&pEmmc, sizeof(struct log_emmc_header));
		if (ret == 0) {
			if (pEmmc.sig != LOG_EMMC_SIG) {
				pr_err("%s: header sig error.\n", __func__);
				memset(&pEmmc, 0, sizeof(struct log_emmc_header));
				pEmmc.sig = LOG_EMMC_SIG;
			}

			log_config.start = pEmmc.offset;
			write_block_offset = pEmmc.offset / expdb_logstore->block_size;
			write_offset = pEmmc.offset % expdb_logstore->block_size;

			if (write_remain > expdb_logstore->logstore_size/4) {
				write_remain = expdb_logstore->logstore_size/4;
				logstore_pos += (write_remain - expdb_logstore->logstore_size/4);
			}

			while (write_remain > 0) {
				remaining_space_in_block = expdb_logstore->block_size - write_offset;
				if ((pEmmc.offset + remaining_space_in_block) >
					(expdb_logstore->logstore_size - expdb_logstore->block_size)) {
					pEmmc.offset = 0;
					write_block_offset = 0;
					write_offset = 0;
				}
				if (write_block_offset > (expdb_logstore->logstore_size /
						expdb_logstore->block_size - 2)) {
					pEmmc.offset = 0;
					write_block_offset = 0;
					write_offset = 0;
				}

				write_chunk_size = (write_remain > remaining_space_in_block) ? remaining_space_in_block
								: write_remain;
				ret = partition_block_rw(expdb_logstore->bdev, 1,
						expdb_logstore->logstore_offset + write_block_offset, write_offset,
						logstore_pos, write_chunk_size);
				if (ret) {
					pr_err("%s: write log to partition is error!\n", __func__);
					return ret;
				}
				write_remain -= write_chunk_size;
				pEmmc.offset += write_chunk_size;
				write_block_offset += 1;
				write_offset = 0;
				if (write_remain > 0)
					logstore_pos += write_chunk_size;
			}

			if (pEmmc.offset >= (expdb_logstore->logstore_size - expdb_logstore->block_size)) {
				pr_err("%s: emmc offset is exceed logstore size!\n", __func__);
				return 0;
			}

			log_config.end = pEmmc.offset;
			log_config.type = log_type;
			index_offset = sizeof(struct log_emmc_header) +
				(pEmmc.reserve_flag[LOG_INDEX] % HEADER_INDEX_MAX)* sizeof(struct emmc_log);

			if (sram_header->reserve[SRAM_EXPDB_VER] != pEmmc.reserve_flag[EXPDB_SIZE_VER])
				pEmmc.reserve_flag[EXPDB_SIZE_VER] = sram_header->reserve[SRAM_EXPDB_VER];

			pEmmc.reserve_flag[LOG_INDEX] += 1;
			pEmmc.reserve_flag[LOG_INDEX] = pEmmc.reserve_flag[LOG_INDEX] % HEADER_INDEX_MAX;

			ret = partition_block_rw(expdb_logstore->bdev, 1, expdb_logstore->logindex_offset, 0,
					&pEmmc, sizeof(struct log_emmc_header));
			if (ret) {
				pr_err("%s: write log index is error!\n", __func__);
				return ret;
			}

			ret = partition_block_rw(expdb_logstore->bdev, 1, expdb_logstore->logindex_offset, index_offset,
					&log_config, sizeof(struct emmc_log));
			if (ret) {
				pr_err("%s: write log to partition is error!\n", __func__);
				return ret;
			}
			ret = write_len;
		}
	pr_info("write log to partition done!\n");
	return ret;
}

static int get_kernel_log(void)
{
	int ret = -1;
	char *kernel_log = NULL;
	size_t len = 0;
	struct kmsg_dump_iter dumper;

	kernel_log = kmalloc(READ_KERNEL_LOG_SIZE, GFP_KERNEL);
	if (kernel_log == NULL)
		return ret;
	memset((char *)kernel_log, 0, READ_KERNEL_LOG_SIZE);

	kmsg_dump_rewind(&dumper);
	kmsg_dump_get_buffer(&dumper, true, kernel_log, READ_KERNEL_LOG_SIZE, &len);
	pr_debug("get kernel log %zu line.\n", len);

	if (len > 0)
		ret = log_store_to_emmc(kernel_log, READ_KERNEL_LOG_SIZE, LOG_LAST_KERNEL);

	kfree(kernel_log);
	return ret;
}

static int boot_log_write_to_partition(char *buffer, size_t write_len)
{
	int ret = -1;
	sector_t block_num = 0;
	struct log_emmc_header pEmmc;
	u32 block_reserve_size = 0, block_offset = 0, write_pos = 0;
	u32 remaining_write_len = 0, write_chunk_size = 0;

	if (expdb_logstore->bdev == NULL)
		if (get_partition_info() != 0)
			return -EIO;

	if (write_len == 0 || buffer == NULL) {
		pr_err("write boot log is error!\n");
		return 0;
	}

	memset(&pEmmc, 0, sizeof(struct log_emmc_header));
	ret = partition_block_rw(expdb_logstore->bdev, 0, expdb_logstore->logindex_offset, 0,
			&pEmmc, sizeof(struct log_emmc_header));
	if (ret == 0) {
		if (pEmmc.sig != LOG_EMMC_SIG) {
			pr_err("%s: header sig error.\n", __func__);
			memset(&pEmmc, 0, sizeof(struct log_emmc_header));
			pEmmc.sig = LOG_EMMC_SIG;
		}
		block_num = pEmmc.reserve_flag[BOOT_PROF_OFFSET] / expdb_logstore->block_size;
		block_offset = pEmmc.reserve_flag[BOOT_PROF_OFFSET] % expdb_logstore->block_size;
		if (expdb_logstore->bootlog_size <= pEmmc.reserve_flag[BOOT_PROF_OFFSET]) {
			block_num = 0;
			block_offset = 0;
		}

		while (write_pos < write_len) {
			block_reserve_size = expdb_logstore->block_size - block_offset;
			remaining_write_len = write_len - write_pos;
			write_chunk_size = (remaining_write_len > block_reserve_size) ? block_reserve_size
								: remaining_write_len;

			ret = partition_block_rw(expdb_logstore->bdev, 1,
					expdb_logstore->bootlog_offset + block_num,
					block_offset, buffer + write_pos, write_chunk_size);
			if (ret) {
				pr_err("1: write boot prof log is error!\n");
				return ret;
			}

			write_pos += write_chunk_size;
			if (write_chunk_size == block_reserve_size)
				block_num = (block_num + 1) % (expdb_logstore->bootlog_size
							/ expdb_logstore->block_size);
			block_offset = 0;
		}

		if (sram_header->reserve[SRAM_EXPDB_VER] != pEmmc.reserve_flag[EXPDB_SIZE_VER])
			pEmmc.reserve_flag[EXPDB_SIZE_VER] = sram_header->reserve[SRAM_EXPDB_VER];
		pEmmc.reserve_flag[BOOT_PROF_OFFSET] += write_len;
		if (pEmmc.reserve_flag[BOOT_PROF_OFFSET] >= expdb_logstore->bootlog_size )
			pEmmc.reserve_flag[BOOT_PROF_OFFSET] = 0;
		ret = partition_block_rw(expdb_logstore->bdev, 1, expdb_logstore->logindex_offset,
			0, &pEmmc, sizeof(struct log_emmc_header));
		if (ret) {
			pr_err("%s: write log index is error!\n", __func__);
			return ret;
		}
		ret = write_len;
	}

	return ret;
}

static void bootlog_to_partition(void)
{
	u32 write_len = 0;

	if (expdb_logstore->bdev == NULL)
		if (get_partition_info() != 0)
			return;

	if (expdb_logstore->log_offset != expdb_logstore->store_offset) {
		/* ring buffer is full */
		if (expdb_logstore->log_offset < expdb_logstore->store_offset) {
			write_len = BOOT_BUFF_SIZE - expdb_logstore->store_offset;
			if (boot_log_write_to_partition(expdb_logstore->bootbuff + expdb_logstore->store_offset,
					write_len) <= 0) {
				pr_err("update boot log to partition 1 is error!\n");
				return;
			}
			/* clear the buffer and set expdb_logstore->store_offset to 0 */
			memset(expdb_logstore->bootbuff + expdb_logstore->store_offset, 0, write_len);
			expdb_logstore->store_offset = 0;
			write_len = expdb_logstore->log_offset;
			if (boot_log_write_to_partition(expdb_logstore->bootbuff + expdb_logstore->store_offset,
					write_len) <= 0) {
				pr_err("update boot log to partition 2 is error!\n");
				return;
			}
			/* update the expdb_logstore->store_offset and clear the buffer */
			memset(expdb_logstore->bootbuff + expdb_logstore->store_offset, 0, write_len);
			expdb_logstore->store_offset = write_len;
		} else {
			/* ring buffer is not full */
			write_len = expdb_logstore->log_offset - expdb_logstore->store_offset;
			if (write_len > 0) {
				if (boot_log_write_to_partition(expdb_logstore->bootbuff + expdb_logstore->store_offset,
					write_len) <= 0) {
					pr_err("update boot log to partition 3 is error!\n");
					return;
				}
				/* update the expdb_logstore->store_offset and clear the buffer */
				memset(expdb_logstore->bootbuff + expdb_logstore->store_offset, 0, write_len);
				expdb_logstore->store_offset += write_len;
			}
		}

		if (expdb_logstore->store_offset >= BOOT_BUFF_SIZE)
			expdb_logstore->store_offset = 0;
	}
}

static void write_to_logstore(char *str, size_t str_len)
{
	char textbuff[BOOT_LOG_LEN];
	u32 new_str_len = 0;
	u32 reserver_memory =0;
	u64 time_ms_high = sched_clock();
	u64 time_ms_low = 0;

	if (str_len <= 0 || expdb_logstore == NULL || expdb_logstore->bootbuff == NULL)
		return;

	memset(textbuff, 0, sizeof(textbuff));
	time_ms_low = do_div(time_ms_high, 1000000);
	new_str_len = scnprintf(textbuff, sizeof(textbuff), "%10llu.%06llu :%5d-%-16s: %s\n",
				time_ms_high, time_ms_low, current->pid, current->comm, str);

	reserver_memory = BOOT_BUFF_SIZE - expdb_logstore->log_offset;
	if(new_str_len > reserver_memory) {
		memcpy_toio(expdb_logstore->bootbuff + expdb_logstore->log_offset, textbuff, reserver_memory);
		memcpy_toio(expdb_logstore->bootbuff, textbuff + reserver_memory, new_str_len - reserver_memory);
		expdb_logstore->log_offset = new_str_len - reserver_memory;
	} else {
		memcpy_toio(expdb_logstore->bootbuff + expdb_logstore->log_offset, textbuff, new_str_len);
		expdb_logstore->log_offset += new_str_len;
	}
}

static void get_bootprof_dts(void)
{
	char textbuff[100];
	int bf_lk_t = 0, bf_pl_t = 0, bf_logo_t = 0;
	int bf_bl2ext_t = 0, bf_gz_t = 0;
	int bf_tfa_t = 0, bf_sec_os_t = 0;
	struct device_node *node;

	node = of_find_node_by_name(NULL, "bootprof");
	if (node) {
		of_property_read_s32(node, "pl_t", &bf_pl_t);
		of_property_read_s32(node, "lk_t", &bf_lk_t);

		if (of_property_read_s32(node, "logo_t", &bf_logo_t))
			of_property_read_s32(node, "lk_logo_t", &bf_logo_t);
		of_property_read_s32(node, "logo_t", &bf_logo_t);
		of_property_read_s32(node, "bl2_ext_t", &bf_bl2ext_t);
		of_property_read_s32(node, "tfa_t", &bf_tfa_t);
		of_property_read_s32(node, "sec_os_t", &bf_sec_os_t);
		of_property_read_s32(node, "gz_t", &bf_gz_t);

		scnprintf(textbuff, sizeof(textbuff),
			"BOOTPROF: pl=%d, bl2ext=%d,lk=%d, logo=%d, tfa=%d, sec_os=%d, gz=%d",
			bf_pl_t, bf_bl2ext_t, bf_lk_t, bf_logo_t, bf_tfa_t, bf_sec_os_t, bf_gz_t);
		write_to_logstore(textbuff, strnlen(textbuff, sizeof(textbuff)));
	}
}

static int write_expdb_thread_fn(void *data)
{
	char *bootbuff = NULL;

	pr_info("log_store: write_expdb_thread is start!\n");
	if (write_expdb_thread == NULL)
		return -1;

	bootbuff = kzalloc(BOOT_BUFF_SIZE, GFP_KERNEL);
	if (bootbuff == NULL) {
		write_expdb_thread = NULL;
		return -1;
	}

	expdb_logstore->bootbuff = bootbuff;
	get_bootprof_dts();
	register_bootprof_write_log(write_to_logstore);

	while (!kthread_should_stop()) {
		if (wait_event_interruptible_timeout(wait_queue,
			expdb_logstore->log_offset != expdb_logstore->store_offset, 2 * HZ)
			== -ERESTARTSYS)
			break;
		bootlog_to_partition();
	}
	return 0;
}

void close_monitor_thread(void)
{
	if (write_expdb_thread != NULL) {
		register_bootprof_write_log(NULL);
		if (expdb_logstore->log_offset != expdb_logstore->store_offset)
			bootlog_to_partition();
		kthread_stop(write_expdb_thread);
		write_expdb_thread = NULL;
		pr_info("update partition thread is stopped!\n");

		if(expdb_logstore->bootbuff != NULL) {
			kfree(expdb_logstore->bootbuff);
			expdb_logstore->bootbuff = NULL;
		}
	}
}
EXPORT_SYMBOL_GPL(close_monitor_thread);
#endif

bool get_pmic_interface(void)
{
	struct device_node *np;
	struct platform_device *pmic_pdev = NULL;
	unsigned int reg_val = 0;

	if (pmic_addr == 0)
		return false;

	np = of_find_node_by_name(NULL, "pmic");
	if (!np) {
		pr_err("log_store: pmic node not found.\n");
		return false;
	}

	pmic_pdev = of_find_device_by_node(np->child);
	if (!pmic_pdev) {
		pr_err("log_store: pmic child device not found.\n");
		return false;
	}

	/* get regmap */
	map = dev_get_regmap(pmic_pdev->dev.parent, NULL);
	if (!map) {
		pr_err("log_store:pmic regmap not found.\n");
		return false;
	}
	regmap_read(map, pmic_addr, &reg_val);
	pr_info("log_store:read pmic register value 0x%x.\n", reg_val);
	return true;

}
EXPORT_SYMBOL_GPL(get_pmic_interface);

u32 set_pmic_boot_phase(u32 boot_phase)
{
	unsigned int reg_val = 0, ret;

	if (!map) {
		if (get_pmic_interface() == false)
			return -1;
	}
	boot_phase = boot_phase & BOOT_PHASE_MASK;
	ret = regmap_read(map, pmic_addr, &reg_val);
	if (ret == 0) {
		reg_val = reg_val & (BOOT_PHASE_MASK << LAST_BOOT_PHASE_SHIFT);
		reg_val |= boot_phase;
		ret = regmap_write(map, pmic_addr, reg_val);
		pr_info("log_store: write pmic value 0x%x, ret 0x%x.\n", reg_val, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(set_pmic_boot_phase);

u32 get_pmic_boot_phase(void)
{
	unsigned int reg_val = 0, ret;

	if (!map) {
		if (get_pmic_interface() == false)
			return -1;
	}

	ret = regmap_read(map, pmic_addr, &reg_val);

	if (ret == 0) {
		reg_val = (reg_val >> LAST_BOOT_PHASE_SHIFT) & BOOT_PHASE_MASK;
		last_boot_phase = reg_val;
		return reg_val;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(get_pmic_boot_phase);

/* set the flag whether store log to emmc in next boot phase in pl */
void store_log_to_emmc_enable(bool value)
{

	if (!sram_dram_buff) {
		pr_notice("%s: sram dram buff is NULL.\n", __func__);
		return;
	}

	if (value) {
		sram_dram_buff->flag |= NEED_SAVE_TO_EMMC;
	} else {
		sram_dram_buff->flag &= ~NEED_SAVE_TO_EMMC;
		sram_header->reboot_count = 0;
		sram_header->save_to_emmc = 0;
	}

	pr_notice(
		"log_store: sram_dram_buff flag 0x%x, reboot count %d, %d.\n",
		sram_dram_buff->flag, sram_header->reboot_count,
		sram_header->save_to_emmc);
}
EXPORT_SYMBOL_GPL(store_log_to_emmc_enable);


void set_boot_phase(u32 step)
{
#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
	struct log_emmc_header pEmmc;
	int ret = 0;
#endif

	if (sram_header->reserve[SRAM_PMIC_BOOT_PHASE] == FLAG_ENABLE) {
		set_pmic_boot_phase(step);
		if (last_boot_phase == 0)
			get_pmic_boot_phase();
	}

	sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] &= ~BOOT_PHASE_MASK;
	sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] |= step;

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
	if (expdb_logstore == NULL)
		return;

	if (expdb_logstore->bdev == NULL)
		if (get_partition_info() != 0)
			return;

		memset(&pEmmc, 0, sizeof(struct log_emmc_header));
		ret = partition_block_rw(expdb_logstore->bdev, 0, expdb_logstore->logindex_offset,
			0, &pEmmc, sizeof(struct log_emmc_header));
		if (ret == 0) {
			if (pEmmc.sig != LOG_EMMC_SIG) {
				pr_err("%s: header sig error.\n", __func__);
				memset(&pEmmc, 0, sizeof(struct log_emmc_header));
				pEmmc.sig = LOG_EMMC_SIG;
			} else if (last_boot_phase == 0) {
				/* get last boot phase */
				last_boot_phase = (pEmmc.reserve_flag[BOOT_STEP] >>
					LAST_BOOT_PHASE_SHIFT) & BOOT_PHASE_MASK;
			}

			/* clear now boot phase */
			pEmmc.reserve_flag[BOOT_STEP] &= (BOOT_PHASE_MASK <<
				LAST_BOOT_PHASE_SHIFT);
			/* set boot phase */
			pEmmc.reserve_flag[BOOT_STEP] |= (step << NOW_BOOT_PHASE_SHIFT);

			if (sram_header->reserve[SRAM_EXPDB_VER] != pEmmc.reserve_flag[EXPDB_SIZE_VER])
				pEmmc.reserve_flag[EXPDB_SIZE_VER] = sram_header->reserve[SRAM_EXPDB_VER];

			ret = partition_block_rw(expdb_logstore->bdev, 1, expdb_logstore->logindex_offset, 0,
				&pEmmc, sizeof(struct log_emmc_header));
			if (ret)
				pr_err("%s: write log index is error!\n", __func__);
		}

#endif
}
EXPORT_SYMBOL_GPL(set_boot_phase);

u32 get_last_boot_phase(void)
{
	return last_boot_phase;
}
EXPORT_SYMBOL_GPL(get_last_boot_phase);

void log_store_bootup(void)
{
	/* Boot up finish, don't save log to emmc in next boot.*/
	store_log_to_emmc_enable(false);
	set_boot_phase(BOOT_PHASE_ANDROID);
	/* store printk log buff information to DRAM */
	store_printk_buff();
}
EXPORT_SYMBOL_GPL(log_store_bootup);

static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot = PAGE_KERNEL;
	unsigned int i;
	void *vaddr;


	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_writecombine(PAGE_KERNEL);
	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_notice("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}
	return vaddr + offset_in_page(start);
}

static int pl_lk_log_show(struct seq_file *m, void *v)
{
	if (dram_curlog_header == NULL || pbuff == NULL) {
		seq_puts(m, "log buff is null.\n");
		return 0;
	}

	seq_printf(m, "show buff sig 0x%x, size 0x%x,pl size 0x%x, lk size 0x%x, last_boot step 0x%x!\n",
			dram_curlog_header->sig, dram_curlog_header->buff_size,
			dram_curlog_header->sz_pl, dram_curlog_header->sz_lk,
			sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] ?
			sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] : last_boot_phase);

	if (dram_log_store_status == BUFF_READY)
		if (dram_curlog_header->buff_size >= (dram_curlog_header->off_pl
		+ dram_curlog_header->sz_pl
		+ dram_curlog_header->sz_lk))
			seq_write(m, pbuff, dram_curlog_header->off_pl +
				dram_curlog_header->sz_lk + dram_curlog_header->sz_pl);

	return 0;
}


static int pl_lk_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, pl_lk_log_show, inode->i_private);
}

static ssize_t pl_lk_file_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	switch (val) {
	case 0:
		log_store_bootup();
#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
		close_monitor_thread();
#endif
		break;

	default:
		break;
	}

	return cnt;
}

static int logstore_pm_notify(struct notifier_block *notify_block,
	unsigned long mode, void *unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		set_boot_phase(BOOT_PHASE_PRE_SUSPEND);
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		set_boot_phase(BOOT_PHASE_EXIT_RESUME);
		break;
	}
	return 0;
}
static const struct proc_ops pl_lk_file_ops = {
	.proc_open = pl_lk_file_open,
	.proc_write = pl_lk_file_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

struct logstore_tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};
#define NORMAL_BOOT_MODE 0

static unsigned int get_boot_mode_from_dts(void)
{
	struct device_node *np_chosen = NULL;
	struct logstore_tag_bootmode *tag = NULL;

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen) {
		pr_notice("log_store: warning: not find node: '/chosen'\n");

		np_chosen = of_find_node_by_path("/chosen@0");
		if (!np_chosen) {
			pr_notice("log_store: warning: not find node: '/chosen@0'\n");
			return NORMAL_BOOT_MODE;
		}
	}

	tag = (struct logstore_tag_bootmode *)
			of_get_property(np_chosen, "atag,boot", NULL);
	if (!tag) {
		pr_notice("log_store: error: not find tag: 'atag,boot';\n");
		return NORMAL_BOOT_MODE;
	}


	pr_notice("log_store: bootmode: 0x%x boottype: 0x%x.\n",
		tag->bootmode, tag->boottype);

	return tag->bootmode;
}

int logstore_reset(struct notifier_block *nb, unsigned long action, void *data)
{
	if(data == NULL)
		return 0;

	pr_info("reboot reason: %s\n", (char *)data);
	if (sram_header->reboot_count != 0 && !strcmp((char *)data, "shell"))
		store_log_to_emmc_enable(false);
#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
	get_kernel_log();
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(logstore_reset);

static struct notifier_block logstore_reboot_notify = {
	.notifier_call = logstore_reset,
};

int log_store_late_init(void)
{
	static struct notifier_block logstore_pm_nb;
	unsigned int boot_mode;

	logstore_pm_nb.notifier_call = logstore_pm_notify;
	if (register_pm_notifier(&logstore_pm_nb))
		pr_warn("Failed to register PM notifier for logstore\n");

	if (register_reboot_notifier(&logstore_reboot_notify))
		pr_warn("Failed to register reboot notifier for logstore\n");

	set_boot_phase(BOOT_PHASE_KERNEL);
	if (sram_dram_buff == NULL) {
		pr_notice("log_store: sram header DRAM buff is null.\n");
		dram_log_store_status = BUFF_ALLOC_ERROR;
		return -1;
	}

	boot_mode = get_boot_mode_from_dts();

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
	if (boot_mode == NORMAL_BOOT_MODE &&
		sram_header->reserve[SRAM_EXPDB_VER] != FLAG_VERSION_0)	{
		expdb_logstore = kzalloc(sizeof(struct log_store_partition), GFP_KERNEL);
		if (expdb_logstore != NULL) {
			memset(expdb_logstore, 0, sizeof(struct log_store_partition));
			expdb_logstore->bootlog_size = 0x100000;
			expdb_logstore->logstore_size = 0x400000;
			write_expdb_thread = kthread_create(write_expdb_thread_fn, NULL ,"log_store_thread");
			if (write_expdb_thread)
				wake_up_process(write_expdb_thread);
			else
				pr_notice("log_store: create log_store_thread failed.\n");
		} else {
			pr_info("log_store: kzalloc expdb_logstore failed, exit write_expdb_thread.\n");
		}
	}
#endif

#if !IS_ENABLED(CONFIG_MTK_LOG_STORE_ALWAYS_CLEAR_EMMC_FLAG)
	if (boot_mode != NORMAL_BOOT_MODE)
#endif
		store_log_to_emmc_enable(false);

	if (!sram_dram_buff->buf_addr || !sram_dram_buff->buf_size) {
		pr_notice("log_store: DRAM buff is null.\n");
		dram_log_store_status = BUFF_ALLOC_ERROR;
		return -1;
	}
	pr_notice("log store:sram_dram_buff addr 0x%llx, size 0x%x.\n",
		sram_dram_buff->buf_addr, sram_dram_buff->buf_size);

	pbuff = remap_lowmem(sram_dram_buff->buf_addr,
		sram_dram_buff->buf_size);
	pr_notice("[PHY layout]log_store_mem:0x%08llx-0x%08llx (0x%llx)\n",
			(unsigned long long)sram_dram_buff->buf_addr,
			(unsigned long long)sram_dram_buff->buf_addr
			+ sram_dram_buff->buf_size - 1,
			(unsigned long long)sram_dram_buff->buf_size);
	if (!pbuff) {
		pr_notice("log_store: ioremap_wc failed.\n");
		dram_log_store_status = BUFF_ERROR;
		return -1;
	}

/* check buff flag */
	if (dram_curlog_header->sig != LOG_STORE_SIG) {
		pr_notice("log store: log sig: 0x%x.\n",
			dram_curlog_header->sig);
		dram_log_store_status = BUFF_ERROR;
		return 0;
	}

	dram_log_store_status = BUFF_READY;
	pr_notice("buff %p, sig %x size %x pl %x, sz %x lk %x, sz %x p %x, l %x\n",
		pbuff, dram_curlog_header->sig,
		dram_curlog_header->buff_size,
		dram_curlog_header->off_pl, dram_curlog_header->sz_pl,
		dram_curlog_header->off_lk, dram_curlog_header->sz_lk,
		dram_curlog_header->pl_flag, dram_curlog_header->lk_flag);

	entry = proc_create("pl_lk", 0664, NULL, &pl_lk_file_ops);
	if (!entry) {
		pr_notice("log_store: failed to create proc entry\n");
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(log_store_late_init);


/* need mapping virtual address to phy address */
void store_printk_buff(void)
{
	/*
	struct printk_ringbuffer **pprb;
	struct printk_ringbuffer *prb;
	char *buff;
	u32 buff_size;

	if (!sram_dram_buff) {
		pr_notice("log_store: sram_dram_buff is null.\n");
		return;
	}

	pprb = (struct printk_ringbuffer **)aee_log_buf_addr_get();
	if (!pprb || !*pprb)
		return;
	prb = *pprb;

	buff = prb->text_data_ring.data;
	buff_size = (u32)(1 << prb->text_data_ring.size_bits);
	sram_dram_buff->klog_addr = __virt_to_phys_nodebug(buff);
	sram_dram_buff->klog_size = buff_size;
	if (buff_size > expdb_logstore->logstore_size/4)
		sram_dram_buff->klog_size = expdb_logstore->logstore_size/4;

	if (!early_log_disable)
		sram_dram_buff->flag |= BUFF_EARLY_PRINTK;
	pr_notice("log_store printk_buff addr:0x%llx,sz:0x%x,buff-flag:0x%x.\n",
		sram_dram_buff->klog_addr,
		sram_dram_buff->klog_size,
		sram_dram_buff->flag);
	*/
	return;
}
EXPORT_SYMBOL_GPL(store_printk_buff);

void disable_early_log(void)
{
	pr_notice("log_store: %s.\n", __func__);
	early_log_disable = true;
	if (!sram_dram_buff) {
		pr_notice("log_store: sram_dram_buff is null.\n");
		return;
	}

	sram_dram_buff->flag &= ~BUFF_EARLY_PRINTK;
}
EXPORT_SYMBOL_GPL(disable_early_log);

int dt_get_log_store(struct mem_desc_ls *data)
{
	struct mem_desc_ls *sram_ls;
	struct device_node *np_chosen, *np_logstore;

	np_logstore = of_find_node_by_name(NULL, "logstore");
	if (np_logstore) {
		of_property_read_u32(np_logstore, "pmic-register", &pmic_addr);
		pr_notice("log_store: get address 0x%x.\n", pmic_addr);
	} else {
		pr_err("log_store: can't get pmic address.\n");
	}

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen)
		np_chosen = of_find_node_by_path("/chosen@0");

	sram_ls = (struct mem_desc_ls *) of_get_property(np_chosen,
			"log_store", NULL);
	if (sram_ls) {
		pr_notice("log_store:[DT] log_store: 0x%x@0x%x\n",
				sram_ls->addr, sram_ls->size);
		*data = *sram_ls;
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(dt_get_log_store);


void *get_sram_header(void)
{
	if ((sram_header != NULL) && (sram_header->sig == SRAM_HEADER_SIG))
		return sram_header;
	return NULL;
}
EXPORT_SYMBOL_GPL(get_sram_header);

int log_store_sram_init(void)
{
#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
	struct mem_desc_ls sram_ls = { 0 };

	if (dt_get_log_store(&sram_ls))
		pr_info("log_store: get ok, sram addr:0x%x, size:0x%x\n",
				sram_ls.addr, sram_ls.size);
	else
		pr_info("log_store: get fail\n");

	sram_header = ioremap_wc(sram_ls.addr,
		CONFIG_MTK_DRAM_LOG_STORE_SIZE);
	dram_curlog_header = &(sram_header->dram_curlog_header);
#else
	pr_notice("log_store: not Found CONFIG_MTK_DRAM_LOG_STORE!\n");
	return -1;
#endif
	pr_notice("log_store: sram header address 0x%p.\n",
		sram_header);
	if (sram_header->sig != SRAM_HEADER_SIG) {
		pr_notice("log_store: sram header sig 0x%x.\n",
			sram_header->sig);
		sram_log_store_status = BUFF_ERROR;
		sram_header = NULL;
		return -1;
	}

	sram_dram_buff = &(sram_header->dram_buf);
	if (sram_dram_buff->sig != DRAM_HEADER_SIG) {
		pr_notice("log_store: sram header DRAM sig error");
		sram_log_store_status = BUFF_ERROR;
		sram_dram_buff = NULL;
		return -1;
	}

	pr_notice("sig 0x%x flag 0x%x add 0x%llx size 0x%x offsize 0x%x point 0x%x\n",
		sram_dram_buff->sig, sram_dram_buff->flag,
		sram_dram_buff->buf_addr, sram_dram_buff->buf_size,
		sram_dram_buff->buf_offsize, sram_dram_buff->buf_point);
	return 0;
}
EXPORT_SYMBOL_GPL(log_store_sram_init);


/* store log_store information to */
static int __init log_store_early_init(void)
{

	log_store_sram_init();
#ifdef MODULE
	log_store_late_init();
#endif

	return 0;
}

#ifdef MODULE
static void __exit log_store_exit(void)
{
	static struct notifier_block logstore_pm_nb;

	if (entry)
		proc_remove(entry);

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
	close_monitor_thread();
#endif
	logstore_pm_nb.notifier_call = logstore_pm_notify;
	unregister_pm_notifier(&logstore_pm_nb);
	unregister_reboot_notifier(&logstore_reboot_notify);
}

module_init(log_store_early_init);
module_exit(log_store_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek LogStore Driver");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#else
early_initcall(log_store_early_init);
#endif
