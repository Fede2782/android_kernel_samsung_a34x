// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][fuse]" fmt

#include "blocktag-fuse-trace.h"
#include <linux/tracepoint.h>
#include <linux/smp.h>
#include <fuse_i.h>

static struct {
	const char *name;
} fuse_ops[] = {
	[FUSE_LOOKUP]		= { "LOOKUP" },
	[FUSE_FORGET]		= { "FORGET" },
	[FUSE_GETATTR]		= { "GETATTR" },
	[FUSE_SETATTR]		= { "SETATTR" },
	[FUSE_READLINK]		= { "READLINK" },
	[FUSE_SYMLINK]		= { "SYMLINK" },
	[FUSE_MKNOD]		= { "MKNOD" },
	[FUSE_MKDIR]		= { "MKDIR" },
	[FUSE_UNLINK]		= { "UNLINK" },
	[FUSE_RMDIR]		= { "RMDIR" },
	[FUSE_RENAME]		= { "RENAME" },
	[FUSE_LINK]		= { "LINK" },
	[FUSE_OPEN]		= { "OPEN" },
	[FUSE_READ]		= { "READ" },
	[FUSE_WRITE]		= { "WRITE" },
	[FUSE_STATFS]		= { "STATFS" },
	[FUSE_RELEASE]		= { "RELEASE" },
	[FUSE_FSYNC]		= { "FSYNC" },
	[FUSE_SETXATTR]		= { "SETXATTR" },
	[FUSE_GETXATTR]		= { "GETXATTR" },
	[FUSE_LISTXATTR]	= { "LISTXATTR" },
	[FUSE_REMOVEXATTR]	= { "REMOVEXATTR" },
	[FUSE_FLUSH]		= { "FLUSH" },
	[FUSE_INIT]		= { "INIT" },
	[FUSE_OPENDIR]		= { "OPENDIR" },
	[FUSE_READDIR]		= { "READDIR" },
	[FUSE_RELEASEDIR]	= { "RELEASEDIR" },
	[FUSE_FSYNCDIR]		= { "FSYNCDIR" },
	[FUSE_GETLK]		= { "GETLK" },
	[FUSE_SETLK]		= { "SETLK" },
	[FUSE_SETLKW]		= { "SETLKW" },
	[FUSE_ACCESS]		= { "ACCESS" },
	[FUSE_CREATE]		= { "CREATE" },
	[FUSE_INTERRUPT]	= { "INTERRUPT" },
	[FUSE_BMAP]		= { "BMAP" },
	[FUSE_DESTROY]		= { "DESTROY" },
	[FUSE_IOCTL]		= { "IOCTL" },
	[FUSE_POLL]		= { "POLL" },
	[FUSE_NOTIFY_REPLY]	= { "NOTIFY_REPLY" },
	[FUSE_BATCH_FORGET]	= { "BATCH_FORGET" },
	[FUSE_FALLOCATE]	= { "FALLOCATE" },
	[FUSE_READDIRPLUS]	= { "READDIRPLUS" },
	[FUSE_RENAME2]		= { "RENAME2" },
	[FUSE_LSEEK]		= { "LSEEK" },
	[FUSE_COPY_FILE_RANGE]	= { "COPY_FILE_RANGE" },
	[FUSE_SETUPMAPPING]	= { "SETUPMAPPING" },
	[FUSE_REMOVEMAPPING]	= { "REMOVEMAPPING" },
	[FUSE_SYNCFS]		= { "SYNCFS" },
	[FUSE_TMPFILE]		= { "TMPFILE" },
	[FUSE_STATX]		= { "STATX" },
	[FUSE_CANONICAL_PATH]	= { "CANONICAL_PATH" },
	[CUSE_INIT]		= { "CUSE_INIT" },
};

#define FUSE_MAXOP ARRAY_SIZE(fuse_ops)

