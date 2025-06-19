// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
#include <linux/scmi_protocol.h>
#include <tinysys-scmi.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#include "apmcupm_scmi_v2.h"
#include "pmsr_v3.h"

#define ACC_RESULTS cfg.acc_results

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static struct scmi_tinysys_info_st *tinfo;
static int scmi_apmcupm_id;
static int pmsr_sspm_ready;
struct apmcupm_scmi_set_input pmsr_scmi_set_data;
#endif

static unsigned int timer_window_len;
static struct pmsr_cfg cfg;
struct hrtimer pmsr_timer;
static struct workqueue_struct *pmsr_tool_forcereq;
static struct delayed_work pmsr_tool_forcereq_work;
static unsigned int pmsr_tool_last_idx;
static unsigned int *pmsr_tool_last_time_stamp;

static char *ch_name[] = {
	"ch0",
	"ch1",
	"ch2",
	"ch3",
	"ch4",
	"ch5",
	"ch6",
	"ch7",
};

static void pmsr_cfg_init(void)
{
	int i;

	cfg.enable = false;
	cfg.pmsr_speed_mode = DEFAULT_SPEED_MODE;
	cfg.pmsr_window_len = 0;
	cfg.clean_records = 0;
	cfg.pmsr_sample_rate = 0;
	cfg.err = 0;
	cfg.test = 0;
	cfg.prof_cnt = 0;
	cfg.pmsr_sig_count = 0;
	cfg.share_buf = NULL;
	cfg.pmsr_tool_share_results = NULL;

	for (i = 0 ; i < PMSR_MET_CH; i++) {
		cfg.ch[i].dpmsr_id = 0xFFFFFFFF;
		cfg.ch[i].signal_id = 0xFFFFFFFF; /* default disabled */
	}

	for (i = 0 ; i < PMSR_MAX_SIG_CH; i++) {
		cfg.pmsr_signal_id[i] = 0xFFFFFFFF;
		ACC_RESULTS.results[i] = 0;
	}
	ACC_RESULTS.time_stamp = 0;
	ACC_RESULTS.winlen = 0;
	ACC_RESULTS.acc_num = 0;

	for (i = 0 ; i < cfg.dpmsr_count; i++) {
		cfg.dpmsr[i].seltype = DEFAULT_SELTYPE;
		cfg.dpmsr[i].montype = DEFAULT_MONTYPE;
		cfg.dpmsr[i].signum = 0;
		cfg.dpmsr[i].en = 0;
	}

	/* pmsr_tool_last_idx is the last position read by ap */
	/* when pmsr tool just starts and read idx below is 4, */
	/* it means ap is faster than sspm -> there is no new data in sram */
	pmsr_tool_last_idx = cfg.pmsr_tool_buffer_max_space - 1;
}

static int pmsr_ipi_init(void)
{
	unsigned int ret = 0;

	/* for AP to SSPM */
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	tinfo = get_scmi_tinysys_info();

	if (!tinfo) {
		pr_info("get scmi info fail\n");
		return ret;
	}

	ret = of_property_read_u32(tinfo->sdev->dev.of_node, "scmi-apmcupm",
			&scmi_apmcupm_id);
	if (ret) {
		pr_info("get scmi-apmcupm fail, ret %d\n", ret);
		pmsr_sspm_ready = -2;
		ret = -1;
		return ret;
	}

	pmsr_sspm_ready = 1;
#endif
	return ret;
}

static int pmsr_get_sspm_sram(void)
{
	unsigned int user_info =
				(APMCU_SET_UID(APMCU_SCMI_UID_PMSR) |
				APMCU_SET_UUID(APMCU_SCMI_UUID_PMSR) |
				APMCU_SET_MG);
	int ret;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct scmi_tinysys_status pmsr_tool_rvalue;

	pmsr_scmi_set_data.user_info =
			(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_GET_SRAM));

	/* SCMI GET interface return sspm sram address */
	ret = scmi_tinysys_common_get(tinfo->ph, scmi_apmcupm_id,
			pmsr_scmi_set_data.user_info, &pmsr_tool_rvalue);
	if (ret) {
		cfg.err |= (1 << PMSR_TOOL_ACT_GET_SRAM);
	} else {
		cfg.share_buf =
			(struct pmsr_tool_mon_results *)sspm_sbuf_get(pmsr_tool_rvalue.r1);
		cfg.pmsr_tool_buffer_max_space = pmsr_tool_rvalue.r2;
		cfg.pmsr_tool_share_results =
			(struct pmsr_tool_results *)sspm_sbuf_get(pmsr_tool_rvalue.r3);
	}
	pmsr_tool_last_time_stamp =
		kcalloc(cfg.pmsr_tool_buffer_max_space, sizeof(unsigned int), GFP_KERNEL);
	if (!pmsr_tool_last_time_stamp)
		pr_notice("pmsr tool last time stamp fail\n");
