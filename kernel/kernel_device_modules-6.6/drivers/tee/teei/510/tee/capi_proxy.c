// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <tee_client_api.h>
#include "capi_proxy.h"

#define IMSG_TAG "[tz_capi_proxy]"
#include <imsg_log.h>

#define HOSTNAME "bta_loader"

struct teei_proxy_context {
	struct TEEC_Context capi_ctx;
	struct list_head link;
	int owner_pid;
};

static struct list_head g_teei_proxy_link;
static struct mutex proxy_link_mutex;
static struct mutex proxy_array_mutex;

#define MAX_CAPI_SESSIONS 10
#define MAX_CAPI_SHMS 12

static struct TEEC_Session capi_session[MAX_CAPI_SESSIONS];
static struct TEEC_SharedMemory capi_shm[MAX_CAPI_SHMS];

struct ida capi_session_ida;
struct ida capi_shm_ida;

static int __link_proxy_context(struct teei_proxy_context *ctx)
{
	mutex_lock(&proxy_link_mutex);
	list_add_tail(&(ctx->link), &g_teei_proxy_link);
	mutex_unlock(&proxy_link_mutex);

	return 0;
}

static int __unlink_proxy_context(struct teei_proxy_context *ctx)
{
	mutex_lock(&proxy_link_mutex);
	list_del(&(ctx->link));
	mutex_unlock(&proxy_link_mutex);

	return 0;
}

static struct teei_proxy_context *__get_proxy_context(void)
{
	struct teei_proxy_context *ctx = NULL;
	int pid = 0;
	int ctx_found = 0;

	pid = current->pid;

	mutex_lock(&proxy_link_mutex);

	list_for_each_entry(ctx, &g_teei_proxy_link, link) {
		if (ctx->owner_pid == pid) {
			ctx_found = 1;
			break;
		}
	}

	mutex_unlock(&proxy_link_mutex);

	if (ctx_found == 1)
		return ctx;

	return NULL;

}

static struct TEEC_Context *teei_create_proxy_context(void)
{
	struct teei_proxy_context *proxy_cont = NULL;
	int pid = 0;
	int retVal = 0;

	proxy_cont = kmalloc(sizeof(struct teei_proxy_context), GFP_KERNEL);
	if (proxy_cont == NULL) {
		IMSG_ERROR("Failed to kmalloc struct teei_proxy_context!\n");
		return NULL;
	}

	memset(proxy_cont, 0, sizeof(struct teei_proxy_context));

	pid = current->pid;

	proxy_cont->owner_pid = pid;
	INIT_LIST_HEAD(&(proxy_cont->link));

	retVal = __link_proxy_context(proxy_cont);
	if (retVal != 0) {
		IMSG_ERROR("Failed to link proxy context, %d!\n", retVal);
		goto free_proxy_context;
	}

	return &(proxy_cont->capi_ctx);

free_proxy_context:
	kfree(proxy_cont);

	return NULL;
}

static struct TEEC_Context *teei_find_proxy_context(void)
{
	struct teei_proxy_context *proxy_cont = NULL;

	proxy_cont = __get_proxy_context();
	if (proxy_cont == NULL) {
		IMSG_ERROR("Failed to get proxy context of current thread!\n");
		return NULL;
	}

	return &(proxy_cont->capi_ctx);
}

static int teei_release_proxy_context(void)
{
	struct teei_proxy_context *proxy_cont = NULL;
	int retVal = 0;

	proxy_cont = __get_proxy_context();
	if (proxy_cont == NULL) {
		IMSG_ERROR("Failed to get proxy context of current thread!\n");
		return -EINVAL;
	}

	retVal = __unlink_proxy_context(proxy_cont);
	if (retVal != 0) {
		IMSG_ERROR("Failed to unlink proxy context, %d!\n", retVal);
		return retVal;
	}

	kfree(proxy_cont);

	return 0;
}

static int alloc_capi_session(void)
{
	int retVal = 0;

	mutex_lock(&proxy_array_mutex);
	retVal = ida_simple_get(&capi_session_ida, 0,
			MAX_CAPI_SESSIONS, GFP_KERNEL);
	mutex_unlock(&proxy_array_mutex);

	return retVal;
}

