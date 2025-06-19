// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Leilk Liu <leilk.liu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/platform_data/spi-mt65xx.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/dma-mapping.h>
#include <linux/time.h>
#include <linux/iopoll.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#define SPI_CFG0_REG                      0x0000
#define SPI_CFG1_REG                      0x0004
#define SPI_TX_SRC_REG                    0x0008
#define SPI_RX_DST_REG                    0x000c
#define SPI_TX_DATA_REG                   0x0010
#define SPI_RX_DATA_REG                   0x0014
#define SPI_CMD_REG                       0x0018
#define SPI_STATUS0_REG                   0x001c
#define SPI_STATUS1_REG                   0x0020
#define SPI_PAD_SEL_REG                   0x0024
#define SPI_CFG2_REG                      0x0028
#define SPI_TX_SRC_REG_64                 0x002c
#define SPI_RX_DST_REG_64                 0x0030
#define SPI_IRQ_TIME32_REG                0x0038
#define SPI_IRQ_TIME64_REG                0x003c
#define SPI_CFG3_REG                      0x0040
#define SPI_DEBUG0_REG                    0x004c
#define SPI_DEBUG1_REG                    0x0050
#define SPI_SLICE_EN_REG                  0x005c
#define SPI_CFG5_REG                      0x0068

#define PERI_SPI_RST                      0x0000
#define PERI_SPI_RST_SET                  0x0004
#define PERI_SPI_RST_CLR                  0x0008

#define SPI_CFG0_SCK_HIGH_OFFSET          0
#define SPI_CFG0_SCK_LOW_OFFSET           8
#define SPI_CFG0_CS_HOLD_OFFSET           16
#define SPI_CFG0_CS_SETUP_OFFSET          24
#define SPI_ADJUST_CFG0_CS_HOLD_OFFSET    0
#define SPI_ADJUST_CFG0_CS_SETUP_OFFSET   16

#define SPI_CFG1_CS_IDLE_OFFSET           0
#define SPI_CFG1_PACKET_LOOP_OFFSET       8
#define SPI_CFG1_PACKET_LENGTH_OFFSET     16
#define SPI_CFG1_GET_TICK_DLY_OFFSET      29
#define SPI_CFG1_GET_TICK_DLY_OFFSET_V1   30

#define SPI_CFG1_GET_TICK_DLY_MASK        0xe0000000
#define SPI_CFG1_GET_TICK_DLY_MASK_V1     0xc0000000

#define SPI_CFG1_CS_IDLE_MASK             0xff
#define SPI_CFG1_PACKET_LOOP_MASK         0xff00
#define SPI_CFG1_PACKET_LENGTH_MASK       0x3ff0000
#define SPI_CFG1_IPM_PACKET_LENGTH_MASK   0xffff0000
#define SPI_CFG2_SCK_HIGH_OFFSET          0
#define SPI_CFG2_SCK_LOW_OFFSET           16

#define SPI_CMD_ACT                  BIT(0)
#define SPI_CMD_RESUME               BIT(1)
#define SPI_CMD_RST                  BIT(2)
#define SPI_CMD_PAUSE_EN             BIT(4)
#define SPI_CMD_DEASSERT             BIT(5)
#define SPI_CMD_SAMPLE_SEL           BIT(6)
#define SPI_CMD_CS_POL               BIT(7)
#define SPI_CMD_CPHA                 BIT(8)
#define SPI_CMD_CPOL                 BIT(9)
#define SPI_CMD_RX_DMA               BIT(10)
#define SPI_CMD_TX_DMA               BIT(11)
#define SPI_CMD_TXMSBF               BIT(12)
#define SPI_CMD_RXMSBF               BIT(13)
#define SPI_CMD_RX_ENDIAN            BIT(14)
#define SPI_CMD_TX_ENDIAN            BIT(15)
#define SPI_CMD_FINISH_IE            BIT(16)
#define SPI_CMD_PAUSE_IE             BIT(17)
#define SPI_CMD_CS_PIN_SEL           BIT(18)
#define SPI_CMD_NONIDLE_MODE	     BIT(19)
#define SPI_CMD_SPIM_LOOP	     BIT(21)
#define SPI_CMD_GET_TICKDLY_OFFSET    22

#define SPI_CMD_GET_TICKDLY_MASK	GENMASK(24, 22)
#define SPI_CMD_GET_TICKDLY_SLICE_ON_MASK	GENMASK(28, 22)

#define PIN_MODE_CFG(x)	((x) / 2)

#define SPI_CFG3_PIN_MODE_OFFSET		0
#define SPI_CFG3_HALF_DUPLEX_DIR		BIT(2)
#define SPI_CFG3_HALF_DUPLEX_EN			BIT(3)
#define SPI_CFG3_XMODE_EN			BIT(4)
#define SPI_CFG3_NODATA_FLAG			BIT(5)
#define SPI_CFG3_CMD_BYTELEN_OFFSET		8
#define SPI_CFG3_ADDR_BYTELEN_OFFSET		12

#define SPI_CFG3_CMD_PIN_MODE_MASK		GENMASK(1, 0)
#define SPI_CFG3_CMD_BYTELEN_MASK		GENMASK(11, 8)
#define SPI_CFG3_ADDR_BYTELEN_MASK		GENMASK(15, 12)

#define SPI_PACKET_LENGTH_LOW_MASK		GENMASK(15, 0)
#define SPI_PACKET_LENGTH_HIGH_MASK		GENMASK(31, 16)

#define SPI_PACKET_LENGTH_HIGH_OFFSET		16

#define SPI_SLICE_EN_RX_WIDTH_MASK         GENMASK(2, 0)
#define SPI_SLICE_EN_TX_WIDTH_MASK         GENMASK(5, 3)
#define SPI_SLICE_EN_RX_OFFSET              0
#define SPI_SLICE_EN_TX_OFFSET              3

#define MT8173_SPI_MAX_PAD_SEL          3
#define MTK_SPI_MAX_TICK_DLY            8
#define MTK_SPI_SLICE_EN_MAX_TICK_DLY   128
#define MTK_SPI_PAD_SEL                 0
#define MTK_SPI_MAX_RESET_BIT           31

#define MTK_SPI_PAUSE_INT_STATUS 0x2
#define MTK_SPI_IDLE_INT_STATUS 0x1

#define MTK_SPI_IDLE 0
#define MTK_SPI_PAUSED 1

#define MTK_SPI_MAX_FIFO_SIZE 32U
#define MTK_SPI_PACKET_SIZE 1024
#define MTK_SPI_32BITS_MASK  (0xffffffff)

#define MTK_SPI_IPM_PACKET_SIZE SZ_64K
#define MTK_SPI_IPM_PACKET_LOOP SZ_256
#define MTK_SPI_IPM_ENHANCE_PACKET_SIZE SZ_4G
#define DMA_ADDR_EXT_BITS (36)
#define DMA_ADDR_DEF_BITS (32)

/* Reference to core layer timeout (us) */
#define SPI_FIFO_POLLING_TIMEOUT (200000)

/* define infra request option type */
#define MTK_SPI_KERNEL_OP_REQ_INFRA 0
#define MTK_SPI_KERNEL_OP_REL_INFRA 1

#define MTK_INFRA_REQ_RETRY_TIMES 2

struct mtk_spi_compatible {
	bool need_pad_sel;
	/* Must explicitly send dummy Rx bytes to do Tx only transfer */
	bool must_rx;
	/* Must explicitly send dummy Tx bytes to do Rx only transfer */
	bool must_tx;
	/* some IC design adjust cfg register to enhance time accuracy */
	bool enhance_timing;
	/* some IC support DMA addr extension */
	bool dma_ext;
	/* some IC registers is new version */
	bool ipm_design;
	bool support_quad;
	/* some IC need change cs by SW */
	bool sw_cs;
	/* some IC enhance patcket length to 4GB*/
	bool enhance_packet_len;
	/* some IC use slice enable for high speed timing*/
	bool slice_en;
	/* starting from mt6899, IP supports non-8-bit aligned dummy data.
	 * however, since the kernel interface handles dummy data in bytes units,
	 * the driver does not currently support this feature.
	 */
	bool dummy_cycle;
	/* some plat IP have infra on/off feature,so should req and rel infra*/
	bool infra_req;
	/* after a timeout occurs, a global reset is required*/
	bool hw_reset;
};

struct mtk_spi {
	void __iomem *base;
	u32 state;
	int pad_num;
	u32 *pad_sel;
	struct clk *parent_clk, *sel_clk, *spi_clk;
	bool no_need_unprepare;
	struct spi_transfer *cur_transfer;
	u32 xfer_len;
	u32 num_xfered;
	struct scatterlist *tx_sgl, *rx_sgl;
	u32 tx_sgl_len, rx_sgl_len;
	const struct mtk_spi_compatible *dev_comp;
	u32 spi_clk_hz;
	struct completion spimem_done;
	/* Add auto_suspend_delay for user to balance
	 * power consumption and performance, default 0.
	 */
	u32 auto_suspend_delay;
	bool suspend_delay_update;
	bool use_spimem;
	struct device *dev;
	dma_addr_t tx_dma;
	dma_addr_t rx_dma;
	int irq;
	u32 reset_bit;
	bool err_occur;
	spinlock_t eh_spi_lock;
	bool dma_en;
};

static const struct mtk_spi_compatible mtk_common_compat;

static const struct mtk_spi_compatible mt2712_compat = {
	.must_rx = true,
	.must_tx = true,
};

static const struct mtk_spi_compatible mt6899_compat = {
	.need_pad_sel = true,
	.must_rx = false,
	.must_tx = false,
	.enhance_timing = true,
	.dma_ext = true,
	.ipm_design = true,
	.support_quad = true,
	.sw_cs = true,
	.enhance_packet_len = true,
	.slice_en = true,
	.dummy_cycle = true,
	.infra_req = true,
	.hw_reset = true,
};

static const struct mtk_spi_compatible mt6991_compat = {
	.need_pad_sel = true,
	.must_rx = false,
	.must_tx = false,
	.enhance_timing = true,
	.dma_ext = true,
	.ipm_design = true,
	.support_quad = true,
	.sw_cs = true,
	.enhance_packet_len = true,
	.slice_en = true,
	.dummy_cycle = false,
	.infra_req = false,
	.hw_reset = true,
};

