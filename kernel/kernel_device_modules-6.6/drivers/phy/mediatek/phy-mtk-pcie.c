// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "phy-mtk-io.h"

/* PHY sif registers */
#define PEXTP_DIG_GLB_00		0x0
#define PRB_SEL_TO_101			0x101
#define PEXTP_DIG_GLB_04		0x4
#define PEXTP_DIG_GLB_08		0x8
#define CKGEN_PRB_SEL_TO_A000A		0xa000a
#define PEXTP_DIG_GLB_10		0x10
#define PEXTP_DIG_GLB_20		0x20
#define RG_XTP_BYPASS_PIPE_RST		BIT(4)
#define RG_XTP_BYPASS_PIPE_RST_RC	BIT(17)
#define PEXTP_DIG_GLB_28		0x28
#define RG_XTP_PCIE_MODE		BIT(3)
#define RG_XTP_PHY_CLKREQ_N_IN		GENMASK(13, 12)
#define PEXTP_DIG_GLB_30		0x30
#define RG_XTP_CKBG_STAL_STB_T_SEL	GENMASK(25, 16)
#define CKBG_STAL_STB_T_SEL_TO_0	0x0
#define PEXTP_DIG_GLB_38		0x38
#define RG_XTP_TPLL_SET_STB_T_SEL	GENMASK(7, 2)
#define TPLL_SET_STB_T_SEL_TO_3F	0x3f
#define RG_XTP_TPLL_PWE_ON_STB_T_SEL	GENMASK(9, 8)
#define TPLL_PWE_ON_STB_T_SEL_TO_3	0x3
#define PEXTP_DIG_GLB_50		0x50
#define RG_XTP_CKM_EN_L1S0		BIT(13)
#define RG_XTP_CKM_EN_L1S1		BIT(14)
#define PEXTP_DIG_PROBE_OUT		0xd0
#define PEXTP_DIG_GLB_70		0x70
#define RG_XTP_PIPE_UPDT		BIT(4)
#define RG_XTP_PIPE_TX_SWING		BIT(22)
#define PEXTP_DIG_GLB_A4		0xa4
#define RG_XTP_FRC_TX_SWING		BIT(1)
#define PEXTP_DIG_GLB_D0		0xd0
#define PEXTP_DIG_GLB_F4		0xf4
#define RG_XTP_TPLL_ISO_EN_STB_T_SEL	GENMASK(13, 12)
#define TPLL_ISO_EN_STB_T_SEL_TO_3	0x3

#define PEXTP_DIG_TPLL0_6C		0x106c
#define RG_XTP_GLB_TPLL0_SDM_SSC_CTL	GENMASK(16, 15)
#define DISABLE_SSC			0x2

#define PEXTP_DIG_TPLL0_78		0x1078
#define RG_XTP_VCO_CFIX_EN_GEN1		GENMASK(21, 20)
#define RG_XTP_VCO_CFIX_EN_GEN2		GENMASK(23, 22)
#define HIGH_VCO_FREQ			0x0

/* PHY ANA GLB registers */
#define PEXTP_DIG_LN_TRX_70		0x3070
#define RG_XTP_LN_FRC_RX_AEQ_DFETP5	BIT(21)
#define RG_XTP_LN_FRC_RX_AEQ_DFETP4	BIT(29)

#define PEXTP_DIG_LN_TRX_74		0x3074
#define RG_XTP_LN_FRC_RX_AEQ_DFETP3	BIT(6)
#define RG_XTP_LN_FRC_RX_AEQ_DFETP2	BIT(14)
#define RG_XTP_LN_FRC_RX_AEQ_DFETP1	BIT(23)

#define PEXTP_DIG_LN_TRX_E8		0x30e8
#define RG_XTP_LN_RX_LF_CTLE_CSEL_GEN4	GENMASK(14, 12)
#define CTLE_CSEL_GEN4_TO_1		0x1

#define PEXTP_DIG_LN_RX_F0		0x50f0
#define RG_XTP_LN_RX_GEN1_CTLE1_CSEL	GENMASK(3, 0)
#define GEN1_CTLE1_CSEL_TO_D		0xd
#define RG_XTP_LN_RX_GEN2_CTLE1_CSEL	GENMASK(7, 4)
#define GEN2_CTLE1_CSEL_TO_D		0xd
#define RG_XTP_LN_RX_GEN3_CTLE1_CSEL	GENMASK(11, 8)
#define GEN3_CTLE1_CSEL_TO_D		0xd

#define PEXTP_DIG_LN_RX2_04			0x6004
#define RG_XTP_LN_RX_AEQ_EGEQ_RATIO_GEN3	GENMASK(21, 16)
#define RG_XTP_LN_RX_AEQ_EGEQ_RATIO_GEN4	GENMASK(29, 24)
#define AEQ_EGEQ_RATIO_GEN3_TO_22		0x16
#define AEQ_EGEQ_RATIO_GEN4_TO_22		0x16

#define PEXTP_DIG_LN_RX2_94		0x6094
#define RG_XTP_LN_RX_CDR_DLY_OFF_GEN3	BIT(6)
#define RG_XTP_LN_RX_CDR_DLY_OFF_GEN4	BIT(7)

#define PEXTP_DIG_LN_RX2_A4		0x60a4
#define RG_XTP_LN_RX_AEQ_OFORCE_GEN3	GENMASK(7, 1)
#define AEQ_OFORCE_GEN3_TO_7F		0x7f
#define RG_XTP_LN_RX_AEQ_OFORCE_GEN4	GENMASK(19, 13)
#define AEQ_OFORCE_GEN4_TO_7F		0x7f

