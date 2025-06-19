// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek PCIe host controller driver.
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/kmemleak.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/spinlock.h>
#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
#include <trace/hooks/traps.h>
#endif

#include "pcie-mediatek-gen3.h"
#include "../pci.h"
#include "../../misc/mediatek/clkbuf/src/clkbuf-ctrl.h"

/* pextp register, CG,HW mode */
#define PCIE_PEXTP_CG_0			0x14
#define PEXTP_CLOCK_CON			0x20
#define P0_LOWPOWER_CK_SEL		BIT(0)
#define P1_LOWPOWER_CK_SEL		P0_LOWPOWER_CK_SEL
#define P2_LOWPOWER_CK_SEL		BIT(3)
#define PEXTP_RES_REQ_STA		0x34
#define PEXTP1_RSV_1			0x3c
#define PCIE_SUSPEND_L2_CTRL		BIT(7)
#define PEXTP_PWRCTL_0			0x40
#define PCIE_HW_MTCMOS_EN		BIT(0)
#define PEXTP_TIMER_SET			GENMASK(31, 8)
#define CK_DIS_TIMER_32K		0x400
#define PEXTP_PWRCTL_1			0x44
#define PEXTP_PWRCTL_3			0x4c
#define PEXTP_PWRCTL_4			0x50
#define PEXTP_PWRCTL_6			0x58
#define PEXTP_RSV_0			0x60
#define PCIE_HW_MTCMOS_EN_MD_P0		BIT(0)
#define PCIE_BBCK2_BYPASS		BIT(5)
#define PEXTP_RSV_1			0x64
#define PCIE_HW_MTCMOS_EN_MD_P1		BIT(0)
#define PEXTP_PWRCTL_8			0x74
#define PCIE_HWMODE_EN			BIT(0)
#define PEXTP_REQ_CTRL			0x7C
#define PCIE_26M_REQ_FORCE_ON		BIT(0)
#define RG_PCIE26M_BYPASS		BIT(4)
#define PEXTP_SLPPROT_RDY		0x264
/* PCIe0/1 is bit2/3, PCIe2 is bit4/5 */
#define PEXTP_MAC_SLP_READY(port)	BIT(2 + (port/2) * 2)
#define PEXTP_PHY_SLP_READY(port)	BIT(3 + (port/2) * 2)
#define PEXTP_SUM_SLP_READY(port) \
	(PEXTP_MAC_SLP_READY(port) | PEXTP_PHY_SLP_READY(port))
#define PCIE_MSI_SEL			0x304

#define PCIE_BASE_CONF_REG              0x14
#define PCIE_SUPPORT_SPEED_MASK         GENMASK(15, 8)
#define PCIE_SUPPORT_SPEED_SHIFT        8
#define PCIE_SUPPORT_SPEED_2_5GT        BIT(8)
#define PCIE_SUPPORT_SPEED_5_0GT        BIT(9)
#define PCIE_SUPPORT_SPEED_8_0GT        BIT(10)
#define PCIE_SUPPORT_SPEED_16_0GT       BIT(11)

#define PCIE_BASIC_STATUS		0x18

#define PCIE_SETTING_REG                0x80
#define PCIE_RC_MODE                    BIT(0)
#define PCIE_GEN_SUPPORT_MASK           GENMASK(14, 12)
#define PCIE_GEN_SUPPORT_SHIFT          12
#define PCIE_GEN2_SUPPORT               BIT(12)
#define PCIE_GEN3_SUPPORT               BIT(13)
#define PCIE_GEN4_SUPPORT               BIT(14)
#define PCIE_GEN_SUPPORT(max_lspd) \
	GENMASK((max_lspd) - 2 + PCIE_GEN_SUPPORT_SHIFT, PCIE_GEN_SUPPORT_SHIFT)
#define PCIE_TARGET_SPEED_MASK          GENMASK(3, 0)

#define PCIE_CFGCTRL			0x84
#define PCIE_DISABLE_LTSSM		BIT(2)
#define PCIE_PCI_IDS_1			0x9c
#define PCI_CLASS(class)		(class << 8)

#define PCIE_PCI_LPM			0xa4

#define PCIE_CFGNUM_REG			0x140
#define PCIE_CFG_DEVFN(devfn)		((devfn) & GENMASK(7, 0))
#define PCIE_CFG_BUS(bus)		(((bus) << 8) & GENMASK(15, 8))
#define PCIE_CFG_BYTE_EN(bytes)		(((bytes) << 16) & GENMASK(19, 16))
#define PCIE_CFG_FORCE_BYTE_EN		BIT(20)
#define PCIE_CFG_OFFSET_ADDR		0x1000
#define PCIE_CFG_HEADER(bus, devfn) \
	(PCIE_CFG_BUS(bus) | PCIE_CFG_DEVFN(devfn))
#define PCIE_RC_CFG \
	(PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) | PCIE_CFG_HEADER(0, 0))

#define PCIE_RST_CTRL_REG		0x148
#define PCIE_MAC_RSTB			BIT(0)
#define PCIE_PHY_RSTB			BIT(1)
#define PCIE_BRG_RSTB			BIT(2)
#define PCIE_PE_RSTB			BIT(3)

#define PCIE_LTSSM_STATUS_REG		0x150
#define PCIE_LTSSM_STATE_MASK		GENMASK(28, 24)
#define PCIE_LTSSM_STATE_CLEAR		GENMASK(31, 0)
#define PCIE_LTSSM_STATE(val)		((val & PCIE_LTSSM_STATE_MASK) >> 24)
#define PCIE_LTSSM_STATE_L2_IDLE	0x14

#define PCIE_LINK_STATUS_REG		0x154
#define PCIE_PORT_LINKUP		BIT(8)

#define PCIE_ASPM_CTRL			0x15c
#define PCIE_P2_EXIT_BY_CLKREQ		BIT(17)
#define PCIE_P2_IDLE_TIME_MASK		GENMASK(27, 24)
#define PCIE_P2_IDLE_TIME(x)		((x << 24) & PCIE_P2_IDLE_TIME_MASK)
#define PCIE_P2_SLEEP_TIME_MASK		GENMASK(31, 28)
#define PCIE_P2_SLEEP_TIME_4US		(0x4 << 28)

#define PCIE_MSI_GROUP_NUM		4
#define PCIE_MSI_SET_NUM		8
#define PCIE_MSI_IRQS_PER_SET		32
#define PCIE_MSI_IRQS_NUM \
	(PCIE_MSI_IRQS_PER_SET * PCIE_MSI_SET_NUM)

#define PCIE_DEBUG_MONITOR		0x2c
#define PCIE_DEBUG_SEL_0		0x164
#define PCIE_DEBUG_SEL_1		0x168
#define PCIE_MONITOR_DEBUG_EN		0x100
#define PCIE_DEBUG_SEL_BUS(b0, b1, b2, b3) \
	((((b0) << 24) & GENMASK(31, 24)) | (((b1) << 16) & GENMASK(23, 16)) | \
	(((b2) << 8) & GENMASK(15, 8)) | ((b3) & GENMASK(7, 0)))
#define PCIE_DEBUG_SEL_PARTITION(p0, p1, p2, p3) \
	((((p0) << 28) & GENMASK(31, 28)) | (((p1) << 24) & GENMASK(27, 24)) | \
	(((p2) << 20) & GENMASK(23, 20)) | (((p3) << 16) & GENMASK(19, 16)) | \
	PCIE_MONITOR_DEBUG_EN)

#define PCIE_INT_ENABLE_REG		0x180
#define PCIE_AXI_POST_ERR_ENABLE	BIT(16)
#define PCIE_AER_EVT_EN			BIT(29)
#define PCIE_MSI_ENABLE			GENMASK(PCIE_MSI_SET_NUM + 8 - 1, 8)
#define PCIE_MSI_SHIFT			8
#define PCIE_INTX_SHIFT			24
#define PCIE_INTX_ENABLE \
	GENMASK(PCIE_INTX_SHIFT + PCI_NUM_INTX - 1, PCIE_INTX_SHIFT)

#define PCIE_INT_STATUS_REG		0x184
#define PCIE_AXIERR_COMPL_TIMEOUT	BIT(18)
#define PCIE_AXI_READ_ERR		GENMASK(18, 16)
#define PCIE_AXI_POST_ERR_EVT		BIT(16)
#define PCIE_AER_EVT			BIT(29)
#define PCIE_ERR_RST_EVT		BIT(31)
#define PCIE_MSI_SET_ENABLE_REG		0x190
#define PCIE_MSI_SET_ENABLE		GENMASK(PCIE_MSI_SET_NUM - 1, 0)

#define PCIE_MSI_SET_BASE_REG		0xc00
#define PCIE_MSI_SET_OFFSET		0x10
#define PCIE_MSI_SET_STATUS_OFFSET	0x04
#define PCIE_MSI_SET_ENABLE_OFFSET	0x08
#define PCIE_MSI_SET_ENABLE_GRP1_OFFSET	0x0c
#define DRIVER_OWN_IRQ_STATUS		BIT(27)

#define PCIE_MSI_SET_ADDR_HI_BASE	0xc80
#define PCIE_MSI_SET_ADDR_HI_OFFSET	0x04

#define PHY_ERR_DEBUG_LANE0		0xD40

#define PCIE_MSI_GRP2_SET_OFFSET	0xDC0
#define PCIE_MSI_GRPX_PER_SET_OFFSET	4
#define PCIE_MSI_GRP3_SET_OFFSET	0xDE0

#define PCIE_RES_STATUS			0xd28
#define ALL_RES_ACK			0xef

#define PCIE_AXI0_ERR_ADDR_L		0xe00
#define PCIE_AXI0_ERR_INFO		0xe08
#define PCIE_ERR_STS_CLEAR		BIT(0)

#define PCIE_ERR_ADDR_L			0xe40
#define PCIE_ERR_ADDR_H			0xe44
#define PCIE_ERR_INFO			0xe48

#define PCIE_LOW_POWER_CTRL		0x194
#define PCIE_ICMD_PM_REG		0x198
#define PCIE_TURN_OFF_LINK		BIT(4)

#define PCIE_ISTATUS_PM			0x19C
#define PCIE_L1PM_SM			GENMASK(10, 8)

#define PCIE_AXI_IF_CTRL		0x1a8
#define PCIE_AXI0_SLV_RESP_MASK		BIT(12)
#define PCIE_CPLTO_SCALE_R2_L		16
#define PCIE_CPLTO_SCALE_R5_L		20
#define PCIE_SW_CPLTO_TIMER		GENMASK(25, 16)
#define SW_CPLTO_DATA_SEL		BIT(28)

#define PCIE_ISTATUS_PENDING_ADT	0x1d4

#define PCIE_WCPL_TIMEOUT		0x340
#define WCPL_TIMEOUT_4MS		0x4

#define PCIE_MISC_CTRL_REG		0x348
#define PCIE_DVFS_REQ_FORCE_ON		BIT(1)
#define PCIE_MAC_SLP_DIS		BIT(7)
#define PCIE_DVFS_REQ_FORCE_OFF		BIT(12)

#define PCIE_TRANS_TABLE_BASE_REG	0x800
#define PCIE_ATR_SRC_ADDR_MSB_OFFSET	0x4
#define PCIE_ATR_TRSL_ADDR_LSB_OFFSET	0x8
#define PCIE_ATR_TRSL_ADDR_MSB_OFFSET	0xc
#define PCIE_ATR_TRSL_PARAM_OFFSET	0x10
#define PCIE_ATR_TLB_SET_OFFSET		0x20

#define PCIE_MAX_TRANS_TABLES		8
#define PCIE_ATR_EN			BIT(0)
#define PCIE_ATR_SIZE(size) \
	(((((size) - 1) << 1) & GENMASK(6, 1)) | PCIE_ATR_EN)
#define PCIE_ATR_ID(id)			((id) & GENMASK(3, 0))
#define PCIE_ATR_TYPE_MEM		PCIE_ATR_ID(0)
#define PCIE_ATR_TYPE_IO		PCIE_ATR_ID(1)
#define PCIE_ATR_TLP_TYPE(type)		(((type) << 16) & GENMASK(18, 16))
#define PCIE_ATR_TLP_TYPE_MEM		PCIE_ATR_TLP_TYPE(0)
#define PCIE_ATR_TLP_TYPE_IO		PCIE_ATR_TLP_TYPE(2)

#define PCIE_RESOURCE_CTRL		0xd2c
#define SYS_CLK_RDY_TIME		GENMASK(7, 0)
#define SYS_CLK_RDY_TIME_TO_10US	0xa
#define PCIE_APSRC_ACK			BIT(10)

/* pcie read completion timeout */
#define PCIE_CONF_DEV2_CTL_STS		0x10a8
#define PCIE_DCR2_CPL_TO		GENMASK(3, 0)
#define PCIE_CPL_TIMEOUT_50MS		0x0
#define PCIE_CPL_TIMEOUT_64US		0x1
#define PCIE_CPL_TIMEOUT_4MS		0x2
#define PCIE_CPL_TIMEOUT_32MS		0x5
#define PCIE_CPLTO_SCALE_4MS		4
#define PCIE_CPLTO_SCALE_10MS		10
#define PCIE_CPLTO_SCALE_29MS		29

#define PCIE_CONF_EXP_LNKCTL2_REG	0x10b0

/* AER status */
#define PCIE_AER_UNC_STATUS		0x1204
#define PCIE_AER_UNC_MTLP		BIT(18)
#define PCIE_AER_CO_STATUS		0x1210
#define AER_CO_RE			BIT(0)
#define AER_CO_BTLP			BIT(6)

#define PCIE_CFG_RSV_0			0x1490
#define MSI_GRP2			BIT(2)
#define MSI_GRP3			BIT(3)

