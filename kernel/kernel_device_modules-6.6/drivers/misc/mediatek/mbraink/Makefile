#####MBRAINK Common Include #####
MTK_MBRAINK_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/
ccflags-y += -I$(MTK_MBRAINK_FOLDER)
subdir-ccflags-y += -I$(MTK_MBRAINK_FOLDER)
subdir-ccflags-y += -Wno-frame-address

ccflags-y += -I$(srctree)/drivers/android/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/bridge
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/bridge

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/mbraink_auto
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/mbraink_auto

ifeq ($(CONFIG_MTK_LOW_POWER_MODULE),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/v2
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/v2

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include
endif

ifeq ($(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/misc/mediatek/videocodec/
endif

ccflags-y += -I$(srctree)/sound/usb
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/usb/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/usb/


subdir-ccflags-y += -Wformat

obj-${CONFIG_MTK_MBRAINK} += mtk_mbraink.o
obj-${CONFIG_MTK_MBRAINK} += modules/
obj-${CONFIG_MTK_MBRAINK} += bridge/

mtk_mbraink-objs += mbraink_power.o mbraink_video.o mbraink_process.o mbraink.o mbraink_memory.o mbraink_gpu.o mbraink_audio.o mbraink_cpufreq.o mbraink_battery.o mbraink_pmu.o mbraink_gps.o mbraink_wifi.o mbraink_usb.o

ifeq ($(CONFIG_MTK_MBRAINK_MT8678),m)
mtk_mbraink-objs += mbraink_auto/mbraink_auto.o

ifeq ($(CONFIG_GRT_HYPERVISOR),m)
mtk_mbraink-objs += mbraink_auto/mbraink_hypervisor_virtio.o mbraink_auto/mbraink_hypervisor.o

subdir-ccflags-y += -I$(srctree)/kernel/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/vhost/	\
			 -I$(KERNEL_SRC)/drivers/vhost/
endif
endif
