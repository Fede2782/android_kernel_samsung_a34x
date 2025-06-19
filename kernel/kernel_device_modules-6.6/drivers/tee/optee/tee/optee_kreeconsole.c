// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/tee_drv.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/module.h>
#include "common/tee_private.h"
#include "optee_private.h"

#define KREE_CONSOLE_BUFSIZE 4096
static struct optee_kreeconsole_buf *optee_kcb;
static struct mutex kree_console_mutex;

#define KREE_CONSOLE_PTA_UUID \
	{ 0x91, 0x89, 0xc1, 0xcb, 0x92, 0x29, 0x51, 0x82, \
	0x47, 0x32, 0xcf, 0x9a, 0x63, 0x84, 0x08, 0xc5 }

#define PTA_CMD_KREE_CONOLE_INIT 0

void handle_rpc_func_kree_console_flush(void)
{
	char buf[128];
	unsigned short end_of_read;
	size_t len;
	int written;

	if (optee_kcb == NULL)
		return;

	mutex_lock(&kree_console_mutex);
	end_of_read = (optee_kcb->write_pos + optee_kcb->size - 1);
	end_of_read = end_of_read % optee_kcb->size;
	while (optee_kcb->read_pos != end_of_read) {
		len = sizeof(buf)-1;
		if (optee_kcb->read_pos > end_of_read) {
			/* wrapped around */
			if (len > optee_kcb->size - (optee_kcb->read_pos+1)) {
				memcpy(buf,
					&optee_kcb->buf[optee_kcb->read_pos+1],
					optee_kcb->size -
					(optee_kcb->read_pos+1));
				written = optee_kcb->size -
					(optee_kcb->read_pos+1);
				len -= written;
				if (len > end_of_read)
					len = end_of_read;
				memcpy(buf+written,
					&optee_kcb->buf[0],
					len);
				buf[written+len] = '\0';
				optee_kcb->read_pos = len;
			} else {
				memcpy(buf,
					&optee_kcb->buf[optee_kcb->read_pos+1],
					len);
				optee_kcb->read_pos += len;
			}
		} else {
			if (len > optee_kcb->size - (optee_kcb->read_pos+1))
				len = optee_kcb->size - (optee_kcb->read_pos+1);
			if (len > end_of_read - optee_kcb->read_pos)
				len = end_of_read - optee_kcb->read_pos;

			memcpy(buf,
				&optee_kcb->buf[optee_kcb->read_pos + 1],
				len);
			buf[len] = '\0';
			optee_kcb->read_pos += len;
			optee_kcb->read_pos %= optee_kcb->size;
		}
		pr_notice("%s", buf);
	}
	mutex_unlock(&kree_console_mutex);
}
EXPORT_SYMBOL_GPL(handle_rpc_func_kree_console_flush);

static int optee_dev_match(struct tee_ioctl_version_data *v,
		const void *d)
{
	if (v->impl_id == TEE_IMPL_ID_OPTEE)
		return 1;
	return 0;
}

int optee_kreeconsole_init(void)
{
	int rc, ret = 0;
	int id = 0;
	struct tee_context *tee_ctx;
	struct tee_ioctl_open_session_arg osarg;
	uint8_t kreeconsole_uuid[] = KREE_CONSOLE_PTA_UUID;
	struct tee_param param;
	struct tee_ioctl_invoke_arg arg;
	unsigned int *data = NULL;

	data = kmalloc(KREE_CONSOLE_BUFSIZE, GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}
	memset(data, 0, KREE_CONSOLE_BUFSIZE);
	mutex_init(&kree_console_mutex);

	/* open context */
	tee_ctx = tee_client_open_context(NULL, optee_dev_match,
						NULL, NULL);
	if (IS_ERR(tee_ctx)) {
		pr_notice("open_context failed err %ld", PTR_ERR(tee_ctx));
		ret = -ENODEV;
		kfree(data);
		goto out;
	}

	/* open session */
	memset(&osarg, 0, sizeof(osarg));
	osarg.num_params = 0;
	osarg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	memcpy(osarg.uuid, kreeconsole_uuid, sizeof(kreeconsole_uuid));
	rc = tee_client_open_session(tee_ctx, &osarg, NULL);
	if (rc || osarg.ret) {
		pr_notice("open_session failed err %d, ret=0x%x", rc, osarg.ret);
		ret = -EACCES;
		kfree(data);
		goto close_context;
	}

	/* alloc shm for logbuf */
	ret = tee_client_register_shm(tee_ctx, (unsigned long)data,
			   KREE_CONSOLE_BUFSIZE, &id);
	if (ret) {
		pr_notice("tee_shm_alloc failed\n");
		ret = -ENOSPC;
		kfree(data);
		goto close_session;
	}

	optee_kcb = (struct optee_kreeconsole_buf *)data;
	optee_kcb->size = KREE_CONSOLE_BUFSIZE - sizeof(*optee_kcb);
	optee_kcb->read_pos = 0;
	optee_kcb->write_pos = 1;

	/* invoke init cmd to pass the logbuf */
	memset(&arg, 0, sizeof(arg));
	arg.num_params = 1;
	arg.session = osarg.session;
	arg.func = PTA_CMD_KREE_CONOLE_INIT; /* cmd id */

	memset(&param, 0, sizeof(param));
	param.attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param.u.memref.shm = tee_shm_get_from_id(tee_ctx, id);
	param.u.memref.size = KREE_CONSOLE_BUFSIZE;

	rc = tee_client_invoke_func(tee_ctx, &arg, &param);
	if (rc) {
		pr_notice("%s(): rc = %d\n", __func__, rc);
		ret = -EPERM;
		kfree(data);
		tee_client_unregister_shm(tee_ctx, id);
		goto close_session;
	}
	if (arg.ret != 0) {
		pr_notice("%s(): ret 0x%x, orig 0x%x", __func__,
				arg.ret, arg.ret_origin);
		ret = -ENOEXEC;
		kfree(data);
		tee_client_unregister_shm(tee_ctx, id);
	}

close_session:
	/* close session */
	rc = tee_client_close_session(tee_ctx, osarg.session);
	if (rc != 0)
		pr_notice("close_session failed err %d", rc);
close_context:
	/* close context */
	tee_client_close_context(tee_ctx);
out:
	return ret;
}
EXPORT_SYMBOL_GPL(optee_kreeconsole_init);

MODULE_LICENSE("GPL");
