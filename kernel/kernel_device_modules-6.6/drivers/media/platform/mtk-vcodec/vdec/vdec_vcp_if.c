// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Chia-Mao Hung<chia-mao.hung@mediatek.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/mtk_vcu_controls.h>
#include <linux/delay.h>
#include <soc/mediatek/smi.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#include "vdec_drv_base.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_drv.h"
#include "vdec_drv_if.h"
#include "vcp_ipi_pin.h"
#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/emi.h>
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_status.h"
#endif
// TODO: need remove ISR ipis
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_dec_pm.h"

#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
#define IPI_TIMEOUT_MS          (10000U)
#else
#define IPI_TIMEOUT_MS          (5000U + ((mtk_vcodec_dbg | mtk_v4l2_dbg_level) ? 5000U : 0U))
#endif
#define IPI_SEND_TIMEOUT_MS	1000U
#define IPI_FIRST_DEC_START_TIMEOUT_MS     (60000U)
#define IPI_POLLING_INTERVAL_US    10

struct vcp_dec_mem_list {
	struct vcodec_mem_obj mem;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct list_head list;
};

static void put_fb_to_free(struct vdec_inst *inst, struct vdec_fb *fb)
{
	struct ring_fb_list *list;

	if (inst->put_frame_async)
		list = &inst->list_free_fb;
	else
		list = &inst->vsi->list_free;

	if (inst->put_frame_async)
		mutex_lock(&inst->list_free_fb_lock);

	if (fb != NULL) {
		if (list->count >= DEC_MAX_FB_NUM)
			mtk_vcodec_err(inst, "[FB] put fb free_list full");
		else {
			mtk_vcodec_debug(inst,"[FB] put fb into free_list @(%p, %llx)",
				fb->fb_base[0].va, (u64)fb->fb_base[1].dma_addr);

			list->fb_list[list->write_idx].vdec_fb_va = (u64)(uintptr_t)fb;
			list->write_idx = (list->write_idx == DEC_MAX_FB_NUM - 1U) ? 0U : list->write_idx + 1U;
			list->count++;
		}
	}

	if (inst->put_frame_async)
		mutex_unlock(&inst->list_free_fb_lock);
}

static void get_pic_info(struct vdec_inst *inst, struct vdec_pic_info *pic)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	memcpy(pic, &inst->vsi->pic, sizeof(struct vdec_pic_info));

	mtk_vcodec_debug(inst, "pic(%d, %d), buf(%d, %d), bitdepth = %d, fourcc = %s(0x%x)\n",
		pic->pic_w, pic->pic_h, pic->buf_w, pic->buf_h,
		pic->bitdepth, FOURCC_STR(pic->fourcc), pic->fourcc);
	mtk_vcodec_debug(inst, "Y/C(%d, %d)", pic->fb_sz[0], pic->fb_sz[1]);
}

static void get_crop_info(struct vdec_inst *inst, struct v4l2_rect *r)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	r->left      = inst->vsi->crop.left;
	r->top       = inst->vsi->crop.top;
	r->width     = inst->vsi->crop.width;
	r->height    = inst->vsi->crop.height;
	mtk_vcodec_debug(inst, "l=%d, t=%d, w=%d, h=%d",
		r->left, r->top, r->width, r->height);
}

static void get_dpb_size(struct vdec_inst *inst, unsigned int *dpb_sz)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	*dpb_sz = inst->vsi->dec.dpb_sz;
	if (inst->vsi->low_pw_mode)
		*dpb_sz += mtk_vdec_lpw_limit;
	mtk_vcodec_debug(inst, "sz=%d", *dpb_sz);
}

static void get_dvfs_data(struct mtk_vcodec_dev *dev, unsigned int need)
{
	dev->vdec_dvfs_params.frame_need_update = need;
}

static void check_error_code(struct vdec_inst *inst, unsigned int hw_id)
{
	if (inst->vsi->dec.error_code[hw_id] == 0)
		return;

	mtk_vcodec_debug(inst, "hw_id %d get error_code %d", hw_id, inst->vsi->dec.error_code[hw_id]);
	mtk_vdec_queue_error_code_event(inst->ctx, inst->vsi->dec.error_code[hw_id]);
}

static void vdec_get_fb_list(struct vdec_inst *inst,
	struct ring_fb_list *src_list, struct ring_fb_list *dst_list, struct mutex *dst_list_lock)
{
	mutex_lock(dst_list_lock);
	while (src_list->read_idx != src_list->write_idx) {
		if (dst_list->count >= DEC_MAX_FB_NUM)
			break;

		mtk_vcodec_debug(inst, "%s fb list copy: src read_idx %d write_idx %d count %d, dst read_idx %d write_idx %d count %d, vdec_fb_va 0x%llx",
			dst_list == &inst->list_disp_fb ? "disp" : "free",
			src_list->read_idx, src_list->write_idx, src_list->count,
			dst_list->read_idx, dst_list->write_idx, dst_list->count,
			src_list->fb_list[src_list->read_idx].vdec_fb_va);
		memcpy(&dst_list->fb_list[dst_list->write_idx],
		       &src_list->fb_list[src_list->read_idx], sizeof(struct vdec_fb_entry));
		dst_list->write_idx = (dst_list->write_idx + 1U) % DEC_MAX_FB_NUM;
		dst_list->count++;

		src_list->read_idx = (src_list->read_idx == DEC_MAX_FB_NUM - 1U) ? 0U : src_list->read_idx + 1U;
		src_list->count = (DEC_MAX_FB_NUM + src_list->write_idx - src_list->read_idx) % DEC_MAX_FB_NUM;
	}
	mutex_unlock(dst_list_lock);
}

static int vdec_vcp_ipi_send(struct vdec_inst *inst, void *msg, int len,
	bool is_ack, bool need_wait_suspend, bool has_lock_dvfs)
{
	int ret, ipi_size;
	unsigned long timeout = 0;
	struct share_obj obj;
	unsigned int suspend_block_cnt = 0;
	struct mutex *msg_mutex;
	unsigned int *msg_signaled;
	wait_queue_head_t *msg_wq;
	bool *vcu_in_ipi;
	bool is_res = false;
	int ipi_wait_type = IPI_SEND_WAIT;
	struct mtk_ipi_device *ipidev;
	struct vdec_ap_ipi_cmd *msg_cmd = (struct vdec_ap_ipi_cmd *)msg;
	struct vdec_ap_ipi_cmd_indp *msg_indp = (struct vdec_ap_ipi_cmd_indp *)msg;
	bool use_msg_indp = (is_ack || msg_cmd->msg_id == AP_IPIMSG_DEC_INIT ||
		(msg_cmd->msg_id & IPIMSG_TYPE_BITS) == IPIMSG_NO_INST_OFFSET); // msg use VDEC_MSG_PREFIX

	if ((!use_msg_indp && msg_cmd->vcu_inst_addr == 0) ||
	     (use_msg_indp && msg_indp->ap_inst_addr == 0 && !is_ack)) {
		mtk_vcodec_err(inst, "msg 0x%x inst addr null", msg_cmd->msg_id);
		return -EINVAL;
	}

	if (preempt_count())
		ipi_wait_type = IPI_SEND_POLLING;

	if (msg_cmd->msg_id == AP_IPIMSG_DEC_FRAME_BUFFER) {
		is_res = true;
		msg_mutex = &inst->ctx->dev->ipi_mutex_res;
	} else
		msg_mutex = &inst->ctx->dev->ipi_mutex;

	if (!is_ack) {
		vcodec_trace_begin("msg_mutex(msg 0x%x)", msg_cmd->msg_id);
		mutex_lock(msg_mutex);
		vcodec_trace_end();

		if (need_wait_suspend) {
			while (inst->ctx->dev->is_codec_suspending == 1) {
				mutex_unlock(msg_mutex);
				if (has_lock_dvfs)
					mutex_unlock(&inst->ctx->dev->dec_dvfs_mutex);

				suspend_block_cnt++;
				if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
					mtk_v4l2_debug(0, "VDEC blocked by suspend\n");
					suspend_block_cnt = 0;
				}
				usleep_range(10000, 20000);

				if (has_lock_dvfs)
					mutex_lock(&inst->ctx->dev->dec_dvfs_mutex);
				mutex_lock(msg_mutex);
			}
		}
	}

	if (inst->vcu.abort || inst->vcu.daemon_pid != vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv"))
		goto ipi_err_unlock;

	while (!is_vcp_ready_ex(VDEC_FEATURE_ID)) {
		mtk_v4l2_debug((((timeout % 20) == 10) ? 0 : 4), "[VCP] wait ready %lu ms", timeout);
		mdelay(1);
		timeout++;
		if (timeout > VCP_SYNC_TIMEOUT_MS) {
			mtk_vcodec_err(inst, "VCP_A_ID not ready");
			/* mtk_smi_dbg_hang_detect("VDEC VCP"); */
#if IS_ENABLED(CONFIG_MTK_EMI)
			mtk_emidbg_dump();
#endif
			//BUG_ON(1);
			goto ipi_err_unlock;
		}
	}

	if (len > (sizeof(struct share_obj) - sizeof(int32_t) - sizeof(uint32_t))) {
		mtk_vcodec_err(inst, "ipi data size wrong %d > %lu", len, sizeof(struct share_obj));
		goto ipi_err_unlock;
	}

	memset(&obj, 0, sizeof(obj));
	memcpy(obj.share_buf, msg, len);

	if (is_res) {
		obj.id = IPI_VDEC_RESOURCE;
		msg_signaled = &inst->vcu.signaled_res;
		msg_wq = &inst->vcu.wq_res;
		vcu_in_ipi = &inst->vcu.in_res_ipi;
	} else {
		obj.id = inst->vcu.id;
		msg_signaled = &inst->vcu.signaled;
		msg_wq = &inst->vcu.wq;
		vcu_in_ipi = &inst->vcu.in_ipi;
	}

	obj.len = len;
	ipi_size = ((sizeof(u32) * 2) + len + 3) / 4;
	if (!is_ack) {
		*msg_signaled = false;
		if (!is_res)
			inst->vcu.failure = 0;
		inst->ctx->dev->codec_stop_done = false;
	}
	inst->ctx->err_msg = 0;

	mtk_v4l2_debug(2, "[%d] id %d len %d msg 0x%x is_ack %d %d",
		inst->ctx->id, obj.id, obj.len, msg_cmd->msg_id, is_ack, *msg_signaled);

	ipidev = vcp_get_ipidev(VDEC_FEATURE_ID);
	if (!ipidev) {
		mtk_vcodec_err(inst, "[%d] vcp_get_ipidev(VDEC_FEATURE_ID) get NULL", inst->ctx->id);
		goto ipi_err_wait_and_unlock;
	}
	vcodec_trace_begin("mtk_ipi_send(msg 0x%x is_ack %d)", msg_cmd->msg_id, is_ack);
	ret = mtk_ipi_send(ipidev, IPI_OUT_VDEC_1, ipi_wait_type, &obj,
		ipi_size, IPI_SEND_TIMEOUT_MS);

	if (ret != IPI_ACTION_DONE) {
		mtk_vcodec_err(inst, "mtk_ipi_send %X fail %d", msg_cmd->msg_id, ret);
		if (!is_ack)
			goto ipi_err_wait_and_unlock;
	}

	if (is_ack) {
		vcodec_trace_end();
		return 0;
	}

	// if (!is_ack)

	*vcu_in_ipi = true;
wait_ack:
	/* wait for VCP's ACK */
	if (msg_cmd->msg_id == AP_IPIMSG_DEC_START &&
	    mtk_vcodec_is_state(inst->ctx, MTK_STATE_INIT)) {
		timeout = msecs_to_jiffies(IPI_FIRST_DEC_START_TIMEOUT_MS);
	} else {
		timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
	}

	if (ipi_wait_type == IPI_SEND_POLLING) {
		ret = IPI_TIMEOUT_MS * 1000;
		while ((*msg_signaled) == false) {
			udelay(IPI_POLLING_INTERVAL_US);
			ret -= IPI_POLLING_INTERVAL_US;
			if (ret < 0) {
				ret = 0;
				break;
			}
		}
	} else
		ret = wait_event_timeout(*msg_wq, *msg_signaled, timeout);
	*vcu_in_ipi = false;
	*msg_signaled = false;

	if (ret == 0) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack time out! %d %d",
			msg_cmd->msg_id, ret, inst->vcu.failure);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		dump_vcp_irq_status();
