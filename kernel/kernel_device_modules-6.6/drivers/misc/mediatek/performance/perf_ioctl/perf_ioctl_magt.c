// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "perf_ioctl_magt.h"
#include "fpsgo_base.h"
#define TAG "PERF_IOCTL_MAGT"
#define cap_scale(v, s) ((v)*(s) >> SCHED_CAPACITY_SHIFT)
#define MAX_RENDER_TID 10
static struct proc_dir_entry *perfmgr_root;
static DEFINE_MUTEX(cpu_lock);

static int thermal_aware_threshold = -1;
static int fpsdrop_aware_threshold = -1;
static int advice_bat_avg_current = -1;
static int advice_bat_max_current = -1;
static int targetfps_throttling_temp = -1;
static int thermal_aware_light_threshold = -1;
static int game_suggestion_jobworker = -1;

module_param(thermal_aware_threshold, int, 0644);
module_param(fpsdrop_aware_threshold, int, 0644);
module_param(advice_bat_avg_current, int, 0644);
module_param(advice_bat_max_current, int, 0644);
module_param(targetfps_throttling_temp, int, 0644);
module_param(thermal_aware_light_threshold, int, 0644);
module_param(game_suggestion_jobworker, int, 0644);

static unsigned long perfctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}
static unsigned long perfctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);
	return ulBytes;
}

/*--------------------GET CPU INFO------------------------*/
static struct cpu_time *cur_wall_time, *cur_idle_time,
						*prev_wall_time, *prev_idle_time;
static struct cpu_info ci;
static int *num_cpus;

unsigned long capacity_curr_of(int cpu)
{
	unsigned long max_cap = cpu_rq(cpu)->cpu_capacity_orig;
	unsigned long scale_freq = arch_scale_freq_capacity(cpu);

	return cap_scale(max_cap, scale_freq);
}

static int arch_get_nr_clusters(void)
{
	int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->cluster_id > max_id)
			max_id = cpu_topo->cluster_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->cluster_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

int init_cpu_time(void)
{
	int i;
	int cpu_num = num_possible_cpus();

	mutex_lock(&cpu_lock);
	cur_wall_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(cur_wall_time))
		goto err_cur_wall_time;

	cur_idle_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(cur_idle_time))
		goto err_cur_idle_time;

	prev_wall_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(prev_wall_time))
		goto err_prev_wall_time;

	prev_idle_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(prev_idle_time))
		goto err_prev_idle_time;

	// _ci = kcalloc(1, sizeof(struct cpu_info), GFP_KERNEL);
	for_each_possible_cpu(i) {
		prev_wall_time[i].time = cur_wall_time[i].time = 0;
		prev_idle_time[i].time = cur_idle_time[i].time = 0;
	}
	mutex_unlock(&cpu_lock);
	return 0;

err_prev_idle_time:
	kfree(prev_wall_time);
err_prev_wall_time:
	kfree(cur_idle_time);
err_cur_idle_time:
	kfree(cur_wall_time);
err_cur_wall_time:
	pr_debug(TAG "%s failed to alloc cpu time", __func__);
	mutex_unlock(&cpu_lock);
	return -ENOMEM;
}

int init_num_cpus(void)
{
	struct cpumask cluster_cpus;
	int i, cluster_nr;

	cluster_nr = arch_get_nr_clusters();
	// Get first cpu id of clusters.
	mutex_lock(&cpu_lock);
	num_cpus = kcalloc(cluster_nr + 1, sizeof(int), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(num_cpus)) {
		pr_debug(TAG "%s failed to alloc num cpus", __func__);
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}
	num_cpus[0] = 0;
	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		num_cpus[i + 1] = num_cpus[i] + cpumask_weight(&cluster_cpus);
		// pr_info("perf_index num_cpus = %d\n", num_cpus[i]);
	}
	mutex_unlock(&cpu_lock);
	return 0;
}

int get_cpu_loading(struct cpu_info *_ci)
{
	int i, cpu_loading = 0;
	u64 wall_time = 0, idle_time = 0;

	mutex_lock(&cpu_lock);
	if (ZERO_OR_NULL_PTR(cur_wall_time)) {
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}

	for (i = 0; i < max_cpus; i++)
		_ci->cpu_loading[i] = 0;

	for_each_possible_cpu(i) {

		if (i >= max_cpus)
			break;

		cpu_loading = 0;
		wall_time = 0;
		idle_time = 0;
		prev_wall_time[i].time = cur_wall_time[i].time;
		prev_idle_time[i].time = cur_idle_time[i].time;
		/*idle time include iowait time*/
		cur_idle_time[i].time = get_cpu_idle_time(i,
				&cur_wall_time[i].time, 1);
		if (cpu_active(i)) {
			wall_time = cur_wall_time[i].time - prev_wall_time[i].time;
			idle_time = cur_idle_time[i].time - prev_idle_time[i].time;
		}
		if (wall_time > 0 && wall_time > idle_time)
			cpu_loading = div_u64((100 * (wall_time - idle_time)),
			wall_time);
		_ci->cpu_loading[i] = cpu_loading;
		// pr_info("CPU %d loading is %d%%\n", i, _ci->cpu_loading[i]);
	}
	mutex_unlock(&cpu_lock);
	return 0;
}
EXPORT_SYMBOL(get_cpu_loading);

