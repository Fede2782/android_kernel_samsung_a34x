// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_helper.h"
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif
#include <video/videomode.h>


#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_dp.h"

#define DP_EN							0x0000
	#define DP_CONTROLLER_EN				BIT(0)
	#define CON_FLD_DP_EN					BIT(0)
#define DP_RST							0x0004
	#define CON_FLD_DP_RST_SEL				BIT(16)
	#define CON_FLD_DP_RST					BIT(0)
#define DP_INTEN						0x0008
	#define INT_TARGET_LINE_EN				BIT(3)
	#define INT_UNDERFLOW_EN				BIT(2)
	#define INT_VDE_EN						BIT(1)
	#define INT_VSYNC_EN					BIT(0)
#define DP_INTSTA						0x000C
	#define INTSTA_TARGET_LINE				BIT(3)
	#define INTSTA_UNDERFLOW				BIT(2)
	#define INTSTA_VDE						BIT(1)
	#define INTSTA_VSYNC					BIT(0)
#define DP_CON							0x0010
	#define CON_FLD_DP_INTL_EN				BIT(2)
	#define CON_FLD_DP_BG_EN				BIT(0)
#define DP_OUTPUT_SETTING				0x0014
	#define RB_SWAP							BIT(0)
#define DP_SIZE							0x0018
#define OUT_BIT				18
#define OUT_BIT_MASK			(0x3 << 18)
#define OUT_BIT_8			0x00
#define OUT_BIT_10			0x01
#define OUT_BIT_12			0x02
#define OUT_BIT_16			0x03

#define DP_TGEN_HWIDTH					0x0020
#define DP_TGEN_HPORCH					0x0024
#define DP_TGEN_VWIDTH					0x0028
#define DP_TGEN_VPORCH					0x002C
#define DP_BG_HCNTL						0x0030
#define DP_BG_VCNTL						0x0034
#define DP_BG_COLOR						0x0038
#define DP_FIFO_CTL						0x003C
#define DP_STATUS						0x0040
	#define DP_BUSY							BIT(24)
#define DP_DCM							0x004C
#define DP_DUMMY						0x0050
#define DP_YUV							0x0054
	#define YUV422_EN						BIT(2)
	#define YUV_EN							BIT(0)
#define DP_TGEN_VWIDTH_LEVEN			0x0068
#define DP_TGEN_VPORCH_LEVEN			0x006C
#define DP_TGEN_VWIDTH_RODD				0x0070
#define DP_TGEN_VPORCH_RODD				0x0074
#define DP_TGEN_VWIDTH_REVEN			0x0078
#define DP_TGEN_VPORCH_REVEN			0x007C
#define DP_MUTEX_VSYNC_SETTING			0x00E0
    #define MUTEX_VSYNC_SEL					BIT(16)
#define DP_SHEUDO_REG_UPDATE			0x00E4
#define DP_INTERNAL_DCM_DIS				0x00E8
#define DP_TARGET_LINE					0x00F0
#define DP_CHKSUM_EN					0x0100
#define DP_CHKSUM0						0x0104
#define DP_CHKSUM1						0x0108
#define DP_CHKSUM2						0x010C
#define DP_CHKSUM3						0x0110
#define DP_CHKSUM4						0x0114
#define DP_CHKSUM5						0x0118
#define DP_CHKSUM6						0x011C
#define DP_CHKSUM7						0x0120
#define DP_BUF_CON0						0x0210
    #define BUF_BUF_EN						BIT(0)
    #define BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK	BIT(4)
#define DP_BUF_CON1						0x0214
#define DP_BUF_RW_TIMES					0x0220
#define DP_BUF_SODI_HIGH				0x0224
#define DP_BUF_SODI_LOW					0x0228
#define DP_BUF_PREULTRA_HIGH			0x0234
#define DP_BUF_PREULTRA_LOW				0x0238
#define DP_BUF_ULTRA_HIGH				0x023C
#define DP_BUF_ULTRA_LOW				0x0240
#define DP_BUF_URGENT_HIGH				0x0244
#define DP_BUF_URGENT_LOW				0x0248
#define DP_BUF_VDE						0x024C
    #define BUF_VDE_BLOCK_URGENT			BIT(0)
    #define BUF_NON_VDE_FORCE_PREULTRA		BIT(1)
    #define BUF_VDE_BLOCK_ULTRA				BIT(2)
#define DP_SW_NP_SEL					0x0250
#define DP_PATTERN_CTRL0				0x0F00
	#define DP_PATTERN_COLOR_BAR			BIT(6)
    #define DP_PATTERN_EN					BIT(0)
#define DP_PATTERN_CTRL1				0x0F04

static const struct of_device_id mtk_dp_intf_driver_dt_match[];
/**
 * struct mtk_dp_intf - DP_INTF driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_dp_intf {
	struct mtk_ddp_comp	 ddp_comp;
	struct device *dev;
	struct mtk_dp_intf_driver_data *driver_data;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_bridge *bridge;
	void __iomem *regs;
	struct clk *hf_fmm_ck;
	struct clk *hf_fdp_ck;
	struct clk *pclk;
	struct clk *vcore_pclk;
	struct clk *pclk_src[6];
	int irq;
	struct drm_display_mode mode;
	int enable;
	int res;
};

struct mtk_dp_intf_resolution_cfg {
	unsigned int clksrc;
	unsigned int con1;
	unsigned int clk;
	unsigned int tvppll_clk;
	unsigned int dp_clk;
};

enum TVDPLL_CLK {
	TVDPLL_PLL = 0,
	TVDPLL_D2 = 1,
	TVDPLL_D4 = 2,
	TVDPLL_D8 = 3,
	TVDPLL_D16 = 4,
	TCK_26M = 5,
};

enum MT6897_TVDPLL_CLK {
	MT6897_TCK_26M = 0,
	MT6897_TVDPLL_D4 = 1,
	MT6897_TVDPLL_D8 = 2,
	MT6897_TVDPLL_D16 = 3,
};

enum MT6989_TVDPLL_CLK {
	MT6989_TCK_26M = 0,
	MT6989_TVDPLL_D16 = 1,
	MT6989_TVDPLL_D8 = 2,
	MT6989_TVDPLL_D4 = 3,
	MT6989_TVDPLL_D2 = 4,
};

enum MT6991_TVDPLL_CLK {
	MT6991_TVDPLL_PLL = 0,
	MT6991_TVDPLL_D16 = 4,
	MT6991_TVDPLL_D8 = 3,
	MT6991_TVDPLL_D4 = 2,
	MT6991_TVDPLL_D2 = 1,
	MT6991_TCK_26M = 5,
};

enum MT6899_TVDPLL_CLK {
	MT6899_TCK_26M = 0,
	MT6899_TVDPLL_D16 = 1,
	MT6899_TVDPLL_D8 = 2,
	MT6899_TVDPLL_D4 = 3,
	MT6899_TVDPLL_D2 = 4,
};

struct mtk_dp_intf_driver_data {
	const u32 reg_cmdq_ofs;
	const u8 np_sel;
	s32 (*poll_for_idle)(struct mtk_dp_intf *dp_intf,
		struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	void (*get_pll_clk)(struct mtk_dp_intf *dp_intf);
	void (*mmclk_by_datarate)(struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_intf *dp_intf, unsigned int en);
	const unsigned int bubble_rate;
	const unsigned int ovlsys_pixel_per_tick;
	const unsigned int pipe_num;
};

#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();  /*make sure register access in order */ \
	} while (0)

