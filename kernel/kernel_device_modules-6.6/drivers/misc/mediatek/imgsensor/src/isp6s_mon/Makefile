# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MTK_PLATFORM := isp6s_mon

ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
MTK_SENSOR_PROJECT := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))
else
MTK_SENSOR_PROJECT :=
endif
MTK_DERIVED_PLATFORM :=

$(info MTK_SENSOR PROJECT: $(MTK_SENSOR_PROJECT))
IMGSENSOR_DRIVER_PATH := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/src
MTK_DRIVER_PATH  := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek
COMMON_VERSION := v1_1

subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(MTK_DRIVER_PATH)/mmdvfs/ \
	-I$(IMGSENSOR_DRIVER_PATH)/inc \
	-I$(IMGSENSOR_DRIVER_PATH)/common/sysfs \
	-I$(IMGSENSOR_DRIVER_PATH)/common/adaptive_mipi \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/

subdir-ccflags-y += -DNO_I2C_MTK -DNO_CLK_METER -DSENINF_USE_RPM -DDFS_CTRL_BY_OPP

obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MON) += imgsensor_isp6s_mon.o
imgsensor_isp6s_mon-objs :=
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_hw.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_proc.o
#imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/seninf_clk.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/seninf.o

# Build sysfs folder
ifdef CONFIG_IMGSENSOR_SYSFS
include $(IMGSENSOR_DRIVER_PATH)/common/sysfs/Makefile
imgsensor_isp6s_mon-objs += $(SYSFS_OBJS)
endif

# Build adaptive mipi folder
ifdef CONFIG_CAMERA_ADAPTIVE_MIPI
include $(IMGSENSOR_DRIVER_PATH)/common/adaptive_mipi/Makefile
imgsensor_isp6s_mon-objs += $(ADAPTIVE_MIPI_OBJS)
endif

# for adding sensor driver
define add_subdrv
$(eval subdrv-rpath = $1)
$(info path: $(srctree)/$(src)/$1/Makefile)
$(eval include $(srctree)/$(src)/$1/Makefile)
endef

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/$(1)),)
unknown_drv += $(1)
else
common_drv += $(1)
endif
else
platform_drv += $(1)
endif
else
project_drv += $(1)
endif
endef


$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

CUSTOM_KERNEL_KNOWN_DRV := $(filter-out $(unknown_drv),$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)))
CUSTOM_KERNEL_FLAGS := $(addprefix -D, $(shell echo $(CUSTOM_KERNEL_KNOWN_DRV) | tr a-z A-Z))
subdir-ccflags-y += $(CUSTOM_KERNEL_FLAGS)

ifneq ($(unknown_drv),)
$(info unknown_drv: $(unknown_drv))
endif

ifneq ($(common_drv),)
$(info common_drv : $(common_drv))
$(foreach drv,$(common_drv),$(eval $(call add_subdrv,../common/$(COMMON_VERSION)/$(drv))))
endif

ifneq ($(platform_drv),)
$(info platform_drv : $(platform_drv))
$(foreach drv,$(platform_drv),$(eval $(call add_subdrv,$(drv))))
endif

ifneq ($(project_drv),)
$(info project_drv : $(project_drv))
$(foreach drv, $(project_drv), $(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/$(drv))))
endif

#camera_hw
ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y		 += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/camera_hw/imgsensor_cfg_table.o
$(eval $(call add_subdrv,camera_hw))
else
subdir-ccflags-y		 += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw_$(MTK_DERIVED_PLATFORM)
subdir-ccflags-y		 += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
$(eval $(call add_subdrv,camera_hw_$(MTK_DERIVED_PLATFORM)))
endif
ifneq ($(project_drv),)
$(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
endif
else
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/camera_hw
#subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw_$(MTK_DERIVED_PLATFORM)
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
$(eval $(call add_subdrv,camera_hw))
$(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
endif

subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/camera_hw
subdir-ccflags-y        += -DNEED_LATE_INITCALL

#seninf
ifneq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf_$(MTK_DERIVED_PLATFORM)
$(eval $(call add_subdrv,seninf_$(MTK_DERIVED_PLATFORM)))
else
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf/
$(eval $(call add_subdrv,seninf))
endif

$(info imgsensor_isp6s_mon-objs : $(imgsensor_isp6s_mon-objs))
$(info imgsensor_isp6s_mon subdir-ccflags-y : $(subdir-ccflags-y))