#define PCIE_AER_UNC_STA		0x204
#define PCIE_AER_CO_STA			0x210
#define PCIE_RP_STS_REG			0x230

/* vlpcfg register */
#define PCIE_VLP_AXI_PROTECT_STA	0x240
#define PCIE_MAC_SLP_READY_MASK(port)	BIT(11 - port)
#define PCIE_PHY_SLP_READY_MASK(port)	BIT(13 - port)
#define PCIE_SUM_SLP_READY(port) \
	(PCIE_MAC_SLP_READY_MASK(port) | PCIE_PHY_SLP_READY_MASK(port))

#define MTK_PCIE_MAX_PORT		2
#define PCIE_CLKBUF_SUBSYS_ID		7
#define PCIE_CLKBUF_XO_ID		1
#define BBCK2_BIND			0x2c1
#define BBCK2_UNBIND			0x2c0
#define LIBER_BBCK2_BIND		0x281
#define LIBER_BBCK2_UNBIND		0x280

/* pmrc register */
#define PMRC_BBCK2_STA			0x41c

struct mtk_pcie_port;

/**
 * struct mtk_pcie_data - PCIe data for each SoC
 * @pre_init: Specific init data, called before linkup
 * @post_init: Specific init data, called after linkup
 * @suspend_l12: To implement special setting in L1.2 suspend flow
 * @resume_l12: To implement special setting in L1.2 resume flow
 * @clkbuf_control: To implement clkbuf control flow
 * @control_vote: To implement RTFF vote control flow
 */
struct mtk_pcie_data {
	int (*pre_init)(struct mtk_pcie_port *port);
	int (*post_init)(struct mtk_pcie_port *port);
	int (*suspend_l12)(struct mtk_pcie_port *port);
	int (*resume_l12)(struct mtk_pcie_port *port);
	void (*clkbuf_control)(struct mtk_pcie_port *port, bool enable);
	int (*control_vote)(struct mtk_pcie_port *port, bool hw_mode_en, u8 who);
};

/**
 * struct mtk_msi_set - MSI information for each set
 * @base: IO mapped register base
 * @msg_addr: MSI message address
 * @saved_irq_state: IRQ enable state saved at suspend time
 */
struct mtk_msi_set {
	void __iomem *base;
	phys_addr_t msg_addr;
	u32 saved_irq_state[PCIE_MSI_GROUP_NUM];
};

/**
 * struct mtk_pcie_port - PCIe port information
 * @dev: pointer to PCIe device
 * @pcidev: pointer to PCI device
 * @base: IO mapped register base
 * @pextpcfg: pextpcfg_ao(pcie HW MTCMOS) IO mapped register base
 * @vlpcfg: vlpcfg(bus protect ready) IO mapped register base
 * @pmrc: pmrc(BBCK2 control) IO mapped register base
 * @reg_base: physical register base
 * @mac_reset: MAC reset control
 * @phy_reset: PHY reset control
 * @phy: PHY controller block
 * @clks: PCIe clocks
 * @num_clks: PCIe clocks count for this port
 * @data: special init data of each SoC
 * @port_num: serial number of pcie port
 * @suspend_mode: pcie enter low poer mode when the system enter suspend
 * @cfg_saved: Determine whether config space has been saved
 * @dvfs_req_en: pcie wait request to reply ack when pcie exit from P2 state
 * @peri_reset_en: clear peri pcie reset to open pcie phy & mac
 * @dump_cfg: dump info when access config space
 * @full_debug_dump: dump full debug probe
 * @rpm: pcie runtime suspend property
 * @eint_irq: eint irq for runtime suspend wakeup source
 * @irq: PCIe controller interrupt number
 * @saved_irq_state: IRQ enable state saved at suspend time
 * @irq_lock: lock protecting IRQ register access
 * @intx_domain: legacy INTx IRQ domain
 * @msi_domain: MSI IRQ domain
 * @msi_bottom_domain: MSI IRQ bottom domain
 * @msi_sets: MSI sets information
 * @lock: lock protecting IRQ bit map
 * @vote_lock: lock protecting vote HW control mode
 * @cfg_lock: lock protecting save/restore cfg space
 * @ep_hw_mode_en: flag of ep control hw mode
 * @rc_hw_mode_en: flag of rc control hw mode
 * @msi_irq_in_use: bit map for assigned MSI IRQ
 */
struct mtk_pcie_port {
	struct device *dev;
	struct pci_dev *pcidev;
	void __iomem *base;
	void __iomem *pextpcfg;
	void __iomem *vlpcfg;
	void __iomem *pmrc;
	phys_addr_t reg_base;
	struct reset_control *mac_reset;
	struct reset_control *phy_reset;
	struct phy *phy;
	struct clk_bulk_data *clks;
	int num_clks;
	int max_link_speed;

	struct mtk_pcie_data *data;
	int port_num;
	u32 suspend_mode;
	u32 rpm_suspend_mode;
	bool cfg_saved;
	bool dvfs_req_en;
	bool peri_reset_en;
	bool soft_off;
	bool dump_cfg;
	bool full_debug_dump;
	bool aer_detect;
	bool skip_suspend;
	bool rpm;
	int eint_irq;
	int irq;
	u32 saved_irq_state;
	raw_spinlock_t irq_lock;
	struct irq_domain *intx_domain;
	struct irq_domain *msi_domain;
	struct irq_domain *msi_bottom_domain;
	struct mtk_msi_set msi_sets[PCIE_MSI_SET_NUM];
	struct mutex lock;
	spinlock_t vote_lock;
	spinlock_t cfg_lock;
	bool ep_hw_mode_en;
	bool rc_hw_mode_en;
	u32 saved_l1ss_ctl1;
	u32 saved_l1ss_ctl2;
	DECLARE_BITMAP(msi_irq_in_use, PCIE_MSI_IRQS_NUM);
};

static struct platform_device *pdev_list[MTK_PCIE_MAX_PORT];

/**
 * mtk_pcie_config_tlp_header() - Configure a configuration TLP header
 * @bus: PCI bus to query
 * @devfn: device/function number
 * @where: offset in config space
 * @size: data size in TLP header
 *
 * Set byte enable field and device information in configuration TLP header.
 */
static void mtk_pcie_config_tlp_header(struct pci_bus *bus, unsigned int devfn,
					int where, int size)
{
	struct mtk_pcie_port *port = bus->sysdata;
	int bytes;
	u32 val;

	bytes = (GENMASK(size - 1, 0) & 0xf) << (where & 0x3);

	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(bytes) |
	      PCIE_CFG_HEADER(bus->number, devfn);

	writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
}

static void __iomem *mtk_pcie_map_bus(struct pci_bus *bus, unsigned int devfn,
				      int where)
{
	struct mtk_pcie_port *port = bus->sysdata;

	return port->base + PCIE_CFG_OFFSET_ADDR + where;
}

static int mtk_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	struct mtk_pcie_port *port = bus->sysdata;
	int ret_val;
	u32 reg;

	if (port->soft_off)
		return 0;

	if (port->dump_cfg && bus->number) {
		dev_info(port->dev, "Dump config access, bus:%#x,devfn:%#x, where:%#x, size:%#x\n",
			 bus->number, devfn, where, size);
		dump_stack();
	}

	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	ret_val = pci_generic_config_read32(bus, devfn, where, size, val);
	if (ret_val)
		return ret_val;

	/*
	 * PCIe cannot read the config space of EP when an AER event occurs,
	 * If rxerr, block PCIe data transmission and avoid system hang.
	 */
	reg = readl_relaxed(port->base + PCIE_INT_STATUS_REG);
	if (reg & PCIE_AER_EVT) {
		writel_relaxed(PCIE_RC_CFG, port->base + PCIE_CFGNUM_REG);
		reg = readl_relaxed(port->base + PCIE_AER_CO_STATUS);
		if (reg & AER_CO_RE) {
			mtk_pcie_dump_link_info(port->port_num);
			mtk_pcie_disable_data_trans(port->port_num);
			dev_info(port->dev, "PCIe Rxerr detected!\n");
		}

		reg = readl_relaxed(port->base + PCIE_AER_UNC_STATUS);
		if ((reg & PCI_ERR_UNC_COMP_TIME) && port->aer_detect) {
			mtk_pcie_dump_link_info(port->port_num);
			mtk_pcie_disable_data_trans(port->port_num);
			dev_info(port->dev, "PCIe CPLTO detected!\n");
		}
	}

	return 0;
}

static int mtk_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	struct mtk_pcie_port *port = bus->sysdata;

	if (port->soft_off)
		return 0;

	if (port->dump_cfg && bus->number) {
		dev_info(port->dev, "Dump config access, bus:%#x,devfn:%#x, where:%#x, size:%#x, val:%#x\n",
			 bus->number, devfn, where, size, val);
		dump_stack();
	}

	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	if (size <= 2)
		val <<= (where & 0x3) * 8;

	return pci_generic_config_write32(bus, devfn, where, 4, val);
}

static struct pci_ops mtk_pcie_ops = {
	.map_bus = mtk_pcie_map_bus,
	.read  = mtk_pcie_config_read,
	.write = mtk_pcie_config_write,
};

static int mtk_pcie_set_trans_table(struct mtk_pcie_port *port,
				    resource_size_t cpu_addr,
				    resource_size_t pci_addr,
				    resource_size_t size,
				    unsigned long type, int num)
{
	void __iomem *table;
	u32 val;

	if (num >= PCIE_MAX_TRANS_TABLES) {
		dev_err(port->dev, "not enough translate table for addr: %#llx, limited to [%d]\n",
			(unsigned long long)cpu_addr, PCIE_MAX_TRANS_TABLES);
		return -ENODEV;
	}

	table = port->base + PCIE_TRANS_TABLE_BASE_REG +
		num * PCIE_ATR_TLB_SET_OFFSET;

	writel_relaxed(lower_32_bits(cpu_addr) | PCIE_ATR_SIZE(fls(size) - 1),
		       table);
	writel_relaxed(upper_32_bits(cpu_addr),
		       table + PCIE_ATR_SRC_ADDR_MSB_OFFSET);
	writel_relaxed(lower_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_LSB_OFFSET);
	writel_relaxed(upper_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_MSB_OFFSET);

	if (type == IORESOURCE_IO)
		val = PCIE_ATR_TYPE_IO | PCIE_ATR_TLP_TYPE_IO;
	else
		val = PCIE_ATR_TYPE_MEM | PCIE_ATR_TLP_TYPE_MEM;

	writel_relaxed(val, table + PCIE_ATR_TRSL_PARAM_OFFSET);

	return 0;
}

static void mtk_pcie_enable_msi(struct mtk_pcie_port *port)
{
	int i;
	u32 val;

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		msi_set->base = port->base + PCIE_MSI_SET_BASE_REG +
				i * PCIE_MSI_SET_OFFSET;
		msi_set->msg_addr = port->reg_base + PCIE_MSI_SET_BASE_REG +
				    i * PCIE_MSI_SET_OFFSET;

		/* Configure the MSI capture address */
		writel_relaxed(lower_32_bits(msi_set->msg_addr), msi_set->base);
		writel_relaxed(upper_32_bits(msi_set->msg_addr),
			       port->base + PCIE_MSI_SET_ADDR_HI_BASE +
			       i * PCIE_MSI_SET_ADDR_HI_OFFSET);
	}

	val = readl_relaxed(port->base + PCIE_MSI_SET_ENABLE_REG);
	val |= PCIE_MSI_SET_ENABLE;
	writel_relaxed(val, port->base + PCIE_MSI_SET_ENABLE_REG);

	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= PCIE_MSI_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
}

/*
 * mtk_pcie_dump_pextp_info() - Dump PEXTP info
 * @port: PCIe port information
 *
 * The location of the relevant registers changes with the pextp version.
 * V1 is suitable for MT6985 and MT6989, V2 is suitable for MT6991.
 */
static void mtk_pcie_dump_pextp_info(struct mtk_pcie_port *port)
{
	int val = 0;

	if (!port || !port->pextpcfg)
		return;

	/* Use port->vlpcfg to determine pextp version, because only V1 need use vlpcfg*/
	if (port->vlpcfg) {
		dev_info(port->dev, "V1:AP HW MODE:%#x, Modem HW MODE:%#x, PEXTP_PWRCTL_3:%#x, Clock gate:%#x, MSI select=%#x\n",
			readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_0),
			readl_relaxed(port->pextpcfg + PEXTP_RSV_0),
			readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_3),
			readl_relaxed(port->pextpcfg + PCIE_PEXTP_CG_0),
			readl_relaxed(port->pextpcfg + PCIE_MSI_SEL));

		return;
	}

	if (port->pmrc)
		val = readl_relaxed(port->pmrc + PMRC_BBCK2_STA);

	dev_info(port->dev, "V2:Modem HW MODE:%#x, RC HW MODE:%#x, EP HW MODE:%#x, Clock gate:%#x, REQ_STA:%#x, REQ_CTRL:%#x, Sleep protect:%#x, pmrc regs:%#x\n",
		readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_4),
		readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_6),
		readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_8),
		readl_relaxed(port->pextpcfg + PCIE_PEXTP_CG_0),
		readl_relaxed(port->pextpcfg + PEXTP_RES_REQ_STA),
		readl_relaxed(port->pextpcfg + PEXTP_REQ_CTRL),
		readl_relaxed(port->pextpcfg + PEXTP_SLPPROT_RDY),
		val);
}

