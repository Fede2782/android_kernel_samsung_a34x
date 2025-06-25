// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <dt-bindings/mml/mml-mt6991.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"

#define TOPOLOGY_PLATFORM	"mt6991"
#define AAL_MIN_WIDTH		50	/* TODO: define in tile? */
/* 2k size and pixel as upper bound */
#define MML_IR_WIDTH_2K		2560
#define MML_IR_HEIGHT_2K	1440
#define MML_IR_2K		(MML_IR_WIDTH_2K * MML_IR_HEIGHT_2K)
/* hd size and pixel as lower bound */
#define MML_IR_WIDTH		640
#define MML_IR_HEIGHT		480
#define MML_IR_MIN		(MML_IR_WIDTH * MML_IR_HEIGHT)
#define MML_IR_RSZ_MIN_RATIO	375	/* resize must lower than this ratio */
#define MML_OUT_MIN_W		784	/* wqhd 1440/2+64=784 */
#define MML_DL_MAX_W		3840
#define MML_DL_MAX_H		2400
#define MML_DL_RROT_S_PX	(1920 * 1088)
#define MML_MIN_SIZE		480
#define MML_DC_MAX_DURATION_US	8300

/* use OPP index 0(229Mhz) 1(273Mhz) 2(458Mhz) */
#define MML_IR_MAX_OPP		2

/* max hrt in MB/s, see mmqos hrt support */
int mml_max_hrt = 6988;
module_param(mml_max_hrt, int, 0644);

/* 0: auto
 * 1: always
 * 1: always + pq
 */
int mml_force_rsz;
module_param(mml_force_rsz, int, 0644);

int mml_rgbrot;
module_param(mml_rgbrot, int, 0644);

int mml_path_mode;
module_param(mml_path_mode, int, 0644);

/* debug param
 * 0: (default)don't care, check dts property to enable racing
 * 1: force enable
 * 2: force disable
 */
int mml_racing;
module_param(mml_racing, int, 0644);

/* debug param
 * 0: (default)don't care, check dts property to enable racing
 * 1: force auto query (skip dts option check)
 * 2: force disable
 * 3: force enable
 */
int mml_dl;
module_param(mml_dl, int, 0644);

int mml_opp_check = 1;
module_param(mml_opp_check, int, 0644);

int mml_rrot;
module_param(mml_rrot, int, 0644);

/* Single RROT support
 * 0: (default)auto query
 * 1: force enable and don't care performance
 * 2: force disable
 */
int mml_rrot_single = 2;
module_param(mml_rrot_single, int, 0644);

int mml_racing_rsz = 1;
module_param(mml_racing_rsz, int, 0644);

#ifndef MML_FPGA
int mml_dpc = 1;
#else
int mml_dpc;
#endif
module_param(mml_dpc, int, 0644);

/* 0: off
 * 1: on
 */
int mml_binning = 1;
module_param(mml_binning, int, 0644);

int mml_shadow = 1;
module_param(mml_shadow, int, 0644);

struct path_node {
	u8 eng;
	u8 next0;
	u8 next1;
};

/* !!Following code generate by topology parser (tpparser.py)!!
 * include: topology_scenario, path_map, engine_reset_bit
 */
enum topology_scenario {
	PATH_MML1_NOPQ = 0,
	PATH_MML1_RSZ,
	PATH_MML1_PQ,
	PATH_MML1_AIPQ,
	PATH_MML1_PQ_HDR,
	PATH_MML1_RR_NOPQ,
	PATH_MML1_RR_RSZ,
	PATH_MML1_RR_PQ,
	PATH_MML1_RR2_PQ,
	PATH_MML1_DL_NOPQ,
	PATH_MML1_DL_RSZ,
	PATH_MML1_DL,
	PATH_MML1_DL_AIPQ,
	PATH_MML1_DL_HDR,
	PATH_MML1_DL2_NOPQ,
	PATH_MML1_DL2_RSZ,
	PATH_MML1_DL2,
	PATH_MML1_DL2_AIPQ,
	PATH_MML1_DL2_HDR,
	PATH_MML0_NOPQ,
	PATH_MML0_RSZ,
	PATH_MML0_PQ,
	PATH_MML0_AIPQ,
	PATH_MML0_PQ_HDR,
	PATH_MML_MAX
};

