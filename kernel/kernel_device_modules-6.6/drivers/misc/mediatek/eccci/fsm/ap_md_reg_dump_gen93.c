// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include "ap_md_reg_dump.h"
#include "cldma_reg.h"

#define TAG "mcd"

static int dump_emi_last_bm(struct ccci_modem *md)
{
	u32 buf_len = 1024;
	u32 i, j;
	char temp_char;
	char *buf = NULL;
	char *temp_buf = NULL;

	buf = kzalloc(buf_len, GFP_ATOMIC);
	if (!buf) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"alloc memory failed for emi last bm\n");
		return -1;
	}
#if IS_ENABLED(CONFIG_MTK_EMI_BWL)
	dump_last_bm(buf, buf_len);
#endif
	CCCI_MEM_LOG_TAG(0, TAG, "Dump EMI last bm\n");
	buf[buf_len - 1] = '\0';
	temp_buf = buf;
	for (i = 0, j = 1; i < buf_len - 1; i++, j++) {
		if (buf[i] == 0x0) /* 0x0 end of hole string. */
			break;
		if (buf[i] == 0x0A && j < 256) {
			/* 0x0A stands for end of string, no 0x0D */
			buf[i] = '\0';
			CCCI_MEM_LOG(0, TAG, "%s\n", temp_buf);
			/* max 256 bytes */
			temp_buf = buf + i + 1;
			j = 0;
		} else if (unlikely(j >= 255)) {
			/*
			 * ccci_mem_log max buffer length: 256,
			 * but dm log maybe only less than 50 bytes.
			 */
			temp_char = buf[i];
			buf[i] = '\0';
			CCCI_MEM_LOG(0, TAG, "%s\n", temp_buf);
			temp_buf = buf + i;
			j = 0;
			buf[i] = temp_char;
		}
	}

	kfree(buf);
	return 0;
}