#endif
		goto ipi_err_wait_and_unlock;
	} else if (-ERESTARTSYS == ret) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack ret %d RESTARTSYS retry! (%d)",
			msg_cmd->msg_id, ret, inst->vcu.failure);
		goto wait_ack;
	} else if (ret < 0) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack fail ret %d! (%d)",
			msg_cmd->msg_id, ret, inst->vcu.failure);
	} else if (inst->vcu.abort) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack abort ret %d! (%d)",
			msg_cmd->msg_id, ret, inst->vcu.failure);
		goto ipi_err_wait_and_unlock;
	}
	vcodec_trace_end();
	mutex_unlock(msg_mutex);

	if (!is_res)
		return inst->vcu.failure;

	return 0;

ipi_err_wait_and_unlock:
	timeout = 0;
	vcodec_trace_end();
	if (inst->vcu.daemon_pid == vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv")) {
		vcp_cmd_ex(VDEC_FEATURE_ID, VCP_SET_HALT, "vdec_srv");

		while (inst->vcu.daemon_pid == vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv") ||
			!inst->ctx->dev->codec_stop_done) {
			if (timeout > VCP_SYNC_TIMEOUT_MS) {
				mtk_v4l2_debug(0, "halt restart timeout %x\n",
					inst->vcu.daemon_pid);
				break;
			}
			usleep_range(10000, 20000);
			timeout += 10;
		}
	}
	inst->ctx->dev->codec_stop_done = false;
	inst->vcu.failure = VDEC_IPI_MSG_STATUS_FAIL;
	inst->ctx->err_msg = *(__u32 *)msg;

ipi_err_unlock:
	inst->vcu.abort = 1;
	if (!is_ack)
		mutex_unlock(msg_mutex);

	return -EIO;
}

static void handle_init_ack_msg(struct mtk_vcodec_dev *dev, struct vdec_vcu_ipi_init_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)(unsigned long)msg->ap_inst_addr;
	__u64 shmem_pa_start = (__u64)vcp_get_reserve_mem_phys_ex(VDEC_MEM_ID);
	__u64 inst_offset = ((msg->vcu_inst_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;

	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx", (uintptr_t)msg->ap_inst_addr);

	vcu->init_done = true;
	vcu->vsi = (void *)((__u64)vcp_get_reserve_mem_virt_ex(VDEC_MEM_ID) + inst_offset);
	vcu->inst_addr = msg->vcu_inst_addr;

	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%llx", vcu->inst_addr);
}

static void handle_query_cap_ack_msg(struct vdec_vcu_ipi_query_cap_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	void *data;
	int size = 0;
	__u64 shmem_pa_start = (__u64)vcp_get_reserve_mem_phys_ex(VDEC_MEM_ID);
	__u64 data_offset = ((msg->vcu_data_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL || msg->vcu_data_addr == 0)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx, vcu_data_addr = 0x%llx, id = %d",
		(uintptr_t)msg->ap_inst_addr, msg->vcu_data_addr, msg->id);
	/* mapping VCU address to kernel virtual address */
	data =  (void *)((__u64)vcp_get_reserve_mem_virt_ex(VDEC_MEM_ID) + data_offset);
	if (data == NULL)
		return;
	switch (msg->id) {
	case GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS:
		size = sizeof(struct mtk_video_fmt);
		memcpy((void *)msg->ap_data_addr, data,
			 size * MTK_MAX_DEC_CODECS_SUPPORT);
		break;
	case GET_PARAM_VDEC_CAP_FRAME_SIZES:
		size = sizeof(struct mtk_codec_framesizes);
		memcpy((void *)msg->ap_data_addr, data,
			size * MTK_MAX_DEC_CODECS_SUPPORT);
		break;
	case GET_PARAM_VDEC_CAP_MAX_BUF_INFO:
		size = sizeof(struct vdec_max_buf_info);
		memcpy((void *)msg->ap_data_addr, data,
			size);
		break;
	case GET_PARAM_VDEC_CAP_FRAMEINTERVALS:
		size = sizeof(struct mtk_video_frame_frameintervals);
		memcpy((void *)msg->ap_data_addr, data,
			size);
		break;
	default:
		break;
	}
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%llx", vcu->inst_addr);
}

static struct device *get_dev_by_mem_type(struct vdec_inst *inst, struct vcodec_mem_obj *mem)
{
	if (inst->ctx->dec_params.svp_mode) {
		if (mem->type == MEM_TYPE_FOR_SW)
			mem->type = MEM_TYPE_FOR_SEC_SW;
		else if (mem->type == MEM_TYPE_FOR_HW ||
			 mem->type == MEM_TYPE_FOR_HW_CACHE)
			mem->type = MEM_TYPE_FOR_SEC_HW;
		else if (mem->type == MEM_TYPE_FOR_UBE_HW)
			mem->type = MEM_TYPE_FOR_SEC_UBE_HW;
	}

	if (mem->type == MEM_TYPE_FOR_SW) {
		if (inst->ctx->dev->iommu_domain_swtich && (inst->ctx->id & 1))
			return vcp_get_io_device_ex(VCP_IOMMU_VCP);
		else
			return vcp_get_io_device_ex(VCP_IOMMU_WORK);
	} else if (mem->type == MEM_TYPE_FOR_SEC_SW)
		return vcp_get_io_device_ex(VCP_IOMMU_SEC);
	else if (mem->type == MEM_TYPE_FOR_HW ||
		 mem->type == MEM_TYPE_FOR_HW_CACHE ||
		 mem->type == MEM_TYPE_FOR_SEC_HW)
		return inst->vcu.ctx->dev->smmu_dev;
	else if (mem->type == MEM_TYPE_FOR_UBE_HW || mem->type == MEM_TYPE_FOR_SEC_UBE_HW) {
		if (vcp_get_io_device_ex(VCP_IOMMU_UBE_LAT) != NULL)
			return vcp_get_io_device_ex(VCP_IOMMU_UBE_LAT);
		else if (vcp_get_io_device_ex(VCP_IOMMU_UBE_CORE) != NULL)
			return vcp_get_io_device_ex(VCP_IOMMU_UBE_CORE);
		else
			return inst->vcu.ctx->dev->smmu_dev;
	} else
		return NULL;
}

static void handle_vdec_mem_alloc(struct vdec_vcu_ipi_mem_op *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	struct vdec_inst *inst = NULL;
	struct device *dev = NULL;
	struct vcp_dec_mem_list *tmp = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;

	if (msg->mem.type == MEM_TYPE_FOR_SHM) {
		msg->status = 0;
		msg->mem.va = (__u64)vcp_get_reserve_mem_virt_ex(VDEC_MEM_ID);
		msg->mem.pa = (__u64)vcp_get_reserve_mem_phys_ex(VDEC_MEM_ID);
		msg->mem.len = (__u64)vcp_get_reserve_mem_size_ex(VDEC_MEM_ID);
		msg->mem.iova = msg->mem.pa;
		mtk_v4l2_debug(4, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d size of %lu %lu\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type,
			sizeof(msg->mem), sizeof(*msg));
	} else {
		if (IS_ERR_OR_NULL(vcu))
			return;

		inst = container_of(vcu, struct vdec_inst, vcu);
		dev = get_dev_by_mem_type(inst, &msg->mem);
		msg->status = mtk_vcodec_alloc_mem(&msg->mem, dev, &attach, &sgt, MTK_INST_DECODER);

		mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type);
	}

	if (msg->status) {
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
		/* reset prevent VCP TF */
		msg->mem.pa = 0;
		msg->mem.iova = 0;
	} else if (msg->mem.type != MEM_TYPE_FOR_SHM) {
		tmp = kmalloc(sizeof(struct vcp_dec_mem_list), GFP_KERNEL);
		if (tmp) {
			mutex_lock(vcu->ctx_ipi_lock);
			tmp->attach = attach;
			tmp->sgt = sgt;
			tmp->mem = msg->mem;
			list_add_tail(&tmp->list, &vcu->bufs);
			mutex_unlock(vcu->ctx_ipi_lock);
		}
	}
}

static void handle_vdec_mem_free(struct vdec_vcu_ipi_mem_op *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	struct vdec_inst *inst = NULL;
	struct device *dev = NULL;
	struct vcp_dec_mem_list *tmp = NULL;
	struct list_head *p, *q;
	bool found = 0;

	if (IS_ERR_OR_NULL(vcu))
		return;
	mutex_lock(vcu->ctx_ipi_lock);
	inst = container_of(vcu, struct vdec_inst, vcu);
	dev = get_dev_by_mem_type(inst, &msg->mem);
	list_for_each_safe(p, q, &vcu->bufs) {
		tmp = list_entry(p, struct vcp_dec_mem_list, list);
		if (!memcmp(&tmp->mem, &msg->mem, sizeof(struct vcodec_mem_obj))) {
			found = 1;
			list_del(p);
			break;
		}
	}
	mutex_unlock(vcu->ctx_ipi_lock);
	if (!found) {
		mtk_vcodec_err(vcu, "not found  %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
		return;
	}

	mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
		msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len,  msg->mem.type);

	msg->status = mtk_vcodec_free_mem(&msg->mem, dev, tmp->attach, tmp->sgt);
	kfree(tmp);

	if (msg->status)
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
}

void vdec_dump_mem_buf(unsigned long h_vdec)
{
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	struct list_head *list_ptr, *tmp;
	struct vcp_dec_mem_list *mem_list = NULL;
	unsigned int bs_fourcc;

	if (inst == NULL || inst->ctx == inst->ctx->dev_ctx)
		return;

	bs_fourcc = inst->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;

	mutex_lock(inst->vcu.ctx_ipi_lock);
	list_for_each_safe(list_ptr, tmp, &inst->vcu.bufs) {
		mem_list = list_entry(list_ptr, struct vcp_dec_mem_list, list);
		mtk_v4l2_err("[%d] %s working buffer va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			inst->ctx->id, FOURCC_STR(bs_fourcc), mem_list->mem.va, mem_list->mem.pa,
			mem_list->mem.iova, mem_list->mem.len,  mem_list->mem.type);
	}
	mutex_unlock(inst->vcu.ctx_ipi_lock);
}

