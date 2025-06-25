// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include <dt-bindings/clock/mmdvfs-clk.h>
#include <dt-bindings/memory/mtk-smi-user.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk-mmdvfs-v3-memory.h"
#include <clk-fmeter.h>
#include "mtk-smi-dbg.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_status.h"
#endif

#include "mtk_dpc_v2.h"
#include "mtk_dpc_mmp.h"
#include "mtk_dpc_internal.h"

#include "mtk_log.h"
#include "mtk_disp_vidle.h"
#include "mtk-mml-dpc.h"
#include "mdp_dpc.h"
#include "mtk_vdisp.h"

int debug_mmp = 1;
module_param(debug_mmp, int, 0644);
int debug_dvfs;
module_param(debug_dvfs, int, 0644);
int debug_irq;
module_param(debug_irq, int, 0644);
int irq_aee;
module_param(irq_aee, int, 0644);
int mminfra_floor;
module_param(mminfra_floor, int, 0644);

int post_vlp_delay = 60;
module_param(post_vlp_delay, int, 0644);

u32 dump_begin;
module_param(dump_begin, uint, 0644);
u32 dump_lines = 40;
module_param(dump_lines, uint, 0644);

int debug_force_wait;
module_param(debug_force_wait, int, 0644);
u32 debug_presz;
module_param(debug_presz, uint, 0644);

static void __iomem *dpc_base;
static struct mtk_dpc *g_priv;

static const char trace_buf_mml_on[] = "C|-65536|MML1_power|1\n";
static const char trace_buf_mml_off[] = "C|-65536|MML1_power|0\n";
static noinline int tracing_mark_write(const char buf[])
{
#ifdef CONFIG_TRACING
	trace_puts(buf);
#endif
	return 0;
}

static struct mtk_dpc_mtcmos_cfg mt6989_mtcmos_cfg[DPC_SUBSYS_CNT] = {
/*	cfg     set    clr    pa va */
	{0x300, 0x320, 0x340, 0, 0, 0},
	{0x400, 0x420, 0x440, 0, 0, 0},
	{0x500, 0x520, 0x540, 0, 0, 0},
	{0x600, 0x620, 0x640, 0, 0, 0},
	{0x700, 0x720, 0x740, 0, 0, 0},
};

static struct mtk_dpc_mtcmos_cfg mt6991_mtcmos_cfg[DPC_SUBSYS_CNT] = {
	{0x500, 0x520, 0x540, 0, 0, 0},
	{0x580, 0x5A0, 0x5C0, 0, 0, 0},
	{0x600, 0x620, 0x640, 0, 0, 0},
	{0x680, 0x6A0, 0x6C0, 0, 0, 0},
	{0x700, 0x720, 0x740, 0, 0, 0},
	{0xB00, 0xB20, 0xB40, 0, 0, 0},
	{0xC00, 0xC20, 0xC40, 0, 0, 0},
	{0xD00, 0xD20, 0xD40, 0, 0, 0},
};

static struct mtk_dpc2_dt_usage mt6991_dt_usage[DPC2_VIDLE_CNT] = {
/* 0*/	{0, 500},						/* OVL/DISP0	EOF	OFF	*/
/* 1*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/* 2*/	{0, 0x13B13B},						/*		TE	PRETE	*/
/* 3*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/* 4*/	{0, 500},						/* DISP1	EOF	OFF	*/
/* 5*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/* 6*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ},			/*		TE	PRETE	*/
/* 7*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/* 8*/	{0, 0x13B13B},	/* VDISP */
/* 9*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*10*/	{1, DPC2_DT_POSTSZ},
/*11*/	{0, 500},	/* HRT */
/*12*/	{1, DPC2_DT_TE_120 - DPC2_DT_INFRA},
/*13*/	{1, DPC2_DT_POSTSZ},
/*14*/	{0, 0x13B13B},	/* SRT */
/*15*/	{0, 0x13B13B},
/*16*/	{0, 0x13B13B},
/*17*/	{0, 0x13B13B},	/* MMINFRA */
/*18*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*19*/	{1, DPC2_DT_POSTSZ},
/*20*/	{0, 0x13B13B},	/* INFRA */
/*21*/	{0, 0x13B13B},
/*22*/	{0, 0x13B13B},
/*23*/	{0, 0x13B13B},	/* MAINPLL */
/*24*/	{0, 0x13B13B},
/*25*/	{0, 0x13B13B},
/*26*/	{0, 0x13B13B},	/* MSYNC 2.0 */
/*27*/	{0, 0x13B13B},
/*28*/	{0, 0x13B13B},
/*29*/	{1, 100},	/* SAFE ZONE */
/*30*/	{0, 1000},	/* RESERVED */
/*31*/	{0, 0x13B13B},
/*32*/	{0, 500},						/* MML1		EOF	OFF	*/
/*33*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/*34*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ + 20},		/*		TE	PRETE	*/
/*35*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/*36*/	{0, 0x13B13B},	/* VDISP */
/*37*/	{0, 0x13B13B},
/*38*/	{0, 0x13B13B},
/*39*/	{0, 500},	/* HRT */
/*40*/	{1, DPC2_DT_TE_120 - DPC2_DT_INFRA},
/*41*/	{1, DPC2_DT_POSTSZ},
/*42*/	{0, 0x13B13B},	/* SRT */
/*43*/	{0, 0x13B13B},
/*44*/	{0, 0x13B13B},
/*45*/	{0, 0x13B13B},	/* MMINFRA */
/*46*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*47*/	{1, DPC2_DT_POSTSZ},
/*48*/	{0, 0x13B13B},	/* INFRA */
/*49*/	{0, 0x13B13B},
/*50*/	{0, 0x13B13B},
/*51*/	{0, 0x13B13B},	/* MAINPLL */
/*52*/	{0, 0x13B13B},
/*53*/	{0, 0x13B13B},
/*54*/	{0, 0x13B13B},	/* RESERVED */
/*55*/	{0, 0x13B13B},
/*56*/	{0, 0x13B13B},
/*57*/	{0, 0x13B13B},	/* DISP 26M */
/*58*/	{0, 0x13B13B},
/*59*/	{0, 0x13B13B},
/*60*/	{0, 0x13B13B},	/* DISP PMIC */
/*61*/	{0, 0x13B13B},
/*62*/	{0, 0x13B13B},
/*63*/	{0, 0x13B13B},	/* DISP VCORE */
/*64*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE},
/*65*/	{1, DPC2_DT_POSTSZ + DPC2_DT_DSIOFF},
/*66*/	{0, 0x13B13B},	/* MML 26M */
/*67*/	{0, 0x13B13B},
/*68*/	{0, 0x13B13B},
/*69*/	{0, 0x13B13B},	/* MML PMIC */
/*70*/	{0, 0x13B13B},
/*71*/	{0, 0x13B13B},
/*72*/	{0, 0x13B13B},	/* MML VCORE */
/*73*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE},
/*74*/	{1, DPC2_DT_POSTSZ + DPC2_DT_DSIOFF},
/*75*/	{0, 0x13B13B},	/* DSIPHY */
/*76*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION},
/*77*/	{1, DPC2_DT_POSTSZ},
};

static struct mtk_dpc_channel_bw_cfg mt6991_ch_bw_cfg[24] = {
/*	offset	shift	bw		bits	AXI	S/H	R/W	mmlsys	*/
/* 0*/	{0xA10,	0, 0, 0},	/*	[9:0]	00	S	R	0xA70	*/
/* 1*/	{0xA1C,	0, 0, 0},	/*	[9:0]	00	S	W	0xA7C	*/
/* 2*/	{0xA28,	0, 0, 0},	/*	[9:0]	00	H	R	0xA88	*/
/* 3*/	{0xA34,	0, 0, 0},	/*	[9:0]	00	H	W	0xA94	*/
/* 4*/	{0xA10,	12, 0, 0},	/*	[21:12]	01	S	R	0xA70	*/
/* 5*/	{0xA1C,	12, 0, 0},	/*	[21:12]	01	S	W	0xA7C	*/
/* 6*/	{0xA28,	12, 0, 0},	/*	[21:12]	01	H	R	0xA88	*/
/* 7*/	{0xA34,	12, 0, 0},	/*	[21:12]	01	H	W	0xA94	*/
/* 8*/	{0xA14,	0, 0, 0},	/*	[9:0]	10	S	R	0xA74	*/
/* 9*/	{0xA20,	0, 0, 0},	/*	[9:0]	10	S	W	0xA80	*/
/*10*/	{0xA2C,	0, 0, 0},	/*	[9:0]	10	H	R	0xA8C	*/
/*11*/	{0xA38,	0, 0, 0},	/*	[9:0]	10	H	W	0xA98	*/
/*12*/	{0xA14,	12, 0, 0},	/*	[21:12]	11	S	R	0xA74	*/
/*13*/	{0xA20,	12, 0, 0},	/*	[21:12]	11	S	W	0xA80	*/
/*14*/	{0xA2C,	12, 0, 0},	/*	[21:12]	11	H	R	0xA8C	*/
/*15*/	{0xA38,	12, 0, 0},	/*	[21:12]	11	H	W	0xA98	*/
/*16*/	{0xA18,	0, 0, 0},	/*	[9:0]	SLB	S	R	0xA78	*/
/*17*/	{0xA24,	0, 0, 0},	/*	[9:0]	SLB	S	W	0xA84	*/
/*18*/	{0xA30,	0, 0, 0},	/*	[9:0]	SLB	H	R	0xA90	*/
/*19*/	{0xA3C,	0, 0, 0},	/*	[9:0]	SLB	H	W	0xA9C	*/
/*20*/	{0xA18,	12, 0, 0},	/*	[21:12]	SLB	S	R	0xA78	*/
/*21*/	{0xA24,	12, 0, 0},	/*	[21:12]	SLB	S	W	0xA84	*/
/*22*/	{0xA30,	12, 0, 0},	/*	[21:12]	SLB	H	R	0xA90	*/
/*23*/	{0xA3C,	12, 0, 0},	/*	[21:12]	SLB	H	W	0xA9C	*/
/*	AXI00	AXI01	AXI10	AXI11	*/
/*	0	1	21	20	*/
/*	37	36	34	35	*/
/*	2	32	3	33	*/
};

