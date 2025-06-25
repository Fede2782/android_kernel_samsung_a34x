// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) "MKP: " fmt

#include <linux/types.h> // for list_head
#include <linux/module.h> // module_layout
#include <linux/init.h> // rodata_enable support
#include <linux/mutex.h>
#include <linux/kernel.h> // round_up
#include <linux/of_reserved_mem.h> // for pkvm dts node check

#ifdef DEMO_MKP
#include "mkp_demo.h"
#endif

#include "mkp.h"

/* ext policies */
#include "ext/mkp_pcpu.h"

static int __init mkp_init(void)
{
	struct device_node *node = NULL;
	const char *pkvm_status = NULL;
	int ret = 0;

	// Get pkvm dts node
	node = of_find_node_by_name(NULL, "pkvm");
	if (!node) {
		pr_info("Cannot find pkvm node, failed to initialize pkvm mkp\n");
		return 0;
	}

	of_property_read_string(node, "status", &pkvm_status);
	if (strncmp(pkvm_status, "okay", sizeof("okay"))) {
		pr_info("pkvm is disabled\n");
		return 0;
	}

	pr_info("pkvm is enabled\n");

	/* TODO: Preparation for grant ticket */

	/****************************************/
	/* Good position to call following APIs */
	/* - mkp_change_policy_action           */
	/* - mkp_request_new_policy             */
	/* - mkp_request_new_specified_policy   */
	/****************************************/

	/* TODO: Try to protect per cpu data */

#ifdef DEMO_MKP
	ret = mkp_demo_init();
#endif

	if (ret)
		pr_info("%s: failed, ret: %d\n", __func__, ret);

	pr_info("%s:%d done\n", __func__, __LINE__);
	return ret;
}
module_init(mkp_init);

static void  __exit mkp_exit(void)
{
	/*
	 * vendor hook cannot unregister, please check vendor_hook.h
	 */
	pr_info("%s:%d\n", __func__, __LINE__);
}
module_exit(mkp_exit);

MODULE_AUTHOR("Chinwen Chang <chinwen.chang@mediatek.com>");
MODULE_AUTHOR("Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>");
MODULE_DESCRIPTION("MediaTek MKP Driver");
MODULE_LICENSE("GPL");
