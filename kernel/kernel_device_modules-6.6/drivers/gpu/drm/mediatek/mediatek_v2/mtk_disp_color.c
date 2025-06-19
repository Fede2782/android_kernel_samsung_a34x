// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_color.h"
#include "mtk_dump.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_debug.h"

#define UNUSED(expr) (void)(expr)

#define SG1 0
#define SG2 1
#define SG3 2
#define SP1 3
#define SP2 4

#define PURP_TONE_START    0
#define PURP_TONE_END      2
#define SKIN_TONE_START    3
#define SKIN_TONE_END     10
#define GRASS_TONE_START  11
#define GRASS_TONE_END    16
#define SKY_TONE_START    17
#define SKY_TONE_END      19

/* Register */
#define DISP_COLOR_CFG_MAIN		0x400
#define DISP_COLOR_WIN_X_MAIN		0x40c
#define DISP_COLOR_WIN_Y_MAIN		0x410
#define DISP_COLOR_DBG_CFG_MAIN		0x420
#define DISP_COLOR_C_BOOST_MAIN		0x428
#define DISP_COLOR_C_BOOST_MAIN_2	0x42C
#define DISP_COLOR_LUMA_ADJ		0x430
#define DISP_COLOR_G_PIC_ADJ_MAIN_1	0x434
#define DISP_COLOR_G_PIC_ADJ_MAIN_2	0x438
#define DISP_COLOR_POS_MAIN		0x484
#define DISP_COLOR_CAP_IN_DATA_MAIN	0x490
#define DISP_COLOR_CAP_IN_DATA_MAIN_CR	0x494
#define DISP_COLOR_CAP_OUT_DATA_MAIN	0x498
#define DISP_COLOR_CAP_OUT_DATA_MAIN_CR 0x49C
#define DISP_COLOR_Y_SLOPE_1_0_MAIN	0x4A0
#define DISP_COLOR_LOCAL_HUE_CD_0	0x620
#define DISP_COLOR_TWO_D_WINDOW_1	0x740
#define DISP_COLOR_TWO_D_W1_RESULT	0x74C
#define DISP_COLOR_PART_SAT_GAIN1_0	0x7FC
#define DISP_COLOR_PART_SAT_GAIN1_1	0x800
#define DISP_COLOR_PART_SAT_GAIN1_2	0x804
#define DISP_COLOR_PART_SAT_GAIN1_3	0x808
#define DISP_COLOR_PART_SAT_GAIN1_4	0x80C
#define DISP_COLOR_PART_SAT_GAIN2_0	0x810
#define DISP_COLOR_PART_SAT_GAIN2_1	0x814
#define DISP_COLOR_PART_SAT_GAIN2_2	0x818
#define DISP_COLOR_PART_SAT_GAIN2_3	0x81C
#define DISP_COLOR_PART_SAT_GAIN2_4	0x820
#define DISP_COLOR_PART_SAT_GAIN3_0	0x824
#define DISP_COLOR_PART_SAT_GAIN3_1	0x828
#define DISP_COLOR_PART_SAT_GAIN3_2	0x82C
#define DISP_COLOR_PART_SAT_GAIN3_3	0x830
#define DISP_COLOR_PART_SAT_GAIN3_4	0x834
#define DISP_COLOR_PART_SAT_POINT1_0	0x838
#define DISP_COLOR_PART_SAT_POINT1_1	0x83C
#define DISP_COLOR_PART_SAT_POINT1_2	0x840
#define DISP_COLOR_PART_SAT_POINT1_3	0x844
#define DISP_COLOR_PART_SAT_POINT1_4	0x848
#define DISP_COLOR_PART_SAT_POINT2_0	0x84C
#define DISP_COLOR_PART_SAT_POINT2_1	0x850
#define DISP_COLOR_PART_SAT_POINT2_2	0x854
#define DISP_COLOR_PART_SAT_POINT2_3	0x858
#define DISP_COLOR_PART_SAT_POINT2_4	0x85C
#define DISP_COLOR_CM_CONTROL		0x860
#define DISP_COLOR_CM_W1_HUE_0		0x864
#define DISP_COLOR_CM_W1_HUE_1		0x868
#define DISP_COLOR_CM_W1_HUE_2		0x86C
#define DISP_COLOR_CM_W1_HUE_3		0x870
#define DISP_COLOR_CM_W1_HUE_4		0x874
#define DISP_COLOR_S_GAIN_BY_Y0_0	0xCF4
#define DISP_COLOR_LSP_1		0xD58
#define DISP_COLOR_LSP_2		0xD5C
#define DISP_COLOR_START_REG		0xC00
#define DISP_COLOR_SHADOW_CTRL		0xCB0

#define DISP_COLOR_START(module)	((module)->data->color_offset)
#define DISP_COLOR_INTEN(reg)		(DISP_COLOR_START(reg) + 0x4UL)
#define DISP_COLOR_OUT_SEL(reg)		(DISP_COLOR_START(reg) + 0xCUL)
#define DISP_COLOR_WIDTH(reg)		(DISP_COLOR_START(reg) + 0x50UL)
#define DISP_COLOR_HEIGHT(reg)		(DISP_COLOR_START(reg) + 0x54UL)
#define DISP_COLOR_CM1_EN(reg)		(DISP_COLOR_START(reg) + 0x60UL)
#define DISP_COLOR_CM2_EN(reg)		(DISP_COLOR_START(reg) + 0xA0UL)

