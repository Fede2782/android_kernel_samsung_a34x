// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ktch]"fmt

#include <linux/cpufreq.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/pm_qos.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#include "ktch.h"


struct boost {
	spinlock_t touch_lock;
	wait_queue_head_t wq;
	struct task_struct *thread;
	int touch_event;
	atomic_t event;
};

/*--------------------------------------------*/

static struct boost ktchboost;

static int ktch_mgr_enable = 1;

static int *opp_count;
static unsigned int **opp_tbl;

struct cpufreq_policy **tchbst_policy;
struct freq_qos_request *tchbst_rq;
static int *target_freq;

static int policy_num;
static int touch_boost_opp; /* boost freq of touch boost */

/*--------------------FUNCTION----------------*/

static int cmp_uint(const void *a, const void *b)
{
	return *(unsigned int *)b - *(unsigned int *)a;
}

void set_freq(int enable)
{
	int i;

	/* boost */
	for (i = 0; i < policy_num; i++) {
		if (enable)
			freq_qos_update_request(&(tchbst_rq[i]), target_freq[i]);
		else
			freq_qos_update_request(&(tchbst_rq[i]), 0);
	}
}

static int ktchboost_thread(void *ptr)
{
	int event;
	unsigned long flags;

	set_user_nice(current, -10);

	while (!kthread_should_stop()) {

		while (!atomic_read(&ktchboost.event))
			wait_event(ktchboost.wq, atomic_read(&ktchboost.event));
		atomic_dec(&ktchboost.event);

		spin_lock_irqsave(&ktchboost.touch_lock, flags);
		event = ktchboost.touch_event;
		spin_unlock_irqrestore(&ktchboost.touch_lock, flags);
		set_freq(event);

	}
	return 0;
}

static ssize_t tb_enable_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	unsigned long flags;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > 1)
		return -1;

	spin_lock_irqsave(&ktchboost.touch_lock, flags);
	ktch_mgr_enable = val;
	spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

	return cnt;
}

static int tb_enable_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", ktch_mgr_enable);
	return 0;
}

static int tb_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, tb_enable_show, inode->i_private);
}

static const struct proc_ops tb_enable_fops = {
	.proc_open = tb_enable_open,
	.proc_write = tb_enable_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t tb_freq_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	int arg1 = -1, arg2 = -1;
	unsigned long flags;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%d %d", &arg1, &arg2) < 2)
		return -EFAULT;

	if (arg1 < 0 || arg2 < 0)
		return -EINVAL;

	if (arg1 < policy_num && arg2 < opp_count[arg1]) {
		spin_lock_irqsave(&ktchboost.touch_lock, flags);
		target_freq[arg1] = opp_tbl[arg1][arg2];
		spin_unlock_irqrestore(&ktchboost.touch_lock, flags);
		return cnt;
	} else
		return -1;
}

static int tb_freq_show(struct seq_file *m, void *v)
{
	int i;

	if (m) {
		for (i = 0; i < policy_num; i++)
			seq_printf(m, "cluster_opp[%d]:\t%d\n",
			i, target_freq[i]);
	}

	return 0;
}

static int tb_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, tb_freq_show, inode->i_private);
}

static const struct proc_ops tb_freq_fops = {
	.proc_open = tb_freq_open,
	.proc_write = tb_freq_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	unsigned long flags;

	if (!ktch_mgr_enable)
		return;

	if ((type == EV_KEY) && (code == BTN_TOUCH)) {
#if DEBUG_LOG
		pr_info("input cb, type:%d, code:%d, value:%d\n",
				type, code, value);
#endif
		spin_lock_irqsave(&ktchboost.touch_lock, flags);
		ktchboost.touch_event = value;
		spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

		atomic_inc(&ktchboost.event);
		wake_up(&ktchboost.wq);
	}
}