static void mtk_pcie_save_restore_l1ss(struct mtk_pcie_port *port, bool save)
{
	struct pci_dev *pdev = port->pcidev;
	u16 l1ss_cap;

	l1ss_cap = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap) {
		dev_info(port->dev, "Can't find L1SS capability\n");
		return;
	}

	if (save) {
		pci_read_config_dword(pdev, l1ss_cap + PCI_L1SS_CTL2, &port->saved_l1ss_ctl2);
		pci_read_config_dword(pdev, l1ss_cap + PCI_L1SS_CTL1, &port->saved_l1ss_ctl1);
		dev_info(port->dev, "Save L1SS CTL1=%#x, CTL2=%#x\n", port->saved_l1ss_ctl1,
				port->saved_l1ss_ctl2);
	} else {
		pci_write_config_dword(pdev, l1ss_cap + PCI_L1SS_CTL2, port->saved_l1ss_ctl2);
		pci_write_config_dword(pdev, l1ss_cap + PCI_L1SS_CTL1, port->saved_l1ss_ctl1);
		dev_info(port->dev, "Restore L1SS CTL1=%#x, CTL2=%#x\n", port->saved_l1ss_ctl1,
				port->saved_l1ss_ctl2);
	}
}

static void mtk_pcie_save_restore_cfg(struct mtk_pcie_port *port, bool save)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	struct pci_dev *pdev;
	unsigned long flags;

	if (!port->pcidev) {
		port->pcidev = pci_get_slot(host->bus, 0);
		if (!port->pcidev) {
			dev_info(port->dev, "port->pcidev is NULL, pci_get_slot failed!\n");
			return;
		}
	}

	pdev = port->pcidev;
	spin_lock_irqsave(&port->cfg_lock, flags);

	if (port->cfg_saved && save) {
		dev_info(port->dev, "Aleady saved config space, exit\n");
		spin_unlock_irqrestore(&port->cfg_lock, flags);
		return;
	}

	dev_info(port->dev, "Preparing %s config space\n", save ? "save" : "restore");

	mtk_pcie_save_restore_l1ss(port, save);

	if (save) {
		port->cfg_saved = true;
		pci_save_state(pdev);
	} else {
		pci_restore_state(pdev);
		port->cfg_saved = false;
	}

	spin_unlock_irqrestore(&port->cfg_lock, flags);
}

/*
 * mtk_pcie_clkbuf_force_bbck2() - Switch BBCK2 to SW mode or HW mode
 * @port: PCIe port information
 * @enable: True is SW mode, false is HW mode
 *
 * SW mode will always on BBCK2, HW mode will be controlled by HW
 */
static void mtk_pcie_clkbuf_force_bbck2(struct mtk_pcie_port *port, bool enable)
{
	static int count;
	int err = 0;

	if (!port)
		return;

	if (enable) {
		if (++count > 1) {
			dev_info(port->dev, "PCIe BBCK2 already enabled, count = %d\n", count);
			return;
		}
	} else {
		if (count == 0) {
			dev_info(port->dev, "PCIe BBCK2 already disabled\n");
			return;
		}

		if (--count) {
			dev_info(port->dev, "PCIe BBCK2 has user, count = %d\n", count);
			return;
		}
	}

	dev_info(port->dev, "Current PCIe BBCK2 count = %d\n", count);
	err = clkbuf_srclken_ctrl(enable ? "RC_FPM_REQ" : "RC_NONE_REQ",
				  PCIE_CLKBUF_SUBSYS_ID);
	if (err)
		dev_info(port->dev, "PCIe fail to request BBCK2\n");
}

/*
 * mtk_pcie_clkbuf_force_26m() - Force 26M request
 * @port: PCIe port information
 * @enable: True is force 26M request, false is disable force 26M request
 *
 * BBCK2 will always use HW mode, but force 26M request always on BBCK2.
 * If disable force 26M request, BBCK2 will be controlled by HW
 */
static void mtk_pcie_clkbuf_force_26m(struct mtk_pcie_port *port, bool enable)
{
	u32 val;

	val = readl_relaxed(port->pextpcfg + PEXTP_REQ_CTRL);

	/* only port0 use BBCK2 in 6991 */
	if (!port->port_num) {
		/* keep BBCK2 request to 1 when swicth PMRC7 mode */
		val |= PCIE_26M_REQ_FORCE_ON;
		writel_relaxed(val, port->pextpcfg + PEXTP_REQ_CTRL);
		mtk_pcie_clkbuf_force_bbck2(port, enable);
	}

	if (enable)
		val |= PCIE_26M_REQ_FORCE_ON;
	else
		val &= ~PCIE_26M_REQ_FORCE_ON;

	writel_relaxed(val, port->pextpcfg + PEXTP_REQ_CTRL);

	mtk_pcie_dump_pextp_info(port);
}

/*
 * mtk_pcie_adjust_cplto_scale() - Adujst cplto timeout
 * @port: PCIe port information
 * @cplto: modify the Completion Timeout value(ms)
 * R2_1MS_10M/R5_16MS_55MS: the two ranges have adjustable precision by 0x1a8.
 */
static void mtk_pcie_adjust_cplto_scale(struct mtk_pcie_port *port, u32 cplto)
{
	u32 range = 0, scale = 0, value = 0;

	if (!port)
		return;

	if (cplto >= 1 && cplto <= 10) {
		range = R2_1MS_10MS;
	} else if (cplto >= 16 && cplto <= 55) {
		range = R5_16MS_55MS;
	} else {
		dev_info(port->dev, "Completion timeout value %d out of range\n", cplto);
		return;
	}

	value = readl_relaxed(port->base + PCIE_CONF_DEV2_CTL_STS);
	value &= ~PCIE_DCR2_CPL_TO;
	scale = readl_relaxed(port->base + PCIE_AXI_IF_CTRL);
	scale &= ~PCIE_SW_CPLTO_TIMER;

	switch (range) {
	case R2_1MS_10MS:
		value |= PCIE_CPL_TIMEOUT_4MS;
		scale |= cplto << PCIE_CPLTO_SCALE_R2_L;
		break;
	case R5_16MS_55MS:
		value |= PCIE_CPL_TIMEOUT_32MS;
		scale |= cplto << PCIE_CPLTO_SCALE_R5_L;
		break;
	default:
		dev_info(port->dev, "Undefined range: %d\n", range);
	}

	writel_relaxed(value, port->base + PCIE_CONF_DEV2_CTL_STS);
	writel_relaxed(scale, port->base + PCIE_AXI_IF_CTRL);
	dev_info(port->dev, "PCIe RC control 2 register=%#x, precision of timeout=%#x\n",
		readl_relaxed(port->base + PCIE_CONF_DEV2_CTL_STS),
		readl_relaxed(port->base + PCIE_AXI_IF_CTRL));
}

static int mtk_pcie_set_link_speed(struct mtk_pcie_port *port)
{
	u32 val;

	if ((port->max_link_speed < 1) || (port->port_num < 0))
		return -EINVAL;

	val = readl_relaxed(port->base + PCIE_BASE_CONF_REG);
	val = (val & PCIE_SUPPORT_SPEED_MASK) >> PCIE_SUPPORT_SPEED_SHIFT;
	if (val & BIT(port->max_link_speed - 1)) {
		val = readl_relaxed(port->base + PCIE_SETTING_REG);
		val &= ~PCIE_GEN_SUPPORT_MASK;

		if (port->max_link_speed > 1)
			val |= PCIE_GEN_SUPPORT(port->max_link_speed);

		writel_relaxed(val, port->base + PCIE_SETTING_REG);

		/* Set target speed */
		val = readl_relaxed(port->base + PCIE_CONF_EXP_LNKCTL2_REG);
		val &= ~PCIE_TARGET_SPEED_MASK;
		writel(val | port->max_link_speed,
		       port->base + PCIE_CONF_EXP_LNKCTL2_REG);

		return 0;
	}

	return -EINVAL;
}

static int mtk_pcie_startup_port(struct mtk_pcie_port *port)
{
	struct resource_entry *entry;
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	unsigned int table_index = 0;
	int err;
	u32 val;

	/* Set as RC mode */
	val = readl_relaxed(port->base + PCIE_SETTING_REG);
	val |= PCIE_RC_MODE;
	writel_relaxed(val, port->base + PCIE_SETTING_REG);

	/* Set class code */
	val = readl_relaxed(port->base + PCIE_PCI_IDS_1);
	val &= ~GENMASK(31, 8);
	val |= PCI_CLASS(PCI_CLASS_BRIDGE_PCI << 8);
	writel_relaxed(val, port->base + PCIE_PCI_IDS_1);

	port->data->clkbuf_control(port, true);

	if (port->data && port->data->pre_init) {
		err = port->data->pre_init(port);
		if (err)
			return err;
	}

	/* Mask all INTx interrupts */
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val &= ~PCIE_INTX_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);

	/* DVFSRC voltage request state */
	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val |= PCIE_DVFS_REQ_FORCE_ON;
	if (!port->dvfs_req_en) {
		val &= ~PCIE_DVFS_REQ_FORCE_ON;
		val |= PCIE_DVFS_REQ_FORCE_OFF;
	}
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	/* Set max link speed */
	err = mtk_pcie_set_link_speed(port);
	if (err)
		dev_info(port->dev, "unsupported speed: GEN%d\n",
			 port->max_link_speed);

	dev_info(port->dev, "PCIe link start ...\n");

	/* Assert all reset signals */
	val = readl_relaxed(port->base + PCIE_RST_CTRL_REG);
	val |= PCIE_MAC_RSTB | PCIE_PHY_RSTB | PCIE_BRG_RSTB | PCIE_PE_RSTB;
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	/*
	 * Described in PCIe CEM specification setctions 2.2 (PERST# Signal)
	 * and 2.2.1 (Initial Power-Up (G3 to S0)).
	 * The deassertion of PERST# should be delayed 100ms (TPVPERL)
	 * for the power and clock to become stable.
	 */
	msleep(50);

	/* De-assert reset signals */
	val &= ~(PCIE_MAC_RSTB | PCIE_PHY_RSTB | PCIE_BRG_RSTB);
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	msleep(50);

	/* De-assert PERST# */
	val &= ~PCIE_PE_RSTB;
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	err = readl_poll_timeout(port->base + PCIE_LINK_STATUS_REG, val,
				 !!(val & PCIE_PORT_LINKUP), 20,
				 PCI_PM_D3COLD_WAIT * USEC_PER_MSEC);
	if (err) {
		val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
		dev_info(port->dev, "PCIe link down, ltssm reg val: %#x\n", val);
		port->full_debug_dump = true;
		mtk_pcie_dump_link_info(port->port_num);
		port->full_debug_dump = false;
		/* only thinmodem not return error when soft on */
		if (!port->soft_off || !port->port_num)
			return err;
	} else {
		dev_info(port->dev, "PCIe linkup success ...\n");
	}

	mtk_pcie_enable_msi(port);

	/* Enable axi post error interrupt*/
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= PCIE_AXI_POST_ERR_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);

	/* PCIe read completion timeout is adjusted to 4ms */
	mtk_pcie_adjust_cplto_scale(port, PCIE_CPLTO_SCALE_4MS);

	if (port->data && port->data->post_init) {
		err = port->data->post_init(port);
		if (err)
			return err;
	}

	/* Set PCIe translation windows */
	resource_list_for_each_entry(entry, &host->windows) {
		struct resource *res = entry->res;
		unsigned long type = resource_type(res);
		resource_size_t cpu_addr;
		resource_size_t pci_addr;
		resource_size_t size;
		const char *range_type;

		if (type == IORESOURCE_IO) {
			cpu_addr = pci_pio_to_address(res->start);
			range_type = "IO";
		} else if (type == IORESOURCE_MEM) {
			cpu_addr = res->start;
			range_type = "MEM";
		} else {
			continue;
		}

		pci_addr = res->start - entry->offset;
		size = resource_size(res);
		err = mtk_pcie_set_trans_table(port, cpu_addr, pci_addr, size,
					       type, table_index);
		if (err)
			return err;

		dev_dbg(port->dev, "set %s trans window[%d]: cpu_addr = %#llx, pci_addr = %#llx, size = %#llx\n",
			range_type, table_index, (unsigned long long)cpu_addr,
			(unsigned long long)pci_addr, (unsigned long long)size);

		table_index++;
	}

	return 0;
}

static int mtk_pcie_set_affinity(struct irq_data *data,
				 const struct cpumask *mask, bool force)
{
	struct mtk_pcie_port *port = data->domain->host_data;
	struct irq_data *port_data = irq_get_irq_data(port->irq);
	struct irq_chip *port_chip;
	int ret;

	if (!port_data)
		return -EINVAL;

	port_chip = irq_data_get_irq_chip(port_data);
	if (!port_chip || !port_chip->irq_set_affinity)
		return -EINVAL;

	ret = port_chip->irq_set_affinity(port_data, mask, force);

	irq_data_update_effective_affinity(data, mask);

	return ret;
}

static void mtk_pcie_msi_irq_mask(struct irq_data *data)
{
	pci_msi_mask_irq(data);
	irq_chip_mask_parent(data);
}

static void mtk_pcie_msi_irq_unmask(struct irq_data *data)
{
	pci_msi_unmask_irq(data);
	irq_chip_unmask_parent(data);
}

static struct irq_chip mtk_msi_irq_chip = {
	.irq_ack = irq_chip_ack_parent,
	.irq_mask = mtk_pcie_msi_irq_mask,
	.irq_unmask = mtk_pcie_msi_irq_unmask,
	.name = "MSI",
};

static struct msi_domain_info mtk_msi_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		   MSI_FLAG_PCI_MSIX | MSI_FLAG_MULTI_PCI_MSI),
	.chip	= &mtk_msi_irq_chip,
};

static void mtk_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	msg->address_hi = upper_32_bits(msi_set->msg_addr);
	msg->address_lo = lower_32_bits(msi_set->msg_addr);
	msg->data = hwirq;
}

static void mtk_msi_bottom_irq_ack(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	writel_relaxed(BIT(hwirq), msi_set->base + PCIE_MSI_SET_STATUS_OFFSET);
}

