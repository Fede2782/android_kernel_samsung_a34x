// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6991-gce.h>

#include "cmdq-util.h"
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
#include "proto.h"
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#include <mtk-smmu-v3.h>
#endif

#define GCE_D_PA	0x300c0000
#define GCE_M_PA	0x30140000
#define GCE_D_NORMAL_SID	199
#define GCE_M_NORMAL_SID	183

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 9:
		case 22 ... 31:
			return "MM_DISP";
		case 16 ... 19:
			return "MM_MML";
		case 20 ... 21:
			return "MM_MDP";
		default:
			return "MM_GCE";
		}
	} else if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 5:
		case 12:
		case 16 ... 19:
		case 22 ... 23:
		case 28 ... 29:
			return "MM_IMGSYS";
		case 10:
			return "MM_IMG_DIP";
		case 11:
		case 20:
			return "MM_IMG_FDVT";
		case 21:
			return "MM_CAM_DPE";
		case 26 ... 27:
		case 30 ... 31:
			return "MM_IMG_QOF";
		case 24:
			return "MM_CAM";
		default:
			return "MM_GCEM";
		}
	}

	return "MM_GCE";
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_SPR_TIMER ... CMDQ_EVENT_SPR_TIMER + 32:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}

	if (gce_pa == GCE_D_PA) // GCE-D
		switch (event) {
		case CMDQ_EVENT_DISP0_STREAM_SOF0
			... CMDQ_EVENT_DISP1_BUF_UNDERRUN_ENG_EVENT10:
		case CMDQ_EVENT_OVL0_STREAM_SOF0
			... CMDQ_EVENT_DPC_DISP_SW_CONFIG_WHEN_MTCMOS_OFF:
		case CMDQ_EVENT_DPTX_DPTX_EVENT0
			... CMDQ_EVENT_EDPTX_EDPTX_EVENT1:
		case CMDQ_EVENT_DSI0_TE_I_DSI0_TE_I
			... CMDQ_EVENT_DSI2_TE_I_DSI2_TE_I:
		case CMDQ_EVENT_DSI3_TE_I_DSI3_TE_I:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY
			... CMDQ_SYNC_TOKEN_VFP_PERIOD:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY_1
			... CMDQ_SYNC_TOKEN_CABC_EOF_1:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY_3
			... CMDQ_SYNC_TOKEN_CABC_EOF_3:
			return "MM_DISP";
		case CMDQ_EVENT_MML0_STREAM_SOF0
			... CMDQ_EVENT_MML0_DISP_MUTEX0_GET_RLZ_ENG_EVENT:
			return "MM_MDP";
		case CMDQ_EVENT_MML1_STREAM_SOF0
			... CMDQ_EVENT_MML1_DISP_MUTEX0_GET_RLZ_ENG_EVENT:
		case CMDQ_EVENT_DPC_MML_SSYS_DT_ERR_ON_BEFORE_OFF
			... CMDQ_EVENT_DPC_MML_SW_CONFIG_WHEN_MTCMOS_OFF:
		case CMDQ_SYNC_TOKEN_MML_BUFA
			... CMDQ_SYNC_TOKEN_MML_APU_START:
			return "MM_MML";
		default:
			return "MM_GCE";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		case CMDQ_EVENT_VENC3_VENC_RESERVED
			... CMDQ_EVENT_VENC1_VENC_SOC_FRAME_DONE:
			return "MM_VENC";
		case CMDQ_EVENT_VDEC1_VDEC_LINE_CNT_INT
			... CMDQ_EVENT_VDEC1_VDEC_LAT_GCE_CNT_OP_THR:
			return "MM_VDEC";
		case CMDQ_EVENT_IMG_IMG_EVENT_0:
			return "MM_IMGSYS";
		case CMDQ_EVENT_IMG_OMC_TNR_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_OMC_TNR_CQ_THR_DONE_P2_5:
		case CMDQ_EVENT_IMG_OMC_LITE_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_OMC_LITE_CQ_THR_DONE_P2_5:
			return "MM_OMC";
		case CMDQ_EVENT_IMG_IMG_EVENT_126
			... CMDQ_EVENT_IMG_IMG_EVENT_127:
			return "MM_IMGSYS";
		case CMDQ_EVENT_IMG_TRAW0_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMG_TRAW0_DUMMY_0:
			return "MM_IMG_TRAW";
		case CMDQ_EVENT_IMG_TRAW1_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMG_TRAW1_DMA_ERR_EVENT:
			return "MM_IMG_LTRAW";
		case CMDQ_EVENT_IMG_IMGSYS_IPE_ME_DONE
			... CMDQ_EVENT_IMG_IMGSYS_IPE_MMG_DONE:
			return "MM_IMG_ME";
		case CMDQ_EVENT_IMG_QOF_ACK_EVENT_0
			... CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_4:
		case CMDQ_SYNC_TOKEN_DIP_POWER_CTRL
			... CMDQ_SYNC_TOKEN_TRAW_PWR_HAND_SHAKE:
			return "MM_IMG_QOF";
		case CMDQ_EVENT_IMG_DIP_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_DIP_DUMMY_0:
			return "MM_IMG_DIP";
		case CMDQ_EVENT_IMG_WPE_EIS_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_EIS_CQ_THR_DONE_P2_5:
		case CMDQ_EVENT_IMG_WPE0_DUMMY_0
			... CMDQ_EVENT_IMG_WPE0_DUMMY_2:
		case CMDQ_EVENT_IMG_WPE1_DUMMY_0
			... CMDQ_EVENT_IMG_WPE_LITE_CQ_THR_DONE_P2_5:
		case CMDQ_EVENT_IMG_WPE2_DUMMY_0
			... CMDQ_EVENT_IMG_WPE2_DUMMY_2:
			return "MM_IMG_WPE"; //WPE_EIS
		case CMDQ_EVENT_IMG_PQDIP_A_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_PQA_DMA_ERR_EVENT:
			return "MM_IMG_PQDIP"; //PQDIP_A
		case CMDQ_EVENT_IMG_PQDIP_B_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_PQB_DMA_ERR_EVENT:
			return "MM_IMG_PQDIP"; //PQDIP_B
		case CMDQ_EVENT_IMG_ADL_TILE_DONE_EVENT
			... CMDQ_EVENT_IMG_ADLWR1_TILE_DONE_EVENT:
			return "MM_IMG_ADL";
		case CMDQ_EVENT_IMG_IMGSYS_IPE_FDVT0_DONE:
			return "MM_IMG_FDVT";
		case CMDQ_EVENT_CAM_CAM_EVENT_0
			... CMDQ_EVENT_CAM_PDA1_IRQO_EVENT_DONE_D1:
		case CMDQ_EVENT_CAM_CAM_EVENT_45
			... CMDQ_EVENT_CAM_CAM_EVENT_127:
		case CMDQ_SYNC_TOKEN_APUSYS_APU:
			return "MM_CAM";
		case CMDQ_EVENT_CAM_DPE_DVP_CMQ_EVENT
			... CMDQ_EVENT_CAM_DPE_DVFG_CMQ_EVENT:
			return "MM_CAM_DPE";
		case CMDQ_EVENT_SMI_EVENT_MMINFRA_SMI_MMSRAM_COMM_SMIASSER
			... CMDQ_EVENT_SMI_EVENT_MMINFRA_SMI_DISP_COMM_SMIASSER:
			return "SMI";

		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_1
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_133:
		case CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS
			... CMDQ_SYNC_TOKEN_IPESYS_ME:
		case CMDQ_SYNC_TOKEN_IMGSYS_VSS_TRAW
			... CMDQ_SYNC_TOKEN_IMGSYS_VSS_DIP:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_134
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_221:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_222
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_250:
			return "MM_IMGSYS";
		case CMDQ_SYNC_TOKEN_DPE_POOL_1
			... CMDQ_SYNC_TOKEN_DPE_POOL_16:
			return "MM_IMG_FRM";
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_251
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_300:
			return "MM_IMGSYS";
		default:
			return "MM_GCEM";
		}

	return "MM_GCE";
}

