# SPDX-License-Identifier: GPL-2.0
obj-y += arm/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu

ifneq ($(filter m y, $(CONFIG_DEVICE_MODULES_MTK_IOMMU_ARM32)),)
obj-$(CONFIG_DEVICE_MODULES_MTK_IOMMU_ARM32) += mtk_iommu_arm32.o
else
obj-$(CONFIG_DEVICE_MODULES_MTK_IOMMU) += mtk_iommu.o
endif