static void mtk_msi_bottom_irq_mask(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq, flags;
	u32 val;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	val &= ~BIT(hwirq);
	writel_relaxed(val, msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mtk_msi_bottom_irq_unmask(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq, flags;
	u32 val;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	val |= BIT(hwirq);
	writel_relaxed(val, msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static struct irq_chip mtk_msi_bottom_irq_chip = {
	.irq_ack		= mtk_msi_bottom_irq_ack,
	.irq_mask		= mtk_msi_bottom_irq_mask,
	.irq_unmask		= mtk_msi_bottom_irq_unmask,
	.irq_compose_msi_msg	= mtk_compose_msi_msg,
	.irq_set_affinity	= mtk_pcie_set_affinity,
	.name			= "MSI",
};

static int mtk_msi_bottom_domain_alloc(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs,
				       void *arg)
{
	struct mtk_pcie_port *port = domain->host_data;
	struct mtk_msi_set *msi_set;
	int i, hwirq, set_idx;

	mutex_lock(&port->lock);

	hwirq = bitmap_find_free_region(port->msi_irq_in_use, PCIE_MSI_IRQS_NUM,
					order_base_2(nr_irqs));

	mutex_unlock(&port->lock);

	if (hwirq < 0)
		return -ENOSPC;

	set_idx = hwirq / PCIE_MSI_IRQS_PER_SET;
	msi_set = &port->msi_sets[set_idx];

	for (i = 0; i < nr_irqs; i++)
		irq_domain_set_info(domain, virq + i, hwirq + i,
				    &mtk_msi_bottom_irq_chip, msi_set,
				    handle_edge_irq, NULL, NULL);

	return 0;
}

static void mtk_msi_bottom_domain_free(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs)
{
	struct mtk_pcie_port *port = domain->host_data;
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);

	mutex_lock(&port->lock);

	bitmap_release_region(port->msi_irq_in_use, data->hwirq,
			      order_base_2(nr_irqs));

	mutex_unlock(&port->lock);

	irq_domain_free_irqs_common(domain, virq, nr_irqs);
}

static const struct irq_domain_ops mtk_msi_bottom_domain_ops = {
	.alloc = mtk_msi_bottom_domain_alloc,
	.free = mtk_msi_bottom_domain_free,
};

static void mtk_intx_mask(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val &= ~BIT(data->hwirq + PCIE_INTX_SHIFT);
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mtk_intx_unmask(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= BIT(data->hwirq + PCIE_INTX_SHIFT);
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

/**
 * mtk_intx_eoi() - Clear INTx IRQ status at the end of interrupt
 * @data: pointer to chip specific data
 *
 * As an emulated level IRQ, its interrupt status will remain
 * until the corresponding de-assert message is received; hence that
 * the status can only be cleared when the interrupt has been serviced.
 */
static void mtk_intx_eoi(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq = data->hwirq + PCIE_INTX_SHIFT;
	writel_relaxed(BIT(hwirq), port->base + PCIE_INT_STATUS_REG);
}

static struct irq_chip mtk_intx_irq_chip = {
	.irq_mask		= mtk_intx_mask,
	.irq_unmask		= mtk_intx_unmask,
	.irq_eoi		= mtk_intx_eoi,
	.irq_set_affinity	= mtk_pcie_set_affinity,
	.name			= "INTx",
};

static int mtk_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
			     irq_hw_number_t hwirq)
{
	irq_set_chip_data(irq, domain->host_data);
	irq_set_chip_and_handler_name(irq, &mtk_intx_irq_chip,
				      handle_fasteoi_irq, "INTx");
	return 0;
}

static const struct irq_domain_ops intx_domain_ops = {
	.map = mtk_pcie_intx_map,
};

static int mtk_pcie_init_irq_domains(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct device_node *intc_node, *node = dev->of_node;
	int ret;

	raw_spin_lock_init(&port->irq_lock);

	/* Setup INTx */
	intc_node = of_get_child_by_name(node, "interrupt-controller");
	if (!intc_node) {
		dev_err(dev, "missing interrupt-controller node\n");
		return -ENODEV;
	}

	port->intx_domain = irq_domain_add_linear(intc_node, PCI_NUM_INTX,
						  &intx_domain_ops, port);
	if (!port->intx_domain) {
		dev_err(dev, "failed to create INTx IRQ domain\n");
		return -ENODEV;
	}

	/* Setup MSI */
	mutex_init(&port->lock);

	port->msi_bottom_domain = irq_domain_add_linear(node, PCIE_MSI_IRQS_NUM,
				  &mtk_msi_bottom_domain_ops, port);
	if (!port->msi_bottom_domain) {
		dev_err(dev, "failed to create MSI bottom domain\n");
		ret = -ENODEV;
		goto err_msi_bottom_domain;
	}

	port->msi_domain = pci_msi_create_irq_domain(dev->fwnode,
						     &mtk_msi_domain_info,
						     port->msi_bottom_domain);
	if (!port->msi_domain) {
		dev_err(dev, "failed to create MSI domain\n");
		ret = -ENODEV;
		goto err_msi_domain;
	}

	return 0;

err_msi_domain:
	irq_domain_remove(port->msi_bottom_domain);
err_msi_bottom_domain:
	irq_domain_remove(port->intx_domain);

	return ret;
}

static void mtk_pcie_irq_teardown(struct mtk_pcie_port *port)
{
	irq_set_chained_handler_and_data(port->irq, NULL, NULL);

	if (port->intx_domain) {
		int virq, i;

		for (i = 0; i < PCI_NUM_INTX; i++) {
			virq = irq_find_mapping(port->intx_domain, i);
			if (virq > 0)
				irq_dispose_mapping(virq);
		}
		irq_domain_remove(port->intx_domain);
	}

	if (port->msi_domain)
		irq_domain_remove(port->msi_domain);

	if (port->msi_bottom_domain)
		irq_domain_remove(port->msi_bottom_domain);

	irq_dispose_mapping(port->irq);
}

static void mtk_pcie_msi_handler(struct mtk_pcie_port *port, int set_idx)
{
	struct mtk_msi_set *msi_set = &port->msi_sets[set_idx];
	unsigned long msi_enable, msi_status;
	irq_hw_number_t bit, hwirq;

	msi_enable = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);

	do {
		msi_status = readl_relaxed(msi_set->base +
					   PCIE_MSI_SET_STATUS_OFFSET);
		msi_status &= msi_enable;
		if (!msi_status)
			break;

		for_each_set_bit(bit, &msi_status, PCIE_MSI_IRQS_PER_SET) {
			hwirq = bit + set_idx * PCIE_MSI_IRQS_PER_SET;
			generic_handle_domain_irq(port->msi_bottom_domain, hwirq);
		}
	} while (true);
}

static void mtk_pcie_irq_handler(struct irq_desc *desc)
{
	struct mtk_pcie_port *port = irq_desc_get_handler_data(desc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	unsigned long status, int_enable;
	irq_hw_number_t irq_bit = PCIE_INTX_SHIFT;
	u32 cor_sta = 0, uncor_sta = 0;

	chained_irq_enter(irqchip, desc);

	status = readl_relaxed(port->base + PCIE_INT_STATUS_REG);
	int_enable = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	status &= int_enable;
	if (status & (PCIE_AXI_POST_ERR_EVT | PCIE_AER_EVT)) {
		if (port->port_num == 1) {
			pci_read_config_dword(port->pcidev, PCIE_AER_CO_STA, &cor_sta);
			pci_read_config_dword(port->pcidev, PCIE_AER_UNC_STA, &uncor_sta);
			dev_info(port->dev, "ltssm reg:%#x, link sta:%#x, axi err add:%#x, axi err info:%#x, cor:%#x, uncor:%#x\n",
				readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
				readl_relaxed(port->base + PCIE_LINK_STATUS_REG),
				readl_relaxed(port->base + PCIE_AXI0_ERR_ADDR_L),
				readl_relaxed(port->base + PCIE_AXI0_ERR_INFO),
				cor_sta, uncor_sta);

			if ((uncor_sta & PCI_ERR_UNC_COMP_TIME) || (status & PCIE_AXI_POST_ERR_EVT)) {
				mtk_pcie_disable_data_trans(port->port_num);
				int_enable = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
				int_enable &= ~PCIE_AER_EVT_EN;
				writel_relaxed(int_enable, port->base + PCIE_INT_ENABLE_REG);
			}
		}

		dev_info(port->dev, "PCIe error %#lx detected\n", status);
		mtk_pcie_dump_link_info(port->port_num);
		writel_relaxed(PCIE_AXI_POST_ERR_EVT, port->base + PCIE_INT_STATUS_REG);
	}

	for_each_set_bit_from(irq_bit, &status, PCI_NUM_INTX +
			      PCIE_INTX_SHIFT)
		generic_handle_domain_irq(port->intx_domain,
					  irq_bit - PCIE_INTX_SHIFT);

	irq_bit = PCIE_MSI_SHIFT;
	for_each_set_bit_from(irq_bit, &status, PCIE_MSI_SET_NUM +
			      PCIE_MSI_SHIFT) {
		mtk_pcie_msi_handler(port, irq_bit - PCIE_MSI_SHIFT);

		writel_relaxed(BIT(irq_bit), port->base + PCIE_INT_STATUS_REG);
	}

	chained_irq_exit(irqchip, desc);
}

static int mtk_pcie_setup_irq(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *eint_test;
	int err;

	err = mtk_pcie_init_irq_domains(port);
	if (err)
		return err;

	port->irq = platform_get_irq(pdev, 0);
	if (port->irq < 0)
		return port->irq;

	irq_set_chained_handler_and_data(port->irq, mtk_pcie_irq_handler, port);

	eint_test = of_parse_phandle(dev->of_node, "eint-irq", 0);
	if (eint_test) {
		port->eint_irq = of_irq_get(eint_test, 0);
		if (port->eint_irq <= 0)
			dev_info(dev, "Failed to get eint irq number, wakeup source for runtime suspend is not supported\n");

		dev_info(dev, "Got PCIe EINT irq: %d\n", port->eint_irq);
	}

	return 0;
}

static irqreturn_t mtk_pcie_eint_handler(int irq, void *data)
{
	struct mtk_pcie_port *port = data;
	struct device *dev = port->dev;
	struct pci_dev *ep_pdev;

	ep_pdev = pci_get_domain_bus_and_slot(port->port_num, 1, 0);
	if (ep_pdev) {
		dev_info(dev, "got %s\n", pci_name(ep_pdev));
		pm_request_resume(&ep_pdev->dev);
	}

	return IRQ_HANDLED;
}

static int mtk_pcie_request_eint_irq(struct mtk_pcie_port *port)
{
	int err;

	if (port->eint_irq <= 0)
		return -EINVAL;

	err = request_irq(port->eint_irq, mtk_pcie_eint_handler,
			  IRQF_TRIGGER_FALLING, "pcie-eint", port);
	if (err < 0) {
		dev_info(port->dev, "failed to request PCIe EINT irq\n");
		return err;
	}

	return 0;
}

static void mtk_pcie_free_eint_irq(struct mtk_pcie_port *port)
{
	if (port->eint_irq)
		free_irq(port->eint_irq, port);
}

static int mtk_pcie_parse_port(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *regs;
	struct device_node *node;
	int ret;

	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-mac");
	if (!regs)
		return -EINVAL;

	port->base = devm_ioremap_resource(dev, regs);
	if (IS_ERR(port->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(port->base);
	}

	port->reg_base = regs->start;

	port->port_num = of_get_pci_domain_nr(dev->of_node);
	if (port->port_num < 0) {
		dev_info(dev, "failed to get domain number\n");
		return port->port_num;
	}

	if (port->port_num < MTK_PCIE_MAX_PORT)
		pdev_list[port->port_num] = pdev;

	port->dvfs_req_en = true;
	ret = of_property_read_bool(dev->of_node, "mediatek,dvfs-req-dis");
	if (ret)
		port->dvfs_req_en = false;

	port->peri_reset_en = true;
	ret = of_property_read_bool(dev->of_node, "mediatek,peri-reset-dis");
	if (ret)
		port->peri_reset_en = false;

	node = of_parse_phandle(dev->of_node, "pextpcfg", 0);
	if (node) {
		port->pextpcfg = of_iomap(node, 0);
		of_node_put(node);
		if (IS_ERR(port->pextpcfg)) {
			ret = PTR_ERR(port->pextpcfg);
			port->pextpcfg = NULL;
			return ret;
		}
	}

	node = of_parse_phandle(dev->of_node, "vlpcfg", 0);
	if (node) {
		port->vlpcfg = of_iomap(node, 0);
		of_node_put(node);
		if (IS_ERR(port->vlpcfg)) {
			ret = PTR_ERR(port->vlpcfg);
			port->vlpcfg = NULL;
			return ret;
		}
	}

	node = of_parse_phandle(dev->of_node, "pmrc", 0);
	if (node) {
		port->pmrc = of_iomap(node, 1);
		of_node_put(node);
		if (IS_ERR(port->pmrc)) {
			ret = PTR_ERR(port->pmrc);
			port->pmrc = NULL;
			return ret;
		}
	}

	port->suspend_mode = LINK_STATE_L2;
	ret = of_property_read_bool(dev->of_node, "mediatek,suspend-mode-l12");
	if (ret)
		port->suspend_mode = LINK_STATE_ASPM_L12;

	port->phy_reset = devm_reset_control_get_optional_exclusive(dev, "phy");
	if (IS_ERR(port->phy_reset)) {
		ret = PTR_ERR(port->phy_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY reset\n");

		return ret;
	}

	port->mac_reset = devm_reset_control_get_optional_exclusive(dev, "mac");
	if (IS_ERR(port->mac_reset)) {
		ret = PTR_ERR(port->mac_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get MAC reset\n");

		return ret;
	}

	port->phy = devm_phy_optional_get(dev, "pcie-phy");
	if (IS_ERR(port->phy)) {
		ret = PTR_ERR(port->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY\n");

		return ret;
	}

	port->num_clks = devm_clk_bulk_get_all(dev, &port->clks);
	if (port->num_clks < 0) {
		dev_err(dev, "failed to get clocks\n");
		return port->num_clks;
	}

	port->max_link_speed = of_pci_get_max_link_speed(dev->of_node);
	if (port->max_link_speed > 0)
		dev_info(dev, "max speed to GEN%d\n", port->max_link_speed);

	ret = of_property_read_bool(dev->of_node, "mediatek,runtime-suspend");
	if (ret)
		port->rpm = true;

	return 0;
}

static int mtk_pcie_peri_reset(struct mtk_pcie_port *port, bool enable)
{
	struct arm_smccc_res res;
	struct device *dev = port->dev;

	arm_smccc_smc(MTK_SIP_KERNEL_PCIE_CONTROL, port->port_num, enable,
		      0, 0, 0, 0, 0, &res);

	if (res.a0)
		dev_info(dev, "Can't %s sw reset through SMC call\n",
			 enable ? "set" : "clear");

	return res.a0;
}

static int match_any(struct device *dev, void *unused)
{
	return 1;
}

static int mtk_pcie_power_up(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	int err;

	/* Clear PCIe sw reset bit */
	if (port->peri_reset_en) {
		err = mtk_pcie_peri_reset(port, false);
		if (err) {
			dev_info(dev, "failed to clear PERI reset control bit\n");
			return err;
		}
	}

	/* PHY init update short reach setting requires pipe clock,
	 * and pipe clock depends on mac resource ready, so power on MAC first
	 */
	reset_control_deassert(port->mac_reset);
	/* MAC power on and enable transaction layer clocks */
	if (!device_find_child(dev, NULL, match_any)) {
		pm_runtime_enable(dev);
		err = pm_runtime_get_sync(dev);
		if (err)
			dev_info(dev, "put_ret:%d, rpm_error:%d\n", err, dev->power.runtime_error);
	}

	err = clk_bulk_prepare_enable(port->num_clks, port->clks);
	if (err) {
		dev_info(dev, "failed to enable clocks\n");
		goto err_clk_init;
	}

	reset_control_deassert(port->phy_reset);

	/* PHY power on and enable pipe clock */
	err = phy_init(port->phy);
	if (err) {
		dev_err(dev, "failed to initialize PHY\n");
		goto err_phy_init;
	}

	err = phy_power_on(port->phy);
	if (err) {
		dev_err(dev, "failed to power on PHY\n");
		goto err_phy_on;
	}

	return 0;

err_phy_on:
	phy_exit(port->phy);
err_phy_init:
	reset_control_assert(port->phy_reset);
	clk_bulk_disable_unprepare(port->num_clks, port->clks);
err_clk_init:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	reset_control_assert(port->mac_reset);

	return err;
}

static void mtk_pcie_power_down(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	int err = 0;

	clk_bulk_disable_unprepare(port->num_clks, port->clks);

	if (!device_find_child(dev, NULL, match_any)) {
		err = pm_runtime_put_sync(dev);
		if (err)
			dev_info(dev, "put_ret:%d, rpm_error:%d\n", err, dev->power.runtime_error);

		pm_runtime_disable(dev);
	}

	reset_control_assert(port->mac_reset);

	phy_power_off(port->phy);
	phy_exit(port->phy);
	reset_control_assert(port->phy_reset);

	/* Set PCIe sw reset bit */
	if (port->peri_reset_en)
		mtk_pcie_peri_reset(port, true);

	port->data->clkbuf_control(port, false);
}

static int mtk_pcie_setup(struct mtk_pcie_port *port)
{
	int err;

	err = mtk_pcie_parse_port(port);
	if (err)
		return err;

	/* Don't touch the hardware registers before power up */
	err = mtk_pcie_power_up(port);
	if (err)
		return err;

	/*
	 * Leroy + Falcon SDES issue workaround, switch pinmux after
	 * PCIe RC MTCMOS on completed for keep PERST low
	 */
	pinctrl_select_default_state(port->dev);

	/* Try link up */
	err = mtk_pcie_startup_port(port);
	if (err)
		goto err_setup;

	err = mtk_pcie_setup_irq(port);
	if (err)
		goto err_setup;

	if (port->pextpcfg) {
		spin_lock_init(&port->vote_lock);
		port->ep_hw_mode_en = false;
		port->rc_hw_mode_en = false;
	}

	spin_lock_init(&port->cfg_lock);

	device_enable_async_suspend(port->dev);

	return 0;

err_setup:
	mtk_pcie_power_down(port);

	return err;
}

static void mtk_pcie_enable_host_bridge_rpm(struct mtk_pcie_port *port)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);

	pm_runtime_forbid(&host->dev);
	pm_runtime_set_active(&host->dev);

	pm_runtime_enable(&host->dev);
	pm_runtime_allow(&host->dev);
	pm_runtime_put_noidle(port->dev);
}

static void mtk_pcie_disable_host_bridge_rpm(struct mtk_pcie_port *port)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);

	pm_runtime_get_noresume(port->dev);
	pm_runtime_forbid(&host->dev);
	pm_runtime_dont_use_autosuspend(&host->dev);
}

static int __maybe_unused avoid_kmemleak_false_alarm(struct pci_dev *dev,
						     void *data)
{
	kmemleak_not_leak(dev);

	return 0;
}

static void mtk_pcie_avoid_kmemleak_false_alarm(struct pci_host_bridge *host)
{
	struct pci_bus_resource *bus_res;

	kmemleak_not_leak(host);
	kmemleak_not_leak(&host->dev);
	kmemleak_not_leak(host->bus);

	list_for_each_entry(bus_res, &host->bus->resources, list)
		kmemleak_not_leak(bus_res);

	pci_walk_bus(host->bus, avoid_kmemleak_false_alarm, NULL);
}

static int mtk_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pcie_port *port;
	struct pci_host_bridge *host;
	int err;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*port));
	if (!host)
		return -ENOMEM;

	port = pci_host_bridge_priv(host);

	port->dev = dev;
	port->data = (struct mtk_pcie_data *)of_device_get_match_data(dev);
	platform_set_drvdata(pdev, port);

	err = mtk_pcie_setup(port);
	if (err)
		goto err_probe;

	host->ops = &mtk_pcie_ops;
	host->sysdata = port;

	err = pci_host_probe(host);
	if (err) {
		mtk_pcie_irq_teardown(port);
		mtk_pcie_power_down(port);
		goto err_probe;
	}

	mtk_pcie_avoid_kmemleak_false_alarm(host);

	if (!port->pcidev)
		port->pcidev = pci_get_slot(host->bus, 0);

	if (port->rpm)
		mtk_pcie_enable_host_bridge_rpm(port);
	else if (port->pcidev->bridge_d3)
		port->pcidev->bridge_d3 = false;

	return 0;

err_probe:
	if (port->pextpcfg)
		iounmap(port->pextpcfg);

	if (port->vlpcfg)
		iounmap(port->vlpcfg);

	if (port->pmrc)
		iounmap(port->pmrc);

	if (mtk_pcie_pinmux_select(port->port_num, PCIE_PINMUX_PD))
		pinctrl_pm_select_sleep_state(&pdev->dev);

	return err;
}

static int mtk_pcie_remove(struct platform_device *pdev)
{
	struct mtk_pcie_port *port = platform_get_drvdata(pdev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	int err = 0;

	if (port->rpm)
		mtk_pcie_disable_host_bridge_rpm(port);

	pci_lock_rescan_remove();
	pci_stop_root_bus(host->bus);
	pci_remove_root_bus(host->bus);
	pci_unlock_rescan_remove();

	mtk_pcie_irq_teardown(port);

	err = pinctrl_pm_select_sleep_state(&pdev->dev);
	if (err)
		dev_info(&pdev->dev, "Failed to set PCIe pins sleep state\n");

	mtk_pcie_power_down(port);

	if (port->pextpcfg)
		iounmap(port->pextpcfg);

	if (port->vlpcfg)
		iounmap(port->vlpcfg);

	if (port->pmrc)
		iounmap(port->pmrc);

	if (port->pcidev)
		pci_dev_put(port->pcidev);

	return err;
}

static struct platform_device *mtk_pcie_find_pdev_by_port(int port)
{
	struct platform_device *pdev = NULL;

	if ((port >= 0) && (port < MTK_PCIE_MAX_PORT) && pdev_list[port])
		pdev = pdev_list[port];

	return pdev;
}

int mtk_pcie_probe_port(int port)
{
	struct platform_device *pdev;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("pcie platform device not found!\n");
		return -ENODEV;
	}

	if (device_attach(&pdev->dev) <= 0) {
		device_release_driver(&pdev->dev);
		pr_info("%s: pcie probe fail!\n", __func__);
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_probe_port);

int mtk_pcie_remove_port(int port)
{
	struct platform_device *pdev;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("pcie platform device not found!\n");
		return -ENODEV;
	}

	mtk_pcie_dump_link_info(port);

	device_release_driver(&pdev->dev);

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_remove_port);

int mtk_pcie_pinmux_select(int port_num, enum pin_state state)
{
	struct platform_device *pdev;
	struct pinctrl *p;

	pdev = mtk_pcie_find_pdev_by_port(port_num);
	if (!pdev) {
		pr_info("PCIe%d platform device not found!\n", port_num);
		return -ENODEV;
	}

	switch (state) {
	case PCIE_PINMUX_PD:
		dev_info(&pdev->dev, "PCIe pinmux switching to pull-down state\n");
		p = pinctrl_get_select(&pdev->dev, "pd");
		if (IS_ERR(p)) {
			dev_info(&pdev->dev, "PCIe pinmux select pull-down state failed\n");
			return PTR_ERR(p);
		}

		pinctrl_put(p);

		break;
	case PCIE_PINMUX_HIZ:
		dev_info(&pdev->dev, "PCIe pinmux switching to Hi-Z state\n");
		p = pinctrl_get_select(&pdev->dev, "hiz");
		if (IS_ERR(p)) {
			dev_info(&pdev->dev, "PCIe pinmux select Hi-Z state failed\n");
			return PTR_ERR(p);
		}

		pinctrl_put(p);

		break;
	default:
		dev_info(&pdev->dev, "Pinmux %d not support\n", state);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_pinmux_select);

/* Set partition when use PCIe MAC debug probe table */
static void mtk_pcie_mac_dbg_set_partition(struct mtk_pcie_port *port, u32 partition)
{
	writel_relaxed(partition, port->base + PCIE_DEBUG_SEL_1);
}

/* Read the PCIe MAC internal signal corresponding to the debug probe table bus */
static void mtk_pcie_mac_dbg_read_bus(struct mtk_pcie_port *port, u32 bus)
{
	writel_relaxed(bus, port->base + PCIE_DEBUG_SEL_0);
	pr_info("PCIe debug table: bus=%#x, partition=%#x, monitor=%#x\n",
		readl_relaxed(port->base + PCIE_DEBUG_SEL_0),
		readl_relaxed(port->base + PCIE_DEBUG_SEL_1),
		readl_relaxed(port->base + PCIE_DEBUG_MONITOR));
}

/* Dump PCIe MAC signal*/
static void mtk_pcie_monitor_mac(struct mtk_pcie_port *port)
{
	u32 val;

	/* Dump the debug table probe when AER event occurs */
	val = readl_relaxed(port->base + PCIE_INT_STATUS_REG);
	if ((val & PCIE_AER_EVT) || port->full_debug_dump) {
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x0, 0x0, 0x0, 0x0));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x0a, 0x0b, 0x15, 0x16));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x1, 0x1, 0x1, 0x1));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x04, 0x0a, 0x0b, 0x15));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x2, 0x2, 0x2, 0x2));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x04, 0x05, 0x06, 0x07));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x80, 0x81, 0x82, 0x87));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x3, 0x3, 0x3, 0x3));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x00, 0x01, 0x07, 0x16));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x0b, 0x0c, 0x0d, 0x0e));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x15, 0x1a, 0x00, 0x00));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x51, 0x52, 0x54, 0x55));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x4, 0x4, 0x4, 0x4));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0xc9, 0xca, 0xcb, 0xcd));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x5, 0x5, 0x5, 0x5));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x08, 0x09, 0x0a, 0x0b));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x0c, 0x0d, 0x0e, 0x0f));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0xc, 0xc, 0xc, 0xc));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x45, 0x47, 0x48, 0x49));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x4a, 0x4b, 0x4c, 0x4d));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x46, 0x51, 0x52, 0x0));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x5c, 0x5d, 0x5e, 0x0));
	}

	pr_info("Port%d, ltssm reg:%#x, link sta:%#x, power sta:%#x, LP ctrl:%#x, IP basic sta:%#x, int sta:%#x, msi set0 sta: %#x, msi set1 sta: %#x, axi err add:%#x, axi err info:%#x, spm res ack=%#x, adt pending sta:=%#x, err addr_l=%#x, err addr_h=%#x, err info=%#x, IF_CTRL=%#x, phy err=%#x\n",
		port->port_num,
		readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
		readl_relaxed(port->base + PCIE_LINK_STATUS_REG),
		readl_relaxed(port->base + PCIE_ISTATUS_PM),
		readl_relaxed(port->base + PCIE_LOW_POWER_CTRL),
		readl_relaxed(port->base + PCIE_BASIC_STATUS),
		readl_relaxed(port->base + PCIE_INT_STATUS_REG),
		readl_relaxed(port->base + PCIE_MSI_SET_BASE_REG +
			      PCIE_MSI_SET_STATUS_OFFSET),
		readl_relaxed(port->base + PCIE_MSI_SET_BASE_REG +
			      PCIE_MSI_SET_OFFSET +
			      PCIE_MSI_SET_STATUS_OFFSET),
		readl_relaxed(port->base + PCIE_AXI0_ERR_ADDR_L),
		readl_relaxed(port->base + PCIE_AXI0_ERR_INFO),
		readl_relaxed(port->base + PCIE_RES_STATUS),
		readl_relaxed(port->base + PCIE_ISTATUS_PENDING_ADT),
		readl_relaxed(port->base + PCIE_ERR_ADDR_L),
		readl_relaxed(port->base + PCIE_ERR_ADDR_H),
		readl_relaxed(port->base + PCIE_ERR_INFO),
		readl_relaxed(port->base + PCIE_AXI_IF_CTRL),
		readl_relaxed(port->base + PHY_ERR_DEBUG_LANE0));

	/* Clear LTSSM record info after dump */
	writel_relaxed(PCIE_LTSSM_STATE_CLEAR, port->base + PCIE_LTSSM_STATUS_REG);
	/* Clear AXI info after dump */
	writel_relaxed(PCIE_ERR_STS_CLEAR, port->base + PCIE_AXI0_ERR_INFO);

	mtk_pcie_dump_pextp_info(port);
}

