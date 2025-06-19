// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include "adsp_reserved_mem.h"
#include "adsp_feature_define.h"
#include "adsp_platform_driver.h"
#include "adsp_logger.h"
#include "adsp_excep.h"
#include "adsp_core.h"
#include "adsp_qos.h"

/* ----------------------------- sys fs ------------------------------------ */
static inline ssize_t dev_dump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);
	int n = 0;

	n +=  scnprintf(buf + n, PAGE_SIZE - n, "name:%s id = %d\n",
			pdata->name, pdata->id);
	n +=  scnprintf(buf + n, PAGE_SIZE - n,
			"status = %d, feature_set = %llX\n",
			pdata->state, pdata->feature_set);

	if (pdata->send_mbox)
		n +=  scnprintf(buf + n, PAGE_SIZE - n, "mailbox send = %d\n",
			pdata->send_mbox->mbox);
	if (pdata->recv_mbox)
		n +=  scnprintf(buf + n, PAGE_SIZE - n, "mailbox recv = %d\n",
			pdata->recv_mbox->mbox);

	return n;
}
DEVICE_ATTR_RO(dev_dump);

static void ipi_test_handler(int id, void *data, unsigned int len)
{
	unsigned long long ns = *(unsigned long long *)data;

	pr_info("%s, receive ipi_test from adsp, adsp_time[%llu.%llu]",
		__func__, ns / 1000000000, (ns / 1000000) % 1000);
}

static inline ssize_t ipi_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);
	int value = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	if (value == 0xee && is_aed_work_busy()) {
		pr_info("[ADSP] aed_work busy skip EE test.\n");
		return -EBUSY;
	}

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		adsp_push_message(ADSP_IPI_TEST1, &value, sizeof(value),
				  20, pdata->id);

		_adsp_deregister_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
	}

	return count;
}

static inline ssize_t ipi_test_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);
	unsigned int value = 0x5A5A;
	int ret;

	adsp_ipi_registration(ADSP_IPI_TEST1, ipi_test_handler, "ipi_test");

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		pr_info("%s, send ipi_test to adsp%d", __func__, pdata->id);

		ret = adsp_push_message(ADSP_IPI_TEST1, &value,
					sizeof(value), 20, pdata->id);
		_adsp_deregister_feature(pdata->id,
					 SYSTEM_FEATURE_ID, 0);
		return scnprintf(buf, PAGE_SIZE, "ADSP ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP register fail\n");
}
DEVICE_ATTR_RW(ipi_test);

static inline ssize_t suspend_cmd_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	return adsp_dump_feature_state(pdata->id, buf, PAGE_SIZE);
}

static inline ssize_t suspend_cmd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t fid = 0;
	char *temp = NULL, *token1 = NULL, *token2 = NULL;
	char *pin = NULL;
	char delim[] = " ,\t\n";
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	temp = kstrdup(buf, GFP_KERNEL);
	pin = temp;
	token1 = strsep(&pin, delim);
	token2 = strsep(&pin, delim);

	if (!token1 || !token2)
		goto EXIT;

	fid = adsp_get_feature_index(token2);

	if (fid == SYSTEM_FEATURE_ID)
		goto EXIT;

	if (!is_feature_in_set(pdata->id, fid))
		goto EXIT;

	if (strcmp(token1, "regi") == 0)
		_adsp_register_feature(pdata->id, fid, 0);

	if (strcmp(token1, "deregi") == 0)
		_adsp_deregister_feature(pdata->id, fid, 0);

EXIT:
	kfree(temp);
	return count;
}
DEVICE_ATTR_RW(suspend_cmd);

static inline ssize_t log_enable_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	return adsp_dump_log_state(pdata->log_ctrl, buf, PAGE_SIZE);
}

static inline ssize_t log_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int enable = 0;
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	adsp_log_enable(pdata->log_ctrl, pdata->id, enable);

	return count;
}
DEVICE_ATTR_RW(log_enable);

static struct attribute *adsp_default_attrs[] = {
	&dev_attr_dev_dump.attr,
	&dev_attr_ipi_test.attr,
	&dev_attr_suspend_cmd.attr,
	&dev_attr_log_enable.attr,
	NULL,
};

struct attribute_group adsp_default_attr_group = {
	.attrs = adsp_default_attrs,
};

/* ---------------------------- debug fs ----------------------------------- */