static int dbs_input_connect(struct input_handler *handler,
		struct input_dev *dev,
		const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "perfmgr";

	error = input_register_handle(handle);

	if (error)
		goto err2;

	error = input_open_device(handle);

	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void dbs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbs_ids[] = {
	{.driver_info = 1},
	{},
};

static struct input_handler dbs_input_handler = {
	.event = dbs_input_event,
	.connect = dbs_input_connect,
	.disconnect = dbs_input_disconnect,
	.name = "cpufreq_ond",
	.id_table = dbs_ids,
};

/*--------------------INIT------------------------*/

int init_ktch(void)
{
	struct proc_dir_entry *ktch_root = NULL;
	struct proc_dir_entry *tbe_dir, *tbf_dir;
	int handle, i;

	pr_info("init_ktch_touch, policy_num=%d\n", policy_num);

	touch_boost_opp = TOUCH_BOOST_OPP;

	target_freq = kcalloc(policy_num, sizeof(int), GFP_KERNEL);

	for (i = 0; i < policy_num; i++)
		target_freq[i] = opp_tbl[i][touch_boost_opp];

	/* create kernel touch root file */
	ktch_root = proc_mkdir("touch_boost", NULL);

	if (!ktch_root)
		pr_debug("ktch_root not create\n");

	/* touch */
	tbe_dir = proc_create("tb_enable", 0640, ktch_root,
			&tb_enable_fops);
	if (!tbe_dir)
		pr_debug("tbe_dir not create\n");

	tbf_dir = proc_create("tb_freq", 0640, ktch_root,
			&tb_freq_fops);
	if (!tbf_dir)
		pr_debug("tbf_dir not create\n");

	spin_lock_init(&ktchboost.touch_lock);
	init_waitqueue_head(&ktchboost.wq);
	atomic_set(&ktchboost.event, 0);
	ktchboost.thread = (struct task_struct *)kthread_run(ktchboost_thread,
			&ktchboost, "touch_boost");
	if (IS_ERR(ktchboost.thread))
		return -EINVAL;

	handle = input_register_handler(&dbs_input_handler);
	if (handle)
		pr_info("Failed to register input handler, handle %d", handle);

	return 0;
}

static int __init init_ktch_mod(void)
{
	int cpu;
	int num = 0, count;
#if DEBUG_LOG
	int i = 0;
#endif
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos;

	pr_info("init_ktch_touch\n");

	/* query policy number */
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d\n",
				__func__, num, cpu, policy->min, policy->max);

			num++;
			cpu = cpumask_last(policy->related_cpus);
		}
	}

	policy_num = num;

	if (policy_num == 0) {
		pr_info("%s, no policy", __func__);
		return 0;
	}

	tchbst_policy = kcalloc(policy_num,	sizeof(struct cpufreq_policy *), GFP_KERNEL);
	tchbst_rq = kcalloc(policy_num,	sizeof(struct freq_qos_request), GFP_KERNEL);
	opp_count = kcalloc(policy_num,	sizeof(int), GFP_KERNEL);
	opp_tbl = kcalloc(policy_num,	sizeof(int *), GFP_KERNEL);

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		tchbst_policy[num] = policy;
#if DEBUG_LOG
		pr_info("%s, policy[%d]: first:%d, sort:%d\n",
			__func__, num, cpu, (int)policy->freq_table_sorted);
#endif

		/* calc opp count */
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
#if DEBUG_LOG
			pr_info("%s, [%d]:%d", __func__, count, pos->frequency);
#endif
			count++;
		}
		opp_count[num] = count;
		pr_info("%s, policy[%d]: opp_count:%d\n", __func__, num, opp_count[num]);

		opp_tbl[num] = kcalloc(count, sizeof(int), GFP_KERNEL);
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			opp_tbl[num][count] = pos->frequency;
			count++;
		}

		sort(opp_tbl[num], opp_count[num], sizeof(unsigned int), cmp_uint, NULL);

#if DEBUG_LOG
		for (i = 0; i < opp_count[num]; i++) {
			pr_info("%s, policy[%d]: opp[%d]:%d\n",
				__func__, num, i, opp_tbl[num][i]);
		}
#endif

		/* freq QoS */
		freq_qos_add_request(&policy->constraints, &(tchbst_rq[num]), FREQ_QOS_MIN, 0);

		num++;
		cpu = cpumask_last(policy->related_cpus);
	}

	/* init procfs */
	init_ktch();

	return 0;
}

static void __exit exit_ktch_mod(void)
{
	int i;

	for (i = 0; i < policy_num; i++) {
		freq_qos_remove_request(&(tchbst_rq[i]));
		if (opp_tbl)
			kfree(opp_tbl[i]);
	}

	kfree(tchbst_policy);
	kfree(tchbst_rq);
	kfree(opp_count);
	kfree(opp_tbl);
}

module_init(init_ktch_mod);
module_exit(exit_ktch_mod);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch boost");
MODULE_AUTHOR("MediaTek Inc.");
