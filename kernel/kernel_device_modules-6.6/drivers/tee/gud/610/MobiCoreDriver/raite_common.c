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

#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

#include "main.h"
#include "client.h"
#include "raite_common.h"

static struct mem_desc_t raite_vm_sm;

static void tee_raite_fe_release(struct kref *kref)
{
	struct tee_raite_fe *raite_fe = container_of(kref,
						struct tee_raite_fe, kref);

	if (raite_fe->pfe.client)
		client_close(raite_fe->pfe.client);

	kfree(raite_fe->ipc_answer_buf);
	kfree(raite_fe);
}

/*
 * RAITE IPC ops are fully asynchronous (FE unblock as soon as BE read).
 * For some of our TEE PV commands, FE need to block until the BE had
 * time to also fill the answer in the commands parameters.
 *
 * List of blocking/synchronous PV commands:
 * - TEE_CONNECT
 * - TEE_MC_OPEN_SESSION
 * - TEE_MC_OPEN_TRUSTLET
 * - TEE_MC_NOTIFY
 * - TEE_MC_MAP
 * - TEE_MC_UNMAP
 * - TEE_GP_REGISTER_SHARED_MEM
 * - TEE_GP_RELEASE_SHARED_MEM
 * - TEE_GP_REQUEST_CANCELLATION
 *
 * All others are asynchrouns and don't need to block
 * (TEE_MC_WAIT_DONE, TEE_GP_OPEN_SESSION_DONE,
 * TEE_GP_CLOSE_SESSION_DONE, TEE_GP_INVOKE_COMMAND_DONE).
 *
 * @param cmd one of the tee_protocol_fe_cmd or tee_protocol_be_cmd
 * @return 0 if not blocking cmd (else 1).
 */
int raite_fe_is_blocking_command(int cmd)
{
	int ret = 1;

	switch (cmd) {
	/* list of not blocking calls */
	case TEE_MC_WAIT_DONE:
	case TEE_GP_OPEN_SESSION_DONE:
	case TEE_GP_CLOSE_SESSION_DONE:
	case TEE_GP_INVOKE_COMMAND_DONE:
		mc_dev_devel("%s cmd %d", __func__, cmd);
		ret = 0;
		break;
	default:
	/* else, default is blocking */
		ret = 1;
		break;
	}
	return ret;
}

/*
 * Manage and allocate all the needed internal struct.
 * Called by FE for sure, but also used by the BE to create FE related
 * structs.
 */
struct tee_raite_fe *tee_raite_fe_create(void)
{
	struct tee_raite_fe *raite_fe;
	int ret;

	raite_fe = kzalloc(sizeof(*raite_fe), GFP_KERNEL);
	if (!raite_fe)
		return ERR_PTR(-ENOMEM);

	ret = protocol_fe_init(&raite_fe->pfe);
	if (ret) {
		mc_dev_err(ret, "failed to init raite_fe");
		goto err;
	}

	/* Allocate the hypervisor inter VM communication buffer.
	 * This buffer contains both fe2be_data and be2fe_data.
	 * Re-assign the generic protocol_fe answer buffer to the hypervisor
	 * specific be2fe communication buffers.
	 * Re-assign the generic protocol_fe command buffer to the hypervisor
	 * specific fe2be communication buffers.
	 */
	raite_fe->ipc_answer_buf =
		kzalloc(sizeof(*raite_fe->ipc_answer_buf), GFP_KERNEL);
	if (!raite_fe->ipc_answer_buf) {
		mc_dev_err(ret, "failed allocate RAITE ipc_answer_buf");
		goto err;
	}
	raite_fe->pfe.be2fe_data =
		(struct be2fe_data *)raite_fe->ipc_answer_buf;

	raite_fe->ipc_cmd_buf =
		(struct raite_cmd_buf *)(&raite_fe->ipc_answer_buf->fe2be_data);
	raite_fe->pfe.fe2be_data =
		(struct fe2be_data *)raite_fe->ipc_cmd_buf;

	kref_init(&raite_fe->kref);
	INIT_LIST_HEAD(&raite_fe->list);

	return raite_fe;

err:
	tee_raite_fe_release(&raite_fe->kref);
	return ERR_PTR(-ENOMEM);
}

void tee_raite_fe_put(struct tee_raite_fe *raite_fe)
{
	kref_put(&raite_fe->kref, tee_raite_fe_release);
}

void tee_raite_fe_cleanup(struct tee_raite_fe *raite_fe)
{
	if (raite_fe->pfe.client) {
		client_close(raite_fe->pfe.client);
		raite_fe->pfe.client = NULL;
	}
}

/*
 * Open a RAITE IPC file descriptor.
 * Function implements a retry loop to wait a bit in case of delay
 * between TEE driver and RAITE IPC device nodes loading.
 * (The RAITE IPC device nodes will be ready after mounting rootfs and
 * devtmpfs).
 */
int raite_ipc_open(char *path, struct file **file)
{
	int ret = 0, retry = 0;

	while (IS_ERR_OR_NULL(*file)) {
		*file =	filp_open(path, O_RDWR, 0);
		if (IS_ERR(*file)) {
			if (retry >= RAITE_SERVER_OPEN_RETRY_MAX) {
				ret = PTR_ERR(*file);
				mc_dev_err(ret, "open %s failed retry %d",
					   path, retry);
				return ret;
			}
		}
		msleep(RAITE_SERVER_OPEN_RETRY_DELAY);
		retry++;
	}

	return ret;
}

