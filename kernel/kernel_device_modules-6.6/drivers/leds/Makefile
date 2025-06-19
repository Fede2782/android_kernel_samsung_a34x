# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/flashlight
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/leds
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/video/backlight
subdir-ccflags-y += -I$(srctree)/drivers/power/supply

obj-$(CONFIG_DEVICE_MODULES_LEDS_MT6360)		+= leds-mt6360.o
obj-$(CONFIG_LEDS_MT6362)		+= leds-mt6362.o
obj-$(CONFIG_LEDS_MTK)			+= leds-mtk.o
obj-$(CONFIG_LEDS_MTK_PWM)		+= leds-mtk-pwm.o
obj-$(CONFIG_LEDS_MTK_DISP)		+= leds-mtk-disp.o
obj-$(CONFIG_LEDS_REGULATOR_VIBRATOR)	+= regulator-vibrator.o
# Flash and Torch LED Drivers
obj-$(CONFIG_LEDS_CLASS_FLASH)		+= flash/

ifeq ($(CONFIG_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/inc \
                    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/src/common/sysfs
$(info leds include path: : $(subdir-ccflags-y))