static const struct path_node path_map[PATH_MML_MAX][MML_MAX_PATH_NODES] = {
	[PATH_MML1_NOPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RDMA0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_RSZ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RDMA0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_PQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RDMA0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_FG0,},
		{MML1_FG0, MML1_HDR0,},
		{MML1_HDR0, MML1_AAL0,},
		{MML1_AAL0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_AIPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RDMA0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL, MML1_RSZ2,},
		{MML1_DLI0_SEL, MML1_FG0,},
		{MML1_FG0, MML1_HDR0,},
		{MML1_HDR0, MML1_AAL0,},
		{MML1_AAL0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_RDMA2, MML1_BIRSZ0,},
		{MML1_BIRSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML1_RSZ2, MML1_WROT2,},
		{MML1_WROT0,},
		{MML1_WROT2,},
	},
	[PATH_MML1_PQ_HDR] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RDMA0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_FG0,},
		{MML1_FG0, MML1_HDR0,},
		{MML1_HDR0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_AAL0,},
		{MML1_AAL0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_RR_NOPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_RR_RSZ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_RR_PQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_FG0,},
		{MML1_FG0, MML1_HDR0,},
		{MML1_HDR0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_AAL0,},
		{MML1_AAL0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML1_WROT0,},
	},
	[PATH_MML1_RR2_PQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML1_RROT0, MML1_MERGE0,},
		{MML0_RROT0, MML0_DLO2,},
		{MML1_DLI2, MML1_MERGE0,},
		{MML1_MERGE0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_FG0,},
		{MML1_FG0, MML1_HDR0,},
		{MML1_HDR0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_AAL0,},
		{MML1_AAL0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_WROT0,},
		{MML0_DLO2,},
		{MML1_WROT0,},
	},
	[PATH_MML1_DL_NOPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL_RSZ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_HDR0,},
		{MML1_HDR0, MML1_AAL0,},
		{MML1_AAL0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL_AIPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL, MML1_RSZ2,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_HDR0,},
		{MML1_HDR0, MML1_AAL0,},
		{MML1_AAL0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_TDSHP0,},
		{MML1_RDMA2, MML1_BIRSZ0,},
		{MML1_BIRSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML1_RSZ2, MML1_WROT2,},
		{MML1_DLO5,},
		{MML1_WROT2,},
	},
	[PATH_MML1_DL_HDR] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML1_RROT0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_HDR0,},
		{MML1_HDR0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_AAL0,},
		{MML1_AAL0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL2_NOPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML1_RROT0, MML1_MERGE0,},
		{MML0_RROT0, MML0_DLO2,},
		{MML1_DLI2, MML1_MERGE0,},
		{MML1_MERGE0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML0_DLO2,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL2_RSZ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML1_RROT0, MML1_MERGE0,},
		{MML0_RROT0, MML0_DLO2,},
		{MML1_DLI2, MML1_MERGE0,},
		{MML1_MERGE0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML0_DLO2,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL2] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML1_RROT0, MML1_MERGE0,},
		{MML0_RROT0, MML0_DLO2,},
		{MML1_DLI2, MML1_MERGE0,},
		{MML1_MERGE0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_HDR0,},
		{MML1_HDR0, MML1_AAL0,},
		{MML1_AAL0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML0_DLO2,},
		{MML1_DLO5,},
	},
	[PATH_MML1_DL2_AIPQ] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML1_RROT0, MML1_MERGE0,},
		{MML0_RROT0, MML0_DLO2,},
		{MML1_DLI2, MML1_MERGE0,},
		{MML1_MERGE0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL, MML1_RSZ2,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_HDR0,},
		{MML1_HDR0, MML1_AAL0,},
		{MML1_AAL0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_TDSHP0,},
		{MML1_RDMA2, MML1_BIRSZ0,},
		{MML1_BIRSZ0, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML1_RSZ2, MML1_WROT2,},
		{MML0_DLO2,},
		{MML1_DLO5,},
		{MML1_WROT2,},
	},
	[PATH_MML1_DL2_HDR] = {
		{MML1_MMLSYS,},
		{MML1_MUTEX,},
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML1_RROT0, MML1_MERGE0,},
		{MML0_RROT0, MML0_DLO2,},
		{MML1_DLI2, MML1_MERGE0,},
		{MML1_MERGE0, MML1_DMA0_SEL,},
		{MML1_DMA0_SEL, MML1_DLI0_SEL,},
		{MML1_DLI0_SEL, MML1_RSZ0,},
		{MML1_RSZ0, MML1_HDR0,},
		{MML1_HDR0, MML1_C3D0_SEL,},
		{MML1_C3D0_SEL, MML1_C3D0,},
		{MML1_C3D0, MML1_AAL0,},
		{MML1_AAL0, MML1_PQ_AAL0_SEL,},
		{MML1_PQ_AAL0_SEL, MML1_TDSHP0,},
		{MML1_TDSHP0, MML1_COLOR0,},
		{MML1_COLOR0, MML1_WROT0_SEL,},
		{MML1_WROT0_SEL, MML1_DLOUT0_SEL,},
		{MML1_DLOUT0_SEL, MML1_DLO5,},
		{MML0_DLO2,},
		{MML1_DLO5,},
	},
	[PATH_MML0_NOPQ] = {
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML0_RDMA0, MML0_DMA0_SEL,},
		{MML0_DMA0_SEL, MML0_WROT0,},
		{MML0_WROT0,},
	},
	[PATH_MML0_RSZ] = {
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML0_RDMA0, MML0_DMA0_SEL,},
		{MML0_DMA0_SEL, MML0_DLI0_SEL,},
		{MML0_DLI0_SEL, MML0_RSZ0,},
		{MML0_RSZ0, MML0_WROT0_SEL,},
		{MML0_WROT0_SEL, MML0_WROT0,},
		{MML0_WROT0,},
	},
	[PATH_MML0_PQ] = {
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML0_RDMA0, MML0_DMA0_SEL,},
		{MML0_DMA0_SEL, MML0_DLI0_SEL,},
		{MML0_DLI0_SEL, MML0_FG0,},
		{MML0_FG0, MML0_HDR0,},
		{MML0_HDR0, MML0_AAL0,},
		{MML0_AAL0, MML0_C3D0_SEL,},
		{MML0_C3D0_SEL, MML0_C3D0,},
		{MML0_C3D0, MML0_RSZ0,},
		{MML0_RSZ0, MML0_TDSHP0,},
		{MML0_TDSHP0, MML0_COLOR0,},
		{MML0_COLOR0, MML0_WROT0_SEL,},
		{MML0_WROT0_SEL, MML0_WROT0,},
		{MML0_WROT0,},
	},
	[PATH_MML0_AIPQ] = {
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML0_RDMA0, MML0_DMA0_SEL,},
		{MML0_DMA0_SEL, MML0_DLI0_SEL, MML0_RSZ2,},
		{MML0_DLI0_SEL, MML0_FG0,},
		{MML0_FG0, MML0_HDR0,},
		{MML0_HDR0, MML0_AAL0,},
		{MML0_AAL0, MML0_C3D0_SEL,},
		{MML0_C3D0_SEL, MML0_C3D0,},
		{MML0_C3D0, MML0_RSZ0,},
		{MML0_RSZ0, MML0_TDSHP0,},
		{MML0_RDMA2, MML0_BIRSZ0,},
		{MML0_BIRSZ0, MML0_TDSHP0,},
		{MML0_TDSHP0, MML0_COLOR0,},
		{MML0_COLOR0, MML0_WROT0_SEL,},
		{MML0_WROT0_SEL, MML0_WROT0,},
		{MML0_RSZ2, MML0_WROT2,},
		{MML0_WROT0,},
		{MML0_WROT2,},
	},
	[PATH_MML0_PQ_HDR] = {
		{MML0_MMLSYS,},
		{MML0_MUTEX,},
		{MML0_RDMA0, MML0_DMA0_SEL,},
		{MML0_DMA0_SEL, MML0_DLI0_SEL,},
		{MML0_DLI0_SEL, MML0_FG0,},
		{MML0_FG0, MML0_HDR0,},
		{MML0_HDR0, MML0_C3D0_SEL,},
		{MML0_C3D0_SEL, MML0_C3D0,},
		{MML0_C3D0, MML0_AAL0,},
		{MML0_AAL0, MML0_PQ_AAL0_SEL,},
		{MML0_PQ_AAL0_SEL, MML0_RSZ0,},
		{MML0_RSZ0, MML0_TDSHP0,},
		{MML0_TDSHP0, MML0_COLOR0,},
		{MML0_COLOR0, MML0_WROT0_SEL,},
		{MML0_WROT0_SEL, MML0_WROT0,},
		{MML0_WROT0,},
	},
};

/* reset bit to each engine,
 * reverse of MMSYS_SW0_RST_B and MMSYS_SW1_RST_B
 */
static u8 engine_reset_bit[MML_ENGINE_TOTAL] = {
	[MML1_MUTEX] = 0,
	[MML1_MMLSYS] = 2,
	[MML1_RDMA0] = 3,
	[MML1_RDMA2] = 5,
	[MML1_BIRSZ0] = 6,
	[MML1_HDR0] = 7,
	[MML1_AAL0] = 8,
	[MML1_RSZ0] = 9,
	[MML1_RSZ2] = 10,
	[MML1_TDSHP0] = 11,
	[MML1_COLOR0] = 12,
	[MML1_WROT0] = 13,
	[MML1_WROT2] = 15,
	[MML1_DLI2] = 22,
	[MML1_RROT0] = 26,
	[MML1_MERGE0] = 27,
	[MML1_C3D0] = 28,
	[MML1_FG0] = 29,
	[MML1_DLO5] = 34,
	[MML0_MUTEX] = 0,
	[MML0_MMLSYS] = 2,
	[MML0_RDMA0] = 3,
	[MML0_RDMA2] = 5,
	[MML0_BIRSZ0] = 6,
	[MML0_HDR0] = 7,
	[MML0_AAL0] = 8,
	[MML0_RSZ0] = 9,
	[MML0_RSZ2] = 10,
	[MML0_TDSHP0] = 11,
	[MML0_COLOR0] = 12,
	[MML0_WROT0] = 13,
	[MML0_WROT2] = 15,
	[MML0_DLO2] = 23,
	[MML0_RROT0] = 26,
	[MML0_C3D0] = 28,
	[MML0_FG0] = 29,
};
/* !!Above code generate by topology parser (tpparser.py)!! */