static void handle_vdec_vp_mode_prepare(struct mtk_vcodec_dev *dev, struct vdec_inst *inst)
{
	struct vdec_common_vsi *vdec_com_vsi = (struct vdec_common_vsi *)dev->com_vsi;
	struct vdec_vp_mode_buf_info *info = (struct vdec_vp_mode_buf_info *)&vdec_com_vsi->vp_mode_info;
	int idx = (inst->vsi->pic.bitdepth == 8) ? 0 : 1;
	int i, ret_val = 0;

	if (dev->smmu_enabled && !info->alloc_src_buf[idx]) {
		ret_val = mtk_vcodec_vp_mode_buf_prepare(dev, inst->vsi->pic.bitdepth);
		if (ret_val >= 0) {
			for (i = 0; i < 3; i++)
				info->src_buf[idx][i] = dev->vp_mode_buf[idx][i].mem.iova;
			info->alloc_src_buf[idx] = (__u8)true;
		}
	}
	mtk_v4l2_debug(2, "[%d] fill vsi vp mode %d bit src buf[%d] iova: 0x%llx 0x%llx 0x%llx (ret %d)",
		inst->ctx->id, inst->vsi->pic.bitdepth, idx,
		info->src_buf[idx][0], info->src_buf[idx][1], info->src_buf[idx][2], ret_val);
}

static int check_codec_id(struct vdec_vcu_ipi_ack *msg, unsigned int fmt, unsigned int svp)
{
	int codec_id = 0, ret = 0;

	switch (fmt) {
	case V4L2_PIX_FMT_H264:
		codec_id = VDEC_H264;
		break;
	case V4L2_PIX_FMT_HEVC:
		codec_id = VDEC_H265;
		break;
	case V4L2_PIX_FMT_HEIF:
		codec_id = VDEC_HEIF;
		break;
	case V4L2_PIX_FMT_VP8:
		codec_id = VDEC_VP8;
		break;
	case V4L2_PIX_FMT_VP9:
		codec_id = VDEC_VP9;
		break;
	case V4L2_PIX_FMT_MPEG4:
		codec_id = VDEC_MPEG4;
		break;
	case V4L2_PIX_FMT_H263:
		codec_id = VDEC_H263;
		break;
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
		codec_id = VDEC_MPEG12;
		break;
	case V4L2_PIX_FMT_WMV1:
	case V4L2_PIX_FMT_WMV2:
	case V4L2_PIX_FMT_WMV3:
	case V4L2_PIX_FMT_WMVA:
	case V4L2_PIX_FMT_WVC1:
		codec_id = VDEC_WMV;
		break;
	case V4L2_PIX_FMT_RV30:
		codec_id = VDEC_RV30;
		break;
	case V4L2_PIX_FMT_RV40:
		codec_id = VDEC_RV40;
		break;
	case V4L2_PIX_FMT_AV1:
		codec_id = VDEC_AV1;
		break;
	default:
		pr_info("%s no fourcc", __func__);
		break;
	}

	if (codec_id == 0) {
		pr_info("[error] vdec unsupported fourcc\n");
		ret = -1;
	} else if (msg->id == codec_id && msg->status == svp) {
		pr_info("%s ipi id %d svp %d is correct\n", __func__, msg->id, msg->status);
		ret = 0;
	} else {
		mtk_v4l2_debug(2, "[Info] ipi id %d svp %d is incorrect\n",
			msg->id, msg->status);
		ret = -1;
	}
	return ret;
}

static void vdec_vcp_free_mq_node(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_msg_node *mq_node)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->mq.lock, flags);
	list_add(&mq_node->list, &dev->mq.nodes);
	spin_unlock_irqrestore(&dev->mq.lock, flags);
}

