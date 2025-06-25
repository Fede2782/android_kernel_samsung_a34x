// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "cmdq_sec_pkvm.h"

s32 cmdq_sec_pkvm_allocate_shared_memory(struct cmdq_sec_pkvm_context *tee,
	const dma_addr_t MVABase, const u32 size)
{
	return 0;
}

s32 cmdq_sec_pkvm_get_reply(struct cmdq_sec_pkvm_context *tee)
{
	cmdq_msg("%s rsp:%llx", __func__, tee->rsp);
	if (tee->rsp != 0)
		return -(tee->rsp);

	return 0;
}

s32 cmdq_sec_pkvm_allocate_wsm(struct cmdq_sec_pkvm_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2)
{
	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2)
		return -EINVAL;

	*wsm_buffer = kzalloc(size, GFP_KERNEL);
	if (!*wsm_buffer)
		return -ENOMEM;

	*wsm_buf_ex = kzalloc(size_ex, GFP_KERNEL);
	if (!*wsm_buf_ex)
		return -ENOMEM;

	*wsm_buf_ex2 = kzalloc(size_ex2, GFP_KERNEL);
	if (!*wsm_buf_ex2)
		return -ENOMEM;

	return 0;
}

s32 cmdq_sec_pkvm_free_wsm(struct cmdq_sec_pkvm_context *tee,
	void **wsm_buffer, void **wsm_buf_ex, void **wsm_buf_ex2)
{
	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2)
		return -EINVAL;

	kfree(*wsm_buffer);
	*wsm_buffer = NULL;

	kfree(*wsm_buf_ex);
	*wsm_buf_ex = NULL;

	kfree(*wsm_buf_ex2);
	*wsm_buf_ex2 = NULL;

	return 0;
}

s32 cmdq_sec_pkvm_execute_session(struct cmdq_sec_pkvm_context *tee,
	u32 cmd, s32 timeout_ms, u32 thread_idx,
	u32 wait_cookie, s32 scenario_aee)
{
	struct arm_smccc_res res;

	cmdq_mbox_mtcmos_by_fast(NULL, true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, cmd + CMDQ_SMC_REQ_MAX,
		thread_idx, wait_cookie, scenario_aee, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(NULL, false);

	tee->rsp = res.a0;
	cmdq_msg("%s: cmd:%u CMDQ_SMC_REQ_MAX:%u rsp:%llx",
		__func__, cmd, CMDQ_SMC_REQ_MAX, tee->rsp);


	return 0;
}

s32 cmdq_sec_pkvm_open_session(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMD_CMDQ_TL_PKVM_INIT,
		0, 0, 0, 0, 0, 0, &res);

	return 0;
}

MODULE_LICENSE("GPL");