static inline bool engine_input(u32 id)
{
	return id == MML1_RDMA0 || id == MML1_RDMA2 ||
		id == MML1_RROT0 || id == MML1_DLI2 ||
		id == MML0_RDMA0 || id == MML0_RDMA2 ||
		id == MML0_RROT0;
}

/* check if engine is output dma engine */
static inline bool engine_wrot(u32 id)
{
	return id == MML1_WROT0 || id == MML1_WROT2 ||
		id == MML0_WROT0 || id == MML0_WROT2;
}

/* check if engine is input region pq rdma engine */
static inline bool engine_pq_rdma(u32 id)
{
	return id == MML1_RDMA2 || id == MML0_RDMA2;
}

/* check if engine is input region pq birsz engine */
static inline bool engine_pq_birsz(u32 id)
{
	return id == MML1_BIRSZ0 || id == MML0_BIRSZ0;
}

/* check if engine is region pq engine */
static inline bool engine_region_pq(u32 id)
{
	return id == MML1_RDMA2 || id == MML1_BIRSZ0 ||
		id == MML0_RDMA2 || id == MML0_BIRSZ0;
}

/* check if engine is dma engine */
static inline bool engine_dma(u32 id)
{
	return engine_input(id) || engine_wrot(id) ||
		id == MML1_FG0 || id == MML0_FG0;
}

static inline bool engine_tdshp(u32 id)
{
	return id == MML1_TDSHP0 || id == MML0_TDSHP0;
}

static inline bool scene_is_dc2(enum topology_scenario scene)
{
	return scene >= PATH_MML0_NOPQ && scene <= PATH_MML0_PQ_HDR;
}

static inline u32 engine_id_to_sys(u32 id)
{
	if (id >= MML1_MMLSYS && id <= MML1_ENGINE_TOTAL)
		return mml_sys_frame;
	return mml_sys_tile;
}

static inline bool scene_is_front_rsz(enum topology_scenario scene)
{
	return scene >= PATH_MML1_DL && scene <= PATH_MML1_DL2_HDR;
}

enum cmdq_clt_usage {
	MML_CLT_PIPE0,
	MML_CLT_PIPE1,
	MML_CLT_MAX
};

static const u8 clt_dispatch[PATH_MML_MAX] = {
	[PATH_MML1_NOPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_RSZ]		= MML_CLT_PIPE0,
	[PATH_MML1_PQ]		= MML_CLT_PIPE0,
	[PATH_MML1_AIPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_PQ_HDR]	= MML_CLT_PIPE0,
	[PATH_MML1_RR_NOPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_RR_RSZ]	= MML_CLT_PIPE0,
	[PATH_MML1_RR_PQ]	= MML_CLT_PIPE0,
	[PATH_MML1_RR2_PQ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL_NOPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL_RSZ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL]		= MML_CLT_PIPE0,
	[PATH_MML1_DL_AIPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL_HDR]	= MML_CLT_PIPE0,
	[PATH_MML1_DL2_NOPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL2_RSZ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL2]		= MML_CLT_PIPE0,
	[PATH_MML1_DL2_AIPQ]	= MML_CLT_PIPE0,
	[PATH_MML1_DL2_HDR]	= MML_CLT_PIPE0,
	[PATH_MML0_NOPQ]	= MML_CLT_PIPE1,
	[PATH_MML0_RSZ]		= MML_CLT_PIPE1,
	[PATH_MML0_PQ]		= MML_CLT_PIPE1,
	[PATH_MML0_AIPQ]	= MML_CLT_PIPE1,
	[PATH_MML0_PQ_HDR]	= MML_CLT_PIPE1,
};

/* mux sof group of mmlsys mout/sel */
enum mux_sof_group {
	MUX_SOF_GRP0 = 0,
	MUX_SOF_GRP1,
	MUX_SOF_GRP2,
	MUX_SOF_GRP3,
	MUX_SOF_GRP4,
	MUX_SOF_GRP5,
	MUX_SOF_GRP6,
	MUX_SOF_GRP7,
};

static const u8 grp_dispatch[PATH_MML_MAX] = {
	[PATH_MML1_NOPQ]	= MUX_SOF_GRP3,
	[PATH_MML1_RSZ]		= MUX_SOF_GRP3,
	[PATH_MML1_PQ]		= MUX_SOF_GRP3,
	[PATH_MML1_AIPQ]	= MUX_SOF_GRP3,
	[PATH_MML1_PQ_HDR]	= MUX_SOF_GRP3,
	[PATH_MML1_RR_NOPQ]	= MUX_SOF_GRP1,
	[PATH_MML1_RR_RSZ]	= MUX_SOF_GRP1,
	[PATH_MML1_RR_PQ]	= MUX_SOF_GRP1,
	[PATH_MML1_RR2_PQ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL_NOPQ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL_RSZ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL]		= MUX_SOF_GRP1,
	[PATH_MML1_DL_AIPQ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL_HDR]	= MUX_SOF_GRP1,
	[PATH_MML1_DL2_NOPQ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL2_RSZ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL2]		= MUX_SOF_GRP1,
	[PATH_MML1_DL2_AIPQ]	= MUX_SOF_GRP1,
	[PATH_MML1_DL2_HDR]	= MUX_SOF_GRP1,
	[PATH_MML0_NOPQ]	= MUX_SOF_GRP2,
	[PATH_MML0_RSZ]		= MUX_SOF_GRP2,
	[PATH_MML0_PQ]		= MUX_SOF_GRP2,
	[PATH_MML0_AIPQ]	= MUX_SOF_GRP2,
	[PATH_MML0_PQ_HDR]	= MUX_SOF_GRP2,
};

/* 6.6 ms as dc mode active time threshold by:
 * 1 / (fps * vblank) = 1000000 / 120 / 1.25 = 6666us
 */
#define MML_DC_ACT_DUR	6600
static u32 opp_pixel_table[MML_MAX_OPPS];

static void tp_dump_path(const struct mml_topology_path *path)
{
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		mml_log(
			"[topology]%u engine %u (%p) prev %p %p next %p %p comp %p tile idx %u out %u",
			i, path->nodes[i].id, &path->nodes[i],
			path->nodes[i].prev[0], path->nodes[i].prev[1],
			path->nodes[i].next[0], path->nodes[i].next[1],
			path->nodes[i].comp,
			path->nodes[i].tile_eng_idx,
			path->nodes[i].out_idx);
	}
}

static void tp_dump_path_short(struct mml_topology_path *path, bool shadow, bool dpc)
{
	char path_desc[64];
	u32 len = 0;
	u8 i;

	for (i = 0; i < path->node_cnt; i++)
		len += snprintf(path_desc + len, sizeof(path_desc) - len, " %u",
			path->nodes[i].id);
	mml_log("[topology]path:%u engines:%s%s%s",
		path->path_id, path_desc, shadow ? " shadow" : "", dpc ? " dpc" : "");
}

