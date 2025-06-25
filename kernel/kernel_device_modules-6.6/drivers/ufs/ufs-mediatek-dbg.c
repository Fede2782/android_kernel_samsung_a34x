// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 */
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/sched/cputime.h>
#include <linux/sched/debug.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/tracepoint.h>
#include <sched/sched.h>
#include "governor.h"
#include "ufshcd-priv.h"
#include <ufs/ufshcd.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"

/* For bus hang issue debugging */
#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
#include "../../clk/mediatek/clk-fmeter.h"
#endif

#define MAX_CMD_HIST_ENTRY_CNT (500)
#define UFS_AEE_BUFFER_SIZE (100 * 1024)

/*
 * Currently only global variables are used.
 *
 * For scalability, may introduce multiple history
 * instances bound to each device in the future.
 */
static bool cmd_hist_initialized;
static bool cmd_hist_enabled;
static spinlock_t cmd_hist_lock;
static unsigned int cmd_hist_cnt;
static unsigned int cmd_hist_ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
static struct cmd_hist_struct *cmd_hist;
static struct ufs_hba *ufshba;
static char *ufs_aee_buffer;

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

extern void mt_irq_dump_status(unsigned int irq);

void ufs_mtk_eh_abort(unsigned int tag)
{
	static int ufs_abort_aee_count;

	if (!ufs_abort_aee_count) {
		ufs_abort_aee_count++;
		ufs_mtk_dbg_cmd_hist_disable();
		ufs_mtk_aee_warning("ufshcd_abort at tag %d", tag);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_eh_abort);

void ufs_mtk_eh_unipro_set_lpm(struct ufs_hba *hba, int ret)
{
	int ret2, val = 0;

	/* Check if irq is pending */
	mt_irq_dump_status(hba->irq);

	ret2 = ufshcd_dme_get(hba,
		UIC_ARG_MIB(VS_UNIPROPOWERDOWNCONTROL), &val);
	if (!ret2) {
		dev_info(hba->dev, "%s: Read 0xD0A8 val=%d\n",
			 __func__, val);
	}

	ufs_mtk_aee_warning(
		"Set 0xD0A8 timeout, ret=%d, ret2=%d, 0xD0A8=%d",
		ret, ret2, val);
}
EXPORT_SYMBOL_GPL(ufs_mtk_eh_unipro_set_lpm);

void ufs_mtk_eh_err_cnt(void)
{
	static ktime_t err_ktime;
	ktime_t delta_ktime;
	s64 delta_msecs;

	delta_ktime = ktime_sub(local_clock(), err_ktime);
	delta_msecs = ktime_to_ms(delta_ktime);

	/* If last error happen more than 72 hrs, clear error count */
	/* Treat errors happen in 3000 ms as one time error */
	if (delta_msecs >= 3000) {
		err_ktime = local_clock();
		ufs_mtk_aee_warning("UIC Error");
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_eh_err_cnt);

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
static void __iomem *reg_ufscfg_ao;
static void __iomem *reg_ufscfg_pdn;
static void __iomem *reg_vlp_cfg;
static void __iomem *reg_ifrbus_ao;
static void __iomem *reg_pericfg_ao;
static void __iomem *reg_topckgen;

void ufs_mtk_check_bus_init(u32 ip_ver)
{
	if (ip_ver == IP_VER_MT6991_A0 ||
		ip_ver == IP_VER_MT6991_B0) {
		if (reg_ufscfg_ao == NULL)
			reg_ufscfg_ao = ioremap(0x168A0000, 0x204);
	} else if (ip_ver == IP_VER_NONE) {
		if (reg_ufscfg_ao == NULL)
			reg_ufscfg_ao = ioremap(0x112B8000, 0xCC);

		if (reg_ufscfg_pdn == NULL)
			reg_ufscfg_pdn = ioremap(0x112BB000, 0xB0);

		if (reg_vlp_cfg == NULL)
			reg_vlp_cfg = ioremap(0x1C00C000, 0x930);

		if (reg_ifrbus_ao == NULL)
			reg_ifrbus_ao = ioremap(0x1002C000, 0xB00);

		if (reg_pericfg_ao == NULL)
			reg_pericfg_ao = ioremap(0x11036000, 0x2A8);

		if (reg_topckgen == NULL)
			reg_topckgen = ioremap(0x10000000, 0x500);

		pr_info("%s: init done\n", __func__);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_check_bus_init);

/* only for IP_VER_MT6897 */
#define FM_U_FAXI_CK		3
#define FM_U_CK		44

void ufs_mtk_check_bus_status(struct ufs_hba *hba)
{
	void __iomem *reg;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host->ip_ver == IP_VER_MT6991_A0 ||
		host->ip_ver == IP_VER_MT6991_B0) {
		if (reg_ufscfg_ao == NULL)
			return;

		/* Read UFSCFG_AO reg to detect bus hang*/
		reg = reg_ufscfg_ao + 0x180;
		if (readl(reg) != 0x10) {
			pr_err("%s: UFS2PERI_AXI off\n", __func__);
			BUG_ON(1);
		}
	} else if (host->ip_ver == IP_VER_NONE) {
		/* Check ufs clock: ufs_axi_ck and ufs_ck */
		if (mt_get_fmeter_freq(FM_U_CK, CKGEN) == 0) {
			pr_err("%s: hf_fufs_ck off\n", __func__);
			BUG_ON(1);
		}

		if (mt_get_fmeter_freq(FM_U_FAXI_CK, CKGEN) == 0) {
			pr_err("%s: hf_fufs_faxi_ck off\n", __func__);
			BUG_ON(1);
		}

		if ((reg_ufscfg_ao == NULL) || (reg_ufscfg_pdn == NULL) ||
		    (reg_vlp_cfg == NULL) || (reg_ifrbus_ao == NULL) ||
		    (reg_pericfg_ao == NULL) || (reg_topckgen == NULL))
			return;
		/*
		 * bus protect setting:
		 * UFS_AO2FE_SLPPROT_EN 0x112B8050[0] = 0
		 * VLP_TOPAXI_PROTECTEN 0x1C00C210[8:6] = 0
		 * PERISYS_PROTECT_EN 0x1002C0E0[0] = 0
		 * PERISYS_PROTECT_EN 0x1002C0E0[5:4] = 0
		 */
		reg = reg_ufscfg_ao + 0x50;
		if ((readl(reg) & 0x1) != 0) {
			pr_err("%s: UFS_AO2FE_SLPPROT_EN = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_vlp_cfg + 0x210;
		if ((readl(reg) & 0x1C0) != 0) {
			pr_err("%s: VLP_TOPAXI_PROTECT_EN = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_ifrbus_ao + 0xE0;
		if ((readl(reg) & 0x31) != 0) {
			pr_err("%s: PERISYS_PROTECT_EN_STA_0 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		/*
		 * cg setting:
		 * PERI_CG_1 0x11036014[22] = 0
		 * UFS_PDN_CG_0 0x112BB004[0] = 0
		 * UFS_PDN_CG_0 0x112BB004[3] = 0
		 * UFS_PDN_CG_0 0x112BB004[5] = 0
		 * CLK_CFG_0 0X10000010[9:8] = 2'b10
		 * CLK_CFG_0 0X10000010[12] = 1'b0
		 * CLK_CFG_0 0X10000010[15] = 1'b0
		 * CLK_CFG_0 0X10000010[17:16] = 2'b01
		 * CLK_CFG_0 0X10000010[20] = 1'b0
		 * CLK_CFG_0 0X10000010[23] = 1'b0
		 * CLK_CFG_10 0X100000B0[28] = 1'b0
		 * CLK_CFG_10 0X100000B0[31] = 1'b0
		 */
		reg = reg_pericfg_ao + 0x14;
		if (((readl(reg) >> 22) & 0x1) != 0) {
			pr_err("%s: PERI_CG_1 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_ufscfg_pdn + 0x4;
		if ((readl(reg) & 0x29) != 0) {
			pr_err("%s: UFS_PDN_CG_0 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_topckgen + 0x10;
		if ((readl(reg) & 0x939300) != 0x10200) {
			pr_err("%s: CLK_CFG_0 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_topckgen + 0xB0;
		if (((readl(reg) >> 28) & 0x9) != 0) {
			pr_err("%s: CLK_CFG_10 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_check_bus_status);
#endif

static void ufs_mtk_dbg_print_err_hist(char **buff, unsigned long *size,
				  struct seq_file *m, u32 id,
				  char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;
	struct ufs_hba *hba = ufshba;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		SPREAD_PRINTF(buff, size, m,
			"%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		SPREAD_PRINTF(buff, size, m, "No record of %s\n", err_name);
}

static void ufs_mtk_dbg_print_info(char **buff, unsigned long *size,
			    struct seq_file *m)
{
	struct ufs_mtk_host *host;
	struct ufs_hba *hba = ufshba;

	if (!hba)
		return;

	host = ufshcd_get_variant(hba);

	/* Host state */
	SPREAD_PRINTF(buff, size, m,
		      "UFS Host state=%d\n", hba->ufshcd_state);
	SPREAD_PRINTF(buff, size, m,
		      "outstanding reqs=0x%lx tasks=0x%lx\n",
		      hba->outstanding_reqs, hba->outstanding_tasks);
	SPREAD_PRINTF(buff, size, m,
		      "saved_err=0x%x, saved_uic_err=0x%x\n",
		      hba->saved_err, hba->saved_uic_err);
	SPREAD_PRINTF(buff, size, m,
		      "Device power mode=%d, UIC link state=%d\n",
		      hba->curr_dev_pwr_mode, hba->uic_link_state);
	SPREAD_PRINTF(buff, size, m,
		      "PM in progress=%d, sys. suspended=%d\n",
		      hba->pm_op_in_progress, hba->is_sys_suspended);
	SPREAD_PRINTF(buff, size, m,
		      "Auto BKOPS=%d, Host self-block=%d\n",
		      hba->auto_bkops_enabled,
		      hba->host->host_self_blocked);
	SPREAD_PRINTF(buff, size, m,
		      "Clk scale sup./en.=%d/%d, suspend sts/cnt=%d/%d, active_reqs=%d, min/max g.=G%d/G%d, polling_ms=%d, upthr=%d, downthr=%d\n",
		    !!ufshcd_is_clkscaling_supported(hba),
			hba->clk_scaling.is_enabled,
			hba->clk_scaling.is_suspended,
			(hba->devfreq ? atomic_read(&hba->devfreq->suspend_count) : 0xFF),
			hba->clk_scaling.active_reqs,
			hba->clk_scaling.min_gear,
			hba->clk_scaling.saved_pwr_info.gear_rx,
			hba->vps->devfreq_profile.polling_ms,
			hba->vps->ondemand_data.upthreshold,
			hba->vps->ondemand_data.downdifferential);
	if (ufshcd_is_clkgating_allowed(hba))
		SPREAD_PRINTF(buff, size, m,
			      "Clk gate=%d, suspended=%d, active_reqs=%d\n",
			      hba->clk_gating.state,
			      hba->clk_gating.is_suspended,
			      hba->clk_gating.active_reqs);
	else
		SPREAD_PRINTF(buff, size, m,
			      "clk_gating is disabled\n");
	if (host->mclk.reg_vcore && !in_interrupt() && !irqs_disabled()) {
		SPREAD_PRINTF(buff, size, m,
			      "Vcore = %d uv\n",
			      regulator_get_voltage(host->mclk.reg_vcore));
	} else {
		SPREAD_PRINTF(buff, size, m,
			      "Vcore = ? uv, in_interrupt:%ld, irqs_disabled:%d\n",
			      in_interrupt(), irqs_disabled());
	}
#ifdef CONFIG_PM
	SPREAD_PRINTF(buff, size, m,
		      "Runtime PM: req=%d, status:%d, err:%d\n",
		      hba->dev->power.request,
		      hba->dev->power.runtime_status,
		      hba->dev->power.runtime_error);
#endif
	SPREAD_PRINTF(buff, size, m,
		      "error handling flags=0x%x, req. abort count=%d\n",
		      hba->eh_flags, hba->req_abort_count);
	SPREAD_PRINTF(buff, size, m,
		      "Host capabilities=0x%x, hba-caps=0x%x, mtk-caps:0x%x\n",
		      hba->capabilities, hba->caps, host->caps);
	SPREAD_PRINTF(buff, size, m,
		      "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		      hba->dev_quirks);
	SPREAD_PRINTF(buff, size, m,
		      "ver. host=0x%x, dev=0x%x\n", hba->ufs_version, hba->dev_info.wspecversion);
	SPREAD_PRINTF(buff, size, m,
		      "last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt = %d\n",
		      ktime_to_us(hba->ufs_stats.last_hibern8_exit_tstamp),
		      hba->ufs_stats.hibern8_exit_cnt);
	/* PWR info */
	SPREAD_PRINTF(buff, size, m,
		      "[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%d, %d], rate = %d\n",
		      hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		      hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		      hba->pwr_info.pwr_rx,
		      hba->pwr_info.pwr_tx,
		      hba->pwr_info.hs_rate);

	if (hba->ufs_device_wlun) {
		/* Device info */
		SPREAD_PRINTF(buff, size, m,
			      "Device vendor=%.8s, model=%.16s, rev=%.4s\n",
			      hba->ufs_device_wlun->vendor,
			      hba->ufs_device_wlun->model, hba->ufs_device_wlun->rev);
	}
	SPREAD_PRINTF(buff, size, m,
		      "MCQ sup./en.: %d/%d, nr_hw_queues=%d\n",
		      hba->mcq_sup, hba->mcq_enabled, hba->nr_hw_queues);
	SPREAD_PRINTF(buff, size, m,
		      "nutrs=%d, dev_info.bqueuedepth=%d\n",
		      hba->nutrs, hba->dev_info.bqueuedepth);

	/* Error history */
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_PA_ERR, "pa_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_DL_ERR, "dl_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_NL_ERR, "nl_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_TL_ERR, "tl_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_DME_ERR, "dme_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_AUTO_HIBERN8_ERR,
			      "auto_hibern8_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_FATAL_ERR, "fatal_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_LINK_STARTUP_FAIL,
			      "link_startup_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_RESUME_ERR, "resume_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_SUSPEND_ERR, "suspend_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_WL_RES_ERR, "wlun resume_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_WL_SUSP_ERR, "wlun suspend_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_DEV_RESET, "dev_reset");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_HOST_RESET, "host_reset");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_ABORT, "task_abort");
}

static int cmd_hist_get_entry(void)
{
	unsigned long flags;
	unsigned int ptr;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_ptr++;
	if (cmd_hist_ptr >= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_ptr = 0;
	ptr = cmd_hist_ptr;

	cmd_hist_cnt++;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	/* Initialize common fields */
	cmd_hist[ptr].cpu = smp_processor_id();
	cmd_hist[ptr].duration = 0;
	cmd_hist[ptr].pid = current->pid;
	cmd_hist[ptr].time = local_clock();

	return ptr;
}

static int cmd_hist_get_prev_ptr(int ptr)
{
	if (ptr == 0)
		return MAX_CMD_HIST_ENTRY_CNT - 1;
	else
		return (ptr - 1);
}

static void probe_android_vh_ufs_send_tm_command(void *data, struct ufs_hba *hba,
						 int tag, int str_t)
{
	u8 tm_func;
	int ptr, lun, task_tag;
	enum cmd_hist_event event = CMD_UNKNOWN;
	enum ufs_trace_str_t _str_t = str_t;
	struct utp_task_req_desc *d = &hba->utmrdl_base_addr[tag];

	if (!cmd_hist_enabled)
		return;

	lun = (be32_to_cpu(d->upiu_req.req_header.dword_0) >> 8) & 0xFF;
	task_tag = be32_to_cpu(d->upiu_req.input_param2);
	tm_func = (be32_to_cpu(d->upiu_req.req_header.dword_1) >> 16) & 0xFFFF;

	switch (_str_t){
	case UFS_TM_SEND:
		event = CMD_TM_SEND;
		break;
	case UFS_TM_COMP:
		event = CMD_TM_COMPLETED;
		break;
	case UFS_TM_ERR:
		event = CMD_TM_COMPLETED_ERR;
		break;
	default:
		pr_notice("%s: undefined TM command (0x%x)", __func__, _str_t);
		break;
	}

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.tm.lun = lun;
	cmd_hist[ptr].cmd.tm.tag = tag;
	cmd_hist[ptr].cmd.tm.task_tag = task_tag;
	cmd_hist[ptr].cmd.tm.tm_func = tm_func;
}

static void cmd_hist_add_dev_cmd(struct ufs_hba *hba,
				 struct ufshcd_lrb *lrbp,
				 enum cmd_hist_event event)
{
	int ptr;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.dev.type = hba->dev_cmd.type;

	if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
		return;

	cmd_hist[ptr].cmd.dev.tag = lrbp->task_tag;
	cmd_hist[ptr].cmd.dev.opcode =
		hba->dev_cmd.query.request.upiu_req.opcode;
	cmd_hist[ptr].cmd.dev.idn =
		hba->dev_cmd.query.request.upiu_req.idn;
	cmd_hist[ptr].cmd.dev.index =
		hba->dev_cmd.query.request.upiu_req.index;
	cmd_hist[ptr].cmd.dev.selector =
		hba->dev_cmd.query.request.upiu_req.selector;
}

static void probe_android_vh_ufs_send_command(void *data, struct ufs_hba *hba,
					      struct ufshcd_lrb *lrbp)
{
	if (!cmd_hist_enabled)
		return;

	if (lrbp->cmd)
		return;

	cmd_hist_add_dev_cmd(hba, lrbp, CMD_DEV_SEND);
}

static void probe_android_vh_ufs_compl_command(void *data, struct ufs_hba *hba,
					      struct ufshcd_lrb *lrbp)
{
	if (!cmd_hist_enabled)
		return;

	if (lrbp->cmd)
		return;

	cmd_hist_add_dev_cmd(hba, lrbp, CMD_DEV_COMPLETED);
}

static void probe_ufshcd_command(void *data, const char *dev_name,
				 enum ufs_trace_str_t str_t, unsigned int tag,
				 u32 doorbell, u32 hwq_id, int transfer_len,
				 u32 intr, u64 lba, u8 opcode, u8 group_id)
{
	int ptr, ptr_cur;
	enum cmd_hist_event event;

	if (!cmd_hist_enabled)
		return;

	if (str_t == UFS_CMD_SEND)
		event = CMD_SEND;
	else if (str_t == UFS_CMD_COMP)
		event = CMD_COMPLETED;
	else
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.utp.tag = tag;
	cmd_hist[ptr].cmd.utp.transfer_len = transfer_len;
	cmd_hist[ptr].cmd.utp.lba = lba;
	cmd_hist[ptr].cmd.utp.opcode = opcode;
	cmd_hist[ptr].cmd.utp.doorbell = doorbell;
	cmd_hist[ptr].cmd.utp.intr = intr;

	/* Need patch trace_ufshcd_command() first */
	cmd_hist[ptr].cmd.utp.crypt_en = 0;
	cmd_hist[ptr].cmd.utp.crypt_keyslot = 0;

	if (event == CMD_COMPLETED) {
		ptr_cur = ptr;
		ptr = cmd_hist_get_prev_ptr(ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.utp.tag == tag) {
				cmd_hist[ptr_cur].duration =
					local_clock() - cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (ptr == ptr_cur)
				break;
		}
	}
}

static void probe_ufshcd_uic_command(void *data, const char *dev_name,
				     enum ufs_trace_str_t str_t, u32 cmd,
				     u32 arg1, u32 arg2, u32 arg3)
{
	int ptr, ptr_cur;
	enum cmd_hist_event event;

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	if (str_t == UFS_CMD_SEND)
		event = CMD_UIC_SEND;
	else
		event = CMD_UIC_CMPL_GENERAL;

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.uic.cmd = cmd;
	cmd_hist[ptr].cmd.uic.arg1 = arg1;
	cmd_hist[ptr].cmd.uic.arg2 = arg2;
	cmd_hist[ptr].cmd.uic.arg3 = arg3;

	if (event == CMD_UIC_CMPL_GENERAL) {
		ptr_cur = ptr;
		ptr = cmd_hist_get_prev_ptr(ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.uic.cmd == cmd) {
				cmd_hist[ptr_cur].duration =
					local_clock() - cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (ptr == ptr_cur)
				break;
		}
	}
}

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
#define NUM_OF_MON	14
#define MON_DEPTH	4
#define W_PT_BITS	0x3
#define W_PT_OFFSET	22
#define W_PT_MASK	(W_PT_BITS << W_PT_OFFSET)

static struct mon_struct mon_info[NUM_OF_MON] = {
	{REG_UFS_NOPOUT_MON, "NOP OUT"},
	{REG_UFS_NOPIN_MON, "NOP IN"},
	{REG_UFS_COMMAND_MON, "COMMAND"},
	{REG_UFS_RESP_MON, "RESPONSE"},
	{REG_UFS_DATAOUT_MON, "DATA OUT"},
	{REG_UFS_DATAIN_MON, "DATA IN"},
	{REG_UFS_TMREQ_MON, "TM REQUEST"},
	{REG_UFS_TMRESP_MON, "TM RESPONSE"},
	{REG_UFS_RTT_MON, "RTT"},
	{REG_UFS_QUERYREQ_MON, "QUERY REQUEST"},
	{REG_UFS_QUERYRESP_MON, "QUERY RESPONSE"},
	{REG_UFS_REJECT_MON, "REJECT"},
	{REG_UFS_AH8E_MON, "H8 ENTER"},
	{REG_UFS_AH8X_MON, "H8 EXIT"},
};

void ufs_mtk_mon_dump(struct ufs_hba *hba)
{
	u8 i, j, sort[MON_DEPTH];
	u32 mon_w_pt[NUM_OF_MON], w_pt;
	u32 mon_data[NUM_OF_MON][MON_DEPTH];

	/* Clear buffer */
	memset(mon_data, 0, sizeof(u32) * NUM_OF_MON * MON_DEPTH);

	dev_info(hba->dev, "format: LLTTtttt (LUN/TAG/timestamp in us\n");
	dev_info(hba->dev, "%16s    %8s %8s %8s %8s", "", "OLDEST", "", "", "LATEST");

	/* Fetch data in a batch */
	for (i = 0; i < NUM_OF_MON; i++) {
		for (j = 0; j < MON_DEPTH; j++) {
			mon_data[i][j] = ufshcd_readl(hba, mon_info[i].offset);
			w_pt = (mon_data[i][j] >> W_PT_OFFSET) & W_PT_BITS;

			/* Get the current write pointer, and alert if not all write pointers are the same */
			if (j == 0)
				mon_w_pt[i] = w_pt;
			else if (mon_w_pt[i] != w_pt) {
				dev_info(hba->dev, "w_pt[%d][0] = %x, w_pt[%d][%d] = %x\n",
					i, mon_w_pt[i], i, j, w_pt);
			}
		}

		/*
		 * Reorder according to the current write pointer
		 * the oldest record is listed first; the latest, last
		 */
		for (j = 0; j < MON_DEPTH; j++)
			sort[j] = (j + mon_w_pt[i]) % MON_DEPTH;

		dev_info(hba->dev, "%16s || %08x %08x %08x %08x",
			mon_info[i].name,
			mon_data[i][sort[0]] &~ W_PT_MASK,
			mon_data[i][sort[1]] &~ W_PT_MASK,
			mon_data[i][sort[2]] &~ W_PT_MASK,
			mon_data[i][sort[3]] &~ W_PT_MASK
			);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_mon_dump);

void ufs_mtk_ahb_dump(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 val = ufshcd_readl(hba, REG_UFS_MMIO_DBG_AHB);

	if (host->ip_ver < IP_VER_MT6899 || host->legacy_ip_ver)
		return;

	dev_info(hba->dev, "=== INPUT ===\n");
	dev_info(hba->dev, "HTRANS(2) = 0x%x\n", (val >> 6) & 0x3);
	dev_info(hba->dev, "HADDR(32) = 0x%x\n", ufshcd_readl(hba, REG_UFS_MMIO_DBG_AHB_HADDR));
	dev_info(hba->dev, "HWRITE(1) = 0x%x\n", (val >> 8) & 0x1);
	dev_info(hba->dev, "HSIZE(3) = 0x%x\n", (val >> 9) & 0x7);
	dev_info(hba->dev, "HBURST(3) = 0x%x\n", (val >> 3) & 0x7);
	dev_info(hba->dev, "HSECUR_B(1) = 0x%x\n", (val >> 2) & 0x1);
	dev_info(hba->dev, "HREADY_IN(1) = 0x%x\n", (val >> 1) & 0x1);
	dev_info(hba->dev, "HWDATA(32) = 0x%x\n", ufshcd_readl(hba, REG_UFS_MMIO_DBG_AHB_HWDATA));
	dev_info(hba->dev, "=== OUTPUT ===\n");
	dev_info(hba->dev, "HREADY(1) = 0x%x\n", (val & 0x1));
	dev_info(hba->dev, "HRDATA(32) = 0x%x\n\n", ufshcd_readl(hba, REG_UFS_MMIO_DBG_AHB_HRDATA));

	val = ufshcd_readl(hba, REG_UFS_MMIO_DBG_NIT);
	dev_info(hba->dev, "=== INPUT ===\n");
	dev_info(hba->dev, "NIT_ADDR(16) = 0x%x\n", (val & 0xFFFF));
	dev_info(hba->dev, "NIT_WR(1) = 0x%x\n", ((val >> 16) & 0x1));
	dev_info(hba->dev, "NIT_WDATA(32) = 0x%x\n", ufshcd_readl(hba, REG_UFS_MMIO_DBG_NIT_WDATA));
	dev_info(hba->dev, "=== OUTPUT ===\n");
	dev_info(hba->dev, "NIT_RDATA(32) = 0x%x\n\n", ufshcd_readl(hba, REG_UFS_MMIO_DBG_NIT_RDATA));
}
EXPORT_SYMBOL_GPL(ufs_mtk_ahb_dump);

void ufs_mtk_axi_dump(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u64 sel_val[13] = {0};
	u32 i, val, reg;

	if (host->ip_ver < IP_VER_MT6897 || host->legacy_ip_ver)
		return;

	reg = REG_UFS_MMIO_DBG_AXIM;
	for (i = 0; i < ARRAY_SIZE(sel_val) * 2; i++) {
		val = (ufshcd_readl(hba, reg) & ~0xFF0000) | (i << 16);
		ufshcd_writel(hba, val, reg);
		val = ufshcd_readl(hba, reg) & 0xFFFF;

		if (i % 2 == 0)
			sel_val[i / 2] |= (u64)val;
		else
			sel_val[i / 2] |= (u64)val << 16;
	}

	/* mru_dbg_axim_aw */
	dev_info(hba->dev, "=== mru_dbg_axim_aw ===\n");
	dev_info(hba->dev, "AWID(2) = 0x%llx\n", sel_val[0] & 0x3);
	dev_info(hba->dev, "AWADDR(36) = 0x%llx\n", (sel_val[0] >> 2) |
						    ((sel_val[1] & 0x3F) << 30));
	dev_info(hba->dev, "AWLEN(8) = 0x%llx\n", (sel_val[1] >> 6) & 0xFF);
	dev_info(hba->dev, "AWSIZE(3) = 0x%llx\n", (sel_val[1] >> 14) & 0x7);
	dev_info(hba->dev, "AWBURST(2) = 0x%llx\n", (sel_val[1] >> 17) & 0x3);
	dev_info(hba->dev, "AWCACHE(4) = 0x%llx\n", (sel_val[1] >> 19) & 0xF);
	dev_info(hba->dev, "AWUSER(4) = 0x%llx\n\n", (sel_val[1] >> 23) & 0xF);

	/* mru_dbg_axim_w */
	dev_info(hba->dev, "=== mru_dbg_axim_w ===\n");
	dev_info(hba->dev, "WID(2) = 0x%llx\n", (sel_val[1] >> 27) & 0x3);
	dev_info(hba->dev, "WDATA(128)[63:0] = 0x%llx\n", (sel_val[1] >> 29) |
							  (sel_val[2] << 3) |
							  ((sel_val[3] & 0x3FFFFFFF) << 35));
	dev_info(hba->dev, "WDATA(128)[128:64] = 0x%llx\n", (sel_val[3] >> 29) |
							    (sel_val[4] << 3) |
							    ((sel_val[5] & 0x3FFFFFFF) << 35));
	dev_info(hba->dev, "WSTRB(16) = 0x%llx\n", (sel_val[5] >> 29) |
						   ((sel_val[6] & 0x1FFF) << 3));
	dev_info(hba->dev, "WLAST(1) = 0x%llx\n\n", (sel_val[6] >> 13) & 0x1);

	/* mru_dbg_axim_b */
	dev_info(hba->dev, "=== mru_dbg_axim_b ===\n");
	dev_info(hba->dev, "BID(2) = 0x%llx\n", (sel_val[6] >> 14) & 0x3);
	dev_info(hba->dev, "BERSP(2) = 0x%llx\n\n", (sel_val[6] >> 16) & 0x3);

	/* mru_dbg_axim_ar */
	dev_info(hba->dev, "=== mru_dbg_axim_ar ===\n");
	dev_info(hba->dev, "ARID(2) = 0x%llx\n", (sel_val[6] >> 18) & 0x3);
	dev_info(hba->dev, "ARADDR(36) = 0x%llx\n", (sel_val[6] >> 20) |
						    ((sel_val[7] & 0xFFFFFF) << 12));
	dev_info(hba->dev, "ARLEN(8) = 0x%llx\n", (sel_val[7] >> 24) & 0xFF);
	dev_info(hba->dev, "ARSIZE(3) = 0x%llx\n", sel_val[8] & 0x7);
	dev_info(hba->dev, "ARURST(2) = 0x%llx\n", (sel_val[8] >> 3) & 0x3);
	dev_info(hba->dev, "ARCACHE(4) = 0x%llx\n", (sel_val[8] >> 5) & 0xF);
	dev_info(hba->dev, "ARUSER(4) = 0x%llx\n\n", (sel_val[8] >> 9) & 0xF);

	/* mru_dbg_axim_r */
	dev_info(hba->dev, "=== mru_dbg_axim_r ===\n");
	dev_info(hba->dev, "RID(2) = 0x%llx\n", (sel_val[8] >> 13) & 0x3);
	dev_info(hba->dev, "RDATA(128)[63:0] = 0x%llx\n", (sel_val[8] >> 15) |
							  (sel_val[9] << 17) |
							  ((sel_val[10] & 0x7FFF) << 49));
	dev_info(hba->dev, "RDATA(128)[128:64] = 0x%llx\n", (sel_val[10] >> 15) |
							    (sel_val[11] << 17) |
							    ((sel_val[12] & 0x7FFF) << 49));
	dev_info(hba->dev, "RRESP(2) = 0x%llx\n", (sel_val[12] >> 15) & 0x3);
	dev_info(hba->dev, "RLAST(1) = 0x%llx\n\n", (sel_val[12] >> 17) & 0x1);
}
EXPORT_SYMBOL_GPL(ufs_mtk_axi_dump);

/* MPHY Debugging is for ENG/USERDEBUG builds only */
static u32 mphy_phys_base;
struct ufs_mtk_mphy_reg {
	const u32 reg;
	const u8 *str;
};
static const struct ufs_mtk_mphy_reg mphy_reg_list[] = {
	/* PHYD */
	{0xA09C, ""},  /* 0 */
	{0xA19C, ""},  /* 1 */

	{0x80C0, ""},  /* 2 */
	{0x81C0, ""},  /* 3 */
	{0xB010, ""},  /* 4 */
	{0xB010, ""},  /* 5 */
	{0xB010, ""},  /* 6 */
	{0xB010, ""},  /* 7 */
	{0xB010, ""},  /* 8 */
	{0xB110, ""},  /* 9 */
	{0xB110, ""},  /* 10 */
	{0xB110, ""},  /* 11 */
	{0xB110, ""},  /* 12 */
	{0xB110, ""},  /* 13 */
	{0xA0AC, ""},  /* 14 */
	{0xA0B0, ""},  /* 15 */
	{0xA09C, ""},  /* 16 */
	{0xA1AC, ""},  /* 17 */
	{0xA1B0, ""},  /* 18 */
	{0xA19C, ""},  /* 19 */

	{0x00B0, ""},  /* 20 */

	{0xA808, ""},  /* 21 */
	{0xA80C, ""},  /* 22 */
	{0xA810, ""},  /* 23 */
	{0xA814, ""},  /* 24 */
	{0xA818, ""},  /* 25 */
	{0xA81C, ""},  /* 26 */
	{0xA820, ""},  /* 27 */
	{0xA824, ""},  /* 28 */
	{0xA828, ""},  /* 29 */
	{0xA82C, ""},  /* 30 */
	{0xA830, ""},  /* 31 */
	{0xA834, ""},  /* 32 */
	{0xA838, ""},  /* 33 */
	{0xA83C, ""},  /* 34 */

	{0xA908, ""},  /* 35 */
	{0xA90C, ""},  /* 36 */
	{0xA910, ""},  /* 37 */
	{0xA914, ""},  /* 38 */
	{0xA918, ""},  /* 39 */
	{0xA91C, ""},  /* 40 */
	{0xA920, ""},  /* 41 */
	{0xA924, ""},  /* 42 */
	{0xA928, ""},  /* 43 */
	{0xA92C, ""},  /* 44 */
	{0xA930, ""},  /* 45 */
	{0xA934, ""},  /* 46 */
	{0xA938, ""},  /* 47 */
	{0xA93C, ""},  /* 48 */

	/* PHYA */
	{0x00B0, "ckbuf_en                                           "},  /* 49 */
	{0x00B0, "sq,imppl_en                                        "},  /* 50 */
	{0x00B0, "n2p_det,term_en                                    "},  /* 51 */
	{0x00B0, "cdr_en                                             "},  /* 52 */
	{0x00B0, "eq_vcm_en                                          "},  /* 53 */
	{0x00B0, "pi_edge_q_en                                       "},  /* 54 */
	{0x00B0, "fedac_en,eq_en,eq_ldo_en,dfe_clk_en                "},  /* 55 */
	{0x00B0, "dfe_clk_edge_sel,dfe_clk,des_en                    "},  /* 56 */
	{0x00B0, "des_en,cdr_ldo_en,comp_difp_en                     "},  /* 57 */
	{0x00B0, "cdr_ldo_en                                         "},  /* 58 */
	{0x00B0, "lck2ref                                            "},  /* 59 */
	{0x00B0, "freacq_en                                          "},  /* 60 */
	{0x00B0, "cdr_dig_en,auto_en                                 "},  /* 61 */
	{0x00B0, "bias_en                                            "},  /* 62 */
	{0x00B0, "pi_edge_i_en,eq_osacal_en,eq_osacal_bg_en,eq_ldo_en"},  /* 63 */
	{0x00B0, "des_en                                             "},  /* 64 */
	{0x00B0, "eq_en,imppl_en,sq_en,term_en                       "},  /* 65 */
	{0x00B0, "pn_swap                                            "},  /* 66 */
	{0x00B0, "sq,imppl_en                                        "},  /* 67 */
	{0x00B0, "n2p_det,term_en                                    "},  /* 68 */
	{0x00B0, "cdr_en                                             "},  /* 69 */
	{0x00B0, "eq_vcm_en                                          "},  /* 70 */
	{0x00B0, "pi_edge_q_en                                       "},  /* 71 */
	{0x00B0, "fedac_en,eq_en,eq_ldo_en,dfe_clk_en                "},  /* 72 */
	{0x00B0, "dfe_clk_edge_sel,dfe_clk,des_en                    "},  /* 73 */
	{0x00B0, "des_en,cdr_ldo_en,comp_difp_en                     "},  /* 74 */
	{0x00B0, "cdr_ldo_en                                         "},  /* 75 */
	{0x00B0, "lck2ref                                            "},  /* 76 */
	{0x00B0, "freacq_en                                          "},  /* 77 */
	{0x00B0, "cdr_dig_en,auto_en                                 "},  /* 78 */
	{0x00B0, "bias_en                                            "},  /* 79 */
	{0x00B0, "pi_edge_i_en,eq_osacal_en,eq_osacal_bg_en,eq_ldo_en"},  /* 80 */
	{0x00B0, "des_en                                             "},  /* 81 */
	{0x00B0, "eq_en,imppl_en,sq_en,term_en                       "},  /* 82 */
	{0x00B0, "pn_swap                                            "},  /* 83 */

	{0x00B0, "IPATH CODE"},  /* 84 */
	{0x00B0, "IPATH CODE"},  /* 85 */
	{0x00B0, "IPATH CODE"},  /* 86 */
	{0x00B0, "IPATH CODE"},  /* 87 */
	{0x00B0, "IPATH CODE"},  /* 88 */
	{0x00B0, "IPATH CODE"},  /* 89 */
	{0x00B0, "IPATH CODE"},  /* 90 */
	{0x00B0, "IPATH CODE"},  /* 91 */
	{0x00B0, "IPATH CODE"},  /* 92 */
	{0x00B0, "IPATH CODE"},  /* 93 */

	{0x00B0, "PI CODE"},  /* 94 */
	{0x00B0, "PI CODE"},  /* 95 */
	{0x00B0, "PI CODE"},  /* 96 */
	{0x00B0, "PI CODE"},  /* 97 */
	{0x00B0, "PI CODE"},  /* 98 */
	{0x00B0, "PI CODE"},  /* 99 */
	{0x00B0, "PI CODE"},  /* 100 */
	{0x00B0, "PI CODE"},  /* 101 */
	{0x00B0, "PI CODE"},  /* 102 */
	{0x00B0, "PI CODE"},  /* 103 */

	{0x00B0, "RXPLL_BAND"},  /* 104 */
	{0x00B0, "RXPLL_BAND"},  /* 105 */
	{0x00B0, "RXPLL_BAND"},  /* 106 */
	{0x00B0, "RXPLL_BAND"},  /* 107 */
	{0x00B0, "RXPLL_BAND"},  /* 108 */
	{0x3080, ""},  /* 109 */

	{0xC210, ""},  /* 110 */
	{0xC280, ""},  /* 111 */
	{0xC268, ""},  /* 112 */
	{0xC228, ""},  /* 113 */
	{0xC22C, ""},  /* 114 */
	{0xC220, ""},  /* 115 */
	{0xC224, ""},  /* 116 */
	{0xC284, ""},  /* 117 */
	{0xC274, ""},  /* 118 */
	{0xC278, ""},  /* 119 */
	{0xC29C, ""},  /* 110 */
	{0xC214, ""},  /* 121 */
	{0xC218, ""},  /* 122 */
	{0xC21C, ""},  /* 123 */
	{0xC234, ""},  /* 124 */
	{0xC230, ""},  /* 125 */
	{0xC244, ""},  /* 126 */
	{0xC250, ""},  /* 127 */
	{0xC270, ""},  /* 128 */
	{0xC26C, ""},  /* 129 */
	{0xC310, ""},  /* 120 */
	{0xC380, ""},  /* 131 */
	{0xC368, ""},  /* 132 */
	{0xC328, ""},  /* 133 */
	{0xC32C, ""},  /* 134 */
	{0xC320, ""},  /* 135 */
	{0xC324, ""},  /* 136 */
	{0xC384, ""},  /* 137 */
	{0xC374, ""},  /* 138 */
	{0xC378, ""},  /* 139 */
	{0xC39C, ""},  /* 140 */
	{0xC314, ""},  /* 141 */
	{0xC318, ""},  /* 142 */
	{0xC31C, ""},  /* 143 */
	{0xC334, ""},  /* 144 */
	{0xC330, ""},  /* 145 */
	{0xC344, ""},  /* 146 */
	{0xC350, ""},  /* 147 */
	{0xC370, ""},  /* 148 */
	{0xC36C, ""},  /* 149 */
};

#define MPHY_DUMP_NUM  (sizeof(mphy_reg_list) / sizeof(struct ufs_mtk_mphy_reg))
#define PHYD_DUMP_NUM  49

struct ufs_mtk_mphy_struct {
	u32 record[MPHY_DUMP_NUM];
	u64 time;
	u64 time_done;
};
static struct ufs_mtk_mphy_struct mphy_record[UFS_MPHY_STAGE_NUM];

void ufs_mtk_dbg_phy_trace(struct ufs_hba *hba, u8 stage)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 i, j;

	if (!host->mphy_base)
		return;

	if (mphy_record[stage].time)
		return;

	mphy_record[stage].time = local_clock();

	writel(0xC1000200, host->mphy_base + 0x20C0);
	for (i = 0; i < 2; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	for (i = 2; i < 20; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}
	writel(0, host->mphy_base + 0x20C0);

	writel(0x0, host->mphy_base + 0x0);
	writel(0x4, host->mphy_base + 0x4);
	for (i = 20; i < 21; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	for (i = 21; i < 49; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	/* DA Probe */
	writel(0x0, host->mphy_base + 0x0);
	writel(0x7, host->mphy_base + 0x4);
	for (i = 49; i < 50; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	/* Lane 0 */
	writel(0xc, host->mphy_base + 0x0);
	writel(0x45, host->mphy_base + 0xA000);
	for (i = 50; i < 51; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x5f, host->mphy_base + 0xA000);
	for (i = 51; i < 52; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x85, host->mphy_base + 0xA000);
	for (i = 52; i < 53; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8a, host->mphy_base + 0xA000);
	for (i = 53; i < 54; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8b, host->mphy_base + 0xA000);
	for (i = 54; i < 55; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8c, host->mphy_base + 0xA000);
	for (i = 55; i < 56; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8d, host->mphy_base + 0xA000);
	for (i = 56; i < 57; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8e, host->mphy_base + 0xA000);
	for (i = 57; i < 58; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x94, host->mphy_base + 0xA000);
	for (i = 58; i < 59; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x95, host->mphy_base + 0xA000);
	for (i = 59; i < 60; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x97, host->mphy_base + 0xA000);
	for (i = 60; i < 61; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x98, host->mphy_base + 0xA000);
	for (i = 61; i < 62; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x99, host->mphy_base + 0xA000);
	for (i = 62; i < 63; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x9c, host->mphy_base + 0xA000);
	for (i = 63; i < 64; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x9d, host->mphy_base + 0xA000);
	for (i = 64; i < 65; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0xbd, host->mphy_base + 0xA000);
	for (i = 65; i < 66; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0xca, host->mphy_base + 0xA000);
	for (i = 66; i < 67; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	/* Lane 1 */
	writel(0xd, host->mphy_base + 0x0);
	writel(0x45, host->mphy_base + 0xA100);
	for (i = 67; i < 68; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x5f, host->mphy_base + 0xA100);
	for (i = 68; i < 69; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x85, host->mphy_base + 0xA100);
	for (i = 69; i < 70; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8a, host->mphy_base + 0xA100);
	for (i = 70; i < 71; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8b, host->mphy_base + 0xA100);
	for (i = 71; i < 72; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8c, host->mphy_base + 0xA100);
	for (i = 72; i < 73; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8d, host->mphy_base + 0xA100);
	for (i = 73; i < 74; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x8e, host->mphy_base + 0xA100);
	for (i = 74; i < 75; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x94, host->mphy_base + 0xA100);
	for (i = 75; i < 76; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x95, host->mphy_base + 0xA100);
	for (i = 76; i < 77; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x97, host->mphy_base + 0xA100);
	for (i = 77; i < 78; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x98, host->mphy_base + 0xA100);
	for (i = 78; i < 79; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x99, host->mphy_base + 0xA100);
	for (i = 79; i < 80; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x9c, host->mphy_base + 0xA100);
	for (i = 80; i < 81; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0x9d, host->mphy_base + 0xA100);
	for (i = 81; i < 82; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0xbd, host->mphy_base + 0xA100);
	for (i = 82; i < 83; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(0xca, host->mphy_base + 0xA100);
	for (i = 83; i < 84; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	/* IPATH CODE */
	for (j = 0; j < 10; j++) {
		writel(0x00000000, host->mphy_base + 0x0000);
		writel(0x2F2E2D2C, host->mphy_base + 0x0004);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x00061003, host->mphy_base + 0xB000);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x00061003, host->mphy_base + 0xB100);
		writel(0x00000101, host->mphy_base + 0xB024);
		writel(0x00000101, host->mphy_base + 0xB124);
		writel(0x00000141, host->mphy_base + 0xB024);
		writel(0x400E1003, host->mphy_base + 0xB000);
		writel(0x00000141, host->mphy_base + 0xB124);
		writel(0x400E1003, host->mphy_base + 0xB100);
		writel(0x00000101, host->mphy_base + 0xB024);
		writel(0x000E1003, host->mphy_base + 0xB000);
		writel(0x00000101, host->mphy_base + 0xB124);
		writel(0x000E1003, host->mphy_base + 0xB100);
		for (i = (84 + j); i < (85 + j); i++) {
			mphy_record[stage].record[i] =
				readl(host->mphy_base + mphy_reg_list[i].reg);
		}
	}

	for (j = 0; j < 10; j++) {
		writel(0x00000000, host->mphy_base + 0x0000);
		writel(0x2F2E2D2C, host->mphy_base + 0x0004);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x00061003, host->mphy_base + 0xB000);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x00061003, host->mphy_base + 0xB100);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x00000041, host->mphy_base + 0xB024);
		writel(0x400E1003, host->mphy_base + 0xB000);
		writel(0x00000041, host->mphy_base + 0xB124);
		writel(0x400E1003, host->mphy_base + 0xB100);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x000E1003, host->mphy_base + 0xB000);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x000E1003, host->mphy_base + 0xB100);
		for (i = (94 + j); i < (95 + j); i++) {
			mphy_record[stage].record[i] =
				readl(host->mphy_base + mphy_reg_list[i].reg);
		}
	}

	writel(0x00000000, host->mphy_base + 0x0000);
	writel(0x2A << 8 | 0x28, host->mphy_base + 0x4);
	for (i = 104; i < 109; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(readl(host->mphy_base + 0x1044) | 0x20,
		host->mphy_base + 0x1044);
	for (i = 109; i < 110; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}


	/* Enable CK */
	writel(readl(host->mphy_base + 0xA02C) | (0x1 << 11),
		host->mphy_base + 0xA02C);
	writel(readl(host->mphy_base + 0xA12C) | (0x1 << 11),
		host->mphy_base + 0xA12C);
	writel(readl(host->mphy_base + 0xA6C8) | (0x3 << 13),
		host->mphy_base + 0xA6C8);
	writel(readl(host->mphy_base + 0xA638) | (0x1 << 10),
		host->mphy_base + 0xA638);
	writel(readl(host->mphy_base + 0xA7C8) | (0x3 << 13),
		host->mphy_base + 0xA7C8);
	writel(readl(host->mphy_base + 0xA738) | (0x1 << 10),
		host->mphy_base + 0xA738);

	/* Dump [Lane0] RX RG */
	for (i = 110; i < 112; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(readl(host->mphy_base + 0xC0DC) & ~(0x1 << 25),
		host->mphy_base + 0xC0DC);
	writel(readl(host->mphy_base + 0xC0DC) | (0x1 << 25),
		host->mphy_base + 0xC0DC);
	writel(readl(host->mphy_base + 0xC0DC) & ~(0x1 << 25),
		host->mphy_base + 0xC0DC);

	for (i = 112; i < 120; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(readl(host->mphy_base + 0xC0C0) & ~(0x1 << 27),
		host->mphy_base + 0xC0C0);
	writel(readl(host->mphy_base + 0xC0C0) | (0x1 << 27),
		host->mphy_base + 0xC0C0);
	writel(readl(host->mphy_base + 0xC0C0) & ~(0x1 << 27),
		host->mphy_base + 0xC0C0);

	for (i = 120; i < 130; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	/* Dump [Lane1] RX RG */
	for (i = 130; i < 132; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(readl(host->mphy_base + 0xC1DC) & ~(0x1 << 25),
		host->mphy_base + 0xC1DC);
	writel(readl(host->mphy_base + 0xC1DC) | (0x1 << 25),
		host->mphy_base + 0xC1DC);
	writel(readl(host->mphy_base + 0xC1DC) & ~(0x1 << 25),
		host->mphy_base + 0xC1DC);

	for (i = 132; i < 140; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	writel(readl(host->mphy_base + 0xC1C0) & ~(0x1 << 27),
		host->mphy_base + 0xC1C0);
	writel(readl(host->mphy_base + 0xC1C0) | (0x1 << 27),
		host->mphy_base + 0xC1C0);
	writel(readl(host->mphy_base + 0xC1C0) & ~(0x1 << 27),
		host->mphy_base + 0xC1C0);


	for (i = 140; i < 150; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_list[i].reg);
	}

	/* Disable CK */
	writel(readl(host->mphy_base + 0xA02C) & ~(0x1 << 11),
		host->mphy_base + 0xA02C);
	writel(readl(host->mphy_base + 0xA12C) & ~(0x1 << 11),
		host->mphy_base + 0xA12C);
	writel(readl(host->mphy_base + 0xA6C8) & ~(0x3 << 13),
		host->mphy_base + 0xA6C8);
	writel(readl(host->mphy_base + 0xA638) & ~(0x1 << 10),
		host->mphy_base + 0xA638);
	writel(readl(host->mphy_base + 0xA7C8) & ~(0x3 << 13),
		host->mphy_base + 0xA7C8);
	writel(readl(host->mphy_base + 0xA738) & ~(0x1 << 10),
		host->mphy_base + 0xA738);

	mphy_record[stage].time_done = local_clock();
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_trace);

void ufs_mtk_dbg_phy_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	/* record burst mode mphy status after resume exit hibern8 complete */
	if (status == POST_CHANGE && cmd == UIC_CMD_DME_HIBER_EXIT &&
		(hba->pm_op_in_progress)) {

		ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_INIT);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_hibern8_notify);

void ufs_mtk_dbg_phy_dump(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct timespec64 dur;
	u32 i, j;

	if (!host->mphy_base)
		return;

	for (i = 0; i < UFS_MPHY_STAGE_NUM; i++) {
		if (mphy_record[i].time == 0)
			continue;

		pr_info("%s: MPHY stage = %d\n", __func__, i);

		dur = ns_to_timespec64(mphy_record[i].time);
		pr_info("%s: MPHY record start at %6llu.%lu\n", __func__,
			dur.tv_sec, dur.tv_nsec);

		dur = ns_to_timespec64(mphy_record[i].time_done);
		pr_info("%s: MPHY record end at %6llu.%lu\n", __func__,
			dur.tv_sec, dur.tv_nsec);

		pr_info("%s: ---PHYD dump---\n", __func__);

		for (j = 0; j < MPHY_DUMP_NUM; j++) {
			if (j == PHYD_DUMP_NUM)
				pr_info("%s: ---PHYA dump---\n", __func__);

			pr_info("%s: 0x%X%04X=0x%x, %s\n", __func__, mphy_phys_base >> 16,
				mphy_reg_list[j].reg, mphy_record[i].record[j], mphy_reg_list[j].str);
		}
		/* clear mphy record time to avoid to print remaining log */
		mphy_record[i].time = 0;
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_dump);

void ufs_mtk_dbg_phy_dump_work(struct work_struct *work)
{
	struct ufs_mtk_host *host;
	struct ufs_hba *hba;

	host = container_of(work, struct ufs_mtk_host, phy_dmp_work.work);
	hba = host->hba;

	ufs_mtk_dbg_phy_dump(hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_dump_work);

void ufs_mtk_dbg_phy_enable(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device_node *np;
	struct resource res;

	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8183-ufsphy");
	if (!np)
		goto out;

	if (of_address_to_resource(np, 0, &res))
		goto out;

	mphy_phys_base = res.start;
	host->mphy_base = ioremap(res.start, 0x10000);
	INIT_DELAYED_WORK(&host->phy_dmp_work, ufs_mtk_dbg_phy_dump_work);
	host->phy_dmp_workq = create_singlethread_workqueue("ufs_mtk_phy_dmp_wq");

	return;
out:
	pr_err("%s: Unable to parse mphy base, disable mphy dump.\n", __func__);
	host->caps &= ~UFS_MTK_CAP_MPHY_DUMP;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_enable);
#endif

static void probe_ufshcd_clk_gating(void *data, const char *dev_name,
				    int state)
{
	int ptr;
#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	struct ufs_mtk_host *host = ufshcd_get_variant(ufshba);
	u32 val;
#endif

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = CMD_CLK_GATING;
	cmd_hist[ptr].cmd.clk_gating.state = state;

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	if (state == CLKS_ON && host->mphy_base) {
		writel(0xC1000200, host->mphy_base + 0x20C0);
		cmd_hist[ptr].cmd.clk_gating.arg1 =
			readl(host->mphy_base + 0xA09C);
		cmd_hist[ptr].cmd.clk_gating.arg2 =
			readl(host->mphy_base + 0xA19C);
		writel(0, host->mphy_base + 0x20C0);
	} else if (state == REQ_CLKS_OFF && host->mphy_base) {
		writel(0xC1000200, host->mphy_base + 0x20C0);
		cmd_hist[ptr].cmd.clk_gating.arg1 =
			readl(host->mphy_base + 0xA09C);
		cmd_hist[ptr].cmd.clk_gating.arg2 =
			readl(host->mphy_base + 0xA19C);
		writel(0, host->mphy_base + 0x20C0);

		/* when req clk off, clear 2 line hw status */
		val = readl(host->mphy_base + 0xA800) | 0x02;
		writel(val, host->mphy_base + 0xA800);
		writel(val, host->mphy_base + 0xA800);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA800);

		val = readl(host->mphy_base + 0xA900) | 0x02;
		writel(val, host->mphy_base + 0xA900);
		writel(val, host->mphy_base + 0xA900);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA900);

		val = readl(host->mphy_base + 0xA804) | 0x02;
		writel(val, host->mphy_base + 0xA804);
		writel(val, host->mphy_base + 0xA804);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA804);

		val = readl(host->mphy_base + 0xA904) | 0x02;
		writel(val, host->mphy_base + 0xA904);
		writel(val, host->mphy_base + 0xA904);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA904);
	} else {
		cmd_hist[ptr].cmd.clk_gating.arg1 = 0;
		cmd_hist[ptr].cmd.clk_gating.arg2 = 0;
	}
	cmd_hist[ptr].cmd.clk_gating.arg3 = 0;
#endif
}

static void probe_ufshcd_profile_clk_scaling(void *data, const char *dev_name,
	const char *profile_info, s64 time_us, int err)
{
	int ptr;

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = CMD_CLK_SCALING;
	if (!strcmp(profile_info, "up"))
		cmd_hist[ptr].cmd.clk_scaling.state = CLKS_SCALE_UP;
	else
		cmd_hist[ptr].cmd.clk_scaling.state = CLKS_SCALE_DOWN;
	cmd_hist[ptr].cmd.clk_scaling.err = err;
}

static void probe_ufshcd_pm(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state,
			    enum ufsdbg_pm_state state)
{
	int ptr;

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = CMD_PM;
	cmd_hist[ptr].cmd.pm.state = state;
	cmd_hist[ptr].cmd.pm.err = err;
	cmd_hist[ptr].cmd.pm.time_us = time_us;
	cmd_hist[ptr].cmd.pm.pwr_mode = pwr_mode;
	cmd_hist[ptr].cmd.pm.link_state = link_state;
}

static void probe_ufshcd_runtime_suspend(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_RUNTIME_SUSPEND);
}

static void probe_ufshcd_runtime_resume(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_RUNTIME_RESUME);
}

static void probe_ufshcd_system_suspend(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_SYSTEM_SUSPEND);
}

static void probe_ufshcd_system_resume(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_SYSTEM_RESUME);
}

/*
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	{.name = "ufshcd_command", .func = probe_ufshcd_command},
	{.name = "ufshcd_uic_command", .func = probe_ufshcd_uic_command},
	{.name = "ufshcd_clk_gating", .func = probe_ufshcd_clk_gating},
	{
		.name = "ufshcd_profile_clk_scaling",
		.func = probe_ufshcd_profile_clk_scaling
	},
	{
		.name = "android_vh_ufs_send_command",
		.func = probe_android_vh_ufs_send_command
	},
	{
		.name = "android_vh_ufs_compl_command",
		.func = probe_android_vh_ufs_compl_command
	},
	{.name = "android_vh_ufs_send_tm_command", .func = probe_android_vh_ufs_send_tm_command},
	{.name = "ufshcd_wl_runtime_suspend", .func = probe_ufshcd_runtime_suspend},
	{.name = "ufshcd_wl_runtime_resume", .func = probe_ufshcd_runtime_resume},
	{.name = "ufshcd_wl_suspend", .func = probe_ufshcd_system_suspend},
	{.name = "ufshcd_wl_resume", .func = probe_ufshcd_system_resume},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

/*
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void ufs_mtk_scsi_unblock_requests(struct ufs_hba *hba)
{
	if (atomic_dec_and_test(&hba->scsi_block_reqs_cnt))
		scsi_unblock_requests(hba->host);
}

static void ufs_mtk_scsi_block_requests(struct ufs_hba *hba)
{
	if (atomic_inc_return(&hba->scsi_block_reqs_cnt) == 1)
		scsi_block_requests(hba->host);
}

static u32 ufs_mtk_pending_cmds(struct ufs_hba *hba)
{
	const struct scsi_device *sdev;
	u32 pending = 0;

	lockdep_assert_held(hba->host->host_lock);
	__shost_for_each_device(sdev, hba->host)
		pending += sbitmap_weight(&sdev->budget_map);

	return pending;
}

static int ufs_mtk_wait_for_doorbell_clr(struct ufs_hba *hba,
					u64 wait_timeout_us)
{
	unsigned long flags;
	int ret = 0;
	u32 tm_doorbell;
	u32 tr_pending;
	bool timeout = false, do_last_check = false;
	ktime_t start;

	ufshcd_hold(hba);
	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * Wait for all the outstanding tasks/transfer requests.
	 * Verify by checking the doorbell registers are clear.
	 */
	start = ktime_get();
	do {
		if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
			ret = -EBUSY;
			goto out;
		}

		tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
		tr_pending = ufs_mtk_pending_cmds(hba);
		if (!tm_doorbell && !tr_pending) {
			timeout = false;
			break;
		} else if (do_last_check) {
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		schedule();
		if (ktime_to_us(ktime_sub(ktime_get(), start)) >
		    wait_timeout_us) {
			timeout = true;
			/*
			 * We might have scheduled out for long time so make
			 * sure to check if doorbells are cleared by this time
			 * or not.
			 */
			do_last_check = true;
		}
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (tm_doorbell || tr_pending);

	if (timeout) {
		dev_err(hba->dev,
			"%s: timedout waiting dbr to clr (tm=0x%x, tr=0x%x)\n",
			__func__, tm_doorbell, tr_pending);
		ret = -EBUSY;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_release(hba);
	return ret;
}

/*
 * Ref-clk start calibration may have jiter, block requests and enter ah8
 */
int ufs_mtk_cali_hold(void)
{
	struct ufs_hba *hba = ufshba;
	u64 timeout = 1000 * 1000; /* 1 sec */;
	int ret = 0;

	if (!hba)
		return -EINVAL;

	pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
	ufs_mtk_scsi_block_requests(hba);

	if (ufs_mtk_wait_for_doorbell_clr(hba, timeout)) {
		dev_err(hba->dev, "%s: wait doorbell clr timeout!\n",
				__func__);
		ret = -EBUSY;
	}

	/* To make sure clock scaling isn't work when ref-clk calibration ongoing */
	if (hba->clk_scaling.workq) {
		queue_work(hba->clk_scaling.workq, &hba->clk_scaling.suspend_work);
		flush_work(&hba->clk_scaling.suspend_work);
	}

	/* Make sure host enter AH8 and clock off */
	mdelay(15);

	dev_info(hba->dev, "%s: UFS Block Request ret = %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(ufs_mtk_cali_hold);

/*
 * Ref-clk calibration end, unblock requests
 */
int ufs_mtk_cali_release(void)
{
	struct ufs_hba *hba = ufshba;

	if (!hba)
		return -EINVAL;

	ufs_mtk_scsi_unblock_requests(hba);
	pm_runtime_put(&hba->ufs_device_wlun->sdev_gendev);

	dev_info(hba->dev, "%s: UFS Unblock Request Success!\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_cali_release);

int ufs_mtk_dbg_cmd_hist_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	if (!cmd_hist) {
		cmd_hist_enabled = false;
		spin_unlock_irqrestore(&cmd_hist_lock, flags);
		return -ENOMEM;
	}

	cmd_hist_enabled = true;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_cmd_hist_enable);

int ufs_mtk_dbg_cmd_hist_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_enabled = false;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_cmd_hist_disable);

static void _cmd_hist_cleanup(void)
{
	ufs_mtk_dbg_cmd_hist_disable();
	vfree(cmd_hist);
}

#define CLK_GATING_STATE_MAX (4)

static char *clk_gating_state_str[CLK_GATING_STATE_MAX + 1] = {
	"clks_off",
	"clks_on",
	"req_clks_off",
	"req_clks_on",
	"unknown"
};

static void ufs_mtk_dbg_print_clk_gating_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_gating.state;

	if (idx < 0 || idx >= CLK_GATING_STATE_MAX)
		idx = CLK_GATING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-CLK_GAT(%d),%6llu.%09lu,%5d,%2d,%13s,arg1=0x%X,arg2=0x%X,arg3=0x%X\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		clk_gating_state_str[idx],
		cmd_hist[ptr].cmd.clk_gating.arg1,
		cmd_hist[ptr].cmd.clk_gating.arg2,
		cmd_hist[ptr].cmd.clk_gating.arg3
		);
}

#define CLK_SCALING_STATE_MAX (2)

static char *clk_scaling_state_str[CLK_SCALING_STATE_MAX + 1] = {
	"clk scale down",
	"clk scale up",
	"unknown"
};

static void ufs_mtk_dbg_print_clk_scaling_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_scaling.state;

	if (idx < 0 || idx >= CLK_SCALING_STATE_MAX)
		idx = CLK_SCALING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-CLKSCAL(%d),%6llu.%09lu,%5d,%2d,%15s, err:%d\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		clk_scaling_state_str[idx],
		cmd_hist[ptr].cmd.clk_scaling.err
		);
}

#define UFSDBG_PM_STATE_MAX (4)
static char *ufsdbg_pm_state_str[UFSDBG_PM_STATE_MAX + 1] = {
	"rs",
	"rr",
	"ss",
	"sr",
	"unknown"
};

static void ufs_mtk_dbg_print_pm_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.pm.state;
	int err = cmd_hist[ptr].cmd.pm.err;
	unsigned long time_us = cmd_hist[ptr].cmd.pm.time_us;
	int pwr_mode = cmd_hist[ptr].cmd.pm.pwr_mode;
	int link_state = cmd_hist[ptr].cmd.pm.link_state;

	if (idx < 0 || idx >= UFSDBG_PM_STATE_MAX)
		idx = UFSDBG_PM_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-PWR_MOD(%d),%6llu.%09lu,%5d,%2d,%3s, ret=%d, time_us=%8lu, pwr_mode=%d, link_status=%d\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		ufsdbg_pm_state_str[idx],
		err,
		time_us,
		pwr_mode,
		link_state
		);
}

static void ufs_mtk_dbg_print_device_reset_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_gating.state;

	if (idx < 0 || idx >= CLK_GATING_STATE_MAX)
		idx = CLK_GATING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-DEV_RST(%d),%6llu.%09lu,%5d,%2d,%13s\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		"device reset"
		);
}

static void ufs_mtk_dbg_print_uic_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-UIC_CMD(%d),%6llu.%09lu,%5d,%2d,0x%2x,arg1=0x%X,arg2=0x%X,arg3=0x%X,\t%llu\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.uic.cmd,
		cmd_hist[ptr].cmd.uic.arg1,
		cmd_hist[ptr].cmd.uic.arg2,
		cmd_hist[ptr].cmd.uic.arg3,
		cmd_hist[ptr].duration
		);
}

static void ufs_mtk_dbg_print_utp_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	if (cmd_hist[ptr].cmd.utp.lba == 0xFFFFFFFFFFFFFFFF)
		cmd_hist[ptr].cmd.utp.lba = 0;
	SPREAD_PRINTF(buff, size, m,
		"%3d-UTP_CMD(%d),%6llu.%09lu,%5d,%2d,0x%2x,t=%2d,db:0x%8x,is:0x%8x,crypt:%d,%d,lba=%10llu,len=%6d,\t%llu\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.utp.opcode,
		cmd_hist[ptr].cmd.utp.tag,
		cmd_hist[ptr].cmd.utp.doorbell,
		cmd_hist[ptr].cmd.utp.intr,
		cmd_hist[ptr].cmd.utp.crypt_en,
		cmd_hist[ptr].cmd.utp.crypt_keyslot,
		cmd_hist[ptr].cmd.utp.lba,
		cmd_hist[ptr].cmd.utp.transfer_len,
		cmd_hist[ptr].duration
		);
}

static void ufs_mtk_dbg_print_dev_event(char **buff, unsigned long *size,
				      struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);

	SPREAD_PRINTF(buff, size, m,
		"%3d-DEV_CMD(%d),%6llu.%09lu,%5d,%2d,%4u,t=%2d,op:%u,idn:%u,idx:%u,sel:%u\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.dev.type,
		cmd_hist[ptr].cmd.dev.tag,
		cmd_hist[ptr].cmd.dev.opcode,
		cmd_hist[ptr].cmd.dev.idn,
		cmd_hist[ptr].cmd.dev.index,
		cmd_hist[ptr].cmd.dev.selector
		);
}

static void ufs_mtk_dbg_print_tm_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	if (cmd_hist[ptr].cmd.utp.lba == 0xFFFFFFFFFFFFFFFF)
		cmd_hist[ptr].cmd.utp.lba = 0;
	SPREAD_PRINTF(buff, size, m,
		"%3d-TAS_MAN(%d),%6llu.%09lu,%5d,%2d,0x%2x,lun=%d,tag=%d,task_tag=%d\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.tm.tm_func,
		cmd_hist[ptr].cmd.tm.lun,
		cmd_hist[ptr].cmd.tm.tag,
		cmd_hist[ptr].cmd.tm.task_tag
		);
}

static void ufs_mtk_dbg_print_cmd_hist(char **buff, unsigned long *size,
				  u32 latest_cnt, struct seq_file *m, bool omit)
{
	int ptr;
	int cnt;
	unsigned long flags;

	if (!cmd_hist_initialized)
		return;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist) {
		spin_unlock_irqrestore(&cmd_hist_lock, flags);
		return;
	}

	if (omit)
		cnt = min_t(u32, cmd_hist_cnt, MAX_CMD_HIST_ENTRY_CNT);
	else
		cnt = MAX_CMD_HIST_ENTRY_CNT;

	if (latest_cnt)
		cnt = min_t(u32, latest_cnt, cnt);

	ptr = cmd_hist_ptr;

	SPREAD_PRINTF(buff, size, m,
		      "UFS CMD History: Latest %d of total %d entries, ptr=%d\n",
		      latest_cnt, cnt, ptr);

	while (cnt) {
		if (cmd_hist[ptr].event < CMD_DEV_SEND)
			ufs_mtk_dbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_TM_SEND)
			ufs_mtk_dbg_print_dev_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_UIC_SEND)
			ufs_mtk_dbg_print_tm_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_REG_TOGGLE)
			ufs_mtk_dbg_print_uic_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_CLK_GATING)
			ufs_mtk_dbg_print_clk_gating_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_CLK_SCALING)
			ufs_mtk_dbg_print_clk_scaling_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_PM)
			ufs_mtk_dbg_print_pm_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_ABORTING)
			ufs_mtk_dbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_DEVICE_RESET)
			ufs_mtk_dbg_print_device_reset_event(buff, size,
							     m, ptr);
		cnt--;
		ptr--;
		if (ptr < 0)
			ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
	}
	if (omit)
		cmd_hist_cnt = 1;

	spin_unlock_irqrestore(&cmd_hist_lock, flags);

}

