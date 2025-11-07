// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/reboot.h>
#include <linux/reboot-mode.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

#include "sec_hal.h"
#include "sec_osal.h"
#include "sec_mod.h"
#include "sec_boot_lib.h"
#include "sec_version.h"
#include <linux/printk.h>

#define MOD                         "MASP"

#define SEC_DEV_NAME                "mtk_sec"
#define SEC_MAJOR                   182

#define TRACE_FUNC()                MSG_FUNC(SEC_DEV_NAME)

enum MTK_SEC_KERNEL_OP {
	MTK_SEC_KERNEL_OP_GET_WRAPPER_KEY,
	MTK_SEC_KERNEL_OP_SET_WRAPPER_KEY,
	MTK_SEC_KERNEL_OP_RESET_WRAPPER_KEY,
	MTK_SEC_KERNEL_OP_NUM,
};

#define SEC_KERNEL_SEC_SEC_WRAPPER_LAST_IDX	(0x3E)

/*************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/
static struct sec_mod sec = { 0 };
static struct cdev sec_dev;
static struct class *sec_class;
static struct device *sec_device;
static const struct of_device_id masp_of_ids[] = {
	{.compatible = "mediatek,masp",},
	{}
};

/**************************************************************************
 *  SEC DRIVER OPEN
 **************************************************************************/
static int sec_open(struct inode *inode, struct file *file)
{
	return 0;
}

/**************************************************************************
 *  SEC DRIVER RELEASE
 **************************************************************************/
static int sec_release(struct inode *inode, struct file *file)
{
	return 0;
}

/**************************************************************************
 *  SEC DRIVER IOCTL
 **************************************************************************/
static long sec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return sec_core_ioctl(file, cmd, arg);
}

static const struct file_operations sec_fops = {
	.owner = THIS_MODULE,
	.open = sec_open,
	.release = sec_release,
	.write = NULL,
	.read = NULL,
	.unlocked_ioctl = sec_ioctl
};

/**************************************************************************
 *  SEC RID PROC FUNCTION
 **************************************************************************/
static int sec_proc_rid_show(struct seq_file *m, void *v)
{
	unsigned int rid[4] = { 0 };
	unsigned int i = 0;

	sec_get_random_id((unsigned int *)rid);

	for (i = 0; i < 16; i++)
		seq_putc(m, *((char *)rid + i));

	return 0;
}

static int sec_proc_rid_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_proc_rid_show, NULL);
}

static const struct proc_ops sec_proc_rid_fops = {
	.proc_open = sec_proc_rid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/**************************************************************************
 * get and show SOC ID from dts node
 **************************************************************************/
static int sec_proc_soc_id_show(struct seq_file *m, void *v)
{
	unsigned int i = 0;

	if (m == NULL)
		return -EINVAL;

	if (v == NULL)
		return -EINVAL;

	seq_printf(m, "SOC_ID: ");
	for (i = 0; i < NUM_SOC_ID_IN_BYTES; i++)
		seq_printf(m, "%02x", g_soc_id[i]);
	seq_printf(m, "\n");

	return 0;
}

static int sec_proc_soc_id_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_proc_soc_id_show, NULL);
}

