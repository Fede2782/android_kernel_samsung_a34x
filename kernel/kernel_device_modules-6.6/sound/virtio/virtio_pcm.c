// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 MediaTek Inc.
 * virtio-snd: Virtio sound device
 * Copyright (C) 2021 OpenSynergy GmbH
 */
#include <linux/moduleparam.h>
#include <linux/virtio_config.h>
#include <linux/debugfs.h>

#include "virtio_card.h"
#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
// #include <linux/regmap.h>
#include <linux/irqdomain.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/nebula/hvcall.h>
#include <linux/irq.h>
// #include "../soc/mediatek/mt6991/mt6991-reg.h"
// #include "../soc/mediatek/mt6991/mt6991-afe-common.h"
#define HWIRQ 383
static DEFINE_SPINLOCK(virtsnd_set_reg_lock);

static const struct mtk_base_memif_data memif_data[MT6991_MEMIF_NUM] = {
	[MT6991_MEMIF_DL0] = {
		.name = "DL0",
		.id = MT6991_MEMIF_DL0,
		.reg_ofs_base = AFE_DL0_BASE,
		.reg_ofs_cur = AFE_DL0_CUR,
		.reg_ofs_end = AFE_DL0_END,
		.reg_ofs_base_msb = AFE_DL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL0_END_MSB,
		.fs_reg = AFE_DL0_CON0,
		.fs_shift = DL0_SEL_FS_SFT,
		.fs_maskbit = DL0_SEL_FS_MASK,
		.mono_reg = AFE_DL0_CON0,
		.mono_shift = DL0_MONO_SFT,
		.enable_reg = AFE_DL0_CON0,
		.enable_shift = DL0_ON_SFT,
		.hd_reg = AFE_DL0_CON0,
		.hd_mask = DL0_HD_MODE_MASK,
		.hd_shift = DL0_HD_MODE_SFT,
		.hd_align_reg = AFE_DL0_CON0,
		.hd_align_mshift = DL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL0_CON0,
		.pbuf_mask = DL0_PBUF_SIZE_MASK,
		.pbuf_shift = DL0_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL0_CON0,
		.minlen_mask = DL0_MINLEN_MASK,
		.minlen_shift = DL0_MINLEN_SFT,
		.maxlen_reg = AFE_DL0_CON0,
		.maxlen_mask = DL0_MAXLEN_MASK,
		.maxlen_shift = DL0_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT6991_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DL1_CON0,
		.fs_shift = DL1_SEL_FS_SFT,
		.fs_maskbit = DL1_SEL_FS_MASK,
		.mono_reg = AFE_DL1_CON0,
		.mono_shift = DL1_MONO_SFT,
		.enable_reg = AFE_DL1_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_DL1_CON0,
		.hd_mask = DL1_HD_MODE_MASK,
		.hd_shift = DL1_HD_MODE_SFT,
		.hd_align_reg = AFE_DL1_CON0,
		.hd_align_mshift = DL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL1_CON0,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL1_CON0,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
		.maxlen_reg = AFE_DL1_CON0,
		.maxlen_mask = DL1_MAXLEN_MASK,
		.maxlen_shift = DL1_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT6991_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DL2_CON0,
		.fs_shift = DL2_SEL_FS_SFT,
		.fs_maskbit = DL2_SEL_FS_MASK,
		.mono_reg = AFE_DL2_CON0,
		.mono_shift = DL2_MONO_SFT,
		.enable_reg = AFE_DL2_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_DL2_CON0,
		.hd_mask = DL2_HD_MODE_MASK,
		.hd_shift = DL2_HD_MODE_SFT,
		.hd_align_reg = AFE_DL2_CON0,
		.hd_align_mshift = DL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL2_CON0,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL2_CON0,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
		.maxlen_reg = AFE_DL2_CON0,
		.maxlen_mask = DL2_MAXLEN_MASK,
		.maxlen_shift = DL2_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT6991_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DL3_CON0,
		.fs_shift = DL3_SEL_FS_SFT,
		.fs_maskbit = DL3_SEL_FS_MASK,
		.mono_reg = AFE_DL3_CON0,
		.mono_shift = DL3_MONO_SFT,
		.enable_reg = AFE_DL3_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_DL3_CON0,
		.hd_mask = DL3_HD_MODE_MASK,
		.hd_shift = DL3_HD_MODE_SFT,
		.hd_align_reg = AFE_DL3_CON0,
		.hd_align_mshift = DL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL3_CON0,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL3_CON0,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
		.maxlen_reg = AFE_DL3_CON0,
		.maxlen_mask = DL3_MAXLEN_MASK,
		.maxlen_shift = DL3_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL4] = {
		.name = "DL4",
		.id = MT6991_MEMIF_DL4,
		.reg_ofs_base = AFE_DL4_BASE,
		.reg_ofs_cur = AFE_DL4_CUR,
		.reg_ofs_end = AFE_DL4_END,
		.reg_ofs_base_msb = AFE_DL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL4_END_MSB,
		.fs_reg = AFE_DL4_CON0,
		.fs_shift = DL4_SEL_FS_SFT,
		.fs_maskbit = DL4_SEL_FS_MASK,
		.mono_reg = AFE_DL4_CON0,
		.mono_shift = DL4_MONO_SFT,
		.enable_reg = AFE_DL4_CON0,
		.enable_shift = DL4_ON_SFT,
		.hd_reg = AFE_DL4_CON0,
		.hd_mask = DL4_HD_MODE_MASK,
		.hd_shift = DL4_HD_MODE_SFT,
		.hd_align_reg = AFE_DL4_CON0,
		.hd_align_mshift = DL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL4_CON0,
		.pbuf_mask = DL4_PBUF_SIZE_MASK,
		.pbuf_shift = DL4_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL4_CON0,
		.minlen_mask = DL4_MINLEN_MASK,
		.minlen_shift = DL4_MINLEN_SFT,
		.maxlen_reg = AFE_DL4_CON0,
		.maxlen_mask = DL4_MAXLEN_MASK,
		.maxlen_shift = DL4_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL5] = {
		.name = "DL5",
		.id = MT6991_MEMIF_DL5,
		.reg_ofs_base = AFE_DL5_BASE,
		.reg_ofs_cur = AFE_DL5_CUR,
		.reg_ofs_end = AFE_DL5_END,
		.reg_ofs_base_msb = AFE_DL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL5_END_MSB,
		.fs_reg = AFE_DL5_CON0,
		.fs_shift = DL5_SEL_FS_SFT,
		.fs_maskbit = DL5_SEL_FS_MASK,
		.mono_reg = AFE_DL5_CON0,
		.mono_shift = DL5_MONO_SFT,
		.enable_reg = AFE_DL5_CON0,
		.enable_shift = DL5_ON_SFT,
		.hd_reg = AFE_DL5_CON0,
		.hd_mask = DL5_HD_MODE_MASK,
		.hd_shift = DL5_HD_MODE_SFT,
		.hd_align_reg = AFE_DL5_CON0,
		.hd_align_mshift = DL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL5_CON0,
		.pbuf_mask = DL5_PBUF_SIZE_MASK,
		.pbuf_shift = DL5_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL5_CON0,
		.minlen_mask = DL5_MINLEN_MASK,
		.minlen_shift = DL5_MINLEN_SFT,
		.maxlen_reg = AFE_DL5_CON0,
		.maxlen_mask = DL5_MAXLEN_MASK,
		.maxlen_shift = DL5_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL6] = {
		.name = "DL6",
		.id = MT6991_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.reg_ofs_end = AFE_DL6_END,
		.reg_ofs_base_msb = AFE_DL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL6_END_MSB,
		.fs_reg = AFE_DL6_CON0,
		.fs_shift = DL6_SEL_FS_SFT,
		.fs_maskbit = DL6_SEL_FS_MASK,
		.mono_reg = AFE_DL6_CON0,
		.mono_shift = DL6_MONO_SFT,
		.enable_reg = AFE_DL6_CON0,
		.enable_shift = DL6_ON_SFT,
		.hd_reg = AFE_DL6_CON0,
		.hd_mask = DL6_HD_MODE_MASK,
		.hd_shift = DL6_HD_MODE_SFT,
		.hd_align_reg = AFE_DL6_CON0,
		.hd_align_mshift = DL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL6_CON0,
		.pbuf_mask = DL6_PBUF_SIZE_MASK,
		.pbuf_shift = DL6_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL6_CON0,
		.minlen_mask = DL6_MINLEN_MASK,
		.minlen_shift = DL6_MINLEN_SFT,
		.maxlen_reg = AFE_DL6_CON0,
		.maxlen_mask = DL6_MAXLEN_MASK,
		.maxlen_shift = DL6_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL7] = {
		.name = "DL7",
		.id = MT6991_MEMIF_DL7,
		.reg_ofs_base = AFE_DL7_BASE,
		.reg_ofs_cur = AFE_DL7_CUR,
		.reg_ofs_end = AFE_DL7_END,
		.reg_ofs_base_msb = AFE_DL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL7_END_MSB,
		.fs_reg = AFE_DL7_CON0,
		.fs_shift = DL7_SEL_FS_SFT,
		.fs_maskbit = DL7_SEL_FS_MASK,
		.mono_reg = AFE_DL7_CON0,
		.mono_shift = DL7_MONO_SFT,
		.enable_reg = AFE_DL7_CON0,
		.enable_shift = DL7_ON_SFT,
		.hd_reg = AFE_DL7_CON0,
		.hd_mask = DL7_HD_MODE_MASK,
		.hd_shift = DL7_HD_MODE_SFT,
		.hd_align_reg = AFE_DL7_CON0,
		.hd_align_mshift = DL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL7_CON0,
		.pbuf_mask = DL7_PBUF_SIZE_MASK,
		.pbuf_shift = DL7_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL7_CON0,
		.minlen_mask = DL7_MINLEN_MASK,
		.minlen_shift = DL7_MINLEN_SFT,
		.maxlen_reg = AFE_DL7_CON0,
		.maxlen_mask = DL7_MAXLEN_MASK,
		.maxlen_shift = DL7_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL8] = {
		.name = "DL8",
		.id = MT6991_MEMIF_DL8,
		.reg_ofs_base = AFE_DL8_BASE,
		.reg_ofs_cur = AFE_DL8_CUR,
		.reg_ofs_end = AFE_DL8_END,
		.reg_ofs_base_msb = AFE_DL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL8_END_MSB,
		.fs_reg = AFE_DL8_CON0,
		.fs_shift = DL8_SEL_FS_SFT,
		.fs_maskbit = DL8_SEL_FS_MASK,
		.mono_reg = AFE_DL8_CON0,
		.mono_shift = DL8_MONO_SFT,
		.enable_reg = AFE_DL8_CON0,
		.enable_shift = DL8_ON_SFT,
		.hd_reg = AFE_DL8_CON0,
		.hd_mask = DL8_HD_MODE_MASK,
		.hd_shift = DL8_HD_MODE_SFT,
		.hd_align_reg = AFE_DL8_CON0,
		.hd_align_mshift = DL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL8_CON0,
		.pbuf_mask = DL8_PBUF_SIZE_MASK,
		.pbuf_shift = DL8_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL8_CON0,
		.minlen_mask = DL8_MINLEN_MASK,
		.minlen_shift = DL8_MINLEN_SFT,
		.maxlen_reg = AFE_DL8_CON0,
		.maxlen_mask = DL8_MAXLEN_MASK,
		.maxlen_shift = DL8_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL23] = {
		.name = "DL23",
		.id = MT6991_MEMIF_DL23,
		.reg_ofs_base = AFE_DL23_BASE,
		.reg_ofs_cur = AFE_DL23_CUR,
		.reg_ofs_end = AFE_DL23_END,
		.reg_ofs_base_msb = AFE_DL23_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL23_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL23_END_MSB,
		.fs_reg = AFE_DL23_CON0,
		.fs_shift = DL23_SEL_FS_SFT,
		.fs_maskbit = DL23_SEL_FS_MASK,
		.mono_reg = AFE_DL23_CON0,
		.mono_shift = DL23_MONO_SFT,
		.enable_reg = AFE_DL23_CON0,
		.enable_shift = DL23_ON_SFT,
		.hd_reg = AFE_DL23_CON0,
		.hd_mask = DL23_HD_MODE_MASK,
		.hd_shift = DL23_HD_MODE_SFT,
		.hd_align_reg = AFE_DL23_CON0,
		.hd_align_mshift = DL23_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL23_CON0,
		.pbuf_mask = DL23_PBUF_SIZE_MASK,
		.pbuf_shift = DL23_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL23_CON0,
		.minlen_mask = DL23_MINLEN_MASK,
		.minlen_shift = DL23_MINLEN_SFT,
		.maxlen_reg = AFE_DL23_CON0,
		.maxlen_mask = DL23_MAXLEN_MASK,
		.maxlen_shift = DL23_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL24] = {
		.name = "DL24",
		.id = MT6991_MEMIF_DL24,
		.reg_ofs_base = AFE_DL24_BASE,
		.reg_ofs_cur = AFE_DL24_CUR,
		.reg_ofs_end = AFE_DL24_END,
		.reg_ofs_base_msb = AFE_DL24_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL24_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL24_END_MSB,
		.fs_reg = AFE_DL24_CON0,
		.fs_shift = DL24_SEL_FS_SFT,
		.fs_maskbit = DL24_SEL_FS_MASK,
		.mono_reg = AFE_DL24_CON0,
		.mono_shift = DL24_MONO_SFT,
		.enable_reg = AFE_DL24_CON0,
		.enable_shift = DL24_ON_SFT,
		.hd_reg = AFE_DL24_CON0,
		.hd_mask = DL24_HD_MODE_MASK,
		.hd_shift = DL24_HD_MODE_SFT,
		.hd_align_reg = AFE_DL24_CON0,
		.hd_align_mshift = DL24_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL24_CON0,
		.pbuf_mask = DL24_PBUF_SIZE_MASK,
		.pbuf_shift = DL24_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL24_CON0,
		.minlen_mask = DL24_MINLEN_MASK,
		.minlen_shift = DL24_MINLEN_SFT,
		.maxlen_reg = AFE_DL24_CON0,
		.maxlen_mask = DL24_MAXLEN_MASK,
		.maxlen_shift = DL24_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL25] = {
		.name = "DL25",
		.id = MT6991_MEMIF_DL25,
		.reg_ofs_base = AFE_DL25_BASE,
		.reg_ofs_cur = AFE_DL25_CUR,
		.reg_ofs_end = AFE_DL25_END,
		.reg_ofs_base_msb = AFE_DL25_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL25_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL25_END_MSB,
		.fs_reg = AFE_DL25_CON0,
		.fs_shift = DL25_SEL_FS_SFT,
		.fs_maskbit = DL25_SEL_FS_MASK,
		.mono_reg = AFE_DL25_CON0,
		.mono_shift = DL25_MONO_SFT,
		.enable_reg = AFE_DL25_CON0,
		.enable_shift = DL25_ON_SFT,
		.hd_reg = AFE_DL25_CON0,
		.hd_mask = DL25_HD_MODE_MASK,
		.hd_shift = DL25_HD_MODE_SFT,
		.hd_align_reg = AFE_DL25_CON0,
		.hd_align_mshift = DL25_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL25_CON0,
		.pbuf_mask = DL25_PBUF_SIZE_MASK,
		.pbuf_shift = DL25_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL25_CON0,
		.minlen_mask = DL25_MINLEN_MASK,
		.minlen_shift = DL25_MINLEN_SFT,
		.maxlen_reg = AFE_DL25_CON0,
		.maxlen_mask = DL25_MAXLEN_MASK,
		.maxlen_shift = DL25_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL26] = {
		.name = "DL26",
		.id = MT6991_MEMIF_DL26,
		.reg_ofs_base = AFE_DL26_BASE,
		.reg_ofs_cur = AFE_DL26_CUR,
		.reg_ofs_end = AFE_DL26_END,
		.reg_ofs_base_msb = AFE_DL26_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL26_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL26_END_MSB,
		.fs_reg = AFE_DL26_CON0,
		.fs_shift = DL26_SEL_FS_SFT,
		.fs_maskbit = DL26_SEL_FS_MASK,
		.mono_reg = AFE_DL26_CON0,
		.mono_shift = DL26_MONO_SFT,
		.enable_reg = AFE_DL26_CON0,
		.enable_shift = DL26_ON_SFT,
		.hd_reg = AFE_DL26_CON0,
		.hd_mask = DL26_HD_MODE_MASK,
		.hd_shift = DL26_HD_MODE_SFT,
		.hd_align_reg = AFE_DL26_CON0,
		.hd_align_mshift = DL26_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL26_CON0,
		.pbuf_mask = DL26_PBUF_SIZE_MASK,
		.pbuf_shift = DL26_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL26_CON0,
		.minlen_mask = DL26_MINLEN_MASK,
		.minlen_shift = DL26_MINLEN_SFT,
		.maxlen_reg = AFE_DL26_CON0,
		.maxlen_mask = DL26_MAXLEN_MASK,
		.maxlen_shift = DL26_MAXLEN_SFT,
	},
	[MT6991_MEMIF_DL_4CH] = {
		.name = "DL_4CH",
		.id = MT6991_MEMIF_DL_4CH,
		.reg_ofs_base = AFE_DL_4CH_BASE,
		.reg_ofs_cur = AFE_DL_4CH_CUR,
		.reg_ofs_end = AFE_DL_4CH_END,
		.reg_ofs_base_msb = AFE_DL_4CH_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL_4CH_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL_4CH_END_MSB,
		.fs_reg = AFE_DL_4CH_CON0,
		.fs_shift = DL_4CH_SEL_FS_SFT,
		.fs_maskbit = DL_4CH_SEL_FS_MASK,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DL_4CH_CON0,
		.enable_shift = DL_4CH_ON_SFT,
		.hd_reg = AFE_DL_4CH_CON0,
		.hd_mask = DL_4CH_HD_MODE_MASK,
		.hd_shift = DL_4CH_HD_MODE_SFT,
		.hd_align_reg = AFE_DL_4CH_CON0,
		.hd_align_mshift = DL_4CH_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL_4CH_CON0,
		.pbuf_mask = DL_4CH_PBUF_SIZE_MASK,
		.pbuf_shift = DL_4CH_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL_4CH_CON0,
		.minlen_mask = DL_4CH_MINLEN_MASK,
		.minlen_shift = DL_4CH_MINLEN_SFT,
		.maxlen_reg = AFE_DL_4CH_CON0,
		.maxlen_mask = DL_4CH_MAXLEN_MASK,
		.maxlen_shift = DL_4CH_MAXLEN_SFT,
		.ch_num_reg = AFE_DL_4CH_CON0,
		.ch_num_maskbit = DL_4CH_NUM_MASK,
		.ch_num_shift = DL_4CH_NUM_SFT,
	},
	[MT6991_MEMIF_DL_24CH] = {
		.name = "DL_24CH",
		.id = MT6991_MEMIF_DL_24CH,
		.reg_ofs_base = AFE_DL_24CH_BASE,
		.reg_ofs_cur = AFE_DL_24CH_CUR,
		.reg_ofs_end = AFE_DL_24CH_END,
		.reg_ofs_base_msb = AFE_DL_24CH_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL_24CH_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL_24CH_END_MSB,
		.fs_reg = AFE_DL_24CH_CON0,
		.fs_shift = DL_24CH_SEL_FS_SFT,
		.fs_maskbit = DL_24CH_SEL_FS_MASK,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DL_24CH_CON0,
		.enable_shift = DL_24CH_ON_SFT,
		.hd_reg = AFE_DL_24CH_CON0,
		.hd_mask = DL_24CH_HD_MODE_MASK,
		.hd_shift = DL_24CH_HD_MODE_SFT,
		.hd_align_reg = AFE_DL_24CH_CON0,
		.hd_align_mshift = DL_24CH_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL_24CH_CON0,
		.pbuf_mask = DL_24CH_PBUF_SIZE_MASK,
		.pbuf_shift = DL_24CH_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL_24CH_CON0,
		.minlen_mask = DL_24CH_MINLEN_MASK,
		.minlen_shift = DL_24CH_MINLEN_SFT,
		.maxlen_reg = AFE_DL_24CH_CON0,
		.maxlen_mask = DL_24CH_MAXLEN_MASK,
		.maxlen_shift = DL_24CH_MAXLEN_SFT,
		.ch_num_reg = AFE_DL_24CH_CON0,
		.ch_num_maskbit = DL_24CH_NUM_MASK,
		.ch_num_shift = DL_24CH_NUM_SFT,
	},
	[MT6991_MEMIF_VUL0] = {
		.name = "VUL0",
		.id = MT6991_MEMIF_VUL0,
		.reg_ofs_base = AFE_VUL0_BASE,
		.reg_ofs_cur = AFE_VUL0_CUR,
		.reg_ofs_end = AFE_VUL0_END,
		.reg_ofs_base_msb = AFE_VUL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL0_END_MSB,
		.fs_reg = AFE_VUL0_CON0,
		.fs_shift = VUL0_SEL_FS_SFT,
		.fs_maskbit = VUL0_SEL_FS_MASK,
		.mono_reg = AFE_VUL0_CON0,
		.mono_shift = VUL0_MONO_SFT,
		.enable_reg = AFE_VUL0_CON0,
		.enable_shift = VUL0_ON_SFT,
		.hd_reg = AFE_VUL0_CON0,
		.hd_mask = VUL0_HD_MODE_MASK,
		.hd_shift = VUL0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL0_CON0,
		.hd_align_mshift = VUL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL1] = {
		.name = "VUL1",
		.id = MT6991_MEMIF_VUL1,
		.reg_ofs_base = AFE_VUL1_BASE,
		.reg_ofs_cur = AFE_VUL1_CUR,
		.reg_ofs_end = AFE_VUL1_END,
		.reg_ofs_base_msb = AFE_VUL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL1_END_MSB,
		.fs_reg = AFE_VUL1_CON0,
		.fs_shift = VUL1_SEL_FS_SFT,
		.fs_maskbit = VUL1_SEL_FS_MASK,
		.mono_reg = AFE_VUL1_CON0,
		.mono_shift = VUL1_MONO_SFT,
		.enable_reg = AFE_VUL1_CON0,
		.enable_shift = VUL1_ON_SFT,
		.hd_reg = AFE_VUL1_CON0,
		.hd_mask = VUL1_HD_MODE_MASK,
		.hd_shift = VUL1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL1_CON0,
		.hd_align_mshift = VUL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT6991_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_VUL2_CON0,
		.fs_shift = VUL2_SEL_FS_SFT,
		.fs_maskbit = VUL2_SEL_FS_MASK,
		.mono_reg = AFE_VUL2_CON0,
		.mono_shift = VUL2_MONO_SFT,
		.enable_reg = AFE_VUL2_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_VUL2_CON0,
		.hd_mask = VUL2_HD_MODE_MASK,
		.hd_shift = VUL2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL2_CON0,
		.hd_align_mshift = VUL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL3] = {
		.name = "VUL3",
		.id = MT6991_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.reg_ofs_end = AFE_VUL3_END,
		.reg_ofs_base_msb = AFE_VUL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL3_END_MSB,
		.fs_reg = AFE_VUL3_CON0,
		.fs_shift = VUL3_SEL_FS_SFT,
		.fs_maskbit = VUL3_SEL_FS_MASK,
		.mono_reg = AFE_VUL3_CON0,
		.mono_shift = VUL3_MONO_SFT,
		.enable_reg = AFE_VUL3_CON0,
		.enable_shift = VUL3_ON_SFT,
		.hd_reg = AFE_VUL3_CON0,
		.hd_mask = VUL3_HD_MODE_MASK,
		.hd_shift = VUL3_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL3_CON0,
		.hd_align_mshift = VUL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL4] = {
		.name = "VUL4",
		.id = MT6991_MEMIF_VUL4,
		.reg_ofs_base = AFE_VUL4_BASE,
		.reg_ofs_cur = AFE_VUL4_CUR,
		.reg_ofs_end = AFE_VUL4_END,
		.reg_ofs_base_msb = AFE_VUL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL4_END_MSB,
		.fs_reg = AFE_VUL4_CON0,
		.fs_shift = VUL4_SEL_FS_SFT,
		.fs_maskbit = VUL4_SEL_FS_MASK,
		.mono_reg = AFE_VUL4_CON0,
		.mono_shift = VUL4_MONO_SFT,
		.enable_reg = AFE_VUL4_CON0,
		.enable_shift = VUL4_ON_SFT,
		.hd_reg = AFE_VUL4_CON0,
		.hd_mask = VUL4_HD_MODE_MASK,
		.hd_shift = VUL4_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL4_CON0,
		.hd_align_mshift = VUL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL5] = {
		.name = "VUL5",
		.id = MT6991_MEMIF_VUL5,
		.reg_ofs_base = AFE_VUL5_BASE,
		.reg_ofs_cur = AFE_VUL5_CUR,
		.reg_ofs_end = AFE_VUL5_END,
		.reg_ofs_base_msb = AFE_VUL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL5_END_MSB,
		.fs_reg = AFE_VUL5_CON0,
		.fs_shift = VUL5_SEL_FS_SFT,
		.fs_maskbit = VUL5_SEL_FS_MASK,
		.mono_reg = AFE_VUL5_CON0,
		.mono_shift = VUL5_MONO_SFT,
		.enable_reg = AFE_VUL5_CON0,
		.enable_shift = VUL5_ON_SFT,
		.hd_reg = AFE_VUL5_CON0,
		.hd_mask = VUL5_HD_MODE_MASK,
		.hd_shift = VUL5_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL5_CON0,
		.hd_align_mshift = VUL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL6] = {
		.name = "VUL6",
		.id = MT6991_MEMIF_VUL6,
		.reg_ofs_base = AFE_VUL6_BASE,
		.reg_ofs_cur = AFE_VUL6_CUR,
		.reg_ofs_end = AFE_VUL6_END,
		.reg_ofs_base_msb = AFE_VUL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL6_END_MSB,
		.fs_reg = AFE_VUL6_CON0,
		.fs_shift = VUL6_SEL_FS_SFT,
		.fs_maskbit = VUL6_SEL_FS_MASK,
		.mono_reg = AFE_VUL6_CON0,
		.mono_shift = VUL6_MONO_SFT,
		.enable_reg = AFE_VUL6_CON0,
		.enable_shift = VUL6_ON_SFT,
		.hd_reg = AFE_VUL6_CON0,
		.hd_mask = VUL6_HD_MODE_MASK,
		.hd_shift = VUL6_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL6_CON0,
		.hd_align_mshift = VUL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL7] = {
		.name = "VUL7",
		.id = MT6991_MEMIF_VUL7,
		.reg_ofs_base = AFE_VUL7_BASE,
		.reg_ofs_cur = AFE_VUL7_CUR,
		.reg_ofs_end = AFE_VUL7_END,
		.reg_ofs_base_msb = AFE_VUL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL7_END_MSB,
		.fs_reg = AFE_VUL7_CON0,
		.fs_shift = VUL7_SEL_FS_SFT,
		.fs_maskbit = VUL7_SEL_FS_MASK,
		.mono_reg = AFE_VUL7_CON0,
		.mono_shift = VUL7_MONO_SFT,
		.enable_reg = AFE_VUL7_CON0,
		.enable_shift = VUL7_ON_SFT,
		.hd_reg = AFE_VUL7_CON0,
		.hd_mask = VUL7_HD_MODE_MASK,
		.hd_shift = VUL7_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL7_CON0,
		.hd_align_mshift = VUL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL8] = {
		.name = "VUL8",
		.id = MT6991_MEMIF_VUL8,
		.reg_ofs_base = AFE_VUL8_BASE,
		.reg_ofs_cur = AFE_VUL8_CUR,
		.reg_ofs_end = AFE_VUL8_END,
		.reg_ofs_base_msb = AFE_VUL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL8_END_MSB,
		.fs_reg = AFE_VUL8_CON0,
		.fs_shift = VUL8_SEL_FS_SFT,
		.fs_maskbit = VUL8_SEL_FS_MASK,
		.mono_reg = AFE_VUL8_CON0,
		.mono_shift = VUL8_MONO_SFT,
		.enable_reg = AFE_VUL8_CON0,
		.enable_shift = VUL8_ON_SFT,
		.hd_reg = AFE_VUL8_CON0,
		.hd_mask = VUL8_HD_MODE_MASK,
		.hd_shift = VUL8_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL8_CON0,
		.hd_align_mshift = VUL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL9] = {
		.name = "VUL9",
		.id = MT6991_MEMIF_VUL9,
		.reg_ofs_base = AFE_VUL9_BASE,
		.reg_ofs_cur = AFE_VUL9_CUR,
		.reg_ofs_end = AFE_VUL9_END,
		.reg_ofs_base_msb = AFE_VUL9_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL9_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL9_END_MSB,
		.fs_reg = AFE_VUL9_CON0,
		.fs_shift = VUL9_SEL_FS_SFT,
		.fs_maskbit = VUL9_SEL_FS_MASK,
		.mono_reg = AFE_VUL9_CON0,
		.mono_shift = VUL9_MONO_SFT,
		.enable_reg = AFE_VUL9_CON0,
		.enable_shift = VUL9_ON_SFT,
		.hd_reg = AFE_VUL9_CON0,
		.hd_mask = VUL9_HD_MODE_MASK,
		.hd_shift = VUL9_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL9_CON0,
		.hd_align_mshift = VUL9_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL10] = {
		.name = "VUL10",
		.id = MT6991_MEMIF_VUL10,
		.reg_ofs_base = AFE_VUL10_BASE,
		.reg_ofs_cur = AFE_VUL10_CUR,
		.reg_ofs_end = AFE_VUL10_END,
		.reg_ofs_base_msb = AFE_VUL10_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL10_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL10_END_MSB,
		.fs_reg = AFE_VUL10_CON0,
		.fs_shift = VUL10_SEL_FS_SFT,
		.fs_maskbit = VUL10_SEL_FS_MASK,
		.mono_reg = AFE_VUL10_CON0,
		.mono_shift = VUL10_MONO_SFT,
		.enable_reg = AFE_VUL10_CON0,
		.enable_shift = VUL10_ON_SFT,
		.hd_reg = AFE_VUL10_CON0,
		.hd_mask = VUL10_HD_MODE_MASK,
		.hd_shift = VUL10_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL10_CON0,
		.hd_align_mshift = VUL10_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL24] = {
		.name = "VUL24",
		.id = MT6991_MEMIF_VUL24,
		.reg_ofs_base = AFE_VUL24_BASE,
		.reg_ofs_cur = AFE_VUL24_CUR,
		.reg_ofs_end = AFE_VUL24_END,
		.reg_ofs_base_msb = AFE_VUL24_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL24_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL24_END_MSB,
		.fs_reg = AFE_VUL24_CON0,
		.fs_shift = VUL24_SEL_FS_SFT,
		.fs_maskbit = VUL24_SEL_FS_MASK,
		.mono_reg = AFE_VUL24_CON0,
		.mono_shift = VUL24_MONO_SFT,
		.enable_reg = AFE_VUL24_CON0,
		.enable_shift = VUL24_ON_SFT,
		.hd_reg = AFE_VUL24_CON0,
		.hd_mask = VUL24_HD_MODE_MASK,
		.hd_shift = VUL24_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL24_CON0,
		.hd_align_mshift = VUL24_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.out_on_use_reg = AFE_VUL24_CON0,
		.out_on_use_mask = OUT_ON_USE_VUL24_MASK,
		.out_on_use_shift = OUT_ON_USE_VUL24_SFT,
	},
	[MT6991_MEMIF_VUL25] = {
		.name = "VUL25",
		.id = MT6991_MEMIF_VUL25,
		.reg_ofs_base = AFE_VUL25_BASE,
		.reg_ofs_cur = AFE_VUL25_CUR,
		.reg_ofs_end = AFE_VUL25_END,
		.reg_ofs_base_msb = AFE_VUL25_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL25_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL25_END_MSB,
		.fs_reg = AFE_VUL25_CON0,
		.fs_shift = VUL25_SEL_FS_SFT,
		.fs_maskbit = VUL25_SEL_FS_MASK,
		.mono_reg = AFE_VUL25_CON0,
		.mono_shift = VUL25_MONO_SFT,
		.enable_reg = AFE_VUL25_CON0,
		.enable_shift = VUL25_ON_SFT,
		.hd_reg = AFE_VUL25_CON0,
		.hd_mask = VUL25_HD_MODE_MASK,
		.hd_shift = VUL25_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL25_CON0,
		.hd_align_mshift = VUL25_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.out_on_use_reg = AFE_VUL25_CON0,
		.out_on_use_mask = OUT_ON_USE_VUL25_MASK,
		.out_on_use_shift = OUT_ON_USE_VUL25_SFT,
	},
	[MT6991_MEMIF_VUL26] = {
		.name = "VUL26",
		.id = MT6991_MEMIF_VUL26,
		.reg_ofs_base = AFE_VUL26_BASE,
		.reg_ofs_cur = AFE_VUL26_CUR,
		.reg_ofs_end = AFE_VUL26_END,
		.reg_ofs_base_msb = AFE_VUL26_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL26_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL26_END_MSB,
		.fs_reg = AFE_VUL26_CON0,
		.fs_shift = VUL26_SEL_FS_SFT,
		.fs_maskbit = VUL26_SEL_FS_MASK,
		.mono_reg = AFE_VUL26_CON0,
		.mono_shift = VUL26_MONO_SFT,
		.enable_reg = AFE_VUL26_CON0,
		.enable_shift = VUL26_ON_SFT,
		.hd_reg = AFE_VUL26_CON0,
		.hd_mask = VUL26_HD_MODE_MASK,
		.hd_shift = VUL26_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL26_CON0,
		.hd_align_mshift = VUL26_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.out_on_use_reg = AFE_VUL26_CON0,
		.out_on_use_mask = OUT_ON_USE_VUL26_MASK,
		.out_on_use_shift = OUT_ON_USE_VUL26_SFT,
	},
	[MT6991_MEMIF_VUL_CM0] = {
		.name = "VUL_CM0",
		.id = MT6991_MEMIF_VUL_CM0,
		.reg_ofs_base = AFE_VUL_CM0_BASE,
		.reg_ofs_cur = AFE_VUL_CM0_CUR,
		.reg_ofs_end = AFE_VUL_CM0_END,
		.reg_ofs_base_msb = AFE_VUL_CM0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM0_END_MSB,
		//.fs_reg = AFE_CM0_CON0,
		//.fs_shift = AFE_CM0_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM0_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM0_CON0,
		//.mono_shift = VUL_CM0_MONO_SFT,
		.enable_reg = AFE_VUL_CM0_CON0,
		.enable_shift = VUL_CM0_ON_SFT,
		.hd_reg = AFE_VUL_CM0_CON0,
		.hd_mask = VUL_CM0_HD_MODE_MASK,
		.hd_shift = VUL_CM0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM0_CON0,
		.hd_align_mshift = VUL_CM0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL_CM1] = {
		.name = "VUL_CM1",
		.id = MT6991_MEMIF_VUL_CM1,
		.reg_ofs_base = AFE_VUL_CM1_BASE,
		.reg_ofs_cur = AFE_VUL_CM1_CUR,
		.reg_ofs_end = AFE_VUL_CM1_END,
		.reg_ofs_base_msb = AFE_VUL_CM1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM1_END_MSB,
		//.fs_reg = AFE_CM1_CON0,
		//.fs_shift = AFE_CM1_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM1_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM1_CON0,
		//.mono_shift = VUL_CM1_MONO_SFT,
		.enable_reg = AFE_VUL_CM1_CON0,
		.enable_shift = VUL_CM1_ON_SFT,
		.hd_reg = AFE_VUL_CM1_CON0,
		.hd_mask = VUL_CM1_HD_MODE_MASK,
		.hd_shift = VUL_CM1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM1_CON0,
		.hd_align_mshift = VUL_CM1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_VUL_CM2] = {
		.name = "VUL_CM2",
		.id = MT6991_MEMIF_VUL_CM2,
		.reg_ofs_base = AFE_VUL_CM2_BASE,
		.reg_ofs_cur = AFE_VUL_CM2_CUR,
		.reg_ofs_end = AFE_VUL_CM2_END,
		.reg_ofs_base_msb = AFE_VUL_CM2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM2_END_MSB,
		//.fs_reg = AFE_CM2_CON0,
		//.fs_shift = AFE_CM2_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM2_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM2_CON0,
		//.mono_shift = VUL_CM2_MONO_SFT,
		.enable_reg = AFE_VUL_CM2_CON0,
		.enable_shift = VUL_CM2_ON_SFT,
		.hd_reg = AFE_VUL_CM2_CON0,
		.hd_mask = VUL_CM2_HD_MODE_MASK,
		.hd_shift = VUL_CM2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM2_CON0,
		.hd_align_mshift = VUL_CM2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_ETDM_IN0] = {
		.name = "ETDM_IN0",
		.id = MT6991_MEMIF_ETDM_IN0,
		.reg_ofs_base = AFE_ETDM_IN0_BASE,
		.reg_ofs_cur = AFE_ETDM_IN0_CUR,
		.reg_ofs_end = AFE_ETDM_IN0_END,
		.reg_ofs_base_msb = AFE_ETDM_IN0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN0_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN0_END_MSB,
		.fs_reg = ETDM_IN0_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN0_CON0,
		.enable_shift = ETDM_IN0_ON_SFT,
		.hd_reg = AFE_ETDM_IN0_CON0,
		.hd_mask = ETDM_IN0_HD_MODE_MASK,
		.hd_shift = ETDM_IN0_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN0_CON0,
		.hd_align_mshift = ETDM_IN0_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_ETDM_IN1] = {
		.name = "ETDM_IN1",
		.id = MT6991_MEMIF_ETDM_IN1,
		.reg_ofs_base = AFE_ETDM_IN1_BASE,
		.reg_ofs_cur = AFE_ETDM_IN1_CUR,
		.reg_ofs_end = AFE_ETDM_IN1_END,
		.reg_ofs_base_msb = AFE_ETDM_IN1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN1_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN1_END_MSB,
		.fs_reg = ETDM_IN1_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN1_CON0,
		.enable_shift = ETDM_IN1_ON_SFT,
		.hd_reg = AFE_ETDM_IN1_CON0,
		.hd_mask = ETDM_IN1_HD_MODE_MASK,
		.hd_shift = ETDM_IN1_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN1_CON0,
		.hd_align_mshift = ETDM_IN1_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_ETDM_IN2] = {
		.name = "ETDM_IN2",
		.id = MT6991_MEMIF_ETDM_IN2,
		.reg_ofs_base = AFE_ETDM_IN2_BASE,
		.reg_ofs_cur = AFE_ETDM_IN2_CUR,
		.reg_ofs_end = AFE_ETDM_IN2_END,
		.reg_ofs_base_msb = AFE_ETDM_IN2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN2_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN2_END_MSB,
		.fs_reg = ETDM_IN2_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN2_CON0,
		.enable_shift = ETDM_IN2_ON_SFT,
		.hd_reg = AFE_ETDM_IN2_CON0,
		.hd_mask = ETDM_IN2_HD_MODE_MASK,
		.hd_shift = ETDM_IN2_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN2_CON0,
		.hd_align_mshift = ETDM_IN2_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_ETDM_IN3] = {
		.name = "ETDM_IN3",
		.id = MT6991_MEMIF_ETDM_IN3,
		.reg_ofs_base = AFE_ETDM_IN3_BASE,
		.reg_ofs_cur = AFE_ETDM_IN3_CUR,
		.reg_ofs_end = AFE_ETDM_IN3_END,
		.reg_ofs_base_msb = AFE_ETDM_IN3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN3_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN3_END_MSB,
		.fs_reg = ETDM_IN3_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN3_CON0,
		.enable_shift = ETDM_IN3_ON_SFT,
		.hd_reg = AFE_ETDM_IN3_CON0,
		.hd_mask = ETDM_IN3_HD_MODE_MASK,
		.hd_shift = ETDM_IN3_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN3_CON0,
		.hd_align_mshift = ETDM_IN3_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_ETDM_IN4] = {
		.name = "ETDM_IN4",
		.id = MT6991_MEMIF_ETDM_IN4,
		.reg_ofs_base = AFE_ETDM_IN4_BASE,
		.reg_ofs_cur = AFE_ETDM_IN4_CUR,
		.reg_ofs_end = AFE_ETDM_IN4_END,
		.reg_ofs_base_msb = AFE_ETDM_IN4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN4_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN4_END_MSB,
		.fs_reg = ETDM_IN4_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN4_CON0,
		.enable_shift = ETDM_IN4_ON_SFT,
		.hd_reg = AFE_ETDM_IN4_CON0,
		.hd_mask = ETDM_IN4_HD_MODE_MASK,
		.hd_shift = ETDM_IN4_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN4_CON0,
		.hd_align_mshift = ETDM_IN4_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_ETDM_IN6] = {
		.name = "ETDM_IN6",
		.id = MT6991_MEMIF_ETDM_IN6,
		.reg_ofs_base = AFE_ETDM_IN6_BASE,
		.reg_ofs_cur = AFE_ETDM_IN6_CUR,
		.reg_ofs_end = AFE_ETDM_IN6_END,
		.reg_ofs_base_msb = AFE_ETDM_IN6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN6_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN6_END_MSB,
		.fs_reg = ETDM_IN6_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN6_CON0,
		.enable_shift = ETDM_IN6_ON_SFT,
		.hd_reg = AFE_ETDM_IN6_CON0,
		.hd_mask = ETDM_IN6_HD_MODE_MASK,
		.hd_shift = ETDM_IN6_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN6_CON0,
		.hd_align_mshift = ETDM_IN6_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6991_MEMIF_HDMI] = {
		.name = "HDMI",
		.id = MT6991_MEMIF_HDMI,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.reg_ofs_end = AFE_HDMI_OUT_END,
		.reg_ofs_base_msb = AFE_HDMI_OUT_BASE_MSB,
		.reg_ofs_cur_msb = AFE_HDMI_OUT_CUR_MSB,
		.reg_ofs_end_msb = AFE_HDMI_OUT_END_MSB,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = -1,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_HDMI_OUT_CON0,
		.enable_shift = HDMI_OUT_ON_SFT,
		.hd_reg = AFE_HDMI_OUT_CON0,
		.hd_mask = HDMI_OUT_HD_MODE_MASK,
		.hd_shift = HDMI_OUT_HD_MODE_SFT,
		.hd_align_reg = AFE_HDMI_OUT_CON0,
		.hd_align_mshift = HDMI_OUT_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_HDMI_OUT_CON0,
		.pbuf_mask = HDMI_OUT_PBUF_SIZE_MASK,
		.pbuf_shift = HDMI_OUT_PBUF_SIZE_SFT,
		.minlen_reg = AFE_HDMI_OUT_CON0,
		.minlen_mask = HDMI_OUT_MINLEN_MASK,
		.minlen_shift = HDMI_OUT_MINLEN_SFT,
	},
};

