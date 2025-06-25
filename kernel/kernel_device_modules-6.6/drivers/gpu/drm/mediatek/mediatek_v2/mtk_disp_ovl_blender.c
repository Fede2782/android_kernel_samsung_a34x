// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ratelimit.h>
#include <linux/iommu.h>
#include <arm/arm-smmu-v3/arm-smmu-v3.h>
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_layering_rule_base.h"
#include "mtk_rect.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_ovl_blender.h"
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "platform/mtk_drm_platform.h"

#define DISP_REG_OVL_BLD_STA		(0x000UL)
	#define ENG_ACTIVE					BIT(0)
	#define IN_RLY0_IDLE				BIT(1)
	#define IN_RLY1_IDLE				BIT(2)
	#define BLD_IDLE					BIT(3)
	#define OUT_RLY_IDLE				BIT(4)
	#define ADDCON_IDLE					BIT(5)
	#define ENG_IDLE					BIT(6)
#define DISP_REG_OVL_BLD_INTEN		(0x0004)
#define DISP_REG_OVL_BLD_INTSTA		(0x0008)
	#define REG_UPDATE			REG_FLD_MSB_LSB(0, 0)
	#define FRAME_DONE			REG_FLD_MSB_LSB(1, 1)
	#define ABNORMAL_SOF		REG_FLD_MSB_LSB(2, 2)
	#define OVL_START_INTEN		REG_FLD_MSB_LSB(3, 3)
	#define OVL_ROL_TIMING_0	REG_FLD_MSB_LSB(8, 8)
	#define OVL_ROL_TIMING_1	REG_FLD_MSB_LSB(9, 9)
	#define OVL_ROL_TIMING_2	REG_FLD_MSB_LSB(10, 10)
	#define OVL_ROL_TIMING_3	REG_FLD_MSB_LSB(11, 11)
	#define OVL_ROL_TIMING_4	REG_FLD_MSB_LSB(12, 12)
	#define OVL_ROL_TIMING_5	REG_FLD_MSB_LSB(13, 13)
	#define OVL_ROL_TIMING_6	REG_FLD_MSB_LSB(14, 14)
	#define OVL_ROL_TIMING_7	REG_FLD_MSB_LSB(15, 15)

#define DISP_REG_OVL_BLD_FUNC_DCM_DIS0		(0x000CUL)
#define DISP_REG_OVL_BLD_DATAPATH_CON		(0x0010UL)
	#define DISP_BGCLR_IN_SEL				BIT(0)
	#define DISP_BGCLR_OUT_TO_PROC			BIT(4)
	#define DISP_BGCLR_OUT_TO_NEXT_LAYER	BIT(5)
	#define DISP_BLD_BGCLR_IN_SEL			REG_FLD_MSB_LSB(0, 0)
	#define DISP_BGCLR_OUT_SEL				REG_FLD_MSB_LSB(5, 4)
	#define DISP_BLD_OUT_PROC				REG_FLD_MSB_LSB(4, 4)
	#define DISP_BLD_OUT_NEXT_LAYER			REG_FLD_MSB_LSB(5, 5)
	#define DISP_OUTPUT_INTERLACE			BIT(16)
#define DISP_REG_OVL_BLD_EN				(0x0020UL)
	#define DISP_OVL_EN						BIT(0)
	#define DISP_OVL_FORCE_RELAY_MODE		BIT(4)
	#define DISP_OVL_RELAY_MODE				BIT(5)

#define DISP_REG_OVL_BLD_RST			(0x0024UL)
#define DISP_REG_OVL_BLD_SHADOW_CTRL	(0x0028UL)
	#define DISP_OVL_READ_WRK_REG			BIT(0)
	#define DISP_OVL_FORCE_COMMIT			BIT(1)
	#define DISP_OVL_BYPASS_SHADOW			BIT(2)

#define DISP_REG_OVL_BLD_ROI_SIZE	(0x0030)
#define DISP_REG_OVL_BLD_L_EN(n)		(0x0040UL + 0x30 * (n))
	#define DISP_OVL_L_EN				BIT(0)
	#define DISP_OVL_L_FBCD_EN			BIT(4)
	#define LSRC_COLOR					BIT(8)
	#define LSRC_UFOD					BIT(9)
	#define LSRC_PQ						(BIT(8) | BIT(9))
	#define L_CON_FLD_LSRC				REG_FLD_MSB_LSB(9, 8)

#define DISP_REG_BLD_OVL_OFFSET(n)		(0x0044UL + 0x30 * (n))
#define DISP_REG_BLD_OVL_SRC_SIZE(n)	(0x0048UL + 0x30 * (n))
#define DISP_REG_BLD_OVL_CLIP(n)		(0x004CUL + 0x30 * (n))
	#define OVL_L_CLIP_FLD_LEFT				REG_FLD_MSB_LSB(7, 0)
	#define OVL_L_CLIP_FLD_RIGHT			REG_FLD_MSB_LSB(15, 8)
	#define OVL_L_CLIP_FLD_TOP				REG_FLD_MSB_LSB(23, 16)
	#define OVL_L_CLIP_FLD_BOTTOM			REG_FLD_MSB_LSB(31, 24)

#define OVL_BLD_L0_CLRFMT(n)	(0x0050 + 0x30 * (n))
	#define OVL_CON_CLRFMT_MAN		BIT(4)

	#define OVL_CON_CLRFMT_RGB (1UL)
	#define OVL_CON_CLRFMT_RGBA8888 (2)
	#define OVL_CON_CLRFMT_ARGB8888 (3)