static int mtk_pcie_sleep_protect_status(struct mtk_pcie_port *port)
{
	/* Determine sleep protect by check SPM regs(6985/6989) */
	if (port->vlpcfg)
		return readl_relaxed(port->vlpcfg + PCIE_VLP_AXI_PROTECT_STA) &
		       PCIE_SUM_SLP_READY(port->port_num);

	/* Determine sleep protect by check pextp regs(6991) */
	return readl_relaxed(port->pextpcfg + PEXTP_SLPPROT_RDY) &
		       PEXTP_SUM_SLP_READY(port->port_num);
}

static bool mtk_pcie_sleep_protect_ready(struct mtk_pcie_port *port)
{
	u32 sleep_protect;

	sleep_protect = mtk_pcie_sleep_protect_status(port);
	if (!sleep_protect) {
		/*
		 * Sleep-protect signal will be de-asserted about 2.59us and
		 * asserted again in HW MTCMOS-on flow. If we run subsequent
		 * flow immediately after seen sleep-protect ready, it may
		 * cause unexcepted errors.
		 * Add SW debounce time here to avoid that corner case and
		 * check again.
		 */
		udelay(6);

		sleep_protect = mtk_pcie_sleep_protect_status(port);
		if (!sleep_protect)
			return true;
	}

	dev_info(port->dev, "PCIe%d sleep protect not ready = %#x\n",
		 port->port_num, sleep_protect);

	return false;
}