static const struct mtk_base_irq_data irq_data[MT6991_IRQ_NUM] = {
	[MT6991_IRQ_0] = {
		.id = MT6991_IRQ_0,
		.irq_cnt_reg = AFE_IRQ0_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ0_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ0_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ0_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ0_MCU_CFG0,
		.irq_en_shift = AFE_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ0_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ0_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ0_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_1] = {
		.id = MT6991_IRQ_1,
		.irq_cnt_reg = AFE_IRQ1_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ1_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ1_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ1_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ1_MCU_CFG0,
		.irq_en_shift = AFE_IRQ1_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ1_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ1_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ1_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_2] = {
		.id = MT6991_IRQ_2,
		.irq_cnt_reg = AFE_IRQ2_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ2_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ2_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ2_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ2_MCU_CFG0,
		.irq_en_shift = AFE_IRQ2_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ2_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ2_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ2_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_3] = {
		.id = MT6991_IRQ_3,
		.irq_cnt_reg = AFE_IRQ3_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ3_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ3_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ3_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ3_MCU_CFG0,
		.irq_en_shift = AFE_IRQ3_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ3_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ3_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ3_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_4] = {
		.id = MT6991_IRQ_4,
		.irq_cnt_reg = AFE_IRQ4_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ4_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ4_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ4_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ4_MCU_CFG0,
		.irq_en_shift = AFE_IRQ4_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ4_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ4_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ4_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_5] = {
		.id = MT6991_IRQ_5,
		.irq_cnt_reg = AFE_IRQ5_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ5_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ5_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ5_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ5_MCU_CFG0,
		.irq_en_shift = AFE_IRQ5_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ5_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ5_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ5_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_6] = {
		.id = MT6991_IRQ_6,
		.irq_cnt_reg = AFE_IRQ6_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ6_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ6_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ6_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ6_MCU_CFG0,
		.irq_en_shift = AFE_IRQ6_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ6_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ6_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ6_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_7] = {
		.id = MT6991_IRQ_7,
		.irq_cnt_reg = AFE_IRQ7_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ7_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ7_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ7_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ7_MCU_CFG0,
		.irq_en_shift = AFE_IRQ7_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ7_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ7_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ7_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_8] = {
		.id = MT6991_IRQ_8,
		.irq_cnt_reg = AFE_IRQ8_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ8_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ8_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ8_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ8_MCU_CFG0,
		.irq_en_shift = AFE_IRQ8_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ8_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ8_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ8_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_9] = {
		.id = MT6991_IRQ_9,
		.irq_cnt_reg = AFE_IRQ9_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ9_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ9_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ9_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ9_MCU_CFG0,
		.irq_en_shift = AFE_IRQ9_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ9_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ9_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ9_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_10] = {
		.id = MT6991_IRQ_10,
		.irq_cnt_reg = AFE_IRQ10_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ10_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ10_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ10_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ10_MCU_CFG0,
		.irq_en_shift = AFE_IRQ10_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ10_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ10_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ10_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_11] = {
		.id = MT6991_IRQ_11,
		.irq_cnt_reg = AFE_IRQ11_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ11_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ11_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ11_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ11_MCU_CFG0,
		.irq_en_shift = AFE_IRQ11_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ11_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ11_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ11_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_12] = {
		.id = MT6991_IRQ_12,
		.irq_cnt_reg = AFE_IRQ12_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ12_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ12_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ12_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ12_MCU_CFG0,
		.irq_en_shift = AFE_IRQ12_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ12_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ12_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ12_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_13] = {
		.id = MT6991_IRQ_13,
		.irq_cnt_reg = AFE_IRQ13_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ13_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ13_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ13_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ13_MCU_CFG0,
		.irq_en_shift = AFE_IRQ13_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ13_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ13_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ13_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_14] = {
		.id = MT6991_IRQ_14,
		.irq_cnt_reg = AFE_IRQ14_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ14_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ14_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ14_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ14_MCU_CFG0,
		.irq_en_shift = AFE_IRQ14_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ14_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ14_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ14_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_15] = {
		.id = MT6991_IRQ_15,
		.irq_cnt_reg = AFE_IRQ15_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ15_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ15_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ15_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ15_MCU_CFG0,
		.irq_en_shift = AFE_IRQ15_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ15_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ15_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ15_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_16] = {
		.id = MT6991_IRQ_16,
		.irq_cnt_reg = AFE_IRQ16_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ16_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ16_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ16_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ16_MCU_CFG0,
		.irq_en_shift = AFE_IRQ16_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ16_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ16_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ16_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_17] = {
		.id = MT6991_IRQ_17,
		.irq_cnt_reg = AFE_IRQ17_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ17_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ17_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ17_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ17_MCU_CFG0,
		.irq_en_shift = AFE_IRQ17_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ17_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ17_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ17_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_18] = {
		.id = MT6991_IRQ_18,
		.irq_cnt_reg = AFE_IRQ18_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ18_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ18_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ18_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ18_MCU_CFG0,
		.irq_en_shift = AFE_IRQ18_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ18_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ18_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ18_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_19] = {
		.id = MT6991_IRQ_19,
		.irq_cnt_reg = AFE_IRQ19_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ19_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ19_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ19_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ19_MCU_CFG0,
		.irq_en_shift = AFE_IRQ19_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ19_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ19_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ19_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_20] = {
		.id = MT6991_IRQ_20,
		.irq_cnt_reg = AFE_IRQ20_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ20_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ20_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ20_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ20_MCU_CFG0,
		.irq_en_shift = AFE_IRQ20_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ20_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ20_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ20_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_21] = {
		.id = MT6991_IRQ_21,
		.irq_cnt_reg = AFE_IRQ21_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ21_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ21_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ21_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ21_MCU_CFG0,
		.irq_en_shift = AFE_IRQ21_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ21_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ21_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ21_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_22] = {
		.id = MT6991_IRQ_22,
		.irq_cnt_reg = AFE_IRQ22_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ22_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ22_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ22_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ22_MCU_CFG0,
		.irq_en_shift = AFE_IRQ22_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ22_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ22_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ22_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_23] = {
		.id = MT6991_IRQ_23,
		.irq_cnt_reg = AFE_IRQ23_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ23_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ23_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ23_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ23_MCU_CFG0,
		.irq_en_shift = AFE_IRQ23_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ23_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ23_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ23_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_24] = {
		.id = MT6991_IRQ_24,
		.irq_cnt_reg = AFE_IRQ24_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ24_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ24_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ24_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ24_MCU_CFG0,
		.irq_en_shift = AFE_IRQ24_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ24_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ24_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ24_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_25] = {
		.id = MT6991_IRQ_25,
		.irq_cnt_reg = AFE_IRQ25_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ25_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ25_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ25_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ25_MCU_CFG0,
		.irq_en_shift = AFE_IRQ25_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ25_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ25_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ25_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_26] = {
		.id = MT6991_IRQ_26,
		.irq_cnt_reg = AFE_IRQ26_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ26_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ26_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ26_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ26_MCU_CFG0,
		.irq_en_shift = AFE_IRQ26_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ26_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ26_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ26_MCU_SCP_EN_SFT,
	},
	[MT6991_IRQ_31] = {
		.id = MT6991_CUS_IRQ_TDM,
		.irq_cnt_reg = AFE_CUSTOM_IRQ0_MCU_CFG1,
		.irq_cnt_shift = AFE_CUSTOM_IRQ0_MCU_CNT_SFT,
		.irq_cnt_maskbit = AFE_CUSTOM_IRQ0_MCU_CNT_MASK,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_en_reg = AFE_CUSTOM_IRQ0_MCU_CFG0,
		.irq_en_shift = AFE_CUSTOM_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_CUSTOM_IRQ0_MCU_CFG1,
		.irq_clr_shift = AFE_CUSTOM_IRQ0_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_CUSTOM_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_CUSTOM_IRQ_MCU_SCP_EN,
	},
};