static const struct mtk_spi_compatible mt6989_compat = {
	.need_pad_sel = true,
	.must_rx = false,
	.must_tx = false,
	.enhance_timing = true,
	.dma_ext = true,
	.ipm_design = true,
	.support_quad = true,
	.sw_cs = true,
	.enhance_packet_len = true,
	.slice_en = false,
};

static const struct mtk_spi_compatible mt6985_compat = {
	.need_pad_sel = true,
	.must_rx = false,
	.must_tx = false,
	.enhance_timing = true,
	.dma_ext = true,
	.ipm_design = true,
	.support_quad = true,
	.sw_cs = true,
	.slice_en = false,
};

static const struct mtk_spi_compatible mt6983_compat = {
	.need_pad_sel = true,
	.must_rx = false,
	.must_tx = false,
	.enhance_timing = true,
	.dma_ext = true,
	.ipm_design = true,
	.support_quad = true,
};

static const struct mtk_spi_compatible mt6765_compat = {
	.need_pad_sel = true,
	.must_rx = true,
	.must_tx = true,
	.enhance_timing = true,
	.dma_ext = true,
};

static const struct mtk_spi_compatible mt7622_compat = {
	.must_rx = true,
	.must_tx = true,
	.enhance_timing = true,
};

static const struct mtk_spi_compatible mt8173_compat = {
	.need_pad_sel = true,
	.must_rx = true,
	.must_tx = true,
};

static const struct mtk_spi_compatible mt8183_compat = {
	.need_pad_sel = true,
	.must_tx = true,
	.enhance_timing = true,
};

static const struct mtk_spi_compatible mt6893_compat = {
	.need_pad_sel = true,
	.must_tx = true,
	.enhance_timing = true,
	.dma_ext = true,
};

/*
 * A piece of default chip info unless the platform
 * supplies it.
 */
static const struct mtk_chip_config mtk_default_chip_info;

static const struct of_device_id mtk_spi_of_match[] = {
	{ .compatible = "mediatek,mt2701-spi",
		.data = (void *)&mtk_common_compat,
	},
	{ .compatible = "mediatek,mt2712-spi",
		.data = (void *)&mt2712_compat,
	},
	{ .compatible = "mediatek,mt6589-spi",
		.data = (void *)&mtk_common_compat,
	},
	{ .compatible = "mediatek,mt6765-spi",
		.data = (void *)&mt6765_compat,
	},
	{ .compatible = "mediatek,mt6991-spi",
		.data = (void *)&mt6991_compat,
	},
	{ .compatible = "mediatek,mt6983-spi",
		.data = (void *)&mt6983_compat,
	},
	{ .compatible = "mediatek,mt6985-spi",
		.data = (void *)&mt6985_compat,
	},
	{ .compatible = "mediatek,mt6989-spi",
		.data = (void *)&mt6989_compat,
	},
	{ .compatible = "mediatek,mt7622-spi",
		.data = (void *)&mt7622_compat,
	},
	{ .compatible = "mediatek,mt7629-spi",
		.data = (void *)&mt7622_compat,
	},
	{ .compatible = "mediatek,mt8135-spi",
		.data = (void *)&mtk_common_compat,
	},
	{ .compatible = "mediatek,mt8173-spi",
		.data = (void *)&mt8173_compat,
	},
	{ .compatible = "mediatek,mt8183-spi",
		.data = (void *)&mt8183_compat,
	},
	{ .compatible = "mediatek,mt8192-spi",
		.data = (void *)&mt6765_compat,
	},
	{ .compatible = "mediatek,mt6893-spi",
		.data = (void *)&mt6893_compat,
	},
	{ .compatible = "mediatek,mt6899-spi",
		.data = (void *)&mt6899_compat,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_spi_of_match);
#define LOG_CLOSE   0
#define LOG_OPEN    1
u8 spi_log_status = LOG_CLOSE;
static uint32_t is_ioremaped;
static void __iomem *reset_base;

#define spi_debug(fmt, args...) do { \
	if (spi_log_status == LOG_OPEN) {\
		pr_info("[%s]%s() " fmt, dev_name(&master->dev),\
			__func__, ##args);\
	} \
} while (0)

static ssize_t spi_log_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	char buf_temp[50] = { 0 };
	int ret;

	if (buf == NULL) {
		pr_notice("%s() *buf is NULL\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf_temp, sizeof(buf_temp), "Now spi log %s.\n",
		(spi_log_status == LOG_CLOSE) ? "disabled" : "enabled");
	if (ret <= 0) {
		pr_notice("%s() snprintf failed\n", __func__);
		return ret;
	}

	strncat(buf, buf_temp, strlen(buf_temp));

	return strlen(buf);
}

static ssize_t spi_log_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!\n", __func__);
		return -EINVAL;
	}

	pr_info("[spi]%s buflen:%zu buf:%s\n", __func__, strlen(buf), buf);
	if (!strncmp(buf, "1", 1)) {
		pr_info("[spi]%s Now enable spi log\n", __func__);
		spi_log_status = LOG_OPEN;
	} else if (!strncmp(buf, "0", 1)) {
		pr_info("[spi]%s Now disable spi log\n", __func__);
		spi_log_status = LOG_CLOSE;
	} else
		pr_info("[spi]%s invalid parameter.Plz Input 1 or 0\n",
			__func__);

	return count;
}

static DEVICE_ATTR_RW(spi_log);

static void spi_dump_reg(struct mtk_spi *mdata, struct spi_master *master)
{
	spi_debug("||**************%s**************||\n", __func__);
	spi_debug("cfg0:0x%.8x\n", readl(mdata->base + SPI_CFG0_REG));
	spi_debug("cfg1:0x%.8x\n", readl(mdata->base + SPI_CFG1_REG));
	spi_debug("cfg2:0x%.8x\n", readl(mdata->base + SPI_CFG2_REG));
	if (mdata->dev_comp->ipm_design) {
		spi_debug("cfg3:0x%.8x\n", readl(mdata->base + SPI_CFG3_REG));
		spi_debug("debug0:0x%.8x\n", readl(mdata->base + SPI_DEBUG0_REG));
		spi_debug("debug1:0x%.8x\n", readl(mdata->base + SPI_DEBUG1_REG));
		spi_debug("irq_time_32:0x%.8x\n", readl(mdata->base + SPI_IRQ_TIME32_REG));
		spi_debug("irq_time_64:0x%.8x\n", readl(mdata->base + SPI_IRQ_TIME64_REG));
	}
	if (mdata->dev_comp->enhance_packet_len)
		spi_debug("cfg5:0x%.8x\n", readl(mdata->base + SPI_CFG5_REG));
	spi_debug("cmd :0x%.8x\n", readl(mdata->base + SPI_CMD_REG));
	spi_debug("tx_s:0x%.8x\n", readl(mdata->base + SPI_TX_SRC_REG));
	spi_debug("tx_s_64:0x%.8x\n", readl(mdata->base + SPI_TX_SRC_REG_64));
	spi_debug("rx_d:0x%.8x\n", readl(mdata->base + SPI_RX_DST_REG));
	spi_debug("rx_d_64:0x%.8x\n", readl(mdata->base + SPI_RX_DST_REG_64));
	spi_debug("status1:0x%.8x\n", readl(mdata->base + SPI_STATUS1_REG));
	spi_debug("pad_sel:0x%.8x\n", readl(mdata->base + SPI_PAD_SEL_REG));
	if (mdata->dev_comp->slice_en)
		spi_debug("slice_en:0x%.8x\n", readl(mdata->base + SPI_SLICE_EN_REG));
	spi_debug("||**************%s end**************||\n", __func__);
}

static void mtk_spi_error_dump(struct spi_controller *ctlr,
		struct spi_message *msg)
{
	struct spi_master *master = ctlr;
	struct mtk_spi *mdata = spi_master_get_devdata(master);
	int i = 0;
	unsigned long flags;

	spi_log_status = LOG_OPEN;

	spin_lock_irqsave(&mdata->eh_spi_lock, flags);

	if (mdata->use_spimem || master->can_dma(master, NULL, mdata->cur_transfer)) {
		mt_irq_dump_status(mdata->irq);
		/* timeout occurred due to no response to irq, check if IP irq bit is raised.*/
		spi_debug("status0:0x%.8x\n", readl(mdata->base + SPI_STATUS0_REG));
	}

	if (mdata->dev_comp->ipm_design)
		writel(0x0, mdata->base + SPI_DEBUG0_REG);

	spi_dump_reg(mdata, master);

	/* start to write 0x0-0xA to debug 0 for more info */
	if (mdata->dev_comp->ipm_design) {
		for (i = 0; i < 11; i++) {
			spi_debug("then debug0 = 0x%x and dump:\n", i);
			writel(i, mdata->base + SPI_DEBUG0_REG);
			spi_debug("debug0:0x%.8x\n", readl(mdata->base + SPI_DEBUG0_REG));
			spi_debug("debug1:0x%.8x\n", readl(mdata->base + SPI_DEBUG1_REG));
		}
		writel(0x0, mdata->base + SPI_DEBUG0_REG);
	}

	if (mdata->dev_comp->hw_reset) {
		writel(0x1 << mdata->reset_bit, reset_base + PERI_SPI_RST_SET);
		writel(0x1 << mdata->reset_bit, reset_base + PERI_SPI_RST_CLR);
	}

	mdata->err_occur = true;
	spin_unlock_irqrestore(&mdata->eh_spi_lock, flags);
	spi_log_status = LOG_CLOSE;
}

static void spi_dump_config(struct spi_master *master, struct spi_message *msg)
{
	struct spi_device *spi = msg->spi;
	struct mtk_chip_config *chip_config = spi->controller_data;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	/*Old cs timing settng was forbidden, please use new method*/
	BUG_ON(chip_config->cs_setuptime | chip_config->cs_holdtime | chip_config->cs_idletime);
	spi_debug("||**************%s**************||\n", __func__);
	spi_debug("chip_config->spi_mode:0x%x\n", spi->mode);
	spi_debug("chip_config->sample_sel:%d\n", chip_config->sample_sel);
	spi_debug("chip_config->chip_select:%d,chip_config->pad_sel:%d\n",
			spi->chip_select, mdata->pad_sel[MTK_SPI_PAD_SEL]);
	spi_debug("||**************%s end**************||\n", __func__);
}

