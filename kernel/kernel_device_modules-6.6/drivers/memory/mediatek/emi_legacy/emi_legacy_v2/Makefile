ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/memory/mediatek/

obj-$(CONFIG_MTK_EMI_LEGACY) += emi.o
emi-objs := emi-cen.o emi-isu.o emi-dbg.o
obj-$(CONFIG_MTK_EMI_LEGACY) += emi-mpu.o
obj-$(CONFIG_MTK_EMI_LEGACY) += emi-fake-eng.o
obj-$(CONFIG_MTK_EMI_LEGACY) += emi-mpu-v2.o
obj-$(CONFIG_MTK_EMI_LEGACY) += emi-slb.o
obj-$(CONFIG_MTK_EMI_LEGACY) += emi-mpu-hook-v1.o
obj-$(CONFIG_MTK_EMI_MPU_TEST) += emi-mpu-test.o
obj-$(CONFIG_MTK_EMI_MPU_TEST) += emi-mpu-test-v2.o
