obj-$(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) += mcupm.o
mcupm-objs += mcupm_driver.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include/v1
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/soc/mediatek
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG

