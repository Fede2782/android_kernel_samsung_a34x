/*
 *sec_debug_extrainfo.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/sec_debug.h>
#include <asm/stacktrace.h>
#include <asm/esr.h>
#include <linux/kdebug.h>
#include <linux/notifier.h>
#include <linux/panic_notifier.h>

#include <trace/hooks/softlockup.h>
#include <trace/hooks/traps.h>
#include <trace/hooks/fault.h>
#include <trace/hooks/bug.h>

/* 
	--------------------------------------------  sec-initlog
	-             INIT_LOG_SIZE 1M             -
	--------------------------------------------  sec-autocomment
	-      AC_BUFFER_SIZE+AC_INFO_SIZE 64K     -  
	--------------------------------------------  debug-history
	-           DEBUG_HIST_SIZE 512K           -
	--------------------------------------------  dbore
	-           DEBUG_BORE_SIZE 512K           -
	--------------------------------------------  sec-extrainfo
    -	           REMAINED_SIZE			   -
    --------------------------------------------  end of reserved sec_debug
*/

#define SZ_960	0x000003C0
#define EXTRA_VERSION	"TE23"

static unsigned int __read_mostly reset_rwc;
module_param(reset_rwc, uint, 0440);

static u32 __initdata sec_extra_info_base;
static u32 __initdata sec_extra_info_size = SZ_64K;

struct sec_debug_panic_extra_info sec_debug_extra_info_init = {
	.item = {
		{"ID",		"", SZ_32},
		{"KTIME",	"", SZ_8},
		{"QBI", 	"", SZ_16},
		{"LEV", 	"", SZ_4},
		{"BIN",		"", SZ_16},
		{"RSTCNT",	"", SZ_4},
		{"RR",		"", SZ_8},
		{"PC",		"", SZ_64},
		{"LR",		"", SZ_64},		
		{"FAULT",	"", SZ_32},
		{"BUG",		"", SZ_64},
		{"PANIC",	"", SZ_64},
		{"STACK",	"", SZ_960},
		{"SMP",		"", SZ_8},
		{"ETC",		"", SZ_256},
		{"KLG",		"", SZ_256},

		/* extrb reset information */
		{"ID",		"", SZ_32},

		/* extrc reset information */
		{"ID",		"", SZ_32},

		/* extrm reset information */
		{"ID",		"", SZ_32},
		{"RR",		"", SZ_8},
		{"MFC",		"", SZ_960},
	}
};

extern struct reserved_mem *secdbg_log_get_rmem(const char *compatible);

struct sec_debug_panic_extra_info *sec_debug_extra_info;
struct sec_debug_panic_extra_info *sec_debug_extra_info_backup;
char *sec_debug_extra_info_buf;

/******************************************************************************
 * sec_debug_init_extra_info() - function to init extra info
 *
 * This function simply initialize each filed of sec_debug_panic_extra_info.
******************************************************************************/
void sec_debug_init_extra_info(struct sec_debug_shared_info *sec_debug_info,
				 phys_addr_t sec_extra_info_size, int reset_status)
{
	if (sec_debug_info) {
		pr_crit("%s: cold reset: %d\n", __func__, reset_status);
		sec_debug_extra_info = &sec_debug_info->sec_debug_extra_info;

		if (reset_status) {
			sec_debug_extra_info_backup = &sec_debug_info->sec_debug_extra_info_backup;
			sec_debug_extra_info_buf = (char *)sec_debug_info + sec_extra_info_size - SZ_1K;
			memcpy(sec_debug_extra_info_backup, sec_debug_extra_info,
					sizeof(struct sec_debug_panic_extra_info));
		}

		if (sec_debug_extra_info)
			memcpy(sec_debug_extra_info, &sec_debug_extra_info_init,
					sizeof(sec_debug_extra_info_init));
	}
}

/******************************************************************************
 * sec_debug_set_extra_info() - function to set each extra info field
 *
 * This function simply set each filed of sec_debug_panic_extra_info.
******************************************************************************/
void sec_debug_set_extra_info(enum sec_debug_extra_buf_type type,
				const char *fmt, ...)
{
	va_list args;

	if (sec_debug_extra_info) {
		if (!strlen(sec_debug_extra_info->item[type].val)) {
			va_start(args, fmt);
			vsnprintf(sec_debug_extra_info->item[type].val,
					sec_debug_extra_info->item[type].max, fmt, args);
			va_end(args);
		}
	}
}

