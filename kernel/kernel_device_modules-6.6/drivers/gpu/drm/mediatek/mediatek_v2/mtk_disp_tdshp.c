// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_disp_tdshp.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_aal.h"

 /*******************************/
 /* field definition */
 /* ------------------------------------- */
 /* DISP TDSHP */
#define DISP_TDSHP_00              (0x000)
#define DISP_TDSHP_01              (0x004)
#define DISP_TDSHP_02              (0x008)
#define DISP_TDSHP_03              (0x00C)
#define DISP_TDSHP_05              (0x014)
#define DISP_TDSHP_06              (0x018)
#define DISP_TDSHP_07              (0x01C)
#define DISP_TDSHP_08              (0x020)
#define DISP_TDSHP_09              (0x024)
#define DISP_PBC_00                (0x040)
#define DISP_PBC_01                (0x044)
#define DISP_PBC_02                (0x048)
#define DISP_PBC_03                (0x04C)
#define DISP_PBC_04                (0x050)
#define DISP_PBC_05                (0x054)
#define DISP_PBC_06                (0x058)
#define DISP_PBC_07                (0x05C)
#define DISP_PBC_08                (0x060)
#define DISP_HIST_CFG_00           (0x064)
#define DISP_HIST_CFG_01           (0x068)
#define DISP_LUMA_HIST_00          (0x06C)
#define DISP_LUMA_HIST_01          (0x070)
#define DISP_LUMA_HIST_02          (0x074)
#define DISP_LUMA_HIST_03          (0x078)
#define DISP_LUMA_HIST_04          (0x07C)
#define DISP_LUMA_HIST_05          (0x080)
#define DISP_LUMA_HIST_06          (0x084)
#define DISP_LUMA_HIST_07          (0x08C)
#define DISP_LUMA_HIST_08          (0x090)
#define DISP_LUMA_HIST_09          (0x094)
#define DISP_LUMA_HIST_10          (0x098)
#define DISP_LUMA_HIST_11          (0x09C)
#define DISP_LUMA_HIST_12          (0x0A0)
#define DISP_LUMA_HIST_13          (0x0A4)
#define DISP_LUMA_HIST_14          (0x0A8)
#define DISP_LUMA_HIST_15          (0x0AC)
#define DISP_LUMA_HIST_16          (0x0B0)
#define DISP_LUMA_SUM              (0x0B4)
#define DISP_Y_FTN_1_0_MAIN        (0x0BC)
#define DISP_Y_FTN_3_2_MAIN        (0x0C0)
#define DISP_Y_FTN_5_4_MAIN        (0x0C4)
#define DISP_Y_FTN_7_6_MAIN        (0x0C8)
#define DISP_Y_FTN_9_8_MAIN        (0x0CC)
#define DISP_Y_FTN_11_10_MAIN      (0x0D0)
#define DISP_Y_FTN_13_12_MAIN      (0x0D4)
#define DISP_Y_FTN_15_14_MAIN      (0x0D8)
#define DISP_Y_FTN_17_16_MAIN      (0x0DC)
#define DISP_C_BOOST_MAIN          (0x0E0)
#define DISP_C_BOOST_MAIN_2        (0x0E4)
#define DISP_TDSHP_C_BOOST_MAIN    (0x0E8)
#define DISP_TDSHP_C_BOOST_MAIN_2  (0x0EC)
#define DISP_TDSHP_ATPG            (0x0FC)
#define DISP_TDSHP_CTRL            (0x100)
#define DISP_TDSHP_INTEN           (0x104)
#define DISP_TDSHP_INTSTA          (0x108)
#define DISP_TDSHP_STATUS          (0x10C)
#define DISP_TDSHP_CFG             (0x110)
#define DISP_TDSHP_INPUT_COUNT     (0x114)
#define DISP_TDSHP_CHKSUM          (0x118)
#define DISP_TDSHP_OUTPUT_COUNT    (0x11C)
#define DISP_TDSHP_INPUT_SIZE      (0x120)
#define DISP_TDSHP_OUTPUT_OFFSET   (0x124)
#define DISP_TDSHP_OUTPUT_SIZE     (0x128)
#define DISP_TDSHP_BLANK_WIDTH     (0x12C)
#define DISP_TDSHP_DEMO_HMASK      (0x130)
#define DISP_TDSHP_DEMO_VMASK      (0x134)
#define DISP_TDSHP_DUMMY_REG        (0x14C)
#define DISP_LUMA_HIST_INIT_00      (0x200)
#define DISP_LUMA_HIST_INIT_01      (0x204)
#define DISP_LUMA_HIST_INIT_02      (0x208)
#define DISP_LUMA_HIST_INIT_03      (0x20C)
#define DISP_LUMA_HIST_INIT_04      (0x210)
#define DISP_LUMA_HIST_INIT_05      (0x214)
#define DISP_LUMA_HIST_INIT_06      (0x218)
#define DISP_LUMA_HIST_INIT_07      (0x21C)
#define DISP_LUMA_HIST_INIT_08      (0x220)
#define DISP_LUMA_HIST_INIT_09      (0x224)
#define DISP_LUMA_HIST_INIT_10      (0x228)
#define DISP_LUMA_HIST_INIT_11      (0x22C)
#define DISP_LUMA_HIST_INIT_12      (0x230)
#define DISP_LUMA_HIST_INIT_13      (0x234)
#define DISP_LUMA_HIST_INIT_14      (0x238)
#define DISP_LUMA_HIST_INIT_15      (0x23C)
#define DISP_LUMA_HIST_INIT_16      (0x240)
#define DISP_LUMA_SUM_INIT          (0x244)

