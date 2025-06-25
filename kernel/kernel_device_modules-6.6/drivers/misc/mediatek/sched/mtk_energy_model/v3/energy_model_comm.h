/* SPDX-License-Identifier: GPL-2.0
 *
 * energy_model.h - energy model header file
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
 */

#define MAX_NR_WL	5

struct mtk_relation {
	unsigned int cpu_type;
	unsigned int dsu_type;
};

struct mtk_mapping {
	unsigned int nr_cpu_type;
	unsigned int nr_dsu_type;
	unsigned int total_type;
	struct mtk_relation *cpu_to_dsu;
};
