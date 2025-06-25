// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/mfd/syscon.h>
#include "clkbuf-util.h"
#include "clkbuf-pmic.h"


#define DCXO_CW00			(0x788)
#define DCXO_CW08			(0x79c)
#define DCXO_CW09			(0x79e)
#define DCXO_CW10			(0x7a4)
#define DCXO_CW12			(0x7a8)
#define DCXO_CW13			(0x7aa)
#define DCXO_CW15			(0x7ae)
#define DCXO_CW16			(0x7b0)
#define DCXO_CW17			(0x7b2)
#define DCXO_CW19			(0x7b6)

#define PMRC_CON0			(0x1A6)
#define TOP_SPI_CON1			(0x458)
#define XO_BUF_CTL0			(0x54c)
#define XO_BUF_CTL1			(0x54e)
#define XO_BUF_CTL2			(0x550)
#define XO_BUF_CTL3			(0x552)
#define XO_BUF_CTL4			(0x554)

#define LDO_VRFCK_ELR			(0x1b40)
#define LDO_VRFCK_CON0			(0x1d1c)
#define LDO_VRFCK_OP_EN			(0x1d22)
#define LDO_VRFCK_OP_EN_SET		(0x1d24)
#define LDO_VRFCK_OP_EN_CLR		(0x1d26)
#define LDO_VBBCK_CON0			(0x1d2e)
#define LDO_VBBCK_OP_EN			(0x1d34)
#define LDO_VBBCK_OP_EN_SET		(0x1d36)
#define LDO_VBBCK_OP_EN_CLR		(0x1d38)
#define DCXO_ADLDO_BIAS_ELR_0		(0x209c)
#define DCXO_ADLDO_BIAS_ELR_1		(0x209e)

