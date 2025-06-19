/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_ISP_WRAPPER_H__
#define __SWPM_ISP_WRAPPER_H__

#include <linux/types.h>

struct ISP_P1 {
	unsigned int fps;
	unsigned int data; /* current frame data size */
	unsigned int exposure_num;
	unsigned int raw_num;
};

struct ISP_P2 {
	unsigned int data; /* current frame data size */
	unsigned int fps;
	unsigned int ds_type;
	unsigned int eis_type;
	unsigned int vr_data; /* vr data size for pqdip */
	unsigned int disp_fps;
	unsigned int disp_ds_type;
	unsigned int disp_eis_type;
	unsigned int disp_data; /* disp data size for pqdip */
};

struct CSI {
	unsigned int csi_clk;
	unsigned int frame_ratio;
	unsigned int ulps_mode;
	unsigned int c_d_phy;
	unsigned int phy_data_lane_num;
	unsigned int bit_data_rate;
	unsigned int frame_width;
	unsigned int frame_hight;
	unsigned int data_bit;
	unsigned int h_blanking;
	unsigned int v_blanking;
};

struct isp_swpm_cb_func {
	bool is_registered;
	void (*p1_cb)(struct ISP_P1 *idx);
	void (*p2_cb)(struct ISP_P2 *idx);
	void (*csi_cb)(struct CSI *idx);
};

#if IS_ENABLED(CONFIG_MTK_SWPM_ISP)
/* isp drv interface */
extern void set_p1_idx(struct ISP_P1 idx);
extern void set_p2_idx(struct ISP_P2 idx);
extern void set_csi_idx(struct CSI idx);
/* isp swpm interface */
extern void isp_swpm_register(struct isp_swpm_cb_func *func);
extern void isp_swpm_unregister(void);
#else
__weak void set_p1_idx(struct ISP_P1 idx) {}
__weak void set_p2_idx(struct ISP_P2 idx) {}
__weak void set_csi_idx(struct CSI idx) {}
__weak void isp_swpm_register(struct isp_swpm_cb_func *func) {}
__weak void isp_swpm_unregister(void) {}
#endif

struct isp_swpm_cb_func *get_cb_func(void);

#endif // __SWPM_ISP_WRAPPER_H__