int vcp_dec_ipi_handler(void *arg)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)arg;
	struct vdec_vcu_ipi_ack *msg = NULL;
	struct vdec_vcu_inst *vcu = NULL;
	struct vdec_inst *inst = NULL;
	struct share_obj *obj;
	int ret = 0;
	struct mtk_vcodec_msg_node *mq_node = NULL;
	struct vdec_vcu_ipi_mem_op *shem_msg;
	unsigned long flags;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *temp_ctx, *ctx;
	int msg_valid = 0;
	struct sched_param sched_p = { .sched_priority = MTK_VCODEC_IPI_THREAD_PRIORITY };

	mtk_v4l2_debug_enter();
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_cmd) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_init) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_dec_start) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_set_param) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_query_cap) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_init_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_query_cap_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_mem_op) > SHARE_BUF_SIZE);

	sched_setscheduler(current, SCHED_FIFO, &sched_p);

	do {
		if (mq_node != NULL)
			vdec_vcp_free_mq_node(dev, mq_node);

		mq_node = NULL;
		ret = wait_event_interruptible(dev->mq.wq, atomic_read(&dev->mq.cnt) > 0);
		if (ret < 0) {
			mtk_v4l2_debug(0, "wait event return %d (suspending %d)\n",
				ret, atomic_read(&dev->mq.cnt));
			continue;
		}

		spin_lock_irqsave(&dev->mq.lock, flags);
		mq_node = list_entry(dev->mq.head.next, struct mtk_vcodec_msg_node, list);
		list_del(&(mq_node->list));
		atomic_dec(&dev->mq.cnt);
		spin_unlock_irqrestore(&dev->mq.lock, flags);

		obj = &mq_node->ipi_data;
		msg = (struct vdec_vcu_ipi_ack *)obj->share_buf;

		if (msg == NULL || (struct vdec_vcu_inst *)msg->ap_inst_addr == NULL) {
			mtk_v4l2_err(" msg invalid %lx (msg 0x%x)\n",
				(unsigned long)msg, msg ? msg->msg_id : 0);
			continue;
		}

		/* handling VSI (shared memory) preparation when VCP init service without inst*/
		if (msg->msg_id == VCU_IPIMSG_DEC_MEM_ALLOC) {
			shem_msg = (struct vdec_vcu_ipi_mem_op *)obj->share_buf;
			if (shem_msg->mem.type == MEM_TYPE_FOR_SHM) {
				struct mtk_ipi_device *ipidev;
				struct vdec_common_vsi *vdec_com_vsi;

				handle_vdec_mem_alloc((void *)shem_msg);
				shem_msg->vcp_addr[0] = (__u32)VCP_PACK_IOVA(
					vcp_get_reserve_mem_phys_ex(VDEC_SET_PROP_MEM_ID));
				shem_msg->vcp_addr[1] = (__u32)VCP_PACK_IOVA(
					vcp_get_reserve_mem_phys_ex(VDEC_VCP_LOG_INFO_ID));

				dev->com_vsi = (void *)vcp_get_reserve_mem_virt_ex(VDEC_MEM_ID);
				vdec_com_vsi = (struct vdec_common_vsi *)dev->com_vsi;
				dev->tf_info = (struct mtk_tf_info *)&vdec_com_vsi->tf_info;
				vdec_com_vsi->vp_mode_info.enable_smmu = (__u8)dev->smmu_enabled;
				mtk_v4l2_debug(2, "com_vsi 0x%lx, tf_info 0x%lx, enable_smmu %d",
					(unsigned long)dev->com_vsi, (unsigned long)dev->tf_info,
					vdec_com_vsi->vp_mode_info.enable_smmu);

				shem_msg->msg_id = AP_IPIMSG_DEC_MEM_ALLOC_DONE;
				ipidev = vcp_get_ipidev(VDEC_FEATURE_ID);
				if (!ipidev)
					mtk_v4l2_err("[%d] vcp_get_ipidev(VDEC_FEATURE_ID) get NULL", shem_msg->ctx_id);
				else {
					ret = mtk_ipi_send(ipidev, IPI_OUT_VDEC_1, IPI_SEND_WAIT, obj,
						PIN_OUT_SIZE_VDEC, 100);
					if (ret != IPI_ACTION_DONE)
						mtk_v4l2_err("mtk_ipi_send (msg_id %X) fail %d", msg->msg_id, ret);
				}
				continue;
			}
		}
		vcu = (struct vdec_vcu_inst *)(unsigned long)msg->ap_inst_addr;

		/* Check IPI inst is valid */
		mutex_lock(&dev->ctx_mutex);
		msg_valid = 0;
		list_for_each_safe(p, q, &dev->ctx_list) {
			temp_ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			inst = (struct vdec_inst *)temp_ctx->drv_handle;
			if (inst != NULL && vcu == &inst->vcu && vcu->ctx == temp_ctx) {
				msg_valid = 1;
				ctx = vcu->ctx;
				mutex_lock(&ctx->ipi_use_lock);
				break;
			}
		}
		if (!msg_valid) {
			if (vcu) {
				inst = container_of(vcu, struct vdec_inst, vcu);
				ctx = vcu->ctx;
			} else {
				inst = NULL;
				ctx = NULL;
			}
			mtk_v4l2_err(" msg msg_id %X vcu not exist 0x%lx (ctx 0x%lx, inst 0x%lx)\n", msg->msg_id,
				(unsigned long)vcu, (unsigned long)ctx, (unsigned long)inst);
			mtk_vcodec_dump_ctx_list(dev, 0);
			mutex_unlock(&dev->ctx_mutex);
			continue;
		}
		mutex_unlock(&dev->ctx_mutex);

		if (vcu->abort || vcu->daemon_pid != vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv")) {
			mtk_vcodec_err(vcu, " msg msg_id %X vcu abort %d %d\n",
				msg->msg_id, vcu->daemon_pid, vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv"));
			mutex_unlock(&ctx->ipi_use_lock);
			continue;
		}
		mtk_v4l2_debug(2, "[%d] pop msg_id %X ml_cnt %d, vcu %lx, status %d", vcu->ctx->id,
			msg->msg_id, atomic_read(&dev->mq.cnt), (unsigned long)vcu, msg->status);

		inst = container_of(vcu, struct vdec_inst, vcu);

		if (msg->msg_id == VCU_IPIMSG_DEC_CHECK_CODEC_ID) {

			if (check_codec_id(msg, vcu->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc,
				vcu->ctx->dec_params.svp_mode) == 0)
				msg->status = 0;
			else
				msg->status = -1;

			msg->msg_id = AP_IPIMSG_DEC_CHECK_CODEC_ID_DONE;
			msg->ctx_id = inst->ctx->id;
			vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
		} else if (msg->status == VDEC_IPI_MSG_STATUS_OK) {
			if ((msg->msg_id & 0xF000) == VCU_IPIMSG_VDEC_SEND_BASE)
				msg->ctx_id = inst->ctx->id;
			switch (msg->msg_id) {
			case VCU_IPIMSG_DEC_DONE:
				vcu->signaled_res = true;
				wake_up(&vcu->wq_res);
				break;
			case VCU_IPIMSG_DEC_INIT_DONE:
				handle_init_ack_msg(dev, (void *)obj->share_buf);
				mtk_vcodec_set_state_from(vcu->ctx, MTK_STATE_INIT, MTK_STATE_FREE);
				goto return_vdec_ipi_ack;
			case VCU_IPIMSG_DEC_DEINIT_DONE:
				mtk_vcodec_set_state(vcu->ctx, MTK_STATE_FREE);
				goto return_vdec_ipi_ack;
			case VCU_IPIMSG_DEC_QUERY_CAP_DONE:
				handle_query_cap_ack_msg((void *)obj->share_buf);
				goto return_vdec_ipi_ack;
			case VCU_IPIMSG_DEC_PWR_CTRL_DONE: {
				struct vdec_ap_ipi_pwr_ctrl *ack_msg =
					(struct vdec_ap_ipi_pwr_ctrl *)obj->share_buf;
				struct mtk_smi_pwr_ctrl_info *ctrl_info =
					(struct mtk_smi_pwr_ctrl_info *)ack_msg->ap_data_addr;

				ctrl_info->ret = ack_msg->info.ret;
				goto return_vdec_ipi_ack;
			}
			case VCU_IPIMSG_DEC_START_DONE:
			case VCU_IPIMSG_DEC_RESET_DONE:
			case VCU_IPIMSG_DEC_SET_PARAM_DONE:
			case VCU_IPIMSG_DEC_BACKUP_DONE:
			case VCU_IPIMSG_DEC_RESUME_DONE:
return_vdec_ipi_ack:
				vcu->signaled = true;
				wake_up(&vcu->wq);
				break;
			case VCU_IPIMSG_DEC_PUT_FRAME_BUFFER:
				inst->put_frame_async = false;
				vcodec_trace_begin("vdec_ipi(PUT_FRAME_BUFFER)");
				check_error_code(inst, MTK_VDEC_CORE);
				mtk_vdec_put_fb(vcu->ctx, PUT_BUFFER_CALLBACK, msg->data != 0);
				msg->msg_id = AP_IPIMSG_DEC_PUT_FRAME_BUFFER_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				vcodec_trace_end();
				break;
			case VCU_ASYNCIPIMSG_DEC_PUT_FRAME_BUFFER:
				inst->put_frame_async = true;
				vcodec_trace_begin("vdec_ipi(ASYNC_PUT_FRAME_BUFFER)");
				check_error_code(inst, MTK_VDEC_CORE);
				vdec_get_fb_list(inst,
					&inst->vsi->list_disp, &inst->list_disp_fb, &inst->list_disp_fb_lock);
				vdec_get_fb_list(inst,
					&inst->vsi->list_free, &inst->list_free_fb, &inst->list_free_fb_lock);
				mtk_vdec_put_fb(vcu->ctx, PUT_BUFFER_CALLBACK, msg->data != 0);
				// async one way ipi, no need ack
				vcodec_trace_end();
				break;
			case VCU_IPIMSG_DEC_MEM_ALLOC:
				vcodec_trace_begin("vdec_ipi(MEM_ALLOC)");
				handle_vdec_mem_alloc((void *)obj->share_buf);
				msg->msg_id = AP_IPIMSG_DEC_MEM_ALLOC_DONE;
				vdec_vcp_ipi_send(inst, msg,
					sizeof(struct vdec_vcu_ipi_mem_op), true, false, false);
				vcodec_trace_end();
				break;
			case VCU_IPIMSG_DEC_MEM_FREE:
				vcodec_trace_begin("vdec_ipi(MEM_FREE)");
				handle_vdec_mem_free((void *)obj->share_buf);
				msg->msg_id = AP_IPIMSG_DEC_MEM_FREE_DONE;
				vdec_vcp_ipi_send(inst, msg,
					sizeof(struct vdec_vcu_ipi_mem_op), true, false, false);
				vcodec_trace_end();
				break;
			// TODO: need remove HW locks /power & ISR ipis
			case VCU_IPIMSG_DEC_LOCK_LAT:
				get_dvfs_data(vcu->ctx->dev, msg->data);
				vdec_decode_prepare(vcu->ctx, MTK_VDEC_LAT);
				atomic_set(&dev->dec_hw_active[MTK_VDEC_LAT], 1);
				msg->msg_id = AP_IPIMSG_DEC_LOCK_LAT_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			case VCU_IPIMSG_DEC_UNLOCK_LAT:
				get_dvfs_data(vcu->ctx->dev, msg->data);
				atomic_set(&dev->dec_hw_active[MTK_VDEC_LAT], 0);
				vdec_decode_unprepare(vcu->ctx, MTK_VDEC_LAT);
				msg->msg_id = AP_IPIMSG_DEC_UNLOCK_LAT_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			case VCU_IPIMSG_DEC_LOCK_CORE:
				if (msg->payload) {
					mtk_vcodec_dec_pw_on(&vcu->ctx->dev->pm);
					dev->dec_ao_pw_cnt++;
				} else {
					get_dvfs_data(vcu->ctx->dev, msg->data);
					vdec_decode_prepare(vcu->ctx, MTK_VDEC_CORE);
					atomic_set(&dev->dec_hw_active[MTK_VDEC_CORE], 1);
				}
				msg->msg_id = AP_IPIMSG_DEC_LOCK_CORE_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			case VCU_IPIMSG_DEC_UNLOCK_CORE:
				if (msg->payload) {
					dev->dec_ao_pw_cnt--;
					mtk_vcodec_dec_pw_off(&vcu->ctx->dev->pm);
				} else {
					get_dvfs_data(vcu->ctx->dev, msg->data);
					atomic_set(&dev->dec_hw_active[MTK_VDEC_CORE], 0);
					vdec_decode_unprepare(vcu->ctx, MTK_VDEC_CORE);
				}
				msg->msg_id = AP_IPIMSG_DEC_UNLOCK_CORE_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			case VCU_IPIMSG_DEC_WAITISR:
				vcodec_trace_count("VDEC_HW_CORE", 2);
				/* wait decoder done interrupt */
				ret = mtk_vcodec_wait_for_done_ctx(vcu->ctx,
					msg->status,
					MTK_INST_IRQ_RECEIVED,
					WAIT_INTR_TIMEOUT_MS);
				msg->msg_id = AP_IPIMSG_DEC_WAITISR_DONE;
				msg->status = ret;
				vcodec_trace_count("VDEC_HW_CORE", 1);
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			case VCU_IPIMSG_DEC_GET_KERNEL_PARAM:
				if (msg->id == GET_KPARAM_VP_MODE_BUF)
					handle_vdec_vp_mode_prepare(dev, inst);

				msg->msg_id = AP_IPIMSG_DEC_GET_KERNEL_PARAM_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			case VCU_IPIMSG_DEC_GET_FRAME_BUFFER:
				mtk_vcodec_err(vcu, "%d GET_FRAME_BUFFER not support", msg->msg_id);
				break;
			case VCU_IPIMSG_DEC_SMI_DBG_DUMP:
				mtk_v4l2_debug(0, "[VDEC] start smi dbg dump");
				atomic_inc(&dev->smi_dump_ref_cnt);
				if (dev->power_in_vcp && mtk_vcodec_is_vcp(MTK_INST_DECODER))
					mtk_smi_dbg_dump_for_vdec();
				else
					mtk_smi_dbg_hang_detect("vdec smi debug dump");
				atomic_dec(&dev->smi_dump_ref_cnt);

				msg->msg_id = AP_IPIMSG_DEC_SMI_DBG_DUMP_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				break;
			default:
				mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
				break;
			}
		} else {
			switch (msg->msg_id) {
			case VCU_IPIMSG_DEC_DONE:
				vcu->signaled_res = true;
				wake_up(&vcu->wq_res);
				break;
			case VCU_IPIMSG_DEC_INIT_DONE:
			case VCU_IPIMSG_DEC_START_DONE:
			case VCU_IPIMSG_DEC_DEINIT_DONE:
			case VCU_IPIMSG_DEC_RESET_DONE:
			case VCU_IPIMSG_DEC_SET_PARAM_DONE:
			case VCU_IPIMSG_DEC_QUERY_CAP_DONE:
			case VCU_IPIMSG_DEC_BACKUP_DONE:
			case VCU_IPIMSG_DEC_RESUME_DONE:
				vcu->failure = msg->status;
				vcu->signaled = true;
				wake_up(&vcu->wq);
				break;
			case VCU_IPIMSG_DEC_WAITISR:
				vcodec_trace_count("VDEC_HW_LAT", 2);
				/* wait decoder done interrupt */
				ret = mtk_vcodec_wait_for_done_ctx(vcu->ctx,
					msg->status,
					MTK_INST_IRQ_RECEIVED,
					WAIT_INTR_TIMEOUT_MS);
				msg->msg_id = AP_IPIMSG_DEC_WAITISR_DONE;
				msg->ctx_id = inst->ctx->id;
				msg->status = ret;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
				vcodec_trace_count("VDEC_HW_LAT", 1);
				break;
			default:
				mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
				break;
			}
		}
		mtk_vcodec_debug(vcu, "- id=%X", msg->msg_id);
		mutex_unlock(&ctx->ipi_use_lock);
	} while (!kthread_should_stop());
	mtk_v4l2_debug_leave();

	return ret;
}

static int vdec_vcp_ipi_isr(unsigned int id, void *prdata, void *data, unsigned int len)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)prdata;
	struct vdec_vcu_ipi_ack *msg = NULL;
	struct share_obj *obj = (struct share_obj *)data;
	struct mtk_vcodec_msg_node *mq_node;
	unsigned long flags;

	msg = (struct vdec_vcu_ipi_ack *)obj->share_buf;

	// add to ipi msg list
	spin_lock_irqsave(&dev->mq.lock, flags);
	if (!list_empty(&dev->mq.nodes)) {
		mq_node = list_entry(dev->mq.nodes.next, struct mtk_vcodec_msg_node, list);
		memcpy(&mq_node->ipi_data, obj, sizeof(struct share_obj));
		list_move_tail(&mq_node->list, &dev->mq.head);
		atomic_inc(&dev->mq.cnt);
		spin_unlock_irqrestore(&dev->mq.lock, flags);
		mtk_v4l2_debug(8, "push ipi_id %x msg_id %x, ml_cnt %d",
			obj->id, msg->msg_id, atomic_read(&dev->mq.cnt));
		wake_up(&dev->mq.wq);
	} else {
		spin_unlock_irqrestore(&dev->mq.lock, flags);
		mtk_v4l2_err("mq no free nodes\n");
	}

	return 0;
}

static void vdec_vcp_set_vcu(struct vdec_vcu_inst *vcu)
{
	vcu->daemon_pid = vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv");
	if (vcu->ctx == vcu->ctx->dev_ctx)
		vcu->abort = false;
}

static int vdec_vcp_backup(struct vdec_inst *inst)
{
	struct vdec_ap_ipi_cmd_indp msg;
	int err = 0;

	if (!inst)
		return -EINVAL;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_BACKUP;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	vdec_vcp_set_vcu(&inst->vcu);

	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, false, false);
	mtk_vcodec_debug(inst, "- ret=%d", err);

	return err;
}

static int vdec_vcp_resume(struct vdec_inst *inst)
{
	struct vdec_ap_ipi_cmd_indp msg;
	int err = 0;

	if (!inst)
		return -EINVAL;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_RESUME;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	vdec_vcp_set_vcu(&inst->vcu);

	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, false, false);
	mtk_vcodec_debug(inst, "- ret=%d", err);

	return err;
}

static bool has_valid_vcp_inst(struct mtk_vcodec_dev *dev)
{
	struct mtk_vcodec_ctx *ctx;
	struct vdec_inst *inst;
	int curr_daemon_pid = vcp_cmd_ex(VDEC_FEATURE_ID, VCP_GET_GEN, "vdec_srv");

	list_for_each_entry(ctx, &dev->ctx_list, list) {
		if (ctx != NULL && ctx->drv_handle != 0 && ctx != &dev->dev_ctx) {
			inst = (struct vdec_inst *)ctx->drv_handle;
			if (inst->vcu.init_done && !inst->vcu.abort && inst->vcu.daemon_pid == curr_daemon_pid)
				return true;
		}
	}
	return false;
}