static const char *opname(enum fuse_opcode opcode)
{
	if (opcode >= FUSE_MAXOP || !fuse_ops[opcode].name)
		return NULL;
	else
		return fuse_ops[opcode].name;
}

static const char *filtername(int filter)
{
	switch (filter) {
	case 0:
		return "NONE";
	case FUSE_PREFILTER:
		return "FUSE_PREFILTER";
	case FUSE_POSTFILTER:
		return "FUSE_POSTFILTER";
	case FUSE_PREFILTER | FUSE_POSTFILTER:
		return "FUSE_PREFILTER | FUSE_POSTFILTER";
	default:
		return NULL;
	}
}

static const char *eventname(int event)
{
	switch (event) {
	case BTAG_FUSE_REQ:
		return "FUSE_REQ";
	default:
		return NULL;
	}
}

/*
 * Fuse Periodic statistics
 */
#define PERIODIC_STAT_MS 100
static struct fuse_periodic_stat pstat;
struct hrtimer pstat_timer;

static void fuse_pstat_inc(void)
{
	guard(spinlock_irqsave)(&pstat.lock);

	pstat.accumulator++;
	if (!hrtimer_active(&pstat_timer))
		hrtimer_start(&pstat_timer,
			      ms_to_ktime(PERIODIC_STAT_MS),
			      HRTIMER_MODE_REL);
}

static void clean_up_pstat(void)
{
	guard(spinlock_irqsave)(&pstat.lock);
	memset(&pstat, 0, sizeof(pstat));
}

u64 btag_fuse_pstat_get_last(void)
{
	guard(spinlock_irqsave)(&pstat.lock);
	return pstat.last_cnt;
}

u64 btag_fuse_pstat_get_max(void)
{
	guard(spinlock_irqsave)(&pstat.lock);
	return pstat.max_cnt;
}

u64 btag_fuse_pstat_get_distribution(u32 idx)
{
	if (idx >= ARRAY_SIZE(pstat.distribution))
		return 0;

	guard(spinlock_irqsave)(&pstat.lock);
	return pstat.distribution[idx];
}

static enum hrtimer_restart pstat_timer_fn(struct hrtimer *timer)
{
	scoped_guard(spinlock_irqsave, &pstat.lock) {
		pstat.last_cnt = pstat.accumulator;
		pstat.distribution[fls(pstat.last_cnt)]++;
		if (pstat.accumulator) {
			pstat.accumulator = 0;
			if (pstat.last_cnt > pstat.max_cnt)
				pstat.max_cnt = pstat.last_cnt;
		} else {
			return HRTIMER_NORESTART;
		}
	}

	hrtimer_forward_now(timer, ms_to_ktime(PERIODIC_STAT_MS));
	return HRTIMER_RESTART;
}

/*
 * Fuse tracer core
 */
#define TEMP_PID_CNT 32768
struct pid_fuse_stat_entry {
	unsigned short req_cnt;
	unsigned short tgid;
};
static struct btag_fuse_req_hist fuse_log;
static struct btag_fuse_req_stat stat[FUSE_MAXOP];
static u64 total_req_cnt;
static struct pid_fuse_stat_entry *pid_fuse_stats;
static unsigned short fuse_req_max_cnt;
static unsigned short fuse_req_max_cnt_pid;
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
static u64 total_top_cnt, prev_total_top_cnt;
static u64 total_top_unlink_cnt, prev_total_top_unlink_cnt;
#else
static u64 prev_total_req_cnt;
static u64 total_unlink_cnt, prev_total_unlink_cnt;
#endif
static DEFINE_SPINLOCK(stat_lock);

static void btag_fuse_queue_request_and_unlock(void *data,
		struct wait_queue_head *wq, bool sync)
{
	struct fuse_iqueue *fiq = container_of(wq, struct fuse_iqueue, waitq);
	struct fuse_req *rq = (struct fuse_req *)fiq->pending.prev;
	u32 opcode = rq->in.h.opcode & FUSE_OPCODE_FILTER;
	u32 filter = rq->in.h.opcode & ~FUSE_OPCODE_FILTER;
	struct btag_fuse_entry *e;
	unsigned long flags;
	unsigned int idx;
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	struct cgroup *grp;
#endif