#endif

	return 0;
}

static void pmsr_tool_send_forcereq(struct work_struct *work)
{
	unsigned int read_idx;
	unsigned int oldest_idx;
	struct pmsr_tool_mon_results *pmsr_tool_val;
	int ret;
	unsigned int i;
	unsigned int user_info =
			(APMCU_SET_UID(APMCU_SCMI_UID_PMSR) |
			APMCU_SET_UUID(APMCU_SCMI_UUID_PMSR) |
			APMCU_SET_MG);

	/* if cfg.test = 1, then scmi has been sent */
	if ((cfg.pmsr_sample_rate != 0) && (cfg.test != 1)) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
		pmsr_scmi_set_data.user_info =
			(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_TEST));
		ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
			pmsr_scmi_set_data.user_info, 0, 0, 0, 0);
		if (ret)
			cfg.err |= (1 << PMSR_TOOL_ACT_TEST);
#endif
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	if (!cfg.pmsr_tool_share_results)
		return;
	oldest_idx = cfg.pmsr_tool_share_results->oldest_idx;
	read_idx = oldest_idx > 0 ?
		oldest_idx - 1 : cfg.pmsr_tool_buffer_max_space - 1;

	if (read_idx != pmsr_tool_last_idx) {
		if (cfg.share_buf) {
			pmsr_tool_val = &cfg.share_buf[read_idx];

			pr_notice("[%u] dpmsr %u(%u): %u, %u, %u, %u, %u, %u, %u, %u\n",
				pmsr_tool_val->time_stamp, cfg.dpmsr_count-1,
				pmsr_tool_val->winlen,
				pmsr_tool_val->results[0],
				pmsr_tool_val->results[1],
				pmsr_tool_val->results[2],
				pmsr_tool_val->results[3],
				pmsr_tool_val->results[4],
				pmsr_tool_val->results[5],
				pmsr_tool_val->results[6],
				pmsr_tool_val->results[7]);

			pmsr_tool_last_idx = read_idx;

			for (i = 0; i < PMSR_MAX_SIG_CH; i++)
				ACC_RESULTS.results[i] += pmsr_tool_val->results[i];

			ACC_RESULTS.time_stamp = pmsr_tool_val->time_stamp;
			ACC_RESULTS.winlen += pmsr_tool_val->winlen;
			ACC_RESULTS.acc_num++;

		}
	}
#endif
}

static void pmsr_procfs_exit(void)
{
	remove_proc_entry("pmsr", NULL);
}

static ssize_t remote_data_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t remote_data_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));
	unsigned int user_info =
				(APMCU_SET_UID(APMCU_SCMI_UID_PMSR) |
				APMCU_SET_UUID(APMCU_SCMI_UUID_PMSR) |
				APMCU_SET_MG);
	int ret;
	int i;
	unsigned int index;
	struct pmsr_tool_mon_results *pmsr_tool_val;

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= MTK_PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	if (!tinfo)
		return -EFAULT;
#endif

	if ((void *)v == (void *)&cfg.enable) {
		if (cfg.enable == true) {
			/* pmsr get sspm sram address */
			pmsr_get_sspm_sram();

			if (!cfg.pmsr_tool_share_results || !cfg.share_buf)
				return -EFAULT;

			index = cfg.pmsr_tool_share_results->oldest_idx;
			pmsr_tool_val = &cfg.share_buf[index];

			for (i = 0 ; i < cfg.pmsr_sig_count; i++) {
				if (cfg.pmsr_signal_id[i] != 0xFFFFFFFF)
					pmsr_tool_val->results[i] = cfg.pmsr_signal_id[i];
			}

			/* pass the channel setting */
			for (i = 0 ; i < PMSR_MET_CH; i++) {
				if (cfg.ch[i].dpmsr_id == 0xFFFFFFFF)
					continue;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_SIGNAL));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.ch[i].dpmsr_id;
				pmsr_scmi_set_data.in3 = cfg.ch[i].signal_id;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1,
					pmsr_scmi_set_data.in2,
					pmsr_scmi_set_data.in3, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_SIGNAL);
