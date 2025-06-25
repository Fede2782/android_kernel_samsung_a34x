# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Werror

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc/dpc_v1 \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mml \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdp \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2 \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
             -I$(DEVICE_MODULES_PATH)/drivers/soc/mediatek \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

obj-$(CONFIG_MTK_DPC) := mtk_dpc_v1.o

mtk_dpc_v1-objs += mtk_dpc_driver.o
mtk_dpc_v1-objs += mtk_dpc_mmp.o

obj-$(CONFIG_MTK_DPC) += mtk_vdisp_v1.o
mtk_vdisp_v1-objs += mtk_vdisp.o
mtk_vdisp_v1-objs += mtk_vdisp_avs.o
