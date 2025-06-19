
# Copyright (C) 2017 MediaTek Inc.
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

ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/	             \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/swpm_legacy/			\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/leakage_table_v2/           \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm_legacy/plat_k6877/    \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos/      \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v2/inc/mach/      \

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2/
endif

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_VERSION)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_TINYSYS_SSPM_VERSION)/
endif
endif

ifneq ($(strip $(CONFIG_MTK_GPU_SWPM_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_SWPM_MT6877)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpu_mali/mali_valhall/mali-r32p1/drivers/gpu/arm/midgard/platform/mtk_platform_common/
endif
endif


obj-${CONFIG_MTK_SWPM_MODULE_LEGACY} += mtk_swpm_legacy.o
mtk_swpm_legacy-y += mtk_swpm.o
mtk_swpm_legacy-y += mtk_swpm_common.o
ifneq ($(strip $(CONFIG_MTK_SWPM_MT6877)),)
mtk_swpm_legacy-y += plat_k6877/mtk_swpm_platform.o
mtk_swpm_legacy-y += plat_k6877/mtk_swpm_sp_platform.o
endif
