# Linux driver folder
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/GT1151/
obj-m += gt1151.o
gt1151-y	+=  gt1x_generic.o
gt1151-n	+=  gt1x_tools.o
gt1151-y	+=  gt1x_tpd.o
gt1151-y	+=  gt1x_update.o
gt1151-y	+=  tpd_control.o
gt1151-y	+=  tpd_button.o
gt1151-y	+=  tpd_debug.o
gt1151-y	+=  tpd_default.o
gt1151-y	+=  tpd_misc.o
gt1151-y	+=  tpd_setting.o