static const int memif_irq_usage[MT6991_MEMIF_NUM] = {
	/* TODO: verify each memif & irq */
	[MT6991_MEMIF_DL0] = MT6991_IRQ_0,
	[MT6991_MEMIF_DL1] = MT6991_IRQ_1,
	[MT6991_MEMIF_DL2] = MT6991_IRQ_2,
	[MT6991_MEMIF_DL3] = MT6991_IRQ_3,
	[MT6991_MEMIF_DL4] = MT6991_IRQ_4,
	[MT6991_MEMIF_DL5] = MT6991_IRQ_5,
	[MT6991_MEMIF_DL6] = MT6991_IRQ_6,
	[MT6991_MEMIF_DL7] = MT6991_IRQ_7,
	[MT6991_MEMIF_DL8] = MT6991_IRQ_8,
	[MT6991_MEMIF_DL23] = MT6991_IRQ_9,
	[MT6991_MEMIF_DL24] = MT6991_IRQ_10,
	[MT6991_MEMIF_DL25] = MT6991_IRQ_11,
	[MT6991_MEMIF_DL26] = MT6991_IRQ_0,
	[MT6991_MEMIF_DL_4CH] = MT6991_IRQ_0,
	[MT6991_MEMIF_DL_24CH] = MT6991_IRQ_12,
	[MT6991_MEMIF_VUL0] = MT6991_IRQ_13,
	[MT6991_MEMIF_VUL1] = MT6991_IRQ_14,
	[MT6991_MEMIF_VUL2] = MT6991_IRQ_15,
	[MT6991_MEMIF_VUL3] = MT6991_IRQ_16,
	[MT6991_MEMIF_VUL4] = MT6991_IRQ_17,
	[MT6991_MEMIF_VUL5] = MT6991_IRQ_18,
	[MT6991_MEMIF_VUL6] = MT6991_IRQ_19,
	[MT6991_MEMIF_VUL7] = MT6991_IRQ_20,
	[MT6991_MEMIF_VUL8] = MT6991_IRQ_21,
	[MT6991_MEMIF_VUL9] = MT6991_IRQ_22,
	[MT6991_MEMIF_VUL10] = MT6991_IRQ_23,
	[MT6991_MEMIF_VUL24] = MT6991_IRQ_24,
	[MT6991_MEMIF_VUL25] = MT6991_IRQ_25,
	[MT6991_MEMIF_VUL26] = MT6991_IRQ_0,
	[MT6991_MEMIF_VUL_CM0] = MT6991_IRQ_26,
	[MT6991_MEMIF_VUL_CM1] = MT6991_IRQ_0,
	[MT6991_MEMIF_VUL_CM2] = MT6991_IRQ_0,
	[MT6991_MEMIF_ETDM_IN0] = MT6991_IRQ_0,
	[MT6991_MEMIF_ETDM_IN1] = MT6991_IRQ_0,
	[MT6991_MEMIF_ETDM_IN2] = MT6991_IRQ_0,
	[MT6991_MEMIF_ETDM_IN3] = MT6991_IRQ_0,
	[MT6991_MEMIF_ETDM_IN4] = MT6991_IRQ_0,
	[MT6991_MEMIF_ETDM_IN6] = MT6991_IRQ_0,
	[MT6991_MEMIF_HDMI] = MT6991_IRQ_31
};
#endif

