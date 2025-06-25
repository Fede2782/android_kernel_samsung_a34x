// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mailbox_controller.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>
#include <linux/proc_fs.h>
#include "linux/seq_file.h"
#include "linux/module.h"
#include <linux/of_reserved_mem.h>
#include <mt-plat/mrdump.h>

#include "cmdq-util.h"

#ifdef CMDQ_SECURE_SUPPORT
#include "cmdq-sec-mailbox.h"
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
#include <soc/mediatek/smi.h>
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <linux/soc/mediatek/devapc_public.h>
#endif

#define CMDQ_MBOX_NUM			2
#define CMDQ_HW_MAX			2
#define CMDQ_RECORD_NUM			512
#define CMDQ_BUF_RECORD_NUM		60
#define CMDQ_USER_BUF_RECORD_NUM	128
#define CMDQ_IRQ_HISTORY_MAX_SIZE		5000

#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_CURR_LOADED_THR		0x18
#define CMDQ_THR_EXEC_CYCLES		0x34
#define CMDQ_THR_TIMEOUT_TIMER		0x38

#define GCE_DBG_CTL			0x3000
#define GCE_DBG0			0x3004
#define GCE_DBG2			0x300C
#define GCE_DBG3			0x3010

#define CMDQ_BUF_REC_BUFFER_SIZE		(PAGE_SIZE * 11)
#define log_length		128

#define util_time_to_us(start, end, duration)	\
{	\
	u64 _duration = end - start;	\
	do_div(_duration, 1000);	\
	duration = (s32)_duration;	\
}

struct cmdq_util_error {
	spinlock_t	lock;
	atomic_t	enable;
	char		*buffer;
	u32		length;
	u64		nsec;
	bool		record;
	char		caller[TASK_COMM_LEN]; // TODO
};

struct cmdq_util_dentry {
	struct dentry	*status;
	struct dentry	*record;
	struct dentry	*log_feature;
	u8		bit_feature;
};

struct cmdq_record {
	unsigned long pkt;
	s32 priority;	/* task priority (not thread priority) */
	s8 id;
	s32 thread;	/* allocated thread */
	s32 reorder;
	u32 size;
	bool is_secure;	/* true for secure task */

	u64 submit;	/* epoch time of IOCTL/Kernel API call */
	u64 trigger;	/* epoch time of enable HW thread */
	/* epoch time of start waiting for task completion */
	u64 wait;
	u64 irq;	/* epoch time of IRQ event */
	u64 done;	/* epoch time of sw leaving wait and task finish */

	u64 last_inst;	/* last instruction, jump addr */

	u32 exec_begin;	/* task execute time in hardware thread */
	u32 exec_end;	/* task execute time in hardware thread */
};

struct cmdq_buf_record {
	u32	buf_peek_cnt[CMDQ_HW_MAX][CMDQ_THR_MAX_COUNT];
	u64	nsec;
};

struct cmdq_hw_trace {
	bool enable;
	bool update;
	bool built_in;
	struct cmdq_client *clt;
	struct cmdq_pkt *pkt;
	struct cmdq_pkt_buffer *buf;
};

struct cmdq_user_buf_record {
	u8	hwid;
	u8	thrd_idx;
	u8	is_alloc;
	u64	nsec;
	dma_addr_t pa;
};

struct cmdq_util {
	struct cmdq_util_error	err[CMDQ_HW_MAX];
	struct cmdq_util_dentry	fs;
	struct cmdq_record record[CMDQ_RECORD_NUM];
	u16 record_idx;
	void *cmdq_mbox[CMDQ_MBOX_NUM];
	void *cmdq_sec_mbox[CMDQ_MBOX_NUM];
	u32 mbox_cnt;
	u32 mbox_sec_cnt;
	const char *first_err_mod[CMDQ_HW_MAX];
	struct cmdq_client *prebuilt_clt[CMDQ_HW_MAX];
	struct cmdq_hw_trace hw_trace[CMDQ_HW_MAX];
	u16 buf_record_idx;
	u16 user_buf_record_idx;
	struct cmdq_buf_record buf_record[CMDQ_BUF_RECORD_NUM];
	struct cmdq_user_buf_record ussr_buf_record[CMDQ_USER_BUF_RECORD_NUM];
	u8	cmdq_irq_thrd_history[CMDQ_HW_MAX][CMDQ_IRQ_HISTORY_MAX_SIZE];
	u16	cmdq_irq_thrd_history_idx[CMDQ_HW_MAX];
	char	*buf_rec_buffer;
};
static struct cmdq_util	util;

static DEFINE_MUTEX(cmdq_record_mutex);
static DEFINE_MUTEX(cmdq_dump_mutex);
static DEFINE_MUTEX(cmdq_buf_record_mutex);
struct cmdq_util_controller_fp controller_fp = {
	.track_ctrl = cmdq_util_track_ctrl,
};
struct cmdq_util_helper_fp helper_fp = {
	.is_feature_en = cmdq_util_is_feature_en,
	.dump_lock = cmdq_util_dump_lock,
	.dump_unlock = cmdq_util_dump_unlock,
	.error_enable = cmdq_util_error_enable,
	.error_disable = cmdq_util_error_disable,
	.dump_smi = cmdq_util_dump_smi,
	.set_first_err_mod = cmdq_util_set_first_err_mod,
	.track = cmdq_util_track,
};

/**
 * shared memory between normal and secure world
 */
struct cmdq_sec_shared_mem {
	void		*va;
	dma_addr_t	pa;
	dma_addr_t	mva;
	u32		size;
};
static struct cmdq_sec_shared_mem *shared_mem;

void cmdq_thrd_irq_history_record(u8 hwid ,u8 thread_idx)
{
	u16 arr_idx;

	if(thread_idx >= CMDQ_THR_MAX_COUNT || hwid >= CMDQ_HW_MAX)
		return;

	arr_idx = util.cmdq_irq_thrd_history_idx[hwid]++;
	if(util.cmdq_irq_thrd_history_idx[hwid] >= CMDQ_IRQ_HISTORY_MAX_SIZE)
		util.cmdq_irq_thrd_history_idx[hwid] = 0;

	util.cmdq_irq_thrd_history[hwid][arr_idx] = thread_idx;
}

void cmdq_dump_thrd_irq_history(u8 hwid)
{
#define txt_sz 128
	u16 arr_idx, thrd_irq_cnt[CMDQ_THR_MAX_COUNT] = {0}, offset;
	u8 thrd_idx, i;
	s16 len;
	char text[txt_sz];

	memset(text, 0, sizeof(text));
	for(arr_idx = 0; arr_idx < CMDQ_IRQ_HISTORY_MAX_SIZE; arr_idx++) {
		thrd_idx = util.cmdq_irq_thrd_history[hwid][arr_idx];
		if(thrd_idx < CMDQ_THR_MAX_COUNT)
			thrd_irq_cnt[thrd_idx]++;
	}

	offset = 0;
	for (i = 0; i < CMDQ_THR_MAX_COUNT; ++i) {
		len = snprintf(text + offset, sizeof(text) - offset, "%d ", thrd_irq_cnt[i]);
		if (len < 0) {
			cmdq_err("snprintf failed len:%d", len);
			return;
		}
		offset += len;
		if (offset >= sizeof(text)) {
			cmdq_err("text size is full offset:%d", offset);
			break;
		}
	}
	cmdq_msg("%s thrd irq cnt: %s", __func__, text);
}
EXPORT_SYMBOL(cmdq_dump_thrd_irq_history);

