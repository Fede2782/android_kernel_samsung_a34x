/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_ADPF_H__
#define __FPSGO_ADPF_H__

struct fpsgo_adpf_session {
	int cmd;
	int sid;
	int tgid;
	int uid;
	int *dep_task_arr;
	int dep_task_num;
	unsigned long long *workload_tcpu_arr;
	unsigned long long *workload_ts_arr;
	int workload_num;
	int used;
	unsigned long long target_time;

	struct work_struct sWork;
};

void __init fpsgo_adpf_init(void);

#endif
