// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6877-gce.h>
#include "cmdq-util.h"

#define MDP_THRD_MIN	19

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
		return "DISP";
	case 7:
		return "VDEC";
	case 10:
	case 19 ... 22:
		return "MDP";
	case 11:
	case 13 ... 14:
	case 16 ... 18:
		return "ISP";
	case 12:
		return "VENC";
	case 15:
		return "CMDQ";
	case 23:
		return "SMI";
	default:
		return "CMDQ";
	}
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_VENC_CMDQ_FRAME_DONE ... CMDQ_EVENT_VENC_CMDQ_VPS_DONE:
		return "VENC";
	case CMDQ_EVENT_LINE_COUNT_THRESHOLD_INTERRUPT
		... CMDQ_EVENT_GCE_CNT_OP_THRESHOLD:
		return "VDEC";
	case CMDQ_EVENT_FDVT_DONE:
		return "FDVT";
	case CMDQ_EVENT_RSC_DONE:
		return "RSC";
	case CMDQ_EVENT_DVS_DONE:
		return "DVS";
	case CMDQ_EVENT_DVP_DONE:
		return "DVP";
	case CMDQ_EVENT_MFB_DONE_LINK_MISC:
		return "MFB";
	case CMDQ_EVENT_WPE_A_DONE_LINE_MISC:
		return "WPE";
	case CMDQ_EVENT_MSS_DONE_LINK_MISC:
		return "MSS";
	case CMDQ_EVENT_DIP_FRAME_DONE_P2_0
		... CMDQ_EVENT_DIP_DMA_ERR_EVENT:
		return "DIP";
	case CMDQ_EVENT_MDP_RDMA0_SOF
		... CMDQ_EVENT_MDP_RDMA0_SW_RST_DONE_ENG_EVENT:
		return "MDP";
	case CMDQ_EVENT_DISP_OVL0_SOF
		... CMDQ_EVENT_DISP_BUF_UNDERRUN_ENG_EVENT_7:
		return "DISP";
	default:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}
}

u32 cmdq_util_hw_id(u32 pa)
{
	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x14000100,	/* mmsys MMSYS_CG_CON0 */
		0x112300a0,	/* msdc0 SW_DBG_SEL: LSB 16-bit only */
		0x1121004c,	/* To-do: audio AFE_I2S_CON3_OFFSET */
		0x110020bc,	/* uart0:LSB 1-bit only */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

const char *cmdq_util_hw_name(void *chan)
{
	return "GCE";
}

bool cmdq_thread_ddr_module(const s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
	case 15:
		return false;
	default:
		return true;
	}
}

uint cmdq_get_mdp_min_thread(void)
{
	return MDP_THRD_MIN;
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
	.get_mdp_min_thread = cmdq_get_mdp_min_thread,
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
	return 0;
}
module_init(cmdq_platform_init);

MODULE_LICENSE("GPL");