void ufs_mtk_dbg_dump(u32 latest_cnt)
{
	ufs_mtk_dbg_print_info(NULL, NULL, NULL);

	ufs_mtk_dbg_print_cmd_hist(NULL, NULL, latest_cnt, NULL, true);
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_dump);

void ufs_mtk_dbg_get_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = UFS_AEE_BUFFER_SIZE;
	char *buff;

	if (!cmd_hist) {
		pr_info("failed to dump UFS: null cmd history buffer");
		return;
	}

	if (!ufs_aee_buffer) {
		pr_info("failed to dump UFS: null AEE buffer");
		return;
	}

	buff = ufs_aee_buffer;
	ufs_mtk_dbg_print_info(&buff, &free_size, NULL);
	ufs_mtk_dbg_print_cmd_hist(&buff, &free_size,
				   MAX_CMD_HIST_ENTRY_CNT, NULL, false);

	/* retrun start location */
	*vaddr = (unsigned long)ufs_aee_buffer;
	*size = UFS_AEE_BUFFER_SIZE - free_size;

	ufs_mtk_dbg_cmd_hist_enable();
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_get_aee_buffer);

#ifndef USER_BUILD_KERNEL
#define PROC_PERM		0660
#else
#define PROC_PERM		0440
#endif

static ssize_t ufs_debug_proc_write(struct file *file, const char *buf,
				 size_t count, loff_t *data)
{
	unsigned long op = UFSDBG_UNKNOWN;
	struct ufs_hba *hba = ufshba;
	char cmd_buf[16];
	u16 rnd;

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 16, &op))
		return -EINVAL;

	if (op == UFSDBG_CMD_LIST_DUMP) {
		dev_info(hba->dev, "debug info and cmd history dump\n");
		ufs_mtk_dbg_dump(MAX_CMD_HIST_ENTRY_CNT);
	} else if (op == UFSDBG_CMD_LIST_ENABLE) {
		ufs_mtk_dbg_cmd_hist_enable();
		dev_info(hba->dev, "cmd history on\n");
	} else if (op == UFSDBG_CMD_LIST_DISABLE) {
		ufs_mtk_dbg_cmd_hist_disable();
		dev_info(hba->dev, "cmd history off\n");
	} else if (op == UFSDBG_MPHY_DUMP) {
		dev_info(hba->dev, "ufs mphy reg debug dump\n");
		ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_DUMP);
		ufs_mtk_dbg_phy_dump(hba);
	} else if (op == UFSDBG_UIC_ERR_INJECT) {
		ufshcd_rpm_get_sync(hba);
		ufshcd_hold(hba);
		get_random_bytes(&rnd, sizeof(rnd));
		dev_info(hba->dev, "Inject UIC error %d, val=%d\n", rnd % (UFS_EVT_FATAL_ERR + 1), rnd);
		ufshcd_update_evt_hist(hba, rnd %(UFS_EVT_FATAL_ERR + 1) , rnd);
		ufshcd_release(hba);
		ufshcd_rpm_put(hba);
	}

	return count;
}