void md_dump_register_for_mt6761(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	struct ccci_per_md *per_md_data = &md->per_md_data;
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };
	void __iomem *dump_reg0;
	unsigned int reg_value1 = 0, reg_value2 = 0;
	u32 boot_status_val = get_expected_boot_status_val();

	CCCI_MEM_LOG_TAG(0, TAG, "%s, 0x%x\n", __func__, boot_status_val);
	/*dump_emi_latency();*/
	dump_emi_last_bm(md);

	if (md->hw_info->plat_ptr->get_md_bootup_status)
		md->hw_info->plat_ptr->get_md_bootup_status(reg_value, 2);

	CCCI_MEM_LOG_TAG(0, TAG, "%s, 0x%x\n", __func__, reg_value[0]);

	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0))
		return;
	else if (!((reg_value[0] == boot_status_val) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF)))
		return;
	if (unlikely(in_interrupt())) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}
	md_cd_lock_modem_clock_src(1);

	/* 1. pre-action */
	if (per_md_data->md_dbg_dump_flag &
		 (MD_DBG_DUMP_ALL & ~(1 << MD_DBG_DUMP_SMEM))) {
		dump_reg0 = ioremap(0x10006000, 0x1000);
		ccci_write32(dump_reg0, 0x430, 0x1);
		udelay(1000);
		CCCI_MEM_LOG_TAG(0, TAG, "md_dbg_sys:0x%X\n",
			cldma_read32(dump_reg0, 0x430));
		iounmap(dump_reg0);
	} else {
		md_cd_lock_modem_clock_src(0);
		return;
	}

	/* 1. PC Monitor */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PCMON)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD PC monitor\n");
		CCCI_MEM_LOG_TAG(0, TAG, "common: 0x%X\n", (0x0D0D9000 + 0x800));
		/* Stop all PCMon */
		dump_reg0 = ioremap(0x0D0D9000, 0x1000);
		ccci_write32(dump_reg0, 0x800, 0x22); /* stop MD PCMon */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x100);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0 + 0x900, 0x60);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xA00), 0x60);
		/* core0 */
		CCCI_MEM_LOG_TAG(0, TAG, "core0/1: [0]0x%X, [1]0x%X\n",
			0x0D0D9000, (0x0D0D9000 + 0x400));
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x400);
		/* core1 */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x400);
		/* Resume PCMon */
		ccci_write32(dump_reg0, 0x800, 0x11);
		ccci_read32(dump_reg0, 0x800);
		iounmap(dump_reg0);
	}

	/* 2. dump PLL */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PLL)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD PLL\n");
		/* MD CLKSW */
		CCCI_MEM_LOG_TAG(0, TAG,
			"CLKSW: [0]0x%X, [1]0x%X, [2]0x%X\n",
			0x0D0D6000, (0x0D0D6000 + 0x100), (0x0D0D6000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D6000, 0xF08);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xD4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x8);
		iounmap(dump_reg0);
		/* MD PLLMIXED */
		CCCI_MEM_LOG_TAG(0, TAG,
			"PLLMIXED:[0]0x%X,[1]0x%X,[2]0x%X,[3]0x%X,[4]0x%X,[5]0x%X,[6]0x%X,[7]0x%X,[8]0x%X,[9]0x%X\n",
			0x0D0D4000,
			(0x0D0D4000 + 0x100),
			(0x0D0D4000 + 0x200),
			(0x0D0D4000 + 0x300),
			(0x0D0D4000 + 0x400),
			(0x0D0D4000 + 0x500),
			(0x0D0D4000 + 0x600),
			(0x0D0D4000 + 0xC00),
			(0x0D0D4000 + 0xD00),
			(0x0D0D4000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D4000, 0xF14);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x68);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x5C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0xD0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x600), 0x10);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x48);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x14);
		iounmap(dump_reg0);
		/* MD CLKCTL */
		CCCI_MEM_LOG_TAG(0, TAG, "CLKCTL: [0]0x%X, [1]0x%X\n",
			0x0D0C3800, (0x0D0C3800 + 0x100));
		dump_reg0 = ioremap(0x0D0C3800, 0x130);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x20);
		iounmap(dump_reg0);
		/* MD GLOBAL CON */
		CCCI_MEM_LOG_TAG(0, TAG,
			"GLOBAL CON: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X, [4]0x%X, [5]0x%X, [6]0x%X, [7]0x%X, [8]0x%X\n",
			0x0D0D5000,
			(0x0D0D5000 + 0x100),
			(0x0D0D5000 + 0x200),
			(0x0D0D5000 + 0x300),
			(0x0D0D5000 + 0x800),
			(0x0D0D5000 + 0x900),
			(0x0D0D5000 + 0xC00),
			(0x0D0D5000 + 0xD00),
			(0x0D0D5000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D5000, 0x1000);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xA0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x10);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x98);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x24);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x900), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 8);
		iounmap(dump_reg0);
	}

	/* 3. Bus status */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUS)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD Bus status: [0]0x%X, [1]0x%X, [2]0x%X\n",
			0x0D0C7000, 0x0D0C9000, 0x0D0E0000);
		dump_reg0 = ioremap(0x0D0C7000, 0xAC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xAC);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0C9000, 0xAC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xAC);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0E0000, 0x6C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x6C);
		iounmap(dump_reg0);
	}

	/* 4. Bus REC */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUSREC)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD Bus REC: [0]0x%X, [1]0x%X\n", 0x0D0C6000, 0x0D0C8000);
		dump_reg0 = ioremap(0x0D0C6000, 0x1000);
		ccci_write32(dump_reg0, 0x10, 0x0); /* stop */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		ccci_write32(dump_reg0, 0x10, 0x1); /* re-start */
		iounmap(dump_reg0);

		dump_reg0 = ioremap(0x0D0C8000, 0x1000);
		ccci_write32(dump_reg0, 0x10, 0x0); /* stop */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		ccci_write32(dump_reg0, 0x10, 0x1);/* re-start */
		iounmap(dump_reg0);
	}

	/* 5. ECT */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_ECT)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD ECT: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			0x0D0CC130, 0x0D0CD130, (0x0D0CE000 + 0x14), (0x0D0CE000 + 0x0C));
		dump_reg0 = ioremap(0x0D0CC130, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0CD130, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0CE000, 0x20);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x14), 0x4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0C), 0x4);
		iounmap(dump_reg0);
	}

	/*avoid deadlock and set bus protect*/
	if (per_md_data->md_dbg_dump_flag & ((1 << MD_DBG_DUMP_TOPSM) |
		(1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST))) {

		/* clear dummy reg flag to access modem reg */
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			&reg_value1);
		reg_value1 &= (~(0x1 << INFRA_AP2MD_DUMMY_BIT));
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			&reg_value1);
		CCCI_MEM_LOG_TAG(0, TAG, "pre: ap2md dummy reg 0x%X: 0x%X\n",
			INFRA_AP2MD_DUMMY_REG, reg_value1);

		/* disable MD to AP */
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_SET,
			(0x1 << INFRA_MD2PERI_PROT_BIT));
		reg_value1 = 0;

		while (reg_value1 != (0x1 << INFRA_MD2PERI_PROT_BIT)) {
			regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_MD2PERI_PROT_RDY, &reg_value1);
			reg_value1 &= (0x1 << INFRA_MD2PERI_PROT_BIT);
		}
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_EN,
			&reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_RDY,
			&reg_value2);
		CCCI_MEM_LOG_TAG(0, TAG, "md2peri: en[0x%X], rdy[0x%X]\n", reg_value1,
			reg_value2);

		/*make sure AP to MD is enabled*/
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_CLR,
			(0x1 << INFRA_PERI2MD_PROT_BIT));
		reg_value1 = 0;
		while (1) {
			regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_PERI2MD_PROT_RDY, &reg_value1);
			if (!(reg_value1 & (0x1 << INFRA_PERI2MD_PROT_BIT)))
				break;
		}
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_EN,
			&reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_RDY,
			&reg_value2);
		CCCI_MEM_LOG_TAG(0, TAG, "peri2md: en[0x%X], rdy[0x%X]\n", reg_value1,
			reg_value2);
	}

	/* 6. TOPSM */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_TOPSM)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD TOPSM status: 0x%X\n", 0x200D0000);
		dump_reg0 = ioremap(0x200D0000, 0x8E4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8E4);
		iounmap(dump_reg0);
	}

	/* 7. MD RGU */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_MDRGU)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD RGU: [0]0x%X, [1]0x%X\n",
			0x200F0100, (0x200F0100 + 0x200));
		dump_reg0 = ioremap(0x200F0100, 0x400);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xCC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x5C);
		iounmap(dump_reg0);
	}

	/* 8 OST */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_OST)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD OST status: [0]0x%X, [1]0x%X\n",
			0x200E0000, (0x200E0000 + 0x200));
		dump_reg0 = ioremap(0x200E0000, 0x300);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xF0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		iounmap(dump_reg0);
		/* 9 CSC */
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD CSC: 0x%X\n", 0x20100000);
		dump_reg0 = ioremap(0x20100000, 0x214);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x214);
		iounmap(dump_reg0);

		/* 10 ELM */
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD ELM: 0x%X\n", 0x20350000);
		dump_reg0 = ioremap(0x20350000, 0x480);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x480);
		iounmap(dump_reg0);
	}

	/* Clear flags for wdt timeout dump MDRGU */
	md->per_md_data.md_dbg_dump_flag &= (~((1 << MD_DBG_DUMP_TOPSM)
		| (1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST)));

	md_cd_lock_modem_clock_src(0);
}

