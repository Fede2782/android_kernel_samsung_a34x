// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Iris-sc Yang <iris-sc.yang@mediatek.com>
 */

#include "mdp_drv_dummy.h"

static struct dpc_funcs mdp_dpc_funcs;

void mdp_dpc_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version)
{
	if (funcs) {
		mdp_dpc_funcs.dpc_dc_force_enable = funcs->dpc_dc_force_enable;
		mdp_dpc_funcs.dpc_vidle_power_keep = funcs->dpc_vidle_power_keep;
		mdp_dpc_funcs.dpc_vidle_power_release = funcs->dpc_vidle_power_release;
	}
}
EXPORT_SYMBOL_GPL(mdp_dpc_register);

void mdp_set_resource_callback(enum cmdq_event res_event,
	CmdqResourceAvailableCB res_available,
	CmdqResourceReleaseCB res_release)
{
	cmdqMdpSetResourceCallback(res_event, res_available, res_release);
}
EXPORT_SYMBOL(mdp_set_resource_callback);

MODULE_DESCRIPTION("MTK MDP DRV_DUMMY");
MODULE_AUTHOR("Iris-sc Yang <iris-sc.yang@mediatek.com>");
MODULE_LICENSE("GPL");
