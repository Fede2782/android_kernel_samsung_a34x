#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/

ifneq ($(filter y m, $(CONFIG_MTK_TEE_GP_COORDINATOR)),)
include $(DEVICE_MODULES_PATH)/drivers/tee/gpapi/Makefile.client.include
else
ifneq ($(filter y m, $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/GP
endif
endif

obj-$(CONFIG_MTK_IOMMU_MISC) += mtk_iommu_util.o
mtk_iommu_util-objs-y += mtk-iommu-util.o
mtk_iommu_util-objs := $(mtk_iommu_util-objs-y)

obj-$(CONFIG_MTK_IOMMU_MISC_DBG)    += iommu_debug.o
obj-$(CONFIG_MTK_IOMMU_MISC_SECURE) += iommu_secure.o
obj-$(CONFIG_MTK_IOMMU_MISC_TEST)   += iommu_test.o
obj-$(CONFIG_MTK_IOMMU_MISC_TEST)   += iommu_engine.o
ifneq ($(filter y m, $(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)),)
obj-$(CONFIG_MTK_IOMMU_MISC_SECURE) += smmu_secure.o
endif
obj-$(CONFIG_DEVICE_MODULES_MTK_SMMU_QOS) += mtk_smmu_qos.o

ifeq ($(filter y m, $(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)),)
obj-$(CONFIG_MTK_IOMMU_MISC)        += iommu_gz.o

ifneq ($(filter y m, $(CONFIG_MTK_ENABLE_GENIEZONE)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/geniezone/public

iommu_gz-objs := iommu_pseudo.o iommu_gz_sec.o
else
iommu_gz-objs := iommu_pseudo.o
endif
endif