void md_dump_register_for_mt6765(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	struct ccci_per_md *per_md_data = &md->per_md_data;
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };
	void __iomem *dump_reg0;
	unsigned int reg_value1 = 0, reg_value2 = 0;
	u32 boot_status_val = get_expected_boot_status_val();

	CCCI_MEM_LOG_TAG(0, TAG, "%s, 0x%x\n", __func__, boot_status_val);
	/*dump_emi_latency();*/
	dump_emi_last_bm(md);

	if (md->hw_info->plat_ptr->get_md_bootup_status)
		md->hw_info->plat_ptr->get_md_bootup_status(reg_value, 2);

	CCCI_MEM_LOG_TAG(0, TAG, "%s, 0x%x\n", __func__, reg_value[0]);

	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0))
		return;
	else if (!((reg_value[0] == boot_status_val) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF)))
		return;
	if (unlikely(in_interrupt())) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}
	md_cd_lock_modem_clock_src(1);

	/* 1. pre-action */
	if (per_md_data->md_dbg_dump_flag &
		 (MD_DBG_DUMP_ALL & ~(1 << MD_DBG_DUMP_SMEM))) {
		dump_reg0 = ioremap(0x10006000, 0x1000);
		ccci_write32(dump_reg0, 0x430, 0x1);
		udelay(1000);
		CCCI_MEM_LOG_TAG(0, TAG, "md_dbg_sys:0x%X\n",
			cldma_read32(dump_reg0, 0x430));
		iounmap(dump_reg0);
	} else {
		md_cd_lock_modem_clock_src(0);
		return;
	}

	/* 1. PC Monitor */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PCMON)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD PC monitor\n");
		CCCI_MEM_LOG_TAG(0, TAG, "common: 0x%X\n", (0x0D0D9000 + 0x800));
		/* Stop all PCMon */
		dump_reg0 = ioremap(0x0D0D9000, 0x1000);
		ccci_write32(dump_reg0, 0x800, 0x22); /* stop MD PCMon */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x100);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0 + 0x900, 0x60);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xA00), 0x60);
		/* core0 */
		CCCI_MEM_LOG_TAG(0, TAG, "core0/1: [0]0x%X, [1]0x%X\n",
			0x0D0D9000, (0x0D0D9000 + 0x400));
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x400);
		/* core1 */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x400);
		/* Resume PCMon */
		ccci_write32(dump_reg0, 0x800, 0x11);
		ccci_read32(dump_reg0, 0x800);
		iounmap(dump_reg0);
	}

	/* 2. dump PLL */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PLL)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD PLL\n");
		/* MD CLKSW */
		CCCI_MEM_LOG_TAG(0, TAG,
			"CLKSW: [0]0x%X, [1]0x%X, [2]0x%X\n",
			0x0D0D6000, (0x0D0D6000 + 0x100), (0x0D0D6000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D6000, 0xF08);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xD4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x8);
		iounmap(dump_reg0);
		/* MD PLLMIXED */
		CCCI_MEM_LOG_TAG(0, TAG,
			"PLLMIXED:[0]0x%X,[1]0x%X,[2]0x%X,[3]0x%X,[4]0x%X,[5]0x%X,[6]0x%X,[7]0x%X,[8]0x%X,[9]0x%X\n",
			0x0D0D4000,
			(0x0D0D4000 + 0x100),
			(0x0D0D4000 + 0x200),
			(0x0D0D4000 + 0x300),
			(0x0D0D4000 + 0x400),
			(0x0D0D4000 + 0x500),
			(0x0D0D4000 + 0x600),
			(0x0D0D4000 + 0xC00),
			(0x0D0D4000 + 0xD00),
			(0x0D0D4000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D4000, 0xF14);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x68);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x5C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0xD0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x600), 0x10);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x48);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x14);
		iounmap(dump_reg0);
		/* MD CLKCTL */
		CCCI_MEM_LOG_TAG(0, TAG, "CLKCTL: [0]0x%X, [1]0x%X\n",
			0x0D0C3800, (0x0D0C3800 + 0x100));
		dump_reg0 = ioremap(0x0D0C3800, 0x130);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x20);
		iounmap(dump_reg0);
		/* MD GLOBAL CON */
		CCCI_MEM_LOG_TAG(0, TAG,
			"GLOBAL CON: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X, [4]0x%X, [5]0x%X, [6]0x%X, [7]0x%X, [8]0x%X\n",
			0x0D0D5000,
			(0x0D0D5000 + 0x100),
			(0x0D0D5000 + 0x200),
			(0x0D0D5000 + 0x300),
			(0x0D0D5000 + 0x800),
			(0x0D0D5000 + 0x900),
			(0x0D0D5000 + 0xC00),
			(0x0D0D5000 + 0xD00),
			(0x0D0D5000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D5000, 0x1000);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xA0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x10);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x98);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x24);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x900), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 8);
		iounmap(dump_reg0);
	}

	/* 3. Bus status */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUS)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD Bus status: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			0x0D0C2000, 0x0D0C7000, 0x0D0C9000, 0x0D0E0000);
		dump_reg0 = ioremap(0x0D0C2000, 0x100);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x100);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0C7000, 0xAC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xAC);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0C9000, 0xAC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xAC);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0E0000, 0x6C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x6C);
		iounmap(dump_reg0);
	}

	/* 4. Bus REC */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUSREC)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD Bus REC: [0]0x%X, [1]0x%X, [2]0x%X\n", 0x0D0C6000, 0x0D0C8000, 0x0D0C2500);
		dump_reg0 = ioremap(0x0D0C6000, 0x1000);
		ccci_write32(dump_reg0, 0x10, 0x0); /* stop */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		ccci_write32(dump_reg0, 0x10, 0x1); /* re-start */
		iounmap(dump_reg0);

		dump_reg0 = ioremap(0x0D0C8000, 0x1000);
		ccci_write32(dump_reg0, 0x10, 0x0); /* stop */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		ccci_write32(dump_reg0, 0x10, 0x1);/* re-start */
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0C2500, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);

	}

	/* 5. ECT */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_ECT)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD ECT: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			0x0D0CC130, 0x0D0CD130, (0x0D0CE000 + 0x14), (0x0D0CE000 + 0x0C));
		dump_reg0 = ioremap(0x0D0CC130, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0CD130, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0CE000, 0x20);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x14), 0x4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0C), 0x4);
		iounmap(dump_reg0);
	}

	/*avoid deadlock and set bus protect*/
	if (per_md_data->md_dbg_dump_flag & ((1 << MD_DBG_DUMP_TOPSM) |
		(1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST))) {

		/* clear dummy reg flag to access modem reg */
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			&reg_value1);
		reg_value1 &= (~(0x1 << INFRA_AP2MD_DUMMY_BIT));
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			&reg_value1);
		CCCI_MEM_LOG_TAG(0, TAG, "pre: ap2md dummy reg 0x%X: 0x%X\n",
			INFRA_AP2MD_DUMMY_REG, reg_value1);

		/* disable MD to AP */
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_SET,
			(0x1 << INFRA_MD2PERI_PROT_BIT));
		reg_value1 = 0;

		while (reg_value1 != (0x1 << INFRA_MD2PERI_PROT_BIT)) {
			regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_MD2PERI_PROT_RDY, &reg_value1);
			reg_value1 &= (0x1 << INFRA_MD2PERI_PROT_BIT);
		}
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_EN,
			&reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_RDY,
			&reg_value2);
		CCCI_MEM_LOG_TAG(0, TAG, "md2peri: en[0x%X], rdy[0x%X]\n", reg_value1,
			reg_value2);

		/*make sure AP to MD is enabled*/
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_CLR,
			(0x1 << INFRA_PERI2MD_PROT_BIT));
		reg_value1 = 0;
		while (1) {
			regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_PERI2MD_PROT_RDY, &reg_value1);
			if (!(reg_value1 & (0x1 << INFRA_PERI2MD_PROT_BIT)))
				break;
		}
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_EN,
			&reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_RDY,
			&reg_value2);
		CCCI_MEM_LOG_TAG(0, TAG, "peri2md: en[0x%X], rdy[0x%X]\n", reg_value1,
			reg_value2);
	}

	/* 6. TOPSM */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_TOPSM)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD TOPSM status: 0x%X\n", 0x200D0000);
		dump_reg0 = ioremap(0x200D0000, 0x8E4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8E4);
		iounmap(dump_reg0);
	}

	/* 7. MD RGU */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_MDRGU)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD RGU: [0]0x%X, [1]0x%X\n",
			0x200F0100, (0x200F0100 + 0x200));
		dump_reg0 = ioremap(0x200F0100, 0x400);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xCC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x5C);
		iounmap(dump_reg0);
	}

	/* 8 OST */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_OST)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD OST status: [0]0x%X, [1]0x%X\n",
			0x200E0000, (0x200E0000 + 0x200));
		dump_reg0 = ioremap(0x200E0000, 0x300);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xF0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		iounmap(dump_reg0);
		/* 9 CSC */
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD CSC: 0x%X\n", 0x20100000);
		dump_reg0 = ioremap(0x20100000, 0x214);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x214);
		iounmap(dump_reg0);

		/* 10 ELM */
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD ELM: 0x%X\n", 0x20350000);
		dump_reg0 = ioremap(0x20350000, 0x480);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x480);
		iounmap(dump_reg0);
	}

	/* Clear flags for wdt timeout dump MDRGU */
	md->per_md_data.md_dbg_dump_flag &= (~((1 << MD_DBG_DUMP_TOPSM)
		| (1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST)));

	md_cd_lock_modem_clock_src(0);
}