static void tp_parse_connect_prev(const struct path_node *route, struct mml_path_node *nodes,
	u8 cur_idx)
{
	u32 i;
	u32 in_idx = 0;	/* current engine input index */
	u32 eng_id = nodes[cur_idx].id;

	for (i = 0; i < cur_idx && in_idx < 2; i++) {
		u32 prev_out_idx;	/* previous engine output index */

		if (route[i].next0 == eng_id)
			prev_out_idx = 0;
		else if (route[i].next1 == eng_id)
			prev_out_idx = 1;
		else
			continue;

		nodes[i].next[prev_out_idx] = &nodes[cur_idx];
		nodes[cur_idx].prev[in_idx++] = &nodes[i];

		if (nodes[i].out_idx || prev_out_idx)
			nodes[cur_idx].out_idx = 1;
	}

	if (!in_idx && !engine_input(eng_id))
		mml_err("[topology]connect fail idx:%u engine:%u", i, eng_id);
}

static void tp_parse_path(struct mml_dev *mml, struct mml_topology_path *path,
	const struct path_node *route)
{
	u8 i, tile_idx, out_eng_idx;
	struct mml_path_node *pq_rdma = NULL;
	struct mml_path_node *pq_birsz = NULL;

	for (i = 0; i < MML_MAX_PATH_NODES; i++) {
		const u8 eng = route[i].eng;
		const u8 sysid = engine_id_to_sys(eng);

		if (!route[i].eng)
			break;

		/* assign current engine */
		path->nodes[i].id = eng;
		path->nodes[i].comp = mml_dev_get_comp_by_id(mml, eng);
		if (!path->nodes[i].comp)
			mml_err("[topology]no comp idx:%u engine:%u", i, eng);

		/* assign reset bits for this path */
		path->reset_bits_sys[sysid] |= 1LL << engine_reset_bit[eng];
		path->engine_flags |= 1LL << eng;

		if (eng == MML1_MMLSYS) {
			path->mmlsys2 = path->mmlsys;
			path->mmlsys2_idx = path->mmlsys_idx;
			path->mmlsys = path->nodes[i].comp;
			path->mmlsys_idx = i;
			path->sys_en[mml_sys_tile] = path->sys_en[mml_sys_frame];
			path->sys_en[mml_sys_frame] = true;
			continue;
		} else if (eng == MML0_MMLSYS) {
			if (!path->mmlsys) {
				path->mmlsys = path->nodes[i].comp;
				path->mmlsys_idx = i;
			} else {
				path->mmlsys2 = path->nodes[i].comp;
				path->mmlsys2_idx = i;
			}
			path->sys_en[mml_sys_tile] = true;
			continue;
		} else if (eng == MML1_MUTEX) {
			path->mutex2 = path->mutex;
			path->mutex2_idx = path->mutex_idx;
			path->mutex = path->nodes[i].comp;
			path->mutex_idx = i;
			continue;
		} else if (eng == MML0_MUTEX) {
			if (!path->mutex) {
				path->mutex = path->nodes[i].comp;
				path->mutex_idx = i;
			} else {
				path->mutex2 = path->nodes[i].comp;
				path->mutex2_idx = i;
			}
			continue;
		} else if (engine_pq_rdma(eng)) {
			pq_rdma = &path->nodes[i];
			continue;
		} else if (engine_pq_birsz(eng)) {
			pq_birsz = &path->nodes[i];
			continue;
		} else if (engine_tdshp(eng)) {
			path->tdshp_id = eng;
			if (pq_rdma && pq_birsz) {
				pq_birsz->prev[0] = pq_rdma;
				pq_rdma->next[0] = pq_birsz;
				pq_birsz->next[0] = &path->nodes[i];
				path->nodes[i].prev[1] = pq_birsz;
			}
		}

		/* find and connect previous engine to current node */
		tp_parse_connect_prev(route, path->nodes, i);

		/* for svp aid binding */
		if (engine_dma(eng) && path->aid_eng_cnt < MML_MAX_AID_COMPS)
			path->aid_engine_sys[sysid].ids[path->aid_engine_sys[sysid].cnt++] = eng;
	}
	path->node_cnt = i;

	/* 0: reset
	 * 1: not reset
	 * so we need to reverse the bits
	 */
	for (i = 0; i < mml_max_sys; i++)
		path->reset_bits_sys[i] = ~path->reset_bits_sys[i];
	mml_msg("[topology]reset bits sys0 %#llx sys1 %#llx engine %#018llx",
		path->reset_bits_sys[0], path->reset_bits_sys[1], path->engine_flags);

	/* collect tile engines */
	tile_idx = 0;
	for (i = 0; i < path->node_cnt; i++) {
		if ((!path->nodes[i].prev[0] && !path->nodes[i].next[0]) ||
		    engine_region_pq(path->nodes[i].id)) {
			path->nodes[i].tile_eng_idx = ~0;
			continue;
		}

		/* assume mml1_rrot0 always tile idx 0 */
		if (path->sys_en[mml_sys_frame] &&
			(path->nodes[i].comp->sysid != mml_sys_frame ||
			path->nodes[i].id == MML1_DLI2)) {
			path->nodes[i].tile_eng_idx = 0;
			continue;
		}

		path->nodes[i].tile_eng_idx = tile_idx;
		path->tile_engines[tile_idx++] = i;
	}
	path->tile_engine_cnt = tile_idx;

	/* scan region pq in engines */
	for (i = 0; i < path->node_cnt; i++) {
		if (engine_pq_rdma(path->nodes[i].id)) {
			path->nodes[i].tile_eng_idx = path->tile_engine_cnt;
			if (path->tile_engine_cnt < MML_MAX_PATH_NODES)
				path->tile_engines[path->tile_engine_cnt] = i;
			else
				mml_err("[topology]RDMA tile_engines idx %d >= MML_MAX_PATH_NODES",
					path->tile_engine_cnt);
			if (path->pq_rdma_id)
				mml_err("[topology]multiple pq rdma engines: was %hhu now %hhu",
					path->pq_rdma_id,
					path->nodes[i].id);
			path->pq_rdma_id = path->nodes[i].id;
		} else if (engine_pq_birsz(path->nodes[i].id)) {
			path->nodes[i].tile_eng_idx = path->tile_engine_cnt + 1;
			if (path->tile_engine_cnt + 1 < MML_MAX_PATH_NODES)
				path->tile_engines[path->tile_engine_cnt + 1] = i;
			else
				mml_err("[topology]BIRSZ tile_engines idx %d >= MML_MAX_PATH_NODES",
					path->tile_engine_cnt + 1);
		}
	}

	/* scan out engines */
	for (i = 0; i < path->node_cnt; i++) {
		if (!engine_wrot(path->nodes[i].id))
			continue;
		out_eng_idx = path->nodes[i].out_idx;
		if (path->out_engine_ids[out_eng_idx])
			mml_err("[topology]multiple out engines: was %u now %u on out idx:%u",
				path->out_engine_ids[out_eng_idx],
				path->nodes[i].id, out_eng_idx);
		path->out_engine_ids[out_eng_idx] = path->nodes[i].id;
	}
}

