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

#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>

#include "mci/mciiwp.h"		/* struct interworld_session */
#include "main.h"
#include "client.h"
#include "iwp.h"
#include "mcp.h"
#include "raite_common.h"
#include "raite_fe.h"
#include "mmu_internal.h"

static struct {
	int				(*start)(void);
	struct tee_raite_fe		*raite_fe;
	//VM shared memory total page
	int				vmsb_total_page;
	int				vmsb_free_pages[PMD_ENTRIES_MAX];
	/* Current index into the vmsb_free_pages tab */
	int				vmsb_index;
	struct completion		other_end_complete;
} l_ctx;

struct raite_fe_map {
	struct protocol_fe_map		map;
	/* Contains phys @ of ptes tables */
	union mmu_table			pmd_table;
	/* Array of pages that hold buffer ptes*/
	union mmu_table			pte_tables[PMD_ENTRIES_MAX];
	/* Actual number of ptes tables */
	size_t				nr_pmd_entries;
	int				pmd_ptes_granted;
};

#if KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE
static inline void reinit_completion_local(struct completion *x)
{
	reinit_completion(x);
}
#else
static inline void reinit_completion_local(struct completion *x)
{
	INIT_COMPLETION(*x);
}
#endif

/* Raite VM shared buffer init */
static int vmsb_init(struct tee_raite_fe *raite_fe)
{
	int i;
	void *base = raite_reserve_memory(&l_ctx.vmsb_total_page);

	if (IS_ERR_OR_NULL(base))
		return PTR_ERR(base);

	raite_fe->vmsb_vaddr = base;
	if (l_ctx.vmsb_total_page > PMD_ENTRIES_MAX)
		l_ctx.vmsb_total_page = PMD_ENTRIES_MAX;

	mc_dev_devel("vmsb_vaddr %p %d pages",
		     raite_fe->vmsb_vaddr, l_ctx.vmsb_total_page);

	for (i = 0; i < l_ctx.vmsb_total_page; i++)
		l_ctx.vmsb_free_pages[i] = i;

	l_ctx.vmsb_index = 0;

	return 0;
}

static void *vmsb_alloc_page(void)
{
	/* VMSB Virtual Base Address */
	char *base = (char *)l_ctx.raite_fe->vmsb_vaddr;
	/* Page Number of the current page */
	int page_no = l_ctx.vmsb_free_pages[l_ctx.vmsb_index];
	/* Virtual Address of the page */
	void *virt_addr = base + (page_no * PAGE_SIZE);

	/* Check the index */
	if (l_ctx.vmsb_index >= l_ctx.vmsb_total_page)
		return NULL;

	l_ctx.vmsb_index++;

	/* Cleanup the pointed page */
	memset(virt_addr, 0x0, PAGE_SIZE);

	return virt_addr;
}

/*
 * Memory Manager:
 * Free the page at the virtual address in arg
 */
static void vmsb_free_page(void *virt_addr)
{
	/* VMSB Virtual Base Address */
	char *base = (char *)l_ctx.raite_fe->vmsb_vaddr;
	/* Page Number of the current page */
	int page_no = ((char *)virt_addr - base) / PAGE_SIZE;

	/* Check the index */
	WARN_ON(!l_ctx.vmsb_index);

	l_ctx.vmsb_index--;

	l_ctx.vmsb_free_pages[l_ctx.vmsb_index] = page_no;
}

/*
 * Memory Manager:
 * Convert the virtual address in arg to an offset
 */
static u64 vmsb_virt_to_offset(void *virt_addr)
{
	char *v_addr = (char *)virt_addr;
	char *base = (char *)l_ctx.raite_fe->vmsb_vaddr;

	return v_addr - base;
}