#define DISP_REG_SET(handle, reg32, val) \
	do { \
		if (handle == NULL) { \
			mt_reg_sync_writel(val, (unsigned long *)(reg32));\
		} \
	} while (0)

#ifdef IF_ZERO
#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = __raw_readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			mt_reg_sync_writel(regval, (reg32));  \
		} \
	} while (0)
#else
#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			writel(regval, (reg32));  \
		} \
	} while (0)

#endif

static int irq_intsa;
static int irq_vdesa;
static int irq_underflowsa;
static int irq_tl;
static unsigned long long dp_intf_bw;
static struct mtk_dp_intf *g_dp_intf;

static inline struct mtk_dp_intf *comp_to_dp_intf(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dp_intf, ddp_comp);
}

static inline struct mtk_dp_intf *encoder_to_dp_intf(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dp_intf, encoder);
}

static inline struct mtk_dp_intf *connector_to_dp_intf(struct drm_connector *c)
{
	return container_of(c, struct mtk_dp_intf, conn);
}

static void mtk_dp_intf_mask(struct mtk_dp_intf *dp_intf, u32 offset,
	u32 mask, u32 data)
{
	u32 temp = readl(dp_intf->regs + offset);

	writel((temp & ~mask) | (data & mask), dp_intf->regs + offset);
}

void dp_intf_dump_reg(void)
{
	u32 i, val[4], reg;

	for (i = 0x0; i < 0x100; i += 16) {
		reg = i;
		val[0] = readl(g_dp_intf->regs + reg);
		val[1] = readl(g_dp_intf->regs + reg + 4);
		val[2] = readl(g_dp_intf->regs + reg + 8);
		val[3] = readl(g_dp_intf->regs + reg + 12);
		DPTXDBG("dp_intf reg[0x%x] = 0x%x 0x%x 0x%x 0x%x\n",
			reg, val[0], val[1], val[2], val[3]);
	}
}

static void mtk_dp_intf_destroy_conn_enc(struct mtk_dp_intf *dp_intf)
{
	drm_encoder_cleanup(&dp_intf->encoder);
	/* Skip connector cleanup if creation was delegated to the bridge */
	if (dp_intf->conn.dev)
		drm_connector_cleanup(&dp_intf->conn);
}

static void mtk_dp_intf_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);

	irq_intsa = 0;
	irq_vdesa = 0;
	irq_underflowsa = 0;
	irq_tl = 0;
	dp_intf_bw = 0;

	mtk_dp_intf_mask(dp_intf, DP_INTSTA, 0xf, 0);
	mtk_ddp_write_mask(comp, 1,
		DP_RST, CON_FLD_DP_RST, handle);
	mtk_ddp_write_mask(comp, 0,
		DP_RST, CON_FLD_DP_RST, handle);
#ifdef IF_ZERO
	mtk_ddp_write_mask(comp,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN),
			DP_INTEN,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN), handle);
#else
	mtk_ddp_write_mask(comp,
			INT_VSYNC_EN,
			DP_INTEN,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN), handle);

#endif
	//mtk_ddp_write_mask(comp, DP_PATTERN_EN,
	//	DP_PATTERN_CTRL0, DP_PATTERN_EN, handle);
	//mtk_ddp_write_mask(comp, DP_PATTERN_COLOR_BAR,
	//	DP_PATTERN_CTRL0, DP_PATTERN_COLOR_BAR, handle);

	mtk_ddp_write_mask(comp, DP_CONTROLLER_EN,
		DP_EN, DP_CONTROLLER_EN, handle);

	dp_intf->enable = 1;
	DPTXMSG("%s, dp_intf_start:0x%x!\n",
		mtk_dump_comp_str(comp), readl(baddr + DP_EN));
	dp_intf_dump_reg();
}

static void mtk_dp_intf_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	//mtk_dp_video_trigger(video_mute<<16 | 0);
	irq_intsa = 0;
	irq_vdesa = 0;
	irq_underflowsa = 0;
	irq_tl = 0;
	dp_intf_bw = 0;

	DPTXMSG("%s, stop\n", mtk_dump_comp_str(comp));
}

void mtk_dp_inf_video_clock(struct mtk_dp_intf *dp_intf);
static void mtk_dp_intf_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_intf *dp_intf = NULL;
	DDPFUNC();
	mtk_dp_poweron_sub();

	dp_intf = comp_to_dp_intf(comp);
	mtk_dp_inf_video_clock(dp_intf);
}

void mtk_dp_intf_unprepare_clk(void)
{
	int ret = 0;

	if (g_dp_intf != NULL) {
		ret = clk_set_parent(g_dp_intf->pclk, g_dp_intf->pclk_src[TCK_26M]);
		if (ret < 0)
			DPTXMSG("%s Failed to clk_set_parent: %d\n",
				__func__, ret);
		clk_disable_unprepare(g_dp_intf->pclk);
		DPTXMSG("%s:succesed disable dp_intf and DP sel clock\n", __func__);
	} else
		DPTXERR("Failed to disable dp_intf clock\n");
}
EXPORT_SYMBOL(mtk_dp_intf_unprepare_clk);