int get_perf_index(struct cpu_info *_ci)
{
	int i, cluster_nr, perf_index = 0, cluster_idx = 0;

	cluster_nr = arch_get_nr_clusters();

	mutex_lock(&cpu_lock);
	if (ZERO_OR_NULL_PTR(num_cpus)) {
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}

	for (i = 0; i < cluster_nr; i++)
		_ci->perf_index[i] = 0;
	for_each_possible_cpu(i) {
		if (i == num_cpus[cluster_idx]) {
			perf_index = div_u64(100 * capacity_curr_of(i), 1024);
			_ci->perf_index[cluster_idx] = perf_index;
			// pr_info("cluster %d perf_index = %d\n",
			// cluster_idx, _ci->perf_index[cluster_idx]);
			cluster_idx++;
		}
	}
	mutex_unlock(&cpu_lock);
	return 0;
}
EXPORT_SYMBOL(get_perf_index);

/*--------------------XGF SET DEPLIST------------------------*/
int (*magt2fpsgo_notify_dep_list_fp)(int pid,
		void *user_dep_arr,
		int user_dep_num);
EXPORT_SYMBOL(magt2fpsgo_notify_dep_list_fp);

/*--------------------FSTB SET TARGET FPS------------------------*/
int (*magt2fpsgo_notify_target_fps_fp)(int *pid_arr,
		int *tid_arr,
		int *tfps_arr,
		int num);
EXPORT_SYMBOL(magt2fpsgo_notify_target_fps_fp);

int (*magt2fpsgo_get_all_fps_control_pid_info)(struct fps_control_pid_info *arr);
EXPORT_SYMBOL(magt2fpsgo_get_all_fps_control_pid_info);

int (*magt2fpsgo_get_fpsgo_frame_info)(int max_num, unsigned long mask,
	struct render_frame_info *frame_info_arr);
EXPORT_SYMBOL(magt2fpsgo_get_fpsgo_frame_info);

/*--------------------FPSGO SET THREAD STATUS------------------------*/
int (*magt2fpsgo_notify_thread_status_fp)(unsigned int frameid,
	unsigned int type,
	unsigned int status,
	unsigned long long tv_ts);
EXPORT_SYMBOL(magt2fpsgo_notify_thread_status_fp);

