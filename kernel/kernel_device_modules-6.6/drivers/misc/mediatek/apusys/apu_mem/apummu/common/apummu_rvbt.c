// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/io.h>
#include <linux/printk.h>
#include "apummu_boot.h"

#define TBL_SRAM_SIZE			(0x3C00)
#define TBL_SIZE			(0x118)
#define TBL_NUM				(TBL_SRAM_SIZE / TBL_SIZE)

enum rsv_vsid {
	RSV_VSID_UPRV = 0,
	RSV_VSID_LOGGER,
	RSV_VSID_APMCU,
	RSV_VSID_GPU,
	RSV_VSID_MDLA,
	RSV_VSID_MVPU0,
	RSV_VSID_CE,
	RSV_VSID_MAX
};

#define VSID_RSV_LAST			(TBL_NUM - 1)
#define VSID_RSV_CE			(VSID_RSV_LAST - RSV_VSID_CE)
#define VSID_RSV_MVPU0			(VSID_RSV_LAST - RSV_VSID_MVPU0)
#define VSID_RSV_MDLA			(VSID_RSV_LAST - RSV_VSID_MDLA)
#define VSID_RSV_GPU			(VSID_RSV_LAST - RSV_VSID_GPU)
#define VSID_RSV_APMCU			(VSID_RSV_LAST - RSV_VSID_APMCU)
#define VSID_RSV_LOGGER			(VSID_RSV_LAST - RSV_VSID_LOGGER)
#define VSID_RSV_UPRV			(VSID_RSV_LAST - RSV_VSID_UPRV)

#define VSID_RSV_START			(VSID_RSV_CE)
#define VSID_RSV_END			(VSID_RSV_UPRV + 1)
#define VSID_RSV_NUM			(VSID_RSV_END - VSID_RSV_START)

#define CMU_TOP_BASE			0x19067000
#define UPRV_TCU_BASE			0x19060000
#define EXTM_TCU_BASE			0x19061000

#define inf_printf(fmt, args...)	pr_info("[ammu][inf] " fmt, ##args)
#define dbg_printf(fmt, args...)	pr_info("[ammu][dbg][%s] " fmt, __func__, ##args)
#define err_printf(fmt, args...)	pr_info("[ammu][err][%s] " fmt, __func__, ##args)

enum {
	APUMMU_THD_ID_APMCU_NORMAL = 1,
	APUMMU_THD_ID_TEE = 1,
	APUMMU_THD_ID_MAX
};

static void __iomem *cmut_base;
static void __iomem *uprv_base;
static void __iomem *extm_base;


static int __ioremap(void)
{
	int ret;

	cmut_base = ioremap(CMU_TOP_BASE,  0x7000);
	if (!cmut_base) {
		ret = -ENOMEM;
		goto exit;
	}

	uprv_base = ioremap(UPRV_TCU_BASE, 0x1000);
	if (!uprv_base) {
		ret = -ENOMEM;
		goto ucmu;
	}

	extm_base = ioremap(EXTM_TCU_BASE, 0x1000);
	if (!extm_base) {
		ret = -ENOMEM;
		goto uprv;
	}

	return 0;

uprv:
	iounmap(uprv_base);
ucmu:
	iounmap(cmut_base);
exit:
	return ret;
}

static void __iounmap(void)
{
	iounmap(extm_base);
	iounmap(uprv_base);
	iounmap(cmut_base);
}

static u32 tbl_offset(u32 vsid)
{
#define DESC_SRAM	0x1400

	u32 val;

	/**
	 * 0x1400
	 * VSID desc sram
	 *   vsid table for index 0
	 *   vsid table for index 1
	 *   ...
	 */
	val  = DESC_SRAM;
	val += vsid * TBL_SIZE;

	return val;
}

static u32 seg_offset(u32 vsid, u32 seg_idx)
{
	u32 val;

	/**
	 * 0x0000
	 *   segment 0
	 * 0x0010
	 *   segment 1
	 * ...
	 * 0x0090
	 *   segment 9
	 */
	val  = tbl_offset(vsid) + 0x0000;
	val += seg_idx * 0x10;

	return val;
}

static u32 tbl_address(u32 vsid)
{
#define DESC_SRAM_START	0x0400

	u32 val;

	/**
	 * 0x0400
	 * VSID desc sram
	 *   vsid table for index 0
	 *   vsid table for index 1
	 *   ...
	 */
	val  = DESC_SRAM_START;
	val += vsid * TBL_SIZE;

	return val;
}

static u32 page_offset(u32 vsid, u32 page_sel)
{
	u32 val;

	/**
	 * 0x00A0
	 *   page array 0
	 * 0x00B8
	 *   page array 1
	 * ...
	 */
	val  = tbl_offset(vsid) + 0x00A0;
	val += (page_sel - 3) * 0x18;

	return val;
}

static int sram_config(void)
{
#define DESC_INDEX	0x1000

	u32 i;

	/**
	 * config reserved vsid: START~END
	 */
	for (i = VSID_RSV_START; i < VSID_RSV_END; i++) {
		u32 ofs, val, j;

		/**
		 * 0x1000
		 * VSID desc index
		 *   0x0000 -> VSID   0 descriptor address
		 *   0x0004 -> VSID   1 descriptor address
		 *   ...
		 *   0x03FC -> VSID 255 descriptor address
		 */
		ofs = DESC_INDEX + i * 4;
		val = tbl_address(i);
		iowrite32(val, cmut_base + ofs);

		for (j = 0; j < 10; j++) {
			/**
			 * 0x000C
			 *   [31:31]: segment vld
			 */
			ofs = seg_offset(i, j) + 0x000C;
			val = 0x0;
			iowrite32(val, cmut_base + ofs);
		}

		for (j = 3; j < 8; j++) {
			/**
			 * 0x0014
			 *   [05:00]: Page enable num
			 */
			ofs = page_offset(i, j) + 0x0014;
			val = 0x0;
			iowrite32(val, cmut_base + ofs);
		}
	}

	return 0;
}

static int ammu_enable(void)
{
#define CMU_CON		0x0000

	u32 ofs, val;

	/**
	 * 0x0000
	 * cmu_con
	 *   [00:00]: APU_MMU_enable
	 *   [01:01]: tcu_apb_dbg_en
	 *   [02:02]: tcu_secure_chk_en
	 *   [03:03]: CMU_DCM_en
	 *   [04:04]: sw_slp_prot_en_override
	 */
	ofs  = CMU_CON;
	val  = ioread32(cmut_base + ofs);
	val |= 0x1;
	iowrite32(val, cmut_base + ofs);

	return 0;
}

static int boot_init(void)
{
	int ret;

	ret = sram_config();
	if (ret)
		goto exit;

	ret = ammu_enable();
	if (ret)
		goto exit;
exit:
	return ret;
}

static int add_seg(u32 vsid, u32 seg_idx, u32 in_adr, u32 map_adr,
	u32 page_len, u32 domain, u32 ns)
{
	u32 seg, val;

	// init
	seg = seg_offset(vsid, seg_idx);

	/**
	 * 0x0000
	 *   [02:00]: Page length
	 *   [05:03]: Page select
	 *   [31:10]: Input address [33:12]
	 */
	val  = (page_len & 0x7) <<  0;
	val |= (0x0)            <<  3;
	val |= (in_adr >> 12)   << 10;
	iowrite32(val, cmut_base + seg + 0x0000);

	/**
	 * 0x0004
	 *   [00:00]: axmmusecsid
	 *   [01:01]: IOMMU enable
	 *   [09:02]: smmu_sid
	 *   [31:10]: Mapped address base [33:12]
	 */
	val  = (0x0)           <<  0;
	val |= (0x1)           <<  1;
	val |= (0x0)           <<  2;
	val |= (map_adr >> 12) << 10;
	iowrite32(val, cmut_base + seg + 0x0004);

	/**
	 * 0x0008
	 *   [00:00]: NS
	 *   [16:13]: Domain
	 */
	val  = (ns ? 1 : 0)   <<  0;
	val |= (domain & 0xf) << 13;
	iowrite32(val, cmut_base + seg + 0x0008);

	/**
	 * 0x000C
	 *   [31:31]: segment vld
	 */
	val  = 0x1 << 31;
	iowrite32(val, cmut_base + seg + 0x000C);

	return 0;
}

static int enable_vsid(u32 vsid)
{
#define VSID_ENABLE	0x0050
#define VSID_VALID	0x00B0

	u32 ofs, val;

	/**
	 * 0x0050
	 * VSID_enable0_set
	 *   [31:00]: the_VSID31_0_enable_set_register  W1S
	 * VSID_enable1_set
	 *   [31:00]: the_VSID63_32_enable_set_register W1S
	 * ...
	 */
	ofs = VSID_ENABLE + (vsid >> 5) * 4;
	val = 0x1 << (vsid & 0x1f);
	iowrite32(val, cmut_base + ofs);

	/**
	 * 0x00B0
	 * VSID_valid0_set
	 *   [31:00]: the_VSID31_0_valid_set_register  W1S
	 * VSID_valid1_set
	 *   [31:00]: the_VSID63_31_valid_set_register W1S
	 * ...
	 */
	ofs = VSID_VALID + (vsid >> 5) * 4;
	val = 0x1 << (vsid & 0x1f);
	iowrite32(val, cmut_base + ofs);

	return 0;
}