#endif
			}

			/* pass the window length */
			if (cfg.pmsr_sample_rate != 0) {
				timer_window_len = cfg.pmsr_sample_rate;
				cfg.pmsr_window_len = 0;
			}
			if (cfg.pmsr_window_len != 0) {
				timer_window_len = cfg.pmsr_window_len;
				cfg.pmsr_sample_rate = 0;
			}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_WINDOW));
			pmsr_scmi_set_data.in1 = cfg.pmsr_window_len;
			pmsr_scmi_set_data.in2 = cfg.pmsr_speed_mode;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				pmsr_scmi_set_data.in1,
				pmsr_scmi_set_data.in2, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_WINDOW);
#endif

			/* pass the signum */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_SIGNUM));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].signum;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1,
					pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_SIGNUM);
#endif
			}
			/* pass the seltype */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_SELTYPE));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].seltype;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1, pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_SELTYPE);
#endif
			}
			/* pass the montype */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_MONTYPE));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].montype;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1, pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_MONTYPE);
#endif
			}
			/* pass the en */
			for (i = 0; i < cfg.dpmsr_count; i++) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_set_data.user_info =
					(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_EN));
				pmsr_scmi_set_data.in1 = i;
				pmsr_scmi_set_data.in2 = cfg.dpmsr[i].en;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_set_data.user_info,
					pmsr_scmi_set_data.in1, pmsr_scmi_set_data.in2,
					0, 0);
				if (ret)
					cfg.err |= (1 << PMSR_TOOL_ACT_EN);
#endif
			}
			/* pass the prof_cnt */
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_PROF_CNT_EN));
			pmsr_scmi_set_data.in1 = cfg.prof_cnt;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				pmsr_scmi_set_data.in1, 0, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_PROF_CNT_EN);

			/* pass the enable command */
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_ENABLE));
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				0, 0, 0, 0);
#endif
			if (!ret) {
				hrtimer_start(&pmsr_timer,
						ns_to_ktime(timer_window_len * NSEC_PER_USEC),
						HRTIMER_MODE_REL_PINNED);
			} else {
				cfg.err |= (1 << PMSR_TOOL_ACT_ENABLE);
			}
		} else {
			pmsr_cfg_init();
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_DISABLE));
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info,
				0, 0, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_DISABLE);
#endif
			hrtimer_try_to_cancel(&pmsr_timer);
		}
	} else if ((void *)v == (void *)&cfg.test) {
		if (cfg.test == 1) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			if (!cfg.pmsr_tool_share_results || !cfg.share_buf)
				return -EFAULT;

			pmsr_scmi_set_data.user_info =
				(user_info | APMCU_SET_ACT(PMSR_TOOL_ACT_TEST));
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_set_data.user_info, 0, 0, 0, 0);
			if (ret)
				cfg.err |= (1 << PMSR_TOOL_ACT_TEST);
			else
				queue_delayed_work(pmsr_tool_forcereq, &pmsr_tool_forcereq_work, 0);
#endif
		}
	}
	return count;
}
static const struct proc_ops remote_data_fops = {
	.proc_read = remote_data_read,
	.proc_write = remote_data_write,
};

static char dbgbuf[4096] = {0};
#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))
#undef log
#define log(fmt, args...)   log2buf(p, dbgbuf, fmt, ##args)

static ssize_t local_ipi_read(struct file *fp, char __user *userbuf,
			      size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbgbuf;

	p[0] = '\0';

	log("pmsr state:\n");
	log("enable %d\n", cfg.enable ? 1 : 0);
	log("speed_mode %u\n", cfg.pmsr_speed_mode);
	log("prof cnt %u\n", cfg.prof_cnt);
	log("clean_records %u\n", cfg.clean_records);
	log("window_len %u (0x%x)\n",
	    cfg.pmsr_window_len, cfg.pmsr_window_len);
	log("sample_rate %u (0x%x)\n",
	    cfg.pmsr_sample_rate, cfg.pmsr_sample_rate);

	for (i = 0; i < PMSR_MET_CH; i++) {
		if (cfg.ch[i].dpmsr_id < cfg.dpmsr_count)
			log("ch%d: dpmsr %u id %u\n",
			    i,
			    cfg.ch[i].dpmsr_id,
			    cfg.ch[i].signal_id);
		else
			log("ch%d: off\n", i);
	}

	for (i = 0; i < PMSR_MAX_SIG_CH; i++) {
		if (cfg.pmsr_signal_id[i] != 0xFFFFFFFF)
			log("sig%d: %u\n",
			    i,
			    cfg.pmsr_signal_id[i]);
		else
			continue;
	}
	for (i = 0; i < cfg.dpmsr_count; i++) {
		log("dpmsr %u seltype %u montype %u (%s) signum %u en %u\n",
				i,
				cfg.dpmsr[i].seltype,
				cfg.dpmsr[i].montype,
				cfg.dpmsr[i].montype == 0 ? "rising" :
				cfg.dpmsr[i].montype == 1 ? "falling" :
				cfg.dpmsr[i].montype == 2 ? "high level" :
				cfg.dpmsr[i].montype == 3 ? "low level" :
				"unknown",
				cfg.dpmsr[i].signum,
				cfg.dpmsr[i].en);
	}
	log("err 0x%x\n", cfg.err);

	len = p - dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, dbgbuf, len);
}