#define COLOR_BYPASS_SHADOW		BIT(0)
#define COLOR_READ_WRK_REG		BIT(2)
#define COLOR_BYPASS_ALL		BIT(7)
#define COLOR_SEQ_SEL			BIT(13)
#define FLD_ALLBP			REG_FLD_MSB_LSB(7, 7)
#define FLD_WIDE_GAMUT_EN		REG_FLD_MSB_LSB(8, 8)
#define FLD_S_GAIN_BY_Y_EN		REG_FLD_MSB_LSB(15, 15)
#define FLD_LSP_EN			REG_FLD_MSB_LSB(20, 20)
#define FLD_LSP_SAT_LIMIT		REG_FLD_MSB_LSB(21, 21)

enum WINDOW_SETTING {
	WIN1 = 0,
	WIN2,
	WIN3,
	WIN_TOTAL
};

enum LUT_YHS {
	LUT_H = 0,
	LUT_Y,
	LUT_S,
	LUT_TOTAL
};

enum LUT_REG {
	REG_SLOPE0 = 0,
	REG_SLOPE1,
	REG_SLOPE2,
	REG_SLOPE3,
	REG_SLOPE4,
	REG_SLOPE5,
	REG_WGT_LSLOPE,
	REG_WGT_USLOPE,
	REG_L,
	REG_POINT0,
	REG_POINT1,
	REG_POINT2,
	REG_POINT3,
	REG_POINT4,
	REG_U,
	LUT_REG_TOTAL
};

enum DISP_COLOR_USER_CMD {
	WRITE_REG = 0,
	PQ_SET_WINDOW,
};

/* initialize index */
struct DISPLAY_PQ_T color_index_init = {
LSP :
	{0x0, 0x0, 0x7F, 0x7F, 0x7F, 0x0, 0x7F, 0x7F},
};

static inline struct mtk_disp_color *comp_to_color(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_color, ddp_comp);
}

static void disp_color_cal_split_window(struct mtk_ddp_comp *comp,
	unsigned int *p_split_window_x, unsigned int *p_split_window_y)
{
	unsigned int split_window_x = 0xFFFF0000;
	unsigned int split_window_y = 0xFFFF0000;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary =
		color->primary_data;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);

	/* save to global, can be applied on following PQ param updating. */
	if (comp->mtk_crtc->is_dual_pipe) {
		if (color->color_dst_w == 0 || color->color_dst_h == 0) {
			DDPINFO("color_dst_w/h not init, return default settings\n");
		} else if (primary->split_en) {
			/* TODO: CONFIG_MTK_LCM_PHYSICAL_ROTATION other case */
			if (!color->is_right_pipe) {
				if (primary->split_window_x_start > color->color_dst_w)
					primary->split_en = 0;
				if (primary->split_window_x_start <= color->color_dst_w) {
					if (primary->split_window_x_end >= color->color_dst_w)
						split_window_x = (color->color_dst_w << 16) |
							primary->split_window_x_start;
					else
						split_window_x =
							(primary->split_window_x_end << 16) |
							primary->split_window_x_start;
					split_window_y = (primary->split_window_y_end << 16) |
						primary->split_window_y_start;
				}
			} else {
				if (primary->split_window_x_start > color->color_dst_w) {
					split_window_x =
					    ((primary->split_window_x_end - color->color_dst_w)
					     << 16) |
					    (primary->split_window_x_start - color->color_dst_w);
				} else if (primary->split_window_x_start <= color->color_dst_w &&
						primary->split_window_x_end > color->color_dst_w){
					split_window_x = ((primary->split_window_x_end -
								color->color_dst_w) << 16) | 0;
				}
				split_window_y =
				    (primary->split_window_y_end << 16) |
				    primary->split_window_y_start;

				if (primary->split_window_x_end <= color->color_dst_w)
					primary->split_en = 0;
			}
		}
	} else if (color->color_dst_w == 0 || color->color_dst_h == 0) {
		DDPINFO("g_color0_dst_w/h not init, return default settings\n");
	} else if (primary->split_en) {
		/* TODO: CONFIG_MTK_LCM_PHYSICAL_ROTATION other case */
		if (params && params->rotate == MTK_PANEL_ROTATE_180) {
			split_window_x =
				((color->color_dst_w - primary->split_window_x_start) << 16) |
				(color->color_dst_w - primary->split_window_x_end);
			split_window_y =
				((color->color_dst_h - primary->split_window_y_start) << 16) |
				(color->color_dst_h - primary->split_window_y_end);
		} else {
			split_window_y = (primary->split_window_y_end << 16) |
				primary->split_window_y_start;
			split_window_x = (primary->split_window_x_end << 16) |
				primary->split_window_x_start;
		}
	}

	*p_split_window_x = split_window_x;
	*p_split_window_y = split_window_y;
}

unsigned long disp_color_get_reg_offset(const char *reg_name)
{
	unsigned long reg_offset = 0x0;

	if (!strcmp(reg_name, "disp_color_two_d_w1_result"))
		reg_offset = DISP_COLOR_TWO_D_W1_RESULT;
	else if (!strcmp(reg_name, "disp_color_pos_main"))
		reg_offset = DISP_COLOR_POS_MAIN;
	else
		DDPPR_ERR("%s: reg_name(%s) error\n", __func__, reg_name);

	pr_notice("%s: reg_name(%s), reg_offset: 0x%lx\n", __func__, reg_name, reg_offset);

	return reg_offset;
}

bool disp_color_reg_get(struct mtk_ddp_comp *comp,
	const char *reg_name, int *value)
{
	if (!strcmp(reg_name, "disp_color_two_d_w1_result"))
		*value = readl(comp->regs + DISP_COLOR_TWO_D_W1_RESULT);
	else
		DDPPR_ERR("%s: reg_name(%s) error\n", __func__, reg_name);

	return true;
}