static struct mtk_dpc_dt_usage mt6989_disp_dt_usage[DPC_DISP_DT_CNT] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_TE,		DT_1,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_TE,		DT_3,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_TE,		DT_5,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_TE,		DT_6,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_TE,		DT_7,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, follow DISP1 by default, or HRT BW */
	{8, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_TE,		DT_12,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_TE,		DT_13,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow DISP1 */
	{14, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow DISP1 by default, or HRT BW */
	{17, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{19, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow DISP1 by default, or HRT BW */
	{20, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow DISP1 by default, or HRT BW */
	{23, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	3000,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_TE,		16000,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6989_mml_dt_usage[DPC_MML_DT_CNT] = {
	/* MML1 */
	{32, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 0 */
	{33, DPC_SP_TE,		DT_1,	DPC_MML_VIDLE_MTCMOS},		/* ON Time */
	{34, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MTCMOS},		/* MML-TE */
	{35, DPC_SP_TE,		DT_3,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* VDISP DVFS, follow MML1 by default, or HRT BW */
	{36, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_VDISP_DVFS},
	{37, DPC_SP_TE,		40329,	DPC_MML_VIDLE_VDISP_DVFS},
	{38, DPC_SP_TE,		40329,	DPC_MML_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{39, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 0 */
	{40, DPC_SP_TE,		DT_12,	DPC_MML_VIDLE_HRT_BW},		/* ON Time */
	{41, DPC_SP_TE,		DT_13,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow MML1 */
	{42, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_SRT_BW},
	{43, DPC_SP_TE,		40329,	DPC_MML_VIDLE_SRT_BW},
	{44, DPC_SP_TE,		40329,	DPC_MML_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow MML1 by default, or HRT BW */
	{45, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_MMINFRA_OFF},
	{46, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MMINFRA_OFF},
	{47, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow MML1 by default, or HRT BW */
	{48, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_INFRA_OFF},
	{49, DPC_SP_TE,		40329,	DPC_MML_VIDLE_INFRA_OFF},
	{50, DPC_SP_TE,		40329,	DPC_MML_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow MML1 by default, or HRT BW */
	{51, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_MAINPLL_OFF},
	{52, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MAINPLL_OFF},
	{53, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MAINPLL_OFF},

	/* RESERVED */
	{54, DPC_SP_RROT_DONE,	3000,	DPC_MML_VIDLE_RESERVED},
	{55, DPC_SP_TE,		16000,	DPC_MML_VIDLE_RESERVED},
	{56, DPC_SP_TE,		40329,	DPC_MML_VIDLE_RESERVED},
};

static inline int dpc_pm_ctrl(bool en)
{
	int ret = 0;

	if (!g_priv->pd_dev)
		return 0;

	if (en) {
		ret = pm_runtime_resume_and_get(g_priv->pd_dev);
		if (ret) {
			DPCERR("get failed ret(%d) vcp_is_alive(%u)", ret, g_priv->vcp_is_alive);
			return -1;
		}

		/* read dummy register to make sure it's ready to use */
		if (g_priv->mminfra_dummy && (readl(g_priv->mminfra_dummy) == 0))
			DPCAEE("read mminfra dummy failed");

		/* disable devapc power check false alarm, */
		/* DPC address is bound by power of disp1 on 6989 */
		if (g_priv->mminfra_hangfree)
			writel(readl(g_priv->mminfra_hangfree) & ~0x1, g_priv->mminfra_hangfree);
	} else
		pm_runtime_put_sync(g_priv->pd_dev);

	return ret;
}

static inline bool dpc_pm_check_and_get(void)
{
	if (!g_priv->pd_dev)
		return false;

	return pm_runtime_get_if_in_use(g_priv->pd_dev) > 0 ? true : false;
}

static void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en)
{
	u32 addr = 0;

	if (subsys >= DPC_SUBSYS_CNT)
		return;

	/* CLR : execute SW threads, disable auto MTCMOS */
	addr = en ? (g_priv->mtcmos_cfg[subsys].thread_clr + thread * 0x4)
		  : (g_priv->mtcmos_cfg[subsys].thread_set + thread * 0x4);
	writel(1, dpc_base + addr);
}

static int mtk_disp_wait_pwr_ack(const enum mtk_dpc_subsys subsys)
{
	int ret = 0;
	u32 value = 0;

	if (subsys >= DPC_SUBSYS_CNT)
		return -1;

	if (!g_priv->mtcmos_cfg[subsys].chk_va) {
		DPCERR("0x%pa not exist\n", &g_priv->mtcmos_cfg[subsys].chk_pa);
		return -1;
	}

	/* by subsys pm */
	// ret = readl_poll_timeout_atomic(g_priv->mtcmos_cfg[subsys].chk_va, value, 0xB, 1, 200);

	/* by dpc */
	ret = readl_poll_timeout_atomic(dpc_base + g_priv->mtcmos_cfg[subsys].cfg + 0x8,
					value, value & BIT(20), 1, 200);
	if (ret < 0)
		DPCERR("wait subsys(%d) power on timeout", subsys);

	return ret;
}

static void dpc_dt_set(u16 dt, u32 us)
{
	u32 value = us * 26;	/* 26M base, 20 bits range, 38.46 ns ~ 38.46 ms*/

	if ((dt > 56) && (g_priv->mmsys_id != MMSYS_MT6991))
		return;

	writel(value, dpc_base + DISP_REG_DPC_DTx_COUNTER(dt));
}

static void dpc2_dt_en(u16 idx, bool en, bool set_sw_trig)
{
	u32 val;
	u32 addr;
	u16 bit;

	if (g_priv->mmsys_id == MMSYS_MT6989 || g_priv->mmsys_id == MMSYS_MT6878) {
		if (idx < 32) {
			addr = DISP_REG_DPC_DISP_DT_EN;
			bit = idx;
		} else {
			addr = DISP_REG_DPC_MML_DT_EN;
			bit = idx - 32;
		}
	} else if (g_priv->mmsys_id == MMSYS_MT6991) {
		if (idx < 32) {
			addr = DISP_REG_DPC_DISP_DT_EN;
			bit = idx;
		} else if (idx < 57) {
			addr = DISP_REG_DPC_MML_DT_EN;
			bit = idx - 32;
		} else if (idx < 66) {
			addr = DISP_REG_DPC2_DISP_DT_EN;
			bit = idx - 57;
		} else if (idx < 75) {
			addr = DISP_REG_DPC2_MML_DT_EN;
			bit = idx - 66;
		} else if (idx < 78){
			addr = DISP_REG_DPC2_DISP_DT_EN;
			bit = idx - 66;
		} else {
			DPCERR("idx(%u) over then dt count", idx);
			return;
		}
	} else {
		DPCERR("not support platform");
		return;
	}

	val = readl(dpc_base + addr);
	if (en)
		val |= BIT(bit);
	else
		val &= ~BIT(bit);
	writel(val, dpc_base + addr);

	if (set_sw_trig) {
		if (idx < 57)
			addr += 0x4;
		else
			addr += 0x8;

		val = readl(dpc_base + addr);
		if (en)
			val |= BIT(bit);
		else
			val &= ~BIT(bit);
		writel(val, dpc_base + addr);
	}
}

static void dpc_dt_en_all(const enum mtk_dpc_subsys subsys, u32 dt_en)
{
	u32 cnt, idx;
	struct mtk_dpc_dt_usage *usage = NULL;

	if (subsys == DPC_SUBSYS_DISP) {
		writel(dt_en, dpc_base + DISP_REG_DPC_DISP_DT_EN);
		usage = &g_priv->disp_dt_usage[0];
	} else if (subsys == DPC_SUBSYS_MML) {
		writel(dt_en, dpc_base + DISP_REG_DPC_MML_DT_EN);
		usage = &g_priv->mml_dt_usage[0];
	}

	if (!usage) {
		DPCERR("%s:%d NULL Pointer\n", __func__, __LINE__);
		return;
	}

	cnt = __builtin_popcount(dt_en);
	while (cnt--) {
		idx = __builtin_ffs(dt_en) - 1;
		dt_en &= ~(1 << idx);
		dpc_dt_set(usage[idx].index, usage[idx].ep);
	}
}

static void dpc_dt_set_update(u16 dt, u32 us)
{
	if (g_priv->dpc2_dt_usage) {
		g_priv->dpc2_dt_usage[dt].val = us;
	} else {
		if (dt < DPC_DISP_DT_CNT)
			mt6989_disp_dt_usage[dt].ep = us;
		else if (dt < DPC_DISP_DT_CNT + DPC_MML_DT_CNT)
			mt6989_mml_dt_usage[dt - DPC_DISP_DT_CNT].ep = us;
	}

	dpc_dt_set(dt, us);
}

static void dpc_duration_update(const u32 us)
{
	u32 presz = DPC2_DT_PRESZ;

	if (debug_presz)
		presz = debug_presz;

	if (g_priv->mmsys_id == MMSYS_MT6991) {
		dpc_dt_set_update( 1, us - presz - DPC2_DT_MTCMOS);
		dpc_dt_set_update( 5, us - presz - DPC2_DT_MTCMOS);
		dpc_dt_set_update( 6, us - presz);
		dpc_dt_set_update( 9, us - presz - DPC2_DT_MMINFRA);
		dpc_dt_set_update(12, us - DPC2_DT_INFRA);
		dpc_dt_set_update(18, us - presz - DPC2_DT_MMINFRA);
		dpc_dt_set_update(33, us - presz - DPC2_DT_MTCMOS);
		dpc_dt_set_update(34, us - DPC2_DT_PRESZ + 20);
		dpc_dt_set_update(40, us - DPC2_DT_INFRA);
		dpc_dt_set_update(46, us - presz - DPC2_DT_MMINFRA);
		dpc_dt_set_update(64, us - presz - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE);
		dpc_dt_set_update(61, us - DPC2_DT_INFRA);
		dpc_dt_set_update(70, us - DPC2_DT_INFRA);
		dpc_dt_set_update(73, us - presz - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE);
		dpc_dt_set_update(76, us - presz - DPC2_DT_MTCMOS - DPC2_DT_DSION);

		/* wa for 90 hz extra dsi te */
		dpc_dt_set_update(7, us == 11111 ? 3000 : DPC2_DT_POSTSZ);
	} else {
		dpc_dt_set_update( 1, us - DT_OVL_OFFSET);
		dpc_dt_set_update( 5, us - DT_DISP1_OFFSET);
		dpc_dt_set_update( 6, us - DT_DISP1TE_OFFSET);
		dpc_dt_set_update(12, us - DT_MMINFRA_OFFSET);
		dpc_dt_set_update(33, us - DT_OVL_OFFSET);
		dpc_dt_set_update(40, us - DT_MMINFRA_OFFSET);
	}
}

static void dpc_ddr_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x000D000D : 0x00050005;

	if (subsys == DPC_SUBSYS_DISP)
		addr = DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG;
	else if (subsys == DPC_SUBSYS_MML)
		addr = DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG;

	writel(value, dpc_base + addr);
}

static void dpc_enable(const u8 en)
{
	u16 i = 0;

	if (en == 2)
		g_priv->vidle_mask = 0;

	if (en) {
		if (g_priv->mmsys_id == MMSYS_MT6991) {
			for (i = 0; i < DPC2_VIDLE_CNT; i ++) {
				if (g_priv->dpc2_dt_usage[i].en) {
					dpc2_dt_en(i, true, false);
					dpc_dt_set_update(i, g_priv->dpc2_dt_usage[i].val);
				}
			}

			if (debug_irq) {
				writel(BIT(31) | BIT(18) | BIT(9),
				       dpc_base + DISP_REG_DPC_DISP_INTEN);

				writel(BIT(13) | BIT(14) | BIT(17) | BIT(18) | BIT(31),
				       dpc_base + DISP_REG_DPC_MML_INTEN);
			}

			writel(0x40, dpc_base + DISP_DPC_ON2SOF_DT_EN);
			writel(0xf, dpc_base + DISP_DPC_ON2SOF_DSI0_SOF_COUNTER); /* should > 2T, cannot be zero */
			writel(0x2, dpc_base + DISP_REG_DPC_DEBUG_SEL);	/* mtcmos_debug */
			writel(0x7, dpc_base + DISP_REG_DPC_DUMMY0); /* criteria switch to DSC */
		} else {
			/* DT enable only 1, 3, 5, 6, 7, 12, 13, 29, 30, 31 */
			dpc_dt_en_all(DPC_SUBSYS_DISP, 0xe00030ea);

			/* DT enable only 1, 3, 8, 9 */
			dpc_dt_en_all(DPC_SUBSYS_MML, 0x30a);
		}

		/* wla ddren ack */
		writel(1, dpc_base + DISP_REG_DPC_DDREN_ACK_SEL);

		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);

		writel(0x1f, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);
		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);

		/* DISP_DPC_EN | DISP_DPC_DT_EN | DISP_DPC_MMQOS_ALWAYS_SCAN_EN */
		writel(0x13 | (en == 2 ? BIT(16) : 0), dpc_base + DISP_REG_DPC_EN);
		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
	} else {
		/* disable inten to avoid burst irq */
		writel(0, dpc_base + DISP_REG_DPC_DISP_INTEN);
		writel(0, dpc_base + DISP_REG_DPC_MML_INTEN);
		writel(0, dpc_base + DISP_REG_DPC_EN);

		/* reset dpc to clean counter start and value */
		writel(1, dpc_base + DISP_REG_DPC_RESET);
		writel(0, dpc_base + DISP_REG_DPC_RESET);

		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
	}

	/* enable gce event */
	writel(en, dpc_base + DISP_REG_DPC_EVENT_EN);

	g_priv->enabled = en;
}

static u8 bw_to_level(const u32 total_bw)
{
	if (total_bw > 6988)
		return 4;
	else if (total_bw > 5129)
		return 3;
	else if (total_bw > 4076)
		return 2;
	else if (total_bw > 3057)
		return 1;
	else
		return 0;
}

static u8 dpc_max_dvfs_level(void)
{
	/* find max(disp level, mml level, bw level) */
	u8 max_level = g_priv->dvfs_bw.disp_level > g_priv->dvfs_bw.bw_level?
		       g_priv->dvfs_bw.disp_level : g_priv->dvfs_bw.bw_level;

	return max_level > g_priv->dvfs_bw.mml_level ? max_level : g_priv->dvfs_bw.mml_level;
}

static void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 total_bw = bw_in_mb;

	if (unlikely(g_priv->total_hrt_unit == 0))
		return;

	/* U32_MAX means no need to update, just read */
	mutex_lock(&g_priv->dvfs_bw.lock);
	if (bw_in_mb != U32_MAX) {
		if (subsys == DPC_SUBSYS_DISP)
			g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML0)
			g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML1)
			g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT] = bw_in_mb;
	}
	total_bw = g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] +
		   g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] + g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT];
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (unlikely(debug_dvfs)) {
		if (bw_in_mb == U32_MAX)
			DPCFUNC("subsys(%u) updated(%u,%u,%u)", subsys,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT]);
		else
			DPCFUNC("subsys(%u) bw_in_mb(%u) trigger(%u) updated(%u,%u,%u)", subsys, bw_in_mb, force,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT]);
	}
	dpc_mmp(hrt_bw, MMPROFILE_FLAG_PULSE, subsys << 16 | force, bw_in_mb);

	if (!force)
		return;

	/* trigger dram dvfs first */
	writel(total_bw * 10000 / g_priv->hrt_emi_efficiency / g_priv->total_hrt_unit,
	       dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW);

	/* trigger vdisp dvfs */
	dpc_dvfs_set(DPC_SUBSYS_DISP, 0, false);
}

static void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 total_bw = bw_in_mb;

	if (g_priv->total_srt_unit == 0)
		return;

	/* U32_MAX means no need to update, just read */
	mutex_lock(&g_priv->dvfs_bw.lock);
	if (bw_in_mb != U32_MAX) {
		if (subsys == DPC_SUBSYS_DISP)
			g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML0)
			g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML1)
			g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT] = bw_in_mb;
	}
	total_bw = g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT] +
		   g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT] + g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT];
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (unlikely(debug_dvfs)) {
		if (bw_in_mb == U32_MAX)
			DPCFUNC("subsys(%u) updated(%u,%u,%u)", subsys,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT]);
		else
			DPCFUNC("subsys(%u) bw_in_mb(%u) trigger(%u) updated(%u,%u,%u)", subsys, bw_in_mb, force,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT]);
	}
	dpc_mmp(srt_bw, MMPROFILE_FLAG_PULSE, subsys << 16 | force, bw_in_mb);

	if (!force)
		return;

	writel(total_bw * g_priv->srt_emi_efficiency / 10000 / g_priv->total_srt_unit,
	       dpc_base + DISP_REG_DPC_DISP_SW_SRT_BW);
}

