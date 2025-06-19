// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/timer.h>

#include <lpm.h>

#include <lpm_plat_apmcu.h>
#include <lpm_module.h>
#include <mtk_cpuidle_status.h>


void __iomem *cpu_pm_mcusys_base;
void __iomem *cpu_pm_syssram_base;

#define plat_node_ready()       (cpu_pm_mcusys_base && cpu_pm_syssram_base)

/* qos */
static struct pm_qos_request lpm_plat_qos_req;

#define lpm_plat_qos_init()\
	cpu_latency_qos_add_request(&lpm_plat_qos_req,\
		PM_QOS_DEFAULT_VALUE)
#define lpm_cpu_off_block()\
	cpu_latency_qos_update_request(&lpm_plat_qos_req, LPM_CPU_OFF_BLOCK_VALUE)
#define lpm_cpu_off_allow()\
	cpu_latency_qos_update_request(&lpm_plat_qos_req, PM_QOS_DEFAULT_VALUE)
#define lpm_plat_qos_uninit()\
	cpu_latency_qos_remove_request(&lpm_plat_qos_req)

#define CHK(cond) WARN_ON(cond)

#define PWR_LVL_BIT(v)  BIT(4 * v)

#define PWR_LVL_MASK(v) (0xF << (4 * (v)))

#define PWR_LVL0        (PWR_LVL_BIT(0))
#define PWR_LVL1        (PWR_LVL_BIT(1) | PWR_LVL0)
#define PWR_LVL2        (PWR_LVL_BIT(2) | PWR_LVL1)
#define PWR_LVL3        (PWR_LVL_BIT(3) | PWR_LVL2)

#define CORE_LVL	PWR_LVL0
#define CLUSTER_LVL	PWR_LVL1
#define MCUSYS_LVL	PWR_LVL2
#define BUS26M_LVL	PWR_LVL3

#define MAX_PWR_LVL     MCUSYS_LVL

struct lpm_device {
	unsigned int pwr_lvl_mask;
	union {
		struct {
			u32 core:4;
			u32 cluster:4;
			u32 mcusys:4;
			u32 bus26m:4;
		};
		u32 lvl;
	} pwr_on;
	struct lpm_device *parent;
};

static struct lpm_device lp_dev_cpu[NR_CPUS];
static struct lpm_device lp_dev_cluster[nr_cluster_ids];
static struct lpm_device lp_dev_mcusys;

static void _lpm_plat_inc_pwr_cnt(struct lpm_device *dev,
					unsigned int lvl)
{
	unsigned int lvl_cnt;

	lvl_cnt = lvl & dev->pwr_lvl_mask;

	if (!lvl_cnt)
		return;

	/**
	 * Exclusive read-modify-write is ensured by
	 *     lpm_mod_locker in idle flow
	 *     lpm_plat_qos_req in hot-plug flow
	 */
	dev->pwr_on.lvl += lvl_cnt;

	CHK(dev->pwr_on.core >= nr_cpu_ids);

	if (dev->parent)
		_lpm_plat_inc_pwr_cnt(dev->parent, lvl);
}

static void _lpm_plat_dec_pwr_cnt(struct lpm_device *dev,
					unsigned int lvl)
{
	unsigned int lvl_cnt;

	lvl_cnt = lvl & dev->pwr_lvl_mask;

	if (!lvl_cnt)
		return;

	/**
	 * Exclusive read-modify-write is ensured by
	 *     lpm_mod_locker in idle flow
	 *     lpm_plat_qos_req in hot-plug flow
	 */
	dev->pwr_on.lvl -= lvl_cnt;

	CHK((dev->pwr_on.lvl & BIT(31)));

	if (dev->parent)
		_lpm_plat_dec_pwr_cnt(dev->parent, lvl);
}

#define lpm_plat_set_off_lvl(cpu, lvl)				\
do {									\
	if (likely(cpu < nr_cpu_ids))					\
		_lpm_plat_dec_pwr_cnt(&lp_dev_cpu[cpu], lvl);	\
} while (0)