static int add_rv_map(u32 map_adr)
{
	u32 domain = 7, ns = 1;
	int ret;

	ret = add_seg(VSID_RSV_UPRV, 0, 0, map_adr, eAPUMMU_PAGE_LEN_1MB, domain, ns);
	if (ret)
		goto exit;

	ret = add_seg(VSID_RSV_UPRV, 1, 0, 0, eAPUMMU_PAGE_LEN_512MB, domain, ns);
	if (ret)
		goto exit;

	ret = add_seg(VSID_RSV_UPRV, 2, 0, 0, eAPUMMU_PAGE_LEN_4GB, domain, ns);
	if (ret)
		goto exit;

	/* enable */

	ret = enable_vsid(VSID_RSV_UPRV);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_vsid(void *base, u32 vsid, u32 thread)
{
	u32 ofs, val;

	/**
	 * 0x0000
	 * thread_to_vsid_cid_mapping_table0
	 *   [00:00]: vsid_vld
	 *   [01:01]: corid_vld
	 *   [02:02]: vsid_prefetch_trigger
	 *   [10:03]: vsid
	 *   [17:11]: cor_id
	 * thread_to_vsid_cid_mapping_table1
	 * ...
	 */
	ofs  = thread * 4;
	val  = (0x1)         <<  0;
	val |= (0x0)         <<  1;
	val |= (vsid & 0xff) <<  3;
	val |= (0x0)         << 11;
	iowrite32(val, base + ofs);

	return 0;
}

static int bind_rv_vsid(u32 thread)
{
	int ret;

	if (thread > 7) {
		err_printf("invalid thread: %d\n", thread);
		ret = -EINVAL;
		goto exit;
	}

	ret = bind_vsid(uprv_base, VSID_RSV_UPRV, thread);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int add_logger_map(u32 in_adr, u32 map_adr, enum eAPUMMUPAGESIZE page_size)
{
	u32 domain = 7, ns = 1;
	int ret;

	ret = add_seg(VSID_RSV_LOGGER, 0, in_adr, map_adr, page_size, domain, ns);
	if (ret)
		goto exit;

	ret = enable_vsid(VSID_RSV_LOGGER);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_logger_vsid(u32 thread)
{
	int ret;

	if (thread > 7) {
		err_printf("invalid thread: %d\n", thread);
		ret = -EINVAL;
		goto exit;
	}

	ret = bind_vsid(uprv_base, VSID_RSV_LOGGER, thread);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int virtual_engine_thread(void)
{
#define D2T_MAP_TBL0	0x0040

	u32 ofs, val;

	/**
	 * APMCU (domain, ns) = (0, 1) = 0000 1 = 1
	 * 0x0040
	 * apu_mmu_d2t_mapping_table0
	 *   [02:00]: domain_ns0_thread_info
	 *   [05:03]: domain_ns1_thread_info
	 *   ...
	 *   [29:27]: domain_ns9_thread_info
	 * ...
	 * apu_mmu_d2t_mapping_table3
	 *   [02:00]: domain_ns30_thread_info
	 *   [05:03]: domain_ns31_thread_info
	 */
	ofs = D2T_MAP_TBL0;
	val = APUMMU_THD_ID_APMCU_NORMAL << 3;
	iowrite32(val, extm_base + ofs);

	return 0;
}

static int add_apmcu_map(u32 in_adr, u32 map_adr, enum eAPUMMUPAGESIZE page_size)
{
	u32 domain = 7, ns = 1;
	int ret;

	ret = add_seg(VSID_RSV_APMCU, 0, in_adr, map_adr, page_size, domain, ns);
	if (ret)
		goto exit;

	ret = enable_vsid(VSID_RSV_APMCU);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_apmcu_vsid(u32 thread)
{
	int ret;

	if (thread > 7) {
		err_printf("invalid thread: %d\n", thread);
		ret = -EINVAL;
		goto exit;
	}

	ret = bind_vsid(extm_base, VSID_RSV_APMCU, thread);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int mt6899_rv_boot(u32 uP_seg_output, u8 uP_hw_thread,
	u32 logger_seg_output, enum eAPUMMUPAGESIZE logger_page_size,
	u32 XPU_seg_output, enum eAPUMMUPAGESIZE XPU_page_size)
{
	int ret;

	ret = __ioremap();
	if (ret)
		goto exit;

	ret = boot_init();
	if (ret)
		goto umap;

	ret = add_rv_map(uP_seg_output);
	if (ret)
		goto umap;

	/* thread: 0 for normal */
	ret = bind_rv_vsid(uP_hw_thread);
	if (ret)
		goto umap;

	/* thread: 1 for secure */
	ret = bind_rv_vsid(uP_hw_thread + 1);
	if (ret)
		goto umap;

	ret = add_logger_map(logger_seg_output, logger_seg_output, logger_page_size);
	if (ret)
		goto umap;

	/* thread: 2 for logger */
	ret = bind_logger_vsid(uP_hw_thread + 2);
	if (ret)
		goto umap;

	ret = virtual_engine_thread();
	if (ret)
		goto umap;

	ret = add_apmcu_map(XPU_seg_output, XPU_seg_output, XPU_page_size);
	if (ret)
		goto umap;

	ret = bind_apmcu_vsid(APUMMU_THD_ID_APMCU_NORMAL);
	if (ret)
		goto umap;
umap:
	__iounmap();
exit:
	return ret;
}

const struct mtk_apu_ammudata mt6899_ammudata = {
	.ops = {
		.rv_boot = mt6899_rv_boot,
	},
};

