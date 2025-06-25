/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_V2_H__
#define __MTK_DPC_V2_H__

#include "mtk_dpc.h"

/*
 * EOF                                                      TE
 *  | OFF0 |                                     | SAFEZONE | OFF1 |
 *  |      |         OVL OFF                     |          |      | OVL OFF |
 *  |      |<-100->| DISP1 OFF           |<-100->|          |      | <-100-> | DISP1 OFF
 *  |      |<-100->| MMINFRA OFF |<-800->|       |          |      | <-100-> | MMINFRA OFF
 *  |      |       |             |       |       |          |      |         |
 *  |      OFF     OFF           ON      ON      ON         |      OFF       OFF
 *         0       4,11          12      5       1                 3         7,13
 */

#define DT_TE_60 16000
#define DT_TE_120 8000
#define DT_TE_360 2650
#define DT_TE_SAFEZONE 350
#define DT_OFF0 38000
#define DT_OFF1 500
#define DT_PRE_DISP1_OFF 100
#define DT_POST_DISP1_OFF 100
#define DT_PRE_MMINFRA_OFF 100
#define DT_POST_MMINFRA_OFF 800 /* infra267 + mminfra300 + margin */

#define DT_MMINFRA_OFFSET (DT_TE_SAFEZONE + DT_POST_DISP1_OFF + DT_POST_MMINFRA_OFF)
#define DT_DISP1_OFFSET   (DT_TE_SAFEZONE + DT_POST_DISP1_OFF)
#define DT_OVL_OFFSET     (DT_TE_SAFEZONE)
#define DT_DISP1TE_OFFSET (DT_TE_SAFEZONE - 50)
#define DT_VCORE_OFFSET   (DT_TE_SAFEZONE + 3000)

#define DT_12 (DT_TE_360 - DT_MMINFRA_OFFSET)
#define DT_5  (DT_TE_360 - DT_DISP1_OFFSET)
#define DT_1  (DT_TE_360 - DT_OVL_OFFSET)
#define DT_6  (DT_TE_360 - DT_DISP1TE_OFFSET)
#define DT_3  (DT_OFF1)
#define DT_7  (DT_OFF1 + DT_PRE_DISP1_OFF)
#define DT_13 (DT_OFF1 + DT_PRE_MMINFRA_OFF)

#define DPC2_DT_PRESZ 600
#define DPC2_DT_POSTSZ 500
#define DPC2_DT_MTCMOS 100
#define DPC2_DT_INFRA 300
#define DPC2_DT_MMINFRA 700
#define DPC2_DT_VCORE 850
#define DPC2_DT_DSION 1000
#define DPC2_DT_DSIOFF 100
#define DPC2_DT_TE_120 8300

enum mtk_dpc2_disp_vidle {
	DPC2_DISP_VIDLE_MTCMOS = 0,
	DPC2_DISP_VIDLE_MTCMOS_DISP1 = 4,
	DPC2_DISP_VIDLE_VDISP_DVFS = 8,
	DPC2_DISP_VIDLE_HRT_BW = 11,
	DPC2_DISP_VIDLE_SRT_BW = 14,
	DPC2_DISP_VIDLE_MMINFRA_OFF = 17,
	DPC2_DISP_VIDLE_INFRA_OFF = 20,
	DPC2_DISP_VIDLE_MAINPLL_OFF = 23,
	DPC2_DISP_VIDLE_MSYNC2_0 = 26,
	DPC2_DISP_VIDLE_RESERVED = 29,
	DPC2_MML_VIDLE_MTCMOS = 32,
	DPC2_MML_VIDLE_VDISP_DVFS = 36,
	DPC2_MML_VIDLE_HRT_BW = 39,
	DPC2_MML_VIDLE_SRT_BW = 42,
	DPC2_MML_VIDLE_MMINFRA_OFF = 45,
	DPC2_MML_VIDLE_INFRA_OFF = 48,
	DPC2_MML_VIDLE_MAINPLL_OFF = 51,
	DPC2_MML_VIDLE_RESERVED = 54,
	DPC2_DISP_VIDLE_26M = 57,
	DPC2_DISP_VIDLE_PMIC = 60,
	DPC2_DISP_VIDLE_VCORE = 63,
	DPC2_MML_VIDLE_26M = 66,
	DPC2_MML_VIDLE_PMIC = 69,
	DPC2_MML_VIDLE_VCORE = 72,
	DPC2_DISP_VIDLE_DSIPHY = 75,
	DPC2_VIDLE_CNT = 78
};

#endif
