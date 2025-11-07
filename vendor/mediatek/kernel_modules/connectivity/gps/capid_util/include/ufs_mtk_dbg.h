/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _UFS_MTK_DBG_H
#define _UFS_MTK_DBG_H

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
extern int ufs_mtk_cali_hold(void);
extern int ufs_mtk_cali_release(void);
#endif

#endif /* _UFS_MTK_DBG_H */