static void free_capi_session(int id)
{
	mutex_lock(&proxy_array_mutex);
	ida_simple_remove(&capi_session_ida, id);
	memset(&capi_session[id], 0, sizeof(struct TEEC_Session));
	mutex_unlock(&proxy_array_mutex);
}

static int alloc_capi_shm(void)
{
	int retVal = 0;

	mutex_lock(&proxy_array_mutex);
	retVal = ida_simple_get(&capi_shm_ida, 0, MAX_CAPI_SHMS, GFP_KERNEL);
	mutex_unlock(&proxy_array_mutex);

	return retVal;
}

static void free_capi_shm(int id)
{
	mutex_lock(&proxy_array_mutex);
	ida_simple_remove(&capi_shm_ida, id);
	memset(&capi_shm[id], 0, sizeof(struct TEEC_SharedMemory));
	mutex_unlock(&proxy_array_mutex);
}

static int capi_params_to_op(struct capi_proxy_param *params,
		struct TEEC_Operation *op)
{
	size_t n;
	long ret = 0;
	union TEEC_Parameter *op_param;
	struct TEEC_SharedMemory *op_shm;
	size_t op_shm_off;
	struct capi_proxy_param_memref *memref;

	memset(op, 0, sizeof(struct TEEC_Operation));

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		op->paramTypes |= (uint32_t)params[n].attr << (4 * n);
		op_param = &op->params[n];

		switch (params[n].attr) {
		case TEEC_NONE:
			break;
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			op_param->value.a = params[n].value.a;
			op_param->value.b = params[n].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			memref = &params[n].memref;

			op_param->tmpref.buffer =
					kmalloc(memref->size, GFP_KERNEL);
			if (!op_param->tmpref.buffer) {
				IMSG_ERROR("failed to allocate buffer\n");
				ret = -ENOMEM;
				goto err;
			}
			op_param->tmpref.size = memref->size;

			ret = copy_from_user(
				(void *)(unsigned long)
				(op_param->tmpref.buffer),
				(void *)(unsigned long)(memref->buffer),
				memref->size);
			if (ret) {
				IMSG_ERROR("failed to copy from user\n");
				ret = -EIO;
				goto err;
			}
			break;
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			memref = &params[n].memref;

			op_param->memref.parent =
					&capi_shm[memref->shm_id];

			op_param->memref.size = memref->size;
			op_param->memref.offset = memref->shm_offset;

			op_shm = op_param->memref.parent;
			op_shm_off = op_param->memref.offset;

			ret = copy_from_user(
				(char *)(unsigned long)(op_shm->buffer)
					+ op_shm_off,
				(char *)(unsigned long)(memref->buffer)
					+ op_shm_off,
				memref->size);
			if (ret) {
				IMSG_ERROR("failed to copy from user\n");
				ret = -EIO;
				goto err;
			}
			break;
		default:
			IMSG_ERROR("unknown param type (0x%llx)\n",
					params[n].attr);
			ret = -EINVAL;
			break;
		}
	}

err:
	if (ret) {
		for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
			switch (params[n].attr) {
			case TEEC_MEMREF_TEMP_INPUT:
			case TEEC_MEMREF_TEMP_OUTPUT:
			case TEEC_MEMREF_TEMP_INOUT:
				kfree(op->params[n].tmpref.buffer);
				break;
			}
		}
	}

	return ret;
}

static void capi_op_to_params(struct capi_proxy_param *params,
		struct TEEC_Operation *op)
{
	size_t n;
	int ret;
	union TEEC_Parameter *op_param;
	struct TEEC_SharedMemory *op_shm;
	size_t op_shm_off;
	struct capi_proxy_param_memref *memref;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		op_param = &op->params[n];