static int vcp_vdec_notify_callback(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct mtk_vcodec_dev *dev;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *ctx = NULL;
	struct mtk_vcodec_ctx *lat_ctx, *core_ctx;
	int timeout = 0;
	struct vdec_inst *inst = NULL;
	int val, wait_cnt, i;
	bool need_backup;

	if (!mtk_vcodec_is_vcp(MTK_INST_DECODER))
		return 0;

	dev = container_of(this, struct mtk_vcodec_dev, vcp_notify);

	switch (event) {
	case VCP_EVENT_STOP:
		while (atomic_read(&dev->mq.cnt)) {
			timeout++;
			mdelay(1);
			if (timeout > VCP_SYNC_TIMEOUT_MS) {
				mtk_v4l2_debug(0, "VCP_EVENT_STOP timeout\n");
				break;
			}
		}

		lat_ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_LAT);
		core_ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_CORE);
		mtk_v4l2_debug(0, "dec power before VCP_EVENT_STOP: LAT %d (ctx %d)(clk ref %d), CORE %d (ctx %d)(clk red %d), ao %d, ref %d\n",
			dev->dec_is_power_on[MTK_VDEC_LAT], lat_ctx ? lat_ctx->id : -1,
			atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_LAT]),
			dev->dec_is_power_on[MTK_VDEC_CORE], core_ctx ? core_ctx->id : -1,
			atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_CORE]),
			dev->dec_ao_pw_cnt, atomic_read(&dev->larb_ref_cnt));

		mutex_lock(&dev->ctx_mutex);
		// release all ctx ipi
		list_for_each_safe(p, q, &dev->ctx_list) {
			ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			if (ctx != NULL && ctx->drv_handle != 0) {
				inst = (struct vdec_inst *)(ctx->drv_handle);
				inst->vcu.failure = VDEC_IPI_MSG_STATUS_FAIL;
				inst->vcu.abort = 1;
				if (inst->vcu.in_ipi) {
					inst->vcu.signaled = true;
					wake_up(&inst->vcu.wq);
				}
				if (inst->vcu.in_res_ipi) {
					inst->vcu.signaled_res = true;
					wake_up(&inst->vcu.wq_res);
				}
			}
		}
		// check release all ctx lock
		list_for_each_safe(p, q, &dev->ctx_list) {
			ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			if (ctx != NULL && ctx != ctx->dev_ctx && !mtk_vcodec_is_state(ctx, MTK_STATE_ABORT))
				mtk_vdec_error_handle(ctx, "STOP");
		}
		mutex_unlock(&dev->ctx_mutex);
		while (dev->dec_ao_pw_cnt > 0) {
			dev->dec_ao_pw_cnt--;
			mtk_vcodec_dec_pw_off(&dev->pm);
		}
		if (atomic_read(&dev->larb_ref_cnt) != 0) {
			lat_ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_LAT);
			core_ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_CORE);
			mtk_v4l2_err("dec power after VCP_EVENT_STOP: LAT %d (ctx %d)(clk ref %d), CORE %d (ctx %d)(clk red %d), ao %d, ref %d\n",
				dev->dec_is_power_on[MTK_VDEC_LAT], lat_ctx ? lat_ctx->id : -1,
				atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_LAT]),
				dev->dec_is_power_on[MTK_VDEC_CORE], core_ctx ? core_ctx->id : -1,
				atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_CORE]),
				dev->dec_ao_pw_cnt, atomic_read(&dev->larb_ref_cnt));
		}
		dev->codec_stop_done = true;
		break;
	case VCP_EVENT_SUSPEND:
		mutex_lock(&dev->dec_dvfs_mutex);
		dev->is_codec_suspending = 1;
		mutex_unlock(&dev->dec_dvfs_mutex);

		// check no more ipi in progress
		mutex_lock(&dev->ipi_mutex);
		mutex_lock(&dev->ipi_mutex_res);
		mutex_unlock(&dev->ipi_mutex_res);
		mutex_unlock(&dev->ipi_mutex);

		// send backup ipi to vcp by dev_ctx if vcp has inst
		mutex_lock(&dev->ctx_mutex);
		mtk_vcodec_alive_checker_suspend(dev);
		need_backup = has_valid_vcp_inst(dev);
		mutex_unlock(&dev->ctx_mutex);

		mtk_v4l2_debug(0, "%sbackup (dvfs freq %d)(pw ref %d, %d %d)(hw active %d %d)",
			need_backup ? "" : "no need ",
			dev->vdec_dvfs_params.target_freq,
			atomic_read(&dev->larb_ref_cnt),
			atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_LAT]),
			atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_CORE]),
			atomic_read(&dev->dec_hw_active[MTK_VDEC_LAT]),
			atomic_read(&dev->dec_hw_active[MTK_VDEC_CORE]));

		if (need_backup) {
			vdec_vcp_backup((struct vdec_inst *)dev->dev_ctx.drv_handle);
			dev->has_backup = true;
		}

		// check all hw lock is released
		for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
			val = down_trylock(&dev->dec_sem[i]);
			for (wait_cnt = 0; val == 1 && wait_cnt < 5; wait_cnt++) {
				usleep_range(10000, 20000);
				val = down_trylock(&dev->dec_sem[i]);
			}
			if (val == 1)
				mtk_v4l2_err("waiting hw_id %d relase lock fail", i);
			else
				up(&dev->dec_sem[i]);
		}

		// wait msg q of ipi_recv all done
		while (atomic_read(&dev->mq.cnt)) {
			timeout += 20;
			usleep_range(10000, 20000);
			if (timeout > VCP_SYNC_TIMEOUT_MS) {
				mtk_v4l2_err("VCP_EVENT_SUSPEND timeout\n");
				break;
			}
		}
	break;
	case VCP_EVENT_RESUME:
		mutex_lock(&dev->ctx_mutex);
		mtk_vcodec_alive_checker_resume(dev);
		mutex_unlock(&dev->ctx_mutex);

		if (dev->has_backup) {
			mtk_v4l2_debug(0, "restore (dvfs freq %d)(pw ref %d, %d %d)(hw active %d %d)",
				dev->vdec_dvfs_params.target_freq,
				atomic_read(&dev->larb_ref_cnt),
				atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_LAT]),
				atomic_read(&dev->dec_clk_ref_cnt[MTK_VDEC_CORE]),
				atomic_read(&dev->dec_hw_active[MTK_VDEC_LAT]),
				atomic_read(&dev->dec_hw_active[MTK_VDEC_CORE]));
			vdec_vcp_resume((struct vdec_inst *)dev->dev_ctx.drv_handle);
			dev->has_backup = false;
		}

		dev->is_codec_suspending = 0;
	break;
	}
	return NOTIFY_DONE;
}

void vdec_vcp_probe(struct mtk_vcodec_dev *dev)
{
	int ret, i;
	struct mtk_vcodec_msg_node *mq_node;
	struct mtk_ipi_device *ipidev;

	mtk_v4l2_debug_enter();
	INIT_LIST_HEAD(&dev->mq.head);
	spin_lock_init(&dev->mq.lock);
	init_waitqueue_head(&dev->mq.wq);
	atomic_set(&dev->mq.cnt, 0);

	INIT_LIST_HEAD(&dev->mq.nodes);
	for (i = 0; i < MTK_VCODEC_MAX_MQ_NODE_CNT; i++) {
		mq_node = kmalloc(sizeof(struct mtk_vcodec_msg_node), GFP_DMA | GFP_ATOMIC);
		list_add(&mq_node->list, &dev->mq.nodes);
	}

	if (!VCU_FPTR(vcu_load_firmware))
		mtk_vcodec_vcp |= 1 << MTK_INST_DECODER;

	ipidev = vcp_get_ipidev(VDEC_FEATURE_ID);
	if (!ipidev)
		mtk_v4l2_err("vcp_get_ipidev(VDEC_FEATURE_ID) get NULL, can't register ipi");
	else {
		ret = mtk_ipi_register(ipidev, IPI_IN_VDEC_1, vdec_vcp_ipi_isr, dev, &dev->dec_ipi_data);
		if (ret)
			mtk_v4l2_debug(0, " ipi_register, ret %d\n", ret);
	}

	kthread_run(vcp_dec_ipi_handler, dev, "vdec_ipi_recv");

	dev->vcp_notify.notifier_call = vcp_vdec_notify_callback;
	dev->vcp_notify.priority = 1;
	vcp_A_register_notify_ex(VDEC_FEATURE_ID, &dev->vcp_notify);

	mtk_v4l2_debug_leave();
}

void vdec_vcp_remove(struct mtk_vcodec_dev *dev)
{
	int timeout = 0;
	struct mtk_vcodec_msg_node *mq_node, *next;
	unsigned long flags;

	while (atomic_read(&dev->mq.cnt)) {
		timeout++;
		mdelay(1);
		if (timeout > VCP_SYNC_TIMEOUT_MS) {
			mtk_v4l2_err("wait msgq empty timeout\n");
			break;
		}
	}

	spin_lock_irqsave(&dev->mq.lock, flags);
	list_for_each_entry_safe(mq_node, next, &dev->mq.nodes, list) {
		list_del(&(mq_node->list));
		kfree(mq_node);
	}
	spin_unlock_irqrestore(&dev->mq.lock, flags);
}

static int vdec_vcp_init(struct mtk_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	int err = 0;
	struct vdec_ap_ipi_init msg;
	struct vdec_inst *inst = NULL;
	__u32 fourcc;

	vcodec_trace_begin_func();

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;
	if (!ctx) {
		err = -ENOMEM;
		goto error_free_inst;
	}
	mutex_init(&inst->list_disp_fb_lock);
	mutex_init(&inst->list_free_fb_lock);

	inst->ctx = ctx;
	fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;

	inst->vcu.id = IPI_VDEC_COMMON;
	inst->vcu.ctx = ctx;
	init_waitqueue_head(&inst->vcu.wq);
	init_waitqueue_head(&inst->vcu.wq_res);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_INIT;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (unsigned long)&inst->vcu;

	inst->vcu.ctx_ipi_lock = kzalloc(sizeof(struct mutex),
		GFP_KERNEL);
	if (!inst->vcu.ctx_ipi_lock)
		goto error_free_inst;
	mutex_init(inst->vcu.ctx_ipi_lock);
	INIT_LIST_HEAD(&inst->vcu.bufs);

	mtk_vcodec_debug(inst, "vdec_inst=%p svp_mode=%d",
		&inst->vcu, ctx->dec_params.svp_mode);
	*h_vdec = (unsigned long)inst;
	vdec_vcp_set_vcu(&inst->vcu);

	mtk_vcodec_add_ctx_list(ctx);

	vcodec_trace_begin("%s(ipi)", __func__);
	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
	vcodec_trace_end();

	if (err != 0) {
		mtk_vcodec_err(inst, "%s err=%d", __func__, err);
		goto error_free_inst_and_list;
	}

	inst->vsi = (struct vdec_vsi *)inst->vcu.vsi;
	inst->vcu.signaled = false;
	inst->vcu.signaled_res = false;
	inst->put_frame_async = false;
	ctx->input_driven = inst->vsi->input_driven;
	ctx->output_async = inst->vsi->output_async;
	ctx->ipi_blocked = &inst->vsi->ipi_blocked;
	*(ctx->ipi_blocked) = 0;
	ctx->low_pw_mode = inst->vsi->low_pw_mode;

	mtk_v4l2_debug(0, "[%d] %s(%d) Decoder Instance >> %p, ap_inst_addr %llx",
		ctx->id, FOURCC_STR(fourcc),
		inst->vcu.id, inst, msg.ap_inst_addr);

	vcodec_trace_end();
	return 0;

error_free_inst_and_list:
	mtk_vcodec_del_ctx_list(ctx);
error_free_inst:
	kfree(inst->vcu.ctx_ipi_lock);
	kfree(inst);
	*h_vdec = (unsigned long)NULL;

	return err;
}

