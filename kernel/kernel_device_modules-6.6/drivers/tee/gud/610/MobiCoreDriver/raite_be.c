// SPDX-License-Identifier: GPL-2.0
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

#ifdef CONFIG_RAITE_HYP

#include <linux/version.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/of.h>

#include "main.h"
#include "admin.h"		/* tee_object* */
#include "client.h"		/* Consider other VMs as clients */
#include "mmu.h"
#include "mcp.h"		/* mcp_get_version */
#include "nq.h"
#include "mmu_internal.h"

#include "raite_common.h"
#include "raite_be.h"

static struct {
	int					(*probe)(void);
	int					(*start)(void);
	struct list_head		raite_fes;
} l_ctx;

static void raite_vm_mem_read(struct tee_raite_fe *raite_fe,
			      void *dest, u64 ref)
{
	char *vmsb_vaddr = (char *)raite_fe->vmsb_vaddr;

	/* ref is the offset in our VM shared buffer table of pages */
	memcpy(dest, vmsb_vaddr + ref, PAGE_SIZE);
}

static int raite_vm_mem_verify(struct tee_raite_fe *raite_fe,
			       unsigned long phys_addr)
{
	int i;

	for (i = 0; i < raite_fe->num_region; i++) {
		if (phys_addr > raite_fe->fe_mem_region[i].start &&
		    (phys_addr + PAGE_SIZE) <=
		     raite_fe->fe_mem_region[i].end) {
			return 0;
		}
	}
	return -1;
}

/* Call the FE server */
static void raite_call_fe(struct protocol_fe *pfe,
			  atomic_t call_vm_instance_no)
{
	struct tee_raite_fe *raite_fe =
		container_of(pfe, struct tee_raite_fe, pfe);
	int ret;
	struct raite_answer_buf *buf;
	size_t size = sizeof(struct raite_answer_buf);

	buf = raite_fe->ipc_answer_buf;
	memcpy(&buf->fe2be_data, raite_fe->pfe.fe2be_data,
	       sizeof(struct fe2be_data));

	if (IS_ERR_OR_NULL(raite_fe->ipc_be2fe_fp)) {
		ret = PTR_ERR(raite_fe->ipc_be2fe_fp);
		mc_dev_err(ret, "%s failed, fp err", __func__);
		return;
	}
	ret = raite_ipc_send(raite_fe->ipc_be2fe_fp, buf, size);
	if (ret) {
		mc_dev_err(ret, "Send message to FE failed");
		return;
	}

	//only cmd like TEE_MC_WAIT_DONE assign be2fe_data->cmd
	//that's why we print both fe2be_data->cmd and be2fe_data->cmd
	mc_dev_devel("raite_ipc_send len %zu fe2be/be2fe cmd %02d/%02d id %d",
		     size,
		     raite_fe->pfe.fe2be_data->cmd,
		     raite_fe->pfe.be2fe_data->cmd,
		     raite_fe->pfe.fe2be_data->id);
	raite_fe->pfe.be2fe_data->cmd = TEE_BE_NONE;
	raite_fe->pfe.be2fe_data->id = 0;
}