#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
static void pcie_android_rvh_do_serror(void *data, struct pt_regs *regs,
				       unsigned int esr, int *ret)
{
	struct mtk_pcie_port *pcie_port;
	struct platform_device *pdev;
	u32 val, i;

	for (i = 0; i < MTK_PCIE_MAX_PORT; i++) {
		pdev = mtk_pcie_find_pdev_by_port(i);
		if (!pdev)
			continue;

		pcie_port = platform_get_drvdata(pdev);
		if (!pcie_port)
			continue;

		pr_info("PCIe%d port found\n", pcie_port->port_num);

		if (!mtk_pcie_sleep_protect_ready(pcie_port))
			continue;

		/* Debug monitor pcie design internal signal */
		writel_relaxed(0x80810001, pcie_port->base + PCIE_DEBUG_SEL_0);
		writel_relaxed(0x22330100, pcie_port->base + PCIE_DEBUG_SEL_1);
		pr_info("Port%d debug recovery:%#x\n", pcie_port->port_num,
			readl_relaxed(pcie_port->base + PCIE_DEBUG_MONITOR));

		pr_info("Port%d ltssm reg: %#x, PCIe interrupt status=%#x, AXI0 ERROR address=%#x, AXI0 ERROR status=%#x\n",
			pcie_port->port_num,
			readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG),
			readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG),
			readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_ADDR_L),
			readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_INFO));

		val = readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG);
		if (val & PCIE_AXI_READ_ERR) {
			*ret = 1;
			dump_stack();
		}

		if (pcie_port->port_num == 1)
			writel(BIT(0), pcie_port->base + PCIE_AXI0_ERR_INFO);
	}
}
#endif

/**
 * mtk_pcie_dump_link_info() - Dump PCIe RC information
 * @port: The port number which EP use
 * @ret_val: bit[4:0]: LTSSM state (PCIe MAC offset 0x150 bit[28:24])
 *           bit[5]: DL_UP state (PCIe MAC offset 0x154 bit[8])
 *           bit[6]: Completion timeout status (PCIe MAC offset 0x184 bit[18])
 *                   AXI fetch error (PCIe MAC offset 0x184 bit[17])
 *           bit[7]: RxErr
 *           bit[8]: MalfTLP
 *           bit[9]: Driver own irq status (MSI set 1 bit[27])
 *           bit[10]: DL_UP exit (SDES) event
 */
u32 mtk_pcie_dump_link_info(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val, ret_val = 0;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return 0;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return 0;
	}

	/* Check the sleep protect ready */
	if (!mtk_pcie_sleep_protect_ready(pcie_port))
		return 0;

	mtk_pcie_monitor_mac(pcie_port);

	val = readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG);
	ret_val |= PCIE_LTSSM_STATE(val);
	val = readl_relaxed(pcie_port->base + PCIE_LINK_STATUS_REG);
	ret_val |= (val >> 3) & BIT(5);

	/* AXI read request error: AXI fetch error and completion timeout */
	val = readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG);
	if (val & PCIE_AXI_READ_ERR)
		ret_val |= BIT(6);

	if (val & PCIE_ERR_RST_EVT)
		ret_val |= BIT(10);

	if ((val & PCIE_AER_EVT) || pcie_port->full_debug_dump) {
		pcie_port->phy->ops->calibrate(pcie_port->phy);
		writel_relaxed(PCIE_AER_EVT,
			       pcie_port->base + PCIE_INT_STATUS_REG);
	}

	/* PCIe RxErr */
	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) |
	      PCIE_CFG_HEADER(0, 0);
	writel_relaxed(val, pcie_port->base + PCIE_CFGNUM_REG);
	val = readl_relaxed(pcie_port->base + PCIE_AER_CO_STATUS);
	if (val & AER_CO_RE)
		ret_val |= BIT(7);

	val = readl_relaxed(pcie_port->base + PCIE_AER_UNC_STATUS);
	if (val & PCIE_AER_UNC_MTLP)
		ret_val |= BIT(8);

	if (val & PCI_ERR_UNC_COMP_TIME)
		ret_val |= BIT(6);

	val = readl_relaxed(pcie_port->base + PCIE_MSI_SET_BASE_REG +
			    PCIE_MSI_SET_OFFSET + PCIE_MSI_SET_STATUS_OFFSET);
	if (val & DRIVER_OWN_IRQ_STATUS)
		ret_val |= BIT(9);

	dev_info(pcie_port->dev, "dump info return = %#x\n", ret_val);

	return ret_val;
}
EXPORT_SYMBOL(mtk_pcie_dump_link_info);

/**
 * mtk_pcie_disable_data_trans - Block pcie
 * and do not accept any data packet transmission.
 * @port: The port number which EP use
 */
int mtk_pcie_disable_data_trans(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return -ENODEV;
	}

	/* Check the sleep protect ready */
	if (!mtk_pcie_sleep_protect_ready(pcie_port))
		return -EPERM;

	val = readl_relaxed(pcie_port->base + PCIE_CFGCTRL);
	if (val & PCIE_DISABLE_LTSSM) {
		pr_info("Already disable data trans, config control(0x84)=%#x\n", val);
		return 0;
	}

	/* only datacard need save config space */
	if (pcie_port->port_num == 1)
		mtk_pcie_save_restore_cfg(pcie_port, true);

	val = readl_relaxed(pcie_port->base + PCIE_RST_CTRL_REG);
	val |= (PCIE_MAC_RSTB | PCIE_PHY_RSTB);
	writel_relaxed(val, pcie_port->base + PCIE_RST_CTRL_REG);

	val = readl_relaxed(pcie_port->base + PCIE_CFGCTRL);
	val |= PCIE_DISABLE_LTSSM;
	writel_relaxed(val, pcie_port->base + PCIE_CFGCTRL);

	val = readl_relaxed(pcie_port->base + PCIE_RST_CTRL_REG);
	val &= ~PCIE_MAC_RSTB;
	writel_relaxed(val, pcie_port->base + PCIE_RST_CTRL_REG);

	/*
	 * Set completion timeout to 64us to avoid corner case
	 * PCIe received a read command from AP but set MAC_RESET=1 before
	 * reply response signal to bus. After set MAC_RESET=0 again,
	 * completion timeout setting will be reset to default value(50ms).
	 * Then internal timer will keep counting until it achieve 50ms limit,
	 * and will cause bus tracker timeout.
	 * (note: bus tracker timeout = 5ms).
	 */
	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) |
	      PCIE_CFG_HEADER(0, 0);
	writel_relaxed(val, pcie_port->base + PCIE_CFGNUM_REG);
	val = readl_relaxed(pcie_port->base + PCIE_CONF_DEV2_CTL_STS);
	val &= ~PCIE_DCR2_CPL_TO;
	val |= PCIE_CPL_TIMEOUT_64US;
	writel_relaxed(val, pcie_port->base + PCIE_CONF_DEV2_CTL_STS);

	pr_info("reset control signal(0x148)=%#x, IP config control(0x84)=%#x\n",
		readl_relaxed(pcie_port->base + PCIE_RST_CTRL_REG),
		readl_relaxed(pcie_port->base + PCIE_CFGCTRL));

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_disable_data_trans);

static void __iomem *mtk_pcie_find_group_addr(struct mtk_pcie_port *port, u32 set, u32 group)
{
	struct mtk_msi_set *msi_set = &port->msi_sets[set];
	void __iomem *addr = NULL;

	switch (group) {
	case 0:
		addr = msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET;
		break;
	case 1:
		addr = msi_set->base + PCIE_MSI_SET_ENABLE_GRP1_OFFSET;
		break;
	case 2:
		addr = port->base + PCIE_MSI_GRP2_SET_OFFSET +
		       PCIE_MSI_GRPX_PER_SET_OFFSET * set;
		break;
	case 3:
		addr = port->base + PCIE_MSI_GRP3_SET_OFFSET +
		       PCIE_MSI_GRPX_PER_SET_OFFSET * set;
		break;
	default:
		dev_info(port->dev, "Group %d out of max range\n", group);
	}

	return addr;
}

/**
 * mtk_msi_unmask_to_other_mcu() - Unmask msi dispatch to other mcu
 * @data: The irq_data of virq
 * @group: MSI will dispatch to which group number
 */
int mtk_msi_unmask_to_other_mcu(struct irq_data *data, u32 group)
{
	struct irq_data *parent_data = data->parent_data;
	struct mtk_msi_set *msi_set;
	struct mtk_pcie_port *port;
	void __iomem *dest_addr;
	unsigned long hwirq;
	u32 val, set_num;

	if (!parent_data)
		return -EINVAL;

	msi_set = irq_data_get_irq_chip_data(parent_data);
	if (!msi_set)
		return -ENODEV;

	port = parent_data->domain->host_data;
	hwirq = parent_data->hwirq % PCIE_MSI_IRQS_PER_SET;
	set_num = parent_data->hwirq / PCIE_MSI_IRQS_PER_SET;
	dest_addr = mtk_pcie_find_group_addr(port, set_num, group);
	if (!dest_addr)
		return -EINVAL;

	val = readl_relaxed(dest_addr);
	val |= BIT(hwirq);
	writel_relaxed(val, dest_addr);

	dev_info(port->dev, "group=%d, hwirq=%ld, SET num=%d, Enable status=%#x\n",
		group, hwirq, set_num, readl_relaxed(dest_addr));

	return 0;
}
EXPORT_SYMBOL(mtk_msi_unmask_to_other_mcu);

static void __maybe_unused mtk_pcie_irq_save(struct mtk_pcie_port *port)
{
	u32 i, j;
	unsigned long flags;
	void __iomem *addr;

	raw_spin_lock_irqsave(&port->irq_lock, flags);

	port->saved_irq_state = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];
		for (j = 0; j < PCIE_MSI_GROUP_NUM; j++) {
			addr = mtk_pcie_find_group_addr(port, i, j);
			if (!addr)
				break;

			msi_set->saved_irq_state[j] = readl_relaxed(addr);
		}
	}

	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void __maybe_unused mtk_pcie_irq_restore(struct mtk_pcie_port *port)
{
	u32 i, j;
	unsigned long flags;
	void __iomem *addr;

	raw_spin_lock_irqsave(&port->irq_lock, flags);

	writel_relaxed(port->saved_irq_state, port->base + PCIE_INT_ENABLE_REG);

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];
		for (j = 0; j < PCIE_MSI_GROUP_NUM; j++) {
			addr = mtk_pcie_find_group_addr(port, i, j);
			if (!addr)
				break;

			writel_relaxed(msi_set->saved_irq_state[j], addr);
		}
	}

	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static int __maybe_unused mtk_pcie_turn_off_link(struct mtk_pcie_port *port)
{
	u32 val;
	int ret;

	/* Clear LTSSM record for enter L2 fail debug */
	writel_relaxed(PCIE_LTSSM_STATE_CLEAR, port->base + PCIE_LTSSM_STATUS_REG);

	val = readl_relaxed(port->base + PCIE_ICMD_PM_REG);
	val |= PCIE_TURN_OFF_LINK;
	writel_relaxed(val, port->base + PCIE_ICMD_PM_REG);

	/* Check the link is L2 */
	ret = readl_poll_timeout(port->base + PCIE_LTSSM_STATUS_REG, val,
				  (PCIE_LTSSM_STATE(val) ==
				   PCIE_LTSSM_STATE_L2_IDLE), 20,
				   50 * USEC_PER_MSEC);
	if (ret) {
		/* Need clear the turn_off_link bit */
		val = readl_relaxed(port->base + PCIE_ICMD_PM_REG);
		val &= ~PCIE_TURN_OFF_LINK;
		writel_relaxed(val, port->base + PCIE_ICMD_PM_REG);

		val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
		dev_info(port->dev, "Can't enter L2 state, LTSSM=%#x\n", val);

		/* Dump WAKE and P2 signal for clarify the problem quickly */
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0xc, 0xc, 0xc, 0xc));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x45, 0x48, 0x4d, 0x59));

		return ret;
	}

	dev_info(port->dev, "Enter L2 state successfully");

	return ret;
}

