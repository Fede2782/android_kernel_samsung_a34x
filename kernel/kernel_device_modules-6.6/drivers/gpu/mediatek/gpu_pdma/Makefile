#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpu_pdma/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpueb/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq

ifneq ($(CONFIG_MTK_GPU_LEGACY),y)
ifneq ($(wildcard $(srctree)/$(src)/gpu_pdma_mt6991.c),)
mtk_gpu_pdma_mt6991-y := gpu_pdma_mt6991.o
obj-$(CONFIG_MTK_GPU_MT6991_SUPPORT) += mtk_gpu_pdma_mt6991.o
endif
endif