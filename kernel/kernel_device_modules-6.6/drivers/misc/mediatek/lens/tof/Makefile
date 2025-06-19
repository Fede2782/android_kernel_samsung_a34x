#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-$(CONFIG_MTK_V4L2_LENS)   += vl53l4/
obj-$(CONFIG_MTK_V4L2_LENS)   += core/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lens/tof/core/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lens/tof/core/src
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lens/tof/core/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lens/tof/vl53l4/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_V4L2_LENS) += stmvl53l4.o

stmvl53l4-objs += /vl53l4/vl53l4.o
stmvl53l4-objs += /core/src/vl53lx_api.o /core/src/vl53lx_api_core.o
stmvl53l4-objs += /core/src/vl53lx_core.o /core/src/vl53lx_register_funcs.o
stmvl53l4-objs += /core/src/vl53lx_api_preset_modes.o
stmvl53l4-objs += /core/src/vl53lx_api_calibration.o
stmvl53l4-objs += /core/src/vl53lx_silicon_core.o
stmvl53l4-objs += /core/src/vl53lx_nvm.o
stmvl53l4-objs += /core/src/vl53lx_api_debug.o /core/src/vl53lx_core_support.o
stmvl53l4-objs += /core/src/vl53lx_wait.o
stmvl53l4-objs += /core/src/vl53lx_nvm_debug.o
stmvl53l4-objs += /core/src/vl53lx_platform.o
stmvl53l4-objs += /core/src/vl53lx_platform_init.o
stmvl53l4-objs += /core/src/vl53lx_platform_ipp.o
stmvl53l4-objs += /core/src/vl53lx_platform_log.o

stmvl53l4-objs += /core/src/vl53lx_hist_funcs.o
stmvl53l4-objs += /core/src/vl53lx_xtalk.o
stmvl53l4-objs += /core/src/vl53lx_hist_core.o
stmvl53l4-objs += /core/src/vl53lx_dmax.o
stmvl53l4-objs += /core/src/vl53lx_hist_algos_gen3.o
stmvl53l4-objs += /core/src/vl53lx_hist_algos_gen4.o
stmvl53l4-objs += /core/src/vl53lx_hist_char.o
stmvl53l4-objs += /core/src/vl53lx_sigma_estimate.o