/*MT6755 New feature*/
#define DISP_DC_DBG_CFG_MAIN            (0x250)
#define DISP_DC_WIN_X_MAIN              (0x254)
#define DISP_DC_WIN_Y_MAIN              (0x258)
#define DISP_DC_TWO_D_W1                (0x25C)
#define DISP_DC_TWO_D_W1_RESULT_INIT    (0x260)
#define DISP_DC_TWO_D_W1_RESULT         (0x264)
/*MT6797 New feature*/
#define DISP_EDF_GAIN_00                (0x300)
#define DISP_EDF_GAIN_01                (0x304)
#define DISP_EDF_GAIN_02                (0x308)
#define DISP_EDF_GAIN_03                (0x30C)
#define DISP_EDF_GAIN_04                (0x310)
#define DISP_EDF_GAIN_05                (0x314)
#define DISP_TDSHP_10                   (0x320)
#define DISP_TDSHP_11                   (0x324)
#define DISP_TDSHP_12                   (0x328)
#define DISP_TDSHP_13                   (0x32C)

#define PAT1_GEN_SET               (0x330)
#define PAT1_GEN_FRM_SIZE          (0x334)
#define PAT1_GEN_COLOR0            (0x338)
#define PAT1_GEN_COLOR1            (0x33C)
#define PAT1_GEN_COLOR2            (0x340)
#define PAT1_GEN_POS               (0x344)
#define PAT1_GEN_TILE_POS          (0x354)
#define PAT1_GEN_TILE_OV           (0x358)
#define PAT2_GEN_SET               (0x360)
#define PAT2_GEN_COLOR0            (0x368)
#define PAT2_GEN_COLOR1            (0x36C)
#define PAT2_GEN_POS               (0x374)
#define PAT2_GEN_CURSOR_RB0        (0x378)
#define PAT2_GEN_CURSOR_RB1        (0x37C)
#define PAT2_GEN_TILE_POS          (0x384)
#define PAT2_GEN_TILE_OV           (0x388)

#define DISP_BITPLUS_00                (0x38C)
#define DISP_BITPLUS_01                (0x390)
#define DISP_BITPLUS_02                (0x394)
#define DISP_DC_SKIN_RANGE0            (0x420)

#define DISP_CONTOUR_HIST_INIT_00      (0x398)
#define DISP_CONTOUR_HIST_INIT_01      (0x39C)
#define DISP_CONTOUR_HIST_INIT_02      (0x3A0)
#define DISP_CONTOUR_HIST_INIT_03      (0x3A4)
#define DISP_CONTOUR_HIST_INIT_04      (0x3A8)
#define DISP_CONTOUR_HIST_INIT_05      (0x3AC)
#define DISP_CONTOUR_HIST_INIT_06      (0x3B0)
#define DISP_CONTOUR_HIST_INIT_07      (0x3B4)
#define DISP_CONTOUR_HIST_INIT_08      (0x3B8)
#define DISP_CONTOUR_HIST_INIT_09      (0x3BC)
#define DISP_CONTOUR_HIST_INIT_10      (0x3C0)
#define DISP_CONTOUR_HIST_INIT_11      (0x3C4)
#define DISP_CONTOUR_HIST_INIT_12      (0x3C8)
#define DISP_CONTOUR_HIST_INIT_13      (0x3CC)
#define DISP_CONTOUR_HIST_INIT_14      (0x3D0)
#define DISP_CONTOUR_HIST_INIT_15      (0x3D4)
#define DISP_CONTOUR_HIST_INIT_16      (0x3D8)
#define DISP_CONTOUR_HIST_00           (0x3DC)
#define DISP_CONTOUR_HIST_01           (0x3E0)
#define DISP_CONTOUR_HIST_02           (0x3E4)
#define DISP_CONTOUR_HIST_03           (0x3E8)
#define DISP_CONTOUR_HIST_04           (0x3EC)
#define DISP_CONTOUR_HIST_05           (0x3F0)
#define DISP_CONTOUR_HIST_06           (0x3F4)
#define DISP_CONTOUR_HIST_07           (0x3F8)
#define DISP_CONTOUR_HIST_08           (0x3FC)
#define DISP_CONTOUR_HIST_09           (0x400)
#define DISP_CONTOUR_HIST_10           (0x404)
#define DISP_CONTOUR_HIST_11           (0x408)
#define DISP_CONTOUR_HIST_12           (0x40C)
#define DISP_CONTOUR_HIST_13           (0x410)
#define DISP_CONTOUR_HIST_14           (0x414)
#define DISP_CONTOUR_HIST_15           (0x418)
#define DISP_CONTOUR_HIST_16           (0x41C)
#define DISP_DC_SKIN_RANGE1            (0x424)
#define DISP_DC_SKIN_RANGE2            (0x428)
#define DISP_DC_SKIN_RANGE3            (0x42C)
#define DISP_DC_SKIN_RANGE4            (0x430)
#define DISP_DC_SKIN_RANGE5            (0x434)
#define DISP_POST_YLEV_00              (0x480)
#define DISP_POST_YLEV_01              (0x484)
#define DISP_POST_YLEV_02              (0x488)
#define DISP_POST_YLEV_03              (0x48C)
#define DISP_POST_YLEV_04              (0x490)