static int ufs_debug_proc_show(struct seq_file *m, void *v)
{
	ufs_mtk_dbg_print_info(NULL, NULL, m);
	ufs_mtk_dbg_print_cmd_hist(NULL, NULL, MAX_CMD_HIST_ENTRY_CNT,
				   m, false);
	return 0;
}

static int ufs_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_debug_proc_show, inode->i_private);
}

static const struct proc_ops ufs_debug_proc_fops = {
	.proc_open = ufs_debug_proc_open,
	.proc_write = ufs_debug_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_mtk_dbg_init_procfs(void)
{
	struct proc_dir_entry *prEntry;
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	/* Create "ufs_debug" node */
	prEntry = proc_create("ufs_debug", PROC_PERM, NULL,
			      &ufs_debug_proc_fops);

	if (prEntry)
		proc_set_user(prEntry, uid, gid);
	else
		pr_info("%s: failed to create ufs_debugn", __func__);

	return 0;
}

int ufs_mtk_dbg_tp_register(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n",
				interests[i].name);
			return -EINVAL;
		}

		if (interests[i].init)
			continue;

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_tp_register);

void ufs_mtk_dbg_tp_unregister(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
			interests[i].init = false;
		}
	}

	tracepoint_synchronize_unregister();
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_tp_unregister);

