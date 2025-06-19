// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "fpsgo_base.h"
#include "fps_composer.h"
#include "mt-plat/fpsgo_common.h"
#include "fpsgo_adpf.h"
#include "powerhal_cpu_ctrl.h"

struct workqueue_struct *adpf_wq;

static void fpsgo_do_adpf_hint(struct work_struct *work)
{
	unsigned long long local_key = 0xFF;
	struct fpsgo_adpf_session *iter = NULL;

	iter = container_of(work, struct fpsgo_adpf_session, sWork);

	local_key = (local_key & iter->sid) | ((unsigned long long)iter->tgid << 8);
	switch (iter->cmd) {
	case ADPF_CREATE_HINT_SESSION:
		fpsgo_ctrl2comp_user_create(iter->tgid, iter->tgid, local_key,
			iter->dep_task_arr, iter->dep_task_num, iter->target_time);
		break;
	case ADPF_REPORT_ACTUAL_WORK_DURATION:
		fpsgo_ctrl2comp_report_workload(iter->tgid, iter->tgid, local_key,
			iter->workload_tcpu_arr, iter->workload_ts_arr, iter->workload_num);
		break;
	case ADPF_UPDATE_TARGET_WORK_DURATION:
		fpsgo_ctrl2comp_set_target_time(iter->tgid, iter->tgid, local_key,
			iter->target_time);
		break;
	case ADPF_SET_THREADS:
		fpsgo_ctrl2comp_set_dep_list(iter->tgid, iter->tgid, local_key,
			iter->dep_task_arr, iter->dep_task_num);
		break;
	case ADPF_PAUSE:
		fpsgo_ctrl2comp_control_pause(iter->tgid, local_key);
		break;
	case ADPF_RESUME:
		fpsgo_ctrl2comp_control_resume(iter->tgid, local_key);
		break;
	case ADPF_CLOSE:
		fpsgo_ctrl2comp_user_close(iter->tgid, iter->tgid, local_key);
		break;
	default:
		fpsgo_main_trace("[adpf] %s unknown cmd:%d", __func__, iter->cmd);
		break;
	}

	kfree(iter->workload_ts_arr);
	kfree(iter->workload_tcpu_arr);
	kfree(iter->dep_task_arr);
	kfree(iter);
}

int fpsgo_receive_adpf_hint(struct _SESSION *session)
{
	int i;
	struct fpsgo_adpf_session *iter = NULL;

	if (!fpsgo_is_enable())
		return -EACCES;

	if (!session)
		return -EFAULT;

	if (!adpf_wq)
		return -EINVAL;

	iter = kzalloc(sizeof(struct fpsgo_adpf_session), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;
	iter->dep_task_arr = kcalloc(ADPF_MAX_THREAD, sizeof(int), GFP_KERNEL);
	iter->workload_tcpu_arr = kcalloc(ADPF_MAX_THREAD, sizeof(unsigned long long), GFP_KERNEL);
	iter->workload_ts_arr = kcalloc(ADPF_MAX_THREAD, sizeof(unsigned long long), GFP_KERNEL);
	if (!iter->dep_task_arr || !iter->workload_tcpu_arr || !iter->workload_ts_arr)
		goto malloc_err;

	iter->cmd = session->cmd;
	iter->sid = session->sid;
	iter->tgid = session->tgid;
	iter->uid = session->uid;
	iter->used = session->used;
	iter->target_time = session->durationNanos;

	if (session->threadIds_size <= ADPF_MAX_THREAD &&
		session->threadIds_size >= 0)
		iter->dep_task_num = session->threadIds_size;
	else
		iter->dep_task_num = 0;
	for (i = 0; i < iter->dep_task_num; i++)
		iter->dep_task_arr[i] = session->threadIds[i];

	if (session->work_duration_size <= ADPF_MAX_THREAD &&
		session->work_duration_size >= 0)
		iter->workload_num = session->work_duration_size;
	else
		iter->workload_num = 0;
	for (i = 0; i < iter->workload_num; i++) {
		iter->workload_tcpu_arr[i] = session->workDuration[i]->durationNanos;
		iter->workload_ts_arr[i] = session->workDuration[i]->timeStampNanos;
	}

	INIT_WORK(&iter->sWork, fpsgo_do_adpf_hint);
	queue_work(adpf_wq, &iter->sWork);

	return 0;

malloc_err:
	kfree(iter->workload_ts_arr);
	kfree(iter->workload_tcpu_arr);
	kfree(iter->dep_task_arr);
	kfree(iter);
	return -ENOMEM;
}

void __init fpsgo_adpf_init(void)
{
	adpf_wq = alloc_ordered_workqueue("fpsgo_adpf_wq", 0);
	adpf_register_callback(fpsgo_receive_adpf_hint);
}

