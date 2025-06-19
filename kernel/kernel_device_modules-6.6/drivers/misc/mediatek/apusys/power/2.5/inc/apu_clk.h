/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_CLK_H__
#define __APU_CLK_H__

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/module.h>

#include "apu_devfreq.h"

enum CG_IDX {
	CG_CON = 0,
	CG_SET,
	CG_CLR,
	CG_REGS,
};

#define	POSDIV_SHIFT	(24)            /* bit */
#define	DDS0_SHIFT	(9)
#define	DDS1_SHIFT	(5)
#define	DDS_SHIFT	(14)            /* bit */
#define	PLL_FIN		(26)            /* MHz */

struct apu_cg {
	u32 cg_ctl[CG_REGS];
	phys_addr_t phyaddr;
	void __iomem *regs;
};

struct apu_cgs {
	struct apu_cg *cgs;
	struct device *dev;
	u32 clk_num;
};

struct apmixpll {
	ulong offset;
	ulong multiplier;
	void __iomem *regs;
};

struct apu_clk_parent {
	ulong rate;
	struct clk *clk;
};

struct apu_clk_gp;
struct apu_clk;

struct apu_clk {
	struct device *dev;
	/* for top/sys mux */
	struct clk_bulk_data *clks;
	struct apu_clk *parents;
	/* for apumix pll */
	struct apmixpll *mixpll;
	u32 clk_num;
	unsigned long def_freq;
	unsigned long shut_freq;

	/* apu_clk_gp flags */
	unsigned always_on:1;	/* clk never enable/disable */
	unsigned keep_enable:1;	/* enable once and never disable */
	unsigned fix_rate:1;		/* fix rate of all clks while enable/disable */
	unsigned dynamic_alloc:1;	/* allocated from dts */

} __aligned(sizeof(long));

/**
 * struct apu_clk_ops -  Callback operations for apu clocks.
 *
 * @enable:	Enable the clock atomically. This must not return until the
 *		clock is generating a valid clock signal, usable by consumer
 *		devices. Called with enable_lock held. This function must not
 *		sleep.
 *
 * @disable:	Disable the clock atomically. Called with enable_lock held.
 *		This function must not sleep.
 *
 * @set_rate:	Change the rate of this clock. The requested rate is specified
 *		by the second argument, which should typically be the return
 *		of .round_rate call.  The third argument gives the parent rate
 *		which is likely helpful for most .set_rate implementation.
 *		Returns 0 on success, -EERROR otherwise.
 *
 * @get_rate:   returns the closest rate actually supported by the clock.
 *
 */
struct apu_clk_ops {
	int		(*prepare)(struct apu_clk_gp *aclk);
	void    (*unprepare)(struct apu_clk_gp *aclk);
	int		(*enable)(struct apu_clk_gp *aclk);
	void	(*disable)(struct apu_clk_gp *aclk);
	int		(*cg_enable)(struct apu_clk_gp *aclk);
	void	(*cg_disable)(struct apu_clk_gp *aclk);
	int		(*cg_status)(struct apu_clk_gp *aclk, u32 *result);
	ulong	(*get_rate)(struct apu_clk_gp *aclk);
	int		(*set_rate)(struct apu_clk_gp *aclk, unsigned long rate);
	int		(*acc_init)(struct apu_clk_gp *aclk); /* for mt6877 */
};

struct apu_clk_gp {
	struct device *dev;
	struct mutex clk_lock;
	/* the top_mux/sys_mux/Pll for this composite clk */
	struct apu_clk *top_mux;
	struct apu_clk *sys_mux;
	struct apu_clk *top_pll;
	struct apu_clk *apmix_pll;
	struct apu_cgs *cg;
	/* the clk_hw/ops for this apu composite clk */
	struct apu_clk_ops	*ops;

	/* only use for mt6877 */
	unsigned int pll_sel;
	phys_addr_t pll_phyaddr;
	void __iomem *pll_regs;
	unsigned div2:1; /* div need or not */

	unsigned fhctl:1;	/* freq hopping or not */

};

struct apu_clk_array {
	char *name;
	struct apu_clk_gp *aclk_gp;
};

/* only for mt6877 */
enum MTK_APUPWR_SMC_OP {
	MTK_APUPWR_SMC_OP_ACC_INIT = 0,
	MTK_APUPWR_SMC_OP_ACC_TOGGLE,
	MTK_APUPWR_SMC_OP_ACC_SET_PARENT,
	MTK_APUPWR_SMC_OP_PLL_SET_RATE,
	MTK_APUPWR_SMC_OP_FMETER_CTL,
	MTK_APUPWR_SMC_OP_NUM
};

/* use for tell atf park acc to SOC */
#define DVFS_FREQ_ACC_SOC   3

/* only for mt6877 */
#define FMETER_PLL			1
#define FMETER_ACC			2
#define FMETER_STEP1		1
#define FMETER_STEP2		2
#define FMETER_STEP3		3
#define FM_PLL1_CK			0
#define FM_PLL2_CK			1
#define FM_PLL3_CK			2
#define FM_PLL4_CK			3
#define FM_ACC0             0x0
#define FM_ACC1             0x1
#define FM_ACC2             0x10
#define FM_ACC4             0x40
#define FM_ACC5             0x44
#define FM_ACC7             0x68

#define ACC_CONFG_SET0      0x000
#define ACC_CONFG_SET1      0x004
#define ACC_CONFG_SET2      0x008
#define ACC_CONFG_SET4      0x010
#define ACC_CONFG_SET5      0x014
#define ACC_CONFG_SET7      0x01C

#define ACC_CONFG_CLR0      0x040
#define ACC_CONFG_CLR1      0x044
#define ACC_CONFG_CLR2      0x048
#define ACC_CONFG_CLR4      0x050
#define ACC_CONFG_CLR5      0x054
#define ACC_CONFG_CLR7      0x05C

#define ACC_FM_CONFG_SET    0x0C0
#define ACC_FM_CONFG_CLR    0x0C4
#define ACC_FM_SEL          0x0C8
#define ACC_FM_CNT          0x0CC

#define APU_PLL4H_FQMTR_CON0 0x200
#define APU_PLL4H_FQMTR_CON1 0x204

struct apu_clk_gp *clk_apu_get_clkgp(struct apu_dev *ad, const char *name);
void clk_apu_show_clk_info(struct apu_clk *dst, bool only_active);


#endif