/* Register_TOP_REG */
#define NULL_ADDR					(0x0)
#define PMRC_EN_ADDR			(PMRC_CON0)
#define PMRC_EN_MASK			(0xFFFF)
#define PMRC_EN_SHIFT			(0)
/* Register_PLT_REG*/
#define RG_SRCLKEN_IN3_EN_ADDR		(TOP_SPI_CON1)
#define RG_SRCLKEN_IN3_EN_MASK		(0x1)
#define RG_SRCLKEN_IN3_EN_SHIFT		(0)
/* Register_SCK_REG*/
#define XO_SOC_VOTE_ADDR		(XO_BUF_CTL0)
#define XO_SOC_VOTE_MASK		(0x7ff)
#define XO_SOC_VOTE_SHIFT		(0)
#define XO_WCN_VOTE_ADDR		(XO_BUF_CTL1)
#define XO_WCN_VOTE_MASK		(0x7ff)
#define XO_WCN_VOTE_SHIFT		(0)
#define XO_NFC_VOTE_ADDR		(XO_BUF_CTL2)
#define XO_NFC_VOTE_MASK		(0x7ff)
#define XO_NFC_VOTE_SHIFT		(0)
#define XO_CEL_VOTE_ADDR		(XO_BUF_CTL3)
#define XO_CEL_VOTE_MASK		(0x7ff)
#define XO_CEL_VOTE_SHIFT		(0)
#define XO_EXT_VOTE_ADDR		(XO_BUF_CTL4)
#define XO_EXT_VOTE_MASK		(0x7ff)
#define XO_EXT_VOTE_SHIFT		(0)
/* Register_DCXO_REG*/
#define XO_EXTBUF1_MODE_ADDR		(DCXO_CW00)
#define XO_EXTBUF1_MODE_MASK		(0x3)
#define XO_EXTBUF1_MODE_SHIFT		(0)
#define XO_EXTBUF1_EN_M_ADDR		(DCXO_CW00)
#define XO_EXTBUF1_EN_M_MASK		(0x1)
#define XO_EXTBUF1_EN_M_SHIFT		(2)
#define XO_EXTBUF2_MODE_ADDR		(DCXO_CW00)
#define XO_EXTBUF2_MODE_MASK		(0x3)
#define XO_EXTBUF2_MODE_SHIFT		(3)
#define XO_EXTBUF2_EN_M_ADDR		(DCXO_CW00)
#define XO_EXTBUF2_EN_M_MASK		(0x1)
#define XO_EXTBUF2_EN_M_SHIFT		(5)
#define XO_EXTBUF3_MODE_ADDR		(DCXO_CW00)
#define XO_EXTBUF3_MODE_MASK		(0x3)
#define XO_EXTBUF3_MODE_SHIFT		(6)
#define XO_EXTBUF3_EN_M_ADDR		(DCXO_CW00)
#define XO_EXTBUF3_EN_M_MASK		(0x1)
#define XO_EXTBUF3_EN_M_SHIFT		(8)
#define XO_EXTBUF4_MODE_ADDR		(DCXO_CW00)
#define XO_EXTBUF4_MODE_MASK		(0x3)
#define XO_EXTBUF4_MODE_SHIFT		(9)
#define XO_EXTBUF4_EN_M_ADDR		(DCXO_CW00)
#define XO_EXTBUF4_EN_M_MASK		(0x1)
#define XO_EXTBUF4_EN_M_SHIFT		(11)
#define XO_BB_LPM_EN_M_ADDR		(DCXO_CW00)
#define XO_BB_LPM_EN_M_MASK		(0x1)
#define XO_BB_LPM_EN_M_SHIFT		(12)
#define XO_PMIC_TOP_DIG_SW_ADDR		(DCXO_CW08)
#define XO_PMIC_TOP_DIG_SW_MASK		(0x1)
#define XO_PMIC_TOP_DIG_SW_SHIFT	(2)
#define XO_EXTBUF6_MODE_ADDR		(DCXO_CW09)
#define XO_EXTBUF6_MODE_MASK		(0x3)
#define XO_EXTBUF6_MODE_SHIFT		(9)
#define XO_EXTBUF6_EN_M_ADDR		(DCXO_CW09)
#define XO_EXTBUF6_EN_M_MASK		(0x1)
#define XO_EXTBUF6_EN_M_SHIFT		(11)
#define XO_EXTBUF7_MODE_ADDR		(DCXO_CW09)
#define XO_EXTBUF7_MODE_MASK		(0x3)
#define XO_EXTBUF7_MODE_SHIFT		(12)
#define XO_EXTBUF7_EN_M_ADDR		(DCXO_CW09)
#define XO_EXTBUF7_EN_M_MASK		(0x1)
#define XO_EXTBUF7_EN_M_SHIFT		(14)
#define XO_BB_LPM_EN_SEL_ADDR		(DCXO_CW12)
#define XO_BB_LPM_EN_SEL_MASK		(0x1)
#define XO_BB_LPM_EN_SEL_SHIFT		(0)
#define XO_EXTBUF1_BBLPM_EN_MASK_ADDR	(DCXO_CW12)
#define XO_EXTBUF1_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF1_BBLPM_EN_MASK_SHIFT	(1)
#define XO_EXTBUF2_BBLPM_EN_MASK_ADDR	(DCXO_CW12)
#define XO_EXTBUF2_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF2_BBLPM_EN_MASK_SHIFT	(2)
#define XO_EXTBUF3_BBLPM_EN_MASK_ADDR	(DCXO_CW12)
#define XO_EXTBUF3_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF3_BBLPM_EN_MASK_SHIFT	(3)
#define XO_EXTBUF4_BBLPM_EN_MASK_ADDR	(DCXO_CW12)
#define XO_EXTBUF4_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF4_BBLPM_EN_MASK_SHIFT	(4)
#define XO_EXTBUF6_BBLPM_EN_MASK_ADDR	(DCXO_CW12)
#define XO_EXTBUF6_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF6_BBLPM_EN_MASK_SHIFT	(5)
#define XO_EXTBUF7_BBLPM_EN_MASK_ADDR	(DCXO_CW12)
#define XO_EXTBUF7_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF7_BBLPM_EN_MASK_SHIFT	(6)
#define RG_XO_EXTBUF2_SRSEL_ADDR	(DCXO_CW13)
#define RG_XO_EXTBUF2_SRSEL_MASK	(0x7)
#define RG_XO_EXTBUF2_SRSEL_SHIFT	(0)
#define RG_XO_EXTBUF4_SRSEL_ADDR	(DCXO_CW13)
#define RG_XO_EXTBUF4_SRSEL_MASK	(0x7)
#define RG_XO_EXTBUF4_SRSEL_SHIFT	(4)
#define RG_XO_EXTBUF3_HD_ADDR		(DCXO_CW13)
#define RG_XO_EXTBUF3_HD_MASK		(0x3)
#define RG_XO_EXTBUF3_HD_SHIFT		(10)
#define XO_STATIC_AUXOUT_SEL_ADDR	(DCXO_CW16)
#define XO_STATIC_AUXOUT_SEL_MASK	(0x3f)
#define XO_STATIC_AUXOUT_SEL_SHIFT	(0)
#define XO_AUXOUT_SEL_ADDR		(DCXO_CW16)
#define XO_AUXOUT_SEL_MASK		(0x3ff)
#define XO_AUXOUT_SEL_SHIFT		(6)
#define XO_STATIC_AUXOUT_ADDR		(DCXO_CW17)
#define XO_STATIC_AUXOUT_MASK		(0xffff)
#define XO_STATIC_AUXOUT_SHIFT		(0)
#define RG_XO_EXTBUF1_RSEL_ADDR		(DCXO_CW19)
#define RG_XO_EXTBUF1_RSEL_MASK		(0x7)
#define RG_XO_EXTBUF1_RSEL_SHIFT	(1)
#define RG_XO_EXTBUF2_RSEL_ADDR		(DCXO_CW19)
#define RG_XO_EXTBUF2_RSEL_MASK		(0x7)
#define RG_XO_EXTBUF2_RSEL_SHIFT	(4)
#define RG_XO_EXTBUF3_RSEL_ADDR		(DCXO_CW19)
#define RG_XO_EXTBUF3_RSEL_MASK		(0x7)
#define RG_XO_EXTBUF3_RSEL_SHIFT	(7)
#define RG_XO_EXTBUF4_RSEL_ADDR		(DCXO_CW19)
#define RG_XO_EXTBUF4_RSEL_MASK		(0x7)
#define RG_XO_EXTBUF4_RSEL_SHIFT	(10)
#define RG_XO_EXTBUF7_RSEL_ADDR		(DCXO_CW19)
#define RG_XO_EXTBUF7_RSEL_MASK		(0x7)
#define RG_XO_EXTBUF7_RSEL_SHIFT	(13)
/* Register_LDO_REG*/
#define RG_LDO_VRFCK_ANA_SEL_ADDR	(LDO_VRFCK_ELR)
#define RG_LDO_VRFCK_ANA_SEL_MASK	(0x1)
#define RG_LDO_VRFCK_ANA_SEL_SHIFT	(0)
#define RG_LDO_VRFCK_EN_ADDR		(LDO_VRFCK_CON0)
#define RG_LDO_VRFCK_EN_MASK		(0x1)
#define RG_LDO_VRFCK_EN_SHIFT		(0)
#define RG_LDO_VRFCK_HW14_OP_EN_ADDR	(LDO_VRFCK_OP_EN)
#define RG_LDO_VRFCK_HW14_OP_EN_MASK	(0x1)
#define RG_LDO_VRFCK_HW14_OP_EN_SHIFT	(14)
#define RG_LDO_VBBCK_EN_ADDR		(LDO_VBBCK_CON0)
#define RG_LDO_VBBCK_EN_MASK		(0x1)
#define RG_LDO_VBBCK_EN_SHIFT		(0)
#define RG_LDO_VBBCK_HW14_OP_EN_ADDR	(LDO_VBBCK_OP_EN)
#define RG_LDO_VBBCK_HW14_OP_EN_MASK	(0x1)
#define RG_LDO_VBBCK_HW14_OP_EN_SHIFT	(14)
#define RG_VRFCK_HV_EN_ADDR		(DCXO_ADLDO_BIAS_ELR_0)
#define RG_VRFCK_HV_EN_MASK		(0x1)
#define RG_VRFCK_HV_EN_SHIFT		(9)
#define RG_VRFCK_NDIS_EN_ADDR		(DCXO_ADLDO_BIAS_ELR_0)
#define RG_VRFCK_NDIS_EN_MASK		(0x1)
#define RG_VRFCK_NDIS_EN_SHIFT		(11)
#define RG_VRFCK_1_NDIS_EN_ADDR		(DCXO_ADLDO_BIAS_ELR_1)
#define RG_VRFCK_1_NDIS_EN_MASK		(0x1)
#define RG_VRFCK_1_NDIS_EN_SHIFT	(0)


