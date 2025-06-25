#
# Copyright (C) 2015 MediaTek Inc.
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

obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += vcodec_kernel_common_driver.o
vcodec_kernel_common_driver-$(CONFIG_MTK_VIDEOCODEC_DRIVER) +=  videocodec_kernel.o   \
								common/val/src/val.o   \
								common/drv/src/drv_base.o

ifeq ($(CONFIG_MTK_VIDEOCODEC_IPM_V1),y)
	ccflags-y += \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/v1
	obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += vcodec_kernel_driver-v1.o
	vcodec_kernel_driver-v1-$(CONFIG_MTK_VIDEOCODEC_DRIVER) +=  v1/mtk_vcodec_pm_plat.o   \
									v1/dvfs_v2.o	\
									v1/videocodec_kernel_drv_plat.o \
									mtk_vcodec_pm.o   \
									mtk_vcodec_intr.o   \
									mtk_vcodec_mem.o   \
									videocodec_kernel_drv.o
endif

ccflags-y += \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/common/drv/inc   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/include   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/include/kernel   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect   \
    -I$(srctree)/drivers/misc/mediatek/iommu/ \
    -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps
