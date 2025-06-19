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

ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cm_mgr_legacy_v0 \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm \
	-I$(DEVICE_MODULES_PATH)/include \

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/src/plat_k6761
endif

ifneq ($(filter y m,$(CONFIG_INTERCONNECT_MTK_EXTENSION)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
endif

obj-${CONFIG_MTK_CM_MGR_LEGACY_V0} += mtk_cm_mgr_v0.o
mtk_cm_mgr_v0-y = mtk_cm_mgr_common.o
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
mtk_cm_mgr_v0-y += mtk_cm_mgr_mt6761.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/src/plat_k6765 \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1
mtk_cm_mgr_v0-y += mtk_cm_mgr_mt6765.o
endif