static int vdisp_level_set_vcp(const enum mtk_dpc_subsys subsys, const u8 level)
{
	int ret = 0;
	u32 value = 0;

	/* polling vdisp dvfsrc idle */
	if (g_priv->vdisp_dvfsrc) {
		ret = readl_poll_timeout(g_priv->vdisp_dvfsrc, value,
					 (value & g_priv->vdisp_dvfsrc_idle_mask) == 0, 1, 500);
		if (ret < 0)
			DPCERR("subsys(%d) wait vdisp dvfsrc idle timeout", subsys);
	}
	writel(level, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL);

	return ret;
}

static void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level, bool update_level)
{
	u32 mmdvfs_user = U32_MAX;
	u8 max_level;

	/* support 575, 600, 650, 700, 750 mV */
	if (update_level && (level > 4)) {
		DPCERR("vdisp support only 5 levels");
		return;
	}

	mutex_lock(&g_priv->dvfs_bw.lock);

	if (update_level) {
		if (subsys == DPC_SUBSYS_DISP)
			g_priv->dvfs_bw.disp_level = level;
		else
			g_priv->dvfs_bw.mml_level = level;
	}

	max_level = dpc_max_dvfs_level();
	if (max_level < level)
		max_level = level;
	vdisp_level_set_vcp(subsys, max_level);

	mutex_unlock(&g_priv->dvfs_bw.lock);

	/* add vdisp info to met */
	if (MEM_BASE) {
		mmdvfs_user = (subsys == DPC_SUBSYS_DISP) ? MMDVFS_USER_DISP : MMDVFS_USER_MML;
		writel(4 - max_level, MEM_USR_OPP(mmdvfs_user, false));
	}

	dpc_mmp(vdisp_level, MMPROFILE_FLAG_PULSE,
		g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] << 16 |
		g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] + g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT],
		((unsigned long)g_priv->dvfs_bw.disp_level) << 24 |
		((unsigned long)g_priv->dvfs_bw.mml_level) << 16 |
		((unsigned long)g_priv->dvfs_bw.bw_level) << 8 |
		(unsigned long)max_level);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) level(%u,%u,%u)", subsys,
			g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level, g_priv->dvfs_bw.bw_level);
}

