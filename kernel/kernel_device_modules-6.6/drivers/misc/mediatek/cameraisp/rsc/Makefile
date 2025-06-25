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

$(info Ming-Hsuan Debug)

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion \
             -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion/mtk
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/

ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/

$(info RSC: CONFIG_MTK_CAMERA_RSC_ISP6S = $(CONFIG_MTK_CAMERA_RSC_ISP6S))
$(info RSC: CONFIG_MTK_CAMERA_RSC_ISP6S_V1 = $(CONFIG_MTK_CAMERA_RSC_ISP6S_V1))
#obj-y += camera_rsc_dummy.c
obj-$(CONFIG_MTK_CAMERA_RSC_ISP6S) += camera_rsc_isp6s.o
camera_rsc_isp6s-objs += engine_request.o
camera_rsc_isp6s-objs += isp_6s/camera_rsc.o

obj-$(CONFIG_MTK_CAMERA_RSC_ISP6S_V1) += camera_rsc_isp6s_v1.o
camera_rsc_isp6s_v1-objs += engine_request.o
camera_rsc_isp6s_v1-objs += 6781/camera_rsc.o