static s32 tp_init_cache(struct mml_dev *mml, struct mml_topology_cache *cache,
	struct cmdq_client **clts, u32 clt_cnt)
{
	struct mml_comp *comp;
	u32 i;

	if (clt_cnt < MML_CLT_MAX) {
		mml_err("[topology]%s not enough cmdq clients to all paths",
			__func__);
		return -ECHILD;
	}
	if (ARRAY_SIZE(cache->paths) < PATH_MML_MAX) {
		mml_err("[topology]%s not enough path cache for all paths",
			__func__);
		return -ECHILD;
	}

	/* assign sys id for mmlsys/mutex compse different behavior */
	for (i = MML1_MMLSYS; i < MML1_ENGINE_TOTAL; i++) {
		comp = mml_dev_get_comp_by_id(mml, i);
		if (comp)
			comp->sysid = mml_sys_frame;
		else
			return -EAGAIN;
	}
	for (i = MML0_MMLSYS; i < MML0_ENGINE_TOTAL; i++) {
		comp = mml_dev_get_comp_by_id(mml, i);
		if (comp)
			comp->sysid = mml_sys_tile;
		else
			return -EAGAIN;
	}

	for (i = 0; i < PATH_MML_MAX; i++) {
		struct mml_topology_path *path = &cache->paths[i];

		path->path_id = i;
		tp_parse_path(mml, path, path_map[i]);
		if (mtk_mml_msg) {
			mml_log("[topology]dump path %u count %u clt id %u",
				i, path->node_cnt, clt_dispatch[i]);
			tp_dump_path(path);
		}

		/* now dispatch cmdq client (channel) to path */
		path->clt = clts[clt_dispatch[i]];
		path->clt_id = clt_dispatch[i];
		path->mux_group = grp_dispatch[i];
	}

	if (mml_get_chip_swver(mml) == 0)
		mml_stash = 0;

	return 0;
}

static inline bool tp_need_resize(struct mml_frame_info *info, bool *can_binning)
{
	u32 w = info->dest[0].data.width;
	u32 h = info->dest[0].data.height;
	u32 cw = info->dest[0].crop.r.width;
	u32 ch = info->dest[0].crop.r.height;

	if (info->dest[0].rotate == MML_ROT_90 ||
		info->dest[0].rotate == MML_ROT_270)
		swap(w, h);

	mml_msg("[topology]%s target %ux%u crop %ux%u",
		__func__, w, h, cw, ch);

	/* default binning off */
	if (can_binning)
		*can_binning = false;

	/* for binning */
	if (mml_binning && info->mode == MML_MODE_DIRECT_LINK) {
		if (can_binning && (cw >= w * 2 || ch >= h * 2) &&
			MML_FMT_YUV420(info->src.format)) {
			*can_binning = true;
			if (cw >= w * 2)
				cw = cw / 2;
			if (ch >= h * 2)
				ch = ch / 2;
		}
	}

	return info->dest_cnt != 1 ||
		cw != w || ch != h ||
		info->dest[0].crop.x_sub_px ||
		info->dest[0].crop.y_sub_px ||
		info->dest[0].crop.w_sub_px ||
		info->dest[0].crop.h_sub_px ||
		info->dest[0].compose.width != info->dest[0].data.width ||
		info->dest[0].compose.height != info->dest[0].data.height;
}

static bool tp_check_tput_dl(struct mml_frame_info *info, struct mml_topology_cache *tp,
	u32 panel_width, u32 panel_height, bool *dual, struct mml_frame_info_cache *info_cache)
{
	u32 srcw, srch, tputw, tputh, pixel;
	u32 cropw = info->dest[0].crop.r.width;
	u32 croph = info->dest[0].crop.r.height;
	u32 destw = info->dest[0].data.width;
	u32 desth = info->dest[0].data.height;
	const enum mml_orientation rotate = info->dest[0].rotate;
	const u32 plane = MML_FMT_PLANE(info->src.format);
	u64 tput, hrt;
	u32 i;

	/* always assign dual as default */
	*dual = mml_rrot_single == 1 ? false : true;

	/* disp not provide act time, assume throughput ok */
	if (!info->act_time)
		return true;

	if (!tp->qos[mml_sys_frame].opp_cnt) {
		mml_err("no opp table support");
		return false;
	}

	srcw = round_up(info->dest[0].crop.r.left + info->dest[0].crop.r.width, 32) -
		round_down(info->dest[0].crop.r.left, 32);
	srch = round_up(info->dest[0].crop.r.top + info->dest[0].crop.r.height, 16) -
		round_down(info->dest[0].crop.r.top, 16);
	tputw = srcw;
	tputh = srch;

	if (rotate == MML_ROT_90 || rotate == MML_ROT_270)
		swap(tputw, tputh);

	/* binning case */
	if (MML_FMT_YUV420(info->src.format)) {
		if (rotate == MML_ROT_0 || rotate == MML_ROT_180) {
			if ((croph >> 1) >= desth)
				tputh = tputh >> 1;
		} else {
			if ((cropw >> 1) >= desth)
				tputh = tputh >> 1;
		}
	}
	tputw = tputw / 2;	/* for rrot 1t2p */
	tputh += 32;		/* for rrot max latency */

	/* path after rsz 1t2p if not aipq */
	if (!info->dest[0].pq_config.en_region_pq)
		destw = destw / 2;

	/* not support if exceeding max throughput
	 * pixel per-pipe is:
	 *	pipe_pixel = pixel * 1.1
	 * note that the 1t2p already contained in tputw before rotate and destw,
	 * and necessary throughput:
	 *	pipe_pixel / active_time(ns) * 1000
	 * so merge all constant:
	 *	tput = pixel  * 11 / 10 / (act_time / 1000)
	 */
	pixel = max(tputw, destw) * max(tputh, desth) * 11 / 10;
	tput = pixel / (info->act_time / 1000);
	if (panel_width > destw)
		tput = tput * panel_width / destw;
	if (mml_rrot_single != 2 && tputw * tputh <= MML_DL_RROT_S_PX &&
		tput < tp->qos[mml_sys_frame].opp_speeds[1]) {
		*dual = false;
		if (info_cache)
			info_cache->dl_opp = 1;
		goto check_hrt;
	}

	pixel = max(tputw / 2, destw) * max(tputh, desth) * 11 / 10;
	tput = pixel / (info->act_time / 1000);
	if (tput < tp->qos[mml_sys_frame].opp_speeds[tp->qos[mml_sys_frame].opp_cnt - 1]) {
		*dual = mml_rrot_single == 1 ? false : true;
		goto find_opp;
	}

	mml_msg("%s pixel %u tput %llu %u %u dest %u %u",
		__func__, pixel, tput, tputw, tputh, destw, desth);

	return false;

find_opp:
	if (info_cache) {
		for (i = 0; i < tp->qos[mml_sys_frame].opp_cnt; i++) {
			if (tput <= tp->qos[mml_sys_frame].opp_speeds[i])
				break;
		}
		info_cache->dl_opp = i;
	}

check_hrt:
	/* calculate source data size as bandwidth */
	hrt = mml_color_get_min_y_size(info->src.format, srcw, srch);
	if (!MML_FMT_COMPRESS(info->src.format) && plane > 1)
		hrt += (u64)mml_color_get_min_uv_size(info->src.format, srcw, srch) * (plane - 1);
	hrt = hrt * 1000 / info->act_time;

	/* check if info cache provide in only query mode path
	 * skip log in select topology path
	 */
	if (info_cache)
		mml_msg("%s pixel %u opp %u tput %llu %u %u act %u dest %u %u hrt %llu",
			__func__, pixel, info_cache->dl_opp, tput, tputw, tputh,
			info->act_time, destw, desth, hrt);

	if (hrt <= mml_max_hrt)
		return true;
	return false;
}

