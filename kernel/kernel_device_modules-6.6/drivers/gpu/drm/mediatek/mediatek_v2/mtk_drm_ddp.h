/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_H
#define MTK_DRM_DDP_H

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_spr.h"
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#define DISP_MUTEX_TOTAL (16)
#define DISP_MUTEX_DDP_FIRST (0)
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#define DISP_MUTEX_DDP_LAST (15)
#else
#define DISP_MUTEX_DDP_LAST (6)
#endif
#define DISP_MUTEX_DDP_COUNT (DISP_MUTEX_DDP_LAST - DISP_MUTEX_DDP_FIRST + 1)
#define __DISP_MUTEX_INT_MSK ((1 << (DISP_MUTEX_DDP_COUNT)) - 1)
#define DISP_MUTEX_INT_MSK                                                     \
	((__DISP_MUTEX_INT_MSK << DISP_MUTEX_TOTAL) | __DISP_MUTEX_INT_MSK)

/* CHIST path select*/
#define DISP_CHIST0_FROM_RDMA0_POS 0
#define DISP_CHIST1_FROM_RDMA0_POS 1
#define DISP_CHIST0_FROM_POSTMASK0 2
#define DISP_CHIST1_FROM_POSTMASK0 3
#define DISP_CHIST0_FROM_DITHER0   4
#define DISP_CHIST1_FROM_DITHER0   5

#define MUTEX_SOF_SINGLE_MODE 0
#define MUTEX_SOF_DSI0 1
#define MUTEX_SOF_DSI1 2
#define MUTEX_SOF_DPI0 3
#define MUTEX_SOF_DPI1 4

#define MUTEX_MOD_CNT	2
#define DISP_REG_MUTEX_EN(n) (0x20 + 0x20 * (n))
#define DISP_REG_MUTEX(n)	 (0x24 + 0x20 * (n))
#define DISP_REG_MUTEX_RST_REG		(0x28)
#define DISP_REG_MUTEX_RST(data, n) (data->mutex_rst_reg + 0x20 * (n))
#define DISP_REG_MUTEX_SOF(data, n) (data->mutex_sof_reg + 0x20 * (n))
#define DISP_REG_MUTEX_MOD(i, data, n) (data->mutex_mod_reg[i] + 0x20 * (n))
#define DISP_REG_MUTEX_MOD2	(0x34)

#define SOF_FLD_MUTEX0_SOF REG_FLD(3, 0)
#define SOF_FLD_MUTEX0_SOF_TIMING REG_FLD(2, 3)
#define SOF_FLD_MUTEX0_SOF_WAIT REG_FLD(1, 5)
#define SOF_FLD_MUTEX0_EOF REG_FLD(3, 6)
#define SOF_FLD_MUTEX0_FOF_TIMING REG_FLD(2, 9)
#define SOF_FLD_MUTEX0_EOF_WAIT REG_FLD(1, 11)

#define MT6991_SOF_FLD_MUTEX0_SOF REG_FLD(4, 0)
#define MT6991_SOF_FLD_MUTEX0_SOF_TIMING REG_FLD(2, 4)
#define MT6991_SOF_FLD_MUTEX0_SOF_WAIT REG_FLD(1, 6)
#define MT6991_SOF_FLD_MUTEX0_EOF REG_FLD(4, 7)
#define MT6991_SOF_FLD_MUTEX0_FOF_TIMING REG_FLD(2, 11)
#define MT6991_SOF_FLD_MUTEX0_EOF_WAIT REG_FLD(1, 13)


#define MT6991_OVLSYS_GCE_FRAME_DONE_SEL0	(0x090)
#define GCE_FRAME_DONE_SEL0		REG_FLD_MSB_LSB(5, 0)
#define GCE_FRAME_DONE_SEL1		REG_FLD_MSB_LSB(13, 8)
#define GCE_FRAME_DONE_SEL2		REG_FLD_MSB_LSB(21, 16)
#define GCE_FRAME_DONE_SEL3		REG_FLD_MSB_LSB(29, 24)

#define MT6991_OVLSYS_GCE_FRAME_DONE_SEL1	(0x094)
#define MT6991_OVLSYS_GCE_FRAME_DONE_SEL2	(0x098)
#define MT6991_OVLSYS_GCE_FRAME_DONE_SEL3	(0x09C)

