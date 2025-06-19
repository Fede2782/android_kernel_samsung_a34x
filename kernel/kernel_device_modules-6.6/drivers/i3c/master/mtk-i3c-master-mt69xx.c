// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * Author: Mingchang Jia <mingchang.jia@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/i3c/master.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/timekeeping.h>

#define I3C_M_RD                      BIT(0)

#define I3C_INTR_ALL                  (0x3ff)
#define I3C_INTR_BUS_ERROR            BIT(8)
#define I3C_INTR_IBI                  BIT(7)
#define I3C_INTR_DMA                  BIT(6)
#define I3C_INTR_TIMEOUT              BIT(5)
#define I3C_INTR_RS                   BIT(4)
#define I3C_INTR_ARB                  BIT(3)
#define I3C_INTR_HS_ACKERR            BIT(2)
#define I3C_INTR_ACKERR               BIT(1)
#define I3C_INTR_COMP                 BIT(0)

#define I3C_CTRL_IRQ_SEL              BIT(7)
#define I3C_CTRL_TRANSFER_LEN_CHANGE  BIT(6)
#define I3C_CTRL_ACKERR_DET_EN        BIT(5)
#define I3C_CTRL_DIR_CHANGE           BIT(4)
#define I3C_CTRL_CLK_EXT_EN           BIT(3)
#define I3C_CTRL_DMA_EN               BIT(2)
#define I3C_CTRL_RS                   BIT(1)
#define I3C_CTRL_FORCE_HS             BIT(0)

#define I3C_START_RS_MUL_CONFIG       BIT(15)
#define I3C_START_RS_MUL_TRIG         BIT(14)
#define I3C_START_RS_MUL_TRIG_CLR     BIT(13)
#define I3C_START_KICK_OFF            BIT(1)
#define I3C_START_TRANSAC             BIT(0)

#define I3C_HS_HOLD_EN                BIT(15)
#define I3C_HS_SPEED                  BIT(7)
#define I3C_HS_NACK_DET               BIT(1)
#define I3C_HS_EN                     BIT(0)

#define I3C_IOCFG_OPEN_DRAIN          (0x3)
#define I3C_IOCFG_PUSH_PULL           (0x0)

#define I3C_FIFO_CLR_ALL              (0x7)
#define I3C_FIFO_CLR_RFIFO            BIT(2)
#define I3C_FIFO_CLR_HFIFO            BIT(1)
#define I3C_FIFO_CLR_FIFO             BIT(0)

#define I3C_RST_HANDSHAKE             BIT(5)
#define I3C_RST_WARM                  BIT(3)
#define I3C_RST_ERROR                 BIT(3)
#define I3C_RST_FSM                   BIT(2)
#define I3C_RST_MASTER                BIT(1)
#define I3C_RST_SOFT                  BIT(0)
#define I3C_RST_CLR                   (0)

#define I3C_TRF_HANDOFF               BIT(14)
#define I3C_TRF_IBI_EN                BIT(13)
#define I3C_TRF_SKIP_SLV_ADDR         BIT(10)
#define I3C_TRF_HEAD_ONLY             BIT(9)
#define I3C_TRF_PARITY_EXIT           BIT(8)
#define I3C_TRF_TBIT_EN               BIT(7)
#define I3C_TRF_DAA_EN                BIT(4)

#define I3C_DEFDA_DAA_SLV_PARITY      BIT(8)
#define I3C_DEFDA_USE_DEF_DA          BIT(7)

#define I3C_SHAPE_T_RS                BIT(3)
#define I3C_SHAPE_T_PARITY            BIT(2)
#define I3C_SHAPE_T_STALL             BIT(1)
#define I3C_SHAPE_T_FILL              BIT(0)
#define I3C_SHAPE_HDR_EXIT            BIT(0)

#define I3C_DBGCRL_PRE_COUNT          (0x8 << 8)
#define I3C_DBGCRL_BUS_ERR            BIT(2)

#define	HFIFO_CCC  (BIT(15) | BIT(10) | BIT(9) | BIT(8))
#define HFIFO_7E   (BIT(15) | BIT(8) | 0xFC)

#define I3C_EXT_CONF_VALUE            (0x125)
#define I3C_DELAY_LEN                 (0x02)

/* DMA Register Bit */
#define I3C_DMA_INT_FLAG_CLR          (0)
#define I3C_DMA_EN_START              BIT(0)

#define I3C_DMA_RST_HANDSHAKE         BIT(2)
#define I3C_DMA_RST_HARD              BIT(1)
#define I3C_DMA_RST_WARM              BIT(0)
#define I3C_DMA_RST_CLR               (0)

#define I3C_DMA_CON_BURST             BIT(12)
#define I3C_DMA_CON_DIR_CHANGE        BIT(9)
#define I3C_DMA_CON_SKIP_CONFIG       BIT(4)
#define I3C_DMA_CON_ASYNC_MODE        BIT(2)
#define I3C_DMA_CON_RX                BIT(0)
#define I3C_DMA_CON_TX                (0)
#define I3C_DMA_CON_DEFAULT_VALUE     (0x8)

#define MTK_I3C_MAX_DEVS              (32)
#define MTK_I3C_FIFO_SIZE             (16)

#define I3C_MAX_CLOCK_DIV_5BITS       (32)
#define I3C_MAX_LS_STEP_CNT_DIV       (64)
#define I3C_MAX_HS_STEP_CNT_DIV       (8)
#define I3C_MAX_SAMPLE_CNT_DIV        (8)
#define I3C_MAX_LS_EXT_TIME           (255)
#define I3C_MAX_HS_EXT_TIME           (127)
#define I3C_LS_FORCE_H_TIME           (40)
#define I3C_HS_FORCE_H_TIME           (40)
#define I3C_DUTY_DIFF_TENTHS          (40)
#define I3C_LS_DUTY                   (45)
#define I3C_HS_DUTY                   (50)

#define I3C_HEAD_SPEED (2300000)

/* Reference to core layer timeout (ns) */
#define I3C_POLLING_TIMEOUT (50000000)

enum I3C_REGS_OFFSET {
	OFFSET_DATA_PORT,
	OFFSET_SLAVE_ADDR,
	OFFSET_INTR_MASK,
	OFFSET_INTR_STAT,
	OFFSET_CONTROL,
	OFFSET_TRANSFER_LEN,
	OFFSET_TRANSAC_LEN,
	OFFSET_DELAY_LEN,
	OFFSET_TIMING,
	OFFSET_START,
	OFFSET_EXT_CONF,
	OFFSET_LTIMING,
	OFFSET_HS,
	OFFSET_IO_CONFIG,
	OFFSET_FIFO_ADDR_CLR,
	OFFSET_SDA_TIMING,
	OFFSET_MCU_INTR,
	OFFSET_TRANSFER_LEN_AUX,
	OFFSET_CLOCK_DIV,
	OFFSET_SOFTRESET,
	OFFSET_TRAFFIC,
	OFFSET_COMMAND,
	OFFSET_CRC_CODE,
	OFFSET_TERNARY,
	OFFSET_IBI_TIMING,
	OFFSET_DEF_DA,
	OFFSET_SHAPE,
	OFFSET_HFIFO_DATA,
	OFFSET_ERROR,
	OFFSET_DEBUGSTAT,
	OFFSET_DEBUGCTRL,
	OFFSET_DMA_FSM_DEBUG,
	OFFSET_MULTIMAS,
	OFFSET_FIFO_STAT,
	OFFSET_FIFO_THRESH,
	OFFSET_HFIFO_STAT,
	OFFSET_MULTI_DMA,
};

static const u16 mt_i3c_regs_v2[] = {
	[OFFSET_DATA_PORT] = 0x0,
	[OFFSET_SLAVE_ADDR] = 0x94,
	[OFFSET_INTR_MASK] = 0x8,
	[OFFSET_INTR_STAT] = 0xc,
	[OFFSET_CONTROL] = 0x10,
	[OFFSET_TRANSFER_LEN] = 0x14,
	[OFFSET_TRANSAC_LEN] = 0x18,
	[OFFSET_DELAY_LEN] = 0x1c,
	[OFFSET_TIMING] = 0x20,
	[OFFSET_START] = 0x24,
	[OFFSET_EXT_CONF] = 0x28,
	[OFFSET_LTIMING] = 0x2c,
	[OFFSET_HS] = 0x30,
	[OFFSET_IO_CONFIG] = 0x34,
	[OFFSET_FIFO_ADDR_CLR] = 0x38,
	[OFFSET_SDA_TIMING] = 0x3c,
	[OFFSET_MCU_INTR] = 0x40,
	[OFFSET_TRANSFER_LEN_AUX] = 0x44,
	[OFFSET_CLOCK_DIV] = 0x48,
	[OFFSET_SOFTRESET] = 0x50,
	[OFFSET_TRAFFIC] = 0x54,
	[OFFSET_COMMAND] = 0x58,
	[OFFSET_CRC_CODE] = 0x5c,
	[OFFSET_TERNARY] = 0x60,
	[OFFSET_IBI_TIMING] = 0x64,
	[OFFSET_DEF_DA] = 0x68,
	[OFFSET_SHAPE] = 0x6c,
	[OFFSET_HFIFO_DATA] = 0x70,
	[OFFSET_ERROR] = 0xd0,
	[OFFSET_DEBUGSTAT] = 0xe4,
	[OFFSET_DEBUGCTRL] = 0xe8,
	[OFFSET_DMA_FSM_DEBUG] = 0xec,
	[OFFSET_MULTIMAS] = 0xf0,
	[OFFSET_FIFO_STAT] = 0xf4,
	[OFFSET_FIFO_THRESH] = 0xf8,
	[OFFSET_HFIFO_STAT] = 0xfc,
	[OFFSET_MULTI_DMA] = 0x8c,
};

enum DMA_REGS_OFFSET {
	OFFSET_INT_FLAG = 0x0,
	OFFSET_INT_EN = 0x04,
	OFFSET_EN = 0x08,
	OFFSET_RST = 0x0c,
	OFFSET_STOP = 0x10,
	OFFSET_FLUSH = 0x14,
	OFFSET_CON = 0x18,
	OFFSET_TX_MEM_ADDR = 0x1c,
	OFFSET_RX_MEM_ADDR = 0x20,
	OFFSET_TX_LEN = 0x24,
	OFFSET_RX_LEN = 0x28,
	OFFSET_INT_BUF_SIZE = 0x38,
	OFFSET_DEBUG_STA = 0x50,
	OFFSET_TX_4G_MODE = 0x54,
	OFFSET_RX_4G_MODE = 0x58,
	OFFSET_SEC_EN = 0x7c,
};

struct mtk_i3c_controller_info {
	int id;
	struct i3c_master_controller *base;
	struct list_head list;
};

struct mtk_i3c_compatible {
	const u16 *regs;
	bool hdr_exit_support;
	unsigned char max_dma_support;
};

static const struct mtk_i3c_compatible mt6989_compat = {
	.regs = mt_i3c_regs_v2,
	.hdr_exit_support = false,
	.max_dma_support = 36,
};

static const struct mtk_i3c_compatible mt6991_compat = {
	.regs = mt_i3c_regs_v2,
	.hdr_exit_support = true,
	.max_dma_support = 36,
};

enum mtk_i3c_master_state {
	MTK_I3C_MASTER_IDLE,
	MTK_I3C_MASTER_START,
};

enum mtk_i3c_trans_op {
	MTK_I3C_MASTER_NONE,
	MTK_I3C_MASTER_WR,
	MTK_I3C_MASTER_RD,
	MTK_I3C_MASTER_WRRD,
	MTK_I3C_MASTER_CON_WR,
};

enum mtk_i3c_mode {
	MTK_I3C_SDR_MODE,
	MTK_I3C_CCC_MODE,
	MTK_I3C_I2C_MODE,
	MTK_I3C_HDR_DDR_MODE,
	MTK_I3C_HDR_TSP_MODE,
	MTK_I3C_HDR_TSL_MODE,
};

struct mtk_dma_ptr {
	u8 *dma_rd_buf;
	u8 *dma_wr_buf;
	dma_addr_t rpaddr;
	dma_addr_t wpaddr;
};

struct mtk_i3c_cal_para {
	u32 max_step;
	u32 best_mul;
	u32 h_sample_cnt;
	u32 h_step_cnt;
	u32 l_sample_cnt;
	u32 l_step_cnt;
	u32 exp_duty;
	u32 exp_duty_diff;
	u32 force_h_time;
	u32 src_clk;
};

struct mtk_i3c_speed_reg {
	u32 speed_hz;
	u16 high_speed_reg;
	u16 timing_reg;
	u16 ltiming_reg;
	u16 clock_div_reg;
	u16 io_config_reg;
	u16 ext_conf_reg;
};

//#define MTK_I3C_RECORD_DMA_REG
#ifdef MTK_I3C_RECORD_DMA_REG
struct mtk_i3c_dma_info {
	u32 int_flag;
	u32 int_en;
	u32 en;
	u32 rst;
	u32 stop;
	u32 flush;
	u32 con;
	u32 tx_mem_addr;
	u32 rx_mem_addr;
	u32 tx_len;
	u32 rx_len;
	u32 int_buf_size;
	u32 debug_sta;
	u32 tx_mem_addr2;
	u32 rx_mem_addr2;
};
#endif

struct mtk_i3c_xfer {
	u16 addr;
	u8 ccc_id;
	u32 tx_len;
	const void *tx_buf;
	void *con_tx_buf;
	u32 rx_len;
	void *rx_buf;
	bool dma_en;
	enum i3c_error_code error;
	enum mtk_i3c_mode mode;
	enum mtk_i3c_trans_op op;
	struct mtk_dma_ptr pdma;
	int nxfers;
	int left_num;
};

struct mtk_i3c_master {
	struct i3c_master_controller base;
	struct device *dev;
	void __iomem *i3cbase;		/* i3c base addr */
	void __iomem *pdmabase;		/* dma base address*/
	struct clk *clk_main;		/* main clock for i3c bus */
	struct clk *clk_dma;		/* DMA clock for i3c via DMA */
	int irq;
	u32 irq_stat;			/* interrupt status */
	u32 clk_src_div; /* not use */
	u32 ls_force_h_time_ns;
	u32 hs_force_h_time_ns;
	u32 head_speed_hz;
	u32 b_ccc_speed_hz;
	u32 clk_src_in_hz; /* used for other domain i3c clock */
	u32 ch_offset_i3c;
	u32 ch_offset_dma;
	u32 scl_gpio_id;
	u32 sda_gpio_id;
	bool suspended;
	bool fifo_use_pulling;
	bool priv_xfer_wo7e;
	bool no_hdr_exit;
	struct mtk_i3c_speed_reg i2c_speed;
	struct mtk_i3c_speed_reg b_ccc_speed;
	struct mtk_i3c_speed_reg with7e_speed;
	struct mtk_i3c_speed_reg without7e_speed;
	const struct mtk_i3c_compatible *dev_comp;
	struct mtk_i3c_xfer xfer;
	struct rt_mutex bus_lock;
	struct completion msg_complete;
	int timeout; /* in jiffies */
	u8 entdaa_last_addr;
	u32 entdaa_count;
	u8 entdaa_addr[MTK_I3C_MAX_DEVS];
	enum mtk_i3c_master_state state;
	u32 free_pos;
	u8 addrs[MTK_I3C_MAX_DEVS];
	struct mtk_i3c_controller_info s_controller_info;
#ifdef MTK_I3C_RECORD_DMA_REG
	struct mtk_i3c_dma_info dma_reg;
#endif
};

