// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <dt-bindings/clock/mmdvfs-clk.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk_mmdvfs.h>
#include <linux/workqueue.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <mt-plat/mrdump.h>

#include <clk-fmeter.h>
#include <mtk-smi-dbg.h>

#include "mtk-mmdvfs-v3-memory.h"
#include "mtk-mmdvfs-ftrace.h"
#include "mtk-mmdvfs-debug.h"
#include "mtk-mmdebug-vcp.h"
#include "clk-mtk.h"

#ifndef MAX_OPP
#define MAX_OPP		(8)
#endif

#define MMDVFS_DBG_VER1	BIT(0)
#define MMDVFS_DBG_VER3	BIT(1)

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs_dbg][dbg]%s: "fmt"\n", __func__, ##args)

#define mmdvfs_debug_dump_line(file, fmt, args...)	\
({							\
	if (file)					\
		seq_printf(file, fmt"\n", ##args);		\
	else						\
		MMDVFS_DBG(fmt, ##args);		\
})

struct mmdvfs_record {
	u64 sec;
	u64 nsec;
	u8 opp;
};

struct mmdvfs_debug {
	struct device *dev;
	struct proc_dir_entry *proc;
	struct proc_dir_entry *proc_mbrain;
	u32 debug_version;

	/* MMDVFS_DBG_VER1 */
	struct regulator *reg;
	u32 reg_cnt_vol;
	u32 force_step0;
	u32 release_step0;
	u32 vote_step;
	u32 force_step;

	spinlock_t lock;
	u8 rec_cnt;
	struct mmdvfs_record rec[MEM_REC_CNT_MAX];

	/* MMDVFS_DBG_VER3 */
	u32 use_v3_pwr;

	/* regulator and fmeter */
	struct regulator *reg_vcore;
	struct regulator *reg_vmm;
	struct regulator *reg_vdisp;
	u8 fmeter_count;
	u8 *fmeter_id;
	u8 *fmeter_type;
	u32 clk_count;
	u32 *clk_ofs;
	u32 clk_base_pa;
	void __iomem *clk_base;
	struct notifier_block smi_dbg_nb;
	struct notifier_block fmeter_nb;
	struct notifier_block mmdebug_nb;
	struct work_struct	work;
	struct workqueue_struct *workq;

	/* user & power id mapping*/
	u8 user_pwr[USER_NUM];
};

static struct mmdvfs_debug *g_mmdvfs;
static bool ftrace_v1_ena, ftrace_v3_ena;
static bool mmdvfs_met_freerun;
static bool mmdvfs_v3_debug_init_done;

void mtk_mmdvfs_debug_release_step0(void)
{
	if (!g_mmdvfs || !g_mmdvfs->release_step0)
		return;

	if (!IS_ERR_OR_NULL(g_mmdvfs->reg))
		regulator_set_voltage(g_mmdvfs->reg, 0, INT_MAX);
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_release_step0);

static int mmdvfs_debug_set_force_step(const char *val,
	const struct kernel_param *kp)
{
	u8 idx = 0;
	s8 opp = 0;
	int ret;

	ret = sscanf(val, "%hhu %hhd", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM) {
		MMDVFS_DBG("failed:%d idx:%hhu opp:%hhd", ret, idx, opp);
		return -EINVAL;
	}

	if (!mmdvfs_is_init_done())
		return -ENODEV;

	if (idx == PWR_MMDVFS_VCORE && (!g_mmdvfs->debug_version ||
		g_mmdvfs->debug_version & MMDVFS_DBG_VER1)) {
		mmdvfs_set_force_step(opp);
		return 0;
	}

	if (g_mmdvfs->debug_version & MMDVFS_DBG_VER3)
		mtk_mmdvfs_v3_set_force_step(idx, opp, true);

	return 0;
}

static struct kernel_param_ops mmdvfs_debug_set_force_step_ops = {
	.set = mmdvfs_debug_set_force_step,
};
module_param_cb(force_step, &mmdvfs_debug_set_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

static int mmdvfs_debug_set_vote_step(const char *val,
	const struct kernel_param *kp)
{
	u8 idx = 0;
	s8 opp = 0;
	int ret;

	ret = sscanf(val, "%hhu %hhd", &idx, &opp);
	if (ret != 2 || idx > PWR_MMDVFS_NUM) {
		MMDVFS_DBG("failed:%d idx:%hhu opp:%hhd", ret, idx, opp);
		return -EINVAL;
	}

	if (!mmdvfs_is_init_done())
		return -ENODEV;

	if (idx == PWR_MMDVFS_VCORE && (!g_mmdvfs->debug_version ||
		g_mmdvfs->debug_version & MMDVFS_DBG_VER1)) {
		mmdvfs_set_vote_step(opp);
		return 0;
	}

	if (g_mmdvfs->debug_version & MMDVFS_DBG_VER3)
		mtk_mmdvfs_v3_set_vote_step(idx, opp, true);

	return 0;
}

static struct kernel_param_ops mmdvfs_debug_set_vote_step_ops = {
	.set = mmdvfs_debug_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_debug_set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

#ifndef CONFIG_64BIT
static inline u64 readq(const void __iomem *addr)
{
	u32 low, high;

	low = readl(addr);
	high = readl(addr + 4);

	return ((u64)high << 32) | low;
}
#endif

static void mmdvfs_debug_record_opp(const u8 opp)
{
	struct mmdvfs_record *rec;
	unsigned long flags;

	if (!g_mmdvfs)
		return;

	spin_lock_irqsave(&g_mmdvfs->lock, flags);

	rec = &g_mmdvfs->rec[g_mmdvfs->rec_cnt];
	rec->sec = sched_clock();
	rec->nsec = do_div(rec->sec, 1000000000);
	rec->opp = opp;

	g_mmdvfs->rec_cnt =
		(g_mmdvfs->rec_cnt + 1) % ARRAY_SIZE(g_mmdvfs->rec);

	spin_unlock_irqrestore(&g_mmdvfs->lock, flags);
}

void mmdvfs_debug_status_dump(struct seq_file *file)
{
	unsigned long flags;
	u32 i, j, k, val;

	if (!g_mmdvfs)
		return;

	spin_lock_irqsave(&g_mmdvfs->lock, flags);
	for (i = 0; i < g_mmdvfs->clk_count; i++)
		mmdvfs_debug_dump_line(file, "[%#010x] = %#010x", g_mmdvfs->clk_base_pa + g_mmdvfs->clk_ofs[i],
			readl(g_mmdvfs->clk_base + g_mmdvfs->clk_ofs[i]));

	/* MMDVFS_DBG_VER1 */
	mmdvfs_debug_dump_line(file, "VER1: mux controlled by vcore regulator:");

	if (g_mmdvfs->rec[g_mmdvfs->rec_cnt].sec)
		for (i = g_mmdvfs->rec_cnt; i < ARRAY_SIZE(g_mmdvfs->rec); i++)
			mmdvfs_debug_dump_line(file, "[%5llu.%06llu] opp:%u",
				g_mmdvfs->rec[i].sec, g_mmdvfs->rec[i].nsec,
				g_mmdvfs->rec[i].opp);

	for (i = 0; i < g_mmdvfs->rec_cnt; i++)
		mmdvfs_debug_dump_line(file, "[%5llu.%06llu] opp:%u",
			g_mmdvfs->rec[i].sec, g_mmdvfs->rec[i].nsec,
			g_mmdvfs->rec[i].opp);

	spin_unlock_irqrestore(&g_mmdvfs->lock, flags);

	/* MMDVFS_DBG_VER3 */
	if (!MEM_BASE)
		return;

	mmdvfs_debug_dump_line(file, "VER3: mux controlled by vcp:");

	// power opp
	i = readl(MEM_REC_PWR_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_PWR_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] pwr:%u opp:%u",
				readl(MEM_REC_PWR_SEC(j)), readl(MEM_REC_PWR_NSEC(j)),
				readl(MEM_REC_PWR_ID(j)), readl(MEM_REC_PWR_OPP(j)));

	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] pwr:%u opp:%u",
			readl(MEM_REC_PWR_SEC(j)), readl(MEM_REC_PWR_NSEC(j)),
			readl(MEM_REC_PWR_ID(j)), readl(MEM_REC_PWR_OPP(j)));

	// user opp
	i = readl(MEM_REC_USR_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_USR_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] pwr:%u usr:%u opp:%u",
				readl(MEM_REC_USR_SEC(j)), readl(MEM_REC_USR_NSEC(j)),
				readl(MEM_REC_USR_PWR(j)),
				readl(MEM_REC_USR_ID(j)), readl(MEM_REC_USR_OPP(j)));

	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] pwr:%u usr:%u opp:%u",
			readl(MEM_REC_USR_SEC(j)), readl(MEM_REC_USR_NSEC(j)),
			readl(MEM_REC_USR_PWR(j)),
			readl(MEM_REC_USR_ID(j)), readl(MEM_REC_USR_OPP(j)));

	//user latest request opp
	mmdvfs_debug_dump_line(file, "user latest request opp");
	for (i = 0; i < USER_NUM; i++)
		mmdvfs_debug_dump_line(file, "user: %u opp: %u", i, readl(MEM_VOTE_OPP_USR(i)));

	mmdvfs_debug_dump_line(file, "clkmux:%#x clkmux_done:%#x",
		readl(MEM_CLKMUX_ENABLE), readl(MEM_CLKMUX_ENABLE_DONE));

	/* MMDVFS_DBG_VER3.5 */
	mmdvfs_debug_dump_line(file, "VER3.5: mux controlled by vcp:");

	//user latest request freq/opp
	mmdvfs_debug_dump_line(file, "user latest request opp/freq");
	for (i = 0; i < MMDVFS_USER_NUM; i++) {
		bool vcp = readl(MEM_USR_OPP(i, true)) != MAX_OPP;

		mmdvfs_debug_dump_line(file, "[%5u.%6u] user: %2u opp: %u freq: %u",
			readl(MEM_USR_OPP_SEC(i, vcp)), readl(MEM_USR_OPP_USEC(i, vcp)), i,
			readl(MEM_USR_OPP(i, vcp)), readl(MEM_USR_FREQ(i, vcp)));
	}

	if (mmdvfs_get_mmup_sram_enable())
		goto sram_dump;

	//power opp/gear
	mmdvfs_debug_dump_line(file, "power latest opp/gear");
	for (i = 0; i < PWR_MMDVFS_NUM; i++)
		mmdvfs_debug_dump_line(file, "[%5u.%6u] power: %u opp: %u gear: %u",
			readl(MEM_PWR_OPP_SEC(i)), readl(MEM_PWR_OPP_USEC(i)), i,
			readl(MEM_PWR_OPP(i)), readl(MEM_PWR_CUR_GEAR(i)));

	//mux opp/min
	mmdvfs_debug_dump_line(file, "mux latest opp/min");
	for (i = 0; i < USER_NUM; i++)
		mmdvfs_debug_dump_line(file, "[%5u.%6u] mux: %2u opp: %u min: %u",
			readl(MEM_MUX_OPP_SEC(i)), readl(MEM_MUX_OPP_USEC(i)), i,
			readl(MEM_MUX_OPP(i)), readl(MEM_MUX_MIN(i)));

	// mux opp records
	mmdvfs_debug_dump_line(file, "mux record opp/min/level");
	i = readl(MEM_REC_MUX_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_MUX_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++) {
			val = readl(MEM_REC_MUX_VAL(j));
			mmdvfs_debug_dump_line(file,
				"[%5u.%6u] mux:%lu opp:%lu min:%lu level:%lu",
				readl(MEM_REC_MUX_SEC(j)), readl(MEM_REC_MUX_USEC(j)),
				(val >> 24) & GENMASK(7, 0), (val >> 16) & GENMASK(7, 0),
				(val >> 8) & GENMASK(7, 0), val & GENMASK(7, 0));
		}

	for (j = 0; j < i; j++) {
		val = readl(MEM_REC_MUX_VAL(j));
		mmdvfs_debug_dump_line(file, "[%5u.%6u] mux:%lu opp:%lu min:%lu level:%lu",
			readl(MEM_REC_MUX_SEC(j)), readl(MEM_REC_MUX_USEC(j)),
			(val >> 24) & GENMASK(7, 0), (val >> 16) & GENMASK(7, 0),
			(val >> 8) & GENMASK(7, 0), val & GENMASK(7, 0));
	}

	// power/alone records
	mmdvfs_debug_dump_line(file, "power/alone opp records");
	for (i = 0; i < MMDVFS_OPP_RECORD_NUM; i++) {
		j = readl(MEM_REC_PWR_ALN_CNT(i)) % MEM_REC_CNT_MAX;
		if (readl(MEM_REC_PWR_ALN_SEC(i, j)))
			for (k = j; k < MEM_REC_CNT_MAX; k++)
				mmdvfs_debug_dump_line(file, "[%5u.%6u] pwr:%u opp:%u",
					readl(MEM_REC_PWR_ALN_SEC(i, k)),
					readl(MEM_REC_PWR_ALN_NSEC(i, k)),
					i, readl(MEM_REC_PWR_ALN_OPP(i, k)));

		for (k = 0; k < j; k++)
			mmdvfs_debug_dump_line(file, "[%5u.%6u] pwr:%u opp:%u",
				readl(MEM_REC_PWR_ALN_SEC(i, k)),
				readl(MEM_REC_PWR_ALN_NSEC(i, k)),
				i, readl(MEM_REC_PWR_ALN_OPP(i, k)));
	}

	// latest mux cb
	mmdvfs_debug_dump_line(file, "latest mux cb mux/opp");
	val = readl(MEM_MUX_CB_MUX_OPP);
	mmdvfs_debug_dump_line(file, "[%5u.%6u] mux:%lu opp:%lu",
		readl(MEM_MUX_CB_SEC), readl(MEM_MUX_CB_USEC), (val >> 8) & GENMASK(7, 0), val & GENMASK(7, 0));
	mmdvfs_debug_dump_line(file, "[%5u.%6u] mux:%lu opp:%lu",
		readl(MEM_MUX_CB_END_SEC), readl(MEM_MUX_CB_END_USEC), (val >> 8) & GENMASK(7, 0), val & GENMASK(7, 0));

	// vcore ceil
	mmdvfs_debug_dump_line(file, "vcore ceil:%#x", readl(MEM_CEIL_LEVEL(PWR_MMDVFS_VCORE)));

	// vmm ceil records
	i = readl(MEM_REC_VMM_CEIL_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_VMM_CEIL_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%6u] vmm_ceil_enable:%#x",
				readl(MEM_REC_VMM_CEIL_SEC(j)), readl(MEM_REC_VMM_CEIL_USEC(j)),
				readl(MEM_REC_VMM_CEIL_VAL(j)));

	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%6u] vmm_ceil_enable:%#x",
			readl(MEM_REC_VMM_CEIL_SEC(j)), readl(MEM_REC_VMM_CEIL_USEC(j)),
			readl(MEM_REC_VMM_CEIL_VAL(j)));

	// vcp exception
	if (readl(MEM_VCP_EXC_SEC)) {
		val = readl(MEM_VCP_EXC_VAL);
		mmdvfs_debug_dump_line(file, "[%5u.%6u] exception_handler",
			readl(MEM_VCP_EXC_SEC), readl(MEM_VCP_EXC_USEC));
		mmdvfs_debug_dump_line(file, " vcore:%lu, vmm:%lu, vdisp:%lu",
			val & GENMASK(7, 0), (val >> 8) & GENMASK(7, 0),
			(val >> 16) & GENMASK(7, 0));
	}

	// vmm debug
	mmdvfs_debug_dump_line(file, "VMM Efuse_low:%#x, Efuse_high:%#x",
		readl(MEM_VMM_EFUSE_LOW), readl(MEM_VMM_EFUSE_HIGH));
	for (j = 0; j < 8; j++)
		mmdvfs_debug_dump_line(file, "VMM voltage level%d:%u",
			j, readl(MEM_VMM_OPP_VOLT(j)));

	i = readl(MEM_REC_VMM_DBG_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_VMM_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++) {
			mmdvfs_debug_dump_line(file, "[%5u.%3u] volt:%u temp:%#x avs:%#x",
				readl(MEM_REC_VMM_SEC(j)), readl(MEM_REC_VMM_NSEC(j)),
				readl(MEM_REC_VMM_VOLT(j)),
				readl(MEM_REC_VMM_TEMP(j)), readl(MEM_REC_VMM_AVS(j)));
		}

	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] volt:%u temp:%#x avs:%#x",
			readl(MEM_REC_VMM_SEC(j)), readl(MEM_REC_VMM_NSEC(j)),
			readl(MEM_REC_VMM_VOLT(j)),
			readl(MEM_REC_VMM_TEMP(j)), readl(MEM_REC_VMM_AVS(j)));
	return;

