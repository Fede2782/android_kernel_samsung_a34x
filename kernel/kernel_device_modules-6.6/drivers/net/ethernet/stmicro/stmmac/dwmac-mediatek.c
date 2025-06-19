// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */
#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/stmmac.h>

#include "mtk_sgmii_core.h"
#include "stmmac.h"
#include "stmmac_platform.h"

/* Peri Configuration register for mt2712 */
#define PERI_ETH_PHY_INTF_SEL	0x418
#define PHY_INTF_MII		0
#define PHY_INTF_RGMII		1
#define PHY_INTF_RMII		4
#define RMII_CLK_SRC_RXC	BIT(4)
#define RMII_CLK_SRC_INTERNAL	BIT(5)

#define PERI_ETH_DLY	0x428
#define ETH_DLY_GTXC_INV	BIT(6)
#define ETH_DLY_GTXC_ENABLE	BIT(5)
#define ETH_DLY_GTXC_STAGES	GENMASK(4, 0)
#define ETH_DLY_TXC_INV		BIT(20)
#define ETH_DLY_TXC_ENABLE	BIT(19)
#define ETH_DLY_TXC_STAGES	GENMASK(18, 14)
#define ETH_DLY_RXC_INV		BIT(13)
#define ETH_DLY_RXC_ENABLE	BIT(12)
#define ETH_DLY_RXC_STAGES	GENMASK(11, 7)

#define PERI_ETH_DLY_FINE	0x800
#define ETH_RMII_DLY_TX_INV	BIT(2)
#define ETH_FINE_DLY_GTXC	BIT(1)
#define ETH_FINE_DLY_RXC	BIT(0)

/* Peri Configuration register for mt8195 */
#define MT8195_PERI_ETH_CTRL0		0xFD0
#define MT8195_RMII_CLK_SRC_INTERNAL	BIT(28)
#define MT8195_RMII_CLK_SRC_RXC		BIT(27)
#define MT8195_ETH_INTF_SEL		GENMASK(26, 24)
#define MT8195_RGMII_TXC_PHASE_CTRL	BIT(22)
#define MT8195_EXT_PHY_MODE		BIT(21)
#define MT8195_DLY_GTXC_INV		BIT(12)
#define MT8195_DLY_GTXC_ENABLE		BIT(5)
#define MT8195_DLY_GTXC_STAGES		GENMASK(4, 0)

#define MT8195_PERI_ETH_CTRL1		0xFD4
#define MT8195_DLY_RXC_INV		BIT(25)
#define MT8195_DLY_RXC_ENABLE		BIT(18)
#define MT8195_DLY_RXC_STAGES		GENMASK(17, 13)
#define MT8195_DLY_TXC_INV		BIT(12)
#define MT8195_DLY_TXC_ENABLE		BIT(5)
#define MT8195_DLY_TXC_STAGES		GENMASK(4, 0)

#define MT8195_PERI_ETH_CTRL2		0xFD8
#define MT8195_DLY_RMII_RXC_INV		BIT(25)
#define MT8195_DLY_RMII_RXC_ENABLE	BIT(18)
#define MT8195_DLY_RMII_RXC_STAGES	GENMASK(17, 13)
#define MT8195_DLY_RMII_TXC_INV		BIT(12)
#define MT8195_DLY_RMII_TXC_ENABLE	BIT(5)
#define MT8195_DLY_RMII_TXC_STAGES	GENMASK(4, 0)

/* Peri Configuration register for mt8678 mac0 */
#define MT8678_MAC0_PERI_ETH_CTRL0	0x300
#define MT8678_MAC0_PERI_ETH_CTRL1	0x304
#define MT8678_MAC0_PERI_ETH_CTRL2	0x308

/* Peri Configuration register for mt8678 mac1 */
#define MT8678_MAC1_PERI_ETH_CTRL0	0x310
#define MT8678_MAC1_PERI_ETH_CTRL1	0x314
#define MT8678_MAC1_PERI_ETH_CTRL2	0x318

#define MT8678_SGMII_CLK_INTERNAL	BIT(29)
#define MT8678_RMII_CLK_SRC_INTERNAL	BIT(28)
#define MT8678_RMII_CLK_SRC_RXC		BIT(27)
#define MT8678_ETH_INTF_SEL		GENMASK(26, 24)
#define MT8678_RGMII_TXC_PHASE_CTRL	BIT(22)
#define MT8678_EXT_PHY_MODE		BIT(21)
#define MT8678_TXC_OUT_OP		BIT(20)
#define MT8678_DLY_GTXC_INV		BIT(12)
#define MT8678_DLY_GTXC_ENABLE		BIT(5)
#define MT8678_DLY_GTXC_STAGES		GENMASK(4, 0)

#define MT8678_DLY_RXC_INV		BIT(25)
#define MT8678_DLY_RXC_ENABLE		BIT(18)
#define MT8678_DLY_RXC_STAGES		GENMASK(17, 13)
#define MT8678_DLY_TXC_INV		BIT(12)
#define MT8678_DLY_TXC_ENABLE		BIT(5)
#define MT8678_DLY_TXC_STAGES		GENMASK(4, 0)

#define MT8678_DLY_RMII_RXC_INV		BIT(25)
#define MT8678_DLY_RMII_RXC_ENABLE	BIT(18)
#define MT8678_DLY_RMII_RXC_STAGES	GENMASK(17, 13)
#define MT8678_DLY_RMII_TXC_INV		BIT(12)
#define MT8678_DLY_RMII_TXC_ENABLE	BIT(5)
#define MT8678_DLY_RMII_TXC_STAGES	GENMASK(4, 0)

struct mac_delay_struct {
	u32 tx_delay;
	u32 rx_delay;
	bool tx_inv;
	bool rx_inv;
};

struct chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

struct mediatek_dwmac_plat_data {
	const struct mediatek_dwmac_variant *variant;
	struct mac_delay_struct mac_delay;
	struct clk *rmii_internal_clk;
	struct clk *sgmii_sel_clk;
	struct clk *sgmii_sbus_sel_clk;
	struct clk_bulk_data *clks;
	struct regmap *peri_regmap;
	struct device_node *np;
	struct device *dev;
	struct mtk_sgmii *sgmii;
	phy_interface_t phy_mode;
	bool rmii_clk_from_mac;
	bool rmii_rxc;
	bool mac_wol;
};

struct mediatek_dwmac_variant {
	int (*dwmac_set_phy_interface)(struct mediatek_dwmac_plat_data *plat);
	int (*dwmac_set_delay)(struct mediatek_dwmac_plat_data *plat);
	int (*dwmac_set_base_addr)(struct platform_device *pdev,
				   struct stmmac_resources *stmmac_res);
	/* clock ids to be requested */
	const char * const *clk_list;
	int num_clks;

	u32 dma_bit_mask;
	u32 rx_delay_max;
	u32 tx_delay_max;
};

/* list of clocks required for mac */
static const char * const mt2712_dwmac_clk_l[] = {
	"axi", "apb", "mac_main", "ptp_ref"
};

static const char * const mt8195_dwmac_clk_l[] = {
	"axi", "apb", "mac_cg", "mac_main", "ptp_ref"
};

static const char * const mt8678_dwmac_clk_l[] = {
	"mac_main", "ptp_ref"
};