struct mtk_i3c_i2c_dev_data {
	u8 index;
};

LIST_HEAD(g_mtk_i3c_list);

#define I3C_DUMP_BUF(p, l, fmt, ...)						\
	do {	\
		int cnt_ = 0;	\
		int len_ = (l <= MTK_I3C_MAX_DEVS ? l : MTK_I3C_MAX_DEVS);	\
		uint8_t raw_buf[MTK_I3C_MAX_DEVS * 3 + 8];	\
		const unsigned char *ptr = p;	\
		for (cnt_ = 0; cnt_ < len_; ++cnt_)	\
			(void)snprintf(raw_buf+3*cnt_, 4, "%02x_", ptr[cnt_]);	\
		raw_buf[3*cnt_] = '\0';	\
		if (l <= MTK_I3C_MAX_DEVS) {	\
			pr_info("[I3C]: "fmt"[%d] %s\n", ##__VA_ARGS__, l, raw_buf);	\
		} else {	\
			pr_info("[I3C]: "fmt"[%d](partial) %s\n", ##__VA_ARGS__, l, raw_buf);	\
		}	\
	} while (0)


static inline struct mtk_i3c_master *
to_mtk_i3c_master(struct i3c_master_controller *master)
{
	return container_of(master, struct mtk_i3c_master, base);
}

static u8 mtk_i3c_readb(struct mtk_i3c_master *i3c, enum I3C_REGS_OFFSET reg)
{
	return readb(i3c->i3cbase + i3c->ch_offset_i3c + i3c->dev_comp->regs[reg]);
}

static void mtk_i3c_writeb(struct mtk_i3c_master *i3c, u8 val,
			   enum I3C_REGS_OFFSET reg)
{
	writeb(val, i3c->i3cbase + i3c->ch_offset_i3c + i3c->dev_comp->regs[reg]);
}

static u32 mtk_i3c_readl(struct mtk_i3c_master *i3c, enum I3C_REGS_OFFSET reg)
{
	return readl(i3c->i3cbase + i3c->ch_offset_i3c + i3c->dev_comp->regs[reg]);
}

static void mtk_i3c_writel(struct mtk_i3c_master *i3c, u32 val,
			   enum I3C_REGS_OFFSET reg)
{
	writel(val, i3c->i3cbase + i3c->ch_offset_i3c + i3c->dev_comp->regs[reg]);
}

static int mtk_i3c_clock_enable(struct mtk_i3c_master *i3c)
{
	int ret;

	ret = clk_prepare_enable(i3c->clk_dma);
	if (ret)
		return ret;

	ret = clk_prepare_enable(i3c->clk_main);
	if (ret)
		goto err_main;

	return 0;

err_main:
	clk_disable_unprepare(i3c->clk_dma);

	return ret;
}

static void mtk_i3c_clock_disable(struct mtk_i3c_master *i3c)
{
	clk_disable_unprepare(i3c->clk_main);
	clk_disable_unprepare(i3c->clk_dma);
}

static void mtk_i3c_lock_bus(struct mtk_i3c_master *i3c)
{
	rt_mutex_lock(&i3c->bus_lock);
}

static void mtk_i3c_unlock_bus(struct mtk_i3c_master *i3c)
{
	rt_mutex_unlock(&i3c->bus_lock);
}

static inline int mtk_i3c_check_suspended(struct mtk_i3c_master *i3c)
{
	if (i3c->suspended) {
		dev_info(i3c->dev, "[%s] Transfer while suspended\n", __func__);
		return -ESHUTDOWN;
	}
	return 0;
}

static inline void mtk_i3c_mark_suspended(struct mtk_i3c_master *i3c)
{
	mtk_i3c_lock_bus(i3c);
	i3c->suspended = true;
	mtk_i3c_unlock_bus(i3c);
}

static inline void mtk_i3c_mark_resumed(struct mtk_i3c_master *i3c)
{
	mtk_i3c_lock_bus(i3c);
	i3c->suspended = false;
	mtk_i3c_unlock_bus(i3c);
}

static int mtk_i3c_master_get_free_pos(struct mtk_i3c_master *i3c)
{
	if (!(i3c->free_pos & GENMASK(MTK_I3C_MAX_DEVS - 1, 0)))
		return -ENOSPC;
	return ffs(i3c->free_pos) - 1;
}

#ifdef MTK_I3C_RECORD_DMA_REG
static void mtk_i3c_record_dma_info(struct mtk_i3c_master *i3c)
{
	i3c->dma_reg.int_flag = readl(i3c->pdmabase + OFFSET_INT_FLAG);
	i3c->dma_reg.int_en = readl(i3c->pdmabase + OFFSET_INT_EN);
	i3c->dma_reg.en = readl(i3c->pdmabase + OFFSET_EN);
	i3c->dma_reg.rst = readl(i3c->pdmabase + OFFSET_RST);
	i3c->dma_reg.stop = readl(i3c->pdmabase + OFFSET_STOP);
	i3c->dma_reg.flush = readl(i3c->pdmabase + OFFSET_FLUSH);
	i3c->dma_reg.con = readl(i3c->pdmabase + OFFSET_CON);
	i3c->dma_reg.tx_mem_addr = readl(i3c->pdmabase + OFFSET_TX_MEM_ADDR);
	i3c->dma_reg.rx_mem_addr = readl(i3c->pdmabase + OFFSET_RX_MEM_ADDR);
	i3c->dma_reg.tx_len = readl(i3c->pdmabase + OFFSET_TX_LEN);
	i3c->dma_reg.rx_len = readl(i3c->pdmabase + OFFSET_RX_LEN);
	i3c->dma_reg.int_buf_size = readl(i3c->pdmabase + OFFSET_INT_BUF_SIZE);
	i3c->dma_reg.debug_sta = readl(i3c->pdmabase + OFFSET_DEBUG_STA);
	i3c->dma_reg.tx_mem_addr2 = readl(i3c->pdmabase + OFFSET_TX_4G_MODE);
	i3c->dma_reg.rx_mem_addr2 = readl(i3c->pdmabase + OFFSET_RX_4G_MODE);
}
#endif

extern void gpio_dump_regs_range(int start, int end);
static void mtk_i3c_gpio_dump(struct mtk_i3c_master *i3c)
{
	gpio_dump_regs_range(i3c->scl_gpio_id, i3c->scl_gpio_id);
	gpio_dump_regs_range(i3c->sda_gpio_id, i3c->sda_gpio_id);
}

static void mtk_i3c_dump_reg(struct mtk_i3c_master *i3c)
{
	dev_info(i3c->dev, "irq=%d,irq_stat=0x%x,addr=0x%x,ccc=0x%x,dma=%d\n"
		"mode=%d,op=%d,mstat=%d\n",
		i3c->irq, i3c->irq_stat, i3c->xfer.addr, i3c->xfer.ccc_id,
		i3c->xfer.dma_en, i3c->xfer.mode, i3c->xfer.op, i3c->state);
	dev_info(i3c->dev, "SADDR=0x%x,IR_MASK=0x%x,IR_ST=0x%x,CTL=0x%x,\n"
		"TRSL=0x%x,TSACL=0x%x,HT=0x%x,START=0x%x,EXT=0x%x,LT=0x%x,\n"
		"HS=0x%x,IOCFG=0x%x,MCUINT=0x%x,TRLAUX=0x%x,CLK_DIV=0x%x\n"
		"TRAFFIC=0x%x,CMD=0x%x,CRC=0x%x,TERNARY=0x%x,IBITIME=0x%x\n"
		"DEFDA=0x%x,SHAPE=0x%x,ERROR=0x%x,DBGSTA=0x%x,DBGCTL=0x%x\n"
		"DMAFSM=0x%x,MAS=0x%x,FSTA=0x%x,FTRSH=0x%x,HFSTA=0x%x,M=0x%x\n",
		(mtk_i3c_readl(i3c, OFFSET_SLAVE_ADDR)),
		(mtk_i3c_readl(i3c, OFFSET_INTR_MASK)),
		(mtk_i3c_readl(i3c, OFFSET_INTR_STAT)),
		(mtk_i3c_readl(i3c, OFFSET_CONTROL)),
		(mtk_i3c_readl(i3c, OFFSET_TRANSFER_LEN)),
		(mtk_i3c_readl(i3c, OFFSET_TRANSAC_LEN)),
		(mtk_i3c_readl(i3c, OFFSET_TIMING)),
		(mtk_i3c_readl(i3c, OFFSET_START)),
		(mtk_i3c_readl(i3c, OFFSET_EXT_CONF)),
		(mtk_i3c_readl(i3c, OFFSET_LTIMING)),
		(mtk_i3c_readl(i3c, OFFSET_HS)),
		(mtk_i3c_readl(i3c, OFFSET_IO_CONFIG)),
		(mtk_i3c_readl(i3c, OFFSET_MCU_INTR)),
		(mtk_i3c_readl(i3c, OFFSET_TRANSFER_LEN_AUX)),
		(mtk_i3c_readl(i3c, OFFSET_CLOCK_DIV)),
		(mtk_i3c_readl(i3c, OFFSET_TRAFFIC)),
		(mtk_i3c_readl(i3c, OFFSET_COMMAND)),
		(mtk_i3c_readl(i3c, OFFSET_CRC_CODE)),
		(mtk_i3c_readl(i3c, OFFSET_TERNARY)),
		(mtk_i3c_readl(i3c, OFFSET_IBI_TIMING)),
		(mtk_i3c_readl(i3c, OFFSET_DEF_DA)),
		(mtk_i3c_readl(i3c, OFFSET_SHAPE)),
		(mtk_i3c_readl(i3c, OFFSET_ERROR)),
		(mtk_i3c_readl(i3c, OFFSET_DEBUGSTAT)),
		(mtk_i3c_readl(i3c, OFFSET_DEBUGCTRL)),
		(mtk_i3c_readl(i3c, OFFSET_DMA_FSM_DEBUG)),
		(mtk_i3c_readl(i3c, OFFSET_MULTIMAS)),
		(mtk_i3c_readl(i3c, OFFSET_FIFO_STAT)),
		(mtk_i3c_readl(i3c, OFFSET_FIFO_THRESH)),
		(mtk_i3c_readl(i3c, OFFSET_HFIFO_STAT)),
		(mtk_i3c_readl(i3c, OFFSET_MULTI_DMA)));
	if (i3c->xfer.dma_en) {
#ifdef MTK_I3C_RECORD_DMA_REG
		dev_info(i3c->dev, "before DMA:flag=0x%x,inten=0x%x,en=0x%x\n"
			"rst=0x%x,stop=0x%x,flush=0x%x,con=0x%x,txaddr=0x%x\n"
			"rxaddr=0x%x,txlen=0x%x,rxlen=0x%x,ibufsize=0x%x\n"
			"dbgsta=0x%x,txaddr2=0x%x,rxaddr2=0x%x\n",
			i3c->dma_reg.int_flag,
			i3c->dma_reg.int_en,
			i3c->dma_reg.en,
			i3c->dma_reg.rst,
			i3c->dma_reg.stop,
			i3c->dma_reg.flush,
			i3c->dma_reg.con,
			i3c->dma_reg.tx_mem_addr,
			i3c->dma_reg.rx_mem_addr,
			i3c->dma_reg.tx_len,
			i3c->dma_reg.rx_len,
			i3c->dma_reg.int_buf_size,
			i3c->dma_reg.debug_sta,
			i3c->dma_reg.tx_mem_addr2,
			i3c->dma_reg.rx_mem_addr2);
#endif
		dev_info(i3c->dev, "after DMA:flag=0x%x,inten=0x%x,en=0x%x\n"
			"rst=0x%x,stop=0x%x,flush=0x%x,con=0x%x,txaddr=0x%x\n"
			"rxaddr=0x%x,txlen=0x%x,rxlen=0x%x,ibufsize=0x%x\n"
			"dbgsta=0x%x,txaddr2=0x%x,rxaddr2=0x%x\n",
			readl(i3c->pdmabase + OFFSET_INT_FLAG),
			readl(i3c->pdmabase + OFFSET_INT_EN),
			readl(i3c->pdmabase + OFFSET_EN),
			readl(i3c->pdmabase + OFFSET_RST),
			readl(i3c->pdmabase + OFFSET_STOP),
			readl(i3c->pdmabase + OFFSET_FLUSH),
			readl(i3c->pdmabase + OFFSET_CON),
			readl(i3c->pdmabase + OFFSET_TX_MEM_ADDR),
			readl(i3c->pdmabase + OFFSET_RX_MEM_ADDR),
			readl(i3c->pdmabase + OFFSET_TX_LEN),
			readl(i3c->pdmabase + OFFSET_RX_LEN),
			readl(i3c->pdmabase + OFFSET_INT_BUF_SIZE),
			readl(i3c->pdmabase + OFFSET_DEBUG_STA),
			readl(i3c->pdmabase + OFFSET_TX_4G_MODE),
			readl(i3c->pdmabase + OFFSET_RX_4G_MODE));
	}
	mtk_i3c_gpio_dump(i3c);
}

static enum i3c_addr_slot_status
i3c_bus_get_addr_slot_status(struct i3c_bus *bus, u16 addr)
{
	unsigned long status;
	int bitpos = addr * 2;

	if (addr > I2C_MAX_ADDR)
		return I3C_ADDR_SLOT_RSVD;

	status = bus->addrslots[bitpos / BITS_PER_LONG];
	status >>= bitpos % BITS_PER_LONG;

	return status & I3C_ADDR_SLOT_STATUS_MASK;
}

static void i3c_bus_set_addr_slot_status(struct i3c_bus *bus, u16 addr,
					 enum i3c_addr_slot_status status)
{
	int bitpos = addr * 2;
	unsigned long *ptr;

	if (addr > I2C_MAX_ADDR)
		return;

	ptr = bus->addrslots + (bitpos / BITS_PER_LONG);
	*ptr &= ~((unsigned long)I3C_ADDR_SLOT_STATUS_MASK <<
						(bitpos % BITS_PER_LONG));
	*ptr |= (unsigned long)status << (bitpos % BITS_PER_LONG);
}

static void *i3c_ccc_cmd_dest_init(struct i3c_ccc_cmd_dest *dest, u8 addr,
		u16 payloadlen)
{
	dest->addr = addr;
	dest->payload.len = payloadlen;
	if (payloadlen)
		dest->payload.data = kzalloc(payloadlen, GFP_KERNEL);
	else
		dest->payload.data = NULL;

	return dest->payload.data;
}

static void i3c_ccc_cmd_dest_cleanup(struct i3c_ccc_cmd_dest *dest)
{
	kfree(dest->payload.data);
}

static void i3c_ccc_cmd_init(struct i3c_ccc_cmd *cmd, bool rnw, u8 id,
		struct i3c_ccc_cmd_dest *dests, unsigned int ndests)
{
	cmd->rnw = rnw ? 1 : 0;
	cmd->id = id;
	cmd->dests = dests;
	cmd->ndests = ndests;
	cmd->err = I3C_ERROR_UNKNOWN;
}

static int i3c_master_send_ccc_cmd_locked(struct i3c_master_controller *master,
		struct i3c_ccc_cmd *cmd)
{
	int ret;

	if (!cmd || !master)
		return -EINVAL;

	if (WARN_ON(master->init_done &&
		    !rwsem_is_locked(&master->bus.lock)))
		return -EINVAL;

	if (!master->ops->send_ccc_cmd)
		return -EINVAL;

	if ((cmd->id & I3C_CCC_DIRECT) && (!cmd->dests || !cmd->ndests))
		return -EINVAL;

	if (master->ops->supports_ccc_cmd &&
	    !master->ops->supports_ccc_cmd(master, cmd))
		return -EINVAL;

	ret = master->ops->send_ccc_cmd(master, cmd);
	if (ret) {
		if (cmd->err != I3C_ERROR_UNKNOWN)
			return cmd->err;

		return ret;
	}

	return 0;
}

int mtk_i3c_master_entasx(struct i3c_master_controller *master,
		u8 addr, u8 entas_mode)
{
#ifdef ASSIGNED_ADDR_I3C_DEV
	enum i3c_addr_slot_status addrstat;
#endif
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master)
		return -EINVAL;

#ifdef ASSIGNED_ADDR_I3C_DEV
	addrstat = i3c_bus_get_addr_slot_status(&master->bus, addr);
	if (addr != I3C_BROADCAST_ADDR && addrstat != I3C_ADDR_SLOT_I3C_DEV)
		return -EINVAL;
#endif
	if (entas_mode > 3)
		return -EINVAL;

	down_read(&master->bus.lock);
	i3c_ccc_cmd_dest_init(&dest, addr, 0);
	i3c_ccc_cmd_init(&cmd, false,
		I3C_CCC_ENTAS(entas_mode, (addr == I3C_BROADCAST_ADDR)), &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_read(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_entasx);

int mtk_i3c_master_rstdaa(struct i3c_master_controller *master,
		u8 addr)
{
#ifdef ASSIGNED_ADDR_I3C_DEV
	enum i3c_addr_slot_status addrstat;
#endif
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master)
		return -EINVAL;

#ifdef ASSIGNED_ADDR_I3C_DEV
	addrstat = i3c_bus_get_addr_slot_status(&master->bus, addr);
	if (addr != I3C_BROADCAST_ADDR && addrstat != I3C_ADDR_SLOT_I3C_DEV)
		return -EINVAL;
#endif

	down_write(&master->bus.lock);
	i3c_ccc_cmd_dest_init(&dest, addr, 0);
	i3c_ccc_cmd_init(&cmd, false,
		I3C_CCC_RSTDAA(addr == I3C_BROADCAST_ADDR), &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_rstdaa);

int mtk_i3c_master_setmwl(struct i3c_master_controller *master,
		struct i3c_device_info *info, u8 addr, struct i3c_ccc_mwl *mwl)
{
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || ! info || !mwl)
		return -EINVAL;

	dest.addr = addr;
	dest.payload.len = sizeof(*mwl);
	dest.payload.data = mwl;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, false,
		I3C_CCC_SETMWL(addr == I3C_BROADCAST_ADDR), &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	info->max_write_len = be16_to_cpu(mwl->len);

out:
	up_write(&master->bus.lock);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_setmwl);

int mtk_i3c_master_setmrl(struct i3c_master_controller *master,
		struct i3c_device_info *info, u8 addr, struct i3c_ccc_mrl *mrl)
{
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info || !mrl)
		return -EINVAL;

	dest.addr = addr;
	dest.payload.len = sizeof(*mrl);
	dest.payload.data = mrl;
	/*
	 * When the device does not have IBI payload GETMRL only returns 2
	 * bytes of data.
	 */
	if (!(info->bcr & I3C_BCR_IBI_PAYLOAD) || (addr == I3C_BROADCAST_ADDR))
		dest.payload.len -= 1;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, false,
		I3C_CCC_SETMRL(addr == I3C_BROADCAST_ADDR), &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	switch (dest.payload.len) {
	case 3:
		info->max_ibi_len = mrl->ibi_len;
		fallthrough;
	case 2:
		info->max_read_len = be16_to_cpu(mrl->read_len);
		break;
	default:
		ret = -EIO;
		goto out;
	}

out:
	up_write(&master->bus.lock);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_setmrl);

int mtk_i3c_master_enttm(struct i3c_master_controller *master,
				struct i3c_ccc_enttm *enttm)
{
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !enttm)
		return -EINVAL;

	down_read(&master->bus.lock);
	dest.addr = I3C_BROADCAST_ADDR;
	dest.payload.len = sizeof(*enttm);
	dest.payload.data = enttm;

	i3c_ccc_cmd_init(&cmd, false, I3C_CCC_ENTTM, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	up_read(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_enttm);

static struct i3c_dev_desc *i3c_master_search_dev
	(struct i3c_master_controller *master, struct i3c_device_info *info)
{
	struct i3c_dev_desc *i3cdev = NULL;

	i3c_bus_for_each_i3cdev(&master->bus, i3cdev) {
		if (&i3cdev->info == info)
			return i3cdev;
	}
	return NULL;
}

static int mtk_i3c_master_setda(struct i3c_master_controller *master,
				   u8 oldaddr, u8 newaddr, bool setdasa)
{
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_setda *setda;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!oldaddr || !newaddr)
		return -EINVAL;

	setda = i3c_ccc_cmd_dest_init(&dest, oldaddr, sizeof(*setda));
	if (!setda)
		return -ENOMEM;

	down_write(&master->bus.lock);
	setda->addr = newaddr << 1;
	i3c_ccc_cmd_init(&cmd, false,
			 setdasa ? I3C_CCC_SETDASA : I3C_CCC_SETNEWDA,
			 &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}

/* only support addr was auto clear when slave reset */
int mtk_i3c_master_setdasa(struct i3c_master_controller *master,
		struct i3c_device_info *info, u8 static_addr, u8 dyn_addr)
{
	int ret = 0;
	struct i3c_dev_desc *i3cdev = NULL;
	struct mtk_i3c_master *i3c = NULL;

	if (!master || !info)
		return -EINVAL;

	i3c = to_mtk_i3c_master(master);
	if (info->dyn_addr != dyn_addr) {
		dev_info(i3c->dev,
			"[%s] error,saddr=0x%x,daddr=0x%x,isaddr=0x%x,idaddr=0x%x\n",
			__func__, static_addr, dyn_addr, info->static_addr,
			info->dyn_addr);
		return -EINVAL;
	}
	i3cdev = i3c_master_search_dev(master, info);
	if (!i3cdev) {
		dev_info(i3c->dev,
			"[%s] i3cdev,saddr=0x%x,daddr=0x%x,isaddr=0x%x,idaddr=0x%x\n",
			__func__, static_addr, dyn_addr, info->static_addr,
			info->dyn_addr);
		ret = -EINVAL;
		goto out;
	}

	ret = mtk_i3c_master_setda(master, static_addr, dyn_addr, true);
	if (ret)
		dev_info(i3c->dev,
			"[%s] setdasa err,saddr=0x%x,daddr=0x%x,isaddr=0x%x,idaddr=0x%x\n",
			__func__, static_addr, dyn_addr, info->static_addr,
			info->dyn_addr);

out:
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_setdasa);

int mtk_i3c_master_setnewda(struct i3c_master_controller *master,
		struct i3c_device_info *info, u8 oldaddr, u8 newaddr)
{
	int ret = 0;
	struct i3c_dev_desc *i3cdev = NULL;
	struct mtk_i3c_master *i3c = NULL;
	enum i3c_addr_slot_status status;

	if (!master || !info)
		return -EINVAL;
	i3c = to_mtk_i3c_master(master);
	if (oldaddr == newaddr) {
		dev_info(i3c->dev, "[%s] addr is same,oldaddr=0x%x,newaddr=0x%x,dynaddr=0x%x\n",
			__func__, oldaddr, newaddr, info->dyn_addr);
		return -EINVAL;
	}

	i3cdev = i3c_master_search_dev(master, info);
	if (!i3cdev) {
		dev_info(i3c->dev, "[%s] not find dev,oldaddr=0x%x,newaddr=0x%x,dynaddr=0x%x\n",
			__func__, oldaddr, newaddr, info->dyn_addr);
		return -EINVAL;
	}

	ret = mtk_i3c_master_setda(master, oldaddr, newaddr, false);
	if (ret) {
		dev_info(i3c->dev, "[%s] setnewda fail,oldaddr=0x%x,newaddr=0x%x,dynaddr=0x%x\n",
			__func__, oldaddr, newaddr, info->dyn_addr);
		goto out;
	}

	info->dyn_addr = newaddr;
	if (!i3cdev->boardinfo ||
		(i3cdev->info.dyn_addr != i3cdev->boardinfo->init_dyn_addr)) {
		status = i3c_bus_get_addr_slot_status(&master->bus, newaddr);
		if (status != I3C_ADDR_SLOT_FREE) {
			dev_info(i3c->dev, "[%s] status=%d,oldaddr=0x%x,newaddr=0x%x,dynaddr=0x%x\n",
				__func__, status, oldaddr, newaddr, info->dyn_addr);
			ret = -EBUSY;
			goto out;
		}
		i3c_bus_set_addr_slot_status(&master->bus, newaddr, I3C_ADDR_SLOT_I3C_DEV);
	}
	if (master->ops->reattach_i3c_dev) {
		ret = master->ops->reattach_i3c_dev(i3cdev, oldaddr);
		if (ret) {
			if (i3cdev->info.static_addr)
				i3c_bus_set_addr_slot_status(&master->bus,
					i3cdev->info.static_addr, I3C_ADDR_SLOT_FREE);
			if (i3cdev->info.dyn_addr)
				i3c_bus_set_addr_slot_status(&master->bus, i3cdev->info.dyn_addr,
					I3C_ADDR_SLOT_FREE);
			if (i3cdev->boardinfo && i3cdev->boardinfo->init_dyn_addr)
				i3c_bus_set_addr_slot_status(&master->bus, i3cdev->info.dyn_addr,
						I3C_ADDR_SLOT_FREE);
			goto out;
		}
	}

out:
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_setnewda);

int mtk_i3c_master_getmwl(struct i3c_master_controller *master,
				    struct i3c_device_info *info)
{
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_mwl *mwl;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info)
		return -EINVAL;
	mwl = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr, sizeof(*mwl));
	if (!mwl)
		return -ENOMEM;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETMWL, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	if (dest.payload.len != sizeof(*mwl)) {
		ret = -EIO;
		goto out;
	}

	info->max_write_len = be16_to_cpu(mwl->len);

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_getmwl);

