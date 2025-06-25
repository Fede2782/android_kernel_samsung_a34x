# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 Samsung Electronics Co., Ltd

SENSOR_NAME  := s5khm6

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/$(SENSOR_NAME)mipiraw_Sensor.o

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MON), m y))
imgsensor_isp6s_mon-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MOU), m y))
imgsensor_isp6s_mou-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG), m y))
imgsensor_isp6s_lag-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

$(info used driver: $(SENSOR_NAME))