#define OVL_CON_CLRFMT_DIM (1 << 8)
#define OVL_CON_CLRFMT_RGB565(module)                                          \
		(((module)->data->fmt_rgb565_is_0 == true) ? 0UL : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(module)                                          \
		(((module)->data->fmt_rgb565_is_0 == true) ? OVL_CON_CLRFMT_RGB : 0UL)
#define OVL_CON_CLRFMT_UYVY(module) ((module)->data->fmt_uyvy)
#define OVL_CON_CLRFMT_YUYV(module) ((module)->data->fmt_yuyv)
#define OVL_CON_AEN BIT(8)
#define OVL_CON_ALPHA 0xff

#define OVL_BLD_BGCLR_CON		(0x100)
#define OVL_BLD_BGCLR_CLG		(0x104)
	#define ROI_BGCLR_BLUE			REG_FLD_MSB_LSB(7, 0)
	#define ROI_BGCLR_GREEN			REG_FLD_MSB_LSB(15, 8)
	#define ROI_BGCLR_BED			REG_FLD_MSB_LSB(23, 16)
	#define ROI_BGCLR_ALPHA			REG_FLD_MSB_LSB(31, 24)
#define OVL_ROI_BGCLR (0xFF000000)

#define OVL_BLD_L_CON2(n)		(0x200 + 0x100*(n))
#define L_ALPHA					REG_FLD_MSB_LSB(7, 0)
#define L_ALPHA_EN				REG_FLD_MSB_LSB(12, 12)
#define L_SRCKEY_EN				REG_FLD_MSB_LSB(16, 16)
#define L_DSTKEY_EN				REG_FLD_MSB_LSB(17, 17)

#define OVL_BLD_SRCKEY(n)		(0x204 + 0x100*(n))
#define OVL_BLD_L_PITCH(n)		(0x208 + 0x100*(n))
#define L0_CONST_BLD			BIT(24)
#define L_SA_SEL				REG_FLD_MSB_LSB(2, 0)
#define L_SRGB_SEL				REG_FLD_MSB_LSB(7, 4)
#define L_DA_SEL				REG_FLD_MSB_LSB(10, 8)
#define L_DRGB_SEL				REG_FLD_MSB_LSB(15, 12)
#define L_CONST_BLD				REG_FLD_MSB_LSB(24, 24)
#define L_BLEND_RND_SHT			REG_FLD_MSB_LSB(25, 25)
#define L_MINUS_BLD				REG_FLD_MSB_LSB(26, 26)
#define L_SURFL_EN				REG_FLD_MSB_LSB(31, 31)
#define OVL_BLD_L_CLR(n)		(0x20c + 0x100*(n))
#define L_CNST_CLR_BLUE			REG_FLD_MSB_LSB(7, 0)
#define L_CNST_CLR_GREEN		REG_FLD_MSB_LSB(15, 8)
#define L_CNST_CLR_RED			REG_FLD_MSB_LSB(23, 16)
#define L_CNST_CLR_ALPHA		REG_FLD_MSB_LSB(31, 24)

#define OVL_BLD_OVLBN_CTRL		(0x600)
#define L0_BN_EN				BIT(0)
#define EL0_BN_EN				BIT(5)
#define EL1_BN_EN				BIT(6)
#define EL2_BN_EN				BIT(7)

#define OVL_BLD_OVLBN_CTRL1		(0x604)

#define OVL_BLD_OVLBN0_CTRL2		(0x608)
#define OVL_BLD_OVLBN1_CTRL4		(0x610)
#define OVL_BLD_OVLBN2_CTRL6		(0x618)
#define OVL_BLD_OVLBN3_CTRL8		(0x620)
	#define ALPHA_2_SHIFT	REG_FLD_MSB_LSB(4, 0)
	#define ALPHA_4_SHIFT	REG_FLD_MSB_LSB(12, 8)
	#define ALPHA_16_SHIFT	REG_FLD_MSB_LSB(20, 17)

#define OVL_BLD_OVLBN0_CTRL3		(0x60C)
#define OVL_BLD_OVLBN1_CTRL5		(0x614)
#define OVL_BLD_OVLBN2_CTRL7		(0x61C)
#define OVL_BLD_OVLBN3_CTRL9		(0x624)
	#define ALPHA_32_SHIFT	REG_FLD_MSB_LSB(4, 0)
	#define ALPHA_64_SHIFT	REG_FLD_MSB_LSB(12, 8)
	#define ALPHA_128_SHIFT	REG_FLD_MSB_LSB(20, 17)

#define OVL_BLD_OVLBN_CTRL10		(0x628)
	#define BN_FPHASE_EN	REG_FLD_MSB_LSB(0, 0)
	#define BN_FPHASE_R		REG_FLD_MSB_LSB(1, 1)
	#define BN_FPHASE_CTRL	REG_FLD_MSB_LSB(5, 4)
	#define BN_FPHASE_SEL	REG_FLD_MSB_LSB(9, 8)
	#define BN_FPHASE_BIT	REG_FLD_MSB_LSB(14, 12)
	#define BN_FPHASE		REG_FLD_MSB_LSB(21, 16)

#define OVL_BLD_OVLBN_CTRL11		(0x62C)
	#define BN_CRC24_RND_SEED			REG_FLD_MSB_LSB(23, 0)
	#define BN_CRC24_RND_SEED_UPDATE	REG_FLD_MSB_LSB(24, 24)

#define OVL_BLD_OVLBN_CTRL12		(0x630)
	#define	L0_BN_ALPHA_SEL				REG_FLD_MSB_LSB(2, 0)
	#define	L0_BN_ALPHA_SEL_MAN			REG_FLD_MSB_LSB(3, 3)
	#define	EL0_BN_ALPHA_SEL			REG_FLD_MSB_LSB(22, 20)
	#define	EL0_BN_ALPHA_SEL_MAN		REG_FLD_MSB_LSB(23, 23)
	#define	EL1_BN_ALPHA_SEL			REG_FLD_MSB_LSB(24, 26)
	#define	EL1_BN_ALPHA_SEL_MAN		REG_FLD_MSB_LSB(27, 27)
	#define	EL2_BN_ALPHA_SEL			REG_FLD_MSB_LSB(30, 28)
	#define	EL2_BN_ALPHA_SEL_MAN		REG_FLD_MSB_LSB(31, 31)

#define OVL_BLD_OVLBN_CTRL13		(0x634)
	#define	BN_FPHASE_INIT				REG_FLD_MSB_LSB(29, 24)
	#define	BN_FPHASE_INIT_UPDATE		REG_FLD_MSB_LSB(31, 31)

#define OVL_BLD_ROI_TIMING_0		(0x700)
#define OVL_BLD_ROI_TIMING_2		(0x704)
#define OVL_BLD_ROI_TIMING_4		(0x708)
#define OVL_BLD_ROI_TIMING_6		(0x70c)

#define OVL_BLD_DBG_STATUS0			(0x7a0)
#define OVL_BLD_DBG_STATUS1			(0x7a4)
#define OVL_BLD_DBG_STATUS3			(0x7ac)
#define OVL_BLD_DBG_STATUS5			(0x7b4)
#define BGCLR_IN_VALID		REG_FLD_MSB_LSB(0, 0)
#define BGCLR_IN_READY		REG_FLD_MSB_LSB(1, 1)
#define LAYER_IN_VALID		REG_FLD_MSB_LSB(2, 2)
#define LAYER_IN_READY		REG_FLD_MSB_LSB(3, 3)
#define BLD2CLR_VALID		REG_FLD_MSB_LSB(12,12)
#define BLD2CLR_READY		REG_FLD_MSB_LSB(13,13)
#define BLD2BLD_VALID		REG_FLD_MSB_LSB(14,14)
#define BLD2BLD_READY		REG_FLD_MSB_LSB(15,15)

//enum mtk_ddp_comp_id g_last_active_bld = DDP_COMPONENT_OVL0_BLENDER0;

#define MAX_LAYER_NUM 1
struct mtk_ovl_backup_info {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int con;
	dma_addr_t addr;
	unsigned int src_size;
	unsigned int src_pitch;
	unsigned int data_path_con;
};

struct mtk_disp_ovl_blender {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_ovl_blender_data *data;
	unsigned int underflow_cnt;
	bool ovl_dis;
	int bg_w, bg_h;
	struct clk *fbdc_clk;
	struct mtk_ovl_backup_info backup_info[MAX_LAYER_NUM];
	unsigned int set_partial_update;
	unsigned int roi_height;
};

static int mtk_ovl_blender_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params);
static void mtk_ovl_blender_connect(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_id prev,
			    enum mtk_ddp_comp_id next);