int mtk_i3c_master_getmrl(struct i3c_master_controller *master,
				    struct i3c_device_info *info)
{
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_mrl *mrl;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info)
		return -EINVAL;
	mrl = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr, sizeof(*mrl));
	if (!mrl)
		return -ENOMEM;

	down_write(&master->bus.lock);
	/*
	 * When the device does not have IBI payload GETMRL only returns 2
	 * bytes of data.
	 */
	if (!(info->bcr & I3C_BCR_IBI_PAYLOAD))
		dest.payload.len -= 1;

	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETMRL, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	switch (dest.payload.len) {
	case 3:
		info->max_ibi_len = mrl->ibi_len;
		fallthrough;
	case 2:
		info->max_read_len = be16_to_cpu(mrl->read_len);
		break;
	default:
		ret = -EIO;
		goto out;
	}

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_getmrl);

int mtk_i3c_master_getpid(struct i3c_master_controller *master,
				    struct i3c_device_info *info)
{
	struct i3c_ccc_getpid *getpid;
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret, i;

	if (!master || !info)
		return -EINVAL;
	getpid = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr, sizeof(*getpid));
	if (!getpid)
		return -ENOMEM;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETPID, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	info->pid = 0;
	for (i = 0; i < sizeof(getpid->pid); i++) {
		int sft = (sizeof(getpid->pid) - i - 1) * 8;

		info->pid |= (u64)getpid->pid[i] << sft;
	}

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_getpid);

int mtk_i3c_master_getbcr(struct i3c_master_controller *master,
				    struct i3c_device_info *info)
{
	struct i3c_ccc_getbcr *getbcr;
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info)
		return -EINVAL;
	getbcr = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr, sizeof(*getbcr));
	if (!getbcr)
		return -ENOMEM;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETBCR, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	info->bcr = getbcr->bcr;

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_getbcr);

int mtk_i3c_master_getdcr(struct i3c_master_controller *master,
				    struct i3c_device_info *info)
{
	struct i3c_ccc_getdcr *getdcr;
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info)
		return -EINVAL;
	getdcr = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr, sizeof(*getdcr));
	if (!getdcr)
		return -ENOMEM;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETDCR, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	info->dcr = getdcr->dcr;

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_getdcr);

int mtk_i3c_master_getmxds(struct i3c_master_controller *master,
				     struct i3c_device_info *info)
{
	struct i3c_ccc_getmxds *getmaxds;
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info)
		return -EINVAL;
	getmaxds = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr,
					 sizeof(*getmaxds));
	if (!getmaxds)
		return -ENOMEM;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETMXDS, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	if (dest.payload.len != 2 && dest.payload.len != 5) {
		ret = -EIO;
		goto out;
	}

	info->max_read_ds = getmaxds->maxrd;
	info->max_write_ds = getmaxds->maxwr;
	if (dest.payload.len == 5)
		info->max_read_turnaround = getmaxds->maxrdturn[0] |
					    ((u32)getmaxds->maxrdturn[1] << 8) |
					    ((u32)getmaxds->maxrdturn[2] << 16);

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_getmxds);

int mtk_i3c_master_gethdrcap(struct i3c_master_controller *master,
				       struct i3c_device_info *info)
{
	struct i3c_ccc_gethdrcap *gethdrcap;
	struct i3c_ccc_cmd_dest dest;
	struct i3c_ccc_cmd cmd;
	int ret;

	if (!master || !info)
		return -EINVAL;
	gethdrcap = i3c_ccc_cmd_dest_init(&dest, info->dyn_addr,
					  sizeof(*gethdrcap));
	if (!gethdrcap)
		return -ENOMEM;

	down_write(&master->bus.lock);
	i3c_ccc_cmd_init(&cmd, true, I3C_CCC_GETHDRCAP, &dest, 1);
	ret = i3c_master_send_ccc_cmd_locked(master, &cmd);
	if (ret)
		goto out;

	if (dest.payload.len != 1) {
		ret = -EIO;
		goto out;
	}