static void vdec_vcp_deinit(unsigned long h_vdec)
{
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	struct vdec_ap_ipi_cmd msg;
	struct vcp_dec_mem_list *tmp = NULL;
	struct list_head *p, *q;
	struct device *dev = NULL;
	int err = 0;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_DEINIT;
	msg.ctx_id = inst->ctx->id;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	vcodec_trace_begin("%s(ipi)", __func__);
	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
	vcodec_trace_end();
	mtk_vcodec_debug(inst, "- ret=%d", err);

	mtk_vcodec_del_ctx_list(inst->ctx);

	if (inst->ctx->dev->smmu_enabled && mtk_vcodec_ctx_list_empty(inst->ctx->dev)) {
		struct vdec_common_vsi *vdec_com_vsi = (struct vdec_common_vsi *)inst->ctx->dev->com_vsi;
		struct vdec_vp_mode_buf_info *vp_mode_info =
			(struct vdec_vp_mode_buf_info *)&vdec_com_vsi->vp_mode_info;

		memset(vp_mode_info->alloc_src_buf, 0, sizeof(vp_mode_info->alloc_src_buf));
		memset(vp_mode_info->src_buf, 0, sizeof(vp_mode_info->src_buf));
		mtk_vcodec_vp_mode_buf_unprepare(inst->ctx->dev);
	}

	mutex_lock(inst->vcu.ctx_ipi_lock);
	list_for_each_safe(p, q, &inst->vcu.bufs) {
		tmp = list_entry(p, struct vcp_dec_mem_list, list);

		dev = get_dev_by_mem_type(inst, &tmp->mem);
		mtk_vcodec_free_mem(&tmp->mem, dev, tmp->attach, tmp->sgt);
		mtk_v4l2_debug(0, "[%d] leak free va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			inst->ctx->id, tmp->mem.va, tmp->mem.pa,
			tmp->mem.iova, tmp->mem.len,  tmp->mem.type);

		list_del(p);
		kfree(tmp);
	}
	mutex_unlock(inst->vcu.ctx_ipi_lock);
	mutex_destroy(inst->vcu.ctx_ipi_lock);
	kfree(inst->vcu.ctx_ipi_lock);
	kfree(inst);
}

int vdec_vcp_reset(struct vdec_inst *inst, enum vdec_reset_type drain_type)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug(inst, "drain_type %d +", drain_type);
	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_RESET;
	msg.ctx_id = inst->ctx->id;
	msg.vcu_inst_addr = inst->vcu.inst_addr;
	msg.drain_type = drain_type;

	vcodec_trace_begin("%s(drain_type %d)", __func__, drain_type);
	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
	vcodec_trace_end();
	mtk_vcodec_debug(inst, "- ret=%d", err);

	inst->ctx->input_driven = inst->vsi->input_driven;
	inst->ctx->output_async = inst->vsi->output_async;
	inst->ctx->low_pw_mode  = inst->vsi->low_pw_mode > 0;

	return err;
}

static int vdec_vcp_decode(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
	struct vdec_fb *fb, unsigned int *src_chg)
{
	int ret = 0;

	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	struct vdec_vcu_inst *vcu = &inst->vcu;
	struct vdec_ap_ipi_dec_start msg;
	uint64_t vdec_fb_va;
	uint64_t fb_dma[VIDEO_MAX_PLANES] = { 0 };
	uint32_t num_planes;
	unsigned int i = 0;
	unsigned int bs_fourcc = inst->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	unsigned int fm_fourcc = inst->ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
	unsigned int *errormap_info = &inst->ctx->errormap_info[0];

	num_planes = fb ? inst->vsi->dec.fb_num_planes : 0U;

	for (i = 0; i < num_planes; i++)
		fb_dma[i] = (u64)fb->fb_base[i].dma_addr;

	vdec_fb_va = (u64)(uintptr_t)fb;

	mtk_vcodec_debug(inst, "+ [%d] FB y_dma=%llx c_dma=%llx va=%p num_planes %d",
		inst->num_nalu, fb_dma[0], fb_dma[1], fb, num_planes);

	/* bs == NULL means reset decoder */
	if (bs == NULL) {
		if (fb == NULL)
			return vdec_vcp_reset(inst, VDEC_FLUSH); // flush (0)
		else if (fb->status == 0)
			return vdec_vcp_reset(inst, VDEC_DRAIN); // drain (1)
		else
			return vdec_vcp_reset(inst, VDEC_DRAIN_EOS); // drain & return EOS frame (2)
	}

	mtk_vcodec_debug(inst, "+ BS dma=0x%llx dmabuf=%p format=%s",
		(uint64_t)bs->dma_addr, bs->dmabuf, FOURCC_STR(bs_fourcc));

	inst->vsi->dec.vdec_bs_va = (u64)(uintptr_t)bs;
	inst->vsi->dec.bs_dma = (uint64_t)bs->dma_addr;
	inst->vsi->dec.bs_non_acp_dma = bs->non_acp_iova;

	for (i = 0; i < num_planes; i++)
		inst->vsi->dec.fb_dma[i] = fb_dma[i];

	if (inst->vsi->input_driven == NON_INPUT_DRIVEN) {
		inst->vsi->dec.vdec_fb_va = (u64)(uintptr_t)NULL;
		inst->vsi->dec.index = 0xFF;
	}
	if (fb != NULL) {
		inst->vsi->dec.vdec_fb_va = vdec_fb_va;
		inst->vsi->dec.index = fb->index;
		if (fb->dma_general_buf != 0) {
			inst->vsi->general_buf_fd = fb->general_buf_fd;
			inst->vsi->general_buf_size = fb->dma_general_buf->size;
			inst->vsi->general_buf_dma = fb->dma_general_addr;
			mtk_vcodec_debug(inst, "dma_general_buf dma_buf=%p fd=%d dma=%llx size=%lu",
			    fb->dma_general_buf, inst->vsi->general_buf_fd,
			    inst->vsi->general_buf_dma,
			    fb->dma_general_buf->size);
		} else {
			fb->general_buf_fd = -1;
			inst->vsi->general_buf_fd = -1;
			inst->vsi->general_buf_size = 0;
			mtk_vcodec_debug(inst, "no general buf dmabuf");
		}
	}

	inst->vsi->dec.timestamp = inst->ctx->timestamp;

	mtk_vcodec_debug(inst, "+ FB y_fd=%llx c_fd=%llx BS fd=%llx format=%s",
		inst->vsi->dec.fb_fd[0], inst->vsi->dec.fb_fd[1],
		inst->vsi->dec.bs_fd, FOURCC_STR(fm_fourcc));


	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_START;
	msg.ctx_id = inst->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;
	msg.data[0] = (unsigned int)bs->size;
	msg.data[1] = (unsigned int)bs->length;
	msg.data[2] = (unsigned int)bs->flags;
	vcodec_trace_begin("%s(ipi)", __func__);
	ret = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
	vcodec_trace_end();

	*src_chg = inst->vsi->dec.vdec_changed_info;
	*(errormap_info + bs->index % VB2_MAX_FRAME) =
		inst->vsi->dec.error_map;

	if (inst->ctx->dev->vdec_hw_ipm == VCODEC_IPM_V2)
		check_error_code(inst, MTK_VDEC_LAT);
	if (!inst->vsi->output_async)
		check_error_code(inst, MTK_VDEC_CORE);

	if ((*src_chg & VDEC_NEED_SEQ_HEADER) != 0U)
		mtk_vcodec_debug(inst, "- need first seq header -");
	else if ((*src_chg & VDEC_RES_CHANGE) != 0U)
		mtk_vcodec_debug(inst, "- resolution changed -");
	else if ((*src_chg & VDEC_HW_NOT_SUPPORT) != 0U)
		mtk_vcodec_err(inst, "- unsupported -");
	/*ack timeout means vpud has crashed*/
	if (ret != IPI_ACTION_DONE) {
		mtk_vcodec_err(inst, "- IPI msg ack fail %d -", ret);
		*src_chg = *src_chg | VDEC_HW_NOT_SUPPORT;
	}

	if (ret < 0 || ((*src_chg & VDEC_HW_NOT_SUPPORT) != 0U)
		|| ((*src_chg & VDEC_NEED_SEQ_HEADER) != 0U))
		goto err_free_fb_out;

	inst->ctx->input_driven = inst->vsi->input_driven;
	inst->ctx->output_async = inst->vsi->output_async;
	inst->ctx->low_pw_mode  = inst->vsi->low_pw_mode > 0;
	inst->num_nalu++;
	return ret;

err_free_fb_out:
	put_fb_to_free(inst, fb);
	mtk_vcodec_debug(inst, "\n - NALU[%d] err=%d -\n", inst->num_nalu, ret);

	return ret;
}

static int vdec_vcp_set_pwr_ctrl(struct vdec_inst *inst, struct mtk_smi_pwr_ctrl_info *ctrl_info)
{
	struct vdec_ap_ipi_pwr_ctrl msg = {0};

	if (ctrl_info->type == MTK_SMI_GET_IF_IN_USE && !has_valid_vcp_inst(inst->ctx->dev)) {
		ctrl_info->ret = 0;
		return 0;
	}

	msg.msg_id = AP_IPIMSG_DEC_PWR_CTRL;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	msg.ap_data_addr = (uintptr_t)ctrl_info;
	msg.info.type = ctrl_info->type;
	msg.info.hw_id = ctrl_info->hw_id;
	vdec_vcp_set_vcu(&inst->vcu);

	return vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
}

static void set_vdec_vcp_data(struct vdec_inst *inst, enum vcp_reserve_mem_id_t id, void *string)
{
	//struct vdec_ap_ipi_set_param msg;
	void *string_va = (void *)(__u64)vcp_get_reserve_mem_virt_ex(id);
	void *string_pa = (void *)(__u64)vcp_get_reserve_mem_phys_ex(id);
	__u64 mem_size = (__u64)vcp_get_reserve_mem_size_ex(id);
	int string_len = strlen((char *)string);

	mtk_vcodec_debug(inst, "mem_size 0x%llx, string_va 0x%lx, string_pa 0x%lx",
		mem_size, (unsigned long)string_va, (unsigned long)string_pa);
	mtk_vcodec_debug(inst, "string: %s", (char *)string);
	mtk_vcodec_debug(inst, "string_len:%d", string_len);

	if (string_len <= (mem_size-1))
		memcpy(string_va, (char *)string, string_len + 1);
}