static int raite_be_start_server(void *arg)
{
	int ret, len;
	loff_t pos = 0;
	u8 *buf = NULL;
	size_t buf_len = 0;
	struct tee_raite_fe *raite_fe;
	int retry = 0;

	if (!arg) {
		mc_dev_err(-ENODEV, "No raite_fe struct found!");
		return -ENODEV;
	}
	raite_fe = (struct tee_raite_fe *)arg;
	buf = (u8 *)raite_fe->ipc_cmd_buf;
	buf_len = sizeof(*raite_fe->ipc_cmd_buf);

	mc_dev_devel("%d", __LINE__);
	ret = raite_ipc_open(raite_fe->ipc_fe2be_path,
			     &raite_fe->ipc_fe2be_fp);
	if (ret)
		return ret;

	mc_dev_info("Waiting for receiving msg from FE...");

	retry = 0;
	while (!kthread_should_stop()) {
		if (retry > RAITE_SERVER_RW_RETRY_MAX) {
			mc_dev_err(-EIO, "IPC read error > %d times",
				   RAITE_SERVER_RW_RETRY_MAX);
			msleep(RAITE_SERVER_OPEN_RETRY_DELAY * 5);
			retry = 0;
		}
		pos = 0;
		retry++;
		raite_fe->ipc_fe2be_fp->f_op->unlocked_ioctl(
				raite_fe->ipc_fe2be_fp,
				IOCTL_IPC_WAIT,
				CMD_IPC_WAIT_A_WRITE);
		raite_fe->ipc_fe2be_fp->f_op->unlocked_ioctl(
				raite_fe->ipc_fe2be_fp,
				IOCTL_IPC_MSG_LEN,
				(unsigned long)&len);
		if (len <= 0) {
			mc_dev_err(0, "ipc ioctl failed %d", len);
			continue;
		} else if (len > buf_len) {
			raite_ipc_flush_buffer(raite_fe->ipc_fe2be_fp, len);
			continue;
		}

		len = kernel_read_local(raite_fe->ipc_fe2be_fp, buf, len, &pos);
		if (len != buf_len) {
			mc_dev_info("read len %d != ipc_cmd_buf %zu (skip it)",
				    len, buf_len);
			continue;
		}
		retry = 0;

		raite_fe->pfe.fe2be_data->otherend_ret =
			protocol_be_dispatch(&raite_fe->pfe);

		mc_dev_devel("FE -> BE result: cmd %u id %u ret %d",
			     raite_fe->pfe.fe2be_data->cmd,
			     raite_fe->pfe.fe2be_data->id,
			     raite_fe->pfe.fe2be_data->otherend_ret);

		if (raite_fe_is_blocking_command(
				raite_fe->pfe.fe2be_data->cmd)) {
			atomic_t vm_instance_no;

			atomic_set(&vm_instance_no, 0);
			/*
			 * Warning: lock required here to avoid be_cmd_worker
			 * and recv thread call call_fe at the same time.
			 * Rely on usual protocol_call_fe() sequence.
			 */
			protocol_get(&raite_fe->pfe);
			raite_call_fe(&raite_fe->pfe, vm_instance_no);
			protocol_put(&raite_fe->pfe);
		}
	}

	return 0;
}

static void raite_be_stop_server(struct tee_raite_fe *raite_fe)
{
	if (raite_fe->ipc_recv_th)
		kthread_stop(raite_fe->ipc_recv_th);

	if (!IS_ERR_OR_NULL(raite_fe->ipc_fe2be_fp))
		filp_close(raite_fe->ipc_fe2be_fp, NULL);

	if (!IS_ERR_OR_NULL(raite_fe->ipc_be2fe_fp))
		filp_close(raite_fe->ipc_be2fe_fp, NULL);
}

int raite_be_start(void)
{
	int ret = 0;
	struct tee_raite_fe *raite_fe;
	int vm_sm_total_page;

	INIT_LIST_HEAD(&l_ctx.raite_fes);

	raite_fe = tee_raite_fe_create();
	if (IS_ERR(raite_fe)) {
		ret = PTR_ERR(raite_fe);
		mc_dev_err(ret, "failed to create raite_fe");
		return ret;
	}
	mc_dev_devel("%d raite_fe %p", __LINE__, raite_fe);
	list_add_tail(&raite_fe->list, &l_ctx.raite_fes);

	raite_fe->vmsb_vaddr = (char *)raite_reserve_memory(&vm_sm_total_page);
	if (IS_ERR_OR_NULL(raite_fe->vmsb_vaddr)) {
		ret = PTR_ERR(raite_fe->vmsb_vaddr);
		mc_dev_err(ret, "failed to get vmsb_vaddr");
		goto err;
	}

	mc_dev_devel("%d raite_fe ipc_cmd_buf %p vmsb_vaddr %p",
		     __LINE__, raite_fe->ipc_cmd_buf, raite_fe->vmsb_vaddr);

	ret = raite_fe_memory_region(raite_fe);
	if (ret) {
		mc_dev_err(ret, "failed to get FE memory region");
		goto err;
	}

	memcpy(raite_fe->ipc_fe2be_path, IPC_FE_TO_BE, sizeof(IPC_FE_TO_BE));
	memcpy(raite_fe->ipc_be2fe_path, IPC_BE_TO_FE, sizeof(IPC_BE_TO_FE));

	raite_fe->ipc_recv_th =
		kthread_run(raite_be_start_server,
			    (void *)raite_fe,
			    "be_ipc_recv_thread");
	if (IS_ERR(raite_fe->ipc_recv_th)) {
		ret = PTR_ERR(raite_fe->ipc_recv_th);
		mc_dev_err(ret, "recv_thread thread creation failed");
		goto err;
	}

	return ret;
err:
	raite_be_stop_server(raite_fe);
	tee_raite_fe_put(raite_fe);
	mc_dev_devel("%d",  __LINE__);
	return ret;
}