static struct reg_t mt6359p_debug_regs[] = {
	[0] =
		DBG_REG(dcxo_cw00, DCXO_CW00, 0xFFFF, 0)
	[1] =
		DBG_REG(dcxo_cw08, DCXO_CW08, 0xFFFF, 0)
	[2] =
		DBG_REG(dcxo_cw09, DCXO_CW09, 0xFFFF, 0)
	[3] =
		DBG_REG(dcxo_cw10, DCXO_CW10, 0xFFFF, 0)
	[4] =
		DBG_REG(dcxo_cw12, DCXO_CW12, 0xFFFF, 0)
	[5] =
		DBG_REG(dcxo_cw13, DCXO_CW13, 0xFFFF, 0)
	[6] =
		DBG_REG(dcxo_cw15, DCXO_CW15, 0xFFFF, 0)
	[7] =
		DBG_REG(dcxo_cw19, DCXO_CW19, 0xFFFF, 0)
	[8] =
		DBG_REG(NULL, NULL_ADDR, 0x0, 0x0)
};

static struct common_regs mt6359p_com_regs = {
	.bblpm_auxout_sel = 14,
	.mode_num = 3,
	SET_REG_BY_NAME(static_aux_sel, XO_STATIC_AUXOUT_SEL)
	SET_REG(bblpm_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 5)
	SET_REG_BY_NAME(swbblpm_en, XO_BB_LPM_EN_M)
	SET_REG_BY_NAME(hwbblpm_sel, XO_BB_LPM_EN_SEL)
};