static void disp_color_set_window(struct mtk_ddp_comp *comp,
	struct DISP_PQ_WIN_PARAM *win_param, struct cmdq_pkt *handle)
{
	unsigned int split_window_x, split_window_y;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		primary_data->split_en = 1;
		primary_data->split_window_x_start = win_param->start_x;
		primary_data->split_window_y_start = win_param->start_y;
		primary_data->split_window_x_end = win_param->end_x;
		primary_data->split_window_y_end = win_param->end_y;
	} else {
		primary_data->split_en = 0;
		primary_data->split_window_x_start = 0x0000;
		primary_data->split_window_y_start = 0x0000;
		primary_data->split_window_x_end = 0xFFFF;
		primary_data->split_window_y_end = 0xFFFF;
	}

	DDPINFO("%s: input: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary_data->split_en,
		((win_param->end_x << 16) | win_param->start_x),
		((win_param->end_y << 16) | win_param->start_y));

	disp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: current window setting: en[%d], x[0x%x], y[0x%x]",
		__func__,
		(readl(comp->regs+DISP_COLOR_DBG_CFG_MAIN)&0x00000008)>>3,
		readl(comp->regs+DISP_COLOR_WIN_X_MAIN),
		readl(comp->regs+DISP_COLOR_WIN_Y_MAIN));

	DDPINFO("%s: output: x[0x%x], y[0x%x]",
		__func__, split_window_x, split_window_y);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_DBG_CFG_MAIN,
		(primary_data->split_en << 3), 0x00000008);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_X_MAIN, split_window_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_Y_MAIN, split_window_y, ~0);
}

void disp_color_on_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int split_window_x, split_window_y;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	disp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary_data->split_en, split_window_x, split_window_y);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_DBG_CFG_MAIN,
		(primary_data->split_en << 3), 0x00000008);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_X_MAIN, split_window_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_Y_MAIN, split_window_y, ~0);
}

static void disp_color_write_drecolor_hw_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_AAL_DRECOLOR_REG *param)
{
	int i, cnt = DRECOLOR_SGY_Y_ENTRY * DRECOLOR_SGY_HUE_NUM / 4;
	uint32_t value = 0, mask = 0;
	unsigned int *sgy_data = (unsigned int *)param->sgy_out_gain;

	SET_VAL_MASK(value, mask, param->sgy_en, FLD_S_GAIN_BY_Y_EN);
	SET_VAL_MASK(value, mask, param->lsp_en, FLD_LSP_EN);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CFG_MAIN, value, mask);

	/* SGY */
	for (i = 0; i < cnt; i++) {
		value = sgy_data[4 * i] |
			(sgy_data[4 * i + 1]  << 8) |
			(sgy_data[4 * i + 2] << 16) |
			(sgy_data[4 * i + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_S_GAIN_BY_Y0_0 + i * 4, value, ~0);
	}

	/* LSP */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_LSP_1,
		(param->lsp_out_setting[3] << 0) |
		(param->lsp_out_setting[2] << 7) |
		(param->lsp_out_setting[1] << 14) |
		(param->lsp_out_setting[0] << 22), 0x1FFFFFFF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_LSP_2,
		(param->lsp_out_setting[7] << 0) |
		(param->lsp_out_setting[6] << 8) |
		(param->lsp_out_setting[5] << 16) |
		(param->lsp_out_setting[4] << 23), 0x3FFF7F7F);
}