void md_dump_register_for_mt6768(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	struct ccci_per_md *per_md_data = &md->per_md_data;
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };
	void __iomem *dump_reg0;
	unsigned int reg_value1 = 0, reg_value2 = 0;
	u32 boot_status_val = get_expected_boot_status_val();

	CCCI_MEM_LOG_TAG(0, TAG, "%s, 0x%x\n", __func__, boot_status_val);
	/*dump_emi_latency();*/
	dump_emi_last_bm(md);

	if (md->hw_info->plat_ptr->get_md_bootup_status)
		md->hw_info->plat_ptr->get_md_bootup_status(reg_value, 2);

	CCCI_MEM_LOG_TAG(0, TAG, "%s, 0x%x\n", __func__, reg_value[0]);

	//md_cd_get_md_bootup_status(reg_value, 2);
	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0))
		return;
	else if (!((reg_value[0] == boot_status_val) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF)))
		return;
	if (unlikely(in_interrupt())) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}

	md_cd_lock_modem_clock_src(1);

	/* 1. pre-action */
	if (per_md_data->md_dbg_dump_flag &
		(MD_DBG_DUMP_ALL & ~(1 << MD_DBG_DUMP_SMEM))) {
		dump_reg0 = ioremap(0x10006000, 0x1000);
		ccci_write32(dump_reg0, 0x430, 0x1);
		udelay(1000);
		CCCI_MEM_LOG_TAG(0, TAG, "md_dbg_sys:0x%X\n",
			cldma_read32(dump_reg0, 0x430));
		iounmap(dump_reg0);
	} else {
		md_cd_lock_modem_clock_src(0);
		return;
	}

	/* 1. PC Monitor */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PCMON)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD PC monitor\n");
		CCCI_MEM_LOG_TAG(0, TAG, "common: 0x%X\n", (0x0D0D9000 + 0x800));
		/* Stop all PCMon */
		dump_reg0 = ioremap(0x0D0D9000, 0x1000);
		mdreg_write32(MD_REG_PC_MONITOR, 0x22); /* stop MD PCMon */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x100);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x900), 0x60);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xA00), 0x60);
		/* core0 */
		CCCI_MEM_LOG_TAG(0, TAG, "core0/1: [0]0x%X, [1]0x%X\n",
			0x0D0D9000, (0x0D0D9000 + 0x400));
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x400);
		/* core1 */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x400);
		/* Resume PCMon */
		mdreg_write32(MD_REG_PC_MONITOR, 0x11);
		ccci_read32(dump_reg0, 0x800);
		iounmap(dump_reg0);
	}

	/* 2. dump PLL */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PLL)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD PLL\n");
		/* MD CLKSW */
		CCCI_MEM_LOG_TAG(0, TAG,
		"CLKSW: [0]0x%X, [1]0x%X, [2]0x%X\n",
			0x0D0D6000, (0x0D0D6000 + 0x100), (0x0D0D6000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D6000, 0xF08);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xD4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x8);
		iounmap(dump_reg0);
		/* MD PLLMIXED */
		CCCI_MEM_LOG_TAG(0, TAG,
			"PLLMIXED:[0]0x%X,[1]0x%X,[2]0x%X,[3]0x%X,[4]0x%X,[5]0x%X,[6]0x%X,[7]0x%X,[8]0x%X,[9]0x%X\n",
			0x0D0D4000, (0x0D0D4000 + 0x100),
			(0x0D0D4000 + 0x200), (0x0D0D4000 + 0x300), (0x0D0D4000 + 0x400),
			(0x0D0D4000 + 0x500), (0x0D0D4000 + 0x600), (0x0D0D4000 + 0xC00),
			(0x0D0D4000 + 0xD00), (0x0D0D4000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D4000, 0xF14);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x68);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x5C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0xD0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x600), 0x10);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x48);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x14);
		iounmap(dump_reg0);
		/* MD CLKCTL */
		CCCI_MEM_LOG_TAG(0, TAG, "CLKCTL: [0]0x%X, [1]0x%X\n",
			0x0D0C3800, (0x0D0C3800 + 0x110));
		dump_reg0 = ioremap(0x0D0C3800, 0x130);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x110), 0x20);
		iounmap(dump_reg0);
		/* MD GLOBAL CON */
		CCCI_MEM_LOG_TAG(0, TAG,
			"GLOBAL CON: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X, [4]0x%X, [5]0x%X, [6]0x%X, [7]0x%X, [8]0x%X\n",
			0x0D0D5000, (0x0D0D5000 + 0x100), (0x0D0D5000 + 0x200),
			(0x0D0D5000 + 0x300), (0x0D0D5000 + 0x800),
			(0x0D0D5000 + 0x900), (0x0D0D5000 + 0xC00),
			(0x0D0D5000 + 0xD00), (0x0D0D5000 + 0xF00));
		dump_reg0 = ioremap(0x0D0D5000, 0x1000);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xA0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x10);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x98);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x24);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x900), 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x1C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 8);
		iounmap(dump_reg0);
	}

	/* 3. Bus status */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUS)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD Bus status: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			0x0D0C2000, 0x0D0C7000, 0x0D0C9000, 0x0D0E0000);
		dump_reg0 = ioremap(0x0D0C2000, 0x100);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x100);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0C7000, 0xAC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xAC);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0C9000, 0xAC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xAC);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0E0000, 0x6C);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x6C);
		iounmap(dump_reg0);
	}

	/* 4. Bus REC */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUSREC)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD Bus REC: [0]0x%X, [1]0x%X\n", 0x0D0C6000, 0x0D0C8000);
		dump_reg0 = ioremap(0x0D0C6000, 0x1000);
		mdreg_write32(MD_REG_MDMCU_BUSMON, 0x0); /* stop */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		mdreg_write32(MD_REG_MDMCU_BUSMON, 0x1); /* re-start */
		iounmap(dump_reg0);

		dump_reg0 = ioremap(0x0D0C8000, 0x1000);
		mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x0); /* stop */
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x1);/* re-start */
		iounmap(dump_reg0);
	}

	/* 5. ECT */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_ECT)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD ECT: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			0x0D0CC130, 0x0D0CD130, (0x0D0CE000 + 0x14), (0x0D0CE000 + 0x0C));
		dump_reg0 = ioremap(0x0D0CC130, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0CD130, 0x8);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
		dump_reg0 = ioremap(0x0D0CE000, 0x20);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x14), 0x4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0C), 0x4);
		iounmap(dump_reg0);
	}

	/*avoid deadlock and set bus protect*/
	if (per_md_data->md_dbg_dump_flag & ((1 << MD_DBG_DUMP_TOPSM) |
		(1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST))) {

		/* clear dummy reg flag to access modem reg */
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			&reg_value1);
		reg_value1 &= (~(0x1 << INFRA_AP2MD_DUMMY_BIT));
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			&reg_value1);
		CCCI_MEM_LOG_TAG(0, TAG, "pre: ap2md dummy reg 0x%X: 0x%X\n",
			INFRA_AP2MD_DUMMY_REG, reg_value1);

		/* disable MD to AP */
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_SET,
			(0x1 << INFRA_MD2PERI_PROT_BIT));
		reg_value1 = 0;

		while (reg_value1 != (0x1 << INFRA_MD2PERI_PROT_BIT)) {
			regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_MD2PERI_PROT_RDY, &reg_value1);
			reg_value1 &= (0x1 << INFRA_MD2PERI_PROT_BIT);
		}
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_EN,
			&reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_RDY,
			&reg_value2);
		CCCI_MEM_LOG_TAG(0, TAG, "md2peri: en[0x%X], rdy[0x%X]\n", reg_value1,
			reg_value2);

		/*make sure AP to MD is enabled*/
		regmap_write(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_CLR,
			(0x1 << INFRA_PERI2MD_PROT_BIT));
		reg_value1 = 0;
		while (1) {
			regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_PERI2MD_PROT_RDY, &reg_value1);
			if (!(reg_value1 & (0x1 << INFRA_PERI2MD_PROT_BIT)))
				break;
		}

		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_EN,
			&reg_value1);
		regmap_read(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_RDY,
			&reg_value2);
		CCCI_MEM_LOG_TAG(0, TAG, "peri2md: en[0x%X], rdy[0x%X]\n", reg_value1,
			reg_value2);
	}

	/* 6. TOPSM */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_TOPSM)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD TOPSM status: 0x%X\n", 0x200D0000);
		dump_reg0 = ioremap(0x200D0000, 0x8E4);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8E4);
		iounmap(dump_reg0);
	}

	/* 7. MD RGU */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_MDRGU)) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD RGU: [0]0x%X, [1]0x%X\n",
			0x200F0100, (0x200F0100 + 0x200));
		dump_reg0 = ioremap(0x200F0100, 0x400);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xCC);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x5C);
		iounmap(dump_reg0);
	}

	/* 8 OST */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_OST)) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump MD OST status: [0]0x%X, [1]0x%X\n", 0x200E0000, (0x200E0000 + 0x200));
		dump_reg0 = ioremap(0x200E0000, 0x300);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0xF0);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		iounmap(dump_reg0);

		/* 9 CSC */
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD CSC: 0x%X\n", 0x20100000);
		dump_reg0 = ioremap(0x20100000, 0x214);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x214);
		iounmap(dump_reg0);

		/* 10 ELM */
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD ELM: 0x%X\n", 0x20350000);
		dump_reg0 = ioremap(0x20350000, 0x480);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dump_reg0, 0x480);
		iounmap(dump_reg0);
	}

	/* Clear flags for wdt timeout dump MDRGU */
	md->per_md_data.md_dbg_dump_flag &= (~((1 << MD_DBG_DUMP_TOPSM)
		| (1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST)));

	md_cd_lock_modem_clock_src(0);
}