static inline struct mtk_disp_ovl_blender *comp_to_ovl_blender(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ovl_blender, ddp_comp);
}

int mtk_ovl_blender_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;
	int offset;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	/* STA, INTEN, INTSTA, DCM_CTRL, DATAPATH_CON*/
	mtk_serial_dump_reg(baddr, 0x00, 4);
	mtk_serial_dump_reg(baddr, 0x10, 1);

	for (i = 0; i < 15; i++) {
		offset = 0x20 + (i * 0x10);
		if (offset == 0x060 || offset == 0x090 || offset == 0x0c0 || offset == 0x0f0)
			continue;
		mtk_serial_dump_reg(baddr, offset, 4);
	}

	for (i = 0; i < 4; i++)
		mtk_serial_dump_reg(baddr, 0x200 + i * 0x100, 4);

	mtk_serial_dump_reg(baddr, 0x600, 4);
	mtk_serial_dump_reg(baddr, 0x610, 4);
	mtk_serial_dump_reg(baddr, 0x620, 4);
	mtk_serial_dump_reg(baddr, 0x630, 2);
	mtk_serial_dump_reg(baddr, 0x7a0, 4);
	mtk_serial_dump_reg(baddr, 0x7b0, 3);
	return 1;
}

static void ovl_dump_bld_layer_info(struct mtk_ddp_comp *comp, int layer)
{
	unsigned int con, lsrc, src_size, offset, pitch, clip;
	/*  enum UNIFIED_COLOR_FMT fmt; */
	void __iomem *baddr = comp->regs;
	void __iomem *Lx_base;
	static const char * const pixel_src[] = { "mem", "color", "ufod", "pq" };

	Lx_base = baddr;
	con = readl(OVL_BLD_L_CON2(layer) + Lx_base);
	lsrc = readl(DISP_REG_OVL_BLD_L_EN(layer) + Lx_base);
	offset = readl(DISP_REG_BLD_OVL_OFFSET(layer) + Lx_base);
	src_size = readl(DISP_REG_BLD_OVL_SRC_SIZE(layer) + Lx_base);
	pitch = readl(OVL_BLD_L_PITCH(layer) + Lx_base);
	clip = readl(DISP_REG_BLD_OVL_CLIP(layer) + Lx_base);

	/* TODO
	 * fmt = display_fmt_reg_to_unified_fmt(
	 * REG_FLD_VAL_GET(L_CON_FLD_CFMT, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_BTSW, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, con));
	 */
	DDPDUMP("%s_L%d:  size=%ux%u,",
		layer ? "ext" : "phy", layer,
		src_size & 0xfff,
		(src_size >> 16) & 0xfff);
	DDPDUMP("offset=(%u,%u), clip(L,R,T,B)=(%u,%u,%u,%u)\n",
		offset & 0xfff,
		(offset >> 16) & 0xfff,
		REG_FLD_VAL_GET(OVL_L_CLIP_FLD_LEFT,clip),
		REG_FLD_VAL_GET(OVL_L_CLIP_FLD_RIGHT,clip),
		REG_FLD_VAL_GET(OVL_L_CLIP_FLD_TOP,clip),
		REG_FLD_VAL_GET(OVL_L_CLIP_FLD_BOTTOM,clip));
	DDPDUMP("layer_src=0x%x, clrfmt=0x%x, a_en=0x%x, alpha=0x%x pitch=0x%x\n",
		REG_FLD_VAL_GET(L_CON_FLD_LSRC, lsrc),
		readl(OVL_BLD_L0_CLRFMT(layer) + baddr),
		REG_FLD_VAL_GET(L_ALPHA_EN, con),
		REG_FLD_VAL_GET(L_ALPHA, con),
		pitch & 0xffff);
}

static void ovl_blender_printf_status(unsigned int status)
{
	DDPDUMP("BGCLR_IN:v:%d,r:%d\n", REG_FLD_VAL_GET(BGCLR_IN_VALID, status),
			REG_FLD_VAL_GET(BGCLR_IN_READY, status));
	DDPDUMP("LAYER_IN:v:%d,r:%d\n", REG_FLD_VAL_GET(LAYER_IN_VALID, status),
			REG_FLD_VAL_GET(LAYER_IN_READY, status));
	DDPDUMP("BLD2CLR:v:%d,r:%d\n", REG_FLD_VAL_GET(BLD2CLR_VALID, status),
			REG_FLD_VAL_GET(BLD2CLR_READY, status));
	DDPDUMP("BLD2BLD:v:%d,r:%d\n", REG_FLD_VAL_GET(BLD2BLD_VALID, status),
			REG_FLD_VAL_GET(BLD2BLD_READY, status));
}