static void mtk_dp_intf_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_intf *dp_intf = NULL;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	DPTXFUNC();
	mtk_dp_poweroff_sub();
	udelay(1000);
	mtk_ddp_write_mask(comp, 0x0, DP_EN, DP_CONTROLLER_EN, NULL);
	dp_intf = comp_to_dp_intf(comp);

	/* disable dp intf clk */
	if (dp_intf != NULL) {
		mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
		if(mtk_crtc) {
			priv = mtk_crtc->base.dev->dev_private;
			if(atomic_read(&priv->kernel_pm.status) == KERNEL_SHUTDOWN)
				dptx_shutdown();
		}
		clk_disable_unprepare(dp_intf->hf_fmm_ck);
		clk_disable_unprepare(dp_intf->hf_fdp_ck);
		clk_disable_unprepare(dp_intf->pclk);
		if(priv != NULL) {
			if (priv->data->mmsys_id == MMSYS_MT6991){
				clk_disable_unprepare(dp_intf->pclk_src[MT6991_TVDPLL_PLL]);
			} else {
				clk_disable_unprepare(dp_intf->vcore_pclk);
			}
			DPTXMSG("%s:succesed disable dp_intf clock\n", __func__);
		} else {
			DPTXERR("Failed to disable dp_intf clock\n");
		}
	}
}

void mtk_dp_inf_video_clock(struct mtk_dp_intf *dp_intf)
{
	int ret = 0;
	struct videomode vm = {0};
	unsigned int clksrc = TVDPLL_D2;
	unsigned int pll_rate;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	DDPFUNC();

	vm.pixelclock = dp_intf->mode.clock * 1000;

	//bit shift TVDPLL_D2: 1, TVDPLL_D4:2, TVDPLL_D8:3, TVDPLL_D16:4
	if (vm.pixelclock < 70000000)
		clksrc = TVDPLL_D16;
	else if (vm.pixelclock < 200000000)
		clksrc = TVDPLL_D8;
	else
		clksrc = TVDPLL_D4;

	pll_rate = vm.pixelclock * (1 << clksrc);

	DPTXMSG("%s pixel %lu clksrc %d pll_rate %d\n",
		__func__, vm.pixelclock, clksrc, pll_rate);

	ret = clk_set_rate(dp_intf->pclk_src[TVDPLL_PLL], pll_rate / 4);
	if (ret) {
		DDPMSG("%s cannot set pclk_src[TVDPLL_PLL]: err=%d\n",
			__func__, ret);
	}

	ret = clk_prepare_enable(dp_intf->pclk_src[TVDPLL_PLL]);
	if (ret) {
		DDPMSG("%s clk_prepare_enable pclk_src[TVDPLL_PLL]: err=%d\n",
			__func__, ret);
	}

	ret = clk_prepare_enable(dp_intf->pclk);
	if (ret)
		DPTXMSG("%s clk_prepare_enable dp_intf->pclk: err=%d\n",
			__func__, ret);

	ret = clk_set_parent(dp_intf->pclk, dp_intf->pclk_src[clksrc]);
	if (ret)
		DPTXMSG("%s clk_set_parent dp_intf->pclk: err=%d\n",
			__func__, ret);

	mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;

	/* dptx vcore clk control */
	if (priv->data->mmsys_id != MMSYS_MT6991) {
		ret = clk_prepare_enable(dp_intf->vcore_pclk);
		ret = clk_set_parent(dp_intf->vcore_pclk, dp_intf->pclk_src[clksrc]);
	}

	ret = clk_prepare_enable(dp_intf->hf_fmm_ck);
	if (ret < 0)
		DDPMSG("%s Failed to enable hf_fmm_ck clock: %d\n",
			__func__, ret);
	ret = clk_prepare_enable(dp_intf->hf_fdp_ck);
	if (ret < 0)
		DDPMSG("%s Failed to enable hf_fdp_ck clock: %d\n",
			__func__, ret);

	DDPMSG("%s dpintf->pclk_src[TVDPLL_PLL] =  %ld\n",
		__func__, clk_get_rate(dp_intf->pclk_src[TVDPLL_PLL]));
	DDPMSG("%s dpintf->pclk =  %ld\n",
		__func__, clk_get_rate(dp_intf->pclk));
	DDPMSG("%s dpintf->hf_fmm_ck =	%ld\n",
		__func__, clk_get_rate(dp_intf->hf_fmm_ck));
	DDPMSG("%s dpintf->hf_fdp_ck =	%ld\n",
		__func__, clk_get_rate(dp_intf->hf_fdp_ck));
}

void mtk_dp_intf_prepare_clk(void)
{
	int ret;

	ret = clk_prepare_enable(g_dp_intf->pclk);
	if (ret < 0)
		DPTXMSG("%s Failed to enable pclk: %d\n",
			__func__, ret);

	ret = clk_set_parent(g_dp_intf->pclk, g_dp_intf->pclk_src[TVDPLL_PLL]);
	if (ret < 0)
		DPTXMSG("%s Failed to clk_set_parent: %d\n",
			__func__, ret);

	DPTXMSG("%s dpintf->pclk =  %ld\n",
		__func__, clk_get_rate(g_dp_intf->pclk));
}
EXPORT_SYMBOL(mtk_dp_intf_prepare_clk);

static void mtk_dp_intf_golden_setting(struct mtk_ddp_comp *comp,
					    struct cmdq_pkt *handle)
{
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	unsigned int dp_buf_sodi_high, dp_buf_sodi_low;
	unsigned int dp_buf_preultra_high, dp_buf_preultra_low;
	unsigned int dp_buf_ultra_high, dp_buf_ultra_low;
	unsigned int dp_buf_urgent_high, dp_buf_urgent_low;
	unsigned int mmsys_clk, dp_clk; //{26000, 37125, 74250, 148500, 297000};
	unsigned int twait = 12, twake = 5;
	unsigned int fill_rate, consume_rate;

	if (dp_intf->res >= SINK_MAX || dp_intf->res < 0) {
		DPTXERR("%s:input res error: %d\n", __func__, dp_intf->res);
		dp_intf->res = SINK_1920_1080;
	}

	if (dp_intf->mode.clock >= 297000*3)
		dp_clk = 297000;
	else if (dp_intf->mode.clock >= 148500*3)
		dp_clk = 148500;
	else if (dp_intf->mode.clock >= 74250*3)
		dp_clk = 74250;
	else if (dp_intf->mode.clock >= 37125*3)
		dp_clk = 37125;
	else
		dp_clk = 26000;

	mmsys_clk = mtk_drm_get_mmclk(&mtk_crtc->base, __func__) / 1000;
	mmsys_clk = mmsys_clk > 0 ? mmsys_clk : 273000;