static u32 pcm_buffer_ms = 160;
module_param(pcm_buffer_ms, uint, 0644);
MODULE_PARM_DESC(pcm_buffer_ms, "PCM substream buffer time in milliseconds");

static u32 pcm_periods_min = 2;
module_param(pcm_periods_min, uint, 0644);
MODULE_PARM_DESC(pcm_periods_min, "Minimum number of PCM periods");

static u32 pcm_periods_max = 16;
module_param(pcm_periods_max, uint, 0644);
MODULE_PARM_DESC(pcm_periods_max, "Maximum number of PCM periods");

static u32 pcm_period_ms_min = 10;
module_param(pcm_period_ms_min, uint, 0644);
MODULE_PARM_DESC(pcm_period_ms_min, "Minimum PCM period time in milliseconds");

static u32 pcm_period_ms_max = 80;
module_param(pcm_period_ms_max, uint, 0644);
MODULE_PARM_DESC(pcm_period_ms_max, "Maximum PCM period time in milliseconds");

/* Map for converting VirtIO format to ALSA format. */
static const snd_pcm_format_t g_v2a_format_map[] = {
	[VIRTIO_SND_PCM_FMT_IMA_ADPCM] = SNDRV_PCM_FORMAT_IMA_ADPCM,
	[VIRTIO_SND_PCM_FMT_MU_LAW] = SNDRV_PCM_FORMAT_MU_LAW,
	[VIRTIO_SND_PCM_FMT_A_LAW] = SNDRV_PCM_FORMAT_A_LAW,
	[VIRTIO_SND_PCM_FMT_S8] = SNDRV_PCM_FORMAT_S8,
	[VIRTIO_SND_PCM_FMT_U8] = SNDRV_PCM_FORMAT_U8,
	[VIRTIO_SND_PCM_FMT_S16] = SNDRV_PCM_FORMAT_S16_LE,
	[VIRTIO_SND_PCM_FMT_U16] = SNDRV_PCM_FORMAT_U16_LE,
	[VIRTIO_SND_PCM_FMT_S18_3] = SNDRV_PCM_FORMAT_S18_3LE,
	[VIRTIO_SND_PCM_FMT_U18_3] = SNDRV_PCM_FORMAT_U18_3LE,
	[VIRTIO_SND_PCM_FMT_S20_3] = SNDRV_PCM_FORMAT_S20_3LE,
	[VIRTIO_SND_PCM_FMT_U20_3] = SNDRV_PCM_FORMAT_U20_3LE,
	[VIRTIO_SND_PCM_FMT_S24_3] = SNDRV_PCM_FORMAT_S24_3LE,
	[VIRTIO_SND_PCM_FMT_U24_3] = SNDRV_PCM_FORMAT_U24_3LE,
	[VIRTIO_SND_PCM_FMT_S20] = SNDRV_PCM_FORMAT_S20_LE,
	[VIRTIO_SND_PCM_FMT_U20] = SNDRV_PCM_FORMAT_U20_LE,
	[VIRTIO_SND_PCM_FMT_S24] = SNDRV_PCM_FORMAT_S24_LE,
	[VIRTIO_SND_PCM_FMT_U24] = SNDRV_PCM_FORMAT_U24_LE,
	[VIRTIO_SND_PCM_FMT_S32] = SNDRV_PCM_FORMAT_S32_LE,
	[VIRTIO_SND_PCM_FMT_U32] = SNDRV_PCM_FORMAT_U32_LE,
	[VIRTIO_SND_PCM_FMT_FLOAT] = SNDRV_PCM_FORMAT_FLOAT_LE,
	[VIRTIO_SND_PCM_FMT_FLOAT64] = SNDRV_PCM_FORMAT_FLOAT64_LE,
	[VIRTIO_SND_PCM_FMT_DSD_U8] = SNDRV_PCM_FORMAT_DSD_U8,
	[VIRTIO_SND_PCM_FMT_DSD_U16] = SNDRV_PCM_FORMAT_DSD_U16_LE,
	[VIRTIO_SND_PCM_FMT_DSD_U32] = SNDRV_PCM_FORMAT_DSD_U32_LE,
	[VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME] =
		SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE
};