int mtk_ovl_blender_analysis(struct mtk_ddp_comp *comp)
{
	int i = 0;
	void __iomem *baddr = comp->regs;
	unsigned int ovl_en, layer_en[4];
	unsigned int path_con;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	ovl_en = readl(DISP_REG_OVL_BLD_EN + baddr);
	layer_en[0] = readl(DISP_REG_OVL_BLD_L_EN(0) + baddr);
	layer_en[1] = readl(DISP_REG_OVL_BLD_L_EN(1) + baddr);
	layer_en[2] = readl(DISP_REG_OVL_BLD_L_EN(2) + baddr);
	layer_en[3] = readl(DISP_REG_OVL_BLD_L_EN(3) + baddr);

	path_con = readl(DISP_REG_OVL_BLD_DATAPATH_CON + baddr);

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("ovl_en=%d,\nbgclr_in_sel=%d, to_outproc=%d, to_next_layer=%d\n",
		ovl_en & 0x1,
		REG_FLD_VAL_GET(DISP_BLD_BGCLR_IN_SEL, path_con),
		REG_FLD_VAL_GET(DISP_BLD_OUT_PROC, path_con),
		REG_FLD_VAL_GET(DISP_BLD_OUT_NEXT_LAYER, path_con));
	DDPDUMP("sta=0x%x\n",readl(DISP_REG_OVL_BLD_STA + baddr));
	DDPDUMP("roi_size=%dx%d, cur_pos=(%d,%d), pos_hit=0x%x, src_rly_pos=(%d,%d)\n",
		readl(DISP_REG_OVL_BLD_ROI_SIZE + baddr) & 0xfff,
		(readl(DISP_REG_OVL_BLD_ROI_SIZE + baddr) >> 16) & 0xfff,
		readl(OVL_BLD_DBG_STATUS0 + baddr) & 0x1fff,
		(readl(OVL_BLD_DBG_STATUS0 + baddr) >> 16) & 0x1fff,
		readl(OVL_BLD_DBG_STATUS1 + baddr),
		readl(OVL_BLD_DBG_STATUS3 + baddr) & 0x1fff,
		(readl(OVL_BLD_DBG_STATUS3 + baddr) >> 16) & 0x1fff);

	if (!(ovl_en & 0x1))
		return 0;

	/* 0: phy layer, 1~3: ext layer */
	for (i = 0; i < 4; i++) {
		if (layer_en[i] & 0x1)
			ovl_dump_bld_layer_info(comp, i);
		else {
			if(i)
				DDPDUMP("ext_L%d:disabled\n", i-1);
			else
				DDPDUMP("phy_L%d:disabled\n", i);
		}
	}

	ovl_blender_printf_status(readl(OVL_BLD_DBG_STATUS5 + baddr));

	return 0;
}

static void _store_bg_roi(struct mtk_ddp_comp *comp, int h, int w)
{
	struct mtk_disp_ovl_blender *ovl = comp_to_ovl_blender(comp);

	ovl->bg_h = h;
	ovl->bg_w = w;
}

static void _get_bg_roi(struct mtk_ddp_comp *comp, int *h, int *w)
{
	struct mtk_disp_ovl_blender *ovl = comp_to_ovl_blender(comp);

	if (ovl->set_partial_update != 1)
		*h = ovl->bg_h;
	else
		*h = ovl->roi_height;

	*w = ovl->bg_w;
}

bool mtk_ovl_blender_keep_layer(struct mtk_ddp_comp *comp, int keep_first_layer, bool dual_layer)
{
	int ret = -1;

	if (keep_first_layer && (comp->id == DDP_COMPONENT_OVL0_BLENDER0 ||
			comp->id == DDP_COMPONENT_OVL0_BLENDER1 ||
			(dual_layer && comp->id == DDP_COMPONENT_OVL0_BLENDER2)))
		ret = 0;

	return (!ret) ? true : false;
}

static void mtk_ovl_blender_all_layer_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int keep_first_layer)
{
	int i = 0;
	DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	int next = 0, prev = 0;
	struct mtk_drm_crtc *mtk_crtc = (comp->mtk_crtc) ? comp->mtk_crtc : NULL;
	bool dual_layer = (mtk_crtc) ? mtk_crtc->path_data->is_exdma_dual_layer : false;

	if (mtk_ovl_blender_keep_layer(comp, keep_first_layer, dual_layer)) {
		if (mtk_crtc && mtk_crtc->path_data &&
			mtk_crtc->path_data->is_exdma_dual_layer &&
			!comp->mtk_crtc->last_blender) {
			next = comp->id + 1;
		} else if (mtk_crtc && mtk_crtc->path_data &&
			mtk_crtc->path_data->is_exdma_dual_layer &&
			comp->mtk_crtc->last_blender)
			prev = comp->mtk_crtc->last_blender->id;

		DDPINFO("%s+ %s not off, mtk_crtc[%p] dual_layer[%d] prev[%d %s] next[%d %s]\n",
			__func__, mtk_dump_comp_str(comp), comp->mtk_crtc,
			comp->mtk_crtc->path_data->is_exdma_dual_layer, next,
			mtk_dump_comp_str_id(next), prev, mtk_dump_comp_str_id(prev));

		if (comp->mtk_crtc->last_blender && comp->mtk_crtc->last_blender->id)
			comp->mtk_crtc->last_blender = comp;

		mtk_drm_crtc_blender_ovl_path(comp->mtk_crtc, comp, handle, false);
		mtk_ovl_blender_connect(comp, handle, prev, next);
		return;
	}

	/**
	 * cmdq_pkt_write(handle, comp->cmdq_base,
	 *		   comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON, 0, ~0);
	 */

	for (i = 0; i < OVL_PHY_LAYER_NR; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
					   comp->regs_pa + DISP_REG_OVL_BLD_L_EN(i), 0, ~0);
}

static void mtk_ovl_blender_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_blender *ovl = comp_to_ovl_blender(comp);
	unsigned int width = 0, height = 0;

	width = cfg->w;

	if (ovl->set_partial_update != 1)
		height = cfg->h;
	else
		height = ovl->roi_height;

	if (cfg->w != 0 && cfg->h != 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_OVL_BLD_ROI_SIZE,
				   height << 16 | width, ~0);
		_store_bg_roi(comp, height, width);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + OVL_BLD_BGCLR_CLG, OVL_ROI_BGCLR,
		       ~0);
}