	info->hdr_cap = gethdrcap->modes;

out:
	i3c_ccc_cmd_dest_cleanup(&dest);
	up_write(&master->bus.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_gethdrcap);

static void mtk_i3c_init_hw(struct mtk_i3c_master *i3c)
{
	u32 reg;

	mtk_i3c_writel(i3c, 0, OFFSET_INTR_MASK);
	i3c->state = MTK_I3C_MASTER_IDLE;
	reg = mtk_i3c_readl(i3c, OFFSET_INTR_STAT);
	mtk_i3c_writel(i3c, reg, OFFSET_INTR_STAT);

	mtk_i3c_writel(i3c, I3C_RST_FSM, OFFSET_SOFTRESET);
	udelay(2);
	writel(I3C_DMA_RST_HARD, i3c->pdmabase + OFFSET_RST);
	mtk_i3c_writel(i3c, I3C_RST_SOFT, OFFSET_SOFTRESET);
	udelay(2);
	writel(I3C_DMA_RST_WARM, i3c->pdmabase + OFFSET_RST);
	udelay(2);
	writel(I3C_DMA_RST_CLR, i3c->pdmabase + OFFSET_RST);
	udelay(2);
	writel(I3C_DMA_RST_HANDSHAKE | I3C_DMA_RST_HARD,
					i3c->pdmabase + OFFSET_RST);
	mtk_i3c_writel(i3c, I3C_RST_HANDSHAKE | I3C_RST_SOFT,
						OFFSET_SOFTRESET);
	udelay(2);
	writel(I3C_DMA_RST_CLR, i3c->pdmabase + OFFSET_RST);
	mtk_i3c_writel(i3c, I3C_RST_CLR, OFFSET_SOFTRESET);

	mtk_i3c_writel(i3c, mtk_i3c_readl(i3c, OFFSET_DEBUGCTRL) & (~I3C_DBGCRL_BUS_ERR),
		OFFSET_DEBUGCTRL);

	mtk_i3c_writel(i3c, I3C_IOCFG_PUSH_PULL, OFFSET_IO_CONFIG);
	mtk_i3c_writel(i3c, I3C_DELAY_LEN, OFFSET_DELAY_LEN);
}


static void mtk_i3c_write_speed_reg(struct mtk_i3c_master *i3c, struct mtk_i3c_speed_reg *s_reg)
{
	//if (mtk_i3c_readl(i3c, OFFSET_LTIMING) == s_reg->ltiming_reg) {
	//	dev_dbg(i3c->dev, "[%s] no need write,%p,hs0x%x,ht0x%x,lt0x%x,div0x%x,io0x%x,ext0x%x\n",
	//		__func__, s_reg, s_reg->high_speed_reg, s_reg->timing_reg, s_reg->ltiming_reg,
	//		s_reg->clock_div_reg, s_reg->io_config_reg, s_reg->ext_conf_reg);
	//	return;
	//}
	mtk_i3c_writel(i3c, s_reg->high_speed_reg, OFFSET_HS);
	mtk_i3c_writel(i3c, s_reg->timing_reg, OFFSET_TIMING);
	mtk_i3c_writel(i3c, s_reg->ltiming_reg, OFFSET_LTIMING);
	mtk_i3c_writel(i3c, s_reg->clock_div_reg, OFFSET_CLOCK_DIV);
	mtk_i3c_writel(i3c, s_reg->io_config_reg, OFFSET_IO_CONFIG);
	mtk_i3c_writel(i3c, s_reg->ext_conf_reg, OFFSET_EXT_CONF);
	dev_dbg(i3c->dev, "[%s] %p,hs0x%x,ht0x%x,lt0x%x,div0x%x,io0x%x,ext0x%x\n",
		__func__, s_reg, s_reg->high_speed_reg, s_reg->timing_reg, s_reg->ltiming_reg,
		s_reg->clock_div_reg, s_reg->io_config_reg, s_reg->ext_conf_reg);
}

static void mtk_i3c_set_speed_reg(struct mtk_i3c_master *i3c, struct mtk_i3c_xfer *xfer)
{
	if ((xfer->mode == MTK_I3C_CCC_MODE) &&
		((xfer->ccc_id < I3C_CCC_DIRECT) || (xfer->ccc_id == I3C_CCC_SETDASA)))
		mtk_i3c_write_speed_reg(i3c, &(i3c->b_ccc_speed));
	else if ((xfer->mode == MTK_I3C_SDR_MODE) && (i3c->priv_xfer_wo7e))
		mtk_i3c_write_speed_reg(i3c, &(i3c->without7e_speed));
	else if (xfer->mode == MTK_I3C_I2C_MODE)
		mtk_i3c_write_speed_reg(i3c, &(i3c->i2c_speed));
	else
		mtk_i3c_write_speed_reg(i3c, &(i3c->with7e_speed));
}

static int mtk_i3c_calculate_mul(struct mtk_i3c_master *i3c,
	unsigned int max_step, unsigned int best_mul,
	unsigned int *sample_cnt, unsigned int *step_cnt)
{
	unsigned int sample = 1;
	unsigned int step = 1;
	int ret = 0;

	if (best_mul > max_step * I3C_MAX_SAMPLE_CNT_DIV)
		return -EINVAL;

	for (sample = 1; sample <= I3C_MAX_SAMPLE_CNT_DIV; sample++) {
		step = best_mul / sample;
		if (step < sample)
			break;
		if ((best_mul % sample) == 0) {
			if (step <= max_step) {
				*sample_cnt = sample;
				*step_cnt = step;
				goto exit;
			}
		}
	}
	ret = -EINVAL;
exit:
	return ret;
}

static int mtk_i3c_calculate_speed(struct mtk_i3c_master *i3c,
	struct mtk_i3c_cal_para *cal_para)
{
	unsigned int best_half_mul = (cal_para->best_mul + 1) / 2;
	unsigned int best_h_mul = 0;
	unsigned int best_l_mul = 0;
	unsigned int h_mul_add = 0;
	unsigned int h_mul_sub = 0;
	unsigned int l_mul_add = 0;
	unsigned int l_mul_sub = 0;
	unsigned int cyc = 0;
	int ret = 0;

	best_h_mul = cal_para->best_mul * cal_para->exp_duty / 100;
	best_l_mul = cal_para->best_mul - best_h_mul;
	for (cyc = 0; (best_h_mul + cyc) <= best_half_mul; cyc++) {
		h_mul_add = best_h_mul + cyc;
		if (best_l_mul <= cyc)
			goto next;
		l_mul_sub = best_l_mul - cyc;

		if ((h_mul_add < 2) || (l_mul_sub < 2) ||
			(h_mul_add * 1000 / cal_para->best_mul >=
			(cal_para->exp_duty * 10 + cal_para->exp_duty_diff)))
			goto next;

		ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, h_mul_add,
				&cal_para->h_sample_cnt, &cal_para->h_step_cnt);
		if (ret < 0)
			goto next;
		ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, l_mul_sub,
			&cal_para->l_sample_cnt, &cal_para->l_step_cnt);
		if (ret < 0)
			goto next;

		goto exit;
next:
		if (best_h_mul <= cyc)
			continue;
		h_mul_sub = best_h_mul - cyc;
		l_mul_add = best_l_mul + cyc;
		if ((h_mul_sub < 2) || (l_mul_add < 2))
			break;
		if (h_mul_sub * 1000 / cal_para->best_mul <
			(cal_para->exp_duty * 10 - cal_para->exp_duty_diff))
			continue;

		ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, h_mul_sub,
			&cal_para->h_sample_cnt, &cal_para->h_step_cnt);
		if (ret < 0)
			continue;
		ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, l_mul_add,
			&cal_para->l_sample_cnt, &cal_para->l_step_cnt);
		if (ret < 0)
			continue;
		goto exit;
	}
	ret = -EINVAL;
exit:
	return ret;
}

static int mtk_i3c_calculate_force_h_time(struct mtk_i3c_master *i3c,
	struct mtk_i3c_cal_para *cal_para)
{
	unsigned int best_h_mul = 0;
	unsigned int best_l_mul = 0;
	unsigned int l_mul_add = 0;
	unsigned int cyc = 0;
	int ret = 0;

	best_h_mul = (cal_para->src_clk / 10000 * cal_para->force_h_time
		/ 10000 + 5) / 10;
	best_l_mul = cal_para->best_mul - best_h_mul;
	if ((best_h_mul < 2) || (best_l_mul < 2))
		return -EINVAL;

	ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, best_h_mul,
			&cal_para->h_sample_cnt, &cal_para->h_step_cnt);
	if (ret < 0) {
		best_h_mul--;
		best_l_mul++;
		if (best_h_mul < 2)
			return -EINVAL;
		ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, best_h_mul,
			&cal_para->h_sample_cnt, &cal_para->h_step_cnt);
		if (ret < 0)
			return -EINVAL;
	}

	for (cyc = 0; (best_l_mul + cyc) <= cal_para->best_mul; cyc++) {
		l_mul_add = best_l_mul + cyc;
		ret = mtk_i3c_calculate_mul(i3c, cal_para->max_step, l_mul_add,
			&cal_para->l_sample_cnt, &cal_para->l_step_cnt);
		if (ret == 0)
			return 0;
	}
	return -EINVAL;
}

static int mtk_i3c_set_speed(struct mtk_i3c_master *i3c,
	struct mtk_i3c_speed_reg *cal_reg, unsigned int parent_clk, bool with_head)
{
	int ret = 0;
	unsigned int clk_div = 1;
	unsigned int h_ext_time = 0;
	unsigned int l_ext_time = 0;
	unsigned int target_speed = cal_reg->speed_hz;
	unsigned int head_speed = i3c->head_speed_hz;
	unsigned int max_clk_div = I3C_MAX_CLOCK_DIV_5BITS;
	struct mtk_i3c_cal_para h_cal_para;
	struct mtk_i3c_cal_para l_cal_para;

	if (target_speed < head_speed)
		head_speed = target_speed;

	for (clk_div = 1; clk_div <= max_clk_div; clk_div++) {
		if (with_head) {
			h_cal_para.max_step = I3C_MAX_HS_STEP_CNT_DIV;
			l_cal_para.max_step = I3C_MAX_LS_STEP_CNT_DIV;
			h_cal_para.force_h_time = i3c->hs_force_h_time_ns;
			l_cal_para.force_h_time = i3c->ls_force_h_time_ns;
			h_cal_para.src_clk = parent_clk / clk_div;
			l_cal_para.src_clk = parent_clk / clk_div;
			h_ext_time = (parent_clk / clk_div) * 20 /
				(target_speed / I2C_MAX_STANDARD_MODE_FREQ * 1000000);
			if (h_ext_time > I3C_MAX_HS_EXT_TIME)
				continue;
			l_ext_time = (parent_clk / clk_div) * 20 /
				(head_speed / I2C_MAX_STANDARD_MODE_FREQ * 1000000);
			if (l_ext_time > I3C_MAX_LS_EXT_TIME)
				continue;
			h_cal_para.best_mul = (parent_clk + clk_div * target_speed - 1) /
				(clk_div * target_speed);
			if (target_speed > I3C_BUS_I2C_FM_PLUS_SCL_RATE) {
				h_cal_para.exp_duty = I3C_HS_DUTY;
			} else {
				h_cal_para.exp_duty = I3C_LS_DUTY;
				h_cal_para.force_h_time = 0;
				l_cal_para.force_h_time = 0;
				h_cal_para.best_mul++;
			}
			h_cal_para.exp_duty_diff = I3C_DUTY_DIFF_TENTHS;
			if (h_cal_para.force_h_time)
				ret = mtk_i3c_calculate_force_h_time(i3c, &h_cal_para);
			else
				ret = mtk_i3c_calculate_speed(i3c, &h_cal_para);
			if (ret < 0)
				continue;

			l_cal_para.best_mul = (parent_clk + clk_div * head_speed - 1) /
				(clk_div * head_speed) + 1;
			l_cal_para.exp_duty = I3C_LS_DUTY;
			l_cal_para.exp_duty_diff = I3C_DUTY_DIFF_TENTHS;
			if (l_cal_para.force_h_time)
				ret = mtk_i3c_calculate_force_h_time(i3c, &l_cal_para);
			else
				ret = mtk_i3c_calculate_speed(i3c, &l_cal_para);
			if (ret < 0)
				continue;

			cal_reg->high_speed_reg = I3C_HS_SPEED | I3C_HS_NACK_DET |
				I3C_HS_EN | ((h_cal_para.h_sample_cnt - 1) << 12) |
				((h_cal_para.h_step_cnt - 1) << 8);
			cal_reg->timing_reg = ((l_cal_para.h_sample_cnt - 1) << 8) |
				(l_cal_para.h_step_cnt - 1);
			cal_reg->ltiming_reg = ((h_cal_para.l_sample_cnt - 1) << 12) |
				((h_cal_para.l_step_cnt - 1) << 9) |
				((l_cal_para.l_sample_cnt - 1) << 6) | (l_cal_para.l_step_cnt - 1);
			cal_reg->ext_conf_reg = (h_ext_time << 1) | (l_ext_time << 8) | (1 << 0);
			goto clk_div_exit;
		} else {
			l_cal_para.max_step = I3C_MAX_LS_STEP_CNT_DIV;
			l_cal_para.src_clk = parent_clk;
			l_ext_time = (parent_clk / clk_div) * 20 /
				(target_speed / I2C_MAX_STANDARD_MODE_FREQ * 1000000);
			if (l_ext_time > I3C_MAX_LS_EXT_TIME)
				continue;

			l_cal_para.best_mul = (parent_clk + clk_div * target_speed - 1) /
				(clk_div * target_speed);
			if (target_speed > I3C_BUS_I2C_FM_PLUS_SCL_RATE) {
				l_cal_para.exp_duty = I3C_HS_DUTY;
				l_cal_para.force_h_time = i3c->hs_force_h_time_ns;
			} else {
				l_cal_para.exp_duty = I3C_LS_DUTY;
				l_cal_para.force_h_time = 0;
				l_cal_para.best_mul++;
			}
			l_cal_para.exp_duty_diff = I3C_DUTY_DIFF_TENTHS;
			if (l_cal_para.force_h_time)
				ret = mtk_i3c_calculate_force_h_time(i3c, &l_cal_para);
			else
				ret = mtk_i3c_calculate_speed(i3c, &l_cal_para);
			if (ret < 0)
				continue;

			cal_reg->high_speed_reg = 0;
			cal_reg->timing_reg = ((l_cal_para.h_sample_cnt - 1) << 8) |
				(l_cal_para.h_step_cnt - 1);
			cal_reg->ltiming_reg = ((l_cal_para.l_sample_cnt - 1) << 6) |
				(l_cal_para.l_step_cnt - 1);
			cal_reg->ext_conf_reg = (l_ext_time << 8) | (1 << 0);
			if (target_speed > I3C_BUS_I2C_FM_PLUS_SCL_RATE) {
				h_cal_para.max_step = I3C_MAX_HS_STEP_CNT_DIV;
				h_cal_para.force_h_time = i3c->hs_force_h_time_ns;
				h_cal_para.src_clk = parent_clk / clk_div;
				h_ext_time = (parent_clk / clk_div) * 20 /
					(target_speed / I2C_MAX_STANDARD_MODE_FREQ * 1000000);
				if (h_ext_time > I3C_MAX_HS_EXT_TIME)
					continue;
				h_cal_para.best_mul = (parent_clk + clk_div * target_speed - 1) /
					(clk_div * target_speed);
				h_cal_para.exp_duty = I3C_HS_DUTY;
				h_cal_para.exp_duty_diff = I3C_DUTY_DIFF_TENTHS;
				if (h_cal_para.force_h_time)
					ret = mtk_i3c_calculate_force_h_time(i3c, &h_cal_para);
				else
					ret = mtk_i3c_calculate_speed(i3c, &h_cal_para);
				if (ret < 0)
					continue;

				cal_reg->high_speed_reg |= (I3C_HS_SPEED |
					((h_cal_para.h_sample_cnt - 1) << 12) |
					((h_cal_para.h_step_cnt - 1) << 8));
				cal_reg->ltiming_reg |= (((h_cal_para.l_sample_cnt - 1) << 12) |
					((h_cal_para.l_step_cnt - 1) << 9));
				cal_reg->ext_conf_reg |= (h_ext_time << 1);
			}
			goto clk_div_exit;
		}
	}

clk_div_exit:
	if (clk_div > max_clk_div)
		return -EINVAL;

	cal_reg->clock_div_reg = ((clk_div - 1) << 8) | (clk_div - 1);
	if (target_speed > I3C_BUS_I2C_FM_PLUS_SCL_RATE)
		cal_reg->io_config_reg = I3C_IOCFG_PUSH_PULL;
	else
		cal_reg->io_config_reg = I3C_IOCFG_OPEN_DRAIN;

