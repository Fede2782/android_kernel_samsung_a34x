#
# Makefile for the Synaptics DSX touchscreen driver.
#
# Each configuration option enables a list of files.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_96851) += synaptics_touch_96851.o
synaptics_touch_96851-y += synaptics_dsx_i2c.o
synaptics_touch_96851-y += synaptics_dsx_core.o

