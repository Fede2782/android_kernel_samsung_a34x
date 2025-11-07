// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <trace/hooks/power.h>

#if defined(CONFIG_SEC_FACTORY)
static unsigned int __read_mostly jigon;
module_param(jigon, uint, 0440);
#endif

#if defined(CONFIG_CPU_BOOTING_CLK_LIMIT)
#define MIN (FREQ_QOS_MIN - 0x1)
#define MAX (FREQ_QOS_MAX - 0x1)

struct freq_qos_request *clk_delay_req[NR_CPUS][FREQ_QOS_MAX];

void get_cpu_min_max_freq(void)
{
	int cpu;
	int num = 0;
	struct cpufreq_policy *policy;

    for_each_possible_cpu(cpu) {
        policy = cpufreq_cpu_get(cpu);

        if (policy) {
            pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
                    __func__, num, cpu, policy->min, policy->max);
            num++;
            cpu = cpumask_last(policy->related_cpus);
            cpufreq_cpu_put(policy);
        }
    }
}

static int set_cpu_min_max_freq(unsigned int cpu, unsigned long freq, unsigned int type)
{
	struct cpufreq_policy *policy;
    unsigned int first_cpu = 0;
	int ret = 0;

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
        pr_err("%s - getting policy was failed <cpu-%u, req_freq : %lu>\n",
                __func__, cpu, freq);
        return -ENODEV;
    }

    first_cpu = cpumask_first(policy->related_cpus);

    switch (type) {
    case FREQ_QOS_MIN:
         ret = freq_qos_update_request(clk_delay_req[first_cpu][MIN], freq);
         break;
    case FREQ_QOS_MAX:
         ret = freq_qos_update_request(clk_delay_req[first_cpu][MAX], freq);
         break;
    default:
         ret = -EINVAL;
    }
    pr_info("%s - updating min/max QoS <cpu-%u, first_cpu : %u, freq : %lu, type : %u, ret : %d>\n",
            __func__, cpu, first_cpu, freq, type, ret);
    cpufreq_cpu_put(policy);

    return ret;
}

static int set_clock_limit(void)
{
    struct cpufreq_policy *policy;
	unsigned long delay = 0;
	int ret = 0, cpu = 0;
	
    for_each_possible_cpu(cpu) {
        policy = cpufreq_cpu_get(cpu);

        clk_delay_req[cpu][MIN] = kzalloc(sizeof(struct freq_qos_request), GFP_KERNEL);
        clk_delay_req[cpu][MAX] = kzalloc(sizeof(struct freq_qos_request), GFP_KERNEL);
	    ret = freq_qos_add_request(&policy->constraints, clk_delay_req[cpu][MIN], FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
        if (ret < 0) {
            pr_err("%s - adding freq QOS type-%u was failed <cpu-%u>\n",
                    __func__, FREQ_QOS_MIN, cpu);
            goto error_done;
        }
	    ret = freq_qos_add_request(&policy->constraints, clk_delay_req[cpu][MAX], FREQ_QOS_MAX, PM_QOS_DEFAULT_VALUE);
        if (ret < 0) {
            pr_err("%s - adding freq QOS type-%u was failed <cpu-%u>\n",
                    __func__, FREQ_QOS_MAX, cpu);
            goto error_done;
        }
        cpu = cpumask_last(policy->related_cpus);
        cpufreq_cpu_put(policy);
    }

    /* sample for updating min-freq for each policy */
	set_cpu_min_max_freq(0, 339000, FREQ_QOS_MIN);
	set_cpu_min_max_freq(4, 622000, FREQ_QOS_MIN);
	set_cpu_min_max_freq(7, 798000, FREQ_QOS_MIN);

    /* sample for updating max-freq for each policy */
	set_cpu_min_max_freq(0, 900000, FREQ_QOS_MAX);
	set_cpu_min_max_freq(4, 1300000, FREQ_QOS_MAX);
	set_cpu_min_max_freq(7, 1400000, FREQ_QOS_MAX);	
	
	delay =  300 * 1000; // 300 seconds
	
	get_cpu_min_max_freq(); // For debugging

	pr_info("clk_delay_thread started, sleeping for %lu seconds...\n", delay);

	msleep_interruptible(delay);

	pr_info("clk_delay_thread woke up, clock restored to default. Exiting now...\n"); // fall through
	
error_done:
    for_each_possible_cpu(cpu) {
        if (clk_delay_req[cpu][MIN]) {
            if (freq_qos_remove_request(clk_delay_req[cpu][MIN]) < 0)
                pr_err("%s - remove freq QOS type-%u was failed <cpu-%u>\n",
                        __func__, FREQ_QOS_MIN, cpu);
            else
                pr_info("%s - remove freq QOS type-%u was succeed <cpu-%u>\n",
                        __func__, FREQ_QOS_MIN, cpu);
        }
        if (clk_delay_req[cpu][MAX]) {
            if (freq_qos_remove_request(clk_delay_req[cpu][MAX]) < 0)
                pr_err("%s - remove freq QOS type-%u was failed <cpu-%u>\n",
                        __func__, FREQ_QOS_MIN, cpu);
            else
                pr_info("%s - remove freq QOS type-%u was succeed <cpu-%u>\n",
                        __func__, FREQ_QOS_MIN, cpu);
        }
    }
    return ret;	
}
#endif

static int clk_delay_thread_function(void *data)
{
#if defined(CONFIG_SEC_FACTORY)
	if(!jigon) {
		pr_info("clk_adjust.jigon is off. skip cpu max clk limiting.\n");
		return 0;
	}
#endif

#if defined(CONFIG_CPU_BOOTING_CLK_LIMIT)
	set_clock_limit();
#endif

	return 0;
}

static int __init clk_adjust_init(void)
{
    struct task_struct *clk_delay_task = NULL;
	
	pr_info("run clk_delay_thread_function\n");
	clk_delay_task = kthread_run(clk_delay_thread_function, NULL, "clk_delay");
	if (IS_ERR(clk_delay_task)) {
		pr_err("clk_delay module failed to start thread\n");
		return PTR_ERR(clk_delay_task);
	}

	pr_info("clk_adjust_init done\n");

	return 0;
}

module_init(clk_adjust_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek POWERHAL_CPU_CTRL");
MODULE_AUTHOR("MediaTek Inc.");
