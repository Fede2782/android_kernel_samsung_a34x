# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Werror

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc/dpc_v2 \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mml \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdp \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2 \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi \
             -I$(DEVICE_MODULES_PATH)/drivers/soc/mediatek \
             -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv_v2/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

ifeq ($(strip $(MGK_INTERNAL)),true)
ccflags-y += -DDISP_VIDLE_ENABLE
endif

obj-$(CONFIG_MTK_DPC) := mtk_dpc_v2.o

mtk_dpc_v2-objs += mtk_dpc_driver.o
mtk_dpc_v2-objs += mtk_dpc_mmp.o

obj-$(CONFIG_MTK_DPC) += mtk_vdisp_v2.o
mtk_vdisp_v2-objs += mtk_vdisp.o
mtk_vdisp_v2-objs += mtk_vdisp_avs.o