/******************************************************************************
 * sec_debug_store_extra_info - function to export extra info
 *
 * This function finally export the extra info to destination buffer.
 * The contents of buffer will be deliverd to framework at the next booting.
*****************************************************************************/
void sec_debug_store_extra_info(int start, int end)
{
	int i;
	int maxlen = MAX_EXTRA_INFO_KEY_LEN + MAX_EXTRA_INFO_VAL_LEN + 10;
	char *ptr = sec_debug_extra_info_buf;

	/* initialize extra info output buffer */
	memset((void *)ptr, 0, SZ_2K);

	if (!sec_debug_extra_info_backup)
		return;

	ptr += snprintf(ptr, maxlen, "\"%s\":\"%s\"", sec_debug_extra_info_backup->item[start].key,
		sec_debug_extra_info_backup->item[start].val);
		

	for (i = start + 1; i < end; i++) {
		if (ptr + strnlen(sec_debug_extra_info_backup->item[i].key, MAX_EXTRA_INFO_KEY_LEN)
			 + strnlen(sec_debug_extra_info_backup->item[i].val, MAX_EXTRA_INFO_VAL_LEN)
			 + MAX_EXTRA_INFO_HDR_LEN > sec_debug_extra_info_buf
			 + SZ_2K)
			break;

		ptr += snprintf(ptr, maxlen, ",\"%s\":\"%s\"",
			sec_debug_extra_info_backup->item[i].key,
			sec_debug_extra_info_backup->item[i].val);
	}
}

/******************************************************************************
 * sec_debug_store_extra_info_A
 ******************************************************************************/
void sec_debug_store_extra_info_A(void)
{
	sec_debug_store_extra_info(INFO_AID, INFO_MAX_A);
}

/******************************************************************************
 * sec_debug_store_extra_info_B
 ******************************************************************************/
void sec_debug_store_extra_info_B(void)
{
	sec_debug_store_extra_info(INFO_BID, INFO_MAX_B);
}

/******************************************************************************
 * sec_debug_store_extra_info_C
 ******************************************************************************/
void sec_debug_store_extra_info_C(void)
{
	sec_debug_store_extra_info(INFO_CID, INFO_MAX_C);
}

/******************************************************************************
 * sec_debug_store_extra_info_M
 ******************************************************************************/
void sec_debug_store_extra_info_M(void)
{
	sec_debug_store_extra_info(INFO_MID, INFO_MAX_M);
}

/******************************************************************************
 * sec_debug_set_extra_info_id
******************************************************************************/
static void sec_debug_set_extra_info_id(void)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);

	sec_debug_set_extra_info(INFO_AID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_BID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_CID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_MID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
}

/******************************************************************************
 * sec_debug_set_extra_info_ktime
******************************************************************************/
static void sec_debug_set_extra_info_ktime(void)
{
	u64 ts_nsec;

	ts_nsec = local_clock();
	do_div(ts_nsec, 1000000000);
	sec_debug_set_extra_info(INFO_KTIME, "%lu", (unsigned long)ts_nsec);
}

/******************************************************************************
 * sec_debug_set_extra_info_fault
******************************************************************************/
void sec_debug_set_extra_info_fault(unsigned long addr, struct pt_regs *regs)
{
	if (regs) {
		pr_crit("sec_debug_set_extra_info_fault = 0x%lx\n", addr);
		sec_debug_set_extra_info(INFO_FAULT, "0x%lx", addr);
		sec_debug_set_extra_info(INFO_PC, "%pS", regs->pc);
		sec_debug_set_extra_info(INFO_LR, "%pS",
					 compat_user_mode(regs) ?
					  regs->compat_lr : regs->regs[30]);
	}
}

/******************************************************************************
 * sec_debug_set_extra_info_bug
******************************************************************************/
void sec_debug_set_extra_info_bug(const char *file, unsigned int line)
{
	sec_debug_set_extra_info(INFO_BUG, "%s:%u", file, line);
}

void sec_debug_set_extra_info_bug_verbose(unsigned long bugaddr)
{
	sec_debug_set_extra_info(INFO_BUG, "%pS", (u64)bugaddr);
}