static bool tp_check_tput_dc(struct mml_frame_info *info, struct mml_topology_cache *tp,
	u32 panel_width, u32 panel_height, struct mml_frame_info_cache *info_cache)
{
	u32 srcw, srch, tputw, tputh;
	u32 destw = info->dest[0].data.width;
	u32 desth = info->dest[0].data.height;
	const enum mml_orientation rotate = info->dest[0].rotate;
	u32 max_clock, pixel, tput;
	u32 i;

	if (!info_cache) {
		/* not checking throughput */
		return true;
	}

	if (!tp || !tp->qos[mml_sys_frame].opp_cnt) {
		mml_err("no opp table support");
		return false;
	}

	srcw = round_up(info->dest[0].crop.r.left + info->dest[0].crop.r.width, 32) -
		round_down(info->dest[0].crop.r.left, 32);
	srch = round_up(info->dest[0].crop.r.top + info->dest[0].crop.r.height, 16) -
		round_down(info->dest[0].crop.r.top, 16);
	tputw = srcw;
	tputh = srch;

	/* rotate destination for data path requirement */
	if (rotate == MML_ROT_90 || rotate == MML_ROT_270)
		swap(destw, desth);

	/* for rrot 1t2p */
	tputw = tputw / 2;

	/* path after rsz 1t2p if not aipq */
	if (!info->dest[0].pq_config.en_region_pq)
		destw = destw / 2;

	/* not support if exceeding max throughput
	 * pixel per-pipe is:
	 *	pipe_pixel = pixel * 1.1
	 * note that the 1t2p already contained in tputw and destw before rotate,
	 * and necessary throughput:
	 *	pipe_pixel / duration
	 * so merge all constant:
	 *	tput = pixel  * 11 / 10 / duration
	 * and back to min necessary duration:
	 *	duration = pixel * 11 / 10 / max_clock
	 */
	pixel = max(tputw, destw) * max(tputh, desth) * 11 / 10;
	max_clock = tp->qos[mml_sys_frame].opp_speeds[tp->qos[mml_sys_frame].opp_cnt - 1];
	info_cache->pixels = pixel;
	info_cache->duration = pixel / max_clock;
	if (info_cache->duration > MML_DC_MAX_DURATION_US)
		return false;

	tput = pixel / info_cache->remain;
	for (i = 0; i < tp->qos[mml_sys_frame].opp_cnt; i++) {
		if (tput <= tp->qos[mml_sys_frame].opp_speeds[i])
			break;
	}
	info_cache->dc_opp = i;

	mml_msg("%s pixel %u opp %u tput %u duration %u remain %u",
		__func__, pixel, i, tput, info_cache->duration, info_cache->remain);
	return true;
}

static enum topology_scenario scene_to_ovl1(u32 ovlsys_id, enum topology_scenario scene)
{
	return scene;
}

static u8 mode_dc2_dispatch[] = {
	[PATH_MML1_NOPQ]	= PATH_MML0_NOPQ,
	[PATH_MML1_RSZ]		= PATH_MML0_RSZ,
	[PATH_MML1_PQ]		= PATH_MML0_PQ,
	[PATH_MML1_AIPQ]	= PATH_MML0_AIPQ,
	[PATH_MML1_PQ_HDR]	= PATH_MML0_PQ_HDR,
};

static u8 mode_rr_dispatch[] = {
	/* rdma dc to rrot dc */
	[PATH_MML1_NOPQ]	= PATH_MML1_RR_NOPQ,
	[PATH_MML1_RSZ]		= PATH_MML1_RR_RSZ,
	[PATH_MML1_PQ]		= PATH_MML1_RR_PQ,
	[PATH_MML1_AIPQ]	= PATH_MML_MAX,
	[PATH_MML1_PQ_HDR]	= PATH_MML1_RR_PQ,

	/* single rrot to dual rrot */
	[PATH_MML1_RR_NOPQ]	= PATH_MML1_RR2_PQ,
	[PATH_MML1_RR_RSZ]	= PATH_MML1_RR2_PQ,
	[PATH_MML1_RR_PQ]	= PATH_MML1_RR2_PQ,

	/* dl rrot to dl rrot dual */
	[PATH_MML1_DL_NOPQ]	= PATH_MML1_DL2_NOPQ,
	[PATH_MML1_DL_RSZ]	= PATH_MML1_DL2_RSZ,
	[PATH_MML1_DL]		= PATH_MML1_DL2,
	[PATH_MML1_DL_AIPQ]	= PATH_MML1_DL2_AIPQ,
	[PATH_MML1_DL_HDR]	= PATH_MML1_DL2_HDR,
};

static void tp_select_path(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg,
	struct mml_topology_path **path)
{
	enum topology_scenario scene = 0;
	bool en_rsz, en_pq, hdrvp, aipq, can_binning = false, dual = true;
	enum mml_color dest_fmt = cfg->info.dest[0].data.format;

	en_rsz = tp_need_resize(&cfg->info, &can_binning) || mml_force_rsz;
	en_pq = cfg->info.dest[0].pq_config.en || MML_FMT_IS_AYUV(dest_fmt) ||
		mml_force_rsz == 2;
	hdrvp = en_pq && cfg->info.dest[0].pq_config.en_hdr &&
		!cfg->info.dest[0].pq_config.en_region_pq;
	aipq = en_pq && cfg->info.dest[0].pq_config.en_hdr &&
		cfg->info.dest[0].pq_config.en_region_pq;