static void mtk_ovl_blender_config_begin(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle, const u32 idx)
{
	DDPINFO("%s,%s\n", __func__, mtk_dump_comp_str_id(comp->id));

#ifdef IF_ZERO
	u32 value = 0;	//0x10
	u32 mask = DISP_BGCLR_IN_SEL | DISP_BGCLR_OUT_TO_PROC | DISP_BGCLR_OUT_TO_NEXT_LAYER;
	struct mtk_crtc_state *new_mtk_state = NULL;

	if (!comp->mtk_crtc)
		return;

	/* no need connect to OVL PQ_LOOP or PQ_OUT path for external display so far */
	if (comp->mtk_crtc->base.index != 0)
		return;

	new_mtk_state = to_mtk_crtc_state(comp->mtk_crtc->base.state);
	if ((new_mtk_state->prop_val[CRTC_PROP_USER_SCEN] & USER_SCEN_BLANK) || (idx == 999)) {
		if (comp->id > g_last_active_bld)
			return;
		else if (comp->id == DDP_COMPONENT_OVL0_BLENDER1)
			value = DISP_BGCLR_OUT_TO_NEXT_LAYER;
		else if (comp->id == g_last_active_bld)
			value = DISP_BGCLR_IN_SEL | DISP_BGCLR_OUT_TO_PROC;
		else
			value = DISP_BGCLR_IN_SEL | DISP_BGCLR_OUT_TO_NEXT_LAYER;


		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			       DISP_REG_OVL_BLD_DATAPATH_CON, value, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			       DISP_REG_OVL_BLD_EN,
			       DISP_OVL_FORCE_RELAY_MODE | DISP_OVL_RELAY_MODE | DISP_OVL_EN, ~0);
		DDPINFO("SR blender(%d,%d) %#x\n", comp->id, g_last_active_bld, value);
		return;
	}

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON,
			value, mask);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_BLD_L_EN(0),
			value, DISP_OVL_L_EN);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_BLD_EN,
			DISP_OVL_EN, ~0);
	DDPINFO("BLD_DATAPATH_CON(%s) 0x%x,0x%x\n", mtk_dump_comp_str(comp), value, mask);
#endif
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_BLD_EN,
		DISP_OVL_EN, ~0);
}

static void mtk_ovl_blender_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	mtk_ovl_blender_io_cmd(comp, handle, IRQ_LEVEL_NORMAL, NULL);

	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_BLD_RST, BIT(0), BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_BLD_RST, 0x0, BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_BLD_INTSTA, 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_BLD_EN,
		       DISP_OVL_EN, DISP_OVL_EN);

	/**
	 * cmdq_pkt_write(handle, comp->cmdq_base,
	 *	       comp->regs_pa + DISP_REG_OVL_BLD_EN,
	 *	       DISP_OVL_FORCE_RELAY_MODE, DISP_OVL_FORCE_RELAY_MODE);
	 */

	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_blender_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_BLD_INTEN, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_BLD_EN,
		       0x0, DISP_OVL_EN);

	mtk_ovl_blender_all_layer_off(comp, handle, 0);

	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_blender_reset(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_BLD_RST, BIT(0), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_BLD_RST, 0, ~0);
	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_blender_layer_on(struct mtk_ddp_comp *comp, unsigned int idx,
			     unsigned int ext_idx, struct cmdq_pkt *handle)
{
	if (ext_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + DISP_REG_OVL_BLD_L_EN(ext_idx), DISP_OVL_L_EN, DISP_OVL_L_EN);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + DISP_REG_OVL_BLD_L_EN(idx), DISP_OVL_L_EN, DISP_OVL_L_EN);
}

static void mtk_ovl_blender_layer_off(struct mtk_ddp_comp *comp, unsigned int idx,
			      unsigned int ext_idx, struct cmdq_pkt *handle)
{
	if (unlikely(!comp->mtk_crtc)) {
		DDPMSG("[E] %s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	if (ext_idx != LYE_NORMAL) {
		DDPINFO("REMOVE EXT BLENDER %d\n", comp->id);
		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + DISP_REG_OVL_BLD_L_EN(ext_idx), 0x0, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + DISP_REG_OVL_BLD_L_EN(idx), 0x0, ~0);

		if (comp->blender_hold) {
			comp->blender_hold = false;
			return;
		}

		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON, 0, ~0);

		mtk_drm_crtc_blender_ovl_path(comp->mtk_crtc, comp, handle, true);
	}
}

static void mtk_ovl_blender_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl_blender *priv = dev_get_drvdata(comp->dev);
	int ret;
	struct mtk_disp_ovl_blender *ovl = comp_to_ovl_blender(comp);

	DDPDBG("%s %s\n", __func__, mtk_dump_comp_str(comp));

	mtk_ddp_comp_clk_prepare(comp);

	if (priv->fbdc_clk != NULL) {
		ret = clk_prepare_enable(priv->fbdc_clk);
		if (ret)
			DDPPR_ERR("clk prepare enable failed:%s\n",
				mtk_dump_comp_str(comp));
	}

	/* Bypass shadow register and read shadow register */
	if (ovl->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, DISP_OVL_BYPASS_SHADOW,
			DISP_REG_OVL_BLD_SHADOW_CTRL, DISP_OVL_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_REG_OVL_BLD_SHADOW_CTRL, DISP_OVL_BYPASS_SHADOW);
}

static void mtk_ovl_blender_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl_blender *priv = dev_get_drvdata(comp->dev);

	if (priv->fbdc_clk != NULL)
		clk_disable_unprepare(priv->fbdc_clk);

	mtk_ddp_comp_clk_unprepare(comp);
}

static int mtk_ovl_blender_first_layer_mt6991(struct mtk_ddp_comp *comp)
{
	struct mtk_ddp_comp *first_blender = comp->mtk_crtc->first_blender;

	DDPDBG("%s %s\n", __func__, mtk_dump_comp_str(comp));

	if (first_blender && ((first_blender->id == comp->id) ||
		(first_blender->id == comp->id+1 &&
		comp->mtk_crtc->path_data->is_exdma_dual_layer)))
		return 1;
	else
		return 0;
}