	fill_rate = mmsys_clk * 4 / 8;
	consume_rate = dp_clk * 4 / 8;
	DPTXMSG("%s mmsys_clk=%d, dp_intf->res=%d, dp_clk=%d, fill_rate=%d, consume_rate=%d\n",
		__func__, mmsys_clk, dp_intf->res, dp_clk, fill_rate, consume_rate);

	dp_buf_sodi_high = (5940000 - twait * 100 * fill_rate / 1000 - consume_rate) * 30 / 32000;
	dp_buf_sodi_low = (25 + twake) * consume_rate * 30 / 32000;

	dp_buf_preultra_high = 36 * consume_rate * 30 / 32000;
	dp_buf_preultra_low = 35 * consume_rate * 30 / 32000;

	dp_buf_ultra_high = 26 * consume_rate * 30 / 32000;
	dp_buf_ultra_low = 25 * consume_rate * 30 / 32000;

	dp_buf_urgent_high = 12 * consume_rate * 30 / 32000;
	dp_buf_urgent_low = 11 * consume_rate * 30 / 32000;

	DPTXDBG("dp_buf_sodi_high=%d, dp_buf_sodi_low=%d, dp_buf_preultra_high=%d, dp_buf_preultra_low=%d\n",
			dp_buf_sodi_high, dp_buf_sodi_low, dp_buf_preultra_high, dp_buf_preultra_low);

	DPTXDBG("dp_buf_ultra_high=%d, dp_buf_ultra_low=%d dp_buf_urgent_high=%d, dp_buf_urgent_low=%d\n",
			dp_buf_ultra_high, dp_buf_ultra_low, dp_buf_urgent_high, dp_buf_urgent_low);

	mtk_ddp_write_relaxed(comp, dp_buf_sodi_high, DP_BUF_SODI_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_sodi_low, DP_BUF_SODI_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_preultra_high, DP_BUF_PREULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_preultra_low, DP_BUF_PREULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_ultra_high, DP_BUF_ULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_ultra_low, DP_BUF_ULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_urgent_high, DP_BUF_URGENT_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_urgent_low, DP_BUF_URGENT_LOW, handle);

}

static void mtk_dp_intf_golden_setting_mt6899(struct mtk_ddp_comp *comp,
					    struct cmdq_pkt *handle)
{
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	/*mt6899 setting*/
	u32 dp_buf_sodi_high = 2966;
	u32 dp_buf_sodi_low = 2089;
	u32 dp_buf_preultra_high = 2506;
	u32 dp_buf_preultra_low = 2437;
	u32 dp_buf_ultra_high = 1810;
	u32 dp_buf_ultra_low = 1741;
	u32 dp_buf_urgent_high = 836;
	u32 dp_buf_urgent_low = 766;

	DPTXDBG("dp_buf_sodi_high=%d, dp_buf_sodi_low=%d, dp_buf_preultra_high=%d, dp_buf_preultra_low=%d\n",
			dp_buf_sodi_high, dp_buf_sodi_low, dp_buf_preultra_high, dp_buf_preultra_low);

	DPTXDBG("dp_buf_ultra_high=%d, dp_buf_ultra_low=%d dp_buf_urgent_high=%d, dp_buf_urgent_low=%d\n",
			dp_buf_ultra_high, dp_buf_ultra_low, dp_buf_urgent_high, dp_buf_urgent_low);

	mtk_ddp_write_relaxed(comp, dp_buf_sodi_high, DP_BUF_SODI_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_sodi_low, DP_BUF_SODI_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_preultra_high, DP_BUF_PREULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_preultra_low, DP_BUF_PREULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_ultra_high, DP_BUF_ULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_ultra_low, DP_BUF_ULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_urgent_high, DP_BUF_URGENT_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_urgent_low, DP_BUF_URGENT_LOW, handle);
}

void mhal_DPTx_ModeCopy(struct drm_display_mode *mode)
{
	drm_mode_copy(&g_dp_intf->mode, mode);
	DDPMSG("[DPTX] %s Htt=%d Vtt=%d Ha=%d Va=%d\n", __func__, g_dp_intf->mode.htotal,
		g_dp_intf->mode.vtotal, g_dp_intf->mode.hdisplay, g_dp_intf->mode.vdisplay);
}

static void mtk_dpi_set_depth(struct mtk_dp_intf *dpi, unsigned int num)
{
	u32 val;

	switch (num) {
	case 1:
		val = OUT_BIT_8;
		break;
	case 2:
		val = OUT_BIT_10;
		break;
	case 3:
		val = OUT_BIT_12;
		break;
	case 4:
		val = OUT_BIT_16;
		break;
	default:
		val = OUT_BIT_8;
		break;
	}

	mtk_dp_intf_mask(dpi, DP_OUTPUT_SETTING, val << OUT_BIT,
		     OUT_BIT_MASK);
}

static void mtk_dp_intf_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	/*u32 reg_val;*/
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;
	unsigned int hsize = 0, vsize = 0;
	unsigned int hpw = 0;
	unsigned int hfp = 0, hbp = 0;
	unsigned int vpw = 0;
	unsigned int vfp = 0, vbp = 0;
	unsigned int vtotal = 0;
	unsigned int bg_left = 0, bg_right = 0;
	unsigned int bg_top = 0, bg_bot = 0;
	unsigned int rw_times = 0;
	struct videomode vm = {0};
	unsigned int vblank_time = 0, prefetch_time = 0, config_time = 0;
	u32 val = 0, line_time = 0;
	u32 dp_vfp_mutex = 0;
	unsigned int colordepth = 0;

	mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;

	DDPFUNC();

	vm.hactive = dp_intf->mode.hdisplay;
	vm.hfront_porch = dp_intf->mode.hsync_start - dp_intf->mode.hdisplay;
	vm.hsync_len = dp_intf->mode.hsync_end - dp_intf->mode.hsync_start;
	vm.hback_porch = dp_intf->mode.htotal - dp_intf->mode.hsync_end;
	vm.vactive = dp_intf->mode.vdisplay;
	vm.vfront_porch = dp_intf->mode.vsync_start - dp_intf->mode.vdisplay;
	vm.vsync_len = dp_intf->mode.vsync_end - dp_intf->mode.vsync_start;
	vm.vback_porch = dp_intf->mode.vtotal - dp_intf->mode.vsync_end;
	vm.pixelclock = dp_intf->mode.clock * 1000;

	DDPMSG("%s Htt=%d Vtt=%d Ha=%d Va=%d\n", __func__, dp_intf->mode.htotal,
		dp_intf->mode.vtotal, dp_intf->mode.hdisplay, dp_intf->mode.vdisplay);

	hsize = vm.hactive;
	vsize = vm.vactive;
	hpw = vm.hsync_len / 4;
	hfp = vm.hfront_porch / 4;
	hbp = vm.hback_porch / 4;
	vpw = vm.vsync_len;
	vfp = vm.vfront_porch;
	vbp = vm.vback_porch;

	mtk_ddp_write_relaxed(comp, vsize << 16 | hsize,
			DP_SIZE, handle);

	mtk_ddp_write_relaxed(comp, hpw,
			DP_TGEN_HWIDTH, handle);
	mtk_ddp_write_relaxed(comp, hfp << 16 | hbp,
			DP_TGEN_HPORCH, handle);

	mtk_ddp_write_relaxed(comp, vpw,
			DP_TGEN_VWIDTH, handle);
	mtk_ddp_write_relaxed(comp, vfp << 16 | vbp,
			DP_TGEN_VPORCH, handle);

	bg_left  = 0x0;
	bg_right = 0x0;
	mtk_ddp_write_relaxed(comp, bg_left << 16 | bg_right,
			DP_BG_HCNTL, handle);

	bg_top  = 0x0;
	bg_bot  = 0x0;
	mtk_ddp_write_relaxed(comp, bg_top << 16 | bg_bot,
			DP_BG_VCNTL, handle);