#define PEXTP_ANA_GLB_00_REG		0x9000

#define PEXTP_ANA_GLB_10_REG		0x9010
#define GLB_TPLL0_RST_DLY		GENMASK(5, 4)
#define RESET_COUNTER_SELECT_2		0x2

#define PEXTP_ANA_GLB_14_REG		0x9014
#define GLB_TPLL0_DEBUG_SEL		GENMASK(13, 11)

#define PEXTP_ANA_GLB_6			(PEXTP_ANA_GLB_00_REG + 0x18)
#define PEXTP_ANA_GLB_9			(PEXTP_ANA_GLB_00_REG + 0x24)
/* Internal Resistor Selection of TX Bias Current */
#define EFUSE_GLB_INTR_SEL		GENMASK(28, 24)

#define PEXTP_ANA_GLB_2C		0x902c
#define RG_XTP_GLB_TPLL1_RESERVE_0	GENMASK(7, 0)
#define TPLL1_P_PATH_GAIN_TO_05		0xf1

#define PEXTP_ANA_GLB_50_REG		0x9050
#define PEXTP_ANA_GLB_54_REG		0x9054

#define PEXTP_ANA_GLB_60		0x9060
#define RG_XTP_GLB_BIAS_INTR_CTRL	GENMASK(5, 0)

#define PEXTP_ANA_GLB_C0		0x90c0
#define RG_XTP_GLB_BIAS_V2V_VTRIM	GENMASK(9, 6)

#define PEXTP_ANA_LN0_TRX_REG		0xa000
#define RG_XTP_LN_TX_RESERVE		GENMASK(31, 16)
#define LN_TX_RESERVE_TO_8		0x8

#define PEXTP_ANA_LN_TRX_C		0xa00c
#define RG_XTP_LN_TX_RSWN_IMPSEL	GENMASK(20, 16)

#define PEXTP_ANA_LN_TRX_34		0xA034
#define RG_XTP_LN_RX_FE			BIT(15)

#define PEXTP_ANA_LN_TRX_6C		0xA06C
#define RG_XTP_LN_RX_AEQ_CTLE_ERR_TYPE	GENMASK(14, 13)
#define AEQ_CTLE_ERR_TYPE_H15		0x0
#define AEQ_CTLE_ERR_TYPE_H15_H25	0x1

#define PEXTP_ANA_LN_TRX_A0		0xa0a0
#define RG_XTP_LN_TX_IMPSEL_PMOS	GENMASK(4, 0)
#define TX_IMPSEL_PMOS_TO_A		0xa
#define RG_XTP_LN_TX_IMPSEL_NMOS	GENMASK(11, 7)
#define TX_IMPSEL_NMOS_TO_9		0x9
#define RG_XTP_LN_RX_IMPSEL		GENMASK(15, 12)

#define PEXTP_ANA_LN_TRX_A8		0xa0a8
#define RG_XTP_LN_RX_LEQ_RL_CTLE_CAL	GENMASK(6, 2)
#define RG_XTP_LN_RX_LEQ_RL_VGA_CAL	GENMASK(11, 7)
#define RG_XTP_LN_RX_LEQ_RL_DFE_CAL	GENMASK(23, 19)

#define PEXTP_DIG_LN_TX_RSWN_4		0xb004
#define PEXTP_DIG_LN_TX_RSWN_8		0xb008
#define PEXTP_DIG_LN_TX_RSWN_C		0xb00c
#define PEXTP_DIG_LN_TX_RSWN_10		0xb010
#define PEXTP_DIG_LN_TX_RSWN_14		0xb014
#define PEXTP_DIG_LN_TX_RSWN_18		0xb018
#define RG_XTP_LN_TX_MGX_PX_CM1		GENMASK(5, 0)
#define MGX_PX_CM1_TO_1			0x1
#define MGX_PX_CM1_TO_2			0x2
#define RG_XTP_LN_TX_MGX_PX_C0		GENMASK(13, 8)
#define MGX_PX_C0_TO_A			0xa
#define MGX_PX_C0_TO_B			0xb
#define MGX_PX_C0_TO_C			0xc
#define RG_XTP_LN_TX_MGX_PX_CP1		GENMASK(21, 16)
#define MGX_PX_CP1_TO_1			0x1
#define MGX_PX_CP1_TO_2			0x2

#define PEXTP_ANA_TX_REG		0x04
/* TX PMOS impedance selection */
#define EFUSE_LN_TX_PMOS_SEL		GENMASK(5, 2)
/* TX NMOS impedance selection */
#define EFUSE_LN_TX_NMOS_SEL		GENMASK(11, 8)

#define PEXTP_ANA_RX_REG		0x3c
/* RX impedance selection */
#define EFUSE_LN_RX_SEL			GENMASK(3, 0)

#define PEXTP_ANA_LANE_OFFSET		0x100

/* PHY ckm regsiters */
#define XTP_CKM_DA_REG_38		0x38
#define RG_CKM_BIAS_WAIT_PRD		GENMASK(21, 16)
#define CKM_BIAS_WAIT_PRD_TO_4US	0x4
#define XTP_CKM_DA_REG_3C		0x3C
#define RG_CKM_PADCK_REQ		GENMASK(13, 12)
#define RG_CKM_PROBE_SEL		GENMASK(19, 17)
#define XTP_CKM_DA_REG_44		0x44
#define XTP_CKM_DA_REG_D4		0xD4
#define RG_CKM_CKTX_IMPSEL_PMOS		GENMASK(19, 16)
#define RG_CKM_CKTX_IMPSEL_NMOS		GENMASK(23, 20)
#define RG_CKM_CKTX_IMPSEL_SW		GENMASK(27, 24)

