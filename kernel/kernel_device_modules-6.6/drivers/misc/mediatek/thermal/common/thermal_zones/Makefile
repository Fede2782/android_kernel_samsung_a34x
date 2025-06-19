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

ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/auxadc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/eem_v1/inc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/inc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/mt-plat/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include/


ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2/
else ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
else
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/
endif
endif