static void mtk_ovl_blender_connect(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_id prev,
			    enum mtk_ddp_comp_id next)
{
	int crtc_first_layer = 0;

	if (comp->funcs->first_layer)
		crtc_first_layer = comp->funcs->first_layer(comp);


	if (handle == NULL)
		writel_relaxed(0, comp->regs + DISP_REG_OVL_BLD_DATAPATH_CON);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON, 0, ~0);

	if (prev == 0 || crtc_first_layer) {
		if (handle == NULL)
			mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_BLD_DATAPATH_CON,
					   0,
					   DISP_BGCLR_IN_SEL);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
					   comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON,
					   0,
					   DISP_BGCLR_IN_SEL);
	} else {
		if (handle == NULL)
			mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_BLD_DATAPATH_CON,
					       DISP_BGCLR_IN_SEL,
					       DISP_BGCLR_IN_SEL);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON,
				       DISP_BGCLR_IN_SEL,
				       DISP_BGCLR_IN_SEL);
	}

	if (comp->mtk_crtc->last_blender != comp ||
			((mtk_ddp_comp_get_type(next) == MTK_OVL_BLENDER ||
			mtk_ddp_comp_get_type(next) == MTK_OVL_EXDMA) &&
			next != DDP_COMPONENT_ID_MAX)) {
		if (handle == NULL)
			mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_BLD_DATAPATH_CON,
					   DISP_BGCLR_OUT_TO_NEXT_LAYER,
					   DISP_BGCLR_OUT_TO_NEXT_LAYER);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
					   comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON,
					   DISP_BGCLR_OUT_TO_NEXT_LAYER,
					   DISP_BGCLR_OUT_TO_NEXT_LAYER);
	} else {
		if (handle == NULL)
			mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_BLD_DATAPATH_CON,
				       DISP_BGCLR_OUT_TO_PROC,
				       DISP_BGCLR_OUT_TO_PROC);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DISP_REG_OVL_BLD_DATAPATH_CON,
				       DISP_BGCLR_OUT_TO_PROC,
				       DISP_BGCLR_OUT_TO_PROC);
	}

}

static int mtk_ovl_blender_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;
	struct mtk_drm_private *priv;

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPINFO("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), io_cmd);
		return -INVALID;
	}

	priv = comp->mtk_crtc->base.dev->dev_private;
	switch (io_cmd) {
	case OVL_ALL_LAYER_OFF:
	{
		int *keep_first_layer = params;

		mtk_ovl_blender_all_layer_off(comp, handle, *keep_first_layer);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = REG_FLD_VAL(REG_UPDATE, 1) |
				REG_FLD_VAL(FRAME_DONE, 1) |
				REG_FLD_VAL(ABNORMAL_SOF, 1) |
				REG_FLD_VAL(OVL_START_INTEN, 1);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_BLD_INTSTA, 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_BLD_INTEN, inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_NORMAL: {
		unsigned int inten;

		//inten = REG_FLD_VAL(FRAME_DONE, 1) |
		//		REG_FLD_VAL(ABNORMAL_SOF, 1) |
		//		REG_FLD_VAL(OVL_START_INTEN, 1);
		inten = 0; /* remove me after irq handling done */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_BLD_INTSTA, 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_BLD_INTEN, inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = REG_FLD_VAL(FRAME_DONE, 1) |
				REG_FLD_VAL(ABNORMAL_SOF, 1) |
				REG_FLD_VAL(OVL_START_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_BLD_INTEN, 0, inten);
		break;
	}
	case OVL_REPLACE_BOOTUP_MVA: {
		#ifdef IF_ZERO
		struct mtk_ddp_fb_info *fb_info =
			(struct mtk_ddp_fb_info *)params;

		mtk_ovl_replace_bootup_mva(comp, handle, params, fb_info);
		if (priv->data->mmsys_id == MMSYS_MT6989)
			iommu_dev_disable_feature(comp->dev, IOMMU_DEV_FEAT_BYPASS_S1);
		#endif
		break;
	}
	case BACKUP_INFO_CMP: {
		//mtk_ovl_backup_info_cmp(comp, params);
		break;
	}
	case BACKUP_OVL_STATUS: {
		#ifdef IF_ZERO
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		dma_addr_t slot = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_OVL_STATUS);

		cmdq_pkt_mem_move(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_STA,
			slot, CMDQ_THR_SPR_IDX3);
		#endif
		break;
	}
	case OVL_SET_PQ_OUT: {
		#ifdef IF_ZERO
		struct mtk_addon_config_type *c = (struct mtk_addon_config_type *)params;
		u32 value = 0, mask = 0;

		if (c->module == OVL_RSZ)
			SET_VAL_MASK(value, mask, 1, DISP_OVL_PQ_OUT_OPT);

		if (c->module != DISP_MML_DL)
			SET_VAL_MASK(value, mask, 1, DISP_OVL_PQ_OUT_EN);

		SET_VAL_MASK(value, mask, c->tgt_layer, DATAPATH_PQ_OUT_SEL);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			       value, mask);

		cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_OVL_PQ_LOOP_CON,
			       DISP_OVL_PQ_OUT_SIZE_SEL, DISP_OVL_PQ_OUT_SIZE_SEL);
		#endif
		break;
	}
	default:
		break;
	}

	return ret;
}

static unsigned int ovl_fmt_convert(struct mtk_disp_ovl_blender *ovl, unsigned int fmt,
				    uint64_t modifier, unsigned int compress)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return OVL_CON_CLRFMT_RGB565(ovl);
	case DRM_FORMAT_BGR565:
		return (unsigned int)OVL_CON_CLRFMT_RGB565(ovl);
	case DRM_FORMAT_RGB888:
		return OVL_CON_CLRFMT_RGB888(ovl);
	case DRM_FORMAT_BGR888:
		return (unsigned int)OVL_CON_CLRFMT_RGB888(ovl);
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 |
			       OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_Y410:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_UYVY:
		return OVL_CON_CLRFMT_UYVY(ovl);
	case DRM_FORMAT_YUYV:
		return OVL_CON_CLRFMT_YUYV(ovl);
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_ABGR16161616F:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_C8:
		return OVL_CON_CLRFMT_DIM | OVL_CON_CLRFMT_RGB888(ovl);
	}
}

