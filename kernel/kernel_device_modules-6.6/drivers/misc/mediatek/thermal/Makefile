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

ccflags-y  += -I$(srctree)/drivers/thermal/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ppm_v3/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include/gpufreq_v1/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2

ifneq ($(strip $(CONFIG_MTK_APUSYS_SUPPORT)),)
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/2.5/inc/
else
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vpu/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdla/1.0
endif

ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/include/

include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/common/thermal_zones/Makefile

ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ppm_v3/inc/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/

obj-$(CONFIG_MTK_LEGACY_THERMAL) += thermal_monitor.o

thermal_monitor-y += common/mtk_thermal_platform.o
thermal_monitor-y += common/ap_thermal_limit.o
thermal_monitor-y += common/mtk_thermal_timer.o
thermal_monitor-y += mtk_cooler_shutdown.o
ifneq ($(strip $(CONFIG_LEDS_MTK)),)
thermal_monitor-y += mtk_cooler_backlight.o
endif
thermal_monitor-y += mtk_cooler_kshutdown.o
thermal_monitor-y += mtk_cooler_cam.o

#thermal zones
thermal_monitor-y += common/thermal_zones/mtk_ts_battery.o

ifneq ($(CONFIG_POWER_EXT),y)
thermal_monitor-y += common/thermal_zones/mtk_ts_bts.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsmdpa.o
endif

thermal_monitor-y += common/thermal_zones/mtk_ts_pa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_pa_thput.o
thermal_monitor-y += common/thermal_zones/mtk_ts_charger.o


ifeq ($(strip $(CONFIG_MTK_PLAT_POWER_6853)),)
thermal_monitor-y += common/thermal_zones/mtk_ts_wmt.o
endif

thermal_monitor-y += common/thermal_zones/mtk_tsAll.o

thermal_monitor-y += common/thermal_zones/mtk_ts_pmic.o

#thermal cooler