#ifdef IF_ZERO
	mtk_ddp_write_mask(comp, DSC_UFOE_SEL,
			DISP_REG_DSC_CON, DSC_UFOE_SEL, handle);
	mtk_ddp_write_relaxed(comp,
			(slice_group_width - 1) << 16 | slice_width,
			DISP_REG_DSC_SLICE_W, handle);
	mtk_ddp_write(comp, 0x20000c03, DISP_REG_DSC_PPS6, handle);
#endif

	if (hsize & 0x3)
		rw_times = ((hsize >> 2) + 1) * vsize;
	else
		rw_times = (hsize >> 2) * vsize;

	mtk_ddp_write_relaxed(comp, rw_times,
			DP_BUF_RW_TIMES, handle);
	mtk_ddp_write_mask(comp, BUF_BUF_EN,
			DP_BUF_CON0, BUF_BUF_EN, handle);
	mtk_ddp_write_mask(comp, BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK,
			DP_BUF_CON0, BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK, handle);
	mtk_ddp_write_relaxed(comp, dp_intf->driver_data->np_sel,
			DP_SW_NP_SEL, handle);
	if (priv->data->mmsys_id == MMSYS_MT6899)
		mtk_dp_intf_golden_setting_mt6899(comp, handle);
	else
		mtk_dp_intf_golden_setting(comp, handle);
	val = BUF_VDE_BLOCK_URGENT | BUF_NON_VDE_FORCE_PREULTRA | BUF_VDE_BLOCK_ULTRA;
	mtk_ddp_write_relaxed(comp, val, DP_BUF_VDE, handle);

	/* fix prefetch time at 133us as DE suggests, *100 for integer calculation,
	 * and also use ceiling function for value (unit: line) written into register
	 */
	vtotal = vfp + vpw + vbp + cfg->h;
	line_time = (vtotal * cfg->vrefresh) > 0 ? 1000000 * 100 / (vtotal * cfg->vrefresh) : 1400;
	vblank_time = line_time * (vfp + vpw + vbp);
	prefetch_time = 13300;
	config_time = (vblank_time > prefetch_time) ? (vblank_time - prefetch_time) : 0;
	val = line_time > 0 ? (config_time + line_time - 100) / line_time : vfp;

	DPTXMSG("line time: %dus, vblank time: %dus, prefetch time: %dus, config time: %dus\n",
		line_time/100, vblank_time/100, prefetch_time/100, config_time/100);
	DPTXMSG("vblank line: %d, mutex_vfp= %d line, prefetch= %d line\n",
		(vfp + vpw + vbp), val, (vfp + vpw + vbp - val));


	val = val | MUTEX_VSYNC_SEL;
	mtk_ddp_write_relaxed(comp, val, DP_MUTEX_VSYNC_SETTING, handle);

	colordepth = mtk_dp_get_colordepth();
	mtk_dpi_set_depth(dp_intf, colordepth);

	DPTXMSG("%s config done\n",
			mtk_dump_comp_str(comp));

	dp_intf->enable = true;
}