	return 0;
}

static int mtk_i3c_init_speed(struct mtk_i3c_master *i3c, unsigned long parent_clk_lu)
{
	int ret = 0;
	unsigned int parent_clk = (unsigned int)parent_clk_lu;

	if (i3c->base.bus.scl_rate.i2c == 0)
		i3c->base.bus.scl_rate.i2c = I3C_BUS_I2C_FM_SCL_RATE;
	if (i3c->base.bus.scl_rate.i3c == 0)
		i3c->base.bus.scl_rate.i3c = I3C_BUS_I2C_FM_PLUS_SCL_RATE;

	i3c->i2c_speed.speed_hz = i3c->base.bus.scl_rate.i2c;
	ret = mtk_i3c_set_speed(i3c, &(i3c->i2c_speed), parent_clk, false);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s]ret=%d,scl_rate.i2c=%lu,clk=%u,cal_i2c_speed_fail\n",
			__func__, ret, i3c->base.bus.scl_rate.i2c, parent_clk);
		return -EINVAL;
	}

	i3c->b_ccc_speed.speed_hz = i3c->b_ccc_speed_hz;
	ret = mtk_i3c_set_speed(i3c, &(i3c->b_ccc_speed), parent_clk, true);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s]ret=%d,b_ccc_speed_hz=%u,clk=%u,cal_ccc_speed_fail\n",
			__func__, ret, i3c->b_ccc_speed_hz, parent_clk);
		return -EINVAL;
	}

	i3c->with7e_speed.speed_hz = i3c->base.bus.scl_rate.i3c;
	ret = mtk_i3c_set_speed(i3c, &(i3c->with7e_speed), parent_clk, true);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s]ret=%d,with7e_speed=%lu,clk=%u,cal_i3c_speed_fail\n",
			__func__, ret, i3c->base.bus.scl_rate.i3c, parent_clk);
		return -EINVAL;
	}

	i3c->without7e_speed.speed_hz = i3c->base.bus.scl_rate.i3c;
	ret = mtk_i3c_set_speed(i3c, &(i3c->without7e_speed), parent_clk, false);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s]ret=%d,without7e_speed=%lu,clk=%u,cal_i3c_speed_fail\n",
			__func__, ret, i3c->base.bus.scl_rate.i3c, parent_clk);
		return -EINVAL;
	}

	return 0;
}

int mtk_i3c_master_change_i3c_speed(struct i3c_master_controller *master,
		unsigned int scl_rate_i3c)
{
	int ret = 0;
	struct mtk_i3c_master *i3c = NULL;
	unsigned int parent_clk = 0;

	if (!master || !scl_rate_i3c) {
		pr_info("[%s] para is error.scl_rate_i3c=%u\n",
			__func__, scl_rate_i3c);
		return -EINVAL;
	}
	i3c = to_mtk_i3c_master(master);
	mtk_i3c_lock_bus(i3c);
	parent_clk = (unsigned int)clk_get_rate(i3c->clk_main);

	i3c->with7e_speed.speed_hz = scl_rate_i3c;
	ret = mtk_i3c_set_speed(i3c, &(i3c->with7e_speed), parent_clk, true);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s]ret=%d,with7e_speed=%u,clk=%u,cal_i3c_speed_fail\n",
			__func__, ret, scl_rate_i3c, parent_clk);
		i3c->with7e_speed.speed_hz = i3c->base.bus.scl_rate.i3c;
		ret = -EINVAL;
		goto err_exit;
	}

	i3c->without7e_speed.speed_hz = scl_rate_i3c;
	ret = mtk_i3c_set_speed(i3c, &(i3c->without7e_speed), parent_clk, false);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s]ret=%d,without7e_speed=%u,clk=%u,cal_i3c_speed_fail\n",
			__func__, ret, scl_rate_i3c, parent_clk);
		i3c->with7e_speed.speed_hz = i3c->base.bus.scl_rate.i3c;
		i3c->without7e_speed.speed_hz = i3c->base.bus.scl_rate.i3c;
		ret = -EINVAL;
		goto err_exit;
	}
	i3c->base.bus.scl_rate.i3c = scl_rate_i3c;

err_exit:
	mtk_i3c_unlock_bus(i3c);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_master_change_i3c_speed);

struct list_head *get_mtk_i3c_list(void)
{
	return &g_mtk_i3c_list;
}
EXPORT_SYMBOL_GPL(get_mtk_i3c_list);

static int mtk_i3c_start_enable(struct mtk_i3c_master *i3c, struct mtk_i3c_xfer *xfer)
{
	u32 control_reg = 0;
	u32 traffic_reg = 0;
	u32 shape_reg = 0;
	u32 start_reg = 0;

	if ((mtk_i3c_readl(i3c, OFFSET_TIMING) == 0) ||
		(readl(i3c->pdmabase + OFFSET_CON) == I3C_DMA_CON_DEFAULT_VALUE))
		mtk_i3c_init_hw(i3c);
	else if (xfer->dma_en) {
		writel(I3C_DMA_RST_HANDSHAKE, i3c->pdmabase + OFFSET_RST);
		writel(I3C_DMA_RST_CLR, i3c->pdmabase + OFFSET_RST);
		mtk_i3c_writel(i3c, I3C_RST_HANDSHAKE, OFFSET_SOFTRESET);
		mtk_i3c_writel(i3c, I3C_RST_CLR, OFFSET_SOFTRESET);
		udelay(1);
	}
	if (((xfer->mode == MTK_I3C_CCC_MODE) && (xfer->ccc_id < I3C_CCC_DIRECT) &&
		(xfer->ccc_id != I3C_CCC_ENTDAA)) || (xfer->mode == MTK_I3C_I2C_MODE))
		control_reg = I3C_CTRL_ACKERR_DET_EN;
	else
		control_reg = I3C_CTRL_ACKERR_DET_EN | I3C_CTRL_RS;
	if (xfer->op == MTK_I3C_MASTER_CON_WR)
		control_reg |= I3C_CTRL_RS;
	if (xfer->dma_en)
		control_reg |= I3C_CTRL_DMA_EN;
	if (xfer->op == MTK_I3C_MASTER_WRRD)
		control_reg |= I3C_CTRL_DIR_CHANGE | I3C_CTRL_RS;
	mtk_i3c_writel(i3c, control_reg, OFFSET_CONTROL);

	if (xfer->op == MTK_I3C_MASTER_RD)
		mtk_i3c_writel(i3c, (xfer->addr << 1) | I3C_M_RD, OFFSET_SLAVE_ADDR);
	else
		mtk_i3c_writel(i3c, (xfer->addr << 1), OFFSET_SLAVE_ADDR);
	/* mask and clear all interrupt for i2c, need think of i3c~~ */
	//mtk_i3c_writel(i3c, I3C_INTR_ALL, OFFSET_INTR_STAT);
	mtk_i3c_writel(i3c, I3C_FIFO_CLR_ALL, OFFSET_FIFO_ADDR_CLR);
	if (xfer->mode != MTK_I3C_I2C_MODE) {
		if (!(i3c->priv_xfer_wo7e) || (xfer->mode != MTK_I3C_SDR_MODE))
			mtk_i3c_writel(i3c, HFIFO_7E, OFFSET_HFIFO_DATA);
		traffic_reg = I3C_TRF_HANDOFF | I3C_TRF_TBIT_EN;
		shape_reg = I3C_SHAPE_T_PARITY | I3C_SHAPE_T_STALL;
		if (xfer->mode == MTK_I3C_CCC_MODE) {
			mtk_i3c_writel(i3c, HFIFO_CCC | xfer->ccc_id, OFFSET_HFIFO_DATA);
			/* broadcast ccc */
			if (xfer->ccc_id < I3C_CCC_DIRECT) {
				const u8 *txptr_fifo = xfer->tx_buf;
				u32 txsize_fifo = xfer->tx_len;

				while ((txsize_fifo--) > 0) {
					mtk_i3c_writel(i3c, HFIFO_CCC | (*txptr_fifo), OFFSET_HFIFO_DATA);
					dev_dbg(i3c->dev, "[%s] b_ccc:i3c=%p,addr=0x%x,data=0x%x,size=0x%x\n",
						__func__, i3c, xfer->addr, *txptr_fifo, txsize_fifo);
					txptr_fifo++;
				}
				if (xfer->ccc_id == I3C_CCC_ENTDAA) {
					traffic_reg = I3C_TRF_HANDOFF | I3C_TRF_DAA_EN;
					shape_reg = I3C_SHAPE_T_STALL;
				} else {
					traffic_reg |= I3C_TRF_HEAD_ONLY | I3C_TRF_SKIP_SLV_ADDR;
				}
			}
		}
		mtk_i3c_writel(i3c, traffic_reg, OFFSET_TRAFFIC);
		if ((i3c->dev_comp->hdr_exit_support == false) || (i3c->no_hdr_exit == true))
			mtk_i3c_writel(i3c, shape_reg, OFFSET_SHAPE);
		else
			mtk_i3c_writel(i3c, shape_reg | I3C_SHAPE_HDR_EXIT, OFFSET_SHAPE);
	} else {
		mtk_i3c_writel(i3c, 0, OFFSET_TRAFFIC);
		mtk_i3c_writel(i3c, 0, OFFSET_SHAPE);
	}
	if (xfer->op == MTK_I3C_MASTER_WR) {
		mtk_i3c_writel(i3c, xfer->tx_len, OFFSET_TRANSFER_LEN);
		mtk_i3c_writel(i3c, 0x1, OFFSET_TRANSAC_LEN);
	} else if (xfer->op == MTK_I3C_MASTER_RD) {
		mtk_i3c_writel(i3c, xfer->rx_len, OFFSET_TRANSFER_LEN);
		if ((xfer->mode == MTK_I3C_CCC_MODE) && (xfer->ccc_id == I3C_CCC_ENTDAA))
			mtk_i3c_writel(i3c, MTK_I3C_MAX_DEVS + 1, OFFSET_TRANSAC_LEN);
		else
			mtk_i3c_writel(i3c, 0x1, OFFSET_TRANSAC_LEN);
	} else if (xfer->op == MTK_I3C_MASTER_WRRD) {
		mtk_i3c_writel(i3c, xfer->tx_len, OFFSET_TRANSFER_LEN);
		mtk_i3c_writel(i3c, 0x2, OFFSET_TRANSAC_LEN);
		mtk_i3c_writel(i3c, xfer->rx_len, OFFSET_TRANSFER_LEN_AUX);
	} else if (xfer->op == MTK_I3C_MASTER_CON_WR) {
		mtk_i3c_writel(i3c, xfer->tx_len / xfer->nxfers, OFFSET_TRANSFER_LEN);
		mtk_i3c_writel(i3c, xfer->nxfers, OFFSET_TRANSAC_LEN);
	} else {
		dev_info(i3c->dev, "[%s] op=%d is error\n", __func__, xfer->op);
		return -EINVAL;
	}

	mtk_i3c_set_speed_reg(i3c, xfer);
	if (xfer->dma_en) {
		if (xfer->op == MTK_I3C_MASTER_WR) {
			writel(I3C_DMA_INT_FLAG_CLR, i3c->pdmabase + OFFSET_INT_FLAG);
			writel(I3C_DMA_CON_TX | I3C_DMA_CON_SKIP_CONFIG | I3C_DMA_CON_ASYNC_MODE,
				i3c->pdmabase + OFFSET_CON);
			if (xfer->tx_len != 0)
				xfer->pdma.dma_wr_buf = kmemdup(xfer->tx_buf, xfer->tx_len, GFP_KERNEL);
			if (!xfer->pdma.dma_wr_buf)
				return -ENOMEM;

			xfer->pdma.wpaddr = dma_map_single(i3c->dev,
				xfer->pdma.dma_wr_buf, xfer->tx_len, DMA_TO_DEVICE);
			if (dma_mapping_error(i3c->dev, xfer->pdma.wpaddr)) {
				kfree(xfer->pdma.dma_wr_buf);
				dev_info(i3c->dev, "[%s] MTK_I3C_WR dma_map_single error\n", __func__);
				return -ENOMEM;
			}
			writel((u32)xfer->pdma.wpaddr, i3c->pdmabase + OFFSET_TX_MEM_ADDR);
			writel(upper_32_bits(xfer->pdma.wpaddr), i3c->pdmabase + OFFSET_TX_4G_MODE);
			writel(xfer->tx_len, i3c->pdmabase + OFFSET_TX_LEN);
		} else if (xfer->op == MTK_I3C_MASTER_RD) {
			writel(I3C_DMA_INT_FLAG_CLR, i3c->pdmabase + OFFSET_INT_FLAG);
			writel(I3C_DMA_CON_RX | I3C_DMA_CON_SKIP_CONFIG | I3C_DMA_CON_ASYNC_MODE,
				i3c->pdmabase + OFFSET_CON);
			if (xfer->rx_len != 0)
				xfer->pdma.dma_rd_buf = kzalloc(xfer->rx_len, GFP_KERNEL);
			if (!xfer->pdma.dma_rd_buf)
				return -ENOMEM;

			xfer->pdma.rpaddr = dma_map_single(i3c->dev,
				xfer->pdma.dma_rd_buf, xfer->rx_len, DMA_FROM_DEVICE);
			if (dma_mapping_error(i3c->dev, xfer->pdma.rpaddr)) {
				kfree(xfer->pdma.dma_rd_buf);
				dev_info(i3c->dev, "[%s] MTK_I3C_RD dma_map_single error\n", __func__);
				return -ENOMEM;
			}
			writel((u32)xfer->pdma.rpaddr, i3c->pdmabase + OFFSET_RX_MEM_ADDR);
			writel(upper_32_bits(xfer->pdma.rpaddr), i3c->pdmabase + OFFSET_RX_4G_MODE);
			writel(xfer->rx_len, i3c->pdmabase + OFFSET_RX_LEN);
		} else if (xfer->op == MTK_I3C_MASTER_WRRD) {
			writel(I3C_DMA_INT_FLAG_CLR, i3c->pdmabase + OFFSET_INT_FLAG);
			writel(I3C_DMA_CON_DIR_CHANGE | I3C_DMA_CON_SKIP_CONFIG |
				I3C_DMA_CON_ASYNC_MODE, i3c->pdmabase + OFFSET_CON);
			if (xfer->tx_len != 0)
				xfer->pdma.dma_wr_buf = kmemdup(xfer->tx_buf, xfer->tx_len, GFP_KERNEL);
			if (!xfer->pdma.dma_wr_buf)
				return -ENOMEM;

			xfer->pdma.wpaddr = dma_map_single(i3c->dev,
				xfer->pdma.dma_wr_buf, xfer->tx_len, DMA_TO_DEVICE);
			if (dma_mapping_error(i3c->dev, xfer->pdma.wpaddr)) {
				kfree(xfer->pdma.dma_wr_buf);
				dev_info(i3c->dev, "[%s] MTK_I3C_WRRD w_dma_map error\n", __func__);
				return -ENOMEM;
			}
			if (xfer->rx_len != 0)
				xfer->pdma.dma_rd_buf = kzalloc(xfer->rx_len, GFP_KERNEL);
			if (!xfer->pdma.dma_rd_buf) {
				dma_unmap_single(i3c->dev, xfer->pdma.wpaddr,
					xfer->tx_len, DMA_TO_DEVICE);
				kfree(xfer->pdma.dma_wr_buf);
				dev_info(i3c->dev, "[%s] MTK_I3C_WRRD kzalloc error\n", __func__);
				return -ENOMEM;
			}
			xfer->pdma.rpaddr = dma_map_single(i3c->dev,
				xfer->pdma.dma_rd_buf, xfer->rx_len, DMA_FROM_DEVICE);
			if (dma_mapping_error(i3c->dev, xfer->pdma.rpaddr)) {
				dma_unmap_single(i3c->dev, xfer->pdma.wpaddr,
					xfer->tx_len, DMA_TO_DEVICE);
				kfree(xfer->pdma.dma_wr_buf);
				kfree(xfer->pdma.dma_rd_buf);
				dev_info(i3c->dev, "[%s] MTK_I3C_WRRD r_dma_map error\n", __func__);
				return -ENOMEM;
			}
			writel((u32)xfer->pdma.wpaddr, i3c->pdmabase + OFFSET_TX_MEM_ADDR);
			writel(upper_32_bits(xfer->pdma.wpaddr), i3c->pdmabase + OFFSET_TX_4G_MODE);
			writel(xfer->tx_len, i3c->pdmabase + OFFSET_TX_LEN);
			writel((u32)xfer->pdma.rpaddr, i3c->pdmabase + OFFSET_RX_MEM_ADDR);
			writel(upper_32_bits(xfer->pdma.rpaddr), i3c->pdmabase + OFFSET_RX_4G_MODE);
			writel(xfer->rx_len, i3c->pdmabase + OFFSET_RX_LEN);
		} else if (xfer->op == MTK_I3C_MASTER_CON_WR) {
			writel(I3C_DMA_INT_FLAG_CLR, i3c->pdmabase + OFFSET_INT_FLAG);
			writel(I3C_DMA_CON_TX | I3C_DMA_CON_SKIP_CONFIG | I3C_DMA_CON_ASYNC_MODE,
				i3c->pdmabase + OFFSET_CON);
			xfer->pdma.wpaddr = dma_map_single(i3c->dev,
				xfer->con_tx_buf, xfer->tx_len, DMA_TO_DEVICE);
			if (dma_mapping_error(i3c->dev, xfer->pdma.wpaddr)) {
				dev_info(i3c->dev, "[%s] MTK_I3C_CON_WR dma_map error\n", __func__);
				return -ENOMEM;
			}
			writel((u32)xfer->pdma.wpaddr, i3c->pdmabase + OFFSET_TX_MEM_ADDR);
			writel(upper_32_bits(xfer->pdma.wpaddr), i3c->pdmabase + OFFSET_TX_4G_MODE);
			writel(xfer->tx_len, i3c->pdmabase + OFFSET_TX_LEN);
		} else {
			dev_info(i3c->dev, "[%s] dma_en op=%d is error\n", __func__, xfer->op);
			return -EINVAL;
		}
#ifdef MTK_I3C_RECORD_DMA_REG
		mtk_i3c_record_dma_info(i3c);
#endif
		/* flush before sending DMA start */
		mb();
		writel(I3C_DMA_EN_START, i3c->pdmabase + OFFSET_EN);
	} else if ((xfer->op == MTK_I3C_MASTER_WR) || (xfer->op == MTK_I3C_MASTER_WRRD) ||
		(xfer->op == MTK_I3C_MASTER_CON_WR)) {
		if (!((xfer->mode == MTK_I3C_CCC_MODE) && (xfer->ccc_id < I3C_CCC_DIRECT))) {
			const u8 *txptr_fifo = xfer->tx_buf;
			u32 txsize_fifo = xfer->tx_len;

			while ((txsize_fifo--) > 0) {
				mtk_i3c_writeb(i3c, *txptr_fifo, OFFSET_DATA_PORT);
				dev_dbg(i3c->dev, "[%s] data:i3c=%p,addr=0x%x,data=0x%x,size=0x%x\n",
					__func__, i3c, xfer->addr, *txptr_fifo, txsize_fifo);
				txptr_fifo++;
			}
		}
	}
	/* All register must be prepared before setting the start bit [SMP] */
	mb();
	if ((xfer->mode == MTK_I3C_CCC_MODE) && (xfer->ccc_id == I3C_CCC_ENTDAA))
		start_reg = I3C_START_TRANSAC | I3C_START_RS_MUL_CONFIG | I3C_START_RS_MUL_TRIG;
	else
		start_reg = I3C_START_TRANSAC;
	mtk_i3c_writel(i3c, start_reg, OFFSET_START);
	/* make sure start complete */
	mb();
	return 0;
}

