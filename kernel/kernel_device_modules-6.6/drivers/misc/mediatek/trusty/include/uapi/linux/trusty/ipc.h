/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _UAPI_LINUX_TRUSTY_IPC_H_
#define _UAPI_LINUX_TRUSTY_IPC_H_

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/uio.h>

/**
 * struct trusty_shm - Describes a transfer of memory to Trusty
 * @fd:       The fd to transfer
 * @transfer: Data size
 */
struct trusty_shdm {
	__s32 fd;
	__u64 size;
};

/**
 * struct tipc_send_msg_req - Request struct for @TIPC_IOC_SEND_MSG
 * @iov:     Pointer to an array of &struct iovec describing data to be sent
 * @shm:     Pointer to an array of &struct trusty_shm describing any file
 *           descriptors to be transferred.
 * @iov_cnt: Number of elements in the @iov array
 * @shm_cnt: Number of elements in the @shm array
 */
struct tipc_send_msg_req {
	__u64 iov;
	__u64 shm;
	__u64 iov_cnt;
	__u64 shm_cnt;
};

#define TIPC_IOC_MAGIC			'r'
#define TIPC_IOC_CONNECT		_IOW(TIPC_IOC_MAGIC, 0x80, char *)
#define TIPC_IOC_SEND_MSG		_IOW(TIPC_IOC_MAGIC, 0x81, \
					struct tipc_send_msg_req)

#endif