#define DISP_HFG_CTRL                  (0x500)
#define DISP_HFG_RAN_0                 (0x504)
#define DISP_HFG_RAN_1                 (0x508)
#define DISP_HFG_RAN_2                 (0x50C)
#define DISP_HFG_RAN_3                 (0x510)
#define DISP_HFG_RAN_4                 (0x514)
#define DISP_HFG_CROP_X                (0x518)
#define DISP_HFG_CROP_Y                (0x51C)
#define DISP_HFC_CON_0                 (0x524)
#define DISP_HFC_LUMA_0                (0x528)
#define DISP_HFC_LUMA_1                (0x52C)
#define DISP_HFC_LUMA_2                (0x530)
#define DISP_HFC_SL2_0                 (0x534)
#define DISP_HFC_SL2_1                 (0x538)
#define DISP_HFC_SL2_2                 (0x53C)
#define DISP_SL2_CEN                   (0x544)
#define DISP_SL2_RR_CON0               (0x548)
#define DISP_SL2_RR_CON1               (0x54C)
#define DISP_SL2_GAIN                  (0x550)
#define DISP_SL2_RZ                    (0x554)
#define DISP_SL2_XOFF                  (0x558)
#define DISP_SL2_YOFF                  (0x55C)
#define DISP_SL2_SLP_CON0              (0x560)
#define DISP_SL2_SLP_CON1              (0x564)
#define DISP_SL2_SLP_CON2              (0x568)
#define DISP_SL2_SLP_CON3              (0x66C)
#define DISP_SL2_SIZE                  (0x670)
#define DISP_HFG_OUTPUT_COUNT          (0x678)
#define DISP_TDSHP_SHADOW_CTRL         (0x724)

#define DISP_TDSHP_EN BIT(0)
#define TDSHP_RELAY_MODE BIT(0)

static void disp_tdshp_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle);

static inline struct mtk_disp_tdshp *comp_to_tdshp(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_tdshp, ddp_comp);
}