static int mtk_i3c_do_transfer(struct mtk_i3c_master *i3c, struct mtk_i3c_xfer *xfer)
{
	u32 intr_mask_reg = 0;
	u32 read_mask = 0;
	int ret = 0;
	unsigned long time_left = 0;
	u64 cur_time = 0;

	i3c->irq_stat = 0;
	xfer->error = I3C_ERROR_UNKNOWN;
	if ((xfer->tx_len > MTK_I3C_FIFO_SIZE) || (xfer->rx_len > MTK_I3C_FIFO_SIZE))
		xfer->dma_en = true;
	else
		xfer->dma_en = false;
	if ((xfer->tx_len == MTK_I3C_FIFO_SIZE) &&
		((xfer->mode == MTK_I3C_SDR_MODE) || (xfer->mode == MTK_I3C_CCC_MODE))) {
		dev_dbg(i3c->dev, "[%s] sdr,ccc tx 16 byte force use dma!\n", __func__);
		xfer->dma_en = true;
	}

	ret = mtk_i3c_clock_enable(i3c);
	if (ret) {
		dev_info(i3c->dev, "[%s] i3c clock enable failed!\n", __func__);
		return ret;
	}
	mtk_i3c_writel(i3c, 0, OFFSET_INTR_MASK);
	i3c->state = MTK_I3C_MASTER_START;
	reinit_completion(&i3c->msg_complete);
	ret = mtk_i3c_start_enable(i3c, xfer);
	if (ret) {
		dev_info(i3c->dev, "[%s] start enable failed!\n", __func__);
		goto err_exit;
	}
	if (i3c->fifo_use_pulling && !xfer->dma_en && (xfer->mode == MTK_I3C_SDR_MODE) &&
		(i3c->base.bus.scl_rate.i3c > I3C_BUS_I2C_FM_PLUS_SCL_RATE))
		intr_mask_reg = 0;
	else
		intr_mask_reg = I3C_INTR_IBI | I3C_INTR_HS_ACKERR | I3C_INTR_ACKERR | I3C_INTR_COMP;
	//if (xfer->dma_en)
	//	intr_mask_reg |= I3C_INTR_DMA;

	//ENTDAA using irq mode
	if ((xfer->mode == MTK_I3C_CCC_MODE) && (xfer->ccc_id == I3C_CCC_ENTDAA)) {
		mtk_i3c_writel(i3c, intr_mask_reg | I3C_INTR_RS, OFFSET_INTR_MASK);
		time_left = wait_for_completion_timeout(&i3c->msg_complete, i3c->timeout);
		read_mask = mtk_i3c_readl(i3c, OFFSET_INTR_MASK);
		/* make sure read intr_mask done */
		mb();
		mtk_i3c_writel(i3c, 0, OFFSET_INTR_MASK);
		dev_info(i3c->dev, "[%s] irq_stat=0x%x,mask=0x%x,0x%x,daa_cnt=%u,last_addr=0x%x\n",
			__func__, i3c->irq_stat, intr_mask_reg, read_mask,
			i3c->entdaa_count, i3c->entdaa_last_addr);
		if ((time_left == 0) || (i3c->irq_stat & I3C_INTR_IBI)) {
			I3C_DUMP_BUF(i3c->entdaa_addr, MTK_I3C_MAX_DEVS,
				"entdaa_count=%u,addr:", i3c->entdaa_count);
			I3C_DUMP_BUF(i3c->addrs, MTK_I3C_MAX_DEVS,
				"free_pos=0x%x,addr:", i3c->free_pos);
			mtk_i3c_dump_reg(i3c);
			dev_info(i3c->dev, "[%s] entdaa timeout or sda lowed.\n", __func__);
			mtk_i3c_init_hw(i3c);
			dev_info(i3c->dev, "[%s] entdaa after reset DBGSTA=0x%x\n",
				__func__, mtk_i3c_readl(i3c, OFFSET_DEBUGSTAT));
			mtk_i3c_gpio_dump(i3c);
			i3c->state = MTK_I3C_MASTER_IDLE;
			xfer->error = I3C_ERROR_M0;
			if (time_left == 0)
				ret = -ETIMEDOUT;
			else
				ret = -EBUSY;
			goto err_exit;
		}
		if (i3c->irq_stat & I3C_INTR_HS_ACKERR) {
			if (i3c->base.init_done) {
				xfer->error = I3C_ERROR_M2;
				ret = -ENXIO;
			}
			dev_info(i3c->dev, "[%s] entdaa hs ackerr, no slave\n", __func__);
		}
		mtk_i3c_init_hw(i3c);
	} else {
		mtk_i3c_writel(i3c, intr_mask_reg, OFFSET_INTR_MASK);
		if (intr_mask_reg) {
			time_left = wait_for_completion_timeout(&i3c->msg_complete, i3c->timeout);
			read_mask = mtk_i3c_readl(i3c, OFFSET_INTR_MASK);
			/* make sure read intr_mask done */
			mb();
			mtk_i3c_writel(i3c, 0, OFFSET_INTR_MASK);
		} else {
			//polling
			cur_time = ktime_get_ns();
			while (!mtk_i3c_readl(i3c, OFFSET_INTR_STAT) &&
				((ktime_get_ns() - cur_time) < I3C_POLLING_TIMEOUT))
				cpu_relax();
			/* make sure memory order */
			mb();
			read_mask = mtk_i3c_readl(i3c, OFFSET_INTR_MASK);
			i3c->irq_stat = mtk_i3c_readl(i3c, OFFSET_INTR_STAT);
			mtk_i3c_writel(i3c, i3c->irq_stat, OFFSET_INTR_STAT);
			i3c->state = MTK_I3C_MASTER_IDLE;
			if (i3c->irq_stat)
				time_left = 1;
			else
				time_left = 0;
		}
		if (xfer->dma_en) {
			if (xfer->op == MTK_I3C_MASTER_WR) {
				dma_unmap_single(i3c->dev, xfer->pdma.wpaddr, xfer->tx_len, DMA_TO_DEVICE);
				kfree(xfer->pdma.dma_wr_buf);
			} else if (xfer->op == MTK_I3C_MASTER_RD) {
				dma_unmap_single(i3c->dev, xfer->pdma.rpaddr, xfer->rx_len, DMA_FROM_DEVICE);
				memcpy(xfer->rx_buf, xfer->pdma.dma_rd_buf, xfer->rx_len);
				kfree(xfer->pdma.dma_rd_buf);
			} else if (xfer->op == MTK_I3C_MASTER_WRRD) {
				dma_unmap_single(i3c->dev, xfer->pdma.wpaddr, xfer->tx_len, DMA_TO_DEVICE);
				kfree(xfer->pdma.dma_wr_buf);
				dma_unmap_single(i3c->dev, xfer->pdma.rpaddr, xfer->rx_len, DMA_FROM_DEVICE);
				memcpy(xfer->rx_buf, xfer->pdma.dma_rd_buf, xfer->rx_len);
				kfree(xfer->pdma.dma_rd_buf);
			} else if (xfer->op == MTK_I3C_MASTER_CON_WR) {
				dma_unmap_single(i3c->dev, xfer->pdma.wpaddr, xfer->tx_len, DMA_TO_DEVICE);
			}
		}
		if ((time_left == 0) || (i3c->irq_stat & I3C_INTR_IBI)) {
			I3C_DUMP_BUF(i3c->entdaa_addr, MTK_I3C_MAX_DEVS,
				"entdaa_count=%u,addr:", i3c->entdaa_count);
			I3C_DUMP_BUF(i3c->addrs, MTK_I3C_MAX_DEVS,
				"free_pos=0x%x,addr:", i3c->free_pos);
			mtk_i3c_dump_reg(i3c);
			dev_info(i3c->dev, "[%s] addr=0x%x,mask=0x%x,0x%x,timeout or sda lowed.\n",
				__func__, xfer->addr, intr_mask_reg, read_mask);
			mtk_i3c_init_hw(i3c);
			dev_info(i3c->dev, "[%s] after reset DBGSTA=0x%x\n",
				__func__, mtk_i3c_readl(i3c, OFFSET_DEBUGSTAT));
			mtk_i3c_gpio_dump(i3c);
			i3c->state = MTK_I3C_MASTER_IDLE;
			if (time_left == 0)
				ret = -ETIMEDOUT;
			else
				ret = -EBUSY;
			goto err_exit;
		}
		if (i3c->irq_stat & I3C_INTR_HS_ACKERR) {
			if (i3c->base.init_done) {
				//mtk_i3c_dump_reg(i3c);
				xfer->error = I3C_ERROR_M2;
				ret = -ENXIO;
			}
			dev_info(i3c->dev, "[%s] addr=0x%x,hsackerr=0x%x,mask=0x%x,0x%x,0x%x,0x%x,%u\n",
				__func__, xfer->addr, i3c->irq_stat, intr_mask_reg, read_mask,
				xfer->mode, xfer->ccc_id, i3c->base.init_done);
			mtk_i3c_init_hw(i3c);
			goto err_exit;
		}

		if (i3c->irq_stat & I3C_INTR_ACKERR) {
			//mtk_i3c_dump_reg(i3c);
			dev_info(i3c->dev, "[%s] addr=0x%x,ackerr=0x%x,mask=0x%x,0x%x,0x%x,0x%x,%u\n",
				__func__, xfer->addr, i3c->irq_stat, intr_mask_reg, read_mask,
				xfer->mode, xfer->ccc_id, i3c->base.init_done);
			mtk_i3c_init_hw(i3c);
			if (i3c->base.init_done) {
				xfer->error = I3C_ERROR_M1;
				ret = -ENXIO;
			}
			goto err_exit;
		}

		if (!(xfer->dma_en)) {
			if ((xfer->op == MTK_I3C_MASTER_RD) || (xfer->op == MTK_I3C_MASTER_WRRD)) {
				u8 *rxptr_fifo = xfer->rx_buf;
				u32 rxsize_fifo = xfer->rx_len;

				while ((rxsize_fifo--) > 0) {
					*rxptr_fifo = mtk_i3c_readb(i3c, OFFSET_DATA_PORT);
					dev_dbg(i3c->dev, "[%s] rd:i3c=%p,addr=0x%x,data=0x%x,size=0x%x\n",
						__func__, i3c, xfer->addr, *rxptr_fifo, rxsize_fifo);
					rxptr_fifo++;
				}
			}
		}
	}

err_exit:
	mtk_i3c_clock_disable(i3c);
	return ret;
}

static int mtk_i3c_master_bus_init(struct i3c_master_controller *m)
{
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	//struct i3c_bus *bus = i3c_master_get_bus(m);
	struct i3c_device_info info = {};
	unsigned long rate;
	int ret;

	/* for reserve prue bus or mix bus speed config */
	rate = clk_get_rate(i3c->clk_main);

	mtk_i3c_lock_bus(i3c);
	ret = mtk_i3c_init_speed(i3c, rate);
	if (ret) {
		dev_info(i3c->dev, "[%s] i3c failed to set the speed.\n", __func__);
		ret = -EINVAL;
		goto err_exit;
	}
	ret = i3c_master_get_free_addr(m, 0);
	if (ret < 0)
		goto err_exit;

	memset(&info, 0, sizeof(info));
	info.dyn_addr = ret;
	ret = i3c_master_set_info(&i3c->base, &info);
	if (ret)
		goto err_exit;

err_exit:
	mtk_i3c_unlock_bus(i3c);
	return ret;
}