#define MT6991_OVLSYS0_GCE_FRAME_DONE_SEL0_WDMA1			(21)
#define MT6991_OVLSYS0_GCE_FRAME_DONE_SEL0_WDMA0			(22)

#define MT6991_OVLSYS1_GCE_FRAME_DONE_SEL0_WDMA0			(22)


#define MT6991_DISP0_GCE_FRAME_DONE_SEL0	(0x410)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL1	(0x414)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL2	(0x418)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL3	(0x41C)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL4	(0x420)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL5	(0x424)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL6	(0x428)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL7	(0x42C)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL8	(0x430)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL9	(0x434)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL10	(0x438)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL11	(0x43C)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL12	(0x440)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL13	(0x444)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL14	(0x448)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL15	(0x44C)


#define MT6991_DISP0_GCE_FRAME_DONE_SEL0_MDP_RDMA0			(9)	//sel0
#define MT6991_DISP0_GCE_FRAME_DONE_SEL1_Y2R0				(2)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL2_AAL0				(29)
#define MT6991_DISP0_GCE_FRAME_DONE_SEL3_AAL1				(28)

#define MT6991_DISP1_GCE_FRAME_DONE_SEL0	(0xA10)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL1	(0xA14)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL2	(0xA18)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL3	(0xA1C)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL4	(0xA20)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL5	(0xA24)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL6	(0xA28)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL7	(0xA2C)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL8	(0xA30)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL9	(0xA34)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL10	(0xA38)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL11	(0xA3C)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL12	(0xA40)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL13	(0xA44)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL14	(0xA48)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL15	(0xA4C)

#define MT6991_DISP1_GCE_FRAME_DONE_SEL0_DP_INTF			(41)	//sel0
#define MT6991_DISP1_GCE_FRAME_DONE_SEL1_DSI0_FRAME_DONE	(31)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL2_DSI1_FRAME_DONE	(29)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL3_RDMA1_FRAME_DONE	(23)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL4_WDMA1_FRAME_DONE	(11)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL5_WDMA3_FRAME_DONE	(9)
#define MT6991_DISP1_GCE_FRAME_DONE_SEL6_DSI2_FRAME_DONE	(27)

enum mtk_ddp_mutex_sof_id {
	DDP_MUTEX_SOF_SINGLE_MODE,
	DDP_MUTEX_SOF_DSI0,
	DDP_MUTEX_SOF_DSI1,
	DDP_MUTEX_SOF_DPI0,
	DDP_MUTEX_SOF_DPI1,
	DDP_MUTEX_SOF_DSI2,
	DDP_MUTEX_SOF_DSI3,
	DDP_MUTEX_SOF_DVO,
	DDP_MUTEX_SOF_MAX,
};

struct regmap;
struct device;
struct mtk_disp_mutex;
struct mtk_mmsys_reg_data;

struct dummy_mapping {
	resource_size_t pa_addr;
	void __iomem *addr;
	enum mtk_ddp_comp_id comp_id;
	unsigned int offset;
};

struct mtk_disp_mutex {
	int id;
	bool claimed;
};

struct mtk_disp_ddp_data {
	const unsigned int *mutex_mod;
	const unsigned int *mutex_ovlsys_mod;
	const unsigned int *mutex_sof;
	const unsigned int *mutex_ovlsys_sof;
	unsigned int mutex_mod_reg[MUTEX_MOD_CNT];
	unsigned int mutex_sof_reg;
	unsigned int mutex_rst_reg;
	const unsigned int *dispsys_map;
	bool wakeup_pf_wq;
	bool wakeup_esd_wq;
};

struct mtk_ddp {
	struct device *dev;
	struct clk *clk;
	void __iomem *regs;
	resource_size_t regs_pa;

	unsigned int dispsys_num;
	unsigned int ovlsys_num;
	struct clk *side_clk;
	struct clk *ovlsys0_clk;
	struct clk *ovlsys1_clk;
	void __iomem *side_regs;
	void __iomem *ovlsys0_regs;
	void __iomem *ovlsys1_regs;
	resource_size_t side_regs_pa;
	resource_size_t ovlsys0_regs_pa;
	resource_size_t ovlsys1_regs_pa;
	struct mtk_disp_mutex mutex[10];
	const struct mtk_disp_ddp_data *data;
	struct mtk_drm_crtc *mtk_crtc[MAX_CRTC];
	struct cmdq_base *cmdq_base;
	struct mtk_ddp_comp ddp_comp;
};