static void disp_color_write_hw_reg(struct mtk_ddp_comp *comp,
	const struct DISPLAY_COLOR_REG *color_reg, struct cmdq_pkt *handle)
{
	int index = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int u4Temp = 0;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	int i, j, reg_index;
	int wide_gamut_en = 0;
	uint32_t value = 0, mask = 0;
	/* unmask s_gain_by_y lsp when drecolor enable */
	int drecolor_sel = primary_data->drecolor_param.drecolor_sel;

	DDPINFO("%s,SET COLOR REG id(%d) drecolor_sel %d\n", __func__, comp->id, drecolor_sel);

	if (color->data->support_color21 == true) {
		SET_VAL_MASK(value, mask, 1 , FLD_LSP_SAT_LIMIT);
		SET_VAL_MASK(value, mask, color_reg->LSP_EN , FLD_LSP_EN);
		SET_VAL_MASK(value, mask, color_reg->S_GAIN_BY_Y_EN, FLD_S_GAIN_BY_Y_EN);
		SET_VAL_MASK(value, mask, wide_gamut_en, FLD_WIDE_GAMUT_EN);
		mask = ~((drecolor_sel << 15) | (drecolor_sel << 20)) & mask;
	} else {
		SET_VAL_MASK(value, mask, 0, FLD_WIDE_GAMUT_EN);
		mask = mask | 0xFF;
		/* disable wide_gamut */
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CFG_MAIN,
		value, mask);

	/* color start */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x1);

	/* enable R2Y/Y2R in Color Wrapper */
	if (color->data->support_color21 == true) {
		/* RDMA & OVL will enable wide-gamut function */
		/* disable rgb clipping function in CM1 */
		/* to keep the wide-gamut range */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM1_EN(color),
			0x03, 0x03);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM1_EN(color),
			0x03, 0x03);
	}

	/* also set no rounding on Y2R */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CM2_EN(color), 0x01, 0x01);

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x40, 0x7F);
	else if (wide_gamut_en == 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x0, 0x7F);

	/* config parameter from customer color_index.h */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_1,
		(color_reg->BRIGHTNESS << 16) | color_reg->CONTRAST,
		0x07FF03FF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_2,
		color_reg->GLOBAL_SAT, 0x000003FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index,
			(color_reg->PARTIAL_Y[2 * index] |
			 color_reg->PARTIAL_Y[2 * index + 1] << 16),
			 0x00FF00FF);
	}

	if (color->data->support_color21 == false)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN,
			0 << 13, 0x00002000);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN_2,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_0,
		(color_reg->PURP_TONE_S[SG1][0] |
		color_reg->PURP_TONE_S[SG1][1] << 8 |
		color_reg->PURP_TONE_S[SG1][2] << 16 |
		color_reg->SKIN_TONE_S[SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_1,
		(color_reg->SKIN_TONE_S[SG1][1] |
		color_reg->SKIN_TONE_S[SG1][2] << 8 |
		color_reg->SKIN_TONE_S[SG1][3] << 16 |
		color_reg->SKIN_TONE_S[SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_2,
		(color_reg->SKIN_TONE_S[SG1][5] |
		color_reg->SKIN_TONE_S[SG1][6] << 8 |
		color_reg->SKIN_TONE_S[SG1][7] << 16 |
		color_reg->GRASS_TONE_S[SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_3,
		(color_reg->GRASS_TONE_S[SG1][1] |
		color_reg->GRASS_TONE_S[SG1][2] << 8 |
		color_reg->GRASS_TONE_S[SG1][3] << 16 |
		color_reg->GRASS_TONE_S[SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_4,
		(color_reg->GRASS_TONE_S[SG1][5] |
		color_reg->SKY_TONE_S[SG1][0] << 8 |
		color_reg->SKY_TONE_S[SG1][1] << 16 |
		color_reg->SKY_TONE_S[SG1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_0,
		(color_reg->PURP_TONE_S[SG2][0] |
		color_reg->PURP_TONE_S[SG2][1] << 8 |
		color_reg->PURP_TONE_S[SG2][2] << 16 |
		color_reg->SKIN_TONE_S[SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_1,
		(color_reg->SKIN_TONE_S[SG2][1] |
		color_reg->SKIN_TONE_S[SG2][2] << 8 |
		color_reg->SKIN_TONE_S[SG2][3] << 16 |
		color_reg->SKIN_TONE_S[SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_2,
		(color_reg->SKIN_TONE_S[SG2][5] |
		color_reg->SKIN_TONE_S[SG2][6] << 8 |
		color_reg->SKIN_TONE_S[SG2][7] << 16 |
		color_reg->GRASS_TONE_S[SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_3,
		(color_reg->GRASS_TONE_S[SG2][1] |
		color_reg->GRASS_TONE_S[SG2][2] << 8 |
		color_reg->GRASS_TONE_S[SG2][3] << 16 |
		color_reg->GRASS_TONE_S[SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_4,
		(color_reg->GRASS_TONE_S[SG2][5] |
		color_reg->SKY_TONE_S[SG2][0] << 8 |
		color_reg->SKY_TONE_S[SG2][1] << 16 |
		color_reg->SKY_TONE_S[SG2][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_0,
		(color_reg->PURP_TONE_S[SG3][0] |
		color_reg->PURP_TONE_S[SG3][1] << 8 |
		color_reg->PURP_TONE_S[SG3][2] << 16 |
		color_reg->SKIN_TONE_S[SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_1,
		(color_reg->SKIN_TONE_S[SG3][1] |
		color_reg->SKIN_TONE_S[SG3][2] << 8 |
		color_reg->SKIN_TONE_S[SG3][3] << 16 |
		color_reg->SKIN_TONE_S[SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_2,
		(color_reg->SKIN_TONE_S[SG3][5] |
		color_reg->SKIN_TONE_S[SG3][6] << 8 |
		color_reg->SKIN_TONE_S[SG3][7] << 16 |
		color_reg->GRASS_TONE_S[SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_3,
		(color_reg->GRASS_TONE_S[SG3][1] |
		color_reg->GRASS_TONE_S[SG3][2] << 8 |
		color_reg->GRASS_TONE_S[SG3][3] << 16 |
		color_reg->GRASS_TONE_S[SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_4,
		(color_reg->GRASS_TONE_S[SG3][5] |
		color_reg->SKY_TONE_S[SG3][0] << 8 |
		color_reg->SKY_TONE_S[SG3][1] << 16 |
		color_reg->SKY_TONE_S[SG3][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_0,
		(color_reg->PURP_TONE_S[SP1][0] |
		color_reg->PURP_TONE_S[SP1][1] << 8 |
		color_reg->PURP_TONE_S[SP1][2] << 16 |
		color_reg->SKIN_TONE_S[SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_1,
		(color_reg->SKIN_TONE_S[SP1][1] |
		color_reg->SKIN_TONE_S[SP1][2] << 8 |
		color_reg->SKIN_TONE_S[SP1][3] << 16 |
		color_reg->SKIN_TONE_S[SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_2,
		(color_reg->SKIN_TONE_S[SP1][5] |
		color_reg->SKIN_TONE_S[SP1][6] << 8 |
		color_reg->SKIN_TONE_S[SP1][7] << 16 |
		color_reg->GRASS_TONE_S[SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_3,
		(color_reg->GRASS_TONE_S[SP1][1] |
		color_reg->GRASS_TONE_S[SP1][2] << 8 |
		color_reg->GRASS_TONE_S[SP1][3] << 16 |
		color_reg->GRASS_TONE_S[SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_4,
		(color_reg->GRASS_TONE_S[SP1][5] |
		color_reg->SKY_TONE_S[SP1][0] << 8 |
		color_reg->SKY_TONE_S[SP1][1] << 16 |
		color_reg->SKY_TONE_S[SP1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_0,
		(color_reg->PURP_TONE_S[SP2][0] |
		color_reg->PURP_TONE_S[SP2][1] << 8 |
		color_reg->PURP_TONE_S[SP2][2] << 16 |
		color_reg->SKIN_TONE_S[SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_1,
		(color_reg->SKIN_TONE_S[SP2][1] |
		color_reg->SKIN_TONE_S[SP2][2] << 8 |
		color_reg->SKIN_TONE_S[SP2][3] << 16 |
		color_reg->SKIN_TONE_S[SP2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_2,
		(color_reg->SKIN_TONE_S[SP2][5] |
		color_reg->SKIN_TONE_S[SP2][6] << 8 |
		color_reg->SKIN_TONE_S[SP2][7] << 16 |
		color_reg->GRASS_TONE_S[SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_3,
		(color_reg->GRASS_TONE_S[SP2][1] |
		color_reg->GRASS_TONE_S[SP2][2] << 8 |
		color_reg->GRASS_TONE_S[SP2][3] << 16 |
		color_reg->GRASS_TONE_S[SP2][4] << 24), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_4,
		(color_reg->GRASS_TONE_S[SP2][5] |
		color_reg->SKY_TONE_S[SP2][0] << 8 |
		color_reg->SKY_TONE_S[SP2][1] << 16 |
		color_reg->SKY_TONE_S[SP2][2] << 24), ~0);

	for (index = 0; index < 3; index++) {
		h_series[index + PURP_TONE_START] =
			color_reg->PURP_TONE_H[index];
	}

	for (index = 0; index < 8; index++) {
		h_series[index + SKIN_TONE_START] =
		    color_reg->SKIN_TONE_H[index];
	}

	for (index = 0; index < 6; index++) {
		h_series[index + GRASS_TONE_START] =
			color_reg->GRASS_TONE_H[index];
	}

	for (index = 0; index < 3; index++) {
		h_series[index + SKY_TONE_START] =
		    color_reg->SKY_TONE_H[index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) +
		    (h_series[4 * index + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LOCAL_HUE_CD_0 + 4 * index,
			u4Temp, ~0);
	}

	if (color->data->support_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT && !drecolor_sel; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
				u4Temp = (color_reg->S_GAIN_BY_Y[i][j]) +
					(color_reg->S_GAIN_BY_Y[i][j + 1]
					<< 8) +
					(color_reg->S_GAIN_BY_Y[i][j + 2]
					<< 16) +
					(color_reg->S_GAIN_BY_Y[i][j + 3]
					<< 24);

				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
					DISP_COLOR_S_GAIN_BY_Y0_0 +
					reg_index,
					u4Temp, ~0);
				reg_index += 4;
			}
		}
		if (!drecolor_sel) {
			/* LSP */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_LSP_1,
				(primary_data->color_index.LSP[3] << 0) |
				(primary_data->color_index.LSP[2] << 7) |
				(primary_data->color_index.LSP[1] << 14) |
				(primary_data->color_index.LSP[0] << 22), 0x1FFFFFFF);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_LSP_2,
				(primary_data->color_index.LSP[7] << 0) |
				(primary_data->color_index.LSP[6] << 8) |
				(primary_data->color_index.LSP[5] << 16) |
				(primary_data->color_index.LSP[4] << 23), 0x3FFF7F7F);
		}
	}

	/* color window */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_TWO_D_WINDOW_1,
		color->data->color_window, ~0);

	if (color->data->support_color30 == true) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM_CONTROL,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_0 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_L] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_U] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT0] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_1 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT1] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT2] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT3] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_2 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT4] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE1] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_3 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE2] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE5] << 24),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_4 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_WGT_USLOPE] << 16),
					~0);

				reg_index += (4 * 5);
			}
		}
	}
}

static void disp_color_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	DDPINFO("line: %d\n", __LINE__);
	if (cfg->tile_overhead.is_support) {
		if (!color->is_right_pipe) {
			color->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				color->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				color->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			color->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			color->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else {
			/*set component overhead*/
			color->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				color->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				color->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			color->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			color->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void disp_color_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	color->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		color->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	color->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void disp_color_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	struct DISP_AAL_DRECOLOR_PARAM *drecolor = &primary_data->drecolor_param;
	unsigned int width;
	unsigned int overhead_v;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = color->tile_overhead.in_width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		primary_data->width = width;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		color->color_dst_w = cfg->w / 2;
	else
		color->color_dst_w = cfg->w;
	color->color_dst_h = cfg->h;

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_COLOR_WIDTH(color), width, ~0);
	if (color->set_partial_update != 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_HEIGHT(color), cfg->h, ~0);
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : color->tile_overhead_v.overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_HEIGHT(color), color->roi_height + overhead_v * 2, ~0);
	}
	// set color_8bit_switch register
	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, (0x1 << 25), (0x1 << 25));
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, (0x0 << 25), (0x1 << 25));
	else
		DDPINFO("Disp COLOR's bit is : %u\n", cfg->bpc);

	if (color->data->need_bypass_shadow)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_SHADOW_CTRL, (0x1 << 0), (0x1 << 0));
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_SHADOW_CTRL, (0x0 << 0), (0x1 << 0));

	disp_color_on_init(comp, handle);
	// config hal parameter if needed
	mutex_lock(&primary_data->data_lock);
	if (primary_data->color_reg_valid) {
		disp_color_write_hw_reg(comp, &primary_data->color_reg, handle);
		if (drecolor->drecolor_sel)
			disp_color_write_drecolor_hw_reg(comp, handle, &drecolor->drecolor_reg);
	}

	if (primary_data->relay_state != 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, COLOR_BYPASS_ALL, COLOR_BYPASS_ALL);
	mutex_unlock(&primary_data->data_lock);
}

int disp_color_act_set_pqindex(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;
	struct DISPLAY_PQ_T *pq_index = &primary_data->color_index;

	DDPINFO("%s...", __func__);

	memcpy(pq_index, (struct DISPLAY_PQ_T *)data,
		sizeof(struct DISPLAY_PQ_T));

	return ret;
}

int disp_color_cfg_set_color_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	DDPINFO("%s,SET COLOR REG id(%d)\n", __func__, comp->id);

	mutex_lock(&primary_data->data_lock);
	if (data != NULL) {
		memcpy(&primary_data->color_reg, (struct DISPLAY_COLOR_REG *)data,
			sizeof(struct DISPLAY_COLOR_REG));

		disp_color_write_hw_reg(comp, &primary_data->color_reg, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DDPINFO("%s,SET COLOR REG id(%d)\n", __func__, comp_color1->id);
			disp_color_write_hw_reg(comp_color1, &primary_data->color_reg, handle);
		}
	} else {
		ret = -EINVAL;
		DDPINFO("%s: data is NULL", __func__);
	}
	mutex_unlock(&primary_data->data_lock);

	if (!primary_data->color_reg_valid) {
		disp_color_bypass(comp, 0, PQ_FEATURE_DEFAULT, handle);
		primary_data->color_reg_valid = 1;
	}

	return ret;
}

int disp_color_act_mutex_control(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	unsigned int *value = data;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	DDPINFO("%s...value:%d", __func__, *value);

	if (*value == 1) {
		primary_data->ncs_tuning_mode = 1;
		DDPINFO("ncs_tuning_mode = 1\n");
	} else if (*value == 2) {
		primary_data->ncs_tuning_mode = 0;
		DDPINFO("ncs_tuning_mode = 0\n");
		mtk_crtc_check_trigger(mtk_crtc, true, true);
	} else {
		DDPPR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control\n");
		return -EFAULT;
	}

	return ret;
}

void disp_color_bypass(struct mtk_ddp_comp *comp, int bypass, int caller,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_color *color;
	struct mtk_disp_color_primary *primary_data;
	struct mtk_ddp_comp *companion;

	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!", __func__);
		return;
	}

	color = comp_to_color(comp);
	primary_data = color->primary_data;
	companion = color->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller, primary_data->relay_state);

	mutex_lock(&primary_data->data_lock);
	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN, COLOR_BYPASS_ALL, COLOR_BYPASS_ALL);
			if (comp->mtk_crtc->is_dual_pipe && companion)
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_COLOR_CFG_MAIN, COLOR_BYPASS_ALL, COLOR_BYPASS_ALL);
		}
		primary_data->relay_state |= (1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~(1 << caller);
			if (primary_data->relay_state == 0) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CFG_MAIN, 0, COLOR_BYPASS_ALL);
				if (comp->mtk_crtc->is_dual_pipe && companion)
					cmdq_pkt_write(handle, companion->cmdq_base,
						companion->regs_pa + DISP_COLOR_CFG_MAIN, 0, COLOR_BYPASS_ALL);
			}
		}
	}
	mutex_unlock(&primary_data->data_lock);
}

int disp_color_act_set_window(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary =
		color->primary_data;
	struct DISP_PQ_WIN_PARAM *win_param = data;

	unsigned int split_window_x, split_window_y;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		primary->split_en = 1;
		primary->split_window_x_start = win_param->start_x;
		primary->split_window_y_start = win_param->start_y;
		primary->split_window_x_end = win_param->end_x;
		primary->split_window_y_end = win_param->end_y;
	} else {
		primary->split_en = 0;
		primary->split_window_x_start = 0x0000;
		primary->split_window_y_start = 0x0000;
		primary->split_window_x_end = 0xFFFF;
		primary->split_window_y_end = 0xFFFF;
	}

	DDPINFO("%s: input: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary->split_en,
		((win_param->end_x << 16) | win_param->start_x),
		((win_param->end_y << 16) | win_param->start_y));

	disp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: output: x[0x%x], y[0x%x]", __func__,
		split_window_x, split_window_y);

	DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
		__func__, comp->id, primary->split_en,
		((primary->split_window_x_end << 16) | primary->split_window_x_start),
		((primary->split_window_y_end << 16) | primary->split_window_y_start));

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, PQ_SET_WINDOW, data);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		disp_color_cal_split_window(comp_color1, &split_window_x, &split_window_y);
		ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp_color1, PQ_SET_WINDOW, data);
		DDPINFO("%s: output: x[0x%x], y[0x%x]", __func__,
			split_window_x, split_window_y);

		DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
			__func__, comp_color1->id, primary->split_en,
			((primary->split_window_x_end << 16) | primary->split_window_x_start),
			((primary->split_window_y_end << 16) | primary->split_window_y_start));
	}
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

static void disp_color_init_primary_data(struct mtk_ddp_comp *comp)
{
	int i;
	struct mtk_disp_color *color_data = comp_to_color(comp);
	struct mtk_disp_color *companion_data = comp_to_color(color_data->companion);
	struct mtk_disp_color_primary *primary_data = color_data->primary_data;

	if (color_data->is_right_pipe) {
		kfree(color_data->primary_data);
		color_data->primary_data = companion_data->primary_data;
		return;
	}

	primary_data->color_param.u4SHPGain = 2;
	primary_data->color_param.u4SatGain = 4;
	for (i = 0; i < PQ_HUE_ADJ_PHASE_CNT; i++)
		primary_data->color_param.u4HueAdj[i] = 9;
	primary_data->color_param.u4Contrast = 4;
	primary_data->color_param.u4Brightness = 4;
	primary_data->split_window_x_end = 0xFFFF;
	primary_data->split_window_y_end = 0xFFFF;
	memcpy(&primary_data->color_index, &color_index_init,
			sizeof(struct DISPLAY_PQ_T));
	mutex_init(&primary_data->data_lock);
	primary_data->color_reg_valid = 0;
	primary_data->relay_state = 0x1 << PQ_FEATURE_DEFAULT;
}

static int disp_color_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_color *data = comp_to_color(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_color *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_color(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_color_init_primary_data(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_color_init_primary_data(data->companion);
	}
		break;
	default:
		break;
	}
	return 0;
}

static void disp_color_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	/* color start */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x1);
}

