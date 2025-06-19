/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_PCIE_MEDIATEK_GEN3_H__
#define __MTK_PCIE_MEDIATEK_GEN3_H__

#include <linux/pci.h>

enum pcie_port_number {
	PCIE_PORT_NUM_0,
	PCIE_PORT_NUM_1
};

enum pin_state {
	PCIE_PINMUX_INIT,
	PCIE_PINMUX_DEFAULT,
	PCIE_PINMUX_SLEEP,
	PCIE_PINMUX_PD,
	PCIE_PINMUX_HIZ
};

enum cplto_range {
	R0_50MS = 0,
	R1_50US_100US,
	R2_1MS_10MS,
	R5_16MS_55MS
};

enum hs_feature_id {
	PCIE_RPM_CTRL = 1
};

enum mtk_pcie_suspend_link_state {
	LINK_STATE_L2 = 0,
	LINK_STATE_PCIPM_L12,
	LINK_STATE_ASPM_L12
};

/*
 * struct handshake_info - PCIe RC handshake protocol with EP
 */
struct handshake_info {
	int feature_id;
	int data_size;
	u8 data[64];
};

bool mtk_pcie_in_use(int port);
int mtk_pcie_ep_set_info(int port, struct handshake_info *params);
int mtk_pcie_probe_port(int port);
int mtk_pcie_remove_port(int port);
int mtk_pcie_disable_refclk(int port);
int mtk_pcie_soft_off(struct pci_bus *bus);
int mtk_pcie_soft_on(struct pci_bus *bus);
int mtk_msi_unmask_to_other_mcu(struct irq_data *data, u32 group);
u32 mtk_pcie_dump_link_info(int port);
int mtk_pcie_disable_data_trans(int port);
int mtk_pcie_hw_control_vote(int port, bool hw_mode_en, u8 who);
int mtk_pcie_pinmux_select(int port_num, enum pin_state state);
int mtk_pcie_enable_cfg_dump(int port);
int mtk_pcie_disable_cfg_dump(int port);

#endif
