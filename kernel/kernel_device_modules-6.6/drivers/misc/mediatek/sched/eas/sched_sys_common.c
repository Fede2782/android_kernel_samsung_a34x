// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/topology.h>
#include <linux/sched/topology.h>
#include <trace/events/sched.h>
#include <trace/hooks/sched.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "common.h"
#include "sched_sys_common.h"

#define CREATE_TRACE_POINTS

static struct attribute *sched_ctl_attrs[] = {
#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
	&sched_core_pause_info_attr.attr,
	&sched_turn_point_freq_attr.attr,
	&sched_target_margin_attr.attr,
	&sched_target_margin_low_attr.attr,
	&sched_switch_adap_margin_low_attr.attr,
	&sched_runnable_boost_ctrl.attr,
#endif
#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
	&sched_prio_control.attr,
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	&sched_gas_for_ta.attr,
#endif
	NULL,
};

static struct attribute_group sched_ctl_attr_group = {
	.attrs = sched_ctl_attrs,
};

static struct kobject *kobj;
int init_sched_common_sysfs(void)
{
	struct device *dev_root = bus_get_dev_root(&cpu_subsys);
	int ret = 0;

	if (dev_root) {
		kobj = kobject_create_and_add("sched_ctl", &dev_root->kobj);
		if (!kobj) {
			pr_info("sched_ctl folder create failed\n");
			return -ENOMEM;
		}
		put_device(dev_root);
	}
	ret = sysfs_create_group(kobj, &sched_ctl_attr_group);
	if (ret)
		goto error;
	kobject_uevent(kobj, KOBJ_ADD);

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	task_rotate_init();
#endif

	return 0;

error:
	kobject_put(kobj);
	kobj = NULL;
	return ret;
}

void cleanup_sched_common_sysfs(void)
{
	if (kobj) {
		sysfs_remove_group(kobj, &sched_ctl_attr_group);
		kobject_put(kobj);
		kobj = NULL;
	}
}

static ssize_t show_sched_target_margin(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int cpu;

	for_each_possible_cpu(cpu)
		len += snprintf(buf+len, max_len-len,
		"high:cpu%d=%d low:cpu%d=%d\n", cpu, get_target_margin(cpu), cpu, get_target_margin_low(cpu));
	return len;
}

static ssize_t show_sched_turn_point_freq(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int cpu;

	for_each_possible_cpu(cpu)
		len += snprintf(buf+len, max_len-len,
		"cpu%d:turn point freq=%lu map util=%u\n", cpu,
		get_turn_point_freq_with_wl(cpu, DEFAULE_WL_TYPE),
		get_turn_point_util(cpu));
	return len;
}

static ssize_t show_sched_switch_adap_margin_low(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int cpu;

	for_each_possible_cpu(cpu)
		len += snprintf(buf+len, max_len-len,
		"cpu:%d switch_adap_margin_low=%d\n", cpu, get_switch_adap_margin_low(cpu));

	return len;
}

ssize_t show_sched_runnable_boost_ctrl(struct kobject *kobj,
struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len,
		"runnable_boost=%d, runnable_boost_cpumask=%lx\n",
		is_runnable_boost_enable(),get_runnable_boost_cpumask()->bits[0]);
	return len;
}

ssize_t store_sched_turn_point_freq(struct kobject *kobj, struct kobj_attribute *attr,
					const char __user *buf, size_t cnt)
{
	int cpu;
	int freq;

	if (sscanf(buf, "%d %d", &cpu, &freq) != 2)
		return -EINVAL;
	set_turn_point_freq_with_wl(cpu, freq, DEFAULE_WL_TYPE);
	return cnt;
}

ssize_t store_sched_target_margin(struct kobject *kobj, struct kobj_attribute *attr,
					const char __user *buf, size_t cnt)
{
	int cpu;
	int value;

	if (sscanf(buf, "%d %d", &cpu, &value) != 2)
		return -EINVAL;
	set_target_margin(cpu, value);
	return cnt;
}

ssize_t store_sched_target_margin_low(struct kobject *kobj, struct kobj_attribute *attr,
					const char __user *buf, size_t cnt)
{
	int cpu;
	int value;

	if (sscanf(buf, "%d %d", &cpu, &value) != 2)
		return -EINVAL;
	set_target_margin_low(cpu, value);
	return cnt;
}

#if IS_ENABLED(CONFIG_MTK_PRIO_TASK_CONTROL)
int sysctl_prio_control;
ssize_t store_sched_prio_control(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val != 0 && (val > 139 || val < 100))
		return -EINVAL;

	sysctl_prio_control = val;

	return cnt;
}

ssize_t show_sched_prio_control(struct kobject *kobj,
struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len, "%d\n", sysctl_prio_control);

	return len;
}

struct kobj_attribute sched_prio_control =
__ATTR(sched_prio_control, 0664, show_sched_prio_control, store_sched_prio_control);
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
static ssize_t show_sched_gas_for_ta(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len = snprintf(buf, max_len, "%d\n",  get_top_grp_aware());
	return len;
}

ssize_t store_sched_gas_for_ta(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int enable;

	if (kstrtouint(buf, 10, &enable))
		return -EINVAL;
	if (enable < 0 || enable > 1)
		return -1;
	set_top_grp_aware(enable, 0);
	return cnt;
}
#endif

ssize_t store_sched_switch_adap_margin_low(struct kobject *kobj, struct kobj_attribute *attr,
					const char __user *buf, size_t cnt)
{
	int cpu;
	int value;

	if (sscanf(buf, "%d %d", &cpu, &value) != 2)
		return -EINVAL;
	set_switch_adap_margin_low(cpu, value);
	return cnt;
}

static ssize_t store_sched_runnable_boost_ctrl(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *ubuf,
				size_t cnt)
{
	int enable;
	int cpus;

	if ((sscanf(ubuf, "%d %d", &enable, &cpus) != 2))
		return -EINVAL;
	set_runnable_boost_with_cpumask(enable, cpus);
	return cnt;
}

struct kobj_attribute sched_turn_point_freq_attr =
__ATTR(sched_turn_point_freq, 0640, show_sched_turn_point_freq, store_sched_turn_point_freq);
struct kobj_attribute sched_target_margin_attr =
__ATTR(sched_target_margin, 0640, show_sched_target_margin, store_sched_target_margin);
struct kobj_attribute sched_target_margin_low_attr =
__ATTR(sched_target_margin_low, 0640, show_sched_target_margin, store_sched_target_margin_low);
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
struct kobj_attribute sched_gas_for_ta =
__ATTR(gas_for_ta, 0640, show_sched_gas_for_ta, store_sched_gas_for_ta);
#endif
struct kobj_attribute sched_switch_adap_margin_low_attr =
__ATTR(sched_switch_adap_margin_low, 0640, show_sched_switch_adap_margin_low, store_sched_switch_adap_margin_low);
struct kobj_attribute sched_runnable_boost_ctrl =
__ATTR(sched_runnable_boost_ctrl, 0640, show_sched_runnable_boost_ctrl, store_sched_runnable_boost_ctrl);
