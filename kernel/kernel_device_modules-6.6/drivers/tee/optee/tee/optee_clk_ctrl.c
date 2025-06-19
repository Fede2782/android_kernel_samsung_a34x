// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/tee_drv.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include "optee_private.h"

#define TEE_DEVLOG_(LEVEL)   pr_##LEVEL
#define CLK_NAME_LEN 16

struct ccl_clk {
	struct list_head list;
	char clk_name[CLK_NAME_LEN];
	struct clk *clk;
};

static LIST_HEAD(ccl_clk_list);

void optee_clkctrl_init(struct device_node *np)
{
	int clks_num;
	int idx;

	clks_num = of_property_count_strings(np, "clock-names");

	for (idx = 0; idx < clks_num; idx++) {
		const char *clk_name;
		struct clk *clk;
		struct ccl_clk *ccl_clk;

		if (of_property_read_string_index(np,
					"clock-names", idx, &clk_name)) {
			TEE_DEVLOG_(warn)("[%s] get clk_name failed, index:%d\n",
					__FILE__,
					idx);
			continue;
		}

		if (strlen(clk_name) > CLK_NAME_LEN-1) {
			TEE_DEVLOG_(warn)("[%s] clk_name %s (%d) too long, trims to %d\n",
					__FILE__,
					clk_name, strlen(clk_name), CLK_NAME_LEN-1);
		}

		clk = of_clk_get(np, idx);
		if (IS_ERR(clk)) {
			TEE_DEVLOG_(warn)("[%s] get devm_clk_get failed, clk_name:%s\n",
					__FILE__,
					clk_name);
			continue;
		}

		ccl_clk = kzalloc(sizeof(struct ccl_clk), GFP_KERNEL);
		if (!ccl_clk) {
			TEE_DEVLOG_(warn)("[%s] kzalloc failed for ccl_clk\n",
					__FILE__);
			continue;
		}

		strscpy(ccl_clk->clk_name, clk_name, CLK_NAME_LEN);
		ccl_clk->clk = clk;
		list_add(&ccl_clk->list, &ccl_clk_list);
	}
}

static struct clk *ccl_clk_get(const char *clk_name)
{
	struct ccl_clk *cur;
	struct ccl_clk *tmp;

	list_for_each_entry_safe(cur, tmp, &ccl_clk_list, list) {
		if (strncmp(cur->clk_name, clk_name,
					strlen(cur->clk_name)) == 0)
			return cur->clk;
	}

	return NULL;
}

void handle_rpc_func_kree_clock_control(struct optee_msg_arg *arg)
{
	struct ccl_clk *cur, *tmp;
	struct clk *ck;
	u32 en;
	char cn[17];
	u64 *pn = (u64 *)cn;

	if (arg->num_params != 1)
		goto bad;
	if ((arg->params[0].attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto bad;

	cn[16] = '\0';
	*pn = arg->params[0].u.value.a;
	*(pn+1) = arg->params[0].u.value.b;
	en = arg->params[0].u.value.c;

	if (strcmp(cn, "all") == 0) {
		list_for_each_entry_safe(cur, tmp, &ccl_clk_list, list) {
			if (en == 0)
				clk_disable_unprepare(cur->clk);
			else
				clk_prepare_enable(cur->clk);
		}
	} else {
		ck = ccl_clk_get(cn);
		if (ck != NULL) {
			if (en == 0)
				clk_disable_unprepare(ck);
			else
				clk_prepare_enable(ck);
			arg->ret = TEEC_SUCCESS;
		} else {
			goto bad;
		}
	}

	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}
