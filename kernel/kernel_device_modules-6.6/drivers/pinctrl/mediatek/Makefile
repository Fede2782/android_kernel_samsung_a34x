# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/pinctrl/mediatek

# Core
ifeq ($(CONFIG_DEVICE_MODULES_EINT_MTK),m)
obj-m += pinctrl-mtk-v2.o
else ifeq ($(CONFIG_DEVICE_MODULES_PINCTRL_MTK_V2),m)
obj-m += pinctrl-mtk-v2.o
else ifeq ($(CONFIG_DEVICE_MODULES_PINCTRL_MTK_PARIS),m)
obj-m += pinctrl-mtk-v2.o
endif

ifeq ($(CONFIG_DEVICE_MODULES_EINT_MTK),y)
obj-y += mtk-eint.o
else ifeq ($(CONFIG_DEVICE_MODULES_EINT_MTK),m)
pinctrl-mtk-v2-y += mtk-eint.o
endif

ifeq ($(CONFIG_DEVICE_MODULES_PINCTRL_MTK_V2),y)
obj-y += pinctrl-mtk-common-v2.o
else ifeq ($(CONFIG_DEVICE_MODULES_PINCTRL_MTK_V2),m)
pinctrl-mtk-v2-y += pinctrl-mtk-common-v2.o
endif

ifeq ($(CONFIG_DEVICE_MODULES_PINCTRL_MTK_PARIS),y)
obj-y += pinctrl-paris.o pinctrl-mtk-common-v2_debug.o
else ifeq ($(CONFIG_DEVICE_MODULES_PINCTRL_MTK_PARIS),m)
pinctrl-mtk-v2-y += pinctrl-paris.o
obj-m += pinctrl-mtk-common-v2_debug.o
endif

# SoC Drivers
obj-$(CONFIG_DEVICE_MODULES_PINCTRL_MT6779)	+= pinctrl-mt6779.o
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6363.c),)
obj-$(CONFIG_PINCTRL_MT6363)	+= pinctrl-mt6363.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6373.c),)
obj-$(CONFIG_PINCTRL_MT6373)	+= pinctrl-mt6373.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6853.c),)
obj-$(CONFIG_PINCTRL_MT6853)	+= pinctrl-mt6853.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6833.c),)
obj-$(CONFIG_PINCTRL_MT6833)	+= pinctrl-mt6833.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6873.c),)
obj-$(CONFIG_PINCTRL_MT6873)	+= pinctrl-mt6873.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6885.c),)
obj-$(CONFIG_PINCTRL_MT6885)	+= pinctrl-mt6885.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6886.c),)
obj-$(CONFIG_PINCTRL_MT6886)	+= pinctrl-mt6886.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6897.c),)
obj-$(CONFIG_PINCTRL_MT6897)	+= pinctrl-mt6897.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6899.c),)
obj-$(CONFIG_PINCTRL_MT6899)	+= pinctrl-mt6899.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6983.c),)
obj-$(CONFIG_PINCTRL_MT6983)	+= pinctrl-mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6985.c),)
obj-$(CONFIG_PINCTRL_MT6985)	+= pinctrl-mt6985.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6768.c),)
obj-$(CONFIG_PINCTRL_MT6768)	+= pinctrl-mt6768.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6761.c),)
obj-$(CONFIG_PINCTRL_MT6761)	+= pinctrl-mt6761.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6781.c),)
obj-$(CONFIG_PINCTRL_MT6781)	+= pinctrl-mt6781.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6765.c),)
obj-$(CONFIG_DEVICE_MODULES_PINCTRL_MT6765)	+= pinctrl-mt6765.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6853.c),)
obj-$(CONFIG_PINCTRL_MT6853)	+= pinctrl-mt6853.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6877.c),)
obj-$(CONFIG_PINCTRL_MT6877)	+= pinctrl-mt6877.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6989.c),)
obj-$(CONFIG_PINCTRL_MT6989)	+= pinctrl-mt6989.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt6991.c),)
obj-$(CONFIG_PINCTRL_MT6991)	+= pinctrl-mt6991.o
endif
ifneq ($(wildcard $(srctree)/$(src)/pinctrl-mt8188.c),)
obj-$(CONFIG_DEVICE_MODULES_PINCTRL_MT8188)	+= pinctrl-mt8188.o
endif
obj-$(CONFIG_DEVICE_MODULES_PINCTRL_MT8195)    += pinctrl-mt8195.o
# Debug and verify gpio configurations.
obj-$(CONFIG_SEC_GPIO_DVS)	+= sec-pinmux.o secgpio_dvs.o