/* Map for converting VirtIO frame rate to ALSA frame rate. */
struct virtsnd_v2a_rate {
	unsigned int alsa_bit;
	unsigned int rate;
};

static const struct virtsnd_v2a_rate g_v2a_rate_map[] = {
	[VIRTIO_SND_PCM_RATE_5512] = { SNDRV_PCM_RATE_5512, 5512 },
	[VIRTIO_SND_PCM_RATE_8000] = { SNDRV_PCM_RATE_8000, 8000 },
	[VIRTIO_SND_PCM_RATE_11025] = { SNDRV_PCM_RATE_11025, 11025 },
	[VIRTIO_SND_PCM_RATE_16000] = { SNDRV_PCM_RATE_16000, 16000 },
	[VIRTIO_SND_PCM_RATE_22050] = { SNDRV_PCM_RATE_22050, 22050 },
	[VIRTIO_SND_PCM_RATE_32000] = { SNDRV_PCM_RATE_32000, 32000 },
	[VIRTIO_SND_PCM_RATE_44100] = { SNDRV_PCM_RATE_44100, 44100 },
	[VIRTIO_SND_PCM_RATE_48000] = { SNDRV_PCM_RATE_48000, 48000 },
	[VIRTIO_SND_PCM_RATE_64000] = { SNDRV_PCM_RATE_64000, 64000 },
	[VIRTIO_SND_PCM_RATE_88200] = { SNDRV_PCM_RATE_88200, 88200 },
	[VIRTIO_SND_PCM_RATE_96000] = { SNDRV_PCM_RATE_96000, 96000 },
	[VIRTIO_SND_PCM_RATE_176400] = { SNDRV_PCM_RATE_176400, 176400 },
	[VIRTIO_SND_PCM_RATE_192000] = { SNDRV_PCM_RATE_192000, 192000 }
};

int virtsnd_write_reg(int reg, unsigned int res)
{
	void __iomem *reg_va = 0;
	unsigned long flags = 0;

	reg_va = ioremap(reg, sizeof(reg_va));

	if (IS_ERR(reg_va))
		return PTR_ERR(reg_va);
	spin_lock_irqsave(&virtsnd_set_reg_lock, flags);
	writel(res, reg_va);
	spin_unlock_irqrestore(&virtsnd_set_reg_lock, flags);
	iounmap(reg_va);
	return 0;
}
int virtsnd_read_reg(int reg, unsigned int *res)
{
	void __iomem *reg_va = 0;

	reg_va = ioremap(reg, sizeof(reg_va));

	if (IS_ERR(reg_va))
		return PTR_ERR(reg_va);
	*res = readl(reg_va);
	iounmap(reg_va);
	return 0;
}
int virtsnd_mtk_reg_update_bits(int reg,
			   unsigned int mask,
			   unsigned int val)
{
	void __iomem *reg_va = 0;
	unsigned int tmp, orig;
	unsigned long flags = 0;

	if (reg < 0)
		return 0;

	reg_va = ioremap(reg, sizeof(reg_va));

	if (IS_ERR(reg_va))
		return PTR_ERR(reg_va);
	spin_lock_irqsave(&virtsnd_set_reg_lock, flags);
	orig = readl(reg_va);

	tmp = orig & ~mask;
	tmp |= val & mask;

	// if (tmp != orig)
		writel(tmp, reg_va);

	spin_unlock_irqrestore(&virtsnd_set_reg_lock, flags);
	iounmap(reg_va);
	return 0;
}

int virtsnd_mtk_reg_write(int reg, unsigned int val)
{
	if (reg < 0)
		return 0;
	return virtsnd_write_reg(reg, val);
}

/**
 * virtsnd_pcm_build_hw() - Parse substream config and build HW descriptor.
 * @vss: VirtIO substream.
 * @info: VirtIO substream information entry.
 *
 * Context: Any context.
 * Return: 0 on success, -EINVAL if configuration is invalid.
 */
static int virtsnd_pcm_build_hw(struct virtio_pcm_substream *vss,
				struct virtio_snd_pcm_info *info)
{
	struct virtio_device *vdev = vss->snd->vdev;
	unsigned int i;
	u64 values;
	size_t sample_max = 0;
	size_t sample_min = 0;

	vss->features = le32_to_cpu(info->features);

	/*
	 * TODO: set SNDRV_PCM_INFO_{BATCH,BLOCK_TRANSFER} if device supports
	 * only message-based transport.
	 */
	vss->hw.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_BATCH |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_PAUSE;

	if (!info->channels_min || info->channels_min > info->channels_max) {
		dev_info(&vdev->dev,
			"SID %u: invalid channel range [%u %u]\n",
			vss->sid, info->channels_min, info->channels_max);
		return -EINVAL;
	}

	vss->hw.channels_min = info->channels_min;
	vss->hw.channels_max = info->channels_max;

	values = le64_to_cpu(info->formats);

	vss->hw.formats = 0;

	for (i = 0; i < ARRAY_SIZE(g_v2a_format_map); ++i)
		if (values & (1ULL << i)) {
			snd_pcm_format_t alsa_fmt = g_v2a_format_map[i];
			int bytes = snd_pcm_format_physical_width(alsa_fmt) / 8;

			if (!sample_min || sample_min > bytes)
				sample_min = bytes;

			if (sample_max < bytes)
				sample_max = bytes;

			vss->hw.formats |= pcm_format_to_bits(alsa_fmt);
		}

	if (!vss->hw.formats) {
		dev_info(&vdev->dev,
			"SID %u: no supported PCM sample formats found\n",
			vss->sid);
		return -EINVAL;
	}

	values = le64_to_cpu(info->rates);

	vss->hw.rates = 0;

	for (i = 0; i < ARRAY_SIZE(g_v2a_rate_map); ++i)
		if (values & (1ULL << i)) {
			if (!vss->hw.rate_min ||
			    vss->hw.rate_min > g_v2a_rate_map[i].rate)
				vss->hw.rate_min = g_v2a_rate_map[i].rate;

			if (vss->hw.rate_max < g_v2a_rate_map[i].rate)
				vss->hw.rate_max = g_v2a_rate_map[i].rate;

			vss->hw.rates |= g_v2a_rate_map[i].alsa_bit;
		}

