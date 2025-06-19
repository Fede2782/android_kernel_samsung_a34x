/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef GPU_PDMA_H
#define GPU_PDMA_H

#define PDMA_RINGBUF_PA_NUM		8
/**
 * @in: Input parameters
 * @in.kctx_id: Context ID that locks HW.
 * @in.mode: Switch default policy by mode.
 * @out: Output parameters
 * @out.status: 0: Fail to lock HW. 1: Success to lock HW.
 * @out.base: PA of PDMA Reg base.
 * @out.ringbuf: PA of ring buffer base
 * @out.size: Size of ring buffer (bytes)
 * @out.hw_sem_base: PA of hw semaphore base
 * @out.hw_sem_offset: Offset from hw semaphore base.
 * @out.gid_list_discardable: available discardable GID list for user
 *    with bit-mask expression. e.q. 0xE mean GID 1 to 3 is available.
 *		Support up to 64 GIDs and share with non-discardable list.
 * @out.gid_list_non_disc: available non-discardable GID list for user
 *    with bit-mask expression. Mutally exclusive with gid_list_discardable.
 * @out.sw_ver: Indicate sw version by kernel driver
 */

struct pdma_hw_lock {
	struct {
		unsigned int kctx_id;
		unsigned int mode;
	} in;
	struct {
		unsigned int status;
		unsigned long base;
		unsigned int region_size;
		unsigned long ringbuf;
		unsigned int size;
		unsigned long hw_sem_base;
		unsigned int hw_sem_offset;
		unsigned long long gid_list_discardable;
		unsigned long long gid_list_non_disc;
		unsigned int sw_ver;
	} out;
};

/**
 * @in: Input parameters
 * @in.kctx_id: Context ID that locks HW.
 * @in.hwptr: Write pointer updated to HW.
 * @out: Output parameters
 * @out.hrptr: Read pointer read from HW.
 */

struct pdma_rw_ptr {
	struct {
		unsigned int kctx_id;
		unsigned int hwptr;
	} in;
	struct {
		unsigned int hrptr;
	} out;
};

struct pdma_sram {
	unsigned int ccmd_hw_reset;
	unsigned int interrupt_status;    /* [0]:enable irq [1]:interrupt status(from eb) */
	unsigned int ringbuf[PDMA_RINGBUF_PA_NUM];
};

/* IOCTL cmd */
#define PDMA_IOCTL_TYPE 0x80

#define GPU_PDMA_LOCKHW					_IOWR(PDMA_IOCTL_TYPE, 0x1, struct pdma_hw_lock)
#define GPU_PDMA_UNLOCKHW				_IOWR(PDMA_IOCTL_TYPE, 0x2, struct pdma_hw_lock)
#define GPU_PDMA_WRITE_HWPTR			_IOWR(PDMA_IOCTL_TYPE, 0x3, struct pdma_rw_ptr)
#define GPU_PDMA_READ_HRPTR			_IOWR(PDMA_IOCTL_TYPE, 0x4, struct pdma_rw_ptr)

/* Export function */
void pdma_lock_reclaim(u32 kctx_id);

#endif /* GPU_PDMA_H */