sram_dump:
	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_RST);
	mmdvfs_vcp_cb_mutex_lock();
	if (!mmdvfs_vcp_cb_ready_get()) {
		mmdvfs_vcp_cb_mutex_unlock();
		MMDVFS_DBG("cb_ready:%d", mmdvfs_vcp_cb_ready_get());
		return;
	}

	mmdvfs_debug_dump_line(file, "VER3.5: mux controlled by vcp sram:%#lx", (unsigned long)(void *)SRAM_BASE);
	// usr
	for (k = 0; k < SRAM_USR_NUM; k++) {
		i = readl(SRAM_REC_CNT_USR(k)) % SRAM_REC_CNT;
		for (j = i; j < SRAM_REC_CNT; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] usr:%u opp:%u",
				readl(SRAM_USR_SEC(k, j)), readl(SRAM_USR_USEC(k, j)),
				k, readl(SRAM_USR_VAL(k, j)));
		for (j = 0; j < i; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] usr:%u opp:%u",
				readl(SRAM_USR_SEC(k, j)), readl(SRAM_USR_USEC(k, j)),
				k, readl(SRAM_USR_VAL(k, j)));
	}

	// mux
	i = readl(SRAM_REC_CNT_MUX) % SRAM_REC_CNT;
	for (j = i; j < SRAM_REC_CNT; j++) {
		val = readl(SRAM_MUX_VAL(j));
		mmdvfs_debug_dump_line(file, "[%5u.%3u] mux:%lu opp:%lu min:%lu level:%lu",
			readl(SRAM_MUX_SEC(j)), readl(SRAM_MUX_USEC(j)),
			(val >> 24) & GENMASK(7, 0), (val >> 16) & GENMASK(7, 0),
			(val >> 8) & GENMASK(7, 0), val & GENMASK(7, 0));
	}
	for (j = 0; j < i; j++) {
		val = readl(SRAM_MUX_VAL(j));
		mmdvfs_debug_dump_line(file, "[%5u.%3u] mux:%lu opp:%lu min:%lu level:%lu",
			readl(SRAM_MUX_SEC(j)), readl(SRAM_MUX_USEC(j)),
			(val >> 24) & GENMASK(7, 0), (val >> 16) & GENMASK(7, 0),
			(val >> 8) & GENMASK(7, 0), val & GENMASK(7, 0));
	}

	// pwr
	for (k = 0; k < SRAM_PWR_CNT; k++) {
		i = readl(SRAM_REC_CNT_PWR(k)) % SRAM_REC_CNT;
		for (j = i; j < SRAM_REC_CNT; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] pwr:%u opp:%u",
				readl(SRAM_PWR_SEC(k, j)), readl(SRAM_PWR_USEC(k, j)),
				k, readl(SRAM_PWR_VAL(k, j)));
		for (j = 0; j < i; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] pwr:%u opp:%u",
				readl(SRAM_PWR_SEC(k, j)), readl(SRAM_PWR_USEC(k, j)),
				k, readl(SRAM_PWR_VAL(k, j)));
	}

	// clk
	for (k = 0; k < SRAM_PWR_CNT; k++) {
		i = readl(SRAM_REC_CNT_CLK(k)) % SRAM_REC_CNT;
		for (j = i; j < SRAM_REC_CNT; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] clk:%u opp:%u",
				readl(SRAM_CLK_SEC(k, j)), readl(SRAM_CLK_USEC(k, j)),
				k, readl(SRAM_CLK_VAL(k, j)));
		for (j = 0; j < i; j++)
			mmdvfs_debug_dump_line(file, "[%5u.%3u] clk:%u opp:%u",
				readl(SRAM_CLK_SEC(k, j)), readl(SRAM_CLK_USEC(k, j)),
				k, readl(SRAM_CLK_VAL(k, j)));
	}

	// rate
	i = readl(SRAM_REC_CNT_RATE) % SRAM_REC_CNT;
	for (j = i; j < SRAM_REC_CNT; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] rate:%u",
			readl(SRAM_RATE_SEC(j)), readl(SRAM_RATE_USEC(j)),readl(SRAM_RATE_VAL(j)));
	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] rate:%u",
			readl(SRAM_RATE_SEC(j)), readl(SRAM_RATE_USEC(j)),readl(SRAM_RATE_VAL(j)));

	// ceil
	i = readl(SRAM_REC_CNT_CEIL) % SRAM_REC_CNT;
	for (j = i; j < SRAM_REC_CNT; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] ceil:%u",
			readl(SRAM_CEIL_SEC(j)), readl(SRAM_CEIL_USEC(j)),readl(SRAM_CEIL_VAL(j)));
	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] ceil:%u",
			readl(SRAM_CEIL_SEC(j)), readl(SRAM_CEIL_USEC(j)),readl(SRAM_CEIL_VAL(j)));

	// vmm
	i = readl(SRAM_REC_CNT_VMM) % SRAM_REC_CNT;
	for (j = i; j < SRAM_REC_CNT; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] vmm val:%u hw:%u volt:%u",
			readl(SRAM_VMM_SEC(j)), readl(SRAM_VMM_USEC(j)),
			readl(SRAM_VMM_VAL(j)), readl(SRAM_VMM_HW_VAL(j)), readl(SRAM_VMM_VOLT(j)));
	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] vmm val:%u hw:%u volt:%u",
			readl(SRAM_VMM_SEC(j)), readl(SRAM_VMM_USEC(j)),
			readl(SRAM_VMM_VAL(j)), readl(SRAM_VMM_HW_VAL(j)), readl(SRAM_VMM_VOLT(j)));
	mmdvfs_debug_dump_line(file, "vmm efuse high:%u low:%u", readl(SRAM_VMM_EFUSE_HIGH), readl(SRAM_VMM_EFUSE_LOW));

	// vdisp
	i = readl(SRAM_REC_CNT_VDISP) % SRAM_REC_CNT;
	for (j = i; j < SRAM_REC_CNT; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] vdisp:%u",
			readl(SRAM_VDISP_SEC(j)), readl(SRAM_VDISP_USEC(j)),readl(SRAM_VDISP_VAL(j)));
	for (j = 0; j < i; j++)
		mmdvfs_debug_dump_line(file, "[%5u.%3u] vdisp:%u",
			readl(SRAM_VDISP_SEC(j)), readl(SRAM_VDISP_USEC(j)),readl(SRAM_VDISP_VAL(j)));

	// mux min, pwr gear, vmm ceil
	for (i = 0; i < SRAM_MUX_CNT; i++)
		mmdvfs_debug_dump_line(file, "mux:%d min:%u", i, readl(SRAM_MUX_MIN(i)));
	for (i = 0; i < SRAM_PWR_CNT - 1; i++)
		mmdvfs_debug_dump_line(file, "pwr:%d gear:%u", i, readl(SRAM_PWR_GEAR(i)));
	mmdvfs_debug_dump_line(file, "pwr:%d ceil:%u", i, readl(SRAM_VMM_CEIL));

	mmdvfs_vcp_cb_mutex_unlock();
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_status_dump);