/* EFUSE */
#define SPHY3_EFUSE_MAX_LANE		2

#define EFUSE_GLB_BIAS_INTR_CTRL	GENMASK(5, 0)
#define EFUSE_GLB_BIAS_V2V_VTRIM	GENMASK(9, 6)
#define EFUSE_CKM_CKTX_IMPSEL_PMOS	GENMASK(13, 10)
#define EFUSE_CKM_CKTX_IMPSEL_NMOS	GENMASK(17, 14)
#define EFUSE_CKM_CKTX_IMPSEL_RMID	GENMASK(21, 18)
#define EFUSE_LN0_TX_RSWN_IMPSEL	GENMASK(26, 22)
#define EFUSE_LN1_TX_RSWN_IMPSEL	GENMASK(31, 27)

#define EFUSE_LN_RX_LEQ_RL_CTLE_CAL	GENMASK(4, 0)
#define EFUSE_LN_RX_LEQ_RL_VGA_CAL	GENMASK(9, 5)
#define EFUSE_LN_RX_LEQ_RL_DEF_CAL	GENMASK(14, 10)
#define EFUSE_LN_RX_IMPSEl		GENMASK(18, 15)
#define EFUSE_LN_TX_IMPSEL_PMOS		GENMASK(24, 20)
#define EFUSE_LN_TX_IMPSEL_NMOS		GENMASK(29, 25)

/**
 * struct mtk_pcie_lane_efuse - eFuse data for each lane
 * @tx_pmos: TX PMOS impedance selection data
 * @tx_nmos: TX NMOS impedance selection data
 * @rx_data: RX impedance selection data
 * @lane_efuse_supported: software eFuse data is supported for this lane
 */
struct mtk_pcie_lane_efuse {
	u32 tx_pmos;
	u32 tx_nmos;
	u32 rx_data;
	bool lane_efuse_supported;
};

/**
 * struct mtk_pcie_phy_data - phy data for each SoC
 * @sw_efuse_supported: support software to load eFuse data
 * @phy_int: special init function of each SoC
 * @get_efuse_info: get efuse info function of each SoC
 */
struct mtk_pcie_phy_data {
	bool sw_efuse_supported;
	int (*phy_init)(struct phy *phy);
	int (*get_efuse_info)(struct phy *phy);
};

/**
 * struct mtk_pcie_phy - PCIe phy driver main structure
 * @dev: pointer to device
 * @phy: pointer to generic phy
 * @sif_base: IO mapped register SIF base address of system interface
 * @ckm_base: IO mapped register CKM base address of system interface
 * @clks: PCIe PHY clocks
 * @num_clks: PCIe PHY clocks count
 * @num_lanes: supported lane numbers
 * @mode: decide PCIe PHY mode
 * @data: pointer to SoC dependent data
 * @sw_efuse_en: software eFuse enable status
 * @short_reach_en: short reach enable status
 * @efuse_glb_intr: internal resistor selection of TX bias current data
 * @efuse: pointer to eFuse data for each lane
 */
struct mtk_pcie_phy {
	struct device *dev;
	struct phy *phy;
	void __iomem *sif_base;
	void __iomem *ckm_base;
	struct clk_bulk_data *clks;
	int num_clks;
	int num_lanes;
	enum phy_mode mode;
	const struct mtk_pcie_phy_data *data;

	bool sw_efuse_en;
	bool short_reach_en;
	bool disable_ssc;
	u32 efuse_glb_intr;
	u32 *efuse_info;
	size_t efuse_info_len;
	struct mtk_pcie_lane_efuse *efuse;
};

static void mtk_pcie_efuse_set_lane(struct mtk_pcie_phy *pcie_phy,
				    unsigned int lane)
{
	struct mtk_pcie_lane_efuse *data = &pcie_phy->efuse[lane];
	void __iomem *addr;

	if (!data->lane_efuse_supported)
		return;

	addr = pcie_phy->sif_base + PEXTP_ANA_LN0_TRX_REG +
	       lane * PEXTP_ANA_LANE_OFFSET;

	mtk_phy_update_field(addr + PEXTP_ANA_TX_REG, EFUSE_LN_TX_PMOS_SEL,
			     data->tx_pmos);

	mtk_phy_update_field(addr + PEXTP_ANA_TX_REG, EFUSE_LN_TX_NMOS_SEL,
			     data->tx_nmos);

	mtk_phy_update_field(addr + PEXTP_ANA_RX_REG, EFUSE_LN_RX_SEL,
			     data->rx_data);
}

/**
 * mtk_pcie_phy_init() - Initialize the phy
 * @phy: the phy to be initialized
 *
 * Initialize the phy by setting the efuse data.
 * The hardware settings will be reset during suspend, it should be
 * reinitialized when the consumer calls phy_init() again on resume.
 */
static int mtk_pcie_phy_init(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int i, ret;

	ret = pm_runtime_get_sync(&phy->dev);
	if (ret < 0)
		goto err_pm_get_sync;

	ret = clk_bulk_prepare_enable(pcie_phy->num_clks, pcie_phy->clks);
	if (ret) {
		dev_info(pcie_phy->dev, "failed to enable clocks\n");
		goto err_pm_get_sync;
	}

	if (pcie_phy->data->phy_init) {
		ret = pcie_phy->data->phy_init(phy);
		if (ret)
			goto err_phy_init;
	}

	if (!pcie_phy->sw_efuse_en)
		return 0;

	/* Set global data */
	mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_GLB_00_REG,
			     EFUSE_GLB_INTR_SEL, pcie_phy->efuse_glb_intr);

	for (i = 0; i < pcie_phy->num_lanes; i++)
		mtk_pcie_efuse_set_lane(pcie_phy, i);

	return 0;

