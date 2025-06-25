/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Iris-sc.Yang <iris-sc.yang@mediatek.com>
 */

#ifndef __MDP_DRV_DUMMY_H__
#define __MDP_DRV_DUMMY_H__

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk_dpc.h"
#include "mdp_event_common.h"

typedef s32(*CmdqResourceReleaseCB) (enum cmdq_event resourceEvent);
typedef s32(*CmdqResourceAvailableCB) (enum cmdq_event resourceEvent);
typedef void (*CmdqMdpSetResourceCallback) (enum cmdq_event res_event,
	CmdqResourceAvailableCB res_available,
	CmdqResourceReleaseCB res_release);
CmdqMdpSetResourceCallback cmdqMdpSetResourceCallback;

void mdp_dpc_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version);
void mdp_set_resource_callback(enum cmdq_event res_event,
	CmdqResourceAvailableCB res_available,
	CmdqResourceReleaseCB res_release);


#endif	/* __MDP_DRV_DUMMY_H__ */
