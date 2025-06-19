# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 Samsung Electronics Co., Ltd

SENSOR_NAME  := gc02m1b
COMMON_SENSOR := gc02m1

ifneq ($(findstring $(COMMON_SENSOR)_mipi_raw, $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR)_mipi_raw device driver is already in use)
IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/$(SENSOR_NAME)_dummy.o
else
$(info use $(COMMON_SENSOR) device driver for $(SENSOR_NAME))
IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/../$(COMMON_SENSOR)_mipi_raw/$(COMMON_SENSOR)mipiraw_Sensor.o
endif

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
