/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/minmax.h>
#include <linux/spinlock.h>

#include <tzdev_internal.h>
#include <core/ffa_shmem.h>
#include <core/iw_mem.h>
#include <core/log.h>
#include <tz_common.h>
#include <ffa.h>

static const struct tz_uuid sp_uuid = {0xe0786148, 0xf8e7, 0xe311, {0xbc, 0x5e, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};

static char rx[PAGE_SIZE] __aligned(PAGE_SIZE);
static char tx[PAGE_SIZE] __aligned(PAGE_SIZE);

static DEFINE_SPINLOCK(rxtx_lock);

static unsigned short tzdev_id, sp_id, spmc_id;

static void __tzdev_ffa_smc_call(struct tzdev_smc_data *data)
{
	register unsigned long _r0 __asm__(REGISTERS_NAME "0") = data->args[0];
	register unsigned long _r1 __asm__(REGISTERS_NAME "1") = data->args[1];
	register unsigned long _r2 __asm__(REGISTERS_NAME "2") = data->args[2];
	register unsigned long _r3 __asm__(REGISTERS_NAME "3") = data->args[3];
	register unsigned long _r4 __asm__(REGISTERS_NAME "4") = data->args[4];
	register unsigned long _r5 __asm__(REGISTERS_NAME "5") = data->args[5];
	register unsigned long _r6 __asm__(REGISTERS_NAME "6") = data->args[6];
	register unsigned long _r7 __asm__(REGISTERS_NAME "7") = data->args[7];

	__asm__ __volatile__(ARCH_EXTENSION SMC(0): "+r"(_r0) , "+r" (_r1) , "+r" (_r2),
			"+r" (_r3), "+r" (_r4), "+r" (_r5), "+r" (_r6) , "+r" (_r7) : : "memory", PARAM_REGISTERS);

	data->args[0] = _r0;
	data->args[1] = _r1;
	data->args[2] = _r2;
	data->args[3] = _r3;
	data->args[4] = _r4;
	data->args[5] = _r5;
	data->args[6] = _r6;
	data->args[7] = _r7;
}

static void tzdev_ffa_smc_call(struct tzdev_smc_data *data)
{
	/* Receiver may be preempted. In this case Relayer answers with FFA_INTERRUPT
	 * and Sender uses FFA_RUN to resume a preempted Receiver.*/
	do {
		__tzdev_ffa_smc_call(data);

		if (data->args[0] == FFA_INTERRUPT) {
			data->args[0] = FFA_RUN;
			data->args[2] = 0;
			data->args[3] = 0;
			data->args[4] = 0;
			data->args[5] = 0;
			data->args[6] = 0;
			data->args[7] = 0;
		}
	} while (data->args[0] == FFA_RUN);
}

static int tzdev_ffa_rx_release(void)
{
	struct tzdev_smc_data data;

	data.args[0] = FFA_RX_RELEASE;
	data.args[1] = 0;
	data.args[2] = 0;
	data.args[3] = 0;
	data.args[4] = 0;
	data.args[5] = 0;
	data.args[6] = 0;
	data.args[7] = 0;

	tzdev_ffa_smc_call(&data);

	return data.args[0] == FFA_SUCCESS ? 0 : ffa_generic_error(FFA_GET_ERROR(data));
}

static int tzdev_ffa_map_rx_tx_buffer(void)
{
	struct tzdev_smc_data data;

	data.args[0] = FFA_RXTX_MAP;
#if IS_MODULE(CONFIG_TZDEV)
	data.args[1] = page_to_phys(vmalloc_to_page(tx));
	data.args[2] = page_to_phys(vmalloc_to_page(rx));
#else
	data.args[1] = page_to_phys(virt_to_page(tx));
	data.args[2] = page_to_phys(virt_to_page(rx));
#endif
	data.args[3] = 1;
	data.args[4] = 0;
	data.args[5] = 0;
	data.args[6] = 0;
	data.args[7] = 0;

	tzdev_ffa_smc_call(&data);

	return data.args[0] == FFA_SUCCESS ? 0 : ffa_generic_error(FFA_GET_ERROR(data));
}

unsigned short tzdev_ffa_id(void)
{
	return tzdev_id;
}

unsigned short tzdev_ffa_spmc_id(void)
{
	return spmc_id;
}

unsigned short tzdev_ffa_sp_id(void)
{
	return sp_id;
}

static int tzdev_ffa_get_id(void)
{
	struct tzdev_smc_data data;

	memset(data.args, 0, sizeof(data.args));
	data.args[0] = FFA_ID_GET;

	tzdev_ffa_smc_call(&data);

	if (data.args[0] == FFA_SUCCESS)
		tzdev_id = data.args[2];

	return data.args[0] == FFA_SUCCESS ? 0 : ffa_generic_error(FFA_GET_ERROR(data));
}

static int tzdev_ffa_get_spmc_id(void)
{
	struct tzdev_smc_data data;

	memset(data.args, 0, sizeof(data.args));
	data.args[0] = FFA_SPMC_ID_GET;

	tzdev_ffa_smc_call(&data);

	if (data.args[0] == FFA_SUCCESS)
		spmc_id = data.args[2];

	return data.args[0] == FFA_SUCCESS ? 0 : ffa_generic_error(FFA_GET_ERROR(data));
}

static int tzdev_ffa_get_sp_id(void)
{
	struct tzdev_smc_data data;
	uint32_t *uuid = (uint32_t *)&sp_uuid;

	data.args[0] = FFA_PARTITION_INFO_GET;
	data.args[1] = *uuid++;
	data.args[2] = *uuid++;
	data.args[3] = *uuid++;
	data.args[4] = *uuid;
	data.args[5] = 0; /* return partition information descriptor */
	data.args[6] = 0;
	data.args[7] = 0;

	spin_lock(&rxtx_lock);

	tzdev_ffa_smc_call(&data);

	if (data.args[0] == FFA_SUCCESS)
		sp_id = *(unsigned short *)rx;

	tzdev_ffa_rx_release();

	spin_unlock(&rxtx_lock);

	return data.args[0] == FFA_SUCCESS ? 0 : ffa_generic_error(FFA_GET_ERROR(data));
}

#ifdef CONFIG_FFA_FRAGMENTED_TRANSMISSION
#define offset_to_region_index(offset)							\
	(((offset) - sizeof(struct tz_mem_trans_desc_t)) / sizeof(struct tz_mem_region_desc_t))
#define mem_regions_per_page	(PAGE_SIZE / sizeof(struct tz_mem_region_desc_t))

int tzdev_ffa_mem_share(unsigned int num_pages, struct page **pages, ffa_handle_t *phandle)
{
	struct tzdev_smc_data data;
	struct tz_mem_trans_desc_t *tdesc = (struct tz_mem_trans_desc_t *)tx;
	struct tz_mem_region_desc_t *txreg = (struct tz_mem_region_desc_t *)tx;
	size_t tdesc_size = sizeof(struct tz_mem_trans_desc_t) +
                num_pages * sizeof(struct tz_mem_region_desc_t);
	size_t first_num_pages = min(num_pages, (PAGE_SIZE - sizeof(struct tz_mem_trans_desc_t)) /
		sizeof(struct tz_mem_region_desc_t));
	size_t first_tdesc_size = sizeof(struct tz_mem_trans_desc_t) +
                first_num_pages * sizeof(struct tz_mem_region_desc_t);
	unsigned int first_tx_page, num_tx_pages;
	ffa_handle_t handle;
	unsigned int i, offset = 0;
	int ret = 0;

	spin_lock(&rxtx_lock);

	/* Prepare memory transfer descriptor */
	memset(tdesc, 0, sizeof(struct tz_mem_trans_desc_t));

	tdesc->sender_id = tzdev_id;
	tdesc->attr = FFA_NORMAL_MEM | FFA_WRITE_BACK | FFA_INNER_SHARED;
	tdesc->flags = 0;
	tdesc->handle = 0;
	tdesc->tag = 0;
#ifdef CONFIG_TZDEV_FFA_1_1
	tdesc->acc_desc_size = sizeof(struct tz_ep_acc_desc_t);
	tdesc->acc_desc_offset = offsetof(struct tz_mem_trans_desc_t, ep_desc);
#endif
	tdesc->acc_desc_count = 1;
	tdesc->ep_desc.mem_acc_perm_desc.ep_id = sp_id;
	tdesc->ep_desc.mem_acc_perm_desc.perms = RW;
	tdesc->ep_desc.mem_acc_perm_desc.flags = 0;
	tdesc->ep_desc.comp_mem_desc_offset = offsetof(struct tz_mem_trans_desc_t, mem_regions);

	tdesc->mem_regions.page_count = num_pages;
	tdesc->mem_regions.addr_range_count = num_pages;

	for (i = 0; i < first_num_pages; i++) {
		tdesc->mem_regions.ranges[i].address = page_to_phys(pages[i]);
		tdesc->mem_regions.ranges[i].page_count = 1;
	}

	data.args[0] = FFA_MEM_SHARE;
	data.args[1] = tdesc_size;
	data.args[2] = first_tdesc_size;
	data.args[3] = 0;
	data.args[4] = 0;
	data.args[5] = 0;
	data.args[6] = 0;
	data.args[7] = 0;

	while (1) {
		tzdev_ffa_smc_call(&data);

		switch(data.args[0]) {

		case FFA_MEM_FRAG_RX:
			handle = FFA_GET_HANDLE_FROM_RX(data);
			offset = data.args[3];

			first_tx_page = offset_to_region_index(offset);
			num_tx_pages = min(num_pages - first_tx_page, mem_regions_per_page);

			data.args[0] = FFA_MEM_FRAG_TX;
			data.args[1] = handle & 0xFFFFFFFF;
			data.args[2] = handle >> 32;
			data.args[3] = num_tx_pages * sizeof(struct tz_mem_region_desc_t);
			data.args[4] = id << 16;
			data.args[5] = 0;
			data.args[6] = 0;
			data.args[7] = 0;

			for (i = 0; i < num_tx_pages; i++) {
				treg[i].address = page_to_phys(pages[first_tx_page + i]);
				treg[i].page_count = 1;
			}
			break;

		case FFA_SUCCESS:
			*phandle = FFA_GET_HANDLE(data);
			ret = 0;
			goto done;

		case FFA_ERROR:
			ret = ffa_generic_error(FFA_GET_ERROR(data));
			goto done;
		}
	}

done:
	spin_unlock(&rxtx_lock);
	return ret;
}
#else
int tzdev_ffa_mem_share(unsigned int num_pages, struct page **pages, ffa_handle_t *phandle)
{
	struct tz_mem_trans_desc_t *tdesc = (struct tz_mem_trans_desc_t *)tx;
	struct tzdev_smc_data data;
	unsigned int i, processed, done = 0;
	unsigned int num_handles = 0;

	if (num_pages > MAX_REGIONS_PER_PAGE * FFA_MAX_HANDLES)
		return -E2BIG;

	while (done < num_pages) {
		processed = min((unsigned long)(num_pages - done), MAX_REGIONS_PER_PAGE);

		spin_lock(&rxtx_lock);

		/* Prepare memory transfer descriptor */
		memset(tdesc, 0, sizeof(struct tz_mem_trans_desc_t));

		tdesc->sender_id = tzdev_id;
		tdesc->attr = FFA_NORMAL_MEM | FFA_WRITE_BACK | FFA_INNER_SHARED;
		tdesc->flags = 0;
		tdesc->handle = 0;
		tdesc->tag = 0;
#ifdef CONFIG_TZDEV_FFA_1_1
		tdesc->acc_desc_size = sizeof(struct tz_ep_acc_desc_t);
		tdesc->acc_desc_offset = offsetof(struct tz_mem_trans_desc_t, ep_desc);
#endif
		tdesc->acc_desc_count = 1;
		tdesc->ep_desc.mem_acc_perm_desc.ep_id = sp_id;
		tdesc->ep_desc.mem_acc_perm_desc.perms = RW;
		tdesc->ep_desc.mem_acc_perm_desc.flags = 0;
		tdesc->ep_desc.comp_mem_desc_offset =
			offsetof(struct tz_mem_trans_desc_t, mem_regions);

		tdesc->mem_regions.page_count = processed;
		tdesc->mem_regions.addr_range_count = processed;

		for (i = 0; i < processed; i++) {
			tdesc->mem_regions.ranges[i].address = page_to_phys(pages[done + i]);
			tdesc->mem_regions.ranges[i].page_count = 1;
		}

		data.args[0] = FFA_MEM_SHARE;
		data.args[1] = sizeof(struct tz_mem_trans_desc_t) +
			processed * sizeof(struct tz_mem_region_desc_t);
		data.args[2] = sizeof(struct tz_mem_trans_desc_t) +
			processed * sizeof(struct tz_mem_region_desc_t);
		data.args[3] = 0;
		data.args[4] = 0;
		data.args[5] = 0;
		data.args[6] = 0;
		data.args[7] = 0;

		tzdev_ffa_smc_call(&data);

		spin_unlock(&rxtx_lock);

		if (IS_FFA_ERROR(data)) {
			(*phandle)[num_handles] = FFA_INVALID_HANDLE;
			goto error;
		}

		(*phandle)[num_handles++] = FFA_GET_HANDLE(data);
		done += processed;
	}

	if (num_handles < FFA_MAX_HANDLES)
		(*phandle)[num_handles] = FFA_INVALID_HANDLE;

	return 0;

error:
	tzdev_ffa_mem_reclaim(*phandle);

	return ffa_generic_error(FFA_GET_ERROR(data));
}
#endif /* CONFIG_FFA_FRAGMENTED_TRANSMISSION */

int tzdev_ffa_mem_reclaim(ffa_handle_t handle)
{
	struct tzdev_smc_data data;
#ifndef CONFIG_FFA_FRAGMENTED_TRANSMISSION
	unsigned int i;
	int res = 0;

	for (i = 0; i < FFA_MAX_HANDLES && handle[i] != FFA_INVALID_HANDLE; i++) {
#endif
		memset(&data.args, 0, sizeof(data.args));
		data.args[0] = FFA_MEM_RECLAIM;

#ifdef CONFIG_FFA_FRAGMENTED_TRANSMISSION
		data.args[1] = handle & 0xFFFFFFFF;
		data.args[2] = handle >> 32;
#else
		data.args[1] = handle[i] & 0xFFFFFFFF;
		data.args[2] = handle[i] >> 32;
#endif

		tzdev_ffa_smc_call(&data);

		if (data.args[0] != FFA_SUCCESS)
			res = ffa_generic_error(FFA_GET_ERROR(data));
#ifndef CONFIG_FFA_FRAGMENTED_TRANSMISSION
	}
#endif

	return res;
}

int tzdev_ffa_init(void)
{
	int ret;

	ret = tzdev_ffa_map_rx_tx_buffer();
	if (ret) {
		log_error(tzdev, "tzdev_ffa_map_rx_tx_buffer return %d\n", ret);
		return ret;
	}

	ret = tzdev_ffa_get_id();
	if (ret) {
		log_error(tzdev, "tzdev_ffa_get_id return %d\n", ret);
		return ret;
	}

	ret = tzdev_ffa_get_spmc_id();
	if (ret) {
		log_error(tzdev, "tzdev_ffa_get_spmc_id return %d\n", ret);
		return ret;
	}

	ret = tzdev_ffa_get_sp_id();
	if (ret) {
		log_error(tzdev, "tzdev_ffa_sp_id return %d\n", ret);
		return ret;
	}

	return 0;
}

void tzdev_ffa_fini(void)
{
}