static void raite_be_map_delete(struct protocol_be_map *map)
{
	mc_dev_devel("freed raite map %p", map);
	kfree(map);
	atomic_dec(&g_ctx.c_vm_maps);
}

static struct protocol_be_map *raite_be_map_create(
	struct tee_protocol_buffer *buffer,
	struct protocol_fe *pfe)
{
	struct protocol_be_map *map;
	struct tee_raite_fe *raite_fe =
		container_of(pfe, struct tee_raite_fe, pfe);
	/* Contains physical addresses of PTEs table */
	union mmu_table *pmd_table;
	/* Contains physical addresses of pages */
	union mmu_table *pte_tables;
	/* Number of pages and PTEs */
	unsigned long nr_pages =
		PAGE_ALIGN(buffer->offset + buffer->length) / PAGE_SIZE;
	unsigned long nr_pte_refs;
	unsigned long nr_pages_left = nr_pages;
	int ret = 0, i, j;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_vm_maps);

	/* Allocate the mmu and increment debug counter */
	map->mmu = tee_mmu_create_and_init();
	if (IS_ERR(map->mmu)) {
		long ptr_err = PTR_ERR(map->mmu);

		kfree(map);
		atomic_dec(&g_ctx.c_vm_maps);
		return ERR_PTR(ptr_err);
	}

	/* Set references to pmd_table and pte_tables */
	pmd_table = &map->mmu->pmd_table;
	pte_tables = map->mmu->pte_tables;

	/* Deduce the number of PTEs and the number of pages */
	nr_pte_refs = (nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX;

	mc_dev_devel("number of PTEs %lx", nr_pte_refs);
	mc_dev_devel("number of pages %lx", nr_pages);

	/* Create PMD table that contains physical PTE addresses */
	pmd_table->page = get_zeroed_page(GFP_KERNEL);
	if (!pmd_table->page) {
		ret = -ENOMEM;
		mc_dev_err(ret, "get_zeroed_page of pmd_table failed");
		goto out;
	}
	map->mmu->pages_created++;

	/*
	 * Read the shared PMD page into pmd_table that contains physical PTE
	 * addresses
	 */
	raite_vm_mem_read(raite_fe, pmd_table->addr, buffer->pmd_ref);

	/* Loop over the number of PTEs */
	for (i = 0; i < nr_pte_refs; i++) {
		nr_pages = nr_pages_left;
		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		/* Create PTE tables that contains physical page addresses */
		pte_tables[i].page = get_zeroed_page(GFP_KERNEL);
		if (!pte_tables[i].page) {
			mc_dev_err(ret,
				   "get_zeroed_page of pte_tables failed");
			goto out;
		}
		map->mmu->pages_created++;
		map->mmu->nr_pmd_entries++;

		/*
		 * Read the shared PTE page into pte_tables that contains
		 * physical page addresses
		 */
		raite_vm_mem_read(raite_fe, pte_tables[i].addr,
				  pmd_table->entries[i]);

		/* Store local pte_tables */
		pmd_table->entries[i] = virt_to_phys(pte_tables[i].addr);

		/* Loop over the number of pages */
		for (j = 0; j < nr_pages; j++) {
			/* Verify the page */
			ret = raite_vm_mem_verify(raite_fe,
						  pte_tables[i].entries[j]);
			if (ret < 0) {
				mc_dev_err(ret, "raite_vm_mem_verify failed");
				goto out;
			}

			nr_pages_left--;
			mc_dev_devel(
				"verified PTE %d PAGE %d ipa %llx, left %ld",
				i, j, pte_tables[i].entries[j], nr_pages_left);
		}
	}
	/* Update the map */
	map->mmu->offset = buffer->offset;
	map->mmu->length = buffer->length;
	map->mmu->flags = buffer->flags;

	/* Auto-delete */
	map->deleter.object = map;
	map->deleter.delete = (void(*)(void *))raite_be_map_delete;
	map->mmu->deleter = &map->deleter;

	mc_dev_devel("verified map %p: nr pages %lu, nr pte %zu",
		     map, map->mmu->nr_pages, map->mmu->nr_pmd_entries);

	return map;
out:
	raite_be_map_delete(map);
	return NULL;
}

