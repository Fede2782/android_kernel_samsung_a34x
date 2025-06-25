#
# Copyright (C) 2021 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
#
# Makefile for misc devices that really don't fit anywhere else.
#
LOCAL_N3D_PATH := $(IMG_COMMON_CODE_PATH)/n3d_fsync

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += $(LOCAL_N3D_PATH)/n3d.o
imgsensor_isp6s-objs += $(LOCAL_N3D_PATH)/n3d_clk.o
imgsensor_isp6s-objs += $(LOCAL_N3D_PATH)/n3d_hw.o
imgsensor_isp6s-objs += $(LOCAL_N3D_PATH)/vsync_recorder.o
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG), m y))
imgsensor_isp6s_lag-objs += $(LOCAL_N3D_PATH)/n3d.o
imgsensor_isp6s_lag-objs += $(LOCAL_N3D_PATH)/n3d_clk.o
imgsensor_isp6s_lag-objs += $(LOCAL_N3D_PATH)/n3d_hw.o
imgsensor_isp6s_lag-objs += $(LOCAL_N3D_PATH)/vsync_recorder.o
endif




MKFILE_PATH := $(lastword $(MAKEFILE_LIST))
N3D_DRIVER_PATH := $(patsubst %/,%,$(dir $(realpath $(MKFILE_PATH))))
FRAME_SYNC := frame-sync

include $(N3D_DRIVER_PATH)/$(FRAME_SYNC)/frame_sync_drv.mk

