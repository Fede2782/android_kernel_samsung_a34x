/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef _DT_BINDINGS_MML_MT6991_H
#define _DT_BINDINGS_MML_MT6991_H

/* MML engines in mt6991 */
/* The id 0 leaves empty, do not use. */
#define MML1_MMLSYS		1
#define MML1_MUTEX		2
#define MML1_RDMA0		3
#define MML1_RROT0		4
#define MML1_MERGE0		5
#define MML1_DLI2		6
#define MML1_RDMA2		7
#define MML1_DMA0_SEL		8
#define MML1_DLI0_SEL		9
#define MML1_FG0		10
#define MML1_HDR0		11
#define MML1_AAL0		12
#define MML1_PQ_AAL0_SEL	13
#define MML1_C3D0_SEL		14
#define MML1_C3D0		15
#define MML1_RSZ0		16
#define MML1_BIRSZ0		17
#define MML1_TDSHP0		18
#define MML1_COLOR0		19
#define MML1_WROT0_SEL		20
#define MML1_RSZ2		21
#define MML1_DLOUT0_SEL		22
#define MML1_WROT0		23
#define MML1_WROT2		24
#define MML1_DLO5		25
#define MML1_ENGINE_TOTAL	26
#define MML0_MMLSYS		26
#define MML0_MUTEX		27
#define MML0_RDMA0		28
#define MML0_RROT0		29
#define MML0_RDMA2		30
#define MML0_DMA0_SEL		31
#define MML0_DLI0_SEL		32
#define MML0_FG0		33
#define MML0_HDR0		34
#define MML0_AAL0		35
#define MML0_PQ_AAL0_SEL	36
#define MML0_C3D0_SEL		37
#define MML0_C3D0		38
#define MML0_RSZ0		39
#define MML0_BIRSZ0		40
#define MML0_TDSHP0		41
#define MML0_COLOR0		42
#define MML0_WROT0_SEL		43
#define MML0_RSZ2		44
#define MML0_WROT0		45
#define MML0_WROT2		46
#define MML0_DLO2		47
#define MML0_ENGINE_TOTAL	48
#define MML_ENGINE_TOTAL	48

/* MML component types. See mtk-mml-sys.c */
#define MML_CT_SYS		1
#define MML_CT_PATH		2
#define MML_CT_DL_IN		3
#define MML_CT_DL_OUT		4

/* MML SYS registers */
#define MMLSYS_MISC		0x0f0
#define MML_CG_CON0		0x100
#define MML_CG_SET0		0x104
#define MML_CG_CLR0		0x108
#define MML_CG_CON1		0x110
#define MML_CG_SET1		0x114
#define MML_CG_CLR1		0x118
#define MML_CG_CON2		0x120
#define MML_CG_SET2		0x124
#define MML_CG_CLR2		0x128
#define MML_CG_CON3		0x130
#define MML_CG_SET3		0x134
#define MML_CG_CLR3		0x138
#define MML_CG_CON4		0x140
#define MML_CG_SET4		0x144
#define MML_CG_CLR4		0x148
#define MML_SW0_RST_B		0x700
#define MML_SW1_RST_B		0x704
#define MML_SW2_RST_B		0x708
#define MML_SW3_RST_B		0x70c
#define MML_SW4_RST_B		0x710
#define MML_IN_LINE_READY_SEL	0x7fc
#define MML_SMI_LARB_GREQ	0x8dc
#define GCE_FRAME_DONE_SEL0	0xd00
#define GCE_FRAME_DONE_SEL1	0xd04
#define GCE_FRAME_DONE_SEL2	0xd08
#define GCE_FRAME_DONE_SEL3	0xd0c
#define GCE_FRAME_DONE_SEL4	0xd10
#define GCE_FRAME_DONE_SEL5	0xd14
#define GCE_FRAME_DONE_SEL6	0xd18
#define GCE_FRAME_DONE_SEL7	0xd1c
#define MML_DDREN_DEBUG		0xe20
#define MML_BYPASS_MUX_SHADOW	0xf00
#define MML_MOUT_RST		0xf04
#define MML_APU_DP_SEL		0xf08

/* MML DL IN/OUT registers in mt6991 */
#define MML_DL_IN_RELAY0_SIZE	0x220	/* OVL0_DLO1 */
#define MML_DL_IN_RELAY1_SIZE	0x224	/* OVL1_DLO1 */
#define MML_DL_OUT_RELAY0_SIZE	0x228
#define MML_DL_OUT_RELAY1_SIZE	0x22c
#define MML_DLO_ASYNC0_STATUS0	0x230
#define MML_DLO_ASYNC0_STATUS1	0x234
#define MML_DLO_ASYNC1_STATUS0	0x238
#define MML_DLO_ASYNC1_STATUS1	0x23c
#define MML_DLI_ASYNC0_STATUS0	0x240
#define MML_DLI_ASYNC0_STATUS1	0x244
#define MML_DLI_ASYNC1_STATUS0	0x248
#define MML_DLI_ASYNC1_STATUS1	0x24c
#define MML_DL_IN_RELAY2_SIZE	0x250	/* MMLSYS0_DLO2 */
#define MML_DL_OUT_RELAY4_SIZE	0x254
#define MML_DL_OUT_RELAY2_SIZE	0x258	/* MMLSYS1_DLI2 */
#define MML_DL_OUT_RELAY3_SIZE	0x25c
#define MML_DLO_ASYNC2_STATUS0	0x260
#define MML_DLO_ASYNC2_STATUS1	0x264
#define MML_DLO_ASYNC3_STATUS0	0x268
#define MML_DLO_ASYNC3_STATUS1	0x26c
#define MML_DLI_ASYNC2_STATUS0	0x270
#define MML_DLI_ASYNC2_STATUS1	0x274
#define MML_DLO_ASYNC4_STATUS0	0x278
#define MML_DLO_ASYNC4_STATUS1	0x27c
#define MML_DLO_ASYNC5_STATUS0	0x41C
#define MML_DLO_ASYNC5_STATUS1	0x420
#define MML_DL_OUT_RELAY5_SIZE	0x428