static int set_frame_buffer(struct vdec_inst *inst, void *fb)
{
	int err = 0;
	struct vdec_ap_ipi_set_param msg;
	struct mtk_video_dec_buf *dst_buf_info = fb;
	struct vdec_ipi_fb ipi_fb;
	struct vdec_fb *pfb = NULL;
	bool dst_not_get = true;

	mtk_vcodec_debug(inst, "+ id=%X", AP_IPIMSG_DEC_FRAME_BUFFER);

	memset(&msg, 0, sizeof(msg));
	memset(&ipi_fb, 0, sizeof(ipi_fb));
	msg.msg_id = AP_IPIMSG_DEC_FRAME_BUFFER;
	msg.ctx_id = inst->ctx->id;
	msg.id = 0;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	do {
		if (fb == NULL) {
			mtk_vcodec_debug(inst, "send flush");
		} else {
			pfb = mtk_vcodec_get_fb(inst->ctx);
			if (pfb == &dst_buf_info->frame_buffer)
				dst_not_get = false;
			ipi_fb.vdec_fb_va = (u64)pfb;
		}

		if (pfb != NULL) {
			ipi_fb.reserved = pfb->index;
			ipi_fb.y_fb_dma = (u64)pfb->fb_base[0].dma_addr;
			if (pfb->num_planes > 1)
				ipi_fb.c_fb_dma = (u64)pfb->fb_base[1].dma_addr;

			if (pfb->dma_general_buf != 0) {
				ipi_fb.dma_general_addr = pfb->dma_general_addr;
				ipi_fb.general_size = pfb->dma_general_buf->size;
				mtk_vcodec_debug(inst, "FB id=%d dma_addr (%llx,%llx) dma_general_buf %p size %lu dma %pad",
					pfb->index, ipi_fb.y_fb_dma, ipi_fb.c_fb_dma,
					pfb->dma_general_buf, pfb->dma_general_buf->size,
					&pfb->dma_general_addr);
			} else {
				ipi_fb.dma_general_addr = -1;
				mtk_vcodec_debug(inst, "FB id=%d dma_addr (%llx,%llx) dma_general_buf %p no general buf dmabuf",
					pfb->index, ipi_fb.y_fb_dma, ipi_fb.c_fb_dma,
					pfb->dma_general_buf);
			}
		}

		if (pfb != NULL || fb == NULL) {
			memcpy(msg.data, &ipi_fb, sizeof(struct vdec_ipi_fb));
			err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
			if (err < 0)
				break;
		}
	} while (pfb != NULL);

	if (fb != NULL && dst_not_get) {
		mtk_vcodec_debug(inst, "warning: dst_buf_info->frame_buffer id=%d %p %llx not get",
			dst_buf_info->frame_buffer.index,
			&dst_buf_info->frame_buffer,
			(u64)&dst_buf_info->frame_buffer);
	}
	if (err < 0)
		mtk_vcodec_err(inst, "- id=%X ret=%d", AP_IPIMSG_DEC_FRAME_BUFFER, err);
	else
		mtk_vcodec_debug(inst, "- id=%X ret=%d", AP_IPIMSG_DEC_FRAME_BUFFER, err);

	return err;
}

static int vdec_vcp_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{
	struct vdec_ap_ipi_set_param msg;
	int ret = 0;
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	uint64_t size;
	unsigned long *param_ptr = (unsigned long *)in;

	if (inst == NULL)
		return -EINVAL;

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_SET_PARAM;
	msg.ctx_id = inst->ctx->id;
	msg.id = type;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	switch (type) {
	case SET_PARAM_FRAME_BUFFER:
		ret = set_frame_buffer(inst, in);
		break;
	case SET_PARAM_SET_FIXED_MAX_OUTPUT_BUFFER:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec_params.fixed_max_frame_size_width = (__u32)(*param_ptr);
		inst->vsi->dec_params.fixed_max_frame_size_height = (__u32)(*(param_ptr + 1));
		inst->vsi->dec_params.fixed_max_frame_buffer_mode = (__u32)(*(param_ptr + 2));
		inst->vsi->dec_params.dec_param_change |= MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE;
		break;
	case SET_PARAM_DECODE_MODE:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec_params.decode_mode = (__u32)(*param_ptr);
		inst->vsi->dec_params.dec_param_change |= MTK_DEC_PARAM_DECODE_MODE;
		break;
	case SET_PARAM_WAIT_KEY_FRAME:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec_params.wait_key_frame = (__u32)(*param_ptr);
		inst->vsi->dec_params.dec_param_change |= MTK_DEC_PARAM_WAIT_KEY_FRAME;
		break;
	case SET_PARAM_DECODE_ERROR_HANDLE_MODE:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec_params.decode_error_handle_mode = (__u32)(*param_ptr);
		inst->vsi->dec_params.dec_param_change |= MTK_DEC_PARAM_DECODE_ERROR_HANDLE_MODE;
		break;
	case SET_PARAM_OPERATING_RATE:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec_params.operating_rate = (__u32)(*param_ptr);
		inst->vsi->dec_params.dec_param_change |= MTK_DEC_PARAM_OPERATING_RATE;
		break;
	case SET_PARAM_DEC_PARAMS:
		if (inst->vsi == NULL)
			return -EINVAL;
		mtk_v4l2_debug(2, "[%d] param change 0x%x decode mode %d max %d %d wait key %d op-rate %d error mode %d",
			inst->ctx->id, inst->vsi->dec_params.dec_param_change,
			inst->vsi->dec_params.decode_mode,
			inst->vsi->dec_params.fixed_max_frame_size_width,
			inst->vsi->dec_params.fixed_max_frame_size_height,
			inst->vsi->dec_params.wait_key_frame,
			inst->vsi->dec_params.operating_rate,
			inst->vsi->dec_params.decode_error_handle_mode);
		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
		break;
	case SET_PARAM_COMPRESSED_MODE:
		msg.data[0] = (__u32)(*param_ptr);
		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
		break;
	case SET_PARAM_CRC_PATH:
		if (inst->vsi == NULL)
			return -EINVAL;
		size = strlen((char *) *(uintptr_t *)in);
		memcpy(inst->vsi->crc_path, (void *) *(uintptr_t *)in, size);
		break;
	case SET_PARAM_GOLDEN_PATH:
		if (inst->vsi == NULL)
			return -EINVAL;
		size = strlen((char *) *(uintptr_t *)in);
		memcpy(inst->vsi->golden_path, (void *) *(uintptr_t *)in, size);
		break;
	case SET_PARAM_FB_NUM_PLANES:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec.fb_num_planes = *(unsigned int *)in;
		break;
	case SET_PARAM_VDEC_PROPERTY:
		set_vdec_vcp_data(inst, VDEC_SET_PROP_MEM_ID, in);
		break;
	case SET_PARAM_VDEC_VCP_LOG_INFO:
		set_vdec_vcp_data(inst, VDEC_VCP_LOG_INFO_ID, in);
		break;
	case SET_PARAM_MMDVFS:
		if (inst->vsi == NULL)
			return -EINVAL;
		msg.data[0] = (__u32)(*param_ptr);
		ret = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, true);
		break;
	case SET_PARAM_VDEC_IN_GROUP:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->in_group = (bool)in;
		break;
	case SET_PARAM_PER_FRAME_SUBSAMPLE_MODE:
	case SET_PARAM_VPEEK_MODE:
	case SET_PARAM_VDEC_PLUS_DROP_RATIO:
	case SET_PARAM_CONTAINER_FRAMERATE:
	case SET_PARAM_DISABLE_DEBLOCK:
	case SET_PARAM_VDEC_LINECOUNT_THRESHOLD:
		msg.data[0] = (__u32)(*param_ptr);
		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
		break;
	case SET_PARAM_ACQUIRE_RESOURCE: {
		struct v4l2_vdec_resource_parameter *res_param = in;
		struct v4l2_fract *framerate = &res_param->frame_rate;

		msg.data[0] = res_param->width;
		msg.data[1] = res_param->height;
		if (framerate->denominator) {
			msg.data[2] = (framerate->numerator + framerate->denominator / 2) /
				    framerate->denominator;
		}
		msg.data[3] = res_param->priority;
		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
		break;
	}
	case SET_PARAM_LOW_LATENCY: {
		struct v4l2_vdec_low_latency_parameter *low_latency_param = in;

		msg.data[0] = low_latency_param->slice_count;
		msg.data[1] = low_latency_param->racing_display;

		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
		break;
	}
	case SET_PARAM_VDEC_PWR_CTRL:
		ret = vdec_vcp_set_pwr_ctrl(inst, (struct mtk_smi_pwr_ctrl_info *)in);
		break;
	default:
		mtk_vcodec_err(inst, "invalid set parameter type=%d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int vdec_vcp_query_cap(struct vdec_inst *inst, int query_cap_id, uintptr_t ap_data_addr)
{
	struct vdec_ap_ipi_query_cap msg;

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_QUERY_CAP;
	msg.id = query_cap_id;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	msg.ap_data_addr = ap_data_addr;
	vdec_vcp_set_vcu(&inst->vcu);

	return vdec_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
}

// TODO: VSI touch common code shared with vcu
static void get_supported_format(struct vdec_inst *inst,
	struct mtk_video_fmt *video_fmt)
{
	unsigned int i = 0;

	vdec_vcp_query_cap(inst, GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS, (uintptr_t)video_fmt);

	for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
		if (video_fmt[i].fourcc != 0) {
			mtk_vcodec_debug(inst, "video_formats[%d] fourcc %s(0x%x) type %d num_planes %d\n",
				i, FOURCC_STR(video_fmt[i].fourcc), video_fmt[i].fourcc, video_fmt[i].type,
				video_fmt[i].num_planes);
		}
	}
}

static void get_supported_frame_intervals(struct vdec_inst *inst,
	struct mtk_video_frame_frameintervals *f_ints)
{
	vdec_vcp_query_cap(inst, GET_PARAM_VDEC_CAP_FRAMEINTERVALS, (uintptr_t)f_ints);

	mtk_vcodec_debug(inst, "codec fourcc %s(0x%x) w %d h %d max %d/%d min %d/%d step %d/%d\n",
		FOURCC_STR(f_ints->fourcc), f_ints->fourcc, f_ints->width, f_ints->height,
		f_ints->stepwise.max.numerator, f_ints->stepwise.max.denominator,
		f_ints->stepwise.min.numerator, f_ints->stepwise.min.denominator,
		f_ints->stepwise.step.numerator, f_ints->stepwise.step.denominator);
}

static void get_frame_sizes(struct vdec_inst *inst,
	struct mtk_codec_framesizes *codec_framesizes)
{
	unsigned int i = 0;

	vdec_vcp_query_cap(inst, GET_PARAM_VDEC_CAP_FRAME_SIZES, (uintptr_t)codec_framesizes);

	for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
		if (codec_framesizes[i].fourcc != 0) {
			mtk_vcodec_debug(inst, "codec_fs[%d] fourcc %s(0x%x) s %d %d %d %d %d %d P %d L %d\n",
				i, FOURCC_STR(codec_framesizes[i].fourcc), codec_framesizes[i].fourcc,
				codec_framesizes[i].stepwise.min_width,
				codec_framesizes[i].stepwise.max_width,
				codec_framesizes[i].stepwise.step_width,
				codec_framesizes[i].stepwise.min_height,
				codec_framesizes[i].stepwise.max_height,
				codec_framesizes[i].stepwise.step_height,
				codec_framesizes[i].profile,
				codec_framesizes[i].level);
		}
	}

}