cmdq_mminfra_power mminfra_power_cb;
EXPORT_SYMBOL(mminfra_power_cb);

void cmdq_get_mminfra_cb(cmdq_mminfra_power cb)
{
	mminfra_power_cb = cb;
}
EXPORT_SYMBOL(cmdq_get_mminfra_cb);

cmdq_mminfra_gce_cg mminfra_gce_cg;
EXPORT_SYMBOL(mminfra_gce_cg);

void cmdq_get_mminfra_gce_cg_cb(cmdq_mminfra_gce_cg cb)
{
	mminfra_gce_cg = cb;
}
EXPORT_SYMBOL(cmdq_get_mminfra_gce_cg_cb);

struct cmdq_util_platform_fp *cmdq_platform;

void cmdq_util_set_fp(struct cmdq_util_platform_fp *cust_cmdq_platform)
{
	s32 i;
	if (!cust_cmdq_platform) {
		cmdq_err("%s cmdq_util_platform_fp is NULL ", __func__);
		return;
	}
	cmdq_platform = cust_cmdq_platform;
	controller_fp.thread_ddr_module = cmdq_platform->thread_ddr_module;
	helper_fp.hw_name = cmdq_platform->util_hw_name;
	helper_fp.event_module_dispatch = cmdq_platform->event_module_dispatch;
	helper_fp.thread_module_dispatch = cmdq_platform->thread_module_dispatch;
	helper_fp.hw_trace_thread = cmdq_platform->hw_trace_thread;
	helper_fp.dump_error_irq_debug = cmdq_platform->dump_error_irq_debug;
	controller_fp.check_tf = cmdq_platform->check_tf;
	for (i = 0; i < util.mbox_cnt; i++)
		cmdq_mbox_set_hw_id(util.cmdq_mbox[i]);
}
EXPORT_SYMBOL(cmdq_util_set_fp);

void cmdq_util_reset_fp(struct cmdq_util_platform_fp *cust_cmdq_platform)
{
	s32 i;

	if (!cust_cmdq_platform) {
		cmdq_err("%s cmdq_util_platform_fp is NULL ", __func__);
		return;
	}
	controller_fp.thread_ddr_module = NULL;
	controller_fp.check_tf = NULL;
	helper_fp.hw_name = NULL;
	helper_fp.event_module_dispatch = NULL;
	helper_fp.thread_module_dispatch = NULL;
	helper_fp.hw_trace_thread = NULL;
	helper_fp.dump_error_irq_debug = NULL;
	for (i = 0; i < util.mbox_cnt; i++)
		cmdq_mbox_reset_hw_id(util.cmdq_mbox[i]);
}
EXPORT_SYMBOL(cmdq_util_reset_fp);

bool cmdq_util_check_hw_trace_work(u8 hwid)
{

	if (hwid >= util.mbox_cnt) {
		cmdq_err("hwid:%d mbox_cnt:%u",
			hwid, util.mbox_cnt);
		return false;
	}

	return (&util.hw_trace[hwid])->clt ? true : false;
}
EXPORT_SYMBOL(cmdq_util_check_hw_trace_work);

const char *cmdq_util_event_module_dispatch(phys_addr_t gce_pa, const u16 event, s32 thread)
{
	const char *mod = NULL;

	if (cmdq_platform->event_module_dispatch)
		mod = cmdq_platform->event_module_dispatch(gce_pa, event, thread);
	else
		cmdq_err("%s event_module_dispatch is NULL ", __func__);
	return mod;
}
EXPORT_SYMBOL(cmdq_util_event_module_dispatch);

const char *cmdq_util_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	const char *mod = NULL;

	if (cmdq_platform->thread_module_dispatch)
		mod = cmdq_platform->thread_module_dispatch(gce_pa, thread);
	else
		cmdq_err("%s thread_module_dispatch is NULL ", __func__);
	return mod;
}
EXPORT_SYMBOL(cmdq_util_thread_module_dispatch);

u32 cmdq_util_get_hw_id(u32 pa)
{
	if (!cmdq_platform || !cmdq_platform->util_hw_id) {
		cmdq_msg("%s cmdq_platform->util_hw_id is NULL ", __func__);
		return -EINVAL;
	}
	return cmdq_platform->util_hw_id(pa);
}
EXPORT_SYMBOL(cmdq_util_get_hw_id);

u32 cmdq_util_get_mdp_min_thrd(void)
{
	if (!cmdq_platform || !cmdq_platform->get_mdp_min_thread) {
		cmdq_msg("%s cmdq_platform->get_mdp_min_thread is NULL ",
			__func__);
		return -EINVAL;
	}
	return cmdq_platform->get_mdp_min_thread();
}

u32 cmdq_util_test_get_subsys_list(u32 **regs_out)
{
	if (!cmdq_platform->test_get_subsys_list) {
		cmdq_err("%s test_get_subsys_list is NULL ", __func__);
		return -EINVAL;
	}
	return cmdq_platform->test_get_subsys_list(regs_out);
}
EXPORT_SYMBOL(cmdq_util_test_get_subsys_list);

void cmdq_util_test_set_ostd(void)
{
	if (!cmdq_platform->test_set_ostd) {
		cmdq_err("%s test_set_ostd is NULL ", __func__);
		return;
	}
	cmdq_platform->test_set_ostd();
}
EXPORT_SYMBOL(cmdq_util_test_set_ostd);

u32 cmdq_util_get_bit_feature(void)
{
	return util.fs.bit_feature;
}
EXPORT_SYMBOL(cmdq_util_get_bit_feature);

bool cmdq_util_is_feature_en(u8 feature)
{
	return (util.fs.bit_feature & BIT(feature)) != 0;
}
EXPORT_SYMBOL(cmdq_util_is_feature_en);

void cmdq_util_error_enable(u8 hwid)
{
	if (!util.err[hwid].nsec)
		util.err[hwid].nsec = sched_clock();
	atomic_inc(&util.err[hwid].enable);
}
EXPORT_SYMBOL(cmdq_util_error_enable);

