/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_AUTO_IOCTL_STRUCT_H
#define MBRAINK_AUTO_IOCTL_STRUCT_H

#include <linux/types.h>

/*auto ioctl case*/
#define HYP_VCPU_SWITCH_INFO		0
#define HYP_WFE_EXIT_COUNT			1
#define HYP_IPI_LATENCY				2
#define HYP_VIRQ_INJECT_LATENCY		3
#define HYP_VCPU_SCHED_LATENCY		4
#define HOST_NOTIFY_CLIENT_INFO		5

struct vcpu_exec_rec {
	u64 vmid : 2;
#define TRACE_YOCTO_VMID      0
#define TRACE_ANDROID_VMID    1
	u64 vcpu : 5;
	u64 pcpu : 5;
	u64 flag : 2;
	u64 timestamp : 50;
};

struct mbraink_vcpu_buf {
	u32 trans_type;
	u32 length;
	u64 current_time;
	u64 cntcvt;
	u64 cntfrq;
	void *vcpu_data;
};

/*wfe exit count*/
#define NBL_TRACE_VCPU_CNT  8
#define NBL_TRACE_VM_CNT    2

struct mbraink_hyp_wfe_exit_buf {
	uint32_t reason[NBL_TRACE_VCPU_CNT][NBL_TRACE_VM_CNT];
};

/*ipi latency*/
#define MAX_BUF_SIZE 128
struct mbraink_send_timestamp {
	uint8_t  src_cpu;
	uint8_t  target_cpu;
	uint64_t timestamp;
};

struct mbraink_received_timestamp {
	uint8_t  cpu;
	uint64_t timestamp;
};

struct mbraink_hyp_cpu_ipi_data_ringbuffer {
	struct mbraink_send_timestamp sendIpi[MAX_BUF_SIZE];
	struct mbraink_received_timestamp receiveIpi[MAX_BUF_SIZE];
	uint32_t snd_point;
	uint32_t rsv_point;
	bool enable;
};

/*vcpu inject latency*/
#define COMCAT_TRACE_MAX_RECS       2048
struct mbraink_hyp_comcat_trace_ringbuf_hdr {
	uint64_t write_pos;
	uint64_t read_pos;
	size_t hdr_size;
	size_t size;
	size_t rec_size;
	uint32_t rec_cnt;
	bool enable;
	uint8_t reserved[3];
};

struct mbraink_hyp_comcat_rec {
	uint32_t tag;
	uint64_t a;
	uint64_t b;
	uint64_t c;
	uint64_t tid;
	uint64_t flag : 1;
	uint64_t timestamp : 50;
};

struct mbraink_hyp_comcat_trace_buf {
	struct mbraink_hyp_comcat_trace_ringbuf_hdr rb_hdr;
	struct mbraink_hyp_comcat_rec rec[COMCAT_TRACE_MAX_RECS];
};

/*vcpu schedule latency*/
struct mbraink_hyp_vCpu_sched_delay_buf_hdr {
	uint64_t enter_timestamp;
	uint64_t current_delay_time;
	uint64_t max_delay_time;
	uint64_t min_delay_time;
	uint64_t total_delay_time;
	uint64_t thread_id;
	uint64_t run_times;
	uint64_t b_run;
};

struct mbraink_hyp_vCpu_sched_delay_buf {
	struct mbraink_hyp_vCpu_sched_delay_buf_hdr delay[NBL_TRACE_VM_CNT][NBL_TRACE_VCPU_CNT];
	bool enable;
};

struct mbraink_auto_ioctl_info {
	u32 auto_ioctl_type;
	void *auto_ioctl_data;
};

struct mbraink_host_notify_client {
	u32 cmdType;
	void *cmdData;
};

#endif
