#
# Makefile for misc devices that really don't fit anywhere else.
#

ccflags-y += -I$(src)/inc -I$(src)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lens/tof/vl53l4/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lens/tof/core/inc

ifdef DEBUG
ccflags-y	+= -DDEBUG
endif

ifdef VL53L4
ccflags-y	+= -DVL53L4
endif

ifdef VL53LX_LOG_ENABLE
ccflags-y	+= -DVL53LX_LOG_ENABLE
endif
