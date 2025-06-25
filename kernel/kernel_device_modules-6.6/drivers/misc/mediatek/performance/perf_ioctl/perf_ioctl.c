// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "perf_ioctl.h"

#define TAG "PERF_IOCTL"

int (*fpsgo_notify_qudeq_fp)(int qudeq,
		unsigned int startend,
		int pid, unsigned long long identifier,
		unsigned long long sf_buf_id);
EXPORT_SYMBOL_GPL(fpsgo_notify_qudeq_fp);
void (*fpsgo_notify_connect_fp)(int pid,
		int connectedAPI, unsigned long long identifier);
EXPORT_SYMBOL_GPL(fpsgo_notify_connect_fp);
void (*fpsgo_notify_bqid_fp)(int pid, unsigned long long bufID,
		int queue_SF, unsigned long long identifier, int create);
EXPORT_SYMBOL_GPL(fpsgo_notify_bqid_fp);
void (*fpsgo_notify_vsync_fp)(void);
EXPORT_SYMBOL_GPL(fpsgo_notify_vsync_fp);
void (*fpsgo_notify_vsync_period_fp)(unsigned long long period);
EXPORT_SYMBOL_GPL(fpsgo_notify_vsync_period_fp);
void (*fpsgo_get_fps_fp)(int *pid, int *fps);
EXPORT_SYMBOL_GPL(fpsgo_get_fps_fp);
void (*fpsgo_get_cmd_fp)(int *cmd, int *value1, int *value2);
EXPORT_SYMBOL_GPL(fpsgo_get_cmd_fp);
int (*fpsgo_get_fstb_active_fp)(long long time_diff);
EXPORT_SYMBOL_GPL(fpsgo_get_fstb_active_fp);
int (*fpsgo_wait_fstb_active_fp)(void);
EXPORT_SYMBOL_GPL(fpsgo_wait_fstb_active_fp);
void (*fpsgo_notify_swap_buffer_fp)(int pid);
EXPORT_SYMBOL_GPL(fpsgo_notify_swap_buffer_fp);
void (*fpsgo_notify_acquire_fp)(int c_pid, int p_pid,
	int connectedAPI, unsigned long long buffer_id);
EXPORT_SYMBOL_GPL(fpsgo_notify_acquire_fp);
void (*fpsgo_get_pid_fp)(int cmd, int *pid, int value1, int value2);
EXPORT_SYMBOL_GPL(fpsgo_get_pid_fp);

void (*fpsgo_notify_sbe_rescue_fp)(int pid, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frameID);
EXPORT_SYMBOL_GPL(fpsgo_notify_sbe_rescue_fp);
int (*fpsgo_notify_sbe_policy_fp)(int pid,  char *name,
	unsigned long mask, int start, char *specific_name, int num);
EXPORT_SYMBOL_GPL(fpsgo_notify_sbe_policy_fp);
int (*fpsgo_notify_frame_hint_fp)(int qudeq,
		int pid, int frameID,
		unsigned long long id,
		int dep_mode, char *dep_name, int dep_num, long long frame_flags);
EXPORT_SYMBOL_GPL(fpsgo_notify_frame_hint_fp);
int (*fpsgo_notify_ux_buffer_count_fp)(int pid,int count, int max_buffer);
EXPORT_SYMBOL_GPL(fpsgo_notify_ux_buffer_count_fp);

int (*fpsgo_notify_smart_launch_algorithm_fp)(int feedback_time,
		int target_time, int pre_opp, int capabilty_ration);
EXPORT_SYMBOL_GPL(fpsgo_notify_smart_launch_algorithm_fp);

void (*fpsgo_notify_buffer_quota_fp)(int pid, int quota, unsigned long long identifier);
EXPORT_SYMBOL_GPL(fpsgo_notify_buffer_quota_fp);

#if IS_ENABLED(CONFIG_ARM64)
struct proc_dir_entry *perfmgr_root;
EXPORT_SYMBOL(perfmgr_root);
#else
static struct proc_dir_entry *perfmgr_root;
#endif

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

/*--------------------XGFFRAME------------------------*/
int (*xgff_frame_startend_fp)(unsigned int startend,
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist);
EXPORT_SYMBOL(xgff_frame_startend_fp);
void (*xgff_frame_getdeplist_maxsize_fp)(
		unsigned int *pdeplistsize);
EXPORT_SYMBOL(xgff_frame_getdeplist_maxsize_fp);
void (*xgff_frame_min_cap_fp)(unsigned int min_cap);
EXPORT_SYMBOL(xgff_frame_min_cap_fp);

