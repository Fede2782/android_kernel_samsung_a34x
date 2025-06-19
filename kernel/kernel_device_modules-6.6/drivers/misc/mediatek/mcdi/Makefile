#
# Copyright (C) 2024 MediaTek Inc.
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

ccflags-y += -I$(srctree)/kernel/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/include/mcdi_v1/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/cpuidle/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm/
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -DUSE_TINYSYS_SSPM
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v0/
ccflags-y += -DUSE_TINYSYS_SSPM
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
ccflags-y += -DUSE_TINYSYS_SSPM
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2/
else
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v3/
endif
endif

obj-$(CONFIG_MEDIATEK_MCDI) += mcdi.o
mcdi-y += mcdi_v1/mtk_mcdi_main.o
mcdi-y += mcdi_v1/mtk_mcdi_util.o
mcdi-y += mcdi_v1/mtk_mcdi_profile.o
mcdi-y += mcdi_v1/mtk_mcdi_cpc.o
mcdi-y += mcdi_v1/mtk_mcdi_mcupm.o
mcdi-y += mtk_mcdi_governor.o
mcdi-y += mtk_mcdi_governor_lib.o
mcdi-y += mtk_mcdi_governor_hint.o
mcdi-y += mtk_mcdi_api.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/plat_k68/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm/common_v1/
mcdi-y += plat_k68/mtk_mcdi_plat.o
mcdi-y += plat_k68/mtk_mcdi_state.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/platform/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm/common_v0/
mcdi-y += platform/mtk_mcdi_plat.o
mcdi-y += platform/mtk_mcdi_state.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/plat_k6765/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm/common_v0/
mcdi-y += plat_k6765/mtk_mcdi_plat.o
mcdi-y += plat_k6765/mtk_mcdi_state.o
endif
