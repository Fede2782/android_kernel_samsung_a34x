#
# Copyright (C) 2024 MediaTek Inc.
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
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/leakage_table_v2/ \

obj-$(CONFIG_MTK_STATIC_POWER_LEGACY) += mediatek_static_power.o
mediatek_static_power-y += mtk_static_power.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
mediatek_static_power-y += mtk_static_power_plat.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
mediatek_static_power-y += mtk_static_power_6765.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6833)),)
mediatek_static_power-y += mtk_static_power_6833.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
mediatek_static_power-y += mtk_static_power_6739.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
mediatek_static_power-y += mtk_static_power_6761.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6877)),)
mediatek_static_power-y += mtk_static_power_6877.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
mediatek_static_power-y += mtk_static_power_6893.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6781)),)
mediatek_static_power-y += mtk_static_power_6781.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6853)),)
mediatek_static_power-y += mtk_static_power_6853.o
endif