u32 cmdq_util_hw_id(u32 pa)
{
	switch (pa) {
	case GCE_D_PA:
		return 0;
	case GCE_M_PA:
		return 1;
	default:
		cmdq_err("unknown addr:%x", pa);
	}

	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x3e000100,	/* mmlsys0_config */
		0x32420000,	/* dispsys */
		0x34101200,	/* imgsys */
		0x1000106c,	/* infra */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

void cmdq_test_set_ostd(void)
{
	void __iomem	*va_base;
	u32 val = 0x01014000;
	u32 pa_base;
	u32 preval, newval;

	/* 1. set mdp_smi_common outstanding to 1 : 0x1E80F10C = 0x01014000 */
	pa_base = 0x1E80F10C;
	va_base = ioremap(pa_base, 0x1000);
	preval = readl(va_base);
	writel(val, va_base);
	newval = readl(va_base);
	cmdq_msg("%s addr0x%#x: 0x%#x -> 0x%#x  ", __func__, pa_base, preval, newval);
}

const char *cmdq_util_hw_name(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	if (hw_id == 0)
		return "GCE-D";

	if (hw_id == 1)
		return "GCE-M";

	return "CMDQ";
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

bool cmdq_mbox_hw_trace_thread(void *chan)
{
	const phys_addr_t gce_pa = cmdq_mbox_get_base_pa(chan);
	const s32 idx = cmdq_mbox_chan_id(chan);

	if (gce_pa == GCE_D_PA)
		switch (idx) {
		case 16 ... 19: // MML
			cmdq_log("%s: pa:%pa idx:%d", __func__, &gce_pa, idx);
			return false;
		}

	return true;
}

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
s32 cmdq_get_thread_id(void *chan)
{
	const s32 idx = cmdq_mbox_chan_id(chan);

	return idx;
}

s32 cmdq_check_pkt_finalize(void *pkt)
{
	s32 result = cmdq_pkt_finalize((struct cmdq_pkt *)pkt);

	return result;
}
#endif

void cmdq_error_irq_debug(void *chan)
{
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#if !IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	struct device *dev = cmdq_mbox_get_dev(chan);
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));
	u32 sid = hw_id? GCE_M_NORMAL_SID: GCE_D_NORMAL_SID;

	//dump smmu info to check gce va mode
	mtk_smmu_reg_dump(MM_SMMU, dev, sid);
	//dump gce req
	cmdq_mbox_dump_gce_req(chan);
#endif
#endif
}

bool cmdq_check_tf(struct device *dev,
	u32 sid, u32 tbu, u32 *axids)
{
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#if !IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	struct mtk_smmu_fault_param out_param;

	return mtk_smmu_tf_detect(MM_SMMU, dev,
		sid, tbu, axids, 1, &out_param);
#else
	return false;
#endif
#else
	return false;
#endif
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.test_set_ostd = cmdq_test_set_ostd,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
	.hw_trace_thread = cmdq_mbox_hw_trace_thread,
	.dump_error_irq_debug = cmdq_error_irq_debug,
	.check_tf = cmdq_check_tf,
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	.get_thread_id = cmdq_get_thread_id,
	.check_pkt_finalize = cmdq_check_pkt_finalize,
#endif
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	virtio_cmdq_util_set_fp(&platform_fp);
#endif
	return 0;
}
module_init(cmdq_platform_init);

MODULE_LICENSE("GPL");