	spin_lock_irqsave(&fuse_log.lock, flags);
	if (fuse_log.enable) {
		idx = fuse_log.next++;
		if (fuse_log.next == MAX_FUSE_REQ_HIST_CNT)
			fuse_log.next = 0;
		e = fuse_log.req_hist + idx;
		e->flags = rq->flags;
		e->time = local_clock();
		e->unique = rq->in.h.unique;
		e->nodeid = rq->in.h.nodeid;
		e->uid = rq->in.h.uid;
		e->gid = rq->in.h.gid;
		e->pid = rq->in.h.pid;
		e->opcode = opcode;
		e->filter = filter;
		e->event = BTAG_FUSE_REQ;
		e->cpu = smp_processor_id();
	}
	spin_unlock_irqrestore(&fuse_log.lock, flags);

	fuse_pstat_inc();

	if (opcode) {
		spin_lock_irqsave(&stat_lock, flags);
		stat[opcode].count++;
		if (filter & FUSE_PREFILTER)
			stat[opcode].prefilter++;
		if (filter & FUSE_POSTFILTER)
			stat[opcode].postfilter++;
		total_req_cnt++;

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
		total_top_cnt++;
		rcu_read_lock();
		grp = task_cgroup(current, cpuset_cgrp_id);
		rcu_read_unlock();
		if (opcode == FUSE_UNLINK) {
			if (grp->kn->name && !strcmp("top-app", grp->kn->name))
				total_top_unlink_cnt++;
		}

#else
		if (opcode == FUSE_UNLINK)
			total_unlink_cnt++;
#endif

		pid_fuse_stats[current->pid].req_cnt++;
		pid_fuse_stats[current->pid].tgid = current->tgid;
		if (pid_fuse_stats[current->pid].req_cnt > fuse_req_max_cnt) {
			fuse_req_max_cnt = pid_fuse_stats[current->pid].req_cnt;
			fuse_req_max_cnt_pid = current->pid;
		}

		spin_unlock_irqrestore(&stat_lock, flags);

		mtk_btag_earaio_update_pwd(BTAG_IO_FUSE, 0);
	}

	if (!opname(opcode))
		pr_err("unknown opcode, rq->in.h.opcode=%u\n", rq->in.h.opcode);

	if (!filtername(filter))
		pr_err("unknown filter, rq->in.h.opcode=%u\n", rq->in.h.opcode);
}

void mtk_btag_fuse_req_hist_show(char **buff, unsigned long *size,
				 struct seq_file *seq)
{
	struct btag_fuse_entry *e;
	struct timespec64 time;
	unsigned long flags;
	int idx;

	spin_lock_irqsave(&fuse_log.lock, flags);

	if (!fuse_log.enable) {
		BTAG_PRINTF(NULL, NULL, seq, "req_hist is disabled\n");
		goto unlock;
	}

	idx = fuse_log.next;

	while (true) {
		idx = idx ? idx - 1 : MAX_FUSE_REQ_HIST_CNT - 1;

		if (idx == fuse_log.next)
			break;

		e = fuse_log.req_hist + idx;
		if (!e->time)
			continue;

		time = ns_to_timespec64(e->time);
		BTAG_PRINTF(buff, size, seq,
			    "%llu.%09lu,%s(%u),flags=0x%03lx,unique=%llu,nodeid=0x%012llx,uid=%u,gid=%u,pid=%u,bpf=%s,opcode=%u(%s)\n",
			    time.tv_sec, time.tv_nsec,
			    eventname(e->event) ?: "???",
			    e->cpu,
			    e->flags,
			    e->unique,
			    e->nodeid,
			    e->uid,
			    e->gid,
			    e->pid,
			    filtername(e->filter) ?: "???",
			    e->opcode,
			    opname(e->opcode) ?: "???");
	}
unlock:
	spin_unlock_irqrestore(&fuse_log.lock, flags);
}