static int spi_req_infra(int ctrl_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_SPI_CONTROL,
		MTK_SPI_KERNEL_OP_REQ_INFRA,
		ctrl_id, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static int spi_rel_infra(int ctrl_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_SPI_CONTROL,
		MTK_SPI_KERNEL_OP_REL_INFRA,
		ctrl_id, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static void mtk_spi_reset(struct mtk_spi *mdata)
{
	u32 reg_val;

	/* set the software reset bit in SPI_CMD_REG. */
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val |= SPI_CMD_RST;
	writel(reg_val, mdata->base + SPI_CMD_REG);

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST;
	if (mdata->dev_comp->sw_cs)
		reg_val &= ~SPI_CMD_CS_PIN_SEL;
	writel(reg_val, mdata->base + SPI_CMD_REG);
}

static int mtk_spi_set_hw_cs_timing(struct spi_device *spi)
{
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);
	struct spi_delay *cs_setup = &spi->cs_setup;
	struct spi_delay *cs_hold = &spi->cs_hold;
	struct spi_delay *cs_inactive = &spi->cs_inactive;
	u32 setup, hold, inactive;
	u32 reg_val;
	int delay;

	delay = spi_delay_to_ns(cs_setup, NULL);
	if (delay < 0)
		return delay;
	setup = (delay * DIV_ROUND_UP(mdata->spi_clk_hz, 1000000)) / 1000;

	delay = spi_delay_to_ns(cs_hold, NULL);
	if (delay < 0)
		return delay;
	hold = (delay * DIV_ROUND_UP(mdata->spi_clk_hz, 1000000)) / 1000;

	delay = spi_delay_to_ns(cs_inactive, NULL);
	if (delay < 0)
		return delay;
	inactive = (delay * DIV_ROUND_UP(mdata->spi_clk_hz, 1000000)) / 1000;

	if (hold || setup) {
		reg_val = readl(mdata->base + SPI_CFG0_REG);
		if (mdata->dev_comp->enhance_timing) {
			if (hold) {
				hold = min_t(u32, hold, 0x10000);
				reg_val &= ~(0xffff << SPI_ADJUST_CFG0_CS_HOLD_OFFSET);
				reg_val |= (((hold - 1) & 0xffff)
					<< SPI_ADJUST_CFG0_CS_HOLD_OFFSET);
			}
			if (setup) {
				setup = min_t(u32, setup, 0x10000);
				reg_val &= ~(0xffff << SPI_ADJUST_CFG0_CS_SETUP_OFFSET);
				reg_val |= (((setup - 1) & 0xffff)
					<< SPI_ADJUST_CFG0_CS_SETUP_OFFSET);
			}
		} else {
			if (hold) {
				hold = min_t(u32, hold, 0x100);
				reg_val &= ~(0xff << SPI_CFG0_CS_HOLD_OFFSET);
				reg_val |= (((hold - 1) & 0xff) << SPI_CFG0_CS_HOLD_OFFSET);
			}
			if (setup) {
				setup = min_t(u32, setup, 0x100);
				reg_val &= ~(0xff << SPI_CFG0_CS_SETUP_OFFSET);
				reg_val |= (((setup - 1) & 0xff)
					<< SPI_CFG0_CS_SETUP_OFFSET);
			}
		}
		writel(reg_val, mdata->base + SPI_CFG0_REG);
	}

	if (inactive) {
		inactive = min_t(u32, inactive, 0x100);
		reg_val = readl(mdata->base + SPI_CFG1_REG);
		reg_val &= ~SPI_CFG1_CS_IDLE_MASK;
		reg_val |= (((inactive - 1) & 0xff) << SPI_CFG1_CS_IDLE_OFFSET);
		writel(reg_val, mdata->base + SPI_CFG1_REG);
	}

	return 0;
}

static int mtk_spi_unprepare_message(struct spi_master *master,
				   struct spi_message *msg)
{
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (mdata->dev_comp->sw_cs) {
		mdata->state = MTK_SPI_IDLE;
		mtk_spi_reset(mdata);
	}
	return 0;
}

static int mtk_spi_prepare_message(struct spi_master *master,
				   struct spi_message *msg)
{
	u16 cpha, cpol;
	u32 reg_val;
	struct spi_device *spi = msg->spi;
	struct mtk_chip_config *chip_config = spi->controller_data;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	cpha = spi->mode & SPI_CPHA ? 1 : 0;
	cpol = spi->mode & SPI_CPOL ? 1 : 0;

	spi_debug("cpha:%d cpol:%d. chip_config as below\n", cpha, cpol);
	spi_dump_config(master, msg);

	reg_val = readl(mdata->base + SPI_CMD_REG);
	if (mdata->dev_comp->ipm_design) {
		/* SPI transfer without idle time until packet length done */
		reg_val |= SPI_CMD_NONIDLE_MODE;
		/* SW CS need setting CS_PIN_SEL bit */
		if (mdata->dev_comp->sw_cs)
			reg_val |= SPI_CMD_CS_PIN_SEL;
		if (spi->mode & SPI_LOOP)
			reg_val |= SPI_CMD_SPIM_LOOP;
		else
			reg_val &= ~SPI_CMD_SPIM_LOOP;
	}

	if (cpha)
		reg_val |= SPI_CMD_CPHA;
	else
		reg_val &= ~SPI_CMD_CPHA;
	if (cpol)
		reg_val |= SPI_CMD_CPOL;
	else
		reg_val &= ~SPI_CMD_CPOL;

	/* set the mlsbx and mlsbtx */
	if (spi->mode & SPI_LSB_FIRST) {
		reg_val &= ~SPI_CMD_TXMSBF;
		reg_val &= ~SPI_CMD_RXMSBF;
	} else {
		reg_val |= SPI_CMD_TXMSBF;
		reg_val |= SPI_CMD_RXMSBF;
	}

	/* set the tx/rx endian */
#ifdef __LITTLE_ENDIAN
	reg_val &= ~SPI_CMD_TX_ENDIAN;
	reg_val &= ~SPI_CMD_RX_ENDIAN;
#else
	reg_val |= SPI_CMD_TX_ENDIAN;
	reg_val |= SPI_CMD_RX_ENDIAN;
#endif

	if (mdata->dev_comp->enhance_timing) {
		/* set CS polarity */
		if (spi->mode & SPI_CS_HIGH)
			reg_val |= SPI_CMD_CS_POL;
		else
			reg_val &= ~SPI_CMD_CS_POL;

		if (chip_config->sample_sel)
			reg_val |= SPI_CMD_SAMPLE_SEL;
		else
			reg_val &= ~SPI_CMD_SAMPLE_SEL;
	}

	/* disable dma mode */
	reg_val &= ~(SPI_CMD_TX_DMA | SPI_CMD_RX_DMA);

	/* disable deassert mode */
	reg_val &= ~SPI_CMD_DEASSERT;

	writel(reg_val, mdata->base + SPI_CMD_REG);

	/* pad select */
	if (mdata->dev_comp->need_pad_sel) {
		writel(mdata->pad_sel[MTK_SPI_PAD_SEL],
		       mdata->base + SPI_PAD_SEL_REG);
		if (mdata->dev_comp->slice_en) {
			/* open slice_en rx and tx */
			reg_val = readl(mdata->base + SPI_SLICE_EN_REG);
			/* do RX slice_en setting */
			reg_val &= ~SPI_SLICE_EN_RX_WIDTH_MASK;
			reg_val |= ((0x1 << mdata->pad_sel[MTK_SPI_PAD_SEL])
					<< SPI_SLICE_EN_RX_OFFSET);
			/* do TX slice en setting */
			reg_val &= ~SPI_SLICE_EN_TX_WIDTH_MASK;
			reg_val |= ((0x1 << mdata->pad_sel[MTK_SPI_PAD_SEL])
					<< SPI_SLICE_EN_TX_OFFSET);
			writel(reg_val, mdata->base + SPI_SLICE_EN_REG);
		}
	}

	/* tick delay */
	if (mdata->dev_comp->enhance_timing) {
		if (mdata->dev_comp->ipm_design) {
			reg_val = readl(mdata->base + SPI_CMD_REG);
			if (!mdata->dev_comp->slice_en){
				reg_val &= ~SPI_CMD_GET_TICKDLY_MASK;
				reg_val |= ((chip_config->tick_delay & 0x7)
						<< SPI_CMD_GET_TICKDLY_OFFSET);
				writel(reg_val, mdata->base + SPI_CMD_REG);
			} else {
				reg_val &= ~SPI_CMD_GET_TICKDLY_SLICE_ON_MASK;
				reg_val |= ((chip_config->tick_delay & 0x7F)
						<< SPI_CMD_GET_TICKDLY_OFFSET);
				writel(reg_val, mdata->base + SPI_CMD_REG);
			}
		} else {
			reg_val = readl(mdata->base + SPI_CFG1_REG);
			reg_val &= ~SPI_CFG1_GET_TICK_DLY_MASK;
			reg_val |= ((chip_config->tick_delay & 0x7)
				    << SPI_CFG1_GET_TICK_DLY_OFFSET);
			writel(reg_val, mdata->base + SPI_CFG1_REG);
		}
	} else {
		reg_val = readl(mdata->base + SPI_CFG1_REG);
		reg_val &= ~SPI_CFG1_GET_TICK_DLY_MASK_V1;
		reg_val |= ((chip_config->tick_delay & 0x3)
			    << SPI_CFG1_GET_TICK_DLY_OFFSET_V1);
		writel(reg_val, mdata->base + SPI_CFG1_REG);
	}

	/* set hw cs timing */
	mtk_spi_set_hw_cs_timing(spi);
	return 0;
}

static int mtk_spi_hw_init(struct spi_master *master,
			   struct spi_device *spi)
{
	u16 cpha, cpol;
	u32 reg_val;
	struct mtk_chip_config *chip_config = spi->controller_data;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	cpha = spi->mode & SPI_CPHA ? 1 : 0;
	cpol = spi->mode & SPI_CPOL ? 1 : 0;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	if (mdata->dev_comp->ipm_design) {
		/* SPI transfer without idle time until packet length done */
		reg_val |= SPI_CMD_NONIDLE_MODE;
		if (spi->mode & SPI_LOOP)
			reg_val |= SPI_CMD_SPIM_LOOP;
		else
			reg_val &= ~SPI_CMD_SPIM_LOOP;
	}

	if (cpha)
		reg_val |= SPI_CMD_CPHA;
	else
		reg_val &= ~SPI_CMD_CPHA;
	if (cpol)
		reg_val |= SPI_CMD_CPOL;
	else
		reg_val &= ~SPI_CMD_CPOL;

	/* set the mlsbx and mlsbtx */
	if (spi->mode & SPI_LSB_FIRST) {
		reg_val &= ~SPI_CMD_TXMSBF;
		reg_val &= ~SPI_CMD_RXMSBF;
	} else {
		reg_val |= SPI_CMD_TXMSBF;
		reg_val |= SPI_CMD_RXMSBF;
	}

	/* set the tx/rx endian */
#ifdef __LITTLE_ENDIAN
	reg_val &= ~SPI_CMD_TX_ENDIAN;
	reg_val &= ~SPI_CMD_RX_ENDIAN;
#else
	reg_val |= SPI_CMD_TX_ENDIAN;
	reg_val |= SPI_CMD_RX_ENDIAN;
#endif

	if (mdata->dev_comp->enhance_timing) {
		/* set CS polarity */
		if (spi->mode & SPI_CS_HIGH)
			reg_val |= SPI_CMD_CS_POL;
		else
			reg_val &= ~SPI_CMD_CS_POL;

		if (chip_config->sample_sel)
			reg_val |= SPI_CMD_SAMPLE_SEL;
		else
			reg_val &= ~SPI_CMD_SAMPLE_SEL;
	}

	/* set finish and pause interrupt always enable */
	reg_val |= SPI_CMD_FINISH_IE | SPI_CMD_PAUSE_IE;

	/* disable dma mode */
	reg_val &= ~(SPI_CMD_TX_DMA | SPI_CMD_RX_DMA);

	/* disable deassert mode */
	reg_val &= ~SPI_CMD_DEASSERT;

	writel(reg_val, mdata->base + SPI_CMD_REG);

	/* pad select */
	if (mdata->dev_comp->need_pad_sel) {
		writel(mdata->pad_sel[MTK_SPI_PAD_SEL],
		       mdata->base + SPI_PAD_SEL_REG);
		if (mdata->dev_comp->slice_en) {
			/* open slice_en rx and tx */
			reg_val = readl(mdata->base + SPI_SLICE_EN_REG);
			/* do RX slice_en setting */
			reg_val &= ~SPI_SLICE_EN_RX_WIDTH_MASK;
			reg_val |= ((0x1 << mdata->pad_sel[MTK_SPI_PAD_SEL])
					<< SPI_SLICE_EN_RX_OFFSET);
			/* do TX slice en setting */
			reg_val &= ~SPI_SLICE_EN_TX_WIDTH_MASK;
			reg_val |= ((0x1 << mdata->pad_sel[MTK_SPI_PAD_SEL])
					<< SPI_SLICE_EN_TX_OFFSET);
			writel(reg_val, mdata->base + SPI_SLICE_EN_REG);
		}
	}

	/* tick delay */
	if (mdata->dev_comp->enhance_timing) {
		if (mdata->dev_comp->ipm_design) {
			reg_val = readl(mdata->base + SPI_CMD_REG);
			if (!mdata->dev_comp->slice_en) {
				reg_val &= ~SPI_CMD_GET_TICKDLY_MASK;
				reg_val |= ((chip_config->tick_delay & 0x7)
						<< SPI_CMD_GET_TICKDLY_OFFSET);
				writel(reg_val, mdata->base + SPI_CMD_REG);
			} else {
				reg_val &= ~SPI_CMD_GET_TICKDLY_SLICE_ON_MASK;
				reg_val |= ((chip_config->tick_delay & 0x7F)
						<< SPI_CMD_GET_TICKDLY_OFFSET);
				writel(reg_val, mdata->base + SPI_CMD_REG);
			}
		} else {
			reg_val = readl(mdata->base + SPI_CFG1_REG);
			reg_val &= ~SPI_CFG1_GET_TICK_DLY_MASK;
			reg_val |= ((chip_config->tick_delay & 0x7)
				    << SPI_CFG1_GET_TICK_DLY_OFFSET);
			writel(reg_val, mdata->base + SPI_CFG1_REG);
		}
	} else {
		reg_val = readl(mdata->base + SPI_CFG1_REG);
		reg_val &= ~SPI_CFG1_GET_TICK_DLY_MASK_V1;
		reg_val |= ((chip_config->tick_delay & 0x3)
			    << SPI_CFG1_GET_TICK_DLY_OFFSET_V1);
		writel(reg_val, mdata->base + SPI_CFG1_REG);
	}

	/* set hw cs timing */
	mtk_spi_set_hw_cs_timing(spi);
	return 0;
}

static void mtk_spi_set_cs(struct spi_device *spi, bool enable)
{
	u32 reg_val;
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);

	if (spi->mode & SPI_CS_HIGH)
		enable = !enable;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	if (mdata->dev_comp->sw_cs) {
		if (!enable) {
			reg_val |= SPI_CMD_CS_POL;
			writel(reg_val, mdata->base + SPI_CMD_REG);
		} else {
			reg_val &= ~SPI_CMD_CS_POL;
			writel(reg_val, mdata->base + SPI_CMD_REG);
		}
	} else {
		if (!enable) {
			reg_val |= SPI_CMD_PAUSE_EN;
			writel(reg_val, mdata->base + SPI_CMD_REG);
		} else {
			reg_val &= ~SPI_CMD_PAUSE_EN;
			writel(reg_val, mdata->base + SPI_CMD_REG);
			mdata->state = MTK_SPI_IDLE;
			mtk_spi_reset(mdata);
		}
	}
}

