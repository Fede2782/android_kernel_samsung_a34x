#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpueb/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/aee/aed/

ccflags-y += \
	-Wno-unused-variable \
	-Wno-unused-function

mtk_gpufreq_wrapper_legacy-y := \
	gpufreq_v2_legacy.o \
	gpufreq_history_legacy.o \
	gpufreq_debug_legacy.o

mtk_gpufreq_mt6768-y := \
	gpufreq_history_mt6768.o \
	gpuppm_legacy.o \
	gpufreq_mt6768.o

mtk_gpufreq_mt6761-y := \
	gpufreq_history_mt6761.o \
        gpuppm_legacy.o \
	gpufreq_mt6761.o

mtk_gpufreq_mt6893-y := \
	gpufreq_history_mt6893.o \
	gpuppm_legacy.o \
	gpufreq_mt6893.o

mtk_gpufreq_mt6833-y := \
	gpufreq_history_mt6833.o \
	gpuppm_legacy.o \
	gpufreq_mt6833.o

mtk_gpufreq_mt6877-y := \
	gpufreq_history_mt6877.o \
	gpuppm_legacy.o \
	gpufreq_mt6877.o

mtk_gpufreq_early_init-y := \
	gpufreq_early_init.o

mtk_gpufreq_mt6781-y := \
	gpufreq_history_mt6781.o \
	gpuppm_legacy.o \
	gpufreq_mt6781.o

mtk_gpufreq_mt6765-y := \
	gpufreq_history_mt6765.o \
	gpuppm_legacy.o \
	gpufreq_mt6765.o

mtk_gpufreq_mt6853-y := \
	gpufreq_history_mt6853.o \
	gpuppm_legacy.o \
	gpufreq_mt6853.o

obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpufreq_wrapper_legacy.o

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6768.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6768.c),)
obj-$(CONFIG_MTK_GPU_MT6768_SUPPORT) += mtk_gpufreq_mt6768.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6893.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6893.c),)
obj-$(CONFIG_MTK_GPU_MT6893_SUPPORT) += mtk_gpufreq_mt6893.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6833.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6833.c),)
obj-$(CONFIG_MTK_GPU_MT6833_SUPPORT) += mtk_gpufreq_mt6833.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6877.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6877.c),)
obj-$(CONFIG_MTK_GPU_MT6877_SUPPORT) += mtk_gpufreq_mt6877.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_early_init.c),)
obj-$(CONFIG_MTK_GPU_MT6877_SUPPORT) += mtk_gpufreq_early_init.o
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6761.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6761.c),)
obj-$(CONFIG_MTK_GPU_MT6761_SUPPORT) += mtk_gpufreq_mt6761.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6765.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6765.c),)
obj-$(CONFIG_MTK_GPU_MT6765_SUPPORT) += mtk_gpufreq_mt6765.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6781.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6781.c),)
obj-$(CONFIG_MTK_GPU_MT6781_SUPPORT) += mtk_gpufreq_mt6781.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6853.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_history_mt6853.c),)
obj-$(CONFIG_MTK_GPU_MT6853_SUPPORT) += mtk_gpufreq_mt6853.o
endif
endif