static void mtk_pcie_enable_hw_control(struct mtk_pcie_port *port, bool enable)
{
	u32 val;
	void __iomem *addr;

	if (port->port_num > 1)
		return;

	if (port->port_num == 0)
		addr = port->pextpcfg + PEXTP_PWRCTL_0;
	else
		addr = port->pextpcfg + PEXTP_PWRCTL_1;

	val = readl_relaxed(addr);
	if (enable)
		val |= PCIE_HW_MTCMOS_EN;
	else
		val &= ~PCIE_HW_MTCMOS_EN;

	writel_relaxed(val, addr);

	if (enable)
		mtk_pcie_dump_pextp_info(port);
}

/*
 * mtk_pcie_control_vote_v1() - V1 Vote mechanism
 * @port: which port control hw mode
 * @hw_mode_en: vote mechanism, true: agree open hw mode;
 *              false: disagree open hw mode
 * @who: 0 is RC, 1 is EP
 * V1 PEXTP has two bits to control whether to enter PCIe RTFF HW mode.
 * MD core uses one bit, AP core uses another bit. When both bits are set to 1,
 * PCIe will enter RTFF HW mode. This function implements an AP core software
 * voting mechanism. The RC and EP driver jointly determine the voting value of the AP core.
 */
static int mtk_pcie_control_vote_v1(struct mtk_pcie_port *port, bool hw_mode_en, u8 who)
{
	bool vote_hw_mode_en = false, last_hw_mode = false;
	unsigned long flags;
	int err = 0;
	u32 val;

	spin_lock_irqsave(&port->vote_lock, flags);

	last_hw_mode = (port->ep_hw_mode_en && port->rc_hw_mode_en)
			? true : false;
	if (who)
		port->ep_hw_mode_en = hw_mode_en;
	else
		port->rc_hw_mode_en = hw_mode_en;

	vote_hw_mode_en = (port->ep_hw_mode_en && port->rc_hw_mode_en)
			   ? true : false;
	mtk_pcie_enable_hw_control(port, vote_hw_mode_en);

	if (!vote_hw_mode_en && last_hw_mode) {
		/* Check the sleep protect ready */
		err = readl_poll_timeout_atomic(port->vlpcfg +
			      PCIE_VLP_AXI_PROTECT_STA, val,
			      !(val & PCIE_SUM_SLP_READY(port->port_num)),
			      10, 10 * USEC_PER_MSEC);
		if (err) {
			dev_info(port->dev, "PCIe sleep protect not ready, %#x\n",
				 readl_relaxed(port->vlpcfg + PCIE_VLP_AXI_PROTECT_STA));
			mtk_pcie_dump_pextp_info(port);
		} else {
			if (!mtk_pcie_sleep_protect_ready(port))
				err = -EPERM;
		}
	}

	spin_unlock_irqrestore(&port->vote_lock, flags);

	return err;
}

/*
 * mtk_pcie_control_vote_v2() - V2 Vote mechanism
 * @port: which port control hw mode
 * @hw_mode_en: vote mechanism, true: agree open hw mode;
 *              false: disagree open hw mode
 * @who: 0 is RC, 1 is EP
 * V2 PEXTP has three bits to control whether to enter PCIe RTFF HW mode.
 * MD core uses one bit, AP core RC/EP driver uses one  bit each.
 * When all three bits are set to 1,PCIe will enter RTFF HW mode.
 */
static int mtk_pcie_control_vote_v2(struct mtk_pcie_port *port, bool hw_mode_en, u8 who)
{
	void __iomem *addr;
	unsigned long flags;
	int err = 0;
	u32 val;

	spin_lock_irqsave(&port->vote_lock, flags);

	if (who)
		addr = port->pextpcfg + PEXTP_PWRCTL_8;
	else
		addr = port->pextpcfg + PEXTP_PWRCTL_6;

	val = readl_relaxed(addr);

	if (hw_mode_en)
		val |= PCIE_HWMODE_EN;
	else
		val &= ~PCIE_HWMODE_EN;

	writel_relaxed(val, addr);

	if (!hw_mode_en && !who) {
		/* Check the sleep protect ready */
		err = readl_poll_timeout_atomic(port->pextpcfg +
			      PEXTP_SLPPROT_RDY, val,
			      !(val & PEXTP_SUM_SLP_READY(port->port_num)),
			      10, 10 * USEC_PER_MSEC);
		if (err) {
			mtk_pcie_dump_pextp_info(port);
		} else {
			if (!mtk_pcie_sleep_protect_ready(port))
				err = -EPERM;
		}
	}

	spin_unlock_irqrestore(&port->vote_lock, flags);

	return err;
}

/*
 * mtk_pcie_hw_control_vote() - Vote mechanism
 * @port: The port number which EP use
 * @hw_mode_en: vote mechanism, true: agree open hw mode;
 *        false: disagree open hw mode
 * @who: 0 is rc, 1 is wifi
 */
int mtk_pcie_hw_control_vote(int port, bool hw_mode_en, u8 who)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	if (!pcie_port->data->control_vote)
		return -EOPNOTSUPP;

	return pcie_port->data->control_vote(pcie_port, hw_mode_en, who);
}
EXPORT_SYMBOL(mtk_pcie_hw_control_vote);

/*
 * mtk_pcie_ep_set_info() - handshake protocol: EP deliver info to RC
 * @port: port number
 * @params: data structure
 */
int mtk_pcie_ep_set_info(int port, struct handshake_info *params)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;

	if (!params)
		return -EINVAL;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return -ENODEV;
	}

	pcie_port->rpm_suspend_mode = LINK_STATE_L2;
	if (params->feature_id == PCIE_RPM_CTRL && params->data[0] == LINK_STATE_PCIPM_L12)
		pcie_port->rpm_suspend_mode = LINK_STATE_PCIPM_L12;

	dev_info(pcie_port->dev, "%s: set rpm mode=%d\n", __func__, pcie_port->rpm_suspend_mode);

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_ep_set_info);

/*
 * mtk_pcie_in_use() - whether pcie is used
 */
bool mtk_pcie_in_use(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val;

	/* Currently only available for port 1 */
	if (port != 1)
		return false;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return false;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return false;
	}

	val = readl_relaxed(pcie_port->pextpcfg + PEXTP1_RSV_1);
	if (val & PCIE_SUSPEND_L2_CTRL)
		return true;

	return false;
}
EXPORT_SYMBOL(mtk_pcie_in_use);

static int mtk_pcie_suspend_l2(struct mtk_pcie_port *port)
{
	int err = 0;

	if (mtk_pcie_in_use(port->port_num)) {
		port->skip_suspend = true;
		dev_info(port->dev, "port%d in use, keep active\n", port->port_num);
		return 0;
	}

	mtk_pcie_save_restore_cfg(port, true);

	/* Trigger link to L2 state */
	err = mtk_pcie_turn_off_link(port);
	if (err)
		return err;

	/* change pinmux before power off to avoid glitch */
	pinctrl_pm_select_idle_state(port->dev);
	mtk_pcie_irq_save(port);
	mtk_pcie_power_down(port);

	return 0;
}

static int mtk_pcie_resume_l2(struct mtk_pcie_port *port)
{
	int err = 0;

	if (port->port_num == 1 && port->skip_suspend) {
		port->skip_suspend = false;
		dev_info(port->dev, "port%d resume done\n", port->port_num);
		return 0;
	}

	err = mtk_pcie_power_up(port);
	if (err)
		return err;

	/* change pinmux after power on to avoid glitch */
	pinctrl_pm_select_default_state(port->dev);

	err = mtk_pcie_startup_port(port);
	if (err) {
		mtk_pcie_power_down(port);
		return err;
	}

	mtk_pcie_irq_restore(port);
	mtk_pcie_save_restore_cfg(port, false);

	return 0;
}

static int __maybe_unused mtk_pcie_runtime_suspend(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	struct pci_dev *pdev = port->pcidev;
	int err = 0;

	if (!device_find_child(dev, NULL, match_any))
		return 0;

	dev_info(port->dev, "rpm suspend mode=%d\n", port->rpm_suspend_mode);

	if (port->rpm_suspend_mode == LINK_STATE_L2) {
		err = mtk_pcie_suspend_l2(port);
		if (err)
			return err;
	}

	if (port->dev->power.runtime_status != RPM_ACTIVE && port->rpm) {
		pdev->current_state = PCI_D3cold;
		if (port->rpm_suspend_mode == LINK_STATE_PCIPM_L12) {
			dev_info(port->dev, "rpm suspend PCIe LTSSM=%#x, PCIe L1SS_pm=%#x\n",
				 readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
				 readl_relaxed(port->base + PCIE_ISTATUS_PM));
		} else {
			err = mtk_pcie_request_eint_irq(port);
			if (err)
				return err;
		}
	}

	return 0;
}

static int __maybe_unused mtk_pcie_runtime_resume(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	struct pci_dev *pdev = port->pcidev;
	int err = 0;

	if (!device_find_child(dev, NULL, match_any))
		return 0;

	dev_info(port->dev, "rpm resume mode=%d\n", port->rpm_suspend_mode);

	if (port->dev->power.runtime_status != RPM_ACTIVE && port->rpm) {
		pdev->current_state = PCI_D0;
		if (port->rpm_suspend_mode == LINK_STATE_PCIPM_L12) {
			dev_info(port->dev, "rpm resume PCIe LTSSM=%#x, PCIe L1SS_pm=%#x\n",
				 readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
				 readl_relaxed(port->base + PCIE_ISTATUS_PM));
		} else {
			mtk_pcie_free_eint_irq(port);
		}
	}

	if (port->rpm_suspend_mode == LINK_STATE_L2) {
		err = mtk_pcie_resume_l2(port);
		if (err)
			return err;
	}

	return 0;
}

static int __maybe_unused mtk_pcie_suspend_noirq(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	int err;

	if (port->suspend_mode == LINK_STATE_ASPM_L12) {
		dev_info(port->dev, "Suspend PCIe LTSSM=%#x, PCIe L1SS_pm=%#x\n",
			 readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
			 readl_relaxed(port->base + PCIE_ISTATUS_PM));

		/* Clear LTSSM record info after dump */
		writel_relaxed(PCIE_LTSSM_STATE_CLEAR, port->base + PCIE_LTSSM_STATUS_REG);

		if (port->data->suspend_l12) {
			err = port->data->suspend_l12(port);
			if (err)
				return err;
		}

		err = mtk_pcie_hw_control_vote(port->port_num, true, 0);
		if (err)
			return err;

		port->data->clkbuf_control(port, false);

		mtk_pcie_dump_pextp_info(port);
	} else {
		err = mtk_pcie_suspend_l2(port);
		if (err)
			return err;
	}

	return 0;
}

static int __maybe_unused mtk_pcie_resume_noirq(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	int err;

	if (port->suspend_mode == LINK_STATE_ASPM_L12) {
		port->data->clkbuf_control(port, true);

		/* Wait 450us for BBCK2 switch SW Mode ready */
		udelay(450);

		err = mtk_pcie_hw_control_vote(port->port_num, false, 0);
		if (err)
			return err;

		mtk_pcie_dump_pextp_info(port);

		if (port->data->resume_l12) {
			err = port->data->resume_l12(port);
			if (err)
				return err;
		}

		dev_info(port->dev, "Resume PCIe LTSSM=%#x, PCIe L1SS_pm=%#x\n",
			 readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
			 readl_relaxed(port->base + PCIE_ISTATUS_PM));

		/* Clear LTSSM record info after dump */
		writel_relaxed(PCIE_LTSSM_STATE_CLEAR, port->base + PCIE_LTSSM_STATUS_REG);
	} else {
		if (port->port_num == 1 && port->skip_suspend) {
			port->skip_suspend = false;
			dev_info(port->dev, "port%d resume done\n", port->port_num);
			return 0;
		}

		err = mtk_pcie_resume_l2(port);
		if (err)
			return err;
	}

	return 0;
}

static const struct dev_pm_ops mtk_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_pcie_suspend_noirq,
				      mtk_pcie_resume_noirq)
	SET_RUNTIME_PM_OPS(mtk_pcie_runtime_suspend, mtk_pcie_runtime_resume, NULL)
};

int mtk_pcie_disable_refclk(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	reset_control_assert(pcie_port->phy_reset);

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_disable_refclk);

int mtk_pcie_enable_cfg_dump(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	pcie_port->dump_cfg = true;

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_enable_cfg_dump);

int mtk_pcie_disable_cfg_dump(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	pcie_port->dump_cfg = false;

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_disable_cfg_dump);

int mtk_pcie_soft_off(struct pci_bus *bus)
{
	struct mtk_pcie_port *port;
	int ret;

	if (!bus) {
		pr_info("There is no bus, please check the host driver\n");
		return -ENODEV;
	}

	port = bus->sysdata;
	if (port->soft_off) {
		dev_info(port->dev, "The soft_off is true, can't soft off\n");
		return -EPERM;
	}

	/* Trigger link to L2 state */
	ret = mtk_pcie_turn_off_link(port);

	mtk_pcie_save_restore_cfg(port, true);
	mtk_pcie_irq_save(port);
	port->soft_off = true;

	if (port->port_num == PCIE_PORT_NUM_1)
		mtk_pcie_pinmux_select(PCIE_PORT_NUM_1, PCIE_PINMUX_HIZ);

	mtk_pcie_power_down(port);

	dev_info(port->dev, "mtk pcie soft off done\n");

	return ret;
}
EXPORT_SYMBOL(mtk_pcie_soft_off);

