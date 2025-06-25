/* SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: xiufeng li <xiufeng.li@mediatek.com>
 */

#ifndef _DT_BINDINGS_CLK_MT6991_IVI_H
#define _DT_BINDINGS_CLK_MT6991_IVI_H

/* CKSYS IVI*/
#define CLK_CK_SGMII0_REF_325M_SEL			0
#define CLK_CK_SGMII0_REG_SEL				1
#define CLK_CK_SGMII1_REF_325M_SEL			2
#define CLK_CK_SGMII1_REG_SEL				3
#define CLK_CK_GMAC_312P5M_SEL				4
#define CLK_CK_GMAC_125M_SEL				5
#define CLK_CK_GMAC_RMII_SEL				6
#define CLK_CK_GMAC_62P4M_PTP_SEL			7
#define CLK_CK_GMAC_312P5M_SEL_V0			8
#define CLK_CK_GMAC_312P5M_SEL_V1			9
#define CLK_CK_GMAC_125M_SEL_V0				10
#define CLK_CK_GMAC_125M_SEL_V1				11
#define CLK_CK_GMAC_RMII_SEL_V0				12
#define CLK_CK_GMAC_RMII_SEL_V1				13
#define CLK_CK_GMAC_62P4M_PTP_SEL_V0			14
#define CLK_CK_GMAC_62P4M_PTP_SEL_V1			15
#define CLK_CK_NET1PLL_D4				16
#define CLK_CK_NET1PLL_D5				17
#define CLK_CK_NET1PLL_D5_D5				18
#define CLK_CK_SGMIIPLL					19
#define CLK_CK_UNIVPLL_D5_D8				20
#define CLK_CK_APLL1_D3					21
#define CLK_CK_APLL2_D3					22
#define CLK_CK_IVI_NR_CLK				23

/* CKSYS2 IVI*/
#define CLK_CK2_DVO_SEL				0
#define CLK_CK2_DVO_FAVT_SEL		1
#define CLK_CK2_TVDPLL3_D2			2
#define CLK_CK2_TVDPLL3_D4			3
#define CLK_CK2_TVDPLL3_D8			4
#define CLK_CK2_TVDPLL3_D16			5
#define CLK_CK2_IVI_NR_CLK			6

/* APMIXEDSYS IVI*/
#define CLK_APMIXED_NET1PLL				0
#define CLK_APMIXED_SGMIIPLL				1
#define CLK_APMIXED_IVI_NR_CLK				2

/* DISPSYS1_CONFIG */
#define CLK_MM1_MOD6				0
#define CLK_MM1_IVI_NR_CLK				1

#endif /* _DT_BINDINGS_CLK_MT6991_IVI_H */