static int mmdvfs_debug_opp_show(struct seq_file *file, void *data)
{
	u32 i, j;

	mmdvfs_debug_status_dump(file);

	if (!MEM_BASE)
		return 0;

	// power total time
	mmdvfs_debug_dump_line(file, "power/alone total time(ms)");
	for (i = 0; i < MMDVFS_OPP_RECORD_NUM; i++)
		for (j = 0; j < MAX_OPP; j++)
			mmdvfs_debug_dump_line(file, "pwr:%u opp:%u total_time:%llu",i, j, readq(
				!mmdvfs_get_mmup_sram_enable() ? MEM_PWR_TOTAL_TIME(i, j) : SRAM_PWR_TOTAL(i, j)));

	return 0;
}

static int mmdvfs_debug_opp_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmdvfs_debug_opp_show, inode->i_private);
}

static const struct proc_ops mmdvfs_debug_opp_fops = {
	.proc_open = mmdvfs_debug_opp_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int mmdvfs_mbrain_test(struct seq_file *file, void *data)
{
	struct mmdvfs_res_mbrain_debug_ops *ops = NULL;
	unsigned int data_size = 0;
	unsigned char *ptr = NULL, *addr = NULL;
	u32 i, j;

	struct mmdvfs_res_mbrain_header header;
	struct mmdvfs_opp_record record[MMDVFS_OPP_RECORD_NUM];

	ops = get_mmdvfs_mbrain_dbg_ops();
	if (ops && ops->get_length && ops->get_data) {
		data_size = ops->get_length();
		seq_printf(file, "data_size:%u\n", data_size);

		if (data_size > 0) {
			ptr = kmalloc(data_size, GFP_KERNEL);
			if (!ptr)
				return 0;

			ops->get_data(ptr, data_size);

			memcpy(&header, ptr, sizeof(header));
			addr = ptr + sizeof(header);
			memcpy(&record, addr, sizeof(record));

			kfree(ptr);
			ptr = NULL;

			seq_printf(file, "header mbrain_module:%u, version:%u ",
				header.mbrain_module, header.version);
			seq_printf(file, "data_offset:%u index_data_length:%u\n",
				header.data_offset, header.index_data_length);

			for (i = 0; i < MMDVFS_OPP_RECORD_NUM; i++)
				for (j = 0; j < MAX_OPP; j++)
					seq_printf(file, "pwr:%u opp:%u total_time:%llu\n",
						i, j, record[i].opp_duration[j]);
		}
	}

	return 0;
}

static int mmdvfs_mbrain_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmdvfs_mbrain_test, inode->i_private);
}

