/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MC_RAITE_COMMON_H
#define MC_RAITE_COMMON_H

#include <linux/list.h>
#include <linux/workqueue.h>

#include "public/mc_user.h"	/* many types */
#include "protocol_common.h"

#define MC_RAITE_BE_PROPNAME			"trustonic,raite-be"
#define RAITE_VM_SHARED_MEM_COMPATIBLE		"raite,vm_shared_mem"
#define RAITE_RESERVED_MEM_NODE_NAME		"reserved-memory"
/*
 * Device tree property dyncamically created at runtime by RAITE
 * hypervisor with actual Guest VM phys addr range
 */
#define RAITE_FE_RESERVED_MEM_PREFIX		"guest-ram"

#define BROOK_IOC_MAGIC     'H'

#define IOCTL_IPC_WAIT					\
	_IOC(_IOC_NONE, BROOK_IOC_MAGIC, 2, sizeof(u32))
#define IOCTL_IPC_MSG_LEN					\
	_IOC(_IOC_NONE, BROOK_IOC_MAGIC, 3, sizeof(u64))
#define IOCTL_IPC_WAITTIMEOUT      \
	_IOC(_IOC_NONE, BROOK_IOC_MAGIC, 5, sizeof(struct wait_timeout))

/* if no IPC_WAITTIMEOUT, type of expected IPC must be specified */
#define CMD_IPC_WAIT_A_READ  0
#define CMD_IPC_WAIT_A_WRITE 1

#define MC_RAITE_IPC_PATH_SIZE 32
#define IPC_BE_TO_FE        "/dev/raite_ipc_66"
#define IPC_FE_TO_BE        "/dev/raite_ipc_67"

/* Retry/delay mechanism to wait RAITE server node.
 * (Overall 30sec delay before exit)
 */
#define RAITE_SERVER_OPEN_RETRY_MAX    60
#define RAITE_SERVER_OPEN_RETRY_DELAY  500
#define RAITE_SERVER_RW_RETRY_MAX      10

//#define RAITE_IPC_PMD_ENTRIES_MAX        PMD_ENTRIES_MAX
//ipc_dev 6500000.ipc: init ipc id = 66  sz 524288
//	e728000006500000 to c663b610 success
//ipc_dev 6500000.ipc: virt1 e7280000 size1 262144 success
//ipc_dev 6500000.ipc: virt2 e72c0000 size2 262144 success

#define RAITE_VM_ID_BE                       "RAITE BE(SEE)"
#define RAITE_VM_ID_FE                       "RAITE FE(NEE)"

#define RAITE_IPC_SEND_TIMEOUT        10000000

struct mem_desc_t {
	unsigned long start;
	unsigned long end;
	unsigned long size;
};

// read: 0 if write; timeout: us
struct wait_timeout {
	u32		read;
	int		timeout;
};

struct tee_raite_fe {
	struct protocol_fe		pfe;
	struct kref			kref;
	struct list_head		list;

	char				ipc_be2fe_path[MC_RAITE_IPC_PATH_SIZE];
	char				ipc_fe2be_path[MC_RAITE_IPC_PATH_SIZE];
	struct file			*ipc_be2fe_fp;
	struct file			*ipc_fe2be_fp;
	struct task_struct		*ipc_recv_th;

	struct raite_answer_buf		*ipc_answer_buf;
	struct raite_cmd_buf		*ipc_cmd_buf;

	int				num_region;
	struct mem_desc_t		*fe_mem_region;
	int				client_is_open;
	/* VM shared buffer virtual address */
	void				*vmsb_vaddr;
};

/* data structure for commands from the FE */
struct raite_cmd_buf {
	struct  fe2be_data	fe2be_data;
};

/*
 * Data structure for cmd and answers from the BE.
 * In BE answer case:
 * - Synchronous cmd return values in cmd buffer (so fe2be channel)
 * - Asynchronous answers provide values in be2fe_data channel.
 * In FE cmd case:
 * - Only fe2be channel is used.
 */
struct raite_answer_buf {
	struct  be2fe_data	be2fe_data;
	struct  fe2be_data	fe2be_data;
};

int raite_fe_is_blocking_command(int cmd);
struct tee_raite_fe *tee_raite_fe_create(void);
void tee_raite_fe_put(struct tee_raite_fe *raite_fe);
void tee_raite_fe_cleanup(struct tee_raite_fe *raite_fe);

int raite_ipc_open(char *path, struct file **file);
int raite_ipc_send(struct file *file, void *buf, size_t buf_len);
void raite_ipc_flush_buffer(struct file *file, size_t len);

void *raite_reserve_memory(int *total_pages);
int raite_fe_memory_region(struct tee_raite_fe *raite_fe);

static inline int kernel_read_local(struct file *file,
				    void *buf, size_t buf_len,
				    loff_t *pos)
{
#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
	return kernel_read(file, *pos, buf, buf_len);
#else
	return kernel_read(file, buf, buf_len, pos);
#endif
}

static inline int kernel_write_local(struct file *file,
				     void *buf, size_t buf_len,
				     loff_t pos)
{
#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
		return kernel_write(file, buf, buf_len, pos);
#else
		return kernel_write(file, buf, buf_len, &pos);
#endif
}

#endif /* MC_RAITE_COMMON_H */

