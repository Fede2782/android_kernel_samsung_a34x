// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "mbraink_auto.h"
#include "mbraink_auto_ioctl_struct_def.h"
#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
#include "mbraink_hypervisor_virtio.h"
#include "mbraink_hypervisor.h"
#endif

static long handle_hyp_vcpu_switch_info(const void *auto_ioctl_data, void *mbraink_auto_data)
{
	struct mbraink_vcpu_buf *mbraink_vcpu_exec_trans_buf =
					(struct mbraink_vcpu_buf *)(mbraink_auto_data);
	long ret = 0;

	if (copy_from_user(mbraink_vcpu_exec_trans_buf, (struct mbraink_vcpu_buf *)auto_ioctl_data,
			sizeof(struct mbraink_vcpu_buf))) {
		pr_notice("copy mbraink_vcpu_buf data from user Err!\n");
		return -EPERM;
	}

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	if (mbraink_vcpu_exec_trans_buf != NULL) {
		switch (mbraink_vcpu_exec_trans_buf->trans_type) {
		case 0:
		{
			ret = mbraink_hyp_set_vcpu_switch_info(0);
			break;
		}
		case 1:
		{
			ret = mbraink_hyp_set_vcpu_switch_info(1);
			break;
		}
		case 2:
		{
			if (mbraink_vcpu_exec_trans_buf->length == 0) {
				pr_notice("length is 0. no need do anything\n");
			} else {
				void *vcpu_switch_info_buffer =
				 vmalloc(mbraink_vcpu_exec_trans_buf->length
				 * sizeof(struct vcpu_exec_rec));

				if (vcpu_switch_info_buffer == NULL)
					return -ENOMEM;
				ret = mbraink_hyp_get_vcpu_switch_info(mbraink_vcpu_exec_trans_buf,
						vcpu_switch_info_buffer);

				if (copy_to_user((struct mbraink_vcpu_buf *)auto_ioctl_data,
						mbraink_vcpu_exec_trans_buf,
						sizeof(struct mbraink_vcpu_buf))) {
					pr_info("Copy mbraink_vcpu_buf to user error!\n");
					vfree(vcpu_switch_info_buffer);
					return -EPERM;
				}
				if (copy_to_user(mbraink_vcpu_exec_trans_buf->vcpu_data,
						vcpu_switch_info_buffer,
						mbraink_vcpu_exec_trans_buf->length *
						sizeof(struct vcpu_exec_rec))) {
					pr_info("Copy vcpu_data to user error!\n");
					vfree(vcpu_switch_info_buffer);
					return -EPERM;
				}

				vfree(vcpu_switch_info_buffer);
			}
			break;
		}
		default:
		{
			pr_info("unknown vcpu type %d\n", mbraink_vcpu_exec_trans_buf->trans_type);
			ret = -1;
		}
		}
	}
#else
	pr_notice("%s: grt hyp vcpu switch info not supported!\n", __func__);
	ret = -1;
#endif

	return ret;
}

static long handle_hyp_wfe_exit_count_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_wfe_exit_buf *hyp_wfe_exit_buf =
		(struct mbraink_hyp_wfe_exit_buf *)(mbraink_data);
	long ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	ret = mbraink_hyp_getWfeExitCountInfo(hyp_wfe_exit_buf);
	if (copy_to_user((struct mbraink_hyp_wfe_exit_buf *) arg,
			hyp_wfe_exit_buf,
			sizeof(struct mbraink_hyp_wfe_exit_buf))) {
		pr_notice("Copy hyp_wfe_exit_buf to UserSpace error!\n");
		return -EPERM;
	}
#else
	pr_notice("%s: grt hyp wfe info not supported!\n", __func__);
	ret = -1;
#endif

	return ret;
}

static long handle_hyp_ipi_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_cpu_ipi_data_ringbuffer *hyp_cpu_ipi_data_ringbuffer =
		(struct mbraink_hyp_cpu_ipi_data_ringbuffer *)(mbraink_data);
	long ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	ret = mbraink_hyp_getIpiInfo(hyp_cpu_ipi_data_ringbuffer);
	if (copy_to_user((struct mbraink_hyp_cpu_ipi_data_ringbuffer *) arg,
			hyp_cpu_ipi_data_ringbuffer,
			sizeof(struct mbraink_hyp_cpu_ipi_data_ringbuffer))) {
		pr_notice("Copy hyp_cpu_ipi_data_ringbuffer to UserSpace error!\n");
		return -EPERM;
	}
#else
	pr_notice("%s: grt hyp ipi info not supported!\n", __func__);
	ret = -1;
#endif

	return ret;
}

static long handle_hyp_virq_inject_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_comcat_trace_buf *hyp_comcat_trace_buf =
		(struct mbraink_hyp_comcat_trace_buf *)(mbraink_data);
	long ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	ret = mbraink_hyp_getvIrqInjectLatency(hyp_comcat_trace_buf);
	if (copy_to_user((struct mbraink_hyp_comcat_trace_buf *) arg,
			hyp_comcat_trace_buf,
			sizeof(struct mbraink_hyp_comcat_trace_buf))) {
		pr_notice("Copy hyp_comcat_trace_buf to UserSpace error!\n");
		return -EPERM;
	}