void cmdq_util_error_disable(u8 hwid)
{
	s32 enable;

	enable = atomic_dec_return(&util.err[hwid].enable);
	if (enable < 0) {
		cmdq_err("enable:%d hwid:%d", enable, hwid);
		dump_stack();
	} else if (enable == 0 && !util.err[hwid].record && cmdq_print_debug) {
		char *buf_va, *buf_va_end;
		u64		sec = 0;
		unsigned long	nsec = 0;
		char title[] = "[cmdq] first error kernel time:";

		if (util.err[1 - hwid].record)
			buf_va = (char *)(shared_mem->va + CMDQ_RECORD_SIZE + util.err[1 -
				hwid].length + log_length * 2);
		else
			buf_va = (char *)(shared_mem->va + CMDQ_RECORD_SIZE + log_length);

		buf_va_end = (char *)(shared_mem->va + CMDQ_RECORD_SIZE + CMDQ_STATUS_SIZE);
		sec = util.err[hwid].nsec;
		nsec = do_div(sec, 1000000000);
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%s[%5llu.%06lu]\n", title, sec, nsec);
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%s\n", util.err[hwid].buffer);
		util.err[hwid].record = true;
	}
}
EXPORT_SYMBOL(cmdq_util_error_disable);

void cmdq_util_dump_lock(void)
{
	mutex_lock(&cmdq_dump_mutex);
}
EXPORT_SYMBOL(cmdq_util_dump_lock);

void cmdq_util_dump_unlock(void)
{
	mutex_unlock(&cmdq_dump_mutex);
}
EXPORT_SYMBOL(cmdq_util_dump_unlock);

s32 cmdq_util_error_save_lst(const char *format, va_list args, u8 hwid)
{
	unsigned long flags;
	s32 size;
	s32 enable;

	enable = atomic_read(&util.err[hwid].enable);
	if ((enable <= 0) || !util.err[hwid].buffer)
		return -EFAULT;

	spin_lock_irqsave(&util.err[hwid].lock, flags);
	size = vsnprintf(util.err[hwid].buffer + util.err[hwid].length,
		CMDQ_FIRST_ERR_SIZE - util.err[hwid].length, format, args);
	if (size >= CMDQ_FIRST_ERR_SIZE - util.err[hwid].length)
		cmdq_log("hwid:%d size:%d over buf size:%d",
			hwid, size, CMDQ_FIRST_ERR_SIZE - util.err[hwid].length);
	util.err[hwid].length += size;
	spin_unlock_irqrestore(&util.err[hwid].lock, flags);

	if (util.err[hwid].length >= CMDQ_FIRST_ERR_SIZE) {
		cmdq_util_error_disable(hwid);
		cmdq_err("hwid:%d util.err.length:%u is over CMDQ_FIRST_ERR_SIZE:%u",
			hwid, util.err[hwid].length, CMDQ_FIRST_ERR_SIZE);
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_util_error_save_lst);

s32 cmdq_util_error_save(const char *format, ...)
{
	va_list args;
	s32 enable;
	u8 i = 0;

	for (i = 0; i < CMDQ_HW_MAX; i++) {
		enable = atomic_read(&util.err[i].enable);
		if (enable < 0)
			return -EFAULT;
		else if (!enable)
			continue;
		else if (enable && util.err[i].buffer != 0) {
			va_start(args, format);
			cmdq_util_error_save_lst(format, args, i);
			va_end(args);
		}
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_util_error_save);

static int cmdq_util_status_print(struct seq_file *seq, void *data)
{
	u64		sec = 0;
	unsigned long	nsec = 0;
	u32		i;

	for (i = 0; i < CMDQ_HW_MAX; i++) {
		if (util.err[i].length) {
			sec = util.err[i].nsec;
			nsec = do_div(sec, 1000000000);
			seq_printf(seq,
				"[cmdq] hwid:%d first error kernel time:[%5llu.%06lu]\n",
				i, sec, nsec);
			seq_printf(seq, "%s", util.err[i].buffer);
		}
	}

	seq_puts(seq, "[cmdq] dump all thread current status\n");
	for (i = 0; i < util.mbox_cnt; i++)
		cmdq_thread_dump_all_seq(util.cmdq_mbox[i], seq);

	return 0;
}

static int cmdq_util_record_print(struct seq_file *seq, void *data)
{
	struct cmdq_record *rec;
	u32 acq_time, irq_time, begin_wait, exec_time, total_time, hw_time;
	u64 submit_sec;
	unsigned long submit_rem, hw_time_rem;
	s32 i, idx;

	mutex_lock(&cmdq_record_mutex);

	seq_puts(seq, "index,pkt,task priority,sec,size,gce,thread,");
	seq_puts(seq,
		"submit,acq_time(us),irq_time(us),begin_wait(us),exec_time(us),total_time(us),jump,");
	seq_puts(seq, "exec begin,exec end,hw_time(us),\n");

	idx = util.record_idx;
	for (i = 0; i < ARRAY_SIZE(util.record); i++) {
		idx--;
		if (idx < 0)
			idx = ARRAY_SIZE(util.record) - 1;

		rec = &util.record[idx];
		if (!rec->pkt)
			continue;

		seq_printf(seq, "%u,%#lx,%d,%d,%u,%hhd,%d,",
			idx, rec->pkt, rec->priority, (int)rec->is_secure,
			rec->size, rec->id, rec->thread);

		submit_sec = rec->submit;
		submit_rem = do_div(submit_sec, 1000000000);

		util_time_to_us(rec->submit, rec->trigger, acq_time);
		util_time_to_us(rec->trigger, rec->irq, irq_time);
		util_time_to_us(rec->submit, rec->wait, begin_wait);
		util_time_to_us(rec->trigger, rec->done, exec_time);
		util_time_to_us(rec->submit, rec->done, total_time);
		seq_printf(seq,
			"%llu.%06lu,%u,%u,%u,%u,%u,%#llx,",
			submit_sec, submit_rem / 1000, acq_time, irq_time,
			begin_wait, exec_time, total_time, rec->last_inst);

		hw_time = rec->exec_end > rec->exec_begin ?
			rec->exec_end - rec->exec_begin :
			~rec->exec_begin + 1 + rec->exec_end;
		hw_time_rem = (u32)CMDQ_TICK_TO_US(hw_time);

		seq_printf(seq, "%u,%u,%u.%06lu,\n",
			rec->exec_begin, rec->exec_end, hw_time, hw_time_rem);
	}

	mutex_unlock(&cmdq_record_mutex);

	return 0;
}

char *cmdq_util_user_buf_record_print(char *buf_start, char *buf_end)
{
	struct cmdq_user_buf_record *user_buf_rec;

	s32		arr_idx, idx;
	u64		sec = 0;
	unsigned long	nsec = 0;

	idx = util.user_buf_record_idx;
	for (arr_idx = 0; arr_idx < ARRAY_SIZE(util.ussr_buf_record); arr_idx++) {
		idx--;
		if (idx < 0)
			idx = ARRAY_SIZE(util.ussr_buf_record) - 1;
		user_buf_rec = &util.ussr_buf_record[idx];

		if(!user_buf_rec->nsec)
			continue;

		sec = user_buf_rec->nsec;
		nsec = do_div(sec, 1000000000);

		buf_start += scnprintf(buf_start, buf_end - buf_start,
			"[%5llu.%06lu] hwid:%d thrd:%d %s pa:%pa\n",
			sec, nsec, user_buf_rec->hwid, user_buf_rec->thrd_idx,
			user_buf_rec->is_alloc ? "alloc" : "free",
			&user_buf_rec->pa);
	}

	return buf_start;
}

void cmdq_util_buf_record_save(void)
{
	struct cmdq_buf_record *buf_rec;

	s32 i, j, arr_idx, idx;
	u64		sec = 0;
	unsigned long	nsec = 0;
	char end[] = "\n";
	char *buf_va, *buf_va_end;

	buf_va = (char *)(util.buf_rec_buffer);
	buf_va_end = (char *)(util.buf_rec_buffer + CMDQ_BUF_REC_BUFFER_SIZE);
	buf_va = cmdq_dump_buffer_size_seq(buf_va, buf_va_end);
	buf_va = cmdq_util_user_buf_record_print(buf_va, buf_va_end);
	idx = util.buf_record_idx;
	for (arr_idx = 0; arr_idx < ARRAY_SIZE(util.buf_record); arr_idx++) {
		idx--;
		if (idx < 0)
			idx = ARRAY_SIZE(util.buf_record) - 1;
		buf_rec = &util.buf_record[idx];

		if(!buf_rec->nsec)
			continue;

		sec = buf_rec->nsec;
		nsec = do_div(sec, 1000000000);
		for (i = 0; i < CMDQ_HW_MAX; i++) {
			buf_va += scnprintf(buf_va, buf_va_end - buf_va,
				"[%5llu.%06lu] hwid:%d ", sec, nsec, i);
			for (j = 0; j < CMDQ_THR_MAX_COUNT; j += 4) {
				buf_va += scnprintf(buf_va, buf_va_end - buf_va,
					"thr%d=[%u] thr%d=[%u] thr%d=[%u] thr%d=[%u]",
					j, buf_rec->buf_peek_cnt[i][j],
					j + 1, buf_rec->buf_peek_cnt[i][j + 1],
					j + 2, buf_rec->buf_peek_cnt[i][j + 2],
					j + 3, buf_rec->buf_peek_cnt[i][j + 3]);
			}
			buf_va += scnprintf(buf_va, buf_va_end - buf_va,
				"%s", end);
		}
	}
}

void cmdq_util_buf_record_aee_dump(unsigned long *vaddr, unsigned long *size)
{
	cmdq_util_buf_record_save();

	if (cmdq_print_debug) {
		char *buf_va, *buf_va_end;

		buf_va = (char *)(shared_mem->va + CMDQ_RECORD_SIZE + util.err[0].length + util.err[1].length);
		buf_va_end = (char *)(shared_mem->va + CMDQ_RECORD_SIZE + CMDQ_STATUS_SIZE);
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%s\n", util.buf_rec_buffer);
	}
}

static int cmdq_util_buf_record_print(struct seq_file *seq, void *data)
{
	mutex_lock(&cmdq_buf_record_mutex);
	cmdq_util_buf_record_save();
	seq_printf(seq, "%s", util.buf_rec_buffer);
	mutex_unlock(&cmdq_buf_record_mutex);
	return 0;
}

static int cmdq_util_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_status_print, inode->i_private);
}

