#
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
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdpm_v1/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm/

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v1.o
mtk_mdpm_v1-y += mtk_mdpm_platform_mt6761.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v1.o
mtk_mdpm_v1-y += mtk_mdpm_platform_mt6768.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6877)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v2.o
mtk_mdpm_v1-y += mtk_mdpm_platform_mt6877.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v1.o
mtk_mdpm_v1-y += mtk_mdpm_platform_6765.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v2.o
mtk_mdpm_v1-y += mtk_mdpm_platform_6893.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6853)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v2.o
mtk_mdpm_v1-y += mtk_mdpm_platform_6853.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6833)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v2.o
mtk_mdpm_v1-y += mtk_mdpm_platform_6833.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6781)),)
obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common_v2.o
mtk_mdpm_v1-y += mtk_mdpm_platform_6781.o
endif