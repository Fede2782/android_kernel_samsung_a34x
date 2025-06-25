/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MTK_DISP_SWPM_H
#define MTK_DISP_SWPM_H

/* only record display0 behavior so far */
#define SWPM_DISP_NUM 1

enum disp_cmd_action {
	DISP_GET_SWPM_ADDR = 0,
	DISP_SWPM_SET_POWER_STA = 1,
	DISP_SWPM_SET_IDLE_STA = 2,
};

struct disp_swpm_data {
	unsigned int dsi_lane_num;
	unsigned int dsi_phy_type;
	unsigned int dsi_data_rate;
};

extern int mtk_disp_get_dsi_data_rate(unsigned int info_idx);
extern int swpm_disp_v6989_init(void);
extern void swpm_disp_v6989_exit(void);
#endif