static int disp_tdshp_write_tdshp_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	struct DISP_TDSHP_REG *disp_tdshp_regs;
	int ret = 0;

	if (lock)
		mutex_lock(&primary_data->data_lock);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CFG, 0x2, 0x2);

	/* to avoid different show of dual pipe, pipe1 use pipe0's config data */
	disp_tdshp_regs = primary_data->tdshp_regs;
	if (disp_tdshp_regs == NULL) {
		DDPINFO("%s: comp %d not initialized\n", __func__, comp->id);
		ret = -EFAULT;
		goto thshp_write_reg_unlock;
	}

	DDPINFO("tdshp_en: %x, tdshp_limit: %x, tdshp_ylev_256: %x, tdshp_gain_high:%d, tdshp_gain_mid:%d\n",
			disp_tdshp_regs->tdshp_en, disp_tdshp_regs->tdshp_limit,
			disp_tdshp_regs->tdshp_ylev_256, disp_tdshp_regs->tdshp_gain_high,
			disp_tdshp_regs->tdshp_gain_mid);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_00,
		(disp_tdshp_regs->tdshp_softcoring_gain << 0 |
				disp_tdshp_regs->tdshp_gain_high << 8 |
				disp_tdshp_regs->tdshp_gain_mid << 16 |
				disp_tdshp_regs->tdshp_ink_sel << 24 |
				disp_tdshp_regs->tdshp_bypass_high << 29 |
				disp_tdshp_regs->tdshp_bypass_mid << 30 |
				disp_tdshp_regs->tdshp_en << 31), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_01,
		(disp_tdshp_regs->tdshp_limit_ratio << 0 |
				disp_tdshp_regs->tdshp_gain << 4 |
				disp_tdshp_regs->tdshp_coring_zero << 16 |
				disp_tdshp_regs->tdshp_coring_thr << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_02,
		(disp_tdshp_regs->tdshp_coring_value << 8 |
				disp_tdshp_regs->tdshp_bound << 16 |
				disp_tdshp_regs->tdshp_limit << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_03,
		(disp_tdshp_regs->tdshp_sat_proc << 0 |
				disp_tdshp_regs->tdshp_ac_lpf_coe << 8 |
				disp_tdshp_regs->tdshp_clip_thr << 16 |
				disp_tdshp_regs->tdshp_clip_ratio << 24 |
				disp_tdshp_regs->tdshp_clip_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_05,
		(disp_tdshp_regs->tdshp_ylev_p048 << 0 |
		disp_tdshp_regs->tdshp_ylev_p032 << 8 |
		disp_tdshp_regs->tdshp_ylev_p016 << 16 |
		disp_tdshp_regs->tdshp_ylev_p000 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_06,
		(disp_tdshp_regs->tdshp_ylev_p112 << 0 |
		disp_tdshp_regs->tdshp_ylev_p096 << 8 |
		disp_tdshp_regs->tdshp_ylev_p080 << 16 |
		disp_tdshp_regs->tdshp_ylev_p064 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_07,
		(disp_tdshp_regs->tdshp_ylev_p176 << 0 |
		disp_tdshp_regs->tdshp_ylev_p160 << 8 |
		disp_tdshp_regs->tdshp_ylev_p144 << 16 |
		disp_tdshp_regs->tdshp_ylev_p128 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_08,
		(disp_tdshp_regs->tdshp_ylev_p240 << 0 |
		disp_tdshp_regs->tdshp_ylev_p224 << 8 |
		disp_tdshp_regs->tdshp_ylev_p208 << 16 |
		disp_tdshp_regs->tdshp_ylev_p192 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_09,
		(disp_tdshp_regs->tdshp_ylev_en << 14 |
		disp_tdshp_regs->tdshp_ylev_alpha << 16 |
		disp_tdshp_regs->tdshp_ylev_256 << 24), ~0);

	// PBC1
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_00,
		(disp_tdshp_regs->pbc1_radius_r << 0 |
		disp_tdshp_regs->pbc1_theta_r << 6 |
		disp_tdshp_regs->pbc1_rslope_1 << 12 |
		disp_tdshp_regs->pbc1_gain << 22 |
		disp_tdshp_regs->pbc1_lpf_en << 30 |
		disp_tdshp_regs->pbc1_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_01,
		(disp_tdshp_regs->pbc1_lpf_gain << 0 |
		disp_tdshp_regs->pbc1_tslope << 6 |
		disp_tdshp_regs->pbc1_radius_c << 16 |
		disp_tdshp_regs->pbc1_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_02,
		(disp_tdshp_regs->pbc1_edge_slope << 0 |
		disp_tdshp_regs->pbc1_edge_thr << 8 |
		disp_tdshp_regs->pbc1_edge_en << 14 |
		disp_tdshp_regs->pbc1_conf_gain << 16 |
		disp_tdshp_regs->pbc1_rslope << 22), ~0);
	// PBC2
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_03,
		(disp_tdshp_regs->pbc2_radius_r << 0 |
		disp_tdshp_regs->pbc2_theta_r << 6 |
		disp_tdshp_regs->pbc2_rslope_1 << 12 |
		disp_tdshp_regs->pbc2_gain << 22 |
		disp_tdshp_regs->pbc2_lpf_en << 30 |
		disp_tdshp_regs->pbc2_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_04,
		(disp_tdshp_regs->pbc2_lpf_gain << 0 |
		disp_tdshp_regs->pbc2_tslope << 6 |
		disp_tdshp_regs->pbc2_radius_c << 16 |
		disp_tdshp_regs->pbc2_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_05,
		(disp_tdshp_regs->pbc2_edge_slope << 0 |
		disp_tdshp_regs->pbc2_edge_thr << 8 |
		disp_tdshp_regs->pbc2_edge_en << 14 |
		disp_tdshp_regs->pbc2_conf_gain << 16 |
		disp_tdshp_regs->pbc2_rslope << 22), ~0);
	// PBC3
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_06,
		(disp_tdshp_regs->pbc3_radius_r << 0 |
		disp_tdshp_regs->pbc3_theta_r << 6 |
		disp_tdshp_regs->pbc3_rslope_1 << 12 |
		disp_tdshp_regs->pbc3_gain << 22 |
		disp_tdshp_regs->pbc3_lpf_en << 30 |
		disp_tdshp_regs->pbc3_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_07,
		(disp_tdshp_regs->pbc3_lpf_gain << 0 |
		disp_tdshp_regs->pbc3_tslope << 6 |
		disp_tdshp_regs->pbc3_radius_c << 16 |
		disp_tdshp_regs->pbc3_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_08,
		(disp_tdshp_regs->pbc3_edge_slope << 0 |
		disp_tdshp_regs->pbc3_edge_thr << 8 |
		disp_tdshp_regs->pbc3_edge_en << 14 |
		disp_tdshp_regs->pbc3_conf_gain << 16 |
		disp_tdshp_regs->pbc3_rslope << 22), ~0);

//#ifdef TDSHP_2_0
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_10,
		(disp_tdshp_regs->tdshp_mid_softlimit_ratio << 0 |
		disp_tdshp_regs->tdshp_mid_coring_zero << 16 |
		disp_tdshp_regs->tdshp_mid_coring_thr << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_11,
		(disp_tdshp_regs->tdshp_mid_softcoring_gain << 0 |
		disp_tdshp_regs->tdshp_mid_coring_value << 8 |
		disp_tdshp_regs->tdshp_mid_bound << 16 |
		disp_tdshp_regs->tdshp_mid_limit << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_12,
		(disp_tdshp_regs->tdshp_high_softlimit_ratio << 0 |
		disp_tdshp_regs->tdshp_high_coring_zero << 16 |
		disp_tdshp_regs->tdshp_high_coring_thr << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_13,
		(disp_tdshp_regs->tdshp_high_softcoring_gain << 0 |
		disp_tdshp_regs->tdshp_high_coring_value << 8 |
		disp_tdshp_regs->tdshp_high_bound << 16 |
		disp_tdshp_regs->tdshp_high_limit << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_00,
		(disp_tdshp_regs->edf_clip_ratio_inc << 0 |
		disp_tdshp_regs->edf_edge_gain << 8 |
		disp_tdshp_regs->edf_detail_gain << 16 |
		disp_tdshp_regs->edf_flat_gain << 24 |
		disp_tdshp_regs->edf_gain_en << 31), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_01,
		(disp_tdshp_regs->edf_edge_th << 0 |
		disp_tdshp_regs->edf_detail_fall_th << 9 |
		disp_tdshp_regs->edf_detail_rise_th << 18 |
		disp_tdshp_regs->edf_flat_th << 25), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_02,
		(disp_tdshp_regs->edf_edge_slope << 0 |
		disp_tdshp_regs->edf_detail_fall_slope << 8 |
		disp_tdshp_regs->edf_detail_rise_slope << 16 |
		disp_tdshp_regs->edf_flat_slope << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_03,
		(disp_tdshp_regs->edf_edge_mono_slope << 0 |
		disp_tdshp_regs->edf_edge_mono_th << 8 |
		disp_tdshp_regs->edf_edge_mag_slope << 16 |
		disp_tdshp_regs->edf_edge_mag_th << 24), 0xFFFFFFFF);

	// DISP TDSHP no DISP_EDF_GAIN_04
	//cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_04,
	//	(disp_tdshp_regs->edf_edge_trend_flat_mag << 8 |
	//	disp_tdshp_regs->edf_edge_trend_slope << 16 |
	//	disp_tdshp_regs->edf_edge_trend_th << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_05,
		(disp_tdshp_regs->edf_bld_wgt_mag << 0 |
		disp_tdshp_regs->edf_bld_wgt_mono << 8), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_C_BOOST_MAIN,
		(disp_tdshp_regs->tdshp_cboost_gain << 0 |
		disp_tdshp_regs->tdshp_cboost_en << 13 |
		disp_tdshp_regs->tdshp_cboost_lmt_l << 16 |
		disp_tdshp_regs->tdshp_cboost_lmt_u << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_C_BOOST_MAIN_2,
		(disp_tdshp_regs->tdshp_cboost_yoffset << 0 |
		disp_tdshp_regs->tdshp_cboost_yoffset_sel << 16 |
		disp_tdshp_regs->tdshp_cboost_yconst << 24), 0xFFFFFFFF);

//#endif // TDSHP_2_0

//#ifdef TDSHP_3_0
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_00,
		(disp_tdshp_regs->tdshp_post_ylev_p048 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p032 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p016 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p000 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_01,
		(disp_tdshp_regs->tdshp_post_ylev_p112 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p096 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p080 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p064 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_02,
		(disp_tdshp_regs->tdshp_post_ylev_p176 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p160 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p144 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p128 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_03,
		(disp_tdshp_regs->tdshp_post_ylev_p240 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p224 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p208 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p192 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_04,
		(disp_tdshp_regs->tdshp_post_ylev_en << 14 |
		disp_tdshp_regs->tdshp_post_ylev_alpha << 16 |
		disp_tdshp_regs->tdshp_post_ylev_256 << 24), 0xFFFFFFFF);
//#endif // TDSHP_3_0

thshp_write_reg_unlock:
	if (lock)
		mutex_unlock(&primary_data->data_lock);

	return ret;
}

static int disp_tdshp_update_param(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_TDSHP_REG *user_tdshp_regs)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	int ret = 0;
	struct DISP_TDSHP_REG *tdshp_regs;

	pr_notice("%s\n", __func__);

	if (primary_data->tdshp_regs == NULL) {
		tdshp_regs = kmalloc(sizeof(struct DISP_TDSHP_REG), GFP_KERNEL);
		if (tdshp_regs == NULL) {
			DDPPR_ERR("%s: no memory\n", __func__);
			return -EFAULT;
		}

		primary_data->tdshp_regs = tdshp_regs;
	}

	if (user_tdshp_regs == NULL) {
		ret = -EFAULT;
	} else {
		mutex_lock(&primary_data->data_lock);
		memcpy(primary_data->tdshp_regs, user_tdshp_regs,
			sizeof(struct DISP_TDSHP_REG));

		pr_notice("%s: Set module(%d) lut\n", __func__, comp->id);
		ret = disp_tdshp_write_tdshp_reg(comp, handle, 0);
		mutex_unlock(&primary_data->data_lock);
	}

	return ret;
}

static int disp_tdshp_wait_size(struct mtk_ddp_comp *comp, unsigned long timeout)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	int ret = 0;

	if (primary_data->get_size_available == false) {
		ret = wait_event_interruptible(primary_data->size_wq,
			primary_data->get_size_available == true);

		DDPINFO("size_available = 1, Wake up, ret = %d\n", ret);
	} else {
		DDPINFO("size_available = 0\n");
	}

	return ret;
}

static int disp_tdshp_cfg_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{

	int ret = 0;
	struct DISP_TDSHP_REG *config = data;
	struct mtk_disp_tdshp *tdshp = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp->primary_data;

	if (disp_tdshp_update_param(comp, handle, config) < 0) {
		DDPPR_ERR("%s: failed\n", __func__);
		return -EFAULT;
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_tdshp1 = tdshp->companion;

		if (disp_tdshp_update_param(comp_tdshp1, handle, config) < 0) {
			DDPPR_ERR("%s: comp_tdshp1 failed\n", __func__);
			return -EFAULT;
		}
	}

	if (!primary_data->tdshp_reg_valid) {
		disp_tdshp_bypass(comp, 0, PQ_FEATURE_DEFAULT, handle);
		primary_data->tdshp_reg_valid = 1;
	}

	return ret;
}

int disp_tdshp_act_get_size(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	u32 width = 0, height = 0;
	struct DISP_TDSHP_DISPLAY_SIZE *dst =
			(struct DISP_TDSHP_DISPLAY_SIZE *)data;

	pr_notice("%s", __func__);

	mtk_drm_crtc_get_panel_original_size(crtc, &width, &height);
	if (width == 0 || height == 0) {
		DDPFUNC("panel original size error(%dx%d).\n", width, height);
		width = crtc->mode.hdisplay;
		height = crtc->mode.vdisplay;
	}

	primary_data->tdshp_size.lcm_width = width;
	primary_data->tdshp_size.lcm_height = height;

	disp_tdshp_wait_size(comp, 60);

	pr_notice("%s ---", __func__);
	memcpy(dst, &primary_data->tdshp_size, sizeof(primary_data->tdshp_size));

	return 0;
}

static void disp_tdshp_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!tdshp_data->is_right_pipe) {
			tdshp_data->tile_overhead.comp_overhead = 3;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				tdshp_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
					tdshp_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			tdshp_data->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			tdshp_data->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else {
			tdshp_data->tile_overhead.comp_overhead = 3;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				tdshp_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				tdshp_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			tdshp_data->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			tdshp_data->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void disp_tdshp_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	tdshp_data->tile_overhead_v.comp_overhead_v = 2;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		tdshp_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	tdshp_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void disp_tdshp_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int in_width, out_width;
	unsigned int in_val, out_val;
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, ((0x1 << 2) | 0x1), ~0);
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, ((0x0 << 2) | 0x1), ~0);
	else
		DDPPR_ERR("%s: Invalid bpc: %u\n", __func__, cfg->bpc);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		in_width = tdshp_data->tile_overhead.in_width;
		out_width = in_width - tdshp_data->tile_overhead.comp_overhead;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			in_width = cfg->w / 2;
		else
			in_width = cfg->w;

		out_width = in_width;
	}

	if (tdshp_data->set_partial_update != 1) {
		in_val = (in_width << 16) | (cfg->h);
		out_val = (out_width << 16) | (cfg->h);
	} else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : tdshp_data->tile_overhead_v.overhead_v;
		comp_overhead_v = (!overhead_v) ? 0 : tdshp_data->tile_overhead_v.comp_overhead_v;

		in_val = (in_width << 16) | (tdshp_data->roi_height + overhead_v * 2);
		out_val = (out_width << 16) |
				  (tdshp_data->roi_height + (overhead_v - comp_overhead_v) * 2);
	}

	DDPINFO("%s: in: 0x%08x, out: 0x%08x\n", __func__, in_val, out_val);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_INPUT_SIZE, in_val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE, out_val, ~0);
	// DISP_TDSHP_OUTPUT_OFFSET
	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (!tdshp_data->is_right_pipe)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0x0, ~0);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET,
				tdshp_data->tile_overhead.comp_overhead << 16 | 0, ~0);
	} else {
		if (tdshp_data->set_partial_update != 1)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0x0, ~0);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, comp_overhead_v, ~0);
	}

	// DISP_TDSHP_SWITCH
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CFG, 0x1 << 13, 0x1 << 13);

	if (tdshp_data->data->need_bypass_shadow)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_SHADOW_CTRL, 0x1, 0x1);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_SHADOW_CTRL, 0x0, 0x1);

	// for Display Clarity
	if (primary_data->aal_clarity_support && *primary_data->aal_clarity_support) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_00, 0x1 << 31, 0x1 << 31);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x1F << 12, 0x1F << 12);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x0 << 12, 0x1 << 12);
	}

	if (primary_data->tdshp_reg_valid)
		disp_tdshp_write_tdshp_reg(comp, handle, 0);

	if (primary_data->relay_state != 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, TDSHP_RELAY_MODE, TDSHP_RELAY_MODE);

	primary_data->tdshp_size.height = cfg->h;
	primary_data->tdshp_size.width = cfg->w;
	if (primary_data->get_size_available == false) {
		primary_data->get_size_available = true;
		wake_up_interruptible(&primary_data->size_wq);
		pr_notice("size available: (w, h)=(%d, %d)+\n", cfg->w, cfg->h);
	}
}