err_phy_init:
	clk_bulk_disable_unprepare(pcie_phy->num_clks, pcie_phy->clks);
err_pm_get_sync:
	pm_runtime_put_sync(&phy->dev);

	return ret;
}

static int mtk_pcie_phy_exit(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	clk_bulk_disable_unprepare(pcie_phy->num_clks, pcie_phy->clks);
	pm_runtime_put_sync(&phy->dev);

	return 0;
}

static int mtk_pcie_phy_set_mode(struct phy *phy, enum phy_mode mode, int subm)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	pcie_phy->mode = mode;

	return 0;
}

/* Set partition when use PCIe PHY debug probe table */
static void mtk_pcie_phy_dbg_set_partition(void __iomem *phy_base, u32 partition)
{
	writel_relaxed(partition, phy_base + PEXTP_DIG_GLB_00);
}

/* Read the PCIe PHY internal signal corresponding to the debug probe table bus */
static u32 mtk_pcie_phy_dbg_read_bus(void __iomem *phy_base, u32 sel, u32 bus)
{
	writel_relaxed(bus, phy_base + sel);
	return readl_relaxed(phy_base + PEXTP_DIG_PROBE_OUT);
}

static int mtk_pcie_monitor_phy(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	void __iomem *sif = pcie_phy->sif_base;
	u32 tbl[11] = {0};

	mtk_pcie_phy_dbg_set_partition(sif, 0x0);
	tbl[0] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0x306);
	tbl[1] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0xc0d);
	tbl[2] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0x1d1e);
	tbl[3] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0x2021);
	tbl[4] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0x2226);
	tbl[5] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0x2f42);
	tbl[6] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_04, 0x4351);

	dev_info(pcie_phy->dev, "PHY misc probe: 0x306=%#x, 0xc0d=%#x, 0x1d1e=%#x, 0x2021=%#x, 0x2226=%#x, 0x2f42=%#x, 0x4351=%#x\n",
		 tbl[0], tbl[1], tbl[2], tbl[3], tbl[4], tbl[5], tbl[6]);

	mtk_pcie_phy_dbg_set_partition(sif, 0x101);
	tbl[0] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_08, 0x8000e);
	tbl[1] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_08, 0x8000e);
	tbl[2] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_08, 0x8000e);
	dev_info(pcie_phy->dev, "PHY ckgen probe: 0x8000e=%#x, 0x8000e=%#x, 0x8000e=%#x\n",
		 tbl[0], tbl[1], tbl[2]);

	/* 0x880089 for polling compliance */
	mtk_pcie_phy_dbg_set_partition(sif, 0x404);
	tbl[0] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x20003);
	tbl[1] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x40011);
	tbl[2] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x120064);
	tbl[3] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x650069);
	tbl[4] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x6f0083);
	tbl[5] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x880089);
	tbl[6] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x8b008e);
	tbl[7] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x8f009c);
	tbl[8] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xa800ad);
	tbl[9] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xba00bb);
	tbl[10] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xbc00bd);
	dev_info(pcie_phy->dev, "PHY ln0 probe: 0x20003=%#x, 0x40011=%#x, 0x120064=%#x, 0x650069=%#x, 0x6f0083=%#x, 0x880089=%#x, 0x8b008e=%#x, 0x8f009c=%#x, 0xa800ad=%#x, 0xba00bb=%#x, 0xbc00bd=%#x\n",
		 tbl[0], tbl[1], tbl[2], tbl[3], tbl[4], tbl[5], tbl[6], tbl[7], tbl[8], tbl[9], tbl[10]);

	mtk_pcie_phy_dbg_set_partition(sif, 0x404);
	tbl[0] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xc000c1);
	tbl[1] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xc200c3);
	tbl[2] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xca00cb);
	tbl[3] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xcc00d3);
	tbl[4] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xd400d5);
	tbl[5] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xd600d7);
	tbl[6] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xd800d9);
	tbl[7] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xda00db);
	tbl[8] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xdc00dd);
	tbl[9] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0xfb00fd);
	tbl[10] = mtk_pcie_phy_dbg_read_bus(sif, PEXTP_DIG_GLB_10, 0x10a013f);
	dev_info(pcie_phy->dev, "PHY ln0 probe: 0xc000c1=%#x, 0xc200c3=%#x, 0xca00cb=%#x, 0xcc00d3=%#x, 0xd400d5=%#x, 0xd600d7=%#x, 0xd800d9=%#x, 0xda00db=%#x, 0xdc00dd=%#x, 0xfb00fd=%#x, 0x10a013f=%#x\n",
		 tbl[0], tbl[1], tbl[2], tbl[3], tbl[4], tbl[5], tbl[6], tbl[7], tbl[8], tbl[9], tbl[10]);

	tbl[0] = readl_relaxed(sif + PEXTP_ANA_GLB_50_REG);
	tbl[1] = readl_relaxed(sif + PEXTP_ANA_GLB_54_REG);
	mtk_phy_update_field(sif + PEXTP_ANA_GLB_14_REG, GLB_TPLL0_DEBUG_SEL, 0x7);
	tbl[2] = readl_relaxed(sif + PEXTP_ANA_GLB_50_REG);
	dev_info(pcie_phy->dev, "ANA_GLB_50 = %#x, ANA_GLB_54 = %#x, PHY Kband = %#x\n",
		 tbl[0], tbl[1], tbl[2]);

	if (pcie_phy->ckm_base) {
		mtk_phy_update_field(pcie_phy->ckm_base + XTP_CKM_DA_REG_3C,
				     RG_CKM_PROBE_SEL, 0x3);
		tbl[0] = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_44);
		tbl[1] = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_44);
		tbl[2] = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_44);
		tbl[3] = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_44);
		tbl[4] = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_44);
		dev_info(pcie_phy->dev, "CKM probe: 3b'011 = %#x, = %#x, = %#x, = %#x, = %#x\n",
			 tbl[0], tbl[1], tbl[2], tbl[3], tbl[4]);
	}

	return 0;
}