static struct xo_buf_t mt6359p_xo_bufs[] = {
 	[0] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF1_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF1_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 13)
		SET_REG_BY_NAME(impedance, RG_XO_EXTBUF1_RSEL)
		SET_REG_BY_NAME(rc_voter, XO_SOC_VOTE)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF1_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
	[1] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF2_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF2_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 11)
		SET_REG_BY_NAME(impedance, RG_XO_EXTBUF2_RSEL)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF2_SRSEL)
		SET_REG_BY_NAME(rc_voter, XO_WCN_VOTE)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF2_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
	[2] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF3_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF3_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 9)
		SET_REG_BY_NAME(impedance, RG_XO_EXTBUF3_RSEL)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF3_HD)
		SET_REG_BY_NAME(rc_voter, XO_NFC_VOTE)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF3_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
	[3] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF4_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF4_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 7)
		SET_REG_BY_NAME(impedance, RG_XO_EXTBUF4_RSEL)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF4_SRSEL)
		SET_REG_BY_NAME(rc_voter, XO_CEL_VOTE)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF4_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
	[6] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF7_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF7_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 3)
		SET_REG_BY_NAME(impedance, RG_XO_EXTBUF7_RSEL)
		SET_REG_BY_NAME(rc_voter, XO_EXT_VOTE)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF7_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
};

struct plat_xodata mt6359p_data = {
	.xo_buf_t = mt6359p_xo_bufs,
	.debug_regs = mt6359p_debug_regs,
	.common_regs = &mt6359p_com_regs,
};