static void dpc_ch_bw_set(const enum mtk_dpc_subsys subsys, const u8 idx, const u32 bw_in_mb)
{
	u32 value = 0;
	u32 ch_bw = bw_in_mb;

	if (g_priv->mmsys_id != MMSYS_MT6991)
		return;

	if (unlikely(mminfra_floor && bw_in_mb && (bw_in_mb < mminfra_floor * 16)))
		ch_bw = mminfra_floor * 16;

	if (idx < 24) {
	/* use display voter for both display and mml, since mml voter is reserved for others */
		value = readl(dpc_base + mt6991_ch_bw_cfg[idx].offset) & ~(0x3ff << mt6991_ch_bw_cfg[idx].shift);
		value |= (ch_bw * 100 / g_priv->ch_bw_urate / 16) << mt6991_ch_bw_cfg[idx].shift;

		if (unlikely(debug_dvfs))
			DPCFUNC("subsys(%u) idx(%u) bw(%u)MB", subsys, idx, ch_bw);

		writel(value, dpc_base + mt6991_ch_bw_cfg[idx].offset);
		dpc_mmp(ch_bw, MMPROFILE_FLAG_PULSE, idx, ch_bw);
	} else {
		DPCERR("idx %u > 24", idx);
		return;
	}
}

static void dpc_channel_bw_set_by_idx(const enum mtk_dpc_subsys subsys, const u8 idx, const u32 bw_in_mb)
{
	u32 ch_bw = bw_in_mb;
	u32 cur_ch_bw = 0;
	u32 max_ch_bw = 0;
	int i = 0;

	mutex_lock(&g_priv->dvfs_bw.lock);
	cur_ch_bw = mt6991_ch_bw_cfg[idx].disp_bw + mt6991_ch_bw_cfg[idx].mml_bw;

	if (subsys == DPC_SUBSYS_DISP)
		mt6991_ch_bw_cfg[idx].disp_bw = bw_in_mb;
	else
		mt6991_ch_bw_cfg[idx].mml_bw = bw_in_mb;

	ch_bw = mt6991_ch_bw_cfg[idx].disp_bw + mt6991_ch_bw_cfg[idx].mml_bw;
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (ch_bw == cur_ch_bw)
		return;

	dpc_mmp(ch_bw, MMPROFILE_FLAG_PULSE, BIT(subsys) << 16 | idx, bw_in_mb << 16 | ch_bw);
	dpc_ch_bw_set(subsys, idx, ch_bw);

	mutex_lock(&g_priv->dvfs_bw.lock);
	for (i = 0; i < 24; i++) {
		ch_bw = mt6991_ch_bw_cfg[i].disp_bw + mt6991_ch_bw_cfg[i].mml_bw;
		if (ch_bw > max_ch_bw)
			max_ch_bw = ch_bw;
	}
	g_priv->dvfs_bw.bw_level = bw_to_level(max_ch_bw);
	mutex_unlock(&g_priv->dvfs_bw.lock);
}

static void dpc_dvfs_trigger(const char *caller)
{
	dpc_hrt_bw_set(DPC_SUBSYS_MML, U32_MAX, true);
	dpc_srt_bw_set(DPC_SUBSYS_MML, U32_MAX, true);

	if (unlikely(debug_dvfs))
		DPCFUNC("by %s", caller);
}

static void mt6991_set_mtcmos(const enum mtk_dpc_subsys subsys, const enum mtk_dpc_mtcmos_mode mode)
{
	bool en = (mode == DPC_MTCMOS_AUTO ? true : false);
	int ret = 0;
	u32 value = 0;
	u32 rtff_mask = 0;
	u8 power_on = dpc_is_power_on() | mminfra_is_power_on() << 1;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}
	if (subsys >= DPC_SUBSYS_CNT) {
		DPCERR("not support subsys(%u)", subsys);
		return;
	}
	if (power_on != 0b11) {
		static bool called;

		mtk_dprec_logger_pr(DPREC_LOGGER_ERROR, "subsys(%u) en(%u) skipped due to no power(%#x)\n",
				    subsys, en, power_on);
		if (!called) {
			called = true;
			DPCERR("subsys(%u) en(%u) skipped due to no power(%#x)",
				    subsys, en, power_on);
			dump_stack();
		}
		return;
	}
	dpc_mmp(mtcmos_auto, MMPROFILE_FLAG_PULSE, subsys, en);

	if (subsys == DPC_SUBSYS_DISP)
		rtff_mask = 0xf00;
	else if (subsys == DPC_SUBSYS_MML1)
		rtff_mask = BIT(15);
	else if (subsys == DPC_SUBSYS_MML0)
		rtff_mask = BIT(14);

	/* [SWITCH TO DPC AUTO MODE]
	 *   1. unset bootup (enable rtff)
	 *   2. enable AUTO_ONOFF_MASTER_EN
	 * [SWITCH TO MANUAL MODE]
	 *   1. vote thread
	 *   2. wait until mtcmos is ON_ACT state
	 *   3. disable AUTO_ONOFF_MASTER_EN
	 *   4. set bootup (disable rtff)
	 *   5. unvote thread
	 */
	if (mode == DPC_MTCMOS_AUTO) {
		if (g_priv->rtff_pwr_con && has_cap(DPC_CAP_MTCMOS))
			writel(readl(g_priv->rtff_pwr_con) & ~rtff_mask, g_priv->rtff_pwr_con);
	} else {
		dpc_mtcmos_vote(subsys, 5, true);

		if (g_priv->mtcmos_cfg[subsys].mode == DPC_MTCMOS_AUTO) {
			/* MTCMOS_STA [20]ON_ACT [21]OFF_IDLE [22]RUNNING */
			ret = readl_poll_timeout_atomic(dpc_base + g_priv->mtcmos_cfg[subsys].cfg + 0x8,
							value, value & BIT(20), 1, 200);
			if (ret < 0)
				DPCERR("wait subsys(%d) ON_ACT state timeout", subsys);
		}
	}

	value = (en && has_cap(DPC_CAP_MTCMOS)) ? 0x31 : 0x70;
	if (subsys == DPC_SUBSYS_DISP) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].cfg);
	} else if (subsys == DPC_SUBSYS_MML1) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].cfg);
	} else if (subsys == DPC_SUBSYS_MML0) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML0].cfg);
	}
	g_priv->mtcmos_cfg[subsys].mode = mode;

	if (!en) {
		if (g_priv->rtff_pwr_con && has_cap(DPC_CAP_MTCMOS))
			writel(readl(g_priv->rtff_pwr_con) | rtff_mask, g_priv->rtff_pwr_con);
		dpc_mtcmos_vote(subsys, 5, false);
	}

	dpc_mmp(mtcmos_auto, MMPROFILE_FLAG_PULSE, subsys, readl(g_priv->rtff_pwr_con));
}

static void mt6989_set_mtcmos(const enum mtk_dpc_subsys subsys, const enum mtk_dpc_mtcmos_mode mode)
{
	bool en = (mode == DPC_MTCMOS_AUTO ? true : false);
	u32 value = (en && has_cap(DPC_CAP_MTCMOS)) ? 0x11 : 0;

	if (!dpc_is_power_on()) {
		DPCFUNC("disp vcore is not power on, subsys(%u) en(%u) skip", subsys, en);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	/* MTCMOS auto_on_off[0] both_ack[4] pwr_off_dependency[6] */
	if (subsys == DPC_SUBSYS_DISP) {
		writel(value | BIT(6), dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].cfg);
	} else if (subsys == DPC_SUBSYS_MML1) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].cfg);
	} else {
		dpc_pm_ctrl(false);
		return;
	}

	if (en) {
		/* pwr on delay default 100 + 50 us, modify to 30 us */
		writel(0x30c, dpc_base + 0xa44);
		writel(0x30c, dpc_base + 0xb44);
		writel(0x30c, dpc_base + 0xc44);
		writel(0x30c, dpc_base + 0xd44);
		writel(0x30c, dpc_base + 0xe44);

		// value = piece ? 0xa280821 : 0xa280820;
		// writel(value, dpc_base + DISP_REG_DPC_DISP0_MTCMOS_OFF_PROT_CFG);
		// writel(value, dpc_base + DISP_REG_DPC_OVL0_MTCMOS_OFF_PROT_CFG);
		// writel(value, dpc_base + DISP_REG_DPC_OVL1_MTCMOS_OFF_PROT_CFG);
		// writel(value, dpc_base + DISP_REG_DPC_DISP1_MTCMOS_OFF_PROT_CFG);
		// writel(value, dpc_base + DISP_REG_DPC_MML1_MTCMOS_OFF_PROT_CFG);
	}

	dpc_pm_ctrl(false);
}

void dpc_mtcmos_auto(const enum mtk_dpc_subsys subsys, const enum mtk_dpc_mtcmos_mode mode)
{
	unsigned long flags;

	if (!g_priv->set_mtcmos)
		return;

	spin_lock_irqsave(&g_priv->mtcmos_cfg_lock, flags);
	g_priv->set_mtcmos(subsys, mode);
	spin_unlock_irqrestore(&g_priv->mtcmos_cfg_lock, flags);
}

static void dpc_dsi_pll_set(const u32 value)
{
	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}

	/* if DSI_PLL_SEL is set, power ON disp1 and set DSI_CK_KEEP_EN */
	if (g_priv->dsi_ck_keep_mask && (value & BIT(0))) {
		mtk_disp_vlp_vote(VOTE_SET, DISP_VIDLE_USER_DISP_DPC_CFG);
		/* will be cleared when ff enable */

		mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS1);
		writel(g_priv->dsi_ck_keep_mask, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg);
	}
}