int mtk_dp_intf_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = NULL;

	if (IS_ERR_OR_NULL(comp) || IS_ERR_OR_NULL(comp->regs)) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("(0x0000) DP_EN                 =0x%x\n",
			readl(baddr + DP_EN));
	DDPDUMP("(0x0004) DP_RST                =0x%x\n",
			readl(baddr + DP_RST));
	DDPDUMP("(0x0008) DP_INTEN              =0x%x\n",
			readl(baddr + DP_INTEN));
	DDPDUMP("(0x000C) DP_INTSTA             =0x%x\n",
			readl(baddr + DP_INTSTA));
	DDPDUMP("(0x0010) DP_CON                =0x%x\n",
			readl(baddr + DP_CON));
	DDPDUMP("(0x0014) DP_OUTPUT_SETTING     =0x%x\n",
			readl(baddr + DP_OUTPUT_SETTING));
	DDPDUMP("(0x0018) DP_SIZE               =0x%x\n",
			readl(baddr + DP_SIZE));
	DDPDUMP("(0x0020) DP_TGEN_HWIDTH        =0x%x\n",
			readl(baddr + DP_TGEN_HWIDTH));
	DDPDUMP("(0x0024) DP_TGEN_HPORCH        =0x%x\n",
			readl(baddr + DP_TGEN_HPORCH));
	DDPDUMP("(0x0028) DP_TGEN_VWIDTH        =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH));
	DDPDUMP("(0x002C) DP_TGEN_VPORCH        =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH));
	DDPDUMP("(0x0030) DP_BG_HCNTL           =0x%x\n",
			readl(baddr + DP_BG_HCNTL));
	DDPDUMP("(0x0034) DP_BG_VCNTL           =0x%x\n",
			readl(baddr + DP_BG_VCNTL));
	DDPDUMP("(0x0038) DP_BG_COLOR           =0x%x\n",
			readl(baddr + DP_BG_COLOR));
	DDPDUMP("(0x003C) DP_FIFO_CTL           =0x%x\n",
			readl(baddr + DP_FIFO_CTL));
	DDPDUMP("(0x0040) DP_STATUS             =0x%x\n",
			readl(baddr + DP_STATUS));
	DDPDUMP("(0x004C) DP_DCM                =0x%x\n",
			readl(baddr + DP_DCM));
	DDPDUMP("(0x0050) DP_DUMMY              =0x%x\n",
			readl(baddr + DP_DUMMY));
	DDPDUMP("(0x0068) DP_TGEN_VWIDTH_LEVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_LEVEN));
	DDPDUMP("(0x006C) DP_TGEN_VPORCH_LEVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_LEVEN));
	DDPDUMP("(0x0070) DP_TGEN_VWIDTH_RODD   =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_RODD));
	DDPDUMP("(0x0074) DP_TGEN_VPORCH_RODD   =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_RODD));
	DDPDUMP("(0x0078) DP_TGEN_VWIDTH_REVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_REVEN));
	DDPDUMP("(0x007C) DP_TGEN_VPORCH_REVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_REVEN));
	DDPDUMP("(0x00E0) DP_MUTEX_VSYNC_SETTING=0x%x\n",
			readl(baddr + DP_MUTEX_VSYNC_SETTING));
	DDPDUMP("(0x00E4) DP_SHEUDO_REG_UPDATE  =0x%x\n",
			readl(baddr + DP_SHEUDO_REG_UPDATE));
	DDPDUMP("(0x00E8) DP_INTERNAL_DCM_DIS   =0x%x\n",
			readl(baddr + DP_INTERNAL_DCM_DIS));
	DDPDUMP("(0x00F0) DP_TARGET_LINE        =0x%x\n",
			readl(baddr + DP_TARGET_LINE));
	DDPDUMP("(0x0100) DP_CHKSUM_EN          =0x%x\n",
			readl(baddr + DP_CHKSUM_EN));
	DDPDUMP("(0x0104) DP_CHKSUM0            =0x%x\n",
			readl(baddr + DP_CHKSUM0));
	DDPDUMP("(0x0108) DP_CHKSUM1            =0x%x\n",
			readl(baddr + DP_CHKSUM1));
	DDPDUMP("(0x010C) DP_CHKSUM2            =0x%x\n",
			readl(baddr + DP_CHKSUM2));
	DDPDUMP("(0x0110) DP_CHKSUM3            =0x%x\n",
			readl(baddr + DP_CHKSUM3));
	DDPDUMP("(0x0114) DP_CHKSUM4            =0x%x\n",
			readl(baddr + DP_CHKSUM4));
	DDPDUMP("(0x0118) DP_CHKSUM5            =0x%x\n",
			readl(baddr + DP_CHKSUM5));
	DDPDUMP("(0x011C) DP_CHKSUM6            =0x%x\n",
			readl(baddr + DP_CHKSUM6));
	DDPDUMP("(0x0120) DP_CHKSUM7            =0x%x\n",
			readl(baddr + DP_CHKSUM7));
	DDPDUMP("(0x0210) DP_BUF_CON0      =0x%x\n",
			readl(baddr + DP_BUF_CON0));
	DDPDUMP("(0x0214) DP_BUF_CON1      =0x%x\n",
			readl(baddr + DP_BUF_CON1));
	DDPDUMP("(0x0220) DP_BUF_RW_TIMES      =0x%x\n",
			readl(baddr + DP_BUF_RW_TIMES));
	DDPDUMP("(0x0F00) DP_PATTERN_CTRL0      =0x%x\n",
			readl(baddr + DP_PATTERN_CTRL0));
	DDPDUMP("(0x0F04) DP_PATTERN_CTRL1      =0x%x\n",
			readl(baddr + DP_PATTERN_CTRL1));

	return 0;
}

int mtk_dp_intf_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("en=%d, rst_sel=%d, rst=%d, bg_en=%d, intl_en=%d\n",
	DISP_REG_GET_FIELD(CON_FLD_DP_EN, baddr + DP_EN),
	DISP_REG_GET_FIELD(CON_FLD_DP_RST_SEL, baddr + DP_RST),
	DISP_REG_GET_FIELD(CON_FLD_DP_RST, baddr + DP_RST),
	DISP_REG_GET_FIELD(CON_FLD_DP_BG_EN, baddr + DP_CON),
	DISP_REG_GET_FIELD(CON_FLD_DP_INTL_EN, baddr + DP_CON));
	DDPDUMP("== End %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	return 0;
}

void mtk_dp_intf_set_mmclk_by_datarate(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_dp_intf *dp_intf, unsigned int en)
{
	unsigned int last_mmclk = 0;
	unsigned int req_mmclk = 0;
	unsigned int ovlsys_pixel_per_tick = 0;
	unsigned int pipe_num = 0;
	unsigned int bubble_rate = 0;
	unsigned int hact = 0;
	unsigned int vtotal = 0;
	unsigned int vrefresh = 0;
	unsigned int total_pixel_per_tick = 0;

	if (!mtk_crtc || !dp_intf)
		return;

	if (!en) {
		mtk_drm_set_mmclk_by_pixclk(&mtk_crtc->base, en, __func__);
		DPTXMSG("%s after set mmclk = %u\n", __func__, mtk_drm_get_mmclk(&mtk_crtc->base, __func__) / 1000000);
		return;
	}

	hact = dp_intf->mode.hdisplay;
	vtotal = dp_intf->mode.vtotal;
	vrefresh = drm_mode_vrefresh(&dp_intf->mode);
	bubble_rate = dp_intf->driver_data->bubble_rate ? (dp_intf->driver_data->bubble_rate) : 115;
	ovlsys_pixel_per_tick = dp_intf->driver_data->ovlsys_pixel_per_tick ?
		(dp_intf->driver_data->ovlsys_pixel_per_tick) : 2;
	pipe_num = dp_intf->driver_data->pipe_num ? (dp_intf->driver_data->pipe_num) : 1;
	total_pixel_per_tick = ovlsys_pixel_per_tick * pipe_num;
	last_mmclk = mtk_drm_get_mmclk(&mtk_crtc->base, __func__) / 1000000;
	req_mmclk = ((hact * vtotal * vrefresh + total_pixel_per_tick - 1) / total_pixel_per_tick)/1000000;
	req_mmclk = req_mmclk * bubble_rate / 100;
	DPTXMSG("%s last_mmclk = %u, req_mmclk = %u\n", __func__, last_mmclk, req_mmclk);

	if (last_mmclk != req_mmclk) {
		mtk_drm_set_mmclk_by_pixclk(&mtk_crtc->base, req_mmclk, __func__);
		DPTXMSG("%s after set mmclk = %u\n", __func__, mtk_drm_get_mmclk(&mtk_crtc->base, __func__) / 1000000);
	}
}


unsigned long long mtk_dpintf_get_frame_hrt_bw_base(
		struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_intf *dp_intf)
{
	unsigned long long base_bw;
	unsigned int vtotal, htotal;
	int vrefresh;
	u32 bpp = 4;

	/* for the case dpintf not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->base.state))
		return 1;

	htotal = mtk_crtc->base.state->adjusted_mode.htotal;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	base_bw = div_u64((unsigned long long)vtotal * htotal * vrefresh * bpp, 1000000);

	if (dp_intf_bw != base_bw) {
		dp_intf_bw = base_bw;
		DPTXMSG("%s Frame Bw:%llu, htotal:%d, vtotal:%d, vrefresh:%d\n",
			__func__, base_bw, htotal, vtotal, vrefresh);
	}

	return base_bw;
}

static unsigned long long mtk_dpintf_get_frame_hrt_bw_base_by_mode(
		struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_intf *dp_intf)
{
	unsigned long long base_bw;
	unsigned int vtotal, htotal;
	unsigned int vrefresh;
	u32 bpp = 4;

	/* for the case dpintf not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->avail_modes))
		return 1;

	htotal = mtk_crtc->avail_modes->htotal ;
	vtotal = mtk_crtc->avail_modes->vtotal;
	vrefresh = drm_mode_vrefresh(mtk_crtc->avail_modes);
	base_bw = div_u64((unsigned long long)vtotal * htotal * vrefresh * bpp, 1000000);

	if (dp_intf_bw != base_bw) {
		dp_intf_bw = base_bw;
		DPTXMSG("%s Frame Bw:%llu, htotal:%d, vtotal:%d, vrefresh:%d\n",
			__func__, base_bw, htotal, vtotal, vrefresh);
	}

	return base_bw;
}

static int mtk_dpintf_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);

	switch (cmd) {
	case SET_MMCLK_BY_DATARATE:
	{
		struct mtk_drm_crtc *mtk_crtc = NULL;
		unsigned int *en = NULL;
		struct mtk_drm_private *priv = NULL;

		mtk_crtc = comp->mtk_crtc;
		en = (unsigned int *)params;
		priv = (mtk_crtc->base).dev->dev_private;
		if (!mtk_crtc || !en || !priv)
			break;

		if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MMDVFS_SUPPORT)) {
			if (dp_intf && dp_intf->driver_data && dp_intf->driver_data->mmclk_by_datarate)
				dp_intf->driver_data->mmclk_by_datarate(mtk_crtc, dp_intf, *en);
		}
	}
	break;
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dpintf_get_frame_hrt_bw_base(mtk_crtc, dp_intf);
	}
		break;
	case GET_FRAME_HRT_BW_BY_MODE:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dpintf_get_frame_hrt_bw_base_by_mode(mtk_crtc, dp_intf);
	}
		break;
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dp_intf_funcs = {
	.config = mtk_dp_intf_config,
	.start = mtk_dp_intf_start,
	.stop = mtk_dp_intf_stop,
	.prepare = mtk_dp_intf_prepare,
	.unprepare = mtk_dp_intf_unprepare,
	.io_cmd = mtk_dpintf_io_cmd,
};

static int mtk_dp_intf_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_dp_intf *dp_intf = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DPTXMSG("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &dp_intf->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	DPTXMSG("%s-\n", __func__);
	return 0;
}

static void mtk_dp_intf_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_dp_intf *dp_intf = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_dp_intf_destroy_conn_enc(dp_intf);
	mtk_ddp_comp_unregister(drm_dev, &dp_intf->ddp_comp);
}

static const struct component_ops mtk_dp_intf_component_ops = {
	.bind = mtk_dp_intf_bind,
	.unbind = mtk_dp_intf_unbind,
};

static int mtk_dp_intf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_intf *dp_intf;
	enum mtk_ddp_comp_id comp_id;
	const struct of_device_id *of_id;
	struct resource *mem;
	int ret;

	DPTXMSG("%s+\n", __func__);
	dp_intf = devm_kzalloc(dev, sizeof(*dp_intf), GFP_KERNEL);
	if (!dp_intf)
		return -ENOMEM;
	dp_intf->dev = dev;

	of_id = of_match_device(mtk_dp_intf_driver_dt_match, &pdev->dev);
	if (!of_id) {
		dev_err(dev, "DP_intf device match failed\n");
		return -ENODEV;
	}
	dp_intf->driver_data = (struct mtk_dp_intf_driver_data *)of_id->data;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dp_intf->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(dp_intf->regs)) {
		ret = PTR_ERR(dp_intf->regs);
		dev_err(dev, "Failed to ioremap mem resource: %d\n", ret);
		return ret;
	}

	/* Get dp intf clk
	 * Input pixel clock(hf_fmm_ck) frequency needs to be > hf_fdp_ck * 4
	 * Otherwise FIFO will underflow
	 */
	dp_intf->hf_fmm_ck = devm_clk_get(dev, "hf_fmm_ck");
	if (IS_ERR(dp_intf->hf_fmm_ck)) {
		ret = PTR_ERR(dp_intf->hf_fmm_ck);
		dev_err(dev, "Failed to get hf_fmm_ck clock: %d\n", ret);
		return ret;
	}
	dp_intf->hf_fdp_ck = devm_clk_get(dev, "hf_fdp_ck");
	if (IS_ERR(dp_intf->hf_fdp_ck)) {
		ret = PTR_ERR(dp_intf->hf_fdp_ck);
		dev_err(dev, "Failed to get hf_fdp_ck clock: %d\n", ret);
		return ret;
	}

	if (dp_intf->driver_data->get_pll_clk)
		dp_intf->driver_data->get_pll_clk(dp_intf);
	else {
		dp_intf->vcore_pclk = devm_clk_get(dp_intf->dev, "MUX_VCORE_DP");
		dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
		dp_intf->pclk_src[0] = devm_clk_get(dp_intf->dev, "DPI_CK");
		dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D2");
		dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
		dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
		dp_intf->pclk_src[4] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
		if (IS_ERR(dp_intf->pclk)
			|| IS_ERR(dp_intf->vcore_pclk)
			|| IS_ERR(dp_intf->pclk_src[1])
			|| IS_ERR(dp_intf->pclk_src[2])
			|| IS_ERR(dp_intf->pclk_src[3]))
			DPTXMSG("Failed to get pclk andr src clock !!!\n");
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DP_INTF);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dp_intf->ddp_comp, comp_id,
				&mtk_dp_intf_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	/* Get dp intf irq num and request irq */
	dp_intf->irq = platform_get_irq(pdev, 0);
	dp_intf->res = SINK_MAX;
	if (dp_intf->irq <= 0) {
		dev_err(dev, "Failed to get irq: %d\n", dp_intf->irq);
		return -EINVAL;
	}

	irq_set_status_flags(dp_intf->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dp_intf->irq, dp_intf->driver_data->irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), dp_intf);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mediatek dp intf irq\n");
		ret = -EPROBE_DEFER;
		return ret;
	}

	platform_set_drvdata(pdev, dp_intf);
	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dp_intf_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	g_dp_intf = dp_intf;
	DPTXMSG("%s-\n", __func__);
	return ret;
}

static int mtk_dp_intf_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dp_intf_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static s32 mtk_dp_intf_poll_for_idle(struct mtk_dp_intf *dp_intf,
	struct cmdq_pkt *handle)
{
	return 0;
}

static void mtk_dp_intf_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D2");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[4] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[0])
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3])
		|| IS_ERR(dp_intf->pclk_src[4]))
		dev_err(dp_intf->dev, "Failed to get pclk andr src clock !!!\n");
}

static void mtk_dp_intf_mt6897_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[0])
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		dev_err(dp_intf->dev, "Failed to get pclk andr src clock !!!\n");
}