	if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		if (aipq)
			scene = PATH_MML1_DL_AIPQ;
		else if (hdrvp)
			scene = PATH_MML1_DL_HDR;
		else if (en_pq)
			scene = PATH_MML1_DL;
		else if (en_rsz)
			scene = PATH_MML1_DL_RSZ;
		else
			scene = PATH_MML1_DL_NOPQ;
	} else {
		/* following code for DC and DC2 */
		if (aipq)
			scene = PATH_MML1_AIPQ;
		else if (hdrvp)
			scene = PATH_MML1_PQ_HDR;
		else if (en_pq)
			scene = PATH_MML1_PQ;
		else if (en_rsz)
			scene = PATH_MML1_RSZ;
		else {
			scene = PATH_MML1_NOPQ;
			if (mml_rgbrot &&
			    MML_FMT_IS_RGB(cfg->info.src.format) && MML_FMT_IS_RGB(dest_fmt)) {
				mml_msg("[topology]enable rgb rotate");
				cfg->rgbrot = true;
			}
		}

		if (cfg->info.mode == MML_MODE_MML_DECOUPLE2) {
			enum topology_scenario scene_dc2;

			if (scene < ARRAY_SIZE(mode_dc2_dispatch))
				scene_dc2 = mode_dc2_dispatch[scene];
			else
				scene_dc2 = PATH_MML0_PQ;

			if (!scene_is_dc2(scene_dc2)) {
				scene_dc2 = PATH_MML0_PQ;
				mml_err("[topology]fail to dispatch scene %u and reset to %u",
					scene, scene_dc2);
			}
			scene = scene_dc2;
		}
	}

	if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		tp_check_tput_dl(&cfg->info, cache, cfg->panel_w, cfg->panel_h, &dual, NULL);
		if (dual)
			scene = mode_rr_dispatch[scene];
	} else if (cfg->info.mode == MML_MODE_MML_DECOUPLE && mml_rrot == 1 && !aipq) {
		/* dc mode but force enable RROT, translate rdma to rrot */
		scene = mode_rr_dispatch[scene];

		/* dual rrot if necessary */
		dual = mml_rrot_single == 2;
		if (dual)
			scene = mode_rr_dispatch[scene];
	}

	/* check if connect to ovlsys1 */
	scene = scene_to_ovl1(cfg->info.ovlsys_id, scene);

	cfg->rrot_dual = dual;
	cfg->merge2p = scene == PATH_MML1_DL2_NOPQ;
	cfg->rsz_front = scene_is_front_rsz(scene);

	*path = &cache->paths[scene];
}

static s32 tp_select(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg)
{
	struct mml_topology_path *path = NULL;

	if (cfg->info.mode == MML_MODE_DDP_ADDON) {
		cfg->framemode = true;
		cfg->nocmd = true;
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		cfg->framemode = true;
	}
	cfg->shadow = mml_shadow;

	tp_select_path(cache, cfg, &path);

	if (!path)
		return -EPERM;

	cfg->path[0] = path;
	cfg->alpharsz = cfg->info.alpha && MML_FMT_ALPHA(cfg->info.src.format) &&
		(!cfg->info.dest[0].pq_config.en || cfg->info.mode == MML_MODE_DIRECT_LINK);

	if (mml_dpc && !cfg->disp_vdo && !mml_dpc_disable(cfg->mml) &&
	    !(cfg->info.dest[0].pq_config.en_ur ||
	      cfg->info.dest[0].pq_config.en_dc ||
	      cfg->info.dest[0].pq_config.en_hdr ||
	      cfg->info.dest[0].pq_config.en_ccorr ||
	      cfg->info.dest[0].pq_config.en_dre ||
	      cfg->info.dest[0].pq_config.en_region_pq ||
	      cfg->info.dest[0].pq_config.en_fg ||
	      cfg->info.dest[0].pq_config.en_c3d) &&
	    (cfg->info.mode == MML_MODE_DIRECT_LINK ||
	     cfg->info.mode == MML_MODE_RACING ||
	     cfg->info.mode == MML_MODE_DDP_ADDON))
		cfg->dpc = true;
	else
		cfg->dpc = false;

	tp_dump_path_short(path, cfg->shadow, cfg->dpc);

	return 0;
}

static enum mml_mode tp_query_mode_dl(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason, u32 panel_width, u32 panel_height, struct mml_frame_info_cache *info_cache)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);
	const struct mml_frame_dest *dest = &info->dest[0];
	const bool rotated = dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270;
	bool dual = true;

	if (unlikely(mml_dl)) {
		if (mml_dl == 2)
			goto decouple;
		else if (mml_dl == 3)
			goto dl_force;
	} else if (!mml_dl_enable(mml))
		goto decouple;

	/* no fg/c3d support for dl mode */
	if (dest->pq_config.en_fg ||
		dest->pq_config.en_c3d) {
		*reason = mml_query_pqen;
		goto decouple;
	}

	/* TODO: remove after AI region PQ DL mode enable */
	if (dest->pq_config.en_dc ||
		dest->pq_config.en_color ||
		dest->pq_config.en_hdr ||
		dest->pq_config.en_ccorr ||
		dest->pq_config.en_dre ||
		dest->pq_config.en_region_pq ||
		dest->pq_config.en_fg ||
		dest->pq_config.en_c3d ||
		dest->pq_config.en_sharp) {
		*reason = mml_query_pqen;
		goto decouple;
	}

	if (info->src.width > MML_DL_MAX_W) {
		*reason = mml_query_inwidth;
		goto decouple;
	}

	if (info->src.height > MML_DL_MAX_H) {
		*reason = mml_query_inheight;
		goto decouple;
	}

	if ((!rotated && dest->crop.r.width < MML_MIN_SIZE) ||
		(rotated && dest->crop.r.height < MML_MIN_SIZE)) {
		*reason = mml_query_min_size;
		goto decouple;
	}

	/* destination width must cross display pipe width */
	if (dest->data.width < MML_OUT_MIN_W) {
		*reason = mml_query_outwidth;
		goto decouple;
	}

	/* get mid opp frequency */
	if (tp && tp->qos[mml_sys_frame].opp_cnt) {
		if (!tp_check_tput_dl(info, tp, panel_width, panel_height, &dual, info_cache)) {
			*reason = mml_query_opp_out;
			goto decouple;
		}
	} else {
		/* still support dl for mmdvfs not ready case */
		dual = true;
	}

dl_force:
	return MML_MODE_DIRECT_LINK;

decouple:
	return MML_MODE_MML_DECOUPLE;
}

static enum mml_mode tp_query_mode_racing(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason)
{
	struct mml_topology_cache *tp;
	u32 pixel;

	if (unlikely(mml_racing)) {
		if (mml_racing == 2)
			goto decouple;
	} else if (!mml_racing_enable(mml))
		goto decouple;

	/* TODO: should REMOVE after inlinerot resize ready */
	if (unlikely(!mml_racing_rsz) && tp_need_resize(info, NULL)) {
		*reason = mml_query_norsz;
		goto decouple;
	}

	/* secure content cannot output to sram */
	if (info->src.secure || info->dest[0].data.secure) {
		*reason = mml_query_sec;
		goto decouple;
	}

	/* no pq support for racing mode */
	if (info->dest[0].pq_config.en_dc ||
		info->dest[0].pq_config.en_color ||
		info->dest[0].pq_config.en_hdr ||
		info->dest[0].pq_config.en_ccorr ||
		info->dest[0].pq_config.en_dre ||
		info->dest[0].pq_config.en_region_pq ||
		info->dest[0].pq_config.en_fg ||
		info->dest[0].pq_config.en_c3d) {
		*reason = mml_query_pqen;
		goto decouple;
	}

	/* get mid opp frequency */
	tp = mml_topology_get_cache(mml);
	if (!tp || !tp->qos[mml_sys_frame].opp_cnt) {
		mml_err("not support racing due to opp not ready");
		goto decouple;
	}

	pixel = max(info->src.width * info->src.height,
		info->dest[0].data.width * info->dest[0].data.height);

