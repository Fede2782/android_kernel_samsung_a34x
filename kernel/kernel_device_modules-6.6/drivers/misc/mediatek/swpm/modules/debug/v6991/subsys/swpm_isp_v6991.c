// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/mutex.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif
#if IS_ENABLED(CONFIG_MTK_THERMAL)
#include <thermal_interface.h>
#endif
#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_isp_v6991.h>
// log level
static u32 log_level;
module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "swpm_log_level");
/* share sram for swpm mem */
static struct isp_swpm_data *isp_swpm_data_ptr;

void update_p1_idx(struct ISP_P1 *idx)
{
	if (!idx) {
		isp_log_detail(log_level, "null ISP_P1 idx");
		return;
	}
	isp_log_detail(log_level, "isp_p1_idx: fps(%d) data(%d) exp(%d) raw(%d)",
				   idx->fps, idx->data, idx->exposure_num, idx->raw_num);
	if (isp_swpm_data_ptr) {
		isp_swpm_data_ptr->isp_p1_idx.fps = idx->fps;
		isp_swpm_data_ptr->isp_p1_idx.data = idx->data;
		isp_swpm_data_ptr->isp_p1_idx.exposure_num = idx->exposure_num;
		isp_swpm_data_ptr->isp_p1_idx.raw_num = idx->raw_num;
	} else {
		isp_log_detail(log_level, "cannot get isp swpm sram");
	}
}

void update_p2_idx(struct ISP_P2 *idx)
{
	if (!idx) {
		isp_log_detail(log_level, "null ISP_P2 idx");
		return;
	}
	isp_log_detail(log_level, "isp_p2_idx: data(%d) fps(%d) ds_type(%d) eis_type(%d) vr_data(%d) disp_fps(%d) ",
				idx->data, idx->fps, idx->ds_type, idx->eis_type, idx->vr_data, idx->disp_fps);
	isp_log_detail(log_level, "disp_ds_type(%d) disp_eis_type(%d) dis_data(%d)",
				idx->disp_ds_type, idx->disp_eis_type, idx->disp_data);
	if (isp_swpm_data_ptr) {
		isp_swpm_data_ptr->isp_p2_idx.data = idx->data;
		isp_swpm_data_ptr->isp_p2_idx.fps = idx->fps;
		isp_swpm_data_ptr->isp_p2_idx.ds_type = idx->ds_type;
		isp_swpm_data_ptr->isp_p2_idx.eis_type = idx->eis_type;
		isp_swpm_data_ptr->isp_p2_idx.vr_data = idx->vr_data;
		isp_swpm_data_ptr->isp_p2_idx.disp_fps = idx->disp_fps;
		isp_swpm_data_ptr->isp_p2_idx.disp_ds_type = idx->disp_ds_type;
		isp_swpm_data_ptr->isp_p2_idx.disp_eis_type = idx->disp_eis_type;
		isp_swpm_data_ptr->isp_p2_idx.disp_data = idx->disp_data;
	} else {
		isp_log_detail(log_level, "cannot get isp swpm sram");
	}
}

void update_csi_idx(struct CSI *idx)
{
	if (!idx) {
		isp_log_detail(log_level, "null CSI idx");
		return;
	}
	isp_log_detail(log_level, "csi_idx: csi_clk(%d) frame_ratio(%d) ulps_mode(%d) c_d_phy(%d) ",
				idx->csi_clk, idx->frame_ratio, idx->ulps_mode, idx->c_d_phy);
	isp_log_detail(log_level, "phy_data_lane_num(%d) bit_data_rate(%d) frame_width(%d) frame_hight(%d) ",
				idx->phy_data_lane_num, idx->bit_data_rate, idx->frame_width, idx->frame_hight);
	isp_log_detail(log_level, "data_bit(%d) h_blanking(%d) v_blanking(%d)",
				idx->data_bit, idx->h_blanking, idx->v_blanking);
	if (isp_swpm_data_ptr) {
		isp_swpm_data_ptr->csi_idx.csi_clk = idx->csi_clk;
		isp_swpm_data_ptr->csi_idx.frame_ratio = idx->frame_ratio;
		isp_swpm_data_ptr->csi_idx.ulps_mode = idx->ulps_mode;
		isp_swpm_data_ptr->csi_idx.c_d_phy = idx->c_d_phy;
		isp_swpm_data_ptr->csi_idx.phy_data_lane_num = idx->phy_data_lane_num;
		isp_swpm_data_ptr->csi_idx.bit_data_rate = idx->bit_data_rate;
		isp_swpm_data_ptr->csi_idx.frame_width = idx->frame_width;
		isp_swpm_data_ptr->csi_idx.frame_hight = idx->frame_hight;
		isp_swpm_data_ptr->csi_idx.data_bit = idx->data_bit;
		isp_swpm_data_ptr->csi_idx.h_blanking = idx->h_blanking;
		isp_swpm_data_ptr->csi_idx.v_blanking = idx->v_blanking;
	} else {
		isp_log_detail(log_level, "cannot get isp swpm sram");
	}
}

int __init swpm_isp_init(void)
{
	unsigned int offset;
	struct isp_swpm_cb_func func;

	offset = swpm_set_and_get_cmd(0, 0, 0, ISP_CMD_TYPE);
	isp_swpm_data_ptr = (struct isp_swpm_data *)sspm_sbuf_get(offset);
    /* exception control for illegal sbuf request */
	if (!isp_swpm_data_ptr) {
		isp_log_basic("swpm isp share sram offset fail\n");
		return -1;
	}
	isp_log_basic("swpm mem init offset = 0x%x\n", offset);

	func.p1_cb = update_p1_idx;
	func.p2_cb = update_p2_idx;
	func.csi_cb = update_csi_idx;
	isp_swpm_register(&func);

	return 0;
}
void __exit swpm_isp_exit(void)
{
	isp_swpm_unregister();
}
module_init(swpm_isp_init);
module_exit(swpm_isp_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6991 SWPM isp debug module");
MODULE_AUTHOR("MediaTek Inc.");