/* Maps create, release and delete */
static void raite_fe_map_release_pmd_ptes(
	struct protocol_fe_map *map,
	const struct tee_protocol_buffer *buffer)
{
	int i;
	struct raite_fe_map *raite_map =
		container_of(map, struct raite_fe_map, map);
	struct mcp_buffer_map b_map;
	/* Contains physical addresses of PTEs table */
	union mmu_table *pmd_table;
	/* Contains physical addresses of pages */
	union mmu_table *pte_tables;
	/* Number of PTEs */
	unsigned long nr_pte_tables;

	/* Verify if at least PMD or one PTE is granted */
	if (!raite_map->pmd_ptes_granted)
		return;

	raite_map->pmd_ptes_granted = false;

	/* Get pmd_table and pte_tables from the mmu */
	pmd_table = &raite_map->map.mmu->pmd_table;
	pte_tables = raite_map->map.mmu->pte_tables;

	if (!raite_map->map.refs_shared)
		return;

	/* Convert raite_map->map.mmu (tee_mmu) into b_map (mcp_buffer_map) */
	tee_mmu_buffer(raite_map->map.mmu, &b_map);

	vmsb_free_page(raite_map->pmd_table.addr);
	mc_dev_devel("released VMSB PMD ipa %p", raite_map->pmd_table.addr);
	raite_map->pmd_table.addr = NULL;

	raite_map->map.refs_shared--;

	/* Deduce the number of PTEs */
	nr_pte_tables = (b_map.mmu->nr_pages + PTE_ENTRIES_MAX - 1) /
		PTE_ENTRIES_MAX;

	/* Loop over the number of PTEs */
	for (i = 0; i < nr_pte_tables; i++) {
		if (!raite_map->map.refs_shared)
			break;

		vmsb_free_page(raite_map->pte_tables[i].addr);
		mc_dev_devel("released VMSB PMD ipa %p",
			     raite_map->pte_tables[i].addr);
		raite_map->pte_tables[i].addr = NULL;

		raite_map->map.refs_shared--;
	}
}

static void raite_fe_map_release(struct protocol_fe_map *map)
{
	/* Release PMD, PTEs and then pages */
	raite_fe_map_release_pmd_ptes(map, NULL);

	/* Check memory leak */
	if (map->refs_shared)
		mc_dev_err(-EUCLEAN,
			   "leak detected: still granted %d",
			   map->refs_shared);

	mc_dev_devel("freed map %p: nr pages %lu, nr PTE %zu",
		     map, map->mmu->nr_pages, map->mmu->nr_pmd_entries);

	kfree(map);
	atomic_dec(&g_ctx.c_vm_maps);
}

static void raite_fe_map_delete(void *arg)
{
	struct protocol_fe_map *map = arg;

	raite_fe_map_release(map);
}

static struct protocol_fe_map *raite_fe_map_create(
	struct tee_protocol_buffer *buffer,
	const struct mcp_buffer_map *b_map,
	struct protocol_fe *pfe)
{
	struct raite_fe_map *raite_map;
	/* Contains physical addresses of PTEs table */
	union mmu_table *pmd_table;
	/* Contains physical addresses of pages */
	union mmu_table *pte_tables;
	/* Deduce the number of PTEs and the number of pages */
	unsigned long nr_pte_refs =
		(b_map->mmu->nr_pages + PTE_ENTRIES_MAX - 1) / PTE_ENTRIES_MAX;
	int ret, i;

	mc_dev_devel("%d raite_map size %zu",
		     __LINE__, sizeof(*raite_map));
	raite_map = kzalloc(sizeof(*raite_map), GFP_KERNEL);
	if (!raite_map)
		return ERR_PTR(-ENOMEM);
	atomic_inc(&g_ctx.c_vm_maps);

	mc_dev_devel("number of PTEs %lx", nr_pte_refs);
	mc_dev_devel("number of pages %lx", b_map->mmu->nr_pages);

	/* b_map describes the PMD which contains pointers to PTE tables */
	pmd_table = &b_map->mmu->pmd_table;
	pte_tables = b_map->mmu->pte_tables;

	/* Update raite_map (protocol_fe_map) */
	raite_map->pmd_ptes_granted = true;
	raite_map->map.mmu = b_map->mmu;

	raite_map->pmd_table.addr = vmsb_alloc_page();
	if (!raite_map->pmd_table.addr) {
		ret = -ENOMEM;
		goto err_mem_grant;
	}
	mc_dev_devel("allocated VMSB PMD ipa %p", raite_map->pmd_table.addr);
	/* Increment the count of shared pages */
	raite_map->map.refs_shared++;