static const struct proc_ops mmdvfs_mbrain_test_fops = {
	.proc_open = mmdvfs_mbrain_test_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int mtk_mmdvfs_debug_force_vcore_notify(const u32 val)
{
	return mtk_mmdvfs_force_vcore_notify(val);
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_force_vcore_notify);

bool mtk_is_mmdvfs_v3_debug_init_done(void)
{
	return mmdvfs_v3_debug_init_done;
}
EXPORT_SYMBOL_GPL(mtk_is_mmdvfs_v3_debug_init_done);

static int mmdvfs_v3_debug_thread(void *data)
{
	phys_addr_t pa = 0ULL;
	unsigned long va;
	int ret = 0, retry = 0, i;

	while (!mmdvfs_is_init_done()) {
		if (++retry > 100) {
			MMDVFS_DBG("mmdvfs_v3 init not ready");
			goto init_done;
		}
		ssleep(2);
	}

	va = (unsigned long)(unsigned long *)mmdvfs_get_mmup_base(&pa);
	if (va && pa) {
		ret = mrdump_mini_add_extra_file(va, pa, PAGE_SIZE, "MMDVFS_OPP");
		if (ret)
			MMDVFS_DBG("failed:%d va:%#lx pa:%pa", ret, va, &pa);
	}

	va = (unsigned long)(unsigned long *)mmdvfs_get_vcp_base(&pa);
	if (va && pa) {
		ret = mrdump_mini_add_extra_file(va, pa, PAGE_SIZE, "MMDVFS_OPP_VCP");
		if (ret)
			MMDVFS_DBG("failed:%d va:%#lx pa:%pa", ret, va, &pa);
	}

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VCORE))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VCORE, g_mmdvfs->force_step0, false);

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VMM))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VMM, g_mmdvfs->force_step0, false);

	if (!g_mmdvfs->release_step0)
		goto init_done;

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VCORE))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VCORE, -1, false);

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VMM))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VMM, -1, false);

	if (g_mmdvfs->vote_step != 0xff)
		for (i = 0; i < PWR_MMDVFS_NUM; i++) {
			mtk_mmdvfs_v3_set_vote_step(
				g_mmdvfs->vote_step >> 4 & 0xf, g_mmdvfs->vote_step & 0xf, false);
			g_mmdvfs->vote_step = g_mmdvfs->vote_step >> 8;
		}

	if (g_mmdvfs->force_step != 0xff)
		for (i = 0; i < PWR_MMDVFS_NUM; i++) {
			mtk_mmdvfs_v3_set_force_step(
				g_mmdvfs->force_step >> 4 & 0xf, g_mmdvfs->force_step & 0xf, false);
			g_mmdvfs->force_step = g_mmdvfs->force_step >> 8;
		}

