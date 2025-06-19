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

ccflags-y += -DMET_USER_EVENT_SUPPORT
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ppm_v3/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/	\

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(CPUFREQ_ROOT_DIR)/src/plat_k68
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/src/plat_k68/Makefile
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ppm_v3/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/mt6768/include	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/mt6768/
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6893/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6765 \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/common/include	\
	-I$(srctree)/drivers/misc/mediatek/cpufreq_v2/src/common/platform   \
	-I$(srctree)/drivers/misc/mediatek/mcupm/common/

endif

ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6761)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6761   \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/common/include	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/common/

endif

ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6877)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6877	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/swpm_v1
endif

#sspm

#sspm for mt6768
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
	ccflags-y += -DUSE_SSPM_VER_V1
	ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif
#sspm for k6765,mt6761
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -DUSE_SSPM_VER_V1
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v0/
endif
#sspm for mt6833,mt6853,mt6781,mt6893
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2/
endif

obj-$(CONFIG_MEDIATEK_CPU_DVFS) += CPU_DVFS.o
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
CPU_DVFS-y := common/mtk_cpufreq_main.o
CPU_DVFS-y += common/mtk_cpufreq_interface.o
CPU_DVFS-y += common/mtk_cpufreq_api.o
CPU_DVFS-y += common/mtk_cpufreq_debug.o
CPU_DVFS-y += common/platform/mtk_cpufreq_platform.o
else
CPU_DVFS-y := mtk_cpufreq_main.o
CPU_DVFS-y += mtk_cpufreq_interface.o
CPU_DVFS-y += mtk_cpufreq_api.o
CPU_DVFS-y += mtk_cpufreq_debug.o
CPU_DVFS-y += mtk_cpufreq_hybrid.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
CPU_DVFS-y += plat_k68/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
CPU_DVFS-y += plat_k6765/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6833)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6833 \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include

ifeq ($(findstring k6833v1_64_33m,$(PROJECT_DTB_NAMES)),k6833v1_64_33m)
subdir-ccflags-y = -DMTK_5GCM_PROJECT
endif
CPU_DVFS-y += plat_k6833/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6761)),)
CPU_DVFS-y += plat_k6761/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6877)),)
CPU_DVFS-y += plat_k6877/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
CPU_DVFS-y += plat_k6893/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6781)),)
ccflags-y += \
	-DUSE_SSPM_VER_V2	\
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6781/

CPU_DVFS-y += plat_k6781/mtk_cpufreq_platform.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6853)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6853/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include

	ifeq ($(findstring _qea,$(PROJECT_DTB_NAMES)),_qea)
	subdir-ccflags-y = -DQEA
	endif

	ifeq ($(findstring k6853tv1,$(PROJECT_DTB_NAMES)),k6853tv1)
	subdir-ccflags-y = -DK6853TV1
	endif

	ifeq ($(findstring _turbo,$(PROJECT_DTB_NAMES)),_turbo)
	subdir-ccflags-y = -DTURBO
	endif

CPU_DVFS-y += plat_k6853/mtk_cpufreq_platform.o
obj-${CONFIG_MTK_PLAT_POWER_6853} += plat_k6853/mtk_cpufreq_utils.o
endif