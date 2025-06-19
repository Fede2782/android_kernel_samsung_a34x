/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

#ifndef _MTK_IO_PGTABLE_ARM_
#define _MTK_IO_PGTABLE_ARM_

#include <linux/io-pgtable-arm.h>

struct arm_lpae_io_pgtable_ext {
	struct arm_lpae_io_pgtable	data;
	spinlock_t			split_lock;
};

/* Struct accessors */
#define io_pgtable_to_data_ext(x)					\
	container_of((x), struct arm_lpae_io_pgtable_ext, data)

#define ARM_64_LPAE_S1_CONTIG U32_MAX
extern struct io_pgtable_init_fns mtk_io_pgtable_arm_64_lpae_s1_contig_fns;

struct io_pgtable_ops *mtk_alloc_io_pgtable_ops(enum io_pgtable_fmt fmt,
			struct io_pgtable_cfg *cfg,
			void *cookie);

void mtk_free_io_pgtable_ops(struct io_pgtable_ops *ops);

#endif /* _MTK_IO_PGTABLE_ARM_ */