thermal_monitor-y += common/coolers/mtk_cooler_atm.o
thermal_monitor-y += common/coolers/mtk_cooler_dtm.o
thermal_monitor-y += common/coolers/mtk_cooler_sysrst.o
thermal_monitor-y += common/coolers/mtk_cooler_VR_FPS.o
thermal_monitor-y += common/coolers/mtk_ta.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v4/src/Makefile
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v4/inc
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v4/include/
thermal_monitor-y += v4/mtk_thermal_monitor.o
thermal_monitor-y += v4/src/mtk_tc.o
thermal_monitor-y += v4/src/mtk_pmic_efuse.o
thermal_monitor-y += v4/src/mtk_ts_6357buck1.o
thermal_monitor-y += v4/src/mtk_ts_6357buck2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
thermal_monitor-y += common/thermal_zones/mtk_ts_charger2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_2nd.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/src/Makefile
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/inc
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += mtk_thermal_monitor.o
thermal_monitor-y += src/mtk_tc.o
thermal_monitor-y += src/mtk_ts_6358buck1.o
thermal_monitor-y += src/mtk_ts_6358buck2.o
thermal_monitor-y += src/mtk_ts_6358buck3.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
thermal_monitor-y += src/mtk_pmic_efuse.o
thermal_monitor-y += src/mtk_thermal_ipi.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6877)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v2/src/Makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include
thermal_monitor-y += v2/mtk_thermal_monitor.o
thermal_monitor-y += v2/src/mtk_ts_6359vcore.o
thermal_monitor-y += v2/src/mtk_ts_6359vproc.o
thermal_monitor-y += v2/src/mtk_ts_6359vgpu.o
thermal_monitor-y += v2/src/mtk_ts_6359tsx.o
thermal_monitor-y += v2/src/mtk_ts_6359dcxo.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsnrpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt_gen97.o
endif
thermal_monitor-y += v2/src/mtk_thermal_ipi.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBankv2.o
thermal_monitor-y += v2/src/mtk_thermal_ipi.o
thermal_monitor-y += v2/src/mtk_lvts_tc.o
thermal_monitor-y += v2/src/mtk_tc_wrapper.o
thermal_monitor-y += v2/src/mtk_pmic_efuse.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6853)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v6/src/Makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include
thermal_monitor-y += v6/mtk_thermal_monitor.o
thermal_monitor-y += v6/src/mtk_ts_6359vcore.o
thermal_monitor-y += v6/src/mtk_ts_6359vproc.o
thermal_monitor-y += v6/src/mtk_ts_6359vgpu.o
thermal_monitor-y += v6/src/mtk_ts_6359tsx.o
thermal_monitor-y += v6/src/mtk_ts_6359dcxo.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsnrpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt_gen97.o
endif
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBankv2.o
#thermal_monitor-y += v6/src/mtk_thermal_ipi.o
thermal_monitor-y += v6/src/mtk_lvts_tc.o
thermal_monitor-y += v6/src/mtk_tc_wrapper.o
thermal_monitor-y += v6/src/mtk_pmic_efuse.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6781)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v7/src/Makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include
thermal_monitor-y += v7/mtk_thermal_monitor.o
thermal_monitor-y += v7/src/mtk_ts_6366vcore.o
thermal_monitor-y += v7/src/mtk_ts_6366vproc.o
thermal_monitor-y += v7/src/mtk_ts_6366vgpu.o
thermal_monitor-y += v7/src/mtk_ts_6366tsx.o
thermal_monitor-y += v7/src/mtk_ts_6366dcxo.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsnrpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt_gen97.o
endif
thermal_monitor-y += v7/src/mtk_thermal_ipi.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBankv2.o
#thermal_monitor-y += v5/src/mtk_thermal_ipi.o
thermal_monitor-y += v7/src/mtk_lvts_tc.o
thermal_monitor-y += v7/src/mtk_tc_wrapper.o
thermal_monitor-y += v7/src/mtk_pmic_efuse.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6833)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v5/src/Makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include
thermal_monitor-y += v5/mtk_thermal_monitor.o
thermal_monitor-y += v5/src/mtk_ts_6359vcore.o
thermal_monitor-y += v5/src/mtk_ts_6359vproc.o
thermal_monitor-y += v5/src/mtk_ts_6359vgpu.o
thermal_monitor-y += v5/src/mtk_ts_6359tsx.o
thermal_monitor-y += v5/src/mtk_ts_6359dcxo.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsnrpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt_gen97.o
endif
thermal_monitor-y += v5/src/mtk_thermal_ipi.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBankv2.o
#thermal_monitor-y += v5/src/mtk_thermal_ipi.o
thermal_monitor-y += v5/src/mtk_lvts_tc.o
thermal_monitor-y += v5/src/mtk_tc_wrapper.o
thermal_monitor-y += v5/src/mtk_pmic_efuse.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v1/src/Makefile
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2_legacy/include
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v1/inc

thermal_monitor-y += v1/src/mtk_ts_6359vcore.o
thermal_monitor-y += v1/src/mtk_ts_6359vproc.o
thermal_monitor-y += v1/src/mtk_ts_6359vgpu.o
thermal_monitor-y += v1/src/mtk_ts_6359tsx.o
thermal_monitor-y += v1/src/mtk_ts_6359dcxo.o
thermal_monitor-y += v1/mtk_thermal_monitor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsnrpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt_gen97.o
endif
#thermal_monitor-y += v1/src/mtk_thermal_ipi.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBankv2.o
thermal_monitor-y += v1/src/mtk_thermal_ipi_mt6893.o
thermal_monitor-y += v1/src/mtk_lvts_tc_mt6893.o
thermal_monitor-y += v1/src/mtk_tc_wrapper_mt6893.o
thermal_monitor-y += v1/src/mtk_pmic_efuse_mt6893.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v3/src/Makefile
ccflags-y  += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v3/inc
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += v3/mtk_thermal_monitor.o
thermal_monitor-y += v3/src/mtk_tc.o
#thermal_monitor-y += v3/src/mtk_ts_6357buck1.o
#thermal_monitor-y += v3/src/mtk_ts_6357buck2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_2nd.o
thermal_monitor-y += v3/src/mtk_pmic_efuse.o
#thermal_monitor-y += common/thermal_zones/mtk_ts_charger2.o
endif