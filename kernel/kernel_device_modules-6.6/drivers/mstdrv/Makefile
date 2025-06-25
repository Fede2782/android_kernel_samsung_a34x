#@file Makefile
#@brief Makefile for MST drivers
#Copyright (c) 2015-2019, Samsung Electronics Corporation. All rights reserved.
#
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 and
#only version 2 as published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

obj-$(CONFIG_MST_LDO) += mstdrv.o
mstdrv-objs += mstdrv_main.o

ifeq ($(CONFIG_MST_NONSECURE),y)
mstdrv-objs += mstdrv_transmit_nonsecure.o
endif

$(warning "CONFIG_TEEGRIS_VERSION: $(CONFIG_TEEGRIS_VERSION)")
$(warning "srctree: $(srctree)")
$(warning "DEVICE_MODULES_PATH: $(DEVICE_MODULES_PATH)")
ifneq ($(DEVICE_MODULES_PATH),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev/$(CONFIG_TEEGRIS_VERSION)/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/tzdev/include
else
ccflags-y += -I$(srctree)/drivers/tee/tzdev/$(CONFIG_TEEGRIS_VERSION)/include
ccflags-y += -I$(srctree)/drivers/tee/tzdev/include
ccflags-y += -I$(srctree)/drivers/misc/tzdev/include
endif