/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_HW_SEMAPHORE_H
#define __MTK_HW_SEMAPHORE_H

enum sema_type {
	SEMA_TYPE_SPM = 0,
	SEMA_TYPE_VCP,
	SEMA_TYPE_NR,
};

enum master {
	MASTER_AP = 0,
	MASTER_VCP,
	MASTER_UNKNOW,
	MASTER_SMMU,
};

#if IS_ENABLED(CONFIG_MTK_HW_SEMAPHORE)

int mtk_hw_semaphore_ctrl(u32 master_id, bool is_get);

#else

static inline int mtk_hw_semaphore_ctrl(u32 master_id, bool is_get)
{
	return 0;
}

#endif /* CONFIG_MTK_HW_SEMAPHORE */

#endif /* __MTK_HW_SEMAPHORE_H */