static void disp_color_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

void disp_color_write_pos_main_for_dual_pipe(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_WRITE_REG *wParams,
	unsigned int pa, unsigned int pa1)
{
	unsigned int pos_x, pos_y, val, val1, mask;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	val = wParams->val;
	mask = wParams->mask;
	pos_x = (wParams->val & 0xffff);
	pos_y = ((wParams->val & (0xffff0000)) >> 16);
	DDPINFO("write POS_MAIN: pos_x[%d] pos_y[%d]\n",
		pos_x, pos_y);
	if (pos_x < primary_data->width) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa, val, mask);
		DDPINFO("dual pipe write pa:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa, val, mask);
		val1 = ((pos_x + primary_data->width) | ((pos_y << 16)));
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa1, val1, mask);
		DDPINFO("dual pipe write pa1:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa1, val1, mask);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa, val, mask);
		DDPINFO("dual pipe write pa:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa, val, mask);
		val1 = ((pos_x - primary_data->width) | ((pos_y << 16)));
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa1, val1, mask);
		DDPINFO("dual pipe write pa1:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa1, val1, mask);
	}
}

static int disp_color_cfg_drecolor_set_param(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_color *priv_data = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = comp_to_color(comp)->primary_data;
	struct DISP_AAL_DRECOLOR_PARAM *param = data;
	struct DISP_AAL_DRECOLOR_PARAM *prev_param = &priv_data->primary_data->drecolor_param;

	if (sizeof(struct DISP_AAL_DRECOLOR_PARAM) < data_size) {
		DDPPR_ERR("%s param size error %lu, %u\n", __func__, sizeof(*param), data_size);
		return -EFAULT;
	}
	DDPINFO("%s sel %d,prev_sel %d\n", __func__, param->drecolor_sel, prev_param->drecolor_sel);
	mutex_lock(&primary_data->data_lock);
	if (!param->drecolor_sel) {
		DDPINFO("%s set skip\n", __func__);
		prev_param->drecolor_sel = param->drecolor_sel;
		mutex_unlock(&primary_data->data_lock);
		return 0;
	}
	memcpy(prev_param, param, sizeof(struct DISP_AAL_DRECOLOR_PARAM));
	disp_color_write_drecolor_hw_reg(comp, handle, &param->drecolor_reg);
	if (comp->mtk_crtc->is_dual_pipe)
		disp_color_write_drecolor_hw_reg(priv_data->companion, handle, &param->drecolor_reg);
	mutex_unlock(&primary_data->data_lock);
	return 0;
}