static void disable_req_hist(void)
{
	unsigned long flags;

	spin_lock_irqsave(&fuse_log.lock, flags);
	if (fuse_log.enable) {
		memset(fuse_log.req_hist, 0, sizeof(fuse_log.req_hist));
		fuse_log.next = 0;
		fuse_log.enable = 0;
	}
	spin_unlock_irqrestore(&fuse_log.lock, flags);
}

static void enable_req_hist(void)
{
	unsigned long flags;

	spin_lock_irqsave(&fuse_log.lock, flags);
	if (!fuse_log.enable)
		fuse_log.enable = 1;
	spin_unlock_irqrestore(&fuse_log.lock, flags);
}

static void clean_up_stat(void)
{
	unsigned long flags;

	spin_lock_irqsave(&stat_lock, flags);
	memset(stat, 0, sizeof(stat));
	total_req_cnt = 0;
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	total_top_unlink_cnt = 0;
	prev_total_top_unlink_cnt = 0;
#else
	total_unlink_cnt = 0;
	prev_total_unlink_cnt = 0;
#endif
	spin_unlock_irqrestore(&stat_lock, flags);
}

/*
 * Fuse Tracepoint Register & Unregister
 */
#define FOR_EACH_INTEREST(i) for (i = 0; i < ARRAY_SIZE(interests); i++)

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	{
		.name = "android_vh_queue_request_and_unlock",
		.func = btag_fuse_queue_request_and_unlock
	},
};

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
			interests[i].init = NULL;
		}
	}
}

static int install_tracepoints(void)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("tracepoints %s not found\n",
				interests[i].name);
			/* Unload previously loaded */
			uninstall_tracepoints();
			return -EINVAL;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}

/*
 * Fuse Proc FS Operations
 */
static struct proc_dir_entry *fuse_root;
static struct proc_dir_entry *req_hist_e;
static struct proc_dir_entry *stat_e;
static struct proc_dir_entry *pstat_e;
static struct proc_dir_entry *control_e;

static int req_hist_proc_show(struct seq_file *m, void *v)
{
	mtk_btag_fuse_req_hist_show(NULL, NULL, m);
	return 0;
}

static int req_hist_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, req_hist_proc_show, inode->i_private);
}

static const struct proc_ops btag_fuse_req_hist_fops = {
	.proc_open		= req_hist_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int stat_proc_show(struct seq_file *seq, void *v)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&stat_lock, flags);
	BTAG_PRINTF(NULL, NULL, seq,
		    "Total Request Count: %llu\n", total_req_cnt);
	spin_unlock_irqrestore(&stat_lock, flags);

	BTAG_PRINTF(NULL, NULL, seq,
		    "opcode,prefilter,postfilter,count,name\n");

	for (i = 0; i < FUSE_MAXOP; i++) {
		if (!opname(i))
			continue;

		spin_lock_irqsave(&stat_lock, flags);
		BTAG_PRINTF(NULL, NULL, seq, "%d,%llu,%llu,%llu,%s\n",
			    i, stat[i].prefilter, stat[i].postfilter,
			    stat[i].count, opname(i));
		spin_unlock_irqrestore(&stat_lock, flags);
	}

	return 0;
}

static int stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, stat_proc_show, inode->i_private);
}

static const struct proc_ops btag_fuse_stat_fops = {
	.proc_open		= stat_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int pstat_proc_show(struct seq_file *seq, void *v)
{
	int i;

	BTAG_PRINTF(NULL, NULL, seq, "last: %llu\n",
			btag_fuse_pstat_get_last());
	BTAG_PRINTF(NULL, NULL, seq, "max: %llu\n",
			btag_fuse_pstat_get_max());

	BTAG_PRINTF(NULL, NULL, seq, "[0, 0]: %llu\n",
			btag_fuse_pstat_get_distribution(0));

	for (i = 1; i < ARRAY_SIZE(pstat.distribution); i++)
		BTAG_PRINTF(NULL, NULL, seq, "[2^%d, 2^%d): %llu\n", i - 1, i,
				btag_fuse_pstat_get_distribution(i));

	return 0;
}

static int pstat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pstat_proc_show, inode->i_private);
}