static void disp_tdshp_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	struct mtk_ddp_comp *companion = tdshp_data->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller, primary_data->relay_state);

	mutex_lock(&primary_data->data_lock);
	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_CFG, TDSHP_RELAY_MODE, TDSHP_RELAY_MODE);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_00, (0x1 << 31), (0x1 << 31));
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_CTRL, 0xfffffffd, ~0);
			if (comp->mtk_crtc->is_dual_pipe && companion) {
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_TDSHP_CFG,
					TDSHP_RELAY_MODE, TDSHP_RELAY_MODE);
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_TDSHP_00, (0x1 << 31), (0x1 << 31));
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_TDSHP_CTRL, 0xfffffffd, ~0);
			}
		}
		primary_data->relay_state |= (1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~ (0x1 << caller);
			if (primary_data->relay_state == 0) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_TDSHP_CFG, 0x0, TDSHP_RELAY_MODE);
				if (comp->mtk_crtc->is_dual_pipe && companion)
					cmdq_pkt_write(handle, companion->cmdq_base,
						companion->regs_pa + DISP_TDSHP_CFG,
						0x0, TDSHP_RELAY_MODE);
			}
		}
	}
	mutex_unlock(&primary_data->data_lock);
}

static int disp_tdshp_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	struct mtk_disp_tdshp *tdshp = comp_to_tdshp(comp);

	pr_notice("%s, cmd: %d\n", __func__, cmd);
	switch (cmd) {
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void disp_tdshp_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("line: %d\n", __LINE__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CTRL, DISP_TDSHP_EN, 0x1);
}