/*--------------------MAGT IOCTL------------------------*/
static long magt_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct cpu_info *ciUM, *ciKM = &ci;
	struct target_fps_info *tfiKM = NULL, *tfiUM = NULL;
	struct target_fps_info tfi;
	unsigned long query_mask = 0;
	struct thread_status_info *tsiKM = NULL, *tsiUM = NULL;
	struct thread_status_info tsi;

	switch (cmd) {
	case MAGT_GET_CPU_LOADING:
	{
		ret = get_cpu_loading(ciKM);
		if (ret < 0)
			goto ret_ioctl;
		ciUM = (struct cpu_info *)arg;
		perfctl_copy_to_user(ciUM, ciKM,
			sizeof(struct cpu_info));
		break;
	}
	case MAGT_GET_PERF_INDEX:
	{
		ret = get_perf_index(ciKM);
		if (ret < 0)
			goto ret_ioctl;
		ciUM = (struct cpu_info *)arg;
		perfctl_copy_to_user(ciUM, ciKM,
			sizeof(struct cpu_info));
		break;
	}
	case MAGT_SET_TARGET_FPS:
	{
		if (!magt2fpsgo_notify_target_fps_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}
		tfiUM = (struct target_fps_info *)arg;

		tfiKM = &tfi;
		if (perfctl_copy_from_user(tfiKM, tfiUM,
				sizeof(struct target_fps_info))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (tfiKM->num > MAX_MAGT_TARGET_FPS_NUM ) {
			ret = -EINVAL;
			goto ret_ioctl;
		}

		ret = magt2fpsgo_notify_target_fps_fp(tfiKM->pid_arr,
			tfiKM->tid_arr, tfiKM->tfps_arr, tfiKM->num);
		break;
	}

	case MAGT_SET_DEP_LIST_V3:
	{
		struct dep_list_info_V3 *dliUM;
		struct dep_list_info_V3 dli;
		if (!magt2fpsgo_notify_dep_list_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		dliUM = (struct dep_list_info_V3 *)arg;

		if (perfctl_copy_from_user(&dli, dliUM,
				sizeof(struct dep_list_info_V3))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (dli.user_dep_num > MAGT_DEP_LIST_NUM ) {
			ret = -EINVAL;
			goto ret_ioctl;
		}

		ret = magt2fpsgo_notify_dep_list_fp(dli.pid,
			dli.user_dep_arr, dli.user_dep_num);
		break;
	}
	case MAGT_GET_FPSGO_SUPPORT:
	{
		struct fpsgo_pid_support pid_support;
		struct render_frame_info *render;

		if (perfctl_copy_from_user(&pid_support, (void *)arg,
			sizeof(struct fpsgo_pid_support))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (!magt2fpsgo_get_fpsgo_frame_info) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		query_mask = (1 << GET_FPSGO_PERF_IDX);
		render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!render) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);
		if (ret >= 0) {
			int i = 0;

			for (i = 0; i < ret; i++) {
				if (render[i].tgid == pid_support.pid) {
					pid_support.isSupport = true;
					break;
				}
			}
			perfctl_copy_to_user((void *)arg, &pid_support, sizeof(struct fpsgo_pid_support));
			ret = 0;
		}
		kfree(render);
		break;
	}
	case MAGT_GET_FPSGO_STATUS:
	{
		struct fpsgo_render_status render_status;
		struct render_frame_info *render;
		const int unitConversion = -1000;

		if (perfctl_copy_from_user(&render_status, (void *)arg,
			sizeof(struct fpsgo_render_status))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (!magt2fpsgo_get_fpsgo_frame_info) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		query_mask = (1 << GET_FPSGO_TARGET_FPS | 1 << GET_FPSGO_QUEUE_FPS
			| 1 << GET_FRS_TARGET_FPS_DIFF | 1 << GET_GED_GPU_TIME);

		render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!render) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);

		if (ret >= 0) {
			int i = 0;
			int render_item = -1;

			for (i = 0; i < ret; i++) {
				if (render_status.pid == render[i].pid) {
					render_item = i;
					break;
				}
			}

			if (render_item == -1) {
				ret = -EINVAL;
				kfree(render);
				break;
			}
			render_status.curFps = render[render_item].queue_fps;
			render_status.targetFps = render[render_item].target_fps;
			render_status.targetFps_diff = (render[render_item].target_fps_diff / unitConversion);
			render_status.t_gpu = render[render_item].t_gpu;
			perfctl_copy_to_user((void *)arg, &render_status, sizeof(struct fpsgo_render_status));
			ret = 0;
		}
		kfree(render);
		break;
	}
	case MAGT_GET_FPSGO_CRITICAL_THREAD_BG:
	{
		struct fpsgo_bg_info bg_info;
		struct render_frame_info *render;

		if (perfctl_copy_from_user(&bg_info, (void *)arg,
			sizeof(struct fpsgo_bg_info))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (!magt2fpsgo_get_fpsgo_frame_info) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		query_mask = (1 << GET_FPSGO_MINITOP_LIST);
		render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!render) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);

		if (ret >= 0) {
			int i = 0;
			int render_item = -1;

			for (i = 0; i < ret; i++) {
				if (bg_info.pid == render[i].pid) {
					render_item = i;
					break;
				}
			}

			if (render_item == -1) {
				ret = -EINVAL;
				kfree(render);
				break;
			}
			bg_info.bg_num = render[render_item].non_dep_num;

			for (i = 0; i < bg_info.bg_num && i < FPSGO_MAX_TASK_NUM; i++) {
				bg_info.bg_pid[i] = render[render_item].non_dep_arr[i].pid;
				bg_info.bg_loading[i] = render[render_item].non_dep_arr[i].loading;
			}

			perfctl_copy_to_user((void *)arg, &bg_info, sizeof(struct fpsgo_bg_info));
			ret = 0;
		}
		kfree(render);
		break;
	}
	case MAGT_GET_FPSGO_CPU_FRAMETIME:
	{
		struct fpsgo_cpu_frametime cpu_time_info;
		struct render_frame_info *render;

		if (perfctl_copy_from_user(&cpu_time_info, (void *)arg,
			sizeof(struct fpsgo_cpu_frametime))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (!magt2fpsgo_get_fpsgo_frame_info) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		query_mask = (1 << GET_FPSGO_RAW_CPU_TIME | 1 << GET_FPSGO_EMA_CPU_TIME);
		render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!render) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);

		if (ret >= 0) {
			int i = 0;
			int render_item = -1;

			for (i = 0; i < ret; i++) {
				if (cpu_time_info.pid == render[i].pid) {
					render_item = i;
					break;
				}
			}

			if (render_item == -1) {
				ret = -EINVAL;
				kfree(render);
				break;
			}
			cpu_time_info.raw_t_cpu = render[render_item].raw_t_cpu;
			cpu_time_info.ema_t_cpu = render[render_item].ema_t_cpu;

			perfctl_copy_to_user((void *)arg, &cpu_time_info, sizeof(struct fpsgo_cpu_frametime));
			ret = 0;
		}
		kfree(render);
		break;
	}
	case MAGT_GET_FPSGO_THREAD_LOADING:
	{
		struct fpsgo_thread_loading thread_loading;
		struct render_frame_info *render;

		if (perfctl_copy_from_user(&thread_loading, (void *)arg,
			sizeof(struct fpsgo_thread_loading))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (!magt2fpsgo_get_fpsgo_frame_info) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		query_mask = (1 << GET_FPSGO_AVG_FRAME_CAP | 1 << GET_FPSGO_DEP_LIST);
		render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!render) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);

		if (ret >= 0) {
			int i = 0;
			int render_item = -1;

			for (i = 0; i < ret; i++) {
				if (thread_loading.pid == render[i].pid) {
					render_item = i;
					break;
				}
			}

			if (render_item == -1) {
				ret = -EINVAL;
				kfree(render);
				break;
			}

			thread_loading.avg_freq = render[render_item].avg_frame_cap;
			thread_loading.dep_num = render[render_item].dep_num;
			for (i = 0; i < thread_loading.dep_num && i < FPSGO_MAX_TASK_NUM; i++) {
				thread_loading.dep_pid[i] = render[render_item].dep_arr[i].pid;
				thread_loading.dep_loading[i] = render[render_item].dep_arr[i].loading;
			}

			perfctl_copy_to_user((void *)arg, &thread_loading, sizeof(struct fpsgo_thread_loading));
			ret = 0;
		}
		kfree(render);
		break;
	}
	case MAGT_GET_FPSGO_RENDER_PERFIDX:
	{
		struct fpsgo_render_perf render_perf;
		struct render_frame_info *render;

		if (perfctl_copy_from_user(&render_perf, (void *)arg,
			sizeof(struct fpsgo_render_perf))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (!magt2fpsgo_get_fpsgo_frame_info) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		query_mask = (1 << GET_FPSGO_PERF_IDX);
		render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!render) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);

		if (ret >= 0) {
			int i = 0;
			int render_item = -1;

			for (i = 0; i < ret; i++) {
				if (render_perf.pid == render[i].pid) {
					render_item = i;
					break;
				}
			}

			if (render_item == -1) {
				ret = -EINVAL;
				kfree(render);
				break;
			}
			render_perf.perf_idx = render[render_item].blc;
			perfctl_copy_to_user((void *)arg, &render_perf, sizeof(struct fpsgo_render_perf));
			ret = 0;
		}
		kfree(render);
		break;
	}
	case MAGT_NOTIFY_THREAD_STATUS:
		if (!magt2fpsgo_notify_thread_status_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}
		tsiUM = (struct thread_status_info *)arg;
		tsiKM = &tsi;

		if (perfctl_copy_from_user(tsiKM, tsiUM,
				sizeof(struct thread_status_info))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		ret = magt2fpsgo_notify_thread_status_fp(tsiKM->frameid,
			tsiKM->type, tsiKM->status, tsiKM->tv_ts);
		break;

	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret =  -EINVAL;
		goto ret_ioctl;
	}
