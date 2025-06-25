#####MBRAINK Common Include #####
MTK_MBRAINK_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mbraink/
ccflags-y += -I$(MTK_MBRAINK_FOLDER)
subdir-ccflags-y += -I$(MTK_MBRAINK_FOLDER)

#####FOR SWPM Include #####
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/

ifeq ($(CONFIG_MTK_SWPM_MODULE),$(filter $(CONFIG_MTK_SWPM_MODULE),m))
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/debug/v6991
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/debug/v6991/subsys
endif

ifeq ($(CONFIG_MTK_FPSGO_V3),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/fpsgo_v3/fstb/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/fpsgo_v3/fstb/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/fpsgo_v3/mt-plat/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/fpsgo_v3/mt-plat/
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/spmi/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/spmi/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/regulator/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/regulator/

ifeq ($(CONFIG_MTK_GPU_SUPPORT),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include/
endif
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2/include/

ifeq ($(CONFIG_MTK_SYS_RES_DBG_SUPPORT),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/sys_res
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/sys_res

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/v2
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/v2

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include

endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv

ifeq ($(CONFIG_MTK_DVFSRC_MB),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dvfsrc/
endif

ccflags-y += -I$(srctree)/drivers/android/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/

ifeq ($(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/
endif

subdir-ccflags-y += -Wformat

BUILD_MBRAINK_MODULE_NAME=mtk_mbraink_v6991
obj-${CONFIG_MTK_MBRAINK_MT6991} += ${BUILD_MBRAINK_MODULE_NAME}.o

${BUILD_MBRAINK_MODULE_NAME}-objs += mbraink_v6991.o mbraink_v6991_memory.o mbraink_v6991_audio.o mbraink_v6991_battery.o mbraink_v6991_power.o mbraink_v6991_gpu.o mbraink_v6991_gps.o mbraink_v6991_wifi.o mbraink_v6991_camera.o

ifeq ($(CONFIG_MTK_SWPM_MODULE),$(filter $(CONFIG_MTK_SWPM_MODULE),m))
${BUILD_MBRAINK_MODULE_NAME}-objs += mbraink_sys_res_plat.o mbraink_sys_res_mbrain_plat.o mbraink_sys_res.o mbraink_sys_res_mbrain_dbg.o
endif