static void disp_tdshp_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CTRL, 0x0, 0x1);
}

static void disp_tdshp_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("id(%d)\n", comp->id);
	mtk_ddp_comp_clk_prepare(comp);
}

static void disp_tdshp_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("id(%d)\n", comp->id);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void disp_tdshp_init_primary_data(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp *companion_data = comp_to_tdshp(tdshp_data->companion);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	struct mtk_ddp_comp *aal_comp;
	struct mtk_disp_aal *aal_data = NULL;

	if (tdshp_data->is_right_pipe) {
		kfree(tdshp_data->primary_data);
		tdshp_data->primary_data = companion_data->primary_data;
		return;
	}
	aal_comp = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc, MTK_DISP_AAL, 0);
	if (aal_comp) {
		aal_data = comp_to_aal(aal_comp);
		primary_data->aal_clarity_support = &aal_data->primary_data->disp_clarity_support;
	}
	init_waitqueue_head(&primary_data->size_wq);
	mutex_init(&primary_data->data_lock);
	primary_data->tdshp_reg_valid = 0;
	primary_data->relay_state = 0x1 << PQ_FEATURE_DEFAULT;
}

static int disp_tdshp_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{

	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_tdshp *data = comp_to_tdshp(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_tdshp *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_tdshp(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_tdshp_init_primary_data(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_tdshp_init_primary_data(data->companion);
	}
		break;
	default:
		break;
	}
	return 0;
}

void disp_tdshp_first_cfg(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	pr_notice("%s\n", __func__);
	disp_tdshp_config(comp, cfg, handle);
}

static int disp_tdshp_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_TDSHP_SET_REG:
		ret = disp_tdshp_cfg_set_reg(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_tdshp_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_TDSHP_GET_SIZE:
		ret = disp_tdshp_act_get_size(comp, data);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_tdshp_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	tdshp_data->set_partial_update = enable;
	tdshp_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : tdshp_data->tile_overhead_v.overhead_v;
	comp_overhead_v = (!overhead_v) ? 0 : tdshp_data->tile_overhead_v.comp_overhead_v;

	DDPINFO("%s, %s overhead_v:%d, comp_overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v, comp_overhead_v);

	if (tdshp_data->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_INPUT_SIZE,
			tdshp_data->roi_height + overhead_v * 2, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE,
			tdshp_data->roi_height + (overhead_v - comp_overhead_v) * 2, 0xffff);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, comp_overhead_v, 0xff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_INPUT_SIZE,
			full_height, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE, full_height, 0xffff);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0, 0xff);
	}

	return 0;

}