#ifdef CONFIG_DEBUG_FS
static int eth_smt_result = 2;

static struct sk_buff *get_skb(struct stmmac_priv *priv,
			       u32 payload_len,
			       u16 queue_id)
{
	struct sk_buff *skb = NULL;
	unsigned char *ppayload;
	struct ethhdr *ehdr;
	u32 size;
	unsigned char bc_addr[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	size = payload_len + sizeof(struct ethhdr);

	skb = netdev_alloc_skb(priv->dev, size);
	if (!skb)
		return NULL;

	ehdr = skb_push(skb, ETH_HLEN);
	prefetchw(skb->data);

	skb_reset_mac_header(skb);
	ether_addr_copy(ehdr->h_source, priv->dev->dev_addr);
	ether_addr_copy(ehdr->h_dest, bc_addr);

	ehdr->h_proto = htons(ETH_P_TSN);

	ppayload = skb_put(skb, payload_len);

	get_random_bytes(ppayload, payload_len);

	skb->protocol = htons(ETH_P_TSN);
	skb->pkt_type = PACKET_HOST;
	skb->dev = priv->dev;

	return skb;
}

static bool __loopback_test(struct stmmac_priv *priv, int num, int delay,
			    bool log, u32 frame_size)
{
	const struct net_device_ops *netdev_ops = priv->dev->netdev_ops;
	unsigned int rx_framecount_gb, rx_octetcount_gb, rx_octetcount_g;
	unsigned int tx_octetcount_gb, tx_framecount_gb;
	struct sk_buff *skb = NULL;
	int i, j, cpu;
	bool ret;

	priv->mmc.mmc_tx_octetcount_gb += readl(priv->mmcaddr + 0x14);
	priv->mmc.mmc_tx_framecount_gb += readl(priv->mmcaddr + 0x18);
	priv->mmc.mmc_rx_framecount_gb += readl(priv->mmcaddr + 0x80);
	priv->mmc.mmc_rx_octetcount_gb += readl(priv->mmcaddr + 0x84);
	priv->mmc.mmc_rx_octetcount_g += readl(priv->mmcaddr + 0x88);
	tx_octetcount_gb = priv->mmc.mmc_tx_octetcount_gb;
	tx_framecount_gb = priv->mmc.mmc_tx_framecount_gb;
	rx_framecount_gb = priv->mmc.mmc_rx_framecount_gb;
	rx_octetcount_gb = priv->mmc.mmc_rx_octetcount_gb;
	rx_octetcount_g = priv->mmc.mmc_rx_octetcount_g;
	usleep_range(delay, delay + 100);
	for (j = 0; j < priv->plat->tx_queues_to_use; j++) {
		for (i = 0; i < num; i++) {
			struct netdev_queue *txq =
					netdev_get_tx_queue(priv->dev, j);
			skb = get_skb(priv, frame_size, j);
			if (!skb) {
				pr_err("alloc skb failed\n");
				return false;
			}
			cpu = get_cpu();
			do {
				/* Disable soft irqs for various locks below.
				 * Also stops preemption for RCU.
				 * avoid dql bug-on issue.
				 */
				rcu_read_lock_bh();
				HARD_TX_LOCK(priv->dev, txq, cpu);
				ret = netdev_ops->ndo_start_xmit(skb,
								 priv->dev);
				if (ret == NETDEV_TX_OK)
					txq_trans_update(txq);
				HARD_TX_UNLOCK(priv->dev, txq);
				rcu_read_unlock_bh();
			} while (ret != NETDEV_TX_OK);
			put_cpu();
		}
	}

	usleep_range(delay, delay + 100);
	priv->mmc.mmc_tx_octetcount_gb += readl(priv->mmcaddr + 0x14);
	priv->mmc.mmc_tx_framecount_gb += readl(priv->mmcaddr + 0x18);
	priv->mmc.mmc_rx_framecount_gb += readl(priv->mmcaddr + 0x80);
	priv->mmc.mmc_rx_octetcount_gb += readl(priv->mmcaddr + 0x84);
	priv->mmc.mmc_rx_octetcount_g += readl(priv->mmcaddr + 0x88);
	tx_octetcount_gb = priv->mmc.mmc_tx_octetcount_gb - tx_octetcount_gb;
	tx_framecount_gb = priv->mmc.mmc_tx_framecount_gb - tx_framecount_gb;
	rx_framecount_gb = priv->mmc.mmc_rx_framecount_gb - rx_framecount_gb;
	rx_octetcount_gb = priv->mmc.mmc_rx_octetcount_gb - rx_octetcount_gb;
	rx_octetcount_g = priv->mmc.mmc_rx_octetcount_g - rx_octetcount_g;

	if (tx_framecount_gb == rx_framecount_gb &&
	    tx_octetcount_gb == rx_octetcount_gb &&
	    rx_octetcount_gb == rx_octetcount_g &&
	    tx_framecount_gb == (priv->plat->tx_queues_to_use * num))
		ret = true;
	else
		ret = false;

	if (log) {
		pr_info("loop back %s:\n", ret ? "success" : "fail");
		pr_info("tx_queues:%d\t pkt_num_per_queue:%d\n",
			priv->plat->tx_queues_to_use, num);
		pr_info("tx_framecount_gb:%u\t tx_octetcount_gb:%u\n",
			tx_framecount_gb, tx_octetcount_gb);
		pr_info("rx_framecount_gb:%u\t rx_octetcount_gb:%u, rx_octetcount_g:%u\n",
			rx_framecount_gb, rx_octetcount_gb, rx_octetcount_g);
	}

	return ret;
}

static int mmd_write(struct stmmac_priv *priv, int phy_addr,
		     int dev_addr, int reg, int data)
{
	priv->mii->write(priv->mii, phy_addr,
			 0xd, dev_addr);

	priv->mii->write(priv->mii, phy_addr,
			 0xe, reg);

	priv->mii->write(priv->mii, phy_addr,
			 0xd, (1 << 14) | dev_addr);

	priv->mii->write(priv->mii, phy_addr,
			 0xe, data);
	return 0;
}

static int mmd_read(struct stmmac_priv *priv, int phy_addr, int dev_addr, int reg)
{
	int origin;

	priv->mii->write(priv->mii, phy_addr,
			 0xd, dev_addr);

	priv->mii->write(priv->mii, phy_addr,
			 0xe, reg);

	priv->mii->write(priv->mii, phy_addr,
			 0xd, (1 << 14) | dev_addr);

	origin = priv->mii->read(priv->mii, phy_addr, 0xe);

	return origin;
}

static ssize_t stmmac_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(netdev);
	int len = 0;
	int buf_len = (int)PAGE_SIZE;
	struct phy_device *phy_dev = of_phy_find_device(priv->plat->phy_node);

	if (!phy_dev) {
		dev_err(dev, "phy_dev is NULL");
		return -ENODEV;
	}
	len += snprintf(buf + len, buf_len - len,
			"stmmac debug commands: in hexadecimal notation\n");
	len += snprintf(buf + len, buf_len - len,
			"mac read: echo er reg_start reg_range > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"mac write: echo ew reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause22 read: echo cl22r phy_reg > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause22 write: echo cl22w phy_reg value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause45 read: echo cl45r dev_id reg_addr > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause45 write: echo cl45w dev_id reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"reg read: echo rr reg_addr > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"reg write: echo wr reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"phy addr:%d\n", phy_dev->mdio.addr);
	len += snprintf(buf + len, buf_len - len,
			"eth_smt_result:%d\n", eth_smt_result);

	return len;
}

