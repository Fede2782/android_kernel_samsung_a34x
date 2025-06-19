/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MBRAINK_HYPERVISOR_H
#define MBRAINK_HYPERVISOR_H

#include <mbraink_auto_ioctl_struct_def.h>

typedef struct {
	uint32_t reason[NBL_TRACE_VCPU_CNT][NBL_TRACE_VM_CNT];
} wfe_exit_buf_t;
extern int start_record_wfe_exit(void);
extern int stop_record_wfe_exit(void);
extern wfe_exit_buf_t *get_wfe_exit_buf(void);

typedef struct {
	struct mbraink_send_timestamp sendIpi[MAX_BUF_SIZE];
	struct mbraink_received_timestamp receiveIpi[MAX_BUF_SIZE];
	uint32_t snd_point;
	uint32_t rsv_point;
	bool enable;
} cpu_ipi_delay_buf_t;
extern int start_record_cpu_ipi_delay(void);
extern int stop_record_cpu_ipi_delay(void);
extern cpu_ipi_delay_buf_t *get_cpu_ipi_delay_buf(void);

typedef struct {
	struct mbraink_hyp_comcat_trace_ringbuf_hdr rb_hdr;
	struct mbraink_hyp_comcat_rec rec[COMCAT_TRACE_MAX_RECS];
} comcat_trace_buf_t;
extern int start_record_comcat_trace(void);
extern int stop_record_comcat_trace(void);
extern comcat_trace_buf_t *get_comcat_trace_buf(void);

typedef struct {
	struct mbraink_hyp_vCpu_sched_delay_buf_hdr delay[NBL_TRACE_VM_CNT][NBL_TRACE_VCPU_CNT];
	bool enable;
} vcpu_sched_delay_buf_t;
extern int start_record_vcpu_sched_delay(void);
extern int stop_record_vcpu_sched_delay(void);
extern vcpu_sched_delay_buf_t *get_vcpu_sched_delay_buf(void);

struct vcpu_ringbuf_hdr {
	uint64_t write_pos;
	uint64_t read_pos;
	size_t hdr_size;
	size_t rec_size;
	uint32_t rec_cnt;
	uint8_t reserved0[4];
	bool enable;
	uint8_t reserved1[7];
};

#define VCPU_EXEC_REC_CNT     10000
#define TRACE_VCPU_EXEC_REC_CNT \
	(NBL_TRACE_VM_CNT * NBL_TRACE_VCPU_CNT * VCPU_EXEC_REC_CNT)

struct vcpu_exec_buf {
	struct vcpu_ringbuf_hdr rb_hdr;
	struct vcpu_exec_rec recs[TRACE_VCPU_EXEC_REC_CNT];
};

extern int virt_mb_enable_vcpu_info(void);
extern int virt_mb_disable_vcpu_info(void);
extern struct vcpu_exec_buf *virt_mb_get_vcpu_buf(void);

long mbraink_hyp_getWfeExitCountInfo(struct mbraink_hyp_wfe_exit_buf *wfe_exit_buf);
long mbraink_hyp_getIpiInfo(struct mbraink_hyp_cpu_ipi_data_ringbuffer *cpu_ipi_data_ringbuffer);
long mbraink_hyp_getvIrqInjectLatency(struct mbraink_hyp_comcat_trace_buf *hyp_comcat_trace_buf);
long mbraink_hyp_getvCpuSchedInfo(struct mbraink_hyp_vCpu_sched_delay_buf *vCpu_sched_delay_buf);

/* vcpu switch info */
int mbraink_hyp_set_vcpu_switch_info(int enable);
int mbraink_hyp_get_vcpu_switch_info(struct mbraink_vcpu_buf *mbraink_vcpu_exec_trans_buf,
					void *vcpu_switch_info_buffer);

#endif /*end of MBRAINK_HYPERVISOR_H*/