static void dpc_disp_group_enable(bool en)
{
	int ret = 0;
	u32 value = 0;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}

	/* DDR_SRC and EMI_REQ DT is follow DISP1 */
	value = (en && has_cap(DPC_CAP_APSRC)) ? 0x00010001 : 0x000D000D;
	writel(value, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);

	/* channel bw DT is follow SRT*/
	value = (en && has_cap(DPC_CAP_QOS)) ? 0 : 0x00010001;
	writel(value, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);

	/* lower vdisp level */
	value = (en && has_cap(DPC_CAP_VDISP)) ? 0 : 1;
	writel(value, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);

	/* mminfra request */
	value = (en && has_cap(DPC_CAP_MMINFRA_PLL)) ? 0 : 0x181818;
	writel(value, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);

	if (g_priv->mmsys_id == MMSYS_MT6991) {
		/* check mminfra voter bit and polling power on */
		if (!en) {
			ret = readl_poll_timeout_atomic(g_priv->mminfra_voter,
							value, value & BIT(6), 1, 100);
			if (ret < 0)
				DPCERR("vote mminfra voter timeout");

			ret = readl_poll_timeout_atomic(g_priv->mminfra_chk, value,
							value & g_priv->mminfra_chk_mask, 10, 700);
			if (ret < 0)
				DPCERR("wait mminfra power timeout");
		}

		/* dsi pll auto */
		value = (en && has_cap(DPC_CAP_DSI)) ? 0x11 : 0x1;
		writel(value, dpc_base + DISP_DPC_MIPI_SODI5_EN);

		/* vcore off */
		value = (en && has_cap(DPC_CAP_PMIC_VCORE)) ? 0x21 : 0x60;
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_EDP].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DPTX].cfg);
		value = (en && has_cap(DPC_CAP_PMIC_VCORE)) ? 0x180000 : 0x181e1e;
		writel(value, dpc_base + DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG);
	}
}

static void dpc_mml_group_enable(bool en)
{
	u32 value = 0;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}

	/* DDR_SRC and EMI_REQ DT is follow MML1 */
	value = (en && has_cap(DPC_CAP_APSRC)) ? 0x00010001 : 0x000D000D;
	writel(value, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);

	/* lower vdisp level */
	value = (en && has_cap(DPC_CAP_VDISP)) ? 0 : 1;
	writel(value, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* channel bw DT is follow SRT*/
	value = (en && has_cap(DPC_CAP_QOS)) ? 0 : 0x00010001;
	writel(value, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

	/* mminfra request */
	value = (en && has_cap(DPC_CAP_MMINFRA_PLL)) ? 0 : 0x181818;
	writel(value, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	if (g_priv->mmsys_id == MMSYS_MT6991) {
		/* vcore off */
		value = (en && has_cap(DPC_CAP_PMIC_VCORE)) ? 0x180000 : 0x181e1e;
		writel(value, dpc_base + DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG);
	}
}

void dpc_group_enable(const u16 group, bool en)
{
	if (group == DPC_SUBSYS_DISP)
		dpc_disp_group_enable(en);
	else {
		dpc_mml_group_enable(en);
		dpc_mtcmos_auto(group, (enum mtk_dpc_mtcmos_mode)en);
	}
}

static int dpc_config(const enum mtk_dpc_subsys subsys, bool en)
{
	static bool is_mminfra_ctrl_by_dpc;

	/* vote power and wait mtcmos on before switch to hw mode */
	mtk_disp_vlp_vote(VOTE_SET, DISP_VIDLE_USER_DISP_DPC_CFG);
	mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS1);
	mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS0);
	mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL0);

	if (!en && is_mminfra_ctrl_by_dpc) {
		mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc get mminfra\n");
		if (dpc_pm_ctrl(true))
			return -EFAULT;
		is_mminfra_ctrl_by_dpc = false;
	}

	writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
	writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_MML_MASK_CFG);

	/* set resource auto or manual mode */
	dpc_disp_group_enable(en);

	/* set mtcmos auto or manual mode */
	dpc_mtcmos_auto(DPC_SUBSYS_DISP, (enum mtk_dpc_mtcmos_mode)en);

	if (en && has_cap(DPC_CAP_MMINFRA_PLL) && !is_mminfra_ctrl_by_dpc) {
		dpc_pm_ctrl(false);
		is_mminfra_ctrl_by_dpc = true;
		mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc put mminfra\n");
	}

	/* will be unvoted by atomic commit gce pkt, for suspend resuming protection */
	/* mtk_disp_vlp_vote(VOTE_CLR, DISP_VIDLE_USER_DISP_DPC_CFG); */

	dpc_mmp(config, MMPROFILE_FLAG_PULSE, subsys, en);
	return 0;
}

irqreturn_t mt6991_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 disp_sta, mml_sta;
	irqreturn_t ret = IRQ_NONE;
	static DEFINE_RATELIMIT_STATE(err_rate, HZ, 1);
	static bool mml_sof_has_begin;
	static bool mml1_has_begin;

	if (IS_ERR_OR_NULL(priv))
		return ret;

	if (!debug_irq) /* avoid irq from being triggered unexpectedly */
		return IRQ_HANDLED;

	dpc_mmp(mminfra, MMPROFILE_FLAG_START, 0x77777777, 1);
	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_END, U32_MAX, U32_MAX);
		return IRQ_NONE;
	}
	dpc_mmp(mminfra, MMPROFILE_FLAG_PULSE, 0x77777777, 2);

	disp_sta = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	mml_sta =  readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
	if ((!disp_sta) && (!mml_sta)) {
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE,
			readl(priv->vdisp_ao_cg_con), readl(priv->mminfra_chk));

		if (__ratelimit(&err_rate)) {
			if (unlikely(irq_aee)) {
				DPCAEE("irq err clksq(%u) ulposc(%u) vdisp_ao_cg(%#x) dpc_merge(%#x, %#x)",
				mt_get_fmeter_freq(47, VLPCK), /* clksq */
				mt_get_fmeter_freq(59, VLPCK), /* ulposc */
				readl(priv->vdisp_ao_cg_con),
				readl(dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG),
				readl(dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG));
			} else {
				DPCERR("irq err clksq(%u) ulposc(%u) vdisp_ao_cg(%#x) dpc_merge(%#x, %#x)",
				mt_get_fmeter_freq(47, VLPCK), /* clksq */
				mt_get_fmeter_freq(59, VLPCK), /* ulposc */
				readl(priv->vdisp_ao_cg_con),
				readl(dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG),
				readl(dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG));
			}
		}

		goto out;
	}

	if (disp_sta)
		writel(~disp_sta, dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (mml_sta)
		writel(~mml_sta, dpc_base + DISP_REG_DPC_MML_INTSTA);

	if (disp_sta & BIT(31) || mml_sta & BIT(31)) {
		u32 disp_err_sta = readl(dpc_base + 0x87c);
		u32 mml_err_sta = readl(dpc_base + 0x880);
		u32 debug_sta = readl(dpc_base + DISP_REG_DPC_DEBUG_STA);

		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, readl(priv->vdisp_ao_cg_con), debug_sta);
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, disp_err_sta, mml_err_sta);

		if (__ratelimit(&err_rate)) {
			if (unlikely(irq_aee))
				DPCAEE("irq err disp(%#x) mml(%#x) debug(%#x)",
					disp_err_sta, mml_err_sta, debug_sta);
			else
				DPCERR("irq err disp(%#x) mml(%#x) debug(%#x)",
					disp_err_sta, mml_err_sta, debug_sta);
		}
	}

	/* MML1 */
	if (mml_sta & BIT(17) && !mml_sof_has_begin) {
		dpc_mmp(mml_sof, MMPROFILE_FLAG_START, 0, 0);
		mml_sof_has_begin = true;
	}
	if (mml_sta & BIT(18) && mml_sof_has_begin) {
		dpc_mmp(mml_sof, MMPROFILE_FLAG_END, 0, 0);
		mml_sof_has_begin = false;
	}
	if (mml_sta & BIT(14) && !mml1_has_begin) {
		dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, 0);
		tracing_mark_write(trace_buf_mml_on);
		mml1_has_begin = true;
	}
	if (mml_sta & BIT(13) && mml1_has_begin) {
		dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, 0);
		tracing_mark_write(trace_buf_mml_off);
		mml1_has_begin = false;
	}

	/* Panel TE */
	if (disp_sta & BIT(18))
		dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, 0);
	if (disp_sta & BIT(9)) {
		u32 presz = DPC2_DT_PRESZ;

		if (debug_presz)
			presz = debug_presz;
		dpc_mmp(prete, MMPROFILE_FLAG_PULSE, presz, priv->dpc2_dt_usage[6].val);
	}

	ret = IRQ_HANDLED;
out:
	dpc_pm_ctrl(false);
	dpc_mmp(mminfra, MMPROFILE_FLAG_END, 0x77777777, 3);
	return ret;
}

irqreturn_t mt6989_disp_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return IRQ_NONE;
	}

	status = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (!status) {
		dpc_pm_ctrl(false);
		return IRQ_NONE;
	}

	writel(~status, dpc_base + DISP_REG_DPC_DISP_INTSTA);

	if (likely(debug_mmp)) {
		if (status & DISP_DPC_INT_DT6)
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, 0);

		if (status & DISP_DPC_INT_DT3)
			dpc_mmp(idle_off, MMPROFILE_FLAG_PULSE, 0, 3);

		if (status & DISP_DPC_INT_MMINFRA_OFF_END)
			dpc_mmp(mminfra, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_MMINFRA_OFF_START)
			dpc_mmp(mminfra, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_DT1)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & DISP_DPC_INT_OVL0_ON)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_OVL0_OFF)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_DT5)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & DISP_DPC_INT_DISP1_ON)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_DISP1_OFF)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_END, 0, 0);
	}

	dpc_pm_ctrl(false);

	return IRQ_HANDLED;
}

irqreturn_t mt6989_mml_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return IRQ_NONE;
	}

	status = readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
	if (!status) {
		dpc_pm_ctrl(false);
		return IRQ_NONE;
	}

	writel(~status, dpc_base + DISP_REG_DPC_MML_INTSTA);

	if (likely(debug_mmp)) {
		if (status & BIT(16))
			dpc_mmp(mml_sof, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & BIT(17))
			dpc_mmp(mml_rrot_done, MMPROFILE_FLAG_PULSE, 0, 0);

		if (status & BIT(13))
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, 0);
		if (status & BIT(12))
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, 0);
	}

	dpc_pm_ctrl(false);

	return IRQ_HANDLED;
}

static void get_addr_byname(const char *name, void __iomem **va, resource_size_t *pa)
{
	void __iomem *_va;
	struct resource *res;

	res = platform_get_resource_byname(g_priv->pdev, IORESOURCE_MEM, name);
	if (res) {
		_va = ioremap(res->start, resource_size(res));
		if (!IS_ERR_OR_NULL(_va)) {
			*va = _va;
			if (pa)
				*pa = res->start;

			DPCDUMP("mapping %s %pa done", name, &res->start);
		} else
			DPCERR("failed to map %s %pR", name, res);
	} else
		DPCERR("failed to map %s", name);
}