	if (!vss->hw.rates) {
		dev_info(&vdev->dev,
			"SID %u: no supported PCM frame rates found\n",
			vss->sid);
		return -EINVAL;
	}

	vss->hw.periods_min = pcm_periods_min;
	vss->hw.periods_max = pcm_periods_max;

	/*
	 * We must ensure that there is enough space in the buffer to store
	 * pcm_buffer_ms ms for the combination (Cmax, Smax, Rmax), where:
	 *   Cmax = maximum supported number of channels,
	 *   Smax = maximum supported sample size in bytes,
	 *   Rmax = maximum supported frame rate.
	 */
	vss->hw.buffer_bytes_max =
		PAGE_ALIGN(sample_max * vss->hw.channels_max * pcm_buffer_ms *
			   (vss->hw.rate_max / MSEC_PER_SEC));

	/*
	 * We must ensure that the minimum period size is enough to store
	 * pcm_period_ms_min ms for the combination (Cmin, Smin, Rmin), where:
	 *   Cmin = minimum supported number of channels,
	 *   Smin = minimum supported sample size in bytes,
	 *   Rmin = minimum supported frame rate.
	 */
	vss->hw.period_bytes_min =
		sample_min * vss->hw.channels_min * pcm_period_ms_min *
		(vss->hw.rate_min / MSEC_PER_SEC);

	/*
	 * We must ensure that the maximum period size is enough to store
	 * pcm_period_ms_max ms for the combination (Cmax, Smax, Rmax).
	 */
	vss->hw.period_bytes_max =
		sample_max * vss->hw.channels_max * pcm_period_ms_max *
		(vss->hw.rate_max / MSEC_PER_SEC);

	return 0;
}

/**
 * virtsnd_pcm_find() - Find the PCM device for the specified node ID.
 * @snd: VirtIO sound device.
 * @nid: Function node ID.
 *
 * Context: Any context.
 * Return: a pointer to the PCM device or ERR_PTR(-ENOENT).
 */
struct virtio_pcm *virtsnd_pcm_find(struct virtio_snd *snd, u32 nid)
{
	struct virtio_pcm *vpcm;

	list_for_each_entry(vpcm, &snd->pcm_list, list)
		if (vpcm->nid == nid)
			return vpcm;

	return ERR_PTR(-ENOENT);
}

/**
 * virtsnd_pcm_find_or_create() - Find or create the PCM device for the
 *                                specified node ID.
 * @snd: VirtIO sound device.
 * @nid: Function node ID.
 *
 * Context: Any context that permits to sleep.
 * Return: a pointer to the PCM device or ERR_PTR(-errno).
 */
struct virtio_pcm *virtsnd_pcm_find_or_create(struct virtio_snd *snd, u32 nid)
{
	struct virtio_device *vdev = snd->vdev;
	struct virtio_pcm *vpcm;

	vpcm = virtsnd_pcm_find(snd, nid);
	if (!IS_ERR(vpcm))
		return vpcm;

	vpcm = devm_kzalloc(&vdev->dev, sizeof(*vpcm), GFP_KERNEL);
	if (!vpcm)
		return ERR_PTR(-ENOMEM);

	vpcm->nid = nid;
	list_add_tail(&vpcm->list, &snd->pcm_list);

	return vpcm;
}

/**
 * virtsnd_pcm_validate() - Validate if the device can be started.
 * @vdev: VirtIO parent device.
 *
 * Context: Any context.
 * Return: 0 on success, -EINVAL on failure.
 */
int virtsnd_pcm_validate(struct virtio_device *vdev)
{
	if (pcm_periods_min < 2 || pcm_periods_min > pcm_periods_max) {
		dev_info(&vdev->dev,
			"invalid range [%u %u] of the number of PCM periods\n",
			pcm_periods_min, pcm_periods_max);
		return -EINVAL;
	}

	if (!pcm_period_ms_min || pcm_period_ms_min > pcm_period_ms_max) {
		dev_info(&vdev->dev,
			"invalid range [%u %u] of the size of the PCM period\n",
			pcm_period_ms_min, pcm_period_ms_max);
		return -EINVAL;
	}

	if (pcm_buffer_ms < pcm_periods_min * pcm_period_ms_min) {
		dev_info(&vdev->dev,
			"pcm_buffer_ms(=%u) value cannot be < %u ms\n",
			pcm_buffer_ms, pcm_periods_min * pcm_period_ms_min);
		return -EINVAL;
	}

	if (pcm_period_ms_max > pcm_buffer_ms / 2) {
		dev_info(&vdev->dev,
			"pcm_period_ms_max(=%u) value cannot be > %u ms\n",
			pcm_period_ms_max, pcm_buffer_ms / 2);
		return -EINVAL;
	}

	return 0;
}

#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
/**
 * virtsnd_pcm_period_elapsed() - Kernel work function to handle the elapsed
 *                                period state.
 * @work: Elapsed period work.
 *
 * The main purpose of this function is to call snd_pcm_period_elapsed() in
 * a process context, not in an interrupt context. This is necessary because PCM
 * devices operate in non-atomic mode.
 *
 * Context: Process context.
 */
static void virtsnd_pcm_period_elapsed(struct work_struct *work)
{
	struct virtio_pcm_substream *vss =
		container_of(work, struct virtio_pcm_substream, elapsed_period);

	snd_pcm_period_elapsed(vss->substream);
}
#endif

/**
 * virtsnd_pcm_parse_cfg() - Parse the stream configuration.
 * @snd: VirtIO sound device.
 *
 * This function is called during initial device initialization.
 *
 * Context: Any context that permits to sleep.
 * Return: 0 on success, -errno on failure.
 */
int virtsnd_pcm_parse_cfg(struct virtio_snd *snd)
{
	struct virtio_device *vdev = snd->vdev;
	struct virtio_snd_pcm_info *info;
	u32 i;
	int rc;

	virtio_cread_le(vdev, struct virtio_snd_config, streams,
			&snd->nsubstreams);
	if (!snd->nsubstreams)
		return 0;

	snd->substreams = devm_kcalloc(&vdev->dev, snd->nsubstreams,
				       sizeof(*snd->substreams), GFP_KERNEL);
	if (!snd->substreams)
		return -ENOMEM;

	info = kcalloc(snd->nsubstreams, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	rc = virtsnd_ctl_query_info(snd, VIRTIO_SND_R_PCM_INFO, 0,
				    snd->nsubstreams, sizeof(*info), info);
	if (rc)
		goto on_exit;

	for (i = 0; i < snd->nsubstreams; ++i) {
		struct virtio_pcm_substream *vss = &snd->substreams[i];
		struct virtio_pcm *vpcm;

		vss->snd = snd;
		vss->sid = i;
#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
		INIT_WORK(&vss->elapsed_period, virtsnd_pcm_period_elapsed);
#endif
		init_waitqueue_head(&vss->msg_empty);
		spin_lock_init(&vss->lock);

		rc = virtsnd_pcm_build_hw(vss, &info[i]);
		if (rc)
			goto on_exit;

		vss->nid = le32_to_cpu(info[i].hdr.hda_fn_nid);

		vpcm = virtsnd_pcm_find_or_create(snd, vss->nid);
		if (IS_ERR(vpcm)) {
			rc = PTR_ERR(vpcm);
			goto on_exit;
		}

		switch (info[i].direction) {
		case VIRTIO_SND_D_OUTPUT:
			vss->direction = SNDRV_PCM_STREAM_PLAYBACK;
			break;
		case VIRTIO_SND_D_INPUT:
			vss->direction = SNDRV_PCM_STREAM_CAPTURE;
			break;
		default:
			dev_info(&vdev->dev, "SID %u: unknown direction (%u)\n",
				vss->sid, info[i].direction);
			rc = -EINVAL;
			goto on_exit;
		}

		vpcm->streams[vss->direction].nsubstreams++;
	}

on_exit:
	kfree(info);

	return rc;
}

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
static const struct mtk_audio_sram_ops virtio_sram_ops = {
	.set_sram_mode = virtio_set_sram_mode,
};

static irqreturn_t virtio_snd_irq_handler(int irq_id, void *dev)
{
	struct virtio_snd *snd = dev;
	struct virtio_pcm *vpcm;
	struct virtio_pcm_stream *vs;
	struct virtio_pcm_substream *vss;
	struct mtk_base_afe_irq *irq;
	unsigned int status;
	unsigned int status_mcu;
	int i;
	struct arm_smccc_res res;
	unsigned int tmp_reg;
	struct mtk_base_afe *afe;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, 0x90000000, HWIRQ, 0, 0, 0, 0, 0, &res);
	// mcu_en = 1;

	tmp_reg = 0;
	afe = dev;

	status = res.a0;//regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status /*& mcu_en*/ & AFE_IRQ_STATUS_BITS;

	for (i = 0; i < snd->nsubstreams; ++i) {

		vss = &snd->substreams[i];

		vpcm = virtsnd_pcm_find(snd, vss->nid);
		if (IS_ERR(vpcm))
			goto err_irq;

		vs = &vpcm->streams[vss->direction];
		if (!vs->nsubstreams)
			continue;

		if (vpcm->irq_usage < 0)
			continue;

		irq = &snd->irqs[vpcm->irq_usage];

		if (status_mcu & (1 << irq->irq_data->irq_en_shift)){
			if (vss->xfer_enabled)
				snd_pcm_period_elapsed(vss->substream);
				// schedule_work(&vss->elapsed_period);
		}
	}

err_irq:
	/* clear irq */
	for (i = 0; i < MT6991_IRQ_NUM; ++i) {
		if (status_mcu & (0x1 << i)) {
			regmap_read(afe->regmap, irq->irq_data[i].irq_clr_reg, &tmp_reg);
			regmap_update_bits(afe->regmap, irq->irq_data[i].irq_clr_reg,
					0xc0000000,
					tmp_reg^0xc0000000);
		}
	}


	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, 0x90000001, HWIRQ, AFE_IRQ_STATUS_BITS, 0, 0, 0,
		      0, &res);
	return IRQ_HANDLED;
}