inline u32 spi_set_nbit(u32 nbit)
{
	u32 ret = 0;

	switch (nbit) {
	case SPI_NBITS_SINGLE:
		ret = 0x0;
		break;
	case SPI_NBITS_DUAL:
		ret = 0x1;
		break;
	case SPI_NBITS_QUAD:
		ret = 0x2;
		break;
	default:
		pr_info("unknown spi nbit mode, use single mode!");
		break;
	}
	return ret;
}

static void mtk_spi_prepare_transfer(struct spi_master *master,
				     u32 speed_hz)
{
	u32 div, sck_time, reg_val, cs_setup_reg_val, cs_setup_time;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (speed_hz && (speed_hz < mdata->spi_clk_hz / 2))
		div = DIV_ROUND_UP(mdata->spi_clk_hz, speed_hz);
	else
		div = 1;

	sck_time = (div + 1) / 2;

	if (mdata->dev_comp->enhance_timing) {
		reg_val = readl(mdata->base + SPI_CFG2_REG);
		reg_val &= ~(0xffff << SPI_CFG2_SCK_HIGH_OFFSET);
		reg_val |= (((sck_time - 1) & 0xffff)
			   << SPI_CFG2_SCK_HIGH_OFFSET);
		reg_val &= ~(0xffff << SPI_CFG2_SCK_LOW_OFFSET);
		reg_val |= (((sck_time - 1) & 0xffff)
			   << SPI_CFG2_SCK_LOW_OFFSET);
		writel(reg_val, mdata->base + SPI_CFG2_REG);
		/* setting default value to protect the potential issue
		 * which caused by user missing to set this value.
		 */
		reg_val = readl(mdata->base + SPI_CFG0_REG);
		cs_setup_reg_val = ((reg_val >> SPI_ADJUST_CFG0_CS_SETUP_OFFSET) & 0xFFFF);
		if (cs_setup_reg_val == 0) {
		/* only delay is 0 ,we setting default cs_setup_cnt val */
			cs_setup_time = sck_time * 2;
			reg_val = readl(mdata->base + SPI_CFG0_REG);
			reg_val &= ~(0xffff << SPI_ADJUST_CFG0_CS_SETUP_OFFSET);
			reg_val |= (((cs_setup_time - 1) & 0xffff)
			   << SPI_ADJUST_CFG0_CS_SETUP_OFFSET);
			writel(reg_val, mdata->base + SPI_CFG0_REG);
		}
	} else {
		reg_val = readl(mdata->base + SPI_CFG0_REG);
		reg_val &= ~(0xff << SPI_CFG0_SCK_HIGH_OFFSET);
		reg_val |= (((sck_time - 1) & 0xff)
			   << SPI_CFG0_SCK_HIGH_OFFSET);
		reg_val &= ~(0xff << SPI_CFG0_SCK_LOW_OFFSET);
		reg_val |= (((sck_time - 1) & 0xff) << SPI_CFG0_SCK_LOW_OFFSET);
		writel(reg_val, mdata->base + SPI_CFG0_REG);
	}
}

static void mtk_spi_setup_packet(struct spi_master *master)
{
	u32 packet_size, packet_loop, reg_val, packet_size_low, packet_size_high;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	reg_val = readl(mdata->base + SPI_CFG1_REG);
	if (!mdata->dev_comp->enhance_packet_len) {
		if (mdata->dev_comp->ipm_design)
			packet_size = min_t(u32,
						mdata->xfer_len,
						MTK_SPI_IPM_PACKET_SIZE);
		else
			packet_size = min_t(u32,
						mdata->xfer_len,
						MTK_SPI_PACKET_SIZE);
		packet_loop = mdata->xfer_len / packet_size;

		if (mdata->dev_comp->ipm_design)
			reg_val &= ~(SPI_CFG1_IPM_PACKET_LENGTH_MASK | SPI_CFG1_PACKET_LOOP_MASK);
		else
			reg_val &= ~(SPI_CFG1_PACKET_LENGTH_MASK | SPI_CFG1_PACKET_LOOP_MASK);

		reg_val |= (packet_size - 1) << SPI_CFG1_PACKET_LENGTH_OFFSET;
		reg_val |= (packet_loop - 1) << SPI_CFG1_PACKET_LOOP_OFFSET;
		writel(reg_val, mdata->base + SPI_CFG1_REG);
	} else {
		reg_val &= ~(SPI_CFG1_IPM_PACKET_LENGTH_MASK | SPI_CFG1_PACKET_LOOP_MASK);
		packet_size_low = (mdata->xfer_len - 1) & SPI_PACKET_LENGTH_LOW_MASK;
		packet_size_high = ((mdata->xfer_len - 1) & SPI_PACKET_LENGTH_HIGH_MASK)
			>> SPI_PACKET_LENGTH_HIGH_OFFSET;
		reg_val |= packet_size_low << SPI_CFG1_PACKET_LENGTH_OFFSET;
		writel(reg_val, mdata->base + SPI_CFG1_REG);
		writel(packet_size_high, mdata->base + SPI_CFG5_REG);
	}
}