static const struct phy_ops mtk_pcie_phy_ops = {
	.init		= mtk_pcie_phy_init,
	.exit		= mtk_pcie_phy_exit,
	.set_mode	= mtk_pcie_phy_set_mode,
	.calibrate	= mtk_pcie_monitor_phy,
	.owner		= THIS_MODULE,
};

static int mtk_pcie_efuse_read_for_lane(struct mtk_pcie_phy *pcie_phy,
					unsigned int lane)
{
	struct mtk_pcie_lane_efuse *efuse = &pcie_phy->efuse[lane];
	struct device *dev = pcie_phy->dev;
	char efuse_id[16];
	int ret;

	ret = snprintf(efuse_id, sizeof(efuse_id), "tx_ln%d_pmos", lane);
	if (ret < 0) {
		dev_info(dev, "Failed to snprintf tx_ln_pmos\n");
		return ret;
	}

	ret = nvmem_cell_read_variable_le_u32(dev, efuse_id, &efuse->tx_pmos);
	if (ret) {
		dev_info(dev, "Failed to read %s\n", efuse_id);
		return ret;
	}

	ret = snprintf(efuse_id, sizeof(efuse_id), "tx_ln%d_nmos", lane);
	if (ret < 0) {
		dev_info(dev, "Failed to snprintf tx_ln_nmos\n");
		return ret;
	}

	ret = nvmem_cell_read_variable_le_u32(dev, efuse_id, &efuse->tx_nmos);
	if (ret) {
		dev_info(dev, "Failed to read %s\n", efuse_id);
		return ret;
	}

	ret = snprintf(efuse_id, sizeof(efuse_id), "rx_ln%d", lane);
	if (ret < 0) {
		dev_info(dev, "Failed to snprintf rx_ln\n");
		return ret;
	}

	ret = nvmem_cell_read_variable_le_u32(dev, efuse_id, &efuse->rx_data);
	if (ret) {
		dev_info(dev, "Failed to read %s\n", efuse_id);
		return ret;
	}

	if (!(efuse->tx_pmos || efuse->tx_nmos || efuse->rx_data)) {
		dev_info(dev,
			 "No eFuse data found for lane%d, but dts enable it\n",
			 lane);
		return -EINVAL;
	}

	efuse->lane_efuse_supported = true;

	return 0;
}

static int mtk_pcie_read_efuse(struct mtk_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	bool nvmem_enabled;
	int ret, i;

	/* nvmem data is optional */
	nvmem_enabled = device_property_present(dev, "nvmem-cells");
	if (!nvmem_enabled)
		return 0;

	ret = nvmem_cell_read_variable_le_u32(dev, "glb_intr",
					      &pcie_phy->efuse_glb_intr);
	if (ret) {
		dev_info(dev, "Failed to read glb_intr\n");
		return ret;
	}

	pcie_phy->sw_efuse_en = true;

	pcie_phy->efuse = devm_kzalloc(dev, pcie_phy->num_lanes *
				       sizeof(*pcie_phy->efuse), GFP_KERNEL);
	if (!pcie_phy->efuse)
		return -ENOMEM;

	for (i = 0; i < pcie_phy->num_lanes; i++) {
		ret = mtk_pcie_efuse_read_for_lane(pcie_phy, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct mtk_pcie_phy *pcie_phy;
	struct resource *ckm_res;
	u32 num_lanes;
	int ret;

	pcie_phy = devm_kzalloc(dev, sizeof(*pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	pcie_phy->sif_base = devm_platform_ioremap_resource_byname(pdev, "sif");
	if (IS_ERR(pcie_phy->sif_base)) {
		dev_info(dev, "Failed to map phy-sif base\n");
		return PTR_ERR(pcie_phy->sif_base);
	}

	ckm_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ckm");
	if (ckm_res) {
		pcie_phy->ckm_base = devm_ioremap_resource(dev, ckm_res);
		if (IS_ERR(pcie_phy->ckm_base)) {
			dev_info(dev, "Failed to map phy-ckm base\n");
			return PTR_ERR(pcie_phy->ckm_base);
		}
	}

	pm_runtime_enable(dev);

	pcie_phy->phy = devm_phy_create(dev, dev->of_node, &mtk_pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		dev_info(dev, "Failed to create PCIe phy\n");
		ret = PTR_ERR(pcie_phy->phy);
		goto err_probe;
	}

	pcie_phy->dev = dev;
	pcie_phy->data = of_device_get_match_data(dev);
	if (!pcie_phy->data) {
		dev_info(dev, "Failed to get phy data\n");
		ret = -EINVAL;
		goto err_probe;
	}

	pcie_phy->num_lanes = 1;
	ret = of_property_read_u32(dev->of_node, "num-lanes", &num_lanes);
	if (!ret)
		pcie_phy->num_lanes = num_lanes;

	ret = of_property_read_bool(dev->of_node, "mediatek,short-reach");
	if (ret)
		pcie_phy->short_reach_en = true;

	ret = of_property_read_bool(dev->of_node, "mediatek,disable-ssc");
	if (ret)
		pcie_phy->disable_ssc = true;

	if (pcie_phy->data->sw_efuse_supported) {
		/*
		 * Failed to read the efuse data is not a fatal problem,
		 * ignore the failure and keep going.
		 */
		ret = mtk_pcie_read_efuse(pcie_phy);
		if (ret == -EPROBE_DEFER || ret == -ENOMEM)
			goto err_probe;
	}

	phy_set_drvdata(pcie_phy->phy, pcie_phy);

	if (pcie_phy->data->get_efuse_info) {
		ret = pcie_phy->data->get_efuse_info(pcie_phy->phy);
		if (ret == -EPROBE_DEFER || ret == -ENOMEM)
			goto err_probe;
	}

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_info(dev, "PCIe phy probe failed\n");
		ret = PTR_ERR(provider);
		goto err_probe;
	}

	pcie_phy->num_clks = devm_clk_bulk_get_all(dev, &pcie_phy->clks);
	if (pcie_phy->num_clks < 0) {
		dev_info(dev, "failed to get clocks\n");
		ret = pcie_phy->num_clks;
		goto err_probe;
	}

	return 0;

err_probe:
	pm_runtime_disable(dev);
	kfree(pcie_phy->efuse_info);

	return ret;
}

static int mtk_pcie_phy_init_6985(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	struct device *dev = pcie_phy->dev;
	u32 val;

	if (!pcie_phy->ckm_base) {
		dev_info(dev, "phy-ckm base is null\n");
		return -EINVAL;
	}

	val = readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_28);
	val |= RG_XTP_PHY_CLKREQ_N_IN;
	writel_relaxed(val, pcie_phy->sif_base + PEXTP_DIG_GLB_28);

	val = readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_50);
	val &= ~RG_XTP_CKM_EN_L1S0;
	writel_relaxed(val, pcie_phy->sif_base + PEXTP_DIG_GLB_50);

	val = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_3C);
	val |= RG_CKM_PADCK_REQ;
	writel_relaxed(val, pcie_phy->ckm_base + XTP_CKM_DA_REG_3C);

	dev_info(dev, "PHY GLB_28=%#x, GLB_50=%#x, CKM_3C=%#x\n",
		 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_28),
		 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_50),
		 readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_3C));

	return 0;
}

