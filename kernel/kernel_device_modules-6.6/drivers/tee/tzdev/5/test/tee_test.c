/*
 * Copyright (C) 2012-2017, Samsung Electronics Co., Ltd.
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

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

#include "core/log.h"
#include "core/cdev.h"
#include "core/subsystem.h"
#include <tzdev/tee_client_api.h>
#include "test/test.h"

#define BUFFER_SIZE_ALLOCATED		50
#define BUFFER_SIZE_TMPREF		100
#define BUFFER_SIZE			32

#define UNUSED(x) ((void )x)

/* test_TA */
static TEEC_UUID uuid = {
	0x08040000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x19}
};

/* for FFA TEST */
#if 0
#define NUM_PAGES	8
#define FILL_PATTERN		0xFFAA5566AA66FF55

struct page *pages[NUM_PAGES];
void *buffer;
ffa_handle_t handle;

/**/

static long tz_mmap_ffa_get_buffer_handle(ffa_handle_t *handle)
{
	int ret;
	unsigned int i;
	
	pr_err("%s\n",__func__);
	/* Allocate non-contiguous buffer to reduce page allocator pressure */
	for (i = 0; i < NUM_PAGES; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (!pages[i]) {
			ret = -ENOMEM;
			pr_err("%s : ENOMEM\n",__func__);
			goto free_page_arr;
		}
	}

	buffer = vmap(pages, NUM_PAGES, VM_MAP, PAGE_KERNEL);
	if (!buffer) {
		ret = -EINVAL;
		pr_err("%s : EINVAL\n",__func__);
		goto free_page_arr;
	}

	ret = tzdev_ffa_mem_share(NUM_PAGES, pages, handle);
	pr_err("%s : handle : %ull, ret %d\n", __func__, (long)*handle, ret);
	if (ret < 0)
		goto unmap_buffer;

	for (i = 0; i < (NUM_PAGES * PAGE_SIZE) / sizeof(uint64_t); i++) {
		((uint64_t *)buffer)[i] = FILL_PATTERN;
	}

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
#endif
uint32_t tz_tee_test(uint32_t *origin)
{
	TEEC_Context context;
    TEEC_Session session;
    TEEC_Operation operation;
    uint32_t returnOrigin;
    TEEC_Result res = TEEC_InitializeContext(NULL, &context);
    char greeting_to_SWd[] = "Hello from NWd";
    char response_from_SWd[0x100] = {'\0'};

    if (res != TEEC_SUCCESS) {
        pr_err("TEST: InitializeContext failed: %x\n", res);
        return 1;
    }

	pr_err("TEEC_OpenSession\n");

    res = TEEC_OpenSession(&context, &session, &uuid, 0, NULL, NULL, \
                           &returnOrigin);
    if (res != TEEC_SUCCESS) {
        pr_err("TEST: TEEC_OpenSession returned %x from %x\n", res,
                returnOrigin);
        return 1;
    }
    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                                            TEEC_MEMREF_TEMP_OUTPUT,
                                            TEEC_NONE,
                                            TEEC_NONE);
    operation.params[0].tmpref.buffer = &greeting_to_SWd;
    operation.params[0].tmpref.size = sizeof(greeting_to_SWd);
    operation.params[1].tmpref.buffer = &response_from_SWd;
    operation.params[1].tmpref.size = sizeof(response_from_SWd);

	pr_err("operation setting done. TEEC_InvokeCommand calling!\n");

    res = TEEC_InvokeCommand(&session, 0x0, &operation, &returnOrigin);
    if (res != TEEC_SUCCESS) {
        pr_err("TEST: TEEC_InvokeCommand returned %x from %x\n", res,
                returnOrigin);
        return 1;
    }

    pr_err("TEST: SWd responded by : %s\n", response_from_SWd);
    TEEC_CloseSession(&session);
    TEEC_FinalizeContext(&context);
    return (res != TEEC_SUCCESS);
}

/*
uint32_t tz_tee_test_ffa(uint32_t *origin)
{
	TEEC_Context context;
    TEEC_Session session;
    TEEC_Operation operation;
    uint32_t returnOrigin;
    TEEC_Result res = TEEC_InitializeContext(NULL, &context);
	int ret;

    if (res != TEEC_SUCCESS) {
        pr_err("TEST: InitializeContext failed: %x\n", res);
        return 1;
    }
#if 1	
	pr_err("%s : FFA TEST start\n", __func__);
	ret = tz_mmap_ffa_get_buffer_handle(&handle);
//	tz_mmap_ffa_close_buffer(handle);
	pr_err("%s : tz_mmap_ffa_get_buufer_handle ret %d\n", __func__, ret);
	
	pr_err("%s : FFA TEST end\n", __func__);
#endif
    res = TEEC_OpenSession(&context, &session, &uuid, 0, NULL, NULL, \
                           &returnOrigin);
    if (res != TEEC_SUCCESS) {
        pr_err("TEST: TEEC_OpenSession returned %x from %x\n", res,
                returnOrigin);
        return 1;
    }

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
											TEEC_NONE, TEEC_NONE, TEEC_NONE);

	operation.params[0].value.a = handle & 0xFFFFFFFF;
	operation.params[0].value.b = (handle >> 32) & 0xFFFFFFFF;

    res = TEEC_InvokeCommand(&session, 0x1, &operation, &returnOrigin);
    if (res != TEEC_SUCCESS) {
        pr_err("TEST: TEEC_InvokeCommand returned %x from %x\n", res,
                returnOrigin);
        return 1;
    }

#if 1
	tz_mmap_ffa_close_buffer(handle);
#endif
    TEEC_CloseSession(&session);
    TEEC_FinalizeContext(&context);	
    return (res != TEEC_SUCCESS);
	
}
*/

uint32_t do_tee_test(void)
{
	uint32_t origin = 0;
	return tz_tee_test(&origin);
}
/*
uint32_t do_tee_test_ffa(void)
{
	uint32_t origin = 0;
	return tz_tee_test_ffa(&origin);
}
*/
static int tee_test_open(struct inode *inode, struct file *file)
{
	UNUSED(inode);
	UNUSED(file);

	pr_err("%s:%d\n", __func__, __LINE__);
	return 0; 
}

static int tee_test_release(struct inode *ino, struct file *file)
{
	UNUSED(ino);
	UNUSED(file);

	pr_err("%s:%d\n", __func__, __LINE__);
	return 0;
}

static ssize_t tee_test_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	long cmd;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtol(desc, 10, &cmd) != 0)
		return count;

	pr_err("%s: received user space cmd '%ld'\n", __func__, cmd);
	
	switch(cmd)	{
	case 0:
		do_tee_test();
		break;
/*	case 1:
 *		do_tee_test_ffa();
 *		break;
 */
	default:
		pr_err("%s: default\n", __func__);
		break;
	}
	return count;
}


static const struct proc_ops tee_test_fops = {
	.proc_open = tee_test_open,
	.proc_release = tee_test_release,
	.proc_ioctl = NULL,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = NULL,
#endif
	.proc_write = tee_test_write,
};

int tee_test_init(void)
{
	pr_err("%s: tzdev\n", __func__);
	proc_create("teegris_tee_test", 0664, NULL, &tee_test_fops);

	return 0;
}

void tee_test_exit(void)
{
//	tz_cdev_unregister(&tee_test_cdev);
}

tzdev_initcall(tee_test_init);
tzdev_exitcall(tee_test_exit);