	/* Loop over the number of PTEs */
	for (i = 0; i < nr_pte_refs; i++) {
		raite_map->pte_tables[i].addr = vmsb_alloc_page();
		if (!raite_map->pte_tables[i].addr) {
			ret = -ENOMEM;
			goto err_mem_grant;
		}

		memcpy(raite_map->pte_tables[i].addr, pte_tables[i].addr,
		       PAGE_SIZE);
		raite_map->pmd_table.entries[i] =
			vmsb_virt_to_offset(raite_map->pte_tables[i].addr);
		mc_dev_devel("allocated VMSB PTE ipa %p",
			     raite_map->pte_tables[i].addr);

		/* Increment the count of shared pages */
		raite_map->map.refs_shared++;
	}

	/* Update the buffer (tee_protocol_buffer) with: */
	buffer->pmd_ref = vmsb_virt_to_offset(raite_map->pmd_table.addr);
	buffer->addr = (uintptr_t)b_map->mmu;	/* MMU */
	buffer->offset = b_map->offset;
	buffer->length = b_map->length;
	buffer->flags = b_map->flags;

	/* Auto-delete */
	raite_map->map.deleter.object = &raite_map->map;
	raite_map->map.deleter.delete = raite_fe_map_delete;
	tee_mmu_set_deleter(b_map->mmu, &raite_map->map.deleter);

	mc_dev_devel("granted map %p: nr pages %lu, nr pte %zu",
		     &raite_map->map, raite_map->map.mmu->nr_pages,
		     raite_map->map.mmu->nr_pmd_entries);

	return &raite_map->map;

err_mem_grant:
	raite_fe_map_release(&raite_map->map);
	return ERR_PTR(-ENOMEM);
}

/*
 * Called from:
 * - protocol_call_be()
 *   Can be called from several parallel threads contexts, however
 *   all the calls to raite_call_be() are protected with a lock in
 *   protocol_fe_get(pfe);
 *
 * - raite_fe_start()
 *   At this point of time, only one thread context can call it.
 *
 * Overall, we are sure we will always only have 1 call at a time.
 */
static int raite_call_be(struct protocol_fe *pfe)
{
	int ret;
	size_t buf_len = 0;
	struct tee_raite_fe *raite_fe =
		container_of(pfe, struct tee_raite_fe, pfe);

	buf_len = sizeof(*raite_fe->ipc_cmd_buf);
	mc_dev_devel("*pfe->fe2be_data size %zu cmd %d id %d",
		     buf_len, pfe->fe2be_data->cmd, pfe->fe2be_data->id);

	if (IS_ERR_OR_NULL(raite_fe->ipc_fe2be_fp)) {
		ret = PTR_ERR(raite_fe->ipc_fe2be_fp);
		mc_dev_err(ret, "%s failed", __func__);
		return ret;
	}

	//We cannot re-initialization the completion after sending
	//The receiver thread may complete before re-initialization and
	//wait_for_completion
	reinit_completion_local(&l_ctx.other_end_complete);

	ret = raite_ipc_send(raite_fe->ipc_fe2be_fp,
			     raite_fe->ipc_cmd_buf, buf_len);
	if (ret) {
		mc_dev_err(ret, "Send message to BE failed\n");
		return ret;
	}

	if (raite_fe_is_blocking_command(pfe->fe2be_data->cmd)) {
		mc_dev_devel("%d waiting completion start",
			     __LINE__);
		if (wait_for_completion_timeout(&l_ctx.other_end_complete,
						msecs_to_jiffies(60000)) == 0) {
			ret = -ETIMEDOUT;
			mc_dev_err(ret,
				   "wait for raite_fe_start_client timeout\n");
		}
		mc_dev_devel("%d waiting completion done", __LINE__);
	}

	return ret;
}