static int mtk_pcie_phy_init_6989(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	u32 val;

	val = readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_20);
	val &= ~RG_XTP_BYPASS_PIPE_RST;
	writel_relaxed(val, pcie_phy->sif_base + PEXTP_DIG_GLB_20);

	return 0;
}

static int mtk_pcie_sr_init_6991(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	u32 i;

	for (i = 0; i < pcie_phy->num_lanes; i++) {
		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN0_TRX_REG +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_RESERVE,
				     LN_TX_RESERVE_TO_8);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_6C +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_AEQ_CTLE_ERR_TYPE,
				     AEQ_CTLE_ERR_TYPE_H15);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX2_A4 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_AEQ_OFORCE_GEN3,
				     AEQ_OFORCE_GEN3_TO_7F);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX2_A4 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_AEQ_OFORCE_GEN4,
				     AEQ_OFORCE_GEN4_TO_7F);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_TRX_74 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_FRC_RX_AEQ_DFETP1);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_TRX_74 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_FRC_RX_AEQ_DFETP2);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_TRX_74 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_FRC_RX_AEQ_DFETP3);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_TRX_70 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_FRC_RX_AEQ_DFETP4);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_TRX_70 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_FRC_RX_AEQ_DFETP5);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_RX2_94 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_RX_CDR_DLY_OFF_GEN3);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_LN_RX2_94 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_RX_CDR_DLY_OFF_GEN4);
	}

	return 0;
}

static int mtk_pcie_get_efuse_info(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	struct device *dev = pcie_phy->dev;
	struct nvmem_cell *cell;
	bool nvmem_enabled;
	size_t len = 0;
	u32 *efuse_buff;

	/* nvmem data is optional */
	nvmem_enabled = device_property_present(dev, "nvmem-cells");
	if (!nvmem_enabled)
		return 0;

	cell = nvmem_cell_get(dev, "calibration");
	if (IS_ERR(cell)) {
		dev_info(dev, "Failed to get calibration nvmem cell\n");
		return PTR_ERR(cell);
	}

	efuse_buff = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(efuse_buff)) {
		dev_info(dev, "Failed to read calibration nvmem cell\n");
		return PTR_ERR(efuse_buff);
	}

	pcie_phy->efuse_info = efuse_buff;
	pcie_phy->efuse_info_len = len;

	return 0;
}