static int cmdq_util_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_record_print, inode->i_private);
}

static const struct proc_ops cmdq_util_status_fops = {
	.proc_open = cmdq_util_status_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int cmdq_util_buffer_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_buf_record_print, inode->i_private);
}

static const struct proc_ops cmdq_util_record_fops = {
	.proc_open = cmdq_util_record_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops cmdq_proc_util_buf_record_fops = {
	.proc_open = cmdq_util_buffer_record_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int cmdq_util_log_feature_get(void *data, u64 *val)
{
	cmdq_msg("data:%p val:%#llx bit_feature:%#x",
		data, *val, util.fs.bit_feature);
	return util.fs.bit_feature;
}

int cmdq_util_log_feature_set(void *data, u64 val)
{
	if (val == CMDQ_LOG_FEAT_NUM) {
		util.fs.bit_feature = 0;
		cmdq_msg("data:%p val:%#llx bit_feature:%#x reset",
			data, val, util.fs.bit_feature);
		return 0;
	}

	if (val >= CMDQ_LOG_FEAT_NUM) {
		cmdq_err("data:%p val:%#llx cannot be over %#x",
			data, val, CMDQ_LOG_FEAT_NUM);
		return -EINVAL;
	}

	util.fs.bit_feature |= (1 << val);
	cmdq_msg("data:%p val:%#llx bit_feature:%#x",
		data, val, util.fs.bit_feature);
	return 0;
}
EXPORT_SYMBOL(cmdq_util_log_feature_set);

DEFINE_SIMPLE_ATTRIBUTE(cmdq_util_log_feature_fops,
	cmdq_util_log_feature_get, cmdq_util_log_feature_set, "%llu");

static atomic_t cmdq_dbg_ctrl[CMDQ_HW_MAX] = {ATOMIC_INIT(0)};

bool cmdq_util_is_prebuilt_client(struct cmdq_client *client)
{
	s32 i;

	for (i = 0; i < CMDQ_HW_MAX; i++)
		if (client == util.prebuilt_clt[i])
			return true;
	return false;
}
EXPORT_SYMBOL(cmdq_util_is_prebuilt_client);

void cmdq_util_prebuilt_set_client(const u16 hwid, struct cmdq_client *client)
{
	if (hwid >= CMDQ_HW_MAX)
		cmdq_err("invalid hwid:%u", hwid);
	else
		util.prebuilt_clt[hwid] = client;
	cmdq_msg("hwid:%u client:%p", hwid, client);
}
EXPORT_SYMBOL(cmdq_util_prebuilt_set_client);

bool cmdq_util_is_secure_client(struct cmdq_client *client)
{
	s32 thread_id = cmdq_mbox_chan_id(client->chan);

	if (thread_id >= 8 && thread_id <= 12)
		return true;
	return false;
}
EXPORT_SYMBOL(cmdq_util_is_secure_client);

void cmdq_util_set_domain(u32 hwid, u32 thrd)
{
	struct arm_smccc_res res;

	cmdq_log("%s hwid:%d thrd:%d", __func__, hwid, thrd);

	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_SET_DOMAIN_EN,
		hwid, thrd, 0, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
}
EXPORT_SYMBOL(cmdq_util_set_domain);

struct cmdq_thread *cmdq_client_get_thread(struct cmdq_client *client)
{
	if (client && client->chan)
		return cmdq_get_thread(cmdq_mbox_chan_id(client->chan),
			cmdq_util_get_hw_id((u32)cmdq_mbox_get_base_pa(
			client->chan)));
	else
		return cmdq_get_thread(cmdq_util_get_mdp_min_thrd(), 0);
}
EXPORT_SYMBOL(cmdq_client_get_thread);

void cmdq_util_set_mml_aid_selmode(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_SET_MML_SEC,
		0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(cmdq_util_set_mml_aid_selmode);

void cmdq_util_mmuen_devapc_disable(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_MMUEN_SET_DEVAPC_DISABLE,
		0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(cmdq_util_mmuen_devapc_disable);

void cmdq_util_enable_disp_va(void)
{
	struct arm_smccc_res res;

	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[0], true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_ENABLE_DISP_VA,
		0, 0, 0, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[0], false);
}
EXPORT_SYMBOL(cmdq_util_enable_disp_va);

void cmdq_util_disp_smc_cmd(u32 crtc_idx, u32 cmd)
{
	struct arm_smccc_res res;

	cmdq_log("%s: crtc %u, cmd %u", __func__, crtc_idx, cmd);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_DISP_CMD,
		crtc_idx, cmd, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(cmdq_util_disp_smc_cmd);

void cmdq_util_pkvm_disable(void)
{
	struct arm_smccc_res res;

	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[1], true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMD_CMDQ_TL_PKVM_DISABLE,
		0, 0, 0, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[1], false);
}
EXPORT_SYMBOL(cmdq_util_pkvm_disable);

void cmdq_util_prebuilt_init(const u16 mod)
{
	struct arm_smccc_res res;

	cmdq_log("%s: mod:%u", __func__, mod);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_PREBUILT_INIT, mod,
		0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(cmdq_util_prebuilt_init);

void cmdq_util_prebuilt_enable(const u16 hwid)
{
	struct arm_smccc_res res;

	cmdq_log("%s: hwid:%u", __func__, hwid);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_PREBUILT_ENABLE, hwid,
		0, 0, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
}
EXPORT_SYMBOL(cmdq_util_prebuilt_enable);

void cmdq_util_prebuilt_disable(const u16 hwid)
{
	struct arm_smccc_res res;

	cmdq_log("%s: hwid:%u", __func__, hwid);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_PREBUILT_DISABLE, hwid,
		0, 0, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
}
EXPORT_SYMBOL(cmdq_util_prebuilt_disable);

void cmdq_util_prebuilt_dump(const u16 hwid, const u16 event)
{
	struct arm_smccc_res res;
	const u16 mod = (event - CMDQ_TOKEN_PREBUILT_MDP_WAIT) /
		(CMDQ_TOKEN_PREBUILT_MML_WAIT - CMDQ_TOKEN_PREBUILT_MDP_WAIT);

	cmdq_msg("%s: hwid:%hu event:%hu mod:%hu", __func__, hwid, event, mod);

	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], true);
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_PREBUILT_DUMP, mod, event,
		0, 0, 0, 0, &res);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
}
EXPORT_SYMBOL(cmdq_util_prebuilt_dump);

void cmdq_util_prebuilt_dump_cpr(const u16 hwid, const u16 cpr, const u16 cnt)
{
	struct cmdq_client *clt = util.prebuilt_clt[hwid];
	struct cmdq_pkt *pkt;
	dma_addr_t pa = 0;
	void *va;
	u32 val[8];
	s32 i, j;

	if (cnt * sizeof(u32) > CMDQ_BUF_ALLOC_SIZE) {
		cmdq_msg("%s: hwid:%hu cpr:%#hx cnt:%hu too much",
			__func__, hwid, cpr, cnt);
		return;
	}

	va = cmdq_mbox_buf_alloc(clt, &pa);
	if (!va)
		return;
	memset(va, 0, CMDQ_BUF_ALLOC_SIZE);

	pkt = cmdq_pkt_create(clt);
	for (i = 0; i < cnt; i++)
		cmdq_pkt_write_indriect(
			pkt, NULL, pa + i * 4, cpr + i, UINT_MAX);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], true);
	cmdq_pkt_flush(pkt);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
	cmdq_pkt_destroy(pkt);

	cmdq_msg("%s: hwid:%hu cpr:%#x cnt:%hu", __func__, hwid, cpr, cnt);
	for (i = 0; i < cnt; i += 8) {
		for (j = 0; j < 8; j++)
			val[j] = readl(va + (i + j) * 4);
		cmdq_msg(
			"%s: cpr:%#x val:%#10x %#10x %#10x %#10x %#10x %#10x %#10x %#10x",
			__func__, cpr + i, val[0], val[1], val[2], val[3],
			val[4], val[5], val[6], val[7]);
	}
	cmdq_mbox_buf_free(clt, va, pa);
}
EXPORT_SYMBOL(cmdq_util_prebuilt_dump_cpr);

