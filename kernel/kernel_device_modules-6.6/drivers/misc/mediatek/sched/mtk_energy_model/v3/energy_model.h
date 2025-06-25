/* SPDX-License-Identifier: GPL-2.0
 *
 * energy_model.h - energy model header file
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
 */
#include "energy_model_comm.h"
#define EM_BASE_INFO_COUNT	1

/* WL Setting(TCM) */
#define DEFAULT_TYPE	0
#define DEFAULT_NR_TYPE	1
#define WL_MEM_RES_IND	1

extern struct mtk_mapping mtk_mapping;

struct leakage_data {
	void __iomem *base;
	int init;
};

struct em_base_info {
	void *usram_base;
	void *csram_base;
	void *wl_base;
	void *curve_adj_base;
	void *eemsn_log;
	bool curve_adj_support;
	bool wl_support;
	unsigned int *cpu_cluster_id;
	cpumask_t **cpu_cluster_mask;
	struct mtk_mapping mtk_mapping;
};

struct em_base_info *mtk_get_em_base_info(void);
extern bool is_wl_support(void);
extern __init int mtk_static_power_init(void);