static ssize_t local_ipi_write(struct file *fp, const char __user *userbuf,
			       size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= MTK_PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	return count;
}

static const struct proc_ops local_ipi_fops = {
	.proc_read = local_ipi_read,
	.proc_write = local_ipi_write,
};

static ssize_t local_sram_read(struct file *fp, char __user *userbuf,
			      size_t count, loff_t *f_pos)
{
	int i, len = 0, read_idx;
	char *p = dbgbuf;
	struct pmsr_tool_mon_results *pmsr_tool_val;


	for (i = 0; i < cfg.pmsr_tool_buffer_max_space; i++) {
		read_idx =
			(cfg.pmsr_tool_share_results->oldest_idx + i)
				% cfg.pmsr_tool_buffer_max_space;

		if (cfg.share_buf) {
			pmsr_tool_val = &cfg.share_buf[read_idx];
			if (pmsr_tool_val->time_stamp != pmsr_tool_last_time_stamp[read_idx]) {
				log("[%u] dpmsr %u(%u): ",
					pmsr_tool_val->time_stamp, cfg.dpmsr_count-1,
					pmsr_tool_val->winlen);

				log("%u, %u, %u, %u, %u, %u, %u, %u\n",
					pmsr_tool_val->results[0],
					pmsr_tool_val->results[1],
					pmsr_tool_val->results[2],
					pmsr_tool_val->results[3],
					pmsr_tool_val->results[4],
					pmsr_tool_val->results[5],
					pmsr_tool_val->results[6],
					pmsr_tool_val->results[7]);

				if (cfg.clean_records != 0) {
					pmsr_tool_last_time_stamp[read_idx] =
						pmsr_tool_val->time_stamp;
				}
			}
		}
	}

	len = p - dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, dbgbuf, len);
}

static ssize_t local_sram_write(struct file *fp, const char __user *userbuf,
			       size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= MTK_PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	return count;
}
static const struct proc_ops local_sram_fops = {
	.proc_read = local_sram_read,
	.proc_write = local_sram_write,
};

static ssize_t local_signal_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	unsigned int i;
	int len = 0;
	char *p = dbgbuf;

	p[0] = '\0';

	log("[%u] dpmsr %u(%llu): ",
		ACC_RESULTS.time_stamp, cfg.dpmsr_count-1, ACC_RESULTS.winlen);

	for (i = 0; i < PMSR_MAX_SIG_CH - 1; i++)
		log("%llu, ", ACC_RESULTS.results[i]);

	log("%llu in %u\n", ACC_RESULTS.results[PMSR_MAX_SIG_CH - 1], ACC_RESULTS.acc_num);
	len = p - dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, dbgbuf, len);
}

static ssize_t local_signal_write(struct file *fp, const char *userbuf,
			       size_t count, loff_t *f_pos)
{
	int ret = -EINVAL;
	unsigned int v = 0;

	if (!userbuf)
		return -EINVAL;

	if (count >= MTK_PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, &v))
		return -EFAULT;

	if (cfg.pmsr_sig_count < PMSR_MAX_SIG_CH) {
		cfg.pmsr_signal_id[cfg.pmsr_sig_count] = v;
		ret = count;
		cfg.pmsr_sig_count++;
	}

	return ret;
}

static const struct proc_ops local_signal_fops = {
	.proc_read = local_signal_read,
	.proc_write = local_signal_write,
};

static struct proc_dir_entry *pmsr_droot;