s32 cmdq_util_hw_trace_set_client(const u16 hwid, struct cmdq_client *client)
{
	if (!client) {
		cmdq_err("hw trace client is null");
		return -EFAULT;
	}

	if (hwid >= CMDQ_HW_MAX) {
		cmdq_err("invalid hwid:%u", hwid);
		return -EINVAL;
	}

	util.hw_trace[hwid].clt = client;
	cmdq_msg("hwid:%u client:%p", hwid, client);

	return 0;
}
EXPORT_SYMBOL(cmdq_util_hw_trace_set_client);

void cmdq_util_hw_trace_enable(const u16 hwid, const bool dram)
{
	struct cmdq_hw_trace *trace;
	s32 ret;

	if (hwid > util.mbox_cnt || !cmdq_hw_trace) {
		cmdq_msg("%s: hwid:%hu mbox_cnt:%u cmdq_hw_trace:%d",
			__func__, hwid, util.mbox_cnt, cmdq_hw_trace);
		return;
	}

	trace = &util.hw_trace[hwid];

	if (unlikely(trace->enable)) {
		cmdq_msg("%s: hwid:%hu enable:%d already",
			__func__, hwid, trace->enable);
		return;
	}
	trace->enable = true;

	if (unlikely(!trace->clt)) {
		ret = cmdq_mbox_set_hw_id(util.cmdq_mbox[hwid]);
		if (ret)
			return;
	}

	if (unlikely(!trace->buf)) {
		trace->buf = kzalloc(sizeof(*trace->buf), GFP_KERNEL);
		trace->buf->va_base =
			cmdq_mbox_buf_alloc(trace->clt, &trace->buf->pa_base);
	}
	memset(trace->buf->va_base, 0, CMDQ_BUF_ALLOC_SIZE);
	if (trace->update) {
		cmdq_pkt_destroy(trace->pkt);
		trace->pkt = NULL;
		trace->update = false;
		hw_trace_built_in[hwid] = trace->built_in;
	}

	if (unlikely(!trace->pkt)) {
		struct cmdq_operand lop, rop;
		s32 i;
		s32 size = hw_trace_built_in[hwid]?
			CMDQ_CPR_HW_TRACE_BUILT_IN_SIZE : CMDQ_CPR_HW_TRACE_SIZE;
		u16 cpr_start = hw_trace_built_in[hwid]?
			CMDQ_CPR_HW_TRACE_BUILT_IN_START : CMDQ_CPR_HW_TRACE_START;

		if (cmdq_get_support_vm(hwid) && hw_trace_built_in[hwid]) {
			size = CMDQ_CPR_HW_TRACE_BUILT_IN_VM_SIZE;
			cpr_start = CMDQ_CPR_HW_TRACE_BUILT_IN_VM_START;
		}

		trace->pkt = cmdq_pkt_create(trace->clt);

		lop.reg = true;
		lop.idx = CMDQ_CPR_HW_TRACE_TEMP;
		rop.reg = false;
		rop.value = 0;

		for (i = 0; i < size; i++) {
			if (hw_trace_built_in[hwid]) {
				cmdq_pkt_assign_command(trace->pkt,
					cpr_start + i, 0);
				continue;
			}
			cmdq_pkt_wfe(trace->pkt, CMDQ_TOKEN_HW_TRACE_WAIT);
			// SRAM
			cmdq_pkt_logic_command(trace->pkt, CMDQ_LOGIC_ADD,
				cpr_start + i, &lop, &rop);
			if (dram)
				cmdq_pkt_write_indriect(trace->pkt, NULL,
					trace->buf->pa_base + i * 4,
					CMDQ_CPR_HW_TRACE_TEMP, UINT_MAX);
		}
	}

	if (!hw_trace_built_in[hwid])
		cmdq_pkt_finalize_loop(trace->pkt);
	cmdq_pkt_flush_async(trace->pkt, NULL, NULL);
}
EXPORT_SYMBOL(cmdq_util_hw_trace_enable);

