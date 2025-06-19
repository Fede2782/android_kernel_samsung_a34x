ccflags-y += -Wall
ccflags-y += -DILITEK_KERNEL_DRIVER
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
#Un-mark below item to enable FW upgrade on boot
#ccflags-y += -DILITEK_BOOT_UPDATE

ILITEK_INTERFACE = i2c
#ILITEK_INTERFACE = spi


ifeq ($(ILITEK_INTERFACE), spi)
$(info start to build ilitek SPI driver)
ccflags-y += -DILITEK_SPI_INTERFACE
ilitek := ilitek_spi
else
$(info start to build ilitek I2C driver)
ilitek := ilitek_i2c
endif

obj-m += $(ilitek).o

$(ilitek)-y += \
	ilitek_main.o \
	ilitek_platform_init.o \
	ilitek_update.o \
	ilitek_tool.o \
	ilitek_protocol.o \
	tpd_control.o \
	tpd_button.o \
	tpd_debug.o	\
	tpd_default.o \
	tpd_misc.o \
	tpd_setting.o