static void mtk_spi_enable_transfer(struct spi_master *master)
{
	u32 cmd;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	cmd = readl(mdata->base + SPI_CMD_REG);
	if (mdata->state == MTK_SPI_IDLE)
		cmd |= SPI_CMD_ACT;
	else
		cmd |= SPI_CMD_RESUME;
	writel(cmd, mdata->base + SPI_CMD_REG);
}

static int mtk_spi_get_mult_delta(struct mtk_spi *mdata, u32 xfer_len)
{
	u32 mult_delta = 0;

	if (!mdata->dev_comp->enhance_packet_len) {
		if (mdata->dev_comp->ipm_design) {
			if (xfer_len > MTK_SPI_IPM_PACKET_SIZE)
				mult_delta = xfer_len % MTK_SPI_IPM_PACKET_SIZE;
		} else {
			if (xfer_len > MTK_SPI_PACKET_SIZE)
				mult_delta = xfer_len % MTK_SPI_PACKET_SIZE;
		}
	}

	return mult_delta;
}

static void mtk_spi_update_mdata_len(struct spi_master *master)
{
	int mult_delta;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (mdata->tx_sgl_len && mdata->rx_sgl_len) {
		if (mdata->tx_sgl_len > mdata->rx_sgl_len) {
			mult_delta = mtk_spi_get_mult_delta(mdata, mdata->rx_sgl_len);
			mdata->xfer_len = mdata->rx_sgl_len - mult_delta;
			mdata->rx_sgl_len = mult_delta;
			mdata->tx_sgl_len -= mdata->xfer_len;
		} else {
			mult_delta = mtk_spi_get_mult_delta(mdata, mdata->tx_sgl_len);
			mdata->xfer_len = mdata->tx_sgl_len - mult_delta;
			mdata->tx_sgl_len = mult_delta;
			mdata->rx_sgl_len -= mdata->xfer_len;
		}
	} else if (mdata->tx_sgl_len) {
		mult_delta = mtk_spi_get_mult_delta(mdata, mdata->tx_sgl_len);
		mdata->xfer_len = mdata->tx_sgl_len - mult_delta;
		mdata->tx_sgl_len = mult_delta;
	} else if (mdata->rx_sgl_len) {
		mult_delta = mtk_spi_get_mult_delta(mdata, mdata->rx_sgl_len);
		mdata->xfer_len = mdata->rx_sgl_len - mult_delta;
		mdata->rx_sgl_len = mult_delta;
	}
}

static void mtk_spi_setup_dma_addr(struct spi_master *master,
				   struct spi_transfer *xfer)
{
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (mdata->tx_sgl) {
		writel((u32)(xfer->tx_dma & MTK_SPI_32BITS_MASK),
		       mdata->base + SPI_TX_SRC_REG);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		if (mdata->dev_comp->dma_ext)
			writel((u32)(xfer->tx_dma >> 32),
			       mdata->base + SPI_TX_SRC_REG_64);
#endif
	}

	if (mdata->rx_sgl) {
		writel((u32)(xfer->rx_dma & MTK_SPI_32BITS_MASK),
		       mdata->base + SPI_RX_DST_REG);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		if (mdata->dev_comp->dma_ext)
			writel((u32)(xfer->rx_dma >> 32),
			       mdata->base + SPI_RX_DST_REG_64);
#endif
	}
}

static int mtk_spi_fifo_transfer(struct spi_master *master,
				 struct spi_device *spi,
				 struct spi_transfer *xfer)
{
	u32 reg_val, cnt, remainder, len, irq_status;
	int ret;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	mdata->cur_transfer = xfer;
	mdata->num_xfered = 0;
	mdata->dma_en = false;
	mtk_spi_prepare_transfer(master, xfer->speed_hz);

	//disable irq
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~(SPI_CMD_FINISH_IE | SPI_CMD_PAUSE_IE);
	writel(reg_val, mdata->base + SPI_CMD_REG);

	while (1) {
		len = xfer->len - mdata->num_xfered;
		mdata->xfer_len = min(MTK_SPI_MAX_FIFO_SIZE, len);
		mtk_spi_setup_packet(master);

		if (xfer->tx_buf) {
			cnt = mdata->xfer_len / 4;
			iowrite32_rep(mdata->base + SPI_TX_DATA_REG,
					xfer->tx_buf + mdata->num_xfered, cnt);

			remainder = mdata->xfer_len % 4;
			if (remainder > 0) {
				reg_val = 0;
				memcpy(&reg_val,
				xfer->tx_buf + (cnt * 4) + mdata->num_xfered,
				remainder);
				writel(reg_val, mdata->base + SPI_TX_DATA_REG);
			}
		}

		spi_debug("spi setting Done.Dump reg before Transfer start:\n");
		spi_dump_reg(mdata, master);

		mtk_spi_enable_transfer(master);

		ret = readl_poll_timeout_atomic(mdata->base + SPI_STATUS1_REG, irq_status,
			irq_status, 1, SPI_FIFO_POLLING_TIMEOUT);
		if (ret) {
			dev_err(mdata->dev, "SPI PIO polling timeout.\n");
			mtk_spi_error_dump(spi->controller, NULL);
			return ret;
		}

		reg_val = readl(mdata->base + SPI_STATUS0_REG);
		if (reg_val & MTK_SPI_PAUSE_INT_STATUS)
			mdata->state = MTK_SPI_PAUSED;
		else
			mdata->state = MTK_SPI_IDLE;

		if (xfer->rx_buf) {
			cnt = mdata->xfer_len / 4;
			ioread32_rep(mdata->base + SPI_RX_DATA_REG,
					xfer->rx_buf + mdata->num_xfered, cnt);
			remainder = mdata->xfer_len % 4;
			if (remainder > 0) {
				reg_val = readl(mdata->base + SPI_RX_DATA_REG);
				memcpy(xfer->rx_buf +
					mdata->num_xfered +
					(cnt * 4),
					&reg_val,
					remainder);
			}
		}

		mdata->num_xfered += mdata->xfer_len;
		if (mdata->num_xfered == xfer->len)
			break;
	}

	return 0;
}

static int mtk_spi_dma_transfer(struct spi_master *master,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	int cmd;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	mdata->tx_sgl = NULL;
	mdata->rx_sgl = NULL;
	mdata->tx_sgl_len = 0;
	mdata->rx_sgl_len = 0;
	mdata->cur_transfer = xfer;
	mdata->num_xfered = 0;
	mdata->dma_en = true;

	mtk_spi_prepare_transfer(master, xfer->speed_hz);

	cmd = readl(mdata->base + SPI_CMD_REG);
	//enable irq
	if (mdata->dev_comp->sw_cs)
		/* set finish interrupt enable */
		cmd |= SPI_CMD_FINISH_IE;
	else
		/* set finish and pause interrupt always enable */
		cmd |= SPI_CMD_FINISH_IE | SPI_CMD_PAUSE_IE;

	if (xfer->tx_buf)
		cmd |= SPI_CMD_TX_DMA;
	if (xfer->rx_buf)
		cmd |= SPI_CMD_RX_DMA;
	writel(cmd, mdata->base + SPI_CMD_REG);

	if (xfer->tx_buf)
		mdata->tx_sgl = xfer->tx_sg.sgl;
	if (xfer->rx_buf)
		mdata->rx_sgl = xfer->rx_sg.sgl;

	if (mdata->tx_sgl) {
		xfer->tx_dma = sg_dma_address(mdata->tx_sgl);
		mdata->tx_sgl_len = sg_dma_len(mdata->tx_sgl);
	}
	if (mdata->rx_sgl) {
		xfer->rx_dma = sg_dma_address(mdata->rx_sgl);
		mdata->rx_sgl_len = sg_dma_len(mdata->rx_sgl);
	}

	mtk_spi_update_mdata_len(master);
	mtk_spi_setup_packet(master);
	mtk_spi_setup_dma_addr(master, xfer);

	spi_debug("spi setting Done.Dump reg before Transfer start:\n");
	spi_dump_reg(mdata, master);

	mtk_spi_enable_transfer(master);

	return 1;
}

static int mtk_spi_transfer_one(struct spi_master *master,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);
	u32 reg_val = 0;

	if (mdata->dev_comp->ipm_design) {
		reg_val = 0;
		if (xfer->tx_buf && xfer->rx_buf)
			reg_val &= ~SPI_CFG3_HALF_DUPLEX_EN;
		else if (xfer->tx_buf) {
			reg_val |= SPI_CFG3_HALF_DUPLEX_EN;
			reg_val &= ~SPI_CFG3_HALF_DUPLEX_DIR;
			reg_val |= spi_set_nbit(xfer->tx_nbits);
		} else {
			reg_val |= SPI_CFG3_HALF_DUPLEX_EN;
			reg_val |= SPI_CFG3_HALF_DUPLEX_DIR;
			reg_val |= spi_set_nbit(xfer->rx_nbits);
		}
		writel(reg_val, mdata->base + SPI_CFG3_REG);
	}

	mdata->err_occur = false;

	if (master->can_dma(master, spi, xfer))
		return mtk_spi_dma_transfer(master, spi, xfer);
	else
		return mtk_spi_fifo_transfer(master, spi, xfer);
}

