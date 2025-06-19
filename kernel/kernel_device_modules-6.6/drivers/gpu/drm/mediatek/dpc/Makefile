# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Werror

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox

obj-$(CONFIG_MTK_DPC) += dpc_v1/
obj-$(CONFIG_MTK_DPC) += dpc_v2/