/******************************************************************************
 * sec_debug_set_extra_info_panic
******************************************************************************/
void sec_debug_set_extra_info_panic(char *str)
{
	if (unlikely(strstr(str, "\nPC is at")))
		strcpy(strstr(str, "\nPC is at"), "");

	sec_debug_set_extra_info(INFO_PANIC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_zswap
******************************************************************************/
void sec_debug_set_extra_info_zswap(char *str)
{
	sec_debug_set_extra_info(INFO_ETC, "%s", str);
}

/******************************************************************************
 * Copied From SED_DEBUG.c
******************************************************************************/
void sec_debug_set_extra_info_upload(char *str)
{
	if (str) {
		sec_debug_set_extra_info_panic(str);
		sec_debug_finish_extra_info();
	}
}

static int secdbg_exin_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	sec_debug_set_extra_info_upload(buf);

	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_exin_panic_handler,
};

static bool is_bug_reported;
static void android_rvh_report_bug(void *data,
		const char *file, unsigned int line, unsigned long bugaddr)
{
	is_bug_reported = true;

	if (file)
		sec_debug_set_extra_info_bug(file, line);
}

static int secdbg_exin_die_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;
	u64 lr;

	if (is_bug_reported)
		sec_debug_set_extra_info_fault((unsigned long)regs->pc, regs);

	if (compat_user_mode(regs))
		lr = regs->compat_lr;
	else
		lr = regs->regs[30];

	sec_debug_set_extra_info(INFO_PC, "%pS", regs->pc);
	sec_debug_set_extra_info(INFO_LR, "%pS",
			user_mode(regs) ? lr : ptrauth_strip_kernel_insn_pac(lr));

	return NOTIFY_DONE;
}

static struct notifier_block nb_die_block = {
	.notifier_call = secdbg_exin_die_handler,
	.priority = INT_MAX,
};

static void register_vendor_hooks(void)
{
#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
/* Disabled on temporary ***************
	register_trace_android_vh_watchdog_timer_softlockup(android_vh_watchdog_timer_softlockup, NULL);
	register_trace_android_rvh_do_el1_bti(android_rvh_do_el1_bti, NULL);
	register_trace_android_rvh_do_el1_fpac(android_rvh_do_el1_fpac, NULL);
	register_trace_android_rvh_panic_unhandled(android_rvh_bad_mode, NULL);
	register_trace_android_rvh_arm64_serror_panic(android_rvh_arm64_serror_panic, NULL);
	register_trace_android_rvh_die_kernel_fault(android_rvh_die_kernel_fault, NULL);
	register_trace_android_rvh_do_sea(android_rvh_do_sea, NULL);
	register_trace_android_rvh_do_sp_pc_abort(android_rvh_do_sp_pc_abort, NULL);
	register_trace_android_vh_try_to_freeze_todo_unfrozen(android_vh_try_to_freeze_todo_unfrozen, NULL);
	register_trace_android_vh_try_to_freeze_todo(android_vh_try_to_freeze_todo, NULL);
	register_trace_android_rvh_do_el1_undef(android_rvh_do_el1_undef, NULL);
****************************************/
	register_trace_android_rvh_report_bug(android_rvh_report_bug, NULL);
	register_die_notifier(&nb_die_block);
#endif
}

static int sec_debug_check_magic(struct sec_debug_shared_info *sdi)
{
	if (sdi->magic[0] != SEC_DEBUG_SHARED_MAGIC0) {
		pr_crit("%s: wrong magic 0: %x|%x\n",
				__func__, sdi->magic[0], SEC_DEBUG_SHARED_MAGIC0);
		return 0;
	}

	if (sdi->magic[1] != SEC_DEBUG_SHARED_MAGIC1) {
		pr_crit("%s: wrong magic 1: %x|%x\n",
				__func__, sdi->magic[1], SEC_DEBUG_SHARED_MAGIC1);
		return 0;
	}

	if (sdi->magic[2] != SEC_DEBUG_SHARED_MAGIC2) {
		pr_crit("%s: wrong magic 2: %x|%x\n",
				__func__, sdi->magic[2], SEC_DEBUG_SHARED_MAGIC2);
		return 0;
	}

	if (sdi->magic[3] != SEC_DEBUG_SHARED_MAGIC3) {
		pr_crit("%s: wrong magic 3: %x|%x\n",
				__func__, sdi->magic[3], SEC_DEBUG_SHARED_MAGIC3);
		return 0;
	}

	return 1;
}

