# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/

ifneq ($(wildcard $(srctree)/$(src)/mtk-vmm-notifier-mt6991.c),)
obj-$(CONFIG_MTK_VMM_NOTIFIER_MT6991) += mtk-vmm-notifier-mt6991.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk-vmm-notifier.c),)
obj-$(CONFIG_MTK_VMM_NOTIFIER) += mtk-vmm-notifier.o
endif
