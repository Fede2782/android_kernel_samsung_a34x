// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_irq.h>
/* mmup mbox/ipi related */
#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#include "vcp_helper.h"
#include "mmup_ipi_table.h"
#include "vcp_feature_define.h"
#include "vcp_excep.h"
#include "vcp_status.h"
#include "vcp.h"

uint32_t msg_vcp_ready1;

void mmup_enable_irqs(void)
{
	int i = 0;

	for (i = 0; i < mmup_mboxdev.count; i++)
		enable_irq(mmup_mboxdev.info_table[i].irq_num);

	pr_debug("[MMUP] MMUP IRQ enabled\n");
}

void mmup_disable_irqs(void)
{
	int i = 0;

	for (i = 0; i < mmup_mboxdev.count; i++)
		disable_irq(mmup_mboxdev.info_table[i].irq_num);

	pr_debug("[MMUP] MMUP IRQ disabled\n");
}

void dump_mmup_irq_status(void)
{
	int i;

	for (i = 0; i < mmup_mboxdev.count; i++) {
		pr_info("[MMUP] %s Dump mbox%d irq %d status\n", __func__,
			i, mmup_mboxdev.info_table[i].irq_num);
		mt_irq_dump_status(mmup_mboxdev.info_table[i].irq_num);
	}

}

struct mtk_ipi_device *mmup_get_ipidev(void)
{
	return &mmup_ipidev;
}

void mmup_dump_last_regs(void)
{
	c1_m->status = readl(R_CORE1_STATUS);
	c1_m->pc = readl(R_CORE1_MON_PC);
	c1_m->lr = readl(R_CORE1_MON_LR);
	c1_m->sp = readl(R_CORE1_MON_SP);
	c1_m->pc_latch = readl(R_CORE1_MON_PC_LATCH);
	c1_m->lr_latch = readl(R_CORE1_MON_LR_LATCH);
	c1_m->sp_latch = readl(R_CORE1_MON_SP_LATCH);


	if (vcpreg.twohart_core1) {
		c1_t1_m->pc = readl(R_CORE1_T1_MON_PC);
		c1_t1_m->lr = readl(R_CORE1_T1_MON_LR);
		c1_t1_m->sp = readl(R_CORE1_T1_MON_SP);
		c1_t1_m->pc_latch = readl(R_CORE1_T1_MON_PC_LATCH);
		c1_t1_m->lr_latch = readl(R_CORE1_T1_MON_LR_LATCH);
		c1_t1_m->sp_latch = readl(R_CORE1_T1_MON_SP_LATCH);
	}

	pr_notice("[MMUP] c1_status = %08x\n", c1_m->status);
	pr_notice("[MMUP] c1_pc = %08x\n", c1_m->pc);
	pr_notice("[MMUP] c1_pc2 = %08x\n", readl(R_CORE1_MON_PC));
	pr_notice("[MMUP] c1_lr = %08x\n", c1_m->lr);
	pr_notice("[MMUP] c1_sp = %08x\n", c1_m->sp);
	pr_notice("[MMUP] c1_pc_latch = %08x\n", c1_m->pc_latch);
	pr_notice("[MMUP] c1_lr_latch = %08x\n", c1_m->lr_latch);
	pr_notice("[MMUP] c1_sp_latch = %08x\n", c1_m->sp_latch);

	if (vcpreg.twohart_core1) {
		pr_notice("[MMUP] c1_t1_pc = %08x\n", c1_t1_m->pc);
		pr_notice("[MMUP] c1_t1_pc2 = %08x\n", readl(R_CORE1_T1_MON_PC));
		pr_notice("[MMUP] c1_t1_lr = %08x\n", c1_t1_m->lr);
		pr_notice("[MMUP] c1_t1_sp = %08x\n", c1_t1_m->sp);
		pr_notice("[MMUP] c1_t1_pc_latch = %08x\n", c1_t1_m->pc_latch);
		pr_notice("[MMUP] c1_t1_lr_latch = %08x\n", c1_t1_m->lr_latch);
		pr_notice("[MMUP] c1_t1_sp_latch = %08x\n", c1_t1_m->sp_latch);
	}
	pr_notice("[MMUP] RSTN_CLR = %08x RSTN_CLR = %08x\n",
		readl(R_CORE1_SW_RSTN_CLR), readl(R_CORE1_SW_RSTN_SET));

	pr_notice("[MMUP] irq sta: %08x,%08x,%08x,%08x\n", readl(R_CORE1_IRQ_STA0),
		readl(R_CORE1_IRQ_STA1), readl(R_CORE1_IRQ_STA2), readl(R_CORE1_IRQ_STA3));

	pr_notice("[MMUP] irq en: %08x,%08x,%08x,%08x\n", readl(R_CORE1_IRQ_EN0),
		readl(R_CORE1_IRQ_EN1), readl(R_CORE1_IRQ_EN2), readl(R_CORE1_IRQ_EN3));

	pr_notice("[MMUP] irq wakeup en: %08x,%08x,%08x,%08x\n", readl(R_CORE1_IRQ_SLP0),
		readl(R_CORE1_IRQ_SLP1), readl(R_CORE1_IRQ_SLP2), readl(R_CORE1_IRQ_SLP3));

	pr_notice("[MMUP] core GPR: %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
		readl(R_CORE1_GENERAL_REG0), readl(R_CORE1_GENERAL_REG1),
		readl(R_CORE1_GENERAL_REG2), readl(R_CORE1_GENERAL_REG3),
		readl(R_CORE1_GENERAL_REG4), readl(R_CORE1_GENERAL_REG5),
		readl(R_CORE1_GENERAL_REG6), readl(R_CORE1_GENERAL_REG7));
}

