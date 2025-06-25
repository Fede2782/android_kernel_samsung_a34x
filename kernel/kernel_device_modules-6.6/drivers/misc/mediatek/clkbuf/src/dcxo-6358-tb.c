// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/mfd/syscon.h>
#include "clkbuf-util.h"
#include "clkbuf-pmic.h"

#define DCXO_CW00			(0x788)
#define DCXO_CW03			(0x792)
#define DCXO_CW04			(0x794)
#define DCXO_CW05			(0x796)
#define DCXO_CW07			(0x79a)
#define DCXO_CW08			(0x79c)
#define DCXO_CW09			(0x79e)
#define DCXO_CW11			(0x7a2)
#define DCXO_CW12			(0x7a8)
#define DCXO_CW13			(0x7aa)
#define DCXO_CW15			(0x7ae)
#define DCXO_CW16			(0x7b0)
#define DCXO_CW18			(0x7b4)
#define DCXO_CW19			(0x7b6)
#define DCXO_CW20			(0x7b8)
#define DCXO_CW23			(0x7be)

/* Register_DCXO_REG*/
#define NULL_ADDR					(0x0)
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
#define RG_XO_CDAC_FPM_ADDR        (DCXO_CW04)
#define RG_XO_CDAC_FPM_MASK        (0xFF)
#define RG_XO_CDAC_FPM_SHIFT       (0)
#define RG_XO_COFST_FPM_ADDR        (DCXO_CW05)
#define RG_XO_COFST_FPM_MASK        (0x3)
#define RG_XO_COFST_FPM_SHIFT       (14)
#define RG_XO_IDAC_FPM_ADDR        (DCXO_CW07)
#define RG_XO_IDAC_FPM_MASK        (0x3)
#define RG_XO_IDAC_FPM_SHIFT       (14)
#define RG_XO_AAC_FPM_SWEN_ADDR    (DCXO_CW09)
#define RG_XO_AAC_FPM_SWEN_MASK    (0x1)
#define RG_XO_AAC_FPM_SWEN_SHIFT   (14)

#define XO_EXTBUF6_MODE_ADDR		(DCXO_CW11)
#define XO_EXTBUF6_MODE_MASK		(0x3)
#define XO_EXTBUF6_MODE_SHIFT		(8)
#define XO_EXTBUF6_EN_M_ADDR		(DCXO_CW11)
#define XO_EXTBUF6_EN_M_MASK		(0x1)
#define XO_EXTBUF6_EN_M_SHIFT		(10)
#define XO_EXTBUF7_MODE_ADDR		(DCXO_CW11)
#define XO_EXTBUF7_MODE_MASK		(0x3)
#define XO_EXTBUF7_MODE_SHIFT		(11)
#define XO_EXTBUF7_EN_M_ADDR		(DCXO_CW11)
#define XO_EXTBUF7_EN_M_MASK		(0x1)
#define XO_EXTBUF7_EN_M_SHIFT		(13)

#define XO_BB_LPM_EN_M_ADDR		(DCXO_CW00)
#define XO_BB_LPM_EN_M_MASK		(0x1)
#define XO_BB_LPM_EN_M_SHIFT		(12)
#define RG_XO_HEATER_SEL_ADDR    (DCXO_CW20)
#define RG_XO_HEATER_SEL_MASK    (0x3)
#define RG_XO_HEATER_SEL_SHIFT   (1)
#define XO_BB_LPM_EN_SEL_ADDR		(DCXO_CW23)
#define XO_BB_LPM_EN_SEL_MASK		(0x1)
#define XO_BB_LPM_EN_SEL_SHIFT		(0)
#define XO_EXTBUF1_BBLPM_EN_MASK_ADDR	(DCXO_CW23)
#define XO_EXTBUF1_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF1_BBLPM_EN_MASK_SHIFT	(1)
#define XO_EXTBUF2_BBLPM_EN_MASK_ADDR	(DCXO_CW23)
#define XO_EXTBUF2_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF2_BBLPM_EN_MASK_SHIFT	(2)
#define XO_EXTBUF3_BBLPM_EN_MASK_ADDR	(DCXO_CW23)
#define XO_EXTBUF3_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF3_BBLPM_EN_MASK_SHIFT	(3)
#define XO_EXTBUF4_BBLPM_EN_MASK_ADDR	(DCXO_CW23)
#define XO_EXTBUF4_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF4_BBLPM_EN_MASK_SHIFT	(4)
#define XO_EXTBUF6_BBLPM_EN_MASK_ADDR	(DCXO_CW23)
#define XO_EXTBUF6_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF6_BBLPM_EN_MASK_SHIFT	(5)
#define XO_EXTBUF7_BBLPM_EN_MASK_ADDR	(DCXO_CW23)
#define XO_EXTBUF7_BBLPM_EN_MASK_MASK	(0x1)
#define XO_EXTBUF7_BBLPM_EN_MASK_SHIFT	(6)


#define RG_XO_EXTBUF1_HD_ADDR		(DCXO_CW15)
#define RG_XO_EXTBUF1_HD_MASK		(0x3)
#define RG_XO_EXTBUF1_HD_SHIFT	(2)
#define RG_XO_EXTBUF2_HD_ADDR		(DCXO_CW15)
#define RG_XO_EXTBUF2_HD_MASK		(0x3)
#define RG_XO_EXTBUF2_HD_SHIFT	(4)
#define RG_XO_EXTBUF3_HD_ADDR		(DCXO_CW15)
#define RG_XO_EXTBUF3_HD_MASK		(0x3)
#define RG_XO_EXTBUF3_HD_SHIFT	(6)
#define RG_XO_EXTBUF4_HD_ADDR		(DCXO_CW15)
#define RG_XO_EXTBUF4_HD_MASK		(0x3)
#define RG_XO_EXTBUF4_HD_SHIFT	(8)
#define RG_XO_EXTBUF6_HD_ADDR		(DCXO_CW15)
#define RG_XO_EXTBUF6_HD_MASK		(0x3)
#define RG_XO_EXTBUF6_HD_SHIFT	(12)
#define RG_XO_EXTBUF7_HD_ADDR		(DCXO_CW15)
#define RG_XO_EXTBUF7_HD_MASK		(0x3)
#define RG_XO_EXTBUF7_HD_SHIFT	(14)

#define XO_STATIC_AUXOUT_SEL_ADDR	(DCXO_CW18)
#define XO_STATIC_AUXOUT_SEL_MASK	(0x3f)
#define XO_STATIC_AUXOUT_SEL_SHIFT	(0)
#define XO_AUXOUT_SEL_ADDR		(DCXO_CW18)
#define XO_AUXOUT_SEL_MASK		(0x3ff)
#define XO_AUXOUT_SEL_SHIFT		(6)
#define XO_STATIC_AUXOUT_ADDR		(DCXO_CW19)
#define XO_STATIC_AUXOUT_MASK		(0xffff)
#define XO_STATIC_AUXOUT_SHIFT		(0)

#define RG_XO_EXTBUF1_ISET_ADDR		(DCXO_CW16)
#define RG_XO_EXTBUF1_ISET_MASK		(0x3)
#define RG_XO_EXTBUF1_ISET_SHIFT	(0)
#define RG_XO_EXTBUF2_ISET_ADDR		(DCXO_CW16)
#define RG_XO_EXTBUF2_ISET_MASK		(0x3)
#define RG_XO_EXTBUF2_ISET_SHIFT	(2)
#define RG_XO_EXTBUF3_ISET_ADDR		(DCXO_CW16)
#define RG_XO_EXTBUF3_ISET_MASK		(0x3)
#define RG_XO_EXTBUF3_ISET_SHIFT	(4)
#define RG_XO_EXTBUF4_ISET_ADDR		(DCXO_CW16)
#define RG_XO_EXTBUF4_ISET_MASK		(0x3)
#define RG_XO_EXTBUF4_ISET_SHIFT	(6)
#define RG_XO_EXTBUF6_ISET_ADDR		(DCXO_CW16)
#define RG_XO_EXTBUF6_ISET_MASK		(0x3)
#define RG_XO_EXTBUF6_ISET_SHIFT	(10)
#define RG_XO_EXTBUF7_ISET_ADDR		(DCXO_CW16)
#define RG_XO_EXTBUF7_ISET_MASK		(0x3)
#define RG_XO_EXTBUF7_ISET_SHIFT	(12)

