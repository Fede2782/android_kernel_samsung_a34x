/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_MML_RSZ_FW_H__
#define __MTK_MML_RSZ_FW_H__

#include <linux/types.h>
#include "mtk-mml-core.h"

struct rsz_fw_in {
	u32 in_width;
	u32 in_height;
	u32 out_width;
	u32 out_height;
	struct mml_crop crop;
	bool power_saving;
	bool use121filter;
};

struct rsz_fw_out {
	u32 hori_step;
	u32 vert_step;
	u32 precision_x;
	u32 precision_y;
	u32 hori_int_ofst;
	u32 hori_sub_ofst;
	u32 vert_int_ofst;
	u32 vert_sub_ofst;
	u32 hori_scale;
	u32 hori_algo;
	u32 vert_scale;
	u32 vert_algo;
	u32 vert_first;
	u32 vert_cubic_trunc;
	u32 con1;
	u32 con2;
	u32 con3;
	u32 tap_adapt;
	u32 etc_ctrl;
	u32 etc_switch_max_min1;
	u32 etc_switch_max_min2;
	u32 etc_ring;
	u32 etc_ring_gaincon1;
	u32 etc_ring_gaincon2;
	u32 etc_ring_gaincon3;
	u32 etc_sim_port_gaincon1;
	u32 etc_sim_port_gaincon2;
	u32 etc_sim_port_gaincon3;
	u32 etc_blend;
};

/* rsz_fw - RSZ firmware calculate RSZ settings
 *
 * @in:		struct rsz_fw_in contains size information.
 * @out:	struct rsz_fw_out contains RSZ setting results.
 * @en_ur:	enable UR flag
 */
void rsz_fw(struct rsz_fw_in *in, struct rsz_fw_out *out, bool en_ur);

#endif	/* __MTK_MML_RSZ_FW_H__ */