init_done:
	mmdvfs_v3_debug_init_done = true;
	return 0;
}

static int mmdvfs_v1_dbg_ftrace_thread(void *data)
{
	static u8 old_cnt;
	unsigned long flags;
	int ret = 0;
	s32 i;

	if (!g_mmdvfs) {
		ftrace_v1_ena = false;
		return 0;
	}

	ftrace_v1_ena = true;
	while (!kthread_should_stop()) {

		spin_lock_irqsave(&g_mmdvfs->lock, flags);

		if (g_mmdvfs->rec_cnt != old_cnt) {
			if (g_mmdvfs->rec_cnt > old_cnt) {
				for (i = old_cnt; i < g_mmdvfs->rec_cnt; i++)
					ftrace_record_opp_v1(1, g_mmdvfs->rec[i].opp);
			} else {
				for (i = old_cnt; i < ARRAY_SIZE(g_mmdvfs->rec); i++)
					ftrace_record_opp_v1(1, g_mmdvfs->rec[i].opp);

				for (i = 0; i < g_mmdvfs->rec_cnt; i++)
					ftrace_record_opp_v1(1, g_mmdvfs->rec[i].opp);
			}

			old_cnt = g_mmdvfs->rec_cnt;
		}

		spin_unlock_irqrestore(&g_mmdvfs->lock, flags);

		msleep(20);
	}

	ftrace_v1_ena = false;
	MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v1 end");
	return ret;
}

static int mmdvfs_v3_dbg_ftrace_thread(void *data)
{
	s32 i;

	if (!g_mmdvfs || !mmdvfs_is_init_done()) {
		ftrace_v3_ena = false;
		return 0;
	}

	MMDVFS_DBG("wait mmdvfs_v3 ready");
	if (!MEM_BASE) {
		MMDVFS_DBG("mmdvfs_v3 MEM_BASE not ready");
		ftrace_v3_ena = false;
		return 0;
	}

	MMDVFS_DBG("mmdvfs_v3 init & MEM_BASE ready");
	ftrace_v3_ena = true;
	while (!kthread_should_stop()) {
		if (mmdvfs_get_version()) {  //mmdvfs v3.5
			// power opp
			for (i = 0; i < PWR_MMDVFS_NUM; i++)
				if (!mmdvfs_get_mmup_sram_enable())
					ftrace_pwr_opp_v3(i, readl(MEM_PWR_OPP(i)));
				else
					ftrace_pwr_opp_v3(i, readl(SRAM_PWR_VAL(i,
						(readl(SRAM_REC_CNT_PWR(i)) + SRAM_REC_CNT - 1) % SRAM_REC_CNT)));
			// user opp
			for (i = 0; i < MMDVFS_USER_NUM; i++)
				if (!mmdvfs_get_mmup_sram_enable() || (i != MMDVFS_USER_DISP && i != MMDVFS_USER_SMI))
					ftrace_user_opp_v3(i, readl(MEM_USR_OPP(i,
						readl(MEM_USR_OPP(i, true)) != MAX_OPP)));
				else if (i == MMDVFS_USER_DISP)
					ftrace_user_opp_v3(SRAM_USR_DPC, readl(SRAM_USR_VAL(SRAM_USR_DPC, (readl(
						SRAM_REC_CNT_USR(SRAM_USR_DPC)) + SRAM_REC_CNT - 1) % SRAM_REC_CNT)));
				else if (i == MMDVFS_USER_SMI)
					ftrace_user_opp_v3(SRAM_USR_SMI, readl(SRAM_USR_VAL(SRAM_USR_SMI, (readl(
						SRAM_REC_CNT_USR(SRAM_USR_SMI)) + SRAM_REC_CNT - 1) % SRAM_REC_CNT)));
		} else {                     //mmdvfs v3.0
			// power opp
			for (i = 0; i <= PWR_MMDVFS_VMM; i++)
				ftrace_pwr_opp_v3(i, readl(MEM_VOTE_OPP_PWR(i)));

			// user opp
			for (i = 0; i < USER_NUM; i++) {
				if (g_mmdvfs->user_pwr[i] == PWR_MMDVFS_VCORE)
					ftrace_user_opp_v3_vcore(i, readl(MEM_VOTE_OPP_USR(i)));
				if (g_mmdvfs->user_pwr[i] == PWR_MMDVFS_VMM)
					ftrace_user_opp_v3_vmm(i, readl(MEM_VOTE_OPP_USR(i)));
			}
		}

		msleep(1);
	}

	ftrace_v3_ena = false;
	MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v3 end");
	return 0;
}