static int dpc_res_init(struct mtk_dpc *priv)
{
	get_addr_byname("DPC_BASE", &dpc_base, &priv->dpc_pa);
	get_addr_byname("rtff_pwr_con", &priv->rtff_pwr_con, NULL);
	get_addr_byname("disp_sw_vote_set", &priv->voter_set_va, &priv->voter_set_pa);
	get_addr_byname("disp_sw_vote_clr", &priv->voter_clr_va, &priv->voter_clr_pa);
	get_addr_byname("vcore_mode_set", &priv->vcore_mode_set_va, NULL);
	get_addr_byname("vcore_mode_clr", &priv->vcore_mode_clr_va, NULL);
	get_addr_byname("vdisp_dvfsrc", &priv->vdisp_dvfsrc, NULL);
	get_addr_byname("disp_vcore_pwr_chk", &priv->dispvcore_chk, NULL);
	get_addr_byname("mminfra_pwr_chk", &priv->mminfra_chk, NULL);
	get_addr_byname("mminfra_voter", &priv->mminfra_voter, NULL);
	get_addr_byname("mminfra_dummy", &priv->mminfra_dummy, NULL);
	get_addr_byname("dis0_pwr_chk",
			&priv->mtcmos_cfg[DPC_SUBSYS_DIS0].chk_va,
			&priv->mtcmos_cfg[DPC_SUBSYS_DIS0].chk_pa);
	get_addr_byname("dis1_pwr_chk",
			&priv->mtcmos_cfg[DPC_SUBSYS_DIS1].chk_va,
			&priv->mtcmos_cfg[DPC_SUBSYS_DIS1].chk_pa);
	get_addr_byname("ovl0_pwr_chk",
			&priv->mtcmos_cfg[DPC_SUBSYS_OVL0].chk_va,
			&priv->mtcmos_cfg[DPC_SUBSYS_OVL0].chk_pa);
	get_addr_byname("ovl1_pwr_chk",
			&priv->mtcmos_cfg[DPC_SUBSYS_OVL1].chk_va,
			&priv->mtcmos_cfg[DPC_SUBSYS_OVL1].chk_pa);
	get_addr_byname("mml1_pwr_chk",
			&priv->mtcmos_cfg[DPC_SUBSYS_MML1].chk_va,
			&priv->mtcmos_cfg[DPC_SUBSYS_MML1].chk_pa);
	get_addr_byname("mml0_pwr_chk",
			&priv->mtcmos_cfg[DPC_SUBSYS_MML0].chk_va,
			&priv->mtcmos_cfg[DPC_SUBSYS_MML0].chk_pa);
	get_addr_byname("mminfra_hangfree", &priv->mminfra_hangfree, NULL);
	get_addr_byname("vdisp_ao_cg_con", &priv->vdisp_ao_cg_con, NULL);

	if (priv->mmsys_id == MMSYS_MT6991) {
		enum mtk_dpc_subsys subsys = 0;

		/* use for gced, modify for access mmup inside mminfra */
		priv->voter_set_pa -= 0x800000;
		priv->voter_clr_pa -= 0x800000;

		/* power check by dpc, instead of subsys_pm */
		for (subsys = 0; subsys < DPC_SUBSYS_CNT; subsys++)
			priv->mtcmos_cfg[subsys].chk_pa = priv->dpc_pa + priv->mtcmos_cfg[subsys].cfg + 0x8;
	}

	return IS_ERR_OR_NULL(dpc_base);
}

static int dpc_irq_init(struct mtk_dpc *priv)
{
	int ret = 0;
	int num_irqs;

	num_irqs = platform_irq_count(priv->pdev);
	if (num_irqs <= 0) {
		DPCERR("unable to count IRQs");
		return -EPROBE_DEFER;
	} else if (num_irqs == 1) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
	} else if (num_irqs == 2) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
		priv->mml_irq = platform_get_irq(priv->pdev, 1);
	}

	if (priv->disp_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->disp_irq, priv->disp_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->disp_irq, ret);
	}
	if (priv->mml_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->mml_irq, priv->mml_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->mml_irq, ret);
	}
	DPCFUNC("disp irq %d, mml irq %d, ret %d", priv->disp_irq, priv->mml_irq, ret);

	/* disable merge irq */
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);

	return ret;
}

static void mtk_disp_vlp_vote(unsigned int vote_set, unsigned int thread)
{
	void __iomem *voter_va = vote_set ? g_priv->voter_set_va : g_priv->voter_clr_va;
	u32 ack = vote_set ? BIT(thread) : 0;
	u32 val = 0;
	u16 i = 0;
	static atomic_t has_begin = ATOMIC_INIT(0);

	if (!voter_va)
		return;

	writel_relaxed(BIT(thread), voter_va);
	do {
		writel_relaxed(BIT(thread), voter_va);
		val = readl(voter_va);
		if ((val & BIT(thread)) == ack)
			break;

		if (i > 2500) {
			DPCERR("%s by thread(%u) timeout, vcp(%u) mminfra(%u)",
				vote_set ? "set" : "clr", thread,
				g_priv->vcp_is_alive, mminfra_is_power_on());
			return;
		}

		udelay(2);
		i++;
	} while (1);

	/* check voter only, later will use another API to power on mminfra */

	dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, BIT(thread) | vote_set, val);

	if (val != 0 && atomic_read(&has_begin) == 0) {
		atomic_set(&has_begin, 1);
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_START, 0, val);
	} else if (val == 0 && atomic_read(&has_begin) == 1) {
		atomic_set(&has_begin, 0);
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_END, 0, 0);
	}
}

static int dpc_vidle_power_keep(const enum mtk_vidle_voter_user _user)
{
	int ret = VOTER_PM_DONE;
	enum mtk_vidle_voter_user user = _user & DISP_VIDLE_USER_MASK;

	if (!g_priv->vcp_is_alive) {
		DPCFUNC("by user(%#x) skipped", _user);
		return VOTER_PM_FAILED;
	}

	if (_user & VOTER_ONLY) {
		mtk_disp_vlp_vote(VOTE_SET, user);
		return VOTER_ONLY;
	} else if (user == DISP_VIDLE_USER_TOP_CLK_ISR) {
		/* skip pm_get to fix unstable DSI TE, mminfra power is held by DPC usually */
		/* but if no power at this time, the user should call pm_get to ensure power */
		mtk_disp_vlp_vote(VOTE_SET, user);
		return mminfra_is_power_on() ? VOTER_ONLY : VOTER_PM_LATER;
	}

	if (dpc_pm_ctrl(true))
		return VOTER_PM_FAILED;

	mtk_disp_vlp_vote(VOTE_SET, user);

	if (!g_priv->enabled || user < DISP_VIDLE_USER_CRTC)
		return ret;

	switch (user) {
	case DISP_VIDLE_USER_MML1:
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
		break;
	case DISP_VIDLE_USER_MML0:
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML0);
		break;
	case DISP_VIDLE_USER_PQ:
		if (g_priv->root_dev) {
			/* can only be used by USER_PQ, as it will not be used within ISR */
			pm_runtime_get_sync(g_priv->root_dev);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS0);
			pm_runtime_put_sync(g_priv->root_dev);
		} else
			udelay(post_vlp_delay);
		break;
	case DISP_VIDLE_USER_CRTC:
	case DISP_VIDLE_USER_DISP_DPC_CFG:
	case DISP_VIDLE_USER_DPC_DUMP:
	case DISP_VIDLE_USER_SMI_DUMP:
	case DISP_VIDLE_FORCE_KEEP:
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS1);
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_DIS0);
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL0);
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL1);
		break;
	default:
		udelay(post_vlp_delay);
	}

	return ret;
}

static void dpc_vidle_power_release(const enum mtk_vidle_voter_user user)
{
	if (!g_priv->vcp_is_alive) {
		DPCFUNC("by user(%u) skipped", user & DISP_VIDLE_USER_MASK);
		return;
	}

	mtk_disp_vlp_vote(VOTE_CLR, user & DISP_VIDLE_USER_MASK);

	if ((user & VOTER_ONLY) || ((user & DISP_VIDLE_USER_MASK) == DISP_VIDLE_USER_TOP_CLK_ISR))
		return;

	dpc_pm_ctrl(false);
}

static void dpc_clear_wfe_event(struct cmdq_pkt *pkt, enum mtk_vidle_voter_user user, int event)
{
	if (!has_cap(DPC_CAP_MTCMOS) && !debug_force_wait)
		return;

	cmdq_pkt_clear_event(pkt, event);
	cmdq_pkt_wfe(pkt, event);
}

static void dpc_vidle_power_keep_by_gce(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user,
					const u16 gpr, struct cmdq_poll_reuse *reuse)
{
	cmdq_pkt_write(pkt, NULL, g_priv->voter_set_pa, BIT(user), U32_MAX);

	switch (user) {
	case DISP_VIDLE_USER_DISP_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].chk_pa, BIT(20));
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].chk_pa, BIT(20));
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].chk_pa, BIT(20));
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].chk_pa, BIT(20));
		break;
	case DISP_VIDLE_USER_DDIC_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].chk_pa, BIT(20));
		break;
	case DISP_VIDLE_USER_MML1_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].chk_pa, BIT(20));
		break;
	case DISP_VIDLE_USER_MML0_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_MML0].chk_pa, BIT(20));
		break;
	default:
		DPCERR("not support user %u", user);
		return;
	}
}

static void dpc_vidle_power_release_by_gce(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user)
{
	cmdq_pkt_write(pkt, NULL, g_priv->voter_clr_pa, BIT(user), U32_MAX);
}

static bool dpc_is_power_on(void)
{
	if (!g_priv->dispvcore_chk)
		return true;

	return readl(g_priv->dispvcore_chk) & g_priv->dispvcore_chk_mask;
}