		switch (params[n].attr) {
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			params[n].value.a = op_param->value.a;
			params[n].value.b = op_param->value.b;
			break;
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			memref = &params[n].memref;

			memref->size = op_param->tmpref.size;

			ret = copy_to_user(
				(void *)(unsigned long)(memref->buffer),
				(void *)(unsigned long)
				(op_param->tmpref.buffer),
				memref->size);
			if (ret)
				IMSG_ERROR("failed to copy to user\n");

			kfree(op->params[n].tmpref.buffer);
			break;
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			memref = &params[n].memref;

			memref->size = op_param->memref.size;

			op_shm = op->params[n].memref.parent;
			op_shm_off = op->params[n].memref.offset;
			ret = copy_to_user(
				(char *)(unsigned long)(memref->buffer)
				+ op_shm_off,
				(char *)(unsigned long)(op_shm->buffer)
				+ op_shm_off,
				memref->size);
			if (ret)
				IMSG_ERROR("failed to copy to user\n");
			break;
		default:
			break;
		}
	}
}

static int do_capi_init_context(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Context *ctx = NULL;

	IMSG_PRINTK("TEEI: init capi_proxy_ctx START by thread (%d) \n", current->pid);

	ctx = teei_create_proxy_context();
	if (ctx == NULL) {
		IMSG_ERROR("Failed to create proxy ctx!\n");
		return -ENOMEM;
	}

	arg->ret = TEEC_InitializeContext(HOSTNAME, ctx);
	if (arg->ret) {
		teei_release_proxy_context();
		return -1;
	}

	IMSG_PRINTK("TEEI: init capi_proxy_ctx END by thread (%d) \n", current->pid);

	return 0;
}

static int do_capi_final_context(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Context *ctx = NULL;
	(void)arg;

	IMSG_PRINTK("TEEI: final capi_proxy_ctx START by thread (%d) \n", current->pid);

	ctx = teei_find_proxy_context();
	if (ctx == NULL) {
		IMSG_ERROR("Failed to find TEEC_context!\n");
		return -EINVAL;
	}

	TEEC_FinalizeContext(ctx);

	teei_release_proxy_context();

	IMSG_PRINTK("TEEI: final capi_proxy_ctx END by thread (%d) \n", current->pid);

	return 0;
}

static int do_capi_open_session(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Context *ctx = NULL;
	struct TEEC_Session *session;
	struct TEEC_Operation op;
	struct TEEC_UUID *uuid = (struct TEEC_UUID *)arg->uuid;
	int ret = 0;
	int sid;

	ctx = teei_find_proxy_context();
	if (ctx == NULL) {
		IMSG_ERROR("Failed to find TEEC_context!\n");
		return -EINVAL;
	}

	sid = alloc_capi_session();
	if (sid < 0) {
		IMSG_ERROR("no free capi session\n");
		return -ENOMEM;
	}

	session = &capi_session[sid];

	capi_params_to_op(arg->params, &op);

	arg->ret = TEEC_OpenSession(ctx, session, uuid,
			TEEC_LOGIN_PUBLIC, NULL, &op, &arg->ret_orig);

	capi_op_to_params(arg->params, &op);

	if (arg->ret) {
		ret = -EFAULT;
		goto err;
	}

	arg->session = (__u32)sid;
	return 0;

err:
	free_capi_session(sid);
	return ret;
}

static int do_capi_invoke_command(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Session *session;
	struct TEEC_Operation op;

	session = &capi_session[arg->session];

	capi_params_to_op(arg->params, &op);

	arg->ret = TEEC_InvokeCommand(session, arg->func, &op,
			&arg->ret_orig);

	capi_op_to_params(arg->params, &op);

	return (arg->ret) ? -EFAULT : 0;
}

static int do_capi_close_session(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Session *session;

	session = &capi_session[arg->session];

	TEEC_CloseSession(session);
	free_capi_session((int)arg->session);

	return 0;
}