int mtk_pcie_soft_on(struct pci_bus *bus)
{
	struct mtk_pcie_port *port;
	int ret;

	if (!bus) {
		pr_info("There is no bus, please check the host driver\n");
		return -ENODEV;
	}

	port = bus->sysdata;
	if (!port->soft_off) {
		dev_info(port->dev, "The soft_off is false, can't soft on\n");
		return -EPERM;
	}

	ret = mtk_pcie_power_up(port);
	if (ret)
		return ret;

	pinctrl_select_default_state(port->dev);

	ret = mtk_pcie_startup_port(port);
	if (ret)
		return ret;

	port->soft_off = false;
	mtk_pcie_irq_restore(port);
	mtk_pcie_save_restore_cfg(port, false);

	/* The detection range is AER enable to soft on done */
	if (port->port_num == 0)
		port->aer_detect = false;

	dev_info(port->dev, "mtk pcie soft on done\n");

	return ret;
}
EXPORT_SYMBOL(mtk_pcie_soft_on);

static int mtk_pcie_pre_init_6985(struct mtk_pcie_port *port)
{
	u32 val;

	/* The two PCIe ports of 6985 only have one interrupt line
	 * connected to ADSP. Default is port0 interrupt dispatch to ADSP.
	 * If set PCIE_MSI_SEL bit[0] to 0x1, will switch the
	 * port1 interrupt dispatch to ADSP
	 */
	if (port->port_num == 1) {
		val = readl_relaxed(port->pextpcfg + PCIE_MSI_SEL);
		val |= BIT(0);
		writel_relaxed(val, port->pextpcfg + PCIE_MSI_SEL);
		dev_info(port->dev, "PCIE MSI select=%#x\n",
			readl_relaxed(port->pextpcfg + PCIE_MSI_SEL));
	}

	/* Enable P2_EXIT signal to phy, wait 8us for EP entering L1ss */
	val = readl_relaxed(port->base + PCIE_ASPM_CTRL);
	val &= ~PCIE_P2_IDLE_TIME_MASK;
	val |= PCIE_P2_EXIT_BY_CLKREQ | PCIE_P2_IDLE_TIME(8);
	writel_relaxed(val, port->base + PCIE_ASPM_CTRL);

	return 0;
}

static int mtk_pcie_suspend_l12_6985(struct mtk_pcie_port *port)
{
	int err;
	u32 val;

	/* Binding of BBCK1 and BBCK2 */
	err = clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, BBCK2_BIND);
	if (err)
		dev_info(port->dev, "Fail to bind BBCK2 with BBCK1\n");

	/* Wait 400us for BBCK2 bind ready */
	udelay(400);

	/* Enable Bypass BBCK2 */
	val = readl_relaxed(port->pextpcfg + PEXTP_RSV_0);
	val |= PCIE_BBCK2_BYPASS;
	writel_relaxed(val, port->pextpcfg + PEXTP_RSV_0);

	return 0;
}

static int mtk_pcie_resume_l12_6985(struct mtk_pcie_port *port)
{
	int err;

	/* Unbinding of BBCK1 and BBCK2 */
	err = clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, BBCK2_UNBIND);
	if (err)
		dev_info(port->dev, "Fail to unbind BBCK2 with BBCK1\n");

	return 0;
}

static const struct mtk_pcie_data mt6985_data = {
	.pre_init = mtk_pcie_pre_init_6985,
	.suspend_l12 = mtk_pcie_suspend_l12_6985,
	.resume_l12 = mtk_pcie_resume_l12_6985,
	.control_vote = mtk_pcie_control_vote_v1,
	.clkbuf_control = mtk_pcie_clkbuf_force_bbck2,
};

static int mtk_pcie_pre_init_6989(struct mtk_pcie_port *port)
{
	u32 val;

	/* Make PCIe RC wait apsrc_ack signal before access EMI */
	val = readl_relaxed(port->base + PCIE_RESOURCE_CTRL);
	val |= PCIE_APSRC_ACK;
	writel_relaxed(val, port->base + PCIE_RESOURCE_CTRL);

	/* Don't let PCIe AXI0 port reply slave error */
	val = readl_relaxed(port->base + PCIE_AXI_IF_CTRL);
	val |= PCIE_AXI0_SLV_RESP_MASK;
	writel_relaxed(val, port->base + PCIE_AXI_IF_CTRL);

	/* Set write completion timeout to 4ms */
	writel_relaxed(WCPL_TIMEOUT_4MS, port->base + PCIE_WCPL_TIMEOUT);

	/* Set p2_sleep_time to 4us */
	val = readl_relaxed(port->base + PCIE_ASPM_CTRL);
	val &= ~PCIE_P2_SLEEP_TIME_MASK;
	val |= PCIE_P2_SLEEP_TIME_4US;
	writel_relaxed(val, port->base + PCIE_ASPM_CTRL);

	return 0;
}

static const struct mtk_pcie_data mt6989_data = {
	.pre_init = mtk_pcie_pre_init_6989,
	.control_vote = mtk_pcie_control_vote_v1,
	.clkbuf_control = mtk_pcie_clkbuf_force_bbck2,
};

static int mtk_pcie_suspend_l12_6991(struct mtk_pcie_port *port)
{
	int err;
	u32 val;

	/* Binding of BBCK1 and BBCK2 */
	err = clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, LIBER_BBCK2_BIND);
	if (err)
		dev_info(port->dev, "Fail to bind BBCK2 with BBCK1\n");

	/* Wait 450us for BBCK2 bind ready */
	udelay(450);

	/* Enable Bypass BBCK2 */
	val = readl_relaxed(port->pextpcfg + PEXTP_REQ_CTRL);
	val |= RG_PCIE26M_BYPASS;
	writel_relaxed(val, port->pextpcfg + PEXTP_REQ_CTRL);

	/* force mac sleep to 0 when switch lowpower clk */
	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val |= PCIE_MAC_SLP_DIS;
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	err = readl_poll_timeout(port->base + PCIE_RES_STATUS, val,
				 ((val & ALL_RES_ACK) == ALL_RES_ACK),
				 20, 1000);
	if (err)
		dev_info(port->dev, "Polling resource ack fail\n");

	/* PCIe lowpower clock sel to 32K */
	val = readl_relaxed(port->pextpcfg + PEXTP_CLOCK_CON);
	val |= P0_LOWPOWER_CK_SEL;
	writel_relaxed(val, port->pextpcfg + PEXTP_CLOCK_CON);
	dev_info(port->dev, "Switch clock sel to %x\n", val);

	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val &= ~PCIE_MAC_SLP_DIS;
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	return 0;
}

static int mtk_pcie_resume_l12_6991(struct mtk_pcie_port *port)
{
	int err;
	u32 val;

	/* Unbinding of BBCK1 and BBCK2 */
	err = clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, LIBER_BBCK2_UNBIND);
	if (err)
		dev_info(port->dev, "Fail to unbind BBCK2 with BBCK1\n");

	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val |= PCIE_MAC_SLP_DIS;
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	err = readl_poll_timeout(port->base + PCIE_RES_STATUS, val,
				 ((val & ALL_RES_ACK) == ALL_RES_ACK),
				 20, 1000);
	if (err)
		dev_info(port->dev, "Polling resource ack fail\n");

	/* PCIe lowpower clock sel to 26M */
	val = readl_relaxed(port->pextpcfg + PEXTP_CLOCK_CON);
	val &= ~P0_LOWPOWER_CK_SEL;
	writel_relaxed(val, port->pextpcfg + PEXTP_CLOCK_CON);
	dev_info(port->dev, "Switch clock sel to %x\n", val);

	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val &= ~PCIE_MAC_SLP_DIS;
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	return 0;
}

static int mtk_pcie_pre_init_6991(struct mtk_pcie_port *port)
{
	u32 val;

	/* Make PCIe RC wait apsrc_ack signal before access EMI */
	val = readl_relaxed(port->base + PCIE_RESOURCE_CTRL);
	val |= PCIE_APSRC_ACK;
	writel_relaxed(val, port->base + PCIE_RESOURCE_CTRL);

	/* Don't let PCIe AXI0 port reply slave error */
	val = readl_relaxed(port->base + PCIE_AXI_IF_CTRL);
	val |= PCIE_AXI0_SLV_RESP_MASK;
	writel_relaxed(val, port->base + PCIE_AXI_IF_CTRL);

	/* Set write completion timeout to 4ms */
	writel_relaxed(WCPL_TIMEOUT_4MS, port->base + PCIE_WCPL_TIMEOUT);

	/* Adjust SYS_CLK_RDY_TIME to 10us to avoid glitch*/
	val = readl_relaxed(port->base + PCIE_RESOURCE_CTRL);
	val &= ~SYS_CLK_RDY_TIME;
	val |= SYS_CLK_RDY_TIME_TO_10US;
	writel_relaxed(val, port->base + PCIE_RESOURCE_CTRL);

	val = readl_relaxed(port->pextpcfg + PEXTP_CLOCK_CON);
	switch (port->port_num) {
	case 0:
		val &= ~P0_LOWPOWER_CK_SEL;
		break;
	case 1:
		val &= ~P1_LOWPOWER_CK_SEL;
		break;
	case 2:
		val &= ~P2_LOWPOWER_CK_SEL;
		break;
	default:
		dev_info(port->dev, "Port num %d out of range\n", port->port_num);
	}

	writel_relaxed(val, port->pextpcfg + PEXTP_CLOCK_CON);

	if (port->port_num == 0) {
		/* wifi request response data is all zero when completion timeout */
		val = readl_relaxed(port->base + PCIE_AXI_IF_CTRL);
		val |= SW_CPLTO_DATA_SEL;
		writel_relaxed(val, port->base + PCIE_AXI_IF_CTRL);
		/* Detect Completion timeout before wifi on */
		port->aer_detect = true;
	}

	/* bypass PMRC signal */
	val = readl_relaxed(port->pextpcfg + PEXTP_REQ_CTRL);
	val |= RG_PCIE26M_BYPASS;
	writel_relaxed(val, port->pextpcfg + PEXTP_REQ_CTRL);

	return 0;
}

static int mtk_pcie_post_init_6991(struct mtk_pcie_port *port)
{
	u32 val;

	if (port->port_num == 1) {
		/*
		 * Use bit[3:0] of reserved register record the msi group number sent to ADSP
		 * ex: Group 2 and 3 of 6991 are sent to ADSP, write 1 to bit[2] and bit[3]
		 */
		val = readl_relaxed(port->base + PCIE_CFG_RSV_0);
		val |= MSI_GRP2 | MSI_GRP3;
		writel_relaxed(val, port->base + PCIE_CFG_RSV_0);

		/* Enable aer report and reset error interrupt */
		val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
		val |= PCIE_AER_EVT_EN;
		writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);

		/* PCIe1 read completion timeout is adjusted to 10ms */
		mtk_pcie_adjust_cplto_scale(port, PCIE_CPLTO_SCALE_10MS);

		/* Mofify the PM capability to not support generating PME from D3hot state */
		val = readw_relaxed(port->base + PCIE_PCI_LPM + PCI_PM_PMC);
		val &= ~PCI_PM_CAP_PME_MASK;
		val |= PCI_PM_CAP_PME_D0;
		writew_relaxed(val, port->base + PCIE_PCI_LPM + PCI_PM_PMC);
	}

	return 0;
}

static const struct mtk_pcie_data mt6991_data = {
	.pre_init = mtk_pcie_pre_init_6991,
	.post_init = mtk_pcie_post_init_6991,
	.suspend_l12 = mtk_pcie_suspend_l12_6991,
	.resume_l12 = mtk_pcie_resume_l12_6991,
	.control_vote = mtk_pcie_control_vote_v2,
	.clkbuf_control = mtk_pcie_clkbuf_force_26m,
};

static const struct of_device_id mtk_pcie_of_match[] = {
	{ .compatible = "mediatek,mt8192-pcie" },
	{ .compatible = "mediatek,mt6985-pcie", .data = &mt6985_data },
	{ .compatible = "mediatek,mt6989-pcie", .data = &mt6989_data },
	{ .compatible = "mediatek,mt6991-pcie", .data = &mt6991_data },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_pcie_of_match);

static struct platform_driver mtk_pcie_driver = {
	.probe = mtk_pcie_probe,
	.remove = mtk_pcie_remove,
	.driver = {
		.name = "mtk-pcie",
		.of_match_table = mtk_pcie_of_match,
		.pm = &mtk_pcie_pm_ops,
	},
};

static int mtk_pcie_init_func(void *pvdev)
{
#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
	int err = 0;

	err = register_trace_android_rvh_do_serror(
			pcie_android_rvh_do_serror, NULL);
	if (err)
		pr_info("register pcie android_rvh_do_serror failed!\n");
#endif

	return platform_driver_register(&mtk_pcie_driver);
}

static int __init mtk_pcie_init(void)
{
	struct task_struct *driver_thread_handle;

	driver_thread_handle = kthread_run(mtk_pcie_init_func,
					   NULL, "pcie_thread");

	if (IS_ERR(driver_thread_handle))
		return PTR_ERR(driver_thread_handle);

	return 0;
}

static void __exit mtk_pcie_exit(void)
{
	platform_driver_unregister(&mtk_pcie_driver);
}

module_init(mtk_pcie_init);
module_exit(mtk_pcie_exit);
MODULE_LICENSE("GPL v2");