static int xgff_show(struct seq_file *m, void *v)
{
	return 0;
}

static int xgff_open(struct inode *inode, struct file *file)
{
	return single_open(file, xgff_show, inode->i_private);
}

static long xgff_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	struct _XGFFRAME_PACKAGE *msgKM = NULL,
		*msgUM = (struct _XGFFRAME_PACKAGE *)arg;
	struct _XGFFRAME_PACKAGE smsgKM;

	__u32 *vpdeplist = NULL;
	unsigned int maxsize_deplist = 0;

	msgKM = (struct _XGFFRAME_PACKAGE *)pKM;
	if (!msgKM) {
		msgKM = &smsgKM;
		if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _XGFFRAME_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
	}

	switch (cmd) {
	case XGFFRAME_START:
		if (!xgff_frame_startend_fp || !xgff_frame_getdeplist_maxsize_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		xgff_frame_getdeplist_maxsize_fp(&maxsize_deplist);
		vpdeplist = kcalloc(msgKM->deplist_size, sizeof(__u32), GFP_KERNEL);
		if (!vpdeplist) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}

		if (perfctl_copy_from_user(vpdeplist,
			msgKM->deplist, msgKM->deplist_size * sizeof(__s32))) {
			kfree(vpdeplist);
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (msgKM->deplist_size > maxsize_deplist)
			msgKM->deplist_size = maxsize_deplist;
		ret = xgff_frame_startend_fp(1, msgKM->tid, msgKM->queueid,
			msgKM->frameid, NULL, NULL, &msgKM->deplist_size, vpdeplist);

		perfctl_copy_to_user(msgUM, msgKM, sizeof(struct _XGFFRAME_PACKAGE));

		kfree(vpdeplist);
		break;
	case XGFFRAME_END:
		if (!xgff_frame_startend_fp || !xgff_frame_getdeplist_maxsize_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		xgff_frame_getdeplist_maxsize_fp(&maxsize_deplist);
		vpdeplist = kcalloc(msgKM->deplist_size, sizeof(__u32), GFP_KERNEL);

		if (!vpdeplist) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}

		if (perfctl_copy_from_user(vpdeplist,
			msgKM->deplist, msgKM->deplist_size * sizeof(__s32))) {
			kfree(vpdeplist);
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (msgKM->deplist_size > maxsize_deplist)
			msgKM->deplist_size = maxsize_deplist;

		ret = xgff_frame_startend_fp(0, msgKM->tid, msgKM->queueid,
			msgKM->frameid, &msgKM->cputime, &msgKM->area,
			&msgKM->deplist_size, vpdeplist);

		perfctl_copy_to_user(msgUM, msgKM, sizeof(struct _XGFFRAME_PACKAGE));


		kfree(vpdeplist);

		break;

	case XGFFRAME_MIN_CAP:
		if (!xgff_frame_min_cap_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}
		xgff_frame_min_cap_fp((unsigned int)msgKM->min_cap);
		break;

	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long xgff_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return xgff_ioctl_impl(filp, cmd, arg, NULL);
}

#if IS_ENABLED(CONFIG_COMPAT)
static long xgff_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct _XGFFRAME_PACKAGE_32 {
		__u32 tid;
		__u64 queueid;
		__u64 frameid;

		__u64 cputime;
		__u32 area;
		__u32 deplist_size;

		union {
			__u32 *deplist;
			__u64 p_dummy_deplist;
		};
	};

	struct _XGFFRAME_PACKAGE sEaraPackageKM64;
	struct _XGFFRAME_PACKAGE_32 sEaraPackageKM32;
	struct _XGFFRAME_PACKAGE_32 *psEaraPackageKM32 = &sEaraPackageKM32;
	struct _XGFFRAME_PACKAGE_32 *psEaraPackageUM32 =
		(struct _XGFFRAME_PACKAGE_32 *)arg;

	if (perfctl_copy_from_user(psEaraPackageKM32,
			psEaraPackageUM32, sizeof(struct _XGFFRAME_PACKAGE_32)))
		goto unlock_and_return;

	sEaraPackageKM64 = *((struct _XGFFRAME_PACKAGE *)psEaraPackageKM32);
	sEaraPackageKM64.deplist =
		(void *)((size_t) psEaraPackageKM32->deplist);

	ret = xgff_ioctl_impl(filp, cmd, arg, &sEaraPackageKM64);

unlock_and_return:
	return ret;
}
#endif

static const struct proc_ops xgff_Fops = {
	.proc_ioctl = xgff_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = xgff_compat_ioctl,
#endif
	.proc_open = xgff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------------FPSGO_LR_IOCTL-----------------------*/
int (*fpsgo_get_lr_pair_fp)(unsigned long long sf_buffer_id,
	unsigned long long *cur_queue_ts,
	unsigned long long *l2q_ns, unsigned long long *logic_head_ts,
	unsigned int *is_logic_head_alive, unsigned long long *now_ts);
EXPORT_SYMBOL(fpsgo_get_lr_pair_fp);

void (*fpsgo_set_rl_expected_l2q_us_fp)(int vsync_multiple,
	unsigned long long user_expected_l2q_us);
EXPORT_SYMBOL(fpsgo_set_rl_expected_l2q_us_fp);

void (*fpsgo_set_rl_l2q_enable_fp)(int enable);
EXPORT_SYMBOL(fpsgo_set_rl_l2q_enable_fp);

static long fpsgo_lr_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	struct _FPSGO_LR_PAIR_PACKAGE *msgKM = NULL,
		*msgUM = (struct _FPSGO_LR_PAIR_PACKAGE *)arg;
	struct _FPSGO_LR_PAIR_PACKAGE smsgKM;
	unsigned long long logical_head_ts = 0;
	unsigned long long l2q_ns_ts = 0;
	unsigned long long cur_queue_end_ts = 0;
	unsigned int is_logic_head_alive = 0;
	unsigned long long ktime_ns = 0;

	msgKM = (struct _FPSGO_LR_PAIR_PACKAGE *)pKM;
	if (!msgKM) {
		msgKM = &smsgKM;
		if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _FPSGO_LR_PAIR_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
	}

	switch (cmd) {
	case FPSGO_LR_PAIR:
		if (!fpsgo_get_lr_pair_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		ret = fpsgo_get_lr_pair_fp(msgKM->buffer_id, &cur_queue_end_ts,
			&l2q_ns_ts, &logical_head_ts, &is_logic_head_alive, &ktime_ns);

		if (logical_head_ts)
			msgKM->logic_head_ts = logical_head_ts;
		if (cur_queue_end_ts)
			msgKM->queue_ts = cur_queue_end_ts;
		if (l2q_ns_ts)
			msgKM->l2q_ns = l2q_ns_ts;
		if (is_logic_head_alive)
			msgKM->is_logic_head_valid = is_logic_head_alive;
		if (ktime_ns)
			msgKM->ktime_now_ns = ktime_ns;

		perfctl_copy_to_user(msgUM, msgKM, sizeof(struct _FPSGO_LR_PAIR_PACKAGE));

		break;
	case FPSGO_SF_TOUCH_ACTIVE:
		if (!fpsgo_set_rl_l2q_enable_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		fpsgo_set_rl_l2q_enable_fp(msgKM->fpsgo_l2q_enable);
		break;

	case FPSGO_SF_EXP_L2Q:
		if (!fpsgo_set_rl_expected_l2q_us_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		// default exp_vsync_multiple = 2, rl_exp_l2q_us = 33333
		// If the value isn't equal to zero, then FPSGO will use the user tuning value.
		// If the user set both param. at the same time, the priority of
		// rl_exp_l2q_us value is higher than exp_vsync_multiple value.
		fpsgo_set_rl_expected_l2q_us_fp(msgKM->exp_vsync_multiple,
			msgKM->rl_exp_l2q_us);
		break;

	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long fpsgo_lr_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return fpsgo_lr_ioctl_impl(filp, cmd, arg, NULL);
}

static int fpsgo_lr_show(struct seq_file *m, void *v)
{
	return 0;
}

static int fpsgo_lr_open(struct inode *inode, struct file *file)
{
	return single_open(file, fpsgo_lr_show, inode->i_private);
}


static const struct proc_ops fpsgo_lr_Fops = {
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = fpsgo_lr_ioctl,
#endif
	.proc_ioctl = fpsgo_lr_ioctl,
	.proc_open = fpsgo_lr_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};


/*--------------------INIT------------------------*/

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	int pwr_cmd = -1, value1 = -1, value2 = -1, pwr_pid = -1, pwr_fps = -1;
	struct _FPSGO_PACKAGE *msgKM = NULL, *msgUM = NULL;
	struct _FPSGO_PACKAGE smsgKM;
	struct _FPSGO_SBE_PACKAGE *msgKM_SBE = NULL, *msgUM_SBE = NULL;
	struct _FPSGO_SBE_PACKAGE smsgKM_SBE;
	struct _SMART_LAUNCH_PACKAGE *smart_launch_p;
	struct _SMART_LAUNCH_PACKAGE smart_launch;

	if (cmd == SMART_LAUNCH_ALGORITHM) {
		smart_launch_p = (struct _SMART_LAUNCH_PACKAGE *)arg;
		if (perfctl_copy_from_user(&smart_launch, smart_launch_p,
					sizeof(struct _SMART_LAUNCH_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (fpsgo_notify_smart_launch_algorithm_fp)
			ret = fpsgo_notify_smart_launch_algorithm_fp(smart_launch.feedback_time,
				smart_launch.target_time, smart_launch.pre_opp, smart_launch.capabilty_ration);

		smart_launch.next_opp = ret;
		perfctl_copy_to_user(smart_launch_p, &smart_launch, sizeof(struct _SMART_LAUNCH_PACKAGE));

		goto ret_ioctl;
	}

	if (cmd == FPSGO_SBE_SET_POLICY || cmd == FPSGO_HINT_FRAME) {
		msgUM_SBE = (struct _FPSGO_SBE_PACKAGE *)arg;
		msgKM_SBE = &smsgKM_SBE;
		if (perfctl_copy_from_user(msgKM_SBE, msgUM_SBE,
					sizeof(struct _FPSGO_SBE_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3)
		switch (cmd) {
		case FPSGO_SBE_SET_POLICY:
			if (fpsgo_notify_sbe_policy_fp)
				ret = fpsgo_notify_sbe_policy_fp(msgKM_SBE->pid, msgKM_SBE->name,
						msgKM_SBE->mask, msgKM_SBE->start,
						msgKM_SBE->specific_name, msgKM_SBE->num);
			break;
		case FPSGO_HINT_FRAME:
			if (fpsgo_notify_frame_hint_fp)
				msgKM_SBE->blc = fpsgo_notify_frame_hint_fp(msgKM_SBE->start,
					msgKM_SBE->rtid, msgKM_SBE->frame_id, msgKM_SBE->identifier,
					msgKM_SBE->mode, msgKM_SBE->specific_name, msgKM_SBE->num, msgKM_SBE->mask);
			perfctl_copy_to_user(msgUM_SBE, msgKM_SBE,
				sizeof(struct _FPSGO_SBE_PACKAGE));
			break;
		default:
			pr_debug(TAG "%s %d: unknown SBE cmd %x\n",
				__FILE__, __LINE__, cmd);
			ret = -1;
			break;
		}
#endif
		goto ret_ioctl;
	}

	msgUM = (struct _FPSGO_PACKAGE *)arg;
	msgKM = &smsgKM;

	if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _FPSGO_PACKAGE))) {
		ret = -EFAULT;
		goto ret_ioctl;
	}

	switch (cmd) {
#if defined(CONFIG_MTK_FPSGO_V3)
	case FPSGO_QUEUE:
		if (fpsgo_notify_qudeq_fp)
			ret = fpsgo_notify_qudeq_fp(1,
					msgKM->start, msgKM->tid,
					msgKM->identifier, msgKM->sf_buf_id);
		break;
	case FPSGO_DEQUEUE:
		if (fpsgo_notify_qudeq_fp)
			fpsgo_notify_qudeq_fp(0,
					msgKM->start, msgKM->tid,
					msgKM->identifier, 0);
		break;
	case FPSGO_QUEUE_CONNECT:
		if (fpsgo_notify_connect_fp)
			fpsgo_notify_connect_fp(msgKM->tid,
					msgKM->connectedAPI, msgKM->identifier);
		break;
	case FPSGO_BQID:
		if (fpsgo_notify_bqid_fp)
			fpsgo_notify_bqid_fp(msgKM->tid, msgKM->bufID,
				msgKM->queue_SF, msgKM->identifier,
				msgKM->start);
		break;
	case FPSGO_TOUCH:
		break;
	case FPSGO_VSYNC:
		if (fpsgo_notify_vsync_fp)
			fpsgo_notify_vsync_fp();
		break;
	case FPSGO_VSYNC_PERIOD:
		if (fpsgo_notify_vsync_period_fp)
			fpsgo_notify_vsync_period_fp(msgKM->frame_time);
		break;
	case FPSGO_SWAP_BUFFER:
		if (fpsgo_notify_swap_buffer_fp)
			fpsgo_notify_swap_buffer_fp(msgKM->tid);
		break;
	case FPSGO_GET_FPS:
		if (fpsgo_get_fps_fp) {
			fpsgo_get_fps_fp(&pwr_pid, &pwr_fps);
			msgKM->tid = pwr_pid;
			msgKM->value1 = pwr_fps;
		} else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_GET_CMD:
		if (fpsgo_get_cmd_fp) {
			fpsgo_get_cmd_fp(&pwr_cmd, &value1, &value2);
			msgKM->cmd = pwr_cmd;
			msgKM->value1 = value1;
			msgKM->value2 = value2;
		} else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_GET_FSTB_ACTIVE:
		if (fpsgo_get_fstb_active_fp)
			msgKM->active = fpsgo_get_fstb_active_fp(msgKM->time_diff);
		else
			ret = 0;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_WAIT_FSTB_ACTIVE:
		if (fpsgo_wait_fstb_active_fp)
			fpsgo_wait_fstb_active_fp();
		break;
	case FPSGO_SBE_RESCUE:
		if (fpsgo_notify_sbe_rescue_fp)
			fpsgo_notify_sbe_rescue_fp(msgKM->tid, msgKM->start, msgKM->value2,
						msgKM->identifier, msgKM->frame_time, msgKM->frame_id);
		break;
	case FPSGO_SBE_BUFFER_COUNT:
		if (fpsgo_notify_ux_buffer_count_fp)
			fpsgo_notify_ux_buffer_count_fp(msgKM->tid, msgKM->value1, msgKM->value2);
		break;
	case FPSGO_ACQUIRE:
		if (fpsgo_notify_acquire_fp)
			fpsgo_notify_acquire_fp(msgKM->pid1, msgKM->pid2,
				msgKM->connectedAPI, msgKM->bufID);
		break;
	case FPSGO_BUFFER_QUOTA:
		if (fpsgo_notify_buffer_quota_fp)
			fpsgo_notify_buffer_quota_fp(msgKM->tid, msgKM->value1,
						msgKM->identifier);
		break;
	case FPSGO_GET_CAM_APK_PID:
	case FPSGO_GET_CAM_SERVER_PID:
		if (fpsgo_get_pid_fp)
			fpsgo_get_pid_fp(msgKM->value1, &msgKM->pid1,
				msgKM->pid1, msgKM->value2);
		perfctl_copy_to_user(msgUM, msgKM,
			sizeof(struct _FPSGO_PACKAGE));
		break;
#else
	case FPSGO_TOUCH:
		 [[fallthrough]];
	case FPSGO_QUEUE:
		 [[fallthrough]];
	case FPSGO_DEQUEUE:
		 [[fallthrough]];
	case FPSGO_QUEUE_CONNECT:
		 [[fallthrough]];
	case FPSGO_VSYNC:
		 [[fallthrough]];
	case FPSGO_VSYNC_PERIOD:
		 [[fallthrough]];
	case FPSGO_BQID:
		 [[fallthrough]];
	case FPSGO_SWAP_BUFFER:
		 [[fallthrough]];
	case FPSGO_GET_FPS:
		 [[fallthrough]];
	case FPSGO_GET_CMD:
		 [[fallthrough]];
	case FPSGO_GET_FSTB_ACTIVE:
		[[fallthrough]];
	case FPSGO_WAIT_FSTB_ACTIVE:
		[[fallthrough]];
	case FPSGO_BUFFER_QUOTA:
		[[fallthrough]];
	case FPSGO_SBE_RESCUE:
		[[fallthrough]];
	case FPSGO_SBE_BUFFER_COUNT:
		[[fallthrough]];
	case FPSGO_ACQUIRE:
		break;
	case FPSGO_GET_CAM_APK_PID:
		break;
	case FPSGO_GET_CAM_SERVER_PID:
		break;
#endif

	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static const struct proc_ops Fops = {
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = device_ioctl,
#endif
	.proc_ioctl = device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------INIT------------------------*/
static void __exit exit_perfctl(void) {}
//static int __init init_perfctl(struct proc_dir_entry *parent)
static int __init init_perfctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;


	pr_debug(TAG"Start to init perf_ioctl driver\n");

	parent = proc_mkdir("perfmgr", NULL);
	perfmgr_root = parent;

	pe = proc_create("perf_ioctl", 0664, parent, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pe = proc_create("xgff_ioctl", 0664, parent, &xgff_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pe = proc_create("fpsgo_lr_ioctl", 0664, parent, &fpsgo_lr_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pr_debug(TAG"init perf_ioctl driver done\n");

	return 0;

out_wq:
	return ret_val;
}

module_init(init_perfctl);
module_exit(exit_perfctl);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek FPSGO perf_ioctl");
MODULE_AUTHOR("MediaTek Inc.");
