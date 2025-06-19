// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

#include <linux/cpumask.h>
#include <linux/tick.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <trace/events/power.h>
#include <uapi/linux/sched/types.h>
#include "../../../kernel/power/power.h"


#undef pr_fmt
#define pr_fmt(fmt) "[wcpu_alloc] " fmt

#define INVALID_SAVED_PRIO	(-1)
#define PM_TASK_RT_PRIO		(1)
#define PMS_TASK_RT
// #define DEBUG_ENABLE

static struct cpumask fast_cpu_mask;
static struct cpumask backup_cpu_mask;
static int start_cpu, end_cpu;
static struct task_struct *pms_task;
#if defined(PMS_TASK_RT)
static int saved_prio = INVALID_SAVED_PRIO;
static int prev_policy;
static unsigned int rt_on = 1;
#endif

static int sec_wcpu_alloc_pm_notifier(struct notifier_block *notifier,
				      unsigned long pm_event, void *dummy);

static int __start_boosting(void);
static void __restore_boosting(void);

static void init_pm_cpumask(void)
{
	int i;

	cpumask_clear(&fast_cpu_mask);
	for (i = start_cpu; i <= end_cpu; i++)
		cpumask_set_cpu(i, &fast_cpu_mask);
}

static int sec_wakeup_cpu_allocator_suspend_noirq(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int __start_boosting(void)
{
#if defined(PMS_TASK_RT)
	struct sched_param param;
	int ret;
#endif

	pr_info("%s\n", __func__);
	cpumask_copy(&backup_cpu_mask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, &fast_cpu_mask);
	pms_task = current;

#if defined(PMS_TASK_RT)
	if (rt_on && current->prio >= MAX_RT_PRIO) {
		saved_prio = current->prio;
		prev_policy = current->policy;
		param.sched_priority = PM_TASK_RT_PRIO;
		pr_info("start boosting target prio: (%d, %d) current (%d, %d)\n",
			PM_TASK_RT_PRIO, SCHED_FIFO, saved_prio, prev_policy);
		ret = sched_setscheduler_nocheck(current, SCHED_FIFO, &param);
		if (ret != 0) {
			pr_err("Failed to set real-time scheduling policy and priority with ret:%d\n", ret);
#if defined(DEBUG_ENABLE)
			BUG();
#endif
			saved_prio = INVALID_SAVED_PRIO;
		}
	}
#endif

	return 0;
}

static void __restore_boosting(void)
{
#if defined(PMS_TASK_RT)
	int ret;
	struct sched_param param;
#endif
	pr_info("%s\n", __func__);

	if (!cpumask_empty(&backup_cpu_mask)) {
		set_cpus_allowed_ptr(pms_task, &backup_cpu_mask);
		cpumask_clear(&backup_cpu_mask);
#if defined(PMS_TASK_RT)
		if (rt_on && saved_prio != INVALID_SAVED_PRIO) {
			pr_info("restored to (%d, %d) from prev prio (%d, %d)\n",
				saved_prio, prev_policy, current->prio, current->policy);

			param.sched_priority = saved_prio - DEFAULT_PRIO;
			ret = sched_setscheduler(pms_task, prev_policy, &param);
			if (ret != 0) {
				pr_crit("Failed to retore to saved_prio with error: %d\n", ret);
#if defined(DEBUG_ENABLE)
				BUG();
#endif
			}
			saved_prio = INVALID_SAVED_PRIO;
		}
#endif
	}
}

static int sec_wakeup_cpu_allocator_resume_noirq(struct device *dev)
{
	return __start_boosting();
}

static void sec_wakeup_cpu_allocator_complete(struct device *dev)
{
	pr_info("%s\n", __func__);

	/*
	 *	if (!cpumask_empty(&backup_cpu_mask)) {
	 *	set_cpus_allowed_ptr(current, &backup_cpu_mask);
	 *	cpumask_clear(&backup_cpu_mask);
	 *}
	 */
}

static const struct dev_pm_ops wakeupcpu_pm_ops = {
	.suspend_noirq = sec_wakeup_cpu_allocator_suspend_noirq,
	.resume_noirq = sec_wakeup_cpu_allocator_resume_noirq,
	.complete = sec_wakeup_cpu_allocator_complete,
};

static struct notifier_block sec_wcpu_alloc_pm_notifier_block = {
	.notifier_call = sec_wcpu_alloc_pm_notifier,
};

static int sec_wcpu_alloc_pm_notifier(struct notifier_block *notifier,
	unsigned long pm_event, void *dummy)
{
	switch (pm_event) {
	case PM_POST_SUSPEND:
		__restore_boosting();
		break;
	}

	return NOTIFY_OK;
}

static int sec_wakeup_cpu_allocator_setup(struct device *dev)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	if (of_property_read_u32(dev->of_node, "start_cpu_num", &start_cpu) < 0) {
		pr_info("%s: start_cpu_num not found\n", __func__);
		ret = -ENODEV;
	}

	if (of_property_read_u32(dev->of_node, "end_cpu_num", &end_cpu) < 0) {
		pr_info("%s: end_cpu_num not found\n", __func__);
		ret = -ENODEV;
	}

	pr_info("%s: alloc cpu %d - %d\n", __func__, start_cpu, end_cpu);

	return ret;
}

