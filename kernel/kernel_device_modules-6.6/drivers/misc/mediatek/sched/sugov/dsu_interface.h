/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef _DSU_INTERFACE_H
#define _DSU_INTERFACE_H

#define PELT_SUM_OFFSET 0x60
#define PELT_WET_OFFSET 0x64
#define DSU_DVFS_VOTE_EAS_1 0x20

extern bool PERCORE_L3_BW;

void __iomem *get_clkg_sram_base_addr(void);
void __iomem *get_l3ctl_sram_base_addr(void);
void __iomem *get_cdsu_sram_base_addr(void);
unsigned int get_wl(unsigned int wl_idx);
void update_pelt_data(unsigned int pelt_weight, unsigned int pelt_sum);
int dsu_pwr_swpm_init(void);
#endif
