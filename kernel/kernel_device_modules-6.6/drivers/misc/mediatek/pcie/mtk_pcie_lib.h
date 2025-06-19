/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_PCIE_LIB_H__
#define __MTK_PCIE_LIB_H__

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/slab.h>

#define PCIE_PORT_MAX		4
#define BUF_SIZE		50
#define MAX_BUF_SZ		2000

struct mtk_pcie_info {
	char *name;
	void __iomem **regs;
	void __iomem **mac_regs;
	struct phy *phys[PCIE_PORT_MAX];
	struct clk_bulk_data *clks[PCIE_PORT_MAX];
	struct reset_control *phy_reset[PCIE_PORT_MAX];
	struct reset_control *mac_reset[PCIE_PORT_MAX];
	int num_clks[PCIE_PORT_MAX];
	unsigned int *test_lane;
	unsigned int *max_lane;
	unsigned int *max_speed;
	unsigned int max_port;
	unsigned int eye[PCIE_LM_EYE_AXIS];
	char response[MAX_BUF_SZ];

	/* char device */
	struct cdev smt_cdev;
	struct class *f_class;
	struct kobject *pcie_test_kobj;
};

unsigned int mtk_get_value(char *str);
int mtk_pcie_lane_margin_entry(struct mtk_pcie_info *pcie_smt, int port, int mode);

#endif /*__MTK_PCIE_LIB_H__*/