static const struct mtk_ddp_comp_funcs mtk_disp_tdshp_funcs = {
	.config = disp_tdshp_config,
	.first_cfg = disp_tdshp_first_cfg,
	.start = disp_tdshp_start,
	.stop = disp_tdshp_stop,
	.bypass = disp_tdshp_bypass,
	.user_cmd = disp_tdshp_user_cmd,
	.prepare = disp_tdshp_prepare,
	.unprepare = disp_tdshp_unprepare,
	.config_overhead = disp_tdshp_config_overhead,
	.config_overhead_v = disp_tdshp_config_overhead_v,
	.io_cmd = disp_tdshp_io_cmd,
	.pq_frame_config = disp_tdshp_frame_config,
	.pq_ioctl_transact = disp_tdshp_ioctl_transact,
	.partial_update = disp_tdshp_set_partial_update,
};

static int disp_tdshp_bind(struct device *dev, struct device *master,
			void *data)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void disp_tdshp_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct component_ops mtk_disp_tdshp_component_ops = {
	.bind = disp_tdshp_bind,
	.unbind = disp_tdshp_unbind,
};

void disp_tdshp_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x4, 0x8, 0xC);
	mtk_cust_dump_reg(baddr, 0x14, 0x18, 0x1C, 0x20);
	mtk_cust_dump_reg(baddr, 0x24, 0x40, 0x44, 0x48);
	mtk_cust_dump_reg(baddr, 0x4C, 0x50, 0x54, 0x58);
	mtk_cust_dump_reg(baddr, 0x5C, 0x60, 0xE0, 0xE4);
	mtk_cust_dump_reg(baddr, 0xFC, 0x100, 0x104, 0x108);
	mtk_cust_dump_reg(baddr, 0x10C, 0x110, 0x114, 0x118);
	mtk_cust_dump_reg(baddr, 0x11C, 0x120, 0x124, 0x128);
	mtk_cust_dump_reg(baddr, 0x12C, 0x14C, 0x300, 0x304);
	mtk_cust_dump_reg(baddr, 0x308, 0x30C, 0x314, 0x320);
	mtk_cust_dump_reg(baddr, 0x324, 0x328, 0x32C, 0x330);
	mtk_cust_dump_reg(baddr, 0x334, 0x338, 0x33C, 0x340);
	mtk_cust_dump_reg(baddr, 0x344, 0x354, 0x358, 0x360);
	mtk_cust_dump_reg(baddr, 0x368, 0x36C, 0x374, 0x378);
	mtk_cust_dump_reg(baddr, 0x37C, 0x384, 0x388, 0x480);
	mtk_cust_dump_reg(baddr, 0x484, 0x488, 0x48C, 0x490);
	mtk_cust_dump_reg(baddr, 0x67C, -1, -1, -1);
	mtk_cust_dump_reg(baddr, 0x644, 0x648, 0x64C, 0x650);
	mtk_cust_dump_reg(baddr, 0x654, 0x658, 0x65C, 0x660);
	mtk_cust_dump_reg(baddr, 0x664, 0x668, 0x66C, 0x670);
}