#define lpm_plat_clr_off_lvl(cpu, lvl)				\
do {									\
	if (likely(cpu < nr_cpu_ids))					\
		_lpm_plat_inc_pwr_cnt(&lp_dev_cpu[cpu], lvl);	\
} while (0)

void lpm_plat_set_mcusys_off(int cpu)
{
	lpm_plat_set_off_lvl(cpu, MCUSYS_LVL);
}

void lpm_plat_clr_mcusys_off(int cpu)
{
	lpm_plat_clr_off_lvl(cpu, MCUSYS_LVL);
}

void lpm_plat_set_cluster_off(int cpu)
{
	lpm_plat_set_off_lvl(cpu, CLUSTER_LVL);
}

void lpm_plat_clr_cluster_off(int cpu)
{
	lpm_plat_clr_off_lvl(cpu, CLUSTER_LVL);
}

static inline void lpm_dev_get_cpus_online(void)
{
	int i;

	for_each_online_cpu(i)
		lpm_plat_clr_off_lvl(i, MAX_PWR_LVL);
}

bool lpm_plat_is_mcusys_off(void)
{
	return !lp_dev_mcusys.pwr_on.mcusys;
}

bool lpm_plat_is_cluster_off(int cpu)
{
	if (unlikely(cpu < 0 || cpu >= nr_cpu_ids || !lp_dev_cpu[cpu].parent))
		return false;

	return !lp_dev_cpu[cpu].parent->pwr_on.cluster;
}

struct lpm_cpu_ret_timer_data {
	int cpu;
	int dts_en;
	int ret_ready;
	struct timer_list ret_timer;
};

static struct lpm_cpu_ret_timer_data lpm_crt[NR_CPUS];
static unsigned int cpu_ret_check_interval;

static void lpm_cpu_ret_check(struct timer_list *t)
{
	long en;
	struct lpm_cpu_ret_timer_data *crtd = from_timer(crtd, t, ret_timer);
	unsigned int cur_cpu = smp_processor_id();

	if (cur_cpu != crtd->cpu) {
		crtd->ret_timer.expires += ((unsigned long)cpu_ret_check_interval);
		add_timer_on(&crtd->ret_timer, crtd->cpu);
		return;
	}

	en = lpm_smc_cpu_pm_lp(CPU_PM_CPU_RET_CTRL, MT_LPM_SMC_ACT_SET,
					0x6, 0);
	if (en == 0) {
		crtd->ret_ready = 1;
		del_timer(&crtd->ret_timer);
	} else if (!timer_pending(&crtd->ret_timer)) {
		crtd->ret_timer.expires += ((unsigned long)cpu_ret_check_interval);
		add_timer_on(&crtd->ret_timer, cur_cpu);
	}
	pr_info("[name:cpu_pm&] cpu_ret_check\n");
}

static int lpm_start_cpu_ret_timer(unsigned int cpu)
{
	if (!lpm_crt[cpu].dts_en || lpm_crt[cpu].ret_ready)
		return 0;
	timer_setup(&lpm_crt[cpu].ret_timer, lpm_cpu_ret_check, 0);
	lpm_crt[cpu].ret_timer.expires =
			jiffies + ((unsigned long)cpu_ret_check_interval);
	add_timer_on(&lpm_crt[cpu].ret_timer, cpu);
	return 0;
}

static int lpm_stop_cpu_ret_timer(unsigned int cpu)
{
	if (!lpm_crt[cpu].dts_en || lpm_crt[cpu].ret_ready)
		return 0;
	del_timer(&lpm_crt[cpu].ret_timer);
	return 0;
}

