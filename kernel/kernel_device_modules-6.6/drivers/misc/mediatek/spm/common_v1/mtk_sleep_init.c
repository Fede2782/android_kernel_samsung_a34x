// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "mtk_spm.h"
#include "mtk_sleep_internal.h"
#include "mtk_idle_module_plat.h"
#include "mtk_spm_suspend_internal.h"
#include "mtk_spm_resource_req_console.h"
#include "mtk_spm_internal.h"
#include "mtk_sleep.h"
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/hashtable.h>

static bool spm_drv_init;


struct qos_work_t{
	struct work_struct	qos_queue;
	unsigned long data;
	int value;
};


struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

struct h_node {
	unsigned long addr;
	char symbol[KSYM_SYMBOL_LEN];
	struct hlist_node node;
};

static DECLARE_HASHTABLE(tbl, 5);

static const char *find_and_get_symobls(unsigned long caller_addr)
{
	struct h_node *cur_node = NULL;
	struct h_node *new_node = NULL;
	const char *cur_symbol = NULL;
	unsigned int cur_key = 0;

	cur_key = (unsigned int) caller_addr & 0x1f;
	// Try to find symbols from history records
	hash_for_each_possible(tbl, cur_node, node, cur_key) {
		if (cur_node->addr == caller_addr) {
			cur_symbol = cur_node->symbol;
			break;
		}
	}
	// Symbols not found. Add new records
	if (!cur_symbol) {
		new_node = kzalloc(sizeof(struct h_node), GFP_KERNEL);
		if (!new_node)
			return NULL;
		new_node->addr = caller_addr;
		sprint_symbol(new_node->symbol, caller_addr);
		cur_symbol = new_node->symbol;
		hash_add(tbl, &new_node->node, cur_key);
	}
	return cur_symbol;
}
/**/
static void handle_cpu_qos_tracer(struct work_struct *qos_queue)
{
	struct qos_work_t *my_work = container_of(qos_queue, struct qos_work_t, qos_queue);
	const char *caller_info = find_and_get_symobls(my_work->data);

	if (caller_info)
		pr_info("[pm_qos_debug] %s request PMQOS = %d\n",caller_info, my_work->value);

	kfree(my_work);
}

static void cpu_qos_tracer(void *ignore, int value)
{
	unsigned long data;
	struct qos_work_t *my_work;

	data = (unsigned long)__builtin_return_address(2);
	my_work = kmalloc(sizeof(struct qos_work_t), GFP_ATOMIC);
	if (my_work) {
		INIT_WORK(&my_work->qos_queue, handle_cpu_qos_tracer);
		my_work->data = data;
		my_work->value = value;
		schedule_work(&my_work->qos_queue);
	}
}

struct tracepoints_table cpu_qos_tracepoints[] = {
	{.name = "pm_qos_update_request", .func = cpu_qos_tracer},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(cpu_qos_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(cpu_qos_tracepoints[i].name, tp->name) == 0)
			cpu_qos_tracepoints[i].tp = tp;
	}
}

void cpu_qos_tracepoint_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (cpu_qos_tracepoints[i].registered) {
			tracepoint_probe_unregister(
			cpu_qos_tracepoints[i].tp,
			cpu_qos_tracepoints[i].func, NULL);
			cpu_qos_tracepoints[i].registered = false;
		}
	}
}

int add_cpu_qos_tracer(void)
{
	unsigned int i , ret;

	hash_init(tbl);
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);
	FOR_EACH_INTEREST(i) {
		if (cpu_qos_tracepoints[i].tp == NULL) {
			pr_info("pbm Error, %s not found\n", cpu_qos_tracepoints[i].name);
			cpu_qos_tracepoint_cleanup();
			return -1;
		}
	}
	ret = tracepoint_probe_register(cpu_qos_tracepoints[0].tp, cpu_qos_tracepoints[0].func,  NULL);
	if (!ret)
		cpu_qos_tracepoints[0].registered = true;
	else
		pr_info("pm_qos_update_request: Couldn't activate tracepoint\n");
	return 0;
}

bool mtk_spm_drv_ready(void)
{
	return spm_drv_init;
}

static int __init mtk_sleep_init(void)
{
	int ret = -1;
	mtk_cpuidle_framework_init();
	mtk_idle_cond_check_init();
	spm_resource_console_init();
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	slp_module_init();
	ret = mtk_spm_init();
#endif
	mtk_idle_module_initialize_plat();
	spm_logger_init();
#if IS_ENABLED(CONFIG_ARM64)
	add_cpu_qos_tracer();
#endif
	spm_drv_init = !ret;
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	ccci_set_spm_mdsrc_cb(&spm_ap_mdsrc_req);
	ccci_set_spm_md_sleep_cb(&spm_is_md1_sleep);
#endif

#if IS_ENABLED(CONFIG_MTK_MDPM_LEGACY)
	mdpm_register_md_status_cb(&spm_vcorefs_get_MD_status);
#endif
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	register_spm_resource_req_func(&spm_resource_req);
#else
	pr_info("[SPM] USB DRIVER IS NOT READY!\n");
#endif
	return 0;
}

static void __exit mtk_sleep_exit(void)
{
}

module_init(mtk_sleep_init);
module_exit(mtk_sleep_exit);
MODULE_LICENSE("GPL");