static int mmdvfs_debug_set_ftrace(const char *val,
	const struct kernel_param *kp)
{
	static struct task_struct *kthr_v1, *kthr_v3;
	u32 ver = 0, ena = 0;
	int ret;

	ret = sscanf(val, "%u %u", &ver, &ena);
	if (ret != 2) {
		MMDVFS_DBG("failed:%d ver:%hhu ena:%hhu", ret, ver, ena);
		return -EINVAL;
	}

	if (!mmdvfs_met_freerun) {
		MMDVFS_DBG("mmdvfs met not freerun");
		return 0;
	}

	if (ver & MMDVFS_DBG_VER1) {
		if (ena) {
			if (ftrace_v1_ena)
				MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v1 already created");
			else {
				kthr_v1 = kthread_run(
					mmdvfs_v1_dbg_ftrace_thread, NULL, "mmdvfs-dbg-ftrace-v1");
				if (IS_ERR(kthr_v1))
					MMDVFS_DBG("create kthread mmdvfs-dbg-ftrace-v1 failed");
			}
		} else {
			if (ftrace_v1_ena) {
				ret = kthread_stop(kthr_v1);
				if (!ret) {
					MMDVFS_DBG("stop kthread mmdvfs-dbg-ftrace-v1");
					ftrace_v1_ena = false;
				}
			}
		}
	}

	if (ver & MMDVFS_DBG_VER3) {
		if (ena) {
			if (ftrace_v3_ena)
				MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v3 already created");
			else {
				kthr_v3 = kthread_run(
					mmdvfs_v3_dbg_ftrace_thread, NULL, "mmdvfs-dbg-ftrace-v3");
				if (IS_ERR(kthr_v3))
					MMDVFS_DBG("create kthread mmdvfs-dbg-ftrace-v3 failed");
			}
		} else {
			if (ftrace_v3_ena) {
				ret = kthread_stop(kthr_v3);
				if (!ret) {
					MMDVFS_DBG("stop kthread mmdvfs-dbg-ftrace-v3");
					ftrace_v3_ena = false;
				}
			}
		}
	}

	return 0;
}

static struct kernel_param_ops mmdvfs_debug_set_ftrace_ops = {
	.set = mmdvfs_debug_set_ftrace,
};
module_param_cb(ftrace, &mmdvfs_debug_set_ftrace_ops, NULL, 0644);
MODULE_PARM_DESC(ftrace, "mmdvfs ftrace log");

static struct mmdvfs_res_mbrain_header mmdvfs_mbrain_header;

static unsigned int mmdvfs_mbrain_get_sys_res_length(void)
{
	mmdvfs_mbrain_header.mbrain_module = MMDVFS_RES_DATA_MODULE_ID;
	mmdvfs_mbrain_header.version = MMDVFS_RES_DATA_VERSION;
	mmdvfs_mbrain_header.data_offset = sizeof(struct mmdvfs_res_mbrain_header);
	mmdvfs_mbrain_header.index_data_length = mmdvfs_mbrain_header.data_offset +
		sizeof(struct mmdvfs_opp_record) * MMDVFS_OPP_RECORD_NUM;

	MMDVFS_DBG("length:%u", mmdvfs_mbrain_header.index_data_length);

	return mmdvfs_mbrain_header.index_data_length;
}

static void *mmdvfs_res_data_copy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

static int mmdvfs_mbrain_get_sys_res_data(void *address, uint32_t size)
{
	struct mmdvfs_opp_record record[MMDVFS_OPP_RECORD_NUM];
	uint64_t ns, sec, usec, us, total;
	int i, j, k, opp;

	ns = sched_clock();
	us = ns / 1000;

	if (!mmdvfs_get_mmup_sram_enable())
		goto dram_cal;

	// sram
	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_RST);
	for (i = 0; i < MMDVFS_OPP_RECORD_NUM; i++) {
		total = 0;
		j = (readl(SRAM_REC_CNT_CLK(i)) - 1 + SRAM_REC_CNT) % SRAM_REC_CNT;
		sec = readl(SRAM_CLK_SEC(i, j));
		usec = readl(SRAM_CLK_USEC(i, j));
		opp = readl(SRAM_CLK_VAL(i, j));

		for (j = 0; j < ARRAY_SIZE(record[i].opp_duration); j++) {
			record[i].opp_duration[j] = readq(SRAM_PWR_TOTAL(i, j));
			if (j == opp && sec && opp < MAX_OPP)
				record[i].opp_duration[j] += (us - (sec * 1000000 + usec)) / 1000;
		}
	}
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);
	goto copy_end;

dram_cal:
	for (i = 0; i < MMDVFS_OPP_RECORD_NUM; i++) {
		total = 0;
		k = readl(MEM_REC_PWR_ALN_CNT(i)) % MEM_REC_CNT_MAX;
		k = (k - 1) < 0 ? MEM_REC_CNT_MAX - 1 : k - 1;
		sec = readl(MEM_REC_PWR_ALN_SEC(i, k));
		usec = readl(MEM_REC_PWR_ALN_NSEC(i, k));
		opp = readl(MEM_REC_PWR_ALN_OPP(i, k));

		if (sec && opp < MAX_OPP) {
			total = readq(MEM_PWR_TOTAL_TIME(i, opp));
			total += (us - (sec * 1000000 + usec)) / 1000;
		}

		MMDVFS_DBG("pwr:%d k:%d sec:%llu usec:%llu opp:%u us:%llu total:%llu",
			i, k, sec, usec, opp, us, total);

		for (j = 0; j < MAX_OPP; j++) {
			if (j == opp)
				record[i].opp_duration[j] = total;
			else
				record[i].opp_duration[j] = readq(MEM_PWR_TOTAL_TIME(i, j));
		}
	}

copy_end:
	mmdvfs_mbrain_get_sys_res_length();
	address = mmdvfs_res_data_copy(address, &mmdvfs_mbrain_header,
		sizeof(struct mmdvfs_res_mbrain_header));
	address = mmdvfs_res_data_copy(address, &record,
		sizeof(struct mmdvfs_opp_record) * MMDVFS_OPP_RECORD_NUM);

	return 0;
}

static struct mmdvfs_res_mbrain_debug_ops mmdvfs_mbrain_ops = {
	.get_length = mmdvfs_mbrain_get_sys_res_length,
	.get_data = mmdvfs_mbrain_get_sys_res_data,
};

struct mmdvfs_res_mbrain_debug_ops *get_mmdvfs_mbrain_dbg_ops(void)
{
	if (!MEM_BASE)
		return NULL;

	return &mmdvfs_mbrain_ops;
}
EXPORT_SYMBOL_GPL(get_mmdvfs_mbrain_dbg_ops);

static void mmdvfs_debug_work(struct work_struct *work)
{
	if (!IS_ERR_OR_NULL(g_mmdvfs->reg_vcore))
		MMDVFS_DBG("vcore enabled:%d voltage:%d", regulator_is_enabled(g_mmdvfs->reg_vcore),
			regulator_get_voltage(g_mmdvfs->reg_vcore));

	if (!IS_ERR_OR_NULL(g_mmdvfs->reg_vmm))
		MMDVFS_DBG("vmm enabled:%d voltage:%d", regulator_is_enabled(g_mmdvfs->reg_vmm),
			regulator_get_voltage(g_mmdvfs->reg_vmm));

	if (!IS_ERR_OR_NULL(g_mmdvfs->reg_vdisp))
		MMDVFS_DBG("vdisp enabled:%d voltage:%d", regulator_is_enabled(g_mmdvfs->reg_vdisp),
			regulator_get_voltage(g_mmdvfs->reg_vdisp));
}

