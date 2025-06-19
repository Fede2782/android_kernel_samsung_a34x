WLA_BUILD_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/wla

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(WLA_BUILD_FOLDER)/inc/
ccflags-y += -I$(WLA_BUILD_FOLDER)

subdir-ccflags-y += -I$(WLA_BUILD_FOLDER)/wla_fs/

#Generic subdir build
obj-$(CONFIG_MTK_WLA) += version/