void disp_tdshp_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp = comp_to_tdshp(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp),
			tdshp->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x720; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k), readl(baddr + k + 0x4),
			readl(baddr + k + 0x8), readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && tdshp->companion) {
		baddr = tdshp->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(tdshp->companion),
				&tdshp->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(tdshp->companion));
		for (k = 0; k <= 0x720; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k), readl(baddr + k + 0x4),
				readl(baddr + k + 0x8), readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(tdshp->companion));
	}
}

static int disp_tdshp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_tdshp *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	pr_notice("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_TDSHP);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_tdshp_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_tdshp_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	pr_notice("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int disp_tdshp_remove(struct platform_device *pdev)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(&pdev->dev);

	pr_notice("%s+\n", __func__);
	component_del(&pdev->dev, &mtk_disp_tdshp_component_ops);

	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	pr_notice("%s-\n", __func__);
	return 0;
}

static const struct mtk_disp_tdshp_data mt6983_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6895_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6879_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6985_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6897_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6899_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6989_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6878_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6991_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_tdshp_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-tdshp",
	  .data = &mt6983_tdshp_driver_data},
	{ .compatible = "mediatek,mt6895-disp-tdshp",
	  .data = &mt6895_tdshp_driver_data},
	{ .compatible = "mediatek,mt6879-disp-tdshp",
	  .data = &mt6879_tdshp_driver_data},
	{ .compatible = "mediatek,mt6985-disp-tdshp",
	  .data = &mt6985_tdshp_driver_data},
	{ .compatible = "mediatek,mt6897-disp-tdshp",
	  .data = &mt6897_tdshp_driver_data},
	{ .compatible = "mediatek,mt6989-disp-tdshp",
	  .data = &mt6989_tdshp_driver_data},
	{ .compatible = "mediatek,mt6878-disp-tdshp",
	  .data = &mt6878_tdshp_driver_data},
	{ .compatible = "mediatek,mt6991-disp-tdshp",
	  .data = &mt6991_tdshp_driver_data},
	{ .compatible = "mediatek,mt6899-disp-tdshp",
	  .data = &mt6899_tdshp_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_tdshp_driver_dt_match);

struct platform_driver mtk_disp_tdshp_driver = {
	.probe = disp_tdshp_probe,
	.remove = disp_tdshp_remove,
	.driver = {
			.name = "mediatek-disp-tdshp",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_tdshp_driver_dt_match,
		},
};

unsigned int disp_tdshp_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_tdshp *tdshp_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_TDSHP, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	tdshp_data = comp_to_tdshp(comp);

	return tdshp_data->primary_data->relay_state != 0 ? 1 : 0;
}
