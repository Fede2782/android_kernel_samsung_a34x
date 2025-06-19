# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the touchscreen drivers.
#
obj-m += tui-common.o
TOUCH_LISTS := $(subst ",,$(CONFIG_TOUCHSCREEN_MTK_TOUCH))
obj-y += $(foreach TOUCH,$(TOUCH_LISTS),$(TOUCH)/)

ifneq ($(wildcard $(srctree)/$(src)/k6985v1_64_alpha),)
obj-$(CONFIG_TOUCHSCREEN_MT6985_ALPHA_SUPPORT) += k6985v1_64_alpha/
endif

obj-$(CONFIG_TOUCHSCREEN_BOTAI_MULTI_TOUCH) += BoTai_Multi_Touch/
obj-$(CONFIG_TOUCHSCREEN_GT9XX) += gt9xx/
obj-$(CONFIG_TOUCHSCREEN_ILITEK_TOUCH) += ILITEK/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_96789) += synaptics_dsx_96789/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_96851) += synaptics_dsx_96851/
obj-$(CONFIG_TOUCHSCREEN_FOCALTECH_TOUCH) += focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += NT36532/
obj-$(CONFIG_TOUCHSCREEN_GOODIX_6936_BRL) += GT6936/