static int raite_fe_start_fe_server(void *arg)
{
	int len, retry = 0;
	loff_t pos = 0;
	struct tee_raite_fe *raite_fe;
	struct raite_answer_buf *buf;
	size_t buf_len = sizeof(struct raite_answer_buf);

	if (!arg) {
		mc_dev_err(-ENODEV, "No raite_fe struct found!");
		return -ENODEV;
	}
	raite_fe = (struct tee_raite_fe *)arg;

	mc_dev_devel("%d raite_answer_buf %zu", __LINE__, buf_len);

	buf = raite_fe->ipc_answer_buf;
	mc_dev_devel("Waiting for receiving msg from BE...\n");
	while (!kthread_should_stop()) {
		if (retry > RAITE_SERVER_RW_RETRY_MAX) {
			mc_dev_err(-EIO, "IPC read error > %d times",
				   RAITE_SERVER_RW_RETRY_MAX);
			msleep(RAITE_SERVER_OPEN_RETRY_DELAY * 5);
			retry = 0;
		}
		pos = 0;
		retry++;
		raite_fe->ipc_be2fe_fp->f_op->unlocked_ioctl(
				raite_fe->ipc_be2fe_fp,
				IOCTL_IPC_WAIT,
				CMD_IPC_WAIT_A_WRITE);
		raite_fe->ipc_be2fe_fp->f_op->unlocked_ioctl(
				raite_fe->ipc_be2fe_fp,
				IOCTL_IPC_MSG_LEN,
				(unsigned long)&len);
		if (len <= 0) {
			mc_dev_err(0, "ipc ioctl failed %d", len);
			continue;
		} else if (len > buf_len) {
			raite_ipc_flush_buffer(raite_fe->ipc_be2fe_fp, len);
			continue;
		}

		len = kernel_read_local(raite_fe->ipc_be2fe_fp, buf, len, &pos);
		if (len != buf_len) {
			mc_dev_err(0,
				   "read len %d != ipc_answer_buf %zu (skip)",
				   len, buf_len);
			continue;
		}
		retry = 0;
		mc_dev_devel("%d read len %d, fe2be_data->cmd %d",
			     __LINE__, len, buf->fe2be_data.cmd);

		if (raite_fe_is_blocking_command(buf->fe2be_data.cmd)) {
			mc_dev_devel("%d complete", __LINE__);
			complete(&l_ctx.other_end_complete);
		} else {
			mc_dev_devel("%d protocol_fe_dispatch", __LINE__);
			raite_fe->pfe.be2fe_data->cmd_ret =
				protocol_fe_dispatch(&raite_fe->pfe);
		}

		if (raite_fe->pfe.be2fe_data->cmd_ret)
			mc_dev_err(raite_fe->pfe.be2fe_data->cmd_ret,
				   "BE -> FE result %u id %u",
				   raite_fe->pfe.be2fe_data->cmd,
				   raite_fe->pfe.be2fe_data->id);
		else
			mc_dev_devel("BE -> FE result %u id %u ret %d",
				     raite_fe->pfe.be2fe_data->cmd,
				     raite_fe->pfe.be2fe_data->id,
				     raite_fe->pfe.be2fe_data->cmd_ret);
	}
	mc_dev_devel("Stop receiving msg from BE...");

	return 0;
}

static void raite_fe_stop_client(struct tee_raite_fe *raite_fe)
{
	if (raite_fe->ipc_recv_th)
		kthread_stop(raite_fe->ipc_recv_th);

	if (!IS_ERR_OR_NULL(raite_fe->ipc_be2fe_fp))
		filp_close(raite_fe->ipc_be2fe_fp, NULL);

	if (!IS_ERR_OR_NULL(raite_fe->ipc_fe2be_fp))
		filp_close(raite_fe->ipc_fe2be_fp, NULL);
}

/* Call it after finishing initialization,
 * send first command to BE as connection test
 */