static bool mtk_spi_can_dma(struct spi_master *master,
			    struct spi_device *spi,
			    struct spi_transfer *xfer)
{
	/* Buffers for DMA transactions must be 4-byte aligned */
	return (xfer->len > MTK_SPI_MAX_FIFO_SIZE &&
		(unsigned long)xfer->tx_buf % 4 == 0 &&
		(unsigned long)xfer->rx_buf % 4 == 0);
}

static int mtk_spi_setup(struct spi_device *spi)
{
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);
	struct mtk_chip_config *spicfg = spi->controller_data;
	struct device_node *np = spi->master->dev.parent->of_node;
	u32 prop;

	if (spicfg == NULL && np) {
		spicfg = kzalloc(sizeof(*spicfg), GFP_KERNEL);
		if (!spicfg)
			return -ENOMEM;
		*spicfg = mtk_default_chip_info;
		/* override with dt configured values */
		if (!of_property_read_u32_index(np, "mediatek,tickdly",
						mdata->pad_sel[MTK_SPI_PAD_SEL],
						&prop)) {
			if (prop < (mdata->dev_comp->slice_en ? MTK_SPI_SLICE_EN_MAX_TICK_DLY : MTK_SPI_MAX_TICK_DLY)) {
				spicfg->tick_delay = prop;
				dev_info(&spi->dev, "setup pad[%d] tickdly[%d] by dts\n",
					mdata->pad_sel[MTK_SPI_PAD_SEL], spicfg->tick_delay);
			} else {
				return -EINVAL;
			}
		}
		spi->controller_data = spicfg;
	}

	if (mdata->dev_comp->need_pad_sel && spi->cs_gpiod)
		gpiod_direction_output(spi->cs_gpiod, !(spi->mode & SPI_CS_HIGH));

	return 0;
}

static void mtk_spi_cleanup(struct spi_device *spi)
{
	struct mtk_chip_config *spicfg = spi->controller_data;

	spi->controller_data = NULL;
	if (spi->dev.of_node)
		kfree(spicfg);
}

static irqreturn_t mtk_spi_interrupt(int irq, void *dev_id)
{
	u32 cmd, reg_val;
	struct spi_master *master = dev_id;
	struct mtk_spi *mdata = spi_master_get_devdata(master);
	struct spi_transfer *trans = mdata->cur_transfer;

	spin_lock(&mdata->eh_spi_lock);
	if (!mdata->dma_en || mdata->err_occur)
		goto out;

	reg_val = readl(mdata->base + SPI_STATUS0_REG);
	if (reg_val & MTK_SPI_PAUSE_INT_STATUS)
		mdata->state = MTK_SPI_PAUSED;
	else if (reg_val & MTK_SPI_IDLE_INT_STATUS)
		mdata->state = MTK_SPI_IDLE;
	else
		goto out;

	/* SPI-MEM ops */
	if (mdata->use_spimem) {
		complete(&mdata->spimem_done);
		goto out;
	}

	if (mdata->tx_sgl)
		trans->tx_dma += mdata->xfer_len;
	if (mdata->rx_sgl)
		trans->rx_dma += mdata->xfer_len;

	if (mdata->tx_sgl && (mdata->tx_sgl_len == 0)) {
		mdata->tx_sgl = sg_next(mdata->tx_sgl);
		if (mdata->tx_sgl) {
			trans->tx_dma = sg_dma_address(mdata->tx_sgl);
			mdata->tx_sgl_len = sg_dma_len(mdata->tx_sgl);
		}
	}
	if (mdata->rx_sgl && (mdata->rx_sgl_len == 0)) {
		mdata->rx_sgl = sg_next(mdata->rx_sgl);
		if (mdata->rx_sgl) {
			trans->rx_dma = sg_dma_address(mdata->rx_sgl);
			mdata->rx_sgl_len = sg_dma_len(mdata->rx_sgl);
		}
	}

	if (!mdata->tx_sgl && !mdata->rx_sgl) {
		/* spi disable dma */
		cmd = readl(mdata->base + SPI_CMD_REG);
		cmd &= ~SPI_CMD_TX_DMA;
		cmd &= ~SPI_CMD_RX_DMA;
		writel(cmd, mdata->base + SPI_CMD_REG);

		spi_finalize_current_transfer(master);
		spi_debug("The last DMA transfer Done.\n");
		goto out;
	}

	spi_debug("One DMA transfer Done.Start Next\n");

	mtk_spi_update_mdata_len(master);
	mtk_spi_setup_packet(master);
	mtk_spi_setup_dma_addr(master, trans);
	mtk_spi_enable_transfer(master);

out:
	spin_unlock(&mdata->eh_spi_lock);
	return IRQ_HANDLED;
}

static int mtk_spi_mem_adjust_op_size(struct spi_mem *mem,
				      struct spi_mem_op *op)
{
	int opcode_len;
	struct mtk_spi *mdata = spi_master_get_devdata(mem->spi->master);
	struct spi_master *master = mem->spi->master;

	if (!mdata->dev_comp->enhance_packet_len) {
		if (op->data.dir != SPI_MEM_NO_DATA) {
			opcode_len = 1 + op->addr.nbytes + op->dummy.nbytes;
			if (opcode_len + op->data.nbytes > MTK_SPI_IPM_PACKET_SIZE) {
				op->data.nbytes = MTK_SPI_IPM_PACKET_SIZE - opcode_len;
				/* force data buffer dma-aligned. */
				op->data.nbytes -= op->data.nbytes % 4;
				spi_debug("%s, adjust op->data.nbytes[%d], opcode_len[%d]\n",
					__func__, op->data.nbytes, opcode_len);
			}
		}
	}

	return 0;
}

static bool mtk_spi_mem_supports_op(struct spi_mem *mem,
				    const struct spi_mem_op *op)
{
	struct mtk_spi *mdata = spi_master_get_devdata(mem->spi->master);
	struct spi_master *master = mem->spi->master;

	if (!spi_mem_default_supports_op(mem, op))
		return false;

	if (op->addr.nbytes && op->dummy.nbytes &&
	    op->addr.buswidth != op->dummy.buswidth)
		return false;

	if (op->addr.nbytes + op->dummy.nbytes > 15)
		return false;

	if ((op->data.dir != SPI_MEM_NO_DATA) && (!op->data.nbytes))
		return false;

	if ((!mdata->dev_comp->enhance_packet_len) &&
		(op->data.nbytes > MTK_SPI_IPM_PACKET_SIZE)) {
		if (op->data.nbytes % MTK_SPI_IPM_PACKET_SIZE != 0 ||
			op->data.nbytes / MTK_SPI_IPM_PACKET_SIZE >
		    MTK_SPI_IPM_PACKET_LOOP)
			return false;
	}
	spi_debug("%s op->cmd.nbytes(%d), op->addr.nbytes(%d), op->dummy.nbytes(%d), op->data.nbytes(%d)!\n",
		__func__, op->cmd.nbytes, op->addr.nbytes, op->dummy.nbytes, op->data.nbytes);
	return true;
}

static void mtk_spi_mem_setup_dma_xfer(struct spi_master *master,
				       const struct spi_mem_op *op)
{
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	writel((u32)(mdata->tx_dma & MTK_SPI_32BITS_MASK),
	       mdata->base + SPI_TX_SRC_REG);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	if (mdata->dev_comp->dma_ext)
		writel((u32)(mdata->tx_dma >> 32),
		       mdata->base + SPI_TX_SRC_REG_64);
#endif

	if (op->data.dir == SPI_MEM_DATA_IN) {
		writel((u32)(mdata->rx_dma & MTK_SPI_32BITS_MASK),
		       mdata->base + SPI_RX_DST_REG);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		if (mdata->dev_comp->dma_ext)
			writel((u32)(mdata->rx_dma >> 32),
			       mdata->base + SPI_RX_DST_REG_64);
#endif
	}
}

static int mtk_spi_transfer_wait(struct spi_mem *mem,
				 const struct spi_mem_op *op)
{
	struct mtk_spi *mdata = spi_master_get_devdata(mem->spi->master);
	/*
	 * For each byte we wait for 8 cycles of the SPI clock.
	 * Since speed is defined in Hz and we want milliseconds,
	 * so it should be 8 * 1000.
	 */
	u64 ms = 8000LL;

	if (op->data.dir == SPI_MEM_NO_DATA)
		ms *= 32; /* prevent we may get 0 for short transfers. */
	else
		ms *= op->data.nbytes;
	ms = div_u64(ms, mem->spi->max_speed_hz);
	ms += ms + 1000; /* 1s tolerance */

	if (ms > UINT_MAX)
		ms = UINT_MAX;

	if (!wait_for_completion_timeout(&mdata->spimem_done,
					 msecs_to_jiffies(ms))) {
		dev_err(mdata->dev, "spi-mem transfer timeout\n");
		mtk_spi_error_dump(mem->spi->controller, NULL);
		/* after timeout reset spi controller */
		mtk_spi_reset(mdata);
		return -ETIMEDOUT;
	}

	return 0;
}

