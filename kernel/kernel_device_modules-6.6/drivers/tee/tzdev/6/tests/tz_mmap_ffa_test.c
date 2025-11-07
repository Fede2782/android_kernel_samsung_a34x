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

#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "core/cdev.h"
#include "core/subsystem.h"
#include "ffa.h"
#include "tz_mmap_ffa_test.h"

MODULE_AUTHOR("Oleksandr Lashchenko <a.lashchenko@samsung.com>");
MODULE_LICENSE("GPL");

#define NUM_PAGES	8
#define FILL_PATTERN	0xFFAA5566AA66FF55

struct page *pages[NUM_PAGES];
void *buffer;
ffa_handle_t handle;

static long tz_mmap_ffa_get_buffer_handle(ffa_handle_t *handle)
{
	int ret;
	unsigned int i;
	
	/* Allocate non-contiguous buffer to reduce page allocator pressure */
	for (i = 0; i < NUM_PAGES; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (!pages[i]) {
			ret = -ENOMEM;
			goto free_page_arr;
		}
	}

	buffer = vmap(pages, NUM_PAGES, VM_MAP, PAGE_KERNEL);
	if (!buffer) {
		ret = -EINVAL;
		goto free_page_arr;
	}

	ret = tzdev_ffa_mem_share(NUM_PAGES, pages, handle);
	if (ret < 0)
		goto unmap_buffer;

	for (i = 0; i < (NUM_PAGES * PAGE_SIZE) / sizeof(uint64_t); i++)
		((uint64_t *)buffer)[i] = FILL_PATTERN;

	return 0;

unmap_buffer:
	vunmap(buffer);

free_page_arr:
	for (i = 0; i < NUM_PAGES; i++) {
		if (!pages[i])
			break;
		__free_page(pages[i]);
	}

	return ret;
}

static void tz_mmap_ffa_close_buffer(ffa_handle_t handle)
{
	unsigned int i;

	tzdev_ffa_mem_reclaim(handle);

	vunmap(buffer);

	for (i = 0; i < NUM_PAGES; i++) {
		if (!pages[i])
			break;
		__free_page(pages[i]);
		pages[i] = NULL;
	}
}

static long tz_mmap_ffa_test_unlocked_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret;

	switch (cmd) {
	case TZ_MMAP_GET_BUFFER_HANDLE:
		ret = tz_mmap_ffa_get_buffer_handle(&handle);
		if (ret)
			return ret;

		ret = put_user(handle, (uint64_t *)arg);
		if (ret)
			return ret;;

		return 0;

	case TZ_MMAP_CLOSE_BUFFER:
		tz_mmap_ffa_close_buffer(handle);

		return 0;
	}

	return -ENOTTY;
}

static const struct file_operations tz_mmap_ffa_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tz_mmap_ffa_test_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_mmap_ffa_test_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tz_mmap_ffa_test_cdev = {
	.name = "tz_mmap_ffa_test",
	.fops = &tz_mmap_ffa_test_fops,
	.owner = THIS_MODULE,
};

int tz_mmap_ffa_test_init(void)
{
	int rc;

	rc = tz_cdev_register(&tz_mmap_ffa_test_cdev);
	if (rc) {
		pr_err("failed to register device, ret=%d\n", rc);
		return rc;
	}

	return 0;
}

void tz_mmap_ffa_test_exit(void)
{
	tz_cdev_unregister(&tz_mmap_ffa_test_cdev);
}

tzdev_initcall(tz_mmap_ffa_test_init);
tzdev_exitcall(tz_mmap_ffa_test_exit);