#else
	pr_notice("%s: grt hyp virq info not supported!\n", __func__);
	ret = -1;
#endif

	return ret;
}

static long handle_hyp_vcpu_sched_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_vCpu_sched_delay_buf *hyp_vCpu_sched_delay_buf =
		(struct mbraink_hyp_vCpu_sched_delay_buf *)(mbraink_data);
	long ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	ret = mbraink_hyp_getvCpuSchedInfo(hyp_vCpu_sched_delay_buf);
	if (copy_to_user((struct mbraink_hyp_vCpu_sched_delay_buf *) arg,
			hyp_vCpu_sched_delay_buf,
			sizeof(struct mbraink_hyp_vCpu_sched_delay_buf))) {
		pr_notice("Copy hyp_vCpu_sched_delay_buf to UserSpace error!\n");
		return -EPERM;
	}
#else
	pr_notice("%s: grt hyp vcpu sched info not supported!\n", __func__);
	ret = -1;
#endif

	return ret;
}

static long handle_h2c_notify(const void *auto_ioctl_data, void *mbraink_auto_data)
{
	struct mbraink_host_notify_client *h2c_msg_info =
					(struct mbraink_host_notify_client *)(mbraink_auto_data);
	long ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	if (copy_from_user(h2c_msg_info, (struct mbraink_host_notify_client *)auto_ioctl_data,
			sizeof(struct mbraink_host_notify_client))) {
		pr_notice("copy mbraink_host_notify_client data from user Err!\n");
		return -EPERM;
	}

	if (h2c_msg_info != NULL)
		ret = h2c_send_msg(h2c_msg_info->cmdType, h2c_msg_info->cmdData);
#else
	pr_notice("%s: grt hyp send msg to client not supported! cmdType: %d\n",
			 __func__, h2c_msg_info->cmdType);
	ret = -1;
#endif
	return ret;
}

long mbraink_auto_ioctl(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	void *mbraink_auto_data = NULL;
	struct mbraink_auto_ioctl_info *auto_ioctl_buf =
		(struct mbraink_auto_ioctl_info *)(mbraink_data);

	if (copy_from_user(auto_ioctl_buf, (struct mbraink_auto_ioctl_info *) arg,
			sizeof(struct mbraink_auto_ioctl_info))) {
		pr_notice("copy mbraink_auto_ioctl_info data from user Err!\n");
		return -EPERM;
	}

	switch (auto_ioctl_buf->auto_ioctl_type) {
	case HYP_VCPU_SWITCH_INFO:
	{
		mbraink_auto_data = kmalloc(sizeof(struct mbraink_vcpu_buf), GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_hyp_vcpu_switch_info(auto_ioctl_buf->auto_ioctl_data,
										 mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	case HYP_WFE_EXIT_COUNT:
	{
		mbraink_auto_data = kzalloc(sizeof(struct mbraink_hyp_wfe_exit_buf),
									GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_hyp_wfe_exit_count_info(auto_ioctl_buf->auto_ioctl_data,
										mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	case HYP_IPI_LATENCY:
	{
		mbraink_auto_data = kzalloc(sizeof(struct mbraink_hyp_cpu_ipi_data_ringbuffer),
									GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_hyp_ipi_info(auto_ioctl_buf->auto_ioctl_data, mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	case HYP_VIRQ_INJECT_LATENCY:
	{
		mbraink_auto_data = kzalloc(sizeof(struct mbraink_hyp_comcat_trace_buf),
									GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_hyp_virq_inject_info(auto_ioctl_buf->auto_ioctl_data,
										mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	case HYP_VCPU_SCHED_LATENCY:
	{
		mbraink_auto_data = kzalloc(sizeof(struct mbraink_hyp_vCpu_sched_delay_buf),
									GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_hyp_vcpu_sched_info(auto_ioctl_buf->auto_ioctl_data,
										mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	case HOST_NOTIFY_CLIENT_INFO:
	{
		mbraink_auto_data = kzalloc(sizeof(struct mbraink_host_notify_client),
									GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_h2c_notify(auto_ioctl_buf->auto_ioctl_data,
								mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	default:
		pr_notice("%s:illegal ioctl number %u.\n", __func__,
			auto_ioctl_buf->auto_ioctl_type);
		return -EINVAL;
	}

	return ret;
End:
	pr_info("%s: kmalloc failed\n", __func__);
	return -ENOMEM;
}

int mbraink_auto_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	ret = vhost_mbraink_init();
	if (ret)
		pr_notice("mbraink virtio init failed.\n");
#endif

	return ret;
}

void mbraink_auto_deinit(void)
{
#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	//vhost_mbraink_deinit();
#endif
}
