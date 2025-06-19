/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DISP_OVL_BLENDER_H__
#define __MTK_DISP_OVL_BLENDER_H__


struct mtk_disp_ovl_blender_data {
	unsigned int addr;
	unsigned int el_addr_offset;
	unsigned int el_hdr_addr;
	unsigned int el_hdr_addr_offset;
	bool fmt_rgb565_is_0;
	unsigned int fmt_uyvy;
	unsigned int fmt_yuyv;
	const struct blender_compress_info *compr_info;
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
};
struct blender_compress_info {
	/* naming rule: tech_version_MTK_sub-version,
	 * i.e.: PVRIC_V3_1_MTK_1
	 * sub-version is used when compression version is the same
	 * but mtk decoder is different among platforms.
	 */
	const char name[25];

	bool (*l_config)(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle);
};

#endif