static int pmsr_procfs_init(void)
{
	int i;
	struct proc_dir_entry *ch;
	struct proc_dir_entry *dpmsr_dir_entry;

	pmsr_cfg_init();

	pmsr_droot = proc_mkdir("pmsr", NULL);
	if (pmsr_droot) {
		proc_create("state", 0644, pmsr_droot, &local_ipi_fops);
		proc_create_data("monitor_results", 0644, pmsr_droot, &local_sram_fops,
				 (void *) &(cfg.clean_records));
		proc_create_data("speed_mode", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_speed_mode));
		proc_create_data("window_len", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_window_len));
		proc_create_data("prof_cnt", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.prof_cnt));
		proc_create_data("sample_rate", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_sample_rate));
		proc_create_data("enable", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.enable));
		proc_create_data("test", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.test));
		proc_create("signal", 0644, pmsr_droot, &local_signal_fops);

		for (i = 0 ; i < PMSR_MET_CH; i++) {
			ch = proc_mkdir(ch_name[i], pmsr_droot);

			if (ch) {
				proc_create_data("dpmsr_id",
						 0644, ch, &local_ipi_fops,
						 (void *)&(cfg.ch[i].dpmsr_id));
				proc_create_data("signal_id",
						 0644, ch, &local_ipi_fops,
						 (void *)&(cfg.ch[i].signal_id));
			}
		}

		/* If cfg.dpmsr is NULL, return -1 since the memory allocation
		 * might go wrong before this step
		 */
		if (!cfg.dpmsr)
			return -1;

		for (i = 0 ; i < cfg.dpmsr_count; i++) {
			char name[10];
			int len = 0;

			len = snprintf(name, sizeof(name), "dpmsr%d", i);
			/* len should be less than the size of name[] */
			if (len >= sizeof(name))
				pr_notice("dpmsr name fail\n");

			dpmsr_dir_entry = proc_mkdir(name, pmsr_droot);
			if (dpmsr_dir_entry) {
				proc_create_data("seltype",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].seltype));
				proc_create_data("montype",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].montype));
				proc_create_data("signum",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].signum));
				proc_create_data("en",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].en));
			}
		}
	}

	return 0;
}

static enum hrtimer_restart pmsr_timer_handle(struct hrtimer *timer)
{
	hrtimer_forward(timer, timer->base->get_time(),
			ns_to_ktime(timer_window_len * NSEC_PER_USEC));

	queue_delayed_work(pmsr_tool_forcereq, &pmsr_tool_forcereq_work, 0);

	return HRTIMER_RESTART;
}

static int __init pmsr_parsing_nodes(void)
{
	struct device_node *pmsr_node = NULL;
	char pmsr_desc[] = "mediatek,mtk-pmsr";
	int ret = 0;

	/* find the root node in dts */
	pmsr_node = of_find_compatible_node(NULL, NULL, pmsr_desc);
	if (!pmsr_node) {
		pr_notice("unable to find pmsr device node\n");
		return -1;
	}

	/* find the node and get the value */
	ret = of_property_read_u32_index(pmsr_node, "dpmsr-count",
			0, &cfg.dpmsr_count);
	if (ret) {
		pr_notice("fail to get dpmsr_count from dts\n");
		return -1;
	}
	return 0;
}

static int __init pmsr_init(void)
{

	int ret;

	ret = pmsr_parsing_nodes();
	/* if an error occurs while parsing the nodes, return directly */
	if (ret)
		return 0;

	/* allocate a memory for dpmsr configurations */
	cfg.dpmsr = kmalloc_array(cfg.dpmsr_count,
					sizeof(struct pmsr_dpmsr_cfg), GFP_KERNEL);

	/* create debugfs node */
	pmsr_procfs_init();

	/* register ipi for AP2SSPM communication */
	pmsr_ipi_init();

	hrtimer_init(&pmsr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pmsr_timer.function = pmsr_timer_handle;

	if (!pmsr_tool_forcereq) {
		pmsr_tool_forcereq = alloc_workqueue("pmsr_tool_forcereq", WQ_HIGHPRI, 0);

		if (!pmsr_tool_forcereq)
			pr_notice("pmsr tool forcereq workqueue fail\n");
	}

	INIT_DELAYED_WORK(&pmsr_tool_forcereq_work, pmsr_tool_send_forcereq);

	return 0;
}

module_init(pmsr_init);

static void __exit pmsr_exit(void)
{
	/* remove debugfs node */
	pmsr_procfs_exit();
	kfree(cfg.dpmsr);
	hrtimer_try_to_cancel(&pmsr_timer);
	kfree(pmsr_tool_last_time_stamp);

	flush_workqueue(pmsr_tool_forcereq);
	destroy_workqueue(pmsr_tool_forcereq);
}

module_exit(pmsr_exit);

MODULE_DESCRIPTION("Mediatek MT68XX pmsr driver");
MODULE_AUTHOR("SHChen <Show-Hong.Chen@mediatek.com>");
MODULE_LICENSE("GPL");
