
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/memory/mediatek/

obj-$(CONFIG_MTK_EMI_LEGACY) += emi-dummy.o

ifeq ($(CONFIG_MTK_EMI_LEGACY_VERSION), 1)
obj-$(CONFIG_MTK_EMI_LEGACY) += emi_legacy_v1/
endif

ifeq ($(CONFIG_MTK_EMI_LEGACY_VERSION), 2)
obj-$(CONFIG_MTK_EMI_LEGACY) += emi_legacy_v2/
endif


#Version List
#	Legacy_V1
#		mt6767, mt6768, mt6769; mt6761; mt6762, mt6765;
#	Legacy_V2
#		mt6833, mt6853, mt6877, mt6885, mt6893

