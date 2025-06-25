/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DISP_OVL_OUT_PROC_H__
#define __MTK_DISP_OVL_OUT_PROC_H__

struct mtk_disp_ovl_outproc_data {
	unsigned int addr;
	unsigned int el_addr_offset;
	unsigned int el_hdr_addr;
	unsigned int el_hdr_addr_offset;
	bool fmt_rgb565_is_0;
	unsigned int fmt_uyvy;
	unsigned int fmt_yuyv;
	const struct exdma_compress_info *compr_info;
	bool support_shadow;
	bool need_bypass_shadow;
	/* golden setting */
	unsigned int preultra_th_dc;
	unsigned int fifo_size;
	unsigned int issue_req_th_dl;
	unsigned int issue_req_th_dc;
	unsigned int issue_req_th_urg_dl;
	unsigned int issue_req_th_urg_dc;
	unsigned int greq_num_dl;
	unsigned int stash_en;
	unsigned int stash_cfg;
	bool is_support_34bits;
	unsigned int (*aid_sel_mapping)(struct mtk_ddp_comp *comp);
	bool aid_per_layer_setting;
	resource_size_t (*mmsys_mapping)(struct mtk_ddp_comp *comp);
	unsigned int source_bpc;
	bool (*is_right_ovl_comp)(struct mtk_ddp_comp *comp);
	unsigned int (*frame_done_event)(struct mtk_ddp_comp *comp);
	unsigned int (*ovlsys_mapping)(struct mtk_ddp_comp *comp);
	unsigned int (*ovl_phy_mapping)(struct mtk_ddp_comp *comp);
};

#endif