static int raite_fe_start(struct tee_raite_fe *raite_fe)
{
	int ret = 0;
	int retry = 0;

	mc_dev_info("Connection to BE open");
	while (IS_ERR_OR_NULL(raite_fe->ipc_fe2be_fp)) {
		raite_fe->ipc_fe2be_fp =
			filp_open(raite_fe->ipc_fe2be_path, O_RDWR, 0);
		if (IS_ERR(raite_fe->ipc_fe2be_fp)) {
			if (retry >= RAITE_SERVER_OPEN_RETRY_MAX) {
				ret = PTR_ERR(raite_fe->ipc_fe2be_fp);
				mc_dev_err(ret, "open %s failed retry %d",
					   raite_fe->ipc_fe2be_path, retry);
				return ret;
			}
		}
		msleep(RAITE_SERVER_OPEN_RETRY_DELAY);
		retry++;
	}

	raite_fe->client_is_open = true;

	/* Mutex is not locked because raite_fe is only local here,
	 * but to pass the WARN_ON check in the call function,
	 * protocol_busy is set before the call.
	 */
	raite_fe->pfe.protocol_busy = true;

	/* Set the cmd for the BE with the FE's name*/
	raite_fe->pfe.fe2be_data->cmd = TEE_CONNECT;
	mc_dev_devel("%d server_name size %zu",
		     __LINE__,
		     sizeof(raite_fe->pfe.fe2be_data->server_name));
	strscpy(raite_fe->pfe.fe2be_data->server_name,
		RAITE_VM_ID_FE,
		sizeof(raite_fe->pfe.fe2be_data->server_name));
	raite_fe->pfe.fe2be_data->server_name[
		sizeof(raite_fe->pfe.fe2be_data->server_name) - 1] = '\0';

	/* Call the BE Server with the FE name to create the BE Client */
	ret = raite_call_be(&raite_fe->pfe);
	if (ret)
		return ret;

	raite_fe->pfe.protocol_busy = false;

	/* Communication is in place, proceed with start */
	ret = l_ctx.start();

	mc_dev_devel("FE client initialized with ret %d", ret);
	return ret;
}

static int raite_fe_early_init(int (*probe)(void), int (*start)(void))
{
	l_ctx.start = start;
	return 0;
}

static void raite_fe_exit(void)
{
	raite_fe_stop_client(l_ctx.raite_fe);
	tee_raite_fe_put(l_ctx.raite_fe);
}

static int raite_fe_probe(void)
{
	struct tee_raite_fe *raite_fe;
	int ret;

	mc_dev_devel("%d", __LINE__);
	init_completion(&l_ctx.other_end_complete);
	raite_fe = tee_raite_fe_create();
	if (IS_ERR(raite_fe)) {
		ret = PTR_ERR(raite_fe);
		mc_dev_err(ret, "Failed to create raite_fe");
		return ret;
	}
	l_ctx.raite_fe = raite_fe;

	memcpy(raite_fe->ipc_fe2be_path, IPC_FE_TO_BE, sizeof(IPC_FE_TO_BE));
	memcpy(raite_fe->ipc_be2fe_path, IPC_BE_TO_FE, sizeof(IPC_BE_TO_FE));

	ret = raite_ipc_open(raite_fe->ipc_be2fe_path,
			     &raite_fe->ipc_be2fe_fp);
	if (ret)
		return ret;

	raite_fe->ipc_recv_th = kthread_run(raite_fe_start_fe_server,
					    (void *)raite_fe,
					    "fe_ipc_recv_thread");
	if (IS_ERR(raite_fe->ipc_recv_th)) {
		ret = PTR_ERR(raite_fe->ipc_recv_th);
		mc_dev_err(ret, "recv_thread thread creation failed\n");
		goto err;
	}

	ret = vmsb_init(raite_fe);
	if (ret)
		goto err;

	ret = raite_fe_start(raite_fe);
	if (ret)
		goto err;

	return 0;

err:
	raite_fe_exit();
	return ret;
}

static struct tee_protocol_fe_call_ops fe_call_ops = {
	.call_be = raite_call_be,
	.fe_map_create = raite_fe_map_create,
	.fe_map_release_pmd_ptes = raite_fe_map_release_pmd_ptes,
};

static struct tee_protocol_ops protocol_ops = {
	.name = "RAITE FE",
	.early_init = raite_fe_early_init,
	.init = raite_fe_probe,
	.exit = raite_fe_exit,
	.fe_call_ops = &fe_call_ops,
};

struct tee_protocol_ops *raite_fe_check(void)
{
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
		mc_dev_info("missing %s property",
			    MC_RAITE_BE_PROPNAME);
		ops = (struct tee_protocol_ops *)&protocol_ops;
	} else {
		ops = NULL;
	}
	of_node_put(np);

	return ops;
}
#endif /* CONFIG_RAITE_HYP */
