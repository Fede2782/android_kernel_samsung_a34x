# Makefile for the focaltech touchscreen drivers.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/focaltech_touch/

obj-m += focaltech_touch.o
focaltech_touch-y	+=  focaltech_core.o
focaltech_touch-y	+=  focaltech_ex_fun.o
focaltech_touch-y	+=  focaltech_ex_mode.o
focaltech_touch-y	+=  focaltech_gesture.o
focaltech_touch-y	+=  focaltech_esdcheck.o
focaltech_touch-y	+=  focaltech_point_report_check.o
focaltech_touch-y	+=  focaltech_proximity.o
focaltech_touch-y	+=  focaltech_i2c.o
focaltech_touch-y	+=  focaltech_flash.o
focaltech_touch-y	+=  mtk_tpd.o
focaltech_touch-y	+=  tpd_button.o
focaltech_touch-y	+=  tpd_debug.o
focaltech_touch-y	+=  tpd_default.o
focaltech_touch-y	+=  tpd_misc.o
focaltech_touch-y	+=  tpd_setting.o