static void mtk_dp_intf_mt6989_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	/* Need to config both vcore and vdisplay */
	dp_intf->vcore_pclk = devm_clk_get(dp_intf->dev, "MUX_VCORE_DP");
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->vcore_pclk)
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		dev_err(dp_intf->dev, "Failed to get pclk andr src clock !!!\n");
}

static void mtk_dp_intf_mt6991_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[MT6991_TCK_26M] = devm_clk_get(dp_intf->dev, "DPI_26M");
	dp_intf->pclk_src[MT6991_TVDPLL_D4] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[MT6991_TVDPLL_D8] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[MT6991_TVDPLL_D16] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[MT6991_TVDPLL_PLL] = devm_clk_get(dp_intf->dev, "DPI_CK");

	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[MT6991_TCK_26M])
		|| IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_D4])
		|| IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_D8])
		|| IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_D16])
		|| IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_PLL]))
		DPTXERR("Failed to get pclk andr src clock, -%d-%d-%d-%d-%d-%d-\n",
			IS_ERR(dp_intf->pclk),
			IS_ERR(dp_intf->pclk_src[MT6991_TCK_26M]),
			IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_D4]),
			IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_D8]),
			IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_D16]),
			IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_PLL]));
}

static void mtk_dp_intf_mt6899_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	/* Need to config both vcore and vdisplay */
	dp_intf->vcore_pclk = devm_clk_get(dp_intf->dev, "MUX_VCORE_DP");
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->vcore_pclk)
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		DPTXERR("Failed to get pclk andr src clock, -%d-%d-%d-%d-%d-\n",
			IS_ERR(dp_intf->pclk),
			IS_ERR(dp_intf->vcore_pclk),
			IS_ERR(dp_intf->pclk_src[1]),
			IS_ERR(dp_intf->pclk_src[2]),
			IS_ERR(dp_intf->pclk_src[3]));
}

