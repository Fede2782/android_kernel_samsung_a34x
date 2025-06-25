// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

static int mt6877_clk_en;
static struct mutex mt6877_clk_lock;
static unsigned long vcore_frq;

static void _dump_pll(struct apu_clk_gp *aclk)
{
	char buf[32];

	memset(buf, 0, 32);
	snprintf(buf, 32, "PLL%d 0x%llx:", aclk->pll_sel, aclk->pll_phyaddr);
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET,
			   16, 4, (aclk->pll_regs), 0x50, 1);
	snprintf(buf, 32, "PLL%d 0x%llx:", aclk->pll_sel, aclk->pll_phyaddr + APU_PLL4H_FQMTR_CON0);
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET,
			   16, 4, (aclk->pll_regs + APU_PLL4H_FQMTR_CON0), 0x10, 1);
}
static unsigned long mt6877_clk_get_rate(struct apu_clk_gp *aclk)
{
	unsigned long freq = 0;
	struct arm_smccc_res res = {0};
	int ret = 0, val = 0;
	struct apu_dev *ad = NULL;

	if (aclk->top_mux->clks == NULL)
		return 0;

	ad = (struct apu_dev *)dev_get_drvdata(aclk->dev);

	mutex_lock(&mt6877_clk_lock);

	/* vcore use fake rate */
	if (ad->user == APUCORE || !aclk->pll_regs) {
		freq = vcore_frq;
		goto out;
	}

	if (!mt6877_clk_en) {
		aclk_info(aclk->dev, "mt6877_clk_en = %d, cannot get rate\n", mt6877_clk_en);
		goto out;
	}

	/* step1. let atf start fmeter */
	arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
			MTK_APUPWR_SMC_OP_FMETER_CTL,
			FMETER_PLL, FMETER_STEP1, aclk->pll_sel, 0, 0, 0, &res);
	if (res.a0) {
		_dump_pll(aclk);
		aclk_err(aclk->dev, "Step1 Fail in pll_sel:%u\n", aclk->pll_sel);
		goto out;
	}

	/* step2. wait frequency meter finish */
	ret = readl_relaxed_poll_timeout_atomic((aclk->pll_regs + APU_PLL4H_FQMTR_CON0),
						val, !(val & 0x10UL),
						POLL_INTERVAL, PLL_POLL_TIMEOUT);
	if (ret) {
		_dump_pll(aclk);
		aclk_err(aclk->dev, "Step2 timeout! [PLL%d]con0: 0x%x, con1: 0x%x\n",
				aclk->pll_sel, val, apu_readl(aclk->pll_regs + APU_PLL4H_FQMTR_CON1));
		goto out;
	}

	freq = apu_readl(aclk->pll_regs + APU_PLL4H_FQMTR_CON1) & 0xFFFF;
	freq = (((freq * 26000)) / (256 * 1000)) * 1000000;

	/* step3. wait frequency meter finish */
	arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
			MTK_APUPWR_SMC_OP_FMETER_CTL,
			FMETER_PLL, FMETER_STEP3, aclk->pll_sel, 0, 0, 0, &res);
	if (res.a0) {
		_dump_pll(aclk);
		aclk_err(aclk->dev, "step3 Fail in pll_sel:%u\n", aclk->pll_sel);
		goto out;
	}

	if (aclk->div2)
		freq >>= aclk->div2;
out:
	mutex_unlock(&mt6877_clk_lock);
	return freq;
}

static int mt6877_clk_set_rate(struct apu_clk_gp *aclk, unsigned long rate)
{
	struct apu_dev *ad = NULL, *tmp_ad = NULL;
	int dom = 0;
	struct arm_smccc_res res = {0};

	if (aclk->top_mux->clks == NULL)
		return 0;

	ad = (struct apu_dev *)dev_get_drvdata(aclk->dev);

	mutex_lock(&mt6877_clk_lock);

	/* Below 400Mhz, set div2 as true */
	if (rate < 400000000) {
		if (ad->user == MDLA) {
			tmp_ad = apu_find_device(MDLA0);
			tmp_ad->aclk->div2 = 1;
		}
		if (ad->user == VPU) {
			tmp_ad = apu_find_device(VPU0);
			tmp_ad->aclk->div2 = 1;
			tmp_ad = apu_find_device(VPU1);
			tmp_ad->aclk->div2 = 1;
		}
		aclk->div2 = 1;
	} else {
		if (ad->user == MDLA) {
			tmp_ad = apu_find_device(MDLA0);
			tmp_ad->aclk->div2 = 0;
		}
		if (ad->user == VPU) {
			tmp_ad = apu_find_device(VPU0);
			tmp_ad->aclk->div2 = 0;
			tmp_ad = apu_find_device(VPU1);
			tmp_ad->aclk->div2 = 0;
		}
		aclk->div2 = 0;
	}
	/* vcore use fake rate */
	if (ad->user == APUCORE || !aclk->pll_regs) {
		vcore_frq = rate;
		goto out;
	}

	if (!mt6877_clk_en) {
		aclk_info(aclk->dev, "mt6877_clk_en = %d, cannot set rate\n", mt6877_clk_en);
		goto out;
	}

	/* ACC only support some power domain */
	dom = apu_dev2_domain(ad->user);
	if (dom != -ENODEV) {
		arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
				MTK_APUPWR_SMC_OP_PLL_SET_RATE,
				(size_t)TOKHZ(rate), (size_t)aclk->div2,
				(size_t)dom, 0, 0, 0, &res);

		if (res.a0) {
			_dump_pll(aclk);
			aclk_err(aclk->dev, "[%s] fail set %lu ret:%lu\n",
						__func__, rate, res.a0);
			res.a0 = EINVAL;
			goto out;
		}
		aclk_info(aclk->dev, "[%s] set %lu ret:%lu\n",
					__func__, rate, res.a0);
	}
out:
	mutex_unlock(&mt6877_clk_lock);
	apu_get_power_info(0);
	return res.a0;
}

static int mt6877_clk_enable(struct apu_clk_gp *aclk)
{
	int ret = 0, dom = 0;
	struct apu_clk *dst;
	struct apu_dev *ad = NULL;
	struct arm_smccc_res res = {0};
	enum DVFS_USER usr = 0;

	ad = (struct apu_dev *)dev_get_drvdata(aclk->dev);
	mutex_lock(&aclk->clk_lock);
	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->always_on) {
			ret = clk_bulk_prepare_enable(dst->clk_num, dst->clks);
			if (ret) {
				aclk_err(aclk->dev, "[%s] %s fail, ret %d\n",
					 __func__, dst->clks->id, ret);
				goto out;
			}
			if (dst->keep_enable)
				dst->always_on = 1;
		}
		if (!dst->fix_rate) {
			ret = _clk_apu_bulk_setparent(dst, aclk->sys_mux);
			if (ret)
				goto out;
		}
	}

	if (ad->user == APUCONN) {
		mutex_lock(&mt6877_clk_lock);
		/* ACC only support some power domain */
		for (usr = 0; usr < APUVB; usr++) {
			dom = apu_dev2_domain(usr);
			if (dom != -ENODEV) {
				arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
						MTK_APUPWR_SMC_OP_ACC_TOGGLE,
						(size_t)dom,
						(size_t)1,
						0, 0, 0, 0, &res);
				if (res.a0) {
					aclk_info(aclk->dev, "domain@%d, enabled FAIL, ret = %lu\n",
								dom, res.a0);
					ret = res.a0;
					mutex_unlock(&mt6877_clk_lock);
					goto out;
				}
				ret = res.a0;
			}
		}
		mt6877_clk_en = 1;
		mutex_unlock(&mt6877_clk_lock);
	}
out:
	mutex_unlock(&aclk->clk_lock);
	apu_get_power_info(0);
	return ret;
}

static void mt6877_clk_disable(struct apu_clk_gp *aclk)
{
	int ret = 0, dom = 0;
	struct apu_clk *dst;
	struct apu_dev *ad = NULL;
	struct arm_smccc_res res = {0};
	enum DVFS_USER usr = 0;

	ad = (struct apu_dev *)dev_get_drvdata(aclk->dev);
	mutex_lock(&aclk->clk_lock);
	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->fix_rate) {
			ret = _clk_apu_bulk_setparent(dst, aclk->sys_mux);
			if (ret)
				goto out;
		}
		if (!dst->always_on && !dst->keep_enable)
			clk_bulk_disable_unprepare(dst->clk_num, dst->clks);
	}

	if (ad->user == APUCONN) {
		mutex_lock(&mt6877_clk_lock);
		/* ACC only support some power domain */
		for (usr = 0; usr < APUVB; usr++) {
			dom = apu_dev2_domain(usr);
			if (dom != -ENODEV) {
				arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
						MTK_APUPWR_SMC_OP_ACC_TOGGLE,
						(size_t)dom,
						(size_t)0,
						0, 0, 0, 0, &res);
				if (res.a0) {
					aclk_err(aclk->dev, "domain@%d, disable FAIL, ret = %lu\n",
								dom, res.a0);
					mutex_unlock(&mt6877_clk_lock);
					goto out;
				}

				arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
						MTK_APUPWR_SMC_OP_ACC_SET_PARENT,
						(size_t)DVFS_FREQ_ACC_SOC,
						(size_t)dom,
						0, 0, 0, 0, &res);
				if (res.a0) {
					aclk_err(aclk->dev, "domain@%d, set_parent SOC FAIL, ret = %lu\n",
								dom, res.a0);
					mutex_unlock(&mt6877_clk_lock);
					goto out;
				}
			}
		}
		mt6877_clk_en = 0;
		mutex_unlock(&mt6877_clk_lock);
	}
out:
	mutex_unlock(&aclk->clk_lock);
	apu_get_power_info(0);
}

static int mt6877_clk_acc_init(struct apu_clk_gp *aclk)
{
	int ret = 0, dom = 0;
	struct apu_dev *ad = NULL;
	struct arm_smccc_res res = {0};
	enum DVFS_USER usr = 0;

	ad = (struct apu_dev *)dev_get_drvdata(aclk->dev);
	if (aclk->pll_phyaddr) {
		aclk->pll_regs = ioremap(aclk->pll_phyaddr, PAGE_SIZE);
		if (!aclk->pll_regs) {
			aclk_err(aclk->dev, "[%s] cannot iomap pa:0x%llx\n",
					__func__, aclk->pll_phyaddr);
			ret = EINVAL;
			goto out;
		}
	}

	if (ad->user == APUCONN) {
		/* ACC only support some power domain */
		for (usr = 0; usr < APUVB; usr++) {
			dom = apu_dev2_domain(usr);
			if (dom != -ENODEV) {
				arm_smccc_smc(MTK_SIP_APUPWR_CONTROL,
						MTK_APUPWR_SMC_OP_ACC_INIT,
						(size_t)dom,
						(size_t)0,
						0, 0, 0, 0, &res);
				if (res.a0) {
					aclk_err(aclk->dev, "domain@%d, ACC_init FAIL, ret = %lu\n",
						 dom, res.a0);
					ret = res.a0;
					goto out;
				}
			}
		}
		mutex_init(&mt6877_clk_lock);
	}
out:
	return ret;
}

static struct apu_clk_ops mt6877_clk_ops = {
	.prepare = clk_apu_prepare,
	.unprepare = clk_apu_unprepare,
	.enable = mt6877_clk_enable,
	.disable = mt6877_clk_disable,
	.cg_enable = clk_apu_cg_enable,
	.cg_status = clk_apu_cg_status,
	.set_rate = mt6877_clk_set_rate,
	.get_rate = mt6877_clk_get_rate,
	.acc_init = mt6877_clk_acc_init,
};

struct apu_clk mt6877_core_topmux = {
	.fix_rate = 1,
};

struct apu_clk mt6877_conn_topmux = {
	.fix_rate = 1,
};

struct apu_clk mt6877_iommu_topmux = {
	.fix_rate = 1,
};

static struct apu_cg mt6877_conn_cg[] = {
	{
		.phyaddr = 0x19029000,
		.cg_ctl = {0, 4, 8},
	},
	{
		.phyaddr = 0x19020000,
		.cg_ctl = {0, 4, 8},
	},
	{
		.phyaddr = 0x19024000,
		.cg_ctl = {0, 4, 8},
	},
};

static struct apu_cgs mt6877_conn_cgs = {
	.cgs = &mt6877_conn_cg[0],
	.clk_num = ARRAY_SIZE(mt6877_conn_cg),
};

struct apu_clk mt6877_mdla_topmux = {
	.fix_rate = 1,
};

struct apu_clk mt6877_mdla0_topmux = {
	.fix_rate = 1,
};

static struct apu_cg mt6877_mdla0_cg[] = {
	{
		.phyaddr = 0x19034000,
		.cg_ctl = {0, 4, 8},
	},
};

static struct apu_cgs mt6877_mdla0_cgs = {
	.cgs = &mt6877_mdla0_cg[0],
	.clk_num = ARRAY_SIZE(mt6877_mdla0_cg),
};

struct apu_clk mt6877_vpu_topmux = {
	.fix_rate = 1,
};

struct apu_clk mt6877_vpu0_topmux = {
	.fix_rate = 1,
};

struct apu_clk mt6877_vpu1_topmux = {
	.fix_rate = 1,
};

static struct apu_cg mt6877_vpu0_cg[] = {
	{
		.phyaddr = 0x19030000,
		.cg_ctl = {0x100, 0x104, 0x108},
	},
};

static struct apu_cgs mt6877_vpu0_cgs = {
	.cgs = &mt6877_vpu0_cg[0],
	.clk_num = ARRAY_SIZE(mt6877_vpu0_cg),
};

static struct apu_cg mt6877_vpu1_cg[] = {
	{
		.phyaddr = 0x19031000,
		.cg_ctl = {0x100, 0x104, 0x108},
	},
};

static struct apu_cgs mt6877_vpu1_cgs = {
	.cgs = &mt6877_vpu1_cg[0],
	.clk_num = ARRAY_SIZE(mt6877_vpu1_cg),
};

static struct apu_clk_gp mt6877_mdla_clk_gp = {
	.top_mux = &mt6877_mdla_topmux,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL1_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_mdla0_clk_gp = {
	.top_mux = &mt6877_mdla0_topmux,
	.cg = &mt6877_mdla0_cgs,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL1_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_vpu_clk_gp = {
	.top_mux = &mt6877_vpu_topmux,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL2_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_vpu0_clk_gp = {
	.top_mux = &mt6877_vpu0_topmux,
	.cg = &mt6877_vpu0_cgs,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL2_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_vpu1_clk_gp = {
	.top_mux = &mt6877_vpu1_topmux,
	.cg = &mt6877_vpu1_cgs,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL2_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_iommu_clk_gp = {
	.top_mux = &mt6877_iommu_topmux,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL4_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_conn_clk_gp = {
	.top_mux = &mt6877_conn_topmux,
	.cg = &mt6877_conn_cgs,
	.ops = &mt6877_clk_ops,
	.pll_sel = FM_PLL3_CK,
	.pll_phyaddr = 0x190f3000,
};

static struct apu_clk_gp mt6877_core_clk_gp = {
	.top_mux = &mt6877_core_topmux,
	.ops = &mt6877_clk_ops,
};