struct mtk_mmsys_reg_data {
	unsigned int ovl0_mout_en;
	unsigned int rdma0_sout_sel_in;
	unsigned int rdma0_sout_color0;
	unsigned int rdma1_sout_sel_in;
	unsigned int rdma1_sout_dpi0;
	unsigned int rdma1_sout_dsi0;
	unsigned int dpi0_sel_in;
	unsigned int dpi0_sel_in_rdma1;
	unsigned int *path_sel;
	unsigned int path_sel_size;
	const unsigned int *dispsys_map;
	const unsigned int *module_rst_offset;
	const unsigned int *module_rst_bit;
};

#define MT6983_DUMMY_REG_CNT 85
extern struct dummy_mapping mt6983_dispsys_dummy_register[MT6983_DUMMY_REG_CNT];

#define MT6879_DUMMY_REG_CNT 53
extern struct dummy_mapping mt6879_dispsys_dummy_register[MT6879_DUMMY_REG_CNT];


const struct mtk_mmsys_reg_data *
mtk_ddp_get_mmsys_reg_data(enum mtk_mmsys_id mmsys_id);

void mtk_disp_ultra_offset(void __iomem *config_regs,
			enum mtk_ddp_comp_id comp, bool is_dc);
void mtk_ddp_add_comp_to_path(struct mtk_drm_crtc *mtk_crtc,
			      struct mtk_ddp_comp *comp,
			      enum mtk_ddp_comp_id prev,
			      enum mtk_ddp_comp_id next);
void mtk_ddp_add_comp_to_path_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					enum mtk_ddp_comp_id cur,
					enum mtk_ddp_comp_id prev,
					enum mtk_ddp_comp_id next,
					struct cmdq_pkt *handle);
void mtk_ddp_remove_comp_from_path(struct mtk_drm_crtc *mtk_crtc,
				   enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next);
void mtk_ddp_remove_comp_from_path_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					     enum mtk_ddp_comp_id cur,
					     enum mtk_ddp_comp_id next,
					     struct cmdq_pkt *handle);

struct mtk_disp_mutex *mtk_disp_mutex_get(struct device *dev, unsigned int id);
int mtk_disp_mutex_prepare(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_add_comp(struct mtk_disp_mutex *mutex,
			     enum mtk_ddp_comp_id id);
void mtk_ovlsys_mutex_add_comp(struct mtk_disp_mutex *mutex,
			     enum mtk_ddp_comp_id id);
void mtk_disp_mutex_add_comp_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
				       enum mtk_ddp_comp_id id, bool is_cmd_mod,
				       struct cmdq_pkt *handle,
				       unsigned int mutex_id);