void mmup_do_tbufdump_RV33(void)
{
	uint32_t tmp, index, offset, wbuf_ptr;
	int i;

	wbuf_ptr = readl(R_CORE1_TBUF_WPTR);
	tmp = readl(R_CORE1_DBG_CTRL) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		writel(tmp | (index << S_CORE_TBUF_DBG_SEL), R_CORE1_DBG_CTRL);
		pr_notice("[MMUP] C0:%02d:0x%08x::0x%08x\n",
			i, readl(R_CORE1_TBUF_DATA31_0 + offset), readl(R_CORE1_TBUF_DATA63_32 + offset));
	}
}

static bool mmup_ipi_table_init(struct mtk_mbox_device *mmup_mboxdev, struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i = 0, ret = 0, mbox_id = 0, recv_opt = 0;

	of_property_read_u32(pdev->dev.of_node, "mbox-count", &mmup_mboxdev->count);
	if (!mmup_mboxdev->count) {
		pr_notice("[MMUP] mbox count not found\n");
		return false;
	}

	mmup_mboxdev->send_count =
		of_property_count_u32_elems(pdev->dev.of_node, "send-table") / send_item_num;
	if (mmup_mboxdev->send_count <= 0) {
		pr_notice("[MMUP] mmup send table not found\n");
		return false;
	}

	mmup_mboxdev->recv_count =
		of_property_count_u32_elems(pdev->dev.of_node, "recv-table") / recv_item_num;
	if (mmup_mboxdev->recv_count <= 0) {
		pr_notice("[MMUP] mmup recv table not found\n");
		return false;
	}
	/* alloc and init mmup_mbox_info */
	mmup_mboxdev->info_table = vzalloc(sizeof(struct mtk_mbox_info) * mmup_mboxdev->count);
	if (!mmup_mboxdev->info_table)
		return false;
	mmup_mbox_info = mmup_mboxdev->info_table;
	for (i = 0; i < mmup_mboxdev->count; ++i) {
		mmup_mbox_info[i].id = i;
		mmup_mbox_info[i].slot = 64;
		mmup_mbox_info[i].enable = 1;
		mmup_mbox_info[i].is64d = 1;
	}

	/* alloc and init send table */
	mmup_mboxdev->pin_send_table =
		vzalloc(sizeof(struct mtk_mbox_pin_send) * mmup_mboxdev->send_count);
	if (!mmup_mboxdev->pin_send_table) {
		pr_notice("[MMUP] pin_send_table alloc fail\n");
		return false;
	}
	mmup_mbox_pin_send = mmup_mboxdev->pin_send_table;
	for (i = 0; i < mmup_mboxdev->send_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num,
				&mmup_mbox_pin_send[i].chan_id);
		if (ret) {
			pr_notice("[MMUP]%s:Cannot get ipi id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[MMUP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mmup_mbox_pin_send[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num + 2,
				&mmup_mbox_pin_send[i].msg_size);
		if (ret) {
			pr_notice("[MMUP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
	}

	/* alloc and init recv table */
	mmup_mboxdev->pin_recv_table =
		vzalloc(sizeof(struct mtk_mbox_pin_recv) * mmup_mboxdev->recv_count);
	if (!mmup_mboxdev->pin_recv_table) {
		pr_notice("[MMUP] pin_recv_table alloc fail\n");
		return false;
	}
	mmup_mbox_pin_recv = mmup_mboxdev->pin_recv_table;
	for (i = 0; i < mmup_mboxdev->recv_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num,
				&mmup_mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_notice("[MMUP]%s:Cannot get ipi id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[MMUP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mmup_mbox_pin_recv[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 2,
				&mmup_mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_notice("[MMUP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 3,
				&recv_opt);
		if (ret) {
			pr_notice("[MMUP]%s:Cannot get recv opt (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mmup_mbox_pin_recv[i].recv_opt = recv_opt;
	}

	return true;
}

static void mbox_setup_pin_table(unsigned int mbox)
{
	int i, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;

	for (i = 0; i < mmup_mboxdev.send_count; i++) {
		if (mbox == mmup_mbox_pin_send[i].mbox) {
			mmup_mbox_pin_send[i].offset = last_ofs + last_slot;
			mmup_mbox_pin_send[i].pin_index = last_idx + last_sz;
			last_idx = mmup_mbox_pin_send[i].pin_index;
			if (mmup_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					mmup_mbox_pin_send[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = mmup_mbox_pin_send[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < mmup_mbox_pin_send[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < mmup_mboxdev.recv_count; i++) {
		if (mbox == mmup_mbox_pin_recv[i].mbox) {
			mmup_mbox_pin_recv[i].offset = last_ofs + last_slot;
			mmup_mbox_pin_recv[i].pin_index = last_idx + last_sz;
			last_idx = mmup_mbox_pin_recv[i].pin_index;
			if (mmup_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					mmup_mbox_pin_recv[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = mmup_mbox_pin_recv[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < mmup_mbox_pin_recv[i].mbox)
			break; /* no need to search the rest ipi */
	}


	if (last_idx > 32 ||
	   (last_ofs + last_slot) > (mmup_mbox_info[mbox].is64d + 1) * 32) {
		pr_notice("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

static int mmup_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;

	pr_debug("[MMUP] %s", __func__);

	of_property_read_u32(pdev->dev.of_node, "twohart"
						, &vcpreg.twohart_core1);
	pr_notice("[MMUP] vcpreg.twohart_core1 = %d\n", vcpreg.twohart_core1);

	/* probe mbox info from dts */
	if (!mmup_ipi_table_init(&mmup_mboxdev, pdev))
		return -ENODEV;

	/* create mbox dev */
	pr_debug("[MMUP] mbox probe\n");
	for (i = 0; i < mmup_mboxdev.count; i++) {
		mmup_mbox_info[i].mbdev = &mmup_mboxdev;
		ret = mtk_mbox_probe(pdev, mmup_mbox_info[i].mbdev, i);
		if (ret < 0 || mmup_mboxdev.info_table[i].irq_num < 0) {
			pr_notice("[MMUP] mbox%d probe fail %d\n", i, ret);
			continue;
		}

		ret = enable_irq_wake(mmup_mboxdev.info_table[i].irq_num);
		if (ret < 0) {
			pr_notice("[MMUP]mbox%d enable irq fail %d\n", i, ret);
			continue;
		}
		mbox_setup_pin_table(i);
	}

	ret = mtk_ipi_device_register(&mmup_ipidev, pdev, &mmup_mboxdev, VCP_IPI_COUNT);
	if (ret)
		pr_notice("[MMUP] ipi_dev_register fail, ret %d\n", ret);

	pr_info("[MMUP] %s done\n", __func__);

	return ret;
}

static int mmup_device_remove(struct platform_device *pdev)
{
	kfree(mmup_mbox_info);
	mmup_mbox_info = NULL;
	kfree(mmup_mbox_pin_recv);
	mmup_mbox_pin_recv = NULL;
	kfree(mmup_mbox_pin_send);
	mmup_mbox_pin_send = NULL;

	return 0;
}

static const struct of_device_id mmup_of_ids[] = {
	{ .compatible = "mediatek,mmup", },
	{}
};

static struct platform_driver mtk_mmup_device = {
	.probe = mmup_device_probe,
	.remove = mmup_device_remove,
	.driver = {
		.name = "mmup",
		.owner = THIS_MODULE,
		.of_match_table = mmup_of_ids,
	},
};

/*
 * driver initialization entry point
 */
int mmup_init(void)
{
	/* mmup platform initialise */
	pr_info("[MMUP] %s v2 begins\n", __func__);

	if (platform_driver_register(&mtk_mmup_device)) {
		pr_info("[MMUP] mmup probe fail\n");
		goto err_mmup;
	}

	mtk_ipi_register(&mmup_ipidev, IPI_IN_VCP_READY_1,
		(void *)vcp_A_ready_ipi_handler, NULL, &msg_vcp_ready1);

	return 0;
err_mmup:
	return -1;
}

/*
 * driver exit point
 */
void mmup_exit(void)
{
	platform_driver_unregister(&mtk_mmup_device);
}