//static bool mt6991_is_volatile_reg(struct device *dev, unsigned int reg)
// {
//	/* these auto-gen reg has read-only bit, so put it as volatile */
//	/* volatile reg cannot be cached, so cannot be set when power off */
//	switch (reg) {
//	case AUDIO_TOP_CON0:    /* reg bit controlled by CCF */
//	case AUDIO_TOP_CON1:    /* reg bit controlled by CCF */
//	case AUDIO_TOP_CON2:
//	case AUDIO_TOP_CON3:
//	case AUDIO_TOP_CON4:
//	case AUD_TOP_MON_RG:
//	case AFE_APLL1_TUNER_MON0:
//	case AFE_APLL2_TUNER_MON0:
//	case AFE_SPM_CONTROL_ACK:
//	case AUDIO_TOP_IP_VERSION:
//	case AUDIO_ENGEN_CON0_MON:
//	case AFE_CONNSYS_I2S_IPM_VER_MON:
//	case AFE_CONNSYS_I2S_MON:
//	case AFE_PCM_INTF_MON:
//	case AFE_PCM_TOP_IP_VERSION:
//	case AFE_IRQ_MCU_STATUS:
//	case AFE_CUSTOM_IRQ_MCU_STATUS:
//	case AFE_IRQ_MCU_MON0:
//	case AFE_IRQ_MCU_MON1:
//	case AFE_IRQ_MCU_MON2:
//	case AFE_IRQ0_CNT_MON:
//	case AFE_IRQ1_CNT_MON:
//	case AFE_IRQ2_CNT_MON:
//	case AFE_IRQ3_CNT_MON:
//	case AFE_IRQ4_CNT_MON:
//	case AFE_IRQ5_CNT_MON:
//	case AFE_IRQ6_CNT_MON:
//	case AFE_IRQ7_CNT_MON:
//	case AFE_IRQ8_CNT_MON:
//	case AFE_IRQ9_CNT_MON:
//	case AFE_IRQ10_CNT_MON:
//	case AFE_IRQ11_CNT_MON:
//	case AFE_IRQ12_CNT_MON:
//	case AFE_IRQ13_CNT_MON:
//	case AFE_IRQ14_CNT_MON:
//	case AFE_IRQ15_CNT_MON:
//	case AFE_IRQ16_CNT_MON:
//	case AFE_IRQ17_CNT_MON:
//	case AFE_IRQ18_CNT_MON:
//	case AFE_IRQ19_CNT_MON:
//	case AFE_IRQ20_CNT_MON:
//	case AFE_IRQ21_CNT_MON:
//	case AFE_IRQ22_CNT_MON:
//	case AFE_IRQ23_CNT_MON:
//	case AFE_IRQ24_CNT_MON:
//	case AFE_IRQ25_CNT_MON:
//	case AFE_IRQ26_CNT_MON:
//	case AFE_CUSTOM_IRQ0_CNT_MON:
//	case AFE_STF_MON:
//	case AFE_STF_IP_VERSION:
//	case AFE_CM0_MON:
//	case AFE_CM0_IP_VERSION:
//	case AFE_CM1_MON:
//	case AFE_CM1_IP_VERSION:
//	case AFE_ADDA_UL0_SRC_DEBUG_MON0:
//	case AFE_ADDA_UL0_SRC_MON0:
//	case AFE_ADDA_UL0_SRC_MON1:
//	case AFE_ADDA_UL0_IP_VERSION:
//	case AFE_ADDA_UL1_SRC_DEBUG_MON0:
//	case AFE_ADDA_UL1_SRC_MON0:
//	case AFE_ADDA_UL1_SRC_MON1:
//	case AFE_ADDA_UL1_IP_VERSION:
//	case AFE_MTKAIF_IPM_VER_MON:
//	case AFE_MTKAIF_MON:
//	case AFE_AUD_PAD_TOP_MON:
//	case AFE_ADDA_MTKAIFV4_MON0:
//	case AFE_ADDA_MTKAIFV4_MON1:
//	case AFE_ADDA6_MTKAIFV4_MON0:
//	case ETDM_IN0_MON:
//	case ETDM_IN1_MON:
//	case ETDM_IN2_MON:
//	case ETDM_IN4_MON:
//	case ETDM_IN6_MON:
//	case ETDM_OUT0_MON:
//	case ETDM_OUT1_MON:
//	case ETDM_OUT2_MON:
//	case ETDM_OUT4_MON:
//	case ETDM_OUT5_MON:
//	case ETDM_OUT6_MON:
//	case AFE_DPTX_MON:
//	case AFE_TDM_TOP_IP_VERSION:
//	case AFE_CONN_MON0:
//	case AFE_CONN_MON1:
//	case AFE_CONN_MON2:
//	case AFE_CONN_MON3:
//	case AFE_CONN_MON4:
//	case AFE_CONN_MON5:
//	case AFE_CBIP_SLV_DECODER_MON0:
//	case AFE_CBIP_SLV_DECODER_MON1:
//	case AFE_CBIP_SLV_MUX_MON0:
//	case AFE_CBIP_SLV_MUX_MON1:
//	case AFE_DL0_CUR_MSB:
//	case AFE_DL0_CUR:
//	case AFE_DL0_RCH_MON:
//	case AFE_DL0_LCH_MON:
//	case AFE_DL1_CUR_MSB:
//	case AFE_DL1_CUR:
//	case AFE_DL1_RCH_MON:
//	case AFE_DL1_LCH_MON:
//	case AFE_DL2_CUR_MSB:
//	case AFE_DL2_CUR:
//	case AFE_DL2_RCH_MON:
//	case AFE_DL2_LCH_MON:
//	case AFE_DL3_CUR_MSB:
//	case AFE_DL3_CUR:
//	case AFE_DL3_RCH_MON:
//	case AFE_DL3_LCH_MON:
//	case AFE_DL4_CUR_MSB:
//	case AFE_DL4_CUR:
//	case AFE_DL4_RCH_MON:
//	case AFE_DL4_LCH_MON:
//	case AFE_DL5_CUR_MSB:
//	case AFE_DL5_CUR:
//	case AFE_DL5_RCH_MON:
//	case AFE_DL5_LCH_MON:
//	case AFE_DL6_CUR_MSB:
//	case AFE_DL6_CUR:
//	case AFE_DL6_RCH_MON:
//	case AFE_DL6_LCH_MON:
//	case AFE_DL7_CUR_MSB:
//	case AFE_DL7_CUR:
//	case AFE_DL7_RCH_MON:
//	case AFE_DL7_LCH_MON:
//	case AFE_DL8_CUR_MSB:
//	case AFE_DL8_CUR:
//	case AFE_DL8_RCH_MON:
//	case AFE_DL8_LCH_MON:
//	case AFE_DL_24CH_CUR_MSB:
//	case AFE_DL_24CH_CUR:
//	case AFE_DL_4CH_CUR_MSB:
//	case AFE_DL_4CH_CUR:
//	case AFE_DL23_CUR_MSB:
//	case AFE_DL23_CUR:
//	case AFE_DL23_RCH_MON:
//	case AFE_DL23_LCH_MON:
//	case AFE_DL24_CUR_MSB:
//	case AFE_DL24_CUR:
//	case AFE_DL24_RCH_MON:
//	case AFE_DL24_LCH_MON:
//	case AFE_DL25_CUR_MSB:
//	case AFE_DL25_CUR:
//	case AFE_DL25_RCH_MON:
//	case AFE_DL25_LCH_MON:
//	case AFE_DL26_CUR_MSB:
//	case AFE_DL26_CUR:
//	case AFE_DL26_RCH_MON:
//	case AFE_DL26_LCH_MON:
//	case AFE_VUL0_CUR_MSB:
//	case AFE_VUL0_CUR:
//	case AFE_VUL1_CUR_MSB:
//	case AFE_VUL1_CUR:
//	case AFE_VUL2_CUR_MSB:
//	case AFE_VUL2_CUR:
//	case AFE_VUL3_CUR_MSB:
//	case AFE_VUL3_CUR:
//	case AFE_VUL4_CUR_MSB:
//	case AFE_VUL4_CUR:
//	case AFE_VUL5_CUR_MSB:
//	case AFE_VUL5_CUR:
//	case AFE_VUL6_CUR_MSB:
//	case AFE_VUL6_CUR:
//	case AFE_VUL7_CUR_MSB:
//	case AFE_VUL7_CUR:
//	case AFE_VUL8_CUR_MSB:
//	case AFE_VUL8_CUR:
//	case AFE_VUL9_CUR_MSB:
//	case AFE_VUL9_CUR:
//	case AFE_VUL10_CUR_MSB:
//	case AFE_VUL10_CUR:
//	case AFE_VUL24_CUR_MSB:
//	case AFE_VUL24_CUR:
//	case AFE_VUL25_CUR_MSB:
//	case AFE_VUL25_CUR:
//	case AFE_VUL25_RCH_MON:
//	case AFE_VUL25_LCH_MON:
//	case AFE_VUL26_CUR_MSB:
//	case AFE_VUL26_CUR:
//	case AFE_VUL_CM0_CUR_MSB:
//	case AFE_VUL_CM0_CUR:
//	case AFE_VUL_CM1_CUR_MSB:
//	case AFE_VUL_CM1_CUR:
//	case AFE_VUL_CM2_CUR_MSB:
//	case AFE_VUL_CM2_CUR:
//	case AFE_ETDM_IN0_CUR_MSB:
//	case AFE_ETDM_IN0_CUR:
//	case AFE_ETDM_IN1_CUR_MSB:
//	case AFE_ETDM_IN1_CUR:
//	case AFE_ETDM_IN2_CUR_MSB:
//	case AFE_ETDM_IN2_CUR:
//	case AFE_ETDM_IN3_CUR_MSB:
//	case AFE_ETDM_IN3_CUR:
//	case AFE_ETDM_IN4_CUR_MSB:
//	case AFE_ETDM_IN4_CUR:
//	case AFE_ETDM_IN6_CUR_MSB:
//	case AFE_ETDM_IN6_CUR:
//	case AFE_HDMI_OUT_CUR_MSB:
//	case AFE_HDMI_OUT_CUR:
//	case AFE_HDMI_OUT_END:
//	case AFE_PROT_SIDEBAND0_MON:
//	case AFE_PROT_SIDEBAND1_MON:
//	case AFE_PROT_SIDEBAND2_MON:
//	case AFE_PROT_SIDEBAND3_MON:
//	case AFE_DOMAIN_SIDEBAND0_MON:
//	case AFE_DOMAIN_SIDEBAND1_MON:
//	case AFE_DOMAIN_SIDEBAND2_MON:
//	case AFE_DOMAIN_SIDEBAND3_MON:
//	case AFE_DOMAIN_SIDEBAND4_MON:
//	case AFE_DOMAIN_SIDEBAND5_MON:
//	case AFE_DOMAIN_SIDEBAND6_MON:
//	case AFE_DOMAIN_SIDEBAND7_MON:
//	case AFE_DOMAIN_SIDEBAND8_MON:
//	case AFE_DOMAIN_SIDEBAND9_MON:
//	case AFE_PCM0_INTF_CON1_MASK_MON:
//	case AFE_PCM0_INTF_CON0_MASK_MON:
//	case AFE_CONNSYS_I2S_CON_MASK_MON:
//	case AFE_TDM_CON2_MASK_MON:
//	case AFE_MTKAIF0_CFG0_MASK_MON:
//	case AFE_MTKAIF1_CFG0_MASK_MON:
//	case AFE_ADDA_UL0_SRC_CON0_MASK_MON:
//	case AFE_ADDA_UL1_SRC_CON0_MASK_MON:
//	case AFE_ASRC_NEW_CON0:
//	case AFE_ASRC_NEW_CON6:
//	case AFE_ASRC_NEW_CON8:
//	case AFE_ASRC_NEW_CON9:
//	case AFE_ASRC_NEW_CON12:
//	case AFE_ASRC_NEW_IP_VERSION:
//	case AFE_GASRC0_NEW_CON0:
//	case AFE_GASRC0_NEW_CON6:
//	case AFE_GASRC0_NEW_CON8:
//	case AFE_GASRC0_NEW_CON9:
//	case AFE_GASRC0_NEW_CON10:
//	case AFE_GASRC0_NEW_CON11:
//	case AFE_GASRC0_NEW_CON12:
//	case AFE_GASRC0_NEW_IP_VERSION:
//	case AFE_GASRC1_NEW_CON0:
//	case AFE_GASRC1_NEW_CON6:
//	case AFE_GASRC1_NEW_CON8:
//	case AFE_GASRC1_NEW_CON9:
//	case AFE_GASRC1_NEW_CON12:
//	case AFE_GASRC1_NEW_IP_VERSION:
//	case AFE_GASRC2_NEW_CON0:
//	case AFE_GASRC2_NEW_CON6:
//	case AFE_GASRC2_NEW_CON8:
//	case AFE_GASRC2_NEW_CON9:
//	case AFE_GASRC2_NEW_CON12:
//	case AFE_GASRC2_NEW_IP_VERSION:
//	case AFE_GASRC3_NEW_CON0:
//	case AFE_GASRC3_NEW_CON6:
//	case AFE_GASRC3_NEW_CON8:
//	case AFE_GASRC3_NEW_CON9:
//	case AFE_GASRC3_NEW_CON12:
//	case AFE_GASRC3_NEW_IP_VERSION:
//	case AFE_GAIN0_CUR_L:
//	case AFE_GAIN0_CUR_R:
//	case AFE_GAIN1_CUR_L:
//	case AFE_GAIN1_CUR_R:
//	case AFE_GAIN2_CUR_L:
//	case AFE_GAIN2_CUR_R:
//	case AFE_GAIN3_CUR_L:
//	case AFE_GAIN3_CUR_R:
//	/* these reg would change in adsp */
//	case AFE_IRQ_MCU_EN:
//	case AFE_IRQ_MCU_DSP_EN:
//	case AFE_IRQ_MCU_DSP2_EN:
//	case AFE_DL5_CON0:
//	case AFE_DL6_CON0:
//	case AFE_DL23_CON0:
//	case AFE_DL_24CH_CON0:
//	case AFE_VUL1_CON0:
//	case AFE_VUL3_CON0:
//	case AFE_VUL4_CON0:
//	case AFE_VUL5_CON0:
//	case AFE_VUL9_CON0:
//	case AFE_VUL25_CON0:
//	case AFE_IRQ0_MCU_CFG0:
//	case AFE_IRQ1_MCU_CFG0:
//	case AFE_IRQ2_MCU_CFG0:
//	case AFE_IRQ3_MCU_CFG0:
//	case AFE_IRQ4_MCU_CFG0:
//	case AFE_IRQ5_MCU_CFG0:
//	case AFE_IRQ6_MCU_CFG0:
//	case AFE_IRQ7_MCU_CFG0:
//	case AFE_IRQ8_MCU_CFG0:
//	case AFE_IRQ9_MCU_CFG0:
//	case AFE_IRQ10_MCU_CFG0:
//	case AFE_IRQ11_MCU_CFG0:
//	case AFE_IRQ12_MCU_CFG0:
//	case AFE_IRQ13_MCU_CFG0:
//	case AFE_IRQ14_MCU_CFG0:
//	case AFE_IRQ15_MCU_CFG0:
//	case AFE_IRQ16_MCU_CFG0:
//	case AFE_IRQ17_MCU_CFG0:
//	case AFE_IRQ18_MCU_CFG0:
//	case AFE_IRQ19_MCU_CFG0:
//	case AFE_IRQ20_MCU_CFG0:
//	case AFE_IRQ21_MCU_CFG0:
//	case AFE_IRQ22_MCU_CFG0:
//	case AFE_IRQ23_MCU_CFG0:
//	case AFE_IRQ24_MCU_CFG0:
//	case AFE_IRQ25_MCU_CFG0:
//	case AFE_IRQ26_MCU_CFG0:
//	case AFE_IRQ0_MCU_CFG1:
//	case AFE_IRQ1_MCU_CFG1:
//	case AFE_IRQ2_MCU_CFG1:
//	case AFE_IRQ3_MCU_CFG1:
//	case AFE_IRQ4_MCU_CFG1:
//	case AFE_IRQ5_MCU_CFG1:
//	case AFE_IRQ6_MCU_CFG1:
//	case AFE_IRQ7_MCU_CFG1:
//	case AFE_IRQ8_MCU_CFG1:
//	case AFE_IRQ9_MCU_CFG1:
//	case AFE_IRQ10_MCU_CFG1:
//	case AFE_IRQ11_MCU_CFG1:
//	case AFE_IRQ12_MCU_CFG1:
//	case AFE_IRQ13_MCU_CFG1:
//	case AFE_IRQ14_MCU_CFG1:
//	case AFE_IRQ15_MCU_CFG1:
//	case AFE_IRQ16_MCU_CFG1:
//	case AFE_IRQ17_MCU_CFG1:
//	case AFE_IRQ18_MCU_CFG1:
//	case AFE_IRQ19_MCU_CFG1:
//	case AFE_IRQ20_MCU_CFG1:
//	case AFE_IRQ21_MCU_CFG1:
//	case AFE_IRQ22_MCU_CFG1:
//	case AFE_IRQ23_MCU_CFG1:
//	case AFE_IRQ24_MCU_CFG1:
//	case AFE_IRQ25_MCU_CFG1:
//	case AFE_IRQ26_MCU_CFG1:
//	/* for vow using */
//	case AFE_IRQ_MCU_SCP_EN:
//	case AFE_VUL_CM0_BASE_MSB:
//	case AFE_VUL_CM0_BASE:
//	case AFE_VUL_CM0_END_MSB:
//	case AFE_VUL_CM0_END:
//	case AFE_VUL_CM0_CON0:
//		return true;
//	default:
//		return false;
//	};
// }

