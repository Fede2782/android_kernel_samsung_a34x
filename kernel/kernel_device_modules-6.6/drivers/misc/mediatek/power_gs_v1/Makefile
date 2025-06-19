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

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_POWER_GS_LEGACY) += mtk_power_gs_v1.o
mtk_power_gs_v1-y += mtk_power_gs.o
mtk_power_gs_v1-y += mtk_power_gs_api.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/plat_k68/
mtk_power_gs_v1-y += plat_k68/mtk_power_gs_6358_array.o
mtk_power_gs_v1-y += plat_k68/mtk_cg_array.o
mtk_power_gs_v1-y += plat_k68/mtk_dcm_array.o
mtk_power_gs_v1-y += plat_k68/mtk_power_gs_sp.o
mtk_power_gs_v1-y += plat_k68/mtk_power_gs_compare.o
mtk_power_gs_v1-y += plat_k68/mtk_power_gs_dump_pmic.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/plat_k61/
mtk_power_gs_v1-y += plat_k61/mtk_power_gs_6357_array.o
mtk_power_gs_v1-y += plat_k61/mtk_cg_array.o
mtk_power_gs_v1-y += plat_k61/mtk_dcm_array.o
mtk_power_gs_v1-y += plat_k61/mtk_power_gs_sp.o
mtk_power_gs_v1-y += plat_k61/mtk_power_gs_compare.o
mtk_power_gs_v1-y += plat_k61/mtk_power_gs_dump_pmic.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/plat_k65/
mtk_power_gs_v1-y += plat_k65/mtk_power_gs_6355_array.o
mtk_power_gs_v1-y += plat_k65/mtk_cg_array.o
mtk_power_gs_v1-y += plat_k65/mtk_dcm_array.o
mtk_power_gs_v1-y += plat_k65/mtk_power_gs_sp.o
mtk_power_gs_v1-y += plat_k65/mtk_power_gs_compare.o
mtk_power_gs_v1-y += plat_k65/mtk_power_gs_dump_pmic.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/plat_k6893/
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_6359_array.o
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_6315_1_array.o
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_6315_2_array.o
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_6315_3_array.o
mtk_power_gs_v1-y += plat_k6893/mtk_cg_array.o
mtk_power_gs_v1-y += plat_k6893/mtk_dcm_array.o
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_sp.o
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_compare.o
mtk_power_gs_v1-y += plat_k6893/mtk_power_gs_dump_pmic.o
endif