static int mtk_pcie_sphy3_calibrate(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	struct device *dev = pcie_phy->dev;
	u32 i;

	/* Efuse info is null or chip without calibrated data */
	if (!pcie_phy->efuse_info || !pcie_phy->efuse_info_len || !pcie_phy->efuse_info[0])
		goto no_efuse_info;

	/* Current SPHY3 efuse architecture only support 1 or 2 lane */
	if (pcie_phy->num_lanes > SPHY3_EFUSE_MAX_LANE) {
		dev_info(dev, "The number of lanes %d out of range\n", pcie_phy->num_lanes);
		goto no_efuse_info;
	}

	if ((pcie_phy->num_lanes + 1) * sizeof(u32) > pcie_phy->efuse_info_len) {
		dev_info(dev, "efuse info length = %zu error\n", pcie_phy->efuse_info_len);
		goto no_efuse_info;
	}

	mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_GLB_60,
			     RG_XTP_GLB_BIAS_INTR_CTRL,
			     FIELD_GET(EFUSE_GLB_BIAS_INTR_CTRL, pcie_phy->efuse_info[0]));

	mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_GLB_C0,
			     RG_XTP_GLB_BIAS_V2V_VTRIM,
			     FIELD_GET(EFUSE_GLB_BIAS_V2V_VTRIM, pcie_phy->efuse_info[0]));

	mtk_phy_update_field(pcie_phy->ckm_base + XTP_CKM_DA_REG_D4,
			     RG_CKM_CKTX_IMPSEL_PMOS,
			     FIELD_GET(EFUSE_CKM_CKTX_IMPSEL_PMOS, pcie_phy->efuse_info[0]));

	mtk_phy_update_field(pcie_phy->ckm_base + XTP_CKM_DA_REG_D4,
			     RG_CKM_CKTX_IMPSEL_NMOS,
			     FIELD_GET(EFUSE_CKM_CKTX_IMPSEL_NMOS, pcie_phy->efuse_info[0]));

	mtk_phy_update_field(pcie_phy->ckm_base + XTP_CKM_DA_REG_D4,
			     RG_CKM_CKTX_IMPSEL_SW,
			     FIELD_GET(EFUSE_CKM_CKTX_IMPSEL_RMID, pcie_phy->efuse_info[0]));

	for (i = 0; i < pcie_phy->num_lanes; i++) {
		if (i)
			mtk_phy_update_field(pcie_phy->sif_base +
					     PEXTP_ANA_LN_TRX_C +
					     i * PEXTP_ANA_LANE_OFFSET,
					     RG_XTP_LN_TX_RSWN_IMPSEL,
					     FIELD_GET(EFUSE_LN1_TX_RSWN_IMPSEL, pcie_phy->efuse_info[0]));
		else
			mtk_phy_update_field(pcie_phy->sif_base +
					     PEXTP_ANA_LN_TRX_C +
					     i * PEXTP_ANA_LANE_OFFSET,
					     RG_XTP_LN_TX_RSWN_IMPSEL,
					     FIELD_GET(EFUSE_LN0_TX_RSWN_IMPSEL, pcie_phy->efuse_info[0]));

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A8 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_LEQ_RL_CTLE_CAL,
				     FIELD_GET(EFUSE_LN_RX_LEQ_RL_CTLE_CAL, pcie_phy->efuse_info[i+1]));

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A8 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_LEQ_RL_VGA_CAL,
				     FIELD_GET(EFUSE_LN_RX_LEQ_RL_VGA_CAL, pcie_phy->efuse_info[i+1]));

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A8 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_LEQ_RL_DFE_CAL,
				     FIELD_GET(EFUSE_LN_RX_LEQ_RL_DEF_CAL, pcie_phy->efuse_info[i+1]));

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_IMPSEL,
				     FIELD_GET(EFUSE_LN_RX_IMPSEl, pcie_phy->efuse_info[i+1]));

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_IMPSEL_PMOS,
				     FIELD_GET(EFUSE_LN_TX_IMPSEL_PMOS, pcie_phy->efuse_info[i+1]));

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_IMPSEL_NMOS,
				     FIELD_GET(EFUSE_LN_TX_IMPSEL_NMOS, pcie_phy->efuse_info[i+1]));
	}

	dev_info(dev, "Calibration successful\n");

	return 0;

no_efuse_info:
	dev_info(dev, "No calibration info\n");

	/* To prevent potential EM problem, apply this setting
	 * if no Efuse calibration
	 */
	for (i = 0; i < pcie_phy->num_lanes; i++) {
		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_IMPSEL_PMOS,
				     TX_IMPSEL_PMOS_TO_A);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_A0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_IMPSEL_NMOS,
				     TX_IMPSEL_NMOS_TO_9);
	}

	return 0;
}

