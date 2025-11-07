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

#include "core/log.h"
#include <ffa.h>
#include <linux/arm_ffa.h>
#include <linux/arm-smccc.h>
#include <linux/scatterlist.h>
#include <linux/uuid.h>
#include <tzdev_internal.h>

extern struct bus_type ffa_bus_type;

static const struct ffa_dev_ops *ffa_ops;
static const unsigned short sp_id = 0x8001;
static struct ffa_device *ffa_dev = NULL;
static struct device *bus_dev;

static const struct ffa_device_id tzdev_ffa_device_id[] = {
	{ UUID_INIT(0x0, 0x0, 0x0,
			0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01) },
	{}
};

int tzdev_ffa_drv_direct_msg(struct tzdev_smc_data *data)
{
	int ret;
	struct ffa_send_direct_data ffa_data = {};

	ffa_data.data0 = data->args[0] | TZDEV_SMC_MAGIC;
	ffa_data.data1 = data->args[1];
	ffa_data.data2 = data->args[2];
	ffa_data.data3 = data->args[3];
	ffa_data.data4 = data->args[4];

	ret = ffa_ops->sync_send_receive(ffa_dev, &ffa_data);
	while (ret == -EBUSY) {
		log_debug(tzdev_ffa_driver_wrapper, "tzdev ffa command retry...\n");
		ret = ffa_ops->sync_send_receive(ffa_dev, &ffa_data);
	}

	if (ret) {
		log_error(tzdev_ffa_driver_wrapper, "tzdev ffa command failed, error=%d\n", ret);
		return ret;
	}

	data->args[0] = ffa_data.data0;
	data->args[1] = ffa_data.data1;
	data->args[2] = ffa_data.data2;
	data->args[3] = ffa_data.data3;

	return 0;
}

int tzdev_ffa_mem_share(unsigned int num_pages, struct page **pages, ffa_handle_t *phandle)
{
	struct sg_table sgt;
	struct ffa_mem_region_attributes mem_region_attributes;
	struct ffa_mem_ops_args args;
	int ret;

	mem_region_attributes.receiver = ffa_dev->vm_id;
	mem_region_attributes.attrs = FFA_MEM_RW;

	args.use_txbuf = true;
	args.attrs = &mem_region_attributes;
	args.nattrs = 1;
	args.flags = 0;
	args.tag = 0;
	args.g_handle = 0;

	ret = sg_alloc_table_from_pages(&sgt, pages, num_pages, 0, num_pages * PAGE_SIZE, GFP_KERNEL);
	if (ret) {
		log_error(tzdev_ffa_driver_wrapper, "Failed to alloc pages table, error=%d\n", ret);
		return ret;
	}

	args.sg = sgt.sgl;

	ret = ffa_ops->memory_share(ffa_dev, &args);
	if (ret < 0) {
		log_error(tzdev_ffa_driver_wrapper, "Failed to share memory, error=%d\n", ret);
		goto exit;
	}

	*phandle = args.g_handle;
	ret = 0;

exit:
	sg_free_table(&sgt);

	return ret;
}

int tzdev_ffa_mem_reclaim(ffa_handle_t handle)
{
	BUG_ON(ffa_ops->memory_reclaim(handle, 0));

	return 0;
}

static int tzdev_ffa_probe(struct ffa_device *ffa_dev)
{
	ffa_ops = ffa_dev_ops_get(ffa_dev);
	if (!ffa_ops) {
		log_error(tzdev_ffa_driver_wrapper, "ffa_ops ops is NULL\n");
		return -EINVAL;
	}

	return 0;
}

static void tzdev_ffa_remove(struct ffa_device *ffa_dev)
{
	(void)ffa_dev;
}

static struct ffa_driver tzdev_ffa_driver = {
	.name = "tzdev",
	.probe = tzdev_ffa_probe,
	.remove = tzdev_ffa_remove,
	.id_table = tzdev_ffa_device_id,
};

int tzdev_ffa_init(void)
{
	int ret;

	while ((bus_dev = bus_find_next_device(&ffa_bus_type, bus_dev))) {
		ffa_dev = to_ffa_dev(bus_dev);
		if (ffa_dev->vm_id == sp_id) {
			break;
		}
		put_device(bus_dev);
	};

	if (bus_dev == NULL) {
		log_error(tzdev_ffa_driver_wrapper, "tzdev ffa device not found\n");
		return -ENOENT;
	}

	uuid_copy(&ffa_dev->uuid, &tzdev_ffa_device_id[0].uuid);

	ret = ffa_register(&tzdev_ffa_driver);

	put_device(bus_dev);

	return ret;
}

void tzdev_ffa_fini(void)
{
	ffa_unregister(&tzdev_ffa_driver);
}