/* MML MUX registers in mt6991 */
#define MML_DLI0_SEL_IN		0xf14
#define MML_RDMA0_MOUT_EN	0xf18
#define MML_HDR0_SEL_IN		0xf1c
#define MML_HDR_MOUT_EN		0xf20
#define MML_CLA2_MOUT_EN	0xf24
#define MML_CLA2_SEL_IN		0xf28
#define MML_GCH0_SEL_IN		0xf2c
#define MML_GCH1_SEL_IN		0xf30
#define MML_WROT0_SEL_IN	0xf34
#define MML_DLOUT0_SEL_IN	0xf38
#define MML_DLO0_SOUT_SEL	0xf3c
#define MML_BYP0_SEL_IN		0xf40
#define MML_PQ0_SOUT_SEL	0xf4c
#define MML_BYP0_MOUT_EN	0xf50
#define MML_MERGE_SOUT_SEL	0xf54
#define MML_AAL0_SEL_IN		0xf58
#define MML_PQ_AAL0_SEL_IN	0xf5c
#define MML_AAL0_SOUT_SEL	0xf60
#define MML_AAL0_MOUT_EN	0xf64
#define MML_C3D0_SEL_IN		0xf68
#define MML_C3D0_SOUT_SEL	0xf6c
#define MML_TDSHP0_SOUT_SEL	0xf70
#define MML_TDSHP0_SEL_IN	0xf74
#define MML_COLOR0_MOUT_EN	0xf78
#define MML_COLOR0_SEL_IN	0xf7c
#define MML_RSZ0_SEL_IN		0xf80
#define MML_RSZ2_SEL_IN		0xf84
#define ISP0_MOUT_EN		0xf88
#define MML_DMA0_SEL_IN		0xf8c
#define MML_RDMA0_SOUT_SEL	0xf90
#define MML_RSZ0_MOUT_EN	0xf94
#define MML_FG0_SEL_IN		0xf98
#define MML_FG0_MOUT_EN		0xf9c
#define MML_CLA2_PRE_MOUT_EN	0xfa0
#define MML_DLOUT0_MOUT_EN	0xfa4

#define MML_MOUT_MASK0		0xfd0
#define MML_MOUT_MASK1		0xfd4
#define MML_MOUT_MASK2		0xfd8

/* MML AID for secure */
#define MML_RDMA0_AIDSEL	0x500
#define MML_RDMA1_AIDSEL	0x504
#define MML_WROT0_AIDSEL	0x508
#define MML_WROT1_AIDSEL	0x50c
#define MML_WROT2_AIDSEL	0x510
#define MML_WROT3_AIDSEL	0x514
#define MML_FAKE0_AIDSEL	0x518
#define MML_RDMA2_AIDSEL	0x51c

/* MMLSys debug valid/ready */
#define MML_DL_VALID0		0xfe0
#define MML_DL_VALID1		0xfe4
#define MML_DL_VALID2		0xfe8
#define MML_DL_VALID3		0xfec
#define MML_DL_READY0		0xff0
#define MML_DL_READY1		0xff4
#define MML_DL_READY2		0xff8
#define MML_DL_READY3		0xfdc

/* MML SYS mux types. See mtk-mml-sys.c */
#define MML_MUX_MOUT		1
#define MML_MUX_SOUT		2
#define MML_MUX_SLIN		3

/* GCE frame done event sel for event merge */
#define MML_EVENT_SEL_AAL0	0
#define MML_EVENT_SEL_BIRSZ0	1
#define MML_EVENT_SEL_C3D0	2
#define MML_EVENT_SEL_CLA2	3
#define MML_EVENT_SEL_COLOR0	4
#define MML_EVENT_SEL_FG0	5
#define MML_EVENT_SEL_HDR0	6
#define MML_EVENT_SEL_MERGE0	7
#define MML_EVENT_SEL_RDMA0	8
#define MML_EVENT_SEL_RDMA1	9
#define MML_EVENT_SEL_RDMA2	10
#define MML_EVENT_SEL_RROT0	11
#define MML_EVENT_SEL_RSZ0	12
#define MML_EVENT_SEL_RSZ2	13
#define MML_EVENT_SEL_TDSHP0	14
#define MML_EVENT_SEL_WROT0	15
#define MML_EVENT_SEL_WROT1	16
#define MML_EVENT_SEL_WROT2	17
#define MML_EVENT_SEL_VPPRSZ0	18
#define MML_EVENT_SEL_VPPRSZ1	19

#endif	/* _DT_BINDINGS_MML_MT6991_H */
