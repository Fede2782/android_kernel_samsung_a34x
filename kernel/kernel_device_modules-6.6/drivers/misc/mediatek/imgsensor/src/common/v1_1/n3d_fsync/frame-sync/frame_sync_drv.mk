# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

subdir-ccflags-y += \
	-I$(N3D_DRIVER_PATH) \
	-I$(N3D_DRIVER_PATH)/$(FRAME_SYNC) \

LOCAL_FSYNC_PATH := $(LOCAL_N3D_PATH)/$(FRAME_SYNC)

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += \
	$(LOCAL_FSYNC_PATH)/frame_sync.o \
	$(LOCAL_FSYNC_PATH)/frame_sync_algo.o \
	$(LOCAL_FSYNC_PATH)/frame_monitor.o \

endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG), m y))
imgsensor_isp6s_lag-objs += \
	$(LOCAL_FSYNC_PATH)/frame_sync.o \
	$(LOCAL_FSYNC_PATH)/frame_sync_algo.o \
	$(LOCAL_FSYNC_PATH)/frame_monitor.o \

endif