# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2021 MediaTek Inc.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdp/$(MTK_PLATFORM) \
	     -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox \
	     -I$(DEVICE_MODULES_PATH)/include/uapi/linux

ccflags-$(CONFIG_VIRTIO_CMDQ) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdp
obj-$(CONFIG_VIRTIO_CMDQ) += virtio-cmdq.o
