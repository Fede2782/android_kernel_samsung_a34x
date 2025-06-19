# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/sc800cs_lymipi_Sensor.o \

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MON), m y))
imgsensor_isp6s_mon-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif
