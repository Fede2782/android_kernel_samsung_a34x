// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_drv.h"


#define DISP_REG_VDISP_AO_INTEN				(0x000UL)
	#define CPU_INTEN				BIT(0)
	#define SCP_INTEN				BIT(1)
	#define CPU_INT_MERGE			BIT(4)
#define DISP_REG_VDISP_AO_MERGED_IRQB_STS	(0x0004)
#define DISP_REG_VDISP_AO_SELECTED_IRQB_STS	(0x0008)
#define DISP_REG_VDISP_AO_TOP_IRQB_STS		(0x000C)
#define DISP_REG_VDISP_AO_SHADOW_CTRL		(0x0010)
	#define FORCE_COMMIT			BIT(0)
	#define BYPASS_SHADOW			BIT(1)
	#define READ_WRK_REG			BIT(2)
#define DISP_REG_VDISP_AO_INT_SEL_G0_MT6991		(0x0020)
	#define CPU_INTSEL_BIT_00		REG_FLD_MSB_LSB(7, 0)	//GIC:450
	#define CPU_INTSEL_BIT_01		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_02		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_03		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G1_MT6991		(0x0024)
	#define CPU_INTSEL_BIT_04		REG_FLD_MSB_LSB(7, 0)	//GIC:454
	#define CPU_INTSEL_BIT_05		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_06		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_07		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G2_MT6991		(0x0028)
	#define CPU_INTSEL_BIT_08		REG_FLD_MSB_LSB(7, 0)	//GIC:458
	#define CPU_INTSEL_BIT_09		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_10		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_11		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G3_MT6991		(0x002C)
	#define CPU_INTSEL_BIT_12		REG_FLD_MSB_LSB(7, 0)	//GIC:462
	#define CPU_INTSEL_BIT_13		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_14		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_15		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G4_MT6991		(0x0030)
	#define CPU_INTSEL_BIT_16		REG_FLD_MSB_LSB(7, 0)	//GIC:456
	#define CPU_INTSEL_BIT_17		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_18		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_19		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G5_MT6991		(0x0034)
	#define CPU_INTSEL_BIT_20		REG_FLD_MSB_LSB(7, 0)	//GIC:470
	#define CPU_INTSEL_BIT_21		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_22		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_23		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G6_MT6991		(0x0038)
	#define CPU_INTSEL_BIT_24		REG_FLD_MSB_LSB(7, 0)	//GIC:474
	#define CPU_INTSEL_BIT_25		REG_FLD_MSB_LSB(15, 8)

//#define VDISP_AO_SELECTED_DISP_DSI0				  BIT(0)	// 8'd53
//#define VDISP_AO_SELECTED_DISP1_MUTEX0			  BIT(1)	// 8'd41
//#define VDISP_AO_SELECTED_OVL0_EXDMA2			  BIT(2)	// 8'd148

#define	IRQ_TABLE_DISP_DSI0_MT6991			(0x35)	//450
#define	IRQ_TABLE_DISP_DISP1_MUTEX0_MT6991	(0x29)	//451
#define	IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991	(0x94)	//452
#define IRQ_TABLE_DISP_DISP1_WDMA1_MT6991	(0x45)  //453
#define IRQ_TABLE_DISP_OVL1_WDMA0_MT6991	(0xDE)  //454
#define	IRQ_TABLE_DISP_DSI1_MT6991		(0x36)	//455
#define	IRQ_TABLE_DISP_DSI2_MT6991		(0x37)	//456
#define	IRQ_TABLE_DISP_Y2R_MT6991			(36)	//457

#define	IRQ_TABLE_DISP_MDP_RDMA0			(27)	//458
#define IRQ_TABLE_DISP_OVL_WDMA0_MT6991	    (0xAE)  //459
#define	IRQ_TABLE_DISP_DP_INTF0_MT6991		(43)	//460
#define	IRQ_TABLE_DISP_DVO0_MT6991		(56)	//461

#define	IRQ_TABLE_DISP_DP_INTF1_MT6991		(44)	//464

#define IRQ_TABLE_DISP_DITHER2_MT6991          (39)    //469
#define IRQ_TABLE_DISP_DITHER1_MT6991          (22)    //470
#define IRQ_TABLE_DISP_DITHER0_MT6991          (21)    //471
#define IRQ_TABLE_DISP_CHIST1_MT6991           (18)    //472
#define IRQ_TABLE_DISP_CHIST0_MT6991           (17)    //473
#define IRQ_TABLE_DISP_AAL1_MT6991             (8)     //474
#define IRQ_TABLE_DISP_AAL0_MT6991             (7)     //475

static void __iomem *vdisp_ao_base;

struct mtk_vdisp_ao {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};

static inline struct mtk_vdisp_ao *comp_to_vdisp_ao(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_vdisp_ao, ddp_comp);
}

static void mtk_vdisp_ao_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

static void mtk_vdisp_ao_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

void mtk_vdisp_ao_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	/* INTEN, MERGED, SELECTED, TOP */
	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INTEN, 4);

	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INT_SEL_G0_MT6991, 4);
	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INT_SEL_G4_MT6991, 3);

	mtk_serial_dump_reg(baddr, 0x100, 3);
	mtk_serial_dump_reg(baddr, 0x120, 3);
	mtk_serial_dump_reg(baddr, 0x140, 3);

	mtk_serial_dump_reg(baddr, 0x210, 2);
}

void mtk_vdisp_ao_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int reg_val = 0, selectd_sts = 0;

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	reg_val = readl(baddr + DISP_REG_VDISP_AO_INTEN);
	selectd_sts = readl(baddr + DISP_REG_VDISP_AO_SELECTED_IRQB_STS);
}

static void mtk_vdisp_ao_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_vdisp_ao_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_vdisp_ao_funcs = {
	.start = mtk_vdisp_ao_start,
	.stop = mtk_vdisp_ao_stop,
	.prepare = mtk_vdisp_ao_prepare,
	.unprepare = mtk_vdisp_ao_unprepare,
};

static int mtk_vdisp_ao_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPFUNC("+");
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	DDPFUNC("-");
	return 0;
}

static void mtk_vdisp_ao_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_vdisp_ao_component_ops = {
	.bind = mtk_vdisp_ao_bind, .unbind = mtk_vdisp_ao_unbind,
};

static void mtk_vdisp_ao_int_sel_g0_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DSI0_MT6991, CPU_INTSEL_BIT_00);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DISP1_MUTEX0_MT6991, CPU_INTSEL_BIT_01);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991, CPU_INTSEL_BIT_02);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DISP1_WDMA1_MT6991, CPU_INTSEL_BIT_03);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G0_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g1_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL1_WDMA0_MT6991, CPU_INTSEL_BIT_04);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DSI1_MT6991, CPU_INTSEL_BIT_05);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DSI2_MT6991, CPU_INTSEL_BIT_06);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_Y2R_MT6991, CPU_INTSEL_BIT_07);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G1_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g2_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_MDP_RDMA0, CPU_INTSEL_BIT_08);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL_WDMA0_MT6991, CPU_INTSEL_BIT_09);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DP_INTF0_MT6991, CPU_INTSEL_BIT_10);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DVO0_MT6991, CPU_INTSEL_BIT_11);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G2_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g3_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DP_INTF1_MT6991, CPU_INTSEL_BIT_14);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G3_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g4_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER2_MT6991, CPU_INTSEL_BIT_19);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G4_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g5_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER1_MT6991, CPU_INTSEL_BIT_20);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER0_MT6991, CPU_INTSEL_BIT_21);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_CHIST1_MT6991, CPU_INTSEL_BIT_22);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_CHIST0_MT6991, CPU_INTSEL_BIT_23);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G5_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g6_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_AAL1_MT6991, CPU_INTSEL_BIT_24);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_AAL0_MT6991, CPU_INTSEL_BIT_25);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G6_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

void mtk_vdisp_ao_irq_config_MT6991(struct drm_device *drm)
{
	DDPINFO("%s:%d\n", __func__, __LINE__);
	writel(1, vdisp_ao_base + DISP_REG_VDISP_AO_INTEN);	// disable merge irq
	mtk_vdisp_ao_int_sel_g0_MT6991();
	mtk_vdisp_ao_int_sel_g1_MT6991();
	mtk_vdisp_ao_int_sel_g2_MT6991();
	mtk_vdisp_ao_int_sel_g3_MT6991();
	mtk_vdisp_ao_int_sel_g4_MT6991();
	mtk_vdisp_ao_int_sel_g5_MT6991();
	mtk_vdisp_ao_int_sel_g6_MT6991();
}

static int mtk_vdisp_ao_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_vdisp_ao *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_VDISP_AO);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_vdisp_ao_funcs);
	vdisp_ao_base = priv->ddp_comp.regs;
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	writel(1, vdisp_ao_base + DISP_REG_VDISP_AO_INTEN);	// disable merge irq

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_vdisp_ao_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPFUNC("-");

	return ret;
}

static int mtk_vdisp_ao_remove(struct platform_device *pdev)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_vdisp_ao_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_vdisp_ao_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-vdisp-ao", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vdisp_ao_driver_dt_match);

struct platform_driver mtk_vdisp_ao_driver = {
	.probe = mtk_vdisp_ao_probe,
	.remove = mtk_vdisp_ao_remove,
	.driver = {

			.name = "mediatek-vdisp-ao",
			.owner = THIS_MODULE,
			.of_match_table = mtk_vdisp_ao_driver_dt_match,
		},
};