static ssize_t adsp_debug_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *pos)
{
	char *buffer = NULL;
	size_t n = 0, max_size;
	struct adsp_priv *pdata = filp->private_data;
	u32 memid = ADSP_A_DEBUG_DUMP_MEM_ID;

	if (pdata->id == ADSP_B_ID)
		memid = ADSP_B_DEBUG_DUMP_MEM_ID;

	buffer = adsp_get_reserve_mem_virt(memid);
	max_size = adsp_get_reserve_mem_size(memid);

	if (buffer)
		n = strnlen(buffer, max_size);

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t adsp_debug_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[64] = {0};
	struct adsp_priv *pdata = filp->private_data;

	if (copy_from_user(buf, buffer, min(count, sizeof(buf) - 1)))
		return -EFAULT;

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		adsp_push_message(ADSP_IPI_ADSP_TIMER,
				buf, strnlen(buf, sizeof(buf) - 1) + 1, 0, pdata->id);

		_adsp_deregister_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
	}

	return count;
}

const struct file_operations adsp_debug_ops = {
	.open = simple_open,
	.read = adsp_debug_read,
	.write = adsp_debug_write,
};

/* ---------------------------- trace fs ----------------------------------- */
#define ADSP_TRACE_SIZE   (256 * 1024) /* bottom of debug memory, Must align firmware */

static void adsp_trace_update_w_ptr(int id, void *buf, unsigned int len)
{
	struct adsp_priv *pdata = NULL;
	u32 *data = (u32 *)buf;
	unsigned int cid, size, flag;

	if (!buf)
		return;

	cid = data[0];
	size = data[1];
	flag = data[2];

	pdata = get_adsp_core_by_id(cid);
	if (!pdata)
		return;

	pdata->tracefifo.kfifo.in += size;

	if (flag) { /* TRACE_GOING = 1 */
		/* drop data if it don't have space for next update */
		/* TODO: LOCK reader */
		if (kfifo_avail(&pdata->tracefifo) < size) {
			pdata->tracefifo.kfifo.out += size;
			pr_debug("%s(), drop data size:%u, len:%d, avail:%d", __func__, size,
				 kfifo_len(&pdata->tracefifo),
				 kfifo_avail(&pdata->tracefifo));
		}
	}

	pr_debug("%s(), return size:+%d, len:%d, avail:%d", __func__, size,
		 kfifo_len(&pdata->tracefifo),
		 kfifo_avail(&pdata->tracefifo));

	/* wakeup if trace_poll */
	wake_up_interruptible(&adspsys->waitq);
}

static unsigned int adsp_trace_poll(struct file *filp, poll_table *wait)
{
	struct adsp_priv *pdata = filp->private_data;

	if (!(filp->f_mode & FMODE_READ))
		return POLLERR;

	poll_wait(filp, &adspsys->waitq, wait);

	if (!kfifo_is_empty(&pdata->tracefifo))
		return POLLIN | POLLRDNORM;

	return 0;
}

static int adsp_trace_open(struct inode *inode, struct file *file)
{
	struct adsp_priv *pdata = inode->i_private;
	char *buffer = NULL;
	size_t size;
	unsigned int memid;

	if (!inode->i_private)
		return -ENODEV;

	file->private_data = inode->i_private;

	if (!kfifo_initialized(&pdata->tracefifo)) {
		/* trace kfifo initialize, use bottom of debug memory */
		if (pdata->id == ADSP_A_ID)
			memid = ADSP_A_DEBUG_DUMP_MEM_ID;
		else
			memid = ADSP_B_DEBUG_DUMP_MEM_ID;

		buffer = adsp_get_reserve_mem_virt(memid);
		size = adsp_get_reserve_mem_size(memid);

		if (!buffer || size < ADSP_TRACE_SIZE)
			return -ENOMEM;

		buffer += size - ADSP_TRACE_SIZE;
		kfifo_init(&pdata->tracefifo, buffer, ADSP_TRACE_SIZE);

		adsp_ipi_registration(ADSP_IPI_TRAX_DONE,
				      adsp_trace_update_w_ptr, "trace_update_w");

		pr_debug("%s(), init done %d, %p, %zu", __func__,
			 kfifo_initialized(&pdata->tracefifo), buffer, size);
	}

	return nonseekable_open(inode, file);
}

static ssize_t adsp_trace_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *pos)
{
	struct adsp_priv *pdata = filp->private_data;
	unsigned int copied;
	int ret;

	/* TODO: LOCK reader */
	ret = kfifo_to_user(&pdata->tracefifo, buf, count, &copied);

	pr_debug("%s(), ret %d, copied %u", __func__, ret, copied);
	return ret ? ret : copied;
}