static bool mminfra_is_power_on(void)
{
	if (!g_priv->mminfra_chk)
		return true;

	/* subsys pm 0x31ac03dc bit1 */
	/* VLP_AO RSVD6(0x918) MM1_DONE bit0 */
	return readl(g_priv->mminfra_chk) & g_priv->mminfra_chk_mask;
}

static void dpc_analysis(void)
{
	char msg[512] = {0};
	int written = 0;
	struct timespec64 ts = {0};

	if (!dpc_is_power_on()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	ktime_get_ts64(&ts);
	written = scnprintf(msg, 512, "[%lu.%06lu]:", (unsigned long)ts.tv_sec,
		(unsigned long)DO_COMMMON_MOD(DO_COMMON_DIV(ts.tv_nsec, NSEC_PER_USEC), 1000000));

	written += scnprintf(msg + written, 512 - written,
		"vidle(%#x) dpc_en(%#x) voter(%#x) mtcmos(%#x %#x %#x %#x %#x %#x) ",
		g_priv->vidle_mask, readl(dpc_base), readl(g_priv->voter_set_va),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML0].cfg));

	written += scnprintf(msg + written, 512 - written,
		"vdisp[cfg val](%#04x %#04x)(%#04x %#04x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));

	written += scnprintf(msg + written, 512 - written,
		"hrt[cfg val](%#010x %#06x)(%#010x %#06x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW),
		readl(dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_SW_HRT_BW));

	written += scnprintf(msg + written, 512 - written,
		"[ddremi mminfra](%#010x %#08x)(%#010x %#08x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG));
	mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);

	if (g_priv->mmsys_id == MMSYS_MT6991) {
		int i;

		written = scnprintf(msg, 512,
			"ch[hrt srt](%#04x %#04x %#04x %#04x)(%#04x %#04x %#04x %#04x) dt",
			(readl(dpc_base + mt6991_ch_bw_cfg[2].offset)) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[6].offset) >> mt6991_ch_bw_cfg[6].shift) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[10].offset)) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[14].offset) >> mt6991_ch_bw_cfg[14].shift) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[0].offset)) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[4].offset) >> mt6991_ch_bw_cfg[4].shift) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[8].offset)) & 0xfff,
			(readl(dpc_base + mt6991_ch_bw_cfg[12].offset) >> mt6991_ch_bw_cfg[12].shift) & 0xfff);

		for (i = 0; i < DPC2_VIDLE_CNT; i ++)
			if (g_priv->dpc2_dt_usage[i].en)
				written += scnprintf(msg + written, 512 - written, "[%d]%u ",
					i, g_priv->dpc2_dt_usage[i].val);
		mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);
	}

	dpc_pm_ctrl(false);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void dpc_dump(void)
{
	u32 i = 0;

	if (!dpc_is_power_on()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	for (i = dump_begin; i < (dump_begin + dump_lines); i++) {
		DPCDUMP("[0x%03X] %08X %08X %08X %08X", i * 0x10,
			readl(dpc_base + i * 0x10 + 0x0),
			readl(dpc_base + i * 0x10 + 0x4),
			readl(dpc_base + i * 0x10 + 0x8),
			readl(dpc_base + i * 0x10 + 0xc));
	}
	dpc_pm_ctrl(false);
}
#endif

static int dpc_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (g_priv->pd_dev) {
			u32 force_release = 0;

			while (atomic_read(&g_priv->pd_dev->power.usage_count) > 0) {
				force_release++;
				pm_runtime_put_sync(g_priv->pd_dev);
			}

			if (unlikely(force_release))
				DPCFUNC("dpc_dev dpc_pm unbalanced(%u)", force_release);
		}
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
static int dpc_vcp_notifier(struct notifier_block *nb, unsigned long vcp_event, void *unused)
{
	mtk_vdisp_avs_vcp_notifier(vcp_event, unused);
	switch (vcp_event) {
	case VCP_EVENT_READY:
	case VCP_EVENT_STOP:
		break;
	case VCP_EVENT_SUSPEND:
		g_priv->vcp_is_alive = false;
		break;
	case VCP_EVENT_RESUME:
		g_priv->vcp_is_alive = true;
		break;
	}
	return NOTIFY_DONE;
}
#endif

static int dpc_smi_pwr_get(void *data)
{
	dpc_vidle_power_keep(DISP_VIDLE_USER_SMI_DUMP);
	g_priv->vidle_mask_bk = g_priv->vidle_mask;
	g_priv->vidle_mask = 0;
	return 0;
}
static int dpc_smi_pwr_put(void *data)
{
	g_priv->vidle_mask = g_priv->vidle_mask_bk;
	dpc_vidle_power_release(DISP_VIDLE_USER_SMI_DUMP);
	return 0;
}
static struct smi_user_pwr_ctrl dpc_smi_pwr_funcs = {
	 .name = "disp_dpc",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_DISP,
	 .smi_user_get = dpc_smi_pwr_get,
	 .smi_user_put = dpc_smi_pwr_put,
};

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void process_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 v1 = 0, v2 = 0, v3 = 0;
	u32 mminfra_hangfree_val = 0;

	if (strncmp(opt, "cap:", 4) == 0) {
		ret = sscanf(opt, "cap:0x%x\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring] cap sscanf not match");
			goto err;
		}
		DPCDUMP("cap(0x%x->0x%x)", g_priv->vidle_mask, v1);
		g_priv->vidle_mask = v1;
	} else if (strncmp(opt, "avs:", 4) == 0) {
		if (mtk_vdisp_avs_dbg_opt(opt))
			goto err;
	} else if (strncmp(opt, "vote:", 5) == 0) {
		ret = sscanf(opt, "vote:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]vote sscanf not match");
			goto err;
		}
		if (v1 == 1)
			writel(0xffffffff, g_priv->voter_clr_va);
		else
			writel(0xffffffff, g_priv->voter_set_va);
	}

	if (!dpc_is_power_on()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	if (g_priv->mminfra_hangfree) {
		/* disable devapc power check temporarily, the value usually not changed after boot */
		mminfra_hangfree_val = readl(g_priv->mminfra_hangfree);
		writel(mminfra_hangfree_val & ~0x1, g_priv->mminfra_hangfree);
	}

	if (strncmp(opt, "wr:", 3) == 0) {
		ret = sscanf(opt, "wr:0x%x=0x%x\n", &v1, &v2);
		if (ret != 2)
			goto err;
		DPCFUNC("(%pK)=(%x)", dpc_base + v1, v2);
		writel(v2, dpc_base + v1);
	} else if (strncmp(opt, "channel:", 8) == 0) {
		ret = sscanf(opt, "channel:%u,%u,%u\n", &v1, &v2, &v3);
		if (ret != 3) {
			DPCDUMP("channel:0,2,1000 => ch(2) bw(1000)MB by subsys(0)");
			goto err;
		}
		dpc_ch_bw_set(v1, v2, v3);
		DPCDUMP("dpc_ch_bw_set subsys(%u) idx(%u) bw(%u)", v1, v2, v3);
	} else if (strncmp(opt, "vdisp:", 6) == 0) {
		ret = sscanf(opt, "vdisp:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("vdisp:0,4 => level(4) by subsys(0)");
			goto err;
		}
		dpc_dvfs_set(v1, v2, true);

	} else if (strncmp(opt, "dt:", 3) == 0) {
		ret = sscanf(opt, "dt:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("dt:2,1000 => set dt(4) counter as 1000us");
			goto err;
		}
		dpc_dt_set_update((u16)v1, v2);
	}  else if (strncmp(opt, "presz:", 6) == 0) {
		ret = sscanf(opt, "presz:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("presz:500,8333 => update dt(8333) by presz(500), unset by presz(0)");
			goto err;
		}
		debug_presz = v1;
		dpc_duration_update(v2);
	} else if (strncmp(opt, "force_rsc:", 10) == 0) {
		ret = sscanf(opt, "force_rsc:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]force_rsc sscanf not match");
			goto err;
		}
		if (v1) {
			writel(0x000D000D, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
			writel(0x000D000D, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
			writel(0x181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
			writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
			writel(0x181818, dpc_base + DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG);
			writel(0x181818, dpc_base + DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG);
		} else {
			writel(0x00050005, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
			writel(0x00050005, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
			writel(0x080808, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
			writel(0x080808, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
			writel(0x080808, dpc_base + DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG);
			writel(0x080808, dpc_base + DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG);
		}
	} else if (strncmp(opt, "dump", 4) == 0) {
		dpc_dump();
	} else if (strncmp(opt, "analysis", 8) == 0) {
		dpc_analysis();
	} else if (strncmp(opt, "thread:", 7) == 0) {
		ret = sscanf(opt, "thread:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]thread sscanf not match");
			goto err;
		}
		if (v1 == 1) {
			dpc_mtcmos_vote(DPC_SUBSYS_DIS0, 6, true);
			dpc_mtcmos_vote(DPC_SUBSYS_DIS1, 6, true);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 6, true);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 6, true);
			dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, true);
		} else {
			dpc_mtcmos_vote(DPC_SUBSYS_DIS0, 6, false);
			dpc_mtcmos_vote(DPC_SUBSYS_DIS1, 6, false);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 6, false);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 6, false);
			dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, false);
		}
	} else if (strncmp(opt, "rtff:", 5) == 0) {
		ret = sscanf(opt, "rtff:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]rtff sscanf not match");
			goto err;
		}
		writel(v1, g_priv->rtff_pwr_con);
	} else if (strncmp(opt, "vcore:", 6) == 0) {
		ret = sscanf(opt, "vcore:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]vcore sscanf not match");
			goto err;
		}
		if (v1)
			writel(v1, g_priv->vcore_mode_set_va);
		else
			writel(v1, g_priv->vcore_mode_clr_va);
	}

	if (g_priv->mminfra_hangfree) {
		/* enable devapc power check */
		writel(mminfra_hangfree_val, g_priv->mminfra_hangfree);
	}

	dpc_pm_ctrl(false);

	return;