static int mmdvfs_debug_smi_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	int i;
	unsigned int val;

	if (!work_pending(&g_mmdvfs->work))
		queue_work(g_mmdvfs->workq, &g_mmdvfs->work);
	else
		MMDVFS_DBG("mmdvfs_debug_work fail");

	for (i = 0; i < g_mmdvfs->fmeter_count; i++) {
		val = mt_get_fmeter_freq(g_mmdvfs->fmeter_id[i], g_mmdvfs->fmeter_type[i]);
		MMDVFS_DBG("i:%d id:%hu type:%hu freq:%u",
			i, g_mmdvfs->fmeter_id[i], g_mmdvfs->fmeter_type[i], val);
	}

	for (i = 0; i < g_mmdvfs->clk_count; i++)
		MMDVFS_DBG("[%#010x] = %#010x", g_mmdvfs->clk_base_pa + g_mmdvfs->clk_ofs[i],
			readl(g_mmdvfs->clk_base + g_mmdvfs->clk_ofs[i]));
	return 0;
}

static int mmdvfs_debug_mmdebug_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	mmdvfs_debug_status_dump(NULL);
	return 0;
}

static int mmdvfs_debug_parse_ver1(void)
{
	struct regulator *reg;
	int ret;

	reg = devm_regulator_get(g_mmdvfs->dev, "dvfsrc-vcore");
	if (IS_ERR_OR_NULL(reg)) {
		MMDVFS_DBG("devm_regulator_get failed:%ld", PTR_ERR(reg));
		return PTR_ERR(reg);
	}
	g_mmdvfs->reg = reg;

	ret = regulator_count_voltages(reg);
	if (ret < 0) {
		MMDVFS_DBG("regulator_count_voltages failed:%d", ret);
		return ret;
	}
	g_mmdvfs->reg_cnt_vol = (u32)ret;

	ret = of_property_read_u32(g_mmdvfs->dev->of_node, "force-step0", &g_mmdvfs->force_step0);
	if (ret) {
		MMDVFS_DBG("force_step0:%u failed:%d", g_mmdvfs->force_step0, ret);
		return ret;
	}

	if (g_mmdvfs->force_step0 >= g_mmdvfs->reg_cnt_vol) {
		MMDVFS_DBG("force_step0:%u cannot larger reg_cnt_vol:%u",
			g_mmdvfs->force_step0, g_mmdvfs->reg_cnt_vol);
		return -EINVAL;
	}

	ret = regulator_list_voltage(reg, g_mmdvfs->reg_cnt_vol - 1 - g_mmdvfs->force_step0);
	MMDVFS_DBG("reg:%p reg_cnt_vol:%u force_step0:%u ret:%d",
		reg, g_mmdvfs->reg_cnt_vol, g_mmdvfs->force_step0, ret);
	regulator_set_voltage(reg, ret, INT_MAX);

	ret = of_property_read_u32(
		g_mmdvfs->dev->of_node, "release-step0", &g_mmdvfs->release_step0);
	if (ret)
		MMDVFS_DBG("release_step0:%u failed:%d", g_mmdvfs->release_step0, ret);

	return ret;
}

static int mmdvfs_debug_parse_ver3(void)
{
	const char *MMDVFS_CLK_NAMES = "mediatek,mmdvfs-clock-names";
	const char *MMDVFS_CLKS = "mediatek,mmdvfs-clocks";
	struct device_node *mmdvfs_clk_node;
	struct of_phandle_args spec;
	u8 mmdvfs_clk_num;
	int i, ret;

	/* user & power id mapping */
	mmdvfs_clk_node = of_parse_phandle(g_mmdvfs->dev->of_node, "mediatek,mmdvfs-clk", 0);
	if (!mmdvfs_clk_node) {
		MMDVFS_DBG("find mmdvfs_clk node failed");
		return 0;
	}

	ret = of_property_count_strings(mmdvfs_clk_node, MMDVFS_CLK_NAMES);
	if (ret <= 0) {
		MMDVFS_DBG("%s invalid:%d", MMDVFS_CLK_NAMES, ret);
		return ret;
	}
	mmdvfs_clk_num = ret;

	for (i = 0; i < mmdvfs_clk_num; i++) {
		ret = of_parse_phandle_with_args(
			mmdvfs_clk_node, MMDVFS_CLKS, "#mmdvfs,clock-cells", i, &spec);
		if (ret) {
			MMDVFS_DBG("parse %s i:%d failed:%d", MMDVFS_CLKS, i, ret);
			return ret;
		}
		g_mmdvfs->user_pwr[spec.args[2]] = spec.args[1];
	}
	return ret;
}

static int mmdvfs_debug_parse_fmeter(void)
{
	int ret;

	ret = of_property_count_u8_elems(g_mmdvfs->dev->of_node, "fmeter-id");
	if (ret < 0) {
		MMDVFS_DBG("count_elems fmeter-id failed:%d", ret);
		return 0;
	}
	g_mmdvfs->fmeter_count = ret;

	g_mmdvfs->fmeter_id = kcalloc(
		g_mmdvfs->fmeter_count, sizeof(*g_mmdvfs->fmeter_id), GFP_KERNEL);
	if (!g_mmdvfs->fmeter_id)
		return -ENOMEM;

	ret = of_property_read_u8_array(g_mmdvfs->dev->of_node,
		"fmeter-id", g_mmdvfs->fmeter_id, g_mmdvfs->fmeter_count);
	if (ret) {
		MMDVFS_DBG("read_array fmeter-id failed:%d", ret);
		return ret;
	}

	g_mmdvfs->fmeter_type = kcalloc(
		g_mmdvfs->fmeter_count, sizeof(*g_mmdvfs->fmeter_type), GFP_KERNEL);
	if (!g_mmdvfs->fmeter_type)
		return -ENOMEM;

	ret = of_property_read_u8_array(g_mmdvfs->dev->of_node,
		"fmeter-type", g_mmdvfs->fmeter_type, g_mmdvfs->fmeter_count);
	if (ret)
		MMDVFS_DBG("read_array fmeter-type failed:%d", ret);

	return ret;
}