static const struct proc_ops btag_fuse_pstat_fops = {
	.proc_open		= pstat_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int control_proc_show(struct seq_file *seq, void *v)
{
	unsigned long flags;
	u8 hist_enable;

	spin_lock_irqsave(&fuse_log.lock, flags);
	hist_enable = fuse_log.enable;
	spin_unlock_irqrestore(&fuse_log.lock, flags);

	BTAG_PRINTF(NULL, NULL, seq, "req_hist status: %s\n",
		    hist_enable ? "enable" : "disable");
	BTAG_PRINTF(NULL, NULL, seq, "=== control command ===\n");
	BTAG_PRINTF(NULL, NULL, seq, "echo n > /proc/blocktag/fuse/control\n");
	BTAG_PRINTF(NULL, NULL, seq, "0: disable req_hist\n");
	BTAG_PRINTF(NULL, NULL, seq, "1: enable req_hist\n");
	BTAG_PRINTF(NULL, NULL, seq, "2: clear stat\n");
	BTAG_PRINTF(NULL, NULL, seq, "3: clear periodic_stat\n");

	return 0;
}

static int control_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, control_proc_show, inode->i_private);
}

static ssize_t control_proc_write(struct file *file, const char *buf,
				  size_t count, loff_t *data)
{
	unsigned int val;
	int err = kstrtouint_from_user(buf, count, 0, &val);

	if (err)
		return err;

	switch (val) {
	case 0:
		disable_req_hist();
		break;
	case 1:
		enable_req_hist();
		break;
	case 2:
		clean_up_stat();
		break;
	case 3:
		clean_up_pstat();
		break;
	default:
		pr_err("unknown command %u\n", val);
		return -EINVAL;
	}

	return count;
}