static void mtk_i3c_master_bus_cleanup(struct i3c_master_controller *m)
{
	//no need bus cleanup
}

static int mtk_i3c_master_attach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	struct mtk_i3c_i2c_dev_data *data;
	int pos = 0;

	pos = mtk_i3c_master_get_free_pos(i3c);
	if (pos < 0)
		return pos;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->index = pos;
	i3c->addrs[pos] = dev->info.dyn_addr ? dev->info.dyn_addr :
		dev->info.static_addr;
	i3c->free_pos &= ~BIT(pos);
	i3c_dev_set_master_data(dev, data);

	return 0;
}

static int mtk_i3c_master_reattach_i3c_dev(struct i3c_dev_desc *dev,
					  u8 old_dyn_addr)
{
	struct mtk_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);

	i3c->addrs[data->index] = dev->info.dyn_addr ? dev->info.dyn_addr :
		dev->info.static_addr;
	return 0;
}

static void mtk_i3c_master_detach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct mtk_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);

	i3c_dev_set_master_data(dev, NULL);
	i3c->addrs[data->index] = 0;
	i3c->free_pos |= BIT(data->index);
	kfree(data);
}

static int mtk_i3c_master_daa(struct i3c_master_controller *m)
{
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	int ret = 0;
	u32 pos = 0;
	u8 daa_data[9];

	mtk_i3c_lock_bus(i3c);
	ret = mtk_i3c_check_suspended(i3c);
	if (ret) {
		dev_info(i3c->dev, "[%s] Transfer while suspended.\n", __func__);
		mtk_i3c_unlock_bus(i3c);
		return ret;
	}

	i3c->xfer.mode = MTK_I3C_CCC_MODE;
	i3c->xfer.addr = I3C_BROADCAST_ADDR;
	i3c->xfer.ccc_id = I3C_CCC_ENTDAA;
	i3c->xfer.op = MTK_I3C_MASTER_RD;
	i3c->xfer.tx_len = 0;
	i3c->xfer.tx_buf = NULL;
	i3c->xfer.rx_len = 0x9;
	i3c->xfer.rx_buf = daa_data;
	i3c->xfer.nxfers = 1;
	i3c->xfer.left_num = 0;
	i3c->entdaa_last_addr = 0;
	i3c->entdaa_count = 0;
	ret = mtk_i3c_do_transfer(i3c, &(i3c->xfer));
	mtk_i3c_unlock_bus(i3c);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s] error! ret=%d\n", __func__, ret);
		return ret;
	}

	for (pos = 0; pos < i3c->entdaa_count; pos++) {
		ret = i3c_master_add_i3c_dev_locked(m, i3c->entdaa_addr[pos]);
		if (ret) {
			dev_info(i3c->dev, "[%s] add_i3c_dev error! pos=%d,addr=0x%x,count=%d\n",
				__func__, pos, i3c->entdaa_addr[pos], i3c->entdaa_count);
			return ret;
		}
	}

	return ret;
}

static bool mtk_i3c_master_supports_ccc_cmd(struct i3c_master_controller *m,
					   const struct i3c_ccc_cmd *cmd)
{
	if (cmd->ndests > 1)
		return false;

	switch (cmd->id) {
	case I3C_CCC_ENEC(true):
		break;
	case I3C_CCC_ENEC(false):
		break;
	case I3C_CCC_DISEC(true):
		break;
	case I3C_CCC_DISEC(false):
		break;
	case I3C_CCC_ENTAS(0, true):
		break;
	case I3C_CCC_ENTAS(0, false):
		break;
	case I3C_CCC_ENTAS(1, true):
		break;
	case I3C_CCC_ENTAS(1, false):
		break;
	case I3C_CCC_ENTAS(2, true):
		break;
	case I3C_CCC_ENTAS(2, false):
		break;
	case I3C_CCC_ENTAS(3, true):
		break;
	case I3C_CCC_ENTAS(3, false):
		break;
	case I3C_CCC_RSTDAA(true):
		break;
	case I3C_CCC_RSTDAA(false):
		break;
	case I3C_CCC_ENTDAA:
		break;
	case I3C_CCC_SETMWL(true):
		break;
	case I3C_CCC_SETMWL(false):
		break;
	case I3C_CCC_SETMRL(true):
		break;
	case I3C_CCC_SETMRL(false):
		break;
	case I3C_CCC_ENTTM:
		break;
	case I3C_CCC_DEFSLVS:
		break;
	case I3C_CCC_ENTHDR(0):
		break;
	case I3C_CCC_SETDASA:
		break;
	case I3C_CCC_SETNEWDA:
		break;
	case I3C_CCC_GETMWL:
		break;
	case I3C_CCC_GETMRL:
		break;
	case I3C_CCC_GETPID:
		break;
	case I3C_CCC_GETBCR:
		break;
	case I3C_CCC_GETDCR:
		break;
	case I3C_CCC_GETSTATUS:
		break;
	case I3C_CCC_GETMXDS:
		cmd->dests->payload.len = 2;
		break;
	case I3C_CCC_GETHDRCAP:
		break;
	default:
		return false;
	}
	return true;
}

static int mtk_i3c_master_send_ccc_cmd(struct i3c_master_controller *m,
				      struct i3c_ccc_cmd *cmd)
{
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	int ret = 0;
	u8 ccc_daa_data[9];

	mtk_i3c_lock_bus(i3c);
	ret = mtk_i3c_check_suspended(i3c);
	if (ret) {
		dev_info(i3c->dev, "[%s] Transfer while suspended.ccc=0x%x,addr=0x%x\n",
			__func__, cmd->id, cmd->dests->addr);
		mtk_i3c_unlock_bus(i3c);
		return ret;
	}

	i3c->xfer.mode = MTK_I3C_CCC_MODE;
	i3c->xfer.addr = cmd->dests->addr;
	i3c->xfer.ccc_id = cmd->id;
	i3c->xfer.nxfers = 1;
	i3c->xfer.left_num = 0;
	//i3c->xfer.error = cmd->err;
	if (cmd->rnw == 0) {
		i3c->xfer.op = MTK_I3C_MASTER_WR;
		i3c->xfer.tx_len = cmd->dests->payload.len;
		i3c->xfer.tx_buf = cmd->dests->payload.data;
		i3c->xfer.rx_len = 0;
		i3c->xfer.rx_buf = NULL;
	} else {
		i3c->xfer.op = MTK_I3C_MASTER_RD;
		i3c->xfer.tx_len = 0;
		i3c->xfer.tx_buf = NULL;
		i3c->xfer.rx_len = cmd->dests->payload.len;
		i3c->xfer.rx_buf = cmd->dests->payload.data;
	}
	if (cmd->id == I3C_CCC_ENTDAA) {
		i3c->xfer.op = MTK_I3C_MASTER_RD;
		i3c->xfer.tx_len = 0;
		i3c->xfer.tx_buf = NULL;
		i3c->xfer.rx_len = 0x9;
		i3c->xfer.rx_buf = ccc_daa_data;
		i3c->entdaa_last_addr = 0;
		i3c->entdaa_count = 0;
	}

	ret = mtk_i3c_do_transfer(i3c, &(i3c->xfer));
	//for slave is power down at init flow
	if (!i3c->base.init_done && (cmd->id == I3C_CCC_GETPID) &&
		(i3c->irq_stat & (I3C_INTR_HS_ACKERR | I3C_INTR_ACKERR))) {
		struct i3c_dev_desc *i3cdev = NULL;
		u8 *pid_buf = cmd->dests->payload.data;
		u64 boardinfo_pid = 0;
		int idx = 0;

		i3c_bus_for_each_i3cdev(&m->bus, i3cdev) {
			if (i3cdev->info.dyn_addr == cmd->dests->addr)
				break;
		}
		if (i3cdev && i3cdev->boardinfo) {
			boardinfo_pid = i3cdev->boardinfo->pid;
			pid_buf += (cmd->dests->payload.len - 1);
			for (idx = 0; idx < cmd->dests->payload.len; idx++) {
				*pid_buf = (u8)(boardinfo_pid & 0xff);
				boardinfo_pid >>= 8;
				pid_buf--;
			}
		}
	}

	cmd->err = i3c->xfer.error;
	mtk_i3c_unlock_bus(i3c);
	if (ret < 0) {
		dev_info(i3c->dev, "[%s] error! ret=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int mtk_i3c_master_priv_xfers(struct i3c_dev_desc *dev,
				    struct i3c_priv_xfer *i3c_xfers,
				    int i3c_nxfers)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	struct i3c_priv_xfer *msg = i3c_xfers;
	unsigned long multi_msg_len = 0;
	int left_num = i3c_nxfers;
	int ret = 0;
	int idx = 0;
	u8 *con_wr_buf;

	mtk_i3c_lock_bus(i3c);
	ret = mtk_i3c_check_suspended(i3c);
	if (ret) {
		dev_info(i3c->dev, "[%s] Transfer while suspended.dyn_addr=0x%x,nxfers=%d\n",
			__func__, dev->info.dyn_addr, i3c_nxfers);
		mtk_i3c_unlock_bus(i3c);
		return ret;
	}

	//if support hdr, dev->info.hdr_cap
	i3c->xfer.mode = MTK_I3C_SDR_MODE;
	i3c->xfer.addr = dev->info.dyn_addr;
	i3c->xfer.ccc_id = 0;
	i3c->xfer.nxfers = i3c_nxfers;

	if (i3c_nxfers > 1) {
		for (idx = 0; idx < i3c_nxfers - 1; idx++) {
			if ((msg[idx].rnw == 0) && (msg[idx + 1].rnw == 0) &&
				(msg[idx].len == msg[idx + 1].len))
				continue;
			else
				break;
		}
		if (idx >= i3c_nxfers - 1) {
			multi_msg_len = (unsigned long)msg->len * (unsigned long)i3c_nxfers;
			if (multi_msg_len > ((u16)~0U))
				goto cycle_xfer;

			con_wr_buf =  kzalloc(multi_msg_len, GFP_KERNEL);
			if (con_wr_buf) {
				for (idx = 0; idx < i3c_nxfers; idx++)
					memcpy(con_wr_buf + msg[idx].len * idx,
						msg[idx].data.out, msg[idx].len);

				i3c->xfer.op = MTK_I3C_MASTER_CON_WR;
				i3c->xfer.tx_len = multi_msg_len;
				i3c->xfer.tx_buf = con_wr_buf;
				i3c->xfer.con_tx_buf = con_wr_buf;
				i3c->xfer.rx_len = 0;
				i3c->xfer.rx_buf = NULL;
				i3c->xfer.left_num = 0;
				ret = mtk_i3c_do_transfer(i3c, &(i3c->xfer));
				i3c_xfers->err = i3c->xfer.error;
				kfree(con_wr_buf);
				if (ret < 0) {
					dev_info(i3c->dev,
						"[%s] CON_WR error! ret=%d,tx_len=%d,nxfers=%d\n",
						__func__, ret, i3c->xfer.tx_len, i3c_nxfers);
					goto exit;
				}
				ret = 0;
				goto exit;
			}
		}
	}

cycle_xfer:
	while (left_num--) {
		i3c->xfer.left_num = left_num;
		if (msg->rnw == 0) {
			i3c->xfer.op = MTK_I3C_MASTER_WR;
			i3c->xfer.tx_len = msg->len;
			i3c->xfer.tx_buf = msg->data.out;
			i3c->xfer.rx_len = 0;
			i3c->xfer.rx_buf = NULL;
			msg++;
			if ((left_num > 0) && (msg->rnw != 0)) {
				left_num--;
				i3c->xfer.op = MTK_I3C_MASTER_WRRD;
				i3c->xfer.rx_len = msg->len;
				i3c->xfer.rx_buf = msg->data.in;
				msg++;
			}
		} else {
			i3c->xfer.op = MTK_I3C_MASTER_RD;
			i3c->xfer.tx_len = 0;
			i3c->xfer.tx_buf = NULL;
			i3c->xfer.rx_len = msg->len;
			i3c->xfer.rx_buf = msg->data.in;
			msg++;
		}
		ret = mtk_i3c_do_transfer(i3c, &(i3c->xfer));
		i3c_xfers->err = i3c->xfer.error;
		if (ret < 0) {
			dev_info(i3c->dev,
				"[%s] error! ret=%d,tlen=%d,rlen=%d,left=%d,nxfers=%d\n",
				__func__, ret, i3c->xfer.tx_len, i3c->xfer.rx_len,
				i3c->xfer.left_num, i3c_nxfers);
			goto exit;
		}
	}
	ret = 0;
exit:
	mtk_i3c_unlock_bus(i3c);
	return ret;
}

static int mtk_i3c_master_attach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	struct mtk_i3c_i2c_dev_data *data;
	int pos = 0;

	pos = mtk_i3c_master_get_free_pos(i3c);
	if (pos < 0)
		return pos;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->index = pos;
	i3c->addrs[pos] = dev->addr;
	i3c->free_pos &= ~BIT(pos);
	i2c_dev_set_master_data(dev, data);

	return 0;
}

static void mtk_i3c_master_detach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct mtk_i3c_i2c_dev_data *data = i2c_dev_get_master_data(dev);
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);

	i2c_dev_set_master_data(dev, NULL);
	i3c->addrs[data->index] = 0;
	i3c->free_pos |= BIT(data->index);
	kfree(data);
}

static int mtk_i3c_master_i2c_xfers(struct i2c_dev_desc *dev,
				   const struct i2c_msg *i2c_xfers,
				   int i2c_nxfers)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct mtk_i3c_master *i3c = to_mtk_i3c_master(m);
	const struct i2c_msg *msg = i2c_xfers;
	unsigned long multi_msg_len = 0;
	int left_num = i2c_nxfers;
	int ret = 0;
	int idx = 0;
	u8 *con_wr_buf;

	mtk_i3c_lock_bus(i3c);
	ret = mtk_i3c_check_suspended(i3c);
	if (ret) {
		dev_info(i3c->dev, "[%s] Transfer while suspended.addr=0x%x,nxfers=%d\n",
			__func__, msg->addr, i2c_nxfers);
		mtk_i3c_unlock_bus(i3c);
		return ret;
	}

	i3c->xfer.mode = MTK_I3C_I2C_MODE;
	/* Doing transfers to different devices is not supported. */
	i3c->xfer.addr = msg->addr;
	i3c->xfer.ccc_id = 0;
	i3c->xfer.nxfers = i2c_nxfers;

	if (i2c_nxfers > 1) {
		for (idx = 0; idx < i2c_nxfers - 1; idx++) {
			if (!(msg[idx].flags & I2C_M_RD) && !(msg[idx + 1].flags & I2C_M_RD) &&
				(msg[idx].len == msg[idx + 1].len))
				continue;
			else
				break;
		}
		if (idx >= i2c_nxfers - 1) {
			multi_msg_len = (unsigned long)msg->len * (unsigned long)i2c_nxfers;
			if (multi_msg_len > ((u16)~0U))
				goto cycle_xfer;

			con_wr_buf =  kzalloc(multi_msg_len, GFP_KERNEL);
			if (con_wr_buf) {
				for (idx = 0; idx < i2c_nxfers; idx++)
					memcpy(con_wr_buf + msg[idx].len * idx, msg[idx].buf, msg[idx].len);

				i3c->xfer.op = MTK_I3C_MASTER_CON_WR;
				i3c->xfer.tx_len = multi_msg_len;
				i3c->xfer.tx_buf = con_wr_buf;
				i3c->xfer.con_tx_buf = con_wr_buf;
				i3c->xfer.rx_len = 0;
				i3c->xfer.rx_buf = NULL;
				i3c->xfer.left_num = 0;
				ret = mtk_i3c_do_transfer(i3c, &(i3c->xfer));
				kfree(con_wr_buf);
				if (ret < 0) {
					dev_info(i3c->dev, "[%s] CON_WR error! ret=%d,tx_len=%d,nxfers=%d\n",
						__func__, ret, i3c->xfer.tx_len, i2c_nxfers);
					goto exit;
				}
				ret = i2c_nxfers;
				goto exit;
			}
		}
	}