	if (info->act_time) {
		u32 i, dc_opp, ir_freq, ir_opp;
		u32 pipe_pixel = pixel / 2;

		if (!tp->qos[mml_sys_frame].opp_cnt) {
			mml_err("no opp table support");
			goto decouple;
		}

		if (!opp_pixel_table[0]) {
			for (i = 0; i < ARRAY_SIZE(opp_pixel_table); i++) {
				opp_pixel_table[i] = tp->qos[mml_sys_frame].opp_speeds[i] * MML_DC_ACT_DUR;
				mml_log("[topology]Racing pixel OPP %u: %u",
					i, opp_pixel_table[i]);
			}
		}
		for (i = 0; i < tp->qos[mml_sys_frame].opp_cnt; i++)
			if (pipe_pixel < opp_pixel_table[i])
				break;
		dc_opp = min_t(u32, i, ARRAY_SIZE(opp_pixel_table) - 1);
		if (dc_opp > MML_IR_MAX_OPP) {
			*reason = mml_query_opp_out;
			goto decouple;
		}

		ir_freq = pipe_pixel * 1000 / info->act_time;
		for (i = 0; i < tp->qos[mml_sys_frame].opp_cnt; i++)
			if (ir_freq < tp->qos[mml_sys_frame].opp_speeds[i])
				break;
		ir_opp = min_t(u32, i, ARRAY_SIZE(opp_pixel_table) - 1);

		/* simple check if ir mode need higher opp */
		if (ir_opp > dc_opp && ir_opp > 1) {
			*reason = mml_query_acttime;
			goto decouple;
		}
	}

	if (info->dest[0].crop.r.width > MML_IR_WIDTH_2K ||
		info->dest[0].crop.r.height > MML_IR_HEIGHT_2K ||
		pixel > MML_IR_2K) {
		*reason = mml_query_highpixel;
		goto decouple;
	}
	if (info->dest[0].crop.r.width < MML_IR_WIDTH ||
		info->dest[0].crop.r.height < MML_IR_HEIGHT ||
		pixel < MML_IR_MIN) {
		*reason = mml_query_lowpixel;
		goto decouple;
	}

	/* destination width must cross display pipe width */
	if (info->dest[0].data.width < MML_OUT_MIN_W) {
		*reason = mml_query_outwidth;
		goto decouple;
	}

	if (info->dest[0].data.width * info->dest[0].data.height * 1000 /
		info->dest[0].crop.r.width / info->dest[0].crop.r.height <
		MML_IR_RSZ_MIN_RATIO) {
		*reason = mml_query_rszratio;
		goto decouple;
	}

	return MML_MODE_RACING;

decouple:
	return MML_MODE_MML_DECOUPLE;
}

static enum mml_mode tp_query_mode(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason, u32 panel_width, u32 panel_height, struct mml_frame_info_cache *info_cache)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);
	enum mml_mode mode;

	if (unlikely(mml_path_mode))
		return mml_path_mode;

	if (unlikely(!tp))
		goto not_support;

	/* for alpha support */
	if (info->alpha) {
		*reason = mml_query_alpha;
		if (!MML_FMT_ALPHA(info->src.format) ||
		    info->src.width <= 32 ||
		    info->dest_cnt != 1 ||
		    info->dest[0].crop.r.width <= 32 ||
		    info->dest[0].compose.width <= 9)
			goto not_support;
		if (mml_isdc(info->mode))
			mode = info->mode;
		else
			mode = MML_MODE_MML_DECOUPLE;
		goto check_dc_tput;
	}

	/* skip all racing mode check if use prefer dc */
	if (mml_isdc(info->mode) || info->mode == MML_MODE_MDP_DECOUPLE) {
		*reason = mml_query_userdc;
		mode = info->mode;
		goto check_dc_tput;
	}

	if (info->mode == MML_MODE_APUDC) {
		*reason = mml_query_apudc;
		return info->mode;
	}

	/* rotate go to racing (inline rotate) */
	if (mml_racing == 1 &&
		(info->dest[0].rotate == MML_ROT_90 || info->dest[0].rotate == MML_ROT_270))
		mode = tp_query_mode_racing(mml, info, reason);
	else
		mode = tp_query_mode_dl(mml, info, reason, panel_width, panel_height, info_cache);

check_dc_tput:
	if (mml_isdc(mode)) {
		/* dl mode not support, check if dc support */
		if (!tp_check_tput_dc(info, tp, panel_width, panel_height, info_cache)) {
			*reason = mml_query_tp;
			mode = MML_MODE_NOT_SUPPORT;
		}
	} else if (mml_opp_check) {
		/* dl mode support, compare opp with dc */
		if (tp_check_tput_dc(info, tp, panel_width, panel_height, info_cache) &&
			info_cache && info_cache->dl_opp > info_cache->dc_opp) {
			*reason = mml_query_lowpower;
			mode = MML_MODE_MML_DECOUPLE;
		}
	}

	return mode;

not_support:
	return MML_MODE_NOT_SUPPORT;
}

static struct cmdq_client *get_racing_clt(struct mml_topology_cache *cache, u32 pipe)
{
	/* use NO PQ path as inline rot path for this platform */
	return cache->paths[PATH_MML1_RR_NOPQ + pipe].clt;
}

static const struct mml_topology_path *tp_get_dl_path(struct mml_topology_cache *cache,
	struct mml_frame_info *info, u32 pipe, struct mml_frame_size *panel)
{
	u32 scene;
	bool dual = true;

	if (!info)
		return &cache->paths[PATH_MML1_DL];

	tp_check_tput_dl(info, cache, panel->width, panel->height, &dual, NULL);

	scene = dual ? PATH_MML1_DL2 : PATH_MML1_DL;

	/* check if connect to ovlsys1 */
	scene = scene_to_ovl1(info->ovlsys_id, scene);

	return &cache->paths[scene];
}

static enum mml_mode tp_support_couple(void)
{
	return MML_MODE_DIRECT_LINK;
}

static bool tp_support_dc2(void)
{
	return true;
}

static enum mml_hw_caps support_hw_caps(void)
{
	return MML_HW_ALPHARSZ | MML_HW_MULTI_LAYER | MML_HW_PQ_HDR | MML_HW_PQ_MATRIX |
		MML_HW_PQ_HDR10 | MML_HW_PQ_HDR10P | MML_HW_PQ_HLG | MML_HW_PQ_HDRVIVID |
		MML_HW_PQ_FG;
}

static const struct mml_topology_ops tp_ops_mt6991 = {
	.query_mode2 = tp_query_mode,
	.init_cache = tp_init_cache,
	.select = tp_select,
	.get_racing_clt = get_racing_clt,
	.get_dl_path = tp_get_dl_path,
	.support_couple = tp_support_couple,
	.support_dc2 = tp_support_dc2,
	.support_hw_caps = support_hw_caps,
};

static __init int mml_topology_ip_init(void)
{
	/* init hrt mode as max ostd */
	mtk_mml_hrt_mode = MML_HRT_ENABLE;

	return mml_topology_register_ip(TOPOLOGY_PLATFORM, &tp_ops_mt6991);
}
module_init(mml_topology_ip_init);

static __exit void mml_topology_ip_exit(void)
{
	mml_topology_unregister_ip(TOPOLOGY_PLATFORM);
}
module_exit(mml_topology_ip_exit);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML for MT6991");
MODULE_LICENSE("GPL");