void cmdq_util_hw_trace_disable(const u16 hwid)
{
	struct cmdq_hw_trace *trace;

	if (hwid > util.mbox_cnt || !cmdq_hw_trace) {
		cmdq_msg("%s: hwid:%hu mbox_cnt:%u cmdq_hw_trace:%d",
			__func__, hwid, util.mbox_cnt, cmdq_hw_trace);
		return;
	}

	trace = &util.hw_trace[hwid];
	if (trace->enable && trace->clt) {
		trace->enable = false;
		cmdq_mbox_stop(trace->clt);
	}
}
EXPORT_SYMBOL(cmdq_util_hw_trace_disable);

void cmdq_util_hw_trace_dump(const u16 hwid, const bool dram)
{
	struct cmdq_hw_trace *trace;
	u32 val[8];
	s32 i, j;
	s32 cpr_size = hw_trace_built_in[hwid]?
		CMDQ_CPR_HW_TRACE_BUILT_IN_SIZE : CMDQ_CPR_HW_TRACE_SIZE;
	u16 cpr_start = hw_trace_built_in[hwid]?
		CMDQ_CPR_HW_TRACE_BUILT_IN_START : CMDQ_CPR_HW_TRACE_START;

	if (cmdq_get_support_vm(hwid) && hw_trace_built_in[hwid]) {
		cpr_size = CMDQ_CPR_HW_TRACE_BUILT_IN_VM_SIZE;
		cpr_start = CMDQ_CPR_HW_TRACE_BUILT_IN_VM_START;
	}

	if (hwid > util.mbox_cnt || !cmdq_hw_trace) {
		cmdq_msg("%s: hwid:%hu mbox_cnt:%u cmdq_hw_trace:%d",
			__func__, hwid, util.mbox_cnt, cmdq_hw_trace);
		return;
	}

	trace = &util.hw_trace[hwid];

	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], true);
	if (!trace->clt) {
		cmdq_err("hw trace disable");
		cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
		return;
	} else if (trace->clt && trace->pkt)
		cmdq_dump_summary(trace->clt, trace->pkt);

	// SRAM
	cmdq_util_prebuilt_dump_cpr(
		hwid, cpr_start, cpr_size);
	cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[hwid], false);
	// DRAM
	for (i = 0; dram && !hw_trace_built_in[hwid] && i < CMDQ_CPR_HW_TRACE_SIZE; i += 8) {
		for (j = 0; j < 8; j++)
			val[j] = readl(trace->buf->va_base + (i + j) * 4);
		cmdq_msg(
			"%s: i:%d val:%#10x %#10x %#10x %#10x %#10x %#10x %#10x %#10x",
			__func__, i, val[0], val[1], val[2], val[3],
			val[4], val[5], val[6], val[7]);
	}
}
EXPORT_SYMBOL(cmdq_util_hw_trace_dump);

void cmdq_util_hw_trace_mode_update(const u16 hwid,const bool built_in)
{
	struct cmdq_hw_trace *trace;

	if (hwid > util.mbox_cnt || !cmdq_hw_trace) {
		cmdq_err("%s: hwid:%u mbox_cnt:%u cmdq_hw_trace:%d",
			__func__, hwid, util.mbox_cnt, cmdq_hw_trace);
		return;
	}

	trace = &util.hw_trace[hwid];
	if (trace->clt && (trace->built_in != built_in)) {
		trace->update = true;
		trace->built_in = built_in;
	}
}
EXPORT_SYMBOL(cmdq_util_hw_trace_mode_update);

void cmdq_util_mminfra_cmd(const u8 type)
{
	struct arm_smccc_res res;

	cmdq_log("%s: type:%hu", __func__, type);

	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_MMINFRA_CMD, type, 0,
		0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(cmdq_util_mminfra_cmd);

void cmdq_util_enable_dbg(u32 id)
{
	if ((id < CMDQ_HW_MAX) && (atomic_cmpxchg(&cmdq_dbg_ctrl[id], 0, 1) == 0)) {
		struct arm_smccc_res res;

		cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[id], true);
		arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_ENABLE_DEBUG, id,
			0, 0, 0, 0, 0, &res);
		cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[id], false);
	}
}
EXPORT_SYMBOL(cmdq_util_enable_dbg);

void cmdq_util_return_dbg(u32 id, u64 *dbg)
{
	if (id < CMDQ_HW_MAX) {
		struct arm_smccc_res res1, res2;

		cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[id], true);
		arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_RETURN_DEBUG_1, id,
			0, 0, 0, 0, 0, &res1);
		arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_RETURN_DEBUG_2, id,
			0, 0, 0, 0, 0, &res2);
		cmdq_mbox_mtcmos_by_fast(util.cmdq_mbox[id], false);

		if (res1.a1) {
			*(dbg + 0) = res1.a1;
			*(dbg + 1) = res1.a2;
		}
		if (res2.a1) {
			*(dbg + 2) = res2.a1;
			*(dbg + 3) = res2.a2;
			*(dbg + 4) = res2.a3;
		}
	}
}
EXPORT_SYMBOL(cmdq_util_return_dbg);