ret_ioctl:
	return ret;
}

static int magt_show(struct seq_file *m, void *v)
{
	return 0;
}
static int magt_open(struct inode *inode, struct file *file)
{
	return single_open(file, magt_show, inode->i_private);
}
static const struct proc_ops Fops = {
	.proc_compat_ioctl = magt_ioctl,
	.proc_ioctl = magt_ioctl,
	.proc_open = magt_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------INIT------------------------*/
static void __exit exit_magt_perfctl(void)
{
	mutex_lock(&cpu_lock);
	kfree(cur_wall_time);
	kfree(cur_idle_time);
	kfree(prev_wall_time);
	kfree(prev_idle_time);
	kfree(num_cpus);
	mutex_unlock(&cpu_lock);
}

static int __init init_magt_perfctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;

	pr_debug(TAG"Start to init MAGT perf_ioctl driver\n");
	parent = proc_mkdir("perfmgr_magt", NULL);
	perfmgr_root = parent;
	pe = proc_create("magt_ioctl", 0660, parent, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}
	pr_debug(TAG"init magt_ioctl driver done\n");

	ret_val |= init_cpu_time();
	ret_val |= init_num_cpus();

	if (ret_val < 0)
		goto out_wq;

	return 0;
out_wq:
	return ret_val;
}
module_init(init_magt_perfctl);
module_exit(exit_magt_perfctl);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MAGT ioctl");
MODULE_AUTHOR("MediaTek Inc.");
