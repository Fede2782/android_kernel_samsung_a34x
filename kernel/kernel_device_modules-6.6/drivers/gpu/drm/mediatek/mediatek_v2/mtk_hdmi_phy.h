/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_HDMI_PHY_H
#define _MTK_HDMI_PHY_H

#include <linux/types.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include "mtk_hdmi.h"

enum mtk_hdmi_phy_clk_id {
	MTK_HDMI_PHY_XTAL_SEL,
	MTK_HDMI_PHY_CLK_COUNT,
};

enum txpll_clk_parent {
	PLLGP1_PLLGP_2H_PLL2,
	HDMIRX,
	XTAL26M,
	TXPLL_CLK_PARENT_COUNT,
};

struct mtk_hdmi_phy_data {
	const bool detect_clock_channel_only;
	const bool power_control_available;
	const bool efuse_sw_mode;
};

struct mtk_hdmi_phy {
	void __iomem *regs;
	struct device *dev;
	struct clk *clk[MTK_HDMI_PHY_CLK_COUNT];
	struct clk *txpll;
	struct mtk_hdmi_phy_data *phy_data;
	struct clk_hw txpll_hw;
	unsigned long txpll_rate;
	enum txpll_clk_parent clk_parent;
	unsigned long rx_clk_rate;
	unsigned long min_tmds_clock;
	unsigned long max_tmds_clock;
	unsigned int efuse;
};

extern struct mtk_hdmi_phy *global_hdmi_phy;

void vTxSignalOnOff(struct mtk_hdmi_phy *hdmi_phy, bool OnOff);
void mtk_hdmi_ana_fifo_en(struct mtk_hdmi_phy *hdmi_phy);
void mtk_tmds_high_bit_clk_ratio(
	struct mtk_hdmi *hdmi, bool enable);

unsigned int mtk_hdmi_phy_read(
	struct mtk_hdmi_phy *hdmi_phy, unsigned short reg);
void mtk_hdmi_phy_write(struct mtk_hdmi_phy *hdmi_phy,
	unsigned short reg, unsigned int val);
void mtk_hdmi_power_control(struct mtk_hdmi_phy *hdmi_phy, bool enable);


#endif