static int do_capi_register_shm(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Context *ctx = NULL;
	struct TEEC_SharedMemory *shm;
	int ret = 0;
	int sid;

	ctx = teei_find_proxy_context();
	if (ctx == NULL) {
		IMSG_ERROR("Failed to find TEEC_context!\n");
		return -EINVAL;
	}

	sid = alloc_capi_shm();
	if (sid < 0) {
		IMSG_ERROR("no free capi shm\n");
		return -ENOMEM;
	}

	shm = &capi_shm[sid];
	shm->flags = arg->params[0].attr;
	shm->size = arg->params[0].memref.size;
	shm->buffer = kmalloc(shm->size, GFP_KERNEL);
	if (!shm->buffer) {
		IMSG_ERROR("failed to allocate buffer\n");
		ret = -ENOMEM;
		goto err;
	}

	arg->ret = TEEC_RegisterSharedMemory(ctx, shm);
	if (arg->ret) {
		ret = -EFAULT;
		goto err;
	}

	arg->params[0].memref.shm_id = (__u64)sid;
	return 0;

err:
	free_capi_shm(sid);
	return ret;
}

static int do_capi_allocate_shm(struct tee_ioctl_capi_proxy_arg *arg)
{
	struct TEEC_Context *ctx = NULL;
	struct TEEC_SharedMemory *shm;
	int ret = 0;
	int sid;

	ctx = teei_find_proxy_context();
	if (ctx == NULL) {
		IMSG_ERROR("Failed to find TEEC_context!\n");
		return -EINVAL;
	}

	sid = alloc_capi_shm();
	if (sid < 0) {
		IMSG_ERROR("no free capi shm\n");
		return -ENOMEM;
	}

	shm = &capi_shm[sid];

	shm->flags = arg->params[0].attr;
	shm->size = arg->params[0].memref.size;

	arg->ret = TEEC_AllocateSharedMemory(ctx, shm);
	if (arg->ret) {
		ret = -EFAULT;
		goto err;
	}

	arg->params[0].memref.shm_id = (__u64)sid;
	return 0;

err:
	free_capi_shm(sid);
	return ret;
}

static int do_capi_release_shm(struct tee_ioctl_capi_proxy_arg *arg)
{
	int sid = (int)arg->params[0].memref.shm_id;
	struct TEEC_SharedMemory *shm = &capi_shm[sid];

	if (shm->buffer && shm->shadow_buffer)
		kfree(shm->buffer);

	TEEC_ReleaseSharedMemory(shm);
	free_capi_shm(sid);

	return 0;
}

struct capi_op {
	int opcode;
	int (*callback)(struct tee_ioctl_capi_proxy_arg *arg);
};

static struct capi_op capi_ops[] = {
	{CAPI_OP_INIT_CONTEXT, do_capi_init_context},
	{CAPI_OP_FINAL_CONTEXT, do_capi_final_context},
	{CAPI_OP_OPEN_SESSION, do_capi_open_session},
	{CAPI_OP_INVOKE_COMMAND, do_capi_invoke_command},
	{CAPI_OP_CLOSE_SESSION, do_capi_close_session},
	{CAPI_OP_REGISTER_SHM, do_capi_register_shm},
	{CAPI_OP_ALLOCATE_SHM, do_capi_allocate_shm},
	{CAPI_OP_RELEASE_SHM, do_capi_release_shm},
};

static const size_t num_capi_ops = sizeof(capi_ops) / sizeof(struct capi_op);

int tee_ioctl_capi_proxy(struct tee_context *ctx,
			struct tee_ioctl_capi_proxy_arg __user *uarg)
{
	struct tee_ioctl_capi_proxy_arg arg;
	int ret;

	if (copy_from_user(&arg, uarg, sizeof(arg))) {
		IMSG_ERROR("failed to copy from user\n");
		return -EFAULT;
	}

	if (arg.capi_op >= num_capi_ops) {
		IMSG_ERROR("unknown CAPI operation (%u)\n",
				arg.capi_op);
		return -EINVAL;
	}

	ret = capi_ops[arg.capi_op].callback(&arg);

	if (copy_to_user(uarg, &arg, sizeof(arg))) {
		IMSG_ERROR("failed to copy to user\n");
		return -EFAULT;
	}

	return ret;
}

void teei_init_proxy_link(void)
{
	mutex_init(&proxy_link_mutex);
	mutex_init(&proxy_array_mutex);
	INIT_LIST_HEAD(&g_teei_proxy_link);

	ida_init(&capi_session_ida);
	ida_init(&capi_shm_ida);
}