static ssize_t stmmac_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(netdev);
	void __iomem *tmp_addr;
	int reg, data, devid, origin, i, reg_addr, phy_addr, dev_addr;
	struct phy_device *phy_dev = of_phy_find_device(priv->plat->phy_node);
	struct mediatek_dwmac_plat_data *priv_plat = priv->plat->bsp_priv;

	if (!phy_dev) {
		dev_err(dev, "phy_dev is NULL");
		return -ENODEV;
	}

	if (!strncmp(buf, "er", 2) &&
	    (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		for (i = 0; i < data / 0x10 + 1; i++) {
			dev_info(dev,
				 "%08x:\t%08x\t%08x\t%08x\t%08x\t\n",
				 reg + i * 16,
				 readl(priv->ioaddr + reg + i * 0x10),
				 readl(priv->ioaddr + reg + i * 0x10 + 0x4),
				 readl(priv->ioaddr + reg + i * 0x10 + 0x8),
				 readl(priv->ioaddr + reg + i * 0x10 + 0xc));
		}
	} else if (!strncmp(buf, "ew", 2) &&
		   (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		origin = readl(priv->ioaddr + reg);
		writel(data, priv->ioaddr + reg);
		dev_info(dev, "mac reg%#x, value:%#x -> %#x\n",
			 reg, origin, readl(priv->ioaddr + reg));
	} else if (!strncmp(buf, "smt", 3)) {
		eth_smt_result = 0;

		if (!phy_dev) {
			dev_info(dev, "null phy_dev\n");
			goto out;
		}

		if (priv->mii->read(priv->mii, phy_dev->mdio.addr, 2) == 0x1c &&
		    priv->mii->read(priv->mii, phy_dev->mdio.addr, 3) == 0xc916)
			eth_smt_result = 1;
		dev_info(dev, "eth_smt_result:%d\n", eth_smt_result);
	} else if (!strncmp(buf, "cl22r", 5)) {
		if (sscanf(buf + 5, "%x %x", &phy_addr, &reg) == 2) {
			dev_info(dev, "cl22 phyaddr:%#x, reg:%#x, value:%#x\n",
				 phy_addr,
				 reg,
				 mdiobus_read(priv->mii, phy_addr, reg));
		} else if (sscanf(buf + 5, "%x", &reg) == 1) {
			if (!phy_dev) {
				dev_info(dev, "null phy_dev\n");
				goto out;
			}

			dev_info(dev, "cl22 phyaddr:%#x, reg:%#x, value:%#x\n",
				 phy_dev->mdio.addr,
				 reg,
				 mdiobus_read(priv->mii,
					      phy_dev->mdio.addr,
					      reg));
		} else {
			dev_info(dev, "format error\n");
		}
	} else if (!strncmp(buf, "cl22w", 5)) {
		if (sscanf(buf + 5, "%x %x %x", &phy_addr, &reg, &data) == 3) {
			origin = mdiobus_read(priv->mii, phy_addr, reg);
			mdiobus_write(priv->mii, phy_addr, reg, data);
			dev_info(dev, "cl22 phyaddr:%#x, reg:%#x, %#x -> %#x\n",
				 phy_addr,
				 reg,
				 origin,
				 mdiobus_read(priv->mii, phy_addr, reg));
		} else if (sscanf(buf + 5, "%x %x", &reg, &data) == 2) {
			if (!phy_dev) {
				dev_info(dev, "null phy_dev\n");
				goto out;
			}

			origin = mdiobus_read(priv->mii, phy_dev->mdio.addr, reg);
			mdiobus_write(priv->mii, phy_dev->mdio.addr, reg, data);
			dev_info(dev, "cl22 phyaddr:%#x, reg:%#x, %#x -> %#x\n",
				 phy_dev->mdio.addr,
				 reg,
				 origin,
				 mdiobus_read(priv->mii,
					      phy_dev->mdio.addr,
					      reg));
		} else {
			dev_info(dev, "format error\n");
		}
	} else if (!strncmp(buf, "cl45r", 5)) {
		if (sscanf(buf + 5, "%x %x %x", &phy_addr, &devid, &reg) == 3) {
			reg_addr = (1 << 30) |
				   ((devid & 0x1f) << 16) |
				   (reg & 0xffff);
			dev_info(dev, "cl45 phyaddr:%#x, reg:%#x-%#x, %#x\n",
				 phy_addr, devid, reg,
				 mdiobus_read(priv->mii, phy_addr, reg_addr));
		} else if (sscanf(buf + 5, "%x %x", &devid, &reg) == 2) {
			if (!phy_dev) {
				dev_info(dev, "null phy_dev\n");
				goto out;
			}

			reg_addr = (1 << 30) |
				   ((devid & 0x1f) << 16) |
				   (reg & 0xffff);
			dev_info(dev, "cl45 phyaddr:%#x, reg:%#x-%#x, %#x\n",
				 phy_dev->mdio.addr,
				 devid,
				 reg,
				 mdiobus_read(priv->mii,
					      phy_dev->mdio.addr,
					      reg_addr));
		} else {
			dev_info(dev, "format error\n");
		}
	} else if (!strncmp(buf, "cl45w", 5)) {
		if (sscanf(buf + 5, "%x %x %x %x", &phy_addr, &devid,
			   &reg, &data) == 4) {
			reg_addr = (1 << 30) |
				   ((devid & 0x1f) << 16) |
				   (reg & 0xffff);
			origin = mdiobus_read(priv->mii,
					      phy_addr,
					      reg_addr);
			mdiobus_write(priv->mii,
				      phy_addr,
				      reg_addr,
				      data);
			dev_info(dev, "cl45 phyaddr:%#x, reg:%#x-%#x, %#x -> %#x\n",
				 phy_addr,
				 devid,
				 reg,
				 origin,
				 mdiobus_read(priv->mii,
					      phy_addr,
					      reg_addr));
		} else if (sscanf(buf + 5, "%x %x %x", &devid,
				  &reg, &data) == 3) {
			if (!phy_dev) {
				dev_info(dev, "null phy_dev\n");
				goto out;
			}

			reg_addr = (1 << 30) |
				   ((devid & 0x1f) << 16) |
				   (reg & 0xffff);
			origin = mdiobus_read(priv->mii,
					      phy_dev->mdio.addr,
					      reg_addr);
			mdiobus_write(priv->mii, phy_dev->mdio.addr,
				      reg_addr, data);
			dev_info(dev, "cl45 phyaddr:%#x, reg:%#x-%#x, %#x -> %#x\n",
				 phy_dev->mdio.addr,
				 devid,
				 reg,
				 origin,
				 mdiobus_read(priv->mii,
					      phy_dev->mdio.addr,
					      reg_addr));
		} else {
			dev_info(dev, "format error\n");
		}
	}  else if (!strncmp(buf, "mmdr", 4)) {
		if (sscanf(buf + 4, "%x %x %x", &phy_addr, &dev_addr, &reg) == 3){
			mmd_read(priv, phy_addr, dev_addr, reg);
			dev_info(dev, "mmd phyaddr:%#x, reg:%#x-%#x, %#x\n",
				 phy_addr,
				 dev_addr,
				 reg,
				 mmd_read(priv,
					  phy_addr,
					  dev_addr,
					  reg));
		}else if (sscanf(buf + 4, "%x %x", &dev_addr, &reg) == 2){
			if (!phy_dev) {
				dev_info(dev, "null phy_dev\n");
				goto out;
			}

			mmd_read(priv, phy_dev->mdio.addr, dev_addr, reg);
			dev_info(dev, "mmd phyaddr:%#x, reg:%#x-%#x, %#x\n",
				 phy_dev->mdio.addr,
				 dev_addr,
				 reg,
				 mmd_read(priv,
					  phy_dev->mdio.addr,
					  dev_addr,
					  reg));
		} else {
			dev_info(dev, "format error\n");
		}
	} else if (!strncmp(buf, "mmdw", 4) ) {
		if (sscanf(buf + 4, "%x %x %x %x", &phy_addr,
			   &dev_addr, &reg, &data) == 4){
			origin = mmd_read(priv, phy_addr, dev_addr, reg);
			mmd_write(priv, phy_addr, dev_addr, reg, data);
			dev_info(dev, "mmd phyaddr:%#x, reg:%#x-%#x, %#x -> %#x\n",
				 phy_addr,
				 dev_addr,
				 reg,
				 origin,
				 mmd_read(priv, phy_addr, dev_addr, reg));
		} else if ((sscanf(buf + 4, "%x %x %x", &dev_addr,
				   &reg, &data) == 3)) {
			if (!phy_dev) {
				dev_info(dev, "null phy_dev\n");
				goto out;
			}

			origin = mmd_read(priv,
					  phy_dev->mdio.addr,
					  dev_addr,
					  reg);
			mmd_write(priv,
				  phy_dev->mdio.addr,
				  dev_addr,
				  reg,
				  data);
			dev_info(dev, "mmd phyaddr:%#x, reg:%#x-%#x, %#x -> %#x\n",
				 phy_dev->mdio.addr,
				 dev_addr,
				 reg,
				 origin,
				 mmd_read(priv,
					  phy_dev->mdio.addr,
					  dev_addr,
					  reg));
		} else {
			dev_info(dev, "format error\n");
		}
	} else if (!strncmp(buf, "rr", 2) &&
		   (sscanf(buf + 2, "%x", &reg) == 1)) {
		tmp_addr = ioremap_cache(reg, 32);
		data = readl(tmp_addr);
		dev_info(dev, "rr reg%#x, value:%#x\n", reg, data);
	} else if (!strncmp(buf, "wr", 2) &&
		   (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		tmp_addr = ioremap_cache(reg, 32);
		origin = readl(tmp_addr);
		writel(data, tmp_addr);
		dev_info(dev, "reg%#x, value:%#x -> %#x\n",
			 reg, origin, readl(tmp_addr));
	} else if (!strncmp(buf, "pager", 5)) {
		if (sscanf(buf + 5, "%x %x", &phy_addr, &reg) == 2) {
			dev_info(dev, "pager page_addr:%#x, reg:%#x, %#x\n",
				 phy_addr,
				 reg,
				 phy_read_paged(phy_dev, phy_addr, reg));
		}
	} else if (!strncmp(buf, "pagew", 5)) {
		if (sscanf(buf + 5, "%x %x %x", &phy_addr, &reg, &data) == 3) {
			origin = phy_read_paged(phy_dev, phy_addr, reg);
			phy_write_paged(phy_dev, phy_addr, reg, data);
			dev_info(dev, "pagew page_addr:%#x, reg:%#x, %#x -> %#x\n",
				 phy_addr,
				 reg,
				 origin,
				 phy_read_paged(phy_dev, phy_addr, reg));
		}
	} else if (!strncmp(buf, "dump_mac", 8)) {
		for (i = 0; i < 0x1300 / 0x10 + 1; i++) {
			pr_info("%08x:\t%08x\t%08x\t%08x\t%08x\t\n",
				i * 16,
				readl(priv->ioaddr + i * 0x10),
				readl(priv->ioaddr + i * 0x10 + 0x4),
				readl(priv->ioaddr + i * 0x10 + 0x8),
				readl(priv->ioaddr + i * 0x10 + 0xc));
		}
	} else if (!strncmp(buf, "tx", 2)) {
		if (sscanf(buf + 2, "%d %d %d", &reg, &data, &reg_addr) == 3)
			__loopback_test(priv, reg, reg_addr, true, data);
		else if (sscanf(buf + 2, "%d %d", &reg, &data) == 2)
			__loopback_test(priv, reg, 10000, true, data);
		else if (sscanf(buf + 2, "%d", &reg) == 1)
			__loopback_test(priv, reg, 10000, true, 1500);
	} else if (!strncmp(buf, "sgmii", 5) &&
		   (sscanf(buf + 5, "%x", &reg) == 1)) {
		if (reg == 1) {
			priv_plat->sgmii->flags = BIT(31);
			mediatek_sgmii_path_setup(priv_plat->sgmii);
			dev_info(dev, "set SGMII to 1G AN mode\n");
		} else if (reg == 2) {
			priv_plat->sgmii->flags = BIT(1);
			mediatek_sgmii_path_setup(priv_plat->sgmii);
			dev_info(dev, "set SGMII to 2.5G fix mode\n");
		} else {
			dev_info(dev, "Error: parameter not support\n");
		}
	} else if (!strncmp(buf, "carrier_on", 10)) {
		netif_carrier_on(priv->dev);
	} else if (!strncmp(buf, "carrier_off", 11)) {
		netif_carrier_off(priv->dev);
	} else {
		dev_info(dev, "Error: command not support\n");
	}
out:
	return count;
}

static DEVICE_ATTR_RW(stmmac);

static int eth_create_attr(struct device *dev)
{
	int err = 0;

	if (!dev)
		return -EINVAL;

	err = device_create_file(dev, &dev_attr_stmmac);
	if (err)
		pr_err("create debug file fail:%s\n", __func__);

	return err;
}

static void eth_remove_attr(struct device *dev)
{
	device_remove_file(dev, &dev_attr_stmmac);
}
#endif

static void sgmii_polling_link_status(void *vpriv)
{
	struct stmmac_priv *priv = vpriv;
	struct mediatek_dwmac_plat_data *priv_plat = priv->plat->bsp_priv;
	int ret;

	if (priv_plat->phy_mode == PHY_INTERFACE_MODE_SGMII ||
	    priv_plat->phy_mode == PHY_INTERFACE_MODE_2500BASEX) {
		ret = mediatek_sgmii_polling_link_status(priv_plat->sgmii);
		if (ret)
			pr_err("%s: sgmii link up fail", __func__);
	}
}

static void sgmii_setup(void *vpriv)
{
	struct stmmac_priv *priv = vpriv;
	struct mediatek_dwmac_plat_data *priv_plat = priv->plat->bsp_priv;
	int ret;

	if (priv_plat->phy_mode == PHY_INTERFACE_MODE_SGMII ||
	    priv_plat->phy_mode == PHY_INTERFACE_MODE_2500BASEX) {
		ret = mediatek_sgmii_path_setup(priv_plat->sgmii);
		if (ret)
			pr_err("%s: sgmii link up fail", __func__);
	}
}

static int mt2712_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	int rmii_clk_from_mac = plat->rmii_clk_from_mac ? RMII_CLK_SRC_INTERNAL : 0;
	int rmii_rxc = plat->rmii_rxc ? RMII_CLK_SRC_RXC : 0;
	u32 intf_val = 0;

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		intf_val |= PHY_INTF_MII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		intf_val |= (PHY_INTF_RMII | rmii_rxc | rmii_clk_from_mac);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val |= PHY_INTF_RGMII;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_write(plat->peri_regmap, PERI_ETH_PHY_INTF_SEL, intf_val);

	return 0;
}

static void mt2712_delay_ps2stage(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/* 550ps per stage for MII/RMII */
		mac_delay->tx_delay /= 550;
		mac_delay->rx_delay /= 550;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		/* 170ps per stage for RGMII */
		mac_delay->tx_delay /= 170;
		mac_delay->rx_delay /= 170;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		break;
	}
}

