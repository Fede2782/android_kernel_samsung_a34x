// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "mbraink_hypervisor.h"

long mbraink_hyp_getWfeExitCountInfo(struct mbraink_hyp_wfe_exit_buf *wfe_exit_buf)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_wfe_exit_buf *tmp_wfe_exit_buf;

	if (!wfe_exit_buf)
		return -1;

	result = start_record_wfe_exit();
	if (result < 0) {
		pr_info("start record hypervisor wfe exit count failed\n");
		return result;
	}

	msleep(100);

	result = stop_record_wfe_exit();
	if (result < 0) {
		pr_info("stop record hypervisor wfe exit count failed\n");
		return result;
	}

	tmp_wfe_exit_buf = (struct mbraink_hyp_wfe_exit_buf *)get_wfe_exit_buf();
	memcpy(wfe_exit_buf, tmp_wfe_exit_buf, sizeof(struct mbraink_hyp_wfe_exit_buf));

	return ret;
}

long mbraink_hyp_getIpiInfo(struct mbraink_hyp_cpu_ipi_data_ringbuffer *cpu_ipi_data_ringbuffer)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_cpu_ipi_data_ringbuffer *tmp_cpu_ipi_data_ringbuffer;

	if (!cpu_ipi_data_ringbuffer)
		return -1;

	result = start_record_cpu_ipi_delay();
	if (result < 0) {
		pr_info("start record hypervisor ipi latency failed\n");
		return result;
	}

	msleep(100);

	result = stop_record_cpu_ipi_delay();
	if (result < 0) {
		pr_info("stop record hypervisor ipi latency failed\n");
		return result;
	}

	tmp_cpu_ipi_data_ringbuffer = (struct mbraink_hyp_cpu_ipi_data_ringbuffer *)
									get_cpu_ipi_delay_buf();
	memcpy(cpu_ipi_data_ringbuffer, tmp_cpu_ipi_data_ringbuffer,
			sizeof(struct mbraink_hyp_cpu_ipi_data_ringbuffer));

	return ret;
}

long mbraink_hyp_getvIrqInjectLatency(struct mbraink_hyp_comcat_trace_buf *hyp_comcat_trace_buf)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_comcat_trace_buf *tmp_hyp_comcat_trace_buf;

	if (!hyp_comcat_trace_buf)
		return -1;

	result = start_record_comcat_trace();
	if (result < 0) {
		pr_info("start record hypervisor vIrq inject latency failed\n");
		return result;
	}

	msleep(100);

	result = stop_record_comcat_trace();
	if (result < 0) {
		pr_info("stop record hypervisor vIrq inject latency failed\n");
		return result;
	}

	tmp_hyp_comcat_trace_buf = (struct mbraink_hyp_comcat_trace_buf *)get_comcat_trace_buf();
	memcpy(hyp_comcat_trace_buf, tmp_hyp_comcat_trace_buf,
			sizeof(struct mbraink_hyp_comcat_trace_buf));

	return ret;
}

long mbraink_hyp_getvCpuSchedInfo(struct mbraink_hyp_vCpu_sched_delay_buf *vCpu_sched_delay_buf)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_vCpu_sched_delay_buf *tmp_vCpu_sched_delay_buf;

	if (!vCpu_sched_delay_buf)
		return -1;

	result = start_record_vcpu_sched_delay();
	if (result < 0) {
		pr_info("start record hypervisor vcpu sched delay failed\n");
		return result;
	}

	msleep(1000);

	result = stop_record_vcpu_sched_delay();
	if (result < 0) {
		pr_info("stop record hypervisor vcpu sched delay failed\n");
		return result;
	}
	tmp_vCpu_sched_delay_buf = (struct mbraink_hyp_vCpu_sched_delay_buf *)
								get_vcpu_sched_delay_buf();
	memcpy(vCpu_sched_delay_buf, tmp_vCpu_sched_delay_buf,
			sizeof(struct mbraink_hyp_vCpu_sched_delay_buf));

	return ret;
}

int mbraink_hyp_set_vcpu_switch_info(int enable)
{
	int ret = 0;

	ret = enable ?  virt_mb_enable_vcpu_info() : virt_mb_disable_vcpu_info();
	if (ret)
		pr_info("%s vcpu record failed\n", enable ? "enable" : "disable");
	return ret;
}

int mbraink_hyp_get_vcpu_switch_info(struct mbraink_vcpu_buf *mbraink_vcpu_exec_trans_buf,
					void *vcpu_switch_info_buffer)
{
	int ret = 0;
	struct timespec64 ts;
	size_t rd, wr = 0;
	size_t len, total_len = 0;
	struct vcpu_exec_buf *vcpu_exec = virt_mb_get_vcpu_buf();
	int raw_record_length = mbraink_vcpu_exec_trans_buf->length;

	if (!vcpu_exec) {
		pr_info("%s: get vcpu exec buf fail\n", __func__);
		goto err;
	}
	if (!vcpu_exec->rb_hdr.enable) {
		pr_info("%s: vcpu exec buf not enable\n", __func__);
		goto err;
	}

	ret = virt_mb_disable_vcpu_info();

	wr = vcpu_exec->rb_hdr.write_pos % vcpu_exec->rb_hdr.rec_cnt;
	/* record length is bigger than write_pos */
	if (raw_record_length > wr) {
		/* write pos has overflowed */
		if (vcpu_exec->rb_hdr.write_pos >= vcpu_exec->rb_hdr.rec_cnt) {
			rd = vcpu_exec->rb_hdr.rec_cnt - (raw_record_length - wr);
			len = (vcpu_exec->rb_hdr.rec_cnt - rd) * vcpu_exec->rb_hdr.rec_size;
			memcpy(vcpu_switch_info_buffer, (u8 *)&vcpu_exec->recs[rd], len);
			total_len += len;
		}
		/* if write_pos not overflow. we only have write_pos to copy.
		 * so set raw_record_length to wr
		 */
		raw_record_length = wr;
	}
	rd = wr - raw_record_length;
	len = (wr - rd) * vcpu_exec->rb_hdr.rec_size;
	memcpy(vcpu_switch_info_buffer + total_len, (u8 *)&vcpu_exec->recs[rd], len);
	total_len += len;

	mbraink_vcpu_exec_trans_buf->length = total_len / vcpu_exec->rb_hdr.rec_size;
	mbraink_vcpu_exec_trans_buf->cntcvt = __arch_counter_get_cntvct();
	mbraink_vcpu_exec_trans_buf->cntfrq = arch_timer_get_cntfrq();
	ktime_get_real_ts64(&ts);
	mbraink_vcpu_exec_trans_buf->current_time = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;

	ret = virt_mb_enable_vcpu_info();

	pr_notice("%s: read HV cpu loading data. data length %d\n",
				__func__, mbraink_vcpu_exec_trans_buf->length);
	return ret;

err:
	mbraink_vcpu_exec_trans_buf->length = 0;
	return -1;
}