static const struct regmap_config virtsnd_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	// .volatile_reg = mt6991_is_volatile_reg,

	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
};

unsigned int virtsnd_mtk_general_rate_transform(struct device *dev,
					   unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_RATE_8K;
	case 11025:
		return MTK_AFE_RATE_11K;
	case 12000:
		return MTK_AFE_RATE_12K;
	case 16000:
		return MTK_AFE_RATE_16K;
	case 22050:
		return MTK_AFE_RATE_22K;
	case 24000:
		return MTK_AFE_RATE_24K;
	case 32000:
		return MTK_AFE_RATE_32K;
	case 44100:
		return MTK_AFE_RATE_44K;
	case 48000:
		return MTK_AFE_RATE_48K;
	case 88200:
		return MTK_AFE_RATE_88K;
	case 96000:
		return MTK_AFE_RATE_96K;
	case 176400:
		return MTK_AFE_RATE_176K;
	case 192000:
		return MTK_AFE_RATE_192K;
	case 260000:
		return MTK_AFE_RATE_260K;
	case 352800:
		return MTK_AFE_RATE_352K;
	case 384000:
		return MTK_AFE_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_RATE_48K);
		return MTK_AFE_RATE_48K;
	}
}

static int virtsnd_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;

	return virtsnd_mtk_general_rate_transform(&snd->vdev->dev, rate);
}

#endif

#ifdef CONFIG_DEBUG_FS
/* debugfs ops */
static int virtsnd_mtk_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t virtsnd_mtk_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	struct virtio_snd *snd = file->private_data;
	const int size = 32768;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	int ret = 0;
	unsigned int value = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	virtsnd_read_reg(snd->res.start + AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON0 = 0x%x\n", value);
	virtsnd_read_reg(snd->res.start + AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON1 = 0x%x\n", value);
	virtsnd_read_reg(snd->res.start + AUDIO_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON2 = 0x%x\n", value);
	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static const struct file_operations virtsnd_mtk_debugfs_ops = {
	.open = virtsnd_mtk_debugfs_open,
	.read = virtsnd_mtk_debugfs_read,
};
#endif

/**
 * virtsnd_pcm_build_devs() - Build ALSA PCM devices.
 * @snd: VirtIO sound device.
 *
 * Context: Any context that permits to sleep.
 * Return: 0 on success, -errno on failure.
 */
int virtsnd_pcm_build_devs(struct virtio_snd *snd)
{
	struct virtio_device *vdev = snd->vdev;
	struct virtio_pcm *vpcm;
	u32 i;
	int rc;

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	char pcm_name[80];
	int cpsIndex = 0;
	// struct resource res;
	//struct regmap *regmap;
	//void __iomem *base_addr;
	//struct irq_domain *domain;
	int virq;
	struct device_node *np = of_find_node_by_name(NULL, "sound");

	if (np == NULL){
		dev_info(&vdev->dev, "afe node not found!\n");
		return -EINVAL;
	}

	if (of_address_to_resource(np, 0, &(snd->res))){
		dev_info(&vdev->dev, "of address to resource failed!\n");
		rc = ENOMEM;
		return rc;
	}

	virq = irq_create_mapping(NULL, HWIRQ);

	// rc = devm_request_irq(&vdev->dev, virq, virtio_snd_irq_handler, 0x00000000,
	//		       dev_name(&vdev->dev), (void *)snd);
	rc = devm_request_threaded_irq(&vdev->dev, virq,
					  NULL, virtio_snd_irq_handler,
					  IRQF_ONESHOT,
					  dev_name(&vdev->dev), (void *)snd);
	if (rc < 0) {
		dev_info(&vdev->dev, "failed to request_irq\n");
		return -EINVAL;
	}

	/* init irq */
	snd->irqs_size = MT6991_IRQ_NUM;
	snd->irqs = devm_kcalloc(&vdev->dev, snd->irqs_size, sizeof(*snd->irqs),
				 GFP_KERNEL);

	if (!snd->irqs)
		return -ENOMEM;

	for (i = 0; i < snd->irqs_size; i++)
		snd->irqs[i].irq_data = &irq_data[i];

	snd->irq_fs = virtsnd_irq_fs;

	snd->sram = devm_kzalloc(&vdev->dev, sizeof(struct mtk_audio_sram),
			 GFP_KERNEL);
	if (!snd->sram)
		return -ENOMEM;

	rc = mtk_audio_sram_init(&vdev->dev, snd->sram, &virtio_sram_ops);
	if (rc) {
		dev_info(&vdev->dev, "mtk_audio_sram_init failed: %d\n", rc);
		return rc;
	}

	rc = virtsnd_kctl_find_by_name(snd, "use_dram_only", &snd->remote_use_dram_only_ctl);
	if (rc) {
		dev_info(&vdev->dev,
				"use_dram_only kctl not found in virtio-end backend: %d\n", rc);
		return rc;
	}

	rc = virtsnd_kctl_find_by_name(snd, "sram_mode", &snd->remote_sram_mode_ctl);
	if (rc) {
		dev_info(&vdev->dev,
				"sram_mode kctl not found in virtio-end backend: %d\n", rc);
		return rc;
	}

	rc = virtsnd_kctl_find_by_name(snd, "passthrough_shm", &snd->remote_passthrough_shm_ctl);
	if (rc) {
		dev_info(&vdev->dev,
				"passthrough_shm kctl not found in virtio-end backend: %d\n",
				rc);
		return rc;
	}
#endif

	list_for_each_entry(vpcm, &snd->pcm_list, list) {
		unsigned int npbs =
			vpcm->streams[SNDRV_PCM_STREAM_PLAYBACK].nsubstreams;
		unsigned int ncps =
			vpcm->streams[SNDRV_PCM_STREAM_CAPTURE].nsubstreams;

		if (!npbs && !ncps)
			continue;

#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
		rc = snd_pcm_new(snd->card, VIRTIO_SND_CARD_DRIVER, vpcm->nid,
				 npbs, ncps, &vpcm->pcm);
#else
		if (ncps == 1 && npbs == 0) {
			if (vpcm->nid >= 10)
				cpsIndex = vpcm->nid - 10;
			snprintf(pcm_name, sizeof(pcm_name),
				"Capture_%u", ++cpsIndex);
		} else if (ncps == 0 && npbs == 1){
			if (vpcm->nid == 21){
				snprintf(pcm_name, sizeof(pcm_name),
					"Playback_%s", "HDMI");
			} else
				snprintf(pcm_name, sizeof(pcm_name),
					"Playback_%u", vpcm->nid);
		} else{
			snprintf(pcm_name, sizeof(pcm_name),
				VIRTIO_SND_CARD_DRIVER);
		}
		rc = snd_pcm_new(snd->card, pcm_name, vpcm->nid,
				 npbs, ncps, &vpcm->pcm);
#endif
		if (rc) {
			dev_info(&vdev->dev, "snd_pcm_new[%u] failed: %d\n",
				vpcm->nid, rc);
			return rc;
		}

		vpcm->pcm->info_flags = 0;
		vpcm->pcm->dev_class = SNDRV_PCM_CLASS_GENERIC;
		vpcm->pcm->dev_subclass = SNDRV_PCM_SUBCLASS_GENERIC_MIX;
#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
		snprintf(vpcm->pcm->name, sizeof(vpcm->pcm->name),
			 VIRTIO_SND_PCM_NAME " %u", vpcm->pcm->device);
// #else
//		snprintf(vpcm->pcm->name, sizeof(vpcm->pcm->name),
//			 "");
#endif
		vpcm->pcm->private_data = vpcm;
		vpcm->pcm->nonatomic = true;

		for (i = 0; i < ARRAY_SIZE(vpcm->streams); ++i) {
			struct virtio_pcm_stream *stream = &vpcm->streams[i];

			if (!stream->nsubstreams)
				continue;

			stream->substreams =
				devm_kcalloc(&vdev->dev, stream->nsubstreams,
					     sizeof(*stream->substreams),
					     GFP_KERNEL);
			if (!stream->substreams)
				return -ENOMEM;

			stream->nsubstreams = 0;
		}
	}

	for (i = 0; i < snd->nsubstreams; ++i) {
		struct virtio_pcm_stream *vs;
		struct virtio_pcm_substream *vss = &snd->substreams[i];

		vpcm = virtsnd_pcm_find(snd, vss->nid);
		if (IS_ERR(vpcm))
			return PTR_ERR(vpcm);

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
		vpcm->data = &memif_data[nid_to_mt6991_memifid[vss->nid].id];
		vpcm->irq_usage = memif_irq_usage[nid_to_mt6991_memifid[vss->nid].id];
		vpcm->const_irq = 1;
#endif
		vs = &vpcm->streams[vss->direction];
		vs->substreams[vs->nsubstreams++] = vss;
	}

	list_for_each_entry(vpcm, &snd->pcm_list, list) {

		for (i = 0; i < ARRAY_SIZE(vpcm->streams); ++i) {
			struct virtio_pcm_stream *vs = &vpcm->streams[i];
			struct snd_pcm_str *ks = &vpcm->pcm->streams[i];
			struct snd_pcm_substream *kss;

			if (!vs->nsubstreams)
				continue;

			for (kss = ks->substream; kss; kss = kss->next)
				vs->substreams[kss->number]->substream = kss;

			snd_pcm_set_ops(vpcm->pcm, i, &virtsnd_pcm_ops);
		}

		snd_pcm_set_managed_buffer_all(vpcm->pcm,
				SNDRV_DMA_TYPE_DEV, NULL,
					       0, 0);
	}

#ifdef CONFIG_DEBUG_FS
	/* debugfs */
	snd->debugfs = debugfs_create_file("mtksocaudio",
					   S_IFREG | 0444, NULL,
					   snd, &virtsnd_mtk_debugfs_ops);
#endif

	return 0;
}

/**
 * virtsnd_pcm_event() - Handle the PCM device event notification.
 * @snd: VirtIO sound device.
 * @event: VirtIO sound event.
 *
 * Context: Interrupt context.
 */
void virtsnd_pcm_event(struct virtio_snd *snd, struct virtio_snd_event *event)
{
	struct virtio_pcm_substream *vss;
	u32 sid = le32_to_cpu(event->data);

	if (sid >= snd->nsubstreams)
		return;

	vss = &snd->substreams[sid];

	switch (le32_to_cpu(event->hdr.code)) {
	case VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED:
		/* TODO: deal with shmem elapsed period */
		break;
	case VIRTIO_SND_EVT_PCM_XRUN:
		spin_lock(&vss->lock);
		if (vss->xfer_enabled)
			vss->xfer_xrun = true;
		spin_unlock(&vss->lock);
		break;
	}
}