struct tee_mmu *raite_be_set_mmu(struct protocol_be_map *map,
				 struct mcp_buffer_map b_map)
{
	return map->mmu;
}

static int raite_create_be_client(struct protocol_fe *pfe)
{
	struct tee_raite_fe *raite_fe =
		container_of(pfe, struct tee_raite_fe, pfe);
	const char *server_name = raite_fe->pfe.fe2be_data->server_name;
	int ret;
	int retry = 0;

	if (raite_fe->client_is_open) {
		tee_raite_fe_cleanup(raite_fe);
		raite_fe->client_is_open = false;
	}

	/* verify FE identity is different than BE to prevent any attack */
	if (strcmp(RAITE_VM_ID_BE, server_name) == 0) {
		ret = -EINVAL;
		mc_dev_err(ret, "invalid FE name %s", server_name);
		return ret;
	}

	while (IS_ERR_OR_NULL(raite_fe->ipc_be2fe_fp) && (retry < 60)) {
		raite_fe->ipc_be2fe_fp =
			filp_open(raite_fe->ipc_be2fe_path, O_RDWR, 0);
		if (IS_ERR(raite_fe->ipc_be2fe_fp)) {
			if (retry >= RAITE_SERVER_OPEN_RETRY_MAX) {
				ret = PTR_ERR(raite_fe->ipc_be2fe_fp);
				mc_dev_err(ret, "open %s failed retry %d",
					   raite_fe->ipc_be2fe_path, retry);
				return ret;
			}
		}
		msleep(RAITE_SERVER_OPEN_RETRY_DELAY);
		retry++;
	}

	raite_fe->pfe.client = client_create(true, server_name);
	if (!raite_fe->pfe.client) {
		ret = -ENODEV;
		mc_dev_err(ret, "Failed to create FE client");
		return ret;
	}

	raite_fe->client_is_open = true;
	return 0;
}

static struct tee_protocol_be_call_ops be_call_ops = {
	.call_fe = raite_call_fe,
	.be_map_create = raite_be_map_create,
	.be_map_delete = raite_be_map_delete,
	.be_set_mmu = raite_be_set_mmu,
	.be_create_client = raite_create_be_client,
};

static struct tee_protocol_ops protocol_ops = {
	.name = "RAITE BE",
	.start = raite_be_start,
	.be_call_ops = &be_call_ops,
};

struct tee_protocol_ops *raite_be_check(void)
{
#ifdef MC_FORCE_TO_BE
	mc_dev_info("MC_FORCE_TO_BE");
	protocol_ops.vm_id = RAITE_VM_ID_BE;
	return &protocol_ops;
#else
	struct tee_protocol_ops *ops;
	struct device_node *np;
	u32 val;

	np = of_find_compatible_node(NULL, NULL, MC_DEVICE_PROPNAME);
	if (!np) {
		mc_dev_err(0, "can't find compatible node for %s",
			   MC_DEVICE_PROPNAME);
		return NULL;
	}

	if (of_property_read_u32(np, MC_RAITE_BE_PROPNAME, &val)) {
		mc_dev_info("missing %s property", MC_RAITE_BE_PROPNAME);
		ops = NULL;
	} else {
		/* Get the vm number to detect if this is a RAITE BE */
		protocol_ops.vm_id = RAITE_VM_ID_BE;
		ops = (struct tee_protocol_ops *)&protocol_ops;
	}
	of_node_put(np);

	return ops;
#endif
}

#endif /* CONFIG_RAITE_HYP */