static int lpm_cpu_ret_init(void)
{
	long en;
	int cpu, hpstate;
	struct device_node *cpu_dts, *lpm_node;

	lpm_node = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);
	if (lpm_node)
		of_property_read_u32(lpm_node, "cpu-ret-chk-interval",
				&cpu_ret_check_interval);

	if (cpu_ret_check_interval)
		cpu_ret_check_interval *= HZ;
	else
		cpu_ret_check_interval = 5 * HZ; /* default check interval: 5s */

	en = lpm_smc_cpu_pm_lp(CPU_PM_CPU_RET_CTRL, MT_LPM_SMC_ACT_COMPAT,
					0, 0);

	for_each_possible_cpu(cpu) {
		const char *pRetention = NULL;

		cpu_dts = of_cpu_device_node_get(cpu);
		of_property_read_string(cpu_dts, "cpu-retention", &pRetention);
		lpm_crt[cpu].cpu = cpu;
		lpm_crt[cpu].ret_ready = 0;
		if (pRetention && !strcmp(pRetention, "disable"))
			lpm_crt[cpu].dts_en = 0;
		else if (en & (1 << cpu))
			lpm_crt[cpu].dts_en = 1;
	}

	if (en) {
		hpstate = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "cpu_ret_en_check",
				lpm_start_cpu_ret_timer, lpm_stop_cpu_ret_timer);
		if (hpstate == -ENOSPC)
			pr_info("[name:cpu_pm&] Fail to register cpu_ret_en_check hpcb\n");
	}
	return 0;
}

/*
 * Timespec interfaces utilizing the ktime based ones
 */
static inline void get_monotonic_boottime(struct timespec64 *ts)
{
	*ts = ktime_to_timespec64(ktime_get_boottime());
}

static void __init lpm_plat_pwr_dev_init(void)
{
	struct lpm_device *dev;
	int i, cluster_id;

	for (i = 0; i < nr_cpu_ids; i++) {

		dev = &lp_dev_cpu[i];
		dev->pwr_lvl_mask = PWR_LVL_MASK(0);

		cluster_id = get_physical_cluster_id(i);

		if (cluster_id < nr_cluster_ids)
			dev->parent = &lp_dev_cluster[cluster_id];
	}

	for (i = 0; i < nr_cluster_ids; i++) {
		dev = &lp_dev_cluster[i];
		dev->pwr_lvl_mask = PWR_LVL_MASK(1);
		dev->parent = &lp_dev_mcusys;
	}

	lp_dev_mcusys.pwr_lvl_mask = PWR_LVL_MASK(2);

	lpm_dev_get_cpus_online();
}

static int __init lpm_plat_mcusys_ctrl_init(void)
{
	struct device_node *node = NULL;

	cpu_pm_mcusys_base = NULL;
	cpu_pm_syssram_base = NULL;

	node = of_find_compatible_node(NULL, NULL,
						"mediatek,mcusys-ctrl");
	if (node) {
		cpu_pm_mcusys_base = of_iomap(node, 0);
		of_node_put(node);
	}

	node = of_find_compatible_node(NULL, NULL,
						"mediatek,cpupm-sysram");
	if (node) {
		cpu_pm_syssram_base = of_iomap(node, 0);
		of_node_put(node);
	}

	return plat_node_ready() ? 0 : -1;
}

int __init lpm_plat_apmcu_init(void)
{
	if (!plat_node_ready()) {
		lpm_plat_qos_uninit();
		return 0;
	}

	lpm_plat_pwr_dev_init();
	lpm_cpu_off_allow();

	lpm_cpu_ret_init();

	return 0;
}

int __init lpm_plat_apmcu_early_init(void)
{
	if (lpm_plat_mcusys_ctrl_init() != 0) {
		pr_notice("%s(): Not support\n", __func__);
		return 0;
	}

	lpm_plat_qos_init();

	if (cpu_pm_syssram_base) {
		memset_io((void __iomem *)
			(cpu_pm_syssram_base + LP_PM_SYSRAM_INFO_OFS),
			0,
			LP_PM_SYSRAM_SIZE - LP_PM_SYSRAM_INFO_OFS);
	}

	lpm_cpu_off_block();

	return 0;
}