static void mt2712_delay_stage2ps(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/* 550ps per stage for MII/RMII */
		mac_delay->tx_delay *= 550;
		mac_delay->rx_delay *= 550;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		/* 170ps per stage for RGMII */
		mac_delay->tx_delay *= 170;
		mac_delay->rx_delay *= 170;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		break;
	}
}

static int mt2712_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 delay_val = 0, fine_val = 0;

	mt2712_delay_ps2stage(plat);

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (plat->rmii_clk_from_mac) {
			/* case 1: mac provides the rmii reference clock,
			 * and the clock output to TXC pin.
			 * The egress timing can be adjusted by GTXC delay macro circuit.
			 * The ingress timing can be adjusted by TXC delay macro circuit.
			 */
			delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->rx_inv);

			delay_val |= FIELD_PREP(ETH_DLY_GTXC_ENABLE, !!mac_delay->tx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_GTXC_STAGES, mac_delay->tx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_GTXC_INV, mac_delay->tx_inv);
		} else {
			/* case 2: the rmii reference clock is from external phy,
			 * and the property "rmii_rxc" indicates which pin(TXC/RXC)
			 * the reference clk is connected to. The reference clock is a
			 * received signal, so rx_delay/rx_inv are used to indicate
			 * the reference clock timing adjustment
			 */
			if (plat->rmii_rxc) {
				/* the rmii reference clock from outside is connected
				 * to RXC pin, the reference clock will be adjusted
				 * by RXC delay macro circuit.
				 */
				delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
				delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
				delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
			} else {
				/* the rmii reference clock from outside is connected
				 * to TXC pin, the reference clock will be adjusted
				 * by TXC delay macro circuit.
				 */
				delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->rx_delay);
				delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->rx_delay);
				delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->rx_inv);
			}
			/* tx_inv will inverse the tx clock inside mac relateive to
			 * reference clock from external phy,
			 * and this bit is located in the same register with fine-tune
			 */
			if (mac_delay->tx_inv)
				fine_val = ETH_RMII_DLY_TX_INV;
		}
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		fine_val = ETH_FINE_DLY_GTXC | ETH_FINE_DLY_RXC;

		delay_val |= FIELD_PREP(ETH_DLY_GTXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_GTXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_GTXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}
	regmap_write(plat->peri_regmap, PERI_ETH_DLY, delay_val);
	regmap_write(plat->peri_regmap, PERI_ETH_DLY_FINE, fine_val);

	mt2712_delay_stage2ps(plat);

	return 0;
}

