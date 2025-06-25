# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := isp6s_mou
MTK_CUSTOM_PATH := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_PLATFORM_SRC_PATH := $(CAM_CAL_SRC_PATH)/$(MTK_PLATFORM)
COMMON_VERSION := v2

MTK_PLATFORM_KO := camera_eeprom_$(MTK_PLATFORM)

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_PLATFORM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION) \

# Common code
ifeq ($(filter $(CONFIG_MTK_CAM_CAL_ISP6S_MOU), m), $(CONFIG_MTK_CAM_CAL_ISP6S_MOU))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_MOU) += $(MTK_PLATFORM_KO).o
endif

$(MTK_PLATFORM_KO)-objs := ../common/$(COMMON_VERSION)/eeprom_driver.o
$(MTK_PLATFORM_KO)-objs += ../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o
$(MTK_PLATFORM_KO)-objs += ../common/$(COMMON_VERSION)/eeprom_utils.o

# Platform code
$(MTK_PLATFORM_KO)-objs += eeprom_i2c_custom_driver.o
$(MTK_PLATFORM_KO)-objs += cam_cal_list.o
