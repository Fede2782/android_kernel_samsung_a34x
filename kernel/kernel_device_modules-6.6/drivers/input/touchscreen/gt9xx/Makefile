#
# Makefile for the Goodix gt9xx touchscreen driver.
#
#subdir-ccflags-y += -DDEBUG
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
obj-$(CONFIG_TOUCHSCREEN_GT9XX)		+= gt9xx_touch.o
gt9xx_touch-y	+= gt9xx.o
gt9xx_touch-y	+= gt9xx_update.o
gt9xx_touch-y	+= goodix_tool.o