static bool check_buf_va(uintptr_t buf_va, uintptr_t *buf_list)
{
	int idx;

	for (idx = 1; idx <= VB2_MAX_FRAME; idx++)
		if (buf_va == buf_list[idx])
			return true;

	return false;
}

static void vdec_get_bs(struct vdec_inst *inst,
	struct ring_bs_list *list,
	struct mtk_vcodec_mem **out_bs)
{
	unsigned long vdec_bs_va;
	struct mtk_vcodec_mem *bs;

get_bs:
	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[BS] there is no bs");
		*out_bs = NULL;
		return;
	}

	vdec_bs_va = (unsigned long)list->vdec_bs_va_list[list->read_idx];
	bs = (struct mtk_vcodec_mem *)vdec_bs_va;
	if (bs == NULL || !check_buf_va((uintptr_t)vdec_bs_va, inst->ctx->bs_list)) {
		mtk_vcodec_err(inst, "free bs list read_idx %d vdec_bs_va 0x%lx invalid !",
			list->read_idx, vdec_bs_va);
		list->read_idx = (list->read_idx == DEC_MAX_BS_NUM - 1U) ? 0U : list->read_idx + 1U;
		list->count--;
		if (list->count > 0)
			goto get_bs;
		else {
			*out_bs = NULL;
			return;
		}
	}

	*out_bs = bs;
	mtk_vcodec_debug(inst, "[BS] get free bs %lx", vdec_bs_va);

	list->read_idx = (list->read_idx == DEC_MAX_BS_NUM - 1) ? 0 : list->read_idx + 1;
	list->count--;
}

static void vdec_get_fb(struct vdec_inst *inst,
	struct ring_fb_list *list,
	bool disp_list, struct vdec_fb **out_fb)
{
	unsigned long vdec_fb_va;
	struct vdec_fb *fb;

get_fb:
	if (list->count >= DEC_MAX_FB_NUM) {
		mtk_vcodec_err(inst, "list count %d invalid ! (write_idx %d, read_idx %d)",
			list->count, list->write_idx, list->read_idx);
		if (list->write_idx >= DEC_MAX_FB_NUM || list->read_idx >= DEC_MAX_FB_NUM)
			list->write_idx = list->read_idx = 0;
		if (list->write_idx >= list->read_idx)
			list->count = list->write_idx - list->read_idx;
		else
			list->count = list->write_idx + DEC_MAX_FB_NUM - list->read_idx;
	}
	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[FB] there is no %s fb", disp_list ? "disp" : "free");
		*out_fb = NULL;
		return;
	}

	vdec_fb_va = (unsigned long)list->fb_list[list->read_idx].vdec_fb_va;
	fb = (struct vdec_fb *)vdec_fb_va;
	if (fb == NULL || !check_buf_va((uintptr_t)vdec_fb_va, inst->ctx->fb_list)) {
		mtk_vcodec_err(inst, "%s fb list read_idx %d write_idx %d count %d vdec_fb_va 0x%lx invalid !",
			disp_list ? "disp" : "free", list->read_idx, list->write_idx, list->count, vdec_fb_va);
		list->read_idx = (list->read_idx == DEC_MAX_FB_NUM - 1U) ? 0U : list->read_idx + 1U;
		list->count--;
		goto get_fb;
	}
	fb->timestamp = list->fb_list[list->read_idx].timestamp;
	fb->field = list->fb_list[list->read_idx].field;
	fb->frame_type = list->fb_list[list->read_idx].frame_type;

	if (disp_list) {
		fb->status |= FB_ST_DISPLAY;
		if (list->fb_list[list->read_idx].flags & VDEC_FB_NO_GENERATED)
			fb->status |= FB_ST_NO_GENERATED;
		if (list->fb_list[list->read_idx].flags & VDEC_FB_CROP_CHANGED)
			fb->status |= FB_ST_CROP_CHANGED;
	} else {
		fb->status |= FB_ST_FREE;
		if (list->fb_list[list->read_idx].flags & VDEC_FB_EOS)
			fb->status |= FB_ST_EOS;
	}

	*out_fb = fb;
	mtk_vcodec_debug(inst, "[FB] get %s fb (read_idx %d write_idx %d count %d) st=0x%x id=%d type 0x%x ts=%llu %lx gbuf fd %d dma %p",
		disp_list ? "disp" : "free", list->read_idx, list->write_idx, list->count,
		fb->status, fb->index, fb->frame_type, fb->timestamp, vdec_fb_va,
		fb->general_buf_fd, fb->dma_general_buf);

	list->read_idx = (list->read_idx == DEC_MAX_FB_NUM - 1U) ? 0U : list->read_idx + 1U;
	list->count--;
}

static void get_color_desc(struct vdec_inst *inst, struct mtk_color_desc *color_desc)
{
	inst->vcu.ctx = inst->ctx;
	memcpy(color_desc, &inst->vsi->color_desc, sizeof(*color_desc));
}

static void get_aspect_ratio(struct vdec_inst *inst, unsigned int *aspect_ratio)
{
	if (inst->vsi == NULL)
		return;

	inst->vcu.ctx = inst->ctx;
	*aspect_ratio = inst->vsi->aspect_ratio;
}

static void get_supported_fix_buffers(struct vdec_inst *inst, unsigned int *supported)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*supported = inst->vsi->fix_buffers;
}

static void get_interlacing(struct vdec_inst *inst, unsigned int *interlacing)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*interlacing = inst->vsi->interlacing;
}

static void get_input_driven(struct vdec_inst *inst, unsigned int *input_driven)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*input_driven = inst->vsi->input_driven;
}

static void get_output_async(struct vdec_inst *inst, bool *output_async)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*output_async = inst->vsi->output_async;
}

static void get_low_pw_mode(struct vdec_inst *inst, unsigned int *low_pw_mode)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*low_pw_mode = inst->vsi->low_pw_mode;
}

static void get_frame_interval(struct vdec_inst *inst, struct v4l2_fract *time_per_frame)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		memcpy(time_per_frame, &inst->vsi->time_per_frame, sizeof(struct v4l2_fract));
}

static void get_res_info(struct vdec_inst *inst,
			 struct vdec_resource_info *res_info)
{
	if (inst->vsi != NULL)
		memcpy(res_info, &inst->vsi->res_info, sizeof(struct vdec_resource_info));
}

static void get_bandwidth_info(struct vdec_inst *inst,
			  struct vdec_bandwidth_info *bandwidth_info)
{
	if (inst->vsi != NULL)
		memcpy(bandwidth_info, &inst->vsi->bandwidth_info,
			 sizeof(struct vdec_bandwidth_info));
}

static void get_max_buf_sizes(struct vdec_inst *inst,
	 struct vdec_max_buf_info *max_buf_info)
{
	vdec_vcp_query_cap(inst, GET_PARAM_VDEC_CAP_MAX_BUF_INFO, (uintptr_t)max_buf_info);
}

static void get_trick_mode(struct vdec_inst *inst,
			   unsigned int *trick_mode)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*trick_mode = inst->vsi->trick_mode;
}

static int vdec_vcp_get_param(unsigned long h_vdec,
	enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;

	if (inst == NULL)
		return -EINVAL;

	switch (type) {
	case GET_PARAM_FREE_BITSTREAM_BUFFER:
		if (inst->vsi == NULL)
			return -EINVAL;
		vdec_get_bs(inst, &inst->vsi->list_free_bs, out);
		break;

	case GET_PARAM_DISP_FRAME_BUFFER:
	{
		struct vdec_fb *pfb;

		if (inst->vsi == NULL)
			return -EINVAL;
		if (inst->put_frame_async) {
			mutex_lock(&inst->list_disp_fb_lock);
			vdec_get_fb(inst, &inst->list_disp_fb, true, out);
			mutex_unlock(&inst->list_disp_fb_lock);
		} else
			vdec_get_fb(inst, &inst->vsi->list_disp, true, out);

		pfb = *((struct vdec_fb **)out);
		if (pfb != NULL) {
			if (pfb->general_buf_fd >= 0) {
				mtk_vcodec_debug(inst, "free pfb->general_buf_fd:%d pfb->dma_general_buf %p\n",
					pfb->general_buf_fd,
					pfb->dma_general_buf);
				pfb->general_buf_fd = -1;
			}
		}
		break;
	}

	case GET_PARAM_FREE_FRAME_BUFFER:
	{
		struct vdec_fb *pfb;

		if (inst->vsi == NULL)
			return -EINVAL;
		if (inst->put_frame_async) {
			mutex_lock(&inst->list_free_fb_lock);
			vdec_get_fb(inst, &inst->list_free_fb, false, out);
			mutex_unlock(&inst->list_free_fb_lock);
		} else
			vdec_get_fb(inst, &inst->vsi->list_free, false, out);

		pfb = *((struct vdec_fb **)out);
		if (pfb != NULL) {
			if (pfb->general_buf_fd >= 0) {
				mtk_vcodec_debug(inst, "free pfb->general_buf_fd:%d pfb->dma_general_buf %p\n",
					pfb->general_buf_fd,
					pfb->dma_general_buf);
				pfb->general_buf_fd = -1;
			}
		}
		break;
	}

	case GET_PARAM_PIC_INFO:
		get_pic_info(inst, out);
		break;

	case GET_PARAM_DPB_SIZE:
		get_dpb_size(inst, out);
		break;

	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;

	case GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS:
		get_supported_format(inst, out);
		break;

	case GET_PARAM_VDEC_CAP_FRAME_SIZES:
		get_frame_sizes(inst, out);
		break;

	case GET_PARAM_COLOR_DESC:
		if (inst->vsi == NULL)
			return -EINVAL;
		get_color_desc(inst, out);
		break;

	case GET_PARAM_ASPECT_RATIO:
		get_aspect_ratio(inst, out);
		break;

	case GET_PARAM_PLATFORM_SUPPORTED_FIX_BUFFERS:
		get_supported_fix_buffers(inst, out);
		break;

	case GET_PARAM_INTERLACING:
		get_interlacing(inst, out);
		break;

	case GET_PARAM_FRAME_INTERVAL:
		get_frame_interval(inst, out);
		break;

	case GET_PARAM_VDEC_CAP_FRAMEINTERVALS:
		get_supported_frame_intervals(inst, out);
		break;

	case GET_PARAM_INPUT_DRIVEN:
		get_input_driven(inst, out);
		break;

	case GET_PARAM_OUTPUT_ASYNC:
		get_output_async(inst, out);
		break;

	case GET_PARAM_LOW_POWER_MODE:
		get_low_pw_mode(inst, out);
		break;

	case GET_PARAM_RES_INFO:
		get_res_info(inst, out);
		break;
	case GET_PARAM_BANDWIDTH_INFO:
		get_bandwidth_info(inst, out);
		break;
	case GET_PARAM_VDEC_CAP_MAX_BUF_INFO:
		get_max_buf_sizes(inst, out);
		break;
	case GET_PARAM_TRICK_MODE:
		get_trick_mode(inst, out);
		break;
	default:
		mtk_vcodec_err(inst, "invalid get parameter type=%d", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct vdec_common_if vdec_vcp_if = {
	vdec_vcp_init,
	vdec_vcp_decode,
	vdec_vcp_get_param,
	vdec_vcp_set_param,
	vdec_vcp_deinit,
};

struct vdec_common_if *get_dec_vcp_if(void)
{
	return &vdec_vcp_if;
}
