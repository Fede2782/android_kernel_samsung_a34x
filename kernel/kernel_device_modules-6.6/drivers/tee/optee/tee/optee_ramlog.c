// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/cpumask.h>
#include <linux/compat.h>
#include <linux/sizes.h>
#include <linux/tee_drv.h>
#include <linux/tee.h>

#include "optee_private.h"
#include "optee_smc.h"

#define tee_ramlog_write_rpoint(value)	\
		(*(unsigned int *)(tee_ramlog_buf_adr+4) = (value - tee_ramlog_buf_adr))
#define tee_ramlog_read_wpoint	\
		((*(unsigned int *)(tee_ramlog_buf_adr)) + tee_ramlog_buf_adr)
#define tee_ramlog_read_rpoint	\
		((*(unsigned int *)(tee_ramlog_buf_adr+4)) + tee_ramlog_buf_adr)
#define MAX_PRINT_SIZE			256
#define TEESMC32_ST_FASTCALL_RAMLOG	0xb200585C
#define TEE_IOCTL_RAMLOG_ADDR		_IO('t', 210)
#define RAMLOG_SIZE		SZ_1M
#define USER_ROOT_DIR		"mtk_tee"
#ifdef SET_USER_ID
#define VENDOR_TEE_GID		SET_USER_ID
#endif

static bool is_ramlog_crit_en;
module_param(is_ramlog_crit_en, bool, 0644);