static void ufs_mtk_dbg_cleanup(void)
{
	ufs_mtk_dbg_tp_unregister();

	_cmd_hist_cleanup();
}

int ufs_mtk_dbg_register(struct ufs_hba *hba)
{
	int ret;

	/*
	 * Ignore any failure of AEE buffer allocation to still allow
	 * command history dump in procfs.
	 */
	ufs_aee_buffer = kzalloc(UFS_AEE_BUFFER_SIZE, GFP_NOFS);

	spin_lock_init(&cmd_hist_lock);
	ufshba = hba;
	cmd_hist_initialized = true;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	ret = ufs_mtk_dbg_tp_register();
	if (ret)
		goto out;

	/* Create control nodes in procfs */
	ret = ufs_mtk_dbg_init_procfs();

out:
	/* Enable command history feature by default */
	if (!ret)
		ufs_mtk_dbg_cmd_hist_enable();
	else
		ufs_mtk_dbg_cleanup();

	return ret;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_register);

static void __exit ufs_mtk_dbg_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	mrdump_set_extra_dump(AEE_EXTRA_FILE_UFS, NULL);
#endif
	kfree(cmd_hist);
}

static int __init ufs_mtk_dbg_init(void)
{
	cmd_hist = kcalloc(MAX_CMD_HIST_ENTRY_CNT,
			   sizeof(struct cmd_hist_struct),
			   GFP_KERNEL);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	mrdump_mini_add_extra_file((unsigned long)cmd_hist,
						__pa_nodebug(cmd_hist),
						UFS_AEE_BUFFER_SIZE,
						"UFS_CMD_HIST");
	mrdump_set_extra_dump(AEE_EXTRA_FILE_UFS, ufs_mtk_dbg_get_aee_buffer);
#endif
	return 0;
}

module_init(ufs_mtk_dbg_init)
module_exit(ufs_mtk_dbg_exit)

MODULE_DESCRIPTION("MediaTek UFS Debugging Facility");
MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL v2");