static struct sec_debug_shared_info *sec_debug_info;

static void sec_debug_init_base_buffer(void)
{
	int reset_status = 0;
	struct reserved_mem *rmem;

	rmem = secdbg_log_get_rmem("samsung,secdbg-extra-info");
	sec_extra_info_base = rmem->base;

	sec_debug_info = (struct sec_debug_shared_info *)phys_to_virt(sec_extra_info_base);

	if (sec_debug_info) {
		reset_status = 1;    // Consider both cold reset and warm reset 

		if (!sec_debug_check_magic(sec_debug_info)) {
			sec_debug_info->magic[0] = SEC_DEBUG_SHARED_MAGIC0;
			sec_debug_info->magic[1] = SEC_DEBUG_SHARED_MAGIC1;
			sec_debug_info->magic[2] = SEC_DEBUG_SHARED_MAGIC2;
			sec_debug_info->magic[3] = SEC_DEBUG_SHARED_MAGIC3;
		}

		sec_debug_init_extra_info(sec_debug_info, sec_extra_info_size, reset_status);
	}

	pr_info("%s, base(virt):0x%lx size:0x%x\n", __func__, (unsigned long)sec_debug_info, sec_extra_info_size);
}

/*
 * proc/extra
 */
static int sec_debug_reset_reason_extra_show(struct seq_file *m, void *v)
{
	ssize_t size = 0;
	char buf[SZ_1K];

	if (!sec_debug_extra_info_backup)
		return -ENOENT;
	
	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_RSTCNT].key,
			sec_debug_extra_info_backup->item[INFO_RSTCNT].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_PC].key,
			sec_debug_extra_info_backup->item[INFO_PC].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_LR].key,
			sec_debug_extra_info_backup->item[INFO_LR].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_STACK].key,
			sec_debug_extra_info_backup->item[INFO_STACK].val);

	seq_printf(m, "%s", buf);

	return 0;
}

static int sec_debug_reset_reason_extra_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_debug_reset_reason_extra_show, NULL);
}

static const struct proc_ops sec_debug_reset_reason_extra_proc_fops = {
	.proc_open = sec_debug_reset_reason_extra_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/******************************************************************************
 * End of Copied From SED_DEBUG.c
******************************************************************************/

void sec_debug_finish_extra_info(void)
{
	sec_debug_set_extra_info_ktime();
}

static int sec_debug_reset_rwc_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d", reset_rwc);
	return 0;
}

static int sec_debug_reset_rwc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_debug_reset_rwc_proc_show, NULL);
}

static const struct proc_ops sec_debug_reset_rwc_proc_fops = {
	.proc_open = sec_debug_reset_rwc_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int set_debug_reset_extra_info_proc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	sec_debug_store_extra_info_A();
	memcpy(buf, sec_debug_extra_info_buf, SZ_1K);
	seq_printf(m, "%s", buf);

	return 0;
}

static int sec_debug_reset_extra_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_debug_reset_extra_info_proc_show, NULL);
}

static const struct proc_ops sec_debug_reset_extra_info_proc_fops = {
	.proc_open = sec_debug_reset_extra_info_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

extern int secdbg_hw_param_init(void);

static int __init secdbg_extra_info_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("extra", 0400, NULL, 
						&sec_debug_reset_reason_extra_proc_fops);

	if (!entry)
		return -ENOMEM;

	entry = proc_create("reset_reason_extra_info", S_IWUGO, NULL,
						&sec_debug_reset_extra_info_proc_fops);
	
	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, SZ_2K);

	entry = proc_create("reset_rwc", S_IWUGO, NULL,
						&sec_debug_reset_rwc_proc_fops);

	if (!entry)
		return -ENOMEM;

	sec_debug_init_base_buffer();

	sec_debug_set_extra_info_id();
	secdbg_hw_param_init();
	
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	
	register_vendor_hooks();

	return 0;
}
module_init(secdbg_extra_info_init);

MODULE_DESCRIPTION("Samsung Debug Extr. Info. driver");
MODULE_LICENSE("GPL v2");