/*
 * Sysfs impl
 */

#if defined(DEBUG_ENABLE)
static ssize_t wcpus_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	unsigned int start, end;

	ret = sscanf(buf, "%u %u", &start, &end);
	if (ret != 2)
		return -EINVAL;

	start_cpu = start;
	end_cpu = end;
	init_pm_cpumask();
	pr_info("wakeup cpus: %d ~ %d\n", start, end);
	return count;
}

#if defined(PMS_TASK_RT)
static ssize_t rt_task_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val > 1)
		return -EINVAL;

	rt_on = val;
	pr_info("pms run by rt: %d\n", rt_on);
	return count;
}
#endif

static DEVICE_ATTR(wcpus, 0220, NULL, wcpus_store);
#if defined(PMS_TASK_RT)
static DEVICE_ATTR(rt_task, 0220, NULL, rt_task_store);
#endif
static struct attribute *sec_wakeup_attrs[] = {
	&dev_attr_wcpus.attr,
#if defined(PMS_TASK_RT)
	&dev_attr_rt_task.attr,
#endif
	NULL,
};

static const struct attribute_group sec_wakeup_attr_group = {
	.attrs = sec_wakeup_attrs,
};
#endif

static int sec_wakeup_cpu_allocator_probe(struct platform_device *pdev)
{
#if defined(DEBUG_ENABLE)
	int ret;
	struct device *dev = &pdev->dev;
#endif
	pr_info("%s\n", __func__);
	if (sec_wakeup_cpu_allocator_setup(&pdev->dev) < 0) {
		pr_crit("%s: failed\n", __func__);
		return -ENODEV;
	}

#if defined(DEBUG_ENABLE)
	ret = sysfs_create_group(&dev->kobj, &sec_wakeup_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs group\n");
		return ret;
	}
#endif
	init_pm_cpumask();

	register_pm_notifier(&sec_wcpu_alloc_pm_notifier_block);

	return 0;
}

static const struct of_device_id of_sec_wakeup_cpu_allocator_match[] = {
	{ .compatible = "samsung,sec_wakeup_cpu_allocator", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_sec_wakeup_cpu_allocator_match);

static struct platform_driver sec_wakeup_cpu_allocator_driver = {
	.probe = sec_wakeup_cpu_allocator_probe,
	.driver = {
		.name = "sec_wakeup_cpu_allocator",
		.of_match_table = of_match_ptr(of_sec_wakeup_cpu_allocator_match),
#if IS_ENABLED(CONFIG_PM)
		.pm = &wakeupcpu_pm_ops,
#endif
	},

};

static int __init sec_wakeup_cpu_allocator_driver_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&sec_wakeup_cpu_allocator_driver);
}
core_initcall(sec_wakeup_cpu_allocator_driver_init);

static void __exit sec_wakeup_cpu_allocator_driver_exit(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_unregister(&sec_wakeup_cpu_allocator_driver);
}
module_exit(sec_wakeup_cpu_allocator_driver_exit);

MODULE_DESCRIPTION("SEC WAKEUP CPU ALLOCATOR driver");
MODULE_LICENSE("GPL");