/* config addr, pitch, src_size */
static void _ovl_bld_common_config(struct mtk_ddp_comp *comp, unsigned int idx,
			       struct mtk_plane_state *state,
			       struct cmdq_pkt *handle)
{
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int fmt = pending->format;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int pitch_msb = ((pending->pitch >> 16) & 0xf);
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x = pending->src_x;
	unsigned int src_y = pending->src_y;
	unsigned int lye_idx = 0, ext_lye_idx = 0 ,id = 0;
	unsigned int src_size = 0;
	unsigned int offset = 0;
	unsigned int clip = 0;
	unsigned int buf_size = 0;
	int rotate = 0;
	/* OVL comp might not attach to CRTC in layer_config(), need to check */
	if (unlikely(!comp->mtk_crtc)) {
		DDPPR_ERR("%s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;

	if (fmt == DRM_FORMAT_YUYV || fmt == DRM_FORMAT_YVYU ||
	    fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_VYUY) {
		if (src_x % 2) {
			src_x -= 1;
			dst_w += 1;
			clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT, 1);
		}
		if ((src_x + dst_w) % 2) {
			dst_w += 1;
			clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT, 1);
		}
	}

	if (pending->pq_loop_type == 2)
		src_size = pending->dst_roi;
	else
		src_size = (dst_h << 16) | dst_w;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (rotate)
		offset = (src_x + dst_w) * mtk_drm_format_plane_cpp(fmt, 0) +
			 (src_y + dst_h - 1) * pitch - 1;
	else
		offset = src_x * mtk_drm_format_plane_cpp(fmt, 0) + src_y * pitch;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	buf_size = (dst_h - 1) * pending->pitch +
		dst_w * mtk_drm_format_plane_cpp(fmt, 0);


	if (ext_lye_idx != LYE_NORMAL)
		id = ext_lye_idx;
	else
		id = lye_idx;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + OVL_BLD_L_PITCH(id), pitch_msb, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + OVL_BLD_L_PITCH(id), pitch, ~0);

	DDPDBG("%s, %d\n", __func__, comp->id);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_BLD_OVL_SRC_SIZE(id), src_size, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_BLD_OVL_CLIP(id), clip, ~0);
}

static void mtk_ovl_blender_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_blender *ovl = comp_to_ovl_blender(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	int rotate = 0;
	unsigned int fmt = pending->format;
	unsigned int offset;
	unsigned int Ln_CLRFMT = 0, con = 0;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int alpha;
	unsigned int alpha_con = 1;
	unsigned int dim_color;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int frame_idx;
	struct mtk_drm_private *priv;
	unsigned long crtc_idx;
	unsigned int disp_reg_ovl_pitch = 0;
	unsigned int pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	unsigned int modifier = 0;

	/* OVL comp might not attach to CRTC in layer_config(), need to check */
	if (unlikely(!comp->mtk_crtc)) {
		DDPPR_ERR("%s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	frame_idx = mtk_crtc_state->prop_val[CRTC_PROP_OVL_DSI_SEQ];
	priv = crtc->dev->dev_private;
	crtc_idx = (unsigned long)drm_crtc_index(crtc);

	/* handle dim layer for compression flag & color dim*/
	if (fmt == DRM_FORMAT_C8) {
		pending->prop_val[PLANE_PROP_COMPRESS] = 0;
		dim_color = (unsigned int)pending->prop_val[PLANE_PROP_DIM_COLOR];
	} else {
		dim_color = 0xff000000;
	}

	/* handle buffer de-compression */
	if (ovl->data->compr_info && ovl->data->compr_info->l_config) {
		if (ovl->data->compr_info->l_config(comp,
				idx, state, handle)) {
			DDPPR_ERR("wrong fbdc input config\n");
			return;
		}
	} else {
		/* Config common register which would be different according
		 * with
		 * this layer is compressed or not, i.e.: addr, pitch...
		 */
		_ovl_bld_common_config(comp, idx, state, handle);
	}

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(crtc) == 0)
		rotate = 1;
#endif

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (fmt == DRM_FORMAT_RGB332) {
		pending->enable = false;
		comp->blender_hold = true;
		DDPINFO("%s: DRM_FORMAT_RGB332 not support, so skip it\n", __func__);
	}
	if (ovl->ovl_dis == true && pending->enable == true) {
		if ((priv->data->mmsys_id == MMSYS_MT6991) &&
			mtk_crtc_is_frame_trigger_mode(crtc))
			pending->enable = false;

		DDPPR_ERR("%s, %s, idx:%d, lye_idx:%d, ext_idx:%d, en:%d\n",
			__func__, mtk_dump_comp_str_id(comp->id), idx, lye_idx,
			ext_lye_idx, pending->enable);
		ovl->ovl_dis = false;
	}

	if (!pending->enable)
		mtk_ovl_blender_layer_off(comp, lye_idx, ext_lye_idx, handle);

	alpha = 0xFF & (state->base.alpha >> 8);

	if (state->base.fb) {
		if (state->base.fb->format->has_alpha)
			pixel_blend_mode = state->base.pixel_blend_mode;
	}

	if (pixel_blend_mode == DRM_MODE_BLEND_PREMULTI)
		modifier |= MTK_FMT_PREMULTIPLIED;

	Ln_CLRFMT = ovl_fmt_convert(ovl, fmt, modifier,
			pending->prop_val[PLANE_PROP_COMPRESS]);
	con |= (alpha_con << 12) | alpha;

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_bg_roi(comp, &bg_h, &bg_w);
		offset = ((bg_h - pending->height - pending->dst_y) << 16) +
			 (bg_w - pending->width - pending->dst_x);
	} else {
		offset = pending->offset;
	}

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_BLD_OVL_OFFSET(ext_lye_idx),
			       offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_BLD_L_CLR(ext_lye_idx),
			       dim_color, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_BLD_L_CON2(ext_lye_idx),
			       con, ~0);

		disp_reg_ovl_pitch = OVL_BLD_L_PITCH(ext_lye_idx);

	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_BLD_OVL_OFFSET(lye_idx), offset, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_BLD_L_CLR(lye_idx),
			       dim_color, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_BLD_L_CON2(lye_idx),
			       con, ~0);

		disp_reg_ovl_pitch = OVL_BLD_L_PITCH(lye_idx);
	}