static const struct mediatek_dwmac_variant mt2712_gmac_variant = {
		.dwmac_set_phy_interface = mt2712_set_interface,
		.dwmac_set_delay = mt2712_set_delay,
		.clk_list = mt2712_dwmac_clk_l,
		.num_clks = ARRAY_SIZE(mt2712_dwmac_clk_l),
		.dma_bit_mask = 33,
		.rx_delay_max = 17600,
		.tx_delay_max = 17600,
};

static int mt8195_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	int rmii_clk_from_mac = plat->rmii_clk_from_mac ? MT8195_RMII_CLK_SRC_INTERNAL : 0;
	int rmii_rxc = plat->rmii_rxc ? MT8195_RMII_CLK_SRC_RXC : 0;
	u32 intf_val = 0;

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		intf_val |= FIELD_PREP(MT8195_ETH_INTF_SEL, PHY_INTF_MII);
		break;
	case PHY_INTERFACE_MODE_RMII:
		intf_val |= (rmii_rxc | rmii_clk_from_mac);
		intf_val |= FIELD_PREP(MT8195_ETH_INTF_SEL, PHY_INTF_RMII);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val |= FIELD_PREP(MT8195_ETH_INTF_SEL, PHY_INTF_RGMII);
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	/* MT8195 only support external PHY */
	intf_val |= MT8195_EXT_PHY_MODE;

	regmap_write(plat->peri_regmap, MT8195_PERI_ETH_CTRL0, intf_val);

	return 0;
}

static void mt8195_delay_ps2stage(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	/* 290ps per stage */
	mac_delay->tx_delay /= 290;
	mac_delay->rx_delay /= 290;
}

static void mt8195_delay_stage2ps(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	/* 290ps per stage */
	mac_delay->tx_delay *= 290;
	mac_delay->rx_delay *= 290;
}