static struct reg_t mt6358_debug_regs[] = {
	[0] =
		DBG_REG(dcxo_cw00, DCXO_CW00, 0xFFFF, 0)
	[1] =
		DBG_REG(dcxo_cw08, DCXO_CW08, 0xFFFF, 0)
	[2] =
		DBG_REG(dcxo_cw09, DCXO_CW09, 0xFFFF, 0)
	[3] =
		DBG_REG(dcxo_cw11, DCXO_CW11, 0xFFFF, 0)
	[4] =
		DBG_REG(dcxo_cw15, DCXO_CW15, 0xFFFF, 0)
	[5] =
		DBG_REG(dcxo_cw16, DCXO_CW16, 0xFFFF, 0)
	[6] =
		DBG_REG(dcxo_cw18, DCXO_CW18, 0xFFFF, 0)
	[7] =
		DBG_REG(dcxo_cw19, DCXO_CW19, 0xFFFF, 0)
	[8] =
		DBG_REG(dcxo_cw23, DCXO_CW23, 0xFFFF, 0)
	[9] =
		DBG_REG(NULL, NULL_ADDR, 0x0, 0x0)
};

static struct common_regs mt6358_com_regs = {
	.bblpm_auxout_sel = 24,
	.mode_num = 3,
	SET_REG_BY_NAME(static_aux_sel, XO_STATIC_AUXOUT_SEL)
	SET_REG(bblpm_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 0)
	SET_REG_BY_NAME(swbblpm_en, XO_BB_LPM_EN_M)
	SET_REG_BY_NAME(hwbblpm_sel, XO_BB_LPM_EN_SEL)
	SET_REG_BY_NAME(cdac_fpm, RG_XO_CDAC_FPM)
	SET_REG_BY_NAME(cofst_fpm, RG_XO_COFST_FPM)
	SET_REG_BY_NAME(idac_fpm, RG_XO_IDAC_FPM)
	SET_REG_BY_NAME(aac_fpm_swen, RG_XO_AAC_FPM_SWEN)
	SET_REG_BY_NAME(heater_sel, RG_XO_HEATER_SEL)
};

static struct xo_buf_t mt6358_xo_bufs[] = {
	[0] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF1_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF1_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 0)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF1_ISET)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF1_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 5,
	},
	[1] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF2_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF2_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 6)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF2_ISET)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF2_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 5,
	},
	[2] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF3_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF3_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 0)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF3_ISET)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF3_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
	[3] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF4_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF4_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 6)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF4_ISET)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF4_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
	[5] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF6_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF6_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 0)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF6_ISET)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF6_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 7,
	},
	[6] = {
		SET_REG_BY_NAME(xo_mode, XO_EXTBUF7_MODE)
		SET_REG_BY_NAME(xo_en, XO_EXTBUF7_EN_M)
		SET_REG(xo_en_auxout, XO_STATIC_AUXOUT_ADDR, 0x1, 12)
		SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF7_ISET)
		SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF7_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
	},
};

struct plat_xodata mt6358_tb_data = {
	.xo_buf_t = mt6358_xo_bufs,
	.debug_regs = mt6358_debug_regs,
	.common_regs = &mt6358_com_regs,
};