static int mtk_spi_mem_exec_op(struct spi_mem *mem,
			       const struct spi_mem_op *op)
{
	struct mtk_spi *mdata = spi_master_get_devdata(mem->spi->master);
	struct spi_master *master = mem->spi->master;
	u32 reg_val, nio, tx_size, reg_val2;
	char *tx_tmp_buf, *rx_tmp_buf;
	int ret = 0;

	mdata->use_spimem = true;
	reinit_completion(&mdata->spimem_done);
	mdata->err_occur = false;
	mdata->dma_en = true;

	mtk_spi_reset(mdata);
	mtk_spi_hw_init(mem->spi->master, mem->spi);
	mtk_spi_prepare_transfer(mem->spi->master, mem->spi->max_speed_hz);

	reg_val = readl(mdata->base + SPI_CFG3_REG);
	/* opcode byte len */
	reg_val &= ~SPI_CFG3_CMD_BYTELEN_MASK;
	reg_val |= 1 << SPI_CFG3_CMD_BYTELEN_OFFSET;

	/* addr & dummy byte len */
	reg_val &= ~SPI_CFG3_ADDR_BYTELEN_MASK;
	if (op->addr.nbytes || op->dummy.nbytes)
		reg_val |= (op->addr.nbytes + op->dummy.nbytes) <<
				SPI_CFG3_ADDR_BYTELEN_OFFSET;

	/* data byte len */
	if (op->data.dir == SPI_MEM_NO_DATA) {
		reg_val |= SPI_CFG3_NODATA_FLAG;
		reg_val2 = readl(mdata->base + SPI_CFG1_REG);
		reg_val2 &= 0xff;
		writel(reg_val2, mdata->base + SPI_CFG1_REG);
	} else {
		reg_val &= ~SPI_CFG3_NODATA_FLAG;
		mdata->xfer_len = op->data.nbytes;
		mtk_spi_setup_packet(mem->spi->master);
	}

	if (op->addr.nbytes || op->dummy.nbytes) {
		if (op->addr.buswidth == 1 || op->dummy.buswidth == 1)
			reg_val |= SPI_CFG3_XMODE_EN;
		else
			reg_val &= ~SPI_CFG3_XMODE_EN;
	}

	if (op->addr.buswidth == 2 ||
	    op->dummy.buswidth == 2 ||
	    op->data.buswidth == 2)
		nio = 2;
	else if (op->addr.buswidth == 4 ||
		 op->dummy.buswidth == 4 ||
		 op->data.buswidth == 4)
		nio = 4;
	else
		nio = 1;

	reg_val &= ~SPI_CFG3_CMD_PIN_MODE_MASK;
	reg_val |= PIN_MODE_CFG(nio);

	reg_val |= SPI_CFG3_HALF_DUPLEX_EN;
	if (op->data.dir == SPI_MEM_DATA_IN)
		reg_val |= SPI_CFG3_HALF_DUPLEX_DIR;
	else
		reg_val &= ~SPI_CFG3_HALF_DUPLEX_DIR;
	writel(reg_val, mdata->base + SPI_CFG3_REG);

	tx_size = 1 + op->addr.nbytes + op->dummy.nbytes;
	if (op->data.dir == SPI_MEM_DATA_OUT)
		tx_size += op->data.nbytes;

	tx_size = max_t(u32, tx_size, 32);

	tx_tmp_buf = kzalloc(tx_size, GFP_KERNEL | GFP_DMA);
	if (!tx_tmp_buf) {
		mdata->use_spimem = false;
		return -ENOMEM;
	}

	tx_tmp_buf[0] = op->cmd.opcode;

	if (op->addr.nbytes) {
		int i;

		for (i = 0; i < op->addr.nbytes; i++)
			tx_tmp_buf[i + 1] = op->addr.val >>
					(8 * (op->addr.nbytes - i - 1));
	}

	if (op->dummy.nbytes)
		memset(tx_tmp_buf + op->addr.nbytes + 1,
		       0xff,
		       op->dummy.nbytes);

	if (op->data.nbytes && op->data.dir == SPI_MEM_DATA_OUT)
		memcpy(tx_tmp_buf + op->dummy.nbytes + op->addr.nbytes + 1,
		       op->data.buf.out,
		       op->data.nbytes);

	mdata->tx_dma = dma_map_single(mdata->dev, tx_tmp_buf,
				       tx_size, DMA_TO_DEVICE);
	if (dma_mapping_error(mdata->dev, mdata->tx_dma)) {
		ret = -ENOMEM;
		goto err_exit;
	}

	if (op->data.dir == SPI_MEM_DATA_IN) {
		if (!IS_ALIGNED((size_t)op->data.buf.in, 4)) {
			rx_tmp_buf = kzalloc(op->data.nbytes,
					     GFP_KERNEL | GFP_DMA);
			if (!rx_tmp_buf) {
				ret = -ENOMEM;
				goto unmap_tx_dma;
			}
		} else {
			rx_tmp_buf = op->data.buf.in;
		}

		mdata->rx_dma = dma_map_single(mdata->dev,
					       rx_tmp_buf,
					       op->data.nbytes,
					       DMA_FROM_DEVICE);
		if (dma_mapping_error(mdata->dev, mdata->rx_dma)) {
			ret = -ENOMEM;
			goto kfree_rx_tmp_buf;
		}
	}

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val |= SPI_CMD_TX_DMA;
	if (op->data.dir == SPI_MEM_DATA_IN)
		reg_val |= SPI_CMD_RX_DMA;
	writel(reg_val, mdata->base + SPI_CMD_REG);

	mtk_spi_mem_setup_dma_xfer(mem->spi->master, op);

	spi_debug("spi-mem setting Done.Dump reg before Transfer start:\n");
	spi_dump_reg(mdata, mem->spi->master);
	mtk_spi_enable_transfer(mem->spi->master);

	/* Wait for the interrupt. */
	ret = mtk_spi_transfer_wait(mem, op);
	if (ret)
		goto unmap_rx_dma;

	/* spi disable dma */
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_TX_DMA;
	if (op->data.dir == SPI_MEM_DATA_IN)
		reg_val &= ~SPI_CMD_RX_DMA;
	writel(reg_val, mdata->base + SPI_CMD_REG);

unmap_rx_dma:
	if (op->data.dir == SPI_MEM_DATA_IN) {
		dma_unmap_single(mdata->dev, mdata->rx_dma,
				 op->data.nbytes, DMA_FROM_DEVICE);
		if (!IS_ALIGNED((size_t)op->data.buf.in, 4))
			memcpy(op->data.buf.in, rx_tmp_buf, op->data.nbytes);
	}
kfree_rx_tmp_buf:
	if (op->data.dir == SPI_MEM_DATA_IN &&
	    !IS_ALIGNED((size_t)op->data.buf.in, 4))
		kfree(rx_tmp_buf);
unmap_tx_dma:
	dma_unmap_single(mdata->dev, mdata->tx_dma,
			 tx_size, DMA_TO_DEVICE);
err_exit:
	kfree(tx_tmp_buf);
	mdata->use_spimem = false;

	return ret;
}

static const struct spi_controller_mem_ops mtk_spi_mem_ops = {
	.adjust_op_size = mtk_spi_mem_adjust_op_size,
	.supports_op = mtk_spi_mem_supports_op,
	.exec_op = mtk_spi_mem_exec_op,
};