void cmdq_util_user_buf_track(struct cmdq_client *cl, dma_addr_t pa, bool alloc)
{
	struct cmdq_user_buf_record *user_buf_record_unit;
	struct cmdq_thread *thread;
	s32 hwid;

	mutex_lock(&cmdq_buf_record_mutex);
	user_buf_record_unit = &util.ussr_buf_record[util.user_buf_record_idx++];
	hwid = (s32)cmdq_util_get_hw_id((u32)cmdq_mbox_get_base_pa(cl->chan));
	thread = (struct cmdq_thread *)((struct mbox_chan *)cl->chan)->con_priv;
	user_buf_record_unit->hwid = hwid;
	user_buf_record_unit->thrd_idx = thread->idx;
	user_buf_record_unit->is_alloc = alloc;
	user_buf_record_unit->pa = pa;
	user_buf_record_unit->nsec = sched_clock();

	if (util.user_buf_record_idx >= CMDQ_USER_BUF_RECORD_NUM)
		util.user_buf_record_idx = 0;
	mutex_unlock(&cmdq_buf_record_mutex);
}
EXPORT_SYMBOL(cmdq_util_user_buf_track);

void cmdq_util_buff_track(u32 *buf_peek_arr, const uint rows, const uint cols)
{
	u32 i, j;
	struct cmdq_buf_record *buf_record_unit;

	if(rows != CMDQ_HW_MAX || cols != CMDQ_THR_MAX_COUNT)
		return;

	mutex_lock(&cmdq_buf_record_mutex);
	buf_record_unit = &util.buf_record[util.buf_record_idx++];
	buf_record_unit->nsec = sched_clock();
	for(i = 0; i < rows; i++)
		for(j = 0; j < cols; j++)
			buf_record_unit->buf_peek_cnt[i][j] = buf_peek_arr[i * rows + j];

	if (util.buf_record_idx >= CMDQ_BUF_RECORD_NUM)
		util.buf_record_idx = 0;
	mutex_unlock(&cmdq_buf_record_mutex);
}
EXPORT_SYMBOL(cmdq_util_buff_track);

void cmdq_util_track(struct cmdq_pkt *pkt)
{
	struct cmdq_record *record;
	struct cmdq_client *cl = pkt->cl;
	struct cmdq_pkt_buffer *buf;
	u64 done = sched_clock();
	u32 offset, *perf, acq_time, irq_time, begin_wait, exec_time, total_time, hw_time;
	u32 exec_begin = 0, exec_end = 0;
	u64 submit_sec, last_inst = 0;
	unsigned long submit_rem, hw_time_rem;

	mutex_lock(&cmdq_record_mutex);

	record = &util.record[util.record_idx];
	record->pkt = (unsigned long)pkt;
	record->priority = pkt->priority;
	record->size = pkt->cmd_buf_size;

	record->submit = pkt->rec_submit;
	record->trigger = pkt->rec_trigger;
	record->wait = pkt->rec_wait;
	record->irq = pkt->rec_irq;
	record->done = done;

	submit_sec = pkt->rec_submit;
	submit_rem = do_div(submit_sec, 1000000000);

	util_time_to_us(pkt->rec_submit, pkt->rec_trigger, acq_time);
	util_time_to_us(pkt->rec_trigger, pkt->rec_irq, irq_time);
	util_time_to_us(pkt->rec_submit, pkt->rec_wait, begin_wait);
	util_time_to_us(pkt->rec_trigger, done, exec_time);
	util_time_to_us(pkt->rec_submit, done, total_time);

	if (cl && cl->chan) {
		record->thread = cmdq_mbox_chan_id(cl->chan);
		record->id = cmdq_util_get_hw_id((u32)cmdq_mbox_get_base_pa(
			cl->chan));
	} else {
		record->thread = -1;
		record->id = -1;
	}

#ifdef CMDQ_SECURE_SUPPORT
	if (pkt->sec_data)
		record->is_secure = true;
#endif

	if (!list_empty(&pkt->buf)) {
		buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);

		buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
		offset = CMDQ_CMD_BUFFER_SIZE - (pkt->buf_size -
			pkt->cmd_buf_size);
		record->last_inst = *(u64 *)(buf->va_base + offset -
			CMDQ_INST_SIZE);
		last_inst = record->last_inst;

		perf = cmdq_pkt_get_perf_ret(pkt);
		if (perf) {
			record->exec_begin = perf[0];
			record->exec_end = perf[1];
			exec_begin = record->exec_begin;
			exec_end = record->exec_end;
		}
	}

	hw_time = exec_end > exec_begin ?
		exec_end - exec_begin :
		~exec_begin + 1 + exec_end;
	hw_time_rem = (u32)CMDQ_TICK_TO_US(hw_time);

	if (cmdq_print_debug) {
		char *buf_va, *buf_va_end;

		buf_va = (char *)(shared_mem->va + log_length * 2 + (util.record_idx * log_length));
		memset_io(buf_va, 0, log_length);
		buf_va_end = buf_va + log_length;
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%u,%#lx,%d,%d,%u,%hhd,%d,", util.record_idx,
			record->pkt, record->priority, record->is_secure, record->size, record->id, record->thread);
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%llu.%06lu,", submit_sec, submit_rem / 1000);
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%u,%u,%u,%u,%u,%#llx,", acq_time, irq_time,
			begin_wait, exec_time, total_time, last_inst);
		buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%u,%u,%u.%06lu,\n", exec_begin, exec_end,
			hw_time, hw_time_rem);
	}

	util.record_idx++;
	if (util.record_idx >= CMDQ_RECORD_NUM)
		util.record_idx = 0;
	mutex_unlock(&cmdq_record_mutex);
}
EXPORT_SYMBOL(cmdq_util_track);

void cmdq_util_dump_smi(void)
{
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
#if !IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	int smi_hang;

	smi_hang = mtk_smi_dbg_hang_detect("CMDQ");
	cmdq_util_err("smi hang:%d", smi_hang);
#endif
#else
	cmdq_util_err("[WARNING]not enable SMI dump now");
#endif
}
EXPORT_SYMBOL(cmdq_util_dump_smi);

void cmdq_util_devapc_dump(void)
{
	u32 i;

	cmdq_util_msg("%s mbox cnt:%u", __func__, util.mbox_cnt);
	cmdq_set_alldump(true);
	for (i = 0; i < util.mbox_cnt; i++) {
		cmdq_mbox_dump_dbg(util.cmdq_mbox[i], NULL, true);
		cmdq_thread_dump_all(util.cmdq_mbox[i], true, true, false);
	}
	cmdq_set_alldump(false);
}
EXPORT_SYMBOL(cmdq_util_devapc_dump);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = INFRA_SUBSYS_GCE,
	.debug_dump = cmdq_util_devapc_dump,
};
#endif

void cmdq_util_dump_fast_mtcmos(void)
{
	u32 i;

	for (i = 0; i < util.mbox_cnt; i++)
		cmdq_mbox_dump_fast_mtcmos(util.cmdq_mbox[i]);
}
EXPORT_SYMBOL(cmdq_util_dump_fast_mtcmos);

