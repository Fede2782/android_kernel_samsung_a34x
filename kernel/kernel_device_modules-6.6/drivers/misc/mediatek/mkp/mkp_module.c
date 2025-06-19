// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Richard Henderson
 * Copyright (C) 2001 Rusty Russell, 2002, 2010 Rusty Russell IBM.
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/of.h>
#include "mkp_module.h"
#include "mkp_api.h"

#define DRV_NUM_MAX 128
static const char *drv_skip_list[DRV_NUM_MAX] __ro_after_init;
static int drv_skip_num __ro_after_init;

/* mkp_drv_skip="ko_1", "ko_2", "ko_3", ..., "ko_128"; */
void __init update_drv_skip_by_dts(struct device_node *node)
{
	if (!node)
		return;

	drv_skip_num = of_property_read_string_array(node, "mkp_drv_skip",
			drv_skip_list, DRV_NUM_MAX);

	if (drv_skip_num < 0 && drv_skip_num >= DRV_NUM_MAX) {
		pr_info("%s: no valid mkp_drv_skip(%d)\n", __func__, drv_skip_num);
		drv_skip_num = 0;
	}
}

bool drv_skip(char *module_name)
{
	int i = 0;

	for (i = 0; i < drv_skip_num; i++) {
		if (strcmp(module_name, drv_skip_list[i]) == 0)
			return true;
	}
	return false;
}

static void module_set_memory(const struct module *mod, enum mod_mem_type type,
			      enum helper_ops ops, uint32_t policy)
{
	const struct module_memory *mod_mem = &mod->mem[type];

	if (mod_mem->size)
		mkp_set_mapping_xxx_helper((unsigned long)mod_mem->base, mod_mem->size >> PAGE_SHIFT, policy, ops);
}

void module_enable_x(const struct module *mod, uint32_t policy)
{
	module_set_memory(mod, MOD_TEXT, HELPER_MAPPING_X, policy);
}

void module_enable_ro(const struct module *mod, bool after_init, uint32_t policy)
{
	/* DO NOT CHANGE THE FOLLOWING ORDER */
	module_set_memory(mod, MOD_TEXT, HELPER_MAPPING_RO, policy);
	module_set_memory(mod, MOD_RODATA, HELPER_MAPPING_RO, policy);
}
void module_enable_nx(const struct module *mod, uint32_t policy)
{
	module_set_memory(mod, MOD_RODATA, HELPER_MAPPING_NX, policy);
	module_set_memory(mod, MOD_RO_AFTER_INIT, HELPER_MAPPING_NX, policy);
	module_set_memory(mod, MOD_DATA, HELPER_MAPPING_NX, policy);
}