static int mtk_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct mtk_spi *mdata;
	const struct of_device_id *of_id;
	int i, irq, ret, addr_bits;
	const char *clk_type = NULL;

	master = spi_alloc_master(&pdev->dev, sizeof(*mdata));
	if (!master) {
		dev_err(&pdev->dev, "failed to alloc spi master\n");
		return -ENOMEM;
	}

	master->auto_runtime_pm = true;
	master->dev.of_node = pdev->dev.of_node;
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_LSB_FIRST;

	if (!of_property_read_bool(pdev->dev.of_node, "tee-only"))
		master->set_cs = mtk_spi_set_cs;

	master->prepare_message = mtk_spi_prepare_message;
	master->unprepare_message = mtk_spi_unprepare_message;
	master->transfer_one = mtk_spi_transfer_one;
	master->can_dma = mtk_spi_can_dma;
	master->setup = mtk_spi_setup;
	master->cleanup = mtk_spi_cleanup;
	master->set_cs_timing = mtk_spi_set_hw_cs_timing;
	master->handle_err = mtk_spi_error_dump;

	of_id = of_match_node(mtk_spi_of_match, pdev->dev.of_node);
	mdata = spi_master_get_devdata(master);
	if (!of_id) {
		dev_err(&pdev->dev, "failed to probe of_node\n");
		ret = -EINVAL;
		goto err_put_master;
	}

	mdata->dev_comp = of_id->data;
	mdata->dev = &pdev->dev;

	if (mdata->dev_comp->enhance_timing)
		master->mode_bits |= SPI_CS_HIGH;
	if (mdata->dev_comp->must_rx)
		master->flags |= SPI_CONTROLLER_MUST_RX;
	if (mdata->dev_comp->must_tx)
		master->flags |= SPI_CONTROLLER_MUST_TX;

	if (mdata->dev_comp->ipm_design) {
		master->mem_ops = &mtk_spi_mem_ops;
		init_completion(&mdata->spimem_done);
		master->mode_bits |= SPI_LOOP;
	}

	if (mdata->dev_comp->support_quad) {
		master->mode_bits |= SPI_RX_DUAL | SPI_TX_DUAL |
				     SPI_RX_QUAD | SPI_TX_QUAD;
	}

	if (mdata->dev_comp->need_pad_sel) {
		mdata->pad_num = of_property_count_u32_elems(
			pdev->dev.of_node,
			"mediatek,pad-select");
		if (mdata->pad_num < 0) {
			dev_err(&pdev->dev,
				"No 'mediatek,pad-select' property\n");
			ret = -EINVAL;
			goto err_put_master;
		}

		mdata->pad_sel = devm_kmalloc_array(&pdev->dev, mdata->pad_num,
						    sizeof(u32), GFP_KERNEL);
		if (!mdata->pad_sel) {
			ret = -ENOMEM;
			goto err_put_master;
		}

		for (i = 0; i < mdata->pad_num; i++) {
			ret = of_property_read_u32_index(pdev->dev.of_node,
						   "mediatek,pad-select",
						   i, &mdata->pad_sel[i]);
			if (ret < 0 || mdata->pad_sel[i] > MT8173_SPI_MAX_PAD_SEL) {
				dev_err(&pdev->dev,
					"getting pad_sel fail or wrong pad-sel[%d]: %u, ret = %d\n",
					i, mdata->pad_sel[i], ret);
				ret = -EINVAL;
				goto err_put_master;
			}
		}
	}

	platform_set_drvdata(pdev, master);
	mdata->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mdata->base)) {
		ret = PTR_ERR(mdata->base);
		goto err_put_master;
	}

	if (mdata->dev_comp->hw_reset) {
		if (!is_ioremaped) {
			reset_base = devm_platform_ioremap_resource(pdev, 1);
			if (IS_ERR(reset_base)) {
				ret = PTR_ERR(reset_base);
				goto err_put_master;
			} else {
				is_ioremaped = 1;
			}
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					"mediatek,reset-bit",
					&mdata->reset_bit);
		if (ret < 0 || mdata->reset_bit > MTK_SPI_MAX_RESET_BIT) {
			dev_err(&pdev->dev,
				"getting reset_bit fail or wrong reset_bit[%d], ret = %d\n",
				mdata->reset_bit, ret);
			ret = -EINVAL;
			goto err_put_master;
		}
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err_put_master;
	}

	mdata->irq = irq;
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (mdata->dev_comp->ipm_design)
		dma_set_max_seg_size(&pdev->dev, SZ_16M);
	else
		dma_set_max_seg_size(&pdev->dev, SZ_256K);

	ret = devm_request_irq(&pdev->dev, irq, mtk_spi_interrupt,
			       IRQF_TRIGGER_NONE | IRQF_NO_SUSPEND,
				   dev_name(&pdev->dev), master);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq (%d)\n", ret);
		goto err_put_master;
	}

	mdata->parent_clk = devm_clk_get(&pdev->dev, "parent-clk");
	if (IS_ERR(mdata->parent_clk)) {
		ret = PTR_ERR(mdata->parent_clk);
		dev_err(&pdev->dev, "failed to get parent-clk: %d\n", ret);
		goto err_put_master;
	}

	mdata->sel_clk = devm_clk_get(&pdev->dev, "sel-clk");
	if (IS_ERR(mdata->sel_clk)) {
		ret = PTR_ERR(mdata->sel_clk);
		dev_err(&pdev->dev, "failed to get sel-clk: %d\n", ret);
		goto err_put_master;
	}

	mdata->spi_clk = devm_clk_get(&pdev->dev, "spi-clk");
	if (IS_ERR(mdata->spi_clk)) {
		ret = PTR_ERR(mdata->spi_clk);
		dev_err(&pdev->dev, "failed to get spi-clk: %d\n", ret);
		goto err_put_master;
	}

	ret = clk_set_parent(mdata->sel_clk, mdata->parent_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to clk_set_parent (%d)\n", ret);
		goto err_put_master;
	}

	mdata->spi_clk_hz = clk_get_rate(mdata->spi_clk);

	spin_lock_init(&mdata->eh_spi_lock);

	ret = of_property_read_string_index(pdev->dev.of_node, "clock-source-type",
				0, &clk_type);
	if ((!ret) && (!strncmp(clk_type, "mainpll", strlen("mainpll"))))
		mdata->no_need_unprepare = true;
	else {
		mdata->no_need_unprepare = false;
		dev_info(&pdev->dev, "SPI probe, 'mainpll' not exist, set univpll as default!\n");
	}

	if (mdata->dev_comp->need_pad_sel) {
		if (mdata->pad_num != master->num_chipselect) {
			dev_err(&pdev->dev,
				"pad_num does not match num_chipselect(%d != %d)\n",
				mdata->pad_num, master->num_chipselect);
			ret = -EINVAL;
			goto err_put_master;
		}

		if (!master->cs_gpiods && master->num_chipselect > 1) {
			dev_err(&pdev->dev,
				"cs_gpiods not specified and num_chipselect > 1\n");
			ret = -EINVAL;
			goto err_put_master;
		}
	}

	if (mdata->dev_comp->dma_ext)
		addr_bits = DMA_ADDR_EXT_BITS;
	else
		addr_bits = DMA_ADDR_DEF_BITS;
	ret = device_create_file(&pdev->dev, &dev_attr_spi_log);
	if (ret)
		dev_notice(&pdev->dev, "SPI sysfs_create_file fail, ret:%d\n",
			ret);
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(addr_bits));
	if (ret)
		dev_notice(&pdev->dev, "SPI dma_set_mask(%d) failed, ret:%d\n",
			   addr_bits, ret);

	ret = of_property_read_u32_index(
				pdev->dev.of_node, "mediatek,autosuspend-delay",
				0, &mdata->auto_suspend_delay);
	if (ret < 0)
		mdata->auto_suspend_delay = 250;
	pm_runtime_set_autosuspend_delay(&pdev->dev, mdata->auto_suspend_delay);
	dev_info(&pdev->dev, "SPI probe, set auto_suspend delay = %dmS!\n",
				mdata->auto_suspend_delay);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	if (mdata->no_need_unprepare) {
		ret = clk_prepare(mdata->spi_clk);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to prepare spi_clk (%d)\n", ret);
			goto err_put_master;
		}
	}

	ret = devm_spi_register_master(&pdev->dev, master);
	if (ret) {
		dev_err(&pdev->dev, "failed to register master (%d)\n", ret);
		goto err_put_master;
	}

	return 0;

err_put_master:
	spi_master_put(master);

	return ret;
}

static int mtk_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	pm_runtime_disable(&pdev->dev);

	mtk_spi_reset(mdata);

	return 0;
}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
int stui_spi_lock(struct spi_master *spi)
{
	int ret = 0;
	struct mtk_spi *mdata = spi_master_get_devdata(spi);

	(void)mdata;

	spi_bus_lock(spi);

	pr_info("STUI %s enter\n", __func__);

#if IS_ENABLED(CONFIG_PM)
	ret = clk_enable(mdata->spi_clk);
	if (ret < 0) {
		pr_err("STUI failed to enable spi_clk (%d)\n", ret);
		spi_bus_unlock(spi);
		return ret;
	}
#endif//CONFIG_PM
	pr_info("STUI %s exit\n", __func__);
	return ret;
}
EXPORT_SYMBOL(stui_spi_lock);

int stui_spi_unlock(struct spi_master *spi)
{
	struct mtk_spi *mdata;

	(void)mdata;

	pr_info("STUI %s enter\n", __func__);

#if IS_ENABLED(CONFIG_PM)
	mdata = spi_master_get_devdata(spi);
	clk_disable(mdata->spi_clk);
#endif//CONFIG_PM

	spi_bus_unlock(spi);

	pr_info("STUI %s exit\n", __func__);
	return 0;
}
EXPORT_SYMBOL(stui_spi_unlock);
#endif//CONFIG_SAMSUNG_TUI

static int mtk_spi_runtime_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct mtk_spi *mdata = spi_controller_get_devdata(master);
	int res_val = -1;

	if (mdata->no_need_unprepare)
		clk_disable(mdata->spi_clk);
	else
		clk_disable_unprepare(mdata->spi_clk);
	if (mdata->dev_comp->infra_req) {
		/* do release infra action */
		res_val = spi_rel_infra(master->bus_num);
		if (res_val != 0)
			dev_err(dev, "fail to release infra\n");
	}
	return 0;
}

static int mtk_spi_runtime_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct mtk_spi *mdata = spi_controller_get_devdata(master);
	int ret;
	int res_val = -1;
	int retry = MTK_INFRA_REQ_RETRY_TIMES;

	if (mdata->suspend_delay_update) {
		pm_runtime_set_autosuspend_delay(dev, mdata->auto_suspend_delay);
		mdata->suspend_delay_update = false;
		dev_info(dev, "set auto_suspend delay = %dmS!\n", mdata->auto_suspend_delay);
	}
	if (mdata->dev_comp->infra_req) {
		/* because of using auto_suspend , it has grouped the infra_req for us */
		/* do infra_request by smc call*/
		do {
			res_val = spi_req_infra(master->bus_num);
			if (res_val == 0)
				break;
			dev_info(dev, "fail to req infra and then retry, res_val:%d\n",
				res_val);
		} while(retry--);

		if (res_val != 0) {
			dev_err(dev, "infra not ready may cause DEVAPC, try last time\n");
			res_val = spi_req_infra(master->bus_num);
			BUG_ON(res_val);
		}
	}

	if (mdata->no_need_unprepare)
		ret = clk_enable(mdata->spi_clk);
	else
		ret = clk_prepare_enable(mdata->spi_clk);

	if (ret < 0) {
		dev_err(dev, "failed to enable spi_clk (%d)\n", ret);
		if (mdata->dev_comp->infra_req) {
			res_val = spi_rel_infra(master->bus_num);
			dev_err(dev, "clk operation err and release infra res_val:%d\n", res_val);
		}
		return ret;
	}

	return 0;
}

void mtk_spi_set_autosuspend_delay(struct spi_device *spi, int delay_ms)
{
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);

	/*delay time longer than 10 seconds, WARNING ON*/
	WARN(delay_ms > 10000, "SPI auto suspend delay too long, pls review it!\n");

	if (!(delay_ms == mdata->auto_suspend_delay)) {
		mdata->auto_suspend_delay = delay_ms;
		mdata->suspend_delay_update = true;
	}
}
EXPORT_SYMBOL(mtk_spi_set_autosuspend_delay);

static int mtk_spi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	int ret;

	ret = spi_controller_suspend(master);
	if (ret)
		return ret;

	if (!pm_runtime_suspended(dev))
		ret = mtk_spi_runtime_suspend(dev);

	return ret;
}

static int mtk_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	int ret;


	if (!pm_runtime_suspended(dev)) {
		ret = mtk_spi_runtime_resume(dev);
		if (ret)
			goto out;
	}

	ret = spi_controller_resume(master);

out:
	return ret;
}

static const struct dev_pm_ops mtk_spi_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_spi_suspend, mtk_spi_resume)
	SET_RUNTIME_PM_OPS(mtk_spi_runtime_suspend,
			   mtk_spi_runtime_resume, NULL)
};

void mt_spi_disable_master_clk(struct spi_device *spidev)
{
	struct mtk_spi *ms;

	ms = spi_master_get_devdata(spidev->master);

	clk_disable_unprepare(ms->spi_clk);
}
EXPORT_SYMBOL(mt_spi_disable_master_clk);

void mt_spi_enable_master_clk(struct spi_device *spidev)
{
	int ret;
	struct mtk_spi *ms;

	ms = spi_master_get_devdata(spidev->master);

	ret = clk_prepare_enable(ms->spi_clk);

	BUG_ON(ret < 0);
}
EXPORT_SYMBOL(mt_spi_enable_master_clk);

static struct platform_driver mtk_spi_driver = {
	.driver = {
		.name = "mtk-spi",
		.pm = &mtk_spi_pm,
		.of_match_table = mtk_spi_of_match,
	},
	.probe = mtk_spi_probe,
	.remove = mtk_spi_remove,
};

module_platform_driver(mtk_spi_driver);

MODULE_DESCRIPTION("MTK SPI Controller driver");
MODULE_AUTHOR("Leilk Liu <leilk.liu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mtk-spi");