void mtk_disp_mutex_enable(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_disable(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_remove_comp(struct mtk_disp_mutex *mutex,
				enum mtk_ddp_comp_id id);
void mtk_disp_mutex_remove_comp_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					  enum mtk_ddp_comp_id id,
					  struct cmdq_pkt *handle,
					  unsigned int mutex_id);
void mtk_disp_mutex_unprepare(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_put(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_acquire(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_release(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_trigger(struct mtk_disp_mutex *mutex, void *handle);

void mtk_disp_mutex_enable_cmdq(struct mtk_disp_mutex *mutex,
				struct cmdq_pkt *cmdq_handle,
				struct cmdq_base *cmdq_base);
void mtk_disp_mutex_src_set(struct mtk_drm_crtc *mtk_crtc, bool is_cmd_mode);
void mtk_disp_mutex_inten_enable_cmdq(struct mtk_disp_mutex *mutex,
				      void *handle);
void mtk_disp_mutex_inten_disable_cmdq(struct mtk_disp_mutex *mutex,
				       void *handle);

void mutex_dump_reg_mt6885(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6983(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6985(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_reg_mt6985(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6897(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_reg_mt6897(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6895(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6879(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6855(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6885(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6983(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_analysis_mt6985(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6985(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_analysis_mt6897(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6897(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6895(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_reg_mt6885(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6879(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6855(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6983(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6985(void __iomem *config_regs);
void ovlsys_config_dump_reg_mt6985(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6897(void __iomem *config_regs);
void ovlsys_config_dump_reg_mt6897(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6895(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6983(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6985(void __iomem *config_regs);
void ovlsys_config_dump_analysis_mt6985(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6897(void __iomem *config_regs);
void ovlsys_config_dump_analysis_mt6897(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6895(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6885(void __iomem *config_regs);
void mutex_dump_reg_mt6989(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_reg_mt6989(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_analysis_mt6989(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6989(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_reg_mt6989(void __iomem *config_regs);
void ovlsys_config_dump_reg_mt6989(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6989(void __iomem *config_regs, int sys_id);
void ovlsys_config_dump_analysis_mt6989(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6768(void __iomem *config_regs);
void mutex_dump_analysis_mt6768(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_analysis_mt6761(void __iomem *config_regs);
void mutex_dump_analysis_mt6761(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_analysis_mt6765(void __iomem *config_regs);
void mutex_dump_analysis_mt6765(struct mtk_disp_mutex *mutex);

void mutex_dump_reg_mt6991(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_reg_mt6991(struct mtk_disp_mutex *mutex);
void mutex_ovlsys_dump_analysis_mt6991(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6991(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_reg_mt6991(void __iomem *config_regs);
void ovlsys_config_dump_reg_mt6991(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6991(void __iomem *config_regs, int sys_id);
void ovlsys_config_dump_analysis_mt6991(void __iomem *config_regs);
void mtk_ddp_insert_dsc_prim_MT6885(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6885(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6983(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6983(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6985(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6985(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6989(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6989(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_mt6897(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_mt6897(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_ext_MT6989(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_ext_MT6989(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6991(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6991(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mtk_ddp_insert_dsc_ext_MT6985(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_ext_MT6985(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6895(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6895(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6879(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6879(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6855(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6855(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_connect_dual_pipe_path(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_disp_mutex *mutex);
void mtk_ddp_disconnect_dual_pipe_path(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_submit_sof(struct mtk_disp_mutex *mutex);
void mtk_ddp_dual_pipe_dump(struct mtk_drm_crtc *mtk_crtc);

void mutex_dump_reg_mt6873(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6873(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_reg_mt6873(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6873(void __iomem *config_regs);

void mtk_ddp_insert_dsc_prim_MT6873(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6873(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6853(void __iomem *config_regs);
void mutex_dump_analysis_mt6853(struct mtk_disp_mutex *mutex);

void mtk_ddp_insert_dsc_prim_MT6853(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6853(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6833(void __iomem *config_regs);
void mutex_dump_analysis_mt6833(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6877(void __iomem *config_regs);
void mutex_dump_analysis_mt6877(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6781(void __iomem *config_regs);
void mutex_dump_analysis_mt6781(struct mtk_disp_mutex *mutex);

void mtk_ddp_insert_dsc_prim_MT6781(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6781(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mtk_ddp_insert_dsc_prim_MT6877(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6877(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6879(void __iomem *config_regs);
void mutex_dump_analysis_mt6879(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6855(void __iomem *config_regs);
void mutex_dump_analysis_mt6855(struct mtk_disp_mutex *mutex);
struct mtk_ddp_comp *mtk_ddp_get_path_addon_dsc_comp(struct mtk_drm_crtc *mtk_crtc);
unsigned int mtk_ddp_ovlsys_path(struct mtk_drm_private *priv, unsigned int **ovl_list);
unsigned int mtk_ddp_ovl_usage_trans(struct mtk_drm_private *priv, unsigned int usage);
unsigned int mtk_ddp_ovl_resource_list(struct mtk_drm_private *priv, unsigned int **ovl_list);

void mtk_ddp_disable_merge_irq(struct drm_device *drm);

void mtk_ddp_clean_ovl_pq_crossbar(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

char *mtk_ddp_get_mutex_sof_name(unsigned int regval);

void mtk_ddp_rst_module(struct mtk_drm_crtc *mtk_crtc,
	enum mtk_ddp_comp_id m, struct cmdq_pkt *handle);

void mtk_disp_dbg_cmdq_use_mutex(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle, int mutex_id);

int mtk_ddp_exdma_mout_MT6991(enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr);

int mtk_ddp_exdma_mout_reset_MT6991(enum mtk_ddp_comp_type type, int *offset,
			   unsigned int *addr_begin, unsigned int *addr_end, int crtc_id);

void mtk_gce_event_config_MT6991(struct drm_device *drm);

#endif /* MTK_DRM_DDP_H */