static int disp_color_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	DDPINFO("%s,SET COLOR REG id(%d) cmd = %d\n", __func__, comp->id, cmd);
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_COLOR_SET_COLOR_REG:
		ret = disp_color_cfg_set_color_reg(comp, handle, data, data_size);
		break;
	case PQ_COLOR_DRECOLOR_SET_PARAM:
		ret = disp_color_cfg_drecolor_set_param(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_color_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case PQ_SET_WINDOW:
	{
		struct DISP_PQ_WIN_PARAM *win_param = data;

		disp_color_set_window(comp, win_param, handle);
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void disp_color_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	primary_data->color_backup.COLOR_CFG_MAIN =
		readl(comp->regs + DISP_COLOR_CFG_MAIN);
}

static void disp_color_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	writel(primary_data->color_backup.COLOR_CFG_MAIN, comp->regs + DISP_COLOR_CFG_MAIN);
}

static void disp_color_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&color->color_is_clock_on, 1);

	/* Bypass shadow register and read shadow register */
	if (color->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, COLOR_BYPASS_SHADOW,
			DISP_COLOR_SHADOW_CTRL, COLOR_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_COLOR_SHADOW_CTRL, COLOR_BYPASS_SHADOW);
	// restore DISP_COLOR_CFG_MAIN register
	disp_color_restore(comp);
}