err:
	DPCERR();
}
static ssize_t fs_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const u32 debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512] = {0};
	char *tok, *buf;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;
	buf = cmd_buffer;
	DPCFUNC("%s", cmd_buffer);

	while ((tok = strsep(&buf, " ")) != NULL)
		process_dbg_opt(tok);

	return ret;
}

static const struct file_operations debug_fops = {
	.write = fs_write,
};
#endif

static const struct dpc_funcs funcs = {
	.dpc_enable = dpc_enable,
	.dpc_config = dpc_config,
	.dpc_group_enable = dpc_group_enable,
	.dpc_mtcmos_auto = dpc_mtcmos_auto,
	.dpc_duration_update = dpc_duration_update,
	.dpc_mtcmos_vote = dpc_mtcmos_vote,
	.dpc_dsi_pll_set = dpc_dsi_pll_set,
	.dpc_clear_wfe_event = dpc_clear_wfe_event,
	.dpc_vidle_power_keep = dpc_vidle_power_keep,
	.dpc_vidle_power_release = dpc_vidle_power_release,
	.dpc_vidle_power_keep_by_gce = dpc_vidle_power_keep_by_gce,
	.dpc_vidle_power_release_by_gce = dpc_vidle_power_release_by_gce,
	.dpc_hrt_bw_set = dpc_hrt_bw_set,
	.dpc_srt_bw_set = dpc_srt_bw_set,
	.dpc_dvfs_set = dpc_dvfs_set,
	.dpc_dvfs_trigger = dpc_dvfs_trigger,
	.dpc_channel_bw_set_by_idx = dpc_channel_bw_set_by_idx,
	.dpc_analysis = dpc_analysis,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.dpc_debug_cmd = process_dbg_opt,
#endif
};

static struct mtk_dpc mt6989_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6989,
	.mtcmos_cfg = mt6989_mtcmos_cfg,
	.vdisp_dvfsrc_idle_mask = 0x3,
	.dispvcore_chk_mask = BIT(3),
	.set_mtcmos = mt6989_set_mtcmos,
	.disp_irq_handler = mt6989_disp_irq_handler,
	.mml_irq_handler = mt6989_mml_irq_handler,
	.dt_follow_cfg = 0x3ff,				// follow dt 11~13
	.disp_dt_usage = mt6989_disp_dt_usage,
	.mml_dt_usage = mt6989_mml_dt_usage,
	.total_srt_unit = 100,
	.total_hrt_unit = 30,
	.srt_emi_efficiency = 10000,			// CHECK ME
	.hrt_emi_efficiency = 10000,			// CHECK ME
	.dsi_ck_keep_mask = BIT(5),
};

static struct mtk_dpc mt6878_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6878,
	.mtcmos_cfg = mt6989_mtcmos_cfg,		// same as 6989
	.set_mtcmos = mt6989_set_mtcmos,		// same as 6989
	.disp_irq_handler = mt6989_disp_irq_handler,	// same as 6989
	.mml_irq_handler = mt6989_mml_irq_handler,	// same as 6989
	.srt_emi_efficiency = 10000,			// CHECK ME
	.hrt_emi_efficiency = 10000,			// CHECK ME
};

static struct mtk_dpc mt6991_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6991,
	.mtcmos_cfg = mt6991_mtcmos_cfg,
	.vdisp_dvfsrc_idle_mask = 0xc00000,
	.dispvcore_chk_mask = BIT(29),
	.mminfra_chk_mask = BIT(0),
	.set_mtcmos = mt6991_set_mtcmos,
	.disp_irq_handler = mt6991_irq_handler,
	.dt_follow_cfg = 0x3f3c,
	.dpc2_dt_usage = mt6991_dt_usage,
	.total_srt_unit = 64,
	.total_hrt_unit = 64,
	.srt_emi_efficiency = 13715,			// multiply (1.33 * 33/32(TCU)) = 1.3715
	.hrt_emi_efficiency = 8242,			// divide 0.85 * 33/32(TCU) = *100/82.4242
	.ch_bw_urate = 70,				// divide 0.7
	.vcp_is_alive = true,
	.dsi_ck_keep_mask = BIT(2),
};

static const struct of_device_id mtk_dpc_driver_v2_dt_match[] = {
	{.compatible = "mediatek,mt6989-disp-dpc", .data = &mt6989_dpc_driver_data},
	{.compatible = "mediatek,mt6878-disp-dpc", .data = &mt6878_dpc_driver_data},
	{.compatible = "mediatek,mt6991-disp-dpc-v2", .data = &mt6991_dpc_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dpc_driver_v2_dt_match);

static int mtk_dpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dpc *priv;
	const struct of_device_id *of_id;
	int ret = 0;

	DPCFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	of_id = of_match_device(mtk_dpc_driver_v2_dt_match, dev);
	if (!of_id) {
		DPCERR("DPC device match failed\n");
		return -EPROBE_DEFER;
	}

	priv = (struct mtk_dpc *)of_id->data;
	if (priv == NULL) {
		DPCERR("invalid priv data\n");
		return -EPROBE_DEFER;
	}
	priv->pdev = pdev;
	priv->dev = dev;
	g_priv = priv;
	platform_set_drvdata(pdev, priv);

	mutex_init(&priv->dvfs_bw.lock);
	spin_lock_init(&priv->mtcmos_cfg_lock);
	spin_lock_init(&priv->skip_force_power_lock);

	if (of_find_property(dev->of_node, "power-domains", NULL)) {
		priv->pd_dev = dev;
		if (!pm_runtime_enabled(priv->pd_dev))
			pm_runtime_enable(priv->pd_dev);
		pm_runtime_irq_safe(priv->pd_dev);
	}

	if (of_find_property(dev->of_node, "root-dev", NULL)) {
		struct device_node *node = of_parse_phandle(dev->of_node, "root-dev", 0);
		struct platform_device *pdev = NULL;

		if (node)
			pdev = of_find_device_by_node(node);
		if (pdev)
			priv->root_dev = &pdev->dev;
	}

#if defined(DISP_VIDLE_ENABLE)
	if (of_property_read_u32(dev->of_node, "vidle-mask", &priv->vidle_mask)) {
		DPCERR("failed to get vidle mask:%#x", priv->vidle_mask);
		priv->vidle_mask = 0;
	}
#endif

	ret = dpc_res_init(priv);
	if (ret) {
		DPCERR("res init failed:%d", ret);
		return ret;
	}

	ret = dpc_irq_init(priv);
	if (ret) {
		DPCERR("irq init failed:%d", ret);
		return ret;
	}

	priv->pm_nb.notifier_call = dpc_pm_notifier;
	ret = register_pm_notifier(&priv->pm_nb);
	if (ret) {
		DPCERR("register_pm_notifier failed %d", ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	priv->vcp_nb.notifier_call = dpc_vcp_notifier;
	vcp_A_register_notify_ex(VDISP_FEATURE_ID, &priv->vcp_nb);
#endif

	/* enable external signal from DSI and TE */
	writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* SW_CTRL and SW_VAL=1 */
	dpc_ddr_force_enable(DPC_SUBSYS_DISP, true);
	dpc_ddr_force_enable(DPC_SUBSYS_MML, true);
	writel(0x181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* keep vdisp opp */
	writel(0x1, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
	writel(0x1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* keep HRT and SRT BW */
	writel(0x00010001, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
	writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	priv->fs = debugfs_create_file("dpc_ctrl", S_IFREG | 0440, NULL, NULL, &debug_fops);
	if (IS_ERR(priv->fs))
		DPCERR("debugfs_create_file failed:%ld", PTR_ERR(priv->fs));
#endif
	dpc_mmp_init();

	if (priv->mmsys_id == MMSYS_MT6991) {
		/* set vdisp level */
		// dpc_dvfs_set(DPC_SUBSYS_DISP, 0x4, true);

		/* set channel bw for larb0 HRT READ */
		dpc_ch_bw_set(DPC_SUBSYS_DISP, 2, 363 * 16);

		/* set total HRT bw */
		dpc_hrt_bw_set(DPC_SUBSYS_DISP, 363 * priv->total_hrt_unit, true);

		writel(priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);
		writel(priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);
		writel(0x40, dpc_base + DISP_DPC_ON2SOF_DT_EN);
		writel(0xf, dpc_base + DISP_DPC_ON2SOF_DSI0_SOF_COUNTER); /* should > 2T, cannot be zero */
		writel(0x2, dpc_base + DISP_REG_DPC_DEBUG_SEL);	/* mtcmos_debug */
	}

	mtk_vidle_register(&funcs, DPC_VER2);
	mml_dpc_register(&funcs, DPC_VER2);
	mdp_dpc_register(&funcs, DPC_VER2);
	mtk_vdisp_dpc_register(&funcs);
	mtk_smi_dbg_register_pwr_ctrl_cb(&dpc_smi_pwr_funcs);

	DPCFUNC("-");
	return ret;
}

static int mtk_dpc_remove(struct platform_device *pdev)
{
	DPCFUNC();
	return 0;
}

static void mtk_dpc_shutdown(struct platform_device *pdev)
{
	struct mtk_dpc *priv = platform_get_drvdata(pdev);

	priv->skip_force_power = true;
}

struct platform_driver mtk_dpc_driver_v2 = {
	.probe = mtk_dpc_probe,
	.remove = mtk_dpc_remove,
	.shutdown = mtk_dpc_shutdown,
	.driver = {
		.name = "mediatek-disp-dpc-v2",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dpc_driver_v2_dt_match,
	},
};

static int __init mtk_dpc_init(void)
{
	DPCFUNC("+");
	platform_driver_register(&mtk_dpc_driver_v2);
	DPCFUNC("-");
	return 0;
}

static void __exit mtk_dpc_exit(void)
{
	DPCFUNC();
}

module_init(mtk_dpc_init);
module_exit(mtk_dpc_exit);

MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK Display Power Controller V2.0");
MODULE_LICENSE("GPL");