static const struct proc_ops btag_fuse_control_fops = {
	.proc_open		= control_proc_open,
	.proc_write		= control_proc_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*
 * MTK Blocktag Fuse Trace Init & Exit
 */
void mtk_btag_fuse_init(struct proc_dir_entry *btag_root)
{
	int ret;

	if (!btag_root) {
		pr_err("Invalid btag_root\n");
		return;
	}

	spin_lock_init(&fuse_log.lock);
	spin_lock_init(&pstat.lock);
#ifdef MTK_BLOCK_IO_DEBUG_BUILD
	fuse_log.enable = 1;
#endif

	ret = install_tracepoints();
	if (ret) {
		pr_err("install tracepoints failed %d\n", ret);
		return;
	}

	hrtimer_init(&pstat_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pstat_timer.function = pstat_timer_fn;
	hrtimer_start(&pstat_timer, ms_to_ktime(PERIODIC_STAT_MS),
			HRTIMER_MODE_REL);

	/* proc_create for fuse/req_hist, fuse/stat, fuse/control */
	fuse_root = proc_mkdir("fuse", btag_root);
	if (IS_ERR(fuse_root)) {
		ret = PTR_ERR(fuse_root);
		pr_err("proc create failed for fuse (%d)\n", ret);
		goto uninstall_tp;
	}
	req_hist_e = proc_create("req_hist", S_IFREG | 0444, fuse_root,
				 &btag_fuse_req_hist_fops);
	if (IS_ERR(req_hist_e)) {
		ret = PTR_ERR(req_hist_e);
		pr_err("proc create failed for fuse/req_hist (%d)\n", ret);
		goto free_fuse_root;
	}
	stat_e = proc_create("stat", S_IFREG | 0444, fuse_root,
				 &btag_fuse_stat_fops);
	if (IS_ERR(stat_e)) {
		ret = PTR_ERR(stat_e);
		pr_err("proc create failed for fuse/stat (%d)\n", ret);
		goto free_req_hist_e;
	}
	pstat_e = proc_create("periodic_stat", S_IFREG | 0444, fuse_root,
				 &btag_fuse_pstat_fops);
	if (IS_ERR(pstat_e)) {
		ret = PTR_ERR(pstat_e);
		pr_err("proc create failed for fuse/periodic_stat (%d)\n", ret);
		goto free_stat_e;
	}
	control_e = proc_create("control", S_IFREG | 0444, fuse_root,
				 &btag_fuse_control_fops);
	if (IS_ERR(control_e)) {
		ret = PTR_ERR(control_e);
		pr_err("proc create failed for fuse/control (%d)\n", ret);
		goto free_pstat_e;
	}

	pid_fuse_stats = kzalloc(sizeof(struct pid_fuse_stat_entry)*TEMP_PID_CNT, GFP_KERNEL);

	return;

free_pstat_e:
	proc_remove(pstat_e);
free_stat_e:
	proc_remove(stat_e);
free_req_hist_e:
	proc_remove(req_hist_e);
free_fuse_root:
	proc_remove(fuse_root);
uninstall_tp:
	uninstall_tracepoints();
	hrtimer_cancel(&pstat_timer);
}

void mtk_btag_fuse_exit(void)
{
	hrtimer_cancel(&pstat_timer);
	proc_remove(control_e);
	proc_remove(stat_e);
	proc_remove(req_hist_e);
	proc_remove(fuse_root);
	uninstall_tracepoints();
}

void mtk_btag_fuse_get_req_cnt(int *total_cnt, int *unlink_cnt)
{
	unsigned long flags;

	spin_lock_irqsave(&stat_lock, flags);
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	*total_cnt = (int)(total_top_cnt - prev_total_top_cnt);
	*unlink_cnt = (int)(total_top_unlink_cnt - prev_total_top_unlink_cnt);
#else
	*total_cnt = (int)(total_req_cnt - prev_total_req_cnt);
	*unlink_cnt = (int)(total_unlink_cnt - prev_total_unlink_cnt);
#endif
	spin_unlock_irqrestore(&stat_lock, flags);
}

void mtk_btag_eara_get_fuse_data(struct eara_iostat *data)
{
	unsigned long flags;

	spin_lock_irqsave(&stat_lock, flags);
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	data->fuse_req_cnt = (int)(total_top_cnt - prev_total_top_cnt);
	data->fuse_unlink_cnt = (int)(total_top_unlink_cnt - prev_total_top_unlink_cnt);
#else
	data->fuse_req_cnt = (int)(total_req_cnt - prev_total_req_cnt);
	data->fuse_unlink_cnt = (int)(total_unlink_cnt - prev_total_unlink_cnt);
#endif

	data->hot_pid = fuse_req_max_cnt_pid;
	data->hot_tgid = pid_fuse_stats[fuse_req_max_cnt_pid].tgid;
	memset(pid_fuse_stats, 0, sizeof(struct pid_fuse_stat_entry)*TEMP_PID_CNT);
	fuse_req_max_cnt = 0;
	fuse_req_max_cnt_pid = 0;

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	prev_total_top_cnt = total_top_cnt;
	prev_total_top_unlink_cnt = total_top_unlink_cnt;
#else
	prev_total_req_cnt = total_req_cnt;
	prev_total_unlink_cnt = total_unlink_cnt;
#endif
	spin_unlock_irqrestore(&stat_lock, flags);
}

void mtk_btag_fuse_clear_req_cnt(void)
{
	unsigned long flags;

	spin_lock_irqsave(&stat_lock, flags);
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	prev_total_top_cnt = total_top_cnt;
#else
	prev_total_req_cnt = total_req_cnt;
#endif
	spin_unlock_irqrestore(&stat_lock, flags);
}