static void disp_color_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);
	unsigned long flags;

	DDPINFO("%s: compid: %d\n", __func__, comp->id);

	//DDPINFO("%s @ %d......... spin_lock_irqsave ++ ", __func__, __LINE__);
	//spin_lock_irqsave(&color_data->clock_lock, flags);
	//DDPINFO("%s @ %d......... spin_lock_irqsave -- ", __func__, __LINE__);
	atomic_set(&color_data->color_is_clock_on, 0);
	//spin_unlock_irqrestore(&color_data->clock_lock, flags);
	//DDPINFO("%s @ %d......... spin_unlock_irqrestore ", __func__, __LINE__);

	// backup DISP_COLOR_CFG_MAIN register
	disp_color_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

void disp_color_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	disp_color_config(comp, cfg, handle);
	disp_color_backup(comp);
}

static int disp_color_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_COLOR_SET_PQINDEX:
		ret = disp_color_act_set_pqindex(comp, data);
		break;
	case PQ_COLOR_MUTEX_CONTROL:
		ret = disp_color_act_mutex_control(comp, data);
		break;
	case PQ_COLOR_SET_WINDOW:
		ret = disp_color_act_set_window(comp, data);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_color_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	color->set_partial_update = enable;
	color->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : color->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (color->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_HEIGHT(color), color->roi_height + overhead_v * 2, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_HEIGHT(color), full_height, ~0);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_color_funcs = {
	.config = disp_color_config,
	.first_cfg = disp_color_first_cfg,
	.start = disp_color_start,
	.stop = disp_color_stop,
	.bypass = disp_color_bypass,
	.user_cmd = disp_color_user_cmd,
	.prepare = disp_color_prepare,
	.unprepare = disp_color_unprepare,
	.config_overhead = disp_color_config_overhead,
	.config_overhead_v = disp_color_config_overhead_v,
	.io_cmd = disp_color_io_cmd,
	.pq_frame_config = disp_color_frame_config,
	.pq_ioctl_transact = disp_color_ioctl_transact,
	.partial_update = disp_color_set_partial_update,
};