static ssize_t adsp_trace_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[64] = {0};
	unsigned int enable = 0;
	struct adsp_priv *pdata = filp->private_data;

	if (kstrtouint_from_user(buffer, count, 0, &enable) != 0)
		return -EINVAL;

	if (enable) {
		kfifo_reset(&pdata->tracefifo);
		strscpy(buf, "trace_start", sizeof(buf) - 1);
	} else {
		strscpy(buf, "trace_stop", sizeof(buf) - 1);
	}

	pr_info("%s(), send '%s' to adsp kfifo:%d/%d", __func__, buf,
		kfifo_avail(&pdata->tracefifo),
		kfifo_size(&pdata->tracefifo));

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		adsp_push_message(ADSP_IPI_ADSP_TIMER, buf, sizeof(buf), 0, pdata->id);
		_adsp_deregister_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
	}

	return count;
}

const struct file_operations adsp_trace_ops = {
	.open = adsp_trace_open,
	.read = adsp_trace_read,
	.write = adsp_trace_write,
	.poll = adsp_trace_poll,
	.llseek = noop_llseek,
};

/* ------------------------------ misc device ----------------------------- */
/*==============================================================================
 *                     ioctl
 *==============================================================================
 */
#define AUDIO_DSP_IOC_MAGIC 'a'
#define AUDIO_DSP_IOCTL_ADSP_REG_FEATURE  \
	_IOW(AUDIO_DSP_IOC_MAGIC, 0, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS \
	_IOR(AUDIO_DSP_IOC_MAGIC, 1, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_RESET_CBK \
	_IOR(AUDIO_DSP_IOC_MAGIC, 2, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_HRT_BW \
	_IOR(AUDIO_DSP_IOC_MAGIC, 3, unsigned int)

union ioctl_param {
	struct {
		uint16_t enable;
		uint16_t fid;
	} cmd0;
	struct {
		int16_t flag;
		uint16_t cid;
	} cmd1;
	struct {
		uint16_t set;
		uint16_t scene;
	} cmd2;
};

/* user-space feature reset */
static int adsp_hal_feature_table[ADSP_NUM_FEATURE_ID];
void reset_hal_feature_table(void)
{
	int i;

	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		while (adsp_hal_feature_table[i] > 0) {
			adsp_deregister_feature(i);
			adsp_hal_feature_table[i]--;
		}
	}
}
EXPORT_SYMBOL(reset_hal_feature_table);

/* file operations */
static ssize_t adsp_driver_read(struct file *filp, char __user *data,
				  size_t len, loff_t *ppos)
{
	struct adsp_priv *pdata = container_of(filp->private_data,
					struct adsp_priv, mdev);

	return adsp_log_read(pdata->log_ctrl, data, len);
}
static int adsp_driver_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static unsigned int adsp_driver_poll(struct file *filp,
				     struct poll_table_struct *poll)
{
	struct adsp_priv *pdata = container_of(filp->private_data,
					struct adsp_priv, mdev);

	if (!pdata->log_ctrl || !pdata->log_ctrl->inited)
		return 0;

	if (!(filp->f_mode & FMODE_READ))
		return 0;

	return adsp_log_poll(pdata->log_ctrl);
}

static long adsp_driver_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	union ioctl_param t;

	switch (cmd) {
	case AUDIO_DSP_IOCTL_ADSP_REG_FEATURE: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		if (t.cmd0.fid == SYSTEM_FEATURE_ID) {
			pr_debug("%s(), reg/dreg SYSTEM_FEATURE_ID is not permitted!\n", __func__);
			ret = -EPERM;
			break;
		}

		if (t.cmd0.enable)
			ret = adsp_register_feature(t.cmd0.fid);
		else
			ret = adsp_deregister_feature(t.cmd0.fid);

		if (ret == 0) {
			if (t.cmd0.enable)
				adsp_hal_feature_table[t.cmd0.fid]++;
			else
				adsp_hal_feature_table[t.cmd0.fid]--;

			if (adsp_hal_feature_table[t.cmd0.fid] < 0)
				adsp_hal_feature_table[t.cmd0.fid] = 0;
		}
		break;
	}
	case AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		t.cmd1.flag = is_adsp_ready(t.cmd1.cid);

		if (copy_to_user((void __user *)arg, &t, sizeof(t))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_DSP_IOCTL_ADSP_HRT_BW: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		ret = adsp_icc_bw_req(t.cmd2.scene, t.cmd2.set);
		break;
	}
	default:
		pr_debug("%s(), invalid ioctl cmd\n", __func__);
	}

	if (ret < 0)
		pr_info("%s(), ioctl error %d\n", __func__, ret);

	return ret;
}

static long adsp_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_notice("op null\n");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}

const struct file_operations adspsys_file_ops = {
	.owner = THIS_MODULE,
	.open = adsp_driver_open,
	.unlocked_ioctl = adsp_driver_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = adsp_driver_compat_ioctl,
#endif
};

const struct file_operations adsp_core_file_ops = {
	.owner = THIS_MODULE,
	.read = adsp_driver_read,
	.open = adsp_driver_open,
	.poll = adsp_driver_poll,
};