u8 cmdq_util_track_ctrl(void *cmdq, phys_addr_t base, bool sec)
{
	cmdq_msg("%s cmdq:%p sec:%s", __func__, cmdq, sec ? "true" : "false");
	if (sec)
		util.cmdq_sec_mbox[util.mbox_sec_cnt++] = cmdq;
	else
		util.cmdq_mbox[util.mbox_cnt++] = cmdq;

	return (u8)cmdq_util_get_hw_id((u32)base);
}
EXPORT_SYMBOL(cmdq_util_track_ctrl);

void cmdq_util_set_first_err_mod(void *chan, const char *mod)
{
	u32 hw_id = cmdq_util_get_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	util.first_err_mod[hw_id] = mod;
}
EXPORT_SYMBOL(cmdq_util_set_first_err_mod);

const char *cmdq_util_get_first_err_mod(void *chan)
{
	u32 hw_id = cmdq_util_get_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	return util.first_err_mod[hw_id];
}
EXPORT_SYMBOL(cmdq_util_get_first_err_mod);

int cmdq_proc_create(void)
{
	struct proc_dir_entry *debugDirEntry = NULL;
	struct proc_dir_entry *entry = NULL;

	debugDirEntry = proc_mkdir("mtk_cmdq_debug", NULL);
	if (!debugDirEntry) {
		cmdq_err("debugfs_create_dir cmdq failed");
		return -EINVAL;
	}

	if (!cmdq_proc_debug_off) {
		entry = proc_create("cmdq-status", 0440, debugDirEntry,
			&cmdq_util_status_fops);
		if (!entry) {
			cmdq_err("proc_create_file cmdq-status failed");
			return -ENOMEM;
		}

		entry = proc_create("cmdq-record", 0440, debugDirEntry,
			&cmdq_util_record_fops);
		if (!entry) {
			cmdq_err("proc_create_file cmdq-record failed");
			return -ENOMEM;
		}
	}

	if (cmdq_dump_buf_size) {
		entry = proc_create("cmdq_buffer_record", 0440, debugDirEntry,
			&cmdq_proc_util_buf_record_fops);
		if (!entry) {
			cmdq_err("proc_create_file cmdq_buffer_record failed");
			return -ENOMEM;
		}
	}
	return 0;
}
EXPORT_SYMBOL(cmdq_proc_create);

void cmdq_util_reserved_memory_lookup(struct device *dev)
{
	struct device_node node;
	static struct reserved_mem *mem;
	static void *va;
	char buf[NAME_MAX] = {0};
	u64 pa = 0;
	s32 i, len;
	char *buf_va, *buf_va_end;
	char title[] = "index,pkt,task priority,sec,size,gce,thread,submit,acq_time(us),"
		"irq_time(us),begin_wait(us),exec_time(us),total_time(us),jump,"
		"exec begin,exec end,hw_time(us),";

	shared_mem = kzalloc(sizeof(*shared_mem), GFP_KERNEL);
	if (!shared_mem)
		return;

	shared_mem->va = dma_alloc_coherent(dev, CMDQ_RECORD_SIZE + CMDQ_STATUS_SIZE,
		&shared_mem->pa, GFP_KERNEL);
	shared_mem->size = CMDQ_RECORD_SIZE + CMDQ_STATUS_SIZE;

	for (i = 0; i < 32 && !mem; i++) {
		memset(buf, 0, sizeof(buf));
		len = snprintf(buf, NAME_MAX - 1, "mblock-%d-me_cmdq_reserved", i);
		if (len < 0 || len >= sizeof(buf)) {
			cmdq_err("mblock-%d-me_cmdq_reserved failed", i);
			return;
		}
		node.full_name = buf;
		mem = of_reserved_mem_lookup(&node);
	}

	if (!mem)
		return;

	pa = mem->base + mem->size - CMDQ_RECORD_SIZE - CMDQ_STATUS_SIZE;

	if (!va)
		va = ioremap(pa, CMDQ_RECORD_SIZE + CMDQ_STATUS_SIZE);
	shared_mem->va = va;


	if (!cpr_not_support_cookie) {
		shared_mem->pa = pa;
		shared_mem->mva = *(u64 *)va ? *(u64 *)va : pa; /* iova */

		cmdq_msg("%s: buf:%s pa:%#llx size:%u va:%p pa:%pa iova:%pa", __func__,
			buf, pa, shared_mem->size, shared_mem->va,
			&shared_mem->pa, &shared_mem->mva);
	} else {
		shared_mem->pa = *(u64 *)va ? *(u64 *)va : pa; /* iova */
		cmdq_msg("%s: buf:%s pa:%#llx size:%u va:%p iova:%pa", __func__,
			buf, pa, shared_mem->size, shared_mem->va, &shared_mem->pa);
	}

	buf_va = (char *)(shared_mem->va);
	buf_va_end = buf_va + strlen(title);
	buf_va += scnprintf(buf_va, buf_va_end - buf_va, "%s\n", title);

}
EXPORT_SYMBOL(cmdq_util_reserved_memory_lookup);

int cmdq_util_init(void)
{
	struct dentry	*dir;
	bool exists = false;
	u32 i;

	cmdq_msg("%s begin", __func__);

	cmdq_controller_set_fp(&controller_fp);
	cmdq_helper_set_fp(&helper_fp);

	for (i = 0; i < CMDQ_HW_MAX; i++) {
		spin_lock_init(&util.err[i].lock);
		util.err[i].buffer = vzalloc(CMDQ_FIRST_ERR_SIZE);
		if (!util.err[i].buffer)
			return -ENOMEM;
	}
	util.buf_rec_buffer = vzalloc(CMDQ_BUF_REC_BUFFER_SIZE);
	if (!util.buf_rec_buffer)
		return -ENOMEM;

	dir = debugfs_lookup("cmdq", NULL);
	if (!dir) {
		dir = debugfs_create_dir("cmdq", NULL);
		if (!dir) {
			cmdq_err("debugfs_create_dir cmdq failed");
			return -EINVAL;
		}
	} else
		exists = true;

	util.fs.log_feature = debugfs_create_file("cmdq-log-feature",
		0444, dir, &util, &cmdq_util_log_feature_fops);
	if (IS_ERR(util.fs.log_feature)) {
		cmdq_err("debugfs_create_file cmdq-log-feature failed:%ld",
			PTR_ERR(util.fs.log_feature));
		return PTR_ERR(util.fs.log_feature);
	}

	if (exists)
		dput(dir);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	memset(util.cmdq_irq_thrd_history, CMDQ_THR_MAX_COUNT, sizeof(util.cmdq_irq_thrd_history));
	mrdump_set_extra_dump(AEE_EXTRA_FILE_CMDQ, cmdq_util_buf_record_aee_dump);
	cmdq_msg("%s end", __func__);

	return 0;
}
EXPORT_SYMBOL(cmdq_util_init);

MODULE_LICENSE("GPL v2");