static int mmdvfs_debug_parse_clk(void)
{
	u32 pa, ofs;
	int i, ret;

	ret = of_property_count_u32_elems(g_mmdvfs->dev->of_node, "clk-offsets");
	if (ret < 0) {
		MMDVFS_DBG("count_u32 clk-offsets failed:%d", ret);
		return 0;
	}
	g_mmdvfs->clk_count = ret;
	MMDVFS_DBG("count_u32 clk-offsets clk_count:%d", g_mmdvfs->clk_count);

	if (!g_mmdvfs->clk_count)
		return 0;

	ret = of_property_read_u32(g_mmdvfs->dev->of_node, "clk-base", &pa);
	if (ret) {
		MMDVFS_DBG("failed:%d base:%#x", ret, pa);
		return ret;
	}
	g_mmdvfs->clk_base_pa = pa;
	g_mmdvfs->clk_base = ioremap(pa, 0x1000);

	g_mmdvfs->clk_ofs = kcalloc(
		g_mmdvfs->clk_count, sizeof(*g_mmdvfs->clk_ofs), GFP_KERNEL);
	if (!g_mmdvfs->clk_ofs)
		return -ENOMEM;

	for (i = 0; i < g_mmdvfs->clk_count; i++) {
		ret = of_property_read_u32_index(g_mmdvfs->dev->of_node, "clk-offsets", i, &ofs);
		if (ret) {
			MMDVFS_DBG("failed:%d i:%d ofs:%#x", ret, i, ofs);
			return ret;
		}
		g_mmdvfs->clk_ofs[i] = ofs;
	}

	return ret;
}

static int mmdvfs_debug_probe(struct platform_device *pdev)
{
	struct proc_dir_entry *dir, *proc;
	struct task_struct *kthr;
	struct regulator *reg;
	int ret;

	g_mmdvfs = kzalloc(sizeof(*g_mmdvfs), GFP_KERNEL);
	if (!g_mmdvfs)
		return -ENOMEM;
	g_mmdvfs->dev = &pdev->dev;

	dir = proc_mkdir("mmdvfs", NULL);
	if (IS_ERR_OR_NULL(dir))
		MMDVFS_DBG("proc_mkdir failed:%ld", PTR_ERR(dir));

	proc = proc_create("mmdvfs_opp", 0440, dir, &mmdvfs_debug_opp_fops);
	if (IS_ERR_OR_NULL(proc))
		MMDVFS_DBG("proc_create failed:%ld", PTR_ERR(proc));
	else
		g_mmdvfs->proc = proc;

	proc = proc_create("mmdvfs_mbrain_test", 0440, dir, &mmdvfs_mbrain_test_fops);
	if (IS_ERR_OR_NULL(proc))
		MMDVFS_DBG("proc_create failed:%ld", PTR_ERR(proc));
	else
		g_mmdvfs->proc_mbrain = proc;

	ret = of_property_read_u32(
		g_mmdvfs->dev->of_node, "debug-version", &g_mmdvfs->debug_version);
	if (ret)
		MMDVFS_DBG("debug_version:%u failed:%d", g_mmdvfs->debug_version, ret);

	mmdvfs_met_freerun = of_property_read_bool(g_mmdvfs->dev->of_node, "mediatek,met-freerun");

	/* MMDVFS_DBG_VER1 */
	spin_lock_init(&g_mmdvfs->lock);
	mmdvfs_debug_record_opp_set_fp(mmdvfs_debug_record_opp);
	ret = mmdvfs_debug_parse_ver1();

	/* MMDVFS_DBG_VER3 */
	ret = mmdvfs_debug_parse_ver3();

	reg = devm_regulator_get(g_mmdvfs->dev, "vcore");
	if (IS_ERR_OR_NULL(reg))
		MMDVFS_DBG("devm_regulator_get vcore failed:%ld", PTR_ERR(reg));
	else
		g_mmdvfs->reg_vcore = reg;

	reg = devm_regulator_get(g_mmdvfs->dev, "vmm-pmic");
	if (IS_ERR_OR_NULL(reg))
		MMDVFS_DBG("devm_regulator_get vmm-pmic failed:%ld", PTR_ERR(reg));
	else
		g_mmdvfs->reg_vmm = reg;

	reg = devm_regulator_get(g_mmdvfs->dev, "vdisp-pmic");
	if (IS_ERR_OR_NULL(reg))
		MMDVFS_DBG("devm_regulator_get vdisp-pmic failed:%ld", PTR_ERR(reg));
	else
		g_mmdvfs->reg_vdisp = reg;

	g_mmdvfs->smi_dbg_nb.notifier_call = mmdvfs_debug_smi_cb;
	mtk_smi_dbg_register_notifier(&g_mmdvfs->smi_dbg_nb);
	g_mmdvfs->fmeter_nb.notifier_call = mmdvfs_debug_smi_cb;
	mtk_mmdvfs_fmeter_register_notifier(&g_mmdvfs->fmeter_nb);
	g_mmdvfs->mmdebug_nb.notifier_call = mmdvfs_debug_mmdebug_cb;
	mtk_mmdebug_status_dump_register_notifier(&g_mmdvfs->mmdebug_nb);

	g_mmdvfs->vote_step = 0xff;
	of_property_read_u32(g_mmdvfs->dev->of_node, "vote-step", &g_mmdvfs->vote_step);
	g_mmdvfs->force_step = 0xff;
	of_property_read_u32(g_mmdvfs->dev->of_node, "force-step", &g_mmdvfs->force_step);
	of_property_read_u32(g_mmdvfs->dev->of_node, "release-step0", &g_mmdvfs->release_step0);

	ret = mmdvfs_debug_parse_fmeter();
	ret = mmdvfs_debug_parse_clk();

	ret = of_property_read_u32(g_mmdvfs->dev->of_node, "use-v3-pwr", &g_mmdvfs->use_v3_pwr);
	if (g_mmdvfs->debug_version & MMDVFS_DBG_VER3) {
		kthr = kthread_run(mmdvfs_v3_debug_thread, NULL, "mmdvfs-dbg-vcp");
		if (IS_ERR(kthr))
			MMDVFS_DBG("create kthread mmdvfs_v3_debug_thread failed");
	}

	g_mmdvfs->workq = create_singlethread_workqueue("mmdvfs_debug_workq");
	INIT_WORK(&g_mmdvfs->work, mmdvfs_debug_work);
	return 0;
}

static int mmdvfs_debug_remove(struct platform_device *pdev)
{
	devm_regulator_put(g_mmdvfs->reg);
	kfree(g_mmdvfs);
	return 0;
}

static const struct of_device_id of_mmdvfs_debug_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs-debug",
	},
	{}
};

static struct platform_driver mmdvfs_debug_drv = {
	.probe = mmdvfs_debug_probe,
	.remove = mmdvfs_debug_remove,
	.driver = {
		.name = "mtk-mmdvfs-debug",
		.of_match_table = of_mmdvfs_debug_match_tbl,
	},
};

static int __init mmdvfs_debug_init(void)
{
	int ret;

	ret = platform_driver_register(&mmdvfs_debug_drv);
	if (ret)
		MMDVFS_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdvfs_debug_exit(void)
{
	platform_driver_unregister(&mmdvfs_debug_drv);
}

module_init(mmdvfs_debug_init);
module_exit(mmdvfs_debug_exit);
MODULE_DESCRIPTION("MMDVFS Debug Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