static irqreturn_t mtk_dp_intf_irq_status(int irq, void *dev_id)
{
	struct mtk_dp_intf *dp_intf = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv = NULL;
	int dpintf_opt = 0;
	mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;
	dpintf_opt = mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_DPINTF_UNDERFLOW_AEE);

	status = readl(dp_intf->regs + DP_INTSTA);

	DRM_MMP_MARK(dp_intf0, status, 0);

	status &= 0xf;
	if (status) {
		mtk_dp_intf_mask(dp_intf, DP_INTSTA, status, 0);
		if (status & INTSTA_VSYNC) {
			// mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
			mtk_crtc_vblank_irq(&mtk_crtc->base);
			irq_intsa++;
		}

		if (status & INTSTA_VDE)
			irq_vdesa++;

		if (status & INTSTA_UNDERFLOW) {
			DPTXMSG("%s dpintf_underflow!\n", __func__);
			irq_underflowsa++;
		}

		if (status & INTSTA_TARGET_LINE)
			irq_tl++;
	}

	if (irq_intsa == 3)
		mtk_dp_video_trigger(video_unmute << 16 | dp_intf->res);

	if (dpintf_opt && (status & INTSTA_UNDERFLOW) && (irq_underflowsa == 1)) {
#if IS_ENABLED(CONFIG_ARM64)
		DDPAEE("DPINTF underflow 0x%x. TS: 0x%08llx\n",
			status, arch_timer_read_counter());
#else
		DDPAEE("DPINTF underflow 0x%x\n",
			status);
#endif
		mtk_drm_crtc_analysis(&(mtk_crtc->base));
		mtk_drm_crtc_dump(&(mtk_crtc->base));
		mtk_smi_dbg_hang_detect("dpintf underflow");
	}

	return IRQ_HANDLED;
}

static const struct mtk_dp_intf_driver_data mt6885_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6895_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6985_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6897_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_mt6897_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6989_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 2,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_mt6989_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6991_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 2,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_mt6991_get_pll_clk,
	.mmclk_by_datarate = mtk_dp_intf_set_mmclk_by_datarate,
	.bubble_rate = 115,
	.ovlsys_pixel_per_tick = 2,
	.pipe_num = 1,
};

static const struct mtk_dp_intf_driver_data mt6899_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 2,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.get_pll_clk = mtk_dp_intf_mt6899_get_pll_clk,
};

static const struct of_device_id mtk_dp_intf_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-dp-intf",
	.data = &mt6885_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6895-dp-intf",
	.data = &mt6895_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6985-dp-intf",
	.data = &mt6985_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6897-dp-intf",
	.data = &mt6897_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6989-dp-intf",
	.data = &mt6989_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6991-dp-intf",
	.data = &mt6991_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6899-dp-intf",
	.data = &mt6899_dp_intf_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dp_intf_driver_dt_match);

struct platform_driver mtk_dp_intf_driver = {
	.probe = mtk_dp_intf_probe,
	.remove = mtk_dp_intf_remove,
	.driver = {
		.name = "mediatek-dp-intf",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dp_intf_driver_dt_match,
	},
};
