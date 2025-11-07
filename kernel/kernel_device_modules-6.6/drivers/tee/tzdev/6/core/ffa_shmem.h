/*
 * Copyright (C) 2023, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TZ_FFA_SHMEM_H__
#define __TZ_FFA_SHMEM_H__

#define MAX_REGIONS_PER_PAGE	(PAGE_SIZE - sizeof(struct tz_mem_trans_desc_t)) / \
		sizeof(struct tz_mem_region_desc_t)

#define FFA_INVALID_HANDLE			((uint64_t)-1)

#ifdef CONFIG_FFA_FRAGMENTED_TRANSMISSION
typedef uint64_t ffa_handle_t;
#define FFA_IS_INVALID_HANDLE(handle)	((handle) == FFA_INVALID_HANDLE)
#define FFA_INIT_HANDLE(handle)		(handle) = FFA_INVALID_HANDLE
#define FFA_HANDLE_TO_UINT(h)		(h)
#define FFA_ULONG_TO_HANDLE(l)		(l)
#else
#define FFA_MAX_HANDLES	16
typedef uint64_t ffa_handle_t[FFA_MAX_HANDLES];
#define FFA_INIT_HANDLE(handle)		(handle)[0] = FFA_INVALID_HANDLE
#define FFA_HANDLE_TO_UINT(h)		(h)[0]
#define FFA_ULONG_TO_HANDLE(l)		{(l), FFA_INVALID_HANDLE}
#endif /* CONFIG_FFA_FRAGMENTED_TRANSMISSION */

#define FFA_GET_HANDLE(smc_data)		((unsigned long)smc_data.args[3] << 32 | smc_data.args[2])
#define FFA_GET_HANDLE_FROM_RX(smc_data)	((unsigned long)smc_data.args[2] << 32 | smc_data.args[1])

/* Memory region attributes bits */
#define FFA_INNER_SHARED	(3 << 0)
#define FFA_OUTER_SHARED	(2 << 0)
#define FFA_WRITE_BACK	(3 << 2)
#define FFA_NON_CACHE	(1 << 2)
#define FFA_NORMAL_MEM	(2 << 4)
#define FFA_DEVICE_MEM	(1 << 4)
#define FFA_NON_SECURE_MEM	(1 << 6)

/* Flags bits */
#define ZERO_MEM		(1 << 0)
#define TIMESLICE_MEM_OP	(1 << 1)

/* Memory access permissions bits */
#define RO		(1 << 0)
#define RW		(2 << 0)
#define NO_EXEC		(1 << 2)
#define EXEC		(2 << 2)

/* Memory access permissions descriptor */
struct tz_mem_acc_perm_desc_t {
	uint16_t ep_id;
	uint8_t perms;
	uint8_t flags;
} __attribute__((packed));

/* Constituent memory region descriptor */
struct tz_mem_region_desc_t {
	uint64_t address;
	uint32_t page_count;
	uint32_t reserved;
} __attribute__((packed));

/* Composite memory region descriptor */
struct tz_composite_mem_region_desc_t {
	uint32_t page_count;
	uint32_t addr_range_count;
	uint64_t reserved;
	struct tz_mem_region_desc_t ranges[];
} __attribute__((packed));

/* Endpoint memory access descriptor */
struct tz_ep_acc_desc_t {
	struct tz_mem_acc_perm_desc_t mem_acc_perm_desc;
	uint32_t comp_mem_desc_offset;
	uint64_t reserved;
} __attribute__((packed));

#ifdef CONFIG_TZDEV_FFA_1_1
/* Memory transaction descriptor */
struct tz_mem_trans_desc_t {
	uint16_t sender_id;
	uint16_t attr;
	uint32_t flags;
	uint64_t handle;
	uint64_t tag;
	uint32_t acc_desc_size;
	uint32_t acc_desc_count;
	uint32_t acc_desc_offset;
	uint8_t reserved[12];
	struct tz_ep_acc_desc_t ep_desc;
	struct tz_composite_mem_region_desc_t mem_regions;
} __attribute__((packed));
#else
struct tz_mem_trans_desc_t {
	uint16_t sender_id;
	uint8_t attr;
	uint8_t reserved_0; /* MBZ */
	uint32_t flags;
	uint64_t handle;
	uint64_t tag;
	uint32_t reserved_1; /* MBZ */
	uint32_t acc_desc_count;
	struct tz_ep_acc_desc_t ep_desc;
	struct tz_composite_mem_region_desc_t mem_regions;
} __attribute__((packed));
#endif /* CONFIG_TZDEV_FFA_1_1 */

#endif /* __TZ_FFA_SHMEM_H__ */