/*
 * Write an IPC message into file. API called under Mutex.
 * It will block and wait until...
 *  - answer received by the other peer
 *  - a timeout IOCTL_IPC_WAITTIMEOUT
 *
 * "Fatal" errors potentially not ideally managed.
 * After several retries, if still failed, we give up and exit to not
 * block the entire kernel.
 * No real recovery solution at this point.
 * Print a log for future debug.
 * (risk is that FE app waiting asynchronous answer staying stuck).
 */
int raite_ipc_send(struct file *file, void *buf, size_t buf_len)
{
	int ret, retry;
	loff_t pos = 0;
	ssize_t send_len = 0;

	ret = -EIO;
	if (IS_ERR_OR_NULL(file)) {
		mc_dev_err(ret, "IPC file error or null");
		return PTR_ERR(file);
	}

	retry = 0;
	while (retry++ < RAITE_SERVER_RW_RETRY_MAX) {
		send_len = kernel_write_local(file, buf, buf_len, pos);

		/* write return len <= 0: write failed, retry directly.
		 *  0: The count in the parameter is too large to be send
		 *  -2: copy_from_user error
		 *
		 * write return len != write len
		 *  it's not possible, but if it happened, let the peer read it
		 * out and write again.
		 * If the peer didn't crash or get stuck, there should be only
		 * one unhandled data.
		 * Because we call IOCTL_IPC_WAITTIMEOUT after writing.
		 */
		if (unlikely(send_len <= 0)) {
			mc_dev_err(ret,
				   "IPC write failed send_len %zd", send_len);
			continue;
		} else if (unlikely(send_len != buf_len)) {
			mc_dev_err(ret, "ipc send failed %zd != %zu",
				   send_len, buf_len);
			continue;
		} else {
			ret = 0;
			break;
		}
	}

	return ret;
}

/* Parital read/write are unsupported.
 * For invalid data, we must read it to flush the buffer else we will
 * keep receiving again and again the same invalid data.
 */
void raite_ipc_flush_buffer(struct file *file, size_t len)
{
	loff_t pos = 0;
	u8 *buf;
	size_t read_len = 0;

	mc_dev_err(0, "Unexpected data len %zu, empty and skip it", len);
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		mc_dev_err(-ENOMEM, "Out of memory, trying to ignore...");
	} else {
		read_len = kernel_read_local(file, buf, len, &pos);
		kfree(buf);
		if (read_len != len) {
			mc_dev_err(0,
				   "buffer flush failed (%zu != %zu), trying to ignore...",
				   len, read_len);
		}
	}
}

static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	// Map shared buffer between VMs, uncached recommended by Zlingsmart
	prot = pgprot_noncached(PAGE_KERNEL);

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
		int ret = PTR_ERR(vaddr);

		mc_dev_err(ret, "Failed to map %u pages", page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

void *raite_reserve_memory(int *total_pages)
{
	struct device_node *np;
	int ret;
	struct resource res;
	void *bufp;

	np = of_find_compatible_node(NULL, NULL,
				     RAITE_VM_SHARED_MEM_COMPATIBLE);
	if (!np) {
		mc_dev_err(0, "can't find compatible node for %s",
			   RAITE_VM_SHARED_MEM_COMPATIBLE);
		return NULL;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		mc_dev_err(0, "failed to get address resource");
		return NULL;
	}

	raite_vm_sm.start = res.start;
	raite_vm_sm.size = resource_size(&res);
	mc_dev_info("get address resource start %lx size %lx",
		    raite_vm_sm.start, raite_vm_sm.size);

	bufp = remap_lowmem(raite_vm_sm.start, raite_vm_sm.size);

	if (bufp) {
		*total_pages = raite_vm_sm.size / PAGE_SIZE;
		mc_dev_devel("buffer start: 0x%p, %d pages",
			     bufp, *total_pages);
	}

	return bufp;
}

int raite_fe_memory_region(struct tee_raite_fe *raite_fe)
{
	struct device_node *np, *child;
	int ret = 0, cnt;
	struct resource res;

	np = of_find_node_by_name(NULL, RAITE_RESERVED_MEM_NODE_NAME);
	if (!np) {
		mc_dev_err(-ENXIO, "can't find %s device node",
			   RAITE_RESERVED_MEM_NODE_NAME);
		return -ENXIO;
	}

	child = NULL;
	cnt = 0;
	while ((child = of_get_next_child(np, child)) != NULL) {
		if (strncmp(child->name,
			    RAITE_FE_RESERVED_MEM_PREFIX,
			    strlen(RAITE_FE_RESERVED_MEM_PREFIX)) == 0) {
			cnt++;
		}
	}

	raite_fe->fe_mem_region = (struct mem_desc_t *)
		kmalloc_array(cnt, sizeof(struct mem_desc_t), GFP_KERNEL);

	child = NULL;
	cnt = 0;
	while ((child = of_get_next_child(np, child)) != NULL) {
		if (strncmp(child->name,
			    RAITE_FE_RESERVED_MEM_PREFIX,
			    strlen(RAITE_FE_RESERVED_MEM_PREFIX)) == 0) {
			ret = of_address_to_resource(child, 0, &res);
			if (ret) {
				mc_dev_info("failed to get resource of %s",
					    child->name);
			} else {
				raite_fe->fe_mem_region[cnt].start = res.start;
				raite_fe->fe_mem_region[cnt].end = res.end;
				raite_fe->fe_mem_region[cnt].size =
					resource_size(&res);
				mc_dev_info("get resource start %llu end %llu",
					    res.start, res.end);
				cnt++;
			}
		}
	}
	raite_fe->num_region = cnt;

	if (cnt == 0) {
		ret = -ENXIO;
		mc_dev_err(-ENXIO, "can't find %sX device node",
			   RAITE_FE_RESERVED_MEM_PREFIX);
	}

	return ret;
}

#endif /* CONFIG_RAITE_HYP */