cycle_xfer:
	while (left_num--) {
		i3c->xfer.left_num = left_num;
		if (!(msg->flags & I2C_M_RD)) {
			i3c->xfer.op = MTK_I3C_MASTER_WR;
			i3c->xfer.tx_len = msg->len;
			i3c->xfer.tx_buf = msg->buf;
			i3c->xfer.rx_len = 0;
			i3c->xfer.rx_buf = NULL;
			msg++;
			if ((left_num > 0) && (msg->flags & I2C_M_RD)) {
				left_num--;
				i3c->xfer.op = MTK_I3C_MASTER_WRRD;
				i3c->xfer.rx_len = msg->len;
				i3c->xfer.rx_buf = msg->buf;
				msg++;
			}
		} else {
			i3c->xfer.op = MTK_I3C_MASTER_RD;
			i3c->xfer.tx_len = 0;
			i3c->xfer.tx_buf = NULL;
			i3c->xfer.rx_len = msg->len;
			i3c->xfer.rx_buf = msg->buf;
			msg++;
		}
		ret = mtk_i3c_do_transfer(i3c, &(i3c->xfer));
		if (ret < 0) {
			dev_info(i3c->dev, "[%s] error! ret=%d,tlen=%d,rlen=%d,left=%d,nxfers=%d\n",
				__func__, ret, i3c->xfer.tx_len, i3c->xfer.rx_len,
				i3c->xfer.left_num, i2c_nxfers);
			goto exit;
		}
	}
	ret = i2c_nxfers;

exit:
	mtk_i3c_unlock_bus(i3c);
	return ret;
}

static irqreturn_t mtk_i3c_irq_thread_fn(int irqno, void *dev_id)
{
	struct mtk_i3c_master *i3c = dev_id;
	int free_addr = 0;
	u8 def_da = 0;

	if ((i3c->irq_stat & (I3C_INTR_RS)) && (i3c->xfer.mode == MTK_I3C_CCC_MODE) &&
		(i3c->xfer.ccc_id == I3C_CCC_ENTDAA)) {
		/* pid,bcr,dcr canbe read if necessary. fifo will overage one byte */
		//if (i3c->entdaa_count != 0)
			//mtk_i3c_readb(i3c, OFFSET_DATA_PORT);
		mtk_i3c_writel(i3c, I3C_FIFO_CLR_ALL, OFFSET_FIFO_ADDR_CLR);
		free_addr = i3c_master_get_free_addr(&i3c->base, i3c->entdaa_last_addr + 1);
		if (free_addr < 0) {
			mtk_i3c_init_hw(i3c);
			dev_info(i3c->dev, "[%s] entdaa get_free_addr fail! addr=0x%x\n",
				__func__, i3c->entdaa_last_addr);
			return IRQ_HANDLED;
		}
		def_da = free_addr & 0x7f;
		i3c->entdaa_addr[i3c->entdaa_count++] = def_da;
		i3c->entdaa_last_addr = def_da;
		mtk_i3c_writel(i3c, I3C_DEFDA_DAA_SLV_PARITY | I3C_DEFDA_USE_DEF_DA |
			def_da, OFFSET_DEF_DA);
		/* make sure register ready before start */
		mb();
		mtk_i3c_writel(i3c, I3C_START_TRANSAC | I3C_START_RS_MUL_CONFIG |
			I3C_START_RS_MUL_TRIG, OFFSET_START);
		/* make sure start complete */
		mb();
		//for deal dummy rs irq
		//udelay(1000);
		mtk_i3c_writel(i3c, I3C_INTR_COMP | I3C_INTR_IBI | I3C_INTR_RS, OFFSET_INTR_MASK);
	}
	return IRQ_HANDLED;
}

static irqreturn_t mtk_i3c_irq(int irqno, void *dev_id)
{
	struct mtk_i3c_master *i3c = dev_id;

	i3c->irq_stat = mtk_i3c_readl(i3c, OFFSET_INTR_STAT);
	mtk_i3c_writel(i3c, i3c->irq_stat, OFFSET_INTR_STAT);
	if ((i3c->irq_stat & (I3C_INTR_RS)) && (i3c->xfer.mode == MTK_I3C_CCC_MODE) &&
		(i3c->xfer.ccc_id == I3C_CCC_ENTDAA)) {
		/* judgment ackerr to deal last 7e nack */
		if (i3c->irq_stat & (I3C_INTR_ACKERR | I3C_INTR_COMP)) {
			complete(&i3c->msg_complete);
			i3c->state = MTK_I3C_MASTER_IDLE;
			return IRQ_HANDLED;
		}
		mtk_i3c_writel(i3c, 0, OFFSET_INTR_MASK);
		if (i3c->entdaa_count >= (MTK_I3C_MAX_DEVS - 1)) {
			I3C_DUMP_BUF(i3c->entdaa_addr, MTK_I3C_MAX_DEVS,
				"entdaa_count=%u,addr:", i3c->entdaa_count);
			I3C_DUMP_BUF(i3c->addrs, MTK_I3C_MAX_DEVS,
				"free_pos=0x%x,addr:", i3c->free_pos);
			return IRQ_HANDLED;
		}
		return IRQ_WAKE_THREAD;
	}
	if (i3c->irq_stat & (I3C_INTR_IBI)) {
		complete(&i3c->msg_complete);
		return IRQ_HANDLED;
	}
	if (i3c->irq_stat & (I3C_INTR_COMP))
		complete(&i3c->msg_complete);

	i3c->state = MTK_I3C_MASTER_IDLE;
	return IRQ_HANDLED;
}

static const struct i3c_master_controller_ops mtk_i3c_master_ops = {
	.bus_init = mtk_i3c_master_bus_init,
	.bus_cleanup = mtk_i3c_master_bus_cleanup,
	.attach_i3c_dev = mtk_i3c_master_attach_i3c_dev,
	.reattach_i3c_dev = mtk_i3c_master_reattach_i3c_dev,
	.detach_i3c_dev = mtk_i3c_master_detach_i3c_dev,
	.do_daa = mtk_i3c_master_daa,
	.supports_ccc_cmd = mtk_i3c_master_supports_ccc_cmd,
	.send_ccc_cmd = mtk_i3c_master_send_ccc_cmd,
	.priv_xfers = mtk_i3c_master_priv_xfers,
	.attach_i2c_dev = mtk_i3c_master_attach_i2c_dev,
	.detach_i2c_dev = mtk_i3c_master_detach_i2c_dev,
	.i2c_xfers = mtk_i3c_master_i2c_xfers,
};

static int mtk_i3c_parse_dt(struct device_node *np, struct mtk_i3c_master *i3c)
{
	int ret;

	ret = of_property_read_u32(np, "ls-force-h-time", &i3c->ls_force_h_time_ns);
	if (ret < 0)
		i3c->ls_force_h_time_ns = I3C_LS_FORCE_H_TIME;

	ret = of_property_read_u32(np, "hs-force-h-time", &i3c->hs_force_h_time_ns);
	if (ret < 0)
		i3c->hs_force_h_time_ns = I3C_HS_FORCE_H_TIME;

	ret = of_property_read_u32(np, "head-speed", &i3c->head_speed_hz);
	if (ret < 0)
		i3c->head_speed_hz = I3C_HEAD_SPEED;

	ret = of_property_read_u32(np, "broadcast-ccc-speed", &i3c->b_ccc_speed_hz);
	if (ret < 0)
		i3c->b_ccc_speed_hz = I3C_BUS_I2C_FM_PLUS_SCL_RATE;

	ret = of_property_read_u32(np, "ch_offset_i3c", &i3c->ch_offset_i3c);
	if (ret < 0)
		i3c->ch_offset_i3c = 0;

	ret = of_property_read_u32(np, "ch_offset_dma", &i3c->ch_offset_dma);
	if (ret < 0)
		i3c->ch_offset_dma = 0;

	of_property_read_u32(np, "scl-gpio-id", &i3c->scl_gpio_id);
	of_property_read_u32(np, "sda-gpio-id", &i3c->sda_gpio_id);

	i3c->priv_xfer_wo7e = of_property_read_bool(np, "mediatek,priv-xfer-without7e");
	i3c->fifo_use_pulling = of_property_read_bool(np, "mediatek,fifo-use-pulling");
	i3c->no_hdr_exit = of_property_read_bool(np, "mediatek,no-hdr-exit");
	dev_info(i3c->dev, "[%s] priv_xfer_wo7e=%d,fifo_use_pulling=%d,no_hdr_exit=%d\n",
		__func__, i3c->priv_xfer_wo7e, i3c->fifo_use_pulling, i3c->no_hdr_exit);

	return 0;
}

static int mtk_i3c_master_probe(struct platform_device *pdev)
{
	struct mtk_i3c_master *i3c;
	int ret = 0;

	i3c = devm_kzalloc(&pdev->dev, sizeof(*i3c), GFP_KERNEL);
	if (!i3c)
		return -ENOMEM;

	i3c->dev = &pdev->dev;
	init_completion(&i3c->msg_complete);
	rt_mutex_init(&i3c->bus_lock);
	i3c->suspended = false;
	i3c->timeout = HZ / 5;
	i3c->dev_comp = of_device_get_match_data(&pdev->dev);
	if (!i3c->dev_comp)
		return -EINVAL;

	i3c->i3cbase = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i3c->i3cbase))
		return PTR_ERR(i3c->i3cbase);

	i3c->pdmabase = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(i3c->pdmabase))
		return PTR_ERR(i3c->pdmabase);

	ret = mtk_i3c_parse_dt(pdev->dev.of_node, i3c);
	if (ret)
		return -EINVAL;

	i3c->clk_main = devm_clk_get(&pdev->dev, "main");
	if (IS_ERR(i3c->clk_main)) {
		dev_info(&pdev->dev, "i3c cannot get main clock\n");
		return PTR_ERR(i3c->clk_main);
	}

	i3c->clk_dma = devm_clk_get(&pdev->dev, "dma");
	if (IS_ERR(i3c->clk_dma)) {
		dev_info(&pdev->dev, "i3c cannot get dma clock\n");
		return PTR_ERR(i3c->clk_dma);
	}

	i3c->irq = platform_get_irq(pdev, 0);
	if (i3c->irq < 0)
		return i3c->irq;

	if (i3c->dev_comp->max_dma_support > 32) {
		ret = dma_set_mask(&pdev->dev,
				DMA_BIT_MASK(i3c->dev_comp->max_dma_support));
		if (ret) {
			dev_info(&pdev->dev, "i3c dma_set_mask return error.\n");
			return ret;
		}
	}

	ret = mtk_i3c_clock_enable(i3c);
	if (ret) {
		dev_info(&pdev->dev, "i3c clock enable failed!\n");
		return ret;
	}
	mtk_i3c_init_hw(i3c);
	mtk_i3c_clock_disable(i3c);

	ret = devm_request_threaded_irq(&pdev->dev, i3c->irq, mtk_i3c_irq,
			       mtk_i3c_irq_thread_fn, IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
			       dev_name(&pdev->dev), i3c);
	if (ret) {
		dev_info(&pdev->dev, "Request I3C IRQ %d fail\n", i3c->irq);
		return ret;
	}
	i3c->free_pos = GENMASK(MTK_I3C_MAX_DEVS - 1, 0);

	platform_set_drvdata(pdev, i3c);

	//workaround for lockdep. It need to communicate with google to resolve
	lockdep_off();
	ret = i3c_master_register(&i3c->base, &pdev->dev,
		    &mtk_i3c_master_ops, false);
	lockdep_on();
	if (ret) {
		dev_info(&pdev->dev, "i3c_master_register failed!\n");
		return ret;
	}
	i3c->timeout = 2 * HZ;
	i3c->s_controller_info.id = i3c->base.bus.id;
	i3c->s_controller_info.base = &i3c->base;
	list_add_tail(&i3c->s_controller_info.list, &g_mtk_i3c_list);

	dev_dbg(&pdev->dev, "[%s] probe_id[%d] success\n", __func__, i3c->base.bus.id);
	return 0;
}

static int mtk_i3c_master_remove(struct platform_device *pdev)
{
	struct mtk_i3c_master *i3c = platform_get_drvdata(pdev);

	list_del(&i3c->s_controller_info.list);
	i3c_master_unregister(&i3c->base);

	return 0;
}

static int mtk_i3c_suspend_noirq(struct device *dev)
{
	struct mtk_i3c_master *i3c = dev_get_drvdata(dev);

	if (!i3c) {
		dev_info(dev, "[%s] i3c is NULL\n", __func__);
		return -EINVAL;
	}
	i2c_mark_adapter_suspended(&i3c->base.i2c);
	mtk_i3c_mark_suspended(i3c);

	return 0;
}

static int mtk_i3c_resume_noirq(struct device *dev)
{
	struct mtk_i3c_master *i3c = dev_get_drvdata(dev);
	int ret = 0;

	if (!i3c) {
		dev_info(dev, "[%s] i3c is NULL\n", __func__);
		return -EINVAL;
	}
	ret = mtk_i3c_clock_enable(i3c);
	if (ret) {
		dev_info(dev, "pm i3c clock enable failed!\n");
		return ret;
	}
	mtk_i3c_init_hw(i3c);
	mtk_i3c_clock_disable(i3c);
	i2c_mark_adapter_resumed(&i3c->base.i2c);
	mtk_i3c_mark_resumed(i3c);

	return 0;
}

static const struct dev_pm_ops mtk_i3c_pm = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_i3c_suspend_noirq,
			mtk_i3c_resume_noirq)
};

static const struct of_device_id mtk_i3c_of_match[] = {
	{ .compatible = "mediatek,mt6989-i3c", .data = &mt6989_compat },
	{ .compatible = "mediatek,mt6991-i3c", .data = &mt6991_compat },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_i3c_of_match);

static struct platform_driver mtk_i3c_driver = {
	.probe = mtk_i3c_master_probe,
	.remove = mtk_i3c_master_remove,
	.driver = {
		.name = "mtk-i3c-master-mt69xx",
		.pm = pm_sleep_ptr(&mtk_i3c_pm),
		.of_match_table = mtk_i3c_of_match,
	},
};
module_platform_driver(mtk_i3c_driver);

MODULE_AUTHOR("Mingchang Jia <mingchang.jia@mediatek.com>");
MODULE_DESCRIPTION("MTK I3C master mt69xx driver");
MODULE_LICENSE("GPL");