static int mt8195_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 gtxc_delay_val = 0, delay_val = 0, rmii_delay_val = 0;

	mt8195_delay_ps2stage(plat);

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		delay_val |= FIELD_PREP(MT8195_DLY_TXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(MT8195_DLY_TXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(MT8195_DLY_TXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(MT8195_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8195_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8195_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (plat->rmii_clk_from_mac) {
			/* case 1: mac provides the rmii reference clock,
			 * and the clock output to TXC pin.
			 * The egress timing can be adjusted by RMII_TXC delay macro circuit.
			 * The ingress timing can be adjusted by RMII_RXC delay macro circuit.
			 */
			rmii_delay_val |= FIELD_PREP(MT8195_DLY_RMII_TXC_ENABLE,
						     !!mac_delay->tx_delay);
			rmii_delay_val |= FIELD_PREP(MT8195_DLY_RMII_TXC_STAGES,
						     mac_delay->tx_delay);
			rmii_delay_val |= FIELD_PREP(MT8195_DLY_RMII_TXC_INV,
						     mac_delay->tx_inv);

			rmii_delay_val |= FIELD_PREP(MT8195_DLY_RMII_RXC_ENABLE,
						     !!mac_delay->rx_delay);
			rmii_delay_val |= FIELD_PREP(MT8195_DLY_RMII_RXC_STAGES,
						     mac_delay->rx_delay);
			rmii_delay_val |= FIELD_PREP(MT8195_DLY_RMII_RXC_INV,
						     mac_delay->rx_inv);
		} else {
			/* case 2: the rmii reference clock is from external phy,
			 * and the property "rmii_rxc" indicates which pin(TXC/RXC)
			 * the reference clk is connected to. The reference clock is a
			 * received signal, so rx_delay/rx_inv are used to indicate
			 * the reference clock timing adjustment
			 */
			if (plat->rmii_rxc) {
				/* the rmii reference clock from outside is connected
				 * to RXC pin, the reference clock will be adjusted
				 * by RXC delay macro circuit.
				 */
				delay_val |= FIELD_PREP(MT8195_DLY_RXC_ENABLE,
							!!mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8195_DLY_RXC_STAGES,
							mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8195_DLY_RXC_INV,
							mac_delay->rx_inv);
			} else {
				/* the rmii reference clock from outside is connected
				 * to TXC pin, the reference clock will be adjusted
				 * by TXC delay macro circuit.
				 */
				delay_val |= FIELD_PREP(MT8195_DLY_TXC_ENABLE,
							!!mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8195_DLY_TXC_STAGES,
							mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8195_DLY_TXC_INV,
							mac_delay->rx_inv);
			}
		}
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		gtxc_delay_val |= FIELD_PREP(MT8195_DLY_GTXC_ENABLE, !!mac_delay->tx_delay);
		gtxc_delay_val |= FIELD_PREP(MT8195_DLY_GTXC_STAGES, mac_delay->tx_delay);
		gtxc_delay_val |= FIELD_PREP(MT8195_DLY_GTXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(MT8195_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8195_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8195_DLY_RXC_INV, mac_delay->rx_inv);

		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_update_bits(plat->peri_regmap,
			   MT8195_PERI_ETH_CTRL0,
			   MT8195_RGMII_TXC_PHASE_CTRL |
			   MT8195_DLY_GTXC_INV |
			   MT8195_DLY_GTXC_ENABLE |
			   MT8195_DLY_GTXC_STAGES,
			   gtxc_delay_val);
	regmap_write(plat->peri_regmap, MT8195_PERI_ETH_CTRL1, delay_val);
	regmap_write(plat->peri_regmap, MT8195_PERI_ETH_CTRL2, rmii_delay_val);

	mt8195_delay_stage2ps(plat);

	return 0;
}

static const struct mediatek_dwmac_variant mt8195_gmac_variant = {
	.dwmac_set_phy_interface = mt8195_set_interface,
	.dwmac_set_delay = mt8195_set_delay,
	.clk_list = mt8195_dwmac_clk_l,
	.num_clks = ARRAY_SIZE(mt8195_dwmac_clk_l),
	.dma_bit_mask = 35,
	.rx_delay_max = 9280,
	.tx_delay_max = 9280,
};

static int mt8678_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	int rmii_clk_from_mac = plat->rmii_clk_from_mac ? MT8678_RMII_CLK_SRC_INTERNAL : 0;
	int rmii_rxc = plat->rmii_rxc ? MT8678_RMII_CLK_SRC_RXC : 0;
	u32 intf_val = 0, offset_ctrl = 0;
	int ret = 0;

	if (of_device_is_compatible(plat->np, "mediatek,mt8678-gmac0"))
		offset_ctrl = MT8678_MAC0_PERI_ETH_CTRL0;
	else
		offset_ctrl = MT8678_MAC1_PERI_ETH_CTRL0;

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		intf_val |= FIELD_PREP(MT8678_ETH_INTF_SEL, PHY_INTF_MII);
		intf_val |= MT8678_EXT_PHY_MODE;
		break;
	case PHY_INTERFACE_MODE_RMII:
		intf_val |= (rmii_rxc | rmii_clk_from_mac);
		intf_val |= FIELD_PREP(MT8678_ETH_INTF_SEL, PHY_INTF_RMII);
		intf_val |= MT8678_EXT_PHY_MODE;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val |= FIELD_PREP(MT8678_ETH_INTF_SEL, PHY_INTF_RGMII);
				intf_val |= MT8678_EXT_PHY_MODE;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_SGMII:
		intf_val |= MT8678_SGMII_CLK_INTERNAL;
		intf_val &= ~MT8678_EXT_PHY_MODE;
		plat->sgmii = devm_kzalloc(plat->dev, sizeof(*plat->sgmii),
					   GFP_KERNEL);
		if (!plat->sgmii)
			return -ENOMEM;

		ret = mediatek_sgmii_init(plat->sgmii, plat->np, plat->dev);
		if (ret) {
			dev_err(plat->dev, "SGMII init failed\n");
			return -EINVAL;
		}
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	intf_val |= MT8678_TXC_OUT_OP;
	intf_val |= MT8678_RMII_CLK_SRC_INTERNAL;
	regmap_write(plat->peri_regmap, offset_ctrl, intf_val);

	return 0;
}

static void mt8678_delay_ps2stage(struct mediatek_dwmac_plat_data *plat)
{
}

static void mt8678_delay_stage2ps(struct mediatek_dwmac_plat_data *plat)
{
}

static int mt8678_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 gtxc_delay_val = 0, delay_val = 0, rmii_delay_val = 0;
	u32 offset_ctrl0 = 0, offset_ctrl1 = 0, offset_ctrl2 = 0;

	if (of_device_is_compatible(plat->np, "mediatek,mt8678-gmac0")) {
		offset_ctrl0 = MT8678_MAC0_PERI_ETH_CTRL0;
		offset_ctrl1 = MT8678_MAC0_PERI_ETH_CTRL1;
		offset_ctrl2 = MT8678_MAC0_PERI_ETH_CTRL2;
	} else {
		offset_ctrl0 = MT8678_MAC1_PERI_ETH_CTRL0;
		offset_ctrl1 = MT8678_MAC1_PERI_ETH_CTRL1;
		offset_ctrl2 = MT8678_MAC1_PERI_ETH_CTRL2;
	}

	mt8678_delay_ps2stage(plat);

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		delay_val |= FIELD_PREP(MT8678_DLY_TXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(MT8678_DLY_TXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(MT8678_DLY_TXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(MT8678_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8678_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8678_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (plat->rmii_clk_from_mac) {
			/* case 1: mac provides the rmii reference clock,
			 * and the clock output to TXC pin.
			 * The egress timing can be adjusted by RMII_TXC delay macro circuit.
			 * The ingress timing can be adjusted by RMII_RXC delay macro circuit.
			 */
			rmii_delay_val |= FIELD_PREP(MT8678_DLY_RMII_TXC_ENABLE,
						     !!mac_delay->tx_delay);
			rmii_delay_val |= FIELD_PREP(MT8678_DLY_RMII_TXC_STAGES,
						     mac_delay->tx_delay);
			rmii_delay_val |= FIELD_PREP(MT8678_DLY_RMII_TXC_INV,
						     mac_delay->tx_inv);

			rmii_delay_val |= FIELD_PREP(MT8678_DLY_RMII_RXC_ENABLE,
						     !!mac_delay->rx_delay);
			rmii_delay_val |= FIELD_PREP(MT8678_DLY_RMII_RXC_STAGES,
						     mac_delay->rx_delay);
			rmii_delay_val |= FIELD_PREP(MT8678_DLY_RMII_RXC_INV,
						     mac_delay->rx_inv);
		} else {
			/* case 2: the rmii reference clock is from external phy,
			 * and the property "rmii_rxc" indicates which pin(TXC/RXC)
			 * the reference clk is connected to. The reference clock is a
			 * received signal, so rx_delay/rx_inv are used to indicate
			 * the reference clock timing adjustment
			 */
			if (plat->rmii_rxc) {
				/* the rmii reference clock from outside is connected
				 * to RXC pin, the reference clock will be adjusted
				 * by RXC delay macro circuit.
				 */
				delay_val |= FIELD_PREP(MT8678_DLY_RXC_ENABLE,
							!!mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8678_DLY_RXC_STAGES,
							mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8678_DLY_RXC_INV,
							mac_delay->rx_inv);
			} else {
				/* the rmii reference clock from outside is connected
				 * to TXC pin, the reference clock will be adjusted
				 * by TXC delay macro circuit.
				 */
				delay_val |= FIELD_PREP(MT8678_DLY_TXC_ENABLE,
							!!mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8678_DLY_TXC_STAGES,
							mac_delay->rx_delay);
				delay_val |= FIELD_PREP(MT8678_DLY_TXC_INV,
							mac_delay->rx_inv);
			}
		}
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		gtxc_delay_val |= MT8678_RGMII_TXC_PHASE_CTRL;
		gtxc_delay_val |= FIELD_PREP(MT8678_DLY_GTXC_ENABLE, !!mac_delay->tx_delay);
		gtxc_delay_val |= FIELD_PREP(MT8678_DLY_GTXC_STAGES, mac_delay->tx_delay);
		gtxc_delay_val |= FIELD_PREP(MT8678_DLY_GTXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(MT8678_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8678_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(MT8678_DLY_RXC_INV, mac_delay->rx_inv);

		break;
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_SGMII:
		delay_val |= MT8678_DLY_RXC_INV;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_update_bits(plat->peri_regmap,
			   offset_ctrl0,
			   MT8678_RGMII_TXC_PHASE_CTRL |
			   MT8678_DLY_GTXC_INV |
			   MT8678_DLY_GTXC_ENABLE |
			   MT8678_DLY_GTXC_STAGES,
			   gtxc_delay_val);
	regmap_write(plat->peri_regmap, offset_ctrl1, delay_val);
	regmap_write(plat->peri_regmap, offset_ctrl2, rmii_delay_val);

	mt8678_delay_stage2ps(plat);

	return 0;
}

int mt8678_set_base_addr(struct platform_device *pdev,
			 struct stmmac_resources *stmmac_res)
{
	struct device_node *dn;
	struct chipid *chipid;
	int sw_ver = 0;

	dn = of_find_node_by_path("/chosen");
	if (!dn)
		dn = of_find_node_by_path("chosen@0");

	if (dn) {
		chipid = (struct chipid *)of_get_property(dn, "atag,chipid", NULL);
		if (!chipid) {
			dev_err(&pdev->dev, "Failed to get chipid\n");
			return -EPROBE_DEFER;
		}
		sw_ver = (int)chipid->sw_ver;
	} else {
		dev_err(&pdev->dev, "Failed to find '/chosen' or 'chosen@0' node\n");
		return -ENODEV;
	}

	if(sw_ver)
		stmmac_res->addr = devm_platform_ioremap_resource_byname(pdev, "b0");

	return 0;
}

static const struct mediatek_dwmac_variant mt8678_gmac_variant = {
	.dwmac_set_phy_interface = mt8678_set_interface,
	.dwmac_set_delay = mt8678_set_delay,
	.dwmac_set_base_addr = mt8678_set_base_addr,
	.clk_list = mt8678_dwmac_clk_l,
	.num_clks = ARRAY_SIZE(mt8678_dwmac_clk_l),
	.dma_bit_mask = 40,
};

static int mediatek_dwmac_config_dt(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 tx_delay_ps, rx_delay_ps;
	int err;

	plat->peri_regmap = syscon_regmap_lookup_by_phandle(plat->np, "mediatek,pericfg");
	if (IS_ERR(plat->peri_regmap)) {
		dev_err(plat->dev, "Failed to get pericfg syscon\n");
		return PTR_ERR(plat->peri_regmap);
	}

	err = of_get_phy_mode(plat->np, &plat->phy_mode);
	if (err) {
		dev_err(plat->dev, "not find phy-mode\n");
		return err;
	}

	if (!of_property_read_u32(plat->np, "mediatek,tx-delay-ps", &tx_delay_ps)) {
		if (tx_delay_ps < plat->variant->tx_delay_max) {
			mac_delay->tx_delay = tx_delay_ps;
		} else {
			dev_err(plat->dev, "Invalid TX clock delay: %dps\n", tx_delay_ps);
			return -EINVAL;
		}
	}

	if (!of_property_read_u32(plat->np, "mediatek,rx-delay-ps", &rx_delay_ps)) {
		if (rx_delay_ps < plat->variant->rx_delay_max) {
			mac_delay->rx_delay = rx_delay_ps;
		} else {
			dev_err(plat->dev, "Invalid RX clock delay: %dps\n", rx_delay_ps);
			return -EINVAL;
		}
	}

	mac_delay->tx_inv = of_property_read_bool(plat->np, "mediatek,txc-inverse");
	mac_delay->rx_inv = of_property_read_bool(plat->np, "mediatek,rxc-inverse");
	plat->rmii_rxc = of_property_read_bool(plat->np, "mediatek,rmii-rxc");
	plat->rmii_clk_from_mac = of_property_read_bool(plat->np, "mediatek,rmii-clk-from-mac");
	plat->mac_wol = of_property_read_bool(plat->np, "mediatek,mac-wol");

	return 0;
}

static int mediatek_dwmac_clk_init(struct mediatek_dwmac_plat_data *plat)
{
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int i, ret;

	plat->clks = devm_kcalloc(plat->dev, variant->num_clks, sizeof(*plat->clks), GFP_KERNEL);
	if (!plat->clks)
		return -ENOMEM;

	for (i = 0; i < variant->num_clks; i++)
		plat->clks[i].id = variant->clk_list[i];

	ret = devm_clk_bulk_get(plat->dev, variant->num_clks, plat->clks);
	if (ret)
		return ret;

	/* The clock labeled as "rmii_internal" is needed only in RMII(when
	 * MAC provides the reference clock), and useless for RGMII/MII or
	 * RMII(when PHY provides the reference clock).
	 * So, "rmii_internal" clock is got and configured only when
	 * reference clock of RMII is from MAC.
	 */
	if (plat->rmii_clk_from_mac) {
		plat->rmii_internal_clk = devm_clk_get(plat->dev, "rmii_internal");
		if (IS_ERR(plat->rmii_internal_clk))
			ret = PTR_ERR(plat->rmii_internal_clk);
	} else {
		plat->rmii_internal_clk = NULL;
	}

	if (plat->phy_mode == PHY_INTERFACE_MODE_SGMII ||
	    plat->phy_mode == PHY_INTERFACE_MODE_2500BASEX) {
		plat->sgmii_sel_clk = devm_clk_get(plat->dev, "sgmii_sel");
		if (IS_ERR(plat->sgmii_sel_clk))
			ret = PTR_ERR(plat->sgmii_sel_clk);

		plat->sgmii_sbus_sel_clk = devm_clk_get(plat->dev, "sgmii_sbus_sel");
		if (IS_ERR(plat->sgmii_sbus_sel_clk))
			ret = PTR_ERR(plat->sgmii_sbus_sel_clk);
	} else {
		plat->sgmii_sel_clk = NULL;
		plat->sgmii_sbus_sel_clk = NULL;
	}

	return ret;
}

static int mediatek_dwmac_init(struct platform_device *pdev, void *priv)
{
	struct mediatek_dwmac_plat_data *plat = priv;
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int ret;

	if (variant->dwmac_set_phy_interface) {
		ret = variant->dwmac_set_phy_interface(plat);
		if (ret) {
			dev_err(plat->dev, "failed to set phy interface, err = %d\n", ret);
			return ret;
		}
	}

	if (variant->dwmac_set_delay) {
		ret = variant->dwmac_set_delay(plat);
		if (ret) {
			dev_err(plat->dev, "failed to set delay value, err = %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int mediatek_dwmac_clks_config(void *priv, bool enabled)
{
	struct mediatek_dwmac_plat_data *plat = priv;
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int ret = 0;

	if (enabled) {
		ret = clk_bulk_prepare_enable(variant->num_clks, plat->clks);
		if (ret) {
			dev_err(plat->dev, "failed to enable clks, err = %d\n", ret);
			return ret;
		}

		ret = clk_prepare_enable(plat->rmii_internal_clk);
		if (ret) {
			dev_err(plat->dev, "failed to enable rmii internal clk, err = %d\n", ret);
			return ret;
		}

		ret = clk_prepare_enable(plat->sgmii_sel_clk);
		if (ret) {
			dev_err(plat->dev, "failed to enable sgmii_sel clk, err = %d\n", ret);
			return ret;
		}

		ret = clk_prepare_enable(plat->sgmii_sbus_sel_clk);
		if (ret) {
			dev_err(plat->dev, "failed to enable sgmii_sbus_sel clk, err = %d\n", ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(plat->sgmii_sbus_sel_clk);
		clk_disable_unprepare(plat->sgmii_sel_clk);
		clk_disable_unprepare(plat->rmii_internal_clk);
		clk_bulk_disable_unprepare(variant->num_clks, plat->clks);
	}

	return ret;
}

static int mediatek_dwmac_common_data(struct platform_device *pdev,
				      struct plat_stmmacenet_data *plat,
				      struct mediatek_dwmac_plat_data *priv_plat)
{
	int i;

	plat->mac_interface = priv_plat->phy_mode;
	if (priv_plat->mac_wol)
		plat->flags |= STMMAC_FLAG_USE_PHY_WOL;
	else
		plat->flags &= ~STMMAC_FLAG_USE_PHY_WOL;
	plat->riwt_off = 1;
	plat->maxmtu = ETH_DATA_LEN;
	plat->host_dma_width = priv_plat->variant->dma_bit_mask;
	plat->bsp_priv = priv_plat;
	plat->init = mediatek_dwmac_init;
	plat->clks_config = mediatek_dwmac_clks_config;
	plat->sgmii_polling_link_status = sgmii_polling_link_status;
	plat->sgmii_setup = sgmii_setup;

	plat->safety_feat_cfg = devm_kzalloc(&pdev->dev,
					     sizeof(*plat->safety_feat_cfg),
					     GFP_KERNEL);
	if (!plat->safety_feat_cfg)
		return -ENOMEM;

	plat->safety_feat_cfg->tsoee = 1;
	plat->safety_feat_cfg->mrxpee = 0;
	plat->safety_feat_cfg->mestee = 1;
	plat->safety_feat_cfg->mrxee = 1;
	plat->safety_feat_cfg->mtxee = 1;
	plat->safety_feat_cfg->epsi = 0;
	plat->safety_feat_cfg->edpp = 1;
	plat->safety_feat_cfg->prtyen = 1;
	plat->safety_feat_cfg->tmouten = 1;

	for (i = 0; i < plat->tx_queues_to_use; i++) {
		/* Default TX Q0 to use TSO and rest TXQ for TBS */
		if (i > 0)
			plat->tx_queues_cfg[i].tbs_en = 1;
	}
	plat->flags |= STMMAC_FLAG_TSO_EN;
	plat->flags |= STMMAC_FLAG_RX_CLK_RUNS_IN_LPI;
	plat->flags |= STMMAC_FLAG_SPH_DISABLE;

	return 0;
}

static int mediatek_dwmac_probe(struct platform_device *pdev)
{
	struct mediatek_dwmac_plat_data *priv_plat;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	int ret;

	priv_plat = devm_kzalloc(&pdev->dev, sizeof(*priv_plat), GFP_KERNEL);
	if (!priv_plat)
		return -ENOMEM;

	priv_plat->variant = of_device_get_match_data(&pdev->dev);
	if (!priv_plat->variant) {
		dev_err(&pdev->dev, "Missing dwmac-mediatek variant\n");
		return -EINVAL;
	}

	priv_plat->dev = &pdev->dev;
	priv_plat->np = pdev->dev.of_node;

	ret = mediatek_dwmac_config_dt(priv_plat);
	if (ret)
		return ret;

	ret = mediatek_dwmac_clk_init(priv_plat);
	if (ret)
		return ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	if(priv_plat->variant->dwmac_set_base_addr) {
		ret = priv_plat->variant->dwmac_set_base_addr(pdev, &stmmac_res);
		if (ret)
			return ret;
	}

	plat_dat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	mediatek_dwmac_common_data(pdev, plat_dat, priv_plat);
	mediatek_dwmac_init(pdev, priv_plat);

	ret = mediatek_dwmac_clks_config(priv_plat, true);
	if (ret)
		goto err_remove_config_dt;

	if (priv_plat->phy_mode == PHY_INTERFACE_MODE_SGMII ||
	    priv_plat->phy_mode == PHY_INTERFACE_MODE_2500BASEX) {
		ret = mediatek_sgmii_path_setup(priv_plat->sgmii);
		if (ret) {
			dev_err(priv_plat->dev, "failed to set sgmii path, err = %d\n", ret);
			goto err_remove_config_dt;
		}
	}

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_drv_probe;

#ifdef CONFIG_DEBUG_FS
	eth_create_attr(&pdev->dev);
#endif

	return 0;

err_drv_probe:
	mediatek_dwmac_clks_config(priv_plat, false);
err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static void mediatek_dwmac_remove(struct platform_device *pdev)
{
	struct mediatek_dwmac_plat_data *priv_plat = get_stmmac_bsp_priv(&pdev->dev);

#ifdef CONFIG_DEBUG_FS
	eth_remove_attr(&pdev->dev);
#endif

	stmmac_pltfr_remove(pdev);
	mediatek_dwmac_clks_config(priv_plat, false);
}

static const struct of_device_id mediatek_dwmac_match[] = {
	{ .compatible = "mediatek,mt2712-gmac",
	  .data = &mt2712_gmac_variant },
	{ .compatible = "mediatek,mt8195-gmac",
	  .data = &mt8195_gmac_variant },
	{ .compatible = "mediatek,mt8678-gmac0",
	  .data = &mt8678_gmac_variant },
	{ .compatible = "mediatek,mt8678-gmac1",
	  .data = &mt8678_gmac_variant },
	{ }
};

MODULE_DEVICE_TABLE(of, mediatek_dwmac_match);

static struct platform_driver mediatek_dwmac_driver = {
	.probe  = mediatek_dwmac_probe,
	.remove_new = mediatek_dwmac_remove,
	.driver = {
		.name           = "dwmac-mediatek",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = mediatek_dwmac_match,
	},
};
module_platform_driver(mediatek_dwmac_driver);

MODULE_AUTHOR("Biao Huang <biao.huang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DWMAC specific glue layer");
MODULE_LICENSE("GPL v2");