static const struct proc_ops sec_proc_soc_id_fops = {
	.proc_open = sec_proc_soc_id_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

#if IS_ENABLED(CONFIG_MTK_SEC_DEBUG_SUPPORT)
/**************************************************************************
 *  SEC WRAPPER_KEY PROC FUNCTION
 **************************************************************************/
#define SEC_KERNEL_PROC_KEY_SLOT            (64)
#define SEC_KERNEL_PROC_KEY_LEN             (256)
#define SEC_KERNEL_SET_WRAPPER_KEY_ACK_SYNC	(0x1000)
#define SEC_KERNEL_GET_WRAPPER_KEY_ACK_SYNC	(0x3000)
#define SEC_KERNEL_WRAPPER_KEY_IDX_MASK		(0xFF000000)
#define SEC_KERNEL_WRAPPER_KEY_IDX_OFFSET	(24)

#define SEC_KERNEL_PROC_INPUT_MAX_LEN       (520)

unsigned int sec_valid_rsa_bytes;

static int sec_proc_wrapper_key_get(struct seq_file *m, void *v)
{
	unsigned int idx, opcode;
	unsigned int get_times = (SEC_KERNEL_PROC_KEY_LEN /4);
	unsigned int rsa_key[SEC_KERNEL_PROC_KEY_SLOT];
	unsigned char *pos;
	struct arm_smccc_res res;
	static DEFINE_MUTEX(wrapper_key_mutex);

	seq_puts(m, "get wrapper key start.\n");

	mutex_lock(&wrapper_key_mutex);
	memset(&rsa_key[0], 0x0, sizeof(rsa_key));

	if (get_times > SEC_KERNEL_PROC_KEY_SLOT) {
		seq_puts(m, "Error: get_times exceeds buffer size\n");
		mutex_unlock(&wrapper_key_mutex);
		return -EINVAL;
	}

	for(idx=0; idx < get_times; idx+=2) {

		opcode =idx;

		arm_smccc_smc(	MTK_SIP_KERNEL_SEC_CONTROL,
						MTK_SEC_KERNEL_OP_GET_WRAPPER_KEY,
						opcode, 0, 0, 0, 0, 0, &res);

		/*  res.a0 act as ack check */
		if((res.a0) != (opcode + SEC_KERNEL_GET_WRAPPER_KEY_ACK_SYNC)) {
			seq_printf(m, "[%s] Error!! opcode not match. opcode =0x%x, res =0x%lx\n",
			__func__, opcode, (res.a0));
			mutex_unlock(&wrapper_key_mutex);
			return -EINVAL;
		}

		/* seq_printf(m, "[%s]: opcode=0x%x, a1=0x%lx, a2=0x%lx, res=0x%lx\n",
		 *	__func__, opcode, (res.a1), (res.a2), (res.a0));
		 */

		rsa_key[idx]=res.a1;
		rsa_key[idx+1]=res.a2;
	}

	seq_puts(m, "Encrypted Wrapper key:");
	pos = (unsigned char *)&(rsa_key[0]);
	for (idx = 0; idx < SEC_KERNEL_PROC_KEY_LEN; idx++)
		seq_printf(m, "%02x", *pos++);

	seq_puts(m, "\n");

	seq_puts(m, "get wrapper key done.\n");
	mutex_unlock(&wrapper_key_mutex);
	return 0;
}

char sec_desc[SEC_KERNEL_PROC_INPUT_MAX_LEN];
char sec_cmd[SEC_KERNEL_PROC_INPUT_MAX_LEN];

static ssize_t sec_proc_wrapper_key_set(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int len = 0;
	unsigned int idx;
	unsigned int set_pattern[2];
	unsigned int opcode;
	unsigned int set_times = (SEC_KERNEL_PROC_KEY_LEN /4);
	unsigned char kwrapper_index;
	unsigned char *pos;
	unsigned int copy_size;

	struct arm_smccc_res res;
	spinlock_t lock;

	spin_lock_init(&lock);

	if (count <= 0 || count > sizeof(sec_desc)) {
		pr_notice("[%s] command input error, count= %zu bytes\n", __func__, count);
		return -EINVAL;
	}

	len = (count < (sizeof(sec_desc))) ? count : (sizeof(sec_desc) -1);
	memset(sec_desc, 0x0, sizeof(sec_desc));

	pr_notice("[%s] command len= %d bytes, max=%d bytes\n", __func__, len, (SEC_KERNEL_PROC_INPUT_MAX_LEN-1));

	if (copy_from_user(sec_desc, buffer, len)) {
		pr_err("[%s] error, copy_from_user failed\n", __func__);
		return -EINVAL;
	}

	sec_desc[len] = '\0';

	copy_size = ((SEC_KERNEL_PROC_KEY_LEN +1)*2);
	if ((copy_size <= sizeof(sec_cmd))
		&& (copy_size <= sizeof(sec_desc))) {
		memcpy(sec_cmd, sec_desc, ((SEC_KERNEL_PROC_KEY_LEN +1)*2));
	} else {
		pr_err("[%s] error, size overflow: copy_size=%d\n", __func__, copy_size);
		return -EINVAL;
	}

	pos=&(sec_cmd[0]);

	if (sscanf(pos, "%2hhx", &kwrapper_index)<=0) {
		pr_err("[%s] error, input error\n", __func__);
		return -EINVAL;
	}
	pos+=2;
	pr_notice("[%s] kwrapper_index =%d\r\n", __func__, kwrapper_index);

	for (idx=0; idx<SEC_KERNEL_PROC_KEY_LEN; idx++) {
		if (sscanf(pos, "%2hhx", &sec_desc[idx])<=0) {
			pr_err("[%s] error, input error\n", __func__);
			return -EINVAL;
		}
		pos+=2;
	}

	/* Set pattern */
	spin_lock(&lock);
	for(idx=0; idx < set_times; idx+=2) {
		opcode =idx;

		if (idx == SEC_KERNEL_SEC_SEC_WRAPPER_LAST_IDX)
			opcode |=  ((kwrapper_index) << SEC_KERNEL_WRAPPER_KEY_IDX_OFFSET);

		memcpy(&set_pattern[0], &sec_desc[idx*4], 4);
		memcpy(&set_pattern[1], &sec_desc[((idx+1)*4)], 4);

		arm_smccc_smc(	MTK_SIP_KERNEL_SEC_CONTROL,
						MTK_SEC_KERNEL_OP_SET_WRAPPER_KEY,
						opcode, set_pattern[0], set_pattern[1],
						0, 0, 0, &res);

		pr_notice("[%s] setting key: opcode=0x%x, p1=0x%x, p2=0x%x, res=0x%lx\n",
		__func__, opcode, set_pattern[0], set_pattern[1], (res.a0));

		/*  res.a0 act as ack check */
		if((res.a0))
			pr_err("[%s]: Error!! res =0x%lx\n", __func__, (res.a0));
	}
	spin_unlock(&lock);

	pr_notice("wrapper key setting done.\r\n");

	return count;
}

static int sec_proc_wrapper_key_show(struct inode *inode, struct file *file)
{
	return single_open(file, sec_proc_wrapper_key_get, NULL);
}


static const struct proc_ops sec_proc_wrapper_key_fops = {
	.proc_open = sec_proc_wrapper_key_show,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
	.proc_write = sec_proc_wrapper_key_set,
};
#endif

/**************************************************************************
 *  set_dmverity_reboot eio flag
 **************************************************************************/
// notify_call function
static int reboot_handler_set_eio_flag(struct notifier_block *reboot,
						unsigned long mode,
					  void *cmd)
{	int ret = 0;
	const char *dm_error_cmd = "dm-verity device corrupted";

	if (cmd && !strcmp(cmd, dm_error_cmd))
		ret = masp_hal_set_dm_verity_error();
	return ret;
}

static struct notifier_block reboot_handler_notifier = {
	.notifier_call = reboot_handler_set_eio_flag,
};

/**************************************************************************
 *  SEC MODULE PARAMETER
 **************************************************************************/
static uint recovery_done;
module_param(recovery_done, uint, 0664); /* rw-rw-r-- */

MODULE_PARM_DESC(recovery_done,
		 "recovery_done status(0 = complete, 1 = on-going, 2 = error)");

/* SEC DRIVER INIT */
static int sec_init(struct platform_device *dev)
{
	int ret = 0;
	dev_t id = {0};
	struct proc_dir_entry *entry = NULL;

	pr_debug("[%s] %s (%d)\n", SEC_DEV_NAME, __func__, ret);

	id = MKDEV(SEC_MAJOR, 0);
	ret = register_chrdev_region(id, 1, SEC_DEV_NAME);

	if (ret) {
		pr_notice("[%s] Regist Failed (%d)\n", SEC_DEV_NAME, ret);
		return ret;
	}

	sec_class = class_create(SEC_DEV_NAME);
	if (IS_ERR(sec_class)) {
		ret = PTR_ERR(sec_class);
		pr_notice("[%s] Create class failed(0x%x)\n",
			  SEC_DEV_NAME,
			  ret);
		goto err_chedev;
	}

	cdev_init(&sec_dev, &sec_fops);
	sec_dev.owner = THIS_MODULE;

	ret = cdev_add(&sec_dev, id, 1);
	if (ret) {
		pr_notice("[%s] Cdev_add Failed (0x%x)\n",
			SEC_DEV_NAME,
			ret);
		goto err_class;
	}

	sec_device = device_create(sec_class, NULL, id, NULL, SEC_DEV_NAME);
	if (IS_ERR(sec_device)) {
		ret = PTR_ERR(sec_device);
		pr_notice("[%s] Create device failed(0x%x)\n",
			  SEC_DEV_NAME,
			  ret);
		goto err_cdev;
	}

	sec.id = id;
	sec.init = 1;
	spin_lock_init(&sec.lock);

	entry = proc_create("rid", 0444, NULL, &sec_proc_rid_fops);

	if (!entry) {
		ret = -ENOMEM;
		pr_notice("[%s] Create /proc/rid failed(0x%x)\n",
			  SEC_DEV_NAME,
			  ret);
		goto err_device;
	}

	entry = proc_create("soc_id", 0444, NULL, &sec_proc_soc_id_fops);
	if (!entry) {
		ret = -ENOMEM;
		pr_notice("[%s] Create /proc/soc_id failed(0x%x)\n",
			  SEC_DEV_NAME,
			  ret);
		goto err_device;
	}

#if IS_ENABLED(CONFIG_MTK_SEC_DEBUG_SUPPORT)
	entry = proc_create("wrapper_key", 0644, NULL, &sec_proc_wrapper_key_fops);
	if (!entry) {
		ret = -ENOMEM;
		pr_notice("[%s] Create /proc/wrapper_key failed(0x%x)\n",
			  SEC_DEV_NAME,
			  ret);
		goto err_device;
	}
#endif

	return ret;

err_device:
	device_destroy(sec_class, id);
err_cdev:
	cdev_del(&sec_dev);
err_class:
	class_destroy(sec_class);
err_chedev:
	unregister_chrdev_region(id, 1);
	memset(&sec, 0, sizeof(sec));

	return ret;
}


/**************************************************************************
 *  SEC DRIVER EXIT
 **************************************************************************/
static void sec_exit(void)
{
	remove_proc_entry("rid", NULL);
	device_destroy(sec_class, sec.id);
	cdev_del(&sec_dev);
	class_destroy(sec_class);
	unregister_chrdev_region(sec.id, 1);
	memset(&sec, 0, sizeof(sec));

	sec_core_exit();
}

/**************************************************************************
 *  MASP PLATFORM DRIVER WRAPPER, FOR BUILD-IN SEQUENCE
 **************************************************************************/
int masp_probe(struct platform_device *dev)
{
	int ret = 0;

	ret = sec_init(dev);
	return ret;
}


int masp_remove(struct platform_device *dev)
{
	sec_exit();
	return 0;
}

static struct platform_driver masp_driver = {
	.driver = {
		.name = "masp",
		.owner = THIS_MODULE,
		.of_match_table = masp_of_ids,
	},
	.probe = masp_probe,
	.remove = masp_remove,
};

static int __init masp_get_from_dts(void)
{
	struct masp_tag *tags;
	int i;
	struct device_node *np_chosen = NULL;

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen) {

		np_chosen = of_find_node_by_path("/chosen@0");
		if (!np_chosen)
			return 1;
	}

	tags = (struct masp_tag *)
			of_get_property(np_chosen, "atag,masp", NULL);


	if (!tags)
		return 1;

	g_rom_info_sbc_attr = tags->rom_info_sbc_attr;
	g_rom_info_sdl_attr = tags->rom_info_sdl_attr;
	g_hw_sbcen = tags->hw_sbcen;
	g_lock_state = tags->lock_state;
	lks = tags->lock_state;
	for (i = 0; i < NUM_RID; i++)
		g_random_id[i] = tags->rid[i];
	for (i = 0; i < NUM_CRYPTO_SEED; i++)
		g_crypto_seed[i] = tags->crypto_seed[i];
	for (i = 0; i < NUM_SBC_PUBK_HASH; i++)
		g_sbc_pubk_hash[i] = tags->sbc_pubk_hash[i];
	for (i = 0; i < NUM_SOC_ID_IN_BYTES; i++)
		g_soc_id[i] = tags->soc_id[i];

	return 0;
}


static int __init masp_init(void)
{
	int ret;

	masp_get_from_dts();
	ret = platform_driver_register(&masp_driver);
	if (ret) {
		pr_notice("[%s] Reg platform driver failed (%d)\n",
			  SEC_DEV_NAME,
			  ret);
		return ret;
	}
	register_reboot_notifier(&reboot_handler_notifier);
	return ret;
}

static void __exit masp_exit(void)
{
	platform_driver_unregister(&masp_driver);
	unregister_reboot_notifier(&reboot_handler_notifier);
}


module_init(masp_init);
module_exit(masp_exit);

/**************************************************************************
 *  EXPORT FUNCTION
 **************************************************************************/
EXPORT_SYMBOL(sec_get_random_id);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek Kernel Security Module");
