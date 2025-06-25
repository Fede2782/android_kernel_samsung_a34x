# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := isp6s_mon
MTK_CUSTOM_PATH := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_PLATFORM_SRC_PATH := $(CAM_CAL_SRC_PATH)/$(MTK_PLATFORM)

ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))
else
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
endif

COMMON_VERSION := v2

MTK_PLATFORM_KO := camera_eeprom_$(MTK_PLATFORM)

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/common/sysfs \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_PLATFORM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION) \

subdir-ccflags-$(CONFIG_CAMERA_OIS) += -I$(MTK_CUSTOM_PATH)/lens/cam_ois/inc

# Common code
ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP6S_MON), m), $(CONFIG_MTK_CAM_CAL_ISP6S_MON))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_MON) += $(MTK_PLATFORM_KO).o
endif

$(MTK_PLATFORM_KO)-objs := ../common/$(COMMON_VERSION)/eeprom_driver.o
$(MTK_PLATFORM_KO)-objs += ../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o
$(MTK_PLATFORM_KO)-objs += ../common/$(COMMON_VERSION)/eeprom_utils.o

# Platform code
$(MTK_PLATFORM_KO)-objs += eeprom_i2c_custom_driver.o

define INC_CUSTOM_SRC
ifeq ($(wildcard $(CAM_CAL_PLATFORM_SRC_PATH)/camera_project/$(ARCH_MTK_PROJECT)/$(1).c),)
$(MTK_PLATFORM_KO)-objs += $(1).o
else
$(MTK_PLATFORM_KO)-objs += camera_project/$(ARCH_MTK_PROJECT)/$(1).o
endif
endef

# Project or platform code
CUSTOM_SRC_LIST := cam_cal_list
$(foreach src,$(CUSTOM_SRC_LIST),$(eval $(call INC_CUSTOM_SRC,$(src))))

$(info $(MTK_PLATFORM_KO)-objs : $($(MTK_PLATFORM_KO)-objs))
