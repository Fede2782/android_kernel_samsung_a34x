// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Author: Yunfei Wang <yf.wang@mediatek.com>
 */

#include "mtk-iommu-util.h"

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_IOMMU)
static const struct mtk_iommu_ops *iommu_ops;

int mtk_iommu_set_ops(const struct mtk_iommu_ops *ops)
{
	if (iommu_ops == NULL)
		iommu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_set_ops);

int mtk_iommu_update_pm_status(u32 type, u32 id, bool pm_sta)
{
	if (iommu_ops && iommu_ops->update_pm_status)
		return iommu_ops->update_pm_status(type, id, pm_sta);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_update_pm_status);

void mtk_iommu_set_pm_ops(const struct mtk_iommu_mm_pm_ops *ops)
{
	if (iommu_ops && iommu_ops->set_pm_ops)
		iommu_ops->set_pm_ops(ops);
}
EXPORT_SYMBOL_GPL(mtk_iommu_set_pm_ops);

#endif /* CONFIG_DEVICE_MODULES_MTK_IOMMU */

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
static const struct mtk_smmu_ops *smmu_ops;

static struct mtk_smmu_data *mtk_smmu_data_get(u32 smmu_type)
{
	if (smmu_ops && smmu_ops->get_smmu_data)
		return smmu_ops->get_smmu_data(smmu_type);

	return NULL;
}

int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops)
{
	if (smmu_ops == NULL)
		smmu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_set_ops);

int mtk_smmu_rpm_get(u32 smmu_type)
{
	struct mtk_smmu_data *data = mtk_smmu_data_get(smmu_type);

	if (data && smmu_ops && smmu_ops->smmu_power_get)
		return smmu_ops->smmu_power_get(&data->smmu);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_smmu_rpm_get);

int mtk_smmu_rpm_put(u32 smmu_type)
{
	struct mtk_smmu_data *data = mtk_smmu_data_get(smmu_type);

	if (data && smmu_ops && smmu_ops->smmu_power_put)
		return smmu_ops->smmu_power_put(&data->smmu);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_smmu_rpm_put);
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */

MODULE_DESCRIPTION("MediaTek IOMMU export API implementations");
MODULE_LICENSE("GPL");