#define _LAYER_CONFIG_FMT \
	"%s %s idx:%d lye_idx:%d ext_idx:%d en:%d fmt:0x%x " \
	"addr:0x%lx compr:%d con:0x%x offset:0x%x lye_cap:%x mml:%d\n"
	DDPINFO(_LAYER_CONFIG_FMT, __func__,
		mtk_dump_comp_str_id(comp->id), idx, lye_idx, ext_lye_idx,
		pending->enable, pending->format, (unsigned long)pending->addr,
		(unsigned int)pending->prop_val[PLANE_PROP_COMPRESS], con, offset,
		state->comp_state.layer_caps & (MTK_DISP_RSZ_LAYER | DISP_MML_CAPS_MASK),
		pending->mml_mode);

	if (pixel_blend_mode == DRM_MODE_BLEND_PIXEL_NONE)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + disp_reg_ovl_pitch,
			L0_CONST_BLD,
			L0_CONST_BLD);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + disp_reg_ovl_pitch,
			0,
			L0_CONST_BLD);

	if (pending->enable) {
		u32 vrefresh;
		u32 ratio_tmp = 0;
		unsigned int vact = 0;
		unsigned int vtotal = 0;
		struct mtk_ddp_comp *output_comp;
		struct drm_display_mode *mode = NULL;

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
			vrefresh =
				mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
		else
			vrefresh = drm_mode_vrefresh(&crtc->state->adjusted_mode);

		if (output_comp &&
		    ((output_comp->id == DDP_COMPONENT_DSI0) ||
		     (output_comp->id == DDP_COMPONENT_DSI1) ||
		     (output_comp->id == DDP_COMPONENT_DSI2)) &&
		     !(mtk_dsi_is_cmd_mode(output_comp))) {
			mtk_ddp_comp_io_cmd(output_comp, NULL,
				DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
			vtotal = mode->vtotal;
			vact = mode->vdisplay;
			ratio_tmp = vtotal * 100 / vact;
		} else
			ratio_tmp = 125;

		if (fmt != DRM_FORMAT_C8)
			mtk_ovl_blender_layer_on(comp, lye_idx, ext_lye_idx, handle);
	}
}

static int mtk_ovl_blender_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_ovl_blender *ovl = comp_to_ovl_blender(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, false);
	struct total_tile_overhead_v to_v_info;
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	ovl->set_partial_update = enable;

	to_v_info = mtk_crtc_get_total_overhead_v(comp->mtk_crtc);
	overhead_v = to_v_info.overhead_v;

	if (comp->mtk_crtc->res_switch == RES_SWITCH_ON_AP &&
		comp->mtk_crtc->scaling_ctx.scaling_en)
		ovl->roi_height = to_v_info.in_height;
	else
		ovl->roi_height = partial_roi.height + (overhead_v * 2);

	DDPDBG("%s, %s overhead_v:%d, roi_height:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v, ovl->roi_height);

	if (ovl->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_BLD_ROI_SIZE,
				ovl->roi_height << 16, 0x1fff << 16);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_BLD_ROI_SIZE,
				full_height << 16, 0x1fff << 16);
	}

	return 0;
}


static const struct mtk_ddp_comp_funcs mtk_disp_ovl_blender_funcs = {
	.config = mtk_ovl_blender_config,
	.first_cfg = mtk_ovl_blender_config,
	.config_begin = mtk_ovl_blender_config_begin,
	.start = mtk_ovl_blender_start,
	.stop = mtk_ovl_blender_stop,
	.reset = mtk_ovl_blender_reset,
	.layer_on = mtk_ovl_blender_layer_on,
	.layer_off = mtk_ovl_blender_layer_off,
	.layer_config = mtk_ovl_blender_layer_config,
//	.addon_config = mtk_ovl_exdma_addon_config,
	.io_cmd = mtk_ovl_blender_io_cmd,
	.prepare = mtk_ovl_blender_prepare,
	.unprepare = mtk_ovl_blender_unprepare,
	.connect = mtk_ovl_blender_connect,
	.first_layer = mtk_ovl_blender_first_layer_mt6991,
//	.config_trigger = mtk_ovl_blender_config_trigger,
	.partial_update = mtk_ovl_blender_set_partial_update,
};

static int mtk_disp_ovl_blender_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl_blender *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_ovl_blender_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl_blender *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_blender_component_ops = {
	.bind = mtk_disp_ovl_blender_bind, .unbind = mtk_disp_ovl_blender_unbind,
};

static int mtk_disp_ovl_blender_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl_blender *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret, len;
	const __be32 *ranges = NULL;

	DDPDBG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;


	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_OVL_BLENDER);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_blender_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}
	priv->fbdc_clk = of_clk_get(dev->of_node, 1);
	if (IS_ERR(priv->fbdc_clk))
		priv->fbdc_clk = NULL;

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges && priv->data && priv->data->is_support_34bits)
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	writel(0, priv->ddp_comp.regs + DISP_REG_OVL_BLD_INTSTA);
	writel(0, priv->ddp_comp.regs + DISP_REG_OVL_BLD_INTEN);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_ovl_blender_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPDBG("%s-\n", __func__);
	return ret;
}

static int mtk_disp_ovl_blender_remove(struct platform_device *pdev)
{
	struct mtk_disp_ovl_blender *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_ovl_blender_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_ovl_blender_data mt6991_ovl_bldner_driver_data = {
	.el_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U,
	.fmt_yuyv = 5U,
	//.compr_info = &compr_info_mt6989,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.greq_num_dl = 0xFFFF,
//	.is_support_34bits = true,
//	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6991,
//	.aid_per_layer_setting = true,
//	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6991,
//	.source_bpc = 10,
//	.ovlsys_mapping = &mtk_ovl_sys_mapping_MT6991,
};


static const struct of_device_id mtk_disp_ovl_blender_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-ovl-blender",
	 .data = &mt6991_ovl_bldner_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_blender_driver_dt_match);

struct platform_driver mtk_ovl_blender_driver = {
	.probe = mtk_disp_ovl_blender_probe,
	.remove = mtk_disp_ovl_blender_remove,
	.driver = {

			.name = "mediatek-disp-ovl-blender",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ovl_blender_driver_dt_match,
		},
};