static int mtk_pcie_phy_init_6991(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	struct device *dev = pcie_phy->dev;
	int ret = 0;
	u32 i;

	if (!pcie_phy->ckm_base) {
		dev_info(dev, "phy-ckm base is null\n");
		return -EINVAL;
	}

	/* Switch PHY mode for port1, only used by EP mode */
	if (pcie_phy->mode == PHY_MODE_PCIE) {
		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_DIG_GLB_28,
				 RG_XTP_PCIE_MODE);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_GLB_10_REG,
				     GLB_TPLL0_RST_DLY,
				     RESET_COUNTER_SELECT_2);

		for (i = 0; i < pcie_phy->num_lanes; i++) {
			mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_TPLL0_78 +
					     i * PEXTP_ANA_LANE_OFFSET,
					     RG_XTP_VCO_CFIX_EN_GEN1,
					     HIGH_VCO_FREQ);

			mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_TPLL0_78 +
					     i * PEXTP_ANA_LANE_OFFSET,
					     RG_XTP_VCO_CFIX_EN_GEN2,
					     HIGH_VCO_FREQ);
		}

		dev_info(dev, "PHY GLB_28=%#x, ANA_GLB_10==%#x",
			 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_28),
			 readl_relaxed(pcie_phy->sif_base + PEXTP_ANA_GLB_10_REG));
	} else {
		/* RC mode need adjust PHY sequence to fix L1.2 issue */
		mtk_phy_update_field(pcie_phy->ckm_base + XTP_CKM_DA_REG_38,
				     RG_CKM_BIAS_WAIT_PRD,
				     CKM_BIAS_WAIT_PRD_TO_4US);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_GLB_38,
				     RG_XTP_TPLL_SET_STB_T_SEL,
				     TPLL_SET_STB_T_SEL_TO_3F);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_GLB_38,
				     RG_XTP_TPLL_PWE_ON_STB_T_SEL,
				     TPLL_PWE_ON_STB_T_SEL_TO_3);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_GLB_F4,
				     RG_XTP_TPLL_ISO_EN_STB_T_SEL,
				     TPLL_ISO_EN_STB_T_SEL_TO_3);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_GLB_30,
				     RG_XTP_CKBG_STAL_STB_T_SEL,
				     CKBG_STAL_STB_T_SEL_TO_0);

		mtk_phy_clear_bits(pcie_phy->sif_base + PEXTP_DIG_GLB_50,
				   RG_XTP_CKM_EN_L1S1);

		/* not bypass pipe reset, pipe reset will reset TPLL */
		mtk_phy_clear_bits(pcie_phy->sif_base + PEXTP_DIG_GLB_20, RG_XTP_BYPASS_PIPE_RST_RC);

		dev_info(dev, "CKM_38=%#x, GLB_20=%#x, GLB_30=%#x, GLB_38=%#x, GLB_50=%#x, GLB_F4=%#x\n",
			 readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_38),
			 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_20),
			 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_30),
			 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_38),
			 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_50),
			 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_F4));
	}

	mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_GLB_2C,
			     RG_XTP_GLB_TPLL1_RESERVE_0,
			     TPLL1_P_PATH_GAIN_TO_05);

	for (i = 0; i < pcie_phy->num_lanes; i++) {
		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_6C +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_AEQ_CTLE_ERR_TYPE,
				     AEQ_CTLE_ERR_TYPE_H15_H25);

		mtk_phy_set_bits(pcie_phy->sif_base + PEXTP_ANA_LN_TRX_34 +
				 i * PEXTP_ANA_LANE_OFFSET,
				 RG_XTP_LN_RX_FE);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TRX_E8 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_LF_CTLE_CSEL_GEN4,
				     CTLE_CSEL_GEN4_TO_1);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX_F0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_GEN1_CTLE1_CSEL,
				     GEN1_CTLE1_CSEL_TO_D);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX_F0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_GEN2_CTLE1_CSEL,
				     GEN2_CTLE1_CSEL_TO_D);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX_F0 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_GEN3_CTLE1_CSEL,
				     GEN3_CTLE1_CSEL_TO_D);


		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX2_04 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_AEQ_EGEQ_RATIO_GEN3,
				     AEQ_EGEQ_RATIO_GEN3_TO_22);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_RX2_04 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_RX_AEQ_EGEQ_RATIO_GEN4,
				     AEQ_EGEQ_RATIO_GEN4_TO_22);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_4 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_C0,
				     MGX_PX_C0_TO_A);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_4 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_CP1,
				     MGX_PX_CP1_TO_2);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_8 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_C0,
				     MGX_PX_C0_TO_B);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_8 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_CP1,
				     MGX_PX_CP1_TO_1);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_C +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_C0,
				     MGX_PX_C0_TO_C);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_10 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_C0,
				     MGX_PX_C0_TO_B);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_10 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_CM1,
				     MGX_PX_CM1_TO_1);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_14 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_C0,
				     MGX_PX_C0_TO_B);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_14 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_CM1,
				     MGX_PX_CM1_TO_1);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_18 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_C0,
				     MGX_PX_C0_TO_A);

		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_LN_TX_RSWN_18 +
				     i * PEXTP_ANA_LANE_OFFSET,
				     RG_XTP_LN_TX_MGX_PX_CM1,
				     MGX_PX_CM1_TO_2);
	}

	ret = mtk_pcie_sphy3_calibrate(phy);
	if (ret < 0)
		return ret;

	if (pcie_phy->disable_ssc)
		mtk_phy_update_field(pcie_phy->sif_base + PEXTP_DIG_TPLL0_6C,
				     RG_XTP_GLB_TPLL0_SDM_SSC_CTL, DISABLE_SSC);

	if (pcie_phy->short_reach_en)
		return mtk_pcie_sr_init_6991(phy);

	return 0;
}

static const struct mtk_pcie_phy_data mt8195_data = {
	.sw_efuse_supported = true,
};

static const struct mtk_pcie_phy_data mt6985_data = {
	.sw_efuse_supported = false,
	.phy_init = mtk_pcie_phy_init_6985,
};

static const struct mtk_pcie_phy_data mt6989_data = {
	.sw_efuse_supported = false,
	.phy_init = mtk_pcie_phy_init_6989,
};

static const struct mtk_pcie_phy_data mt6991_data = {
	.sw_efuse_supported = false,
	.phy_init = mtk_pcie_phy_init_6991,
	.get_efuse_info = mtk_pcie_get_efuse_info,
};

static const struct of_device_id mtk_pcie_phy_of_match[] = {
	{ .compatible = "mediatek,mt8195-pcie-phy", .data = &mt8195_data },
	{ .compatible = "mediatek,mt6985-pcie-phy", .data = &mt6985_data },
	{ .compatible = "mediatek,mt6989-pcie-phy", .data = &mt6989_data },
	{ .compatible = "mediatek,mt6991-pcie-phy", .data = &mt6991_data },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_pcie_phy_of_match);

static struct platform_driver mtk_pcie_phy_driver = {
	.probe	= mtk_pcie_phy_probe,
	.driver	= {
		.name = "mtk-pcie-phy",
		.of_match_table = mtk_pcie_phy_of_match,
	},
};
module_platform_driver(mtk_pcie_phy_driver);

MODULE_DESCRIPTION("MediaTek PCIe PHY driver");
MODULE_AUTHOR("Jianjun Wang <jianjun.wang@mediatek.com>");
MODULE_LICENSE("GPL");