#define ramlog_printk(fmt, args...)	\
	do { \
		if (is_ramlog_crit_en)	\
			pr_notice(fmt, ##args);	\
		else	\
			pr_info(fmt, ##args);	\
	} while (0)

static DEFINE_MUTEX(tee_ramlog_lock);
static struct task_struct *tee_ramlog_tsk;
static unsigned long tee_ramlog_buf_adr;
static unsigned long tee_ramlog_buf_len;
static unsigned long ramlog_vaddr;
static struct tee_context *mtk_tee_ctx;
static struct tee_shm *mtk_ramlog_buf;
#define SUPPORT 0
#define TA_CMD_REGISTER_RAMLOG 104

static int _optee_match(struct tee_ioctl_version_data *data, const void *vers)
{
	return 1;
}

static void optee_smccc_smc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static void optee_smccc_hvc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static optee_invoke_fn *get_invoke_func(struct device_node *np)
{
	const char *method;

	pr_info("probing for conduit method from DT.\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_notice("missing \"method\" property\n");
		return ERR_PTR(-ENXIO);
	}

	if (!strcmp("hvc", method))
		return optee_smccc_hvc;
	else if (!strcmp("smc", method))
		return optee_smccc_smc;

	pr_notice("invalid \"method\" property: %s\n", method);
	return ERR_PTR(-EINVAL);
}

static inline bool valid_ramlog_ptr(unsigned long ptr)
{
	if (ptr < tee_ramlog_buf_adr ||
		ptr > tee_ramlog_buf_adr + tee_ramlog_buf_len)
		return false;

	return true;
}

static void tee_ramlog_dump(void)
{
	char log_buf[MAX_PRINT_SIZE];
	char *log_buff_read_point = NULL;
	char *tmp_point = NULL;
	unsigned int log_count = 0;
	unsigned long log_buff_write_point = tee_ramlog_read_wpoint;

	log_buff_read_point = (char *)tee_ramlog_read_rpoint;
	if (!valid_ramlog_ptr(log_buff_write_point) ||
			!valid_ramlog_ptr((unsigned long)log_buff_read_point)) {
		pr_notice("tee ramlog: invalid read or write pointer\n");
		return;
	}
	if ((unsigned long)log_buff_read_point == log_buff_write_point)
		return;

	mutex_lock(&tee_ramlog_lock);

	while ((unsigned long)log_buff_read_point != log_buff_write_point) {
		if (isascii(*(log_buff_read_point)) &&
			*(log_buff_read_point) != '\0') {
			if (log_count >= MAX_PRINT_SIZE) {
				log_buf[log_count-2] = '\n';
				log_buf[log_count-1] = '\0';
				ramlog_printk("%s", log_buf);
				log_count = 0;
			} else {
				log_buf[log_count] = *(log_buff_read_point);
				log_count++;
				if (*(log_buff_read_point) == '\n') {
					if (log_count >= MAX_PRINT_SIZE) {
						log_buf[log_count-2] = '\n';
						log_buf[log_count-1] = '\0';
						ramlog_printk("%s", log_buf);
					} else {
						log_buf[log_count] = '\0';
						ramlog_printk("%s", log_buf);
					}
					log_count = 0;
				}
			}
		}

		log_buff_read_point++;
		tmp_point = (char *)(tee_ramlog_buf_adr+tee_ramlog_buf_len);
		if (log_buff_read_point == tmp_point) {
			tmp_point = (char *)(tee_ramlog_buf_adr + 8);
			log_buff_read_point = tmp_point;
		}
	}
	tee_ramlog_write_rpoint((unsigned long)log_buff_read_point);

	mutex_unlock(&tee_ramlog_lock);
}

static int tee_ramlog_loop(void *p)
{
	while (1) {
		tee_ramlog_dump();
		msleep(500);
	}
	return 0;
}

static int tee_ramlog_set_addr_len(uint64_t buf_adr, uint64_t buf_len)
{
	void *mapping_vaddr = NULL;

	if ((buf_adr == 0) || (buf_len == 0))
		return -EINVAL;

	if (tee_ramlog_tsk)
		return 0;

	mutex_lock(&tee_ramlog_lock);
	mapping_vaddr = ioremap_cache(buf_adr, buf_len);
	if (mapping_vaddr == NULL) {
		mutex_unlock(&tee_ramlog_lock);
		pr_info("[OPTEE][RAMLOG] %s %d\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	tee_ramlog_buf_adr = (unsigned long)mapping_vaddr;
	tee_ramlog_buf_len = buf_len;
	mutex_unlock(&tee_ramlog_lock);

	if (tee_ramlog_tsk == NULL)
		tee_ramlog_tsk = kthread_run(tee_ramlog_loop,
					NULL, "tee_ramlog_loop");

	return 0;
}

static int tee_dyn_ramlog_set_addr_len(struct tee_shm *shm, unsigned long buf_len)
{
	if ((shm == 0) || (buf_len == 0) || !ramlog_vaddr)
		return -EINVAL;

	if (tee_ramlog_tsk)
		return 0;

	mutex_lock(&tee_ramlog_lock);
	tee_ramlog_buf_adr = ramlog_vaddr;
	tee_ramlog_buf_len = buf_len;
	mutex_unlock(&tee_ramlog_lock);

	if (tee_ramlog_tsk == NULL)
		tee_ramlog_tsk = kthread_run(tee_ramlog_loop,
					NULL, "tee_dyn_ramlog");
	return 0;
}

static const struct of_device_id optee_match[] = {
	{ .compatible = "linaro,optee-tz" },
	{},
};

static unsigned long optee_disable_fastcall(optee_invoke_fn *invoke_fn)
{
	union {
		struct arm_smccc_res smccc;
		struct optee_smc_get_shm_config_result result;
	} res;

	invoke_fn(TEESMC32_ST_FASTCALL_RAMLOG, 0,
				0, 0, 0, 0, 0, 0, &res.smccc);

	pr_info("%s(%d): a0:%lx : disable OPTEE fastcall!\n",
		__func__, __LINE__, res.smccc.a0);
	return res.smccc.a0;
}

static unsigned long mtk_tee_probe(struct device_node *np)
{
	optee_invoke_fn *invoke_fn;

	invoke_fn = get_invoke_func(np);
	if (IS_ERR(invoke_fn))
		return -EINVAL;

	return optee_disable_fastcall(invoke_fn);
}

static ssize_t tz_write_ramlog_addr(struct file *filp,
			const char __user *buffer, size_t count, loff_t *ppos)
{
	char local_buf[256];
	char *const delim = " ";
	char *token, *cur;
	int i;
	uint64_t param_value[2];
	uint64_t val;

	if (count >= 256)
		return -EINVAL;

	if (copy_from_user(local_buf, buffer, count)) {
		pr_notice("%s %d copy_from_user fail\n", __func__, __LINE__);
		return -EFAULT;
	}

	local_buf[count] = 0;
	cur = local_buf;
	for (i = 0 ; i < ARRAY_SIZE(param_value) ; i++) {
		int ret;

		token = strsep(&cur, delim);
		if (!token) {
			pr_notice("%s %d token is NULL\n", __func__, __LINE__);
			return -EINVAL;
		}

		ret = kstrtoull(token, 16, &val);
		if (ret) {
			pr_notice("%s %d kstrtoull error:%d\n",
				__func__, __LINE__, ret);
			return ret;
		}
		param_value[i] = val;
	}

	if (mtk_ramlog_buf) {
		if (tee_dyn_ramlog_set_addr_len(mtk_ramlog_buf, RAMLOG_SIZE))
			return -EINVAL;
	} else {
		if (tee_ramlog_set_addr_len(param_value[0], param_value[1])) {
			pr_notice("%s - can't configure ramlog\n", __func__);
			return -EINVAL;
		}
	}
	return count;
}

static long tz_ioctl_ramlog(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case TEE_IOCTL_RAMLOG_ADDR:
		if (mtk_ramlog_buf)
			if (tee_dyn_ramlog_set_addr_len(mtk_ramlog_buf, RAMLOG_SIZE))
				return -EINTR;
		else
			return -EFAULT;
		break;
	default:
		pr_notice("%s - invalid ioctl command: 0x%x\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct proc_ops ramlog_fops = {
	.proc_write = tz_write_ramlog_addr,
	.proc_ioctl = tz_ioctl_ramlog,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = tz_ioctl_ramlog,
#endif
};

static int mtk_ramlog_open_session(uint32_t *session_id)
{
	uuid_t ramlog_uuid = UUID_INIT(0xa9aa0a93, 0xe9f5, 0x4234,
				0x8f, 0xec, 0x21, 0x09,
				0xcb, 0xa2, 0xf6, 0x70);
	int ret = 0;
	struct tee_ioctl_open_session_arg sess_arg;

	/* Open session with ramlog Trusted App */
	memset(&sess_arg, 0, sizeof(sess_arg));
	memcpy(sess_arg.uuid, ramlog_uuid.b, TEE_IOCTL_UUID_LEN);
	sess_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	sess_arg.num_params = 0;

	ret = tee_client_open_session(mtk_tee_ctx, &sess_arg, NULL);
	if ((ret < 0) || (sess_arg.ret != 0)) {
		pr_notice("tee_client_open_session failed, err: %x\n",
			sess_arg.ret);
		return -EINVAL;
	}
	*session_id = sess_arg.session;
	return 0;
}

static int mtk_register_ramlog(uint32_t sess, struct tee_shm *shm)
{
	int ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[4];

	memset(&inv_arg, 0, sizeof(inv_arg));
	memset(&param, 0, sizeof(param));

	/* Invoke TA_CMD_REGISTER_RAMLOG function of Trusted App */
	inv_arg.func = TA_CMD_REGISTER_RAMLOG;
	inv_arg.session = sess;
	inv_arg.num_params = 4;

	/* Fill invoke cmd params */
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;

	/* (value.a << 32) | (value.b) = pointer of shm */
#ifdef __aarch64__
	param[0].u.value.a = (uint32_t) ((uintptr_t)(void *)shm >> 32);
#else
	param[0].u.value.a = 0;
#endif
	param[0].u.value.b = (uint32_t) ((uintptr_t)(void *)shm & 0xFFFFFFFF);

	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = shm->size;

	ret = tee_client_invoke_func(mtk_tee_ctx, &inv_arg, param);
	if ((ret < 0) || (inv_arg.ret != 0)) {
		pr_notice("TA_CMD_REGISTER_RAMLOG invoke err: %x\n",
			inv_arg.ret);
		tee_shm_free(shm);
	}
	tee_client_close_session(mtk_tee_ctx, sess);
	tee_client_close_context(mtk_tee_ctx);
	return ret;
}

static int mtk_prepare_dyn_ramlog_buffer(void)
{
	uint32_t session_id = 0;
	struct optee *optee = NULL;
	struct tee_ioctl_version_data vers = {
		.impl_id = TEE_OPTEE_CAP_TZ,
		.impl_caps = TEE_IMPL_ID_OPTEE,
		.gen_caps = TEE_GEN_CAP_GP,
	};

	mtk_tee_ctx = tee_client_open_context(NULL, _optee_match, NULL, &vers);
	if (IS_ERR(mtk_tee_ctx)) {
		pr_notice("\033[1;31m[%s] context is NULL\033[m\n", __func__);
		return -EINVAL;
	}

	optee = tee_get_drvdata(mtk_tee_ctx->teedev);
	if (optee->smc.sec_caps & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM) {
		int ret = 0;

		ramlog_vaddr = (unsigned long)vmalloc(RAMLOG_SIZE);
		if (!ramlog_vaddr)
			return -ENOMEM;

		mtk_ramlog_buf = tee_shm_register_kernel_buf(mtk_tee_ctx, (void *)ramlog_vaddr,
							     RAMLOG_SIZE);
		if (IS_ERR(mtk_ramlog_buf)) {
			pr_notice("%s: tee_shm_register_kernel_buf failed with %ld\n",
				__func__, PTR_ERR(mtk_ramlog_buf));
			vfree((void *)ramlog_vaddr);
			ramlog_vaddr = 0;
			return PTR_ERR(mtk_ramlog_buf);
		}

		ret = mtk_ramlog_open_session(&session_id);
		if (ret) {
			pr_notice("\033[0;32;31m %s %d: open session Fail\033[m\n",
				__func__, __LINE__);
			vfree((void *)ramlog_vaddr);
			ramlog_vaddr = 0;
			tee_shm_free(mtk_ramlog_buf);
			return ret;
		}
		return mtk_register_ramlog(session_id, mtk_ramlog_buf);
	}
	pr_info("[MTK TEE] Static RAMLOG\n");

	return 0;
}

static int __init mtk_tee_init(void)
{
	struct proc_dir_entry *root;
	struct proc_dir_entry *dir;
	struct device_node *fw_np;
	struct device_node *np;
	unsigned long ramlog_supp = 0;

	fw_np = of_find_node_by_name(NULL, "firmware");
	if (!fw_np)
		return -ENODEV;

	np = of_find_matching_node(fw_np, optee_match);
	if (!np)
		return -ENODEV;

	ramlog_supp = mtk_tee_probe(np);
	of_node_put(np);

	root = proc_mkdir(USER_ROOT_DIR, NULL);
	if (!root) {
		pr_notice("%s(%d): create /proc/%s failed!\n",
			__func__, __LINE__, USER_ROOT_DIR);
		return -ENOMEM;
	}

	dir = proc_create("ramlog_setup", 0660, root, &ramlog_fops);
	if (!dir) {
		pr_notice("%s(%d): create /proc/%s/ramlog_setup failed!\n",
			__func__, __LINE__, USER_ROOT_DIR);
		return -ENOMEM;
	}
#ifdef SET_USER_ID
	proc_set_user(dir, KUIDT_INIT(VENDOR_TEE_GID), KGIDT_INIT(VENDOR_TEE_GID));
#endif

	if (ramlog_supp == SUPPORT) {
		if (mtk_prepare_dyn_ramlog_buffer())
			pr_notice("[RAMLOG] preparation was failed\n");
	}

	pr_info("mtk tee init done\n");
	return 0;
}
late_initcall(mtk_tee_init);

static void __exit mtk_tee_exit(void)
{
	if (mtk_ramlog_buf)
		tee_shm_free(mtk_ramlog_buf);
	if (ramlog_vaddr) {
		vfree((void *)ramlog_vaddr);
		ramlog_vaddr = 0;
	}
	remove_proc_subtree(USER_ROOT_DIR, NULL);
}
module_exit(mtk_tee_exit);

MODULE_AUTHOR("MediaTek");
MODULE_DESCRIPTION("MediaTek TEE Driver");