void disp_color_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_serial_dump_reg(baddr, 0x400, 3);
	mtk_serial_dump_reg(baddr, 0xC50, 2);
}

void disp_color_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp),
			comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp),
			color->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0x400; k <= 0xd5c; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && color->companion) {
		baddr = color->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(color->companion),
				color->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(color->companion));
		for (k = 0x400; k <= 0xd5c; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(color->companion));
	}
}

static int disp_color_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_color *priv = dev_get_drvdata(dev);
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

static void disp_color_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_color *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops disp_color_component_ops = {
	.bind	= disp_color_bind,
	.unbind = disp_color_unbind,
};

static int disp_color_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_color *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_COLOR);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_color_funcs);
	if (ret != 0) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &disp_color_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int disp_color_remove(struct platform_device *pdev)
{
	struct mtk_disp_color *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &disp_color_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

#define DISP_COLOR_START_MT2701	0x0f00
static const struct mtk_disp_color_data mt2701_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT2701,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40106051,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6761_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6765_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6768_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt8173_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40106051,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6885_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6877_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6853_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6833_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6781_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6983_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6895_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6879_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = false,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6985_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6886_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6897_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6899_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6989_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6878_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6991_color_driver_data = {
	.color_offset = DISP_COLOR_START_REG,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_color_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-color",
	 .data = &mt2701_color_driver_data},
	{.compatible = "mediatek,mt6761-disp-color",
	 .data = &mt6761_color_driver_data},
	{.compatible = "mediatek,mt6765-disp-color",
	 .data = &mt6765_color_driver_data},
	{.compatible = "mediatek,mt6768-disp-color",
	 .data = &mt6768_color_driver_data},
	{.compatible = "mediatek,mt6885-disp-color",
	 .data = &mt6885_color_driver_data},
	{.compatible = "mediatek,mt6877-disp-color",
	 .data = &mt6877_color_driver_data},
	{.compatible = "mediatek,mt8173-disp-color",
	 .data = &mt8173_color_driver_data},
	{.compatible = "mediatek,mt6853-disp-color",
	 .data = &mt6853_color_driver_data},
	{.compatible = "mediatek,mt6833-disp-color",
	 .data = &mt6833_color_driver_data},
	{.compatible = "mediatek,mt6781-disp-color",
	 .data = &mt6781_color_driver_data},
	{.compatible = "mediatek,mt6983-disp-color",
	 .data = &mt6983_color_driver_data},
	{.compatible = "mediatek,mt6895-disp-color",
	 .data = &mt6895_color_driver_data},
	{.compatible = "mediatek,mt6879-disp-color",
	 .data = &mt6879_color_driver_data},
	{.compatible = "mediatek,mt6985-disp-color",
	 .data = &mt6985_color_driver_data},
	{.compatible = "mediatek,mt6886-disp-color",
	 .data = &mt6886_color_driver_data},
	{.compatible = "mediatek,mt6835-disp-color",
	 .data = &mt6835_color_driver_data},
	{.compatible = "mediatek,mt6897-disp-color",
	 .data = &mt6897_color_driver_data},
	{.compatible = "mediatek,mt6989-disp-color",
	 .data = &mt6989_color_driver_data},
	{.compatible = "mediatek,mt6878-disp-color",
	 .data = &mt6878_color_driver_data},
	{.compatible = "mediatek,mt6991-disp-color",
	 .data = &mt6991_color_driver_data},
	{.compatible = "mediatek,mt6899-disp-color",
	 .data = &mt6899_color_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_color_driver_dt_match);

struct platform_driver mtk_disp_color_driver = {
	.probe = disp_color_probe,
	.remove = disp_color_remove,
	.driver = {
			.name = "mediatek-disp-color",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_color_driver_dt_match,
		},
};

unsigned int disp_color_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int relay = 0;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_COLOR, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	struct mtk_disp_color *color_data = comp_to_color(comp);

	relay = color_data->primary_data->relay_state != 0 ? 1 : 0;

	return relay;
}
