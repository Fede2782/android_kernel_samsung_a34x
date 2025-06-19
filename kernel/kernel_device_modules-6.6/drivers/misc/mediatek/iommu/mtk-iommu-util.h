/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Author: Yunfei Wang <yf.wang@mediatek.com>
 */

#ifndef _MTK_IOMMU_UTIL_
#define _MTK_IOMMU_UTIL_

#include "../../../iommu/mtk_iommu.h"
#include "../../../iommu/arm/arm-smmu-v3/mtk-smmu-v3.h"

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_IOMMU)
int mtk_iommu_set_ops(const struct mtk_iommu_ops *ops);
int mtk_iommu_update_pm_status(u32 iommu_type, u32 iommu_id, bool pm_sta);
void mtk_iommu_set_pm_ops(const struct mtk_iommu_mm_pm_ops *ops);
#else /* CONFIG_DEVICE_MODULES_MTK_IOMMU */
static inline int mtk_iommu_set_ops(const struct mtk_iommu_ops *ops)
{
	return -1;
}

static inline int mtk_iommu_update_pm_status(u32 iommu_type, u32 iommu_id, bool pm_sta)
{
	return -1;
}

static inline void mtk_iommu_set_pm_ops(const struct mtk_iommu_mm_pm_ops *ops)
{
	return;
}

#endif /* CONFIG_DEVICE_MODULES_MTK_IOMMU */

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops);
int mtk_smmu_rpm_get(u32 smmu_type);
int mtk_smmu_rpm_put(u32 smmu_type);
#else /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
static inline int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops)
{
	return -1;
}

static inline int mtk_smmu_rpm_get(u32 smmu_type)
{
	return -1;
}

static inline int mtk_smmu_rpm_put(u32 smmu_type)
{
	return -1;
}
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
#endif /* _MTK_IOMMU_UTIL_ */
